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

#ifndef EXPP_WAVE_H
#define EXPP_WAVE_H

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
/* Python API function prototypes for the Wave module.                       */
/*****************************************************************************/
PyObject *M_Wave_New (PyObject *self, PyObject *args);
PyObject *M_Wave_Get (PyObject *self, PyObject *args);


/*****************************************************************************/
/* Python C_Wave structure definition:                                       */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  Effect   *wave;
} C_Wave;

#include"Effect.h"
/*****************************************************************************/
/* Python C_Wave methods declarations:                                       */
/*****************************************************************************/
PyObject *Effect_getType(C_Effect *self);
PyObject *Effect_setType(C_Effect *self, PyObject *args);
PyObject *Effect_getFlag(C_Effect *self);
PyObject *Effect_setFlag(C_Effect *self, PyObject *args);
PyObject *Wave_getStartx(C_Wave *self);
PyObject *Wave_setStartx(C_Wave *self,PyObject*a);
PyObject *Wave_getStarty(C_Wave *self);
PyObject *Wave_setStarty(C_Wave *self,PyObject*a);
PyObject *Wave_getHeight(C_Wave *self);
PyObject *Wave_setHeight(C_Wave *self,PyObject*a);
PyObject *Wave_getWidth(C_Wave *self);
PyObject *Wave_setWidth(C_Wave *self,PyObject*a);
PyObject *Wave_getNarrow(C_Wave *self);
PyObject *Wave_setNarrow(C_Wave *self,PyObject*a);
PyObject *Wave_getSpeed(C_Wave *self);
PyObject *Wave_setSpeed(C_Wave *self,PyObject*a);
PyObject *Wave_getMinfac(C_Wave *self);
PyObject *Wave_setMinfac(C_Wave *self,PyObject*a);
PyObject *Wave_getDamp(C_Wave *self);
PyObject *Wave_setDamp(C_Wave *self,PyObject*a);
PyObject *Wave_getTimeoffs(C_Wave *self);
PyObject *Wave_setTimeoffs(C_Wave *self,PyObject*a);
PyObject *Wave_getLifetime(C_Wave *self);
PyObject *Wave_setLifetime(C_Wave *self,PyObject*a);

/*****************************************************************************/
/* Python Wave_Type callback function prototypes:                            */
/*****************************************************************************/
void WaveDeAlloc (C_Wave *msh);
int WavePrint (C_Wave *msh, FILE *fp, int flags);
int WaveSetAttr (C_Wave *msh, char *name, PyObject *v);
PyObject *WaveGetAttr (C_Wave *msh, char *name);
PyObject *WaveRepr (C_Wave *msh);
PyObject* WaveCreatePyObject (struct Effect *wave);
int WaveCheckPyObject (PyObject *py_obj);
struct Wave* WaveFromPyObject (PyObject *py_obj);


#endif /* EXPP_WAVE_H */
