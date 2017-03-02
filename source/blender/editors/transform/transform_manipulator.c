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
 * The Original Code is Copyright (C) 2005 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform_manipulator.c
 *  \ingroup edtransform
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "RNA_access.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_editmesh.h"
#include "BKE_lattice.h"
#include "BKE_gpencil.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_particle.h"
#include "ED_view3d.h"
#include "ED_gpencil.h"

#include "UI_resources.h"

/* local module include */
#include "transform.h"

#include "GPU_select.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"

/* return codes for select, and drawing flags */

#define MAN_TRANS_X		(1 << 0)
#define MAN_TRANS_Y		(1 << 1)
#define MAN_TRANS_Z		(1 << 2)
#define MAN_TRANS_C		(MAN_TRANS_X | MAN_TRANS_Y | MAN_TRANS_Z)

#define MAN_ROT_X		(1 << 3)
#define MAN_ROT_Y		(1 << 4)
#define MAN_ROT_Z		(1 << 5)
#define MAN_ROT_V		(1 << 6)
#define MAN_ROT_T		(1 << 7)
#define MAN_ROT_C		(MAN_ROT_X | MAN_ROT_Y | MAN_ROT_Z | MAN_ROT_V | MAN_ROT_T)

#define MAN_SCALE_X		(1 << 8)
#define MAN_SCALE_Y		(1 << 9)
#define MAN_SCALE_Z		(1 << 10)
#define MAN_SCALE_C		(MAN_SCALE_X | MAN_SCALE_Y | MAN_SCALE_Z)

/* color codes */

#define MAN_RGB     0
#define MAN_GHOST   1
#define MAN_MOVECOL 2

/* threshold for testing view aligned manipulator axis */
#define TW_AXIS_DOT_MIN 0.02f
#define TW_AXIS_DOT_MAX 0.1f

/* transform widget center calc helper for below */
static void calc_tw_center(Scene *scene, const float co[3])
{
	float *twcent = scene->twcent;
	float *min = scene->twmin;
	float *max = scene->twmax;

	minmax_v3v3_v3(min, max, co);
	add_v3_v3(twcent, co);
}

static void protectflag_to_drawflags(short protectflag, short *drawflags)
{
	if (protectflag & OB_LOCK_LOCX)
		*drawflags &= ~MAN_TRANS_X;
	if (protectflag & OB_LOCK_LOCY)
		*drawflags &= ~MAN_TRANS_Y;
	if (protectflag & OB_LOCK_LOCZ)
		*drawflags &= ~MAN_TRANS_Z;

	if (protectflag & OB_LOCK_ROTX)
		*drawflags &= ~MAN_ROT_X;
	if (protectflag & OB_LOCK_ROTY)
		*drawflags &= ~MAN_ROT_Y;
	if (protectflag & OB_LOCK_ROTZ)
		*drawflags &= ~MAN_ROT_Z;

	if (protectflag & OB_LOCK_SCALEX)
		*drawflags &= ~MAN_SCALE_X;
	if (protectflag & OB_LOCK_SCALEY)
		*drawflags &= ~MAN_SCALE_Y;
	if (protectflag & OB_LOCK_SCALEZ)
		*drawflags &= ~MAN_SCALE_Z;
}

/* for pose mode */
static void stats_pose(Scene *scene, RegionView3D *rv3d, bPoseChannel *pchan)
{
	Bone *bone = pchan->bone;

	if (bone) {
		calc_tw_center(scene, pchan->pose_head);
		protectflag_to_drawflags(pchan->protectflag, &rv3d->twdrawflag);
	}
}

/* for editmode*/
static void stats_editbone(RegionView3D *rv3d, EditBone *ebo)
{
	if (ebo->flag & BONE_EDITMODE_LOCKED)
		protectflag_to_drawflags(OB_LOCK_LOC | OB_LOCK_ROT | OB_LOCK_SCALE, &rv3d->twdrawflag);
}

/* could move into BLI_math however this is only useful for display/editing purposes */
static void axis_angle_to_gimbal_axis(float gmat[3][3], const float axis[3], const float angle)
{
	/* X/Y are arbitrary axies, most importantly Z is the axis of rotation */

	float cross_vec[3];
	float quat[4];

	/* this is an un-scientific method to get a vector to cross with
	 * XYZ intentionally YZX */
	cross_vec[0] = axis[1];
	cross_vec[1] = axis[2];
	cross_vec[2] = axis[0];

	/* X-axis */
	cross_v3_v3v3(gmat[0], cross_vec, axis);
	normalize_v3(gmat[0]);
	axis_angle_to_quat(quat, axis, angle);
	mul_qt_v3(quat, gmat[0]);

	/* Y-axis */
	axis_angle_to_quat(quat, axis, M_PI_2);
	copy_v3_v3(gmat[1], gmat[0]);
	mul_qt_v3(quat, gmat[1]);

	/* Z-axis */
	copy_v3_v3(gmat[2], axis);

	normalize_m3(gmat);
}


static int test_rotmode_euler(short rotmode)
{
	return (ELEM(rotmode, ROT_MODE_AXISANGLE, ROT_MODE_QUAT)) ? 0 : 1;
}

bool gimbal_axis(Object *ob, float gmat[3][3])
{
	if (ob) {
		if (ob->mode & OB_MODE_POSE) {
			bPoseChannel *pchan = BKE_pose_channel_active(ob);

			if (pchan) {
				float mat[3][3], tmat[3][3], obmat[3][3];
				if (test_rotmode_euler(pchan->rotmode)) {
					eulO_to_gimbal_axis(mat, pchan->eul, pchan->rotmode);
				}
				else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
					axis_angle_to_gimbal_axis(mat, pchan->rotAxis, pchan->rotAngle);
				}
				else { /* quat */
					return 0;
				}


				/* apply bone transformation */
				mul_m3_m3m3(tmat, pchan->bone->bone_mat, mat);

				if (pchan->parent) {
					float parent_mat[3][3];

					copy_m3_m4(parent_mat, pchan->parent->pose_mat);
					mul_m3_m3m3(mat, parent_mat, tmat);

					/* needed if object transformation isn't identity */
					copy_m3_m4(obmat, ob->obmat);
					mul_m3_m3m3(gmat, obmat, mat);
				}
				else {
					/* needed if object transformation isn't identity */
					copy_m3_m4(obmat, ob->obmat);
					mul_m3_m3m3(gmat, obmat, tmat);
				}

				normalize_m3(gmat);
				return 1;
			}
		}
		else {
			if (test_rotmode_euler(ob->rotmode)) {
				eulO_to_gimbal_axis(gmat, ob->rot, ob->rotmode);
			}
			else if (ob->rotmode == ROT_MODE_AXISANGLE) {
				axis_angle_to_gimbal_axis(gmat, ob->rotAxis, ob->rotAngle);
			}
			else { /* quat */
				return 0;
			}

			if (ob->parent) {
				float parent_mat[3][3];
				copy_m3_m4(parent_mat, ob->parent->obmat);
				normalize_m3(parent_mat);
				mul_m3_m3m3(gmat, parent_mat, gmat);
			}
			return 1;
		}
	}

	return 0;
}


