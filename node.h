#ifndef AISE_NODE_H
#define AISE_NODE_H

#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_os_ostream.h"
#include <string>
#include <vector>
#include <list>

namespace aise
{

class Node;
class ConstNode;
typedef std::vector<Node *> NodeArray;

class Node
{
  public:
    enum NodeType {
        UnkTy,
        ConstTy,

        // Unary ops
        AddInvTy,
        MulInvTy,

        // Binary arithmetic ops
        AddTy,
        SubTy,
        MulTy,
        DivTy,
        RemTy,

        // Binary logical ops
        ShlTy,
        LshrTy,
        AshrTy,
        AndTy,
        OrTy,
        XorTy,

        // Binary comparative ops
        EqTy,
        NeTy,
        GtTy,
        GeTy,
        LtTy,
        LeTy,

        // Trinary op
        SelectTy,

        // Ordering labels
        // Note: there is no Order0Ty since order 0 doesn't need a label.
        Order1Ty,
        Order2Ty,

        // Input variables
        FirstInputTy,
    };

    NodeType Type;
    std::list<Node *> Pred, Succ;
    size_t Index;

    Node() : Type(UnkTy) {}
    Node(NodeType type) : Type(type) {}

    static const char *TypeName(NodeType type);
    const char *TypeName() const { return TypeName(Type); }
    void WriteTypeName(std::string &buffer) const;

    bool TypeOf(const Node *target) const { return target->Type == Type; }
    bool TypeOf(NodeType _type) const { return _type == Type; }

    bool IsLabel() const { return TypeOf(Order1Ty) || TypeOf(Order2Ty); }
    bool IsConstant() const { return TypeOf(ConstTy); }
    bool IsAssociative() const;

    static Node *FromInstruction(const llvm::Instruction *inst);
    static Node *FromValue(const llvm::Value *val);
    static Node *FromTypeOfNode(const Node *target);
    // FromToken creates a node corresponding to the token.
    // Pred of the node is set to all NULLs. The caller is responsible to
    // assign right values to them.
    // If token is invalid, this method returns NULL and sets error.
    static Node *FromToken(const std::string &token, std::string &error);

    // Delete deletes the node.
    // This method will call the right deconstructor. Always use this one
    // instead of calling delete yourself.
    static void Delete(Node *node);

    void AddPred(Node *node) { Pred.push_back(node); }
    void AddSucc(Node *node) { Succ.push_back(node); }
    void PropagateSucc();

    typedef std::list<Node *>::iterator node_iterator;
    typedef std::list<Node *>::const_iterator const_node_iterator;

    const_node_iterator PredBegin() const { return Pred.begin(); }
    const_node_iterator PredEnd() const { return Pred.end(); }
    const_node_iterator SuccBegin() const { return Succ.begin(); }
    const_node_iterator SuccEnd() const { return Succ.end(); }

    struct LessIndexCompare {
        bool operator()(const Node *a, const Node *b) const;
    };

    struct LessTypeCompare {
        bool operator()(const Node *a, const Node *b) const;
    };

    // ToAssociative transforms the op to it's associative-equivalent form.
    // There may be new nodes created and they are added to buffer.
    // Note: call this method after the node is completed.
    void ToAssociative(std::list<Node *> &buffer);

    // RelaxOrder merges operands of associative ops (+, *, &, |, ^), and
    // adds order labels to non-commutative ops (-, /, %, shift, ?:, cmp).
    // Merged operands are excluded from the current node. For convenience
    // of memory management, new labels are added to buffer.
    // Note: this method is not recursive. Call it in topological order.
    void RelaxOrder(std::list<Node *> &buffer);

    // Sort sorts the operands of the current node (not recursive).
    // It's required that the predecessors are all sorted.
    void Sort();

    // WriteRPN writes the Reversed Polish notation of the expanding tree
    // of this node.
    void WriteRPN(std::string &buffer) const;

    // WriteRefRPN is like WriteRPN but uses reference when an operand has
    // been represented before.
    // index is the current index in RPN (starts from 1).
    // Returns the new index.
    size_t WriteRefRPN(std::string &buffer, size_t index = 1);
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, Node::NodeType type);
llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const Node &node);

class ConstNode : public Node
{
  public:
    std::string Value;

    static std::string &ValueOf(Node *node)
    {
        return ((ConstNode *)node)->Value;
    }
    static const std::string &ValueOf(const Node *node)
    {
        return ((const ConstNode *)node)->Value;
    }

    ConstNode() : Node(ConstTy) {}
    ConstNode(const std::string &value) : Node(ConstTy), Value(value) {}
};

} // namespace aise

#endif
