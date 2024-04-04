/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <optional>

#include <fmt/format.h>

#include "BKE_action.h"
#include "BKE_anim_data.hh"
#include "BKE_attribute.hh"
#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_curves.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_grease_pencil.hh"
#include "BKE_grease_pencil_legacy_convert.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_object.hh"

#include "BLO_readfile.hh"

#include "BLI_color.hh"
#include "BLI_function_ref.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DNA_anim_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

namespace blender::bke::greasepencil::convert {

/* -------------------------------------------------------------------- */
/** \name Animation conversion helpers.
 *
 * These utilities will call given callback over all relevant F-curves
 * (also includes drivers, and actions linked through the NLA).
 * \{ */

using FCurveCallback = bool(bAction *owner_action, FCurve &fcurve);

/* Utils to move a list of fcurves from one container (Action or Drivers) to another. */
static void legacy_fcurves_move(ListBase &fcurves_dst,
                                ListBase &fcurves_src,
                                const Span<FCurve *> fcurves)
{
  for (FCurve *fcurve : fcurves) {
    BLI_assert(BLI_findindex(&fcurves_src, fcurve) >= 0);
    BLI_remlink(&fcurves_src, fcurve);
    BLI_addtail(&fcurves_dst, fcurve);
  }
}

/* Basic common check to decide whether a legacy fcurve should be processed or not. */
static bool legacy_fcurves_is_valid_for_root_path(FCurve &fcurve, StringRefNull legacy_root_path)
{
  if (!fcurve.rna_path) {
    return false;
  }
  StringRefNull rna_path = fcurve.rna_path;
  if (!rna_path.startswith(legacy_root_path)) {
    return false;
  }
  return true;
}

static bool legacy_fcurves_process(bAction *owner_action,
                                   ListBase &fcurves,
                                   blender::FunctionRef<FCurveCallback> callback)
{
  bool is_changed = false;
  LISTBASE_FOREACH (FCurve *, fcurve, &fcurves) {
    const bool local_is_changed = callback(owner_action, *fcurve);
    is_changed = is_changed || local_is_changed;
  }
  return is_changed;
}

static bool legacy_nla_strip_process(NlaStrip &nla_strip,
                                     blender::FunctionRef<FCurveCallback> callback)
{
  bool is_changed = false;
  if (nla_strip.act) {
    if (legacy_fcurves_process(nla_strip.act, nla_strip.act->curves, callback)) {
      DEG_id_tag_update(&nla_strip.act->id, ID_RECALC_ANIMATION);
      is_changed = true;
    }
  }
  LISTBASE_FOREACH (NlaStrip *, nla_strip_children, &nla_strip.strips) {
    const bool local_is_changed = legacy_nla_strip_process(*nla_strip_children, callback);
    is_changed = is_changed || local_is_changed;
  }
  return is_changed;
}

static bool legacy_animation_process(AnimData &anim_data,
                                     blender::FunctionRef<FCurveCallback> callback)
{
  bool is_changed = false;
  if (anim_data.action) {
    if (legacy_fcurves_process(anim_data.action, anim_data.action->curves, callback)) {
      DEG_id_tag_update(&anim_data.action->id, ID_RECALC_ANIMATION);
      is_changed = true;
    }
  }
  if (anim_data.tmpact) {
    if (legacy_fcurves_process(anim_data.tmpact, anim_data.tmpact->curves, callback)) {
      DEG_id_tag_update(&anim_data.tmpact->id, ID_RECALC_ANIMATION);
      is_changed = true;
    }
  }

  {
    const bool local_is_changed = legacy_fcurves_process(nullptr, anim_data.drivers, callback);
    is_changed = is_changed || local_is_changed;
  }

  LISTBASE_FOREACH (NlaTrack *, nla_track, &anim_data.nla_tracks) {
    LISTBASE_FOREACH (NlaStrip *, nla_strip, &nla_track->strips) {
      const bool local_is_changed = legacy_nla_strip_process(*nla_strip, callback);
      is_changed = is_changed || local_is_changed;
    }
  }
  return is_changed;
}

/* \} */

/**
 * Find vertex groups that have assigned vertices in this drawing.
 * Returns:
 * - ListBase with used vertex group names (bDeformGroup)
 * - Array of indices in the new vertex group list for remapping
 */
static void find_used_vertex_groups(const bGPDframe &gpf,
                                    const ListBase &all_names,
                                    ListBase &r_vertex_group_names,
                                    Array<int> &r_indices)
{
  const int num_vertex_groups = BLI_listbase_count(&all_names);
  Array<int> is_group_used(num_vertex_groups, false);
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf.strokes) {
    if (!gps->dvert) {
      continue;
    }
    Span<MDeformVert> dverts = {gps->dvert, gps->totpoints};
    for (const MDeformVert &dvert : dverts) {
      for (const MDeformWeight &weight : Span<MDeformWeight>{dvert.dw, dvert.totweight}) {
        if (weight.def_nr >= dvert.totweight) {
          /* Ignore invalid deform weight group indices. */
          continue;
        }
        is_group_used[weight.def_nr] = true;
      }
    }
  }
  BLI_listbase_clear(&r_vertex_group_names);
  r_indices.reinitialize(num_vertex_groups);
  int new_group_i = 0;
  int old_group_i;
  LISTBASE_FOREACH_INDEX (const bDeformGroup *, def_group, &all_names, old_group_i) {
    if (!is_group_used[old_group_i]) {
      r_indices[old_group_i] = -1;
      continue;
    }
    r_indices[old_group_i] = new_group_i++;

    bDeformGroup *def_group_copy = static_cast<bDeformGroup *>(MEM_dupallocN(def_group));
    BLI_addtail(&r_vertex_group_names, def_group_copy);
  }
}

/*
 * This takes the legacy UV transforms and returns the stroke-space to texture-space matrix.
 */
static float3x2 get_legacy_stroke_to_texture_matrix(const float2 uv_translation,
                                                    const float uv_rotation,
                                                    const float2 uv_scale)
{
  using namespace blender;

  /* Bounding box data. */
  const float2 minv = float2(-1.0f, -1.0f);
  const float2 maxv = float2(1.0f, 1.0f);
  /* Center of rotation. */
  const float2 center = float2(0.5f, 0.5f);

  const float2 uv_scale_inv = math::safe_rcp(uv_scale);
  const float2 diagonal = maxv - minv;
  const float sin_rotation = sin(uv_rotation);
  const float cos_rotation = cos(uv_rotation);
  const float2x2 rotation = float2x2(float2(cos_rotation, sin_rotation),
                                     float2(-sin_rotation, cos_rotation));

  float3x2 texture_matrix = float3x2::identity();

  /* Apply bounding box rescaling. */
  texture_matrix[2] -= minv;
  texture_matrix = math::from_scale<float2x2>(1.0f / diagonal) * texture_matrix;

  /* Apply translation. */
  texture_matrix[2] += uv_translation;

  /* Apply rotation. */
  texture_matrix[2] -= center;
  texture_matrix = rotation * texture_matrix;
  texture_matrix[2] += center;

  /* Apply scale. */
  texture_matrix = math::from_scale<float2x2>(uv_scale_inv) * texture_matrix;

  return texture_matrix;
}

/*
 * This gets the legacy layer-space to stroke-space matrix.
 */
static blender::float4x2 get_legacy_layer_to_stroke_matrix(bGPDstroke *gps)
{
  using namespace blender;
  using namespace blender::math;

  const bGPDspoint *points = gps->points;
  const int totpoints = gps->totpoints;

  if (totpoints < 2) {
    return float4x2::identity();
  }

  const bGPDspoint *point0 = &points[0];
  const bGPDspoint *point1 = &points[1];
  const bGPDspoint *point3 = &points[int(totpoints * 0.75f)];

  const float3 pt0 = float3(point0->x, point0->y, point0->z);
  const float3 pt1 = float3(point1->x, point1->y, point1->z);
  const float3 pt3 = float3(point3->x, point3->y, point3->z);

  /* Local X axis (p0 -> p1) */
  const float3 local_x = normalize(pt1 - pt0);

  /* Point vector at 3/4 */
  const float3 local_3 = (totpoints == 2) ? (pt3 * 0.001f) - pt0 : pt3 - pt0;

  /* Vector orthogonal to polygon plane. */
  const float3 normal = cross(local_x, local_3);

  /* Local Y axis (cross to normal/x axis). */
  const float3 local_y = normalize(cross(normal, local_x));

  /* Get local space using first point as origin. */
  const float4x2 mat = transpose(
      float2x4(float4(local_x, -dot(pt0, local_x)), float4(local_y, -dot(pt0, local_y))));

  return mat;
}

static blender::float4x2 get_legacy_texture_matrix(bGPDstroke *gps)
{
  const float3x2 texture_matrix = get_legacy_stroke_to_texture_matrix(
      float2(gps->uv_translation), gps->uv_rotation, float2(gps->uv_scale));

  const float4x2 strokemat = get_legacy_layer_to_stroke_matrix(gps);
  float4x3 strokemat4x3 = float4x3(strokemat);
  /*
   * We need the diagonal of ones to start from the bottom right instead top left to properly apply
   * the two matrices.
   *
   * i.e.
   *          # # # #              # # # #
   * We need  # # # #  Instead of  # # # #
   *          0 0 0 1              0 0 1 0
   *
   */
  strokemat4x3[2][2] = 0.0f;
  strokemat4x3[3][2] = 1.0f;

  return texture_matrix * strokemat4x3;
}

