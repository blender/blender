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


#include"Effect.h"
/*****************************************************************************/
/* Python BPy_Wave methods declarations:                                       */
/*****************************************************************************/
PyObject *Effect_getType(BPy_Effect *self);
PyObject *Effect_setType(BPy_Effect *self, PyObject *args);
PyObject *Effect_getFlag(BPy_Effect *self);
PyObject *Effect_setFlag(BPy_Effect *self, PyObject *args);
PyObject *Wave_getStartx(BPy_Wave *self);
PyObject *Wave_setStartx(BPy_Wave *self,PyObject*a);
PyObject *Wave_getStarty(BPy_Wave *self);
PyObject *Wave_setStarty(BPy_Wave *self,PyObject*a);
PyObject *Wave_getHeight(BPy_Wave *self);
PyObject *Wave_setHeight(BPy_Wave *self,PyObject*a);
PyObject *Wave_getWidth(BPy_Wave *self);
PyObject *Wave_setWidth(BPy_Wave *self,PyObject*a);
PyObject *Wave_getNarrow(BPy_Wave *self);
PyObject *Wave_setNarrow(BPy_Wave *self,PyObject*a);
PyObject *Wave_getSpeed(BPy_Wave *self);
PyObject *Wave_setSpeed(BPy_Wave *self,PyObject*a);
PyObject *Wave_getMinfac(BPy_Wave *self);
PyObject *Wave_setMinfac(BPy_Wave *self,PyObject*a);
PyObject *Wave_getDamp(BPy_Wave *self);
PyObject *Wave_setDamp(BPy_Wave *self,PyObject*a);
PyObject *Wave_getTimeoffs(BPy_Wave *self);
PyObject *Wave_setTimeoffs(BPy_Wave *self,PyObject*a);
PyObject *Wave_getLifetime(BPy_Wave *self);
PyObject *Wave_setLifetime(BPy_Wave *self,PyObject*a);

/*****************************************************************************/
/* Python Wave_Type callback function prototypes:                            */
/*****************************************************************************/
void WaveDeAlloc (BPy_Wave *msh);
int WavePrint (BPy_Wave *msh, FILE *fp, int flags);
int WaveSetAttr (BPy_Wave *msh, char *name, PyObject *v);
PyObject *WaveGetAttr (BPy_Wave *msh, char *name);
PyObject *WaveRepr (BPy_Wave *msh);
PyObject* WaveCreatePyObject (struct Effect *wave);
int WaveCheckPyObject (PyObject *py_obj);
struct Wave* WaveFromPyObject (PyObject *py_obj);


#endif /* EXPP_WAVE_H */
