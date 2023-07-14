/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 *
 * Used for vertex color & weight paint and mode switching.
 *
 * \note This file is already big,
 * use `paint_vertex_color_ops.c` & `paint_vertex_weight_ops.c` for general purpose operators.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_color.hh"
#include "BLI_color_mix.hh"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

/* For IMB_BlendMode only. */
#include "IMB_imbuf.h"

#include "BKE_ccg.h"
#include "bmesh.h"

#include "paint_intern.hh" /* own include */
#include "sculpt_intern.hh"

using blender::IndexRange;
using namespace blender;
using namespace blender::color;
using namespace blender::ed::sculpt_paint; /* For vwpaint namespace. */
using blender::ed::sculpt_paint::vwpaint::NormalAnglePrecalc;

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
    return c.decode();
  }
  else {
    return c;
  }
}

template<typename Color> static Color fromFloat(const ColorPaint4f &c)
{
  if constexpr (std::is_same_v<Color, ColorPaint4b>) {
    return c.encode();
  }
  else {
    return c;
  }
}

/* Use for 'blur' brush, align with PBVH nodes, created and freed on each update. */
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

bool use_normal(const VPaint *vp)
{
  return ((vp->paint.brush->flag & BRUSH_FRONTFACE) != 0) ||
         ((vp->paint.brush->flag & BRUSH_FRONTFACE_FALLOFF) != 0);
}

bool brush_use_accumulate_ex(const Brush *brush, const int ob_mode)
{
  return ((brush->flag & BRUSH_ACCUMULATE) != 0 ||
          (ob_mode == OB_MODE_VERTEX_PAINT ? (brush->vertexpaint_tool == VPAINT_TOOL_SMEAR) :
                                             (brush->weightpaint_tool == WPAINT_TOOL_SMEAR)));
}

bool brush_use_accumulate(const VPaint *vp)
{
  return brush_use_accumulate_ex(vp->paint.brush, vp->paint.runtime.ob_mode);
}

void init_stroke(Depsgraph *depsgraph, Object *ob)
{
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, true);
  SculptSession *ss = ob->sculpt;

  /* Ensure ss->cache is allocated.  It will mostly be initialized in
   * vwpaint::update_cache_invariants and vwpaint::update_cache_variants.
   */
  if (!ss->cache) {
    ss->cache = MEM_new<StrokeCache>(__func__);
  }
}

/* Toggle operator for turning vertex paint mode on or off (copied from sculpt.cc) */
void init_session(Depsgraph *depsgraph, Scene *scene, Object *ob, eObjectMode object_mode)
{
  /* Create persistent sculpt mode data */
  BKE_sculpt_toolsettings_data_ensure(scene);

  BLI_assert(ob->sculpt == nullptr);
  ob->sculpt = MEM_new<SculptSession>(__func__);
  ob->sculpt->mode_type = object_mode;
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, true);

  SCULPT_ensure_valid_pivot(ob, scene);
}

void init_session_data(const ToolSettings *ts, Object *ob)
{
  /* Create maps */
  SculptVertexPaintGeomMap *gmap = nullptr;
  if (ob->mode == OB_MODE_VERTEX_PAINT) {
    gmap = &ob->sculpt->mode.vpaint.gmap;
    BLI_assert(ob->sculpt->mode_type == OB_MODE_VERTEX_PAINT);
  }
  else if (ob->mode == OB_MODE_WEIGHT_PAINT) {
    gmap = &ob->sculpt->mode.wpaint.gmap;
    BLI_assert(ob->sculpt->mode_type == OB_MODE_WEIGHT_PAINT);
  }
  else {
    ob->sculpt->mode_type = (eObjectMode)0;
    BLI_assert(0);
    return;
  }

  Mesh *me = (Mesh *)ob->data;
  const blender::OffsetIndices polys = me->polys();
  const Span<int> corner_verts = me->corner_verts();

  if (gmap->vert_to_loop_indices.is_empty()) {
    gmap->vert_to_loop = blender::bke::mesh::build_vert_to_loop_map(
        corner_verts, me->totvert, gmap->vert_to_loop_offsets, gmap->vert_to_loop_indices);
    gmap->vert_to_poly = blender::bke::mesh::build_vert_to_poly_map(
        polys, corner_verts, me->totvert, gmap->vert_to_poly_offsets, gmap->vert_to_poly_indices);
  }

  /* Create average brush arrays */
  if (ob->mode == OB_MODE_WEIGHT_PAINT) {
    if (!vwpaint::brush_use_accumulate(ts->wpaint)) {
      if (ob->sculpt->mode.wpaint.alpha_weight == nullptr) {
        ob->sculpt->mode.wpaint.alpha_weight = (float *)MEM_callocN(me->totvert * sizeof(float),
                                                                    __func__);
      }
      if (ob->sculpt->mode.wpaint.dvert_prev == nullptr) {
        ob->sculpt->mode.wpaint.dvert_prev = (MDeformVert *)MEM_callocN(
            me->totvert * sizeof(MDeformVert), __func__);
        MDeformVert *dv = ob->sculpt->mode.wpaint.dvert_prev;
        for (int i = 0; i < me->totvert; i++, dv++) {
          /* Use to show this isn't initialized, never apply to the mesh data. */
          dv->flag = 1;
        }
      }
    }
    else {
      MEM_SAFE_FREE(ob->sculpt->mode.wpaint.alpha_weight);
      if (ob->sculpt->mode.wpaint.dvert_prev != nullptr) {
        BKE_defvert_array_free_elems(ob->sculpt->mode.wpaint.dvert_prev, me->totvert);
        MEM_freeN(ob->sculpt->mode.wpaint.dvert_prev);
        ob->sculpt->mode.wpaint.dvert_prev = nullptr;
      }
    }
  }
}

Vector<PBVHNode *> pbvh_gather_generic(Object *ob, VPaint *wp, Sculpt *sd, Brush *brush)
{
  SculptSession *ss = ob->sculpt;
  const bool use_normal = vwpaint::use_normal(wp);
  Vector<PBVHNode *> nodes;

  /* Build a list of all nodes that are potentially within the brush's area of influence */
  if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    SculptSearchSphereData data = {nullptr};
    data.ss = ss;
    data.sd = sd;
    data.radius_squared = ss->cache->radius_squared;
    data.original = true;

    nodes = blender::bke::pbvh::search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data);

    if (use_normal) {
      SCULPT_pbvh_calc_area_normal(brush, ob, nodes, true, ss->cache->sculpt_normal_symm);
    }
    else {
      zero_v3(ss->cache->sculpt_normal_symm);
    }
  }
  else {
    DistRayAABB_Precalc dist_ray_to_aabb_precalc;
    dist_squared_ray_to_aabb_v3_precalc(
        &dist_ray_to_aabb_precalc, ss->cache->location, ss->cache->view_normal);
    SculptSearchCircleData data = {nullptr};
    data.ss = ss;
    data.sd = sd;
    data.radius_squared = ss->cache->radius_squared;
    data.original = true;
    data.dist_ray_to_aabb_precalc = &dist_ray_to_aabb_precalc;

    nodes = blender::bke::pbvh::search_gather(ss->pbvh, SCULPT_search_circle_cb, &data);

    if (use_normal) {
      copy_v3_v3(ss->cache->sculpt_normal_symm, ss->cache->view_normal);
    }
    else {
      zero_v3(ss->cache->sculpt_normal_symm);
    }
  }
  return nodes;
}

