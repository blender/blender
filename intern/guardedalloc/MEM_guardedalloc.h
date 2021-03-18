/*
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
 */

/** \file
 * \ingroup MEM
 *
 * \brief Read \ref MEMPage
 *
 * \page MEMPage Guarded memory(de)allocation
 *
 * \section aboutmem c-style guarded memory allocation
 *
 * \subsection memabout About the MEM module
 *
 * MEM provides guarded malloc/calloc calls. All memory is enclosed by
 * pads, to detect out-of-bound writes. All blocks are placed in a
 * linked list, so they remain reachable at all times. There is no
 * back-up in case the linked-list related data is lost.
 *
 * \subsection memissues Known issues with MEM
 *
 * There are currently no known issues with MEM. Note that there is a
 * second intern/ module with MEM_ prefix, for use in c++.
 *
 * \subsection memdependencies Dependencies
 * - stdlib
 * - stdio
 *
 * \subsection memdocs API Documentation
 * See \ref MEM_guardedalloc.h
 */

#ifndef __MEM_GUARDEDALLOC_H__
#define __MEM_GUARDEDALLOC_H__

#include <stdio.h> /* needed for FILE* */

/* Needed for uintptr_t and attributes, exception, don't use BLI anywhere else in `MEM_*` */
#include "../../source/blender/blenlib/BLI_compiler_attrs.h"
#include "../../source/blender/blenlib/BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Returns the length of the allocated memory segment pointed at
 * by vmemh. If the pointer was not previously allocated by this
 * module, the result is undefined.*/
extern size_t (*MEM_allocN_len)(const void *vmemh) ATTR_WARN_UNUSED_RESULT;

/**
 * Release memory previously allocated by this module.
 */
extern void (*MEM_freeN)(void *vmemh);

#if 0 /* UNUSED */
/**
   * Return zero if memory is not in allocated list
   */
extern short (*MEM_testN)(void *vmemh);
#endif

/**
 * Duplicates a block of memory, and returns a pointer to the
 * newly allocated block.  */
extern void *(*MEM_dupallocN)(const void *vmemh) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT;

/**
 * Reallocates a block of memory, and returns pointer to the newly
 * allocated block, the old one is freed. this is not as optimized
 * as a system realloc but just makes a new allocation and copies
 * over from existing memory. */
