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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/util/crazyspace.c
 *  \ingroup edutil
 */


#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_bitmap.h"

#include "BKE_DerivedMesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_mesh.h"
#include "BKE_editmesh.h"

#include "ED_util.h"

typedef struct {
	float (*vertexcos)[3];
	BLI_bitmap *vertex_visit;
} MappedUserData;

BLI_INLINE void tan_calc_quat_v3(
        float r_quat[4],
        const float co_1[3], const float co_2[3], const float co_3[3])
{
	float vec_u[3], vec_v[3];
	float nor[3];

	sub_v3_v3v3(vec_u, co_1, co_2);
	sub_v3_v3v3(vec_v, co_1, co_3);

	cross_v3_v3v3(nor, vec_u, vec_v);

	if (normalize_v3(nor) > FLT_EPSILON) {
		const float zero_vec[3] = {0.0f};
		tri_to_quat_ex(r_quat, zero_vec, vec_u, vec_v, nor);
	}
	else {
		unit_qt(r_quat);
	}
}

static void set_crazy_vertex_quat(
        float r_quat[4],
        const float co_1[3], const float co_2[3], const float co_3[3],
        const float vd_1[3], const float vd_2[3], const float vd_3[3])
{
	float q1[4], q2[4];

	tan_calc_quat_v3(q1, co_1, co_2, co_3);
	tan_calc_quat_v3(q2, vd_1, vd_2, vd_3);

	sub_qt_qtqt(r_quat, q2, q1);
}

static void make_vertexcos__mapFunc(void *userData, int index, const float co[3],
                                    const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	MappedUserData *mappedData = (MappedUserData *)userData;

	if (BLI_BITMAP_GET(mappedData->vertex_visit, index) == 0) {
		/* we need coord from prototype vertex, not from copies,
		 * assume they stored in the beginning of vertex array stored in DM
		 * (mirror modifier for eg does this) */
		copy_v3_v3(mappedData->vertexcos[index], co);
		BLI_BITMAP_SET(mappedData->vertex_visit, index);
	}
}

static int modifiers_disable_subsurf_temporary(Object *ob)
{
	ModifierData *md;
	int disabled = 0;

	for (md = ob->modifiers.first; md; md = md->next)
		if (md->type == eModifierType_Subsurf)
			if (md->mode & eModifierMode_OnCage) {
				md->mode ^= eModifierMode_DisableTemporary;
				disabled = 1;
			}

	return disabled;
}

/* disable subsurf temporal, get mapped cos, and enable it */
float (*crazyspace_get_mapped_editverts(Scene *scene, Object *obedit))[3]
{
	Mesh *me = obedit->data;
	DerivedMesh *dm;
	float (*vertexcos)[3];
	int nverts = me->edit_btmesh->bm->totvert;
	BLI_bitmap *vertex_visit;
	MappedUserData userData;

	/* disable subsurf temporal, get mapped cos, and enable it */
	if (modifiers_disable_subsurf_temporary(obedit)) {
		/* need to make new derivemesh */
		makeDerivedMesh(scene, obedit, me->edit_btmesh, CD_MASK_BAREMESH, 0);
	}

	/* now get the cage */
	dm = editbmesh_get_derived_cage(scene, obedit, me->edit_btmesh, CD_MASK_BAREMESH);

	vertexcos = MEM_callocN(sizeof(*vertexcos) * nverts, "vertexcos map");
	vertex_visit = BLI_BITMAP_NEW(nverts, "vertexcos flags");

	userData.vertexcos = vertexcos;
	userData.vertex_visit = vertex_visit;
	dm->foreachMappedVert(dm, make_vertexcos__mapFunc, &userData, DM_FOREACH_NOP);

	dm->release(dm);

	/* set back the flag, no new cage needs to be built, transform does it */
	modifiers_disable_subsurf_temporary(obedit);

	MEM_freeN(vertex_visit);

	return vertexcos;
}

