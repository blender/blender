# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

# Standalone or with Blender
if(NOT WITH_BLENDER)
  set(CYCLES_INSTALL_PATH ${CMAKE_INSTALL_PREFIX})
else()
  set(WITH_CYCLES_BLENDER ON)
  # WINDOWS_PYTHON_DEBUG needs to write into the user addons folder since it will
  # be started with --env-system-scripts pointing to the release folder, which will
  # lack the cycles addon, and we don't want to write into it.
  if(NOT WINDOWS_PYTHON_DEBUG)
    set(CYCLES_INSTALL_PATH "scripts/addons_core/cycles")
  else()
    set(CYCLES_INSTALL_PATH
      "$ENV{appdata}/blender foundation/blender/${BLENDER_VERSION}/scripts/addons_core/cycles"
    )
  endif()
endif()

# External Libraries

if(NOT CYCLES_STANDALONE_REPOSITORY)
  include(cmake/external_libs.cmake)
  include(cmake/macros.cmake)
endif()

# Build Flags
# TODO: this code could be refactored a bit to avoid duplication.
# NOTE: CXX_HAS_SSE42 is needed in case passing SSE flags fails altogether (`gcc-arm`).

# Determine vectorization support and required build flags
if(WITH_CYCLES_NATIVE_ONLY)
  set(CXX_HAS_SSE42 FALSE)
  set(CXX_HAS_AVX2 FALSE)
  add_definitions(
    -DWITH_KERNEL_NATIVE
  )

  if(NOT MSVC)
    add_check_cxx_compiler_flags(
      CMAKE_CXX_FLAGS
      _has_march_native "-march=native"
    )
    if(_has_march_native)
      string(APPEND CMAKE_CXX_FLAGS " -march=native")
    else()
      string(APPEND CMAKE_CXX_FLAGS "")
    endif()
    unset(_has_march_native)
  else()
    if(NOT MSVC_NATIVE_ARCH_FLAGS)
      try_run(
        arch_run_result
        arch_compile_result
        ${CMAKE_CURRENT_BINARY_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/msvc_arch_flags.c
        COMPILE_OUTPUT_VARIABLE arch_compile_output
        RUN_OUTPUT_VARIABLE arch_run_output
      )
      if(arch_compile_result AND "${arch_run_result}" EQUAL "0")
        string(STRIP ${arch_run_output} arch_run_output)
        set(MSVC_NATIVE_ARCH_FLAGS ${arch_run_output} CACHE STRING "MSVC Native architecture flags")
      endif()
    endif()
    string(APPEND CMAKE_CXX_FLAGS " ${MSVC_NATIVE_ARCH_FLAGS}")
  endif()
elseif(WIN32 AND MSVC AND SUPPORT_NEON_BUILD AND SSE2NEON_FOUND)
  set(CXX_HAS_SSE42 FALSE)
  set(CXX_HAS_AVX2 FALSE)
elseif(NOT WITH_CPU_SIMD OR (SUPPORT_NEON_BUILD AND SSE2NEON_FOUND))
  set(CXX_HAS_SSE42 FALSE)
  set(CXX_HAS_AVX2 FALSE)
elseif(WIN32 AND MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CXX_HAS_SSE42 TRUE)
  set(CXX_HAS_AVX2 TRUE)

  # /arch:AVX for VC2012 and above
  if(NOT MSVC_VERSION LESS 1700)
    set(CYCLES_AVX2_FLAGS "/arch:AVX /arch:AVX2")
  elseif(NOT CMAKE_CL_64)
    set(CYCLES_AVX2_FLAGS "/arch:SSE2")
  endif()

  # there is no /arch:SSE3, but intrinsics are available anyway
  if(CMAKE_CL_64)
    set(CYCLES_SSE42_FLAGS "")
  else()
    set(CYCLES_SSE42_FLAGS "/arch:SSE2")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
  check_cxx_compiler_flag(-msse4.2 CXX_HAS_SSE42)
  check_cxx_compiler_flag(-mavx2 CXX_HAS_AVX2)

  if(CXX_HAS_SSE42)
    set(CYCLES_SSE42_FLAGS "-msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2")
    if(CXX_HAS_AVX2)
      set(CYCLES_AVX2_FLAGS "${CYCLES_SSE42_FLAGS} -mavx -mavx2 -mfma -mlzcnt -mbmi -mbmi2 -mf16c")
    endif()
  endif()

elseif(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  check_cxx_compiler_flag(/QxSSE4.2 CXX_HAS_SSE42)
  check_cxx_compiler_flag(/QxCORE-AVX2 CXX_HAS_AVX2)

  if(CXX_HAS_SSE42)
    set(CYCLES_SSE42_FLAGS "/QxSSE4.2")

    if(CXX_HAS_AVX2)
      set(CYCLES_AVX2_FLAGS "/QxCORE-AVX2")
    endif()
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  check_cxx_compiler_flag(-xsse4.2 CXX_HAS_SSE42)
  check_cxx_compiler_flag(-xcore-avx2 CXX_HAS_AVX2)

  if(CXX_HAS_SSE42)
    set(CYCLES_SSE42_FLAGS "-xsse4.2")

    if(CXX_HAS_AVX2)
      set(CYCLES_AVX2_FLAGS "-xcore-avx2")
    endif()
  endif()
endif()

if(CXX_HAS_SSE42)
  add_definitions(
    -DWITH_KERNEL_SSE42
  )
  # We require SSE4.2 as a minimum, so make use of it
  string(APPEND CMAKE_CXX_FLAGS " ${CYCLES_SSE42_FLAGS}")
endif()

if(CXX_HAS_AVX2)
  add_definitions(-DWITH_KERNEL_AVX2)
endif()

# Enable math optimizations

if(WIN32 AND MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Unlike GCC/clang we still use fast math, because there is no fine
  # grained control and the speedup we get here is too big to ignore.
  string(APPEND CMAKE_CXX_FLAGS " /fp:fast -D_CRT_SECURE_NO_WARNINGS /GS-")
  string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Ox")
  string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " /Ox")
  string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL " /Ox")

  # `jumptablerdata` improves performance when there is contention in large switch statements
  # such as in `svm.h`.
  # This flag is supported starting with MSVC 17.7 preview 3:
  # https://learn.microsoft.com/en-us/cpp/build/reference/jump-table-rdata
  # This is an x64 specific optimization.
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.37.32820
     AND CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
    string(APPEND CMAKE_CXX_FLAGS " /jumptablerdata")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
  # Assume no signal trapping for better code generation.
  list(APPEND CYCLES_MATH_FLAGS "-fno-trapping-math")
  # Avoid overhead of setting `errno` for NaNs.
  list(APPEND CYCLES_MATH_FLAGS "-fno-math-errno")
  # Let compiler optimize 0.0 - x without worrying about signed zeros.
  list(APPEND CYCLES_MATH_FLAGS "-fno-signed-zeros")
  # Let the compiler generate fused multiply-add instructions
  list(APPEND CYCLES_MATH_FLAGS "-ffp-contract=fast")
  # Let the compiler replace x/y with x*(1/y)
  list(APPEND CYCLES_MATH_FLAGS "-freciprocal-math")
  # Let the compiler reorder terms to save operations
  # NOTE: Disabled for now due to problems with bsdf_D for GGX on Linux (#130389)
  # list(APPEND CYCLES_MATH_FLAGS "-fassociative-math")
  # Don't enable `-ffinite-math-only` since the BVH code relies on NaNs.
  # Otherwise, we could just use `-ffast-math`.

  if(CMAKE_COMPILER_IS_GNUCC)
    # Assume no signal trapping for better code generation.
    list(APPEND CYCLES_MATH_FLAGS "-fno-signaling-nans")
    # Assume a fixed rounding mode for better constant folding.
    list(APPEND CYCLES_MATH_FLAGS "-fno-rounding-math")

    if(CXX_HAS_SSE42)
      list(APPEND CYCLES_MATH_FLAGS "-mfpmath=sse")
    endif()
  endif()

  if(WIN32 AND MSVC)
    # Pass clang flags directly to clang otherwise. Clang-cl doesn't recognize
    # these flags by default
    list(TRANSFORM CYCLES_MATH_FLAGS PREPEND "/clang:")
  endif()

  list(JOIN CYCLES_MATH_FLAGS " " CYCLES_MATH_FLAGS)
  string(APPEND CMAKE_CXX_FLAGS " ${CYCLES_MATH_FLAGS}")
endif()

# Definitions and Includes

add_definitions(
  ${BOOST_DEFINITIONS}
)

add_definitions(
  -DCCL_NAMESPACE_BEGIN=namespace\ ccl\ {
  -DCCL_NAMESPACE_END=}
)

include_directories(
  SYSTEM
  ${BOOST_INCLUDE_DIR}
  ${OPENIMAGEIO_INCLUDE_DIRS}
  ${IMATH_INCLUDE_DIRS}
  ${OPENEXR_INCLUDE_DIRS}
  ${PUGIXML_INCLUDE_DIR}
)

if(WITH_CYCLES_DEBUG)
  add_definitions(-DWITH_CYCLES_DEBUG)
endif()
if(WITH_CYCLES_STANDALONE_GUI)
  add_definitions(-DWITH_CYCLES_STANDALONE_GUI)
endif()

if(WITH_CYCLES_PTEX)
  add_definitions(-DWITH_PTEX)
endif()

if(WITH_CYCLES_OSL)
  add_definitions(-DWITH_OSL)
  include_directories(
    SYSTEM
    ${OSL_INCLUDE_DIR}
  )
endif()

if(WITH_CYCLES_DEVICE_CUDA OR WITH_CYCLES_DEVICE_OPTIX)
  add_definitions(-DWITH_CUDA)

  if(WITH_CUDA_DYNLOAD)
    include_directories(
      ../../extern/cuew/include
    )
    add_definitions(-DWITH_CUDA_DYNLOAD)
  else()
    include_directories(
      SYSTEM
      ${CUDA_TOOLKIT_INCLUDE}
    )
  endif()
endif()

if(WITH_CYCLES_DEVICE_HIP)
  add_definitions(-DWITH_HIP)

  if(WITH_CYCLES_DEVICE_HIPRT)
    include_directories(
      ${HIPRT_INCLUDE_DIR}
    )
    add_definitions(-DWITH_HIPRT)
  endif()

  if(WITH_HIP_DYNLOAD)
    include_directories(
      ../../extern/hipew/include
    )
    add_definitions(-DWITH_HIP_DYNLOAD)
  endif()
endif()

if(WITH_CYCLES_DEVICE_OPTIX)
  find_package(OptiX 8.0.0)

  if(OPTIX_FOUND)
    add_definitions(-DWITH_OPTIX)
    include_directories(
      SYSTEM
      ${OPTIX_INCLUDE_DIR}
    )
  else()
    set_and_warn_library_found("OptiX" OPTIX_FOUND WITH_CYCLES_DEVICE_OPTIX)
  endif()
endif()

if(WITH_CYCLES_DEVICE_METAL)
  add_definitions(-DWITH_METAL)
endif()

if(WITH_CYCLES_DEVICE_ONEAPI)
  add_definitions(-DWITH_ONEAPI)
endif()

if(WITH_CYCLES_EMBREE)
  add_definitions(-DWITH_EMBREE)
  if(WITH_CYCLES_DEVICE_ONEAPI AND EMBREE_SYCL_SUPPORT)
    # NOTE: The debug version of Embree is built without SYCL support on Windows
    # since 7fb480095e371f8f5ac4f647f0ba2fd78da486f7. This is not reflected in
    # EMBREE_SYCL_SUPPORT which is coming from Embree headers that aren't
    # differentiated for release and debug, so we handle this case here by
    # disabling its use when embree4_sycl_d.lib doesn't exist.
    set(EMBREE_SYCL_DEBUG_LIBRARY ${EMBREE_LIBRARIES})
    list(FILTER EMBREE_SYCL_DEBUG_LIBRARY INCLUDE REGEX "_sycl_d\\.lib$")
    if(WIN32 AND NOT EMBREE_SYCL_DEBUG_LIBRARY)
      add_compile_definitions("$<$<CONFIG:Release>:WITH_EMBREE_GPU>")
      add_compile_definitions("$<$<CONFIG:RelWithDebInfo>:WITH_EMBREE_GPU>")
      add_compile_definitions("$<$<CONFIG:MinSizeRel>:WITH_EMBREE_GPU>")
      if(CMAKE_BUILD_TYPE MATCHES "Debug" OR GENERATOR_IS_MULTI_CONFIG)
        message(STATUS
          "The use of Embree GPU is disabled for the Debug configuration "
          "as embree${EMBREE_MAJOR_VERSION}_sycl_d.lib is not found."
        )
      endif()
    else()
      add_definitions(-DWITH_EMBREE_GPU)
    endif()
  endif()
  add_definitions(-DEMBREE_MAJOR_VERSION=${EMBREE_MAJOR_VERSION})
  include_directories(
    SYSTEM
    ${EMBREE_INCLUDE_DIRS}
  )
endif()

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
  include_directories(
    SYSTEM
    ${OPENIMAGEDENOISE_INCLUDE_DIRS}
  )
endif()

# Includes that might be overrides by USD last, to avoid compiling
# against the wrong versions of other libraries.
include_directories(
  SYSTEM
  ${TBB_INCLUDE_DIRS}
)

if(WITH_OPENVDB)
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
  include_directories(
    SYSTEM
    ${OPENVDB_INCLUDE_DIRS}
  )
endif()

if(WITH_NANOVDB)
  add_definitions(-DWITH_NANOVDB)
  include_directories(
    SYSTEM
    ${NANOVDB_INCLUDE_DIR}
  )
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
  include_directories(
    SYSTEM
    ${OPENSUBDIV_INCLUDE_DIRS}
  )
endif()

if(WITH_OPENCOLORIO)
  add_definitions(-DWITH_OCIO)
  include_directories(
    SYSTEM
    ${OPENCOLORIO_INCLUDE_DIRS}
  )
endif()

if(WITH_CYCLES_PATH_GUIDING)
  add_definitions(-DWITH_PATH_GUIDING)

  # The level of the guiding integration.
  # Different levels can be selected to measure the overhead of different stages.
  # 1 = recording the path segments
  # 2 = 1 + generating (not storing) sample data from the segments
  # 3 = 2 + storing the generates sample data
  # 4 = 3 + training the guiding fields
  # 5 = 4 + querying the trained guiding for sampling (full path guiding)
  add_definitions(-DPATH_GUIDING_LEVEL=5)

  include_directories(
    SYSTEM
    ${OPENPGL_INCLUDE_DIR}
  )
endif()

# NaN debugging
if(WITH_CYCLES_DEBUG_NAN)
  add_definitions(-DWITH_CYCLES_DEBUG_NAN)
endif()

if(WITH_PUGIXML OR OPENIMAGEIO_PUGIXML_FOUND)
  add_definitions(-DWITH_PUGIXML)
  if((NOT OPENIMAGEIO_PUGIXML_FOUND) OR WIN32)
    add_definitions(-DWITH_SYSTEM_PUGIXML)
  endif()
endif()

if(CYCLES_STANDALONE_REPOSITORY)
  include_directories(../third_party/atomic)
else()
  include_directories(../atomic)
endif()

# Warnings
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_check_cxx_compiler_flags(
    CMAKE_CXX_FLAGS
    _has_no_error_unused_macros "-Wno-error=unused-macros"
  )
  unset(_has_no_error_unused_macros)
endif()

if(WITH_USD)
  add_definitions(-DWITH_USD)
endif()

if(WITH_CYCLES_HYDRA_RENDER_DELEGATE AND (NOT WITH_USD))
  set_and_warn_library_found("USD" WITH_USD WITH_CYCLES_HYDRA_RENDER_DELEGATE)
endif()
if(WITH_CYCLES_HYDRA_RENDER_DELEGATE AND (NOT WITH_BLENDER) AND (NOT WITH_CYCLES_STANDALONE))
  set(CYCLES_INSTALL_PATH ${CYCLES_INSTALL_PATH}/hdCycles/resources)
endif()

if(WITH_CYCLES_CUDA_BINARIES)
  if(MSVC)
    set(MAX_MSVC 1800)
    if(${CUDA_VERSION} EQUAL "8.0")
      set(MAX_MSVC 1900)
    elseif(${CUDA_VERSION} EQUAL "9.0")
      set(MAX_MSVC 1910)
    elseif(${CUDA_VERSION} EQUAL "9.1")
      set(MAX_MSVC 1911)
    elseif(${CUDA_VERSION} VERSION_GREATER_EQUAL 10.0)
      set(MAX_MSVC 1999)
    endif()
    unset(MAX_MSVC)
  endif()
endif()

# Subdirectories

if(WITH_CYCLES_BLENDER)
  # Not needed to make cycles automated tests pass with -march=native.
  # However Blender itself needs this flag.
  # Note: the clang-cl style removal must go first, to avoid a dangling "/clang:"
  remove_cc_flag("/clang:-ffp-contract=off")
  remove_cc_flag("-ffp-contract=off")
  add_definitions(-DWITH_BLENDER_GUARDEDALLOC)
  add_subdirectory(blender)
endif()

add_subdirectory(app)
add_subdirectory(bvh)
add_subdirectory(device)
add_subdirectory(doc)
add_subdirectory(graph)
add_subdirectory(integrator)
add_subdirectory(kernel)
add_subdirectory(scene)
add_subdirectory(session)
add_subdirectory(subd)
add_subdirectory(util)

# TODO(sergey): Make this to work with standalone repository.
if(WITH_GTESTS)
  add_subdirectory(test)
endif()

if(WITH_CYCLES_HYDRA_RENDER_DELEGATE OR (WITH_CYCLES_STANDALONE AND WITH_USD))
  add_subdirectory(hydra)
endif()

if(NOT WITH_BLENDER)
  if(CYCLES_STANDALONE_REPOSITORY)
    delayed_do_install()
  else()
    delayed_do_install(${CMAKE_BINARY_DIR}/bin)
  endif()
endif()
