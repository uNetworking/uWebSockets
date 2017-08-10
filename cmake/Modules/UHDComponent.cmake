#
# Copyright 2010-2011,2013 Ettus Research LLC
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

########################################################################
SET(_uhd_enabled_components "" CACHE INTERNAL "" FORCE)
SET(_uhd_disabled_components "" CACHE INTERNAL "" FORCE)

########################################################################
# Register a component into the system
#  - name the component string name
#  - var the global enable variable
#  - enb the default enable setting
#  - deps a list of dependencies
#  - dis the default disable setting
########################################################################
MACRO(LIBUHD_REGISTER_COMPONENT name var enb deps dis)
    MESSAGE(STATUS "")
    MESSAGE(STATUS "Configuring ${name} support...")
    FOREACH(dep ${deps})
        MESSAGE(STATUS "  Dependency ${dep} = ${${dep}}")
    ENDFOREACH(dep)

    #setup the dependent option for this component
    INCLUDE(CMakeDependentOption)
    CMAKE_DEPENDENT_OPTION(${var} "enable ${name} support" ${enb} "${deps}" ${dis})

    #append the component into one of the lists
    IF(${var})
        MESSAGE(STATUS "  Enabling ${name} support.")
        LIST(APPEND _uhd_enabled_components ${name})
    ELSE(${var})
        MESSAGE(STATUS "  Disabling ${name} support.")
        LIST(APPEND _uhd_disabled_components ${name})
    ENDIF(${var})
    MESSAGE(STATUS "  Override with -D${var}=ON/OFF")

    #make components lists into global variables
    SET(_uhd_enabled_components ${_uhd_enabled_components} CACHE INTERNAL "" FORCE)
    SET(_uhd_disabled_components ${_uhd_disabled_components} CACHE INTERNAL "" FORCE)
ENDMACRO(LIBUHD_REGISTER_COMPONENT)

########################################################################
# Install only if appropriate for package and if component is enabled
########################################################################
FUNCTION(UHD_INSTALL)
    include(CMakeParseArgumentsCopy)
    CMAKE_PARSE_ARGUMENTS(UHD_INSTALL "" "DESTINATION;COMPONENT" "TARGETS;FILES;PROGRAMS" ${ARGN})

    IF(UHD_INSTALL_FILES)
        SET(TO_INSTALL "${UHD_INSTALL_FILES}")
    ELSEIF(UHD_INSTALL_PROGRAMS)
        SET(TO_INSTALL "${UHD_INSTALL_PROGRAMS}")
    ELSEIF(UHD_INSTALL_TARGETS)
        SET(TO_INSTALL "${UHD_INSTALL_TARGETS}")
    ENDIF(UHD_INSTALL_FILES)

    IF(UHD_INSTALL_COMPONENT STREQUAL "headers")
        IF(NOT LIBUHD_PKG AND NOT UHDHOST_PKG)
            INSTALL(${ARGN})
        ENDIF(NOT LIBUHD_PKG AND NOT UHDHOST_PKG)
    ELSEIF(UHD_INSTALL_COMPONENT STREQUAL "libraries")
        IF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
            INSTALL(${ARGN})
        ENDIF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
    ELSEIF(UHD_INSTALL_COMPONENT STREQUAL "examples")
        IF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
            INSTALL(${ARGN})
        ENDIF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
    ELSEIF(UHD_INSTALL_COMPONENT STREQUAL "tests")
        IF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
            INSTALL(${ARGN})
        ENDIF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
    ELSEIF(UHD_INSTALL_COMPONENT STREQUAL "utilities")
        IF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
            INSTALL(${ARGN})
        ENDIF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
    ELSEIF(UHD_INSTALL_COMPONENT STREQUAL "manual")
        IF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
            INSTALL(${ARGN})
        ENDIF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
    ELSEIF(UHD_INSTALL_COMPONENT STREQUAL "doxygen")
        IF(NOT LIBUHD_PKG AND NOT UHDHOST_PKG)
            INSTALL(${ARGN})
        ENDIF(NOT LIBUHD_PKG AND NOT UHDHOST_PKG)
    ELSEIF(UHD_INSTALL_COMPONENT STREQUAL "manpages")
        IF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
            INSTALL(${ARGN})
        ENDIF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG)
    ELSEIF(UHD_INSTALL_COMPONENT STREQUAL "images")
        IF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG AND NOT UHDHOST_PKG)
            INSTALL(${ARGN})
        ENDIF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG AND NOT UHDHOST_PKG)
    ELSEIF(UHD_INSTALL_COMPONENT STREQUAL "readme")
        IF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG AND NOT UHDHOST_PKG)
            INSTALL(${ARGN})
        ENDIF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG AND NOT UHDHOST_PKG)
    ELSEIF(UHD_INSTALL_COMPONENT STREQUAL "defs")
        IF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG AND NOT UHDHOST_PKG)
            INSTALL(${ARGN})
        ENDIF(NOT LIBUHD_PKG AND NOT LIBUHDDEV_PKG AND NOT UHDHOST_PKG)
    ENDIF(UHD_INSTALL_COMPONENT STREQUAL "headers")
ENDFUNCTION(UHD_INSTALL)

########################################################################
# Print the registered component summary
########################################################################
FUNCTION(UHD_PRINT_COMPONENT_SUMMARY)
    MESSAGE(STATUS "")
    MESSAGE(STATUS "######################################################")
    MESSAGE(STATUS "# Enabled components                              ")
    MESSAGE(STATUS "######################################################")
    FOREACH(comp ${_uhd_enabled_components})
        MESSAGE(STATUS "  * ${comp}")
    ENDFOREACH(comp)

    MESSAGE(STATUS "")
    MESSAGE(STATUS "######################################################")
    MESSAGE(STATUS "# Disabled components                             ")
    MESSAGE(STATUS "######################################################")
    FOREACH(comp ${_uhd_disabled_components})
        MESSAGE(STATUS "  * ${comp}")
    ENDFOREACH(comp)

    MESSAGE(STATUS "")
ENDFUNCTION(UHD_PRINT_COMPONENT_SUMMARY)
