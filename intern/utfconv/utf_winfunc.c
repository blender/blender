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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Alexandr Kuznetsov, Andrea Weikert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef _WIN32_IE
#define _WIN32_IE 0x0501
#endif

#include "utf_winfunc.h"
#include <io.h>
#include <Windows.h>
#include <wchar.h>


FILE * ufopen(const char * filename, const char * mode)
{
	FILE *f = NULL;
	UTF16_ENCODE(filename);
	UTF16_ENCODE (mode);

	if(filename_16 && mode_16) {
		f = _wfopen(filename_16, mode_16);
	}
	
	UTF16_UN_ENCODE(mode);
	UTF16_UN_ENCODE(filename);

	if (!f) {
		if ((f = fopen(filename, mode))) {
			printf("WARNING: %s is not utf path. Please update it.\n",filename);
		}
	}

	return f;
}

int uopen(const char *filename, int oflag, int pmode)
{
	int f = -1;
	UTF16_ENCODE(filename);
	
	if (filename_16) {
		f = _wopen(filename_16, oflag, pmode);
	}

	UTF16_UN_ENCODE(filename);

	if (f == -1) {
		if ((f=open(filename,oflag, pmode)) != -1) {
			printf("WARNING: %s is not utf path. Please update it.\n",filename);
		}
	}

	return f;
}

int urename(const char *oldname, const char *newname )
{
	int r = -1;
	UTF16_ENCODE(oldname);
	UTF16_ENCODE (newname);

	if(oldname_16 && newname_16) r = _wrename(oldname_16, newname_16);
	
	UTF16_UN_ENCODE(newname);
	UTF16_UN_ENCODE(oldname);
	return r;
}

int umkdir(const char *pathname)
{

	BOOL r = 0;
	UTF16_ENCODE(pathname);
	
	if(pathname_16) r = CreateDirectoryW(pathname_16, NULL);

	UTF16_UN_ENCODE(pathname);

	return r ? 0 : -1;
}

char * u_alloc_getenv(const char *varname)
{
	char * r = 0;
	wchar_t * str;
	UTF16_ENCODE(varname);
	if (varname_16) {
		str = _wgetenv(varname_16);
		r = alloc_utf_8_from_16(str, 0);
	}
	UTF16_UN_ENCODE(varname);

	return r;
}
void  u_free_getenv(char *val)
{
	free(val);
}

int uput_getenv(const char *varname, char * value, size_t buffsize)
{
	int r = 0;
	wchar_t * str;
	if(!buffsize) return r;

	UTF16_ENCODE(varname);
	if(varname_16) {
		str = _wgetenv(varname_16);
		conv_utf_16_to_8(str, value, buffsize);
		r = 1;
	}
	UTF16_UN_ENCODE(varname);

	if (!r) value[0] = 0;

	return r;
}

int uputenv(const char *name, const char *value)
{
	int r = -1;
	UTF16_ENCODE(name);
	UTF16_ENCODE(value);
	if(name_16 && value_16) {
		r = (SetEnvironmentVariableW(name_16,value_16)!= 0) ? 0 : -1;
	}
	UTF16_UN_ENCODE(value);
	UTF16_UN_ENCODE(name);

	return r;
}
