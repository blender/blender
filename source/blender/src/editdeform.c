/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * Creator-specific support for vertex deformation groups
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_global.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"

#include "BIF_editdeform.h"
#include "BIF_toolbox.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void sel_verts_defgroup (int select)
{
	EditVert		*eve;
	Object			*ob;
	int				i;

	ob=G.obedit;

	if (!ob)
		return;

	switch (ob->type){
	case OB_MESH:
		for (eve=G.edve.first; eve; eve=eve->next){
			if (eve->totweight){
				for (i=0; i<eve->totweight; i++){
					if (eve->dw[i].def_nr == (ob->actdef-1)){
						if (select) eve->f |= 1;
						else eve->f &= ~1;
						break;
					}
				}
			}
		}
		break;
	default:
		break;
	}

}

MDeformWeight *verify_defweight (MDeformVert *dv, int defgroup)
/* Ensures that mv has a deform weight entry for
the specified defweight group */
{
	int	i;
	MDeformWeight *newdw;

	if (!dv)
		return NULL;

	for (i=0; i<dv->totweight; i++){
		if (dv->dw[i].def_nr == defgroup)
			return dv->dw+i;
	}

	newdw = MEM_callocN (sizeof(MDeformWeight)*(dv->totweight+1), "deformWeight");
	if (dv->dw){
		memcpy (newdw, dv->dw, sizeof(MDeformWeight)*dv->totweight);
		MEM_freeN (dv->dw);
	}
	dv->dw=newdw;
	
	dv->dw[dv->totweight].weight=0;
	dv->dw[dv->totweight].def_nr=defgroup;
	/* Group index */
	
	dv->totweight++;

	return dv->dw+(dv->totweight-1);
}

void add_defgroup (Object *ob) {
	add_defgroup_name (ob, "Group");
}

bDeformGroup *add_defgroup_name (Object *ob, char *name)
{
	bDeformGroup	*defgroup;
	
	if (!ob)
		return NULL;
	
	defgroup = MEM_callocN (sizeof(bDeformGroup), "deformGroup");

	/* I think there should be some length
	 * checking here -- don't know why NaN
	 * never checks name lengths (see
	 * unique_vertexgroup_name, for example).
	 */
	strcpy (defgroup->name, name);

	BLI_addtail(&ob->defbase, defgroup);
	unique_vertexgroup_name(defgroup, ob);

	ob->actdef = BLI_countlist(&ob->defbase);

	return defgroup;
}

void del_defgroup (Object *ob)
{
	bDeformGroup	*defgroup;
	EditVert		*eve;
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
	for (eve=G.edve.first; eve; eve=eve->next){
		for (i=0; i<eve->totweight; i++){
			if (eve->dw[i].def_nr > (ob->actdef-1))
				eve->dw[i].def_nr--;
		}
	}

	/* Update the active material index if necessary */
	if (ob->actdef==BLI_countlist(&ob->defbase))
		ob->actdef--;

	/* Remove the group */
	BLI_freelinkN (&ob->defbase, defgroup);
}

void create_dverts(Mesh *me)
{
	/* create deform verts for the mesh
	 */
	int i;

	me->dvert= MEM_mallocN(sizeof(MDeformVert)*me->totvert, "deformVert");
	for (i=0; i < me->totvert; ++i) {
		me->dvert[i].totweight = 0;
		me->dvert[i].dw        = NULL;
	}
}

int  get_defgroup_num (Object *ob, bDeformGroup	*dg)
{
	/* Fetch the location of this deform group
	 * within the linked list of deform groups.
	 * (this number is stored in the deform
	 * weights of the deform verts to link them
	 * to this deform group) deform deform
	 * deform blah blah deform
	 */

	bDeformGroup	*eg;
	int def_nr;

	eg = ob->defbase.first;
	def_nr = 0;

	/* loop through all deform groups
	 */
	while (eg != NULL){

		/* if the current deform group is
		 * the one we are after, return
		 * def_nr
		 */
		if (eg == dg){
			break;
		}
		++def_nr;
		eg = eg->next;
	}

	/* if there was no deform group found then
	 * return -1 (should set up a nice symbolic
	 * constant for this)
	 */
	if (eg == NULL) return -1;
	
	return def_nr;
    
}


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
	MDeformVert *dvert;
	int i;

	/* if this mesh has no deform mesh abort
	 */
	if (!((Mesh*)ob->data)->dvert) return;

	/* get the deform mesh cooresponding to the
	 * vertnum
	 */
	dvert = ((Mesh*)ob->data)->dvert + vertnum;

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
			}
		}
	}

}

void add_vert_defnr (Object *ob, int def_nr, int vertnum, 
                           float weight, int assignmode)
{
	/* add the vert to the deform group with the
	 * specified number
	 */

	MDeformVert *dv;
	MDeformWeight *newdw;
	int	i;

	/* get the vert
	 */
	dv = ((Mesh*)ob->data)->dvert + vertnum;

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

	/* if this mesh has no deform verts then
	 * create some
	 */
	if (!((Mesh*)ob->data)->dvert) {
		create_dverts((Mesh*)ob->data);
	}

	/* call another function to do the work
	 */
	add_vert_defnr (ob, def_nr, vertnum, weight, assignmode);
}


