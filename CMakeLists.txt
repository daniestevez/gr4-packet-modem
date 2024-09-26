cmake_minimum_required(VERSION 3.25)

project(gr4-packet-modem CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

option(ADDRESS_SANITIZER "Enable address sanitizer" OFF)
option(UB_SANITIZER "Enable undefined behavior sanitizer" OFF)
option(THREAD_SANITIZER "Enable thread sanitizer" OFF)

# Corrosion
include(FetchContent)
FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.5 # Optionally specify a commit hash, version tag or branch here
)
FetchContent_Declare(
    ldpc-toolbox
    GIT_REPOSITORY https://github.com/daniestevez/ldpc-toolbox.git
    GIT_TAG v0.6.3
)
# Set any global configuration variables such as `Rust_TOOLCHAIN` before this line!
FetchContent_MakeAvailable(Corrosion ldpc-toolbox)
corrosion_import_crate(MANIFEST_PATH ${ldpc-toolbox_SOURCE_DIR}/Cargo.toml)

# Make sure our local CMake Modules path comes first
list(INSERT CMAKE_MODULE_PATH 0 ${PROJECT_SOURCE_DIR}/cmake/Modules)

# Some blocks use ZMQ
find_package(ZeroMQ MODULE REQUIRED)

option(ENABLE_PACKET_MODEM_PYTHON "Enable gr4_packet_modem Python package" OFF)
if (ENABLE_PACKET_MODEM_PYTHON)
  # Find the module development requirements (requires FindPython from 3.17 or
  # scikit-build-core's built-in backport)
  find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
  find_package(pybind11 CONFIG REQUIRED)
endif ()

set(CMAKE_EXT_DEP_WARNING_GUARD "")
if(DISABLE_EXTERNAL_DEPS_WARNINGS) # enable warnings for external dependencies
    set(CMAKE_EXT_DEP_WARNING_GUARD SYSTEM)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "(Clang|GNU|Intel)")
    # -Og is a much more reasonable default for debugging. Also enable gdb extensions.
    set(CMAKE_CXX_FLAGS_DEBUG "-Og -ggdb" CACHE INTERNAL
        "Flags used by the compiler during debug builds.")

    # Add a build type that keeps runtime checks enabled
    set(CMAKE_CXX_FLAGS_RELWITHASSERT "-O3" CACHE INTERNAL
        "Flags used by the compiler during release builds containing runtime checks.")

    # The default value is often an empty string, but this is usually not desirable and one of the
    # other standard build types is usually more appropriate.
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "RelWithAssert" CACHE STRING
            "Choose the type of build. Options are: None Debug Release RelWithAssert RelWithDebInfo MinSizeRel.\n\
 - None: no compiler flags, defaults and target-specific flags apply\n\
 - Debug: best/complete debugging experience; as optimized as reasonable\n\
 - Release: full optimization; some runtime checks disabled\n\
 - RelWithAssert: full optimization; runtime checks enabled\n\
 - RelWithDebInfo: optimized; debug info; some runtime checks disabled"
            FORCE)
    endif(NOT CMAKE_BUILD_TYPE)

    if (CMAKE_BUILD_TYPE STREQUAL "" AND NOT CMAKE_CXX_FLAGS MATCHES "-O[123gs]")
        message(WARNING "It seems you are compiling without optimization. Please set CMAKE_BUILD_TYPE or CMAKE_CXX_FLAGS.")
    endif ()
endif ()

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang") # set default C++ STL to Clang's libc++ when using Clang
    add_compile_options(-stdlib=libc++ -fcolor-diagnostics)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "(Clang|GNU)")
    # Validate that only one sanitizer option is enabled
    if ((ADDRESS_SANITIZER AND UB_SANITIZER) OR (ADDRESS_SANITIZER AND THREAD_SANITIZER) OR (UB_SANITIZER AND THREAD_SANITIZER))
        message(FATAL_ERROR "Only one of ADDRESS_SANITIZER, UB_SANITIZER, or THREAD_SANITIZER can be enabled at the same time.")
    endif ()

    if (ADDRESS_SANITIZER)
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
        message(STATUS "Enable ADDRESS_SANITIZER: ${ADDRESS_SANITIZER}")
    elseif (UB_SANITIZER)
        add_compile_options(-fsanitize=undefined)
        add_link_options(-fsanitize=undefined)
        message(STATUS "Enable UB_SANITIZER: ${UB_SANITIZER}")
    elseif (THREAD_SANITIZER)
        add_compile_options(-fsanitize=thread)
        add_link_options(-fsanitize=thread)
        message(STATUS "Enable THREAD_SANITIZER: ${THREAD_SANITIZER}")
    endif ()
