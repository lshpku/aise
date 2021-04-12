#include "node.h"
#include "miso.h"
#include "ioutils.h"
#include "llvm/Support/raw_os_ostream.h"
#include <vector>
#include <set>
#include <map>
#include <queue>

using namespace aise;
using namespace llvm;

namespace
{

typedef std::priority_queue<Node *, std::vector<Node *>, Node::IndexLessCompare> node_heap;

void pushPred(Node *node, node_heap &buffer)
{
    Node::const_node_iterator i = node->PredBegin(), e = node->PredEnd();
    for (; i != e; ++i) {
        if ((*i)->Type != Node::UnkTy) {
            buffer.push(*i);
        }
    }
}

bool succAllSelected(const Node *node, const std::set<Node *> &selected)
{
    Node::const_node_iterator i = node->SuccBegin(),
                              e = node->SuccEnd();
    for (; i != e; ++i) {
        if (selected.find(*i) == selected.end()) {
            return false;
        }
    }
    return true;
}

void getUpperCone(Node *root, std::vector<Node *> &buffer)
{
    if (root->Type == Node::UnkTy) {
        return;
    }

    node_heap candidates;
    std::set<Node *> selected;
    pushPred(root, candidates);
    buffer.push_back(root);
    selected.insert(root);

    while (!candidates.empty()) {
        Node *node = candidates.top();
        candidates.pop();

        // node not selected
        if (selected.find(node) != selected.end()) {
            continue;
        }
        // convex and has no outer successor
        if (!succAllSelected(node, selected)) {
            continue;
        }

        // select the node
        pushPred(node, candidates);
        buffer.push_back(node);
        selected.insert(node);
    }
}

struct MISOContext {
    // root is at index 0
    std::vector<Node *> upperCone;
    std::vector<bool> choices;
    // selected and inputs don't overlap
    std::set<Node *> selected, inputs;
    int maxIn;
};

void yieldMISO(const std::vector<Node *> &nodes, MISOContext &context)
{
    /*std::map<size_t, Node *> nodeMap;
    std::set<size_t>::iterator i, e;

    // make a copy of selected and input nodes
    i = context.selected.begin(), e = context.selected.end();
    for (; i != e; ++i) {
        Node *node = new Node(nodes[*i]->Type());
        // selected node inherites all predicessors
        node->prevList = nodes[*i]->prevList;
        nodeMap[*i] = node;
    }

    unsigned intputIndex = 0;
    i = context.selected.begin(), e = context.selected.end();
    for (; i != e; ++i) {
        unsigned type = Node::FirstInputTy + intputIndex++;
        Node *node = new Node((Node::NodeType)type);
        // input node have no predicessor
        nodeMap[*i] = node;
    }

    // get key of the MISO
    std::string key;
    std::vector<size_t> stack;
    outs() << key << '\n';*/
}

void recurseMISO(const std::vector<Node *> &nodes, MISOContext &context)
{
    /*std::vector<size_t> newInputs;
    bool isInput = false;
    bool choice = context.choices.back();
    size_t index = context.upperCone[context.choices.size() - 1];

    if (choice) {
        // check outputs
        // all nodes except root should have their outputs selected
        if (context.choices.size() > 1) {
            if (!succAllSelected(nodes[index], context.selected)) {
                return;
            }
        }

        // update inputs
        Node::list_iter prevIter = nodes[index]->prevList.begin(),
                        prevEnd = nodes[index]->prevList.end();
        for (; prevIter != prevEnd; ++prevIter) {
            if (context.inputs.find(*prevIter) == context.inputs.end()) {
                newInputs.push_back(*prevIter);
                context.inputs.insert(*prevIter);
            }
        }
        if (context.inputs.find(index) != context.inputs.end()) {
            isInput = true;
            context.inputs.insert(index);
        }

        context.selected.insert(index);

        // yield a result
        if (context.inputs.size() <= context.maxIn) {
            yieldMISO(nodes, context);
        }
    }

    // recurse
    if (context.choices.size() < context.upperCone.size()) {
        context.choices.push_back(true);
        recurseMISO(nodes, context);
        context.choices.pop_back();
        context.choices.push_back(false);
        recurseMISO(nodes, context);
        context.choices.pop_back();
    }

    // restore selected and inputs
    if (choice) {
        context.selected.erase(index);
        if (isInput) {
            context.inputs.insert(index);
        }
        std::vector<size_t>::iterator inputIter = newInputs.begin(),
                                      inputEnd = newInputs.end();
        for (; inputIter != inputEnd; ++inputIter) {
            context.inputs.erase(*inputIter);
        }
    }*/
}

} // namespace

namespace aise
{

void EnumerateMISO(const std::vector<Node *> &nodes, int maxIn)
{
    if (nodes.empty()) {
        return;
    }

    MISOContext context;
    context.maxIn = maxIn;

    for (int i = 0, e = nodes.size(); i < e; i++) {
        context.choices.clear();
        context.selected.clear();
        context.upperCone.clear();
        context.inputs.clear();
        getUpperCone(nodes[i], context.upperCone);
        outs() << "    " << *nodes[i] << "\n[";
        for (int j = 0; j < context.upperCone.size(); j++) {
            outs() << context.upperCone[j]->Index << ' ';
        }
        outs() << "]\n";

        if (!context.upperCone.empty()) {
            // always select root
            context.choices.push_back(context.upperCone[0]);
            recurseMISO(nodes, context);
        }
    }
}

} // namespace aise
