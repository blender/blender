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

#ifndef EXPP_PARTICLE_H
#define EXPP_PARTICLE_H

#include <Python.h>
#include <stdio.h>

#include <BLI_arithb.h>
#include <BLI_blenlib.h>
#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <DNA_effect_types.h>

#include "gen_utils.h"
#include "bpy_types.h"

/*****************************************************************************/
/* Python API function prototypes for the Particle module.                   */
/*****************************************************************************/
PyObject *M_Particle_New (PyObject *self, PyObject *args);
PyObject *M_Particle_Get (PyObject *self, PyObject *args);



#include"Effect.h"
/*****************************************************************************/
/* Python BPy_Particle methods declarations:                                 */
/*****************************************************************************/
PyObject *Effect_getType(BPy_Effect *self);
PyObject *Effect_setType(BPy_Effect *self, PyObject *args);
PyObject *Effect_getFlag(BPy_Effect *self);
PyObject *Effect_setFlag(BPy_Effect *self, PyObject *args);
PyObject *Particle_getSta(BPy_Particle *self);
PyObject *Particle_setSta(BPy_Particle *self,PyObject*a);
PyObject *Particle_getEnd(BPy_Particle *self);
PyObject *Particle_setEnd(BPy_Particle *self,PyObject*a);
PyObject *Particle_getLifetime(BPy_Particle *self);
PyObject *Particle_setLifetime(BPy_Particle *self,PyObject*a);
PyObject *Particle_getNormfac(BPy_Particle *self);
PyObject *Particle_setNormfac(BPy_Particle *self,PyObject*a);
PyObject *Particle_getObfac(BPy_Particle *self);
PyObject *Particle_setObfac(BPy_Particle *self,PyObject*a);
PyObject *Particle_getRandfac(BPy_Particle *self);
PyObject *Particle_setRandfac(BPy_Particle *self,PyObject*a);
PyObject *Particle_getTexfac(BPy_Particle *self);
PyObject *Particle_setTexfac(BPy_Particle *self,PyObject*a);
PyObject *Particle_getRandlife(BPy_Particle *self);
PyObject *Particle_setRandlife(BPy_Particle *self,PyObject*a);
PyObject *Particle_getNabla(BPy_Particle *self);
PyObject *Particle_setNabla(BPy_Particle *self,PyObject*a);
PyObject *Particle_getVectsize(BPy_Particle *self);
PyObject *Particle_setVectsize(BPy_Particle *self,PyObject*a);
PyObject *Particle_getTotpart(BPy_Particle *self);
PyObject *Particle_setTotpart(BPy_Particle *self,PyObject*a);
PyObject *Particle_getTotkey(BPy_Particle *self);
PyObject *Particle_setTotkey(BPy_Particle *self,PyObject*a);
PyObject *Particle_getSeed(BPy_Particle *self);
PyObject *Particle_setSeed(BPy_Particle *self,PyObject*a);
PyObject *Particle_getForce(BPy_Particle *self);
PyObject *Particle_setForce(BPy_Particle *self,PyObject*a);
PyObject *Particle_getMult(BPy_Particle *self);
PyObject *Particle_setMult(BPy_Particle *self,PyObject*a);
PyObject *Particle_getLife(BPy_Particle *self);
PyObject *Particle_setLife(BPy_Particle *self,PyObject*a);
PyObject *Particle_getMat(BPy_Particle *self);
PyObject *Particle_setMat(BPy_Particle *self,PyObject*a);
PyObject *Particle_getChild(BPy_Particle *self);
PyObject *Particle_setChild(BPy_Particle *self,PyObject*a);
PyObject *Particle_getDefvec(BPy_Particle *self);
PyObject *Particle_setDefvec(BPy_Particle *self,PyObject*a);



/*****************************************************************************/
/* Python Particle_Type callback function prototypes:                        */
/*****************************************************************************/
void ParticleDeAlloc (BPy_Particle *msh);
int ParticlePrint (BPy_Particle *msh, FILE *fp, int flags);
int ParticleSetAttr (BPy_Particle *msh, char *name, PyObject *v);
PyObject *ParticleGetAttr (BPy_Particle *msh, char *name);
PyObject *ParticleRepr (BPy_Particle *msh);
PyObject* ParticleCreatePyObject (struct Effect *particle);
int ParticleCheckPyObject (PyObject *py_obj);
struct Particle* ParticleFromPyObject (PyObject *py_obj);



#endif /* EXPP_PARTICLE_H */
