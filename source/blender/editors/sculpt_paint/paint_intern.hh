/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_compiler_compat.h"
#include "BLI_function_ref.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "DNA_object_enums.h"
#include "DNA_scene_enums.h"
#include "DNA_vec_types.h"

enum class PaintMode : int8_t;

struct ARegion;
struct bContext;
struct Brush;
struct ColorManagedDisplay;
struct Depsgraph;
struct Image;
struct ImagePool;
struct ImageUser;
struct ImBuf;
struct Main;
struct MTex;
struct Object;
struct Paint;
struct PointerRNA;
struct RegionView3D;
struct ReportList;
struct Scene;
struct SculptSession;
struct SpaceImage;
struct ToolSettings;
struct VertProjHandle;
struct ViewContext;
struct VPaint;
struct wmEvent;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperator;
struct wmOperatorType;
namespace blender {
namespace bke {
namespace pbvh {
class Node;
}
}  // namespace bke
namespace ed::sculpt_paint {
struct PaintStroke;
struct StrokeCache;
}  // namespace ed::sculpt_paint
}  // namespace blender

/* paint_stroke.cc */

namespace blender::ed::sculpt_paint {

using StrokeGetLocation = bool (*)(bContext *C,
                                   float location[3],
                                   const float mouse[2],
                                   bool force_original);
using StrokeTestStart = bool (*)(bContext *C, wmOperator *op, const float mouse[2]);
using StrokeUpdateStep = void (*)(bContext *C,
                                  wmOperator *op,
                                  PaintStroke *stroke,
                                  PointerRNA *itemptr);
using StrokeRedraw = void (*)(const bContext *C, PaintStroke *stroke, bool final);
using StrokeDone = void (*)(const bContext *C, PaintStroke *stroke);

PaintStroke *paint_stroke_new(bContext *C,
                              wmOperator *op,
                              StrokeGetLocation get_location,
                              StrokeTestStart test_start,
                              StrokeUpdateStep update_step,
                              StrokeRedraw redraw,
                              StrokeDone done,
                              int event_type);
void paint_stroke_free(bContext *C, wmOperator *op, PaintStroke *stroke);

/**
 * Returns zero if the stroke dots should not be spaced, non-zero otherwise.
 */
bool paint_space_stroke_enabled(const Brush &br, PaintMode mode);
/**
 * Return true if the brush size can change during paint (normally used for pressure).
 */
bool paint_supports_dynamic_size(const Brush &br, PaintMode mode);
/**
 * Return true if the brush size can change during paint (normally used for pressure).
 */
bool paint_supports_dynamic_tex_coords(const Brush &br, PaintMode mode);
bool paint_supports_smooth_stroke(PaintStroke *stroke, const Brush &br, PaintMode mode);
bool paint_supports_texture(PaintMode mode);

/**
 * Called in paint_ops.cc, on each regeneration of key-maps.
 */
wmKeyMap *paint_stroke_modal_keymap(wmKeyConfig *keyconf);
int paint_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event, PaintStroke **stroke_p);
int paint_stroke_exec(bContext *C, wmOperator *op, PaintStroke *stroke);
void paint_stroke_cancel(bContext *C, wmOperator *op, PaintStroke *stroke);
bool paint_stroke_flipped(PaintStroke *stroke);
bool paint_stroke_inverted(PaintStroke *stroke);
ViewContext *paint_stroke_view_context(PaintStroke *stroke);
void *paint_stroke_mode_data(PaintStroke *stroke);
float paint_stroke_distance_get(PaintStroke *stroke);

class PaintModeData {
 public:
  virtual ~PaintModeData() = default;
};
void paint_stroke_set_mode_data(PaintStroke *stroke, std::unique_ptr<PaintModeData> mode_data);

bool paint_stroke_started(PaintStroke *stroke);
void paint_stroke_jitter_pos(Scene &scene,
                             const PaintStroke &stroke,
                             const PaintMode mode,
                             const Brush &brush,
                             const float pressure,
                             const float mval[2],
                             float r_mouse_out[2]);

