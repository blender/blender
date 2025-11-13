/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 *
 * Used for vertex color & weight paint and mode switching.
 *
 * \note This file is already big,
 * use `paint_vertex_color_ops.cc` & `paint_vertex_weight_ops.cc` for general purpose operators.
 */

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "BLI_color.hh"
#include "BLI_color_mix.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_library.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "ED_image.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_object_vgroup.hh"
#include "ED_paint.hh"
#include "ED_screen.hh"
#include "ED_sculpt.hh"
#include "ED_view3d.hh"

/* For IMB_BlendMode only. */
#include "IMB_imbuf.hh"

#include "bmesh.hh"

#include "mesh_brush_common.hh"
#include "paint_intern.hh" /* own include */
#include "sculpt_automask.hh"
#include "sculpt_intern.hh"
#include "sculpt_pose.hh"

using blender::IndexRange;
using blender::bke::AttrDomain;
using namespace blender;
using namespace blender::color;
using namespace blender::ed::sculpt_paint; /* For vwpaint namespace. */
using blender::ed::sculpt_paint::vwpaint::NormalAnglePrecalc;

static CLG_LogRef LOG = {"paint.vertex"};

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

static bool isZero(ColorPaint4f c)
{
  return c.r == 0.0f && c.g == 0.0f && c.b == 0.0f && c.a == 0.0f;
}

static bool isZero(ColorPaint4b c)
{
  return c.r == 0 && c.g == 0 && c.b == 0 && c.a == 0;
}

template<typename Color> static ColorPaint4f toFloat(const Color &c)
{
  if constexpr (std::is_same_v<Color, ColorPaint4b>) {
    return blender::color::decode(c);
  }
  else {
    return c;
  }
}

template<typename Color> static Color fromFloat(const ColorPaint4f &c)
{
  if constexpr (std::is_same_v<Color, ColorPaint4b>) {
    return blender::color::encode(c);
  }
  else {
    return c;
  }
}

/* Use for 'blur' brush, align with pbvh::Tree nodes, created and freed on each update. */
template<typename BlendType> struct VPaintAverageAccum {
  BlendType len;
  BlendType value[3];
};

namespace blender::ed::sculpt_paint::vwpaint {

/* -------------------------------------------------------------------- */
/** \name Shared vertex/weight paint code.
 * \{ */

void view_angle_limits_init(NormalAnglePrecalc *a, float angle, bool do_mask_normal)
{
  angle = RAD2DEGF(angle);
  a->do_mask_normal = do_mask_normal;
  if (do_mask_normal) {
    a->angle_inner = angle;
    a->angle = (a->angle_inner + 90.0f) * 0.5f;
  }
  else {
    a->angle_inner = a->angle = angle;
  }

  a->angle_inner *= float(M_PI_2 / 90);
  a->angle *= float(M_PI_2 / 90);
  a->angle_range = a->angle - a->angle_inner;

  if (a->angle_range <= 0.0f) {
    a->do_mask_normal = false; /* no need to do blending */
  }

  a->angle__cos = cosf(a->angle);
  a->angle_inner__cos = cosf(a->angle_inner);
}

float view_angle_limits_apply_falloff(const NormalAnglePrecalc *a, float angle_cos, float *mask_p)
{
  if (angle_cos <= a->angle__cos) {
    /* outsize the normal limit */
    return false;
  }
  if (angle_cos < a->angle_inner__cos) {
    *mask_p *= (a->angle - acosf(angle_cos)) / a->angle_range;
    return true;
  }
  return true;
}

bool test_brush_angle_falloff(const Brush &brush,
                              const NormalAnglePrecalc &normal_angle_precalc,
                              const float angle_cos,
                              float *brush_strength)
{
  if (((brush.flag & BRUSH_FRONTFACE) == 0 || (angle_cos > 0.0f)) &&
      ((brush.flag & BRUSH_FRONTFACE_FALLOFF) == 0 ||
       vwpaint::view_angle_limits_apply_falloff(&normal_angle_precalc, angle_cos, brush_strength)))
  {
    return true;
  }
  return false;
}

bool use_normal(const VPaint &vp)
{
  const Brush &brush = *BKE_paint_brush_for_read(&vp.paint);
  return ((brush.flag & BRUSH_FRONTFACE) != 0) || ((brush.flag & BRUSH_FRONTFACE_FALLOFF) != 0);
}

bool brush_use_accumulate_ex(const Brush &brush, const eObjectMode ob_mode)
{
  return ((brush.flag & BRUSH_ACCUMULATE) != 0 ||
          (ob_mode == OB_MODE_VERTEX_PAINT ?
               (brush.vertex_brush_type == VPAINT_BRUSH_TYPE_SMEAR) :
               (brush.weight_brush_type == WPAINT_BRUSH_TYPE_SMEAR)));
}

bool brush_use_accumulate(const VPaint &vp)
{
  const Brush *brush = BKE_paint_brush_for_read(&vp.paint);
  return brush_use_accumulate_ex(*brush, eObjectMode(vp.paint.runtime->ob_mode));
}

void init_stroke(Depsgraph &depsgraph, Object &ob)
{
  BKE_sculpt_update_object_for_edit(&depsgraph, &ob, true);
  SculptSession &ss = *ob.sculpt;

  /* Ensure ss.cache is allocated.  It will mostly be initialized in
   * vwpaint::update_cache_invariants and vwpaint::update_cache_variants.
   */
  if (!ss.cache) {
    ss.cache = MEM_new<StrokeCache>(__func__);
  }
}

void init_session(Main &bmain,
                  Depsgraph &depsgraph,
                  Scene &scene,
                  Paint &paint,
                  Object &ob,
                  eObjectMode object_mode)
{
  /* Create persistent sculpt mode data */
  BKE_sculpt_toolsettings_data_ensure(&bmain, &scene);

  BLI_assert(ob.sculpt == nullptr);
  ob.sculpt = MEM_new<SculptSession>(__func__);
  ob.sculpt->mode_type = object_mode;
  BKE_sculpt_update_object_for_edit(&depsgraph, &ob, true);

  ensure_valid_pivot(ob, paint);
}

void init_session_data(const ToolSettings &ts, Object &ob)
{
  /* Create maps */
  if (ob.mode == OB_MODE_VERTEX_PAINT) {
    BLI_assert(ob.sculpt->mode_type == OB_MODE_VERTEX_PAINT);
  }
  else if (ob.mode == OB_MODE_WEIGHT_PAINT) {
    BLI_assert(ob.sculpt->mode_type == OB_MODE_WEIGHT_PAINT);
  }
  else {
    ob.sculpt->mode_type = (eObjectMode)0;
    BLI_assert(0);
    return;
  }

  Mesh *mesh = (Mesh *)ob.data;

  /* Create average brush arrays */
  if (ob.mode == OB_MODE_WEIGHT_PAINT) {
    SculptSession &ss = *ob.sculpt;
    if (!vwpaint::brush_use_accumulate(*ts.wpaint)) {
      if (ss.mode.wpaint.alpha_weight == nullptr) {
        ss.mode.wpaint.alpha_weight = MEM_calloc_arrayN<float>(mesh->verts_num, __func__);
      }
      if (ss.mode.wpaint.dvert_prev.is_empty()) {
        MDeformVert initial_value{};
        /* Use to show this isn't initialized, never apply to the mesh data. */
        initial_value.flag = 1;
        ss.mode.wpaint.dvert_prev = Array<MDeformVert>(mesh->verts_num, initial_value);
      }
    }
    else {
      MEM_SAFE_FREE(ss.mode.wpaint.alpha_weight);
      if (!ss.mode.wpaint.dvert_prev.is_empty()) {
        BKE_defvert_array_free_elems(ss.mode.wpaint.dvert_prev.data(),
                                     ss.mode.wpaint.dvert_prev.size());
        ss.mode.wpaint.dvert_prev = {};
      }
    }
  }
}

IndexMask pbvh_gather_generic(const Depsgraph &depsgraph,
                              const Object &ob,
                              const VPaint &wp,
                              const Brush &brush,
                              IndexMaskMemory &memory)
{
  SculptSession &ss = *ob.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  const bool use_normal = vwpaint::use_normal(wp);
  IndexMask nodes;

  /* Build a list of all nodes that are potentially within the brush's area of influence */
  if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    nodes = bke::pbvh::search_nodes(pbvh, memory, [&](const bke::pbvh::Node &node) {
      return node_in_sphere(node, ss.cache->location_symm, ss.cache->radius_squared, true);
    });

    ss.cache->sculpt_normal_symm =
        use_normal ? calc_area_normal(depsgraph, brush, ob, nodes).value_or(float3(0)) : float3(0);
  }
  else {
    const DistRayAABB_Precalc ray_dist_precalc = dist_squared_ray_to_aabb_v3_precalc(
        ss.cache->location_symm, ss.cache->view_normal_symm);
    nodes = bke::pbvh::search_nodes(pbvh, memory, [&](const bke::pbvh::Node &node) {
      return node_in_cylinder(ray_dist_precalc, node, ss.cache->radius_squared, true);
    });

    ss.cache->sculpt_normal_symm = use_normal ? ss.cache->view_normal_symm : float3(0);
  }
  return nodes;
}

