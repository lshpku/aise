#include "node.h"
#include "miso.h"
#include "ioutils.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_os_ostream.h"
#include <vector>
#include <set>
#include <map>
#include <queue>

using namespace aise;
using namespace llvm;

namespace
{

typedef std::priority_queue<Node *, std::vector<Node *>, Node::LessIndexCompare> node_heap;
typedef std::set<Node *, Node::LessIndexCompare> node_set;
typedef std::map<Node *, Node *, Node::LessIndexCompare> node_node_map;

void pushAllPred(Node *node, node_heap &buffer)
{
    Node::const_node_iterator i = node->PredBegin(), e = node->PredEnd();
    for (; i != e; ++i) {
        if ((*i)->Type != Node::UnkTy) {
            buffer.push(*i);
        }
    }
}

bool isConvexAndNotOutput(const Node *node, const node_set &selected)
{
    Node::const_node_iterator i = node->SuccBegin(), e = node->SuccEnd();

    // A constant is considered convex if one of its successors is
    // selected. Constants are never considered as output.
    if (node->Type == Node::ConstTy) {
        for (; i != e; ++i) {
            if (selected.find(*i) != selected.end()) {
                return true;
            }
        }
        return false;
    }

    for (; i != e; ++i) {
        if (selected.find(*i) == selected.end()) {
            return false;
        }
    }
    return true;
}

// Get upper cone of root in reversed topological order (root at buffer[0]).
void getUpperCone(Node *root, std::vector<Node *> &buffer)
{
    if (root->Type == Node::UnkTy) {
        return;
    }

    node_heap candidates;
    node_set selected;
    pushAllPred(root, candidates);
    buffer.push_back(root);
    selected.insert(root);

    while (!candidates.empty()) {
        Node *node = candidates.top();
        candidates.pop();

        // node not selected
        if (selected.find(node) != selected.end()) {
            continue;
        }
        // convex and has no outer successor
        if (!isConvexAndNotOutput(node, selected)) {
            continue;
        }

        // select the node
        pushAllPred(node, candidates);
        buffer.push_back(node);
        selected.insert(node);
    }
}

class MISOContext
{
  public:
    // root is at index 0
    std::vector<Node *> upperCone;
    std::vector<bool> choices;
    // selected and inputs don't overlap
    node_set selected, inputs;
    int maxIn;

