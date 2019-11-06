# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.
#
#.rst:
# FindFTI
# -------
#
# Finds the FTI library
#
# Author: Kai Keller (kellekai@gmx.de) - 2018
#
# Sets variables:
#    FTI_FOUND
#    FTI_HOME
#    FTI_INCLUDE_DIR
#    FTI_STATIC_LIB
#    FTI_DYNAMIC_LIB

find_path( FTI_HOME include/fti.h 
    HINTS ENV FTI_HOME
    DOC "FTI Base Directory")

find_path( FTI_INCLUDE_DIR_FOUND fti.h 
    HINTS ${FTI_HOME}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH)

find_library(
    FTI_STATIC_LIB_FOUND
    NAMES libfti.a
    HINTS ${FTI_HOME}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)

find_library(
    FTI_DYNAMIC_LIB_FOUND
    NAMES libfti.so
    HINTS ${FTI_HOME}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)

include(
    FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    FTI DEFAULT_MSG
    FTI_HOME
    FTI_INCLUDE_DIR_FOUND
    FTI_STATIC_LIB_FOUND
    FTI_DYNAMIC_LIB_FOUND)

mark_as_advanced(
    FTI_HOME
    FTI_INCLUDE_DIR_FOUND
    FTI_STATIC_LIB_FOUND
    FTI_DYNAMIC_LIB_FOUND)

set(
    FTI_INCLUDE_DIR ${FTI_INCLUDE_DIR_FOUND}
    CACHE STRING "FTI Include Path" FORCE)

set(
    FTI_STATIC_LIB ${FTI_STATIC_LIB_FOUND}
    CACHE STRING "FTI static library" FORCE)

set(
    FTI_DYNAMIC_LIB ${FTI_DYNAMIC_LIB_FOUND}
    CACHE STRING "FTI static library" FORCE)

