/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_undo_system.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct MemFileUndoData;
struct bContext;

#define BKE_UNDO_STR_MAX 64

struct MemFileUndoData *BKE_memfile_undo_encode(struct Main *bmain,
                                                struct MemFileUndoData *mfu_prev);
bool BKE_memfile_undo_decode(struct MemFileUndoData *mfu,
                             enum eUndoStepDir undo_direction,
                             bool use_old_bmain_data,
                             struct bContext *C);
void BKE_memfile_undo_free(struct MemFileUndoData *mfu);

#ifdef __cplusplus
}
#endif
