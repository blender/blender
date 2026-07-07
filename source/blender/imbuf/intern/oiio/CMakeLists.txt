# SPDX-FileCopyrightText: 2013 Blender Authors
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
  openimageio_api.h
  openimageio_support.hh

  openimageio_api.cpp
  openimageio_support.cc
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::clog
  PRIVATE bf::dependencies::openimageio
  PRIVATE bf::dependencies::optional::openexr
  PRIVATE bf::dependencies::optional::pugixml
)

blender_add_lib(bf_imbuf_openimageio "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