void crazyspace_set_quats_editmesh(BMEditMesh *em, float (*origcos)[3], float (*mappedcos)[3], float (*quats)[4],
                                   const bool use_select)
{
	BMFace *f;
	BMIter iter;
	int index;

	{
		BMVert *v;
		BM_ITER_MESH_INDEX (v, &iter, em->bm, BM_VERTS_OF_MESH, index) {
			BM_elem_flag_disable(v, BM_ELEM_TAG);
			BM_elem_index_set(v, index);  /* set_inline */
		}
		em->bm->elem_index_dirty &= ~BM_VERT;
	}

	BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
		BMLoop *l_iter, *l_first;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			if (BM_elem_flag_test(l_iter->v, BM_ELEM_HIDDEN) ||
			    BM_elem_flag_test(l_iter->v, BM_ELEM_TAG) ||
			    (use_select && !BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)))
			{
				continue;
			}

			if (!BM_elem_flag_test(l_iter->v, BM_ELEM_TAG)) {
				const float *co_prev, *co_curr, *co_next;  /* orig */
				const float *vd_prev, *vd_curr, *vd_next;  /* deform */

				const int i_prev = BM_elem_index_get(l_iter->prev->v);
				const int i_curr = BM_elem_index_get(l_iter->v);
				const int i_next = BM_elem_index_get(l_iter->next->v);

				/* retrieve mapped coordinates */
				vd_prev = mappedcos[i_prev];
				vd_curr = mappedcos[i_curr];
				vd_next = mappedcos[i_next];

				if (origcos) {
					co_prev = origcos[i_prev];
					co_curr = origcos[i_curr];
					co_next = origcos[i_next];
				}
				else {
					co_prev = l_iter->prev->v->co;
					co_curr = l_iter->v->co;
					co_next = l_iter->next->v->co;
				}

				set_crazy_vertex_quat(quats[i_curr],
				                      co_curr, co_next, co_prev,
				                      vd_curr, vd_next, vd_prev);

				BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);
			}
		} while ((l_iter = l_iter->next) != l_first);
	}
}

void crazyspace_set_quats_mesh(Mesh *me, float (*origcos)[3], float (*mappedcos)[3], float (*quats)[4])
{
	int i;
	MVert *mvert;
	MLoop *mloop;
	MPoly *mp;

	mvert = me->mvert;
	for (i = 0; i < me->totvert; i++, mvert++)
		mvert->flag &= ~ME_VERT_TMP_TAG;

	/* first store two sets of tangent vectors in vertices, we derive it just from the face-edges */
	mvert = me->mvert;
	mp = me->mpoly;
	mloop = me->mloop;

	for (i = 0; i < me->totpoly; i++, mp++) {
		MLoop *ml_prev, *ml_curr, *ml_next;
		int j;

		ml_next = &mloop[mp->loopstart];
		ml_curr = &ml_next[mp->totloop - 1];
		ml_prev = &ml_next[mp->totloop - 2];

		for (j = 0; j < mp->totloop; j++) {
			if ((mvert[ml_curr->v].flag & ME_VERT_TMP_TAG) == 0) {
				const float *co_prev, *co_curr, *co_next;  /* orig */
				const float *vd_prev, *vd_curr, *vd_next;  /* deform */

				/* retrieve mapped coordinates */
				vd_prev = mappedcos[ml_prev->v];
				vd_curr = mappedcos[ml_curr->v];
				vd_next = mappedcos[ml_next->v];

				if (origcos) {
					co_prev = origcos[ml_prev->v];
					co_curr = origcos[ml_curr->v];
					co_next = origcos[ml_next->v];
				}
				else {
					co_prev = mvert[ml_prev->v].co;
					co_curr = mvert[ml_curr->v].co;
					co_next = mvert[ml_next->v].co;
				}

				set_crazy_vertex_quat(quats[ml_curr->v],
				                      co_curr, co_next, co_prev,
				                      vd_curr, vd_next, vd_prev);

				mvert[ml_curr->v].flag |= ME_VERT_TMP_TAG;
			}

			ml_prev = ml_curr;
			ml_curr = ml_next;
			ml_next++;
		}
	}
}

int editbmesh_get_first_deform_matrices(Scene *scene, Object *ob, BMEditMesh *em, 
                                        float (**deformmats)[3][3], float (**deformcos)[3])
{
	ModifierData *md;
	DerivedMesh *dm;
	int i, a, numleft = 0, numVerts = 0;
	int cageIndex = modifiers_getCageIndex(scene, ob, NULL, 1);
	float (*defmats)[3][3] = NULL, (*deformedVerts)[3] = NULL;
	VirtualModifierData virtualModifierData;

	modifiers_clearErrors(ob);

	dm = NULL;
	md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

	/* compute the deformation matrices and coordinates for the first
	 * modifiers with on cage editing that are enabled and support computing
	 * deform matrices */
	for (i = 0; md && i <= cageIndex; i++, md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!editbmesh_modifier_is_enabled(scene, md, dm))
			continue;

		if (mti->type == eModifierTypeType_OnlyDeform && mti->deformMatricesEM) {
			if (!defmats) {
				dm = getEditDerivedBMesh(em, ob, NULL);
				deformedVerts = editbmesh_get_vertex_cos(em, &numVerts);
				defmats = MEM_mallocN(sizeof(*defmats) * numVerts, "defmats");

				for (a = 0; a < numVerts; a++)
					unit_m3(defmats[a]);
			}

			mti->deformMatricesEM(md, ob, em, dm, deformedVerts, defmats,
			                      numVerts);
		}
		else
			break;
	}

	for (; md && i <= cageIndex; md = md->next, i++)
		if (editbmesh_modifier_is_enabled(scene, md, dm) && modifier_isCorrectableDeformed(md))
			numleft++;

	if (dm)
		dm->release(dm);

	*deformmats = defmats;
	*deformcos = deformedVerts;

	return numleft;
}

