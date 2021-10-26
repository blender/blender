# Copyright 2011-2020 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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

set(SRC_KERNEL_DEVICE_OPTIX
  device/optix/kernel.cu
  device/optix/kernel_shader_raytrace.cu
)

set(SRC_KERNEL_DEVICE_CPU_HEADERS
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
  device/gpu/parallel_reduce.h
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
  device/optix/compat.h
  device/optix/globals.h
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
  svm/noise.h
  svm/noisetex.h
  svm/normal.h
  svm/ramp.h
  svm/ramp_util.h
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

set(SRC_KERNEL_GEOM_HEADERS
  geom/geom.h
  geom/attribute.h
  geom/curve.h
  geom/curve_intersect.h
  geom/motion_curve.h
  geom/motion_triangle.h
  geom/motion_triangle_intersect.h
  geom/motion_triangle_shader.h
  geom/object.h
  geom/patch.h
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
  bvh/embree.h
)

set(SRC_KERNEL_CAMERA_HEADERS
  camera/camera.h
  camera/projection.h
)

set(SRC_KERNEL_FILM_HEADERS
  film/accumulate.h
  film/adaptive_sampling.h
  film/id_passes.h
  film/passes.h
  film/read.h
  film/write_passes.h
)

set(SRC_KERNEL_INTEGRATOR_HEADERS
  integrator/init_from_bake.h
  integrator/init_from_camera.h
  integrator/intersect_closest.h
  integrator/intersect_shadow.h
  integrator/intersect_subsurface.h
  integrator/intersect_volume_stack.h
  integrator/megakernel.h
  integrator/path_state.h
  integrator/shade_background.h
  integrator/shade_light.h
  integrator/shader_eval.h
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
  integrator/volume_stack.h
)

set(SRC_KERNEL_LIGHT_HEADERS
  light/light.h
  light/background.h
  light/common.h
  light/sample.h
)

set(SRC_KERNEL_SAMPLE_HEADERS
  sample/jitter.h
  sample/lcg.h
  sample/mapping.h
  sample/mis.h
  sample/pattern.h
)

set(SRC_KERNEL_UTIL_HEADERS
  util/color.h
  util/differential.h
  util/lookup_table.h
  util/profiling.h
)

set(SRC_KERNEL_TYPES_HEADERS
  textures.h
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
  ../util/math_int2.h
  ../util/math_int3.h
  ../util/math_int4.h
  ../util/math_matrix.h
  ../util/projection.h
  ../util/rect.h
  ../util/static_assert.h
  ../util/transform.h
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
  ../util/types_vector3.h
  ../util/types_vector3_impl.h
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
  if((CUDA_VERSION MATCHES "101") OR
     (CUDA_VERSION MATCHES "102") OR
     (CUDA_VERSION MATCHES "111") OR
     (CUDA_VERSION MATCHES "112") OR
     (CUDA_VERSION MATCHES "113") OR
     (CUDA_VERSION MATCHES "114"))
  else()
    message(WARNING
      "CUDA version ${CUDA_VERSION_MAJOR}.${CUDA_VERSION_MINOR} detected, "
      "build may succeed but only CUDA 10.1 to 11.4 are officially supported")
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
      -o ${CMAKE_CURRENT_BINARY_DIR}/${cuda_file})

    if(${experimental})
      set(cuda_flags ${cuda_flags} -D __KERNEL_EXPERIMENTAL__)
      set(name ${name}_experimental)
    endif()

    if(WITH_NANOVDB)
      set(cuda_flags ${cuda_flags}
        -D WITH_NANOVDB
        -I "${NANOVDB_INCLUDE_DIR}")
    endif()

    if(WITH_CYCLES_CUBIN_COMPILER)
      string(SUBSTRING ${arch} 3 -1 CUDA_ARCH)

      # Needed to find libnvrtc-builtins.so. Can't do it from inside
      # cycles_cubin_cc since the env variable is read before main()
      if(APPLE)
        set(CUBIN_CC_ENV ${CMAKE_COMMAND}
          -E env DYLD_LIBRARY_PATH="${cuda_toolkit_root_dir}/lib")
      elseif(UNIX)
        set(CUBIN_CC_ENV ${CMAKE_COMMAND}
          -E env LD_LIBRARY_PATH="${cuda_toolkit_root_dir}/lib64")
      endif()

      add_custom_command(
        OUTPUT ${cuda_file}
        COMMAND ${CUBIN_CC_ENV}
            "$<TARGET_FILE:cycles_cubin_cc>"
            -target ${CUDA_ARCH}
            -i ${CMAKE_CURRENT_SOURCE_DIR}${cuda_kernel_src}
            ${cuda_flags}
            -v
            -cuda-toolkit-dir "${cuda_toolkit_root_dir}"
        DEPENDS ${kernel_sources} cycles_cubin_cc)
    else()
      set(_cuda_nvcc_args
            -arch=${arch}
            ${CUDA_NVCC_FLAGS}
            --${format}
            ${CMAKE_CURRENT_SOURCE_DIR}${cuda_kernel_src}
            --ptxas-options="-v"
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
    endif()
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
      elseif(${CUDA_VERSION} LESS 110) # Support for sm_30 was removed in CUDA 11
        set(cuda_nvcc_executable ${CUDA_NVCC_EXECUTABLE})
        set(cuda_toolkit_root_dir ${CUDA_TOOLKIT_ROOT_DIR})
      else()
        message(STATUS "CUDA binaries for ${arch} require CUDA 10 or earlier, skipped.")
      endif()
    elseif(${arch} MATCHES ".*_7." AND ${CUDA_VERSION} LESS 100)
      message(STATUS "CUDA binaries for ${arch} require CUDA 10.0+, skipped.")
    elseif(${arch} MATCHES ".*_8.")
      if(DEFINED CUDA11_NVCC_EXECUTABLE)
        set(cuda_nvcc_executable ${CUDA11_NVCC_EXECUTABLE})
        set(cuda_toolkit_root_dir ${CUDA11_TOOLKIT_ROOT_DIR})
      elseif(${CUDA_VERSION} GREATER_EQUAL 111) # Support for sm_86 was introduced in CUDA 11
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
      CYCLES_CUDA_KERNEL_ADD(${arch} ${prev_arch} kernel "" "${cuda_sources}" FALSE)

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

####################################################### START

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
        -E env "HIP_PATH=${HIP_ROOT_DIR}" "PATH=${HIP_PERL_DIR}"
        ${HIP_HIPCC_EXECUTABLE}.bat)
    else()
      set(hip_command ${HIP_HIPCC_EXECUTABLE})
      set(hip_flags)
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
      -o ${CMAKE_CURRENT_BINARY_DIR}/${hip_file})

    if(${experimental})
      set(hip_flags ${hip_flags} -D __KERNEL_EXPERIMENTAL__)
      set(name ${name}_experimental)
    endif()

    if(WITH_CYCLES_DEBUG)
      set(hip_flags ${hip_flags} -D __KERNEL_DEBUG__)
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
    CYCLES_HIP_KERNEL_ADD(${arch} kernel "" "${hip_sources}" FALSE)
  endforeach()

  add_custom_target(cycles_kernel_hip ALL DEPENDS ${hip_fatbins})
  cycles_set_solution_folder(cycles_kernel_hip)
