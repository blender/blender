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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Reevan McKay
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/deform.c
 *  \ingroup bke
 */


#include <string.h>
#include <math.h>
#include "ctype.h"

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_deform.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"


void defgroup_copy_list(ListBase *outbase, ListBase *inbase)
{
	bDeformGroup *defgroup, *defgroupn;

	outbase->first= outbase->last= NULL;

	for (defgroup = inbase->first; defgroup; defgroup=defgroup->next) {
		defgroupn= defgroup_duplicate(defgroup);
		BLI_addtail(outbase, defgroupn);
	}
}

bDeformGroup *defgroup_duplicate(bDeformGroup *ingroup)
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
void defvert_copy(MDeformVert *dvert_dst, const MDeformVert *dvert_src)
{
	if (dvert_dst->totweight == dvert_src->totweight) {
		if (dvert_src->totweight)
			memcpy(dvert_dst->dw, dvert_src->dw, dvert_src->totweight * sizeof(MDeformWeight));
	}
	else {
		if (dvert_dst->dw)
			MEM_freeN(dvert_dst->dw);

		if (dvert_src->totweight)
			dvert_dst->dw= MEM_dupallocN(dvert_src->dw);
		else
			dvert_dst->dw= NULL;

		dvert_dst->totweight = dvert_src->totweight;
	}
}

/* copy an index from one dvert to another
 * - do nothing if neither are set.
 * - add destination weight if needed.
 */
void defvert_copy_index(MDeformVert *dvert_dst, const MDeformVert *dvert_src, const int defgroup)
{
	MDeformWeight *dw_src, *dw_dst;

	dw_src= defvert_find_index(dvert_src, defgroup);

	if (dw_src) {
		/* source is valid, verify destination */
		dw_dst= defvert_verify_index(dvert_dst, defgroup);
		dw_dst->weight= dw_src->weight;
	}
	else {
		/* source was NULL, assign zero, could also remove */
		dw_dst= defvert_find_index(dvert_dst, defgroup);

		if (dw_dst) {
			dw_dst->weight= 0.0f;
		}
	}
}

/* only sync over matching weights, don't add or remove groups
 * warning, loop within loop.
 */
void defvert_sync(MDeformVert *dvert_dst, const MDeformVert *dvert_src, int use_verify)
{
	if (dvert_src->totweight && dvert_dst->totweight) {
		int i;
		MDeformWeight *dw_src;
		for (i=0, dw_src=dvert_src->dw; i < dvert_src->totweight; i++, dw_src++) {
			MDeformWeight *dw_dst;
			if (use_verify) dw_dst= defvert_verify_index(dvert_dst, dw_src->def_nr);
			else            dw_dst= defvert_find_index(dvert_dst, dw_src->def_nr);

			if (dw_dst) {
				dw_dst->weight= dw_src->weight;
			}
		}
	}
}

/* be sure all flip_map values are valid */
void defvert_sync_mapped(MDeformVert *dvert_dst, const MDeformVert *dvert_src,
                         const int *flip_map, const int flip_map_len, const int use_verify)
{
	if (dvert_src->totweight && dvert_dst->totweight) {
		int i;
		MDeformWeight *dw_src;
		for (i=0, dw_src=dvert_src->dw; i < dvert_src->totweight; i++, dw_src++) {
			if (dw_src->def_nr < flip_map_len) {
				MDeformWeight *dw_dst;
				if (use_verify) dw_dst= defvert_verify_index(dvert_dst, flip_map[dw_src->def_nr]);
				else            dw_dst= defvert_find_index(dvert_dst, flip_map[dw_src->def_nr]);

				if (dw_dst) {
					dw_dst->weight= dw_src->weight;
				}
			}
		}
	}
}

/* be sure all flip_map values are valid */
void defvert_remap(MDeformVert *dvert, int *map, const int map_len)
{
	MDeformWeight *dw= dvert->dw;
	unsigned int i;
	for (i= dvert->totweight; i != 0; i--, dw++) {
		if (dw->def_nr < map_len) {
			dw->def_nr= map[dw->def_nr];

			/* just in case */
			BLI_assert(dw->def_nr >= 0);
		}
	}
}

void defvert_normalize(MDeformVert *dvert)
{
	if (dvert->totweight <= 0) {
		/* nothing */
	}
	else if (dvert->totweight==1) {
		dvert->dw[0].weight= 1.0f;
	}
	else {
		MDeformWeight *dw;
		unsigned int i;
		float tot_weight= 0.0f;

		for (i= dvert->totweight, dw= dvert->dw; i != 0; i--, dw++) {
			tot_weight += dw->weight;
		}

		if (tot_weight > 0.0f) {
			float scalar= 1.0f / tot_weight;
			for (i= dvert->totweight, dw= dvert->dw; i != 0; i--, dw++) {
				dw->weight *= scalar;

				/* in case of division errors with very low weights */
				CLAMP(dw->weight, 0.0f, 1.0f);
			}
		}
	}
}

