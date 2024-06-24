add_library(gr4-packet-modem-blocks INTERFACE)
target_link_libraries(gr4-packet-modem-blocks INTERFACE gnuradio-core gnuradio-algorithm ldpc_toolbox)
target_include_directories(gr4-packet-modem-blocks INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/> $<INSTALL_INTERFACE:include/> ${ldpc-toolbox_SOURCE_DIR}/include)