void legacy_gpencil_frame_to_grease_pencil_drawing(const bGPDframe &gpf,
                                                   const ListBase &vertex_group_names,
                                                   GreasePencilDrawing &r_drawing)
{
  /* Construct an empty CurvesGeometry in-place. */
  new (&r_drawing.geometry) CurvesGeometry();
  r_drawing.base.type = GP_DRAWING;
  r_drawing.runtime = MEM_new<bke::greasepencil::DrawingRuntime>(__func__);

  /* Get the number of points, number of strokes and the offsets for each stroke. */
  Vector<int> offsets;
  Vector<int8_t> curve_types;
  offsets.append(0);
  int num_strokes = 0;
  int num_points = 0;
  bool has_bezier_stroke = false;
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf.strokes) {
    if (gps->editcurve != nullptr) {
      has_bezier_stroke = true;
      num_points += gps->editcurve->tot_curve_points;
      curve_types.append(CURVE_TYPE_BEZIER);
    }
    else {
      num_points += gps->totpoints;
      curve_types.append(CURVE_TYPE_POLY);
    }
    num_strokes++;
    offsets.append(num_points);
  }

  /* Resize the CurvesGeometry. */
  Drawing &drawing = r_drawing.wrap();
  CurvesGeometry &curves = drawing.strokes_for_write();
  curves.resize(num_points, num_strokes);
  if (num_strokes > 0) {
    curves.offsets_for_write().copy_from(offsets);
  }
  OffsetIndices<int> points_by_curve = curves.points_by_curve();
  MutableAttributeAccessor attributes = curves.attributes_for_write();

  if (!has_bezier_stroke) {
    /* All strokes are poly curves. */
    curves.fill_curve_types(CURVE_TYPE_POLY);
  }
  else {
    curves.curve_types_for_write().copy_from(curve_types);
    curves.update_curve_types();
  }

  /* Find used vertex groups in this drawing. */
  ListBase stroke_vertex_group_names;
  Array<int> stroke_def_nr_map;
  find_used_vertex_groups(gpf, vertex_group_names, stroke_vertex_group_names, stroke_def_nr_map);
  BLI_assert(BLI_listbase_is_empty(&curves.vertex_group_names));
  curves.vertex_group_names = stroke_vertex_group_names;
  const bool use_dverts = !BLI_listbase_is_empty(&curves.vertex_group_names);

  /* Copy vertex weights and map the vertex group indices. */
  auto copy_dvert = [&](const MDeformVert &src_dvert, MDeformVert &dst_dvert) {
    dst_dvert = src_dvert;
    dst_dvert.dw = static_cast<MDeformWeight *>(MEM_dupallocN(src_dvert.dw));
    const MutableSpan<MDeformWeight> vertex_weights = {dst_dvert.dw, dst_dvert.totweight};
    for (MDeformWeight &weight : vertex_weights) {
      if (weight.def_nr >= dst_dvert.totweight) {
        /* Ignore invalid deform weight group indices. */
        continue;
      }
      /* Map def_nr to the reduced vertex group list. */
      weight.def_nr = stroke_def_nr_map[weight.def_nr];
    }
  };

  /* Point Attributes. */
  MutableSpan<float3> positions = curves.positions_for_write();
  MutableSpan<float3> handle_positions_left = has_bezier_stroke ?
                                                  curves.handle_positions_left_for_write() :
                                                  MutableSpan<float3>();
  MutableSpan<float3> handle_positions_right = has_bezier_stroke ?
                                                   curves.handle_positions_right_for_write() :
                                                   MutableSpan<float3>();
  MutableSpan<float> radii = drawing.radii_for_write();
  MutableSpan<float> opacities = drawing.opacities_for_write();
  SpanAttributeWriter<float> delta_times = attributes.lookup_or_add_for_write_span<float>(
      "delta_time", AttrDomain::Point);
  SpanAttributeWriter<float> rotations = attributes.lookup_or_add_for_write_span<float>(
      "rotation", AttrDomain::Point);
  SpanAttributeWriter<ColorGeometry4f> vertex_colors =
      attributes.lookup_or_add_for_write_span<ColorGeometry4f>("vertex_color", AttrDomain::Point);
  SpanAttributeWriter<bool> selection = attributes.lookup_or_add_for_write_span<bool>(
      ".selection", AttrDomain::Point);
  MutableSpan<MDeformVert> dverts = use_dverts ? curves.wrap().deform_verts_for_write() :
                                                 MutableSpan<MDeformVert>();

  /* Curve Attributes. */
  SpanAttributeWriter<bool> stroke_cyclic = attributes.lookup_or_add_for_write_span<bool>(
      "cyclic", AttrDomain::Curve);
  /* TODO: This should be a `double` attribute. */
  SpanAttributeWriter<float> stroke_init_times = attributes.lookup_or_add_for_write_span<float>(
      "init_time", AttrDomain::Curve);
  SpanAttributeWriter<int8_t> stroke_start_caps = attributes.lookup_or_add_for_write_span<int8_t>(
      "start_cap", AttrDomain::Curve);
  SpanAttributeWriter<int8_t> stroke_end_caps = attributes.lookup_or_add_for_write_span<int8_t>(
      "end_cap", AttrDomain::Curve);
  SpanAttributeWriter<float> stroke_hardnesses = attributes.lookup_or_add_for_write_span<float>(
      "hardness", AttrDomain::Curve);
  SpanAttributeWriter<float> stroke_point_aspect_ratios =
      attributes.lookup_or_add_for_write_span<float>("aspect_ratio", AttrDomain::Curve);
  SpanAttributeWriter<ColorGeometry4f> stroke_fill_colors =
      attributes.lookup_or_add_for_write_span<ColorGeometry4f>(
          "fill_color",
          AttrDomain::Curve,
          bke::AttributeInitVArray(VArray<ColorGeometry4f>::ForSingle(
              ColorGeometry4f(float4(0.0f)), curves.curves_num())));
  SpanAttributeWriter<int> stroke_materials = attributes.lookup_or_add_for_write_span<int>(
      "material_index", AttrDomain::Curve);

  Array<float4x2> legacy_texture_matrices(num_strokes);

  int stroke_i = 0;
  LISTBASE_FOREACH_INDEX (bGPDstroke *, gps, &gpf.strokes, stroke_i) {
    stroke_cyclic.span[stroke_i] = (gps->flag & GP_STROKE_CYCLIC) != 0;
    /* TODO: This should be a `double` attribute. */
    stroke_init_times.span[stroke_i] = float(gps->inittime);
    stroke_start_caps.span[stroke_i] = int8_t(gps->caps[0]);
    stroke_end_caps.span[stroke_i] = int8_t(gps->caps[1]);
    stroke_hardnesses.span[stroke_i] = gps->hardness;
    stroke_point_aspect_ratios.span[stroke_i] = gps->aspect_ratio[0] /
                                                max_ff(gps->aspect_ratio[1], 1e-8);
    stroke_fill_colors.span[stroke_i] = ColorGeometry4f(gps->vert_color_fill);
    stroke_materials.span[stroke_i] = gps->mat_nr;

    IndexRange points = points_by_curve[stroke_i];
    if (points.is_empty()) {
      continue;
    }

    const Span<bGPDspoint> src_points{gps->points, gps->totpoints};
    /* Previously, Grease Pencil used a radius convention where 1 `px` = 0.001 units. This `px`
     * was the brush size which would be stored in the stroke thickness and then scaled by the
     * point pressure factor. Finally, the render engine would divide this thickness value by
     * 2000 (we're going from a thickness to a radius, hence the factor of two) to convert back
     * into blender units. Store the radius now directly in blender units. This makes it
     * consistent with how hair curves handle the radius. */
    const float stroke_thickness = float(gps->thickness) / 2000.0f;
    MutableSpan<float3> dst_positions = positions.slice(points);
    MutableSpan<float3> dst_handle_positions_left = has_bezier_stroke ?
                                                        handle_positions_left.slice(points) :
                                                        MutableSpan<float3>();
    MutableSpan<float3> dst_handle_positions_right = has_bezier_stroke ?
                                                         handle_positions_right.slice(points) :
                                                         MutableSpan<float3>();
    MutableSpan<float> dst_radii = radii.slice(points);
    MutableSpan<float> dst_opacities = opacities.slice(points);
    MutableSpan<float> dst_deltatimes = delta_times.span.slice(points);
    MutableSpan<float> dst_rotations = rotations.span.slice(points);
    MutableSpan<ColorGeometry4f> dst_vertex_colors = vertex_colors.span.slice(points);
    MutableSpan<bool> dst_selection = selection.span.slice(points);
    MutableSpan<MDeformVert> dst_dverts = use_dverts ? dverts.slice(points) :
                                                       MutableSpan<MDeformVert>();

    if (curve_types[stroke_i] == CURVE_TYPE_POLY) {
      threading::parallel_for(src_points.index_range(), 4096, [&](const IndexRange range) {
        for (const int point_i : range) {
          const bGPDspoint &pt = src_points[point_i];
          dst_positions[point_i] = float3(pt.x, pt.y, pt.z);
          dst_radii[point_i] = stroke_thickness * pt.pressure;
          dst_opacities[point_i] = pt.strength;
          dst_rotations[point_i] = pt.uv_rot;
          dst_vertex_colors[point_i] = ColorGeometry4f(pt.vert_color);
          dst_selection[point_i] = (pt.flag & GP_SPOINT_SELECT) != 0;
          if (use_dverts && gps->dvert) {
            copy_dvert(gps->dvert[point_i], dst_dverts[point_i]);
          }
        }
      });

      dst_deltatimes.first() = 0;
      threading::parallel_for(
          src_points.index_range().drop_front(1), 4096, [&](const IndexRange range) {
            for (const int point_i : range) {
              const bGPDspoint &pt = src_points[point_i];
              const bGPDspoint &pt_prev = src_points[point_i - 1];
              dst_deltatimes[point_i] = pt.time - pt_prev.time;
            }
          });
    }
    else if (curve_types[stroke_i] == CURVE_TYPE_BEZIER) {
      BLI_assert(gps->editcurve != nullptr);
      Span<bGPDcurve_point> src_curve_points{gps->editcurve->curve_points,
                                             gps->editcurve->tot_curve_points};

      threading::parallel_for(src_curve_points.index_range(), 4096, [&](const IndexRange range) {
        for (const int point_i : range) {
          const bGPDcurve_point &cpt = src_curve_points[point_i];
          dst_positions[point_i] = float3(cpt.bezt.vec[1]);
          dst_handle_positions_left[point_i] = float3(cpt.bezt.vec[0]);
          dst_handle_positions_right[point_i] = float3(cpt.bezt.vec[2]);
          dst_radii[point_i] = stroke_thickness * cpt.pressure;
          dst_opacities[point_i] = cpt.strength;
          dst_rotations[point_i] = cpt.uv_rot;
          dst_vertex_colors[point_i] = ColorGeometry4f(cpt.vert_color);
          dst_selection[point_i] = (cpt.flag & GP_CURVE_POINT_SELECT) != 0;
          if (use_dverts && gps->dvert) {
            copy_dvert(gps->dvert[point_i], dst_dverts[point_i]);
          }
        }
      });
    }
    else {
      /* Unknown curve type. */
      BLI_assert_unreachable();
    }

    const float4x2 legacy_texture_matrix = get_legacy_texture_matrix(gps);
    legacy_texture_matrices[stroke_i] = legacy_texture_matrix;
  }

  /* Ensure that the normals are up to date. */
  curves.tag_normals_changed();
  drawing.set_texture_matrices(legacy_texture_matrices.as_span(), curves.curves_range());

  delta_times.finish();
  rotations.finish();
  vertex_colors.finish();
  selection.finish();

  stroke_cyclic.finish();
  stroke_init_times.finish();
  stroke_start_caps.finish();
  stroke_end_caps.finish();
  stroke_hardnesses.finish();
  stroke_point_aspect_ratios.finish();
  stroke_fill_colors.finish();
  stroke_materials.finish();
}

void legacy_gpencil_to_grease_pencil(Main &bmain, GreasePencil &grease_pencil, bGPdata &gpd)
{
  using namespace blender::bke::greasepencil;

  if (gpd.flag & LIB_FAKEUSER) {
    id_fake_user_set(&grease_pencil.id);
  }

  BLI_assert(!grease_pencil.id.properties);
  if (gpd.id.properties) {
    grease_pencil.id.properties = IDP_CopyProperty(gpd.id.properties);
  }

  /** Convert Grease Pencil data flag. */
  SET_FLAG_FROM_TEST(
      grease_pencil.flag, (gpd.flag & GP_DATA_EXPAND) != 0, GREASE_PENCIL_ANIM_CHANNEL_EXPANDED);
  SET_FLAG_FROM_TEST(grease_pencil.flag,
                     (gpd.flag & GP_DATA_AUTOLOCK_LAYERS) != 0,
                     GREASE_PENCIL_AUTOLOCK_LAYERS);
  SET_FLAG_FROM_TEST(
      grease_pencil.flag, (gpd.draw_mode == GP_DRAWMODE_3D), GREASE_PENCIL_STROKE_ORDER_3D);

  int num_drawings = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd.layers) {
    num_drawings += BLI_listbase_count(&gpl->frames);
  }

  grease_pencil.drawing_array_num = num_drawings;
  grease_pencil.drawing_array = reinterpret_cast<GreasePencilDrawingBase **>(
      MEM_cnew_array<GreasePencilDrawing *>(num_drawings, __func__));

  int i = 0, layer_idx = 0;
  LISTBASE_FOREACH_INDEX (bGPDlayer *, gpl, &gpd.layers, layer_idx) {
    /* Create a new layer. */
    Layer &new_layer = grease_pencil.add_layer(
        StringRefNull(gpl->info, BLI_strnlen(gpl->info, 128)));

    /* Flags. */
    new_layer.set_visible((gpl->flag & GP_LAYER_HIDE) == 0);
    new_layer.set_locked((gpl->flag & GP_LAYER_LOCKED) != 0);
    new_layer.set_selected((gpl->flag & GP_LAYER_SELECT) != 0);
    SET_FLAG_FROM_TEST(
        new_layer.base.flag, (gpl->flag & GP_LAYER_FRAMELOCK), GP_LAYER_TREE_NODE_MUTE);
    SET_FLAG_FROM_TEST(
        new_layer.base.flag, (gpl->flag & GP_LAYER_USE_LIGHTS), GP_LAYER_TREE_NODE_USE_LIGHTS);
    SET_FLAG_FROM_TEST(new_layer.base.flag,
                       (gpl->onion_flag & GP_LAYER_ONIONSKIN) == 0,
                       GP_LAYER_TREE_NODE_HIDE_ONION_SKINNING);
    SET_FLAG_FROM_TEST(
        new_layer.base.flag, (gpl->flag & GP_LAYER_USE_MASK) == 0, GP_LAYER_TREE_NODE_HIDE_MASKS);

    new_layer.blend_mode = int8_t(gpl->blend_mode);

    new_layer.parent = gpl->parent;
    new_layer.set_parent_bone_name(gpl->parsubstr);

    copy_v3_v3(new_layer.translation, gpl->location);
    copy_v3_v3(new_layer.rotation, gpl->rotation);
    copy_v3_v3(new_layer.scale, gpl->scale);

    new_layer.set_view_layer_name(gpl->viewlayername);

    /* Convert the layer masks. */
    LISTBASE_FOREACH (bGPDlayer_Mask *, mask, &gpl->mask_layers) {
      LayerMask *new_mask = MEM_new<LayerMask>(__func__, mask->name);
      new_mask->flag = mask->flag;
      BLI_addtail(&new_layer.masks, new_mask);
    }
    new_layer.opacity = gpl->opacity;

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      grease_pencil.drawing_array[i] = reinterpret_cast<GreasePencilDrawingBase *>(
          MEM_new<GreasePencilDrawing>(__func__));
      GreasePencilDrawing &drawing = *reinterpret_cast<GreasePencilDrawing *>(
          grease_pencil.drawing_array[i]);

      /* Convert the frame to a drawing. */
      legacy_gpencil_frame_to_grease_pencil_drawing(*gpf, gpd.vertex_group_names, drawing);

      /* Add the frame to the layer. */
      if (GreasePencilFrame *new_frame = new_layer.add_frame(gpf->framenum, i)) {
        new_frame->type = gpf->key_type;
        SET_FLAG_FROM_TEST(new_frame->flag, (gpf->flag & GP_FRAME_SELECT), GP_FRAME_SELECTED);
      }
      i++;
    }

    if ((gpl->flag & GP_LAYER_ACTIVE) != 0) {
      grease_pencil.set_active_layer(&new_layer);
    }

    /* TODO: Update drawing user counts. */
  }

  /* Second loop, to write to layer attributes after all layers were created. */
  MutableAttributeAccessor layer_attributes = grease_pencil.attributes_for_write();
  SpanAttributeWriter<int> layer_passes = layer_attributes.lookup_or_add_for_write_span<int>(
      "pass_index", bke::AttrDomain::Layer);

  layer_idx = 0;
  LISTBASE_FOREACH_INDEX (bGPDlayer *, gpl, &gpd.layers, layer_idx) {
    layer_passes.span[layer_idx] = int(gpl->pass_index);
  }

  layer_passes.finish();

  /* Copy vertex group names and settings. */
  BKE_defgroup_copy_list(&grease_pencil.vertex_group_names, &gpd.vertex_group_names);
  grease_pencil.vertex_group_active_index = gpd.vertex_group_active_index;

  /* Convert the onion skinning settings. */
  GreasePencilOnionSkinningSettings &settings = grease_pencil.onion_skinning_settings;
  settings.opacity = gpd.onion_factor;
  settings.mode = gpd.onion_mode;
  SET_FLAG_FROM_TEST(settings.flag,
                     ((gpd.onion_flag & GP_ONION_GHOST_PREVCOL) != 0 &&
                      (gpd.onion_flag & GP_ONION_GHOST_NEXTCOL) != 0),
                     GP_ONION_SKINNING_USE_CUSTOM_COLORS);
  SET_FLAG_FROM_TEST(
      settings.flag, (gpd.onion_flag & GP_ONION_FADE) != 0, GP_ONION_SKINNING_USE_FADE);
  SET_FLAG_FROM_TEST(
      settings.flag, (gpd.onion_flag & GP_ONION_LOOP) != 0, GP_ONION_SKINNING_SHOW_LOOP);
  /* Convert keytype filter to a bit flag. */
  if (gpd.onion_keytype == -1) {
    settings.filter = GREASE_PENCIL_ONION_SKINNING_FILTER_ALL;
  }
  else {
    settings.filter = (1 << gpd.onion_keytype);
  }
  settings.num_frames_before = gpd.gstep;
  settings.num_frames_after = gpd.gstep_next;
  copy_v3_v3(settings.color_before, gpd.gcolor_prev);
  copy_v3_v3(settings.color_after, gpd.gcolor_next);

  BKE_id_materials_copy(&bmain, &gpd.id, &grease_pencil.id);

  /* Copy animation data from legacy GP data.
   *
   * Note that currently, Actions IDs are not duplicated. They may be needed ultimately, but for
   * the time being, assuming invalid fcurves/drivers are fine here. */
  if (AnimData *gpd_animdata = BKE_animdata_from_id(&gpd.id)) {
    grease_pencil.adt = BKE_animdata_copy_in_lib(
        &bmain, gpd.id.lib, gpd_animdata, LIB_ID_COPY_DEFAULT);
  }
}