int sculpt_get_first_deform_matrices(Scene *scene, Object *ob, float (**deformmats)[3][3], float (**deformcos)[3])
{
	ModifierData *md;
	DerivedMesh *dm;
	int a, numVerts = 0;
	float (*defmats)[3][3] = NULL, (*deformedVerts)[3] = NULL;
	MultiresModifierData *mmd = get_multires_modifier(scene, ob, 0);
	const bool has_multires = mmd != NULL && mmd->sculptlvl > 0;
	int numleft = 0;
	VirtualModifierData virtualModifierData;

	if (has_multires) {
		*deformmats = NULL;
		*deformcos = NULL;
		return numleft;
	}

	dm = NULL;
	md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

	for (; md; md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!modifier_isEnabled(scene, md, eModifierMode_Realtime)) continue;

		if (mti->type == eModifierTypeType_OnlyDeform) {
			if (!defmats) {
				Mesh *me = (Mesh *)ob->data;
				dm = mesh_create_derived(me, NULL);
				deformedVerts = BKE_mesh_vertexCos_get(me, &numVerts);
				defmats = MEM_callocN(sizeof(*defmats) * numVerts, "defmats");

				for (a = 0; a < numVerts; a++)
					unit_m3(defmats[a]);
			}

			if (mti->deformMatrices) mti->deformMatrices(md, ob, dm, deformedVerts, defmats, numVerts);
			else break;
		}
	}

	for (; md; md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!modifier_isEnabled(scene, md, eModifierMode_Realtime)) continue;

		if (mti->type == eModifierTypeType_OnlyDeform)
			numleft++;
	}

	if (dm)
		dm->release(dm);

	*deformmats = defmats;
	*deformcos = deformedVerts;

	return numleft;
}

void crazyspace_build_sculpt(Scene *scene, Object *ob, float (**deformmats)[3][3], float (**deformcos)[3])
{
	int totleft = sculpt_get_first_deform_matrices(scene, ob, deformmats, deformcos);

	if (totleft) {
		/* there are deformation modifier which doesn't support deformation matrices
		 * calculation. Need additional crazyspace correction */

		float (*deformedVerts)[3] = *deformcos;
		float (*origVerts)[3] = MEM_dupallocN(deformedVerts);
		float (*quats)[4];
		int i, deformed = 0;
		VirtualModifierData virtualModifierData;
		ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
		Mesh *me = (Mesh *)ob->data;

		for (; md; md = md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);

			if (!modifier_isEnabled(scene, md, eModifierMode_Realtime)) continue;

			if (mti->type == eModifierTypeType_OnlyDeform) {
				/* skip leading modifiers which have been already
				 * handled in sculpt_get_first_deform_matrices */
				if (mti->deformMatrices && !deformed)
					continue;

				mti->deformVerts(md, ob, NULL, deformedVerts, me->totvert, 0);
				deformed = 1;
			}
		}

		quats = MEM_mallocN(me->totvert * sizeof(*quats), "crazy quats");

		crazyspace_set_quats_mesh(me, origVerts, deformedVerts, quats);

		for (i = 0; i < me->totvert; i++) {
			float qmat[3][3], tmat[3][3];

			quat_to_mat3(qmat, quats[i]);
			mul_m3_m3m3(tmat, qmat, (*deformmats)[i]);
			copy_m3_m3((*deformmats)[i], tmat);
		}

		MEM_freeN(origVerts);
		MEM_freeN(quats);
	}

	if (*deformmats == NULL) {
		int a, numVerts;
		Mesh *me = (Mesh *)ob->data;

		*deformcos = BKE_mesh_vertexCos_get(me, &numVerts);
		*deformmats = MEM_callocN(sizeof(*(*deformmats)) * numVerts, "defmats");

		for (a = 0; a < numVerts; a++)
			unit_m3((*deformmats)[a]);
	}
}
