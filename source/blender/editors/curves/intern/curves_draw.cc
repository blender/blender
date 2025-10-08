/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_curve_types.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_mempool.h"

#include "BLT_translation.hh"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_object_types.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"

#include "ED_curves.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

extern "C" {
#include "curve_fit_nd.h"
}

namespace blender::ed::curves {

/* Distance between input samples */
#define STROKE_SAMPLE_DIST_MIN_PX 1
#define STROKE_SAMPLE_DIST_MAX_PX 3

/* Distance between start/end points to consider cyclic */
#define STROKE_CYCLIC_DIST_PX 8

/* -------------------------------------------------------------------- */
/** \name StrokeElem / #RNA_OperatorStrokeElement Conversion Functions
 * \{ */

struct StrokeElem {
  float mval[2];
  float location_world[3];
  float location_local[3];

  /* Surface normal, may be zeroed. */
  float normal_world[3];
  float normal_local[3];

  float pressure;
};

enum CurveDrawState {
  CURVE_DRAW_IDLE = 0,
  CURVE_DRAW_PAINTING = 1,
};

struct CurveDrawData {
  short init_event_type;
  short curve_type;
  float bevel_radius;
  bool is_curve_2d;

  /* projecting 2D into 3D space */
  struct {
    /* use a plane or project to the surface */
    bool use_plane;
    float plane[4];

    /* use 'rv3d->depths', note that this will become 'damaged' while drawing, but that's OK. */
    bool use_depth;

    /* offset projection by this value */
    bool use_offset;
    float offset[3]; /* world-space */
    float surface_offset;
    bool use_surface_offset_absolute;
  } project;

  /* cursor sampling */
  struct {
    /* use substeps, needed for nicely interpolating depth */
    bool use_substeps;
  } sample;

  struct {
    float min, max, range;
  } radius;

  struct {
    float mval[2];
    /* Used in case we can't calculate the depth. */
    float location_world[3];

    float location_world_valid[3];

    const StrokeElem *selem;
  } prev;

  ViewContext vc;
  ViewDepths *depths;
  CurveDrawState state;

  /* StrokeElem */
  BLI_mempool *stroke_elem_pool;

  void *draw_handle_view;
};

static float stroke_elem_radius_from_pressure(const CurveDrawData *cdd, const float pressure)
{
  return ((pressure * cdd->radius.range) + cdd->radius.min) * cdd->bevel_radius;
}

static float stroke_elem_radius(const CurveDrawData *cdd, const StrokeElem *selem)
{
  return stroke_elem_radius_from_pressure(cdd, selem->pressure);
}

static void stroke_elem_pressure_set(const CurveDrawData *cdd, StrokeElem *selem, float pressure)
{
  if ((cdd->project.surface_offset != 0.0f) && !cdd->project.use_surface_offset_absolute &&
      !is_zero_v3(selem->normal_local))
  {
    const float adjust = stroke_elem_radius_from_pressure(cdd, pressure) -
                         stroke_elem_radius_from_pressure(cdd, selem->pressure);
    madd_v3_v3fl(selem->location_local, selem->normal_local, adjust);
    mul_v3_m4v3(
        selem->location_world, cdd->vc.obedit->object_to_world().ptr(), selem->location_local);
  }
  selem->pressure = pressure;
}

static void stroke_elem_interp(StrokeElem *selem_out,
                               const StrokeElem *selem_a,
                               const StrokeElem *selem_b,
                               float t)
{
  interp_v2_v2v2(selem_out->mval, selem_a->mval, selem_b->mval, t);
  interp_v3_v3v3(selem_out->location_world, selem_a->location_world, selem_b->location_world, t);
  interp_v3_v3v3(selem_out->location_local, selem_a->location_local, selem_b->location_local, t);
  selem_out->pressure = interpf(selem_a->pressure, selem_b->pressure, t);
}

/**
 * Sets the depth from #StrokeElem.mval
 */
static bool stroke_elem_project(const CurveDrawData *cdd,
                                const int mval_i[2],
                                const float mval_fl[2],
                                float surface_offset,
                                const float radius,
                                float r_location_world[3],
                                float r_normal_world[3])
{
  ARegion *region = cdd->vc.region;

  bool is_location_world_set = false;

  /* project to 'location_world' */
  if (cdd->project.use_plane) {
    /* get the view vector to 'location' */
    if (ED_view3d_win_to_3d_on_plane(region, cdd->project.plane, mval_fl, true, r_location_world))
    {
      if (r_normal_world) {
        zero_v3(r_normal_world);
      }
      is_location_world_set = true;
    }
  }
  else {
    const ViewDepths *depths = cdd->depths;
    if (depths && (uint(mval_i[0]) < depths->w) && (uint(mval_i[1]) < depths->h)) {
      float depth_fl = 1.0f;
      ED_view3d_depth_read_cached(depths, mval_i, 0, &depth_fl);
      const double depth = double(depth_fl);
      if ((depth > depths->depth_range[0]) && (depth < depths->depth_range[1])) {
        if (ED_view3d_depth_unproject_v3(region, mval_i, depth, r_location_world)) {
          is_location_world_set = true;
          if (r_normal_world) {
            zero_v3(r_normal_world);
          }

          if (surface_offset != 0.0f) {
            const float offset = cdd->project.use_surface_offset_absolute ? 1.0f : radius;
            float normal[3];
            if (ED_view3d_depth_read_cached_normal(region, depths, mval_i, normal)) {
              madd_v3_v3fl(r_location_world, normal, offset * surface_offset);
              if (r_normal_world) {
                copy_v3_v3(r_normal_world, normal);
              }
            }
          }
        }
      }
    }
  }

  if (is_location_world_set) {
    if (cdd->project.use_offset) {
      add_v3_v3(r_location_world, cdd->project.offset);
    }
  }

  return is_location_world_set;
}

static bool stroke_elem_project_fallback(const CurveDrawData *cdd,
                                         const int mval_i[2],
                                         const float mval_fl[2],
                                         const float surface_offset,
                                         const float radius,
                                         const float location_fallback_depth[3],
                                         float r_location_world[3],
                                         float r_location_local[3],
                                         float r_normal_world[3],
                                         float r_normal_local[3])
{
  bool is_depth_found = stroke_elem_project(
      cdd, mval_i, mval_fl, surface_offset, radius, r_location_world, r_normal_world);
  if (is_depth_found == false) {
    ED_view3d_win_to_3d(
        cdd->vc.v3d, cdd->vc.region, location_fallback_depth, mval_fl, r_location_world);
    zero_v3(r_normal_local);
  }
  mul_v3_m4v3(r_location_local, cdd->vc.obedit->world_to_object().ptr(), r_location_world);

  if (!is_zero_v3(r_normal_world)) {
    copy_v3_v3(r_normal_local, r_normal_world);
    mul_transposed_mat3_m4_v3(cdd->vc.obedit->object_to_world().ptr(), r_normal_local);
    normalize_v3(r_normal_local);
  }
  else {
    zero_v3(r_normal_local);
  }

  return is_depth_found;
}

/**
 * \note #StrokeElem.mval & #StrokeElem.pressure must be set first.
 */
static bool stroke_elem_project_fallback_elem(const CurveDrawData *cdd,
                                              const float location_fallback_depth[3],
                                              StrokeElem *selem)
{
  const int mval_i[2] = {int(selem->mval[0]), int(selem->mval[1])};
  const float radius = stroke_elem_radius(cdd, selem);
  return stroke_elem_project_fallback(cdd,
                                      mval_i,
                                      selem->mval,
                                      cdd->project.surface_offset,
                                      radius,
                                      location_fallback_depth,
                                      selem->location_world,
                                      selem->location_local,
                                      selem->normal_world,
                                      selem->normal_local);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator/Stroke Conversion
 * \{ */

static void curve_draw_stroke_to_operator_elem(wmOperator *op, const StrokeElem *selem)
{
  PointerRNA itemptr;
  RNA_collection_add(op->ptr, "stroke", &itemptr);

  RNA_float_set_array(&itemptr, "mouse", selem->mval);
  RNA_float_set_array(&itemptr, "location", selem->location_world);
  RNA_float_set(&itemptr, "pressure", selem->pressure);
}

static void curve_draw_stroke_from_operator_elem(wmOperator *op, PointerRNA *itemptr)
{
  CurveDrawData *cdd = static_cast<CurveDrawData *>(op->customdata);

  StrokeElem *selem = static_cast<StrokeElem *>(BLI_mempool_calloc(cdd->stroke_elem_pool));

  RNA_float_get_array(itemptr, "mouse", selem->mval);
  RNA_float_get_array(itemptr, "location", selem->location_world);
  mul_v3_m4v3(
      selem->location_local, cdd->vc.obedit->world_to_object().ptr(), selem->location_world);
  selem->pressure = RNA_float_get(itemptr, "pressure");
}

static void curve_draw_stroke_to_operator(wmOperator *op)
{
  CurveDrawData *cdd = static_cast<CurveDrawData *>(op->customdata);

  BLI_mempool_iter iter;
  const StrokeElem *selem;

  BLI_mempool_iternew(cdd->stroke_elem_pool, &iter);
  for (selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)); selem;
       selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)))
  {
    curve_draw_stroke_to_operator_elem(op, selem);
  }
}

