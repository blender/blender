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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/mesh_iterators.c
 *  \ingroup bke
 *
 * Functions for iterating mesh features.
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_mesh_iterators.h"


/* Copied from cdDM_foreachMappedVert */
void BKE_mesh_foreach_mapped_vert(
        Mesh *mesh,
        void (*func)(void *userData, int index, const float co[3], const float no_f[3], const short no_s[3]),
        void *userData,
        MeshForeachFlag flag)
{
	MVert *mv = mesh->mvert;
	const int *index = CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX);
	int i;

	if (index) {
		for (i = 0; i < mesh->totvert; i++, mv++) {
			const short *no = (flag & MESH_FOREACH_USE_NORMAL) ? mv->no : NULL;
			const int orig = *index++;
			if (orig == ORIGINDEX_NONE) continue;
			func(userData, orig, mv->co, NULL, no);
		}
	}
	else {
		for (i = 0; i < mesh->totvert; i++, mv++) {
			const short *no = (flag & MESH_FOREACH_USE_NORMAL) ? mv->no : NULL;
			func(userData, i, mv->co, NULL, no);
		}
	}
}

/* Copied from cdDM_foreachMappedEdge */
void BKE_mesh_foreach_mapped_edge(
        Mesh *mesh,
        void (*func)(void *userData, int index, const float v0co[3], const float v1co[3]),
        void *userData)
{
	MVert *mv = mesh->mvert;
	MEdge *med = mesh->medge;
	int i, orig, *index = CustomData_get_layer(&mesh->edata, CD_ORIGINDEX);

	for (i = 0; i < mesh->totedge; i++, med++) {
		if (index) {
			orig = *index++;
			if (orig == ORIGINDEX_NONE) continue;
			func(userData, orig, mv[med->v1].co, mv[med->v2].co);
		}
		else
			func(userData, i, mv[med->v1].co, mv[med->v2].co);
	}
}

/* Copied from cdDM_foreachMappedLoop */
void BKE_mesh_foreach_mapped_loop(
        Mesh *mesh,
        void (*func)(void *userData, int vertex_index, int face_index, const float co[3], const float no[3]),
        void *userData,
        MeshForeachFlag flag)
{
	/* We can't use dm->getLoopDataLayout(dm) here, we want to always access dm->loopData, EditDerivedBMesh would
	 * return loop data from bmesh itself. */
	const float (*lnors)[3] = (flag & MESH_FOREACH_USE_NORMAL) ? CustomData_get_layer(&mesh->ldata, CD_NORMAL) : NULL;

	const MVert *mv = mesh->mvert;
	const MLoop *ml = mesh->mloop;
	const MPoly *mp = mesh->mpoly;
	const int *v_index = CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX);
	const int *f_index = CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX);
	int p_idx, i;

	for (p_idx = 0; p_idx < mesh->totpoly; ++p_idx, ++mp) {
		for (i = 0; i < mp->totloop; ++i, ++ml) {
			const int v_idx = v_index ? v_index[ml->v] : ml->v;
			const int f_idx = f_index ? f_index[p_idx] : p_idx;
			const float *no = lnors ? *lnors++ : NULL;
			if (!ELEM(ORIGINDEX_NONE, v_idx, f_idx)) {
				func(userData, v_idx, f_idx, mv[ml->v].co, no);
			}
		}
	}
}

/* Copied from cdDM_foreachMappedFaceCenter */
void BKE_mesh_foreach_mapped_face_center(
        Mesh *mesh,
        void (*func)(void *userData, int index, const float cent[3], const float no[3]),
        void *userData,
        MeshForeachFlag flag)
{
	MVert *mvert = mesh->mvert;
	MPoly *mp;
	MLoop *ml;
	int i, orig, *index;

	index = CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX);
	mp = mesh->mpoly;
	for (i = 0; i < mesh->totpoly; i++, mp++) {
		float cent[3];
		float *no, _no[3];

		if (index) {
			orig = *index++;
			if (orig == ORIGINDEX_NONE) continue;
		}
		else {
			orig = i;
		}

		ml = &mesh->mloop[mp->loopstart];
		BKE_mesh_calc_poly_center(mp, ml, mvert, cent);

		if (flag & MESH_FOREACH_USE_NORMAL) {
			BKE_mesh_calc_poly_normal(mp, ml, mvert, (no = _no));
		}
		else {
			no = NULL;
		}

		func(userData, orig, cent, no);
	}

}
