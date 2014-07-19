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
 * Contributor(s): Martin Poirier
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform_orientations.c
 *  \ingroup edtransform
 */


#include <string.h>
#include <stddef.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_curve.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_report.h"
#include "BKE_main.h"
#include "BKE_screen.h"

#include "BLF_translation.h"

#include "ED_armature.h"

#include "RNA_define.h"

#include "UI_interface.h"

#include "transform.h"

/* *********************** TransSpace ************************** */

void BIF_clearTransformOrientation(bContext *C)
{
	View3D *v3d = CTX_wm_view3d(C);

	ListBase *transform_spaces = &CTX_data_scene(C)->transform_spaces;
	BLI_freelistN(transform_spaces);
	
	// Need to loop over all view3d
	if (v3d && v3d->twmode >= V3D_MANIP_CUSTOM) {
		v3d->twmode = V3D_MANIP_GLOBAL; /* fallback to global	*/
	}
}

static TransformOrientation *findOrientationName(ListBase *lb, const char *name)
{
	return BLI_findstring(lb, name, offsetof(TransformOrientation, name));
}

static bool uniqueOrientationNameCheck(void *arg, const char *name)
{
	return findOrientationName((ListBase *)arg, name) != NULL;
}

static void uniqueOrientationName(ListBase *lb, char *name)
{
	BLI_uniquename_cb(uniqueOrientationNameCheck, lb, CTX_DATA_(BLF_I18NCONTEXT_ID_SCENE, "Space"), '.', name,
	                  sizeof(((TransformOrientation *)NULL)->name));
}

static TransformOrientation *createViewSpace(bContext *C, ReportList *UNUSED(reports),
                                             const char *name, const bool overwrite)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	float mat[3][3];

	if (!rv3d)
		return NULL;

	copy_m3_m4(mat, rv3d->viewinv);
	normalize_m3(mat);

	if (name[0] == 0) {
		View3D *v3d = CTX_wm_view3d(C);
		if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
			/* If an object is used as camera, then this space is the same as object space! */
			name = v3d->camera->id.name + 2;
		}
		else {
			name = "Custom View";
		}
	}

	return addMatrixSpace(C, mat, name, overwrite);
}

static TransformOrientation *createObjectSpace(bContext *C, ReportList *UNUSED(reports),
                                               const char *name, const bool overwrite)
{
	Base *base = CTX_data_active_base(C);
	Object *ob;
	float mat[3][3];

	if (base == NULL)
		return NULL;

	ob = base->object;

	copy_m3_m4(mat, ob->obmat);
	normalize_m3(mat);

	/* use object name if no name is given */
	if (name[0] == 0) {
		name = ob->id.name + 2;
	}

	return addMatrixSpace(C, mat, name, overwrite);
}

static TransformOrientation *createBoneSpace(bContext *C, ReportList *reports,
                                             const char *name, const bool overwrite)
{
	float mat[3][3];
	float normal[3], plane[3];

	getTransformOrientation(C, normal, plane, 0);

	if (createSpaceNormalTangent(mat, normal, plane) == 0) {
		BKE_reports_prepend(reports, "Cannot use zero-length bone");
		return NULL;
	}

	if (name[0] == 0) {
		name = "Bone";
	}

	return addMatrixSpace(C, mat, name, overwrite);
}

static TransformOrientation *createCurveSpace(bContext *C, ReportList *reports,
                                              const char *name, const bool overwrite)
{
	float mat[3][3];
	float normal[3], plane[3];

	getTransformOrientation(C, normal, plane, 0);

	if (createSpaceNormalTangent(mat, normal, plane) == 0) {
		BKE_reports_prepend(reports, "Cannot use zero-length curve");
		return NULL;
	}

	if (name[0] == 0) {
		name = "Curve";
	}

	return addMatrixSpace(C, mat, name, overwrite);
}


