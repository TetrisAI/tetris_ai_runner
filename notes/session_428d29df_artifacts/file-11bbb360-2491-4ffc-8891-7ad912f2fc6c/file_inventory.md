# File Inventory

## A. src/ 下所有 search_* 文件
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_aspin.cpp
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_aspin.h
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_path.h
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_path_node.cpp
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_path_node.h
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_simple.h
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_simple_node.cpp
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_simple_node.h
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_simulate.h
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_simulate_node.cpp
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_simulate_node.h
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_tag.h
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_tspin.cpp
- /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/src/search_tspin.h

## B. CMakeLists.txt 全文 (标注 search_* 引用)
注：`src/CMakeLists.txt` 和 `tests/CMakeLists.txt` 不存在，以下为根目录 `CMakeLists.txt` 的全文及标注。

【1】cmake_minimum_required(VERSION 3.12)
【2】project(tetris_ai)
【3】
【4】enable_language(CXX)
【5】enable_language(C)
【6】set(CMAKE_CXX_STANDARD 20)
【7】set(CMAKE_CXX_STANDARD_REQUIRED ON)
【8】set(CMAKE_CXX_EXTENSIONS OFF)
【9】if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
【10】    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -flto)
【11】endif()
【12】
【13】# 编译期 constexpr 评估 ops/steps 上限.
【14】# 用于 extreme_rule_diff 这类需要在编译期跑大量 group-by / hash 的 target.
【15】# 默认 96M (= 100663296), 二分实测 (GCC 12.2): 40M 失败, 44M 通过, 取 ~2.2x 余量.
【16】# 用户可通过 -DTETRIS_CONSTEXPR_OPS_LIMIT=<N> 覆盖.
【17】set(TETRIS_CONSTEXPR_OPS_LIMIT 100663296 CACHE STRING
【18】    "Maximum constexpr operations / steps for heavy compile-time tables (default 96M)")
【19】
【20】# ---------------------------------------------------------------------------
【21】# tetris_core: shared engine core (TetrisContext / TetrisNode tables / map ops).
【22】# Production targets only need this; oracle BFS searches live in tetris_oracle
【23】# below and are linked exclusively by diff harnesses.
【24】# ---------------------------------------------------------------------------
【25】add_library(tetris_core STATIC
【26】    core/tetris_core.cpp
【27】)
【28】target_include_directories(tetris_core PUBLIC core src)
【29】set_target_properties(tetris_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
【30】
【31】# ---------------------------------------------------------------------------
【32】# tetris_oracle: legacy master BFS searches (search_simple / search_path /   <-- [search_* reference]
【33】# search_simulate / search_tag). Consumed only by *_diff harnesses   <-- [search_* reference]
【34】# (oracle_diff / path_node_diff / simulate_node_diff / tag_node_diff /
【35】# extreme_rule_diff) and the tag_landpoint_collision_dump tool.
【36】# Production targets must NOT link or include it.
【37】# ---------------------------------------------------------------------------
【38】add_library(tetris_oracle STATIC
【39】    oracle/search_simple.cpp   <-- [search_* reference]
【40】    oracle/search_path.cpp   <-- [search_* reference]
【41】    oracle/search_simulate.cpp   <-- [search_* reference]
【42】    oracle/search_tag.cpp   <-- [search_* reference]
【43】)
【44】target_include_directories(tetris_oracle PUBLIC oracle)
【45】target_link_libraries(tetris_oracle PUBLIC tetris_core)
【46】set_target_properties(tetris_oracle PROPERTIES POSITION_INDEPENDENT_CODE ON)
【47】
【48】add_library(${PROJECT_NAME} SHARED
【49】    src/ai.cpp
【50】    src/ai_ax.cpp
【51】    src/ai_farter.cpp
【52】    src/ai_misaka.cpp
【53】    src/ai_tag.cpp
【54】    src/dllmain.c
【55】    src/integer_utils.cpp
【56】    src/rule_asrs.cpp
【57】    src/search_aspin.cpp   <-- [search_* reference]
【58】    src/search_tspin.cpp   <-- [search_* reference]
【59】    src/rule_botris.cpp
【60】    src/random.cpp
【61】    src/rule_c2.cpp
【62】    src/rule_qq.cpp
【63】    src/rule_srsx.cpp
【64】    src/rule_st.cpp
【65】    src/rule_tag.cpp
【66】    src/rule_toj.cpp
【67】    src/ai_zzz.cpp
【68】    src/rule_srs.cpp
【69】    src/search_path_node.cpp   <-- [search_* reference]
【70】    src/search_simple_node.cpp   <-- [search_* reference]
【71】    src/search_simulate_node.cpp   <-- [search_* reference]
【72】)
【73】target_include_directories(${PROJECT_NAME} PRIVATE src)
【74】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)
【75】
【76】
【77】project(tetris_ai_runner)
【78】add_library(${PROJECT_NAME} SHARED
【79】    src/ai.cpp
【80】    src/ai_ax.cpp
【81】    src/ai_farter.cpp
【82】    src/ai_misaka.cpp
【83】    src/ai_tag.cpp
【84】    src/dllmain.c
【85】    src/integer_utils.cpp
【86】    src/rule_asrs.cpp
【87】    src/search_tspin.cpp   <-- [search_* reference]
【88】    src/search_aspin.cpp   <-- [search_* reference]
【89】    src/rule_botris.cpp
【90】    src/random.cpp
【91】    src/rule_c2.cpp
【92】    src/rule_qq.cpp
【93】    src/rule_srsx.cpp
【94】    src/rule_st.cpp
【95】    src/rule_tag.cpp
【96】    src/rule_toj.cpp
【97】    src/ai_zzz.cpp
【98】    src/rule_srs.cpp
【99】    src/search_path_node.cpp   <-- [search_* reference]
【100】    src/search_simple_node.cpp   <-- [search_* reference]
【101】    src/search_simulate_node.cpp   <-- [search_* reference]
【102】)
【103】target_include_directories(${PROJECT_NAME} PRIVATE src)
【104】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)
【105】
【106】
【107】project(top_pso)
【108】add_executable(${PROJECT_NAME}
【109】    src/ai_zzz.cpp
【110】    src/integer_utils.cpp
【111】    src/random.cpp
【112】    src/rule_srs.cpp
【113】    src/search_tspin.cpp   <-- [search_* reference]
【114】    src/ppt_pso.cpp
【115】)
【116】target_include_directories(${PROJECT_NAME} PRIVATE src)
【117】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)
【118】
【119】project(botris)
【120】add_executable(${PROJECT_NAME}
【121】    src/ai_zzz.cpp
【122】    src/botris.cpp
【123】    src/integer_utils.cpp
【124】    src/random.cpp
【125】    src/rule_botris.cpp
【126】    src/search_aspin.cpp   <-- [search_* reference]
【127】)
【128】target_include_directories(${PROJECT_NAME} PRIVATE src)
【129】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)
【130】
【131】project(cmd_tris)
【132】add_executable(${PROJECT_NAME}
【133】        src/ai_zzz.cpp
【134】        src/cmd_tris.cpp
【135】        src/integer_utils.cpp
【136】        src/random.cpp
【137】        src/rule_botris.cpp
【138】        src/search_aspin.cpp   <-- [search_* reference]
【139】)
【140】target_include_directories(${PROJECT_NAME} PRIVATE src)
【141】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)
【142】
【143】project(perft_movegen)
【144】add_executable(${PROJECT_NAME}
【145】    tests/perft_movegen.cpp
【146】)
【147】target_include_directories(${PROJECT_NAME} PRIVATE src)
【148】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)
【149】
【150】project(oracle_diff)
【151】add_executable(${PROJECT_NAME}
【152】    src/ai_zzz.cpp
【153】    src/integer_utils.cpp
【154】    src/random.cpp
【155】    src/rule_srs.cpp
【156】    src/search_tspin.cpp   <-- [search_* reference]
【157】    tests/oracle_diff.cpp
【158】)
【159】target_include_directories(${PROJECT_NAME} PRIVATE src oracle)
【160】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_oracle)
【161】
【162】project(path_node_diff)
【163】add_executable(${PROJECT_NAME}
【164】    src/ai_zzz.cpp
【165】    src/integer_utils.cpp
【166】    src/random.cpp
【167】    src/rule_srs.cpp
【168】    src/search_tspin.cpp   <-- [search_* reference]
【169】    src/search_path_node.cpp   <-- [search_* reference]
【170】    tests/path_node_diff.cpp
【171】)
【172】target_include_directories(${PROJECT_NAME} PRIVATE src oracle)
【173】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_oracle)
【174】
【175】project(simulate_node_diff)
【176】add_executable(${PROJECT_NAME}
【177】    src/ai_zzz.cpp
【178】    src/integer_utils.cpp
【179】    src/random.cpp
【180】    src/rule_srs.cpp
【181】    src/search_tspin.cpp   <-- [search_* reference]
【182】    src/search_path_node.cpp   <-- [search_* reference]
【183】    src/search_simulate_node.cpp   <-- [search_* reference]
【184】    tests/simulate_node_diff.cpp
【185】)
【186】target_include_directories(${PROJECT_NAME} PRIVATE src oracle)
【187】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_oracle)
【188】
【189】project(tag_node_diff)
【190】add_executable(${PROJECT_NAME}
【191】    src/ai_zzz.cpp
【192】    src/integer_utils.cpp
【193】    src/random.cpp
【194】    src/rule_srs.cpp
【195】    src/search_tspin.cpp   <-- [search_* reference]
【196】    src/search_path_node.cpp   <-- [search_* reference]
【197】    tests/tag_node_diff.cpp
【198】)
【199】target_include_directories(${PROJECT_NAME} PRIVATE src oracle)
【200】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_oracle)
【201】
【202】project(tag_landpoint_collision_dump)
【203】add_executable(${PROJECT_NAME}
【204】    src/ai_zzz.cpp
【205】    src/integer_utils.cpp
【206】    src/random.cpp
【207】    src/rule_srs.cpp
【208】    src/search_tspin.cpp   <-- [search_* reference]
【209】    tools/tag_landpoint_collision_dump.cpp
【210】)
【211】target_include_directories(${PROJECT_NAME} PRIVATE src oracle)
【212】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_oracle)
【213】
【214】project(aspin_dump)
【215】add_executable(${PROJECT_NAME}
【216】    src/ai_zzz.cpp
【217】    src/integer_utils.cpp
【218】    src/random.cpp
【219】    src/rule_botris.cpp
【220】    src/search_aspin.cpp   <-- [search_* reference]
【221】    tools/aspin_dump.cpp
【222】)
【223】target_include_directories(${PROJECT_NAME} PRIVATE src)
【224】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)
【225】
【226】project(extreme_rule_diff)
【227】add_executable(${PROJECT_NAME}
【228】    src/integer_utils.cpp
【229】    src/random.cpp
【230】    src/rule_extreme.cpp
【231】    tests/extreme_rule_diff.cpp
【232】)
【233】target_include_directories(${PROJECT_NAME} PRIVATE src oracle)
【234】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_oracle)
【235】# 极端规则下 piece 'I'/'J' 的 kFilteredTable<> constexpr 推导
【236】# 单 piece 估算 ~9~10 cell × R=8~10 旋转 × W=13 × H=30, 编译期 group-by
【237】# (compute_key + bubble sort + hash probe) 远超默认 33M ops 上限.
【238】# 二分实测 (GCC 12.2): 40M 失败, 44M 通过; 取 ~2.2x 余量, 使用 96M 默认值.
【239】# 默认值与覆盖入口见顶部 TETRIS_CONSTEXPR_OPS_LIMIT cache option.
【240】# MSVC 注意: /constexpr:steps<N> 紧贴数字, 中间没有空格 (官方文档语法).
【241】# 该分支当前没有 MSVC 实测环境, 按官方文档语法编写, 实际启用时需要再验证.
【242】target_compile_options(${PROJECT_NAME} PRIVATE
【243】    $<$<CXX_COMPILER_ID:GNU>:-fconstexpr-ops-limit=${TETRIS_CONSTEXPR_OPS_LIMIT}>
【244】    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fconstexpr-steps=${TETRIS_CONSTEXPR_OPS_LIMIT}>
【245】    $<$<CXX_COMPILER_ID:MSVC>:/constexpr:steps${TETRIS_CONSTEXPR_OPS_LIMIT}>
【246】)
【247】
【248】
【249】# ---------------------------------------------------------------------------
【250】# dummy_strategy_compile_check: compile-only sanity target for the third-party
【251】# extension example documented in `docs/extending_search.md`.
【252】# Instantiates Searcher<DummyStrategy, NoSpinHook, rule_srs::rule_spec> against
【253】# the public movegen surface so that any future drift between the docs/sample
【254】# and the live API is caught at build time.
【255】# ---------------------------------------------------------------------------
【256】project(dummy_strategy_compile_check)
【257】add_executable(${PROJECT_NAME}
【258】    examples/dummy_strategy_compile.cpp
【259】    src/integer_utils.cpp
【260】    src/random.cpp
【261】    src/rule_srs.cpp
【262】    src/search_tspin.cpp   <-- [search_* reference]
【263】)
【264】target_include_directories(${PROJECT_NAME} PRIVATE src examples)
【265】target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)
【266】

