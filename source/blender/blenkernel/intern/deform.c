/*  deform.c   June 2001
 *  
 *  support for deformation groups
 * 
 *	Reevan McKay
 *
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
#include <math.h>
#include "ctype.h"

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"

#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_object.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"
#include "BKE_mesh.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


void copy_defgroups (ListBase *outbase, ListBase *inbase)
{
	bDeformGroup *defgroup, *defgroupn;

	outbase->first= outbase->last= 0;

	for (defgroup = inbase->first; defgroup; defgroup=defgroup->next){
		defgroupn= copy_defgroup(defgroup);
		BLI_addtail(outbase, defgroupn);
	}
}

bDeformGroup *copy_defgroup (bDeformGroup *ingroup)
{
	bDeformGroup *outgroup;

	if (!ingroup)
		return NULL;

	outgroup=MEM_callocN(sizeof(bDeformGroup), "copy deformGroup");
	
	/* For now, just copy everything over. */
	memcpy (outgroup, ingroup, sizeof(bDeformGroup));

	outgroup->next=outgroup->prev=NULL;

	return outgroup;
}

void copy_defvert (MDeformVert *dvert_r, const MDeformVert *dvert)
{
	if(dvert_r->totweight == dvert->totweight) {
		if(dvert->totweight)
			memcpy(dvert_r->dw, dvert->dw, dvert->totweight * sizeof(MDeformWeight));
	}
	else {
		if(dvert_r->dw)
			MEM_freeN(dvert_r->dw);

		if(dvert->totweight)
			dvert_r->dw= MEM_dupallocN(dvert->dw);
		else
			dvert_r->dw= NULL;

		dvert_r->totweight = dvert->totweight;
	}
}

void flip_defvert (MDeformVert *dvert, int *flip_map)
{
	MDeformWeight *dw;
	int i;

	for(dw= dvert->dw, i=0; i<dvert->totweight; dw++, i++)
		if(flip_map[dw->def_nr] >= 0)
			dw->def_nr= flip_map[dw->def_nr];
}


bDeformGroup *get_named_vertexgroup (Object *ob, char *name)
{
	/* return a pointer to the deform group with this name
	 * or return NULL otherwise.
	 */
	bDeformGroup *curdef;

	for (curdef = ob->defbase.first; curdef; curdef=curdef->next) {
		if (!strcmp(curdef->name, name)) {
			return curdef;
		}
	}
	return NULL;
}

int get_named_vertexgroup_num (Object *ob, const char *name)
{
	/* Return the location of the named deform group within the list of
	 * deform groups. This function is a combination of get_defgroup_num and
	 * get_named_vertexgroup. The other two could be called instead, but that
	 * require looping over the vertexgroups twice.
	 */
	bDeformGroup *curdef;
	int def_nr;
	
	for (curdef=ob->defbase.first, def_nr=0; curdef; curdef=curdef->next, def_nr++) {
		if (!strcmp(curdef->name, name))
			return def_nr;
	}
	
	return -1;
}

