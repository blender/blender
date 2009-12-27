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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_ipo_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_linklist.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_context.h"
#include "BKE_armature.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "view3d_intern.h"


/* ************************************************** */
/* ********************* old transform stuff ******** */
/* *********** will get replaced with new transform * */
/* ************************************************** */

typedef struct TransVert {
	float *loc;
	float oldloc[3], fac;
	float *val, oldval;
	int flag;
	float *nor;
} TransVert;

static TransVert *transvmain=NULL;
static int tottrans= 0;

/* copied from editobject.c, now uses (almost) proper depgraph */
static void special_transvert_update(Scene *scene, Object *obedit)
{
	
	if(obedit) {
		
		DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
		
		if(obedit->type==OB_MESH) {
			Mesh *me= obedit->data;
			recalc_editnormals(me->edit_mesh);	// does face centers too
		}
		else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
			Curve *cu= obedit->data;
			Nurb *nu= cu->editnurb->first;
			
			while(nu) {
				test2DNurb(nu);
				testhandlesNurb(nu); /* test for bezier too */
				nu= nu->next;
			}
		}
		else if(obedit->type==OB_ARMATURE){
			bArmature *arm= obedit->data;
			EditBone *ebo;
			TransVert *tv= transvmain;
			int a=0;
			
			/* Ensure all bone tails are correctly adjusted */
			for (ebo= arm->edbo->first; ebo; ebo=ebo->next) {
				/* adjust tip if both ends selected */
				if ((ebo->flag & BONE_ROOTSEL) && (ebo->flag & BONE_TIPSEL)) {
					if (tv) {
						float diffvec[3];
						
						sub_v3_v3v3(diffvec, tv->loc, tv->oldloc);
						add_v3_v3v3(ebo->tail, ebo->tail, diffvec);
						
						a++;
						if (a<tottrans) tv++;
					}
				}
			}
			
			/* Ensure all bones are correctly adjusted */
			for (ebo= arm->edbo->first; ebo; ebo=ebo->next) {
				if ((ebo->flag & BONE_CONNECTED) && ebo->parent){
					/* If this bone has a parent tip that has been moved */
					if (ebo->parent->flag & BONE_TIPSEL){
						VECCOPY (ebo->head, ebo->parent->tail);
					}
					/* If this bone has a parent tip that has NOT been moved */
					else{
						VECCOPY (ebo->parent->tail, ebo->head);
					}
				}
			}
			if(arm->flag & ARM_MIRROR_EDIT) 
				transform_armature_mirror_update(obedit);
		}
		else if(obedit->type==OB_LATTICE) {
			Lattice *lt= obedit->data;
			
			if(lt->editlatt->flag & LT_OUTSIDE) 
				outside_lattice(lt->editlatt);
		}
	}
}

