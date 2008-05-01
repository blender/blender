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
 * Creator-specific support for vertex deformation groups
 * Added: apply deform function (ton)
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "BIF_interface.h"
#include "BIF_editdeform.h"
#include "BIF_editmesh.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BSE_edit.h"

#include "butspace.h"
#include "mydevice.h"
#include "editmesh.h"
#include "multires.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* only in editmode */
void sel_verts_defgroup (int select)
{
	EditVert *eve;
	Object *ob;
	int i;
	MDeformVert *dvert;

	ob= G.obedit;

	if (!ob)
		return;

	switch (ob->type){
	case OB_MESH:
		for (eve=G.editMesh->verts.first; eve; eve=eve->next){
			dvert= CustomData_em_get(&G.editMesh->vdata, eve->data, CD_MDEFORMVERT);

			if (dvert && dvert->totweight){
				for (i=0; i<dvert->totweight; i++){
					if (dvert->dw[i].def_nr == (ob->actdef-1)){
						if (select) eve->f |= SELECT;
						else eve->f &= ~SELECT;
						
						break;
					}
				}
			}
		}
		/* this has to be called, because this function operates on vertices only */
		if(select) EM_select_flush();	// vertices to edges/faces
		else EM_deselect_flush();
		
		break;
	case OB_LATTICE:
		if(editLatt->dvert) {
			BPoint *bp;
			int a, tot;
			
			dvert= editLatt->dvert;

			tot= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
			for(a=0, bp= editLatt->def; a<tot; a++, bp++, dvert++) {
				for (i=0; i<dvert->totweight; i++){
					if (dvert->dw[i].def_nr == (ob->actdef-1)) {
						if(select) bp->f1 |= SELECT;
						else bp->f1 &= ~SELECT;
						
						break;
					}
				}
			}
		}	
		break;
		
	default:
		break;
	}
	
	countall();
	
}

/* check if deform vertex has defgroup index */
MDeformWeight *get_defweight (MDeformVert *dv, int defgroup)
{
	int i;
	
	if (!dv || defgroup<0)
		return NULL;
	
	for (i=0; i<dv->totweight; i++){
		if (dv->dw[i].def_nr == defgroup)
			return dv->dw+i;
	}
	return NULL;
}

/* Ensures that mv has a deform weight entry for
   the specified defweight group */
/* Note this function is mirrored in editmesh_tools.c, for use for editvertices */
MDeformWeight *verify_defweight (MDeformVert *dv, int defgroup)
{
	MDeformWeight *newdw;

	/* do this check always, this function is used to check for it */
	if (!dv || defgroup<0)
		return NULL;
	
	newdw = get_defweight (dv, defgroup);
	if (newdw)
		return newdw;
	
	newdw = MEM_callocN (sizeof(MDeformWeight)*(dv->totweight+1), "deformWeight");
	if (dv->dw){
		memcpy (newdw, dv->dw, sizeof(MDeformWeight)*dv->totweight);
		MEM_freeN (dv->dw);
	}
	dv->dw=newdw;
	
	dv->dw[dv->totweight].weight=0.0f;
	dv->dw[dv->totweight].def_nr=defgroup;
	/* Group index */
	
	dv->totweight++;

	return dv->dw+(dv->totweight-1);
}

void add_defgroup (Object *ob) 
{
	add_defgroup_name (ob, "Group");
}

bDeformGroup *add_defgroup_name (Object *ob, char *name)
{
	bDeformGroup	*defgroup;
	
	if (!ob)
		return NULL;
	
	defgroup = MEM_callocN (sizeof(bDeformGroup), "add deformGroup");

	BLI_strncpy (defgroup->name, name, 32);

	BLI_addtail(&ob->defbase, defgroup);
	unique_vertexgroup_name(defgroup, ob);

	ob->actdef = BLI_countlist(&ob->defbase);

	return defgroup;
}

