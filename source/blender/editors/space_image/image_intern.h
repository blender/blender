/**
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_IMAGE_INTERN_H
#define ED_IMAGE_INTERN_H

/* internal exports only */
struct bContext;
struct ARegion;
struct ARegionType;
struct ScrArea;
struct SpaceImage;
struct Object;
struct Image;
struct ImBuf;
struct wmOperatorType;
struct Scene;
struct bNodeTree;

/* space_image.c */
struct ARegion *image_has_buttons_region(struct ScrArea *sa);
struct ARegion *image_has_scope_region(struct ScrArea *sa);

/* image_header.c */
void image_header_buttons(const struct bContext *C, struct ARegion *ar);

void IMAGE_OT_toolbox(struct wmOperatorType *ot);

/* image_draw.c */
void draw_image_main(struct SpaceImage *sima, struct ARegion *ar, struct Scene *scene);
void draw_image_info(struct ARegion *ar, int channels, int x, int y, char *cp, float *fp, int *zp, float *zpf);
void draw_image_grease_pencil(struct bContext *C, short onlyv2d);

/* image_ops.c */
int space_image_main_area_poll(struct bContext *C);

void IMAGE_OT_view_all(struct wmOperatorType *ot);
void IMAGE_OT_view_pan(struct wmOperatorType *ot);
void IMAGE_OT_view_selected(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_in(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_out(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_ratio(struct wmOperatorType *ot);

void IMAGE_OT_new(struct wmOperatorType *ot);
void IMAGE_OT_open(struct wmOperatorType *ot);
void IMAGE_OT_replace(struct wmOperatorType *ot);
void IMAGE_OT_reload(struct wmOperatorType *ot);
void IMAGE_OT_save(struct wmOperatorType *ot);
void IMAGE_OT_save_as(struct wmOperatorType *ot);
void IMAGE_OT_save_sequence(struct wmOperatorType *ot);
void IMAGE_OT_pack(struct wmOperatorType *ot);
void IMAGE_OT_unpack(struct wmOperatorType *ot);

void IMAGE_OT_cycle_render_slot(struct wmOperatorType *ot);

void IMAGE_OT_sample(struct wmOperatorType *ot);
void IMAGE_OT_curves_point_set(struct wmOperatorType *ot);

void IMAGE_OT_record_composite(struct wmOperatorType *ot);

/* uvedit_draw.c */
void draw_uvedit_main(struct SpaceImage *sima, struct ARegion *ar, struct Scene *scene, struct Object *obedit);

/* image_panels.c */
struct ImageUser *ntree_get_active_iuser(struct bNodeTree *ntree);
void image_buttons_register(struct ARegionType *art);
void IMAGE_OT_properties(struct wmOperatorType *ot);
void IMAGE_OT_scopes(struct wmOperatorType *ot);

#endif /* ED_IMAGE_INTERN_H */