static TransformOrientation *createMeshSpace(bContext *C, ReportList *reports,
                                             const char *name, const bool overwrite)
{
	float mat[3][3];
	float normal[3], plane[3];
	int type;

	type = getTransformOrientation(C, normal, plane, 0);
	
	switch (type) {
		case ORIENTATION_VERT:
			if (createSpaceNormal(mat, normal) == 0) {
				BKE_reports_prepend(reports, "Cannot use vertex with zero-length normal");
				return NULL;
			}
	
			if (name[0] == 0) {
				name = "Vertex";
			}
			break;
		case ORIENTATION_EDGE:
			if (createSpaceNormalTangent(mat, normal, plane) == 0) {
				BKE_reports_prepend(reports, "Cannot use zero-length edge");
				return NULL;
			}
	
			if (name[0] == 0) {
				name = "Edge";
			}
			break;
		case ORIENTATION_FACE:
			if (createSpaceNormalTangent(mat, normal, plane) == 0) {
				BKE_reports_prepend(reports, "Cannot use zero-area face");
				return NULL;
			}
	
			if (name[0] == 0) {
				name = "Face";
			}
			break;
		default:
			return NULL;
	}

	return addMatrixSpace(C, mat, name, overwrite);
}

bool createSpaceNormal(float mat[3][3], const float normal[3])
{
	float tangent[3] = {0.0f, 0.0f, 1.0f};
	
	copy_v3_v3(mat[2], normal);
	if (normalize_v3(mat[2]) == 0.0f) {
		return false;  /* error return */
	}

	cross_v3_v3v3(mat[0], mat[2], tangent);
	if (is_zero_v3(mat[0])) {
		tangent[0] = 1.0f;
		tangent[1] = tangent[2] = 0.0f;
		cross_v3_v3v3(mat[0], tangent, mat[2]);
	}

	cross_v3_v3v3(mat[1], mat[2], mat[0]);

	normalize_m3(mat);
	
	return true;
}

/**
 * \note To recreate an orientation from the matrix:
 * - (plane  == mat[1])
 * - (normal == mat[2])
 */
bool createSpaceNormalTangent(float mat[3][3], const float normal[3], const float tangent[3])
{
	if (normalize_v3_v3(mat[2], normal) == 0.0f) {
		return false;  /* error return */
	}

	/* negate so we can use values from the matrix as input */
	negate_v3_v3(mat[1], tangent);
	/* preempt zero length tangent from causing trouble */
	if (is_zero_v3(mat[1])) {
		mat[1][2] = 1.0f;
	}

	cross_v3_v3v3(mat[0], mat[2], mat[1]);
	if (normalize_v3(mat[0]) == 0.0f) {
		return false;  /* error return */
	}
	
	cross_v3_v3v3(mat[1], mat[2], mat[0]);
	normalize_v3(mat[1]);

	/* final matrix must be normalized, do inline */
	// normalize_m3(mat);
	
	return true;
}

void BIF_createTransformOrientation(bContext *C, ReportList *reports,
                                    const char *name, const bool use_view,
                                    const bool activate, const bool overwrite)
{
	TransformOrientation *ts = NULL;

	if (use_view) {
		ts = createViewSpace(C, reports, name, overwrite);
	}
	else {
		Object *obedit = CTX_data_edit_object(C);
		Object *ob = CTX_data_active_object(C);
		if (obedit) {
			if (obedit->type == OB_MESH)
				ts = createMeshSpace(C, reports, name, overwrite);
			else if (obedit->type == OB_ARMATURE)
				ts = createBoneSpace(C, reports, name, overwrite);
			else if (obedit->type == OB_CURVE)
				ts = createCurveSpace(C, reports, name, overwrite);
		}
		else if (ob && (ob->mode & OB_MODE_POSE)) {
			ts = createBoneSpace(C, reports, name, overwrite);
		}
		else {
			ts = createObjectSpace(C, reports, name, overwrite);
		}
	}

	if (activate && ts != NULL) {
		BIF_selectTransformOrientation(C, ts);
	}
}

