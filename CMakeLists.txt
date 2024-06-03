cmake_minimum_required(VERSION 3.25)

project(gr4-packet-modem CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

option(ADDRESS_SANITIZER "Enable address sanitizer" OFF)
option(UB_SANITIZER "Enable undefined behavior sanitizer" OFF)
option(THREAD_SANITIZER "Enable thread sanitizer" OFF)

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

enable_testing()

add_subdirectory(blocks)
add_subdirectory(examples)
add_subdirectory(gnuradio4)
add_subdirectory(test)
add_subdirectory(src)
