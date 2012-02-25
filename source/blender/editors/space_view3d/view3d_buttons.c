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

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_screen.h"
#include "BKE_tessmesh.h"
#include "BKE_deform.h"
#include "BKE_object.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_armature.h"
#include "ED_gpencil.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_curve.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "view3d_intern.h"	// own include


/* ******************* view3d space & buttons ************** */
#define B_NOP		1
#define B_REDR		2
#define B_OBJECTPANELROT 	1007
#define B_OBJECTPANELMEDIAN 1008
#define B_ARMATUREPANEL1 	1009
#define B_ARMATUREPANEL2 	1010
#define B_OBJECTPANELPARENT 1011
#define B_OBJECTPANEL		1012
#define B_ARMATUREPANEL3 	1013
#define B_OBJECTPANELSCALE 	1014
#define B_OBJECTPANELDIMS 	1015
#define B_TRANSFORMSPACEADD	1016
#define B_TRANSFORMSPACECLEAR	1017
#define B_SETPT_AUTO	2125
#define B_SETPT_VECTOR	2126
#define B_SETPT_ALIGN	2127
#define B_SETPT_FREE	2128
#define B_RECALCMBALL	2501

#define B_WEIGHT0_0		2840
#define B_WEIGHT1_4		2841
#define B_WEIGHT1_2		2842
#define B_WEIGHT3_4		2843
#define B_WEIGHT1_0		2844

#define B_OPA1_8		2845
#define B_OPA1_4		2846
#define B_OPA1_2		2847
#define B_OPA3_4		2848
#define B_OPA1_0		2849

#define B_CLR_WPAINT	2850

#define B_RV3D_LOCKED	2900
#define B_RV3D_BOXVIEW	2901
#define B_RV3D_BOXCLIP	2902

#define B_IDNAME		3000

/* temporary struct for storing transform properties */
typedef struct {
	float ob_eul[4];	// used for quat too....
	float ob_scale[3]; // need temp space due to linked values
	float ob_dims[3];
	short link_scale;
	float ve_median[7];
	int curdef;
	float *defweightp;
} TransformProperties;


