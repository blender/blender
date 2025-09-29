/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "BKE_attribute.h"

#include "BLT_translation.hh"

#include "DNA_grease_pencil_types.h"
#include "DNA_scene_types.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_api.hh"

const EnumPropertyItem rna_enum_stroke_depth_order_items[] = {
    {0,
     "2D",
     0,
     "2D Layers",
     "Display strokes using Grease Pencil layer order and stroke order to define depth"},
    {GREASE_PENCIL_STROKE_ORDER_3D,
     "3D",
     0,
     "3D Location",
     "Display strokes using real 3D position in 3D space"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BKE_attribute.hh"
#  include "BKE_curves.hh"
#  include "BKE_global.hh"
#  include "BKE_grease_pencil.hh"

#  include "BLI_math_matrix.hh"
#  include "BLI_span.hh"
#  include "BLI_string.h"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

#  include "ED_grease_pencil.hh"

static GreasePencil *rna_grease_pencil(const PointerRNA *ptr)
{
  return reinterpret_cast<GreasePencil *>(ptr->owner_id);
}

static void rna_grease_pencil_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(&rna_grease_pencil(ptr)->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, rna_grease_pencil(ptr));
}

static void rna_grease_pencil_autolock(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  if (grease_pencil->flag & GREASE_PENCIL_AUTOLOCK_LAYERS) {
    grease_pencil->autolock_inactive_layers();
  }
  else {
    for (Layer *layer : grease_pencil->layers_for_write()) {
      layer->set_locked(false);
    }
  }

  rna_grease_pencil_update(nullptr, nullptr, ptr);
}

static void rna_grease_pencil_dependency_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(&rna_grease_pencil(ptr)->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, rna_grease_pencil(ptr));
}

static int rna_Drawing_user_count_get(PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  const GreasePencilDrawing *drawing = static_cast<const GreasePencilDrawing *>(ptr->data);
  return drawing->wrap().user_count();
}

static int rna_GreasePencilDrawing_curve_offset_data_length(PointerRNA *ptr)
{
  const GreasePencilDrawing *drawing = static_cast<GreasePencilDrawing *>(ptr->data);
  return drawing->geometry.curve_num + 1;
}

static void rna_GreasePencilDrawing_curve_offset_data_begin(CollectionPropertyIterator *iter,
                                                            PointerRNA *ptr)
{
  GreasePencilDrawing *drawing = static_cast<GreasePencilDrawing *>(ptr->data);
  rna_iterator_array_begin(iter,
                           ptr,
                           drawing->geometry.wrap().offsets_for_write().data(),
                           sizeof(int),
                           drawing->geometry.curve_num + 1,
                           false,
                           nullptr);
}

static bool rna_GreasePencilDrawing_curve_offset_data_lookup_int(PointerRNA *ptr,
                                                                 int index,
                                                                 PointerRNA *r_ptr)
{
  GreasePencilDrawing *drawing = static_cast<GreasePencilDrawing *>(ptr->data);
  if (index < 0 || index >= drawing->geometry.curve_num + 1) {
    return false;
  }
  rna_pointer_create_with_ancestors(
      *ptr, &RNA_IntAttributeValue, &drawing->geometry.wrap().offsets_for_write()[index], *r_ptr);
  return true;
}

static void rna_GreasePencilLayer_frames_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  Layer &layer = static_cast<GreasePencilLayer *>(ptr->data)->wrap();
  blender::Span<FramesMapKeyT> sorted_keys = layer.sorted_keys();

  rna_iterator_array_begin(iter,
                           ptr,
                           (void *)sorted_keys.data(),
                           sizeof(FramesMapKeyT),
                           sorted_keys.size(),
                           false,
                           nullptr);
}

static PointerRNA rna_GreasePencilLayer_frames_get(CollectionPropertyIterator *iter)
{
  using namespace blender::bke::greasepencil;
  const FramesMapKeyT frame_key = *static_cast<FramesMapKeyT *>(rna_iterator_array_get(iter));
  const Layer &layer = static_cast<GreasePencilLayer *>(iter->parent.data)->wrap();
  const GreasePencilFrame *frame = layer.frames().lookup_ptr(frame_key);
  return RNA_pointer_create_with_parent(
      iter->parent,
      &RNA_GreasePencilFrame,
      static_cast<void *>(const_cast<GreasePencilFrame *>(frame)));
}

static int rna_GreasePencilLayer_frames_length(PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  Layer &layer = static_cast<GreasePencilLayer *>(ptr->data)->wrap();
  return layer.frames().size();
}

static bool rna_GreasePencilLayer_frames_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  using namespace blender::bke::greasepencil;
  Layer &layer = static_cast<GreasePencilLayer *>(ptr->data)->wrap();
  if (index < 0 || index >= layer.sorted_keys().size()) {
    return false;
  }
  const FramesMapKeyT frame_key = layer.sorted_keys()[index];
  const GreasePencilFrame *frame = layer.frames().lookup_ptr(frame_key);
  rna_pointer_create_with_ancestors(*ptr,
                                    &RNA_GreasePencilFrame,
                                    static_cast<void *>(const_cast<GreasePencilFrame *>(frame)),
                                    *r_ptr);
  return true;
}

static std::pair<int, const blender::bke::greasepencil::Layer *> find_layer_of_frame(
    const GreasePencil &grease_pencil, const GreasePencilFrame &find_frame)
{
  using namespace blender::bke::greasepencil;
  for (const Layer *layer : grease_pencil.layers()) {
    for (const auto &[key, frame] : layer->frames().items()) {
      if (&frame == &find_frame) {
        return {int(key), layer};
      }
    }
  }
  return {0, nullptr};
}

static std::pair<int, blender::bke::greasepencil::Layer *> find_layer_of_frame(
    GreasePencil &grease_pencil, const GreasePencilFrame &find_frame)
{
  using namespace blender::bke::greasepencil;
  for (Layer *layer : grease_pencil.layers_for_write()) {
    for (const auto &[key, frame] : layer->frames().items()) {
      if (&frame == &find_frame) {
        return {int(key), layer};
      }
    }
  }
  return {0, nullptr};
}