void assign_verts_defgroup (void)
/* Only available in editmode */
{
	Object *ob;
	EditVert *eve;
	bDeformGroup	*dg, *eg;
	extern float editbutvweight;	/* buttons.c */
	int	i, done;
	MDeformWeight *newdw;

	ob=G.obedit;

	if (!ob)
		return;

	dg=BLI_findlink(&ob->defbase, ob->actdef-1);
	if (!dg){
		error ("No vertex group active");
		return;
	}

	switch (ob->type){
	case OB_MESH:
		/* Go through the list of editverts and assign them */
		for (eve=G.edve.first; eve; eve=eve->next){
			if (eve->f & 1){
				done=0;
				/* See if this vert already has a reference to this group */
				/*		If so: Change its weight */
				done=0;
				for (i=0; i<eve->totweight; i++){
					eg = BLI_findlink (&ob->defbase, eve->dw[i].def_nr);
					/* Find the actual group */
					if (eg==dg){
						eve->dw[i].weight=editbutvweight;
						done=1;
						break;
					}
			 	}
				/*		If not: Add the group and set its weight */
				if (!done){
					newdw = MEM_callocN (sizeof(MDeformWeight)*(eve->totweight+1), "deformWeight");
					if (eve->dw){
						memcpy (newdw, eve->dw, sizeof(MDeformWeight)*eve->totweight);
						MEM_freeN (eve->dw);
					}
					eve->dw=newdw;

					eve->dw[eve->totweight].weight=editbutvweight;
					eve->dw[eve->totweight].def_nr=ob->actdef-1;

					eve->totweight++;

				}
			}
		}
		break;
	default:
		printf ("Assigning deformation groups to unknown object type\n");
		break;
	}

}

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

	/* if this isn't a mesh abort
	 */
	if (ob->type != OB_MESH) return;

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

void remove_verts_defgroup (int allverts)
/* Only available in editmode */
{
	Object *ob;
	EditVert *eve;
	MDeformWeight *newdw;
	bDeformGroup	*dg, *eg;
	int	i;

	ob=G.obedit;

	if (!ob)
		return;

	dg=BLI_findlink(&ob->defbase, ob->actdef-1);
	if (!dg){
		error ("No vertex group active");
		return;
	}

	switch (ob->type){
	case OB_MESH:
		for (eve=G.edve.first; eve; eve=eve->next){
			if (eve->dw && ((eve->f & 1) || allverts)){
				for (i=0; i<eve->totweight; i++){
					/* Find group */
					eg = BLI_findlink (&ob->defbase, eve->dw[i].def_nr);
					if (eg == dg){
						eve->totweight--;
						if (eve->totweight){
							newdw = MEM_mallocN (sizeof(MDeformWeight)*(eve->totweight), "deformWeight");
							
							if (eve->dw){
								memcpy (newdw, eve->dw, sizeof(MDeformWeight)*i);
								memcpy (newdw+i, eve->dw+i+1, sizeof(MDeformWeight)*(eve->totweight-i));
								MEM_freeN (eve->dw);
							}
							eve->dw=newdw;
						}
						else{
							MEM_freeN (eve->dw);
							eve->dw=NULL;
						}
					}
				}
			}
		}
		break;
	default:
		printf ("Removing deformation groups from unknown object type\n");
		break;
	}
}

void verify_defgroups (Object *ob)
{
	/* Ensure the defbase & the dverts match */
	switch (ob->type){
	case OB_MESH:

		/* I'm pretty sure this means "If there are no
		 * deform groups defined, yet there are deform
		 * vertices, then delete the deform vertices
		 */
		if (!ob->defbase.first){
			if (((Mesh*)ob->data)->dvert){
				free_dverts(((Mesh*)ob->data)->dvert, 
							((Mesh*)ob->data)->totvert);
				((Mesh*)ob->data)->dvert=NULL;
			}
		}
		break;
	default:
		break;
	}
}

bDeformGroup *get_named_vertexgroup(Object *ob, char *name)
{
	/* return a pointer to the deform group with this name
	 * or return NULL otherwise.
	 */
	bDeformGroup *curdef;

	for (curdef = ob->defbase.first; curdef; curdef=curdef->next){
		if (!strcmp(curdef->name, name)){
			return curdef;
		}
	}
	return NULL;
}


void unique_vertexgroup_name (bDeformGroup *dg, Object *ob)
{
	char		tempname[64];
	int			number;
	char		*dot;
	int exists = 0;
	bDeformGroup *curdef;
	
	if (!ob)
		return;
	/* See if we even need to do this */
	for (curdef = ob->defbase.first; curdef; curdef=curdef->next){
		if (dg!=curdef){
			if (!strcmp(curdef->name, dg->name)){
				exists = 1;
				break;
			}
		}
	}
	
	if (!exists)
		return;

	/*	Strip off the suffix */
	dot=strchr(dg->name, '.');
	if (dot)
		*dot=0;
	
	for (number = 1; number <=999; number++){
		sprintf (tempname, "%s.%03d", dg->name, number);
		
		exists = 0;
		for (curdef=ob->defbase.first; curdef; curdef=curdef->next){
			if (dg!=curdef){
				if (!strcmp (curdef->name, tempname)){
					exists = 1;
					break;
				}
			}
		}
		if (!exists){
			strcpy (dg->name, tempname);
			return;
		}
	}	
}
