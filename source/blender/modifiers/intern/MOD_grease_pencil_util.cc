/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "MOD_grease_pencil_util.hh"

#include "BLI_set.hh"
#include "BLI_vector_set.hh"

#include "DNA_grease_pencil_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_query.hh"
#include "BKE_material.hh"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "GEO_resample_curves.hh"

namespace blender::modifier::greasepencil {

using bke::greasepencil::Drawing;
using bke::greasepencil::FramesMapKeyT;
using bke::greasepencil::Layer;
using bke::greasepencil::LayerGroup;

void init_influence_data(GreasePencilModifierInfluenceData *influence_data,
                         const bool has_custom_curve)
{
  if (has_custom_curve) {
    influence_data->custom_curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
    BKE_curvemapping_init(influence_data->custom_curve);
  }
}

void copy_influence_data(const GreasePencilModifierInfluenceData *influence_data_src,
                         GreasePencilModifierInfluenceData *influence_data_dst,
                         const int /*flag*/)
{
  memcpy(influence_data_dst, influence_data_src, sizeof(GreasePencilModifierInfluenceData));
  influence_data_dst->custom_curve = BKE_curvemapping_copy(influence_data_src->custom_curve);
}

void free_influence_data(GreasePencilModifierInfluenceData *influence_data)
{
  if (influence_data->custom_curve) {
    BKE_curvemapping_free(influence_data->custom_curve);
    influence_data->custom_curve = nullptr;
  }
}

void foreach_influence_ID_link(GreasePencilModifierInfluenceData *influence_data,
                               Object *ob,
                               IDWalkFunc walk,
                               void *user_data)
{
  walk(user_data, ob, (ID **)&influence_data->material, IDWALK_CB_USER);
}

void write_influence_data(BlendWriter *writer,
                          const GreasePencilModifierInfluenceData *influence_data)
{
  if (influence_data->custom_curve) {
    BKE_curvemapping_blend_write(writer, influence_data->custom_curve);
  }
}

void read_influence_data(BlendDataReader *reader,
                         GreasePencilModifierInfluenceData *influence_data)
{
  BLO_read_struct(reader, CurveMapping, &influence_data->custom_curve);
  if (influence_data->custom_curve) {
    BKE_curvemapping_blend_read(reader, influence_data->custom_curve);
    /* Make sure the internal table exists. */
    BKE_curvemapping_init(influence_data->custom_curve);
  }
}

void draw_layer_filter_settings(const bContext * /*C*/, uiLayout *layout, PointerRNA *ptr)
{
  PointerRNA ob_ptr = RNA_pointer_create_discrete(ptr->owner_id, &RNA_Object, ptr->owner_id);
  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");
  const bool use_layer_pass = RNA_boolean_get(ptr, "use_layer_pass_filter");
  const bool use_layer_group_filter = RNA_boolean_get(ptr, "use_layer_group_filter");
  uiLayout *row, *col, *sub, *subsub;

  layout->use_property_split_set(true);

  col = &layout->column(true);
  row = &col->row(true);
  row->use_property_decorate_set(false);
  if (use_layer_group_filter) {
    row->prop_search(ptr,
                     "tree_node_filter",
                     &obj_data_ptr,
                     "layer_groups",
                     IFACE_("Group"),
                     ICON_GREASEPENCIL_LAYER_GROUP);
  }
  else {
    row->prop_search(ptr,
                     "tree_node_filter",
                     &obj_data_ptr,
                     "layers",
                     std::nullopt,
                     ICON_OUTLINER_DATA_GP_LAYER);
  }
  sub = &row->row(true);
  sub->prop(ptr, "use_layer_group_filter", UI_ITEM_NONE, "", ICON_GREASEPENCIL_LAYER_GROUP);
  sub->prop(ptr, "invert_layer_filter", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);

  row = &col->row(true, IFACE_("Layer Pass"));
  row->use_property_decorate_set(false);
  sub = &row->row(true);
  sub->prop(ptr, "use_layer_pass_filter", UI_ITEM_NONE, "", ICON_NONE);
  subsub = &sub->row(true);
  subsub->active_set(use_layer_pass);
  subsub->prop(ptr, "layer_pass_filter", UI_ITEM_NONE, "", ICON_NONE);
  subsub->prop(ptr, "invert_layer_pass_filter", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
}

void draw_material_filter_settings(const bContext * /*C*/, uiLayout *layout, PointerRNA *ptr)
{
  PointerRNA ob_ptr = RNA_pointer_create_discrete(ptr->owner_id, &RNA_Object, ptr->owner_id);
  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");
  const bool use_material_pass = RNA_boolean_get(ptr, "use_material_pass_filter");
  uiLayout *row, *col, *sub, *subsub;

  layout->use_property_split_set(true);

  col = &layout->column(true);
  row = &col->row(true);
  row->use_property_decorate_set(false);
  row->prop_search(
      ptr, "material_filter", &obj_data_ptr, "materials", std::nullopt, ICON_SHADING_TEXTURE);
  sub = &row->row(true);
  sub->prop(ptr, "invert_material_filter", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);

  row = &col->row(true, IFACE_("Material Pass"));
  row->use_property_decorate_set(false);
  sub = &row->row(true);
  sub->prop(ptr, "use_material_pass_filter", UI_ITEM_NONE, "", ICON_NONE);
  subsub = &sub->row(true);
  subsub->active_set(use_material_pass);
  subsub->prop(ptr, "material_pass_filter", UI_ITEM_NONE, "", ICON_NONE);
  subsub->prop(ptr, "invert_material_pass_filter", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
}

void draw_vertex_group_settings(const bContext * /*C*/, uiLayout *layout, PointerRNA *ptr)
{
  PointerRNA ob_ptr = RNA_pointer_create_discrete(ptr->owner_id, &RNA_Object, ptr->owner_id);
  bool has_vertex_group = RNA_string_length(ptr, "vertex_group_name") != 0;
  uiLayout *row, *col, *sub;

  layout->use_property_split_set(true);

  col = &layout->column(true);
  row = &col->row(true);
  row->use_property_decorate_set(false);
  row->prop_search(ptr, "vertex_group_name", &ob_ptr, "vertex_groups", std::nullopt, ICON_NONE);
  sub = &row->row(true);
  sub->active_set(has_vertex_group);
  sub->use_property_decorate_set(false);
  sub->prop(ptr, "invert_vertex_group", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
}

void draw_custom_curve_settings(const bContext * /*C*/, uiLayout *layout, PointerRNA *ptr)
{
  bool use_custom_curve = RNA_boolean_get(ptr, "use_custom_curve");
  uiLayout *row;

  layout->use_property_split_set(true);
  row = &layout->row(true);
  row->use_property_decorate_set(false);
  row->prop(ptr, "use_custom_curve", UI_ITEM_NONE, IFACE_("Custom Curve"), ICON_NONE);
  if (use_custom_curve) {
    uiTemplateCurveMapping(layout, ptr, "custom_curve", 0, false, false, false, false, false);
  }
}

/**
 * Get a list of pass IDs used by grease pencil materials.
 * This way the material pass can be looked up by index instead of having to get the material for
 * each curve.
 */
static Vector<int> get_grease_pencil_material_passes(const Object *ob)
{
  short *totcol = BKE_object_material_len_p(const_cast<Object *>(ob));
  Vector<int> result(*totcol, 0);
  for (short i = 0; i < *totcol; i++) {
    const Material *ma = BKE_object_material_get(const_cast<Object *>(ob), i + 1);
    if (ma) {
      /* Pass index of the grease pencil material. */
      result[i] = ma->gp_style->index;
    }
  }
  return result;
}

static IndexMask get_filtered_layer_mask(const GreasePencil &grease_pencil,
                                         const std::optional<StringRef> tree_node_name_filter,
                                         const std::optional<int> layer_pass_filter,
                                         const bool layer_filter_invert,
                                         const bool layer_pass_filter_invert,
                                         IndexMaskMemory &memory)
{
  const IndexMask full_mask = grease_pencil.layers().index_range();
  if (!tree_node_name_filter && !layer_pass_filter) {
    return full_mask;
  }

  bke::AttributeAccessor layer_attributes = grease_pencil.attributes();
  const Span<const Layer *> layers = grease_pencil.layers();
  const VArray<int> layer_passes =
      layer_attributes.lookup_or_default<int>("pass_index", bke::AttrDomain::Layer, 0).varray;

  const LayerGroup *filter_layer_group = nullptr;
  if (tree_node_name_filter) {
    for (const LayerGroup *group : grease_pencil.layer_groups()) {
      if (group->name() == tree_node_name_filter.value()) {
        filter_layer_group = group;
        break;
      }
    }
  }

  IndexMask result = IndexMask::from_predicate(
      full_mask, GrainSize(4096), memory, [&](const int64_t layer_i) {
        if (tree_node_name_filter) {
          const Layer *layer = layers[layer_i];
          if (filter_layer_group) {
            const bool match = layer->is_child_of(*filter_layer_group);
            if (match == layer_filter_invert) {
              return false;
            }
          }
          else {
            const bool match = (layer->name() == tree_node_name_filter.value());
            if (match == layer_filter_invert) {
              return false;
            }
          }
        }
        if (layer_pass_filter) {
          const int layer_pass = layer_passes.get(layer_i);
          const bool match = (layer_pass == layer_pass_filter.value());
          if (match == layer_pass_filter_invert) {
            return false;
          }
        }
        return true;
      });
  return result;
}

IndexMask get_filtered_layer_mask(const GreasePencil &grease_pencil,
                                  const GreasePencilModifierInfluenceData &influence_data,
                                  IndexMaskMemory &memory)
{
  return get_filtered_layer_mask(
      grease_pencil,
      influence_data.layer_name[0] != '\0' ?
          std::make_optional<StringRef>(influence_data.layer_name) :
          std::nullopt,
      (influence_data.flag & GREASE_PENCIL_INFLUENCE_USE_LAYER_PASS_FILTER) ?
          std::make_optional<int>(influence_data.layer_pass) :
          std::nullopt,
      influence_data.flag & GREASE_PENCIL_INFLUENCE_INVERT_LAYER_FILTER,
      influence_data.flag & GREASE_PENCIL_INFLUENCE_INVERT_LAYER_PASS_FILTER,
      memory);
}

static IndexMask get_filtered_stroke_mask(const Object *ob,
                                          const bke::CurvesGeometry &curves,
                                          const Material *material_filter,
                                          const std::optional<int> material_pass_filter,
                                          const bool material_filter_invert,
                                          const bool material_pass_filter_invert,
                                          IndexMaskMemory &memory)
{
  const IndexMask full_mask = curves.curves_range();
  if (!material_filter && !material_pass_filter) {
    return full_mask;
  }

  const int material_filter_index = BKE_object_material_index_get(
      const_cast<Object *>(ob), const_cast<Material *>(material_filter));
  const Vector<int> material_pass_by_index = get_grease_pencil_material_passes(ob);

  bke::AttributeAccessor attributes = curves.attributes();
  VArray<int> stroke_materials =
      attributes.lookup_or_default<int>("material_index", bke::AttrDomain::Curve, 0).varray;

  IndexMask result = IndexMask::from_predicate(
      full_mask, GrainSize(4096), memory, [&](const int64_t stroke_i) {
        const int material_index = stroke_materials.get(stroke_i);
        if (material_filter != nullptr) {
          const bool match = (material_index == material_filter_index);
          if (match == material_filter_invert) {
            return false;
          }
        }
        if (material_pass_filter) {
          const int material_pass = material_pass_by_index[material_index];
          const bool match = (material_pass == material_pass_filter.value());
          if (match == material_pass_filter_invert) {
            return false;
          }
        }
        return true;
      });
  return result;
}

IndexMask get_filtered_stroke_mask(const Object *ob,
                                   const bke::CurvesGeometry &curves,
                                   const GreasePencilModifierInfluenceData &influence_data,
                                   IndexMaskMemory &memory)
{
  return get_filtered_stroke_mask(
      ob,
      curves,
      influence_data.material,
      (influence_data.flag & GREASE_PENCIL_INFLUENCE_USE_MATERIAL_PASS_FILTER) ?
          std::make_optional<int>(influence_data.material_pass) :
          std::nullopt,
      influence_data.flag & GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_FILTER,
      influence_data.flag & GREASE_PENCIL_INFLUENCE_INVERT_MATERIAL_PASS_FILTER,
      memory);
}

VArray<float> get_influence_vertex_weights(const bke::CurvesGeometry &curves,
                                           const GreasePencilModifierInfluenceData &influence_data)
{
  if (influence_data.vertex_group_name[0] == '\0') {
    /* If vertex group is not set, use full weight for all vertices. */
    return VArray<float>::from_single(1.0f, curves.point_num);
  }
  /* Vertex group weights, with zero weight as a fallback. */
  VArray<float> influence_weights = *curves.attributes().lookup_or_default<float>(
      influence_data.vertex_group_name, bke::AttrDomain::Point, 0.0f);

  if (influence_data.flag & GREASE_PENCIL_INFLUENCE_INVERT_VERTEX_GROUP) {
    Array<float> influence_weights_inverted(influence_weights.size());
    threading::parallel_for(
        influence_weights_inverted.index_range(), 8192, [&](const IndexRange range) {
          for (const int i : range) {
            influence_weights_inverted[i] = 1.0f - influence_weights[i];
          }
        });
    return VArray<float>::from_container(influence_weights_inverted);
  }

  return influence_weights;
}

Vector<bke::greasepencil::Drawing *> get_drawings_for_write(GreasePencil &grease_pencil,
                                                            const IndexMask &layer_mask,
                                                            const int frame)
{
  using namespace blender::bke::greasepencil;
  VectorSet<Drawing *> drawings;
  layer_mask.foreach_index([&](const int64_t layer_i) {
    const Layer &layer = grease_pencil.layer(layer_i);
    /* Set of owned drawings, ignore drawing references to other data blocks. */
    if (Drawing *drawing = grease_pencil.get_drawing_at(layer, frame)) {
      drawings.add(drawing);
    }
  });
  return Vector<Drawing *>(drawings.as_span());
}

Vector<LayerDrawingInfo> get_drawing_infos_by_layer(GreasePencil &grease_pencil,
                                                    const IndexMask &layer_mask,
                                                    const int frame)
{
  using namespace blender::bke::greasepencil;
  Set<Drawing *> drawings;
  Vector<LayerDrawingInfo> drawing_infos;
  layer_mask.foreach_index([&](const int64_t layer_i) {
    const Layer &layer = grease_pencil.layer(layer_i);
    Drawing *drawing = grease_pencil.get_drawing_at(layer, frame);
    if (drawing == nullptr) {
      return;
    }

    if (!drawings.contains(drawing)) {
      drawings.add_new(drawing);
      drawing_infos.append({drawing, int(layer_i)});
    }
  });
  return drawing_infos;
}

Vector<FrameDrawingInfo> get_drawing_infos_by_frame(GreasePencil &grease_pencil,
                                                    const IndexMask &layer_mask,
                                                    const int frame)
{
  using namespace blender::bke::greasepencil;
  Set<Drawing *> drawings;
  Vector<FrameDrawingInfo> drawing_infos;
  layer_mask.foreach_index([&](const int64_t layer_i) {
    const Layer &layer = grease_pencil.layer(layer_i);
    const std::optional<int> start_frame = layer.start_frame_at(frame);
    if (!start_frame) {
      return;
    }
    Drawing *drawing = grease_pencil.get_drawing_at(layer, *start_frame);
    if (drawing == nullptr) {
      return;
    }

    if (!drawings.contains(drawing)) {
      drawings.add_new(drawing);
      drawing_infos.append({drawing, *start_frame});
    }
  });
  return drawing_infos;
}

void ensure_no_bezier_curves(Drawing &drawing)
{
  const bke::CurvesGeometry &curves = drawing.strokes();
  IndexMaskMemory memory;
  const IndexMask bezier_selection = curves.indices_for_curve_type(CURVE_TYPE_BEZIER, memory);
  if (bezier_selection.is_empty()) {
    return;
  }
  drawing.strokes_for_write() = geometry::resample_to_evaluated(curves, bezier_selection);
  drawing.tag_topology_changed();
}

}  // namespace blender::modifier::greasepencil
