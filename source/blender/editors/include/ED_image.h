/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
struct Main;
struct ReportList;
struct Scene;
struct SpaceImage;
struct View2D;
struct bContext;
struct Paint;
struct wmOperator;
struct wmWindowManager;

/* image_draw.c */

float ED_space_image_zoom_level(const struct View2D *v2d, int grid_dimension);
void ED_space_image_grid_steps(struct SpaceImage *sima,
                               float grid_steps_x[SI_GRID_STEPS_LEN],
                               float grid_steps_y[SI_GRID_STEPS_LEN],
                               int grid_dimension);
/**
 * Calculate the increment snapping value for UV/image editor based on the zoom factor
 * The code in here (except the offset part) is used in `grid_frag.glsl` (see `grid_res`) for
 * drawing the grid overlay for the UV/Image editor.
 */
float ED_space_image_increment_snap_value(int grid_dimensions,
                                          const float grid_steps[SI_GRID_STEPS_LEN],
                                          float zoom_factor);

/* image_edit.c, exported for transform. */

struct Image *ED_space_image(const struct SpaceImage *sima);
void ED_space_image_set(struct Main *bmain,
                        struct SpaceImage *sima,
                        struct Image *ima,
                        bool automatic);
void ED_space_image_sync(struct Main *bmain, Image *image, bool ignore_render_viewer);
void ED_space_image_auto_set(const struct bContext *C, struct SpaceImage *sima);
struct Mask *ED_space_image_get_mask(const struct SpaceImage *sima);
void ED_space_image_set_mask(struct bContext *C, struct SpaceImage *sima, struct Mask *mask);

/**
 * Returns mouse position in image space.
 */
bool ED_space_image_get_position(struct SpaceImage *sima,
                                 struct ARegion *region,
                                 int mval[2],
                                 float fpos[2]);
/**
 * Returns color in linear space, matching #ED_space_node_color_sample().
 */
bool ED_space_image_color_sample(struct SpaceImage *sima,
                                 struct ARegion *region,
                                 const int mval[2],
                                 float r_col[3],
                                 bool *r_is_data);
struct ImBuf *ED_space_image_acquire_buffer(struct SpaceImage *sima, void **r_lock, int tile);
/**
 * Get the #SpaceImage flag that is valid for the given ibuf.
 */
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

/**
 * Enable the paint cursor if it isn't already.
 *
 * purpose is to make sure the paint cursor is shown if paint mode is enabled in the image editor.
 * The paint poll will ensure that the cursor is hidden when not in paint mode.
 */
void ED_space_image_paint_update(struct Main *bmain,
                                 struct wmWindowManager *wm,
                                 struct Scene *scene);

void ED_image_get_uv_aspect(struct Image *ima,
                            struct ImageUser *iuser,
                            float *r_aspx,
                            float *r_aspy);
/** Takes `event->mval`. */
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
/**
 * This is more a user-level functionality, for going to `next/prev` used slot,
 * Stepping onto the last unused slot too.
 */
bool ED_image_slot_cycle(struct Image *image, int direction);

bool ED_space_image_show_render(const struct SpaceImage *sima);
bool ED_space_image_show_paint(const struct SpaceImage *sima);
bool ED_space_image_show_uvedit(const struct SpaceImage *sima, struct Object *obedit);

bool ED_space_image_paint_curve(const struct bContext *C);

/**
 * Matches clip function.
 */
bool ED_space_image_check_show_maskedit(struct SpaceImage *sima, struct Object *obedit);

/* Returns true when the following conditions are met:
 * - Current space is Image Editor.
 * - The image editor is not a UV Editor.
 * - It is set to Mask mode.
 *
 * It is not required to have mask opened for editing. */
bool ED_space_image_maskedit_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space is Image Editor.
 * - The image editor is not a UV Editor.
 * - It is set to Mask mode.
 * - Mask has visible and editable splines.
 *
 * It is not required to have mask opened for editing. */
bool ED_space_image_maskedit_visible_splines_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space is Image Editor.
 * - The image editor is not an UV Editor.
 * - It is set to Mask mode.
 * - The space has mask opened. */
bool ED_space_image_maskedit_mask_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space is Image Editor.
 * - The image editor is not an UV Editor.
 * - It is set to Mask mode.
 * - The space has mask opened.
 * - Mask has visible and editable splines. */
bool ED_space_image_maskedit_mask_visible_splines_poll(struct bContext *C);

bool ED_space_image_cursor_poll(struct bContext *C);

/**
 * Used by node view too.
 */
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

bool ED_space_image_show_cache(const struct SpaceImage *sima);
bool ED_space_image_show_cache_and_mval_over(const struct SpaceImage *sima,
                                             struct ARegion *region,
                                             const int mval[2]);

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
  bool udims_detected;
  ListBase udim_tiles;

  /* Temporary data. */
  ListBase frames;
} ImageFrameRange;

/**
 * Used for both images and volume file loading.
 */
ListBase ED_image_filesel_detect_sequences(struct Main *bmain,
                                           struct wmOperator *op,
                                           bool detect_udim);

bool ED_image_tools_paint_poll(struct bContext *C);
void ED_paint_cursor_start(struct Paint *p, bool (*poll)(struct bContext *C));

#ifdef __cplusplus
}
#endif