static PointerRNA rna_Frame_drawing_get(PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  const GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  GreasePencilFrame &frame_to_find = *static_cast<GreasePencilFrame *>(ptr->data);
  if (frame_to_find.is_end()) {
    return PointerRNA_NULL;
  }

  /* RNA doesn't give access to the parented layer object, so we have to iterate over all layers
   * and search for the matching GreasePencilFrame pointer in the frames collection. */
  auto [frame_number, this_layer] = find_layer_of_frame(grease_pencil, frame_to_find);
  if (this_layer == nullptr) {
    return PointerRNA_NULL;
  }

  const Drawing *drawing = grease_pencil.get_drawing_at(*this_layer, frame_number);
  return RNA_pointer_create_with_parent(
      *ptr, &RNA_GreasePencilDrawing, static_cast<void *>(const_cast<Drawing *>(drawing)));
}

static void rna_Frame_drawing_set(PointerRNA *frame_ptr,
                                  const PointerRNA drawing_ptr,
                                  ReportList * /*reports*/)
{
  using namespace blender::bke::greasepencil;
  GreasePencil &grease_pencil = *rna_grease_pencil(frame_ptr);
  GreasePencilFrame &frame_to_find = *static_cast<GreasePencilFrame *>(frame_ptr->data);
  /* It shouldn't be possible for the user to get an PointerRNA to a frame that just marks the end
   * of another frame. */
  BLI_assert(!frame_to_find.is_end());

  /* RNA doesn't give access to the parented layer object, so we have to iterate over all layers
   * and search for the matching GreasePencilFrame pointer in the frames collection. */
  auto [frame_number, this_layer] = find_layer_of_frame(grease_pencil, frame_to_find);
  /* Layer should exist. */
  BLI_assert(this_layer != nullptr);

  Drawing *dst_drawing = grease_pencil.get_drawing_at(*this_layer, frame_number);
  if (dst_drawing == nullptr) {
    return;
  }
  const Drawing *src_drawing = static_cast<const Drawing *>(drawing_ptr.data);
  if (src_drawing == nullptr) {
    /* Clear the drawing. */
    *dst_drawing = {};
  }
  else {
    *dst_drawing = *src_drawing;
  }
}

static int rna_Frame_frame_number_get(PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  const GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  GreasePencilFrame &frame_to_find = *static_cast<GreasePencilFrame *>(ptr->data);

  /* RNA doesn't give access to the parented layer object, so we have to iterate over all layers
   * and search for the matching GreasePencilFrame pointer in the frames collection. */
  auto [frame_number, this_layer] = find_layer_of_frame(grease_pencil, frame_to_find);
  /* Layer should exist. */
  BLI_assert(this_layer != nullptr);
  return frame_number;
}

static void rna_grease_pencil_layer_mask_name_get(PointerRNA *ptr, char *dst)
{
  using namespace blender;
  GreasePencilLayerMask *mask = static_cast<GreasePencilLayerMask *>(ptr->data);
  if (mask->layer_name != nullptr) {
    strcpy(dst, mask->layer_name);
  }
  else {
    dst[0] = '\0';
  }
}

static int rna_grease_pencil_layer_mask_name_length(PointerRNA *ptr)
{
  using namespace blender;
  GreasePencilLayerMask *mask = static_cast<GreasePencilLayerMask *>(ptr->data);
  if (mask->layer_name != nullptr) {
    return strlen(mask->layer_name);
  }
  return 0;
}

static void rna_grease_pencil_layer_mask_name_set(PointerRNA *ptr, const char *value)
{
  using namespace blender;
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  GreasePencilLayerMask *mask = static_cast<GreasePencilLayerMask *>(ptr->data);

  const std::string oldname(mask->layer_name);
  if (bke::greasepencil::TreeNode *node = grease_pencil->find_node_by_name(oldname)) {
    grease_pencil->rename_node(*G_MAIN, *node, value);
  }
}

static int rna_grease_pencil_active_mask_index_get(PointerRNA *ptr)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);
  return layer->active_mask_index;
}

static void rna_grease_pencil_active_mask_index_set(PointerRNA *ptr, int value)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);
  layer->active_mask_index = value;
}

static void rna_grease_pencil_active_mask_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);
  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&layer->masks) - 1);
}

static void tree_node_name_get(blender::bke::greasepencil::TreeNode &node, char *dst)
{
  if (!node.name().is_empty()) {
    strcpy(dst, node.name().c_str());
  }
  else {
    dst[0] = '\0';
  }
}

static int tree_node_name_length(blender::bke::greasepencil::TreeNode &node)
{
  if (!node.name().is_empty()) {
    return node.name().size();
  }
  return 0;
}

static std::optional<std::string> tree_node_name_path(blender::bke::greasepencil::TreeNode &node,
                                                      const char *prefix)
{
  using namespace blender::bke::greasepencil;
  BLI_assert(!node.name().is_empty());
  const size_t name_length = node.name().size();
  std::string name_esc(name_length * 2, '\0');
  BLI_str_escape(name_esc.data(), node.name().c_str(), name_length * 2);
  return fmt::format("{}[\"{}\"]", prefix, name_esc.c_str());
}

static StructRNA *rna_GreasePencilTreeNode_refine(PointerRNA *ptr)
{
  GreasePencilLayerTreeNode *node = static_cast<GreasePencilLayerTreeNode *>(ptr->data);
  switch (node->type) {
    case GP_LAYER_TREE_LEAF:
      return &RNA_GreasePencilLayer;
    case GP_LAYER_TREE_GROUP:
      return &RNA_GreasePencilLayerGroup;
    default:
      BLI_assert_unreachable();
  }
  return nullptr;
}

static void rna_GreasePencilTreeNode_name_get(PointerRNA *ptr, char *value)
{
  GreasePencilLayerTreeNode *node = static_cast<GreasePencilLayerTreeNode *>(ptr->data);
  tree_node_name_get(node->wrap(), value);
}

static int rna_GreasePencilTreeNode_name_length(PointerRNA *ptr)
{
  GreasePencilLayerTreeNode *node = static_cast<GreasePencilLayerTreeNode *>(ptr->data);
  return tree_node_name_length(node->wrap());
}

