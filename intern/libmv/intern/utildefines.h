/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LIBMV_C_API_UTILDEFINES_H_
#define LIBMV_C_API_UTILDEFINES_H_

#if defined(_MSC_VER) && _MSC_VER < 1900
#  define __func__ __FUNCTION__
#  define snprintf _snprintf
#endif

#ifdef WITH_LIBMV_GUARDED_ALLOC
#  include "MEM_guardedalloc.h"
#  if defined __GNUC__
#    define LIBMV_OBJECT_NEW(type, args...)                                    \
      new (MEM_mallocN(sizeof(type), __func__)) type(args)
#  else
#    define LIBMV_OBJECT_NEW(type, ...)                                        \
      new (MEM_mallocN(sizeof(type), __FUNCTION__)) type(__VA_ARGS__)
#  endif
#  define LIBMV_OBJECT_DELETE(what, type)                                      \
    {                                                                          \
      if (what) {                                                              \
        ((type*)what)->~type();                                                \
        MEM_freeN(what);                                                       \
      }                                                                        \
    }                                                                          \
    (void)0
#  define LIBMV_STRUCT_NEW(type, count)                                        \
    (type*)MEM_mallocN(sizeof(type) * count, __func__)
#  define LIBMV_STRUCT_DELETE(what) MEM_freeN(what)
#else
// Need this to keep libmv-capi potentially standalone.
#  if defined __GNUC__ || defined __sun
#    define LIBMV_OBJECT_NEW(type, args...)                                    \
      new (malloc(sizeof(type))) type(args)
#  else
#    define LIBMV_OBJECT_NEW(type, ...)                                        \
      new (malloc(sizeof(type))) type(__VA_ARGS__)
#  endif
#  define LIBMV_OBJECT_DELETE(what, type)                                      \
    {                                                                          \
      if (what) {                                                              \
        ((type*)(what))->~type();                                              \
        free(what);                                                            \
      }                                                                        \
    }                                                                          \
    (void)0
#  define LIBMV_STRUCT_NEW(type, count) (type*)malloc(sizeof(type) * count)
#  define LIBMV_STRUCT_DELETE(what)                                            \
    {                                                                          \
      if (what)                                                                \
        free(what);                                                            \
    }                                                                          \
    (void)0
#endif

#endif  // LIBMV_C_API_UTILDEFINES_H_
