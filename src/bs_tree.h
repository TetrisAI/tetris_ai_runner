
#pragma once

#include <iterator>

namespace zzz
{
    //incomplete
    template<class Interface>
    class bs_tree
    {
    public:
        typedef Interface interface_t;
        typedef typename interface_t::node_t node_t;
        typedef typename interface_t::value_node_t value_node_t;
        typedef typename interface_t::key_t key_t;
    protected:
        bs_tree()
        {
            set_nil_(nil_(), true);
            set_root_(nil_());
            set_most_left_(nil_());
            set_most_right_(nil_());
        }

    protected:
        node_t head_;

    protected:
        node_t *nil_()
        {
            return &head_;
        }

        node_t *get_root_()
        {
            return get_parent_(&head_);
        }

        void set_root_(node_t *root)
        {
            set_parent_(&head_, root);
        }

        node_t *get_most_left_()
        {
            return get_left_(&head_);
        }

        void set_most_left_(node_t *left)
        {
            set_left_(&head_, left);
        }

        node_t *get_most_right_()
        {
            return get_right_(&head_);
        }

        void set_most_right_(node_t *right)
        {
            set_right_(&head_, right);
        }

        static key_t const &get_key_(node_t *node)
        {
            return interface_t::get_key(static_cast<value_node_t *>(node));
        }

        static void set_nil_(node_t *node, bool nil)
        {
            return interface_t::set_nil(node, nil);
        }

        static bool is_nil_(node_t *node)
        {
            return interface_t::is_nil(node);
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
            set_root_(nil_());
            set_most_left_(nil_());
            set_most_right_(nil_());
        }

        node_t *bst_init_node_(node_t *parent, node_t *node)
        {
            set_nil_(node, false);
            set_parent_(node, parent);
            set_left_(node, nil_());
            set_right_(node, nil_());
            return node;
        }

        static node_t *bst_next_(node_t *node)
        {
            if(!is_nil_(node))
            {
                if(!is_nil_(get_right_(node)))
                {
                    node = get_right_(node);
                    while(!is_nil_(get_left_(node)))
                    {
                        node = get_left_(node);
                    }
                }
                else
                {
                    node_t *parent;
                    while(!is_nil_(parent = get_parent_(node)) && node == get_right_(parent))
                    {
                        node = parent;
                    }
                    node = parent;
                }
            }
            else
            {
                return get_left_(node);
            }
            return node;
        }

        static node_t *bst_prev_(node_t *node)
        {
            if(!is_nil_(node))
            {
                if(!is_nil_(get_left_(node)))
                {
                    node = get_left_(node);
                    while(!is_nil_(get_right_(node)))
                    {
                        node = get_right_(node);
                    }
                }
                else
                {
                    node_t *parent;
                    while(!is_nil_(parent = get_parent_(node)) && node == get_left_(parent))
                    {
                        node = parent;
                    }
                    node = parent;
                }
            }
            else
            {
                return get_right_(node);
            }
            return node;
        }

        static node_t *bst_min_(node_t *node)
        {
            while(!is_nil_(get_left_(node)))
            {
                node = get_left_(node);
            }
            return node;
        }

        static node_t *bst_max_(node_t *node)
        {
            while(!is_nil_(get_right_(node)))
            {
                node = get_right_(node);
            }
            return node;
        }

        node_t *bst_lower_bound_(key_t const &key)
        {
            node_t *node = get_root_(), *where = nil_();
            while(!is_nil_(node))
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
            node_t *node = root_, *where = nil_();
            while(!is_nil_(node))
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
            if(!is_nil_(get_left_(right)))
            {
                set_parent_(get_left_(right), node);
            }
            set_parent_(right, parent);
            if(node == get_root_())
            {
                set_root_(right);
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
            if(!is_nil_(get_right_(left)))
            {
                set_parent_(get_right_(left), node);
            }
            set_parent_(left, parent);
            if(is_nil_(parent))
            {
                set_root_(left);
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