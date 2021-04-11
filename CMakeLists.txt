# ---------------------------------------------------------------------------
# Author:      Steven Adler based on (jongough aka. Jon Gough) Copyright: License:     wxWidgets License
# ---------------------------------------------------------------------------

## ----- When changing this file do NOT change the order in which sections occur        ----- ##
## ----- Changes should only be made between the section blocks that identify where     ----- ##
## ----- these changes should be. The whole configuration process relies on this        ----- ##
## ----- sequence to be successful                                                      ----- ##
##

##
## ----- Modify section below to include all the details for your plugin ----- ##
##


set(CMLOC "CMakeLists: ")

# define minimum cmake version
cmake_minimum_required(VERSION 3.1.1)
if(COMMAND cmake_policy)
    if(POLICY CMP0043)
        cmake_policy(SET CMP0043 NEW)
    endif(POLICY CMP0043)
    cmake_policy(SET CMP0048 NEW)
    if(POLICY CMP0077)
        cmake_policy(SET CMP0077 NEW)
    endif(POLICY CMP0077)
endif(COMMAND cmake_policy)


# define plugin name, owner and versions
set(VERBOSE_NAME "TwoCan_Plugin")
set(COMMON_NAME "TwoCan Plugin")
set(TITLE_NAME "TWOCANPLUGIN")
set(PACKAGE_CONTACT "twocanplugin@hotmail.com")
set(PACKAGE "twocan_plugin")
set(SHORT_DESCRIPTION "TwoCan Plugin, integrates OpenCPN with NMEA 2000 networks")
set(LONG_DESCRIPTION "TwoCan Plugin, integrates OpenCPN with NMEA 2000 networks by converting NMEA 2000 messages to NMEA 183 sentences for use by OpenCPN") 
set(VERSION_MAJOR "1")
set(VERSION_MINOR "9")
set(VERSION_PATCH "3")
set(VERSION_TWEAK "0")
set(VERSION_DATE "10/04/2021")
set(OCPN_MIN_VERSION "ov50")
set(OCPN_API_VERSION_MAJOR "1")
set(OCPN_API_VERSION_MINOR "16")
set(TP_COMMENT "Release for O5 using CI")

set(PARENT "opencpn")

# The next line allows for setting up your own local webserver with git on it for testing purposes, the default is github.com
set(GIT_REPOSITORY_SERVER "github.com")

# Specifies cloudsmith user and repository names (note suffixes for repositories production, beta & alpha)
set(PROD "prod") # Standard Repos
set(BETA "beta") # Standard Repos
set(ALPHA "alpha") # Standard Repos

set(CLOUDSMITH_USER "steven-adler")
# set this if your cloudsmith repository name does not match your git repository name 
set(CLOUDSMITH_BASE_REPOSITORY "twocanplugin")

set(XML_INFO_URL "https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:developer_manual:plugins:beta_plugins:twocan_pi")
set(XML_SUMMARY ${SHORT_DESCRIPTION})
set(XML_DESCRIPTION ${LONG_DESCRIPTION})

##
## ----- Modify section below if there are special requirements for the plugin ----- ##
##

## TwoCan Plugin will eventually uses SVG graphics for the toolbar icons
option(PLUGIN_USE_SVG "Use SVG graphics" ON)

set(CMAKE_CXX_STANDARD 11)

# Prefer libGL.so to libOpenGL.so, see CMP0072
set(OpenGL_GL_PREFERENCE "LEGACY")
# Don't use local version of GLU library
set(USE_LOCAL_GLU FALSE)
option(USE_GL "Enable OpenGL support" OFF)
message(STATUS "${CMLOC}USE_GL: ${USE_GL}")

## Define the build type
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE
        "Release"
        CACHE STRING "Choose the type of build" FORCE)
endif("${CMAKE_BUILD_TYPE}" STREQUAL "")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "" "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
message(STATUS "${CMLOC}Build type: ${CMAKE_BUILD_TYPE}")

##
## ----- The statements below are used to setup standard variables that are required by the CMAKE process - do not remove ----- ##
##

project(${PACKAGE})

include("cmake/PluginSetup.cmake")

