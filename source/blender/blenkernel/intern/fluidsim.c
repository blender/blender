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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/fluidsim.c
 *  \ingroup bke
 */


#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_customdata.h"
#include "BKE_fluidsim.h"
#include "BKE_library.h"
#include "BKE_mesh_runtime.h"

/* ************************* fluidsim bobj file handling **************************** */

//-------------------------------------------------------------------------------
// file handling
//-------------------------------------------------------------------------------

void initElbeemMesh(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *ob,
                    int *numVertices, float **vertices,
                    int *numTriangles, int **triangles,
                    int useGlobalCoords, int modifierIndex)
{
	Mesh *mesh;
	const MVert *mvert;
	const MLoop *mloop;
	const MLoopTri *looptri, *lt;
	int i, mvert_num, looptri_num;
	float *verts;
	int *tris;

	mesh = mesh_create_eval_final_index_render(depsgraph, scene, ob, CD_MASK_BAREMESH, modifierIndex);

	mvert = mesh->mvert;
	mloop = mesh->mloop;
	looptri = BKE_mesh_runtime_looptri_ensure(mesh);
	mvert_num = mesh->totvert;
	looptri_num = mesh->runtime.looptris.len;

	*numVertices = mvert_num;
	verts = MEM_mallocN(mvert_num * sizeof(float[3]), "elbeemmesh_vertices");
	for (i = 0; i < mvert_num; i++) {
		copy_v3_v3(&verts[i * 3], mvert[i].co);
		if (useGlobalCoords) { mul_m4_v3(ob->obmat, &verts[i * 3]); }
	}
	*vertices = verts;

	*numTriangles = looptri_num;
	tris = MEM_mallocN(looptri_num * sizeof(int[3]), "elbeemmesh_triangles");
	for (i = 0, lt = looptri; i < looptri_num; i++, lt++) {
		tris[(i * 3) + 0] = mloop[lt->tri[0]].v;
		tris[(i * 3) + 1] = mloop[lt->tri[1]].v;
		tris[(i * 3) + 2] = mloop[lt->tri[2]].v;
	}
	*triangles = tris;

	BKE_id_free(NULL, mesh);
}
