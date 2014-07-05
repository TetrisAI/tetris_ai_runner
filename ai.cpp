#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS

#include <string.h>

#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall

char gName[64]; // 返回名字用，必须全局

#ifdef __cplusplus
extern "C" {
#endif

// 返回AI名字，会显示在界面上
DECLSPEC_EXPORT
char* WINAPI Name()
{
	strcpy(gName, "ZouZhiZhang v0.1");
	return gName;
}

#ifdef __cplusplus
}
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#include <unordered_map>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <array>
#include <functional>
#include <string>
#include <ctime>

struct TetrisNode;
struct TetrisOpertion;
struct TetrisMap;
union TetrisBlockStatus;

struct TetrisMap
{
    int row[32];
    int top;
    int width;
    int height;
    int count;
    inline bool full(int x, int y) const
    {
        return (row[y] >> x) & 1;
    }
};

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

struct TetrisOpertion
{
    TetrisNode(*create)(int w, int h);
    TetrisNode const *(*generate)(TetrisMap const &);
    bool(*rotate_clockwise)(TetrisNode &, TetrisMap const &);
    bool(*rotate_counterclockwise)(TetrisNode &, TetrisMap const &);
    bool(*rotate_opposite)(TetrisNode &, TetrisMap const &);
    bool(*move_left)(TetrisNode &, TetrisMap const &);
    bool(*move_right)(TetrisNode &, TetrisMap const &);
    bool(*move_down)(TetrisNode &, TetrisMap const &);
};

struct TetrisNode
{
    TetrisBlockStatus status;
    TetrisOpertion op;
    int data[4];
    int row;

    TetrisNode const *rotate_clockwise;
    TetrisNode const *rotate_counterclockwise;
    TetrisNode const *rotate_opposite;
    TetrisNode const *move_left;
    TetrisNode const *move_right;
    TetrisNode const *move_down;

    bool check(TetrisMap const &map) const
    {
        return !(
            check_row<1>(row + 0, data[3], map.row) ||
            check_row<1>(row + 1, data[2], map.row) ||
            check_row<0>(row + 2, data[1], map.row) ||
            check_row<0>(row + 3, data[0], map.row) ||
            false);
    }
    bool valid(TetrisMap const &map) const
    {
        int count = 0;
        for(int i = 0, flag = 1; i < map.width; ++i, flag <<= 1)
        {
            count += (row < -3) ? 0 : !!(data[0] & flag);
            count += (row < -2) ? 0 : !!(data[1] & flag);
            count += (row < -1) ? 0 : !!(data[2] & flag);
            count += (row <  0) ? 0 : !!(data[3] & flag);
        }
        return count == 4;
    }
    size_t attach(TetrisMap &map) const
    {
        const int full = (1 << map.width) - 1;
        attach_row<0>(row + 0, data[3], map.row);
        attach_row<0>(row + 1, data[2], map.row);
        attach_row<1>(row + 2, data[1], map.row);
        attach_row<1>(row + 3, data[0], map.row);
        int clear = 0;
        clear += clear_row(row + 3, full, map.height, map.row);
        clear += clear_row(row + 2, full, map.height, map.row);
        clear += clear_row(row + 1, full, map.height, map.row);
        clear += clear_row(row + 0, full, map.height, map.row);
        if(data[0] != 0)
        {
            map.top = std::max(map.top, row + 4) - clear;
        }
        else if(data[1] != 0)
        {
            map.top = std::max(map.top, row + 3) - clear;
        }
        else
        {
            map.top = std::max(map.top, row + 2) - clear;
        }
        map.count += 4 - clear * map.width;
        return clear;
    }
private:
    inline int clear_row(int row, int const full, int height, int map[]) const
    {
        if(row >= 0 && map[row] == full)
        {
            memmove(&map[row], &map[row + 1], (height - row - 1) * sizeof(int));
            map[height - 1] = 0;
            return 1;
        }
        return 0;
    }
    template<bool check>
    static inline void attach_row(int row, int data_row, int map[])
    {
        if(check || row >= 0)
        {
            map[row] |= data_row;
        }
    }
    template<bool check>
    static inline int check_row(int row, int data_row, int const map[])
    {
        if(check || row >= 0)
        {
            return map[row] & data_row;
        }
        return 0;
    }
};

