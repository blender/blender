/**
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
/* opy_nmesh.c */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DNA_mesh_types.h"

#define NMesh_Check(v)       ((v)->ob_type == &NMesh_Type)
#define NMFace_Check(v)      ((v)->ob_type == &NMFace_Type)
#define NMVert_Check(v)      ((v)->ob_type == &NMVert_Type)
#define NMCol_Check(v)       ((v)->ob_type == &NMCol_Type)

typedef struct _NMCol {
	PyObject_HEAD

	unsigned char r, g, b, a;
} NMCol;

struct PyBlock;

typedef struct _NMFace {
	PyObject_HEAD
	
	PyObject *v;
	PyObject *uv;
	PyObject *col;
	short mode;
	short flag;
	unsigned char transp;
	DataBlock *tpage; /* Image */
	char mat_nr, smooth;
} NMFace;

typedef struct _NMesh {
	PyObject_HEAD
	Mesh *mesh;
	PyObject *name;
	PyObject *materials;
	PyObject *verts;
	PyObject *faces;
	int sel_face; /* XXX remove */
	char flags;
#define NMESH_HASMCOL	1<<0
#define NMESH_HASVERTUV	1<<1
#define NMESH_HASFACEUV	1<<2

} NMesh;

typedef struct _NMVert {
	PyObject_VAR_HEAD

	float co[3];
	float no[3];
	float uvco[3];
	
	int index;
} NMVert;


/* PROTOS */

PyObject *newNMesh(Mesh *oldmesh);
Mesh *Mesh_fromNMesh(NMesh *nmesh);
PyObject *NMesh_assignMaterials_toObject(NMesh *nmesh, Object *ob);
Material **nmesh_updateMaterials(NMesh *nmesh);