/* is used for both read and write... */
static void v3d_editvertex_buts(uiLayout *layout, View3D *v3d, Object *ob, float lim)
{
	uiBlock *block= (layout)? uiLayoutAbsoluteBlock(layout): NULL;
	MDeformVert *dvert=NULL;
	TransformProperties *tfp;
	float median[7], ve_median[7];
	int tot, totw, totweight, totedge, totradius;
	char defstr[320];
	PointerRNA radius_ptr;

	median[0]= median[1]= median[2]= median[3]= median[4]= median[5]= median[6]= 0.0;
	tot= totw= totweight= totedge= totradius= 0;
	defstr[0]= 0;

	/* make sure we got storage */
	if (v3d->properties_storage==NULL)
		v3d->properties_storage= MEM_callocN(sizeof(TransformProperties), "TransformProperties");
	tfp= v3d->properties_storage;
	
	if (ob->type==OB_MESH) {
		Mesh *me= ob->data;
		BMEditMesh *em = me->edit_btmesh;
		BMesh *bm = em->bm;
		BMVert *eve, *evedef=NULL;
		BMEdge *eed;
		BMIter iter;
		
		BM_ITER(eve, &iter, bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
				evedef= eve;
				tot++;
				add_v3_v3(median, eve->co);
			}
		}

		BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
				float *f;

				totedge++;
				f = (float *)CustomData_bmesh_get(&bm->edata, eed->head.data, CD_CREASE);
				median[3]+= f ? *f : 0.0f;

				f = (float *)CustomData_bmesh_get(&bm->edata, eed->head.data, CD_BWEIGHT);
				median[6]+= f ? *f : 0.0f;
			}
		}

		/* check for defgroups */
		if (evedef)
			dvert= CustomData_bmesh_get(&bm->vdata, evedef->head.data, CD_MDEFORMVERT);
		if (tot==1 && dvert && dvert->totweight) {
			bDeformGroup *dg;
			int i, max=1, init=1;
			char str[320];
			
			for (i=0; i<dvert->totweight; i++) {
				dg = BLI_findlink (&ob->defbase, dvert->dw[i].def_nr);
				if (dg) {
					max+= BLI_snprintf(str, sizeof(str), "%s %%x%d|", dg->name, dvert->dw[i].def_nr); 
					if (max < sizeof(str)) strcat(defstr, str);
				}

				if (tfp->curdef==dvert->dw[i].def_nr) {
					init= 0;
					tfp->defweightp= &dvert->dw[i].weight;
				}
			}
			
			if (init) {	// needs new initialized
				tfp->curdef= dvert->dw[0].def_nr;
				tfp->defweightp= &dvert->dw[0].weight;
			}
		}
	}
	else if (ob->type==OB_CURVE || ob->type==OB_SURF) {
		Curve *cu= ob->data;
		Nurb *nu;
		BPoint *bp;
		BezTriple *bezt;
		int a;
		ListBase *nurbs= curve_editnurbs(cu);
		StructRNA *seltype= NULL;
		void *selp= NULL;

		nu= nurbs->first;
		while (nu) {
			if (nu->type == CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while (a--) {
					if (bezt->f2 & SELECT) {
						add_v3_v3(median, bezt->vec[1]);
						tot++;
						median[4]+= bezt->weight;
						totweight++;
						median[5]+= bezt->radius;
						totradius++;
						selp= bezt;
						seltype= &RNA_BezierSplinePoint;
					}
					else {
						if (bezt->f1 & SELECT) {
							add_v3_v3(median, bezt->vec[0]);
							tot++;
						}
						if (bezt->f3 & SELECT) {
							add_v3_v3(median, bezt->vec[2]);
							tot++;
						}
					}
					bezt++;
				}
			}
			else {
				bp= nu->bp;
				a= nu->pntsu*nu->pntsv;
				while (a--) {
					if (bp->f1 & SELECT) {
						add_v3_v3(median, bp->vec);
						median[3]+= bp->vec[3];
						totw++;
						tot++;
						median[4]+= bp->weight;
						totweight++;
						median[5]+= bp->radius;
						totradius++;
						selp= bp;
						seltype= &RNA_SplinePoint;
					}
					bp++;
				}
			}
			nu= nu->next;
		}

		if (totradius==1)
			RNA_pointer_create(&cu->id, seltype, selp, &radius_ptr);
	}
	else if (ob->type==OB_LATTICE) {
		Lattice *lt= ob->data;
		BPoint *bp;
		int a;
		
		a= lt->editlatt->latt->pntsu*lt->editlatt->latt->pntsv*lt->editlatt->latt->pntsw;
		bp= lt->editlatt->latt->def;
		while (a--) {
			if (bp->f1 & SELECT) {
				add_v3_v3(median, bp->vec);
				tot++;
				median[4]+= bp->weight;
				totweight++;
			}
			bp++;
		}
	}
	
	if (tot==0) {
		uiDefBut(block, LABEL, 0, "Nothing selected",0, 130, 200, 20, NULL, 0, 0, 0, 0, "");
		return;
	}
	median[0] /= (float)tot;
	median[1] /= (float)tot;
	median[2] /= (float)tot;
	if (totedge) {
		median[3] /= (float)totedge;
		median[6] /= (float)totedge;
	}
	else if (totw) median[3] /= (float)totw;
	if (totweight) median[4] /= (float)totweight;
	if (totradius) median[5] /= (float)totradius;
	
	if (v3d->flag & V3D_GLOBAL_STATS)
		mul_m4_v3(ob->obmat, median);
	
	if (block) {	// buttons
		uiBut *but;

		memcpy(tfp->ve_median, median, sizeof(tfp->ve_median));
		
		uiBlockBeginAlign(block);
		if (tot==1) {
			uiDefBut(block, LABEL, 0, "Vertex:",					0, 150, 200, 20, NULL, 0, 0, 0, 0, "");
			uiBlockBeginAlign(block);

			but= uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "X:",		0, 130, 200, 20, &(tfp->ve_median[0]), -lim, lim, 10, 3, "");
			uiButSetUnitType(but, PROP_UNIT_LENGTH);
			but= uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Y:",		0, 110, 200, 20, &(tfp->ve_median[1]), -lim, lim, 10, 3, "");
			uiButSetUnitType(but, PROP_UNIT_LENGTH);
			but= uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Z:",		0, 90, 200, 20, &(tfp->ve_median[2]), -lim, lim, 10, 3, "");
			uiButSetUnitType(but, PROP_UNIT_LENGTH);

			if (totw==1) {
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "W:",	0, 70, 200, 20, &(tfp->ve_median[3]), 0.01, 100.0, 1, 3, "");
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, V3D_GLOBAL_STATS, B_REDR, "Global",		0, 45, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays global values");
				uiDefButBitS(block, TOGN, V3D_GLOBAL_STATS, B_REDR, "Local",		100, 45, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays local values");
				uiBlockEndAlign(block);
				if (totweight)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Weight:",	0, 20, 200, 20, &(tfp->ve_median[4]), 0.0, 1.0, 1, 3, "");
				if (totradius) {
					if (totradius==1) uiDefButR(block, NUM, 0, "Radius", 0, 20, 200, 20, &radius_ptr, "radius", 0, 0.0, 100.0, 10, 3, NULL);
					else uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Radius:",	0, 20, 200, 20, &(tfp->ve_median[5]), 0.0, 100.0, 1, 3, "Radius of curve CPs");
				}
			}
			else {
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, V3D_GLOBAL_STATS, B_REDR, "Global",		0, 65, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays global values");
				uiDefButBitS(block, TOGN, V3D_GLOBAL_STATS, B_REDR, "Local",		100, 65, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays local values");
				uiBlockEndAlign(block);
				if (totweight)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Weight:",	0, 40, 200, 20, &(tfp->ve_median[4]), 0.0, 1.0, 10, 3, "");
				if (totradius) {
					if (totradius==1) uiDefButR(block, NUM, 0, "Radius", 0, 40, 200, 20, &radius_ptr, "radius", 0, 0.0, 100.0, 10, 3, NULL);
					else uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Radius:",	0, 40, 200, 20, &(tfp->ve_median[5]), 0.0, 100.0, 10, 3, "Radius of curve CPs");
				}
			}
		}
		else {
			uiDefBut(block, LABEL, 0, "Median:",					0, 150, 200, 20, NULL, 0, 0, 0, 0, "");
			uiBlockBeginAlign(block);
			but= uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "X:",		0, 130, 200, 20, &(tfp->ve_median[0]), -lim, lim, 10, 3, "");
			uiButSetUnitType(but, PROP_UNIT_LENGTH);
			but= uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Y:",		0, 110, 200, 20, &(tfp->ve_median[1]), -lim, lim, 10, 3, "");
			uiButSetUnitType(but, PROP_UNIT_LENGTH);
			but= uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Z:",		0, 90, 200, 20, &(tfp->ve_median[2]), -lim, lim, 10, 3, "");
			uiButSetUnitType(but, PROP_UNIT_LENGTH);
			if (totw==tot) {
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "W:",	0, 70, 200, 20, &(tfp->ve_median[3]), 0.01, 100.0, 1, 3, "");
				uiBlockEndAlign(block);
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, V3D_GLOBAL_STATS, B_REDR, "Global",		0, 45, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays global values");
				uiDefButBitS(block, TOGN, V3D_GLOBAL_STATS, B_REDR, "Local",		100, 45, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays local values");
				uiBlockEndAlign(block);
				if (totweight)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Weight:",	0, 20, 200, 20, &(tfp->ve_median[4]), 0.0, 1.0, 10, 3, "Weight is used for SoftBody Goal");
				if (totradius)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Radius:",	0, 20, 200, 20, &(tfp->ve_median[5]), 0.0, 100.0, 10, 3, "Radius of curve CPs");
				uiBlockEndAlign(block);
			}
			else {
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, V3D_GLOBAL_STATS, B_REDR, "Global",		0, 65, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays global values");
				uiDefButBitS(block, TOGN, V3D_GLOBAL_STATS, B_REDR, "Local",		100, 65, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays local values");
				uiBlockEndAlign(block);
				if (totweight)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Weight:",	0, 40, 200, 20, &(tfp->ve_median[4]), 0.0, 1.0, 1, 3, "Weight is used for SoftBody Goal");
				if (totradius)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Radius:",	0, 20, 200, 20, &(tfp->ve_median[5]), 0.0, 100.0, 1, 3, "Radius of curve CPs");
				uiBlockEndAlign(block);
			}
		}

		if (totedge==1) {
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Crease:",	0, 40, 200, 20, &(tfp->ve_median[3]), 0.0, 1.0, 1, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Bevel Weight:",	0, 20, 200, 20, &(tfp->ve_median[6]), 0.0, 1.0, 1, 3, "");
		}
		else if (totedge>1) {
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Mean Crease:",	0, 40, 200, 20, &(tfp->ve_median[3]), 0.0, 1.0, 1, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Mean Bevel Weight:",	0, 20, 200, 20, &(tfp->ve_median[6]), 0.0, 1.0, 1, 3, "");
		}

	}
	else {	// apply
		memcpy(ve_median, tfp->ve_median, sizeof(tfp->ve_median));
		
		if (v3d->flag & V3D_GLOBAL_STATS) {
			invert_m4_m4(ob->imat, ob->obmat);
			mul_m4_v3(ob->imat, median);
			mul_m4_v3(ob->imat, ve_median);
		}
		sub_v3_v3v3(median, ve_median, median);
		median[3]= ve_median[3]-median[3];
		median[4]= ve_median[4]-median[4];
		median[5]= ve_median[5]-median[5];
		median[6]= ve_median[6]-median[6];
		
		if (ob->type==OB_MESH) {
			Mesh *me= ob->data;
			BMEditMesh *em = me->edit_btmesh;
			BMesh *bm = em->bm;
			BMVert *eve;
			BMIter iter;

			if (len_v3(median) > 0.000001f) {

				BM_ITER(eve, &iter, bm, BM_VERTS_OF_MESH, NULL) {
					if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
						add_v3_v3(eve->co, median);
					}
				}
				
				EDBM_RecalcNormals(em);
			}
			
			if (median[3] != 0.0f) {
				BMEdge *eed;
				const float fixed_crease= (ve_median[3] <= 0.0f ? 0.0f : (ve_median[3] >= 1.0f ? 1.0f : FLT_MAX));
				
				if (fixed_crease != FLT_MAX) {
					/* simple case */

					BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
						if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
							float *crease = (float *)CustomData_bmesh_get(&bm->edata, eed->head.data, CD_CREASE);
							if (!crease) break;
							
							*crease= fixed_crease;
						}
					}
				}
				else {
					/* scale crease to target median */
					float median_new= ve_median[3];
					float median_orig= ve_median[3] - median[3]; /* previous median value */

					/* incase of floating point error */
					CLAMP(median_orig, 0.0f, 1.0f);
					CLAMP(median_new, 0.0f, 1.0f);

					if (median_new < median_orig) {
						/* scale down */
						const float sca= median_new / median_orig;
						
						BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
							if (BM_elem_flag_test(eed, BM_ELEM_SELECT) && !BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
								float *crease = (float *)CustomData_bmesh_get(&bm->edata, eed->head.data, CD_CREASE);
								
								if (!crease) break;
								
								*crease *= sca;
								CLAMP(*crease, 0.0f, 1.0f);
							}
						}
					}
					else {
						/* scale up */
						const float sca= (1.0f - median_new) / (1.0f - median_orig);

						BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
							if (BM_elem_flag_test(eed, BM_ELEM_SELECT) && !BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
								float *crease = (float *)CustomData_bmesh_get(&bm->edata, eed->head.data, CD_CREASE);
								if (!crease) break;

								*crease = 1.0f - ((1.0f - *crease) * sca);
								CLAMP(*crease, 0.0f, 1.0f);
							}
						}
					}
				}
			}

			if (median[6] != 0.0f) {
				BMEdge *eed;
				const float fixed_bweight = (ve_median[6] <= 0.0f ? 0.0f : (ve_median[6] >= 1.0f ? 1.0f : FLT_MAX));

				if (fixed_bweight != FLT_MAX) {
					/* simple case */

					BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
						if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
							float *bweight = (float *)CustomData_bmesh_get(&bm->edata, eed->head.data, CD_BWEIGHT);
							if (!bweight) break;
							
							*bweight = fixed_bweight;
						}
					}
				}
				else {
					/* scale crease to target median */
					float median_new = ve_median[6];
					float median_orig = ve_median[6] - median[6]; /* previous median value */

					/* incase of floating point error */
					CLAMP(median_orig, 0.0f, 1.0f);
					CLAMP(median_new, 0.0f, 1.0f);

					if (median_new < median_orig) {
						/* scale down */
						const float sca = median_new / median_orig;
						
						BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
							if (BM_elem_flag_test(eed, BM_ELEM_SELECT) && !BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
								float *bweight = (float *)CustomData_bmesh_get(&bm->edata, eed->head.data, CD_BWEIGHT);
								if (!bweight) break;
								
								*bweight *= sca;
								CLAMP(*bweight, 0.0f, 1.0f);
							}
						}
					}
					else {
						/* scale up */
						const float sca = (1.0f - median_new) / (1.0f - median_orig);

						BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
							if (BM_elem_flag_test(eed, BM_ELEM_SELECT) && !BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
								float *bweight = (float *)CustomData_bmesh_get(&bm->edata, eed->head.data, CD_BWEIGHT);
								if (!bweight) break;

								*bweight = 1.0f - ((1.0f - *bweight) * sca);
								CLAMP(*bweight, 0.0f, 1.0f);
							}
						}
					}
				}
			}
			EDBM_RecalcNormals(em);
		}
		else if (ob->type==OB_CURVE || ob->type==OB_SURF) {
			Curve *cu= ob->data;
			Nurb *nu;
			BPoint *bp;
			BezTriple *bezt;
			int a;
			ListBase *nurbs= curve_editnurbs(cu);

			nu= nurbs->first;
			while (nu) {
				if (nu->type == CU_BEZIER) {
					bezt= nu->bezt;
					a= nu->pntsu;
					while (a--) {
						if (bezt->f2 & SELECT) {
							add_v3_v3(bezt->vec[0], median);
							add_v3_v3(bezt->vec[1], median);
							add_v3_v3(bezt->vec[2], median);
							bezt->weight+= median[4];
							bezt->radius+= median[5];
						}
						else {
							if (bezt->f1 & SELECT) {
								add_v3_v3(bezt->vec[0], median);
							}
							if (bezt->f3 & SELECT) {
								add_v3_v3(bezt->vec[2], median);
							}
						}
						bezt++;
					}
				}
				else {
					bp= nu->bp;
					a= nu->pntsu*nu->pntsv;
					while (a--) {
						if (bp->f1 & SELECT) {
							add_v3_v3(bp->vec, median);
							bp->vec[3]+= median[3];
							bp->weight+= median[4];
							bp->radius+= median[5];
						}
						bp++;
					}
				}
				test2DNurb(nu);
				testhandlesNurb(nu); /* test for bezier too */

				nu= nu->next;
			}
		}
		else if (ob->type==OB_LATTICE) {
			Lattice *lt= ob->data;
			BPoint *bp;
			int a;
			
			a= lt->editlatt->latt->pntsu*lt->editlatt->latt->pntsv*lt->editlatt->latt->pntsw;
			bp= lt->editlatt->latt->def;
			while (a--) {
				if (bp->f1 & SELECT) {
					add_v3_v3(bp->vec, median);
					bp->weight+= median[4];
				}
				bp++;
			}
		}
		
