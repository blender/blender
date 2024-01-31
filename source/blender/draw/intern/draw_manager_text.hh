/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_sys_types.h"

#pragma once

struct ARegion;
struct DRWTextStore;
struct Object;
struct UnitSettings;
struct View3D;

DRWTextStore *DRW_text_cache_create();
void DRW_text_cache_destroy(DRWTextStore *dt);

void DRW_text_cache_add(DRWTextStore *dt,
                        const float co[3],
                        const char *str,
                        int str_len,
                        short xoffs,
                        short yoffs,
                        short flag,
                        const uchar col[4],
                        const bool shadow = false,
                        const bool align_center = false);

void DRW_text_cache_draw(DRWTextStore *dt, ARegion *region, View3D *v3d);

void DRW_text_edit_mesh_measure_stats(ARegion *region,
                                      View3D *v3d,
                                      Object *ob,
                                      const UnitSettings *unit);

enum {
  // DRW_UNUSED_1 = (1 << 0),  /* dirty */
  DRW_TEXT_CACHE_GLOBALSPACE = (1 << 1),
  DRW_TEXT_CACHE_LOCALCLIP = (1 << 2),
  /* reference the string by pointer */
  DRW_TEXT_CACHE_STRING_PTR = (1 << 3),
};

/* `draw_manager.cc` */

DRWTextStore *DRW_text_cache_ensure();
