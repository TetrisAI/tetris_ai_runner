
#pragma once

#ifndef NDEBUG
#   include <cassert>
#   include <set>
#endif

namespace zzz
{
    enum
    {
        rb_red = 0, rb_black = 1
    };

    //just for tetris_core ... incomplete !
    template<class Node, class Wrapper>
    class rb_tree
    {
    public:
        class Iterator
        {
        public:
            Iterator(Node *node) : ptr_(node)
            {
            }
            Iterator() : ptr_(nullptr)
            {
            }
            Iterator &operator++()
            {
                if(ptr_ != nullptr)
                {
                    if(Wrapper::GetRight(ptr_) != nullptr)
                    {
                        ptr_ = Wrapper::GetRight(ptr_);
                        while(Wrapper::GetLeft(ptr_))
                        {
                            ptr_ = Wrapper::GetLeft(ptr_);
                        }
                    }
                    else
                    {
                        Node *parent;
                        while((parent = Wrapper::GetParent(ptr_)) != nullptr && ptr_ == Wrapper::GetRight(parent))
                        {
                            ptr_ = parent;
                        }
                        ptr_ = parent;
                    }
                }
                return *this;
            }
            Node &operator *()
            {
                return *ptr_;
            }
            Node *operator->()
            {
                return ptr_;
            }
            bool operator == (Iterator const &other) const
            {
                return ptr_ == other.ptr_;
            }
            bool operator != (Iterator const &other) const
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
#ifndef NDEBUG
            assert(debug_set_.find(node) == debug_set_.end());
            debug_set_.insert(node);
#endif
        }
        template<class Iterator>
        void insert(Iterator begin, Iterator end)
        {
            for(; begin != end; ++begin)
            {
                insert(*begin);
            }
        }
        Iterator begin()
        {
            return Iterator(left_);
        }
        Iterator end()
        {
            return Iterator();
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
#ifndef NDEBUG
            debug_set_.clear();
#endif
        }
        size_t size() const
        {
            return size_;
        }

    private:
        Node *root_;
        Node *left_;
        size_t size_;
#ifndef NDEBUG
        std::multiset<Node *> debug_set_;
#endif

    private:
        static Node *GetParent(Node *node)
        {
            return Wrapper::GetParent(node);
        }
        static void SetParent(Node *node, Node *parent)
        {
            Wrapper::SetParent(node, parent);
        }
        static Node *GetLeft(Node *node)
        {
            return Wrapper::GetLeft(node);
        }
        static void SetLeft(Node *node, Node *left)
        {
            Wrapper::SetLeft(node, left);
        }
        static Node *GetRight(Node *node)
        {
            return Wrapper::GetRight(node);
        }
        static void SetRight(Node *node, Node *right)
        {
            Wrapper::SetRight(node, right);
        }
        static int GetColor(Node *node)
        {
            return node == nullptr ? zzz::rb_black : Wrapper::GetColor(node);
        }
        static void SetColor(Node *node, int color)
        {
            Wrapper::SetColor(node, color);
        }

        Node *rb_init_node_(Node *parent, Node *node)
        {
            Wrapper::SetColor(node, rb_red);
            Wrapper::SetParent(node, parent);
            Wrapper::SetLeft(node, nullptr);
            Wrapper::SetRight(node, nullptr);
            return node;
        }

        void rb_right_rotate_(Node *node)
        {
            Node *left = GetLeft(node), *parent = GetParent(node);
            SetLeft(node, GetRight(left));
            if(GetRight(left) != nullptr)
            {
                SetParent(GetRight(left), node);
            }
            SetParent(left, parent);
            if(parent == nullptr)
            {
                root_ = left;
            }
            else if(node == GetRight(parent))
            {
                SetRight(parent, left);
            }
            else
            {
                SetLeft(parent, left);
            }
            SetRight(left, node);
            SetParent(node, left);
        }

        void rb_left_rotate_(Node *node)
        {
            Node *right = GetRight(node), *parent = GetParent(node);
            SetRight(node, GetLeft(right));
            if(GetLeft(right) != nullptr)
            {
                SetParent(GetLeft(right), node);
            }
            SetParent(right, parent);
            if(node == root_)
            {
                root_ = right;
            }
            else if(node == GetLeft(parent))
            {
                SetLeft(parent, right);
            }
            else
            {
                SetRight(parent, right);
            }
            SetLeft(right, node);
            SetParent(node, right);
        }

        void rb_insert_(Node *key)
        {
            if(root_ == nullptr)
            {
                root_ = rb_init_node_(nullptr, key);
                SetColor(root_, rb_black);
                left_ = root_;
                return;
            }
            Node *node = root_, *where = nullptr;
            bool is_left = true;
            while(node != nullptr)
            {
                where = node;
                if(is_left = Wrapper::Compare(key, node))
                {
                    node = GetLeft(node);
                }
                else
                {
                    node = GetRight(node);
                }
            }
            if(is_left)
            {
                SetLeft(where, node = rb_init_node_(where, key));
                if(where == left_)
                {
                    left_ = node;
                }
            }
            else
            {
                SetRight(where, node = rb_init_node_(where, key));
            }
            while(GetColor(GetParent(node)) == rb_red)
            {
                if(GetParent(node) == GetLeft(GetParent(GetParent(node))))
                {
                    where = GetRight(GetParent(GetParent(node)));
                    if(GetColor(where) == rb_red)
                    {
                        SetColor(GetParent(node), rb_black);
                        SetColor(where, rb_black);
                        SetColor(GetParent(GetParent(node)), rb_red);
                        node = GetParent(GetParent(node));
                    }
                    else
                    {
                        if(node == GetRight(GetParent(node)))
                        {
                            node = GetParent(node);
                            rb_left_rotate_(node);
                        }
                        SetColor(GetParent(node), rb_black);
                        SetColor(GetParent(GetParent(node)), rb_red);
                        rb_right_rotate_(GetParent(GetParent(node)));
                    }
                }
                else
                {
                    where = GetLeft(GetParent(GetParent(node)));
                    if(GetColor(where) == rb_red)
                    {
                        SetColor(GetParent(node), rb_black);
                        SetColor(where, rb_black);
                        SetColor(GetParent(GetParent(node)), rb_red);
                        node = GetParent(GetParent(node));
                    }
                    else
                    {
                        if(node == GetLeft(GetParent(node)))
                        {
                            node = GetParent(node);
                            rb_right_rotate_(node);
                        }
                        SetColor(GetParent(node), rb_black);
                        SetColor(GetParent(GetParent(node)), rb_red);
                        rb_left_rotate_(GetParent(GetParent(node)));
                    }
                }
            }
            SetColor(root_, rb_black);
        }

    };

}