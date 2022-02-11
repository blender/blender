# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

set(INC
  .
  ..
  ../..
  ../../../blenkernel
  ../../../blenlib
  ../../../makesdna
  ../../../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
  cineonlib.h
  dpxlib.h
  logImageCore.h
  logmemfile.h

  cineon_dpx.c
  cineonlib.c
  dpxlib.c
  logImageCore.c
  logmemfile.c
)

set(LIB
)

if(WITH_IMAGE_CINEON)
  add_definitions(-DWITH_CINEON)
endif()

blender_add_lib(bf_imbuf_cineon "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
