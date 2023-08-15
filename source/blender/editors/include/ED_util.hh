/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "WM_types.hh"

struct IDRemapper;
struct Main;
struct bContext;

/* ed_util.cc */

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
void ED_spacedata_id_remap(ScrArea *area, SpaceLink *sl, const IDRemapper *mappings);

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
    int x, int y, ImBuf *ibuf, const rctf *frame, float zoomx, float zoomy);

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

float ED_slider_factor_get(tSlider *slider);
void ED_slider_factor_set(tSlider *slider, float factor);

/* One bool value for each side of the slider. Allows to enable overshoot only on one side. */
void ED_slider_allow_overshoot_set(tSlider *slider, bool lower, bool upper);

/**
 * Set the soft limits for the slider, which are applied until the user enables overshooting.
 */
void ED_slider_factor_bounds_set(tSlider *slider, float lower_bound, float upper_bound);

bool ED_slider_allow_increments_get(tSlider *slider);
void ED_slider_allow_increments_set(tSlider *slider, bool value);

void ED_slider_mode_set(tSlider *slider, SliderMode unit);
void ED_slider_unit_set(tSlider *slider, const char *unit);

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
