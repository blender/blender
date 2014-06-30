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
 * The Original Code is Copyright (C) 2013 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file guardedalloc/intern/mallocn_intern.h
 *  \ingroup MEM
 */

#ifndef __MALLOCN_INTERN_H__
#define __MALLOCN_INTERN_H__

/* mmap exception */
#if defined(WIN32)
#  include "mmap_win.h"
#else
#  include <sys/mman.h>
#endif

#if defined(_MSC_VER)
#  define __func__ __FUNCTION__
#endif

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

#undef HAVE_MALLOC_STATS

#if defined(__linux__) || (defined(__FreeBSD_kernel__) && !defined(__FreeBSD__)) || defined(__GLIBC__)
#  include <malloc.h>
#  define HAVE_MALLOC_STATS
#elif defined(__FreeBSD__)
#  include <malloc_np.h>
#elif defined(__APPLE__)
#  include <malloc/malloc.h>
#  define malloc_usable_size malloc_size
#elif defined(WIN32)
#  include <malloc.h>
#  define malloc_usable_size _msize
#else
#  error "We don't know how to use malloc_usable_size on your platform"
#endif

/* Blame Microsoft for LLP64 and no inttypes.h, quick workaround needed: */
#if defined(WIN64)
#  define SIZET_FORMAT "%I64u"
#  define SIZET_ARG(a) ((unsigned long long)(a))
#else
#  define SIZET_FORMAT "%lu"
#  define SIZET_ARG(a) ((unsigned long)(a))
#endif

#define SIZET_ALIGN_4(len) ((len + 3) & ~(size_t)3)

#ifdef __GNUC__
#  define LIKELY(x)       __builtin_expect(!!(x), 1)
#  define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#  define LIKELY(x)       (x)
#  define UNLIKELY(x)     (x)
#endif

#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__NetBSD__)
// Needed for memalign on Linux and _aligned_alloc on Windows.
#  ifdef FREE_WINDOWS
/* make sure _aligned_malloc is included */
#    ifdef __MSVCRT_VERSION__
#      undef __MSVCRT_VERSION__
#    endif

#    define __MSVCRT_VERSION__ 0x0700
#  endif  // FREE_WINDOWS

#  include <malloc.h>
#else
// Apple's malloc is 16-byte aligned, and does not have malloc.h, so include
// stdilb instead.
#  include <stdlib.h>
#endif

/* visual studio 2012 does not define inline for C */
#ifdef _MSC_VER
#  define MEM_INLINE static __inline
#else
#  define MEM_INLINE static inline
#endif

#define IS_POW2(a) (((a) & ((a) - 1)) == 0)

/* Extra padding which needs to be applied on MemHead to make it aligned. */
#define MEMHEAD_ALIGN_PADDING(alignment) ((size_t)alignment - (sizeof(MemHeadAligned) % (size_t)alignment))

/* Real pointer returned by the malloc or aligned_alloc. */
#define MEMHEAD_REAL_PTR(memh) ((char *)memh - MEMHEAD_ALIGN_PADDING(memh->alignment))

void *aligned_malloc(size_t size, size_t alignment);
void aligned_free(void *ptr);

/* Prototypes for counted allocator functions */
size_t MEM_lockfree_allocN_len(const void *vmemh) ATTR_WARN_UNUSED_RESULT;
void MEM_lockfree_freeN(void *vmemh);
void *MEM_lockfree_dupallocN(const void *vmemh) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
void *MEM_lockfree_reallocN_id(void *vmemh, size_t len, const char *UNUSED(str))  ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(2);
void *MEM_lockfree_recallocN_id(void *vmemh, size_t len, const char *UNUSED(str))  ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(2);
void *MEM_lockfree_callocN(size_t len, const char *UNUSED(str))  ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(1) ATTR_NONNULL(2);
void *MEM_lockfree_mallocN(size_t len, const char *UNUSED(str)) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(1) ATTR_NONNULL(2);
void *MEM_lockfree_mallocN_aligned(size_t len, size_t alignment, const char *UNUSED(str)) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(1) ATTR_NONNULL(3);
void *MEM_lockfree_mapallocN(size_t len, const char *UNUSED(str)) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(1) ATTR_NONNULL(2);
void MEM_lockfree_printmemlist_pydict(void);
void MEM_lockfree_printmemlist(void);
void MEM_lockfree_callbackmemlist(void (*func)(void *));
void MEM_lockfree_printmemlist_stats(void);
void MEM_lockfree_set_error_callback(void (*func)(const char *));
bool MEM_lockfree_check_memory_integrity(void);
void MEM_lockfree_set_lock_callback(void (*lock)(void), void (*unlock)(void));
void MEM_lockfree_set_memory_debug(void);
uintptr_t MEM_lockfree_get_memory_in_use(void);
uintptr_t MEM_lockfree_get_mapped_memory_in_use(void);
unsigned int MEM_lockfree_get_memory_blocks_in_use(void);
void MEM_lockfree_reset_peak_memory(void);
uintptr_t MEM_lockfree_get_peak_memory(void) ATTR_WARN_UNUSED_RESULT;
#ifndef NDEBUG
const char *MEM_lockfree_name_ptr(void *vmemh);
#endif

/* Prototypes for fully guarded allocator functions */
size_t MEM_guarded_allocN_len(const void *vmemh) ATTR_WARN_UNUSED_RESULT;
void MEM_guarded_freeN(void *vmemh);
void *MEM_guarded_dupallocN(const void *vmemh) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
void *MEM_guarded_reallocN_id(void *vmemh, size_t len, const char *UNUSED(str)) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(2);
void *MEM_guarded_recallocN_id(void *vmemh, size_t len, const char *UNUSED(str)) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(2);
void *MEM_guarded_callocN(size_t len, const char *UNUSED(str)) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(1) ATTR_NONNULL(2);
void *MEM_guarded_mallocN(size_t len, const char *UNUSED(str)) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(1) ATTR_NONNULL(2);
void *MEM_guarded_mallocN_aligned(size_t len, size_t alignment, const char *UNUSED(str)) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(1) ATTR_NONNULL(3);
void *MEM_guarded_mapallocN(size_t len, const char *UNUSED(str)) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_ALLOC_SIZE(1) ATTR_NONNULL(2);
void MEM_guarded_printmemlist_pydict(void);
void MEM_guarded_printmemlist(void);
void MEM_guarded_callbackmemlist(void (*func)(void *));
void MEM_guarded_printmemlist_stats(void);
void MEM_guarded_set_error_callback(void (*func)(const char *));
bool MEM_guarded_check_memory_integrity(void);
void MEM_guarded_set_lock_callback(void (*lock)(void), void (*unlock)(void));
void MEM_guarded_set_memory_debug(void);
uintptr_t MEM_guarded_get_memory_in_use(void);
uintptr_t MEM_guarded_get_mapped_memory_in_use(void);
unsigned int MEM_guarded_get_memory_blocks_in_use(void);
void MEM_guarded_reset_peak_memory(void);
uintptr_t MEM_guarded_get_peak_memory(void) ATTR_WARN_UNUSED_RESULT;
#ifndef NDEBUG
const char *MEM_guarded_name_ptr(void *vmemh);
#endif

#endif  /* __MALLOCN_INTERN_H__ */