void mode_enter_generic(
    Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob, const eObjectMode mode_flag)
{
  ob.mode |= mode_flag;
  Mesh *mesh = BKE_mesh_from_object(&ob);

  /* Same as sculpt mode, make sure we don't have cached derived mesh which
   * points to freed arrays.
   */
  BKE_object_free_derived_caches(&ob);

  Paint *paint = nullptr;
  if (mode_flag == OB_MODE_VERTEX_PAINT) {
    const PaintMode paint_mode = PaintMode::Vertex;
    ED_mesh_color_ensure(mesh, nullptr);

    BKE_paint_ensure(scene.toolsettings, (Paint **)&scene.toolsettings->vpaint);
    paint = BKE_paint_get_active_from_paintmode(&scene, paint_mode);
    ED_paint_cursor_start(paint, vertex_paint_poll);
    BKE_paint_init(&bmain, &scene, paint_mode);
  }
  else if (mode_flag == OB_MODE_WEIGHT_PAINT) {
    const PaintMode paint_mode = PaintMode::Weight;

    BKE_paint_ensure(scene.toolsettings, (Paint **)&scene.toolsettings->wpaint);
    paint = BKE_paint_get_active_from_paintmode(&scene, paint_mode);
    ED_paint_cursor_start(paint, weight_paint_poll);
    BKE_paint_init(&bmain, &scene, paint_mode);

    /* weight paint specific */
    ED_mesh_mirror_spatial_table_end(&ob);
    blender::ed::object::vgroup_sync_from_pose(&ob);
  }
  else {
    BLI_assert(0);
  }

  /* Create vertex/weight paint mode session data */
  if (ob.sculpt) {
    MEM_delete(ob.sculpt->cache);
    ob.sculpt->cache = nullptr;
    BKE_sculptsession_free(&ob);
  }

  BLI_assert(paint != nullptr);
  vwpaint::init_session(bmain, depsgraph, scene, *paint, ob, mode_flag);

  /* Flush object mode. */
  DEG_id_tag_update(&ob.id, ID_RECALC_SYNC_TO_EVAL);
}

void mode_exit_generic(Object &ob, const eObjectMode mode_flag)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(&ob);
  ob.mode &= ~mode_flag;

  if (mode_flag == OB_MODE_VERTEX_PAINT) {
    if (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) {
      bke::mesh_select_face_flush(*mesh);
    }
    else if (mesh->editflag & ME_EDIT_PAINT_VERT_SEL) {
      bke::mesh_select_vert_flush(*mesh);
    }
  }
  else if (mode_flag == OB_MODE_WEIGHT_PAINT) {
    if (mesh->editflag & ME_EDIT_PAINT_VERT_SEL) {
      bke::mesh_select_vert_flush(*mesh);
    }
    else if (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) {
      bke::mesh_select_face_flush(*mesh);
    }
  }
  else {
    BLI_assert(0);
  }

  /* If the cache is not released by a cancel or a done, free it now. */
  if (ob.sculpt) {
    MEM_delete(ob.sculpt->cache);
    ob.sculpt->cache = nullptr;
  }

  BKE_sculptsession_free(&ob);

  paint_cursor_delete_textures();

  if (mode_flag == OB_MODE_WEIGHT_PAINT) {
    ED_mesh_mirror_spatial_table_end(&ob);
    ED_mesh_mirror_topo_table_end(&ob);
  }

  /* Never leave derived meshes behind. */
  BKE_object_free_derived_caches(&ob);

  /* Flush object mode. */
  DEG_id_tag_update(&ob.id, ID_RECALC_SYNC_TO_EVAL);
}

bool mode_toggle_poll_test(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || ob->type != OB_MESH) {
    return false;
  }
  if (!ob->data || !ID_IS_EDITABLE(ob->data)) {
    return false;
  }
  return true;
}

void smooth_brush_toggle_off(Paint *paint, StrokeCache *cache)
{
  Brush *brush = BKE_paint_brush(paint);
  /* The current brush should match with what we have stored in the cache. */
  BLI_assert(brush == cache->brush);

  /* If saved_active_brush is not set, brush was not switched/affected in
   * smooth_brush_toggle_on(). */
  if (cache->saved_active_brush) {
    BKE_brush_size_set(paint, brush, cache->saved_smooth_size);
    BKE_paint_brush_set(paint, cache->saved_active_brush);
    cache->saved_active_brush = nullptr;
  }
}
void update_cache_invariants(
    bContext *C, VPaint &vp, SculptSession &ss, wmOperator *op, const float mval[2])
{
  StrokeCache *cache;
  bke::PaintRuntime &paint_runtime = *vp.paint.runtime;
  ViewContext *vc = paint_stroke_view_context((PaintStroke *)op->customdata);
  Object &ob = *CTX_data_active_object(C);
  float mat[3][3];
  float view_dir[3] = {0.0f, 0.0f, 1.0f};
  int mode;

  /* VW paint needs to allocate stroke cache before update is called. */
  if (!ss.cache) {
    cache = MEM_new<StrokeCache>(__func__);
    ss.cache = cache;
  }
  else {
    cache = ss.cache;
  }

  /* Initial mouse location */
  if (mval) {
    copy_v2_v2(cache->initial_mouse, mval);
  }
  else {
    zero_v2(cache->initial_mouse);
  }

  mode = RNA_enum_get(op->ptr, "mode");
  cache->invert = mode == BRUSH_STROKE_INVERT;
  cache->alt_smooth = mode == BRUSH_STROKE_SMOOTH;
  /* not very nice, but with current events system implementation
   * we can't handle brush appearance inversion hotkey separately (sergey) */
  if (cache->invert) {
    paint_runtime.draw_inverted = true;
  }
  else {
    paint_runtime.draw_inverted = false;
  }

  if (cache->alt_smooth) {
    vwpaint::smooth_brush_toggle_on(C, &vp.paint, cache);
  }

  copy_v2_v2(cache->mouse, cache->initial_mouse);
  const Brush *brush = BKE_paint_brush(&vp.paint);
  /* Truly temporary data that isn't stored in properties */
  cache->vc = vc;
  cache->brush = brush;
  cache->paint = &vp.paint;
  cache->first_time = true;

  /* cache projection matrix */
  cache->projection_mat = ED_view3d_ob_project_mat_get(cache->vc->rv3d, &ob);

  invert_m4_m4(ob.runtime->world_to_object.ptr(), ob.object_to_world().ptr());
  copy_m3_m4(mat, cache->vc->rv3d->viewinv);
  mul_m3_v3(mat, view_dir);
  copy_m3_m4(mat, ob.world_to_object().ptr());
  mul_m3_v3(mat, view_dir);
  normalize_v3_v3(cache->view_normal, view_dir);

  cache->view_normal_symm = cache->view_normal;
  cache->bstrength = BKE_brush_alpha_get(&vp.paint, brush);
  cache->is_last_valid = false;

  cache->accum = true;

  if (BKE_brush_color_jitter_get_settings(&vp.paint, brush)) {
    cache->initial_hsv_jitter = seed_hsv_jitter();
  }
}

void update_cache_variants(bContext *C, VPaint &vp, Object &ob, PointerRNA *ptr)
{
  using namespace blender;
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  const PaintMode paint_mode = BKE_paintmode_get_active_from_context(C);
  SculptSession &ss = *ob.sculpt;
  StrokeCache *cache = ss.cache;
  Brush &brush = *BKE_paint_brush(&vp.paint);

  /* This effects the actual brush radius, so things farther away
   * are compared with a larger radius and vice versa. */
  if (cache->first_time) {
    RNA_float_get_array(ptr, "location", cache->location);
  }

  RNA_float_get_array(ptr, "mouse", cache->mouse);

  /* XXX: Use pressure value from first brush step for brushes which don't
   * support strokes (grab, thumb). They depends on initial state and
   * brush coord/pressure/etc.
   * It's more an events design issue, which doesn't split coordinate/pressure/angle
   * changing events. We should avoid this after events system re-design */
  if (paint_supports_dynamic_size(brush, paint_mode) || cache->first_time) {
    cache->pressure = RNA_float_get(ptr, "pressure");
  }

  /* Truly temporary data that isn't stored in properties */
  if (cache->first_time) {
    cache->initial_radius = paint_calc_object_space_radius(
        *cache->vc, cache->location, BKE_brush_radius_get(&vp.paint, &brush));
    BKE_brush_unprojected_size_set(&vp.paint, &brush, cache->initial_radius * 2.0f);
  }

  if (BKE_brush_use_size_pressure(&brush) && paint_supports_dynamic_size(brush, paint_mode)) {
    cache->radius = cache->initial_radius *
                    BKE_curvemapping_evaluateF(brush.curve_size, 0, cache->pressure);
  }
  else {
    cache->radius = cache->initial_radius;
  }

  cache->radius_squared = cache->radius * cache->radius;

  if (bke::pbvh::Tree *pbvh = bke::object::pbvh_get(ob)) {
    pbvh->update_bounds(depsgraph, ob);
  }
}

void get_brush_alpha_data(const SculptSession &ss,
                          const Paint &paint,
                          const Brush &brush,
                          float *r_brush_size_pressure,
                          float *r_brush_alpha_value,
                          float *r_brush_alpha_pressure)
{
  *r_brush_size_pressure = BKE_brush_radius_get(&paint, &brush) *
                           (BKE_brush_use_size_pressure(&brush) ?
                                BKE_curvemapping_evaluateF(
                                    brush.curve_size, 0, ss.cache->pressure) :
                                1.0f);
  *r_brush_alpha_value = BKE_brush_alpha_get(&paint, &brush);
  *r_brush_alpha_pressure = BKE_brush_use_alpha_pressure(&brush) ?
                                BKE_curvemapping_evaluateF(
                                    brush.curve_strength, 0, ss.cache->pressure) :
                                1.0f;
}

void last_stroke_update(const float location[3], Paint &paint)
{
  bke::PaintRuntime &paint_runtime = *paint.runtime;
  paint_runtime.average_stroke_counter++;
  add_v3_v3(paint_runtime.average_stroke_accum, location);
  paint_runtime.last_stroke_valid = true;
}

/* -------------------------------------------------------------------- */

void smooth_brush_toggle_on(const bContext *C, Paint *paint, StrokeCache *cache)
{
  Main *bmain = CTX_data_main(C);
  Brush *cur_brush = BKE_paint_brush(paint);

  /* Switch to the blur (smooth) brush if possible. */
  BKE_paint_brush_set_essentials(bmain, paint, "Blur");
  Brush *smooth_brush = BKE_paint_brush(paint);

  if (!smooth_brush) {
    BKE_paint_brush_set(paint, cur_brush);
    CLOG_WARN(&LOG, "Switching to the blur (smooth) brush not possible, corresponding brush not");
    cache->saved_active_brush = nullptr;
    return;
  }

  int cur_brush_size = BKE_brush_size_get(paint, cur_brush);

  cache->saved_active_brush = cur_brush;
  cache->saved_smooth_size = BKE_brush_size_get(paint, smooth_brush);
  BKE_brush_size_set(paint, smooth_brush, cur_brush_size);
  BKE_curvemapping_init(smooth_brush->curve_distance_falloff);
}
/** \} */
}  // namespace blender::ed::sculpt_paint::vwpaint

