# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)

set(SRC
  info_draw.c
  info_ops.c
  info_report.c
  info_stats.cc
  space_info.c
  textview.c

  info_intern.h
  textview.h
)

set(LIB
)


blender_add_lib(bf_editor_space_info "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
