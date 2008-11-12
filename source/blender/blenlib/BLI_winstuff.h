/**
 * Compatibility-like things for windows.
 *
 * $Id$ 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#pragma warning(once: 4761 4305 4244 4018)

#define WIN32_LEAN_AND_MEAN

#ifndef WIN32_SKIP_HKEY_PROTECTION
#define HKEY WIN32_HKEY				// prevent competing definitions
#include <windows.h>
#undef HKEY
#else
#include <windows.h>
#endif

#undef near
#undef far
#undef rad
#undef rad1
#undef rad2
#undef rad3
#undef vec
#undef rect
#undef rct1
#undef rct2

#define near clipsta
#define far clipend

#undef small

#ifndef __WINSTUFF_H__
#define __WINSTUFF_H__

	// These definitions are also in arithb for simplicity

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2		1.57079632679489661923
#endif
#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2	0.70710678118654752440
#endif
#ifndef M_1_PI
#define M_1_PI		0.318309886183790671538
#endif

#define MAXPATHLEN MAX_PATH

#ifndef S_ISREG
#define S_ISREG(x) ((x&S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(x) ((x&S_IFMT) == S_IFDIR)
#endif

#ifndef FREE_WINDOWS
typedef unsigned int mode_t;
#endif

struct dirent {
	int d_ino;
	int d_off;
	unsigned short d_reclen;
	char *d_name;
};

typedef struct _DIR {
	HANDLE handle;
	WIN32_FIND_DATA data;
	char path[MAX_PATH];
	long dd_loc;
	long dd_size;
	char dd_buf[4096];
	void *dd_direct;
	
	struct dirent direntry;
} DIR;

void RegisterBlendExtension(char * str);
DIR *opendir (const char *path);
struct dirent *readdir(DIR *dp);
int closedir (DIR *dp);
void get_default_root(char *root);
int check_file_chars(char *filename);

#ifdef __cplusplus
}
#endif

#endif /* __WINSTUFF_H__ */

