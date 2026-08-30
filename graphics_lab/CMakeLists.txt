# SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.20)
project(shadps4_graphics_lab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SHADPS4_LAB_BUILD_TESTS "Build the standalone plugin ABI smoke test" ON)

function(shadps4_lab_target_win7 target)
    if(WIN32)
        target_compile_definitions(${target} PRIVATE
            _WIN32_WINNT=0x0601
            WINVER=0x0601
            NTDDI_VERSION=0x06010000)
    endif()
endfunction()

function(shadps4_lab_add_plugin target source output_name)
    add_library(${target} SHARED ${source})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
        PREFIX ""
        OUTPUT_NAME ${output_name})
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    shadps4_lab_target_win7(${target})
endfunction()

shadps4_lab_add_plugin(shadps4_lab_safe_gpu
    plugins/safe_gpu/plugin.cpp
    shadps4_safe_gpu)
shadps4_lab_add_plugin(shadps4_lab_vulkan
    plugins/vulkan_lab/plugin.cpp
    shadps4_vulkan_lab)
shadps4_lab_add_plugin(shadps4_lab_trace_probe
    plugins/trace_probe/plugin.cpp
    shadps4_trace_probe)

add_executable(shadps4_trace_collector diagnostics/collector/main.cpp)
shadps4_lab_target_win7(shadps4_trace_collector)
if(MSVC)
    target_compile_options(shadps4_trace_collector PRIVATE /W4 /permissive-)
    target_link_options(shadps4_trace_collector PRIVATE /SUBSYSTEM:CONSOLE,6.01)
else()
    target_compile_options(shadps4_trace_collector PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SHADPS4_LAB_BUILD_TESTS)
    include(CTest)
    add_executable(shadps4_lab_plugin_smoke tests/plugin_smoke.cpp)
    target_include_directories(shadps4_lab_plugin_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(shadps4_lab_plugin_smoke PRIVATE ${CMAKE_DL_LIBS})
    shadps4_lab_target_win7(shadps4_lab_plugin_smoke)
    if(MSVC)
        target_compile_options(shadps4_lab_plugin_smoke PRIVATE /W4 /permissive-)
        target_link_options(shadps4_lab_plugin_smoke PRIVATE /SUBSYSTEM:CONSOLE,6.01)
    else()
        target_compile_options(shadps4_lab_plugin_smoke PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME shadps4_lab_plugin_abi_smoke
        COMMAND shadps4_lab_plugin_smoke
            $<TARGET_FILE:shadps4_lab_safe_gpu>
            $<TARGET_FILE:shadps4_lab_vulkan>
            $<TARGET_FILE:shadps4_lab_trace_probe>)

    add_executable(shadps4_lab_host_smoke
        tests/host_smoke.cpp
        host/plugin_host.cpp)
    target_include_directories(shadps4_lab_host_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/host)
    target_link_libraries(shadps4_lab_host_smoke PRIVATE ${CMAKE_DL_LIBS})
    shadps4_lab_target_win7(shadps4_lab_host_smoke)
    if(MSVC)
        target_compile_options(shadps4_lab_host_smoke PRIVATE /W4 /permissive-)
        target_link_options(shadps4_lab_host_smoke PRIVATE /SUBSYSTEM:CONSOLE,6.01)
    else()
        target_compile_options(shadps4_lab_host_smoke PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    shadps4_lab_add_plugin(shadps4_lab_incompatible_fixture
        tests/incompatible_plugin.cpp
        shadps4_lab_incompatible_fixture)
    add_test(NAME shadps4_lab_host_shadow_copy_smoke
        COMMAND shadps4_lab_host_smoke
            $<TARGET_FILE_DIR:shadps4_lab_safe_gpu>
            $<TARGET_FILE_DIR:shadps4_lab_host_smoke>/host-shadow
            $<TARGET_FILE:shadps4_lab_incompatible_fixture>)
endif()

install(TARGETS
    shadps4_lab_safe_gpu
    shadps4_lab_vulkan
    shadps4_lab_trace_probe
    shadps4_trace_collector
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .)
