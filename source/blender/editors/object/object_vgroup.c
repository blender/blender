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

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"
#include "DNA_particle_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_paint.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_view3d.h"

#include "UI_interface.h"

#include "object_intern.h"

/************************ Exported Functions **********************/

static Lattice *vgroup_edit_lattice(Object *ob)
{
	if(ob->type==OB_LATTICE) {
		Lattice *lt= ob->data;
		return (lt->editlatt)? lt->editlatt: lt;
	}

	return NULL;
}

/* check if deform vertex has defgroup index */
MDeformWeight *ED_vgroup_weight_get(MDeformVert *dv, int defgroup)
{
	int i;
	
	if(!dv || defgroup<0)
		return NULL;
	
	for(i=0; i<dv->totweight; i++)
		if(dv->dw[i].def_nr == defgroup)
			return dv->dw+i;

	return NULL;
}

/* Ensures that mv has a deform weight entry for the specified defweight group */
/* Note this function is mirrored in editmesh_tools.c, for use for editvertices */
MDeformWeight *ED_vgroup_weight_verify(MDeformVert *dv, int defgroup)
{
	MDeformWeight *newdw;

	/* do this check always, this function is used to check for it */
	if(!dv || defgroup<0)
		return NULL;
	
	newdw = ED_vgroup_weight_get(dv, defgroup);
	if(newdw)
		return newdw;
	
	newdw = MEM_callocN(sizeof(MDeformWeight)*(dv->totweight+1), "deformWeight");
	if(dv->dw) {
		memcpy(newdw, dv->dw, sizeof(MDeformWeight)*dv->totweight);
		MEM_freeN(dv->dw);
	}
	dv->dw=newdw;
	
	dv->dw[dv->totweight].weight=0.0f;
	dv->dw[dv->totweight].def_nr=defgroup;
	/* Group index */
	
	dv->totweight++;

	return dv->dw+(dv->totweight-1);
}

bDeformGroup *ED_vgroup_add_name(Object *ob, char *name)
{
	bDeformGroup *defgroup;
	
	if(!ob)
		return NULL;
	
	defgroup = MEM_callocN(sizeof(bDeformGroup), "add deformGroup");

	BLI_strncpy(defgroup->name, name, 32);

	BLI_addtail(&ob->defbase, defgroup);
	unique_vertexgroup_name(defgroup, ob);

	ob->actdef = BLI_countlist(&ob->defbase);

	return defgroup;
}

bDeformGroup *ED_vgroup_add(Object *ob) 
{
	return ED_vgroup_add_name(ob, "Group");
}

void ED_vgroup_data_create(ID *id)
{
	/* create deform verts */

	if(GS(id->name)==ID_ME) {
		Mesh *me= (Mesh *)id;
		me->dvert= CustomData_add_layer(&me->vdata, CD_MDEFORMVERT, CD_CALLOC, NULL, me->totvert);
	}
	else if(GS(id->name)==ID_LT) {
		Lattice *lt= (Lattice *)id;
		lt->dvert= MEM_callocN(sizeof(MDeformVert)*lt->pntsu*lt->pntsv*lt->pntsw, "lattice deformVert");
	}
}

/* for mesh in object mode
   lattice can be in editmode */
void ED_vgroup_nr_vert_remove(Object *ob, int def_nr, int vertnum)
{
	/* This routine removes the vertex from the deform
	 * group with number def_nr.
	 *
	 * This routine is meant to be fast, so it is the
	 * responsibility of the calling routine to:
	 *   a) test whether ob is non-NULL
	 *   b) test whether ob is a mesh
	 *   c) calculate def_nr
	 */

	MDeformWeight *newdw;
	MDeformVert *dvert= NULL;
	int i;

	/* get the deform vertices corresponding to the
	 * vertnum
	 */
	if(ob->type==OB_MESH) {
		if(((Mesh*)ob->data)->dvert)
			dvert = ((Mesh*)ob->data)->dvert + vertnum;
	}
	else if(ob->type==OB_LATTICE) {
		Lattice *lt= vgroup_edit_lattice(ob);
		
		if(lt->dvert)
			dvert = lt->dvert + vertnum;
	}
	
	if(dvert==NULL)
		return;
	
	/* for all of the deform weights in the
	 * deform vert
	 */
	for(i=dvert->totweight - 1 ; i>=0 ; i--){

		/* if the def_nr is the same as the one
		 * for our weight group then remove it
		 * from this deform vert.
		 */
		if(dvert->dw[i].def_nr == def_nr) {
			dvert->totweight--;
        
			/* if there are still other deform weights
			 * attached to this vert then remove this
			 * deform weight, and reshuffle the others
			 */
			if(dvert->totweight) {
				newdw = MEM_mallocN(sizeof(MDeformWeight)*(dvert->totweight), 
									 "deformWeight");
				if(dvert->dw){
					memcpy(newdw, dvert->dw, sizeof(MDeformWeight)*i);
					memcpy(newdw+i, dvert->dw+i+1, 
							sizeof(MDeformWeight)*(dvert->totweight-i));
					MEM_freeN(dvert->dw);
				}
				dvert->dw=newdw;
			}
			/* if there are no other deform weights
			 * left then just remove the deform weight
			 */
			else {
				MEM_freeN(dvert->dw);
				dvert->dw = NULL;
				break;
			}
		}
	}

}