/** Returns true if the active tool uses brushes. */
bool paint_brush_tool_poll(bContext *C);
/** Returns true if the brush cursor should be activated. */
bool paint_brush_cursor_poll(bContext *C);
/** Initialize the stroke cache variants from operator properties. */
bool paint_brush_update(bContext *C,
                        const Brush &brush,
                        PaintMode mode,
                        PaintStroke *stroke,
                        const float mouse_init[2],
                        float mouse[2],
                        float pressure,
                        float r_location[3],
                        bool *r_location_is_set);

void BRUSH_OT_asset_activate(wmOperatorType *ot);
void BRUSH_OT_asset_save_as(wmOperatorType *ot);
void BRUSH_OT_asset_edit_metadata(wmOperatorType *ot);
void BRUSH_OT_asset_load_preview(wmOperatorType *ot);
void BRUSH_OT_asset_delete(wmOperatorType *ot);
void BRUSH_OT_asset_save(wmOperatorType *ot);
void BRUSH_OT_asset_revert(wmOperatorType *ot);

}  // namespace blender::ed::sculpt_paint

/**
 * Delete overlay cursor textures to preserve memory and invalidate all overlay flags.
 */
void paint_cursor_delete_textures();

/* `paint_vertex.cc` */

bool weight_paint_poll(bContext *C);
bool weight_paint_poll_ignore_tool(bContext *C);
bool weight_paint_mode_poll(bContext *C);
bool weight_paint_mode_region_view3d_poll(bContext *C);
bool vertex_paint_poll(bContext *C);
bool vertex_paint_poll_ignore_tool(bContext *C);
/**
 * Returns true if vertex paint mode is active.
 */
bool vertex_paint_mode_poll(bContext *C);

using VPaintTransform_Callback = void (*)(const float col[3],
                                          const void *user_data,
                                          float r_col[3]);

void PAINT_OT_weight_paint_toggle(wmOperatorType *ot);
void PAINT_OT_weight_paint(wmOperatorType *ot);
void PAINT_OT_weight_set(wmOperatorType *ot);

enum {
  WPAINT_GRADIENT_TYPE_LINEAR,
  WPAINT_GRADIENT_TYPE_RADIAL,
};
void PAINT_OT_weight_gradient(wmOperatorType *ot);

void PAINT_OT_vertex_paint_toggle(wmOperatorType *ot);
void PAINT_OT_vertex_paint(wmOperatorType *ot);

/**
 * \note weight-paint has an equivalent function: #ED_wpaint_blend_tool
 */
unsigned int ED_vpaint_blend_tool(int tool, uint col, uint paintcol, int alpha_i);

/* `paint_vertex_weight_utils.cc` */

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
bool ED_wpaint_ensure_data(bContext *C,
                           ReportList *reports,
                           enum eWPaintFlag flag,
                           WPaintVGroupIndex *vgroup_index);
/** Return -1 when invalid. */
int ED_wpaint_mirror_vgroup_ensure(Object *ob, int vgroup_active);

/* `paint_vertex_color_ops.cc` */

void PAINT_OT_vertex_color_set(wmOperatorType *ot);
void PAINT_OT_vertex_color_from_weight(wmOperatorType *ot);
void PAINT_OT_vertex_color_smooth(wmOperatorType *ot);
void PAINT_OT_vertex_color_brightness_contrast(wmOperatorType *ot);
void PAINT_OT_vertex_color_hsv(wmOperatorType *ot);
void PAINT_OT_vertex_color_invert(wmOperatorType *ot);
void PAINT_OT_vertex_color_levels(wmOperatorType *ot);

/* `paint_vertex_weight_ops.cc` */

void PAINT_OT_weight_from_bones(wmOperatorType *ot);
void PAINT_OT_weight_sample(wmOperatorType *ot);
void PAINT_OT_weight_sample_group(wmOperatorType *ot);

