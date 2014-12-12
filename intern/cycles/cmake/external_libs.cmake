###########################################################################
# Precompiled libraries tips and hints, for find_package().

if(CYCLES_STANDALONE_REPOSITORY)
	if(APPLE OR WIN32)
		include(precompiled_libs)
	endif()
endif()

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

# Workaround for unconventional variable name use in Blender.
if(NOT CYCLES_STANDALONE_REPOSITORY)
	set(GLEW_INCLUDE_DIR "${GLEW_INCLUDE_PATH}")
endif()

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

# Packages which are being found by Blender when building from inside Blender
# source code. but which we need to take care of when building Cycles from a
# standalone repository
if(CYCLES_STANDALONE_REPOSITORY)
	# PThreads
	# TODO(sergey): Bloody exception, handled in precompiled_libs.cmake.
	if(NOT WIN32)
		set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
		find_package(Threads REQUIRED)
		set(PTHREADS_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
	endif()

	####
	# OpenGL

	# TODO(sergey): We currently re-use the same variable name as we use
	# in Blender. Ideally we need to make it CYCLES_GL_LIBRARIES.
	find_package(OpenGL REQUIRED)
	find_package(GLEW REQUIRED)
	list(APPEND BLENDER_GL_LIBRARIES
		"${OPENGL_gl_LIBRARY}"
		"${OPENGL_glu_LIBRARY}"
		"${GLEW_LIBRARY}"
	)

	####
	# OpenImageIO
	find_package(OpenImageIO REQUIRED)
	if(OPENIMAGEIO_PUGIXML_FOUND)
		set(PUGIXML_INCLUDE_DIR "${OPENIMAGEIO_INCLUDE_DIR/OpenImageIO}")
		set(PUGIXML_LIBRARIES "")
	else()
		find_package(PugiXML REQUIRED)
	endif()

	# OIIO usually depends on OpenEXR, so find this library
	# but don't make it required.
	find_package(OpenEXR)

	####
	# Boost
	set(__boost_packages filesystem regex system thread date_time)
	if(WITH_CYCLES_NETWORK)
		list(APPEND __boost_packages serialization)
	endif()
	if(WITH_CYCLES_OSL)
		# TODO(sergey): This is because of the way how our precompiled
		# libraries works, could be different for someone's else libs..
		if(APPLE OR MSVC)
			list(APPEND __boost_packages wave)
		endif()
	endif()
	find_package(Boost 1.48 COMPONENTS ${__boost_packages} REQUIRED)
	if(NOT Boost_FOUND)
		# Try to find non-multithreaded if -mt not found, this flag
		# doesn't matter for us, it has nothing to do with thread
		# safety, but keep it to not disturb build setups.
		set(Boost_USE_MULTITHREADED OFF)
		find_package(Boost 1.48 COMPONENTS ${__boost_packages})
	endif()
	unset(__boost_packages)
	set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
	set(BOOST_LIBRARIES ${Boost_LIBRARIES})
	set(BOOST_LIBPATH ${Boost_LIBRARY_DIRS})
	set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB")

	####
	# OpenShadingLanguage
	if(WITH_CYCLES_OSL)
		find_package(OpenShadingLanguage REQUIRED)
		find_package(LLVM REQUIRED)
	endif()

	####
	# Logging
	if(WITH_CYCLES_LOGGING)
		find_package(Glog REQUIRED)
		find_package(Gflags REQUIRED)
	endif()

	unset(_lib_DIR)
else()
	if(WIN32)
		set(GLOG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/libmv/third_party/glog/src/windows)
		set(GFLAGS_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/libmv/third_party/gflags)
	else()
		set(GLOG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/libmv/third_party/glog/src)
		set(GFLAGS_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/libmv/third_party/gflags)
	endif()
	set(GFLAGS_NAMESPACE gflags)
endif()
