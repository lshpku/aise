#include "ioutils.h"
#include "miso.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/system_error.h"
#include <queue>

using namespace aise;
using namespace llvm;

namespace
{

typedef DenseMap<Value const *, Node *> value_node_map;

void parseBasicBlock(const BasicBlock &bb, std::vector<BBDAG *> &buffer)
{
    outs() << "  " << bb.getName() << ":\n";

    BBDAG *dag = new BBDAG();
    value_node_map opToNode;

    BasicBlock::const_iterator instIter = bb.begin(), instEnd = bb.end();
    for (; instIter != instEnd; ++instIter) {
        const Instruction &inst = *instIter;
        Node *node = Node::FromInstruction(inst);
        opToNode.insert(std::make_pair(&inst, node));

        User::const_op_iterator opIter = inst.op_begin(), opEnd = inst.op_end();
        for (; opIter != opEnd; ++opIter) {
            value_node_map::iterator opNode = opToNode.find(*opIter);

            // When the operand is defined in another basic block, or is defined
            // later in the same block but used by a phi instruction, it would
            // not be found. In this case, replace it with a unk.
            if (opNode == opToNode.end()) {
                Node *virtIn = new Node(Node::UnknownTy);
                dag->AddNode(virtIn);
                node->AddPrev(virtIn);
                virtIn->AddSucc(node);
            } else {
                node->AddPrev(opNode->second);
                opNode->second->AddSucc(node);
            }
        }

        Node *virtSucc = NULL;
        if (inst.isUsedOutsideOfBlock(&bb)) {
            virtSucc = new Node();
        } else {
            Value::const_use_iterator useIter = inst.use_begin(),
                                      useEnd = inst.use_end();
            for (; useIter != useEnd; ++useIter) {
                // used by previous phi in the same block
                if (opToNode.find(*useIter) != opToNode.end()) {
                    virtSucc = new Node();
                    break;
                }
            }
        }

        dag->AddNode(node);
        if (virtSucc) {
            node->AddSucc(virtSucc);
            virtSucc->AddPrev(node);
            dag->AddNode(virtSucc);
        }
    }
    dag->Sort();
    EnumerateMISO(dag->Nodes(), 2);
    buffer.push_back(dag);
}

} // namespace

namespace aise
{

void BBDAG::Sort()
{
    std::reverse(nodes.begin(), nodes.end());
}

int ParseBitcode(Twine path, std::vector<BBDAG *> &buffer)
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

    Module::const_iterator funcIter = mod->getFunctionList().begin(),
                           funcEnd = mod->getFunctionList().end();
    for (; funcIter != funcEnd; ++funcIter) {
        if (funcIter->isDeclaration()) {
            continue;
        }
        outs() << "func " << funcIter->getName() << " {\n";
        Function::const_iterator bbIter = funcIter->getBasicBlockList().begin(),
                                 bbEnd = funcIter->getBasicBlockList().end();
        for (; bbIter != bbEnd; ++bbIter) {
            parseBasicBlock(*bbIter, buffer);
        }
        outs() << "}\n";
    }
    return 0;
}

} // namespace aise