static void rna_GreasePencilTreeNode_name_set(PointerRNA *ptr, const char *value)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  GreasePencilLayerTreeNode *node = static_cast<GreasePencilLayerTreeNode *>(ptr->data);

  grease_pencil->rename_node(*G_MAIN, node->wrap(), value);
}

static PointerRNA rna_GreasePencilTreeNode_parent_layer_group_get(PointerRNA *ptr)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  GreasePencilLayerTreeNode *node = static_cast<GreasePencilLayerTreeNode *>(ptr->data);
  /* Return 'None' when node is in the root group. This group is not meant to be seen. */
  if (node->parent == nullptr || node->parent == grease_pencil->root_group_ptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_with_parent(
      *ptr, &RNA_GreasePencilLayerGroup, static_cast<void *>(node->parent));
}

static void rna_iterator_grease_pencil_layers_begin(CollectionPropertyIterator *iter,
                                                    PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  const blender::Span<const Layer *> layers = grease_pencil->layers();

  iter->internal.count.item = 0;
  iter->valid = !layers.is_empty();
}

static void rna_iterator_grease_pencil_layers_next(CollectionPropertyIterator *iter)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = static_cast<GreasePencil *>(iter->parent.data);
  const blender::Span<const Layer *> layers = grease_pencil->layers();

  iter->internal.count.item++;
  iter->valid = layers.index_range().contains(iter->internal.count.item);
}

static PointerRNA rna_iterator_grease_pencil_layers_get(CollectionPropertyIterator *iter)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = static_cast<GreasePencil *>(iter->parent.data);
  blender::Span<Layer *> layers = grease_pencil->layers_for_write();

  return RNA_pointer_create_discrete(iter->parent.owner_id,
                                     &RNA_GreasePencilLayer,
                                     static_cast<void *>(layers[iter->internal.count.item]));
}

static int rna_iterator_grease_pencil_layers_length(PointerRNA *ptr)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  return grease_pencil->layers().size();
}

static std::optional<std::string> rna_GreasePencilLayer_path(const PointerRNA *ptr)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);
  return tree_node_name_path(layer->wrap().as_node(), "layers");
}

static int rna_GreasePencilLayer_pass_index_get(PointerRNA *ptr)
{
  using namespace blender;
  const GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  const bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  const int layer_idx = *grease_pencil.get_layer_index(layer);

  const VArray layer_passes = *grease_pencil.attributes().lookup_or_default<int>(
      "pass_index", bke::AttrDomain::Layer, 0);
  return layer_passes[layer_idx];
}

static void rna_GreasePencilLayer_pass_index_set(PointerRNA *ptr, int value)
{
  using namespace blender;
  GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  const bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  const int layer_idx = *grease_pencil.get_layer_index(layer);

  if (bke::SpanAttributeWriter<int> layer_passes =
          grease_pencil.attributes_for_write().lookup_or_add_for_write_span<int>(
              "pass_index", bke::AttrDomain::Layer))
  {
    layer_passes.span[layer_idx] = std::max(0, value);
    layer_passes.finish();
  }
}

static void rna_GreasePencilLayer_parent_set(PointerRNA *ptr,
                                             PointerRNA value,
                                             ReportList * /*reports*/)
{
  using namespace blender;
  bke::greasepencil::Layer &layer = static_cast<GreasePencilLayer *>(ptr->data)->wrap();
  Object *parent = static_cast<Object *>(value.data);

  ed::greasepencil::grease_pencil_layer_parent_set(layer, parent, layer.parent_bone_name(), false);
}

static void rna_GreasePencilLayer_bone_set(PointerRNA *ptr, const char *value)
{
  using namespace blender;
  bke::greasepencil::Layer &layer = static_cast<GreasePencilLayer *>(ptr->data)->wrap();

  ed::greasepencil::grease_pencil_layer_parent_set(layer, layer.parent, value, false);
}

static void rna_GreasePencilLayer_tint_color_get(PointerRNA *ptr, float *values)
{
  using namespace blender;
  const GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  const bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  const int layer_idx = *grease_pencil.get_layer_index(layer);

  const VArray tint_colors = *grease_pencil.attributes().lookup_or_default<ColorGeometry4f>(
      "tint_color", bke::AttrDomain::Layer, ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
  copy_v3_v3(values, tint_colors[layer_idx]);
}

static void rna_GreasePencilLayer_tint_color_set(PointerRNA *ptr, const float *values)
{
  using namespace blender;
  GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  const bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  const int layer_idx = *grease_pencil.get_layer_index(layer);

  if (bke::SpanAttributeWriter<ColorGeometry4f> tint_colors =
          grease_pencil.attributes_for_write().lookup_or_add_for_write_span<ColorGeometry4f>(
              "tint_color",
              bke::AttrDomain::Layer,
              bke::AttributeInitVArray(VArray<ColorGeometry4f>::from_single(
                  ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f), grease_pencil.layers().size()))))
  {
    copy_v3_v3(tint_colors.span[layer_idx], values);
    tint_colors.finish();
  }
}

static float rna_GreasePencilLayer_tint_factor_get(PointerRNA *ptr)
{
  using namespace blender;
  const GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  const bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  const int layer_idx = *grease_pencil.get_layer_index(layer);

  const VArray tint_colors = *grease_pencil.attributes().lookup_or_default<ColorGeometry4f>(
      "tint_color", bke::AttrDomain::Layer, ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
  return tint_colors[layer_idx][3];
}

static void rna_GreasePencilLayer_tint_factor_set(PointerRNA *ptr, const float value)
{
  using namespace blender;
  GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  const bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  const int layer_idx = *grease_pencil.get_layer_index(layer);

  if (bke::SpanAttributeWriter<ColorGeometry4f> tint_colors =
          grease_pencil.attributes_for_write().lookup_or_add_for_write_span<ColorGeometry4f>(
              "tint_color",
              bke::AttrDomain::Layer,
              bke::AttributeInitVArray(VArray<ColorGeometry4f>::from_single(
                  ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f), grease_pencil.layers().size()))))
  {
    tint_colors.span[layer_idx][3] = value;
    tint_colors.finish();
  }
}

