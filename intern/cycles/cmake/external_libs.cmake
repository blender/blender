
###########################################################################
# GLUT

if(WITH_CYCLES_TEST)
	set(GLUT_ROOT_PATH ${CYCLES_GLUT})

	find_package(GLUT)
	message(STATUS "GLUT_FOUND=${GLUT_FOUND}")

	include_directories(${GLUT_INCLUDE_DIR})
endif()

if(WITH_BUILTIN_GLEW)
	set(CYCLES_GLEW_LIBRARY extern_glew)
else()
	set(CYCLES_GLEW_LIBRARY ${GLEW_LIBRARY})
endif()

###########################################################################
# OpenShadingLanguage

if(WITH_CYCLES_OSL)

	set(CYCLES_OSL "" CACHE PATH "Path to OpenShadingLanguage installation")

	message(STATUS "CYCLES_OSL = ${CYCLES_OSL}")

	find_library(OSL_LIBRARIES NAMES oslexec oslcomp oslquery PATHS ${CYCLES_OSL}/lib ${CYCLES_OSL}/dist)
	find_path(OSL_INCLUDES OSL/oslclosure.h PATHS ${CYCLES_OSL}/include ${CYCLES_OSL}/dist)
	find_program(OSL_COMPILER NAMES oslc PATHS ${CYCLES_OSL}/bin ${CYCLES_OSL}/dist)

	if(OSL_INCLUDES AND OSL_LIBRARIES AND OSL_COMPILER)
		set(OSL_FOUND TRUE)
		message(STATUS "OSL includes = ${OSL_INCLUDES}")
		message(STATUS "OSL library = ${OSL_LIBRARIES}")
		message(STATUS "OSL compiler = ${OSL_COMPILER}")
	else()
		message(STATUS "OSL not found")
	endif()

	include_directories(${OSL_INCLUDES} ${OSL_INCLUDES}/OSL ${OSL_INCLUDES}/../../../src/liboslexec)

endif()

###########################################################################
# Partio

if(WITH_CYCLES_PARTIO)

	set(CYCLES_PARTIO "" CACHE PATH "Path to Partio installation")

	message(STATUS "CYCLES_PARTIO = ${CYCLES_PARTIO}")

	find_library(PARTIO_LIBRARIES NAMES partio PATHS ${CYCLES_PARTIO}/lib)
	find_path(PARTIO_INCLUDES Partio.h ${CYCLES_PARTIO}/include)

	find_package(ZLIB)

	if(PARTIO_INCLUDES AND PARTIO_LIBRARIES AND ZLIB_LIBRARIES)
		list(APPEND PARTIO_LIBRARIES ${ZLIB_LIBRARIES})
		set(PARTIO_FOUND TRUE)
		message(STATUS "PARTIO includes = ${PARTIO_INCLUDES}")
		message(STATUS "PARTIO library = ${PARTIO_LIBRARIES}")
	else()
		message(STATUS "PARTIO not found")
	endif()

	include_directories(${PARTIO_INCLUDES})

endif()

###########################################################################
# Blender

if(WITH_CYCLES_BLENDER)

	set(BLENDER_INCLUDE_DIRS
		${CMAKE_SOURCE_DIR}/intern/guardedalloc
		${CMAKE_SOURCE_DIR}/source/blender/makesdna
		${CMAKE_SOURCE_DIR}/source/blender/makesrna
		${CMAKE_SOURCE_DIR}/source/blender/blenloader
		${CMAKE_BINARY_DIR}/source/blender/makesrna/intern)

	ADD_DEFINITIONS(-DBLENDER_PLUGIN)
endif()

###########################################################################
# CUDA

if(WITH_CYCLES_CUDA)

	set(CYCLES_CUDA "/usr/local/cuda" CACHE PATH "Path to CUDA installation")
	set(CYCLES_CUDA_ARCH sm_10 sm_11 sm_12 sm_13 sm_20 sm_21 CACHE STRING "CUDA architectures to build for")
	set(CYCLES_CUDA_MAXREG 24 CACHE STRING "CUDA maximum number of register to use")

	find_path(CUDA_INCLUDES cuda.h ${CYCLES_CUDA}/include NO_DEFAULT_PATH)
	find_program(CUDA_NVCC NAMES nvcc PATHS ${CYCLES_CUDA}/bin NO_DEFAULT_PATH)

	if(CUDA_INCLUDES AND CUDA_NVCC)
		message(STATUS "CUDA includes = ${CUDA_INCLUDES}")
		message(STATUS "CUDA nvcc = ${CUDA_NVCC}")
	else()
		message(STATUS "CUDA not found")
	endif()

	include_directories(${CUDA_INCLUDES})

endif()

###########################################################################
# OpenCL

if(WITH_CYCLES_OPENCL)

	if(APPLE)
		set(OPENCL_INCLUDE_DIR "/System/Library/Frameworks/OpenCL.framework/Headers")
		set(OPENCL_LIBRARIES "-framework OpenCL")
	endif()

	if(WIN32)
		set(OPENCL_INCLUDE_DIR "")
		set(OPENCL_LIBRARIES "OpenCL")
	endif()

	if(UNIX AND NOT APPLE)
		set(OPENCL_INCLUDE_DIR ${CYCLES_OPENCL})
		set(OPENCL_LIBRARIES "OpenCL")
	endif()
endif()