void duplicate_defgroup ( Object *ob )
{
	bDeformGroup *dg, *cdg;
	char name[32], s[32];
	MDeformWeight *org, *cpy;
	MDeformVert *dvert;
	Mesh *me;
	int i, idg, icdg;

	if (ob->type != OB_MESH)
		return;

	dg = BLI_findlink (&ob->defbase, (ob->actdef-1));
	if (!dg)
		return;
	
	if (strstr(dg->name, "_copy")) {
		BLI_strncpy (name, dg->name, 32); /* will be renamed _copy.001... etc */
	} else {
		BLI_snprintf (name, 32, "%s_copy", dg->name);
		while (get_named_vertexgroup (ob, name)) {
			if ((strlen (name) + 6) > 32) {
				error ("Error: the name for the new group is > 32 characters");
				return;
			}
			strcpy (s, name);
			BLI_snprintf (name, 32, "%s_copy", s);
		}
	}		

	cdg = copy_defgroup (dg);
	strcpy (cdg->name, name);
	unique_vertexgroup_name(cdg, ob);
	
	BLI_addtail (&ob->defbase, cdg);

	idg = (ob->actdef-1);
	ob->actdef = BLI_countlist (&ob->defbase);
	icdg = (ob->actdef-1);

	me = get_mesh (ob);
	if (!me->dvert)
		return;

	for (i = 0; i < me->totvert; i++) {
		dvert = me->dvert+i;
		org = get_defweight (dvert, idg);
		if (org) {
			cpy = verify_defweight (dvert, icdg);
			cpy->weight = org->weight;
		}
	}
}

void del_defgroup_in_object_mode ( Object *ob )
{
	bDeformGroup *dg;
	MDeformVert *dvert;
	Mesh *me;
	int i, e;

	if ((!ob) || (ob->type != OB_MESH))
		return;

	dg = BLI_findlink (&ob->defbase, (ob->actdef-1));
	if (!dg)
		return;

	me = get_mesh (ob);
	if (me->dvert) {
		for (i = 0; i < me->totvert; i++) {
			dvert = me->dvert + i;
			if (dvert) {
				if (get_defweight (dvert, (ob->actdef-1)))
					remove_vert_defgroup (ob, dg, i);
			}
		}

		for (i = 0; i < me->totvert; i++) {
			dvert = me->dvert+i;
			if (dvert) {
				for (e = 0; e < dvert->totweight; e++) {
					if (dvert->dw[e].def_nr > (ob->actdef-1))
						dvert->dw[e].def_nr--;
				}
			}
		}
	}

	/* Update the active deform index if necessary */
	if (ob->actdef == BLI_countlist(&ob->defbase))
		ob->actdef--;
  
	/* Remove the group */
	BLI_freelinkN (&ob->defbase, dg);
}

void del_defgroup (Object *ob)
{
	bDeformGroup	*defgroup;
	int				i;

	if (!ob)
		return;

	if (!ob->actdef)
		return;

	defgroup = BLI_findlink(&ob->defbase, ob->actdef-1);
	if (!defgroup)
		return;

	/* Make sure that no verts are using this group */
	remove_verts_defgroup(1);

	/* Make sure that any verts with higher indices are adjusted accordingly */
	if(ob->type==OB_MESH) {
		EditMesh *em = G.editMesh;
		EditVert *eve;
		MDeformVert *dvert;
		
		for (eve=em->verts.first; eve; eve=eve->next){
			dvert= CustomData_em_get(&G.editMesh->vdata, eve->data, CD_MDEFORMVERT);

			if (dvert)
				for (i=0; i<dvert->totweight; i++)
					if (dvert->dw[i].def_nr > (ob->actdef-1))
						dvert->dw[i].def_nr--;
		}
	}
	else {
		BPoint *bp;
		MDeformVert *dvert= editLatt->dvert;
		int a, tot;
		
		if (dvert) {
			tot= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
			for(a=0, bp= editLatt->def; a<tot; a++, bp++, dvert++) {
				for (i=0; i<dvert->totweight; i++){
					if (dvert->dw[i].def_nr > (ob->actdef-1))
						dvert->dw[i].def_nr--;
				}
			}
		}
	}

	/* Update the active deform index if necessary */
	if (ob->actdef==BLI_countlist(&ob->defbase))
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
		else {
			if (editLatt->dvert) {
				MEM_freeN(editLatt->dvert);
				editLatt->dvert= NULL;
			}
		}
	}
}

