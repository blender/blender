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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct bContext;

/* ed_util.c */
void ED_editors_init_for_undo(struct Main *bmain);
void ED_editors_init(struct bContext *C);
void ED_editors_exit(struct Main *bmain, bool do_undo_system);

bool ED_editors_flush_edits_for_object_ex(struct Main *bmain,
                                          struct Object *ob,
                                          bool for_render,
                                          bool check_needs_flush);
bool ED_editors_flush_edits_for_object(struct Main *bmain, struct Object *ob);

bool ED_editors_flush_edits_ex(struct Main *bmain, bool for_render, bool check_needs_flush);
bool ED_editors_flush_edits(struct Main *bmain);

void ED_spacedata_id_remap(struct ScrArea *area,
                           struct SpaceLink *sl,
                           struct ID *old_id,
                           struct ID *new_id);

void ED_operatortypes_edutils(void);

/* Drawing */
void ED_region_draw_mouse_line_cb(const struct bContext *C,
                                  struct ARegion *region,
                                  void *arg_info);

void ED_region_image_metadata_draw(
    int x, int y, struct ImBuf *ibuf, const rctf *frame, float zoomx, float zoomy);

/* ************** XXX OLD CRUFT WARNING ************* */

void apply_keyb_grid(
    int shift, int ctrl, float *val, float fac1, float fac2, float fac3, int invert);

/* where else to go ? */
void unpack_menu(struct bContext *C,
                 const char *opname,
                 const char *id_name,
                 const char *abs_name,
                 const char *folder,
                 struct PackedFile *pf);

#ifdef __cplusplus
}
#endif
