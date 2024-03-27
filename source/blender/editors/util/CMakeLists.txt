# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../space_sequencer
  ../../asset_system
  ../../blenkernel
  ../../blentranslation
  ../../bmesh
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../sequencer
  ../../windowmanager
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  ed_draw.cc
  ed_transverts.cc
  ed_util.cc
  ed_util_imbuf.cc
  ed_util_ops.cc
  ed_viewer_path.cc
  gizmo_utils.cc
  numinput.cc
  select_utils.cc

  # general includes
  ../include/BIF_glutil.hh
  ../include/ED_anim_api.hh
  ../include/ED_armature.hh
  ../include/ED_asset.hh
  ../include/ED_asset_menu_utils.hh
  ../include/ED_buttons.hh
  ../include/ED_clip.hh
  ../include/ED_curve.hh
  ../include/ED_curves.hh
  ../include/ED_curves_sculpt.hh
  ../include/ED_datafiles.h
  ../include/ED_file_indexer.hh
  ../include/ED_fileselect.hh
  ../include/ED_geometry.hh
  ../include/ED_gizmo_library.hh
  ../include/ED_gizmo_utils.hh
  ../include/ED_gpencil_legacy.hh
  ../include/ED_grease_pencil.hh
  ../include/ED_image.hh
  ../include/ED_info.hh
  ../include/ED_keyframes_draw.hh
  ../include/ED_keyframes_edit.hh
  ../include/ED_keyframes_keylist.hh
  ../include/ED_keyframing.hh
  ../include/ED_lattice.hh
  ../include/ED_markers.hh
  ../include/ED_mask.hh
  ../include/ED_mball.hh
  ../include/ED_mesh.hh
  ../include/ED_node.hh
  ../include/ED_node_c.hh
  ../include/ED_node_preview.hh
  ../include/ED_numinput.hh
  ../include/ED_object.hh
  ../include/ED_object_vgroup.hh
  ../include/ED_outliner.hh
  ../include/ED_paint.hh
  ../include/ED_particle.hh
  ../include/ED_physics.hh
  ../include/ED_render.hh
  ../include/ED_scene.hh
  ../include/ED_screen.hh
  ../include/ED_screen_types.hh
  ../include/ED_sculpt.hh
  ../include/ED_select_utils.hh
  ../include/ED_sequencer.hh
  ../include/ED_sound.hh
  ../include/ED_space_api.hh
  ../include/ED_spreadsheet.hh
  ../include/ED_text.hh
  ../include/ED_time_scrub_ui.hh
  ../include/ED_transform.hh
  ../include/ED_transform_snap_object_context.hh
  ../include/ED_transverts.hh
  ../include/ED_undo.hh
  ../include/ED_userpref.hh
  ../include/ED_util.hh
  ../include/ED_util_imbuf.hh
  ../include/ED_uvedit.hh
  ../include/ED_view3d.hh
  ../include/ED_view3d_offscreen.hh
  ../include/ED_viewer_path.hh
  ../include/UI_abstract_view.hh
  ../include/UI_grid_view.hh
  ../include/UI_icons.hh
  ../include/UI_interface.hh
  ../include/UI_interface_c.hh
  ../include/UI_interface_icons.hh
  ../include/UI_resources.hh
  ../include/UI_string_search.hh
  ../include/UI_tree_view.hh
  ../include/UI_view2d.hh
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::blenfont
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
)


if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
  list(APPEND INC
    ../../python
  )
endif()

blender_add_lib(bf_editor_util "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_util bf_rna)
