
#pragma once

#include "bs_tree.h"

namespace zzz
{
    //incomplete
    template<class Node, class Interface>
    class sb_tree : public bs_tree<Node, Interface>
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
            iterator() : ptr_(nullptr)
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
                return *ptr_;
            }
            node_t *operator->()
            {
                return ptr_;
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
        }
        sb_tree(sb_tree &&other)
        {
            root_ = other.root_;
            left_ = other.left_;
            right_ = other.right_;
            other.root_ = nullptr;
            other.left_ = nullptr;
            other.right_ = nullptr;
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
            return (where == nullptr || interface_t::predicate(key, get_key(where))) ? iterator() : iterator(where);
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
            return iterator(left_);
        }
        iterator end()
        {
            return iterator();
        }
        bool empty() const
        {
            return root_ == nullptr;
        }
        void clear()
        {
            bst_clear_();
        }
        size_t size() const
        {
            return get_size_(root_);
        }
        node_t *at(size_t index)
        {
            if(index >= size())
            {
                return nullptr;
            }
            node_t *node = root_;
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
            size_t rank = get_size_(get_left_(node));
            node_t *parent = get_parent_(node);
            while(parent != nullptr)
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
                if(get_parent_(node) == nullptr)
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
            return node == nullptr ? 0 : interface_t::get_size(node);
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
            set_size_(right, get_size_(node));
            sbt_refresh_size_(node);
            return right;
        }

        node_t *sbt_right_rotate_(node_t *node)
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
            set_size_(left, get_size_(node));
            sbt_refresh_size_(node);
            return left;
        }

        void sbt_insert_(node_t *key)
        {
            set_size_(key, 1);
            if(root_ == nullptr)
            {
                root_ = bst_init_node_(nullptr, key);
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
                set_left_(where, node = bst_init_node_(where, key));
                if(where == left_)
                {
                    left_ = node;
                }
            }
            else
            {
                set_right_(where, node = bst_init_node_(where, key));
                if(where == right_)
                {
                    right_ = node;
                }
            }
            //where = get_parent_(where);
            while(where != nullptr)
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
                if(get_left_(node) == nullptr)
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
                if(get_right_(node) == nullptr)
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
            if(get_left_(node) != nullptr)
            {
                sbt_maintain_<true>(get_left_(node));
            }
            if(get_right_(node) != nullptr)
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
                    sbt_refresh_size_(node);
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
                left_ = fix_node == nullptr ? fix_node_parent : bst_min_(fix_node);
            }
            if(right_ == erase_node)
            {
                right_ = fix_node == nullptr ? fix_node_parent : bst_max_(fix_node);
            }
        }

    public:
        void print_tree()
        {
            printf("\n\n\n\n\n");
            print_tree(root_, 0, "  ", "", 0);
        }

    protected:
        void print_tree(node_t *node, size_t level, std::string head, std::string with, int type)
        {
            if(node != nullptr)
            {
                std::string fork =
                    get_left_(node) != nullptr && get_right_(node) != nullptr ? "©Ï" :
                    get_left_(node) == nullptr && get_right_(node) == nullptr ? "* " :
                    get_right_(node) != nullptr ? "©¿" : "©·";
                std::string next_left = type == 0 ? "" : type == 1 ? "©§" : "  ";
                std::string next_right = type == 0 ? "" : type == 1 ? "  " : "©§";
                print_tree(get_right_(node), level + 1, head + next_right, "©³", 1);
                printf("%s%d\n", (head + with + fork).c_str(), rank(node));
                print_tree(get_left_(node), level + 1, head + next_left, "©»", 2);
            }
        }
    };

}