#ifndef AISE_IOUTILS_H
#define AISE_IOUTILS_H

#include <vector>
#include "node.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_os_ostream.h"

namespace aise
{

class BBDAG
{
    std::vector<Node *> nodes;

  public:
    void AddNode(Node *node) { nodes.push_back(node); }
    const std::vector<Node *> &Nodes() { return nodes; }
    Node *NodeAt(int i) { return nodes[i]; }

    // Sort sorts the nodes in reversed topological order, i.e., output nodes
    // have smaller indexes.
    void Sort();
};

// ReadBitcode parses bitcode file as DAGs.
// Returns the number of parsed DAGs, -1 if there is any error.
int ParseBitcode(llvm::Twine path, std::vector<BBDAG *> &buffer);

template <typename Iterator>
inline void PrintRange(Iterator begin, Iterator end)
{
    for (; begin != end; ++begin) {
        llvm::outs() << *begin << ' ';
    }
}

} // namespace aise

#endif
