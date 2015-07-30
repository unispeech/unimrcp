# - Find APR-util (Apache Portable Runtime utilities) library
# This module finds if APR-util is installed or available in its source dir
# and determines where the include files and libraries are.
# This code sets the following variables:
#
#  APU_FOUND           - have the APR-util libs been found
#  APU_LIBRARIES       - path to the APR-util library
#  APU_INCLUDE_DIRS    - path to where apu.h is found
#  APU_DEFINES         - flags to define to compile with APR-util
#  APU_VERSION_STRING  - version of the APR-util lib found
#
# The APU_STATIC variable can be used to specify whether to prefer
# static version of APR-util library.
# You need to set this variable before calling find_package(APRUtil).
#
# If you'd like to specify the installation of APR-util to use, you should modify
# the following cache variables:
#  APU_LIBRARY             - path to the APR-util library
#  APU_INCLUDE_DIR         - path to where apu.h is found
#  APU_XML_LIBRARY         - path to eXpat library bundled with APR-util
#                            (only needed when linking statically)
# If APR-util not installed, it can be used from the source directory:
#  APU_SOURCE_DIR          - path to compiled APR-util source directory

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


option (APU_STATIC "Try to find and link static APR-util library" ${APU_STATIC})
mark_as_advanced (APU_STATIC)

include (SelectLibraryConfigurations)

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

macro (find_apu_static)
	set (_apu_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
	if (WIN32)
		set (CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
	else (WIN32)
		set (CMAKE_FIND_LIBRARY_SUFFIXES .a)
	endif (WIN32)
	set (_apu_hints)
	if (APU_SOURCE_DIR)
		set (_apu_hints ${_apu_hints} "${APU_SOURCE_DIR}/lib"
			"${APU_SOURCE_DIR}/xml/expat/lib" "${APU_SOURCE_DIR}/xml/expat/lib"
			"${APU_SOURCE_DIR}/x64/LibR" "${APU_SOURCE_DIR}/xml/expat/lib/x64/LibR"
			 "${APU_SOURCE_DIR}/.libs" "${APU_SOURCE_DIR}/xml/expat/.libs")
	endif (APU_SOURCE_DIR)
	set (_apu_hints ${_apu_hints} /usr/local/lib)
	find_libs (APU "aprutil-1" "${_apu_hints}")
	find_libs (APU_XML "expat;xml" "${_apu_hints}")
	set (CMAKE_FIND_LIBRARY_SUFFIXES ${_apu_CMAKE_FIND_LIBRARY_SUFFIXES})
endmacro (find_apu_static)

macro (find_apu_dynamic)
	set (_apu_hints)
	if (APU_SOURCE_DIR)
		set (_apu_hints ${_apu_hints} "${APU_SOURCE_DIR}/lib" "${APU_SOURCE_DIR}/x64/Release"
			"${APU_SOURCE_DIR}/.libs")
	endif (APU_SOURCE_DIR)
	set (_apu_hints ${_apu_hints} /usr/local/lib)
	find_libs (APU "libaprutil-1;aprutil-1" "${_apu_hints}")
endmacro (find_apu_dynamic)

include (FindPackageMessage)
if (APU_STATIC)
	find_apu_static ()
	if (NOT APU_LIBRARIES)
		find_package_message (APRUtil "Static APR-util library not found, trying dynamic"
			"[${APU_LIBRARY}][${APU_INCLUDE_DIR}][${APU_STATIC}]")
		find_apu_dynamic ()
	endif (NOT APU_LIBRARIES)
	set (APU_DEFINES -DAPU_DECLARE_STATIC)
else (APU_STATIC)
	find_apu_dynamic ()
	if (NOT APU_LIBRARIES)
		find_package_message (APRUtil "Dynamic APR-util library not found, trying static"
			"[${APU_LIBRARY}][${APU_INCLUDE_DIR}][${APU_STATIC}]")
		find_apu_static ()
	endif (NOT APU_LIBRARIES)
	set (APU_DEFINES)
endif (APU_STATIC)

if (APU_STATIC AND APU_LIBRARIES)
	if (APU_XML_LIBRARIES)
		set (APU_LIBRARIES ${APU_LIBRARIES} ${APU_XML_LIBRARIES})
	else (APU_XML_LIBRARIES)
		message ("Statically linked APR-util requires eXpat, please set APU_XML_LIBRARY")
	endif (APU_XML_LIBRARIES)
endif (APU_STATIC AND APU_LIBRARIES)

set (_apu_hints)
if (APU_SOURCE_DIR)
	set (_apu_hints ${_apu_hints} "${APU_SOURCE_DIR}/include" "${APU_SOURCE_DIR}/include/apr-1")
endif (APU_SOURCE_DIR)
set (_apu_hints ${_apu_hints} /usr/local/include/apr-1)
find_path (APU_INCLUDE_DIR apu_version.h
	HINTS ${_apu_hints})
mark_as_advanced (APU_INCLUDE_DIR)
set (APU_INCLUDE_DIRS ${APU_INCLUDE_DIR})

if (APU_INCLUDE_DIR)
	list (REMOVE_DUPLICATES APU_INCLUDE_DIRS)
	file (STRINGS "${APU_INCLUDE_DIR}/apu_version.h" _apu_ver
		REGEX "^#define[ \t]+APU_[ACHIJMNOPRT]+_VERSION[ \t]+[0-9]+")
	string (REGEX REPLACE ".*[ \t]APU_MAJOR_VERSION[ \t]+([0-9]+).*" "\\1" _apu_major "${_apu_ver}")
	string (REGEX REPLACE ".*[ \t]APU_MINOR_VERSION[ \t]+([0-9]+).*" "\\1" _apu_minor "${_apu_ver}")
	string (REGEX REPLACE ".*[ \t]APU_PATCH_VERSION[ \t]+([0-9]+).*" "\\1" _apu_patch "${_apu_ver}")
	set (APU_VERSION_STRING "${_apu_major}.${_apu_minor}.${_apu_patch}")
endif (APU_INCLUDE_DIR)

unset (APU_FOUND)
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (APRUtil
	REQUIRED_VARS APU_LIBRARIES APU_INCLUDE_DIRS
	VERSION_VAR APU_VERSION_STRING)
