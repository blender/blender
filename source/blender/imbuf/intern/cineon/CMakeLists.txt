# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../..
  ../../../blenkernel
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
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_IMAGE_CINEON)
  add_definitions(-DWITH_CINEON)
endif()

blender_add_lib(bf_imbuf_cineon "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
