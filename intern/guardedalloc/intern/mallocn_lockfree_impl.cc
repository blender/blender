/* SPDX-FileCopyrightText: 2013-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_mem
 *
 * Memory allocation which keeps track on allocated memory counters
 */

#include <stdarg.h>
#include <stdio.h> /* printf */
#include <stdlib.h>
#include <string.h> /* memcpy */
#include <sys/types.h>

#include "MEM_guardedalloc.h"

/* Quiet warnings when dealing with allocated data written into the blend file.
 * This also rounds up and causes warnings which we don't consider bugs in practice. */
#ifdef WITH_MEM_VALGRIND
#  include "valgrind/memcheck.h"
#endif

/* to ensure strict conversions */
#include "../../source/blender/blenlib/BLI_strict_flags.h"

#include "atomic_ops.h"
#include "mallocn_intern.hh"
#include "mallocn_intern_function_pointers.hh"

using namespace mem_guarded::internal;

namespace {

typedef struct MemHead {
  /* Length of allocated memory block. */
  size_t len;
} MemHead;
static_assert(MEM_MIN_CPP_ALIGNMENT <= alignof(MemHead), "Bad alignment of MemHead");
static_assert(MEM_MIN_CPP_ALIGNMENT <= sizeof(MemHead), "Bad size of MemHead");

typedef struct MemHeadAligned {
  short alignment;
  size_t len;
} MemHeadAligned;
static_assert(MEM_MIN_CPP_ALIGNMENT <= alignof(MemHeadAligned), "Bad alignment of MemHeadAligned");
static_assert(MEM_MIN_CPP_ALIGNMENT <= sizeof(MemHeadAligned), "Bad size of MemHeadAligned");

}  // namespace

static bool malloc_debug_memset = false;

static void (*error_callback)(const char *) = nullptr;

/**
 * Guardedalloc always allocate multiple of 4 bytes. That means that the lower 2 bits of the
 * `len` member of #MemHead/#MemHeadAligned data can be used for the bitflags below.
 */
enum {
  /** This block used aligned allocation, and its 'head' is of #MemHeadAligned type. */
  MEMHEAD_FLAG_ALIGN = 1 << 0,
  /**
   * This block of memory has been allocated from CPP `new` (e.g. #MEM_new, or some
   * guardedalloc-overloaded `new` operator). It mainly checks that #MEM_freeN is not directly
   * called on it (#MEM_delete or some guardedalloc-overloaded `delete` operator should always be
   * used instead).
   */
  MEMHEAD_FLAG_FROM_CPP_NEW = 1 << 1,

  MEMHEAD_FLAG_MASK = (1 << 2) - 1
};

#define MEMHEAD_FROM_PTR(ptr) (((MemHead *)ptr) - 1)
#define PTR_FROM_MEMHEAD(memhead) (memhead + 1)
#define MEMHEAD_ALIGNED_FROM_PTR(ptr) (((MemHeadAligned *)ptr) - 1)
#define MEMHEAD_IS_ALIGNED(memhead) ((memhead)->len & size_t(MEMHEAD_FLAG_ALIGN))
#define MEMHEAD_IS_FROM_CPP_NEW(memhead) ((memhead)->len & size_t(MEMHEAD_FLAG_FROM_CPP_NEW))
#define MEMHEAD_LEN(memhead) ((memhead)->len & ~size_t(MEMHEAD_FLAG_MASK))

#ifdef __GNUC__
__attribute__((format(printf, 1, 0)))
#endif
static void
print_error(const char *message, va_list str_format_args)
{
  char buf[512];
  vsnprintf(buf, sizeof(buf), message, str_format_args);
  buf[sizeof(buf) - 1] = '\0';

  if (error_callback) {
    error_callback(buf);
  }
}

#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
static void
print_error(const char *message, ...)
{
  va_list str_format_args;
  va_start(str_format_args, message);
  print_error(message, str_format_args);
  va_end(str_format_args);
}

