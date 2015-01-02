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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Windows-posix compatibility layer for opendir/readdir/closedir
 */

/** \file blender/blenlib/intern/winstuff_dir.c
 *  \ingroup bli
 *
 * Posix compatibility functions for windows dealing with DIR
 * (opendir, readdir, closedir)
 */

#ifdef WIN32

/* standalone for inclusion in binaries other then blender */
#  ifdef USE_STANDALONE
#    define MEM_mallocN(size, str) ((void)str, malloc(size))
#    define MEM_callocN(size, str) ((void)str, calloc(size, 1))
#    define MEM_freeN(ptr) free(ptr)
#  else
#    include "MEM_guardedalloc.h"
#  endif

#define WIN32_SKIP_HKEY_PROTECTION      // need to use HKEY
#include "BLI_winstuff.h"
#include "BLI_utildefines.h"
#include "utfconv.h"

/* Note: MinGW (FREE_WINDOWS) has opendir() and _wopendir(), and only the
 * latter accepts a path name of wchar_t type.  Rather than messing up with
 * extra #ifdef's here and there, Blender's own implementations of opendir()
 * and related functions are used to properly support paths with non-ASCII
 * characters. (kjym3)
 */

DIR *opendir(const char *path)
{
	wchar_t *path_16 = alloc_utf16_from_8(path, 0);

	if (GetFileAttributesW(path_16) & FILE_ATTRIBUTE_DIRECTORY) {
		DIR *newd = MEM_mallocN(sizeof(DIR), "opendir");

		newd->handle = INVALID_HANDLE_VALUE;
		sprintf(newd->path, "%s\\*", path);
		
		newd->direntry.d_ino = 0;
		newd->direntry.d_off = 0;
		newd->direntry.d_reclen = 0;
		newd->direntry.d_name = NULL;
		
		free(path_16);
		return newd;
	}
	else {
		free(path_16);
		return NULL;
	}
}

static char *BLI_alloc_utf_8_from_16(wchar_t *in16, size_t add)
{
	size_t bsize = count_utf_8_from_16(in16);
	char *out8 = NULL;
	if (!bsize) return NULL;
	out8 = (char *)MEM_mallocN(sizeof(char) * (bsize + add), "UTF-8 String");
	conv_utf_16_to_8(in16, out8, bsize);
	return out8;
}

static wchar_t *UNUSED_FUNCTION(BLI_alloc_utf16_from_8) (char *in8, size_t add)
{
	size_t bsize = count_utf_16_from_8(in8);
	wchar_t *out16 = NULL;
	if (!bsize) return NULL;
	out16 = (wchar_t *) MEM_mallocN(sizeof(wchar_t) * (bsize + add), "UTF-16 String");
	conv_utf_8_to_16(in8, out16, bsize);
	return out16;
}



struct dirent *readdir(DIR *dp)
{
	if (dp->direntry.d_name) {
		MEM_freeN(dp->direntry.d_name);
		dp->direntry.d_name = NULL;
	}
		
	if (dp->handle == INVALID_HANDLE_VALUE) {
		wchar_t *path_16 = alloc_utf16_from_8(dp->path, 0);
		dp->handle = FindFirstFileW(path_16, &(dp->data));
		free(path_16);
		if (dp->handle == INVALID_HANDLE_VALUE)
			return NULL;
			
		dp->direntry.d_name = BLI_alloc_utf_8_from_16(dp->data.cFileName, 0);
		
		return &dp->direntry;
	}
	else if (FindNextFileW(dp->handle, &(dp->data))) {
		dp->direntry.d_name = BLI_alloc_utf_8_from_16(dp->data.cFileName, 0);

		return &dp->direntry;
	}
	else {
		return NULL;
	}
}

int closedir(DIR *dp)
{
	if (dp->direntry.d_name) MEM_freeN(dp->direntry.d_name);
	if (dp->handle != INVALID_HANDLE_VALUE) FindClose(dp->handle);

	MEM_freeN(dp);
	
	return 0;
}

/* End of copied part */

#else

/* intentionally empty for UNIX */

#endif
