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

# 编译期 constexpr 评估 ops/steps 上限.
# 用于 extreme_rule_diff 这类需要在编译期跑大量 group-by / hash 的 target.
# 默认 96M (= 100663296), 二分实测 (GCC 12.2): 40M 失败, 44M 通过, 取 ~2.2x 余量.
# 用户可通过 -DTETRIS_CONSTEXPR_OPS_LIMIT=<N> 覆盖.
set(TETRIS_CONSTEXPR_OPS_LIMIT 100663296 CACHE STRING
    "Maximum constexpr operations / steps for heavy compile-time tables (default 96M)")

add_library(${PROJECT_NAME} SHARED
    src/ai.cpp
    src/ai_ax.cpp
    src/ai_farter.cpp
    src/ai_misaka.cpp
    src/ai_tag.cpp
    src/dllmain.c
    src/integer_utils.cpp
    src/rule_asrs.cpp
    src/search_path.cpp
    src/search_simple.cpp
    src/search_simulate.cpp
    src/search_tag.cpp
    src/search_aspin.cpp
    src/search_tspin.cpp
    src/rule_botris.cpp
    src/random.cpp
    src/rule_c2.cpp
    src/rule_qq.cpp
    src/rule_srsx.cpp
    src/rule_st.cpp
    src/rule_tag.cpp
    src/rule_toj.cpp
    src/tetris_core.cpp
    src/ai_zzz.cpp
    src/search_cautious.cpp
    src/rule_srs.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)


project(tetris_ai_runner)
add_library(${PROJECT_NAME} SHARED
    src/ai.cpp
    src/ai_ax.cpp
    src/ai_farter.cpp
    src/ai_misaka.cpp
    src/ai_tag.cpp
    src/dllmain.c
    src/integer_utils.cpp
    src/rule_asrs.cpp
    src/search_path.cpp
    src/search_simple.cpp
    src/search_simulate.cpp
    src/search_tag.cpp
    src/search_tspin.cpp
    src/search_aspin.cpp
    src/rule_botris.cpp
    src/random.cpp
    src/rule_c2.cpp
    src/rule_qq.cpp
    src/rule_srsx.cpp
    src/rule_st.cpp
    src/rule_tag.cpp
    src/rule_toj.cpp
    src/tetris_core.cpp
    src/ai_zzz.cpp
    src/search_cautious.cpp
    src/rule_srs.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)


project(top_pso)
add_executable(${PROJECT_NAME}
    src/ai_zzz.cpp
    src/integer_utils.cpp
    src/random.cpp
    src/rule_srs.cpp
    src/search_tspin.cpp
    src/tetris_core.cpp
    src/ppt_pso.cpp
)

project(botris)
add_executable(${PROJECT_NAME}
    src/ai_zzz.cpp
    src/botris.cpp
    src/integer_utils.cpp
    src/random.cpp
    src/rule_botris.cpp
    src/search_aspin.cpp
    src/tetris_core.cpp
)

project(cmd_tris)
add_executable(${PROJECT_NAME}
        src/ai_zzz.cpp
        src/cmd_tris.cpp
        src/integer_utils.cpp
        src/random.cpp
        src/rule_botris.cpp
        src/search_aspin.cpp
        src/tetris_core.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)

project(perft_movegen)
add_executable(${PROJECT_NAME}
    tests/perft_movegen.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)

project(oracle_diff)
add_executable(${PROJECT_NAME}
    src/ai_zzz.cpp
    src/integer_utils.cpp
    src/random.cpp
    src/rule_srs.cpp
    src/search_tspin.cpp
    src/tetris_core.cpp
    tests/oracle_diff.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)

project(aspin_dump)
add_executable(${PROJECT_NAME}
    src/ai_zzz.cpp
    src/integer_utils.cpp
    src/random.cpp
    src/rule_botris.cpp
    src/search_aspin.cpp
    src/tetris_core.cpp
    tools/aspin_dump.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)

project(extreme_rule_diff)
add_executable(${PROJECT_NAME}
    src/integer_utils.cpp
    src/random.cpp
    src/rule_extreme.cpp
    src/search_simple.cpp
    src/tetris_core.cpp
    tests/extreme_rule_diff.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)
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