/* for Mesh in Object mode */
/* allows editmode for Lattice */
void ED_vgroup_nr_vert_add(Object *ob, int def_nr, int vertnum, float weight, int assignmode)
{
	/* add the vert to the deform group with the
	 * specified number
	 */
	MDeformVert *dv= NULL;
	MDeformWeight *newdw;
	int	i;

	/* get the vert */
	if(ob->type==OB_MESH) {
		if(((Mesh*)ob->data)->dvert)
			dv = ((Mesh*)ob->data)->dvert + vertnum;
	}
	else if(ob->type==OB_LATTICE) {
		Lattice *lt= vgroup_edit_lattice(ob);
		
		if(lt->dvert)
			dv = lt->dvert + vertnum;
	}
	
	if(dv==NULL)
		return;
	
	/* Lets first check to see if this vert is
	 * already in the weight group -- if so
	 * lets update it
	 */
	for(i=0; i<dv->totweight; i++){
		
		/* if this weight cooresponds to the
		 * deform group, then add it using
		 * the assign mode provided
		 */
		if(dv->dw[i].def_nr == def_nr){
			
			switch(assignmode) {
			case WEIGHT_REPLACE:
				dv->dw[i].weight=weight;
				break;
			case WEIGHT_ADD:
				dv->dw[i].weight+=weight;
				if(dv->dw[i].weight >= 1.0)
					dv->dw[i].weight = 1.0;
				break;
			case WEIGHT_SUBTRACT:
				dv->dw[i].weight-=weight;
				/* if the weight is zero or less then
				 * remove the vert from the deform group
				 */
				if(dv->dw[i].weight <= 0.0)
					ED_vgroup_nr_vert_remove(ob, def_nr, vertnum);
				break;
			}
			return;
		}
	}

	/* if the vert wasn't in the deform group then
	 * we must take a different form of action ...
	 */

	switch(assignmode) {
	case WEIGHT_SUBTRACT:
		/* if we are subtracting then we don't
		 * need to do anything
		 */
		return;

	case WEIGHT_REPLACE:
	case WEIGHT_ADD:
		/* if we are doing an additive assignment, then
		 * we need to create the deform weight
		 */
		newdw = MEM_callocN(sizeof(MDeformWeight)*(dv->totweight+1), 
							 "deformWeight");
		if(dv->dw){
			memcpy(newdw, dv->dw, sizeof(MDeformWeight)*dv->totweight);
			MEM_freeN(dv->dw);
		}
		dv->dw=newdw;
    
		dv->dw[dv->totweight].weight=weight;
		dv->dw[dv->totweight].def_nr=def_nr;
    
		dv->totweight++;
		break;
	}
}

/* called while not in editmode */
void ED_vgroup_vert_add(Object *ob, bDeformGroup *dg, int vertnum, float weight, int assignmode)
{
	/* add the vert to the deform group with the
	 * specified assign mode
	 */
	int	def_nr;

	/* get the deform group number, exit if
	 * it can't be found
	 */
	def_nr = get_defgroup_num(ob, dg);
	if(def_nr < 0) return;

	/* if there's no deform verts then
	 * create some
	 */
	if(ob->type==OB_MESH) {
		if(!((Mesh*)ob->data)->dvert)
			ED_vgroup_data_create(ob->data);
	}
	else if(ob->type==OB_LATTICE) {
		if(!((Lattice*)ob->data)->dvert)
			ED_vgroup_data_create(ob->data);
	}

	/* call another function to do the work
	 */
	ED_vgroup_nr_vert_add(ob, def_nr, vertnum, weight, assignmode);
}

