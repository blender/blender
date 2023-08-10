/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_mem
 *
 * Guarded memory allocation, and boundary-write detection.
 */

#include <stdarg.h>
#include <stddef.h> /* offsetof */
#include <stdio.h>  /* printf */
#include <stdlib.h>
#include <string.h> /* memcpy */
#include <sys/types.h>

#include <pthread.h>

#include "MEM_guardedalloc.h"

/* to ensure strict conversions */
#include "../../source/blender/blenlib/BLI_strict_flags.h"

#include "atomic_ops.h"
#include "mallocn_intern.h"

/* Only for debugging:
 * store original buffer's name when doing MEM_dupallocN
 * helpful to profile issues with non-freed "dup_alloc" buffers,
 * but this introduces some overhead to memory header and makes
 * things slower a bit, so better to keep disabled by default
 */
//#define DEBUG_MEMDUPLINAME

/* Only for debugging:
 * lets you count the allocations so as to find the allocator of unfreed memory
 * in situations where the leak is predictable */

//#define DEBUG_MEMCOUNTER

/* Only for debugging:
 * Defining DEBUG_BACKTRACE will display a backtrace from where memory block was allocated and
 * print this trace for all unfreed blocks. This will only work for ASAN enabled builds. This
 * option will be on by default for MSVC as it currently does not have LSAN which would normally
 * report these leaks, off by default on all other platforms because it would report the leaks
 * twice, once here, and once by LSAN.
 */
#if defined(_MSC_VER)
#  define DEBUG_BACKTRACE
#else
//#define DEBUG_BACKTRACE
#endif

#ifdef DEBUG_MEMCOUNTER
/* set this to the value that isn't being freed */
#  define DEBUG_MEMCOUNTER_ERROR_VAL 0
static int _mallocn_count = 0;

/* Break-point here. */
static void memcount_raise(const char *name)
{
  fprintf(stderr, "%s: memcount-leak, %d\n", name, _mallocn_count);
}
#endif

/* --------------------------------------------------------------------- */
/* Data definition                                                       */
/* --------------------------------------------------------------------- */
/* all memory chunks are put in linked lists */
typedef struct localLink {
  struct localLink *next, *prev;
} localLink;

typedef struct localListBase {
  void *first, *last;
} localListBase;

/* NOTE(@hos): keep this struct aligned (e.g., IRIX/GCC). */
typedef struct MemHead {
  int tag1;
  size_t len;
  struct MemHead *next, *prev;
  const char *name;
  const char *nextname;
  int tag2;
  short pad1;
  /* if non-zero aligned allocation was used and alignment is stored here. */
  short alignment;
#ifdef DEBUG_MEMCOUNTER
  int _count;
#endif

#ifdef DEBUG_MEMDUPLINAME
  int need_free_name, pad;
#endif
} MemHead;

typedef MemHead MemHeadAligned;

typedef struct MemTail {
  int tag3, pad;
} MemTail;

/* --------------------------------------------------------------------- */
/* local functions                                                       */
/* --------------------------------------------------------------------- */

static void addtail(volatile localListBase *listbase, void *vlink);
static void remlink(volatile localListBase *listbase, void *vlink);
static void rem_memblock(MemHead *memh);
static void MemorY_ErroR(const char *block, const char *error);
static const char *check_memlist(MemHead *memh);

/* --------------------------------------------------------------------- */
/* locally used defines                                                  */
/* --------------------------------------------------------------------- */

#ifdef __BIG_ENDIAN__
#  define MAKE_ID(a, b, c, d) ((int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d))
#else
#  define MAKE_ID(a, b, c, d) ((int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a))
#endif

#define MEMTAG1 MAKE_ID('M', 'E', 'M', 'O')
#define MEMTAG2 MAKE_ID('R', 'Y', 'B', 'L')
#define MEMTAG3 MAKE_ID('O', 'C', 'K', '!')
#define MEMFREE MAKE_ID('F', 'R', 'E', 'E')

