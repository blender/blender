# SPDX-License-Identifier: Apache-2.0
# Copyright 2011-2022 Blender Foundation

remove_extra_strict_flags()

set(INC
  ..
)

set(INC_SYS

)

set(SRC_KERNEL_DEVICE_CPU
  device/cpu/kernel.cpp
  device/cpu/kernel_sse2.cpp
  device/cpu/kernel_sse3.cpp
  device/cpu/kernel_sse41.cpp
  device/cpu/kernel_avx.cpp
  device/cpu/kernel_avx2.cpp
)

set(SRC_KERNEL_DEVICE_CUDA
  device/cuda/kernel.cu
)

set(SRC_KERNEL_DEVICE_HIP
  device/hip/kernel.cpp
)

set(SRC_KERNEL_DEVICE_METAL
  device/metal/kernel.metal
)

set(SRC_KERNEL_DEVICE_OPTIX
  device/optix/kernel.cu
  device/optix/kernel_shader_raytrace.cu
)

if(WITH_CYCLES_OSL AND (OSL_LIBRARY_VERSION_MINOR GREATER_EQUAL 13 OR OSL_LIBRARY_VERSION_MAJOR GREATER 1))
  set(SRC_KERNEL_DEVICE_OPTIX
    ${SRC_KERNEL_DEVICE_OPTIX}
    osl/services_optix.cu
    device/optix/kernel_osl.cu
  )
endif()

set(SRC_KERNEL_DEVICE_ONEAPI
  device/oneapi/kernel.cpp
)

set(SRC_KERNEL_DEVICE_CPU_HEADERS
  device/cpu/bvh.h
  device/cpu/compat.h
  device/cpu/image.h
  device/cpu/globals.h
  device/cpu/kernel.h
  device/cpu/kernel_arch.h
  device/cpu/kernel_arch_impl.h
)
set(SRC_KERNEL_DEVICE_GPU_HEADERS
  device/gpu/image.h
  device/gpu/kernel.h
  device/gpu/parallel_active_index.h
  device/gpu/parallel_prefix_sum.h
  device/gpu/parallel_sorted_index.h
  device/gpu/work_stealing.h
)

set(SRC_KERNEL_DEVICE_CUDA_HEADERS
  device/cuda/compat.h
  device/cuda/config.h
  device/cuda/globals.h
)

set(SRC_KERNEL_DEVICE_HIP_HEADERS
  device/hip/compat.h
  device/hip/config.h
  device/hip/globals.h
)

set(SRC_KERNEL_DEVICE_OPTIX_HEADERS
  device/optix/bvh.h
  device/optix/compat.h
  device/optix/globals.h
)

set(SRC_KERNEL_DEVICE_METAL_HEADERS
  device/metal/bvh.h
  device/metal/compat.h
  device/metal/context_begin.h
  device/metal/context_end.h
  device/metal/function_constants.h
  device/metal/globals.h
)

set(SRC_KERNEL_DEVICE_ONEAPI_HEADERS
  device/oneapi/compat.h
  device/oneapi/context_begin.h
  device/oneapi/context_end.h
  device/oneapi/globals.h
  device/oneapi/image.h
  device/oneapi/kernel.h
  device/oneapi/kernel_templates.h
)

set(SRC_KERNEL_CLOSURE_HEADERS
  closure/alloc.h
  closure/bsdf.h
  closure/bsdf_ashikhmin_velvet.h
  closure/bsdf_diffuse.h
  closure/bsdf_diffuse_ramp.h
  closure/bsdf_microfacet.h
  closure/bsdf_microfacet_multi.h
  closure/bsdf_microfacet_multi_impl.h
  closure/bsdf_oren_nayar.h
  closure/bsdf_phong_ramp.h
  closure/bsdf_reflection.h
  closure/bsdf_refraction.h
  closure/bsdf_toon.h
  closure/bsdf_transparent.h
  closure/bsdf_util.h
  closure/bsdf_ashikhmin_shirley.h
  closure/bsdf_hair.h
  closure/bssrdf.h
  closure/emissive.h
  closure/volume.h
  closure/bsdf_principled_diffuse.h
  closure/bsdf_principled_sheen.h
  closure/bsdf_hair_principled.h
)

set(SRC_KERNEL_SVM_HEADERS
  svm/svm.h
  svm/ao.h
  svm/aov.h
  svm/attribute.h
  svm/bevel.h
  svm/blackbody.h
  svm/bump.h
  svm/camera.h
  svm/clamp.h
  svm/closure.h
  svm/convert.h
  svm/checker.h
  svm/color_util.h
  svm/brick.h
  svm/displace.h
  svm/fresnel.h
  svm/wireframe.h
  svm/wavelength.h
  svm/gamma.h
  svm/brightness.h
  svm/geometry.h
  svm/gradient.h
  svm/hsv.h
  svm/ies.h
  svm/image.h
  svm/invert.h
  svm/light_path.h
  svm/magic.h
  svm/map_range.h
  svm/mapping.h
  svm/mapping_util.h
  svm/math.h
  svm/math_util.h
  svm/mix.h
  svm/musgrave.h
  svm/node_types_template.h
  svm/noise.h
  svm/noisetex.h
  svm/normal.h
  svm/ramp.h
  svm/ramp_util.h
  svm/sepcomb_color.h
  svm/sepcomb_hsv.h
  svm/sepcomb_vector.h
  svm/sky.h
  svm/tex_coord.h
  svm/fractal_noise.h
  svm/types.h
  svm/value.h
  svm/vector_rotate.h
  svm/vector_transform.h
  svm/voronoi.h
  svm/voxel.h
  svm/wave.h
  svm/white_noise.h
  svm/vertex_color.h
)

