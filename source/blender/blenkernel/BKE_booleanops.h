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
#ifndef BKE_BOOLEANOPS_H
#define BKE_BOOLEANOPS_H

struct Mesh;
struct Object;
struct Base;
struct CSG_FaceIteratorDescriptor;
struct CSG_VertexIteratorDescriptor;
struct CSG_MeshPropertyDescriptor;

/**
 * Perform a boolean operation between 2 mesh objects and
 * add the result as a new mesh into blender data structures.
 * Assumes you have checked that the 2 objects are infact mesh
 * objects. Returns 1 on success and zero when it encountered
 * a problem. In the latter case no object is added and you should
 * report an appropriate error.
 */
extern
	int
NewBooleanMesh(
	struct Base * base,
	struct Base * base_select,
	int op_type
);

	
/**
 * Functions exposed for use by BKE_booleanops_mesh
 */

/**
 * Returns pointers to new mesh descriptors for 
 * the given mesh. Make sure you call FreeMeshDescriptors
 * after calling this function.
 */ 

extern 
	void
BuildMeshDescriptors(
	struct Object * ob,
	struct CSG_FaceIteratorDescriptor * face_it,
	struct CSG_VertexIteratorDescriptor * vertex_it
);

extern
	void
FreeMeshDescriptors(
	struct CSG_FaceIteratorDescriptor * face_it,
	struct CSG_VertexIteratorDescriptor * vertex_it
);

extern
	int
ConvertCSGDescriptorsToMeshObject(
	struct Object *ob,
	struct CSG_MeshPropertyDescriptor * props,
	struct CSG_FaceIteratorDescriptor * face_it,
	struct CSG_VertexIteratorDescriptor * vertex_it,
	float parinv[][4]
);

/**
 * This little function adds a new mesh object 
 * to the blender object list. It uses ob to duplicate
 * data as this seems to be easier than cerating a new one.
 * This new oject contains no faces nor vertices.
 */

extern
	struct Object *
AddNewBlenderMesh(
	struct Base *base
);

extern
	int
InterpNoUserData(
	void *d1,
	void *d2,
	void *dnew,
	float epsilon
);

extern
	int
InterpFaceVertexData(
	void *d1,
	void *d2,
	void *dnew,
	float epsilon
);

typedef struct {
	float uv[2];
	float color[4];
} FaceVertexData;

typedef struct {
	struct Material *material;
	
		/* assorted tface flags */
	void *tpage;
	char flag, transp, faceflag;
	short mode, tile;
} FaceData;

#endif

