
# Standalone or with Blender
if(NOT WITH_BLENDER AND WITH_CYCLES_STANDALONE)
	set(CYCLES_INSTALL_PATH "")
else()
	set(WITH_CYCLES_BLENDER ON)
	set(CYCLES_INSTALL_PATH "scripts/addons/cycles")
endif()

# External Libraries

include(cmake/external_libs.cmake)

# Build Flags
# todo: refactor this code to match scons

if(WIN32 AND MSVC)
	# there is no /arch:SSE3, but intrinsics are available anyway
	if(CMAKE_CL_64)
		set(CYCLES_SSE2_KERNEL_FLAGS "/fp:fast -D_CRT_SECURE_NO_WARNINGS /Gs-")
		set(CYCLES_SSE3_KERNEL_FLAGS "/fp:fast -D_CRT_SECURE_NO_WARNINGS /Gs-")
		set(CYCLES_SSE41_KERNEL_FLAGS "/fp:fast -D_CRT_SECURE_NO_WARNINGS /Gs-")
		set(CYCLES_AVX_KERNEL_FLAGS "/fp:fast -D_CRT_SECURE_NO_WARNINGS /Gs-") #/arch:AVX for VC2012 and above
	else()
		set(CYCLES_SSE2_KERNEL_FLAGS "/arch:SSE2 /fp:fast -D_CRT_SECURE_NO_WARNINGS /Gs-")
		set(CYCLES_SSE3_KERNEL_FLAGS "/arch:SSE2 /fp:fast -D_CRT_SECURE_NO_WARNINGS /Gs-")
		set(CYCLES_SSE41_KERNEL_FLAGS "/arch:SSE2 /fp:fast -D_CRT_SECURE_NO_WARNINGS /Gs-")
		set(CYCLES_AVX_KERNEL_FLAGS "/arch:SSE2 /fp:fast -D_CRT_SECURE_NO_WARNINGS /Gs-") #/arch:AVX for VC2012 and above
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast -D_CRT_SECURE_NO_WARNINGS /Gs-")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ox")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Ox")
	set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /Ox")
elseif(CMAKE_COMPILER_IS_GNUCC)
	set(CYCLES_SSE2_KERNEL_FLAGS "-ffast-math -msse -msse2 -mfpmath=sse")
	set(CYCLES_SSE3_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -mfpmath=sse")
	set(CYCLES_SSE41_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mfpmath=sse")
	set(CYCLES_AVX_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mavx -mfpmath=sse")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CYCLES_SSE2_KERNEL_FLAGS "-ffast-math -msse -msse2")
	set(CYCLES_SSE3_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3")
	set(CYCLES_SSE41_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1")
	set(CYCLES_AVX_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mavx")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
endif()

add_definitions(-DWITH_KERNEL_SSE2 -DWITH_KERNEL_SSE3 -DWITH_KERNEL_SSE41 -DWITH_KERNEL_AVX)

# for OSL
if(WIN32 AND MSVC)
	set(RTTI_DISABLE_FLAGS "/GR- -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")
elseif(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
	set(RTTI_DISABLE_FLAGS "-fno-rtti -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")
endif()

# Definitions and Includes

add_definitions(${BOOST_DEFINITIONS} ${OPENIMAGEIO_DEFINITIONS})

add_definitions(-DCCL_NAMESPACE_BEGIN=namespace\ ccl\ {)
add_definitions(-DCCL_NAMESPACE_END=})

if(WITH_CYCLES_NETWORK)
	add_definitions(-DWITH_NETWORK)
endif()

if(WITH_CYCLES_STANDALONE_GUI)
	add_definitions(-DWITH_CYCLES_STANDALONE_GUI)
endif()

if(WITH_CYCLES_PTEX)
	add_definitions(-DWITH_PTEX)
endif()

if(WITH_CYCLES_OPENSUBDIV)
	add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_CYCLES_OSL)
	add_definitions(-DWITH_OSL)
	add_definitions(-DOSL_STATIC_LIBRARY)
	include_directories(${OSL_INCLUDES})
endif()

add_definitions(-DWITH_OPENCL)
add_definitions(-DWITH_CUDA)
add_definitions(-DWITH_MULTI)

include_directories(
	SYSTEM
	${BOOST_INCLUDE_DIR}
	${OPENIMAGEIO_INCLUDE_DIRS}
	${OPENIMAGEIO_INCLUDE_DIRS}/OpenImageIO
	${OPENEXR_INCLUDE_DIR}
	${OPENEXR_INCLUDE_DIRS})

# Subdirectories

if(WITH_CYCLES_BLENDER)
	add_subdirectory(blender)
endif()

if(WITH_CYCLES_STANDALONE OR WITH_CYCLES_NETWORK)
	add_subdirectory(app)
endif()

add_subdirectory(bvh)
add_subdirectory(device)
add_subdirectory(doc)
add_subdirectory(kernel)
add_subdirectory(render)
add_subdirectory(subd)
add_subdirectory(util)

if(NOT WITH_BLENDER AND WITH_CYCLES_STANDALONE)
	delayed_do_install(${CMAKE_BINARY_DIR}/bin)
endif()

