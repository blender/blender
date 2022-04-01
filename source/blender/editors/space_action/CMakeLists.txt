# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(SRC
  action_buttons.c
  action_data.c
  action_draw.c
  action_edit.c
  action_ops.c
  action_select.c
  space_action.c

  action_intern.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)


blender_add_lib(bf_editor_space_action "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_space_action bf_rna)
