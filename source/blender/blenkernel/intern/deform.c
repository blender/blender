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
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_customdata.h"
#include "BKE_object_data_transfer.h"
#include "BKE_deform.h"  /* own include */
#include "BKE_mesh_mapping.h"
#include "BKE_object_deform.h"

#include "data_transfer_intern.h"


bDeformGroup *BKE_defgroup_new(Object *ob, const char *name)
{
	bDeformGroup *defgroup;

	BLI_assert(OB_TYPE_SUPPORT_VGROUP(ob->type));

	defgroup = MEM_callocN(sizeof(bDeformGroup), __func__);

	BLI_strncpy(defgroup->name, name, sizeof(defgroup->name));

	BLI_addtail(&ob->defbase, defgroup);
	defgroup_unique_name(defgroup, ob);

	return defgroup;
}

void defgroup_copy_list(ListBase *outbase, ListBase *inbase)
{
	bDeformGroup *defgroup, *defgroupn;

	BLI_listbase_clear(outbase);

	for (defgroup = inbase->first; defgroup; defgroup = defgroup->next) {
		defgroupn = defgroup_duplicate(defgroup);
		BLI_addtail(outbase, defgroupn);
	}
}

bDeformGroup *defgroup_duplicate(bDeformGroup *ingroup)
{
	bDeformGroup *outgroup;

	if (!ingroup)
		return NULL;

	outgroup = MEM_callocN(sizeof(bDeformGroup), "copy deformGroup");

	/* For now, just copy everything over. */
	memcpy(outgroup, ingroup, sizeof(bDeformGroup));

	outgroup->next = outgroup->prev = NULL;

	return outgroup;
}

/* overwrite weights filtered by vgroup_subset
 * - do nothing if neither are set.
 * - add destination weight if needed
 */
void defvert_copy_subset(MDeformVert *dvert_dst, const MDeformVert *dvert_src,
                         const bool *vgroup_subset, const int vgroup_tot)
{
	int defgroup;
	for (defgroup = 0; defgroup < vgroup_tot; defgroup++) {
		if (vgroup_subset[defgroup]) {
			defvert_copy_index(dvert_dst, dvert_src, defgroup);
		}
	}
}

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
			dvert_dst->dw = MEM_dupallocN(dvert_src->dw);
		else
			dvert_dst->dw = NULL;

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

	dw_src = defvert_find_index(dvert_src, defgroup);

	if (dw_src) {
		/* source is valid, verify destination */
		dw_dst = defvert_verify_index(dvert_dst, defgroup);
		dw_dst->weight = dw_src->weight;
	}
	else {
		/* source was NULL, assign zero, could also remove */
		dw_dst = defvert_find_index(dvert_dst, defgroup);

		if (dw_dst) {
			dw_dst->weight = 0.0f;
		}
	}
}

/* only sync over matching weights, don't add or remove groups
 * warning, loop within loop.
 */
void defvert_sync(MDeformVert *dvert_dst, const MDeformVert *dvert_src, const bool use_verify)
{
	if (dvert_src->totweight && dvert_dst->totweight) {
		int i;
		MDeformWeight *dw_src;
		for (i = 0, dw_src = dvert_src->dw; i < dvert_src->totweight; i++, dw_src++) {
			MDeformWeight *dw_dst;
			if (use_verify) dw_dst = defvert_verify_index(dvert_dst, dw_src->def_nr);
			else            dw_dst = defvert_find_index(dvert_dst, dw_src->def_nr);

			if (dw_dst) {
				dw_dst->weight = dw_src->weight;
			}
		}
	}
}

/* be sure all flip_map values are valid */
void defvert_sync_mapped(MDeformVert *dvert_dst, const MDeformVert *dvert_src,
                         const int *flip_map, const int flip_map_len, const bool use_verify)
{
	if (dvert_src->totweight && dvert_dst->totweight) {
		int i;
		MDeformWeight *dw_src;
		for (i = 0, dw_src = dvert_src->dw; i < dvert_src->totweight; i++, dw_src++) {
			if (dw_src->def_nr < flip_map_len) {
				MDeformWeight *dw_dst;
				if (use_verify) dw_dst = defvert_verify_index(dvert_dst, flip_map[dw_src->def_nr]);
				else            dw_dst = defvert_find_index(dvert_dst, flip_map[dw_src->def_nr]);

				if (dw_dst) {
					dw_dst->weight = dw_src->weight;
				}
			}
		}
	}
}

/* be sure all flip_map values are valid */
void defvert_remap(MDeformVert *dvert, int *map, const int map_len)
{
	MDeformWeight *dw = dvert->dw;
	unsigned int i;
	for (i = dvert->totweight; i != 0; i--, dw++) {
		if (dw->def_nr < map_len) {
			dw->def_nr = map[dw->def_nr];

			/* just in case */
			BLI_assert(dw->def_nr >= 0);
		}
	}
}

/**
 * Same as #defvert_normalize but takes a bool array.
 */