#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
static void
report_error_on_address(const void *vmemh, const char *message, ...)
{
  va_list str_format_args;

  va_start(str_format_args, message);
  print_error(message, str_format_args);
  va_end(str_format_args);

  if (vmemh == nullptr) {
    MEM_trigger_error_on_memory_block(nullptr, 0);
    return;
  }

  const MemHead *memh = MEMHEAD_FROM_PTR(vmemh);
  const size_t len = MEMHEAD_LEN(memh);

  const void *address = memh;
  size_t size = len + sizeof(*memh);
  if (UNLIKELY(MEMHEAD_IS_ALIGNED(memh))) {
    const MemHeadAligned *memh_aligned = MEMHEAD_ALIGNED_FROM_PTR(vmemh);
    address = MEMHEAD_REAL_PTR(memh_aligned);
    size = len + sizeof(*memh_aligned) + MEMHEAD_ALIGN_PADDING(memh_aligned->alignment);
  }
  MEM_trigger_error_on_memory_block(address, size);
}

size_t MEM_lockfree_allocN_len(const void *vmemh)
{
  if (LIKELY(vmemh)) {
    return MEMHEAD_LEN(MEMHEAD_FROM_PTR(vmemh));
  }

  return 0;
}

void MEM_lockfree_freeN(void *vmemh, AllocationType allocation_type)
{
  if (UNLIKELY(leak_detector_has_run)) {
    print_error("%s\n", free_after_leak_detection_message);
  }

  if (UNLIKELY(vmemh == nullptr)) {
    report_error_on_address(vmemh, "Attempt to free nullptr pointer\n");
    return;
  }

  MemHead *memh = MEMHEAD_FROM_PTR(vmemh);
  size_t len = MEMHEAD_LEN(memh);

  if (allocation_type != AllocationType::NEW_DELETE && MEMHEAD_IS_FROM_CPP_NEW(memh)) {
    report_error_on_address(
        vmemh,
        "Attempt to use C-style MEM_freeN on a pointer created with CPP-style MEM_new or new\n");
  }

  memory_usage_block_free(len);

  if (UNLIKELY(malloc_debug_memset && len)) {
    memset(memh + 1, 255, len);
  }
  if (UNLIKELY(MEMHEAD_IS_ALIGNED(memh))) {
    MemHeadAligned *memh_aligned = MEMHEAD_ALIGNED_FROM_PTR(vmemh);
    aligned_free(MEMHEAD_REAL_PTR(memh_aligned));
  }
  else {
    free(memh);
  }
}

void *MEM_lockfree_dupallocN(const void *vmemh)
{
  void *newp = nullptr;
  if (vmemh) {
    const MemHead *memh = MEMHEAD_FROM_PTR(vmemh);
    const size_t prev_size = MEM_lockfree_allocN_len(vmemh);

    if (MEMHEAD_IS_FROM_CPP_NEW(memh)) {
      report_error_on_address(vmemh,
                              "Attempt to use C-style MEM_dupallocN on a pointer created with "
                              "CPP-style MEM_new or new\n");
    }

    if (UNLIKELY(MEMHEAD_IS_ALIGNED(memh))) {
      const MemHeadAligned *memh_aligned = MEMHEAD_ALIGNED_FROM_PTR(vmemh);
      newp = MEM_lockfree_mallocN_aligned(
          prev_size, size_t(memh_aligned->alignment), "dupli_malloc", AllocationType::ALLOC_FREE);
    }
    else {
      newp = MEM_lockfree_mallocN(prev_size, "dupli_malloc");
    }
    memcpy(newp, vmemh, prev_size);
  }
  return newp;
}

