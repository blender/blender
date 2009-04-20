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
 * Contributor(s): Jacques Guignot, Cedric Paille
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#ifndef EXPP_PARTICLESYS_H
#define EXPP_PARTICLESYS_H

#include <Python.h>
#include "DNA_particle_types.h"
#include "DNA_object_types.h"

extern PyTypeObject ParticleSys_Type;

#define BPy_ParticleSys_Check(v) \
    ((v)->ob_type == &ParticleSys_Type)	/* for type checking */

/* Python BPy_Effect structure definition */
typedef struct {
	PyObject_HEAD		/* required py macro */
	ParticleSystem *psys;
	Object *object;  /* fixeme: if this points back to the parent object,it is wrong */
} BPy_PartSys;

PyObject *ParticleSys_Init( void );
PyObject *ParticleSys_CreatePyObject( ParticleSystem * psystem, Object *ob );


#endif				/* EXPP_EFFECT_H */