#define MEMNEXT(x) ((MemHead *)(((char *)x) - offsetof(MemHead, next)))

/* --------------------------------------------------------------------- */
/* vars                                                                  */
/* --------------------------------------------------------------------- */

static uint totblock = 0;
static size_t mem_in_use = 0, peak_mem = 0;

static volatile localListBase _membase;
static volatile localListBase *membase = &_membase;
static void (*error_callback)(const char *) = NULL;

static bool malloc_debug_memset = false;

#ifdef malloc
#  undef malloc
#endif

#ifdef calloc
#  undef calloc
#endif

#ifdef free
#  undef free
#endif

/* --------------------------------------------------------------------- */
/* implementation                                                        */
/* --------------------------------------------------------------------- */

#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
static void
print_error(const char *str, ...)
{
  char buf[1024];
  va_list ap;

  va_start(ap, str);
  vsnprintf(buf, sizeof(buf), str, ap);
  va_end(ap);
  buf[sizeof(buf) - 1] = '\0';

  if (error_callback) {
    error_callback(buf);
  }
  else {
    fputs(buf, stderr);
  }
}

static pthread_mutex_t thread_lock = PTHREAD_MUTEX_INITIALIZER;

static void mem_lock_thread(void)
{
  pthread_mutex_lock(&thread_lock);
}

static void mem_unlock_thread(void)
{
  pthread_mutex_unlock(&thread_lock);
}

bool MEM_guarded_consistency_check(void)
{
  const char *err_val = NULL;
  MemHead *listend;
  /* check_memlist starts from the front, and runs until it finds
   * the requested chunk. For this test, that's the last one. */
  listend = membase->last;

  err_val = check_memlist(listend);

  return (err_val == NULL);
}

void MEM_guarded_set_error_callback(void (*func)(const char *))
{
  error_callback = func;
}

void MEM_guarded_set_memory_debug(void)
{
  malloc_debug_memset = true;
}

size_t MEM_guarded_allocN_len(const void *vmemh)
{
  if (vmemh) {
    const MemHead *memh = vmemh;

    memh--;
    return memh->len;
  }

  return 0;
}

void *MEM_guarded_dupallocN(const void *vmemh)
{
  void *newp = NULL;

  if (vmemh) {
    const MemHead *memh = vmemh;
    memh--;

#ifndef DEBUG_MEMDUPLINAME
    if (LIKELY(memh->alignment == 0)) {
      newp = MEM_guarded_mallocN(memh->len, "dupli_alloc");
    }
    else {
      newp = MEM_guarded_mallocN_aligned(memh->len, (size_t)memh->alignment, "dupli_alloc");
    }

    if (newp == NULL) {
      return NULL;
    }
#else
    {
      MemHead *nmemh;
      const char name_prefix[] = "dupli_alloc ";
      const size_t name_prefix_len = sizeof(name_prefix) - 1;
      const size_t name_size = strlen(memh->name) + 1;
      char *name = malloc(name_prefix_len + name_size);
      memcpy(name, name_prefix, sizeof(name_prefix));
      memcpy(name + name_prefix_len, memh->name, name_size);

      if (LIKELY(memh->alignment == 0)) {
        newp = MEM_guarded_mallocN(memh->len, name);
      }
      else {
        newp = MEM_guarded_mallocN_aligned(memh->len, (size_t)memh->alignment, name);
      }

      if (newp == NULL)
        return NULL;

      nmemh = newp;
      nmemh--;

      nmemh->need_free_name = 1;
    }
#endif

    memcpy(newp, vmemh, memh->len);
  }

  return newp;
}

