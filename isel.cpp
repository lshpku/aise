#include "miso.h"

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
    context ctx;

    // find all possible tiles for each node in the DAG
    MISOEnumerator misoEnum(maxInput);
    misoEnum.Enumerate(DAG);

    for (size_t i = 0, e = DAG->size(); i != e; ++i) {
        // wrap nodes in selectNode
        selectNode *snode = new selectNode();
        snode->NodeImpl = DAG->at(i);
        ctx.DAG.push_back(snode);

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

    buttomUp(ctx);

    {
        std::vector<selectNode *>::iterator i = ctx.DAG.begin(),
                                            e = ctx.DAG.end();
        for (; i != e; ++i) {
            Node *node = (*i)->NodeImpl;
            outs() << *node << '\n';
            for (std::list<IntriNode *>::iterator k = node->TileList.begin(),
                                                  q = node->TileList.end();
                 k != q; ++k) {
                outs() << "    " << *(*k) << '\n';
            }
            outs() << "    Best: " << *(*i)->BestTile
                   << ", Cost: " << (*i)->MinCost << '\n';
        }
    }

    return 0;
}

void MISOSelector::buttomUp(context &ctx)
{
    typedef std::vector<selectNode *>::iterator sn_iter;
    typedef std::list<IntriNode *>::iterator in_iter;

    sn_iter i = ctx.DAG.begin(), e = ctx.DAG.end();
    for (; i != e; ++i) {
        // Node: each node must have a default tile
        in_iter tileIter = (*i)->NodeImpl->TileList.begin(),
                tileEnd = (*i)->NodeImpl->TileList.end();
        for (; tileIter != tileEnd; ++tileIter) {
            size_t cost = sumCost(*tileIter, ctx);
            if (cost < (*i)->MinCost) {
                (*i)->MinCost = cost;
                (*i)->BestTile = *tileIter;
            }
        }
    }
}

size_t MISOSelector::sumCost(const IntriNode *tile, context &ctx)
{
    size_t cost = tile->Cost;
    Node::const_node_iterator i = tile->PredBegin(), e = tile->PredEnd();
    for (; i != e; ++i) {
        cost += ctx.DAG[(*i)->Index]->MinCost;
    }
    return cost;
}

} // namespace aise
