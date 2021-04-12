#include "node.h"
#include "miso.h"
#include "llvm/Support/raw_os_ostream.h"
#include <vector>

using namespace aise;
using namespace llvm;

namespace
{

Node *getUpperCone(const Node *root)
{
    if (root->Type() == Node::UnkTy) {
        return NULL;
    }
}

} // namespace

namespace aise
{

void EnumerateMISO(const std::vector<Node *> &nodes, int maxIn)
{
    if (nodes.empty()) {
        return;
    }
    std::vector<size_t> ucIndex;

    for (int i = 0, e = nodes.size(); i < e; i++) {
        ucIndex.clear();
        Node *ucRoot = getUpperCone(nodes[i]);
        if (ucRoot) {
            delete ucRoot;
        }
    }
}

} // namespace aise
