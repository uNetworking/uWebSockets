##############################################################
# Verify VPATH build
##############################################################

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "Prevented in-tree build. This is bad practice.")
endif(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

##############################################################
# Project setup
##############################################################

CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
SET(PROJECT_NAME uWebSockets)
PROJECT(${PROJECT_NAME} CXX C)
ENABLE_TESTING()

# select the release build type by default to get optimization flags

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "Release")
    MESSAGE(STATUS "Build type not specified: defaulting to release.")
ENDIF()

SET(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

# Set the version information here

SET(VERSION_MAJOR 0)
SET(VERSION_API_COMPAT 1)
SET(VERSION_MINOR 0)
SET(VERSION_MAINT git)

SET(${PROJECT_NAME}_VERSION_FULL
    ${VERSION_MAJOR}.${VERSION_API_COMPAT}.${VERSION_MINOR}
)
SET(${PROJECT_NAME}_VERSION_PATCH
    ${VERSION_MAJOR}.${VERSION_API_COMPAT}
)

##############################################################
# Set up CMake Module Path and Include Modules
##############################################################
SET( COMMONCMAKE ${CMAKE_SOURCE_DIR}/../cmake )
message(STATUS ${COMMONCMAKE}/defaultbuild.cmake.inc)
include(${COMMONCMAKE}/defaultbuild.cmake.inc)

##############################################################
# Define install directories
##############################################################

# when the library suffix should be 64 (applies to redhat linux family)
IF(NOT DEFINED LIB_SUFFIX AND CMAKE_SYSTEM_PROCESSOR MATCHES "64$" AND 
   ${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND ${CMAKE_SYSTEM_NAME} MATCHES "RedHat")
    SET(LIB_SUFFIX 64)
    MESSAGE(STATUS "64-bit RedHat Linux Detected.  Setting LIB_SUFFIX to 64")
ENDIF()
SET(LIB_SUFFIX ${LIB_SUFFIX} CACHE STRING "lib directory suffix")
SET(RUNTIME_DIR bin)
SET(LIBRARY_DIR lib${LIB_SUFFIX})
SET(XID_INCLUDE_DIR include/XID)
SET(PKG_DEFS_DIR defs/${PROJECT_NAME})
SET(INCLUDE_DIR include/${PROJECT_NAME})
SET(PKG_DATA_DIR share/${PROJECT_NAME})
SET(PKG_DOC_DIR share/doc/${PROJECT_NAME})
SET(DOCS_DIR docs)
SET(PKG_MAN_DIR share/man/man1)

##############################################################
# Setup common include paths within this build
##############################################################

#set the generic SDK path
set( sdkpath ${CMAKE_INSTALL_PREFIX})
set( sdkincpath ${sdkpath}/include)
set( sdklibpath ${sdkpath}/lib${LIB_SUFFIX})

INCLUDE_DIRECTORIES( ${sdkincpath} ${CMAKE_CURRENT_SOURCE_DIR}/include)

LINK_DIRECTORIES( ${sdklibpath} )

#Add subdirectories.
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(examples)
#ADD_SUBDIRECTORY(test)


