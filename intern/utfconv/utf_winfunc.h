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

#ifndef __UTF_WINFUNC_H__
#define __UTF_WINFUNC_H__

#ifndef WIN32
#  error "This file can only compile on windows"
#endif

#include <stdio.h>

FILE *ufopen(const char * filename, const char * mode);
int uopen(const char *filename, int oflag, int pmode);
int uaccess(const char *filename, int mode);
int urename(const char *oldname, const char *newname);

char *u_alloc_getenv(const char *varname);
void  u_free_getenv(char *val);

int uput_getenv(const char *varname, char *value, size_t buffsize);
int uputenv(const char *name, const char *value);

int umkdir(const char *pathname);

#endif  /* __UTF_WINFUNC_H__ */
