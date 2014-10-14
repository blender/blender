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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_buttons.c
 *  \ingroup spview3d
 */


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLF_translation.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_screen.h"
#include "BKE_editmesh.h"
#include "BKE_deform.h"
#include "BKE_object.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_armature.h"
#include "ED_gpencil.h"
#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "view3d_intern.h"  /* own include */


/* ******************* view3d space & buttons ************** */
#define B_REDR              2
#define B_OBJECTPANELMEDIAN 1008

#define NBR_TRANSFORM_PROPERTIES 8

/* temporary struct for storing transform properties */
typedef struct {
	float ob_eul[4];   /* used for quat too... */
	float ob_scale[3]; /* need temp space due to linked values */
	float ob_dims[3];
	short link_scale;
	float ve_median[NBR_TRANSFORM_PROPERTIES];
} TransformProperties;

/* Helper function to compute a median changed value,
 * when the value should be clamped in [0.0, 1.0].
 * Returns either 0.0, 1.0 (both can be applied directly), a positive scale factor
 * for scale down, or a negative one for scale up.
 */
static float compute_scale_factor(const float ve_median, const float median)
{
	if (ve_median <= 0.0f)
		return 0.0f;
	else if (ve_median >= 1.0f)
		return 1.0f;
	else {
		/* Scale value to target median. */
		float median_new = ve_median;
		float median_orig = ve_median - median; /* Previous median value. */

		/* In case of floating point error. */
		CLAMP(median_orig, 0.0f, 1.0f);
		CLAMP(median_new, 0.0f, 1.0f);

		if (median_new <= median_orig) {
			/* Scale down. */
			return median_new / median_orig;
		}
		else {
			/* Scale up, negative to indicate it... */
			return -(1.0f - median_new) / (1.0f - median_orig);
		}
	}
}

