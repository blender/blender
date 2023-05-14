# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../uvedit
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../draw
  ../../functions
  ../../geometry
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../nodes
  ../../render
  ../../windowmanager
  ../../../../intern/atomic
  ../../../../intern/clog
  ../../../../intern/eigen
  ../../../../intern/guardedalloc
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(SRC
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
  sculpt.cc
  sculpt_api.cc
  sculpt_curvature.cc
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
  sculpt_mask_expand.cc
  sculpt_mask_init.cc
  sculpt_multiplane_scrape.cc
  sculpt_ops.cc
  sculpt_paint_color.cc
  sculpt_paint_image.cc
  sculpt_pose.cc
  sculpt_smooth.cc
  sculpt_transform.cc
  sculpt_undo.cc
  sculpt_uv.cc
  curves_sculpt_intern.hh
  paint_intern.hh
  sculpt_intern.hh
)

set(LIB
  bf_blenkernel
  bf_blenlib
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

blender_add_lib(bf_editor_sculpt_paint "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_sculpt_paint bf_rna)
