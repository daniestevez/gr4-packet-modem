file(GLOB benchmarks "benchmark_*.cpp")
foreach(benchmark ${benchmarks})
  get_filename_component(benchmark_name ${benchmark} NAME_WE)
  add_executable(${benchmark_name} ${benchmark})
  target_link_libraries(${benchmark_name} PUBLIC
    gr4-packet-modem-blocks gnuradio-options gnuradio-core gr-soapy fmt::fmt refl-cpp magic_enum ut
    PRIVATE ZeroMQ::ZeroMQ)
endforeach(benchmark)