/* `paint_vertex_proj.cc` */

VertProjHandle *ED_vpaint_proj_handle_create(Depsgraph &depsgraph,
                                             Scene &scene,
                                             Object &ob,
                                             blender::Span<blender::float3> &r_vert_positions,
                                             blender::Span<blender::float3> &r_vert_normals);
void ED_vpaint_proj_handle_update(Depsgraph *depsgraph,
                                  VertProjHandle *vp_handle,
                                  /* runtime vars */
                                  ARegion *region,
                                  const float mval_fl[2]);
void ED_vpaint_proj_handle_free(VertProjHandle *vp_handle);

/* `paint_image.cc` */

struct ImagePaintPartialRedraw {
  rcti dirty_region;
};

bool image_texture_paint_poll(bContext *C);
void imapaint_image_update(
    SpaceImage *sima, Image *image, ImBuf *ibuf, ImageUser *iuser, short texpaint);
ImagePaintPartialRedraw *get_imapaintpartial();
void set_imapaintpartial(ImagePaintPartialRedraw *ippr);
void imapaint_region_tiles(
    ImBuf *ibuf, int x, int y, int w, int h, int *tx, int *ty, int *tw, int *th);
bool get_imapaint_zoom(bContext *C, float *zoomx, float *zoomy);
void *paint_2d_new_stroke(bContext *, wmOperator *, int mode);
void paint_2d_redraw(const bContext *C, void *ps, bool final);
void paint_2d_stroke_done(void *ps);
void paint_2d_stroke(void *ps,
                     const float prev_mval[2],
                     const float mval[2],
                     bool eraser,
                     float pressure,
                     float distance,
                     float base_size);
/**
 * This function expects linear space color values.
 */
void paint_2d_bucket_fill(const bContext *C,
                          const float color[3],
                          Brush *br,
                          const float mouse_init[2],
                          const float mouse_final[2],
                          void *ps);
void paint_2d_gradient_fill(
    const bContext *C, Brush *br, const float mouse_init[2], const float mouse_final[2], void *ps);
void *paint_proj_new_stroke(bContext *C, Object *ob, const float mouse[2], int mode);
void paint_proj_stroke(const bContext *C,
                       void *ps_handle_p,
                       const float prev_pos[2],
                       const float pos[2],
                       bool eraser,
                       float pressure,
                       float distance,
                       float size);
void paint_proj_redraw(const bContext *C, void *ps_handle_p, bool final);
void paint_proj_stroke_done(void *ps_handle_p);

void paint_brush_color_get(Scene *scene,
                           Brush *br,
                           bool color_correction,
                           bool invert,
                           float distance,
                           float pressure,
                           ColorManagedDisplay *display,
                           float r_color[3]);
bool paint_use_opacity_masking(Brush *brush);
void paint_brush_init_tex(Brush *brush);
void paint_brush_exit_tex(Brush *brush);
bool image_paint_poll(bContext *C);

void PAINT_OT_grab_clone(wmOperatorType *ot);
void PAINT_OT_sample_color(wmOperatorType *ot);
void PAINT_OT_brush_colors_flip(wmOperatorType *ot);
void PAINT_OT_texture_paint_toggle(wmOperatorType *ot);
void PAINT_OT_project_image(wmOperatorType *ot);
void PAINT_OT_image_from_view(wmOperatorType *ot);
void PAINT_OT_add_texture_paint_slot(wmOperatorType *ot);
void PAINT_OT_image_paint(wmOperatorType *ot);
void PAINT_OT_add_simple_uvs(wmOperatorType *ot);

/* paint_image_2d_curve_mask.cc */

/**
 * \brief Caching structure for curve mask.
 *
 * When 2d painting images the curve mask is used as an input.
 */
struct CurveMaskCache {
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
};

void paint_curve_mask_cache_free_data(CurveMaskCache *curve_mask_cache);
void paint_curve_mask_cache_update(CurveMaskCache *curve_mask_cache,
                                   const Brush *brush,
                                   int diameter,
                                   float radius,
                                   const float cursor_position[2]);