/* centroid, boundbox, of selection */
/* returns total items selected */
static int calc_manipulator_stats(const bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	SceneLayer *sl = CTX_data_scene_layer(C);
	Object *obedit = CTX_data_edit_object(C);
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;
	Base *base;
	Object *ob = OBACT_NEW;
	bGPdata *gpd = CTX_data_gpencil_data(C);
	const bool is_gp_edit = ((gpd) && (gpd->flag & GP_DATA_STROKE_EDITMODE));
	int a, totsel = 0;

	/* transform widget matrix */
	unit_m4(rv3d->twmat);

	rv3d->twdrawflag = 0xFFFF;

	/* transform widget centroid/center */
	INIT_MINMAX(scene->twmin, scene->twmax);
	zero_v3(scene->twcent);
	
	if (is_gp_edit) {
		float diff_mat[4][4];
		float fpt[3];

		for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			/* only editable and visible layers are considered */
			if (gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {

				/* calculate difference matrix if parent object */
				if (gpl->parent != NULL) {
					ED_gpencil_parent_location(gpl, diff_mat);
				}

				for (bGPDstroke *gps = gpl->actframe->strokes.first; gps; gps = gps->next) {
					/* skip strokes that are invalid for current view */
					if (ED_gpencil_stroke_can_use(C, gps) == false) {
						continue;
					}

					/* we're only interested in selected points here... */
					if (gps->flag & GP_STROKE_SELECT) {
						bGPDspoint *pt;
						int i;

						/* Change selection status of all points, then make the stroke match */
						for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
							if (pt->flag & GP_SPOINT_SELECT) {
								if (gpl->parent == NULL) {
									calc_tw_center(scene, &pt->x);
									totsel++;
								}
								else {
									mul_v3_m4v3(fpt, diff_mat, &pt->x);
									calc_tw_center(scene, fpt);
									totsel++;
								}
							}
						}
					}
				}
			}
		}


		/* selection center */
		if (totsel) {
			mul_v3_fl(scene->twcent, 1.0f / (float)totsel);   /* centroid! */
		}
	}
	else if (obedit) {
		ob = obedit;
		if ((ob->lay & v3d->lay) == 0) return 0;

		if (obedit->type == OB_MESH) {
			BMEditMesh *em = BKE_editmesh_from_object(obedit);
			BMEditSelection ese;
			float vec[3] = {0, 0, 0};

			/* USE LAST SELECTE WITH ACTIVE */
			if ((v3d->around == V3D_AROUND_ACTIVE) && BM_select_history_active_get(em->bm, &ese)) {
				BM_editselection_center(&ese, vec);
				calc_tw_center(scene, vec);
				totsel = 1;
			}
			else {
				BMesh *bm = em->bm;
				BMVert *eve;

				BMIter iter;

				BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
					if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
							totsel++;
							calc_tw_center(scene, eve->co);
						}
					}
				}
			}
		} /* end editmesh */
		else if (obedit->type == OB_ARMATURE) {
			bArmature *arm = obedit->data;
			EditBone *ebo;

			if ((v3d->around == V3D_AROUND_ACTIVE) && (ebo = arm->act_edbone)) {
				/* doesn't check selection or visibility intentionally */
				if (ebo->flag & BONE_TIPSEL) {
					calc_tw_center(scene, ebo->tail);
					totsel++;
				}
				if ((ebo->flag & BONE_ROOTSEL) ||
				    ((ebo->flag & BONE_TIPSEL) == false))  /* ensure we get at least one point */
				{
					calc_tw_center(scene, ebo->head);
					totsel++;
				}
				stats_editbone(rv3d, ebo);
			}
			else {
				for (ebo = arm->edbo->first; ebo; ebo = ebo->next) {
					if (EBONE_VISIBLE(arm, ebo)) {
						if (ebo->flag & BONE_TIPSEL) {
							calc_tw_center(scene, ebo->tail);
							totsel++;
						}
						if ((ebo->flag & BONE_ROOTSEL) &&
						    /* don't include same point multiple times */
						    ((ebo->flag & BONE_CONNECTED) &&
						     (ebo->parent != NULL) &&
						     (ebo->parent->flag & BONE_TIPSEL) &&
						     EBONE_VISIBLE(arm, ebo->parent)) == 0)
						{
							calc_tw_center(scene, ebo->head);
							totsel++;
						}
						if (ebo->flag & BONE_SELECTED) {
							stats_editbone(rv3d, ebo);
						}
					}
				}
			}
		}
		else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
			Curve *cu = obedit->data;
			float center[3];

			if (v3d->around == V3D_AROUND_ACTIVE && ED_curve_active_center(cu, center)) {
				calc_tw_center(scene, center);
				totsel++;
			}
			else {
				Nurb *nu;
				BezTriple *bezt;
				BPoint *bp;
				ListBase *nurbs = BKE_curve_editNurbs_get(cu);

				nu = nurbs->first;
				while (nu) {
					if (nu->type == CU_BEZIER) {
						bezt = nu->bezt;
						a = nu->pntsu;
						while (a--) {
							/* exceptions
							 * if handles are hidden then only check the center points.
							 * If the center knot is selected then only use this as the center point.
							 */
							if (cu->drawflag & CU_HIDE_HANDLES) {
								if (bezt->f2 & SELECT) {
									calc_tw_center(scene, bezt->vec[1]);
									totsel++;
								}
							}
							else if (bezt->f2 & SELECT) {
								calc_tw_center(scene, bezt->vec[1]);
								totsel++;
							}
							else {
								if (bezt->f1 & SELECT) {
									calc_tw_center(scene, bezt->vec[(v3d->around == V3D_AROUND_LOCAL_ORIGINS) ? 1 : 0]);
									totsel++;
								}
								if (bezt->f3 & SELECT) {
									calc_tw_center(scene, bezt->vec[(v3d->around == V3D_AROUND_LOCAL_ORIGINS) ? 1 : 2]);
									totsel++;
								}
							}
							bezt++;
						}
					}
					else {
						bp = nu->bp;
						a = nu->pntsu * nu->pntsv;
						while (a--) {
							if (bp->f1 & SELECT) {
								calc_tw_center(scene, bp->vec);
								totsel++;
							}
							bp++;
						}
					}
					nu = nu->next;
				}
			}
		}
		else if (obedit->type == OB_MBALL) {
			MetaBall *mb = (MetaBall *)obedit->data;
			MetaElem *ml;

			if ((v3d->around == V3D_AROUND_ACTIVE) && (ml = mb->lastelem)) {
				calc_tw_center(scene, &ml->x);
				totsel++;
			}
			else {
				for (ml = mb->editelems->first; ml; ml = ml->next) {
					if (ml->flag & SELECT) {
						calc_tw_center(scene, &ml->x);
						totsel++;
					}
				}
			}
		}
		else if (obedit->type == OB_LATTICE) {
			Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
			BPoint *bp;

			if ((v3d->around == V3D_AROUND_ACTIVE) && (bp = BKE_lattice_active_point_get(lt))) {
				calc_tw_center(scene, bp->vec);
				totsel++;
			}
			else {
				bp = lt->def;
				a = lt->pntsu * lt->pntsv * lt->pntsw;
				while (a--) {
					if (bp->f1 & SELECT) {
						calc_tw_center(scene, bp->vec);
						totsel++;
					}
					bp++;
				}
			}
		}

		/* selection center */
		if (totsel) {
			mul_v3_fl(scene->twcent, 1.0f / (float)totsel);   // centroid!
			mul_m4_v3(obedit->obmat, scene->twcent);
			mul_m4_v3(obedit->obmat, scene->twmin);
			mul_m4_v3(obedit->obmat, scene->twmax);
		}
	}
	else if (ob && (ob->mode & OB_MODE_POSE)) {
		bPoseChannel *pchan;
		int mode = TFM_ROTATION; // mislead counting bones... bah. We don't know the manipulator mode, could be mixed
		bool ok = false;

		if ((ob->lay & v3d->lay) == 0) return 0;

		if ((v3d->around == V3D_AROUND_ACTIVE) && (pchan = BKE_pose_channel_active(ob))) {
			/* doesn't check selection or visibility intentionally */
			Bone *bone = pchan->bone;
			if (bone) {
				stats_pose(scene, rv3d, pchan);
				totsel = 1;
				ok = true;
			}
		}
		else {
			totsel = count_set_pose_transflags(&mode, 0, ob);

			if (totsel) {
				/* use channels to get stats */
				for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
					Bone *bone = pchan->bone;
					if (bone && (bone->flag & BONE_TRANSFORM)) {
						stats_pose(scene, rv3d, pchan);
					}
				}
				ok = true;
			}
		}

		if (ok) {
			mul_v3_fl(scene->twcent, 1.0f / (float)totsel);   // centroid!
			mul_m4_v3(ob->obmat, scene->twcent);
			mul_m4_v3(ob->obmat, scene->twmin);
			mul_m4_v3(ob->obmat, scene->twmax);
		}
	}
	else if (ob && (ob->mode & OB_MODE_ALL_PAINT)) {
		/* pass */
	}
	else if (ob && ob->mode & OB_MODE_PARTICLE_EDIT) {
		PTCacheEdit *edit = PE_get_current(scene, sl, ob);
		PTCacheEditPoint *point;
		PTCacheEditKey *ek;
		int k;

		if (edit) {
			point = edit->points;
			for (a = 0; a < edit->totpoint; a++, point++) {
				if (point->flag & PEP_HIDE) continue;

				for (k = 0, ek = point->keys; k < point->totkey; k++, ek++) {
					if (ek->flag & PEK_SELECT) {
						calc_tw_center(scene, (ek->flag & PEK_USE_WCO) ? ek->world_co : ek->co);
						totsel++;
					}
				}
			}

			/* selection center */
			if (totsel)
				mul_v3_fl(scene->twcent, 1.0f / (float)totsel);  // centroid!
		}
	}
	else {

		/* we need the one selected object, if its not active */
		base = BASACT_NEW;
		ob = OBACT_NEW;
		if (base && ((base->flag & BASE_SELECTED) == 0)) ob = NULL;

		for (base = sl->object_bases.first; base; base = base->next) {
			if (TESTBASELIB_NEW(base)) {
				if (ob == NULL)
					ob = base->object;
				calc_tw_center(scene, base->object->obmat[3]);
				protectflag_to_drawflags(base->object->protectflag, &rv3d->twdrawflag);
				totsel++;
			}
		}

		/* selection center */
		if (totsel) {
			mul_v3_fl(scene->twcent, 1.0f / (float)totsel);   // centroid!
		}
	}

	/* global, local or normal orientation? */
	if (ob && totsel && !is_gp_edit) {

		switch (v3d->twmode) {
		
			case V3D_MANIP_GLOBAL:
			{
				break; /* nothing to do */
			}
			case V3D_MANIP_GIMBAL:
			{
				float mat[3][3];
				if (gimbal_axis(ob, mat)) {
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				/* if not gimbal, fall through to normal */
				/* fall-through */
			}
			case V3D_MANIP_NORMAL:
			{
				if (obedit || ob->mode & OB_MODE_POSE) {
					float mat[3][3];
					ED_getTransformOrientationMatrix(C, mat, v3d->around);
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				/* no break we define 'normal' as 'local' in Object mode */
				/* fall-through */
			}
			case V3D_MANIP_LOCAL:
			{
				if (ob->mode & OB_MODE_POSE) {
					/* each bone moves on its own local axis, but  to avoid confusion,
					 * use the active pones axis for display [#33575], this works as expected on a single bone
					 * and users who select many bones will understand whats going on and what local means
					 * when they start transforming */
					float mat[3][3];
					ED_getTransformOrientationMatrix(C, mat, v3d->around);
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				copy_m4_m4(rv3d->twmat, ob->obmat);
				normalize_m4(rv3d->twmat);
				break;
			}
			case V3D_MANIP_VIEW:
			{
				float mat[3][3];
				copy_m3_m4(mat, rv3d->viewinv);
				normalize_m3(mat);
				copy_m4_m3(rv3d->twmat, mat);
				break;
			}
			default: /* V3D_MANIP_CUSTOM */
			{
				float mat[3][3];
				if (applyTransformOrientation(C, mat, NULL, v3d->twmode - V3D_MANIP_CUSTOM)) {
					copy_m4_m3(rv3d->twmat, mat);
				}
				break;
			}
		}

	}

	return totsel;
}

/* don't draw axis perpendicular to the view */
static void test_manipulator_axis(const bContext *C)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	float view_vec[3], axis_vec[3];
	float idot;
	int i;

	const int twdrawflag_axis[3] = {
	    (MAN_TRANS_X | MAN_SCALE_X),
	    (MAN_TRANS_Y | MAN_SCALE_Y),
	    (MAN_TRANS_Z | MAN_SCALE_Z)};

	ED_view3d_global_to_vector(rv3d, rv3d->twmat[3], view_vec);

	for (i = 0; i < 3; i++) {
		normalize_v3_v3(axis_vec, rv3d->twmat[i]);
		rv3d->tw_idot[i] = idot = 1.0f - fabsf(dot_v3v3(view_vec, axis_vec));
		if (idot < TW_AXIS_DOT_MIN) {
			rv3d->twdrawflag &= ~twdrawflag_axis[i];
		}
	}
}


/* ******************** DRAWING STUFFIES *********** */

static float screen_aligned(RegionView3D *rv3d, float mat[4][4])
{
	gpuTranslate3fv(mat[3]);

	/* sets view screen aligned */
	gpuRotate3f(-360.0f * saacos(rv3d->viewquat[0]) / (float)M_PI, rv3d->viewquat[1], rv3d->viewquat[2], rv3d->viewquat[3]);

	return len_v3(mat[0]); /* draw scale */
}


/* radring = radius of doughnut rings
 * radhole = radius hole
 * start = starting segment (based on nrings)
 * end   = end segment
 * nsides = amount of points in ring
 * nrigns = amount of rings
 */
static void partial_doughnut(unsigned int pos, float radring, float radhole, int start, int end, int nsides, int nrings)
{
	float theta, phi, theta1;
	float cos_theta, sin_theta;
	float cos_theta1, sin_theta1;
	float ring_delta, side_delta;
	int i, j, do_caps = true;

	if (start == 0 && end == nrings) do_caps = false;

	ring_delta = 2.0f * (float)M_PI / (float)nrings;
	side_delta = 2.0f * (float)M_PI / (float)nsides;

	theta = (float)M_PI + 0.5f * ring_delta;
	cos_theta = cosf(theta);
	sin_theta = sinf(theta);

	for (i = nrings - 1; i >= 0; i--) {
		theta1 = theta + ring_delta;
		cos_theta1 = cosf(theta1);
		sin_theta1 = sinf(theta1);

		if (do_caps && i == start) {  // cap
			immBegin(GL_TRIANGLE_FAN, nsides+1);
			phi = 0.0;
			for (j = nsides; j >= 0; j--) {
				float cos_phi, sin_phi, dist;

				phi += side_delta;
				cos_phi = cosf(phi);
				sin_phi = sinf(phi);
				dist = radhole + radring * cos_phi;

				immVertex3f(pos, cos_theta1 * dist, -sin_theta1 * dist,  radring * sin_phi);
			}
			immEnd();
		}
		if (i >= start && i <= end) {
			immBegin(GL_TRIANGLE_STRIP, (nsides+1) * 2);
			phi = 0.0;
			for (j = nsides; j >= 0; j--) {
				float cos_phi, sin_phi, dist;

				phi += side_delta;
				cos_phi = cosf(phi);
				sin_phi = sinf(phi);
				dist = radhole + radring * cos_phi;

				immVertex3f(pos, cos_theta1 * dist, -sin_theta1 * dist, radring * sin_phi);
				immVertex3f(pos, cos_theta * dist, -sin_theta * dist,  radring * sin_phi);
			}
			immEnd();
		}

		if (do_caps && i == end) {    // cap
			immBegin(GL_TRIANGLE_FAN, nsides+1);
			phi = 0.0;
			for (j = nsides; j >= 0; j--) {
				float cos_phi, sin_phi, dist;

				phi -= side_delta;
				cos_phi = cosf(phi);
				sin_phi = sinf(phi);
				dist = radhole + radring * cos_phi;

				immVertex3f(pos, cos_theta * dist, -sin_theta * dist,  radring * sin_phi);
			}
			immEnd();
		}


		theta = theta1;
		cos_theta = cos_theta1;
		sin_theta = sin_theta1;
	}
}

static char axisBlendAngle(float idot)
{
	if (idot > TW_AXIS_DOT_MAX) {
		return 255;
	}
	else if (idot < TW_AXIS_DOT_MIN) {
		return 0;
	}
	else {
		return (char)(255.0f * (idot - TW_AXIS_DOT_MIN) / (TW_AXIS_DOT_MAX - TW_AXIS_DOT_MIN));
	}
}

/* three colors can be set:
 * gray for ghosting
 * moving: in transform theme color
 * else the red/green/blue
 */
static void manipulator_setcolor(View3D *v3d, char axis, int colcode, unsigned char alpha)
{
	unsigned char col[4] = {0};
	col[3] = alpha;

	if (colcode == MAN_GHOST) {
		col[3] = 70;
	}
	else if (colcode == MAN_MOVECOL) {
		UI_GetThemeColor3ubv(TH_TRANSFORM, col);
	}
	else {
		switch (axis) {
			case 'C':
				UI_GetThemeColor3ubv(TH_TRANSFORM, col);
				if (v3d->twmode == V3D_MANIP_LOCAL) {
					col[0] = col[0] > 200 ? 255 : col[0] + 55;
					col[1] = col[1] > 200 ? 255 : col[1] + 55;
					col[2] = col[2] > 200 ? 255 : col[2] + 55;
				}
				else if (v3d->twmode == V3D_MANIP_NORMAL) {
					col[0] = col[0] < 55 ? 0 : col[0] - 55;
					col[1] = col[1] < 55 ? 0 : col[1] - 55;
					col[2] = col[2] < 55 ? 0 : col[2] - 55;
				}
				break;
			case 'X':
				UI_GetThemeColor3ubv(TH_AXIS_X, col);
				break;
			case 'Y':
				UI_GetThemeColor3ubv(TH_AXIS_Y, col);
				break;
			case 'Z':
				UI_GetThemeColor3ubv(TH_AXIS_Z, col);
				break;
			default:
				BLI_assert(0);
				break;
		}
	}

	immUniformColor4ubv(col);
}

static void manipulator_axis_order(RegionView3D *rv3d, int r_axis_order[3])
{
	float axis_values[3];
	float vec[3];

	ED_view3d_global_to_vector(rv3d, rv3d->twmat[3], vec);

	axis_values[0] = -dot_v3v3(rv3d->twmat[0], vec);
	axis_values[1] = -dot_v3v3(rv3d->twmat[1], vec);
	axis_values[2] = -dot_v3v3(rv3d->twmat[2], vec);

	axis_sort_v3(axis_values, r_axis_order);
}

/* viewmatrix should have been set OK, also no shademode! */
static void draw_manipulator_axes_single(View3D *v3d, RegionView3D *rv3d, int colcode,
                                         int flagx, int flagy, int flagz, int axis,
                                         const bool is_picksel, unsigned int pos)
{
	switch (axis) {
		case 0:
			/* axes */
			if (flagx) {
				if (is_picksel) {
					if      (flagx & MAN_SCALE_X) GPU_select_load_id(MAN_SCALE_X);
					else if (flagx & MAN_TRANS_X) GPU_select_load_id(MAN_TRANS_X);
				}
				else {
					manipulator_setcolor(v3d, 'X', colcode, axisBlendAngle(rv3d->tw_idot[0]));
				}
				immBegin(GL_LINES, 2);
				immVertex3f(pos, 0.2f, 0.0f, 0.0f);
				immVertex3f(pos, 1.0f, 0.0f, 0.0f);
				immEnd();
			}
			break;
		case 1:
			if (flagy) {
				if (is_picksel) {
					if      (flagy & MAN_SCALE_Y) GPU_select_load_id(MAN_SCALE_Y);
					else if (flagy & MAN_TRANS_Y) GPU_select_load_id(MAN_TRANS_Y);
				}
				else {
					manipulator_setcolor(v3d, 'Y', colcode, axisBlendAngle(rv3d->tw_idot[1]));
				}
				immBegin(GL_LINES, 2);
				immVertex3f(pos, 0.0f, 0.2f, 0.0f);
				immVertex3f(pos, 0.0f, 1.0f, 0.0f);
				immEnd();
			}
			break;
		case 2:
			if (flagz) {
				if (is_picksel) {
					if      (flagz & MAN_SCALE_Z) GPU_select_load_id(MAN_SCALE_Z);
					else if (flagz & MAN_TRANS_Z) GPU_select_load_id(MAN_TRANS_Z);
				}
				else {
					manipulator_setcolor(v3d, 'Z', colcode, axisBlendAngle(rv3d->tw_idot[2]));
				}
				immBegin(GL_LINES, 2);
				immVertex3f(pos, 0.0f, 0.0f, 0.2f);
				immVertex3f(pos, 0.0f, 0.0f, 1.0f);
				immEnd();
			}
			break;
	}
}
static void draw_manipulator_axes(View3D *v3d, RegionView3D *rv3d, int colcode,
                                  int flagx, int flagy, int flagz,
                                  const int axis_order[3], const bool is_picksel, unsigned int pos)
{
	int i;
	for (i = 0; i < 3; i++) {
		draw_manipulator_axes_single(v3d, rv3d, colcode, flagx, flagy, flagz, axis_order[i], is_picksel, pos);
	}
}

static void preOrthoFront(const bool ortho, float twmat[4][4], int axis)
{
	if (ortho == false) {
		float omat[4][4];
		copy_m4_m4(omat, twmat);
		orthogonalize_m4(omat, axis);
		gpuPushMatrix();
		gpuMultMatrix3D(omat);
		glFrontFace(is_negative_m4(omat) ? GL_CW : GL_CCW);
	}
}

static void postOrtho(const bool ortho)
{
	if (ortho == false) {
		gpuPopMatrix();
	}
}

BLI_INLINE bool manipulator_rotate_is_visible(const int drawflags)
{
	return (drawflags & (MAN_ROT_X | MAN_ROT_Y | MAN_ROT_Z));
}

static void draw_manipulator_rotate(
        View3D *v3d, RegionView3D *rv3d, const int drawflags, const int combo,
        const bool is_moving, const bool is_picksel)
{
	double plane[4];
	float matt[4][4];
	float size, unitmat[4][4];
	float cywid = 0.33f * 0.01f * (float)U.tw_handlesize;
	float cusize = cywid * 0.65f;
	int arcs = (G.debug_value != 2);
	const int colcode = (is_moving) ? MAN_MOVECOL : MAN_RGB;
	bool ortho;

	/* skip drawing if all axes are locked */
	if (manipulator_rotate_is_visible(drawflags) == false) return;

	/* Init stuff */
	glDisable(GL_DEPTH_TEST);
	unit_m4(unitmat);


	/* prepare for screen aligned draw */
	size = len_v3(rv3d->twmat[0]);
	gpuMatrixBegin3D_legacy();
	gpuPushMatrix();
	gpuTranslate3fv(rv3d->twmat[3]);


	const unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);


	if (arcs) {
		/* clipplane makes nice handles, calc here because of multmatrix but with translate! */
		copy_v3db_v3fl(plane, rv3d->viewinv[2]);
		plane[3] = -0.02f * size; // clip just a bit more
		glClipPlane(GL_CLIP_PLANE0, plane);
	}
	/* sets view screen aligned */
	gpuRotate3f(-360.0f * saacos(rv3d->viewquat[0]) / (float)M_PI, rv3d->viewquat[1], rv3d->viewquat[2], rv3d->viewquat[3]);

	/* Screen aligned help circle */
	if (arcs) {
		if (is_picksel == false) {
			immUniformThemeColorShade(TH_BACK, -30);
			imm_drawcircball(unitmat[3], size, unitmat, pos);
		}
	}

	/* Screen aligned trackball rot circle */
	if (drawflags & MAN_ROT_T) {
		if (is_picksel) GPU_select_load_id(MAN_ROT_T);
		else immUniformThemeColor(TH_TRANSFORM);

		imm_drawcircball(unitmat[3], 0.2f * size, unitmat, pos);
	}

	/* Screen aligned view rot circle */
	if (drawflags & MAN_ROT_V) {
		if (is_picksel) GPU_select_load_id(MAN_ROT_V);
		else immUniformThemeColor(TH_TRANSFORM);
		imm_drawcircball(unitmat[3], 1.2f * size, unitmat, pos);

		if (is_moving) {
			float vec[3];
			vec[0] = 0; // XXX (float)(t->mouse.imval[0] - t->center2d[0]);
			vec[1] = 0; // XXX (float)(t->mouse.imval[1] - t->center2d[1]);
			vec[2] = 0.0f;
			normalize_v3_length(vec, 1.2f * size);
			immBegin(GL_LINES, 2);
			immVertex3f(pos, 0.0f, 0.0f, 0.0f);
			immVertex3fv(pos,vec);
			immEnd();
		}
	}
	gpuPopMatrix();

	gpuPushMatrix();

	ortho = is_orthogonal_m4(rv3d->twmat);
	
	/* apply the transform delta */
	if (is_moving) {
		copy_m4_m4(matt, rv3d->twmat); // to copy the parts outside of [3][3]
		// XXX mul_m4_m3m4(matt, t->mat, rv3d->twmat);
		if (ortho) {
			gpuMultMatrix3D(matt);
			glFrontFace(is_negative_m4(matt) ? GL_CW : GL_CCW);
		}
	}
	else {
		if (ortho) {
			glFrontFace(is_negative_m4(rv3d->twmat) ? GL_CW : GL_CCW);
			gpuMultMatrix3D(rv3d->twmat);
		}
	}

	/* axes */
	if (arcs == 0) {
		if (!is_picksel) {
			if ((combo & V3D_MANIP_SCALE) == 0) {
				/* axis */
				if ((drawflags & MAN_ROT_X) || (is_moving && (drawflags & MAN_ROT_Z))) {
					preOrthoFront(ortho, rv3d->twmat, 2);
					manipulator_setcolor(v3d, 'X', colcode, 255);
					immBegin(GL_LINES, 2);
					immVertex3f(pos, 0.2f, 0.0f, 0.0f);
					immVertex3f(pos, 1.0f, 0.0f, 0.0f);
					immEnd();
					postOrtho(ortho);
				}
				if ((drawflags & MAN_ROT_Y) || (is_moving && (drawflags & MAN_ROT_X))) {
					preOrthoFront(ortho, rv3d->twmat, 0);
					manipulator_setcolor(v3d, 'Y', colcode, 255);
					immBegin(GL_LINES, 2);
					immVertex3f(pos, 0.0f, 0.2f, 0.0f);
					immVertex3f(pos, 0.0f, 1.0f, 0.0f);
					immEnd();
					postOrtho(ortho);
				}
				if ((drawflags & MAN_ROT_Z) || (is_moving && (drawflags & MAN_ROT_Y))) {
					preOrthoFront(ortho, rv3d->twmat, 1);
					manipulator_setcolor(v3d, 'Z', colcode, 255);
					immBegin(GL_LINES, 2);
					immVertex3f(pos, 0.0f, 0.0f, 0.2f);
					immVertex3f(pos, 0.0f, 0.0f, 1.0f);
					immEnd();
					postOrtho(ortho);
				}
			}
		}
	}

	if (arcs == 0 && is_moving) {

		/* Z circle */
		if (drawflags & MAN_ROT_Z) {
			preOrthoFront(ortho, matt, 2);
			if (is_picksel) GPU_select_load_id(MAN_ROT_Z);
			else manipulator_setcolor(v3d, 'Z', colcode, 255);
			imm_drawcircball(unitmat[3], 1.0, unitmat, pos);
			postOrtho(ortho);
		}
		/* X circle */
		if (drawflags & MAN_ROT_X) {
			preOrthoFront(ortho, matt, 0);
			if (is_picksel) GPU_select_load_id(MAN_ROT_X);
			else manipulator_setcolor(v3d, 'X', colcode, 255);
			gpuRotate3f(90.0, 0.0, 1.0, 0.0);
			imm_drawcircball(unitmat[3], 1.0, unitmat, pos);
			gpuRotate3f(-90.0, 0.0, 1.0, 0.0);
			postOrtho(ortho);
		}
		/* Y circle */
		if (drawflags & MAN_ROT_Y) {
			preOrthoFront(ortho, matt, 1);
			if (is_picksel) GPU_select_load_id(MAN_ROT_Y);
			else manipulator_setcolor(v3d, 'Y', colcode, 255);
			gpuRotate3f(-90.0, 1.0, 0.0, 0.0);
			imm_drawcircball(unitmat[3], 1.0, unitmat, pos);
			gpuRotate3f(90.0, 1.0, 0.0, 0.0);
			postOrtho(ortho);
		}
	}
	// donut arcs
	if (arcs) {
		glEnable(GL_CLIP_PLANE0);

		/* Z circle */
		if (drawflags & MAN_ROT_Z) {
			preOrthoFront(ortho, rv3d->twmat, 2);
			if (is_picksel) GPU_select_load_id(MAN_ROT_Z);
			else manipulator_setcolor(v3d, 'Z', colcode, 255);
			partial_doughnut(pos, cusize / 4.0f, 1.0f, 0, 48, 8, 48);
			postOrtho(ortho);
		}
		/* X circle */
		if (drawflags & MAN_ROT_X) {
			preOrthoFront(ortho, rv3d->twmat, 0);
			if (is_picksel) GPU_select_load_id(MAN_ROT_X);
			else manipulator_setcolor(v3d, 'X', colcode, 255);
			gpuRotate3f(90.0, 0.0, 1.0, 0.0);
			partial_doughnut(pos, cusize / 4.0f, 1.0f, 0, 48, 8, 48);
			gpuRotate3f(-90.0, 0.0, 1.0, 0.0);
			postOrtho(ortho);
		}
		/* Y circle */
		if (drawflags & MAN_ROT_Y) {
			preOrthoFront(ortho, rv3d->twmat, 1);
			if (is_picksel) GPU_select_load_id(MAN_ROT_Y);
			else manipulator_setcolor(v3d, 'Y', colcode, 255);
			gpuRotate3f(-90.0, 1.0, 0.0, 0.0);
			partial_doughnut(pos, cusize / 4.0f, 1.0f, 0, 48, 8, 48);
			gpuRotate3f(90.0, 1.0, 0.0, 0.0);
			postOrtho(ortho);
		}

		glDisable(GL_CLIP_PLANE0);
	}

	if (arcs == 0) {

		/* Z handle on X axis */
		if (drawflags & MAN_ROT_Z) {
			preOrthoFront(ortho, rv3d->twmat, 2);
			gpuPushMatrix();
			if (is_picksel) GPU_select_load_id(MAN_ROT_Z);
			else manipulator_setcolor(v3d, 'Z', colcode, 255);

			partial_doughnut(pos, 0.7f * cusize, 1.0f, 31, 33, 8, 64);

			gpuPopMatrix();
			postOrtho(ortho);
		}

		/* Y handle on X axis */
		if (drawflags & MAN_ROT_Y) {
			preOrthoFront(ortho, rv3d->twmat, 1);
			gpuPushMatrix();
			if (is_picksel) GPU_select_load_id(MAN_ROT_Y);
			else manipulator_setcolor(v3d, 'Y', colcode, 255);

			gpuRotate3f(90.0, 1.0, 0.0, 0.0);
			gpuRotate3f(90.0, 0.0, 0.0, 1.0);
			partial_doughnut(pos, 0.7f * cusize, 1.0f, 31, 33, 8, 64);

			gpuPopMatrix();
			postOrtho(ortho);
		}

		/* X handle on Z axis */
		if (drawflags & MAN_ROT_X) {
			preOrthoFront(ortho, rv3d->twmat, 0);
			gpuPushMatrix();
			if (is_picksel) GPU_select_load_id(MAN_ROT_X);
			else manipulator_setcolor(v3d, 'X', colcode, 255);

			gpuRotate3f(-90.0, 0.0, 1.0, 0.0);
			gpuRotate3f(90.0, 0.0, 0.0, 1.0);
			partial_doughnut(pos, 0.7f * cusize, 1.0f, 31, 33, 8, 64);

			gpuPopMatrix();
			postOrtho(ortho);
		}

	}

	/* restore */
	gpuPopMatrix();
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);

	immUnbindProgram();
	gpuMatrixEnd();
}