endif ()

# Mainly for FMT
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

option(ENABLE_PACKET_MODEM_TESTING "Enable gr4-packet-modem QA tests" ON)
if (ENABLE_PACKET_MODEM_TESTING)
  enable_testing()
  add_subdirectory(test)
endif ()

add_subdirectory(blocks)
add_subdirectory(gnuradio4)

option(ENABLE_PACKET_MODEM_APPS "Enable gr4-packet-modem applications and examples" ON)
if (ENABLE_PACKET_MODEM_APPS)
  add_subdirectory(apps)
  add_subdirectory(examples)
endif ()

if (ENABLE_PACKET_MODEM_PYTHON)
  # Add a library using FindPython's tooling (pybind11 also provides a helper like
  # this)
  python_add_library(gr4_packet_modem_python MODULE
    python/bindings/python_bindings.cpp
    python/bindings/register_blocks.cpp
    python/bindings/register_add.cpp
    python/bindings/register_additive_scrambler.cpp
    python/bindings/register_binary_slicer.cpp
    python/bindings/register_burst_shaper.cpp
    python/bindings/register_coarse_frequency_correction.cpp
    python/bindings/register_constellation_llr_decoder.cpp
    python/bindings/register_costas_loop.cpp
    python/bindings/register_crc_append.cpp
    python/bindings/register_crc_check.cpp
    python/bindings/register_file_sink.cpp
    python/bindings/register_file_source.cpp
    python/bindings/register_glfsr_source.cpp
    python/bindings/register_head.cpp
    python/bindings/register_header_fec_decoder.cpp
    python/bindings/register_header_fec_encoder.cpp
    python/bindings/register_header_formatter.cpp
    python/bindings/register_header_parser.cpp
    python/bindings/register_header_payload_split.cpp
    python/bindings/register_interpolating_fir_filter.cpp
    python/bindings/register_item_strobe.cpp
    python/bindings/register_mapper.cpp
    python/bindings/register_message_debug.cpp
    python/bindings/register_message_strobe.cpp
    python/bindings/register_multiply_packet_len_tag.cpp
    python/bindings/register_noise_source.cpp
    python/bindings/register_null_sink.cpp
    python/bindings/register_null_source.cpp
    python/bindings/register_pack_bits.cpp
    python/bindings/register_packet_counter.cpp
    python/bindings/register_packet_ingress.cpp
    python/bindings/register_packet_limiter.cpp
    python/bindings/register_packet_mux.cpp
    python/bindings/register_packet_strobe.cpp
    python/bindings/register_packet_to_stream.cpp
    python/bindings/register_packet_type_filter.cpp
    python/bindings/register_payload_metadata_insert.cpp
    python/bindings/register_pdu_to_tagged_stream.cpp
    python/bindings/register_pfb_arb_resampler.cpp
    python/bindings/register_probe_rate.cpp
    python/bindings/register_random_source.cpp
    python/bindings/register_rotator.cpp
    python/bindings/register_stream_to_pdu.cpp
    python/bindings/register_stream_to_tagged_stream.cpp
    python/bindings/register_symbol_filter.cpp
    python/bindings/register_syncword_detection.cpp
    python/bindings/register_syncword_detection_filter.cpp
    python/bindings/register_syncword_wipeoff.cpp
    python/bindings/register_tag_gate.cpp
    python/bindings/register_tagged_stream_to_pdu.cpp
    python/bindings/register_throttle.cpp
    python/bindings/register_tun_sink.cpp
    python/bindings/register_tun_source.cpp
    python/bindings/register_unpack_bits.cpp
    python/bindings/register_vector_sink.cpp
    python/bindings/register_vector_source.cpp
    python/bindings/register_zmq_pdu_pub_sink.cpp
    WITH_SOABI)
  target_link_libraries(gr4_packet_modem_python
    PUBLIC
    gr4-packet-modem-blocks
    gnuradio-options
    gnuradio-core
    gr-soapy
    fmt::fmt
    refl-cpp
    magic_enum
    ut
    PRIVATE
    ZeroMQ::ZeroMQ
    pybind11::headers)
  if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    # also link against LLVM's libc++
    target_link_libraries(gr4_packet_modem_python PUBLIC c++)
  endif()

  # This is passing in the version as a define just as an example
  target_compile_definitions(gr4_packet_modem_python PRIVATE VERSION_INFO=${PROJECT_VERSION})

  # The install directory is the output (wheel) directory
  install(TARGETS gr4_packet_modem_python DESTINATION gr4_packet_modem)
endif ()
