
#pragma once

#include <algorithm>
#include <numeric>
#include "bst_base.h"

namespace zzz
{
    //incomplete
    template<class Interface>
    class sb_tree : public bst_base<Interface>
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
                ptr_ = sb_tree::sbt_advance_(ptr_, diff);
                return *this;
            }
            iterator &operator -= (difference_type diff)
            {
                ptr_ = sb_tree::sbt_advance_(ptr_, -diff);
                return *this;
            }
            iterator operator + (difference_type diff)
            {
                return iterator(sb_tree::sbt_advance_(ptr_, diff));
            }
            iterator operator - (difference_type diff)
            {
                return iterator(sb_tree::sbt_advance_(ptr_, -diff));
            }
            difference_type operator - (iterator const &other)
            {
                return static_cast<int>(sb_tree::sbt_rank_(ptr_)) - static_cast<int>(sb_tree::sbt_rank_(other.ptr_));
            }
            iterator &operator++()
            {
                ptr_ = sb_tree::bst_move_<true>(ptr_);
                return *this;
            }
            iterator &operator--()
            {
                ptr_ = sb_tree::bst_move_<false>(ptr_);
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
        friend class iterator;

    public:
        sb_tree() : bst_base()
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

        typedef std::pair<iterator, bool> pair_ib_t;
        typedef std::pair<iterator, iterator> pair_ii_t;

        pair_ib_t insert(value_node_t *node)
        {
            sbt_insert_(node);
            return pair_ib_t(node, true);
        }
        template<class iterator_t>
        size_t insert(iterator_t begin, iterator_t end)
        {
            size_t insert_count = 0;
            for(; begin != end; ++begin)
            {
                if(insert(*begin).second)
                {
                    ++insert_count;
                }
            }
            return insert_count;
        }
        iterator find(key_t const &key)
        {
            node_t *where = bst_lower_bound_(key);
            return (is_nil_(where) || interface_t::predicate(key, get_key_(where))) ? iterator(nil_()) : iterator(where);
        }
        void erase(iterator where)
        {
            sbt_erase_(&*where);
        }
        void erase(value_node_t *node)
        {
            sbt_erase_(node);
        }
        size_t erase(key_t const &key)
        {
            size_t erase_count = 0;
            pair_ii_t range = equal_range(key);
            while(range.first != range.second)
            {
                erase(range.first++);
                ++erase_count;
            }
            return erase_count;
        }
        size_t count(key_t const &key)
        {
            pair_ii_t range = equal_range(key);
            return std::distance(range.first, range.second);
        }
        size_t count(key_t const &min, key_t const &max)
        {
            return sbt_rank_(bst_upper_bound_(max)) - sbt_rank_(bst_lower_bound_(min));
        }
        pair_ii_t range(key_t const &min, key_t const &max)
        {
            return pair_ii_t(bst_lower_bound_(min), bst_upper_bound_(max));
        }
        pair_ii_t slice(int begin = 0, int end = std::numeric_limits<int>::max())
        {
            int size_s = size();
            if(begin < 0)
            {
                begin = std::max(size_s + begin, 0);
            }
            if(end < 0)
            {
                end = size_s + end;
            }
            if(begin > end || begin >= size_s)
            {
                return pair_ii_t(sb_tree::end(), sb_tree::end());
            }
            if(end > size_s)
            {
                end = size_s;
            }
            return pair_ii_t(sb_tree::begin() + begin, sb_tree::end() - (size_s - end));
        }
        iterator lower_bound(key_t const &key)
        {
            return iterator(bst_lower_bound_(key));
        }
        iterator upper_bound(key_t const &key)
        {
            return iterator(bst_upper_bound_(key));
        }
        pair_ii_t equal_range(key_t const &key)
        {
            node_t *lower, *upper;
            bst_equal_range_(key, lower, upper);
            return pair_ii_t(lower, upper);
        }
        iterator begin()
        {
            return iterator(get_most_left_());
        }
        iterator end()
        {
            return iterator(nil_());
        }
        value_node_t *front()
        {
            return static_cast<value_node_t *>(get_most_left_());
        }
        value_node_t *back()
        {
            return static_cast<value_node_t *>(get_most_right_());
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
        value_node_t *at(size_t index)
        {
            return sbt_at_(get_root_(), index);
        }
        size_t rank(key_t const &key)
        {
            return sbt_rank_(bst_upper_bound_(key));
        }
        static size_t rank(value_node_t *node)
        {
            return sbt_rank_(node);
        }
        static size_t rank(iterator where)
        {
            return sbt_rank_(&*where);
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

        static node_t *sbt_at_(node_t *node, size_t index)
        {
            if(index >= get_size_(node))
            {
                return nullptr;
            }
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

        static node_t *sbt_advance_(node_t *node, int step)
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
                    return get_parent_(node);
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

        static size_t sbt_rank_(node_t *node)
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

        template<bool is_left>
        node_t *sbt_rotate_(node_t *node)
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
            set_size_(child, get_size_(node));
            sbt_refresh_size_(node);
            return child;
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
            if(is_nil_(get_child_<is_left>(node)))
            {
                return node;
            }
            if(get_size_(get_child_<is_left>(get_child_<is_left>(node))) > get_size_(get_child_<!is_left>(node)))
            {
                node = sbt_rotate_<!is_left>(node);
            }
            else
            {
                if(get_size_(get_child_<!is_left>(get_child_<is_left>(node))) > get_size_(get_child_<!is_left>(node)))
                {
                    sbt_rotate_<is_left>(get_child_<is_left>(node));
                    node = sbt_rotate_<!is_left>(node);
                }
                else
                {
                    return node;
                };
            };
            if(!is_nil_(get_child_<true>(node)))
            {
                sbt_maintain_<true>(get_child_<true>(node));
            }
            if(!is_nil_(get_child_<false>(node)))
            {
                sbt_maintain_<false>(get_child_<false>(node));
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
                    sbt_erase_on_<true>(node);
                }
                else
                {
                    sbt_erase_on_<false>(node);
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
                set_most_left_(is_nil_(fix_node) ? fix_node_parent : bst_most_<true>(fix_node));
            }
            if(get_most_right_() == erase_node)
            {
                set_most_right_(is_nil_(fix_node) ? fix_node_parent : bst_most_<false>(fix_node));
            }
        }

        template<bool is_left>
        void sbt_erase_on_(node_t *node)
        {
            node_t *erase_node = node;
            node_t *fix_node;
            node_t *fix_node_parent;
            node = bst_move_<!is_left>(node);
            fix_node = get_child_<is_left>(node);
            fix_node_parent = node;
            while((fix_node_parent = get_parent_(fix_node_parent)) != erase_node)
            {
                set_size_(fix_node_parent, get_size_(fix_node_parent) - 1);
            }
            set_parent_(get_child_<!is_left>(erase_node), node);
            set_child_<!is_left>(node, get_child_<!is_left>(erase_node));
            if(node == get_child_<is_left>(erase_node))
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
                set_child_<!is_left>(fix_node_parent, fix_node);
                set_child_<is_left>(node, get_child_<is_left>(erase_node));
                set_parent_(get_child_<is_left>(erase_node), node);
            }
            if(get_root_() == erase_node)
            {
                set_root_(node);
            }
            else if(get_child_<!is_left>(get_parent_(erase_node)) == erase_node)
            {
                set_child_<!is_left>(get_parent_(erase_node), node);
            }
            else
            {
                set_child_<is_left>(get_parent_(erase_node), node);
            }
            set_parent_(node, get_parent_(erase_node));
            sbt_refresh_size_(node);
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
                printf("%s%d\n", (head + with + fork).c_str(), sbt_get_index_(node));
                print_tree(get_left_(node), level + 1, head + next_left, "©»", 2);
            }
        }
    };

}