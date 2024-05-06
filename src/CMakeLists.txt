add_executable(head_test head_test.cpp)
target_link_libraries(head_test PUBLIC gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)

add_executable(scratch scratch.cpp)
target_link_libraries(scratch PUBLIC gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)
