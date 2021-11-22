# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  composite
  function
  geometry
  intern
  shader
  texture
  ../editors/include
  ../blenkernel
  ../blenlib
  ../blentranslation
  ../bmesh
  ../depsgraph
  ../functions
  ../geometry
  ../gpu
  ../imbuf
  ../makesdna
  ../makesrna
  ../render
  ../windowmanager
  ../../../intern/glew-mx
  ../../../intern/guardedalloc
  ../../../intern/sky/include
)


set(SRC
  composite/nodes/node_composite_alphaOver.cc
  composite/nodes/node_composite_antialiasing.cc
  composite/nodes/node_composite_bilateralblur.cc
  composite/nodes/node_composite_blur.cc
  composite/nodes/node_composite_bokehblur.cc
  composite/nodes/node_composite_bokehimage.cc
  composite/nodes/node_composite_boxmask.cc
  composite/nodes/node_composite_brightness.cc
  composite/nodes/node_composite_channelMatte.cc
  composite/nodes/node_composite_chromaMatte.cc
  composite/nodes/node_composite_colorMatte.cc
  composite/nodes/node_composite_colorSpill.cc
  composite/nodes/node_composite_colorbalance.cc
  composite/nodes/node_composite_colorcorrection.cc
  composite/nodes/node_composite_common.cc
  composite/nodes/node_composite_composite.cc
  composite/nodes/node_composite_cornerpin.cc
  composite/nodes/node_composite_crop.cc
  composite/nodes/node_composite_cryptomatte.cc
  composite/nodes/node_composite_curves.cc
  composite/nodes/node_composite_defocus.cc
  composite/nodes/node_composite_denoise.cc
  composite/nodes/node_composite_despeckle.cc
  composite/nodes/node_composite_diffMatte.cc
  composite/nodes/node_composite_dilate.cc
  composite/nodes/node_composite_directionalblur.cc
  composite/nodes/node_composite_displace.cc
  composite/nodes/node_composite_distanceMatte.cc
  composite/nodes/node_composite_doubleEdgeMask.cc
  composite/nodes/node_composite_ellipsemask.cc
  composite/nodes/node_composite_exposure.cc
  composite/nodes/node_composite_filter.cc
  composite/nodes/node_composite_flip.cc
  composite/nodes/node_composite_gamma.cc
  composite/nodes/node_composite_glare.cc
  composite/nodes/node_composite_hueSatVal.cc
  composite/nodes/node_composite_huecorrect.cc
  composite/nodes/node_composite_idMask.cc
  composite/nodes/node_composite_image.cc
  composite/nodes/node_composite_inpaint.cc
  composite/nodes/node_composite_invert.cc
  composite/nodes/node_composite_keying.cc
  composite/nodes/node_composite_keyingscreen.cc
  composite/nodes/node_composite_lensdist.cc
  composite/nodes/node_composite_levels.cc
  composite/nodes/node_composite_lummaMatte.cc
  composite/nodes/node_composite_mapRange.cc
  composite/nodes/node_composite_mapUV.cc
  composite/nodes/node_composite_mapValue.cc
  composite/nodes/node_composite_mask.cc
  composite/nodes/node_composite_math.cc
  composite/nodes/node_composite_mixrgb.cc
  composite/nodes/node_composite_movieclip.cc
  composite/nodes/node_composite_moviedistortion.cc
  composite/nodes/node_composite_normal.cc
  composite/nodes/node_composite_normalize.cc
  composite/nodes/node_composite_outputFile.cc
  composite/nodes/node_composite_pixelate.cc
  composite/nodes/node_composite_planetrackdeform.cc
  composite/nodes/node_composite_posterize.cc
  composite/nodes/node_composite_premulkey.cc
  composite/nodes/node_composite_rgb.cc
  composite/nodes/node_composite_rotate.cc
  composite/nodes/node_composite_scale.cc
  composite/nodes/node_composite_sepcombHSVA.cc
  composite/nodes/node_composite_sepcombRGBA.cc
  composite/nodes/node_composite_sepcombYCCA.cc
  composite/nodes/node_composite_sepcombYUVA.cc
  composite/nodes/node_composite_setalpha.cc
  composite/nodes/node_composite_splitViewer.cc
  composite/nodes/node_composite_stabilize2d.cc
  composite/nodes/node_composite_sunbeams.cc
  composite/nodes/node_composite_switch.cc
  composite/nodes/node_composite_switchview.cc
  composite/nodes/node_composite_texture.cc
  composite/nodes/node_composite_tonemap.cc
  composite/nodes/node_composite_trackpos.cc
  composite/nodes/node_composite_transform.cc
  composite/nodes/node_composite_translate.cc
  composite/nodes/node_composite_valToRgb.cc
  composite/nodes/node_composite_value.cc
  composite/nodes/node_composite_vecBlur.cc
  composite/nodes/node_composite_viewer.cc
  composite/nodes/node_composite_zcombine.cc

  composite/node_composite_tree.cc
  composite/node_composite_util.cc

  function/nodes/legacy/node_fn_random_float.cc

  function/nodes/node_fn_align_euler_to_vector.cc
  function/nodes/node_fn_boolean_math.cc
  function/nodes/node_fn_float_compare.cc
  function/nodes/node_fn_float_to_int.cc
  function/nodes/node_fn_input_bool.cc
  function/nodes/node_fn_input_color.cc
  function/nodes/node_fn_input_int.cc
  function/nodes/node_fn_input_special_characters.cc
  function/nodes/node_fn_input_string.cc
  function/nodes/node_fn_input_vector.cc
  function/nodes/node_fn_random_value.cc
  function/nodes/node_fn_replace_string.cc
  function/nodes/node_fn_rotate_euler.cc
  function/nodes/node_fn_slice_string.cc
  function/nodes/node_fn_string_length.cc
  function/nodes/node_fn_value_to_string.cc
  function/node_function_util.cc

  geometry/nodes/legacy/node_geo_align_rotation_to_vector.cc
  geometry/nodes/legacy/node_geo_attribute_clamp.cc
  geometry/nodes/legacy/node_geo_attribute_color_ramp.cc
  geometry/nodes/legacy/node_geo_attribute_combine_xyz.cc
  geometry/nodes/legacy/node_geo_attribute_compare.cc
  geometry/nodes/legacy/node_geo_attribute_convert.cc
  geometry/nodes/legacy/node_geo_attribute_curve_map.cc
  geometry/nodes/legacy/node_geo_attribute_fill.cc
  geometry/nodes/legacy/node_geo_attribute_map_range.cc
  geometry/nodes/legacy/node_geo_attribute_math.cc
  geometry/nodes/legacy/node_geo_attribute_mix.cc
  geometry/nodes/legacy/node_geo_attribute_proximity.cc
  geometry/nodes/legacy/node_geo_attribute_randomize.cc
  geometry/nodes/legacy/node_geo_attribute_sample_texture.cc
  geometry/nodes/legacy/node_geo_attribute_separate_xyz.cc
  geometry/nodes/legacy/node_geo_attribute_transfer.cc
  geometry/nodes/legacy/node_geo_attribute_vector_math.cc
  geometry/nodes/legacy/node_geo_attribute_vector_rotate.cc
  geometry/nodes/legacy/node_geo_curve_endpoints.cc
  geometry/nodes/legacy/node_geo_curve_reverse.cc
  geometry/nodes/legacy/node_geo_curve_select_by_handle_type.cc
  geometry/nodes/legacy/node_geo_curve_set_handles.cc
  geometry/nodes/legacy/node_geo_curve_spline_type.cc
  geometry/nodes/legacy/node_geo_curve_subdivide.cc
  geometry/nodes/legacy/node_geo_curve_to_points.cc
  geometry/nodes/legacy/node_geo_delete_geometry.cc
  geometry/nodes/legacy/node_geo_edge_split.cc
  geometry/nodes/legacy/node_geo_material_assign.cc
  geometry/nodes/legacy/node_geo_mesh_to_curve.cc
  geometry/nodes/legacy/node_geo_point_distribute.cc
  geometry/nodes/legacy/node_geo_point_instance.cc
  geometry/nodes/legacy/node_geo_point_rotate.cc
  geometry/nodes/legacy/node_geo_point_scale.cc
  geometry/nodes/legacy/node_geo_point_separate.cc
  geometry/nodes/legacy/node_geo_point_translate.cc
  geometry/nodes/legacy/node_geo_points_to_volume.cc
  geometry/nodes/legacy/node_geo_raycast.cc
  geometry/nodes/legacy/node_geo_select_by_material.cc
  geometry/nodes/legacy/node_geo_subdivision_surface.cc
  geometry/nodes/legacy/node_geo_volume_to_mesh.cc

  geometry/nodes/node_geo_attribute_capture.cc
  geometry/nodes/node_geo_attribute_remove.cc
  geometry/nodes/node_geo_attribute_statistic.cc
  geometry/nodes/node_geo_boolean.cc
  geometry/nodes/node_geo_bounding_box.cc
  geometry/nodes/node_geo_collection_info.cc
  geometry/nodes/node_geo_common.cc
  geometry/nodes/node_geo_convex_hull.cc
  geometry/nodes/node_geo_curve_endpoint_selection.cc
  geometry/nodes/node_geo_curve_fill.cc
  geometry/nodes/node_geo_curve_fillet.cc
  geometry/nodes/node_geo_curve_handle_type_selection.cc
  geometry/nodes/node_geo_curve_length.cc
  geometry/nodes/node_geo_curve_parameter.cc
  geometry/nodes/node_geo_curve_primitive_bezier_segment.cc
  geometry/nodes/node_geo_curve_primitive_circle.cc
  geometry/nodes/node_geo_curve_primitive_line.cc
  geometry/nodes/node_geo_curve_primitive_quadratic_bezier.cc
  geometry/nodes/node_geo_curve_primitive_quadrilateral.cc
  geometry/nodes/node_geo_curve_primitive_spiral.cc
  geometry/nodes/node_geo_curve_primitive_star.cc
  geometry/nodes/node_geo_curve_resample.cc
  geometry/nodes/node_geo_curve_reverse.cc
  geometry/nodes/node_geo_curve_sample.cc
  geometry/nodes/node_geo_curve_set_handles.cc
  geometry/nodes/node_geo_curve_spline_type.cc
  geometry/nodes/node_geo_curve_subdivide.cc
  geometry/nodes/node_geo_curve_to_mesh.cc
  geometry/nodes/node_geo_curve_to_points.cc
  geometry/nodes/node_geo_curve_trim.cc
  geometry/nodes/node_geo_delete_geometry.cc
  geometry/nodes/node_geo_distribute_points_on_faces.cc
  geometry/nodes/node_geo_edge_split.cc
  geometry/nodes/node_geo_image_texture.cc
  geometry/nodes/node_geo_input_curve_handles.cc
  geometry/nodes/node_geo_input_curve_tilt.cc
  geometry/nodes/node_geo_input_id.cc
  geometry/nodes/node_geo_input_index.cc
  geometry/nodes/node_geo_input_material_index.cc
  geometry/nodes/node_geo_input_material.cc
  geometry/nodes/node_geo_input_normal.cc
  geometry/nodes/node_geo_input_position.cc
  geometry/nodes/node_geo_input_radius.cc
  geometry/nodes/node_geo_input_shade_smooth.cc
  geometry/nodes/node_geo_input_spline_cyclic.cc
  geometry/nodes/node_geo_input_spline_length.cc
  geometry/nodes/node_geo_input_spline_resolution.cc
  geometry/nodes/node_geo_input_tangent.cc
  geometry/nodes/node_geo_instance_on_points.cc
  geometry/nodes/node_geo_instances_to_points.cc
  geometry/nodes/node_geo_is_viewport.cc
  geometry/nodes/node_geo_join_geometry.cc
  geometry/nodes/node_geo_material_replace.cc
  geometry/nodes/node_geo_material_selection.cc
  geometry/nodes/node_geo_mesh_primitive_circle.cc
  geometry/nodes/node_geo_mesh_primitive_cone.cc
  geometry/nodes/node_geo_mesh_primitive_cube.cc
  geometry/nodes/node_geo_mesh_primitive_cylinder.cc
  geometry/nodes/node_geo_mesh_primitive_grid.cc
  geometry/nodes/node_geo_mesh_primitive_ico_sphere.cc
  geometry/nodes/node_geo_mesh_primitive_line.cc
  geometry/nodes/node_geo_mesh_primitive_uv_sphere.cc
  geometry/nodes/node_geo_mesh_subdivide.cc
  geometry/nodes/node_geo_mesh_to_curve.cc
  geometry/nodes/node_geo_mesh_to_points.cc
  geometry/nodes/node_geo_object_info.cc
  geometry/nodes/node_geo_points_to_vertices.cc
  geometry/nodes/node_geo_points_to_volume.cc
  geometry/nodes/node_geo_proximity.cc
  geometry/nodes/node_geo_raycast.cc
  geometry/nodes/node_geo_realize_instances.cc
  geometry/nodes/node_geo_rotate_instances.cc
  geometry/nodes/node_geo_scale_instances.cc
  geometry/nodes/node_geo_separate_components.cc
  geometry/nodes/node_geo_separate_geometry.cc
  geometry/nodes/node_geo_set_curve_handles.cc
  geometry/nodes/node_geo_set_curve_radius.cc
  geometry/nodes/node_geo_set_curve_tilt.cc
  geometry/nodes/node_geo_set_id.cc
  geometry/nodes/node_geo_set_material_index.cc
  geometry/nodes/node_geo_set_material.cc
  geometry/nodes/node_geo_set_point_radius.cc
  geometry/nodes/node_geo_set_position.cc
  geometry/nodes/node_geo_set_shade_smooth.cc
  geometry/nodes/node_geo_set_spline_cyclic.cc
  geometry/nodes/node_geo_set_spline_resolution.cc
  geometry/nodes/node_geo_string_join.cc
  geometry/nodes/node_geo_string_to_curves.cc
  geometry/nodes/node_geo_subdivision_surface.cc
  geometry/nodes/node_geo_switch.cc
  geometry/nodes/node_geo_transfer_attribute.cc
  geometry/nodes/node_geo_transform.cc
  geometry/nodes/node_geo_translate_instances.cc
  geometry/nodes/node_geo_triangulate.cc
  geometry/nodes/node_geo_viewer.cc
  geometry/nodes/node_geo_volume_to_mesh.cc

  geometry/node_geometry_exec.cc
  geometry/node_geometry_tree.cc
  geometry/node_geometry_util.cc

  shader/nodes/node_shader_add_shader.c
  shader/nodes/node_shader_ambient_occlusion.c
  shader/nodes/node_shader_attribute.c
  shader/nodes/node_shader_background.c
  shader/nodes/node_shader_bevel.c
  shader/nodes/node_shader_blackbody.c
  shader/nodes/node_shader_brightness.c
  shader/nodes/node_shader_bsdf_anisotropic.c
  shader/nodes/node_shader_bsdf_diffuse.c
  shader/nodes/node_shader_bsdf_glass.c
  shader/nodes/node_shader_bsdf_glossy.c
  shader/nodes/node_shader_bsdf_hair.c
  shader/nodes/node_shader_bsdf_hair_principled.c
  shader/nodes/node_shader_bsdf_principled.c
  shader/nodes/node_shader_bsdf_refraction.c
  shader/nodes/node_shader_bsdf_toon.c
  shader/nodes/node_shader_bsdf_translucent.c
  shader/nodes/node_shader_bsdf_transparent.c
  shader/nodes/node_shader_bsdf_velvet.c
  shader/nodes/node_shader_bump.c
  shader/nodes/node_shader_camera.c
  shader/nodes/node_shader_clamp.cc
  shader/nodes/node_shader_common.c
  shader/nodes/node_shader_curves.cc
  shader/nodes/node_shader_displacement.c
  shader/nodes/node_shader_eevee_specular.c
  shader/nodes/node_shader_emission.c
  shader/nodes/node_shader_fresnel.c
  shader/nodes/node_shader_gamma.c
  shader/nodes/node_shader_geometry.c
  shader/nodes/node_shader_hair_info.c
  shader/nodes/node_shader_holdout.c
  shader/nodes/node_shader_hueSatVal.c
  shader/nodes/node_shader_ies_light.c
  shader/nodes/node_shader_invert.c
  shader/nodes/node_shader_layer_weight.c
  shader/nodes/node_shader_light_falloff.c
  shader/nodes/node_shader_light_path.c
  shader/nodes/node_shader_map_range.cc
  shader/nodes/node_shader_mapping.c
  shader/nodes/node_shader_math.cc
  shader/nodes/node_shader_mixRgb.cc
  shader/nodes/node_shader_mix_shader.c
  shader/nodes/node_shader_normal.c
  shader/nodes/node_shader_normal_map.c
  shader/nodes/node_shader_object_info.c
  shader/nodes/node_shader_output_aov.c
  shader/nodes/node_shader_output_light.c
  shader/nodes/node_shader_output_linestyle.c
  shader/nodes/node_shader_output_material.c
  shader/nodes/node_shader_output_world.c
  shader/nodes/node_shader_particle_info.c
  shader/nodes/node_shader_rgb.c
  shader/nodes/node_shader_script.c
  shader/nodes/node_shader_sepcombHSV.c
  shader/nodes/node_shader_sepcombRGB.cc
  shader/nodes/node_shader_sepcombXYZ.cc
  shader/nodes/node_shader_shaderToRgb.c
  shader/nodes/node_shader_squeeze.c
  shader/nodes/node_shader_subsurface_scattering.c
  shader/nodes/node_shader_tangent.c
  shader/nodes/node_shader_tex_brick.cc
  shader/nodes/node_shader_tex_checker.cc
  shader/nodes/node_shader_tex_coord.c
  shader/nodes/node_shader_tex_environment.c
  shader/nodes/node_shader_tex_gradient.cc
  shader/nodes/node_shader_tex_image.cc
  shader/nodes/node_shader_tex_magic.cc
  shader/nodes/node_shader_tex_musgrave.cc
  shader/nodes/node_shader_tex_noise.cc
  shader/nodes/node_shader_tex_pointdensity.c
  shader/nodes/node_shader_tex_sky.c
  shader/nodes/node_shader_tex_voronoi.cc
  shader/nodes/node_shader_tex_wave.cc
  shader/nodes/node_shader_tex_white_noise.cc
  shader/nodes/node_shader_uvAlongStroke.c
  shader/nodes/node_shader_uvmap.c
  shader/nodes/node_shader_valToRgb.cc
  shader/nodes/node_shader_value.cc
  shader/nodes/node_shader_vectTransform.c
  shader/nodes/node_shader_vector_displacement.c
  shader/nodes/node_shader_vector_math.cc
  shader/nodes/node_shader_vector_rotate.cc
  shader/nodes/node_shader_vertex_color.c
  shader/nodes/node_shader_volume_absorption.c
  shader/nodes/node_shader_volume_info.c
  shader/nodes/node_shader_volume_principled.c
  shader/nodes/node_shader_volume_scatter.c
  shader/nodes/node_shader_wavelength.c
  shader/nodes/node_shader_wireframe.c
  shader/node_shader_tree.c
  shader/node_shader_util.c

  texture/nodes/node_texture_at.c
  texture/nodes/node_texture_bricks.c
  texture/nodes/node_texture_checker.c
  texture/nodes/node_texture_common.c
  texture/nodes/node_texture_compose.c
  texture/nodes/node_texture_coord.c
  texture/nodes/node_texture_curves.c
  texture/nodes/node_texture_decompose.c
  texture/nodes/node_texture_distance.c
  texture/nodes/node_texture_hueSatVal.c
  texture/nodes/node_texture_image.c
  texture/nodes/node_texture_invert.c
  texture/nodes/node_texture_math.c
  texture/nodes/node_texture_mixRgb.c
  texture/nodes/node_texture_output.c
  texture/nodes/node_texture_proc.c
  texture/nodes/node_texture_rotate.c
  texture/nodes/node_texture_scale.c
  texture/nodes/node_texture_texture.c
  texture/nodes/node_texture_translate.c
  texture/nodes/node_texture_valToNor.c
  texture/nodes/node_texture_valToRgb.c
  texture/nodes/node_texture_viewer.c
  texture/node_texture_tree.c
  texture/node_texture_util.c

  intern/derived_node_tree.cc
  intern/extern_implementations.cc
  intern/geometry_nodes_eval_log.cc
  intern/math_functions.cc
  intern/node_common.cc
  intern/node_declaration.cc
  intern/node_exec.cc
  intern/node_geometry_exec.cc
  intern/node_multi_function.cc
  intern/node_socket.cc
  intern/node_socket_declarations.cc
  intern/node_tree_ref.cc
  intern/node_util.c
  intern/type_conversions.cc

  composite/node_composite_util.hh
  function/node_function_util.hh
  shader/node_shader_util.h
  geometry/node_geometry_util.hh
  texture/node_texture_util.h

  NOD_common.h
  NOD_composite.h
  NOD_derived_node_tree.hh
  NOD_function.h
  NOD_geometry.h
  NOD_geometry_exec.hh
  NOD_geometry_nodes_eval_log.hh
  NOD_math_functions.hh
  NOD_multi_function.hh
  NOD_node_declaration.hh
  NOD_node_tree_ref.hh
  NOD_shader.h
  NOD_socket.h
  NOD_socket_declarations.hh
  NOD_socket_declarations_geometry.hh
  NOD_static_types.h
  NOD_texture.h
  NOD_type_conversions.hh
  intern/node_common.h
  intern/node_exec.h
  intern/node_util.h
)

set(LIB
  bf_bmesh
  bf_functions
  bf_geometry
  bf_intern_sky
)

if(WITH_BULLET)
  list(APPEND INC_SYS
    ${BULLET_INCLUDE_DIRS}
    "../../../intern/rigidbody/"
  )
  if(NOT WITH_SYSTEM_BULLET)
  list(APPEND LIB
    extern_bullet
    )
  endif()

  list(APPEND LIB
    ${BULLET_LIBRARIES}
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_PYTHON)
  list(APPEND INC
    ../python
  )
  list(APPEND INC_SYS
    ${PYTHON_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${PYTHON_LINKFLAGS}
    ${PYTHON_LIBRARIES}
  )
  add_definitions(-DWITH_PYTHON)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_TBB)
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the stl versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_COMPOSITOR)
  list(APPEND INC
    ../compositor
  )
  add_definitions(-DWITH_COMPOSITOR)
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_GMP)
  add_definitions(-DWITH_GMP)

  list(APPEND INC_SYS
    ${GMP_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${GMP_LIBRARIES}
  )
endif()

if(WITH_OPENVDB)
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
endif()

blender_add_lib(bf_nodes "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
