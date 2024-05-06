add_executable(minimal_flowgraph minimal_flowgraph.cpp)
target_link_libraries(minimal_flowgraph PUBLIC gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)

add_executable(head_test head_test.cpp)
target_link_libraries(head_test PUBLIC gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)

add_executable(tags_example tags_example.cpp)
target_link_libraries(tags_example PUBLIC gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)

add_executable(scratch scratch.cpp)
target_link_libraries(scratch PUBLIC gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)

add_executable(vector_source_test vector_source_test.cpp)
target_link_libraries(vector_source_test PUBLIC gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)
