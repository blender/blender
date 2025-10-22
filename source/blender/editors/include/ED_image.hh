/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "DNA_listBase.h"
#include "DNA_space_types.h"

#include "BLI_string_ref.hh"

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

/* `image_draw.cc` */

float ED_space_image_zoom_level(const View2D *v2d, int grid_dimension);
void ED_space_image_grid_steps(SpaceImage *sima,
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

/* `image_edit.cc`, exported for transform. */

Image *ED_space_image(const SpaceImage *sima);
void ED_space_image_set(Main *bmain, SpaceImage *sima, Image *ima, bool automatic);
void ED_space_image_sync(Main *bmain, Image *image, bool ignore_render_viewer);
void ED_space_image_auto_set(const bContext *C, SpaceImage *sima);
Mask *ED_space_image_get_mask(const SpaceImage *sima);
void ED_space_image_set_mask(bContext *C, SpaceImage *sima, Mask *mask);

/**
 * Returns mouse position in image space.
 */
bool ED_space_image_get_position(SpaceImage *sima,
                                 ARegion *region,
                                 const int mval[2],
                                 float r_fpos[2]);
/**
 * Returns color in linear space, matching #ED_space_node_color_sample().
 */
bool ED_space_image_color_sample(
    SpaceImage *sima, ARegion *region, const int mval[2], float r_col[3], bool *r_is_data);
ImBuf *ED_space_image_acquire_buffer(SpaceImage *sima, void **r_lock, int tile);
/**
 * Get the #SpaceImage flag that is valid for the given ibuf.
 */
int ED_space_image_get_display_channel_mask(ImBuf *ibuf);
void ED_space_image_release_buffer(SpaceImage *sima, ImBuf *ibuf, void *lock);
bool ED_space_image_has_buffer(SpaceImage *sima);

void ED_space_image_get_size(SpaceImage *sima, int *r_width, int *r_height);
void ED_space_image_get_size_fl(SpaceImage *sima, float r_size[2]);
void ED_space_image_get_aspect(SpaceImage *sima, float *r_aspx, float *r_aspy);
void ED_space_image_get_zoom(SpaceImage *sima,
                             const ARegion *region,
                             float *r_zoomx,
                             float *r_zoomy);
void ED_space_image_get_uv_aspect(SpaceImage *sima, float *r_aspx, float *r_aspy);

void ED_space_image_scopes_update(const bContext *C,
                                  SpaceImage *sima,
                                  ImBuf *ibuf,
                                  bool use_view_settings);

/**
 * Enable the paint cursor if it isn't already.
 *
 * purpose is to make sure the paint cursor is shown if paint mode is enabled in the image editor.
 * The paint poll will ensure that the cursor is hidden when not in paint mode.
 */
void ED_space_image_paint_update(Main *bmain, wmWindowManager *wm, Scene *scene);

void ED_image_get_uv_aspect(Image *ima, ImageUser *iuser, float *r_aspx, float *r_aspy);
/** Takes `event->mval`. */
void ED_image_mouse_pos(SpaceImage *sima, const ARegion *region, const int mval[2], float co[2]);
void ED_image_view_center_to_point(SpaceImage *sima, float x, float y);
void ED_image_point_pos(
    SpaceImage *sima, const ARegion *region, float x, float y, float *r_x, float *r_y);
void ED_image_point_pos__reverse(SpaceImage *sima,
                                 const ARegion *region,
                                 const float co[2],
                                 float r_co[2]);
/**
 * This is more a user-level functionality, for going to `next/prev` used slot,
 * Stepping onto the last unused slot too.
 */
bool ED_image_slot_cycle(Image *image, int direction);

bool ED_space_image_show_render(const SpaceImage *sima);
bool ED_space_image_show_paint(const SpaceImage *sima);
bool ED_space_image_show_mask(const SpaceImage *sima);
bool ED_space_image_show_uvedit(const SpaceImage *sima, Object *obedit);

bool ED_space_image_paint_curve(const bContext *C);

/**
 * Matches clip function.
 */
bool ED_space_image_check_show_maskedit(SpaceImage *sima, Object *obedit);

/**
 * Returns true when the following conditions are met:
 * - Current space is Image Editor.
 * - The image editor is not a UV Editor.
 * - It is set to Mask mode.
 *
 * It is not required to have mask opened for editing.
 */
bool ED_space_image_maskedit_poll(bContext *C);

/**
 * Returns true when the following conditions are met:
 * - Current space is Image Editor.
 * - The image editor is not a UV Editor.
 * - It is set to Mask mode.
 * - Mask has visible and editable splines.
 *
 * It is not required to have mask opened for editing.
 */
bool ED_space_image_maskedit_visible_splines_poll(bContext *C);

/**
 * Returns true when the following conditions are met:
 * - Current space is Image Editor.
 * - The image editor is not an UV Editor.
 * - It is set to Mask mode.
 * - The space has mask opened.
 */
bool ED_space_image_maskedit_mask_poll(bContext *C);

/**
 * Returns true when the following conditions are met:
 * - Current space is Image Editor.
 * - The image editor is not an UV Editor.
 * - It is set to Mask mode.
 * - The space has mask opened.
 * - Mask has visible and editable splines.
 */
bool ED_space_image_maskedit_mask_visible_splines_poll(bContext *C);

bool ED_space_image_cursor_poll(bContext *C);

/**
 * Used by node view too.
 */
void ED_image_draw_info(Scene *scene,
                        ARegion *region,
                        bool color_manage,
                        bool use_default_view,
                        int channels,
                        int x,
                        int y,
                        const unsigned char cp[4],
                        const float fp[4],
                        const float linearcol[4]);

bool ED_space_image_show_cache(const SpaceImage *sima);
bool ED_space_image_show_cache_and_mval_over(const SpaceImage *sima,
                                             ARegion *region,
                                             const int mval[2]);

bool ED_image_should_save_modified(const Main *bmain);
int ED_image_save_all_modified_info(const Main *bmain, ReportList *reports);
bool ED_image_save_all_modified(const bContext *C, ReportList *reports);

/* `image_sequence.cc` */

struct ImageFrameRange {
  ImageFrameRange *next, *prev;

  /**
   * File path of the first file in the range.
   * May be relative to `G_MAIN->filepath`.
   */
  char filepath[FILE_MAX];
  /* Sequence parameters. */
  int length; /* Does not include placeholders, stops at gaps in sequence. */
  int offset;
  int max_framenr; /* Allows for calculating length including placeholders. */

  /* UDIM tiles. */
  bool udims_detected;
  ListBase udim_tiles;

  /* Temporary data. */
  ListBase frames; /* ImageFrame. */
};

struct ImageFrame {
  ImageFrame *next, *prev;
  int framenr;
};

/**
 * Used for both images and volume file loading.
 */
ListBase ED_image_filesel_detect_sequences(blender::StringRefNull root_path,
                                           wmOperator *op,
                                           bool detect_udim);

bool ED_image_tools_paint_poll(bContext *C);
void ED_paint_cursor_start(Paint *paint, bool (*poll)(bContext *C));
