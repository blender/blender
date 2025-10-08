/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_listbase.h"
#include "BLI_math_matrix.hh"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"

#include "GEO_join_geometries.hh"

#include "ED_grease_pencil.hh"

namespace blender::ed::greasepencil {

using bke::greasepencil::Layer;
using bke::greasepencil::LayerGroup;
using bke::greasepencil::TreeNode;

static void copy_layer_groups_without_layers(GreasePencil &dst_grease_pencil,
                                             const LayerGroup &src_parent,
                                             LayerGroup &dst_parent)
{
  using namespace bke::greasepencil;
  /* Note: Don't loop over all children, just the direct children. */
  LISTBASE_FOREACH (GreasePencilLayerTreeNode *, node, &src_parent.children) {
    if (!node->wrap().is_group()) {
      continue;
    }
    const LayerGroup &src_group = node->wrap().as_group();
    LayerGroup &new_group = dst_grease_pencil.add_layer_group(dst_parent, src_group.name(), false);
    BKE_grease_pencil_copy_layer_group_parameters(src_group, new_group);
    /* Repeat recursively for groups in group. */
    copy_layer_groups_without_layers(dst_grease_pencil, src_group, new_group);
  }
}

static Vector<const LayerGroup *> get_sorted_layer_parents(const Layer &layer)
{
  Vector<const LayerGroup *> parents;
  const TreeNode *node = &layer.as_node();
  while (node->parent_group()) {
    parents.append(node->parent_group());
    node = node->parent_node();
  }
  /* Reverse so that the root group is the first element. */
  std::reverse(parents.begin(), parents.end());
  return parents;
}

static const LayerGroup &find_lowest_common_ancestor(const GreasePencil &grease_pencil,
                                                     const Span<int> src_layer_indices)
{
  using namespace bke::greasepencil;
  BLI_assert(src_layer_indices.size() > 0);
  const Span<const Layer *> layers = grease_pencil.layers();
  if (src_layer_indices.size() == 1) {
    return layers[src_layer_indices.first()]->parent_group();
  }

  Vector<const LayerGroup *> candidates = get_sorted_layer_parents(
      *layers[src_layer_indices.first()]);
  for (const int layer_i : src_layer_indices) {
    const Layer &layer = *layers[layer_i];
    const Vector<const LayerGroup *> parents = get_sorted_layer_parents(layer);
    /* Possibly shrink set of candidates so that it only contains the parents common with the
     * current layer. */
    candidates.resize(std::min(candidates.size(), parents.size()));
    for (const int i : candidates.index_range()) {
      if (candidates[i] != parents[i]) {
        candidates.resize(i);
        break;
      }
    }
  }

  BLI_assert(!candidates.is_empty());
  return *candidates.last();
}

static bke::CurvesGeometry join_curves(const GreasePencil &src_grease_pencil,
                                       const Span<const bke::CurvesGeometry *> all_src_curves,
                                       const Span<float4x4> transforms_to_apply)
{
  BLI_assert(all_src_curves.size() == transforms_to_apply.size());
  Vector<bke::GeometrySet> src_geometries(all_src_curves.size());
  for (const int src_curves_i : all_src_curves.index_range()) {
    bke::CurvesGeometry src_curves = *all_src_curves[src_curves_i];
    if (src_curves.is_empty()) {
      continue;
    }
    const float4x4 &transform = transforms_to_apply[src_curves_i];
    src_curves.transform(transform);
    Curves *src_curves_id = bke::curves_new_nomain(std::move(src_curves));
    src_curves_id->mat = static_cast<Material **>(MEM_dupallocN(src_grease_pencil.material_array));
    src_curves_id->totcol = src_grease_pencil.material_array_num;
    src_geometries[src_curves_i].replace_curves(src_curves_id);
  }
  bke::GeometrySet joined_geometry = geometry::join_geometries(src_geometries, {});
  if (joined_geometry.has_curves()) {
    return joined_geometry.get_curves()->geometry.wrap();
  }
  return {};
}

void merge_layers(const GreasePencil &src_grease_pencil,
                  const Span<Vector<int>> src_layer_indices_by_dst_layer,
                  GreasePencil &dst_grease_pencil)
{
  using namespace bke::greasepencil;
  const int num_dst_layers = src_layer_indices_by_dst_layer.size();
  const Span<const Layer *> src_layers = src_grease_pencil.layers();
  const Span<const LayerGroup *> src_groups = src_grease_pencil.layer_groups();
  const Span<const GreasePencilDrawingBase *> src_drawings = src_grease_pencil.drawings();

  /* Reconstruct the same layer tree structure from the source. */
  copy_layer_groups_without_layers(
      dst_grease_pencil, src_grease_pencil.root_group(), dst_grease_pencil.root_group());
  BLI_assert(src_groups.size() == dst_grease_pencil.layer_groups().size());

  /* Find the parent group indices for all the dst layers. */
  Array<int> parent_group_index_by_dst_layer(num_dst_layers);
  for (const int dst_layer_i : src_layer_indices_by_dst_layer.index_range()) {
    const Span<int> src_layer_indices = src_layer_indices_by_dst_layer[dst_layer_i];
    const LayerGroup &parent = find_lowest_common_ancestor(src_grease_pencil, src_layer_indices);
    /* Note: For layers in the root group the index will be -1. */
    parent_group_index_by_dst_layer[dst_layer_i] = src_groups.first_index_try(&parent);
  }

  /* Important: The cache for the groups changes when layers are added. We have to make a copy of
   * all the pointers here. */
  const Array<LayerGroup *> dst_groups = dst_grease_pencil.layer_groups_for_write();

  /* Add all the layers in the destination under the right parent groups. */
  int num_dst_drawings = 0;
  Vector<Vector<int>> src_drawing_indices_by_dst_drawing;
  Vector<Vector<float4x4>> src_transforms_by_dst_drawing;
  Map<const Layer *, int> dst_layer_to_old_index_map;
  for (const int dst_layer_i : src_layer_indices_by_dst_layer.index_range()) {
    const Span<int> src_layer_indices = src_layer_indices_by_dst_layer[dst_layer_i];
    const Layer &src_first = *src_layers[src_layer_indices.first()];
    const int parent_index = parent_group_index_by_dst_layer[dst_layer_i];
    /* If the parent index is -1 then the layer is added in the root group. */
    LayerGroup &dst_parent = (parent_index == -1) ? dst_grease_pencil.root_group() :
                                                    *dst_groups[parent_index];
    Layer &dst_layer = dst_grease_pencil.add_layer(dst_parent, src_first.name(), false);
    /* Copy the layer parameters of the first source layer. */
    BKE_grease_pencil_copy_layer_parameters(src_first, dst_layer);

    dst_layer_to_old_index_map.add(&dst_layer, dst_layer_i);

    const int dst_drawing_start_index = src_drawing_indices_by_dst_drawing.size();
    const float4x4 dst_layer_transform = dst_layer.local_transform();
    const float4x4 dst_layer_transform_inv = math::invert(dst_layer_transform);
    if (src_layer_indices.size() == 1) {
      const Map<FramesMapKeyT, GreasePencilFrame> &src_frames = src_first.frames();
      Map<FramesMapKeyT, GreasePencilFrame> &dst_frames = dst_layer.frames_for_write();

      VectorSet<int> unique_src_indices;
      for (const FramesMapKeyT key : src_first.sorted_keys()) {
        const GreasePencilFrame &src_frame = src_frames.lookup(key);
        GreasePencilFrame &value = dst_frames.lookup_or_add(key, src_frame);

        int index = unique_src_indices.index_of_try(src_frame.drawing_index);
        if (index == -1) {
          unique_src_indices.add_new(src_frame.drawing_index);
          index = src_drawing_indices_by_dst_drawing.append_and_get_index(
              {src_frame.drawing_index});
          src_transforms_by_dst_drawing.append(
              {dst_layer_transform_inv * src_first.local_transform()});
          num_dst_drawings++;
        }
        else {
          index = index + dst_drawing_start_index;
        }
        value.drawing_index = index;
      }

      dst_layer.tag_frames_map_changed();
      continue;
    }

    struct InsertKeyframe {
      GreasePencilFrame frame;
      int duration = -1;
    };
    Map<FramesMapKeyT, InsertKeyframe> dst_frames;
    for (const int src_layer_i : src_layer_indices) {
      const Layer &src_layer = *src_layers[src_layer_i];
      for (const auto &item : src_layer.frames().items()) {
        if (item.value.is_end()) {
          continue;
        }
        const int duration = src_layer.get_frame_duration_at(item.key);
        BLI_assert(duration >= 0);
        dst_frames.add_or_modify(
            item.key,
            [&](InsertKeyframe *frame) { *frame = {item.value, duration}; },
            [&](InsertKeyframe *frame) {
              /* The destination frame is always an implicit hold if at least on of the source
               * frame is an implicit hold. */
              if (duration == 0) {
                frame->duration = 0;
                frame->frame.flag |= GP_FRAME_IMPLICIT_HOLD;
              }
              else if (frame->duration > 0) {
                /* For fixed duration frames, use the longest duration of the source frames. */
                frame->duration = std::max(frame->duration, duration);
              }
            });
      }
    }

    Array<FramesMapKeyT> sorted_keys(dst_frames.size());
    {
      int i = 0;
      for (const FramesMapKeyT key : dst_frames.keys()) {
        sorted_keys[i++] = key;
      }
      std::sort(sorted_keys.begin(), sorted_keys.end());
    }

    Array<Vector<int>> src_drawing_indices_by_frame(sorted_keys.size());
    Array<Vector<float4x4>> src_transforms_by_frame(sorted_keys.size());
    for (const int src_layer_i : src_layer_indices) {
      const Layer &src_layer = *src_layers[src_layer_i];
      for (const int key_i : sorted_keys.index_range()) {
        const FramesMapKeyT key = sorted_keys[key_i];
        const int drawing_index = src_layer.drawing_index_at(key);
        if (drawing_index != -1) {
          src_drawing_indices_by_frame[key_i].append(drawing_index);
          src_transforms_by_frame[key_i].append(dst_layer_transform_inv *
                                                src_layer.local_transform());
        }
      }
    }

    /* Add all the destination frames. */
    VectorSet<Vector<int>> unique_src_indices_per_drawing;
    for (const int key_i : sorted_keys.index_range()) {
      const FramesMapKeyT key = sorted_keys[key_i];
      const InsertKeyframe value = dst_frames.lookup(key);
      const Vector<int> &src_drawing_indices = src_drawing_indices_by_frame[key_i];
      const Vector<float4x4> &src_transforms = src_transforms_by_frame[key_i];

      GreasePencilFrame *frame = dst_layer.add_frame(key, value.duration);
      /* Copy frame parameters. */
      frame->flag = value.frame.flag;
      frame->type = value.frame.type;

      /* In case drawings are shared in the source, keep sharing the drawings if possible. */
      int index = unique_src_indices_per_drawing.index_of_try(src_drawing_indices);
      if (index == -1) {
        unique_src_indices_per_drawing.add_new(src_drawing_indices);
        index = src_drawing_indices_by_dst_drawing.append_and_get_index(src_drawing_indices);
        src_transforms_by_dst_drawing.append(src_transforms);
        num_dst_drawings++;
      }
      else {
        index = index + dst_drawing_start_index;
      }
      frame->drawing_index = index;
    }

    dst_layer.tag_frames_map_changed();
  }

  /* The destination layers don't map to the order of elements in #src_layer_indices_by_dst_layer.
   * This map maps between the old order and the final order in the destination Grease Pencil. */
  Array<int> old_to_new_index_map(num_dst_layers);
  for (const int layer_i : dst_grease_pencil.layers().index_range()) {
    const Layer *layer = &dst_grease_pencil.layer(layer_i);
    old_to_new_index_map[dst_layer_to_old_index_map.lookup(layer)] = layer_i;
  }

  /* Add all the drawings. */
  if (num_dst_drawings > 0) {
    dst_grease_pencil.add_empty_drawings(num_dst_drawings);
  }

  MutableSpan<GreasePencilDrawingBase *> dst_drawings = dst_grease_pencil.drawings();
  threading::parallel_for(dst_drawings.index_range(), 32, [&](const IndexRange range) {
    for (const int dst_drawing_i : range) {
      const Span<int> src_drawing_indices = src_drawing_indices_by_dst_drawing[dst_drawing_i];
      const Span<float4x4> src_transforms_to_apply = src_transforms_by_dst_drawing[dst_drawing_i];
      const GreasePencilDrawingBase *src_first_base = src_drawings[src_drawing_indices.first()];
      BLI_assert(src_first_base->type == GP_DRAWING);
      GreasePencilDrawingBase *dst_base = dst_drawings[dst_drawing_i];
      BLI_assert(dst_base->type == GP_DRAWING);
      /* Copy the parameters of the first source drawing. */
      dst_base->flag = src_first_base->flag;

      Drawing &dst_drawing = reinterpret_cast<GreasePencilDrawing *>(dst_base)->wrap();
      if (src_drawing_indices.size() == 1) {
        const Drawing &src_drawing =
            reinterpret_cast<const GreasePencilDrawing *>(src_first_base)->wrap();
        dst_drawing.strokes_for_write() = src_drawing.strokes();
        dst_drawing.tag_topology_changed();
        continue;
      }

      /* Gather all the source curves to be merged. */
      Vector<const bke::CurvesGeometry *> all_src_curves;
      for (const int src_darwing_i : src_drawing_indices) {
        const GreasePencilDrawingBase *src_base = src_drawings[src_darwing_i];
        BLI_assert(src_base->type == GP_DRAWING);
        const Drawing &src_drawing =
            reinterpret_cast<const GreasePencilDrawing *>(src_base)->wrap();
        all_src_curves.append(&src_drawing.strokes());
      }

      dst_drawing.strokes_for_write() = join_curves(
          src_grease_pencil, all_src_curves, src_transforms_to_apply);
      dst_drawing.tag_topology_changed();
    }
  });

  /* Update the user count for all the drawings. */
  for (const Layer *dst_layer : dst_grease_pencil.layers()) {
    dst_grease_pencil.update_drawing_users_for_layer(*dst_layer);
  }

  /* Gather all the layer attributes. */
  const bke::AttributeAccessor src_attributes = src_grease_pencil.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_grease_pencil.attributes_for_write();
  src_attributes.foreach_attribute([&](const blender::bke::AttributeIter &iter) {
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    bke::GAttributeReader src_attribute = iter.get();
    bke::GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, bke::AttrDomain::Layer, iter.data_type);
    if (!dst_attribute) {
      return;
    }

    const CPPType &type = dst_attribute.span.type();
    bke::attribute_math::convert_to_static_type(type, [&](auto type) {
      using T = decltype(type);
      const VArraySpan<T> src_span = src_attribute.varray.typed<T>();
      MutableSpan<T> new_span = dst_attribute.span.typed<T>();

      bke::attribute_math::DefaultMixer<T> mixer(new_span);
      for (const int dst_layer_i : IndexRange(num_dst_layers)) {
        const Span<int> src_layer_indices = src_layer_indices_by_dst_layer[dst_layer_i];
        const int new_index = old_to_new_index_map[dst_layer_i];
        for (const int src_layer_i : src_layer_indices) {
          const T &src_value = src_span[src_layer_i];
          mixer.mix_in(new_index, src_value);
        }
      }
      mixer.finalize();
    });

    dst_attribute.finish();
  });
}

}  // namespace blender::ed::greasepencil
