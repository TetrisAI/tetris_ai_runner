# c_golden_compare.cmake — run NEW binary, strip MODE= first line, byte-compare to golden.
# fresh-checkout safe: 不依赖 snapshot OLD; golden 随 test 入库.
execute_process(COMMAND ${NEW} OUTPUT_FILE ${BODY}_new.out RESULT_VARIABLE rc_new)
if(NOT rc_new EQUAL 0)
  message(FATAL_ERROR "NEW side failed rc=${rc_new}")
endif()
file(READ ${BODY}_new.out new_txt)
file(READ ${GOLDEN} golden_txt)
string(REGEX REPLACE "^[^\n]*\n" "" new_body "${new_txt}")
string(REGEX REPLACE "^[^\n]*\n" "" golden_body "${golden_txt}")
if(NOT new_body STREQUAL golden_body)
  message(FATAL_ERROR "framework_ai_c_diff: NEW vs golden body differs")
endif()
message(STATUS "framework_ai_c_diff: NEW vs golden byte-identical")
