/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <fmt/format.h>

#include "BKE_anim_data.h"
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
#include "BKE_material.h"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_object.hh"

#include "BLI_color.hh"
#include "BLI_function_ref.hh"
#include "BLI_listbase.h"
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

static bool legacy_fcurves_process(ListBase &fcurves,
                                   blender::FunctionRef<bool(FCurve *fcurve)> callback)
{
  bool is_changed = false;
  LISTBASE_FOREACH (FCurve *, fcurve, &fcurves) {
    const bool local_is_changed = callback(fcurve);
    is_changed = is_changed || local_is_changed;
  }
  return is_changed;
}

static bool legacy_nla_strip_process(NlaStrip &nla_strip,
                                     blender::FunctionRef<bool(FCurve *fcurve)> callback)
{
  bool is_changed = false;
  if (nla_strip.act) {
    if (legacy_fcurves_process(nla_strip.act->curves, callback)) {
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
                                     blender::FunctionRef<bool(FCurve *fcurve)> callback)
{
  bool is_changed = false;
  if (anim_data.action) {
    if (legacy_fcurves_process(anim_data.action->curves, callback)) {
      DEG_id_tag_update(&anim_data.action->id, ID_RECALC_ANIMATION);
      is_changed = true;
    }
  }
  if (anim_data.tmpact) {
    if (legacy_fcurves_process(anim_data.tmpact->curves, callback)) {
      DEG_id_tag_update(&anim_data.tmpact->id, ID_RECALC_ANIMATION);
      is_changed = true;
    }
  }

  {
    const bool local_is_changed = legacy_fcurves_process(anim_data.drivers, callback);
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
  SpanAttributeWriter<float2> stroke_fill_translations =
      attributes.lookup_or_add_for_write_span<float2>("fill_translation", AttrDomain::Curve);
  SpanAttributeWriter<float> stroke_fill_rotations =
      attributes.lookup_or_add_for_write_span<float>("fill_rotation", AttrDomain::Curve);
  SpanAttributeWriter<float2> stroke_fill_scales = attributes.lookup_or_add_for_write_span<float2>(
      "fill_scale", AttrDomain::Curve);
  SpanAttributeWriter<ColorGeometry4f> stroke_fill_colors =
      attributes.lookup_or_add_for_write_span<ColorGeometry4f>("fill_color", AttrDomain::Curve);
  SpanAttributeWriter<int> stroke_materials = attributes.lookup_or_add_for_write_span<int>(
      "material_index", AttrDomain::Curve);

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
    stroke_fill_translations.span[stroke_i] = float2(gps->uv_translation);
    stroke_fill_rotations.span[stroke_i] = gps->uv_rotation;
    stroke_fill_scales.span[stroke_i] = float2(gps->uv_scale);
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
  }

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
  stroke_fill_translations.finish();
  stroke_fill_rotations.finish();
  stroke_fill_scales.finish();
  stroke_fill_colors.finish();
  stroke_materials.finish();
}

void legacy_gpencil_to_grease_pencil(Main &bmain, GreasePencil &grease_pencil, bGPdata &gpd)
{
  using namespace blender::bke::greasepencil;

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
                       (gpl->onion_flag & GP_LAYER_ONIONSKIN),
                       GP_LAYER_TREE_NODE_USE_ONION_SKINNING);

    new_layer.blend_mode = int8_t(gpl->blend_mode);

    new_layer.parent = gpl->parent;
    new_layer.set_parent_bone_name(gpl->parsubstr);

    copy_v3_v3(new_layer.translation, gpl->location);
    copy_v3_v3(new_layer.rotation, gpl->rotation);
    copy_v3_v3(new_layer.scale, gpl->scale);

    /* Convert the layer masks. */
    LISTBASE_FOREACH (bGPDlayer_Mask *, mask, &gpl->mask_layers) {
      LayerMask *new_mask = MEM_new<LayerMask>(mask->name);
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
  grease_pencil.onion_skinning_settings.opacity = gpd.onion_factor;
  grease_pencil.onion_skinning_settings.mode = gpd.onion_mode;
  if (gpd.onion_keytype == -1) {
    grease_pencil.onion_skinning_settings.filter = GREASE_PENCIL_ONION_SKINNING_FILTER_ALL;
  }
  else {
    grease_pencil.onion_skinning_settings.filter = (1 << gpd.onion_keytype);
  }
  grease_pencil.onion_skinning_settings.num_frames_before = gpd.gstep;
  grease_pencil.onion_skinning_settings.num_frames_after = gpd.gstep_next;
  copy_v3_v3(grease_pencil.onion_skinning_settings.color_before, gpd.gcolor_prev);
  copy_v3_v3(grease_pencil.onion_skinning_settings.color_after, gpd.gcolor_next);

  BKE_id_materials_copy(&bmain, &gpd.id, &grease_pencil.id);
}

static bNodeTree *add_offset_radius_node_tree(Main &bmain)
{
  using namespace blender;
  bNodeTree *group = ntreeAddTree(&bmain, DATA_("Offset Radius"), "GeometryNodeTree");

  if (!group->geometry_node_asset_traits) {
    group->geometry_node_asset_traits = MEM_new<GeometryNodeAssetTraits>(__func__);
  }
  group->geometry_node_asset_traits->flag |= GEO_NODE_ASSET_MODIFIER;

  group->tree_interface.add_socket(DATA_("Geometry"),
                                   "",
                                   "NodeSocketGeometry",
                                   NODE_INTERFACE_SOCKET_INPUT | NODE_INTERFACE_SOCKET_OUTPUT,
                                   nullptr);

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
              nodeFindSocket(group_output, SOCK_IN, "Socket_0"));

  nodeAddLink(group,
              group_input,
              nodeFindSocket(group_input, SOCK_OUT, "Socket_2"),
              named_layer_selection,
              nodeFindSocket(named_layer_selection, SOCK_IN, "Name"));
  nodeAddLink(group,
              named_layer_selection,
              nodeFindSocket(named_layer_selection, SOCK_OUT, "Selection"),
              set_curve_radius,
              nodeFindSocket(set_curve_radius, SOCK_IN, "Selection"));

  nodeAddLink(group,
              group_input,
              nodeFindSocket(group_input, SOCK_OUT, "Socket_1"),
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

void layer_adjustments_to_modifiers(Main &bmain,
                                    const bGPdata &src_object_data,
                                    Object &dst_object)
{
  bNodeTree *offset_radius_node_tree = nullptr;
  /* Replace layer adjustments with modifiers. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &src_object_data.layers) {
    const float3 tint_color = float3(gpl->tintcolor);
    const float tint_factor = gpl->tintcolor[3];
    const int thickness_px = gpl->line_change;
    /* Tint adjustment. */
    if (tint_factor > 0.0f) {
      ModifierData *md = BKE_modifier_new(eModifierType_GreasePencilTint);
      GreasePencilTintModifierData *tmd = reinterpret_cast<GreasePencilTintModifierData *>(md);

      copy_v3_v3(tmd->color, tint_color);
      tmd->factor = tint_factor;
      STRNCPY(tmd->influence.layer_name, gpl->info);

      char modifier_name[64];
      SNPRINTF(modifier_name, "Tint %s", gpl->info);
      STRNCPY(md->name, modifier_name);
      BKE_modifier_unique_name(&dst_object.modifiers, md);

      BLI_addtail(&dst_object.modifiers, md);
      BKE_modifiers_persistent_uid_init(dst_object, *md);
    }
    /* Thickness adjustment. */
    if (thickness_px != 0) {
      /* Convert the "pixel" offset value into a radius value.
       * GPv2 used a conversion of 1 "px" = 0.001. */
      /* Note: this offset may be negative. */
      const float radius_offset = float(thickness_px) / 2000.0f;
      if (!offset_radius_node_tree) {
        offset_radius_node_tree = add_offset_radius_node_tree(bmain);
        BKE_ntree_update_main_tree(&bmain, offset_radius_node_tree, nullptr);
      }
      auto *md = reinterpret_cast<NodesModifierData *>(BKE_modifier_new(eModifierType_Nodes));

      char modifier_name[64];
      SNPRINTF(modifier_name, "Thickness %s", gpl->info);
      STRNCPY(md->modifier.name, modifier_name);
      BKE_modifier_unique_name(&dst_object.modifiers, &md->modifier);
      md->node_group = offset_radius_node_tree;

      BLI_addtail(&dst_object.modifiers, md);
      BKE_modifiers_persistent_uid_init(dst_object, md->modifier);

      md->settings.properties = bke::idprop::create_group("Nodes Modifier Settings").release();
      IDProperty *radius_offset_prop =
          bke::idprop::create(DATA_("Socket_1"), radius_offset).release();
      auto *ui_data = reinterpret_cast<IDPropertyUIDataFloat *>(
          IDP_ui_data_ensure(radius_offset_prop));
      ui_data->soft_min = 0.0f;
      ui_data->base.rna_subtype = PROP_TRANSLATION;
      IDP_AddToGroup(md->settings.properties, radius_offset_prop);
      IDP_AddToGroup(md->settings.properties,
                     bke::idprop::create(DATA_("Socket_2"), gpl->info).release());
    }
  }

  DEG_relations_tag_update(&bmain);
}

static ModifierData &legacy_object_modifier_common(Object &object,
                                                   const ModifierType type,
                                                   GpencilModifierData &legacy_md)
{
  /* TODO: Copy of most of #ED_object_modifier_add, this should be a BKE_modifiers function
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
  AnimData *anim_data = BKE_animdata_from_id(&object.id);
  if (anim_data) {
    auto modifier_path_update = [&](FCurve *fcurve) -> bool {
      /* NOTE: This logic will likely need to be re-used in other similar conditions for other
       * areas, should be put into its own util then. */
      if (!fcurve->rna_path) {
        return false;
      }
      StringRefNull rna_path = fcurve->rna_path;
      const std::string legacy_root_path = fmt::format("grease_pencil_modifiers[\"{}\"]",
                                                       legacy_md.name);
      if (!rna_path.startswith(legacy_root_path)) {
        return false;
      }
      const std::string new_rna_path = fmt::format(
          "modifiers[\"{}\"]{}", new_md.name, rna_path.substr(int64_t(legacy_root_path.size())));
      MEM_freeN(fcurve->rna_path);
      fcurve->rna_path = BLI_strdupn(new_rna_path.c_str(), new_rna_path.size());
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
      case eGpencilModifierType_Array:
        legacy_object_modifier_array(object, *gpd_md);
        break;
      case eGpencilModifierType_Color:
        legacy_object_modifier_color(object, *gpd_md);
        break;
      case eGpencilModifierType_Dash:
        legacy_object_modifier_dash(object, *gpd_md);
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
      case eGpencilModifierType_Smooth:
        legacy_object_modifier_smooth(object, *gpd_md);
        break;
      case eGpencilModifierType_Subdiv:
        legacy_object_modifier_subdiv(object, *gpd_md);
        break;
      case eGpencilModifierType_Thick:
        legacy_object_modifier_thickness(object, *gpd_md);
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

      case eGpencilModifierType_Build:
      case eGpencilModifierType_Simplify:
      case eGpencilModifierType_Armature:
      case eGpencilModifierType_Time:
      case eGpencilModifierType_Texture:
      case eGpencilModifierType_Lineart:
      case eGpencilModifierType_Shrinkwrap:
      case eGpencilModifierType_Envelope:
      case eGpencilModifierType_Outline:
        break;
    }

    BKE_gpencil_modifier_free_ex(gpd_md, 0);
  }
}

void legacy_gpencil_object(Main &bmain, Object &object)
{
  bGPdata *gpd = static_cast<bGPdata *>(object.data);

  GreasePencil *new_grease_pencil = static_cast<GreasePencil *>(
      BKE_id_new(&bmain, ID_GP, gpd->id.name + 2));
  object.data = new_grease_pencil;
  object.type = OB_GREASE_PENCIL;

  /* NOTE: Could also use #BKE_id_free_us, to also free the legacy GP if not used anymore? */
  id_us_min(&gpd->id);
  /* No need to increase user-count of `new_grease_pencil`,
   * since ID creation already set it to 1. */

  legacy_gpencil_to_grease_pencil(bmain, *new_grease_pencil, *gpd);

  legacy_object_modifiers(bmain, object);

  /* Layer adjustments should be added after all other modifiers. */
  layer_adjustments_to_modifiers(bmain, *gpd, object);
  /* Thickness factor is applied after all other changes to the radii. */
  thickness_factor_to_modifier(*gpd, object);

  BKE_object_free_derived_caches(&object);
}

}  // namespace blender::bke::greasepencil::convert
