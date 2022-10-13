
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../include
  ../../blenkernel
  ../../blenlib
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
)

set(INC_SYS
)

set(SRC
    project_ops.cc
)

set(LIB
  bf_blenkernel
)

blender_add_lib(bf_editor_project "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