static void drawsolidcube(unsigned int pos, float size)
{
	const float cube[8][3] = {
		{-1.0, -1.0, -1.0},
		{-1.0, -1.0,  1.0},
		{-1.0,  1.0,  1.0},
		{-1.0,  1.0, -1.0},
		{ 1.0, -1.0, -1.0},
		{ 1.0, -1.0,  1.0},
		{ 1.0,  1.0,  1.0},
		{ 1.0,  1.0, -1.0},
	};

	gpuPushMatrix();
	gpuScaleUniform(size);

	immBegin(GL_TRIANGLES, 12 * 3);
	immVertex3fv(pos, cube[0]); immVertex3fv(pos, cube[1]); immVertex3fv(pos, cube[2]);
	immVertex3fv(pos, cube[0]); immVertex3fv(pos, cube[2]); immVertex3fv(pos, cube[3]);
	immVertex3fv(pos, cube[0]); immVertex3fv(pos, cube[4]); immVertex3fv(pos, cube[5]); 
	immVertex3fv(pos, cube[0]); immVertex3fv(pos, cube[5]); immVertex3fv(pos, cube[1]);
	immVertex3fv(pos, cube[4]); immVertex3fv(pos, cube[7]); immVertex3fv(pos, cube[6]);
	immVertex3fv(pos, cube[4]); immVertex3fv(pos, cube[6]); immVertex3fv(pos, cube[5]);
	immVertex3fv(pos, cube[7]); immVertex3fv(pos, cube[3]); immVertex3fv(pos, cube[2]);
	immVertex3fv(pos, cube[7]); immVertex3fv(pos, cube[2]); immVertex3fv(pos, cube[6]);
	immVertex3fv(pos, cube[1]); immVertex3fv(pos, cube[5]); immVertex3fv(pos, cube[6]);
	immVertex3fv(pos, cube[1]); immVertex3fv(pos, cube[6]); immVertex3fv(pos, cube[2]);
	immVertex3fv(pos, cube[7]); immVertex3fv(pos, cube[4]); immVertex3fv(pos, cube[0]);
	immVertex3fv(pos, cube[7]); immVertex3fv(pos, cube[0]); immVertex3fv(pos, cube[3]);
	immEnd();

	gpuPopMatrix();
}


