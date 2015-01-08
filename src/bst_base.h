
#pragma once

#include <iterator>

namespace zzz
{
    //incomplete
    template<class Interface>
    class bst_base
    {
    public:
        typedef Interface interface_t;
        typedef typename interface_t::node_t node_t;
        typedef typename interface_t::value_node_t value_node_t;
        typedef typename interface_t::key_t key_t;
    protected:
        bst_base()
        {
            static_assert(std::is_base_of<node_t, value_node_t>::value, "node_t must be base of value_node_t");
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

        template<bool is_left>
        static void set_child_(node_t *node, node_t *child)
        {
            if(is_left)
            {
                set_left_(node, child);
            }
            else
            {
                set_right_(node, child);
            }
        }

        template<bool is_left>
        static node_t *get_child_(node_t *node)
        {
            if(is_left)
            {
                return get_left_(node);
            }
            else
            {
                return get_right_(node);
            }
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

        template<bool is_next>
        static node_t *bst_move_(node_t *node)
        {
            if(!is_nil_(node))
            {
                if(!is_nil_(get_child_<!is_next>(node)))
                {
                    node = get_child_<!is_next>(node);
                    while(!is_nil_(get_child_<is_next>(node)))
                    {
                        node = get_child_<is_next>(node);
                    }
                }
                else
                {
                    node_t *parent;
                    while(!is_nil_(parent = get_parent_(node)) && node == get_child_<!is_next>(parent))
                    {
                        node = parent;
                    }
                    node = parent;
                }
            }
            else
            {
                return get_child_<is_next>(node);
            }
            return node;
        }

        template<bool is_min>
        static node_t *bst_most_(node_t *node)
        {
            while(!is_nil_(get_child_<is_min>(node)))
            {
                node = get_child_<is_min>(node);
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
            node_t *node = get_root_(), *where = nil_();
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

        void bst_equal_range_(key_t const &key, node_t *&lower_node, node_t *&upper_node)
        {
            node_t *node = get_root_();
            node_t *lower = nil_();
            node_t *upper = nil_();
            while(!is_nil_(node))
            {
                if(interface_t::predicate(get_key_(node), key))
                {
                    node = get_right_(node);
                }
                else
                {
                    if(is_nil_(upper) && interface_t::predicate(key, get_key_(node)))
                    {
                        upper = node;
                    }
                    lower = node;
                    node = get_left_(node);
                }
            }
            node = is_nil_(upper) ? get_root_() : get_left_(upper);
            while(!is_nil_(node))
            {
                if(interface_t::predicate(key, get_key_(node)))
                {
                    upper = node;
                    node = get_left_(node);
                }
                else
                {
                    node = get_right_(node);
                }
            }
            lower_node = lower;
            upper_node = upper;
        }

        template<bool is_left>
        node_t *bst_rotate_(node_t *node)
        {
            node_t *child = get_child_<!is_left>(node), *parent = get_parent_(node);
            set_child_<!is_left>(node, get_child_<is_left>(child));
            if(!is_nil_(get_child_<is_left>(child)))
            {
                set_parent_(get_child_<is_left>(child), node);
            }
            set_parent_(child, parent);
            if(node == get_root_())
            {
                set_root_(child);
            }
            else if(node == get_child_<is_left>(parent))
            {
                set_child_<is_left>(parent, child);
            }
            else
            {
                set_child_<!is_left>(parent, child);
            }
            set_child_<is_left>(child, node);
            set_parent_(node, child);
            return child;
        }

    };

}