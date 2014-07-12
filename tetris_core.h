
#pragma once

#include <unordered_map>
#include <vector>
#include <algorithm>


struct TetrisNode;
struct TetrisOpertion;
struct TetrisMap;
union TetrisBlockStatus;

//��Ϸ����,�±��0��ʼ,���½�Ϊԭ��,���֧��[�߶�=32,���=32]
struct TetrisMap
{
    //������,�����÷���full������...
    int row[32];
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

template<>
struct std::hash<TetrisBlockStatus>
{
    size_t operator()(TetrisBlockStatus const &block) const
    {
        return std::hash<size_t>()(block.status);
    };
};

template<>
struct std::equal_to<TetrisBlockStatus>
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
    //��ָ������ȡ�ó�ʼ����
    TetrisNode const *(*generate)(TetrisMap const &);
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

    //����ָ����������
    //��Ӧ��������ɵ����ݸı�ȫ��Ԥ�ú�,����Ҫ�ټ���
    //���Ϊ��,��ʾ�Ѿ����ﳡ���߽���߲�֧�ָò���

    TetrisNode const *rotate_clockwise;
    TetrisNode const *rotate_counterclockwise;
    TetrisNode const *rotate_opposite;
    TetrisNode const *move_left;
    TetrisNode const *move_right;
    TetrisNode const *move_down;
    TetrisNode const *move_down_multi[32];

    //��鵱ǰ���Ƿ��ܹ��ϲ��볡��
    inline bool check(TetrisMap const &map) const
    {
        if(map.row[row] & data[0])
        {
            return false;
        }
        if(height == 1)
        {
            return true;
        }
        if(map.row[row + 1] & data[1])
        {
            return false;
        }
        if(height == 2)
        {
            return true;
        }
        if(map.row[row + 2] & data[2])
        {
            return false;
        }
        if(height == 3)
        {
            return true;
        }
        if(map.row[row + 3] & data[3])
        {
            return false;
        }
        return true;
    }
    //��鵱ǰ���Ƿ���¶���
    inline bool open(TetrisMap const &map) const
    {
        if(bottom[0] < map.top[col])
        {
            return false;
        }
        if(width == 1)
        {
            return true;
        }
        if(bottom[1] < map.top[col + 1])
        {
            return false;
        }
        if(width == 2)
        {
            return true;
        }
        if(bottom[2] < map.top[col + 2])
        {
            return false;
        }
        if(width == 3)
        {
            return true;
        }
        if(bottom[3] < map.top[col + 3])
        {
            return false;
        }
        return true;
    }
    //��ǰ��ϲ��볡��,ͬʱ���³�������
    inline size_t attach(TetrisMap &map) const
    {
        const int full = (1 << map.width) - 1;
        map.row[row] |= data[0];
        if(height > 1)
        {
            map.row[row + 1] |= data[1];
            if(height > 2)
            {
                map.row[row + 2] |= data[2];
                if(height > 3)
                {
                    map.row[row + 3] |= data[3];
                }
            }
        }
        int clear = 0;
        for(int i = height; i > 0; --i)
        {
            if(map.row[row + i - 1] == full)
            {
                memmove(&map.row[row + i - 1], &map.row[row + i], (map.height - i) * sizeof(int));
                map.row[map.height - 1] = 0;
                ++clear;
            }
        }
        if(top[0] > map.top[col])
        {
            map.top[col] = top[0];
            map.roof = std::max(top[0], map.roof);
        }
        if(width > 1)
        {
            if(top[1] > map.top[col + 1])
            {
                map.top[col + 1] = top[1];
                map.roof = std::max(top[1], map.roof);
            }
            if(width > 2)
            {
                if(top[2] > map.top[col + 2])
                {
                    map.top[col + 2] = top[2];
                    map.roof = std::max(top[2], map.roof);
                }
                if(width > 3)
                {
                    if(top[3] > map.top[col + 3])
                    {
                        map.top[col + 3] = top[3];
                        map.roof = std::max(top[3], map.roof);
                    }
                }
            }
        }
        map.roof -= clear;
        map.count += 4 - clear * map.width;
        if(clear > 0)
        {
            for(int x = 0; x < map.width; ++x)
            {
                map.top[x] = 0;
                for(int y = map.roof - 1; y >= 0; --y)
                {
                    if(map.full(x, y))
                    {
                        map.top[x] = y + 1;
                        break;
                    }
                }
            }
        }
        return clear;
    }
    //���㵱ǰ����λ��
    inline TetrisNode const *drop(TetrisMap const &map) const
    {
        int value = bottom[0] - map.top[col];
        if(width > 1)
        {
            value = std::min<int>(value, bottom[1] - map.top[col + 1]);
            if(width > 2)
            {
                value = std::min<int>(value, bottom[2] - map.top[col + 2]);
                if(width > 3)
                {
                    value = std::min<int>(value, bottom[3] - map.top[col + 3]);
                }
            }
        }
        if(value >= 0)
        {
            return move_down_multi[value];
        }
        else
        {
            TetrisNode const *node = this;
            while(node->move_down != nullptr && node->move_down->check(map))
            {
                node = node->move_down;
            }
            return node;
        }
    }
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
//����Σ��(��һ����ֿ��ܾ͹���)
extern inline bool map_in_danger(TetrisMap const &map);

//AI��ʼ��(��Ҫ����,��ֻ����һ��)
extern "C" void attach_init();

//��ʼ����Ⱥ͸߶�,�������֧��32x32�ĳ���(�ظ�����û�ж���ɱ�)
bool init_ai(int w, int h);

//���������,����(��ת,ƽ��,׹��)
namespace ai_simple
{
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