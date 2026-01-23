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
  nodes/node_composite_alpha_convert.cc
  nodes/node_composite_alpha_over.cc
  nodes/node_composite_anti_aliasing.cc
  nodes/node_composite_bilateral_blur.cc
  nodes/node_composite_blur.cc
  nodes/node_composite_bokeh_blur.cc
  nodes/node_composite_bokeh_image.cc
  nodes/node_composite_box_mask.cc
  nodes/node_composite_brightness_contrast.cc
  nodes/node_composite_channel_key.cc
  nodes/node_composite_chroma_key.cc
  nodes/node_composite_color.cc
  nodes/node_composite_color_balance.cc
  nodes/node_composite_color_correction.cc
  nodes/node_composite_color_key.cc
  nodes/node_composite_color_spill.cc
  nodes/node_composite_combine_color.cc
  nodes/node_composite_convert_color_space.cc
  nodes/node_composite_convert_to_display.cc
  nodes/node_composite_convolve.cc
  nodes/node_composite_corner_pin.cc
  nodes/node_composite_crop.cc
  nodes/node_composite_cryptomatte.cc
  nodes/node_composite_defocus.cc
  nodes/node_composite_denoise.cc
  nodes/node_composite_depth_combine.cc
  nodes/node_composite_despeckle.cc
  nodes/node_composite_difference_key.cc
  nodes/node_composite_dilate_erode.cc
  nodes/node_composite_directional_blur.cc
  nodes/node_composite_displace.cc
  nodes/node_composite_distance_key.cc
  nodes/node_composite_double_edge_mask.cc
  nodes/node_composite_ellipse_mask.cc
  nodes/node_composite_exposure.cc
  nodes/node_composite_file_output.cc
  nodes/node_composite_filter.cc
  nodes/node_composite_flip.cc
  nodes/node_composite_glare.cc
  nodes/node_composite_group.cc
  nodes/node_composite_hue_correct.cc
  nodes/node_composite_hue_saturation_value.cc
  nodes/node_composite_id_mask.cc
  nodes/node_composite_image.cc
  nodes/node_composite_image_coordinates.cc
  nodes/node_composite_image_info.cc
  nodes/node_composite_inpaint.cc
  nodes/node_composite_invert.cc
  nodes/node_composite_keying.cc
  nodes/node_composite_keying_screen.cc
  nodes/node_composite_kuwahara.cc
  nodes/node_composite_lens_distortion.cc
  nodes/node_composite_levels.cc
  nodes/node_composite_luminance_key.cc
  nodes/node_composite_map_uv.cc
  nodes/node_composite_mask.cc
  nodes/node_composite_mask_to_sdf.cc
  nodes/node_composite_movie_clip.cc
  nodes/node_composite_movie_distortion.cc
  nodes/node_composite_normal.cc
  nodes/node_composite_normalize.cc
  nodes/node_composite_pixelate.cc
  nodes/node_composite_plane_track_deform.cc
  nodes/node_composite_posterize.cc
  nodes/node_composite_relative_to_pixel.cc
  nodes/node_composite_render_layers.cc
  nodes/node_composite_rgb_curves.cc
  nodes/node_composite_rgb_to_bw.cc
  nodes/node_composite_rotate.cc
  nodes/node_composite_scale.cc
  nodes/node_composite_scene_time.cc
  nodes/node_composite_separate_color.cc
  nodes/node_composite_set_alpha.cc
  nodes/node_composite_split.cc
  nodes/node_composite_stabilize_2d.cc
  nodes/node_composite_strip_info.cc
  nodes/node_composite_switch.cc
  nodes/node_composite_switch_view.cc
  nodes/node_composite_time_curve.cc
  nodes/node_composite_tone_map.cc
  nodes/node_composite_track_position.cc
  nodes/node_composite_transform.cc
  nodes/node_composite_translate.cc
  nodes/node_composite_vector_blur.cc
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
  bf_compositor
  PRIVATE bf::render
  PRIVATE bf::windowmanager
  PRIVATE bf::sequencer
  PRIVATE bf::dependencies::optional::opencolorio
  PRIVATE bf::dependencies::optional::tbb
  PRIVATE bf::dependencies::optional::openimagedenoise
  PRIVATE bf::dependencies::optional::fftw3
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

blender_add_lib(bf_nodes_composite "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
target_link_libraries(bf_nodes_compositor_generated bf_nodes_composite)

blender_set_target_unity_build(bf_nodes_composite 10)

# RNA_prototypes.hh
add_dependencies(bf_nodes_composite bf_rna)
