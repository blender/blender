/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef LIBMV_C_API_UTILDEFINES_H_
#define LIBMV_C_API_UTILDEFINES_H_

#if defined(_MSC_VER)
#  define __func__ __FUNCTION__
#  define snprintf _snprintf
#endif

#ifdef WITH_LIBMV_GUARDED_ALLOC
#  include "MEM_guardedalloc.h"
#  define LIBMV_OBJECT_NEW OBJECT_GUARDED_NEW
#  define LIBMV_OBJECT_DELETE OBJECT_GUARDED_DELETE
#  define LIBMV_OBJECT_DELETE OBJECT_GUARDED_DELETE
#  define LIBMV_STRUCT_NEW(type, count) \
  (type*)MEM_mallocN(sizeof(type) * count, __func__)
#  define LIBMV_STRUCT_DELETE(what) MEM_freeN(what)
#else
// Need this to keep libmv-capi potentially standalone.
#  if defined __GNUC__ || defined __sun
#    define LIBMV_OBJECT_NEW(type, args ...) \
  new(malloc(sizeof(type))) type(args)
#  else
#    define LIBMV_OBJECT_NEW(type, ...) \
  new(malloc(sizeof(type))) type(__VA_ARGS__)
#endif
#  define LIBMV_OBJECT_DELETE(what, type) \
  { \
    if (what) { \
      ((type*)(what))->~type(); \
      free(what); \
    } \
  } (void)0
#  define LIBMV_STRUCT_NEW(type, count) (type*)malloc(sizeof(type) * count)
#  define LIBMV_STRUCT_DELETE(what) { if (what) free(what); } (void)0
#endif

#endif  // LIBMV_C_API_UTILDEFINES_H_