void mode_enter_generic(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob, const eObjectMode mode_flag)
{
  ob->mode |= mode_flag;
  Mesh *me = BKE_mesh_from_object(ob);

  /* Same as sculpt mode, make sure we don't have cached derived mesh which
   * points to freed arrays.
   */
  BKE_object_free_derived_caches(ob);

  if (mode_flag == OB_MODE_VERTEX_PAINT) {
    const ePaintMode paint_mode = PAINT_MODE_VERTEX;
    ED_mesh_color_ensure(me, nullptr);

    BKE_paint_ensure(scene->toolsettings, (Paint **)&scene->toolsettings->vpaint);
    Paint *paint = BKE_paint_get_active_from_paintmode(scene, paint_mode);
    ED_paint_cursor_start(paint, vertex_paint_poll);
    BKE_paint_init(bmain, scene, paint_mode, PAINT_CURSOR_VERTEX_PAINT);
  }
  else if (mode_flag == OB_MODE_WEIGHT_PAINT) {
    const ePaintMode paint_mode = PAINT_MODE_WEIGHT;

    BKE_paint_ensure(scene->toolsettings, (Paint **)&scene->toolsettings->wpaint);
    Paint *paint = BKE_paint_get_active_from_paintmode(scene, paint_mode);
    ED_paint_cursor_start(paint, weight_paint_poll);
    BKE_paint_init(bmain, scene, paint_mode, PAINT_CURSOR_WEIGHT_PAINT);

    /* weight paint specific */
    ED_mesh_mirror_spatial_table_end(ob);
    ED_vgroup_sync_from_pose(ob);
  }
  else {
    BLI_assert(0);
  }

  /* Create vertex/weight paint mode session data */
  if (ob->sculpt) {
    if (ob->sculpt->cache) {
      SCULPT_cache_free(ob->sculpt->cache);
      ob->sculpt->cache = nullptr;
    }
    BKE_sculptsession_free(ob);
  }

  vwpaint::init_session(depsgraph, scene, ob, mode_flag);

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

void mode_exit_generic(Object *ob, const eObjectMode mode_flag)
{
  Mesh *me = BKE_mesh_from_object(ob);
  ob->mode &= ~mode_flag;

  if (mode_flag == OB_MODE_VERTEX_PAINT) {
    if (me->editflag & ME_EDIT_PAINT_FACE_SEL) {
      BKE_mesh_flush_select_from_polys(me);
    }
    else if (me->editflag & ME_EDIT_PAINT_VERT_SEL) {
      BKE_mesh_flush_select_from_verts(me);
    }
  }
  else if (mode_flag == OB_MODE_WEIGHT_PAINT) {
    if (me->editflag & ME_EDIT_PAINT_VERT_SEL) {
      BKE_mesh_flush_select_from_verts(me);
    }
    else if (me->editflag & ME_EDIT_PAINT_FACE_SEL) {
      BKE_mesh_flush_select_from_polys(me);
    }
  }
  else {
    BLI_assert(0);
  }

  /* If the cache is not released by a cancel or a done, free it now. */
  if (ob->sculpt && ob->sculpt->cache) {
    SCULPT_cache_free(ob->sculpt->cache);
    ob->sculpt->cache = nullptr;
  }

  BKE_sculptsession_free(ob);

  paint_cursor_delete_textures();

  if (mode_flag == OB_MODE_WEIGHT_PAINT) {
    ED_mesh_mirror_spatial_table_end(ob);
    ED_mesh_mirror_topo_table_end(ob);
  }

  /* Never leave derived meshes behind. */
  BKE_object_free_derived_caches(ob);

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

bool mode_toggle_poll_test(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || ob->type != OB_MESH) {
    return false;
  }
  if (!ob->data || ID_IS_LINKED(ob->data)) {
    return false;
  }
  return true;
}

void smooth_brush_toggle_off(const bContext *C, Paint *paint, StrokeCache *cache)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Brush *brush = BKE_paint_brush(paint);
  /* The current brush should match with what we have stored in the cache. */
  BLI_assert(brush == cache->brush);

  /* Try to switch back to the saved/previous brush. */
  BKE_brush_size_set(scene, brush, cache->saved_smooth_size);
  brush = (Brush *)BKE_libblock_find_name(bmain, ID_BR, cache->saved_active_brush_name);
  if (brush) {
    BKE_paint_brush_set(paint, brush);
  }
}
/* Initialize the stroke cache invariants from operator properties */
void update_cache_invariants(
    bContext *C, VPaint *vp, SculptSession *ss, wmOperator *op, const float mval[2])
{
  StrokeCache *cache;
  const Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  ViewContext *vc = paint_stroke_view_context((PaintStroke *)op->customdata);
  Object *ob = CTX_data_active_object(C);
  float mat[3][3];
  float view_dir[3] = {0.0f, 0.0f, 1.0f};
  int mode;

  /* VW paint needs to allocate stroke cache before update is called. */
  if (!ss->cache) {
    cache = MEM_new<StrokeCache>(__func__);
    ss->cache = cache;
  }
  else {
    cache = ss->cache;
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
    ups->draw_inverted = true;
  }
  else {
    ups->draw_inverted = false;
  }

  if (cache->alt_smooth) {
    vwpaint::smooth_brush_toggle_on(C, &vp->paint, cache);
  }

  copy_v2_v2(cache->mouse, cache->initial_mouse);
  const Brush *brush = vp->paint.brush;
  /* Truly temporary data that isn't stored in properties */
  cache->vc = vc;
  cache->brush = brush;
  cache->first_time = true;

  /* cache projection matrix */
  ED_view3d_ob_project_mat_get(cache->vc->rv3d, ob, cache->projection_mat);

  invert_m4_m4(ob->world_to_object, ob->object_to_world);
  copy_m3_m4(mat, cache->vc->rv3d->viewinv);
  mul_m3_v3(mat, view_dir);
  copy_m3_m4(mat, ob->world_to_object);
  mul_m3_v3(mat, view_dir);
  normalize_v3_v3(cache->true_view_normal, view_dir);

  copy_v3_v3(cache->view_normal, cache->true_view_normal);
  cache->bstrength = BKE_brush_alpha_get(scene, brush);
  cache->is_last_valid = false;

  cache->accum = true;
}

/* Initialize the stroke cache variants from operator properties */
void update_cache_variants(bContext *C, VPaint *vp, Object *ob, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  Brush *brush = BKE_paint_brush(&vp->paint);

  /* This effects the actual brush radius, so things farther away
   * are compared with a larger radius and vice versa. */
  if (cache->first_time) {
    RNA_float_get_array(ptr, "location", cache->true_location);
  }

  RNA_float_get_array(ptr, "mouse", cache->mouse);

  /* XXX: Use pressure value from first brush step for brushes which don't
   * support strokes (grab, thumb). They depends on initial state and
   * brush coord/pressure/etc.
   * It's more an events design issue, which doesn't split coordinate/pressure/angle
   * changing events. We should avoid this after events system re-design */
  if (paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT) || cache->first_time) {
    cache->pressure = RNA_float_get(ptr, "pressure");
  }

  /* Truly temporary data that isn't stored in properties */
  if (cache->first_time) {
    cache->initial_radius = paint_calc_object_space_radius(
        cache->vc, cache->true_location, BKE_brush_size_get(scene, brush));
    BKE_brush_unprojected_radius_set(scene, brush, cache->initial_radius);
  }

  if (BKE_brush_use_size_pressure(brush) && paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT))
  {
    cache->radius = cache->initial_radius * cache->pressure;
  }
  else {
    cache->radius = cache->initial_radius;
  }

  cache->radius_squared = cache->radius * cache->radius;

  if (ss->pbvh) {
    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateRedraw | PBVH_UpdateBB);
  }
}

void get_brush_alpha_data(const Scene *scene,
                          const SculptSession *ss,
                          const Brush *brush,
                          float *r_brush_size_pressure,
                          float *r_brush_alpha_value,
                          float *r_brush_alpha_pressure)
{
  *r_brush_size_pressure = BKE_brush_size_get(scene, brush) *
                           (BKE_brush_use_size_pressure(brush) ? ss->cache->pressure : 1.0f);
  *r_brush_alpha_value = BKE_brush_alpha_get(scene, brush);
  *r_brush_alpha_pressure = (BKE_brush_use_alpha_pressure(brush) ? ss->cache->pressure : 1.0f);
}

void last_stroke_update(Scene *scene, const float location[3])
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  ups->average_stroke_counter++;
  add_v3_v3(ups->average_stroke_accum, location);
  ups->last_stroke_valid = true;
}

