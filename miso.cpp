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

typedef std::priority_queue<Node *, std::vector<Node *>, Node::IndexLessCompare> node_heap;
typedef std::set<Node *, Node::IndexLessCompare> node_set;

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

    std::vector<std::string> uniqueInst;
    StringMap<size_t> permutedInst;
};

void yieldMISO(MISOContext &context)
{
    std::map<Node *, Node *> nodeMap; // old ptr -> new ptr
    std::vector<Node *> inputs;       // for permutation
    node_set::iterator i, e;

    // make a copy of selected and input nodes
    // Copy Pred, but leave Succ and Index empty.
    for (i = context.inputs.begin(), e = context.inputs.end(); i != e; ++i) {
        Node *node = new Node();
        nodeMap[*i] = node;
        inputs.push_back(node);
        // input nodes has no predecessor
    }
    for (i = context.selected.begin(), e = context.selected.end(); i != e; ++i) {
        Node *node = Node::FromTypeOfNode(*i);
        nodeMap[*i] = node;
        // the mapping should work since selected is in topological order
        Node::const_node_iterator predIter = (*i)->PredBegin(),
                                  predEnd = (*i)->PredEnd();
        for (; predIter != predEnd; ++predIter) {
            if (nodeMap.find(*predIter) == nodeMap.end()) {
                outs() << "not found\n";
            }
            node->AddPred(nodeMap.find(*predIter)->second);
        }
    }

    // relax order
    Node *root = nodeMap[context.upperCone[0]];
    outs() << "[";
    for (std::map<Node *, Node *>::iterator i = nodeMap.begin(), e = nodeMap.end(); i != e; ++i) {
        outs() << ' ' << *i->second << ';';
    }
    outs() << "]\n";
    std::vector<Node *> labelNodes;
    root->RelaxOrder(labelNodes);

    // try each order of input
    // For instructions like a single constant, the input number is 0 and
    // there is no permutation. It will generate no instruction.
    Permutation perm(inputs.size());
    std::string RPN;

    for (bool isFirst = true; perm.HasNext(); isFirst = false) {
        const std::vector<size_t> &indexes = perm.Next();
        for (int i = indexes.size() - 1; i >= 0; i--) {
            inputs[i]->Type = (Node::NodeType)(indexes[i] + Node::FirstInputTy);
        }
        root->Sort();
        RPN.clear();
        root->WriteRPN(RPN);

        if (context.permutedInst.find(RPN) != context.permutedInst.end()) {
            break; // inst exists
        }
        if (isFirst) {
            context.uniqueInst.push_back(RPN);
        }
        context.permutedInst[RPN] = context.uniqueInst.size() - 1;
    }

    /*/ delete new nodes
    {
        std::map<Node *, Node *>::iterator i = nodeMap.begin(),
                                           e = nodeMap.end();
        for (; i != e; ++i) {
            delete i->second;
        }
    }
    {
        std::vector<Node *>::iterator i = labelNodes.begin(),
                                      e = labelNodes.end();
        for (; i != e; ++i) {
            delete *i;
        }
    }*/
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

void EnumerateMISO(const std::vector<Node *> &nodes, int maxIn)
{
    if (nodes.empty()) {
        return;
    }

    MISOContext context;
    context.maxIn = 2;

    {
        std::vector<Node *>::const_iterator i = nodes.begin(),
                                            e = nodes.end();
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
