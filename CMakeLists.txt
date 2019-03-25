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

SET(VERSION_MAJOR "1")
SET(VERSION_MINOR "3")

SET(CMAKE_BUILD_TYPE Release)

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
            inc/version.h
            img/twocan_16_icon.xpm
            img/twocan_32.xpm
            img/twocan_64.xpm
            img/twocan_128.xpm
 	)

IF(UNIX)
    LIST(APPEND SRC_TWOCAN 
	    src/twocansocket.cpp
            inc/twocansocket.h
            src/twocanlogreader.cpp
            inc/twocanlogreader.h
    )
ENDIF(UNIX)

ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_TWOCAN} )

INCLUDE("cmake/PluginInstall.cmake")
INCLUDE("cmake/PluginLocalization.cmake")
INCLUDE("cmake/PluginPackage.cmake")

