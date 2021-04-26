#include "node.h"
#include "utils.h"
#include "miso.h"
#include "llvm/Support/CommandLine.h"

using namespace aise;
using namespace llvm;

namespace
{
cl::opt<std::string> bitcodePath(cl::Positional, cl::desc("<input file>"), cl::Required);
cl::opt<std::string> savePath("o", cl::desc("Specify output file (default stdout)"), cl::value_desc("filename"));
cl::opt<std::string> maxInput("max-input", cl::desc("Specify max input (default 2)"), cl::value_desc("int"), cl::init("2"));
} // namespace

int main(int argc, char **argv)
{
    std::list<NodeArray *> buffer0;
    ParseMISO("result.miso.txt", buffer0);
    return 0;
    cl::ParseCommandLineOptions(argc, argv, "AISE: Automatic Instruction Set Extension");

    std::list<NodeArray *> buffer;
    if (ParseBitcode(bitcodePath, buffer) < 0) {
        return -1;
    }

    int maxInputVal;
    if (ParseInt(maxInput, maxInputVal) < 0) {
        errs() << "Not an integer: " << maxInput << '\n';
        return -1;
    }
    if (maxInputVal <= 0) {
        errs() << "Invalid value for max input '" << maxInputVal
               << "': Should be positive\n";
        return -1;
    }

    MISOEnumerator misoEnum(maxInputVal);
    std::list<NodeArray *>::iterator i, e;
    for (i = buffer.begin(), e = buffer.end(); i != e; ++i) {
        misoEnum.Enumerate(*i);
    }

    if (savePath.empty()) {
        misoEnum.Save(outs());
    } else {
        OutFile *out = OutFile::Create(savePath.c_str());
        if (!out) {
            return -1;
        }
        misoEnum.Save(out->OS());
        out->Close();
    }

    return 0;
}