void *MEM_guarded_reallocN_id(void *vmemh, size_t len, const char *str)
{
  void *newp = NULL;

  if (vmemh) {
    MemHead *memh = vmemh;
    memh--;

    if (LIKELY(memh->alignment == 0)) {
      newp = MEM_guarded_mallocN(len, memh->name);
    }
    else {
      newp = MEM_guarded_mallocN_aligned(len, (size_t)memh->alignment, memh->name);
    }

    if (newp) {
      if (len < memh->len) {
        /* shrink */
        memcpy(newp, vmemh, len);
      }
      else {
        /* grow (or remain same size) */
        memcpy(newp, vmemh, memh->len);
      }
    }

    MEM_guarded_freeN(vmemh);
  }
  else {
    newp = MEM_guarded_mallocN(len, str);
  }

  return newp;
}

void *MEM_guarded_recallocN_id(void *vmemh, size_t len, const char *str)
{
  void *newp = NULL;

  if (vmemh) {
    MemHead *memh = vmemh;
    memh--;

    if (LIKELY(memh->alignment == 0)) {
      newp = MEM_guarded_mallocN(len, memh->name);
    }
    else {
      newp = MEM_guarded_mallocN_aligned(len, (size_t)memh->alignment, memh->name);
    }

    if (newp) {
      if (len < memh->len) {
        /* shrink */
        memcpy(newp, vmemh, len);
      }
      else {
        memcpy(newp, vmemh, memh->len);

        if (len > memh->len) {
          /* grow */
          /* zero new bytes */
          memset(((char *)newp) + memh->len, 0, len - memh->len);
        }
      }
    }

    MEM_guarded_freeN(vmemh);
  }
  else {
    newp = MEM_guarded_callocN(len, str);
  }

  return newp;
}

static void make_memhead_header(MemHead *memh, size_t len, const char *str)
{
  MemTail *memt;

  memh->tag1 = MEMTAG1;
  memh->name = str;
  memh->nextname = NULL;
  memh->len = len;
  memh->pad1 = 0;
  memh->alignment = 0;
  memh->tag2 = MEMTAG2;

#ifdef DEBUG_MEMDUPLINAME
  memh->need_free_name = 0;
#endif

  memt = (MemTail *)(((char *)memh) + sizeof(MemHead) + len);
  memt->tag3 = MEMTAG3;

  atomic_add_and_fetch_u(&totblock, 1);
  atomic_add_and_fetch_z(&mem_in_use, len);

  mem_lock_thread();
  addtail(membase, &memh->next);
  if (memh->next) {
    memh->nextname = MEMNEXT(memh->next)->name;
  }
  peak_mem = mem_in_use > peak_mem ? mem_in_use : peak_mem;
  mem_unlock_thread();
}

void *MEM_guarded_mallocN(size_t len, const char *str)
{
  MemHead *memh;

  len = SIZET_ALIGN_4(len);

  memh = (MemHead *)malloc(len + sizeof(MemHead) + sizeof(MemTail));

  if (LIKELY(memh)) {
    make_memhead_header(memh, len, str);
    if (UNLIKELY(malloc_debug_memset && len)) {
      memset(memh + 1, 255, len);
    }

#ifdef DEBUG_MEMCOUNTER
    if (_mallocn_count == DEBUG_MEMCOUNTER_ERROR_VAL)
      memcount_raise(__func__);
    memh->_count = _mallocn_count++;
#endif
    return (++memh);
  }
  print_error("Malloc returns null: len=" SIZET_FORMAT " in %s, total " SIZET_FORMAT "\n",
              SIZET_ARG(len),
              str,
              mem_in_use);
  return NULL;
}

void *MEM_guarded_malloc_arrayN(size_t len, size_t size, const char *str)
{
  size_t total_size;
  if (UNLIKELY(!MEM_size_safe_multiply(len, size, &total_size))) {
    print_error(
        "Malloc array aborted due to integer overflow: "
        "len=" SIZET_FORMAT "x" SIZET_FORMAT " in %s, total " SIZET_FORMAT "\n",
        SIZET_ARG(len),
        SIZET_ARG(size),
        str,
        mem_in_use);
    abort();
    return NULL;
  }

  return MEM_guarded_mallocN(total_size, str);
}