if(WITH_CYCLES_OSL)
  set(SRC_KERNEL_OSL_HEADERS
    osl/osl.h
    osl/closures_setup.h
    osl/closures_template.h
    osl/services_gpu.h
    osl/types.h
  )
endif()

set(SRC_KERNEL_GEOM_HEADERS
  geom/geom.h
  geom/attribute.h
  geom/curve.h
  geom/curve_intersect.h
  geom/motion_curve.h
  geom/motion_point.h
  geom/motion_triangle.h
  geom/motion_triangle_intersect.h
  geom/motion_triangle_shader.h
  geom/object.h
  geom/patch.h
  geom/point.h
  geom/point_intersect.h
  geom/primitive.h
  geom/shader_data.h
  geom/subd_triangle.h
  geom/triangle.h
  geom/triangle_intersect.h
  geom/volume.h
)

set(SRC_KERNEL_BAKE_HEADERS
  bake/bake.h
)

set(SRC_KERNEL_BVH_HEADERS
  bvh/bvh.h
  bvh/nodes.h
  bvh/shadow_all.h
  bvh/local.h
  bvh/traversal.h
  bvh/types.h
  bvh/util.h
  bvh/volume.h
  bvh/volume_all.h
)

set(SRC_KERNEL_CAMERA_HEADERS
  camera/camera.h
  camera/projection.h
)

set(SRC_KERNEL_FILM_HEADERS
  film/adaptive_sampling.h
  film/aov_passes.h
  film/data_passes.h
  film/denoising_passes.h
  film/cryptomatte_passes.h
  film/light_passes.h
  film/read.h
  film/write.h
)

set(SRC_KERNEL_INTEGRATOR_HEADERS
  integrator/displacement_shader.h
  integrator/init_from_bake.h
  integrator/init_from_camera.h
  integrator/intersect_closest.h
  integrator/intersect_shadow.h
  integrator/intersect_subsurface.h
  integrator/intersect_volume_stack.h
  integrator/guiding.h
  integrator/megakernel.h
  integrator/mnee.h
  integrator/path_state.h
  integrator/shade_background.h
  integrator/shade_light.h
  integrator/shade_shadow.h
  integrator/shade_surface.h
  integrator/shade_volume.h
  integrator/shadow_catcher.h
  integrator/shadow_state_template.h
  integrator/state_flow.h
  integrator/state.h
  integrator/state_template.h
  integrator/state_util.h
  integrator/subsurface_disk.h
  integrator/subsurface.h
  integrator/subsurface_random_walk.h
  integrator/surface_shader.h
  integrator/volume_shader.h
  integrator/volume_stack.h
)

set(SRC_KERNEL_LIGHT_HEADERS
  light/area.h
  light/background.h
  light/common.h
  light/distant.h
  light/distribution.h
  light/light.h
  light/point.h
  light/sample.h
  light/spot.h
  light/tree.h
  light/triangle.h
)

set(SRC_KERNEL_SAMPLE_HEADERS
  sample/lcg.h
  sample/mapping.h
  sample/mis.h
  sample/pattern.h
  sample/sobol_burley.h
  sample/tabulated_sobol.h
  sample/util.h
)

set(SRC_KERNEL_UTIL_HEADERS
  util/color.h
  util/differential.h
  util/lookup_table.h
  util/profiling.h
)

set(SRC_KERNEL_TYPES_HEADERS
  data_arrays.h
  data_template.h
  tables.h
  types.h
)

set(SRC_KERNEL_HEADERS
  ${SRC_KERNEL_BAKE_HEADERS}
  ${SRC_KERNEL_BVH_HEADERS}
  ${SRC_KERNEL_CAMERA_HEADERS}
  ${SRC_KERNEL_CLOSURE_HEADERS}
  ${SRC_KERNEL_FILM_HEADERS}
  ${SRC_KERNEL_GEOM_HEADERS}
  ${SRC_KERNEL_INTEGRATOR_HEADERS}
  ${SRC_KERNEL_LIGHT_HEADERS}
  ${SRC_KERNEL_OSL_HEADERS}
  ${SRC_KERNEL_SAMPLE_HEADERS}
  ${SRC_KERNEL_SVM_HEADERS}
  ${SRC_KERNEL_TYPES_HEADERS}
  ${SRC_KERNEL_UTIL_HEADERS}
)

