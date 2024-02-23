# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
  CpuArch.c
  LzFind.c
  LzFindMt.c
  LzFindOpt.c
  LzmaDec.c
  LzmaEnc.c
  LzmaLib.c
  Threads.c

  7zTypes.h
  7zWindows.h
  Alloc.h
  Compiler.h
  CpuArch.h
  LzFind.h
  LzFindMt.h
  LzHash.h
  LzmaDec.h
  LzmaEnc.h
  LzmaLib.h
  Precomp.h
  Threads.h
)

set(LIB
)

blender_add_lib(extern_lzma "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
