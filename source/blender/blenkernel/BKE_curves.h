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
 */

#pragma once

/** \file
 * \ingroup bke
 * \brief Low-level operations for curves.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BoundBox;
struct CustomDataLayer;
struct Depsgraph;
struct Curves;
struct Main;
struct Object;
struct Scene;

void *BKE_curves_add(struct Main *bmain, const char *name);

struct BoundBox *BKE_curves_boundbox_get(struct Object *ob);

void BKE_curves_update_customdata_pointers(struct Curves *curves);
bool BKE_curves_customdata_required(struct Curves *curves, struct CustomDataLayer *layer);

/* Depsgraph */

struct Curves *BKE_curves_new_for_eval(const struct Curves *curves_src,
                                       int totpoint,
                                       int totcurve);
struct Curves *BKE_curves_copy_for_eval(struct Curves *curves_src, bool reference);

void BKE_curves_data_update(struct Depsgraph *depsgraph,
                            struct Scene *scene,
                            struct Object *object);

/* Draw Cache */

enum {
  BKE_CURVES_BATCH_DIRTY_ALL = 0,
};

void BKE_curves_batch_cache_dirty_tag(struct Curves *curves, int mode);
void BKE_curves_batch_cache_free(struct Curves *curves);

extern void (*BKE_curves_batch_cache_dirty_tag_cb)(struct Curves *curves, int mode);
extern void (*BKE_curves_batch_cache_free_cb)(struct Curves *curves);

#ifdef __cplusplus
}
#endif
