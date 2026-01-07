# SPDX-FileCopyrightText: 2011-2026 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ../../..
)

set(INC_SYS

)

set(SRC_KERNEL_DEVICE_CUDA
  kernel.cu
)

set(SRC_KERNEL_DEVICE_CUDA_HEADERS
  compat.h
  config.h
  globals.h
)

set(LIB

)

function(cuda_get_version out_version)
  execute_process(COMMAND ${CUDA_NVCC_EXECUTABLE} "--version" OUTPUT_VARIABLE NVCC_OUT)
  string(REGEX REPLACE ".*release ([0-9]+)\\.([0-9]+).*" "\\1" CUDA_VERSION_MAJOR "${NVCC_OUT}")
  string(REGEX REPLACE ".*release ([0-9]+)\\.([0-9]+).*" "\\2" CUDA_VERSION_MINOR "${NVCC_OUT}")
  set(${out_version} "${CUDA_VERSION_MAJOR}${CUDA_VERSION_MINOR}" PARENT_SCOPE)
endfunction()

function(cuda_add_common_flags cuda_version arch in_flags out_flags)
  set(flags ${in_flags})

  if(CUDA_HOST_COMPILER)
    set(flags ${flags} -ccbin="${CUDA_HOST_COMPILER}")
  endif()

  set(flags ${flags}
    # Helps with compatibility when using recent clang host compiler.
    "-std=c++17"
    --use_fast_math
    -Wno-deprecated-gpu-targets)

  if(WITH_CYCLES_DEBUG)
    set(flags ${flags}
      -D WITH_CYCLES_DEBUG
      --ptxas-options="-v")
  endif()

  if(WITH_NANOVDB)
    set(flags ${flags} -D WITH_NANOVDB)
  endif()

  if("${cuda_version}" GREATER_EQUAL 123 AND "${arch}" STREQUAL "sm_120")
    # Enable jump table generation for the SVM switch statement.
    set(flags ${flags} --jump-table-density 80)
  endif()

  if(NOT WITH_CYCLES_CUDA_BUILD_SERIAL AND "${cuda_version}" GREATER_EQUAL 129)
    # Only use split compile with few binaries, to avoid excessive memory usage.
    # This is mainly helpful for quick local builds for one architecture.
    list(LENGTH CYCLES_CUDA_BINARIES_ARCH _num_binaries)
    if(_num_binaries LESS_EQUAL 2)
      set(flags ${flags} --split-compile=0)
    endif()
  endif()

  set(${out_flags} ${flags} PARENT_SCOPE)
endfunction()

