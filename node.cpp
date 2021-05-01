#include "node.h"
#include "utils.h"
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
        name = "$*";
    }
    return name;
}

void Node::WriteTypeName(std::string &buffer) const
{
    switch (Type) {
    case ConstTy:
        buffer.append(((const ConstNode *)this)->Value);
        break;
    case IntriTy:
        buffer.append("\"");
        buffer.append(((const IntriNode *)this)->RefRPN);
        buffer.append("\"");
        break;
    default:
        if (Type >= FirstInputTy) {
            buffer.push_back('$');
            buffer.append(ToString(Type - FirstInputTy + 1));
        } else {
            buffer.append(TypeName());
        }
    }
}

#define CASE_ASSOCIATIVE \
    case AddTy:          \
    case MulTy:          \
    case AndTy:          \
    case OrTy:           \
    case XorTy

bool Node::IsAssociative() const
{
    switch (Type) {
    CASE_ASSOCIATIVE:
        return true;
    default:
        return false;
    }
}

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

Node *Node::FromValue(const Value *val)
{
    if (!Constant::classof(val)) {
        return new Node(); // unk
    }
    ConstNode *node = new ConstNode();
    if (val->getType()->isIntegerTy()) {
        node->Value = ((const Constant *)val)->getUniqueInteger().toString(10, true);
    } else {
        node->Value = "inf";
    }
    return node;
}

Node *Node::FromTypeOfNode(const Node *target)
{
    if (target->TypeOf(ConstTy)) {
        return new ConstNode(ConstNode::ValueOf(target));
    }
    return new Node(target->Type);
}

#define CASE_TOKEN_TYPE(c, t) \
    case c:                   \
        type = t;             \
        break

#define MATCH_TOKEN_TYPE(m, t) \
    if (token == m) {          \
        type = t;              \
        break;                 \
    }

Node *Node::FromToken(const std::string &token, std::string &error)
{
    if (token.empty()) {
        error = "Empty token";
        return NULL;
    }
    int value;
    if (ParseInt(token, value) == 0) {
        return new ConstNode(token);
    }

    // decide by the first char
    NodeType type = UnkTy;
    switch (token[0]) {
        CASE_TOKEN_TYPE('+', AddTy);
        CASE_TOKEN_TYPE('-', SubTy);
        CASE_TOKEN_TYPE('/', DivTy);
        CASE_TOKEN_TYPE('%', RemTy);
        CASE_TOKEN_TYPE('&', AndTy);
        CASE_TOKEN_TYPE('|', OrTy);

    case '*':
        MATCH_TOKEN_TYPE("*-1", AddInvTy)
        type = MulTy;
        break;
    case '^':
        MATCH_TOKEN_TYPE("^-1", MulInvTy)
        type = XorTy;
        break;
    case '<':
        MATCH_TOKEN_TYPE("<", LtTy)
        MATCH_TOKEN_TYPE("<=", LeTy)
        MATCH_TOKEN_TYPE("<<", ShlTy)
        break;
    case '>':
        MATCH_TOKEN_TYPE(">", GtTy)
        MATCH_TOKEN_TYPE(">=", GeTy)
        MATCH_TOKEN_TYPE(">>", AshrTy)
        MATCH_TOKEN_TYPE(">>>", LshrTy)
        break;
    case '=':
        MATCH_TOKEN_TYPE("==", EqTy);
        break;
    case '!':
        MATCH_TOKEN_TYPE("!=", NeTy);
        break;
    case '?':
        MATCH_TOKEN_TYPE("?:", SelectTy);
        break;

    case '$':
        if (ParseInt(token.substr(1), value) < 0) {
            error = "Invalid input index: ";
            error.append(token);
            return NULL;
        }
        if (value < 1) {
            error = "Input index too small: ";
            error.append(token);
            return NULL;
        }
        return new Node((NodeType)(FirstInputTy + value - 1));
    }

    if (type == UnkTy) {
        error = "Unknown token: ";
        error.append(token);
        return NULL;
    }

    // decide number of preds
    size_t predCnt = 2;
    switch (type) {
    CASE_ASSOCIATIVE:
        if (token.size() > 1) {
            if (ParseInt(token.substr(1), value) < 0) {
                error = "Invalid pred identifier: ";
                error.append(token);
                return NULL;
            }
            if (value <= 2) {
                error = "Pred identifier too small: ";
                error.append(token);
                return NULL;
            }
            predCnt = value;
        }
        break;

    case AddInvTy:
    case MulInvTy:
        predCnt = 1;
        break;
    case SelectTy:
        predCnt = 3;
        break;
    }

    Node *node = new Node(type);
    node->Pred.resize(predCnt);
    return node;
}