/* is used for both read and write... */
static void v3d_editvertex_buts(uiLayout *layout, View3D *v3d, Object *ob, float lim)
{
/* Get rid of those ugly magic numbers, even in a single func they become confusing! */
/* Location, common to all. */
/* Next three *must* remain contiguous (used as array)! */
#define LOC_X        0
#define LOC_Y        1
#define LOC_Z        2
/* Meshes... */
#define M_BV_WEIGHT  3
/* Next two *must* remain contiguous (used as array)! */
#define M_SKIN_X     4
#define M_SKIN_Y     5
#define M_BE_WEIGHT  6
#define M_CREASE     7
/* Curves... */
#define C_BWEIGHT    3
#define C_WEIGHT     4
#define C_RADIUS     5
#define C_TILT       6
/*Lattice... */
#define L_WEIGHT     4

	uiBlock *block = (layout) ? uiLayoutAbsoluteBlock(layout) : NULL;
	TransformProperties *tfp;
	float median[NBR_TRANSFORM_PROPERTIES], ve_median[NBR_TRANSFORM_PROPERTIES];
	int tot, totedgedata, totcurvedata, totlattdata, totcurvebweight;
	bool has_meshdata = false;
	bool has_skinradius = false;
	PointerRNA data_ptr;

	fill_vn_fl(median, NBR_TRANSFORM_PROPERTIES, 0.0f);
	tot = totedgedata = totcurvedata = totlattdata = totcurvebweight = 0;

	/* make sure we got storage */
	if (v3d->properties_storage == NULL)
		v3d->properties_storage = MEM_callocN(sizeof(TransformProperties), "TransformProperties");
	tfp = v3d->properties_storage;

	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		BMEditMesh *em = me->edit_btmesh;
		BMesh *bm = em->bm;
		BMVert *eve;
		BMEdge *eed;
		BMIter iter;

		const int cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
		const int cd_vert_skin_offset    = CustomData_get_offset(&bm->vdata, CD_MVERT_SKIN);
		const int cd_edge_bweight_offset = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
		const int cd_edge_crease_offset  = CustomData_get_offset(&bm->edata, CD_CREASE);

		has_skinradius = (cd_vert_skin_offset != -1);

		if (bm->totvertsel) {
			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
					tot++;
					add_v3_v3(&median[LOC_X], eve->co);

					if (cd_vert_bweight_offset != -1) {
						median[M_BV_WEIGHT] += BM_ELEM_CD_GET_FLOAT(eve, cd_vert_bweight_offset);
					}

					if (has_skinradius) {
						MVertSkin *vs = BM_ELEM_CD_GET_VOID_P(eve, cd_vert_skin_offset);
						add_v2_v2(&median[M_SKIN_X], vs->radius); /* Third val not used currently. */
					}
				}
			}
		}

		if ((cd_edge_bweight_offset != -1) || (cd_edge_crease_offset  != -1)) {
			if (bm->totedgesel) {
				BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
					if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
						if (cd_edge_bweight_offset != -1) {
							median[M_BE_WEIGHT] += BM_ELEM_CD_GET_FLOAT(eed, cd_edge_bweight_offset);
						}

						if (cd_edge_crease_offset != -1) {
							median[M_CREASE] += BM_ELEM_CD_GET_FLOAT(eed, cd_edge_crease_offset);
						}

						totedgedata++;
					}
				}
			}
		}
		else {
			totedgedata = bm->totedgesel;
		}

		has_meshdata = (tot || totedgedata);
	}
	else if (ob->type == OB_CURVE || ob->type == OB_SURF) {
		Curve *cu = ob->data;
		Nurb *nu;
		BPoint *bp;
		BezTriple *bezt;
		int a;
		ListBase *nurbs = BKE_curve_editNurbs_get(cu);
		StructRNA *seltype = NULL;
		void *selp = NULL;

		nu = nurbs->first;
		while (nu) {
			if (nu->type == CU_BEZIER) {
				bezt = nu->bezt;
				a = nu->pntsu;
				while (a--) {
					if (bezt->f2 & SELECT) {
						add_v3_v3(&median[LOC_X], bezt->vec[1]);
						tot++;
						median[C_WEIGHT] += bezt->weight;
						median[C_RADIUS] += bezt->radius;
						median[C_TILT] += bezt->alfa;
						if (!totcurvedata) { /* I.e. first time... */
							selp = bezt;
							seltype = &RNA_BezierSplinePoint;
						}
						totcurvedata++;
					}
					else {
						if (bezt->f1 & SELECT) {
							add_v3_v3(&median[LOC_X], bezt->vec[0]);
							tot++;
						}
						if (bezt->f3 & SELECT) {
							add_v3_v3(&median[LOC_X], bezt->vec[2]);
							tot++;
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
						add_v3_v3(&median[LOC_X], bp->vec);
						median[C_BWEIGHT] += bp->vec[3];
						totcurvebweight++;
						tot++;
						median[C_WEIGHT] += bp->weight;
						median[C_RADIUS] += bp->radius;
						median[C_TILT] += bp->alfa;
						if (!totcurvedata) { /* I.e. first time... */
							selp = bp;
							seltype = &RNA_SplinePoint;
						}
						totcurvedata++;
					}
					bp++;
				}
			}
			nu = nu->next;
		}

		if (totcurvedata == 1)
			RNA_pointer_create(&cu->id, seltype, selp, &data_ptr);
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = ob->data;
		BPoint *bp;
		int a;
		StructRNA *seltype = NULL;
		void *selp = NULL;

		a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
		bp = lt->editlatt->latt->def;
		while (a--) {
			if (bp->f1 & SELECT) {
				add_v3_v3(&median[LOC_X], bp->vec);
				tot++;
				median[L_WEIGHT] += bp->weight;
				if (!totlattdata) { /* I.e. first time... */
					selp = bp;
					seltype = &RNA_LatticePoint;
				}
				totlattdata++;
			}
			bp++;
		}

		if (totlattdata == 1)
			RNA_pointer_create(&lt->id, seltype, selp, &data_ptr);
	}

	if (tot == 0) {
		uiDefBut(block, LABEL, 0, IFACE_("Nothing selected"), 0, 130, 200, 20, NULL, 0, 0, 0, 0, "");
		return;
	}

	/* Location, X/Y/Z */
	mul_v3_fl(&median[LOC_X], 1.0f / (float)tot);
	if (v3d->flag & V3D_GLOBAL_STATS)
		mul_m4_v3(ob->obmat, &median[LOC_X]);

	if (has_meshdata) {
		if (totedgedata) {
			median[M_CREASE] /= (float)totedgedata;
			median[M_BE_WEIGHT] /= (float)totedgedata;
		}
		if (tot) {
			median[M_BV_WEIGHT] /= (float)tot;
			if (has_skinradius) {
				median[M_SKIN_X] /= (float)tot;
				median[M_SKIN_Y] /= (float)tot;
			}
		}
	}
	else if (totcurvedata) {
		if (totcurvebweight) {
			median[C_BWEIGHT] /= (float)totcurvebweight;
		}
		median[C_WEIGHT] /= (float)totcurvedata;
		median[C_RADIUS] /= (float)totcurvedata;
		median[C_TILT] /= (float)totcurvedata;
	}
	else if (totlattdata) {
		median[L_WEIGHT] /= (float)totlattdata;
	}

	if (block) { /* buttons */
		uiBut *but;
		int yi = 200;
		const float tilt_limit = DEG2RADF(21600.0f);
		const int buth = 20 * UI_DPI_FAC;
		const int but_margin = 2;
		const char *c;

		memcpy(tfp->ve_median, median, sizeof(tfp->ve_median));

		uiBlockBeginAlign(block);
		if (tot == 1) {
			if (totcurvedata) /* Curve */
				c = IFACE_("Control Point:");
			else /* Mesh or lattice */
				c = IFACE_("Vertex:");
		}
		else
			c = IFACE_("Median:");
		uiDefBut(block, LABEL, 0, c, 0, yi -= buth, 200, buth, NULL, 0, 0, 0, 0, "");

		uiBlockBeginAlign(block);

		/* Should be no need to translate these. */
		but = uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, IFACE_("X:"), 0, yi -= buth, 200, buth,
		                &(tfp->ve_median[LOC_X]), -lim, lim, 10, RNA_TRANSLATION_PREC_DEFAULT, "");
		uiButSetUnitType(but, PROP_UNIT_LENGTH);
		but = uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, IFACE_("Y:"), 0, yi -= buth, 200, buth,
		                &(tfp->ve_median[LOC_Y]), -lim, lim, 10, RNA_TRANSLATION_PREC_DEFAULT, "");
		uiButSetUnitType(but, PROP_UNIT_LENGTH);
		but = uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, IFACE_("Z:"), 0, yi -= buth, 200, buth,
		                &(tfp->ve_median[LOC_Z]), -lim, lim, 10, RNA_TRANSLATION_PREC_DEFAULT, "");
		uiButSetUnitType(but, PROP_UNIT_LENGTH);

		if (totcurvebweight == tot) {
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, IFACE_("W:"), 0, yi -= buth, 200, buth,
			          &(tfp->ve_median[C_BWEIGHT]), 0.01, 100.0, 1, 3, "");
		}

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, V3D_GLOBAL_STATS, B_REDR, IFACE_("Global"),
		             0, yi -= buth + but_margin, 100, buth,
		             &v3d->flag, 0, 0, 0, 0, TIP_("Displays global values"));
		uiDefButBitS(block, TOGN, V3D_GLOBAL_STATS, B_REDR, IFACE_("Local"),
		             100, yi, 100, buth,
		             &v3d->flag, 0, 0, 0, 0, TIP_("Displays local values"));
		uiBlockEndAlign(block);

		/* Meshes... */
		if (has_meshdata) {
			if (tot) {
				uiDefBut(block, LABEL, 0, tot == 1 ? IFACE_("Vertex Data:") : IFACE_("Vertices Data:"),
				         0, yi -= buth + but_margin, 200, buth, NULL, 0.0, 0.0, 0, 0, "");
				/* customdata layer added on demand */
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN,
				          tot == 1 ? IFACE_("Bevel Weight:") : IFACE_("Mean Bevel Weight:"),
				          0, yi -= buth + but_margin, 200, buth,
				          &(tfp->ve_median[M_BV_WEIGHT]), 0.0, 1.0, 1, 2, TIP_("Vertex weight used by Bevel modifier"));
			}
			if (has_skinradius) {
				uiBlockBeginAlign(block);
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN,
				          tot == 1 ? IFACE_("Radius X:") : IFACE_("Mean Radius X:"),
				          0, yi -= buth + but_margin, 200, buth,
				          &(tfp->ve_median[M_SKIN_X]), 0.0, 100.0, 1, 3, TIP_("X radius used by Skin modifier"));
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN,
				          tot == 1 ? IFACE_("Radius Y:") : IFACE_("Mean Radius Y:"),
				          0, yi -= buth + but_margin, 200, buth,
				          &(tfp->ve_median[M_SKIN_Y]), 0.0, 100.0, 1, 3, TIP_("Y radius used by Skin modifier"));
				uiBlockEndAlign(block);
			}
			if (totedgedata) {
				uiDefBut(block, LABEL, 0, totedgedata == 1 ? IFACE_("Edge Data:") : IFACE_("Edges Data:"),
				         0, yi -= buth + but_margin, 200, buth, NULL, 0.0, 0.0, 0, 0, "");
				/* customdata layer added on demand */
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN,
				          totedgedata == 1 ? IFACE_("Bevel Weight:") : IFACE_("Mean Bevel Weight:"),
				          0, yi -= buth + but_margin, 200, buth,
				          &(tfp->ve_median[M_BE_WEIGHT]), 0.0, 1.0, 1, 2, TIP_("Edge weight used by Bevel modifier"));
				/* customdata layer added on demand */
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN,
				          totedgedata == 1 ? IFACE_("Crease:") : IFACE_("Mean Crease:"),
				          0, yi -= buth + but_margin, 200, buth,
				          &(tfp->ve_median[M_CREASE]), 0.0, 1.0, 1, 2, TIP_("Weight used by SubSurf modifier"));
			}
		}
		/* Curve... */
		else if (totcurvedata == 1) {
			uiDefButR(block, NUM, 0, IFACE_("Weight:"), 0, yi -= buth + but_margin, 200, buth,
			          &data_ptr, "weight_softbody", 0, 0.0, 1.0, 1, 3, NULL);
			uiDefButR(block, NUM, 0, IFACE_("Radius:"), 0, yi -= buth + but_margin, 200, buth,
			          &data_ptr, "radius", 0, 0.0, 100.0, 1, 3, NULL);
			uiDefButR(block, NUM, 0, IFACE_("Tilt:"), 0, yi -= buth + but_margin, 200, buth,
			          &data_ptr, "tilt", 0, -tilt_limit, tilt_limit, 1, 3, NULL);
		}
		else if (totcurvedata > 1) {
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, IFACE_("Mean Weight:"),
			          0, yi -= buth + but_margin, 200, buth,
			          &(tfp->ve_median[C_WEIGHT]), 0.0, 1.0, 1, 3, TIP_("Weight used for SoftBody Goal"));
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, IFACE_("Mean Radius:"),
			          0, yi -= buth + but_margin, 200, buth,
			          &(tfp->ve_median[C_RADIUS]), 0.0, 100.0, 1, 3, TIP_("Radius of curve control points"));
			but = uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, IFACE_("Mean Tilt:"),
			                0, yi -= buth + but_margin, 200, buth,
			                &(tfp->ve_median[C_TILT]), -tilt_limit, tilt_limit, 1, 3,
			                TIP_("Tilt of curve control points"));
			uiButSetUnitType(but, PROP_UNIT_ROTATION);
		}
		/* Lattice... */
		else if (totlattdata == 1) {
			uiDefButR(block, NUM, 0, IFACE_("Weight:"), 0, yi -= buth + but_margin, 200, buth,
			          &data_ptr, "weight_softbody", 0, 0.0, 1.0, 1, 3, NULL);
		}
		else if (totlattdata > 1) {
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, IFACE_("Mean Weight:"),
			          0, yi -= buth + but_margin, 200, buth,
			          &(tfp->ve_median[L_WEIGHT]), 0.0, 1.0, 1, 3, TIP_("Weight used for SoftBody Goal"));
		}

		uiBlockEndAlign(block);
	}
	else { /* apply */
		int i;

		memcpy(ve_median, tfp->ve_median, sizeof(tfp->ve_median));

		if (v3d->flag & V3D_GLOBAL_STATS) {
			invert_m4_m4(ob->imat, ob->obmat);
			mul_m4_v3(ob->imat, &median[LOC_X]);
			mul_m4_v3(ob->imat, &ve_median[LOC_X]);
		}
		i = NBR_TRANSFORM_PROPERTIES;
		while (i--)
			median[i] = ve_median[i] - median[i];

		if (ob->type == OB_MESH) {
			Mesh *me = ob->data;
			BMEditMesh *em = me->edit_btmesh;
			BMesh *bm = em->bm;
			BMIter iter;

			if (tot == 1 || len_v3(&median[LOC_X]) != 0.0f) {
				BMVert *eve;

				BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
					if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
						if (tot == 1) {
							/* In case we only have one element selected, copy directly the value instead of applying
							 * the diff. Avoids some glitches when going e.g. from 3 to 0.0001 (see [#37327]).
							 */
							copy_v3_v3(eve->co, &ve_median[LOC_X]);
						}
						else {
							add_v3_v3(eve->co, &median[LOC_X]);
						}
					}
				}

				EDBM_mesh_normals_update(em);
			}

			if (median[M_BV_WEIGHT] != 0.0f) {
				const int cd_vert_bweight_offset = (BM_mesh_cd_flag_ensure(bm, me, ME_CDFLAG_VERT_BWEIGHT),
				                                    CustomData_get_offset(&bm->vdata, CD_BWEIGHT));
				const float sca = compute_scale_factor(ve_median[M_BV_WEIGHT], median[M_BV_WEIGHT]);
				BMVert *eve;

				BLI_assert(cd_vert_bweight_offset != -1);

				if (ELEM(sca, 0.0f, 1.0f)) {
					BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
							float *bweight = BM_ELEM_CD_GET_VOID_P(eve, cd_vert_bweight_offset);
							*bweight = sca;
						}
					}
				}
				else if (sca > 0.0f) {
					BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && !BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
							float *bweight = BM_ELEM_CD_GET_VOID_P(eve, cd_vert_bweight_offset);
							*bweight *= sca;
							CLAMP(*bweight, 0.0f, 1.0f);
						}
					}
				}
				else {
					BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && !BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
							float *bweight = BM_ELEM_CD_GET_VOID_P(eve, cd_vert_bweight_offset);
							*bweight = 1.0f + ((1.0f - *bweight) * sca);
							CLAMP(*bweight, 0.0f, 1.0f);
						}
					}
				}
			}

			if (median[M_SKIN_X] != 0.0f) {
				const int cd_vert_skin_offset = CustomData_get_offset(&bm->vdata, CD_MVERT_SKIN);
				/* That one is not clamped to [0.0, 1.0]. */
				float sca = ve_median[M_SKIN_X];
				BMVert *eve;

				BLI_assert(cd_vert_skin_offset != -1);

				if (ve_median[M_SKIN_X] - median[M_SKIN_X] == 0.0f) {
					BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
							MVertSkin *vs = BM_ELEM_CD_GET_VOID_P(eve, cd_vert_skin_offset);
							vs->radius[0] = sca;
						}
					}
				}
				else {
					sca /= (ve_median[M_SKIN_X] - median[M_SKIN_X]);
					BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
							MVertSkin *vs = BM_ELEM_CD_GET_VOID_P(eve, cd_vert_skin_offset);
							vs->radius[0] *= sca;
						}
					}
				}
			}
			if (median[M_SKIN_Y] != 0.0f) {
				const int cd_vert_skin_offset = CustomData_get_offset(&bm->vdata, CD_MVERT_SKIN);
				/* That one is not clamped to [0.0, 1.0]. */
				float sca = ve_median[M_SKIN_Y];
				BMVert *eve;

				BLI_assert(cd_vert_skin_offset != -1);

				if (ve_median[M_SKIN_Y] - median[M_SKIN_Y] == 0.0f) {
					BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
							MVertSkin *vs = BM_ELEM_CD_GET_VOID_P(eve, cd_vert_skin_offset);
							vs->radius[1] = sca;
						}
					}
				}
				else {
					sca /= (ve_median[M_SKIN_Y] - median[M_SKIN_Y]);
					BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
							MVertSkin *vs = BM_ELEM_CD_GET_VOID_P(eve, cd_vert_skin_offset);
							vs->radius[1] *= sca;
						}
					}
				}
			}

			if (median[M_BE_WEIGHT] != 0.0f) {
				const int cd_edge_bweight_offset = (BM_mesh_cd_flag_ensure(bm, me, ME_CDFLAG_EDGE_BWEIGHT),
				                                    CustomData_get_offset(&bm->edata, CD_BWEIGHT));
				const float sca = compute_scale_factor(ve_median[M_BE_WEIGHT], median[M_BE_WEIGHT]);
				BMEdge *eed;

				BLI_assert(cd_edge_bweight_offset != -1);

				if (ELEM(sca, 0.0f, 1.0f)) {
					BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
							float *bweight = BM_ELEM_CD_GET_VOID_P(eed, cd_edge_bweight_offset);
							*bweight = sca;
						}
					}
				}
				else if (sca > 0.0f) {
					BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(eed, BM_ELEM_SELECT) && !BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
							float *bweight = BM_ELEM_CD_GET_VOID_P(eed, cd_edge_bweight_offset);
							*bweight *= sca;
							CLAMP(*bweight, 0.0f, 1.0f);
						}
					}
				}
				else {
					BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(eed, BM_ELEM_SELECT) && !BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
							float *bweight = BM_ELEM_CD_GET_VOID_P(eed, cd_edge_bweight_offset);
							*bweight = 1.0f + ((1.0f - *bweight) * sca);
							CLAMP(*bweight, 0.0f, 1.0f);
						}
					}
				}
			}

			if (median[M_CREASE] != 0.0f) {
				const int cd_edge_crease_offset  = (BM_mesh_cd_flag_ensure(bm, me, ME_CDFLAG_EDGE_CREASE),
				                                    CustomData_get_offset(&bm->edata, CD_CREASE));
				const float sca = compute_scale_factor(ve_median[M_CREASE], median[M_CREASE]);
				BMEdge *eed;

				if (ELEM(sca, 0.0f, 1.0f)) {
					BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
							BM_ELEM_CD_SET_FLOAT(eed, cd_edge_crease_offset, sca);
						}
					}
				}
				else if (sca > 0.0f) {
					BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(eed, BM_ELEM_SELECT) && !BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
							float *crease = BM_ELEM_CD_GET_VOID_P(eed, cd_edge_crease_offset);
							*crease *= sca;
							CLAMP(*crease, 0.0f, 1.0f);
						}
					}
				}
				else {
					BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(eed, BM_ELEM_SELECT) && !BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
							float *crease = BM_ELEM_CD_GET_VOID_P(eed, cd_edge_crease_offset);
							*crease = 1.0f + ((1.0f - *crease) * sca);
							CLAMP(*crease, 0.0f, 1.0f);
						}
					}
				}
			}
		}
		else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
			Curve *cu = ob->data;
			Nurb *nu;
			BPoint *bp;
			BezTriple *bezt;
			int a;
			ListBase *nurbs = BKE_curve_editNurbs_get(cu);
			const float scale_w = compute_scale_factor(ve_median[C_WEIGHT], median[C_WEIGHT]);

			nu = nurbs->first;
			while (nu) {
				if (nu->type == CU_BEZIER) {
					for (a = nu->pntsu, bezt = nu->bezt; a--; bezt++) {
						if (bezt->f2 & SELECT) {
							/* Here we always have to use the diff... :/
							 * Cannot avoid some glitches when going e.g. from 3 to 0.0001 (see [#37327]),
							 * unless we use doubles.
							 */
							add_v3_v3(bezt->vec[0], &median[LOC_X]);
							add_v3_v3(bezt->vec[1], &median[LOC_X]);
							add_v3_v3(bezt->vec[2], &median[LOC_X]);

							if (median[C_WEIGHT] != 0.0f) {
								if (ELEM(scale_w, 0.0f, 1.0f)) {
									bezt->weight = scale_w;
								}
								else {
									bezt->weight = scale_w > 0.0f ? bezt->weight * scale_w :
									                                1.0f + ((1.0f - bezt->weight) * scale_w);
									CLAMP(bezt->weight, 0.0f, 1.0f);
								}
							}

							bezt->radius += median[C_RADIUS];
							bezt->alfa += median[C_TILT];
						}
						else {
							if (bezt->f1 & SELECT) {
								if (tot == 1) {
									copy_v3_v3(bezt->vec[0], &ve_median[LOC_X]);
								}
								else {
									add_v3_v3(bezt->vec[0], &median[LOC_X]);
								}
							}
							if (bezt->f3 & SELECT) {
								if (tot == 1) {
									copy_v3_v3(bezt->vec[2], &ve_median[LOC_X]);
								}
								else {
									add_v3_v3(bezt->vec[2], &median[LOC_X]);
								}
							}
						}
					}
				}
				else {
					for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a--; bp++) {
						if (bp->f1 & SELECT) {
							if (tot == 1) {
								copy_v3_v3(bp->vec, &ve_median[LOC_X]);
								bp->vec[3] = ve_median[C_BWEIGHT];
								bp->radius = ve_median[C_RADIUS];
								bp->alfa = ve_median[C_TILT];
							}
							else {
								add_v3_v3(bp->vec, &median[LOC_X]);
								bp->vec[3] += median[C_BWEIGHT];
								bp->radius += median[C_RADIUS];
								bp->alfa += median[C_TILT];
							}

							if (median[C_WEIGHT] != 0.0f) {
								if (ELEM(scale_w, 0.0f, 1.0f)) {
									bp->weight = scale_w;
								}
								else {
									bp->weight = scale_w > 0.0f ? bp->weight * scale_w :
									                              1.0f + ((1.0f - bp->weight) * scale_w);
									CLAMP(bp->weight, 0.0f, 1.0f);
								}
							}
						}
					}
				}
				BKE_nurb_test2D(nu);
				BKE_nurb_handles_test(nu, true); /* test for bezier too */

				nu = nu->next;
			}
		}
		else if (ob->type == OB_LATTICE) {
			Lattice *lt = ob->data;
			BPoint *bp;
			int a;
			const float scale_w = compute_scale_factor(ve_median[L_WEIGHT], median[L_WEIGHT]);

			a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
			bp = lt->editlatt->latt->def;
			while (a--) {
				if (bp->f1 & SELECT) {
					if (tot == 1) {
						copy_v3_v3(bp->vec, &ve_median[LOC_X]);
					}
					else {
						add_v3_v3(bp->vec, &median[LOC_X]);
					}

					if (median[L_WEIGHT] != 0.0f) {
						if (ELEM(scale_w, 0.0f, 1.0f)) {
							bp->weight = scale_w;
						}
						else {
							bp->weight = scale_w > 0.0f ? bp->weight * scale_w :
							             1.0f + ((1.0f - bp->weight) * scale_w);
							CLAMP(bp->weight, 0.0f, 1.0f);
						}
					}
				}
				bp++;
			}
		}

