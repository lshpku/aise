#include "node.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include <sstream>
#include <set>

using namespace aise;
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

void Node::RelaxOrder(std::list<Node *> &buffer)
{
    switch (Type) {
    // associative
    CASE_ASSOCIATIVE : {
        // count steps instead of using end() since Pred may increase
        size_t size = Pred.size(), pos = 0;
        for (node_iterator i = Pred.begin(); pos < size; ++pos) {
            if (TypeOf(*i)) {
                Pred.insert(Pred.end(), (*i)->PredBegin(), (*i)->PredEnd());
                i = Pred.erase(i);
            } else {
                ++i;
            }
        }
    } break;

    // non-commutative
    case SubTy:
    case DivTy:
    case RemTy:
    case ShlTy:
    case LshrTy:
    case AshrTy:
    CASE_CMP:
    case SelectTy: {
        node_iterator i = Pred.begin(), e = Pred.end();
        for (unsigned cnt = 0; i != e && cnt < 3; ++cnt, ++i) {
            // add ordering labels to operands since the second one
            if (cnt > 0) {
                Node *label = new Node((NodeType)(Order1Ty + cnt - 1));
                label->AddPred(*i);
                *i = label;
                buffer.push_back(label);
            }
        }
    } break;

    default:
        break;
    }
}

void Node::ToAssociative(std::list<Node *> &buffer)
{
    NodeType invType;

    switch (Type) {
    case SubTy:
        Type = AddTy;
        invType = AddInvTy;
        break;
    case DivTy:
        Type = MulTy;
        invType = MulInvTy;
        break;
    default:
        return;
    }

    Node *inv = new Node(invType);
    inv->AddPred(Pred.back());
    Pred.back() = inv;
    buffer.push_back(inv);
}

bool Node::IndexLessCompare::operator()(const Node *a, const Node *b) const
{
    if (a == b) {
        return false;
    }
    return a->Index < b->Index;
}

/*bool Node::typeLessCompare::operator()(const Node *a, const Node *b) const
{
    if (a == b) {
        return false;
    }

    if (a->TypeOf(b)) {
        if (a->IsConstant()) {
            return a->SubName < b->SubName;
        }

        // compare recursively when two nodes have the same type
        const_node_iterator ia = a->PredBegin(), ea = a->PredEnd();
        const_node_iterator ib = b->PredBegin(), eb = b->PredEnd();
        for (; ia != ea && ib != eb; ++ia, ++ib) {
            if (operator()(*ia, *ib)) {
                return true;
            }
            if (operator()(*ib, *ia)) {
                return false;
            }
            // continue comparing if *ia and *ib are equal
        }

        // When all operands are equal, node with less operands are
        // considered smaller.
        return a->Pred.size() < b->Pred.size();
    }

    // lable is always bigger than any other types
    if (a->IsLabel()) {
        if (b->IsLabel()) {
            return a->Type < b->Type;
        }
        return false;
    }
    if (b->IsLabel()) {
        return true;
    }

    return a->Type < b->Type;
}*/

void Node::Sort()
{
}

void Node::WriteRPN(std::string &buffer) const
{
    if (TypeOf(ConstTy)) {
        buffer.append(SubName);
        return;
    }
    if (TypeOf(Order1Ty) || TypeOf(Order2Ty)) {
        // label node has exactly one operand
        (*PredBegin())->WriteRPN(buffer);
        return;
    }

    Node::const_node_iterator i = PredBegin(), e = PredEnd();
    for (; i != e; ++i) {
        (*i)->WriteRPN(buffer);
        buffer.push_back(' ');
    }
    buffer.append(TypeName());

    switch (Type) {
    // add a number to associative ops with more than 2 operands
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

namespace
{

bool inline compareTypeHash(const Node *a, const Node *b)
{
    return false;
}

struct typeHashAddrCompare {
    bool operator()(const Node *a, const Node *b) const {
        return false;
    }
};

typedef std::set<Node *, typeHashAddrCompare> node_canon_set;

} // namespace

namespace aise
{
void CanonTopoSort(std::vector<Node *> &DAG, std::vector<size_t> &choice)
{
    node_canon_set leaf;
    std::vector<Node *> sortedDAG;
    sortedDAG.reserve(DAG.size());

    {
        std::vector<Node *>::iterator i = DAG.begin(), e = DAG.end();
        for (; i != e; ++i) {
            if ((*i)->Pred.empty()) {
                leaf.insert(*i);
            }
        }
    }

    size_t choiceIndex = 0;

    while (!leaf.empty()) {
        node_canon_set::iterator i = leaf.begin(), e = leaf.end();
        if (i != e) {
            
        }
    }
}
} // namespace aise