typedef std::unordered_map<TetrisBlockStatus, TetrisNode> NodeCache_t;
NodeCache_t node_cache;

inline TetrisNode const *get(TetrisBlockStatus const &status)
{
    auto find = node_cache.find(status);
    if(find == node_cache.end())
    {
        return nullptr;
    }
    else
    {
        return &find->second;
    }
}
std::map<unsigned char, TetrisOpertion> op;

////////////////////////////////////////////////////////////////////////////////
#define T(o, a, b, c, d) (((a) ? (1 << ((o) - 2)) : 0) | ((b) ? (1 << ((o) - 1)) : 0) | ((c) ? (1 << (o)) : 0) | ((d) ? (1 << ((o) + 1)) : 0))

TetrisNode create_O(int w, int h)
{
    TetrisBlockStatus status =
    {
        'O', w / 2, h - 2, 1
    };
    return
    {
        status,
        {},
        {
            T(status.x, 0, 0, 0, 0),
            T(status.x, 0, 1, 1, 0),
            T(status.x, 0, 1, 1, 0),
            T(status.x, 0, 0, 0, 0),
        },
        h - 4
    };
}

TetrisNode create_I(int w, int h)
{
    TetrisBlockStatus status =
    {
        'I', w / 2, h - 2, 1
    };
    return
    {
        status,
        {},
        {
            T(status.x, 0, 0, 0, 0),
            T(status.x, 1, 1, 1, 1),
            T(status.x, 0, 0, 0, 0),
            T(status.x, 0, 0, 0, 0),
        },
        h - 4
    };
}

TetrisNode create_S(int w, int h)
{
    TetrisBlockStatus status =
    {
        'S', w / 2, h - 2, 1
    };
    return
    {
        status,
        {},
        {
            T(status.x, 0, 0, 0, 0),
            T(status.x, 0, 0, 1, 1),
            T(status.x, 0, 1, 1, 0),
            T(status.x, 0, 0, 0, 0),
        },
        h - 4
    };
}

TetrisNode create_Z(int w, int h)
{
    TetrisBlockStatus status =
    {
        'Z', w / 2, h - 2, 1
    };
    return
    {
        status,
        {},
        {
            T(status.x, 0, 0, 0, 0),
            T(status.x, 0, 1, 1, 0),
            T(status.x, 0, 0, 1, 1),
            T(status.x, 0, 0, 0, 0),
        },
        h - 4
    };
}

TetrisNode create_L(int w, int h)
{
    TetrisBlockStatus status =
    {
        'L', w / 2, h - 2, 1
    };
    return
    {
        status,
        {},
        {
            T(status.x, 0, 0, 0, 0),
            T(status.x, 0, 1, 1, 1),
            T(status.x, 0, 1, 0, 0),
            T(status.x, 0, 0, 0, 0),
        },
        h - 4
    };
}

TetrisNode create_J(int w, int h)
{
    TetrisBlockStatus status =
    {
        'J', w / 2, h - 2, 1
    };
    return
    {
        status,
        {},
        {
            T(status.x, 0, 0, 0, 0),
            T(status.x, 0, 1, 1, 1),
            T(status.x, 0, 0, 0, 1),
            T(status.x, 0, 0, 0, 0),
        },
        h - 4
    };
}

TetrisNode create_T(int w, int h)
{
    TetrisBlockStatus status =
    {
        'T', w / 2, h - 2, 1
    };
    return
    {
        status,
        {},
        {
            T(status.x, 0, 0, 0, 0),
            T(status.x, 0, 1, 1, 1),
            T(status.x, 0, 0, 1, 0),
            T(status.x, 0, 0, 0, 0),
        },
        h - 4
    };
}