/* -------------------------------------------------------------------- */
void smooth_brush_toggle_on(const bContext *C, Paint *paint, StrokeCache *cache)
{
  Scene *scene = CTX_data_scene(C);
  Brush *brush = paint->brush;
  int cur_brush_size = BKE_brush_size_get(scene, brush);

  STRNCPY(cache->saved_active_brush_name, brush->id.name + 2);

  /* Switch to the blur (smooth) brush. */
  brush = BKE_paint_toolslots_brush_get(paint, WPAINT_TOOL_BLUR);
  if (brush) {
    BKE_paint_brush_set(paint, brush);
    cache->saved_smooth_size = BKE_brush_size_get(scene, brush);
    BKE_brush_size_set(scene, brush, cur_brush_size);
    BKE_curvemapping_init(brush->curve);
  }
}
/** \} */
}  // namespace blender::editors::vwpaint

bool vertex_paint_mode_poll(bContext *C)
{
  const Object *ob = CTX_data_active_object(C);
  if (!ob) {
    return false;
  }
  const Mesh *mesh = static_cast<const Mesh *>(ob->data);

  if (!(ob->mode == OB_MODE_VERTEX_PAINT && mesh->totpoly)) {
    return false;
  }

  if (!mesh->attributes().contains(mesh->active_color_attribute)) {
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

static ColorPaint4f vpaint_get_current_col(Scene *scene, VPaint *vp, bool secondary)
{
  const Brush *brush = BKE_paint_brush_for_read(&vp->paint);
  float color[4];
  const float *brush_color = secondary ? BKE_brush_secondary_color_get(scene, brush) :
                                         BKE_brush_color_get(scene, brush);
  IMB_colormanagement_srgb_to_scene_linear_v3(color, brush_color);

  color[3] = 1.0f; /* alpha isn't used, could even be removed to speedup paint a little */

  return ColorPaint4f(color);
}

/* wpaint has 'wpaint_blend' */
template<typename Color, typename Traits>
static Color vpaint_blend(const VPaint *vp,
                          Color color_curr,
                          Color color_orig,
                          Color color_paint,
                          const typename Traits::ValueType alpha,
                          const typename Traits::BlendType brush_alpha_value)
{
  using Value = typename Traits::ValueType;

  const Brush *brush = vp->paint.brush;
  const IMB_BlendMode blend = (IMB_BlendMode)brush->blend;

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

  if ((brush->flag & BRUSH_LOCK_ALPHA) && !ELEM(blend, IMB_BLEND_ERASE_ALPHA, IMB_BLEND_ADD_ALPHA))
  {
    Value *cp, *cc;
    cp = (Value *)&color_blend;
    cc = (Value *)&color_curr;
    cp[3] = cc[3];
  }

  return color_blend;
}

static void paint_and_tex_color_alpha_intern(VPaint *vp,
                                             const ViewContext *vc,
                                             const float co[3],
                                             float r_rgba[4])
{
  const Brush *brush = BKE_paint_brush(&vp->paint);
  const MTex *mtex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);
  BLI_assert(mtex->tex != nullptr);
  if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
    BKE_brush_sample_tex_3d(vc->scene, brush, mtex, co, r_rgba, 0, nullptr);
  }
  else {
    float co_ss[2]; /* screenspace */
    if (ED_view3d_project_float_object(
            vc->region,
            co,
            co_ss,
            (eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
    {
      const float co_ss_3d[3] = {co_ss[0], co_ss[1], 0.0f}; /* we need a 3rd empty value */
      BKE_brush_sample_tex_3d(vc->scene, brush, mtex, co_ss_3d, r_rgba, 0, nullptr);
    }
    else {
      zero_v4(r_rgba);
    }
  }
}

static void vertex_paint_init_stroke(Scene *scene, Depsgraph *depsgraph, Object *ob)
{
  vwpaint::init_stroke(depsgraph, ob);

  SculptSession *ss = ob->sculpt;
  ToolSettings *ts = scene->toolsettings;

  /* Allocate scratch array for previous colors if needed. */
  if (!vwpaint::brush_use_accumulate(ts->vpaint)) {
    if (ss->cache->prev_colors_vpaint.is_empty()) {
      const Mesh *mesh = BKE_object_get_original_mesh(ob);
      const GVArray attribute = *mesh->attributes().lookup(mesh->active_color_attribute);
      ss->cache->prev_colors_vpaint = GArray(attribute.type(), attribute.size());
      attribute.type().value_initialize_n(ss->cache->prev_colors_vpaint.data(),
                                          ss->cache->prev_colors_vpaint.size());
    }
  }
  else {
    ss->cache->prev_colors_vpaint = {};
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enter Vertex Paint Mode
 * \{ */

void ED_object_vpaintmode_enter_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  vwpaint::mode_enter_generic(bmain, depsgraph, scene, ob, OB_MODE_VERTEX_PAINT);
}
void ED_object_vpaintmode_enter(bContext *C, Depsgraph *depsgraph)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  ED_object_vpaintmode_enter_ex(bmain, depsgraph, scene, ob);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exit Vertex Paint Mode
 * \{ */

void ED_object_vpaintmode_exit_ex(Object *ob)
{
  vwpaint::mode_exit_generic(ob, OB_MODE_VERTEX_PAINT);
}
void ED_object_vpaintmode_exit(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  ED_object_vpaintmode_exit_ex(ob);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Vertex Paint Operator
 * \{ */

/**
 * \note Keep in sync with #wpaint_mode_toggle_exec
 */
static int vpaint_mode_toggle_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  Object *ob = CTX_data_active_object(C);
  const int mode_flag = OB_MODE_VERTEX_PAINT;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, (eObjectMode)mode_flag, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  Mesh *me = BKE_mesh_from_object(ob);

  /* toggle: end vpaint */
  if (is_mode_set) {
    ED_object_vpaintmode_exit_ex(ob);
  }
  else {
    Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);
    if (depsgraph) {
      depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    }
    ED_object_vpaintmode_enter_ex(bmain, depsgraph, scene, ob);
    BKE_paint_toolslots_brush_validate(bmain, &ts->vpaint->paint);
  }

  BKE_mesh_batch_cache_dirty_tag((Mesh *)ob->data, BKE_MESH_BATCH_DIRTY_ALL);

  /* update modifier stack for mapping requirements */
  DEG_id_tag_update(&me->id, 0);

  WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);
  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_paint_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Mode";
  ot->idname = "PAINT_OT_vertex_paint_toggle";
  ot->description = "Toggle the vertex paint mode in 3D view";

  /* api callbacks */
  ot->exec = vpaint_mode_toggle_exec;
  ot->poll = vwpaint::mode_toggle_poll_test;

  /* flags */
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
static void to_static_color_type(const eCustomDataType type, const Func &func)
{
  switch (type) {
    case CD_PROP_COLOR:
      func(ColorGeometry4f());
      break;
    case CD_PROP_BYTE_COLOR:
      func(ColorGeometry4b());
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

struct VPaintData {
  ViewContext vc;
  eAttrDomain domain;
  eCustomDataType type;

  NormalAnglePrecalc normal_angle_precalc;

  ColorPaint4f paintcol;

  VertProjHandle *vp_handle;
  CoNo *vertexcosnos;

  bool is_texbrush;

  /* Special storage for smear brush, avoid feedback loop - update each step. */
  struct {
    GArray<> color_prev;
    GArray<> color_curr;
  } smear;
};

static VPaintData *vpaint_init_vpaint(bContext *C,
                                      wmOperator *op,
                                      Scene *scene,
                                      Depsgraph *depsgraph,
                                      VPaint *vp,
                                      Object *ob,
                                      Mesh *me,
                                      const eAttrDomain domain,
                                      const eCustomDataType type,
                                      const Brush *brush)
{
  VPaintData *vpd = MEM_new<VPaintData>(__func__);
  vpd->type = type;
  vpd->domain = domain;

  ED_view3d_viewcontext_init(C, &vpd->vc, depsgraph);
  vwpaint::view_angle_limits_init(&vpd->normal_angle_precalc,
                                  vp->paint.brush->falloff_angle,
                                  (vp->paint.brush->flag & BRUSH_FRONTFACE_FALLOFF) != 0);

  vpd->paintcol = vpaint_get_current_col(
      scene, vp, (RNA_enum_get(op->ptr, "mode") == BRUSH_STROKE_INVERT));

  vpd->is_texbrush = !(brush->vertexpaint_tool == VPAINT_TOOL_BLUR) && brush->mtex.tex;

  if (brush->vertexpaint_tool == VPAINT_TOOL_SMEAR) {
    const GVArray attribute = *me->attributes().lookup(me->active_color_attribute, domain);
    vpd->smear.color_prev = GArray(attribute.type(), attribute.size());
    attribute.materialize(vpd->smear.color_prev.data());

    vpd->smear.color_curr = vpd->smear.color_prev;
  }

  /* Create projection handle */
  if (vpd->is_texbrush) {
    ob->sculpt->building_vp_handle = true;
    vpd->vp_handle = ED_vpaint_proj_handle_create(depsgraph, scene, ob, &vpd->vertexcosnos);
    ob->sculpt->building_vp_handle = false;
  }

  return vpd;
}

static bool vpaint_stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  PaintStroke *stroke = (PaintStroke *)op->customdata;
  VPaint *vp = ts->vpaint;
  Brush *brush = BKE_paint_brush(&vp->paint);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* context checks could be a poll() */
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr || me->totpoly == 0) {
    return false;
  }

  ED_mesh_color_ensure(me, nullptr);

  const std::optional<bke::AttributeMetaData> meta_data = *me->attributes().lookup_meta_data(
      me->active_color_attribute);

  if (!meta_data) {
    return false;
  }

  VPaintData *vpd = vpaint_init_vpaint(
      C, op, scene, depsgraph, vp, ob, me, meta_data->domain, meta_data->data_type, brush);

  paint_stroke_set_mode_data(stroke, vpd);

  /* If not previously created, create vertex/weight paint mode session data */
  vertex_paint_init_stroke(scene, depsgraph, ob);
  vwpaint::update_cache_invariants(C, vp, ss, op, mouse);
  vwpaint::init_session_data(ts, ob);

  return true;
}

static void do_vpaint_brush_blur_loops(bContext *C,
                                       Sculpt * /*sd*/,
                                       VPaint *vp,
                                       VPaintData *vpd,
                                       Object *ob,
                                       Mesh *me,
                                       Span<PBVHNode *> nodes,
                                       GMutableSpan attribute)
{
  SculptSession *ss = ob->sculpt;

  const Brush *brush = ob->sculpt->cache->brush;
  const Scene *scene = CTX_data_scene(C);

  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);

  const SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;
  const StrokeCache *cache = ss->cache;
  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  vwpaint::get_brush_alpha_data(
      scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_vert_sel = (me->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) !=
                            0;
  const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

  SculptBrushTest test_init;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test_init, brush->falloff_shape);
  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush->falloff_shape);

  GMutableSpan g_previous_color = ss->cache->prev_colors_vpaint;

  const blender::VArray<bool> select_vert = *me->attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  const blender::VArray<bool> select_poly = *me->attributes().lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  blender::threading::parallel_for(nodes.index_range(), 1LL, [&](IndexRange range) {
    SculptBrushTest test = test_init;
    for (int n : range) {
      /* For each vertex */
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
        /* Test to see if the vertex coordinates are within the spherical brush region. */
        if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
          continue;
        }
        /* For grid based pbvh, take the vert whose loop corresponds to the current grid.
         * Otherwise, take the current vert. */
        const int v_index = has_grids ? ss->corner_verts[vd.grid_indices[vd.g]] :
                                        vd.vert_indices[vd.i];
        const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;

        /* If the vertex is selected for painting. */
        if (use_vert_sel && !select_vert[v_index]) {
          continue;
        }

        float brush_strength = cache->bstrength;
        const float angle_cos = (use_normal && vd.no) ? dot_v3v3(sculpt_normal_frontface, vd.no) :
                                                        1.0f;
        if (!vwpaint::test_brush_angle_falloff(
                *brush, vpd->normal_angle_precalc, angle_cos, &brush_strength))
        {
          continue;
        }

        const float brush_fade = BKE_brush_curve_strength(brush, sqrtf(test.dist), cache->radius);

        to_static_color_type(vpd->type, [&](auto dummy) {
          using T = decltype(dummy);
          using Color =
              std::conditional_t<std::is_same_v<T, ColorGeometry4f>, ColorPaint4f, ColorPaint4b>;
          using Traits = color::Traits<Color>;
          using Blend = typename Traits::BlendType;
          MutableSpan<Color> previous_color = g_previous_color.typed<T>().template cast<Color>();
          MutableSpan<Color> colors = attribute.typed<T>().template cast<Color>();
          /* Get the average poly color */
          Color color_final(0, 0, 0, 0);

          int total_hit_loops = 0;
          Blend blend[4] = {0};

          for (const int p_index : gmap->vert_to_poly[v_index]) {
            if (use_face_sel && !select_poly[p_index]) {
              return;
            }
            const blender::IndexRange poly = ss->polys[p_index];
            total_hit_loops += poly.size();
            for (const int corner : poly) {
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

          /* For each poly owning this vert,
           * paint each loop belonging to this vert. */
          for (const int j : gmap->vert_to_poly[v_index].index_range()) {
            const int p_index = gmap->vert_to_poly[v_index][j];
            const int l_index = gmap->vert_to_loop[v_index][j];
            BLI_assert(ss->corner_verts[l_index] == v_index);
            if (use_face_sel && !select_poly[p_index]) {
              continue;
            }
            Color color_orig(0, 0, 0, 0); /* unused when array is nullptr */

            if (!previous_color.is_empty()) {
              /* Get the previous loop color */
              if (isZero(previous_color[l_index])) {
                previous_color[l_index] = colors[l_index];
              }
              color_orig = previous_color[l_index];
            }
            const float final_alpha = Traits::range * brush_fade * brush_strength *
                                      brush_alpha_pressure * grid_alpha;
            /* Mix the new color with the original
             * based on the brush strength and the curve. */
            colors[l_index] = vpaint_blend<Color, Traits>(vp,
                                                          colors[l_index],
                                                          color_orig,
                                                          *col,
                                                          final_alpha,
                                                          Traits::range * brush_strength);
          }
        });
      }
      BKE_pbvh_vertex_iter_end;
    };
  });
}

static void do_vpaint_brush_blur_verts(bContext *C,
                                       Sculpt * /*sd*/,
                                       VPaint *vp,
                                       VPaintData *vpd,
                                       Object *ob,
                                       Mesh *me,
                                       Span<PBVHNode *> nodes,
                                       GMutableSpan attribute)
{
  SculptSession *ss = ob->sculpt;

  const Brush *brush = ob->sculpt->cache->brush;
  const Scene *scene = CTX_data_scene(C);

  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);

  const SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;
  const StrokeCache *cache = ss->cache;
  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  vwpaint::get_brush_alpha_data(
      scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_vert_sel = (me->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) !=
                            0;
  const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

  SculptBrushTest test_init;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test_init, brush->falloff_shape);
  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush->falloff_shape);

  GMutableSpan g_previous_color = ss->cache->prev_colors_vpaint;

  const blender::VArray<bool> select_vert = *me->attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  const blender::VArray<bool> select_poly = *me->attributes().lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  blender::threading::parallel_for(nodes.index_range(), 1LL, [&](IndexRange range) {
    SculptBrushTest test = test_init;
    for (int n : range) {
      /* For each vertex */
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
        /* Test to see if the vertex coordinates are within the spherical brush region. */
        if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
          continue;
        }
        /* For grid based pbvh, take the vert whose loop corresponds to the current grid.
         * Otherwise, take the current vert. */
        const int v_index = has_grids ? ss->corner_verts[vd.grid_indices[vd.g]] :
                                        vd.vert_indices[vd.i];
        const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;

        /* If the vertex is selected for painting. */
        if (use_vert_sel && !select_vert[v_index]) {
          continue;
        }

        float brush_strength = cache->bstrength;
        const float angle_cos = (use_normal && vd.no) ? dot_v3v3(sculpt_normal_frontface, vd.no) :
                                                        1.0f;
        if (!vwpaint::test_brush_angle_falloff(
                *brush, vpd->normal_angle_precalc, angle_cos, &brush_strength))
        {
          continue;
        }
        const float brush_fade = BKE_brush_curve_strength(brush, sqrtf(test.dist), cache->radius);

        /* Get the average poly color */
        to_static_color_type(vpd->type, [&](auto dummy) {
          using T = decltype(dummy);
          using Color =
              std::conditional_t<std::is_same_v<T, ColorGeometry4f>, ColorPaint4f, ColorPaint4b>;
          using Traits = color::Traits<Color>;
          using Blend = typename Traits::BlendType;
          MutableSpan<Color> previous_color = g_previous_color.typed<T>().template cast<Color>();
          MutableSpan<Color> colors = attribute.typed<T>().template cast<Color>();
          Color color_final(0, 0, 0, 0);

          int total_hit_loops = 0;
          Blend blend[4] = {0};

          for (const int p_index : gmap->vert_to_poly[v_index]) {
            if (use_face_sel && !select_poly[p_index]) {
              continue;
            }
            const blender::IndexRange poly = ss->polys[p_index];
            total_hit_loops += poly.size();
            for (const int vert : ss->corner_verts.slice(poly)) {
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
            if (isZero(previous_color[v_index])) {
              previous_color[v_index] = colors[v_index];
            }
            color_orig = previous_color[v_index];
          }
          const float final_alpha = Traits::range * brush_fade * brush_strength *
                                    brush_alpha_pressure * grid_alpha;
          /* Mix the new color with the original
           * based on the brush strength and the curve. */
          colors[v_index] = vpaint_blend<Color, Traits>(vp,
                                                        colors[v_index],
                                                        color_orig,
                                                        color_final,
                                                        final_alpha,
                                                        Traits::range * brush_strength);
        });
      }
      BKE_pbvh_vertex_iter_end;
    };
  });
}

static void do_vpaint_brush_smear(bContext *C,
                                  Sculpt * /*sd*/,
                                  VPaint *vp,
                                  VPaintData *vpd,
                                  Object *ob,
                                  Mesh *me,
                                  Span<PBVHNode *> nodes,
                                  GMutableSpan attribute)
{
  SculptSession *ss = ob->sculpt;

  const SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;
  const StrokeCache *cache = ss->cache;
  if (!cache->is_last_valid) {
    return;
  }
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);

  const Brush *brush = ob->sculpt->cache->brush;
  const Scene *scene = CTX_data_scene(C);
  GMutableSpan g_color_curr = vpd->smear.color_curr;
  GMutableSpan g_color_prev_smear = vpd->smear.color_prev;
  GMutableSpan g_color_prev = ss->cache->prev_colors_vpaint;

  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;

  vwpaint::get_brush_alpha_data(
      scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_vert_sel = (me->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) !=
                            0;
  const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

  float brush_dir[3];
  sub_v3_v3v3(brush_dir, cache->location, cache->last_location);
  project_plane_v3_v3v3(brush_dir, brush_dir, cache->view_normal);
  if (normalize_v3(brush_dir) == 0.0f) {
    return;
  }

  SculptBrushTest test_init;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test_init, brush->falloff_shape);
  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush->falloff_shape);

  const blender::VArray<bool> select_vert = *me->attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  const blender::VArray<bool> select_poly = *me->attributes().lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  blender::threading::parallel_for(nodes.index_range(), 1LL, [&](IndexRange range) {
    SculptBrushTest test = test_init;
    for (int n : range) {
      /* For each vertex */
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
        /* Test to see if the vertex coordinates are within the spherical brush region. */
        if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
          continue;
        }
        /* For grid based pbvh, take the vert whose loop corresponds to the current grid.
         * Otherwise, take the current vert. */
        const int v_index = has_grids ? ss->corner_verts[vd.grid_indices[vd.g]] :
                                        vd.vert_indices[vd.i];
        const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;
        const float3 &mv_curr = ss->vert_positions[v_index];

        /* if the vertex is selected for painting. */
        if (use_vert_sel && !select_vert[v_index]) {
          continue;
        }

        /* Calculate the dot prod. between ray norm on surf and current vert
         * (ie splash prevention factor), and only paint front facing verts. */
        float brush_strength = cache->bstrength;
        const float angle_cos = (use_normal && vd.no) ? dot_v3v3(sculpt_normal_frontface, vd.no) :
                                                        1.0f;
        if (!vwpaint::test_brush_angle_falloff(
                *brush, vpd->normal_angle_precalc, angle_cos, &brush_strength))
        {
          continue;
        }
        const float brush_fade = BKE_brush_curve_strength(brush, sqrtf(test.dist), cache->radius);

        bool do_color = false;
        /* Minimum dot product between brush direction and current
         * to neighbor direction is 0.0, meaning orthogonal. */
        float stroke_dot_max = 0.0f;

        /* Get the color of the loop in the opposite
         * direction of the brush movement */
        to_static_color_type(vpd->type, [&](auto dummy) {
          using T = decltype(dummy);
          using Color =
              std::conditional_t<std::is_same_v<T, ColorGeometry4f>, ColorPaint4f, ColorPaint4b>;
          using Traits = color::Traits<Color>;
          MutableSpan<Color> color_curr = g_color_curr.typed<T>().template cast<Color>();
          MutableSpan<Color> color_prev_smear =
              g_color_prev_smear.typed<T>().template cast<Color>();
          MutableSpan<Color> color_prev = g_color_prev.typed<T>().template cast<Color>();
          MutableSpan<Color> colors = attribute.typed<T>().template cast<Color>();

          Color color_final(0, 0, 0, 0);

          for (const int j : gmap->vert_to_poly[v_index].index_range()) {
            const int p_index = gmap->vert_to_poly[v_index][j];
            const int l_index = gmap->vert_to_loop[v_index][j];
            BLI_assert(ss->corner_verts[l_index] == v_index);
            UNUSED_VARS_NDEBUG(l_index);
            if (use_face_sel && !select_poly[p_index]) {
              continue;
            }
            for (const int corner : ss->polys[p_index]) {
              const int v_other_index = ss->corner_verts[corner];
              if (v_other_index == v_index) {
                continue;
              }

              /* Get the direction from the
               * selected vert to the neighbor. */
              float other_dir[3];
              sub_v3_v3v3(other_dir, mv_curr, ss->vert_positions[v_other_index]);
              project_plane_v3_v3v3(other_dir, other_dir, cache->view_normal);

              normalize_v3(other_dir);

              const float stroke_dot = dot_v3v3(other_dir, brush_dir);
              int elem_index;

              if (vpd->domain == ATTR_DOMAIN_POINT) {
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
                                    brush_alpha_pressure * grid_alpha;

          /* For each poly owning this vert,
           * paint each loop belonging to this vert. */
          for (const int j : gmap->vert_to_poly[v_index].index_range()) {
            const int p_index = gmap->vert_to_poly[v_index][j];

            int elem_index;
            if (vpd->domain == ATTR_DOMAIN_POINT) {
              elem_index = v_index;
            }
            else {
              const int l_index = gmap->vert_to_loop[v_index][j];
              elem_index = l_index;
              BLI_assert(ss->corner_verts[l_index] == v_index);
            }
            if (use_face_sel && !select_poly[p_index]) {
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
      BKE_pbvh_vertex_iter_end;
    }
  });
}

static void calculate_average_color(VPaintData *vpd,
                                    Object *ob,
                                    Mesh *me,
                                    const Brush *brush,
                                    const GSpan attribute,
                                    Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);
  const bool has_grids = (pbvh_type == PBVH_GRIDS);
  const SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;

  StrokeCache *cache = ss->cache;
  const bool use_vert_sel = (me->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) !=
                            0;

  SculptBrushTest test_init;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test_init, brush->falloff_shape);

  const blender::VArray<bool> select_vert = *me->attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);

  to_static_color_type(vpd->type, [&](auto dummy) {
    using T = decltype(dummy);
    using Color =
        std::conditional_t<std::is_same_v<T, ColorGeometry4f>, ColorPaint4f, ColorPaint4b>;
    using Traits = color::Traits<Color>;
    using Blend = typename Traits::BlendType;
    const Span<Color> colors = attribute.typed<T>().template cast<Color>();

    Array<VPaintAverageAccum<Blend>> accum(nodes.size());
    blender::threading::parallel_for(nodes.index_range(), 1LL, [&](IndexRange range) {
      SculptBrushTest test = test_init;
      for (int n : range) {
        VPaintAverageAccum<Blend> &accum2 = accum[n];
        accum2.len = 0;
        memset(accum2.value, 0, sizeof(accum2.value));

        /* For each vertex */
        PBVHVertexIter vd;
        BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
          /* Test to see if the vertex coordinates are within the spherical brush region. */
          if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
            continue;
          }
          if (BKE_brush_curve_strength(brush, 0.0, cache->radius) <= 0.0f) {
            continue;
          }
          const int v_index = has_grids ? ss->corner_verts[vd.grid_indices[vd.g]] :
                                          vd.vert_indices[vd.i];
          /* If the vertex is selected for painting. */
          if (use_vert_sel && !select_vert[v_index]) {
            continue;
          }

          accum2.len += gmap->vert_to_poly[v_index].size();
          /* if a vertex is within the brush region, then add its color to the blend. */
          for (int j = 0; j < gmap->vert_to_poly[v_index].size(); j++) {
            int elem_index;

            if (vpd->domain == ATTR_DOMAIN_CORNER) {
              elem_index = gmap->vert_to_loop[v_index][j];
            }
            else {
              elem_index = v_index;
            }

            /* Color is squared to compensate the `sqrt` color encoding. */
            const Color &col = colors[elem_index];
            accum2.value[0] += col.r * col.r;
            accum2.value[1] += col.g * col.g;
            accum2.value[2] += col.b * col.b;
          }
        }
        BKE_pbvh_vertex_iter_end;
      }
    });

    Blend accum_len = 0;
    Blend accum_value[3] = {0};
    Color blend(0, 0, 0, 0);

    for (int i = 0; i < nodes.size(); i++) {
      accum_len += accum[i].len;
      accum_value[0] += accum[i].value[0];
      accum_value[1] += accum[i].value[1];
      accum_value[2] += accum[i].value[2];
    }
    if (accum_len != 0) {
      blend.r = Traits::round(sqrtf(Traits::divide_round(accum_value[0], accum_len)));
      blend.g = Traits::round(sqrtf(Traits::divide_round(accum_value[1], accum_len)));
      blend.b = Traits::round(sqrtf(Traits::divide_round(accum_value[2], accum_len)));
      blend.a = Traits::range;

      vpd->paintcol = toFloat(blend);
    }
  });
}

