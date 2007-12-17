/* 
 * $Id: Material.h 10649 2007-05-04 03:23:40Z campbellbarton $
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
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

#ifndef EXPP_MATERIAL_H
#define EXPP_MATERIAL_H

#include <Python.h>
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h" /* colorband */
#include "rgbTuple.h"

/*****************************************************************************/
/* Python BPy_Material structure definition:        */
/*****************************************************************************/
typedef struct {
	PyObject_HEAD 
	Material * material; /* libdata must be second */
	BPy_rgbTuple *col, *amb, *spec, *mir, *sss;
} BPy_Material;

extern PyTypeObject Material_Type;	/* The Material PyType Object */

#define BPy_Material_Check(v) \
		((v)->ob_type == &Material_Type)	/* for type checking */

/*****************************************************************************/
/* Module Blender.Material - public functions	 */
/*****************************************************************************/
PyObject *M_Material_Init( void );

PyObject *Material_Init( void );
PyObject *Material_CreatePyObject( Material * mat );
Material *Material_FromPyObject( PyObject * pyobj );

/* colorband tp_getseters */
PyObject *EXPP_PyList_fromColorband( ColorBand *coba );
int EXPP_Colorband_fromPyList( ColorBand **coba, PyObject * value );

/* Some functions needed by NMesh, Curve and friends */
PyObject *EXPP_PyList_fromMaterialList( Material ** matlist, int len,
					int all );
Material **EXPP_newMaterialList_fromPyList( PyObject * list );
Material **EXPP_newMaterialList( int len );
void EXPP_incr_mats_us( Material ** matlist, int len );
int EXPP_synchronizeMaterialLists( Object * object );
int EXPP_releaseMaterialList( Material ** matlist, int len );

#endif				/* EXPP_MATERIAL_H */
