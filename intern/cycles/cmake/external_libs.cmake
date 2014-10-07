###########################################################################
# GLUT

if(WITH_CYCLES_STANDALONE AND WITH_CYCLES_STANDALONE_GUI)
	set(GLUT_ROOT_PATH ${CYCLES_GLUT})

	find_package(GLUT)
	message(STATUS "GLUT_FOUND=${GLUT_FOUND}")

	include_directories(
		SYSTEM
		${GLUT_INCLUDE_DIR}
	)
endif()

###########################################################################
# GLEW

if(WITH_CYCLES_STANDALONE AND WITH_CYCLES_STANDALONE_GUI)
	set(CYCLES_APP_GLEW_LIBRARY ${BLENDER_GLEW_LIBRARIES})
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
