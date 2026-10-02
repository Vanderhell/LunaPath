# SPDX-License-Identifier: Apache-2.0
if(NOT DEFINED LUNAPATH_PATH_BITS)
    set(LUNAPATH_PATH_BITS 64)
endif()
if(NOT DEFINED LUNAPATH_GUARD)
    set(LUNAPATH_GUARD 2)
endif()
if(NOT DEFINED LUNAPATH_USE_ESP_CRC32)
    set(LUNAPATH_USE_ESP_CRC32 OFF)
endif()
set(LUNAPATH_CORE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../../src/lunapath.c")
set(LUNAPATH_PUBLIC_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/../../../include")
set(LUNAPATH_V1_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../../legacy/v1/lunu_primitives.c")
set(LUNAPATH_SOURCES "app_main.c" "${LUNAPATH_CORE_SOURCE}" "${LUNAPATH_V1_SOURCE}")
if(LUNAPATH_GUARD EQUAL 3 AND LUNAPATH_USE_ESP_CRC32)
    list(APPEND LUNAPATH_SOURCES "esp_crc_backend.c")
endif()
idf_component_register(SRCS ${LUNAPATH_SOURCES}
                       INCLUDE_DIRS "." "${LUNAPATH_PUBLIC_INCLUDE}" "${CMAKE_CURRENT_LIST_DIR}/../../../legacy/v1"
                       REQUIRES esp_hw_support)
target_compile_features(${COMPONENT_LIB} PRIVATE c_std_17)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LUNAPATH_PATH_BITS=${LUNAPATH_PATH_BITS}
    LUNAPATH_GUARD=${LUNAPATH_GUARD})
if(LUNAPATH_GUARD EQUAL 3 AND LUNAPATH_USE_ESP_CRC32)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LUNAPATH_USE_ESP_CRC32=1)
endif()
