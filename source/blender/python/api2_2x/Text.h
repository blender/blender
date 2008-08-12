/* 
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
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#ifndef EXPP_TEXT_H
#define EXPP_TEXT_H

#include <Python.h>
#include "DNA_text_types.h"

extern PyTypeObject Text_Type;

/* Type checking for EXPP PyTypes */
#define BPy_Text_Check(v)       ((v)->ob_type == &Text_Type)

typedef struct {
	PyObject_HEAD
	Text * text; /* libdata must be second */
	TextLine * iol; /* current line being read or NULL if reset */
	int ioc; /* character offset in line being read */
} BPy_Text;

PyObject *Text_Init( void );
PyObject *Text_CreatePyObject( Text * txt );

#endif				/* EXPP_TEXT_H */
