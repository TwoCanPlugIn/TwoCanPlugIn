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
set(VERSION_MAJOR "2")
set(VERSION_MINOR "1")
set(VERSION_PATCH "0")
set(VERSION_TWEAK "0")
set(VERSION_DATE "10/10/2021")
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

set(XML_INFO_URL "https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:opencpn_user_manual:plugins:other:twocan_pi")
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

## Provide path to additional CMAKE files (extension is assumed to be .cmake)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include("PluginSetup")

##
## ----- Modify section below if there are special requirements for the plugin ----- ##
##

set(CMAKE_VERBOSE_MAKEFILE "Activate verbose mode for make files" ON)

option(Plugin_CXX11 "Use c++11" OFF)


##
## ----- do not change next section - needed to configure build process ----- ##
##

include("PluginConfigure")

##
## ----- Change below to match project requirements for source, headers, etc. ----- ##
##

## TwoCan Plugin does not understand/use this definition
##add_definitions(-DUSE_S57)

##TwoCan plugin will use SVG for toolbar icons
add_definitions(-DPLUGIN_USE_SVG)

## the media player control uses json
if(APPLE)
    option(PI_JSON_SCHEMA_VALIDATOR "Use JSON Schema validator" OFF)
else(APPLE)
    option(PI_JSON_SCHEMA_VALIDATOR "Use JSON Schema validator" ON)
endif(APPLE)

## Needed for ocpn_plugin.h
INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/libs/ocpn-api)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/inc ${CMAKE_SOURCE_DIR}/nmea183/inc ${CMAKE_SOURCE_DIR}/wxJSON/include)

SET(SOURCES src/twocanerror.cpp
            src/twocandevice.cpp
            src/twocanplugin.cpp
            src/twocansettings.cpp
            src/twocansettingsbase.cpp
            src/twocanutils.cpp
            src/twocanicons.cpp
            src/twocanencoder.cpp
            src/twocanautopilot.cpp
            src/twocanautopilotdialog.cpp
            src/twocanautopilotdialogbase.cpp
            src/twocanais.cpp
            src/twocanmedia.cpp)

SET(HEADERS inc/twocanerror.h
            inc/twocandevice.h
            inc/twocanplugin.h
            inc/twocansettings.h
            inc/twocansettingsbase.h
            inc/twocanutils.h
            inc/twocanicons.h
            inc/twocanencoder.h
            inc/twocanautopilot.h
            inc/twocanautopilotdialog.h
            inc/twocanautopilotdialogbase.h
            inc/twocanais.h
            inc/twocanmedia.h)

SET(NMEA183_SRC nmea183/src/apb.cpp
                nmea183/src/bod.cpp
                nmea183/src/bwr.cpp
                nmea183/src/dbt.cpp
                nmea183/src/dpt.cpp
		nmea183/src/dsc.cpp
		nmea183/src/dse.cpp
                nmea183/src/expid.cpp
                nmea183/src/gga.cpp
                nmea183/src/gll.cpp
                nmea183/src/gpwpl.cpp
                nmea183/src/gsv.cpp
                nmea183/src/hdg.cpp
                nmea183/src/hdm.cpp
                nmea183/src/hdt.cpp
                nmea183/src/hexvalue.cpp
                nmea183/src/lat.cpp
                nmea183/src/latlong.cpp
                nmea183/src/long.cpp
		nmea183/src/mob.cpp
                nmea183/src/mtw.cpp
                nmea183/src/mwv.cpp
                nmea183/src/nmea0183.cpp
                nmea183/src/response.cpp
                nmea183/src/rmb.cpp
                nmea183/src/rmc.cpp
                nmea183/src/rot.cpp
                nmea183/src/rpm.cpp
                nmea183/src/rsa.cpp
                nmea183/src/rte.cpp
                nmea183/src/sentence.cpp
                nmea183/src/talkerid.cpp
                nmea183/src/vdm.cpp
                nmea183/src/vdr.cpp
                nmea183/src/vhw.cpp
                nmea183/src/vlw.cpp
                nmea183/src/vtg.cpp
                nmea183/src/wcv.cpp
                nmea183/src/wpl.cpp
                nmea183/src/xdr.cpp
                nmea183/src/xte.cpp
                nmea183/src/zda.cpp)

