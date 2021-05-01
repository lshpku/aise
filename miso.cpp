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
        IntriNode *tile = new IntriNode();
        tile->RefRPN = minRPN;
        std::vector<Node *> orderedInputs(inputs.size());
        for (int i = minIndexes.size() - 1; i >= 0; i--) {
            orderedInputs[minIndexes[i]] = inputMap[inputs[i]];
        }
        tile->Pred.insert(tile->Pred.end(),
                          orderedInputs.begin(), orderedInputs.end());
        context.UpperCone[0]->AddTile(tile);
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

void MISOSelector::AddInstr(const NodeArray *DAG)
{
    NodeArray instrDAG;
    instrDAG.reserve(DAG->size());

    // copy nodes into a new DAG
    {
        NodeArray::const_iterator i, e;
        for (i = DAG->begin(), e = DAG->end(); i != e; ++i) {
            Node *node = Node::FromTypeOfNode(*i);
            node->Index = 0;
            {
                Node::const_node_iterator p = (*i)->PredBegin(), e;
                for (e = (*i)->PredEnd(); p != e; ++p) {
                    node->AddPred(instrDAG[(*p)->Index]);
                }
            }
            instrDAG.push_back(node);
        }
    }

    std::string RPN;
    instrDAG.back()->WriteRefRPN(RPN);

    // calculate cost
    // use Index to keep the cost value
    {
        NodeArray::iterator i = instrDAG.begin(), e = instrDAG.end();
        size_t inputCount = 0;
        for (; i != e; ++i) {
            (*i)->Index = (*i)->CriticalPathCost();
            if ((*i)->IsInput()) {
                inputCount++;
            }
        }
        maxInput = std::max(maxInput, inputCount);
    }
    size_t rootCost = instrDAG.back()->Index;

    // save instruction
    IntriNode *intriNode = new IntriNode();
    intriNode->Pred.resize(1, NULL);
    intriNode->RefRPN = RPN;
    intriNode->Cost = Node::RoundUpUnitCost(rootCost);
    instrMap[intriNode->RefRPN] = intriNode;

    // delete copied nodes
    {
        NodeArray::iterator i = instrDAG.begin(), e = instrDAG.end();
        for (; i != e; ++i) {
            Node::Delete(*i);
        }
    }
}

size_t MISOSelector::Select(NodeArray *DAG)
{
    // find all possible tiles for each node in the DAG
    MISOEnumerator misoEnum(maxInput);
    misoEnum.Enumerate(DAG);

    for (size_t i = 0, e = DAG->size(); i != e; ++i) {
        // assign index
        Node *node = DAG->at(i);
        node->Index = i;

        // filter tiles found in enum stage and assigned costs to them
        {
            std::list<IntriNode *>::iterator i = node->TileList.begin();
            typedef StringMap<IntriNode *>::iterator in_iter;
            while (i != node->TileList.end()) {
                in_iter inIter = instrMap.find((*i)->RefRPN);
                if (inIter == instrMap.end()) {
                    i = node->TileList.erase(i);
                } else {
                    (*i)->Cost = inIter->second->Cost;
                    ++i;
                }
            }
        }
        // add default tile
        node->AddTile(IntriNode::TileOfNode(node));
    }

    context ctx;
    ctx.DAG.swap(*DAG);

    buttomUp(ctx);
    topDown(ctx);

    while (0) {
        NodeArray::iterator i = ctx.DAG.begin(), e = ctx.DAG.end();
        for (; i != e; ++i) {
            outs() << *(*i) << '\n';
            for (std::list<IntriNode *>::iterator k = (*i)->TileList.begin(),
                                                  q = (*i)->TileList.end();
                 k != q; ++k) {
                outs() << "    " << *(*k) << '\t' << (*k)->Cost << '\n';
            }
            size_t index = (*i)->Index;
            if (ctx.Matched[index]) {
                outs() << "    " << *ctx.BestTile[index] << '\t'
                       << ctx.MinCost[index] << " *\n";
            }
        }
    }

    // assign tiling to DAG
    size_t cost = 0;
    for (size_t i = 0, e = ctx.DAG.size(); i < e; i++) {
        ctx.DAG[i]->TileList.clear();
        if (ctx.Matched[i]) {
            ctx.DAG[i]->TileList.push_back(ctx.BestTile[i]);
            cost += ctx.BestTile[i]->Cost;
        }
    }

    return cost;
}

void MISOSelector::buttomUp(context &ctx)
{
    size_t size = ctx.DAG.size();
    ctx.MinCost.clear();
    ctx.MinCost.resize(size, -1);
    ctx.BestTile.clear();
    ctx.BestTile.resize(size, NULL);

    for (size_t i = 0; i < size; i++) {
        Node *node = ctx.DAG[i];
        std::list<IntriNode *>::iterator ti = node->TileList.begin(),
                                         te = node->TileList.end();
        for (; ti != te; ++ti) {
            size_t cost = sumCost(*ti, ctx);
            if (cost < ctx.MinCost[i]) {
                ctx.MinCost[i] = cost;
                ctx.BestTile[i] = *ti;
            }
        }
    }
}

size_t MISOSelector::sumCost(const IntriNode *tile, context &ctx)
{
    size_t cost = tile->Cost;
    Node::const_node_iterator i = tile->PredBegin(), e = tile->PredEnd();
    for (; i != e; ++i) {
        cost += ctx.MinCost[(*i)->Index];
    }
    return cost;
}

void MISOSelector::topDown(context &ctx)
{
    size_t size = ctx.DAG.size();
    ctx.Matched.clear();
    ctx.Matched.resize(size, false);

    std::queue<size_t> queue;
    for (size_t i = 0; i < size; i++) {
        if (ctx.DAG[i]->Succ.empty()) {
            queue.push(i);
        }
    }

    while (!queue.empty()) {
        size_t index = queue.front();
        queue.pop();
        if (ctx.Matched[index]) {
            continue;
        }
        ctx.Matched[index] = true;

        IntriNode *tile = ctx.BestTile[index];
        Node::const_node_iterator i = tile->PredBegin(), e;
        for (e = tile->PredEnd(); i != e; ++i) {
            queue.push((*i)->Index);
        }
    }
}

} // namespace aise
