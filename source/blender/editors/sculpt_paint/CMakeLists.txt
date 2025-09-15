# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../..

  ../asset
  ../include
  ../uvedit
  ../../blenlib
  ../../makesrna
  ../../../../intern/eigen
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${ZSTD_INCLUDE_DIRS}
)

set(SRC
  brush_asset_ops.cc
  curves_sculpt_add.cc
  curves_sculpt_brush.cc
  curves_sculpt_comb.cc
  curves_sculpt_delete.cc
  curves_sculpt_density.cc
  curves_sculpt_grow_shrink.cc
  curves_sculpt_ops.cc
  curves_sculpt_pinch.cc
  curves_sculpt_puff.cc
  curves_sculpt_selection.cc
  curves_sculpt_selection_paint.cc
  curves_sculpt_slide.cc
  curves_sculpt_smooth.cc
  curves_sculpt_snake_hook.cc
  grease_pencil_draw_ops.cc
  grease_pencil_erase.cc
  grease_pencil_fill.cc
  grease_pencil_interpolate.cc
  grease_pencil_paint.cc
  grease_pencil_paint_common.cc
  grease_pencil_sculpt_clone.cc
  grease_pencil_sculpt_grab.cc
  grease_pencil_sculpt_pinch.cc
  grease_pencil_sculpt_push.cc
  grease_pencil_sculpt_randomize.cc
  grease_pencil_sculpt_smooth.cc
  grease_pencil_sculpt_strength.cc
  grease_pencil_sculpt_thickness.cc
  grease_pencil_sculpt_twist.cc
  grease_pencil_tint.cc
  grease_pencil_trace.cc
  grease_pencil_trace_util.cc
  grease_pencil_vertex_average.cc
  grease_pencil_vertex_blur.cc
  grease_pencil_vertex_paint.cc
  grease_pencil_vertex_replace.cc
  grease_pencil_vertex_smear.cc
  grease_pencil_weight_average.cc
  grease_pencil_weight_blur.cc
  grease_pencil_weight_draw.cc
  grease_pencil_weight_smear.cc
  paint_canvas.cc
  paint_cursor.cc
  paint_curve.cc
  paint_curve_undo.cc
  paint_hide.cc
  paint_image.cc
  paint_image_2d.cc
  paint_image_2d_curve_mask.cc
  paint_image_ops_paint.cc
  paint_image_proj.cc
  paint_mask.cc
  paint_ops.cc
  paint_sample_color.cc
  paint_stroke.cc
  paint_utils.cc
  paint_vertex.cc
  paint_vertex_color_ops.cc
  paint_vertex_proj.cc
  paint_vertex_weight_ops.cc
  paint_vertex_weight_utils.cc
  paint_weight.cc
  sculpt.cc
  sculpt_automasking.cc
  sculpt_boundary.cc
  sculpt_cloth.cc
  sculpt_detail.cc
  sculpt_dyntopo.cc
  sculpt_expand.cc
  sculpt_face_set.cc
  sculpt_filter_color.cc
  sculpt_filter_mask.cc
  sculpt_filter_mesh.cc
  sculpt_flood_fill.cc
  sculpt_geodesic.cc
  sculpt_gesture.cc
  sculpt_hide.cc
  sculpt_mask_init.cc
  sculpt_ops.cc
  sculpt_paint_color.cc
  sculpt_paint_image.cc
  sculpt_pose.cc
  sculpt_project.cc
  sculpt_smooth.cc
  sculpt_transform.cc
  sculpt_trim.cc
  sculpt_undo.cc
  sculpt_uv.cc

  curves_sculpt_intern.hh
  grease_pencil_intern.hh
  grease_pencil_trace_util.hh
  grease_pencil_weight_paint.hh
  mesh_brush_common.hh
  paint_hide.hh
  paint_intern.hh
  paint_mask.hh
  sculpt_automask.hh
  sculpt_boundary.hh
  sculpt_cloth.hh
  sculpt_color.hh
  sculpt_dyntopo.hh
  sculpt_expand.hh
  sculpt_face_set.hh
  sculpt_filter.hh
  sculpt_flood_fill.hh
  sculpt_geodesic.hh
  sculpt_gesture.hh
  sculpt_hide.hh
  sculpt_intern.hh
  sculpt_islands.hh
  sculpt_pose.hh
  sculpt_smooth.hh
  sculpt_undo.hh

  brushes/bmesh_topology_rake.cc
  brushes/clay.cc
  brushes/clay_strips.cc
  brushes/clay_thumb.cc
  brushes/crease.cc
  brushes/draw.cc
  brushes/draw_face_sets.cc
  brushes/draw_sharp.cc
  brushes/draw_vector_displacement.cc
  brushes/elastic_deform.cc
  brushes/enhance_details.cc
  brushes/grab.cc
  brushes/inflate.cc
  brushes/layer.cc
  brushes/mask.cc
  brushes/multiplane_scrape.cc
  brushes/multires_displacement_eraser.cc
  brushes/multires_displacement_smear.cc
  brushes/pinch.cc
  brushes/plane.cc
  brushes/relax.cc
  brushes/rotate.cc
  brushes/smooth.cc
  brushes/smooth_mask.cc
  brushes/snake_hook.cc
  brushes/surface_smooth.cc
  brushes/thumb.cc
  brushes/topology_slide.cc

  brushes/brushes.hh
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
  PRIVATE bf::extern::fmtlib
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
)

if(WITH_POTRACE)
  list(APPEND INC
    ${POTRACE_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${POTRACE_LIBRARIES}
  )
  add_definitions(-DWITH_POTRACE)
endif()

if(WIN32)
  add_definitions(-DNOMINMAX)
endif()

if(WITH_GTESTS)
  set(TEST_SRC
    mesh_brush_common_tests.cc
    paint_test.cc
    sculpt_detail_test.cc
    sculpt_undo_test.cc
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
