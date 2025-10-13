/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "atomic_ops.h"

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_atomic_disjoint_set.hh"
#include "BLI_math_matrix.hh"
#include "BLI_sort.hh"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

#include "DNA_mesh_types.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_mesh_selection.hh"

#include "NOD_rna_define.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_scale_elements_cc {

static const EnumPropertyItem scale_mode_items[] = {
    {GEO_NODE_SCALE_ELEMENTS_UNIFORM,
     "UNIFORM",
     ICON_NONE,
     N_("Uniform"),
     N_("Scale elements by the same factor in every direction")},
    {GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS,
     "SINGLE_AXIS",
     ICON_NONE,
     N_("Single Axis"),
     N_("Scale elements in a single direction")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.add_input<decl::Geometry>("Geometry")
      .supported_type(GeometryComponent::Type::Mesh)
      .description("Geometry to scale elements of");
  b.add_output<decl::Geometry>("Geometry").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();

  b.add_input<decl::Float>("Scale", "Scale").default_value(1.0f).min(0.0f).field_on_all();
  b.add_input<decl::Vector>("Center")
      .subtype(PROP_TRANSLATION)
      .implicit_field_on_all(NODE_DEFAULT_INPUT_POSITION_FIELD)
      .description(
          "Origin of the scaling for each element. If multiple elements are connected, their "
          "center is averaged");
  b.add_input<decl::Menu>("Scale Mode")
      .static_items(scale_mode_items)
      .default_value(GEO_NODE_SCALE_ELEMENTS_UNIFORM)
      .optional_label();
  b.add_input<decl::Vector>("Axis")
      .default_value({1.0f, 0.0f, 0.0f})
      .field_on_all()
      .description("Direction in which to scale the element")
      .usage_by_single_menu(GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS);
};

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int16_t(AttrDomain::Face);
}

static Array<int> create_reverse_offsets(const Span<int> indices, const int items_num)
{
  Array<int> offsets(items_num + 1, 0);
  offset_indices::build_reverse_offsets(indices, offsets);
  return offsets;
}

static Span<int> front_indices_to_same_value(const Span<int> indices, const Span<int> values)
{
  const int value = values[indices.first()];
  const int &first_other = *std::find_if(
      indices.begin(), indices.end(), [&](const int index) { return values[index] != value; });
  return indices.take_front(&first_other - indices.begin());
}

static void from_indices_large_groups(const Span<int> group_indices,
                                      MutableSpan<int> r_counts_to_offset,
                                      MutableSpan<int> r_indices)
{
  constexpr const int segment_size = 1024;
  constexpr const IndexRange segment(segment_size);
  const bool last_small_segmet = bool(group_indices.size() % segment_size);
  const int total_segments = group_indices.size() / segment_size + int(last_small_segmet);

  Array<int> src_indices(group_indices.size());
  threading::parallel_for_each(IndexRange(total_segments), [&](const int segment_index) {
    const IndexRange range = segment.shift(segment_size * segment_index);
    MutableSpan<int> segment_indices = src_indices.as_mutable_span().slice_safe(range);
    std::iota(segment_indices.begin(), segment_indices.end(), segment_size * segment_index);
    parallel_sort(segment_indices.begin(), segment_indices.end(), [&](const int a, const int b) {
      return group_indices[a] < group_indices[b];
    });

    for (Span<int> indices = segment_indices; !indices.is_empty();) {
      const int group = group_indices[indices.first()];
      const int step_size = front_indices_to_same_value(indices, group_indices).size();
      atomic_add_and_fetch_int32(&r_counts_to_offset[group], step_size);
      indices = indices.drop_front(step_size);
    }
  });

  const OffsetIndices<int> offset = offset_indices::accumulate_counts_to_offsets(
      r_counts_to_offset);
  Array<int> counts(offset.size(), 0);
  threading::parallel_for_each(IndexRange(total_segments), [&](const int segment_index) {
    const IndexRange range = segment.shift(segment_size * segment_index);
    const Span<int> segment_indices = src_indices.as_span().slice_safe(range);
    for (Span<int> indices = segment_indices; !indices.is_empty();) {
      const Span<int> indices_of_current_group = front_indices_to_same_value(indices,
                                                                             group_indices);
      const int step_size = indices_of_current_group.size();
      const int group = group_indices[indices.first()];
      const int start = atomic_add_and_fetch_int32(&counts[group], step_size) - step_size;
      const IndexRange dst_range = offset[group].slice(start, step_size);
      array_utils::copy(indices_of_current_group, r_indices.slice(dst_range));
      indices = indices.drop_front(step_size);
    }
  });
}

