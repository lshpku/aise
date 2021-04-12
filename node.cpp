#include "node.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"

using namespace llvm;

namespace aise
{

#define OPCODE_NODE_TYPE(o, t) \
    case Instruction::o:       \
        type = Node::t;        \
        break

#define PRED_NODE_TYPE(p, t) \
    case CmpInst::p:         \
        type = Node::t;      \
        break

Node *Node::FromInstruction(const Instruction &inst)
{
    Node::NodeType type;

    switch (inst.getOpcode()) {
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
        switch (((const ICmpInst &)inst).getSignedPredicate()) {
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

#define NODE_TYPE_NAME(t, s) \
    case Node::t:            \
        name = s;            \
        break

raw_ostream &operator<<(raw_ostream &out, Node::NodeType type)
{
    char const *name;

    switch (type) {
        NODE_TYPE_NAME(UnkTy, "unk");

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
        out << "args[" << type - Node::FirstInputTy << "]";
    }

    out << name;
    return out;
}

raw_ostream &operator<<(raw_ostream &out, const Node &node)
{
    out << '%' << node.Index << " = " << node.Type;
    Node::const_node_iterator i = node.PredBegin(), e = node.PredEnd();
    for (; i != e; ++i) {
        out << " %" << (*i)->Index;
    }
    return out;
}

#undef NODE_TYPE_NAME

} // namespace aise
