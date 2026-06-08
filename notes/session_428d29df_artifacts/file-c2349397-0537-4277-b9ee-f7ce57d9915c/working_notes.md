# flip-bits 文件结构统一 - 工作记录

## 用户当前指令
> 调整文件结构, 不损失功能, 清理死字段
> 移除 search_*_node.* search_*.cc, 让 search 文件结构统一

## 事实核对 (源自 file_inventory.md)

### 1) 没有 .cc 文件
全仓 search_*.cc grep 结果: **零命中**. 项目没有任何 .cc 后缀源文件.

### 2) search_*_node.* 不是死代码副本, 是生产代码
src/CMakeLists 中, 这三个文件:
- src/search_path_node.cpp
- src/search_simple_node.cpp
- src/search_simulate_node.cpp

被以下 4 个 **生产 / 测试** target 链接:
- `tetris_ai` (SHARED 主库)
- `tetris_ai_runner` (SHARED 主库)
- `path_node_diff` (测试)
- `simulate_node_diff` (测试)
- `tag_node_diff` (链接 search_path_node.cpp)

并被 src/ai.cpp 直接 #include:
- ai.cpp:12 #include "search_simple_node.h"
- ai.cpp:13 #include "search_path_node.h"
- ai.cpp:14 #include "search_simulate_node.h"

### 3) oracle 副本不在 src/, 在 oracle/
真正的 master-pointer oracle 副本位于 oracle/, 由 tetris_oracle static 库消费:
- oracle/search_simple.cpp
- oracle/search_path.cpp
- oracle/search_simulate.cpp
- oracle/search_tag.cpp

### 4) src/ 下位板原生 strategy header (header-only)
- src/search_path.h
- src/search_simulate.h
- src/search_tag.h
- src/search_simple.h
- src/search_aspin.{h,cpp} (façade)
- src/search_tspin.{h,cpp} (legacy, 大量被复用)

## 核心冲突
"移除 src/search_*_node.*" 等价于:
1. ai.cpp 必须切换到位板版本的 search_path / search_simulate / search_simple
2. *_node_diff 测试集合 (path_node_diff / simulate_node_diff / tag_node_diff) 全部失去 candidate, 要么删除要么重写为对 strategy 版本
3. 这是一个 **行为迁移** 而非纯文件移动, 不是"不损失功能"

## node_mark_ / node_mark_filtered_ 死字段
search_aspin.cpp:54-58 注释自承死字段, search/make_path 调用面无访问.
search_aspin.h:70-71 字段声明.

## 建议向用户确认的问题
本轮指令与代码实情不自洽: search_*_node.* 是 ai.cpp 在用的生产路径, 不是 oracle 副本; 没有 .cc 文件. 需用户确认范围.
