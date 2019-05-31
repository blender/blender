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
 * Generic 2d view with should allow drawing grids,
 * panning, zooming, scrolling, ..
 */

/** \file
 * \ingroup editorui
 */

#ifndef __UI_VIEW2D_H__
#define __UI_VIEW2D_H__

#include "BLI_compiler_attrs.h"

/* ------------------------------------------ */
/* Settings and Defines:                      */

/* ---- General Defines ---- */

/* generic value to use when coordinate lies out of view when converting */
#define V2D_IS_CLIPPED 12000

/* Common View2D view types
 * NOTE: only define a type here if it completely sets all (+/- a few) of the relevant flags
 *       and settings for a View2D region, and that set of settings is used in more
 *       than one specific place
 */
enum eView2D_CommonViewTypes {
  /* custom view type (region has defined all necessary flags already) */
  V2D_COMMONVIEW_CUSTOM = -1,
  /* standard (only use this when setting up a new view, as a sensible base for most settings) */
  V2D_COMMONVIEW_STANDARD,
  /* listview (i.e. Outliner) */
  V2D_COMMONVIEW_LIST,
  /* stackview (this is basically a list where new items are added at the top) */
  V2D_COMMONVIEW_STACK,
  /* headers (this is basically the same as listview, but no y-panning) */
  V2D_COMMONVIEW_HEADER,
  /* ui region containing panels */
  V2D_COMMONVIEW_PANELS_UI,
};

/* ---- Defines for Scroller Arguments ----- */

/* ------ Defines for Scrollers ----- */

/* scroller area */
#define V2D_SCROLL_HEIGHT (0.45f * U.widget_unit)
#define V2D_SCROLL_WIDTH (0.45f * U.widget_unit)
/* For scrollers with scale handlers */
#define V2D_SCROLL_HEIGHT_HANDLES (0.6f * U.widget_unit)
#define V2D_SCROLL_WIDTH_HANDLES (0.6f * U.widget_unit)

/* scroller 'handles' hotspot radius for mouse */
#define V2D_SCROLLER_HANDLE_SIZE (0.6f * U.widget_unit)

/* ------ Define for UI_view2d_sync ----- */

/* means copy it from another v2d */
#define V2D_LOCK_SET 0
/* means copy it to the other v2ds */
#define V2D_LOCK_COPY 1

/* ------------------------------------------ */
/* Macros:                                    */

/* test if mouse in a scrollbar (assume that scroller availability has been tested) */
#define IN_2D_VERT_SCROLL(v2d, co) (BLI_rcti_isect_pt_v(&v2d->vert, co))
#define IN_2D_HORIZ_SCROLL(v2d, co) (BLI_rcti_isect_pt_v(&v2d->hor, co))

#define IN_2D_VERT_SCROLL_RECT(v2d, rct) (BLI_rcti_isect(&v2d->vert, rct, NULL))
#define IN_2D_HORIZ_SCROLL_RECT(v2d, rct) (BLI_rcti_isect(&v2d->hor, rct, NULL))

/* ------------------------------------------ */
/* Type definitions:                          */

struct View2D;
struct View2DScrollers;

struct ARegion;
struct Scene;
struct ScrArea;
struct bContext;
struct bScreen;
struct rctf;
struct wmGizmoGroupType;
struct wmKeyConfig;

typedef struct View2DScrollers View2DScrollers;

/* ----------------------------------------- */
/* Prototypes:                               */

/* refresh and validation (of view rects) */
void UI_view2d_region_reinit(struct View2D *v2d, short type, int winx, int winy);

void UI_view2d_curRect_validate(struct View2D *v2d);
void UI_view2d_curRect_reset(struct View2D *v2d);
void UI_view2d_sync(struct bScreen *screen, struct ScrArea *sa, struct View2D *v2dcur, int flag);

void UI_view2d_totRect_set(struct View2D *v2d, int width, int height);
void UI_view2d_totRect_set_resize(struct View2D *v2d, int width, int height, bool resize);

void UI_view2d_mask_from_win(const struct View2D *v2d, struct rcti *r_mask);

/* per tab offsets, returns 1 if tab changed */
bool UI_view2d_tab_set(struct View2D *v2d, int tab);

void UI_view2d_zoom_cache_reset(void);

/* view matrix operations */
void UI_view2d_view_ortho(const struct View2D *v2d);
void UI_view2d_view_orthoSpecial(struct ARegion *ar, struct View2D *v2d, const bool xaxis);
void UI_view2d_view_restore(const struct bContext *C);

/* grid drawing */
void UI_view2d_constant_grid_draw(struct View2D *v2d, float step);
void UI_view2d_multi_grid_draw(
    struct View2D *v2d, int colorid, float step, int level_size, int totlevels);

void UI_view2d_draw_lines_y__values(const struct View2D *v2d);
void UI_view2d_draw_lines_x__values(const struct View2D *v2d);
void UI_view2d_draw_lines_x__discrete_values(const struct View2D *v2d);
void UI_view2d_draw_lines_x__discrete_time(const struct View2D *v2d, const struct Scene *scene);
void UI_view2d_draw_lines_x__discrete_frames_or_seconds(const struct View2D *v2d,
                                                        const struct Scene *scene,
                                                        bool display_seconds);
void UI_view2d_draw_lines_x__frames_or_seconds(const struct View2D *v2d,
                                               const struct Scene *scene,
                                               bool display_seconds);

float UI_view2d_grid_resolution_x__frames_or_seconds(const struct View2D *v2d,
                                                     const struct Scene *scene,
                                                     bool display_seconds);
float UI_view2d_grid_resolution_y__values(const struct View2D *v2d);

