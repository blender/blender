/* 
 * $Id$
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
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
*/

/* Note, the BGE needs to use this too, keep it minimal */

#ifndef EXPP_bpy_import_h
#define EXPP_bpy_import_h

/* python redefines :/ */
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif

#include <Python.h>
#include "compile.h"		/* for the PyCodeObject */
#include "eval.h"		/* for PyEval_EvalCode */

PyObject*	bpy_text_import( char *name, int *found );
PyObject*	bpy_text_reimport( PyObject *module, int *found );
/* void		bpy_text_clear_modules( int clear_all );*/ /* Clear user modules */ 
extern PyMethodDef bpy_import_meth[];
extern PyMethodDef bpy_reload_meth[];

/* The game engine has its own Main struct, if this is set search this rather then G.main */
struct Main *bpy_import_main_get(void);
void bpy_import_main_set(struct Main *maggie);


#endif				/* EXPP_bpy_import_h */
