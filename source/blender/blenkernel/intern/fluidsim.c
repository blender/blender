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


// headers for fluidsim bobj meshes
#include <stdlib.h>
#include <zlib.h>
#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_force_types.h" // for pointcache
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_fluidsim.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"

/* ************************* fluidsim bobj file handling **************************** */


//-------------------------------------------------------------------------------
// file handling
//-------------------------------------------------------------------------------

void initElbeemMesh(struct Scene *scene, struct Object *ob,
                    int *numVertices, float **vertices,
                    int *numTriangles, int **triangles,
                    int useGlobalCoords, int modifierIndex)
{
	DerivedMesh *dm;
	const MVert *mvert;
	const MLoop *mloop;
	const MLoopTri *looptri, *lt;
	int i, mvert_num, looptri_num;
	float *verts;
	int *tris;

	dm = mesh_create_derived_index_render(scene, ob, CD_MASK_BAREMESH, modifierIndex);

	mvert = dm->getVertArray(dm);
	mloop = dm->getLoopArray(dm);
	looptri = dm->getLoopTriArray(dm);
	mvert_num = dm->getNumVerts(dm);
	looptri_num = dm->getNumLoopTri(dm);

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

	dm->release(dm);
}
