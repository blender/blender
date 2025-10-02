/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include <cfloat>
#include <cstring>

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLT_translation.hh"

#include "BLI_array_utils.h"
#include "BLI_bit_vector.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_library.hh"
#include "BKE_mesh_types.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"
#include "BKE_object_types.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_object_vgroup.hh"
#include "ED_screen.hh"

#include "ANIM_bone_collections.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "view3d_intern.hh" /* own include */

/* ******************* view3d space & buttons ************** */
enum {
  B_REDR = 2,
  B_TRANSFORM_PANEL_MEDIAN = 1008,
  B_TRANSFORM_PANEL_DIMS = 1009,
};

/* All must start w/ location */

struct TransformMedian_Generic {
  float location[3];
};

struct TransformMedian_Mesh {
  float location[3], bv_weight, v_crease, be_weight, skin[2], e_crease;
};

struct TransformMedian_Curve {
  float location[3], weight, b_weight, radius, tilt;
};

struct TransformMedian_Lattice {
  float location[3], weight;
};

struct TransformMedian_Curves {
  float location[3], nurbs_weight, radius, tilt;
};

union TransformMedian {
  TransformMedian_Generic generic;
  TransformMedian_Mesh mesh;
  TransformMedian_Curve curve;
  TransformMedian_Lattice lattice;
  TransformMedian_Curves curves;
};

struct CurvesDataPanelState {
  KnotsMode nurbs_knot_mode;
  int order;
  int resolution;
  char cyclic;
};

/* temporary struct for storing transform properties */

struct TransformProperties {
  float ob_obmat_orig[4][4];
  float ob_dims_orig[3];
  float ob_scale_orig[3];
  float ob_dims[3];
  blender::Vector<float> vertex_weights;

  CurvesDataPanelState modified, current;

  /* Floats only (treated as an array). */
  TransformMedian ve_median, median;
  bool tag_for_update;
};

#define TRANSFORM_MEDIAN_ARRAY_LEN (sizeof(TransformMedian) / sizeof(float))

static TransformProperties *v3d_transform_props_ensure(View3D *v3d);

/* -------------------------------------------------------------------- */
/** \name Edit Mesh Partial Updates
 * \{ */

static void *editmesh_partial_update_begin_fn(bContext * /*C*/,
                                              const uiBlockInteraction_Params *params,
                                              void *arg1)
{
  const int retval_test = B_TRANSFORM_PANEL_MEDIAN;
  if (BLI_array_findindex(
          params->unique_retval_ids, params->unique_retval_ids_len, &retval_test) == -1)
  {
    return nullptr;
  }

  BMEditMesh *em = static_cast<BMEditMesh *>(arg1);

  int verts_mask_count = 0;
  BMIter iter;
  BMVert *eve;
  int i;

  blender::BitVector<> verts_mask(em->bm->totvert);
  BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
    if (!BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
      continue;
    }
    verts_mask[i].set();
    verts_mask_count += 1;
  }

  BMPartialUpdate_Params update_params{};
  update_params.do_tessellate = true;
  update_params.do_normals = true;
  BMPartialUpdate *bmpinfo = BM_mesh_partial_create_from_verts_group_single(
      *em->bm, update_params, verts_mask, verts_mask_count);

  return bmpinfo;
}

static void editmesh_partial_update_end_fn(bContext * /*C*/,
                                           const uiBlockInteraction_Params * /*params*/,
                                           void * /*arg1*/,
                                           void *user_data)
{
  BMPartialUpdate *bmpinfo = static_cast<BMPartialUpdate *>(user_data);
  if (bmpinfo == nullptr) {
    return;
  }
  BM_mesh_partial_destroy(bmpinfo);
}

static void editmesh_partial_update_update_fn(bContext *C,
                                              const uiBlockInteraction_Params * /*params*/,
                                              void *arg1,
                                              void *user_data)
{
  BMPartialUpdate *bmpinfo = static_cast<BMPartialUpdate *>(user_data);
  if (bmpinfo == nullptr) {
    return;
  }

  View3D *v3d = CTX_wm_view3d(C);
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);
  if (tfp->tag_for_update == false) {
    return;
  }
  tfp->tag_for_update = false;

  BMEditMesh *em = static_cast<BMEditMesh *>(arg1);

  BKE_editmesh_looptris_and_normals_calc_with_partial(em, bmpinfo);
}

/** \} */

/* Helper function to compute a median changed value,
 * when the value should be clamped in [0.0, 1.0].
 * Returns either 0.0, 1.0 (both can be applied directly), a positive scale factor
 * for scale down, or a negative one for scale up.
 */
static float compute_scale_factor(const float ve_median, const float median)
{
  if (ve_median <= 0.0f) {
    return 0.0f;
  }
  if (ve_median >= 1.0f) {
    return 1.0f;
  }

  /* Scale value to target median. */
  float median_new = ve_median;
  float median_orig = ve_median - median; /* Previous median value. */

  /* In case of floating point error. */
  CLAMP(median_orig, 0.0f, 1.0f);
  CLAMP(median_new, 0.0f, 1.0f);

  if (median_new <= median_orig) {
    /* Scale down. */
    return median_new / median_orig;
  }

  /* Scale up, negative to indicate it... */
  return -(1.0f - median_new) / (1.0f - median_orig);
}

/**
 * Apply helpers.
 * \note In case we only have one element,
 * copy directly the value instead of applying the diff or scale factor.
 * Avoids some glitches when going e.g. from 3 to 0.0001 (see #37327).
 */
static void apply_raw_diff(float *val, const int tot, const float ve_median, const float median)
{
  *val = (tot == 1) ? ve_median : (*val + median);
}

static void apply_raw_diff_v3(float val[3],
                              const int tot,
                              const float ve_median[3],
                              const float median[3])
{
  if (tot == 1) {
    copy_v3_v3(val, ve_median);
  }
  else {
    add_v3_v3(val, median);
  }
}

static void apply_scale_factor(
    float *val, const int tot, const float ve_median, const float median, const float sca)
{
  if (tot == 1 || ve_median == median) {
    *val = ve_median;
  }
  else {
    *val *= sca;
  }
}

static void apply_scale_factor_clamp(float *val,
                                     const int tot,
                                     const float ve_median,
                                     const float sca)
{
  if (tot == 1) {
    *val = ve_median;
    CLAMP(*val, 0.0f, 1.0f);
  }
  else if (ELEM(sca, 0.0f, 1.0f)) {
    *val = sca;
  }
  else {
    *val = (sca > 0.0f) ? (*val * sca) : (1.0f + ((1.0f - *val) * sca));
    CLAMP(*val, 0.0f, 1.0f);
  }
}

static TransformProperties *v3d_transform_props_ensure(View3D *v3d)
{
  if (v3d->runtime.properties_storage == nullptr) {
    TransformProperties *tfp = MEM_new<TransformProperties>("TransformProperties");
    /* Construct C++ structures in otherwise zero initialized struct. */
    new (tfp) TransformProperties();

    v3d->runtime.properties_storage = tfp;
    v3d->runtime.properties_storage_free = [](void *properties_storage) {
      MEM_delete(static_cast<TransformProperties *>(properties_storage));
    };
  }
  return static_cast<TransformProperties *>(v3d->runtime.properties_storage);
}

struct CurvesPointSelectionStatus {
  TransformMedian_Curves median = {};
  int total = 0;
  int total_curve_points = 0;
  int total_nurbs_weights = 0;

  static CurvesPointSelectionStatus sum(const CurvesPointSelectionStatus &a,
                                        const CurvesPointSelectionStatus &b)
  {
    CurvesPointSelectionStatus result;
    add_v3_v3v3(result.median.location, a.median.location, b.median.location);
    result.median.nurbs_weight = a.median.nurbs_weight + b.median.nurbs_weight;
    result.median.radius = a.median.radius + b.median.radius;
    result.median.tilt = a.median.tilt + b.median.tilt;
    result.total = a.total + b.total;
    result.total_curve_points = a.total_curve_points + b.total_curve_points;
    result.total_nurbs_weights = a.total_nurbs_weights + b.total_nurbs_weights;
    return result;
  }
};

static CurvesPointSelectionStatus init_curves_point_selection_status(
    const blender::bke::CurvesGeometry &curves)
{
  using namespace blender;
  using namespace ed::curves;

  if (curves.is_empty()) {
    return CurvesPointSelectionStatus();
  }
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<int8_t> curve_types = curves.curve_types();
  const std::optional<Span<float>> nurbs_weights = curves.nurbs_weights();
  const VArray<float> radius = curves.radius();
  const VArray<float> tilt = curves.tilt();
  const Span<float3> positions = curves.positions();

  IndexMaskMemory memory;
  const IndexMask selection = retrieve_selected_points(curves, memory);

  CurvesPointSelectionStatus status = threading::parallel_reduce(
      curves.curves_range(),
      512,
      CurvesPointSelectionStatus(),
      [&](const IndexRange range, const CurvesPointSelectionStatus &acc) {
        CurvesPointSelectionStatus value = acc;

        for (const int curve : range) {
          const IndexRange points = points_by_curve[curve];
          const CurveType curve_type = CurveType(curve_types[curve]);
          const bool is_nurbs = curve_type == CURVE_TYPE_NURBS;
          const IndexMask curve_selection = selection.slice_content(points);

          value.total += curve_selection.size();
          value.total_curve_points += curve_selection.size();

          curve_selection.foreach_index([&](const int point) {
            add_v3_v3(value.median.location, positions[point]);
            value.total_nurbs_weights += is_nurbs;
            value.median.nurbs_weight += is_nurbs ?
                                             (nurbs_weights ? (*nurbs_weights)[point] : 1.0f) :
                                             0;
            value.median.radius += radius[point];
            value.median.tilt += tilt[point];
          });
        }
        return value;
      },
      CurvesPointSelectionStatus::sum);

  if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return status;
  }

  const IndexMask bezier_points = bke::curves::curve_type_point_selection(
      curves, CURVE_TYPE_BEZIER, memory);

  auto add_handles = [&](StringRef selection_attribute, std::optional<Span<float3>> positions) {
    if (!positions) {
      return;
    }
    const IndexMask selection = retrieve_selected_points(
        curves, selection_attribute, bezier_points, memory);
    if (selection.is_empty()) {
      return;
    }

    status.total += selection.size();

    selection.foreach_index(
        [&](const int point) { add_v3_v3(status.median.location, (*positions)[point]); });
  };

  add_handles(".selection_handle_left", curves.handle_positions_left());
  add_handles(".selection_handle_right", curves.handle_positions_right());
  return status;
}