void *MEM_lockfree_reallocN_id(void *vmemh, size_t len, const char *str)
{
  void *newp = nullptr;

  if (vmemh) {
    const MemHead *memh = MEMHEAD_FROM_PTR(vmemh);
    const size_t old_len = MEM_lockfree_allocN_len(vmemh);

    if (MEMHEAD_IS_FROM_CPP_NEW(memh)) {
      report_error_on_address(vmemh,
                              "Attempt to use C-style MEM_reallocN on a pointer created with "
                              "CPP-style MEM_new or new\n");
    }

    if (LIKELY(!MEMHEAD_IS_ALIGNED(memh))) {
      newp = MEM_lockfree_mallocN(len, "realloc");
    }
    else {
      const MemHeadAligned *memh_aligned = MEMHEAD_ALIGNED_FROM_PTR(vmemh);
      newp = MEM_lockfree_mallocN_aligned(
          len, size_t(memh_aligned->alignment), "realloc", AllocationType::ALLOC_FREE);
    }

    if (newp) {
      if (len < old_len) {
        /* shrink */
        memcpy(newp, vmemh, len);
      }
      else {
        /* grow (or remain same size) */
        memcpy(newp, vmemh, old_len);
      }
    }

    MEM_lockfree_freeN(vmemh, AllocationType::ALLOC_FREE);
  }
  else {
    newp = MEM_lockfree_mallocN(len, str);
  }

  return newp;
}

void *MEM_lockfree_recallocN_id(void *vmemh, size_t len, const char *str)
{
  void *newp = nullptr;

  if (vmemh) {
    const MemHead *memh = MEMHEAD_FROM_PTR(vmemh);
    const size_t old_len = MEM_lockfree_allocN_len(vmemh);

    if (MEMHEAD_IS_FROM_CPP_NEW(memh)) {
      report_error_on_address(vmemh,
                              "Attempt to use C-style MEM_recallocN on a pointer created with "
                              "CPP-style MEM_new or new\n");
    }

    if (LIKELY(!MEMHEAD_IS_ALIGNED(memh))) {
      newp = MEM_lockfree_mallocN(len, "recalloc");
    }
    else {
      const MemHeadAligned *memh_aligned = MEMHEAD_ALIGNED_FROM_PTR(vmemh);
      newp = MEM_lockfree_mallocN_aligned(
          len, size_t(memh_aligned->alignment), "recalloc", AllocationType::ALLOC_FREE);
    }

    if (newp) {
      if (len < old_len) {
        /* shrink */
        memcpy(newp, vmemh, len);
      }
      else {
        memcpy(newp, vmemh, old_len);

        if (len > old_len) {
          /* grow */
          /* zero new bytes */
          memset(((char *)newp) + old_len, 0, len - old_len);
        }
      }
    }

    MEM_lockfree_freeN(vmemh, AllocationType::ALLOC_FREE);
  }
  else {
    newp = MEM_lockfree_callocN(len, str);
  }

  return newp;
}

void *MEM_lockfree_callocN(size_t len, const char *str)
{
  MemHead *memh;

  len = SIZET_ALIGN_4(len);

  memh = (MemHead *)calloc(1, len + sizeof(MemHead));

  if (LIKELY(memh)) {
    memh->len = len;
    memory_usage_block_alloc(len);

    return PTR_FROM_MEMHEAD(memh);
  }
  print_error("Calloc returns null: len=" SIZET_FORMAT " in %s, total " SIZET_FORMAT "\n",
              SIZET_ARG(len),
              str,
              memory_usage_current());
  return nullptr;
}

void *MEM_lockfree_calloc_arrayN(size_t len, size_t size, const char *str)
{
  size_t total_size;
  if (UNLIKELY(!MEM_size_safe_multiply(len, size, &total_size))) {
    print_error(
        "Calloc array aborted due to integer overflow: "
        "len=" SIZET_FORMAT "x" SIZET_FORMAT " in %s, total " SIZET_FORMAT "\n",
        SIZET_ARG(len),
        SIZET_ARG(size),
        str,
        memory_usage_current());
    abort();
    return nullptr;
  }

  return MEM_lockfree_callocN(total_size, str);
}