static void draw_manipulator_scale(
        View3D *v3d, RegionView3D *rv3d, const int drawflags, const int combo, const int colcode,
        const bool is_moving, const bool is_picksel)
{
	float cywid = 0.25f * 0.01f * (float)U.tw_handlesize;
	float cusize = cywid * 0.75f, dz;
	int axis_order[3] = {2, 0, 1};
	int i;

	/* when called while moving in mixed mode, do not draw when... */
	if ((drawflags & MAN_SCALE_C) == 0) return;

	manipulator_axis_order(rv3d, axis_order);

	glDisable(GL_DEPTH_TEST);

	const unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	gpuMatrixBegin3D_legacy();
	gpuPushMatrix();

	/* not in combo mode */
	if ((combo & (V3D_MANIP_TRANSLATE | V3D_MANIP_ROTATE)) == 0) {
		float size, unitmat[4][4];
		int shift = 0; // XXX

		/* center circle, do not add to selection when shift is pressed (planar constraint)  */
		if (is_picksel && shift == 0) GPU_select_load_id(MAN_SCALE_C);
		else manipulator_setcolor(v3d, 'C', colcode, 255);

		gpuPushMatrix();
		size = screen_aligned(rv3d, rv3d->twmat);
		unit_m4(unitmat);
		imm_drawcircball(unitmat[3], 0.2f * size, unitmat, pos);
		gpuPopMatrix();

		dz = 1.0;
	}
	else {
		dz = 1.0f - 4.0f * cusize;
	}

	if (is_moving) {
		float matt[4][4];

		copy_m4_m4(matt, rv3d->twmat); // to copy the parts outside of [3][3]
		// XXX mul_m4_m3m4(matt, t->mat, rv3d->twmat);
		gpuMultMatrix3D(matt);
		glFrontFace(is_negative_m4(matt) ? GL_CW : GL_CCW);
	}
	else {
		gpuMultMatrix3D(rv3d->twmat);
		glFrontFace(is_negative_m4(rv3d->twmat) ? GL_CW : GL_CCW);
	}

	/* axis */

	/* in combo mode, this is always drawn as first type */
	draw_manipulator_axes(v3d, rv3d, colcode,
	                      drawflags & MAN_SCALE_X, drawflags & MAN_SCALE_Y, drawflags & MAN_SCALE_Z,
	                      axis_order, is_picksel, pos);


	for (i = 0; i < 3; i++) {
		switch (axis_order[i]) {
			case 0: /* X cube */
				if (drawflags & MAN_SCALE_X) {
					gpuTranslate3f(dz, 0.0, 0.0);
					if (is_picksel) GPU_select_load_id(MAN_SCALE_X);
					else manipulator_setcolor(v3d, 'X', colcode, axisBlendAngle(rv3d->tw_idot[0]));
					drawsolidcube(pos, cusize);
					gpuTranslate3f(-dz, 0.0, 0.0);
				}
				break;
			case 1: /* Y cube */
				if (drawflags & MAN_SCALE_Y) {
					gpuTranslate3f(0.0, dz, 0.0);
					if (is_picksel) GPU_select_load_id(MAN_SCALE_Y);
					else manipulator_setcolor(v3d, 'Y', colcode, axisBlendAngle(rv3d->tw_idot[1]));
					drawsolidcube(pos, cusize);
					gpuTranslate3f(0.0, -dz, 0.0);
				}
				break;
			case 2: /* Z cube */
				if (drawflags & MAN_SCALE_Z) {
					gpuTranslate3f(0.0, 0.0, dz);
					if (is_picksel) GPU_select_load_id(MAN_SCALE_Z);
					else manipulator_setcolor(v3d, 'Z', colcode, axisBlendAngle(rv3d->tw_idot[2]));
					drawsolidcube(pos, cusize);
					gpuTranslate3f(0.0, 0.0, -dz);
				}
				break;
		}
	}

#if 0 // XXX
	/* if shiftkey, center point as last, for selectbuffer order */
	if (is_picksel) {
		int shift = 0; // XXX

		if (shift) {
			gpuTranslate3f(0.0, -dz, 0.0);
			GPU_select_load_id(MAN_SCALE_C);
			/* TODO: set glPointSize before drawing center point */
			immBegin(GL_POINTS, 1);
			immVertex3f(0.0, 0.0, 0.0);
			immEnd();
		}
	}
#endif

	/* restore */
	gpuPopMatrix();

	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	glFrontFace(GL_CCW);

	gpuMatrixEnd();
	immUnbindProgram();
}