/* copied from editobject.c, needs to be replaced with new transform code still */
/* mode: 1 = proportional, 2 = all joints (for bones only) */
static void make_trans_verts(Object *obedit, float *min, float *max, int mode)	
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	TransVert *tv=NULL;
	MetaElem *ml;
	EditVert *eve;
	EditBone	*ebo;
	float total, center[3], centroid[3];
	int a;

	tottrans= 0; // global!
	
	INIT_MINMAX(min, max);
	centroid[0]=centroid[1]=centroid[2]= 0.0;
	
	if(obedit->type==OB_MESH) {
		Mesh *me= obedit->data;
		EditMesh *em= me->edit_mesh;
		int proptrans= 0;
		
		// transform now requires awareness for select mode, so we tag the f1 flags in verts
		tottrans= 0;
		if(em->selectmode & SCE_SELECT_VERTEX) {
			for(eve= em->verts.first; eve; eve= eve->next) {
				if(eve->h==0 && (eve->f & SELECT)) {
					eve->f1= SELECT;
					tottrans++;
				}
				else eve->f1= 0;
			}
		}
		else if(em->selectmode & SCE_SELECT_EDGE) {
			EditEdge *eed;
			for(eve= em->verts.first; eve; eve= eve->next) eve->f1= 0;
			for(eed= em->edges.first; eed; eed= eed->next) {
				if(eed->h==0 && (eed->f & SELECT)) eed->v1->f1= eed->v2->f1= SELECT;
			}
			for(eve= em->verts.first; eve; eve= eve->next) if(eve->f1) tottrans++;
		}
		else {
			EditFace *efa;
			for(eve= em->verts.first; eve; eve= eve->next) eve->f1= 0;
			for(efa= em->faces.first; efa; efa= efa->next) {
				if(efa->h==0 && (efa->f & SELECT)) {
					efa->v1->f1= efa->v2->f1= efa->v3->f1= SELECT;
					if(efa->v4) efa->v4->f1= SELECT;
				}
			}
			for(eve= em->verts.first; eve; eve= eve->next) if(eve->f1) tottrans++;
		}
		
		/* proportional edit exception... */
		if((mode & 1) && tottrans) {
			for(eve= em->verts.first; eve; eve= eve->next) {
				if(eve->h==0) {
					eve->f1 |= 2;
					proptrans++;
				}
			}
			if(proptrans>tottrans) tottrans= proptrans;
		}
		
		/* and now make transverts */
		if(tottrans) {
			tv=transvmain= MEM_callocN(tottrans*sizeof(TransVert), "maketransverts");

			for(eve= em->verts.first; eve; eve= eve->next) {
				if(eve->f1) {
					VECCOPY(tv->oldloc, eve->co);
					tv->loc= eve->co;
					if(eve->no[0]!=0.0 || eve->no[1]!=0.0 ||eve->no[2]!=0.0)
						tv->nor= eve->no; // note this is a hackish signal (ton)
					tv->flag= eve->f1 & SELECT;
					tv++;
				}
			}
		}
	}
	else if (obedit->type==OB_ARMATURE){
		bArmature *arm= obedit->data;
		int totmalloc= BLI_countlist(arm->edbo);

        totmalloc *= 2;  /* probably overkill but bones can have 2 trans verts each */

		tv=transvmain= MEM_callocN(totmalloc*sizeof(TransVert), "maketransverts armature");
		
		for (ebo= arm->edbo->first; ebo; ebo=ebo->next){
			if(ebo->layer & arm->layer) {
				short tipsel= (ebo->flag & BONE_TIPSEL);
				short rootsel= (ebo->flag & BONE_ROOTSEL);
				short rootok= (!(ebo->parent && (ebo->flag & BONE_CONNECTED) && ebo->parent->flag & BONE_TIPSEL));
				
				if ((tipsel && rootsel) || (rootsel)) {
					/* Don't add the tip (unless mode & 2, for getting all joints), 
					 * otherwise we get zero-length bones as tips will snap to the same
					 * location as heads. 
					 */
					if (rootok) {
						VECCOPY (tv->oldloc, ebo->head);
						tv->loc= ebo->head;
						tv->nor= NULL;
						tv->flag= 1;
						tv++;
						tottrans++;
					}	
					
					if ((mode & 2) && (tipsel)) {
						VECCOPY (tv->oldloc, ebo->tail);
						tv->loc= ebo->tail;
						tv->nor= NULL;
						tv->flag= 1;
						tv++;
						tottrans++;
					}					
				}
				else if (tipsel) {
					VECCOPY (tv->oldloc, ebo->tail);
					tv->loc= ebo->tail;
					tv->nor= NULL;
					tv->flag= 1;
					tv++;
					tottrans++;
				}
			}			
		}
	}
	else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
		Curve *cu= obedit->data;
		int totmalloc= 0;
		
		for(nu= cu->editnurb->first; nu; nu= nu->next) {
			if(nu->type == CU_BEZIER)
				totmalloc += 3*nu->pntsu;
			else
				totmalloc += nu->pntsu*nu->pntsv;
		}
		tv=transvmain= MEM_callocN(totmalloc*sizeof(TransVert), "maketransverts curve");

		nu= cu->editnurb->first;
		while(nu) {
			if(nu->type == CU_BEZIER) {
				a= nu->pntsu;
				bezt= nu->bezt;
				while(a--) {
					if(bezt->hide==0) {
						if((mode & 1) || (bezt->f1 & SELECT)) {
							VECCOPY(tv->oldloc, bezt->vec[0]);
							tv->loc= bezt->vec[0];
							tv->flag= bezt->f1 & SELECT;
							tv++;
							tottrans++;
						}
						if((mode & 1) || (bezt->f2 & SELECT)) {
							VECCOPY(tv->oldloc, bezt->vec[1]);
							tv->loc= bezt->vec[1];
							tv->val= &(bezt->alfa);
							tv->oldval= bezt->alfa;
							tv->flag= bezt->f2 & SELECT;
							tv++;
							tottrans++;
						}
						if((mode & 1) || (bezt->f3 & SELECT)) {
							VECCOPY(tv->oldloc, bezt->vec[2]);
							tv->loc= bezt->vec[2];
							tv->flag= bezt->f3 & SELECT;
							tv++;
							tottrans++;
						}
					}
					bezt++;
				}
			}
			else {
				a= nu->pntsu*nu->pntsv;
				bp= nu->bp;
				while(a--) {
					if(bp->hide==0) {
						if((mode & 1) || (bp->f1 & SELECT)) {
							VECCOPY(tv->oldloc, bp->vec);
							tv->loc= bp->vec;
							tv->val= &(bp->alfa);
							tv->oldval= bp->alfa;
							tv->flag= bp->f1 & SELECT;
							tv++;
							tottrans++;
						}
					}
					bp++;
				}
			}
			nu= nu->next;
		}
	}
	else if(obedit->type==OB_MBALL) {
		MetaBall *mb= obedit->data;
		int totmalloc= BLI_countlist(mb->editelems);
		
		tv=transvmain= MEM_callocN(totmalloc*sizeof(TransVert), "maketransverts mball");
		
		ml= mb->editelems->first;
		while(ml) {
			if(ml->flag & SELECT) {
				tv->loc= &ml->x;
				VECCOPY(tv->oldloc, tv->loc);
				tv->val= &(ml->rad);
				tv->oldval= ml->rad;
				tv->flag= 1;
				tv++;
				tottrans++;
			}
			ml= ml->next;
		}
	}
	else if(obedit->type==OB_LATTICE) {
		Lattice *lt= obedit->data;
		
		bp= lt->editlatt->def;
		
		a= lt->editlatt->pntsu*lt->editlatt->pntsv*lt->editlatt->pntsw;
		
		tv=transvmain= MEM_callocN(a*sizeof(TransVert), "maketransverts curve");
		
		while(a--) {
			if((mode & 1) || (bp->f1 & SELECT)) {
				if(bp->hide==0) {
					VECCOPY(tv->oldloc, bp->vec);
					tv->loc= bp->vec;
					tv->flag= bp->f1 & SELECT;
					tv++;
					tottrans++;
				}
			}
			bp++;
		}
	}
	
	/* cent etc */
	tv= transvmain;
	total= 0.0;
	for(a=0; a<tottrans; a++, tv++) {
		if(tv->flag & SELECT) {
			centroid[0]+= tv->oldloc[0];
			centroid[1]+= tv->oldloc[1];
			centroid[2]+= tv->oldloc[2];
			total+= 1.0;
			DO_MINMAX(tv->oldloc, min, max);
		}
	}
	if(total!=0.0) {
		centroid[0]/= total;
		centroid[1]/= total;
		centroid[2]/= total;
	}

	center[0]= (min[0]+max[0])/2.0;
	center[1]= (min[1]+max[1])/2.0;
	center[2]= (min[2]+max[2])/2.0;
	
}