TransformOrientation *addMatrixSpace(bContext *C, float mat[3][3],
                                     const char *name, const bool overwrite)
{
	ListBase *transform_spaces = &CTX_data_scene(C)->transform_spaces;
	TransformOrientation *ts = NULL;
	char name_unique[sizeof(ts->name)];

	if (overwrite) {
		ts = findOrientationName(transform_spaces, name);
	}
	else {
		BLI_strncpy(name_unique, name, sizeof(name_unique));
		uniqueOrientationName(transform_spaces, name_unique);
		name = name_unique;
	}

	/* if not, create a new one */
	if (ts == NULL) {
		ts = MEM_callocN(sizeof(TransformOrientation), "UserTransSpace from matrix");
		BLI_addtail(transform_spaces, ts);
		BLI_strncpy(ts->name, name, sizeof(ts->name));
	}

	/* copy matrix into transform space */
	copy_m3_m3(ts->mat, mat);

	return ts;
}

void BIF_removeTransformOrientation(bContext *C, TransformOrientation *target)
{
	Scene *scene = CTX_data_scene(C);
	ListBase *transform_spaces = &scene->transform_spaces;
	const int i = BLI_findindex(transform_spaces, target);

	if (i != -1) {
		Main *bmain = CTX_data_main(C);
		BKE_screen_view3d_main_twmode_remove(&bmain->screen, scene, i);
		BLI_freelinkN(transform_spaces, target);
	}
}

void BIF_removeTransformOrientationIndex(bContext *C, int index)
{
	ListBase *transform_spaces = &CTX_data_scene(C)->transform_spaces;
	TransformOrientation *ts = BLI_findlink(transform_spaces, index);

	if (ts) {
		BIF_removeTransformOrientation(C, ts);
	}
}

void BIF_selectTransformOrientation(bContext *C, TransformOrientation *target)
{
	ListBase *transform_spaces = &CTX_data_scene(C)->transform_spaces;
	const int i = BLI_findindex(transform_spaces, target);

	if (i != -1) {
		View3D *v3d = CTX_wm_view3d(C);
		v3d->twmode = V3D_MANIP_CUSTOM + i;
	}
}

void BIF_selectTransformOrientationValue(bContext *C, int orientation)
{
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d) /* currently using generic poll */
		v3d->twmode = orientation;
}

int BIF_countTransformOrientation(const bContext *C)
{
	ListBase *transform_spaces = &CTX_data_scene(C)->transform_spaces;
	return BLI_countlist(transform_spaces);
}

bool applyTransformOrientation(const bContext *C, float mat[3][3], char *r_name)
{
	View3D *v3d = CTX_wm_view3d(C);
	int selected_index = (v3d->twmode - V3D_MANIP_CUSTOM);

	ListBase *transform_spaces = &CTX_data_scene(C)->transform_spaces;
	TransformOrientation *ts = BLI_findlink(transform_spaces, selected_index);

	BLI_assert(selected_index >= 0);

	if (ts) {
		if (r_name) {
			BLI_strncpy(r_name, ts->name, MAX_NAME);
		}

		copy_m3_m3(mat, ts->mat);
		return true;
	}
	else {
		/* invalid index, can happen sometimes */
		return false;
	}
}

static int count_bone_select(bArmature *arm, ListBase *lb, const bool do_it)
{
	Bone *bone;
	bool do_next;
	int total = 0;
	
	for (bone = lb->first; bone; bone = bone->next) {
		bone->flag &= ~BONE_TRANSFORM;
		do_next = do_it;
		if (do_it) {
			if (bone->layer & arm->layer) {
				if (bone->flag & BONE_SELECTED) {
					bone->flag |= BONE_TRANSFORM;
					total++;

					/* no transform on children if one parent bone is selected */
					do_next = false;
				}
			}
		}
		total += count_bone_select(arm, &bone->childbase, do_next);
	}
	
	return total;
}

