# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ..
)

set(INC_SYS

)

set(SRC
  intern/MEM_CacheLimiterC-Api.cpp
  intern/MEM_RefCountedC-Api.cpp
  intern/MEM_alloc_string_storage.cc

  MEM_Allocator.h
  MEM_CacheLimiter.h
  MEM_CacheLimiterC-Api.h
  MEM_RefCounted.h
  MEM_RefCountedC-Api.h
  MEM_alloc_string_storage.hh
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_intern_memutil "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::intern::memutil ALIAS bf_intern_memutil)
