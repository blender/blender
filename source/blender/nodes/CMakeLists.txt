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
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC 
	.
	../blenfont
	../blenkernel
	../blenlib
	../blenloader
	../gpu
	../imbuf
	../makesdna
	../makesrna
	../render/extern/include
	../../../intern/guardedalloc
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
	intern
	composite
	shader
	texture
)

set(SRC
	composite/nodes/node_composite_alphaOver.c
	composite/nodes/node_composite_bilateralblur.c
	composite/nodes/node_composite_blur.c
	composite/nodes/node_composite_brightness.c
	composite/nodes/node_composite_channelMatte.c
	composite/nodes/node_composite_chromaMatte.c
	composite/nodes/node_composite_colorMatte.c
	composite/nodes/node_composite_colorSpill.c
	composite/nodes/node_composite_colorbalance.c
	composite/nodes/node_composite_common.c
	composite/nodes/node_composite_composite.c
	composite/nodes/node_composite_crop.c
	composite/nodes/node_composite_curves.c
	composite/nodes/node_composite_doubleEdgeMask.c
	composite/nodes/node_composite_defocus.c
	composite/nodes/node_composite_diffMatte.c
	composite/nodes/node_composite_dilate.c
	composite/nodes/node_composite_directionalblur.c
	composite/nodes/node_composite_displace.c
	composite/nodes/node_composite_distanceMatte.c
	composite/nodes/node_composite_filter.c
	composite/nodes/node_composite_flip.c
	composite/nodes/node_composite_gamma.c
	composite/nodes/node_composite_glare.c
	composite/nodes/node_composite_hueSatVal.c
	composite/nodes/node_composite_huecorrect.c
	composite/nodes/node_composite_idMask.c
	composite/nodes/node_composite_image.c
	composite/nodes/node_composite_invert.c
	composite/nodes/node_composite_lensdist.c
	composite/nodes/node_composite_levels.c
	composite/nodes/node_composite_lummaMatte.c
	composite/nodes/node_composite_mapUV.c
	composite/nodes/node_composite_mapValue.c
	composite/nodes/node_composite_math.c
	composite/nodes/node_composite_mixrgb.c
	composite/nodes/node_composite_movieclip.c
	composite/nodes/node_composite_moviedistortion.c
	composite/nodes/node_composite_normal.c
	composite/nodes/node_composite_normalize.c
	composite/nodes/node_composite_outputFile.c
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
	composite/nodes/node_composite_texture.c
	composite/nodes/node_composite_tonemap.c
	composite/nodes/node_composite_transform.c
	composite/nodes/node_composite_translate.c
	composite/nodes/node_composite_valToRgb.c
	composite/nodes/node_composite_value.c
	composite/nodes/node_composite_vecBlur.c
	composite/nodes/node_composite_viewer.c
	composite/nodes/node_composite_zcombine.c
	composite/node_composite_tree.c
	composite/node_composite_util.c

	shader/nodes/node_shader_camera.c
	shader/nodes/node_shader_common.c
	shader/nodes/node_shader_curves.c
	shader/nodes/node_shader_dynamic.c
	shader/nodes/node_shader_gamma.c
	shader/nodes/node_shader_brightness.c
	shader/nodes/node_shader_geom.c
	shader/nodes/node_shader_hueSatVal.c
	shader/nodes/node_shader_invert.c
	shader/nodes/node_shader_mapping.c
	shader/nodes/node_shader_material.c
	shader/nodes/node_shader_math.c
	shader/nodes/node_shader_mixRgb.c
	shader/nodes/node_shader_normal.c
	shader/nodes/node_shader_output.c
	shader/nodes/node_shader_rgb.c
	shader/nodes/node_shader_sepcombRGB.c
	shader/nodes/node_shader_squeeze.c
	shader/nodes/node_shader_texture.c
	shader/nodes/node_shader_valToRgb.c
	shader/nodes/node_shader_value.c
	shader/nodes/node_shader_vectMath.c
	shader/nodes/node_shader_attribute.c
	shader/nodes/node_shader_background.c
	# shader/nodes/node_shader_bsdf_anisotropic.c  # XXX, why not included?
	shader/nodes/node_shader_bsdf_diffuse.c
	shader/nodes/node_shader_bsdf_glossy.c
	shader/nodes/node_shader_bsdf_glass.c
	shader/nodes/node_shader_bsdf_translucent.c
	shader/nodes/node_shader_bsdf_transparent.c
	shader/nodes/node_shader_bsdf_velvet.c
	shader/nodes/node_shader_emission.c
	shader/nodes/node_shader_fresnel.c
	shader/nodes/node_shader_layer_weight.c
	shader/nodes/node_shader_geometry.c
	shader/nodes/node_shader_holdout.c
	shader/nodes/node_shader_volume_transparent.c
	shader/nodes/node_shader_volume_isotropic.c
	shader/nodes/node_shader_light_path.c
	shader/nodes/node_shader_mix_shader.c
	shader/nodes/node_shader_add_shader.c
	shader/nodes/node_shader_output_lamp.c
	shader/nodes/node_shader_output_material.c
	shader/nodes/node_shader_output_world.c
	shader/nodes/node_shader_tex_gradient.c
	shader/nodes/node_shader_tex_coord.c
	shader/nodes/node_shader_tex_environment.c
	shader/nodes/node_shader_tex_image.c
	shader/nodes/node_shader_tex_magic.c
	shader/nodes/node_shader_tex_musgrave.c
	shader/nodes/node_shader_tex_noise.c
	shader/nodes/node_shader_tex_sky.c
	shader/nodes/node_shader_tex_voronoi.c
	shader/nodes/node_shader_tex_wave.c
	shader/nodes/node_shader_tex_checker.c
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

	intern/node_util.c
	intern/node_exec.c
	intern/node_common.c
	intern/node_socket.c

	composite/node_composite_util.h
	shader/node_shader_util.h
	texture/node_texture_util.h

	NOD_composite.h
	NOD_shader.h
	NOD_texture.h
	NOD_socket.h
	intern/node_util.h
	intern/node_exec.h
	intern/node_common.h
)

if(WITH_PYTHON)
	list(APPEND INC
		../python
	)
	list(APPEND INC_SYS
		${PYTHON_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_PYTHON)
endif()

blender_add_lib(bf_nodes "${SRC}" "${INC}" "${INC_SYS}")
