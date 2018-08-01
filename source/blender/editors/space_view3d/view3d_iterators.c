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
 * Contributor(s): Blender Foundation, full recode and added functions
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_iterators.c
 *  \ingroup spview3d
 */

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_rect.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_curve.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_editmesh.h"
#include "BKE_context.h"
#include "BKE_mesh_runtime.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "bmesh.h"

#include "ED_screen.h"
#include "ED_armature.h"
#include "ED_view3d.h"

typedef struct foreachScreenObjectVert_userData {
	void (*func)(void *userData, MVert *mv, const float screen_co_b[2], int index);
	void *userData;
	ViewContext vc;
	eV3DProjTest clip_flag;
} foreachScreenObjectVert_userData;

typedef struct foreachScreenVert_userData {
	void (*func)(void *userData, BMVert *eve, const float screen_co_b[2], int index);
	void *userData;
	ViewContext vc;
	eV3DProjTest clip_flag;
} foreachScreenVert_userData;

/* user data structures for derived mesh callbacks */
typedef struct foreachScreenEdge_userData {
	void (*func)(void *userData, BMEdge *eed, const float screen_co_a[2], const float screen_co_b[2], int index);
	void *userData;
	ViewContext vc;
	rctf win_rect; /* copy of: vc.ar->winx/winy, use for faster tests, minx/y will always be 0 */
	eV3DProjTest clip_flag;
} foreachScreenEdge_userData;

typedef struct foreachScreenFace_userData {
	void (*func)(void *userData, BMFace *efa, const float screen_co_b[2], int index);
	void *userData;
	ViewContext vc;
	eV3DProjTest clip_flag;
} foreachScreenFace_userData;


/* Note! - foreach funcs should be called while drawing or directly after
 * if not, ED_view3d_init_mats_rv3d() can be used for selection tools
 * but would not give correct results with dupli's for eg. which don't
 * use the object matrix in the usual way */

/* ------------------------------------------------------------------------ */


static void meshobject_foreachScreenVert__mapFunc(void *userData, int index, const float co[3],
                                            const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	foreachScreenObjectVert_userData *data = userData;
	struct MVert *mv = &((Mesh *)(data->vc.obact->data))->mvert[index];

	if (!(mv->flag & ME_HIDE)) {
		float screen_co[2];

		if (ED_view3d_project_float_object(data->vc.ar, co, screen_co, data->clip_flag) != V3D_PROJ_RET_OK) {
			return;
		}

		data->func(data->userData, mv, screen_co, index);
	}
}

void meshobject_foreachScreenVert(
        ViewContext *vc,
        void (*func)(void *userData, MVert *eve, const float screen_co[2], int index),
        void *userData, eV3DProjTest clip_flag)
{
	foreachScreenObjectVert_userData data;
	DerivedMesh *dm;

	dm = mesh_get_derived_deform(vc->depsgraph, vc->scene, vc->obact, CD_MASK_BAREMESH);

	ED_view3d_check_mats_rv3d(vc->rv3d);

	data.vc = *vc;
	data.func = func;
	data.userData = userData;
	data.clip_flag = clip_flag;

	if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
		ED_view3d_clipping_local(vc->rv3d, vc->obact->obmat);
	}

	dm->foreachMappedVert(dm, meshobject_foreachScreenVert__mapFunc, &data, DM_FOREACH_NOP);

	dm->release(dm);
}

static void mesh_foreachScreenVert__mapFunc(void *userData, int index, const float co[3],
                                            const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	foreachScreenVert_userData *data = userData;
	BMVert *eve = BM_vert_at_index(data->vc.em->bm, index);

	if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
		float screen_co[2];

		if (ED_view3d_project_float_object(data->vc.ar, co, screen_co, data->clip_flag) != V3D_PROJ_RET_OK) {
			return;
		}

		data->func(data->userData, eve, screen_co, index);
	}
}

