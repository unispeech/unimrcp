# - Find OpenCore AMR library
# This module finds if OpenCore AMR library is installed
# and determines where the include files and libraries are.
#
# The module sets the following variables:
#
#  OPENCOREAMR_FOUND        - have the OpenCore AMR lib been found
#  OPENCOREAMR_LIBRARIES    - path to the OpenCore AMR library
#  OPENCOREAMR_INCLUDE_DIRS - path to the OpenCore AMR include dir
#
# The module makes use of the following variables:
#
# OPENCOREAMR_ROOT_DIR      - root directory to the OpenCore AMR installation

# Look for the header in OPENCOREAMR_ROOT_DIR first
find_path (
    OPENCOREAMR_INCLUDE_DIRS
    NAMES opencore-amrwb/dec_if.h
    PATHS ${OPENCOREAMR_ROOT_DIR}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
)
	
# Fall back to standard search paths next
find_path (
    OPENCOREAMR_INCLUDE_DIRS
    NAMES opencore-amrwb/dec_if.h
)

# Look for the library in OPENCOREAMR_ROOT_DIR first
find_library (
    OPENCOREAMR_LIBRARIES
    NAMES opencore-amrwb
    PATHS ${OPENCOREAMR_ROOT_DIR}
    PATH_SUFFIXES lib64 lib32 lib
    NO_DEFAULT_PATH
)

# Fall back to standard search paths next
find_library (
    OPENCOREAMR_LIBRARIES
    NAMES opencore-amrwb
)

include (FindPackageHandleStandardArgs)

find_package_handle_standard_args (
    OpenCoreAMR
    REQUIRED_VARS OPENCOREAMR_LIBRARIES OPENCOREAMR_INCLUDE_DIRS
    VERSION_VAR "1.0"
)

mark_as_advanced (OPENCOREAMR_INCLUDE_DIRS OPENCOREAMR_LIBRARIES)