    // inst in minimal PRN
    std::vector<std::string> uniqueInst;
    // inst in each permutation of inputs
    StringMap<size_t> permutedInst;
};

void yieldMISO(MISOContext &context)
{
    node_node_map nodeMap; // old ptr -> new ptr
    std::list<Node *> newNodes;
    std::vector<Node *> inputs; // for permutation
    node_set::iterator i, e;

    // make a copy of selected and input nodes
    // Only copy Pred, leave Succ and Index empty.
    for (i = context.inputs.begin(), e = context.inputs.end(); i != e; ++i) {
        // input nodes has no type nor predecessor
        Node *node = new Node();
        nodeMap[*i] = node;
        inputs.push_back(node);
    }
    for (i = context.selected.begin(), e = context.selected.end(); i != e; ++i) {
        Node *node = Node::FromTypeOfNode(*i);
        // the mapping should work since selected is in topological order
        Node::const_node_iterator predIter = (*i)->PredBegin(),
                                  predEnd = (*i)->PredEnd();
        for (; predIter != predEnd; ++predIter) {
            node->AddPred(nodeMap.find(*predIter)->second);
        }
        node->ToAssociative(newNodes);
        nodeMap[*i] = node;
    }

    // relax order
    // also build succ relation to find unavailable nodes
    {
        node_node_map::iterator i = nodeMap.begin(), e = nodeMap.end();
        for (; i != e; ++i) {
            i->second->RelaxOrder(newNodes);
            i->second->PropagateSucc();
        }
    }    
    {
        std::list<Node *>::iterator i = newNodes.begin(), e = newNodes.end();
        for (; i != e; ++i) {
            (*i)->PropagateSucc();
        }
    }

    // Copy available nodes in nodeMap into newNodes to avoid redundant
    // sorting, and delete those that are unavailable.
    Node *root = nodeMap[context.upperCone[0]];
    {
        // add nodes in topological order
        node_node_map::iterator i = nodeMap.begin(), e = nodeMap.end();
        for (; i != e; ++i) {
            if (i->second == root || i->second->Succ.size() > 0) {
                newNodes.push_back(i->second);
            } else {
                delete i->second;
            }
        }
    }

    // try each order of input
    // For instructions like a single constant, the input number is 0 and
    // there is no permutation, thus no instruction is generated.
    Permutation perm(inputs.size());
    std::string RPN, minRPN;
    while (perm.HasNext()) {
        const std::vector<size_t> &indexes = perm.Next();
        for (int i = indexes.size() - 1; i >= 0; i--) {
            inputs[i]->Type = (Node::NodeType)(indexes[i] + Node::FirstInputTy);
        }

        // call Sort() in topological order
        // In fact, only nodes that are not labels are in order, but it
        // doesn't matter since labels don't need sorting.
        {
            std::list<Node *>::iterator i, e;
            for (i = newNodes.begin(), e = newNodes.end(); i != e; ++i) {
                (*i)->Sort();
                (*i)->Index = 0;
            }
        }

        RPN.clear();
        root->WriteRefRPN(RPN);

        if (context.permutedInst.find(RPN) != context.permutedInst.end()) {
            break; // inst exists
        }
        if (minRPN.empty() || RPN < minRPN) {
            minRPN = RPN;
        }
        context.permutedInst[RPN] = context.uniqueInst.size() - 1;
    }
    if (!minRPN.empty()) {
        context.uniqueInst.push_back(minRPN);
    }

    // delete new nodes
    {
        std::list<Node *>::iterator i, e;
        for (i = newNodes.begin(), e = newNodes.end(); i != e; ++i) {
            delete *i;
        }
    }
}

void recurseMISO(MISOContext &context)
{
    std::vector<Node *> newInputs;
    bool isInput = false;
    bool choice = context.choices.back();
    Node *node = context.upperCone[context.choices.size() - 1];

    if (choice) {
        // check outputs
        // all nodes except root should have their outputs selected
        if (context.choices.size() > 1) {
            if (!isConvexAndNotOutput(node, context.selected)) {
                return;
            }
        }

        // update inputs
        {
            Node::const_node_iterator i = node->PredBegin(),
                                      e = node->PredEnd();
            for (; i != e; ++i) {
                if (context.inputs.find(*i) == context.inputs.end()) {
                    newInputs.push_back(*i);
                    context.inputs.insert(*i);
                }
            }
        }
        if (context.inputs.find(node) != context.inputs.end()) {
            isInput = true;
            context.inputs.erase(node);
        }

        context.selected.insert(node);

        // check inputs before yielding a result
        // Note: omit primary op.
        if (context.inputs.size() <= context.maxIn &&
            context.selected.size() > 1) {
            yieldMISO(context);
        }
    }

    // recurse
    if (context.choices.size() < context.upperCone.size()) {
        context.choices.push_back(true);
        recurseMISO(context);
        context.choices.pop_back();
        context.choices.push_back(false);
        recurseMISO(context);
        context.choices.pop_back();
    }

    // restore selected and inputs
    if (choice) {
        context.selected.erase(node);
        std::vector<Node *>::iterator i = newInputs.begin(),
                                      e = newInputs.end();
        for (; i != e; ++i) {
            context.inputs.erase(*i);
        }
        if (isInput) {
            context.inputs.insert(node);
        }
    }
}

} // namespace

namespace aise
{

Permutation::Permutation(size_t n)
{
    index.resize(n);
    for (size_t i = 0; i < n; i++) {
        index[i] = i;
    }
    status.reserve(n);
    if (n > 0) {
        status.push_back(0);
    }
}

const std::vector<size_t> &Permutation::Next()
{
    // push status until full
    while (status.size() < index.size()) {
        status.push_back(0);
    }

    // pop status until one is increased or empty
    while (!status.empty()) {
        size_t i = status.size() - 1;
        if (status.back() == index.size() - status.size()) {
            status.pop_back();
            size_t lastIndex = index[i];
            for (; i < index.size() - 1; i++) {
                index[i] = index[i + 1];
            }
            index[i] = lastIndex;
        } else {
            status[i]++;
            size_t tmp = index[i];
            index[i] = index[i + status[i]];
            index[i + status[i]] = tmp;
            break;
        }
    }

    return index;
}

void EnumerateMISO(const std::vector<Node *> &DAG, int maxIn)
{
    if (DAG.empty()) {
        return;
    }

    MISOContext context;
    context.maxIn = 2;

    // try each node in DAG as root of the MISO instruction
    {
        std::vector<Node *>::const_iterator i = DAG.begin(),
                                            e = DAG.end();
        for (; i != e; ++i) {
            context.choices.clear();
            context.selected.clear();
            context.upperCone.clear();
            context.inputs.clear();
            getUpperCone(*i, context.upperCone);

            if (!context.upperCone.empty()) {
                // always select root
                context.choices.push_back(true);
                recurseMISO(context);
            }
        }
    }
    {
        std::vector<std::string>::iterator i = context.uniqueInst.begin(),
                                           e = context.uniqueInst.end();
        for (; i != e; ++i) {
            outs() << "    " << *i << '\n';
        }
        outs() << "    ---- total " << context.permutedInst.size() << " ----\n";
    }
}

} // namespace aise