#define NSEGMENTS 8
static void draw_cone(unsigned int pos, float len, float width)
{
	/* a ring of vertices in the XY plane */
	float p[NSEGMENTS][2];
	for (int i = 0; i < NSEGMENTS; ++i) {
		float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
		p[i][0] = width * cosf(angle);
		p[i][1] = width * sinf(angle);
	}

	float zbase = -0.5f * len;
	float ztop = 0.5f * len;

	/* cone sides */
	immBegin(GL_TRIANGLE_FAN, NSEGMENTS + 2);
	immVertex3f(pos, 0, 0, ztop);
	for (int i = 0; i < NSEGMENTS; ++i)
		immVertex3f(pos, p[i][0], p[i][1], zbase);
	immVertex3f(pos, p[0][0], p[0][1], zbase);
	immEnd();

	/* end cap */
	immBegin(GL_TRIANGLE_FAN, NSEGMENTS);
	for (int i = NSEGMENTS - 1; i >= 0; --i)
		immVertex3f(pos, p[i][0], p[i][1], zbase);
	immEnd();
}

static void draw_cylinder(unsigned int pos, float len, float width)
{
	width *= 0.8f;   // just for beauty

	/* a ring of vertices in the XY plane */
	float p[NSEGMENTS][2];
	for (int i = 0; i < NSEGMENTS; ++i) {
		float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
		p[i][0] = width * cosf(angle);
		p[i][1] = width * sinf(angle);
	}

	float zbase = -0.5f * len;
	float ztop = 0.5f * len;

	/* cylinder sides */
	immBegin(GL_TRIANGLE_STRIP, (NSEGMENTS + 1) * 2);
	for (int i = 0; i < NSEGMENTS; ++i) {
		immVertex3f(pos, p[i][0], p[i][1], zbase);
		immVertex3f(pos, p[i][0], p[i][1], ztop);
	}
	immVertex3f(pos, p[0][0], p[0][1], zbase);
	immVertex3f(pos, p[0][0], p[0][1], ztop);
	immEnd();

	/* end caps */
	immBegin(GL_TRIANGLE_FAN, NSEGMENTS);
	for (int i = NSEGMENTS - 1; i >= 0; --i)
		immVertex3f(pos, p[i][0], p[i][1], zbase);
	immEnd();
	immBegin(GL_TRIANGLE_FAN, NSEGMENTS);
	for (int i = 0; i < NSEGMENTS; ++i)
		immVertex3f(pos, p[i][0], p[i][1], ztop);
	immEnd();
}
#undef NSEGMENTS