set(SRC_UTIL_HEADERS
  ../util/atomic.h
  ../util/color.h
  ../util/defines.h
  ../util/half.h
  ../util/hash.h
  ../util/math.h
  ../util/math_fast.h
  ../util/math_intersect.h
  ../util/math_float2.h
  ../util/math_float3.h
  ../util/math_float4.h
  ../util/math_float8.h
  ../util/math_int2.h
  ../util/math_int3.h
  ../util/math_int4.h
  ../util/math_int8.h
  ../util/math_matrix.h
  ../util/projection.h
  ../util/rect.h
  ../util/static_assert.h
  ../util/transform.h
  ../util/transform_inverse.h
  ../util/texture.h
  ../util/types.h
  ../util/types_float2.h
  ../util/types_float2_impl.h
  ../util/types_float3.h
  ../util/types_float3_impl.h
  ../util/types_float4.h
  ../util/types_float4_impl.h
  ../util/types_float8.h
  ../util/types_float8_impl.h
  ../util/types_int2.h
  ../util/types_int2_impl.h
  ../util/types_int3.h
  ../util/types_int3_impl.h
  ../util/types_int4.h
  ../util/types_int4_impl.h
  ../util/types_int8.h
  ../util/types_int8_impl.h
  ../util/types_spectrum.h
  ../util/types_uchar2.h
  ../util/types_uchar2_impl.h
  ../util/types_uchar3.h
  ../util/types_uchar3_impl.h
  ../util/types_uchar4.h
  ../util/types_uchar4_impl.h
  ../util/types_uint2.h
  ../util/types_uint2_impl.h
  ../util/types_uint3.h
  ../util/types_uint3_impl.h
  ../util/types_uint4.h
  ../util/types_uint4_impl.h
  ../util/types_ushort4.h
)

set(LIB

)

# CUDA module

if(WITH_CYCLES_CUDA_BINARIES)
  # 64 bit only
  set(CUDA_BITS 64)

  # CUDA version
  execute_process(COMMAND ${CUDA_NVCC_EXECUTABLE} "--version" OUTPUT_VARIABLE NVCC_OUT)
  string(REGEX REPLACE ".*release ([0-9]+)\\.([0-9]+).*" "\\1" CUDA_VERSION_MAJOR "${NVCC_OUT}")
  string(REGEX REPLACE ".*release ([0-9]+)\\.([0-9]+).*" "\\2" CUDA_VERSION_MINOR "${NVCC_OUT}")
  set(CUDA_VERSION "${CUDA_VERSION_MAJOR}${CUDA_VERSION_MINOR}")

  # warn for other versions
  if((CUDA_VERSION STREQUAL "101") OR
     (CUDA_VERSION STREQUAL "102") OR
     (CUDA_VERSION_MAJOR STREQUAL "11"))
  else()
    message(WARNING
      "CUDA version ${CUDA_VERSION_MAJOR}.${CUDA_VERSION_MINOR} detected, "
      "build may succeed but only CUDA 11, 10.2 and 10.1 have been tested")
  endif()

  # build for each arch
  set(cuda_sources device/cuda/kernel.cu
    ${SRC_KERNEL_HEADERS}
    ${SRC_KERNEL_DEVICE_GPU_HEADERS}
    ${SRC_KERNEL_DEVICE_CUDA_HEADERS}
    ${SRC_UTIL_HEADERS}
  )
  set(cuda_cubins)

  macro(CYCLES_CUDA_KERNEL_ADD arch prev_arch name flags sources experimental)
    if(${arch} MATCHES "compute_.*")
      set(format "ptx")
    else()
      set(format "cubin")
    endif()
    set(cuda_file ${name}_${arch}.${format})

    set(kernel_sources ${sources})
    if(NOT ${prev_arch} STREQUAL "none")
      if(${prev_arch} MATCHES "compute_.*")
        set(kernel_sources ${kernel_sources} ${name}_${prev_arch}.ptx)
      else()
        set(kernel_sources ${kernel_sources} ${name}_${prev_arch}.cubin)
      endif()
    endif()

    set(cuda_kernel_src "/device/cuda/${name}.cu")

    set(cuda_flags ${flags}
      -D CCL_NAMESPACE_BEGIN=
      -D CCL_NAMESPACE_END=
      -D NVCC
      -m ${CUDA_BITS}
      -I ${CMAKE_CURRENT_SOURCE_DIR}/..
      -I ${CMAKE_CURRENT_SOURCE_DIR}/device/cuda
      --use_fast_math
      -o ${CMAKE_CURRENT_BINARY_DIR}/${cuda_file}
      -Wno-deprecated-gpu-targets)

    if(CUDA_HOST_COMPILER)
      set(cuda_flags ${cuda_flags}
        -ccbin="${CUDA_HOST_COMPILER}")
    endif()

    if(WITH_NANOVDB)
      set(cuda_flags ${cuda_flags}
        -D WITH_NANOVDB
        -I "${NANOVDB_INCLUDE_DIR}")
    endif()

    if(WITH_CYCLES_DEBUG)
      set(cuda_flags ${cuda_flags} -D WITH_CYCLES_DEBUG)
      set(cuda_flags ${cuda_flags} --ptxas-options="-v")
    endif()

    set(_cuda_nvcc_args
          -arch=${arch}
          ${CUDA_NVCC_FLAGS}
          --${format}
          ${CMAKE_CURRENT_SOURCE_DIR}${cuda_kernel_src}
          ${cuda_flags})

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

    unset(_cuda_nvcc_args)
    delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${cuda_file}" ${CYCLES_INSTALL_PATH}/lib)
    list(APPEND cuda_cubins ${cuda_file})

    unset(cuda_debug_flags)
  endmacro()

  set(prev_arch "none")
  foreach(arch ${CYCLES_CUDA_BINARIES_ARCH})
    if(${arch} MATCHES ".*_2.")
      message(STATUS "CUDA binaries for ${arch} are no longer supported, skipped.")
    elseif(${arch} MATCHES ".*_30")
      if(DEFINED CUDA10_NVCC_EXECUTABLE)
        set(cuda_nvcc_executable ${CUDA10_NVCC_EXECUTABLE})
        set(cuda_toolkit_root_dir ${CUDA10_TOOLKIT_ROOT_DIR})
      elseif("${CUDA_VERSION}" LESS 110) # Support for sm_30 was removed in CUDA 11
        set(cuda_nvcc_executable ${CUDA_NVCC_EXECUTABLE})
        set(cuda_toolkit_root_dir ${CUDA_TOOLKIT_ROOT_DIR})
      else()
        message(STATUS "CUDA binaries for ${arch} require CUDA 10 or earlier, skipped.")
      endif()
    elseif(${arch} MATCHES ".*_7." AND "${CUDA_VERSION}" LESS 100)
      message(STATUS "CUDA binaries for ${arch} require CUDA 10.0+, skipped.")
    elseif(${arch} MATCHES ".*_8.")
      if(DEFINED CUDA11_NVCC_EXECUTABLE)
        set(cuda_nvcc_executable ${CUDA11_NVCC_EXECUTABLE})
        set(cuda_toolkit_root_dir ${CUDA11_TOOLKIT_ROOT_DIR})
      elseif("${CUDA_VERSION}" GREATER_EQUAL 111) # Support for sm_86 was introduced in CUDA 11
        set(cuda_nvcc_executable ${CUDA_NVCC_EXECUTABLE})
        set(cuda_toolkit_root_dir ${CUDA_TOOLKIT_ROOT_DIR})
      else()
        message(STATUS "CUDA binaries for ${arch} require CUDA 11.1+, skipped.")
      endif()
    else()
      set(cuda_nvcc_executable ${CUDA_NVCC_EXECUTABLE})
      set(cuda_toolkit_root_dir ${CUDA_TOOLKIT_ROOT_DIR})
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

  add_custom_target(cycles_kernel_cuda ALL DEPENDS ${cuda_cubins})
  cycles_set_solution_folder(cycles_kernel_cuda)
