# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

# OSL node shaders

set(SRC_OSL
  node_add_closure.osl
  node_ambient_occlusion.osl
  node_attribute.osl
  node_background.osl
  node_bevel.osl
  node_brick_texture.osl
  node_brightness.osl
  node_bump.osl
  node_camera.osl
  node_checker_texture.osl
  node_clamp.osl
  node_combine_color.osl
  node_combine_xyz.osl
  node_convert_from_color.osl
  node_convert_from_float.osl
  node_convert_from_int.osl
  node_convert_from_normal.osl
  node_convert_from_point.osl
  node_convert_from_vector.osl
  node_diffuse_bsdf.osl
  node_displacement.osl
  node_vector_displacement.osl
  node_emission.osl
  node_environment_texture.osl
  node_float_curve.osl
  node_fresnel.osl
  node_gabor_texture.osl
  node_gamma.osl
  node_geometry.osl
  node_glass_bsdf.osl
  node_glossy_bsdf.osl
  node_gradient_texture.osl
  node_hair_info.osl
  node_point_info.osl
  node_scatter_volume.osl
  node_absorption_volume.osl
  node_volume_coefficients.osl
  node_principled_volume.osl
  node_holdout.osl
  node_hsv.osl
  node_ies_light.osl
  node_image_texture.osl
  node_invert.osl
  node_layer_weight.osl
  node_light_falloff.osl
  node_light_path.osl
  node_magic_texture.osl
  node_map_range.osl
  node_mapping.osl
  node_math.osl
  node_metallic_bsdf.osl
  node_mix.osl
  node_mix_closure.osl
  node_mix_color.osl
  node_mix_float.osl
  node_mix_vector.osl
  node_mix_vector_non_uniform.osl
  node_noise_texture.osl
  node_normal.osl
  node_normal_map.osl
  node_object_info.osl
  node_output_displacement.osl
  node_output_surface.osl
  node_output_volume.osl
  node_particle_info.osl
  node_ray_portal_bsdf.osl
  node_refraction_bsdf.osl
  node_rgb_curves.osl
  node_rgb_ramp.osl
  node_radial_tiling.osl
  node_separate_color.osl
  node_separate_xyz.osl
  node_set_normal.osl
  node_sheen_bsdf.osl
  node_sky_texture.osl
  node_subsurface_scattering.osl
  node_tangent.osl
  node_texture_coordinate.osl
  node_toon_bsdf.osl
  node_translucent_bsdf.osl
  node_transparent_bsdf.osl
  node_value.osl
  node_vector_curves.osl
  node_vector_math.osl
  node_vector_map_range.osl
  node_vector_rotate.osl
  node_vector_transform.osl
  node_vertex_color.osl
  node_voronoi_texture.osl
  node_wavelength.osl
  node_blackbody.osl
  node_wave_texture.osl
  node_white_noise_texture.osl
  node_wireframe.osl
  node_hair_bsdf.osl
  node_principled_hair_bsdf.osl
  node_uv_map.osl
  node_principled_bsdf.osl
  node_rgb_to_bw.osl
)

# The headers that OSL ships differs per release so we can not
# hard-code this.
file(GLOB SRC_OSL_HEADER_DIST ${OSL_SHADER_DIR}/*.h)

set(SRC_OSL_HEADERS
  int_vector_types.h
  node_color.h
  node_color_blend.h
  node_fractal_voronoi.h
  node_fresnel.h
  node_hash.h
  node_math.h
  node_noise.h
  node_ramp_util.h
  node_radial_tiling_shared.h
  node_voronoi.h
  stdcycles.h
  ${SRC_OSL_HEADER_DIST}
)

set(SRC_OSO

)

# TODO, add a module to compile OSL
foreach(_file ${SRC_OSL})
  set(_OSL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${_file})
  set_source_files_properties(${_file} PROPERTIES HEADER_FILE_ONLY TRUE)
  string(REPLACE ".osl" ".oso" _OSO_FILE ${_OSL_FILE})
  string(REPLACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} _OSO_FILE ${_OSO_FILE})
  add_custom_command(
    OUTPUT ${_OSO_FILE}
    COMMAND
      ${CMAKE_COMMAND} -E env ${PLATFORM_ENV_BUILD}
      ${OSL_COMPILER} -q -O2
        -I"${CMAKE_CURRENT_SOURCE_DIR}"
        -I"${OSL_SHADER_DIR}"
        -o ${_OSO_FILE}
        ${_OSL_FILE}
    DEPENDS ${_OSL_FILE} ${SRC_OSL_HEADERS} ${OSL_COMPILER}
  )
  list(APPEND SRC_OSO
    ${_OSO_FILE}
  )

  unset(_OSL_FILE)
  unset(_OSO_FILE)
endforeach()

add_custom_target(cycles_osl_shaders ALL DEPENDS ${SRC_OSO} ${SRC_OSL_HEADERS} ${OSL_COMPILER} SOURCES ${SRC_OSL})
cycles_set_solution_folder(cycles_osl_shaders)

# CMAKE_CURRENT_SOURCE_DIR is already included in OSO paths
delayed_install("" "${SRC_OSO}" ${CYCLES_INSTALL_PATH}/shader)
delayed_install("${CMAKE_CURRENT_SOURCE_DIR}" "${SRC_OSL_HEADERS}" ${CYCLES_INSTALL_PATH}/shader)
