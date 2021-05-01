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
class IntriNode;
typedef std::vector<Node *> NodeArray;

class Node
{
  public:
    enum NodeType {
        UnkTy,
        ConstTy,
        IntriTy,

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
    bool IsIntrinsic() const { return TypeOf(IntriTy); }
    bool IsAssociative() const;
    bool IsInput() const { return Type >= FirstInputTy; }

    static Node *FromInstruction(const llvm::Instruction *inst);
    static Node *FromValue(const llvm::Value *val);
    static Node *FromTypeOfNode(const Node *target);
    // FromToken creates a node corresponding to the token.
    // Pred of the node is set to all NULLs. The caller is responsible to
    // assign right values to them.
    // If token is invalid, this method returns NULL and sets error.
    static Node *FromToken(const std::string &token, std::string &error);
    template <typename T>
    static Node *FromType(T type) { return new Node((NodeType)type); }

    // UnitCost is the cost of one adder.
    // Cost of an instruction should be round up to multiple of UnitCost.
    static const size_t UnitCost = 100;
    static size_t RoundUpUnitCost(size_t cost)
    {
        return (cost + UnitCost - 1) / UnitCost * UnitCost;
    }

    // TypeCost returns the base cost of this type.
    static size_t TypeCost(NodeType type);
    // CriticalPathCost returns the cost sum of this node and the operand
    // in the critical path. It considers impact of association.
    // This method requires that the costs of its operands are already
    // computed and saved in Index.
    size_t CriticalPathCost() const;

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

    // WriteRefRPN writes the referenced Reversed Polish notation of the
    // upper cone of this node.
    // This method requires that indexes of all the nodes in the upper cone
    // be set to 0, and will change these indexes during processing.
    void WriteRefRPN(std::string &buffer) { writeRefRPNImpl(buffer, 1); }

  private:
    size_t writeRefRPNImpl(std::string &buffer, size_t index);

  public:
    std::list<IntriNode *> TileList;

    // AddTile adds the tile into the matched tile list.
    void AddTile(IntriNode *tile) { TileList.push_back(tile); }
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

class IntriNode : public Node
{
  public:
    std::string RefRPN; // empty for default tile
    size_t Cost;
    std::list<Node *> Covering; // not include inputs

    IntriNode() : Node(IntriTy), Cost(0) {}

    // TileForNode creates the default tile of node. The operands are
    // directly copied and cost is properly set.
    static IntriNode *TileOfNode(Node *node);
};

} // namespace aise

#endif