static void curve_draw_stroke_from_operator(wmOperator *op)
{
  RNA_BEGIN (op->ptr, itemptr, "stroke") {
    curve_draw_stroke_from_operator_elem(op, &itemptr);
  }
  RNA_END;
}

/** \} */

static void curve_draw_stroke_3d(const bContext * /*C*/, ARegion * /*region*/, void *arg)
{
  wmOperator *op = static_cast<wmOperator *>(arg);
  CurveDrawData *cdd = static_cast<CurveDrawData *>(op->customdata);

  const int stroke_len = BLI_mempool_len(cdd->stroke_elem_pool);

  if (stroke_len == 0) {
    return;
  }

  Object *obedit = cdd->vc.obedit;

  /* Disabled: not representative in enough cases, and curves draw shape is not per object yet.
   * In the future this could be enabled when the object's draw shape is "strand" or "3D". */
  if (false && cdd->bevel_radius > 0.0f) {
    BLI_mempool_iter iter;
    const StrokeElem *selem;

    const float location_zero[3] = {0};
    const float *location_prev = location_zero;

    float color[3];
    UI_GetThemeColor3fv(TH_WIRE, color);

    gpu::Batch *sphere = GPU_batch_preset_sphere(0);
    GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
    GPU_batch_uniform_3fv(sphere, "color", color);

    /* scale to edit-mode space */
    GPU_matrix_push();
    GPU_matrix_mul(obedit->object_to_world().ptr());

    BLI_mempool_iternew(cdd->stroke_elem_pool, &iter);
    for (selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)); selem;
         selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)))
    {
      GPU_matrix_translate_3f(selem->location_local[0] - location_prev[0],
                              selem->location_local[1] - location_prev[1],
                              selem->location_local[2] - location_prev[2]);

      const float radius = stroke_elem_radius(cdd, selem);

      GPU_matrix_push();
      GPU_matrix_scale_1f(radius);
      GPU_batch_draw(sphere);
      GPU_matrix_pop();

      location_prev = selem->location_local;
    }

    GPU_matrix_pop();
  }

  if (stroke_len > 1) {
    float (*coord_array)[3] = static_cast<float (*)[3]>(
        MEM_mallocN(sizeof(*coord_array) * stroke_len, __func__));

    {
      BLI_mempool_iter iter;
      const StrokeElem *selem;
      int i;
      BLI_mempool_iternew(cdd->stroke_elem_pool, &iter);
      for (selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)), i = 0; selem;
           selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)), i++)
      {
        copy_v3_v3(coord_array[i], selem->location_world);
      }
    }

    {
      GPUVertFormat *format = immVertexFormat();
      uint pos = GPU_vertformat_attr_add(
          format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

      GPU_depth_test(GPU_DEPTH_NONE);
      GPU_blend(GPU_BLEND_ALPHA);
      GPU_line_smooth(true);
      GPU_line_width(3.0f);

      imm_cpack(0x0);
      immBegin(GPU_PRIM_LINE_STRIP, stroke_len);
      for (int i = 0; i < stroke_len; i++) {
        immVertex3fv(pos, coord_array[i]);
      }
      immEnd();

      GPU_line_width(1.0f);

      imm_cpack(0xffffffff);
      immBegin(GPU_PRIM_LINE_STRIP, stroke_len);
      for (int i = 0; i < stroke_len; i++) {
        immVertex3fv(pos, coord_array[i]);
      }
      immEnd();

      /* Reset defaults */
      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
      GPU_blend(GPU_BLEND_NONE);
      GPU_line_smooth(false);

      immUnbindProgram();
    }

    MEM_freeN(coord_array);
  }
}

