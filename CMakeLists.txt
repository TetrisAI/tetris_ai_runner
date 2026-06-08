cmake_minimum_required(VERSION 3.12)
project(tetris_ai)

enable_language(CXX)
enable_language(C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -flto)
endif()

# ctest 集成入口: 让七 diff driver + extreme_rule_diff + perft_movegen 一键跑.
enable_testing()

# 编译期 constexpr 评估 ops/steps 上限.
# 用于 extreme_rule_diff 这类需要在编译期跑大量 group-by / hash 的 target.
# 默认 96M (= 100663296), 二分实测 (GCC 12.2): 40M 失败, 44M 通过, 取 ~2.2x 余量.
# 用户可通过 -DTETRIS_CONSTEXPR_OPS_LIMIT=<N> 覆盖.
set(TETRIS_CONSTEXPR_OPS_LIMIT 100663296 CACHE STRING
    "Maximum constexpr operations / steps for heavy compile-time tables (default 96M)")

# ---------------------------------------------------------------------------
# ORACLE mode: -DORACLE=ON 时，五个生产 target 使用 oracle/ 里的指针图实现
# (与 flip-bits 分支行为完全等价); 默认 OFF，使用 src/ 里的位板实现.
# ---------------------------------------------------------------------------
option(ORACLE "Build production targets using oracle/ pointer-graph implementation" OFF)

# ---------------------------------------------------------------------------
# tetris_core: shared engine core (TetrisContext / TetrisNode tables / map ops).
# Used by production targets in bitboard mode (ORACLE=OFF).
# In ORACLE=ON mode, production targets embed oracle/tetris_core.cpp directly
# and do NOT link this library.
# ---------------------------------------------------------------------------
add_library(tetris_core STATIC
    src/tetris_core.cpp
)
target_include_directories(tetris_core PUBLIC src)
set_target_properties(tetris_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# tetris_oracle: legacy master BFS searches (search_simple / search_path /
# search_simulate / search_tag / search_aspin / search_cautious / search_tspin).
# Consumed only by *_diff harnesses and the tag_landpoint_collision_dump tool.
# Production targets must NOT link or include it (regardless of ORACLE mode).
# ---------------------------------------------------------------------------
add_library(tetris_oracle STATIC
    oracle/tetris_core.cpp
    oracle/rule_srs.cpp
    oracle/search_simple.cpp
    oracle/search_path.cpp
    oracle/search_simulate.cpp
    oracle/search_tag.cpp
    oracle/search_aspin.cpp
    oracle/search_cautious.cpp
    oracle/search_tspin.cpp
)
target_include_directories(tetris_oracle PUBLIC oracle)
target_link_libraries(tetris_oracle PUBLIC tetris_core)
set_target_properties(tetris_oracle PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(ORACLE)
    # -----------------------------------------------------------------------
    # ORACLE=ON: 指针图实现，源文件与 flip-bits 分支完全对齐.
    # -----------------------------------------------------------------------
    set(_AI_SHARED_SRCS
        oracle/tetris_core.cpp
        oracle/rule_asrs.cpp
        oracle/rule_botris.cpp
        oracle/rule_c2.cpp
        oracle/rule_ppt.cpp
        oracle/rule_qq.cpp
        oracle/rule_srs.cpp
        oracle/rule_srsx.cpp
        oracle/rule_st.cpp
        oracle/rule_tag.cpp
        oracle/rule_toj.cpp
        oracle/search_aspin.cpp
        oracle/search_cautious.cpp
        oracle/search_path.cpp
        oracle/search_simple.cpp
        oracle/search_simulate.cpp
        oracle/search_tag.cpp
        oracle/search_tspin.cpp
        oracle/ai_ax.cpp
        oracle/ai_farter.cpp
        oracle/ai_misaka.cpp
        oracle/ai_tag.cpp
        oracle/ai_zzz.cpp
        src/ai.cpp
        src/dllmain.c
        src/integer_utils.cpp
        src/random.cpp
    )

    add_library(tetris_ai SHARED ${_AI_SHARED_SRCS})
    target_include_directories(tetris_ai PRIVATE oracle src)

    project(tetris_ai_runner)
    add_library(tetris_ai_runner SHARED ${_AI_SHARED_SRCS})
    target_include_directories(tetris_ai_runner PRIVATE oracle src)

    project(top_pso)
    add_executable(top_pso
        src/ai_zzz.cpp
        src/integer_utils.cpp
        src/random.cpp
        src/ppt_pso.cpp
    )
    target_include_directories(top_pso PRIVATE src)
    target_link_libraries(top_pso PRIVATE tetris_core)

    project(botris)
    add_executable(botris
        src/ai_zzz.cpp
        src/botris.cpp
        src/integer_utils.cpp
        src/random.cpp
    )
    target_include_directories(botris PRIVATE src)
    target_link_libraries(botris PRIVATE tetris_core)

    project(cmd_tris)
    add_executable(cmd_tris
        src/ai_zzz.cpp
        src/cmd_tris.cpp
        src/integer_utils.cpp
        src/random.cpp
    )
    target_include_directories(cmd_tris PRIVATE src)
    target_link_libraries(cmd_tris PRIVATE tetris_core)

else()
    # -----------------------------------------------------------------------
    # ORACLE=OFF (default): 位板实现.
    # -----------------------------------------------------------------------
    add_library(${PROJECT_NAME} SHARED
        src/ai.cpp
        src/ai_ax.cpp
        src/ai_farter.cpp
        src/ai_misaka.cpp
        src/ai_tag.cpp
        src/dllmain.c
        src/integer_utils.cpp
        src/random.cpp
        src/ai_zzz.cpp
    )
    target_include_directories(${PROJECT_NAME} PRIVATE src)
    target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)

    project(tetris_ai_runner)
    add_library(${PROJECT_NAME} SHARED
        src/ai.cpp
        src/ai_ax.cpp
        src/ai_farter.cpp
        src/ai_misaka.cpp
        src/ai_tag.cpp
        src/dllmain.c
        src/integer_utils.cpp
        src/random.cpp
        src/ai_zzz.cpp
    )
    target_include_directories(${PROJECT_NAME} PRIVATE src)
    target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)

    project(top_pso)
    add_executable(${PROJECT_NAME}
        src/ai_zzz.cpp
        src/integer_utils.cpp
        src/random.cpp
        src/ppt_pso.cpp
    )
    target_include_directories(${PROJECT_NAME} PRIVATE src)
    target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)

    project(botris)
    add_executable(${PROJECT_NAME}
        src/ai_zzz.cpp
        src/botris.cpp
        src/integer_utils.cpp
        src/random.cpp
    )
    target_include_directories(${PROJECT_NAME} PRIVATE src)
    target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)

    project(cmd_tris)
    add_executable(${PROJECT_NAME}
            src/ai_zzz.cpp
            src/cmd_tris.cpp
            src/integer_utils.cpp
            src/random.cpp
    )
    target_include_directories(${PROJECT_NAME} PRIVATE src)
    target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)

