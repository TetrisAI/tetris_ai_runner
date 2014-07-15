
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

    //�����ַ�
    extern unsigned char const tetris[7];
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
        bool(*rotate_clockwise)(TetrisNode &, TetrisMap const &);
        //��ʱ����ת(����,r+1)
        bool(*rotate_counterclockwise)(TetrisNode &, TetrisMap const &);
        //ת��180��(r+2)
        bool(*rotate_opposite)(TetrisNode &, TetrisMap const &);
        //����
        bool(*move_left)(TetrisNode &, TetrisMap const &);
        //����
        bool(*move_right)(TetrisNode &, TetrisMap const &);
        //����
        bool(*move_down)(TetrisNode &, TetrisMap const &);
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
        inline bool check(TetrisMap const &map) const;
        //��鵱ǰ���Ƿ���¶���
        inline bool open(TetrisMap const &map) const;
        //��ǰ��ϲ��볡��,ͬʱ���³�������
        inline size_t attach(TetrisMap &map) const;
        //���㵱ǰ����λ��
        inline TetrisNode const *drop(TetrisMap const &map) const;
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

    class TetrisContext
    {
    private:
        std::unordered_map<TetrisBlockStatus, TetrisNode, TetrisBlockStatusHash, TetrisBlockStatusEqual> node_cache_;
        std::map<std::pair<unsigned char, unsigned char>, TetrisOpertion> init_opertion_;
        std::map<unsigned char, TetrisBlockStatus(*)(TetrisContext const *)> init_generate_;
        int width_, height_;
        int full_;

        std::map<unsigned char, std::vector<TetrisNode const *>> place_cache_;
        TetrisNodeMark node_mark_;
        size_t type_max_;
        TetrisNode const *generate_cache_[256];
        struct
        {
            int data[4];
        } map_danger_data_[256];
        unsigned char index_to_type_[256];
        unsigned char type_to_index_[256];

    public:
        bool prepare(int width, int height);
        int width() const;
        int height() const;
        int full() const;
        TetrisOpertion get_opertion(unsigned char t, unsigned char r) const;
        TetrisNode const *get(TetrisBlockStatus const &status) const;
        TetrisNode const *get(unsigned char t, char x, char y, unsigned char r) const;
        inline TetrisNode const *generate(unsigned char type) const;
        inline TetrisNode const *generate(size_t index) const;
        inline TetrisNode const *generate() const;
        inline size_t map_in_danger(TetrisMap const &map) const;
    };










    //��״̬��ȡָ�����ڵ�
    extern inline TetrisNode const *get(TetrisBlockStatus const &status);
    //��״̬��ȡָ�����ڵ�
    extern inline TetrisNode const *get(unsigned char t, char x, char y, unsigned char r);
    //��һ���������䵽�赲λ��
    extern inline TetrisNode const *drop(TetrisNode const *node, TetrisMap const &map);
    //��ȡָ������(����������OISZLJT֮һ)
    extern inline TetrisNode const *generate(unsigned char type, TetrisMap const &map);
    //��ȡָ������(�����±�,OISZLJT)
    extern inline TetrisNode const *generate(size_t index, TetrisMap const &map);
    //���һ������
    extern inline TetrisNode const *generate(TetrisMap const &map);
    //����Σ��(��һ����ֿ��ܾ͹���),����7�з����м��ֻ�ҵ�
    extern inline size_t map_in_danger(TetrisMap const &map);

    //AI��ʼ��(��Ҫ����,��ֻ����һ��)
    extern "C" void attach_init();

    //��ʼ����Ⱥ͸߶�,�������֧��32x32�ĳ���(�ظ�����û�ж���ɱ�)
    bool init_ai(int w, int h);

    //���������,����(��ת,ƽ��,׹��)
    namespace ai_simple
    {
        //����һ������·��(Ϊ�ձ�ʾ�޷�����,ĩβ�Դ�\0)
        extern std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map);
        //AI���
        extern std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
    }

    //�����������,��(Ѱ�����пɵ����λ��),���ܱ��������Ե�(�����ǵ�һ�����)
    namespace ai_path
    {
        //����һ������·��(Ϊ�ձ�ʾ�޷�����,ĩβ�Դ�\0,·��ԭ���ǲ�����������)
        extern std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map);
        //AI���
        extern std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
    }

    //�߼��������,��,��ǽ(����Ͳ�������),���ܻ���֪��...TODO
    namespace ai_senior
    {
        //����һ������·��(Ϊ�ձ�ʾ�޷�����,ĩβ�Դ�\0)
        extern std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map);
        //AI���
        extern std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
    }

    //misakamm����õ���mt���
    namespace ege
    {
        extern void mtsrand(unsigned int s);
        extern unsigned int mtirand();
        extern double mtdrand();
    }
    using ege::mtdrand;
    using ege::mtirand;
    using ege::mtsrand;

    //eavl����
    struct EvalParam
    {
        EvalParam(TetrisNode const *_node, size_t _clear, TetrisMap const &_map) : node(_node), clear(_clear), map(_map)
        {
        }
        //��ǰ��
        TetrisNode const *node;
        //������
        size_t clear;
        //�ϲ�ǰ�ĳ���
        TetrisMap const &map;
    };
    //�����AI����һ�����ְ�...
    extern std::string ai_name();
    //eval����...��������ֵ(history����,Խ�����ʷ���ݲ���Խ��ǰ)
    extern int ai_eval(TetrisMap const &map, EvalParam *history, size_t history_length);

    namespace m_tetris_rule_toole
    {

        template<unsigned char T, char X, char Y, unsigned char R, int line1, int line2, int line3, int line4>
        TetrisNode create_node(int w, int h, TetrisOpertion op)
        {
            static_assert(line1 || line2 || line3 || line3, "data error");
            TetrisBlockStatus status =
            {
                T, X, Y, R
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
            TetrisNode const *cache = context->get(status);
            if(cache != nullptr)
            {
                node = *cache;
                return true;
            }
            TetrisOpertion op = context->get_opertion(node.status.t, R);
            TetrisNode new_node = op.create(context->width(), context->height(), op);
            while(new_node.status.x < node.status.x)
            {
                if(!move_right(new_node, map))
                {
                    return false;
                }
            }
            while(new_node.status.y > node.status.y)
            {
                if(!move_down(new_node, map))
                {
                    return false;
                }
            }
            node = new_node;
            return true;
        }

        bool move_left(TetrisNode &node, TetrisMap const &map);

        bool move_right(TetrisNode &node, TetrisMap const &map);

        bool move_down(TetrisNode &node, TetrisMap const &map);
    }

    template<class TetrisRuleSet>
    class TetrisContextBuilder
    {
        void build_context(TetrisContext *context)
        {

        }
    };


    template<class TetrisRuleSet, class TetrisLandPointSearchEngine, class TetrisMapEval, class TetrisLandPointEval, class TetrisPruning, size_t MaxDeep>
    class TetrisEngine
    {

    };

}