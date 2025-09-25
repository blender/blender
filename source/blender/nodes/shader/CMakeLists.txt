# SPDX-FileCopyrightText: 2021 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../intern
  ../../editors/include
  ../../makesrna
  ../../../../intern/sky/include
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  nodes/node_shader_add_shader.cc
  nodes/node_shader_ambient_occlusion.cc
  nodes/node_shader_attribute.cc
  nodes/node_shader_background.cc
  nodes/node_shader_bevel.cc
  nodes/node_shader_blackbody.cc
  nodes/node_shader_brightness.cc
  nodes/node_shader_bsdf_diffuse.cc
  nodes/node_shader_bsdf_glass.cc
  nodes/node_shader_bsdf_glossy.cc
  nodes/node_shader_bsdf_hair.cc
  nodes/node_shader_bsdf_hair_principled.cc
  nodes/node_shader_bsdf_metallic.cc
  nodes/node_shader_bsdf_principled.cc
  nodes/node_shader_bsdf_ray_portal.cc
  nodes/node_shader_bsdf_refraction.cc
  nodes/node_shader_bsdf_sheen.cc
  nodes/node_shader_bsdf_toon.cc
  nodes/node_shader_bsdf_translucent.cc
  nodes/node_shader_bsdf_transparent.cc
  nodes/node_shader_bump.cc
  nodes/node_shader_camera.cc
  nodes/node_shader_clamp.cc
  nodes/node_shader_color_ramp.cc
  nodes/node_shader_common.cc
  nodes/node_shader_curves.cc
  nodes/node_shader_displacement.cc
  nodes/node_shader_eevee_specular.cc
  nodes/node_shader_emission.cc
  nodes/node_shader_fresnel.cc
  nodes/node_shader_gamma.cc
  nodes/node_shader_geometry.cc
  nodes/node_shader_hair_info.cc
  nodes/node_shader_holdout.cc
  nodes/node_shader_hueSatVal.cc
  nodes/node_shader_ies_light.cc
  nodes/node_shader_invert.cc
  nodes/node_shader_layer_weight.cc
  nodes/node_shader_light_falloff.cc
  nodes/node_shader_light_path.cc
  nodes/node_shader_map_range.cc
  nodes/node_shader_mapping.cc
  nodes/node_shader_math.cc
  nodes/node_shader_mix.cc
  nodes/node_shader_mix_rgb.cc
  nodes/node_shader_mix_shader.cc
  nodes/node_shader_normal.cc
  nodes/node_shader_normal_map.cc
  nodes/node_shader_object_info.cc
  nodes/node_shader_output_aov.cc
  nodes/node_shader_output_light.cc
  nodes/node_shader_output_linestyle.cc
  nodes/node_shader_output_material.cc
  nodes/node_shader_output_world.cc
  nodes/node_shader_particle_info.cc
  nodes/node_shader_point_info.cc
  nodes/node_shader_radial_tiling.cc
  nodes/node_shader_rgb.cc
  nodes/node_shader_rgb_to_bw.cc
  nodes/node_shader_script.cc
  nodes/node_shader_sepcomb_color.cc
  nodes/node_shader_sepcomb_xyz.cc
  nodes/node_shader_shader_to_rgb.cc
  nodes/node_shader_squeeze.cc
  nodes/node_shader_subsurface_scattering.cc
  nodes/node_shader_tangent.cc
  nodes/node_shader_tex_brick.cc
  nodes/node_shader_tex_checker.cc
  nodes/node_shader_tex_coord.cc
  nodes/node_shader_tex_environment.cc
  nodes/node_shader_tex_gabor.cc
  nodes/node_shader_tex_gradient.cc
  nodes/node_shader_tex_image.cc
  nodes/node_shader_tex_magic.cc
  nodes/node_shader_tex_noise.cc
  nodes/node_shader_tex_sky.cc
  nodes/node_shader_tex_voronoi.cc
  nodes/node_shader_tex_wave.cc
  nodes/node_shader_tex_white_noise.cc
  nodes/node_shader_uv_along_stroke.cc
  nodes/node_shader_uvmap.cc
  nodes/node_shader_value.cc
  nodes/node_shader_vector_displacement.cc
  nodes/node_shader_vector_math.cc
  nodes/node_shader_vector_rotate.cc
  nodes/node_shader_vector_transform.cc
  nodes/node_shader_vertex_color.cc
  nodes/node_shader_volume_absorption.cc
  nodes/node_shader_volume_coefficients.cc
  nodes/node_shader_volume_info.cc
  nodes/node_shader_volume_principled.cc
  nodes/node_shader_volume_scatter.cc
  nodes/node_shader_wavelength.cc
  nodes/node_shader_wireframe.cc

  node_shader_register.cc
  node_shader_tree.cc
  node_shader_util.cc

  node_shader_register.hh
  node_shader_util.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::functions
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  bf_intern_sky
  bf_nodes
  PRIVATE bf::render
  PRIVATE bf::windowmanager
)

if(WITH_MATERIALX)
  add_definitions(-DWITH_MATERIALX)
  list(APPEND SRC
    materialx/group_nodes.cc
    materialx/material.cc
    materialx/node_graph.cc
    materialx/node_item.cc
    materialx/node_parser.cc

    materialx/group_nodes.h
    materialx/material.h
    materialx/node_graph.h
    materialx/node_item.h
    materialx/node_parser.h
  )
  list(APPEND LIB
    MaterialXCore
    MaterialXFormat
  )

  if(WITH_USD)
    add_definitions(-DWITH_USD)
    list(APPEND INC_SYS
      ${USD_INCLUDE_DIRS}
      ${BOOST_INCLUDE_DIR}
    )
  endif()
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the STL versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

blender_add_lib(bf_nodes_shader "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

blender_set_target_unity_build(bf_nodes_shader 10)

# RNA_prototypes.hh
add_dependencies(bf_nodes_shader bf_rna)