SET(NMEA183_INC nmea183/inc/apb.hpp
                nmea183/inc/bod.hpp
                nmea183/inc/bwr.hpp
                nmea183/inc/dbt.hpp
                nmea183/inc/dpt.hpp
		nmea183/inc/dsc.hpp
		nmea183/inc/dse.hpp
                nmea183/inc/gga.hpp
                nmea183/inc/gll.hpp
                nmea183/inc/gpwpl.hpp
                nmea183/inc/gsv.hpp
                nmea183/inc/hdg.hpp
                nmea183/inc/hdm.hpp
                nmea183/inc/hdt.hpp
                nmea183/inc/latlong.hpp
		nmea183/inc/mob.hpp
                nmea183/inc/mtw.hpp
                nmea183/inc/mwv.hpp
                nmea183/inc/nmea0183.h
                nmea183/inc/nmea0183.hpp
                nmea183/inc/response.hpp
                nmea183/inc/rmb.hpp
                nmea183/inc/rmc.hpp
                nmea183/inc/rot.hpp
                nmea183/inc/rpm.hpp
                nmea183/inc/rsa.hpp
                nmea183/inc/rte.hpp
                nmea183/inc/satinfo.h
                nmea183/inc/sentence.hpp
                nmea183/inc/vdm.hpp
                nmea183/inc/vdr.hpp
                nmea183/inc/vhw.hpp
                nmea183/inc/vlw.hpp
                nmea183/inc/vtg.hpp
                nmea183/inc/wcv.hpp
                nmea183/inc/wpl.hpp
                nmea183/inc/xdr.hpp
                nmea183/inc/xte.hpp
                nmea183/inc/zda.hpp)

SET(JSON wxJSON/src/jsonreader.cpp
                wxJSON/src/jsonwriter.cpp
                wxJSON/src/jsonval.cpp)

IF(UNIX AND NOT APPLE)

    LIST(APPEND SOURCES 
	src/twocansocket.cpp
        src/twocanlogreader.cpp
        src/twocaninterface.cpp
        src/twocanpcap.cpp)

    LIST(APPEND HEADERS
        inc/twocansocket.h
        inc/twocanlogreader.h
        inc/twocaninterface.h
        inc/twocanpcap.h)

ENDIF(UNIX AND NOT APPLE)

IF(APPLE)

    LIST(APPEND SOURCES 
        src/twocanlogreader.cpp
        src/twocanmacserial.cpp
        src/twocanmactoucan.cpp
        src/twocanmackvaser.cpp
        src/twocaninterface.cpp
        src/twocanpcap.cpp)

    LIST(APPEND HEADERS
        inc/twocanlogreader.h
        inc/twocanmacserial.h
        inc/twocanmactoucan.h
        inc/twocanmackvaser.h
        inc/twocaninterface.h
        inc/twocanpcap.h)

    # For Rusoku Toucan & Kvaser interfaces, The MacCan Rusoku & Kvaser headers have been manually copied to this location
    # refer to ci/circleci-build-macos.sh under the Rusoku & Kvaser sections
    SET(MACCAN_HEADER "/usr/local/include")
    INCLUDE_DIRECTORIES(AFTER ${MACCAN_HEADER})

ENDIF(APPLE)

TARGET_SOURCES(${PACKAGE_NAME} PUBLIC ${SOURCES} ${HEADERS} ${NMEA183_SRC} ${NMEA183_INC} ${JSON})

IF(WIN32)
# This library is used to obtain a MAC address and hence derive a NMEA 2000 Unique Id
	TARGET_LINK_LIBRARIES(${PACKAGE_NAME} iphlpapi)
ENDIF(WIN32)

IF(APPLE)
    # use runtime paths
    set(CMAKE_MACOSX_RPATH TRUE)
    
    # Refer to ci/circleci_build-macos.sh as we build the Toucan & Kvaser library and copy the dylib to data/drivers
    # That script also creates a symbolic link, which is used both for the dynamic linking  and also for creating te correct 
    # runtime path. Note that using a symbolic link isolates the plugin from any version changes to the Rusolu & Kvaser libraries 

    FIND_LIBRARY(RUSOKU_LIBRARY
    NAMES libTouCAN.dylib
    PATHS usr/local/lib
    )

    FIND_LIBRARY(KVASER_LIBRARY
    NAMES libKvaserCAN.dylib
    PATHS usr/local/lib
    )

    TARGET_LINK_LIBRARIES(${PACKAGE_NAME} ${RUSOKU_LIBRARY} ${KVASER_LIBRARY})

    # As we install the Rusoku library in the plugin's data/drivers directory we need to "patch" the dylib  with the correct runtime paths.
    ADD_CUSTOM_COMMAND( POST_BUILD
        TARGET ${PACKAGE_NAME}
        # Add the drivers directory as a runtime path to the plugin 
        COMMAND install_name_tool -add_rpath "@loader_path/./../SharedSupport/plugins/${PACKAGE_NAME}/data/drivers" "lib${PACKAGE_NAME}.dylib"
        # Add the runtime path to the symbolic link for the Rusoku library
        COMMAND install_name_tool -change libTouCAN.dylib @rpath/libTouCAN.dylib lib${PACKAGE_NAME}.dylib
    )
    
    # Similarly we add the runtime path to the symbolic link for the Kvaser library 
    ADD_CUSTOM_COMMAND( POST_BUILD
        TARGET ${PACKAGE_NAME} 
        COMMAND install_name_tool -change libKvaserCAN.dylib @rpath/libKvaserCAN.dylib lib${PACKAGE_NAME}.dylib
    )

ENDIF(APPLE)

##
## ----- do not change next section - needed to configure build process ----- ##
##

include("PluginInstall")
include("PluginLocalization")
include("PluginPackage")

