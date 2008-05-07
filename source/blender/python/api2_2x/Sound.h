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
 * Contributor(s): Chris Keith
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#ifndef EXPP_SOUND_H
#define EXPP_SOUND_H

#include <Python.h>
#include "DNA_sound_types.h"

#define BPy_Sound_Check(v)       ((v)->ob_type == &Sound_Type)
extern PyTypeObject Sound_Type;

/*****************************************************************************/
/* Python BPy_Sound structure definition                                     */
/*****************************************************************************/
typedef struct {
	PyObject_HEAD 
	bSound * sound;
} BPy_Sound;

/*****************************************************************************/
/* Module Blender.Sound - public functions                                   */
/*****************************************************************************/
PyObject *Sound_Init( void );
PyObject *Sound_CreatePyObject( bSound * sound );
bSound *Sound_FromPyObject( PyObject * pyobj );

#endif				/* EXPP_SOUND_H */