/* mesh object mode, lattice can be in editmode */
void ED_vgroup_vert_remove(Object *ob, bDeformGroup	*dg, int vertnum)
{
	/* This routine removes the vertex from the specified
	 * deform group.
	 */

	int def_nr;

	/* if the object is NULL abort
	 */
	if(!ob)
		return;

	/* get the deform number that cooresponds
	 * to this deform group, and abort if it
	 * can not be found.
	 */
	def_nr = get_defgroup_num(ob, dg);
	if(def_nr < 0) return;

	/* call another routine to do the work
	 */
	ED_vgroup_nr_vert_remove(ob, def_nr, vertnum);
}

static float get_vert_def_nr(Object *ob, int def_nr, int vertnum)
{
	MDeformVert *dvert= NULL;
	EditVert *eve;
	Mesh *me;
	int i;

	/* get the deform vertices corresponding to the vertnum */
	if(ob->type==OB_MESH) {
		me= ob->data;

		if(me->edit_mesh) {
			eve= BLI_findlink(&me->edit_mesh->verts, vertnum);
			if(!eve) return 0.0f;
			dvert= CustomData_em_get(&me->edit_mesh->vdata, eve->data, CD_MDEFORMVERT);
			vertnum= 0;
		}
		else
			dvert = me->dvert;
	}
	else if(ob->type==OB_LATTICE) {
		Lattice *lt= vgroup_edit_lattice(ob);
		
		if(lt->dvert)
			dvert = lt->dvert;
	}
	
	if(dvert==NULL)
		return 0.0f;
	
	dvert += vertnum;
	
	for(i=dvert->totweight-1 ; i>=0 ; i--)
		if(dvert->dw[i].def_nr == def_nr)
			return dvert->dw[i].weight;

	return 0.0f;
}

float ED_vgroup_vert_weight(Object *ob, bDeformGroup *dg, int vertnum)
{
	int def_nr;

	if(!ob) return 0.0f;

	def_nr = get_defgroup_num(ob, dg);
	if(def_nr < 0) return 0.0f;

	return get_vert_def_nr(ob, def_nr, vertnum);
}

void ED_vgroup_select_by_name(Object *ob, char *name)
{
	bDeformGroup *curdef;
	int actdef= 1;
	
	for(curdef = ob->defbase.first; curdef; curdef=curdef->next, actdef++){
		if(!strcmp(curdef->name, name)) {
			ob->actdef= actdef;
			return;
		}
	}

	ob->actdef=0;	// this signals on painting to create a new one, if a bone in posemode is selected */
}

/********************** Operator Implementations *********************/

/* only in editmode */
static void vgroup_select_verts(Object *ob, int select)
{
	EditVert *eve;
	MDeformVert *dvert;
	int i;

	if(ob->type == OB_MESH) {
		Mesh *me= ob->data;
		EditMesh *em = BKE_mesh_get_editmesh(me);

		for(eve=em->verts.first; eve; eve=eve->next){
			dvert= CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT);

			if(dvert && dvert->totweight){
				for(i=0; i<dvert->totweight; i++){
					if(dvert->dw[i].def_nr == (ob->actdef-1)){
						if(select) eve->f |= SELECT;
						else eve->f &= ~SELECT;
						
						break;
					}
				}
			}
		}
		/* this has to be called, because this function operates on vertices only */
		if(select) EM_select_flush(em);	// vertices to edges/faces
		else EM_deselect_flush(em);

		BKE_mesh_end_editmesh(me, em);
	}
	else if(ob->type == OB_LATTICE) {
		Lattice *lt= vgroup_edit_lattice(ob);
		
		if(lt->dvert) {
			BPoint *bp;
			int a, tot;
			
			dvert= lt->dvert;

			tot= lt->pntsu*lt->pntsv*lt->pntsw;
			for(a=0, bp= lt->def; a<tot; a++, bp++, dvert++) {
				for(i=0; i<dvert->totweight; i++){
					if(dvert->dw[i].def_nr == (ob->actdef-1)) {
						if(select) bp->f1 |= SELECT;
						else bp->f1 &= ~SELECT;
						
						break;
					}
				}
			}
		}
	}
}

