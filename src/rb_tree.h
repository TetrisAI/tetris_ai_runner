
#pragma once

#include <iterator>

namespace zzz
{
    //just for tetris_core ... incomplete !
    template<class Node, class Interface>
    class rb_tree
    {
    public:
        typedef Node node_t;
        typedef Interface interface_t;
        typedef typename interface_t::key_t key_t;
    public:
        class iterator
        {
        public:
            typedef std::forward_iterator_tag iterator_category;
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
                ptr_ = rb_tree::rb_next_(ptr_);
                return *this;
            }
            iterator &operator--()
            {
                ptr_ = rb_tree::rb_prev_(ptr_);
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
        rb_tree() : root_(), left_(), size_()
        {
        }
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
            node_t *where = rb_find(key);
            return (where == nullptr || interface_t::predicate(key, get_key(where))) ? iterator() : iterator(where);
        }
        void erase(iterator where)
        {
            rb_erase_(&*where);
        }
        void erase(node_t *node)
        {
            rb_erase_(node);
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
            root_ = nullptr;
            left_ = nullptr;
            size_ = 0;
        }
        size_t size() const
        {
            return size_;
        }

    private:
        node_t *root_;
        node_t *left_;
        size_t size_;

    private:
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

        node_t *rb_init_node_(node_t *parent, node_t *node)
        {
            set_black_(node, false);
            set_parent_(node, parent);
            set_left_(node, nullptr);
            set_right_(node, nullptr);
            return node;
        }

        static node_t *rb_next_(node_t *node)
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

        static node_t *rb_prev_(node_t *node)
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

        static node_t *rb_min_(node_t *node)
        {
            while(get_left_(node) != nullptr)
            {
                node = get_left_(node);
            }
            return node;
        }

        static node_t *rb_max_(node_t *node)
        {
            while(get_right_(node) != nullptr)
            {
                node = get_right_(node);
            }
            return node;
        }

        node_t *rb_find(key_t const &key)
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

        void rb_right_rotate_(node_t *node)
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
        }

        void rb_left_rotate_(node_t *node)
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
        }

        void rb_insert_(node_t *key)
        {
            if(root_ == nullptr)
            {
                root_ = rb_init_node_(nullptr, key);
                set_black_(root_, true);
                left_ = root_;
                return;
            }
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
                set_left_(where, node = rb_init_node_(where, key));
                if(where == left_)
                {
                    left_ = node;
                }
            }
            else
            {
                set_right_(where, node = rb_init_node_(where, key));
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
                            rb_left_rotate_(node);
                        }
                        set_black_(get_parent_(node), true);
                        set_black_(get_parent_(get_parent_(node)), false);
                        rb_right_rotate_(get_parent_(get_parent_(node)));
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
                            rb_right_rotate_(node);
                        }
                        set_black_(get_parent_(node), true);
                        set_black_(get_parent_(get_parent_(node)), false);
                        rb_left_rotate_(get_parent_(get_parent_(node)));
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
                node = rb_next_(node);
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
                    left_ = fix_node == nullptr ? fix_node_parent : rb_min_(fix_node);
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
                            rb_left_rotate_(fix_node_parent);
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
                                rb_right_rotate_(node);
                                node = get_right_(fix_node_parent);
                            }
                            set_black_(node, is_black_(fix_node_parent));
                            set_black_(fix_node_parent, true);
                            set_black_(get_right_(node), true);
                            rb_left_rotate_(fix_node_parent);
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
                            rb_right_rotate_(fix_node_parent);
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
                                rb_left_rotate_(node);
                                node = get_left_(fix_node_parent);
                            }
                            set_black_(node, is_black_(fix_node_parent));
                            set_black_(fix_node_parent, true);
                            set_black_(get_left_(node), true);
                            rb_right_rotate_(fix_node_parent);
                            break;
                        }
                    }
                }
                if(fix_node != nullptr)
                {
                    set_black_(fix_node, true);
                }
            }
            --size_;
        }
    };

}