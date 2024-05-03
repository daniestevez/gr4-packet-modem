add_library(gr4-packet-modem-blocks INTERFACE)
target_link_libraries(gr4-packet-modem-blocks INTERFACE gnuradio-core gnuradio-algorithm)
target_include_directories(gr4-packet-modem-blocks INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/> $<INSTALL_INTERFACE:include/>)
