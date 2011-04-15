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
	../blenkernel
	../blenlib
	../blenloader
	../gpu
	../imbuf
	../makesdna
	../makesrna
	../render/extern/include
	../../../intern/guardedalloc
	${GLEW_INCLUDE_PATH}
)

set(SRC
	intern/CMP_nodes/CMP_alphaOver.c
	intern/CMP_nodes/CMP_bilateralblur.c
	intern/CMP_nodes/CMP_blur.c
	intern/CMP_nodes/CMP_brightness.c
	intern/CMP_nodes/CMP_channelMatte.c
	intern/CMP_nodes/CMP_chromaMatte.c
	intern/CMP_nodes/CMP_colorMatte.c
	intern/CMP_nodes/CMP_colorSpill.c
	intern/CMP_nodes/CMP_colorbalance.c
	intern/CMP_nodes/CMP_composite.c
	intern/CMP_nodes/CMP_crop.c
	intern/CMP_nodes/CMP_curves.c
	intern/CMP_nodes/CMP_defocus.c
	intern/CMP_nodes/CMP_diffMatte.c
	intern/CMP_nodes/CMP_dilate.c
	intern/CMP_nodes/CMP_directionalblur.c
	intern/CMP_nodes/CMP_displace.c
	intern/CMP_nodes/CMP_distanceMatte.c
	intern/CMP_nodes/CMP_filter.c
	intern/CMP_nodes/CMP_flip.c
	intern/CMP_nodes/CMP_gamma.c
	intern/CMP_nodes/CMP_glare.c
	intern/CMP_nodes/CMP_hueSatVal.c
	intern/CMP_nodes/CMP_huecorrect.c
	intern/CMP_nodes/CMP_idMask.c
	intern/CMP_nodes/CMP_image.c
	intern/CMP_nodes/CMP_invert.c
	intern/CMP_nodes/CMP_lensdist.c
	intern/CMP_nodes/CMP_levels.c
	intern/CMP_nodes/CMP_lummaMatte.c
	intern/CMP_nodes/CMP_mapUV.c
	intern/CMP_nodes/CMP_mapValue.c
	intern/CMP_nodes/CMP_math.c
	intern/CMP_nodes/CMP_mixrgb.c
	intern/CMP_nodes/CMP_normal.c
	intern/CMP_nodes/CMP_normalize.c
	intern/CMP_nodes/CMP_outputFile.c
	intern/CMP_nodes/CMP_premulkey.c
	intern/CMP_nodes/CMP_rgb.c
	intern/CMP_nodes/CMP_rotate.c
	intern/CMP_nodes/CMP_scale.c
	intern/CMP_nodes/CMP_sepcombHSVA.c
	intern/CMP_nodes/CMP_sepcombRGBA.c
	intern/CMP_nodes/CMP_sepcombYCCA.c
	intern/CMP_nodes/CMP_sepcombYUVA.c
	intern/CMP_nodes/CMP_setalpha.c
	intern/CMP_nodes/CMP_splitViewer.c
	intern/CMP_nodes/CMP_texture.c
	intern/CMP_nodes/CMP_tonemap.c
	intern/CMP_nodes/CMP_translate.c
	intern/CMP_nodes/CMP_valToRgb.c
	intern/CMP_nodes/CMP_value.c
	intern/CMP_nodes/CMP_vecBlur.c
	intern/CMP_nodes/CMP_viewer.c
	intern/CMP_nodes/CMP_zcombine.c
	intern/CMP_util.c
	intern/SHD_nodes/SHD_camera.c
	intern/SHD_nodes/SHD_curves.c
	intern/SHD_nodes/SHD_dynamic.c
	intern/SHD_nodes/SHD_geom.c
	intern/SHD_nodes/SHD_hueSatVal.c
	intern/SHD_nodes/SHD_invert.c
	intern/SHD_nodes/SHD_mapping.c
	intern/SHD_nodes/SHD_material.c
	intern/SHD_nodes/SHD_math.c
	intern/SHD_nodes/SHD_mixRgb.c
	intern/SHD_nodes/SHD_normal.c
	intern/SHD_nodes/SHD_output.c
	intern/SHD_nodes/SHD_rgb.c
	intern/SHD_nodes/SHD_sepcombRGB.c
	intern/SHD_nodes/SHD_squeeze.c
	intern/SHD_nodes/SHD_texture.c
	intern/SHD_nodes/SHD_valToRgb.c
	intern/SHD_nodes/SHD_value.c
	intern/SHD_nodes/SHD_vectMath.c
	intern/SHD_util.c
	intern/TEX_nodes/TEX_at.c
	intern/TEX_nodes/TEX_bricks.c
	intern/TEX_nodes/TEX_checker.c
	intern/TEX_nodes/TEX_compose.c
	intern/TEX_nodes/TEX_coord.c
	intern/TEX_nodes/TEX_curves.c
	intern/TEX_nodes/TEX_decompose.c
	intern/TEX_nodes/TEX_distance.c
	intern/TEX_nodes/TEX_hueSatVal.c
	intern/TEX_nodes/TEX_image.c
	intern/TEX_nodes/TEX_invert.c
	intern/TEX_nodes/TEX_math.c
	intern/TEX_nodes/TEX_mixRgb.c
	intern/TEX_nodes/TEX_output.c
	intern/TEX_nodes/TEX_proc.c
	intern/TEX_nodes/TEX_rotate.c
	intern/TEX_nodes/TEX_scale.c
	intern/TEX_nodes/TEX_texture.c
	intern/TEX_nodes/TEX_translate.c
	intern/TEX_nodes/TEX_valToNor.c
	intern/TEX_nodes/TEX_valToRgb.c
	intern/TEX_nodes/TEX_viewer.c
	intern/TEX_util.c
	intern/node_util.c

	CMP_node.h
	SHD_node.h
	TEX_node.h
	intern/CMP_util.h
	intern/SHD_util.h
	intern/TEX_util.h
	intern/node_util.h
)

if(WITH_PYTHON)
	set(INC ${INC} ../python ${PYTHON_INCLUDE_DIRS})
	add_definitions(-DWITH_PYTHON)
endif()

blender_add_lib(bf_nodes "${SRC}" "${INC}")

