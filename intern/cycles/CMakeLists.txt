
set(CYCLES_INSTALL_PATH "scripts/addons/cycles")
set(WITH_CYCLES_BLENDER ON)

# External Libraries

include(cmake/external_libs.cmake)

# Build Flags

if(WITH_RAYOPTIMIZATION AND SUPPORT_SSE_BUILD)
	set(WITH_CYCLES_OPTIMIZED_KERNEL ON)
endif()

if(WIN32 AND MSVC)
	set(CYCLES_OPTIMIZED_KERNEL_FLAGS "/arch:SSE2 -D_CRT_SECURE_NO_WARNINGS /fp:fast /EHsc")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast -D_CRT_SECURE_NO_WARNINGS /EHsc")
elseif(CMAKE_COMPILER_IS_GNUCC)
	set(CYCLES_OPTIMIZED_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mfpmath=sse")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
endif()

# for OSL, not needed yet
# set(RTTI_DISABLE_FLAGS "-fno-rtti -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")
# set(RTTI_DISABLE_FLAGS "/GR- -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")

# Definitions and Includes

add_definitions(${BOOST_DEFINITIONS} ${OPENIMAGEIO_DEFINITIONS})

add_definitions(-DCCL_NAMESPACE_BEGIN=namespace\ ccl\ {)
add_definitions(-DCCL_NAMESPACE_END=})

if(WITH_CYCLES_OPTIMIZED_KERNEL)
	add_definitions(-DWITH_OPTIMIZED_KERNEL)
endif()

if(WITH_CYCLES_NETWORK)
	add_definitions(-DWITH_NETWORK)
endif()

if(WITH_CYCLES_OSL)
	add_definitions(-DWITH_OSL)
endif()

if(WITH_CYCLES_PARTIO)
	add_definitions(-DWITH_PARTIO)
endif()

if(WITH_CYCLES_CUDA_BINARIES)
	add_definitions(-DWITH_CUDA_BINARIES)
endif()

add_definitions(-DWITH_OPENCL)
add_definitions(-DWITH_CUDA)
add_definitions(-DWITH_MULTI)

include_directories(
	${BOOST_INCLUDE_DIR}
	${OPENIMAGEIO_INCLUDE_DIRS}
	${OPENIMAGEIO_INCLUDE_DIRS}/OpenImageIO)

# Subdirectories

if(WITH_CYCLES_BLENDER)
	add_definitions(-DBLENDER_PLUGIN)
	add_subdirectory(blender)
endif()

add_subdirectory(app)
add_subdirectory(bvh)
add_subdirectory(device)
add_subdirectory(doc)
add_subdirectory(kernel)
add_subdirectory(render)
add_subdirectory(subd)
add_subdirectory(util)

