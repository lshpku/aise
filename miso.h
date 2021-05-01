#ifndef AISE_MISO_H
#define AISE_MISO_H

#include "node.h"
#include "llvm/ADT/StringMap.h"
#include <vector>
#include <set>

namespace aise
{

class MISOEnumerator
{
    int maxInput;
    // inst in minimal PRN
    llvm::StringMap<size_t> instrMap;

    // Get upper cone of root into buffer.
    // Nodes are added in reversed topological order.
    void getUpperCone(Node *root, NodeArray &buffer);

    class Context
    {
      public:
        // in reversed topological order
        NodeArray UpperCone;
        std::vector<bool> Choices;
        // don't overlap
        std::set<Node *, Node::LessIndexCompare> Selected, Inputs;
    };

    // recurse recurses on the current upper cone.
    void recurse(Context &context);

    // yield yields the currently selected MISO instruction.
    void yield(Context &context);

  public:
    MISOEnumerator(size_t _maxInput) : maxInput(_maxInput) {}

    void Enumerate(const NodeArray *DAG);

    void Save(llvm::raw_ostream &out);
};

// LegalizeDAG inserts ordering labels before non-associative ops, then
// assignes indexes and builds successing relationship.
// Nodes in DAG keep topological order after processing.
void LegalizeDAG(NodeArray *DAG);

class MISOSelector
{
    // each instruction is represented by an IntriNode
    llvm::StringMap<IntriNode *> instrMap;
    std::vector<IntriNode *> instrList;
    size_t maxInput;

    class context
    {
      public:
        typedef std::set<size_t> IndexSet;

        NodeArray DAG;

        // in the same order of nodes in DAG
        std::vector<IntriNode *> BestTile;
        std::vector<size_t> MinCost;
        std::vector<bool> Fixed;
        std::vector<bool> Matched;
        std::vector<IndexSet> CoveredBy;
    };

    // buttomUp traverses in topological order to decide the locally best
    // tile for each node.
    void buttomUp(context &ctx);

    // sumCost returns the cost sum of the tile itself and its operands.
    size_t sumCost(const IntriNode *tile, context &ctx);

    // topDown traverses in reversed topological order to get a tiling of
    // the DAG.
    void topDown(context &ctx);

  public:
    MISOSelector() : maxInput(0) {}

    // Note: DAG should be legalized.
    void AddInstr(const NodeArray *DAG);

    // Select maps DAG into configured instructions using a near-optimal,
    // linear-time algorithm.
    // Returns the static execution time of mapped DAG.
    size_t Select(NodeArray *DAG);
};

} // namespace aise

#endif
