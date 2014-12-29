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
# note: CXX_HAS_SSE is needed in case passing SSE flags fails altogether (gcc-arm)

if(NOT WITH_CPU_SSE)
	set(CXX_HAS_SSE FALSE)
elseif(WIN32 AND MSVC)
	set(CXX_HAS_SSE TRUE)

	# /arch:AVX for VC2012 and above
	if(NOT MSVC_VERSION LESS 1700)
		set(CYCLES_AVX_ARCH_FLAGS "/arch:AVX")
		set(CYCLES_AVX2_ARCH_FLAGS "/arch:AVX /arch:AVX2")
	elseif(NOT CMAKE_CL_64)
		set(CYCLES_AVX_ARCH_FLAGS "/arch:SSE2")
		set(CYCLES_AVX2_ARCH_FLAGS "/arch:SSE2")
	endif()

	# there is no /arch:SSE3, but intrinsics are available anyway
	if(CMAKE_CL_64)
		set(CYCLES_SSE2_KERNEL_FLAGS "/fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
		set(CYCLES_SSE3_KERNEL_FLAGS "/fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
		set(CYCLES_SSE41_KERNEL_FLAGS "/fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
		set(CYCLES_AVX_KERNEL_FLAGS "${CYCLES_AVX_ARCH_FLAGS} /fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
		set(CYCLES_AVX2_KERNEL_FLAGS "${CYCLES_AVX2_ARCH_FLAGS} /fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
	else()
		set(CYCLES_SSE2_KERNEL_FLAGS "/arch:SSE2 /fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
		set(CYCLES_SSE3_KERNEL_FLAGS "/arch:SSE2 /fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
		set(CYCLES_SSE41_KERNEL_FLAGS "/arch:SSE2 /fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
		set(CYCLES_AVX_KERNEL_FLAGS "${CYCLES_AVX_ARCH_FLAGS} /fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
		set(CYCLES_AVX2_KERNEL_FLAGS "${CYCLES_AVX2_ARCH_FLAGS} /fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ox")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Ox")
	set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /Ox")
elseif(CMAKE_COMPILER_IS_GNUCC)
	check_cxx_compiler_flag(-msse CXX_HAS_SSE)
	if(CXX_HAS_SSE)
		set(CYCLES_SSE2_KERNEL_FLAGS "-ffast-math -msse -msse2 -mfpmath=sse")
		set(CYCLES_SSE3_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -mfpmath=sse")
		set(CYCLES_SSE41_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mfpmath=sse")
		set(CYCLES_AVX_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mavx -mfpmath=sse")
		set(CYCLES_AVX2_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mavx -mavx2 -mfma -mlzcnt -mbmi -mbmi2 -mfpmath=sse")
	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	check_cxx_compiler_flag(-msse CXX_HAS_SSE)
	if(CXX_HAS_SSE)
		set(CYCLES_SSE2_KERNEL_FLAGS "-ffast-math -msse -msse2")
		set(CYCLES_SSE3_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3")
		set(CYCLES_SSE41_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1")
		set(CYCLES_AVX_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mavx")
		set(CYCLES_AVX2_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mavx -mavx2 -mfma -mlzcnt -mbmi -mbmi2")
	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
endif()

if(CXX_HAS_SSE)
	add_definitions(
		-DWITH_KERNEL_SSE2
		-DWITH_KERNEL_SSE3
		-DWITH_KERNEL_SSE41
		-DWITH_KERNEL_AVX
		-DWITH_KERNEL_AVX2
	)
endif()

if(WITH_CYCLES_OSL)
	if(WIN32 AND MSVC)
		set(RTTI_DISABLE_FLAGS "/GR- -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")
	elseif(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
		set(RTTI_DISABLE_FLAGS "-fno-rtti -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")
	endif()
endif()

# Definitions and Includes

add_definitions(
	${BOOST_DEFINITIONS}
	${OPENIMAGEIO_DEFINITIONS}
)

add_definitions(
	-DCCL_NAMESPACE_BEGIN=namespace\ ccl\ {
	-DCCL_NAMESPACE_END=}
)

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
	include_directories(
		SYSTEM
		${OSL_INCLUDE_DIR}
	)
endif()

add_definitions(
	-DWITH_OPENCL
	-DWITH_CUDA
	-DWITH_MULTI
)

# Logging capabilities using GLog library.
if(WITH_CYCLES_LOGGING)
	add_definitions(-DWITH_CYCLES_LOGGING)
	add_definitions(-DGOOGLE_GLOG_DLL_DECL=)
	if(WIN32)
		include_directories(
			SYSTEM
			../../extern/libmv/third_party/glog/src/windows
			../../extern/libmv/third_party/gflags
		)
	else()
		include_directories(
			SYSTEM
			../../extern/libmv/third_party/glog/src
			../../extern/libmv/third_party/gflags
		)
	endif()
endif()

# Debugging capabilities (debug passes etc).
if(WITH_CYCLES_DEBUG)
	add_definitions(-DWITH_CYCLES_DEBUG)
endif()

include_directories(
	SYSTEM
	${BOOST_INCLUDE_DIR}
	${OPENIMAGEIO_INCLUDE_DIRS}
	${OPENIMAGEIO_INCLUDE_DIRS}/OpenImageIO
	${OPENEXR_INCLUDE_DIR}
	${OPENEXR_INCLUDE_DIRS}
)

# TODO(sergey): Adjust so standalone repository is also happy.
include_directories(
	../atomic
)

# Warnings
if(CMAKE_COMPILER_IS_GNUCXX)
	ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS _has_cxxflag_float_conversion "-Werror=float-conversion")
	ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS _has_cxxflag_double_promotion "-Werror=double-promotion")
	unset(_has_cxxflag_float_conversion)
	unset(_has_cxxflag_double_promotion)
endif()


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

