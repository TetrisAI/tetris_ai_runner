
#pragma once

#include "base_tree.h"

namespace zzz
{
    template<class Node, class Interface>
    class sb_tree : public base_tree<Node, Interface>
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
                ptr_ = sb_tree::bs_next_(ptr_);
                return *this;
            }
            iterator &operator--()
            {
                ptr_ = sb_tree::bs_prev_(ptr_);
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
        sb_tree() : base_tree()
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
            sb_insert_(node);
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
            node_t *where = bs_lower_bound_(key);
            return (where == nullptr || interface_t::predicate(key, get_key(where))) ? iterator() : iterator(where);
        }
        void erase(iterator where)
        {
            sb_erase_(&*where);
        }
        void erase(node_t *node)
        {
            sb_erase_(node);
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
            bs_clear_();
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
    };

}