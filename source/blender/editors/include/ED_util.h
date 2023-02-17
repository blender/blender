/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "WM_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct IDRemapper;
struct Main;
struct bContext;

/* ed_util.cc */

void ED_editors_init_for_undo(struct Main *bmain);
void ED_editors_init(struct bContext *C);
/**
 * Frees all edit-mode stuff.
 */
void ED_editors_exit(struct Main *bmain, bool do_undo_system);

bool ED_editors_flush_edits_for_object_ex(struct Main *bmain,
                                          struct Object *ob,
                                          bool for_render,
                                          bool check_needs_flush);
bool ED_editors_flush_edits_for_object(struct Main *bmain, struct Object *ob);

/**
 * Flush any temp data from object editing to DNA before writing files, rendering, copying, etc.
 */
bool ED_editors_flush_edits_ex(struct Main *bmain, bool for_render, bool check_needs_flush);
bool ED_editors_flush_edits(struct Main *bmain);

/**
 * Use to free ID references within runtime data (stored outside of DNA)
 *
 * \param new_id: may be NULL to unlink \a old_id.
 */
void ED_spacedata_id_remap_single(struct ScrArea *area,
                                  struct SpaceLink *sl,
                                  struct ID *old_id,
                                  struct ID *new_id);
void ED_spacedata_id_remap(struct ScrArea *area,
                           struct SpaceLink *sl,
                           const struct IDRemapper *mappings);

void ED_operatortypes_edutils(void);

/* Drawing */

/**
 * Callback that draws a line between the mouse and a position given as the initial argument.
 */
void ED_region_draw_mouse_line_cb(const struct bContext *C,
                                  struct ARegion *region,
                                  void *arg_info);

/**
 * \note Keep in sync with #BKE_image_stamp_buf.
 */
void ED_region_image_metadata_draw(
    int x, int y, struct ImBuf *ibuf, const rctf *frame, float zoomx, float zoomy);

/* Slider */

struct tSlider;

struct tSlider *ED_slider_create(struct bContext *C);
/**
 * For modal operations so the percentage doesn't pop on the first mouse movement.
 */
void ED_slider_init(struct tSlider *slider, const struct wmEvent *event);
/**
 * Calculate slider factor based on mouse position.
 */
bool ED_slider_modal(struct tSlider *slider, const struct wmEvent *event);
void ED_slider_destroy(struct bContext *C, struct tSlider *slider);

/**
 * Return string based on the current state of the slider.
 */
void ED_slider_status_string_get(const struct tSlider *slider,
                                 char *status_string,
                                 size_t size_of_status_string);

float ED_slider_factor_get(struct tSlider *slider);
void ED_slider_factor_set(struct tSlider *slider, float factor);

bool ED_slider_allow_overshoot_get(struct tSlider *slider);
void ED_slider_allow_overshoot_set(struct tSlider *slider, bool value);

bool ED_slider_allow_increments_get(struct tSlider *slider);
void ED_slider_allow_increments_set(struct tSlider *slider, bool value);

bool ED_slider_is_bidirectional_get(struct tSlider *slider);
void ED_slider_is_bidirectional_set(struct tSlider *slider, bool value);

/* ************** XXX OLD CRUFT WARNING ************* */

/**
 * Now only used in 2D spaces, like time, f-curve, NLA, image, etc.
 *
 * \note Shift/Control are not configurable key-bindings.
 */
void apply_keyb_grid(
    bool shift, bool ctrl, float *val, float fac1, float fac2, float fac3, int invert);

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