static Array<int> reverse_indices_in_groups(const Span<int> group_indices,
                                            const OffsetIndices<int> offsets)
{
  if (group_indices.is_empty()) {
    return {};
  }
  BLI_assert(*std::max_element(group_indices.begin(), group_indices.end()) < offsets.size());
  BLI_assert(*std::min_element(group_indices.begin(), group_indices.end()) >= 0);

  /* `counts` keeps track of how many elements have been added to each group, and is incremented
   * atomically by many threads in parallel. `calloc` can be measurably faster than a parallel fill
   * of zero. Alternatively the offsets could be copied and incremented directly, but the cost of
   * the copy is slightly higher than the cost of `calloc`. */
  int *counts = MEM_calloc_arrayN<int>(offsets.size(), __func__);
  BLI_SCOPED_DEFER([&]() { MEM_freeN(counts); })
  Array<int> results(group_indices.size());
  threading::parallel_for(group_indices.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t i : range) {
      const int group_index = group_indices[i];
      const int index_in_group = atomic_fetch_and_add_int32(&counts[group_index], 1);
      results[offsets[group_index][index_in_group]] = int(i);
    }
  });
  return results;
}

static GroupedSpan<int> gather_groups(const Span<int> group_indices,
                                      const int groups_num,
                                      Array<int> &r_offsets,
                                      Array<int> &r_indices)
{
  if (group_indices.size() / groups_num > 1000) {
    r_offsets.reinitialize(groups_num + 1);
    r_offsets.as_mutable_span().fill(0);
    r_indices.reinitialize(group_indices.size());
    from_indices_large_groups(group_indices, r_offsets, r_indices);
  }
  else {
    r_offsets = create_reverse_offsets(group_indices, groups_num);
    r_indices = reverse_indices_in_groups(group_indices, r_offsets.as_span());
  }
  return {OffsetIndices<int>(r_offsets), r_indices};
}

template<typename T> static T gather_mean(const VArray<T> &values, const Span<int> indices)
{
  BLI_assert(!indices.is_empty());
  if (const std::optional<T> value = values.get_if_single()) {
    return *value;
  }

  using MeanAccumulator = std::pair<T, int>;
  const auto join_accumulators = [](const MeanAccumulator a,
                                    const MeanAccumulator b) -> MeanAccumulator {
    return {(a.first + b.first) / (a.second + b.second), 1};
  };

  T value;
  devirtualize_varray(values, [&](const auto values) {
    const auto accumulator = threading::parallel_deterministic_reduce<MeanAccumulator>(
        indices.index_range(),
        2048,
        MeanAccumulator(T(), 0),
        [&](const IndexRange range, MeanAccumulator other) -> MeanAccumulator {
          T value(0);
          for (const int i : indices.slice(range)) {
            value += values[i];
          }
          return join_accumulators({value, int(range.size())}, other);
        },
        join_accumulators);
    value = accumulator.first / accumulator.second;
  });
  return value;
}

static float3 transform_with_uniform_scale(const float3 &position,
                                           const float3 &center,
                                           const float scale)
{
  const float3 diff = position - center;
  const float3 scaled_diff = scale * diff;
  const float3 new_position = center + scaled_diff;
  return new_position;
}