endif()

####################################################### END
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

    if(WITH_CYCLES_CUBIN_COMPILER)
      # Needed to find libnvrtc-builtins.so. Can't do it from inside
      # cycles_cubin_cc since the env variable is read before main()
      if(APPLE)
        set(CUBIN_CC_ENV ${CMAKE_COMMAND}
          -E env DYLD_LIBRARY_PATH="${CUDA_TOOLKIT_ROOT_DIR}/lib")
      elseif(UNIX)
        set(CUBIN_CC_ENV ${CMAKE_COMMAND}
          -E env LD_LIBRARY_PATH="${CUDA_TOOLKIT_ROOT_DIR}/lib64")
      endif()

      add_custom_command(
        OUTPUT ${output}
        DEPENDS
          ${input}
          ${SRC_KERNEL_HEADERS}
          ${SRC_KERNEL_DEVICE_GPU_HEADERS}
          ${SRC_KERNEL_DEVICE_CUDA_HEADERS}
          ${SRC_KERNEL_DEVICE_OPTIX_HEADERS}
          ${SRC_UTIL_HEADERS}
        COMMAND ${CUBIN_CC_ENV}
            "$<TARGET_FILE:cycles_cubin_cc>"
            -target 50
            -ptx
            -i ${CMAKE_CURRENT_SOURCE_DIR}/${input}
            ${cuda_flags}
            -v
            -cuda-toolkit-dir "${CUDA_TOOLKIT_ROOT_DIR}"
        DEPENDS ${kernel_sources} cycles_cubin_cc)
    else()
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
    endif()
    list(APPEND optix_ptx ${output})

    delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${output}" ${CYCLES_INSTALL_PATH}/lib)
  endmacro()

  CYCLES_OPTIX_KERNEL_ADD(
    kernel_optix
    "device/optix/kernel.cu"
    "")
  CYCLES_OPTIX_KERNEL_ADD(
    kernel_optix_shader_raytrace
    "device/optix/kernel_shader_raytrace.cu"
    "--keep-device-functions")

  add_custom_target(cycles_kernel_optix ALL DEPENDS ${optix_ptx})
  cycles_set_solution_folder(cycles_kernel_optix)
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
    # With OSL, Cycles disables rtti in some modules, wich then breaks at linking
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

cycles_add_library(cycles_kernel "${LIB}"
  ${SRC_KERNEL_DEVICE_CPU}
  ${SRC_KERNEL_DEVICE_CUDA}
  ${SRC_KERNEL_DEVICE_HIP}
  ${SRC_KERNEL_DEVICE_OPTIX}
  ${SRC_KERNEL_HEADERS}
  ${SRC_KERNEL_DEVICE_CPU_HEADERS}
  ${SRC_KERNEL_DEVICE_GPU_HEADERS}
  ${SRC_KERNEL_DEVICE_CUDA_HEADERS}
  ${SRC_KERNEL_DEVICE_HIP_HEADERS}
  ${SRC_KERNEL_DEVICE_OPTIX_HEADERS}
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
source_group("film" FILES ${SRC_KERNEL_FILM_HEADERS})
source_group("geom" FILES ${SRC_KERNEL_GEOM_HEADERS})
source_group("integrator" FILES ${SRC_KERNEL_INTEGRATOR_HEADERS})
source_group("kernel" FILES ${SRC_KERNEL_TYPES_HEADERS})
source_group("light" FILES ${SRC_KERNEL_LIGHT_HEADERS})
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
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_FILM_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/film)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_GEOM_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/geom)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_INTEGRATOR_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/integrator)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_LIGHT_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/light)
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
