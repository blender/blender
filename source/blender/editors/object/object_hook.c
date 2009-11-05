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
 * Contributor(s): Blender Foundation, 2002-2008 full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "ED_curve.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_view3d.h"

#include "object_intern.h"

/* XXX operators for this are not implemented yet */

static int return_editmesh_indexar(EditMesh *em, int *tot, int **indexar, float *cent)
{
	EditVert *eve;
	int *index, nr, totvert=0;
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f & SELECT) totvert++;
	}
	if(totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	cent[0]= cent[1]= cent[2]= 0.0;
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f & SELECT) {
			*index= nr; index++;
			VecAddf(cent, cent, eve->co);
		}
		nr++;
	}
	
	VecMulf(cent, 1.0f/(float)totvert);
	
	return totvert;
}

static int return_editmesh_vgroup(Object *obedit, EditMesh *em, char *name, float *cent)
{
	MDeformVert *dvert;
	EditVert *eve;
	int i, totvert=0;
	
	cent[0]= cent[1]= cent[2]= 0.0;
	
	if(obedit->actdef) {
		
		/* find the vertices */
		for(eve= em->verts.first; eve; eve= eve->next) {
			dvert= CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT);

			if(dvert) {
				for(i=0; i<dvert->totweight; i++){
					if(dvert->dw[i].def_nr == (obedit->actdef-1)) {
						totvert++;
						VecAddf(cent, cent, eve->co);
					}
				}
			}
		}
		if(totvert) {
			bDeformGroup *defGroup = BLI_findlink(&obedit->defbase, obedit->actdef-1);
			strcpy(name, defGroup->name);
			VecMulf(cent, 1.0f/(float)totvert);
			return 1;
		}
	}
	
	return 0;
}	

static void select_editmesh_hook(Object *ob, HookModifierData *hmd)
{
	Mesh *me= ob->data;
	EditMesh *em= BKE_mesh_get_editmesh(me);
	EditVert *eve;
	int index=0, nr=0;
	
	if (hmd->indexar == NULL)
		return;
	
	for(eve= em->verts.first; eve; eve= eve->next, nr++) {
		if(nr==hmd->indexar[index]) {
			eve->f |= SELECT;
			if(index < hmd->totindex-1) index++;
		}
	}
	EM_select_flush(em);

	BKE_mesh_end_editmesh(me, em);
}

