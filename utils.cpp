#include "utils.h"
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
#include <queue>
#include <sstream>
#include <fstream>

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

#define PARSE_MISO_POS \
    errs() << "At line " << lineNum << ", token " << tokenNum << ": "

int ParseMISO(llvm::Twine path, std::list<NodeArray *> &buffer)
{
    OwningPtr<MemoryBuffer> fileBuffer;
    error_code getFileErr = MemoryBuffer::getFile(path, fileBuffer);
    if (getFileErr != error_code::success()) {
        errs() << path << ": " << getFileErr.message() << '\n';
        return -1;
    }

    StringRef fileRef = fileBuffer->getBuffer(), lineRef;
    size_t EOL = 0, lineNum = 1, tokenNum;
    std::string token, fromTokenErr;

    for (; EOL != StringRef::npos; lineNum++) {
        size_t nextEOL = fileRef.find('\n', EOL);
        if (nextEOL == StringRef::npos) {
            lineRef = fileRef.substr(EOL);
            EOL = nextEOL;
        } else {
            lineRef = fileRef.substr(EOL, nextEOL - EOL);
            EOL = nextEOL + 1;
        }

        lineRef = lineRef.trim();
        if (lineRef.empty()) {
            continue;
        }
        std::stringstream line(lineRef.str());

        // RPN holds nodes in the same order as in input text. There may be
        // replicated nodes when meets an "@".
        NodeArray RPN, stack;
        // DAG is the formal representation of the instruction. It contains
        // invisible nodes like ordering labels.
        NodeArray *DAG = new NodeArray();

        for (tokenNum = 1; line >> token; tokenNum++) {
            // For ref node, push into stack but don't add to DAG.
            if (token[0] == '@') {
                int value;
                if (ParseInt(token.substr(1), value) < 0) {
                    PARSE_MISO_POS << "invalid ref: " << token << '\n';
                    return -1;
                }
                if (value < 1 || value > RPN.size()) {
                    PARSE_MISO_POS << "Ref index out of bound: "
                                   << token << '\n';
                    return -1;
                }
                Node *node = RPN[value - 1];
                RPN.push_back(node);
                stack.push_back(node);
            }

            // For real node, replace into stack and add to DAG.
            else {
                Node *node = Node::FromToken(token, fromTokenErr);
                if (!node) {
                    PARSE_MISO_POS << fromTokenErr << '\n';
                    return -1;
                }
                RPN.push_back(node);
                DAG->push_back(node);
                // set pred for node
                std::list<Node *>::reverse_iterator
                    i = node->Pred.rbegin(),
                    e = node->Pred.rend();
                for (; i != e; ++i) {
                    if (stack.empty()) {
                        PARSE_MISO_POS << "Stack pop out\n";
                        return -1;
                    }
                    *i = stack.back();
                    stack.pop_back();
                }
                stack.push_back(node);
            }
        }

        if (stack.size() > 1) {
            tokenNum--;
            PARSE_MISO_POS << "Too many outputs\n";
            return -1;
        }

        LegalizeDAG(DAG);
        buffer.push_back(DAG);
    }
    return lineNum;
}

#define PARSE_CONF_POS errs() << "At line " << lineNum << ": "

int ParseConf(llvm::Twine path, std::list<size_t> &buffer)
{
    OwningPtr<MemoryBuffer> fileBuffer;
    error_code getFileErr = MemoryBuffer::getFile(path, fileBuffer);
    if (getFileErr != error_code::success()) {
        errs() << path << ": " << getFileErr.message() << '\n';
        return -1;
    }
    StringRef fileRef = fileBuffer->getBuffer(), lineRef;
    size_t EOL = 0, lineNum = 1;

    for (; EOL != StringRef::npos; lineNum++) {
        size_t nextEOL = fileRef.find('\n', EOL);
        if (nextEOL == StringRef::npos) { // last line
            lineRef = fileRef.substr(EOL);
            EOL = nextEOL;
        } else {
            lineRef = fileRef.substr(EOL, nextEOL - EOL);
            EOL = nextEOL + 1;
        }

        lineRef = lineRef.trim();
        if (lineRef.empty()) {
            continue;
        }

        size_t eqIndex = lineRef.find('=');
        if (eqIndex == StringRef::npos) {
            PARSE_CONF_POS << "Incomplete line: Missing '='\n";
            return -1;
        }

        StringRef valueRef = lineRef.substr(eqIndex + 1).trim();
        int value;
        if (ParseInt(valueRef.str(), value) < 0) {
            PARSE_CONF_POS << "Invalid value: " << valueRef << '\n';
            return -1;
        }

        buffer.push_back(value);
    }
    return lineNum;
}

int ParseInt(const std::string &str, int &buffer)
{
    if (str.empty()) {
        return -1;
    }

    size_t i = 0, e = str.size();
    bool positive;
    if (str[i] == '-') {
        if (++i == e) {
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
            return -1;
        }
        sum = sum * 10;
        sum += val;
    }

    buffer = positive ? sum : -sum;
    return 0;
}

std::string ToString(int a)
{
    std::stringstream buf;
    buf.clear();
    buf << a;
    return buf.str();
}

OutFile::OutFile(const char *path)
{
    std::string err;
    out = new tool_output_file(path, err);
    if (!err.empty()) {
        errs() << err << '\n';
        delete out;
        out = NULL;
    }
}

OutFile::~OutFile()
{
    out->keep();
    delete out;
    out = NULL;
}

Permutation::Permutation(size_t n)
{
    index.resize(n);
    for (size_t i = 0; i < n; i++) {
        index[i] = i;
    }
    status.reserve(n);
    if (n > 0) {
        status.push_back(0);
    }
}

const std::vector<size_t> &Permutation::Next()
{
    // push status until full
    while (status.size() < index.size()) {
        status.push_back(0);
    }

    // pop status until one is increased or empty
    while (!status.empty()) {
        size_t i = status.size() - 1;
        if (status.back() == index.size() - status.size()) {
            status.pop_back();
            size_t lastIndex = index[i];
            for (; i < index.size() - 1; i++) {
                index[i] = index[i + 1];
            }
            index[i] = lastIndex;
        } else {
            status[i]++;
            size_t tmp = index[i];
            index[i] = index[i + status[i]];
            index[i + status[i]] = tmp;
            break;
        }
    }

    return index;
}

} // namespace aise
