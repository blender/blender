# Standalone or with Blender
if(NOT WITH_BLENDER AND WITH_CYCLES_STANDALONE)
	set(CYCLES_INSTALL_PATH "")
else()
	set(WITH_CYCLES_BLENDER ON)
	set(CYCLES_INSTALL_PATH "scripts/addons/cycles")
endif()

# External Libraries

include(cmake/external_libs.cmake)
include(cmake/macros.cmake)

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

	if(NOT MSVC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
		set(CYCLES_KERNEL_FLAGS "-march=native")
	endif()
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

	# Unlike GCC/clang we still use fast math, because there is no fine
	# grained control and the speedup we get here is too big to ignore.
	set(CYCLES_KERNEL_FLAGS "/fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")

	# there is no /arch:SSE3, but intrinsics are available anyway
	if(CMAKE_CL_64)
		set(CYCLES_SSE2_KERNEL_FLAGS "${CYCLES_KERNEL_FLAGS}")
		set(CYCLES_SSE3_KERNEL_FLAGS "${CYCLES_KERNEL_FLAGS}")
		set(CYCLES_SSE41_KERNEL_FLAGS "${CYCLES_KERNEL_FLAGS}")
		set(CYCLES_AVX_KERNEL_FLAGS "${CYCLES_AVX_ARCH_FLAGS} ${CYCLES_KERNEL_FLAGS}")
		set(CYCLES_AVX2_KERNEL_FLAGS "${CYCLES_AVX2_ARCH_FLAGS} ${CYCLES_KERNEL_FLAGS}")
	else()
		set(CYCLES_SSE2_KERNEL_FLAGS "/arch:SSE2 ${CYCLES_KERNEL_FLAGS}")
		set(CYCLES_SSE3_KERNEL_FLAGS "/arch:SSE2 ${CYCLES_KERNEL_FLAGS}")
		set(CYCLES_SSE41_KERNEL_FLAGS "/arch:SSE2 ${CYCLES_KERNEL_FLAGS}")
		set(CYCLES_AVX_KERNEL_FLAGS "${CYCLES_AVX_ARCH_FLAGS} ${CYCLES_KERNEL_FLAGS}")
		set(CYCLES_AVX2_KERNEL_FLAGS "${CYCLES_AVX2_ARCH_FLAGS} ${CYCLES_KERNEL_FLAGS}")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CYCLES_KERNEL_FLAGS}")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ox")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Ox")
	set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /Ox")
elseif(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
	check_cxx_compiler_flag(-msse CXX_HAS_SSE)
	check_cxx_compiler_flag(-mavx CXX_HAS_AVX)
	check_cxx_compiler_flag(-mavx2 CXX_HAS_AVX2)

	# Assume no signal trapping for better code generation.
	set(CYCLES_KERNEL_FLAGS "-fno-trapping-math")
	# Avoid overhead of setting errno for NaNs.
	set(CYCLES_KERNEL_FLAGS "${CYCLES_KERNEL_FLAGS} -fno-math-errno")
	# Let compiler optimize 0.0 - x without worrying about signed zeros.
	set(CYCLES_KERNEL_FLAGS "${CYCLES_KERNEL_FLAGS} -fno-signed-zeros")

	if(CMAKE_COMPILER_IS_GNUCC)
		# Assume no signal trapping for better code generation.
		set(CYCLES_KERNEL_FLAGS "${CYCLES_KERNEL_FLAGS} -fno-signaling-nans")
		# Assume a fixed rounding mode for better constant folding.
		set(CYCLES_KERNEL_FLAGS "${CYCLES_KERNEL_FLAGS} -fno-rounding-math")
	endif()

	if(CXX_HAS_SSE)
		if(CMAKE_COMPILER_IS_GNUCC)
			set(CYCLES_KERNEL_FLAGS "${CYCLES_KERNEL_FLAGS} -mfpmath=sse")
		endif()

		set(CYCLES_SSE2_KERNEL_FLAGS "${CYCLES_KERNEL_FLAGS} -msse -msse2")
		set(CYCLES_SSE3_KERNEL_FLAGS "${CYCLES_SSE2_KERNEL_FLAGS} -msse3 -mssse3")
		set(CYCLES_SSE41_KERNEL_FLAGS "${CYCLES_SSE3_KERNEL_FLAGS} -msse4.1")
		if(CXX_HAS_AVX)
			set(CYCLES_AVX_KERNEL_FLAGS "${CYCLES_SSE41_KERNEL_FLAGS} -mavx")
		endif()
		if(CXX_HAS_AVX2)
			set(CYCLES_AVX2_KERNEL_FLAGS "${CYCLES_SSE41_KERNEL_FLAGS} -mavx -mavx2 -mfma -mlzcnt -mbmi -mbmi2 -mf16c")
		endif()
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CYCLES_KERNEL_FLAGS}")
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

if(WITH_CYCLES_STANDALONE)
	set(WITH_CYCLES_DEVICE_OPENCL TRUE)
	set(WITH_CYCLES_DEVICE_CUDA TRUE)
	# Experimental and unfinished.
	set(WITH_CYCLES_NETWORK FALSE)
endif()
# TODO(sergey): Consider removing it, only causes confusion in interface.
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

if(WITH_CYCLES_CUDA_BINARIES AND (NOT WITH_CYCLES_CUBIN_COMPILER))
	if(MSVC)
		set(MAX_MSVC 1800)
		if(${CUDA_VERSION} EQUAL "8.0")
			set(MAX_MSVC 1900)
		elseif(${CUDA_VERSION} EQUAL "9.0")
			set(MAX_MSVC 1910)
		elseif(${CUDA_VERSION} EQUAL "9.1")
			set(MAX_MSVC 1911)
		endif()
		if(NOT MSVC_VERSION LESS ${MAX_MSVC})
			message(STATUS "nvcc not supported for this compiler version, using cycles_cubin_cc instead.")
			set(WITH_CYCLES_CUBIN_COMPILER ON)
		endif()
		unset(MAX_MSVC)
	endif()
endif()


# Subdirectories

if(WITH_CYCLES_BLENDER)
	add_definitions(-DWITH_BLENDER_GUARDEDALLOC)
	add_subdirectory(blender)
endif()

if(WITH_CYCLES_NETWORK)
	add_definitions(-DWITH_NETWORK)
endif()

if(WITH_CYCLES_STANDALONE OR WITH_CYCLES_NETWORK OR WITH_CYCLES_CUBIN_COMPILER)
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