static void draw_manipulator_translate(
        View3D *v3d, RegionView3D *rv3d, int drawflags, int combo, int colcode,
        const bool UNUSED(is_moving), const bool is_picksel)
{
	float cylen = 0.01f * (float)U.tw_handlesize;
	float cywid = 0.25f * cylen, dz, size;
	float unitmat[4][4];
	int shift = 0; // XXX
	int axis_order[3] = {0, 1, 2};
	int i;

	/* when called while moving in mixed mode, do not draw when... */
	if ((drawflags & MAN_TRANS_C) == 0) return;

	manipulator_axis_order(rv3d, axis_order);

	// XXX if (moving) gpuTranslate3fv(t->vec);
	glDisable(GL_DEPTH_TEST);

	const unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	gpuMatrixBegin3D_legacy();
	gpuPushMatrix();

	/* center circle, do not add to selection when shift is pressed (planar constraint) */
	if (is_picksel && shift == 0) GPU_select_load_id(MAN_TRANS_C);
	else manipulator_setcolor(v3d, 'C', colcode, 255);

	gpuPushMatrix();
	size = screen_aligned(rv3d, rv3d->twmat);
	unit_m4(unitmat);
	imm_drawcircball(unitmat[3], 0.2f * size, unitmat, pos);
	gpuPopMatrix();

	/* and now apply matrix, we move to local matrix drawing */
	gpuMultMatrix3D(rv3d->twmat);

	/* axis */
	GPU_select_load_id(-1);

	// translate drawn as last, only axis when no combo with scale, or for ghosting
	if ((combo & V3D_MANIP_SCALE) == 0 || colcode == MAN_GHOST) {
		draw_manipulator_axes(v3d, rv3d, colcode,
		                      drawflags & MAN_TRANS_X, drawflags & MAN_TRANS_Y, drawflags & MAN_TRANS_Z,
		                      axis_order, is_picksel, pos);
	}

	/* offset in combo mode, for rotate a bit more */
	if (combo & (V3D_MANIP_ROTATE)) dz = 1.0f + 2.0f * cylen;
	else if (combo & (V3D_MANIP_SCALE)) dz = 1.0f + 0.5f * cylen;
	else dz = 1.0f;

	for (i = 0; i < 3; i++) {
		switch (axis_order[i]) {
			case 0: /* Z Cone */
				if (drawflags & MAN_TRANS_Z) {
					gpuPushMatrix();
					gpuTranslate3f(0.0, 0.0, dz);
					if (is_picksel) GPU_select_load_id(MAN_TRANS_Z);
					else manipulator_setcolor(v3d, 'Z', colcode, axisBlendAngle(rv3d->tw_idot[2]));
					draw_cone(pos, cylen, cywid);
					gpuPopMatrix();
				}
				break;
			case 1: /* X Cone */
				if (drawflags & MAN_TRANS_X) {
					gpuPushMatrix();
					gpuTranslate3f(dz, 0.0, 0.0);
					if (is_picksel) GPU_select_load_id(MAN_TRANS_X);
					else manipulator_setcolor(v3d, 'X', colcode, axisBlendAngle(rv3d->tw_idot[0]));
					gpuRotate3f(90.0, 0.0, 1.0, 0.0);
					draw_cone(pos, cylen, cywid);
					gpuPopMatrix();
				}
				break;
			case 2: /* Y Cone */
				if (drawflags & MAN_TRANS_Y) {
					gpuPushMatrix();
					gpuTranslate3f(0.0, dz, 0.0);
					if (is_picksel) GPU_select_load_id(MAN_TRANS_Y);
					else manipulator_setcolor(v3d, 'Y', colcode, axisBlendAngle(rv3d->tw_idot[1]));
					gpuRotate3f(-90.0, 1.0, 0.0, 0.0);
					draw_cone(pos, cylen, cywid);
					gpuPopMatrix();
				}
				break;
		}
	}

	gpuPopMatrix();

	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);

	immUnbindProgram();
	gpuMatrixEnd();
}

