
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(cycles)
SET(CYCLES_VERSION_MAJOR 0)
SET(CYCLES_VERSION_MINOR 0)
SET(CYCLES_VERSION ${CYCLES_VERSION_MAJOR}.${CYCLES_VERSION_MINOR})

# Options

OPTION(WITH_OSL "Build with Open Shading Language support" OFF)
OPTION(WITH_CUDA "Build with CUDA support" OFF)
OPTION(WITH_OPENCL "Build with OpenCL support (not working)" OFF)
OPTION(WITH_BLENDER "Build Blender Python extension" OFF)
OPTION(WITH_PARTIO "Build with Partio point cloud support (unfinished)" OFF)
OPTION(WITH_NETWORK "Build with network rendering support (unfinished)" OFF)
OPTION(WITH_MULTI "Build with network rendering support (unfinished)" OFF)
OPTION(WITH_DOCS "Build html documentation" OFF)

# Flags
SET(CUDA_ARCH sm_10 sm_11 sm_12 sm_13 sm_20 sm_21 CACHE STRING "CUDA architectures to build for")
SET(CUDA_MAXREG 24 CACHE STRING "CUDA maximum number of register to use")

# Paths

SET(OSL_PATH "" CACHE PATH "Path to OpenShadingLanguage installation")
SET(OIIO_PATH "" CACHE PATH "Path to OpenImageIO installation")
SET(BOOST_PATH "/usr" CACHE PATH "Path to Boost installation")
SET(CUDA_PATH "/usr/local/cuda" CACHE PATH "Path to CUDA installation")
SET(OPENCL_PATH "" CACHE PATH "Path to OpenCL installation")
SET(PYTHON_PATH "" CACHE PATH "Path to Python installation")
SET(BLENDER_PATH "" CACHE PATH "Path to Blender installation")
SET(PARTIO_PATH "" CACHE PATH "Path to Partio installation")
SET(GLEW_PATH "" CACHE PATH "Path to GLEW installation")
SET(GLUT_PATH "" CACHE PATH "Path to GLUT installation")
SET(INSTALL_PATH "${CMAKE_BINARY_DIR}/install" CACHE PATH "Path to install to")

# External Libraries

INCLUDE(cmake/external_libs.cmake)

# Platforms

INCLUDE(cmake/platforms.cmake)

# Subdirectories

IF(WITH_BLENDER)
	ADD_SUBDIRECTORY(blender)
ENDIF(WITH_BLENDER)

ADD_SUBDIRECTORY(app)
ADD_SUBDIRECTORY(bvh)
ADD_SUBDIRECTORY(device)
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(kernel)
ADD_SUBDIRECTORY(render)
ADD_SUBDIRECTORY(subd)
ADD_SUBDIRECTORY(util)

