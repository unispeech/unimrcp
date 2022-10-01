cmake_minimum_required (VERSION 2.8)
project (UniMRCP C)

# CMake options
option (ENABLE_CLIENT_LIB          "Enable UniMRCP client library"                  ON)
option (ENABLE_CLIENT_APP          "Enable sample UniMRCP client application"       ON)
option (ENABLE_UMC_APP             "Enable sample UniMRCP client C++ application"   ON)
option (ENABLE_ASR_CLIENT          "Enable misc ASR client library and application" ON)

option (ENABLE_SERVER_LIB          "Enable UniMRCP server library"                  ON)
option (ENABLE_SERVER_APP          "Enable UniMRCP server application"              ON)

option (ENABLE_DEMOSYNTH_PLUGIN    "Enable demo synthesizer plugin"                 ON)
option (ENABLE_DEMORECOG_PLUGIN    "Enable demo recognizer plugin"                  ON)
option (ENABLE_DEMOVERIFIER_PLUGIN "Enable demo verifier plugin"                    ON)
option (ENABLE_RECORDER_PLUGIN     "Enable recorder plugin"                         ON)

option (ENABLE_AMR_CODEC           "Enable AMR Codec"                               OFF)

option (ENABLE_TEST_SUITES         "Enable test suites"                             OFF)

# Organize targets into folders
set_property (GLOBAL PROPERTY USE_FOLDERS ON)

# Set source directories for dependencies
set (APR_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/apr CACHE PATH "Path to APR source directory")
set (APU_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/apr-util CACHE PATH "Path to APR-util source directory")
set (SOFIA_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/sofia-sip CACHE PATH "Path to SofiaSIP source directory")

# Set CMake module path
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/build/cmake)

# Find dependencies
find_package (APR REQUIRED)
find_package (APRUtil REQUIRED)
find_package (Sofia REQUIRED)
if (ENABLE_AMR_CODEC)
	find_package (OpenCoreAMR REQUIRED)
	find_package (VoAMRWBEnc REQUIRED)
endif()

# Set API definitions
set (APR_TOOLKIT_DEFINES -DAPT_STATIC_LIB)
set (MPF_DEFINES -DMPF_STATIC_LIB)
set (RTSP_DEFINES -DRTSP_STATIC_LIB)
set (MRCP_DEFINES -DMRCP_STATIC_LIB)

# Set compiler flags
if (CMAKE_C_COMPILER_ID MATCHES MSVC)
	# Microsoft Visual Studio Compiler
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4 /WX /wd4100")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /wd4100")
elseif (CMAKE_C_COMPILER_ID MATCHES GNU)
	# GNU Compiler
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wall -Werror")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Werror")
endif ()

# Shared modules (plug-ins) should not have any prefix set
set (CMAKE_SHARED_MODULE_PREFIX "")

# Pertain RPATH upon installation 
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Add library directory to RPATH
set (CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

# Set default installation path
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/installed" CACHE PATH "Default installation path" FORCE )
endif ()

# Set include directories
set (APR_TOOLKIT_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/apr-toolkit/include)
set (MPF_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mpf/include)
set (RTSP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/uni-rtsp/include)
set (MRCP_INCLUDE_DIRS
	${CMAKE_SOURCE_DIR}/libs/mrcp/include
	${CMAKE_SOURCE_DIR}/libs/mrcp/control/include
	${CMAKE_SOURCE_DIR}/libs/mrcp/message/include
	${CMAKE_SOURCE_DIR}/libs/mrcp/resources/include)
set (MRCP_SIGNALING_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mrcp-signaling/include)
set (MRCPv2_TRANSPORT_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mrcpv2-transport/include)
set (MRCP_ENGINE_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mrcp-engine/include)
set (MRCP_CLIENT_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mrcp-client/include)
set (MRCP_SERVER_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mrcp-server/include)
set (MRCP_SOFIASIP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/modules/mrcp-sofiasip/include)
set (MRCP_UNIRTSP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/modules/mrcp-unirtsp/include)
set (UNIMRCP_CLIENT_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/platforms/libunimrcp-client/include)
set (UNIMRCP_SERVER_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/platforms/libunimrcp-server/include)
set (VERSION_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/build)

# Sub-projects: libraries
add_subdirectory (libs/apr-toolkit)
add_subdirectory (libs/mpf)
add_subdirectory (libs/uni-rtsp)
add_subdirectory (libs/mrcp)
add_subdirectory (libs/mrcp-signaling)
add_subdirectory (libs/mrcpv2-transport)
if (ENABLE_CLIENT_LIB)
add_subdirectory (libs/mrcp-client)
endif ()
if (ENABLE_SERVER_LIB)
add_subdirectory (libs/mrcp-engine)
add_subdirectory (libs/mrcp-server)
endif ()

# Sub-projects: modules
add_subdirectory (modules/mrcp-sofiasip)
add_subdirectory (modules/mrcp-unirtsp)

# Sub-projects: plug-ins
if (ENABLE_DEMORECOG_PLUGIN)
add_subdirectory (plugins/demo-recog)
endif ()
if (ENABLE_DEMOSYNTH_PLUGIN)
add_subdirectory (plugins/demo-synth)
endif ()
if (ENABLE_DEMOVERIFIER_PLUGIN)
add_subdirectory (plugins/demo-verifier)
endif ()
if (ENABLE_RECORDER_PLUGIN)
add_subdirectory (plugins/mrcp-recorder)
endif ()

# Sub-projects: platform libraries
if (ENABLE_CLIENT_LIB)
add_subdirectory (platforms/libunimrcp-client)
endif ()
if (ENABLE_SERVER_LIB)
add_subdirectory (platforms/libunimrcp-server)
endif ()

# Sub-projects: platform applications
if (ENABLE_CLIENT_LIB AND ENABLE_UMC_APP)
add_subdirectory (platforms/umc)
endif ()
if (ENABLE_CLIENT_LIB AND ENABLE_CLIENT_APP)
add_subdirectory (platforms/unimrcp-client)
endif ()
if (ENABLE_SERVER_LIB AND ENABLE_SERVER_APP)
add_subdirectory (platforms/unimrcp-server)
endif ()

# Sub-projects: asr-client
if (ENABLE_CLIENT_LIB AND ENABLE_ASR_CLIENT)
add_subdirectory (platforms/libasr-client)
add_subdirectory (platforms/asr-client)
endif ()

# Sub-projects: tests
if (ENABLE_TEST_SUITES)
add_subdirectory (tests/apttest)
add_subdirectory (tests/mpftest)
add_subdirectory (tests/mrcptest)
add_subdirectory (tests/rtsptest)
add_subdirectory (tests/strtablegen)
endif ()

# Installation directives
install (DIRECTORY DESTINATION log)
install (DIRECTORY DESTINATION var)
install (DIRECTORY data/ DESTINATION data PATTERN Makefile* EXCLUDE)
install (DIRECTORY conf/ DESTINATION conf PATTERN Makefile* EXCLUDE)
