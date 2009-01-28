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
struct SpaceImage;
struct Object;
struct Image;
struct ImBuf;
struct wmOperatorType;
struct Scene;

/* space_image.c */
struct Image *get_space_image(struct SpaceImage *sima);
void set_space_image(struct SpaceImage *sima, struct Scene *scene, struct Object *obedit, struct Image *ima);

struct ImBuf *get_space_image_buffer(struct SpaceImage *sima);
void get_space_image_size(struct SpaceImage *sima, int *width, int *height);
void get_space_image_aspect(struct SpaceImage *sima, float *aspx, float *aspy);
void get_space_image_zoom(struct SpaceImage *sima, struct ARegion *ar, float *zoomx, float *zoomy);
void get_space_image_uv_aspect(struct SpaceImage *sima, float *aspx, float *aspy);

int get_space_image_show_render(struct SpaceImage *sima);
int get_space_image_show_paint(struct SpaceImage *sima);
int get_space_image_show_uvedit(struct SpaceImage *sima, struct Object *obedit);
int get_space_image_show_uvshadow(struct SpaceImage *sima, struct Object *obedit);

/* image_header.c */
void image_header_buttons(const struct bContext *C, struct ARegion *ar);

/* image_draw.c */
void draw_image_main(struct SpaceImage *sima, struct ARegion *ar, struct Scene *scene);

/* image_ops.c */
void IMAGE_OT_view_all(struct wmOperatorType *ot);
void IMAGE_OT_view_pan(struct wmOperatorType *ot);
void IMAGE_OT_view_selected(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_in(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_out(struct wmOperatorType *ot);
void IMAGE_OT_view_zoom_ratio(struct wmOperatorType *ot);

/* uvedit_draw.c */
void draw_uvedit_main(struct SpaceImage *sima, struct ARegion *ar, struct Scene *scene, struct Object *obedit);

#endif /* ED_IMAGE_INTERN_H */