void *MEM_lockfree_mallocN(size_t len, const char *str)
{
  MemHead *memh;

#ifdef WITH_MEM_VALGRIND
  const size_t len_unaligned = len;
#endif
  len = SIZET_ALIGN_4(len);

  memh = (MemHead *)malloc(len + sizeof(MemHead));

  if (LIKELY(memh)) {

    if (LIKELY(len)) {
      if (UNLIKELY(malloc_debug_memset)) {
        memset(memh + 1, 255, len);
      }
#ifdef WITH_MEM_VALGRIND
      if (malloc_debug_memset) {
        VALGRIND_MAKE_MEM_UNDEFINED(memh + 1, len_unaligned);
      }
      else {
        VALGRIND_MAKE_MEM_DEFINED((const char *)(memh + 1) + len_unaligned, len - len_unaligned);
      }
#endif /* WITH_MEM_VALGRIND */
    }

    memh->len = len;
    memory_usage_block_alloc(len);

    return PTR_FROM_MEMHEAD(memh);
  }
  print_error("Malloc returns null: len=" SIZET_FORMAT " in %s, total " SIZET_FORMAT "\n",
              SIZET_ARG(len),
              str,
              memory_usage_current());
  return nullptr;
}

void *MEM_lockfree_malloc_arrayN(size_t len, size_t size, const char *str)
{
  size_t total_size;
  if (UNLIKELY(!MEM_size_safe_multiply(len, size, &total_size))) {
    print_error(
        "Malloc array aborted due to integer overflow: "
        "len=" SIZET_FORMAT "x" SIZET_FORMAT " in %s, total " SIZET_FORMAT "\n",
        SIZET_ARG(len),
        SIZET_ARG(size),
        str,
        memory_usage_current());
    abort();
    return nullptr;
  }

  return MEM_lockfree_mallocN(total_size, str);
}

void *MEM_lockfree_mallocN_aligned(size_t len,
                                   size_t alignment,
                                   const char *str,
                                   const AllocationType allocation_type)
{
  /* Huge alignment values doesn't make sense and they wouldn't fit into 'short' used in the
   * MemHead. */
  assert(alignment < 1024);

  /* We only support alignments that are a power of two. */
  assert(IS_POW2(alignment));

  /* Some OS specific aligned allocators require a certain minimal alignment. */
  if (alignment < ALIGNED_MALLOC_MINIMUM_ALIGNMENT) {
    alignment = ALIGNED_MALLOC_MINIMUM_ALIGNMENT;
  }

  /* It's possible that MemHead's size is not properly aligned,
   * do extra padding to deal with this.
   *
   * We only support small alignments which fits into short in
   * order to save some bits in MemHead structure.
   */
  size_t extra_padding = MEMHEAD_ALIGN_PADDING(alignment);

#ifdef WITH_MEM_VALGRIND
  const size_t len_unaligned = len;
#endif
  len = SIZET_ALIGN_4(len);

  MemHeadAligned *memh = (MemHeadAligned *)aligned_malloc(
      len + extra_padding + sizeof(MemHeadAligned), alignment);

  if (LIKELY(memh)) {
    /* We keep padding in the beginning of MemHead,
     * this way it's always possible to get MemHead
     * from the data pointer.
     */
    memh = (MemHeadAligned *)((char *)memh + extra_padding);

    if (LIKELY(len)) {
      if (UNLIKELY(malloc_debug_memset)) {
        memset(memh + 1, 255, len);
      }
#ifdef WITH_MEM_VALGRIND
      if (malloc_debug_memset) {
        VALGRIND_MAKE_MEM_UNDEFINED(memh + 1, len_unaligned);
      }
      else {
        VALGRIND_MAKE_MEM_DEFINED((const char *)(memh + 1) + len_unaligned, len - len_unaligned);
      }
#endif /* WITH_MEM_VALGRIND */
    }

    memh->len = len | size_t(MEMHEAD_FLAG_ALIGN) |
                size_t(allocation_type == AllocationType::NEW_DELETE ? MEMHEAD_FLAG_FROM_CPP_NEW :
                                                                       0);
    memh->alignment = short(alignment);
    memory_usage_block_alloc(len);

    return PTR_FROM_MEMHEAD(memh);
  }
  print_error("Malloc returns null: len=" SIZET_FORMAT " in %s, total " SIZET_FORMAT "\n",
              SIZET_ARG(len),
              str,
              memory_usage_current());
  return nullptr;
}

