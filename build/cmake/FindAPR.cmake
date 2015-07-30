# - Find APR (Apache Portable Runtime) library
# This module finds if APR is installed or available in its source dir
# and determines where the include files and libraries are.
# This code sets the following variables:
#
#  APR_FOUND           - have the APR libs been found
#  APR_LIBRARIES       - path to the APR library
#  APR_INCLUDE_DIRS    - path to where apr.h is found
#  APR_DEFINES         - flags to define to compile with APR
#  APR_VERSION_STRING  - version of the APR lib found
#
# The APR_STATIC variable can be used to specify whether to prefer
# static version of APR library.
# You need to set this variable before calling find_package(APR).
#
# If you'd like to specify the installation of APR to use, you should modify
# the following cache variables:
#  APR_LIBRARY             - path to the APR library
#  APR_INCLUDE_DIR         - path to where apr.h is found
# If APR not installed, it can be used from the source directory:
#  APR_SOURCE_DIR          - path to compiled APR source directory

#=============================================================================
# Copyright 2014 SpeechTech, s.r.o. http://www.speechtech.cz/en
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# $Id$
#=============================================================================


option (APR_STATIC "Try to find and link static APR library" ${APR_STATIC})
mark_as_advanced (APR_STATIC)

# Try to find library specified by ${libnames}
# in ${hints} and put its path to ${var}_LIBRARY and ${var}_LIBRARY_DEBUG,
# and set ${var}_LIBRARIES similarly to CMake's select_library_configurations macro.
# For 32bit configurations, "/x64/" is replaced with "/".
function (find_libs var libnames hints)
	if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
		string (REGEX REPLACE "[\\\\/][xX]64[\\\\/]" "/" hints "${hints}")
	endif (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	string (REPLACE "/LibR" "/LibD" hints_debug "${hints}")
	string (REPLACE "/Release" "/Debug" hints_debug "${hints_debug}")
	find_library (${var}_LIBRARY
		NAMES ${libnames}
		HINTS ${hints})
	find_library (${var}_LIBRARY_DEBUG
		NAMES ${libnames}
		HINTS ${hints_debug})
	mark_as_advanced (${var}_LIBRARY ${var}_LIBRARY_DEBUG)
	if (${var}_LIBRARY AND ${var}_LIBRARY_DEBUG AND
			NOT (${var}_LIBRARY STREQUAL ${var}_LIBRARY_DEBUG) AND
			(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE))
		set (${var}_LIBRARIES optimized ${${var}_LIBRARY} debug ${${var}_LIBRARY_DEBUG} PARENT_SCOPE)
	elseif (${var}_LIBRARY)
		set (${var}_LIBRARIES ${${var}_LIBRARY} PARENT_SCOPE)
	elseif (${var}_LIBRARY_DEBUG)
		set (${var}_LIBRARIES ${${var}_LIBRARY_DEBUG} PARENT_SCOPE)
	else ()
		set (${var}_LIBRARIES ${var}_LIBRARY-NOTFOUND PARENT_SCOPE)
	endif ()
endfunction (find_libs)

macro (find_apr_static)
	set (_apr_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
	if (WIN32)
		set (CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
	else (WIN32)
		set (CMAKE_FIND_LIBRARY_SUFFIXES .a)
	endif (WIN32)
	set (_apr_hints)
	if (APR_SOURCE_DIR)
		set (_apr_hints ${_apr_hints} "${APR_SOURCE_DIR}/lib"
			"${APR_SOURCE_DIR}/x64/LibR" "${APR_SOURCE_DIR}/.libs")
	endif (APR_SOURCE_DIR)
	set (_apr_hints ${_apr_hints} /usr/local/lib)
	find_libs (APR "apr-1" "${_apr_hints}")
	set (CMAKE_FIND_LIBRARY_SUFFIXES ${_apr_CMAKE_FIND_LIBRARY_SUFFIXES})
endmacro (find_apr_static)

macro (find_apr_dynamic)
	set (_apr_hints)
	if (APR_SOURCE_DIR)
		set (_apr_hints ${_apr_hints} "${APR_SOURCE_DIR}/lib"
			"${APR_SOURCE_DIR}/x64/Release" "${APR_SOURCE_DIR}/.libs")
	endif (APR_SOURCE_DIR)
	set (_apr_hints ${_apr_hints} /usr/local/lib)
	find_libs (APR "libapr-1;apr-1" "${_apr_hints}")
endmacro (find_apr_dynamic)

include (FindPackageMessage)
if (APR_STATIC)
	find_apr_static ()
	if (NOT APR_LIBRARIES)
		find_package_message (APR "Static APR library not found, trying dynamic"
			"[${APR_LIBRARY}][${APR_INCLUDE_DIR}][${APR_STATIC}]")
		find_apr_dynamic ()
	endif (NOT APR_LIBRARIES)
	set (APR_DEFINES -DAPR_DECLARE_STATIC)
else (APR_STATIC)
	find_apr_dynamic ()
	if (NOT APR_LIBRARIES)
		find_package_message (APR "Dynamic APR library not found, trying static"
			"[${APR_LIBRARY}][${APR_INCLUDE_DIR}][${APR_STATIC}]")
		find_apr_static ()
	endif (NOT APR_LIBRARIES)
	set (APR_DEFINES)
endif (APR_STATIC)

set (_apr_hints)
if (APR_SOURCE_DIR)
	set (_apr_hints ${_apr_hints} "${APR_SOURCE_DIR}/include" "${APR_SOURCE_DIR}/include/apr-1")
endif (APR_SOURCE_DIR)
set (_apr_hints ${_apr_hints} /usr/local/include/apr-1)
find_path (APR_INCLUDE_DIR apr_version.h
	HINTS ${_apr_hints})
mark_as_advanced (APR_INCLUDE_DIR)
set (APR_INCLUDE_DIRS ${APR_INCLUDE_DIR})

if (APR_INCLUDE_DIR)
	file (STRINGS "${APR_INCLUDE_DIR}/apr_version.h" _apr_ver
		REGEX "^#define[ \t]+APR_[ACHIJMNOPRT]+_VERSION[ \t]+[0-9]+")
	string (REGEX REPLACE ".*[ \t]APR_MAJOR_VERSION[ \t]+([0-9]+).*" "\\1" _apr_major "${_apr_ver}")
	string (REGEX REPLACE ".*[ \t]APR_MINOR_VERSION[ \t]+([0-9]+).*" "\\1" _apr_minor "${_apr_ver}")
	string (REGEX REPLACE ".*[ \t]APR_PATCH_VERSION[ \t]+([0-9]+).*" "\\1" _apr_patch "${_apr_ver}")
	set (APR_VERSION_STRING "${_apr_major}.${_apr_minor}.${_apr_patch}")
endif (APR_INCLUDE_DIR)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (APR
	REQUIRED_VARS APR_LIBRARIES APR_INCLUDE_DIRS
	VERSION_VAR APR_VERSION_STRING)
