/**
 * $Id$
 *
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

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_action.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_idprop.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"
#include "BKE_deform.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_armature.h"
#include "ED_gpencil.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"

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
	float ve_median[6];
	int curdef;
	float *defweightp;
} TransformProperties;


/* is used for both read and write... */
static void v3d_editvertex_buts(const bContext *C, uiLayout *layout, View3D *v3d, Object *ob, float lim)
{
	uiBlock *block= (layout)? uiLayoutAbsoluteBlock(layout): NULL;
	MDeformVert *dvert=NULL;
	TransformProperties *tfp= v3d->properties_storage;
	float median[6], ve_median[6];
	int tot, totw, totweight, totedge, totradius;
	char defstr[320];
	
	median[0]= median[1]= median[2]= median[3]= median[4]= median[5]= 0.0;
	tot= totw= totweight= totedge= totradius= 0;
	defstr[0]= 0;

	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		EditMesh *em = BKE_mesh_get_editmesh(me);
		EditVert *eve, *evedef=NULL;
		EditEdge *eed;
		
		eve= em->verts.first;
		while(eve) {
			if(eve->f & SELECT) {
				evedef= eve;
				tot++;
				add_v3_v3(median, eve->co);
			}
			eve= eve->next;
		}
		eed= em->edges.first;
		while(eed) {
			if((eed->f & SELECT)) {
				totedge++;
				median[3]+= eed->crease;
			}
			eed= eed->next;
		}

		/* check for defgroups */
		if(evedef)
			dvert= CustomData_em_get(&em->vdata, evedef->data, CD_MDEFORMVERT);
		if(tot==1 && dvert && dvert->totweight) {
			bDeformGroup *dg;
			int i, max=1, init=1;
			char str[320];
			
			for (i=0; i<dvert->totweight; i++){
				dg = BLI_findlink (&ob->defbase, dvert->dw[i].def_nr);
				if(dg) {
					max+= BLI_snprintf(str, sizeof(str), "%s %%x%d|", dg->name, dvert->dw[i].def_nr); 
					if(max<320) strcat(defstr, str);
				}

				if(tfp->curdef==dvert->dw[i].def_nr) {
					init= 0;
					tfp->defweightp= &dvert->dw[i].weight;
				}
			}
			
			if(init) {	// needs new initialized 
				tfp->curdef= dvert->dw[0].def_nr;
				tfp->defweightp= &dvert->dw[0].weight;
			}
		}

		BKE_mesh_end_editmesh(me, em);
	}
	else if(ob->type==OB_CURVE || ob->type==OB_SURF) {
		Curve *cu= ob->data;
		Nurb *nu;
		BPoint *bp;
		BezTriple *bezt;
		int a;
		
		nu= cu->editnurb->first;
		while(nu) {
			if(nu->type == CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(bezt->f2 & SELECT) {
						add_v3_v3(median, bezt->vec[1]);
						tot++;
						median[4]+= bezt->weight;
						totweight++;
						median[5]+= bezt->radius;
						totradius++;
					}
					else {
						if(bezt->f1 & SELECT) {
							add_v3_v3(median, bezt->vec[0]);
							tot++;
						}
						if(bezt->f3 & SELECT) {
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
				while(a--) {
					if(bp->f1 & SELECT) {
						add_v3_v3(median, bp->vec);
						median[3]+= bp->vec[3];
						totw++;
						tot++;
						median[4]+= bp->weight;
						totweight++;
						median[5]+= bp->radius;
						totradius++;
					}
					bp++;
				}
			}
			nu= nu->next;
		}
	}
	else if(ob->type==OB_LATTICE) {
		Lattice *lt= ob->data;
		BPoint *bp;
		int a;
		
		a= lt->editlatt->pntsu*lt->editlatt->pntsv*lt->editlatt->pntsw;
		bp= lt->editlatt->def;
		while(a--) {
			if(bp->f1 & SELECT) {
				add_v3_v3(median, bp->vec);
				tot++;
				median[4]+= bp->weight;
				totweight++;
			}
			bp++;
		}
	}
	
	if(tot==0) return;

	median[0] /= (float)tot;
	median[1] /= (float)tot;
	median[2] /= (float)tot;
	if(totedge) median[3] /= (float)totedge;
	else if(totw) median[3] /= (float)totw;
	if(totweight) median[4] /= (float)totweight;
	if(totradius) median[5] /= (float)totradius;
	
	if(v3d->flag & V3D_GLOBAL_STATS)
		mul_m4_v3(ob->obmat, median);
	
	if(block) {	// buttons
		int but_y;
		if((ob->parent) && (ob->partype == PARBONE))	but_y = 135;
		else											but_y = 150;
		
		
		
		memcpy(tfp->ve_median, median, sizeof(tfp->ve_median));
		
		uiBlockBeginAlign(block);
		if(tot==1) {
			uiDefBut(block, LABEL, 0, "Vertex:",					0, 130, 200, 20, 0, 0, 0, 0, 0, "");
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "X:",		0, 110, 200, 20, &(tfp->ve_median[0]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Y:",		0, 90, 200, 20, &(tfp->ve_median[1]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Z:",		0, 70, 200, 20, &(tfp->ve_median[2]), -lim, lim, 10, 3, "");
			
			if(totw==1) {
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "W:",	0, 50, 200, 20, &(tfp->ve_median[3]), 0.01, 100.0, 10, 3, "");
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, V3D_GLOBAL_STATS, B_REDR, "Global",		0, 25, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays global values");
				uiDefButBitS(block, TOGN, V3D_GLOBAL_STATS, B_REDR, "Local",		100, 25, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays local values");
				uiBlockEndAlign(block);
				if(totweight)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Weight:",	0, 0, 200, 20, &(tfp->ve_median[4]), 0.0, 1.0, 10, 3, "");
				if(totradius)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Radius:",	0, 0, 200, 20, &(tfp->ve_median[5]), 0.0, 100.0, 10, 3, "Radius of curve CPs");
				}
			else {
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, V3D_GLOBAL_STATS, B_REDR, "Global",		0, 45, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays global values");
				uiDefButBitS(block, TOGN, V3D_GLOBAL_STATS, B_REDR, "Local",		100, 45, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays local values");
				uiBlockEndAlign(block);
				if(totweight)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Weight:",	0, 20, 200, 20, &(tfp->ve_median[4]), 0.0, 1.0, 10, 3, "");
				if(totradius)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Radius:",	0, 20, 200, 20, &(tfp->ve_median[5]), 0.0, 100.0, 10, 3, "Radius of curve CPs");
			}
		}
		else {
			uiDefBut(block, LABEL, 0, "Median:",					0, 130, 200, 20, 0, 0, 0, 0, 0, "");
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "X:",		0, 110, 200, 20, &(tfp->ve_median[0]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Y:",		0, 90, 200, 20, &(tfp->ve_median[1]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Z:",		0, 70, 200, 20, &(tfp->ve_median[2]), -lim, lim, 10, 3, "");
			if(totw==tot) {
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "W:",	0, 50, 200, 20, &(tfp->ve_median[3]), 0.01, 100.0, 10, 3, "");
				uiBlockEndAlign(block);
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, V3D_GLOBAL_STATS, B_REDR, "Global",		0, 25, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays global values");
				uiDefButBitS(block, TOGN, V3D_GLOBAL_STATS, B_REDR, "Local",		100, 25, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays local values");
				uiBlockEndAlign(block);
				if(totweight)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Weight:",	0, 0, 200, 20, &(tfp->ve_median[4]), 0.0, 1.0, 10, 3, "Weight is used for SoftBody Goal");
				if(totradius)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Radius:",	0, 0, 200, 20, &(tfp->ve_median[5]), 0.0, 100.0, 10, 3, "Radius of curve CPs");
				uiBlockEndAlign(block);
			}
			else {
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, V3D_GLOBAL_STATS, B_REDR, "Global",		0, 45, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays global values");
				uiDefButBitS(block, TOGN, V3D_GLOBAL_STATS, B_REDR, "Local",		100, 45, 100, 20, &v3d->flag, 0, 0, 0, 0, "Displays local values");
				uiBlockEndAlign(block);
				if(totweight)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Weight:",	0, 20, 200, 20, &(tfp->ve_median[4]), 0.0, 1.0, 10, 3, "Weight is used for SoftBody Goal");
				if(totradius)
					uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Radius:",	0, 0, 200, 20, &(tfp->ve_median[5]), 0.0, 100.0, 10, 3, "Radius of curve CPs");
				uiBlockEndAlign(block);
			}
		}
				
		if(totedge==1)
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Crease:",	0, 20, 200, 20, &(tfp->ve_median[3]), 0.0, 1.0, 10, 3, "");
		else if(totedge>1)
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Median Crease:",	0, 20, 200, 20, &(tfp->ve_median[3]), 0.0, 1.0, 10, 3, "");
		
	}
	else {	// apply
		memcpy(ve_median, tfp->ve_median, sizeof(tfp->ve_median));
		
		if(v3d->flag & V3D_GLOBAL_STATS) {
			invert_m4_m4(ob->imat, ob->obmat);
			mul_m4_v3(ob->imat, median);
			mul_m4_v3(ob->imat, ve_median);
		}
		sub_v3_v3v3(median, ve_median, median);
		median[3]= ve_median[3]-median[3];
		median[4]= ve_median[4]-median[4];
		median[5]= ve_median[5]-median[5];
		
		if(ob->type==OB_MESH) {
			Mesh *me= ob->data;
			EditMesh *em = BKE_mesh_get_editmesh(me);
			EditVert *eve;
			EditEdge *eed;
			
			eve= em->verts.first;
			while(eve) {
				if(eve->f & SELECT) {
					add_v3_v3(eve->co, median);
				}
				eve= eve->next;
			}
			
			for(eed= em->edges.first; eed; eed= eed->next) {
				if(eed->f & SELECT) {
					/* ensure the median can be set to zero or one */
					if(ve_median[3]==0.0f) eed->crease= 0.0f;
					else if(ve_median[3]==1.0f) eed->crease= 1.0f;
					else {
						eed->crease+= median[3];
						CLAMP(eed->crease, 0.0, 1.0);
					}
				}
			}
			
			recalc_editnormals(em);

			BKE_mesh_end_editmesh(me, em);
		}
		else if(ob->type==OB_CURVE || ob->type==OB_SURF) {
			Curve *cu= ob->data;
			Nurb *nu;
			BPoint *bp;
			BezTriple *bezt;
			int a;
			
			nu= cu->editnurb->first;
			while(nu) {
				if(nu->type == CU_BEZIER) {
					bezt= nu->bezt;
					a= nu->pntsu;
					while(a--) {
						if(bezt->f2 & SELECT) {
							add_v3_v3(bezt->vec[0], median);
							add_v3_v3(bezt->vec[1], median);
							add_v3_v3(bezt->vec[2], median);
							bezt->weight+= median[4];
							bezt->radius+= median[5];
						}
						else {
							if(bezt->f1 & SELECT) {
								add_v3_v3(bezt->vec[0], median);
							}
							if(bezt->f3 & SELECT) {
								add_v3_v3(bezt->vec[2], median);
							}
						}
						bezt++;
					}
				}
				else {
					bp= nu->bp;
					a= nu->pntsu*nu->pntsv;
					while(a--) {
						if(bp->f1 & SELECT) {
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
		else if(ob->type==OB_LATTICE) {
			Lattice *lt= ob->data;
			BPoint *bp;
			int a;
			
			a= lt->editlatt->pntsu*lt->editlatt->pntsv*lt->editlatt->pntsw;
			bp= lt->editlatt->def;
			while(a--) {
				if(bp->f1 & SELECT) {
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

static void act_vert_def(Object *ob, EditVert **eve, MDeformVert **dvert)
{
	if(ob && ob->mode & OB_MODE_EDIT && ob->type==OB_MESH && ob->defbase.first) {
		Mesh *me= ob->data;
		EditMesh *em = BKE_mesh_get_editmesh(me);
		EditSelection *ese = ((EditSelection*)em->selected.last);

		if(ese && ese->type == EDITVERT) {
			*eve= (EditVert*)ese->data;
			*dvert= CustomData_em_get(&em->vdata, (*eve)->data, CD_MDEFORMVERT);
			return;
		}

		BKE_mesh_end_editmesh(me, em);
	}

	*eve= NULL;
	*dvert= NULL;
}

static void editvert_mirror_update(Object *ob, EditVert *eve, int def_nr, int index)
{
	Mesh *me= ob->data;
	EditMesh *em = BKE_mesh_get_editmesh(me);
	EditVert *eve_mirr;

	eve_mirr= editmesh_get_x_mirror_vert(ob, em, eve, eve->co, index);

	if(eve_mirr && eve_mirr != eve) {
		MDeformVert *dvert_src= CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT);
		MDeformVert *dvert_dst= CustomData_em_get(&em->vdata, eve_mirr->data, CD_MDEFORMVERT);
		if(dvert_dst) {
			if(def_nr == -1) {
				/* all vgroups, add groups where neded  */

				int *flip_map= defgroup_flip_map(ob, 1);
				defvert_sync_mapped(dvert_dst, dvert_src, flip_map, 1);
				MEM_freeN(flip_map);
			}
			else {
				/* single vgroup */
				MDeformWeight *dw= defvert_verify_index(dvert_dst, defgroup_flip_index(ob, def_nr, 1));
				if(dw) {
					dw->weight= defvert_find_weight(dvert_src, def_nr);
				}
			}
		}
	}
}

static void vgroup_adjust_active(Object *ob, int def_nr)
{
	EditVert *eve_act;
	MDeformVert *dvert_act;

	act_vert_def(ob, &eve_act, &dvert_act);

	if(dvert_act) {
		if(((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X)
			editvert_mirror_update(ob, eve_act, def_nr, -1);
	}
}

static void vgroup_copy_active_to_sel(Object *ob)
{
	EditVert *eve_act;
	MDeformVert *dvert_act;

	act_vert_def(ob, &eve_act, &dvert_act);

	if(dvert_act==NULL) {
		return;
	}
	else {
		Mesh *me= ob->data;
		EditMesh *em = BKE_mesh_get_editmesh(me);
		EditVert *eve;
		MDeformVert *dvert;
		int index= 0;

		for(eve= em->verts.first; eve; eve= eve->next, index++) {
			if(eve->f & SELECT && eve != eve_act) {
				dvert= CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT);
				if(dvert) {
					defvert_copy(dvert, dvert_act);

					if(me->editflag & ME_EDIT_MIRROR_X)
						editvert_mirror_update(ob, eve, -1, index);

				}
			}
		}
	}
}

static void vgroup_copy_active_to_sel_single(Object *ob, int def_nr)
{
	EditVert *eve_act;
	MDeformVert *dvert_act;

	act_vert_def(ob, &eve_act, &dvert_act);

	if(dvert_act==NULL) {
		return;
	}
	else {
		Mesh *me= ob->data;
		EditMesh *em = BKE_mesh_get_editmesh(me);
		EditVert *eve;
		MDeformVert *dvert;
		MDeformWeight *dw;
		float act_weight = -1.0f;
		int i;
		int index= 0;

		for(i=0, dw=dvert_act->dw; i < dvert_act->totweight; i++, dw++) {
			if(def_nr == dw->def_nr) {
				act_weight= dw->weight;
				break;
			}
		}

		if(act_weight < -0.5f)
			return;

		for(eve= em->verts.first; eve; eve= eve->next, index++) {
			if(eve->f & SELECT && eve != eve_act) {
				dvert= CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT);
				if(dvert) {
					for(i=0, dw=dvert->dw; i < dvert->totweight; i++, dw++) {
						if(def_nr == dw->def_nr) {
							dw->weight= act_weight;

							if(me->editflag & ME_EDIT_MIRROR_X)
								editvert_mirror_update(ob, eve, -1, index);

							break;
						}
					}
				}
			}
		}

		if(me->editflag & ME_EDIT_MIRROR_X)
			editvert_mirror_update(ob, eve_act, -1, -1);

	}
}

static void vgroup_normalize_active(Object *ob)
{
	EditVert *eve_act;
	MDeformVert *dvert_act;

	act_vert_def(ob, &eve_act, &dvert_act);

	if(dvert_act==NULL)
		return;

	defvert_normalize(dvert_act);

	if(((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X)
		editvert_mirror_update(ob, eve_act, -1, -1);



}

static void do_view3d_vgroup_buttons(bContext *C, void *arg, int event)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= OBACT;

	if(event==B_VGRP_PNL_NORMALIZE) {
		vgroup_normalize_active(ob);
	}
	else if(event == B_VGRP_PNL_COPY) {
		vgroup_copy_active_to_sel(ob);
	}
	else if(event >= B_VGRP_PNL_COPY_SINGLE) {
		vgroup_copy_active_to_sel_single(ob, event - B_VGRP_PNL_COPY_SINGLE);
	}
	else if(event >= B_VGRP_PNL_EDIT_SINGLE) {
		vgroup_adjust_active(ob, event - B_VGRP_PNL_EDIT_SINGLE);
	}

//  todo
//	if(((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X)
//		ED_vgroup_mirror(ob, 1, 1, 0);

	/* default for now */
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);
}

int view3d_panel_vgroup_poll(const bContext *C, PanelType *pt)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= OBACT;
	EditVert *eve_act;
	MDeformVert *dvert_act;

	act_vert_def(ob, &eve_act, &dvert_act);

	return dvert_act ? dvert_act->totweight : 0;
}


static void view3d_panel_vgroup(const bContext *C, Panel *pa)
{
	uiBlock *block= uiLayoutAbsoluteBlock(pa->layout);
	Scene *scene= CTX_data_scene(C);
	Object *ob= OBACT;

	EditVert *eve;
	MDeformVert *dvert;

	act_vert_def(ob, &eve, &dvert);

	if(dvert && dvert->totweight) {
		uiLayout *col;
		bDeformGroup *dg;
		int i;
		int yco = 0;

		uiBlockSetHandleFunc(block, do_view3d_vgroup_buttons, NULL);

		col= uiLayoutColumn(pa->layout, 0);
		block= uiLayoutAbsoluteBlock(col);

		uiBlockBeginAlign(block);

		for (i=0; i<dvert->totweight; i++){
			dg = BLI_findlink (&ob->defbase, dvert->dw[i].def_nr);
			if(dg) {
				uiDefButF(block, NUM, B_VGRP_PNL_EDIT_SINGLE + dvert->dw[i].def_nr, dg->name,	0, yco, 180, 20, &dvert->dw[i].weight, 0.0, 1.0, 1, 3, "");
				uiDefBut(block, BUT, B_VGRP_PNL_COPY_SINGLE + dvert->dw[i].def_nr, "C", 180,yco,20,20, 0, 0, 0, 0, 0, "Copy this groups weight to other selected verts");
				yco -= 20;
			}
		}
		yco-=2;

		uiBlockEndAlign(block);
		uiBlockBeginAlign(block);
		uiDefBut(block, BUT, B_VGRP_PNL_NORMALIZE, "Normalize", 0, yco,100,20, 0, 0, 0, 0, 0, "Normalize active vertex weights");
		uiDefBut(block, BUT, B_VGRP_PNL_COPY, "Copy", 100,yco,100,20, 0, 0, 0, 0, 0, "Copy active vertex to other seleted verts");
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
	uiItemR(colsub, ptr, "location", 0, "Location", 0);
	colsub = uiLayoutColumn(split, 1);
	uiItemL(colsub, "", 0);
	uiItemR(colsub, ptr, "lock_location", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_LOCKED);
	
	split = uiLayoutSplit(layout, 0.8, 0);
	
	switch(RNA_enum_get(ptr, "rotation_mode")) {
		case ROT_MODE_QUAT: /* quaternion */
			colsub = uiLayoutColumn(split, 1);
			uiItemR(colsub, ptr, "rotation_quaternion", 0, "Rotation", 0);
			colsub = uiLayoutColumn(split, 1);
			uiItemR(colsub, ptr, "lock_rotations_4d", UI_ITEM_R_TOGGLE, "4L", 0);
			if (RNA_boolean_get(ptr, "lock_rotations_4d"))
				uiItemR(colsub, ptr, "lock_rotation_w", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_LOCKED);
			else
				uiItemL(colsub, "", 0);
			uiItemR(colsub, ptr, "lock_rotation", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_LOCKED);
			break;
		case ROT_MODE_AXISANGLE: /* axis angle */
			colsub = uiLayoutColumn(split, 1);
			uiItemR(colsub, ptr, "rotation_axis_angle", 0, "Rotation", 0);
			colsub = uiLayoutColumn(split, 1);
			uiItemR(colsub, ptr, "lock_rotations_4d", UI_ITEM_R_TOGGLE, "4L", 0);
			if (RNA_boolean_get(ptr, "lock_rotations_4d"))
				uiItemR(colsub, ptr, "lock_rotation_w", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_LOCKED);
			else
				uiItemL(colsub, "", 0);
			uiItemR(colsub, ptr, "lock_rotation", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_LOCKED);
			break;
		default: /* euler rotations */
			colsub = uiLayoutColumn(split, 1);
			uiItemR(colsub, ptr, "rotation_euler", 0, "Rotation", 0);
			colsub = uiLayoutColumn(split, 1);
			uiItemL(colsub, "", 0);
			uiItemR(colsub, ptr, "lock_rotation", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_LOCKED);
			break;
	}
	uiItemR(layout, ptr, "rotation_mode", 0, "", 0);
	
	split = uiLayoutSplit(layout, 0.8, 0);
	colsub = uiLayoutColumn(split, 1);
	uiItemR(colsub, ptr, "scale", 0, "Scale", 0);
	colsub = uiLayoutColumn(split, 1);
	uiItemL(colsub, "", 0);
	uiItemR(colsub, ptr, "lock_scale", UI_ITEM_R_TOGGLE+UI_ITEM_R_ICON_ONLY, "", ICON_LOCKED);
	
	if (ptr->type == &RNA_Object) {
		Object *ob = ptr->data;
		if (ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL))
			uiItemR(layout, ptr, "dimensions", 0, "Dimensions", 0);
	}
}

static void v3d_posearmature_buts(uiLayout *layout, View3D *v3d, Object *ob, float lim)
{
//	uiBlock *block= uiLayoutGetBlock(layout);
//	bArmature *arm;
	bPoseChannel *pchan;
//	TransformProperties *tfp= v3d->properties_storage;
	PointerRNA pchanptr;
	uiLayout *col;
//	uiLayout *row;

	pchan= get_active_posechannel(ob);

//	row= uiLayoutRow(layout, 0);
	
	if (!pchan)	{
		uiItemL(layout, "No Bone Active", 0);
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
	tfp->ob_eul[0]*= 180.0/M_PI;
	tfp->ob_eul[1]*= 180.0/M_PI;
	tfp->ob_eul[2]*= 180.0/M_PI;
	
	uiDefBut(block, LABEL, 0, "Location:",			0, 240, 100, 20, 0, 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "X:",	0, 220, 120, 19, pchan->loc, -lim, lim, 100, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "Y:",	0, 200, 120, 19, pchan->loc+1, -lim, lim, 100, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "Z:",	0, 180, 120, 19, pchan->loc+2, -lim, lim, 100, 3, "");
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
void validate_editbonebutton_cb(bContext *C, void *bonev, void *namev)
{
	EditBone *eBone= bonev;
	char oldname[32], newname[32];
	
	/* need to be on the stack */
	BLI_strncpy(newname, eBone->name, 32);
	BLI_strncpy(oldname, (char *)namev, 32);
	/* restore */
	BLI_strncpy(eBone->name, oldname, 32);
	
	ED_armature_bone_rename(CTX_data_edit_object(C)->data, oldname, newname); // editarmature.c
	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, CTX_data_edit_object(C)); // XXX fix
}

static void v3d_editarmature_buts(uiLayout *layout, View3D *v3d, Object *ob, float lim)
{
//	uiBlock *block= uiLayoutGetBlock(layout);
	bArmature *arm= ob->data;
	EditBone *ebone;
//	TransformProperties *tfp= v3d->properties_storage;
//	uiLayout *row;
	uiLayout *col;
	PointerRNA eboneptr;
	
	ebone= arm->act_edbone;

	if (!ebone || (ebone->layer & arm->layer)==0)
		return;
	
//	row= uiLayoutRow(layout, 0);
	RNA_pointer_create(&arm->id, &RNA_EditBone, ebone, &eboneptr);


	col= uiLayoutColumn(layout, 0);
	uiItemR(col, &eboneptr, "head", 0, "Head", 0);
	if (ebone->parent && ebone->flag & BONE_CONNECTED ) {
		PointerRNA parptr = RNA_pointer_get(&eboneptr, "parent");
		uiItemR(col, &parptr, "tail_radius", 0, "Radius", 0);
	} else {
		uiItemR(col, &eboneptr, "head_radius", 0, "Radius", 0);
	}
	
	uiItemR(col, &eboneptr, "tail", 0, "Tail", 0);
	uiItemR(col, &eboneptr, "tail_radius", 0, "Radius", 0);
	
	uiItemR(col, &eboneptr, "roll", 0, "Roll", 0);
}

static void v3d_editmetaball_buts(uiLayout *layout, Object *ob, float lim)
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
	uiItemR(col, &ptr, "location", 0, "Location", 0);
	
	uiItemR(col, &ptr, "radius", 0, "Radius", 0);
	uiItemR(col, &ptr, "stiffness", 0, "Stiffness", 0);
	
	uiItemR(col, &ptr, "type", 0, "Type", 0);
	
	col= uiLayoutColumn(layout, 1);
	switch (RNA_enum_get(&ptr, "type")) {
		case MB_BALL:
			break;
		case MB_CUBE:
			uiItemL(col, "Size:", 0);
			uiItemR(col, &ptr, "size_x", 0, "X", 0);
			uiItemR(col, &ptr, "size_y", 0, "Y", 0);
			uiItemR(col, &ptr, "size_z", 0, "Z", 0);
			break;
		case MB_TUBE:
			uiItemL(col, "Size:", 0);
			uiItemR(col, &ptr, "size_x", 0, "X", 0);
			break;
		case MB_PLANE:
			uiItemL(col, "Size:", 0);
			uiItemR(col, &ptr, "size_x", 0, "X", 0);
			uiItemR(col, &ptr, "size_y", 0, "Y", 0);
			break;
		case MB_ELIPSOID:
			uiItemL(col, "Size:", 0);
			uiItemR(col, &ptr, "size_x", 0, "X", 0);
			uiItemR(col, &ptr, "size_y", 0, "Y", 0);
			uiItemR(col, &ptr, "size_z", 0, "Z", 0);
			break;		   
	}	
}

/* test if 'ob' is a parent somewhere in par's parents */
static int test_parent_loop(Object *par, Object *ob)
{
	if(par == NULL) return 0;
	if(ob == par) return 1;
	return test_parent_loop(par->parent, ob);
}

static void do_view3d_region_buttons(bContext *C, void *arg, int event)
{
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
		DAG_id_flush_update(&ob->id, OB_RECALC_OB);
		break;

	
	case B_OBJECTPANELMEDIAN:
		if(ob) {
			v3d_editvertex_buts(C, NULL, v3d, ob, 1.0);
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		}
		break;
		
		/* note; this case also used for parbone */
	case B_OBJECTPANELPARENT:
		if(ob) {
			if(ob->id.lib || test_parent_loop(ob->parent, ob) ) 
				ob->parent= NULL;
			else {
				DAG_scene_sort(scene);
				DAG_id_flush_update(&ob->id, OB_RECALC_OB);
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
			eul[0]= M_PI*tfp->ob_eul[0]/180.0;
			eul[1]= M_PI*tfp->ob_eul[1]/180.0;
			eul[2]= M_PI*tfp->ob_eul[2]/180.0;
			
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
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		}
		break;
	case B_TRANSFORMSPACEADD:
		BIF_createTransformOrientation(C, NULL, "", 1, 0);
		break;
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
//		if(!multires_level1_test()) {
		{
			bDeformGroup *defGroup = BLI_findlink(&ob->defbase, ob->actdef-1);
			if(defGroup) {
				Mesh *me= ob->data;
				int a;
				for(a=0; a<me->totvert; a++)
					ED_vgroup_vert_remove (ob, defGroup, a);
				DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
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
			
			if((viewlock & RV3D_LOCKED)==0)
				viewlock= 0;
			else if((viewlock & RV3D_BOXVIEW)==0)
				viewlock &= ~RV3D_BOXCLIP;
			
			for(; ar; ar= ar->prev) {
				if(ar->alignment==RGN_ALIGN_QSPLIT) {
					rv3d= ar->regiondata;
					rv3d->viewlock= viewlock;
				}
			}
			
			if(rv3d->viewlock & RV3D_BOXVIEW)
				view3d_boxview_copy(sa, sa->regionbase.last);
			
			ED_area_tag_redraw(sa);
		}
		break;
	}

	/* default for now */
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_VIEW3D, ob);
}

void removeTransformOrientation_func(bContext *C, void *target, void *unused)
{
	BIF_removeTransformOrientation(C, (TransformOrientation *) target);
}

void selectTransformOrientation_func(bContext *C, void *target, void *unused)
{
	BIF_selectTransformOrientation(C, (TransformOrientation *) target);
}

static void view3d_panel_object(const bContext *C, Panel *pa)
{
	uiBlock *block;
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	View3D *v3d= CTX_wm_view3d(C);
	//uiBut *bt;
	Object *ob= OBACT;
	TransformProperties *tfp;
	PointerRNA obptr;
	uiLayout *col, *row;
	float lim;
	
	if(ob==NULL) return;

	/* make sure we got storage */
	if(v3d->properties_storage==NULL)
		v3d->properties_storage= MEM_callocN(sizeof(TransformProperties), "TransformProperties");
	tfp= v3d->properties_storage;
	
// XXX	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	/*
	if(ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT)) {
	}
	else {
		if((ob->mode & OB_MODE_PARTICLE_EDIT)==0) {
			uiBlockEndAlign(block);
		}
	}
	*/

	lim= 10000.0f*MAX2(1.0, v3d->grid);

	block= uiLayoutGetBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_view3d_region_buttons, NULL);

	col= uiLayoutColumn(pa->layout, 0);
	row= uiLayoutRow(col, 0);
	RNA_id_pointer_create(&ob->id, &obptr);

	if(ob==obedit) {
		if(ob->type==OB_ARMATURE) v3d_editarmature_buts(col, v3d, ob, lim);
		if(ob->type==OB_MBALL) v3d_editmetaball_buts(col, ob, lim);
		else v3d_editvertex_buts(C, col, v3d, ob, lim);
	}
	else if(ob->mode & OB_MODE_POSE) {
		v3d_posearmature_buts(col, v3d, ob, lim);
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
	
	block= uiBeginBlock(C, ar, "view3d_panel_preview", UI_EMBOSS);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | UI_PNL_SCALE | cntrl);
	uiSetPanelHandler(VIEW3D_HANDLER_PREVIEW);  // for close and esc
	
	ofsx= -150+(sa->winx/2)/v3d->blockscale;
	ofsy= -100+(sa->winy/2)/v3d->blockscale;
	if(uiNewPanel(C, ar, block, "Preview", "View3d", ofsx, ofsy, 300, 200)==0) return;

	uiBlockSetDrawExtraFunc(block, BIF_view3d_previewdraw);
	
	if(scene->recalc & SCE_PRV_CHANGED) {
		scene->recalc &= ~SCE_PRV_CHANGED;
		//printf("found recalc\n");
		BIF_view3d_previewrender_free(sa->spacedata.first);
		BIF_preview_changed(0);
	}
}
#endif

#if 0 // XXX not used
static void delete_sketch_armature(bContext *C, void *arg1, void *arg2)
{
	BIF_deleteSketch(C);
}

static void convert_sketch_armature(bContext *C, void *arg1, void *arg2)
{
	BIF_convertSketch(C);
}

static void assign_template_sketch_armature(bContext *C, void *arg1, void *arg2)
{
	int index = *(int*)arg1;
	BIF_setTemplate(C, index);
}


static int view3d_panel_bonesketch_spaces_poll(const bContext *C, PanelType *pt)
{
	Object *obedit = CTX_data_edit_object(C);

	/* replace with check call to sketching lib */
	return (obedit && obedit->type == OB_ARMATURE);
}
static void view3d_panel_bonesketch_spaces(const bContext *C, Panel *pa)
{
	Scene *scene = CTX_data_scene(C);
	static int template_index;
	static char joint_label[128];
	uiBlock *block;
	uiBut *but;
	char *bone_name;
	int yco = 130;
	int nb_joints;
	static char subdiv_tooltip[4][64] = {
		"Subdivide arcs based on a fixed number of bones",
		"Subdivide arcs in bones of equal length",
		"Subdivide arcs based on correlation",
		"Retarget template to stroke"
		};

	
	block= uiLayoutAbsoluteBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_view3d_region_buttons, NULL);

	uiBlockBeginAlign(block);
	
	/* use real flag instead of 1 */
	uiDefButBitC(block, TOG, BONE_SKETCHING, B_REDR, "Use Bone Sketching", 10, yco, 160, 20, &scene->toolsettings->bone_sketching, 0, 0, 0, 0, "Use sketching to create and edit bones, (Ctrl snaps to mesh volume)");
	uiDefButBitC(block, TOG, BONE_SKETCHING_ADJUST, B_REDR, "A", 170, yco, 20, 20, &scene->toolsettings->bone_sketching, 0, 0, 0, 0, "Adjust strokes by drawing near them");
	uiDefButBitC(block, TOG, BONE_SKETCHING_QUICK, B_REDR, "Q", 190, yco, 20, 20, &scene->toolsettings->bone_sketching, 0, 0, 0, 0, "Automatically convert and delete on stroke end");
	yco -= 20;
	
	but = uiDefBut(block, BUT, B_REDR, "Convert", 10,yco,100,20, 0, 0, 0, 0, 0, "Convert sketch to armature");
	uiButSetFunc(but, convert_sketch_armature, NULL, NULL);

	but = uiDefBut(block, BUT, B_REDR, "Delete", 110,yco,100,20, 0, 0, 0, 0, 0, "Delete sketch");
	uiButSetFunc(but, delete_sketch_armature, NULL, NULL);
	yco -= 20;
	
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	
	uiDefButC(block, MENU, B_REDR, "Subdivision Method%t|Length%x1|Adaptative%x2|Fixed%x0|Template%x3", 10,yco,60,19, &scene->toolsettings->bone_sketching_convert, 0, 0, 0, 0, subdiv_tooltip[(unsigned char)scene->toolsettings->bone_sketching_convert]);

	switch(scene->toolsettings->bone_sketching_convert)
	{
	case SK_CONVERT_CUT_LENGTH:
		uiDefButF(block, NUM, B_REDR, 					"Lim:",		70, yco, 140, 19, &scene->toolsettings->skgen_length_limit,0.1,50.0, 10, 0,		"Maximum length of the subdivided bones");
		yco -= 20;
		break;
	case SK_CONVERT_CUT_ADAPTATIVE:
		uiDefButF(block, NUM, B_REDR, 					"Thres:",			70, yco, 140, 19, &scene->toolsettings->skgen_correlation_limit,0.0, 1.0, 0.01, 0,	"Correlation threshold for subdivision");
		yco -= 20;
		break;
	default:
	case SK_CONVERT_CUT_FIXED:
		uiDefButC(block, NUM, B_REDR, 					"Num:",		70, yco, 140, 19, &scene->toolsettings->skgen_subdivision_number,1, 100, 1, 5,	"Number of subdivided bones");
		yco -= 20;
		break;
	case SK_CONVERT_RETARGET:
		uiDefButC(block, ROW, B_NOP, "No",			70,  yco, 40,19, &scene->toolsettings->skgen_retarget_roll, 0, 0, 0, 0,									"No special roll treatment");
		uiDefButC(block, ROW, B_NOP, "View",		110,  yco, 50,19, &scene->toolsettings->skgen_retarget_roll, 0, SK_RETARGET_ROLL_VIEW, 0, 0,				"Roll bones perpendicular to view");
		uiDefButC(block, ROW, B_NOP, "Joint",		160, yco, 50,19, &scene->toolsettings->skgen_retarget_roll, 0, SK_RETARGET_ROLL_JOINT, 0, 0,				"Roll bones relative to joint bend");
		yco -= 30;

		uiBlockEndAlign(block);

		uiBlockBeginAlign(block);
		/* button here to select what to do (copy or not), template, ...*/

		BIF_makeListTemplates(C);
		template_index = BIF_currentTemplate(C);
		
		but = uiDefButI(block, MENU, B_REDR, BIF_listTemplates(C), 10,yco,200,19, &template_index, 0, 0, 0, 0, "Template");
		uiButSetFunc(but, assign_template_sketch_armature, &template_index, NULL);
		
		yco -= 20;
		
		uiDefButF(block, NUM, B_NOP, 							"A:",			10, yco, 66,19, &scene->toolsettings->skgen_retarget_angle_weight, 0, 10, 1, 0,		"Angle Weight");
		uiDefButF(block, NUM, B_NOP, 							"L:",			76, yco, 67,19, &scene->toolsettings->skgen_retarget_length_weight, 0, 10, 1, 0,		"Length Weight");
		uiDefButF(block, NUM, B_NOP, 							"D:",		143,yco, 67,19, &scene->toolsettings->skgen_retarget_distance_weight, 0, 10, 1, 0,		"Distance Weight");
		yco -= 20;
		
		uiDefBut(block, TEX,B_REDR,"S:",							10,  yco, 90, 20, scene->toolsettings->skgen_side_string, 0.0, 8.0, 0, 0, "Text to replace &S with");
		uiDefBut(block, TEX,B_REDR,"N:",							100, yco, 90, 20, scene->toolsettings->skgen_num_string, 0.0, 8.0, 0, 0, "Text to replace &N with");
		uiDefIconButBitC(block, TOG, SK_RETARGET_AUTONAME, B_NOP, ICON_AUTO,190,yco,20,20, &scene->toolsettings->skgen_retarget_options, 0, 0, 0, 0, "Use Auto Naming");	
		yco -= 20;

		/* auto renaming magic */
		uiBlockEndAlign(block);
		
		nb_joints = BIF_nbJointsTemplate(C);

		if (nb_joints == -1)
		{
			//XXX
			//nb_joints = G.totvertsel;
		}
		
		bone_name = BIF_nameBoneTemplate(C);
		
		BLI_snprintf(joint_label, 32, "%i joints: %s", nb_joints, bone_name);
		
		uiDefBut(block, LABEL, 1, joint_label,					10, yco, 200, 20, NULL, 0.0, 0.0, 0, 0, "");
		yco -= 20;
		break;
	}

	uiBlockEndAlign(block);
}

/* op->invoke */
static void redo_cb(bContext *C, void *arg_op, void *arg2)
{
	wmOperator *lastop= arg_op;
	
	if(lastop) {
		int retval;
		
		if (G.f & G_DEBUG)
			printf("operator redo %s\n", lastop->type->name);
		ED_undo_pop(C);
		retval= WM_operator_repeat(C, lastop);
		if((retval & OPERATOR_FINISHED)==0) {
			if (G.f & G_DEBUG)
				printf("operator redo failed %s\n", lastop->type->name);
			ED_undo_redo(C);
		}
	}
}

static void view3d_panel_operator_redo(const bContext *C, Panel *pa)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmOperator *op;
	PointerRNA ptr;
	uiBlock *block;
	
	block= uiLayoutGetBlock(pa->layout);

	/* only for operators that are registered and did an undo push */
	for(op= wm->operators.last; op; op= op->prev)
		if((op->type->flag & OPTYPE_REGISTER) && (op->type->flag & OPTYPE_UNDO))
			break;
	
	if(op==NULL)
		return;
	
	uiBlockSetFunc(block, redo_cb, op, NULL);
	
	if(!op->properties) {
		IDPropertyTemplate val = {0};
		op->properties= IDP_New(IDP_GROUP, val, "wmOperatorProperties");
	}
	
	RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);
	uiDefAutoButsRNA(C, pa->layout, &ptr, 2);
}
#endif // XXX not used

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

static int view3d_properties(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= view3d_has_buttons_region(sa);
	
	if(ar)
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
