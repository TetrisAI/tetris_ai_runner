
#include "rb_tree.h"

#include <string>
#include <cstring>
#include <iostream>

struct Node
{
    Node *rb_parent, *rb_left, *rb_right;
    bool is_black;
    int value;
};

struct RBTreeInterface
{
    typedef int key_t;
    static key_t const &get_key(Node *node)
    {
        return node->value;
    }
    static Node *get_parent(Node *node)
    {
        return node->rb_parent;
    }
    static void set_parent(Node *node, Node *parent)
    {
        node->rb_parent = parent;
    }
    static Node *get_left(Node *node)
    {
        return node->rb_left;
    }
    static void set_left(Node *node, Node *left)
    {
        node->rb_left = left;
    }
    static Node *get_right(Node *node)
    {
        return node->rb_right;
    }
    static void set_right(Node *node, Node *right)
    {
        node->rb_right = right;
    }
    static bool is_black(Node *node)
    {
        return node->is_black;
    }
    static void set_black(Node *node, bool black)
    {
        node->is_black = black;
    }
    static bool predicate(key_t const &left, key_t const &right)
    {
        return left < right;
    }
};

void rb_tree_test()
{
    zzz::rb_tree<Node, RBTreeInterface> t;

    auto c = [](int v)
    {
        auto n = new Node();
        n->value = v;
        return n;
    };
    for(int i = 0; i < 100; ++i)
    {
        t.insert(c(i));
    }
    for(int i = 0; i < 50; ++i)
    {
        auto it = t.begin();
        std::advance(it, std::rand() % t.size());
        t.erase(it);
    }
    for(int i = 100; i < 200; ++i)
    {
        t.insert(c(i));
    }
    for(int i = 0; i < 149; ++i)
    {
        auto it = t.begin();
        std::advance(it, std::rand() % t.size());
        t.erase(it);
    }
    t.erase(t.begin());
    std::string str;
    for(auto &v : t)
    {
        char buff[32];
        sprintf(buff, "%d\n", v.value);
        str.append(buff);
    }
    std::cout << str;
}