
#pragma once

#include <unordered_map>
#include <vector>
#include <map>
#include <algorithm>

namespace m_tetris
{

    struct TetrisNode;
    struct TetrisOpertion;
    struct TetrisMap;
    union TetrisBlockStatus;
    class TetrisContext;

    //��Ϸ����,�±��0��ʼ,���½�Ϊԭ��,���֧��[�߶�=40,���=32]
    struct TetrisMap
    {
        //������,�����÷���full������...
        int row[40];
        //ÿһ�еĸ߶�
        int top[32];
        //������
        int width;
        //������
        int height;
        //����Ŀǰ���߶�
        int roof;
        //�����ķ�����
        int count;
        //�ж�[x,y]�����Ƿ��з���
        inline bool full(int x, int y) const
        {
            return (row[y] >> x) & 1;
        }
    };

    //����״̬
    //t:OISZLJT�ַ�
    //[x,y]����,yԽ��߶�Խ��
    //r:��ת״̬
    union TetrisBlockStatus
    {
        struct
        {
            unsigned char t;
            char x, y;
            unsigned char r;
        };
        size_t status;
    };

    struct TetrisBlockStatusHash
    {
        size_t operator()(TetrisBlockStatus const &block) const
        {
            return std::hash<size_t>()(block.status);
        };
    };

    struct TetrisBlockStatusEqual
    {
        bool operator()(TetrisBlockStatus const &left, TetrisBlockStatus const &right) const
        {
            return left.status == right.status;
        };
    };

    //�������
    struct TetrisOpertion
    {
        //����һ������
        TetrisNode(*create)(int w, int h, TetrisOpertion op);
        //˳ʱ����ת(����,r-1)
        bool(*rotate_clockwise)(TetrisNode &node, TetrisContext const *context);
        //��ʱ����ת(����,r+1)
        bool(*rotate_counterclockwise)(TetrisNode &node, TetrisContext const *context);
        //ת��180��(r+2)
        bool(*rotate_opposite)(TetrisNode &node, TetrisContext const *context);
        //����
        bool(*move_left)(TetrisNode &node, TetrisContext const *context);
        //����
        bool(*move_right)(TetrisNode &node, TetrisContext const *context);
        //����
        bool(*move_down)(TetrisNode &node, TetrisContext const *context);
    };

    //ָ�����ڵ�
    struct TetrisNode
    {
    public:
        //����״̬
        TetrisBlockStatus status;
        //�����������
        TetrisOpertion op;
        //����ÿ�е�����
        int data[4];
        //����ÿ�е����ظ߶�
        int top[4];
        //����ÿ�е����ظ߶�
        int bottom[4];
        //�����ڳ����еľ���λ��
        char row, height, col, width;
        //���ֱ��λᴥ��������͸߶�
        int low;
        //ָ��������
        size_t index;
        //������������Ż�
        std::vector<TetrisNode const *> const *place;

        //������ָ����������
        //��Ӧ��������ɵ����ݸı�ȫ��Ԥ�ú�,����Ҫ�ټ���
        //���Ϊ��,��ʾ�Ѿ����ﳡ���߽���߲�֧�ָò���

        TetrisNode const *rotate_clockwise;
        TetrisNode const *rotate_counterclockwise;
        TetrisNode const *rotate_opposite;
        TetrisNode const *move_left;
        TetrisNode const *move_right;
        TetrisNode const *move_down;
        TetrisNode const *move_down_multi[32];

        TetrisContext const *context;

        //��鵱ǰ���Ƿ��ܹ��ϲ��볡��
        bool check(TetrisMap const &map) const;
        //��鵱ǰ���Ƿ���¶���
        bool open(TetrisMap const &map) const;
        //��ǰ��ϲ��볡��,ͬʱ���³�������
        size_t attach(TetrisMap &map) const;
        //���㵱ǰ����λ��
        TetrisNode const *drop(TetrisMap const &map) const;
    };

    class TetrisNodeMark
    {
    private:
        struct Mark
        {
            Mark() : version(0)
            {
            }
            size_t version;
            std::pair<TetrisNode const *, char> data;
        };
        size_t version_;
        std::vector<Mark> data_;

    public:
        void init(size_t size);
        inline void clear();
        inline std::pair<TetrisNode const *, char> get(TetrisNode const *key);
        inline bool set(TetrisNode const *key, TetrisNode const *node, char op);
        inline bool mark(TetrisNode const *key);
    };

    template<class T>
    struct TetrisContextBuilder;

    class TetrisContext
    {
        template<class T>
        friend struct TetrisContextBuilder;
    private:
        TetrisContext()
        {
        }
        std::unordered_map<TetrisBlockStatus, TetrisNode, TetrisBlockStatusHash, TetrisBlockStatusEqual> node_cache_;
        std::map<std::pair<unsigned char, unsigned char>, TetrisOpertion> init_opertion_;
        std::map<unsigned char, TetrisBlockStatus(*)(TetrisContext const *)> init_generate_;
        int width_, height_;
        int full_;

        std::map<unsigned char, std::vector<TetrisNode const *>> place_cache_;
        TetrisNodeMark node_mark_;
        size_t type_max_;
        TetrisNode const *generate_cache_[256];
        unsigned char index_to_type_[256];
        unsigned char type_to_index_[256];

