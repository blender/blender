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

bool BKE_mball_is_any_selected(const struct MetaBall *mb);
bool BKE_mball_is_any_selected_multi(struct Base **bases, int bases_len);
bool BKE_mball_is_any_unselected(const struct MetaBall *mb);
bool BKE_mball_is_basis_for(struct Object *ob1, struct Object *ob2);
/**
 * Test, if \a ob is a basis meta-ball.
 *
 * It test last character of Object ID name.
 * If last character is digit it return 0, else it return 1.
 */
bool BKE_mball_is_basis(struct Object *ob);
/**
 * This function finds the basis meta-ball.
 *
 * Basis meta-ball doesn't include any number at the end of
 * its name. All meta-balls with same base of name can be
 * blended. meta-balls with different basic name can't be blended.
 *
 * \warning #BKE_mball_is_basis() can fail on returned object, see function docs for details.
 */
struct Object *BKE_mball_basis_find(struct Scene *scene, struct Object *ob);

/**
 * Compute bounding box of all meta-elements / meta-ball.
 *
 * Bounding box is computed from polygonized surface. \a ob is
 * basic meta-balls (with name `Meta` for example). All other meta-ball objects
 * (with names `Meta.001`, `Meta.002`, etc) are included in this bounding-box.
 */
void BKE_mball_texspace_calc(struct Object *ob);
/**
 * Return or compute bounding-box for given meta-ball object.
 */
struct BoundBox *BKE_mball_boundbox_get(struct Object *ob);
float *BKE_mball_make_orco(struct Object *ob, struct ListBase *dispbase);

/**
 * Copy some properties from object to other meta-ball object with same base name.
 *
 * When some properties (wire-size, threshold, update flags) of meta-ball are changed, then this
 * properties are copied to all meta-balls in same "group" (meta-balls with same base name:
 * `MBall`, `MBall.001`, `MBall.002`, etc). The most important is to copy properties to the base
 * meta-ball, because this meta-ball influence polygonization of meta-balls. */
void BKE_mball_properties_copy(struct Scene *scene, struct Object *active_object);

bool BKE_mball_minmax_ex(
    const struct MetaBall *mb, float min[3], float max[3], const float obmat[4][4], short flag);

/* Basic vertex data functions. */

bool BKE_mball_minmax(const struct MetaBall *mb, float min[3], float max[3]);
bool BKE_mball_center_median(const struct MetaBall *mb, float r_cent[3]);
bool BKE_mball_center_bounds(const struct MetaBall *mb, float r_cent[3]);
void BKE_mball_transform(struct MetaBall *mb, const float mat[4][4], bool do_props);
void BKE_mball_translate(struct MetaBall *mb, const float offset[3]);

/**
 * Most simple meta-element adding function.
 *
 * \note don't do context manipulation here (rna uses).
 */
struct MetaElem *BKE_mball_element_add(struct MetaBall *mb, int type);

/* *** select funcs *** */

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
