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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */ 

/** \file BLI_memarena.h
 *  \ingroup bli
 *  \brief Memory arena ADT.
 *  \section aboutmemarena Memory Arena
 * Memory arena's are commonly used when the program
 * needs to quickly allocate lots of little bits of
 * data, which are all freed at the same moment.
 */

#ifndef __BLI_MEMARENA_H__
#define __BLI_MEMARENA_H__

#ifdef __cplusplus
extern "C" {
#endif

/* A reasonable standard buffer size, big
 * enough to not cause much internal fragmentation,
 * small enough not to waste resources
 */
#define BLI_MEMARENA_STD_BUFSIZE    (1 << 14)

struct MemArena;
typedef struct MemArena MemArena;

struct MemArena    *BLI_memarena_new(const int bufsize, const char *name)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull(2)))
#endif
;

void                BLI_memarena_free(struct MemArena *ma)
#ifdef __GNUC__
__attribute__((nonnull(1)))
#endif
;

void                BLI_memarena_use_malloc(struct MemArena *ma)
#ifdef __GNUC__
__attribute__((nonnull(1)))
#endif
;
void                BLI_memarena_use_calloc(struct MemArena *ma)
#ifdef __GNUC__
__attribute__((nonnull(1)))
#endif
;

void                BLI_memarena_use_align(struct MemArena *ma, const int align)
#ifdef __GNUC__
__attribute__((nonnull(1)))
#endif
;

void               *BLI_memarena_alloc(struct MemArena *ma, int size)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull(1)))
__attribute__((alloc_size(2)))
#endif
;

#ifdef __cplusplus
}
#endif


#endif

