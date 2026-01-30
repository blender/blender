# SPDX-FileCopyrightText: 2011-2026 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ../../..
)

set(INC_SYS

)

set(SRC_KERNEL_DEVICE_ONEAPI
  kernel.cpp
)

set(SRC_KERNEL_DEVICE_ONEAPI_HEADERS
  compat.h
  context_begin.h
  context_end.h
  context_intersect_begin.h
  context_intersect_end.h
  globals.h
  kernel.h
  kernel_templates.h
  ../cpu/bvh.h
)

set(LIB

)

if(WITH_CYCLES_DEVICE_ONEAPI)
  if(WITH_CYCLES_ONEAPI_BINARIES)
    set(cycles_kernel_oneapi_lib_suffix "_aot")
  else()
    set(cycles_kernel_oneapi_lib_suffix "_jit")
  endif()

  if(WIN32)
    set(cycles_kernel_oneapi_lib ${CMAKE_CURRENT_BINARY_DIR}/cycles_kernel_oneapi${cycles_kernel_oneapi_lib_suffix}.dll)
    set(cycles_kernel_oneapi_linker_lib ${CMAKE_CURRENT_BINARY_DIR}/cycles_kernel_oneapi${cycles_kernel_oneapi_lib_suffix}.lib)
  else()
    set(cycles_kernel_oneapi_lib ${CMAKE_CURRENT_BINARY_DIR}/libcycles_kernel_oneapi${cycles_kernel_oneapi_lib_suffix}.so)
  endif()

  set(cycles_oneapi_kernel_sources
    ${SRC_KERNEL_DEVICE_ONEAPI}
    ${SRC_KERNEL_DEVICE_ONEAPI_HEADERS}
    $<TARGET_PROPERTY:cycles_kernel,INTERFACE_SOURCES>
  )

  set(SYCL_OFFLINE_COMPILER_PARALLEL_JOBS 1 CACHE STRING "Number of parallel compiler instances to use for device binaries compilation (expect ~8GB peak memory usage per instance).")
  mark_as_advanced(SYCL_OFFLINE_COMPILER_PARALLEL_JOBS)

  if(WITH_CYCLES_ONEAPI_BINARIES)
    message(STATUS "${SYCL_OFFLINE_COMPILER_PARALLEL_JOBS} instance(s) of oneAPI offline compiler will be used.")
  endif()
  set(sycl_compiler_flags
    ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_KERNEL_DEVICE_ONEAPI}
    -fsycl
    -fsycl-unnamed-lambda
    -fdelayed-template-parsing
    -fsycl-device-code-split=per_kernel
    -fsycl-max-parallel-link-jobs=${SYCL_OFFLINE_COMPILER_PARALLEL_JOBS}
    --offload-compress
    --offload-compression-level=19
    -shared
    -DWITH_ONEAPI
    -O2
    -ffast-math
    -D__KERNEL_LOCAL_ATOMIC_SORT__
    -o"${cycles_kernel_oneapi_lib}"
    -I"${CMAKE_CURRENT_SOURCE_DIR}/../../.."
  )
  # SYCL_CPP_FLAGS is a variable that the user can set to pass extra compiler options.
  if(DEFINED SYCL_CPP_FLAGS)
    list(APPEND sycl_compiler_flags ${SYCL_CPP_FLAGS})
  endif()

  # Set defaults for spir64 and spir64_gen options
  if(NOT DEFINED CYCLES_ONEAPI_SYCL_OPTIONS_spir64)
    set(CYCLES_ONEAPI_SYCL_OPTIONS_spir64 "-options '-cl-fast-relaxed-math -ze-intel-enable-auto-large-GRF-mode -ze-opt-regular-grf-kernel integrator_intersect -ze-opt-large-grf-kernel shade_surface -ze-opt-no-local-to-generic'")
  endif()
  if(NOT DEFINED CYCLES_ONEAPI_SYCL_OPTIONS_spir64_gen)
    set(CYCLES_ONEAPI_SYCL_OPTIONS_spir64_gen "${CYCLES_ONEAPI_SYCL_OPTIONS_spir64}" CACHE STRING "Extra build options for spir64_gen target")
    mark_as_advanced(CYCLES_ONEAPI_SYCL_OPTIONS_spir64_gen)
  endif()
  # Enable `zebin`, a graphics binary format with improved compatibility.
  string(PREPEND CYCLES_ONEAPI_SYCL_OPTIONS_spir64_gen "--format zebin ")

  # Host execution won't use GPU binaries, no need to compile them.
  if(WITH_CYCLES_ONEAPI_BINARIES)
    # Add the list of Intel devices to build binaries for.
    foreach(device ${CYCLES_ONEAPI_INTEL_BINARIES_ARCH})
      # Run `ocloc` ids to test if the device is supported.
      if(WIN32)
        execute_process(
          COMMAND ${OCLOC_INSTALL_DIR}/ocloc.exe ids ${device}
          RESULT_VARIABLE oclocids_ret
          OUTPUT_QUIET
          ERROR_QUIET
        )
      else()
        set(_ocloc_ld_library_path "${OCLOC_INSTALL_DIR}/lib")
        if(DEFINED IGC_INSTALL_DIR)
          string(APPEND _ocloc_ld_library_path ":${IGC_INSTALL_DIR}/lib")
        endif()
        execute_process(
          COMMAND ${CMAKE_COMMAND}
          -E env "LD_LIBRARY_PATH=${_ocloc_ld_library_path}"
          ${OCLOC_INSTALL_DIR}/bin/ocloc ids ${device}

          RESULT_VARIABLE oclocids_ret
          OUTPUT_QUIET
          ERROR_QUIET
        )
        unset(_ocloc_ld_library_path)
      endif()
      if(NOT oclocids_ret EQUAL 0)
        list(REMOVE_ITEM CYCLES_ONEAPI_INTEL_BINARIES_ARCH ${device})
        message(STATUS
          "Cycles oneAPI: "
          "binaries for ${device} not supported by Intel Graphics Compiler/ocloc, skipped."
        )
      endif()
    endforeach()
    list(JOIN CYCLES_ONEAPI_INTEL_BINARIES_ARCH "," gen_devices_string)
    if("${gen_devices_string}" STREQUAL "")
      # Don't compile spir64_gen if no device is targeted
      message(STATUS "Cycles oneAPI: skipping spir64_gen compilation as no devices are targeted.")
      list(REMOVE_ITEM CYCLES_ONEAPI_SYCL_TARGETS spir64_gen)
    else()
      string(PREPEND CYCLES_ONEAPI_SYCL_OPTIONS_spir64_gen "-device ${gen_devices_string} ")
    endif()
  else()
    list(REMOVE_ITEM CYCLES_ONEAPI_SYCL_TARGETS spir64_gen)
  endif()

  # Iterate over all targets and their options.
  list(JOIN CYCLES_ONEAPI_SYCL_TARGETS "," targets_string)
  list(APPEND sycl_compiler_flags -fsycl-targets=${targets_string})
  foreach(target ${CYCLES_ONEAPI_SYCL_TARGETS})
    if(DEFINED CYCLES_ONEAPI_SYCL_OPTIONS_${target})
      list(APPEND sycl_compiler_flags
        "-Xsycl-target-backend=${target} \"${CYCLES_ONEAPI_SYCL_OPTIONS_${target}}\""
      )
    endif()
  endforeach()

  if(WITH_NANOVDB)
    list(APPEND sycl_compiler_flags
      -DWITH_NANOVDB)
  endif()

  if(WITH_CYCLES_EMBREE AND EMBREE_SYCL_SUPPORT)
    list(APPEND sycl_compiler_flags
      -DWITH_EMBREE
      -DWITH_EMBREE_GPU
      -DEMBREE_MAJOR_VERSION=${EMBREE_MAJOR_VERSION}
      -I"${EMBREE_INCLUDE_DIRS}")

    if(WIN32)
      list(APPEND sycl_compiler_flags
        -ladvapi32.lib
      )
    endif()

    set(next_library_mode "")
    foreach(library ${EMBREE_LIBRARIES})
      string(TOLOWER "${library}" library_lower)
      if(("${library_lower}" STREQUAL "optimized") OR
         ("${library_lower}" STREQUAL "debug"))
        set(next_library_mode "${library_lower}")
      else()
        if(next_library_mode STREQUAL "")
          list(APPEND EMBREE_TBB_LIBRARIES_optimized ${library})
          list(APPEND EMBREE_TBB_LIBRARIES_debug ${library})
        else()
          list(APPEND EMBREE_TBB_LIBRARIES_${next_library_mode} ${library})
        endif()
        set(next_library_mode "")
      endif()
    endforeach()

    foreach(library ${TBB_LIBRARIES})
      string(TOLOWER "${library}" library_lower)
      if(("${library_lower}" STREQUAL "optimized") OR
         ("${library_lower}" STREQUAL "debug"))
        set(next_library_mode "${library_lower}")
      else()
        if(next_library_mode STREQUAL "")
          list(APPEND EMBREE_TBB_LIBRARIES_optimized ${library})
          list(APPEND EMBREE_TBB_LIBRARIES_debug ${library})
        else()
          list(APPEND EMBREE_TBB_LIBRARIES_${next_library_mode} ${library})
        endif()
        set(next_library_mode "")
      endif()
    endforeach()
    list(APPEND sycl_compiler_flags
      "$<$<CONFIG:Release>:${EMBREE_TBB_LIBRARIES_optimized}>"
      "$<$<CONFIG:RelWithDebInfo>:${EMBREE_TBB_LIBRARIES_optimized}>"
      "$<$<CONFIG:MinSizeRel>:${EMBREE_TBB_LIBRARIES_optimized}>"
      "$<$<CONFIG:Debug>:${EMBREE_TBB_LIBRARIES_debug}>"
    )
  endif()

  if(WITH_CYCLES_DEBUG)
    list(APPEND sycl_compiler_flags -DWITH_CYCLES_DEBUG)
  endif()

  get_filename_component(sycl_compiler_root ${SYCL_COMPILER} DIRECTORY)

  if(WIN32) # Add Windows specific compiler flags.
    list(APPEND sycl_compiler_flags
      -fms-extensions
      -fms-compatibility
      -D_WINDLL
      -D_MBCS
      -DWIN32
      -D_WINDOWS
      -D_CRT_NONSTDC_NO_DEPRECATE
      -D_CRT_SECURE_NO_DEPRECATE
      -DONEAPI_EXPORT
    )
  else() # Add Linux specific compiler flags.
    list(APPEND sycl_compiler_flags -fPIC)
    list(APPEND sycl_compiler_flags -fvisibility=hidden)

    # Add $ORIGIN to `cycles_kernel_oneapi.so` RPATH so `libsycl.so` and
    # `libpi_level_zero.so` can be placed next to it and get found.
    list(APPEND sycl_compiler_flags -Wl,-rpath,'$$ORIGIN')
  endif()

  # Create CONFIG specific compiler flags.
  set(sycl_compiler_flags_Release ${sycl_compiler_flags})
  set(sycl_compiler_flags_Debug ${sycl_compiler_flags})
  set(sycl_compiler_flags_RelWithDebInfo ${sycl_compiler_flags})

  list(APPEND sycl_compiler_flags_Release
    -DNDEBUG
  )
  list(APPEND sycl_compiler_flags_RelWithDebInfo
    -DNDEBUG
    -g
  )
  list(APPEND sycl_compiler_flags_Debug
    -g
  )

  if(WIN32)
    list(APPEND sycl_compiler_flags_Debug
      -D_DEBUG
      -nostdlib
      -Xclang --dependent-lib=msvcrtd
    )

    list(APPEND sycl_compiler_flags
      -L"${sycl_compiler_root}/../lib" # To find sycl.lib
      -L"${sycl_compiler_root}/../compiler/lib/intel64_win" # To find libircmt.lib (when using `icpx`)
    )

    add_custom_command(
      OUTPUT ${cycles_kernel_oneapi_lib} ${cycles_kernel_oneapi_linker_lib}
      COMMAND ${CMAKE_COMMAND} -E env
        "PATH=${OCLOC_INSTALL_DIR}\;${sycl_compiler_root}"
        ${SYCL_COMPILER}
        "$<$<CONFIG:Release>:${sycl_compiler_flags_Release}>"
        "$<$<CONFIG:RelWithDebInfo>:${sycl_compiler_flags_RelWithDebInfo}>"
        "$<$<CONFIG:Debug>:${sycl_compiler_flags_Debug}>"
        "$<$<CONFIG:MinSizeRel>:${sycl_compiler_flags_Release}>"
      COMMAND_EXPAND_LISTS
      DEPENDS ${cycles_oneapi_kernel_sources} ${SYCL_COMPILER})
  else()
    if(NOT IGC_INSTALL_DIR)
      get_filename_component(IGC_INSTALL_DIR "${sycl_compiler_root}/../lib/igc" ABSOLUTE)
    endif()
    # The following join/replace operations are to prevent cmake from
    # escaping space chars with backslashes in add_custom_command.
    list(JOIN sycl_compiler_flags_Release " " sycl_compiler_flags_Release_str)
    string(REPLACE " " ";" sycl_compiler_flags_Release_str ${sycl_compiler_flags_Release_str})
    list(JOIN sycl_compiler_flags_RelWithDebInfo " " sycl_compiler_flags_RelWithDebInfo_str)
    string(REPLACE " " ";" sycl_compiler_flags_RelWithDebInfo_str ${sycl_compiler_flags_RelWithDebInfo_str})
    list(JOIN sycl_compiler_flags_Debug " " sycl_compiler_flags_Debug_str)
    string(REPLACE " " ";" sycl_compiler_flags_Debug_str ${sycl_compiler_flags_Debug_str})
    add_custom_command(
      OUTPUT ${cycles_kernel_oneapi_lib}
      COMMAND
        ${CMAKE_COMMAND} -E env
        "LD_LIBRARY_PATH=${sycl_compiler_root}/../lib:${OCLOC_INSTALL_DIR}/lib:${IGC_INSTALL_DIR}/lib"
        # `$ENV{PATH}` is for compiler to find `ld`.
        "PATH=${OCLOC_INSTALL_DIR}/bin:${sycl_compiler_root}:$ENV{PATH}"
        ${SYCL_COMPILER}
        "$<$<CONFIG:Release>:${sycl_compiler_flags_Release_str}>"
        "$<$<CONFIG:RelWithDebInfo>:${sycl_compiler_flags_RelWithDebInfo_str}>"
        "$<$<CONFIG:Debug>:${sycl_compiler_flags_Debug_str}>"
        "$<$<CONFIG:MinSizeRel>:${sycl_compiler_flags_Release_str}>"
      COMMAND_EXPAND_LISTS
      DEPENDS ${cycles_oneapi_kernel_sources} ${SYCL_COMPILER})
  endif()

  # install dynamic libraries required at runtime
  delayed_install("" "${cycles_kernel_oneapi_lib}" ${cycles_kernel_runtime_lib_target_path})

  add_custom_target(cycles_kernel_oneapi
    ALL
    DEPENDS ${cycles_kernel_oneapi_lib}
    SOURCES ${SRC_KERNEL_DEVICE_ONEAPI} ${SRC_KERNEL_DEVICE_ONEAPI_HEADERS}
  )
  cycles_set_solution_folder(cycles_kernel_oneapi)

  source_group("device\\oneapi" FILES ${SRC_KERNEL_DEVICE_ONEAPI} ${SRC_KERNEL_DEVICE_ONEAPI_HEADERS})

  add_dependencies(cycles_kernel cycles_kernel_oneapi)
endif()
