
SET(CYCLES_VERSION_MAJOR 0)
SET(CYCLES_VERSION_MINOR 0)
SET(CYCLES_VERSION ${CYCLES_VERSION_MAJOR}.${CYCLES_VERSION_MINOR})

# Options

OPTION(WITH_CYCLES_OSL "Build with Open Shading Language support" OFF)
OPTION(WITH_CYCLES_CUDA "Build with CUDA support" OFF)
OPTION(WITH_CYCLES_OPENCL "Build with OpenCL support (not working)" OFF)
OPTION(WITH_CYCLES_BLENDER "Build Blender Python extension" ON)
OPTION(WITH_CYCLES_PARTIO "Build with Partio point cloud support (unfinished)" OFF)
OPTION(WITH_CYCLES_NETWORK "Build with network rendering support (unfinished)" OFF)
OPTION(WITH_CYCLES_MULTI "Build with network rendering support (unfinished)" OFF)
OPTION(WITH_CYCLES_TEST "Build cycles test application" OFF)

# Flags

SET(CYCLES_CUDA_ARCH sm_10 sm_11 sm_12 sm_13 sm_20 sm_21 CACHE STRING "CUDA architectures to build for")
SET(CYCLES_CUDA_MAXREG 24 CACHE STRING "CUDA maximum number of register to use")

# Paths

IF(UNIX AND NOT APPLE)
	SET(CYCLES_OIIO "" CACHE PATH "Path to OpenImageIO installation")
	SET(CYCLES_BOOST "/usr" CACHE PATH "Path to Boost installation")
ENDIF()

SET(CYCLES_OSL "" CACHE PATH "Path to OpenShadingLanguage installation")
SET(CYCLES_CUDA "/usr/local/cuda" CACHE PATH "Path to CUDA installation")
SET(CYCLES_OPENCL "" CACHE PATH "Path to OpenCL installation")
SET(CYCLES_PARTIO "" CACHE PATH "Path to Partio installation")
SET(CYCLES_GLEW "" CACHE PATH "Path to GLEW installation")

# Install, todo: deduplicate install path code

if(MSVC_IDE)
	set(TARGETDIR ${EXECUTABLE_OUTPUT_PATH}/\${BUILD_TYPE})
elseif(APPLE)
	set(TARGETDIR ${EXECUTABLE_OUTPUT_PATH}/\${BUILD_TYPE})
else()
	set(TARGETDIR ${EXECUTABLE_OUTPUT_PATH})
endif()

if(UNIX AND NOT APPLE)
	if(WITH_INSTALL_PORTABLE)
		set(TARGETDIR_VER ${TARGETDIR}/${BLENDER_VERSION})
	else()
		set(TARGETDIR_VER ${CMAKE_INSTALL_PREFIX}/share/blender/${BLENDER_VERSION})
	endif()
elseif(WIN32)
	set(TARGETDIR_VER ${TARGETDIR}/${BLENDER_VERSION})
elseif(APPLE)
	set(TARGETDIR_VER ${TARGETDIR}/blender.app/Contents/MacOS/${BLENDER_VERSION})
endif()

SET(CYCLES_INSTALL_PATH "${TARGETDIR_VER}/scripts/addons")

# External Libraries

INCLUDE(cmake/external_libs.cmake)

# Platforms

INCLUDE(cmake/platforms.cmake)

# Subdirectories

IF(WITH_CYCLES_BLENDER)
	ADD_SUBDIRECTORY(blender)
ENDIF(WITH_CYCLES_BLENDER)

ADD_SUBDIRECTORY(app)
ADD_SUBDIRECTORY(bvh)
ADD_SUBDIRECTORY(device)
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(kernel)
ADD_SUBDIRECTORY(render)
ADD_SUBDIRECTORY(subd)
ADD_SUBDIRECTORY(util)

