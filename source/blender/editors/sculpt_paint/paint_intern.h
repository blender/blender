/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BKE_paint.h"

#include "BLI_compiler_compat.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "ED_select_utils.h"

#include "DNA_scene_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct Brush;
struct ColorManagedDisplay;
struct ColorSpace;
struct ImagePool;
struct MTex;
struct Object;
struct Paint;
struct PaintStroke;
struct PointerRNA;
struct RegionView3D;
struct Scene;
struct VPaint;
struct ViewContext;
struct bContext;
struct wmEvent;
struct wmKeyConfig;
struct wmOperator;
struct wmOperatorType;

typedef struct CoNo {
  float co[3];
  float no[3];
} CoNo;

/* paint_stroke.c */

typedef bool (*StrokeGetLocation)(struct bContext *C,
                                  float location[3],
                                  const float mouse[2],
                                  bool force_original);
typedef bool (*StrokeTestStart)(struct bContext *C, struct wmOperator *op, const float mouse[2]);
typedef void (*StrokeUpdateStep)(struct bContext *C,
                                 struct wmOperator *op,
                                 struct PaintStroke *stroke,
                                 struct PointerRNA *itemptr);
typedef void (*StrokeRedraw)(const struct bContext *C, struct PaintStroke *stroke, bool final);
typedef void (*StrokeDone)(const struct bContext *C, struct PaintStroke *stroke);

struct PaintStroke *paint_stroke_new(struct bContext *C,
                                     struct wmOperator *op,
                                     StrokeGetLocation get_location,
                                     StrokeTestStart test_start,
                                     StrokeUpdateStep update_step,
                                     StrokeRedraw redraw,
                                     StrokeDone done,
                                     int event_type);
void paint_stroke_free(struct bContext *C, struct wmOperator *op, struct PaintStroke *stroke);

/**
 * Returns zero if the stroke dots should not be spaced, non-zero otherwise.
 */
bool paint_space_stroke_enabled(struct Brush *br, enum ePaintMode mode);
/**
 * Return true if the brush size can change during paint (normally used for pressure).
 */
bool paint_supports_dynamic_size(struct Brush *br, enum ePaintMode mode);
/**
 * Return true if the brush size can change during paint (normally used for pressure).
 */
bool paint_supports_dynamic_tex_coords(struct Brush *br, enum ePaintMode mode);
bool paint_supports_smooth_stroke(struct Brush *br, enum ePaintMode mode);
bool paint_supports_texture(enum ePaintMode mode);
bool paint_supports_jitter(enum ePaintMode mode);

/**
 * Called in paint_ops.c, on each regeneration of key-maps.
 */
struct wmKeyMap *paint_stroke_modal_keymap(struct wmKeyConfig *keyconf);
int paint_stroke_modal(struct bContext *C,
                       struct wmOperator *op,
                       const struct wmEvent *event,
                       struct PaintStroke **stroke_p);
int paint_stroke_exec(struct bContext *C, struct wmOperator *op, struct PaintStroke *stroke);
void paint_stroke_cancel(struct bContext *C, struct wmOperator *op, struct PaintStroke *stroke);
bool paint_stroke_flipped(struct PaintStroke *stroke);
bool paint_stroke_inverted(struct PaintStroke *stroke);
struct ViewContext *paint_stroke_view_context(struct PaintStroke *stroke);
void *paint_stroke_mode_data(struct PaintStroke *stroke);
float paint_stroke_distance_get(struct PaintStroke *stroke);
void paint_stroke_set_mode_data(struct PaintStroke *stroke, void *mode_data);
bool paint_stroke_started(struct PaintStroke *stroke);

bool PAINT_brush_tool_poll(struct bContext *C);
/**
 * Delete overlay cursor textures to preserve memory and invalidate all overlay flags.
 */
void paint_cursor_delete_textures(void);

/* paint_vertex.c */

bool weight_paint_poll(struct bContext *C);
bool weight_paint_poll_ignore_tool(bContext *C);
bool weight_paint_mode_poll(struct bContext *C);
bool vertex_paint_poll(struct bContext *C);
bool vertex_paint_poll_ignore_tool(struct bContext *C);
/**
 * Returns true if vertex paint mode is active.
 */
bool vertex_paint_mode_poll(struct bContext *C);

typedef void (*VPaintTransform_Callback)(const float col[3],
                                         const void *user_data,
                                         float r_col[3]);

void PAINT_OT_weight_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_weight_paint(struct wmOperatorType *ot);
void PAINT_OT_weight_set(struct wmOperatorType *ot);

enum {
  WPAINT_GRADIENT_TYPE_LINEAR,
  WPAINT_GRADIENT_TYPE_RADIAL,
};
void PAINT_OT_weight_gradient(struct wmOperatorType *ot);

