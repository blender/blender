/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "WM_types.hh"

struct Main;
struct bContext;
class WorkspaceStatus;

namespace blender::bke::id {
class IDRemapper;
}

/* `ed_util.cc` */

void ED_editors_init_for_undo(Main *bmain);
void ED_editors_init(bContext *C);
/**
 * Frees all edit-mode stuff.
 */
void ED_editors_exit(Main *bmain, bool do_undo_system);

bool ED_editors_flush_edits_for_object_ex(Main *bmain,
                                          Object *ob,
                                          bool for_render,
                                          bool check_needs_flush);
bool ED_editors_flush_edits_for_object(Main *bmain, Object *ob);

/**
 * Flush any temp data from object editing to DNA before writing files, rendering, copying, etc.
 */
bool ED_editors_flush_edits_ex(Main *bmain, bool for_render, bool check_needs_flush);
bool ED_editors_flush_edits(Main *bmain);

/**
 * Use to free ID references within runtime data (stored outside of DNA)
 *
 * \param new_id: may be NULL to unlink \a old_id.
 */
void ED_spacedata_id_remap_single(ScrArea *area, SpaceLink *sl, ID *old_id, ID *new_id);
void ED_spacedata_id_remap(ScrArea *area,
                           SpaceLink *sl,
                           const blender::bke::id::IDRemapper &mappings);

/**
 * Helper for context sensitive operations: Returns the "id" context member wrapped in a
 * #PointerRNA vector. Useful when the API uses vectors to also support acting on multiple IDs,
 * e.g. as returned by #ED_operator_get_ids_from_context_as_vec().
 */
blender::Vector<PointerRNA> ED_operator_single_id_from_context_as_vec(const bContext *C);
/**
 * Helper for context sensitive operations: Returns the "selected_ids" context member or, if none,
 * the "id" context member as a #PointerRNA vector. Batch operations can use this to get all IDs to
 * act on, including a fallback to the active ID if there's no selection.
 */
blender::Vector<PointerRNA> ED_operator_get_ids_from_context_as_vec(const bContext *C);

void ED_operatortypes_edutils();

/* Drawing */

/**
 * Callback that draws a line between the mouse and a position given as the initial argument.
 */
void ED_region_draw_mouse_line_cb(const bContext *C, ARegion *region, void *arg_info);

/**
 * \note Keep in sync with #BKE_image_stamp_buf.
 */
void ED_region_image_metadata_draw(
    int x, int y, const ImBuf *ibuf, const rctf *frame, float zoomx, float zoomy);

void ED_region_image_overlay_info_text_draw(const int render_size_x,
                                            const int render_size_y,

                                            const int viewer_size_x,
                                            const int viewer_size_y,

                                            const int draw_offset_x,
                                            const int draw_offset_y);

void ED_region_image_render_region_draw(
    int x, int y, const rcti *frame, float zoomx, float zoomy, float passepartout_alpha);

/* Slider */

struct tSlider;
enum SliderMode { SLIDER_MODE_PERCENT = 0, SLIDER_MODE_FLOAT = 1 };

tSlider *ED_slider_create(bContext *C);
/**
 * For modal operations so the percentage doesn't pop on the first mouse movement.
 */
void ED_slider_init(tSlider *slider, const wmEvent *event);
/**
 * Calculate slider factor based on mouse position.
 */
bool ED_slider_modal(tSlider *slider, const wmEvent *event);
void ED_slider_destroy(bContext *C, tSlider *slider);

/**
 * Return string based on the current state of the slider.
 */
void ED_slider_status_string_get(const tSlider *slider,
                                 char *status_string,
                                 size_t size_of_status_string);

void ED_slider_status_get(const tSlider *slider, WorkspaceStatus &status);

float ED_slider_factor_get(const tSlider *slider);
void ED_slider_factor_set(tSlider *slider, float factor);

/**
 * By default the increment step is 0.1, which depending on the factor bounds might not be desired.
 * Only has an effect if increment is allowed and enabled.
 * See `ED_slider_allow_increments_set`.
 * \param increment_step: cannot be 0.
 */
void ED_slider_increment_step_set(tSlider *slider, float increment_step);

/** One bool value for each side of the slider. Allows to enable overshoot only on one side. */
void ED_slider_allow_overshoot_set(tSlider *slider, bool lower, bool upper);

/**
 * Set the soft limits for the slider, which are applied until the user enables overshooting.
 */
void ED_slider_factor_bounds_set(tSlider *slider,
                                 float factor_bound_lower,
                                 float factor_bound_upper);

bool ED_slider_allow_increments_get(const tSlider *slider);
void ED_slider_allow_increments_set(tSlider *slider, bool value);

void ED_slider_mode_set(tSlider *slider, SliderMode mode);
SliderMode ED_slider_mode_get(const tSlider *slider);
void ED_slider_unit_set(tSlider *slider, const char *unit);
/* Set a name that will show next to the slider to indicate which property is modified currently.
 * To clear, set to an empty string. */
void ED_slider_property_label_set(tSlider *slider, const char *property_label);

/* ************** XXX OLD CRUFT WARNING ************* */

/**
 * Now only used in 2D spaces, like time, f-curve, NLA, image, etc.
 *
 * \note Shift/Control are not configurable key-bindings.
 */
void apply_keyb_grid(
    bool shift, bool ctrl, float *val, float fac1, float fac2, float fac3, int invert);

/* where else to go ? */
void unpack_menu(bContext *C,
                 const char *opname,
                 const char *id_name,
                 const char *abs_name,
                 const char *folder,
                 PackedFile *pf);
