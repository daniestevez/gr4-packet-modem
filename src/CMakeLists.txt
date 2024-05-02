add_executable(minimal_flowgraph minimal_flowgraph.cpp)
target_link_libraries(minimal_flowgraph PUBLIC gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)
