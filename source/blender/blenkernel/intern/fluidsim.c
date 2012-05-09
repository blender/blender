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
#include "DNA_object_fluidsim.h"
#include "DNA_object_force.h" // for pointcache 
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h" // N_T

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
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
	DerivedMesh *dm = NULL;
	MVert *mvert;
	MFace *mface;
	int countTris=0, i, totvert, totface;
	float *verts;
	int *tris;

	dm = mesh_create_derived_index_render(scene, ob, CD_MASK_BAREMESH, modifierIndex);

	DM_ensure_tessface(dm);

	mvert = dm->getVertArray(dm);
	mface = dm->getTessFaceArray(dm);
	totvert = dm->getNumVerts(dm);
	totface = dm->getNumTessFaces(dm);

	*numVertices = totvert;
	verts = MEM_callocN(totvert*3*sizeof(float), "elbeemmesh_vertices");
	for (i=0; i<totvert; i++) {
		copy_v3_v3(&verts[i*3], mvert[i].co);
		if (useGlobalCoords) { mul_m4_v3(ob->obmat, &verts[i*3]); }
	}
	*vertices = verts;

	for (i=0; i<totface; i++) {
		countTris++;
		if (mface[i].v4) { countTris++; }
	}
	*numTriangles = countTris;
	tris = MEM_callocN(countTris*3*sizeof(int), "elbeemmesh_triangles");
	countTris = 0;
	for (i=0; i<totface; i++) {
		int face[4];
		face[0] = mface[i].v1;
		face[1] = mface[i].v2;
		face[2] = mface[i].v3;
		face[3] = mface[i].v4;

		tris[countTris*3+0] = face[0];
		tris[countTris*3+1] = face[1];
		tris[countTris*3+2] = face[2];
		countTris++;
		if (face[3]) {
			tris[countTris*3+0] = face[0];
			tris[countTris*3+1] = face[2];
			tris[countTris*3+2] = face[3];
			countTris++;
		}
	}
	*triangles = tris;

	dm->release(dm);
}

