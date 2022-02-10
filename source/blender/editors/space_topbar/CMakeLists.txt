# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)


set(SRC
  space_topbar.c
)

set(LIB
)


blender_add_lib(bf_editor_space_topbar "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
