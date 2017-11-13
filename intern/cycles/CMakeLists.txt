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
# todo: this code could be refactored a bit to avoid duplication
# note: CXX_HAS_SSE is needed in case passing SSE flags fails altogether (gcc-arm)

if(WITH_CYCLES_NATIVE_ONLY)
	set(CXX_HAS_SSE FALSE)
	set(CXX_HAS_AVX FALSE)
	set(CXX_HAS_AVX2 FALSE)
	add_definitions(
		-DWITH_KERNEL_NATIVE
	)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
	set(CYCLES_KERNEL_FLAGS "-march=native")
elseif(NOT WITH_CPU_SSE)
	set(CXX_HAS_SSE FALSE)
	set(CXX_HAS_AVX FALSE)
	set(CXX_HAS_AVX2 FALSE)
elseif(WIN32 AND MSVC)
	set(CXX_HAS_SSE TRUE)
	set(CXX_HAS_AVX TRUE)
	set(CXX_HAS_AVX2 TRUE)

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

	set(CYCLES_KERNEL_FLAGS "/fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
elseif(CMAKE_COMPILER_IS_GNUCC)
	check_cxx_compiler_flag(-msse CXX_HAS_SSE)
	check_cxx_compiler_flag(-mavx CXX_HAS_AVX)
	check_cxx_compiler_flag(-mavx2 CXX_HAS_AVX2)
	set(CYCLES_KERNEL_FLAGS "-ffast-math")
	if(CXX_HAS_SSE)
		set(CYCLES_SSE2_KERNEL_FLAGS "-ffast-math -msse -msse2 -mfpmath=sse")
		set(CYCLES_SSE3_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -mfpmath=sse")
		set(CYCLES_SSE41_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mfpmath=sse")
	endif()
	if(CXX_HAS_AVX)
		set(CYCLES_AVX_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mavx -mfpmath=sse")
	endif()
	if(CXX_HAS_AVX2)
		set(CYCLES_AVX2_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mavx -mavx2 -mfma -mlzcnt -mbmi -mbmi2 -mf16c -mfpmath=sse")
	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fno-finite-math-only")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	check_cxx_compiler_flag(-msse CXX_HAS_SSE)
	check_cxx_compiler_flag(-mavx CXX_HAS_AVX)
	check_cxx_compiler_flag(-mavx2 CXX_HAS_AVX2)
	set(CYCLES_KERNEL_FLAGS "-ffast-math")
	if(CXX_HAS_SSE)
		set(CYCLES_SSE2_KERNEL_FLAGS "-ffast-math -msse -msse2")
		set(CYCLES_SSE3_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3")
		set(CYCLES_SSE41_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1")
	endif()
	if(CXX_HAS_AVX)
		set(CYCLES_AVX_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mavx")
	endif()
	if(CXX_HAS_AVX2)
		set(CYCLES_AVX2_KERNEL_FLAGS "-ffast-math -msse -msse2 -msse3 -mssse3 -msse4.1 -mavx -mavx2 -mfma -mlzcnt -mbmi -mbmi2 -mf16c")
	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fno-finite-math-only")
endif()

if(CXX_HAS_SSE)
	add_definitions(
		-DWITH_KERNEL_SSE2
		-DWITH_KERNEL_SSE3
		-DWITH_KERNEL_SSE41
	)
endif()

if(CXX_HAS_AVX)
	add_definitions(-DWITH_KERNEL_AVX)
endif()

if(CXX_HAS_AVX2)
	add_definitions(-DWITH_KERNEL_AVX2)
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

if(WITH_CYCLES_STANDALONE_GUI)
	add_definitions(-DWITH_CYCLES_STANDALONE_GUI)
endif()

if(WITH_CYCLES_PTEX)
	add_definitions(-DWITH_PTEX)
endif()

if(WITH_CYCLES_OSL)
	add_definitions(-DWITH_OSL)
	add_definitions(-DOSL_STATIC_LIBRARY)
	include_directories(
		SYSTEM
		${OSL_INCLUDE_DIR}
	)
endif()

if(WITH_CYCLES_OPENSUBDIV)
	add_definitions(-DWITH_OPENSUBDIV)
	include_directories(
		SYSTEM
		${OPENSUBDIV_INCLUDE_DIR}
	)
endif()

set(WITH_CYCLES_DEVICE_OPENCL TRUE)
set(WITH_CYCLES_DEVICE_CUDA TRUE)
set(WITH_CYCLES_DEVICE_MULTI TRUE)

if(CYCLES_STANDALONE_REPOSITORY)
	TEST_UNORDERED_MAP_SUPPORT()
endif()
if(WITH_CXX11)
	add_definitions(-DCYCLES_STD_UNORDERED_MAP)
elseif(HAVE_STD_UNORDERED_MAP_HEADER)
	if(HAVE_UNORDERED_MAP_IN_STD_NAMESPACE)
		add_definitions(-DCYCLES_STD_UNORDERED_MAP)
	else()
		if(HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
			add_definitions(-DCYCLES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
		else()
			add_definitions(-DCYCLES_NO_UNORDERED_MAP)
			message(STATUS "Replacing unordered_map/set with map/set (warning: slower!)")
		endif()
	endif()
else()
	if(HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
		add_definitions(-DCYCLES_TR1_UNORDERED_MAP)
	else()
		add_definitions(-DCYCLES_NO_UNORDERED_MAP)
		message(STATUS "Replacing unordered_map/set with map/set (warning: slower!)")
	endif()
endif()

# Logging capabilities using GLog library.
if(WITH_CYCLES_LOGGING)
	add_definitions(-DWITH_CYCLES_LOGGING)
	add_definitions(${GLOG_DEFINES})
	add_definitions(-DCYCLES_GFLAGS_NAMESPACE=${GFLAGS_NAMESPACE})
	include_directories(
		SYSTEM
		${GLOG_INCLUDE_DIRS}
		${GFLAGS_INCLUDE_DIRS}
	)
endif()

# Debugging capabilities (debug passes etc).
if(WITH_CYCLES_DEBUG)
	add_definitions(-DWITH_CYCLES_DEBUG)
endif()

if(NOT OPENIMAGEIO_PUGIXML_FOUND)
	add_definitions(-DWITH_SYSTEM_PUGIXML)
endif()

include_directories(
	SYSTEM
	${BOOST_INCLUDE_DIR}
	${OPENIMAGEIO_INCLUDE_DIRS}
	${OPENIMAGEIO_INCLUDE_DIRS}/OpenImageIO
	${OPENEXR_INCLUDE_DIR}
	${OPENEXR_INCLUDE_DIRS}
	${PUGIXML_INCLUDE_DIR}
)

if(CYCLES_STANDALONE_REPOSITORY)
	include_directories(../third_party/atomic)
else()
	include_directories(../atomic)
endif()

# Warnings
if(CMAKE_COMPILER_IS_GNUCXX)
	ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS _has_cxxflag_float_conversion "-Werror=float-conversion")
	ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS _has_cxxflag_double_promotion "-Werror=double-promotion")
	ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS _has_no_error_unused_macros "-Wno-error=unused-macros")
	unset(_has_cxxflag_float_conversion)
	unset(_has_cxxflag_double_promotion)
	unset(_has_no_error_unused_macros)
endif()


# Subdirectories

if(WITH_CYCLES_BLENDER)
	add_definitions(-DWITH_BLENDER_GUARDEDALLOC)
	add_subdirectory(blender)
endif()

if(WITH_CYCLES_NETWORK)
	add_definitions(-DWITH_NETWORK)
endif()

if(WITH_CYCLES_STANDALONE OR WITH_CYCLES_NETWORK)
	add_subdirectory(app)
endif()

add_subdirectory(bvh)
add_subdirectory(device)
add_subdirectory(doc)
add_subdirectory(graph)
add_subdirectory(kernel)
add_subdirectory(render)
add_subdirectory(subd)
add_subdirectory(util)

# TODO(sergey): Make this to work with standalone repository.
if(WITH_GTESTS)
	add_subdirectory(test)
endif()

if(NOT WITH_BLENDER AND WITH_CYCLES_STANDALONE)
	delayed_do_install(${CMAKE_BINARY_DIR}/bin)
endif()