static void curve_draw_event_add(wmOperator *op, const wmEvent *event)
{
  CurveDrawData *cdd = static_cast<CurveDrawData *>(op->customdata);
  Object *obedit = cdd->vc.obedit;

  invert_m4_m4(obedit->runtime->world_to_object.ptr(), obedit->object_to_world().ptr());

  StrokeElem *selem = static_cast<StrokeElem *>(BLI_mempool_calloc(cdd->stroke_elem_pool));

  ARRAY_SET_ITEMS(selem->mval, event->mval[0], event->mval[1]);

  /* handle pressure sensitivity (which is supplied by tablets or otherwise 1.0) */
  selem->pressure = event->tablet.pressure;

  bool is_depth_found = stroke_elem_project_fallback_elem(
      cdd, cdd->prev.location_world_valid, selem);

  if (is_depth_found) {
    /* use the depth if a fallback wasn't used */
    copy_v3_v3(cdd->prev.location_world_valid, selem->location_world);
  }
  copy_v3_v3(cdd->prev.location_world, selem->location_world);

  float len_sq = len_squared_v2v2(cdd->prev.mval, selem->mval);
  copy_v2_v2(cdd->prev.mval, selem->mval);

  if (cdd->sample.use_substeps && cdd->prev.selem) {
    const StrokeElem selem_target = *selem;
    StrokeElem *selem_new_last = selem;
    if (len_sq >= square_f(STROKE_SAMPLE_DIST_MAX_PX)) {
      int n = int(ceil(sqrt(double(len_sq)))) / STROKE_SAMPLE_DIST_MAX_PX;

      for (int i = 1; i < n; i++) {
        StrokeElem *selem_new = selem_new_last;
        stroke_elem_interp(selem_new, cdd->prev.selem, &selem_target, float(i) / n);

        const bool is_depth_found_substep = stroke_elem_project_fallback_elem(
            cdd, cdd->prev.location_world_valid, selem_new);
        if (is_depth_found == false) {
          if (is_depth_found_substep) {
            copy_v3_v3(cdd->prev.location_world_valid, selem_new->location_world);
          }
        }

        selem_new_last = static_cast<StrokeElem *>(BLI_mempool_calloc(cdd->stroke_elem_pool));
      }
    }
    selem = selem_new_last;
    *selem_new_last = selem_target;
  }

  cdd->prev.selem = selem;

  ED_region_tag_redraw(cdd->vc.region);
}

static void curve_draw_event_add_first(wmOperator *op, const wmEvent *event)
{
  CurveDrawData *cdd = static_cast<CurveDrawData *>(op->customdata);
  const CurvePaintSettings *cps = &cdd->vc.scene->toolsettings->curve_paint_settings;

  /* add first point */
  curve_draw_event_add(op, event);

  if ((cps->depth_mode == CURVE_PAINT_PROJECT_SURFACE) && cdd->project.use_depth &&
      (cps->flag & CURVE_PAINT_FLAG_DEPTH_STROKE_ENDPOINTS))
  {
    RegionView3D *rv3d = cdd->vc.rv3d;

    cdd->project.use_depth = false;
    cdd->project.use_plane = true;

    float normal[3] = {0.0f};
    if (ELEM(cps->surface_plane,
             CURVE_PAINT_SURFACE_PLANE_NORMAL_VIEW,
             CURVE_PAINT_SURFACE_PLANE_NORMAL_SURFACE))
    {
      if (ED_view3d_depth_read_cached_normal(cdd->vc.region, cdd->depths, event->mval, normal)) {
        if (cps->surface_plane == CURVE_PAINT_SURFACE_PLANE_NORMAL_VIEW) {
          float cross_a[3], cross_b[3];
          cross_v3_v3v3(cross_a, rv3d->viewinv[2], normal);
          cross_v3_v3v3(cross_b, normal, cross_a);
          copy_v3_v3(normal, cross_b);
        }
      }
    }

    /* CURVE_PAINT_SURFACE_PLANE_VIEW or fallback */
    if (is_zero_v3(normal)) {
      copy_v3_v3(normal, rv3d->viewinv[2]);
    }

    normalize_v3_v3(cdd->project.plane, normal);
    cdd->project.plane[3] = -dot_v3v3(cdd->project.plane, cdd->prev.location_world_valid);

    /* Special case for when we only have offset applied on the first-hit,
     * the remaining stroke must be offset too. */
    if (cdd->project.surface_offset != 0.0f) {
      const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};

      float location_no_offset[3];

      if (stroke_elem_project(cdd, event->mval, mval_fl, 0.0f, 0.0f, location_no_offset, nullptr))
      {
        sub_v3_v3v3(cdd->project.offset, cdd->prev.location_world_valid, location_no_offset);
        if (!is_zero_v3(cdd->project.offset)) {
          cdd->project.use_offset = true;
        }
      }
    }
    /* end special case */
  }

  cdd->init_event_type = event->type;
  cdd->state = CURVE_DRAW_PAINTING;
}