template<typename Color>
static float paint_and_tex_color_alpha(VPaint *vp,
                                       VPaintData *vpd,
                                       const float v_co[3],
                                       Color *r_color)
{
  ColorPaint4f rgba;
  ColorPaint4f rgba_br = toFloat(*r_color);

  paint_and_tex_color_alpha_intern(vp, &vpd->vc, v_co, &rgba.r);

  rgb_uchar_to_float(&rgba_br.r, (const uchar *)&vpd->paintcol);
  mul_v3_v3(rgba_br, rgba);

  *r_color = fromFloat<Color>(rgba_br);
  return rgba[3];
}

static void vpaint_do_draw(bContext *C,
                           Sculpt * /*sd*/,
                           VPaint *vp,
                           VPaintData *vpd,
                           Object *ob,
                           Mesh *me,
                           Span<PBVHNode *> nodes,
                           GMutableSpan attribute)
{
  SculptSession *ss = ob->sculpt;
  const PBVHType pbvh_type = BKE_pbvh_type(ss->pbvh);

  const Brush *brush = ob->sculpt->cache->brush;
  const Scene *scene = CTX_data_scene(C);

  const bool has_grids = (pbvh_type == PBVH_GRIDS);
  const SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;

  const StrokeCache *cache = ss->cache;
  float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
  vwpaint::get_brush_alpha_data(
      scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
  const bool use_normal = vwpaint::use_normal(vp);
  const bool use_vert_sel = (me->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) !=
                            0;
  const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

  SculptBrushTest test_init;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test_init, brush->falloff_shape);
  const float *sculpt_normal_frontface = SCULPT_brush_frontface_normal_from_falloff_shape(
      ss, brush->falloff_shape);

  GMutableSpan g_previous_color = ss->cache->prev_colors_vpaint;

  const blender::VArray<bool> select_vert = *me->attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  const blender::VArray<bool> select_poly = *me->attributes().lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  blender::threading::parallel_for(nodes.index_range(), 1LL, [&](IndexRange range) {
    for (int n : range) {
      SculptBrushTest test = test_init;
      /* For each vertex */
      PBVHVertexIter vd;
      BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
        /* Test to see if the vertex coordinates are within the spherical brush region. */
        if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
          continue;
        }
        /* NOTE: Grids are 1:1 with corners (aka loops).
         * For grid based pbvh, take the vert whose loop corresponds to the current grid.
         * Otherwise, take the current vert. */
        const int v_index = has_grids ? ss->corner_verts[vd.grid_indices[vd.g]] :
                                        vd.vert_indices[vd.i];
        /* If the vertex is selected for painting. */
        if (use_vert_sel && !select_vert[v_index]) {
          continue;
        }

        const float grid_alpha = has_grids ? 1.0f / vd.gridsize : 1.0f;

        /* Calc the dot prod. between ray norm on surf and current vert
         * (ie splash prevention factor), and only paint front facing verts. */
        float brush_strength = cache->bstrength;
        const float angle_cos = (use_normal && vd.no) ? dot_v3v3(sculpt_normal_frontface, vd.no) :
                                                        1.0f;
        if (!vwpaint::test_brush_angle_falloff(
                *brush, vpd->normal_angle_precalc, angle_cos, &brush_strength))
        {
          continue;
        }
        const float brush_fade = BKE_brush_curve_strength(brush, sqrtf(test.dist), cache->radius);

        to_static_color_type(vpd->type, [&](auto dummy) {
          using T = decltype(dummy);
          using Color =
              std::conditional_t<std::is_same_v<T, ColorGeometry4f>, ColorPaint4f, ColorPaint4b>;
          using Traits = color::Traits<Color>;
          MutableSpan<Color> colors = attribute.typed<T>().template cast<Color>();
          MutableSpan<Color> previous_color = g_previous_color.typed<T>().template cast<Color>();
          Color color_final = fromFloat<Color>(vpd->paintcol);

          /* If we're painting with a texture, sample the texture color and alpha. */
          float tex_alpha = 1.0;
          if (vpd->is_texbrush) {
            /* NOTE: we may want to paint alpha as vertex color alpha. */
            tex_alpha = paint_and_tex_color_alpha<Color>(
                vp, vpd, vpd->vertexcosnos[v_index].co, &color_final);
          }

          Color color_orig(0, 0, 0, 0);

          if (vpd->domain == ATTR_DOMAIN_POINT) {
            int v_index = vd.index;

            if (!previous_color.is_empty()) {
              /* Get the previous loop color */
              if (isZero(previous_color[v_index])) {
                previous_color[v_index] = colors[v_index];
              }
              color_orig = previous_color[v_index];
            }
            const float final_alpha = Traits::frange * brush_fade * brush_strength * tex_alpha *
                                      brush_alpha_pressure * grid_alpha;

            colors[v_index] = vpaint_blend<Color, Traits>(vp,
                                                          colors[v_index],
                                                          color_orig,
                                                          color_final,
                                                          final_alpha,
                                                          Traits::range * brush_strength);
          }
          else {
            /* For each poly owning this vert, paint each loop belonging to this vert. */
            for (const int j : gmap->vert_to_poly[v_index].index_range()) {
              const int p_index = gmap->vert_to_poly[v_index][j];
              const int l_index = gmap->vert_to_loop[v_index][j];
              BLI_assert(ss->corner_verts[l_index] == v_index);
              if (use_face_sel && !select_poly[p_index]) {
                continue;
              }
              Color color_orig = Color(0, 0, 0, 0); /* unused when array is nullptr */

              if (!previous_color.is_empty()) {
                /* Get the previous loop color */
                if (isZero(previous_color[l_index])) {
                  previous_color[l_index] = colors[l_index];
                }
                color_orig = previous_color[l_index];
              }
              const float final_alpha = Traits::frange * brush_fade * brush_strength * tex_alpha *
                                        brush_alpha_pressure * grid_alpha;

              /* Mix the new color with the original based on final_alpha. */
              colors[l_index] = vpaint_blend<Color, Traits>(vp,
                                                            colors[l_index],
                                                            color_orig,
                                                            color_final,
                                                            final_alpha,
                                                            Traits::range * brush_strength);
            }
          }
        });
      }
      BKE_pbvh_vertex_iter_end;
    }
  });
}

