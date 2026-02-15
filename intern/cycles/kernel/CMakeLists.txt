# SPDX-FileCopyrightText: 2011-2026 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ..
)

set(INC_SYS

)

set(SRC_KERNEL_DEVICE_GPU_HEADERS
  device/gpu/block_sizes.h
  device/gpu/image.h
  device/gpu/kernel.h
  device/gpu/parallel_active_index.h
  device/gpu/parallel_prefix_sum.h
  device/gpu/parallel_sorted_index.h
  device/gpu/work_stealing.h
)

set(SRC_KERNEL_CLOSURE_HEADERS
  closure/alloc.h
  closure/bsdf.h
  closure/bsdf_ashikhmin_velvet.h
  closure/bsdf_burley.h
  closure/bsdf_diffuse.h
  closure/bsdf_diffuse_ramp.h
  closure/bsdf_microfacet.h
  closure/bsdf_oren_nayar.h
  closure/bsdf_phong_ramp.h
  closure/bsdf_ray_portal.h
  closure/bsdf_sheen.h
  closure/bsdf_toon.h
  closure/bsdf_transparent.h
  closure/bsdf_util.h
  closure/bsdf_ashikhmin_shirley.h
  closure/bsdf_hair.h
  closure/bssrdf.h
  closure/emissive.h
  closure/volume.h
  closure/volume_util.h
  closure/volume_henyey_greenstein.h
  closure/volume_rayleigh.h
  closure/volume_fournier_forand.h
  closure/volume_draine.h
  closure/bsdf_principled_hair_chiang.h
  closure/bsdf_principled_hair_huang.h
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
  svm/gabor.h
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
  svm/node_types_template.h
  svm/noise.h
  svm/noisetex.h
  svm/normal.h
  svm/ramp.h
  svm/ramp_util.h
  svm/radial_tiling.h
  svm/radial_tiling_shared.h
  svm/raycast.h
  svm/sepcomb_color.h
  svm/sepcomb_vector.h
  svm/sky.h
  svm/tex_coord.h
  svm/fractal_noise.h
  svm/types.h
  svm/util.h
  svm/value.h
  svm/vector_rotate.h
  svm/vector_transform.h
  svm/voronoi.h
  svm/wave.h
  svm/white_noise.h
  svm/vertex_color.h
)

if(WITH_CYCLES_OSL)
  set(SRC_KERNEL_OSL_HEADERS
    osl/osl.h
    osl/camera.h
    osl/closures_setup.h
    osl/closures_template.h
    osl/services_gpu.h
    osl/strings.h
    osl/types.h
  )
endif()

set(SRC_KERNEL_GEOM_HEADERS
  geom/attribute.h
  geom/curve.h
  geom/curve_intersect.h
  geom/geom_intersect.h
  geom/motion_curve.h
  geom/motion_point.h
  geom/motion_triangle.h
  geom/motion_triangle_intersect.h
  geom/motion_triangle_shader.h
  geom/object.h
  geom/point.h
  geom/point_intersect.h
  geom/primitive.h
  geom/shader_data.h
  geom/triangle.h
  geom/triangle_intersect.h
  geom/volume.h
)

set(SRC_KERNEL_BAKE_HEADERS
  bake/bake.h
)

set(SRC_KERNEL_BVH_HEADERS
  bvh/bvh.h
  bvh/intersect_filter.h
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
  film/volume_guiding_denoise.h
  film/cryptomatte_passes.h
  film/light_passes.h
  film/read.h
  film/write.h
)

set(SRC_KERNEL_INTEGRATOR_HEADERS
  integrator/displacement_shader.h
  integrator/init_from_bake.h
  integrator/init_from_camera.h
  integrator/intersect_dedicated_light.h
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
  integrator/shade_dedicated_light.h
  integrator/shadow_catcher.h
  integrator/shadow_linking.h
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
  util/colorspace.h
  util/differential.h
  util/ies.h
  util/image_2d.h
  util/image_3d.h
  util/lookup_table.h
  util/nanovdb.h
  util/profiler.h
)

set(SRC_KERNEL_TYPES_HEADERS
  data_arrays.h
  data_template.h
  features.h
  globals.h
  image.h
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
  ../util/guiding.h
  ../util/half.h
  ../util/hash.h
  ../util/math.h
  ../util/math_base.h
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
  ../util/math_dual.h
  ../util/projection.h
  ../util/projection_inverse.h
  ../util/rect.h
  ../util/static_assert.h
  ../util/transform.h
  ../util/types.h
  ../util/types_base.h
  ../util/types_float2.h
  ../util/types_float3.h
  ../util/types_float4.h
  ../util/types_float8.h
  ../util/types_image.h
  ../util/types_int2.h
  ../util/types_int3.h
  ../util/types_int4.h
  ../util/types_int8.h
  ../util/types_normal.h
  ../util/types_spectrum.h
  ../util/types_rgbe.h
  ../util/types_uchar2.h
  ../util/types_uchar3.h
  ../util/types_uchar4.h
  ../util/types_uint2.h
  ../util/types_uint3.h
  ../util/types_uint4.h
  ../util/types_ushort4.h
  ../util/types_dual.h
)

set(LIB

)

