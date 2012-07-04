
###########################################################################
# GLUT

if(WITH_CYCLES_TEST)
	set(GLUT_ROOT_PATH ${CYCLES_GLUT})

	find_package(GLUT)
	message(STATUS "GLUT_FOUND=${GLUT_FOUND}")

	include_directories(${GLUT_INCLUDE_DIR})
endif()

if(WITH_SYSTEM_GLEW)
	set(CYCLES_GLEW_LIBRARY ${GLEW_LIBRARY})
else()
	set(CYCLES_GLEW_LIBRARY extern_glew)
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
# CUDA

if(WITH_CYCLES_CUDA_BINARIES)
	find_package(CUDA) # Try to auto locate CUDA toolkit
	if(CUDA_FOUND)
		message(STATUS "CUDA nvcc = ${CUDA_NVCC_EXECUTABLE}")
	else()
		message(STATUS "CUDA compiler not found, disabling WITH_CYCLES_CUDA_BINARIES")
		set(WITH_CYCLES_CUDA_BINARIES OFF)
	endif()
endif()

