/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_map.hh"
#include "BLI_noise.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"

#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"

#include "node_geometry_util.hh"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_duplicate_elements_cc {

NODE_STORAGE_FUNCS(NodeGeometryDuplicateElements);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Int>("Amount").min(0).default_value(1).field_on_all().description(
      "The number of duplicates to create for each element");

  b.add_output<decl::Geometry>("Geometry")
      .propagate_all()
      .description("The duplicated geometry, not including the original geometry");
  b.add_output<decl::Int>("Duplicate Index")
      .field_on_all()
      .description("The indices of the duplicates for each element");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryDuplicateElements *data = MEM_cnew<NodeGeometryDuplicateElements>(__func__);
  data->domain = int8_t(AttrDomain::Point);
  node->storage = data;
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

struct IndexAttributes {
  AnonymousAttributeIDPtr duplicate_index;
};

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

static OffsetIndices<int> accumulate_counts_to_offsets(const IndexMask &selection,
                                                       const VArray<int> &counts,
                                                       Array<int> &r_offset_data)
{
  r_offset_data.reinitialize(selection.size() + 1);
  if (const std::optional<int> count = counts.get_if_single()) {
    offset_indices::fill_constant_group_size(*count, 0, r_offset_data);
  }
  else {
    array_utils::gather(counts, selection, r_offset_data.as_mutable_span().drop_back(1), 1024);
    offset_indices::accumulate_counts_to_offsets(r_offset_data);
  }
  return OffsetIndices<int>(r_offset_data);
}

static void copy_hashed_ids(const Span<int> src, const int hash, MutableSpan<int> dst)
{
  for (const int i : src.index_range()) {
    dst[i] = noise::hash(src[i], hash);
  }
}

static void threaded_id_offset_copy(const OffsetIndices<int> offsets,
                                    const Span<int> src,
                                    MutableSpan<int> all_dst)
{
  BLI_assert(offsets.total_size() == all_dst.size());
  threading::parallel_for(offsets.index_range(), 512, [&](IndexRange range) {
    for (const int i : range) {
      MutableSpan<int> dst = all_dst.slice(offsets[i]);
      if (dst.is_empty()) {
        continue;
      }
      dst.first() = src[i];
      for (const int i_duplicate : dst.index_range().drop_front(1)) {
        dst[i_duplicate] = noise::hash(src[i], i_duplicate);
      }
    }
  });
}

/** Create the copy indices for the duplication domain. */
static void create_duplicate_index_attribute(bke::MutableAttributeAccessor attributes,
                                             const AttrDomain output_domain,
                                             const IndexMask &selection,
                                             const IndexAttributes &attribute_outputs,
                                             const OffsetIndices<int> offsets)
{
  SpanAttributeWriter<int> duplicate_indices = attributes.lookup_or_add_for_write_only_span<int>(
      attribute_outputs.duplicate_index.get(), output_domain);
  for (const int i : IndexRange(selection.size())) {
    MutableSpan<int> indices = duplicate_indices.span.slice(offsets[i]);
    for (const int i : indices.index_range()) {
      indices[i] = i;
    }
  }
  duplicate_indices.finish();
}

/**
 * Copy the stable ids to the first duplicate and create new ids based on a hash of the original id
 * and the duplicate number. This function is used for the point domain elements.
 */