//		ED_undo_push(C, "Transform properties");
	}
}
#define B_VGRP_PNL_COPY 1
#define B_VGRP_PNL_NORMALIZE 2
#define B_VGRP_PNL_EDIT_SINGLE 8 /* or greater */
#define B_VGRP_PNL_COPY_SINGLE 16384 /* or greater */

static void act_vert_def(Object *ob, BMVert **eve, MDeformVert **dvert)
{
	if (ob && ob->mode & OB_MODE_EDIT && ob->type==OB_MESH && ob->defbase.first) {
		Mesh *me= ob->data;
		BMEditMesh *em = me->edit_btmesh;
		BMEditSelection *ese = (BMEditSelection *)em->bm->selected.last;

		if (ese && ese->htype == BM_VERT) {
			*eve = (BMVert *)ese->ele;
			*dvert = CustomData_bmesh_get(&em->bm->vdata, (*eve)->head.data, CD_MDEFORMVERT);
			return;
		}
	}

	*eve= NULL;
	*dvert= NULL;
}

static void editvert_mirror_update(Object *ob, BMVert *eve, int def_nr, int index)
{
	Mesh *me= ob->data;
	BMEditMesh *em = me->edit_btmesh;
	BMVert *eve_mirr;

	eve_mirr= editbmesh_get_x_mirror_vert(ob, em, eve, eve->co, index);

	if (eve_mirr && eve_mirr != eve) {
		MDeformVert *dvert_src= CustomData_bmesh_get(&em->bm->vdata, eve->head.data, CD_MDEFORMVERT);
		MDeformVert *dvert_dst= CustomData_bmesh_get(&em->bm->vdata, eve_mirr->head.data, CD_MDEFORMVERT);
		if (dvert_dst) {
			if (def_nr == -1) {
				/* all vgroups, add groups where neded  */
				int flip_map_len;
				int *flip_map= defgroup_flip_map(ob, &flip_map_len, TRUE);
				defvert_sync_mapped(dvert_dst, dvert_src, flip_map, flip_map_len, TRUE);
				MEM_freeN(flip_map);
			}
			else {
				/* single vgroup */
				MDeformWeight *dw= defvert_verify_index(dvert_dst, defgroup_flip_index(ob, def_nr, 1));
				if (dw) {
					dw->weight= defvert_find_weight(dvert_src, def_nr);
				}
			}
		}
	}
}