static void curve_draw_exit(wmOperator *op)
{
  CurveDrawData *cdd = static_cast<CurveDrawData *>(op->customdata);
  if (cdd) {
    if (cdd->draw_handle_view) {
      ED_region_draw_cb_exit(cdd->vc.region->runtime->type, cdd->draw_handle_view);
      WM_cursor_modal_restore(cdd->vc.win);
    }

    if (cdd->stroke_elem_pool) {
      BLI_mempool_destroy(cdd->stroke_elem_pool);
    }

    if (cdd->depths) {
      ED_view3d_depths_free(cdd->depths);
    }
    MEM_freeN(cdd);
    op->customdata = nullptr;
  }
}

static bool curve_draw_init(bContext *C, wmOperator *op, bool is_invoke)
{
  BLI_assert(op->customdata == nullptr);

  CurveDrawData *cdd = MEM_callocN<CurveDrawData>(__func__);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  if (is_invoke) {
    cdd->vc = ED_view3d_viewcontext_init(C, depsgraph);
    if (ELEM(nullptr, cdd->vc.region, cdd->vc.rv3d, cdd->vc.v3d, cdd->vc.win, cdd->vc.scene)) {
      MEM_freeN(cdd);
      BKE_report(op->reports, RPT_ERROR, "Unable to access 3D viewport");
      return false;
    }
  }
  else {
    cdd->vc.bmain = CTX_data_main(C);
    cdd->vc.depsgraph = depsgraph;
    cdd->vc.scene = CTX_data_scene(C);
    cdd->vc.view_layer = CTX_data_view_layer(C);
    cdd->vc.obedit = CTX_data_edit_object(C);

    /* Using an empty stroke complicates logic later,
     * it's simplest to disallow early on (see: #94085). */
    if (RNA_collection_is_empty(op->ptr, "stroke")) {
      MEM_freeN(cdd);
      BKE_report(op->reports, RPT_ERROR, "The \"stroke\" cannot be empty");
      return false;
    }
  }

  op->customdata = cdd;
  cdd->bevel_radius = 1.0f;
  cdd->is_curve_2d = RNA_boolean_get(op->ptr, "is_curve_2d");

  const CurvePaintSettings *cps = &cdd->vc.scene->toolsettings->curve_paint_settings;

  cdd->curve_type = cps->curve_type;

  cdd->radius.min = cps->radius_min;
  cdd->radius.max = cps->radius_max;
  cdd->radius.range = cps->radius_max - cps->radius_min;
  cdd->project.surface_offset = cps->surface_offset;
  cdd->project.use_surface_offset_absolute = (cps->flag &
                                              CURVE_PAINT_FLAG_DEPTH_STROKE_OFFSET_ABS) != 0;

  cdd->stroke_elem_pool = BLI_mempool_create(sizeof(StrokeElem), 0, 512, BLI_MEMPOOL_ALLOW_ITER);

  return true;
}

static void create_Bezier(bke::CurvesGeometry &curves,
                          bke::MutableAttributeAccessor &attributes,
                          const CurveDrawData *cdd,
                          const int curve_index,
                          const bool is_cyclic,
                          const uint cubic_spline_len,
                          const int dims,
                          const int radius_index,
                          const float radius_max,
                          const float *cubic_spline,
                          const uint *corners_index,
                          const uint corners_index_len)
{
  curves.resize(curves.points_num() + cubic_spline_len, curve_index + 1);

  MutableSpan<float3> positions = curves.positions_for_write();
  MutableSpan<float3> handle_positions_l = curves.handle_positions_left_for_write();
  MutableSpan<float3> handle_positions_r = curves.handle_positions_right_for_write();
  MutableSpan<int8_t> handle_types_l = curves.handle_types_left_for_write();
  MutableSpan<int8_t> handle_types_r = curves.handle_types_right_for_write();

  const IndexRange new_points = curves.points_by_curve()[curve_index];

  bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_only_span<float>(
      "radius", bke::AttrDomain::Point);

  const float *co = cubic_spline;

  for (const int64_t i : new_points) {
    const float *handle_l = co + (dims * 0);
    const float *pt = co + (dims * 1);
    const float *handle_r = co + (dims * 2);

    copy_v3_v3(handle_positions_l[i], handle_l);
    copy_v3_v3(positions[i], pt);
    copy_v3_v3(handle_positions_r[i], handle_r);

    const float radius = (radius_index != -1) ?
                             (pt[radius_index] * cdd->radius.range) + cdd->radius.min :
                             radius_max;
    radii.span[i] = radius;

    handle_types_l[i] = BEZIER_HANDLE_ALIGN;
    handle_types_r[i] = BEZIER_HANDLE_ALIGN;
    co += (dims * 3);
  }

  if (corners_index) {
    /* ignore the first and last */
    uint i_start = 0, i_end = corners_index_len;

    if ((corners_index_len >= 2) && !is_cyclic) {
      i_start += 1;
      i_end -= 1;
    }

    for (const auto i : IndexRange(i_start, i_end - i_start)) {
      const int64_t corner_i = new_points[corners_index[i]];
      handle_types_l[corner_i] = BEZIER_HANDLE_FREE;
      handle_types_r[corner_i] = BEZIER_HANDLE_FREE;
    }
  }

  radii.finish();
}