void PAINT_OT_vertex_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_vertex_paint(struct wmOperatorType *ot);

unsigned int vpaint_get_current_color(struct Scene *scene, struct VPaint *vp, bool secondary);

/**
 * \note weight-paint has an equivalent function: #ED_wpaint_blend_tool
 */
unsigned int ED_vpaint_blend_tool(int tool, uint col, uint paintcol, int alpha_i);

/* paint_vertex_weight_utils.c */

/**
 * \param weight: Typically the current weight: #MDeformWeight.weight
 *
 * \return The final weight, note that this is _not_ clamped from [0-1].
 * Clamping must be done on the final #MDeformWeight.weight
 *
 * \note vertex-paint has an equivalent function: #ED_vpaint_blend_tool
 */
float ED_wpaint_blend_tool(int tool, float weight, float paintval, float alpha);
/* Utility for tools to ensure vertex groups exist before they begin. */
enum eWPaintFlag {
  WPAINT_ENSURE_MIRROR = (1 << 0),
};
struct WPaintVGroupIndex {
  int active;
  int mirror;
};
/**
 * Ensure we have data on wpaint start, add if needed.
 */
bool ED_wpaint_ensure_data(struct bContext *C,
                           struct ReportList *reports,
                           enum eWPaintFlag flag,
                           struct WPaintVGroupIndex *vgroup_index);
/** Return -1 when invalid. */
int ED_wpaint_mirror_vgroup_ensure(struct Object *ob, int vgroup_active);

/* paint_vertex_color_ops.c */

void PAINT_OT_vertex_color_set(struct wmOperatorType *ot);
void PAINT_OT_vertex_color_from_weight(struct wmOperatorType *ot);
void PAINT_OT_vertex_color_smooth(struct wmOperatorType *ot);
void PAINT_OT_vertex_color_brightness_contrast(struct wmOperatorType *ot);
void PAINT_OT_vertex_color_hsv(struct wmOperatorType *ot);
void PAINT_OT_vertex_color_invert(struct wmOperatorType *ot);
void PAINT_OT_vertex_color_levels(struct wmOperatorType *ot);

/* paint_vertex_weight_ops.c */

void PAINT_OT_weight_from_bones(struct wmOperatorType *ot);
void PAINT_OT_weight_sample(struct wmOperatorType *ot);
void PAINT_OT_weight_sample_group(struct wmOperatorType *ot);

/* paint_vertex_proj.c */

struct VertProjHandle;
struct VertProjHandle *ED_vpaint_proj_handle_create(struct Depsgraph *depsgraph,
                                                    struct Scene *scene,
                                                    struct Object *ob,
                                                    struct CoNo **r_vcosnos);
void ED_vpaint_proj_handle_update(struct Depsgraph *depsgraph,
                                  struct VertProjHandle *vp_handle,
                                  /* runtime vars */
                                  struct ARegion *region,
                                  const float mval_fl[2]);
void ED_vpaint_proj_handle_free(struct VertProjHandle *vp_handle);

/* paint_image.c */

typedef struct ImagePaintPartialRedraw {
  rcti dirty_region;
} ImagePaintPartialRedraw;

bool image_texture_paint_poll(struct bContext *C);
void imapaint_image_update(struct SpaceImage *sima,
                           struct Image *image,
                           struct ImBuf *ibuf,
                           struct ImageUser *iuser,
                           short texpaint);
struct ImagePaintPartialRedraw *get_imapaintpartial(void);
void set_imapaintpartial(struct ImagePaintPartialRedraw *ippr);
void imapaint_region_tiles(
    struct ImBuf *ibuf, int x, int y, int w, int h, int *tx, int *ty, int *tw, int *th);
bool get_imapaint_zoom(struct bContext *C, float *zoomx, float *zoomy);
void *paint_2d_new_stroke(struct bContext *, struct wmOperator *, int mode);
void paint_2d_redraw(const struct bContext *C, void *ps, bool final);
void paint_2d_stroke_done(void *ps);
void paint_2d_stroke(void *ps,
                     const float prev_mval[2],
                     const float mval[2],
                     bool eraser,
                     float pressure,
                     float distance,
                     float size);
/**
 * This function expects linear space color values.
 */
void paint_2d_bucket_fill(const struct bContext *C,
                          const float color[3],
                          struct Brush *br,
                          const float mouse_init[2],
                          const float mouse_final[2],
                          void *ps);
void paint_2d_gradient_fill(const struct bContext *C,
                            struct Brush *br,
                            const float mouse_init[2],
                            const float mouse_final[2],
                            void *ps);
void *paint_proj_new_stroke(struct bContext *C, struct Object *ob, const float mouse[2], int mode);
void paint_proj_stroke(const struct bContext *C,
                       void *ps_handle_p,
                       const float prev_pos[2],
                       const float pos[2],
                       bool eraser,
                       float pressure,
                       float distance,
                       float size);