static void vgroup_duplicate(Object *ob)
{
	bDeformGroup *dg, *cdg;
	char name[32], s[32];
	MDeformWeight *org, *cpy;
	MDeformVert *dvert, *dvert_array=NULL;
	int i, idg, icdg, dvert_tot=0;

	dg = BLI_findlink(&ob->defbase, (ob->actdef-1));
	if(!dg)
		return;
	
	if(strstr(dg->name, "_copy")) {
		BLI_strncpy(name, dg->name, 32); /* will be renamed _copy.001... etc */
	}
	else {
		BLI_snprintf(name, 32, "%s_copy", dg->name);
		while(get_named_vertexgroup(ob, name)) {
			if((strlen(name) + 6) > 32) {
				printf("Internal error: the name for the new vertex group is > 32 characters");
				return;
			}
			strcpy(s, name);
			BLI_snprintf(name, 32, "%s_copy", s);
		}
	}		

	cdg = copy_defgroup(dg);
	strcpy(cdg->name, name);
	unique_vertexgroup_name(cdg, ob);
	
	BLI_addtail(&ob->defbase, cdg);

	idg = (ob->actdef-1);
	ob->actdef = BLI_countlist(&ob->defbase);
	icdg = (ob->actdef-1);

	if(ob->type == OB_MESH) {
		Mesh *me = get_mesh(ob);
		dvert_array= me->dvert;
		dvert_tot= me->totvert;
	}
	else if(ob->type == OB_LATTICE) {
		Lattice *lt= (Lattice *)ob->data;
		dvert_array= lt->dvert;
		dvert_tot= lt->pntsu*lt->pntsv*lt->pntsw;
	}
	
	if(!dvert_array)
		return;

	for(i = 0; i < dvert_tot; i++) {
		dvert = dvert_array+i;
		org = ED_vgroup_weight_get(dvert, idg);
		if(org) {
			float weight = org->weight;
			/* ED_vgroup_weight_verify re-allocs org so need to store the weight first */
			cpy = ED_vgroup_weight_verify(dvert, icdg);
			cpy->weight = weight;
		}
	}
}

static void vgroup_delete_update_users(Object *ob, int id)
{
	ExplodeModifierData *emd;
	ModifierData *md;
	ParticleSystem *psys;
	ClothModifierData *clmd;
	ClothSimSettings *clsim;
	int a;

	/* these cases don't use names to refer to vertex groups, so when
	 * they get deleted the numbers get out of sync, this corrects that */

	if(ob->soft) {
		if(ob->soft->vertgroup == id)
			ob->soft->vertgroup= 0;
		else if(ob->soft->vertgroup > id)
			ob->soft->vertgroup--;
	}

	for(md=ob->modifiers.first; md; md=md->next) {
		if(md->type == eModifierType_Explode) {
			emd= (ExplodeModifierData*)md;

			if(emd->vgroup == id)
				emd->vgroup= 0;
			else if(emd->vgroup > id)
				emd->vgroup--;
		}
		else if(md->type == eModifierType_Cloth) {
			clmd= (ClothModifierData*)md;
			clsim= clmd->sim_parms;

			if(clsim) {
				if(clsim->vgroup_mass == id)
					clsim->vgroup_mass= 0;
				else if(clsim->vgroup_mass > id)
					clsim->vgroup_mass--;

				if(clsim->vgroup_bend == id)
					clsim->vgroup_bend= 0;
				else if(clsim->vgroup_bend > id)
					clsim->vgroup_bend--;

				if(clsim->vgroup_struct == id)
					clsim->vgroup_struct= 0;
				else if(clsim->vgroup_struct > id)
					clsim->vgroup_struct--;
			}
		}
	}

	for(psys=ob->particlesystem.first; psys; psys=psys->next) {
		for(a=0; a<PSYS_TOT_VG; a++)
			if(psys->vgroup[a] == id)
				psys->vgroup[a]= 0;
			else if(psys->vgroup[a] > id)
				psys->vgroup[a]--;
	}
}