/* `sculpt_uv.cc` */

void SCULPT_OT_uv_sculpt_grab(wmOperatorType *ot);
void SCULPT_OT_uv_sculpt_relax(wmOperatorType *ot);
void SCULPT_OT_uv_sculpt_pinch(wmOperatorType *ot);

/* paint_utils.cc */

/**
 * Convert the object-space axis-aligned bounding box (expressed as
 * its minimum and maximum corners) into a screen-space rectangle,
 * returns zero if the result is empty.
 */
bool paint_convert_bb_to_rect(rcti *rect,
                              const float bb_min[3],
                              const float bb_max[3],
                              const ARegion &region,
                              const RegionView3D &rv3d,
                              const Object &ob);

/**
 * Get four planes in object-space that describe the projection of
 * screen_rect from screen into object-space (essentially converting a
 * 2D screens-space bounding box into four 3D planes).
 */
void paint_calc_redraw_planes(float planes[4][4],
                              const ARegion &region,
                              const Object &ob,
                              const rcti &screen_rect);

float paint_calc_object_space_radius(const ViewContext &vc,
                                     const blender::float3 &center,
                                     float pixel_radius);

/**
 * Returns true when a color was sampled and false when a value was sampled.
 */
bool paint_get_tex_pixel(const MTex *mtex,
                         float u,
                         float v,
                         ImagePool *pool,
                         int thread,
                         float *r_intensity,
                         float r_rgba[4]);

/**
 * Used for both 3D view and image window.
 */
void paint_sample_color(
    bContext *C, ARegion *region, int x, int y, bool texpaint_proj, bool palette);

void paint_stroke_operator_properties(wmOperatorType *ot);

void BRUSH_OT_curve_preset(wmOperatorType *ot);
void BRUSH_OT_sculpt_curves_falloff_preset(wmOperatorType *ot);

void PAINT_OT_face_select_linked(wmOperatorType *ot);
void PAINT_OT_face_select_linked_pick(wmOperatorType *ot);
void PAINT_OT_face_select_all(wmOperatorType *ot);
void PAINT_OT_face_select_more(wmOperatorType *ot);
void PAINT_OT_face_select_less(wmOperatorType *ot);
void PAINT_OT_face_select_hide(wmOperatorType *ot);
void PAINT_OT_face_select_loop(wmOperatorType *ot);

void PAINT_OT_face_vert_reveal(wmOperatorType *ot);

void PAINT_OT_vert_select_all(wmOperatorType *ot);
void PAINT_OT_vert_select_ungrouped(wmOperatorType *ot);
void PAINT_OT_vert_select_hide(wmOperatorType *ot);
void PAINT_OT_vert_select_linked(wmOperatorType *ot);
void PAINT_OT_vert_select_linked_pick(wmOperatorType *ot);
void PAINT_OT_vert_select_more(wmOperatorType *ot);
void PAINT_OT_vert_select_less(wmOperatorType *ot);

bool vert_paint_poll(bContext *C);
bool mask_paint_poll(bContext *C);
bool paint_curve_poll(bContext *C);

bool facemask_paint_poll(bContext *C);

namespace blender::ed::sculpt_paint {

inline float3 symmetry_flip(const float3 &src, const ePaintSymmetryFlags symm)
{
  float3 dst;
  if (symm & PAINT_SYMM_X) {
    dst.x = -src.x;
  }
  else {
    dst.x = src.x;
  }
  if (symm & PAINT_SYMM_Y) {
    dst.y = -src.y;
  }
  else {
    dst.y = src.y;
  }
  if (symm & PAINT_SYMM_Z) {
    dst.z = -src.z;
  }
  else {
    dst.z = src.z;
  }
  return dst;
}

}  // namespace blender::ed::sculpt_paint

