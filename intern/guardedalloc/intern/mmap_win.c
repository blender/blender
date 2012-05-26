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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Andrea Weikert.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file guardedalloc/intern/mmap_win.c
 *  \ingroup MEM
 */

#ifdef WIN32

#include <windows.h>
#include <errno.h>
#include <io.h>
#include <sys/types.h>
#include <stdio.h>

#include "mmap_win.h"

#ifndef FILE_MAP_EXECUTE
//not defined in earlier versions of the Platform  SDK (before February 2003)
#define FILE_MAP_EXECUTE 0x0020
#endif

/* copied from BKE_utildefines.h ugh */
#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) x
#endif

/* --------------------------------------------------------------------- */
/* local storage definitions                                             */
/* --------------------------------------------------------------------- */
/* all memory mapped chunks are put in linked lists */
typedef struct mmapLink {
	struct mmapLink *next, *prev;
} mmapLink;

typedef struct mmapListBase {
	void *first, *last;
} mmapListBase;

typedef struct MemMap {
	struct MemMap *next, *prev;
	void *mmap;
	HANDLE fhandle;
	HANDLE maphandle;
} MemMap;

/* --------------------------------------------------------------------- */
/* local functions                                                       */
/* --------------------------------------------------------------------- */

static void mmap_addtail(volatile mmapListBase *listbase, void *vlink);
static void mmap_remlink(volatile mmapListBase *listbase, void *vlink);
static void *mmap_findlink(volatile mmapListBase *listbase, void *ptr);

static int mmap_get_prot_flags(int flags);
static int mmap_get_access_flags(int flags);

/* --------------------------------------------------------------------- */
/* vars                                                                  */
/* --------------------------------------------------------------------- */
volatile static struct mmapListBase _mmapbase;
volatile static struct mmapListBase *mmapbase = &_mmapbase;


/* --------------------------------------------------------------------- */
/* implementation                                                        */
/* --------------------------------------------------------------------- */

/* mmap for windows */
void *mmap(void *UNUSED(start), size_t len, int prot, int flags, int fd, off_t offset)
{
	HANDLE fhandle = INVALID_HANDLE_VALUE;
	HANDLE maphandle;
	int prot_flags = mmap_get_prot_flags(prot);
	int access_flags = mmap_get_access_flags(prot);
	MemMap *mm = NULL;
	void *ptr = NULL;

	if (flags & MAP_FIXED) {
		return MAP_FAILED;
	}

#if 0
	if ( fd == -1 ) {
		_set_errno( EBADF );
		return MAP_FAILED;
	}
#endif

	if (fd != -1) {
		fhandle = (HANDLE) _get_osfhandle(fd);
	}
	if (fhandle == INVALID_HANDLE_VALUE) {
		if (!(flags & MAP_ANONYMOUS)) {
			errno = EBADF;
			return MAP_FAILED;
		}
	}
	else {
		if (!DuplicateHandle(GetCurrentProcess(), fhandle, GetCurrentProcess(),
		                     &fhandle, 0, FALSE, DUPLICATE_SAME_ACCESS) ) {
			return MAP_FAILED;
		}
	}

	maphandle = CreateFileMapping(fhandle, NULL, prot_flags, 0, len, NULL);
	if (maphandle == 0) {
		errno = EBADF;
		return MAP_FAILED;
	}

	ptr = MapViewOfFile(maphandle, access_flags, 0, offset, 0);
	if (ptr == NULL) {
		DWORD dwLastErr = GetLastError();
		if (dwLastErr == ERROR_MAPPED_ALIGNMENT)
			errno = EINVAL;
		else
			errno = EACCES;
		CloseHandle(maphandle);
		return MAP_FAILED;
	}

	mm = (MemMap *)malloc(sizeof(MemMap));
	if (!mm) {
		errno = ENOMEM;
	}
	mm->fhandle = fhandle;
	mm->maphandle = maphandle;
	mm->mmap = ptr;
	mmap_addtail(mmapbase, mm);

	return ptr;
}

/* munmap for windows */
intptr_t munmap(void *ptr, intptr_t UNUSED(size))
{
	MemMap *mm = mmap_findlink(mmapbase, ptr);
	if (!mm) {
		errno = EINVAL;
		return -1;
	}
	UnmapViewOfFile(mm->mmap);
	CloseHandle(mm->maphandle);
	CloseHandle(mm->fhandle);
	mmap_remlink(mmapbase, mm);
	free(mm);
	return 0;
}

/* --------------------------------------------------------------------- */
/* local functions                                                       */
/* --------------------------------------------------------------------- */

static void mmap_addtail(volatile mmapListBase *listbase, void *vlink)
{
	struct mmapLink *link = vlink;

	if (link == 0) return;
	if (listbase == 0) return;

	link->next = 0;
	link->prev = listbase->last;

	if (listbase->last) ((struct mmapLink *)listbase->last)->next = link;
	if (listbase->first == 0) listbase->first = link;
	listbase->last = link;
}

static void mmap_remlink(volatile mmapListBase *listbase, void *vlink)
{
	struct mmapLink *link = vlink;

	if (link == 0) return;
	if (listbase == 0) return;

	if (link->next) link->next->prev = link->prev;
	if (link->prev) link->prev->next = link->next;

	if (listbase->last == link) listbase->last = link->prev;
	if (listbase->first == link) listbase->first = link->next;
}

static void *mmap_findlink(volatile mmapListBase *listbase, void *ptr)
{
	MemMap *mm;

	if (ptr == 0) return NULL;
	if (listbase == 0) return NULL;
	
	mm = (MemMap *)listbase->first;
	while (mm) {
		if (mm->mmap == ptr) {
			return mm;
		}
		mm = mm->next;
	}
	return NULL;
}

static int mmap_get_prot_flags(int flags)
{
	int prot = PAGE_NOACCESS;

	if ( (flags & PROT_READ) == PROT_READ) {
		if ( (flags & PROT_WRITE) == PROT_WRITE) {
			prot = (flags & PROT_EXEC) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
		}
		else {
			prot = (flags & PROT_EXEC) ? PAGE_EXECUTE_READ : PAGE_READONLY;
		}
	}
	else if ( (flags & PROT_WRITE) == PROT_WRITE) {
		prot = (flags & PROT_EXEC) ? PAGE_EXECUTE_READ : PAGE_WRITECOPY;
	}
	else if ( (flags & PROT_EXEC) == PROT_EXEC) {
		prot = PAGE_EXECUTE_READ;
	}
	return prot;
}

static int mmap_get_access_flags(int flags)
{
	int access = 0;

	if ( (flags & PROT_READ) == PROT_READ) {
		if ( (flags & PROT_WRITE) == PROT_WRITE) {
			access = FILE_MAP_WRITE;
		}
		else {
			access = (flags & PROT_EXEC) ? FILE_MAP_EXECUTE : FILE_MAP_READ;
		}
	}
	else if ( (flags & PROT_WRITE) == PROT_WRITE) {
		access = FILE_MAP_COPY;
	}
	else if ( (flags & PROT_EXEC) == PROT_EXEC) {
		access = FILE_MAP_EXECUTE;
	}
	return access;
}

#endif // WIN32
