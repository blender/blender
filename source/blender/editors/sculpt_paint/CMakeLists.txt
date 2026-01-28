# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../..

  ../asset
  ../include
  ../mesh
  ../uvedit
  ../../blenlib
  ../../makesrna
  ../../../../intern/eigen
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  brush_asset_ops.cc
  paint_cursor.cc
  paint_curve.cc
  paint_curve_undo.cc
  paint_ops.cc
  paint_sample_color.cc
  paint_stroke.cc
  paint_utils.cc
  sculpt_uv.cc

  curves/sculpt_add.cc
  curves/sculpt_brush.cc
  curves/sculpt_comb.cc
  curves/sculpt_delete.cc
  curves/sculpt_density.cc
  curves/sculpt_grow_shrink.cc
  curves/sculpt_ops.cc
  curves/sculpt_pinch.cc
  curves/sculpt_puff.cc
  curves/sculpt_selection.cc
  curves/sculpt_selection_paint.cc
  curves/sculpt_slide.cc
  curves/sculpt_smooth.cc
  curves/sculpt_snake_hook.cc

  grease_pencil/draw_ops.cc
  grease_pencil/erase.cc
  grease_pencil/fill.cc
  grease_pencil/interpolate.cc
  grease_pencil/paint.cc
  grease_pencil/paint_common.cc
  grease_pencil/sculpt_clone.cc
  grease_pencil/sculpt_grab.cc
  grease_pencil/sculpt_pinch.cc
  grease_pencil/sculpt_push.cc
  grease_pencil/sculpt_randomize.cc
  grease_pencil/sculpt_smooth.cc
  grease_pencil/sculpt_strength.cc
  grease_pencil/sculpt_thickness.cc
  grease_pencil/sculpt_twist.cc
  grease_pencil/tint.cc
  grease_pencil/trace.cc
  grease_pencil/trace_util.cc
  grease_pencil/vertex_average.cc
  grease_pencil/vertex_blur.cc
  grease_pencil/vertex_paint.cc
  grease_pencil/vertex_replace.cc
  grease_pencil/vertex_smear.cc
  grease_pencil/weight_average.cc
  grease_pencil/weight_blur.cc
  grease_pencil/weight_draw.cc
  grease_pencil/weight_smear.cc

  mesh/paint_canvas.cc
  mesh/paint_hide.cc
  mesh/paint_image.cc
  mesh/paint_image_2d.cc
  mesh/paint_image_2d_curve_mask.cc
  mesh/paint_image_ops_paint.cc
  mesh/paint_image_proj.cc
  mesh/paint_mask.cc
  mesh/paint_vertex.cc
  mesh/paint_vertex_color_ops.cc
  mesh/paint_vertex_weight_ops.cc
  mesh/paint_vertex_weight_utils.cc
  mesh/paint_weight.cc
  mesh/sculpt.cc
  mesh/sculpt_automasking.cc
  mesh/sculpt_boundary.cc
  mesh/sculpt_cloth.cc
  mesh/sculpt_detail.cc
  mesh/sculpt_dyntopo.cc
  mesh/sculpt_expand.cc
  mesh/sculpt_face_set.cc
  mesh/sculpt_filter_color.cc
  mesh/sculpt_filter_mask.cc
  mesh/sculpt_filter_mesh.cc
  mesh/sculpt_flood_fill.cc
  mesh/sculpt_geodesic.cc
  mesh/sculpt_gesture.cc
  mesh/sculpt_hide.cc
  mesh/sculpt_mask_extract.cc
  mesh/sculpt_mask_init.cc
  mesh/sculpt_ops.cc
  mesh/sculpt_paint_color.cc
  mesh/sculpt_paint_image.cc
  mesh/sculpt_pose.cc
  mesh/sculpt_project.cc
  mesh/sculpt_smooth.cc
  mesh/sculpt_transform.cc
  mesh/sculpt_trim.cc
  mesh/sculpt_undo.cc

  paint_intern.hh

  curves/sculpt_intern.hh

  grease_pencil/grease_pencil_intern.hh
  grease_pencil/trace_util.hh
  grease_pencil/weight_paint.hh

  mesh/mesh_brush_common.hh
  mesh/paint_hide.hh
  mesh/paint_mask.hh
  mesh/sculpt_automask.hh
  mesh/sculpt_boundary.hh
  mesh/sculpt_cloth.hh
  mesh/sculpt_color.hh
  mesh/sculpt_dyntopo.hh
  mesh/sculpt_expand.hh
  mesh/sculpt_face_set.hh
  mesh/sculpt_filter.hh
  mesh/sculpt_flood_fill.hh
  mesh/sculpt_geodesic.hh
  mesh/sculpt_gesture.hh
  mesh/sculpt_hide.hh
  mesh/sculpt_intern.hh
  mesh/sculpt_islands.hh
  mesh/sculpt_pose.hh
  mesh/sculpt_smooth.hh
  mesh/sculpt_undo.hh

  mesh/brushes/bmesh_topology_rake.cc
  mesh/brushes/clay.cc
  mesh/brushes/clay_strips.cc
  mesh/brushes/clay_thumb.cc
  mesh/brushes/crease.cc
  mesh/brushes/draw.cc
  mesh/brushes/draw_face_sets.cc
  mesh/brushes/draw_sharp.cc
  mesh/brushes/draw_vector_displacement.cc
  mesh/brushes/elastic_deform.cc
  mesh/brushes/enhance_details.cc
  mesh/brushes/grab.cc
  mesh/brushes/inflate.cc
  mesh/brushes/layer.cc
  mesh/brushes/mask.cc
  mesh/brushes/multiplane_scrape.cc
  mesh/brushes/multires_displacement_eraser.cc
  mesh/brushes/multires_displacement_smear.cc
  mesh/brushes/pinch.cc
  mesh/brushes/plane.cc
  mesh/brushes/relax.cc
  mesh/brushes/rotate.cc
  mesh/brushes/smooth.cc
  mesh/brushes/smooth_mask.cc
  mesh/brushes/snake_hook.cc
  mesh/brushes/surface_smooth.cc
  mesh/brushes/thumb.cc
  mesh/brushes/topology_slide.cc

  mesh/brushes/brushes.hh
)

set(LIB
  PRIVATE bf::asset_system
  PRIVATE bf::blenkernel
  PRIVATE bf::animrig
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::draw
  PRIVATE bf::functions
  PRIVATE bf::geometry
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::nodes
  PRIVATE bf::render
  PRIVATE bf::windowmanager
  PRIVATE bf::dependencies::zstd
  PRIVATE bf::dependencies::optional::potrace
)

if(WIN32)
  add_definitions(-DNOMINMAX)
endif()

if(WITH_GTESTS)
  set(TEST_SRC
    mesh/mesh_brush_common_tests.cc
    mesh/paint_test.cc
    mesh/sculpt_boundary_tests.cc
    mesh/sculpt_detail_test.cc
    mesh/sculpt_undo_test.cc
  )
  set(TEST_INC
  )
  set(TEST_LIB
    ${LIB}
    bf_rna  # RNA_prototypes.hh
  )
  blender_add_test_suite_lib(editor_sculpt_paint "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${TEST_LIB}")
endif()

# If compiling with MSVC clang we need to add the `D_LIBCPP_VERSION` define
# so we don't run into TBB errors when compiling with lib.
if(WITH_TBB AND MSVC_CLANG)
  string(APPEND CMAKE_CXX_FLAGS " /D_LIBCPP_VERSION")
endif()

blender_add_lib(bf_editor_sculpt_paint "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
# RNA_prototypes.hh
add_dependencies(bf_editor_sculpt_paint bf_rna)