/*		ED_undo_push(C, "Transform properties"); */
	}

/* Clean up! */
/* Location, common to all. */
#undef LOC_X
#undef LOC_Y
#undef LOC_Z
/* Meshes (and lattice)... */
#undef M_BV_WEIGHT
#undef M_SKIN_X
#undef M_SKIN_Y
#undef M_BE_WEIGHT
#undef M_CREASE
/* Curves... */
#undef C_BWEIGHT
#undef C_WEIGHT
#undef C_RADIUS
#undef C_TILT
/* Lattice... */
#undef L_WEIGHT
}
#undef NBR_TRANSFORM_PROPERTIES

#define B_VGRP_PNL_EDIT_SINGLE 8       /* or greater */

static void do_view3d_vgroup_buttons(bContext *C, void *UNUSED(arg), int event)
{
	if (event < B_VGRP_PNL_EDIT_SINGLE) {
		/* not for me */
		return;
	}
	else {
		Scene *scene = CTX_data_scene(C);
		Object *ob = scene->basact->object;
		ED_vgroup_vert_active_mirror(ob, event - B_VGRP_PNL_EDIT_SINGLE);
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
	}
}

static int view3d_panel_vgroup_poll(const bContext *C, PanelType *UNUSED(pt))
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = OBACT;
	if (ob && (BKE_object_is_in_editmode_vgroup(ob) ||
	           BKE_object_is_in_wpaint_select_vert(ob)))
	{
		MDeformVert *dvert_act = ED_mesh_active_dvert_get_only(ob);
		if (dvert_act) {
			return (dvert_act->totweight != 0);
		}
	}

	return false;
}


