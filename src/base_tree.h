
#pragma once

#include <iterator>

namespace zzz
{
    template<class Node, class Interface>
    class base_tree
    {
    public:
        typedef Node node_t;
        typedef Interface interface_t;
        typedef typename interface_t::key_t key_t;
    protected:
        base_tree() : root_(), left_(), right_()
        {
        }

    protected:
        node_t *root_;
        node_t *left_;
        node_t *right_;

    protected:
        static key_t const &get_key(node_t *node)
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

        static size_t get_size_(node_t *node)
        {
            return node == nullptr ? 0 : interface_t::get_size(node);
        }

        static void set_size_(node_t *node, size_t size)
        {
            interface_t::set_size(node, size);
        }

        static bool is_black_(node_t *node)
        {
            return node == nullptr ? true : interface_t::is_black(node);
        }

        static void set_black_(node_t *node, bool black)
        {
            interface_t::set_black(node, black);
        }

        static bool predicate(node_t *left, node_t *right)
        {
            return interface_t::predicate(get_key(left), get_key(right));
        }

        void bs_clear_()
        {
            root_ = nullptr;
            left_ = nullptr;
            right_ = nullptr;
        }

        node_t *bs_init_node_(node_t *parent, node_t *node)
        {
            set_parent_(node, parent);
            set_left_(node, nullptr);
            set_right_(node, nullptr);
            return node;
        }

        static node_t *bs_next_(node_t *node)
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

        static node_t *bs_prev_(node_t *node)
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

        static node_t *bs_min_(node_t *node)
        {
            while(get_left_(node) != nullptr)
            {
                node = get_left_(node);
            }
            return node;
        }

        static node_t *bs_max_(node_t *node)
        {
            while(get_right_(node) != nullptr)
            {
                node = get_right_(node);
            }
            return node;
        }

