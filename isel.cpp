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
            (*i)->Index = (*i)->AccCost();
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
    // case nodes into select nodes
    std::vector<selectNode *> selectDAG;
    selectDAG.reserve(DAG->size());
    {
        NodeArray::iterator i = DAG->begin(), e = DAG->end();
        for (; i != e; ++i) {
            selectNode *snode = new selectNode();
            snode->NodeImpl = *i;
            snode->BestInst = NULL;
            selectDAG.push_back(snode);
        }
    }

    // find all possible tiles for each node in the DAG
    MISOEnumerator misoEnum(maxInput);
    misoEnum.Enumerate(DAG);

    return 0;
}

} // namespace aise
