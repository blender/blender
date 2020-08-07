/*
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
 */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Base;
struct BoundBox;
struct Depsgraph;
struct Main;
struct MetaBall;
struct MetaElem;
struct Object;
struct Scene;

struct MetaBall *BKE_mball_add(struct Main *bmain, const char *name);
struct MetaBall *BKE_mball_copy(struct Main *bmain, const struct MetaBall *mb);

bool BKE_mball_is_any_selected(const struct MetaBall *mb);
bool BKE_mball_is_any_selected_multi(struct Base **bases, int bases_len);
bool BKE_mball_is_any_unselected(const struct MetaBall *mb);
bool BKE_mball_is_basis_for(struct Object *ob1, struct Object *ob2);
bool BKE_mball_is_basis(struct Object *ob);
struct Object *BKE_mball_basis_find(struct Scene *scene, struct Object *ob);

void BKE_mball_texspace_calc(struct Object *ob);
struct BoundBox *BKE_mball_boundbox_get(struct Object *ob);
float *BKE_mball_make_orco(struct Object *ob, struct ListBase *dispbase);

void BKE_mball_properties_copy(struct Scene *scene, struct Object *active_object);

bool BKE_mball_minmax_ex(const struct MetaBall *mb,
                         float min[3],
                         float max[3],
                         const float obmat[4][4],
                         const short flag);
bool BKE_mball_minmax(const struct MetaBall *mb, float min[3], float max[3]);
bool BKE_mball_center_median(const struct MetaBall *mb, float r_cent[3]);
bool BKE_mball_center_bounds(const struct MetaBall *mb, float r_cent[3]);
void BKE_mball_transform(struct MetaBall *mb, const float mat[4][4], const bool do_props);
void BKE_mball_translate(struct MetaBall *mb, const float offset[3]);

struct MetaElem *BKE_mball_element_add(struct MetaBall *mb, const int type);

int BKE_mball_select_count(const struct MetaBall *mb);
int BKE_mball_select_count_multi(struct Base **bases, int bases_len);
bool BKE_mball_select_all(struct MetaBall *mb);
bool BKE_mball_select_all_multi_ex(struct Base **bases, int bases_len);
bool BKE_mball_deselect_all(struct MetaBall *mb);
bool BKE_mball_deselect_all_multi_ex(struct Base **bases, int bases_len);
bool BKE_mball_select_swap(struct MetaBall *mb);
bool BKE_mball_select_swap_multi_ex(struct Base **bases, int bases_len);

/* **** Depsgraph evaluation **** */

struct Depsgraph;

/* Draw Cache */

enum {
  BKE_MBALL_BATCH_DIRTY_ALL = 0,
};
void BKE_mball_batch_cache_dirty_tag(struct MetaBall *mb, int mode);
void BKE_mball_batch_cache_free(struct MetaBall *mb);

extern void (*BKE_mball_batch_cache_dirty_tag_cb)(struct MetaBall *mb, int mode);
extern void (*BKE_mball_batch_cache_free_cb)(struct MetaBall *mb);

#ifdef __cplusplus
}
#endif