static void create_NURBS(bke::CurvesGeometry &curves,
                         bke::MutableAttributeAccessor &attributes,
                         const CurveDrawData *cdd,
                         const int curve_index,
                         const bool is_cyclic,
                         const uint cubic_spline_len,
                         const int dims,
                         const int radius_index,
                         const float radius_max,
                         const float *cubic_spline)
{
  const int point_num = (cubic_spline_len - 2) * 3 + 4 + (is_cyclic ? 2 : 0);
  curves.resize(curves.points_num() + point_num, curve_index + 1);

  MutableSpan<float3> positions = curves.positions_for_write();
  MutableSpan<float> weights = curves.nurbs_weights_for_write();

  const IndexRange new_points = curves.points_by_curve()[curve_index];

  bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_only_span<float>(
      "radius", bke::AttrDomain::Point);
  /* If cyclic shows to first left handle else first control point. */
  const float *pt = cubic_spline + (is_cyclic ? 0 : dims);

  for (const int64_t i : new_points) {
    const float radius = (radius_index != -1) ?
                             (pt[radius_index] * cdd->radius.range) + cdd->radius.min :
                             radius_max;
    copy_v3_v3(positions[i], pt);
    weights[i] = 1.0f;
    radii.span[i] = radius;

    pt += dims;
  }

  radii.finish();
}

