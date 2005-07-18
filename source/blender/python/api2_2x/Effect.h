/*
 * $Id$
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

#ifndef EXPP_EFFECT_H
#define EXPP_EFFECT_H

#include <Python.h>
#include "DNA_effect_types.h"

extern PyTypeObject Effect_Type;

#define BPy_Effect_Check(v) ((v)->ob_type==&Effect_Type)

/* Python BPy_Effect structure definition */
typedef struct {
	PyObject_HEAD		/* required py macro */
	Effect * effect;
} BPy_Effect;


/*****************************************************************************/
/* Python API function prototypes for the Effect module.                     */
/*****************************************************************************/
PyObject *M_Effect_New( PyObject * self, PyObject * args );
PyObject *M_Effect_Get( PyObject * self, PyObject * args );



/*****************************************************************************/
/* Python BPy_Effect methods declarations:                                   */
/*****************************************************************************/
/*PyObject *Effect_getType(BPy_Effect *self);*/


/*****************************************************************************/
/* Python Effect_Type callback function prototypes:                          */
/*****************************************************************************/

PyObject *Effect_Init( void );
void EffectDeAlloc( BPy_Effect * msh );
//int EffectPrint (BPy_Effect *msh, FILE *fp, int flags);
int EffectSetAttr( BPy_Effect * msh, char *name, PyObject * v );
PyObject *EffectGetAttr( BPy_Effect * msh, char *name );
PyObject *EffectRepr( BPy_Effect * msh );
PyObject *EffectCreatePyObject( struct Effect *effect );
int EffectCheckPyObject( PyObject * py_obj );
struct Effect *EffectFromPyObject( PyObject * py_obj );

#endif				/* EXPP_EFFECT_H */