static void *mem_lockfree_malloc_arrayN_aligned(const size_t len,
                                                const size_t size,
                                                const size_t alignment,
                                                const char *str,
                                                size_t &r_bytes_num)
{
  if (UNLIKELY(!MEM_size_safe_multiply(len, size, &r_bytes_num))) {
    print_error(
        "Calloc array aborted due to integer overflow: "
        "len=" SIZET_FORMAT "x" SIZET_FORMAT " in %s, total " SIZET_FORMAT "\n",
        SIZET_ARG(len),
        SIZET_ARG(size),
        str,
        memory_usage_current());
    abort();
    return nullptr;
  }
  if (alignment <= MEM_MIN_CPP_ALIGNMENT) {
    return mem_mallocN(r_bytes_num, str);
  }
  void *ptr = MEM_mallocN_aligned(r_bytes_num, alignment, str);
  return ptr;
}

void *MEM_lockfree_malloc_arrayN_aligned(const size_t len,
                                         const size_t size,
                                         const size_t alignment,
                                         const char *str)
{
  size_t bytes_num;
  return mem_lockfree_malloc_arrayN_aligned(len, size, alignment, str, bytes_num);
}

void *MEM_lockfree_calloc_arrayN_aligned(const size_t len,
                                         const size_t size,
                                         const size_t alignment,
                                         const char *str)
{
  /* There is no lower level #calloc with an alignment parameter, so unless the alignment is less
   * than or equal to what we'd get by default, we have to fall back to #memset unfortunately. */
  if (alignment <= MEM_MIN_CPP_ALIGNMENT) {
    return MEM_lockfree_calloc_arrayN(len, size, str);
  }

  size_t bytes_num;
  void *ptr = mem_lockfree_malloc_arrayN_aligned(len, size, alignment, str, bytes_num);
  if (!ptr) {
    return nullptr;
  }
  memset(ptr, 0, bytes_num);
  return ptr;
}

void MEM_lockfree_printmemlist_pydict() {}

void MEM_lockfree_printmemlist() {}

void mem_lockfree_clearmemlist() {}

/* Unused. */

void MEM_lockfree_callbackmemlist(void (*func)(void *))
{
  (void)func; /* Ignored. */
}

void MEM_lockfree_printmemlist_stats()
{
  printf("\ntotal memory len: %.3f MB\n", double(memory_usage_current()) / double(1024 * 1024));
  printf("peak memory len: %.3f MB\n", double(memory_usage_peak()) / double(1024 * 1024));
  printf(
      "\nFor more detailed per-block statistics run Blender with memory debugging command line "
      "argument.\n");

#ifdef HAVE_MALLOC_STATS
  printf("System Statistics:\n");
  malloc_stats();
#endif
}

void MEM_lockfree_set_error_callback(void (*func)(const char *))
{
  error_callback = func;
}

bool MEM_lockfree_consistency_check()
{
  return true;
}

void MEM_lockfree_set_memory_debug()
{
  malloc_debug_memset = true;
}

size_t MEM_lockfree_get_memory_in_use()
{
  return memory_usage_current();
}

uint MEM_lockfree_get_memory_blocks_in_use()
{
  return uint(memory_usage_block_num());
}

/* Dummy. */

void MEM_lockfree_reset_peak_memory()
{
  memory_usage_peak_reset();
}

size_t MEM_lockfree_get_peak_memory()
{
  return memory_usage_peak();
}

#ifndef NDEBUG
const char *MEM_lockfree_name_ptr(void *vmemh)
{
  if (vmemh) {
    return "unknown block name ptr";
  }

  return "MEM_lockfree_name_ptr(nullptr)";
}

void MEM_lockfree_name_ptr_set(void * /*vmemh*/, const char * /*str*/) {}
#endif /* !NDEBUG */