void defvert_normalize_subset(MDeformVert *dvert,
                              const bool *vgroup_subset, const int vgroup_tot)
{
	if (dvert->totweight == 0) {
		/* nothing */
	}
	else if (dvert->totweight == 1) {
		MDeformWeight *dw = dvert->dw;
		if ((dw->def_nr < vgroup_tot) && vgroup_subset[dw->def_nr]) {
			dw->weight = 1.0f;
		}
	}
	else {
		MDeformWeight *dw;
		unsigned int i;
		float tot_weight = 0.0f;

		for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
			if ((dw->def_nr < vgroup_tot) && vgroup_subset[dw->def_nr]) {
				tot_weight += dw->weight;
			}
		}

		if (tot_weight > 0.0f) {
			float scalar = 1.0f / tot_weight;
			for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
				if ((dw->def_nr < vgroup_tot) && vgroup_subset[dw->def_nr]) {
					dw->weight *= scalar;

					/* in case of division errors with very low weights */
					CLAMP(dw->weight, 0.0f, 1.0f);
				}
			}
		}
	}
}

void defvert_normalize(MDeformVert *dvert)
{
	if (dvert->totweight == 0) {
		/* nothing */
	}
	else if (dvert->totweight == 1) {
		dvert->dw[0].weight = 1.0f;
	}
	else {
		MDeformWeight *dw;
		unsigned int i;
		float tot_weight = 0.0f;

		for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
			tot_weight += dw->weight;
		}

		if (tot_weight > 0.0f) {
			float scalar = 1.0f / tot_weight;
			for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
				dw->weight *= scalar;

				/* in case of division errors with very low weights */
				CLAMP(dw->weight, 0.0f, 1.0f);
			}
		}
	}
}

/* Same as defvert_normalize() if the locked vgroup is not a member of the subset */
void defvert_normalize_lock_single(MDeformVert *dvert,
                                   const bool *vgroup_subset, const int vgroup_tot,
                                   const int def_nr_lock)
{
	if (dvert->totweight == 0) {
		/* nothing */
	}
	else if (dvert->totweight == 1) {
		MDeformWeight *dw = dvert->dw;
		if ((dw->def_nr < vgroup_tot) && vgroup_subset[dw->def_nr]) {
			if (def_nr_lock != 0) {
				dw->weight = 1.0f;
			}
		}
	}
	else {
		MDeformWeight *dw_lock = NULL;
		MDeformWeight *dw;
		unsigned int i;
		float tot_weight = 0.0f;
		float lock_iweight = 1.0f;

		for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
			if ((dw->def_nr < vgroup_tot) && vgroup_subset[dw->def_nr]) {
				if (dw->def_nr != def_nr_lock) {
					tot_weight += dw->weight;
				}
				else {
					dw_lock = dw;
					lock_iweight = (1.0f - dw_lock->weight);
					CLAMP(lock_iweight, 0.0f, 1.0f);
				}
			}
		}

		if (tot_weight > 0.0f) {
			/* paranoid, should be 1.0 but in case of float error clamp anyway */

			float scalar = (1.0f / tot_weight) * lock_iweight;
			for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
				if ((dw->def_nr < vgroup_tot) && vgroup_subset[dw->def_nr]) {
					if (dw != dw_lock) {
						dw->weight *= scalar;

						/* in case of division errors with very low weights */
						CLAMP(dw->weight, 0.0f, 1.0f);
					}
				}
			}
		}
	}
}

/* Same as defvert_normalize() if no locked vgroup is a member of the subset */
void defvert_normalize_lock_map(
        MDeformVert *dvert,
        const bool *vgroup_subset, const int vgroup_tot,
        const bool *lock_flags, const int defbase_tot)
{
	if (dvert->totweight == 0) {
		/* nothing */
	}
	else if (dvert->totweight == 1) {
		MDeformWeight *dw = dvert->dw;
		if ((dw->def_nr < vgroup_tot) && vgroup_subset[dw->def_nr]) {
			if (LIKELY(defbase_tot >= 1) && lock_flags[0]) {
				dw->weight = 1.0f;
			}
		}
	}
	else {
		MDeformWeight *dw;
		unsigned int i;
		float tot_weight = 0.0f;
		float lock_iweight = 0.0f;

		for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
			if ((dw->def_nr < vgroup_tot) && vgroup_subset[dw->def_nr]) {
				if ((dw->def_nr < defbase_tot) && (lock_flags[dw->def_nr] == false)) {
					tot_weight += dw->weight;
				}
				else {
					/* invert after */
					lock_iweight += dw->weight;
				}
			}
		}

		lock_iweight = max_ff(0.0f, 1.0f - lock_iweight);

		if (tot_weight > 0.0f) {
			/* paranoid, should be 1.0 but in case of float error clamp anyway */

			float scalar = (1.0f / tot_weight) * lock_iweight;
			for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
				if ((dw->def_nr < vgroup_tot) && vgroup_subset[dw->def_nr]) {
					if ((dw->def_nr < defbase_tot) && (lock_flags[dw->def_nr] == false)) {
						dw->weight *= scalar;

						/* in case of division errors with very low weights */
						CLAMP(dw->weight, 0.0f, 1.0f);
					}
				}
			}
		}
	}
}