void initTransformOrientation(bContext *C, TransInfo *t)
{
	View3D *v3d = CTX_wm_view3d(C);
	Object *ob = CTX_data_active_object(C);
	Object *obedit = CTX_data_active_object(C);

	switch (t->current_orientation) {
		case V3D_MANIP_GLOBAL:
			unit_m3(t->spacemtx);
			BLI_strncpy(t->spacename, IFACE_("global"), sizeof(t->spacename));
			break;

		case V3D_MANIP_GIMBAL:
			unit_m3(t->spacemtx);
			if (gimbal_axis(ob, t->spacemtx)) {
				BLI_strncpy(t->spacename, IFACE_("gimbal"), sizeof(t->spacename));
				break;
			}
			/* fall-through */  /* no gimbal fallthrough to normal */
		case V3D_MANIP_NORMAL:
			if (obedit || (ob && ob->mode & OB_MODE_POSE)) {
				BLI_strncpy(t->spacename, IFACE_("normal"), sizeof(t->spacename));
				ED_getTransformOrientationMatrix(C, t->spacemtx, (v3d->around == V3D_ACTIVE));
				break;
			}
			/* fall-through */  /* we define 'normal' as 'local' in Object mode */
		case V3D_MANIP_LOCAL:
			BLI_strncpy(t->spacename, IFACE_("local"), sizeof(t->spacename));
		
			if (ob) {
				copy_m3_m4(t->spacemtx, ob->obmat);
				normalize_m3(t->spacemtx);
			}
			else {
				unit_m3(t->spacemtx);
			}
		
			break;
		
		case V3D_MANIP_VIEW:
			if (t->ar->regiontype == RGN_TYPE_WINDOW) {
				RegionView3D *rv3d = t->ar->regiondata;
				float mat[3][3];

				BLI_strncpy(t->spacename, IFACE_("view"), sizeof(t->spacename));
				copy_m3_m4(mat, rv3d->viewinv);
				normalize_m3(mat);
				copy_m3_m3(t->spacemtx, mat);
			}
			else {
				unit_m3(t->spacemtx);
			}
			break;
		default: /* V3D_MANIP_CUSTOM */
			if (applyTransformOrientation(C, t->spacemtx, t->spacename)) {
				/* pass */
			}
			else {
				unit_m3(t->spacemtx);
			}
			break;
	}
}