endif()

# HIP module

if(WITH_CYCLES_HIP_BINARIES AND WITH_CYCLES_DEVICE_HIP)
  # build for each arch
  set(hip_sources device/hip/kernel.cpp
    ${SRC_KERNEL_HEADERS}
    ${SRC_KERNEL_DEVICE_GPU_HEADERS}
    ${SRC_KERNEL_DEVICE_HIP_HEADERS}
    ${SRC_UTIL_HEADERS}
  )
  set(hip_fatbins)

  macro(CYCLES_HIP_KERNEL_ADD arch name flags sources experimental)
    set(format "fatbin")
    set(hip_file ${name}_${arch}.${format})
    set(kernel_sources ${sources})

    set(hip_kernel_src "/device/hip/${name}.cpp")

    if(WIN32)
      set(hip_command ${CMAKE_COMMAND})
      set(hip_flags
        -E env "HIP_PATH=${HIP_ROOT_DIR}"
        ${HIP_HIPCC_EXECUTABLE}.bat)
    else()
      set(hip_command ${HIP_HIPCC_EXECUTABLE})
      set(hip_flags)
    endif()

    # There's a bug in the compiler causing some scenes to fail to render on Vega cards
    # A workaround currently is to set -O1 opt level during kernel compilation for these
    # cards Remove this when a newer compiler is available with fixes.
    if(WIN32 AND (${arch} MATCHES "gfx90[a-z0-9]+"))
      set(hip_opt_flags "-O1")
    else()
      set(hip_opt_flags)
    endif()

    set(hip_flags
      ${hip_flags}
      --amdgpu-target=${arch}
      ${HIP_HIPCC_FLAGS}
      --genco
      ${CMAKE_CURRENT_SOURCE_DIR}${hip_kernel_src}
      ${flags}
      -D CCL_NAMESPACE_BEGIN=
      -D CCL_NAMESPACE_END=
      -D HIPCC
      -I ${CMAKE_CURRENT_SOURCE_DIR}/..
      -I ${CMAKE_CURRENT_SOURCE_DIR}/device/hip
      -Wno-parentheses-equality
      -Wno-unused-value
      --hipcc-func-supp
      -ffast-math
      ${hip_opt_flags}
      -o ${CMAKE_CURRENT_BINARY_DIR}/${hip_file})

    if(WITH_NANOVDB)
      set(hip_flags ${hip_flags}
        -D WITH_NANOVDB
        -I "${NANOVDB_INCLUDE_DIR}")
    endif()

    if(WITH_CYCLES_DEBUG)
      set(hip_flags ${hip_flags} -D WITH_CYCLES_DEBUG)
    endif()

    add_custom_command(
      OUTPUT ${hip_file}
      COMMAND ${hip_command} ${hip_flags}
      DEPENDS ${kernel_sources})
    delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${hip_file}" ${CYCLES_INSTALL_PATH}/lib)
    list(APPEND hip_fatbins ${hip_file})
  endmacro()

  foreach(arch ${CYCLES_HIP_BINARIES_ARCH})
    # Compile regular kernel
    cycles_hip_kernel_add(${arch} kernel "" "${hip_sources}" FALSE)
  endforeach()

  add_custom_target(cycles_kernel_hip ALL DEPENDS ${hip_fatbins})
  cycles_set_solution_folder(cycles_kernel_hip)
