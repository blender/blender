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
 * Contributor(s): Willian P. Germano, Alex Mole
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_TYPES_H
#define EXPP_TYPES_H

#include "Python.h"

extern PyTypeObject Action_Type, Armature_Type;
extern PyTypeObject BezTriple_Type, Bone_Type, Build_Type, Button_Type;
extern PyTypeObject Camera_Type, Curve_Type;
extern PyTypeObject Effect_Type;
extern PyTypeObject Image_Type, Ipo_Type, IpoCurve_Type;
extern PyTypeObject Lamp_Type, Lattice_Type;
extern PyTypeObject Material_Type, Metaball_Type, MTex_Type;
extern PyTypeObject NMFace_Type, NMVert_Type, NMCol_Type, NMesh_Type;
extern PyTypeObject Object_Type;
extern PyTypeObject Particle_Type;
extern PyTypeObject Scene_Type, RenderData_Type;
extern PyTypeObject Text_Type, Texture_Type;
extern PyTypeObject Wave_Type, World_Type;

extern PyTypeObject buffer_Type, constant_Type, euler_Type;
extern PyTypeObject matrix_Type, quaternion_Type, rgbTuple_Type, vector_Type;

#endif /* EXPP_TYPES_H */