static bNodeTree *add_offset_radius_node_tree(Main &bmain)
{
  using namespace blender;
  bNodeTree *group = ntreeAddTree(&bmain, DATA_("Offset Radius"), "GeometryNodeTree");

  if (!group->geometry_node_asset_traits) {
    group->geometry_node_asset_traits = MEM_new<GeometryNodeAssetTraits>(__func__);
  }
  group->geometry_node_asset_traits->flag |= GEO_NODE_ASSET_MODIFIER;

  group->tree_interface.add_socket(
      DATA_("Geometry"), "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_INPUT, nullptr);
  group->tree_interface.add_socket(
      DATA_("Geometry"), "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_OUTPUT, nullptr);

  bNodeTreeInterfaceSocket *radius_offset = group->tree_interface.add_socket(
      DATA_("Offset"), "", "NodeSocketFloat", NODE_INTERFACE_SOCKET_INPUT, nullptr);
  auto &radius_offset_data = *static_cast<bNodeSocketValueFloat *>(radius_offset->socket_data);
  radius_offset_data.subtype = PROP_DISTANCE;
  radius_offset_data.min = -FLT_MAX;
  radius_offset_data.max = FLT_MAX;

  group->tree_interface.add_socket(
      DATA_("Layer"), "", "NodeSocketString", NODE_INTERFACE_SOCKET_INPUT, nullptr);

  bNode *group_output = nodeAddNode(nullptr, group, "NodeGroupOutput");
  group_output->locx = 580;
  group_output->locy = 160;
  bNode *group_input = nodeAddNode(nullptr, group, "NodeGroupInput");
  group_input->locx = 0;
  group_input->locy = 160;

  bNode *set_curve_radius = nodeAddNode(nullptr, group, "GeometryNodeSetCurveRadius");
  set_curve_radius->locx = 400;
  set_curve_radius->locy = 160;
  bNode *named_layer_selection = nodeAddNode(
      nullptr, group, "GeometryNodeInputNamedLayerSelection");
  named_layer_selection->locx = 200;
  named_layer_selection->locy = 100;
  bNode *input_radius = nodeAddNode(nullptr, group, "GeometryNodeInputRadius");
  input_radius->locx = 0;
  input_radius->locy = 0;

  bNode *add = nodeAddNode(nullptr, group, "ShaderNodeMath");
  add->custom1 = NODE_MATH_ADD;
  add->locx = 200;
  add->locy = 0;

  nodeAddLink(group,
              group_input,
              nodeFindSocket(group_input, SOCK_OUT, "Socket_0"),
              set_curve_radius,
              nodeFindSocket(set_curve_radius, SOCK_IN, "Curve"));
  nodeAddLink(group,
              set_curve_radius,
              nodeFindSocket(set_curve_radius, SOCK_OUT, "Curve"),
              group_output,
              nodeFindSocket(group_output, SOCK_IN, "Socket_1"));

  nodeAddLink(group,
              group_input,
              nodeFindSocket(group_input, SOCK_OUT, "Socket_3"),
              named_layer_selection,
              nodeFindSocket(named_layer_selection, SOCK_IN, "Name"));
  nodeAddLink(group,
              named_layer_selection,
              nodeFindSocket(named_layer_selection, SOCK_OUT, "Selection"),
              set_curve_radius,
              nodeFindSocket(set_curve_radius, SOCK_IN, "Selection"));

  nodeAddLink(group,
              group_input,
              nodeFindSocket(group_input, SOCK_OUT, "Socket_2"),
              add,
              nodeFindSocket(add, SOCK_IN, "Value"));
  nodeAddLink(group,
              input_radius,
              nodeFindSocket(input_radius, SOCK_OUT, "Radius"),
              add,
              nodeFindSocket(add, SOCK_IN, "Value_001"));
  nodeAddLink(group,
              add,
              nodeFindSocket(add, SOCK_OUT, "Value"),
              set_curve_radius,
              nodeFindSocket(set_curve_radius, SOCK_IN, "Radius"));

  LISTBASE_FOREACH (bNode *, node, &group->nodes) {
    nodeSetSelected(node, false);
  }

  return group;
}

void thickness_factor_to_modifier(const bGPdata &src_object_data, Object &dst_object)
{
  if (src_object_data.pixfactor == 1.0f) {
    return;
  }
  const float thickness_factor = src_object_data.pixfactor;

  ModifierData *md = BKE_modifier_new(eModifierType_GreasePencilThickness);
  GreasePencilThickModifierData *tmd = reinterpret_cast<GreasePencilThickModifierData *>(md);

  tmd->thickness_fac = thickness_factor;

  STRNCPY(md->name, DATA_("Thickness"));
  BKE_modifier_unique_name(&dst_object.modifiers, md);

  BLI_addtail(&dst_object.modifiers, md);
  BKE_modifiers_persistent_uid_init(dst_object, *md);
}

void layer_adjustments_to_modifiers(Main &bmain, bGPdata &src_object_data, Object &dst_object)
{
  bNodeTree *offset_radius_node_tree = nullptr;

  /* Handling of animation here is a bit complex, since paths needs to be updated, but also
   * FCurves need to be transferred from legacy GPData animation to Object animation.
   *
   * NOTE: NLA animation in GPData that would control adjustment properties is not converted. This
   * would require (partially) re-creating a copy of the potential bGPData NLA into the Object NLA,
   * which is too complex for the few potential use cases.
   *
   * This is achieved in several steps, roughly:
   *   * For each GP layer, check if there is animation on the adjustment data.
   *   * Rename relevant FCurves RNA paths from GP animation data, and store their reference in
   *     temporary vectors.
   *   * Once all layers have been processed, move all affected FCurves from GPData animation to
   *     Object one. */

  AnimData *gpd_animdata = BKE_animdata_from_id(&src_object_data.id);
  AnimData *dst_object_animdata = BKE_animdata_from_id(&dst_object.id);

  /* Old 'adjutment' GPData RNA property path to new matching modifier property RNA path. */
  static const std::pair<const char *, const char *> fcurve_tint_valid_prop_paths[] = {
      {".tint_color", ".color"}, {".tint_factor", ".factor"}};
  static const std::pair<const char *, const char *> fcurve_thickness_valid_prop_paths[] = {
      {".line_change", "[\"Socket_1\"]"}};

  /* Store all FCurves that need to be moved from GPData animation to Object animation,
   * respectively for the main action, the temp action, and the drivers. */
  blender::Vector<FCurve *> fcurves_from_gpd_main_action;
  blender::Vector<FCurve *> fcurves_from_gpd_tmp_action;
  blender::Vector<FCurve *> fcurves_from_gpd_drivers;

  /* Common filtering of FCurve RNA path to decide whether they can/need to be processed here or
   * not. */
  auto adjustment_animation_fcurve_is_valid =
      [&](bAction *owner_action, FCurve &fcurve, blender::StringRefNull legacy_root_path) -> bool {
    /* Only take into account drivers (nullptr `action_owner`), and Actions directly assigned
     * to the animdata, not the NLA ones. */
    if (owner_action && !ELEM(owner_action, gpd_animdata->action, gpd_animdata->tmpact)) {
      return false;
    }
    if (!legacy_fcurves_is_valid_for_root_path(fcurve, legacy_root_path)) {
      return false;
    }
    return true;
  };

  /* Replace layer adjustments with modifiers. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &src_object_data.layers) {
    const float3 tint_color = float3(gpl->tintcolor);
    const float tint_factor = gpl->tintcolor[3];
    const int thickness_px = gpl->line_change;

    bool has_tint_adjustment = tint_factor > 0.0f;
    bool has_tint_adjustment_animation = false;
    bool has_thickness_adjustment = thickness_px != 0;
    bool has_thickness_adjustment_animation = false;

    char layer_name_esc[sizeof(gpl->info) * 2];
    BLI_str_escape(layer_name_esc, gpl->info, sizeof(layer_name_esc));
    const std::string legacy_root_path = fmt::format("layers[\"{}\"]", layer_name_esc);

    /* If tint or thickness are animated, relevant modifiers also need to be created. */
    if (gpd_animdata) {
      auto adjustment_animation_detection = [&](bAction *owner_action, FCurve &fcurve) -> bool {
        /* Early out if we already know that both data are animated. */
        if (has_tint_adjustment_animation && has_thickness_adjustment_animation) {
          return false;
        }
        if (!adjustment_animation_fcurve_is_valid(owner_action, fcurve, legacy_root_path)) {
          return false;
        }
        StringRefNull rna_path = fcurve.rna_path;
        for (const auto &[from_prop_path, to_prop_path] : fcurve_tint_valid_prop_paths) {
          const char *rna_adjustment_prop_path = from_prop_path;
          const std::string old_rna_path = fmt::format(
              "{}{}", legacy_root_path, rna_adjustment_prop_path);
          if (rna_path == old_rna_path) {
            has_tint_adjustment_animation = true;
            return false;
          }
        }
        for (const auto &[from_prop_path, to_prop_path] : fcurve_thickness_valid_prop_paths) {
          const char *rna_prop_rna_adjustment_prop_pathpath = from_prop_path;
          const std::string old_rna_path = fmt::format(
              "{}{}", legacy_root_path, rna_prop_rna_adjustment_prop_pathpath);
          if (rna_path == old_rna_path) {
            has_thickness_adjustment_animation = true;
            return false;
          }
        }
        return false;
      };

      legacy_animation_process(*gpd_animdata, adjustment_animation_detection);
      has_tint_adjustment = has_tint_adjustment || has_tint_adjustment_animation;
      has_thickness_adjustment = has_thickness_adjustment || has_thickness_adjustment_animation;
    }

    /* Create animdata in destination object if needed. */
    if ((has_tint_adjustment_animation || has_thickness_adjustment_animation) &&
        !dst_object_animdata)
    {
      dst_object_animdata = BKE_animdata_ensure_id(&dst_object.id);
    }

    /* Tint adjustment. */
    if (has_tint_adjustment) {
      ModifierData *md = BKE_modifier_new(eModifierType_GreasePencilTint);
      GreasePencilTintModifierData *tmd = reinterpret_cast<GreasePencilTintModifierData *>(md);

      copy_v3_v3(tmd->color, tint_color);
      tmd->factor = tint_factor;
      STRNCPY(tmd->influence.layer_name, gpl->info);

      char modifier_name[MAX_NAME];
      SNPRINTF(modifier_name, "Tint %s", gpl->info);
      STRNCPY(md->name, modifier_name);
      BKE_modifier_unique_name(&dst_object.modifiers, md);

      BLI_addtail(&dst_object.modifiers, md);
      BKE_modifiers_persistent_uid_init(dst_object, *md);

      if (has_tint_adjustment_animation) {
        char modifier_name_esc[MAX_NAME * 2];
        BLI_str_escape(modifier_name_esc, md->name, sizeof(modifier_name_esc));

        auto adjustment_tint_path_update = [&](bAction *owner_action, FCurve &fcurve) -> bool {
          if (!adjustment_animation_fcurve_is_valid(owner_action, fcurve, legacy_root_path)) {
            return false;
          }
          StringRefNull rna_path = fcurve.rna_path;
          for (const auto &[from_prop_path, to_prop_path] : fcurve_tint_valid_prop_paths) {
            const char *rna_adjustment_prop_path = from_prop_path;
            const char *rna_modifier_prop_path = to_prop_path;
            const std::string old_rna_path = fmt::format(
                "{}{}", legacy_root_path, rna_adjustment_prop_path);
            if (rna_path == old_rna_path) {
              const std::string new_rna_path = fmt::format(
                  "modifiers[\"{}\"]{}", modifier_name_esc, rna_modifier_prop_path);
              MEM_freeN(fcurve.rna_path);
              fcurve.rna_path = BLI_strdupn(new_rna_path.c_str(), new_rna_path.size());
              if (owner_action) {
                if (owner_action == gpd_animdata->action) {
                  fcurves_from_gpd_main_action.append(&fcurve);
                }
                else {
                  BLI_assert(owner_action == gpd_animdata->tmpact);
                  fcurves_from_gpd_tmp_action.append(&fcurve);
                }
              }
              else { /* Driver */
                fcurves_from_gpd_drivers.append(&fcurve);
              }
              return true;
            }
          }
          return false;
        };

        const bool has_edits = legacy_animation_process(*gpd_animdata,
                                                        adjustment_tint_path_update);
        BLI_assert(has_edits);
        UNUSED_VARS_NDEBUG(has_edits);
      }
    }
    /* Thickness adjustment. */
    if (has_thickness_adjustment) {
      /* Convert the "pixel" offset value into a radius value.
       * GPv2 used a conversion of 1 "px" = 0.001. */
      /* Note: this offset may be negative. */
      const float radius_offset = float(thickness_px) / 2000.0f;
      if (!offset_radius_node_tree) {
        offset_radius_node_tree = add_offset_radius_node_tree(bmain);
        BKE_ntree_update_main_tree(&bmain, offset_radius_node_tree, nullptr);
      }
      auto *md = reinterpret_cast<NodesModifierData *>(BKE_modifier_new(eModifierType_Nodes));

      char modifier_name[MAX_NAME];
      SNPRINTF(modifier_name, "Thickness %s", gpl->info);
      STRNCPY(md->modifier.name, modifier_name);
      BKE_modifier_unique_name(&dst_object.modifiers, &md->modifier);
      md->node_group = offset_radius_node_tree;

      BLI_addtail(&dst_object.modifiers, md);
      BKE_modifiers_persistent_uid_init(dst_object, md->modifier);

      md->settings.properties = bke::idprop::create_group("Nodes Modifier Settings").release();
      IDProperty *radius_offset_prop =
          bke::idprop::create(DATA_("Socket_2"), radius_offset).release();
      auto *ui_data = reinterpret_cast<IDPropertyUIDataFloat *>(
          IDP_ui_data_ensure(radius_offset_prop));
      ui_data->soft_min = 0.0;
      ui_data->base.rna_subtype = PROP_TRANSLATION;
      IDP_AddToGroup(md->settings.properties, radius_offset_prop);
      IDP_AddToGroup(md->settings.properties,
                     bke::idprop::create(DATA_("Socket_3"), gpl->info).release());

      if (has_thickness_adjustment_animation) {
        char modifier_name_esc[MAX_NAME * 2];
        BLI_str_escape(modifier_name_esc, md->modifier.name, sizeof(modifier_name_esc));

        auto adjustment_thickness_path_update = [&](bAction *owner_action,
                                                    FCurve &fcurve) -> bool {
          if (!adjustment_animation_fcurve_is_valid(owner_action, fcurve, legacy_root_path)) {
            return false;
          }
          StringRefNull rna_path = fcurve.rna_path;
          for (const auto &[from_prop_path, to_prop_path] : fcurve_thickness_valid_prop_paths) {
            const char *rna_adjustment_prop_path = from_prop_path;
            const char *rna_modifier_prop_path = to_prop_path;
            const std::string old_rna_path = fmt::format(
                "{}{}", legacy_root_path, rna_adjustment_prop_path);
            if (rna_path == old_rna_path) {
              const std::string new_rna_path = fmt::format(
                  "modifiers[\"{}\"]{}", modifier_name_esc, rna_modifier_prop_path);
              MEM_freeN(fcurve.rna_path);
              fcurve.rna_path = BLI_strdupn(new_rna_path.c_str(), new_rna_path.size());
              if (owner_action) {
                if (owner_action == gpd_animdata->action) {
                  fcurves_from_gpd_main_action.append(&fcurve);
                }
                else {
                  BLI_assert(owner_action == gpd_animdata->tmpact);
                  fcurves_from_gpd_tmp_action.append(&fcurve);
                }
              }
              else { /* Driver */
                fcurves_from_gpd_drivers.append(&fcurve);
              }
              return true;
            }
          }
          return false;
        };

        const bool has_edits = legacy_animation_process(*gpd_animdata,
                                                        adjustment_thickness_path_update);
        BLI_assert(has_edits);
        UNUSED_VARS_NDEBUG(has_edits);
      }
    }
  }

  if (!fcurves_from_gpd_main_action.is_empty()) {
    if (!dst_object_animdata->action) {
      dst_object_animdata->action = BKE_action_add(&bmain, gpd_animdata->action->id.name + 2);
    }
    legacy_fcurves_move(dst_object_animdata->action->curves,
                        gpd_animdata->action->curves,
                        fcurves_from_gpd_main_action);
    DEG_id_tag_update(&dst_object.id, ID_RECALC_ANIMATION);
    DEG_id_tag_update(&src_object_data.id, ID_RECALC_ANIMATION);
  }
  if (!fcurves_from_gpd_tmp_action.is_empty()) {
    if (!dst_object_animdata->tmpact) {
      dst_object_animdata->tmpact = BKE_action_add(&bmain, gpd_animdata->tmpact->id.name + 2);
    }
    legacy_fcurves_move(dst_object_animdata->tmpact->curves,
                        gpd_animdata->tmpact->curves,
                        fcurves_from_gpd_tmp_action);
    DEG_id_tag_update(&dst_object.id, ID_RECALC_ANIMATION);
    DEG_id_tag_update(&src_object_data.id, ID_RECALC_ANIMATION);
  }
  if (!fcurves_from_gpd_drivers.is_empty()) {
    legacy_fcurves_move(
        dst_object_animdata->drivers, gpd_animdata->drivers, fcurves_from_gpd_drivers);
    DEG_id_tag_update(&dst_object.id, ID_RECALC_ANIMATION);
    DEG_id_tag_update(&src_object_data.id, ID_RECALC_ANIMATION);
  }

  DEG_relations_tag_update(&bmain);
}

static ModifierData &legacy_object_modifier_common(Object &object,
                                                   const ModifierType type,
                                                   GpencilModifierData &legacy_md)
{
  /* TODO: Copy of most of #ed::object::modifier_add, this should be a BKE_modifiers function
   * actually. */
  const ModifierTypeInfo *mti = BKE_modifier_get_info(type);

  ModifierData &new_md = *BKE_modifier_new(type);

  if (mti->flags & eModifierTypeFlag_RequiresOriginalData) {
    ModifierData *md;
    for (md = static_cast<ModifierData *>(object.modifiers.first);
         md && BKE_modifier_get_info(ModifierType(md->type))->type == ModifierTypeType::OnlyDeform;
         md = md->next)
      ;
    BLI_insertlinkbefore(&object.modifiers, md, &new_md);
  }
  else {
    BLI_addtail(&object.modifiers, &new_md);
  }

  /* Generate new persistent UID and best possible unique name. */
  BKE_modifiers_persistent_uid_init(object, new_md);
  if (legacy_md.name[0]) {
    STRNCPY_UTF8(new_md.name, legacy_md.name);
  }
  BKE_modifier_unique_name(&object.modifiers, &new_md);

  /* Handle common modifier data. */
  new_md.mode = legacy_md.mode;
  new_md.flag |= legacy_md.flag & (eModifierFlag_OverrideLibrary_Local | eModifierFlag_Active);

  /* Attempt to copy UI state (panels) as best as possible. */
  new_md.ui_expand_flag = legacy_md.ui_expand_flag;

  /* Convert animation data if needed. */
  if (AnimData *anim_data = BKE_animdata_from_id(&object.id)) {
    char legacy_name_esc[MAX_NAME * 2];
    BLI_str_escape(legacy_name_esc, legacy_md.name, sizeof(legacy_name_esc));
    const std::string legacy_root_path = fmt::format("grease_pencil_modifiers[\"{}\"]",
                                                     legacy_name_esc);
    char new_name_esc[MAX_NAME * 2];
    BLI_str_escape(new_name_esc, new_md.name, sizeof(new_name_esc));

    auto modifier_path_update = [&](bAction * /*owner_action*/, FCurve &fcurve) -> bool {
      if (!legacy_fcurves_is_valid_for_root_path(fcurve, legacy_root_path)) {
        return false;
      }
      StringRefNull rna_path = fcurve.rna_path;
      const std::string new_rna_path = fmt::format(
          "modifiers[\"{}\"]{}", new_name_esc, rna_path.substr(int64_t(legacy_root_path.size())));
      MEM_freeN(fcurve.rna_path);
      fcurve.rna_path = BLI_strdupn(new_rna_path.c_str(), new_rna_path.size());
      return true;
    };

    if (legacy_animation_process(*anim_data, modifier_path_update)) {
      DEG_id_tag_update(&object.id, ID_RECALC_ANIMATION);
    }
  }

  return new_md;
}

static void legacy_object_modifier_influence(GreasePencilModifierInfluenceData &influence,
                                             StringRef layername,
                                             const int layer_pass,
                                             const bool invert_layer,
                                             const bool invert_layer_pass,
                                             Material **material,
                                             const int material_pass,
                                             const bool invert_material,
                                             const bool invert_material_pass,
                                             StringRef vertex_group_name,
                                             const bool invert_vertex_group,
                                             CurveMapping **custom_curve,
                                             const bool use_custom_curve)
{
  influence.flag = 0;

  STRNCPY(influence.layer_name, layername.data());
  if (invert_layer) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_LAYER_FILTER;
  }
  influence.layer_pass = layer_pass;
  if (layer_pass > 0) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_USE_LAYER_PASS_FILTER;
  }
  if (invert_layer_pass) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_LAYER_PASS_FILTER;
  }

  if (material) {
    influence.material = *material;
    *material = nullptr;
  }
  if (invert_material) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_FILTER;
  }
  influence.material_pass = material_pass;
  if (material_pass > 0) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_USE_MATERIAL_PASS_FILTER;
  }
  if (invert_material_pass) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_PASS_FILTER;
  }

  STRNCPY(influence.vertex_group_name, vertex_group_name.data());
  if (invert_vertex_group) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_VERTEX_GROUP;
  }

  if (custom_curve) {
    if (influence.custom_curve) {
      BKE_curvemapping_free(influence.custom_curve);
    }
    influence.custom_curve = *custom_curve;
    *custom_curve = nullptr;
  }
  if (use_custom_curve) {
    influence.flag |= GREASE_PENCIL_INFLUENCE_USE_CUSTOM_CURVE;
  }
}

