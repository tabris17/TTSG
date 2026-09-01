cmake_minimum_required(VERSION 3.20)
project(ttsg LANGUAGES CXX)

# Version of the executable. CI overrides this with the git tag on release:
#   cmake -DTTSG_VERSION=0.1.1 ...
if(NOT DEFINED TTSG_VERSION)
    set(TTSG_VERSION "0.0.0")
endif()
if(NOT TTSG_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR
        "TTSG_VERSION must be MAJOR.MINOR.PATCH, got: \"${TTSG_VERSION}\"")
endif()
set(TTSG_VERSION_MAJOR ${CMAKE_MATCH_1})
set(TTSG_VERSION_MINOR ${CMAKE_MATCH_2})
set(TTSG_VERSION_PATCH ${CMAKE_MATCH_3})
configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ttsg WIN32
    src/main.cpp
    src/alert.cpp
    src/app.cpp
    src/bootlog.cpp
    src/countdown.cpp
    src/settingsdlg.cpp
    src/util.cpp
    src/ttsg.rc
)

target_compile_definitions(ttsg PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
)

# Generated version.h (included by ttsg.rc for the VERSIONINFO resource).
target_include_directories(ttsg PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(ttsg PRIVATE
    user32
    gdi32
    shell32
    comctl32
    advapi32
)

if(MSVC)
    target_compile_options(ttsg PRIVATE /utf-8 /W3)
    # wcsncpy etc. are used deliberately with fixed-size buffers.
    target_compile_definitions(ttsg PRIVATE _CRT_SECURE_NO_WARNINGS)
    # The linker must not inject its own manifest: ttsg.rc already embeds one
    # (resource id 1), otherwise cvtres reports a duplicate MANIFEST resource.
    target_link_options(ttsg PRIVATE /MANIFEST:NO)
endif()

if(MINGW)
    target_compile_options(ttsg PRIVATE -Wall -Wextra)
    # Portable single-file executable.
    target_link_options(ttsg PRIVATE -static)

    # mingw-w64 GCC auto-links default-manifest.o via the *endfile spec, which
    # collides with the manifest embedded in ttsg.rc ("multiple non-default
    # manifests"). Generate a specs file that drops the automatic one so only
    # our manifest (comctl32 v6 + PerMonitorV2 DPI) ends up in the executable.
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -dumpspecs
        OUTPUT_VARIABLE _ttsg_specs
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REPLACE "%:if-exists(default-manifest.o%s)" ""
                   _ttsg_specs "${_ttsg_specs}")
    file(WRITE ${CMAKE_BINARY_DIR}/ttsg-no-default-manifest.specs "${_ttsg_specs}\n")
    target_link_options(ttsg PRIVATE
        -specs=${CMAKE_BINARY_DIR}/ttsg-no-default-manifest.specs)
endif()

# All binaries go to <source dir>/out, regardless of generator or configuration.
set_target_properties(ttsg PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out
)
foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(ttsg PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config} ${CMAKE_SOURCE_DIR}/out
    )
endforeach()
