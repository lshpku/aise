#include "node.h"
#include "miso.h"
#include "utils.h"
#include "llvm/Support/raw_os_ostream.h"
#include <vector>
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
    if (node->TypeOf(Node::ConstTy)) {
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

} // namespace

namespace aise
{

void MISOEnumerator::getUpperCone(Node *root, NodeArray &buffer)
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

        // skip nodes that are selected
        if (selected.find(node) != selected.end()) {
            continue;
        }
        // node should be convex and has no outer successor
        if (!isConvexAndNotOutput(node, selected)) {
            continue;
        }

        // select the node
        pushAllPred(node, candidates);
        buffer.push_back(node);
        selected.insert(node);
    }
}

void MISOEnumerator::yield(Context &context)
{
    node_node_map nodeMap;             // old -> new
    std::map<Node *, Node *> inputMap; // new -> old
    std::list<Node *> newNodes;
    std::vector<Node *> inputs; // for permutation
    node_set::iterator i, e;

    // make a copy of selected and input nodes
    // Only copy Pred, leave Succ and Index empty.
    for (i = context.Inputs.begin(), e = context.Inputs.end(); i != e; ++i) {
        // input nodes has no type nor predecessor
        Node *node = new Node();
        nodeMap[*i] = node;
        inputMap[node] = *i;
        inputs.push_back(node);
    }
    for (i = context.Selected.begin(), e = context.Selected.end(); i != e; ++i) {
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
    Node *root = nodeMap[context.UpperCone[0]];
    {
        // add nodes in topological order
        node_node_map::iterator i = nodeMap.begin(), e = nodeMap.end();
        for (; i != e; ++i) {
            if (i->second == root || i->second->Succ.size() > 0) {
                newNodes.push_back(i->second);
            } else {
                Node::Delete(i->second);
            }
        }
    }

    // try each order of input
    // For instructions like a single constant, the input number is 0 and
    // there is no permutation, thus no instruction is generated.
    Permutation perm(inputs.size());
    std::string RPN, minRPN;
    std::vector<size_t> minIndexes;
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
        if (minRPN.empty() || RPN < minRPN) {
            minRPN = RPN;
            minIndexes = indexes;
        }
    }

    if (!minRPN.empty()) {
        // save instruction if it's new
        if (instrMap.find(minRPN) == instrMap.end()) {
            size_t instrIndex = instrMap.size();
            instrMap[minRPN] = instrIndex;
        }

        // add instruction to node as a tile
        IntriNode *instr = new IntriNode();
        instr->RefRPN = minRPN;
        std::vector<Node *> orderedInputs(inputs.size());
        for (int i = minIndexes.size() - 1; i >= 0; i--) {
            orderedInputs[minIndexes[i]] = inputMap[inputs[i]];
        }
        instr->Pred.insert(instr->Pred.end(),
                           orderedInputs.begin(), orderedInputs.end());
        context.UpperCone[0]->AddTile(instr);
    }

    // delete new nodes
    {
        std::list<Node *>::iterator i, e;
        for (i = newNodes.begin(), e = newNodes.end(); i != e; ++i) {
            Node::Delete(*i);
        }
    }
}

void MISOEnumerator::recurse(Context &context)
{
    std::vector<Node *> newInputs;
    bool isInput = false;
    // there must be at least one choice
    bool choice = context.Choices.back();
    Node *node = context.UpperCone[context.Choices.size() - 1];

    if (choice) {
        // check outputs
        // all nodes except root should have their outputs selected
        if (context.Choices.size() > 1) {
            if (!isConvexAndNotOutput(node, context.Selected)) {
                return;
            }
        }

        // update inputs
        {
            Node::const_node_iterator i = node->PredBegin(),
                                      e = node->PredEnd();
            for (; i != e; ++i) {
                if (context.Inputs.find(*i) == context.Inputs.end()) {
                    newInputs.push_back(*i);
                    context.Inputs.insert(*i);
                }
            }
        }
        if (context.Inputs.find(node) != context.Inputs.end()) {
            isInput = true;
            context.Inputs.erase(node);
        }

        context.Selected.insert(node);

        // check inputs before yielding a result
        // Note: omit primary op.
        if (context.Inputs.size() <= maxInput &&
            context.Selected.size() > 1) {
            yield(context);
        }
    }

    // recurse
    if (context.Choices.size() < context.UpperCone.size()) {
        context.Choices.push_back(true);
        recurse(context);
        context.Choices.pop_back();
        context.Choices.push_back(false);
        recurse(context);
        context.Choices.pop_back();
    }

    // restore selected and inputs
    if (choice) {
        context.Selected.erase(node);
        std::vector<Node *>::iterator i = newInputs.begin(),
                                      e = newInputs.end();
        for (; i != e; ++i) {
            context.Inputs.erase(*i);
        }
        if (isInput) {
            context.Inputs.insert(node);
        }
    }
}

void MISOEnumerator::Enumerate(const NodeArray *DAG)
{
    if (DAG->empty()) {
        return;
    }

    // try each node in DAG as root of the MISO instruction
    NodeArray::const_iterator i = DAG->begin(), e = DAG->end();
    for (; i != e; ++i) {
        Context context;
        getUpperCone(*i, context.UpperCone);

        if (!context.UpperCone.empty()) {
            // always select root
            context.Choices.push_back(true);
            recurse(context);
        }
    }
}

void MISOEnumerator::Save(raw_ostream &out)
{
    typedef llvm::StringMap<size_t>::iterator smi;
    std::vector<std::string> instrArray(instrMap.size());
    for (smi i = instrMap.begin(), e = instrMap.end(); i != e; ++i) {
        instrArray[i->second] = i->first();
    }
    typedef std::vector<std::string>::iterator vsi;
    for (vsi i = instrArray.begin(), e = instrArray.end(); i != e; ++i) {
        out << *i << '\n';
    }
}

void LegalizeDAG(NodeArray *DAG)
{
    NodeArray legalDAG;
    NodeArray::iterator i = DAG->begin(), e = DAG->end();

    for (; i != e; ++i) {
        size_t size = (*i)->Pred.size();
        if (!(*i)->IsAssociative() && size > 1) {
            std::list<Node *>::iterator p = (*i)->Pred.begin();
            size_t index = 1;
            for (++p; index < size && index < 3; index++) {
                Node *label = Node::FromType(Node::Order1Ty + index - 1);
                label->AddPred(*p);
                *p = label;
                legalDAG.push_back(label);
            }
        }
        legalDAG.push_back(*i);
    }

    for (size_t index = 0, size = legalDAG.size(); index < size; index++) {
        legalDAG[index]->Index = index;
        legalDAG[index]->PropagateSucc();
    }

    DAG->swap(legalDAG);
}

} // namespace aise
