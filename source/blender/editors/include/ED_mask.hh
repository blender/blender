/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "DNA_mask_types.h"

struct ARegion;
struct Depsgraph;
struct KeyframeEditData;
struct MaskLayer;
struct MaskLayerShape;
struct Scene;
struct ScrArea;
struct bContext;
struct wmKeyConfig;

/* `mask_edit.cc` */

/**
 * Returns true when the following conditions are met:
 * - Current space supports mask editing.
 * - The space is configured to interact with mask.
 *
 * It is not required to have mask opened for editing.
 */
bool ED_maskedit_poll(bContext *C);

/**
 * Returns true when the following conditions are met:
 * - Current space supports mask editing.
 * - The space is configured to interact with mask.
 * - Mask has visible and editable splines.
 *
 * It is not required to have mask opened for editing.
 */
bool ED_maskedit_visible_splines_poll(bContext *C);

/**
 * Returns true when the following conditions are met:
 * - Current space supports mask editing.
 * - The space is configured to interact with mask.
 * - The space has mask open for editing.
 */
bool ED_maskedit_mask_poll(bContext *C);

/**
 * Returns true when the following conditions are met:
 * - Current space supports mask editing.
 * - The space is configured to interact with mask.
 * - The space has mask opened.
 * - Mask has visible and editable splines.
 */
bool ED_maskedit_mask_visible_splines_poll(bContext *C);

void ED_mask_deselect_all(const bContext *C);

void ED_operatortypes_mask();
void ED_keymap_mask(wmKeyConfig *keyconf);
void ED_operatormacros_mask();

/* `mask_query.cc` */

void ED_mask_get_size(ScrArea *area, int *r_width, int *r_height);
void ED_mask_zoom(ScrArea *area, ARegion *region, float *r_zoomx, float *r_zoomy);
void ED_mask_get_aspect(ScrArea *area, ARegion *region, float *r_aspx, float *r_aspy);

void ED_mask_pixelspace_factor(ScrArea *area, ARegion *region, float *r_scalex, float *r_scaley);
/**
 * Takes `event->mval`.
 */
void ED_mask_mouse_pos(ScrArea *area, ARegion *region, const int mval[2], float r_co[2]);

/**
 * \param x/y: input, mval space.
 * \param xr/yr: output, mask point space.
 */
void ED_mask_point_pos(ScrArea *area, ARegion *region, float x, float y, float *r_x, float *r_y);
void ED_mask_point_pos__reverse(
    ScrArea *area, ARegion *region, float x, float y, float *r_x, float *r_y);

void ED_mask_cursor_location_get(ScrArea *area, float cursor[2]);
bool ED_mask_selected_minmax(const bContext *C,
                             float min[2],
                             float max[2],
                             bool handles_as_control_point);

void ED_mask_center_from_pivot_ex(
    const bContext *C, ScrArea *area, float r_center[2], char mode, bool *r_has_select);

/* `mask_draw.cc` */

/**
 * Sets up the opengl context.
 * width, height are to match the values from #ED_mask_get_size().
 */
void ED_mask_draw_region(Depsgraph *depsgraph,
                         Mask *mask,
                         ARegion *region,
                         bool show_overlays,
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
                         const bContext *C);

void ED_mask_draw_frames(Mask *mask, ARegion *region, int cfra, int sfra, int efra);

/* `mask_shapekey.cc` */

void ED_mask_layer_shape_auto_key(MaskLayer *mask_layer, int frame);
bool ED_mask_layer_shape_auto_key_all(Mask *mask, int frame);
bool ED_mask_layer_shape_auto_key_select(Mask *mask, int frame);

/* ----------- Mask AnimEdit API ------------------ */

/**
 * Loops over the mask-frames for a mask-layer, and applies the given callback.
 */
bool ED_masklayer_frames_looper(MaskLayer *mask_layer,
                                Scene *scene,
                                bool (*mask_layer_shape_cb)(MaskLayerShape *, Scene *));
/**
 * Make a listing all the mask-frames in a layer as cfraelems.
 */
void ED_masklayer_make_cfra_list(MaskLayer *mask_layer, ListBase *elems, bool onlysel);

/**
 * Check if one of the frames in this layer is selected.
 */
bool ED_masklayer_frame_select_check(const MaskLayer *mask_layer);
/**
 * Set all/none/invert select.
 */
void ED_masklayer_frame_select_set(MaskLayer *mask_layer, short mode);
/**
 * Select the frames in this layer that occur within the bounds specified.
 */
void ED_masklayer_frames_select_box(MaskLayer *mask_layer,
                                    float min,
                                    float max,
                                    short select_mode);
/**
 * Select the frames in this layer that occur within the lasso/circle region specified.
 */
void ED_masklayer_frames_select_region(KeyframeEditData *ked,
                                       MaskLayer *mask_layer,
                                       short tool,
                                       short select_mode);
/**
 * Set all/none/invert select (like above, but with SELECT_* modes).
 */
void ED_mask_select_frames(MaskLayer *mask_layer, short select_mode);
/**
 * Select the frame in this layer that occurs on this frame (there should only be one at most).
 */
void ED_mask_select_frame(MaskLayer *mask_layer, int selx, short select_mode);

/**
 * Delete selected frames.
 */
bool ED_masklayer_frames_delete(MaskLayer *mask_layer);
/**
 * Duplicate selected frames from given mask-layer.
 */
bool ED_masklayer_frames_duplicate(MaskLayer *mask_layer);

/**
 * Snap selected frames to ...
 */
void ED_masklayer_snap_frames(MaskLayer *mask_layer, Scene *scene, short mode);

#if 0
void free_gpcopybuf();
void copy_gpdata();
void paste_gpdata();

void mirror_masklayer_frames(MaskLayer *mask_layer, short mode);
#endif