void defvert_flip(MDeformVert *dvert, const int *flip_map, const int flip_map_len)
{
	MDeformWeight *dw;
	int i;

	for (dw = dvert->dw, i = 0; i < dvert->totweight; dw++, i++) {
		if (dw->def_nr < flip_map_len) {
			if (flip_map[dw->def_nr] >= 0) {
				dw->def_nr = flip_map[dw->def_nr];
			}
		}
	}
}

void defvert_flip_merged(MDeformVert *dvert, const int *flip_map, const int flip_map_len)
{
	MDeformWeight *dw, *dw_cpy;
	float weight;
	int i, totweight = dvert->totweight;

	/* copy weights */
	for (dw = dvert->dw, i = 0; i < totweight; dw++, i++) {
		if (dw->def_nr < flip_map_len) {
			if (flip_map[dw->def_nr] >= 0) {
				/* error checkers complain of this but we'll never get NULL return */
				dw_cpy = defvert_verify_index(dvert, flip_map[dw->def_nr]);
				dw = &dvert->dw[i]; /* in case array got realloced */

				/* distribute weights: if only one of the vertex groups was
				 * assigned this will halve the weights, otherwise it gets
				 * evened out. this keeps it proportional to other groups */
				weight = 0.5f * (dw_cpy->weight + dw->weight);
				dw_cpy->weight = weight;
				dw->weight = weight;
			}
		}
	}
}

bDeformGroup *defgroup_find_name(Object *ob, const char *name)
{
	return BLI_findstring(&ob->defbase, name, offsetof(bDeformGroup, name));
}

int defgroup_name_index(Object *ob, const char *name)
{
	return (name) ? BLI_findstringindex(&ob->defbase, name, offsetof(bDeformGroup, name)) : -1;
}

/* note, must be freed */
int *defgroup_flip_map(Object *ob, int *flip_map_len, const bool use_default)
{
	int defbase_tot = *flip_map_len = BLI_listbase_count(&ob->defbase);

	if (defbase_tot == 0) {
		return NULL;
	}
	else {
		bDeformGroup *dg;
		char name_flip[sizeof(dg->name)];
		int i, flip_num, *map = MEM_mallocN(defbase_tot * sizeof(int), __func__);

		for (i = 0; i < defbase_tot; i++) {
			map[i] = -1;
		}

		for (dg = ob->defbase.first, i = 0; dg; dg = dg->next, i++) {
			if (map[i] == -1) { /* may be calculated previously */

				/* in case no valid value is found, use this */
				if (use_default)
					map[i] = i;

				BKE_deform_flip_side_name(name_flip, dg->name, false);

				if (!STREQ(name_flip, dg->name)) {
					flip_num = defgroup_name_index(ob, name_flip);
					if (flip_num >= 0) {
						map[i] = flip_num;
						map[flip_num] = i; /* save an extra lookup */
					}
				}
			}
		}
		return map;
	}
}

/* note, must be freed */
int *defgroup_flip_map_single(Object *ob, int *flip_map_len, const bool use_default, int defgroup)
{
	int defbase_tot = *flip_map_len = BLI_listbase_count(&ob->defbase);

	if (defbase_tot == 0) {
		return NULL;
	}
	else {
		bDeformGroup *dg;
		char name_flip[sizeof(dg->name)];
		int i, flip_num, *map = MEM_mallocN(defbase_tot * sizeof(int), __func__);

		for (i = 0; i < defbase_tot; i++) {
			map[i] = use_default ? i : -1;
		}

		dg = BLI_findlink(&ob->defbase, defgroup);

		BKE_deform_flip_side_name(name_flip, dg->name, false);
		if (!STREQ(name_flip, dg->name)) {
			flip_num = defgroup_name_index(ob, name_flip);

			if (flip_num != -1) {
				map[defgroup] = flip_num;
				map[flip_num] = defgroup;
			}
		}

		return map;
	}
}

int defgroup_flip_index(Object *ob, int index, const bool use_default)
{
	bDeformGroup *dg = BLI_findlink(&ob->defbase, index);
	int flip_index = -1;

	if (dg) {
		char name_flip[sizeof(dg->name)];
		BKE_deform_flip_side_name(name_flip, dg->name, false);

		if (!STREQ(name_flip, dg->name)) {
			flip_index = defgroup_name_index(ob, name_flip);
		}
	}

	return (flip_index == -1 && use_default) ? index : flip_index;
}

static bool defgroup_find_name_dupe(const char *name, bDeformGroup *dg, Object *ob)
{
	bDeformGroup *curdef;

	for (curdef = ob->defbase.first; curdef; curdef = curdef->next) {
		if (dg != curdef) {
			if (!strcmp(curdef->name, name)) {
				return true;
			}
		}
	}

	return false;
}

static bool defgroup_unique_check(void *arg, const char *name)
{
	struct {Object *ob; void *dg; } *data = arg;
	return defgroup_find_name_dupe(name, data->dg, data->ob);
}

void defgroup_unique_name(bDeformGroup *dg, Object *ob)
{
	struct {Object *ob; void *dg; } data;
	data.ob = ob;
	data.dg = dg;

	BLI_uniquename_cb(defgroup_unique_check, &data, DATA_("Group"), '.', dg->name, sizeof(dg->name));
}

