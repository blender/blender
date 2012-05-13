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
 * The Original Code is: all of this file, with exception of below:
 *
 * Contributor(s): Peter O'Gorman
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/dynlib.c
 *  \ingroup bli
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_dynlib.h"

struct DynamicLibrary {
	void *handle;
};

#ifdef WIN32

#include <windows.h>
#include "utf_winfunc.h"
#include "utfconv.h"

DynamicLibrary *BLI_dynlib_open(char *name)
{
	DynamicLibrary *lib;
	void *handle;

	UTF16_ENCODE(name);
	handle = LoadLibraryW(name_16);
	UTF16_UN_ENCODE(name);

	if (!handle)
		return NULL;

	lib = MEM_callocN(sizeof(*lib), "Dynamic Library");
	lib->handle = handle;
		
	return lib;
}

void *BLI_dynlib_find_symbol(DynamicLibrary *lib, const char *symname)
{
	return GetProcAddress(lib->handle, symname);
}

char *BLI_dynlib_get_error_as_string(DynamicLibrary *lib)
{
	int err;

	/* if lib is NULL reset the last error code */
	err = GetLastError();
	if (!lib)
		SetLastError(ERROR_SUCCESS);

	if (err) {
		static char buf[1024];

		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		                  NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		                  buf, sizeof(buf), NULL))
		{
			return buf;
		}
	}
	
	return NULL;
}

void BLI_dynlib_close(DynamicLibrary *lib)
{
	FreeLibrary(lib->handle);
	MEM_freeN(lib);
}

#else /* Unix */

#include <dlfcn.h>

DynamicLibrary *BLI_dynlib_open(char *name)
{
	DynamicLibrary *lib;
	void *handle = dlopen(name, RTLD_LAZY);

	if (!handle)
		return NULL;

	lib = MEM_callocN(sizeof(*lib), "Dynamic Library");
	lib->handle = handle;
		
	return lib;
}

void *BLI_dynlib_find_symbol(DynamicLibrary *lib, const char *symname)
{
	return dlsym(lib->handle, symname);
}

char *BLI_dynlib_get_error_as_string(DynamicLibrary *lib)
{
	(void)lib; /* unused */
	return dlerror();
}
	
void BLI_dynlib_close(DynamicLibrary *lib)
{
	dlclose(lib->handle);
	MEM_freeN(lib);
}

#endif

