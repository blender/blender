/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_attribute.hh"
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
#include "BKE_node.h"
#include "BKE_node_tree_update.hh"
#include "BKE_object.hh"

#include "BLI_color.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "DEG_depsgraph_build.hh"

namespace blender::bke::greasepencil::convert {

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
      BLI_snprintf(modifier_name, 64, "Tint %s", gpl->info);
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
      BLI_snprintf(modifier_name, 64, "Thickness %s", gpl->info);
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

  /* Handle commom modifier data. */
  new_md.mode = legacy_md.mode;
  new_md.flag |= legacy_md.flag & (eModifierFlag_OverrideLibrary_Local | eModifierFlag_Active);

  /* Attempt to copy UI state (panels) as best as possible. */
  new_md.ui_expand_flag = legacy_md.ui_expand_flag;

  return new_md;
}

static void legacy_object_modifier_noise(GreasePencilNoiseModifierData &gp_md_noise,
                                         NoiseGpencilModifierData &legacy_md_noise)
{
  gp_md_noise.flag = legacy_md_noise.flag;
  gp_md_noise.factor = legacy_md_noise.factor;
  gp_md_noise.factor_strength = legacy_md_noise.factor_strength;
  gp_md_noise.factor_thickness = legacy_md_noise.factor_thickness;
  gp_md_noise.factor_uvs = legacy_md_noise.factor_uvs;
  gp_md_noise.noise_scale = legacy_md_noise.noise_scale;
  gp_md_noise.noise_offset = legacy_md_noise.noise_offset;
  gp_md_noise.noise_mode = legacy_md_noise.noise_mode;
  gp_md_noise.step = legacy_md_noise.step;
  gp_md_noise.seed = legacy_md_noise.seed;

  STRNCPY(gp_md_noise.influence.layer_name, legacy_md_noise.layername);
  if (legacy_md_noise.flag & GP_NOISE_INVERT_LAYER) {
    gp_md_noise.influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_LAYER_FILTER;
  }
  gp_md_noise.influence.layer_pass = legacy_md_noise.layer_pass;
  if (gp_md_noise.influence.layer_pass > 0) {
    gp_md_noise.influence.flag |= GREASE_PENCIL_INFLUENCE_USE_LAYER_PASS_FILTER;
  }
  if (legacy_md_noise.flag & GP_NOISE_INVERT_LAYERPASS) {
    gp_md_noise.influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_LAYER_PASS_FILTER;
  }

  if (legacy_md_noise.material) {
    gp_md_noise.influence.material = legacy_md_noise.material;
    legacy_md_noise.material = nullptr;
  }
  if (legacy_md_noise.flag & GP_NOISE_INVERT_MATERIAL) {
    gp_md_noise.influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_FILTER;
  }
  gp_md_noise.influence.material_pass = legacy_md_noise.pass_index;
  if (gp_md_noise.influence.material_pass > 0) {
    gp_md_noise.influence.flag |= GREASE_PENCIL_INFLUENCE_USE_MATERIAL_PASS_FILTER;
  }
  if (legacy_md_noise.flag & GP_NOISE_INVERT_PASS) {
    gp_md_noise.influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_PASS_FILTER;
  }

  if (legacy_md_noise.vgname[0] != '\0') {
    STRNCPY(gp_md_noise.influence.vertex_group_name, legacy_md_noise.vgname);
  }
  if (legacy_md_noise.flag & GP_NOISE_INVERT_VGROUP) {
    gp_md_noise.influence.flag |= GREASE_PENCIL_INFLUENCE_INVERT_VERTEX_GROUP;
  }

  if (legacy_md_noise.curve_intensity) {
    if (gp_md_noise.influence.custom_curve) {
      BKE_curvemapping_free(gp_md_noise.influence.custom_curve);
    }
    gp_md_noise.influence.custom_curve = legacy_md_noise.curve_intensity;
    legacy_md_noise.curve_intensity = nullptr;
  }
  if (legacy_md_noise.flag & GP_NOISE_CUSTOM_CURVE) {
    gp_md_noise.influence.flag |= GREASE_PENCIL_INFLUENCE_USE_CUSTOM_CURVE;
  }
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
      case eGpencilModifierType_Noise: {
        NoiseGpencilModifierData &legacy_md_noise = *reinterpret_cast<NoiseGpencilModifierData *>(
            gpd_md);
        GreasePencilNoiseModifierData &gp_md_noise =
            reinterpret_cast<GreasePencilNoiseModifierData &>(
                legacy_object_modifier_common(object, eModifierType_GreasePencilNoise, *gpd_md));
        legacy_object_modifier_noise(gp_md_noise, legacy_md_noise);
        break;
      }
      case eGpencilModifierType_Subdiv:
      case eGpencilModifierType_Thick:
      case eGpencilModifierType_Tint:
      case eGpencilModifierType_Array:
      case eGpencilModifierType_Build:
      case eGpencilModifierType_Opacity:
      case eGpencilModifierType_Color:
      case eGpencilModifierType_Lattice:
      case eGpencilModifierType_Simplify:
      case eGpencilModifierType_Smooth:
      case eGpencilModifierType_Hook:
      case eGpencilModifierType_Offset:
      case eGpencilModifierType_Mirror:
      case eGpencilModifierType_Armature:
      case eGpencilModifierType_Time:
      case eGpencilModifierType_Multiply:
      case eGpencilModifierType_Texture:
      case eGpencilModifierType_Lineart:
      case eGpencilModifierType_Length:
      case eGpencilModifierType_WeightProximity:
      case eGpencilModifierType_Dash:
      case eGpencilModifierType_WeightAngle:
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

  /* Layer adjusments should be added after all other modifiers. */
  layer_adjustments_to_modifiers(bmain, *gpd, object);
  /* Thickness factor is applied after all other changes to the radii. */
  thickness_factor_to_modifier(*gpd, object);

  BKE_object_free_derived_caches(&object);
}

}  // namespace blender::bke::greasepencil::convert
