/* SPDX-License-Identifier: GPL-2.0-or-later */

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
