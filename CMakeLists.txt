cmake_minimum_required(VERSION 3.0)
project(tetris_ai)
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
add_library(${PROJECT_NAME} SHARED
    src/ai_zzz.cpp
    src/integer_utils.cpp
    src/random.cpp
    src/rule_srs.cpp
    src/search_tspin.cpp
    src/tetris_core.cpp
    src/ppt_pso.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)
