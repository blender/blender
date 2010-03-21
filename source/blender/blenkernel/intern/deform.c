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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_deform.h"

#include "BLI_blenlib.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


void defgroup_copy_list (ListBase *outbase, ListBase *inbase)
{
	bDeformGroup *defgroup, *defgroupn;

	outbase->first= outbase->last= 0;

	for (defgroup = inbase->first; defgroup; defgroup=defgroup->next){
		defgroupn= defgroup_duplicate(defgroup);
		BLI_addtail(outbase, defgroupn);
	}
}

bDeformGroup *defgroup_duplicate (bDeformGroup *ingroup)
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

/* copy & overwrite weights */
void defvert_copy (MDeformVert *dvert_r, const MDeformVert *dvert)
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

/* only sync over matching weights, don't add or remove groups
 * warning, loop within loop.
 */
void defvert_sync (MDeformVert *dvert_r, const MDeformVert *dvert, int use_verify)
{
	if(dvert->totweight && dvert_r->totweight) {
		int i;
		MDeformWeight *dw;
		for(i=0, dw=dvert->dw; i < dvert->totweight; i++, dw++) {
			MDeformWeight *dw_r;
			if(use_verify)	dw_r= defvert_find_index(dvert_r, dw->def_nr);
			else			dw_r= defvert_verify_index(dvert_r, dw->def_nr);

			if(dw_r) {
				dw_r->weight= dw->weight;
			}
		}
	}
}

/* be sure all flip_map values are valid */
void defvert_sync_mapped (MDeformVert *dvert_r, const MDeformVert *dvert, int *flip_map, int use_verify)
{
	if(dvert->totweight && dvert_r->totweight) {
		int i;
		MDeformWeight *dw;
		for(i=0, dw=dvert->dw; i < dvert->totweight; i++, dw++) {
			MDeformWeight *dw_r;
			if(use_verify)	dw_r= defvert_find_index(dvert_r, flip_map[dw->def_nr]);
			else			dw_r= defvert_verify_index(dvert_r, flip_map[dw->def_nr]);

			if(dw_r) {
				dw_r->weight= dw->weight;
			}
		}
	}
}

/* be sure all flip_map values are valid */
void defvert_remap (MDeformVert *dvert, int *map)
{
	MDeformWeight *dw;
	int i;
	for(i=0, dw=dvert->dw; i<dvert->totweight; i++, dw++) {
		dw->def_nr= map[dw->def_nr];
	}
}

void defvert_normalize (MDeformVert *dvert)
{
	if(dvert->totweight<=0) {
		/* nothing */
	}
	else if (dvert->totweight==1) {
		dvert->dw[0].weight= 1.0f;
	}
	else {
		int i;
		float tot= 0.0f;
		MDeformWeight *dw;
		for(i=0, dw=dvert->dw; i < dvert->totweight; i++, dw++)
			tot += dw->weight;

		if(tot > 0.0f) {
			for(i=0, dw=dvert->dw; i < dvert->totweight; i++, dw++)
				dw->weight /= tot;
		}
	}
}

void defvert_flip (MDeformVert *dvert, int *flip_map)
{
	MDeformWeight *dw;
	int i;

	for(dw= dvert->dw, i=0; i<dvert->totweight; dw++, i++)
		if(flip_map[dw->def_nr] >= 0)
			dw->def_nr= flip_map[dw->def_nr];
}


bDeformGroup *defgroup_find_name (Object *ob, char *name)
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

int defgroup_name_index (Object *ob, const char *name)
{
	/* Return the location of the named deform group within the list of
	 * deform groups. This function is a combination of defgroup_find_index and
	 * defgroup_find_name. The other two could be called instead, but that
	 * require looping over the vertexgroups twice.
	 */
	bDeformGroup *curdef;
	int def_nr;
	
	if(name[0] != '\0') {
		for (curdef=ob->defbase.first, def_nr=0; curdef; curdef=curdef->next, def_nr++) {
			if (!strcmp(curdef->name, name))
				return def_nr;
		}
	}

	return -1;
}

int defgroup_find_index (Object *ob, bDeformGroup *dg)
{
	/* Fetch the location of this deform group
	 * within the linked list of deform groups.
	 * (this number is stored in the deform
	 * weights of the deform verts to link them
	 * to this deform group).
	 *
	 * note: this is zero based, ob->actdef starts at 1.
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
int *defgroup_flip_map(Object *ob, int use_default)
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

				/* incase no valid value is found, use this */
				if(use_default)
					map[i]= i;

				flip_side_name(name, dg->name, 0);
				if(strcmp(name, dg->name)) {
					flip_num= defgroup_name_index(ob, name);
					if(flip_num >= 0) {
						map[i]= flip_num;
						map[flip_num]= i; /* save an extra lookup */
					}
				}
			}
		}
		return map;
	}
}

int defgroup_flip_index(Object *ob, int index, int use_default)
{
	bDeformGroup *dg= BLI_findlink(&ob->defbase, index);
	int flip_index = -1;

	if(dg) {
		char name[sizeof(dg->name)];
		flip_side_name(name, dg->name, 0);

		if(strcmp(name, dg->name))
			flip_index= defgroup_name_index(ob, name);
	}

	return (flip_index==-1 && use_default) ? index : flip_index;
}

void defgroup_unique_name (bDeformGroup *dg, Object *ob)
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
void flip_side_name (char *name, const char *from_name, int strip_number)
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

float defvert_find_weight(const struct MDeformVert *dvert, int group_num)
{
	MDeformWeight *dw= defvert_find_index(dvert, group_num);
	return dw ? dw->weight : 0.0f;
}

float defvert_array_find_weight_safe(const struct MDeformVert *dvert, int index, int group_num)
{
	if(group_num == -1 || dvert == NULL)
		return 1.0f;

	return defvert_find_weight(dvert+index, group_num);
}


MDeformWeight *defvert_find_index(const MDeformVert *dvert, int defgroup)
{
	if(dvert && defgroup >= 0) {
		MDeformWeight *dw = dvert->dw;
		int i;

		for(i=dvert->totweight; i>0; i--, dw++)
			if(dw->def_nr == defgroup)
				return dw;
	}

	return NULL;
}

/* Ensures that mv has a deform weight entry for the specified defweight group */
/* Note this function is mirrored in editmesh_tools.c, for use for editvertices */
MDeformWeight *defvert_verify_index(MDeformVert *dv, int defgroup)
{
	MDeformWeight *newdw;

	/* do this check always, this function is used to check for it */
	if(!dv || defgroup<0)
		return NULL;

	newdw = defvert_find_index(dv, defgroup);
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