##
## ----- Modify section below if there are special requirements for the plugin ----- ##
##

set(CMAKE_VERBOSE_MAKEFILE ON)

option(Plugin_CXX11 "Use c++11" OFF)


##
## ----- do not change next section - needed to configure build process ----- ##
##

include("cmake/PluginConfigure.cmake")

##
## ----- Change below to match project requirements for source, headers, etc. ----- ##
##

## TwoCan Plugin does not understand/use this definition
##add_definitions(-DUSE_S57)

##TwoCan plugin will use SVG for toolbar icons
add_definitions(-DPLUGIN_USE_SVG)

## We don't use JSON
##if(APPLE)
##    option(PI_JSON_SCHEMA_VALIDATOR "Use JSON Schema validator" OFF)
##else(APPLE)
##    option(PI_JSON_SCHEMA_VALIDATOR "Use JSON Schema validator" ON)
##endif(APPLE)

## Need api-16 for ocpn_plugin.h
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/api-16)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/inc ${CMAKE_SOURCE_DIR}/src)

SET(SOURCES src/twocanerror.cpp
            src/twocandevice.cpp
            src/twocanplugin.cpp
            src/twocansettings.cpp
            src/twocansettingsbase.cpp
            src/twocanutils.cpp
            src/twocanicons.cpp)

SET(HEADERS inc/twocanerror.h
            inc/twocandevice.h
            inc/twocanplugin.h
            inc/twocansettings.h
            inc/twocansettingsbase.h
            inc/twocanutils.h
            inc/twocanicons.h)

IF(UNIX AND NOT APPLE)

    LIST(APPEND SOURCES 
	src/twocansocket.cpp
        src/twocanlogreader.cpp
        src/twocaninterface.cpp)

    LIST(APPEND HEADERS
        inc/twocansocket.h
        inc/twocanlogreader.h
        inc/twocaninterface.h)

ENDIF(UNIX AND NOT APPLE)

IF(APPLE)

    LIST(APPEND SOURCES 
        src/twocanlogreader.cpp
        src/twocanmacserial.cpp
        src/twocanmactoucan.cpp
        src/twocaninterface.cpp)

    LIST(APPEND HEADERS
        inc/twocanlogreader.h
        inc/twocanmacserial.h
        inc/twocanmactoucan.h
        inc/twocaninterface.h)

    # For Rusoku Toucan, Assumes the MacCan Rusoku headers have been installed in the default location
    SET(RUSOKU_HEADER "/usr/local/include")
    INCLUDE_DIRECTORIES(AFTER ${RUSOKU_HEADER})

ENDIF(APPLE)

ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SOURCES} ${HEADERS} )

IF(WIN32)
	TARGET_LINK_LIBRARIES(${PACKAGE_NAME} iphlpapi)
ENDIF(WIN32)

IF(APPLE)
    # use runtime paths
    set(CMAKE_MACOSX_RPATH TRUE)
    
    # Refer to circleci_build-macos as we build the Toucan library and copy the dylib to data/drivers
    # So that is the location where will link, but will also need to specify the rpath

    FIND_LIBRARY(RUSOKU_LIBRARY
    NAMES libTouCAN.dylib
    PATHS usr/local/lib
    )

    TARGET_LINK_LIBRARIES(${PACKAGE_NAME} ${RUSOKU_LIBRARY})

    # As we install the Rusoku library in the plugin's data/drivers directory we need to "patch" the dylib  with the correct runtime paths.
    ADD_CUSTOM_COMMAND( POST_BUILD
      TARGET ${PACKAGE_NAME} 
      COMMAND install_name_tool -add_rpath "@loader_path/./../SharedSupport/plugins/${PACKAGE_NAME}/data/drivers" "lib${PACKAGE_NAME}.dylib"
      COMMAND install_name_tool -change libTouCAN.0.1.dylib @rpath/libTouCAN.0.1.dylib lib${PACKAGE_NAME}.dylib
    )
    
ENDIF(APPLE)

##
## ----- do not change next section - needed to configure build process ----- ##
##

include("cmake/PluginInstall.cmake")
include("cmake/PluginLocalization.cmake")
include("cmake/PluginPackage.cmake")

