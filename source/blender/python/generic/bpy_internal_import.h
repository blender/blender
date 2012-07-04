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
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/generic/bpy_internal_import.h
 *  \ingroup pygen
 */


/* Note, the BGE needs to use this too, keep it minimal */

#ifndef __BPY_INTERNAL_IMPORT_H__
#define __BPY_INTERNAL_IMPORT_H__

/* python redefines :/ */
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif

struct Text;

void bpy_import_init(PyObject *builtins);

PyObject*	bpy_text_import(struct Text *text);
PyObject*	bpy_text_import_name(const char *name, int *found);
PyObject*	bpy_text_reimport(PyObject *module, int *found);
/* void		bpy_text_clear_modules(int clear_all);*/ /* Clear user modules */ 

void bpy_text_filename_get(char *fn, size_t fn_len, struct Text *text);

extern PyMethodDef bpy_import_meth;
extern PyMethodDef bpy_reload_meth;

/* The game engine has its own Main struct, if this is set search this rather than G.main */
struct Main *bpy_import_main_get(void);
void bpy_import_main_set(struct Main *maggie);

/* This is used for importing text from dynamically loaded libraries in the game engine */
void bpy_import_main_extra_add(struct Main *maggie);
void bpy_import_main_extra_remove(struct Main *maggie);

#endif				/* __BPY_INTERNAL_IMPORT_H__ */
