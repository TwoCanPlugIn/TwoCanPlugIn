##---------------------------------------------------------------------------
## Author:      Steven Adler (based on standard OpenCPN Plug-In CMAKE commands)
## Copyright:   2018
## License:     GPL v3+
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(twocan_pi)

SET(PACKAGE_NAME twocan_pi)
SET(VERBOSE_NAME TWOCAN)
SET(TITLE_NAME TWOCAN)
SET(CPACK_PACKAGE_CONTACT "TwoCanPlugin@hotmail.com")
SET(CMAKE_BUILD_TYPE Release)
SET(CMAKE_CXX_STANDARD 11)

INCLUDE("cmake/PluginConfigure.cmake")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/inc ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/img)

SET(SRC_TWOCAN
            src/twocanerror.cpp
            inc/twocanerror.h
            src/twocandevice.cpp
            inc/twocandevice.h
            src/twocanplugin.cpp
            inc/twocanplugin.h
            src/twocansettings.cpp
            inc/twocansettings.h
            src/twocansettingsbase.cpp
            inc/twocansettingsbase.h
            src/twocanutils.cpp
            inc/twocanutils.h
            inc/twocanversion.h
            inc/twocanicons.h
            src/twocanicons.cpp
 	)

IF(UNIX AND NOT APPLE)
    LIST(APPEND SRC_TWOCAN 
	    src/twocansocket.cpp
            inc/twocansocket.h
            src/twocanlogreader.cpp
            inc/twocanlogreader.h
    )
ENDIF(UNIX AND NOT APPLE)

ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_TWOCAN} )

IF(WIN32)
	TARGET_LINK_LIBRARIES(${PACKAGE_NAME} iphlpapi)
ENDIF(WIN32)


INCLUDE("cmake/PluginInstall.cmake")
INCLUDE("cmake/PluginLocalization.cmake")
INCLUDE("cmake/PluginPackage.cmake")