static void view3d_panel_vgroup(const bContext *C, Panel *pa)
{
	uiBlock *block = uiLayoutAbsoluteBlock(pa->layout);
	Scene *scene = CTX_data_scene(C);
	Object *ob = scene->basact->object;

	MDeformVert *dv;

	dv = ED_mesh_active_dvert_get_only(ob);

	if (dv && dv->totweight) {
		ToolSettings *ts = scene->toolsettings;

		wmOperatorType *ot_weight_set_active = WM_operatortype_find("OBJECT_OT_vertex_weight_set_active", true);
		wmOperatorType *ot_weight_paste = WM_operatortype_find("OBJECT_OT_vertex_weight_paste", true);
		wmOperatorType *ot_weight_delete = WM_operatortype_find("OBJECT_OT_vertex_weight_delete", true);

		wmOperatorType *ot;
		PointerRNA op_ptr, tools_ptr;
		PointerRNA *but_ptr;

		uiLayout *col, *bcol;
		uiLayout *row;
		uiBut *but;
		bDeformGroup *dg;
		unsigned int i;
		int subset_count, vgroup_tot;
		const bool *vgroup_validmap;
		eVGroupSelect subset_type = ts->vgroupsubset;
		int yco = 0;
		int lock_count = 0;

		uiBlockSetHandleFunc(block, do_view3d_vgroup_buttons, NULL);

		bcol = uiLayoutColumn(pa->layout, true);
		row = uiLayoutRow(bcol, true); /* The filter button row */
		
		RNA_pointer_create(NULL, &RNA_ToolSettings, ts, &tools_ptr);
		uiItemR(row, &tools_ptr, "vertex_group_subset", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

		col = uiLayoutColumn(bcol, true);

		vgroup_validmap = ED_vgroup_subset_from_select_type(ob, subset_type, &vgroup_tot, &subset_count);
		for (i = 0, dg = ob->defbase.first; dg; i++, dg = dg->next) {
			bool locked = dg->flag & DG_LOCK_WEIGHT;
			if (vgroup_validmap[i]) {
				MDeformWeight *dw = defvert_find_index(dv, i);
				if (dw) {
					int x, xco = 0;
					int icon;
					uiLayout *split = uiLayoutSplit(col, 0.45, true);
					row = uiLayoutRow(split, true);

					/* The Weight Group Name */

					ot = ot_weight_set_active;
					but = uiDefButO_ptr(block, BUT, ot, WM_OP_EXEC_DEFAULT, dg->name,
					                    xco, yco, (x = UI_UNIT_X * 5), UI_UNIT_Y, "");
					but_ptr = uiButGetOperatorPtrRNA(but);
					RNA_int_set(but_ptr, "weight_group", i);
					uiButSetDrawFlag(but, UI_BUT_TEXT_RIGHT);
					if (ob->actdef != i + 1) {
						uiButSetFlag(but, UI_BUT_INACTIVE);
					}
					xco += x;
					
					row = uiLayoutRow(split, true);
					uiLayoutSetEnabled(row, !locked);

					/* The weight group value */
					/* To be reworked still */
					but = uiDefButF(block, NUM, B_VGRP_PNL_EDIT_SINGLE + i, "",
					                xco, yco, (x = UI_UNIT_X * 4), UI_UNIT_Y,
					                &dw->weight, 0.0, 1.0, 1, 3, "");
					uiButSetDrawFlag(but, UI_BUT_TEXT_LEFT);
					if (locked) {
						lock_count++;
					}
					xco += x;

					/* The weight group paste function */

					ot = ot_weight_paste;
					WM_operator_properties_create_ptr(&op_ptr, ot);
					RNA_int_set(&op_ptr, "weight_group", i);
					icon = (locked) ? ICON_BLANK1 : ICON_PASTEDOWN;
					uiItemFullO_ptr(row, ot, "", icon, op_ptr.data, WM_OP_INVOKE_DEFAULT, 0);

					/* The weight entry delete function */

					ot = ot_weight_delete;
					WM_operator_properties_create_ptr(&op_ptr, ot);
					RNA_int_set(&op_ptr, "weight_group", i);
					icon = (locked) ? ICON_LOCKED : ICON_X;
					uiItemFullO_ptr(row, ot, "", icon, op_ptr.data, WM_OP_INVOKE_DEFAULT, 0);

					yco -= UI_UNIT_Y;
					
				}
			}
		}
		MEM_freeN((void *)vgroup_validmap);

		yco -= 2;

		col = uiLayoutColumn(pa->layout, true);
		row = uiLayoutRow(col, true);

		ot = WM_operatortype_find("OBJECT_OT_vertex_weight_normalize_active_vertex", 1);
		but = uiDefButO_ptr(block, BUT, ot, WM_OP_EXEC_DEFAULT, "Normalize",
		                    0, yco, UI_UNIT_X * 5, UI_UNIT_Y,
		                    TIP_("Normalize weights of active vertex (if affected groups are unlocked)"));
		if (lock_count) {
			uiButSetFlag(but, UI_BUT_DISABLED);
		}

		ot = WM_operatortype_find("OBJECT_OT_vertex_weight_copy", 1);
		but = uiDefButO_ptr(block, BUT, ot, WM_OP_EXEC_DEFAULT, "Copy",
		                    UI_UNIT_X * 5, yco, UI_UNIT_X * 5, UI_UNIT_Y,
		                    TIP_("Copy active vertex to other selected vertices (if affected groups are unlocked)"));
		if (lock_count) {
			uiButSetFlag(but, UI_BUT_DISABLED);
		}

	}
}

static void v3d_transform_butsR(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *split, *colsub;

	split = uiLayoutSplit(layout, 0.8f, false);

	if (ptr->type == &RNA_PoseBone) {
		PointerRNA boneptr;
		Bone *bone;

		boneptr = RNA_pointer_get(ptr, "bone");
		bone = boneptr.data;
		uiLayoutSetActive(split, !(bone->parent && bone->flag & BONE_CONNECTED));
	}
	colsub = uiLayoutColumn(split, true);
	uiItemR(colsub, ptr, "location", 0, NULL, ICON_NONE);
	colsub = uiLayoutColumn(split, true);
	uiItemL(colsub, "", ICON_NONE);
	uiItemR(colsub, ptr, "lock_location", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "", ICON_NONE);

	split = uiLayoutSplit(layout, 0.8f, false);

	switch (RNA_enum_get(ptr, "rotation_mode")) {
		case ROT_MODE_QUAT: /* quaternion */
			colsub = uiLayoutColumn(split, true);
			uiItemR(colsub, ptr, "rotation_quaternion", 0, IFACE_("Rotation"), ICON_NONE);
			colsub = uiLayoutColumn(split, true);
			uiItemR(colsub, ptr, "lock_rotations_4d", UI_ITEM_R_TOGGLE, IFACE_("4L"), ICON_NONE);
			if (RNA_boolean_get(ptr, "lock_rotations_4d"))
				uiItemR(colsub, ptr, "lock_rotation_w", UI_ITEM_R_TOGGLE + UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			else
				uiItemL(colsub, "", ICON_NONE);
			uiItemR(colsub, ptr, "lock_rotation", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			break;
		case ROT_MODE_AXISANGLE: /* axis angle */
			colsub = uiLayoutColumn(split, true);
			uiItemR(colsub, ptr, "rotation_axis_angle", 0, IFACE_("Rotation"), ICON_NONE);
			colsub = uiLayoutColumn(split, true);
			uiItemR(colsub, ptr, "lock_rotations_4d", UI_ITEM_R_TOGGLE, IFACE_("4L"), ICON_NONE);
			if (RNA_boolean_get(ptr, "lock_rotations_4d"))
				uiItemR(colsub, ptr, "lock_rotation_w", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			else
				uiItemL(colsub, "", ICON_NONE);
			uiItemR(colsub, ptr, "lock_rotation", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			break;
		default: /* euler rotations */
			colsub = uiLayoutColumn(split, true);
			uiItemR(colsub, ptr, "rotation_euler", 0, IFACE_("Rotation"), ICON_NONE);
			colsub = uiLayoutColumn(split, true);
			uiItemL(colsub, "", ICON_NONE);
			uiItemR(colsub, ptr, "lock_rotation", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			break;
	}
	uiItemR(layout, ptr, "rotation_mode", 0, "", ICON_NONE);

	split = uiLayoutSplit(layout, 0.8f, false);
	colsub = uiLayoutColumn(split, true);
	uiItemR(colsub, ptr, "scale", 0, NULL, ICON_NONE);
	colsub = uiLayoutColumn(split, true);
	uiItemL(colsub, "", ICON_NONE);
	uiItemR(colsub, ptr, "lock_scale", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "", ICON_NONE);

	if (ptr->type == &RNA_Object) {
		Object *ob = ptr->data;
		/* dimensions and material support just happen to be the same checks
		 * later we may want to add dimensions for lattice, armature etc too */
		if (OB_TYPE_SUPPORT_MATERIAL(ob->type)) {
			uiItemR(layout, ptr, "dimensions", 0, NULL, ICON_NONE);
		}
	}
}

static void v3d_posearmature_buts(uiLayout *layout, Object *ob)
{
	bPoseChannel *pchan;
	PointerRNA pchanptr;
	uiLayout *col;

	pchan = BKE_pose_channel_active(ob);

	if (!pchan) {
		uiItemL(layout, IFACE_("No Bone Active"), ICON_NONE);
		return;
	}

	RNA_pointer_create(&ob->id, &RNA_PoseBone, pchan, &pchanptr);

	col = uiLayoutColumn(layout, false);

	/* XXX: RNA buts show data in native types (i.e. quats, 4-component axis/angle, etc.)
	 * but old-school UI shows in eulers always. Do we want to be able to still display in Eulers?
	 * Maybe needs RNA/ui options to display rotations as different types... */
	v3d_transform_butsR(col, &pchanptr);
}

static void v3d_editarmature_buts(uiLayout *layout, Object *ob)
{
	bArmature *arm = ob->data;
	EditBone *ebone;
	uiLayout *col;
	PointerRNA eboneptr;

	ebone = arm->act_edbone;

	if (!ebone || (ebone->layer & arm->layer) == 0) {
		uiItemL(layout, IFACE_("Nothing selected"), ICON_NONE);
		return;
	}

	RNA_pointer_create(&arm->id, &RNA_EditBone, ebone, &eboneptr);

	col = uiLayoutColumn(layout, false);
	uiItemR(col, &eboneptr, "head", 0, NULL, ICON_NONE);
	if (ebone->parent && ebone->flag & BONE_CONNECTED) {
		PointerRNA parptr = RNA_pointer_get(&eboneptr, "parent");
		uiItemR(col, &parptr, "tail_radius", 0, IFACE_("Radius (Parent)"), ICON_NONE);
	}
	else {
		uiItemR(col, &eboneptr, "head_radius", 0, IFACE_("Radius"), ICON_NONE);
	}

	uiItemR(col, &eboneptr, "tail", 0, NULL, ICON_NONE);
	uiItemR(col, &eboneptr, "tail_radius", 0, IFACE_("Radius"), ICON_NONE);

	uiItemR(col, &eboneptr, "roll", 0, NULL, ICON_NONE);
	uiItemR(col, &eboneptr, "envelope_distance", 0, IFACE_("Envelope"), ICON_NONE);
}

static void v3d_editmetaball_buts(uiLayout *layout, Object *ob)
{
	PointerRNA mbptr, ptr;
	MetaBall *mball = ob->data;
	uiLayout *col;

	if (!mball || !(mball->lastelem))
		return;

	RNA_pointer_create(&mball->id, &RNA_MetaBall, mball, &mbptr);

	RNA_pointer_create(&mball->id, &RNA_MetaElement, mball->lastelem, &ptr);

	col = uiLayoutColumn(layout, false);
	uiItemR(col, &ptr, "co", 0, NULL, ICON_NONE);

	uiItemR(col, &ptr, "radius", 0, NULL, ICON_NONE);
	uiItemR(col, &ptr, "stiffness", 0, NULL, ICON_NONE);

	uiItemR(col, &ptr, "type", 0, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, true);
	switch (RNA_enum_get(&ptr, "type")) {
		case MB_BALL:
			break;
		case MB_CUBE:
			uiItemL(col, IFACE_("Size:"), ICON_NONE);
			uiItemR(col, &ptr, "size_x", 0, "X", ICON_NONE);
			uiItemR(col, &ptr, "size_y", 0, "Y", ICON_NONE);
			uiItemR(col, &ptr, "size_z", 0, "Z", ICON_NONE);
			break;
		case MB_TUBE:
			uiItemL(col, IFACE_("Size:"), ICON_NONE);
			uiItemR(col, &ptr, "size_x", 0, "X", ICON_NONE);
			break;
		case MB_PLANE:
			uiItemL(col, IFACE_("Size:"), ICON_NONE);
			uiItemR(col, &ptr, "size_x", 0, "X", ICON_NONE);
			uiItemR(col, &ptr, "size_y", 0, "Y", ICON_NONE);
			break;
		case MB_ELIPSOID:
			uiItemL(col, IFACE_("Size:"), ICON_NONE);
			uiItemR(col, &ptr, "size_x", 0, "X", ICON_NONE);
			uiItemR(col, &ptr, "size_y", 0, "Y", ICON_NONE);
			uiItemR(col, &ptr, "size_z", 0, "Z", ICON_NONE);
			break;
	}
}

static void do_view3d_region_buttons(bContext *C, void *UNUSED(index), int event)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	Object *ob = OBACT;

	switch (event) {

		case B_REDR:
			ED_area_tag_redraw(CTX_wm_area(C));
			return; /* no notifier! */

		case B_OBJECTPANELMEDIAN:
			if (ob) {
				v3d_editvertex_buts(NULL, v3d, ob, 1.0);
				DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			}
			break;
	}

	/* default for now */
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
}

static int view3d_panel_transform_poll(const bContext *C, PanelType *UNUSED(pt))
{
	Scene *scene = CTX_data_scene(C);
	return (scene->basact != NULL);
}

static void view3d_panel_transform(const bContext *C, Panel *pa)
{
	uiBlock *block;
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Object *ob = scene->basact->object;
	uiLayout *col;

	block = uiLayoutGetBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_view3d_region_buttons, NULL);

	col = uiLayoutColumn(pa->layout, false);

	if (ob == obedit) {
		if (ob->type == OB_ARMATURE) {
			v3d_editarmature_buts(col, ob);
		}
		else if (ob->type == OB_MBALL) {
			v3d_editmetaball_buts(col, ob);
		}
		else {
			View3D *v3d = CTX_wm_view3d(C);
			const float lim = 10000.0f * max_ff(1.0f, v3d->grid);
			v3d_editvertex_buts(col, v3d, ob, lim);
		}
	}
	else if (ob->mode & OB_MODE_POSE) {
		v3d_posearmature_buts(col, ob);
	}
	else {
		PointerRNA obptr;

		RNA_id_pointer_create(&ob->id, &obptr);
		v3d_transform_butsR(col, &obptr);
	}
}

void view3d_buttons_register(ARegionType *art)
{
	PanelType *pt;

	pt = MEM_callocN(sizeof(PanelType), "spacetype view3d panel object");
	strcpy(pt->idname, "VIEW3D_PT_transform");
	strcpy(pt->label, N_("Transform"));  /* XXX C panels not  available through RNA (bpy.types)! */
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = view3d_panel_transform;
	pt->poll = view3d_panel_transform_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt = MEM_callocN(sizeof(PanelType), "spacetype view3d panel gpencil");
	strcpy(pt->idname, "VIEW3D_PT_gpencil");
	strcpy(pt->label, N_("Grease Pencil"));  /* XXX C panels are not available through RNA (bpy.types)! */
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw_header = ED_gpencil_panel_standard_header;
	pt->draw = ED_gpencil_panel_standard;
	BLI_addtail(&art->paneltypes, pt);

	pt = MEM_callocN(sizeof(PanelType), "spacetype view3d panel vgroup");
	strcpy(pt->idname, "VIEW3D_PT_vgroup");
	strcpy(pt->label, N_("Vertex Weights"));  /* XXX C panels are not available through RNA (bpy.types)! */
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = view3d_panel_vgroup;
	pt->poll = view3d_panel_vgroup_poll;
	BLI_addtail(&art->paneltypes, pt);
}

static int view3d_properties_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = view3d_has_buttons_region(sa);

	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_properties(wmOperatorType *ot)
{
	ot->name = "Properties";
	ot->description = "Toggles the properties panel display";
	ot->idname = "VIEW3D_OT_properties";

	ot->exec = view3d_properties_toggle_exec;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = 0;
}