static void scale_uniformly(const GroupedSpan<int> elem_islands,
                            const GroupedSpan<int> vert_islands,
                            const VArray<float> &scale_varray,
                            const VArray<float3> &center_varray,
                            Mesh &mesh)
{
  MutableSpan<float3> positions = mesh.vert_positions_for_write();
  threading::parallel_for(
      elem_islands.index_range(),
      512,
      [&](const IndexRange range) {
        for (const int island_index : range) {
          const Span<int> vert_island = vert_islands[island_index];
          const Span<int> elem_island = elem_islands[island_index];

          const float scale = gather_mean<float>(scale_varray, elem_island);
          const float3 center = gather_mean<float3>(center_varray, elem_island);

          threading::parallel_for(vert_island.index_range(), 2048, [&](const IndexRange range) {
            for (const int vert_i : vert_island.slice(range)) {
              positions[vert_i] = transform_with_uniform_scale(positions[vert_i], center, scale);
            }
          });
        }
      },
      threading::accumulated_task_sizes([&](const IndexRange range) {
        return elem_islands.offsets[range].size() + vert_islands.offsets[range].size();
      }));
}

static float4x4 create_single_axis_transform(const float3 &center,
                                             const float3 &axis,
                                             const float scale)
{
  /* Scale along x axis. The other axis need to be orthogonal, but their specific value does not
   * matter. */
  const float3 x_axis = math::normalize(axis);
  float3 y_axis = math::cross(x_axis, float3(0.0f, 0.0f, 1.0f));
  if (math::is_zero(y_axis)) {
    y_axis = math::cross(x_axis, float3(0.0f, 1.0f, 0.0f));
  }
  y_axis = math::normalize(y_axis);
  const float3 z_axis = math::cross(x_axis, y_axis);

  float4x4 transform = float4x4::identity();

  /* Move scaling center to the origin. */
  transform.location() -= center;

  /* `base_change` and `base_change_inv` are used to rotate space so that scaling along the
   * provided axis is the same as scaling along the x axis. */
  float4x4 base_change = float4x4::identity();
  base_change.x_axis() = x_axis;
  base_change.y_axis() = y_axis;
  base_change.z_axis() = z_axis;

  /* Can invert by transposing, because the matrix is orthonormal. */
  float4x4 base_change_inv = math::transpose(base_change);

  float4x4 scale_transform = float4x4::identity();
  scale_transform[0][0] = scale;

  transform = base_change * scale_transform * base_change_inv * transform;

  /* Move scaling center back to where it was. */
  transform.location() += center;

  return transform;
}

static void scale_on_axis(const GroupedSpan<int> elem_islands,
                          const GroupedSpan<int> vert_islands,
                          const VArray<float> &scale_varray,
                          const VArray<float3> &center_varray,
                          const VArray<float3> &axis_varray,
                          Mesh &mesh)
{
  MutableSpan<float3> positions = mesh.vert_positions_for_write();
  threading::parallel_for(
      elem_islands.index_range(),
      512,
      [&](const IndexRange range) {
        for (const int island_index : range) {
          const Span<int> vert_island = vert_islands[island_index];
          const Span<int> elem_island = elem_islands[island_index];

          const float scale = gather_mean<float>(scale_varray, elem_island);
          const float3 center = gather_mean<float3>(center_varray, elem_island);
          const float3 axis = gather_mean<float3>(axis_varray, elem_island);
          const float3 fixed_axis = math::is_zero(axis) ? float3(1.0f, 0.0f, 0.0f) : axis;

          const float4x4 transform = create_single_axis_transform(center, fixed_axis, scale);
          threading::parallel_for(vert_island.index_range(), 2048, [&](const IndexRange range) {
            for (const int vert_i : vert_island.slice(range)) {
              positions[vert_i] = math::transform_point(transform, positions[vert_i]);
            }
          });
        }
      },
      threading::accumulated_task_sizes([&](const IndexRange range) {
        return vert_islands.offsets[range].size() + elem_islands.offsets[range].size();
      }));
}

static int face_to_vert_islands(const Mesh &mesh,
                                const IndexMask &face_mask,
                                const IndexMask &vert_mask,
                                MutableSpan<int> face_island_indices,
                                MutableSpan<int> vert_island_indices)
{
  Array<int> verts_pos(vert_mask.min_array_size());
  index_mask::build_reverse_map<int>(vert_mask, verts_pos);

  AtomicDisjointSet disjoint_set(vert_mask.size());
  const GroupedSpan<int> face_verts(mesh.faces(), mesh.corner_verts());

  face_mask.foreach_index_optimized<int>(GrainSize(4096), [&](const int face_i) {
    const Span<int> verts = face_verts[face_i];
    const int v1 = verts_pos[verts.first()];
    for (const int vert_i : verts.drop_front(1)) {
      const int v2 = verts_pos[vert_i];
      disjoint_set.join(v1, v2);
    }
  });

  disjoint_set.calc_reduced_ids(vert_island_indices);

  face_mask.foreach_index(GrainSize(4096), [&](const int face_i, const int face_pos) {
    const int face_vert_i = face_verts[face_i].first();
    const int vert_pos = verts_pos[face_vert_i];
    const int vert_island = vert_island_indices[vert_pos];
    face_island_indices[face_pos] = vert_island;
  });

  return disjoint_set.count_sets();
}

