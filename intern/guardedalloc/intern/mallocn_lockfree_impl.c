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
 * Contributor(s): Brecht Van Lommel
 *                 Campbell Barton
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file guardedalloc/intern/mallocn.c
 *  \ingroup MEM
 *
 * Memory allocation which keeps track on allocated memory counters
 */

#include <stdlib.h>
#include <string.h> /* memcpy */
#include <stdarg.h>
#include <sys/types.h>

#include "MEM_guardedalloc.h"

/* to ensure strict conversions */
#include "../../source/blender/blenlib/BLI_strict_flags.h"

#include "atomic_ops.h"
#include "mallocn_intern.h"

typedef struct MemHead {
	/* Length of allocated memory block. */
	size_t len;
} MemHead;

static unsigned int totblock = 0;
static size_t mem_in_use = 0, mmap_in_use = 0, peak_mem = 0;
static bool malloc_debug_memset = false;

static void (*error_callback)(const char *) = NULL;
static void (*thread_lock_callback)(void) = NULL;
static void (*thread_unlock_callback)(void) = NULL;

#define MEMHEAD_FROM_PTR(ptr) (((MemHead*) vmemh) - 1)
#define PTR_FROM_MEMHEAD(memhead) (memhead + 1)
#define MEMHEAD_IS_MMAP(memhead) ((memhead)->len & (size_t) 1)

#ifdef __GNUC__
__attribute__ ((format(printf, 1, 2)))
#endif
static void print_error(const char *str, ...)
{
	char buf[512];
	va_list ap;

	va_start(ap, str);
	vsnprintf(buf, sizeof(buf), str, ap);
	va_end(ap);
	buf[sizeof(buf) - 1] = '\0';

	if (error_callback) {
		error_callback(buf);
	}
}

#if defined(WIN32)
static void mem_lock_thread(void)
{
	if (thread_lock_callback)
		thread_lock_callback();
}

static void mem_unlock_thread(void)
{
	if (thread_unlock_callback)
		thread_unlock_callback();
}
#endif

size_t MEM_lockfree_allocN_len(const void *vmemh)
{
	if (vmemh) {
		return MEMHEAD_FROM_PTR(vmemh)->len & ~((size_t) 1);
	}
	else {
		return 0;
	}
}

void MEM_lockfree_freeN(void *vmemh)
{
	MemHead *memh = MEMHEAD_FROM_PTR(vmemh);
	size_t len = MEM_lockfree_allocN_len(vmemh);

	atomic_sub_u(&totblock, 1);
	atomic_sub_z(&mem_in_use, len);

	if (MEMHEAD_IS_MMAP(memh)) {
		atomic_sub_z(&mmap_in_use, len);
#if defined(WIN32)
		/* our windows mmap implementation is not thread safe */
		mem_lock_thread();
#endif
		if (munmap(memh, len + sizeof(MemHead)))
			printf("Couldn't unmap memory\n");
#if defined(WIN32)
		mem_unlock_thread();
#endif
	}
	else {
		if (malloc_debug_memset && len) {
			memset(memh + 1, 255, len);
		}
		free(memh);
	}
}

