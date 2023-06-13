# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation

# avoid noisy warnings
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_c_flag(
    "-Wno-self-assign"
  )
endif()

set(INC
  .
)

set(INC_SYS

)

set(SRC
  Alloc.c
  LzFind.c
  LzmaDec.c
  LzmaEnc.c
  LzmaLib.c

  Alloc.h
  LzFind.h
  LzHash.h
  LzmaDec.h
  LzmaEnc.h
  LzmaLib.h
  Types.h
)

set(LIB
)

blender_add_lib(extern_lzma "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