endif() # ORACLE

project(perft_movegen)
add_executable(${PROJECT_NAME}
    test/perft_movegen.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)

# ---------------------------------------------------------------------------
# Diff harness 七 driver: 七个 search 形态各自把 oracle/ master-graph BFS 与
# src/ 位板 strategy 在共享 fixture 上对拍, 共用 test/_diff_harness.h 抽出
# 的棋面 / 元组 / unified-diff 报告 / xfail 列表机制.
#
# 目标: ctest --test-dir build 一键全跑, 全绿守门 (xfail 不计 fail; simple_diff
# 始终 pass). 旧 oracle_diff target 被 tspin_diff 取代, 不再列入.
# ---------------------------------------------------------------------------
foreach(diff_driver IN ITEMS
    path_diff
    tag_diff
    simulate_diff
    tspin_diff
    aspin_diff
    cautious_diff
    simple_diff
)
    add_executable(${diff_driver}
        src/ai_zzz.cpp
        src/integer_utils.cpp
        src/random.cpp
        test/${diff_driver}.cpp
    )
    target_include_directories(${diff_driver} PRIVATE src oracle)
    target_link_libraries(${diff_driver} PRIVATE tetris_oracle)
endforeach()

project(tag_landpoint_collision_dump)
add_executable(${PROJECT_NAME}
    src/ai_zzz.cpp
    src/integer_utils.cpp
    src/random.cpp
    tools/tag_landpoint_collision_dump.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src oracle)
target_link_libraries(${PROJECT_NAME} PRIVATE tetris_oracle)

project(aspin_dump)
add_executable(${PROJECT_NAME}
    src/ai_zzz.cpp
    src/integer_utils.cpp
    src/random.cpp
    tools/aspin_dump.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)

project(extreme_rule_diff)
add_executable(${PROJECT_NAME}
    src/integer_utils.cpp
    src/random.cpp
    test/extreme_rule_diff.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src oracle)