void create_dverts(ID *id)
{
	/* create deform verts
	 */

	if( GS(id->name)==ID_ME) {
		Mesh *me= (Mesh *)id;
		me->dvert= CustomData_add_layer(&me->vdata, CD_MDEFORMVERT, CD_CALLOC, NULL, me->totvert);
	}
	else if( GS(id->name)==ID_LT) {
		Lattice *lt= (Lattice *)id;
		lt->dvert= MEM_callocN(sizeof(MDeformVert)*lt->pntsu*lt->pntsv*lt->pntsw, "lattice deformVert");
	}
}

/* for mesh in object mode
   lattice can be in editmode */
void remove_vert_def_nr (Object *ob, int def_nr, int vertnum)
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
		if( ((Mesh*)ob->data)->dvert )
			dvert = ((Mesh*)ob->data)->dvert + vertnum;
	}
	else if(ob->type==OB_LATTICE) {
		Lattice *lt= ob->data;
		
		if(ob==G.obedit)
			lt= editLatt;
		
		if(lt->dvert)
			dvert = lt->dvert + vertnum;
	}
	
	if(dvert==NULL)
		return;
	
	/* for all of the deform weights in the
	 * deform vert
	 */
	for (i=dvert->totweight - 1 ; i>=0 ; i--){

		/* if the def_nr is the same as the one
		 * for our weight group then remove it
		 * from this deform vert.
		 */
		if (dvert->dw[i].def_nr == def_nr) {
			dvert->totweight--;
        
			/* if there are still other deform weights
			 * attached to this vert then remove this
			 * deform weight, and reshuffle the others
			 */
			if (dvert->totweight) {
				newdw = MEM_mallocN (sizeof(MDeformWeight)*(dvert->totweight), 
									 "deformWeight");
				if (dvert->dw){
					memcpy (newdw, dvert->dw, sizeof(MDeformWeight)*i);
					memcpy (newdw+i, dvert->dw+i+1, 
							sizeof(MDeformWeight)*(dvert->totweight-i));
					MEM_freeN (dvert->dw);
				}
				dvert->dw=newdw;
			}
			/* if there are no other deform weights
			 * left then just remove the deform weight
			 */
			else {
				MEM_freeN (dvert->dw);
				dvert->dw = NULL;
				break;
			}
		}
	}

}

/* for Mesh in Object mode */
/* allows editmode for Lattice */
void add_vert_defnr (Object *ob, int def_nr, int vertnum, 
                           float weight, int assignmode)
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
		Lattice *lt;
		
		if(ob==G.obedit)
			lt= editLatt;
		else
			lt= ob->data;
			
		if(lt->dvert)
			dv = lt->dvert + vertnum;
	}
	
	if(dv==NULL)
		return;
	
	/* Lets first check to see if this vert is
	 * already in the weight group -- if so
	 * lets update it
	 */
	for (i=0; i<dv->totweight; i++){
		
		/* if this weight cooresponds to the
		 * deform group, then add it using
		 * the assign mode provided
		 */
		if (dv->dw[i].def_nr == def_nr){
			
			switch (assignmode) {
			case WEIGHT_REPLACE:
				dv->dw[i].weight=weight;
				break;
			case WEIGHT_ADD:
				dv->dw[i].weight+=weight;
				if (dv->dw[i].weight >= 1.0)
					dv->dw[i].weight = 1.0;
				break;
			case WEIGHT_SUBTRACT:
				dv->dw[i].weight-=weight;
				/* if the weight is zero or less then
				 * remove the vert from the deform group
				 */
				if (dv->dw[i].weight <= 0.0)
					remove_vert_def_nr(ob, def_nr, vertnum);
				break;
			}
			return;
		}
	}

	/* if the vert wasn't in the deform group then
	 * we must take a different form of action ...
	 */

	switch (assignmode) {
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
		newdw = MEM_callocN (sizeof(MDeformWeight)*(dv->totweight+1), 
							 "deformWeight");
		if (dv->dw){
			memcpy (newdw, dv->dw, sizeof(MDeformWeight)*dv->totweight);
			MEM_freeN (dv->dw);
		}
		dv->dw=newdw;
    
		dv->dw[dv->totweight].weight=weight;
		dv->dw[dv->totweight].def_nr=def_nr;
    
		dv->totweight++;
		break;
	}
}