    public:
        bool prepare(int width, int height);
        int width() const;
        int height() const;
        int full() const;
        size_t type_max() const;
        TetrisOpertion get_opertion(unsigned char t, unsigned char r) const;
        TetrisNode const *get(TetrisBlockStatus const &status) const;
        TetrisNode const *get(unsigned char t, char x, char y, unsigned char r) const;
        TetrisNode const *generate(unsigned char type) const;
        TetrisNode const *generate(size_t index) const;
        TetrisNode const *generate() const;
        bool TetrisContext::create(TetrisBlockStatus const &status, TetrisNode &node) const;
    };

    template<class LandPointEval, class MapEval>
    struct EvalData
    {
        TetrisNode const *node;
        size_t clear;
        TetrisMap map;
        LandPointEval land_point_eval;
        MapEval map_eval;
    };

    template<class LandPointEval>
    struct EvalParam
    {
        EvalParam(TetrisNode const *_node, size_t _clear, TetrisMap const &_map, LandPointEval const &_eval) : node(_node), clear(_clear), map(_map), eval(_eval)
        {
        }
        //��ǰ��
        TetrisNode const *node;
        //������
        size_t clear;
        //�ϲ�ǰ�ĳ���
        TetrisMap const &map;
        //�������
        LandPointEval const &eval;
    };
    template<class MapEval>
    struct PruneParam
    {
        PruneParam(MapEval const &_eval) : eval(_eval), pruned(false)
        {
        }
        MapEval eval;
        bool pruned;
    };

    template<class TetrisRuleSet>
    struct TetrisContextBuilder
    {
        static TetrisContext build_context()
        {
            TetrisContext context = {};
            context.init_opertion_ = TetrisRuleSet::get_opertion_info();
            context.init_generate_ = TetrisRuleSet::get_generate_info();
            return context;
        }
    };


    template<class TetrisRuleSet, class TetrisAI, class TetrisLandPointSearchEngine>
    class TetrisEngine
    {
    private:
        TetrisContext context_;
        TetrisLandPointSearchEngine search_;
        TetrisAI ai_;
        std::vector<EvalParam<typename TetrisAI::LandPointEval>> history_;

    public:
        TetrisEngine() : context_(TetrisContextBuilder<TetrisRuleSet>::build_context())
        {
        }
        TetrisNode const *get(TetrisBlockStatus const &status) const
        {
            return context_.get(status);
        }
        TetrisAI const &ai() const
        {
            return ai_;
        }
        TetrisContext const *context() const
        {
            return &context_;
        }
        std::string ai_name() const
        {
            return ai_.ai_name();
        }
        bool prepare(int width, int height)
        {
            if(context_.prepare(width, height))
            {
                ai_.init(&context_);
                return true;
            }
            return false;
        }
        TetrisNode const *run(TetrisMap const &map, TetrisNode const *node, unsigned char *next, size_t next_length)
        {
            return search_.do_ai(&context_, ai_, map, node, history_, next, next_length).first;
        }
        std::vector<char> path(TetrisNode const *node, TetrisNode const *land_point, TetrisMap const &map)
        {
            return search_.make_path(&context_, node, land_point, map);
        }
    };

}

namespace m_tetris_rule_tools
{
    using namespace m_tetris;
    template<unsigned char T, char X, char Y, unsigned char R, int line1, int line2, int line3, int line4>
    TetrisNode create_node(int w, int h, TetrisOpertion op)
    {
        static_assert(X < 0 || X >= 4 || Y < 0 || Y >= 3 || (line1 || line2 || line3 || line3), "data error");
        TetrisBlockStatus status =
        {
            T, X, h - Y - 1, R
        };
        TetrisNode node =
        {
            status, op, {line4, line3, line2, line1}, {}, {}, h - 4, 4, 0, 4
        };
        while(node.data[0] == 0)
        {
            ++node.row;
            --node.height;
            memmove(&node.data[0], &node.data[1], node.height * sizeof(int));
            node.data[node.height] = 0;
        }
        while(node.data[node.height - 1] == 0)
        {
            --node.height;
        }
        while(!((node.data[0] >> node.col) & 1) && !((node.data[1] >> node.col) & 1) && !((node.data[2] >> node.col) & 1) && !((node.data[3] >> node.col) & 1))
        {
            ++node.col;
            --node.width;
        }
        while(!((node.data[0] >> (node.col + node.width - 1)) & 1) && !((node.data[1] >> (node.col + node.width - 1)) & 1) && !((node.data[2] >> (node.col + node.width - 1)) & 1) && !((node.data[3] >> (node.col + node.width - 1)) & 1))
        {
            --node.width;
        }
        for(int x = node.col; x < node.col + node.width; ++x)
        {
            int y;
            for(y = node.height; y > 0; --y)
            {
                if((node.data[y - 1] >> x) & 1)
                {
                    break;
                }
            }
            node.top[x - node.col] = node.row + y;
            for(y = 0; y < node.height; ++y)
            {
                if((node.data[y] >> x) & 1)
                {
                    break;
                }
            }
            node.bottom[x - node.col] = node.row + y;
        }
        return node;
    }

    template<unsigned char R>
    bool rotate_template(TetrisNode &node, TetrisContext const *context)
    {
        TetrisBlockStatus status =
        {
            node.status.t, node.status.x, node.status.y, R
        };
        return context->create(status, node);
    }

    bool move_left(TetrisNode &node, TetrisContext const *context);
    bool move_right(TetrisNode &node, TetrisContext const *context);
    bool move_down(TetrisNode &node, TetrisContext const *context);
}