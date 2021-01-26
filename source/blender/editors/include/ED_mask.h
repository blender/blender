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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_mask_types.h"

struct Depsgraph;
struct KeyframeEditData;
struct MaskLayer;
struct MaskLayerShape;
struct bContext;
struct wmKeyConfig;

/* mask_edit.c */
void ED_mask_deselect_all(const struct bContext *C);

void ED_operatortypes_mask(void);
void ED_keymap_mask(struct wmKeyConfig *keyconf);
void ED_operatormacros_mask(void);

/* mask_query.c */
void ED_mask_get_size(struct ScrArea *area, int *width, int *height);
void ED_mask_zoom(struct ScrArea *area, struct ARegion *region, float *zoomx, float *zoomy);
void ED_mask_get_aspect(struct ScrArea *area, struct ARegion *region, float *aspx, float *aspy);

void ED_mask_pixelspace_factor(struct ScrArea *area,
                               struct ARegion *region,
                               float *scalex,
                               float *scaley);
void ED_mask_mouse_pos(struct ScrArea *area,
                       struct ARegion *region,
                       const int mval[2],
                       float co[2]);

void ED_mask_point_pos(
    struct ScrArea *area, struct ARegion *region, float x, float y, float *xr, float *yr);
void ED_mask_point_pos__reverse(
    struct ScrArea *area, struct ARegion *region, float x, float y, float *xr, float *yr);

void ED_mask_cursor_location_get(struct ScrArea *area, float cursor[2]);
bool ED_mask_selected_minmax(const struct bContext *C,
                             float min[2],
                             float max[2],
                             bool include_handles);

/* mask_draw.c */
void ED_mask_draw(const struct bContext *C, const char draw_flag, const char draw_type);
void ED_mask_draw_region(struct Depsgraph *depsgraph,
                         struct Mask *mask,
                         struct ARegion *region,
                         const char draw_flag,
                         const char draw_type,
                         const eMaskOverlayMode overlay_mode,
                         const int width_i,
                         const int height_i,
                         const float aspx,
                         const float aspy,
                         const bool do_scale_applied,
                         const bool do_draw_cb,
                         float stabmat[4][4],
                         const struct bContext *C);

void ED_mask_draw_frames(
    struct Mask *mask, struct ARegion *region, const int cfra, const int sfra, const int efra);

/* mask_shapekey.c */
void ED_mask_layer_shape_auto_key(struct MaskLayer *mask_layer, const int frame);
bool ED_mask_layer_shape_auto_key_all(struct Mask *mask, const int frame);
bool ED_mask_layer_shape_auto_key_select(struct Mask *mask, const int frame);

/* ----------- Mask AnimEdit API ------------------ */
bool ED_masklayer_frames_looper(struct MaskLayer *mask_layer,
                                struct Scene *scene,
                                bool (*mask_layer_shape_cb)(struct MaskLayerShape *,
                                                            struct Scene *));
void ED_masklayer_make_cfra_list(struct MaskLayer *mask_layer, ListBase *elems, bool onlysel);

bool ED_masklayer_frame_select_check(struct MaskLayer *mask_layer);
void ED_masklayer_frame_select_set(struct MaskLayer *mask_layer, short mode);
void ED_masklayer_frames_select_box(struct MaskLayer *mask_layer,
                                    float min,
                                    float max,
                                    short select_mode);
void ED_masklayer_frames_select_region(struct KeyframeEditData *ked,
                                       struct MaskLayer *mask_layer,
                                       short tool,
                                       short select_mode);
void ED_mask_select_frames(struct MaskLayer *mask_layer, short select_mode);
void ED_mask_select_frame(struct MaskLayer *mask_layer, int selx, short select_mode);

bool ED_masklayer_frames_delete(struct MaskLayer *mask_layer);
void ED_masklayer_frames_duplicate(struct MaskLayer *mask_layer);

void ED_masklayer_snap_frames(struct MaskLayer *mask_layer, struct Scene *scene, short mode);

#if 0
void free_gpcopybuf(void);
void copy_gpdata(void);
void paste_gpdata(void);

void mirror_masklayer_frames(struct MaskLayer *mask_layer, short mode);
#endif

#ifdef __cplusplus
}
#endif