if(WITH_CYCLES_CUDA_BINARIES)
  # 64 bit only
  set(CUDA_BITS 64)

  # CUDA version
  cuda_get_version(CUDA_VERSION)

  # warn for other versions
  if((CUDA_VERSION STREQUAL "101") OR
     (CUDA_VERSION STREQUAL "102") OR
     (CUDA_VERSION_MAJOR STREQUAL "11") OR
     (CUDA_VERSION_MAJOR STREQUAL "12"))
  else()
    message(WARNING
      "CUDA version ${CUDA_VERSION_MAJOR}.${CUDA_VERSION_MINOR} detected, "
      "build may succeed but only CUDA 12, 11, 10.2 and 10.1 have been tested")
  endif()

  # build for each arch
  set(cuda_sources kernel.cu
    ${SRC_KERNEL_DEVICE_CUDA_HEADERS}
    $<TARGET_PROPERTY:cycles_kernel,INTERFACE_SOURCES>
  )
  set(cuda_cubins)

  macro(CYCLES_CUDA_KERNEL_ADD arch prev_arch name flags sources experimental)
    if(${arch} MATCHES "compute_.*")
      set(format "ptx")
    else()
      set(format "cubin")
    endif()
    set(cuda_file ${name}_${arch}.${format})
    set(cuda_file_compressed ${cuda_file}.zst)

    set(kernel_sources ${sources})
    if(NOT ${prev_arch} STREQUAL "none")
      if(${prev_arch} MATCHES "compute_.*")
        set(kernel_sources ${kernel_sources} ${name}_${prev_arch}.ptx)
      else()
        set(kernel_sources ${kernel_sources} ${name}_${prev_arch}.cubin)
      endif()
    endif()

    set(cuda_kernel_src "/${name}.cu")

    set(cuda_flags ${flags}
      -D CCL_NAMESPACE_BEGIN=
      -D CCL_NAMESPACE_END=
      -D NVCC
      -D _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
      -m ${CUDA_BITS}
      -I ${CMAKE_CURRENT_SOURCE_DIR}/../../..
      -o ${CMAKE_CURRENT_BINARY_DIR}/${cuda_file})

    cuda_add_common_flags(${cuda_version} ${arch} "${cuda_flags}" cuda_flags)

    set(_cuda_nvcc_args
      -arch=${arch}
      ${CUDA_NVCC_FLAGS}
      --${format}
      ${CMAKE_CURRENT_SOURCE_DIR}${cuda_kernel_src}
      ${cuda_flags}
    )

    if(WITH_COMPILER_CCACHE AND CCACHE_PROGRAM)
      add_custom_command(
        OUTPUT ${cuda_file}
        COMMAND ${CCACHE_PROGRAM} ${cuda_nvcc_executable} ${_cuda_nvcc_args}
        DEPENDS ${kernel_sources})
    else()
      add_custom_command(
        OUTPUT ${cuda_file}
        COMMAND ${cuda_nvcc_executable} ${_cuda_nvcc_args}
        DEPENDS ${kernel_sources})
    endif()

    add_custom_command(
      OUTPUT ${cuda_file_compressed}
      COMMAND "$<TARGET_FILE:zstd_compress>" ${cuda_file} ${cuda_file_compressed}
      DEPENDS ${cuda_file})

    unset(_cuda_nvcc_args)
    delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${cuda_file_compressed}" ${CYCLES_INSTALL_PATH}/lib)
    list(APPEND cuda_cubins ${cuda_file_compressed})

    unset(cuda_debug_flags)
  endmacro()

  set(prev_arch "none")
  foreach(arch ${CYCLES_CUDA_BINARIES_ARCH})
    if(${arch} MATCHES ".*_3.")
      message(STATUS "CUDA binaries for ${arch} are no longer supported, skipped.")
    elseif(${arch} MATCHES "compute_7." AND DEFINED CUDA11_NVCC_EXECUTABLE)
      # Use CUDA 11 if available for the default PTX kernel. This allows us to
      # keep the driver requirements for user machines low.
      set(cuda_nvcc_executable ${CUDA11_NVCC_EXECUTABLE})
      set(cuda_toolkit_root_dir ${CUDA11_TOOLKIT_ROOT_DIR})
      set(cuda_version 110)
    elseif((${arch} MATCHES ".*_5." OR ${arch} MATCHES ".*_6." OR ${arch} MATCHES ".*_70") AND "${CUDA_VERSION}" GREATER_EQUAL 130)
      # Support for Maxwell, Pascal and Volta was dropped in CUDA 13
      if(DEFINED CUDA11_NVCC_EXECUTABLE)
        set(cuda_nvcc_executable ${CUDA11_NVCC_EXECUTABLE})
        set(cuda_toolkit_root_dir ${CUDA11_TOOLKIT_ROOT_DIR})
        set(cuda_version 110)
      else()
        message(STATUS "CUDA binaries for ${arch} are no longer supported with CUDA 13.0+, skipped.")
      endif()
    elseif(${arch} MATCHES ".*_7." AND "${CUDA_VERSION}" LESS 100)
      message(STATUS "CUDA binaries for ${arch} require CUDA 10.0+, skipped.")
    elseif(${arch} MATCHES ".*_8.")
      if("${CUDA_VERSION}" GREATER_EQUAL 111) # Support for sm_86 was introduced in CUDA 11
        set(cuda_nvcc_executable ${CUDA_NVCC_EXECUTABLE})
        set(cuda_toolkit_root_dir ${CUDA_TOOLKIT_ROOT_DIR})
        set(cuda_version ${CUDA_VERSION})
      elseif(DEFINED CUDA11_NVCC_EXECUTABLE)
        set(cuda_nvcc_executable ${CUDA11_NVCC_EXECUTABLE})
        set(cuda_toolkit_root_dir ${CUDA11_TOOLKIT_ROOT_DIR})
        set(cuda_version 110)
      else()
        message(STATUS "CUDA binaries for ${arch} require CUDA 11.1+, skipped.")
      endif()
    elseif(${arch} MATCHES ".*_10." OR ${arch} MATCHES ".*_120")
      if("${CUDA_VERSION}" GREATER_EQUAL 128) # Support for sm_100, sm_101, sm_120 was introduced in CUDA 12.8
        set(cuda_nvcc_executable ${CUDA_NVCC_EXECUTABLE})
        set(cuda_toolkit_root_dir ${CUDA_TOOLKIT_ROOT_DIR})
        set(cuda_version ${CUDA_VERSION})
      else()
        message(STATUS "CUDA binaries for ${arch} require CUDA 12.8+, skipped.")
      endif()
    else()
      set(cuda_nvcc_executable ${CUDA_NVCC_EXECUTABLE})
      set(cuda_toolkit_root_dir ${CUDA_TOOLKIT_ROOT_DIR})
      set(cuda_version ${CUDA_VERSION})
    endif()
    if(DEFINED cuda_nvcc_executable AND DEFINED cuda_toolkit_root_dir)
      # Compile regular kernel
      cycles_cuda_kernel_add(${arch} ${prev_arch} kernel "" "${cuda_sources}" FALSE)

      if(WITH_CYCLES_CUDA_BUILD_SERIAL)
        set(prev_arch ${arch})
      endif()

      unset(cuda_nvcc_executable)
      unset(cuda_toolkit_root_dir)
    endif()
  endforeach()

  add_custom_target(cycles_kernel_cuda
    ALL
    DEPENDS ${cuda_cubins}
    SOURCES ${SRC_KERNEL_DEVICE_CUDA} ${SRC_KERNEL_DEVICE_CUDA_HEADERS}
  )
  cycles_set_solution_folder(cycles_kernel_cuda)

  source_group("device\\cuda" FILES ${SRC_KERNEL_DEVICE_CUDA} ${SRC_KERNEL_DEVICE_CUDA_HEADERS})

  add_dependencies(cycles_kernel cycles_kernel_cuda)
endif()

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_CUDA}" ${CYCLES_INSTALL_PATH}/source/kernel/device/cuda)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_CUDA_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/device/cuda)
