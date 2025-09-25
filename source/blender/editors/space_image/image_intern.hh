/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 */

#pragma once

/* internal exports only */
struct ARegion;
struct ARegionType;
struct ImageUser;
struct SpaceImage;
struct bContext;
struct bNodeTree;
struct wmOperatorType;
struct rctf;

/* `space_image.cc` */

extern "C" {
extern const char *image_context_dir[]; /* doc access */
}

/* `image_draw.cc` */

void draw_image_main_helpers(const bContext *C, ARegion *region);
void draw_image_cache(const bContext *C, ARegion *region);
void draw_image_sample_line(SpaceImage *sima);
void draw_image_uv_custom_region(const ARegion *region, const rctf &custom_region);

/* `image_ops.cc` */

bool space_image_main_region_poll(bContext *C);
bool space_image_view_center_cursor_poll(bContext *C);

void IMAGE_OT_view_all(wmOperatorType *ot);
void IMAGE_OT_view_pan(wmOperatorType *ot);
void IMAGE_OT_view_selected(wmOperatorType *ot);
void IMAGE_OT_view_center_cursor(wmOperatorType *ot);
void IMAGE_OT_view_cursor_center(wmOperatorType *ot);
void IMAGE_OT_view_zoom(wmOperatorType *ot);
void IMAGE_OT_view_zoom_in(wmOperatorType *ot);
void IMAGE_OT_view_zoom_out(wmOperatorType *ot);
void IMAGE_OT_view_zoom_ratio(wmOperatorType *ot);
void IMAGE_OT_view_zoom_border(wmOperatorType *ot);
#ifdef WITH_INPUT_NDOF
void IMAGE_OT_view_ndof(wmOperatorType *ot);
#endif

void IMAGE_OT_new(wmOperatorType *ot);
/**
 * Called by other space types too.
 */
void IMAGE_OT_open(wmOperatorType *ot);
void IMAGE_OT_file_browse(wmOperatorType *ot);
/**
 * Called by other space types too.
 */
void IMAGE_OT_match_movie_length(wmOperatorType *ot);
void IMAGE_OT_replace(wmOperatorType *ot);
void IMAGE_OT_reload(wmOperatorType *ot);
void IMAGE_OT_save(wmOperatorType *ot);
void IMAGE_OT_save_as(wmOperatorType *ot);
void IMAGE_OT_save_sequence(wmOperatorType *ot);
void IMAGE_OT_save_all_modified(wmOperatorType *ot);
void IMAGE_OT_pack(wmOperatorType *ot);
void IMAGE_OT_unpack(wmOperatorType *ot);
void IMAGE_OT_clipboard_copy(wmOperatorType *ot);
void IMAGE_OT_clipboard_paste(wmOperatorType *ot);

void IMAGE_OT_flip(wmOperatorType *ot);
void IMAGE_OT_rotate_orthogonal(wmOperatorType *ot);
void IMAGE_OT_invert(wmOperatorType *ot);
void IMAGE_OT_resize(wmOperatorType *ot);

void IMAGE_OT_cycle_render_slot(wmOperatorType *ot);
void IMAGE_OT_clear_render_slot(wmOperatorType *ot);
void IMAGE_OT_add_render_slot(wmOperatorType *ot);
void IMAGE_OT_remove_render_slot(wmOperatorType *ot);

void IMAGE_OT_sample(wmOperatorType *ot);
void IMAGE_OT_sample_line(wmOperatorType *ot);
void IMAGE_OT_curves_point_set(wmOperatorType *ot);

void IMAGE_OT_change_frame(wmOperatorType *ot);

void IMAGE_OT_read_viewlayers(wmOperatorType *ot);
void IMAGE_OT_render_border(wmOperatorType *ot);
void IMAGE_OT_clear_render_border(wmOperatorType *ot);

void IMAGE_OT_tile_add(wmOperatorType *ot);
void IMAGE_OT_tile_remove(wmOperatorType *ot);
void IMAGE_OT_tile_fill(wmOperatorType *ot);

/* image_panels.c */

/**
 * Gets active viewer user.
 */
ImageUser *ntree_get_active_iuser(bNodeTree *ntree);
void image_buttons_register(ARegionType *art);