extern void *(*MEM_reallocN_id)(void *vmemh,
                                size_t len,
                                const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(2);

/**
 * A variant of realloc which zeros new bytes
 */
extern void *(*MEM_recallocN_id)(void *vmemh,
                                 size_t len,
                                 const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(2);

#define MEM_reallocN(vmemh, len) MEM_reallocN_id(vmemh, len, __func__)
#define MEM_recallocN(vmemh, len) MEM_recallocN_id(vmemh, len, __func__)

/**
 * Allocate a block of memory of size len, with tag name str. The
 * memory is cleared. The name must be static, because only a
 * pointer to it is stored ! */
extern void *(*MEM_callocN)(size_t len, const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(1) ATTR_NONNULL(2);

/**
 * Allocate a block of memory of size (len * size), with tag name
 * str, aborting in case of integer overflows to prevent vulnerabilities.
 * The memory is cleared. The name must be static, because only a
 * pointer to it is stored ! */
extern void *(*MEM_calloc_arrayN)(size_t len,
                                  size_t size,
                                  const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(1, 2) ATTR_NONNULL(3);

/**
 * Allocate a block of memory of size len, with tag name str. The
 * name must be a static, because only a pointer to it is stored !
 */
extern void *(*MEM_mallocN)(size_t len, const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(1) ATTR_NONNULL(2);

/**
 * Allocate a block of memory of size (len * size), with tag name str,
 * aborting in case of integer overflow to prevent vulnerabilities. The
 * name must be a static, because only a pointer to it is stored !
 */
extern void *(*MEM_malloc_arrayN)(size_t len,
                                  size_t size,
                                  const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(1, 2) ATTR_NONNULL(3);

/**
 * Allocate an aligned block of memory of size len, with tag name str. The
 * name must be a static, because only a pointer to it is stored !
 */
extern void *(*MEM_mallocN_aligned)(size_t len,
                                    size_t alignment,
                                    const char *str) /* ATTR_MALLOC */ ATTR_WARN_UNUSED_RESULT
    ATTR_ALLOC_SIZE(1) ATTR_NONNULL(3);

/** Print a list of the names and sizes of all allocated memory
 * blocks. as a python dict for easy investigation */
extern void (*MEM_printmemlist_pydict)(void);

/** Print a list of the names and sizes of all allocated memory
 * blocks. */
extern void (*MEM_printmemlist)(void);

/** calls the function on all allocated memory blocks. */
extern void (*MEM_callbackmemlist)(void (*func)(void *));

/** Print statistics about memory usage */
extern void (*MEM_printmemlist_stats)(void);

/** Set the callback function for error output. */
extern void (*MEM_set_error_callback)(void (*func)(const char *));

/**
 * Are the start/end block markers still correct ?
 *
 * \retval true for correct memory, false for corrupted memory. */
extern bool (*MEM_consistency_check)(void);

/** Attempt to enforce OSX (or other OS's) to have malloc and stack nonzero */
extern void (*MEM_set_memory_debug)(void);

/** Memory usage stats. */
extern size_t (*MEM_get_memory_in_use)(void);
/** Get amount of memory blocks in use. */
extern unsigned int (*MEM_get_memory_blocks_in_use)(void);

/** Reset the peak memory statistic to zero. */
extern void (*MEM_reset_peak_memory)(void);

/** Get the peak memory usage in bytes, including mmap allocations. */
extern size_t (*MEM_get_peak_memory)(void) ATTR_WARN_UNUSED_RESULT;

#ifdef __GNUC__
#  define MEM_SAFE_FREE(v) \
    do { \
      typeof(&(v)) _v = &(v); \
      if (*_v) { \
        /* Cast so we can free constant arrays. */ \
        MEM_freeN((void *)*_v); \
        *_v = NULL; \
      } \
    } while (0)
#else
#  define MEM_SAFE_FREE(v) \
    do { \
      void **_v = (void **)&(v); \
      if (*_v) { \
        MEM_freeN(*_v); \
        *_v = NULL; \
      } \
    } while (0)
#endif

/* overhead for lockfree allocator (use to avoid slop-space) */
#define MEM_SIZE_OVERHEAD sizeof(size_t)
#define MEM_SIZE_OPTIMAL(size) ((size)-MEM_SIZE_OVERHEAD)

#ifndef NDEBUG
extern const char *(*MEM_name_ptr)(void *vmemh);
#endif

/** This should be called as early as possible in the program. When it has been called, information
 * about memory leaks will be printed on exit. */
void MEM_init_memleak_detection(void);

/**
 * Use this if we want to call #exit during argument parsing for example,
 * without having to free all data.
 */
void MEM_use_memleak_detection(bool enabled);

/** When this has been called and memory leaks have been detected, the process will have an exit
 * code that indicates failure. This can be used for when checking for memory leaks with automated
 * tests. */
void MEM_enable_fail_on_memleak(void);

/* Switch allocator to fast mode, with less tracking.
 *
 * Use in the production code where performance is the priority, and exact details about allocation
 * is not. This allocator keeps track of number of allocation and amount of allocated bytes, but it
 * does not track of names of allocated blocks.
 *
 * NOTE: The switch between allocator types can only happen before any allocation did happen. */
void MEM_use_lockfree_allocator(void);

/* Switch allocator to slow fully guarded mode.
 *
 * Use for debug purposes. This allocator contains lock section around every allocator call, which
 * makes it slow. What is gained with this is the ability to have list of allocated blocks (in an
 * addition to the tracking of number of allocations and amount of allocated bytes).
 *
 * NOTE: The switch between allocator types can only happen before any allocation did happen. */
void MEM_use_guarded_allocator(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus
/* Allocation functions (for C++ only). */
#  define MEM_CXX_CLASS_ALLOC_FUNCS(_id) \
   public: \
    void *operator new(size_t num_bytes) \
    { \
      return MEM_mallocN(num_bytes, _id); \
    } \
    void operator delete(void *mem) \
    { \
      if (mem) { \
        MEM_freeN(mem); \
      } \
    } \
    void *operator new[](size_t num_bytes) \
    { \
      return MEM_mallocN(num_bytes, _id "[]"); \
    } \
    void operator delete[](void *mem) \
    { \
      if (mem) { \
        MEM_freeN(mem); \
      } \
    } \
    void *operator new(size_t /*count*/, void *ptr) \
    { \
      return ptr; \
    } \
    /* This is the matching delete operator to the placement-new operator above. Both parameters \
     * will have the same value. Without this, we get the warning C4291 on windows. */ \
    void operator delete(void * /*ptr_to_free*/, void * /*ptr*/) \
    { \
    }

/* Needed when type includes a namespace, then the namespace should not be
 * specified after ~, so using a macro fails. */
template<class T> inline void OBJECT_GUARDED_DESTRUCTOR(T *what)
{
  what->~T();
}

#  if defined __GNUC__
#    define OBJECT_GUARDED_NEW(type, args...) new (MEM_mallocN(sizeof(type), __func__)) type(args)
#  else
#    define OBJECT_GUARDED_NEW(type, ...) \
      new (MEM_mallocN(sizeof(type), __FUNCTION__)) type(__VA_ARGS__)
#  endif
#  define OBJECT_GUARDED_DELETE(what, type) \
    { \
      if (what) { \
        OBJECT_GUARDED_DESTRUCTOR((type *)what); \
        MEM_freeN(what); \
      } \
    } \
    (void)0
#  define OBJECT_GUARDED_SAFE_DELETE(what, type) \
    { \
      if (what) { \
        OBJECT_GUARDED_DESTRUCTOR((type *)what); \
        MEM_freeN(what); \
        what = NULL; \
      } \
    } \
    (void)0
#endif /* __cplusplus */

#endif /* __MEM_GUARDEDALLOC_H__ */