## C. 包含 src/search_*_node.{h,cpp} 和 src/search_*.cc 的文件
- **src/search_path_node.h**:
  - tetris_ai_runner/src/ai.cpp:13: `#include "search_path_node.h"`
  - tetris_ai_runner/src/search_path_node.cpp:1: `#include "search_path_node.h"`
  - tetris_ai_runner/tests/path_node_diff.cpp:11: `#include "../src/search_path_node.h"`
- **src/search_simple_node.h**:
  - tetris_ai_runner/src/ai.cpp:12: `#include "search_simple_node.h"`
  - tetris_ai_runner/src/search_simple_node.cpp:1: `#include "search_simple_node.h"`
- **src/search_simulate_node.h**:
  - tetris_ai_runner/src/ai.cpp:14: `#include "search_simulate_node.h"`
  - tetris_ai_runner/src/search_simulate_node.cpp:1: `#include "search_simulate_node.h"`
  - tetris_ai_runner/tests/simulate_node_diff.cpp:10: `#include "../src/search_simulate_node.h"`
- **src/search_aspin.cpp**:
  - tetris_ai_runner/src/search_aspin.cpp:1: `#include "search_aspin.h"` (Indirect, searching for .cpp inclusion found no results)
- **src/search_tspin.cpp**:
  - (No direct inclusion found)

