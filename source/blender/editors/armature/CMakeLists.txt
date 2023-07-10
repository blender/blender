# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../asset_system
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../depsgraph
  ../../gpu
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/eigen
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  armature_add.cc
  armature_edit.cc
  armature_naming.cc
  armature_ops.cc
  armature_relations.cc
  armature_select.cc
  armature_skinning.cc
  armature_utils.cc
  editarmature_undo.cc
  meshlaplacian.cc
  pose_edit.cc
  pose_group.cc
  pose_lib_2.cc
  pose_select.cc
  pose_slide.cc
  pose_transform.cc
  pose_utils.cc

  armature_intern.h
  meshlaplacian.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)


blender_add_lib(bf_editor_armature "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_armature bf_rna)