void *MEM_guarded_mallocN_aligned(size_t len, size_t alignment, const char *str)
{
  /* We only support alignment to a power of two. */
  assert(IS_POW2(alignment));

  /* Use a minimal alignment of 8. Otherwise MEM_guarded_freeN thinks it is an illegal pointer. */
  if (alignment < 8) {
    alignment = 8;
  }

  /* It's possible that MemHead's size is not properly aligned,
   * do extra padding to deal with this.
   *
   * We only support small alignments which fits into short in
   * order to save some bits in MemHead structure.
   */
  size_t extra_padding = MEMHEAD_ALIGN_PADDING(alignment);

  /* Huge alignment values doesn't make sense and they
   * wouldn't fit into 'short' used in the MemHead.
   */
  assert(alignment < 1024);

  len = SIZET_ALIGN_4(len);

  MemHead *memh = (MemHead *)aligned_malloc(
      len + extra_padding + sizeof(MemHead) + sizeof(MemTail), alignment);

  if (LIKELY(memh)) {
    /* We keep padding in the beginning of MemHead,
     * this way it's always possible to get MemHead
     * from the data pointer.
     */
    memh = (MemHead *)((char *)memh + extra_padding);

    make_memhead_header(memh, len, str);
    memh->alignment = (short)alignment;
    if (UNLIKELY(malloc_debug_memset && len)) {
      memset(memh + 1, 255, len);
    }

#ifdef DEBUG_MEMCOUNTER
    if (_mallocn_count == DEBUG_MEMCOUNTER_ERROR_VAL)
      memcount_raise(__func__);
    memh->_count = _mallocn_count++;
#endif
    return (++memh);
  }
  print_error("aligned_malloc returns null: len=" SIZET_FORMAT " in %s, total " SIZET_FORMAT "\n",
              SIZET_ARG(len),
              str,
              mem_in_use);
  return NULL;
}

void *MEM_guarded_callocN(size_t len, const char *str)
{
  MemHead *memh;

  len = SIZET_ALIGN_4(len);

  memh = (MemHead *)calloc(len + sizeof(MemHead) + sizeof(MemTail), 1);

  if (memh) {
    make_memhead_header(memh, len, str);
#ifdef DEBUG_MEMCOUNTER
    if (_mallocn_count == DEBUG_MEMCOUNTER_ERROR_VAL)
      memcount_raise(__func__);
    memh->_count = _mallocn_count++;
#endif
    return (++memh);
  }
  print_error("Calloc returns null: len=" SIZET_FORMAT " in %s, total " SIZET_FORMAT "\n",
              SIZET_ARG(len),
              str,
              mem_in_use);
  return NULL;
}

void *MEM_guarded_calloc_arrayN(size_t len, size_t size, const char *str)
{
  size_t total_size;
  if (UNLIKELY(!MEM_size_safe_multiply(len, size, &total_size))) {
    print_error(
        "Calloc array aborted due to integer overflow: "
        "len=" SIZET_FORMAT "x" SIZET_FORMAT " in %s, total " SIZET_FORMAT "\n",
        SIZET_ARG(len),
        SIZET_ARG(size),
        str,
        mem_in_use);
    abort();
    return NULL;
  }

  return MEM_guarded_callocN(total_size, str);
}

/* Memory statistics print */
typedef struct MemPrintBlock {
  const char *name;
  uintptr_t len;
  int items;
} MemPrintBlock;

static int compare_name(const void *p1, const void *p2)
{
  const MemPrintBlock *pb1 = (const MemPrintBlock *)p1;
  const MemPrintBlock *pb2 = (const MemPrintBlock *)p2;

  return strcmp(pb1->name, pb2->name);
}

static int compare_len(const void *p1, const void *p2)
{
  const MemPrintBlock *pb1 = (const MemPrintBlock *)p1;
  const MemPrintBlock *pb2 = (const MemPrintBlock *)p2;

  if (pb1->len < pb2->len) {
    return 1;
  }
  if (pb1->len == pb2->len) {
    return 0;
  }

  return -1;
}

