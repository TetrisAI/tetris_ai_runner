cmake_minimum_required(VERSION 3.16)
project(cultrisai LANGUAGES CXX)

set(TETRIS_AI_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
find_package(Threads REQUIRED)

add_library(cultrisai SHARED
    c2_ai_impl.cpp
    c2_jni.cpp
    ${TETRIS_AI_ROOT}/src/ai_zzz.cpp
    ${TETRIS_AI_ROOT}/src/integer_utils.cpp
    ${TETRIS_AI_ROOT}/src/random.cpp
)

target_include_directories(cultrisai PRIVATE ${TETRIS_AI_ROOT}/src)
target_compile_features(cultrisai PRIVATE cxx_std_20)
target_compile_options(cultrisai PRIVATE -Wno-unused-value -O1)
target_link_libraries(cultrisai PRIVATE Threads::Threads)

find_package(JNI QUIET)
if(JNI_FOUND)
    target_include_directories(cultrisai PRIVATE ${JNI_INCLUDE_DIRS})
else()
    set(_cultrisai_jni_candidates)
    if(DEFINED ENV{JAVA_HOME})
        list(APPEND _cultrisai_jni_candidates "$ENV{JAVA_HOME}")
    endif()
    list(APPEND _cultrisai_jni_candidates
        "$ENV{HOME}/Documents/CultrisII.def/c2_apple_silicon/jdks/jdk8u412-b08/Contents/Home")
    set(_cultrisai_jni_found FALSE)
    foreach(_jdk_home IN LISTS _cultrisai_jni_candidates)
        if(EXISTS "${_jdk_home}/include/jni.h" AND EXISTS "${_jdk_home}/include/darwin/jni_md.h")
            target_include_directories(cultrisai PRIVATE
                "${_jdk_home}/include" "${_jdk_home}/include/darwin")
            set(_cultrisai_jni_found TRUE)
            message(STATUS "cultrisai: using JNI headers from ${_jdk_home}")
            break()
        endif()
    endforeach()
    if(NOT _cultrisai_jni_found)
        message(FATAL_ERROR "cultrisai: JNI headers not found; set JAVA_HOME to a JDK with include/jni.h")
    endif()
endif()

set_target_properties(cultrisai PROPERTIES
    OUTPUT_NAME cultrisai
    PREFIX lib
    SUFFIX .dylib
    POSITION_INDEPENDENT_CODE ON
)