static void vgroup_adjust_active(Object *ob, int def_nr)
{
	BMVert *eve_act;
	MDeformVert *dvert_act;

	act_vert_def(ob, &eve_act, &dvert_act);

	if (dvert_act) {
		if (((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X)
			editvert_mirror_update(ob, eve_act, def_nr, -1);
	}
}

static void vgroup_copy_active_to_sel(Object *ob)
{
	BMVert *eve_act;
	MDeformVert *dvert_act;

	act_vert_def(ob, &eve_act, &dvert_act);

	if (dvert_act==NULL) {
		return;
	}
	else {
		Mesh *me= ob->data;
		BMEditMesh *em = me->edit_btmesh;
		BMIter iter;
		BMVert *eve;
		MDeformVert *dvert;
		int index= 0;

		BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && eve != eve_act) {
				dvert= CustomData_bmesh_get(&em->bm->vdata, eve->head.data, CD_MDEFORMVERT);
				if (dvert) {
					defvert_copy(dvert, dvert_act);

					if (me->editflag & ME_EDIT_MIRROR_X)
						editvert_mirror_update(ob, eve, -1, index);

				}
			}

			index++;
		}
	}
}

static void vgroup_copy_active_to_sel_single(Object *ob, const int def_nr)
{
	BMVert *eve_act;
	MDeformVert *dv_act;

	act_vert_def(ob, &eve_act, &dv_act);

	if (dv_act==NULL) {
		return;
	}
	else {
		Mesh *me= ob->data;
		BMEditMesh *em = me->edit_btmesh;
		BMIter iter;
		BMVert *eve;
		MDeformVert *dv;
		MDeformWeight *dw;
		float weight_act;
		int index= 0;

		dw= defvert_find_index(dv_act, def_nr);

		if (dw == NULL)
			return;
		
		weight_act= dw->weight;

		eve = BM_iter_new(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
		for (index=0; eve; eve=BM_iter_step(&iter), index++) {
			if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && eve != eve_act) {
				dv= CustomData_bmesh_get(&em->bm->vdata, eve->head.data, CD_MDEFORMVERT);
				dw= defvert_find_index(dv, def_nr);
				if (dw) {
					dw->weight= weight_act;

					if (me->editflag & ME_EDIT_MIRROR_X) {
						editvert_mirror_update(ob, eve, -1, index);
					}
				}
			}
		}

		if (me->editflag & ME_EDIT_MIRROR_X) {
			editvert_mirror_update(ob, eve_act, -1, -1);
		}
	}
}

static void vgroup_normalize_active(Object *ob)
{
	BMVert *eve_act;
	MDeformVert *dvert_act;

	act_vert_def(ob, &eve_act, &dvert_act);

	if (dvert_act==NULL)
		return;

	defvert_normalize(dvert_act);

	if (((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X)
		editvert_mirror_update(ob, eve_act, -1, -1);



}

static void do_view3d_vgroup_buttons(bContext *C, void *UNUSED(arg), int event)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= OBACT;

	if (event==B_VGRP_PNL_NORMALIZE) {
		vgroup_normalize_active(ob);
	}
	else if (event == B_VGRP_PNL_COPY) {
		vgroup_copy_active_to_sel(ob);
	}
	else if (event >= B_VGRP_PNL_COPY_SINGLE) {
		vgroup_copy_active_to_sel_single(ob, event - B_VGRP_PNL_COPY_SINGLE);
	}
	else if (event >= B_VGRP_PNL_EDIT_SINGLE) {
		vgroup_adjust_active(ob, event - B_VGRP_PNL_EDIT_SINGLE);
	}

//  todo
//	if (((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X)
//		ED_vgroup_mirror(ob, 1, 1, 0);

	/* default for now */
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);
}

