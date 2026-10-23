#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 版本号 */
#define C2_AI_API_VERSION 4

/* 玩家操作枚举 */
typedef enum {
    C2OP_NONE        = 0,
    C2OP_ROT_CW      = 1,
    C2OP_ROT_CCW     = 2,
    C2OP_ROT_180     = 3,
    C2OP_MOVE_LEFT   = 4,
    C2OP_MOVE_RIGHT  = 5,
    C2OP_DAS_LEFT    = 6,
    C2OP_DAS_RIGHT   = 7,
    C2OP_SOFT_DROP   = 8,
    C2OP_HARD_DROP   = 9,
    C2OP_SOFT_DROP_LOCK = 11  /* soft-drop-to-ground (piece does not lock) */
} C2Op;

/* AI 状态枚举 */
typedef enum {
    C2_STATUS_OK            = 0,  /* 正常返回操作 */
    C2_STATUS_SEARCHING     = 1,  /* 搜索中，返回 NONE 等待 */
    C2_STATUS_NO_MOVE       = 2,  /* 无可行操作（场地已死） */
    C2_STATUS_INVALID_INPUT = 3   /* 输入参数非法 */
} C2Status;

/* 方块种类（与 tetris_ai_runner TetrisKind 对齐） */
typedef enum {
    C2_KIND_NONE = -1,
    C2_KIND_I = 0,
    C2_KIND_T = 1,
    C2_KIND_S = 2,
    C2_KIND_Z = 3,
    C2_KIND_L = 4,
    C2_KIND_J = 5,
    C2_KIND_O = 6
} C2Kind;

/* C2StepInput.reserved[] 的 ABI 兼容字段索引。 */
typedef enum {
    C2_INPUT_SOFT_DROP = 0,       /* 0=禁用逐格软降/tuck，非0=启用 */
    C2_INPUT_SAFE = 1,            /* C2 evaluator safe rows；限制到 0..3 */
    C2_INPUT_PLANNING_BPM = 2,    /* 整棵推演的每分钟落块数；<=0 使用安全默认值 */
    C2_INPUT_COMBO_REMAINING_MS = 3 /* 根 combo 剩余时间，毫秒；限制到非负 */
} C2InputReservedIndex;

/* 单步输入 */
typedef struct {
    int32_t api_version;      /* 调用方填 C2_AI_API_VERSION */

    /* 棋盘：28行×10列，row_bits[0]=第0行（最底行），bit0=最左列，1=有块 */
    uint64_t row_bits[28];

    /* 当前块 */
    int32_t  cur_kind;        /* C2Kind */
    int32_t  cur_x;           /* 当前块中心 X（列，0-based） */
    int32_t  cur_y;           /* 当前块 Y（spawn Y 可变，表示入场位置） */
    int32_t  cur_r;           /* 当前旋转状态 0-3 */

    /* 下一块 */
    int32_t  next_kind;       /* C2Kind，-1=未知 */

    /* 攻击力放大参数（首版仅透传，evaluator 暂不使用） */
    float    attack_curve_scale;   /* GarbageCurveTuningProfile.attackCurveScale，默认 1.95 */
    float    pressure_blend;       /* 时间 blend 系数 0.0~1.0（0=低压，1=高压） */

    /* 预告垃圾行（待消化的攻击） */
    int32_t  incoming_garbage;

    /* combo 计数（当前连消数，用于 AI 评分） */
    int32_t  combo_count;
    int64_t  piece_serial;     /* 每个活动方块唯一 ID；换块时必须变化 */
    int64_t  board_gen;        /* 仅实际盘面变化时递增；结果消费必须匹配 */

    /* ABI 兼容配置字段：soft_drop、safe、planning BPM、combo 剩余毫秒。 */
    int32_t  reserved[4];
} C2StepInput;

#ifdef __cplusplus
static_assert(sizeof(C2StepInput) == 304, "C2StepInput ABI size changed");
static_assert(offsetof(C2StepInput, reserved) == 288, "C2StepInput reserved offset changed");
static_assert(C2_INPUT_SOFT_DROP == 0 && C2_INPUT_SAFE == 1
              && C2_INPUT_PLANNING_BPM == 2 && C2_INPUT_COMBO_REMAINING_MS == 3,
              "C2StepInput reserved slot ABI changed");
#endif

/* 单步输出 */
typedef struct {
    C2Op     next_op;   /* 下一个玩家操作 */
    C2Status status;    /* 状态码 */
    int32_t  reserved[4];
} C2StepOutput;

/* --- API 函数 --- */

int64_t c2ai_create(void);
void c2ai_destroy(int64_t handle);
void c2ai_step(int64_t handle, const C2StepInput* in, C2StepOutput* out);
void c2ai_set_mode(int64_t handle, int32_t mode);
void c2ai_reset_plan(int64_t handle);
/** Configure C2::iterate score range. 1-indexed ranks (lowest=1) in ascending
 *  sort order; inclusive on both ends. Values are clamped to [1, 7] with
 *  low <= high. Defaults to low=4, high=7 (average of top 4 of 7). */
void c2ai_set_iterate_range(int64_t handle, int32_t low, int32_t high);
/** Configure how the "next piece" preview string is built per step. Both args
 *  are treated as booleans (nonzero=true). The string passed to the engine is:
 *    virtual=1,preview=1 -> "?T?"   virtual=1,preview=0 -> "?"
 *    virtual=0,preview=1 -> "T"     virtual=0,preview=0 -> ""
 *  where T is the actual next-piece letter when known. Defaults to
 *  virtual=1, preview=1 which matches the historical "?T?" behavior. */
void c2ai_set_next_mode(int64_t handle, int32_t virtual_flag, int32_t preview_flag);
/** Stable UTF-8 identity of the algorithm and exact native source build. */
const char* c2ai_name(void);
const char* c2ai_version(void);

#ifdef __cplusplus
}
#endif