static void vgroup_delete_object_mode(Object *ob)
{
	bDeformGroup *dg;
	MDeformVert *dvert, *dvert_array=NULL;
	int i, e, dvert_tot=0;

	if(ob->type == OB_MESH) {
		Mesh *me = get_mesh(ob);
		dvert_array= me->dvert;
		dvert_tot= me->totvert;
	}
	else if(ob->type == OB_LATTICE) {
		Lattice *lt= (Lattice *)ob->data;
		dvert_array= lt->dvert;
		dvert_tot= lt->pntsu*lt->pntsv*lt->pntsw;
	}
	
	dg = BLI_findlink(&ob->defbase, (ob->actdef-1));
	if(!dg)
		return;
	
	if(dvert_array) {
		for(i = 0; i < dvert_tot; i++) {
			dvert = dvert_array + i;
			if(dvert) {
				if(ED_vgroup_weight_get(dvert, (ob->actdef-1)))
					ED_vgroup_vert_remove(ob, dg, i);
			}
		}

		for(i = 0; i < dvert_tot; i++) {
			dvert = dvert_array+i;
			if(dvert) {
				for(e = 0; e < dvert->totweight; e++) {
					if(dvert->dw[e].def_nr > (ob->actdef-1))
						dvert->dw[e].def_nr--;
				}
			}
		}
	}

	vgroup_delete_update_users(ob, ob->actdef);

	/* Update the active deform index if necessary */
	if(ob->actdef == BLI_countlist(&ob->defbase))
		ob->actdef--;
  
	/* Remove the group */
	BLI_freelinkN(&ob->defbase, dg);
}

/* only in editmode */
/* removes from active defgroup, if allverts==0 only selected vertices */
static void vgroup_active_remove_verts(Object *ob, int allverts)
{
	EditVert *eve;
	MDeformVert *dvert;
	MDeformWeight *newdw;
	bDeformGroup *dg, *eg;
	int	i;

	dg=BLI_findlink(&ob->defbase, ob->actdef-1);
	if(!dg)
		return;

	if(ob->type == OB_MESH) {
		Mesh *me= ob->data;
		EditMesh *em = BKE_mesh_get_editmesh(me);

		for(eve=em->verts.first; eve; eve=eve->next){
			dvert= CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT);
		
			if(dvert && dvert->dw && ((eve->f & 1) || allverts)){
				for(i=0; i<dvert->totweight; i++){
					/* Find group */
					eg = BLI_findlink(&ob->defbase, dvert->dw[i].def_nr);
					if(eg == dg){
						dvert->totweight--;
						if(dvert->totweight){
							newdw = MEM_mallocN(sizeof(MDeformWeight)*(dvert->totweight), "deformWeight");
							
							if(dvert->dw){
								memcpy(newdw, dvert->dw, sizeof(MDeformWeight)*i);
								memcpy(newdw+i, dvert->dw+i+1, sizeof(MDeformWeight)*(dvert->totweight-i));
								MEM_freeN(dvert->dw);
							}
							dvert->dw=newdw;
						}
						else{
							MEM_freeN(dvert->dw);
							dvert->dw=NULL;
							break;
						}
					}
				}
			}
		}
		BKE_mesh_end_editmesh(me, em);
	}
	else if(ob->type == OB_LATTICE) {
		Lattice *lt= vgroup_edit_lattice(ob);
		
		if(lt->dvert) {
			BPoint *bp;
			int a, tot= lt->pntsu*lt->pntsv*lt->pntsw;
				
			for(a=0, bp= lt->def; a<tot; a++, bp++) {
				if(allverts || (bp->f1 & SELECT))
					ED_vgroup_vert_remove(ob, dg, a);
			}
		}
	}
}

