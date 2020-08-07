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

#pragma once

#include "DNA_listBase.h"
#include "DNA_space_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct ImBuf;
struct Image;
struct ImageUser;
struct LinkNodePair;
struct Main;
struct ReportList;
struct Scene;
struct SpaceImage;
struct ViewLayer;
struct bContext;
struct wmOperator;
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
                                 struct ARegion *region,
                                 int mval[2],
                                 float r_col[3]);
struct ImBuf *ED_space_image_acquire_buffer(struct SpaceImage *sima, void **r_lock, int tile);
int ED_space_image_get_display_channel_mask(struct ImBuf *ibuf);
void ED_space_image_release_buffer(struct SpaceImage *sima, struct ImBuf *ibuf, void *lock);
bool ED_space_image_has_buffer(struct SpaceImage *sima);

void ED_space_image_get_size(struct SpaceImage *sima, int *r_width, int *r_height);
void ED_space_image_get_size_fl(struct SpaceImage *sima, float r_size[2]);
void ED_space_image_get_aspect(struct SpaceImage *sima, float *r_aspx, float *r_aspy);
void ED_space_image_get_zoom(struct SpaceImage *sima,
                             const struct ARegion *region,
                             float *r_zoomx,
                             float *r_zoomy);
void ED_space_image_get_uv_aspect(struct SpaceImage *sima, float *r_aspx, float *r_aspy);

void ED_space_image_scopes_update(const struct bContext *C,
                                  struct SpaceImage *sima,
                                  struct ImBuf *ibuf,
                                  bool use_view_settings);

void ED_space_image_paint_update(struct Main *bmain,
                                 struct wmWindowManager *wm,
                                 struct Scene *scene);

void ED_image_get_uv_aspect(struct Image *ima,
                            struct ImageUser *iuser,
                            float *r_aspx,
                            float *r_aspy);
void ED_image_mouse_pos(struct SpaceImage *sima,
                        const struct ARegion *region,
                        const int mval[2],
                        float co[2]);
void ED_image_view_center_to_point(struct SpaceImage *sima, float x, float y);
void ED_image_point_pos(struct SpaceImage *sima,
                        const struct ARegion *region,
                        float x,
                        float y,
                        float *r_x,
                        float *r_y);
void ED_image_point_pos__reverse(struct SpaceImage *sima,
                                 const struct ARegion *region,
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
bool ED_space_image_cursor_poll(struct bContext *C);

void ED_image_draw_info(struct Scene *scene,
                        struct ARegion *region,
                        bool color_manage,
                        bool use_default_view,
                        int channels,
                        int x,
                        int y,
                        const unsigned char cp[4],
                        const float fp[4],
                        const float linearcol[4],
                        const int *zp,
                        const float *zpf);

bool ED_space_image_show_cache(struct SpaceImage *sima);

bool ED_image_should_save_modified(const struct Main *bmain);
int ED_image_save_all_modified_info(const struct Main *bmain, struct ReportList *reports);
bool ED_image_save_all_modified(const struct bContext *C, struct ReportList *reports);

/* image_sequence.c */
typedef struct ImageFrameRange {
  struct ImageFrameRange *next, *prev;

  /** Absolute file path of the first file in the range. */
  char filepath[FILE_MAX];
  /* Sequence parameters. */
  int length;
  int offset;
  /* UDIM tiles. */
  ListBase udim_tiles;

  /* Temporary data. */
  ListBase frames;
} ImageFrameRange;

ListBase ED_image_filesel_detect_sequences(struct Main *bmain,
                                           struct wmOperator *op,
                                           const bool detect_udim);

#ifdef __cplusplus
}
#endif
