#include "node.h"
#include "ioutils.h"
#include "llvm/Support/CommandLine.h"

using namespace aise;
using namespace llvm;

namespace
{
cl::opt<std::string> bitcodePath(cl::Positional, cl::desc("path to bitcode file"), cl::Required);
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv, "AISE: Automatic Instruction Set Extension");

    std::vector<BBDAG *> buffer;
    if (ParseBitcode(bitcodePath, buffer) < 0) {
        return -1;
    }
}
