#include "ioutils.h"
#include "miso.h"
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
#include <vector>
#include <queue>

using namespace aise;
using namespace llvm;

namespace
{

typedef DenseMap<Value const *, Node *> value_node_map;

void parseBasicBlock(const BasicBlock &bb, std::vector<BBDAG *> &buffer)
{
    outs() << "  " << bb.getName() << ":\n";

    std::vector<Node *> nodes;
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
                virtIn->Index = nodes.size();
                node->AddPred(virtIn);
                nodes.push_back(virtIn);
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
        node->Index = nodes.size();
        nodeMap[&inst] = node;
        nodes.push_back(node);
        if (virtSucc) {
            virtSucc->Index = nodes.size();
            virtSucc->AddPred(node);
            nodes.push_back(virtSucc);
        }
    }

    // Build successor dependency
    {
        std::vector<Node *>::iterator i = nodes.begin(), e = nodes.end();
        for (; i != e; ++i) {
            (*i)->PropagateSucc();
        }
    }

    EnumerateMISO(nodes, 2);
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