static bool apply_to_curves_point_selection(const int tot,
                                            const TransformMedian_Curves &median,
                                            const TransformMedian_Curves &ve_median,
                                            blender::bke::CurvesGeometry &curves)
{
  using namespace blender;
  using namespace ed::curves;
  if (curves.is_empty()) {
    return false;
  }

  bool changed = false;

  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<int8_t> curve_types = curves.curve_types();
  const MutableSpan<float> nurbs_weights = median.nurbs_weight ? curves.nurbs_weights_for_write() :
                                                                 MutableSpan<float>{};
  const MutableSpan<float> radius = median.radius ? curves.radius_for_write() :
                                                    MutableSpan<float>{};
  const MutableSpan<float> tilt = median.tilt ? curves.tilt_for_write() : MutableSpan<float>{};

  IndexMaskMemory memory;
  const IndexMask selection = retrieve_selected_points(curves, memory);
  const bool update_location = math::length_manhattan(float3(median.location)) > 0;
  MutableSpan<float3> positions = update_location && !selection.is_empty() ?
                                      curves.positions_for_write() :
                                      MutableSpan<float3>();

  threading::parallel_for(curves.curves_range(), 512, [&](const IndexRange range) {
    for (const int curve : range) {
      const IndexRange points = points_by_curve[curve];
      const CurveType curve_type = CurveType(curve_types[curve]);
      const bool is_nurbs = curve_type == CURVE_TYPE_NURBS;
      const IndexMask curve_selection = selection.slice_content(points);

      if (!curve_selection.is_empty()) {
        changed = true;
      }

      curve_selection.foreach_index([&](const int point) {
        if (is_nurbs && median.nurbs_weight) {
          apply_raw_diff(&nurbs_weights[point], tot, ve_median.nurbs_weight, median.nurbs_weight);
          nurbs_weights[point] = math::clamp(nurbs_weights[point], 0.01f, 100.0f);
        }
        if (median.radius) {
          apply_raw_diff(&radius[point], tot, ve_median.radius, median.radius);
        }
        if (median.tilt) {
          apply_raw_diff(&tilt[point], tot, ve_median.tilt, median.tilt);
        }
        if (update_location) {
          apply_raw_diff_v3(positions[point], tot, ve_median.location, median.location);
        }
      });
    }
  });

  /* Only location can be changed for Bezier handles. */
  if (!update_location || !curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return changed;
  }

  const IndexMask bezier_points = bke::curves::curve_type_point_selection(
      curves, CURVE_TYPE_BEZIER, memory);

  auto apply_to_handles = [&](StringRef selection_attribute, StringRef handles_attribute) {
    const IndexMask selection = retrieve_selected_points(
        curves, selection_attribute, bezier_points, memory);
    if (selection.is_empty()) {
      return;
    }

    bke::SpanAttributeWriter<float3> handles =
        curves.attributes_for_write().lookup_for_write_span<float3>(handles_attribute);
    selection.foreach_index(GrainSize(2048), [&](const int point) {
      apply_raw_diff_v3(handles.span[point], tot, ve_median.location, median.location);
    });
    handles.finish();

    changed = true;
  };

  apply_to_handles(".selection_handle_left", "handle_left");
  apply_to_handles(".selection_handle_right", "handle_right");

  if (changed) {
    curves.calculate_bezier_auto_handles();
  }

  return changed;
}

struct CurvesSelectionStatus {
  int curve_count = 0;
  int nurbs_count = 0;
  int bezier_count = 0;
  int poly_count = 0;

  int cyclic_count = 0;
  int nurbs_knot_mode_sum = 0;
  int nurbs_knot_mode_max = 0;
  int order_sum = 0;
  int order_max = 0;
  int resolution_sum = 0;
  int resolution_max = 0;

  static CurvesSelectionStatus sum(const CurvesSelectionStatus &a, const CurvesSelectionStatus &b)
  {
    return {a.curve_count + b.curve_count,
            a.nurbs_count + b.nurbs_count,
            a.bezier_count + b.bezier_count,
            a.poly_count + b.poly_count,
            a.cyclic_count + b.cyclic_count,
            a.nurbs_knot_mode_sum + b.nurbs_knot_mode_sum,
            std::max(a.nurbs_knot_mode_max, b.nurbs_knot_mode_max),
            a.order_sum + b.order_sum,
            std::max(a.order_max, b.order_max),
            a.resolution_sum + b.resolution_sum,
            std::max(a.resolution_max, b.resolution_max)};
  }
};

static CurvesSelectionStatus init_curves_selection_status(
    const blender::bke::CurvesGeometry &curves)
{
  using namespace blender;
  using namespace ed::curves;

  if (curves.is_empty()) {
    return CurvesSelectionStatus();
  }
  const VArray<int8_t> curve_types = curves.curve_types();
  const VArray<bool> cyclic = curves.cyclic();
  const VArray<int8_t> nurbs_knot_modes = curves.nurbs_knots_modes();
  const VArray<int8_t> orders = curves.nurbs_orders();
  const VArray<int> resolution = curves.resolution();

  IndexMaskMemory memory;
  const IndexMask selection = retrieve_selected_curves(curves, memory);

  return threading::parallel_reduce(
      curves.curves_range(),
      512,
      CurvesSelectionStatus(),
      [&](const IndexRange range, const CurvesSelectionStatus &acc) {
        CurvesSelectionStatus value = acc;

        selection.slice_content(range).foreach_index([&](const int curve) {
          const CurveType curve_type = CurveType(curve_types[curve]);
          const bool is_nurbs = curve_type == CURVE_TYPE_NURBS;
          const bool is_bezier = curve_type == CURVE_TYPE_BEZIER;
          const bool is_poly = curve_type == CURVE_TYPE_POLY;

          value.curve_count++;
          value.nurbs_count += is_nurbs;
          value.bezier_count += is_bezier;
          value.poly_count += is_poly;

          value.cyclic_count += cyclic[curve];

          const int order = is_nurbs ? orders[curve] : 0;
          value.order_sum += order;
          value.order_max = std::max(value.order_max, order);

          const int nurbs_knot_mode = is_nurbs ? nurbs_knot_modes[curve] : 0;
          value.nurbs_knot_mode_sum += nurbs_knot_mode;
          value.nurbs_knot_mode_max = std::max(value.nurbs_knot_mode_max, nurbs_knot_mode);

          const int res = resolution[curve];
          value.resolution_sum += res;
          value.resolution_max = std::max(value.resolution_max, res);
        });
        return value;
      },
      CurvesSelectionStatus::sum);
}