## D. node_mark_ / node_mark_filtered_ 出现位置
### src/search_aspin.cpp
【27】    //pImpl: Search 旧成员 (node_mark_ / node_mark_filtered_ / node_search_ / land_point_cache_)
【28】    //  全部删除, 仅保留 config_ / context_ 指针和一个 Backend. 老 search_aspin.h 内部
--
【54】        //commit 5: node_mark_ / node_mark_filtered_ 不再被使用, 仅為保 ABI 留字段.
【55】        //  保留 init 调用是因为 TetrisNodeMark 内部分配 vector, 老外部代码若有
--
【57】        node_mark_.init(context->node_max());
【58】        node_mark_filtered_.init(context->node_max());

### src/search_aspin.h
【70】        m_tetris::TetrisNodeMark node_mark_;
【71】        m_tetris::TetrisNodeMarkFiltered node_mark_filtered_;

### 整个仓库 grep (src/, tests/, tools/, oracle/)
#### src/
- **src/search_simulate_node.h**:
  - 【89】        m_tetris::TetrisNodeMarkFiltered node_mark_filtered_;
- **src/search_path_node.cpp**:
  - 【15】        node_mark_filtered_.init(context->node_max());
  - 【25】        node_mark_filtered_.clear();
  - 【37】                if (node_mark_filtered_.mark(drop_node))
  - 【60】                        if (dedup_.node_mark_.mark(check_node))
  - 【69】            bfs::TwentyGravityCollector visitor{&land_point_cache_, &node_mark_filtered_};
  - 【74】            bfs::OneGravityCollector visitor{&land_point_cache_, &node_mark_filtered_};
