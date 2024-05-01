# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../asset
  ../uvedit
  ../../asset_system
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../bmesh
  ../../draw
  ../../functions
  ../../geometry
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../nodes
  ../../render
  ../../windowmanager
  ../../../../intern/eigen
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
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
  grease_pencil_paint.cc
  grease_pencil_sculpt_clone.cc
  grease_pencil_sculpt_common.cc
  grease_pencil_sculpt_grab.cc
  grease_pencil_sculpt_pinch.cc
  grease_pencil_sculpt_push.cc
  grease_pencil_sculpt_randomize.cc
  grease_pencil_sculpt_smooth.cc
  grease_pencil_sculpt_strength.cc
  grease_pencil_sculpt_thickness.cc
  grease_pencil_sculpt_twist.cc
  grease_pencil_tint.cc
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
  sculpt_brush_types.cc
  sculpt_cloth.cc
  sculpt_detail.cc
  sculpt_dyntopo.cc
  sculpt_expand.cc
  sculpt_face_set.cc
  sculpt_filter_color.cc
  sculpt_filter_mask.cc
  sculpt_filter_mesh.cc
  sculpt_geodesic.cc
  sculpt_gesture.cc
  sculpt_mask_init.cc
  sculpt_multiplane_scrape.cc
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
  grease_pencil_weight_paint.hh
  paint_intern.hh
  sculpt_intern.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::animrig
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
)

if(WITH_TBB)
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_TBB)
endif()

if(WIN32)
  add_definitions(-DNOMINMAX)
endif()

if(WITH_GTESTS)
  set(TEST_SRC
    sculpt_detail_test.cc
  )
  set(TEST_INC
  )
  set(TEST_LIB
    ${LIB}
    bf_rna  # RNA_prototypes.h
  )
  blender_add_test_suite_lib(editor_sculpt_paint "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${TEST_LIB}")
endif()

# If compiling with msvc clang we need to add the D_LIBCPP_VERSION define
# so we don't run into tbb errors when compiling with lib
if(WITH_TBB AND MSVC_CLANG)
  string(APPEND CMAKE_CXX_FLAGS " /D_LIBCPP_VERSION")
endif()

blender_add_lib(bf_editor_sculpt_paint "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
# RNA_prototypes.h
add_dependencies(bf_editor_sculpt_paint bf_rna)
