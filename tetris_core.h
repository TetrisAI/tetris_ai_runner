
#pragma once

#include <unordered_map>
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
    //ԭʼ�����ڳ����е�����
    int low;
    //ָ��������
    size_t index;

    //����ָ����������
    //��Ӧ��������ɵ����ݸı�ȫ��Ԥ�ú�,����Ҫ�ټ���
    //���Ϊ��,��ʾ�Ѿ����ﳡ���߽���߲�֧�ָò���

    TetrisNode const *rotate_clockwise;
    TetrisNode const *rotate_counterclockwise;
    TetrisNode const *rotate_opposite;
    TetrisNode const *move_left;
    TetrisNode const *move_right;
    TetrisNode const *move_down;

    //��鵱ǰ���Ƿ��ܹ��ϲ��볡��
    bool check(TetrisMap const &map) const
    {
        return !(check_row(0, map) || check_row(1, map) || check_row(2, map) || check_row(3, map));
    }
    //��鵱ǰ���Ƿ���¶���
    bool open(TetrisMap const &map) const
    {
        return open_col(0, map) && open_col(1, map) && open_col(2, map) && open_col(3, map);
    }
    //��ǰ��ϲ��볡��,ͬʱ���³�������
    size_t attach(TetrisMap &map) const
    {
        const int full = (1 << map.width) - 1;
        attach_row(0, map);
        attach_row(1, map);
        attach_row(2, map);
        attach_row(3, map);
        int clear = 0;
        clear += clear_row(3, full, map);
        clear += clear_row(2, full, map);
        clear += clear_row(1, full, map);
        clear += clear_row(0, full, map);
        update_top(0, map);
        update_top(1, map);
        update_top(2, map);
        update_top(3, map);
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
    //���㵱ǰ�����������ٸ�
    size_t drop(TetrisMap const &map) const
    {
        int value = map.height;
        drop_col(0, value, map);
        drop_col(1, value, map);
        drop_col(2, value, map);
        drop_col(3, value, map);
        return value > 0 ? value : 0;
    }
private:
    inline int check_row(int offset, TetrisMap const &map) const
    {
        if(offset < height)
        {
            return map.row[row + offset] & data[offset];
        }
        return 0;
    }
    inline int open_col(int offset, TetrisMap const &map) const
    {
        if(offset < width)
        {
            return bottom[offset] >= map.top[col + offset];
        }
        return true;
    }
    inline void update_top(int offset, TetrisMap &map) const
    {
        if(offset < width)
        {
            int &value = map.top[col + offset];
            value = std::max<int>(value, top[offset]);
            if(value > map.roof)
            {
                map.roof = value;
            }
        }
    }
    inline void attach_row(int offset, TetrisMap &map) const
    {
        if(offset < height)
        {
            map.row[row + offset] |= data[offset];
        }
    }
    inline int clear_row(int offset, int const full, TetrisMap &map) const
    {
        if(offset >= height)
        {
            return 0;
        }
        int index = row + offset;
        if(map.row[index] == full)
        {
            memmove(&map.row[index], &map.row[index + 1], (map.height - index - 1) * sizeof(int));
            map.row[map.height - 1] = 0;
            return 1;
        }
        return 0;
    }
    inline void drop_col(int offset, int &value, TetrisMap const &map) const
    {
        if(offset < width)
        {
            value = std::min<int>(value, bottom[offset] - map.top[offset + col]);
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
//���һ������
extern inline TetrisNode const *generate(TetrisMap const &map);

//AI��ʼ��
extern "C" void attach_init();

//��ʼ����Ⱥ͸߶�(�ظ�����û�ж���ɱ�)
bool init_ai(int w, int h);
//��char array����һ������
void build_map(char board[], int w, int h, TetrisMap &map);

//���������(��ת,ƽ��,׹��)
namespace ai_simple
{
    //AI���
    extern std::pair<TetrisNode const *, int> do_ai(TetrisMap const &primeval_map, TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
}

//�����������(Ѱ�����пɵ����λ��)
namespace ai_path
{
    //����һ������·��(Ϊ�ձ�ʾ�޷�����,ĩβ�Դ�\0,·��ԭ���Ǿ����ܶ�)
    extern std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map);
    //AI���
    extern std::pair<TetrisNode const *, int> do_ai(TetrisMap const &primeval_map, TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
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
//eval����...��������ֵ(history����,Խ�����ʷ���ݲ���Խ��ǰ)
extern int ai_eval(TetrisMap const &map, EvalParam *history, size_t history_length);