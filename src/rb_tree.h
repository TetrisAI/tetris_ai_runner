
#pragma once

namespace zzz
{
    //just for tetris_core ... incomplete !
    template<class Node, class Interface>
    class rb_tree
    {
    public:
        class iterator
        {
        public:
            iterator(Node *node) : ptr_(node)
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
            Node &operator *()
            {
                return *ptr_;
            }
            Node *operator->()
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
            Node *ptr_;
        };
    public:
        rb_tree() : root_(), left_(), size_()
        {
        }
        void insert(Node *node)
        {
            rb_insert_(node);
            ++size_;
        }
        template<class iterator>
        void insert(iterator begin, iterator end)
        {
            for(; begin != end; ++begin)
            {
                insert(*begin);
            }
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
        Node *root_;
        Node *left_;
        size_t size_;

    private:
        static Node *get_parent_(Node *node)
        {
            return Interface::get_parent(node);
        }

        static void set_parent_(Node *node, Node *parent)
        {
            Interface::set_parent(node, parent);
        }

        static Node *get_left_(Node *node)
        {
            return Interface::get_left(node);
        }

        static void set_left_(Node *node, Node *left)
        {
            Interface::set_left(node, left);
        }

        static Node *get_right_(Node *node)
        {
            return Interface::get_right(node);
        }

        static void set_right_(Node *node, Node *right)
        {
            Interface::set_right(node, right);
        }

        static int is_black_(Node *node)
        {
            return node == nullptr ? true : Interface::is_black(node);
        }

        static void set_black_(Node *node, bool black)
        {
            Interface::set_black(node, black);
        }

        static bool predicate(Node *left, Node *right)
        {
            return Interface::predicate(left, right);
        }

        Node *rb_init_node_(Node *parent, Node *node)
        {
            set_black_(node, false);
            set_parent_(node, parent);
            set_left_(node, nullptr);
            set_right_(node, nullptr);
            return node;
        }

        static Node *rb_next_(Node *node)
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
                    Node *parent;
                    while((parent = get_parent_(node)) != nullptr && node == get_right_(parent))
                    {
                        node = parent;
                    }
                    node = parent;
                }
            }
            return node;
        }

        static Node *rb_prev_(Node *node)
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
                    Node *parent;
                    while((parent = get_parent_(node)) != nullptr && node == get_left_(parent))
                    {
                        node = parent;
                    }
                    node = parent;
                }
            }
            return node;
        }

        void rb_right_rotate_(Node *node)
        {
            Node *left = get_left_(node), *parent = get_parent_(node);
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

        void rb_left_rotate_(Node *node)
        {
            Node *right = get_right_(node), *parent = get_parent_(node);
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

        void rb_insert_(Node *key)
        {
            if(root_ == nullptr)
            {
                root_ = rb_init_node_(nullptr, key);
                set_black_(root_, true);
                left_ = root_;
                return;
            }
            Node *node = root_, *where = nullptr;
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

    };

}