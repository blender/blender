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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct DRWTextStore;
struct Object;
struct UnitSettings;
struct View3D;

struct DRWTextStore *DRW_text_cache_create(void);
void DRW_text_cache_destroy(struct DRWTextStore *dt);

void DRW_text_cache_add(struct DRWTextStore *dt,
                        const float co[3],
                        const char *str,
                        const int str_len,
                        short xoffs,
                        short yoffs,
                        short flag,
                        const uchar col[4]);

void DRW_text_cache_draw(struct DRWTextStore *dt, struct ARegion *region, struct View3D *v3d);

void DRW_text_edit_mesh_measure_stats(struct ARegion *region,
                                      struct View3D *v3d,
                                      struct Object *ob,
                                      const struct UnitSettings *unit);

enum {
  // DRW_UNUSED_1 = (1 << 0),  /* dirty */
  DRW_TEXT_CACHE_GLOBALSPACE = (1 << 1),
  DRW_TEXT_CACHE_LOCALCLIP = (1 << 2),
  /* reference the string by pointer */
  DRW_TEXT_CACHE_STRING_PTR = (1 << 3),
};

/* draw_manager.c */
struct DRWTextStore *DRW_text_cache_ensure(void);

#ifdef __cplusplus
}
#endif
