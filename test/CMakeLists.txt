function(setup_test_no_asan TEST_NAME)
  target_link_libraries(${TEST_NAME} PRIVATE
    gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt refl-cpp
    yaml-cpp::yaml-cpp fftw gr-basic gr-testing ut)
  add_test(NAME ${TEST_NAME} COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME})
endfunction()

function(setup_test TEST_NAME)
  # limited to gcc due to a Ubuntu packaging bug of libc++, see
  # https://github.com/llvm/llvm-project/issues/59432
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # for testing consider enabling -D_GLIBCXX_DEBUG and
    # -D_GLIBCXX_SANITIZE_VECTOR
    target_compile_options(${TEST_NAME} PRIVATE -fsanitize=address)
    # for testing consider enabling -D_GLIBCXX_DEBUG and
    # -D_GLIBCXX_SANITIZE_VECTOR
    target_link_options(${TEST_NAME} PRIVATE -fsanitize=address)
  endif()

  setup_test_no_asan(${TEST_NAME})
endfunction()

function(add_ut_test TEST_NAME)
  add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
  setup_test(${TEST_NAME})
endfunction()

add_ut_test(firdes)
