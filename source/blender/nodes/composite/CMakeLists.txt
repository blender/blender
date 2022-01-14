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
  ..
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

  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../../makesdna/intern
)


set(SRC
  nodes/node_composite_alpha_over.cc
  nodes/node_composite_antialiasing.cc
  nodes/node_composite_bilateralblur.cc
  nodes/node_composite_blur.cc
  nodes/node_composite_bokehblur.cc
  nodes/node_composite_bokehimage.cc
  nodes/node_composite_boxmask.cc
  nodes/node_composite_brightness.cc
  nodes/node_composite_channel_matte.cc
  nodes/node_composite_chroma_matte.cc
  nodes/node_composite_color_matte.cc
  nodes/node_composite_color_spill.cc
  nodes/node_composite_colorbalance.cc
  nodes/node_composite_colorcorrection.cc
  nodes/node_composite_convert_color_space.cc
  nodes/node_composite_common.cc
  nodes/node_composite_composite.cc
  nodes/node_composite_cornerpin.cc
  nodes/node_composite_crop.cc
  nodes/node_composite_cryptomatte.cc
  nodes/node_composite_curves.cc
  nodes/node_composite_defocus.cc
  nodes/node_composite_denoise.cc
  nodes/node_composite_despeckle.cc
  nodes/node_composite_diff_matte.cc
  nodes/node_composite_dilate.cc
  nodes/node_composite_directionalblur.cc
  nodes/node_composite_displace.cc
  nodes/node_composite_distance_matte.cc
  nodes/node_composite_double_edge_mask.cc
  nodes/node_composite_ellipsemask.cc
  nodes/node_composite_exposure.cc
  nodes/node_composite_filter.cc
  nodes/node_composite_flip.cc
  nodes/node_composite_gamma.cc
  nodes/node_composite_glare.cc
  nodes/node_composite_hue_sat_val.cc
  nodes/node_composite_huecorrect.cc
  nodes/node_composite_id_mask.cc
  nodes/node_composite_image.cc
  nodes/node_composite_inpaint.cc
  nodes/node_composite_invert.cc
  nodes/node_composite_keying.cc
  nodes/node_composite_keyingscreen.cc
  nodes/node_composite_lensdist.cc
  nodes/node_composite_levels.cc
  nodes/node_composite_luma_matte.cc
  nodes/node_composite_map_range.cc
  nodes/node_composite_map_uv.cc
  nodes/node_composite_map_value.cc
  nodes/node_composite_mask.cc
  nodes/node_composite_math.cc
  nodes/node_composite_mixrgb.cc
  nodes/node_composite_movieclip.cc
  nodes/node_composite_moviedistortion.cc
  nodes/node_composite_normal.cc
  nodes/node_composite_normalize.cc
  nodes/node_composite_output_file.cc
  nodes/node_composite_pixelate.cc
  nodes/node_composite_planetrackdeform.cc
  nodes/node_composite_posterize.cc
  nodes/node_composite_premulkey.cc
  nodes/node_composite_rgb.cc
  nodes/node_composite_rotate.cc
  nodes/node_composite_scale.cc
  nodes/node_composite_sepcomb_hsva.cc
  nodes/node_composite_sepcomb_rgba.cc
  nodes/node_composite_sepcomb_ycca.cc
  nodes/node_composite_sepcomb_yuva.cc
  nodes/node_composite_setalpha.cc
  nodes/node_composite_split_viewer.cc
  nodes/node_composite_stabilize2d.cc
  nodes/node_composite_sunbeams.cc
  nodes/node_composite_switch.cc
  nodes/node_composite_switchview.cc
  nodes/node_composite_texture.cc
  nodes/node_composite_tonemap.cc
  nodes/node_composite_trackpos.cc
  nodes/node_composite_transform.cc
  nodes/node_composite_translate.cc
  nodes/node_composite_val_to_rgb.cc
  nodes/node_composite_value.cc
  nodes/node_composite_vec_blur.cc
  nodes/node_composite_viewer.cc
  nodes/node_composite_zcombine.cc
  nodes/node_composite_scene_time.cc

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

if(WITH_UNITY_BUILD)
  set_target_properties(bf_nodes_composite PROPERTIES UNITY_BUILD ON)
  set_target_properties(bf_nodes_composite PROPERTIES UNITY_BUILD_BATCH_SIZE 10)
endif()

# Needed so we can use dna_type_offsets.h for defaults initialization.
add_dependencies(bf_nodes_composite bf_dna)