/* called while not in editmode */
void add_vert_to_defgroup (Object *ob, bDeformGroup *dg, int vertnum, 
                           float weight, int assignmode)
{
	/* add the vert to the deform group with the
	 * specified assign mode
	 */
	int	def_nr;

	/* get the deform group number, exit if
	 * it can't be found
	 */
	def_nr = get_defgroup_num(ob, dg);
	if (def_nr < 0) return;

	/* if there's no deform verts then
	 * create some
	 */
	if(ob->type==OB_MESH) {
		if (!((Mesh*)ob->data)->dvert)
			create_dverts(ob->data);
	}
	else if(ob->type==OB_LATTICE) {
		if (!((Lattice*)ob->data)->dvert)
			create_dverts(ob->data);
	}

	/* call another function to do the work
	 */
	add_vert_defnr (ob, def_nr, vertnum, weight, assignmode);
}

/* Only available in editmode */
void assign_verts_defgroup (void)
{
	extern float editbutvweight;	/* buttons.c */
	Object *ob;
	EditVert *eve;
	bDeformGroup *dg, *eg;
	MDeformWeight *newdw;
	MDeformVert *dvert;
	int	i, done;
	
	if(multires_level1_test()) return;

	ob= G.obedit;

	if (!ob)
		return;

	dg=BLI_findlink(&ob->defbase, ob->actdef-1);
	if (!dg){
		error ("No vertex group is active");
		return;
	}

	switch (ob->type){
	case OB_MESH:
		if (!CustomData_has_layer(&G.editMesh->vdata, CD_MDEFORMVERT))
			EM_add_data_layer(&G.editMesh->vdata, CD_MDEFORMVERT);

		/* Go through the list of editverts and assign them */
		for (eve=G.editMesh->verts.first; eve; eve=eve->next){
			dvert= CustomData_em_get(&G.editMesh->vdata, eve->data, CD_MDEFORMVERT);

			if (dvert && (eve->f & 1)){
				done=0;
				/* See if this vert already has a reference to this group */
				/*		If so: Change its weight */
				done=0;
				for (i=0; i<dvert->totweight; i++){
					eg = BLI_findlink (&ob->defbase, dvert->dw[i].def_nr);
					/* Find the actual group */
					if (eg==dg){
						dvert->dw[i].weight=editbutvweight;
						done=1;
						break;
					}
			 	}
				/*		If not: Add the group and set its weight */
				if (!done){
					newdw = MEM_callocN (sizeof(MDeformWeight)*(dvert->totweight+1), "deformWeight");
					if (dvert->dw){
						memcpy (newdw, dvert->dw, sizeof(MDeformWeight)*dvert->totweight);
						MEM_freeN (dvert->dw);
					}
					dvert->dw=newdw;

					dvert->dw[dvert->totweight].weight= editbutvweight;
					dvert->dw[dvert->totweight].def_nr= ob->actdef-1;

					dvert->totweight++;

				}
			}
		}
		break;
	case OB_LATTICE:
		{
			BPoint *bp;
			int a, tot;
			
			if(editLatt->dvert==NULL)
				create_dverts(&editLatt->id);
			
			tot= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
			for(a=0, bp= editLatt->def; a<tot; a++, bp++) {
				if(bp->f1 & SELECT)
					add_vert_defnr (ob, ob->actdef-1, a, editbutvweight, WEIGHT_REPLACE);
			}
		}	
		break;
	default:
		printf ("Assigning deformation groups to unknown object type\n");
		break;
	}

}

/* mesh object mode, lattice can be in editmode */
void remove_vert_defgroup (Object *ob, bDeformGroup	*dg, int vertnum)
{
	/* This routine removes the vertex from the specified
	 * deform group.
	 */

	int def_nr;

	/* if the object is NULL abort
	 */
	if (!ob)
		return;

	/* get the deform number that cooresponds
	 * to this deform group, and abort if it
	 * can not be found.
	 */
	def_nr = get_defgroup_num(ob, dg);
	if (def_nr < 0) return;

	/* call another routine to do the work
	 */
	remove_vert_def_nr (ob, def_nr, vertnum);
}