target_link_libraries(${PROJECT_NAME} PRIVATE tetris_oracle)
# 极端规则下 piece 'I'/'J' 的 kFilteredTable<> constexpr 推导
# 单 piece 估算 ~9~10 cell × R=8~10 旋转 × W=13 × H=30, 编译期 group-by
# (compute_key + bubble sort + hash probe) 远超默认 33M ops 上限.
# 二分实测 (GCC 12.2): 40M 失败, 44M 通过; 取 ~2.2x 余量, 使用 96M 默认值.
# 默认值与覆盖入口见顶部 TETRIS_CONSTEXPR_OPS_LIMIT cache option.
# MSVC 注意: /constexpr:steps<N> 紧贴数字, 中间没有空格 (官方文档语法).
# 该分支当前没有 MSVC 实测环境, 按官方文档语法编写, 实际启用时需要再验证.
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-fconstexpr-ops-limit=${TETRIS_CONSTEXPR_OPS_LIMIT}>
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fconstexpr-steps=${TETRIS_CONSTEXPR_OPS_LIMIT}>
    $<$<CXX_COMPILER_ID:MSVC>:/constexpr:steps${TETRIS_CONSTEXPR_OPS_LIMIT}>
)


# ---------------------------------------------------------------------------
# dummy_strategy_compile_check: compile-only sanity target for the third-party
# extension example documented in `docs/extending_search.md`.
# Instantiates Searcher<DummyStrategy, NoSpinHook, rule_srs::rule_spec> against
# the public movegen surface so that any future drift between the docs/sample
# and the live API is caught at build time.
# ---------------------------------------------------------------------------
project(dummy_strategy_compile_check)
add_executable(${PROJECT_NAME}
    examples/dummy_strategy_compile.cpp
    src/integer_utils.cpp
    src/random.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src examples)
target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)

# ---------------------------------------------------------------------------
# policy_deduction_test: 编译期 static_assert 验证 DeduceSpinPolicy 推导链
# 以及 TetrisEngine2 实例化等价性.  纯编译验证, 运行时 main 直接返回 0.
# ---------------------------------------------------------------------------
project(policy_deduction_test)
add_executable(${PROJECT_NAME}
    src/ai_zzz.cpp
    src/integer_utils.cpp
    src/random.cpp
    test/policy_deduction_test.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)

# ---------------------------------------------------------------------------
# typed_dispatch_test: 验证 for_each_typed_r dispatch 正确性
# 以及 TypedLandPoint<T,R> 编译期常量字段. 纯头文件依赖, 无额外 .cpp.
# ---------------------------------------------------------------------------
project(typed_dispatch_test)
add_executable(${PROJECT_NAME}
    test/typed_dispatch_test.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)

# ---------------------------------------------------------------------------
# bb_node_test: 验证 BBNodeBase / BBNode<Spec,T,R,SpinT> 的编译期字段、
# 坐标语义（row() = y - kHeight + 1）以及 monostate 偏特化无 spin 成员.
# 依赖 rule_srs（需要 integer_utils / random / tetris_core）.
# ---------------------------------------------------------------------------
project(bb_node_test)
add_executable(${PROJECT_NAME}
    src/integer_utils.cpp
    src/random.cpp
    test/bb_node_test.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)

# ---------------------------------------------------------------------------
# ctest 注册. 名字前缀 tetris_diff_* 让 `ctest -R tetris_diff_` 能一键勾选.
#
# 严格 driver: binary exit 非零即 ctest 失败 (默认 PASS_REGULAR_EXPRESSION /
# WILL_FAIL 都不设). simple_diff 已在自身 main 内 strict=false, binary 永
# 远 exit 0, 用 WILL_FAIL FALSE 显式标注语义 (ctest 默认就视为 PASS).
# ---------------------------------------------------------------------------
foreach(diff_driver IN ITEMS
    path_diff
    tag_diff
    simulate_diff
    tspin_diff
    aspin_diff
    cautious_diff
    simple_diff
    extreme_rule_diff
)
    add_test(NAME tetris_diff_${diff_driver} COMMAND ${diff_driver})
endforeach()
add_test(NAME tetris_diff_perft_movegen COMMAND perft_movegen all)
add_test(NAME tetris_policy_deduction   COMMAND policy_deduction_test)
add_test(NAME tetris_typed_dispatch     COMMAND typed_dispatch_test)
add_test(NAME tetris_bb_node           COMMAND bb_node_test)
set_tests_properties(tetris_diff_simple_diff PROPERTIES WILL_FAIL FALSE)