static int view3d_panel_vgroup_poll(const bContext *C, PanelType *UNUSED(pt))
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= OBACT;
	BMVert *eve_act;
	MDeformVert *dvert_act;

	act_vert_def(ob, &eve_act, &dvert_act);

	return dvert_act ? dvert_act->totweight : 0;
}


static void view3d_panel_vgroup(const bContext *C, Panel *pa)
{
	uiBlock *block= uiLayoutAbsoluteBlock(pa->layout);
	Scene *scene= CTX_data_scene(C);
	Object *ob= OBACT;

	BMVert *eve;
	MDeformVert *dv;

	act_vert_def(ob, &eve, &dv);

	if (dv && dv->totweight) {
		uiLayout *col;
		bDeformGroup *dg;
		MDeformWeight *dw = dv->dw;
		unsigned int i;
		int yco = 0;

		uiBlockSetHandleFunc(block, do_view3d_vgroup_buttons, NULL);

		col= uiLayoutColumn(pa->layout, 0);
		block= uiLayoutAbsoluteBlock(col);

		uiBlockBeginAlign(block);

		for (i= dv->totweight; i != 0; i--, dw++) {
			dg = BLI_findlink (&ob->defbase, dw->def_nr);
			if (dg) {
				uiDefButF(block, NUM, B_VGRP_PNL_EDIT_SINGLE + dw->def_nr, dg->name,	0, yco, 180, 20, &dw->weight, 0.0, 1.0, 1, 3, "");
				uiDefBut(block, BUT, B_VGRP_PNL_COPY_SINGLE + dw->def_nr, "C", 180,yco,20,20, NULL, 0, 0, 0, 0, "Copy this groups weight to other selected verts");
				yco -= 20;
			}
		}
		yco-=2;

		uiBlockEndAlign(block);
		uiBlockBeginAlign(block);
		uiDefBut(block, BUT, B_VGRP_PNL_NORMALIZE, "Normalize", 0, yco,100,20, NULL, 0, 0, 0, 0, "Normalize active vertex weights");
		uiDefBut(block, BUT, B_VGRP_PNL_COPY, "Copy", 100,yco,100,20, NULL, 0, 0, 0, 0, "Copy active vertex to other seleted verts");
		uiBlockEndAlign(block);
	}
}

