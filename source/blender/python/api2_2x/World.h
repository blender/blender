/* 
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

#include <DNA_world_types.h>

#include "constant.h"
#include "gen_utils.h"

/* The World PyType Object defined in World.c */
extern PyTypeObject World_Type;


/*****************************************************************************/
/* Python C_World structure definition:                                   */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  World *world;

} C_World;

/*****************************************************************************/
/* Python World_Type helper functions needed by Blender (the Init function) */
/* and Object modules.                                                       */
/*****************************************************************************/
PyObject *M_World_Init (void);
PyObject *World_CreatePyObject (World *cam);
World   *World_FromPyObject (PyObject *pyobj);
int       World_CheckPyObject (PyObject *pyobj);


#endif /* EXPP_WORLD_H */
