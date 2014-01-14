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
 
#ifndef __BLI_WINSTUFF_H__
#define __BLI_WINSTUFF_H__

/** \file BLI_winstuff.h
 *  \ingroup bli
 *  \brief Compatibility-like things for windows.
 */

#ifndef _WIN32
#  error "This include is for Windows only!"
#endif

#ifndef FREE_WINDOWS
#  pragma warning(once: 4761 4305 4244 4018)
#else
#  ifdef WINVER
#    undef WINVER
#  endif

/* Some stuff requires WINVER 0x500, but mingw's default is 0x400 */
#  define WINVER 0x0501
#endif

#define WIN32_LEAN_AND_MEAN

#ifndef WIN32_SKIP_HKEY_PROTECTION
#  undef HKEY
#  define HKEY WIN32_HKEY  /* prevent competing definitions */
#  include <windows.h>
#  undef HKEY
#else
#  include <windows.h>
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

// These definitions are also in BLI_math for simplicity

#ifdef __cplusplus
extern "C" {
#endif

#define _USE_MATH_DEFINES
#define MAXPATHLEN MAX_PATH

#ifndef S_ISREG
#  define S_ISREG(x) (((x) & _S_IFREG) == _S_IFREG)
#endif
#ifndef S_ISDIR
#  define S_ISDIR(x) (((x) & _S_IFDIR) == _S_IFDIR)
#endif

/* defines for using ISO C++ conformant names */
#define snprintf _snprintf

#if defined(_MSC_VER) || (defined(FREE_WINDOWS) && !defined(FREE_WINDOWS64))
#  define	R_OK	4
#  define	W_OK	2
// not accepted by access() on windows
//#  define	X_OK	1
#  define	F_OK	0
#endif

#ifndef FREE_WINDOWS
typedef unsigned int mode_t;
#endif

/* use functions that take a 64 bit offset for files larger than 4GB */
#ifndef FREE_WINDOWS
#  include <stdio.h>
#  define fseek(stream, offset, origin) _fseeki64(stream, offset, origin)
#  define ftell(stream) _ftelli64(stream)
#  define lseek(fd, offset, origin) _lseeki64(fd, offset, origin)
#  define tell(fd) _telli64(fd)
#endif

/* mingw using _SSIZE_T_ to declare ssize_t type */
#ifndef _SSIZE_T_
#  define _SSIZE_T_
/* python uses HAVE_SSIZE_T */
#  ifndef HAVE_SSIZE_T
#    define HAVE_SSIZE_T 1
#    ifndef FREE_WINDOWS64
typedef long ssize_t;
#    endif
#  endif
#endif


#ifdef FREE_WINDOWS
#include <dirent.h>
#else
struct dirent {
	int d_ino;
	int d_off;
	unsigned short d_reclen;
	char *d_name;
};

typedef struct _DIR {
	HANDLE handle;
	WIN32_FIND_DATAW data;
	char path[MAX_PATH];
	long dd_loc;
	long dd_size;
	char dd_buf[4096];
	void *dd_direct;
	
	struct dirent direntry;
} DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dp);
int closedir(DIR *dp);
#endif

void RegisterBlendExtension(void);
void get_default_root(char *root);
int check_file_chars(char *filename);
const char *dirname(char *path);

int BLI_getInstallationDir(char *str);

#ifdef __cplusplus
}
#endif

#endif /* __BLI_WINSTUFF_H__ */