static void vpaint_do_blur(bContext *C,
                           Sculpt *sd,
                           VPaint *vp,
                           VPaintData *vpd,
                           Object *ob,
                           Mesh *me,
                           Span<PBVHNode *> nodes,
                           GMutableSpan attribute)
{
  if (vpd->domain == ATTR_DOMAIN_POINT) {
    do_vpaint_brush_blur_verts(C, sd, vp, vpd, ob, me, nodes, attribute);
  }
  else {
    do_vpaint_brush_blur_loops(C, sd, vp, vpd, ob, me, nodes, attribute);
  }
}

static void vpaint_paint_leaves(bContext *C,
                                Sculpt *sd,
                                VPaint *vp,
                                VPaintData *vpd,
                                Object *ob,
                                Mesh *me,
                                GMutableSpan attribute,
                                Span<PBVHNode *> nodes)
{
  for (PBVHNode *node : nodes) {
    SCULPT_undo_push_node(ob, node, SCULPT_UNDO_COLOR);
  }

  const Brush *brush = ob->sculpt->cache->brush;

  switch ((eBrushVertexPaintTool)brush->vertexpaint_tool) {
    case VPAINT_TOOL_AVERAGE:
      calculate_average_color(vpd, ob, me, brush, attribute, nodes);
      vpaint_do_draw(C, sd, vp, vpd, ob, me, nodes, attribute);
      break;
    case VPAINT_TOOL_DRAW:
      vpaint_do_draw(C, sd, vp, vpd, ob, me, nodes, attribute);
      break;
    case VPAINT_TOOL_BLUR:
      vpaint_do_blur(C, sd, vp, vpd, ob, me, nodes, attribute);
      break;
    case VPAINT_TOOL_SMEAR:
      do_vpaint_brush_smear(C, sd, vp, vpd, ob, me, nodes, attribute);
      break;
    default:
      break;
  }
}