static void gather_face_islands(const Mesh &mesh,
                                const IndexMask &face_mask,
                                Array<int> &r_item_offsets,
                                Array<int> &r_item_indices,
                                Array<int> &r_vert_offsets,
                                Array<int> &r_vert_indices)
{
  IndexMaskMemory memory;
  const IndexMask vert_mask = geometry::vert_selection_from_face(
      mesh.face_offsets(), face_mask, mesh.corner_verts(), mesh.verts_num, memory);

  Array<int> face_island_indices(face_mask.size());
  Array<int> vert_island_indices(vert_mask.size());
  const int total_islands = face_to_vert_islands(
      mesh, face_mask, vert_mask, face_island_indices, vert_island_indices);

  /* Group gathered vertices and faces. */
  gather_groups(vert_island_indices, total_islands, r_vert_offsets, r_vert_indices);
  gather_groups(face_island_indices, total_islands, r_item_offsets, r_item_indices);

  /* If result indices is for gathered array, map than back into global indices. */
  if (face_mask.size() != mesh.faces_num) {
    Array<int> face_mask_map(face_mask.size());
    face_mask.to_indices<int>(face_mask_map);
    array_utils::gather<int>(
        face_mask_map.as_span(), r_item_indices.as_span(), r_item_indices.as_mutable_span());
  }
  if (vert_mask.size() != mesh.verts_num) {
    Array<int> vert_mask_map(vert_mask.size());
    vert_mask.to_indices<int>(vert_mask_map);
    array_utils::gather<int>(
        vert_mask_map.as_span(), r_vert_indices.as_span(), r_vert_indices.as_mutable_span());
  }
}

static int edge_to_vert_islands(const Mesh &mesh,
                                const IndexMask &edge_mask,
                                const IndexMask &vert_mask,
                                MutableSpan<int> edge_island_indices,
                                MutableSpan<int> vert_island_indices)
{
  Array<int> verts_pos(vert_mask.min_array_size());
  index_mask::build_reverse_map<int>(vert_mask, verts_pos);

  AtomicDisjointSet disjoint_set(vert_mask.size());
  const Span<int2> edges = mesh.edges();

  edge_mask.foreach_index_optimized<int>(GrainSize(4096), [&](const int edge_i) {
    const int2 edge = edges[edge_i];
    const int v1 = verts_pos[edge[0]];
    const int v2 = verts_pos[edge[1]];
    disjoint_set.join(v1, v2);
  });

  disjoint_set.calc_reduced_ids(vert_island_indices);

  edge_mask.foreach_index(GrainSize(4096), [&](const int edge_i, const int edge_pos) {
    const int2 edge = edges[edge_i];
    const int edge_vert_i = edge[0];
    const int vert_pos = verts_pos[edge_vert_i];
    const int vert_island = vert_island_indices[vert_pos];
    edge_island_indices[edge_pos] = vert_island;
  });

  return disjoint_set.count_sets();
}

