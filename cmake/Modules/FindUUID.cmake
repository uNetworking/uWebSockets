# - Find UUID
# Find the native UUID includes and library
# This module defines
#  UUID_INCLUDE_DIR, where to find uuid.h, etc.
#  UUID_LIBRARIES, the libraries needed to use UUID.
#  UUID_FOUND, If false, do not try to use UUID.
# also defined, but not for general use are
#  UUID_LIBRARY, where to find the UUID library.
#
#  Copyright (c) 2006-2016 Mathieu Malaterre <mathieu.malaterre@gmail.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

# On Mac OS X the uuid functions are in the System library.
if(APPLE)
  set(UUID_LIBRARY_VAR System)
else()
  # Linux type:
  set(UUID_LIBRARY_VAR uuid)
endif()

find_library(UUID_LIBRARY
  NAMES ${UUID_LIBRARY_VAR}
  PATHS /lib /usr/lib /usr/local/lib
  )

# Must be *after* the lib itself
set(CMAKE_FIND_FRAMEWORK_SAVE ${CMAKE_FIND_FRAMEWORK})
set(CMAKE_FIND_FRAMEWORK NEVER)

find_path(UUID_INCLUDE_DIR uuid/uuid.h
/usr/local/include
/usr/include
)

if (UUID_LIBRARY AND UUID_INCLUDE_DIR)
  set(UUID_LIBRARIES ${UUID_LIBRARY})
  set(UUID_FOUND "YES")
else ()
  set(UUID_FOUND "NO")
endif ()


if (UUID_FOUND)
   if (NOT UUID_FIND_QUIETLY)
      message(STATUS "Found UUID: ${UUID_LIBRARIES}")
   endif ()
else ()
   if (UUID_FIND_REQUIRED)
      message( "library: ${UUID_LIBRARY}" )
      message( "include: ${UUID_INCLUDE_DIR}" )
      message(FATAL_ERROR "Could not find UUID library")
   endif ()
endif ()

# Deprecated declarations.
#set (NATIVE_UUID_INCLUDE_PATH ${UUID_INCLUDE_DIR} )
#get_filename_component (NATIVE_UUID_LIB_PATH ${UUID_LIBRARY} PATH)

mark_as_advanced(
  UUID_LIBRARY
  UUID_INCLUDE_DIR
  )
set(CMAKE_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK_SAVE})
