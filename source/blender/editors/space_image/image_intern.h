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
 * \ingroup spimage
 */

#ifndef __IMAGE_INTERN_H__
#define __IMAGE_INTERN_H__

/* internal exports only */
struct ARegion;
struct ARegionType;
struct ScrArea;
struct SpaceImage;
struct bContext;
struct bNodeTree;
struct wmOperatorType;

/* space_image.c */

extern const char *image_context_dir[]; /* doc access */

/* image_draw.c */
void draw_image_main(const struct bContext *C, struct ARegion *ar);
void draw_image_cache(const struct bContext *C, struct ARegion *ar);
void draw_image_grease_pencil(struct bContext *C, bool onlyv2d);
void draw_image_sample_line(struct SpaceImage *sima);

/* image_ops.c */
bool space_image_main_region_poll(struct bContext *C);

void IMAGE_OT_view_all(struct wmOperatorType *ot);
void IMAGE_OT_view_pan(struct wmOperatorType *ot);
void IMAGE_OT_view_selected(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_in(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_out(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_ratio(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_border(struct wmOperatorType *ot);
#ifdef WITH_INPUT_NDOF
void IMAGE_OT_view_ndof(struct wmOperatorType *ot);
#endif

void IMAGE_OT_new(struct wmOperatorType *ot);
void IMAGE_OT_open(struct wmOperatorType *ot);
void IMAGE_OT_match_movie_length(struct wmOperatorType *ot);
void IMAGE_OT_replace(struct wmOperatorType *ot);
void IMAGE_OT_reload(struct wmOperatorType *ot);
void IMAGE_OT_save(struct wmOperatorType *ot);
void IMAGE_OT_save_as(struct wmOperatorType *ot);
void IMAGE_OT_save_sequence(struct wmOperatorType *ot);
void IMAGE_OT_save_all_modified(struct wmOperatorType *ot);
void IMAGE_OT_pack(struct wmOperatorType *ot);
void IMAGE_OT_unpack(struct wmOperatorType *ot);

void IMAGE_OT_invert(struct wmOperatorType *ot);

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

/* image_panels.c */
struct ImageUser *ntree_get_active_iuser(struct bNodeTree *ntree);
void image_buttons_register(struct ARegionType *art);

#endif /* __IMAGE_INTERN_H__ */