static void draw_manipulator_rotate_cyl(
        View3D *v3d, RegionView3D *rv3d, int drawflags, const int combo, const int colcode,
        const bool is_moving, const bool is_picksel)
{
	float size;
	float cylen = 0.01f * (float)U.tw_handlesize;
	float cywid = 0.25f * cylen;
	int axis_order[3] = {2, 0, 1};
	int i;

	/* skip drawing if all axes are locked */
	if (manipulator_rotate_is_visible(drawflags) == false) return;

	manipulator_axis_order(rv3d, axis_order);

	const unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	gpuMatrixBegin3D_legacy();
	gpuPushMatrix();

	/* prepare for screen aligned draw */
	gpuPushMatrix();
	size = screen_aligned(rv3d, rv3d->twmat);

	glDisable(GL_DEPTH_TEST);

	/* Screen aligned view rot circle */
	if (drawflags & MAN_ROT_V) {
		float unitmat[4][4];

		unit_m4(unitmat);

		if (is_picksel) GPU_select_load_id(MAN_ROT_V);
		immUniformThemeColor(TH_TRANSFORM);
		imm_drawcircball(unitmat[3], 1.2f * size, unitmat, pos);

		if (is_moving) {
			float vec[3];
			vec[0] = 0; // XXX (float)(t->mouse.imval[0] - t->center2d[0]);
			vec[1] = 0; // XXX (float)(t->mouse.imval[1] - t->center2d[1]);
			vec[2] = 0.0f;
			normalize_v3_length(vec, 1.2f * size);
			immBegin(GL_LINES, 2);
			immVertex3f(pos, 0.0, 0.0, 0.0);
			immVertex3fv(pos, vec);
			immEnd();
		}
	}
	gpuPopMatrix();

	/* apply the transform delta */
	if (is_moving) {
		float matt[4][4];
		copy_m4_m4(matt, rv3d->twmat); // to copy the parts outside of [3][3]
		// XXX      if (t->flag & T_USES_MANIPULATOR) {
		// XXX          mul_m4_m3m4(matt, t->mat, rv3d->twmat);
		// XXX }
		gpuMultMatrix3D(matt);
	}
	else {
		gpuMultMatrix3D(rv3d->twmat);
	}

	glFrontFace(is_negative_m4(rv3d->twmat) ? GL_CW : GL_CCW);

	/* axis */
	if (is_picksel == false) {

		// only draw axis when combo didn't draw scale axes
		if ((combo & V3D_MANIP_SCALE) == 0) {
			draw_manipulator_axes(v3d, rv3d, colcode,
			                      drawflags & MAN_ROT_X, drawflags & MAN_ROT_Y, drawflags & MAN_ROT_Z,
			                      axis_order, is_picksel, pos);
		}
	}

	for (i = 0; i < 3; i++) {
		switch (axis_order[i]) {
			case 0: /* X cylinder */
				if (drawflags & MAN_ROT_X) {
					gpuPushMatrix();
					gpuTranslate3f(1.0, 0.0, 0.0);
					if (is_picksel) GPU_select_load_id(MAN_ROT_X);
					gpuRotate3f(90.0, 0.0, 1.0, 0.0);
					manipulator_setcolor(v3d, 'X', colcode, 255);
					draw_cylinder(pos, cylen, cywid);
					gpuPopMatrix();
				}
				break;
			case 1: /* Y cylinder */
				if (drawflags & MAN_ROT_Y) {
					gpuPushMatrix();
					gpuTranslate3f(0.0, 1.0, 0.0);
					if (is_picksel) GPU_select_load_id(MAN_ROT_Y);
					gpuRotate3f(-90.0, 1.0, 0.0, 0.0);
					manipulator_setcolor(v3d, 'Y', colcode, 255);
					draw_cylinder(pos, cylen, cywid);
					gpuPopMatrix();
				}
				break;
			case 2: /* Z cylinder */
				if (drawflags & MAN_ROT_Z) {
					gpuPushMatrix();
					gpuTranslate3f(0.0, 0.0, 1.0);
					if (is_picksel) GPU_select_load_id(MAN_ROT_Z);
					manipulator_setcolor(v3d, 'Z', colcode, 255);
					draw_cylinder(pos, cylen, cywid);
					gpuPopMatrix();
				}
				break;
		}
	}

	/* restore */
	gpuPopMatrix();

	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);

	gpuMatrixEnd();
	immUnbindProgram();
}


/* ********************************************* */

/* main call, does calc centers & orientation too */
static int drawflags = 0xFFFF;       // only for the calls below, belongs in scene...?

void BIF_draw_manipulator(const bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	SceneLayer *sl = CTX_data_scene_layer(C);
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;
	int totsel;

	const bool is_picksel = false;

	if (!(v3d->twflag & V3D_USE_MANIPULATOR)) return;

	if ((v3d->twtype & (V3D_MANIP_TRANSLATE | V3D_MANIP_ROTATE | V3D_MANIP_SCALE)) == 0) return;

	{
		v3d->twflag &= ~V3D_DRAW_MANIPULATOR;

		totsel = calc_manipulator_stats(C);
		if (totsel == 0) return;

		v3d->twflag |= V3D_DRAW_MANIPULATOR;

		/* now we can define center */
		switch (v3d->around) {
			case V3D_AROUND_CENTER_BOUNDS:
			case V3D_AROUND_ACTIVE:
			{
				bGPdata *gpd = CTX_data_gpencil_data(C);
				Object *ob = OBACT_NEW;

				if (((v3d->around == V3D_AROUND_ACTIVE) && (scene->obedit == NULL)) &&
				    ((gpd == NULL) || !(gpd->flag & GP_DATA_STROKE_EDITMODE)) &&
				    (ob && !(ob->mode & OB_MODE_POSE)))
				{
					copy_v3_v3(rv3d->twmat[3], ob->obmat[3]);
				}
				else {
					mid_v3_v3v3(rv3d->twmat[3], scene->twmin, scene->twmax);
				}
				break;
			}
			case V3D_AROUND_LOCAL_ORIGINS:
			case V3D_AROUND_CENTER_MEAN:
				copy_v3_v3(rv3d->twmat[3], scene->twcent);
				break;
			case V3D_AROUND_CURSOR:
				copy_v3_v3(rv3d->twmat[3], ED_view3d_cursor3d_get(scene, v3d));
				break;
		}

		mul_mat3_m4_fl(rv3d->twmat, ED_view3d_pixel_size(rv3d, rv3d->twmat[3]) * U.tw_size);
	}

	/* when looking through a selected camera, the manipulator can be at the
	 * exact same position as the view, skip so we don't break selection */
	if (fabsf(mat4_to_scale(rv3d->twmat)) < 1e-7f)
		return;

	test_manipulator_axis(C);
	drawflags = rv3d->twdrawflag;    /* set in calc_manipulator_stats */

	if (v3d->twflag & V3D_DRAW_MANIPULATOR) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glLineWidth(1.0f);

		if (v3d->twtype & V3D_MANIP_ROTATE) {
			if (G.debug_value == 3) {
				if (G.moving & (G_TRANSFORM_OBJ | G_TRANSFORM_EDIT))
					draw_manipulator_rotate_cyl(v3d, rv3d, drawflags, v3d->twtype, MAN_MOVECOL, true, is_picksel);
				else
					draw_manipulator_rotate_cyl(v3d, rv3d, drawflags, v3d->twtype, MAN_RGB, false, is_picksel);
			}
			else {
				draw_manipulator_rotate(v3d, rv3d, drawflags, v3d->twtype, false, is_picksel);
			}
		}
		if (v3d->twtype & V3D_MANIP_SCALE) {
			draw_manipulator_scale(v3d, rv3d, drawflags, v3d->twtype, MAN_RGB, false, is_picksel);
		}
		if (v3d->twtype & V3D_MANIP_TRANSLATE) {
			draw_manipulator_translate(v3d, rv3d, drawflags, v3d->twtype, MAN_RGB, false, is_picksel);
		}

		glDisable(GL_BLEND);
	}
}