static float rna_GreasePencilLayer_radius_offset_get(PointerRNA *ptr)
{
  using namespace blender;
  const GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  const bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  const int layer_idx = *grease_pencil.get_layer_index(layer);

  const VArray radius_offsets = *grease_pencil.attributes().lookup_or_default<float>(
      "radius_offset", bke::AttrDomain::Layer, 0.0f);
  return radius_offsets[layer_idx];
}

static void rna_GreasePencilLayer_radius_offset_set(PointerRNA *ptr, const float value)
{
  using namespace blender;
  GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  const bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  const int layer_idx = *grease_pencil.get_layer_index(layer);

  if (bke::SpanAttributeWriter<float> radius_offsets =
          grease_pencil.attributes_for_write().lookup_or_add_for_write_span<float>(
              "radius_offset",
              bke::AttrDomain::Layer,
              bke::AttributeInitVArray(
                  VArray<float>::from_single(0.0f, grease_pencil.layers().size()))))
  {
    radius_offsets.span[layer_idx] = value;
    radius_offsets.finish();
  }
}

static void rna_GreasePencilLayer_matrix_local_get(PointerRNA *ptr, float *values)
{
  const blender::bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  std::copy_n(layer.local_transform().base_ptr(), 16, values);
}

static void rna_GreasePencilLayer_matrix_parent_inverse_get(PointerRNA *ptr, float *values)
{
  const blender::bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  std::copy_n(layer.parent_inverse().base_ptr(), 16, values);
}

static PointerRNA rna_GreasePencil_active_layer_get(PointerRNA *ptr)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  if (grease_pencil->has_active_layer()) {
    return RNA_pointer_create_with_parent(
        *ptr, &RNA_GreasePencilLayer, static_cast<void *>(grease_pencil->get_active_layer()));
  }
  return PointerRNA_NULL;
}

static void rna_GreasePencil_active_layer_set(PointerRNA *ptr,
                                              PointerRNA value,
                                              ReportList * /*reports*/)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  grease_pencil->set_active_layer(static_cast<blender::bke::greasepencil::Layer *>(value.data));
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED | NA_SELECTED, grease_pencil);
}

static PointerRNA rna_GreasePencil_active_group_get(PointerRNA *ptr)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  if (grease_pencil->has_active_group()) {
    return RNA_pointer_create_with_parent(
        *ptr, &RNA_GreasePencilLayerGroup, static_cast<void *>(grease_pencil->get_active_group()));
  }
  return PointerRNA_NULL;
}

static void rna_GreasePencil_active_group_set(PointerRNA *ptr,
                                              PointerRNA value,
                                              ReportList * /*reports*/)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  TreeNode *node = static_cast<TreeNode *>(value.data);
  if (node->is_group()) {
    grease_pencil->set_active_node(node);
    WM_main_add_notifier(NC_GPENCIL | NA_EDITED | NA_SELECTED, grease_pencil);
  }
}

static std::optional<std::string> rna_GreasePencilLayerGroup_path(const PointerRNA *ptr)
{
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);
  return tree_node_name_path(group->wrap().as_node(), "layer_groups");
}

static void rna_GreasePencilLayerGroup_is_expanded_set(PointerRNA *ptr, const bool value)
{
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);
  group->wrap().set_expanded(value);
}

static void rna_iterator_grease_pencil_layer_groups_begin(CollectionPropertyIterator *iter,
                                                          PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  const blender::Span<const LayerGroup *> groups = grease_pencil->layer_groups();

  iter->internal.count.item = 0;
  iter->valid = !groups.is_empty();
}

static void rna_iterator_grease_pencil_layer_groups_next(CollectionPropertyIterator *iter)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = static_cast<GreasePencil *>(iter->parent.data);
  const blender::Span<const LayerGroup *> groups = grease_pencil->layer_groups();

  iter->internal.count.item++;
  iter->valid = groups.index_range().contains(iter->internal.count.item);
}

static PointerRNA rna_iterator_grease_pencil_layer_groups_get(CollectionPropertyIterator *iter)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = static_cast<GreasePencil *>(iter->parent.data);
  blender::Span<LayerGroup *> groups = grease_pencil->layer_groups_for_write();

  return RNA_pointer_create_discrete(iter->parent.owner_id,
                                     &RNA_GreasePencilLayerGroup,
                                     static_cast<void *>(groups[iter->internal.count.item]));
}

static int rna_iterator_grease_pencil_layer_groups_length(PointerRNA *ptr)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  return grease_pencil->layer_groups().size();
}

static int rna_group_color_tag_get(PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);
  return group->color_tag;
}

static void rna_group_color_tag_set(PointerRNA *ptr, int value)
{
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);
  group->color_tag = value;
  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);
}

#else

static void rna_def_grease_pencil_drawing(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem rna_enum_drawing_type_items[] = {
      {GP_DRAWING, "DRAWING", 0, "Drawing", ""},
      {GP_DRAWING_REFERENCE, "REFERENCE", 0, "Reference", ""},
      {0, nullptr, 0, nullptr, nullptr}};

  srna = RNA_def_struct(brna, "GreasePencilDrawing", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencilDrawing");
  RNA_def_struct_ui_text(srna, "Grease Pencil Drawing", "A Grease Pencil drawing");

  /* Type. */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "base.type");
  RNA_def_property_enum_items(prop, rna_enum_drawing_type_items);
  RNA_def_parameter_clear_flags(prop, PROP_EDITABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "Type", "Drawing type");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* User Count. */
  prop = RNA_def_property(srna, "user_count", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop, "rna_Drawing_user_count_get", nullptr, nullptr);
  RNA_def_parameter_clear_flags(prop, PROP_EDITABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "User Count", "The number of keyframes this drawing is used by");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Curve offsets. */
  prop = RNA_def_property(srna, "curve_offsets", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "IntAttributeValue");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_GreasePencilDrawing_curve_offset_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_GreasePencilDrawing_curve_offset_data_length",
                                    "rna_GreasePencilDrawing_curve_offset_data_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_parameter_clear_flags(prop, PROP_EDITABLE, ParameterFlag(0));
  RNA_def_property_ui_text(
      prop, "Curve Offsets", "Offset indices of the first point of each curve");
  RNA_def_property_update(prop, 0, "rna_grease_pencil_update");

  RNA_api_grease_pencil_drawing(srna);

  /* Attributes. */
  rna_def_attributes_common(srna, AttributeOwnerType::GreasePencilDrawing);
}

