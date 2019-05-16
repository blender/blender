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
 * \ingroup editors
 */

#ifndef __ED_IMAGE_H__
#define __ED_IMAGE_H__

struct ARegion;
struct ImBuf;
struct Image;
struct ImageUser;
struct ReportList;
struct Scene;
struct SpaceImage;
struct ToolSettings;
struct ViewLayer;
struct bContext;
struct wmWindowManager;

/* image_edit.c, exported for transform */
struct Image *ED_space_image(struct SpaceImage *sima);
void ED_space_image_set(struct Main *bmain,
                        struct SpaceImage *sima,
                        struct Object *obedit,
                        struct Image *ima,
                        bool automatic);
void ED_space_image_auto_set(const struct bContext *C, struct SpaceImage *sima);
struct Mask *ED_space_image_get_mask(struct SpaceImage *sima);
void ED_space_image_set_mask(struct bContext *C, struct SpaceImage *sima, struct Mask *mask);

bool ED_space_image_color_sample(struct SpaceImage *sima,
                                 struct ARegion *ar,
                                 int mval[2],
                                 float r_col[3]);
struct ImBuf *ED_space_image_acquire_buffer(struct SpaceImage *sima, void **r_lock);
void ED_space_image_release_buffer(struct SpaceImage *sima, struct ImBuf *ibuf, void *lock);
bool ED_space_image_has_buffer(struct SpaceImage *sima);

void ED_space_image_get_size(struct SpaceImage *sima, int *width, int *height);
void ED_space_image_get_size_fl(struct SpaceImage *sima, float size[2]);
void ED_space_image_get_aspect(struct SpaceImage *sima, float *aspx, float *aspy);
void ED_space_image_get_zoom(struct SpaceImage *sima,
                             struct ARegion *ar,
                             float *zoomx,
                             float *zoomy);
void ED_space_image_get_uv_aspect(struct SpaceImage *sima, float *aspx, float *aspy);

void ED_space_image_scopes_update(const struct bContext *C,
                                  struct SpaceImage *sima,
                                  struct ImBuf *ibuf,
                                  bool use_view_settings);

void ED_space_image_paint_update(struct Main *bmain,
                                 struct wmWindowManager *wm,
                                 struct Scene *scene);

void ED_image_get_uv_aspect(struct Image *ima, struct ImageUser *iuser, float *aspx, float *aspy);
void ED_image_mouse_pos(struct SpaceImage *sima,
                        struct ARegion *ar,
                        const int mval[2],
                        float co[2]);
void ED_image_point_pos(
    struct SpaceImage *sima, struct ARegion *ar, float x, float y, float *xr, float *yr);
void ED_image_point_pos__reverse(struct SpaceImage *sima,
                                 struct ARegion *ar,
                                 const float co[2],
                                 float r_co[2]);
bool ED_image_slot_cycle(struct Image *image, int direction);

bool ED_space_image_show_render(struct SpaceImage *sima);
bool ED_space_image_show_paint(struct SpaceImage *sima);
bool ED_space_image_show_uvedit(struct SpaceImage *sima, struct Object *obedit);

bool ED_space_image_paint_curve(const struct bContext *C);

bool ED_space_image_check_show_maskedit(struct SpaceImage *sima, struct ViewLayer *view_layer);
bool ED_space_image_maskedit_poll(struct bContext *C);
bool ED_space_image_maskedit_mask_poll(struct bContext *C);

void ED_image_draw_info(struct Scene *scene,
                        struct ARegion *ar,
                        bool color_manage,
                        bool use_default_view,
                        int channels,
                        int x,
                        int y,
                        const unsigned char cp[4],
                        const float fp[4],
                        const float linearcol[4],
                        int *zp,
                        float *zpf);

bool ED_space_image_show_cache(struct SpaceImage *sima);

int ED_image_save_all_modified_count(const struct bContext *C);
bool ED_image_save_all_modified(const struct bContext *C, struct ReportList *reports);

#endif /* __ED_IMAGE_H__ */
