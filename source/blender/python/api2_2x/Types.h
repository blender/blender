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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_TYPES_H
#define EXPP_TYPES_H

#include "Python.h"

extern PyTypeObject Button_Type, Material_Type;

extern PyTypeObject Object_Type;
extern PyTypeObject NMesh_Type, NMFace_Type, NMVert_Type, NMCol_Type;
extern PyTypeObject Camera_Type, Lamp_Type, Image_Type, Text_Type;

extern PyTypeObject vector_Type, buffer_Type, rgbTuple_Type,
				            constant_Type;

static char M_Types_doc[] =
"The Blender Types module\n\n\
This module is a dictionary of all Blender Python types";

#endif /* EXPP_TYPES_H */