static void rna_def_grease_pencil_frame(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem rna_enum_keyframe_type_items[] = {
      {BEZT_KEYTYPE_KEYFRAME,
       "KEYFRAME",
       ICON_KEYTYPE_KEYFRAME_VEC,
       "Keyframe",
       "Normal keyframe, e.g. for key poses"},
      {BEZT_KEYTYPE_BREAKDOWN,
       "BREAKDOWN",
       ICON_KEYTYPE_BREAKDOWN_VEC,
       "Breakdown",
       "A breakdown pose, e.g. for transitions between key poses"},
      {BEZT_KEYTYPE_MOVEHOLD,
       "MOVING_HOLD",
       ICON_KEYTYPE_MOVING_HOLD_VEC,
       "Moving Hold",
       "A keyframe that is part of a moving hold"},
      {BEZT_KEYTYPE_EXTREME,
       "EXTREME",
       ICON_KEYTYPE_EXTREME_VEC,
       "Extreme",
       "An 'extreme' pose, or some other purpose as needed"},
      {BEZT_KEYTYPE_JITTER,
       "JITTER",
       ICON_KEYTYPE_JITTER_VEC,
       "Jitter",
       "A filler or baked keyframe for keying on ones, or some other purpose as needed"},
      {BEZT_KEYTYPE_GENERATED,
       "GENERATED",
       ICON_KEYTYPE_GENERATED_VEC,
       "Generated",
       "A key generated automatically by a tool, not manually created"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "GreasePencilFrame", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencilFrame");
  RNA_def_struct_ui_text(srna, "Grease Pencil Frame", "A Grease Pencil keyframe");

  /* Drawing. */
  prop = RNA_def_property(srna, "drawing", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "GreasePencilDrawing");
  RNA_def_property_pointer_funcs(
      prop, "rna_Frame_drawing_get", "rna_Frame_drawing_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Drawing", "A Grease Pencil drawing");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Frame number. */
  prop = RNA_def_property(srna, "frame_number", PROP_INT, PROP_NONE);
  /* TODO: Make property editable, ensure frame number isn't already in use. */
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_Frame_frame_number_get", nullptr, nullptr);
  RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "Frame Number", "The frame number in the scene");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Selection status. */
  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_FRAME_SELECTED);
  RNA_def_property_ui_text(prop, "Select", "Frame Selection in the Dope Sheet");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Keyframe type. */
  prop = RNA_def_property(srna, "keyframe_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_enum_items(prop, rna_enum_keyframe_type_items);
  RNA_def_property_ui_text(prop, "Keyframe Type", "Type of keyframe");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");
}

static void rna_def_grease_pencil_frames(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  RNA_def_property_srna(cprop, "GreasePencilFrames");
  srna = RNA_def_struct(brna, "GreasePencilFrames", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencilLayer");
  RNA_def_struct_ui_text(srna, "Grease Pencil Frames", "Collection of Grease Pencil frames");

  RNA_api_grease_pencil_frames(srna);
}

static void rna_def_grease_pencil_layer_mask(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GreasePencilLayerMask", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencilLayerMask");
  RNA_def_struct_ui_text(srna, "Grease Pencil Masking Layers", "List of Mask Layers");
  // RNA_def_struct_path_func(srna, "rna_GreasePencilLayerMask_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Layer", "Mask layer name");
  RNA_def_property_string_sdna(prop, nullptr, "layer_name");
  RNA_def_property_string_funcs(prop,
                                "rna_grease_pencil_layer_mask_name_get",
                                "rna_grease_pencil_layer_mask_name_length",
                                "rna_grease_pencil_layer_mask_name_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_RENAME, nullptr);

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_MASK_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide", "Set mask Visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_MASK_INVERT);
  RNA_def_property_ui_icon(prop, ICON_SELECT_INTERSECT, 1);
  RNA_def_property_ui_text(prop, "Invert", "Invert mask");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");
}

static void rna_def_grease_pencil_layer_masks(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "GreasePencilLayerMasks");
  srna = RNA_def_struct(brna, "GreasePencilLayerMasks", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencilLayer");
  RNA_def_struct_ui_text(
      srna, "Grease Pencil Mask Layers", "Collection of Grease Pencil masking layers");

  prop = RNA_def_property(srna, "active_mask_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_grease_pencil_active_mask_index_get",
                             "rna_grease_pencil_active_mask_index_set",
                             "rna_grease_pencil_active_mask_index_range");
  RNA_def_property_ui_text(prop, "Active Layer Mask Index", "Active index in layer mask array");
}

static void rna_def_grease_pencil_tree_node(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GreasePencilTreeNode", nullptr);
  RNA_def_struct_ui_text(
      srna, "Tree Node", "Grease Pencil node in the layer tree. Either a layer or a group");
  RNA_def_struct_sdna(srna, "GreasePencilLayerTreeNode");
  RNA_def_struct_refine_func(srna, "rna_GreasePencilTreeNode_refine");

  /* Name. */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "The name of the tree node");
  RNA_def_property_string_funcs(prop,
                                "rna_GreasePencilTreeNode_name_get",
                                "rna_GreasePencilTreeNode_name_length",
                                "rna_GreasePencilTreeNode_name_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_RENAME, "rna_grease_pencil_update");

  /* Visibility. */
  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_TREE_NODE_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide", "Set tree node visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Lock. */
  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_TREE_NODE_LOCKED);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_ui_text(prop, "Locked", "Protect tree node from editing");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Select. */
  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_TREE_NODE_SELECT);
  RNA_def_property_ui_text(prop, "Select", "Tree node is selected");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Onion Skinning. */
  prop = RNA_def_property(srna, "use_onion_skinning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_icon(prop, ICON_ONIONSKIN_OFF, 1);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "flag", GP_LAYER_TREE_NODE_HIDE_ONION_SKINNING);
  RNA_def_property_ui_text(
      prop, "Onion Skinning", "Display onion skins before and after the current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Use Masks. */
  prop = RNA_def_property(srna, "use_masks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_icon(prop, ICON_CLIPUV_HLT, -1);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", GP_LAYER_TREE_NODE_HIDE_MASKS);
  RNA_def_property_ui_text(
      prop,
      "Use Masks",
      "The visibility of drawings in this tree node is affected by the layers in the masks list");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Channel color. */
  prop = RNA_def_property(srna, "channel_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Channel Color", "Color of the channel in the dope sheet");
  RNA_def_property_update(prop, NC_GPENCIL | NA_EDITED, nullptr);

  /* Next tree node. */
  prop = RNA_def_property(srna, "next_node", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "next");
  RNA_def_property_struct_type(prop, "GreasePencilTreeNode");
  RNA_def_property_ui_text(prop, "Next Node", "The layer tree node after (i.e. above) this one");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);

  /* Previous tree node. */
  prop = RNA_def_property(srna, "prev_node", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "prev");
  RNA_def_property_struct_type(prop, "GreasePencilTreeNode");
  RNA_def_property_ui_text(
      prop, "Previous Node", "The layer tree node before (i.e. below) this one");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);

  /* Parent group. */
  prop = RNA_def_property(srna, "parent_group", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "GreasePencilLayerGroup");
  RNA_def_property_pointer_funcs(
      prop, "rna_GreasePencilTreeNode_parent_layer_group_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Parent Layer Group", "The parent group of this layer tree node");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
}