void mesh_foreachScreenVert(
        ViewContext *vc,
        void (*func)(void *userData, BMVert *eve, const float screen_co[2], int index),
        void *userData, eV3DProjTest clip_flag)
{
	foreachScreenVert_userData data;
	DerivedMesh *dm;

	dm = editbmesh_get_derived_cage(vc->depsgraph, vc->scene, vc->obedit, vc->em, CD_MASK_BAREMESH);

	ED_view3d_check_mats_rv3d(vc->rv3d);

	data.vc = *vc;
	data.func = func;
	data.userData = userData;
	data.clip_flag = clip_flag;

	if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
		ED_view3d_clipping_local(vc->rv3d, vc->obedit->obmat);  /* for local clipping lookups */
	}

	BM_mesh_elem_table_ensure(vc->em->bm, BM_VERT);
	dm->foreachMappedVert(dm, mesh_foreachScreenVert__mapFunc, &data, DM_FOREACH_NOP);

	dm->release(dm);
}

/* ------------------------------------------------------------------------ */

static void mesh_foreachScreenEdge__mapFunc(void *userData, int index, const float v0co[3], const float v1co[3])
{
	foreachScreenEdge_userData *data = userData;
	BMEdge *eed = BM_edge_at_index(data->vc.em->bm, index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
		float screen_co_a[2];
		float screen_co_b[2];
		eV3DProjTest clip_flag_nowin = data->clip_flag &= ~V3D_PROJ_TEST_CLIP_WIN;

		if (ED_view3d_project_float_object(data->vc.ar, v0co, screen_co_a, clip_flag_nowin) != V3D_PROJ_RET_OK) {
			return;
		}
		if (ED_view3d_project_float_object(data->vc.ar, v1co, screen_co_b, clip_flag_nowin) != V3D_PROJ_RET_OK) {
			return;
		}

		if (data->clip_flag & V3D_PROJ_TEST_CLIP_WIN) {
			if (!BLI_rctf_isect_segment(&data->win_rect, screen_co_a, screen_co_b)) {
				return;
			}
		}

		data->func(data->userData, eed, screen_co_a, screen_co_b, index);
	}
}

void mesh_foreachScreenEdge(
        ViewContext *vc,
        void (*func)(void *userData, BMEdge *eed, const float screen_co_a[2], const float screen_co_b[2], int index),
        void *userData, eV3DProjTest clip_flag)
{
	foreachScreenEdge_userData data;
	DerivedMesh *dm;

	dm = editbmesh_get_derived_cage(vc->depsgraph, vc->scene, vc->obedit, vc->em, CD_MASK_BAREMESH);

	ED_view3d_check_mats_rv3d(vc->rv3d);

	data.vc = *vc;

	data.win_rect.xmin = 0;
	data.win_rect.ymin = 0;
	data.win_rect.xmax = vc->ar->winx;
	data.win_rect.ymax = vc->ar->winy;

	data.func = func;
	data.userData = userData;
	data.clip_flag = clip_flag;

	if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
		ED_view3d_clipping_local(vc->rv3d, vc->obedit->obmat);  /* for local clipping lookups */
	}

	BM_mesh_elem_table_ensure(vc->em->bm, BM_EDGE);
	dm->foreachMappedEdge(dm, mesh_foreachScreenEdge__mapFunc, &data);

	dm->release(dm);
}

/* ------------------------------------------------------------------------ */

static void mesh_foreachScreenFace__mapFunc(void *userData, int index, const float cent[3], const float UNUSED(no[3]))
{
	foreachScreenFace_userData *data = userData;
	BMFace *efa = BM_face_at_index(data->vc.em->bm, index);

	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		float screen_co[2];
		if (ED_view3d_project_float_object(data->vc.ar, cent, screen_co, data->clip_flag) == V3D_PROJ_RET_OK) {
			data->func(data->userData, efa, screen_co, index);
		}
	}
}

void mesh_foreachScreenFace(
        ViewContext *vc,
        void (*func)(void *userData, BMFace *efa, const float screen_co_b[2], int index),
        void *userData, const eV3DProjTest clip_flag)
{
	foreachScreenFace_userData data;
	DerivedMesh *dm;

	dm = editbmesh_get_derived_cage(vc->depsgraph, vc->scene, vc->obedit, vc->em, CD_MASK_BAREMESH);

	ED_view3d_check_mats_rv3d(vc->rv3d);

	data.vc = *vc;
	data.func = func;
	data.userData = userData;
	data.clip_flag = clip_flag;

	BM_mesh_elem_table_ensure(vc->em->bm, BM_FACE);
	dm->foreachMappedFaceCenter(dm, mesh_foreachScreenFace__mapFunc, &data, DM_FOREACH_NOP);

	dm->release(dm);
}

