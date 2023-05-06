# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../gpu
  ../../ikplugin
  ../../makesdna
  ../../makesrna
  ../../render
  ../../sequencer
  ../../windowmanager
  ../../../../intern/guardedalloc
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)
set(INC_SYS
)

set(SRC
  transform.c
  transform_constraints.c
  transform_convert.c
  transform_convert_action.c
  transform_convert_armature.c
  transform_convert_cursor.c
  transform_convert_curve.c
  transform_convert_curves.cc
  transform_convert_gpencil_legacy.c
  transform_convert_graph.c
  transform_convert_lattice.c
  transform_convert_mask.c
  transform_convert_mball.c
  transform_convert_mesh.c
  transform_convert_mesh_edge.c
  transform_convert_mesh_skin.c
  transform_convert_mesh_uv.c
  transform_convert_mesh_vert_cdata.c
  transform_convert_nla.c
  transform_convert_node.cc
  transform_convert_object.c
  transform_convert_object_texspace.c
  transform_convert_paintcurve.c
  transform_convert_particle.c
  transform_convert_sculpt.cc
  transform_convert_sequencer.c
  transform_convert_sequencer_image.c
  transform_convert_tracking.c
  transform_convert_tracking_curves.c
  transform_draw_cursors.c
  transform_generics.c
  transform_gizmo_2d.c
  transform_gizmo_3d.cc
  transform_gizmo_3d_cage.cc
  transform_gizmo_3d_shear.cc
  transform_gizmo_extrude_3d.c
  transform_input.c
  transform_mode.c
  transform_mode_align.c
  transform_mode_baketime.c
  transform_mode_bbone_resize.c
  transform_mode_bend.c
  transform_mode_boneenvelope.c
  transform_mode_boneroll.c
  transform_mode_curveshrinkfatten.c
  transform_mode_customdata.c
  transform_mode_edge_rotate_normal.c
  transform_mode_edge_seq_slide.c
  transform_mode_edge_slide.c
  transform_mode_gpopacity.c
  transform_mode_gpshrinkfatten.c
  transform_mode_maskshrinkfatten.c
  transform_mode_mirror.c
  transform_mode_push_pull.c
  transform_mode_resize.c
  transform_mode_rotate.c
  transform_mode_shear.c
  transform_mode_shrink_fatten.c
  transform_mode_skin_resize.c
  transform_mode_tilt.c
  transform_mode_timescale.c
  transform_mode_timeslide.c
  transform_mode_timetranslate.c
  transform_mode_tosphere.c
  transform_mode_trackball.c
  transform_mode_translate.c
  transform_mode_vert_slide.c
  transform_ops.c
  transform_orientations.c
  transform_snap.cc
  transform_snap_animation.c
  transform_snap_object.cc
  transform_snap_sequencer.c

  transform.h
  transform_constraints.h
  transform_convert.h
  transform_data.h
  transform_draw_cursors.h
  transform_gizmo.h
  transform_mode.h
  transform_orientations.h
  transform_snap.h
)

set(LIB
  bf_blenfont
  bf_blenkernel
  bf_blenlib
  bf_bmesh
  bf_editor_mask
  bf_gpu
)


blender_add_lib(bf_editor_transform "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_transform bf_rna)