static void legacy_object_modifier_armature(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilArmature, legacy_md);
  auto &md_armature = reinterpret_cast<GreasePencilArmatureModifierData &>(md);
  auto &legacy_md_armature = reinterpret_cast<ArmatureGpencilModifierData &>(legacy_md);

  md_armature.object = legacy_md_armature.object;
  legacy_md_armature.object = nullptr;
  md_armature.deformflag = legacy_md_armature.deformflag;

  legacy_object_modifier_influence(md_armature.influence,
                                   "",
                                   0,
                                   false,
                                   false,
                                   nullptr,
                                   0,
                                   false,
                                   false,
                                   legacy_md_armature.vgname,
                                   legacy_md_armature.deformflag & ARM_DEF_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_array(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilArray, legacy_md);
  auto &md_array = reinterpret_cast<GreasePencilArrayModifierData &>(md);
  auto &legacy_md_array = reinterpret_cast<ArrayGpencilModifierData &>(legacy_md);

  md_array.object = legacy_md_array.object;
  legacy_md_array.object = nullptr;
  md_array.count = legacy_md_array.count;
  md_array.flag = 0;
  if (legacy_md_array.flag & GP_ARRAY_UNIFORM_RANDOM_SCALE) {
    md_array.flag |= MOD_GREASE_PENCIL_ARRAY_UNIFORM_RANDOM_SCALE;
  }
  if (legacy_md_array.flag & GP_ARRAY_USE_OB_OFFSET) {
    md_array.flag |= MOD_GREASE_PENCIL_ARRAY_USE_OB_OFFSET;
  }
  if (legacy_md_array.flag & GP_ARRAY_USE_OFFSET) {
    md_array.flag |= MOD_GREASE_PENCIL_ARRAY_USE_OFFSET;
  }
  if (legacy_md_array.flag & GP_ARRAY_USE_RELATIVE) {
    md_array.flag |= MOD_GREASE_PENCIL_ARRAY_USE_RELATIVE;
  }
  copy_v3_v3(md_array.offset, legacy_md_array.offset);
  copy_v3_v3(md_array.shift, legacy_md_array.shift);
  copy_v3_v3(md_array.rnd_offset, legacy_md_array.rnd_offset);
  copy_v3_v3(md_array.rnd_rot, legacy_md_array.rnd_rot);
  copy_v3_v3(md_array.rnd_scale, legacy_md_array.rnd_scale);
  md_array.seed = legacy_md_array.seed;
  md_array.mat_rpl = legacy_md_array.mat_rpl;

  legacy_object_modifier_influence(md_array.influence,
                                   legacy_md_array.layername,
                                   legacy_md_array.layer_pass,
                                   legacy_md_array.flag & GP_ARRAY_INVERT_LAYER,
                                   legacy_md_array.flag & GP_ARRAY_INVERT_LAYERPASS,
                                   &legacy_md_array.material,
                                   legacy_md_array.pass_index,
                                   legacy_md_array.flag & GP_ARRAY_INVERT_MATERIAL,
                                   legacy_md_array.flag & GP_ARRAY_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_color(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilColor, legacy_md);
  auto &md_color = reinterpret_cast<GreasePencilColorModifierData &>(md);
  auto &legacy_md_color = reinterpret_cast<ColorGpencilModifierData &>(legacy_md);

  switch (eModifyColorGpencil_Flag(legacy_md_color.modify_color)) {
    case GP_MODIFY_COLOR_BOTH:
      md_color.color_mode = MOD_GREASE_PENCIL_COLOR_BOTH;
      break;
    case GP_MODIFY_COLOR_STROKE:
      md_color.color_mode = MOD_GREASE_PENCIL_COLOR_STROKE;
      break;
    case GP_MODIFY_COLOR_FILL:
      md_color.color_mode = MOD_GREASE_PENCIL_COLOR_FILL;
      break;
    case GP_MODIFY_COLOR_HARDNESS:
      md_color.color_mode = MOD_GREASE_PENCIL_COLOR_HARDNESS;
      break;
  }
  copy_v3_v3(md_color.hsv, legacy_md_color.hsv);

  legacy_object_modifier_influence(md_color.influence,
                                   legacy_md_color.layername,
                                   legacy_md_color.layer_pass,
                                   legacy_md_color.flag & GP_COLOR_INVERT_LAYER,
                                   legacy_md_color.flag & GP_COLOR_INVERT_LAYERPASS,
                                   &legacy_md_color.material,
                                   legacy_md_color.pass_index,
                                   legacy_md_color.flag & GP_COLOR_INVERT_MATERIAL,
                                   legacy_md_color.flag & GP_COLOR_INVERT_PASS,
                                   "",
                                   false,
                                   &legacy_md_color.curve_intensity,
                                   legacy_md_color.flag & GP_COLOR_CUSTOM_CURVE);
}

static void legacy_object_modifier_dash(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilDash, legacy_md);
  auto &md_dash = reinterpret_cast<GreasePencilDashModifierData &>(md);
  auto &legacy_md_dash = reinterpret_cast<DashGpencilModifierData &>(legacy_md);

  md_dash.dash_offset = legacy_md_dash.dash_offset;
  md_dash.segment_active_index = legacy_md_dash.segment_active_index;
  md_dash.segments_num = legacy_md_dash.segments_len;
  MEM_SAFE_FREE(md_dash.segments_array);
  md_dash.segments_array = MEM_cnew_array<GreasePencilDashModifierSegment>(
      legacy_md_dash.segments_len, __func__);
  for (const int i : IndexRange(md_dash.segments_num)) {
    GreasePencilDashModifierSegment &dst_segment = md_dash.segments_array[i];
    const DashGpencilModifierSegment &src_segment = legacy_md_dash.segments[i];
    STRNCPY(dst_segment.name, src_segment.name);
    dst_segment.flag = 0;
    if (src_segment.flag & GP_DASH_USE_CYCLIC) {
      dst_segment.flag |= MOD_GREASE_PENCIL_DASH_USE_CYCLIC;
    }
    dst_segment.dash = src_segment.dash;
    dst_segment.gap = src_segment.gap;
    dst_segment.opacity = src_segment.opacity;
    dst_segment.radius = src_segment.radius;
    dst_segment.mat_nr = src_segment.mat_nr;
  }

  legacy_object_modifier_influence(md_dash.influence,
                                   legacy_md_dash.layername,
                                   legacy_md_dash.layer_pass,
                                   legacy_md_dash.flag & GP_DASH_INVERT_LAYER,
                                   legacy_md_dash.flag & GP_DASH_INVERT_LAYERPASS,
                                   &legacy_md_dash.material,
                                   legacy_md_dash.pass_index,
                                   legacy_md_dash.flag & GP_DASH_INVERT_MATERIAL,
                                   legacy_md_dash.flag & GP_DASH_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_envelope(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilEnvelope, legacy_md);
  auto &md_envelope = reinterpret_cast<GreasePencilEnvelopeModifierData &>(md);
  auto &legacy_md_envelope = reinterpret_cast<EnvelopeGpencilModifierData &>(legacy_md);

  switch (eEnvelopeGpencil_Mode(legacy_md_envelope.mode)) {
    case GP_ENVELOPE_DEFORM:
      md_envelope.mode = MOD_GREASE_PENCIL_ENVELOPE_DEFORM;
      break;
    case GP_ENVELOPE_SEGMENTS:
      md_envelope.mode = MOD_GREASE_PENCIL_ENVELOPE_SEGMENTS;
      break;
    case GP_ENVELOPE_FILLS:
      md_envelope.mode = MOD_GREASE_PENCIL_ENVELOPE_FILLS;
      break;
  }
  md_envelope.mat_nr = legacy_md_envelope.mat_nr;
  md_envelope.thickness = legacy_md_envelope.thickness;
  md_envelope.strength = legacy_md_envelope.strength;
  md_envelope.skip = legacy_md_envelope.skip;
  md_envelope.spread = legacy_md_envelope.spread;

  legacy_object_modifier_influence(md_envelope.influence,
                                   legacy_md_envelope.layername,
                                   legacy_md_envelope.layer_pass,
                                   legacy_md_envelope.flag & GP_ENVELOPE_INVERT_LAYER,
                                   legacy_md_envelope.flag & GP_ENVELOPE_INVERT_LAYERPASS,
                                   &legacy_md_envelope.material,
                                   legacy_md_envelope.pass_index,
                                   legacy_md_envelope.flag & GP_ENVELOPE_INVERT_MATERIAL,
                                   legacy_md_envelope.flag & GP_ENVELOPE_INVERT_PASS,
                                   legacy_md_envelope.vgname,
                                   legacy_md_envelope.flag & GP_ENVELOPE_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_hook(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilHook, legacy_md);
  auto &md_hook = reinterpret_cast<GreasePencilHookModifierData &>(md);
  auto &legacy_md_hook = reinterpret_cast<HookGpencilModifierData &>(legacy_md);

  md_hook.flag = 0;
  if (legacy_md_hook.flag & GP_HOOK_UNIFORM_SPACE) {
    md_hook.flag |= MOD_GREASE_PENCIL_HOOK_UNIFORM_SPACE;
  }
  switch (eHookGpencil_Falloff(legacy_md_hook.falloff_type)) {
    case eGPHook_Falloff_None:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_None;
      break;
    case eGPHook_Falloff_Curve:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Curve;
      break;
    case eGPHook_Falloff_Sharp:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Sharp;
      break;
    case eGPHook_Falloff_Smooth:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Smooth;
      break;
    case eGPHook_Falloff_Root:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Root;
      break;
    case eGPHook_Falloff_Linear:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Linear;
      break;
    case eGPHook_Falloff_Const:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Const;
      break;
    case eGPHook_Falloff_Sphere:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_Sphere;
      break;
    case eGPHook_Falloff_InvSquare:
      md_hook.falloff_type = MOD_GREASE_PENCIL_HOOK_Falloff_InvSquare;
      break;
  }
  md_hook.object = legacy_md_hook.object;
  legacy_md_hook.object = nullptr;
  STRNCPY(md_hook.subtarget, legacy_md_hook.subtarget);
  copy_m4_m4(md_hook.parentinv, legacy_md_hook.parentinv);
  copy_v3_v3(md_hook.cent, legacy_md_hook.cent);
  md_hook.falloff = legacy_md_hook.falloff;
  md_hook.force = legacy_md_hook.force;

  legacy_object_modifier_influence(md_hook.influence,
                                   legacy_md_hook.layername,
                                   legacy_md_hook.layer_pass,
                                   legacy_md_hook.flag & GP_HOOK_INVERT_LAYER,
                                   legacy_md_hook.flag & GP_HOOK_INVERT_LAYERPASS,
                                   &legacy_md_hook.material,
                                   legacy_md_hook.pass_index,
                                   legacy_md_hook.flag & GP_HOOK_INVERT_MATERIAL,
                                   legacy_md_hook.flag & GP_HOOK_INVERT_PASS,
                                   legacy_md_hook.vgname,
                                   legacy_md_hook.flag & GP_HOOK_INVERT_VGROUP,
                                   &legacy_md_hook.curfalloff,
                                   true);
}

static void legacy_object_modifier_lattice(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilLattice, legacy_md);
  auto &md_lattice = reinterpret_cast<GreasePencilLatticeModifierData &>(md);
  auto &legacy_md_lattice = reinterpret_cast<LatticeGpencilModifierData &>(legacy_md);

  md_lattice.object = legacy_md_lattice.object;
  legacy_md_lattice.object = nullptr;
  md_lattice.strength = legacy_md_lattice.strength;

  legacy_object_modifier_influence(md_lattice.influence,
                                   legacy_md_lattice.layername,
                                   legacy_md_lattice.layer_pass,
                                   legacy_md_lattice.flag & GP_LATTICE_INVERT_LAYER,
                                   legacy_md_lattice.flag & GP_LATTICE_INVERT_LAYERPASS,
                                   &legacy_md_lattice.material,
                                   legacy_md_lattice.pass_index,
                                   legacy_md_lattice.flag & GP_LATTICE_INVERT_MATERIAL,
                                   legacy_md_lattice.flag & GP_LATTICE_INVERT_PASS,
                                   legacy_md_lattice.vgname,
                                   legacy_md_lattice.flag & GP_LATTICE_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_length(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilLength, legacy_md);
  auto &md_length = reinterpret_cast<GreasePencilLengthModifierData &>(md);
  auto &legacy_md_length = reinterpret_cast<LengthGpencilModifierData &>(legacy_md);

  md_length.flag = legacy_md_length.flag;
  md_length.start_fac = legacy_md_length.start_fac;
  md_length.end_fac = legacy_md_length.end_fac;
  md_length.rand_start_fac = legacy_md_length.rand_start_fac;
  md_length.rand_end_fac = legacy_md_length.rand_end_fac;
  md_length.rand_offset = legacy_md_length.rand_offset;
  md_length.overshoot_fac = legacy_md_length.overshoot_fac;
  md_length.seed = legacy_md_length.seed;
  md_length.step = legacy_md_length.step;
  md_length.mode = legacy_md_length.mode;
  md_length.point_density = legacy_md_length.point_density;
  md_length.segment_influence = legacy_md_length.segment_influence;
  md_length.max_angle = legacy_md_length.max_angle;

  legacy_object_modifier_influence(md_length.influence,
                                   legacy_md_length.layername,
                                   legacy_md_length.layer_pass,
                                   legacy_md_length.flag & GP_LENGTH_INVERT_LAYER,
                                   legacy_md_length.flag & GP_LENGTH_INVERT_LAYERPASS,
                                   &legacy_md_length.material,
                                   legacy_md_length.pass_index,
                                   legacy_md_length.flag & GP_LENGTH_INVERT_MATERIAL,
                                   legacy_md_length.flag & GP_LENGTH_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_mirror(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilMirror, legacy_md);
  auto &md_mirror = reinterpret_cast<GreasePencilMirrorModifierData &>(md);
  auto &legacy_md_mirror = reinterpret_cast<MirrorGpencilModifierData &>(legacy_md);

  md_mirror.object = legacy_md_mirror.object;
  legacy_md_mirror.object = nullptr;
  md_mirror.flag = 0;
  if (legacy_md_mirror.flag & GP_MIRROR_AXIS_X) {
    md_mirror.flag |= MOD_GREASE_PENCIL_MIRROR_AXIS_X;
  }
  if (legacy_md_mirror.flag & GP_MIRROR_AXIS_Y) {
    md_mirror.flag |= MOD_GREASE_PENCIL_MIRROR_AXIS_Y;
  }
  if (legacy_md_mirror.flag & GP_MIRROR_AXIS_Z) {
    md_mirror.flag |= MOD_GREASE_PENCIL_MIRROR_AXIS_Z;
  }

  legacy_object_modifier_influence(md_mirror.influence,
                                   legacy_md_mirror.layername,
                                   legacy_md_mirror.layer_pass,
                                   legacy_md_mirror.flag & GP_MIRROR_INVERT_LAYER,
                                   legacy_md_mirror.flag & GP_MIRROR_INVERT_LAYERPASS,
                                   &legacy_md_mirror.material,
                                   legacy_md_mirror.pass_index,
                                   legacy_md_mirror.flag & GP_MIRROR_INVERT_MATERIAL,
                                   legacy_md_mirror.flag & GP_MIRROR_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_multiply(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilMultiply, legacy_md);
  auto &md_multiply = reinterpret_cast<GreasePencilMultiModifierData &>(md);
  auto &legacy_md_multiply = reinterpret_cast<MultiplyGpencilModifierData &>(legacy_md);

  md_multiply.flag = 0;
  if (legacy_md_multiply.flags & GP_MULTIPLY_ENABLE_FADING) {
    md_multiply.flag |= MOD_GREASE_PENCIL_MULTIPLY_ENABLE_FADING;
  }
  md_multiply.duplications = legacy_md_multiply.duplications;
  md_multiply.distance = legacy_md_multiply.distance;
  md_multiply.offset = legacy_md_multiply.offset;
  md_multiply.fading_center = legacy_md_multiply.fading_center;
  md_multiply.fading_thickness = legacy_md_multiply.fading_thickness;
  md_multiply.fading_opacity = legacy_md_multiply.fading_opacity;

  /* Note: This looks wrong, but GPv2 version uses Mirror modifier flags in its `flag` property
   * and own flags in its `flags` property. */
  legacy_object_modifier_influence(md_multiply.influence,
                                   legacy_md_multiply.layername,
                                   legacy_md_multiply.layer_pass,
                                   legacy_md_multiply.flag & GP_MIRROR_INVERT_LAYER,
                                   legacy_md_multiply.flag & GP_MIRROR_INVERT_LAYERPASS,
                                   &legacy_md_multiply.material,
                                   legacy_md_multiply.pass_index,
                                   legacy_md_multiply.flag & GP_MIRROR_INVERT_MATERIAL,
                                   legacy_md_multiply.flag & GP_MIRROR_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_noise(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilNoise, legacy_md);
  auto &md_noise = reinterpret_cast<GreasePencilNoiseModifierData &>(md);
  auto &legacy_md_noise = reinterpret_cast<NoiseGpencilModifierData &>(legacy_md);

  md_noise.flag = legacy_md_noise.flag;
  md_noise.factor = legacy_md_noise.factor;
  md_noise.factor_strength = legacy_md_noise.factor_strength;
  md_noise.factor_thickness = legacy_md_noise.factor_thickness;
  md_noise.factor_uvs = legacy_md_noise.factor_uvs;
  md_noise.noise_scale = legacy_md_noise.noise_scale;
  md_noise.noise_offset = legacy_md_noise.noise_offset;
  md_noise.noise_mode = legacy_md_noise.noise_mode;
  md_noise.step = legacy_md_noise.step;
  md_noise.seed = legacy_md_noise.seed;

  legacy_object_modifier_influence(md_noise.influence,
                                   legacy_md_noise.layername,
                                   legacy_md_noise.layer_pass,
                                   legacy_md_noise.flag & GP_NOISE_INVERT_LAYER,
                                   legacy_md_noise.flag & GP_NOISE_INVERT_LAYERPASS,
                                   &legacy_md_noise.material,
                                   legacy_md_noise.pass_index,
                                   legacy_md_noise.flag & GP_NOISE_INVERT_MATERIAL,
                                   legacy_md_noise.flag & GP_NOISE_INVERT_PASS,
                                   legacy_md_noise.vgname,
                                   legacy_md_noise.flag & GP_NOISE_INVERT_VGROUP,
                                   &legacy_md_noise.curve_intensity,
                                   legacy_md_noise.flag & GP_NOISE_CUSTOM_CURVE);
}

static void legacy_object_modifier_offset(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilOffset, legacy_md);
  auto &md_offset = reinterpret_cast<GreasePencilOffsetModifierData &>(md);
  auto &legacy_md_offset = reinterpret_cast<OffsetGpencilModifierData &>(legacy_md);

  md_offset.flag = 0;
  if (legacy_md_offset.flag & GP_OFFSET_UNIFORM_RANDOM_SCALE) {
    md_offset.flag |= MOD_GREASE_PENCIL_OFFSET_UNIFORM_RANDOM_SCALE;
  }
  switch (eOffsetGpencil_Mode(legacy_md_offset.mode)) {
    case GP_OFFSET_RANDOM:
      md_offset.offset_mode = MOD_GREASE_PENCIL_OFFSET_RANDOM;
      break;
    case GP_OFFSET_LAYER:
      md_offset.offset_mode = MOD_GREASE_PENCIL_OFFSET_LAYER;
      break;
    case GP_OFFSET_MATERIAL:
      md_offset.offset_mode = MOD_GREASE_PENCIL_OFFSET_MATERIAL;
      break;
    case GP_OFFSET_STROKE:
      md_offset.offset_mode = MOD_GREASE_PENCIL_OFFSET_STROKE;
      break;
  }
  copy_v3_v3(md_offset.loc, legacy_md_offset.loc);
  copy_v3_v3(md_offset.rot, legacy_md_offset.rot);
  copy_v3_v3(md_offset.scale, legacy_md_offset.scale);
  copy_v3_v3(md_offset.stroke_loc, legacy_md_offset.rnd_offset);
  copy_v3_v3(md_offset.stroke_rot, legacy_md_offset.rnd_rot);
  copy_v3_v3(md_offset.stroke_scale, legacy_md_offset.rnd_scale);
  md_offset.seed = legacy_md_offset.seed;
  md_offset.stroke_step = legacy_md_offset.stroke_step;
  md_offset.stroke_start_offset = legacy_md_offset.stroke_start_offset;

  legacy_object_modifier_influence(md_offset.influence,
                                   legacy_md_offset.layername,
                                   legacy_md_offset.layer_pass,
                                   legacy_md_offset.flag & GP_OFFSET_INVERT_LAYER,
                                   legacy_md_offset.flag & GP_OFFSET_INVERT_LAYERPASS,
                                   &legacy_md_offset.material,
                                   legacy_md_offset.pass_index,
                                   legacy_md_offset.flag & GP_OFFSET_INVERT_MATERIAL,
                                   legacy_md_offset.flag & GP_OFFSET_INVERT_PASS,
                                   legacy_md_offset.vgname,
                                   legacy_md_offset.flag & GP_OFFSET_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_opacity(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilOpacity, legacy_md);
  auto &md_opacity = reinterpret_cast<GreasePencilOpacityModifierData &>(md);
  auto &legacy_md_opacity = reinterpret_cast<OpacityGpencilModifierData &>(legacy_md);

  md_opacity.flag = 0;
  if (legacy_md_opacity.flag & GP_OPACITY_NORMALIZE) {
    md_opacity.flag |= MOD_GREASE_PENCIL_OPACITY_USE_UNIFORM_OPACITY;
  }
  if (legacy_md_opacity.flag & GP_OPACITY_WEIGHT_FACTOR) {
    md_opacity.flag |= MOD_GREASE_PENCIL_OPACITY_USE_WEIGHT_AS_FACTOR;
  }
  switch (eModifyColorGpencil_Flag(legacy_md_opacity.modify_color)) {
    case GP_MODIFY_COLOR_BOTH:
      md_opacity.color_mode = MOD_GREASE_PENCIL_COLOR_BOTH;
      break;
    case GP_MODIFY_COLOR_STROKE:
      md_opacity.color_mode = MOD_GREASE_PENCIL_COLOR_STROKE;
      break;
    case GP_MODIFY_COLOR_FILL:
      md_opacity.color_mode = MOD_GREASE_PENCIL_COLOR_FILL;
      break;
    case GP_MODIFY_COLOR_HARDNESS:
      md_opacity.color_mode = MOD_GREASE_PENCIL_COLOR_HARDNESS;
      break;
  }
  md_opacity.color_factor = legacy_md_opacity.factor;
  md_opacity.hardness_factor = legacy_md_opacity.hardness;

  legacy_object_modifier_influence(md_opacity.influence,
                                   legacy_md_opacity.layername,
                                   legacy_md_opacity.layer_pass,
                                   legacy_md_opacity.flag & GP_OPACITY_INVERT_LAYER,
                                   legacy_md_opacity.flag & GP_OPACITY_INVERT_LAYERPASS,
                                   &legacy_md_opacity.material,
                                   legacy_md_opacity.pass_index,
                                   legacy_md_opacity.flag & GP_OPACITY_INVERT_MATERIAL,
                                   legacy_md_opacity.flag & GP_OPACITY_INVERT_PASS,
                                   legacy_md_opacity.vgname,
                                   legacy_md_opacity.flag & GP_OPACITY_INVERT_VGROUP,
                                   &legacy_md_opacity.curve_intensity,
                                   legacy_md_opacity.flag & GP_OPACITY_CUSTOM_CURVE);
}

static void legacy_object_modifier_outline(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilOutline, legacy_md);
  auto &md_outline = reinterpret_cast<GreasePencilOutlineModifierData &>(md);
  auto &legacy_md_outline = reinterpret_cast<OutlineGpencilModifierData &>(legacy_md);

  md_outline.flag = 0;
  if (legacy_md_outline.flag & GP_OUTLINE_KEEP_SHAPE) {
    md_outline.flag |= MOD_GREASE_PENCIL_OUTLINE_KEEP_SHAPE;
  }
  md_outline.object = legacy_md_outline.object;
  legacy_md_outline.object = nullptr;
  md_outline.outline_material = legacy_md_outline.outline_material;
  legacy_md_outline.outline_material = nullptr;
  md_outline.sample_length = legacy_md_outline.sample_length;
  md_outline.subdiv = legacy_md_outline.subdiv;
  md_outline.thickness = legacy_md_outline.thickness;

  legacy_object_modifier_influence(md_outline.influence,
                                   legacy_md_outline.layername,
                                   legacy_md_outline.layer_pass,
                                   legacy_md_outline.flag & GP_OUTLINE_INVERT_LAYER,
                                   legacy_md_outline.flag & GP_OUTLINE_INVERT_LAYERPASS,
                                   &legacy_md_outline.material,
                                   legacy_md_outline.pass_index,
                                   legacy_md_outline.flag & GP_OUTLINE_INVERT_MATERIAL,
                                   legacy_md_outline.flag & GP_OUTLINE_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_shrinkwrap(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilShrinkwrap, legacy_md);
  auto &md_shrinkwrap = reinterpret_cast<GreasePencilShrinkwrapModifierData &>(md);
  auto &legacy_md_shrinkwrap = reinterpret_cast<ShrinkwrapGpencilModifierData &>(legacy_md);

  /* Shrinkwrap enums and flags do not have named types. */
  /* MOD_SHRINKWRAP_NEAREST_SURFACE etc. */
  md_shrinkwrap.shrink_type = legacy_md_shrinkwrap.shrink_type;
  /* MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR etc. */
  md_shrinkwrap.shrink_opts = legacy_md_shrinkwrap.shrink_opts;
  /* MOD_SHRINKWRAP_ON_SURFACE etc. */
  md_shrinkwrap.shrink_mode = legacy_md_shrinkwrap.shrink_mode;
  /* MOD_SHRINKWRAP_PROJECT_OVER_NORMAL etc. */
  md_shrinkwrap.proj_axis = legacy_md_shrinkwrap.proj_axis;

  md_shrinkwrap.target = legacy_md_shrinkwrap.target;
  legacy_md_shrinkwrap.target = nullptr;
  md_shrinkwrap.aux_target = legacy_md_shrinkwrap.aux_target;
  legacy_md_shrinkwrap.aux_target = nullptr;
  md_shrinkwrap.keep_dist = legacy_md_shrinkwrap.keep_dist;
  md_shrinkwrap.proj_limit = legacy_md_shrinkwrap.proj_limit;
  md_shrinkwrap.subsurf_levels = legacy_md_shrinkwrap.subsurf_levels;
  md_shrinkwrap.smooth_factor = legacy_md_shrinkwrap.smooth_factor;
  md_shrinkwrap.smooth_step = legacy_md_shrinkwrap.smooth_step;

  legacy_object_modifier_influence(md_shrinkwrap.influence,
                                   legacy_md_shrinkwrap.layername,
                                   legacy_md_shrinkwrap.layer_pass,
                                   legacy_md_shrinkwrap.flag & GP_SHRINKWRAP_INVERT_LAYER,
                                   legacy_md_shrinkwrap.flag & GP_SHRINKWRAP_INVERT_LAYERPASS,
                                   &legacy_md_shrinkwrap.material,
                                   legacy_md_shrinkwrap.pass_index,
                                   legacy_md_shrinkwrap.flag & GP_SHRINKWRAP_INVERT_MATERIAL,
                                   legacy_md_shrinkwrap.flag & GP_SHRINKWRAP_INVERT_PASS,
                                   legacy_md_shrinkwrap.vgname,
                                   legacy_md_shrinkwrap.flag & GP_SHRINKWRAP_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_smooth(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilSmooth, legacy_md);
  auto &md_smooth = reinterpret_cast<GreasePencilSmoothModifierData &>(md);
  auto &legacy_md_smooth = reinterpret_cast<SmoothGpencilModifierData &>(legacy_md);

  md_smooth.flag = 0;
  if (legacy_md_smooth.flag & GP_SMOOTH_MOD_LOCATION) {
    md_smooth.flag |= MOD_GREASE_PENCIL_SMOOTH_MOD_LOCATION;
  }
  if (legacy_md_smooth.flag & GP_SMOOTH_MOD_STRENGTH) {
    md_smooth.flag |= MOD_GREASE_PENCIL_SMOOTH_MOD_STRENGTH;
  }
  if (legacy_md_smooth.flag & GP_SMOOTH_MOD_THICKNESS) {
    md_smooth.flag |= MOD_GREASE_PENCIL_SMOOTH_MOD_THICKNESS;
  }
  if (legacy_md_smooth.flag & GP_SMOOTH_MOD_UV) {
    md_smooth.flag |= MOD_GREASE_PENCIL_SMOOTH_MOD_UV;
  }
  if (legacy_md_smooth.flag & GP_SMOOTH_KEEP_SHAPE) {
    md_smooth.flag |= MOD_GREASE_PENCIL_SMOOTH_KEEP_SHAPE;
  }
  md_smooth.factor = legacy_md_smooth.factor;
  md_smooth.step = legacy_md_smooth.step;

  legacy_object_modifier_influence(md_smooth.influence,
                                   legacy_md_smooth.layername,
                                   legacy_md_smooth.layer_pass,
                                   legacy_md_smooth.flag & GP_SMOOTH_INVERT_LAYER,
                                   legacy_md_smooth.flag & GP_SMOOTH_INVERT_LAYERPASS,
                                   &legacy_md_smooth.material,
                                   legacy_md_smooth.pass_index,
                                   legacy_md_smooth.flag & GP_SMOOTH_INVERT_MATERIAL,
                                   legacy_md_smooth.flag & GP_SMOOTH_INVERT_PASS,
                                   legacy_md_smooth.vgname,
                                   legacy_md_smooth.flag & GP_SMOOTH_INVERT_VGROUP,
                                   &legacy_md_smooth.curve_intensity,
                                   legacy_md_smooth.flag & GP_SMOOTH_CUSTOM_CURVE);
}

static void legacy_object_modifier_subdiv(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilSubdiv, legacy_md);
  auto &md_subdiv = reinterpret_cast<GreasePencilSubdivModifierData &>(md);
  auto &legacy_md_subdiv = reinterpret_cast<SubdivGpencilModifierData &>(legacy_md);

  switch (eSubdivGpencil_Type(legacy_md_subdiv.type)) {
    case GP_SUBDIV_CATMULL:
      md_subdiv.type = MOD_GREASE_PENCIL_SUBDIV_CATMULL;
      break;
    case GP_SUBDIV_SIMPLE:
      md_subdiv.type = MOD_GREASE_PENCIL_SUBDIV_SIMPLE;
      break;
  }
  md_subdiv.level = legacy_md_subdiv.level;

  legacy_object_modifier_influence(md_subdiv.influence,
                                   legacy_md_subdiv.layername,
                                   legacy_md_subdiv.layer_pass,
                                   legacy_md_subdiv.flag & GP_SUBDIV_INVERT_LAYER,
                                   legacy_md_subdiv.flag & GP_SUBDIV_INVERT_LAYERPASS,
                                   &legacy_md_subdiv.material,
                                   legacy_md_subdiv.pass_index,
                                   legacy_md_subdiv.flag & GP_SUBDIV_INVERT_MATERIAL,
                                   legacy_md_subdiv.flag & GP_SUBDIV_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_texture(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilTexture, legacy_md);
  auto &md_texture = reinterpret_cast<GreasePencilTextureModifierData &>(md);
  auto &legacy_md_texture = reinterpret_cast<TextureGpencilModifierData &>(legacy_md);

  switch (eTextureGpencil_Mode(legacy_md_texture.mode)) {
    case STROKE:
      md_texture.mode = MOD_GREASE_PENCIL_TEXTURE_STROKE;
      break;
    case FILL:
      md_texture.mode = MOD_GREASE_PENCIL_TEXTURE_FILL;
      break;
    case STROKE_AND_FILL:
      md_texture.mode = MOD_GREASE_PENCIL_TEXTURE_STROKE_AND_FILL;
      break;
  }
  switch (eTextureGpencil_Fit(legacy_md_texture.fit_method)) {
    case GP_TEX_FIT_STROKE:
      md_texture.fit_method = MOD_GREASE_PENCIL_TEXTURE_FIT_STROKE;
      break;
    case GP_TEX_CONSTANT_LENGTH:
      md_texture.fit_method = MOD_GREASE_PENCIL_TEXTURE_CONSTANT_LENGTH;
      break;
  }
  md_texture.uv_offset = legacy_md_texture.uv_offset;
  md_texture.uv_scale = legacy_md_texture.uv_scale;
  md_texture.fill_rotation = legacy_md_texture.fill_rotation;
  copy_v2_v2(md_texture.fill_offset, legacy_md_texture.fill_offset);
  md_texture.fill_scale = legacy_md_texture.fill_scale;
  md_texture.layer_pass = legacy_md_texture.layer_pass;
  md_texture.alignment_rotation = legacy_md_texture.alignment_rotation;

  legacy_object_modifier_influence(md_texture.influence,
                                   legacy_md_texture.layername,
                                   legacy_md_texture.layer_pass,
                                   legacy_md_texture.flag & GP_TEX_INVERT_LAYER,
                                   legacy_md_texture.flag & GP_TEX_INVERT_LAYERPASS,
                                   &legacy_md_texture.material,
                                   legacy_md_texture.pass_index,
                                   legacy_md_texture.flag & GP_TEX_INVERT_MATERIAL,
                                   legacy_md_texture.flag & GP_TEX_INVERT_PASS,
                                   legacy_md_texture.vgname,
                                   legacy_md_texture.flag & GP_TEX_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_thickness(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilThickness, legacy_md);
  auto &md_thickness = reinterpret_cast<GreasePencilThickModifierData &>(md);
  auto &legacy_md_thickness = reinterpret_cast<ThickGpencilModifierData &>(legacy_md);

  md_thickness.flag = 0;
  if (legacy_md_thickness.flag & GP_THICK_NORMALIZE) {
    md_thickness.flag |= MOD_GREASE_PENCIL_THICK_NORMALIZE;
  }
  if (legacy_md_thickness.flag & GP_THICK_WEIGHT_FACTOR) {
    md_thickness.flag |= MOD_GREASE_PENCIL_THICK_WEIGHT_FACTOR;
  }
  md_thickness.thickness_fac = legacy_md_thickness.thickness_fac;
  md_thickness.thickness = legacy_md_thickness.thickness;

  legacy_object_modifier_influence(md_thickness.influence,
                                   legacy_md_thickness.layername,
                                   legacy_md_thickness.layer_pass,
                                   legacy_md_thickness.flag & GP_THICK_INVERT_LAYER,
                                   legacy_md_thickness.flag & GP_THICK_INVERT_LAYERPASS,
                                   &legacy_md_thickness.material,
                                   legacy_md_thickness.pass_index,
                                   legacy_md_thickness.flag & GP_THICK_INVERT_MATERIAL,
                                   legacy_md_thickness.flag & GP_THICK_INVERT_PASS,
                                   legacy_md_thickness.vgname,
                                   legacy_md_thickness.flag & GP_THICK_INVERT_VGROUP,
                                   &legacy_md_thickness.curve_thickness,
                                   legacy_md_thickness.flag & GP_THICK_CUSTOM_CURVE);
}

static void legacy_object_modifier_time(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilTime, legacy_md);
  auto &md_time = reinterpret_cast<GreasePencilTimeModifierData &>(md);
  auto &legacy_md_time = reinterpret_cast<TimeGpencilModifierData &>(legacy_md);

  md_time.flag = 0;
  if (legacy_md_time.flag & GP_TIME_CUSTOM_RANGE) {
    md_time.flag |= MOD_GREASE_PENCIL_TIME_CUSTOM_RANGE;
  }
  if (legacy_md_time.flag & GP_TIME_KEEP_LOOP) {
    md_time.flag |= MOD_GREASE_PENCIL_TIME_KEEP_LOOP;
  }
  switch (eTimeGpencil_Mode(legacy_md_time.mode)) {
    case GP_TIME_MODE_NORMAL:
      md_time.mode = MOD_GREASE_PENCIL_TIME_MODE_NORMAL;
      break;
    case GP_TIME_MODE_REVERSE:
      md_time.mode = MOD_GREASE_PENCIL_TIME_MODE_REVERSE;
      break;
    case GP_TIME_MODE_FIX:
      md_time.mode = MOD_GREASE_PENCIL_TIME_MODE_FIX;
      break;
    case GP_TIME_MODE_PINGPONG:
      md_time.mode = MOD_GREASE_PENCIL_TIME_MODE_PINGPONG;
      break;
    case GP_TIME_MODE_CHAIN:
      md_time.mode = MOD_GREASE_PENCIL_TIME_MODE_CHAIN;
      break;
  }
  md_time.offset = legacy_md_time.offset;
  md_time.frame_scale = legacy_md_time.frame_scale;
  md_time.sfra = legacy_md_time.sfra;
  md_time.efra = legacy_md_time.efra;
  md_time.segment_active_index = legacy_md_time.segment_active_index;
  md_time.segments_num = legacy_md_time.segments_len;
  MEM_SAFE_FREE(md_time.segments_array);
  md_time.segments_array = MEM_cnew_array<GreasePencilTimeModifierSegment>(
      legacy_md_time.segments_len, __func__);
  for (const int i : IndexRange(md_time.segments_num)) {
    GreasePencilTimeModifierSegment &dst_segment = md_time.segments_array[i];
    const TimeGpencilModifierSegment &src_segment = legacy_md_time.segments[i];
    STRNCPY(dst_segment.name, src_segment.name);
    switch (eTimeGpencil_Seg_Mode(src_segment.seg_mode)) {
      case GP_TIME_SEG_MODE_NORMAL:
        dst_segment.segment_mode = MOD_GREASE_PENCIL_TIME_SEG_MODE_NORMAL;
        break;
      case GP_TIME_SEG_MODE_REVERSE:
        dst_segment.segment_mode = MOD_GREASE_PENCIL_TIME_SEG_MODE_REVERSE;
        break;
      case GP_TIME_SEG_MODE_PINGPONG:
        dst_segment.segment_mode = MOD_GREASE_PENCIL_TIME_SEG_MODE_PINGPONG;
        break;
    }
    dst_segment.segment_start = src_segment.seg_start;
    dst_segment.segment_end = src_segment.seg_end;
    dst_segment.segment_repeat = src_segment.seg_repeat;
  }

  /* Note: GPv2 time modifier has a material pointer but it is unused. */
  legacy_object_modifier_influence(md_time.influence,
                                   legacy_md_time.layername,
                                   legacy_md_time.layer_pass,
                                   legacy_md_time.flag & GP_TIME_INVERT_LAYER,
                                   legacy_md_time.flag & GP_TIME_INVERT_LAYERPASS,
                                   nullptr,
                                   0,
                                   false,
                                   false,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_tint(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilTint, legacy_md);
  auto &md_tint = reinterpret_cast<GreasePencilTintModifierData &>(md);
  auto &legacy_md_tint = reinterpret_cast<TintGpencilModifierData &>(legacy_md);

  md_tint.flag = 0;
  if (legacy_md_tint.flag & GP_TINT_WEIGHT_FACTOR) {
    md_tint.flag |= MOD_GREASE_PENCIL_TINT_USE_WEIGHT_AS_FACTOR;
  }
  switch (eGp_Vertex_Mode(legacy_md_tint.mode)) {
    case GPPAINT_MODE_BOTH:
      md_tint.color_mode = MOD_GREASE_PENCIL_COLOR_BOTH;
      break;
    case GPPAINT_MODE_STROKE:
      md_tint.color_mode = MOD_GREASE_PENCIL_COLOR_STROKE;
      break;
    case GPPAINT_MODE_FILL:
      md_tint.color_mode = MOD_GREASE_PENCIL_COLOR_FILL;
      break;
  }
  switch (eTintGpencil_Type(legacy_md_tint.type)) {
    case GP_TINT_UNIFORM:
      md_tint.tint_mode = MOD_GREASE_PENCIL_TINT_UNIFORM;
      break;
    case GP_TINT_GRADIENT:
      md_tint.tint_mode = MOD_GREASE_PENCIL_TINT_GRADIENT;
      break;
  }
  md_tint.factor = legacy_md_tint.factor;
  md_tint.radius = legacy_md_tint.radius;
  copy_v3_v3(md_tint.color, legacy_md_tint.rgb);
  md_tint.object = legacy_md_tint.object;
  legacy_md_tint.object = nullptr;
  MEM_SAFE_FREE(md_tint.color_ramp);
  md_tint.color_ramp = legacy_md_tint.colorband;
  legacy_md_tint.colorband = nullptr;

  legacy_object_modifier_influence(md_tint.influence,
                                   legacy_md_tint.layername,
                                   legacy_md_tint.layer_pass,
                                   legacy_md_tint.flag & GP_TINT_INVERT_LAYER,
                                   legacy_md_tint.flag & GP_TINT_INVERT_LAYERPASS,
                                   &legacy_md_tint.material,
                                   legacy_md_tint.pass_index,
                                   legacy_md_tint.flag & GP_TINT_INVERT_MATERIAL,
                                   legacy_md_tint.flag & GP_TINT_INVERT_PASS,
                                   legacy_md_tint.vgname,
                                   legacy_md_tint.flag & GP_TINT_INVERT_VGROUP,
                                   &legacy_md_tint.curve_intensity,
                                   legacy_md_tint.flag & GP_TINT_CUSTOM_CURVE);
}

static void legacy_object_modifier_weight_angle(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilWeightAngle, legacy_md);
  auto &md_weight_angle = reinterpret_cast<GreasePencilWeightAngleModifierData &>(md);
  auto &legacy_md_weight_angle = reinterpret_cast<WeightAngleGpencilModifierData &>(legacy_md);

  md_weight_angle.flag = 0;
  if (legacy_md_weight_angle.flag & GP_WEIGHT_MULTIPLY_DATA) {
    md_weight_angle.flag |= MOD_GREASE_PENCIL_WEIGHT_ANGLE_MULTIPLY_DATA;
  }
  if (legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_OUTPUT) {
    md_weight_angle.flag |= MOD_GREASE_PENCIL_WEIGHT_ANGLE_INVERT_OUTPUT;
  }
  switch (eGpencilModifierSpace(legacy_md_weight_angle.space)) {
    case GP_SPACE_LOCAL:
      md_weight_angle.space = MOD_GREASE_PENCIL_WEIGHT_ANGLE_SPACE_LOCAL;
      break;
    case GP_SPACE_WORLD:
      md_weight_angle.space = MOD_GREASE_PENCIL_WEIGHT_ANGLE_SPACE_WORLD;
      break;
  }
  md_weight_angle.axis = legacy_md_weight_angle.axis;
  STRNCPY(md_weight_angle.target_vgname, legacy_md_weight_angle.target_vgname);
  md_weight_angle.min_weight = legacy_md_weight_angle.min_weight;
  md_weight_angle.angle = legacy_md_weight_angle.angle;

  legacy_object_modifier_influence(md_weight_angle.influence,
                                   legacy_md_weight_angle.layername,
                                   legacy_md_weight_angle.layer_pass,
                                   legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_LAYER,
                                   legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_LAYERPASS,
                                   &legacy_md_weight_angle.material,
                                   legacy_md_weight_angle.pass_index,
                                   legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_MATERIAL,
                                   legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_PASS,
                                   legacy_md_weight_angle.vgname,
                                   legacy_md_weight_angle.flag & GP_WEIGHT_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_weight_proximity(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilWeightProximity, legacy_md);
  auto &md_weight_prox = reinterpret_cast<GreasePencilWeightProximityModifierData &>(md);
  auto &legacy_md_weight_prox = reinterpret_cast<WeightProxGpencilModifierData &>(legacy_md);

  md_weight_prox.flag = 0;
  if (legacy_md_weight_prox.flag & GP_WEIGHT_MULTIPLY_DATA) {
    md_weight_prox.flag |= MOD_GREASE_PENCIL_WEIGHT_PROXIMITY_MULTIPLY_DATA;
  }
  if (legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_OUTPUT) {
    md_weight_prox.flag |= MOD_GREASE_PENCIL_WEIGHT_PROXIMITY_INVERT_OUTPUT;
  }
  STRNCPY(md_weight_prox.target_vgname, legacy_md_weight_prox.target_vgname);
  md_weight_prox.min_weight = legacy_md_weight_prox.min_weight;
  md_weight_prox.dist_start = legacy_md_weight_prox.dist_start;
  md_weight_prox.dist_end = legacy_md_weight_prox.dist_end;
  md_weight_prox.object = legacy_md_weight_prox.object;
  legacy_md_weight_prox.object = nullptr;

  legacy_object_modifier_influence(md_weight_prox.influence,
                                   legacy_md_weight_prox.layername,
                                   legacy_md_weight_prox.layer_pass,
                                   legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_LAYER,
                                   legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_LAYERPASS,
                                   &legacy_md_weight_prox.material,
                                   legacy_md_weight_prox.pass_index,
                                   legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_MATERIAL,
                                   legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_PASS,
                                   legacy_md_weight_prox.vgname,
                                   legacy_md_weight_prox.flag & GP_WEIGHT_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_weight_lineart(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilWeightAngle, legacy_md);
  auto &md_lineart = reinterpret_cast<GreasePencilLineartModifierData &>(md);
  auto &legacy_md_lineart = reinterpret_cast<LineartGpencilModifierData &>(legacy_md);

  greasepencil::convert::lineart_wrap_v3(&legacy_md_lineart, &md_lineart);
}

static void legacy_object_modifier_build(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilBuild, legacy_md);
  auto &md_build = reinterpret_cast<GreasePencilBuildModifierData &>(md);
  auto &legacy_md_build = reinterpret_cast<BuildGpencilModifierData &>(legacy_md);

  md_build.flag = 0;
  if (legacy_md_build.flag & GP_BUILD_RESTRICT_TIME) {
    md_build.flag |= MOD_GREASE_PENCIL_BUILD_RESTRICT_TIME;
  }
  if (legacy_md_build.flag & GP_BUILD_USE_FADING) {
    md_build.flag |= MOD_GREASE_PENCIL_BUILD_USE_FADING;
  }

  switch (legacy_md_build.mode) {
    case GP_BUILD_MODE_ADDITIVE:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_MODE_ADDITIVE;
      break;
    case GP_BUILD_MODE_CONCURRENT:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_MODE_CONCURRENT;
      break;
    case GP_BUILD_MODE_SEQUENTIAL:
    default:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_MODE_SEQUENTIAL;
      break;
  }

  switch (legacy_md_build.time_alignment) {
    default:
    case GP_BUILD_TIMEALIGN_START:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_TIMEALIGN_START;
      break;
    case GP_BUILD_TIMEALIGN_END:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_TIMEALIGN_END;
      break;
  }

  switch (legacy_md_build.time_mode) {
    default:
    case GP_BUILD_TIMEMODE_FRAMES:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_TIMEMODE_FRAMES;
      break;
    case GP_BUILD_TIMEMODE_PERCENTAGE:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_TIMEMODE_PERCENTAGE;
      break;
    case GP_BUILD_TIMEMODE_DRAWSPEED:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_TIMEMODE_DRAWSPEED;
      break;
  }

  switch (legacy_md_build.transition) {
    default:
    case GP_BUILD_TRANSITION_GROW:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_TRANSITION_GROW;
      break;
    case GP_BUILD_TRANSITION_SHRINK:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_TRANSITION_SHRINK;
      break;
    case GP_BUILD_TRANSITION_VANISH:
      md_build.mode = MOD_GREASE_PENCIL_BUILD_TRANSITION_VANISH;
      break;
  }

  md_build.start_frame = legacy_md_build.start_frame;
  md_build.end_frame = legacy_md_build.end_frame;
  md_build.start_delay = legacy_md_build.start_delay;
  md_build.length = legacy_md_build.length;
  md_build.fade_fac = legacy_md_build.fade_fac;
  md_build.fade_opacity_strength = legacy_md_build.fade_opacity_strength;
  md_build.fade_thickness_strength = legacy_md_build.fade_thickness_strength;
  md_build.percentage_fac = legacy_md_build.percentage_fac;
  md_build.speed_fac = legacy_md_build.speed_fac;
  md_build.speed_maxgap = legacy_md_build.speed_maxgap;
  STRNCPY(md_build.target_vgname, legacy_md_build.target_vgname);

  legacy_object_modifier_influence(md_build.influence,
                                   legacy_md_build.layername,
                                   legacy_md_build.layer_pass,
                                   legacy_md_build.flag & GP_WEIGHT_INVERT_LAYER,
                                   legacy_md_build.flag & GP_WEIGHT_INVERT_LAYERPASS,
                                   &legacy_md_build.material,
                                   legacy_md_build.pass_index,
                                   legacy_md_build.flag & GP_WEIGHT_INVERT_MATERIAL,
                                   legacy_md_build.flag & GP_WEIGHT_INVERT_PASS,
                                   legacy_md_build.target_vgname,
                                   legacy_md_build.flag & GP_WEIGHT_INVERT_VGROUP,
                                   nullptr,
                                   false);
}

static void legacy_object_modifier_simplify(Object &object, GpencilModifierData &legacy_md)
{
  ModifierData &md = legacy_object_modifier_common(
      object, eModifierType_GreasePencilSimplify, legacy_md);
  auto &md_simplify = reinterpret_cast<GreasePencilSimplifyModifierData &>(md);
  auto &legacy_md_simplify = reinterpret_cast<SimplifyGpencilModifierData &>(legacy_md);

  switch (legacy_md_simplify.mode) {
    case GP_SIMPLIFY_FIXED:
      md_simplify.mode = MOD_GREASE_PENCIL_SIMPLIFY_FIXED;
      break;
    case GP_SIMPLIFY_ADAPTIVE:
      md_simplify.mode = MOD_GREASE_PENCIL_SIMPLIFY_ADAPTIVE;
      break;
    case GP_SIMPLIFY_SAMPLE:
      md_simplify.mode = MOD_GREASE_PENCIL_SIMPLIFY_SAMPLE;
      break;
    case GP_SIMPLIFY_MERGE:
      md_simplify.mode = MOD_GREASE_PENCIL_SIMPLIFY_MERGE;
      break;
  }

  md_simplify.step = legacy_md_simplify.step;
  md_simplify.factor = legacy_md_simplify.factor;
  md_simplify.length = legacy_md_simplify.length;
  md_simplify.sharp_threshold = legacy_md_simplify.sharp_threshold;
  md_simplify.distance = legacy_md_simplify.distance;

  legacy_object_modifier_influence(md_simplify.influence,
                                   legacy_md_simplify.layername,
                                   legacy_md_simplify.layer_pass,
                                   legacy_md_simplify.flag & GP_SIMPLIFY_INVERT_LAYER,
                                   legacy_md_simplify.flag & GP_SIMPLIFY_INVERT_LAYERPASS,
                                   &legacy_md_simplify.material,
                                   legacy_md_simplify.pass_index,
                                   legacy_md_simplify.flag & GP_SIMPLIFY_INVERT_MATERIAL,
                                   legacy_md_simplify.flag & GP_SIMPLIFY_INVERT_PASS,
                                   "",
                                   false,
                                   nullptr,
                                   false);
}

static void legacy_object_modifiers(Main & /*bmain*/, Object &object)
{
  BLI_assert(BLI_listbase_is_empty(&object.modifiers));

  while (GpencilModifierData *gpd_md = static_cast<GpencilModifierData *>(
             BLI_pophead(&object.greasepencil_modifiers)))
  {
    switch (gpd_md->type) {
      case eGpencilModifierType_None:
        /* Unknown type, just ignore. */
        break;
      case eGpencilModifierType_Armature:
        legacy_object_modifier_armature(object, *gpd_md);
        break;
      case eGpencilModifierType_Array:
        legacy_object_modifier_array(object, *gpd_md);
        break;
      case eGpencilModifierType_Color:
        legacy_object_modifier_color(object, *gpd_md);
        break;
      case eGpencilModifierType_Dash:
        legacy_object_modifier_dash(object, *gpd_md);
        break;
      case eGpencilModifierType_Envelope:
        legacy_object_modifier_envelope(object, *gpd_md);
        break;
      case eGpencilModifierType_Hook:
        legacy_object_modifier_hook(object, *gpd_md);
        break;
      case eGpencilModifierType_Lattice:
        legacy_object_modifier_lattice(object, *gpd_md);
        break;
      case eGpencilModifierType_Length:
        legacy_object_modifier_length(object, *gpd_md);
        break;
      case eGpencilModifierType_Mirror:
        legacy_object_modifier_mirror(object, *gpd_md);
        break;
      case eGpencilModifierType_Multiply:
        legacy_object_modifier_multiply(object, *gpd_md);
        break;
      case eGpencilModifierType_Noise:
        legacy_object_modifier_noise(object, *gpd_md);
        break;
      case eGpencilModifierType_Offset:
        legacy_object_modifier_offset(object, *gpd_md);
        break;
      case eGpencilModifierType_Opacity:
        legacy_object_modifier_opacity(object, *gpd_md);
        break;
      case eGpencilModifierType_Outline:
        legacy_object_modifier_outline(object, *gpd_md);
        break;
      case eGpencilModifierType_Shrinkwrap:
        legacy_object_modifier_shrinkwrap(object, *gpd_md);
        break;
      case eGpencilModifierType_Smooth:
        legacy_object_modifier_smooth(object, *gpd_md);
        break;
      case eGpencilModifierType_Subdiv:
        legacy_object_modifier_subdiv(object, *gpd_md);
        break;
      case eGpencilModifierType_Texture:
        legacy_object_modifier_texture(object, *gpd_md);
        break;
      case eGpencilModifierType_Thick:
        legacy_object_modifier_thickness(object, *gpd_md);
        break;
      case eGpencilModifierType_Time:
        legacy_object_modifier_time(object, *gpd_md);
        break;
      case eGpencilModifierType_Tint:
        legacy_object_modifier_tint(object, *gpd_md);
        break;
      case eGpencilModifierType_WeightAngle:
        legacy_object_modifier_weight_angle(object, *gpd_md);
        break;
      case eGpencilModifierType_WeightProximity:
        legacy_object_modifier_weight_proximity(object, *gpd_md);
        break;
      case eGpencilModifierType_Lineart:
        legacy_object_modifier_weight_lineart(object, *gpd_md);
        break;
      case eGpencilModifierType_Build:
        legacy_object_modifier_build(object, *gpd_md);
        break;
      case eGpencilModifierType_Simplify:
        legacy_object_modifier_simplify(object, *gpd_md);
        break;
        break;
    }

    BKE_gpencil_modifier_free_ex(gpd_md, 0);
  }
}

static void legacy_gpencil_object_ex(
    Main &bmain,
    Object &object,
    std::optional<blender::Map<bGPdata *, GreasePencil *>> legacy_to_greasepencil_data)
{
  BLI_assert((GS(static_cast<ID *>(object.data)->name) == ID_GD_LEGACY));

  bGPdata *gpd = static_cast<bGPdata *>(object.data);
  GreasePencil *new_grease_pencil = nullptr;
  bool do_gpencil_data_conversion = true;

  if (legacy_to_greasepencil_data) {
    new_grease_pencil = legacy_to_greasepencil_data->lookup_default(gpd, nullptr);
    do_gpencil_data_conversion = (new_grease_pencil == nullptr);
  }

  if (!new_grease_pencil) {
    new_grease_pencil = static_cast<GreasePencil *>(
        BKE_id_new_in_lib(&bmain, gpd->id.lib, ID_GP, gpd->id.name + 2));
    id_us_min(&new_grease_pencil->id);
  }

  object.data = new_grease_pencil;
  object.type = OB_GREASE_PENCIL;

  /* NOTE: Could also use #BKE_id_free_us, to also free the legacy GP if not used anymore? */
  id_us_min(&gpd->id);
  id_us_plus(&new_grease_pencil->id);

  if (do_gpencil_data_conversion) {
    legacy_gpencil_to_grease_pencil(bmain, *new_grease_pencil, *gpd);
    if (legacy_to_greasepencil_data) {
      legacy_to_greasepencil_data->add(gpd, new_grease_pencil);
    }
  }

  legacy_object_modifiers(bmain, object);

  /* Layer adjustments should be added after all other modifiers. */
  layer_adjustments_to_modifiers(bmain, *gpd, object);
  /* Thickness factor is applied after all other changes to the radii. */
  thickness_factor_to_modifier(*gpd, object);

  BKE_object_free_derived_caches(&object);
}

void legacy_gpencil_object(Main &bmain, Object &object)
{
  legacy_gpencil_object_ex(bmain, object, std::nullopt);
}

void legacy_main(Main &bmain, BlendFileReadReport & /*reports*/)
{
  /* Allows to convert a legacy GPencil data only once, in case it's used by several objects. */
  blender::Map<bGPdata *, GreasePencil *> legacy_to_greasepencil_data;

  LISTBASE_FOREACH (Object *, object, &bmain.objects) {
    if (object->type != OB_GPENCIL_LEGACY) {
      continue;
    }
    legacy_gpencil_object_ex(bmain, *object, std::make_optional(legacy_to_greasepencil_data));
  }

  /* Potential other usages of legacy bGPdata IDs also need to be remapped to their matching new
   * GreasePencil counterparts. */
  blender::bke::id::IDRemapper gpd_remapper;
  /* Allow remapping from legacy bGPdata IDs to new GreasePencil ones. */
  gpd_remapper.allow_idtype_mismatch = true;

  LISTBASE_FOREACH (bGPdata *, legacy_gpd, &bmain.gpencils) {
    GreasePencil *new_grease_pencil = legacy_to_greasepencil_data.lookup_default(legacy_gpd,
                                                                                 nullptr);
    if (!new_grease_pencil) {
      new_grease_pencil = static_cast<GreasePencil *>(
          BKE_id_new_in_lib(&bmain, legacy_gpd->id.lib, ID_GP, legacy_gpd->id.name + 2));
      id_us_min(&new_grease_pencil->id);
      legacy_gpencil_to_grease_pencil(bmain, *new_grease_pencil, *legacy_gpd);
      legacy_to_greasepencil_data.add(legacy_gpd, new_grease_pencil);
    }
    gpd_remapper.add(&legacy_gpd->id, &new_grease_pencil->id);
  }

  BKE_libblock_remap_multiple(&bmain, gpd_remapper, ID_REMAP_ALLOW_IDTYPE_MISMATCH);
}

void lineart_wrap_v3(const LineartGpencilModifierData *lmd_legacy,
                     GreasePencilLineartModifierData *lmd)
{
#define LMD_WRAP(var) lmd->var = lmd_legacy->var

  LMD_WRAP(edge_types);
  LMD_WRAP(source_type);
  LMD_WRAP(use_multiple_levels);
  LMD_WRAP(level_start);
  LMD_WRAP(level_end);
  LMD_WRAP(source_camera);
  LMD_WRAP(light_contour_object);
  LMD_WRAP(source_object);
  LMD_WRAP(source_collection);
  LMD_WRAP(target_material);
  STRNCPY(lmd->source_vertex_group, lmd_legacy->source_vertex_group);
  STRNCPY(lmd->vgname, lmd_legacy->vgname);
  LMD_WRAP(overscan);
  LMD_WRAP(shadow_camera_fov);
  LMD_WRAP(shadow_camera_size);
  LMD_WRAP(shadow_camera_near);
  LMD_WRAP(shadow_camera_far);
  LMD_WRAP(opacity);
  lmd->thickness = lmd_legacy->thickness / 2;
  LMD_WRAP(mask_switches);
  LMD_WRAP(material_mask_bits);
  LMD_WRAP(intersection_mask);
  LMD_WRAP(shadow_selection);
  LMD_WRAP(silhouette_selection);
  LMD_WRAP(crease_threshold);
  LMD_WRAP(angle_splitting_threshold);
  LMD_WRAP(chain_smooth_tolerance);
  LMD_WRAP(chaining_image_threshold);
  LMD_WRAP(calculation_flags);
  LMD_WRAP(flags);
  LMD_WRAP(stroke_depth_offset);
  LMD_WRAP(level_start_override);
  LMD_WRAP(level_end_override);
  LMD_WRAP(edge_types_override);
  LMD_WRAP(shadow_selection_override);
  LMD_WRAP(shadow_use_silhouette_override);
  LMD_WRAP(cache);
  LMD_WRAP(la_data_ptr);

#undef LMD_WRAP
}

void lineart_unwrap_v3(LineartGpencilModifierData *lmd_legacy,
                       const GreasePencilLineartModifierData *lmd)
{
#define LMD_UNWRAP(var) lmd_legacy->var = lmd->var

  LMD_UNWRAP(edge_types);
  LMD_UNWRAP(source_type);
  LMD_UNWRAP(use_multiple_levels);
  LMD_UNWRAP(level_start);
  LMD_UNWRAP(level_end);
  LMD_UNWRAP(source_camera);
  LMD_UNWRAP(light_contour_object);
  LMD_UNWRAP(source_object);
  LMD_UNWRAP(source_collection);
  LMD_UNWRAP(target_material);
  STRNCPY(lmd_legacy->source_vertex_group, lmd->source_vertex_group);
  STRNCPY(lmd_legacy->vgname, lmd->vgname);
  LMD_UNWRAP(overscan);
  LMD_UNWRAP(shadow_camera_fov);
  LMD_UNWRAP(shadow_camera_size);
  LMD_UNWRAP(shadow_camera_near);
  LMD_UNWRAP(shadow_camera_far);
  LMD_UNWRAP(opacity);
  lmd_legacy->thickness = lmd->thickness * 2;
  LMD_UNWRAP(mask_switches);
  LMD_UNWRAP(material_mask_bits);
  LMD_UNWRAP(intersection_mask);
  LMD_UNWRAP(shadow_selection);
  LMD_UNWRAP(silhouette_selection);
  LMD_UNWRAP(crease_threshold);
  LMD_UNWRAP(angle_splitting_threshold);
  LMD_UNWRAP(chain_smooth_tolerance);
  LMD_UNWRAP(chaining_image_threshold);
  LMD_UNWRAP(calculation_flags);
  LMD_UNWRAP(flags);
  LMD_UNWRAP(stroke_depth_offset);
  LMD_UNWRAP(level_start_override);
  LMD_UNWRAP(level_end_override);
  LMD_UNWRAP(edge_types_override);
  LMD_UNWRAP(shadow_selection_override);
  LMD_UNWRAP(shadow_use_silhouette_override);
  LMD_UNWRAP(cache);
  LMD_UNWRAP(la_data_ptr);

#undef LMD_UNWRAP
}

}  // namespace blender::bke::greasepencil::convert