# `Zstd` compressor for kernels.
add_executable(zstd_compress ../cmake/zstd_compress.cpp)

target_link_libraries(zstd_compress PRIVATE bf::dependencies::zstd)
target_link_libraries(zstd_compress PRIVATE bf::dependencies::pthreads)

if(NOT WITH_BLENDER)
  # For the Cycles standalone put libraries next to the Cycles application.
  set(cycles_kernel_runtime_lib_target_path ${CYCLES_INSTALL_PATH})
else()
  # For Blender put the libraries next to the Blender executable.
  #
  # Note that the installation path in the delayed_install is relative to the versioned folder,
  # which means we need to go one level up.
  set(cycles_kernel_runtime_lib_target_path "../")
endif()

if(UNIX AND NOT APPLE)
  set(cycles_kernel_runtime_lib_target_path ${cycles_kernel_runtime_lib_target_path}/lib)
endif()

if(WITH_COMPILER_ASAN)
  if((CMAKE_C_COMPILER_ID STREQUAL "GNU") AND (NOT WITH_CYCLES_KERNEL_ASAN))
    # GCC hangs compiling the big kernel files with ASAN and release, so disable by default.
    string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " -fno-sanitize=all")
    string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fno-sanitize=vptr")
  elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    # With OSL, Cycles disables RTTI in some modules, which then breaks at linking
    # when trying to use `vptr` sanitizer (included into 'undefined' general option).
    string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " -fno-sanitize=vptr")
    string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fno-sanitize=vptr")
  endif()
endif()

# -----------------------------------------------------------------------------
# Create module with headers common for all compute backends.

add_library(cycles_kernel INTERFACE)
target_sources(cycles_kernel INTERFACE
  ${SRC_KERNEL_HEADERS}
  ${SRC_KERNEL_DEVICE_GPU_HEADERS}
  ${SRC_KERNEL_UTIL_HEADERS}
)

source_group("bake" FILES ${SRC_KERNEL_BAKE_HEADERS})
source_group("bvh" FILES ${SRC_KERNEL_BVH_HEADERS})
source_group("camera" FILES ${SRC_KERNEL_CAMERA_HEADERS})
source_group("closure" FILES ${SRC_KERNEL_CLOSURE_HEADERS})
source_group("device\\gpu" FILES ${SRC_KERNEL_DEVICE_GPU_HEADERS})
source_group("film" FILES ${SRC_KERNEL_FILM_HEADERS})
source_group("geom" FILES ${SRC_KERNEL_GEOM_HEADERS})
source_group("integrator" FILES ${SRC_KERNEL_INTEGRATOR_HEADERS})
source_group("kernel" FILES ${SRC_KERNEL_TYPES_HEADERS})
source_group("light" FILES ${SRC_KERNEL_LIGHT_HEADERS})
source_group("osl" FILES ${SRC_KERNEL_OSL_HEADERS})
source_group("sample" FILES ${SRC_KERNEL_SAMPLE_HEADERS})
source_group("svm" FILES ${SRC_KERNEL_SVM_HEADERS})
source_group("util" FILES ${SRC_KERNEL_UTIL_HEADERS})

# -----------------------------------------------------------------------------
# Install kernel source for runtime compilation.

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_BAKE_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/bake)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_BVH_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/bvh)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_CAMERA_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/camera)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_CLOSURE_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/closure)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNEL_DEVICE_GPU_HEADERS}" ${CYCLES_INSTALL_PATH}/source/kernel/device/gpu)
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

# -----------------------------------------------------------------------------
# OSL module.

if(WITH_CYCLES_OSL)
  list(APPEND LIB
    cycles_kernel_osl
  )
  add_subdirectory(osl)
  add_subdirectory(osl/shaders)
endif()

# -----------------------------------------------------------------------------
# Backend-specific rules.

add_subdirectory(device/cpu)
add_subdirectory(device/cuda)
add_subdirectory(device/hip)
add_subdirectory(device/hiprt)
add_subdirectory(device/metal)
add_subdirectory(device/oneapi)
add_subdirectory(device/optix)

target_link_libraries(cycles_kernel
  INTERFACE cycles_kernel_cpu
)

if(NOT WITH_CYCLES_PARALLEL_DEVICE_KERNEL_BUILD)
  set(_cycles_prev_gpu_kernel_target "")
  macro(cycles_add_gpu_kernel_dependencies GPU_KERNEL_TARGET)
    if(TARGET ${GPU_KERNEL_TARGET})
      if(_cycles_prev_gpu_kernel_target)
        add_dependencies(${GPU_KERNEL_TARGET} ${_cycles_prev_gpu_kernel_target})
      endif()
      set(_cycles_prev_gpu_kernel_target ${GPU_KERNEL_TARGET})
    endif()
  endmacro()

  cycles_add_gpu_kernel_dependencies(cycles_kernel_cuda)
  cycles_add_gpu_kernel_dependencies(cycles_kernel_optix)
  cycles_add_gpu_kernel_dependencies(cycles_kernel_hip)
  cycles_add_gpu_kernel_dependencies(cycles_kernel_hiprt)
  cycles_add_gpu_kernel_dependencies(cycles_kernel_oneapi)
  cycles_add_gpu_kernel_dependencies(cycles_kernel_cpu)
endif()
