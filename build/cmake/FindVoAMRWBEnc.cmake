# - Find VoAMRWBEnc library
# This module finds if VoAMRWBEnc library is installed
# and determines where the include files and libraries are.
#
# The module sets the following variables:
#
#  VOAMRWBENC_FOUND           - have the VoAMRWBEnc lib been found
#  VOAMRWBENC_LIBRARIES       - path to the VoAMRWBEnc library
#  VOAMRWBENC_INCLUDE_DIRS    - path to the VoAMRWBEnc include dir
#
# The module makes use of the following variables:
#
# VOAMRWBENC_ROOT_DIR         - root directory to the VoAMRWBEnc installation

# Look for the header in VOAMRWBENC_ROOT_DIR first
find_path (
    VOAMRWBENC_INCLUDE_DIRS
    NAMES vo-amrwbenc/enc_if.h
    PATHS ${VOAMRWBENC_ROOT_DIR}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
)
	
# Fall back to standard search paths next
find_path (
    VOAMRWBENC_INCLUDE_DIRS
    NAMES vo-amrwbenc/enc_if.h
)

# Look for the library in VOAMRWBENC_ROOT_DIR first
find_library (
    VOAMRWBENC_LIBRARIES
    NAMES vo-amrwbenc
    PATHS ${VOAMRWBENC_ROOT_DIR}
    PATH_SUFFIXES lib64 lib
    NO_DEFAULT_PATH
)

# Fall back to standard search paths next
find_library (
    VOAMRWBENC_LIBRARIES
    NAMES vo-amrwbenc
)

include (FindPackageHandleStandardArgs)

find_package_handle_standard_args (
    VoAMRWBEnc
    REQUIRED_VARS VOAMRWBENC_LIBRARIES VOAMRWBENC_INCLUDE_DIRS
    VERSION_VAR "1.0"
)

mark_as_advanced (VOAMRWBENC_INCLUDE_DIRS VOAMRWBENC_LIBRARIES)
