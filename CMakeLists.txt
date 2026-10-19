cmake_minimum_required(VERSION 3.12)
project(tetris_ai)

enable_language(CXX)
enable_language(C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# MSVC 不认识 -flto; string(APPEND) 避免已有 CMAKE_CXX_FLAGS 时拼出 ';'.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
    string(APPEND CMAKE_CXX_FLAGS " -flto")
endif()
find_package(Threads REQUIRED)

# ctest 集成入口: 让七 diff driver + extreme_rule_diff + perft_movegen 一键跑.
enable_testing()

# 编译期 constexpr 评估 ops/steps 上限.
# 用于 extreme_rule_diff 这类需要在编译期跑大量 group-by / hash 的 target.
# 默认 96M (= 100663296), 二分实测 (GCC 12.2): 40M 失败, 44M 通过, 取 ~2.2x 余量.
# 用户可通过 -DTETRIS_CONSTEXPR_OPS_LIMIT=<N> 覆盖.
set(TETRIS_CONSTEXPR_OPS_LIMIT 100663296 CACHE STRING
    "Maximum constexpr operations / steps for heavy compile-time tables (default 96M)")

# compile_commands.json 开启时, 每次构建把只含 src/ai_zzz* 翻译单元的子集写到源码根目录,
# 供 run-clang-tidy / omp cleanse 只诊断 ai_zzz; clangd 仍读 build/ 下的完整数据库.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(ai_zzz_compile_commands ALL
        COMMAND ${CMAKE_COMMAND}
            -DIN=${CMAKE_BINARY_DIR}/compile_commands.json
            -DOUT=${CMAKE_SOURCE_DIR}/compile_commands.json
            "-DREGEX=/src/ai_zzz([^/]*\\.(c|cc|cpp|cxx)|/.*)$"
            -P ${CMAKE_SOURCE_DIR}/cmake/filter_compile_commands.cmake
        VERBATIM)
endif()

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
add_library(tetris_core INTERFACE)
target_include_directories(tetris_core INTERFACE src)

# ---------------------------------------------------------------------------
# tetris_oracle: legacy master BFS searches (search_simple / search_path /
# search_simulate / search_tag / search_aspin / search_cautious / search_tspin).
# Consumed only by the *_diff harnesses.
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

project(aspin_dump)
add_executable(${PROJECT_NAME}
    src/ai_zzz.cpp
    src/integer_utils.cpp
    src/random.cpp
    tools/aspin_dump.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)

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
# 以及 TetrisEngine 实例化等价性.  纯编译验证, 运行时 main 直接返回 0.
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
# engine_regression_test: TetrisEngine env queue (4-arg get, hold swap, iterate
# + held tail), TSpinHook spin types and corner polarity, attach. Built with
# ASan/UBSan where available so out-of-bounds queue reads fail the test.
# ---------------------------------------------------------------------------
project(engine_regression_test)
add_executable(${PROJECT_NAME}
    src/ai_zzz.cpp
    src/integer_utils.cpp
    src/random.cpp
    test/engine_regression_test.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)
if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_libraries(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME tetris_engine_regression COMMAND engine_regression_test)

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
add_test(NAME tetris_dummy_strategy    COMMAND dummy_strategy_compile_check)
set_tests_properties(tetris_diff_simple_diff PROPERTIES WILL_FAIL FALSE)

# ---------------------------------------------------------------------------
# tree_test: rb_tree / sb_tree 正确性与性能验证工具.
# ---------------------------------------------------------------------------
add_executable(tree_test
    src/tree_test.cpp
    src/random.cpp
)
target_include_directories(tree_test PRIVATE src)

# ---------------------------------------------------------------------------
# demo: 入门级演示 AI DLL stub（对应 misakamm/blog/504）.
# ---------------------------------------------------------------------------
add_library(demo SHARED
    src/demo.cpp
    src/random.cpp
)
target_include_directories(demo PRIVATE src)

# ---------------------------------------------------------------------------
# pso: PSO 参数调优工具（SRS 规则，ai_zzz 评估函数）.
# 注：pso.cpp 已有 main()，直接接入。
# ---------------------------------------------------------------------------
add_executable(pso
    src/ai_zzz.cpp
    src/integer_utils.cpp
    src/random.cpp
    src/pso.cpp
)
target_include_directories(pso PRIVATE src)
target_link_libraries(pso PRIVATE Threads::Threads)

