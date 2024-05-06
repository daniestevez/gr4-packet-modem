add_executable(minimal_flowgraph minimal_flowgraph.cpp)
target_link_libraries(minimal_flowgraph PUBLIC gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)

add_executable(tags tags.cpp)
target_link_libraries(tags PUBLIC gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)

add_executable(stop_flowgraph stop_flowgraph.cpp)
target_link_libraries(stop_flowgraph PUBLIC gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)