static bool is_char_sep(const char c)
{
	return ELEM(c, '.', ' ', '-', '_');
}

/* based on BLI_split_dirfile() / os.path.splitext(), "a.b.c" -> ("a.b", ".c") */

void BKE_deform_split_suffix(const char string[MAX_VGROUP_NAME], char body[MAX_VGROUP_NAME], char suf[MAX_VGROUP_NAME])
{
	size_t len = BLI_strnlen(string, MAX_VGROUP_NAME);
	size_t i;

	body[0] = suf[0] = '\0';

	for (i = len; i > 0; i--) {
		if (is_char_sep(string[i])) {
			BLI_strncpy(body, string, i + 1);
			BLI_strncpy(suf, string + i,  (len + 1) - i);
			return;
		}
	}

	memcpy(body, string, len + 1);
}

/* "a.b.c" -> ("a.", "b.c") */
void BKE_deform_split_prefix(const char string[MAX_VGROUP_NAME], char pre[MAX_VGROUP_NAME], char body[MAX_VGROUP_NAME])
{
	size_t len = BLI_strnlen(string, MAX_VGROUP_NAME);
	size_t i;

	body[0] = pre[0] = '\0';

	for (i = 1; i < len; i++) {
		if (is_char_sep(string[i])) {
			i++;
			BLI_strncpy(pre, string, i + 1);
			BLI_strncpy(body, string + i, (len + 1) - i);
			return;
		}
	}

	BLI_strncpy(body, string, len);
}

/* finds the best possible flipped name. For renaming; check for unique names afterwards */
/* if strip_number: removes number extensions
 * note: don't use sizeof() for 'name' or 'from_name' */
void BKE_deform_flip_side_name(char name[MAX_VGROUP_NAME], const char from_name[MAX_VGROUP_NAME],
                               const bool strip_number)
{
	int     len;
	char    prefix[MAX_VGROUP_NAME]  = "";   /* The part before the facing */
	char    suffix[MAX_VGROUP_NAME]  = "";   /* The part after the facing */
	char    replace[MAX_VGROUP_NAME] = "";   /* The replacement string */
	char    number[MAX_VGROUP_NAME]  = "";   /* The number extension string */
	char    *index = NULL;
	bool is_set = false;

	/* always copy the name, since this can be called with an uninitialized string */
	BLI_strncpy(name, from_name, MAX_VGROUP_NAME);

	len = BLI_strnlen(from_name, MAX_VGROUP_NAME);
	if (len < 3) {
		/* we don't do names like .R or .L */
		return;
	}

	/* We first check the case with a .### extension, let's find the last period */
	if (isdigit(name[len - 1])) {
		index = strrchr(name, '.'); // last occurrence
		if (index && isdigit(index[1])) { // doesnt handle case bone.1abc2 correct..., whatever!
			if (strip_number == false) {
				BLI_strncpy(number, index, sizeof(number));
			}
			*index = 0;
			len = BLI_strnlen(name, MAX_VGROUP_NAME);
		}
	}

	BLI_strncpy(prefix, name, sizeof(prefix));

	/* first case; separator . - _ with extensions r R l L  */
	if (is_char_sep(name[len - 2])) {
		is_set = true;
		switch (name[len - 1]) {
			case 'l':
				prefix[len - 1] = 0;
				strcpy(replace, "r");
				break;
			case 'r':
				prefix[len - 1] = 0;
				strcpy(replace, "l");
				break;
			case 'L':
				prefix[len - 1] = 0;
				strcpy(replace, "R");
				break;
			case 'R':
				prefix[len - 1] = 0;
				strcpy(replace, "L");
				break;
			default:
				is_set = false;
		}
	}

	/* case; beginning with r R l L, with separator after it */
	if (!is_set && is_char_sep(name[1])) {
		is_set = true;
		switch (name[0]) {
			case 'l':
				strcpy(replace, "r");
				BLI_strncpy(suffix, name + 1, sizeof(suffix));
				prefix[0] = 0;
				break;
			case 'r':
				strcpy(replace, "l");
				BLI_strncpy(suffix, name + 1, sizeof(suffix));
				prefix[0] = 0;
				break;
			case 'L':
				strcpy(replace, "R");
				BLI_strncpy(suffix, name + 1, sizeof(suffix));
				prefix[0] = 0;
				break;
			case 'R':
				strcpy(replace, "L");
				BLI_strncpy(suffix, name + 1, sizeof(suffix));
				prefix[0] = 0;
				break;
			default:
				is_set = false;
		}
	}

	if (!is_set && len > 5) {
		/* hrms, why test for a separator? lets do the rule 'ultimate left or right' */
		if (((index = BLI_strcasestr(prefix, "right")) == prefix) ||
		    (index == prefix + len - 5))
		{
			is_set = true;
			if (index[0] == 'r') {
				strcpy(replace, "left");
			}
			else {
				strcpy(replace, (index[1] == 'I') ? "LEFT" : "Left");
			}
			*index = 0;
			BLI_strncpy(suffix, index + 5, sizeof(suffix));
		}
		else if (((index = BLI_strcasestr(prefix, "left")) == prefix) ||
		         (index == prefix + len - 4))
		{
			is_set = true;
			if (index[0] == 'l') {
				strcpy(replace, "right");
			}
			else {
				strcpy(replace, (index[1] == 'E') ? "RIGHT" : "Right");
			}
			*index = 0;
			BLI_strncpy(suffix, index + 4, sizeof(suffix));
		}
	}

	(void)is_set;  /* quiet warning */

	BLI_snprintf(name, MAX_VGROUP_NAME, "%s%s%s%s", prefix, replace, suffix, number);
}

