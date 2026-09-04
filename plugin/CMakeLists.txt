# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project(DisplayInfo)

cmake_minimum_required(VERSION 3.3)

find_package(WPEFramework)

project_version(1.0.0)
set(PLUGIN_NAME DisplayInfo)
set(MODULE_NAME ${NAMESPACE}${PROJECT_NAME})
message("Setup ${MODULE_NAME} v${PROJECT_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(PLUGIN_DISPLAYINFO_AUTOSTART "true" CACHE STRING "Automatically start DisplayInfo plugin")
set(PLUGIN_DISPLAYINFO_STARTUPORDER "" CACHE STRING "Automatically start DisplayInfo plugin")
set(PLUGIN_DISPLAYINFO_MODE "Off" CACHE STRING "Set DisplayInfo plugin mode, controls if the plugin should run in its own process, in process or remote.")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)
find_package(BCM_HOST QUIET)
find_package(NEXUS QUIET)
find_package(NXCLIENT QUIET)
find_package(LibDRM QUIET)
find_path (STAGING_INCDIR glib-2.0)
include_directories(include)
include_directories(${STAGING_INCDIR})
include_directories(${STAGING_INCDIR}/libdrm)

add_library(${MODULE_NAME} SHARED
    DisplayInfo.cpp
    Module.cpp)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_compile_definitions(${MODULE_NAME} PRIVATE MODULE_NAME=Plugin_${PLUGIN_NAME})

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        ${NAMESPACE}::${NAMESPACE}Helpers)

if (PLUGIN_DISPLAYINFO_BCM_VERSION_MAJOR)
    target_compile_definitions(${MODULE_NAME}
        PRIVATE
            DISPLAYINFO_BCM_VERSION_MAJOR=${PLUGIN_DISPLAYINFO_BCM_VERSION_MAJOR})
endif()

# -------------------------------------------------------------------
# USE_DEVICESETTING_PLUGIN: COM-RPC path via entservices-devicesettings
# Mutually exclusive with USE_DEVICESETTINGS (libds path).
# -------------------------------------------------------------------
option(USE_DEVICESETTING_PLUGIN
    "Use DeviceSettings COM-RPC plugin instead of libds for DisplayInfo" OFF)

if (USE_DEVICESETTING_PLUGIN)
    add_definitions(-DUSE_DEVICESETTING_PLUGIN)
    message(STATUS "DisplayInfo: USE_DEVICESETTING_PLUGIN=ON (COM-RPC via entservices-helpers)")

    find_package(${NAMESPACE}Helpers REQUIRED)

    # edid-parser.hpp is needed for EDID width/height/colorimetry parsing.
    # It lives under <staging-incdir>/rdk/ds/ in the Yocto sysroot.
    find_path(EDID_PARSER_DIR NAMES edid-parser.hpp
        PATHS "${STAGING_INCDIR}/rdk/ds"
        PATH_SUFFIXES rdk/ds)
    if (EDID_PARSER_DIR)
        target_include_directories(${MODULE_NAME} PRIVATE ${EDID_PARSER_DIR})
    else()
        message(WARNING "DisplayInfo: edid-parser.hpp not found — EDID parsing may fail at compile time")
    endif()

    target_sources(${MODULE_NAME}
        PRIVATE
            DeviceSettings/DS_COMRPC/DSPlatformImplementation.cpp)

    # displayinfo-soc is still needed for SoC_GetTotalGpuRam / SoC_GetFreeGpuRam.
    target_link_libraries(${MODULE_NAME} PRIVATE
        displayinfo-soc
        ${NAMESPACE}::${NAMESPACE}Helpers)

elseif (USE_DEVICESETTINGS)
    find_package(DS)
    find_package(IARMBus)
    target_include_directories(${MODULE_NAME} PRIVATE ${IARMBUS_INCLUDE_DIRS}
        ${DS_INCLUDE_DIRS})
if (RDK_SERVICES_L1_TEST OR DRDK_SERVICES_L2_TEST)
    target_link_libraries(${MODULE_NAME} PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${IARMBUS_LIBRARIES}
        ${DS_LIBRARIES}
        ${NAMESPACE}::${NAMESPACE}Helpers)
else()   
    target_link_libraries(${MODULE_NAME} PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${IARMBUS_LIBRARIES}
        ${DS_LIBRARIES}
        displayinfo-soc
        ${NAMESPACE}::${NAMESPACE}Helpers)
endif()

    target_sources(${MODULE_NAME}
        PRIVATE
            DeviceSettings/DS_IARM/PlatformImplementation.cpp)
    if (BUILD_RPI)
        target_sources(${MODULE_NAME}
            PRIVATE
                DeviceSettings/RPI/SoC_abstraction.cpp
                DeviceSettings/RPI/kms.c)
    endif()

elseif (NXCLIENT_FOUND AND NEXUS_FOUND)
    if (DISPLAYINFO_IMPLEMENTATION_PATH)
        target_sources(${MODULE_NAME}
            PRIVATE
                ${DISPLAYINFO_IMPLEMENTATION_PATH}/PlatformImplementation.cpp)
    else()
        include(GetExternalCode)
        set(DISPLAYINFO_IMPLEMENTATION_VERSION "master" CACHE STRING "DisplayInfo implementation version: commit_id_or_tag_or_branch_name")
        set(DISPLAYINFO_IMPLEMENTATION_REPOSITORY "https://code.rdkcentral.com/r/soc/broadcom/components/rdkcentral/thundernanoservices/DisplayInfo" CACHE STRING "DisplayInfo implementation repository")
        GetExternalCode(
            GIT_REPOSITORY ${DISPLAYINFO_IMPLEMENTATION_REPOSITORY}
            GIT_VERSION ${DISPLAYINFO_IMPLEMENTATION_VERSION}
            SOURCE_DIR "Nexus"
        )
        target_sources(${MODULE_NAME}
            PRIVATE
                Nexus/PlatformImplementation.cpp)
    endif()
    target_link_libraries(${MODULE_NAME}
        PRIVATE
            NEXUS::NEXUS
            NXCLIENT::NXCLIENT)

elseif (BCM_HOST_FOUND)
    target_sources(${MODULE_NAME}
        PRIVATE
            RPI/PlatformImplementation.cpp)
    target_link_libraries(${MODULE_NAME}
        PRIVATE
            BCM_HOST::BCM_HOST)

elseif (LIBDRM_FOUND)
    target_sources(${MODULE_NAME}
        PRIVATE
	Linux/PlatformImplementation.cpp)

    target_link_libraries(${MODULE_NAME}
        PRIVATE
            drm)
else ()
    message(FATAL_ERROR "There is no graphic backend for display info plugin")
endif ()

install(TARGETS ${MODULE_NAME}
    DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