/* for mesh in object mode lattice can be in editmode */
static float get_vert_def_nr (Object *ob, int def_nr, int vertnum)
{
	MDeformVert *dvert= NULL;
	int i;

	/* get the deform vertices corresponding to the
	 * vertnum
	 */
	if(ob->type==OB_MESH) {
		if( ((Mesh*)ob->data)->dvert )
			dvert = ((Mesh*)ob->data)->dvert + vertnum;
	}
	else if(ob->type==OB_LATTICE) {
		Lattice *lt= ob->data;
		
		if(ob==G.obedit)
			lt= editLatt;
		
		if(lt->dvert)
			dvert = lt->dvert + vertnum;
	}
	
	if(dvert==NULL)
		return 0.0f;
	
	for(i=dvert->totweight-1 ; i>=0 ; i--)
		if(dvert->dw[i].def_nr == def_nr)
			return dvert->dw[i].weight;

	return 0.0f;
}

/* mesh object mode, lattice can be in editmode */
float get_vert_defgroup (Object *ob, bDeformGroup *dg, int vertnum)
{
	int def_nr;

	if(!ob)
		return 0.0f;

	def_nr = get_defgroup_num(ob, dg);
	if(def_nr < 0) return 0.0f;

	return get_vert_def_nr (ob, def_nr, vertnum);
}

/* Only available in editmode */
/* removes from active defgroup, if allverts==0 only selected vertices */
void remove_verts_defgroup (int allverts)
{
	Object *ob;
	EditVert *eve;
	MDeformVert *dvert;
	MDeformWeight *newdw;
	bDeformGroup *dg, *eg;
	int	i;
	
	if(multires_level1_test()) return;

	ob= G.obedit;

	if (!ob)
		return;

	dg=BLI_findlink(&ob->defbase, ob->actdef-1);
	if (!dg){
		error ("No vertex group is active");
		return;
	}

	switch (ob->type){
	case OB_MESH:
		for (eve=G.editMesh->verts.first; eve; eve=eve->next){
			dvert= CustomData_em_get(&G.editMesh->vdata, eve->data, CD_MDEFORMVERT);
		
			if (dvert && dvert->dw && ((eve->f & 1) || allverts)){
				for (i=0; i<dvert->totweight; i++){
					/* Find group */
					eg = BLI_findlink (&ob->defbase, dvert->dw[i].def_nr);
					if (eg == dg){
						dvert->totweight--;
						if (dvert->totweight){
							newdw = MEM_mallocN (sizeof(MDeformWeight)*(dvert->totweight), "deformWeight");
							
							if (dvert->dw){
								memcpy (newdw, dvert->dw, sizeof(MDeformWeight)*i);
								memcpy (newdw+i, dvert->dw+i+1, sizeof(MDeformWeight)*(dvert->totweight-i));
								MEM_freeN (dvert->dw);
							}
							dvert->dw=newdw;
						}
						else{
							MEM_freeN (dvert->dw);
							dvert->dw=NULL;
							break;
						}
					}
				}
			}
		}
		break;
	case OB_LATTICE:
		
		if(editLatt->dvert) {
			BPoint *bp;
			int a, tot= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
				
			for(a=0, bp= editLatt->def; a<tot; a++, bp++) {
				if(allverts || (bp->f1 & SELECT))
					remove_vert_defgroup (ob, dg, a);
			}
		}
		break;
		
	default:
		printf ("Removing deformation groups from unknown object type\n");
		break;
	}
}

/* Only available in editmode */
/* removes from all defgroup, if allverts==0 only selected vertices */
void remove_verts_defgroups(int allverts)
{
	Object *ob;
	int actdef, defCount;
	
	if (multires_level1_test()) return;

	ob= G.obedit;
	if (ob == NULL) return;
	
	actdef= ob->actdef;
	defCount= BLI_countlist(&ob->defbase);
	
	if (defCount == 0) {
		error("Object has no vertex groups");
		return;
	}
	
	/* To prevent code redundancy, we just use remove_verts_defgroup, but that
	 * only operates on the active vgroup. So we iterate through all groups, by changing
	 * active group index
	 */
	for (ob->actdef= 1; ob->actdef <= defCount; ob->actdef++)
		remove_verts_defgroup(allverts);
		
	ob->actdef= actdef;
}

