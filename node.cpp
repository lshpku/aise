#include "node.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include <sstream>

using namespace llvm;

namespace aise
{

#define NODE_TYPE_NAME(t, s) \
    case Node::t:            \
        name = s;            \
        break

const char *Node::TypeName(NodeType type)
{
    const char *name;
    static const char *inputName[] = {"%0", "%1", "%2", "%3", "%4"};

    switch (type) {
        NODE_TYPE_NAME(UnkTy, "unk");
        NODE_TYPE_NAME(ConstTy, "C");

        NODE_TYPE_NAME(AddInvTy, "*-1");
        NODE_TYPE_NAME(MulInvTy, "^-1");

        NODE_TYPE_NAME(AddTy, "+");
        NODE_TYPE_NAME(SubTy, "-");
        NODE_TYPE_NAME(MulTy, "*");
        NODE_TYPE_NAME(DivTy, "/");
        NODE_TYPE_NAME(RemTy, "%");

        NODE_TYPE_NAME(ShlTy, "<<");
        NODE_TYPE_NAME(LshrTy, ">>>");
        NODE_TYPE_NAME(AshrTy, ">>");
        NODE_TYPE_NAME(AndTy, "&");
        NODE_TYPE_NAME(OrTy, "|");
        NODE_TYPE_NAME(XorTy, "^");

        NODE_TYPE_NAME(EqTy, "==");
        NODE_TYPE_NAME(NeTy, "!=");
        NODE_TYPE_NAME(GtTy, ">");
        NODE_TYPE_NAME(GeTy, ">=");
        NODE_TYPE_NAME(LtTy, "<");
        NODE_TYPE_NAME(LeTy, "<=");

        NODE_TYPE_NAME(SelectTy, "?:");

        NODE_TYPE_NAME(Order1Ty, "[1]");
        NODE_TYPE_NAME(Order2Ty, "[2]");

    default:
        if (type - Node::FirstInputTy < 5) {
            name = inputName[type - Node::FirstInputTy];
        } else {
            name = "...";
        }
    }
    return name;
}

#undef NODE_TYPE_NAME

#define OPCODE_NODE_TYPE(o, t) \
    case Instruction::o:       \
        type = Node::t;        \
        break

#define PRED_NODE_TYPE(p, t) \
    case CmpInst::p:         \
        type = Node::t;      \
        break

Node *Node::FromInstruction(const Instruction *inst)
{
    Node::NodeType type;

    switch (inst->getOpcode()) {
        OPCODE_NODE_TYPE(Add, AddTy);
        OPCODE_NODE_TYPE(FAdd, AddTy);
        OPCODE_NODE_TYPE(Sub, SubTy);
        OPCODE_NODE_TYPE(FSub, SubTy);
        OPCODE_NODE_TYPE(Mul, MulTy);
        OPCODE_NODE_TYPE(FMul, MulTy);
        OPCODE_NODE_TYPE(UDiv, DivTy);
        OPCODE_NODE_TYPE(SDiv, DivTy);
        OPCODE_NODE_TYPE(FDiv, DivTy);
        OPCODE_NODE_TYPE(URem, RemTy);
        OPCODE_NODE_TYPE(SRem, RemTy);
        OPCODE_NODE_TYPE(FRem, RemTy);

        OPCODE_NODE_TYPE(Shl, ShlTy);
        OPCODE_NODE_TYPE(LShr, LshrTy);
        OPCODE_NODE_TYPE(AShr, AshrTy);
        OPCODE_NODE_TYPE(And, AndTy);
        OPCODE_NODE_TYPE(Or, OrTy);
        OPCODE_NODE_TYPE(Xor, XorTy);

        OPCODE_NODE_TYPE(Select, SelectTy);

    case Instruction::ICmp:
        switch (((const ICmpInst *)inst)->getSignedPredicate()) {
            PRED_NODE_TYPE(ICMP_EQ, EqTy);
            PRED_NODE_TYPE(ICMP_NE, NeTy);
            PRED_NODE_TYPE(ICMP_SGT, GtTy);
            PRED_NODE_TYPE(ICMP_SGE, GeTy);
            PRED_NODE_TYPE(ICMP_SLT, LtTy);
            PRED_NODE_TYPE(ICMP_SLE, LeTy);
        }
        break;

    case Instruction::FCmp:
        type = Node::UnkTy;
        break;

    default:
        type = Node::UnkTy;
        break;
    }

    return new Node(type);
}

#undef OPCODE_NODE_TYPE
#undef PRED_NODE_TYPE

Node *Node::FromValue(const Value *val)
{
    if (!Constant::classof(val)) {
        return new Node(); // unk
    }
    Node *node = new Node(ConstTy);
    if (val->getType()->isIntegerTy()) {
        node->SubName = ((const Constant *)val)->getUniqueInteger().toString(10, true);
    } else {
        node->SubName = "inf";
    }
    return node;
}

Node *Node::FromTypeOfNode(const Node *target)
{
    Node *node = new Node(target->Type);
    if (target->Type == ConstTy) {
        node->SubName = target->SubName;
    }
    return node;
}

void Node::getSameTypeNodes(std::list<Node *> &buffer)
{
    const_node_iterator i = PredBegin(), e = PredEnd();
    for (; i != e; ++i) {
        if (TypeOf(*i)) {
            buffer.push_back(*i);
            (*i)->getSameTypeNodes(buffer);
        }
    }
}

#define CASE_ASSOCIATIVE \
    case AddTy:          \
    case MulTy:          \
    case AndTy:          \
    case OrTy:           \
    case XorTy

#define CASE_CMP \
    case EqTy:   \
    case NeTy:   \
    case GtTy:   \
    case GeTy:   \
    case LtTy:   \
    case LeTy

void Node::RelaxOrder(std::vector<Node *> &buffer)
{
    switch (Type) {
    // associative
    CASE_ASSOCIATIVE : {
        // Merge all operands of the connected nodes of the same type.
        // Those nodes are as wiped from the instruction.
        std::list<Node *> sameTypeNodes;
        getSameTypeNodes(sameTypeNodes);
        outs() << "same type nodes: [";
        for (std::list<Node *>::iterator i = sameTypeNodes.begin(), e = sameTypeNodes.end(); i != e; ++i) {
            outs() << ' ' << *(*i) << ';';
        }
        outs() << "]\n";
        std::list<Node *>::iterator nodeIter = sameTypeNodes.begin(),
                                    nodeEnd = sameTypeNodes.end();
        for (; nodeIter != nodeEnd; ++nodeIter) {
            Node::const_node_iterator opIter = (*nodeIter)->PredBegin(),
                                      opEnd = (*nodeIter)->PredEnd();
            for (; opIter != opEnd; ++opIter) {
                if (!TypeOf(*opIter)) {
                    Pred.push_back(*opIter);
                }
            }
        }
        // delete nodes of the same type from the current node
        {
            node_iterator i = Pred.begin(), e = Pred.end();
            while (i != e) {
                if (TypeOf(*i)) {
                    i = Pred.erase(i);
                } else {
                    ++i;
                }
            }
        }
    } break;

    // non-commutative but can be transformed to commutative one
    case SubTy:
    case DivTy: {

    } break;

    // non-commutative and just need ordering labels
    case RemTy:
    case ShlTy:
    case LshrTy:
    case AshrTy:
    CASE_CMP:
    case SelectTy: {

    } break;

    default:
        break;
    }
}

void Node::Sort()
{
}

void Node::WriteRPN(std::string &buffer) const
{
    if (Type == ConstTy) {
        buffer.append(SubName);
        return;
    }

    Node::const_node_iterator i = PredBegin(), e = PredEnd();
    for (; i != e; ++i) {
        (*i)->WriteRPN(buffer);
        buffer.push_back(' ');
    }
    buffer.append(TypeName());

    // For associative op, add a number to it when having variable operands.
    switch (Type) {
    CASE_ASSOCIATIVE:
        if (Pred.size() > 2) {
            std::stringstream buf;
            buf << Pred.size();
            buffer.append(buf.str());
        }
        break;
    }
}

raw_ostream &operator<<(raw_ostream &out, Node::NodeType type)
{
    if (type >= Node::FirstInputTy) {
        out << '%' << type - Node::FirstInputTy;
    } else {
        out << Node::TypeName(type);
    }
    return out;
}

raw_ostream &operator<<(raw_ostream &out, const Node &node)
{
    out << &node << " = " << node.Type;
    Node::const_node_iterator i = node.PredBegin(), e = node.PredEnd();
    for (; i != e; ++i) {
        out << ' ' << *i;
    }
    return out;
}

} // namespace aise
