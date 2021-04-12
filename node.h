#ifndef AISE_NODE_H
#define AISE_NODE_H

#include <string>
#include <ostream>
#include <list>
#include <set>
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_os_ostream.h"

namespace aise
{

class Node
{
  public:
    enum NodeType {
        UnkTy,

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

    struct Comparator {
        bool operator()(const Node *a, const Node *b) const;
    };

  public:
    NodeType type;
    std::list<Node *> prevList, succList;
    std::set<Node *> prevSet, succSet;

    bool Selected, HasPathToSelected;
    bool isInput;

  public:
    Node() : type(UnkTy) {}
    Node(NodeType type_) : type(type_) {}

    static Node *FromInstruction(const llvm::Instruction &inst);

    NodeType Type() const { return type; }

    void AddPrev(Node *node)
    {
        prevList.push_back(node);
        prevSet.insert(node);
    }
    void AddSucc(Node *node)
    {
        succList.push_back(node);
        succSet.insert(node);
    }

    typedef std::list<Node *>::const_iterator list_iter;

    bool hasPrev(Node *node) { return prevSet.find(node) != prevSet.end(); }
    bool hasSucc(Node *node) { return succSet.find(node) != succSet.end(); }

    // Sort sorts the tree into a canonical form.
    // It doesn't expand the DAG into a tree, but change the order of
    // children of each node.
    void Sort();

    // Represent the expanding tree of the node in Reversed Polish Notation.
    std::string Key();
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, Node::NodeType type);
llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const Node &node);

} // namespace aise

#endif