void MEM_guarded_printmemlist_stats(void)
{
  MemHead *membl;
  MemPrintBlock *pb, *printblock;
  uint totpb, a, b;
  size_t mem_in_use_slop = 0;

  mem_lock_thread();

  if (totblock != 0) {
    /* put memory blocks into array */
    printblock = malloc(sizeof(MemPrintBlock) * totblock);

    if (UNLIKELY(!printblock)) {
      mem_unlock_thread();
      print_error("malloc returned null while generating stats");
      return;
    }
  }
  else {
    printblock = NULL;
  }

  pb = printblock;
  totpb = 0;

  membl = membase->first;
  if (membl) {
    membl = MEMNEXT(membl);
  }

  while (membl && pb) {
    pb->name = membl->name;
    pb->len = membl->len;
    pb->items = 1;

    totpb++;
    pb++;

#ifdef USE_MALLOC_USABLE_SIZE
    if (membl->alignment == 0) {
      mem_in_use_slop += (sizeof(MemHead) + sizeof(MemTail) + malloc_usable_size((void *)membl)) -
                         membl->len;
    }
#endif

    if (membl->next) {
      membl = MEMNEXT(membl->next);
    }
    else {
      break;
    }
  }

  /* sort by name and add together blocks with the same name */
  if (totpb > 1) {
    qsort(printblock, totpb, sizeof(MemPrintBlock), compare_name);
  }

  for (a = 0, b = 0; a < totpb; a++) {
    if (a == b) {
      continue;
    }
    if (strcmp(printblock[a].name, printblock[b].name) == 0) {
      printblock[b].len += printblock[a].len;
      printblock[b].items++;
    }
    else {
      b++;
      memcpy(&printblock[b], &printblock[a], sizeof(MemPrintBlock));
    }
  }
  totpb = b + 1;

  /* sort by length and print */
  if (totpb > 1) {
    qsort(printblock, totpb, sizeof(MemPrintBlock), compare_len);
  }

  printf("\ntotal memory len: %.3f MB\n", (double)mem_in_use / (double)(1024 * 1024));
  printf("peak memory len: %.3f MB\n", (double)peak_mem / (double)(1024 * 1024));
  printf("slop memory len: %.3f MB\n", (double)mem_in_use_slop / (double)(1024 * 1024));
  printf(" ITEMS TOTAL-MiB AVERAGE-KiB TYPE\n");
  for (a = 0, pb = printblock; a < totpb; a++, pb++) {
    printf("%6d (%8.3f  %8.3f) %s\n",
           pb->items,
           (double)pb->len / (double)(1024 * 1024),
           (double)pb->len / 1024.0 / (double)pb->items,
           pb->name);
  }

  if (printblock != NULL) {
    free(printblock);
  }

  mem_unlock_thread();

#ifdef HAVE_MALLOC_STATS
  printf("System Statistics:\n");
  malloc_stats();
#endif
}

static const char mem_printmemlist_pydict_script[] =
    "mb_userinfo = {}\n"
    "totmem = 0\n"
    "for mb_item in membase:\n"
    "    mb_item_user_size = mb_userinfo.setdefault(mb_item['name'], [0,0])\n"
    "    mb_item_user_size[0] += 1 # Add a user\n"
    "    mb_item_user_size[1] += mb_item['len'] # Increment the size\n"
    "    totmem += mb_item['len']\n"
    "print('(membase) items:', len(membase), '| unique-names:',\n"
    "      len(mb_userinfo), '| total-mem:', totmem)\n"
    "mb_userinfo_sort = list(mb_userinfo.items())\n"
    "for sort_name, sort_func in (('size', lambda a: -a[1][1]),\n"
    "                             ('users', lambda a: -a[1][0]),\n"
    "                             ('name', lambda a: a[0])):\n"
    "    print('\\nSorting by:', sort_name)\n"
    "    mb_userinfo_sort.sort(key = sort_func)\n"
    "    for item in mb_userinfo_sort:\n"
    "        print('name:%%s, users:%%i, len:%%i' %%\n"
    "              (item[0], item[1][0], item[1][1]))\n";

