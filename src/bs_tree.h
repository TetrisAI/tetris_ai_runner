
#pragma once

#include <iterator>

namespace zzz
{
    //incomplete
    template<class Node, class Interface>
    class bs_tree
    {
    public:
        typedef Node node_t;
        typedef Interface interface_t;
        typedef typename interface_t::key_t key_t;
    protected:
        bs_tree() : root_(), left_(), right_()
        {
        }

    protected:
        node_t *root_;
        node_t *left_;
        node_t *right_;

    protected:
        static key_t const &get_key_(node_t *node)
        {
            return interface_t::get_key(node);
        }

        static node_t *get_parent_(node_t *node)
        {
            return interface_t::get_parent(node);
        }

        static void set_parent_(node_t *node, node_t *parent)
        {
            interface_t::set_parent(node, parent);
        }

        static node_t *get_left_(node_t *node)
        {
            return interface_t::get_left(node);
        }

        static void set_left_(node_t *node, node_t *left)
        {
            interface_t::set_left(node, left);
        }

        static node_t *get_right_(node_t *node)
        {
            return interface_t::get_right(node);
        }

        static void set_right_(node_t *node, node_t *right)
        {
            interface_t::set_right(node, right);
        }

        static bool predicate(node_t *left, node_t *right)
        {
            return interface_t::predicate(get_key_(left), get_key_(right));
        }

        void bst_clear_()
        {
            root_ = nullptr;
            left_ = nullptr;
            right_ = nullptr;
        }

        node_t *bst_init_node_(node_t *parent, node_t *node)
        {
            set_parent_(node, parent);
            set_left_(node, nullptr);
            set_right_(node, nullptr);
            return node;
        }

        static node_t *bst_next_(node_t *node)
        {
            if(node != nullptr)
            {
                if(get_right_(node) != nullptr)
                {
                    node = get_right_(node);
                    while(get_left_(node) != nullptr)
                    {
                        node = get_left_(node);
                    }
                }
                else
                {
                    node_t *parent;
                    while((parent = get_parent_(node)) != nullptr && node == get_right_(parent))
                    {
                        node = parent;
                    }
                    node = parent;
                }
            }
            return node;
        }

        static node_t *bst_prev_(node_t *node)
        {
            if(node != nullptr)
            {
                if(get_left_(node) != nullptr)
                {
                    node = get_left_(node);
                    while(get_right_(node) != nullptr)
                    {
                        node = get_right_(node);
                    }
                }
                else
                {
                    node_t *parent;
                    while((parent = get_parent_(node)) != nullptr && node == get_left_(parent))
                    {
                        node = parent;
                    }
                    node = parent;
                }
            }
            return node;
        }

        static node_t *bst_min_(node_t *node)
        {
            while(get_left_(node) != nullptr)
            {
                node = get_left_(node);
            }
            return node;
        }

        static node_t *bst_max_(node_t *node)
        {
            while(get_right_(node) != nullptr)
            {
                node = get_right_(node);
            }
            return node;
        }

        node_t *bst_lower_bound_(key_t const &key)
        {
            node_t *node = root_, *where = nullptr;
            while(node != nullptr)
            {
                if(interface_t::predicate(get_key_(node), key))
                {
                    node = get_right_(node);
                }
                else
                {
                    where = node;
                    node = get_left_(node);
                }
            }
            return where;
        }

        node_t *bst_upper_bound_(key_t const &key)
        {
            node_t *node = root_, *where = nullptr;
            while(node != nullptr)
            {
                if(interface_t::predicate(key, get_key_(node)))
                {
                    where = node;
                    node = get_left_(node);
                }
                else
                {
                    node = get_right_(node);
                }
            }
            return where;
        }

        node_t *bst_left_rotate_(node_t *node)
        {
            node_t *right = get_right_(node), *parent = get_parent_(node);
            set_right_(node, get_left_(right));
            if(get_left_(right) != nullptr)
            {
                set_parent_(get_left_(right), node);
            }
            set_parent_(right, parent);
            if(node == root_)
            {
                root_ = right;
            }
            else if(node == get_left_(parent))
            {
                set_left_(parent, right);
            }
            else
            {
                set_right_(parent, right);
            }
            set_left_(right, node);
            set_parent_(node, right);
            return right;
        }

        node_t *bst_right_rotate_(node_t *node)
        {
            node_t *left = get_left_(node), *parent = get_parent_(node);
            set_left_(node, get_right_(left));
            if(get_right_(left) != nullptr)
            {
                set_parent_(get_right_(left), node);
            }
            set_parent_(left, parent);
            if(parent == nullptr)
            {
                root_ = left;
            }
            else if(node == get_right_(parent))
            {
                set_right_(parent, left);
            }
            else
            {
                set_left_(parent, left);
            }
            set_right_(left, node);
            set_parent_(node, left);
            return left;
        }
    };

}