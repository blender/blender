# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../gpu
  ../../ikplugin
  ../../makesrna
  ../../render
  ../../sequencer
  ../../windowmanager
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)
set(INC_SYS
)

set(SRC
  transform.cc
  transform_constraints.cc
  transform_convert.cc
  transform_convert_action.cc
  transform_convert_armature.cc
  transform_convert_cursor.cc
  transform_convert_curve.cc
  transform_convert_curves.cc
  transform_convert_gpencil_legacy.cc
  transform_convert_graph.cc
  transform_convert_lattice.cc
  transform_convert_mask.cc
  transform_convert_mball.cc
  transform_convert_mesh.cc
  transform_convert_mesh_edge.cc
  transform_convert_mesh_skin.cc
  transform_convert_mesh_uv.cc
  transform_convert_mesh_vert_cdata.cc
  transform_convert_nla.cc
  transform_convert_node.cc
  transform_convert_object.cc
  transform_convert_object_texspace.cc
  transform_convert_paintcurve.cc
  transform_convert_particle.cc
  transform_convert_sculpt.cc
  transform_convert_sequencer.cc
  transform_convert_sequencer_image.cc
  transform_convert_tracking.cc
  transform_convert_tracking_curves.cc
  transform_draw_cursors.cc
  transform_generics.cc
  transform_gizmo_2d.cc
  transform_gizmo_3d.cc
  transform_gizmo_3d_cage.cc
  transform_gizmo_3d_shear.cc
  transform_gizmo_extrude_3d.cc
  transform_input.cc
  transform_mode.cc
  transform_mode_align.cc
  transform_mode_baketime.cc
  transform_mode_bbone_resize.cc
  transform_mode_bend.cc
  transform_mode_boneenvelope.cc
  transform_mode_boneroll.cc
  transform_mode_curveshrinkfatten.cc
  transform_mode_customdata.cc
  transform_mode_edge_rotate_normal.cc
  transform_mode_edge_seq_slide.cc
  transform_mode_edge_slide.cc
  transform_mode_gpopacity.cc
  transform_mode_gpshrinkfatten.cc
  transform_mode_maskshrinkfatten.cc
  transform_mode_mirror.cc
  transform_mode_push_pull.cc
  transform_mode_resize.cc
  transform_mode_rotate.cc
  transform_mode_shear.cc
  transform_mode_shrink_fatten.cc
  transform_mode_skin_resize.cc
  transform_mode_snapsource.cc
  transform_mode_tilt.cc
  transform_mode_timescale.cc
  transform_mode_timeslide.cc
  transform_mode_timetranslate.cc
  transform_mode_tosphere.cc
  transform_mode_trackball.cc
  transform_mode_translate.cc
  transform_mode_vert_slide.cc
  transform_ops.cc
  transform_orientations.cc
  transform_snap.cc
  transform_snap_animation.cc
  transform_snap_object.cc
  transform_snap_object_armature.cc
  transform_snap_object_camera.cc
  transform_snap_object_curve.cc
  transform_snap_object_editmesh.cc
  transform_snap_object_mesh.cc
  transform_snap_sequencer.cc

  transform.hh
  transform_constraints.hh
  transform_convert.hh
  transform_data.hh
  transform_draw_cursors.hh
  transform_gizmo.hh
  transform_mode.hh
  transform_orientations.hh
  transform_snap.hh
  transform_snap_object.hh
)

set(LIB
  PRIVATE bf::animrig
  bf_blenfont
  bf_blenkernel
  PRIVATE bf::blenlib
  bf_bmesh
  PRIVATE bf::dna
  bf_editor_mask
  bf_gpu
  PRIVATE bf::intern::guardedalloc
)


blender_add_lib(bf_editor_transform "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_transform bf_rna)