/* Prints in python syntax for easy */
static void MEM_guarded_printmemlist_internal(int pydict)
{
  MemHead *membl;

  mem_lock_thread();

  membl = membase->first;
  if (membl) {
    membl = MEMNEXT(membl);
  }

  if (pydict) {
    print_error("# membase_debug.py\n");
    print_error("membase = [\n");
  }
  while (membl) {
    if (pydict) {
      print_error("    {'len':" SIZET_FORMAT
                  ", "
                  "'name':'''%s''', "
                  "'pointer':'%p'},\n",
                  SIZET_ARG(membl->len),
                  membl->name,
                  (void *)(membl + 1));
    }
    else {
#ifdef DEBUG_MEMCOUNTER
      print_error("%s len: " SIZET_FORMAT " %p, count: %d\n",
                  membl->name,
                  SIZET_ARG(membl->len),
                  membl + 1,
                  membl->_count);
#else
      print_error("%s len: " SIZET_FORMAT " %p\n",
                  membl->name,
                  SIZET_ARG(membl->len),
                  (void *)(membl + 1));
#endif
#ifdef DEBUG_BACKTRACE
#  ifdef WITH_ASAN
      __asan_describe_address(membl);
#  endif
#endif
    }
    if (membl->next) {
      membl = MEMNEXT(membl->next);
    }
    else {
      break;
    }
  }
  if (pydict) {
    print_error("]\n\n");
    print_error(mem_printmemlist_pydict_script);
  }

  mem_unlock_thread();
}

void MEM_guarded_callbackmemlist(void (*func)(void *))
{
  MemHead *membl;

  mem_lock_thread();

  membl = membase->first;
  if (membl) {
    membl = MEMNEXT(membl);
  }

  while (membl) {
    func(membl + 1);
    if (membl->next) {
      membl = MEMNEXT(membl->next);
    }
    else {
      break;
    }
  }

  mem_unlock_thread();
}

#if 0
short MEM_guarded_testN(void *vmemh)
{
  MemHead *membl;

  mem_lock_thread();

  membl = membase->first;
  if (membl)
    membl = MEMNEXT(membl);

  while (membl) {
    if (vmemh == membl + 1) {
      mem_unlock_thread();
      return 1;
    }

    if (membl->next)
      membl = MEMNEXT(membl->next);
    else
      break;
  }

  mem_unlock_thread();

  print_error("Memoryblock %p: pointer not in memlist\n", vmemh);
  return 0;
}
#endif

void MEM_guarded_printmemlist(void)
{
  MEM_guarded_printmemlist_internal(0);
}
void MEM_guarded_printmemlist_pydict(void)
{
  MEM_guarded_printmemlist_internal(1);
}
void mem_guarded_clearmemlist(void)
{
  membase->first = membase->last = NULL;
}