static void vgroup_delete_edit_mode(Object *ob)
{
	bDeformGroup *defgroup;
	int i;

	if(!ob->actdef)
		return;

	defgroup = BLI_findlink(&ob->defbase, ob->actdef-1);
	if(!defgroup)
		return;

	/* Make sure that no verts are using this group */
	vgroup_active_remove_verts(ob, 1);

	/* Make sure that any verts with higher indices are adjusted accordingly */
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		EditMesh *em = BKE_mesh_get_editmesh(me);
		EditVert *eve;
		MDeformVert *dvert;
		
		for(eve=em->verts.first; eve; eve=eve->next){
			dvert= CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT);

			if(dvert)
				for(i=0; i<dvert->totweight; i++)
					if(dvert->dw[i].def_nr > (ob->actdef-1))
						dvert->dw[i].def_nr--;
		}
		BKE_mesh_end_editmesh(me, em);
	}
	else if(ob->type==OB_LATTICE) {
		Lattice *lt= vgroup_edit_lattice(ob);
		BPoint *bp;
		MDeformVert *dvert= lt->dvert;
		int a, tot;
		
		if(dvert) {
			tot= lt->pntsu*lt->pntsv*lt->pntsw;
			for(a=0, bp= lt->def; a<tot; a++, bp++, dvert++) {
				for(i=0; i<dvert->totweight; i++){
					if(dvert->dw[i].def_nr > (ob->actdef-1))
						dvert->dw[i].def_nr--;
				}
			}
		}
	}

	vgroup_delete_update_users(ob, ob->actdef);

	/* Update the active deform index if necessary */
	if(ob->actdef==BLI_countlist(&ob->defbase))
		ob->actdef--;
	
	/* Remove the group */
	BLI_freelinkN (&ob->defbase, defgroup);
	
	/* remove all dverts */
	if(ob->actdef==0) {
		if(ob->type==OB_MESH) {
			Mesh *me= ob->data;
			CustomData_free_layer_active(&me->vdata, CD_MDEFORMVERT, me->totvert);
			me->dvert= NULL;
		}
		else if(ob->type==OB_LATTICE) {
			Lattice *lt= vgroup_edit_lattice(ob);
			if(lt->dvert) {
				MEM_freeN(lt->dvert);
				lt->dvert= NULL;
			}
		}
	}
}

static int vgroup_object_in_edit_mode(Object *ob)
{
	if(ob->type == OB_MESH)
		return (((Mesh*)ob->data)->edit_mesh != NULL);
	else if(ob->type == OB_LATTICE)
		return (((Lattice*)ob->data)->editlatt != NULL);
	
	return 0;
}

static void vgroup_delete(Object *ob)
{
	if(vgroup_object_in_edit_mode(ob))
		vgroup_delete_edit_mode(ob);
	else
		vgroup_delete_object_mode(ob);
}

static void vgroup_delete_all(Object *ob)
{
	/* Remove all DVerts */
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		CustomData_free_layer_active(&me->vdata, CD_MDEFORMVERT, me->totvert);
		me->dvert= NULL;
	}
	else if(ob->type==OB_LATTICE) {
		Lattice *lt= vgroup_edit_lattice(ob);
		if(lt->dvert) {
			MEM_freeN(lt->dvert);
			lt->dvert= NULL;
		}
	}
	
	/* Remove all DefGroups */
	BLI_freelistN(&ob->defbase);
	
	/* Fix counters/indices */
	ob->actdef= 0;
}

/* only in editmode */
static void vgroup_assign_verts(Object *ob, float weight)
{
	EditVert *eve;
	bDeformGroup *dg, *eg;
	MDeformWeight *newdw;
	MDeformVert *dvert;
	int	i, done;

	dg=BLI_findlink(&ob->defbase, ob->actdef-1);
	if(!dg)
		return;

	if(ob->type == OB_MESH) {
		Mesh *me= ob->data;
		EditMesh *em = BKE_mesh_get_editmesh(me);

		if(!CustomData_has_layer(&em->vdata, CD_MDEFORMVERT))
			EM_add_data_layer(em, &em->vdata, CD_MDEFORMVERT);

		/* Go through the list of editverts and assign them */
		for(eve=em->verts.first; eve; eve=eve->next){
			dvert= CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT);

			if(dvert && (eve->f & 1)){
				done=0;
				/* See if this vert already has a reference to this group */
				/*		If so: Change its weight */
				done=0;
				for(i=0; i<dvert->totweight; i++){
					eg = BLI_findlink(&ob->defbase, dvert->dw[i].def_nr);
					/* Find the actual group */
					if(eg==dg){
						dvert->dw[i].weight= weight;
						done=1;
						break;
					}
			 	}
				/*		If not: Add the group and set its weight */
				if(!done){
					newdw = MEM_callocN(sizeof(MDeformWeight)*(dvert->totweight+1), "deformWeight");
					if(dvert->dw){
						memcpy(newdw, dvert->dw, sizeof(MDeformWeight)*dvert->totweight);
						MEM_freeN(dvert->dw);
					}
					dvert->dw=newdw;

					dvert->dw[dvert->totweight].weight= weight;
					dvert->dw[dvert->totweight].def_nr= ob->actdef-1;

					dvert->totweight++;

				}
			}
		}
		BKE_mesh_end_editmesh(me, em);
	}
	else if(ob->type == OB_LATTICE) {
		Lattice *lt= vgroup_edit_lattice(ob);
		BPoint *bp;
		int a, tot;
		
		if(lt->dvert==NULL)
			ED_vgroup_data_create(&lt->id);
		
		tot= lt->pntsu*lt->pntsv*lt->pntsw;
		for(a=0, bp= lt->def; a<tot; a++, bp++) {
			if(bp->f1 & SELECT)
				ED_vgroup_nr_vert_add(ob, ob->actdef-1, a, weight, WEIGHT_REPLACE);
		}
	}
}