static void vpaint_do_paint(bContext *C,
                            Sculpt *sd,
                            VPaint *vp,
                            VPaintData *vpd,
                            Object *ob,
                            Mesh *me,
                            Brush *brush,
                            const ePaintSymmetryFlags symm,
                            const int axis,
                            const int i,
                            const float angle)
{
  SculptSession *ss = ob->sculpt;
  ss->cache->radial_symmetry_pass = i;
  SCULPT_cache_calc_brushdata_symm(ss->cache, symm, axis, angle);

  Vector<PBVHNode *> nodes = vwpaint::pbvh_gather_generic(ob, vp, sd, brush);

  bke::GSpanAttributeWriter attribute = me->attributes_for_write().lookup_for_write_span(
      me->active_color_attribute);
  BLI_assert(attribute.domain == vpd->domain);

  /* Paint those leaves. */
  vpaint_paint_leaves(C, sd, vp, vpd, ob, me, attribute.span, nodes);

  attribute.finish();
}

static void vpaint_do_radial_symmetry(bContext *C,
                                      Sculpt *sd,
                                      VPaint *vp,
                                      VPaintData *vpd,
                                      Object *ob,
                                      Mesh *me,
                                      Brush *brush,
                                      const ePaintSymmetryFlags symm,
                                      const int axis)
{
  for (int i = 1; i < vp->radial_symm[axis - 'X']; i++) {
    const float angle = (2.0 * M_PI) * i / vp->radial_symm[axis - 'X'];
    vpaint_do_paint(C, sd, vp, vpd, ob, me, brush, symm, axis, i, angle);
  }
}