static void gather_edge_islands(const Mesh &mesh,
                                const IndexMask &edge_mask,
                                Array<int> &r_item_offsets,
                                Array<int> &r_item_indices,
                                Array<int> &r_vert_offsets,
                                Array<int> &r_vert_indices)
{
  IndexMaskMemory memory;
  const IndexMask vert_mask = geometry::vert_selection_from_edge(
      mesh.edges(), edge_mask, mesh.verts_num, memory);

  Array<int> edge_island_indices(edge_mask.size());
  Array<int> vert_island_indices(vert_mask.size());
  const int total_islands = edge_to_vert_islands(
      mesh, edge_mask, vert_mask, edge_island_indices, vert_island_indices);

  /* Group gathered vertices and edges. */
  gather_groups(vert_island_indices, total_islands, r_vert_offsets, r_vert_indices);
  gather_groups(edge_island_indices, total_islands, r_item_offsets, r_item_indices);

  /* If result indices is for gathered array, map than back into global indices. */
  if (edge_mask.size() != mesh.edges_num) {
    Array<int> edge_mask_map(edge_mask.size());
    edge_mask.to_indices<int>(edge_mask_map);
    array_utils::gather<int>(
        edge_mask_map.as_span(), r_item_indices.as_span(), r_item_indices.as_mutable_span());
  }
  if (vert_mask.size() != mesh.verts_num) {
    Array<int> vert_mask_map(vert_mask.size());
    vert_mask.to_indices<int>(vert_mask_map);
    array_utils::gather<int>(
        vert_mask_map.as_span(), r_vert_indices.as_span(), r_vert_indices.as_mutable_span());
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const AttrDomain domain = AttrDomain(node.custom1);
  const auto scale_mode = params.get_input<GeometryNodeScaleElementsMode>("Scale Mode");

  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");

  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  const Field<float> scale_field = params.extract_input<Field<float>>("Scale");
  const Field<float3> center_field = params.extract_input<Field<float3>>("Center");

  geometry::foreach_real_geometry(geometry, [&](GeometrySet &geometry) {
    if (Mesh *mesh = geometry.get_mesh_for_write()) {
      const bke::MeshFieldContext context{*mesh, domain};
      FieldEvaluator evaluator{context, mesh->attributes().domain_size(domain)};
      evaluator.set_selection(selection_field);
      evaluator.add(scale_field);
      evaluator.add(center_field);
      if (scale_mode == GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS) {
        evaluator.add(params.get_input<Field<float3>>("Axis"));
      }
      evaluator.evaluate();
      const IndexMask &mask = evaluator.get_evaluated_selection_as_mask();
      if (mask.is_empty()) {
        return;
      }

      Array<int> item_offsets;
      Array<int> item_indices;

      Array<int> vert_offsets;
      Array<int> vert_indices;

      switch (domain) {
        case AttrDomain::Face:
          gather_face_islands(*mesh, mask, item_offsets, item_indices, vert_offsets, vert_indices);
          break;
        case AttrDomain::Edge:
          gather_edge_islands(*mesh, mask, item_offsets, item_indices, vert_offsets, vert_indices);
          break;
        default:
          BLI_assert_unreachable();
      }

      const GroupedSpan<int> item_islands(item_offsets.as_span(), item_indices);
      const GroupedSpan<int> vert_islands(vert_offsets.as_span(), vert_indices);

      const VArray<float> scale_varray = evaluator.get_evaluated<float>(0);
      const VArray<float3> center_varray = evaluator.get_evaluated<float3>(1);

      switch (scale_mode) {
        case GEO_NODE_SCALE_ELEMENTS_UNIFORM:
          scale_uniformly(item_islands, vert_islands, scale_varray, center_varray, *mesh);
          break;
        case GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS: {
          const VArray<float3> axis_varray = evaluator.get_evaluated<float3>(2);
          scale_on_axis(
              item_islands, vert_islands, scale_varray, center_varray, axis_varray, *mesh);
          break;
        }
      }
      mesh->tag_positions_changed();
    }
  });

  params.set_output("Geometry", std::move(geometry));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem domain_items[] = {
      {int(AttrDomain::Face),
       "FACE",
       ICON_NONE,
       "Face",
       "Scale individual faces or neighboring face islands"},
      {int(AttrDomain::Edge),
       "EDGE",
       ICON_NONE,
       "Edge",
       "Scale individual edges or neighboring edge islands"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "Element type to transform",
                    domain_items,
                    NOD_inline_enum_accessors(custom1),
                    int(AttrDomain::Face));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeScaleElements", GEO_NODE_SCALE_ELEMENTS);
  ntype.ui_name = "Scale Elements";
  ntype.ui_description = "Scale groups of connected edges and faces";
  ntype.enum_name_legacy = "SCALE_ELEMENTS";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_scale_elements_cc
