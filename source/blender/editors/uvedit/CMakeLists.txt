# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/eigen
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)


set(SRC
  uvedit_buttons.c
  uvedit_draw.c
  uvedit_islands.c
  uvedit_ops.c
  uvedit_parametrizer.c
  uvedit_path.c
  uvedit_rip.c
  uvedit_select.c
  uvedit_smart_stitch.c
  uvedit_unwrap_ops.c

  uvedit_intern.h
  uvedit_parametrizer.h
)

set(LIB
  bf_bmesh
)


blender_add_lib(bf_editor_uvedit "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
