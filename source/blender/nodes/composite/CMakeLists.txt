# SPDX-FileCopyrightText: 2021 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  include
  ../intern
  ../../editors/include
  ../../compositor
  ../../makesrna
  ../../compositor/algorithms
  ../../compositor/cached_resources
  ../../compositor/derived_resources
  ../../compositor/utilities
  ../../sequencer

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
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
  nodes/node_composite_common.cc
  nodes/node_composite_convert_color_space.cc
  nodes/node_composite_convert_to_display.cc
  nodes/node_composite_convolve.cc
  nodes/node_composite_cornerpin.cc
  nodes/node_composite_crop.cc
  nodes/node_composite_cryptomatte.cc
  nodes/node_composite_curves.cc
  nodes/node_composite_defocus.cc
  nodes/node_composite_denoise.cc
  nodes/node_composite_depth_combine.cc
  nodes/node_composite_despeckle.cc
  nodes/node_composite_diff_matte.cc
  nodes/node_composite_dilate.cc
  nodes/node_composite_directionalblur.cc
  nodes/node_composite_displace.cc
  nodes/node_composite_distance_matte.cc
  nodes/node_composite_double_edge_mask.cc
  nodes/node_composite_ellipsemask.cc
  nodes/node_composite_exposure.cc
  nodes/node_composite_file_output.cc
  nodes/node_composite_filter.cc
  nodes/node_composite_flip.cc
  nodes/node_composite_glare.cc
  nodes/node_composite_group_input.cc
  nodes/node_composite_group_output.cc
  nodes/node_composite_hue_sat_val.cc
  nodes/node_composite_huecorrect.cc
  nodes/node_composite_id_mask.cc
  nodes/node_composite_image.cc
  nodes/node_composite_image_coordinates.cc
  nodes/node_composite_image_info.cc
  nodes/node_composite_inpaint.cc
  nodes/node_composite_invert.cc
  nodes/node_composite_keying.cc
  nodes/node_composite_keyingscreen.cc
  nodes/node_composite_kuwahara.cc
  nodes/node_composite_lensdist.cc
  nodes/node_composite_levels.cc
  nodes/node_composite_luma_matte.cc
  nodes/node_composite_map_uv.cc
  nodes/node_composite_mask.cc
  nodes/node_composite_movieclip.cc
  nodes/node_composite_moviedistortion.cc
  nodes/node_composite_normal.cc
  nodes/node_composite_normalize.cc
  nodes/node_composite_pixelate.cc
  nodes/node_composite_planetrackdeform.cc
  nodes/node_composite_posterize.cc
  nodes/node_composite_premulkey.cc
  nodes/node_composite_relative_to_pixel.cc
  nodes/node_composite_render_layers.cc
  nodes/node_composite_rgb.cc
  nodes/node_composite_rotate.cc
  nodes/node_composite_scale.cc
  nodes/node_composite_scene_time.cc
  nodes/node_composite_sepcomb_color.cc
  nodes/node_composite_setalpha.cc
  nodes/node_composite_split.cc
  nodes/node_composite_stabilize2d.cc
  nodes/node_composite_strip_info.cc
  nodes/node_composite_switch.cc
  nodes/node_composite_switchview.cc
  nodes/node_composite_tonemap.cc
  nodes/node_composite_trackpos.cc
  nodes/node_composite_transform.cc
  nodes/node_composite_translate.cc
  nodes/node_composite_val_to_rgb.cc
  nodes/node_composite_vec_blur.cc
  nodes/node_composite_viewer.cc

  node_composite_tree.cc
  node_composite_util.cc

  include/NOD_compositor_file_output.hh
  node_composite_util.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::blenloader
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::functions
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::extern::fmtlib
  bf_compositor
  PRIVATE bf::render
  PRIVATE bf::windowmanager
  PRIVATE bf::dependencies::optional::opencolorio
  PRIVATE bf::sequencer
)

add_node_discovery(
  bf_nodes_compositor_generated
  "${SRC}"
  ${CMAKE_CURRENT_BINARY_DIR}/register_compositor_nodes.cc
  register_compositor_nodes
)

list(APPEND LIB
  bf_nodes_compositor_generated
)

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_IMAGE_OPENEXR)
endif()

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
  add_definitions(-DOIDN_STATIC_LIB)
  list(APPEND INC_SYS
    ${OPENIMAGEDENOISE_INCLUDE_DIRS}
    ${TBB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENIMAGEDENOISE_LIBRARIES}
    ${TBB_LIBRARIES}
  )
endif()


if(WITH_FFTW3)
  list(APPEND INC_SYS
    ${FFTW3_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${FFTW3_LIBRARIES}
  )
  add_definitions(-DWITH_FFTW3)
endif()

blender_add_lib(bf_nodes_composite "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
target_link_libraries(bf_nodes_compositor_generated bf_nodes_composite)

blender_set_target_unity_build(bf_nodes_composite 10)

# RNA_prototypes.hh
add_dependencies(bf_nodes_composite bf_rna)