void defvert_normalize_lock(MDeformVert *dvert, const int def_nr_lock)
{
	if (dvert->totweight <= 0) {
		/* nothing */
	}
	else if (dvert->totweight==1) {
		dvert->dw[0].weight= 1.0f;
	}
	else {
		MDeformWeight *dw_lock = NULL;
		MDeformWeight *dw;
		unsigned int i;
		float tot_weight= 0.0f;
		float lock_iweight= 1.0f;

		for (i= dvert->totweight, dw= dvert->dw; i != 0; i--, dw++) {
			if(dw->def_nr != def_nr_lock) {
				tot_weight += dw->weight;
			}
			else {
				dw_lock= dw;
				lock_iweight = (1.0f - dw_lock->weight);
				CLAMP(lock_iweight, 0.0f, 1.0f);
			}
		}

		if (tot_weight > 0.0f) {
			/* paranoid, should be 1.0 but in case of float error clamp anyway */

			float scalar= (1.0f / tot_weight) * lock_iweight;
			for (i= dvert->totweight, dw= dvert->dw; i != 0; i--, dw++) {
				if(dw != dw_lock) {
					dw->weight *= scalar;

					/* in case of division errors with very low weights */
					CLAMP(dw->weight, 0.0f, 1.0f);
				}
			}
		}
	}
}

void defvert_flip(MDeformVert *dvert, const int *flip_map, const int flip_map_len)
{
	MDeformWeight *dw;
	int i;

	for (dw= dvert->dw, i=0; i<dvert->totweight; dw++, i++) {
		if (dw->def_nr < flip_map_len) {
			if (flip_map[dw->def_nr] >= 0) {
				dw->def_nr= flip_map[dw->def_nr];
			}
		}
	}
}


bDeformGroup *defgroup_find_name(Object *ob, const char *name)
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

int defgroup_name_index(Object *ob, const char *name)
{
	/* Return the location of the named deform group within the list of
	 * deform groups. This function is a combination of BLI_findlink and
	 * defgroup_find_name. The other two could be called instead, but that
	 * require looping over the vertexgroups twice.
	 */
	bDeformGroup *curdef;
	int def_nr;

	if (name && name[0] != '\0') {
		for (curdef=ob->defbase.first, def_nr=0; curdef; curdef=curdef->next, def_nr++) {
			if (!strcmp(curdef->name, name))
				return def_nr;
		}
	}

	return -1;
}

/* note, must be freed */
int *defgroup_flip_map(Object *ob, int *flip_map_len, int use_default)
{
	int defbase_tot= *flip_map_len= BLI_countlist(&ob->defbase);

	if (defbase_tot==0) {
		return NULL;
	}
	else {
		bDeformGroup *dg;
		char name[sizeof(dg->name)];
		int i, flip_num, *map= MEM_mallocN(defbase_tot * sizeof(int), __func__);

		for (i=0; i < defbase_tot; i++) {
			map[i]= -1;
		}

		for (dg=ob->defbase.first, i=0; dg; dg=dg->next, i++) {
			if (map[i] == -1) { /* may be calculated previously */

				/* in case no valid value is found, use this */
				if (use_default)
					map[i]= i;

				flip_side_name(name, dg->name, FALSE);
				if (strcmp(name, dg->name)) {
					flip_num= defgroup_name_index(ob, name);
					if (flip_num >= 0) {
						map[i]= flip_num;
						map[flip_num]= i; /* save an extra lookup */
					}
				}
			}
		}
		return map;
	}
}

/* note, must be freed */
int *defgroup_flip_map_single(Object *ob, int *flip_map_len, int use_default, int defgroup)
{
	int defbase_tot= *flip_map_len= BLI_countlist(&ob->defbase);

	if (defbase_tot==0) {
		return NULL;
	}
	else {
		bDeformGroup *dg;
		char name[sizeof(dg->name)];
		int i, flip_num, *map= MEM_mallocN(defbase_tot * sizeof(int), __func__);

		for (i=0; i < defbase_tot; i++) {
			if (use_default) map[i]= i;
			else             map[i]= -1;
		}

		dg= BLI_findlink(&ob->defbase, defgroup);

		flip_side_name(name, dg->name, FALSE);
		if (strcmp(name, dg->name)) {
			flip_num= defgroup_name_index(ob, name);

			if (flip_num >= 0) {
				map[defgroup]= flip_num;
				map[flip_num]= defgroup;
			}
		}

		return map;
	}
}

