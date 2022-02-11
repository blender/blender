# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../depsgraph
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/eigen
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)

set(SRC
  armature_add.c
  armature_edit.c
  armature_naming.c
  armature_ops.c
  armature_relations.c
  armature_select.c
  armature_skinning.c
  armature_utils.c
  editarmature_undo.c
  meshlaplacian.c
  pose_backup.cc
  pose_edit.c
  pose_group.c
  pose_lib.c
  pose_lib_2.c
  pose_select.c
  pose_slide.c
  pose_transform.c
  pose_utils.c

  armature_intern.h
  meshlaplacian.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)


blender_add_lib(bf_editor_armature "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
