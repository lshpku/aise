#include "node.h"
#include "utils.h"
#include "miso.h"
#include "llvm/Support/CommandLine.h"

using namespace aise;
using namespace llvm;

namespace
{
cl::opt<std::string> command(cl::Positional, cl::desc("<command>"), cl::Required);
cl::list<std::string> inputList(cl::Positional, cl::desc("[<input>...]"));
cl::opt<std::string> outputPath("o", cl::desc("Specify output file (default stdout)"), cl::value_desc("filename"));
cl::opt<std::string> maxInput("max-input", cl::desc("Specify max input (default 2)"), cl::value_desc("int"), cl::init("2"));
cl::opt<std::string> maxDepth("max-depth", cl::desc("Specify max depth (default 10)"), cl::value_desc("int"), cl::init("10"));
cl::opt<bool> interactive("interactive", cl::desc("Use interactive mode"));
cl::extrahelp commandHelp(
    "\nCOMMAND:\n"
    "  enum - Enumerate MISO instructions in LLVM assembly\n"
    "         input: <bitcode>\n"
    "  isel - Apply MISO instructions to LLVM assembly\n"
    "         inputs (one-off): <bitcode> <miso> [<bcconf>]\n"
    "         inputs (interactive): <bitcode> [<bcconf>]\n"
    "  area - Count area of MISO instructions\n"
    "         input: <miso>\n");

int parseNonNeg(const std::string &str, const char *name)
{
    int value;
    if (ParseInt(str, value) < 0) {
        errs() << "Not an integer: " << str << '\n';
        return -1;
    }
    if (value < 0) {
        errs() << "Invalid value '" << value << "' for '" << name
               << "': Should be non-negative\n";
        return -1;
    }
    return value;
}

int doEnum()
{
    if (inputList.size() != 1) {
        errs() << "enum: Requires exactly 1 input\n";
        return -1;
    }

    std::list<NodeArray *> buffer;
    if (ParseBitcode(inputList[0], buffer) < 0) {
        return -1;
    }

    int maxInputVal, maxDepthVal;
    if ((maxInputVal = parseNonNeg(maxInput, "-max-input")) < 0) {
        return -1;
    }
    if ((maxDepthVal = parseNonNeg(maxDepth, "-max-depth")) < 0) {
        return -1;
    }

    MISOEnumerator misoEnum(maxInputVal, maxDepthVal);
    std::list<NodeArray *>::iterator i, e;
    for (i = buffer.begin(), e = buffer.end(); i != e; ++i) {
        misoEnum.Enumerate(*i);
    }

    if (outputPath.empty()) {
        misoEnum.Save(outs());
    } else {
        OutFile out(outputPath.c_str());
        if (!out.IsOpen()) {
            return -1;
        }
        misoEnum.Save(out.OS());
    }
    return 0;
}

int doIsel()
{
    if (inputList.size() < 2 || inputList.size() > 3) {
        errs() << "isel (one-off): Requires 2 or 3 inputs\n";
        return -1;
    }

    std::list<NodeArray *> bcBuffer, misoBuffer;
    std::list<size_t> confBuffer;

    if (ParseBitcode(inputList[0], bcBuffer) < 0) {
        return -1;
    }
    if (ParseMISO(inputList[1], misoBuffer) < 0) {
        return -1;
    }
    if (inputList.size() == 3) {
        if (ParseConf(inputList[2], confBuffer) < 0) {
            return -1;
        }
        if (bcBuffer.size() != confBuffer.size()) {
            errs() << "Basic blocks and configurations don't match: "
                   << bcBuffer.size() << " and " << confBuffer.size() << '\n';
            return -1;
        }
    } else {
        confBuffer.resize(bcBuffer.size(), 1);
    }

    MISOSelector misoSel;
    std::list<NodeArray *>::iterator i, e;
    for (i = misoBuffer.begin(), e = misoBuffer.end(); i != e; ++i) {
        misoSel.AddInstr(*i);
    }

    size_t totalSTA = 0;
    std::list<size_t>::iterator c = confBuffer.begin();
    for (i = bcBuffer.begin(), e = bcBuffer.end(); i != e; ++i, ++c) {
        size_t STA = misoSel.Select(*i);
        totalSTA += STA * (*c);
    }

    outs() << "STA: " << totalSTA << '\n';

    return 0;
}

int doArea()
{
    if (inputList.size() != 1) {
        errs() << "area: Requires exactly 1 input\n";
        return -1;
    }

    std::list<NodeArray *> buffer;
    if (ParseMISO(inputList[0], buffer) < 0) {
        return -1;
    }

    MISOSynthesizer misoSyn;
    typedef std::list<NodeArray *>::iterator ln_iter;
    for (ln_iter i = buffer.begin(), e = buffer.end(); i != e; ++i) {
        misoSyn.AddInstr(*i);
    }
    outs() << "Area: " << misoSyn.GetArea() << '\n';
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv, "AISE: Automatic Instruction Set Extension");

    if (command == "enum") {
        return doEnum();
    } else if (command == "isel") {
        return doIsel();
    } else if (command == "area") {
        return doArea();
    } else {
        errs() << "main: Unknown command: " << command << '\n';
        return -1;
    }
}