#define CASE_TYPE_COST(t, c) \
    case t:                  \
        return c

size_t Node::TypeCost(NodeType type)
{
    switch (type) {
        // Cost of inv types is set as that the total cost is the sum of
        // this and cost of the base type.
        CASE_TYPE_COST(AddInvTy, 0);
        CASE_TYPE_COST(MulInvTy, 200);

        CASE_TYPE_COST(AddTy, 100);
        CASE_TYPE_COST(SubTy, 100);
        CASE_TYPE_COST(MulTy, 300);
        CASE_TYPE_COST(DivTy, 500);
        CASE_TYPE_COST(RemTy, 500);

        CASE_TYPE_COST(ShlTy, 20);
        CASE_TYPE_COST(LshrTy, 20);
        CASE_TYPE_COST(AshrTy, 20);
        CASE_TYPE_COST(AndTy, 10);
        CASE_TYPE_COST(OrTy, 10);
        CASE_TYPE_COST(XorTy, 10);

        CASE_TYPE_COST(EqTy, 10);
        CASE_TYPE_COST(NeTy, 10);
        CASE_TYPE_COST(GtTy, 100);
        CASE_TYPE_COST(GeTy, 100);
        CASE_TYPE_COST(LtTy, 100);
        CASE_TYPE_COST(LeTy, 100);

        CASE_TYPE_COST(SelectTy, 20);

    // unk, const and input nodes has a cost of 0
    default:
        return 0;
    }
}

size_t Node::AccCost() const
{
    size_t maxCost = 0;
    const_node_iterator i = PredBegin(), e = PredEnd();
    for (; i != e; ++i) {
        maxCost = std::max(maxCost, (*i)->Index);
    }
    if (IsAssociative()) {
        return (Pred.size() - 1) * TypeCost(Type) + maxCost;
    }
    return TypeCost(Type) + maxCost;
}

#define CASE_TYPE_DELETE(t, c) \
    case t:                    \
        delete (c *)node;      \
        break

void Node::Delete(Node *node)
{
    switch (node->Type) {
        CASE_TYPE_DELETE(ConstTy, ConstNode);
        CASE_TYPE_DELETE(IntriTy, IntriNode);
    default:
        delete node;
    }
}

void Node::PropagateSucc()
{
    for (const_node_iterator i = PredBegin(), e = PredEnd(); i != e; ++i) {
        (*i)->AddSucc(this);
    }
}

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

    case UnkTy:
        break;

    // non-commutative
    default: {
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

bool Node::LessIndexCompare::operator()(const Node *a, const Node *b) const
{
    if (a == b) {
        return false;
    }
    return a->Index < b->Index;
}

bool Node::LessTypeCompare::operator()(const Node *a, const Node *b) const
{
    if (a == b) {
        return false;
    }

    if (a->TypeOf(b)) {
        if (a->IsConstant()) {
            return ConstNode::ValueOf(a) < ConstNode::ValueOf(b);
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
}

void Node::Sort() { Pred.sort(LessTypeCompare()); }

size_t Node::writeRefRPNImpl(std::string &buffer, size_t index)
{
    if (Index > 0) {
        buffer.push_back('@');
        std::stringstream buf;
        buf << Index;
        buffer.append(buf.str());
        return index + 1;
    }

    if (TypeOf(ConstTy)) {
        buffer.append(ConstNode::ValueOf(this));
        Index = index;
        return index + 1;
    }
    if (TypeOf(Order1Ty) || TypeOf(Order2Ty)) {
        // label node doesn't take up space
        return (*PredBegin())->writeRefRPNImpl(buffer, index);
    }

    node_iterator i = Pred.begin(), e = Pred.end();
    for (; i != e; ++i) {
        index = (*i)->writeRefRPNImpl(buffer, index);
        buffer.push_back(' ');
    }
    WriteTypeName(buffer);

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

    Index = index;
    return index + 1;
}

raw_ostream &operator<<(raw_ostream &out, Node::NodeType type)
{
    if (type >= Node::FirstInputTy) {
        out << '$' << type - Node::FirstInputTy + 1;
    } else {
        out << Node::TypeName(type);
    }
    return out;
}

raw_ostream &operator<<(raw_ostream &out, const Node &node)
{
    out << &node << " = ";
    std::string str;
    node.WriteTypeName(str);
    out << str;

    Node::const_node_iterator i = node.PredBegin(), e = node.PredEnd();
    for (; i != e; ++i) {
        out << ' ' << *i;
    }
    return out;
}

} // namespace aise