int defgroup_flip_index(Object *ob, int index, int use_default)
{
	bDeformGroup *dg= BLI_findlink(&ob->defbase, index);
	int flip_index = -1;

	if (dg) {
		char name[sizeof(dg->name)];
		flip_side_name(name, dg->name, 0);

		if (strcmp(name, dg->name))
			flip_index= defgroup_name_index(ob, name);
	}

	return (flip_index==-1 && use_default) ? index : flip_index;
}

static int defgroup_find_name_dupe(const char *name, bDeformGroup *dg, Object *ob)
{
	bDeformGroup *curdef;

	for (curdef = ob->defbase.first; curdef; curdef=curdef->next) {
		if (dg!=curdef) {
			if (!strcmp(curdef->name, name)) {
				return 1;
			}
		}
	}

	return 0;
}

static int defgroup_unique_check(void *arg, const char *name)
{
	struct {Object *ob; void *dg;} *data= arg;
	return defgroup_find_name_dupe(name, data->dg, data->ob);
}

void defgroup_unique_name(bDeformGroup *dg, Object *ob)
{
	struct {Object *ob; void *dg;} data;
	data.ob= ob;
	data.dg= dg;

	BLI_uniquename_cb(defgroup_unique_check, &data, "Group", '.', dg->name, sizeof(dg->name));
}

/* finds the best possible flipped name. For renaming; check for unique names afterwards */
/* if strip_number: removes number extensions
 * note: dont use sizeof() for 'name' or 'from_name' */
