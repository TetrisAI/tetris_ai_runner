
#pragma once

#include "bs_tree.h"

namespace zzz
{
    //incomplete
    template<class Interface>
    class sb_tree : public bs_tree<Interface>
    {
    public:
        class iterator
        {
        public:
            typedef std::random_access_iterator_tag iterator_category;
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
            iterator &operator += (difference_type diff)
            {
                ptr_ = sb_tree::advance(ptr_, diff);
                return *this;
            }
            iterator &operator -= (difference_type diff)
            {
                ptr_ = sb_tree::advance(ptr_, -diff);
                return *this;
            }
            iterator operator + (difference_type diff)
            {
                return iterator(sb_tree::advance(ptr_, diff));
            }
            iterator operator - (difference_type diff)
            {
                return iterator(sb_tree::advance(ptr_, -diff));
            }
            difference_type operator - (iterator const &other)
            {
                return static_cast<int>(sb_tree::rank(ptr_)) - static_cast<int>(sb_tree::rank(other.ptr_));
            }
            iterator &operator++()
            {
                ptr_ = sb_tree::bst_next_(ptr_);
                return *this;
            }
            iterator &operator--()
            {
                ptr_ = sb_tree::bst_prev_(ptr_);
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
            node_t &operator *()
            {
                return *static_cast<value_node_t *>(ptr_);
            }
            node_t *operator->()
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
        sb_tree() : bs_tree()
        {
            set_size_(get_root_(), 0);
        }
        sb_tree(sb_tree &&other)
        {
            set_root_(other.get_root_());
            set_most_left_(other.get_most_left_());
            set_most_right_(other.get_most_right_());
            other.set_root_(other.nil_());
            other.set_most_left_(other.nil_());
            other.set_most_right_(other.nil_());
        }
        sb_tree(sb_tree const &other) = delete;
        sb_tree &operator = (sb_tree const &other) = delete;

        void insert(node_t *node)
        {
            sbt_insert_(node);
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
            return (is_nil_(where) || interface_t::predicate(key, get_key(where))) ? iterator() : iterator(where);
        }
        void erase(iterator where)
        {
            sbt_erase_(&*where);
        }
        void erase(node_t *node)
        {
            sbt_erase_(node);
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
        }
        size_t size()
        {
            return get_size_(get_root_());
        }
        node_t *at(size_t index)
        {
            if(index >= size())
            {
                return nullptr;
            }
            node_t *node = get_root_();
            size_t rank = get_size_(get_left_(node));
            while(index != rank)
            {
                if(index < rank)
                {
                    node = get_left_(node);
                }
                else
                {
                    index -= rank + 1;
                    node = get_right_(node);
                }
                rank = get_size_(get_left_(node));
            }
            return node;
        }
        static size_t rank(node_t *node)
        {
            if(is_nil_(node))
            {
                return get_size_(get_parent_(node));
            }
            size_t rank = get_size_(get_left_(node));
            node_t *parent = get_parent_(node);
            while(!is_nil_(parent))
            {
                if(node == get_right_(parent))
                {
                    rank += get_size_(get_left_(parent)) + 1;
                }
                node = parent;
                parent = get_parent_(node);
            }
            return rank;
        }
        static node_t *advance(node_t *node, int step)
        {
            if(is_nil_(node))
            {
                if(step == 0)
                {
                    return node;
                }
                else if(step > 0)
                {
                    --step;
                    node = get_left_(node);
                }
                else
                {
                    ++step;
                    node = get_right_(node);
                }
                if(is_nil_(node))
                {
                    return node;
                }
            }
            size_t u_step;
            while(step != 0)
            {
                if(step > 0)
                {
                    u_step = step;
                    if(get_size_(get_right_(node)) >= u_step)
                    {
                        step -= get_size_(get_left_(get_right_(node))) + 1;
                        node = get_right_(node);
                        continue;
                    }
                }
                else
                {
                    u_step = -step;
                    if(get_size_(get_left_(node)) >= u_step)
                    {
                        step += get_size_(get_right_(get_left_(node))) + 1;
                        node = get_left_(node);
                        continue;
                    }
                }
                if(is_nil_(get_parent_(node)))
                {
                    return nullptr;
                }
                else
                {
                    if(get_right_(get_parent_(node)) == node)
                    {
                        step += get_size_(get_left_(node)) + 1;
                        node = get_parent_(node);
                    }
                    else
                    {
                        step -= get_size_(get_right_(node)) + 1;
                        node = get_parent_(node);
                    }
                }
            }
            return node;
        }

    protected:
        static size_t get_size_(node_t *node)
        {
            return interface_t::get_size(node);
        }

        static void set_size_(node_t *node, size_t size)
        {
            interface_t::set_size(node, size);
        }

        void sbt_refresh_size_(node_t *node)
        {
            set_size_(node, get_size_(get_left_(node)) + get_size_(get_right_(node)) + 1);
        }

        node_t *sbt_left_rotate_(node_t *node)
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
            set_size_(right, get_size_(node));
            sbt_refresh_size_(node);
            return right;
        }

