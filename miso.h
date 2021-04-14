#ifndef AISE_MISO_H
#define AISE_MISO_H

#include "ioutils.h"
#include <vector>

namespace aise
{

class Permutation
{
    std::vector<size_t> index;
    std::vector<size_t> status;

  public:
    Permutation(size_t n);

    bool HasNext() const { return !status.empty(); }
    const std::vector<size_t> &Next();
};

void EnumerateMISO(const std::vector<Node *> &nodes, int maxIn);

}

#endif