bool rotate_I12(TetrisNode &node, TetrisMap const &map);
bool rotate_I21(TetrisNode &node, TetrisMap const &map);

bool rotate_S12(TetrisNode &node, TetrisMap const &map);
bool rotate_S21(TetrisNode &node, TetrisMap const &map);

bool rotate_Z12(TetrisNode &node, TetrisMap const &map);
bool rotate_Z21(TetrisNode &node, TetrisMap const &map);

bool rotate_L12(TetrisNode &node, TetrisMap const &map);
bool rotate_L23(TetrisNode &node, TetrisMap const &map);
bool rotate_L34(TetrisNode &node, TetrisMap const &map);
bool rotate_L41(TetrisNode &node, TetrisMap const &map);
bool rotate_L14(TetrisNode &node, TetrisMap const &map);
bool rotate_L43(TetrisNode &node, TetrisMap const &map);
bool rotate_L32(TetrisNode &node, TetrisMap const &map);
bool rotate_L21(TetrisNode &node, TetrisMap const &map);

bool rotate_J12(TetrisNode &node, TetrisMap const &map);
bool rotate_J23(TetrisNode &node, TetrisMap const &map);
bool rotate_J34(TetrisNode &node, TetrisMap const &map);
bool rotate_J41(TetrisNode &node, TetrisMap const &map);
bool rotate_J14(TetrisNode &node, TetrisMap const &map);
bool rotate_J43(TetrisNode &node, TetrisMap const &map);
bool rotate_J32(TetrisNode &node, TetrisMap const &map);
bool rotate_J21(TetrisNode &node, TetrisMap const &map);

bool rotate_T12(TetrisNode &node, TetrisMap const &map);
bool rotate_T23(TetrisNode &node, TetrisMap const &map);
bool rotate_T34(TetrisNode &node, TetrisMap const &map);
bool rotate_T41(TetrisNode &node, TetrisMap const &map);
bool rotate_T14(TetrisNode &node, TetrisMap const &map);
bool rotate_T43(TetrisNode &node, TetrisMap const &map);
bool rotate_T32(TetrisNode &node, TetrisMap const &map);
bool rotate_T21(TetrisNode &node, TetrisMap const &map);

bool rotate_I12(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 2
        },
        {
            node.op.create,
            node.op.generate,
            rotate_I21,
            rotate_I21,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}

bool rotate_I21(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 1
        },
        {
            node.op.create,
            node.op.generate,
            rotate_I12,
            rotate_I12,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 1, 1, 1, 1),
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}

bool rotate_S12(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 2
        },
        {
            node.op.create,
            node.op.generate,
            rotate_S21,
            rotate_S21,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 1),
            T(node.status.x, 0, 0, 0, 1),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}

bool rotate_S21(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 1
        },
        {
            node.op.create,
            node.op.generate,
            rotate_S12,
            rotate_S12,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 0, 1, 1),
            T(node.status.x, 0, 1, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}

bool rotate_Z12(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 2
        },
        {
            node.op.create,
            node.op.generate,
            rotate_Z21,
            rotate_Z21,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 1),
            T(node.status.x, 0, 0, 1, 1),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}

bool rotate_Z21(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 1
        },
        {
            node.op.create,
            node.op.generate,
            rotate_Z12,
            rotate_Z12,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 1, 1, 0),
            T(node.status.x, 0, 0, 1, 1),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}

