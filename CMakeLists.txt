# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20)
project(LunaPath VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

set(LUNAPATH_PATH_BITS 64 CACHE STRING "Compile-time path capacity: 32, 64, 128, or 256")
set_property(CACHE LUNAPATH_PATH_BITS PROPERTY STRINGS 32 64 128 256)
set(LUNAPATH_GUARD 2 CACHE STRING "Compile-time guard: 0=NONE, 1=PARITY, 2=CRC16, 3=CRC32, 4=ROLL64")
set_property(CACHE LUNAPATH_GUARD PROPERTY STRINGS 0 1 2 3 4)
if(NOT LUNAPATH_PATH_BITS MATCHES "^(32|64|128|256)$")
    message(FATAL_ERROR "LUNAPATH_PATH_BITS must be 32, 64, 128, or 256")
endif()
if(NOT LUNAPATH_GUARD MATCHES "^[0-4]$")
    message(FATAL_ERROR "LUNAPATH_GUARD must be 0..4")
endif()

option(LUNAPATH_BUILD_TESTS "Build the strict profile test matrix" ${BUILD_TESTING})
option(LUNAPATH_BUILD_EXHAUSTIVE_TESTS "Build exhaustive all-paths-through-depth-20 tests" OFF)
option(LUNAPATH_BUILD_EXAMPLES "Build the firmware workflow example" ON)
option(LUNAPATH_BUILD_BENCHMARKS "Build host benchmarks" OFF)

function(lunapath_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

add_library(lunapath src/lunapath.c)
add_library(LunaPath::lunapath ALIAS lunapath)
set_target_properties(lunapath PROPERTIES EXPORT_NAME lunapath)
target_compile_features(lunapath PUBLIC c_std_17)
target_compile_definitions(lunapath PUBLIC
    LUNAPATH_PATH_BITS=${LUNAPATH_PATH_BITS}
    LUNAPATH_GUARD=${LUNAPATH_GUARD})
target_include_directories(lunapath PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(LUNAPATH_BUILD_TESTS)
    lunapath_strict_warnings(lunapath)
endif()

if(LUNAPATH_BUILD_TESTS)
    enable_testing()
    set(LUNAPATH_LEGACY_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/legacy/v1/lunu_primitives.c)
    foreach(bits IN ITEMS 32 64 128 256)
        foreach(guard RANGE 0 4)
            set(target test_lunapath_${bits}_${guard})
            add_executable(${target} tests/test_lunapath.c src/lunapath.c ${LUNAPATH_LEGACY_SOURCE})
            target_compile_features(${target} PRIVATE c_std_17)
            target_include_directories(${target} PRIVATE include legacy/v1)
            target_compile_definitions(${target} PRIVATE
                LUNAPATH_PATH_BITS=${bits} LUNAPATH_GUARD=${guard})
            lunapath_strict_warnings(${target})
            add_test(NAME ${target} COMMAND ${target})
        endforeach()
        if(LUNAPATH_BUILD_EXHAUSTIVE_TESTS)
            set(target test_lunapath_exhaustive_${bits})
            add_executable(${target} tests/test_lunapath.c src/lunapath.c ${LUNAPATH_LEGACY_SOURCE})
            target_compile_features(${target} PRIVATE c_std_17)
            target_include_directories(${target} PRIVATE include legacy/v1)
            target_compile_definitions(${target} PRIVATE LUNAPATH_PATH_BITS=${bits} LUNAPATH_GUARD=2)
            lunapath_strict_warnings(${target})
            add_test(NAME ${target} COMMAND ${target} exhaustive)
            set_tests_properties(${target} PROPERTIES LABELS exhaustive)
        endif()
    endforeach()

    if(LUNAPATH_BUILD_EXAMPLES)
        add_executable(test_firmware_workflow examples/firmware_workflow.c src/lunapath.c)
        target_compile_features(test_firmware_workflow PRIVATE c_std_17)
        target_include_directories(test_firmware_workflow PRIVATE include)
        target_compile_definitions(test_firmware_workflow PRIVATE LUNAPATH_PATH_BITS=64 LUNAPATH_GUARD=2)
        lunapath_strict_warnings(test_firmware_workflow)
        add_test(NAME firmware_workflow COMMAND test_firmware_workflow)
    endif()
endif()

if(LUNAPATH_BUILD_EXAMPLES AND NOT LUNAPATH_BUILD_TESTS)
    add_executable(firmware_workflow examples/firmware_workflow.c)
    target_link_libraries(firmware_workflow PRIVATE LunaPath::lunapath)
endif()

if(LUNAPATH_BUILD_BENCHMARKS)
    add_executable(benchmark_lunapath benchmarks/benchmark_lunapath.c)
    target_link_libraries(benchmark_lunapath PRIVATE LunaPath::lunapath)
    add_executable(profile_sizes benchmarks/profile_sizes.c)
    target_link_libraries(profile_sizes PRIVATE LunaPath::lunapath)
    if(LUNAPATH_BUILD_TESTS)
        lunapath_strict_warnings(benchmark_lunapath)
        lunapath_strict_warnings(profile_sizes)
    endif()
endif()

configure_package_config_file(cmake/LunaPathConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/LunaPathConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LunaPath)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/LunaPathConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(TARGETS lunapath EXPORT LunaPathTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES include/lunapath.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT LunaPathTargets NAMESPACE LunaPath:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LunaPath)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/LunaPathConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/LunaPathConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LunaPath)
install(FILES LICENSE NOTICE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/LunaPath)
