/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_undo_system.hh"

struct Main;
struct MemFileUndoData;
struct bContext;

#define BKE_UNDO_STR_MAX 64

MemFileUndoData *BKE_memfile_undo_encode(Main *bmain, MemFileUndoData *mfu_prev);
bool BKE_memfile_undo_decode(MemFileUndoData *mfu,
                             eUndoStepDir undo_direction,
                             bool use_old_bmain_data,
                             bContext *C);
void BKE_memfile_undo_free(MemFileUndoData *mfu);