endif()

# OptiX PTX modules

if(WITH_CYCLES_DEVICE_OPTIX AND WITH_CYCLES_CUDA_BINARIES)
  macro(CYCLES_OPTIX_KERNEL_ADD name input flags)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${name}.ptx")

    set(cuda_flags ${flags}
      -I "${OPTIX_INCLUDE_DIR}"
      -I "${CMAKE_CURRENT_SOURCE_DIR}/.."
      -I "${CMAKE_CURRENT_SOURCE_DIR}/device/cuda"
      --use_fast_math
      -Wno-deprecated-gpu-targets
      -o ${output})

    if(WITH_NANOVDB)
      set(cuda_flags ${cuda_flags}
        -D WITH_NANOVDB
        -I "${NANOVDB_INCLUDE_DIR}")
    endif()

    if(WITH_CYCLES_DEBUG)
      set(cuda_flags ${cuda_flags} -D WITH_CYCLES_DEBUG)
    endif()

    add_custom_command(
      OUTPUT
        ${output}
      DEPENDS
        ${input}
        ${SRC_KERNEL_HEADERS}
        ${SRC_KERNEL_DEVICE_GPU_HEADERS}
        ${SRC_KERNEL_DEVICE_CUDA_HEADERS}
        ${SRC_KERNEL_DEVICE_OPTIX_HEADERS}
        ${SRC_UTIL_HEADERS}
      COMMAND
        ${CUDA_NVCC_EXECUTABLE}
        --ptx
        -arch=sm_50
        ${cuda_flags}
        ${input}
      WORKING_DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}")

    list(APPEND optix_ptx ${output})

    delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${output}" ${CYCLES_INSTALL_PATH}/lib)
  endmacro()

  cycles_optix_kernel_add(
    kernel_optix
    "device/optix/kernel.cu"
    "")
  cycles_optix_kernel_add(
    kernel_optix_shader_raytrace
    "device/optix/kernel_shader_raytrace.cu"
    "--keep-device-functions")
  if(WITH_CYCLES_OSL AND (OSL_LIBRARY_VERSION_MINOR GREATER_EQUAL 13 OR OSL_LIBRARY_VERSION_MAJOR GREATER 1))
    CYCLES_OPTIX_KERNEL_ADD(
      kernel_optix_osl
      "device/optix/kernel_osl.cu"
      "--relocatable-device-code=true")
    CYCLES_OPTIX_KERNEL_ADD(
      kernel_optix_osl_services
      "osl/services_optix.cu"
      "--relocatable-device-code=true")
  endif()

  add_custom_target(cycles_kernel_optix ALL DEPENDS ${optix_ptx})
  cycles_set_solution_folder(cycles_kernel_optix)
endif()