static void copy_stable_id_point(const OffsetIndices<int> offsets,
                                 const bke::AttributeAccessor src_attributes,
                                 bke::MutableAttributeAccessor dst_attributes)
{
  GAttributeReader src_attribute = src_attributes.lookup("id");
  if (!src_attribute) {
    return;
  }
  GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_only_span(
      "id", AttrDomain::Point, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  VArraySpan<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.span.typed<int>();
  threaded_id_offset_copy(offsets, src, dst);
  dst_attribute.finish();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Curves
 * \{ */

/**
 * Copies the attributes for curve duplicates. If copying the curve domain, the attributes are
 * copied with an offset fill, otherwise a mapping is used.
 */
static void copy_curve_attributes_without_id(
    const bke::CurvesGeometry &src_curves,
    const IndexMask &selection,
    const OffsetIndices<int> curve_offsets,
    const AnonymousAttributePropagationInfo &propagation_info,
    bke::CurvesGeometry &dst_curves)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  for (auto &attribute : bke::retrieve_attributes_for_transfer(src_curves.attributes(),
                                                               dst_curves.attributes_for_write(),
                                                               ATTR_DOMAIN_MASK_ALL,
                                                               propagation_info,
                                                               {"id"}))
  {
    switch (attribute.meta_data.domain) {
      case AttrDomain::Curve:
        bke::attribute_math::gather_to_groups(
            curve_offsets, selection, attribute.src, attribute.dst.span);
        break;
      case AttrDomain::Point:
        bke::attribute_math::convert_to_static_type(attribute.src.type(), [&](auto dummy) {
          using T = decltype(dummy);
          const Span<T> src = attribute.src.typed<T>();
          MutableSpan<T> dst = attribute.dst.span.typed<T>();
          selection.foreach_index(
              GrainSize(512), [&](const int64_t index, const int64_t i_selection) {
                const Span<T> curve_src = src.slice(src_points_by_curve[index]);
                for (const int dst_curve_index : curve_offsets[i_selection]) {
                  dst.slice(dst_points_by_curve[dst_curve_index]).copy_from(curve_src);
                }
              });
        });
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
    attribute.dst.finish();
  }
}

/**
 * Copy the stable ids to the first duplicate and create new ids based on a hash of the original id
 * and the duplicate number. In the curve case, copy the entire curve's points to the
 * destination,
 * then loop over the remaining ones point by point, hashing their ids to the new ids.
 */
static void copy_stable_id_curves(const bke::CurvesGeometry &src_curves,
                                  const IndexMask &selection,
                                  const OffsetIndices<int> offsets,
                                  bke::CurvesGeometry &dst_curves)
{
  GAttributeReader src_attribute = src_curves.attributes().lookup("id");
  if (!src_attribute) {
    return;
  }
  GSpanAttributeWriter dst_attribute =
      dst_curves.attributes_for_write().lookup_or_add_for_write_only_span(
          "id", AttrDomain::Point, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  VArraySpan<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.span.typed<int>();

  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  selection.foreach_index(
      GrainSize(512), [&](const int64_t i_src_curve, const int64_t i_selection) {
        const Span<int> curve_src = src.slice(src_points_by_curve[i_src_curve]);
        const IndexRange duplicates_range = offsets[i_selection];
        for (const int i_duplicate : IndexRange(offsets[i_selection].size()).drop_front(1)) {
          const int i_dst_curve = duplicates_range[i_duplicate];
          copy_hashed_ids(curve_src, i_duplicate, dst.slice(dst_points_by_curve[i_dst_curve]));
        }
      });

  dst_attribute.finish();
}

static void duplicate_curves(GeometrySet &geometry_set,
                             const Field<int> &count_field,
                             const Field<bool> &selection_field,
                             const IndexAttributes &attribute_outputs,
                             const AnonymousAttributePropagationInfo &propagation_info)
{
  if (!geometry_set.has_curves()) {
    geometry_set.remove_geometry_during_modify();
    return;
  }
  geometry_set.keep_only_during_modify({GeometryComponent::Type::Curve});
  GeometryComponentEditData::remember_deformed_positions_if_necessary(geometry_set);

  const Curves &curves_id = *geometry_set.get_curves();
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();

  const bke::CurvesFieldContext field_context{curves, AttrDomain::Curve};
  FieldEvaluator evaluator{field_context, curves.curves_num()};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  const OffsetIndices points_by_curve = curves.points_by_curve();

  /* The offset in the result curve domain at every selected input curve. */
  Array<int> curve_offset_data(selection.size() + 1);
  Array<int> point_offset_data(selection.size() + 1);

  int dst_curves_num = 0;
  int dst_points_num = 0;

  selection.foreach_index_optimized<int>([&](const int index, const int i_curve) {
    const int count = counts[index];
    curve_offset_data[i_curve] = dst_curves_num;
    point_offset_data[i_curve] = dst_points_num;
    dst_curves_num += count;
    dst_points_num += count * points_by_curve[index].size();
  });

  if (dst_points_num == 0) {
    geometry_set.remove_geometry_during_modify();
    return;
  }

  curve_offset_data.last() = dst_curves_num;
  point_offset_data.last() = dst_points_num;

  const OffsetIndices<int> curve_offsets(curve_offset_data);
  const OffsetIndices<int> point_offsets(point_offset_data);

  Curves *new_curves_id = bke::curves_new_nomain(dst_points_num, dst_curves_num);
  bke::curves_copy_parameters(curves_id, *new_curves_id);
  bke::CurvesGeometry &new_curves = new_curves_id->geometry.wrap();
  MutableSpan<int> all_dst_offsets = new_curves.offsets_for_write();

  selection.foreach_index(GrainSize(512),
                          [&](const int64_t i_src_curve, const int64_t i_selection) {
                            const IndexRange src_curve_range = points_by_curve[i_src_curve];
                            const IndexRange dst_curves_range = curve_offsets[i_selection];
                            MutableSpan<int> dst_offsets = all_dst_offsets.slice(dst_curves_range);
                            for (const int i_duplicate : IndexRange(dst_curves_range.size())) {
                              dst_offsets[i_duplicate] = point_offsets[i_selection].start() +
                                                         src_curve_range.size() * i_duplicate;
                            }
                          });

  all_dst_offsets.last() = dst_points_num;

  copy_curve_attributes_without_id(curves, selection, curve_offsets, propagation_info, new_curves);

  copy_stable_id_curves(curves, selection, curve_offsets, new_curves);

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(new_curves.attributes_for_write(),
                                     AttrDomain::Curve,
                                     selection,
                                     attribute_outputs,
                                     curve_offsets);
  }

  new_curves.update_curve_types();
  geometry_set.replace_curves(new_curves_id);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Faces
 * \{ */

/**
 * Copies the attributes for face duplicates. If copying the face domain, the attributes are
 * copied with an offset fill, otherwise a mapping is used.
 */
static void copy_face_attributes_without_id(
    const Span<int> edge_mapping,
    const Span<int> vert_mapping,
    const Span<int> loop_mapping,
    const OffsetIndices<int> offsets,
    const IndexMask &selection,
    const AnonymousAttributePropagationInfo &propagation_info,
    const bke::AttributeAccessor src_attributes,
    bke::MutableAttributeAccessor dst_attributes)
{
  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes,
           dst_attributes,
           ATTR_DOMAIN_MASK_ALL,
           propagation_info,
           {"id", ".corner_vert", ".corner_edge", ".edge_verts"}))
  {
    switch (attribute.meta_data.domain) {
      case AttrDomain::Point:
        bke::attribute_math::gather(attribute.src, vert_mapping, attribute.dst.span);
        break;
      case AttrDomain::Edge:
        bke::attribute_math::gather(attribute.src, edge_mapping, attribute.dst.span);
        break;
      case AttrDomain::Face:
        bke::attribute_math::gather_to_groups(
            offsets, selection, attribute.src, attribute.dst.span);
        break;
      case AttrDomain::Corner:
        bke::attribute_math::gather(attribute.src, loop_mapping, attribute.dst.span);
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
    attribute.dst.finish();
  }
}

/**
 * Copy the stable ids to the first duplicate and create new ids based on a hash of the original id
 * and the duplicate number. This function is used for points when duplicating the face domain.
 *
 * This function could be threaded in the future, but since it is only 1 attribute and the
 * `face->edge->vert` mapping would mean creating a 1/1 mapping to allow for it, is it worth it?
 */
static void copy_stable_id_faces(const Mesh &mesh,
                                 const IndexMask &selection,
                                 const OffsetIndices<int> face_offsets,
                                 const Span<int> vert_mapping,
                                 const bke::AttributeAccessor src_attributes,
                                 bke::MutableAttributeAccessor dst_attributes)
{
  GAttributeReader src_attribute = src_attributes.lookup("id");
  if (!src_attribute) {
    return;
  }
  GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_only_span(
      "id", AttrDomain::Point, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  VArraySpan<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.span.typed<int>();

  const OffsetIndices faces = mesh.faces();
  int loop_index = 0;
  for (const int i_face : selection.index_range()) {
    const IndexRange range = face_offsets[i_face];
    if (range.is_empty()) {
      continue;
    }
    const IndexRange source = faces[i_face];
    for ([[maybe_unused]] const int i_duplicate : IndexRange(range.size())) {
      for ([[maybe_unused]] const int i_loops : IndexRange(source.size())) {
        if (i_duplicate == 0) {
          dst[loop_index] = src[vert_mapping[loop_index]];
        }
        else {
          dst[loop_index] = noise::hash(src[vert_mapping[loop_index]], i_duplicate);
        }
        loop_index++;
      }
    }
  }

  dst_attribute.finish();
}

static void duplicate_faces(GeometrySet &geometry_set,
                            const Field<int> &count_field,
                            const Field<bool> &selection_field,
                            const IndexAttributes &attribute_outputs,
                            const AnonymousAttributePropagationInfo &propagation_info)
{
  if (!geometry_set.has_mesh()) {
    geometry_set.remove_geometry_during_modify();
    return;
  }
  geometry_set.keep_only_during_modify({GeometryComponent::Type::Mesh});

  const Mesh &mesh = *geometry_set.get_mesh();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int> corner_edges = mesh.corner_edges();

  const bke::MeshFieldContext field_context{mesh, AttrDomain::Face};
  FieldEvaluator evaluator(field_context, faces.size());
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);

  int total_faces = 0;
  int total_loops = 0;
  Array<int> offset_data(selection.size() + 1);
  selection.foreach_index_optimized<int>([&](const int index, const int i_selection) {
    const int count = counts[index];
    offset_data[i_selection] = total_faces;
    total_faces += count;
    total_loops += count * faces[index].size();
  });
  offset_data[selection.size()] = total_faces;

  const OffsetIndices<int> duplicates(offset_data);

  Mesh *new_mesh = BKE_mesh_new_nomain(total_loops, total_loops, total_faces, total_loops);
  MutableSpan<int2> new_edges = new_mesh->edges_for_write();
  MutableSpan<int> new_face_offsets = new_mesh->face_offsets_for_write();
  MutableSpan<int> new_corner_verts = new_mesh->corner_verts_for_write();
  MutableSpan<int> new_corner_edges = new_mesh->corner_edges_for_write();

  Array<int> vert_mapping(new_mesh->verts_num);
  Array<int> edge_mapping(new_edges.size());
  Array<int> loop_mapping(total_loops);

  int face_index = 0;
  int loop_index = 0;
  selection.foreach_index_optimized<int>([&](const int index, const int i_selection) {
    const IndexRange face_range = duplicates[i_selection];
    const IndexRange source = faces[index];
    for ([[maybe_unused]] const int i_duplicate : face_range.index_range()) {
      new_face_offsets[face_index] = loop_index;
      for (const int src_corner : source) {
        loop_mapping[loop_index] = src_corner;
        vert_mapping[loop_index] = corner_verts[src_corner];
        edge_mapping[loop_index] = corner_edges[src_corner];
        new_edges[loop_index][0] = loop_index;
        if (src_corner != source.last()) {
          new_edges[loop_index][1] = loop_index + 1;
        }
        else {
          new_edges[loop_index][1] = new_face_offsets[face_index];
        }
        loop_index++;
      }
      face_index++;
    }
  });
  array_utils::fill_index_range<int>(new_corner_verts);
  array_utils::fill_index_range<int>(new_corner_edges);

  new_mesh->tag_loose_verts_none();
  new_mesh->tag_loose_edges_none();
  new_mesh->tag_overlapping_none();

  copy_face_attributes_without_id(edge_mapping,
                                  vert_mapping,
                                  loop_mapping,
                                  duplicates,
                                  selection,
                                  propagation_info,
                                  mesh.attributes(),
                                  new_mesh->attributes_for_write());

  copy_stable_id_faces(mesh,
                       selection,
                       duplicates,
                       vert_mapping,
                       mesh.attributes(),
                       new_mesh->attributes_for_write());

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(new_mesh->attributes_for_write(),
                                     AttrDomain::Face,
                                     selection,
                                     attribute_outputs,
                                     duplicates);
  }

  geometry_set.replace_mesh(new_mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Edges
 * \{ */

/**
 * Copies the attributes for edge duplicates. If copying the edge domain, the attributes are
 * copied with an offset fill, for point domain a mapping is used.
 */
static void copy_edge_attributes_without_id(
    const Span<int> point_mapping,
    const OffsetIndices<int> offsets,
    const IndexMask &selection,
    const AnonymousAttributePropagationInfo &propagation_info,
    const bke::AttributeAccessor src_attributes,
    bke::MutableAttributeAccessor dst_attributes)
{
  for (auto &attribute :
       bke::retrieve_attributes_for_transfer(src_attributes,
                                             dst_attributes,
                                             ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_EDGE,
                                             propagation_info,
                                             {"id", ".edge_verts"}))
  {
    switch (attribute.meta_data.domain) {
      case AttrDomain::Edge:
        bke::attribute_math::gather_to_groups(
            offsets, selection, attribute.src, attribute.dst.span);
        break;
      case AttrDomain::Point:
        bke::attribute_math::gather(attribute.src, point_mapping, attribute.dst.span);
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
    attribute.dst.finish();
  }
}

/**
 * Copy the stable ids to the first duplicate and create new ids based on a hash of the original id
 * and the duplicate number. This function is used for points when duplicating the edge domain.
 */
static void copy_stable_id_edges(const Mesh &mesh,
                                 const IndexMask &selection,
                                 const OffsetIndices<int> offsets,
                                 const bke::AttributeAccessor src_attributes,
                                 bke::MutableAttributeAccessor dst_attributes)
{
  GAttributeReader src_attribute = src_attributes.lookup("id");
  if (!src_attribute) {
    return;
  }
  GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_only_span(
      "id", AttrDomain::Point, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  const Span<int2> edges = mesh.edges();

  VArraySpan<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.span.typed<int>();
  selection.foreach_index(GrainSize(1024), [&](const int64_t index, const int64_t i_selection) {
    const IndexRange edge_range = offsets[i_selection];
    if (edge_range.is_empty()) {
      return;
    }
    const int2 &edge = edges[index];
    const IndexRange vert_range = {edge_range.start() * 2, edge_range.size() * 2};

    dst[vert_range[0]] = src[edge[0]];
    dst[vert_range[1]] = src[edge[1]];
    for (const int i_duplicate : IndexRange(1, edge_range.size() - 1)) {
      dst[vert_range[i_duplicate * 2]] = noise::hash(src[edge[0]], i_duplicate);
      dst[vert_range[i_duplicate * 2 + 1]] = noise::hash(src[edge[1]], i_duplicate);
    }
  });
  dst_attribute.finish();
}

static void duplicate_edges(GeometrySet &geometry_set,
                            const Field<int> &count_field,
                            const Field<bool> &selection_field,
                            const IndexAttributes &attribute_outputs,
                            const AnonymousAttributePropagationInfo &propagation_info)
{
  if (!geometry_set.has_mesh()) {
    geometry_set.remove_geometry_during_modify();
    return;
  };
  const Mesh &mesh = *geometry_set.get_mesh();
  const Span<int2> edges = mesh.edges();

  const bke::MeshFieldContext field_context{mesh, AttrDomain::Edge};
  FieldEvaluator evaluator{field_context, edges.size()};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  Array<int> offset_data;
  const OffsetIndices<int> duplicates = accumulate_counts_to_offsets(
      selection, counts, offset_data);
  const int output_edges_num = duplicates.total_size();

  Mesh *new_mesh = BKE_mesh_new_nomain(output_edges_num * 2, output_edges_num, 0, 0);
  MutableSpan<int2> new_edges = new_mesh->edges_for_write();

  Array<int> vert_orig_indices(output_edges_num * 2);
  selection.foreach_index(GrainSize(1024), [&](const int64_t index, const int64_t i_selection) {
    const int2 &edge = edges[index];
    const IndexRange edge_range = duplicates[i_selection];
    const IndexRange vert_range(edge_range.start() * 2, edge_range.size() * 2);

    for (const int i_duplicate : IndexRange(edge_range.size())) {
      vert_orig_indices[vert_range[i_duplicate * 2]] = edge[0];
      vert_orig_indices[vert_range[i_duplicate * 2 + 1]] = edge[1];
    }
  });

  threading::parallel_for(selection.index_range(), 1024, [&](IndexRange range) {
    for (const int i_selection : range) {
      const IndexRange edge_range = duplicates[i_selection];
      const IndexRange vert_range(edge_range.start() * 2, edge_range.size() * 2);
      for (const int i_duplicate : IndexRange(edge_range.size())) {
        int2 &new_edge = new_edges[edge_range[i_duplicate]];
        new_edge[0] = vert_range[i_duplicate * 2];
        new_edge[1] = vert_range[i_duplicate * 2] + 1;
      }
    }
  });

  copy_edge_attributes_without_id(vert_orig_indices,
                                  duplicates,
                                  selection,
                                  propagation_info,
                                  mesh.attributes(),
                                  new_mesh->attributes_for_write());

  copy_stable_id_edges(
      mesh, selection, duplicates, mesh.attributes(), new_mesh->attributes_for_write());

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(new_mesh->attributes_for_write(),
                                     AttrDomain::Edge,
                                     selection,
                                     attribute_outputs,
                                     duplicates);
  }

  new_mesh->tag_overlapping_none();

  geometry_set.replace_mesh(new_mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Points (Curves)
 * \{ */

static void duplicate_points_curve(GeometrySet &geometry_set,
                                   const Field<int> &count_field,
                                   const Field<bool> &selection_field,
                                   const IndexAttributes &attribute_outputs,
                                   const AnonymousAttributePropagationInfo &propagation_info)
{
  const Curves &src_curves_id = *geometry_set.get_curves();
  const bke::CurvesGeometry &src_curves = src_curves_id.geometry.wrap();
  if (src_curves.points_num() == 0) {
    return;
  }

  const bke::CurvesFieldContext field_context{src_curves, AttrDomain::Point};
  FieldEvaluator evaluator{field_context, src_curves.points_num()};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  Array<int> offset_data;
  const OffsetIndices<int> duplicates = accumulate_counts_to_offsets(
      selection, counts, offset_data);
  const int dst_num = duplicates.total_size();

  const Array<int> point_to_curve_map = src_curves.point_to_curve_map();

  Curves *new_curves_id = bke::curves_new_nomain(dst_num, dst_num);
  bke::curves_copy_parameters(src_curves_id, *new_curves_id);
  bke::CurvesGeometry &new_curves = new_curves_id->geometry.wrap();
  offset_indices::fill_constant_group_size(1, 0, new_curves.offsets_for_write());

  bke::gather_attributes_to_groups(src_curves.attributes(),
                                   AttrDomain::Point,
                                   propagation_info,
                                   {},
                                   duplicates,
                                   selection,
                                   new_curves.attributes_for_write());

  for (auto &attribute : bke::retrieve_attributes_for_transfer(src_curves.attributes(),
                                                               new_curves.attributes_for_write(),
                                                               ATTR_DOMAIN_MASK_CURVE,
                                                               propagation_info,
                                                               {"id"}))
  {
    bke::attribute_math::convert_to_static_type(attribute.src.type(), [&](auto dummy) {
      using T = decltype(dummy);
      const Span<T> src = attribute.src.typed<T>();
      MutableSpan<T> dst = attribute.dst.span.typed<T>();
      selection.foreach_index(GrainSize(512), [&](const int64_t index, const int64_t i_selection) {
        const T &src_value = src[point_to_curve_map[index]];
        dst.slice(duplicates[i_selection]).fill(src_value);
      });
    });
    attribute.dst.finish();
  }

  copy_stable_id_point(duplicates, src_curves.attributes(), new_curves.attributes_for_write());

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(new_curves.attributes_for_write(),
                                     AttrDomain::Point,
                                     selection,
                                     attribute_outputs,
                                     duplicates);
  }

  geometry_set.replace_curves(new_curves_id);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Points (Mesh)
 * \{ */

static void duplicate_points_mesh(GeometrySet &geometry_set,
                                  const Field<int> &count_field,
                                  const Field<bool> &selection_field,
                                  const IndexAttributes &attribute_outputs,
                                  const AnonymousAttributePropagationInfo &propagation_info)
{
  const Mesh &mesh = *geometry_set.get_mesh();

  const bke::MeshFieldContext field_context{mesh, AttrDomain::Point};
  FieldEvaluator evaluator{field_context, mesh.verts_num};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  Array<int> offset_data;
  const OffsetIndices<int> duplicates = accumulate_counts_to_offsets(
      selection, counts, offset_data);

  Mesh *new_mesh = BKE_mesh_new_nomain(duplicates.total_size(), 0, 0, 0);

  bke::gather_attributes_to_groups(mesh.attributes(),
                                   AttrDomain::Point,
                                   propagation_info,
                                   {"id"},
                                   duplicates,
                                   selection,
                                   new_mesh->attributes_for_write());

  copy_stable_id_point(duplicates, mesh.attributes(), new_mesh->attributes_for_write());

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(new_mesh->attributes_for_write(),
                                     AttrDomain::Point,
                                     selection,
                                     attribute_outputs,
                                     duplicates);
  }

  new_mesh->tag_overlapping_none();

  geometry_set.replace_mesh(new_mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Points (Point Cloud)
 * \{ */

static void duplicate_points_pointcloud(GeometrySet &geometry_set,
                                        const Field<int> &count_field,
                                        const Field<bool> &selection_field,
                                        const IndexAttributes &attribute_outputs,
                                        const AnonymousAttributePropagationInfo &propagation_info)
{
  const PointCloud &src_points = *geometry_set.get_pointcloud();

  const bke::PointCloudFieldContext field_context{src_points};
  FieldEvaluator evaluator{field_context, src_points.totpoint};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  Array<int> offset_data;
  const OffsetIndices<int> duplicates = accumulate_counts_to_offsets(
      selection, counts, offset_data);

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(duplicates.total_size());

  bke::gather_attributes_to_groups(src_points.attributes(),
                                   AttrDomain::Point,
                                   propagation_info,
                                   {"id"},
                                   duplicates,
                                   selection,
                                   pointcloud->attributes_for_write());

  copy_stable_id_point(duplicates, src_points.attributes(), pointcloud->attributes_for_write());

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(pointcloud->attributes_for_write(),
                                     AttrDomain::Point,
                                     selection,
                                     attribute_outputs,
                                     duplicates);
  }
  geometry_set.replace_pointcloud(pointcloud);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Points
 * \{ */

static void duplicate_points(GeometrySet &geometry_set,
                             const Field<int> &count_field,
                             const Field<bool> &selection_field,
                             const IndexAttributes &attribute_outputs,
                             const AnonymousAttributePropagationInfo &propagation_info)
{
  Vector<GeometryComponent::Type> component_types = geometry_set.gather_component_types(true,
                                                                                        true);
  for (const GeometryComponent::Type component_type : component_types) {
    switch (component_type) {
      case GeometryComponent::Type::PointCloud:
        if (geometry_set.has_pointcloud()) {
          duplicate_points_pointcloud(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
        }
        break;
      case GeometryComponent::Type::Mesh:
        if (geometry_set.has_mesh()) {
          duplicate_points_mesh(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
        }
        break;
      case GeometryComponent::Type::Curve:
        if (geometry_set.has_curves()) {
          duplicate_points_curve(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
        }
        break;
      default:
        break;
    }
  }
  component_types.append(GeometryComponent::Type::Instance);
  geometry_set.keep_only_during_modify(component_types);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Instances
 * \{ */

static void duplicate_instances(GeometrySet &geometry_set,
                                const Field<int> &count_field,
                                const Field<bool> &selection_field,
                                const IndexAttributes &attribute_outputs,
                                const AnonymousAttributePropagationInfo &propagation_info)
{
  if (!geometry_set.has_instances()) {
    geometry_set.clear();
    return;
  }

  const bke::Instances &src_instances = *geometry_set.get_instances();

  bke::InstancesFieldContext field_context{src_instances};
  FieldEvaluator evaluator{field_context, src_instances.instances_num()};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);

  Array<int> offset_data;
  const OffsetIndices<int> duplicates = accumulate_counts_to_offsets(
      selection, counts, offset_data);
  if (duplicates.total_size() == 0) {
    geometry_set.clear();
    return;
  }

  std::unique_ptr<bke::Instances> dst_instances = std::make_unique<bke::Instances>();

  dst_instances->resize(duplicates.total_size());
  for (const int i_selection : selection.index_range()) {
    const IndexRange range = duplicates[i_selection];
    if (range.is_empty()) {
      continue;
    }
    const int old_handle = src_instances.reference_handles()[i_selection];
    const bke::InstanceReference reference = src_instances.references()[old_handle];
    const int new_handle = dst_instances->add_reference(reference);
    dst_instances->reference_handles_for_write().slice(range).fill(new_handle);
  }

  bke::gather_attributes_to_groups(src_instances.attributes(),
                                   AttrDomain::Instance,
                                   propagation_info,
                                   {"id", ".reference_index"},
                                   duplicates,
                                   selection,
                                   dst_instances->attributes_for_write());

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(dst_instances->attributes_for_write(),
                                     AttrDomain::Instance,
                                     selection,
                                     attribute_outputs,
                                     duplicates);
  }

  geometry_set = GeometrySet::from_instances(dst_instances.release());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Entry Point
 * \{ */

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  const NodeGeometryDuplicateElements &storage = node_storage(params.node());
  const AttrDomain duplicate_domain = AttrDomain(storage.domain);

  static auto max_zero_fn = mf::build::SI1_SO<int, int>(
      "max_zero",
      [](int value) { return std::max(0, value); },
      mf::build::exec_presets::AllSpanOrSingle());
  Field<int> count_field(
      FieldOperation::Create(max_zero_fn, {params.extract_input<Field<int>>("Amount")}));

  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  IndexAttributes attribute_outputs;
  attribute_outputs.duplicate_index = params.get_output_anonymous_attribute_id_if_needed(
      "Duplicate Index");

  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Geometry");

  if (duplicate_domain == AttrDomain::Instance) {
    duplicate_instances(
        geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
  }
  else {
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      switch (duplicate_domain) {
        case AttrDomain::Curve:
          duplicate_curves(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
          break;
        case AttrDomain::Face:
          duplicate_faces(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
          break;
        case AttrDomain::Edge:
          duplicate_edges(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
          break;
        case AttrDomain::Point:
          duplicate_points(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
    });
  }

  if (geometry_set.is_empty()) {
    params.set_default_remaining_outputs();
    return;
  }

  params.set_output("Geometry", std::move(geometry_set));
}

/** \} */

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem domain_items[] = {
      {int(AttrDomain::Point), "POINT", 0, "Point", ""},
      {int(AttrDomain::Edge), "EDGE", 0, "Edge", ""},
      {int(AttrDomain::Face), "FACE", 0, "Face", ""},
      {int(AttrDomain::Curve), "SPLINE", 0, "Spline", ""},
      {int(AttrDomain::Instance), "INSTANCE", 0, "Instance", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "Which domain to duplicate",
                    domain_items,
                    NOD_storage_enum_accessors(domain),
                    int(AttrDomain::Point),
                    nullptr,
                    true);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_DUPLICATE_ELEMENTS, "Duplicate Elements", NODE_CLASS_GEOMETRY);

  blender::bke::node_type_storage(&ntype,
                                  "NodeGeometryDuplicateElements",
                                  node_free_standard_storage,
                                  node_copy_standard_storage);

  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_duplicate_elements_cc