        node_t *sbt_right_rotate_(node_t *node)
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
            set_size_(left, get_size_(node));
            sbt_refresh_size_(node);
            return left;
        }

        void sbt_insert_(node_t *key)
        {
            set_size_(key, 1);
            if(is_nil_(get_root_()))
            {
                set_root_(bst_init_node_(nil_(), key));
                set_most_left_(get_root_());
                set_most_right_(get_root_());
                return;
            }
            node_t *node = get_root_(), *where = nil_();
            bool is_left = true;
            while(!is_nil_(node))
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
            while(!is_nil_(where))
            {
                if(node == get_left_(where))
                {
                    where = sbt_maintain_<true>(where);
                }
                else
                {
                    where = sbt_maintain_<false>(where);
                }
                node = where;
                where = get_parent_(where);
            }
        }

        template<bool is_left>
        node_t *sbt_maintain_(node_t *node)
        {
            if(is_left)
            {
                if(is_nil_(get_left_(node)))
                {
                    return node;
                }
                if(get_size_(get_left_(get_left_(node))) > get_size_(get_right_(node)))
                {
                    node = sbt_right_rotate_(node);
                }
                else
                {
                    if(get_size_(get_right_(get_left_(node))) > get_size_(get_right_(node)))
                    {
                        sbt_left_rotate_(get_left_(node));
                        node = sbt_right_rotate_(node);
                    }
                    else
                    {
                        return node;
                    };
                };
            }
            else
            {
                if(is_nil_(get_right_(node)))
                {
                    return node;
                }
                if(get_size_(get_right_(get_right_(node))) > get_size_(get_left_(node)))
                {
                    node = sbt_left_rotate_(node);
                }
                else
                {
                    if(get_size_(get_left_(get_right_(node))) > get_size_(get_left_(node)))
                    {
                        sbt_right_rotate_(get_right_(node));
                        node = sbt_left_rotate_(node);
                    }
                    else
                    {
                        return node;
                    };
                };
            };
            if(!is_nil_(get_left_(node)))
            {
                sbt_maintain_<true>(get_left_(node));
            }
            if(!is_nil_(get_right_(node)))
            {
                sbt_maintain_<false>(get_right_(node));
            }
            node = sbt_maintain_<true>(node);
            node = sbt_maintain_<false>(node);
            return node;
        }

        void sbt_erase_(node_t *node)
        {
            node_t *erase_node = node;
            node_t *fix_node = node;
            node_t *fix_node_parent;
            while(!is_nil_((fix_node = get_parent_(fix_node))))
            {
                set_size_(fix_node, get_size_(fix_node) - 1);
            }

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
                if(get_size_(get_left_(node)) > get_size_(get_right_(node)))
                {
                    node = bst_prev_(node);
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
                        if(!is_nil_(fix_node))
                        {
                            set_parent_(fix_node, fix_node_parent);
                        }
                        set_right_(fix_node_parent, fix_node);
                        set_left_(node, get_left_(erase_node));
                        set_parent_(get_left_(erase_node), node);
                    }
                    if(get_root_() == erase_node)
                    {
                        set_root_(node);
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
                    sbt_refresh_size_(node);
                }
                else
                {
                    node = bst_next_(node);
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
                    sbt_refresh_size_(node);
                }
                return;
            }
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

    public:
        void print_tree()
        {
            printf("\n\n\n\n\n");
            print_tree(get_root_(), 0, "  ", "", 0);
        }

    protected:
        void print_tree(node_t *node, size_t level, std::string head, std::string with, int type)
        {
            if(!is_nil_(node))
            {
                if(get_size_(node) != get_size_(get_left_(node)) + get_size_(get_right_(node)) + 1)
                {
                    _asm int 3;
                }
                std::string fork =
                    !is_nil_(get_left_(node)) && !is_nil_(get_right_(node)) ? "©Ï" :
                    is_nil_(get_left_(node)) && is_nil_(get_right_(node)) ? "* " :
                    !is_nil_(get_right_(node)) ? "©¿" : "©·";
                std::string next_left = type == 0 ? "" : type == 1 ? "©§" : "  ";
                std::string next_right = type == 0 ? "" : type == 1 ? "  " : "©§";
                print_tree(get_right_(node), level + 1, head + next_right, "©³", 1);
                printf("%s%d\n", (head + with + fork).c_str(), rank(node));
                print_tree(get_left_(node), level + 1, head + next_left, "©»", 2);
            }
        }
    };

}