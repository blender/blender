/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_image.h
 *  \ingroup editors
 */

#ifndef ED_IMAGE_H
#define ED_IMAGE_H

struct SpaceImage;
struct Main;
struct bContext;
struct Image;
struct ImageUser;
struct ToolSettings;
struct uiBlock;
struct wmWindowManager;

/* space_image.c, exported for transform */
struct Image *ED_space_image(struct SpaceImage *sima);
void ED_space_image_set(struct SpaceImage *sima, struct Scene *scene, struct Object *obedit, struct Image *ima);

struct ImBuf *ED_space_image_acquire_buffer(struct SpaceImage *sima, void **lock_r);
void ED_space_image_release_buffer(struct SpaceImage *sima, void *lock);
int ED_space_image_has_buffer(struct SpaceImage *sima);

void ED_space_image_size(struct SpaceImage *sima, int *width, int *height);
void ED_space_image_aspect(struct SpaceImage *sima, float *aspx, float *aspy);
void ED_space_image_zoom(struct SpaceImage *sima, struct ARegion *ar, float *zoomx, float *zoomy);
void ED_space_image_uv_aspect(struct SpaceImage *sima, float *aspx, float *aspy);

void ED_space_image_paint_update(struct wmWindowManager *wm, struct ToolSettings *settings);
void ED_space_image_uv_sculpt_update(struct wmWindowManager *wm, struct ToolSettings *settings);

void ED_image_size(struct Image *ima, int *width, int *height);
void ED_image_aspect(struct Image *ima, float *aspx, float *aspy);
void ED_image_uv_aspect(struct Image *ima, float *aspx, float *aspy);

int ED_space_image_show_render(struct SpaceImage *sima);
int ED_space_image_show_paint(struct SpaceImage *sima);
int ED_space_image_show_uvedit(struct SpaceImage *sima, struct Object *obedit);
int ED_space_image_show_uvshadow(struct SpaceImage *sima, struct Object *obedit);

/* UI level image (texture) updating... render calls own stuff (too) */
void ED_image_update_frame(const struct Main *mainp, int cfra);

void ED_image_draw_info(struct ARegion *ar, int color_manage, int channels,
                        int x, int y, const char cp[4], const float fp[4], int *zp, float *zpf);

#endif /* ED_IMAGE_H */

