/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_BOOLEANOPS_MESH_H
#define BKE_BOOLEANOPS_MESH_H

/** \file BKE_booleanops_mesh.h
 *  \ingroup bke
 */

#include "CSG_BooleanOps.h"

/**
 * Internal mesh structure.
 * Safe to copy by value... hopefully.
 */

struct Base;
struct Object;
struct CSG_MeshDescriptor;

typedef void (*CSG_DestroyMeshFunc)(struct CSG_MeshDescriptor *);

typedef struct CSG_MeshDescriptor {
	struct Base *base; // Ptr to base of original blender object - used in creating a new object
	CSG_MeshPropertyDescriptor m_descriptor;
	CSG_FaceIteratorDescriptor m_face_iterator;
	CSG_VertexIteratorDescriptor m_vertex_iterator;
	CSG_DestroyMeshFunc m_destroy_func;
} CSG_MeshDescriptor;


extern
	int
CSG_LoadBlenderMesh(
	struct Object * obj,
	CSG_MeshDescriptor *output
);

/**
 * Destroy the contents of a mesh descriptor.
 * If the internal descriptor refers to a blender
 * mesh, no action is performed apart from freeing
 * internal memory in the desriptor.
 */

extern
	void
CSG_DestroyMeshDescriptor(
	CSG_MeshDescriptor *mesh
);

/**
 * Perform a boolean operation between 2 meshes and return the 
 * result as a new mesh descriptor.
 * op_type is an integer code of the boolean operation type.
 * 1 = intersection,
 * 2 = union,
 * 3 = difference.
 */

extern
	int 
CSG_PerformOp(
	CSG_MeshDescriptor *mesh1,
	CSG_MeshDescriptor *mesh2,
	int op_type,
	CSG_MeshDescriptor *output
);



/**
 * Add a mesh to blender as a new object.
 */

extern
	int
CSG_AddMeshToBlender(
	CSG_MeshDescriptor *mesh
);

/**
 * Test functionality.
 */

extern
	int
NewBooleanMeshTest(
	struct Base * base,
	struct Base * base_select,
	int op_type
);

#endif