static void rna_def_grease_pencil_layer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const float scale_defaults[3] = {1.0f, 1.0f, 1.0f};

  static const EnumPropertyItem rna_enum_layer_blend_modes_items[] = {
      {GP_LAYER_BLEND_NONE, "REGULAR", 0, "Regular", ""},
      {GP_LAYER_BLEND_HARDLIGHT, "HARDLIGHT", 0, "Hard Light", ""},
      {GP_LAYER_BLEND_ADD, "ADD", 0, "Add", ""},
      {GP_LAYER_BLEND_SUBTRACT, "SUBTRACT", 0, "Subtract", ""},
      {GP_LAYER_BLEND_MULTIPLY, "MULTIPLY", 0, "Multiply", ""},
      {GP_LAYER_BLEND_DIVIDE, "DIVIDE", 0, "Divide", ""},
      {0, nullptr, 0, nullptr, nullptr}};

  srna = RNA_def_struct(brna, "GreasePencilLayer", "GreasePencilTreeNode");
  RNA_def_struct_sdna(srna, "GreasePencilLayer");
  RNA_def_struct_ui_text(srna, "Grease Pencil Layer", "Collection of related drawings");
  RNA_def_struct_path_func(srna, "rna_GreasePencilLayer_path");

  /* Frames. */
  prop = RNA_def_property(srna, "frames", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "GreasePencilFrame");
  RNA_def_property_ui_text(prop, "Frames", "Grease Pencil frames");
  RNA_def_property_collection_funcs(prop,
                                    "rna_GreasePencilLayer_frames_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_GreasePencilLayer_frames_get",
                                    "rna_GreasePencilLayer_frames_length",
                                    "rna_GreasePencilLayer_frames_lookup_int",
                                    nullptr,
                                    nullptr);
  rna_def_grease_pencil_frames(brna, prop);

  /* Mask Layers */
  prop = RNA_def_property(srna, "mask_layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "masks", nullptr);
  RNA_def_property_struct_type(prop, "GreasePencilLayerMask");
  RNA_def_property_ui_text(prop, "Masks", "List of Masking Layers");
  rna_def_grease_pencil_layer_masks(brna, prop);

  /* Lock Frame. */
  prop = RNA_def_property(srna, "lock_frame", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_MUTE);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Frame Locked", "Lock current frame displayed by layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Opacity */
  prop = RNA_def_property(srna, "opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, "GreasePencilLayer", "opacity");
  RNA_def_property_ui_text(prop, "Opacity", "Layer Opacity");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Tint Color. */
  prop = RNA_def_property(srna, "tint_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop,
                               "rna_GreasePencilLayer_tint_color_get",
                               "rna_GreasePencilLayer_tint_color_set",
                               nullptr);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Tint Color", "Color for tinting stroke colors");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Tint Factor. */
  prop = RNA_def_property(srna, "tint_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_funcs(prop,
                               "rna_GreasePencilLayer_tint_factor_get",
                               "rna_GreasePencilLayer_tint_factor_set",
                               nullptr);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Tint Factor", "Factor of tinting color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Radius Offset. */
  prop = RNA_def_property(srna, "radius_offset", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_funcs(prop,
                               "rna_GreasePencilLayer_radius_offset_get",
                               "rna_GreasePencilLayer_radius_offset_set",
                               nullptr);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop, "Radius Offset", "Radius change to apply to current strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "use_lights", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_USE_LIGHTS);
  RNA_def_property_ui_text(
      prop, "Use Lights", "Enable the use of lights on stroke and fill materials");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* pass index for compositing and modifiers */
  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Pass Index", "Index number for the \"Layer Index\" pass");
  RNA_def_property_int_funcs(prop,
                             "rna_GreasePencilLayer_pass_index_get",
                             "rna_GreasePencilLayer_pass_index_set",
                             nullptr);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_GreasePencilLayer_parent_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Parent", "Parent object");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_dependency_update");

  prop = RNA_def_property(srna, "parent_bone", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "parsubstr");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_GreasePencilLayer_bone_set");
  RNA_def_property_ui_text(
      prop,
      "Parent Bone",
      "Name of parent bone. Only used when the parent object is an armature.");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_dependency_update");

  prop = RNA_def_property(srna, "translation", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_sdna(prop, nullptr, "translation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop, "Translation", "Translation of the layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_EULER);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_sdna(prop, nullptr, "rotation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop, "Rotation", "Euler rotation of the layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_sdna(prop, nullptr, "scale");
  RNA_def_property_float_array_default(prop, scale_defaults);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 3);
  RNA_def_property_ui_text(prop, "Scale", "Scale of the layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "viewlayer_render", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "viewlayername");
  RNA_def_property_ui_text(
      prop,
      "ViewLayer",
      "Only include Layer in this View Layer render output (leave blank to include always)");

  prop = RNA_def_property(srna, "use_viewlayer_masks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_DISABLE_MASKS_IN_VIEWLAYER);
  RNA_def_property_ui_text(
      prop, "Use Masks in Render", "Include the mask layers when rendering the view-layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "blend_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "blend_mode");
  RNA_def_property_enum_items(prop, rna_enum_layer_blend_modes_items);
  RNA_def_property_ui_text(prop, "Blend Mode", "Blend mode");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "ignore_locked_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_IGNORE_LOCKED_MATERIALS);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Ignore Material Locking", "Allow editing strokes even if they use locked materials");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);
  /* Local transformation matrix. */
  prop = RNA_def_property(srna, "matrix_local", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Local Matrix", "Local transformation matrix of the layer");
  RNA_def_property_float_funcs(prop, "rna_GreasePencilLayer_matrix_local_get", nullptr, nullptr);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Inverse transform of layer's parent. */
  prop = RNA_def_property(srna, "matrix_parent_inverse", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Inverse Parent Matrix", "Inverse of layer's parent transformation matrix");
  RNA_def_property_float_funcs(
      prop, "rna_GreasePencilLayer_matrix_parent_inverse_get", nullptr, nullptr);

  RNA_api_grease_pencil_layer(srna);
}

static void rna_def_grease_pencil_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "GreasePencilv3Layers");
  srna = RNA_def_struct(brna, "GreasePencilv3Layers", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencil");
  RNA_def_struct_ui_text(srna, "Grease Pencil Layers", "Collection of Grease Pencil layers");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "GreasePencilLayer");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_GreasePencil_active_layer_get",
                                 "rna_GreasePencil_active_layer_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Layer", "Active Grease Pencil layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  RNA_api_grease_pencil_layers(srna);
}

const EnumPropertyItem enum_layergroup_color_items[] = {
    {LAYERGROUP_COLOR_NONE, "NONE", ICON_X, "Reset color tag", ""},
    {LAYERGROUP_COLOR_01, "COLOR1", ICON_LAYERGROUP_COLOR_01, "Color tag 1", ""},
    {LAYERGROUP_COLOR_02, "COLOR2", ICON_LAYERGROUP_COLOR_02, "Color tag 2", ""},
    {LAYERGROUP_COLOR_03, "COLOR3", ICON_LAYERGROUP_COLOR_03, "Color tag 3", ""},
    {LAYERGROUP_COLOR_04, "COLOR4", ICON_LAYERGROUP_COLOR_04, "Color tag 4", ""},
    {LAYERGROUP_COLOR_05, "COLOR5", ICON_LAYERGROUP_COLOR_05, "Color tag 5", ""},
    {LAYERGROUP_COLOR_06, "COLOR6", ICON_LAYERGROUP_COLOR_06, "Color tag 6", ""},
    {LAYERGROUP_COLOR_07, "COLOR7", ICON_LAYERGROUP_COLOR_07, "Color tag 7", ""},
    {LAYERGROUP_COLOR_08, "COLOR8", ICON_LAYERGROUP_COLOR_08, "Color tag 8", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rna_def_grease_pencil_layer_group(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GreasePencilLayerGroup", "GreasePencilTreeNode");
  RNA_def_struct_sdna(srna, "GreasePencilLayerTreeGroup");
  RNA_def_struct_ui_text(srna, "Grease Pencil Layer Group", "Group of Grease Pencil layers");
  RNA_def_struct_path_func(srna, "rna_GreasePencilLayerGroup_path");

  /* Expanded */
  prop = RNA_def_property(srna, "is_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_EXPANDED);
  RNA_def_property_ui_text(prop, "Expanded", "The layer group is expanded in the UI");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_GreasePencilLayerGroup_is_expanded_set");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Color tag. */
  prop = RNA_def_property(srna, "color_tag", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop, "rna_group_color_tag_get", "rna_group_color_tag_set", nullptr);
  RNA_def_property_enum_items(prop, enum_layergroup_color_items);
}

static void rna_def_grease_pencil_layer_groups(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "GreasePencilv3LayerGroup");
  srna = RNA_def_struct(brna, "GreasePencilv3LayerGroup", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencil");
  RNA_def_struct_ui_text(srna, "Grease Pencil Group", "Collection of Grease Pencil layers");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "GreasePencilLayerGroup");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_GreasePencil_active_group_get",
                                 "rna_GreasePencil_active_group_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Layer Group", "Active Grease Pencil layer group");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  RNA_api_grease_pencil_layer_groups(srna);
}

static void rna_def_grease_pencil_onion_skinning(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem prop_enum_onion_modes_items[] = {
      {GP_ONION_SKINNING_MODE_ABSOLUTE,
       "ABSOLUTE",
       0,
       "Frames",
       "Frames in absolute range of the scene frame"},
      {GP_ONION_SKINNING_MODE_RELATIVE,
       "RELATIVE",
       0,
       "Keyframes",
       "Frames in relative range of the Grease Pencil keyframes"},
      {GP_ONION_SKINNING_MODE_SELECTED, "SELECTED", 0, "Selected", "Only selected keyframes"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_enum_onion_keyframe_type_items[] = {
      {GREASE_PENCIL_ONION_SKINNING_FILTER_ALL, "ALL", 0, "All", "Include all Keyframe types"},
      {GP_ONION_SKINNING_FILTER_KEYTYPE_KEYFRAME,
       "KEYFRAME",
       ICON_KEYTYPE_KEYFRAME_VEC,
       "Keyframe",
       "Normal keyframe, e.g. for key poses"},
      {GP_ONION_SKINNING_FILTER_KEYTYPE_BREAKDOWN,
       "BREAKDOWN",
       ICON_KEYTYPE_BREAKDOWN_VEC,
       "Breakdown",
       "A breakdown pose, e.g. for transitions between key poses"},
      {GP_ONION_SKINNING_FILTER_KEYTYPE_MOVEHOLD,
       "MOVING_HOLD",
       ICON_KEYTYPE_MOVING_HOLD_VEC,
       "Moving Hold",
       "A keyframe that is part of a moving hold"},
      {GP_ONION_SKINNING_FILTER_KEYTYPE_EXTREME,
       "EXTREME",
       ICON_KEYTYPE_EXTREME_VEC,
       "Extreme",
       "An 'extreme' pose, or some other purpose as needed"},
      {GP_ONION_SKINNING_FILTER_KEYTYPE_JITTER,
       "JITTER",
       ICON_KEYTYPE_JITTER_VEC,
       "Jitter",
       "A filler or baked keyframe for keying on ones, or some other purpose as needed"},
      {BEZT_KEYTYPE_GENERATED,
       "GENERATED",
       ICON_KEYTYPE_GENERATED_VEC,
       "Generated",
       "A key generated automatically by a tool, not manually created"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "ghost_before_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "onion_skinning_settings.num_frames_before");
  RNA_def_property_range(prop, 0, 120);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop,
                           "Frames Before",
                           "Maximum number of frames to show before current frame "
                           "(0 = don't show any frames before current)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "ghost_after_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "onion_skinning_settings.num_frames_after");
  RNA_def_property_range(prop, 0, 120);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop,
                           "Frames After",
                           "Maximum number of frames to show after current frame "
                           "(0 = don't show any frames after current)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "use_ghost_custom_colors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "onion_skinning_settings.flag", GP_ONION_SKINNING_USE_CUSTOM_COLORS);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "Use Custom Ghost Colors", "Use custom colors for ghost frames");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "before_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "onion_skinning_settings.color_before");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "Before Color", "Base color for ghosts before the active frame");
  RNA_def_property_update(prop,
                          NC_SCREEN | NC_SCENE | ND_TOOLSETTINGS | ND_DATA | NC_GPENCIL,
                          "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "after_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "onion_skinning_settings.color_after");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "After Color", "Base color for ghosts after the active frame");
  RNA_def_property_update(prop,
                          NC_SCREEN | NC_SCENE | ND_TOOLSETTINGS | ND_DATA | NC_GPENCIL,
                          "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "onion_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "onion_skinning_settings.mode");
  RNA_def_property_enum_items(prop, prop_enum_onion_modes_items);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "Mode", "Mode to display frames");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "onion_keyframe_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "onion_skinning_settings.filter");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_enum_items(prop, prop_enum_onion_keyframe_type_items);
  RNA_def_property_ui_text(prop, "Filter by Type", "Type of keyframe (for filtering)");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "use_onion_fade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "onion_skinning_settings.flag", GP_ONION_SKINNING_USE_FADE);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(
      prop, "Fade", "Display onion keyframes with a fade in color transparency");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "use_onion_loop", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "onion_skinning_settings.flag", GP_ONION_SKINNING_SHOW_LOOP);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(
      prop, "Show Start Frame", "Display onion keyframes for looping animations");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "onion_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "onion_skinning_settings.opacity");
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "Onion Opacity", "Change fade opacity of displayed onion frames");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");
}

