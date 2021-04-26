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
    std::vector<std::string> uniqueInsts;
    // inst in each permutation of inputs
    llvm::StringMap<size_t> permutedInsts;

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

class MISOInstr
{
    // nodes are in topological order, i.e., root at the back
    NodeArray nodes;
    // inputs are in the order that when assigned from 1 to N, the RPN
    // is exactly minimal
    NodeArray inputs;
    std::string rpn;

  public:
    const std::string &GetRPN() const { return rpn; }
};

} // namespace aise

#endif
