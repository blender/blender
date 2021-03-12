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
  ../gpu
  ../imbuf
  ../makesdna
  ../makesrna
  ../render
  ../../../intern/glew-mx
  ../../../intern/guardedalloc
  ../../../intern/sky/include
)


set(SRC
  composite/nodes/node_composite_alphaOver.c
  composite/nodes/node_composite_bilateralblur.c
  composite/nodes/node_composite_blur.c
  composite/nodes/node_composite_bokehblur.c
  composite/nodes/node_composite_bokehimage.c
  composite/nodes/node_composite_boxmask.c
  composite/nodes/node_composite_brightness.c
  composite/nodes/node_composite_channelMatte.c
  composite/nodes/node_composite_chromaMatte.c
  composite/nodes/node_composite_colorMatte.c
  composite/nodes/node_composite_colorSpill.c
  composite/nodes/node_composite_colorbalance.c
  composite/nodes/node_composite_colorcorrection.c
  composite/nodes/node_composite_common.c
  composite/nodes/node_composite_composite.c
  composite/nodes/node_composite_cornerpin.c
  composite/nodes/node_composite_crop.c
  composite/nodes/node_composite_cryptomatte.cc
  composite/nodes/node_composite_curves.c
  composite/nodes/node_composite_defocus.c
  composite/nodes/node_composite_denoise.c
  composite/nodes/node_composite_despeckle.c
  composite/nodes/node_composite_diffMatte.c
  composite/nodes/node_composite_dilate.c
  composite/nodes/node_composite_directionalblur.c
  composite/nodes/node_composite_displace.c
  composite/nodes/node_composite_distanceMatte.c
  composite/nodes/node_composite_doubleEdgeMask.c
  composite/nodes/node_composite_ellipsemask.c
  composite/nodes/node_composite_exposure.c
  composite/nodes/node_composite_filter.c
  composite/nodes/node_composite_flip.c
  composite/nodes/node_composite_gamma.c
  composite/nodes/node_composite_glare.c
  composite/nodes/node_composite_hueSatVal.c
  composite/nodes/node_composite_huecorrect.c
  composite/nodes/node_composite_idMask.c
  composite/nodes/node_composite_image.c
  composite/nodes/node_composite_inpaint.c
  composite/nodes/node_composite_invert.c
  composite/nodes/node_composite_keying.c
  composite/nodes/node_composite_keyingscreen.c
  composite/nodes/node_composite_lensdist.c
  composite/nodes/node_composite_levels.c
  composite/nodes/node_composite_lummaMatte.c
  composite/nodes/node_composite_mapRange.c
  composite/nodes/node_composite_mapUV.c
  composite/nodes/node_composite_mapValue.c
  composite/nodes/node_composite_mask.c
  composite/nodes/node_composite_math.c
  composite/nodes/node_composite_mixrgb.c
  composite/nodes/node_composite_movieclip.c
  composite/nodes/node_composite_moviedistortion.c
  composite/nodes/node_composite_normal.c
  composite/nodes/node_composite_normalize.c
  composite/nodes/node_composite_outputFile.c
  composite/nodes/node_composite_pixelate.c
  composite/nodes/node_composite_planetrackdeform.c
  composite/nodes/node_composite_premulkey.c
  composite/nodes/node_composite_rgb.c
  composite/nodes/node_composite_rotate.c
  composite/nodes/node_composite_scale.c
  composite/nodes/node_composite_sepcombHSVA.c
  composite/nodes/node_composite_sepcombRGBA.c
  composite/nodes/node_composite_sepcombYCCA.c
  composite/nodes/node_composite_sepcombYUVA.c
  composite/nodes/node_composite_setalpha.c
  composite/nodes/node_composite_splitViewer.c
  composite/nodes/node_composite_stabilize2d.c
  composite/nodes/node_composite_sunbeams.c
  composite/nodes/node_composite_switch.c
  composite/nodes/node_composite_switchview.c
  composite/nodes/node_composite_texture.c
  composite/nodes/node_composite_tonemap.c
  composite/nodes/node_composite_trackpos.c
  composite/nodes/node_composite_transform.c
  composite/nodes/node_composite_translate.c
  composite/nodes/node_composite_valToRgb.c
  composite/nodes/node_composite_value.c
  composite/nodes/node_composite_vecBlur.c
  composite/nodes/node_composite_viewer.c
  composite/nodes/node_composite_zcombine.c

  composite/node_composite_tree.c
  composite/node_composite_util.c

  function/nodes/node_fn_boolean_math.cc
  function/nodes/node_fn_combine_strings.cc
  function/nodes/node_fn_float_compare.cc
  function/nodes/node_fn_group_instance_id.cc
  function/nodes/node_fn_input_string.cc
  function/nodes/node_fn_input_vector.cc
  function/nodes/node_fn_object_transforms.cc
  function/nodes/node_fn_random_float.cc
  function/nodes/node_fn_switch.cc
  function/node_function_util.cc

  geometry/nodes/node_geo_align_rotation_to_vector.cc
  geometry/nodes/node_geo_attribute_color_ramp.cc
  geometry/nodes/node_geo_attribute_combine_xyz.cc
  geometry/nodes/node_geo_attribute_compare.cc
  geometry/nodes/node_geo_attribute_fill.cc
  geometry/nodes/node_geo_attribute_math.cc
  geometry/nodes/node_geo_attribute_mix.cc
  geometry/nodes/node_geo_attribute_proximity.cc
  geometry/nodes/node_geo_attribute_randomize.cc
  geometry/nodes/node_geo_attribute_sample_texture.cc
  geometry/nodes/node_geo_attribute_separate_xyz.cc
  geometry/nodes/node_geo_attribute_vector_math.cc
  geometry/nodes/node_geo_attribute_remove.cc
  geometry/nodes/node_geo_boolean.cc
  geometry/nodes/node_geo_collection_info.cc
  geometry/nodes/node_geo_common.cc
  geometry/nodes/node_geo_edge_split.cc
  geometry/nodes/node_geo_is_viewport.cc
  geometry/nodes/node_geo_join_geometry.cc
  geometry/nodes/node_geo_object_info.cc
  geometry/nodes/node_geo_point_distribute.cc
  geometry/nodes/node_geo_point_instance.cc
  geometry/nodes/node_geo_point_rotate.cc
  geometry/nodes/node_geo_point_scale.cc
  geometry/nodes/node_geo_point_separate.cc
  geometry/nodes/node_geo_point_translate.cc
  geometry/nodes/node_geo_points_to_volume.cc
  geometry/nodes/node_geo_subdivide_smooth.cc
  geometry/nodes/node_geo_subdivide.cc
  geometry/nodes/node_geo_transform.cc
  geometry/nodes/node_geo_triangulate.cc
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
  shader/nodes/node_shader_curves.c
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
  shader/nodes/node_shader_mixRgb.c
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
  shader/nodes/node_shader_tex_brick.c
  shader/nodes/node_shader_tex_checker.c
  shader/nodes/node_shader_tex_coord.c
  shader/nodes/node_shader_tex_environment.c
  shader/nodes/node_shader_tex_gradient.c
  shader/nodes/node_shader_tex_image.c
  shader/nodes/node_shader_tex_magic.c
  shader/nodes/node_shader_tex_musgrave.c
  shader/nodes/node_shader_tex_noise.c
  shader/nodes/node_shader_tex_pointdensity.c
  shader/nodes/node_shader_tex_sky.c
  shader/nodes/node_shader_tex_voronoi.c
  shader/nodes/node_shader_tex_wave.c
  shader/nodes/node_shader_tex_white_noise.c
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
  intern/math_functions.cc
  intern/node_common.c
  intern/node_exec.c
  intern/node_geometry_exec.cc
  intern/node_socket.cc
  intern/node_tree_multi_function.cc
  intern/node_tree_ref.cc
  intern/node_util.c
  intern/type_callbacks.cc

  composite/node_composite_util.h
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
  NOD_math_functions.hh
  NOD_node_tree_multi_function.hh
  NOD_node_tree_ref.hh
  NOD_shader.h
  NOD_socket.h
  NOD_static_types.h
  NOD_texture.h
  NOD_type_callbacks.hh
  intern/node_common.h
  intern/node_exec.h
  intern/node_util.h
)

set(LIB
  bf_bmesh
  bf_functions
  bf_intern_sky
)

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

if(WITH_OPENVDB)
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
endif()

blender_add_lib(bf_nodes "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