/* ------------------------------------------------------------------------ */

void nurbs_foreachScreenVert(
        ViewContext *vc,
        void (*func)(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, const float screen_co_b[2]),
        void *userData, const eV3DProjTest clip_flag)
{
	Curve *cu = vc->obedit->data;
	Nurb *nu;
	int i;
	ListBase *nurbs = BKE_curve_editNurbs_get(cu);

	ED_view3d_check_mats_rv3d(vc->rv3d);

	if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
		ED_view3d_clipping_local(vc->rv3d, vc->obedit->obmat); /* for local clipping lookups */
	}

	for (nu = nurbs->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			for (i = 0; i < nu->pntsu; i++) {
				BezTriple *bezt = &nu->bezt[i];

				if (bezt->hide == 0) {
					float screen_co[2];

					if (cu->drawflag & CU_HIDE_HANDLES) {
						if (ED_view3d_project_float_object(vc->ar, bezt->vec[1], screen_co,
						                                   V3D_PROJ_RET_CLIP_BB | V3D_PROJ_RET_CLIP_WIN) == V3D_PROJ_RET_OK)
						{
							func(userData, nu, NULL, bezt, 1, screen_co);
						}
					}
					else {
						if (ED_view3d_project_float_object(vc->ar, bezt->vec[0], screen_co,
						                                   V3D_PROJ_RET_CLIP_BB | V3D_PROJ_RET_CLIP_WIN) == V3D_PROJ_RET_OK)
						{
							func(userData, nu, NULL, bezt, 0, screen_co);
						}
						if (ED_view3d_project_float_object(vc->ar, bezt->vec[1], screen_co,
						                                   V3D_PROJ_RET_CLIP_BB | V3D_PROJ_RET_CLIP_WIN) == V3D_PROJ_RET_OK)
						{
							func(userData, nu, NULL, bezt, 1, screen_co);
						}
						if (ED_view3d_project_float_object(vc->ar, bezt->vec[2], screen_co,
						                                   V3D_PROJ_RET_CLIP_BB | V3D_PROJ_RET_CLIP_WIN) == V3D_PROJ_RET_OK)
						{
							func(userData, nu, NULL, bezt, 2, screen_co);
						}
					}
				}
			}
		}
		else {
			for (i = 0; i < nu->pntsu * nu->pntsv; i++) {
				BPoint *bp = &nu->bp[i];

				if (bp->hide == 0) {
					float screen_co[2];
					if (ED_view3d_project_float_object(vc->ar, bp->vec, screen_co,
					                                   V3D_PROJ_RET_CLIP_BB | V3D_PROJ_RET_CLIP_WIN) == V3D_PROJ_RET_OK)
					{
						func(userData, nu, bp, NULL, -1, screen_co);
					}
				}
			}
		}
	}
}

/* ------------------------------------------------------------------------ */

/* ED_view3d_init_mats_rv3d must be called first */
void mball_foreachScreenElem(
        struct ViewContext *vc,
        void (*func)(void *userData, struct MetaElem *ml, const float screen_co_b[2]),
        void *userData, const eV3DProjTest clip_flag)
{
	MetaBall *mb = (MetaBall *)vc->obedit->data;
	MetaElem *ml;

	ED_view3d_check_mats_rv3d(vc->rv3d);

	for (ml = mb->editelems->first; ml; ml = ml->next) {
		float screen_co[2];
		if (ED_view3d_project_float_object(vc->ar, &ml->x, screen_co, clip_flag) == V3D_PROJ_RET_OK) {
			func(userData, ml, screen_co);
		}
	}
}

/* ------------------------------------------------------------------------ */