/* scale indicator text drawing */
void UI_view2d_draw_scale_y__values(const struct ARegion *ar,
                                    const struct View2D *v2d,
                                    const struct rcti *rect,
                                    int colorid);
void UI_view2d_draw_scale_y__block(const struct ARegion *ar,
                                   const struct View2D *v2d,
                                   const struct rcti *rect,
                                   int colorid);
void UI_view2d_draw_scale_x__discrete_frames_or_seconds(const struct ARegion *ar,
                                                        const struct View2D *v2d,
                                                        const struct rcti *rect,
                                                        const struct Scene *scene,
                                                        bool display_seconds,
                                                        int colorid);
void UI_view2d_draw_scale_x__frames_or_seconds(const struct ARegion *ar,
                                               const struct View2D *v2d,
                                               const struct rcti *rect,
                                               const struct Scene *scene,
                                               bool display_seconds,
                                               int colorid);

/* scrollbar drawing */
View2DScrollers *UI_view2d_scrollers_calc(struct View2D *v2d, const struct rcti *mask_custom);
void UI_view2d_scrollers_draw(struct View2D *v2d, View2DScrollers *scrollers);
void UI_view2d_scrollers_free(View2DScrollers *scrollers);

/* list view tools */
void UI_view2d_listview_view_to_cell(float columnwidth,
                                     float rowheight,
                                     float startx,
                                     float starty,
                                     float viewx,
                                     float viewy,
                                     int *column,
                                     int *row);

/* coordinate conversion */
float UI_view2d_region_to_view_x(const struct View2D *v2d, float x);
float UI_view2d_region_to_view_y(const struct View2D *v2d, float y);
void UI_view2d_region_to_view(
    const struct View2D *v2d, float x, float y, float *r_view_x, float *r_view_y) ATTR_NONNULL();
void UI_view2d_region_to_view_rctf(const struct View2D *v2d,
                                   const struct rctf *rect_src,
                                   struct rctf *rect_dst) ATTR_NONNULL();

float UI_view2d_view_to_region_x(const struct View2D *v2d, float x);
float UI_view2d_view_to_region_y(const struct View2D *v2d, float y);
bool UI_view2d_view_to_region_clip(
    const struct View2D *v2d, float x, float y, int *r_region_x, int *r_region_y) ATTR_NONNULL();

void UI_view2d_view_to_region(
    struct View2D *v2d, float x, float y, int *r_region_x, int *r_region_y) ATTR_NONNULL();
void UI_view2d_view_to_region_fl(
    struct View2D *v2d, float x, float y, float *r_region_x, float *r_region_y) ATTR_NONNULL();
void UI_view2d_view_to_region_m4(struct View2D *v2d, float matrix[4][4]) ATTR_NONNULL();
void UI_view2d_view_to_region_rcti(struct View2D *v2d,
                                   const struct rctf *rect_src,
                                   struct rcti *rect_dst) ATTR_NONNULL();
bool UI_view2d_view_to_region_rcti_clip(struct View2D *v2d,
                                        const struct rctf *rect_src,
                                        struct rcti *rect_dst) ATTR_NONNULL();

/* utilities */
struct View2D *UI_view2d_fromcontext(const struct bContext *C);
struct View2D *UI_view2d_fromcontext_rwin(const struct bContext *C);

void UI_view2d_scale_get(struct View2D *v2d, float *r_x, float *r_y);
float UI_view2d_scale_get_x(const struct View2D *v2d);
float UI_view2d_scale_get_y(const struct View2D *v2d);
void UI_view2d_scale_get_inverse(struct View2D *v2d, float *r_x, float *r_y);

void UI_view2d_center_get(struct View2D *v2d, float *r_x, float *r_y);
void UI_view2d_center_set(struct View2D *v2d, float x, float y);

void UI_view2d_offset(struct View2D *v2d, float xfac, float yfac);

char UI_view2d_mouse_in_scrollers_ex(
    const struct ARegion *ar, const struct View2D *v2d, int x, int y, int *r_scroll);
char UI_view2d_mouse_in_scrollers(const struct ARegion *ar,
                                  const struct View2D *v2d,
                                  int x,
                                  int y);
char UI_view2d_rect_in_scrollers_ex(const struct ARegion *ar,
                                    const struct View2D *v2d,
                                    const struct rcti *rect,
                                    int *r_scroll);
char UI_view2d_rect_in_scrollers(const struct ARegion *ar,
                                 const struct View2D *v2d,
                                 const struct rcti *rect);

/* cached text drawing in v2d, to allow pixel-aligned draw as post process */
void UI_view2d_text_cache_add(
    struct View2D *v2d, float x, float y, const char *str, size_t str_len, const char col[4]);
void UI_view2d_text_cache_add_rectf(struct View2D *v2d,
                                    const struct rctf *rect_view,
                                    const char *str,
                                    size_t str_len,
                                    const char col[4]);
void UI_view2d_text_cache_draw(struct ARegion *ar);

/* operators */
void ED_operatortypes_view2d(void);
void ED_keymap_view2d(struct wmKeyConfig *keyconf);

void UI_view2d_smooth_view(struct bContext *C,
                           struct ARegion *ar,
                           const struct rctf *cur,
                           const int smooth_viewtx);

#define UI_MARKER_MARGIN_Y (42 * UI_DPI_FAC)
#define UI_TIME_SCRUB_MARGIN_Y (23 * UI_DPI_FAC)

/* Gizmo Types */

/* view2d_gizmo_navigate.c */
/* Caller passes in own idname.  */
void VIEW2D_GGT_navigate_impl(struct wmGizmoGroupType *gzgt, const char *idname);

#endif /* __UI_VIEW2D_H__ */