/* near duplicate of: sculpt.cc's,
 * 'do_symmetrical_brush_actions' and 'wpaint_do_symmetrical_brush_actions'. */
static void vpaint_do_symmetrical_brush_actions(
    bContext *C, Sculpt *sd, VPaint *vp, VPaintData *vpd, Object *ob)
{
  Brush *brush = BKE_paint_brush(&vp->paint);
  Mesh *me = (Mesh *)ob->data;
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  int i = 0;

  /* initial stroke */
  const ePaintSymmetryFlags initial_symm = ePaintSymmetryFlags(0);
  cache->mirror_symmetry_pass = ePaintSymmetryFlags(0);
  vpaint_do_paint(C, sd, vp, vpd, ob, me, brush, initial_symm, 'X', 0, 0);
  vpaint_do_radial_symmetry(C, sd, vp, vpd, ob, me, brush, initial_symm, 'X');
  vpaint_do_radial_symmetry(C, sd, vp, vpd, ob, me, brush, initial_symm, 'Y');
  vpaint_do_radial_symmetry(C, sd, vp, vpd, ob, me, brush, initial_symm, 'Z');

  cache->symmetry = symm;

  /* symm is a bit combination of XYZ - 1 is mirror
   * X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  for (i = 1; i <= symm; i++) {
    if (symm & i && (symm != 5 || i != 3) && (symm != 6 || !ELEM(i, 3, 5))) {
      const ePaintSymmetryFlags symm_pass = ePaintSymmetryFlags(i);
      cache->mirror_symmetry_pass = symm_pass;
      cache->radial_symmetry_pass = 0;
      SCULPT_cache_calc_brushdata_symm(cache, symm_pass, 0, 0);

      if (i & (1 << 0)) {
        vpaint_do_paint(C, sd, vp, vpd, ob, me, brush, symm_pass, 'X', 0, 0);
        vpaint_do_radial_symmetry(C, sd, vp, vpd, ob, me, brush, symm_pass, 'X');
      }
      if (i & (1 << 1)) {
        vpaint_do_paint(C, sd, vp, vpd, ob, me, brush, symm_pass, 'Y', 0, 0);
        vpaint_do_radial_symmetry(C, sd, vp, vpd, ob, me, brush, symm_pass, 'Y');
      }
      if (i & (1 << 2)) {
        vpaint_do_paint(C, sd, vp, vpd, ob, me, brush, symm_pass, 'Z', 0, 0);
        vpaint_do_radial_symmetry(C, sd, vp, vpd, ob, me, brush, symm_pass, 'Z');
      }
    }
  }

  copy_v3_v3(cache->true_last_location, cache->true_location);
  cache->is_last_valid = true;
}

static void vpaint_stroke_update_step(bContext *C,
                                      wmOperator * /*op*/,
                                      PaintStroke *stroke,
                                      PointerRNA *itemptr)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  VPaintData *vpd = static_cast<VPaintData *>(paint_stroke_mode_data(stroke));
  VPaint *vp = ts->vpaint;
  ViewContext *vc = &vpd->vc;
  Object *ob = vc->obact;
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  vwpaint::update_cache_variants(C, vp, ob, itemptr);

  float mat[4][4];

  ED_view3d_init_mats_rv3d(ob, vc->rv3d);

  /* load projection matrix */
  mul_m4_m4m4(mat, vc->rv3d->persmat, ob->object_to_world);

  swap_m4m4(vc->rv3d->persmat, mat);

  vpaint_do_symmetrical_brush_actions(C, sd, vp, vpd, ob);

  swap_m4m4(vc->rv3d->persmat, mat);

  BKE_mesh_batch_cache_dirty_tag((Mesh *)ob->data, BKE_MESH_BATCH_DIRTY_ALL);

  if (vp->paint.brush->vertexpaint_tool == VPAINT_TOOL_SMEAR) {
    vpd->smear.color_prev = vpd->smear.color_curr;
  }

  /* Calculate pivot for rotation around selection if needed.
   * also needed for "Frame Selected" on last stroke. */
  float loc_world[3];
  mul_v3_m4v3(loc_world, ob->object_to_world, ss->cache->true_location);
  vwpaint::last_stroke_update(scene, loc_world);

  ED_region_tag_redraw(vc->region);

  DEG_id_tag_update((ID *)ob->data, ID_RECALC_GEOMETRY);
}