# oneAPI module

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
    ${SRC_KERNEL_HEADERS}
    ${SRC_KERNEL_DEVICE_GPU_HEADERS}
    ${SRC_KERNEL_DEVICE_ONEAPI_HEADERS}
    ${SRC_UTIL_HEADERS}
  )

  set (SYCL_OFFLINE_COMPILER_PARALLEL_JOBS 1 CACHE STRING "Number of parallel compiler instances to use for device binaries compilation (expect ~8GB peak memory usage per instance).")
  if (WITH_CYCLES_ONEAPI_BINARIES)
    message(STATUS "${SYCL_OFFLINE_COMPILER_PARALLEL_JOBS} instance(s) of oneAPI offline compiler will be used.")
  endif()
  # SYCL_CPP_FLAGS is a variable that the user can set to pass extra compiler options
  set(sycl_compiler_flags
      ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_KERNEL_DEVICE_ONEAPI}
      -fsycl
      -fsycl-unnamed-lambda
      -fdelayed-template-parsing
      -mllvm -inlinedefault-threshold=250
      -mllvm -inlinehint-threshold=350
      -fsycl-device-code-split=per_kernel
      -fsycl-max-parallel-link-jobs=${SYCL_OFFLINE_COMPILER_PARALLEL_JOBS}
      -shared
      -DWITH_ONEAPI
      -ffast-math
      -DNDEBUG
      -O2
      -o ${cycles_kernel_oneapi_lib}
      -I${CMAKE_CURRENT_SOURCE_DIR}/..
      ${SYCL_CPP_FLAGS}
      )

  # Set defaults for spir64 and spir64_gen options
  if (NOT DEFINED CYCLES_ONEAPI_SYCL_OPTIONS_spir64)
    set(CYCLES_ONEAPI_SYCL_OPTIONS_spir64 "-options '-ze-opt-large-register-file -ze-opt-regular-grf-kernel integrator_intersect'")
  endif()
  if (NOT DEFINED CYCLES_ONEAPI_SYCL_OPTIONS_spir64_gen)
    SET (CYCLES_ONEAPI_SYCL_OPTIONS_spir64_gen "${CYCLES_ONEAPI_SYCL_OPTIONS_spir64}" CACHE STRING "Extra build options for spir64_gen target")
  endif()
  # Enable zebin, a graphics binary format with improved compatibility.
  string(PREPEND CYCLES_ONEAPI_SYCL_OPTIONS_spir64_gen "--format zebin ")
  string(PREPEND CYCLES_ONEAPI_SYCL_OPTIONS_spir64_gen "-device ${CYCLES_ONEAPI_SPIR64_GEN_DEVICES} ")

  if (WITH_CYCLES_ONEAPI_BINARIES)
    # AoT binaries aren't currently reused when calling sycl::build.
    list (APPEND sycl_compiler_flags -DSYCL_SKIP_KERNELS_PRELOAD)
    # Iterate over all targest and their options
    list (JOIN CYCLES_ONEAPI_SYCL_TARGETS "," targets_string)
    list (APPEND sycl_compiler_flags -fsycl-targets=${targets_string})
    foreach(target ${CYCLES_ONEAPI_SYCL_TARGETS})
      if(DEFINED CYCLES_ONEAPI_SYCL_OPTIONS_${target})
        list (APPEND sycl_compiler_flags -Xsycl-target-backend=${target} "${CYCLES_ONEAPI_SYCL_OPTIONS_${target}}")
      endif()
    endforeach()
  else()
    # If AOT is disabled, build for spir64
    list(APPEND sycl_compiler_flags
      -fsycl-targets=spir64
      -Xsycl-target-backend=spir64 "${CYCLES_ONEAPI_SYCL_OPTIONS_spir64}")
  endif()

  if(WITH_NANOVDB)
    list(APPEND sycl_compiler_flags
      -DWITH_NANOVDB
      -I"${NANOVDB_INCLUDE_DIR}")
  endif()

  if(WITH_CYCLES_DEBUG)
    list(APPEND sycl_compiler_flags -DWITH_CYCLES_DEBUG)
  endif()

  get_filename_component(sycl_compiler_root ${SYCL_COMPILER} DIRECTORY)
  get_filename_component(sycl_compiler_compiler_name ${SYCL_COMPILER} NAME_WE)

  if(UNIX AND NOT APPLE)
    if(NOT WITH_CXX11_ABI)
      check_library_exists(sycl
        _ZN4sycl3_V17handler22verifyUsedKernelBundleERKSs ${sycl_compiler_root}/../lib SYCL_NO_CXX11_ABI)
      if(SYCL_NO_CXX11_ABI)
        list(APPEND sycl_compiler_flags -D_GLIBCXX_USE_CXX11_ABI=0)
      endif()
    endif()
  endif()

  if(WIN32)
    list(APPEND sycl_compiler_flags
    -fuse-ld=link
    -fms-extensions
    -fms-compatibility
    -D_WINDLL
    -D_MBCS
    -DWIN32
    -D_WINDOWS
    -D_CRT_NONSTDC_NO_DEPRECATE
    -D_CRT_SECURE_NO_DEPRECATE
    -DONEAPI_EXPORT)

    string(REPLACE /Redist/ /Tools/ MSVC_TOOLS_DIR ${MSVC_REDIST_DIR})
    # Version Folder between Redist and Tools can mismatch sometimes
    if(NOT EXISTS ${MSVC_TOOLS_DIR})
      get_filename_component(cmake_ar_dir ${CMAKE_AR} DIRECTORY)
      get_filename_component(MSVC_TOOLS_DIR "${cmake_ar_dir}/../../../" ABSOLUTE)
    endif()
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
      set(WINDOWS_KIT_DIR ${WINDOWS_KITS_DIR}/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION})
    else() # case for Ninja on Windows
      get_filename_component(cmake_mt_dir ${CMAKE_MT} DIRECTORY)
      string(REPLACE /bin/ /Lib/ WINDOWS_KIT_DIR ${cmake_mt_dir})
      get_filename_component(WINDOWS_KIT_DIR "${WINDOWS_KIT_DIR}/../" ABSOLUTE)
    endif()
    list(APPEND sycl_compiler_flags
                -L "${MSVC_TOOLS_DIR}/lib/x64"
                -L "${WINDOWS_KIT_DIR}/um/x64"
                -L "${WINDOWS_KIT_DIR}/ucrt/x64")

    set(sycl_compiler_flags_Release ${sycl_compiler_flags})
    set(sycl_compiler_flags_Debug ${sycl_compiler_flags})
    set(sycl_compiler_flags_RelWithDebInfo ${sycl_compiler_flags})
    set(sycl_compiler_flags_MinSizeRel ${sycl_compiler_flags})
    list(APPEND sycl_compiler_flags_RelWithDebInfo -g)
    list(APPEND sycl_compiler_flags_Debug
                -g
                -D_DEBUG
                -nostdlib -Xclang --dependent-lib=msvcrtd)

    add_custom_command(
      OUTPUT ${cycles_kernel_oneapi_lib} ${cycles_kernel_oneapi_linker_lib}
      COMMAND ${CMAKE_COMMAND} -E env
              "LIB=${sycl_compiler_root}/../lib" # for compiler to find sycl.lib
              "PATH=${OCLOC_INSTALL_DIR}\;${sycl_compiler_root}"
              ${SYCL_COMPILER}
              "$<$<CONFIG:Release>:${sycl_compiler_flags_Release}>"
              "$<$<CONFIG:RelWithDebInfo>:${sycl_compiler_flags_RelWithDebInfo}>"
              "$<$<CONFIG:Debug>:${sycl_compiler_flags_Debug}>"
              "$<$<CONFIG:MinSizeRel>:${sycl_compiler_flags_Release}>"
              COMMAND_EXPAND_LISTS
              DEPENDS ${cycles_oneapi_kernel_sources})
  else()
    list(APPEND sycl_compiler_flags -fPIC)

    # We avoid getting __FAST_MATH__ to be defined when building on CentOS-7 until the compilation
    # crash it triggers at either AoT or JIT stages gets fixed.
    # TODO: check if this is still needed on Rocky-8.
    list(APPEND sycl_compiler_flags -fhonor-nans)

    # add $ORIGIN to cycles_kernel_oneapi.so rpath so libsycl.so and
    # libpi_level_zero.so can be placed next to it and get found.
    list(APPEND sycl_compiler_flags -Wl,-rpath,'$$ORIGIN')

    if(NOT IGC_INSTALL_DIR)
      get_filename_component(IGC_INSTALL_DIR "${sycl_compiler_root}/../lib/igc" ABSOLUTE)
    endif()
    add_custom_command(
      OUTPUT ${cycles_kernel_oneapi_lib}
      COMMAND ${CMAKE_COMMAND} -E env
              "LD_LIBRARY_PATH=${sycl_compiler_root}/../lib:${OCLOC_INSTALL_DIR}/lib:${IGC_INSTALL_DIR}/lib"
              # `$ENV{PATH}` is for compiler to find `ld`.
              "PATH=${OCLOC_INSTALL_DIR}/bin:${sycl_compiler_root}:$ENV{PATH}"
              ${SYCL_COMPILER} $<$<CONFIG:Debug>:-g>$<$<CONFIG:RelWithDebInfo>:-g> ${sycl_compiler_flags}
      DEPENDS ${cycles_oneapi_kernel_sources})
  endif()

  if(NOT WITH_BLENDER)
    # For the Cycles standalone put libraries next to the Cycles application.
    set(cycles_oneapi_target_path ${CYCLES_INSTALL_PATH})
  else()
    # For Blender put the libraries next to the Blender executable.
    #
    # Note that the installation path in the delayed_install is relative to the versioned folder,
    # which means we need to go one level up.
    set(cycles_oneapi_target_path "../")
  endif()

  # install dynamic libraries required at runtime
  if(WIN32)
    delayed_install("" "${cycles_kernel_oneapi_lib}" ${cycles_oneapi_target_path})
  elseif(UNIX AND NOT APPLE)
    delayed_install("" "${cycles_kernel_oneapi_lib}" ${cycles_oneapi_target_path}/lib)
  endif()

  add_custom_target(cycles_kernel_oneapi ALL DEPENDS ${cycles_kernel_oneapi_lib})
