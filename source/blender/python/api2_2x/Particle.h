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

/*****************************************************************************/
/* Python API function prototypes for the Particle module.                   */
/*****************************************************************************/
PyObject *M_Particle_New (PyObject *self, PyObject *args);
PyObject *M_Particle_Get (PyObject *self, PyObject *args);


/*****************************************************************************/
/* Python C_Particle structure definition:                                   */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  Effect   *particle;
} C_Particle;

#include"Effect.h"
/*****************************************************************************/
/* Python C_Particle methods declarations:                                   */
/*****************************************************************************/
PyObject *Effect_getType(C_Effect *self);
PyObject *Effect_setType(C_Effect *self, PyObject *args);
PyObject *Effect_getFlag(C_Effect *self);
PyObject *Effect_setFlag(C_Effect *self, PyObject *args);
PyObject *Particle_getSta(C_Particle *self);
PyObject *Particle_setSta(C_Particle *self,PyObject*a);
PyObject *Particle_getEnd(C_Particle *self);
PyObject *Particle_setEnd(C_Particle *self,PyObject*a);
PyObject *Particle_getLifetime(C_Particle *self);
PyObject *Particle_setLifetime(C_Particle *self,PyObject*a);
PyObject *Particle_getNormfac(C_Particle *self);
PyObject *Particle_setNormfac(C_Particle *self,PyObject*a);
PyObject *Particle_getObfac(C_Particle *self);
PyObject *Particle_setObfac(C_Particle *self,PyObject*a);
PyObject *Particle_getRandfac(C_Particle *self);
PyObject *Particle_setRandfac(C_Particle *self,PyObject*a);
PyObject *Particle_getTexfac(C_Particle *self);
PyObject *Particle_setTexfac(C_Particle *self,PyObject*a);
PyObject *Particle_getRandlife(C_Particle *self);
PyObject *Particle_setRandlife(C_Particle *self,PyObject*a);
PyObject *Particle_getNabla(C_Particle *self);
PyObject *Particle_setNabla(C_Particle *self,PyObject*a);
PyObject *Particle_getVectsize(C_Particle *self);
PyObject *Particle_setVectsize(C_Particle *self,PyObject*a);
PyObject *Particle_getTotpart(C_Particle *self);
PyObject *Particle_setTotpart(C_Particle *self,PyObject*a);
PyObject *Particle_getTotkey(C_Particle *self);
PyObject *Particle_setTotkey(C_Particle *self,PyObject*a);
PyObject *Particle_getSeed(C_Particle *self);
PyObject *Particle_setSeed(C_Particle *self,PyObject*a);
PyObject *Particle_getForce(C_Particle *self);
PyObject *Particle_setForce(C_Particle *self,PyObject*a);
PyObject *Particle_getMult(C_Particle *self);
PyObject *Particle_setMult(C_Particle *self,PyObject*a);
PyObject *Particle_getLife(C_Particle *self);
PyObject *Particle_setLife(C_Particle *self,PyObject*a);
PyObject *Particle_getMat(C_Particle *self);
PyObject *Particle_setMat(C_Particle *self,PyObject*a);
PyObject *Particle_getChild(C_Particle *self);
PyObject *Particle_setChild(C_Particle *self,PyObject*a);
PyObject *Particle_getDefvec(C_Particle *self);
PyObject *Particle_setDefvec(C_Particle *self,PyObject*a);



/*****************************************************************************/
/* Python Particle_Type callback function prototypes:                        */
/*****************************************************************************/
void ParticleDeAlloc (C_Particle *msh);
int ParticlePrint (C_Particle *msh, FILE *fp, int flags);
int ParticleSetAttr (C_Particle *msh, char *name, PyObject *v);
PyObject *ParticleGetAttr (C_Particle *msh, char *name);
PyObject *ParticleRepr (C_Particle *msh);
PyObject* ParticleCreatePyObject (struct Effect *particle);
int ParticleCheckPyObject (PyObject *py_obj);
struct Particle* ParticleFromPyObject (PyObject *py_obj);



#endif /* EXPP_PARTICLE_H */
