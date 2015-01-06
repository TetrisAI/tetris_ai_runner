
#include "rb_tree.h"
#include "sb_tree.h"

#include <string>
#include <cstring>
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
    for(int i = 0; i < 10000; ++i)
    {
        auto n = c(std::rand());
        rb.insert(n);
        sb.insert(n);
    }
    for(int i = 0; i < 9000; ++i)
    {
        auto it_rb = rb.begin();
        auto it_sb = sb.begin();
        std::advance(it_rb, std::rand() % rb.size());
        std::advance(it_sb, std::rand() % sb.size());
        rb.erase(it_rb);
        sb.erase(it_sb);
    }
    for(int i = 0; i < 19000; ++i)
    {
        auto n = c(std::rand());
        rb.insert(n);
        sb.insert(n);
    }

    for(int i = 0; i < 10000; ++i)
    {
        auto assert = [](bool no_error)
        {
            if(!no_error)
            {
                *static_cast<int *>(0) = 0;
            }
        };
        typedef decltype(sb.begin()) iter_t;
        int off = std::rand() % rb.size();
        iter_t it(sb.at(off));
        assert(it - sb.begin() == off);
        assert(it - off == sb.begin());
        assert(sb.begin() + off == it);
        while(off > 0)
        {
            --off;
            --it;
        }
        assert(sb.begin() == it);
        int part = sb.size() / 4;
        int a = part + std::rand() % (part * 2);
        int b = part;
        assert(iter_t(sb.at(a)) + b == iter_t(sb.at(a + b)));
        assert(sb.begin() + a == iter_t(sb.at(a + b)) - b);
        assert(iter_t(sb.at(a)) - iter_t(sb.at(b)) == a - b);
    }

    for(int i = 0; i < 19999; ++i)
    {
        auto it_rb = rb.begin();
        auto it_sb = sb.begin();
        std::advance(it_rb, std::rand() % rb.size());
        std::advance(it_sb, std::rand() % sb.size());
        rb.erase(it_rb);
        sb.erase(it_sb);
    }
    rb.erase(rb.begin());
    sb.erase(sb.begin());

    for(auto n : data)
    {
        delete n;
    }
    data.clear();
}