endif()

# OSL module

if(WITH_CYCLES_OSL)
  list(APPEND LIB
    cycles_kernel_osl
  )
  add_subdirectory(osl)
  add_subdirectory(osl/shaders)
endif()

# CPU module

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

if(WITH_COMPILER_ASAN)
  if(CMAKE_COMPILER_IS_GNUCC AND (NOT WITH_CYCLES_KERNEL_ASAN))
    # GCC hangs compiling the big kernel files with asan and release, so disable by default.
    string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " -fno-sanitize=all")
    string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fno-sanitize=vptr")
  elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    # With OSL, Cycles disables rtti in some modules, which then breaks at linking
    # when trying to use vptr sanitizer (included into 'undefined' general option).
    string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " -fno-sanitize=vptr")
    string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fno-sanitize=vptr")
  endif()
endif()

set_source_files_properties(device/cpu/kernel.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_KERNEL_FLAGS}")

if(CXX_HAS_SSE)
  set_source_files_properties(device/cpu/kernel_sse2.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE2_KERNEL_FLAGS}")
  set_source_files_properties(device/cpu/kernel_sse3.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE3_KERNEL_FLAGS}")
  set_source_files_properties(device/cpu/kernel_sse41.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE41_KERNEL_FLAGS}")
endif()

if(CXX_HAS_AVX)
  set_source_files_properties(device/cpu/kernel_avx.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX_KERNEL_FLAGS}")
endif()

if(CXX_HAS_AVX2)
  set_source_files_properties(device/cpu/kernel_avx2.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX2_KERNEL_FLAGS}")
endif()

# Warnings to avoid using doubles in the kernel.
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_check_cxx_compiler_flag(CMAKE_CXX_FLAGS _has_cxxflag_float_conversion "-Werror=float-conversion")
  add_check_cxx_compiler_flag(CMAKE_CXX_FLAGS _has_cxxflag_double_promotion "-Werror=double-promotion")
  unset(_has_cxxflag_float_conversion)
  unset(_has_cxxflag_double_promotion)
endif()