void flip_side_name(char name[MAX_VGROUP_NAME], const char from_name[MAX_VGROUP_NAME], int strip_number)
{
	int     len;
	char    prefix[MAX_VGROUP_NAME]=  "";   /* The part before the facing */
	char    suffix[MAX_VGROUP_NAME]=  "";   /* The part after the facing */
	char    replace[MAX_VGROUP_NAME]= "";   /* The replacement string */
	char    number[MAX_VGROUP_NAME]=  "";   /* The number extension string */
	char    *index=NULL;

	/* always copy the name, since this can be called with an uninitialized string */
	BLI_strncpy(name, from_name, MAX_VGROUP_NAME);

	len= BLI_strnlen(from_name, MAX_VGROUP_NAME);
	if (len < 3) {
		/* we don't do names like .R or .L */
		return;
	}

	/* We first check the case with a .### extension, let's find the last period */
	if (isdigit(name[len-1])) {
		index= strrchr(name, '.'); // last occurrence
		if (index && isdigit(index[1]) ) { // doesnt handle case bone.1abc2 correct..., whatever!
			if (strip_number==0)
				BLI_strncpy(number, index, sizeof(number));
			*index= 0;
			len= BLI_strnlen(name, MAX_VGROUP_NAME);
		}
	}

	BLI_strncpy(prefix, name, sizeof(prefix));

#define IS_SEPARATOR(a) ((a)=='.' || (a)==' ' || (a)=='-' || (a)=='_')

	/* first case; separator . - _ with extensions r R l L  */
	if (IS_SEPARATOR(name[len-2]) ) {
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
	else if (IS_SEPARATOR(name[1]) ) {
		switch(name[0]) {
			case 'l':
				strcpy(replace, "r");
				BLI_strncpy(suffix, name+1, sizeof(suffix));
				prefix[0]= 0;
				break;
			case 'r':
				strcpy(replace, "l");
				BLI_strncpy(suffix, name+1, sizeof(suffix));
				prefix[0]= 0;
				break;
			case 'L':
				strcpy(replace, "R");
				BLI_strncpy(suffix, name+1, sizeof(suffix));
				prefix[0]= 0;
				break;
			case 'R':
				strcpy(replace, "L");
				BLI_strncpy(suffix, name+1, sizeof(suffix));
				prefix[0]= 0;
				break;
		}
	}
	else if (len > 5) {
		/* hrms, why test for a separator? lets do the rule 'ultimate left or right' */
		index = BLI_strcasestr(prefix, "right");
		if (index==prefix || index==prefix+len-5) {
			if (index[0]=='r')
				strcpy(replace, "left");
			else {
				if (index[1]=='I')
					strcpy(replace, "LEFT");
				else
					strcpy(replace, "Left");
			}
			*index= 0;
			BLI_strncpy(suffix, index+5, sizeof(suffix));
		}
		else {
			index = BLI_strcasestr(prefix, "left");
			if (index==prefix || index==prefix+len-4) {
				if (index[0]=='l')
					strcpy(replace, "right");
				else {
					if (index[1]=='E')
						strcpy(replace, "RIGHT");
					else
						strcpy(replace, "Right");
				}
				*index= 0;
				BLI_strncpy(suffix, index + 4, sizeof(suffix));
			}
		}
	}

#undef IS_SEPARATOR

	BLI_snprintf (name, MAX_VGROUP_NAME, "%s%s%s%s", prefix, replace, suffix, number);
}

float defvert_find_weight(const struct MDeformVert *dvert, const int defgroup)
{
	MDeformWeight *dw= defvert_find_index(dvert, defgroup);
	return dw ? dw->weight : 0.0f;
}

/* take care with this the rationale is:
 * - if the object has no vertex group. act like vertex group isnt set and return 1.0,
 * - if the vertex group exists but the 'defgroup' isnt found on this vertex, _still_ return 0.0
 *
 * This is a bit confusing, just saves some checks from the caller.
 */
float defvert_array_find_weight_safe(const struct MDeformVert *dvert, const int index, const int defgroup)
{
	if (defgroup == -1 || dvert == NULL)
		return 1.0f;

	return defvert_find_weight(dvert+index, defgroup);
}


MDeformWeight *defvert_find_index(const MDeformVert *dvert, const int defgroup)
{
	if (dvert && defgroup >= 0) {
		MDeformWeight *dw = dvert->dw;
		unsigned int i;

		for (i= dvert->totweight; i != 0; i--, dw++) {
			if (dw->def_nr == defgroup) {
				return dw;
			}
		}
	}

	return NULL;
}

/* Ensures that mv has a deform weight entry for the specified defweight group */
/* Note this function is mirrored in editmesh_tools.c, for use for editvertices */
MDeformWeight *defvert_verify_index(MDeformVert *dvert, const int defgroup)
{
	MDeformWeight *dw_new;

	/* do this check always, this function is used to check for it */
	if (!dvert || defgroup < 0)
		return NULL;

	dw_new= defvert_find_index(dvert, defgroup);
	if (dw_new)
		return dw_new;

	dw_new= MEM_callocN(sizeof(MDeformWeight)*(dvert->totweight+1), "deformWeight");
	if (dvert->dw) {
		memcpy(dw_new, dvert->dw, sizeof(MDeformWeight)*dvert->totweight);
		MEM_freeN(dvert->dw);
	}
	dvert->dw= dw_new;
	dw_new += dvert->totweight;
	dw_new->weight= 0.0f;
	dw_new->def_nr= defgroup;
	/* Group index */

	dvert->totweight++;

	return dw_new;
}

/* TODO. merge with code above! */

/* Adds the given vertex to the specified vertex group, with given weight.
 * warning, this does NOT check for existing, assume caller already knows its not there */
void defvert_add_index_notest(MDeformVert *dvert, int defgroup, const float weight)
{
	MDeformWeight *dw_new;

	/* do this check always, this function is used to check for it */
	if (!dvert || defgroup < 0)
		return;

	dw_new = MEM_callocN(sizeof(MDeformWeight)*(dvert->totweight+1), "defvert_add_to group, new deformWeight");
	if(dvert->dw) {
		memcpy(dw_new, dvert->dw, sizeof(MDeformWeight)*dvert->totweight);
		MEM_freeN(dvert->dw);
	}
	dvert->dw = dw_new;
	dw_new += dvert->totweight;
	dw_new->weight = weight;
	dw_new->def_nr = defgroup;
	dvert->totweight++;
}


/* Removes the given vertex from the vertex group.
 * WARNING: This function frees the given MDeformWeight, do not use it afterward! */
void defvert_remove_group(MDeformVert *dvert, MDeformWeight *dw)
{
	if (dvert && dw) {
		MDeformWeight *dw_new;
		int i = dw - dvert->dw;

		/* Security check! */
		if(i < 0 || i >= dvert->totweight) {
			return;
		}

		dvert->totweight--;
		/* If there are still other deform weights attached to this vert then remove
		 * this deform weight, and reshuffle the others.
		 */
		if (dvert->totweight) {
			dw_new = MEM_mallocN(sizeof(MDeformWeight)*(dvert->totweight), __func__);
			if (dvert->dw) {
#if 1			/* since we dont care about order, swap this with the last, save a memcpy */
				if (i != dvert->totweight) {
					dvert->dw[i]= dvert->dw[dvert->totweight];
				}
				memcpy(dw_new, dvert->dw, sizeof(MDeformWeight) * dvert->totweight);
#else
				memcpy(dw_new, dvert->dw, sizeof(MDeformWeight)*i);
				memcpy(dw_new+i, dvert->dw+i+1, sizeof(MDeformWeight)*(dvert->totweight-i));
#endif
				MEM_freeN(dvert->dw);
			}
			dvert->dw = dw_new;
		}
		else {
			/* If there are no other deform weights left then just remove this one. */
			MEM_freeN(dvert->dw);
			dvert->dw = NULL;
		}
	}
}