int get_defgroup_num (Object *ob, bDeformGroup *dg)
{
	/* Fetch the location of this deform group
	 * within the linked list of deform groups.
	 * (this number is stored in the deform
	 * weights of the deform verts to link them
	 * to this deform group).
	 */

	bDeformGroup *eg;
	int def_nr;

	eg = ob->defbase.first;
	def_nr = 0;

	/* loop through all deform groups */
	while (eg != NULL) {

		/* if the current deform group is
		 * the one we are after, return
		 * def_nr
		 */
		if (eg == dg) {
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

/* note, must be freed */
int *get_defgroup_flip_map(Object *ob)
{
	bDeformGroup *dg;
	int totdg= BLI_countlist(&ob->defbase);

	if(totdg==0) {
		return NULL;
	}
	else {
		char name[sizeof(dg->name)];
		int i, flip_num, *map= MEM_mallocN(totdg * sizeof(int), "get_defgroup_flip_map");
		memset(map, -1, totdg * sizeof(int));

		for (dg=ob->defbase.first, i=0; dg; dg=dg->next, i++) {
			if(map[i] == -1) { /* may be calculated previously */
				flip_vertexgroup_name(name, dg->name, 0);
				if(strcmp(name, dg->name)) {
					flip_num= get_named_vertexgroup_num(ob, name);
					if(flip_num > -1) {
						map[i]= flip_num;
						map[flip_num]= i; /* save an extra lookup */
					}
				}
			}
		}
		return map;
	}
}

void unique_vertexgroup_name (bDeformGroup *dg, Object *ob)
{
	bDeformGroup *curdef;
	int number;
	int exists = 0;
	char tempname[64];
	char *dot;
	
	if (!ob)
		return;
		
	/* See if we are given an empty string */
	if (dg->name[0] == '\0') {
		/* give it default name first */
		strcpy (dg->name, "Group");
	}	
		
	/* See if we even need to do this */
	for (curdef = ob->defbase.first; curdef; curdef=curdef->next) {
		if (dg!=curdef) {
			if (!strcmp(curdef->name, dg->name)) {
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
	
	for (number = 1; number <=999; number++) {
		sprintf (tempname, "%s.%03d", dg->name, number);
		
		exists = 0;
		for (curdef=ob->defbase.first; curdef; curdef=curdef->next) {
			if (dg!=curdef) {
				if (!strcmp (curdef->name, tempname)) {
					exists = 1;
					break;
				}
			}
		}
		if (!exists) {
			BLI_strncpy (dg->name, tempname, 32);
			return;
		}
	}	
}


/* finds the best possible flipped name. For renaming; check for unique names afterwards */
/* if strip_number: removes number extensions */
void flip_vertexgroup_name (char *name, const char *from_name, int strip_number)
{
	int     len;
	char    prefix[sizeof((bDeformGroup *)NULL)->name]={""};   /* The part before the facing */
	char    suffix[sizeof((bDeformGroup *)NULL)->name]={""};   /* The part after the facing */
	char    replace[sizeof((bDeformGroup *)NULL)->name]={""};  /* The replacement string */
	char    number[sizeof((bDeformGroup *)NULL)->name]={""};   /* The number extension string */
	char    *index=NULL;

	len= strlen(from_name);
	if(len<3) return; // we don't do names like .R or .L

	strcpy(name, from_name);

	/* We first check the case with a .### extension, let's find the last period */
	if(isdigit(name[len-1])) {
		index= strrchr(name, '.'); // last occurrence
		if (index && isdigit(index[1]) ) { // doesnt handle case bone.1abc2 correct..., whatever!
			if(strip_number==0)
				strcpy(number, index);
			*index= 0;
			len= strlen(name);
		}
	}

	strcpy (prefix, name);

#define IS_SEPARATOR(a) ((a)=='.' || (a)==' ' || (a)=='-' || (a)=='_')

	/* first case; separator . - _ with extensions r R l L  */
	if( IS_SEPARATOR(name[len-2]) ) {
		switch(name[len-1]) {
			case 'l':
				prefix[len-1]= 0;
				strcpy(replace, "r");
				break;
			case 'r':
				prefix[len-1]= 0;
				strcpy(replace, "l");
				break;
			case 'L':
				prefix[len-1]= 0;
				strcpy(replace, "R");
				break;
			case 'R':
				prefix[len-1]= 0;
				strcpy(replace, "L");
				break;
		}
	}
	/* case; beginning with r R l L , with separator after it */
	else if( IS_SEPARATOR(name[1]) ) {
		switch(name[0]) {
			case 'l':
				strcpy(replace, "r");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'r':
				strcpy(replace, "l");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'L':
				strcpy(replace, "R");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'R':
				strcpy(replace, "L");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
		}
	}
	else if(len > 5) {
		/* hrms, why test for a separator? lets do the rule 'ultimate left or right' */
		index = BLI_strcasestr(prefix, "right");
		if (index==prefix || index==prefix+len-5) {
			if(index[0]=='r')
				strcpy (replace, "left");
			else {
				if(index[1]=='I')
					strcpy (replace, "LEFT");
				else
					strcpy (replace, "Left");
			}
			*index= 0;
			strcpy (suffix, index+5);
		}
		else {
			index = BLI_strcasestr(prefix, "left");
			if (index==prefix || index==prefix+len-4) {
				if(index[0]=='l')
					strcpy (replace, "right");
				else {
					if(index[1]=='E')
						strcpy (replace, "RIGHT");
					else
						strcpy (replace, "Right");
				}
				*index= 0;
				strcpy (suffix, index+4);
			}
		}
	}

#undef IS_SEPARATOR

	sprintf (name, "%s%s%s%s", prefix, replace, suffix, number);
}



float deformvert_get_weight(const struct MDeformVert *dvert, int group_num)
{
	if(dvert)
	{
		const MDeformWeight *dw = dvert->dw;
		int i;

		for(i=dvert->totweight; i>0; i--, dw++)
			if(dw->def_nr == group_num)
				return dw->weight;
	}

	/* Not found */
	return 0.0;
}

float vertexgroup_get_vertex_weight(const struct MDeformVert *dvert, int index, int group_num)
{
	if(group_num == -1 || dvert == NULL)
		return 1.0;

	return deformvert_get_weight(dvert+index, group_num);
}