void vertexgroup_select_by_name(Object *ob, char *name)
{
	bDeformGroup *curdef;
	int actdef= 1;
	
	if(ob==NULL) return;
	
	for (curdef = ob->defbase.first; curdef; curdef=curdef->next, actdef++){
		if (!strcmp(curdef->name, name)) {
			ob->actdef= actdef;
			return;
		}
	}
	ob->actdef=0;	// this signals on painting to create a new one, if a bone in posemode is selected */
}

/* This function provides a shortcut for adding/removing verts from 
 * vertex groups. It is called by the Ctrl-G hotkey in EditMode for Meshes
 * and Lattices. (currently only restricted to those two)
 * It is only responsible for 
 */
void vgroup_assign_with_menu(void)
{
	Object *ob= G.obedit;
	int defCount;
	int mode;
	
	/* prevent crashes */
	if (ob==NULL) return;
	
	defCount= BLI_countlist(&ob->defbase);
	
	/* give user choices of adding to current/new or removing from current */
	if (defCount && ob->actdef)
		mode = pupmenu("Vertex Groups %t|Add Selected to New Group %x1|Add Selected to Active Group %x2|Remove Selected from Active Group %x3|Remove Selected from All Groups %x4");
	else
		mode= pupmenu("Vertex Groups %t|Add Selected to New Group %x1");
	
	/* handle choices */
	switch (mode) {
		case 1: /* add to new group */
			add_defgroup(ob);
			assign_verts_defgroup();
			allqueue(REDRAWVIEW3D, 1);
			BIF_undo_push("Assign to vertex group");
			break;
		case 2: /* add to current group */
			assign_verts_defgroup();
			allqueue(REDRAWVIEW3D, 1);
			BIF_undo_push("Assign to vertex group");
			break;
		case 3:	/* remove from current group */
			remove_verts_defgroup(0);
			allqueue(REDRAWVIEW3D, 1);
			BIF_undo_push("Remove from vertex group");
			break;
		case 4: /* remove from all groups */
			remove_verts_defgroups(0);
			allqueue(REDRAWVIEW3D, 1);
			BIF_undo_push("Remove from all vertex groups");
			break;
	}
}

/* This function provides a shortcut for commonly used vertex group 
 * functions - change weight (not implemented), change active group, delete active group,
 * when Ctrl-Shift-G is used in EditMode, for Meshes and Lattices (only for now).
 */
void vgroup_operation_with_menu(void)
{
	Object *ob= G.obedit;
	int defCount;
	int mode;
	
	/* prevent crashes and useless cases */
	if (ob==NULL) return;
	
	defCount= BLI_countlist(&ob->defbase);
	if (defCount == 0) return;
	
	/* give user choices of adding to current/new or removing from current */
	if (ob->actdef)
		mode = pupmenu("Vertex Groups %t|Change Active Group%x1|Delete Active Group%x2");
	else
		mode= pupmenu("Vertex Groups %t|Change Active Group%x1");
	
	/* handle choices */
	switch (mode) {
		case 1: /* change active group*/
			{
				char *menustr= get_vertexgroup_menustr(ob);
				short nr;
				
				if (menustr) {
					nr= pupmenu(menustr);
					
					if ((nr >= 1) && (nr <= defCount)) 
						ob->actdef= nr;
						
					MEM_freeN(menustr);
				}
				allqueue(REDRAWBUTSALL, 0);
			}
			break;
		case 2: /* delete active group  */
			{
				del_defgroup(ob);
				allqueue (REDRAWVIEW3D, 1);
				allqueue(REDRAWOOPS, 0);
				BIF_undo_push("Delete vertex group");
			}
			break;
	}
}

/* ******************* other deform edit stuff ********** */

void object_apply_deform(Object *ob)
{
	notice("Apply Deformation now only availble in Modifier buttons");
}