static void vpaint_stroke_done(const bContext *C, PaintStroke *stroke)
{
  VPaintData *vpd = static_cast<VPaintData *>(paint_stroke_mode_data(stroke));
  Object *ob = vpd->vc.obact;

  if (vpd->is_texbrush) {
    ED_vpaint_proj_handle_free(vpd->vp_handle);
  }

  MEM_delete(vpd);

  SculptSession *ss = ob->sculpt;

  if (ss->cache && ss->cache->alt_smooth) {
    ToolSettings *ts = CTX_data_tool_settings(C);
    VPaint *vp = ts->vpaint;
    vwpaint::smooth_brush_toggle_off(C, &vp->paint, ss->cache);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  SCULPT_undo_push_end(ob);

  SCULPT_cache_free(ob->sculpt->cache);
  ob->sculpt->cache = nullptr;
}

static int vpaint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval;

  op->customdata = paint_stroke_new(C,
                                    op,
                                    SCULPT_stroke_get_location,
                                    vpaint_stroke_test_start,
                                    vpaint_stroke_update_step,
                                    nullptr,
                                    vpaint_stroke_done,
                                    event->type);

  Object *ob = CTX_data_active_object(C);

  if (SCULPT_has_loop_colors(ob) && ob->sculpt->pbvh) {
    BKE_pbvh_ensure_node_loops(ob->sculpt->pbvh);
  }

  SCULPT_undo_push_begin_ex(ob, "Vertex Paint");

  if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
    paint_stroke_free(C, op, (PaintStroke *)op->customdata);
    return OPERATOR_FINISHED;
  }

  /* add modal handler */
  WM_event_add_modal_handler(C, op);

  OPERATOR_RETVAL_CHECK(retval);
  BLI_assert(retval == OPERATOR_RUNNING_MODAL);

  return OPERATOR_RUNNING_MODAL;
}

static int vpaint_exec(bContext *C, wmOperator *op)
{
  op->customdata = paint_stroke_new(C,
                                    op,
                                    SCULPT_stroke_get_location,
                                    vpaint_stroke_test_start,
                                    vpaint_stroke_update_step,
                                    nullptr,
                                    vpaint_stroke_done,
                                    0);

  /* frees op->customdata */
  paint_stroke_exec(C, op, (PaintStroke *)op->customdata);

  return OPERATOR_FINISHED;
}

static void vpaint_cancel(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if (ob->sculpt->cache) {
    SCULPT_cache_free(ob->sculpt->cache);
    ob->sculpt->cache = nullptr;
  }

  paint_stroke_cancel(C, op, (PaintStroke *)op->customdata);
}

static int vpaint_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, (PaintStroke **)&op->customdata);
}

void PAINT_OT_vertex_paint(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint";
  ot->idname = "PAINT_OT_vertex_paint";
  ot->description = "Paint a stroke in the active color attribute layer";

  /* api callbacks */
  ot->invoke = vpaint_invoke;
  ot->modal = vpaint_modal;
  ot->exec = vpaint_exec;
  ot->poll = vertex_paint_poll;
  ot->cancel = vpaint_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Vertex Colors Operator
 * \{ */

template<typename T>
static void fill_bm_face_or_corner_attribute(BMesh &bm,
                                             const T &value,
                                             const eAttrDomain domain,
                                             const int cd_offset,
                                             const bool use_vert_sel)
{
  BMFace *f;
  BMIter iter;
  BM_ITER_MESH (f, &iter, &bm, BM_FACES_OF_MESH) {
    BMLoop *l = f->l_first;
    do {
      if (!(use_vert_sel && !BM_elem_flag_test(l->v, BM_ELEM_SELECT))) {
        if (domain == ATTR_DOMAIN_CORNER) {
          *static_cast<T *>(BM_ELEM_CD_GET_VOID_P(l, cd_offset)) = value;
        }
        else if (domain == ATTR_DOMAIN_POINT) {
          *static_cast<T *>(BM_ELEM_CD_GET_VOID_P(l->v, cd_offset)) = value;
        }
      }
    } while ((l = l->next) != f->l_first);
  }
}

template<typename T>
static void fill_mesh_face_or_corner_attribute(Mesh &mesh,
                                               const T &value,
                                               const eAttrDomain domain,
                                               const MutableSpan<T> data,
                                               const bool use_vert_sel,
                                               const bool use_face_sel)
{
  const VArray<bool> select_vert = *mesh.attributes().lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  const VArray<bool> select_poly = *mesh.attributes().lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  const OffsetIndices polys = mesh.polys();
  const Span<int> corner_verts = mesh.corner_verts();

  for (const int i : polys.index_range()) {
    if (use_face_sel && !select_poly[i]) {
      continue;
    }
    for (const int corner : polys[i]) {
      const int vert = corner_verts[corner];
      if (use_vert_sel && !select_vert[vert]) {
        continue;
      }
      if (domain == ATTR_DOMAIN_CORNER) {
        data[corner] = value;
      }
      else {
        data[vert] = value;
      }
    }
  }

  BKE_mesh_tessface_clear(&mesh);
}

static void fill_mesh_color(Mesh &mesh,
                            const ColorPaint4f &color,
                            const StringRef attribute_name,
                            const bool use_vert_sel,
                            const bool use_face_sel)
{
  if (mesh.edit_mesh) {
    BMesh *bm = mesh.edit_mesh->bm;
    const std::string name = attribute_name;
    const CustomDataLayer *layer = BKE_id_attributes_color_find(&mesh.id, name.c_str());
    const eAttrDomain domain = BKE_id_attribute_domain(&mesh.id, layer);
    if (layer->type == CD_PROP_COLOR) {
      fill_bm_face_or_corner_attribute<ColorPaint4f>(
          *bm, color, domain, layer->offset, use_vert_sel);
    }
    else if (layer->type == CD_PROP_BYTE_COLOR) {
      fill_bm_face_or_corner_attribute<ColorPaint4b>(
          *bm, color.encode(), domain, layer->offset, use_vert_sel);
    }
  }
  else {
    bke::GSpanAttributeWriter attribute = mesh.attributes_for_write().lookup_for_write_span(
        attribute_name);
    if (attribute.span.type().is<ColorGeometry4f>()) {
      fill_mesh_face_or_corner_attribute<ColorPaint4f>(
          mesh,
          color,
          attribute.domain,
          attribute.span.typed<ColorGeometry4f>().cast<ColorPaint4f>(),
          use_vert_sel,
          use_face_sel);
    }
    else if (attribute.span.type().is<ColorGeometry4b>()) {
      fill_mesh_face_or_corner_attribute<ColorPaint4b>(
          mesh,
          color.encode(),
          attribute.domain,
          attribute.span.typed<ColorGeometry4b>().cast<ColorPaint4b>(),
          use_vert_sel,
          use_face_sel);
    }
    attribute.finish();
  }
}

/**
 * See doc-string for #BKE_object_attributes_active_color_fill.
 */
static bool paint_object_attributes_active_color_fill_ex(Object *ob,
                                                         ColorPaint4f fill_color,
                                                         bool only_selected = true)
{
  Mesh *me = BKE_mesh_from_object(ob);
  if (!me) {
    return false;
  }

  const bool use_face_sel = only_selected ? (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0 : false;
  const bool use_vert_sel = only_selected ? (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0 : false;
  fill_mesh_color(*me, fill_color, me->active_color_attribute, use_vert_sel, use_face_sel);

  DEG_id_tag_update(&me->id, ID_RECALC_COPY_ON_WRITE);

  /* NOTE: Original mesh is used for display, so tag it directly here. */
  BKE_mesh_batch_cache_dirty_tag(me, BKE_MESH_BATCH_DIRTY_ALL);

  return true;
}

bool BKE_object_attributes_active_color_fill(Object *ob,
                                             const float fill_color[4],
                                             bool only_selected)
{
  return paint_object_attributes_active_color_fill_ex(ob, ColorPaint4f(fill_color), only_selected);
}

static int vertex_color_set_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Object *obact = CTX_data_active_object(C);

  ColorPaint4f paintcol = vpaint_get_current_col(scene, scene->toolsettings->vpaint, false);

  if (paint_object_attributes_active_color_fill_ex(obact, paintcol)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Vertex Colors";
  ot->idname = "PAINT_OT_vertex_color_set";
  ot->description = "Fill the active vertex color layer with the current paint color";

  /* api callbacks */
  ot->exec = vertex_color_set_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