static void v3d_transform_butsR(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *split, *colsub;
	
	split = uiLayoutSplit(layout, 0.8, 0);
	
	if (ptr->type == &RNA_PoseBone) {
		PointerRNA boneptr;
		Bone *bone;
		
		boneptr = RNA_pointer_get(ptr, "bone");
		bone = boneptr.data;
		uiLayoutSetActive(split, !(bone->parent && bone->flag & BONE_CONNECTED));
	}
	colsub = uiLayoutColumn(split, 1);
	uiItemR(colsub, ptr, "location", 0, "Location", ICON_NONE);
	colsub = uiLayoutColumn(split, 1);
	uiItemL(colsub, "", ICON_NONE);
	uiItemR(colsub, ptr, "lock_location", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
	
	split = uiLayoutSplit(layout, 0.8, 0);
	
	switch(RNA_enum_get(ptr, "rotation_mode")) {
		case ROT_MODE_QUAT: /* quaternion */
			colsub = uiLayoutColumn(split, 1);
			uiItemR(colsub, ptr, "rotation_quaternion", 0, "Rotation", ICON_NONE);
			colsub = uiLayoutColumn(split, 1);
			uiItemR(colsub, ptr, "lock_rotations_4d", UI_ITEM_R_TOGGLE, "4L", ICON_NONE);
			if (RNA_boolean_get(ptr, "lock_rotations_4d"))
				uiItemR(colsub, ptr, "lock_rotation_w", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			else
				uiItemL(colsub, "", ICON_NONE);
			uiItemR(colsub, ptr, "lock_rotation", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			break;
		case ROT_MODE_AXISANGLE: /* axis angle */
			colsub = uiLayoutColumn(split, 1);
			uiItemR(colsub, ptr, "rotation_axis_angle", 0, "Rotation", ICON_NONE);
			colsub = uiLayoutColumn(split, 1);
			uiItemR(colsub, ptr, "lock_rotations_4d", UI_ITEM_R_TOGGLE, "4L", ICON_NONE);
			if (RNA_boolean_get(ptr, "lock_rotations_4d"))
				uiItemR(colsub, ptr, "lock_rotation_w", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			else
				uiItemL(colsub, "", ICON_NONE);
			uiItemR(colsub, ptr, "lock_rotation", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			break;
		default: /* euler rotations */
			colsub = uiLayoutColumn(split, 1);
			uiItemR(colsub, ptr, "rotation_euler", 0, "Rotation", ICON_NONE);
			colsub = uiLayoutColumn(split, 1);
			uiItemL(colsub, "", ICON_NONE);
			uiItemR(colsub, ptr, "lock_rotation", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			break;
	}
	uiItemR(layout, ptr, "rotation_mode", 0, "", ICON_NONE);
	
	split = uiLayoutSplit(layout, 0.8, 0);
	colsub = uiLayoutColumn(split, 1);
	uiItemR(colsub, ptr, "scale", 0, "Scale", ICON_NONE);
	colsub = uiLayoutColumn(split, 1);
	uiItemL(colsub, "", ICON_NONE);
	uiItemR(colsub, ptr, "lock_scale", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
	
	if (ptr->type == &RNA_Object) {
		Object *ob = ptr->data;
		/* dimensions and material support just happen to be the same checks
		 * later we may want to add dimensions for lattice, armature etc too */
		if (OB_TYPE_SUPPORT_MATERIAL(ob->type)) {
			uiItemR(layout, ptr, "dimensions", 0, "Dimensions", ICON_NONE);
		}
	}
}

static void v3d_posearmature_buts(uiLayout *layout, Object *ob)
{
//	uiBlock *block= uiLayoutGetBlock(layout);
//	bArmature *arm;
	bPoseChannel *pchan;
//	TransformProperties *tfp= v3d->properties_storage;
	PointerRNA pchanptr;
	uiLayout *col;
//	uiLayout *row;
//	uiBut *but;

	pchan= get_active_posechannel(ob);

//	row= uiLayoutRow(layout, 0);
	
	if (!pchan)	{
		uiItemL(layout, "No Bone Active", ICON_NONE);
		return; 
	}

	RNA_pointer_create(&ob->id, &RNA_PoseBone, pchan, &pchanptr);

	col= uiLayoutColumn(layout, 0);
	
	/* XXX: RNA buts show data in native types (i.e. quats, 4-component axis/angle, etc.)
	 * but oldskool UI shows in eulers always. Do we want to be able to still display in Eulers?
	 * Maybe needs RNA/ui options to display rotations as different types... */
	v3d_transform_butsR(col, &pchanptr);

#if 0
	uiLayoutAbsoluteBlock(layout);

	if (pchan->rotmode == ROT_MODE_AXISANGLE) {
		float quat[4];
		/* convert to euler, passing through quats... */
		axis_angle_to_quat(quat, pchan->rotAxis, pchan->rotAngle);
		quat_to_eul( tfp->ob_eul,quat);
	}
	else if (pchan->rotmode == ROT_MODE_QUAT)
		quat_to_eul( tfp->ob_eul,pchan->quat);
	else
		copy_v3_v3(tfp->ob_eul, pchan->eul);
	tfp->ob_eul[0]*= RAD2DEGF(1.0f);
	tfp->ob_eul[1]*= RAD2DEGF(1.0f);
	tfp->ob_eul[2]*= RAD2DEGF(1.0f);
	
	uiDefBut(block, LABEL, 0, "Location:",			0, 240, 100, 20, 0, 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	
	but= uiDefButF(block, NUM, B_ARMATUREPANEL2, "X:",	0, 220, 120, 19, pchan->loc, -lim, lim, 100, 3, "");
	uiButSetUnitType(but, PROP_UNIT_LENGTH);
	but= uiDefButF(block, NUM, B_ARMATUREPANEL2, "Y:",	0, 200, 120, 19, pchan->loc+1, -lim, lim, 100, 3, "");
	uiButSetUnitType(but, PROP_UNIT_LENGTH);
	but= uiDefButF(block, NUM, B_ARMATUREPANEL2, "Z:",	0, 180, 120, 19, pchan->loc+2, -lim, lim, 100, 3, "");
	uiButSetUnitType(but, PROP_UNIT_LENGTH);
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_LOCX, B_REDR, ICON_UNLOCKED,	125, 220, 25, 19, &(pchan->protectflag), 0, 0, 0, 0, "Protects X Location value from being Transformed");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_LOCY, B_REDR, ICON_UNLOCKED,	125, 200, 25, 19, &(pchan->protectflag), 0, 0, 0, 0, "Protects Y Location value from being Transformed");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_LOCZ, B_REDR, ICON_UNLOCKED,	125, 180, 25, 19, &(pchan->protectflag), 0, 0, 0, 0, "Protects Z Location value from being Transformed");
	uiBlockEndAlign(block);
	
	uiDefBut(block, LABEL, 0, "Rotation:",			0, 160, 100, 20, 0, 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL3, "X:",	0, 140, 120, 19, tfp->ob_eul, -1000.0, 1000.0, 100, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL3, "Y:",	0, 120, 120, 19, tfp->ob_eul+1, -1000.0, 1000.0, 100, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL3, "Z:",	0, 100, 120, 19, tfp->ob_eul+2, -1000.0, 1000.0, 100, 3, "");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTX, B_REDR, ICON_UNLOCKED,	125, 140, 25, 19, &(pchan->protectflag), 0, 0, 0, 0, "Protects X Rotation value from being Transformed");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTY, B_REDR, ICON_UNLOCKED,	125, 120, 25, 19, &(pchan->protectflag), 0, 0, 0, 0, "Protects Y Rotation value from being Transformed");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTZ, B_REDR, ICON_UNLOCKED,	125, 100, 25, 19, &(pchan->protectflag), 0, 0, 0, 0, "Protects Z Rotation value from being Transformed");
	uiBlockEndAlign(block);
	
	uiDefBut(block, LABEL, 0, "Scale:",				0, 80, 100, 20, 0, 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "X:",	0, 60, 120, 19, pchan->size, -lim, lim, 10, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "Y:",	0, 40, 120, 19, pchan->size+1, -lim, lim, 10, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "Z:",	0, 20, 120, 19, pchan->size+2, -lim, lim, 10, 3, "");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_SCALEX, B_REDR, ICON_UNLOCKED,	125, 60, 25, 19, &(pchan->protectflag), 0, 0, 0, 0, "Protects X Scale value from being Transformed");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_SCALEY, B_REDR, ICON_UNLOCKED,	125, 40, 25, 19, &(pchan->protectflag), 0, 0, 0, 0, "Protects Y Scale value from being Transformed");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_SCALEZ, B_REDR, ICON_UNLOCKED,	125, 20, 25, 19, &(pchan->protectflag), 0, 0, 0, 0, "Protects z Scale value from being Transformed");
	uiBlockEndAlign(block);
#endif
}

/* assumes armature editmode */
#if 0
static void validate_editbonebutton_cb(bContext *C, void *bonev, void *namev)
{
	EditBone *eBone= bonev;
	char oldname[sizeof(eBone->name)], newname[sizeof(eBone->name)];

	/* need to be on the stack */
	BLI_strncpy(newname, eBone->name, sizeof(eBone->name));
	BLI_strncpy(oldname, (char *)namev, sizeof(eBone->name));
	/* restore */
	BLI_strncpy(eBone->name, oldname, sizeof(eBone->name));

	ED_armature_bone_rename(CTX_data_edit_object(C)->data, oldname, newname); // editarmature.c
	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, CTX_data_edit_object(C)); // XXX fix
}
#endif