void paint_proj_redraw(const struct bContext *C, void *ps_handle_p, bool final);
void paint_proj_stroke_done(void *ps_handle_p);

void paint_brush_color_get(struct Scene *scene,
                           struct Brush *br,
                           bool color_correction,
                           bool invert,
                           float distance,
                           float pressure,
                           float color[3],
                           struct ColorManagedDisplay *display);
bool paint_use_opacity_masking(struct Brush *brush);
void paint_brush_init_tex(struct Brush *brush);
void paint_brush_exit_tex(struct Brush *brush);
bool image_paint_poll(struct bContext *C);

void PAINT_OT_grab_clone(struct wmOperatorType *ot);
void PAINT_OT_sample_color(struct wmOperatorType *ot);
void PAINT_OT_brush_colors_flip(struct wmOperatorType *ot);
void PAINT_OT_texture_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_project_image(struct wmOperatorType *ot);
void PAINT_OT_image_from_view(struct wmOperatorType *ot);
void PAINT_OT_add_texture_paint_slot(struct wmOperatorType *ot);
void PAINT_OT_image_paint(struct wmOperatorType *ot);
void PAINT_OT_add_simple_uvs(struct wmOperatorType *ot);

/* paint_image_2d_curve_mask.cc */

/**
 * \brief Caching structure for curve mask.
 *
 * When 2d painting images the curve mask is used as an input.
 */
typedef struct CurveMaskCache {
  /**
   * \brief Last #CurveMapping.changed_timestamp being read.
   *
   * When different the input cache needs to be recalculated.
   */
  int last_curve_timestamp;

  /**
   * \brief sampled version of the brush curve-mapping.
   */
  float *sampled_curve;

  /**
   * \brief Size in bytes of the curve_mask field.
   *
   * Used to determine if the curve_mask needs to be re-allocated.
   */
  size_t curve_mask_size;

  /**
   * \brief Curve mask that can be passed as curve_mask parameter when.
   */
  ushort *curve_mask;
} CurveMaskCache;

void paint_curve_mask_cache_free_data(CurveMaskCache *curve_mask_cache);
void paint_curve_mask_cache_update(CurveMaskCache *curve_mask_cache,
                                   const struct Brush *brush,
                                   int diameter,
                                   float radius,
                                   const float cursor_position[2]);

/* sculpt_uv.c */

void SCULPT_OT_uv_sculpt_stroke(struct wmOperatorType *ot);

/* paint_utils.c */

/**
 * Convert the object-space axis-aligned bounding box (expressed as
 * its minimum and maximum corners) into a screen-space rectangle,
 * returns zero if the result is empty.
 */
bool paint_convert_bb_to_rect(struct rcti *rect,
                              const float bb_min[3],
                              const float bb_max[3],
                              const struct ARegion *region,
                              struct RegionView3D *rv3d,
                              struct Object *ob);

/**
 * Get four planes in object-space that describe the projection of
 * screen_rect from screen into object-space (essentially converting a
 * 2D screens-space bounding box into four 3D planes).
 */
void paint_calc_redraw_planes(float planes[4][4],
                              const struct ARegion *region,
                              struct Object *ob,
                              const struct rcti *screen_rect);

float paint_calc_object_space_radius(struct ViewContext *vc,
                                     const float center[3],
                                     float pixel_radius);
float paint_get_tex_pixel(
    const struct MTex *mtex, float u, float v, struct ImagePool *pool, int thread);
void paint_get_tex_pixel_col(const struct MTex *mtex,
                             float u,
                             float v,
                             float rgba[4],
                             struct ImagePool *pool,
                             int thread,
                             bool convert,
                             struct ColorSpace *colorspace);

/**
 * Used for both 3D view and image window.
 */
void paint_sample_color(
    struct bContext *C, struct ARegion *region, int x, int y, bool texpaint_proj, bool palette);

void paint_stroke_operator_properties(struct wmOperatorType *ot);

void BRUSH_OT_curve_preset(struct wmOperatorType *ot);

void PAINT_OT_face_select_linked(struct wmOperatorType *ot);
void PAINT_OT_face_select_linked_pick(struct wmOperatorType *ot);
void PAINT_OT_face_select_all(struct wmOperatorType *ot);
void PAINT_OT_face_select_hide(struct wmOperatorType *ot);

void PAINT_OT_face_vert_reveal(struct wmOperatorType *ot);

void PAINT_OT_vert_select_all(struct wmOperatorType *ot);
void PAINT_OT_vert_select_ungrouped(struct wmOperatorType *ot);
void PAINT_OT_vert_select_hide(struct wmOperatorType *ot);

