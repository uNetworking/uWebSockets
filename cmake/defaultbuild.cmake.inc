#
# This is a common includable CMakeList.txt that all adev builds use.
# if has all the platform specific stuff here so that you don't have
# to maintain this information in individual project CMakeLists.txt files.
#

cmake_policy(SET CMP0014 NEW)
cmake_policy(SET CMP0042 NEW)
cmake_policy(SET CMP0056 NEW)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${COMMONCMAKE}/Modules")

include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckStructHasMember)
include(UHDComponent)

##############################################################
# Add definition for host OS
##############################################################

message(STATUS "System name: '${CMAKE_SYSTEM_NAME}'")
IF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    ADD_DEFINITIONS(-DLINUX)
    IF( EXISTS "/opt/vc/include/bcm_host.h" )
        message(STATUS "Raspberry Pi System Detected")
        ADD_DEFINITIONS(-DRASPI)
        SET(RASPI "RASPI")
    ELSE()
        message(STATUS "Linux System Detected")
    ENDIF()
ELSEIF(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    message(STATUS "OS X System Detected")
    ADD_DEFINITIONS(-DAPPLE)
ELSEIF(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    message(STATUS "Android System Detected")
    ADD_DEFINITIONS(-DANDROID)
    IF( CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        ADD_DEFINITIONS(-DANDROID64)
    ELSE()
        ADD_DEFINITIONS(-DANDROID32)
    ENDIF()

ELSEIF(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    message(STATUS "Windows System Detected")
    ADD_DEFINITIONS(-DWINDOWS)

    IF( CMAKE_SYSTEM_PROCESSOR MATCHES "64$")
        ADD_DEFINITIONS(-DWIN64)
    ELSE()
        ADD_DEFINITIONS(-DWIN32)
    ENDIF()
ELSE()
    message(FATAL_ERROR "Unexpected system name: ${CMAKE_SYSTEM_NAME}")
ENDIF()

##############################################################
# Compiler specific setup
##############################################################

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckCXXCompilerFlag)

MACRO(ADD_OPTIONAL_COMPILER_FLAG type flag)
    IF(${type} STREQUAL "ALL")

        CHECK_C_COMPILER_FLAG(${flag} HAVE_DUMMY)
        IF(HAVE_DUMMY)
            ADD_DEFINITIONS(${flag})
        ENDIF()
        UNSET(HAVE_DUMMY)

    ELSEIF(${type} STREQUAL "C")

        CHECK_C_COMPILER_FLAG(${flag} HAVE_DUMMY)
        IF(HAVE_DUMMY)
            SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
        ENDIF()
        SET(HAVE_DUMMY)
        UNSET(HAVE_DUMMY)

    ELSEIF(${type} STREQUAL "CXX")

        CHECK_CXX_COMPILER_FLAG(${flag} HAVE_DUMMY)
        IF(HAVE_DUMMY)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
        ENDIF()
        SET(HAVE_DUMMY)
        UNSET(HAVE_DUMMY)

    ENDIF()
ENDMACRO(ADD_OPTIONAL_COMPILER_FLAG)

ADD_DEFINITIONS(-DTENSOR_THREAD_SAFE)


# Define debug defs if building debug.
IF( ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    ADD_DEFINITIONS(-D_DEBUG)
ENDIF()

# Define EXCLUDE_ENCRYPTION preprocessor macro
OPTION(CMAKE_EXCLUDE_ENCRYPTION "Define EXCLUDE_ENCRYPTION directive" OFF)
IF(CMAKE_EXCLUDE_ENCRYPTION)
    ADD_DEFINITIONS(-DEXCLUDE_ENCRYPTION)
ENDIF(CMAKE_EXCLUDE_ENCRYPTION)

# Define CPU_ARCH_TYPE if not defined
IF(NOT CMAKE_CPU_ARCH_TYPE)
    SET(CMAKE_CPU_ARCH_TYPE "native")
    MESSAGE(STATUS "CPU architecture type not specified: defaulting to native.")
ENDIF()
SET(CMAKE_CPU_ARCH_TYPE ${CMAKE_CPU_ARCH_TYPE} CACHE STRING "")


# compiler switches ....

# ONLY do this if the CXX compiler is being used
IF(CMAKE_CXX_COMPILER_ID)
IF(CMAKE_COMPILER_IS_GNUCXX)
    message(STATUS "GNU C++ Detected")
    # We are not using this yet in TENSOR_PORTABLE_FRAMEWORK,
    # see the url for more info.
    # http://gcc.gnu.org/wiki/Visibility
    # ADD_OPTIONAL_COMPILER_FLAG(CXX -fvisibility=hidden)
    # ADD_OPTIONAL_COMPILER_FLAG(CXX -fvisibility-inlines-hidden)

    ADD_OPTIONAL_COMPILER_FLAG(ALL -fmessage-length=0)
    ADD_OPTIONAL_COMPILER_FLAG(ALL -fPIC)

    IF( ${RASPI} )
		#-O2 -Otime
		ADD_OPTIONAL_COMPILER_FLAG(CXX -marm)
		ADD_OPTIONAL_COMPILER_FLAG(CXX -mabi=aapcs-linux)
		#-march=armv8-a+crc
		ADD_OPTIONAL_COMPILER_FLAG(CXX -mfloat-abi=hard)
		#other options are 'fp-armv8', 'neon-fp-armv8', and 'crypto-neon-fp-armv8'
		ADD_OPTIONAL_COMPILER_FLAG(CXX -mfpu=neon-fp-armv8)
		ADD_OPTIONAL_COMPILER_FLAG(CXX -funsafe-math-optimizations)
		ADD_OPTIONAL_COMPILER_FLAG(CXX -mhard-float)
		ADD_OPTIONAL_COMPILER_FLAG(CXX -mlittle-endian)
		#or just gnu if gnu2 doesn't work
		ADD_OPTIONAL_COMPILER_FLAG(CXX -mtls-dialect=gnu2)
		ADD_OPTIONAL_COMPILER_FLAG(CXX -mtune=cortex-a53)
		ADD_OPTIONAL_COMPILER_FLAG(CXX -munaligned-access)
		ADD_OPTIONAL_COMPILER_FLAG(CXX -mvectorize-with-neon-quad)
	ELSE()
        IF( NOT ${CMAKE_CPU_ARCH_TYPE} STREQUAL "generic")
          ADD_OPTIONAL_COMPILER_FLAG(CXX -march=${CMAKE_CPU_ARCH_TYPE})
        ENDIF()
        ADD_OPTIONAL_COMPILER_FLAG(CXX -mtune=${CMAKE_CPU_ARCH_TYPE})

        ADD_OPTIONAL_COMPILER_FLAG(CXX -m64)
        ADD_OPTIONAL_COMPILER_FLAG(CXX -msse2)
        ADD_OPTIONAL_COMPILER_FLAG(CXX -mfpmath=sse)
    ENDIF()

    ADD_OPTIONAL_COMPILER_FLAG(CXX -Wall)
    ADD_DEFINITIONS(-Wuninitialized)
    ADD_OPTIONAL_COMPILER_FLAG(CXX -Wextra)
    ADD_OPTIONAL_COMPILER_FLAG(CXX -Wsign-compare)
    ADD_OPTIONAL_COMPILER_FLAG(CXX -Wnon-virtual-dtor)
    #ADD_OPTIONAL_COMPILER_FLAG(CXX -Weffc++)
    #ADD_OPTIONAL_COMPILER_FLAG(CXX -Wold-style-cast)
    #ADD_OPTIONAL_COMPILER_FLAG(CXX -pedantic)  
    #ADD_OPTIONAL_COMPILER_FLAG(CXX -Wconversion)


    # lots of deprecated declarations in the Eigen library
    ADD_OPTIONAL_COMPILER_FLAG(CXX -Wno-deprecated-declarations)

    IF( NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      ADD_OPTIONAL_COMPILER_FLAG(CXX -s)
    ENDIF()

ELSEIF ( ${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
    message(STATUS "Clang C++ Detected")

    ADD_OPTIONAL_COMPILER_FLAG(ALL -fmessage-length=0)
    ADD_OPTIONAL_COMPILER_FLAG(ALL -fPIC)

    IF( NOT ${RASPI} )
        IF( NOT ${CMAKE_CPU_ARCH_TYPE} STREQUAL "generic")
          ADD_OPTIONAL_COMPILER_FLAG(CXX -march=${CMAKE_CPU_ARCH_TYPE})
        ENDIF()
        ADD_OPTIONAL_COMPILER_FLAG(CXX -mtune=${CMAKE_CPU_ARCH_TYPE})

        ADD_OPTIONAL_COMPILER_FLAG(CXX -m64)
        ADD_OPTIONAL_COMPILER_FLAG(CXX -msse2)
        ADD_OPTIONAL_COMPILER_FLAG(CXX -mfpmath=sse)
    ENDIF()

    ADD_OPTIONAL_COMPILER_FLAG(CXX -Wall)
    ADD_DEFINITIONS(-Wuninitialized)
    ADD_OPTIONAL_COMPILER_FLAG(CXX -Wextra)
    ADD_OPTIONAL_COMPILER_FLAG(CXX -Wsign-compare)
    ADD_OPTIONAL_COMPILER_FLAG(CXX -Wnon-virtual-dtor)

    # since there is lots of generated code without implementations (or doesn't use all parameters)
    #ADD_OPTIONAL_COMPILER_FLAG(CXX -Wno-unused-parameter)

    # don't warn on unused fields and variables (could remove later to clean up, but these are annoying for now).
    ADD_OPTIONAL_COMPILER_FLAG(CXX -Wno-unused-private-field)
    ADD_OPTIONAL_COMPILER_FLAG(CXX -Wno-unused-const-variable)
    #ADD_OPTIONAL_COMPILER_FLAG(CXX -Wno-unused-variable)

    # the following lines suppress some warnings that are common in the TPF code.
    # we should probably clean up this code, but seeing tons of warnings is a distraction
    # ADD_OPTIONAL_COMPILER_FLAG(CXX -Wno-deprecated-register)
    # ADD_OPTIONAL_COMPILER_FLAG(CXX -Wno-unused-local-typedef)
    #ADD_OPTIONAL_COMPILER_FLAG(CXX -Weffc++)
    #ADD_OPTIONAL_COMPILER_FLAG(CXX -Wold-style-cast)
    #ADD_OPTIONAL_COMPILER_FLAG(CXX -pedantic)  
    #ADD_OPTIONAL_COMPILER_FLAG(CXX -Wconversion)

ELSEIF ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    message(STATUS "Microsoft C++ Detected")
    ADD_OPTIONAL_COMPILER_FLAG(CXX /wd4996)
    #ADD_DEFINITIONS(-D_UNICODE)
    #ADD_DEFINITIONS(-DUNICODE)
ELSE ()
    message(FATAL_ERROR "Unexpected compiler ${CMAKE_CXX_COMPILER_ID}.  Please update CMakeLists.txt")
ENDIF()
ENDIF()

message ("CMAKE_CXX_FLAGS are ${CMAKE_CXX_FLAGS}")

# ONLY do this if the C compiler is being used
IF(CMAKE_C_COMPILER_ID)
IF( ${CMAKE_C_COMPILER_ID} STREQUAL GNU )
    message(STATUS "GNU C Detected")
    ADD_OPTIONAL_COMPILER_FLAG(C -Wall)
    ADD_OPTIONAL_COMPILER_FLAG(C -std=c99)
    ADD_OPTIONAL_COMPILER_FLAG(C -Wno-unused-value)
    ADD_OPTIONAL_COMPILER_FLAG(C -Wno-format)

    ADD_OPTIONAL_COMPILER_FLAG(ALL -fmessage-length=0)

    IF( ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
      ADD_OPTIONAL_COMPILER_FLAG(C -O0)
      #ADD_OPTIONAL_COMPILER_FLAG(C -g)
    ELSE()
      #ADD_OPTIONAL_COMPILER_FLAG(C -O3)
      ADD_OPTIONAL_COMPILER_FLAG(C -s)
    ENDIF()
ELSEIF ( ${CMAKE_C_COMPILER_ID} MATCHES Clang)
    message(STATUS "Clang C Detected")
    ADD_OPTIONAL_COMPILER_FLAG(C -Wall)
    ADD_OPTIONAL_COMPILER_FLAG(C -std=c99)
    ADD_OPTIONAL_COMPILER_FLAG(C -Wno-unused-value)
    ADD_OPTIONAL_COMPILER_FLAG(C -Wno-format)

ELSEIF ( ${CMAKE_C_COMPILER_ID} STREQUAL MSVC)
    message(STATUS "Microsoft C Detected")
    #ADD_OPTIONAL_COMPILER_FLAG(CXX /wd4996)
    #ADD_DEFINITIONS(-D_UNICODE)
    #ADD_DEFINITIONS(-DUNICODE)
ELSE ()
    message(FATAL_ERROR "Unexpected compiler ${CMAKE_C_COMPILER_ID}.  Please update CMakeLists.txt")
ENDIF()
ENDIF()

message ("CMAKE_C_FLAGS are ${CMAKE_C_FLAGS}")