static wmOperatorStatus curves_draw_exec(bContext *C, wmOperator *op)
{
  if (op->customdata == nullptr) {
    if (!curve_draw_init(C, op, false)) {
      return OPERATOR_CANCELLED;
    }
  }

  CurveDrawData *cdd = static_cast<CurveDrawData *>(op->customdata);

  const CurvePaintSettings *cps = &cdd->vc.scene->toolsettings->curve_paint_settings;
  Object *obedit = cdd->vc.obedit;

  int stroke_len = BLI_mempool_len(cdd->stroke_elem_pool);

  invert_m4_m4(obedit->runtime->world_to_object.ptr(), obedit->object_to_world().ptr());

  if (BLI_mempool_len(cdd->stroke_elem_pool) == 0) {
    curve_draw_stroke_from_operator(op);
    stroke_len = BLI_mempool_len(cdd->stroke_elem_pool);
  }

  /* error in object local space */
  const int fit_method = RNA_enum_get(op->ptr, "fit_method");
  const float error_threshold = RNA_float_get(op->ptr, "error_threshold");
  const float corner_angle = RNA_float_get(op->ptr, "corner_angle");
  const bool use_cyclic = RNA_boolean_get(op->ptr, "use_cyclic");
  const bool bezier_as_nurbs = RNA_boolean_get(op->ptr, "bezier_as_nurbs");
  bool is_cyclic = (stroke_len > 2) && use_cyclic;

  const float radius_min = cps->radius_min;
  const float radius_max = cps->radius_max;
  const float radius_range = cps->radius_max - cps->radius_min;

  Curves *curves_id = static_cast<Curves *>(obedit->data);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  const int curve_index = curves.curves_num();

  const bool use_pressure_radius = (cps->flag & CURVE_PAINT_FLAG_PRESSURE_RADIUS) ||
                                   ((cps->radius_taper_start != 0.0f) ||
                                    (cps->radius_taper_end != 0.0f));

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  Span<StringRef> selection_attribute_names = get_curves_selection_attribute_names(curves);
  remove_selection_attributes(attributes, selection_attribute_names);

  if (cdd->curve_type == CU_BEZIER) {
    /* Allow to interpolate multiple channels */
    int dims = 3;
    const int radius_index = use_pressure_radius ? dims++ : -1;

    float *coords = MEM_malloc_arrayN<float>(stroke_len * dims, __func__);

    float *cubic_spline = nullptr;
    uint cubic_spline_len = 0;

    {
      BLI_mempool_iter iter;
      const StrokeElem *selem;
      float *co = coords;

      BLI_mempool_iternew(cdd->stroke_elem_pool, &iter);
      for (selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)); selem;
           selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)), co += dims)
      {
        copy_v3_v3(co, selem->location_local);
        if (radius_index != -1) {
          co[radius_index] = selem->pressure;
        }

        /* remove doubles */
        if ((co != coords) && UNLIKELY(memcmp(co, co - dims, sizeof(float) * dims) == 0)) {
          co -= dims;
          stroke_len--;
        }
      }
    }

    uint *corners = nullptr;
    uint corners_len = 0;

    if ((fit_method == CURVE_PAINT_FIT_METHOD_SPLIT) && (corner_angle < float(M_PI))) {
      /* this could be configurable... */
      const float corner_radius_min = error_threshold / 8;
      const float corner_radius_max = error_threshold * 2;
      const uint samples_max = 16;

      curve_fit_corners_detect_fl(coords,
                                  stroke_len,
                                  dims,
                                  corner_radius_min,
                                  corner_radius_max,
                                  samples_max,
                                  corner_angle,
                                  &corners,
                                  &corners_len);
    }

    uint *corners_index = nullptr;
    uint corners_index_len = 0;
    uint calc_flag = CURVE_FIT_CALC_HIGH_QUALIY;

    if ((stroke_len > 2) && use_cyclic) {
      calc_flag |= CURVE_FIT_CALC_CYCLIC;
    }
    else {
      /* Might need this update if stroke_len <= 2 after removing doubles. */
      is_cyclic = false;
    }

    int result;
    if (fit_method == CURVE_PAINT_FIT_METHOD_REFIT) {
      result = curve_fit_cubic_to_points_refit_fl(coords,
                                                  stroke_len,
                                                  dims,
                                                  error_threshold,
                                                  calc_flag,
                                                  nullptr,
                                                  0,
                                                  corner_angle,
                                                  &cubic_spline,
                                                  &cubic_spline_len,
                                                  nullptr,
                                                  &corners_index,
                                                  &corners_index_len);
    }
    else {
      result = curve_fit_cubic_to_points_fl(coords,
                                            stroke_len,
                                            dims,
                                            error_threshold,
                                            calc_flag,
                                            corners,
                                            corners_len,
                                            &cubic_spline,
                                            &cubic_spline_len,
                                            nullptr,
                                            &corners_index,
                                            &corners_index_len);
    }

    MEM_freeN(coords);
    if (corners) {
      free(corners);
    }

    if (result == 0) {
      int8_t knots_mode;
      int8_t order;
      CurveType curve_type;
      if (bezier_as_nurbs) {
        bool is_cyclic_curve = calc_flag & CURVE_FIT_CALC_CYCLIC;
        create_NURBS(curves,
                     attributes,
                     cdd,
                     curve_index,
                     is_cyclic_curve,
                     cubic_spline_len,
                     dims,
                     radius_index,
                     radius_max,
                     cubic_spline);
        order = 4;
        knots_mode = is_cyclic_curve ? NURBS_KNOT_MODE_BEZIER : NURBS_KNOT_MODE_ENDPOINT_BEZIER;
        curve_type = CURVE_TYPE_NURBS;
      }
      else {
        create_Bezier(curves,
                      attributes,
                      cdd,
                      curve_index,
                      is_cyclic,
                      cubic_spline_len,
                      dims,
                      radius_index,
                      radius_max,
                      cubic_spline,
                      corners_index,
                      corners_index_len);
        order = 0;
        knots_mode = 0;
        curve_type = CURVE_TYPE_BEZIER;
      }
      curves.nurbs_knots_modes_for_write()[curve_index] = knots_mode;
      curves.nurbs_orders_for_write()[curve_index] = order;
      curves.fill_curve_types(IndexRange(curve_index, 1), curve_type);

      /* If Bezier curve is being added, loop through all three names, otherwise through ones in
       * `selection_attribute_names`. */
      for (const StringRef selection_name :
           (bezier_as_nurbs ? selection_attribute_names :
                              get_curves_all_selection_attribute_names()))
      {
        bke::AttributeWriter<bool> selection = attributes.lookup_or_add_for_write<bool>(
            selection_name, bke::AttrDomain::Curve);
        if (selection_name == ".selection" || !bezier_as_nurbs) {
          selection.varray.set(curve_index, true);
        }
        selection.finish();
      }

      if (attributes.contains("resolution")) {
        curves.resolution_for_write()[curve_index] = 12;
      }
      bke::fill_attribute_range_default(
          attributes,
          bke::AttrDomain::Point,
          bke::attribute_filter_from_skip_ref({"position",
                                               "radius",
                                               "handle_left",
                                               "handle_right",
                                               "handle_type_left",
                                               "handle_type_right",
                                               "nurbs_weight",
                                               ".selection",
                                               ".selection_handle_left",
                                               ".selection_handle_right"}),
          curves.points_by_curve()[curve_index]);
      bke::fill_attribute_range_default(
          attributes,
          bke::AttrDomain::Curve,
          bke::attribute_filter_from_skip_ref({"curve_type",
                                               "resolution",
                                               "cyclic",
                                               "nurbs_order",
                                               "knots_mode",
                                               ".selection",
                                               ".selection_handle_left",
                                               ".selection_handle_right"}),
          IndexRange(curve_index, 1));
    }

    if (corners_index) {
      free(corners_index);
    }

    if (cubic_spline) {
      free(cubic_spline);
    }
  }
  else { /* CU_POLY */
    curves.resize(curves.points_num() + stroke_len, curve_index + 1);
    curves.fill_curve_types(IndexRange(curve_index, 1), CURVE_TYPE_POLY);

    MutableSpan<float3> positions = curves.positions_for_write();
    bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_only_span<float>(
        "radius", bke::AttrDomain::Point);

    const IndexRange new_points = curves.points_by_curve()[curve_index];

    IndexRange::Iterator points_iter = new_points.begin();

    BLI_mempool_iter iter;
    BLI_mempool_iternew(cdd->stroke_elem_pool, &iter);
    for (const auto *selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)); selem;
         selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)), points_iter++)
    {
      const int64_t i = *points_iter;
      copy_v3_v3(positions[i], selem->location_local);
      if (cdd->is_curve_2d) {
        positions[i][2] = 0.0f;
      }

      radii.span[i] = use_pressure_radius ? (selem->pressure * radius_range) + radius_min :
                                            cps->radius_max;
    }

    radii.finish();

    bke::AttributeWriter<bool> selection = attributes.lookup_or_add_for_write<bool>(
        ".selection", bke::AttrDomain::Curve);
    selection.varray.set(curve_index, true);
    selection.finish();

    /* Creates ".selection_handle_left" and ".selection_handle_right" attributes, otherwise all
     * existing Bezier handles would be treated as selected. */
    for (const StringRef selection_name : get_curves_bezier_selection_attribute_names(curves)) {
      bke::AttributeWriter<bool> selection = attributes.lookup_or_add_for_write<bool>(
          selection_name, bke::AttrDomain::Curve);
      selection.finish();
    }

    bke::fill_attribute_range_default(
        attributes,
        bke::AttrDomain::Point,
        bke::attribute_filter_from_skip_ref({"position",
                                             "radius",
                                             ".selection",
                                             ".selection_handle_left",
                                             ".selection_handle_right"}),
        new_points);
    bke::fill_attribute_range_default(
        attributes,
        bke::AttrDomain::Curve,
        bke::attribute_filter_from_skip_ref(
            {"curve_type", ".selection", ".selection_handle_left", ".selection_handle_right"}),
        IndexRange(curve_index, 1));
  }

  if (is_cyclic) {
    curves.cyclic_for_write()[curve_index] = true;
  }

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);

  curve_draw_exit(op);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus curves_draw_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (RNA_struct_property_is_set(op->ptr, "stroke")) {
    return curves_draw_exec(C, op);
  }

  if (!curve_draw_init(C, op, true)) {
    return OPERATOR_CANCELLED;
  }

  CurveDrawData *cdd = static_cast<CurveDrawData *>(op->customdata);

  const CurvePaintSettings *cps = &cdd->vc.scene->toolsettings->curve_paint_settings;

  const bool is_modal = RNA_boolean_get(op->ptr, "wait_for_input");

  /* Fallback (in case we can't find the depth on first test). */
  {
    const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
    float center[3];
    negate_v3_v3(center, cdd->vc.rv3d->ofs);
    ED_view3d_win_to_3d(cdd->vc.v3d, cdd->vc.region, center, mval_fl, cdd->prev.location_world);
    copy_v3_v3(cdd->prev.location_world_valid, cdd->prev.location_world);
  }

  cdd->draw_handle_view = ED_region_draw_cb_activate(
      cdd->vc.region->runtime->type, curve_draw_stroke_3d, op, REGION_DRAW_POST_VIEW);
  WM_cursor_modal_set(cdd->vc.win, WM_CURSOR_PAINT_BRUSH);

  {
    View3D *v3d = cdd->vc.v3d;
    RegionView3D *rv3d = cdd->vc.rv3d;
    Object *obedit = cdd->vc.obedit;

    const float *plane_no = nullptr;
    const float *plane_co = nullptr;

    if (cdd->is_curve_2d) {
      /* 2D overrides other options */
      plane_co = obedit->object_to_world().location();
      plane_no = obedit->object_to_world().ptr()[2];
      cdd->project.use_plane = true;
    }
    else {
      if ((cps->depth_mode == CURVE_PAINT_PROJECT_SURFACE) && (v3d->shading.type > OB_WIRE)) {
        /* needed or else the draw matrix can be incorrect */
        view3d_operator_needs_gpu(C);

        eV3DDepthOverrideMode depth_mode = V3D_DEPTH_ALL;
        if (cps->flag & CURVE_PAINT_FLAG_DEPTH_ONLY_SELECTED) {
          depth_mode = V3D_DEPTH_SELECTED_ONLY;
        }

        ED_view3d_depth_override(cdd->vc.depsgraph,
                                 cdd->vc.region,
                                 cdd->vc.v3d,
                                 nullptr,
                                 depth_mode,
                                 false,
                                 &cdd->depths);

        if (cdd->depths != nullptr) {
          cdd->project.use_depth = true;
        }
        else {
          BKE_report(op->reports, RPT_WARNING, "Unable to access depth buffer, using view plane");
          cdd->project.use_depth = false;
        }
      }

      /* use view plane (when set or as a fallback when surface can't be found) */
      if (cdd->project.use_depth == false) {
        plane_co = cdd->vc.scene->cursor.location;
        plane_no = rv3d->viewinv[2];
        cdd->project.use_plane = true;
      }

      if (cdd->project.use_depth && (cdd->curve_type != CU_POLY)) {
        cdd->sample.use_substeps = true;
      }
    }

    if (cdd->project.use_plane) {
      normalize_v3_v3(cdd->project.plane, plane_no);
      cdd->project.plane[3] = -dot_v3v3(cdd->project.plane, plane_co);
    }
  }

  if (is_modal == false) {
    curve_draw_event_add_first(op, event);
  }

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void curve_draw_cancel(bContext * /*C*/, wmOperator *op)
{
  curve_draw_exit(op);
}