- **src/bfs_engine.h**:
  - 【157】        m_tetris::TetrisNodeMark node_mark_;
  - 【160】            node_mark_.init(context->node_max());
  - 【164】            node_mark_.clear();
  - 【168】            return node_mark_.mark(node);
  - 【175】        m_tetris::TetrisNodeMark node_mark_;
  - 【178】            node_mark_.init(context->node_max());
  - 【182】            node_mark_.clear();
  - 【186】            return node_mark_.set(node, from, action);
  - 【190】            return node_mark_.get(node);
- **src/search_tspin.h**:
  - 【87】        m_tetris::TetrisNodeMark node_mark_;
  - 【88】        m_tetris::TetrisNodeMarkFiltered node_mark_filtered_;
- **src/search_simulate_node.cpp**:
  - 【15】        node_mark_filtered_.init(context->node_max());
  - 【26】        node_mark_filtered_.clear();
  - 【33】                if (node_mark_filtered_.mark(land_point))
  - 【68】                                if (node_mark_filtered_.mark(node_check))
  - 【84】                                if (node_mark_filtered_.mark(node_check))
  - 【98】            bfs::OneGravityCollector visitor{&land_point_cache_, &node_mark_filtered_};
- **src/search_tspin.cpp**: (大量出现，略)

#### tests/
- **tests/extreme_rule_diff.cpp**:
  - 【7】//     非 SRS / 非 4-cell / R_count > 4 配置下与 master node_mark_filtered_
  - 【61】    //  search_simple::Search 走 BFS 同时维护 node_mark_filtered_, 但其底层

#### oracle/
- **oracle/search_simple.h**:
  - 【13】        m_tetris::TetrisNodeMarkFiltered node_mark_filtered_;
- **oracle/search_path.cpp**:
  - 【10】        node_mark_.init(context->node_max());
  - 【11】        node_mark_filtered_.init(context->node_max());
  - 【20】        node_mark_.clear();
  - 【40】        node_mark_.set(node, nullptr, '\0');
  - (及其他 set/mark/clear 调用)
