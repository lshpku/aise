#include "ioutils.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/system_error.h"
#include <queue>
#include <sstream>

using namespace aise;
using namespace llvm;

namespace
{

typedef DenseMap<Value const *, Node *> value_node_map;

NodeArray *parseBasicBlock(const BasicBlock &bb)
{
    NodeArray *DAGPtr = new NodeArray(), &DAG = *DAGPtr;
    value_node_map nodeMap;

    BasicBlock::const_iterator instIter = bb.begin(), instEnd = bb.end();
    for (; instIter != instEnd; ++instIter) {
        const Instruction &inst = *instIter;
        Node *node = Node::FromInstruction(&inst);

        // Add operands to node
        User::const_op_iterator opIter = inst.op_begin(), opEnd = inst.op_end();
        for (; opIter != opEnd; ++opIter) {
            value_node_map::iterator opNode = nodeMap.find(*opIter);

            // When the operand is defined in another basic block, or is defined
            // later in the same block but used by a phi instruction, or is
            // constant, it would not be found.
            if (opNode == nodeMap.end()) {
                Node *virtIn = Node::FromValue(*opIter);
                virtIn->Index = DAG.size();
                node->AddPred(virtIn);
                DAG.push_back(virtIn);
                nodeMap[*opIter] = virtIn;
            } else {
                node->AddPred(opNode->second);
            }
        }

        // Look for external uses of node
        Node *virtSucc = NULL;
        if (inst.isUsedOutsideOfBlock(&bb)) {
            virtSucc = new Node();
        } else {
            Value::const_use_iterator useIter = inst.use_begin(),
                                      useEnd = inst.use_end();
            for (; useIter != useEnd; ++useIter) {
                // used by previous phi in the same block
                if (nodeMap.find(*useIter) != nodeMap.end()) {
                    virtSucc = new Node();
                    break;
                }
            }
        }

        // Add node to dag
        node->Index = DAG.size();
        nodeMap[&inst] = node;
        DAG.push_back(node);
        if (virtSucc) {
            virtSucc->Index = DAG.size();
            virtSucc->AddPred(node);
            DAG.push_back(virtSucc);
        }
    }

    // Build successor dependency
    {
        std::vector<Node *>::iterator i = DAG.begin(), e = DAG.end();
        for (; i != e; ++i) {
            (*i)->PropagateSucc();
        }
    }

    return DAGPtr;
}

} // namespace

namespace aise
{

int ParseBitcode(Twine path, std::list<NodeArray *> &buffer)
{
    OwningPtr<MemoryBuffer> bitcodeBuffer;
    error_code getFileErr = MemoryBuffer::getFile(path, bitcodeBuffer);
    if (getFileErr != error_code::success()) {
        errs() << path << ": " << getFileErr.message() << '\n';
        return -1;
    }

    std::string parseBitcodeErr;
    Module *mod = ParseBitcodeFile(bitcodeBuffer.get(), getGlobalContext(), &parseBitcodeErr);
    if (!mod) {
        errs() << path << ": " << parseBitcodeErr << '\n';
        return -1;
    }

    int bbCount = 0;
    Module::const_iterator funcIter = mod->getFunctionList().begin(),
                           funcEnd = mod->getFunctionList().end();
    for (; funcIter != funcEnd; ++funcIter) {
        if (funcIter->isDeclaration()) {
            continue;
        }
        Function::const_iterator bbIter = funcIter->getBasicBlockList().begin(),
                                 bbEnd = funcIter->getBasicBlockList().end();
        for (; bbIter != bbEnd; ++bbIter, ++bbCount) {
            buffer.push_back(parseBasicBlock(*bbIter));
        }
    }
    return bbCount;
}

#define NOT_AN_INTEGER errs() << "Not an integer: " << str << '\n'

int ParseInt(std::string &str, int &buffer)
{
    if (str.empty()) {
        NOT_AN_INTEGER;
        return -1;
    }

    size_t i = 0, e = str.size();
    bool positive;
    if (str[i] == '-') {
        if (++i == e) {
            NOT_AN_INTEGER;
            return -1;
        }
        positive = false;
    } else {
        positive = true;
    }

    int sum = 0;
    for (; i != e; i++) {
        int val = str[i] - '0';
        if (val < 0 || val > 9) {
            NOT_AN_INTEGER;
            return -1;
        }
        sum = sum * 10;
        sum += val;
    }

    buffer = positive ? sum : -sum;
    return 0;
}

#undef NOT_AN_INTEGER

OutFile *OutFile::Create(const char *path)
{
    std::string err;
    tool_output_file *out = new tool_output_file(path, err);
    if (!err.empty()) {
        errs() << err << '\n';
        return NULL;
    }
    return new OutFile(out);
}

void OutFile::Close()
{
    out->keep();
    delete out;
    out = NULL;
}

} // namespace aise