bool vert_paint_poll(struct bContext *C);
bool mask_paint_poll(struct bContext *C);
bool paint_curve_poll(struct bContext *C);

bool facemask_paint_poll(struct bContext *C);
/**
 * Uses symm to selectively flip any axis of a coordinate.
 */

BLI_INLINE void flip_v3_v3(float out[3], const float in[3], const ePaintSymmetryFlags symm)
{
  if (symm & PAINT_SYMM_X) {
    out[0] = -in[0];
  }
  else {
    out[0] = in[0];
  }
  if (symm & PAINT_SYMM_Y) {
    out[1] = -in[1];
  }
  else {
    out[1] = in[1];
  }
  if (symm & PAINT_SYMM_Z) {
    out[2] = -in[2];
  }
  else {
    out[2] = in[2];
  }
}

BLI_INLINE void flip_qt_qt(float out[4], const float in[4], const ePaintSymmetryFlags symm)
{
  float axis[3], angle;

  quat_to_axis_angle(axis, &angle, in);
  normalize_v3(axis);

  if (symm & PAINT_SYMM_X) {
    axis[0] *= -1.0f;
    angle *= -1.0f;
  }
  if (symm & PAINT_SYMM_Y) {
    axis[1] *= -1.0f;
    angle *= -1.0f;
  }
  if (symm & PAINT_SYMM_Z) {
    axis[2] *= -1.0f;
    angle *= -1.0f;
  }

  axis_angle_normalized_to_quat(out, axis, angle);
}

BLI_INLINE void flip_v3(float v[3], const ePaintSymmetryFlags symm)
{
  flip_v3_v3(v, v, symm);
}

BLI_INLINE void flip_qt(float quat[4], const ePaintSymmetryFlags symm)
{
  flip_qt_qt(quat, quat, symm);
}

/* stroke operator */
typedef enum BrushStrokeMode {
  BRUSH_STROKE_NORMAL,
  BRUSH_STROKE_INVERT,
  BRUSH_STROKE_SMOOTH,
} BrushStrokeMode;

/* paint_hide.c */

typedef enum {
  PARTIALVIS_HIDE,
  PARTIALVIS_SHOW,
} PartialVisAction;

typedef enum {
  PARTIALVIS_INSIDE,
  PARTIALVIS_OUTSIDE,
  PARTIALVIS_ALL,
  PARTIALVIS_MASKED,
} PartialVisArea;

void PAINT_OT_hide_show(struct wmOperatorType *ot);

/* paint_mask.c */

/* The gesture API doesn't write to this enum type,
 * it writes to eSelectOp from ED_select_utils.h.
 * We must thus map the modes here to the desired
 * eSelectOp modes.
 *
 * Fixes T102349.
 */
typedef enum {
  PAINT_MASK_FLOOD_VALUE = SEL_OP_SUB,
  PAINT_MASK_FLOOD_VALUE_INVERSE = SEL_OP_ADD,
  PAINT_MASK_INVERT = SEL_OP_XOR,
} PaintMaskFloodMode;

void PAINT_OT_mask_flood_fill(struct wmOperatorType *ot);
void PAINT_OT_mask_lasso_gesture(struct wmOperatorType *ot);
void PAINT_OT_mask_box_gesture(struct wmOperatorType *ot);
void PAINT_OT_mask_line_gesture(struct wmOperatorType *ot);

/* paint_curve.c */

void PAINTCURVE_OT_new(struct wmOperatorType *ot);
void PAINTCURVE_OT_add_point(struct wmOperatorType *ot);
void PAINTCURVE_OT_delete_point(struct wmOperatorType *ot);
void PAINTCURVE_OT_select(struct wmOperatorType *ot);
void PAINTCURVE_OT_slide(struct wmOperatorType *ot);
void PAINTCURVE_OT_draw(struct wmOperatorType *ot);
void PAINTCURVE_OT_cursor(struct wmOperatorType *ot);

/* image painting blur kernel */
typedef struct {
  float *wdata;     /* actual kernel */
  int side;         /* kernel side */
  int side_squared; /* data side */
  int pixel_len;    /* pixels around center that kernel is wide */
} BlurKernel;

enum eBlurKernelType;
/**
 * Paint blur kernels. Projective painting enforces use of a 2x2 kernel due to lagging.
 * Can be extended to other blur kernels later,
 */
BlurKernel *paint_new_blur_kernel(struct Brush *br, bool proj);
void paint_delete_blur_kernel(BlurKernel *);

/** Initialize viewport pivot from evaluated bounding box center of `ob`. */
void paint_init_pivot(struct Object *ob, struct Scene *scene);

/* paint curve defines */
#define PAINT_CURVE_NUM_SEGMENTS 40

#ifdef __cplusplus
}
#endif
