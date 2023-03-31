/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation */

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

/* Returns true when the following conditions are met:
 * - Current space supports mask editing.
 * - The space is configured to interact with mask.
 *
 * It is not required to have mask opened for editing. */
bool ED_maskedit_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space supports mask editing.
 * - The space is configured to interact with mask.
 * - Mask has visible and editable splines.
 *
 * It is not required to have mask opened for editing. */
bool ED_maskedit_visible_splines_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space supports mask editing.
 * - The space is configured to interact with mask.
 * - The space has mask open for editing. */
bool ED_maskedit_mask_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space supports mask editing.
 * - The space is configured to interact with mask.
 * - The space has mask opened.
 * - Mask has visible and editable splines. */
bool ED_maskedit_mask_visible_splines_poll(struct bContext *C);

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
/**
 * Takes `event->mval`.
 */
void ED_mask_mouse_pos(struct ScrArea *area,
                       struct ARegion *region,
                       const int mval[2],
                       float co[2]);

/**
 * \param x/y: input, mval space.
 * \param xr/yr: output, mask point space.
 */
void ED_mask_point_pos(
    struct ScrArea *area, struct ARegion *region, float x, float y, float *xr, float *yr);
void ED_mask_point_pos__reverse(
    struct ScrArea *area, struct ARegion *region, float x, float y, float *xr, float *yr);

void ED_mask_cursor_location_get(struct ScrArea *area, float cursor[2]);
bool ED_mask_selected_minmax(const struct bContext *C,
                             float min[2],
                             float max[2],
                             bool handles_as_control_point);

/* mask_draw.c */

/**
 * Sets up the opengl context.
 * width, height are to match the values from #ED_mask_get_size().
 */
void ED_mask_draw_region(struct Depsgraph *depsgraph,
                         struct Mask *mask,
                         struct ARegion *region,
                         char draw_flag,
                         char draw_type,
                         eMaskOverlayMode overlay_mode,
                         float blend_factor,
                         int width_i,
                         int height_i,
                         float aspx,
                         float aspy,
                         bool do_scale_applied,
                         bool do_draw_cb,
                         float stabmat[4][4],
                         const struct bContext *C);

void ED_mask_draw_frames(struct Mask *mask, struct ARegion *region, int cfra, int sfra, int efra);

/* mask_shapekey.c */

void ED_mask_layer_shape_auto_key(struct MaskLayer *mask_layer, int frame);
bool ED_mask_layer_shape_auto_key_all(struct Mask *mask, int frame);
bool ED_mask_layer_shape_auto_key_select(struct Mask *mask, int frame);

/* ----------- Mask AnimEdit API ------------------ */

/**
 * Loops over the mask-frames for a mask-layer, and applies the given callback.
 */
bool ED_masklayer_frames_looper(struct MaskLayer *mask_layer,
                                struct Scene *scene,
                                bool (*mask_layer_shape_cb)(struct MaskLayerShape *,
                                                            struct Scene *));
/**
 * Make a listing all the mask-frames in a layer as cfraelems.
 */
void ED_masklayer_make_cfra_list(struct MaskLayer *mask_layer, ListBase *elems, bool onlysel);

/**
 * Check if one of the frames in this layer is selected.
 */
bool ED_masklayer_frame_select_check(const struct MaskLayer *mask_layer);
/**
 * Set all/none/invert select.
 */
void ED_masklayer_frame_select_set(struct MaskLayer *mask_layer, short mode);
/**
 * Select the frames in this layer that occur within the bounds specified.
 */
void ED_masklayer_frames_select_box(struct MaskLayer *mask_layer,
                                    float min,
                                    float max,
                                    short select_mode);
/**
 * Select the frames in this layer that occur within the lasso/circle region specified.
 */
void ED_masklayer_frames_select_region(struct KeyframeEditData *ked,
                                       struct MaskLayer *mask_layer,
                                       short tool,
                                       short select_mode);
/**
 * Set all/none/invert select (like above, but with SELECT_* modes).
 */
void ED_mask_select_frames(struct MaskLayer *mask_layer, short select_mode);
/**
 * Select the frame in this layer that occurs on this frame (there should only be one at most).
 */
void ED_mask_select_frame(struct MaskLayer *mask_layer, int selx, short select_mode);

/**
 * Delete selected frames.
 */
bool ED_masklayer_frames_delete(struct MaskLayer *mask_layer);
/**
 * Duplicate selected frames from given mask-layer.
 */
void ED_masklayer_frames_duplicate(struct MaskLayer *mask_layer);

/**
 * Snap selected frames to ...
 */
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
