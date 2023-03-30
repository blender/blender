# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation

set(INC
  .
  ..
  ../..
  ../../../blenkernel
  ../../../blenlib
  ../../../makesdna
  ../../../../../intern/guardedalloc
  ../../../../../intern/utfconv
)

set(INC_SYS

)

set(SRC
  BlockDXT.h
  Color.h
  ColorBlock.h
  Common.h
  DirectDrawSurface.h
  FlipDXT.h
  Image.h
  PixelFormat.h
  Stream.h
  dds_api.h

  BlockDXT.cpp
  ColorBlock.cpp
  DirectDrawSurface.cpp
  FlipDXT.cpp
  Image.cpp
  Stream.cpp
  dds_api.cpp
)

set(LIB
)

if(WITH_IMAGE_DDS)
  add_definitions(-DWITH_DDS)
endif()

blender_add_lib(bf_imbuf_dds "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
