file(GLOB examples "*.cpp")
foreach(example ${examples})
  get_filename_component(example_name ${example} NAME_WE)
  add_executable(${example_name} ${example})
  target_link_libraries(${example_name} PUBLIC
    gr4-packet-modem-blocks gnuradio-options gnuradio-core fmt::fmt refl-cpp magic_enum ut)
endforeach(example)