# ---------------------------------------------------------------------------
# the_ai_games: The AI Games 平台对战入口（TAG 规则 + ai_tag）.
# ---------------------------------------------------------------------------
add_executable(the_ai_games
    src/ai_tag.cpp
    src/integer_utils.cpp
    src/random.cpp
    src/the_ai_games.cpp
)
target_include_directories(the_ai_games PRIVATE src)
target_link_libraries(the_ai_games PRIVATE Threads::Threads)

# ---------------------------------------------------------------------------
# vs: 多规则终端对战工具（QQ/ST/SRS，ai_ax + ai_zzz）.
# ---------------------------------------------------------------------------
add_executable(vs
    src/ai_ax.cpp
    src/ai_zzz.cpp
    src/integer_utils.cpp
    src/random.cpp
    src/vs.cpp
)
target_include_directories(vs PRIVATE src)
if(NOT WIN32)
    target_link_libraries(vs PRIVATE dl)
endif()

# ---------------------------------------------------------------------------
# io_uds: TETR.IO bot server for botconnect (protocol v4, AF_UNIX socket, POSIX only).
# Off by default: it downloads nlohmann_json, which the CI `tetris_ai` build must not need.
# ---------------------------------------------------------------------------
option(TETRIS_IO_UDS "Build the io_uds TETR.IO socket server (downloads nlohmann_json)" OFF)
if(TETRIS_IO_UDS)
    if(WIN32)
        message(FATAL_ERROR "io_uds needs AF_UNIX stream sockets (POSIX only)")
    endif()
    if(CMAKE_VERSION VERSION_LESS 3.14)
        message(FATAL_ERROR "io_uds needs CMake >= 3.14 for FetchContent_MakeAvailable")
    endif()
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    )
    FetchContent_MakeAvailable(nlohmann_json)

    add_executable(io_uds src/io_uds.cpp)
    target_link_libraries(io_uds PRIVATE ai_zzz_io tetris_core nlohmann_json::nlohmann_json Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# TETR.IO Season 2: io_rules (header-only rules logic), the ai_zzz::IO AI and its kill check (IOLethal).
# Hosts (io_uds, io_sim, io_pso) link ai_zzz_io.
# ---------------------------------------------------------------------------
add_library(ai_zzz_io STATIC src/ai_zzz_io.cpp src/ai_zzz_io_lethal.cpp)
target_link_libraries(ai_zzz_io PUBLIC tetris_core)

add_executable(io_rules_test test/io_rules_test.cpp)
target_link_libraries(io_rules_test PRIVATE tetris_core)
add_test(NAME tetris_io_rules COMMAND io_rules_test)

add_executable(ai_zzz_io_test test/ai_zzz_io_test.cpp)
target_link_libraries(ai_zzz_io_test PRIVATE ai_zzz_io tetris_core)
add_test(NAME tetris_ai_zzz_io COMMAND ai_zzz_io_test)

add_executable(ai_zzz_io_spin_slots_test test/ai_zzz_io_spin_slots_test.cpp)
target_link_libraries(ai_zzz_io_spin_slots_test PRIVATE tetris_core)
add_test(NAME tetris_ai_zzz_io_spin_slots COMMAND ai_zzz_io_spin_slots_test)

# ---------------------------------------------------------------------------
# io_pso / io_sim_test: TETR.IO S2 match simulator (src/io_sim.*) and the PSO
# trainer over ai_zzz::IO parameters. Train with a Release build.
# ---------------------------------------------------------------------------
add_executable(io_pso src/io_pso.cpp src/io_sim.cpp)
target_link_libraries(io_pso PRIVATE ai_zzz_io tetris_core Threads::Threads)

add_executable(io_sim_test test/io_sim_test.cpp src/io_sim.cpp)
target_link_libraries(io_sim_test PRIVATE ai_zzz_io tetris_core)
add_test(NAME tetris_io_sim COMMAND io_sim_test)

# ---------------------------------------------------------------------------
# amini tests: rule_io + amini::Search against an independent TETR.IO model
# (test/_amini_reference.h). amini_witness_test also uses the diff fixtures.
# ---------------------------------------------------------------------------
add_executable(amini_witness_test
    src/integer_utils.cpp
    src/random.cpp
    test/amini_witness_test.cpp
)
target_include_directories(amini_witness_test PRIVATE src oracle)
target_link_libraries(amini_witness_test PRIVATE tetris_oracle)
add_test(NAME tetris_amini_witness COMMAND amini_witness_test)

add_executable(amini_classification_test test/amini_classification_test.cpp)
target_link_libraries(amini_classification_test PRIVATE tetris_core)
add_test(NAME tetris_amini_classification COMMAND amini_classification_test)
