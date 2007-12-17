/* 
 * $Id: World.h 10269 2007-03-15 01:09:14Z campbellbarton $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * Contributor(s): Jacques Guignot
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_WORLD_H
#define EXPP_WORLD_H

#include <Python.h>
#include "DNA_world_types.h"

#define BPy_World_Check(v) ((v)->ob_type==&World_Type)

/* Python BPy_World structure definition */
typedef struct {
	PyObject_HEAD		/* required py macro */
	World * world;		/* Libdata must be second */
} BPy_World;

extern PyTypeObject World_Type;

/*****************************************************************************/
/* Python World_Type helper functions needed by Blender (the Init function) */
/* and Object modules.                                                       */
/*****************************************************************************/

PyObject *World_Init( void );
PyObject *World_CreatePyObject( World * world );
World *World_FromPyObject( PyObject * pyobj );

#endif				/* EXPP_WORLD_H */