int getTransformOrientation(const bContext *C, float normal[3], float plane[3], const bool activeOnly)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	Object *obedit = CTX_data_edit_object(C);
	Base *base;
	Object *ob = OBACT;
	int result = ORIENTATION_NONE;

	zero_v3(normal);
	zero_v3(plane);

	if (obedit) {
		float imat[3][3], mat[3][3];
		
		/* we need the transpose of the inverse for a normal... */
		copy_m3_m4(imat, ob->obmat);
		
		invert_m3_m3(mat, imat);
		transpose_m3(mat);

		ob = obedit;

		if (ob->type == OB_MESH) {
			BMEditMesh *em = BKE_editmesh_from_object(ob);
			BMVert *eve;
			BMEditSelection ese;
			float vec[3] = {0, 0, 0};
			
			/* USE LAST SELECTED WITH ACTIVE */
			if (activeOnly && BM_select_history_active_get(em->bm, &ese)) {
				BM_editselection_normal(&ese, normal);
				BM_editselection_plane(&ese, plane);
				
				switch (ese.htype) {
					case BM_VERT:
						result = ORIENTATION_VERT;
						break;
					case BM_EDGE:
						result = ORIENTATION_EDGE;
						break;
					case BM_FACE:
						result = ORIENTATION_FACE;
						break;
				}
			}
			else {
				if (em->bm->totfacesel >= 1) {
					BMFace *efa;
					BMIter iter;

					BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
						if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
							BM_face_calc_plane(efa, vec);
							add_v3_v3(normal, efa->no);
							add_v3_v3(plane, vec);
						}
					}
					
					result = ORIENTATION_FACE;
				}
				else if (em->bm->totvertsel == 3) {
					BMVert *v1 = NULL, *v2 = NULL, *v3 = NULL;
					BMIter iter;
					
					BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
							if (v1 == NULL) {
								v1 = eve; 
							}
							else if (v2 == NULL) {
								v2 = eve;
							}
							else {
								float no_test[3];

								float tan_a[3], tan_b[3], tan_c[3];
								float len_a, len_b, len_c;
								const float *tan_best;


								v3 = eve;
								sub_v3_v3v3(tan_a, v2->co, v1->co);
								sub_v3_v3v3(tan_b, v3->co, v2->co);
								sub_v3_v3v3(tan_c, v1->co, v3->co);
								cross_v3_v3v3(normal, tan_b, tan_a);

								/* check if the normal is pointing opposite to vert normals */
								no_test[0] = v1->no[0] + v2->no[0] + v3->no[0];
								no_test[1] = v1->no[1] + v2->no[1] + v3->no[1];
								no_test[2] = v1->no[2] + v2->no[2] + v3->no[2];
								if (dot_v3v3(no_test, normal) < 0.0f) {
									negate_v3(normal);
								}

								/* always give the plane to the 2 most distant verts */
								len_a = len_squared_v3(tan_a);
								len_b = len_squared_v3(tan_b);
								len_c = len_squared_v3(tan_c);

								tan_best = MAX3_PAIR(len_a, len_b, len_c,
								                     tan_a, tan_b, tan_c);

								copy_v3_v3(plane, tan_best);

								break;
							}
						}
					}

					/* if there's an edge available, use that for the tangent */
					if (em->bm->totedgesel >= 1) {
						BMEdge *eed = NULL;
						
						BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
							if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
								sub_v3_v3v3(plane, eed->v2->co, eed->v1->co);
								break;
							}
						}
					}

					result = ORIENTATION_FACE;
				}
				else if (em->bm->totedgesel == 1 || em->bm->totvertsel == 2) {
					BMVert *v1 = NULL, *v2 = NULL;
					BMIter iter;
					
					if (em->bm->totedgesel == 1) {
						BMEdge *eed = NULL;
						BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
							if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
								v1 = eed->v1;
								v2 = eed->v2;
							}
						}
					}
					else {
						BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
							if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
								if (v1 == NULL) {
									v1 = eve;
								}
								else {
									v2 = eve;
									break;
								}
							}
						}
					}

					/* should never fail */
					if (LIKELY(v1 && v2)) {
						/* Logic explained:
						 *
						 * - Edges and vert-pairs treated the same way.
						 * - Point the Z axis along the edge vector (towards the active vertex).
						 * - Point the Y axis outwards (the same direction as the normals).
						 *
						 * Note that this is at odds a little with face select (and 3 vertices)
						 * which point the Z axis along the normal, however in both cases Z is the dominant axis.
						 */

						/* be deterministic where possible and ensure v1 is active */
						if (BM_mesh_active_vert_get(em->bm) == v2) {
							SWAP(BMVert *, v1, v2);
						}

						add_v3_v3v3(plane, v1->no, v2->no);
						sub_v3_v3v3(normal, v1->co, v2->co);
						/* flip the plane normal so we point outwards */
						negate_v3(plane);
					}

					result = ORIENTATION_EDGE;
				}
				else if (em->bm->totvertsel == 1) {
					BMIter iter;

					BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
							copy_v3_v3(normal, eve->no);
							break;
						}
					}
					result = ORIENTATION_VERT;
				}
				else if (em->bm->totvertsel > 3) {
					BMIter iter;

					zero_v3(normal);

					BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
							add_v3_v3(normal, eve->no);
						}
					}
					normalize_v3(normal);
					result = ORIENTATION_VERT;
				}
			}

			/* not needed but this matches 2.68 and older behavior */
			negate_v3(plane);

		} /* end editmesh */
		else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
			Curve *cu = obedit->data;
			Nurb *nu = NULL;
			BezTriple *bezt = NULL;
			int a;
			ListBase *nurbs = BKE_curve_editNurbs_get(cu);

			if (activeOnly && BKE_curve_nurb_vert_active_get(cu, &nu, (void *)&bezt)) {
				if (nu->type == CU_BEZIER) {
					BKE_nurb_bezt_calc_normal(nu, bezt, normal);
					BKE_nurb_bezt_calc_plane(nu, bezt, plane);
				}
			}
			else {
				const bool use_handle = (cu->drawflag & CU_HIDE_HANDLES) == 0;

				for (nu = nurbs->first; nu; nu = nu->next) {
					/* only bezier has a normal */
					if (nu->type == CU_BEZIER) {
						bezt = nu->bezt;
						a = nu->pntsu;
						while (a--) {
							short flag = 0;

#define SEL_F1 (1 << 0)
#define SEL_F2 (1 << 1)
#define SEL_F3 (1 << 2)

							if (use_handle) {
								if (bezt->f1 & SELECT) flag |= SEL_F1;
								if (bezt->f2 & SELECT) flag |= SEL_F2;
								if (bezt->f3 & SELECT) flag |= SEL_F3;
							}
							else {
								flag = (bezt->f2 & SELECT) ? (SEL_F1 | SEL_F2 | SEL_F3) : 0;
							}

							/* exception */
							if (flag) {
								float tvec[3];
								if ((v3d->around == V3D_LOCAL) ||
								    ELEM(flag, SEL_F2, SEL_F1 | SEL_F3, SEL_F1 | SEL_F2 | SEL_F3))
								{
									BKE_nurb_bezt_calc_normal(nu, bezt, tvec);
									add_v3_v3(normal, tvec);
								}
								else {
									/* ignore bezt->f2 in this case */
									if (flag & SEL_F1) {
										sub_v3_v3v3(tvec, bezt->vec[0], bezt->vec[1]);
										normalize_v3(tvec);
										add_v3_v3(normal, tvec);
									}
									if (flag & SEL_F3) {
										sub_v3_v3v3(tvec, bezt->vec[1], bezt->vec[2]);
										normalize_v3(tvec);
										add_v3_v3(normal, tvec);
									}
								}

								BKE_nurb_bezt_calc_plane(nu, bezt, tvec);
								add_v3_v3(plane, tvec);
							}

#undef SEL_F1
#undef SEL_F2
#undef SEL_F3

							bezt++;
						}
					}
				}
			}
			
			if (!is_zero_v3(normal)) {
				result = ORIENTATION_FACE;
			}
		}
		else if (obedit->type == OB_MBALL) {
			MetaBall *mb = obedit->data;
			MetaElem *ml;
			bool ok = false;
			float tmat[3][3];
			
			if (activeOnly && (ml = mb->lastelem)) {
				quat_to_mat3(tmat, ml->quat);
				add_v3_v3(normal, tmat[2]);
				add_v3_v3(plane, tmat[1]);
				ok = true;
			}
			else {
				for (ml = mb->editelems->first; ml; ml = ml->next) {
					if (ml->flag & SELECT) {
						quat_to_mat3(tmat, ml->quat);
						add_v3_v3(normal, tmat[2]);
						add_v3_v3(plane, tmat[1]);
						ok = true;
					}
				}
			}

			if (ok) {
				if (!is_zero_v3(plane)) {
					result = ORIENTATION_FACE;
				}
			}
		}
		else if (obedit->type == OB_ARMATURE) {
			bArmature *arm = obedit->data;
			EditBone *ebone;
			bool ok = false;
			float tmat[3][3];

			if (activeOnly && (ebone = arm->act_edbone)) {
				ED_armature_ebone_to_mat3(ebone, tmat);
				add_v3_v3(normal, tmat[2]);
				add_v3_v3(plane, tmat[1]);
				ok = true;
			}
			else {
				for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
					if (arm->layer & ebone->layer) {
						if (ebone->flag & BONE_SELECTED) {
							ED_armature_ebone_to_mat3(ebone, tmat);
							add_v3_v3(normal, tmat[2]);
							add_v3_v3(plane, tmat[1]);
							ok = true;
						}
					}
				}
			}
			
			if (ok) {
				if (!is_zero_v3(plane)) {
					result = ORIENTATION_EDGE;
				}
			}
		}

		/* Vectors from edges don't need the special transpose inverse multiplication */
		if (result == ORIENTATION_EDGE) {
			mul_mat3_m4_v3(ob->obmat, normal);
			mul_mat3_m4_v3(ob->obmat, plane);
		}
		else {
			mul_m3_v3(mat, normal);
			mul_m3_v3(mat, plane);
		}
	}
	else if (ob && (ob->mode & OB_MODE_POSE)) {
		bArmature *arm = ob->data;
		bPoseChannel *pchan;
		float imat[3][3], mat[3][3];
		bool ok = false;

		if (activeOnly && (pchan = BKE_pose_channel_active(ob))) {
			add_v3_v3(normal, pchan->pose_mat[2]);
			add_v3_v3(plane, pchan->pose_mat[1]);
			ok = true;
		}
		else {
			int totsel;

			totsel = count_bone_select(arm, &arm->bonebase, true);
			if (totsel) {
				/* use channels to get stats */
				for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
					if (pchan->bone && pchan->bone->flag & BONE_TRANSFORM) {
						add_v3_v3(normal, pchan->pose_mat[2]);
						add_v3_v3(plane, pchan->pose_mat[1]);
					}
				}
				ok = true;
			}
		}

		/* use for both active & all */
		if (ok) {
			/* we need the transpose of the inverse for a normal... */
			copy_m3_m4(imat, ob->obmat);
			
			invert_m3_m3(mat, imat);
			transpose_m3(mat);
			mul_m3_v3(mat, normal);
			mul_m3_v3(mat, plane);
			
			result = ORIENTATION_EDGE;
		}
	}
	else if (ob && (ob->mode & (OB_MODE_ALL_PAINT | OB_MODE_PARTICLE_EDIT))) {
		/* pass */
	}
	else {
		/* we need the one selected object, if its not active */
		ob = OBACT;
		if (ob && (ob->flag & SELECT)) {
			/* pass */
		}
		else {
			/* first selected */
			ob = NULL;
			for (base = scene->base.first; base; base = base->next) {
				if (TESTBASELIB(v3d, base)) {
					ob = base->object;
					break;
				}
			}
		}
		
		if (ob) {
			copy_v3_v3(normal, ob->obmat[2]);
			copy_v3_v3(plane, ob->obmat[1]);
		}
		result = ORIENTATION_NORMAL;
	}
	
	return result;
}

void ED_getTransformOrientationMatrix(const bContext *C, float orientation_mat[3][3], const bool activeOnly)
{
	float normal[3] = {0.0, 0.0, 0.0};
	float plane[3] = {0.0, 0.0, 0.0};

	int type;

	type = getTransformOrientation(C, normal, plane, activeOnly);

	switch (type) {
		case ORIENTATION_NORMAL:
			if (createSpaceNormalTangent(orientation_mat, normal, plane) == 0) {
				type = ORIENTATION_NONE;
			}
			break;
		case ORIENTATION_VERT:
			if (createSpaceNormal(orientation_mat, normal) == 0) {
				type = ORIENTATION_NONE;
			}
			break;
		case ORIENTATION_EDGE:
			if (createSpaceNormalTangent(orientation_mat, normal, plane) == 0) {
				type = ORIENTATION_NONE;
			}
			break;
		case ORIENTATION_FACE:
			if (createSpaceNormalTangent(orientation_mat, normal, plane) == 0) {
				type = ORIENTATION_NONE;
			}
			break;
	}

	if (type == ORIENTATION_NONE) {
		unit_m3(orientation_mat);
	}
}
