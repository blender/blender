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

/** \file blender/blenlib/BLI_dynlib.h
 *  \ingroup bli
 */

#ifndef __BLI_DYNLIB_H__
#define __BLI_DYNLIB_H__

typedef struct DynamicLibrary DynamicLibrary;

DynamicLibrary *BLI_dynlib_open(char *name);
void *BLI_dynlib_find_symbol(DynamicLibrary *lib, const char *symname);
char *BLI_dynlib_get_error_as_string(DynamicLibrary *lib);
void BLI_dynlib_close(DynamicLibrary *lib);

#endif /* __BLI_DYNLIB_H__ */

