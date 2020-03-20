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

#ifndef __BKE_HAIR_H__
#define __BKE_HAIR_H__

/** \file
 * \ingroup bke
 * \brief General operations for hairs.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct BoundBox;
struct Depsgraph;
struct Hair;
struct Main;
struct Object;
struct Scene;

void *BKE_hair_add(struct Main *bmain, const char *name);
struct Hair *BKE_hair_copy(struct Main *bmain, const struct Hair *hair);

struct BoundBox *BKE_hair_boundbox_get(struct Object *ob);

void BKE_hair_update_customdata_pointers(struct Hair *hair);

/* Depsgraph */

struct Hair *BKE_hair_new_for_eval(const struct Hair *hair_src, int totpoint, int totcurve);
struct Hair *BKE_hair_copy_for_eval(struct Hair *hair_src, bool reference);

void BKE_hair_data_update(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *object);

/* Draw Cache */

enum {
  BKE_HAIR_BATCH_DIRTY_ALL = 0,
};

void BKE_hair_batch_cache_dirty_tag(struct Hair *hair, int mode);
void BKE_hair_batch_cache_free(struct Hair *hair);

extern void (*BKE_hair_batch_cache_dirty_tag_cb)(struct Hair *hair, int mode);
extern void (*BKE_hair_batch_cache_free_cb)(struct Hair *hair);

#ifdef __cplusplus
}
#endif

#endif