static void v3d_editarmature_buts(uiLayout *layout, Object *ob)
{
//	uiBlock *block= uiLayoutGetBlock(layout);
	bArmature *arm= ob->data;
	EditBone *ebone;
//	TransformProperties *tfp= v3d->properties_storage;
//	uiLayout *row;
	uiLayout *col;
	PointerRNA eboneptr;
	
	ebone= arm->act_edbone;

	if (!ebone || (ebone->layer & arm->layer)==0) {
		uiItemL(layout, "Nothing selected", ICON_NONE);
		return;
	}
//	row= uiLayoutRow(layout, 0);
	RNA_pointer_create(&arm->id, &RNA_EditBone, ebone, &eboneptr);

	col= uiLayoutColumn(layout, 0);
	uiItemR(col, &eboneptr, "head", 0, "Head", ICON_NONE);
	if (ebone->parent && ebone->flag & BONE_CONNECTED ) {
		PointerRNA parptr = RNA_pointer_get(&eboneptr, "parent");
		uiItemR(col, &parptr, "tail_radius", 0, "Radius (Parent)", ICON_NONE);
	}
	else {
		uiItemR(col, &eboneptr, "head_radius", 0, "Radius", ICON_NONE);
	}
	
	uiItemR(col, &eboneptr, "tail", 0, "Tail", ICON_NONE);
	uiItemR(col, &eboneptr, "tail_radius", 0, "Radius", ICON_NONE);
	
	uiItemR(col, &eboneptr, "roll", 0, "Roll", ICON_NONE);
	uiItemR(col, &eboneptr, "envelope_distance", 0, "Envelope", ICON_NONE);
}

static void v3d_editmetaball_buts(uiLayout *layout, Object *ob)
{
	PointerRNA mbptr, ptr;
	MetaBall *mball= ob->data;
//	uiLayout *row;
	uiLayout *col;
	
	if (!mball || !(mball->lastelem)) return;
	
	RNA_pointer_create(&mball->id, &RNA_MetaBall, mball, &mbptr);
	
//	row= uiLayoutRow(layout, 0);

	RNA_pointer_create(&mball->id, &RNA_MetaElement, mball->lastelem, &ptr);
	
	col= uiLayoutColumn(layout, 0);
	uiItemR(col, &ptr, "co", 0, "Location", ICON_NONE);
	
	uiItemR(col, &ptr, "radius", 0, "Radius", ICON_NONE);
	uiItemR(col, &ptr, "stiffness", 0, "Stiffness", ICON_NONE);
	
	uiItemR(col, &ptr, "type", 0, "Type", ICON_NONE);
	
	col= uiLayoutColumn(layout, 1);
	switch (RNA_enum_get(&ptr, "type")) {
		case MB_BALL:
			break;
		case MB_CUBE:
			uiItemL(col, "Size:", ICON_NONE);
			uiItemR(col, &ptr, "size_x", 0, "X", ICON_NONE);
			uiItemR(col, &ptr, "size_y", 0, "Y", ICON_NONE);
			uiItemR(col, &ptr, "size_z", 0, "Z", ICON_NONE);
			break;
		case MB_TUBE:
			uiItemL(col, "Size:", ICON_NONE);
			uiItemR(col, &ptr, "size_x", 0, "X", ICON_NONE);
			break;
		case MB_PLANE:
			uiItemL(col, "Size:", ICON_NONE);
			uiItemR(col, &ptr, "size_x", 0, "X", ICON_NONE);
			uiItemR(col, &ptr, "size_y", 0, "Y", ICON_NONE);
			break;
		case MB_ELIPSOID:
			uiItemL(col, "Size:", ICON_NONE);
			uiItemR(col, &ptr, "size_x", 0, "X", ICON_NONE);
			uiItemR(col, &ptr, "size_y", 0, "Y", ICON_NONE);
			uiItemR(col, &ptr, "size_z", 0, "Z", ICON_NONE);
			break;		   
	}	
}

static void do_view3d_region_buttons(bContext *C, void *UNUSED(index), int event)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
//	Object *obedit= CTX_data_edit_object(C);
	View3D *v3d= CTX_wm_view3d(C);
//	BoundBox *bb;
	Object *ob= OBACT;
	TransformProperties *tfp= v3d->properties_storage;
	
	switch(event) {
	
	case B_REDR:
		ED_area_tag_redraw(CTX_wm_area(C));
		return; /* no notifier! */
		
	case B_OBJECTPANEL:
		DAG_id_tag_update(&ob->id, OB_RECALC_OB);
		break;

	
	case B_OBJECTPANELMEDIAN:
		if (ob) {
			v3d_editvertex_buts(NULL, v3d, ob, 1.0);
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		}
		break;
		
		/* note; this case also used for parbone */
	case B_OBJECTPANELPARENT:
		if (ob) {
			if (ob->id.lib || BKE_object_parent_loop_check(ob->parent, ob))
				ob->parent= NULL;
			else {
				DAG_scene_sort(bmain, scene);
				DAG_id_tag_update(&ob->id, OB_RECALC_OB);
			}
		}
		break;
		

	case B_ARMATUREPANEL3:  // rotate button on channel
		{
			bPoseChannel *pchan;
			float eul[3];
			
			pchan= get_active_posechannel(ob);
			if (!pchan) return;
			
			/* make a copy to eul[3], to allow TAB on buttons to work */
			eul[0]= DEG2RADF(tfp->ob_eul[0]);
			eul[1]= DEG2RADF(tfp->ob_eul[1]);
			eul[2]= DEG2RADF(tfp->ob_eul[2]);
			
			if (pchan->rotmode == ROT_MODE_AXISANGLE) {
				float quat[4];
				/* convert to axis-angle, passing through quats  */
				eul_to_quat( quat,eul);
				quat_to_axis_angle( pchan->rotAxis, &pchan->rotAngle,quat);
			}
			else if (pchan->rotmode == ROT_MODE_QUAT)
				eul_to_quat( pchan->quat,eul);
			else
				copy_v3_v3(pchan->eul, eul);
		}
		/* no break, pass on */
	case B_ARMATUREPANEL2:
		{
			ob->pose->flag |= (POSE_LOCKED|POSE_DO_UNLOCK);
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		}
		break;
	case B_TRANSFORMSPACEADD:
	{
		char names[sizeof(((TransformOrientation *)NULL)->name)]= "";
		BIF_createTransformOrientation(C, NULL, names, 1, 0);
		break;
	}
	case B_TRANSFORMSPACECLEAR:
		BIF_clearTransformOrientation(C);
		break;
		
#if 0 // XXX
	case B_WEIGHT0_0:
		wpaint->weight = 0.0f;
		break;
		
	case B_WEIGHT1_4:
		wpaint->weight = 0.25f;
		break;
	case B_WEIGHT1_2:
		wpaint->weight = 0.5f;
		break;
	case B_WEIGHT3_4:
		wpaint->weight = 0.75f;
		break;
	case B_WEIGHT1_0:
		wpaint->weight = 1.0f;
		break;
		
	case B_OPA1_8:
		wpaint->a = 0.125f;
		break;
	case B_OPA1_4:
		wpaint->a = 0.25f;
		break;
	case B_OPA1_2:
		wpaint->a = 0.5f;
		break;
	case B_OPA3_4:
		wpaint->a = 0.75f;
		break;
	case B_OPA1_0:
		wpaint->a = 1.0f;
		break;
#endif
	case B_CLR_WPAINT:
//		if (!multires_level1_test()) {
		{
			bDeformGroup *defGroup = BLI_findlink(&ob->defbase, ob->actdef-1);
			if (defGroup) {
				Mesh *me= ob->data;
				int a;
				for (a=0; a<me->totvert; a++)
					ED_vgroup_vert_remove (ob, defGroup, a);
				DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			}
		}
		break;
	case B_RV3D_LOCKED:
	case B_RV3D_BOXVIEW:
	case B_RV3D_BOXCLIP:
		{
			ScrArea *sa= CTX_wm_area(C);
			ARegion *ar= sa->regionbase.last;
			RegionView3D *rv3d;
			short viewlock;
			
			ar= ar->prev;
			rv3d= ar->regiondata;
			viewlock= rv3d->viewlock;
			
			if ((viewlock & RV3D_LOCKED)==0)
				viewlock= 0;
			else if ((viewlock & RV3D_BOXVIEW)==0)
				viewlock &= ~RV3D_BOXCLIP;
			
			for (; ar; ar= ar->prev) {
				if (ar->alignment==RGN_ALIGN_QSPLIT) {
					rv3d= ar->regiondata;
					rv3d->viewlock= viewlock;
				}
			}
			
			if (rv3d->viewlock & RV3D_BOXVIEW)
				view3d_boxview_copy(sa, sa->regionbase.last);
			
			ED_area_tag_redraw(sa);
		}
		break;
	}

	/* default for now */
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_VIEW3D, v3d);
}

