# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

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

  cineon_dpx.cc
  cineonlib.cc
  dpxlib.cc
  logImageCore.cc
  logmemfile.cc
)

set(LIB
)

if(WITH_IMAGE_CINEON)
  add_definitions(-DWITH_CINEON)
endif()

blender_add_lib(bf_imbuf_cineon "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