bool rotate_L12(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 2
        },
        {
            node.op.create,
            node.op.generate,
            rotate_L21,
            rotate_L23,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 1),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_L23(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 3
        },
        {
            node.op.create,
            node.op.generate,
            rotate_L32,
            rotate_L34,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 1),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_L34(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 4
        },
        {
            node.op.create,
            node.op.generate,
            rotate_L43,
            rotate_L41,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 1, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_L41(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 1
        },
        {
            node.op.create,
            node.op.generate,
            rotate_L14,
            rotate_L12,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 1, 0, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_L14(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 4
        },
        {
            node.op.create,
            node.op.generate,
            rotate_L43,
            rotate_L41,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 1, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_L43(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 3
        },
        {
            node.op.create,
            node.op.generate,
            rotate_L32,
            rotate_L34,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 1),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_L32(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 2
        },
        {
            node.op.create,
            node.op.generate,
            rotate_L21,
            rotate_L23,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 1),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_L21(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 1
        },
        {
            node.op.create,
            node.op.generate,
            rotate_L14,
            rotate_L12,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 1, 0, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}

bool rotate_J12(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 2
        },
        {
            node.op.create,
            node.op.generate,
            rotate_J21,
            rotate_J23,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 1),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_J23(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 3
        },
        {
            node.op.create,
            node.op.generate,
            rotate_J32,
            rotate_J34,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 1, 0, 0),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_J34(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 4
        },
        {
            node.op.create,
            node.op.generate,
            rotate_J43,
            rotate_J41,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 1, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_J41(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 1
        },
        {
            node.op.create,
            node.op.generate,
            rotate_J14,
            rotate_J12,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 0, 0, 1),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_J14(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 4
        },
        {
            node.op.create,
            node.op.generate,
            rotate_J43,
            rotate_J41,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 1, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_J43(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 3
        },
        {
            node.op.create,
            node.op.generate,
            rotate_J32,
            rotate_J34,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 1, 0, 0),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_J32(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 2
        },
        {
            node.op.create,
            node.op.generate,
            rotate_J21,
            rotate_J23,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 1),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_J21(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 1
        },
        {
            node.op.create,
            node.op.generate,
            rotate_J14,
            rotate_J12,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 0, 0, 1),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}

bool rotate_T12(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 2
        },
        {
            node.op.create,
            node.op.generate,
            rotate_T21,
            rotate_T23,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 1),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_T23(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 3
        },
        {
            node.op.create,
            node.op.generate,
            rotate_T32,
            rotate_T34,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_T34(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 4
        },
        {
            node.op.create,
            node.op.generate,
            rotate_T43,
            rotate_T41,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 1, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_T41(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 1
        },
        {
            node.op.create,
            node.op.generate,
            rotate_T14,
            rotate_T12,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_T14(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 4
        },
        {
            node.op.create,
            node.op.generate,
            rotate_T43,
            rotate_T41,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 1, 1, 0),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_T43(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 3
        },
        {
            node.op.create,
            node.op.generate,
            rotate_T32,
            rotate_T34,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_T32(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 2
        },
        {
            node.op.create,
            node.op.generate,
            rotate_T21,
            rotate_T23,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 1, 1),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}
bool rotate_T21(TetrisNode &node, TetrisMap const &map)
{
    TetrisNode new_node =
    {
        {
            node.status.t, node.status.x, node.status.y, 1
        },
        {
            node.op.create,
            node.op.generate,
            rotate_T14,
            rotate_T12,
            node.op.rotate_opposite,
            node.op.move_left,
            node.op.move_right,
            node.op.move_down
        },
        {
            T(node.status.x, 0, 0, 0, 0),
            T(node.status.x, 0, 1, 1, 1),
            T(node.status.x, 0, 0, 1, 0),
            T(node.status.x, 0, 0, 0, 0),
        },
        node.row
    };
    if(new_node.valid(map))
    {
        node = new_node;
        return true;
    }
    return false;
}


#undef T

template<unsigned char T>
TetrisNode const *generate_template(TetrisMap const &map)
{
    TetrisBlockStatus s =
    {
        T, map.width / 2, map.height - 2, 1
    };
    return get(s);
}
bool move_left(TetrisNode &node, TetrisMap const &map)
{
    if((node.data[0] & 1) || (node.data[1] & 1) || (node.data[2] & 1) || (node.data[3] & 1))
    {
        return false;
    }
    node.data[0] >>= 1;
    node.data[1] >>= 1;
    node.data[2] >>= 1;
    node.data[3] >>= 1;
    --node.status.x;
    return true;
}
bool move_right(TetrisNode &node, TetrisMap const &map)
{
    const int check = 1 << (map.width - 1);
    if((node.data[0] & check) || (node.data[1] & check) || (node.data[2] & check) || (node.data[3] & check))
    {
        return false;
    }
    node.data[0] <<= 1;
    node.data[1] <<= 1;
    node.data[2] <<= 1;
    node.data[3] <<= 1;
    ++node.status.x;
    return true;
}
bool move_down(TetrisNode &node, TetrisMap const &map)
{
    if(node.row <= 0 && node.data[3 + node.row] != 0)
    {
        return false;
    }
    --node.row;
    --node.status.y;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
namespace ege
{
    extern void mtsrand(unsigned int s);
    extern unsigned int mtirand();
    extern double mtdrand();
}
using ege::mtdrand;
using ege::mtirand;
using ege::mtsrand;

extern "C" void attach_init()
{
    TetrisOpertion op_O = 
    {
        create_O,
        generate_template<'O'>,
        nullptr,
        nullptr,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_I =
    {
        create_I,
        generate_template<'I'>,
        rotate_I12,
        rotate_I12,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_S =
    {
        create_S,
        generate_template<'S'>,
        rotate_S12,
        rotate_S12,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_Z =
    {
        create_Z,
        generate_template<'Z'>,
        rotate_Z12,
        rotate_Z12,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_L =
    {
        create_L,
        generate_template<'L'>,
        rotate_L14,
        rotate_L12,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_J =
    {
        create_J,
        generate_template<'J'>,
        rotate_J14,
        rotate_J12,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_T =
    {
        create_T,
        generate_template<'T'>,
        rotate_T14,
        rotate_T12,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    op.insert(std::make_pair('O', op_O));
    op.insert(std::make_pair('I', op_I));
    op.insert(std::make_pair('S', op_S));
    op.insert(std::make_pair('Z', op_Z));
    op.insert(std::make_pair('L', op_L));
    op.insert(std::make_pair('J', op_J));
    op.insert(std::make_pair('T', op_T));
    mtsrand(unsigned int(time(nullptr)));
}

int map_width = 0, map_height = 0;

bool init_ai(int w, int h)
{
    if(w == map_width && h == map_height)
    {
        return true;
    }
    if(w > 30 || h > 30)
    {
        return false;
    }
    node_cache.clear();
    map_width = w;
    map_height = h;

    unsigned char init[] = "OISZLJT";
    for(int i = 0; i < 7; ++i)
    {
        TetrisNode node = op[init[i]].create(w, h);
        node.op = op[init[i]];
        node_cache.insert(std::make_pair(node.status, node));
    }
    bool redo;
    TetrisMap map = {{}, 0, w, h};
    do
    {
        redo = false;
        for(auto it = node_cache.begin(); it != node_cache.end(); ++it)
        {
            TetrisNode &node = it->second;
#define D(func)\
/**/do{\
/**//**/TetrisNode copy =\
/**//**/{\
/**//**//**/node.status, node.op, {node.data[0], node.data[1], node.data[2], node.data[3]}, node.row\
/**//**/};\
/**//**/if(copy.op.func != nullptr && copy.op.func(copy, map))\
/**//**/{\
/**//**//**/auto result = node_cache.insert(std::make_pair(copy.status, copy));\
/**//**//**/if(node.func == nullptr)\
/**//**//**/{\
/**//**//**//**/redo = true;\
/**//**//**//**/node.func = &result.first->second;\
/**//**//**/}\
/**//**/}\
/**/}\
/**/while(false)\
/**/
            D(rotate_clockwise);
            D(rotate_counterclockwise);
            D(rotate_opposite);
            D(move_left);
            D(move_right);
            D(move_down);
#undef D
        }
    }
    while(redo);
    return true;
}

void build_map(char board[], int w, int h, TetrisMap &map)
{
    memset(&map, 0, sizeof map);
    map.width = w;
    map.height = h;
    for(int y = 0, add = 0; y < h; ++y, add += w)
    {
        for(int x = 0; x < w; ++x)
        {
            if(board[x + add] == '1')
            {
                map.row[y] |= 1 << x;
                map.top = y + 1;
                ++map.count;
            }
        }
    }
}

int do_ai_run(TetrisNode const *node, TetrisMap const &map, TetrisMap const &old_map, size_t clear)
{
    return 0;
}

std::vector<std::vector<TetrisNode const *>> check_cache;

std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, int next[], size_t next_count)
{
    if(node == nullptr || !node->check(map))
    {
        return std::make_pair(nullptr, std::numeric_limits<int>::min());
    }
    if(check_cache.size() <= next_count)
    {
        check_cache.resize(next_count + 1);
    }
    std::vector<TetrisNode const *> &check = check_cache[next_count];
    check.clear();
    TetrisNode const *rotate = node;
    do
    {
        check.push_back(rotate);
        TetrisNode const *left = rotate->move_left;
        while(left != nullptr && (left->row >= map.top || left->check(map)))
        {
            check.push_back(left);
            left = left->move_left;
        }
        TetrisNode const *right = rotate->move_right;
        while(right != nullptr && (right->row >= map.top || right->check(map)))
        {
            check.push_back(right);
            right = right->move_right;
        }
        rotate = rotate->rotate_counterclockwise;
    }
    while(rotate != nullptr  && rotate != node && (rotate->row >= map.top || rotate->check(map)));
    int score = std::numeric_limits<int>::min();
    TetrisNode const *beat_node = node;
    size_t best = 0;
    TetrisMap map_copy;
    for(size_t i = 0; i < check.size(); ++i)
    {
        node = check[i];
        while(node->move_down != nullptr && (node->row >= map.top || node->move_down->check(map)))
        {
            node = node->move_down;
        }
        map_copy = map;
        size_t clear = node->attach(map_copy);
        int new_score = next_count == 0 ? do_ai_run(node, map_copy, map, clear) : do_ai(map_copy, op[*next].generate(map_copy), next + 1, next_count - 1).second;
        if(new_score > score)
        {
            best = i;
            beat_node = node;
            score = new_score;
        }
    }
    return std::make_pair(beat_node, score);
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

/*
 * board是一个boardW*boardH长度用01组成的字符串，原点于左下角，先行后列。
 * 例如8*3的场地实际形状：
 * 00000000
 * 00011001
 * 01111111
 * 则参数board的内容为："011111110001100100000000"。
 *
 * Piece参数使用字符 OISZLJT 及空格表示方块形状。
 * nextPiece为' '时表示无预览。
 * curR是方向朝向，1是初始方向，2是逆时针90度，3是180度，4是顺时针90度。
 * curX,curY的坐标，是以当前块4*4矩阵的上数第二行，右数第二列为基准，
 *     左下角为x=1,y=1；右下角为x=boardW,y=1；左上角为x=1,y=boardH
 *     具体方块形状参阅上一级目录下的pieces_orientations.jpg
 *
 * bestX,bestRotation 用于返回最优位置，与curX,curR的规则相同。
 *
 * 注意：方块操作次序规定为先旋转，再平移，最后放下。
 *       若中间有阻挡而AI程序没有判断则会导致错误摆放。
 *       该函数在出现新方块的时候被调用，一个方块调用一次。
 */
DECLSPEC_EXPORT int WINAPI AI(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, int* bestX, int* bestRotation)
{
    if(!init_ai(boardW, boardH))
    {
        return 0;
    }
    TetrisMap map;
    TetrisBlockStatus status =
    {
        curPiece, curX - 1, curY - 1, curR
    };
    int next_count = 0;
    int next;
    if(nextPiece != ' ')
    {
        next = nextPiece;
        next_count = 1;
    }

    build_map(board, boardW, boardH, map);
    auto result = do_ai(map, get(status), &next, next_count).first;

    if(result != nullptr)
    {
        *bestX = result->status.x + 1;
        *bestRotation = result->status.r;
    }

    return 0;
}

/*
 * path 用于接收操作过程并返回，操作字符集：
 *      'l': 左移一格
 *      'r': 右移一格
 *      'd': 下移一格
 *      'L': 左移到头
 *      'R': 右移到头
 *      'D': 下移到底（但不粘上，可继续移动）
 *      'z': 逆时针旋转
 *      'c': 顺时针旋转
 * 字符串末尾要加'\0'，表示落地操作（或硬降落）
 *
 * 本函数支持任意路径操作，若不需要此函数只想使用上面一个的话，则删掉本函数即可
 */
//DECLSPEC_EXPORT int WINAPI AIPath(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, char path[])
//{
//    return 0;
//}

#ifdef __cplusplus
}
#endif

void zzz_ai_run()
{
    int w = 10, h = 20;
    TetrisMap map =
    {
        {}, 0, w, h, 0
    };
    init_ai(w, h);
    clock_t start = clock();
    size_t lines = 0, piece = 0;
    while(true)
    {
        unsigned char const tetris[] = "OISZLJT";
        TetrisNode const *node = do_ai(map, op[tetris[size_t(mtdrand() * 7)]].generate(map), nullptr, 0).first;
        if(node != nullptr)
        {
            int clear = node->attach(map);
            lines += clear;
            ++piece;
            if(clock() - start > 10000)
            {
                start += 10000;
                printf("speed = %d rowa / s, %d piecea / s\n", lines / 10, piece / 10);
                lines = 0;
                piece = 0;
            }
        }
        else
        {
            map.count = 0;
            map.top = 0;
            memset(map.row, 0, sizeof map.row);
        }
    }
}


#ifndef WINVER                          // 指定要求的最低平台是 Windows Vista。
#define WINVER 0x0500           // 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif

#ifndef _WIN32_WINNT            // 指定要求的最低平台是 Windows Vista。
#define _WIN32_WINNT 0x0501     // 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头中排除极少使用的资料

// Windows 头文件:
#include <windows.h>

int wmain(unsigned int argc, wchar_t *argv[], wchar_t *eve[])
{
    attach_init();
    //zzz_ai_run();
    if(argc < 2)
    {
        return 0;
    }
    HMODULE hDll = LoadLibrary(argv[1]);
    if(hDll == nullptr)
    {
        return 0;
    }
    void *name = nullptr;
    name = GetProcAddress(hDll, "_Name@0");
    if(name == nullptr)
    {
        name = GetProcAddress(hDll, "Name@0");
    }
    if(name == nullptr)
    {
        name = GetProcAddress(hDll, "Name");
    }
    void *ai[2] = {};
    ai[0] = GetProcAddress(hDll, "_AIPath@36");
    if(ai[0] == NULL)
    {
        ai[0] = GetProcAddress(hDll, "AIPath@36");
    }
    if(ai[0] == NULL)
    {
        ai[0] = GetProcAddress(hDll, "AIPath");
    }
    ai[1] = GetProcAddress(hDll, "_AI@40");
    if(ai[1] == NULL)
    {
        ai[1] = GetProcAddress(hDll, "AI@40");
    }
    if(ai[1] == NULL)
    {
        ai[1] = GetProcAddress(hDll, "AI");
    }
    
    if(name == nullptr)
    {
        return 0;
    }
    int version = -1;
    for(int i = 0; i < sizeof ai / sizeof ai[0]; ++i)
    {
        if(ai[i] != nullptr)
        {
            version = i;
        }
    }
    if(version == -1)
    {
        return 0;
    }
    SetWindowTextA(GetConsoleWindow(), ((char *(*)())name)());
    int w = 10, h = 20;
    TetrisMap map =
    {
        {}, 0, w, h, 0
    };
    char *param_map = new char[w * h];
    char *path = new char[1024];
    init_ai(w, h);
    clock_t log_start = clock();
    clock_t log_time = log_start;
    clock_t log_new_time;

    clock_t log_interval = 1000;
    long long log_rows = 0, log_piece = 0;

    long long total_lines = 0;
    long long this_lines = 0;
    long long max_line = 0;
    long long game_count = 0;

    while(true)
    {
        unsigned char const tetris[] = "OISZLJT";
        TetrisNode const *node = op[tetris[size_t(mtdrand() * 7)]].generate(map);
        log_new_time = clock();
        if(log_new_time - log_time > log_interval)
        {
            printf("{\"time\":%.2lf,\"current\":%lld,\"rows_ps\":%lld,\"piece_ps\":%lld}\n", (log_new_time - log_start) / 1000., this_lines, log_rows * 1000 / log_interval, log_piece * 1000 / log_interval);
            log_time += log_interval;
            log_rows = 0;
            log_piece = 0;
        }
        if(!node->check(map))
        {
            total_lines += this_lines;
            if(this_lines > max_line)
            {
                max_line = this_lines;
            }
            ++game_count;
            printf("{\"avg\":%.2lf,\"max\":%lld,\"count\":%lld,\"current\":%lld}\n", game_count == 0 ? 0. : double(total_lines) / game_count, max_line, game_count, this_lines);
            this_lines = 0;
            map.count = 0;
            map.top = 0;
            memset(map.row, 0, sizeof map.row);
        }
        for(int y = 0; y < h; ++y)
        {
            int row = y * w;
            for(int x = 0; x < w; ++x)
            {
                param_map[x + row] = map.full(x, y) ? '1' : '0';
            }
        }
        if(version == 0)
        {
            memset(path, 0, 1024);
            typedef int(__stdcall *ai_run_t)(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, char path[]);
            ((ai_run_t)ai[version])(w, h, param_map, node->status.t, node->status.x + 1, node->status.y + 1, node->status.r, ' ', path);
            char *move = path, *move_end = path + 1024;
            while(move != move_end && *move != '\0')
            {
                switch(*move)
                {
                case 'l':
                    if(node->move_left && node->move_left->check(map))
                    {
                        node = node->move_left;
                    }
                    break;
                case 'r':
                    if(node->move_right && node->move_right->check(map))
                    {
                        node = node->move_right;
                    }
                    break;
                case 'd':
                    if(node->move_down && node->move_down->check(map))
                    {
                        node = node->move_down;
                    }
                    break;
                case 'L':
                    while(node->move_left && node->move_left->check(map))
                    {
                        node = node->move_left;
                    }
                    break;
                case 'R':
                    while(node->move_right && node->move_right->check(map))
                    {
                        node = node->move_right;
                    }
                    break;
                case 'D':
                    while(node->move_down && node->move_down->check(map))
                    {
                        node = node->move_down;
                    }
                    break;
                case 'z':
                    if(node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
                    {
                        node = node->rotate_counterclockwise;
                    }
                    break;
                case 'c':
                    if(node->rotate_clockwise && node->rotate_clockwise->check(map))
                    {
                        node = node->rotate_clockwise;
                    }
                    break;
                default:
                    move = move_end;
                    break;
                }
            }
        }
        else
        {
            typedef int(__stdcall *ai_run_t)(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, int *bestX, int *bestRotation);
            int best_x = node->status.x + 1, best_r = node->status.r;
            ((ai_run_t)ai[version])(w, h, param_map, node->status.t, best_x, node->status.y + 1, best_r, ' ', &best_x, &best_r);
            --best_x;
            int r = node->status.r;
            while(best_r > r && node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
            {
                ++r;
                node = node->rotate_counterclockwise;
            }
            while(best_r < r && node->rotate_clockwise && node->rotate_clockwise->check(map))
            {
                --r;
                node = node->rotate_clockwise;
            }
            while(best_x > node->status.x && node->move_right && node->move_right->check(map))
            {
                node = node->move_right;
            }
            while(best_x < node->status.x && node->move_left && node->move_left->check(map))
            {
                node = node->move_left;
            }
        }
        while(node->move_down && node->move_down->check(map))
        {
            node = node->move_down;
        }
        int clear = node->attach(map);
        this_lines += clear;
        log_rows += clear;
        ++log_piece;
    }
}