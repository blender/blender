/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_curves_types.h"

/** \file
 * \ingroup bke
 * \brief Low-level operations for curves that cannot be defined in the C++ header yet.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BoundBox;
struct Curves;
struct Depsgraph;
struct Main;
struct Object;
struct Scene;

void *BKE_curves_add(struct Main *bmain, const char *name);

struct BoundBox *BKE_curves_boundbox_get(struct Object *ob);

bool BKE_curves_attribute_required(const struct Curves *curves, const char *name);

/* Depsgraph */

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