void MEM_guarded_freeN(void *vmemh)
{
  MemTail *memt;
  MemHead *memh = vmemh;
  const char *name;

  if (memh == NULL) {
    MemorY_ErroR("free", "attempt to free NULL pointer");
    // print_error(err_stream, "%d\n", (memh+4000)->tag1);
    return;
  }

  if (sizeof(intptr_t) == 8) {
    if (((intptr_t)memh) & 0x7) {
      MemorY_ErroR("free", "attempt to free illegal pointer");
      return;
    }
  }
  else {
    if (((intptr_t)memh) & 0x3) {
      MemorY_ErroR("free", "attempt to free illegal pointer");
      return;
    }
  }

  memh--;
  if (memh->tag1 == MEMFREE && memh->tag2 == MEMFREE) {
    MemorY_ErroR(memh->name, "double free");
    return;
  }

  if ((memh->tag1 == MEMTAG1) && (memh->tag2 == MEMTAG2) && ((memh->len & 0x3) == 0)) {
    memt = (MemTail *)(((char *)memh) + sizeof(MemHead) + memh->len);
    if (memt->tag3 == MEMTAG3) {

      if (leak_detector_has_run) {
        MemorY_ErroR(memh->name, free_after_leak_detection_message);
      }

      memh->tag1 = MEMFREE;
      memh->tag2 = MEMFREE;
      memt->tag3 = MEMFREE;
      /* after tags !!! */
      rem_memblock(memh);

      return;
    }
    MemorY_ErroR(memh->name, "end corrupt");
    name = check_memlist(memh);
    if (name != NULL) {
      if (name != memh->name) {
        MemorY_ErroR(name, "is also corrupt");
      }
    }
  }
  else {
    mem_lock_thread();
    name = check_memlist(memh);
    mem_unlock_thread();
    if (name == NULL) {
      MemorY_ErroR("free", "pointer not in memlist");
    }
    else {
      MemorY_ErroR(name, "error in header");
    }
  }

  totblock--;
  /* here a DUMP should happen */
}

/* --------------------------------------------------------------------- */
/* local functions                                                       */
/* --------------------------------------------------------------------- */

static void addtail(volatile localListBase *listbase, void *vlink)
{
  localLink *link = vlink;

  /* for a generic API error checks here is fine but
   * the limited use here they will never be NULL */
#if 0
  if (link == NULL)
    return;
  if (listbase == NULL)
    return;
#endif

  link->next = NULL;
  link->prev = listbase->last;

  if (listbase->last) {
    ((localLink *)listbase->last)->next = link;
  }
  if (listbase->first == NULL) {
    listbase->first = link;
  }
  listbase->last = link;
}

static void remlink(volatile localListBase *listbase, void *vlink)
{
  localLink *link = vlink;

  /* for a generic API error checks here is fine but
   * the limited use here they will never be NULL */
#if 0
  if (link == NULL)
    return;
  if (listbase == NULL)
    return;
#endif

  if (link->next) {
    link->next->prev = link->prev;
  }
  if (link->prev) {
    link->prev->next = link->next;
  }

  if (listbase->last == link) {
    listbase->last = link->prev;
  }
  if (listbase->first == link) {
    listbase->first = link->next;
  }
}

static void rem_memblock(MemHead *memh)
{
  mem_lock_thread();
  remlink(membase, &memh->next);
  if (memh->prev) {
    if (memh->next) {
      MEMNEXT(memh->prev)->nextname = MEMNEXT(memh->next)->name;
    }
    else {
      MEMNEXT(memh->prev)->nextname = NULL;
    }
  }
  mem_unlock_thread();

  atomic_sub_and_fetch_u(&totblock, 1);
  atomic_sub_and_fetch_z(&mem_in_use, memh->len);

#ifdef DEBUG_MEMDUPLINAME
  if (memh->need_free_name)
    free((char *)memh->name);
#endif

  if (UNLIKELY(malloc_debug_memset && memh->len)) {
    memset(memh + 1, 255, memh->len);
  }
  if (LIKELY(memh->alignment == 0)) {
    free(memh);
  }
  else {
    aligned_free(MEMHEAD_REAL_PTR(memh));
  }
}

static void MemorY_ErroR(const char *block, const char *error)
{
  print_error("Memoryblock %s: %s\n", block, error);

#ifdef WITH_ASSERT_ABORT
  abort();
#endif
}

