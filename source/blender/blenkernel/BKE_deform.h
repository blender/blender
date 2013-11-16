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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_DEFORM_H__
#define __BKE_DEFORM_H__

/** \file BKE_deform.h
 *  \ingroup bke
 *  \since June 2001
 *  \author Reevan McKay et al
 *  \brief support for deformation groups and hooks.
 */

struct Object;
struct ListBase;
struct bDeformGroup;
struct MDeformVert;

struct bDeformGroup *BKE_defgroup_new(struct Object *ob, const char *name);
void                 defgroup_copy_list(struct ListBase *lb1, struct ListBase *lb2);
struct bDeformGroup *defgroup_duplicate(struct bDeformGroup *ingroup);
struct bDeformGroup *defgroup_find_name(struct Object *ob, const char *name);
int                 *defgroup_flip_map(struct Object *ob, int *flip_map_len, const bool use_default);
int                 *defgroup_flip_map_single(struct Object *ob, int *flip_map_len, const bool use_default, int defgroup);
int                  defgroup_flip_index(struct Object *ob, int index, const bool use_default);
int                  defgroup_name_index(struct Object *ob, const char *name);
void                 defgroup_unique_name(struct bDeformGroup *dg, struct Object *ob);

struct MDeformWeight    *defvert_find_index(const struct MDeformVert *dv, const int defgroup);
struct MDeformWeight    *defvert_verify_index(struct MDeformVert *dv, const int defgroup);
void                     defvert_add_index_notest(struct MDeformVert *dv, int defgroup, const float weight);
void                     defvert_remove_group(struct MDeformVert *dvert, struct MDeformWeight *dw);
void                     defvert_clear(struct MDeformVert *dvert);
int                      defvert_find_shared(const struct MDeformVert *dvert_a, const struct MDeformVert *dvert_b);
bool                     defvert_is_weight_zero(const struct MDeformVert *dvert, const int defgroup_tot);

void BKE_defvert_array_free_elems(struct MDeformVert *dvert, int totvert);
void BKE_defvert_array_free(struct MDeformVert *dvert, int totvert);
void BKE_defvert_array_copy(struct MDeformVert *dst, const struct MDeformVert *src, int totvert);

float  defvert_find_weight(const struct MDeformVert *dvert, const int defgroup);
float  defvert_array_find_weight_safe(const struct MDeformVert *dvert, const int index, const int defgroup);

void defvert_copy(struct MDeformVert *dvert_dst, const struct MDeformVert *dvert_src);
void defvert_copy_subset(struct MDeformVert *dvert_dst, const struct MDeformVert *dvert_src,
                         const bool *vgroup_subset, const int vgroup_tot);
void defvert_copy_index(struct MDeformVert *dvert_dst, const struct MDeformVert *dvert_src, const int defgroup);
void defvert_sync(struct MDeformVert *dvert_dst, const struct MDeformVert *dvert_src, const bool use_verify);
void defvert_sync_mapped(struct MDeformVert *dvert_dst, const struct MDeformVert *dvert_src,
                         const int *flip_map, const int flip_map_len, const bool use_verify);
void defvert_remap(struct MDeformVert *dvert, int *map, const int map_len);
void defvert_flip(struct MDeformVert *dvert, const int *flip_map, const int flip_map_len);
void defvert_flip_merged(struct MDeformVert *dvert, const int *flip_map, const int flip_map_len);
void defvert_normalize(struct MDeformVert *dvert);
void defvert_normalize_subset(struct MDeformVert *dvert,
                              const bool *vgroup_subset, const int vgroup_tot);
void defvert_normalize_lock_single(struct MDeformVert *dvert,
                                   const bool *vgroup_subset, const int vgroup_tot,
                                   const int def_nr_lock);
void defvert_normalize_lock_map(struct MDeformVert *dvert,
                                const bool *vgroup_subset, const int vgroup_tot,
                                const bool *lock_flags, const int defbase_tot);

/* utility function, note that MAX_VGROUP_NAME chars is the maximum string length since its only
 * used with defgroups currently */

void BKE_deform_split_suffix(const char string[MAX_VGROUP_NAME], char base[MAX_VGROUP_NAME], char ext[MAX_VGROUP_NAME]);
void BKE_deform_split_prefix(const char string[MAX_VGROUP_NAME], char base[MAX_VGROUP_NAME], char ext[MAX_VGROUP_NAME]);

void BKE_deform_flip_side_name(char name[MAX_VGROUP_NAME], const char from_name[MAX_VGROUP_NAME],
                               const bool strip_number);

#endif  /* __BKE_DEFORM_H__ */