bool vertex_paint_mode_poll(bContext *C)
{
  const Object *ob = CTX_data_active_object(C);
  if (!ob) {
    return false;
  }
  const Mesh *mesh = static_cast<const Mesh *>(ob->data);

  if (!(ob->mode == OB_MODE_VERTEX_PAINT && mesh->faces_num)) {
    return false;
  }

  if (!BKE_color_attribute_supported(*mesh, mesh->active_color_attribute)) {
    return false;
  }

  return true;
}

static bool vertex_paint_poll_ex(bContext *C, bool check_tool)
{
  if (vertex_paint_mode_poll(C) && BKE_paint_brush(&CTX_data_tool_settings(C)->vpaint->paint)) {
    ScrArea *area = CTX_wm_area(C);
    if (area && area->spacetype == SPACE_VIEW3D) {
      ARegion *region = CTX_wm_region(C);
      if (region->regiontype == RGN_TYPE_WINDOW) {
        if (!check_tool || WM_toolsystem_active_tool_is_brush(C)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool vertex_paint_poll(bContext *C)
{
  return vertex_paint_poll_ex(C, true);
}

bool vertex_paint_poll_ignore_tool(bContext *C)
{
  return vertex_paint_poll_ex(C, false);
}

static ColorPaint4f vpaint_get_current_col(VPaint &vp, bool secondary)
{
  const Brush *brush = BKE_paint_brush_for_read(&vp.paint);
  float color[4];
  const float *brush_color = secondary ? BKE_brush_secondary_color_get(&vp.paint, brush) :
                                         BKE_brush_color_get(&vp.paint, brush);
  copy_v3_v3(color, brush_color);

  color[3] = 1.0f; /* alpha isn't used, could even be removed to speedup paint a little */

  return ColorPaint4f(color);
}

/* wpaint has 'wpaint_blend' */
template<typename Color, typename Traits>
static Color vpaint_blend(const VPaint &vp,
                          Color color_curr,
                          Color color_orig,
                          Color color_paint,
                          const typename Traits::ValueType alpha,
                          const typename Traits::BlendType brush_alpha_value)
{
  using Value = typename Traits::ValueType;

  const Brush &brush = *BKE_paint_brush_for_read(&vp.paint);
  const IMB_BlendMode blend = (IMB_BlendMode)brush.blend;

  const Color color_blend = BLI_mix_colors<Color, Traits>(blend, color_curr, color_paint, alpha);

  /* If no accumulate, clip color adding with `color_orig` & `color_test`. */
  if (!vwpaint::brush_use_accumulate(vp)) {
    uint a;
    Color color_test;
    Value *cp, *ct, *co;

    color_test = BLI_mix_colors<Color, Traits>(blend, color_orig, color_paint, brush_alpha_value);

    cp = (Value *)&color_blend;
    ct = (Value *)&color_test;
    co = (Value *)&color_orig;

    for (a = 0; a < 4; a++) {
      if (ct[a] < co[a]) {
        if (cp[a] < ct[a]) {
          cp[a] = ct[a];
        }
        else if (cp[a] > co[a]) {
          cp[a] = co[a];
        }
      }
      else {
        if (cp[a] < co[a]) {
          cp[a] = co[a];
        }
        else if (cp[a] > ct[a]) {
          cp[a] = ct[a];
        }
      }
    }
  }

  if ((brush.flag & BRUSH_LOCK_ALPHA) && !ELEM(blend, IMB_BLEND_ERASE_ALPHA, IMB_BLEND_ADD_ALPHA))
  {
    Value *cp, *cc;
    cp = (Value *)&color_blend;
    cc = (Value *)&color_curr;
    cp[3] = cc[3];
  }

  return color_blend;
}

/**
 * If in accumulate mode, blend brush mark directly onto mesh, else blend into temporary
 * stroke_buffer and blend the stroke onto the mesh.
 *
 * \param brush_mark_alpha: Modulated strength on a per-vertex basis
 * \param brush_strength: Unmodified raw value of the brush
 */
template<typename Color, typename Traits>
static Color vpaint_blend_stroke(const VPaint &vp,
                                 MutableSpan<Color> prev_vertex_colors,
                                 MutableSpan<Color> vertex_colors,
                                 MutableSpan<Color> stroke_buffer,
                                 Color brush_mark_color,
                                 float brush_mark_alpha,
                                 float brush_strength,
                                 int index)
{
  Color result;
  if (!vwpaint::brush_use_accumulate(vp)) {
    BLI_assert(!stroke_buffer.is_empty());
    BLI_assert(!prev_vertex_colors.is_empty());

    if (isZero(prev_vertex_colors[index])) {
      prev_vertex_colors[index] = vertex_colors[index];
    }

    /* Mix with mesh color under the stroke (a bit easier than trying to premultiply
     * byte Color types */
    if (isZero(stroke_buffer[index])) {
      stroke_buffer[index] = vertex_colors[index];
      stroke_buffer[index].a = 0;
    }

    stroke_buffer[index] = BLI_mix_colors<Color, Traits>(IMB_BlendMode::IMB_BLEND_MIX,
                                                         brush_mark_color,
                                                         stroke_buffer[index],
                                                         stroke_buffer[index].a);

    result = vpaint_blend<Color, Traits>(vp,
                                         vertex_colors[index],
                                         prev_vertex_colors[index],
                                         stroke_buffer[index],
                                         brush_mark_alpha,
                                         Traits::range * brush_strength);
  }
  else {
    result = vpaint_blend<Color, Traits>(vp,
                                         vertex_colors[index],
                                         Color() /* unused in accumulate mode */,
                                         brush_mark_color,
                                         brush_mark_alpha,
                                         Traits::range * brush_strength);
  }
  return result;
}

static void paint_and_tex_color_alpha_intern(const VPaint &vp,
                                             const ViewContext *vc,
                                             const float co[3],
                                             float r_rgba[4])
{
  const Brush *brush = BKE_paint_brush_for_read(&vp.paint);
  const MTex *mtex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);
  BLI_assert(mtex->tex != nullptr);
  if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
    BKE_brush_sample_tex_3d(&vp.paint, brush, mtex, co, r_rgba, 0, nullptr);
  }
  else {
    float co_ss[2]; /* screenspace */
    if (ED_view3d_project_float_object(
            vc->region, co, co_ss, (V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) ==
        V3D_PROJ_RET_OK)
    {
      const float co_ss_3d[3] = {co_ss[0], co_ss[1], 0.0f}; /* we need a 3rd empty value */
      BKE_brush_sample_tex_3d(&vp.paint, brush, mtex, co_ss_3d, r_rgba, 0, nullptr);
    }
    else {
      zero_v4(r_rgba);
    }
  }
}

static void vertex_paint_init_stroke(Depsgraph &depsgraph, Object &ob)
{
  vwpaint::init_stroke(depsgraph, ob);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enter Vertex Paint Mode
 * \{ */

void ED_object_vpaintmode_enter_ex(Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob)
{
  vwpaint::mode_enter_generic(bmain, depsgraph, scene, ob, OB_MODE_VERTEX_PAINT);
}
void ED_object_vpaintmode_enter(bContext *C, Depsgraph &depsgraph)
{
  Main &bmain = *CTX_data_main(C);
  Scene &scene = *CTX_data_scene(C);
  Object &ob = *CTX_data_active_object(C);
  ED_object_vpaintmode_enter_ex(bmain, depsgraph, scene, ob);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exit Vertex Paint Mode
 * \{ */

void ED_object_vpaintmode_exit_ex(Object &ob)
{
  vwpaint::mode_exit_generic(ob, OB_MODE_VERTEX_PAINT);
}
void ED_object_vpaintmode_exit(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  ED_object_vpaintmode_exit_ex(*ob);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Vertex Paint Operator
 * \{ */

/**
 * \note Keep in sync with #wpaint_mode_toggle_exec
 */
static wmOperatorStatus vpaint_mode_toggle_exec(bContext *C, wmOperator *op)
{
  Main &bmain = *CTX_data_main(C);
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  Object &ob = *CTX_data_active_object(C);
  const int mode_flag = OB_MODE_VERTEX_PAINT;
  const bool is_mode_set = (ob.mode & mode_flag) != 0;
  Scene &scene = *CTX_data_scene(C);
  ToolSettings &ts = *scene.toolsettings;

  if (!is_mode_set) {
    if (!blender::ed::object::mode_compat_set(C, &ob, (eObjectMode)mode_flag, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  Mesh *mesh = BKE_mesh_from_object(&ob);

  if (is_mode_set) {
    ED_object_vpaintmode_exit_ex(ob);
  }
  else {
    Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);
    if (depsgraph) {
      depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    }
    ED_object_vpaintmode_enter_ex(bmain, *depsgraph, scene, ob);
    BKE_paint_brushes_validate(&bmain, &ts.vpaint->paint);
  }

  BKE_mesh_batch_cache_dirty_tag((Mesh *)ob.data, BKE_MESH_BATCH_DIRTY_ALL);

  /* update modifier stack for mapping requirements */
  DEG_id_tag_update(&mesh->id, 0);

  WM_event_add_notifier(C, NC_SCENE | ND_MODE, &scene);
  WM_msg_publish_rna_prop(mbus, &ob.id, &ob, Object, mode);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_paint_toggle(wmOperatorType *ot)
{
  ot->name = "Vertex Paint Mode";
  ot->idname = "PAINT_OT_vertex_paint_toggle";
  ot->description = "Toggle the vertex paint mode in 3D view";

  ot->exec = vpaint_mode_toggle_exec;
  ot->poll = vwpaint::mode_toggle_poll_test;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Paint Operator
 * \{ */

/* Implementation notes:
 *
 * Operator->invoke()
 * - Validate context (add #Mesh.mloopcol).
 * - Create custom-data storage.
 * - Call paint once (mouse click).
 * - Add modal handler.
 *
 * Operator->modal()
 * - For every mouse-move, apply vertex paint.
 * - Exit on mouse release, free custom-data.
 *   (return OPERATOR_FINISHED also removes handler and operator)
 *
 * For future:
 * - implement a stroke event (or mouse-move with past positions).
 * - revise whether op->customdata should be added in object, in set_vpaint.
 */

template<typename Func>
static void to_static_color_type(const bke::AttrType type, const Func &func)
{
  switch (type) {
    case bke::AttrType::ColorFloat:
      func(ColorGeometry4f());
      break;
    case bke::AttrType::ColorByte:
      func(ColorGeometry4b());
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

struct VPaintData : public PaintModeData {
  ViewContext vc;
  AttrDomain domain;
  bke::AttrType type;

  NormalAnglePrecalc normal_angle_precalc;

  ColorPaint4f paintcol;

  VertProjHandle *vp_handle;
  /**
   * Owned by #vp_handle.
   * \todo Look into replacing this with just using the evaluated/deform positions.
   */
  Span<float3> vert_positions;
  Span<float3> vert_normals;

  bool is_texbrush;

  /* Special storage for smear brush, avoid feedback loop - update each step. */
  struct {
    GArray<> color_prev;
    GArray<> color_curr;
  } smear;

  /* For brushes that don't use accumulation, a temporary holding array */
  GArray<> prev_colors;
  GArray<> stroke_buffer;

  ~VPaintData() override
  {
    if (vp_handle) {
      ED_vpaint_proj_handle_free(vp_handle);
    }
  }
};

static std::unique_ptr<VPaintData> vpaint_init_vpaint(bContext *C,
                                                      wmOperator *op,
                                                      Scene &scene,
                                                      Depsgraph &depsgraph,
                                                      VPaint &vp,
                                                      Object &ob,
                                                      Mesh &mesh,
                                                      const AttrDomain domain,
                                                      const bke::AttrType type,
                                                      const Brush &brush)
{
  std::unique_ptr<VPaintData> vpd = std::make_unique<VPaintData>();

  vpd->type = type;
  vpd->domain = domain;

  vpd->vc = ED_view3d_viewcontext_init(C, &depsgraph);

  vwpaint::view_angle_limits_init(&vpd->normal_angle_precalc,
                                  brush.falloff_angle,
                                  (brush.flag & BRUSH_FRONTFACE_FALLOFF) != 0);

  vpd->paintcol = vpaint_get_current_col(vp,
                                         (RNA_enum_get(op->ptr, "mode") == BRUSH_STROKE_INVERT));

  vpd->is_texbrush = !(brush.vertex_brush_type == VPAINT_BRUSH_TYPE_BLUR) && brush.mtex.tex;

  if (brush.vertex_brush_type == VPAINT_BRUSH_TYPE_SMEAR) {
    const GVArray attribute = *mesh.attributes().lookup(mesh.active_color_attribute, domain);
    vpd->smear.color_prev = GArray(attribute.type(), attribute.size());
    attribute.materialize(vpd->smear.color_prev.data());

    vpd->smear.color_curr = vpd->smear.color_prev;
  }

  /* Create projection handle */
  if (vpd->is_texbrush) {
    ob.sculpt->building_vp_handle = true;
    vpd->vp_handle = ED_vpaint_proj_handle_create(
        depsgraph, scene, ob, vpd->vert_positions, vpd->vert_normals);
    ob.sculpt->building_vp_handle = false;
  }

  if (!vwpaint::brush_use_accumulate(vp)) {
    if (vpd->prev_colors.is_empty()) {
      const GVArray attribute = *mesh.attributes().lookup(mesh.active_color_attribute);
      vpd->prev_colors = GArray(attribute.type(), attribute.size());
      attribute.type().value_initialize_n(vpd->prev_colors.data(), vpd->prev_colors.size());
    }

    if (vpd->stroke_buffer.is_empty()) {
      const GVArray attribute = *mesh.attributes().lookup(mesh.active_color_attribute);
      vpd->stroke_buffer = GArray(attribute.type(), attribute.size());
      attribute.type().value_initialize_n(vpd->stroke_buffer.data(), vpd->stroke_buffer.size());
    }
  }
  else {
    vpd->prev_colors = {};
    vpd->stroke_buffer = {};
  }

  return vpd;
}

static bool vpaint_stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
  Scene &scene = *CTX_data_scene(C);
  ToolSettings &ts = *scene.toolsettings;
  PaintStroke *stroke = (PaintStroke *)op->customdata;
  VPaint &vp = *ts.vpaint;
  Brush &brush = *BKE_paint_brush(&vp.paint);
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;
  Depsgraph &depsgraph = *CTX_data_ensure_evaluated_depsgraph(C);

  /* context checks could be a poll() */
  Mesh *mesh = BKE_mesh_from_object(&ob);
  if (mesh == nullptr || mesh->faces_num == 0) {
    return false;
  }

  ED_mesh_color_ensure(mesh, nullptr);

  const std::optional<bke::AttributeMetaData> meta_data = mesh->attributes().lookup_meta_data(
      mesh->active_color_attribute);
  if (!BKE_color_attribute_supported(*mesh, mesh->active_color_attribute)) {
    return false;
  }

  std::unique_ptr<VPaintData> vpd = vpaint_init_vpaint(
      C, op, scene, depsgraph, vp, ob, *mesh, meta_data->domain, meta_data->data_type, brush);

  paint_stroke_set_mode_data(stroke, std::move(vpd));

  /* If not previously created, create vertex/weight paint mode session data */
  vertex_paint_init_stroke(depsgraph, ob);
  vwpaint::update_cache_invariants(C, vp, ss, op, mouse);
  vwpaint::init_session_data(ts, ob);

  return true;
}

static void filter_factors_with_selection(const Span<bool> select_vert,
                                          const Span<int> verts,
                                          const MutableSpan<float> factors)
{
  BLI_assert(verts.size() == factors.size());

  for (const int i : verts.index_range()) {
    if (!select_vert[verts[i]]) {
      factors[i] = 0.0f;
    }
  }
}

static void do_vpaint_brush_blur_loops(const bContext *C,
                                       const VPaint &vp,
                                       VPaintData &vpd,
                                       Object &ob,
                                       Mesh &mesh,
                                       const Span<bke::pbvh::MeshNode> nodes,
                                       const IndexMask &node_mask,
                                       GMutableSpan attribute)
{
  SculptSession &ss = *ob.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Brush &brush = *ob.sculpt->cache->brush;
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);

  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  vwpaint::get_brush_alpha_data(
      ss, vp.paint, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_vert_sel = (mesh.editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) !=
                            0;
  const bool use_face_sel = (mesh.editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush.falloff_shape);

  GMutableSpan g_previous_color = vpd.prev_colors;

  const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face = mesh.vert_to_face_map();
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, ob);
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
  VArraySpan<bool> select_vert;
  if (use_vert_sel) {
    select_vert = *attributes.lookup<bool>(".select_vert", bke::AttrDomain::Point);
  }
  VArraySpan<bool> select_poly;
  if (use_face_sel) {
    select_poly = *attributes.lookup<bool>(".select_poly", bke::AttrDomain::Face);
  }

  struct LocalData {
    Vector<float> factors;
    Vector<float> distances;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    LocalData &tls = all_tls.local();
    const Span<int> verts = nodes[i].verts();
    tls.factors.resize(verts.size());
    const MutableSpan<float> factors = tls.factors;
    fill_factor_from_hide(hide_vert, verts, factors);
    filter_region_clip_factors(ss, vert_positions, verts, factors);
    if (!select_vert.is_empty()) {
      filter_factors_with_selection(select_vert, verts, factors);
    }

    tls.distances.resize(verts.size());
    const MutableSpan<float> distances = tls.distances;
    calc_brush_distances(
        ss, vert_positions, verts, eBrushFalloffShape(brush.falloff_shape), distances);
    filter_distances_with_radius(cache.radius, distances, factors);
    calc_brush_strength_factors(cache, brush, distances, factors);

    for (const int i : verts.index_range()) {
      const int vert = verts[i];
      if (factors[i] == 0.0f) {
        continue;
      }

      float brush_strength = cache.bstrength;
      const float angle_cos = use_normal ? dot_v3v3(sculpt_normal_frontface, vert_normals[vert]) :
                                           1.0f;
      if (!vwpaint::test_brush_angle_falloff(
              brush, vpd.normal_angle_precalc, angle_cos, &brush_strength))
      {
        continue;
      }

      const float brush_fade = factors[i];

      to_static_color_type(vpd.type, [&](auto dummy) {
        using T = decltype(dummy);
        using Color =
            std::conditional_t<std::is_same_v<T, ColorGeometry4f>, ColorPaint4f, ColorPaint4b>;
        using Traits = blender::color::Traits<Color>;
        using Blend = typename Traits::BlendType;
        MutableSpan<Color> previous_color = g_previous_color.typed<T>().template cast<Color>();
        MutableSpan<Color> colors = attribute.typed<T>().template cast<Color>();
        /* Get the average face color */
        Color color_final(0, 0, 0, 0);

        int total_hit_loops = 0;
        Blend blend[4] = {0};

        for (const int face : vert_to_face[vert]) {
          if (!select_poly.is_empty() && !select_poly[face]) {
            return;
          }
          total_hit_loops += faces[face].size();
          for (const int corner : faces[face]) {
            const Color &col = colors[corner];

            /* Color is squared to compensate the `sqrt` color encoding. */
            blend[0] += (Blend)col.r * (Blend)col.r;
            blend[1] += (Blend)col.g * (Blend)col.g;
            blend[2] += (Blend)col.b * (Blend)col.b;
            blend[3] += (Blend)col.a * (Blend)col.a;
          }
        }

        if (total_hit_loops == 0) {
          return;
        }

        /* Use rgb^2 color averaging. */
        Color *col = &color_final;

        color_final.r = Traits::round(sqrtf(Traits::divide_round(blend[0], total_hit_loops)));
        color_final.g = Traits::round(sqrtf(Traits::divide_round(blend[1], total_hit_loops)));
        color_final.b = Traits::round(sqrtf(Traits::divide_round(blend[2], total_hit_loops)));
        color_final.a = Traits::round(sqrtf(Traits::divide_round(blend[3], total_hit_loops)));

        /* For each face owning this vert,
         * paint each loop belonging to this vert. */
        for (const int face : vert_to_face[vert]) {
          const int corner = bke::mesh::face_find_corner_from_vert(
              faces[face], corner_verts, vert);
          if (!select_poly.is_empty() && !select_poly[face]) {
            continue;
          }
          Color color_orig(0, 0, 0, 0); /* unused when array is nullptr */

          if (!previous_color.is_empty()) {
            /* Get the previous loop color */
            if (isZero(previous_color[corner])) {
              previous_color[corner] = colors[corner];
            }
            color_orig = previous_color[corner];
          }
          const float final_alpha = Traits::range * brush_fade * brush_strength *
                                    brush_alpha_pressure;
          /* Mix the new color with the original
           * based on the brush strength and the curve. */
          colors[corner] = vpaint_blend<Color, Traits>(
              vp, colors[corner], color_orig, *col, final_alpha, Traits::range * brush_strength);
        }
      });
    }
  });
}

static void do_vpaint_brush_blur_verts(const bContext *C,
                                       const VPaint &vp,
                                       VPaintData &vpd,
                                       Object &ob,
                                       Mesh &mesh,
                                       const Span<bke::pbvh::MeshNode> nodes,
                                       const IndexMask &node_mask,
                                       GMutableSpan attribute)
{
  SculptSession &ss = *ob.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Brush &brush = *ss.cache->brush;
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);

  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  vwpaint::get_brush_alpha_data(
      ss, vp.paint, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_vert_sel = (mesh.editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) !=
                            0;
  const bool use_face_sel = (mesh.editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush.falloff_shape);

  GMutableSpan g_previous_color = vpd.prev_colors;

  const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face = mesh.vert_to_face_map();
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, ob);
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
  VArraySpan<bool> select_vert;
  if (use_vert_sel) {
    select_vert = *attributes.lookup<bool>(".select_vert", bke::AttrDomain::Point);
  }
  VArraySpan<bool> select_poly;
  if (use_face_sel) {
    select_poly = *attributes.lookup<bool>(".select_poly", bke::AttrDomain::Face);
  }

  struct LocalData {
    Vector<float> factors;
    Vector<float> distances;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    LocalData &tls = all_tls.local();
    const Span<int> verts = nodes[i].verts();
    tls.factors.resize(verts.size());
    const MutableSpan<float> factors = tls.factors;
    fill_factor_from_hide(hide_vert, verts, factors);
    filter_region_clip_factors(ss, vert_positions, verts, factors);
    if (!select_vert.is_empty()) {
      filter_factors_with_selection(select_vert, verts, factors);
    }

    tls.distances.resize(verts.size());
    const MutableSpan<float> distances = tls.distances;
    calc_brush_distances(
        ss, vert_positions, verts, eBrushFalloffShape(brush.falloff_shape), distances);
    filter_distances_with_radius(cache.radius, distances, factors);
    calc_brush_strength_factors(cache, brush, distances, factors);

    for (const int i : verts.index_range()) {
      const int vert = verts[i];
      if (factors[i] == 0.0f) {
        continue;
      }

      float brush_strength = cache.bstrength;
      const float angle_cos = use_normal ? dot_v3v3(sculpt_normal_frontface, vert_normals[vert]) :
                                           1.0f;
      if (!vwpaint::test_brush_angle_falloff(
              brush, vpd.normal_angle_precalc, angle_cos, &brush_strength))
      {
        continue;
      }
      const float brush_fade = factors[i];

      /* Get the average face color */
      to_static_color_type(vpd.type, [&](auto dummy) {
        using T = decltype(dummy);
        using Color =
            std::conditional_t<std::is_same_v<T, ColorGeometry4f>, ColorPaint4f, ColorPaint4b>;
        using Traits = blender::color::Traits<Color>;
        using Blend = typename Traits::BlendType;
        MutableSpan<Color> previous_color = g_previous_color.typed<T>().template cast<Color>();
        MutableSpan<Color> colors = attribute.typed<T>().template cast<Color>();
        Color color_final(0, 0, 0, 0);

        int total_hit_loops = 0;
        Blend blend[4] = {0};

        for (const int face : vert_to_face[vert]) {
          if (!select_poly.is_empty() && !select_poly[face]) {
            continue;
          }
          total_hit_loops += faces[face].size();
          for (const int vert : corner_verts.slice(faces[face])) {
            const Color &col = colors[vert];

            /* Color is squared to compensate the `sqrt` color encoding. */
            blend[0] += (Blend)col.r * (Blend)col.r;
            blend[1] += (Blend)col.g * (Blend)col.g;
            blend[2] += (Blend)col.b * (Blend)col.b;
            blend[3] += (Blend)col.a * (Blend)col.a;
          }
        }

        if (total_hit_loops == 0) {
          return;
        }
        /* Use rgb^2 color averaging. */
        color_final.r = Traits::round(sqrtf(Traits::divide_round(blend[0], total_hit_loops)));
        color_final.g = Traits::round(sqrtf(Traits::divide_round(blend[1], total_hit_loops)));
        color_final.b = Traits::round(sqrtf(Traits::divide_round(blend[2], total_hit_loops)));
        color_final.a = Traits::round(sqrtf(Traits::divide_round(blend[3], total_hit_loops)));

        Color color_orig(0, 0, 0, 0); /* unused when array is nullptr */

        if (!previous_color.is_empty()) {
          /* Get the previous loop color */
          if (isZero(previous_color[vert])) {
            previous_color[vert] = colors[vert];
          }
          color_orig = previous_color[vert];
        }
        const float final_alpha = Traits::range * brush_fade * brush_strength *
                                  brush_alpha_pressure;
        /* Mix the new color with the original
         * based on the brush strength and the curve. */
        colors[vert] = vpaint_blend<Color, Traits>(vp,
                                                   colors[vert],
                                                   color_orig,
                                                   color_final,
                                                   final_alpha,
                                                   Traits::range * brush_strength);
      });
    }
  });
}

static void do_vpaint_brush_smear(const bContext *C,
                                  const VPaint &vp,
                                  VPaintData &vpd,
                                  Object &ob,
                                  Mesh &mesh,
                                  const Span<bke::pbvh::MeshNode> nodes,
                                  const IndexMask &node_mask,
                                  GMutableSpan attribute)
{
  SculptSession &ss = *ob.sculpt;
  StrokeCache &cache = *ss.cache;
  if (!cache.is_last_valid) {
    return;
  }

  const Brush &brush = *cache.brush;
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  GMutableSpan g_color_curr = vpd.smear.color_curr;
  GMutableSpan g_color_prev_smear = vpd.smear.color_prev;
  GMutableSpan g_color_prev = vpd.prev_colors;

  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;

  vwpaint::get_brush_alpha_data(
      ss, vp.paint, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_vert_sel = (mesh.editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) !=
                            0;
  const bool use_face_sel = (mesh.editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

  float brush_dir[3];
  sub_v3_v3v3(brush_dir, cache.location_symm, cache.last_location_symm);
  project_plane_v3_v3v3(brush_dir, brush_dir, cache.view_normal_symm);
  if (normalize_v3(brush_dir) == 0.0f) {
    return;
  }

  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush.falloff_shape);

  const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face = mesh.vert_to_face_map();
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, ob);
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
  VArraySpan<bool> select_vert;
  if (use_vert_sel) {
    select_vert = *attributes.lookup<bool>(".select_vert", bke::AttrDomain::Point);
  }
  VArraySpan<bool> select_poly;
  if (use_face_sel) {
    select_poly = *attributes.lookup<bool>(".select_poly", bke::AttrDomain::Face);
  }

  struct LocalData {
    Vector<float> factors;
    Vector<float> distances;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    LocalData &tls = all_tls.local();
    const Span<int> verts = nodes[i].verts();
    tls.factors.resize(verts.size());
    const MutableSpan<float> factors = tls.factors;
    fill_factor_from_hide(hide_vert, verts, factors);
    filter_region_clip_factors(ss, vert_positions, verts, factors);
    if (!select_vert.is_empty()) {
      filter_factors_with_selection(select_vert, verts, factors);
    }

    tls.distances.resize(verts.size());
    const MutableSpan<float> distances = tls.distances;
    calc_brush_distances(
        ss, vert_positions, verts, eBrushFalloffShape(brush.falloff_shape), distances);
    filter_distances_with_radius(cache.radius, distances, factors);
    calc_brush_strength_factors(cache, brush, distances, factors);

    for (const int i : verts.index_range()) {
      const int vert = verts[i];
      if (factors[i] == 0.0f) {
        continue;
      }

      /* Calculate the dot prod. between ray norm on surf and current vert
       * (ie splash prevention factor), and only paint front facing verts. */
      float brush_strength = cache.bstrength;
      const float angle_cos = use_normal ? dot_v3v3(sculpt_normal_frontface, vert_normals[vert]) :
                                           1.0f;
      if (!vwpaint::test_brush_angle_falloff(
              brush, vpd.normal_angle_precalc, angle_cos, &brush_strength))
      {
        continue;
      }
      const float brush_fade = factors[i];

      bool do_color = false;
      /* Minimum dot product between brush direction and current
       * to neighbor direction is 0.0, meaning orthogonal. */
      float stroke_dot_max = 0.0f;

      /* Get the color of the loop in the opposite
       * direction of the brush movement */
      to_static_color_type(vpd.type, [&](auto dummy) {
        using T = decltype(dummy);
        using Color =
            std::conditional_t<std::is_same_v<T, ColorGeometry4f>, ColorPaint4f, ColorPaint4b>;
        using Traits = blender::color::Traits<Color>;
        MutableSpan<Color> color_curr = g_color_curr.typed<T>().template cast<Color>();
        MutableSpan<Color> color_prev_smear = g_color_prev_smear.typed<T>().template cast<Color>();
        MutableSpan<Color> color_prev = g_color_prev.typed<T>().template cast<Color>();
        MutableSpan<Color> colors = attribute.typed<T>().template cast<Color>();

        Color color_final(0, 0, 0, 0);

        for (const int face : vert_to_face[vert]) {
          if (!select_poly.is_empty() && !select_poly[face]) {
            continue;
          }
          for (const int corner : faces[face]) {
            const int v_other_index = corner_verts[corner];
            if (v_other_index == vert) {
              continue;
            }

            /* Get the direction from the
             * selected vert to the neighbor. */
            float other_dir[3];
            sub_v3_v3v3(other_dir, vert_positions[vert], vert_positions[v_other_index]);
            project_plane_v3_v3v3(other_dir, other_dir, cache.view_normal_symm);

            normalize_v3(other_dir);

            const float stroke_dot = dot_v3v3(other_dir, brush_dir);
            int elem_index;

            if (vpd.domain == AttrDomain::Point) {
              elem_index = v_other_index;
            }
            else {
              elem_index = corner;
            }

            if (stroke_dot > stroke_dot_max) {
              stroke_dot_max = stroke_dot;
              color_final = color_prev_smear[elem_index];
              do_color = true;
            }
          }
        }

        if (!do_color) {
          return;
        }

        const float final_alpha = Traits::range * brush_fade * brush_strength *
                                  brush_alpha_pressure;

        /* For each face owning this vert,
         * paint each loop belonging to this vert. */
        for (const int face : vert_to_face[vert]) {

          int elem_index;
          if (vpd.domain == AttrDomain::Point) {
            elem_index = vert;
          }
          else {
            elem_index = bke::mesh::face_find_corner_from_vert(faces[face], corner_verts, vert);
          }
          if (!select_poly.is_empty() && !select_poly[face]) {
            continue;
          }

          /* Get the previous element color */
          Color color_orig(0, 0, 0, 0); /* unused when array is nullptr */

          if (!color_prev.is_empty()) {
            /* Get the previous element color */
            if (isZero(color_prev[elem_index])) {
              color_prev[elem_index] = colors[elem_index];
            }
            color_orig = color_prev[elem_index];
          }
          /* Mix the new color with the original
           * based on the brush strength and the curve. */
          colors[elem_index] = vpaint_blend<Color, Traits>(vp,
                                                           colors[elem_index],
                                                           color_orig,
                                                           color_final,
                                                           final_alpha,
                                                           Traits::range * brush_strength);

          color_curr[elem_index] = colors[elem_index];
        }
      });
    }
  });
}

static void calculate_average_color(VPaintData &vpd,
                                    Object &ob,
                                    Mesh &mesh,
                                    const Brush &brush,
                                    const GSpan attribute,
                                    const Span<bke::pbvh::MeshNode> nodes,
                                    const IndexMask &node_mask)
{
  SculptSession &ss = *ob.sculpt;
  const StrokeCache &cache = *ss.cache;
  const Depsgraph &depsgraph = *vpd.vc.depsgraph;

  const bool use_vert_sel = (mesh.editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) !=
                            0;

  const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
  VArraySpan<bool> select_vert;
  if (use_vert_sel) {
    select_vert = *attributes.lookup<bool>(".select_vert", bke::AttrDomain::Point);
  }

  struct LocalData {
    Vector<float> factors;
    Vector<float> distances;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  to_static_color_type(vpd.type, [&](auto dummy) {
    using T = decltype(dummy);
    using Color =
        std::conditional_t<std::is_same_v<T, ColorGeometry4f>, ColorPaint4f, ColorPaint4b>;
    using Traits = blender::color::Traits<Color>;
    using Blend = typename Traits::BlendType;
    const Span<Color> colors = attribute.typed<T>().template cast<Color>();

    Array<VPaintAverageAccum<Blend>> accum(nodes.size(), {0, {0, 0, 0}});
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      VPaintAverageAccum<Blend> &accum2 = accum[i];
      LocalData &tls = all_tls.local();

      const Span<int> verts = nodes[i].verts();
      tls.factors.resize(verts.size());
      const MutableSpan<float> factors = tls.factors;
      fill_factor_from_hide(hide_vert, verts, factors);
      filter_region_clip_factors(ss, vert_positions, verts, factors);
      if (!select_vert.is_empty()) {
        filter_factors_with_selection(select_vert, verts, factors);
      }

      tls.distances.resize(verts.size());
      const MutableSpan<float> distances = tls.distances;
      calc_brush_distances(
          ss, vert_positions, verts, eBrushFalloffShape(brush.falloff_shape), distances);
      filter_distances_with_radius(cache.radius, distances, factors);
      calc_brush_strength_factors(cache, brush, distances, factors);

      for (const int i : verts.index_range()) {
        const int vert = verts[i];
        if (factors[i] == 0.0f) {
          continue;
        }

        accum2.len += vert_to_face[vert].size();
        /* if a vertex is within the brush region, then add its color to the blend. */
        for (const int face : vert_to_face[vert]) {
          int elem_index;
          if (vpd.domain == AttrDomain::Corner) {
            elem_index = bke::mesh::face_find_corner_from_vert(faces[face], corner_verts, vert);
          }
          else {
            elem_index = vert;
          }

          /* Color is squared to compensate the `sqrt` color encoding. */
          const Color &col = colors[elem_index];
          accum2.value[0] += col.r * col.r;
          accum2.value[1] += col.g * col.g;
          accum2.value[2] += col.b * col.b;
        }
      }
    });

    Blend accum_len = 0;
    Blend accum_value[3] = {0};
    Color blend(0, 0, 0, 0);

    node_mask.foreach_index([&](const int i) {
      accum_len += accum[i].len;
      accum_value[0] += accum[i].value[0];
      accum_value[1] += accum[i].value[1];
      accum_value[2] += accum[i].value[2];
    });
    if (accum_len != 0) {
      blend.r = Traits::round(sqrtf(Traits::divide_round(accum_value[0], accum_len)));
      blend.g = Traits::round(sqrtf(Traits::divide_round(accum_value[1], accum_len)));
      blend.b = Traits::round(sqrtf(Traits::divide_round(accum_value[2], accum_len)));
      blend.a = Traits::range;

      vpd.paintcol = toFloat(blend);
    }
  });
}

template<typename Color>
static float paint_and_tex_color_alpha(const VPaint &vp,
                                       VPaintData &vpd,
                                       const float v_co[3],
                                       Color *r_color)
{
  ColorPaint4f rgba;
  paint_and_tex_color_alpha_intern(vp, &vpd.vc, v_co, &rgba.r);

  ColorPaint4f rgba_br = toFloat(vpd.paintcol);
  mul_v3_v3(rgba_br, rgba);

  *r_color = fromFloat<Color>(rgba_br);
  return rgba[3];
}

/* Compute brush color, using jitter if it's enabled */
static blender::float3 get_brush_color(const Paint *paint,
                                       const Brush *brush,
                                       const StrokeCache &cache,
                                       const ColorPaint4f &paint_color)
{
  blender::float3 brush_color = blender::float3(paint_color.r, paint_color.g, paint_color.b);
  const std::optional<BrushColorJitterSettings> color_jitter_settings =
      BKE_brush_color_jitter_get_settings(paint, brush);
  if (color_jitter_settings) {
    brush_color = BKE_paint_randomize_color(*color_jitter_settings,
                                            *cache.initial_hsv_jitter,
                                            cache.stroke_distance,
                                            cache.pressure,
                                            brush_color);
  }
  return brush_color;
}

static void vpaint_do_draw(const bContext *C,
                           const VPaint &vp,
                           VPaintData &vpd,
                           Object &ob,
                           Mesh &mesh,
                           const Span<bke::pbvh::MeshNode> nodes,
                           const IndexMask &node_mask,
                           GMutableSpan attribute)
{
  SculptSession &ss = *ob.sculpt;
  const StrokeCache &cache = *ss.cache;
  const Brush &brush = *ob.sculpt->cache->brush;
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);

  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  vwpaint::get_brush_alpha_data(
      ss, vp.paint, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_vert_sel = (mesh.editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) !=
                            0;
  const bool use_face_sel = (mesh.editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush.falloff_shape);

  GMutableSpan g_previous_color = vpd.prev_colors;
  GMutableSpan g_stroke_buffer = vpd.stroke_buffer;

  const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, ob);
  const GroupedSpan<int> vert_to_face = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
  VArraySpan<bool> select_vert;
  if (use_vert_sel) {
    select_vert = *attributes.lookup<bool>(".select_vert", bke::AttrDomain::Point);
  }
  VArraySpan<bool> select_poly;
  if (use_face_sel) {
    select_poly = *attributes.lookup<bool>(".select_poly", bke::AttrDomain::Face);
  }

  const blender::float3 brush_color = get_brush_color(&vp.paint, &brush, cache, vpd.paintcol);

  struct LocalData {
    Vector<float> factors;
    Vector<float> distances;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    LocalData &tls = all_tls.local();
    const Span<int> verts = nodes[i].verts();
    tls.factors.resize(verts.size());
    const MutableSpan<float> factors = tls.factors;
    fill_factor_from_hide(hide_vert, verts, factors);
    filter_region_clip_factors(ss, vert_positions, verts, factors);
    if (!select_vert.is_empty()) {
      filter_factors_with_selection(select_vert, verts, factors);
    }

    tls.distances.resize(verts.size());
    const MutableSpan<float> distances = tls.distances;
    calc_brush_distances(
        ss, vert_positions, verts, eBrushFalloffShape(brush.falloff_shape), distances);
    filter_distances_with_radius(cache.radius, distances, factors);
    calc_brush_strength_factors(cache, brush, distances, factors);

    for (const int i : verts.index_range()) {
      const int vert = verts[i];
      if (factors[i] == 0.0f) {
        continue;
      }

      /* Calculate the dot product between ray normal on surface and current vertex
       * (ie splash prevention factor), and only paint front facing verts. */
      float brush_strength = cache.bstrength;
      const float angle_cos = use_normal ? dot_v3v3(sculpt_normal_frontface, vert_normals[vert]) :
                                           1.0f;
      if (!vwpaint::test_brush_angle_falloff(
              brush, vpd.normal_angle_precalc, angle_cos, &brush_strength))
      {
        continue;
      }
      const float brush_fade = factors[i];

      to_static_color_type(vpd.type, [&](auto dummy) {
        using T = decltype(dummy);
        using Color =
            std::conditional_t<std::is_same_v<T, ColorGeometry4f>, ColorPaint4f, ColorPaint4b>;
        using Traits = blender::color::Traits<Color>;
        MutableSpan<Color> colors = attribute.typed<T>().template cast<Color>();
        MutableSpan<Color> previous_color = g_previous_color.typed<T>().template cast<Color>();
        MutableSpan<Color> stroke_buffer = g_stroke_buffer.typed<T>().template cast<Color>();

        Color color_final = fromFloat<Color>(
            ColorPaint4f(brush_color[0], brush_color[1], brush_color[2], 1.0));

        /* If we're painting with a texture, sample the texture color and alpha. */
        float tex_alpha = 1.0;
        if (vpd.is_texbrush) {
          /* NOTE: we may want to paint alpha as vertex color alpha. */

          /* If the active area is being applied for symmetry, flip it
           * across the symmetry axis and rotate it back to the original
           * position in order to project it. This ensures that the
           * brush texture will be oriented correctly.
           * This is the method also used in #sculpt_apply_texture(). */
          float3 position = vpd.vert_positions[vert];
          if (cache.radial_symmetry_pass) {
            position = blender::math::transform_point(cache.symm_rot_mat_inv, position);
          }
          const float3 symm_point = blender::ed::sculpt_paint::symmetry_flip(
              position, cache.mirror_symmetry_pass);

          tex_alpha = paint_and_tex_color_alpha<Color>(vp, vpd, symm_point, &color_final);
        }

        const float final_alpha = Traits::frange * brush_fade * brush_strength * tex_alpha *
                                  brush_alpha_pressure;

        if (vpd.domain == AttrDomain::Point) {
          colors[vert] = vpaint_blend_stroke<Color, Traits>(vp,
                                                            previous_color,
                                                            colors,
                                                            stroke_buffer,
                                                            color_final,
                                                            final_alpha,
                                                            brush_strength,
                                                            vert);
        }
        else {
          /* For each face owning this vert, paint each loop belonging to this vert. */
          for (const int face : vert_to_face[vert]) {
            const int corner = bke::mesh::face_find_corner_from_vert(
                faces[face], corner_verts, vert);
            if (!select_poly.is_empty() && !select_poly[face]) {
              continue;
            }
            colors[corner] = vpaint_blend_stroke<Color, Traits>(vp,
                                                                previous_color,
                                                                colors,
                                                                stroke_buffer,
                                                                color_final,
                                                                final_alpha,
                                                                brush_strength,
                                                                corner);
          }
        }
      });
    }
  });
}

static void vpaint_do_blur(const bContext *C,
                           const VPaint &vp,
                           VPaintData &vpd,
                           Object &ob,
                           Mesh &mesh,
                           const Span<bke::pbvh::MeshNode> nodes,
                           const IndexMask &node_mask,
                           GMutableSpan attribute)
{
  if (vpd.domain == AttrDomain::Point) {
    do_vpaint_brush_blur_verts(C, vp, vpd, ob, mesh, nodes, node_mask, attribute);
  }
  else {
    do_vpaint_brush_blur_loops(C, vp, vpd, ob, mesh, nodes, node_mask, attribute);
  }
}

static void vpaint_paint_leaves(bContext *C,
                                const VPaint &vp,
                                VPaintData &vpd,
                                Object &ob,
                                Mesh &mesh,
                                GMutableSpan attribute,
                                const Span<bke::pbvh::MeshNode> nodes,
                                const IndexMask &node_mask)
{
  const Brush &brush = *ob.sculpt->cache->brush;

  switch ((eBrushVertexPaintType)brush.vertex_brush_type) {
    case VPAINT_BRUSH_TYPE_AVERAGE:
      calculate_average_color(vpd, ob, mesh, brush, attribute, nodes, node_mask);
      vpaint_do_draw(C, vp, vpd, ob, mesh, nodes, node_mask, attribute);
      break;
    case VPAINT_BRUSH_TYPE_DRAW:
      vpaint_do_draw(C, vp, vpd, ob, mesh, nodes, node_mask, attribute);
      break;
    case VPAINT_BRUSH_TYPE_BLUR:
      vpaint_do_blur(C, vp, vpd, ob, mesh, nodes, node_mask, attribute);
      break;
    case VPAINT_BRUSH_TYPE_SMEAR:
      do_vpaint_brush_smear(C, vp, vpd, ob, mesh, nodes, node_mask, attribute);
      break;
  }
}

static void vpaint_do_paint(bContext *C,
                            const VPaint &vp,
                            VPaintData &vpd,
                            Object &ob,
                            Mesh &mesh,
                            const Brush &brush,
                            const ePaintSymmetryFlags symm,
                            const int axis,
                            const int i,
                            const float angle)
{
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  SculptSession &ss = *ob.sculpt;
  ss.cache->radial_symmetry_pass = i;
  SCULPT_cache_calc_brushdata_symm(*ss.cache, symm, axis, angle);

  IndexMaskMemory memory;
  const IndexMask node_mask = vwpaint::pbvh_gather_generic(depsgraph, ob, vp, brush, memory);

  bke::GSpanAttributeWriter attribute = mesh.attributes_for_write().lookup_for_write_span(
      mesh.active_color_attribute);
  BLI_assert(attribute.domain == vpd.domain);

  /* Paint those leaves. */
  vpaint_paint_leaves(C,
                      vp,
                      vpd,
                      ob,
                      mesh,
                      attribute.span,
                      bke::object::pbvh_get(ob)->nodes<bke::pbvh::MeshNode>(),
                      node_mask);

  attribute.finish();
}

static void vpaint_do_radial_symmetry(bContext *C,
                                      const VPaint &vp,
                                      VPaintData &vpd,
                                      Object &ob,
                                      Mesh &mesh,
                                      const Brush &brush,
                                      const ePaintSymmetryFlags symm,
                                      const int axis)
{
  for (int i = 1; i < mesh.radial_symmetry[axis - 'X']; i++) {
    const float angle = (2.0 * M_PI) * i / mesh.radial_symmetry[axis - 'X'];
    vpaint_do_paint(C, vp, vpd, ob, mesh, brush, symm, axis, i, angle);
  }
}

/* near duplicate of: sculpt.cc's,
 * 'do_symmetrical_brush_actions' and 'wpaint_do_symmetrical_brush_actions'. */
static void vpaint_do_symmetrical_brush_actions(bContext *C,
                                                const VPaint &vp,
                                                VPaintData &vpd,
                                                Object &ob)
{
  const Brush &brush = *BKE_paint_brush_for_read(&vp.paint);
  Mesh &mesh = *(Mesh *)ob.data;
  SculptSession &ss = *ob.sculpt;
  StrokeCache &cache = *ss.cache;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  int i = 0;

  /* initial stroke */
  const ePaintSymmetryFlags initial_symm = ePaintSymmetryFlags(0);
  cache.mirror_symmetry_pass = ePaintSymmetryFlags(0);
  vpaint_do_paint(C, vp, vpd, ob, mesh, brush, initial_symm, 'X', 0, 0);
  vpaint_do_radial_symmetry(C, vp, vpd, ob, mesh, brush, initial_symm, 'X');
  vpaint_do_radial_symmetry(C, vp, vpd, ob, mesh, brush, initial_symm, 'Y');
  vpaint_do_radial_symmetry(C, vp, vpd, ob, mesh, brush, initial_symm, 'Z');

  cache.symmetry = symm;

  for (i = 1; i <= symm; i++) {
    if (is_symmetry_iteration_valid(i, symm)) {
      const ePaintSymmetryFlags symm_pass = ePaintSymmetryFlags(i);
      cache.mirror_symmetry_pass = symm_pass;
      cache.radial_symmetry_pass = 0;
      SCULPT_cache_calc_brushdata_symm(cache, symm_pass, 0, 0);

      if (i & (1 << 0)) {
        vpaint_do_paint(C, vp, vpd, ob, mesh, brush, symm_pass, 'X', 0, 0);
        vpaint_do_radial_symmetry(C, vp, vpd, ob, mesh, brush, symm_pass, 'X');
      }
      if (i & (1 << 1)) {
        vpaint_do_paint(C, vp, vpd, ob, mesh, brush, symm_pass, 'Y', 0, 0);
        vpaint_do_radial_symmetry(C, vp, vpd, ob, mesh, brush, symm_pass, 'Y');
      }
      if (i & (1 << 2)) {
        vpaint_do_paint(C, vp, vpd, ob, mesh, brush, symm_pass, 'Z', 0, 0);
        vpaint_do_radial_symmetry(C, vp, vpd, ob, mesh, brush, symm_pass, 'Z');
      }
    }
  }

  copy_v3_v3(cache.last_location, cache.location);
  cache.is_last_valid = true;
}

static void vpaint_stroke_update_step(bContext *C,
                                      wmOperator * /*op*/,
                                      PaintStroke *stroke,
                                      PointerRNA *itemptr)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  VPaintData &vpd = *static_cast<VPaintData *>(paint_stroke_mode_data(stroke));
  VPaint &vp = *ts->vpaint;
  ViewContext &vc = vpd.vc;
  Object &ob = *vc.obact;
  SculptSession &ss = *ob.sculpt;

  ss.cache->stroke_distance = paint_stroke_distance_get(stroke);

  vwpaint::update_cache_variants(C, vp, ob, itemptr);

  float mat[4][4];

  ED_view3d_init_mats_rv3d(&ob, vc.rv3d);

  mul_m4_m4m4(mat, vc.rv3d->persmat, ob.object_to_world().ptr());

  swap_m4m4(vc.rv3d->persmat, mat);

  vpaint_do_symmetrical_brush_actions(C, vp, vpd, ob);

  swap_m4m4(vc.rv3d->persmat, mat);

  BKE_mesh_batch_cache_dirty_tag((Mesh *)ob.data, BKE_MESH_BATCH_DIRTY_ALL);

  Brush &brush = *BKE_paint_brush(&vp.paint);
  if (brush.vertex_brush_type == VPAINT_BRUSH_TYPE_SMEAR) {
    vpd.smear.color_prev = vpd.smear.color_curr;
  }

  /* Calculate pivot for rotation around selection if needed.
   * also needed for "Frame Selected" on last stroke. */
  float loc_world[3];
  mul_v3_m4v3(loc_world, ob.object_to_world().ptr(), ss.cache->location);
  vwpaint::last_stroke_update(loc_world, vp.paint);

  ED_region_tag_redraw(vc.region);

  DEG_id_tag_update((ID *)ob.data, ID_RECALC_GEOMETRY);
}

static void vpaint_stroke_done(const bContext *C, PaintStroke *stroke)
{
  VPaintData *vpd = static_cast<VPaintData *>(paint_stroke_mode_data(stroke));
  Object &ob = *vpd->vc.obact;

  SculptSession &ss = *ob.sculpt;

  if (ss.cache && ss.cache->alt_smooth) {
    ToolSettings *ts = CTX_data_tool_settings(C);
    VPaint &vp = *ts->vpaint;
    vwpaint::smooth_brush_toggle_off(&vp.paint, ss.cache);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, &ob);

  MEM_delete(ob.sculpt->cache);
  ob.sculpt->cache = nullptr;
}

static wmOperatorStatus vpaint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  op->customdata = paint_stroke_new(C,
                                    op,
                                    stroke_get_location_bvh,
                                    vpaint_stroke_test_start,
                                    vpaint_stroke_update_step,
                                    nullptr,
                                    vpaint_stroke_done,
                                    event->type);

  const wmOperatorStatus retval = op->type->modal(C, op, event);
  OPERATOR_RETVAL_CHECK(retval);

  if (retval == OPERATOR_FINISHED) {
    paint_stroke_free(C, op, (PaintStroke *)op->customdata);
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);

  BLI_assert(retval == OPERATOR_RUNNING_MODAL);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus vpaint_exec(bContext *C, wmOperator *op)
{
  op->customdata = paint_stroke_new(C,
                                    op,
                                    stroke_get_location_bvh,
                                    vpaint_stroke_test_start,
                                    vpaint_stroke_update_step,
                                    nullptr,
                                    vpaint_stroke_done,
                                    0);

  paint_stroke_exec(C, op, (PaintStroke *)op->customdata);

  return OPERATOR_FINISHED;
}

static void vpaint_cancel(bContext *C, wmOperator *op)
{
  Object &ob = *CTX_data_active_object(C);
  MEM_delete(ob.sculpt->cache);
  ob.sculpt->cache = nullptr;

  paint_stroke_cancel(C, op, (PaintStroke *)op->customdata);
}

static wmOperatorStatus vpaint_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, (PaintStroke **)&op->customdata);
}

void PAINT_OT_vertex_paint(wmOperatorType *ot)
{
  ot->name = "Vertex Paint";
  ot->idname = "PAINT_OT_vertex_paint";
  ot->description = "Paint a stroke in the active color attribute layer";

  ot->invoke = vpaint_invoke;
  ot->modal = vpaint_modal;
  ot->exec = vpaint_exec;
  ot->poll = vertex_paint_poll;
  ot->cancel = vpaint_cancel;

  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  paint_stroke_operator_properties(ot);
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna,
      "override_location",
      false,
      "Override Location",
      "Override the given \"location\" array by recalculating object space positions from the "
      "provided \"mouse_event\" positions");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Vertex Colors Operator
 * \{ */

namespace blender::ed::sculpt_paint {

template<typename T>
static void fill_bm_face_or_corner_attribute(BMesh &bm,
                                             const T &value,
                                             const AttrDomain domain,
                                             const int cd_offset,
                                             const bool use_vert_sel)
{
  BMFace *f;
  BMIter iter;
  BM_ITER_MESH (f, &iter, &bm, BM_FACES_OF_MESH) {
    BMLoop *l = f->l_first;
    do {
      if (!(use_vert_sel && !BM_elem_flag_test(l->v, BM_ELEM_SELECT))) {
        if (domain == AttrDomain::Corner) {
          *static_cast<T *>(BM_ELEM_CD_GET_VOID_P(l, cd_offset)) = value;
        }
        else if (domain == AttrDomain::Point) {
          *static_cast<T *>(BM_ELEM_CD_GET_VOID_P(l->v, cd_offset)) = value;
        }
      }
    } while ((l = l->next) != f->l_first);
  }
}

template<typename T>
static void fill_mesh_face_or_corner_attribute(Mesh &mesh,
                                               const T &value,
                                               const AttrDomain domain,
                                               const MutableSpan<T> data,
                                               const bool use_vert_sel,
                                               const bool use_face_sel,
                                               const bool affect_alpha)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  VArraySpan<bool> select_vert;
  if (use_vert_sel) {
    select_vert = *attributes.lookup<bool>(".select_vert", bke::AttrDomain::Point);
  }
  VArraySpan<bool> select_poly;
  if (use_face_sel) {
    select_poly = *attributes.lookup<bool>(".select_poly", bke::AttrDomain::Face);
  }

  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  for (const int i : faces.index_range()) {
    if (!select_poly.is_empty() && !select_poly[i]) {
      continue;
    }
    for (const int corner : faces[i]) {
      const int vert = corner_verts[corner];
      if (!select_vert.is_empty() && !select_vert[vert]) {
        continue;
      }
      const int data_index = domain == AttrDomain::Corner ? corner : vert;
      data[data_index].r = value.r;
      data[data_index].g = value.g;
      data[data_index].b = value.b;
      if (affect_alpha) {
        data[data_index].a = value.a;
      }
    }
  }

  BKE_mesh_tessface_clear(&mesh);
}

static void fill_mesh_color(Mesh &mesh,
                            const ColorPaint4f &color,
                            const StringRef name,
                            const bool use_vert_sel,
                            const bool use_face_sel,
                            const bool affect_alpha)
{
  if (BMEditMesh *em = mesh.runtime->edit_mesh.get()) {
    BMesh *bm = em->bm;
    const BMDataLayerLookup attr = BM_data_layer_lookup(*mesh.runtime->edit_mesh->bm, name);
    if (attr.type == bke::AttrType::ColorFloat) {
      fill_bm_face_or_corner_attribute<ColorPaint4f>(
          *bm, color, attr.domain, attr.offset, use_vert_sel);
    }
    else if (attr.type == bke::AttrType::ColorByte) {
      fill_bm_face_or_corner_attribute<ColorPaint4b>(
          *bm, blender::color::encode(color), attr.domain, attr.offset, use_vert_sel);
    }
  }
  else {
    bke::GSpanAttributeWriter attribute = mesh.attributes_for_write().lookup_for_write_span(name);
    if (attribute.span.type().is<ColorGeometry4f>()) {
      fill_mesh_face_or_corner_attribute<ColorPaint4f>(
          mesh,
          color,
          attribute.domain,
          attribute.span.typed<ColorGeometry4f>().cast<ColorPaint4f>(),
          use_vert_sel,
          use_face_sel,
          affect_alpha);
    }
    else if (attribute.span.type().is<ColorGeometry4b>()) {
      fill_mesh_face_or_corner_attribute<ColorPaint4b>(
          mesh,
          blender::color::encode(color),
          attribute.domain,
          attribute.span.typed<ColorGeometry4b>().cast<ColorPaint4b>(),
          use_vert_sel,
          use_face_sel,
          affect_alpha);
    }
    attribute.finish();
  }
}

static bool fill_active_color(Object &ob,
                              ColorPaint4f fill_color,
                              bool only_selected = true,
                              bool affect_alpha = true)
{
  Mesh *mesh = BKE_mesh_from_object(&ob);
  if (!mesh) {
    return false;
  }

  const bool use_face_sel = only_selected ? (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) != 0 : false;
  const bool use_vert_sel = only_selected ? (mesh->editflag & ME_EDIT_PAINT_VERT_SEL) != 0 : false;
  fill_mesh_color(
      *mesh, fill_color, mesh->active_color_attribute, use_vert_sel, use_face_sel, affect_alpha);

  DEG_id_tag_update(&mesh->id, ID_RECALC_SYNC_TO_EVAL);

  /* NOTE: Original mesh is used for display, so tag it directly here. */
  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);

  return true;
}

bool object_active_color_fill(Object &ob, const float fill_color[4], bool only_selected)
{
  return fill_active_color(ob, ColorPaint4f(fill_color), only_selected);
}

}  // namespace blender::ed::sculpt_paint

static wmOperatorStatus vertex_color_set_exec(bContext *C, wmOperator *op)
{
  using namespace blender::ed::sculpt_paint;
  Scene &scene = *CTX_data_scene(C);
  Object &obact = *CTX_data_active_object(C);
  if (!BKE_mesh_from_object(&obact)) {
    return OPERATOR_CANCELLED;
  }

  ColorPaint4f paintcol = vpaint_get_current_col(*scene.toolsettings->vpaint, false);
  const bool affect_alpha = RNA_boolean_get(op->ptr, "use_alpha");

  /* Ensure valid sculpt state. */
  BKE_sculpt_update_object_for_edit(CTX_data_ensure_evaluated_depsgraph(C), &obact, true);

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(obact);

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

  Mesh &mesh = *static_cast<Mesh *>(obact.data);

  fill_active_color(obact, paintcol, true, affect_alpha);

  pbvh.tag_attribute_changed(node_mask, mesh.active_color_attribute);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, &obact);
  return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_color_set(wmOperatorType *ot)
{
  ot->name = "Set Vertex Colors";
  ot->idname = "PAINT_OT_vertex_color_set";
  ot->description = "Fill the active vertex color layer with the current paint color";

  ot->exec = vertex_color_set_exec;
  ot->poll = vertex_paint_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "use_alpha",
                  true,
                  "Affect Alpha",
                  "Set color completely opaque instead of reusing existing alpha");
}

/** \} */
