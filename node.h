#ifndef AISE_NODE_H
#define AISE_NODE_H

#include <string>
#include <list>
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_os_ostream.h"

namespace aise
{

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

    struct IndexLessCompare {
        bool operator()(const Node *a, const Node *b) const
        {
            return a->Index < b->Index;
        }
    };

    NodeType Type;
    std::list<Node *> Pred, Succ;
    size_t Index;
    std::string SubName;

    Node() : Type(UnkTy) {}
    Node(NodeType type) : Type(type) {}

    static const char *TypeName(NodeType type);
    const char *TypeName() const { return TypeName(Type); }

    bool TypeOf(const Node *target) const { return target->Type == Type; }

    static Node *FromInstruction(const llvm::Instruction *inst);
    static Node *FromValue(const llvm::Value *val);
    static Node *FromTypeOfNode(const Node *target);

    void AddPred(Node *node) { Pred.push_back(node); }
    void AddSucc(Node *node) { Succ.push_back(node); }

    typedef std::list<Node *>::iterator node_iterator;
    typedef std::list<Node *>::const_iterator const_node_iterator;

    const_node_iterator PredBegin() const { return Pred.begin(); }
    const_node_iterator PredEnd() const { return Pred.end(); }
    const_node_iterator SuccBegin() const { return Succ.begin(); }
    const_node_iterator SuccEnd() const { return Succ.end(); }

  private:
    // getSameTypeNodes adds all connected nodes of the same type to buffer.
    // The current node would not be added.
    void getSameTypeNodes(std::list<Node *> &buffer);

  public:
    // RelaxOrder merges operands of associative ops (+, *, &, |, ^), and
    // adds order labels to non-commutative ops (-, /, %, shift, ?:, cmp).
    // Nodes may be excluded from the instruction, and there may be new
    // nodes created. For convenience of memory management, new nodes are
    // added to buffer.
    void RelaxOrder(std::vector<Node *> &buffer);

    // Sort sorts the tree into a canonical form.
    // It doesn't expand the DAG into a tree, but change the order of
    // children of each node.
    void Sort();

    // WriteRPN writes the Reversed Polish notation of the expanding tree
    // of this node.
    void WriteRPN(std::string &buffer) const;
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, Node::NodeType type);
llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const Node &node);

} // namespace aise

#endif
