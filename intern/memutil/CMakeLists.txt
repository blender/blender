# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../../source/blender/blenlib
)

set(INC_SYS

)

set(SRC
  intern/MEM_CacheLimiterC-Api.cpp
  intern/MEM_RefCountedC-Api.cpp

  MEM_Allocator.h
  MEM_CacheLimiter.h
  MEM_CacheLimiterC-Api.h
  MEM_RefCounted.h
  MEM_RefCountedC-Api.h
)

set(LIB
)

blender_add_lib(bf_intern_memutil "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
