/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 * Generic 2D view with should allow drawing grids,
 * panning, zooming, scrolling, .. etc.
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_rect.h"

/* -------------------------------------------------------------------- */
/** \name General Defines
 * \{ */

/** Generic value to use when coordinate lies out of view when converting. */
#define V2D_IS_CLIPPED 12000

/**
 * Common View2D view types.
 *
 * \note only define a type here if it completely sets all (+/- a few) of the relevant flags and
 * settings for a View2D region, and that set of settings is used in more than one specific place.
 */
enum eView2D_CommonViewTypes {
  /** custom view type (region has defined all necessary flags already). */
  V2D_COMMONVIEW_CUSTOM = -1,
  /** standard (only use this when setting up a new view, as a sensible base for most settings). */
  V2D_COMMONVIEW_STANDARD,
  /** List-view (i.e. Outliner). */
  V2D_COMMONVIEW_LIST,
  /** Stack-view (this is basically a list where new items are added at the top). */
  V2D_COMMONVIEW_STACK,
  /** headers (this is basically the same as list-view, but no Y-panning). */
  V2D_COMMONVIEW_HEADER,
  /** UI region containing panels. */
  V2D_COMMONVIEW_PANELS_UI,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Defines for Scroll Bars
 * \{ */

/** Scroll bar area. */

/* Maximum has to include outline which varies with line width. */
#define V2D_SCROLL_HEIGHT ((0.45f * U.widget_unit) + (2.0f * U.pixelsize))
#define V2D_SCROLL_WIDTH ((0.45f * U.widget_unit) + (2.0f * U.pixelsize))

/* Alpha of scroll-bar when at minimum size. */
#define V2D_SCROLL_MIN_ALPHA (0.4f)

/* Minimum size needs to include outline which varies with line width. */
#define V2D_SCROLL_MIN_WIDTH ((5.0f * UI_SCALE_FAC) + (2.0f * U.pixelsize))

/* When to start showing the full-width scroller. */
#define V2D_SCROLL_HIDE_WIDTH (AREAMINX * UI_SCALE_FAC)
#define V2D_SCROLL_HIDE_HEIGHT (HEADERY * UI_SCALE_FAC)

/** Scroll bars with 'handles' used for scale (zoom). */
#define V2D_SCROLL_HANDLE_HEIGHT (0.6f * U.widget_unit)
#define V2D_SCROLL_HANDLE_WIDTH (0.6f * U.widget_unit)

/** Scroll bar with 'handles' hot-spot radius for cursor proximity. */
#define V2D_SCROLL_HANDLE_SIZE_HOTSPOT (0.6f * U.widget_unit)

/** Don't allow scroll thumb to show below this size (so it's never too small to click on). */
#define V2D_SCROLL_THUMB_SIZE_MIN (30.0 * UI_SCALE_FAC)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Define for #UI_view2d_sync
 * \{ */

/* means copy it from another v2d */
#define V2D_LOCK_SET 0
/* means copy it to the other v2ds */
#define V2D_LOCK_COPY 1

/** \} */

/* -------------------------------------------------------------------- */
/** \name Macros
 * \{ */

/* Test if mouse in a scroll-bar (assume that scroller availability has been tested). */
#define IN_2D_VERT_SCROLL(v2d, co) (BLI_rcti_isect_pt_v(&v2d->vert, co))
#define IN_2D_HORIZ_SCROLL(v2d, co) (BLI_rcti_isect_pt_v(&v2d->hor, co))

#define IN_2D_VERT_SCROLL_RECT(v2d, rct) (BLI_rcti_isect(&v2d->vert, rct, NULL))
#define IN_2D_HORIZ_SCROLL_RECT(v2d, rct) (BLI_rcti_isect(&v2d->hor, rct, NULL))

/** \} */

/* -------------------------------------------------------------------- */
/** \name Forward Declarations
 * \{ */

struct View2D;

struct ARegion;
struct Scene;
struct ScrArea;
struct bContext;
struct bScreen;
struct rctf;
struct rcti;
struct wmEvent;
struct wmGizmoGroupType;
struct wmKeyConfig;
struct wmOperator;
struct wmOperatorType;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Prototypes
 * \{ */

/**
 * Refresh and validation (of view rects).
 *
 * Initialize all relevant View2D data (including view rects if first time)
 * and/or refresh mask sizes after view resize.
 *
 * - For some of these presets, it is expected that the region will have defined some
 *   additional settings necessary for the customization of the 2D viewport to its requirements
 * - This function should only be called from region init() callbacks, where it is expected that
 *   this is called before #UI_view2d_size_update(),
 *   as this one checks that the rects are properly initialized.
 */
void UI_view2d_region_reinit(View2D *v2d, short type, int winx, int winy);

void UI_view2d_curRect_validate(View2D *v2d);
/**
 * Restore 'cur' rect to standard orientation (i.e. optimal maximum view of tot).
 * This does not take into account if zooming the view on an axis
 * will improve the view (if allowed).
 */
void UI_view2d_curRect_reset(View2D *v2d);
bool UI_view2d_area_supports_sync(ScrArea *area);
/**
 * Called by menus to activate it, or by view2d operators
 * to make sure 'related' views stay in synchrony.
 */
void UI_view2d_sync(bScreen *screen, ScrArea *area, View2D *v2dcur, int flag);

/**
 * Perform all required updates after `v2d->cur` as been modified.
 * This includes like validation view validation (#UI_view2d_curRect_validate).
 *
 * Current intent is to use it from user code, such as view navigation and zoom operations.
 */
void UI_view2d_curRect_changed(const bContext *C, View2D *v2d);

void UI_view2d_totRect_set(View2D *v2d, int width, int height);

void UI_view2d_mask_from_win(const View2D *v2d, rcti *r_mask);

void UI_view2d_zoom_cache_reset();

/**
 * Clamp view2d area to what's visible, preventing
 * scrolling vertically to infinity.
 */
void UI_view2d_curRect_clamp_y(View2D *v2d);

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Matrix Operations
 * \{ */

/**
 * Set view matrices to use 'cur' rect as viewing frame for View2D drawing.
 */
void UI_view2d_view_ortho(const View2D *v2d);
/**
 * Set view matrices to only use one axis of 'cur' only
 *
 * \param xaxis: if non-zero, only use cur x-axis,
 * otherwise use cur-yaxis (mostly this will be used for x).
 */
void UI_view2d_view_orthoSpecial(ARegion *region, View2D *v2d, bool xaxis);
/**
 * Restore view matrices after drawing.
 */
void UI_view2d_view_restore(const bContext *C);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grid Drawing
 * \{ */

/**
 * Draw a multi-level grid in given 2d-region.
 */
void UI_view2d_multi_grid_draw(
    const View2D *v2d, int colorid, float step, int level_size, int totlevels);
/**
 * Draw a multi-level grid of dots, with a dynamic number of levels based on the fading.
 *
 * \param grid_color_id: The theme color used for the points. Faded dynamically based on zoom.
 * \param min_step: The base size of the grid. At different zoom levels, the visible grid may have
 * a larger step size.
 * \param grid_subdivisions: The maximum number of sub-levels drawn at once.
 */
void UI_view2d_dot_grid_draw(const View2D *v2d,
                             int grid_color_id,
                             float min_step,
                             int grid_subdivisions);

/**
 * Draw horizontal lines.
 *
 * \param base: Defines in what step the lines are drawn.
 * Depending on the zoom level of the `v2d` the step is a full fraction of the given base.
 */
void UI_view2d_draw_lines_y__values(const View2D *v2d, int base);
void UI_view2d_draw_lines_x__values(const View2D *v2d, int base);
void UI_view2d_draw_lines_x__discrete_values(const View2D *v2d,
                                             int base,
                                             bool display_minor_lines);
void UI_view2d_draw_lines_x__discrete_time(const View2D *v2d, int base, bool display_minor_lines);
void UI_view2d_draw_lines_x__discrete_frames_or_seconds(const View2D *v2d,
                                                        const Scene *scene,
                                                        bool display_seconds,
                                                        bool display_minor_lines);
void UI_view2d_draw_lines_x__frames_or_seconds(const View2D *v2d,
                                               const Scene *scene,
                                               bool display_seconds);

float UI_view2d_grid_resolution_x__frames_or_seconds(const View2D *v2d, const Scene *scene);
float UI_view2d_grid_resolution_y__values(const View2D *v2d, int base);

/**
 * Scale indicator text drawing.
 */
void UI_view2d_draw_scale_y__values(
    const ARegion *region, const View2D *v2d, const rcti *rect, int colorid, int base);
/**
 * Draw a text scale in either frames or seconds. The minimum step distance is 1, meaning no
 * subframe indicators will be drawn.
 */
void UI_view2d_draw_scale_x__discrete_frames_or_seconds(const ARegion *region,
                                                        const View2D *v2d,
                                                        const rcti *rect,
                                                        const Scene *scene,
                                                        bool display_seconds,
                                                        int colorid,
                                                        int base);
/**
 * Draw a text scale in either frames or seconds.
 * This can draw indicators on subframes, e.g. "1.5".
 */
void UI_view2d_draw_scale_x__frames_or_seconds(const ARegion *region,
                                               const View2D *v2d,
                                               const rcti *rect,
                                               const Scene *scene,
                                               bool display_seconds,
                                               int colorid,
                                               int base);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scroll-bar Drawing
 * \{ */

/**
 * Draw scroll-bars in the given 2D-region.
 */
void UI_view2d_scrollers_draw(View2D *v2d, const rcti *mask_custom);

/** \} */

/* -------------------------------------------------------------------- */
/** \name List View Tools
 * \{ */

/**
 * Get the 'cell' (row, column) that the given 2D-view coordinates
 * (i.e. in 'tot' rect space) lie in.
 *
 * \param columnwidth, rowheight: size of each 'cell'
 * \param startx, starty: coordinates (in 'tot' rect space) that the list starts from.
 * This should be (0,0) for most views. However, for those where the starting row was offsetted
 * (like for Animation Editor channel lists, to make the first entry more visible), these will be
 * the min-coordinates of the first item.
 * \param viewx, viewy: 2D-coordinates (in 2D-view / 'tot' rect space) to get the cell for
 * \param r_column, r_row: The 'coordinates' of the relevant 'cell'.
 */
void UI_view2d_listview_view_to_cell(float columnwidth,
                                     float rowheight,
                                     float startx,
                                     float starty,
                                     float viewx,
                                     float viewy,
                                     int *r_column,
                                     int *r_row);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Coordinate Conversion
 * \{ */

float UI_view2d_region_to_view_x(const View2D *v2d, float x);
float UI_view2d_region_to_view_y(const View2D *v2d, float y);
/**
 * Convert from screen/region space to 2d-View space
 *
 * \param x, y: coordinates to convert
 * \param r_view_x, r_view_y: resultant coordinates
 */
void UI_view2d_region_to_view(
    const View2D *v2d, float x, float y, float *r_view_x, float *r_view_y) ATTR_NONNULL();
void UI_view2d_region_to_view_rctf(const View2D *v2d, const rctf *rect_src, rctf *rect_dst)
    ATTR_NONNULL();

float UI_view2d_view_to_region_x(const View2D *v2d, float x);
float UI_view2d_view_to_region_y(const View2D *v2d, float y);
/**
 * Convert from 2d-View space to screen/region space
 * \note Coordinates are clamped to lie within bounds of region
 *
 * \param x, y: Coordinates to convert.
 * \param r_region_x, r_region_y: Resultant coordinates.
 */
bool UI_view2d_view_to_region_clip(
    const View2D *v2d, float x, float y, int *r_region_x, int *r_region_y) ATTR_NONNULL();

bool UI_view2d_view_to_region_segment_clip(const View2D *v2d,
                                           const float xy_a[2],
                                           const float xy_b[2],
                                           int r_region_a[2],
                                           int r_region_b[2]) ATTR_NONNULL();

/**
 * Convert from 2d-view space to screen/region space
 *
 * \note Coordinates are NOT clamped to lie within bounds of region.
 *
 * \param x, y: Coordinates to convert.
 * \param r_region_x, r_region_y: Resultant coordinates.
 */
void UI_view2d_view_to_region(
    const View2D *v2d, float x, float y, int *r_region_x, int *r_region_y) ATTR_NONNULL();
void UI_view2d_view_to_region_fl(
    const View2D *v2d, float x, float y, float *r_region_x, float *r_region_y) ATTR_NONNULL();
void UI_view2d_view_to_region_m4(const View2D *v2d, float matrix[4][4]) ATTR_NONNULL();
void UI_view2d_view_to_region_rcti(const View2D *v2d, const rctf *rect_src, rcti *rect_dst)
    ATTR_NONNULL();
bool UI_view2d_view_to_region_rcti_clip(const View2D *v2d, const rctf *rect_src, rcti *rect_dst)
    ATTR_NONNULL();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * View2D data by default resides in region, so get from region stored in context.
 */
View2D *UI_view2d_fromcontext(const bContext *C);
/**
 * Same as #UI_view2d_fromcontext, but it returns region-window.
 * Utility for pull-downs or buttons.
 */
View2D *UI_view2d_fromcontext_rwin(const bContext *C);

/**
 * Get scroll-bar sizes of the current 2D view.
 * The size will be zero if the view has its scroll-bars disabled.
 *
 * \param mapped: whether to use view2d_scroll_mapped which changes flags
 */
void UI_view2d_scroller_size_get(const View2D *v2d, bool mapped, float *r_x, float *r_y);
/**
 * Calculate the scale per-axis of the drawing-area
 *
 * Is used to inverse correct drawing of icons, etc. that need to follow view
 * but not be affected by scale
 *
 * \param r_x, r_y: scale on each axis
 */
void UI_view2d_scale_get(const View2D *v2d, float *r_x, float *r_y);
float UI_view2d_scale_get_x(const View2D *v2d);
float UI_view2d_scale_get_y(const View2D *v2d);
/**
 * Same as `UI_view2d_scale_get() - 1.0f / x, y`.
 */
void UI_view2d_scale_get_inverse(const View2D *v2d, float *r_x, float *r_y);

/**
 * Simple functions for consistent center offset access.
 * Used by node editor to shift view center for each individual node tree.
 */
void UI_view2d_center_get(const View2D *v2d, float *r_x, float *r_y);
void UI_view2d_center_set(View2D *v2d, float x, float y);

/**
 * Simple pan function
 *  (0.0, 0.0) bottom left
 *  (0.5, 0.5) center
 *  (1.0, 1.0) top right.
 */
void UI_view2d_offset(View2D *v2d, float xfac, float yfac);

/**
 * Scrolls the view so that the upper edge is at a multiple of the page size.
 */
void UI_view2d_offset_y_snap_to_closest_page(View2D *v2d);

/**
 * Check if mouse is within scrollers
 *
 * \param xy: Mouse coordinates in screen (not region) space.
 * \param r_scroll: Return argument for the mapped view2d scroll flag.
 *
 * \return appropriate code for match.
 * - 'h' = in horizontal scroller.
 * - 'v' = in vertical scroller.
 * - 0 = not in scroller.
 */
char UI_view2d_mouse_in_scrollers_ex(const ARegion *region,
                                     const View2D *v2d,
                                     const int xy[2],
                                     int *r_scroll) ATTR_NONNULL(1, 2, 3, 4);
char UI_view2d_mouse_in_scrollers(const ARegion *region, const View2D *v2d, const int xy[2])
    ATTR_NONNULL(1, 2, 3);
char UI_view2d_rect_in_scrollers_ex(const ARegion *region,
                                    const View2D *v2d,
                                    const rcti *rect,
                                    int *r_scroll) ATTR_NONNULL(1, 2, 3);
char UI_view2d_rect_in_scrollers(const ARegion *region, const View2D *v2d, const rcti *rect)
    ATTR_NONNULL(1, 2, 3);

/**
 * Cached text drawing in v2d, to allow pixel-aligned draw as post process.
 */
void UI_view2d_text_cache_add(
    View2D *v2d, float x, float y, const char *str, size_t str_len, const unsigned char col[4]);
/**
 * No clip (yet).
 */
void UI_view2d_text_cache_add_rectf(View2D *v2d,
                                    const rctf *rect_view,
                                    const char *str,
                                    size_t str_len,
                                    const unsigned char col[4]);
void UI_view2d_text_cache_draw(ARegion *region);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operators
 * \{ */

void ED_operatortypes_view2d();
void ED_keymap_view2d(wmKeyConfig *keyconf);

/**
 * Will start timer if appropriate.
 * the arguments are the desired situation.
 */
void UI_view2d_smooth_view(const bContext *C, ARegion *region, const rctf *cur, int smooth_viewtx);

#define UI_MARKER_MARGIN_Y (42 * UI_SCALE_FAC)
#define UI_TIME_SCRUB_MARGIN_Y (23 * UI_SCALE_FAC)
#define UI_TIME_CACHE_MARGIN_Y (UI_TIME_SCRUB_MARGIN_Y / 6.0f)
#define UI_ANIM_MINY (HEADERY * UI_SCALE_FAC * 1.1f)
#define UI_MARKERS_MINY (HEADERY * UI_SCALE_FAC * 2.0f)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmo Types
 * \{ */

/* `view2d_gizmo_navigate.cc` */

/**
 * Caller defines the name for gizmo group.
 */
void VIEW2D_GGT_navigate_impl(wmGizmoGroupType *gzgt, const char *idname);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge Pan
 * \{ */

/**
 * Custom-data for view panning operators.
 */
struct View2DEdgePanData {
  /** Screen where view pan was initiated. */
  bScreen *screen;
  /** Area where view pan was initiated. */
  ScrArea *area;
  /** Region where view pan was initiated. */
  ARegion *region;
  /** View2d we're operating in. */
  View2D *v2d;
  /** Limit maximum pannable area. */
  struct rctf limit;

  /** Panning should only start once being in the inside rect once (e.g. adding nodes can happen
   * outside). */
  bool enabled;
  /** Inside distance in UI units from the edge of the region within which to start panning. */
  float inside_pad;
  /** Outside distance in UI units from the edge of the region at which to stop panning. */
  float outside_pad;
  /**
   * Width of the zone in UI units where speed increases with distance from the edge.
   * At the end of this zone max speed is reached.
   */
  float speed_ramp;
  /** Maximum speed in UI units per second. */
  float max_speed;
  /** Delay in seconds before maximum speed is reached. */
  float delay;
  /**
   * Influence factor for view zoom:
   * - 0 = Constant speed in UI units.
   * - 1 = Constant speed in view space, UI speed slows down when zooming out.
   */
  float zoom_influence;

  /** Initial view rect. */
  rctf initial_rect;

  /** Amount to move view relative to zoom. */
  float facx, facy;

  /* Timers. */
  double edge_pan_last_time;
  double edge_pan_start_time_x, edge_pan_start_time_y;
};

void UI_view2d_edge_pan_init(bContext *C,
                             View2DEdgePanData *vpd,
                             float inside_pad,
                             float outside_pad,
                             float speed_ramp,
                             float max_speed,
                             float delay,
                             float zoom_influence);

/**
 * Set area which can be panned
 */
void UI_view2d_edge_pan_set_limits(
    View2DEdgePanData *vpd, float xmin, float xmax, float ymin, float ymax);

void UI_view2d_edge_pan_reset(View2DEdgePanData *vpd);

/**
 * Apply transform to view (i.e. adjust 'cur' rect).
 */
void UI_view2d_edge_pan_apply(bContext *C, View2DEdgePanData *vpd, const int xy[2])
    ATTR_NONNULL(1, 2, 3);

/**
 * Apply transform to view using mouse events.
 */
void UI_view2d_edge_pan_apply_event(bContext *C, View2DEdgePanData *vpd, const wmEvent *event);

void UI_view2d_edge_pan_cancel(bContext *C, View2DEdgePanData *vpd);

void UI_view2d_edge_pan_operator_properties(wmOperatorType *ot);

void UI_view2d_edge_pan_operator_properties_ex(wmOperatorType *ot,
                                               float inside_pad,
                                               float outside_pad,
                                               float speed_ramp,
                                               float max_speed,
                                               float delay,
                                               float zoom_influence);

/**
 * Initialize panning data with operator settings.
 */
void UI_view2d_edge_pan_operator_init(bContext *C, View2DEdgePanData *vpd, wmOperator *op);

/** \} */
