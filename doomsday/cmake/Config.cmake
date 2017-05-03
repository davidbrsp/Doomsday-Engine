# Doomsday Engine -- General configuration
#
# All CMakeLists should include this file to gain access to the overall
# project configuration.

cmake_policy (SET CMP0053 OLD)  # Warning from Qt 5.8.0 modules

get_filename_component (_where "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
message (STATUS "Configuring ${_where}...")

macro (set_path var path)
    get_filename_component (${var} "${path}" REALPATH)
endmacro (set_path)

# Project Options & Paths ----------------------------------------------------

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message (FATAL_ERROR "In-source builds are not allowed. Please create a build directory and run CMake from there.")
endif ()

set_path (DENG_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set (DENG_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")
if (APPLE OR WIN32)
    set_path (DENG_DISTRIB_DIR "${DENG_SOURCE_DIR}/../distrib/products")
else ()
    set_path (DENG_DISTRIB_DIR /usr)
endif ()
set (DENG_EXTERNAL_SOURCE_DIR "${DENG_SOURCE_DIR}/external")
set (DENG_API_DIR "${DENG_SOURCE_DIR}/apps/api")
set (CMAKE_MODULE_PATH "${DENG_CMAKE_DIR}")
set (DENG_SDK_DIR "" CACHE PATH "Location of the Doomsday SDK to use for compiling")

if (CMAKE_CONFIGURATION_TYPES)
    set (DENG_CONFIGURATION_TYPES ${CMAKE_CONFIGURATION_TYPES})
else ()
    set (DENG_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})
endif ()

include (Macros)
include (Arch)
include (BuildTypes)
include (InstallPrefix)
include (Version)
find_package (Ccache)
include (Options)
include (Packaging)

if (UNIX AND NOT APPLE)
    include (GNUInstallDirs)
endif()

# Install directories.
set (DENG_INSTALL_DATA_DIR "share/doomsday")
set (DENG_INSTALL_DOC_DIR "share/doc")
set (DENG_INSTALL_MAN_DIR "share/man/man6")
if (DEFINED CMAKE_INSTALL_LIBDIR)
    set (DENG_INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR})
else ()
    set (DENG_INSTALL_LIB_DIR "lib")
endif ()
set (DENG_INSTALL_PLUGIN_DIR "${DENG_INSTALL_LIB_DIR}/doomsday")

set (DENG_BUILD_STAGING_DIR "${CMAKE_BINARY_DIR}/bundle-staging")
set (DENG_VS_STAGING_DIR "${CMAKE_BINARY_DIR}/products") # for Visual Studio

set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "client")

# Prefix path is used for finding CMake config packages.
if (NOT DENG_SDK_DIR STREQUAL "")
    set (CMAKE_PREFIX_PATH "${DENG_SDK_DIR}/${DENG_INSTALL_LIB_DIR}")
else ()
    set (CMAKE_PREFIX_PATH "${DENG_CMAKE_DIR}/config")
endif ()

# Qt Configuration -----------------------------------------------------------

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
set_property (GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER Generated)

find_package (Qt)

# Find Qt5 in all projects to ensure automoc is run.
list (APPEND CMAKE_PREFIX_PATH "${QT_PREFIX_DIR}")

# This will ensure automoc works in all projects.
if (QT_MODULE STREQUAL Qt5)
    find_package (Qt5 COMPONENTS Core Network)
else ()
    find_package (Qt4 REQUIRED)
endif ()

# Check for mobile platforms.
if (NOT QMAKE_XSPEC_CHECKED)
    qmake_query (xspec "QMAKE_XSPEC")
    set (QMAKE_XSPEC ${xspec} CACHE STRING "Value of QMAKE_XSPEC")
    set (QMAKE_XSPEC_CHECKED YES CACHE BOOL "QMAKE_XSPEC has been checked")
    mark_as_advanced (QMAKE_XSPEC)
    mark_as_advanced (QMAKE_XSPEC_CHECKED)
    if (QMAKE_XSPEC STREQUAL "macx-ios-clang")
        set (IOS YES CACHE BOOL "Building for iOS platform")
    endif ()
endif ()

# Platform-Specific Configuration --------------------------------------------

if (IOS)
    include (PlatformiOS)
elseif (APPLE)
    include (PlatformMacx)
elseif (WIN32)
    include (PlatformWindows)
else ()
    include (PlatformUnix)
endif ()

# Helpers --------------------------------------------------------------------

set (Python_ADDITIONAL_VERSIONS 2.7)
find_package (PythonInterp REQUIRED)

if (DENG_ENABLE_COTIRE)
    include (cotire)
endif ()
include (LegacyPK3s)