static int manipulator_selectbuf(ScrArea *sa, ARegion *ar, const int mval[2], float hotspot)
{
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;
	rctf rect, selrect;
	GLuint buffer[64];      // max 4 items per select, so large enuf
	short hits;
	const bool is_picksel = true;
	const bool do_passes = GPU_select_query_check_active();

	/* XXX check a bit later on this... (ton) */
	extern void view3d_winmatrix_set(ARegion *ar, View3D *v3d, rctf *rect);

	/* when looking through a selected camera, the manipulator can be at the
	 * exact same position as the view, skip so we don't break selection */
	if (fabsf(mat4_to_scale(rv3d->twmat)) < 1e-7f)
		return 0;

	rect.xmin = mval[0] - hotspot;
	rect.xmax = mval[0] + hotspot;
	rect.ymin = mval[1] - hotspot;
	rect.ymax = mval[1] + hotspot;

	selrect = rect;

	view3d_winmatrix_set(ar, v3d, &rect);
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);

	if (do_passes)
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_NEAREST_FIRST_PASS, 0);
	else
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_ALL, 0);

	/* do the drawing */
	if (v3d->twtype & V3D_MANIP_ROTATE) {
		if (G.debug_value == 3) draw_manipulator_rotate_cyl(v3d, rv3d, MAN_ROT_C & rv3d->twdrawflag, v3d->twtype, MAN_RGB, false, is_picksel);
		else draw_manipulator_rotate(v3d, rv3d, MAN_ROT_C & rv3d->twdrawflag, v3d->twtype, false, is_picksel);
	}
	if (v3d->twtype & V3D_MANIP_SCALE)
		draw_manipulator_scale(v3d, rv3d, MAN_SCALE_C & rv3d->twdrawflag, v3d->twtype, MAN_RGB, false, is_picksel);
	if (v3d->twtype & V3D_MANIP_TRANSLATE)
		draw_manipulator_translate(v3d, rv3d, MAN_TRANS_C & rv3d->twdrawflag, v3d->twtype, MAN_RGB, false, is_picksel);

	hits = GPU_select_end();

	if (do_passes) {
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_NEAREST_SECOND_PASS, hits);

		/* do the drawing */
		if (v3d->twtype & V3D_MANIP_ROTATE) {
			if (G.debug_value == 3) draw_manipulator_rotate_cyl(v3d, rv3d, MAN_ROT_C & rv3d->twdrawflag, v3d->twtype, MAN_RGB, false, is_picksel);
			else draw_manipulator_rotate(v3d, rv3d, MAN_ROT_C & rv3d->twdrawflag, v3d->twtype, false, is_picksel);
		}
		if (v3d->twtype & V3D_MANIP_SCALE)
			draw_manipulator_scale(v3d, rv3d, MAN_SCALE_C & rv3d->twdrawflag, v3d->twtype, MAN_RGB, false, is_picksel);
		if (v3d->twtype & V3D_MANIP_TRANSLATE)
			draw_manipulator_translate(v3d, rv3d, MAN_TRANS_C & rv3d->twdrawflag, v3d->twtype, MAN_RGB, false, is_picksel);

		GPU_select_end();
	}

	view3d_winmatrix_set(ar, v3d, NULL);
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);

	if (hits == 1) return buffer[3];
	else if (hits > 1) {
		GLuint val, dep, mindep = 0, mindeprot = 0, minval = 0, minvalrot = 0;
		int a;

		/* we compare the hits in buffer, but value centers highest */
		/* we also store the rotation hits separate (because of arcs) and return hits on other widgets if there are */

		for (a = 0; a < hits; a++) {
			dep = buffer[4 * a + 1];
			val = buffer[4 * a + 3];

			if (val == MAN_TRANS_C) {
				return MAN_TRANS_C;
			}
			else if (val == MAN_SCALE_C) {
				return MAN_SCALE_C;
			}
			else {
				if (val & MAN_ROT_C) {
					if (minvalrot == 0 || dep < mindeprot) {
						mindeprot = dep;
						minvalrot = val;
					}
				}
				else {
					if (minval == 0 || dep < mindep) {
						mindep = dep;
						minval = val;
					}
				}
			}
		}

		if (minval)
			return minval;
		else
			return minvalrot;
	}
	return 0;
}


/* return 0; nothing happened */
int BIF_do_manipulator(bContext *C, const struct wmEvent *event, wmOperator *op)
{
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;
	ARegion *ar = CTX_wm_region(C);
	int constraint_axis[3] = {0, 0, 0};
	int val;
	const bool use_planar = RNA_boolean_get(op->ptr, "use_planar_constraint");

	if (!(v3d->twflag & V3D_USE_MANIPULATOR)) return 0;
	if (!(v3d->twflag & V3D_DRAW_MANIPULATOR)) return 0;

	/* Force orientation */
	RNA_enum_set(op->ptr, "constraint_orientation", v3d->twmode);

	// find the hotspots first test narrow hotspot
	val = manipulator_selectbuf(sa, ar, event->mval, 0.5f * (float)U.tw_hotspot);
	if (val) {

		// drawflags still global, for drawing call above
		drawflags = manipulator_selectbuf(sa, ar, event->mval, 0.2f * (float)U.tw_hotspot);
		if (drawflags == 0) drawflags = val;

		if (drawflags & MAN_TRANS_C) {
			switch (drawflags) {
				case MAN_TRANS_C:
					break;
				case MAN_TRANS_X:
					if (use_planar) {
						constraint_axis[1] = 1;
						constraint_axis[2] = 1;
					}
					else
						constraint_axis[0] = 1;
					break;
				case MAN_TRANS_Y:
					if (use_planar) {
						constraint_axis[0] = 1;
						constraint_axis[2] = 1;
					}
					else
						constraint_axis[1] = 1;
					break;
				case MAN_TRANS_Z:
					if (use_planar) {
						constraint_axis[0] = 1;
						constraint_axis[1] = 1;
					}
					else
						constraint_axis[2] = 1;
					break;
			}
			RNA_boolean_set_array(op->ptr, "constraint_axis", constraint_axis);
			WM_operator_name_call(C, "TRANSFORM_OT_translate", WM_OP_INVOKE_DEFAULT, op->ptr);
		}
		else if (drawflags & MAN_SCALE_C) {
			switch (drawflags) {
				case MAN_SCALE_X:
					if (use_planar) {
						constraint_axis[1] = 1;
						constraint_axis[2] = 1;
					}
					else
						constraint_axis[0] = 1;
					break;
				case MAN_SCALE_Y:
					if (use_planar) {
						constraint_axis[0] = 1;
						constraint_axis[2] = 1;
					}
					else
						constraint_axis[1] = 1;
					break;
				case MAN_SCALE_Z:
					if (use_planar) {
						constraint_axis[0] = 1;
						constraint_axis[1] = 1;
					}
					else
						constraint_axis[2] = 1;
					break;
			}
			RNA_boolean_set_array(op->ptr, "constraint_axis", constraint_axis);
			WM_operator_name_call(C, "TRANSFORM_OT_resize", WM_OP_INVOKE_DEFAULT, op->ptr);
		}
		else if (drawflags == MAN_ROT_T) { /* trackball need special case, init is different */
			/* Do not pass op->ptr!!! trackball has no "constraint" properties!
			 * See [#34621], it's a miracle it did not cause more problems!!! */
			/* However, we need to copy the "release_confirm" property, but only if defined, see T41112. */
			PointerRNA props_ptr;
			PropertyRNA *prop;
			wmOperatorType *ot = WM_operatortype_find("TRANSFORM_OT_trackball", true);
			WM_operator_properties_create_ptr(&props_ptr, ot);
			if ((prop = RNA_struct_find_property(op->ptr, "release_confirm")) && RNA_property_is_set(op->ptr, prop)) {
				RNA_property_boolean_set(&props_ptr, prop, RNA_property_boolean_get(op->ptr, prop));
			}
			WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr);
			WM_operator_properties_free(&props_ptr);
		}
		else if (drawflags & MAN_ROT_C) {
			switch (drawflags) {
				case MAN_ROT_X:
					constraint_axis[0] = 1;
					break;
				case MAN_ROT_Y:
					constraint_axis[1] = 1;
					break;
				case MAN_ROT_Z:
					constraint_axis[2] = 1;
					break;
			}
			RNA_boolean_set_array(op->ptr, "constraint_axis", constraint_axis);
			WM_operator_name_call(C, "TRANSFORM_OT_rotate", WM_OP_INVOKE_DEFAULT, op->ptr);
		}
	}
	/* after transform, restore drawflags */
	drawflags = 0xFFFF;

	return val;
}