        node_t *bs_lower_bound_(key_t const &key)
        {
            node_t *node = root_, *where = nullptr;
            while(node != nullptr)
            {
                if(interface_t::predicate(get_key(node), key))
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

        node_t *bs_upper_bound_(key_t const &key)
        {
            node_t *node = root_, *where = nullptr;
            while(node != nullptr)
            {
                if(interface_t::predicate(key, get_key(node)))
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

        node_t *bs_right_rotate_(node_t *node)
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

        node_t *bs_left_rotate_(node_t *node)
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

        void rb_insert_(node_t *key)
        {
            if(root_ == nullptr)
            {
                root_ = bs_init_node_(nullptr, key);
                set_black_(root_, true);
                left_ = root_;
                right_ = root_;
                return;
            }
            set_black_(key, false);
            node_t *node = root_, *where = nullptr;
            bool is_left = true;
            while(node != nullptr)
            {
                where = node;
                if(is_left = predicate(key, node))
                {
                    node = get_left_(node);
                }
                else
                {
                    node = get_right_(node);
                }
            }
            if(is_left)
            {
                set_left_(where, node = bs_init_node_(where, key));
                if(where == left_)
                {
                    left_ = node;
                }
            }
            else
            {
                set_right_(where, node = bs_init_node_(where, key));
                if(where == right_)
                {
                    right_ = node;
                }
            }
            while(!is_black_(get_parent_(node)))
            {
                if(get_parent_(node) == get_left_(get_parent_(get_parent_(node))))
                {
                    where = get_right_(get_parent_(get_parent_(node)));
                    if(!is_black_(where))
                    {
                        set_black_(get_parent_(node), true);
                        set_black_(where, true);
                        set_black_(get_parent_(get_parent_(node)), false);
                        node = get_parent_(get_parent_(node));
                    }
                    else
                    {
                        if(node == get_right_(get_parent_(node)))
                        {
                            node = get_parent_(node);
                            bs_left_rotate_(node);
                        }
                        set_black_(get_parent_(node), true);
                        set_black_(get_parent_(get_parent_(node)), false);
                        bs_right_rotate_(get_parent_(get_parent_(node)));
                    }
                }
                else
                {
                    where = get_left_(get_parent_(get_parent_(node)));
                    if(!is_black_(where))
                    {
                        set_black_(get_parent_(node), true);
                        set_black_(where, true);
                        set_black_(get_parent_(get_parent_(node)), false);
                        node = get_parent_(get_parent_(node));
                    }
                    else
                    {
                        if(node == get_left_(get_parent_(node)))
                        {
                            node = get_parent_(node);
                            bs_right_rotate_(node);
                        }
                        set_black_(get_parent_(node), true);
                        set_black_(get_parent_(get_parent_(node)), false);
                        bs_left_rotate_(get_parent_(get_parent_(node)));
                    }
                }
            }
            set_black_(root_, true);
        }

        void rb_erase_(node_t *node)
        {
            node_t *erase_node = node;
            node_t *fix_node;
            node_t *fix_node_parent;

            if(get_left_(node) == nullptr)
            {
                fix_node = get_right_(node);
            }
            else if(get_right_(node) == nullptr)
            {
                fix_node = get_left_(node);
            }
            else
            {
                node = bs_next_(node);
                fix_node = get_right_(node);
            }
            if(node == erase_node)
            {
                fix_node_parent = get_parent_(erase_node);
                if(fix_node != nullptr)
                {
                    set_parent_(fix_node, fix_node_parent);
                }
                if(root_ == erase_node)
                {
                    root_ = fix_node;
                }
                else if(get_left_(fix_node_parent) == erase_node)
                {
                    set_left_(fix_node_parent, fix_node);
                }
                else
                {
                    set_right_(fix_node_parent, fix_node);
                }
                if(left_ == erase_node)
                {
                    left_ = fix_node == nullptr ? fix_node_parent : bs_min_(fix_node);
                }
                if(right_ == erase_node)
                {
                    right_ = fix_node == nullptr ? fix_node_parent : bs_max_(fix_node);
                }
            }
            else
            {
                set_parent_(get_left_(erase_node), node);
                set_left_(node, get_left_(erase_node));
                if(node == get_right_(erase_node))
                {
                    fix_node_parent = node;
                }
                else
                {
                    fix_node_parent = get_parent_(node);
                    if(fix_node != nullptr)
                    {
                        set_parent_(fix_node, fix_node_parent);
                    }
                    set_left_(fix_node_parent, fix_node);
                    set_right_(node, get_right_(erase_node));
                    set_parent_(get_right_(erase_node), node);
                }
                if(root_ == erase_node)
                {
                    root_ = node;
                }
                else if(get_left_(get_parent_(erase_node)) == erase_node)
                {
                    set_left_(get_parent_(erase_node), node);
                }
                else
                {
                    set_right_(get_parent_(erase_node), node);
                }
                set_parent_(node, get_parent_(erase_node));
                bool is_black = is_black_(node);
                set_black_(node, is_black_(erase_node));
                set_black_(erase_node, is_black);
            }
            if(is_black_(erase_node))
            {
                for(; fix_node != root_ && is_black_(fix_node); fix_node_parent = get_parent_(fix_node))
                {
                    if(fix_node == get_left_(fix_node_parent))
                    {
                        node = get_right_(fix_node_parent);
                        if(!is_black_(node))
                        {
                            set_black_(node, true);
                            set_black_(fix_node_parent, false);
                            bs_left_rotate_(fix_node_parent);
                            node = get_right_(fix_node_parent);
                        }
                        if(node == nullptr)
                        {
                            fix_node = fix_node_parent;
                        }
                        else if(is_black_(get_left_(node)) && is_black_(get_right_(node)))
                        {
                            set_black_(node, false);
                            fix_node = fix_node_parent;
                        }
                        else
                        {
                            if(is_black_(get_right_(node)))
                            {
                                set_black_(get_left_(node), true);
                                set_black_(node, false);
                                bs_right_rotate_(node);
                                node = get_right_(fix_node_parent);
                            }
                            set_black_(node, is_black_(fix_node_parent));
                            set_black_(fix_node_parent, true);
                            set_black_(get_right_(node), true);
                            bs_left_rotate_(fix_node_parent);
                            break;
                        }
                    }
                    else
                    {
                        node = get_left_(fix_node_parent);
                        if(!is_black_(node))
                        {
                            set_black_(node, true);
                            set_black_(fix_node_parent, false);
                            bs_right_rotate_(fix_node_parent);
                            node = get_left_(fix_node_parent);
                        }
                        if(node == nullptr)
                        {
                            fix_node = fix_node_parent;
                        }
                        else if(is_black_(get_right_(node)) && is_black_(get_left_(node)))
                        {
                            set_black_(node, false);
                            fix_node = fix_node_parent;
                        }
                        else
                        {
                            if(is_black_(get_left_(node)))
                            {
                                set_black_(get_right_(node), true);
                                set_black_(node, false);
                                bs_left_rotate_(node);
                                node = get_left_(fix_node_parent);
                            }
                            set_black_(node, is_black_(fix_node_parent));
                            set_black_(fix_node_parent, true);
                            set_black_(get_left_(node), true);
                            bs_right_rotate_(fix_node_parent);
                            break;
                        }
                    }
                }
                if(fix_node != nullptr)
                {
                    set_black_(fix_node, true);
                }
            }
        }

        node_t *sb_left_rotate_(node_t *node)
        {
            node_t *left = bs_left_rotate_(node);
            set_size_(left, get_size_(node));
            set_size_(node, get_size_(get_left_(node)) + get_size_(get_right_(node)) + 1);
            return left;
        }

        node_t *sb_right_rotate_(node_t *node)
        {
            node_t *right = bs_right_rotate_(node);
            set_size_(right, get_size_(node));
            set_size_(node, get_size_(get_right_(node)) + get_size_(get_left_(node)) + 1);
            return right;
        }

        void sb_insert_(node_t *key)
        {
            set_size_(key, 1);
            if(root_ == nullptr)
            {
                root_ = bs_init_node_(nullptr, key);
                left_ = root_;
                right_ = root_;
                return;
            }
            node_t *node = root_, *where = nullptr;
            bool is_left = true;
            while(node != nullptr)
            {
                set_size_(node, get_size_(node) + 1);
                where = node;
                if(is_left = predicate(key, node))
                {
                    node = get_left_(node);
                }
                else
                {
                    node = get_right_(node);
                }
            }
            if(is_left)
            {
                set_left_(where, node = bs_init_node_(where, key));
                if(where == left_)
                {
                    left_ = node;
                }
            }
            else
            {
                set_right_(where, node = bs_init_node_(where, key));
                if(where == right_)
                {
                    right_ = node;
                }
            }
            do
            {
                sb_maintain_(where, is_left);
            } while((where = get_parent_(where)) != nullptr);
        }

        void sb_maintain_(node_t *node, bool is_left)
        {
            if(node == nullptr)
            {
                return;
            }
            if(is_left)
            {
                if(get_left_(node) == nullptr)
                {
                    return;
                }
                if(get_size_(get_left_(get_left_(node))) > get_size_(get_right_(node)))
                {
                    node = sb_right_rotate_(node);
                }
                else
                {
                    if(get_size_(get_right_(get_left_(node))) > get_size_(get_right_(node)))
                    {
                        sb_left_rotate_(get_left_(node));
                        node = sb_right_rotate_(node);
                    }
                    else
                    {
                        return;
                    };
                };
            }
            else
            {
                if(get_right_(node) == nullptr)
                {
                    return;
                }
                if(get_size_(get_right_(get_right_(node))) > get_size_(get_left_(node)))
                {
                    node = sb_left_rotate_(node);
                }
                else
                {
                    if(get_size_(get_left_(get_right_(node))) > get_size_(get_left_(node)))
                    {
                        sb_right_rotate_(get_right_(node));
                        node = sb_left_rotate_(node);
                    }
                    else
                    {
                        return;
                    };
                };
            };
            sb_maintain_(get_left_(node), true);
            sb_maintain_(get_right_(node), false);
            sb_maintain_(node, true);
            sb_maintain_(node, false);
        }

        void sb_erase_(node_t *node)
        {
            node_t *erase_node = node;
            node_t *fix_node = node;
            node_t *fix_node_parent;
            while((fix_node = get_parent_(fix_node)) != nullptr)
            {
                set_size_(fix_node, get_size_(fix_node) - 1);
            }

            if(get_left_(node) == nullptr)
            {
                fix_node = get_right_(node);
            }
            else if(get_right_(node) == nullptr)
            {
                fix_node = get_left_(node);
            }
            else
            {
                if(get_size_(get_left_(node)) > get_size_(get_right_(node)))
                {
                    node = bs_prev_(node);
                    fix_node = get_left_(node);
                    fix_node_parent = node;
                    while((fix_node_parent = get_parent_(fix_node_parent)) != erase_node)
                    {
                        set_size_(fix_node_parent, get_size_(fix_node_parent) - 1);
                    }
                    set_parent_(get_right_(erase_node), node);
                    set_right_(node, get_right_(erase_node));
                    if(node == get_left_(erase_node))
                    {
                        fix_node_parent = node;
                    }
                    else
                    {
                        fix_node_parent = get_parent_(node);
                        if(fix_node != nullptr)
                        {
                            set_parent_(fix_node, fix_node_parent);
                        }
                        set_right_(fix_node_parent, fix_node);
                        set_left_(node, get_left_(erase_node));
                        set_parent_(get_left_(erase_node), node);
                    }
                    if(root_ == erase_node)
                    {
                        root_ = node;
                    }
                    else if(get_right_(get_parent_(erase_node)) == erase_node)
                    {
                        set_right_(get_parent_(erase_node), node);
                    }
                    else
                    {
                        set_left_(get_parent_(erase_node), node);
                    }
                    set_parent_(node, get_parent_(erase_node));
                    set_size_(node, get_size_(get_left_(node)) + get_size_(get_right_(node)) + 1);
                }
                else
                {
                    node = bs_next_(node);
                    fix_node = get_right_(node);
                    fix_node_parent = node;
                    while((fix_node_parent = get_parent_(fix_node_parent)) != erase_node)
                    {
                        set_size_(fix_node_parent, get_size_(fix_node_parent) - 1);
                    }
                    set_parent_(get_left_(erase_node), node);
                    set_left_(node, get_left_(erase_node));
                    if(node == get_right_(erase_node))
                    {
                        fix_node_parent = node;
                    }
                    else
                    {
                        fix_node_parent = get_parent_(node);
                        if(fix_node != nullptr)
                        {
                            set_parent_(fix_node, fix_node_parent);
                        }
                        set_left_(fix_node_parent, fix_node);
                        set_right_(node, get_right_(erase_node));
                        set_parent_(get_right_(erase_node), node);
                    }
                    if(root_ == erase_node)
                    {
                        root_ = node;
                    }
                    else if(get_left_(get_parent_(erase_node)) == erase_node)
                    {
                        set_left_(get_parent_(erase_node), node);
                    }
                    else
                    {
                        set_right_(get_parent_(erase_node), node);
                    }
                    set_parent_(node, get_parent_(erase_node));
                    set_size_(node, get_size_(get_right_(node)) + get_size_(get_left_(node)) + 1);
                }
                return;
            }
            fix_node_parent = get_parent_(erase_node);
            if(fix_node != nullptr)
            {
                set_parent_(fix_node, fix_node_parent);
            }
            if(root_ == erase_node)
            {
                root_ = fix_node;
            }
            else if(get_left_(fix_node_parent) == erase_node)
            {
                set_left_(fix_node_parent, fix_node);
            }
            else
            {
                set_right_(fix_node_parent, fix_node);
            }
            if(left_ == erase_node)
            {
                left_ = fix_node == nullptr ? fix_node_parent : bs_min_(fix_node);
            }
            if(right_ == erase_node)
            {
                right_ = fix_node == nullptr ? fix_node_parent : bs_max_(fix_node);
            }
        }
    };

}