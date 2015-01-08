
#pragma once

#include "bs_tree.h"

namespace zzz
{
    //incomplete
    template<class Interface>
    class rb_tree : public bs_tree<Interface>
    {
    public:
        class iterator
        {
        public:
            typedef std::bidirectional_iterator_tag iterator_category;
            typedef node_t value_type;
            typedef int difference_type;
            typedef unsigned int distance_type;
            typedef node_t *pointer;
            typedef node_t reference;
        public:
            iterator(node_t *node) : ptr_(node)
            {
            }
            iterator(iterator const &other) : ptr_(other.ptr_)
            {
            }
            iterator &operator++()
            {
                ptr_ = rb_tree::bst_next_(ptr_);
                return *this;
            }
            iterator &operator--()
            {
                ptr_ = rb_tree::bst_prev_(ptr_);
                return *this;
            }
            iterator operator++(int)
            {
                iterator save(*this);
                ++*this;
                return save;
            }
            iterator operator--(int)
            {
                iterator save(*this);
                --*this;
                return save;
            }
            value_node_t &operator *()
            {
                return *static_cast<value_node_t *>(ptr_);
            }
            value_node_t *operator->()
            {
                return *static_cast<value_node_t *>(ptr_);
            }
            bool operator == (iterator const &other) const
            {
                return ptr_ == other.ptr_;
            }
            bool operator != (iterator const &other) const
            {
                return ptr_ != other.ptr_;
            }
        private:
            node_t *ptr_;
        };

    public:
        rb_tree() : bs_tree(), size_()
        {
            set_black_(get_root_(), true);
        }
        rb_tree(rb_tree &&other)
        {
            set_root_(other.get_root_());
            set_most_left_(other.get_most_left_());
            set_most_right_(other.get_most_right_());
            size_ = other.size_;
            other.set_root_(other.nil_());
            other.set_most_left_(other.nil_());
            other.set_most_right_(other.nil_());
            other.size_ = 0;
        }
        rb_tree(rb_tree const &other) = delete;
        rb_tree &operator = (rb_tree const &other) = delete;

        void insert(node_t *node)
        {
            rbt_insert_(node);
            ++size_;
        }
        template<class iterator_t>
        void insert(iterator_t begin, iterator_t end)
        {
            for(; begin != end; ++begin)
            {
                insert(*begin);
            }
        }
        iterator find(key_t const &key)
        {
            node_t *where = bst_lower_bound_(key);
            return (is_nil_(where) || interface_t::predicate(key, get_key_(where))) ? iterator(nil_()) : iterator(where);
        }
        void erase(iterator where)
        {
            rbt_erase_(&*where);
            --size_;
        }
        void erase(node_t *node)
        {
            rbt_erase_(node);
            --size_;
        }
        iterator begin()
        {
            return iterator(get_most_left_());
        }
        iterator end()
        {
            return iterator(nil_());
        }
        bool empty()
        {
            return is_nil_(get_root_());
        }
        void clear()
        {
            bst_clear_();
            size_ = 0;
        }
        size_t size() const
        {
            return size_;
        }

    protected:
        size_t size_;

    protected:
        static bool is_black_(node_t *node)
        {
            return interface_t::is_black(node);
        }

        static void set_black_(node_t *node, bool black)
        {
            interface_t::set_black(node, black);
        }

        void rbt_insert_(node_t *key)
        {
            if(is_nil_(get_root_()))
            {
                set_root_(bst_init_node_(nil_(), key));
                set_black_(get_root_(), true);
                set_most_left_(get_root_());
                set_most_right_(get_root_());
                return;
            }
            set_black_(key, false);
            node_t *node = get_root_(), *where = nil_();
            bool is_left = true;
            while(!is_nil_(node))
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
                set_left_(where, node = bst_init_node_(where, key));
                if(where == get_most_left_())
                {
                    set_most_left_(node);
                }
            }
            else
            {
                set_right_(where, node = bst_init_node_(where, key));
                if(where == get_most_right_())
                {
                    set_most_right_(node);
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
                            bst_left_rotate_(node);
                        }
                        set_black_(get_parent_(node), true);
                        set_black_(get_parent_(get_parent_(node)), false);
                        bst_right_rotate_(get_parent_(get_parent_(node)));
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
                            bst_right_rotate_(node);
                        }
                        set_black_(get_parent_(node), true);
                        set_black_(get_parent_(get_parent_(node)), false);
                        bst_left_rotate_(get_parent_(get_parent_(node)));
                    }
                }
            }
            set_black_(get_root_(), true);
        }

        void rbt_erase_(node_t *node)
        {
            node_t *erase_node = node;
            node_t *fix_node;
            node_t *fix_node_parent;
            if(is_nil_(get_left_(node)))
            {
                fix_node = get_right_(node);
            }
            else if(is_nil_(get_right_(node)))
            {
                fix_node = get_left_(node);
            }
            else
            {
                node = bst_next_(node);
                fix_node = get_right_(node);
            }
            if(node == erase_node)
            {
                fix_node_parent = get_parent_(erase_node);
                if(!is_nil_(fix_node))
                {
                    set_parent_(fix_node, fix_node_parent);
                }
                if(get_root_() == erase_node)
                {
                    set_root_(fix_node);
                }
                else if(get_left_(fix_node_parent) == erase_node)
                {
                    set_left_(fix_node_parent, fix_node);
                }
                else
                {
                    set_right_(fix_node_parent, fix_node);
                }
                if(get_most_left_() == erase_node)
                {
                    set_most_left_(is_nil_(fix_node) ? fix_node_parent : bst_min_(fix_node));
                }
                if(get_most_right_() == erase_node)
                {
                    set_most_right_(is_nil_(fix_node) ? fix_node_parent : bst_max_(fix_node));
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
                    if(!is_nil_(fix_node))
                    {
                        set_parent_(fix_node, fix_node_parent);
                    }
                    set_left_(fix_node_parent, fix_node);
                    set_right_(node, get_right_(erase_node));
                    set_parent_(get_right_(erase_node), node);
                }
                if(get_root_() == erase_node)
                {
                    set_root_(node);
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
                for(; fix_node != get_root_() && is_black_(fix_node); fix_node_parent = get_parent_(fix_node))
                {
                    if(fix_node == get_left_(fix_node_parent))
                    {
                        node = get_right_(fix_node_parent);
                        if(!is_black_(node))
                        {
                            set_black_(node, true);
                            set_black_(fix_node_parent, false);
                            bst_left_rotate_(fix_node_parent);
                            node = get_right_(fix_node_parent);
                        }
                        if(is_nil_(node))
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
                                bst_right_rotate_(node);
                                node = get_right_(fix_node_parent);
                            }
                            set_black_(node, is_black_(fix_node_parent));
                            set_black_(fix_node_parent, true);
                            set_black_(get_right_(node), true);
                            bst_left_rotate_(fix_node_parent);
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
                            bst_right_rotate_(fix_node_parent);
                            node = get_left_(fix_node_parent);
                        }
                        if(is_nil_(node))
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
                                bst_left_rotate_(node);
                                node = get_left_(fix_node_parent);
                            }
                            set_black_(node, is_black_(fix_node_parent));
                            set_black_(fix_node_parent, true);
                            set_black_(get_left_(node), true);
                            bst_right_rotate_(fix_node_parent);
                            break;
                        }
                    }
                }
                if(!is_nil_(fix_node))
                {
                    set_black_(fix_node, true);
                }
            }
        }

    };

}