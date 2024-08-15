file(GLOB apps "*.cpp")
foreach(app ${apps})
  get_filename_component(app_name ${app} NAME_WE)
  add_executable(${app_name} ${app})
  target_link_libraries(${app_name} PUBLIC
    gr4-packet-modem-blocks gnuradio-options gnuradio-core gr-soapy fmt::fmt refl-cpp magic_enum ut
    PRIVATE ZeroMQ::ZeroMQ)
endforeach(app)
