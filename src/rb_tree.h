
#pragma once

#include "base_tree.h"

namespace zzz
{
    template<class Node, class Interface>
    class rb_tree : public base_tree<Node, Interface>
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
            iterator() : ptr_(nullptr)
            {
            }
            iterator &operator++()
            {
                ptr_ = rb_tree::bs_next_(ptr_);
                return *this;
            }
            iterator &operator--()
            {
                ptr_ = rb_tree::bs_prev_(ptr_);
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
        rb_tree() : base_tree(), size_()
        {
        }
        rb_tree(rb_tree &&other)
        {
            root_ = other.root_;
            left_ = other.left_;
            right_ = other.right_;
            size_ = other.size_;
            other.root_ = nullptr;
            other.left_ = nullptr;
            other.right_ = nullptr;
            other.size_ = 0;
        }
        rb_tree(rb_tree const &other) = delete;
        rb_tree &operator = (rb_tree const &other) = delete;

        void insert(node_t *node)
        {
            rb_insert_(node);
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
            node_t *where = bs_lower_bound_(key);
            return (where == nullptr || interface_t::predicate(key, get_key(where))) ? iterator() : iterator(where);
        }
        void erase(iterator where)
        {
            rb_erase_(&*where);
            --size_;
        }
        void erase(node_t *node)
        {
            rb_erase_(node);
            --size_;
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
            size_ = 0;
        }
        size_t size() const
        {
            return size_;
        }

    protected:
        size_t size_;
    };

}