static void rna_def_grease_pencil_data(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GreasePencil", "ID");
  RNA_def_struct_sdna(srna, "GreasePencil");
  RNA_def_struct_ui_text(srna, "Grease Pencil", "Grease Pencil data-block");
  RNA_def_struct_ui_icon(srna, ICON_OUTLINER_DATA_GREASEPENCIL);

  /* attributes */
  rna_def_attributes_common(srna, AttributeOwnerType::GreasePencil);

  /* Animation Data */
  rna_def_animdata_common(srna);

  /* Materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "material_array", "material_array_num");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.cc */
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_IDMaterials_assign_int");

  /* Layers */
  prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "GreasePencilLayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_grease_pencil_layers_begin",
                                    "rna_iterator_grease_pencil_layers_next",
                                    nullptr,
                                    "rna_iterator_grease_pencil_layers_get",
                                    "rna_iterator_grease_pencil_layers_length",
                                    nullptr, /* TODO */
                                    nullptr, /* TODO */
                                    nullptr);
  RNA_def_property_ui_text(prop, "Layers", "Grease Pencil layers");
  rna_def_grease_pencil_layers(brna, prop);

  /* Layer Groups */
  prop = RNA_def_property(srna, "layer_groups", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "GreasePencilLayerGroup");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_grease_pencil_layer_groups_begin",
                                    "rna_iterator_grease_pencil_layer_groups_next",
                                    nullptr,
                                    "rna_iterator_grease_pencil_layer_groups_get",
                                    "rna_iterator_grease_pencil_layer_groups_length",
                                    nullptr, /* TODO */
                                    nullptr, /* TODO */
                                    nullptr);
  RNA_def_property_ui_text(prop, "Layer Groups", "Grease Pencil layer groups");
  rna_def_grease_pencil_layer_groups(brna, prop);

  prop = RNA_def_property(srna, "use_autolock_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GREASE_PENCIL_AUTOLOCK_LAYERS);
  RNA_def_property_ui_text(
      prop,
      "Auto-Lock Layers",
      "Automatically lock all layers except the active one to avoid accidental changes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_autolock");

  /* Uses a single flag, because the depth order can only be 2D or 3D. */
  prop = RNA_def_property(srna, "stroke_depth_order", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, rna_enum_stroke_depth_order_items);
  RNA_def_property_ui_text(
      prop,
      "Stroke Depth Order",
      "Defines how the strokes are ordered in 3D space (for objects not displayed 'In Front')");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Onion skinning. */
  rna_def_grease_pencil_onion_skinning(srna);
}

void RNA_def_grease_pencil(BlenderRNA *brna)
{
  rna_def_grease_pencil_data(brna);
  rna_def_grease_pencil_tree_node(brna);
  rna_def_grease_pencil_layer(brna);
  rna_def_grease_pencil_layer_mask(brna);
  rna_def_grease_pencil_layer_group(brna);
  rna_def_grease_pencil_frame(brna);
  rna_def_grease_pencil_drawing(brna);
}

#endif