void lattice_foreachScreenVert(
        ViewContext *vc,
        void (*func)(void *userData, BPoint *bp, const float screen_co[2]),
        void *userData, const eV3DProjTest clip_flag)
{
	Object *obedit = vc->obedit;
	Lattice *lt = obedit->data;
	BPoint *bp = lt->editlatt->latt->def;
	DispList *dl = obedit->runtime.curve_cache ? BKE_displist_find(&obedit->runtime.curve_cache->disp, DL_VERTS) : NULL;
	const float *co = dl ? dl->verts : NULL;
	int i, N = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;

	ED_view3d_check_mats_rv3d(vc->rv3d);

	if (clip_flag & V3D_PROJ_TEST_CLIP_BB) {
		ED_view3d_clipping_local(vc->rv3d, obedit->obmat); /* for local clipping lookups */
	}

	for (i = 0; i < N; i++, bp++, co += 3) {
		if (bp->hide == 0) {
			float screen_co[2];
			if (ED_view3d_project_float_object(vc->ar, dl ? co : bp->vec, screen_co, clip_flag) == V3D_PROJ_RET_OK) {
				func(userData, bp, screen_co);
			}
		}
	}
}

/* ------------------------------------------------------------------------ */

/* ED_view3d_init_mats_rv3d must be called first */
void armature_foreachScreenBone(
        struct ViewContext *vc,
        void (*func)(void *userData, struct EditBone *ebone, const float screen_co_a[2], const float screen_co_b[2]),
        void *userData, const eV3DProjTest clip_flag)
{
	bArmature *arm = vc->obedit->data;
	EditBone *ebone;

	ED_view3d_check_mats_rv3d(vc->rv3d);

	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_VISIBLE(arm, ebone)) {
			float screen_co_a[2], screen_co_b[2];
			int points_proj_tot = 0;

			/* project head location to screenspace */
			if (ED_view3d_project_float_object(vc->ar, ebone->head, screen_co_a, clip_flag) == V3D_PROJ_RET_OK) {
				points_proj_tot++;
			}
			else {
				screen_co_a[0] = IS_CLIPPED;  /* weak */
				/* screen_co_a[1]: intentionally dont set this so we get errors on misuse */
			}

			/* project tail location to screenspace */
			if (ED_view3d_project_float_object(vc->ar, ebone->tail, screen_co_b, clip_flag) == V3D_PROJ_RET_OK) {
				points_proj_tot++;
			}
			else {
				screen_co_b[0] = IS_CLIPPED;  /* weak */
				/* screen_co_b[1]: intentionally dont set this so we get errors on misuse */
			}

			if (points_proj_tot) {  /* at least one point's projection worked */
				func(userData, ebone, screen_co_a, screen_co_b);
			}
		}
	}
}

/* ------------------------------------------------------------------------ */

/* ED_view3d_init_mats_rv3d must be called first */
/* almost _exact_ copy of #armature_foreachScreenBone */
void pose_foreachScreenBone(
        struct ViewContext *vc,
        void (*func)(void *userData, struct bPoseChannel *pchan, const float screen_co_a[2], const float screen_co_b[2]),
        void *userData, const eV3DProjTest clip_flag)
{
	const Object *ob_eval = DEG_get_evaluated_object(vc->depsgraph, vc->obact);
	const bArmature *arm_eval = ob_eval->data;
	bPose *pose = vc->obact->pose;
	bPoseChannel *pchan;

	ED_view3d_check_mats_rv3d(vc->rv3d);

	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		if (PBONE_VISIBLE(arm_eval, pchan->bone)) {
			bPoseChannel *pchan_eval = BKE_pose_channel_find_name(ob_eval->pose, pchan->name);
			float screen_co_a[2], screen_co_b[2];
			int points_proj_tot = 0;

			/* project head location to screenspace */
			if (ED_view3d_project_float_object(vc->ar, pchan_eval->pose_head, screen_co_a, clip_flag) == V3D_PROJ_RET_OK) {
				points_proj_tot++;
			}
			else {
				screen_co_a[0] = IS_CLIPPED;  /* weak */
				/* screen_co_a[1]: intentionally dont set this so we get errors on misuse */
			}

			/* project tail location to screenspace */
			if (ED_view3d_project_float_object(vc->ar, pchan_eval->pose_tail, screen_co_b, clip_flag) == V3D_PROJ_RET_OK) {
				points_proj_tot++;
			}
			else {
				screen_co_b[0] = IS_CLIPPED;  /* weak */
				/* screen_co_b[1]: intentionally dont set this so we get errors on misuse */
			}

			if (points_proj_tot) {  /* at least one point's projection worked */
				func(userData, pchan, screen_co_a, screen_co_b);
			}
		}
	}
}