static int return_editlattice_indexar(Lattice *editlatt, int *tot, int **indexar, float *cent)
{
	BPoint *bp;
	int *index, nr, totvert=0, a;
	
	/* count */
	a= editlatt->pntsu*editlatt->pntsv*editlatt->pntsw;
	bp= editlatt->def;
	while(a--) {
		if(bp->f1 & SELECT) {
			if(bp->hide==0) totvert++;
		}
		bp++;
	}

	if(totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	cent[0]= cent[1]= cent[2]= 0.0;
	
	a= editlatt->pntsu*editlatt->pntsv*editlatt->pntsw;
	bp= editlatt->def;
	while(a--) {
		if(bp->f1 & SELECT) {
			if(bp->hide==0) {
				*index= nr; index++;
				VecAddf(cent, cent, bp->vec);
			}
		}
		bp++;
		nr++;
	}
	
	VecMulf(cent, 1.0f/(float)totvert);
	
	return totvert;
}

static void select_editlattice_hook(Object *obedit, HookModifierData *hmd)
{
	Lattice *lt= obedit->data;
	BPoint *bp;
	int index=0, nr=0, a;
	
	/* count */
	a= lt->editlatt->pntsu*lt->editlatt->pntsv*lt->editlatt->pntsw;
	bp= lt->editlatt->def;
	while(a--) {
		if(hmd->indexar[index]==nr) {
			bp->f1 |= SELECT;
			if(index < hmd->totindex-1) index++;
		}
		nr++;
		bp++;
	}
}

static int return_editcurve_indexar(Object *obedit, int *tot, int **indexar, float *cent)
{
	ListBase *editnurb= curve_get_editcurve(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int *index, a, nr, totvert=0;
	
	for(nu= editnurb->first; nu; nu= nu->next) {
		if(nu->type == CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(bezt->f1 & SELECT) totvert++;
				if(bezt->f2 & SELECT) totvert++;
				if(bezt->f3 & SELECT) totvert++;
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				if(bp->f1 & SELECT) totvert++;
				bp++;
			}
		}
	}
	if(totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	cent[0]= cent[1]= cent[2]= 0.0;
	
	for(nu= editnurb->first; nu; nu= nu->next) {
		if(nu->type == CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(bezt->f1 & SELECT) {
					*index= nr; index++;
					VecAddf(cent, cent, bezt->vec[0]);
				}
				nr++;
				if(bezt->f2 & SELECT) {
					*index= nr; index++;
					VecAddf(cent, cent, bezt->vec[1]);
				}
				nr++;
				if(bezt->f3 & SELECT) {
					*index= nr; index++;
					VecAddf(cent, cent, bezt->vec[2]);
				}
				nr++;
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				if(bp->f1 & SELECT) {
					*index= nr; index++;
					VecAddf(cent, cent, bp->vec);
				}
				nr++;
				bp++;
			}
		}
	}
	
	VecMulf(cent, 1.0f/(float)totvert);
	
	return totvert;
}

int object_hook_index_array(Object *obedit, int *tot, int **indexar, char *name, float *cent_r)
{
	*indexar= NULL;
	*tot= 0;
	name[0]= 0;
	
	switch(obedit->type) {
		case OB_MESH:
		{
			Mesh *me= obedit->data;
			EditMesh *em = BKE_mesh_get_editmesh(me);

			/* check selected vertices first */
			if( return_editmesh_indexar(em, tot, indexar, cent_r)) {
				BKE_mesh_end_editmesh(me, em);
				return 1;
			} else {
				int ret = return_editmesh_vgroup(obedit, em, name, cent_r);
				BKE_mesh_end_editmesh(me, em);
				return ret;
			}
		}
		case OB_CURVE:
		case OB_SURF:
			return return_editcurve_indexar(obedit, tot, indexar, cent_r);
		case OB_LATTICE:
		{
			Lattice *lt= obedit->data;
			return return_editlattice_indexar(lt->editlatt, tot, indexar, cent_r);
		}
		default:
			return 0;
	}
}

static void select_editcurve_hook(Object *obedit, HookModifierData *hmd)
{
	ListBase *editnurb= curve_get_editcurve(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int index=0, a, nr=0;
	
	for(nu= editnurb->first; nu; nu= nu->next) {
		if(nu->type == CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(nr == hmd->indexar[index]) {
					bezt->f1 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;
				if(nr == hmd->indexar[index]) {
					bezt->f2 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;
				if(nr == hmd->indexar[index]) {
					bezt->f3 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;
				
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				if(nr == hmd->indexar[index]) {
					bp->f1 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;
				bp++;
			}
		}
	}
}

void object_hook_select(Object *ob, HookModifierData *hmd) 
{
	if (hmd->indexar == NULL)
		return;
	
	if(ob->type==OB_MESH) select_editmesh_hook(ob, hmd);
	else if(ob->type==OB_LATTICE) select_editlattice_hook(ob, hmd);
	else if(ob->type==OB_CURVE) select_editcurve_hook(ob, hmd);
	else if(ob->type==OB_SURF) select_editcurve_hook(ob, hmd);
}


void add_hook(Scene *scene, View3D *v3d, int mode)
{
	ModifierData *md = NULL;
	HookModifierData *hmd = NULL;
	Object *ob=NULL;
	Object *obedit= scene->obedit;  // XXX get from context
	
	if(obedit==NULL) return;
	
	/* preconditions */
	if(mode==2) { /* selected object */
		Base *base;
		for(base= FIRSTBASE; base; base= base->next) {
			if(TESTBASELIB(v3d, base)) {
				if(base!=BASACT) {
					ob= base->object;
					break;
				}
			}
		}
		if(ob==NULL) {
			// XXX error("Requires selected Object");
			return;
		}
	}
	else if(mode!=1) {
		int maxlen=0, a, nr;
		char *cp;
		
		/* make pupmenu with hooks */
		for(md=obedit->modifiers.first; md; md= md->next) {
			if (md->type==eModifierType_Hook) 
				maxlen+=32;
		}
		
		if(maxlen==0) {
			// XXX error("Object has no hooks yet");
			return;
		}
		
		cp= MEM_callocN(maxlen+32, "temp string");
		if(mode==3) strcpy(cp, "Remove %t|");
		else if(mode==4) strcpy(cp, "Reassign %t|");
		else if(mode==5) strcpy(cp, "Select %t|");
		else if(mode==6) strcpy(cp, "Clear Offset %t|");
		
		for(md=obedit->modifiers.first; md; md= md->next) {
			if (md->type==eModifierType_Hook) {
				strcat(cp, md->name);
				strcat(cp, " |");
			}
		}
		
		nr= 0; // XXX pupmenu(cp);
		MEM_freeN(cp);
		
		if(nr<1) return;
		
		a= 1;
		for(md=obedit->modifiers.first; md; md=md->next) {
			if (md->type==eModifierType_Hook) {
				if(a==nr) break;
				a++;
			}
		}
		
		hmd = (HookModifierData*) md;
		ob= hmd->object;
	}

	/* do it, new hooks or reassign */
	if(mode==1 || mode==2 || mode==4) {
		float cent[3];
		int tot, ok, *indexar;
		char name[32];
		
		ok = object_hook_index_array(obedit, &tot, &indexar, name, cent);
		
		if(ok==0) {
			// XXX error("Requires selected vertices or active Vertex Group");
		}
		else {
			
			if(mode==1) {
				Base *base= BASACT, *newbase;
				
				ob= add_object(scene, OB_EMPTY);
				/* set layers OK */
				newbase= BASACT;
				newbase->lay= base->lay;
				ob->lay= newbase->lay;
				
				/* transform cent to global coords for loc */
				VecMat4MulVecfl(ob->loc, obedit->obmat, cent);
				
				/* restore, add_object sets active */
				BASACT= base;
			}
			/* if mode is 2 or 4, ob has been set */
			
			/* new hook */
			if(mode==1 || mode==2) {
				ModifierData *md = obedit->modifiers.first;
				
				while (md && modifierType_getInfo(md->type)->type==eModifierTypeType_OnlyDeform) {
					md = md->next;
				}
				
				hmd = (HookModifierData*) modifier_new(eModifierType_Hook);
				BLI_insertlinkbefore(&obedit->modifiers, md, hmd);
				sprintf(hmd->modifier.name, "Hook-%s", ob->id.name+2);
				modifier_unique_name(&obedit->modifiers, (ModifierData*)hmd);
			}
			else if (hmd->indexar) MEM_freeN(hmd->indexar); /* reassign, hook was set */
		
			hmd->object= ob;
			hmd->indexar= indexar;
			VecCopyf(hmd->cent, cent);
			hmd->totindex= tot;
			BLI_strncpy(hmd->name, name, 32);
			
			// TODO: need to take into account bone targets here too now...
			if(mode==1 || mode==2) {
				/* matrix calculus */
				/* vert x (obmat x hook->imat) x hook->obmat x ob->imat */
				/*        (parentinv         )                          */
				
				where_is_object(scene, ob);
				
				Mat4Invert(ob->imat, ob->obmat);
				/* apparently this call goes from right to left... */
				Mat4MulSerie(hmd->parentinv, ob->imat, obedit->obmat, NULL, 
							 NULL, NULL, NULL, NULL, NULL);
			}
		}
	}
	else if(mode==3) { /* remove */
		BLI_remlink(&obedit->modifiers, md);
		modifier_free(md);
	}
	else if(mode==5) { /* select */
		// FIXME: this is now OBJECT_OT_hook_select
		object_hook_select(obedit, hmd);
	}
	else if(mode==6) { /* clear offset */
		// FIXME: this is now OBJECT_OT_hook_reset operator
		where_is_object(scene, ob);	/* ob is hook->parent */

		Mat4Invert(ob->imat, ob->obmat);
		/* this call goes from right to left... */
		Mat4MulSerie(hmd->parentinv, ob->imat, obedit->obmat, NULL, 
					 NULL, NULL, NULL, NULL, NULL);
	}

	DAG_scene_sort(scene);
}

void add_hook_menu(Scene *scene, View3D *v3d)
{
	Object *obedit= scene->obedit;  // XXX get from context
	int mode;
	
	if(obedit==NULL) return;
	
	if(modifiers_findByType(obedit, eModifierType_Hook))
		mode= 0; // XXX pupmenu("Hooks %t|Add, To New Empty %x1|Add, To Selected Object %x2|Remove... %x3|Reassign... %x4|Select... %x5|Clear Offset...%x6");
	else
		mode= 0; // XXX pupmenu("Hooks %t|Add, New Empty %x1|Add, To Selected Object %x2");

	if(mode<1) return;
		
	/* do operations */
	add_hook(scene, v3d, mode);
}

void hookmenu(Scene *scene, View3D *v3d)
{
	/* only called in object mode */
	short event, changed=0;
	Object *ob;
	Base *base;
	ModifierData *md;
	HookModifierData *hmd;
	
	event= 0; // XXX pupmenu("Modify Hooks for Selected...%t|Reset Offset%x1|Recenter at Cursor%x2");
	if (event==-1) return;
	if (event==2 && !(v3d)) {
		// XXX error("Cannot perform this operation without a 3d view");
		return;
	}
	
	for (base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			for (md = base->object->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Hook) {
					ob = base->object;
					hmd = (HookModifierData*) md;
					
					/*
					 * Copied from modifiers_cursorHookCenter and
					 * modifiers_clearHookOffset, should consolidate
					 * */
					
					if (event==1) {
						if(hmd->object) {
							Mat4Invert(hmd->object->imat, hmd->object->obmat);
							Mat4MulSerie(hmd->parentinv, hmd->object->imat, ob->obmat, NULL, NULL, NULL, NULL, NULL, NULL);
							
							changed= 1;
							DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
						}
					} else {
						float *curs = give_cursor(scene, v3d);
						float bmat[3][3], imat[3][3];
						
						where_is_object(scene, ob);
					
						Mat3CpyMat4(bmat, ob->obmat);
						Mat3Inv(imat, bmat);
				
						curs= give_cursor(scene, v3d);
						hmd->cent[0]= curs[0]-ob->obmat[3][0];
						hmd->cent[1]= curs[1]-ob->obmat[3][1];
						hmd->cent[2]= curs[2]-ob->obmat[3][2];
						Mat3MulVecfl(imat, hmd->cent);
						
						changed= 1;
						DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
					} 
				}
			}
		}
	}
	
	if (changed) {
	}	
}