/* is used for both read and write... */
static void v3d_editvertex_buts(
    const bContext *C, uiLayout *layout, View3D *v3d, Object *ob, float lim)
{
  using namespace blender;
  uiBlock *block = (layout) ? layout->absolute_block() : nullptr;
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);
  TransformMedian median_basis, ve_median_basis;
  int tot, totedgedata, totcurvedata, totlattdata, totcurvebweight;
  int total_curve_points_data = 0;
  bool has_meshdata = false;
  bool has_skinradius = false;
  PointerRNA data_ptr;

  copy_vn_fl((float *)&median_basis, TRANSFORM_MEDIAN_ARRAY_LEN, 0.0f);
  tot = totedgedata = totcurvedata = totlattdata = totcurvebweight = 0;

  if (ob->type == OB_MESH) {
    TransformMedian_Mesh *median = &median_basis.mesh;
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    BMEditMesh *em = mesh->runtime->edit_mesh.get();
    BMesh *bm = em->bm;
    BMVert *eve;
    BMEdge *eed;
    BMIter iter;

    const int cd_vert_bweight_offset = CustomData_get_offset_named(
        &bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert");
    const int cd_vert_crease_offset = CustomData_get_offset_named(
        &bm->vdata, CD_PROP_FLOAT, "crease_vert");
    const int cd_vert_skin_offset = CustomData_get_offset(&bm->vdata, CD_MVERT_SKIN);
    const int cd_edge_bweight_offset = CustomData_get_offset_named(
        &bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
    const int cd_edge_crease_offset = CustomData_get_offset_named(
        &bm->edata, CD_PROP_FLOAT, "crease_edge");

    has_skinradius = (cd_vert_skin_offset != -1);

    if (bm->totvertsel) {
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
          tot++;
          add_v3_v3(median->location, eve->co);

          if (cd_vert_bweight_offset != -1) {
            median->bv_weight += BM_ELEM_CD_GET_FLOAT(eve, cd_vert_bweight_offset);
          }

          if (cd_vert_crease_offset != -1) {
            median->v_crease += BM_ELEM_CD_GET_FLOAT(eve, cd_vert_crease_offset);
          }

          if (has_skinradius) {
            MVertSkin *vs = static_cast<MVertSkin *>(
                BM_ELEM_CD_GET_VOID_P(eve, cd_vert_skin_offset));
            add_v2_v2(median->skin, vs->radius); /* Third val not used currently. */
          }
        }
      }
    }

    if ((cd_edge_bweight_offset != -1) || (cd_edge_crease_offset != -1)) {
      if (bm->totedgesel) {
        BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
          if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
            if (cd_edge_bweight_offset != -1) {
              median->be_weight += BM_ELEM_CD_GET_FLOAT(eed, cd_edge_bweight_offset);
            }

            if (cd_edge_crease_offset != -1) {
              median->e_crease += BM_ELEM_CD_GET_FLOAT(eed, cd_edge_crease_offset);
            }

            totedgedata++;
          }
        }
      }
    }
    else {
      totedgedata = bm->totedgesel;
    }

    has_meshdata = (tot || totedgedata);
  }
  else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
    TransformMedian_Curve *median = &median_basis.curve;
    Curve *cu = static_cast<Curve *>(ob->data);
    BPoint *bp;
    BezTriple *bezt;
    int a;
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);
    StructRNA *seltype = nullptr;
    void *selp = nullptr;

    LISTBASE_FOREACH (Nurb *, nu, nurbs) {
      if (nu->type == CU_BEZIER) {
        bezt = nu->bezt;
        a = nu->pntsu;
        while (a--) {
          if (bezt->f2 & SELECT) {
            add_v3_v3(median->location, bezt->vec[1]);
            tot++;
            median->weight += bezt->weight;
            median->radius += bezt->radius;
            median->tilt += bezt->tilt;
            if (!totcurvedata) { /* I.e. first time... */
              selp = bezt;
              seltype = &RNA_BezierSplinePoint;
            }
            totcurvedata++;
          }
          else {
            if (bezt->f1 & SELECT) {
              add_v3_v3(median->location, bezt->vec[0]);
              tot++;
            }
            if (bezt->f3 & SELECT) {
              add_v3_v3(median->location, bezt->vec[2]);
              tot++;
            }
          }
          bezt++;
        }
      }
      else {
        bp = nu->bp;
        a = nu->pntsu * nu->pntsv;
        while (a--) {
          if (bp->f1 & SELECT) {
            add_v3_v3(median->location, bp->vec);
            median->b_weight += bp->vec[3];
            totcurvebweight++;
            tot++;
            median->weight += bp->weight;
            median->radius += bp->radius;
            median->tilt += bp->tilt;
            if (!totcurvedata) { /* I.e. first time... */
              selp = bp;
              seltype = &RNA_SplinePoint;
            }
            totcurvedata++;
          }
          bp++;
        }
      }
    }

    if (totcurvedata == 1) {
      data_ptr = RNA_pointer_create_discrete(&cu->id, seltype, selp);
    }
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = static_cast<Lattice *>(ob->data);
    TransformMedian_Lattice *median = &median_basis.lattice;
    BPoint *bp;
    int a;
    StructRNA *seltype = nullptr;
    void *selp = nullptr;

    a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
    bp = lt->editlatt->latt->def;
    while (a--) {
      if (bp->f1 & SELECT) {
        add_v3_v3(median->location, bp->vec);
        tot++;
        median->weight += bp->weight;
        if (!totlattdata) { /* I.e. first time... */
          selp = bp;
          seltype = &RNA_LatticePoint;
        }
        totlattdata++;
      }
      bp++;
    }

    if (totlattdata == 1) {
      data_ptr = RNA_pointer_create_discrete(&lt->id, seltype, selp);
    }
  }
  else if (ELEM(ob->type, OB_GREASE_PENCIL, OB_CURVES)) {
    CurvesPointSelectionStatus status;

    if (ob->type == OB_GREASE_PENCIL) {
      using namespace ed::greasepencil;
      using namespace ed::curves;
      Scene &scene = *CTX_data_scene(C);
      GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
      Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);

      status = threading::parallel_reduce(
          drawings.index_range(),
          1L,
          CurvesPointSelectionStatus(),
          [&](const IndexRange range, const CurvesPointSelectionStatus &acc) {
            CurvesPointSelectionStatus value = acc;
            for (const int drawing : range) {
              value = CurvesPointSelectionStatus::sum(
                  value, init_curves_point_selection_status(drawings[drawing].drawing.strokes()));
            }
            return value;
          },
          CurvesPointSelectionStatus::sum);
    }
    else {
      using namespace ed::curves;
      const Curves &curves_id = *static_cast<Curves *>(ob->data);
      status = init_curves_point_selection_status(curves_id.geometry.wrap());
    }

    TransformMedian_Curves &median = median_basis.curves;
    median = status.median;
    tot = status.total;
    total_curve_points_data = status.total_curve_points;
    totcurvebweight = status.total_nurbs_weights;
  }

  if (tot == 0) {
    uiDefBut(
        block, ButType::Label, 0, IFACE_("Nothing selected"), 0, 130, 200, 20, nullptr, 0, 0, "");
    return;
  }

  /* Location, X/Y/Z */
  mul_v3_fl(median_basis.generic.location, 1.0f / float(tot));
  if (v3d->flag & V3D_GLOBAL_STATS) {
    mul_m4_v3(ob->object_to_world().ptr(), median_basis.generic.location);
  }

  if (has_meshdata) {
    TransformMedian_Mesh *median = &median_basis.mesh;
    if (totedgedata) {
      median->e_crease /= float(totedgedata);
      median->be_weight /= float(totedgedata);
    }
    if (tot) {
      median->bv_weight /= float(tot);
      median->v_crease /= float(tot);
      if (has_skinradius) {
        median->skin[0] /= float(tot);
        median->skin[1] /= float(tot);
      }
    }
  }
  else if (total_curve_points_data) {
    TransformMedian_Curves &median = median_basis.curves;
    if (totcurvebweight) {
      median.nurbs_weight /= totcurvebweight;
    }
    median.radius /= total_curve_points_data;
    median.tilt /= total_curve_points_data;
  }
  else if (totcurvedata) {
    TransformMedian_Curve *median = &median_basis.curve;
    if (totcurvebweight) {
      median->b_weight /= float(totcurvebweight);
    }
    median->weight /= float(totcurvedata);
    median->radius /= float(totcurvedata);
    median->tilt /= float(totcurvedata);
  }
  else if (totlattdata) {
    TransformMedian_Lattice *median = &median_basis.lattice;
    median->weight /= float(totlattdata);
  }

  if (block) { /* buttons */
    uiBut *but;
    int yi = 200;
    const float tilt_limit = DEG2RADF(21600.0f);
    const int butw = 200;
    const int buth = 20 * UI_SCALE_FAC;
    const int but_margin = 2;
    const char *c;

    memcpy(&tfp->ve_median, &median_basis, sizeof(tfp->ve_median));

    UI_block_align_begin(block);
    if (tot == 1) {
      if (totcurvedata) {
        /* Curve */
        c = IFACE_("Control Point:");
      }
      else if (ELEM(ob->type, OB_CURVES, OB_GREASE_PENCIL)) {
        c = IFACE_("Point:");
      }
      else {
        /* Mesh or lattice */
        c = IFACE_("Vertex:");
      }
    }
    else {
      c = IFACE_("Median:");
    }
    uiDefBut(block, ButType::Label, 0, c, 0, yi -= buth, butw, buth, nullptr, 0, 0, "");

    UI_block_align_begin(block);

    /* Should be no need to translate these. */
    but = uiDefButF(block,
                    ButType::Num,
                    B_TRANSFORM_PANEL_MEDIAN,
                    IFACE_("X:"),
                    0,
                    yi -= buth,
                    butw,
                    buth,
                    &tfp->ve_median.generic.location[0],
                    -lim,
                    lim,
                    "");
    UI_but_number_step_size_set(but, 10);
    UI_but_number_precision_set(but, RNA_TRANSLATION_PREC_DEFAULT);
    UI_but_unit_type_set(but, PROP_UNIT_LENGTH);
    but = uiDefButF(block,
                    ButType::Num,
                    B_TRANSFORM_PANEL_MEDIAN,
                    IFACE_("Y:"),
                    0,
                    yi -= buth,
                    butw,
                    buth,
                    &tfp->ve_median.generic.location[1],
                    -lim,
                    lim,
                    "");
    UI_but_number_step_size_set(but, 10);
    UI_but_number_precision_set(but, RNA_TRANSLATION_PREC_DEFAULT);
    UI_but_unit_type_set(but, PROP_UNIT_LENGTH);
    but = uiDefButF(block,
                    ButType::Num,
                    B_TRANSFORM_PANEL_MEDIAN,
                    IFACE_("Z:"),
                    0,
                    yi -= buth,
                    butw,
                    buth,
                    &tfp->ve_median.generic.location[2],
                    -lim,
                    lim,
                    "");
    UI_but_number_step_size_set(but, 10);
    UI_but_number_precision_set(but, RNA_TRANSLATION_PREC_DEFAULT);
    UI_but_unit_type_set(but, PROP_UNIT_LENGTH);

    if (totcurvebweight == tot) {
      float &weight = ELEM(ob->type, OB_CURVES, OB_GREASE_PENCIL) ?
                          tfp->ve_median.curves.nurbs_weight :
                          tfp->ve_median.curve.b_weight;
      but = uiDefButF(block,
                      ButType::Num,
                      B_TRANSFORM_PANEL_MEDIAN,
                      IFACE_("W:"),
                      0,
                      yi -= buth,
                      butw,
                      buth,
                      &weight,
                      0.01,
                      100.0,
                      "");
      UI_but_number_step_size_set(but, 1);
      UI_but_number_precision_set(but, 3);
    }

    UI_block_align_begin(block);
    uiDefButBitS(block,
                 ButType::Toggle,
                 V3D_GLOBAL_STATS,
                 B_REDR,
                 IFACE_("Global"),
                 0,
                 yi -= buth + but_margin,
                 100,
                 buth,
                 &v3d->flag,
                 0,
                 0,
                 TIP_("Displays global values"));
    uiDefButBitS(block,
                 ButType::ToggleN,
                 V3D_GLOBAL_STATS,
                 B_REDR,
                 IFACE_("Local"),
                 100,
                 yi,
                 100,
                 buth,
                 &v3d->flag,
                 0,
                 0,
                 TIP_("Displays local values"));
    UI_block_align_end(block);

    /* Meshes... */
    if (has_meshdata) {
      TransformMedian_Mesh *ve_median = &tfp->ve_median.mesh;
      if (tot) {
        uiDefBut(block,
                 ButType::Label,
                 0,
                 tot == 1 ? IFACE_("Vertex Data:") : IFACE_("Vertices Data:"),
                 0,
                 yi -= buth + but_margin,
                 butw,
                 buth,
                 nullptr,
                 0.0,
                 0.0,
                 "");
        /* customdata layer added on demand */
        but = uiDefButF(block,
                        ButType::Num,
                        B_TRANSFORM_PANEL_MEDIAN,
                        tot == 1 ? IFACE_("Bevel Weight:") : IFACE_("Mean Bevel Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->bv_weight,
                        0.0,
                        1.0,
                        TIP_("Vertex weight used by Bevel modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 2);
        /* customdata layer added on demand */
        but = uiDefButF(block,
                        ButType::Num,
                        B_TRANSFORM_PANEL_MEDIAN,
                        tot == 1 ? IFACE_("Vertex Crease:") : IFACE_("Mean Vertex Crease:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->v_crease,
                        0.0,
                        1.0,
                        TIP_("Weight used by the Subdivision Surface modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 2);
      }
      if (has_skinradius) {
        UI_block_align_begin(block);
        but = uiDefButF(block,
                        ButType::Num,
                        B_TRANSFORM_PANEL_MEDIAN,
                        tot == 1 ? IFACE_("Radius X:") : IFACE_("Mean Radius X:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->skin[0],
                        0.0,
                        100.0,
                        TIP_("X radius used by Skin modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        but = uiDefButF(block,
                        ButType::Num,
                        B_TRANSFORM_PANEL_MEDIAN,
                        tot == 1 ? IFACE_("Radius Y:") : IFACE_("Mean Radius Y:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->skin[1],
                        0.0,
                        100.0,
                        TIP_("Y radius used by Skin modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        UI_block_align_end(block);
      }
      if (totedgedata) {
        uiDefBut(block,
                 ButType::Label,
                 0,
                 totedgedata == 1 ? IFACE_("Edge Data:") : IFACE_("Edges Data:"),
                 0,
                 yi -= buth + but_margin,
                 butw,
                 buth,
                 nullptr,
                 0.0,
                 0.0,
                 "");
        /* customdata layer added on demand */
        but = uiDefButF(block,
                        ButType::Num,
                        B_TRANSFORM_PANEL_MEDIAN,
                        totedgedata == 1 ? IFACE_("Bevel Weight:") : IFACE_("Mean Bevel Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->be_weight,
                        0.0,
                        1.0,
                        TIP_("Edge weight used by Bevel modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 2);
        /* customdata layer added on demand */
        but = uiDefButF(block,
                        ButType::Num,
                        B_TRANSFORM_PANEL_MEDIAN,
                        totedgedata == 1 ? IFACE_("Crease:") : IFACE_("Mean Crease:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->e_crease,
                        0.0,
                        1.0,
                        TIP_("Weight used by the Subdivision Surface modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 2);
      }
    }
    /* Curve or GP... */
    else if (total_curve_points_data) {
      const bool is_single = total_curve_points_data == 1;
      TransformMedian_Curves *ve_median = &tfp->ve_median.curves;

      but = uiDefButF(block,
                      ButType::Num,
                      B_TRANSFORM_PANEL_MEDIAN,
                      is_single ? IFACE_("Radius:") : IFACE_("Mean Radius:"),
                      0,
                      yi -= buth + but_margin,
                      butw,
                      buth,
                      &ve_median->radius,
                      0.0,
                      100.0,
                      is_single ?
                          std::nullopt :
                          std::optional<StringRef>{TIP_("Radius of curve control points")});
      UI_but_number_step_size_set(but, 1);
      UI_but_number_precision_set(but, 3);
      but = uiDefButF(block,
                      ButType::Num,
                      B_TRANSFORM_PANEL_MEDIAN,
                      is_single ? IFACE_("Tilt:") : IFACE_("Mean Tilt:"),
                      0,
                      yi -= buth + but_margin,
                      butw,
                      buth,
                      &ve_median->tilt,
                      -tilt_limit,
                      tilt_limit,
                      is_single ? std::nullopt :
                                  std::optional<StringRef>{TIP_("Tilt of curve control points")});
      UI_but_number_step_size_set(but, 1);
      UI_but_number_precision_set(but, 3);
      UI_but_unit_type_set(but, PROP_UNIT_ROTATION);
    }
    /* Curve... */
    else if (totcurvedata) {
      TransformMedian_Curve *ve_median = &tfp->ve_median.curve;
      if (totcurvedata == 1) {
        but = uiDefButR(block,
                        ButType::Num,
                        0,
                        IFACE_("Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &data_ptr,
                        "weight_softbody",
                        0,
                        0.0,
                        1.0,
                        std::nullopt);
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        but = uiDefButR(block,
                        ButType::Num,
                        0,
                        IFACE_("Radius:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &data_ptr,
                        "radius",
                        0,
                        0.0,
                        100.0,
                        std::nullopt);
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        but = uiDefButR(block,
                        ButType::Num,
                        0,
                        IFACE_("Tilt:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &data_ptr,
                        "tilt",
                        0,
                        -tilt_limit,
                        tilt_limit,
                        std::nullopt);
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
      }
      else if (totcurvedata > 1) {
        but = uiDefButF(block,
                        ButType::Num,
                        B_TRANSFORM_PANEL_MEDIAN,
                        IFACE_("Mean Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->weight,
                        0.0,
                        1.0,
                        TIP_("Weight used for Soft Body Goal"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        but = uiDefButF(block,
                        ButType::Num,
                        B_TRANSFORM_PANEL_MEDIAN,
                        IFACE_("Mean Radius:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->radius,
                        0.0,
                        100.0,
                        TIP_("Radius of curve control points"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        but = uiDefButF(block,
                        ButType::Num,
                        B_TRANSFORM_PANEL_MEDIAN,
                        IFACE_("Mean Tilt:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->tilt,
                        -tilt_limit,
                        tilt_limit,
                        TIP_("Tilt of curve control points"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        UI_but_unit_type_set(but, PROP_UNIT_ROTATION);
      }
    }
    /* Lattice... */
    else if (totlattdata) {
      TransformMedian_Lattice *ve_median = &tfp->ve_median.lattice;
      if (totlattdata == 1) {
        but = uiDefButR(block,
                        ButType::Num,
                        0,
                        IFACE_("Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &data_ptr,
                        "weight_softbody",
                        0,
                        0.0,
                        1.0,
                        std::nullopt);
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
      }
      else if (totlattdata > 1) {
        but = uiDefButF(block,
                        ButType::Num,
                        B_TRANSFORM_PANEL_MEDIAN,
                        IFACE_("Mean Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->weight,
                        0.0,
                        1.0,
                        TIP_("Weight used for Soft Body Goal"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
      }
    }

    UI_block_align_end(block);

    if (ob->type == OB_MESH) {
      Mesh *mesh = static_cast<Mesh *>(ob->data);
      if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
        uiBlockInteraction_CallbackData callback_data{};
        callback_data.begin_fn = editmesh_partial_update_begin_fn;
        callback_data.end_fn = editmesh_partial_update_end_fn;
        callback_data.update_fn = editmesh_partial_update_update_fn;
        callback_data.arg1 = em;
        UI_block_interaction_set(block, &callback_data);
      }
    }
  }
  else { /* apply */
    memcpy(&ve_median_basis, &tfp->ve_median, sizeof(tfp->ve_median));

    if (v3d->flag & V3D_GLOBAL_STATS) {
      invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
      mul_m4_v3(ob->world_to_object().ptr(), median_basis.generic.location);
      mul_m4_v3(ob->world_to_object().ptr(), ve_median_basis.generic.location);
    }
    sub_vn_vnvn((float *)&median_basis,
                (float *)&ve_median_basis,
                (float *)&median_basis,
                TRANSFORM_MEDIAN_ARRAY_LEN);

    /* Note with a single element selected, we always do. */
    const bool apply_vcos = (tot == 1) || (len_squared_v3(median_basis.generic.location) != 0.0f);

    if ((ob->type == OB_MESH) &&
        (apply_vcos || median_basis.mesh.bv_weight || median_basis.mesh.v_crease ||
         median_basis.mesh.skin[0] || median_basis.mesh.skin[1] || median_basis.mesh.be_weight ||
         median_basis.mesh.e_crease))
    {
      const TransformMedian_Mesh *median = &median_basis.mesh, *ve_median = &ve_median_basis.mesh;
      Mesh *mesh = static_cast<Mesh *>(ob->data);
      BMEditMesh *em = mesh->runtime->edit_mesh.get();
      BMesh *bm = em->bm;
      BMIter iter;
      BMVert *eve;
      BMEdge *eed;

      int cd_vert_bweight_offset = -1;
      int cd_vert_crease_offset = -1;
      int cd_vert_skin_offset = -1;
      int cd_edge_bweight_offset = -1;
      int cd_edge_crease_offset = -1;

      float scale_bv_weight = 1.0f;
      float scale_v_crease = 1.0f;
      float scale_skin[2] = {1.0f, 1.0f};
      float scale_be_weight = 1.0f;
      float scale_e_crease = 1.0f;

      /* Vertices */

      if (apply_vcos || median->bv_weight || median->v_crease || median->skin[0] ||
          median->skin[1])
      {
        if (median->bv_weight) {
          if (!CustomData_has_layer_named(&bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert")) {
            BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert");
          }
          cd_vert_bweight_offset = CustomData_get_offset_named(
              &bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert");
          BLI_assert(cd_vert_bweight_offset != -1);

          scale_bv_weight = compute_scale_factor(ve_median->bv_weight, median->bv_weight);
        }

        if (median->v_crease) {
          if (!CustomData_has_layer_named(&bm->vdata, CD_PROP_FLOAT, "crease_vert")) {
            BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_FLOAT, "crease_vert");
          }
          cd_vert_crease_offset = CustomData_get_offset_named(
              &bm->vdata, CD_PROP_FLOAT, "crease_vert");
          BLI_assert(cd_vert_crease_offset != -1);

          scale_v_crease = compute_scale_factor(ve_median->v_crease, median->v_crease);
        }

        for (int i = 0; i < 2; i++) {
          if (median->skin[i]) {
            cd_vert_skin_offset = CustomData_get_offset(&bm->vdata, CD_MVERT_SKIN);
            BLI_assert(cd_vert_skin_offset != -1);

            if (ve_median->skin[i] != median->skin[i]) {
              scale_skin[i] = ve_median->skin[i] / (ve_median->skin[i] - median->skin[i]);
            }
          }
        }

        BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
          if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
            if (apply_vcos) {
              apply_raw_diff_v3(eve->co, tot, ve_median->location, median->location);
            }

            if (cd_vert_bweight_offset != -1) {
              float *b_weight = static_cast<float *>(
                  BM_ELEM_CD_GET_VOID_P(eve, cd_vert_bweight_offset));
              apply_scale_factor_clamp(b_weight, tot, ve_median->bv_weight, scale_bv_weight);
            }

            if (cd_vert_crease_offset != -1) {
              float *crease = static_cast<float *>(
                  BM_ELEM_CD_GET_VOID_P(eve, cd_vert_crease_offset));
              apply_scale_factor_clamp(crease, tot, ve_median->v_crease, scale_v_crease);
            }

            if (cd_vert_skin_offset != -1) {
              MVertSkin *vs = static_cast<MVertSkin *>(
                  BM_ELEM_CD_GET_VOID_P(eve, cd_vert_skin_offset));

              /* That one is not clamped to [0.0, 1.0]. */
              for (int i = 0; i < 2; i++) {
                if (median->skin[i] != 0.0f) {
                  apply_scale_factor(
                      &vs->radius[i], tot, ve_median->skin[i], median->skin[i], scale_skin[i]);
                }
              }
            }
          }
        }
      }

      if (apply_vcos) {
        /* Tell the update callback to run. */
        tfp->tag_for_update = true;
      }

      /* Edges */

      if (median->be_weight || median->e_crease) {
        if (median->be_weight) {
          if (!CustomData_has_layer_named(&bm->edata, CD_PROP_FLOAT, "bevel_weight_edge")) {
            BM_data_layer_add_named(bm, &bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
          }
          cd_edge_bweight_offset = CustomData_get_offset_named(
              &bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
          BLI_assert(cd_edge_bweight_offset != -1);

          scale_be_weight = compute_scale_factor(ve_median->be_weight, median->be_weight);
        }

        if (median->e_crease) {
          if (!CustomData_has_layer_named(&bm->edata, CD_PROP_FLOAT, "crease_edge")) {
            BM_data_layer_add_named(bm, &bm->edata, CD_PROP_FLOAT, "crease_edge");
          }
          cd_edge_crease_offset = CustomData_get_offset_named(
              &bm->edata, CD_PROP_FLOAT, "crease_edge");
          BLI_assert(cd_edge_crease_offset != -1);

          scale_e_crease = compute_scale_factor(ve_median->e_crease, median->e_crease);
        }

        BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
          if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
            if (median->be_weight != 0.0f) {
              float *b_weight = static_cast<float *>(
                  BM_ELEM_CD_GET_VOID_P(eed, cd_edge_bweight_offset));
              apply_scale_factor_clamp(b_weight, tot, ve_median->be_weight, scale_be_weight);
            }

            if (median->e_crease != 0.0f) {
              float *crease = static_cast<float *>(
                  BM_ELEM_CD_GET_VOID_P(eed, cd_edge_crease_offset));
              apply_scale_factor_clamp(crease, tot, ve_median->e_crease, scale_e_crease);
            }
          }
        }
      }
    }
    else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF) &&
             (apply_vcos || median_basis.curve.b_weight || median_basis.curve.weight ||
              median_basis.curve.radius || median_basis.curve.tilt))
    {
      const TransformMedian_Curve *median = &median_basis.curve,
                                  *ve_median = &ve_median_basis.curve;
      Curve *cu = static_cast<Curve *>(ob->data);
      BPoint *bp;
      BezTriple *bezt;
      int a;
      ListBase *nurbs = BKE_curve_editNurbs_get(cu);
      const float scale_w = compute_scale_factor(ve_median->weight, median->weight);

      LISTBASE_FOREACH (Nurb *, nu, nurbs) {
        if (nu->type == CU_BEZIER) {
          for (a = nu->pntsu, bezt = nu->bezt; a--; bezt++) {
            if (bezt->f2 & SELECT) {
              if (apply_vcos) {
                /* Here we always have to use the diff... :/
                 * Cannot avoid some glitches when going e.g. from 3 to 0.0001 (see #37327),
                 * unless we use doubles.
                 */
                add_v3_v3(bezt->vec[0], median->location);
                add_v3_v3(bezt->vec[1], median->location);
                add_v3_v3(bezt->vec[2], median->location);
              }
              if (median->weight) {
                apply_scale_factor_clamp(&bezt->weight, tot, ve_median->weight, scale_w);
              }
              if (median->radius) {
                apply_raw_diff(&bezt->radius, tot, ve_median->radius, median->radius);
              }
              if (median->tilt) {
                apply_raw_diff(&bezt->tilt, tot, ve_median->tilt, median->tilt);
              }
            }
            else if (apply_vcos) {
              /* Handles can only have their coordinates changed here. */
              if (bezt->f1 & SELECT) {
                apply_raw_diff_v3(bezt->vec[0], tot, ve_median->location, median->location);
              }
              if (bezt->f3 & SELECT) {
                apply_raw_diff_v3(bezt->vec[2], tot, ve_median->location, median->location);
              }
            }
          }
        }
        else {
          for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a--; bp++) {
            if (bp->f1 & SELECT) {
              if (apply_vcos) {
                apply_raw_diff_v3(bp->vec, tot, ve_median->location, median->location);
              }
              if (median->b_weight) {
                apply_raw_diff(&bp->vec[3], tot, ve_median->b_weight, median->b_weight);
              }
              if (median->weight) {
                apply_scale_factor_clamp(&bp->weight, tot, ve_median->weight, scale_w);
              }
              if (median->radius) {
                apply_raw_diff(&bp->radius, tot, ve_median->radius, median->radius);
              }
              if (median->tilt) {
                apply_raw_diff(&bp->tilt, tot, ve_median->tilt, median->tilt);
              }
            }
          }
        }
        if (CU_IS_2D(cu)) {
          BKE_nurb_project_2d(nu);
        }
        /* In the case of weight, tilt or radius (these don't change positions),
         * don't change handle types. */
        if ((nu->type == CU_BEZIER) && apply_vcos) {
          BKE_nurb_handles_test(nu, NURB_HANDLE_TEST_EACH, false); /* test for bezier too */
        }
      }
    }
    else if ((ob->type == OB_LATTICE) && (apply_vcos || median_basis.lattice.weight)) {
      const TransformMedian_Lattice *median = &median_basis.lattice,
                                    *ve_median = &ve_median_basis.lattice;
      Lattice *lt = static_cast<Lattice *>(ob->data);
      BPoint *bp;
      int a;
      const float scale_w = compute_scale_factor(ve_median->weight, median->weight);

      a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
      bp = lt->editlatt->latt->def;
      while (a--) {
        if (bp->f1 & SELECT) {
          if (apply_vcos) {
            apply_raw_diff_v3(bp->vec, tot, ve_median->location, median->location);
          }
          if (median->weight) {
            apply_scale_factor_clamp(&bp->weight, tot, ve_median->weight, scale_w);
          }
        }
        bp++;
      }
    }
    else if (ob->type == OB_GREASE_PENCIL &&
             (apply_vcos || median_basis.curves.nurbs_weight || median_basis.curves.radius ||
              median_basis.curves.tilt))
    {
      using namespace ed::greasepencil;
      using namespace ed::curves;
      Scene &scene = *CTX_data_scene(C);
      GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
      Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);

      threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
        bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
        if (apply_to_curves_point_selection(
                tot, median_basis.curves, ve_median_basis.curves, curves))
        {
          info.drawing.tag_positions_changed();
        }
      });
    }
    else if (ob->type == OB_CURVES && (apply_vcos || median_basis.curves.nurbs_weight ||
                                       median_basis.curves.radius || median_basis.curves.tilt))
    {
      using namespace ed::curves;
      Curves &curves_id = *static_cast<Curves *>(ob->data);
      bke::CurvesGeometry &curves = curves_id.geometry.wrap();
      if (apply_to_curves_point_selection(
              tot, median_basis.curves, ve_median_basis.curves, curves))
      {
        curves.tag_positions_changed();
      }
    }
  }

  // ED_undo_push(C, "Transform properties");
}

#undef TRANSFORM_MEDIAN_ARRAY_LEN

static void v3d_object_dimension_buts(bContext *C, uiLayout *layout, View3D *v3d, Object *ob)
{
  uiBlock *block = (layout) ? layout->block() : nullptr;
  uiLayout *sub_layout = layout ? &layout->absolute(false) : nullptr;
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);
  const bool is_editable = ID_IS_EDITABLE(&ob->id);

  if (block) {
    BLI_assert(C == nullptr);
    int yi = 200;
    const int butw = 200;
    const int buth = 20 * UI_SCALE_FAC;

    BKE_object_dimensions_eval_cached_get(ob, tfp->ob_dims);
    copy_v3_v3(tfp->ob_dims_orig, tfp->ob_dims);
    copy_v3_v3(tfp->ob_scale_orig, ob->scale);
    copy_m4_m4(tfp->ob_obmat_orig, ob->object_to_world().ptr());

    if (!is_editable && sub_layout) {
      sub_layout->enabled_set(false);
    }

    uiDefBut(block,
             ButType::Label,
             0,
             IFACE_("Dimensions:"),
             0,
             yi -= buth,
             butw,
             buth,
             nullptr,
             0,
             0,
             "");
    UI_block_align_begin(block);
    const float lim = FLT_MAX;
    for (int i = 0; i < 3; i++) {
      uiBut *but;
      const char text[3] = {char('X' + i), ':', '\0'};
      but = uiDefButF(block,
                      ButType::Num,
                      B_TRANSFORM_PANEL_DIMS,
                      text,
                      0,
                      yi -= buth,
                      butw,
                      buth,
                      &(tfp->ob_dims[i]),
                      0.0f,
                      lim,
                      "");
      UI_but_number_step_size_set(but, 10);
      UI_but_number_precision_set(but, 3);
      UI_but_unit_type_set(but, PROP_UNIT_LENGTH);
      if (!is_editable) {
        UI_but_disable(but, "Cannot edit this property from a linked data-block");
      }
    }
    UI_block_align_end(block);
  }
  else { /* apply */
    int axis_mask = 0;
    for (int i = 0; i < 3; i++) {
      if (tfp->ob_dims[i] == tfp->ob_dims_orig[i]) {
        axis_mask |= (1 << i);
      }
    }
    BKE_object_dimensions_set_ex(
        ob, tfp->ob_dims, axis_mask, tfp->ob_scale_orig, tfp->ob_obmat_orig);

    PointerRNA obptr = RNA_id_pointer_create(&ob->id);
    PropertyRNA *prop = RNA_struct_find_property(&obptr, "scale");
    RNA_property_update(C, &obptr, prop);
  }
}

#define B_VGRP_PNL_EDIT_SINGLE 8 /* or greater */

static void do_view3d_vgroup_buttons(bContext *C, void * /*arg*/, int event)
{
  if (event < B_VGRP_PNL_EDIT_SINGLE) {
    /* not for me */
    return;
  }

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  blender::ed::object::vgroup_vert_active_mirror(ob, event - B_VGRP_PNL_EDIT_SINGLE);
  DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
}

static bool view3d_panel_vgroup_poll(const bContext *C, PanelType * /*pt*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  if (ob && (BKE_object_is_in_editmode_vgroup(ob) || BKE_object_is_in_wpaint_select_vert(ob))) {
    MDeformVert *dvert_act = ED_mesh_active_dvert_get_only(ob);
    if (dvert_act) {
      return (dvert_act->totweight != 0);
    }
  }

  return false;
}

static void update_active_vertex_weight(bContext *C, void *arg1, void * /*arg2*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  MDeformVert *dv = ED_mesh_active_dvert_get_only(ob);
  const int vertex_group_index = POINTER_AS_INT(arg1);
  MDeformWeight *dw = BKE_defvert_find_index(dv, vertex_group_index);
  dw->weight = tfp->vertex_weights[vertex_group_index];
}

static void view3d_panel_vgroup(const bContext *C, Panel *panel)
{
  uiBlock *block = panel->layout->absolute_block();
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  View3D *v3d = CTX_wm_view3d(C);
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);

  MDeformVert *dv;

  dv = ED_mesh_active_dvert_get_only(ob);

  if (dv && dv->totweight) {
    ToolSettings *ts = scene->toolsettings;

    wmOperatorType *ot;
    PointerRNA op_ptr;
    PointerRNA *but_ptr;

    uiLayout *col, *bcol;
    uiLayout *row;
    uiBut *but;
    bDeformGroup *dg;
    uint i;
    int subset_count, vgroup_tot;
    const bool *vgroup_validmap;
    eVGroupSelect subset_type = eVGroupSelect(ts->vgroupsubset);
    int yco = 0;
    int lock_count = 0;

    UI_block_func_handle_set(block, do_view3d_vgroup_buttons, nullptr);

    bcol = &panel->layout->column(true);
    row = &bcol->row(true); /* The filter button row */

    PointerRNA tools_ptr = RNA_pointer_create_discrete(nullptr, &RNA_ToolSettings, ts);
    row->prop(&tools_ptr, "vertex_group_subset", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

    col = &bcol->column(true);

    vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
        ob, subset_type, &vgroup_tot, &subset_count);
    const ListBase *defbase = BKE_object_defgroup_list(ob);
    const int vgroup_num = BLI_listbase_count(defbase);
    tfp->vertex_weights.resize(vgroup_num);

    for (i = 0, dg = static_cast<bDeformGroup *>(defbase->first); dg; i++, dg = dg->next) {
      bool locked = (dg->flag & DG_LOCK_WEIGHT) != 0;
      if (vgroup_validmap[i]) {
        MDeformWeight *dw = BKE_defvert_find_index(dv, i);
        if (dw) {
          int x, xco = 0;
          int icon;
          uiLayout *split = &col->split(0.45, true);
          row = &split->row(true);

          /* The Weight Group Name */

          ot = WM_operatortype_find("OBJECT_OT_vertex_weight_set_active", true);
          but = uiDefButO_ptr(block,
                              ButType::But,
                              ot,
                              blender::wm::OpCallContext::ExecDefault,
                              dg->name,
                              xco,
                              yco,
                              (x = UI_UNIT_X * 5),
                              UI_UNIT_Y,
                              "");
          but_ptr = UI_but_operator_ptr_ensure(but);
          RNA_int_set(but_ptr, "weight_group", i);
          UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
          if (BKE_object_defgroup_active_index_get(ob) != i + 1) {
            UI_but_flag_enable(but, UI_BUT_INACTIVE);
          }
          xco += x;

          row = &split->row(true);
          row->enabled_set(!locked);

          /* The weight group value */
          /* To be reworked still */
          float &vertex_weight = tfp->vertex_weights[i];
          vertex_weight = dw->weight;
          but = uiDefButF(block,
                          ButType::Num,
                          B_VGRP_PNL_EDIT_SINGLE + i,
                          "",
                          xco,
                          yco,
                          (x = UI_UNIT_X * 4),
                          UI_UNIT_Y,
                          &vertex_weight,
                          0.0,
                          1.0,
                          "");
          UI_but_number_step_size_set(but, 1);
          UI_but_number_precision_set(but, 3);
          UI_but_drawflag_enable(but, UI_BUT_TEXT_LEFT);
          UI_but_func_set(but, update_active_vertex_weight, POINTER_FROM_INT(i), nullptr);
          if (locked) {
            lock_count++;
          }
          xco += x;

          /* The weight group paste function */
          icon = (locked) ? ICON_BLANK1 : ICON_PASTEDOWN;
          op_ptr = row->op("OBJECT_OT_vertex_weight_paste",
                           "",
                           icon,
                           blender::wm::OpCallContext::InvokeDefault,
                           UI_ITEM_NONE);
          RNA_int_set(&op_ptr, "weight_group", i);

          /* The weight entry delete function */
          icon = (locked) ? ICON_LOCKED : ICON_X;
          op_ptr = row->op("OBJECT_OT_vertex_weight_delete",
                           "",
                           icon,
                           blender::wm::OpCallContext::InvokeDefault,
                           UI_ITEM_NONE);
          RNA_int_set(&op_ptr, "weight_group", i);

          yco -= UI_UNIT_Y;
        }
      }
    }
    MEM_freeN(vgroup_validmap);

    yco -= 2;

    col = &panel->layout->column(true);
    row = &col->row(true);

    ot = WM_operatortype_find("OBJECT_OT_vertex_weight_normalize_active_vertex", true);
    but = uiDefButO_ptr(
        block,
        ButType::But,
        ot,
        blender::wm::OpCallContext::ExecDefault,
        IFACE_("Normalize"),
        0,
        yco,
        UI_UNIT_X * 5,
        UI_UNIT_Y,
        TIP_("Normalize weights of active vertex (if affected groups are unlocked)"));

    ot = WM_operatortype_find("OBJECT_OT_vertex_weight_copy", true);
    but = uiDefButO_ptr(
        block,
        ButType::But,
        ot,
        blender::wm::OpCallContext::ExecDefault,
        IFACE_("Copy"),
        UI_UNIT_X * 5,
        yco,
        UI_UNIT_X * 5,
        UI_UNIT_Y,
        TIP_("Copy active vertex to other selected vertices (if affected groups are unlocked)"));
    if (lock_count) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }
  }
}

static void v3d_transform_butsR(uiLayout *layout, PointerRNA *ptr)
{
  uiLayout *split, *colsub;

  split = &layout->split(0.8f, false);

  if (ptr->type == &RNA_PoseBone) {
    PointerRNA boneptr;
    Bone *bone;

    boneptr = RNA_pointer_get(ptr, "bone");
    bone = static_cast<Bone *>(boneptr.data);
    split->active_set(!(bone->parent && bone->flag & BONE_CONNECTED));
  }
  colsub = &split->column(true);
  colsub->prop(ptr, "location", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  colsub = &split->column(true);
  colsub->emboss_set(blender::ui::EmbossType::NoneOrStatus);
  colsub->label("", ICON_NONE);
  colsub->prop(
      ptr, "lock_location", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "", ICON_DECORATE_UNLOCKED);

  split = &layout->split(0.8f, false);

  switch (RNA_enum_get(ptr, "rotation_mode")) {
    case ROT_MODE_QUAT: /* quaternion */
      colsub = &split->column(true);
      colsub->prop(ptr, "rotation_quaternion", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
      colsub = &split->column(true);
      colsub->emboss_set(blender::ui::EmbossType::NoneOrStatus);
      colsub->prop(ptr, "lock_rotations_4d", UI_ITEM_R_TOGGLE, IFACE_("4L"), ICON_NONE);
      if (RNA_boolean_get(ptr, "lock_rotations_4d")) {
        colsub->prop(ptr,
                     "lock_rotation_w",
                     UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
                     "",
                     ICON_DECORATE_UNLOCKED);
      }
      else {
        colsub->label("", ICON_NONE);
      }
      colsub->prop(ptr,
                   "lock_rotation",
                   UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
                   "",
                   ICON_DECORATE_UNLOCKED);
      break;
    case ROT_MODE_AXISANGLE: /* axis angle */
      colsub = &split->column(true);
      colsub->prop(ptr, "rotation_axis_angle", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
      colsub = &split->column(true);
      colsub->emboss_set(blender::ui::EmbossType::NoneOrStatus);
      colsub->prop(ptr, "lock_rotations_4d", UI_ITEM_R_TOGGLE, IFACE_("4L"), ICON_NONE);
      if (RNA_boolean_get(ptr, "lock_rotations_4d")) {
        colsub->prop(ptr,
                     "lock_rotation_w",
                     UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
                     "",
                     ICON_DECORATE_UNLOCKED);
      }
      else {
        colsub->label("", ICON_NONE);
      }
      colsub->prop(ptr,
                   "lock_rotation",
                   UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
                   "",
                   ICON_DECORATE_UNLOCKED);
      break;
    default: /* euler rotations */
      colsub = &split->column(true);
      colsub->prop(ptr, "rotation_euler", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
      colsub = &split->column(true);
      colsub->emboss_set(blender::ui::EmbossType::NoneOrStatus);
      colsub->label("", ICON_NONE);
      colsub->prop(ptr,
                   "lock_rotation",
                   UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
                   "",
                   ICON_DECORATE_UNLOCKED);
      break;
  }
  layout->prop(ptr, "rotation_mode", UI_ITEM_NONE, "", ICON_NONE);

  split = &layout->split(0.8f, false);
  colsub = &split->column(true);
  colsub->prop(ptr, "scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  colsub = &split->column(true);
  colsub->emboss_set(blender::ui::EmbossType::NoneOrStatus);
  colsub->label("", ICON_NONE);
  colsub->prop(
      ptr, "lock_scale", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "", ICON_DECORATE_UNLOCKED);
}

static void v3d_posearmature_buts(uiLayout *layout, Object *ob)
{
  bPoseChannel *pchan;
  uiLayout *col;

  pchan = BKE_pose_channel_active_if_bonecoll_visible(ob);

  if (!pchan) {
    layout->label(IFACE_("No Bone Active"), ICON_NONE);
    return;
  }

  PointerRNA pchanptr = RNA_pointer_create_discrete(&ob->id, &RNA_PoseBone, pchan);

  col = &layout->column(false);

  /* XXX: RNA buts show data in native types (i.e. quaternion, 4-component axis/angle, etc.)
   * but old-school UI shows in eulers always. Do we want to be able to still display in Eulers?
   * Maybe needs RNA/UI options to display rotations as different types. */
  v3d_transform_butsR(col, &pchanptr);
}

static void v3d_editarmature_buts(uiLayout *layout, Object *ob)
{
  bArmature *arm = static_cast<bArmature *>(ob->data);
  EditBone *ebone;
  uiLayout *col;

  ebone = arm->act_edbone;

  if (!ebone || !ANIM_bonecoll_is_visible_editbone(arm, ebone)) {
    layout->label(IFACE_("Nothing selected"), ICON_NONE);
    return;
  }

  PointerRNA eboneptr = RNA_pointer_create_discrete(&arm->id, &RNA_EditBone, ebone);

  col = &layout->column(false);
  col->prop(&eboneptr, "head", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (ebone->parent && ebone->flag & BONE_CONNECTED) {
    PointerRNA parptr = RNA_pointer_get(&eboneptr, "parent");
    col->prop(&parptr, "tail_radius", UI_ITEM_NONE, IFACE_("Radius (Parent)"), ICON_NONE);
  }
  else {
    col->prop(&eboneptr, "head_radius", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);
  }

  col->prop(&eboneptr, "tail", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&eboneptr, "tail_radius", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);

  col->prop(&eboneptr, "roll", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&eboneptr, "length", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&eboneptr, "envelope_distance", UI_ITEM_NONE, IFACE_("Envelope"), ICON_NONE);
}

static void v3d_editmetaball_buts(uiLayout *layout, Object *ob)
{
  MetaBall *mball = static_cast<MetaBall *>(ob->data);
  uiLayout *col;

  if (!mball || !(mball->lastelem)) {
    layout->label(IFACE_("Nothing selected"), ICON_NONE);
    return;
  }

  PointerRNA ptr = RNA_pointer_create_discrete(&mball->id, &RNA_MetaElement, mball->lastelem);

  col = &layout->column(false);
  col->prop(&ptr, "co", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col->prop(&ptr, "radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&ptr, "stiffness", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col->prop(&ptr, "type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col = &layout->column(true);
  switch (RNA_enum_get(&ptr, "type")) {
    case MB_BALL:
      break;
    case MB_CUBE:
      col->label(IFACE_("Size:"), ICON_NONE);
      col->prop(&ptr, "size_x", UI_ITEM_NONE, IFACE_("X"), ICON_NONE);
      col->prop(&ptr, "size_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
      col->prop(&ptr, "size_z", UI_ITEM_NONE, IFACE_("Z"), ICON_NONE);
      break;
    case MB_TUBE:
      col->label(IFACE_("Size:"), ICON_NONE);
      col->prop(&ptr, "size_x", UI_ITEM_NONE, IFACE_("X"), ICON_NONE);
      break;
    case MB_PLANE:
      col->label(IFACE_("Size:"), ICON_NONE);
      col->prop(&ptr, "size_x", UI_ITEM_NONE, IFACE_("X"), ICON_NONE);
      col->prop(&ptr, "size_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
      break;
    case MB_ELIPSOID:
      col->label(IFACE_("Size:"), ICON_NONE);
      col->prop(&ptr, "size_x", UI_ITEM_NONE, IFACE_("X"), ICON_NONE);
      col->prop(&ptr, "size_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
      col->prop(&ptr, "size_z", UI_ITEM_NONE, IFACE_("Z"), ICON_NONE);
      break;
  }
}

static void do_view3d_region_buttons(bContext *C, void * /*index*/, int event)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  switch (event) {

    case B_REDR:
      ED_area_tag_redraw(CTX_wm_area(C));
      return; /* no notifier! */

    case B_TRANSFORM_PANEL_MEDIAN:
      if (ob) {
        v3d_editvertex_buts(C, nullptr, v3d, ob, 1.0);
        DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_GEOMETRY);
      }
      break;
    case B_TRANSFORM_PANEL_DIMS:
      if (ob) {
        v3d_object_dimension_buts(C, nullptr, v3d, ob);
      }
      break;
  }

  /* default for now */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
}

static bool view3d_panel_transform_poll(const bContext *C, PanelType * /*pt*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  return (BKE_view_layer_active_base_get(view_layer) != nullptr);
}

static void view3d_panel_transform(const bContext *C, Panel *panel)
{
  uiBlock *block;
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Object *obedit = OBEDIT_FROM_OBACT(ob);
  uiLayout *col;

  block = panel->layout->block();
  UI_block_func_handle_set(block, do_view3d_region_buttons, nullptr);

  col = &panel->layout->column(false);

  if (ob == obedit) {
    if (ob->type == OB_ARMATURE) {
      v3d_editarmature_buts(col, ob);
    }
    else if (ob->type == OB_MBALL) {
      v3d_editmetaball_buts(col, ob);
    }
    else {
      View3D *v3d = CTX_wm_view3d(C);
      v3d_editvertex_buts(C, col, v3d, ob, FLT_MAX);
    }
  }
  else if (ob->mode & OB_MODE_POSE) {
    v3d_posearmature_buts(col, ob);
  }
  else {
    PointerRNA obptr = RNA_id_pointer_create(&ob->id);
    v3d_transform_butsR(col, &obptr);

    /* Dimensions and editmode are mostly the same check. */
    if (OB_TYPE_SUPPORT_EDITMODE(ob->type) || ELEM(ob->type, OB_VOLUME, OB_CURVES, OB_POINTCLOUD))
    {
      View3D *v3d = CTX_wm_view3d(C);
      v3d_object_dimension_buts(nullptr, col, v3d, ob);
    }
  }
}

static bool view3d_panel_curve_data_poll(const bContext *C, PanelType * /*pt*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  return (ob && ELEM(ob->type, OB_GREASE_PENCIL, OB_CURVES) && BKE_object_is_in_editmode(ob));
}

static void apply_to_active_object(
    bContext *C,
    blender::FunctionRef<void(const CurvesDataPanelState &modified_state,
                              const blender::IndexMask &selection,
                              blender::bke::CurvesGeometry &curves)> curves_geometry_handler)
{
  using namespace blender;

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  View3D *v3d = CTX_wm_view3d(C);
  const TransformProperties &tfp = *v3d_transform_props_ensure(v3d);
  const CurvesDataPanelState &modified = tfp.modified;

  if (ob->type == OB_GREASE_PENCIL) {
    using namespace ed::greasepencil;
    Scene &scene = *CTX_data_scene(C);
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
    Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);

    threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
      bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
      IndexMaskMemory memory;
      const IndexMask selection = ed::curves::retrieve_selected_curves(curves, memory);
      if (selection.is_empty()) {
        return;
      }

      curves_geometry_handler(modified, selection, curves);
      info.drawing.tag_topology_changed();
    });
  }
  else {
    Curves &curves_id = *static_cast<Curves *>(ob->data);
    bke::CurvesGeometry &curves = curves_id.geometry.wrap();
    IndexMaskMemory memory;
    const IndexMask selection = ed::curves::retrieve_selected_curves(curves, memory);

    if (!selection.is_empty()) {
      curves_geometry_handler(modified, selection, curves);
      curves.tag_topology_changed();
    }
  }

  DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
}

static void handle_curves_cyclic(bContext *C, void *, void *)
{
  using namespace blender;

  apply_to_active_object(C,
                         [](const CurvesDataPanelState &modified_state,
                            const IndexMask &selection,
                            bke::CurvesGeometry &curves) {
                           index_mask::masked_fill(
                               curves.cyclic_for_write(), bool(modified_state.cyclic), selection);
                         });
}

static void update_custom_knots(const blender::OffsetIndices<int> &src_custom_knots_by_curve,
                                const blender::Span<int8_t> src_knot_modes,
                                const blender::Span<float> src_custom_knots,
                                blender::bke::CurvesGeometry &curves)
{
  using namespace blender;
  curves.nurbs_custom_knots_update_size();
  IndexMaskMemory memory;
  const IndexMask custom_knot_curves = curves.nurbs_custom_knot_curves(memory);
  if (!custom_knot_curves.is_empty()) {
    const OffsetIndices points_by_curve = curves.points_by_curve();
    const OffsetIndices<int> custom_knots_by_curve = curves.nurbs_custom_knots_by_curve();
    const VArray<int8_t> orders = curves.nurbs_orders();
    const VArray<bool> cyclic = curves.cyclic();
    MutableSpan<float> custom_knots = curves.nurbs_custom_knots_for_write();

    custom_knot_curves.foreach_index(GrainSize(512), [&](const int curve) {
      const IndexRange dst_knots = custom_knots_by_curve[curve];
      const IndexRange src_knots = src_custom_knots_by_curve[curve];
      if (src_knots.is_empty()) {
        const int points_num = points_by_curve[curve].size();
        const int order = orders[curve];
        const bool is_cyclic = cyclic[curve];
        Array<float> knots_buffer(bke::curves::nurbs::knots_num(points_num, order, is_cyclic));
        bke::curves::nurbs::calculate_knots(
            points_num, KnotsMode(src_knot_modes[curve]), order, is_cyclic, knots_buffer);
        custom_knots.slice(dst_knots).copy_from(
            knots_buffer.as_span().take_front(dst_knots.size()));
      }
      else {
        custom_knots.slice(dst_knots).copy_from(src_custom_knots.slice(src_knots));
      }
    });
  }
}

static void handle_curves_knot_mode(bContext *C, void *, void *)
{
  using namespace blender;

  apply_to_active_object(
      C,
      [](const CurvesDataPanelState &modified_state,
         const IndexMask &selection,
         bke::CurvesGeometry &curves) {
        const OffsetIndices<int> src_custom_knots_by_curve = curves.nurbs_custom_knots_by_curve();
        /* Ensure `src_custom_knots_by_curve` will not get deleted. */
        const SharedCache<Vector<int>> custom_knot_offsets_cache =
            curves.runtime->custom_knot_offsets_cache;
        const Span<float> src_custom_knots = curves.nurbs_custom_knots();
        const ImplicitSharingInfo *knots_sharing_info = curves.runtime->custom_knots_sharing_info;
        if (knots_sharing_info != nullptr) {
          knots_sharing_info->add_weak_user();
        }

        Array<int8_t> src_knot_modes;
        if (!src_custom_knots.is_empty() ||
            modified_state.nurbs_knot_mode == NURBS_KNOT_MODE_CUSTOM) {
          src_knot_modes.reinitialize(curves.curves_num());
          curves.nurbs_knots_modes().materialize(src_knot_modes);
        }

        const MutableSpan<int8_t> nurbs_knot_modes = curves.nurbs_knots_modes_for_write();

        index_mask::masked_fill(
            nurbs_knot_modes, int8_t(modified_state.nurbs_knot_mode), selection);
        /**
         * Copies custom knots from the original array for curves which retain
         * NURBS_KNOT_MODE_CUSTOM. Calculates custom knots for curves which gain
         * NURBS_KNOT_MODE_CUSTOM.
         */
        update_custom_knots(src_custom_knots_by_curve, src_knot_modes, src_custom_knots, curves);

        if (knots_sharing_info != nullptr) {
          knots_sharing_info->remove_weak_user_and_delete_if_last();
        }
      });
}

static void handle_curves_order(bContext *C, void *, void *)
{
  using namespace blender;

  apply_to_active_object(
      C,
      [](const CurvesDataPanelState &modified_state,
         const IndexMask &selection,
         bke::CurvesGeometry &curves) {
        const MutableSpan<int8_t> nurbs_knot_modes = curves.nurbs_knots_modes_for_write();
        const MutableSpan<int8_t> orders = curves.nurbs_orders_for_write();

        const OffsetIndices<int> src_custom_knots_by_curve = curves.nurbs_custom_knots_by_curve();
        /* Ensure `src_custom_knots_by_curve` will not get deleted. */
        const SharedCache<Vector<int>> custom_knot_offsets_cache =
            curves.runtime->custom_knot_offsets_cache;
        const Span<float> src_custom_knots = curves.nurbs_custom_knots();
        const ImplicitSharingInfo *knots_sharing_info = curves.runtime->custom_knots_sharing_info;
        if (knots_sharing_info != nullptr) {
          knots_sharing_info->add_weak_user();
        }

        bool knot_modes_changed = false;

        selection.foreach_index(GrainSize(512), [&](const int curve) {
          if (orders[curve] != modified_state.order &&
              nurbs_knot_modes[curve] == NURBS_KNOT_MODE_CUSTOM)
          {
            nurbs_knot_modes[curve] = NURBS_KNOT_MODE_NORMAL;
            knot_modes_changed = true;
          }
          orders[curve] = modified_state.order;
        });

        /**
         * Custom knots need to be recopied, if some curves loose NURBS_KNOT_MODE_CUSTOM.
         */
        if (knot_modes_changed) {
          update_custom_knots(src_custom_knots_by_curve, {}, src_custom_knots, curves);
        }

        if (knots_sharing_info != nullptr) {
          knots_sharing_info->remove_weak_user_and_delete_if_last();
        }
      });
}

static void handle_curves_resolution(bContext *C, void *, void *)
{
  using namespace blender;

  apply_to_active_object(C,
                         [](const CurvesDataPanelState &modified_state,
                            const IndexMask &selection,
                            bke::CurvesGeometry &curves) {
                           index_mask::masked_fill(curves.resolution_for_write(),
                                                   modified_state.resolution,
                                                   selection);
                         });
}

constexpr std::array<EnumPropertyItem, 5> enum_curve_knot_mode_items{{
    {NURBS_KNOT_MODE_NORMAL, "NORMAL", ICON_NONE, "Normal", ""},
    {NURBS_KNOT_MODE_ENDPOINT, "ENDPOINT", ICON_NONE, "Endpoint", ""},
    {NURBS_KNOT_MODE_BEZIER, "BEZIER", ICON_NONE, "Bezier", ""},
    {NURBS_KNOT_MODE_ENDPOINT_BEZIER, "ENDPOINT_BEZIER", ICON_NONE, "Endpoint Bezier", ""},
    {NURBS_KNOT_MODE_CUSTOM, "CUSTOM", ICON_NONE, "Custom", ""},
}};

static void knot_modes_menu(bContext * /*C*/, uiLayout *layout, void *knot_mode_p)
{
  uiBlock *block = layout->block();
  blender::ui::block_layout_set_current(block, layout);
  layout->column(false);

  for (const EnumPropertyItem &item : enum_curve_knot_mode_items) {
    uiDefButI(block,
              ButType::ButMenu,
              0,
              IFACE_(item.name),
              0,
              0,
              UI_UNIT_X * 5,
              UI_UNIT_Y,
              reinterpret_cast<int *>(knot_mode_p),
              item.value,
              0.0,
              "");
  }
}

static void view3d_panel_curve_data(const bContext *C, Panel *panel)
{
  using namespace blender;
  using namespace ed::curves;

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  uiBlock *block = panel->layout->block();

  CurvesSelectionStatus status;

  if (ob->type == OB_GREASE_PENCIL) {
    using namespace ed::greasepencil;
    Scene &scene = *CTX_data_scene(C);
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
    Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);

    status = threading::parallel_reduce(
        drawings.index_range(),
        1L,
        CurvesSelectionStatus(),
        [&](const IndexRange range, const CurvesSelectionStatus &acc) {
          CurvesSelectionStatus value = acc;
          for (const int drawing : range) {
            value = CurvesSelectionStatus::sum(
                value, init_curves_selection_status(drawings[drawing].drawing.strokes()));
          }
          return value;
        },
        CurvesSelectionStatus::sum);
  }
  else {
    const Curves &curves_id = *static_cast<Curves *>(ob->data);
    status = init_curves_selection_status(curves_id.geometry.wrap());
  }

  if (status.curve_count == 0) {
    uiDefBut(
        block, ButType::Label, 0, IFACE_("Nothing selected"), 0, 130, 200, 20, nullptr, 0, 0, "");
    return;
  }

  View3D *v3d = CTX_wm_view3d(C);
  TransformProperties &tfp = *v3d_transform_props_ensure(v3d);
  CurvesDataPanelState &modified = tfp.modified;
  CurvesDataPanelState &current = tfp.current;

  current.cyclic = status.cyclic_count > 0;
  current.nurbs_knot_mode = KnotsMode(
      math::safe_divide(status.nurbs_knot_mode_sum, status.nurbs_count));
  current.order = math::safe_divide(status.order_sum, status.nurbs_count);
  current.resolution = math::safe_divide(status.resolution_sum, status.curve_count);

  modified = current;

  panel->layout->use_property_split_set(true);
  uiLayout &bcol = panel->layout->column(false);

  auto add_labeled_field =
      [&](const StringRef label, const bool active, FunctionRef<uiBut *()> add_button) {
        uiLayout &row = bcol.row(true);
        uiLayout &split = row.split(0.4, true);
        uiLayout &col = split.column(true);
        col.alignment_set(ui::LayoutAlign::Right);
        col.label(label, ICON_NONE);
        split.column(false);
        uiBut *but = add_button();
        if (active) {
          UI_but_drawflag_disable(but, UI_BUT_INDETERMINATE);
        }
        else {
          UI_but_drawflag_enable(but, UI_BUT_INDETERMINATE);
        }
      };

  const int butw = 10 * UI_UNIT_X;
  const int buth = 20 * UI_SCALE_FAC;

  add_labeled_field(
      IFACE_("Cyclic"),
      status.cyclic_count == 0 || status.cyclic_count == status.curve_count,
      [&]() {
        uiBut *but = uiDefButC(
            block, ButType::Checkbox, 0, "", 0, 0, butw, buth, &modified.cyclic, 0, 1, "");
        UI_but_func_set(but, handle_curves_cyclic, nullptr, nullptr);
        return but;
      });

  if (status.nurbs_count == status.curve_count) {
    add_labeled_field(
        IFACE_("Knot Mode"),
        status.nurbs_knot_mode_max * status.nurbs_count == status.nurbs_knot_mode_sum,
        [&]() {
          uiBut *but = uiDefMenuBut(block,
                                    knot_modes_menu,
                                    &modified.nurbs_knot_mode,
                                    enum_curve_knot_mode_items[modified.nurbs_knot_mode].name,
                                    0,
                                    0,
                                    butw,
                                    buth,
                                    "");
          UI_but_type_set_menu_from_pulldown(but);
          UI_but_func_set(but, handle_curves_knot_mode, nullptr, nullptr);
          return but;
        });

    add_labeled_field(
        IFACE_("Order"), status.order_max * status.nurbs_count == status.order_sum, [&]() {
          uiBut *but = uiDefButI(
              block, ButType::Num, 0, "", 0, 0, butw, buth, &modified.order, 2, 6, "");
          UI_but_number_step_size_set(but, 1);
          UI_but_number_precision_set(but, -1);
          UI_but_func_set(but, handle_curves_order, nullptr, nullptr);
          return but;
        });
  }

  if (status.poly_count == 0) {
    add_labeled_field(
        IFACE_("Resolution"),
        status.resolution_max * status.curve_count == status.resolution_sum,
        [&]() {
          uiBut *but = uiDefButI(
              block, ButType::Num, 0, "", 0, 0, butw, buth, &modified.resolution, 1, 64, "");
          UI_but_number_step_size_set(but, 1);
          UI_but_number_precision_set(but, -1);
          UI_but_func_set(but, handle_curves_resolution, nullptr, nullptr);
          return but;
        });
  }
}

void view3d_buttons_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN<PanelType>("spacetype view3d panel object");
  STRNCPY_UTF8(pt->idname, "VIEW3D_PT_transform");
  STRNCPY_UTF8(pt->label, N_("Transform")); /* XXX C panels unavailable through RNA bpy.types! */
  STRNCPY_UTF8(pt->category, "Item");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = view3d_panel_transform;
  pt->poll = view3d_panel_transform_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN<PanelType>("spacetype view3d panel vgroup");
  STRNCPY_UTF8(pt->idname, "VIEW3D_PT_vgroup");
  STRNCPY_UTF8(pt->label,
               N_("Vertex Weights")); /* XXX C panels unavailable through RNA bpy.types! */
  STRNCPY_UTF8(pt->category, "Item");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = view3d_panel_vgroup;
  pt->poll = view3d_panel_vgroup_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN<PanelType>("spacetype view3d panel curves");
  STRNCPY_UTF8(pt->idname, "VIEW3D_PT_curves");
  STRNCPY_UTF8(pt->label, N_("Curve Data")); /* XXX C panels unavailable through RNA bpy.types! */
  STRNCPY_UTF8(pt->category, "Item");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = view3d_panel_curve_data;
  pt->poll = view3d_panel_curve_data_poll;
  BLI_addtail(&art->paneltypes, pt);
}

static wmOperatorStatus view3d_object_mode_menu_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "No active object found");
    return OPERATOR_CANCELLED;
  }
  if (((ob->mode & OB_MODE_EDIT) == 0) && ELEM(ob->type, OB_ARMATURE)) {
    blender::ed::object::mode_set(C, (ob->mode == OB_MODE_OBJECT) ? OB_MODE_POSE : OB_MODE_OBJECT);
    return OPERATOR_CANCELLED;
  }

  UI_pie_menu_invoke(C, "VIEW3D_MT_object_mode_pie", CTX_wm_window(C)->eventstate);
  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_object_mode_pie_or_toggle(wmOperatorType *ot)
{
  ot->name = "Object Mode Menu";
  ot->idname = "VIEW3D_OT_object_mode_pie_or_toggle";

  ot->exec = view3d_object_mode_menu_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}