/* *********************** operators ******************** */

static int snap_sel_to_grid(bContext *C, wmOperator *op)
{
	extern float originmat[3][3];	/* XXX object.c */
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene= CTX_data_scene(C);
	RegionView3D *rv3d= CTX_wm_region_data(C);
	TransVert *tv;
	float gridf, imat[3][3], bmat[3][3], vec[3];
	int a;

	gridf= rv3d->gridview;

	if(obedit) {
		tottrans= 0;
		
		if ELEM6(obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL) 
			make_trans_verts(obedit, bmat[0], bmat[1], 0);
		if(tottrans==0) return OPERATOR_CANCELLED;
		
		copy_m3_m4(bmat, obedit->obmat);
		invert_m3_m3(imat, bmat);
		
		tv= transvmain;
		for(a=0; a<tottrans; a++, tv++) {
			
			VECCOPY(vec, tv->loc);
			mul_m3_v3(bmat, vec);
			add_v3_v3v3(vec, vec, obedit->obmat[3]);
			vec[0]= gridf*floor(.5+ vec[0]/gridf);
			vec[1]= gridf*floor(.5+ vec[1]/gridf);
			vec[2]= gridf*floor(.5+ vec[2]/gridf);
			sub_v3_v3v3(vec, vec, obedit->obmat[3]);
			
			mul_m3_v3(imat, vec);
			VECCOPY(tv->loc, vec);
		}
		
		special_transvert_update(scene, obedit);
		
		MEM_freeN(transvmain);
		transvmain= NULL;
	
	}
	else {

		CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
			if(ob->mode & OB_MODE_POSE) {
				bPoseChannel *pchan;
				bArmature *arm= ob->data;
				
				for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
					if(pchan->bone->flag & BONE_SELECTED) {
						if(pchan->bone->layer & arm->layer) {
							if((pchan->bone->flag & BONE_CONNECTED)==0) { 
								float vecN[3], nLoc[3]; 
								
								/* get nearest grid point to snap to */
								VECCOPY(nLoc, pchan->pose_mat[3]);
								vec[0]= gridf * (float)(floor(.5+ nLoc[0]/gridf));
								vec[1]= gridf * (float)(floor(.5+ nLoc[1]/gridf));
								vec[2]= gridf * (float)(floor(.5+ nLoc[2]/gridf));
								
								/* get bone-space location of grid point */
								armature_loc_pose_to_bone(pchan, vec, vecN);
								
								/* adjust location */
								VECCOPY(pchan->loc, vecN);
							}
							/* if the bone has a parent and is connected to the parent, 
							 * don't do anything - will break chain unless we do auto-ik. 
							 */
						}
					}
				}
				ob->pose->flag |= (POSE_LOCKED|POSE_DO_UNLOCK);
				
				/* auto-keyframing */
// XXX				autokeyframe_pose_cb_func(ob, TFM_TRANSLATION, 0);
				DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
			}
			else {
				ob->recalc |= OB_RECALC_OB;
				
				vec[0]= -ob->obmat[3][0]+gridf*floor(.5+ ob->obmat[3][0]/gridf);
				vec[1]= -ob->obmat[3][1]+gridf*floor(.5+ ob->obmat[3][1]/gridf);
				vec[2]= -ob->obmat[3][2]+gridf*floor(.5+ ob->obmat[3][2]/gridf);
				
				if(ob->parent) {
					where_is_object(scene, ob);
					
					invert_m3_m3(imat, originmat);
					mul_m3_v3(imat, vec);
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
				else {
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
			
				/* auto-keyframing */
// XXX				autokeyframe_ob_cb_func(ob, TFM_TRANSLATION);
			}
		}
		CTX_DATA_END;
	}

	DAG_ids_flush_update(0);
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_selected_to_grid(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Snap Selection to Grid";
	ot->description= "Snap selected item(s) to nearest grid node.";
	ot->idname= "VIEW3D_OT_snap_selected_to_grid";
	
	/* api callbacks */
	ot->exec= snap_sel_to_grid;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *************************************************** */

static int snap_sel_to_curs(bContext *C, wmOperator *op)
{
	extern float originmat[3][3];	/* XXX object.c */
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);
	TransVert *tv;
	float *curs, imat[3][3], bmat[3][3], vec[3];
	int a;

	curs= give_cursor(scene, v3d);

	if(obedit) {
		tottrans= 0;
		
		if ELEM6(obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL) 
			make_trans_verts(obedit, bmat[0], bmat[1], 0);
		if(tottrans==0) return OPERATOR_CANCELLED;
		
		copy_m3_m4(bmat, obedit->obmat);
		invert_m3_m3(imat, bmat);
		
		tv= transvmain;
		for(a=0; a<tottrans; a++, tv++) {
			vec[0]= curs[0]-obedit->obmat[3][0];
			vec[1]= curs[1]-obedit->obmat[3][1];
			vec[2]= curs[2]-obedit->obmat[3][2];
			
			mul_m3_v3(imat, vec);
			VECCOPY(tv->loc, vec);
		}
		
		special_transvert_update(scene, obedit);
		
		MEM_freeN(transvmain);
		transvmain= NULL;
		
	}
	else {
		CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
			if(ob->mode & OB_MODE_POSE) {
				bPoseChannel *pchan;
				bArmature *arm= ob->data;
				float cursp[3];
				
				invert_m4_m4(ob->imat, ob->obmat);
				VECCOPY(cursp, curs);
				mul_m4_v3(ob->imat, cursp);
				
				for (pchan = ob->pose->chanbase.first; pchan; pchan=pchan->next) {
					if(pchan->bone->flag & BONE_SELECTED) {
						if(pchan->bone->layer & arm->layer) {
							if((pchan->bone->flag & BONE_CONNECTED)==0) { 
								float curspn[3];
								
								/* get location of cursor in bone-space */
								armature_loc_pose_to_bone(pchan, cursp, curspn);
								
								/* calculate new position */
								VECCOPY(pchan->loc, curspn);
							}
							/* if the bone has a parent and is connected to the parent, 
							 * don't do anything - will break chain unless we do auto-ik. 
							 */
						}
					}
				}
				ob->pose->flag |= (POSE_LOCKED|POSE_DO_UNLOCK);
				
				/* auto-keyframing */
// XXX				autokeyframe_pose_cb_func(ob, TFM_TRANSLATION, 0);
				DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
			}
			else {
				ob->recalc |= OB_RECALC_OB;
				
				vec[0]= -ob->obmat[3][0] + curs[0];
				vec[1]= -ob->obmat[3][1] + curs[1];
				vec[2]= -ob->obmat[3][2] + curs[2];
				
				if(ob->parent) {
					where_is_object(scene, ob);
					
					invert_m3_m3(imat, originmat);
					mul_m3_v3(imat, vec);
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
				else {
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
				/* auto-keyframing */
// XXX				autokeyframe_ob_cb_func(ob, TFM_TRANSLATION);
			}
		}
		CTX_DATA_END;
	}

	DAG_ids_flush_update(0);
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_selected_to_cursor(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Snap Selection to Cursor";
	ot->description= "Snap selected item(s) to cursor.";
	ot->idname= "VIEW3D_OT_snap_selected_to_cursor";
	
	/* api callbacks */
	ot->exec= snap_sel_to_curs;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *************************************************** */

static int snap_curs_to_grid(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	RegionView3D *rv3d= CTX_wm_region_data(C);
	View3D *v3d= CTX_wm_view3d(C);
	float gridf, *curs;

	gridf= rv3d->gridview;
	curs= give_cursor(scene, v3d);

	curs[0]= gridf*floor(.5+curs[0]/gridf);
	curs[1]= gridf*floor(.5+curs[1]/gridf);
	curs[2]= gridf*floor(.5+curs[2]/gridf);
	
	WM_event_add_notifier(C, NC_SCENE|ND_TRANSFORM, scene);	// hrm
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_cursor_to_grid(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Snap Cursor to Grid";
	ot->description= "Snap cursor to nearest grid node.";
	ot->idname= "VIEW3D_OT_snap_cursor_to_grid";
	
	/* api callbacks */
	ot->exec= snap_curs_to_grid;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* **************************************************** */

static int snap_curs_to_sel(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);
	TransVert *tv;
	float *curs, bmat[3][3], vec[3], min[3], max[3], centroid[3];
	int count, a;

	curs= give_cursor(scene, v3d);

	count= 0;
	INIT_MINMAX(min, max);
	centroid[0]= centroid[1]= centroid[2]= 0.0;

	if(obedit) {
		tottrans=0;
		
		if ELEM6(obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL) 
			make_trans_verts(obedit, bmat[0], bmat[1], 2);
		if(tottrans==0) return OPERATOR_CANCELLED;
		
		copy_m3_m4(bmat, obedit->obmat);
		
		tv= transvmain;
		for(a=0; a<tottrans; a++, tv++) {
			VECCOPY(vec, tv->loc);
			mul_m3_v3(bmat, vec);
			add_v3_v3v3(vec, vec, obedit->obmat[3]);
			add_v3_v3v3(centroid, centroid, vec);
			DO_MINMAX(vec, min, max);
		}
		
		if(v3d->around==V3D_CENTROID) {
			mul_v3_fl(centroid, 1.0/(float)tottrans);
			VECCOPY(curs, centroid);
		}
		else {
			curs[0]= (min[0]+max[0])/2;
			curs[1]= (min[1]+max[1])/2;
			curs[2]= (min[2]+max[2])/2;
		}
		MEM_freeN(transvmain);
		transvmain= NULL;
	}
	else {
		Object *ob= CTX_data_active_object(C);
		
		if(ob && (ob->mode & OB_MODE_POSE)) {
			bArmature *arm= ob->data;
			bPoseChannel *pchan;
			for (pchan = ob->pose->chanbase.first; pchan; pchan=pchan->next) {
				if(arm->layer & pchan->bone->layer) {
					if(pchan->bone->flag & BONE_SELECTED) {
						VECCOPY(vec, pchan->pose_head);
						mul_m4_v3(ob->obmat, vec);
						add_v3_v3v3(centroid, centroid, vec);
						DO_MINMAX(vec, min, max);
						count++;
					}
				}
			}
		}
		else {
			CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
				VECCOPY(vec, ob->obmat[3]);
				add_v3_v3v3(centroid, centroid, vec);
				DO_MINMAX(vec, min, max);
				count++;
			}
			CTX_DATA_END;
		}
		if(count) {
			if(v3d->around==V3D_CENTROID) {
				mul_v3_fl(centroid, 1.0/(float)count);
				VECCOPY(curs, centroid);
			}
			else {
				curs[0]= (min[0]+max[0])/2;
				curs[1]= (min[1]+max[1])/2;
				curs[2]= (min[2]+max[2])/2;
			}
		}
	}
	WM_event_add_notifier(C, NC_SCENE|ND_TRANSFORM, scene);	// hrm
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_cursor_to_selected(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Snap Cursor to Selected";
	ot->description= "Snap cursor to center of selected item(s)."; 
	ot->idname= "VIEW3D_OT_snap_cursor_to_selected";
	
	/* api callbacks */
	ot->exec= snap_curs_to_sel;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ********************************************** */

static int snap_curs_to_active(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Object *obact= CTX_data_active_object(C);
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);
	float *curs;
	
	curs = give_cursor(scene, v3d);

	if (obedit)  {
		if (obedit->type == OB_MESH) {
			/* check active */
			Mesh *me= obedit->data;
			EditSelection ese;
			
			if (EM_get_actSelection(me->edit_mesh, &ese)) {
				EM_editselection_center(curs, &ese);
			}
			
			mul_m4_v3(obedit->obmat, curs);
		}
	}
	else {
		if (obact) {
			VECCOPY(curs, obact->obmat[3]);
		}
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_TRANSFORM, scene);
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_cursor_to_active(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Snap Cursor to Active";
	ot->description= "Snap cursor to active item.";
	ot->idname= "VIEW3D_OT_snap_cursor_to_active";
	
	/* api callbacks */
	ot->exec= snap_curs_to_active;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************************************** */

static int snap_selected_to_center(bContext *C, wmOperator *op)
{
	extern float originmat[3][3]; 	/* XXX object.c */
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);
	TransVert *tv;
	float snaploc[3], imat[3][3], bmat[3][3], vec[3], min[3], max[3], centroid[3];
	int count, a;

	/*calculate the snaplocation (centerpoint) */
	count= 0;
	INIT_MINMAX(min, max);
	centroid[0]= centroid[1]= centroid[2]= 0.0f;
	snaploc[0]= snaploc[1]= snaploc[2]= 0.0f;

	if(obedit) {
		tottrans= 0;
		
		if ELEM6(obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL) 
			make_trans_verts(obedit, bmat[0], bmat[1], 0);
		if(tottrans==0) return OPERATOR_CANCELLED;
		
		copy_m3_m4(bmat, obedit->obmat);
		invert_m3_m3(imat, bmat);
		
		tv= transvmain;
		for(a=0; a<tottrans; a++, tv++) {
			VECCOPY(vec, tv->loc);
			mul_m3_v3(bmat, vec);
			add_v3_v3v3(vec, vec, obedit->obmat[3]);
			add_v3_v3v3(centroid, centroid, vec);
			DO_MINMAX(vec, min, max);
		}
		
		if(v3d->around==V3D_CENTROID) {
			mul_v3_fl(centroid, 1.0/(float)tottrans);
			VECCOPY(snaploc, centroid);
		}
		else {
			snaploc[0]= (min[0]+max[0])/2;
			snaploc[1]= (min[1]+max[1])/2;
			snaploc[2]= (min[2]+max[2])/2;
		}
		
		MEM_freeN(transvmain);
		transvmain= NULL;
	}
	else {
		
		CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
			if(ob->mode & OB_MODE_POSE) {
				bPoseChannel *pchan;
				bArmature *arm= ob->data;
				
				for (pchan = ob->pose->chanbase.first; pchan; pchan=pchan->next) {
					if(pchan->bone->flag & BONE_SELECTED) {
						if(pchan->bone->layer & arm->layer) {
							VECCOPY(vec, pchan->pose_mat[3]);
							add_v3_v3v3(centroid, centroid, vec);
							DO_MINMAX(vec, min, max);
							count++;
						}
					}
				}
			}
			else {
				/* not armature bones (i.e. objects) */
				VECCOPY(vec, ob->obmat[3]);
				add_v3_v3v3(centroid, centroid, vec);
				DO_MINMAX(vec, min, max);
				count++;
			}
		}
		CTX_DATA_END;

		if(count) {
			if(v3d->around==V3D_CENTROID) {
				mul_v3_fl(centroid, 1.0/(float)count);
				VECCOPY(snaploc, centroid);
			}
			else {
				snaploc[0]= (min[0]+max[0])/2;
				snaploc[1]= (min[1]+max[1])/2;
				snaploc[2]= (min[2]+max[2])/2;
			}
		}
	}

	/* Snap the selection to the snaplocation (duh!) */
	if(obedit) {
		tottrans= 0;
		
		if ELEM6(obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL) 
			make_trans_verts(obedit, bmat[0], bmat[1], 0);
		if(tottrans==0) return OPERATOR_CANCELLED;
		
		copy_m3_m4(bmat, obedit->obmat);
		invert_m3_m3(imat, bmat);
		
		tv= transvmain;
		for(a=0; a<tottrans; a++, tv++) {
			vec[0]= snaploc[0]-obedit->obmat[3][0];
			vec[1]= snaploc[1]-obedit->obmat[3][1];
			vec[2]= snaploc[2]-obedit->obmat[3][2];
			
			mul_m3_v3(imat, vec);
			VECCOPY(tv->loc, vec);
		}
		
		special_transvert_update(scene, obedit);
		
		MEM_freeN(transvmain);
		transvmain= NULL;
		
	}
	else {

		CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
			if(ob->mode & OB_MODE_POSE) {
				bPoseChannel *pchan;
				bArmature *arm= ob->data;
				
				for (pchan = ob->pose->chanbase.first; pchan; pchan=pchan->next) {
					if(pchan->bone->flag & BONE_SELECTED) {
						if(pchan->bone->layer & arm->layer) {
							if((pchan->bone->flag & BONE_CONNECTED)==0) { 
								/* get location of cursor in bone-space */
								armature_loc_pose_to_bone(pchan, snaploc, vec);
								
								/* calculate new position */
								VECCOPY(pchan->loc, vec);
							}
							/* if the bone has a parent and is connected to the parent, 
							 * don't do anything - will break chain unless we do auto-ik. 
							 */
						}
					}
				}
				
				/* auto-keyframing */
				ob->pose->flag |= POSE_DO_UNLOCK;
// XXX				autokeyframe_pose_cb_func(ob, TFM_TRANSLATION, 0);
				DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
			}
			else {
				ob->recalc |= OB_RECALC_OB;
				
				vec[0]= -ob->obmat[3][0] + snaploc[0];
				vec[1]= -ob->obmat[3][1] + snaploc[1];
				vec[2]= -ob->obmat[3][2] + snaploc[2];
				
				if(ob->parent) {
					where_is_object(scene, ob);
					
					invert_m3_m3(imat, originmat);
					mul_m3_v3(imat, vec);
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
				else {
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
				/* auto-keyframing */
// XXX				autokeyframe_ob_cb_func(ob, TFM_TRANSLATION);
			}
		}
		CTX_DATA_END;
	}
	
	DAG_ids_flush_update(0);
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_selected_to_center(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Snap Selection to Center";
	ot->description= "Snap selected items to selections geometric center.";
	ot->idname= "VIEW3D_OT_snap_selected_to_center";
	
	/* api callbacks */
	ot->exec= snap_selected_to_center;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


int minmax_verts(Object *obedit, float *min, float *max)
{
	TransVert *tv;
	float centroid[3], vec[3], bmat[3][3];
	int a;

	tottrans=0;
	if ELEM5(obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) 
		make_trans_verts(obedit, bmat[0], bmat[1], 2);
	
	if(tottrans==0) return 0;

	copy_m3_m4(bmat, obedit->obmat);
	
	tv= transvmain;
	for(a=0; a<tottrans; a++, tv++) {		
		VECCOPY(vec, tv->loc);
		mul_m3_v3(bmat, vec);
		add_v3_v3v3(vec, vec, obedit->obmat[3]);
		add_v3_v3v3(centroid, centroid, vec);
		DO_MINMAX(vec, min, max);		
	}
	
	MEM_freeN(transvmain);
	transvmain= NULL;
	
	return 1;
}

