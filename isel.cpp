#include "miso.h"
#include <queue>

using namespace aise;
using namespace llvm;

namespace aise
{

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
    ctx.Fixed.resize(ctx.DAG.size(), false);

    buttomUp(ctx);
    topDown(ctx);

    {
        NodeArray::iterator i = ctx.DAG.begin(), e = ctx.DAG.end();
        for (; i != e; ++i) {
            outs() << *(*i) << '\n';
            for (std::list<IntriNode *>::iterator k = (*i)->TileList.begin(),
                                                  q = (*i)->TileList.end();
                 k != q; ++k) {
                outs() << "    " << *(*k) << '\n';
            }
            size_t index = (*i)->Index;
            outs() << "    Best: " << *ctx.BestTile[index]
                   << ", Cost: " << ctx.MinCost[index] << '\n';
        }
    }

    return 0;
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
    ctx.CoveredBy.clear();
    ctx.CoveredBy.resize(size);

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
        // build the covered-by relationship
        {
            std::list<Node *>::iterator i = tile->Covering.begin(),
                                        e = tile->Covering.end();
            for (; i != e; ++i) {
                ctx.CoveredBy[(*i)->Index].insert(index);
            }
        }
        // search ahead
        {
            Node::const_node_iterator i = tile->PredBegin(),
                                      e = tile->PredEnd();
            for (; i != e; ++i) {
                queue.push((*i)->Index);
            }
        }
    }
}

} // namespace aise