- **oracle/search_simple.cpp**:
  - 【12】        node_mark_filtered_.init(context->node_max());
  - 【72】        node_mark_filtered_.clear();
  - 【77】                push(node_mark_filtered_, land_point_cache_, (*cit)->drop(map));

## E. tests/path_node_diff.cpp / tests/simulate_node_diff.cpp / tests/tag_node_diff.cpp
### 头文件 #include 段 (前 30 行)
#### tests/path_node_diff.cpp
```cpp
【1】// path_node_diff: assert search_path_node::Search reproduces the land_point set
【2】// of oracle search_path::Search bit-for-bit (compared via the index_filtered set).
【3】//
【4】// Operates entirely in the oracle pointer-graph world: both candidate and reference
【5】// walk m_tetris::TetrisNode indices over the same m_tetris::TetrisMap.
【6】
【7】#include "../oracle/search_path.h"
【8】#include "../core/tetris_core.h"
【9】#include "../src/ai_zzz.h"
【10】#include "../src/rule_srs.h"
【11】#include "../src/search_path_node.h"
【12】#include "../src/search_tspin.h"
【13】
【14】#include <algorithm>
...
```

#### tests/simulate_node_diff.cpp
```cpp
【1】// simulate_node_diff: assert search_simulate_node::Search reproduces the
【2】// land_point set of oracle search_simulate::Search bit-for-bit (compared via
【3】// the index_filtered set), and that 1G make_path output replays back to the
【4】// requested land_point under the oracle pointer-graph world.
【5】
【6】#include "../oracle/search_simulate.h"
【7】#include "../core/tetris_core.h"
【8】#include "../src/ai_zzz.h"
【9】#include "../src/rule_srs.h"
【10】#include "../src/search_simulate_node.h"
【11】#include "../src/search_tspin.h"
...
```

#### tests/tag_node_diff.cpp
```cpp
【1】// tag_node_diff: assert TagStrategy<TSpinHook, rule_srs::rule_spec> reproduces
【2】// the land_point set of oracle search_tag::Search bit-for-bit, comparing not
【3】// only index_filtered but also (is_last_rotate, is_ready). For 1G cases, also
【4】// replay make_path.
【5】
【6】#include "../oracle/search_tag.h"
【7】#include "../core/tetris_core.h"
【8】#include "../src/ai_zzz.h"
【9】#include "../src/movegen_hook.h"
【10】#include "../src/movegen_searcher.h"
【11】#include "../src/rule_srs.h"
【12】#include "../src/search_tag.h"
【13】#include "../src/search_tspin.h"
...
```

### CMake targets (add_executable)
- **path_node_diff** (CMakeLists.txt:162-171):
  ```cmake
  【163】add_executable(${PROJECT_NAME}
  【164】    src/ai_zzz.cpp
  【165】    src/integer_utils.cpp
  【166】    src/random.cpp
  【167】    src/rule_srs.cpp
  【168】    src/search_tspin.cpp
  【169】    src/search_path_node.cpp
  【170】    tests/path_node_diff.cpp
  【171】)
  ```
- **simulate_node_diff** (CMakeLists.txt:175-185):
  ```cmake
  【176】add_executable(${PROJECT_NAME}
  【177】    src/ai_zzz.cpp
  【178】    src/integer_utils.cpp
  【179】    src/random.cpp
  【180】    src/rule_srs.cpp
  【181】    src/search_tspin.cpp
  【182】    src/search_path_node.cpp
  【183】    src/search_simulate_node.cpp
  【184】    tests/simulate_node_diff.cpp
  【185】)
  ```
- **tag_node_diff** (CMakeLists.txt:189-198):
  ```cmake
  【190】add_executable(${PROJECT_NAME}
  【191】    src/ai_zzz.cpp
  【192】    src/integer_utils.cpp
  【193】    src/random.cpp
  【194】    src/rule_srs.cpp
  【195】    src/search_tspin.cpp
  【196】    src/search_path_node.cpp
  【197】    tests/tag_node_diff.cpp
  【198】)
  ```

## F. oracle/ 目录盘点
- **是否有 search_*_node.{h,cpp} 同名文件？**: 否。
- **oracle/ 全部文件路径**:
  - tetris_ai_runner/oracle/search_path.cpp
  - tetris_ai_runner/oracle/search_path.h
  - tetris_ai_runner/oracle/search_simple.cpp
  - tetris_ai_runner/oracle/search_simple.h
  - tetris_ai_runner/oracle/search_simulate.cpp
  - tetris_ai_runner/oracle/search_simulate.h
  - tetris_ai_runner/oracle/search_tag.cpp
  - tetris_ai_runner/oracle/search_tag.h