/* stroke operator */
enum BrushStrokeMode {
  BRUSH_STROKE_NORMAL,
  BRUSH_STROKE_INVERT,
  BRUSH_STROKE_SMOOTH,
  BRUSH_STROKE_ERASE,
};

/* `paint_curve.cc` */

void PAINTCURVE_OT_new(wmOperatorType *ot);
void PAINTCURVE_OT_add_point(wmOperatorType *ot);
void PAINTCURVE_OT_delete_point(wmOperatorType *ot);
void PAINTCURVE_OT_select(wmOperatorType *ot);
void PAINTCURVE_OT_slide(wmOperatorType *ot);
void PAINTCURVE_OT_draw(wmOperatorType *ot);
void PAINTCURVE_OT_cursor(wmOperatorType *ot);

/* image painting blur kernel */
struct BlurKernel {
  float *wdata;     /* actual kernel */
  int side;         /* kernel side */
  int side_squared; /* data side */
  int pixel_len;    /* pixels around center that kernel is wide */
};

/**
 * Paint blur kernels. Projective painting enforces use of a 2x2 kernel due to lagging.
 * Can be extended to other blur kernels later,
 */
BlurKernel *paint_new_blur_kernel(Brush *br, bool proj);
void paint_delete_blur_kernel(BlurKernel *);

/** Initialize viewport pivot from evaluated bounding box center of `ob`. */
void paint_init_pivot(Object *ob, Scene *scene);

/* paint curve defines */
#define PAINT_CURVE_NUM_SEGMENTS 40

namespace blender::ed::sculpt_paint::vwpaint {
struct NormalAnglePrecalc {
  bool do_mask_normal;
  /* what angle to mask at */
  float angle;
  /* cos(angle), faster to compare */
  float angle__cos;
  float angle_inner;
  float angle_inner__cos;
  /* difference between angle and angle_inner, for easy access */
  float angle_range;
};

void view_angle_limits_init(NormalAnglePrecalc *a, float angle, bool do_mask_normal);
float view_angle_limits_apply_falloff(const NormalAnglePrecalc *a, float angle_cos, float *mask_p);
bool test_brush_angle_falloff(const Brush &brush,
                              const NormalAnglePrecalc &normal_angle_precalc,
                              const float angle_cos,
                              float *brush_strength);
bool use_normal(const VPaint &vp);

bool brush_use_accumulate_ex(const Brush &brush, eObjectMode ob_mode);
bool brush_use_accumulate(const VPaint &vp);

void get_brush_alpha_data(const Scene &scene,
                          const SculptSession &ss,
                          const Brush &brush,
                          float *r_brush_size_pressure,
                          float *r_brush_alpha_value,
                          float *r_brush_alpha_pressure);

void init_stroke(Depsgraph &depsgraph, Object &ob);
void init_session_data(const ToolSettings &ts, Object &ob);
/** Toggle operator for turning vertex paint mode on or off (copied from `sculpt.cc`) */
void init_session(
    Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob, eObjectMode object_mode);

IndexMask pbvh_gather_generic(const Depsgraph &depsgraph,
                              const Object &ob,
                              const VPaint &wp,
                              const Brush &brush,
                              IndexMaskMemory &memory);

void mode_enter_generic(
    Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob, eObjectMode mode_flag);
void mode_exit_generic(Object &ob, eObjectMode mode_flag);
bool mode_toggle_poll_test(bContext *C);

void smooth_brush_toggle_off(const bContext *C, Paint *paint, StrokeCache *cache);
void smooth_brush_toggle_on(const bContext *C, Paint *paint, StrokeCache *cache);

/** Initialize the stroke cache variants from operator properties. */
void update_cache_variants(bContext *C, VPaint &vp, Object &ob, PointerRNA *ptr);
/** Initialize the stroke cache invariants from operator properties. */
void update_cache_invariants(
    bContext *C, VPaint &vp, SculptSession &ss, wmOperator *op, const float mval[2]);
void last_stroke_update(Scene &scene, const float location[3]);
}  // namespace blender::ed::sculpt_paint::vwpaint
