# Standalone or with Blender
if(NOT WITH_BLENDER AND WITH_CYCLES_STANDALONE)
  set(CYCLES_INSTALL_PATH "")
else()
  set(WITH_CYCLES_BLENDER ON)
  # WINDOWS_PYTHON_DEBUG needs to write into the user addons folder since it will
  # be started with --env-system-scripts pointing to the release folder, which will
  # lack the cycles addon, and we don't want to write into it.
  if(NOT WINDOWS_PYTHON_DEBUG)
    set(CYCLES_INSTALL_PATH "scripts/addons/cycles")
  else()
    set(CYCLES_INSTALL_PATH "$ENV{appdata}/blender foundation/blender/${BLENDER_VERSION}/scripts/addons/cycles")
  endif()
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
elseif(WIN32 AND MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
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
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  check_cxx_compiler_flag(/QxSSE2 CXX_HAS_SSE)
  check_cxx_compiler_flag(/arch:AVX CXX_HAS_AVX)
  check_cxx_compiler_flag(/QxCORE-AVX2 CXX_HAS_AVX2)

  if(CXX_HAS_SSE)
    set(CYCLES_SSE2_KERNEL_FLAGS "/QxSSE2")
    set(CYCLES_SSE3_KERNEL_FLAGS "/QxSSSE3")
    set(CYCLES_SSE41_KERNEL_FLAGS "/QxSSE4.1")

    if(CXX_HAS_AVX)
      set(CYCLES_AVX_KERNEL_FLAGS "/arch:AVX")
    endif()

    if(CXX_HAS_AVX2)
      set(CYCLES_AVX2_KERNEL_FLAGS "/QxCORE-AVX2")
    endif()
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  if(APPLE)
    # ICC does not support SSE2 flag on MacOSX
    check_cxx_compiler_flag(-xssse3 CXX_HAS_SSE)
  else()
    check_cxx_compiler_flag(-xsse2 CXX_HAS_SSE)
  endif()

  check_cxx_compiler_flag(-xavx CXX_HAS_AVX)
  check_cxx_compiler_flag(-xcore-avx2 CXX_HAS_AVX2)

  if(CXX_HAS_SSE)
    if(APPLE)
      # ICC does not support SSE2 flag on MacOSX
      set(CYCLES_SSE2_KERNEL_FLAGS "-xssse3")
    else()
      set(CYCLES_SSE2_KERNEL_FLAGS "-xsse2")
    endif()

    set(CYCLES_SSE3_KERNEL_FLAGS "-xssse3")
    set(CYCLES_SSE41_KERNEL_FLAGS "-xsse4.1")

    if(CXX_HAS_AVX)
      set(CYCLES_AVX_KERNEL_FLAGS "-xavx")
    endif()

    if(CXX_HAS_AVX2)
      set(CYCLES_AVX2_KERNEL_FLAGS "-xcore-avx2")
    endif()
  endif()
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

# LLVM and OSL need to build without RTTI
if(WIN32 AND MSVC)
  set(RTTI_DISABLE_FLAGS "/GR- -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")
elseif(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
  set(RTTI_DISABLE_FLAGS "-fno-rtti -DBOOST_NO_RTTI -DBOOST_NO_TYPEID")
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
  # osl 1.9.x
  add_definitions(-DOSL_STATIC_BUILD)
  # pre 1.9
  add_definitions(-DOSL_STATIC_LIBRARY)
  include_directories(
    SYSTEM
    ${OSL_INCLUDE_DIR}
  )
endif()

if(WITH_CYCLES_DEVICE_OPTIX)
  find_package(OptiX)

  if(OPTIX_FOUND)
    add_definitions(-DWITH_OPTIX)
    include_directories(
      SYSTEM
      ${OPTIX_INCLUDE_DIR}
      )
  else()
    message(STATUS "OptiX not found, disabling it from Cycles")
    set(WITH_CYCLES_DEVICE_OPTIX OFF)
  endif()
endif()

if(WITH_CYCLES_EMBREE)
  add_definitions(-DWITH_EMBREE)
  add_definitions(-DEMBREE_STATIC_LIB)
  include_directories(
    SYSTEM
    ${EMBREE_INCLUDE_DIRS}
  )
endif()

if(WITH_OPENSUBDIV)
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
  ${TBB_INCLUDE_DIRS}
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
    elseif(${CUDA_VERSION} LESS "11.0")
      set(MAX_MSVC 1999)
    endif()
    if(NOT MSVC_VERSION LESS ${MAX_MSVC} OR CMAKE_C_COMPILER_ID MATCHES "Clang")
      message(STATUS "nvcc not supported for this compiler version, using cycles_cubin_cc instead.")
      set(WITH_CYCLES_CUBIN_COMPILER ON)
    endif()
    unset(MAX_MSVC)
  elseif(APPLE)
    if(NOT (${XCODE_VERSION} VERSION_LESS 10.0))
      message(STATUS "nvcc not supported for this compiler version, using cycles_cubin_cc instead.")
      set(WITH_CYCLES_CUBIN_COMPILER ON)
    endif()
  endif()
endif()

# NVRTC gives wrong rendering result in CUDA 10.0, so we must use NVCC.
if(WITH_CYCLES_CUDA_BINARIES AND WITH_CYCLES_CUBIN_COMPILER AND NOT WITH_CYCLES_CUBIN_COMPILER_OVERRRIDE)
  if(NOT (${CUDA_VERSION} VERSION_LESS 10.0))
    message(STATUS "cycles_cubin_cc not supported for CUDA 10.0+, using nvcc instead.")
    set(WITH_CYCLES_CUBIN_COMPILER OFF)
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