static void view3d_panel_object(const bContext *C, Panel *pa)
{
	uiBlock *block;
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	View3D *v3d= CTX_wm_view3d(C);
	//uiBut *bt;
	Object *ob= OBACT;
	// TransformProperties *tfp; // UNUSED
	PointerRNA obptr;
	uiLayout *col /* , *row */ /* UNUSED */;
	float lim;
	
	if (ob==NULL) return;

	/* make sure we got storage */
	/*
	if (v3d->properties_storage==NULL)
		v3d->properties_storage= MEM_callocN(sizeof(TransformProperties), "TransformProperties");
	tfp= v3d->properties_storage;
	
// XXX	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);

	if (ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT)) {
	}
	else {
		if ((ob->mode & OB_MODE_PARTICLE_EDIT)==0) {
			uiBlockEndAlign(block);
		}
	}
	*/

	lim= 10000.0f * MAX2(1.0f, v3d->grid);

	block= uiLayoutGetBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_view3d_region_buttons, NULL);

	col= uiLayoutColumn(pa->layout, 0);
	/* row= uiLayoutRow(col, 0); */ /* UNUSED */
	RNA_id_pointer_create(&ob->id, &obptr);

	if (ob==obedit) {
		if (ob->type==OB_ARMATURE) v3d_editarmature_buts(col, ob);
		else if (ob->type==OB_MBALL) v3d_editmetaball_buts(col, ob);
		else v3d_editvertex_buts(col, v3d, ob, lim);
	}
	else if (ob->mode & OB_MODE_POSE) {
		v3d_posearmature_buts(col, ob);
	}
	else {

		v3d_transform_butsR(col, &obptr);
	}
}

#if 0
static void view3d_panel_preview(bContext *C, ARegion *ar, short cntrl)	// VIEW3D_HANDLER_PREVIEW
{
	uiBlock *block;
	View3D *v3d= sa->spacedata.first;
	int ofsx, ofsy;
	
	block= uiBeginBlock(C, ar, __func__, UI_EMBOSS);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | UI_PNL_SCALE | cntrl);
	uiSetPanelHandler(VIEW3D_HANDLER_PREVIEW);  // for close and esc
	
	ofsx= -150+(sa->winx/2)/v3d->blockscale;
	ofsy= -100+(sa->winy/2)/v3d->blockscale;
	if (uiNewPanel(C, ar, block, "Preview", "View3d", ofsx, ofsy, 300, 200)==0) return;

	uiBlockSetDrawExtraFunc(block, BIF_view3d_previewdraw);
	
	if (scene->recalc & SCE_PRV_CHANGED) {
		scene->recalc &= ~SCE_PRV_CHANGED;
		//printf("found recalc\n");
		BIF_view3d_previewrender_free(sa->spacedata.first);
		BIF_preview_changed(0);
	}
}
#endif

void view3d_buttons_register(ARegionType *art)
{
	PanelType *pt;

	pt= MEM_callocN(sizeof(PanelType), "spacetype view3d panel object");
	strcpy(pt->idname, "VIEW3D_PT_object");
	strcpy(pt->label, "Transform");
	pt->draw= view3d_panel_object;
	BLI_addtail(&art->paneltypes, pt);
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype view3d panel gpencil");
	strcpy(pt->idname, "VIEW3D_PT_gpencil");
	strcpy(pt->label, "Grease Pencil");
	pt->draw= gpencil_panel_standard;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype view3d panel vgroup");
	strcpy(pt->idname, "VIEW3D_PT_vgroup");
	strcpy(pt->label, "Vertex Groups");
	pt->draw= view3d_panel_vgroup;
	pt->poll= view3d_panel_vgroup_poll;
	BLI_addtail(&art->paneltypes, pt);

	// XXX view3d_panel_preview(C, ar, 0);
}

static int view3d_properties(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= view3d_has_buttons_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_properties(wmOperatorType *ot)
{
	ot->name= "Properties";
	ot->description= "Toggles the properties panel display";
	ot->idname= "VIEW3D_OT_properties";
	
	ot->exec= view3d_properties;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= 0;
}
