/* SPDX-FileCopyrightText: 2023 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_grease_pencil_types.h"

/** \file
 * \ingroup bke
 * \brief Low-level operations for grease pencil that cannot be defined in the C++ header yet.
 */

#ifdef __cplusplus
extern "C" {
#endif

enum {
  BKE_GREASEPENCIL_BATCH_DIRTY_ALL = 0,
};

extern void (*BKE_grease_pencil_batch_cache_dirty_tag_cb)(GreasePencil *grease_pencil, int mode);
extern void (*BKE_grease_pencil_batch_cache_free_cb)(GreasePencil *grease_pencil);

void BKE_grease_pencil_batch_cache_dirty_tag(GreasePencil *grease_pencil, int mode);
void BKE_grease_pencil_batch_cache_free(GreasePencil *grease_pencil);

#ifdef __cplusplus
}
#endif
