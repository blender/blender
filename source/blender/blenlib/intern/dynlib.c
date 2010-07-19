/**
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

#include <stdlib.h>

#include "../PIL_dynlib.h"

#if !defined(CHAR_MAX)
#define CHAR_MAX 255
#endif

/*
 * XXX, should use mallocN so we can see
 * handle's not being released. fixme zr
 */
 
#ifdef WIN32
#include <string.h>
#include <stdio.h>

#include <windows.h>

struct PILdynlib {
	void *handle;
};

PILdynlib *PIL_dynlib_open(char *name) {
	void *handle= LoadLibrary(name);

	if (handle) {	
		PILdynlib *lib= malloc(sizeof(*lib));
		lib->handle= handle;
		
		return lib;
	} else {
		return NULL;
	}
}

void *PIL_dynlib_find_symbol(PILdynlib* lib, char *symname) {
	return GetProcAddress(lib->handle, symname);
}

char *PIL_dynlib_get_error_as_string(PILdynlib* lib) {
	int err;

	/* if lib is NULL reset the last error code */
	err= GetLastError();
	if (!lib) SetLastError(ERROR_SUCCESS);

	if (err) {
		static char buf[1024];

		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, 
					NULL, 
					err, 
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
					buf, 
					sizeof(buf), 
					NULL))
			return buf;
	}
	
	return err;
}

void PIL_dynlib_close(PILdynlib *lib) {
	FreeLibrary(lib->handle);
	
	free(lib);
}

#else	/* Unix */

#include <dlfcn.h>

struct PILdynlib {
	void *handle;
};

PILdynlib *PIL_dynlib_open(char *name) {
	void *handle= dlopen(name, RTLD_LAZY);

	if (handle) {	
		PILdynlib *lib= malloc(sizeof(*lib));
		lib->handle= handle;
		
		return lib;
	} else {
		return NULL;
	}
}

void *PIL_dynlib_find_symbol(PILdynlib* lib, char *symname) {
	return dlsym(lib->handle, symname);
}

char *PIL_dynlib_get_error_as_string(PILdynlib* lib) {
	return dlerror();
}
	
void PIL_dynlib_close(PILdynlib *lib) {
	dlclose(lib->handle);
	
	free(lib);
}

#endif

