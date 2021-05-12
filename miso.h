#ifndef AISE_MISO_H
#define AISE_MISO_H

#include "node.h"
#include "llvm/ADT/StringMap.h"
#include <vector>
#include <map>
#include <set>
#include <queue>

namespace aise
{

class MISOEnumerator
{
    int maxInput, maxDepth;
    // inst in minimal PRN
    llvm::StringMap<size_t> instrMap;

    typedef std::set<Node *, Node::LessIndexCompare> node_set;

    class Context
    {
        typedef std::priority_queue<Node *, NodeArray, Node::LessIndexCompare> node_heap;

        std::map<Node *, size_t> nodeDepth;

        void pushAllPred(Node *root, node_heap &queue);

      public:
        // UpperCone is the MaxMISO rooted at root.
        // Nodes in UpperCone are in reversed topological order.
        NodeArray UpperCone;
        node_set UpperConeSet;

        // parallel to UpperCone
        std::vector<bool> Choice;

        node_set Selected;
        node_set Input;
        // Number of inputs in Inputs that don't belong to UpperCone.
        size_t MandatoryInputs;

        Context() : MandatoryInputs(0) {}

        // Init initializes context for root and its upper cone.
        // Do call this method once for each instance of Context.
        void Init(Node *root, size_t maxDepth);

        // IsOutput checks if node is used by nodes outside Selected.
        bool IsOutput(Node *node);
    };

    // recurse recurses on the current upper cone.
    void recurse(Context &ctx);

    // yield yields the currently selected MISO instruction.
    void yield(Context &ctx);

  public:
    MISOEnumerator(size_t _maxInput, size_t _maxDepth);

    // Enumerate enumerates all MISO instructions in DAG.
    void Enumerate(NodeArray *DAG);

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
    size_t maxInput, maxDepth;

    class context
    {
      public:
        typedef std::set<size_t> IndexSet;

        NodeArray DAG;

        // in the same order of nodes in DAG
        std::vector<IntriNode *> BestTile;
        std::vector<size_t> MinCost;
        std::vector<bool> Matched;
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

    // Select maps DAG into configured instructions using dynamic
    // programming. DAG should have been enumerated.
    // Nodes in DAG will be assigned the correspoding tiles in their
    // TileList. Skipped nodes have an empty TileList.
    // Returns the static execution time of mapped DAG.
    size_t Select(NodeArray *DAG);

    size_t GetMaxInput() { return maxInput; }
};

class MISOSynthesizer
{
    size_t area;

  public:
    MISOSynthesizer() : area(0) {}

    void AddInstr(const NodeArray *DAG);

    size_t GetArea() { return area; }
};

} // namespace aise

#endif