/**
 * Initialize values before calling 'exec' (when running interactively).
 */
static void curve_draw_exec_precalc(wmOperator *op)
{
  CurveDrawData *cdd = static_cast<CurveDrawData *>(op->customdata);
  const CurvePaintSettings *cps = &cdd->vc.scene->toolsettings->curve_paint_settings;
  PropertyRNA *prop;

  prop = RNA_struct_find_property(op->ptr, "fit_method");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_enum_set(op->ptr, prop, cps->fit_method);
  }

  prop = RNA_struct_find_property(op->ptr, "corner_angle");
  if (!RNA_property_is_set(op->ptr, prop)) {
    const float corner_angle = (cps->flag & CURVE_PAINT_FLAG_CORNERS_DETECT) ? cps->corner_angle :
                                                                               float(M_PI);
    RNA_property_float_set(op->ptr, prop, corner_angle);
  }

  prop = RNA_struct_find_property(op->ptr, "error_threshold");
  if (!RNA_property_is_set(op->ptr, prop)) {

    /* Error isn't set so we'll have to calculate it from the pixel values. */
    BLI_mempool_iter iter;
    const StrokeElem *selem, *selem_prev;

    float len_3d = 0.0f, len_2d = 0.0f;
    float scale_px; /* pixel to local space scale */

    int i = 0;
    BLI_mempool_iternew(cdd->stroke_elem_pool, &iter);
    selem_prev = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter));
    for (selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)); selem;
         selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)), i++)
    {
      len_3d += len_v3v3(selem->location_local, selem_prev->location_local);
      len_2d += len_v2v2(selem->mval, selem_prev->mval);
      selem_prev = selem;
    }
    scale_px = ((len_3d > 0.0f) && (len_2d > 0.0f)) ? (len_3d / len_2d) : 0.0f;
    float error_threshold = (cps->error_threshold * UI_SCALE_FAC) * scale_px;
    RNA_property_float_set(op->ptr, prop, error_threshold);
  }

  prop = RNA_struct_find_property(op->ptr, "use_cyclic");
  if (!RNA_property_is_set(op->ptr, prop)) {
    bool use_cyclic = false;

    if (BLI_mempool_len(cdd->stroke_elem_pool) > 2) {
      BLI_mempool_iter iter;
      const StrokeElem *selem, *selem_first, *selem_last;

      BLI_mempool_iternew(cdd->stroke_elem_pool, &iter);
      selem_first = selem_last = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter));
      for (selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)); selem;
           selem = static_cast<const StrokeElem *>(BLI_mempool_iterstep(&iter)))
      {
        selem_last = selem;
      }

      if (len_squared_v2v2(selem_first->mval, selem_last->mval) <=
          square_f(STROKE_CYCLIC_DIST_PX * UI_SCALE_FAC))
      {
        use_cyclic = true;
      }
    }

    RNA_property_boolean_set(op->ptr, prop, use_cyclic);
  }

  if ((cps->radius_taper_start != 0.0f) || (cps->radius_taper_end != 0.0f)) {
    /* NOTE: we could try to de-duplicate the length calculations above. */
    const int stroke_len = BLI_mempool_len(cdd->stroke_elem_pool);

    BLI_mempool_iter iter;
    StrokeElem *selem, *selem_prev;

    float *lengths = MEM_malloc_arrayN<float>(stroke_len, __func__);
    StrokeElem **selem_array = static_cast<StrokeElem **>(
        MEM_mallocN(sizeof(*selem_array) * stroke_len, __func__));
    lengths[0] = 0.0f;

    float len_3d = 0.0f;

    int i = 1;
    BLI_mempool_iternew(cdd->stroke_elem_pool, &iter);
    selem_prev = static_cast<StrokeElem *>(BLI_mempool_iterstep(&iter));
    selem_array[0] = selem_prev;
    for (selem = static_cast<StrokeElem *>(BLI_mempool_iterstep(&iter)); selem;
         selem = static_cast<StrokeElem *>(BLI_mempool_iterstep(&iter)), i++)
    {
      const float len_3d_segment = len_v3v3(selem->location_local, selem_prev->location_local);
      len_3d += len_3d_segment;
      lengths[i] = len_3d;
      selem_array[i] = selem;
      selem_prev = selem;
    }

    if (cps->radius_taper_start != 0.0f) {
      const float len_taper_max = cps->radius_taper_start * len_3d;
      for (i = 0; i < stroke_len && lengths[i] < len_taper_max; i++) {
        const float pressure_new = selem_array[i]->pressure * (lengths[i] / len_taper_max);
        stroke_elem_pressure_set(cdd, selem_array[i], pressure_new);
      }
    }

    if (cps->radius_taper_end != 0.0f) {
      const float len_taper_max = cps->radius_taper_end * len_3d;
      const float len_taper_min = len_3d - len_taper_max;
      for (i = stroke_len - 1; i > 0 && lengths[i] > len_taper_min; i--) {
        const float pressure_new = selem_array[i]->pressure *
                                   ((len_3d - lengths[i]) / len_taper_max);
        stroke_elem_pressure_set(cdd, selem_array[i], pressure_new);
      }
    }

    MEM_freeN(lengths);
    MEM_freeN(selem_array);
  }
}