static const char *check_memlist(MemHead *memh)
{
  MemHead *forw, *back, *forwok, *backok;
  const char *name;

  forw = membase->first;
  if (forw) {
    forw = MEMNEXT(forw);
  }
  forwok = NULL;
  while (forw) {
    if (forw->tag1 != MEMTAG1 || forw->tag2 != MEMTAG2) {
      break;
    }
    forwok = forw;
    if (forw->next) {
      forw = MEMNEXT(forw->next);
    }
    else {
      forw = NULL;
    }
  }

  back = (MemHead *)membase->last;
  if (back) {
    back = MEMNEXT(back);
  }
  backok = NULL;
  while (back) {
    if (back->tag1 != MEMTAG1 || back->tag2 != MEMTAG2) {
      break;
    }
    backok = back;
    if (back->prev) {
      back = MEMNEXT(back->prev);
    }
    else {
      back = NULL;
    }
  }

  if (forw != back) {
    return ("MORE THAN 1 MEMORYBLOCK CORRUPT");
  }

  if (forw == NULL && back == NULL) {
    /* no wrong headers found then but in search of memblock */

    forw = membase->first;
    if (forw) {
      forw = MEMNEXT(forw);
    }
    forwok = NULL;
    while (forw) {
      if (forw == memh) {
        break;
      }
      if (forw->tag1 != MEMTAG1 || forw->tag2 != MEMTAG2) {
        break;
      }
      forwok = forw;
      if (forw->next) {
        forw = MEMNEXT(forw->next);
      }
      else {
        forw = NULL;
      }
    }
    if (forw == NULL) {
      return NULL;
    }

    back = (MemHead *)membase->last;
    if (back) {
      back = MEMNEXT(back);
    }
    backok = NULL;
    while (back) {
      if (back == memh) {
        break;
      }
      if (back->tag1 != MEMTAG1 || back->tag2 != MEMTAG2) {
        break;
      }
      backok = back;
      if (back->prev) {
        back = MEMNEXT(back->prev);
      }
      else {
        back = NULL;
      }
    }
  }

  if (forwok) {
    name = forwok->nextname;
  }
  else {
    name = "No name found";
  }

  if (forw == memh) {
    /* to be sure but this block is removed from the list */
    if (forwok) {
      if (backok) {
        forwok->next = (MemHead *)&backok->next;
        backok->prev = (MemHead *)&forwok->next;
        forwok->nextname = backok->name;
      }
      else {
        forwok->next = NULL;
        membase->last = (localLink *)&forwok->next;
      }
    }
    else {
      if (backok) {
        backok->prev = NULL;
        membase->first = &backok->next;
      }
      else {
        membase->first = membase->last = NULL;
      }
    }
  }
  else {
    MemorY_ErroR(name, "Additional error in header");
    return ("Additional error in header");
  }

  return name;
}

size_t MEM_guarded_get_peak_memory(void)
{
  size_t _peak_mem;

  mem_lock_thread();
  _peak_mem = peak_mem;
  mem_unlock_thread();

  return _peak_mem;
}

void MEM_guarded_reset_peak_memory(void)
{
  mem_lock_thread();
  peak_mem = mem_in_use;
  mem_unlock_thread();
}

size_t MEM_guarded_get_memory_in_use(void)
{
  size_t _mem_in_use;

  mem_lock_thread();
  _mem_in_use = mem_in_use;
  mem_unlock_thread();

  return _mem_in_use;
}

uint MEM_guarded_get_memory_blocks_in_use(void)
{
  uint _totblock;

  mem_lock_thread();
  _totblock = totblock;
  mem_unlock_thread();

  return _totblock;
}

#ifndef NDEBUG
const char *MEM_guarded_name_ptr(void *vmemh)
{
  if (vmemh) {
    MemHead *memh = vmemh;
    memh--;
    return memh->name;
  }

  return "MEM_guarded_name_ptr(NULL)";
}

void MEM_guarded_name_ptr_set(void *vmemh, const char *str)
{
  if (!vmemh) {
    return;
  }

  MemHead *memh = vmemh;
  memh--;
  memh->name = str;
  if (memh->prev) {
    MEMNEXT(memh->prev)->nextname = str;
  }
}
#endif /* NDEBUG */
