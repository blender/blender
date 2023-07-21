# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenloader
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../sequencer
  ../../../../intern/clog

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  outliner_collections.cc
  outliner_context.cc
  outliner_dragdrop.cc
  outliner_draw.cc
  outliner_edit.cc
  outliner_ops.cc
  outliner_query.cc
  outliner_select.cc
  outliner_sync.cc
  outliner_tools.cc
  outliner_tree.cc
  outliner_utils.cc
  space_outliner.cc
  tree/common.cc
  tree/tree_display.cc
  tree/tree_display_data.cc
  tree/tree_display_libraries.cc
  tree/tree_display_orphaned.cc
  tree/tree_display_override_library_hierarchies.cc
  tree/tree_display_override_library_properties.cc
  tree/tree_display_scenes.cc
  tree/tree_display_sequencer.cc
  tree/tree_display_view_layer.cc
  tree/tree_element.cc
  tree/tree_element_anim_data.cc
  tree/tree_element_bone.cc
  tree/tree_element_collection.cc
  tree/tree_element_driver.cc
  tree/tree_element_edit_bone.cc
  tree/tree_element_gpencil_layer.cc
  tree/tree_element_id.cc
  tree/tree_element_id_armature.cc
  tree/tree_element_id_collection.cc
  tree/tree_element_id_curve.cc
  tree/tree_element_id_gpencil_legacy.cc
  tree/tree_element_id_library.cc
  tree/tree_element_id_linestyle.cc
  tree/tree_element_id_mesh.cc
  tree/tree_element_id_metaball.cc
  tree/tree_element_id_object.cc
  tree/tree_element_id_scene.cc
  tree/tree_element_id_texture.cc
  tree/tree_element_label.cc
  tree/tree_element_nla.cc
  tree/tree_element_overrides.cc
  tree/tree_element_rna.cc
  tree/tree_element_scene_objects.cc
  tree/tree_element_seq.cc
  tree/tree_element_view_layer.cc
  tree/tree_iterator.cc

  outliner_intern.hh
  tree/common.hh
  tree/tree_display.hh
  tree/tree_element.hh
  tree/tree_element_anim_data.hh
  tree/tree_element_bone.hh
  tree/tree_element_collection.hh
  tree/tree_element_driver.hh
  tree/tree_element_edit_bone.hh
  tree/tree_element_gpencil_layer.hh
  tree/tree_element_id.hh
  tree/tree_element_id_armature.hh
  tree/tree_element_id_collection.hh
  tree/tree_element_id_curve.hh
  tree/tree_element_id_gpencil_legacy.hh
  tree/tree_element_id_library.hh
  tree/tree_element_id_linestyle.hh
  tree/tree_element_id_mesh.hh
  tree/tree_element_id_metaball.hh
  tree/tree_element_id_object.hh
  tree/tree_element_id_scene.hh
  tree/tree_element_id_texture.hh
  tree/tree_element_label.hh
  tree/tree_element_nla.hh
  tree/tree_element_overrides.hh
  tree/tree_element_rna.hh
  tree/tree_element_scene_objects.hh
  tree/tree_element_seq.hh
  tree/tree_element_view_layer.hh
  tree/tree_iterator.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_editor_undo
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)


blender_add_lib(bf_editor_space_outliner "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h dna_type_offsets.h
add_dependencies(bf_editor_space_outliner bf_rna)
