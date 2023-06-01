/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 */

#pragma once

/* internal exports only */
struct ARegion;
struct ARegionType;
struct SpaceImage;
struct bContext;
struct bNodeTree;
struct wmOperatorType;

/* space_image.c */

extern const char *image_context_dir[]; /* doc access */

/* image_draw.c */

void draw_image_main_helpers(const struct bContext *C, struct ARegion *region);
void draw_image_cache(const struct bContext *C, struct ARegion *region);
void draw_image_sample_line(struct SpaceImage *sima);

/* image_ops.c */

bool space_image_main_region_poll(struct bContext *C);
bool space_image_view_center_cursor_poll(struct bContext *C);

void IMAGE_OT_view_all(struct wmOperatorType *ot);
void IMAGE_OT_view_pan(struct wmOperatorType *ot);
void IMAGE_OT_view_selected(struct wmOperatorType *ot);
void IMAGE_OT_view_center_cursor(struct wmOperatorType *ot);
void IMAGE_OT_view_cursor_center(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_in(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_out(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_ratio(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_border(struct wmOperatorType *ot);
#ifdef WITH_INPUT_NDOF
void IMAGE_OT_view_ndof(struct wmOperatorType *ot);
#endif

void IMAGE_OT_new(struct wmOperatorType *ot);
/**
 * Called by other space types too.
 */
void IMAGE_OT_open(struct wmOperatorType *ot);
void IMAGE_OT_file_browse(struct wmOperatorType *ot);
/**
 * Called by other space types too.
 */
void IMAGE_OT_match_movie_length(struct wmOperatorType *ot);
void IMAGE_OT_replace(struct wmOperatorType *ot);
void IMAGE_OT_reload(struct wmOperatorType *ot);
void IMAGE_OT_save(struct wmOperatorType *ot);
void IMAGE_OT_save_as(struct wmOperatorType *ot);
void IMAGE_OT_save_sequence(struct wmOperatorType *ot);
void IMAGE_OT_save_all_modified(struct wmOperatorType *ot);
void IMAGE_OT_pack(struct wmOperatorType *ot);
void IMAGE_OT_unpack(struct wmOperatorType *ot);
void IMAGE_OT_clipboard_copy(struct wmOperatorType *ot);
void IMAGE_OT_clipboard_paste(struct wmOperatorType *ot);

void IMAGE_OT_flip(struct wmOperatorType *ot);
void IMAGE_OT_invert(struct wmOperatorType *ot);
void IMAGE_OT_resize(struct wmOperatorType *ot);

void IMAGE_OT_cycle_render_slot(struct wmOperatorType *ot);
void IMAGE_OT_clear_render_slot(struct wmOperatorType *ot);
void IMAGE_OT_add_render_slot(struct wmOperatorType *ot);
void IMAGE_OT_remove_render_slot(struct wmOperatorType *ot);

void IMAGE_OT_sample(struct wmOperatorType *ot);
void IMAGE_OT_sample_line(struct wmOperatorType *ot);
void IMAGE_OT_curves_point_set(struct wmOperatorType *ot);

void IMAGE_OT_change_frame(struct wmOperatorType *ot);

void IMAGE_OT_read_viewlayers(struct wmOperatorType *ot);
void IMAGE_OT_render_border(struct wmOperatorType *ot);
void IMAGE_OT_clear_render_border(struct wmOperatorType *ot);

void IMAGE_OT_tile_add(struct wmOperatorType *ot);
void IMAGE_OT_tile_remove(struct wmOperatorType *ot);
void IMAGE_OT_tile_fill(struct wmOperatorType *ot);

/* image_panels.c */

/**
 * Gets active viewer user.
 */
struct ImageUser *ntree_get_active_iuser(struct bNodeTree *ntree);
void image_buttons_register(struct ARegionType *art);
