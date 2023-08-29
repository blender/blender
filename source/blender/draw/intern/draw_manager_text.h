/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
                        int str_len,
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

/* `draw_manager.cc` */

struct DRWTextStore *DRW_text_cache_ensure(void);

#ifdef __cplusplus
}
#endif