void *MEM_lockfree_dupallocN(const void *vmemh)
{
	void *newp = NULL;
	if (vmemh) {
		MemHead *memh = MEMHEAD_FROM_PTR(vmemh);
		const size_t prev_size = MEM_allocN_len(vmemh);
		if (MEMHEAD_IS_MMAP(memh)) {
			newp = MEM_lockfree_mapallocN(prev_size, "dupli_mapalloc");
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
	void *newp = NULL;

	if (vmemh) {
		size_t old_len = MEM_allocN_len(vmemh);

		newp = MEM_lockfree_mallocN(len, "realloc");
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

		MEM_lockfree_freeN(vmemh);
	}
	else {
		newp = MEM_lockfree_mallocN(len, str);
	}

	return newp;
}

void *MEM_lockfree_recallocN_id(void *vmemh, size_t len, const char *str)
{
	void *newp = NULL;

	if (vmemh) {
		size_t old_len = MEM_allocN_len(vmemh);

		newp = MEM_lockfree_mallocN(len, "recalloc");
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

		MEM_lockfree_freeN(vmemh);
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

	if (memh) {
		memh->len = len;
		atomic_add_u(&totblock, 1);
		atomic_add_z(&mem_in_use, len);

		/* TODO(sergey): Not strictly speaking thread-safe. */
		peak_mem = mem_in_use > peak_mem ? mem_in_use : peak_mem;

		return PTR_FROM_MEMHEAD(memh);
	}
	print_error("Calloc returns null: len=" SIZET_FORMAT " in %s, total %u\n",
	            SIZET_ARG(len), str, (unsigned int) mem_in_use);
	return NULL;
}

void *MEM_lockfree_mallocN(size_t len, const char *str)
{
	MemHead *memh;

	len = SIZET_ALIGN_4(len);

	memh = (MemHead *)malloc(len + sizeof(MemHead));

	if (memh) {
		if (malloc_debug_memset && len) {
			memset(memh + 1, 255, len);
		}

		memh->len = len;
		atomic_add_u(&totblock, 1);
		atomic_add_z(&mem_in_use, len);

		/* TODO(sergey): Not strictly speaking thread-safe. */
		peak_mem = mem_in_use > peak_mem ? mem_in_use : peak_mem;

		return PTR_FROM_MEMHEAD(memh);
	}
	print_error("Malloc returns null: len=" SIZET_FORMAT " in %s, total %u\n",
	            SIZET_ARG(len), str, (unsigned int) mem_in_use);
	return NULL;
}

void *MEM_lockfree_mapallocN(size_t len, const char *str)
{
	MemHead *memh;

	/* on 64 bit, simply use calloc instead, as mmap does not support
	 * allocating > 4 GB on Windows. the only reason mapalloc exists
	 * is to get around address space limitations in 32 bit OSes. */
	if(sizeof(void*) >= 8)
		return MEM_lockfree_callocN(len, str);

	len = SIZET_ALIGN_4(len);

#if defined(WIN32)
	/* our windows mmap implementation is not thread safe */
	mem_lock_thread();
#endif
	memh = mmap(NULL, len + sizeof(MemHead),
	            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
#if defined(WIN32)
	mem_unlock_thread();
#endif

	if (memh != (MemHead *)-1) {
		memh->len = len | (size_t) 1;
		atomic_add_u(&totblock, 1);
		atomic_add_z(&mem_in_use, len);
		atomic_add_z(&mmap_in_use, len);

		/* TODO(sergey): Not strictly speaking thread-safe. */
		peak_mem = mem_in_use > peak_mem ? mem_in_use : peak_mem;
		peak_mem = mmap_in_use > peak_mem ? mmap_in_use : peak_mem;

		return PTR_FROM_MEMHEAD(memh);
	}
	print_error("Mapalloc returns null, fallback to regular malloc: "
	            "len=" SIZET_FORMAT " in %s, total %u\n",
	            SIZET_ARG(len), str, (unsigned int) mmap_in_use);
	return MEM_lockfree_callocN(len, str);
}

void MEM_lockfree_printmemlist_pydict(void)
{
}

void MEM_lockfree_printmemlist(void)
{
}

/* unused */
void MEM_lockfree_callbackmemlist(void (*func)(void *))
{
	(void) func;  /* Ignored. */
}

void MEM_lockfree_printmemlist_stats(void)
{
	printf("\ntotal memory len: %.3f MB\n",
	       (double)mem_in_use / (double)(1024 * 1024));
	printf("peak memory len: %.3f MB\n",
	       (double)peak_mem / (double)(1024 * 1024));
	printf("\nFor more detailed per-block statistics run Blender with memory debugging command line argument.\n");

#ifdef HAVE_MALLOC_STATS
	printf("System Statistics:\n");
	malloc_stats();
#endif
}

void MEM_lockfree_set_error_callback(void (*func)(const char *))
{
	error_callback = func;
}

bool MEM_lockfree_check_memory_integrity(void)
{
	return true;
}

void MEM_lockfree_set_lock_callback(void (*lock)(void), void (*unlock)(void))
{
	thread_lock_callback = lock;
	thread_unlock_callback = unlock;
}

void MEM_lockfree_set_memory_debug(void)
{
	malloc_debug_memset = true;
}

uintptr_t MEM_lockfree_get_memory_in_use(void)
{
	return mem_in_use;
}

uintptr_t MEM_lockfree_get_mapped_memory_in_use(void)
{
	return mmap_in_use;
}

unsigned int MEM_lockfree_get_memory_blocks_in_use(void)
{
	return totblock;
}

/* dummy */
void MEM_lockfree_reset_peak_memory(void)
{
	peak_mem = 0;
}

uintptr_t MEM_lockfree_get_peak_memory(void)
{
	return peak_mem;
}

#ifndef NDEBUG
const char *MEM_lockfree_name_ptr(void *vmemh)
{
	if (vmemh) {
		return "unknown block name ptr";
	}
	else {
		return "MEM_lockfree_name_ptr(NULL)";
	}
}
#endif  /* NDEBUG */
