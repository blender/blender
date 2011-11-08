
set(CYCLES_INSTALL_PATH "scripts/addons/cycles")

# External Libraries

include(cmake/external_libs.cmake)

# Build Flags

set(GCC_WARNING_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-long-long")
set(GCC_OPTIM_FLAGS "-ffast-math -msse -msse2 -msse3 -mtune=native")

if(APPLE)
	set(CMAKE_CXX_FLAGS "${GCC_WARNING_FLAGS} ${GCC_OPTIM_FLAGS}")
	set(RTTI_DISABLE_FLAGS "-fno-rtti -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")
endif(APPLE)

if(WIN32)
	if(MSVC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Ox /Ot /arch:SSE2 -D_CRT_SECURE_NO_WARNINGS /EHsc /fp:fast")
		set(RTTI_DISABLE_FLAGS "/GR- -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")
	elseif(CMAKE_COMPILER_IS_GNUCC)
		set(CMAKE_CXX_FLAGS "${GCC_WARNING_FLAGS} ${GCC_OPTIM_FLAGS}")
		set(RTTI_DISABLE_FLAGS "-fno-rtti -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")
	endif()
endif(WIN32)

if(UNIX AND NOT APPLE)
	set(CMAKE_CXX_FLAGS "${GCC_WARNING_FLAGS} ${GCC_OPTIM_FLAGS}")
	set(RTTI_DISABLE_FLAGS "-fno-rtti -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")
endif(UNIX AND NOT APPLE)

# Definitions and Includes

add_definitions(${BOOST_DEFINITIONS} ${OPENIMAGEIO_DEFINITIONS})

add_definitions(-DCCL_NAMESPACE_BEGIN=namespace\ ccl\ {)
add_definitions(-DCCL_NAMESPACE_END=})

if(WITH_CYCLES_NETWORK)
  add_definitions(-DWITH_NETWORK)
endif()

if(WITH_CYCLES_OSL)
  add_definitions(-DWITH_OSL)
endif()

if(WITH_CYCLES_PARTIO)
  add_definitions(-DWITH_PARTIO)
endif()

add_definitions(-DWITH_OPENCL)
add_definitions(-DWITH_CUDA)
add_definitions(-DWITH_MULTI)

include_directories(
	${BOOST_INCLUDE_DIR}
	${OPENIMAGEIO_INCLUDE_DIRS}
	${OPENIMAGEIO_INCLUDE_DIRS}/OpenImageIO)

# Subdirectories

SET(WITH_CYCLES_BLENDER ON)

if(WITH_CYCLES_BLENDER)
	add_subdirectory(blender)
endif(WITH_CYCLES_BLENDER)

add_subdirectory(app)
add_subdirectory(bvh)
add_subdirectory(device)
add_subdirectory(doc)
add_subdirectory(kernel)
add_subdirectory(render)
add_subdirectory(subd)
add_subdirectory(util)