static wmOperatorStatus curves_draw_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus ret = OPERATOR_RUNNING_MODAL;
  CurveDrawData *cdd = static_cast<CurveDrawData *>(op->customdata);

  UNUSED_VARS(C, op);

  if (event->type == cdd->init_event_type) {
    if (event->val == KM_RELEASE) {
      ED_region_tag_redraw(cdd->vc.region);

      curve_draw_exec_precalc(op);

      curve_draw_stroke_to_operator(op);

      curves_draw_exec(C, op);

      return OPERATOR_FINISHED;
    }
  }
  else if (ELEM(event->type, EVT_ESCKEY, RIGHTMOUSE)) {
    ED_region_tag_redraw(cdd->vc.region);
    curve_draw_cancel(C, op);
    return OPERATOR_CANCELLED;
  }
  else if (ELEM(event->type, LEFTMOUSE)) {
    if (event->val == KM_PRESS) {
      curve_draw_event_add_first(op, event);
    }
  }
  else if (ISMOUSE_MOTION(event->type)) {
    if (cdd->state == CURVE_DRAW_PAINTING) {
      const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
      if (len_squared_v2v2(mval_fl, cdd->prev.mval) > square_f(STROKE_SAMPLE_DIST_MIN_PX)) {
        curve_draw_event_add(op, event);
      }
    }
  }

  return ret;
}

void CURVES_OT_draw(wmOperatorType *ot)
{
  ot->name = "Draw Curves";
  ot->idname = __func__;
  ot->description = "Draw a freehand curve";

  ot->exec = curves_draw_exec;
  ot->invoke = curves_draw_invoke;
  ot->modal = curves_draw_modal;
  ot->poll = editable_curves_in_edit_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_float_distance(ot->srna,
                                "error_threshold",
                                0.0f,
                                0.0f,
                                10.0f,
                                "Error",
                                "Error distance threshold (in object units)",
                                0.0001f,
                                10.0f);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_AMOUNT);
  RNA_def_property_ui_range(prop, 0.0, 10, 1, 4);

  RNA_def_enum(ot->srna,
               "fit_method",
               rna_enum_curve_fit_method_items,
               CURVE_PAINT_FIT_METHOD_REFIT,
               "Fit Method",
               "");

  prop = RNA_def_float_distance(
      ot->srna, "corner_angle", DEG2RADF(70.0f), 0.0f, M_PI, "Corner Angle", "", 0.0f, M_PI);
  RNA_def_property_subtype(prop, PROP_ANGLE);

  prop = RNA_def_boolean(ot->srna, "use_cyclic", true, "Cyclic", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "is_curve_2d", false, "Curve 2D", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "bezier_as_nurbs", false, "As NURBS", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

}  // namespace blender::ed::curves
