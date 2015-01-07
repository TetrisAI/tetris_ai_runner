
#include "rb_tree.h"
#include "sb_tree.h"

#include <string>
#include <ctime>
#include <iostream>
#include <vector>

struct Node
{
    Node *rb_parent, *rb_left, *rb_right;
    Node *sb_parent, *sb_left, *sb_right;
    bool is_black;
    size_t size;
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

struct SBTreeInterface
{
    typedef int key_t;
    static key_t const &get_key(Node *node)
    {
        return node->value;
    }
    static Node *get_parent(Node *node)
    {
        return node->sb_parent;
    }
    static void set_parent(Node *node, Node *parent)
    {
        node->sb_parent = parent;
    }
    static Node *get_left(Node *node)
    {
        return node->sb_left;
    }
    static void set_left(Node *node, Node *left)
    {
        node->sb_left = left;
    }
    static Node *get_right(Node *node)
    {
        return node->sb_right;
    }
    static void set_right(Node *node, Node *right)
    {
        node->sb_right = right;
    }
    static size_t get_size(Node *node)
    {
        return node->size;
    }
    static void set_size(Node *node, size_t size)
    {
        node->size = size;
    }
    static bool predicate(key_t const &left, key_t const &right)
    {
        return left < right;
    }
};

void tree_test()
{
    zzz::rb_tree<Node, RBTreeInterface> rb;
    zzz::sb_tree<Node, SBTreeInterface> sb;
    std::vector<Node *> data;

    auto c = [&data](int v)
    {
        auto n = new Node();
        n->value = v;
        data.push_back(n);
        return n;
    };
    int length = 20000;
    for(int i = 0; i < length; ++i)
    {
        c(std::rand());
    }

    time_t rb_t1 = clock();
    for(auto n : data)
    {
        rb.insert(n);
    }
    time_t rb_t2 = clock();

    std::cout << "rb random insert " << rb_t2 - rb_t1 << std::endl;

    time_t sb_t1 = clock();
    for(auto n : data)
    {
        sb.insert(n);
    }
    time_t sb_t2 = clock();

    std::cout << "sb random insert " << sb_t2 - sb_t1 << std::endl;

    for(int i = 0; i < length; ++i)
    {
        data[i]->value = i;
    }
    rb.clear();
    sb.clear();

    time_t rb_t3 = clock();
    for(auto n : data)
    {
        rb.insert(n);
    }
    time_t rb_t4 = clock();

    std::cout << "rb sorted insert " << rb_t4 - rb_t3 << std::endl;

    time_t sb_t3 = clock();
    for(auto n : data)
    {
        sb.insert(n);
    }
    time_t sb_t4 = clock();

    std::cout << "sb sorted insert " << sb_t4 - sb_t3 << std::endl;
    
    for(auto n : data)
    {
        delete n;
    }
    data.clear();
}