float defvert_find_weight(const struct MDeformVert *dvert, const int defgroup)
{
	MDeformWeight *dw = defvert_find_index(dvert, defgroup);
	return dw ? dw->weight : 0.0f;
}

/* take care with this the rationale is:
 * - if the object has no vertex group. act like vertex group isn't set and return 1.0,
 * - if the vertex group exists but the 'defgroup' isn't found on this vertex, _still_ return 0.0
 *
 * This is a bit confusing, just saves some checks from the caller.
 */
float defvert_array_find_weight_safe(const struct MDeformVert *dvert, const int index, const int defgroup)
{
	if (defgroup == -1 || dvert == NULL)
		return 1.0f;

	return defvert_find_weight(dvert + index, defgroup);
}


MDeformWeight *defvert_find_index(const MDeformVert *dvert, const int defgroup)
{
	if (dvert && defgroup >= 0) {
		MDeformWeight *dw = dvert->dw;
		unsigned int i;

		for (i = dvert->totweight; i != 0; i--, dw++) {
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

	dw_new = defvert_find_index(dvert, defgroup);
	if (dw_new)
		return dw_new;

	dw_new = MEM_mallocN(sizeof(MDeformWeight) * (dvert->totweight + 1), "deformWeight");
	if (dvert->dw) {
		memcpy(dw_new, dvert->dw, sizeof(MDeformWeight) * dvert->totweight);
		MEM_freeN(dvert->dw);
	}
	dvert->dw = dw_new;
	dw_new += dvert->totweight;
	dw_new->weight = 0.0f;
	dw_new->def_nr = defgroup;
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

	dw_new = MEM_callocN(sizeof(MDeformWeight) * (dvert->totweight + 1), "defvert_add_to group, new deformWeight");
	if (dvert->dw) {
		memcpy(dw_new, dvert->dw, sizeof(MDeformWeight) * dvert->totweight);
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
		if (i < 0 || i >= dvert->totweight) {
			return;
		}

		dvert->totweight--;
		/* If there are still other deform weights attached to this vert then remove
		 * this deform weight, and reshuffle the others.
		 */
		if (dvert->totweight) {
			dw_new = MEM_mallocN(sizeof(MDeformWeight) * (dvert->totweight), __func__);
			if (dvert->dw) {
#if 1           /* since we don't care about order, swap this with the last, save a memcpy */
				if (i != dvert->totweight) {
					dvert->dw[i] = dvert->dw[dvert->totweight];
				}
				memcpy(dw_new, dvert->dw, sizeof(MDeformWeight) * dvert->totweight);
#else
				memcpy(dw_new, dvert->dw, sizeof(MDeformWeight) * i);
				memcpy(dw_new + i, dvert->dw + i + 1, sizeof(MDeformWeight) * (dvert->totweight - i));
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

void defvert_clear(MDeformVert *dvert)
{
	if (dvert->dw) {
		MEM_freeN(dvert->dw);
		dvert->dw = NULL;
	}

	dvert->totweight = 0;
}

/**
 * \return The first group index shared by both deform verts
 * or -1 if none are found.
 */
int defvert_find_shared(const MDeformVert *dvert_a, const MDeformVert *dvert_b)
{
	if (dvert_a->totweight && dvert_b->totweight) {
		MDeformWeight *dw = dvert_a->dw;
		unsigned int i;

		for (i = dvert_a->totweight; i != 0; i--, dw++) {
			if (dw->weight > 0.0f && defvert_find_weight(dvert_b, dw->def_nr) > 0.0f) {
				return dw->def_nr;
			}
		}
	}

	return -1;
}

/**
 * return true if has no weights
 */
bool defvert_is_weight_zero(const struct MDeformVert *dvert, const int defgroup_tot)
{
	MDeformWeight *dw = dvert->dw;
	unsigned int i;
	for (i = dvert->totweight; i != 0; i--, dw++) {
		if (dw->weight != 0.0f) {
			/* check the group is in-range, happens on rare situations */
			if (LIKELY(dw->def_nr < defgroup_tot)) {
				return false;
			}
		}
	}
	return true;
}

/* -------------------------------------------------------------------- */
/* Defvert Array functions */

void BKE_defvert_array_copy(MDeformVert *dst, const MDeformVert *src, int copycount)
{
	/* Assumes dst is already set up */
	int i;

	if (!src || !dst)
		return;

	memcpy(dst, src, copycount * sizeof(MDeformVert));

	for (i = 0; i < copycount; i++) {
		if (src[i].dw) {
			dst[i].dw = MEM_mallocN(sizeof(MDeformWeight) * src[i].totweight, "copy_deformWeight");
			memcpy(dst[i].dw, src[i].dw, sizeof(MDeformWeight) * src[i].totweight);
		}
	}

}

void BKE_defvert_array_free_elems(MDeformVert *dvert, int totvert)
{
	/* Instead of freeing the verts directly,
	 * call this function to delete any special
	 * vert data */
	int i;

	if (!dvert)
		return;

	/* Free any special data from the verts */
	for (i = 0; i < totvert; i++) {
		if (dvert[i].dw) MEM_freeN(dvert[i].dw);
	}
}

void BKE_defvert_array_free(MDeformVert *dvert, int totvert)
{
	/* Instead of freeing the verts directly,
	 * call this function to delete any special
	 * vert data */
	if (!dvert)
		return;

	/* Free any special data from the verts */
	BKE_defvert_array_free_elems(dvert, totvert);

	MEM_freeN(dvert);
}

void BKE_defvert_extract_vgroup_to_vertweights(
        MDeformVert *dvert, const int defgroup, const int num_verts, float *r_weights, const bool invert_vgroup)
{
	if (dvert && defgroup != -1) {
		int i = num_verts;

		while (i--) {
			const float w = defvert_find_weight(&dvert[i], defgroup);
			r_weights[i] = invert_vgroup ? (1.0f - w) : w;
		}
	}
	else {
		fill_vn_fl(r_weights, invert_vgroup ? 1.0f : 0.0f, num_verts);
	}
}

/* The following three make basic interpolation, using temp vert_weights array to avoid looking up same weight
 * several times. */

void BKE_defvert_extract_vgroup_to_edgeweights(
        MDeformVert *dvert, const int defgroup, const int num_verts, MEdge *edges, const int num_edges,
        float *r_weights, const bool invert_vgroup)
{
	if (dvert && defgroup != -1) {
		int i = num_edges;
		float *tmp_weights = MEM_mallocN(sizeof(*tmp_weights) * (size_t)num_verts, __func__);

		BKE_defvert_extract_vgroup_to_vertweights(dvert, defgroup, num_verts, tmp_weights, invert_vgroup);

		while (i--) {
			MEdge *me = &edges[i];

			r_weights[i] = (tmp_weights[me->v1] + tmp_weights[me->v2]) * 0.5f;
		}

		MEM_freeN(tmp_weights);
	}
	else {
		fill_vn_fl(r_weights, 0.0f, num_edges);
	}
}

void BKE_defvert_extract_vgroup_to_loopweights(
        MDeformVert *dvert, const int defgroup, const int num_verts, MLoop *loops, const int num_loops,
        float *r_weights, const bool invert_vgroup)
{
	if (dvert && defgroup != -1) {
		int i = num_loops;
		float *tmp_weights = MEM_mallocN(sizeof(*tmp_weights) * (size_t)num_verts, __func__);

		BKE_defvert_extract_vgroup_to_vertweights(dvert, defgroup, num_verts, tmp_weights, invert_vgroup);

		while (i--) {
			MLoop *ml = &loops[i];

			r_weights[i] = tmp_weights[ml->v];
		}

		MEM_freeN(tmp_weights);
	}
	else {
		fill_vn_fl(r_weights, 0.0f, num_loops);
	}
}

void BKE_defvert_extract_vgroup_to_polyweights(
        MDeformVert *dvert, const int defgroup, const int num_verts, MLoop *loops, const int UNUSED(num_loops),
        MPoly *polys, const int num_polys, float *r_weights, const bool invert_vgroup)
{
	if (dvert && defgroup != -1) {
		int i = num_polys;
		float *tmp_weights = MEM_mallocN(sizeof(*tmp_weights) * (size_t)num_verts, __func__);

		BKE_defvert_extract_vgroup_to_vertweights(dvert, defgroup, num_verts, tmp_weights, invert_vgroup);

		while (i--) {
			MPoly *mp = &polys[i];
			MLoop *ml = &loops[mp->loopstart];
			int j = mp->totloop;
			float w = 0.0f;

			for (; j--; ml++) {
				w += tmp_weights[ml->v];
			}
			r_weights[i] = w / (float)mp->totloop;
		}

		MEM_freeN(tmp_weights);
	}
	else {
		fill_vn_fl(r_weights, 0.0f, num_polys);
	}
}

/*********** Data Transfer **********/

static void vgroups_datatransfer_interp(const CustomDataTransferLayerMap *laymap, void *dest,
                                        void **sources, const float *weights, const int count, const float mix_factor)
{
	MDeformVert **data_src = (MDeformVert **)sources;
	MDeformVert *data_dst = (MDeformVert *)dest;
	const int idx_src = laymap->data_src_n;
	const int idx_dst = laymap->data_dst_n;

	const int mix_mode = laymap->mix_mode;

	int i, j;

	MDeformWeight *dw_src;
	MDeformWeight *dw_dst = defvert_find_index(data_dst, idx_dst);
	float weight_src = 0.0f, weight_dst = 0.0f;

	if (sources) {
		for (i = count; i--;) {
			for (j = data_src[i]->totweight; j--;) {
				if ((dw_src = &data_src[i]->dw[j])->def_nr == idx_src) {
					weight_src += dw_src->weight * weights[i];
					break;
				}
			}
		}
	}

	if (dw_dst) {
		weight_dst = dw_dst->weight;
	}
	else if (mix_mode == CDT_MIX_REPLACE_ABOVE_THRESHOLD) {
		return;  /* Do not affect destination. */
	}

	weight_src = data_transfer_interp_float_do(mix_mode, weight_dst, weight_src, mix_factor);

	CLAMP(weight_src, 0.0f, 1.0f);

	if (!dw_dst) {
		defvert_add_index_notest(data_dst, idx_dst, weight_src);
	}
	else {
		dw_dst->weight = weight_src;
	}
}

static bool data_transfer_layersmapping_vgroups_multisrc_to_dst(
        ListBase *r_map, const int mix_mode, const float mix_factor, const float *mix_weights,
        const int num_elem_dst, const bool use_create, const bool use_delete,
        Object *ob_src, Object *ob_dst, MDeformVert *data_src, MDeformVert *data_dst,
        CustomData *UNUSED(cd_src), CustomData *cd_dst, const bool UNUSED(use_dupref_dst),
        const int tolayers, bool *use_layers_src, const int num_layers_src)
{
	int idx_src;
	int idx_dst;
	int tot_dst = BLI_listbase_count(&ob_dst->defbase);

	const size_t elem_size = sizeof(*((MDeformVert *)NULL));

	switch (tolayers) {
		case DT_LAYERS_INDEX_DST:
			idx_dst = tot_dst;

			/* Find last source actually used! */
			idx_src = num_layers_src;
			while (idx_src-- && !use_layers_src[idx_src]);
			idx_src++;

			if (idx_dst < idx_src) {
				if (!use_create) {
					return false;
				}
				/* Create as much vgroups as necessary! */
				for (; idx_dst < idx_src; idx_dst++) {
					BKE_object_defgroup_add(ob_dst);
				}
			}
			else if (use_delete && idx_dst > idx_src) {
				while (idx_dst-- > idx_src) {
					BKE_object_defgroup_remove(ob_dst, ob_dst->defbase.last);
				}
			}
			if (r_map) {
				/* At this stage, we **need** a valid CD_MDEFORMVERT layer on dest!
				 * Again, use_create is not relevant in this case */
				if (!data_dst) {
					data_dst = CustomData_add_layer(cd_dst, CD_MDEFORMVERT, CD_CALLOC, NULL, num_elem_dst);
				}

				while (idx_src--) {
					if (!use_layers_src[idx_src]) {
						continue;
					}
					data_transfer_layersmapping_add_item(r_map, CD_FAKE_MDEFORMVERT, mix_mode, mix_factor, mix_weights,
					                                     data_src, data_dst, idx_src, idx_src,
					                                     elem_size, 0, 0, 0, vgroups_datatransfer_interp);
				}
			}
			break;
		case DT_LAYERS_NAME_DST:
			{
				bDeformGroup *dg_src, *dg_dst;

				if (use_delete) {
					/* Remove all unused dst vgroups first, simpler in this case. */
					for (dg_dst = ob_dst->defbase.first; dg_dst;) {
						bDeformGroup *dg_dst_next = dg_dst->next;

						if (defgroup_name_index(ob_src, dg_dst->name) == -1) {
							BKE_object_defgroup_remove(ob_dst, dg_dst);
						}
						dg_dst = dg_dst_next;
					}
				}

				for (idx_src = 0, dg_src = ob_src->defbase.first;
				     idx_src < num_layers_src;
				     idx_src++, dg_src = dg_src->next)
				{
					if (!use_layers_src[idx_src]) {
						continue;
					}

					if ((idx_dst = defgroup_name_index(ob_dst, dg_src->name)) == -1) {
						if (!use_create) {
							if (r_map) {
								BLI_freelistN(r_map);
							}
							return false;
						}
						BKE_object_defgroup_add_name(ob_dst, dg_src->name);
						idx_dst = ob_dst->actdef - 1;
					}
					if (r_map) {
						/* At this stage, we **need** a valid CD_MDEFORMVERT layer on dest!
						 * use_create is not relevant in this case */
						if (!data_dst) {
							data_dst = CustomData_add_layer(cd_dst, CD_MDEFORMVERT, CD_CALLOC, NULL, num_elem_dst);
						}

						data_transfer_layersmapping_add_item(
						        r_map, CD_FAKE_MDEFORMVERT, mix_mode, mix_factor, mix_weights,
						        data_src, data_dst, idx_src, idx_dst,
						        elem_size, 0, 0, 0, vgroups_datatransfer_interp);
					}
				}
				break;
			}
		default:
			return false;
	}

	return true;
}

bool data_transfer_layersmapping_vgroups(
        ListBase *r_map, const int mix_mode, const float mix_factor, const float *mix_weights,
        const int num_elem_dst, const bool use_create, const bool use_delete, Object *ob_src, Object *ob_dst,
        CustomData *cd_src, CustomData *cd_dst, const bool use_dupref_dst, const int fromlayers, const int tolayers)
{
	int idx_src, idx_dst;
	MDeformVert *data_src, *data_dst = NULL;

	const size_t elem_size = sizeof(*((MDeformVert *)NULL));

	/* Note: VGroups are a bit hairy, since their layout is defined on object level (ob->defbase), while their actual
	 *       data is a (mesh) CD layer.
	 *       This implies we may have to handle data layout itself while having NULL data itself,
	 *       and even have to support NULL data_src in transfer data code (we always create a data_dst, though).
	 */

	if (BLI_listbase_is_empty(&ob_src->defbase)) {
		if (use_delete) {
			BKE_object_defgroup_remove_all(ob_dst);
		}
		return true;
	}

	data_src = CustomData_get_layer(cd_src, CD_MDEFORMVERT);

	data_dst = CustomData_get_layer(cd_dst, CD_MDEFORMVERT);
	if (data_dst && use_dupref_dst && r_map) {
		/* If dest is a derivedmesh, we do not want to overwrite cdlayers of org mesh! */
		data_dst = CustomData_duplicate_referenced_layer(cd_dst, CD_MDEFORMVERT, num_elem_dst);
	}

	if (fromlayers == DT_LAYERS_ACTIVE_SRC || fromlayers >= 0) {
		/* Note: use_delete has not much meaning in this case, ignored. */

		if (fromlayers >= 0) {
			idx_src = fromlayers;
			BLI_assert(idx_src < BLI_listbase_count(&ob_src->defbase));
		}
		else if ((idx_src = ob_src->actdef - 1) == -1) {
			return false;
		}

		if (tolayers >= 0) {
			/* Note: in this case we assume layer exists! */
			idx_dst = tolayers;
			BLI_assert(idx_dst < BLI_listbase_count(&ob_dst->defbase));
		}
		else if (tolayers == DT_LAYERS_ACTIVE_DST) {
			if ((idx_dst = ob_dst->actdef - 1) == -1) {
				bDeformGroup *dg_src;
				if (!use_create) {
					return true;
				}
				dg_src = BLI_findlink(&ob_src->defbase, idx_src);
				BKE_object_defgroup_add_name(ob_dst, dg_src->name);
				idx_dst = ob_dst->actdef - 1;
			}
		}
		else if (tolayers == DT_LAYERS_INDEX_DST) {
			int num = BLI_listbase_count(&ob_src->defbase);
			idx_dst = idx_src;
			if (num <= idx_dst) {
				if (!use_create) {
					return true;
				}
				/* Create as much vgroups as necessary! */
				for (; num <= idx_dst; num++) {
					BKE_object_defgroup_add(ob_dst);
				}
			}
		}
		else if (tolayers == DT_LAYERS_NAME_DST) {
			bDeformGroup *dg_src = BLI_findlink(&ob_src->defbase, idx_src);
			if ((idx_dst = defgroup_name_index(ob_dst, dg_src->name)) == -1) {
				if (!use_create) {
					return true;
				}
				BKE_object_defgroup_add_name(ob_dst, dg_src->name);
				idx_dst = ob_dst->actdef - 1;
			}
		}
		else {
			return false;
		}

		if (r_map) {
			/* At this stage, we **need** a valid CD_MDEFORMVERT layer on dest!
			 * use_create is not relevant in this case */
			if (!data_dst) {
				data_dst = CustomData_add_layer(cd_dst, CD_MDEFORMVERT, CD_CALLOC, NULL, num_elem_dst);
			}

			data_transfer_layersmapping_add_item(r_map, CD_FAKE_MDEFORMVERT, mix_mode, mix_factor, mix_weights,
			                                     data_src, data_dst, idx_src, idx_dst,
			                                     elem_size, 0, 0, 0, vgroups_datatransfer_interp);
		}
	}
	else {
		int num_src, num_sel_unused;
		bool *use_layers_src = NULL;
		bool ret = false;

		switch (fromlayers) {
			case DT_LAYERS_ALL_SRC:
				use_layers_src = BKE_object_defgroup_subset_from_select_type(ob_src, WT_VGROUP_ALL,
				                                                             &num_src, &num_sel_unused);
				break;
			case DT_LAYERS_VGROUP_SRC_BONE_SELECT:
				use_layers_src = BKE_object_defgroup_subset_from_select_type(ob_src, WT_VGROUP_BONE_SELECT,
				                                                             &num_src, &num_sel_unused);
				break;
			case DT_LAYERS_VGROUP_SRC_BONE_DEFORM:
				use_layers_src = BKE_object_defgroup_subset_from_select_type(ob_src, WT_VGROUP_BONE_DEFORM,
				                                                             &num_src, &num_sel_unused);
				break;
		}

		if (use_layers_src) {
			ret = data_transfer_layersmapping_vgroups_multisrc_to_dst(
			        r_map, mix_mode, mix_factor, mix_weights, num_elem_dst, use_create, use_delete,
			        ob_src, ob_dst, data_src, data_dst, cd_src, cd_dst, use_dupref_dst,
			        tolayers, use_layers_src, num_src);
		}

		MEM_SAFE_FREE(use_layers_src);
		return ret;
	}

	return true;
}