cycles_add_library(cycles_kernel "${LIB}"
  ${SRC_KERNEL_DEVICE_CPU}
  ${SRC_KERNEL_DEVICE_CUDA}
  ${SRC_KERNEL_DEVICE_HIP}
  ${SRC_KERNEL_DEVICE_OPTIX}
  ${SRC_KERNEL_DEVICE_METAL}
  ${SRC_KERNEL_HEADERS}
  ${SRC_KERNEL_DEVICE_CPU_HEADERS}
  ${SRC_KERNEL_DEVICE_GPU_HEADERS}
  ${SRC_KERNEL_DEVICE_CUDA_HEADERS}
  ${SRC_KERNEL_DEVICE_HIP_HEADERS}
  ${SRC_KERNEL_DEVICE_OPTIX_HEADERS}
  ${SRC_KERNEL_DEVICE_METAL_HEADERS}
  ${SRC_KERNEL_DEVICE_ONEAPI_HEADERS}
)

source_group("bake" FILES ${SRC_KERNEL_BAKE_HEADERS})
source_group("bvh" FILES ${SRC_KERNEL_BVH_HEADERS})
source_group("camera" FILES ${SRC_KERNEL_CAMERA_HEADERS})
source_group("closure" FILES ${SRC_KERNEL_CLOSURE_HEADERS})
source_group("device\\cpu" FILES ${SRC_KERNEL_DEVICE_CPU} ${SRC_KERNEL_DEVICE_CPU_HEADERS})
source_group("device\\cuda" FILES ${SRC_KERNEL_DEVICE_CUDA} ${SRC_KERNEL_DEVICE_CUDA_HEADERS})
source_group("device\\gpu" FILES ${SRC_KERNEL_DEVICE_GPU_HEADERS})
source_group("device\\hip" FILES ${SRC_KERNEL_DEVICE_HIP} ${SRC_KERNEL_DEVICE_HIP_HEADERS})
source_group("device\\optix" FILES ${SRC_KERNEL_DEVICE_OPTIX} ${SRC_KERNEL_DEVICE_OPTIX_HEADERS})
source_group("device\\metal" FILES ${SRC_KERNEL_DEVICE_METAL} ${SRC_KERNEL_DEVICE_METAL_HEADERS})
source_group("device\\oneapi" FILES ${SRC_KERNEL_DEVICE_ONEAPI} ${SRC_KERNEL_DEVICE_ONEAPI_HEADERS})
source_group("film" FILES ${SRC_KERNEL_FILM_HEADERS})
source_group("geom" FILES ${SRC_KERNEL_GEOM_HEADERS})
source_group("integrator" FILES ${SRC_KERNEL_INTEGRATOR_HEADERS})
source_group("kernel" FILES ${SRC_KERNEL_TYPES_HEADERS})
source_group("light" FILES ${SRC_KERNEL_LIGHT_HEADERS})
source_group("osl" FILES ${SRC_KERNEL_OSL_HEADERS})
source_group("sample" FILES ${SRC_KERNEL_SAMPLE_HEADERS})
source_group("svm" FILES ${SRC_KERNEL_SVM_HEADERS})
source_group("util" FILES ${SRC_KERNEL_UTIL_HEADERS})

if(WITH_CYCLES_CUDA)
  add_dependencies(cycles_kernel cycles_kernel_cuda)
endif()
if(WITH_CYCLES_DEVICE_OPTIX AND WITH_CYCLES_CUDA_BINARIES)
  add_dependencies(cycles_kernel cycles_kernel_optix)
endif()
if(WITH_CYCLES_HIP)
  add_dependencies(cycles_kernel cycles_kernel_hip)
endif()
if(WITH_CYCLES_DEVICE_ONEAPI)
  add_dependencies(cycles_kernel cycles_kernel_oneapi)
endif()

# Install kernel source for runtime compilation

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_BAKE_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/bake)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_BVH_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/bvh)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_CAMERA_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/camera)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_CLOSURE_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/closure)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_CUDA}" ${CYCLES_INSTALL_PATH}/source/kernel/device/cuda)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_CUDA_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/device/cuda)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_GPU_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/device/gpu)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_HIP}" ${CYCLES_INSTALL_PATH}/source/kernel/device/hip)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_HIP_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/device/hip)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_OPTIX}" ${CYCLES_INSTALL_PATH}/source/kernel/device/optix)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_OPTIX_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/device/optix)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_METAL}" ${CYCLES_INSTALL_PATH}/source/kernel/device/metal)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_METAL_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/device/metal)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_FILM_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/film)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_GEOM_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/geom)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_INTEGRATOR_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/integrator)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_LIGHT_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/light)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_OSL_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/osl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_SAMPLE_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/sample)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_SVM_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/svm)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_TYPES_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_UTIL_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/util)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_UTIL_HEADERS}" ${CYCLES_INSTALL_PATH}/source/util)

if(WITH_NANOVDB)
  set(SRC_NANOVDB_HEADERS
    nanovdb/NanoVDB.h
    nanovdb/CNanoVDB.h
  )
  set(SRC_NANOVDB_UTIL_HEADERS
    nanovdb/util/CSampleFromVoxels.h
    nanovdb/util/SampleFromVoxels.h
  )
  delayed_install(${NANOVDB_INCLUDE_DIR} "${SRC_NANOVDB_HEADERS}" ${CYCLES_INSTALL_PATH}/source/nanovdb)
  delayed_install(${NANOVDB_INCLUDE_DIR} "${SRC_NANOVDB_UTIL_HEADERS}" ${CYCLES_INSTALL_PATH}/source/nanovdb/util)
endif()
