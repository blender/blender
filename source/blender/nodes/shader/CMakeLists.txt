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
# The Original Code is Copyright (C) 2021, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../
  ../intern
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../depsgraph
  ../../editors/include
  ../../functions
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../render
  ../../windowmanager
  ../../../../intern/guardedalloc
  ../../../../intern/sky/include
)


set(SRC
  nodes/node_shader_add_shader.cc
  nodes/node_shader_ambient_occlusion.c
  nodes/node_shader_attribute.c
  nodes/node_shader_background.c
  nodes/node_shader_bevel.c
  nodes/node_shader_blackbody.c
  nodes/node_shader_brightness.c
  nodes/node_shader_bsdf_anisotropic.cc
  nodes/node_shader_bsdf_diffuse.cc
  nodes/node_shader_bsdf_glass.cc
  nodes/node_shader_bsdf_glossy.cc
  nodes/node_shader_bsdf_hair.cc
  nodes/node_shader_bsdf_hair_principled.cc
  nodes/node_shader_bsdf_principled.cc
  nodes/node_shader_bsdf_refraction.cc
  nodes/node_shader_bsdf_toon.cc
  nodes/node_shader_bsdf_translucent.cc
  nodes/node_shader_bsdf_transparent.cc
  nodes/node_shader_bsdf_velvet.cc
  nodes/node_shader_bump.cc
  nodes/node_shader_camera.c
  nodes/node_shader_clamp.cc
  nodes/node_shader_color_ramp.cc
  nodes/node_shader_common.c
  nodes/node_shader_curves.cc
  nodes/node_shader_displacement.c
  nodes/node_shader_eevee_specular.c
  nodes/node_shader_emission.c
  nodes/node_shader_fresnel.c
  nodes/node_shader_gamma.c
  nodes/node_shader_geometry.c
  nodes/node_shader_hair_info.c
  nodes/node_shader_holdout.c
  nodes/node_shader_hueSatVal.c
  nodes/node_shader_ies_light.cc
  nodes/node_shader_invert.c
  nodes/node_shader_layer_weight.c
  nodes/node_shader_light_falloff.c
  nodes/node_shader_light_path.c
  nodes/node_shader_map_range.cc
  nodes/node_shader_mapping.c
  nodes/node_shader_math.cc
  nodes/node_shader_mix_rgb.cc
  nodes/node_shader_mix_shader.cc
  nodes/node_shader_normal.c
  nodes/node_shader_normal_map.c
  nodes/node_shader_object_info.c
  nodes/node_shader_output_aov.c
  nodes/node_shader_output_light.c
  nodes/node_shader_output_linestyle.c
  nodes/node_shader_output_material.c
  nodes/node_shader_output_world.c
  nodes/node_shader_particle_info.c
  nodes/node_shader_rgb_to_bw.cc
  nodes/node_shader_rgb.c
  nodes/node_shader_script.c
  nodes/node_shader_sepcomb_hsv.c
  nodes/node_shader_sepcomb_rgb.cc
  nodes/node_shader_sepcomb_xyz.cc
  nodes/node_shader_shader_to_rgb.cc
  nodes/node_shader_squeeze.c
  nodes/node_shader_subsurface_scattering.cc
  nodes/node_shader_tangent.c
  nodes/node_shader_tex_brick.cc
  nodes/node_shader_tex_checker.cc
  nodes/node_shader_tex_coord.c
  nodes/node_shader_tex_environment.cc
  nodes/node_shader_tex_gradient.cc
  nodes/node_shader_tex_image.cc
  nodes/node_shader_tex_magic.cc
  nodes/node_shader_tex_musgrave.cc
  nodes/node_shader_tex_noise.cc
  nodes/node_shader_tex_pointdensity.cc
  nodes/node_shader_tex_sky.cc
  nodes/node_shader_tex_voronoi.cc
  nodes/node_shader_tex_wave.cc
  nodes/node_shader_tex_white_noise.cc
  nodes/node_shader_uv_along_stroke.c
  nodes/node_shader_uvmap.c
  nodes/node_shader_value.cc
  nodes/node_shader_vector_displacement.c
  nodes/node_shader_vector_math.cc
  nodes/node_shader_vector_rotate.cc
  nodes/node_shader_vector_transform.c
  nodes/node_shader_vertex_color.c
  nodes/node_shader_volume_absorption.cc
  nodes/node_shader_volume_info.c
  nodes/node_shader_volume_principled.cc
  nodes/node_shader_volume_scatter.cc
  nodes/node_shader_wavelength.c
  nodes/node_shader_wireframe.c

  node_shader_tree.c
  node_shader_util.cc

  node_shader_util.h
)

set(LIB
  bf_functions
  bf_intern_sky
)

if(WITH_PYTHON)
  list(APPEND INC
    ../../python
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

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

blender_add_lib(bf_nodes_shader "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
