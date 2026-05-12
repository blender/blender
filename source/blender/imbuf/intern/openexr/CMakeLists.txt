# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../..
  ../../../../../intern/utfconv
)

set(INC_SYS
)

set(SRC
  openexr_api.cpp
  openexr_api.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::clog
  PRIVATE bf::dependencies::openexr
)

blender_add_lib(bf_imbuf_openexr "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