/* only in editmode */
/* removes from all defgroup, if allverts==0 only selected vertices */
static void vgroup_remove_verts(Object *ob, int allverts)
{
	int actdef, defCount;
	
	actdef= ob->actdef;
	defCount= BLI_countlist(&ob->defbase);
	
	if(defCount == 0)
		return;
	
	/* To prevent code redundancy, we just use vgroup_active_remove_verts, but that
	 * only operates on the active vgroup. So we iterate through all groups, by changing
	 * active group index
	 */
	for(ob->actdef= 1; ob->actdef <= defCount; ob->actdef++)
		vgroup_active_remove_verts(ob, allverts);
		
	ob->actdef= actdef;
}

/********************** vertex group operators *********************/

static int vertex_group_poll(bContext *C)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	ID *data= (ob)? ob->data: NULL;
	return (ob && !ob->id.lib && ELEM(ob->type, OB_MESH, OB_LATTICE) && data && !data->lib);
}

static int vertex_group_poll_edit(bContext *C)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	ID *data= (ob)? ob->data: NULL;

	if(!(ob && !ob->id.lib && data && !data->lib))
		return 0;

	return vgroup_object_in_edit_mode(ob);
}

static int vertex_group_add_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	ED_vgroup_add(ob);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Vertex Group";
	ot->idname= "OBJECT_OT_vertex_group_add";
	
	/* api callbacks */
	ot->poll= vertex_group_poll;
	ot->exec= vertex_group_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int vertex_group_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	if(RNA_boolean_get(op->ptr, "all"))
		vgroup_delete_all(ob);
	else
		vgroup_delete(ob);

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Vertex Group";
	ot->idname= "OBJECT_OT_vertex_group_remove";
	
	/* api callbacks */
	ot->poll= vertex_group_poll;
	ot->exec= vertex_group_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 0, "All", "Remove from all vertex groups.");
}

static int vertex_group_assign_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *ob= CTX_data_edit_object(C);

	if(RNA_boolean_get(op->ptr, "new"))
		ED_vgroup_add(ob);

	vgroup_assign_verts(ob, ts->vgroup_weight);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_assign(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Assign Vertex Group";
	ot->idname= "OBJECT_OT_vertex_group_assign";
	
	/* api callbacks */
	ot->poll= vertex_group_poll_edit;
	ot->exec= vertex_group_assign_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "new", 0, "New", "Assign vertex to new vertex group.");
}

static int vertex_group_remove_from_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_edit_object(C);

	vgroup_remove_verts(ob, 0);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_remove_from(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove from Vertex Group";
	ot->idname= "OBJECT_OT_vertex_group_remove_from";

	/* api callbacks */
	ot->poll= vertex_group_poll_edit;
	ot->exec= vertex_group_remove_from_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 0, "All", "Remove from all vertex groups.");
}

