/*
 * $Id: Mesh.h 10269 2007-03-15 01:09:14Z campbellbarton $
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
 * Contributor(s): Ken Hughes
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/* Most of this file comes from opy_nmesh.[ch] in the old bpython dir */

#ifndef EXPP_MESH_H
#define EXPP_MESH_H

#include <Python.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "Material.h"
#include "Image.h"

/* EXPP PyType Objects */
extern PyTypeObject Mesh_Type;
extern PyTypeObject MVert_Type;
extern PyTypeObject PVert_Type;
extern PyTypeObject MVertSeq_Type;
extern PyTypeObject MEdge_Type;
extern PyTypeObject MFace_Type;
extern PyTypeObject MCol_Type;

struct BPy_Object;

/* Type checking for EXPP PyTypes */
#define BPy_Mesh_Check(v)       ((v)->ob_type == &Mesh_Type)
#define BPy_MFace_Check(v)      ((v)->ob_type == &MFace_Type)
#define BPy_MEdge_Check(v)      ((v)->ob_type == &MEdge_Type)
#define BPy_MVert_Check(v)      ((v)->ob_type == &MVert_Type)
#define BPy_PVert_Check(v)      ((v)->ob_type == &PVert_Type)
#define BPy_MCol_Check(v)       ((v)->ob_type == &MCol_Type)

/* Typedefs for the new types */

typedef struct {
	PyObject_HEAD		/* required python macro   */
	MCol *color;
} BPy_MCol;			    /* a Mesh color: [r,g,b,a] */

typedef struct {
	PyObject_VAR_HEAD	/* required python macro   */
	void * data;		/* points to a Mesh or an MVert */
	int index;
} BPy_MVert;			/* a Mesh vertex */

typedef struct {
	PyObject_VAR_HEAD	/* required python macro   */
	Mesh * mesh;
	int iter;
} BPy_MVertSeq;			/* a Mesh vertex sequence */

typedef struct {
	PyObject_VAR_HEAD	/* required python macro   */
	Mesh *mesh;			/* points to a Mesh */
	int index;
	short iter;			/* char because it can only ever be between -1 and 2 */
} BPy_MEdge;			/* a Mesh edge */

typedef struct {
	PyObject_VAR_HEAD	/* required python macro   */
	Mesh * mesh;
	int iter;
} BPy_MEdgeSeq;			/* a Mesh edge sequence */

typedef struct {
	PyObject_VAR_HEAD	/* required python macro   */
	Mesh * mesh;
	int index;
	short iter;			/* char because it can only ever be between -1 and 4 */
} BPy_MFace;			/* a Mesh face */

typedef struct {
	PyObject_VAR_HEAD	/* required python macro   */
	Mesh * mesh;
	int iter;
} BPy_MFaceSeq;			/* a Mesh face sequence */

typedef struct {
	PyObject_HEAD		/* required python macro   */
	Mesh *mesh;
	Object *object;
	char new;			/* was mesh created or already existed? */
} BPy_Mesh;

/* PROTOS */

PyObject *Mesh_Init( void );
PyObject *Mesh_CreatePyObject( Mesh * me, Object *obj );
Mesh *Mesh_FromPyObject( PyObject * pyobj, Object *obj );

#endif				/* EXPP_MESH_H */
