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
  ../../editors/include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../depsgraph
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../render
  ../../windowmanager
  ../../../../intern/guardedalloc
)


set(SRC
  nodes/node_composite_alphaOver.cc
  nodes/node_composite_antialiasing.cc
  nodes/node_composite_bilateralblur.cc
  nodes/node_composite_blur.cc
  nodes/node_composite_bokehblur.cc
  nodes/node_composite_bokehimage.cc
  nodes/node_composite_boxmask.cc
  nodes/node_composite_brightness.cc
  nodes/node_composite_channelMatte.cc
  nodes/node_composite_chromaMatte.cc
  nodes/node_composite_colorMatte.cc
  nodes/node_composite_colorSpill.cc
  nodes/node_composite_colorbalance.cc
  nodes/node_composite_colorcorrection.cc
  nodes/node_composite_common.cc
  nodes/node_composite_composite.cc
  nodes/node_composite_cornerpin.cc
  nodes/node_composite_crop.cc
  nodes/node_composite_cryptomatte.cc
  nodes/node_composite_curves.cc
  nodes/node_composite_defocus.cc
  nodes/node_composite_denoise.cc
  nodes/node_composite_despeckle.cc
  nodes/node_composite_diffMatte.cc
  nodes/node_composite_dilate.cc
  nodes/node_composite_directionalblur.cc
  nodes/node_composite_displace.cc
  nodes/node_composite_distanceMatte.cc
  nodes/node_composite_doubleEdgeMask.cc
  nodes/node_composite_ellipsemask.cc
  nodes/node_composite_exposure.cc
  nodes/node_composite_filter.cc
  nodes/node_composite_flip.cc
  nodes/node_composite_gamma.cc
  nodes/node_composite_glare.cc
  nodes/node_composite_hueSatVal.cc
  nodes/node_composite_huecorrect.cc
  nodes/node_composite_idMask.cc
  nodes/node_composite_image.cc
  nodes/node_composite_inpaint.cc
  nodes/node_composite_invert.cc
  nodes/node_composite_keying.cc
  nodes/node_composite_keyingscreen.cc
  nodes/node_composite_lensdist.cc
  nodes/node_composite_levels.cc
  nodes/node_composite_lummaMatte.cc
  nodes/node_composite_mapRange.cc
  nodes/node_composite_mapUV.cc
  nodes/node_composite_mapValue.cc
  nodes/node_composite_mask.cc
  nodes/node_composite_math.cc
  nodes/node_composite_mixrgb.cc
  nodes/node_composite_movieclip.cc
  nodes/node_composite_moviedistortion.cc
  nodes/node_composite_normal.cc
  nodes/node_composite_normalize.cc
  nodes/node_composite_outputFile.cc
  nodes/node_composite_pixelate.cc
  nodes/node_composite_planetrackdeform.cc
  nodes/node_composite_posterize.cc
  nodes/node_composite_premulkey.cc
  nodes/node_composite_rgb.cc
  nodes/node_composite_rotate.cc
  nodes/node_composite_scale.cc
  nodes/node_composite_sepcombHSVA.cc
  nodes/node_composite_sepcombRGBA.cc
  nodes/node_composite_sepcombYCCA.cc
  nodes/node_composite_sepcombYUVA.cc
  nodes/node_composite_setalpha.cc
  nodes/node_composite_splitViewer.cc
  nodes/node_composite_stabilize2d.cc
  nodes/node_composite_sunbeams.cc
  nodes/node_composite_switch.cc
  nodes/node_composite_switchview.cc
  nodes/node_composite_texture.cc
  nodes/node_composite_tonemap.cc
  nodes/node_composite_trackpos.cc
  nodes/node_composite_transform.cc
  nodes/node_composite_translate.cc
  nodes/node_composite_valToRgb.cc
  nodes/node_composite_value.cc
  nodes/node_composite_vecBlur.cc
  nodes/node_composite_viewer.cc
  nodes/node_composite_zcombine.cc

  node_composite_tree.cc
  node_composite_util.cc

  node_composite_util.hh
)

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_COMPOSITOR)
  list(APPEND INC
    ../../compositor
  )
  add_definitions(-DWITH_COMPOSITOR)
endif()

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
endif()

blender_add_lib(bf_nodes_composite "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