static int vertex_group_select_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_edit_object(C);

	if(!ob || ob->id.lib)
		return OPERATOR_CANCELLED;

	vgroup_select_verts(ob, 1);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Vertex Group";
	ot->idname= "OBJECT_OT_vertex_group_select";

	/* api callbacks */
	ot->poll= vertex_group_poll_edit;
	ot->exec= vertex_group_select_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int vertex_group_deselect_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_edit_object(C);

	vgroup_select_verts(ob, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_deselect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Deselect Vertex Group";
	ot->idname= "OBJECT_OT_vertex_group_deselect";

	/* api callbacks */
	ot->poll= vertex_group_poll_edit;
	ot->exec= vertex_group_deselect_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int vertex_group_copy_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	vgroup_duplicate(ob);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy Vertex Group";
	ot->idname= "OBJECT_OT_vertex_group_copy";

	/* api callbacks */
	ot->poll= vertex_group_poll;
	ot->exec= vertex_group_copy_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int vertex_group_copy_to_linked_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Base *base;
	int retval= OPERATOR_CANCELLED;

	for(base=scene->base.first; base; base= base->next) {
		if(base->object->type==ob->type) {
			if(base->object!=ob && base->object->data==ob->data) {
				BLI_freelistN(&base->object->defbase);
				BLI_duplicatelist(&base->object->defbase, &ob->defbase);
				base->object->actdef= ob->actdef;

				DAG_id_flush_update(&base->object->id, OB_RECALC_DATA);
				WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, base->object);
				WM_event_add_notifier(C, NC_GEOM|ND_DATA, base->object->data);

				retval = OPERATOR_FINISHED;
			}
		}
	}

	return retval;
}

void OBJECT_OT_vertex_group_copy_to_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy Vertex Group to Linked";
	ot->idname= "OBJECT_OT_vertex_group_copy_to_linked";

	/* api callbacks */
	ot->poll= vertex_group_poll;
	ot->exec= vertex_group_copy_to_linked_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static EnumPropertyItem vgroup_items[]= {
	{0, NULL, 0, NULL, NULL}};

static int set_active_group_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	int nr= RNA_enum_get(op->ptr, "group");

	ob->actdef= nr+1;

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

static EnumPropertyItem *vgroup_itemf(bContext *C, PointerRNA *ptr, int *free)
{	
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	EnumPropertyItem *item= NULL;
	bDeformGroup *def;
	int a, totitem= 0;
	
	if(!ob)
		return vgroup_items;
	
	for(a=0, def=ob->defbase.first; def; def=def->next, a++) {
		tmp.value= a;
		tmp.identifier= def->name;
		tmp.name= def->name;
		RNA_enum_item_add(&item, &totitem, &tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*free= 1;

	return item;
}

void OBJECT_OT_vertex_group_set_active(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Set Active Vertex Group";
	ot->idname= "OBJECT_OT_vertex_group_set_active";

	/* api callbacks */
	ot->poll= vertex_group_poll;
	ot->exec= set_active_group_exec;
	ot->invoke= WM_menu_invoke;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	prop= RNA_def_enum(ot->srna, "group", vgroup_items, 0, "Group", "Vertex group to set as active.");
	RNA_def_enum_funcs(prop, vgroup_itemf);
}

static int vertex_group_menu_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "Vertex Groups", 0);
	layout= uiPupMenuLayout(pup);
	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

	if(vgroup_object_in_edit_mode(ob)) {
		uiItemBooleanO(layout, "Assign to New Group", 0, "OBJECT_OT_vertex_group_assign", "new", 1);

		if(BLI_countlist(&ob->defbase) && ob->actdef) {
			uiItemO(layout, "Assign to Group", 0, "OBJECT_OT_vertex_group_assign");
			uiItemO(layout, "Remove from Group", 0, "OBJECT_OT_vertex_group_remove_from");
			uiItemBooleanO(layout, "Remove from All", 0, "OBJECT_OT_vertex_group_remove_from", "all", 1);
		}
	}

	if(BLI_countlist(&ob->defbase) && ob->actdef) {
		if(vgroup_object_in_edit_mode(ob))
			uiItemS(layout);

		uiItemO(layout, "Set Active Group", 0, "OBJECT_OT_vertex_group_set_active");
		uiItemO(layout, "Remove Group", 0, "OBJECT_OT_vertex_group_remove");
		uiItemBooleanO(layout, "Remove All Groups", 0, "OBJECT_OT_vertex_group_remove", "all", 1);
	}

	uiPupMenuEnd(C, pup);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_menu(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Vertex Group Menu";
	ot->idname= "OBJECT_OT_vertex_group_menu";

	/* api callbacks */
	ot->poll= vertex_group_poll;
	ot->exec= vertex_group_menu_exec;
}

