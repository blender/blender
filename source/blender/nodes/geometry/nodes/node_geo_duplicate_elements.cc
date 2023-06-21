/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_map.hh"
#include "BLI_noise.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.h"

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_geo_duplicate_elements_cc {

NODE_STORAGE_FUNCS(NodeGeometryDuplicateElements);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection").hide_value().default_value(true).field_on_all();
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
  data->domain = ATTR_DOMAIN_POINT;
  node->storage = data;
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
}

struct IndexAttributes {
  AnonymousAttributeIDPtr duplicate_index;
};

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

static OffsetIndices<int> accumulate_counts_to_offsets(const IndexMask selection,
                                                       const VArray<int> &counts,
                                                       Array<int> &r_offset_data)
{
  r_offset_data.reinitialize(selection.size() + 1);
  if (counts.is_single()) {
    const int count = counts.get_internal_single();
    threading::parallel_for(selection.index_range(), 1024, [&](const IndexRange range) {
      for (const int64_t i : range) {
        r_offset_data[i] = count * i;
      }
    });
    r_offset_data.last() = count * selection.size();
  }
  else {
    threading::parallel_for(selection.index_range(), 1024, [&](const IndexRange range) {
      counts.materialize_compressed(selection.slice(range),
                                    r_offset_data.as_mutable_span().slice(range));
    });
    offset_indices::accumulate_counts_to_offsets(r_offset_data);
  }
  return OffsetIndices<int>(r_offset_data);
}

/* Utility functions for threaded copying of attribute data where possible. */
template<typename T>
static void threaded_slice_fill(const OffsetIndices<int> offsets,
                                const IndexMask selection,
                                const Span<T> src,
                                MutableSpan<T> dst)
{
  BLI_assert(offsets.total_size() == dst.size());
  threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
    for (const int i : range) {
      dst.slice(offsets[i]).fill(src[selection[i]]);
    }
  });
}

static void threaded_slice_fill(const OffsetIndices<int> offsets,
                                const IndexMask selection,
                                const GSpan src,
                                GMutableSpan dst)
{
  bke::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    threaded_slice_fill<T>(offsets, selection, src.typed<T>(), dst.typed<T>());
  });
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
                                             const eAttrDomain output_domain,
                                             const IndexMask selection,
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
      "id", ATTR_DOMAIN_POINT, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  VArraySpan<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.span.typed<int>();
  threaded_id_offset_copy(offsets, src, dst);
  dst_attribute.finish();
}

static void copy_attributes_without_id(const OffsetIndices<int> offsets,
                                       const IndexMask selection,
                                       const AnonymousAttributePropagationInfo &propagation_info,
                                       const eAttrDomain domain,
                                       const bke::AttributeAccessor src_attributes,
                                       bke::MutableAttributeAccessor dst_attributes)
{
  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes, dst_attributes, ATTR_DOMAIN_AS_MASK(domain), propagation_info, {"id"}))
  {
    threaded_slice_fill(offsets, selection, attribute.src, attribute.dst.span);
    attribute.dst.finish();
  }
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
    const IndexMask selection,
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
      case ATTR_DOMAIN_CURVE:
        threaded_slice_fill(curve_offsets, selection, attribute.src, attribute.dst.span);
        break;
      case ATTR_DOMAIN_POINT:
        bke::attribute_math::convert_to_static_type(attribute.src.type(), [&](auto dummy) {
          using T = decltype(dummy);
          const Span<T> src = attribute.src.typed<T>();
          MutableSpan<T> dst = attribute.dst.span.typed<T>();
          threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
            for (const int i_selection : range) {
              const int i_src_curve = selection[i_selection];
              const Span<T> curve_src = src.slice(src_points_by_curve[i_src_curve]);
              for (const int i_dst_curve : curve_offsets[i_selection]) {
                dst.slice(dst_points_by_curve[i_dst_curve]).copy_from(curve_src);
              }
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
                                  const IndexMask selection,
                                  const OffsetIndices<int> offsets,
                                  bke::CurvesGeometry &dst_curves)
{
  GAttributeReader src_attribute = src_curves.attributes().lookup("id");
  if (!src_attribute) {
    return;
  }
  GSpanAttributeWriter dst_attribute =
      dst_curves.attributes_for_write().lookup_or_add_for_write_only_span(
          "id", ATTR_DOMAIN_POINT, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  VArraySpan<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.span.typed<int>();

  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
    for (const int i_selection : range) {
      const int i_src_curve = selection[i_selection];
      const Span<int> curve_src = src.slice(src_points_by_curve[i_src_curve]);
      const IndexRange duplicates_range = offsets[i_selection];
      for (const int i_duplicate : IndexRange(offsets[i_selection].size()).drop_front(1)) {
        const int i_dst_curve = duplicates_range[i_duplicate];
        copy_hashed_ids(curve_src, i_duplicate, dst.slice(dst_points_by_curve[i_dst_curve]));
      }
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
  geometry_set.keep_only_during_modify({GEO_COMPONENT_TYPE_CURVE});
  GeometryComponentEditData::remember_deformed_curve_positions_if_necessary(geometry_set);

  const Curves &curves_id = *geometry_set.get_curves_for_read();
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();

  const bke::CurvesFieldContext field_context{curves, ATTR_DOMAIN_CURVE};
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
  for (const int i_curve : selection.index_range()) {
    const int count = counts[selection[i_curve]];
    curve_offset_data[i_curve] = dst_curves_num;
    point_offset_data[i_curve] = dst_points_num;
    dst_curves_num += count;
    dst_points_num += count * points_by_curve[selection[i_curve]].size();
  }

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

  threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
    for (const int i_selection : range) {
      const int i_src_curve = selection[i_selection];
      const IndexRange src_curve_range = points_by_curve[i_src_curve];
      const IndexRange dst_curves_range = curve_offsets[i_selection];
      MutableSpan<int> dst_offsets = all_dst_offsets.slice(dst_curves_range);
      for (const int i_duplicate : IndexRange(dst_curves_range.size())) {
        dst_offsets[i_duplicate] = point_offsets[i_selection].start() +
                                   src_curve_range.size() * i_duplicate;
      }
    }
  });
  all_dst_offsets.last() = dst_points_num;

  copy_curve_attributes_without_id(curves, selection, curve_offsets, propagation_info, new_curves);

  copy_stable_id_curves(curves, selection, curve_offsets, new_curves);

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(new_curves.attributes_for_write(),
                                     ATTR_DOMAIN_CURVE,
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
    const IndexMask selection,
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
      case ATTR_DOMAIN_POINT:
        bke::attribute_math::gather(attribute.src, vert_mapping, attribute.dst.span);
        break;
      case ATTR_DOMAIN_EDGE:
        bke::attribute_math::gather(attribute.src, edge_mapping, attribute.dst.span);
        break;
      case ATTR_DOMAIN_FACE:
        threaded_slice_fill(offsets, selection, attribute.src, attribute.dst.span);
        break;
      case ATTR_DOMAIN_CORNER:
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
                                 const IndexMask selection,
                                 const OffsetIndices<int> poly_offsets,
                                 const Span<int> vert_mapping,
                                 const bke::AttributeAccessor src_attributes,
                                 bke::MutableAttributeAccessor dst_attributes)
{
  GAttributeReader src_attribute = src_attributes.lookup("id");
  if (!src_attribute) {
    return;
  }
  GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_only_span(
      "id", ATTR_DOMAIN_POINT, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  VArraySpan<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.span.typed<int>();

  const OffsetIndices polys = mesh.polys();
  int loop_index = 0;
  for (const int i_poly : selection.index_range()) {
    const IndexRange range = poly_offsets[i_poly];
    if (range.size() == 0) {
      continue;
    }
    const IndexRange source = polys[i_poly];
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
  geometry_set.keep_only_during_modify({GEO_COMPONENT_TYPE_MESH});

  const Mesh &mesh = *geometry_set.get_mesh_for_read();
  const Span<int2> edges = mesh.edges();
  const OffsetIndices polys = mesh.polys();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int> corner_edges = mesh.corner_edges();

  const bke::MeshFieldContext field_context{mesh, ATTR_DOMAIN_FACE};
  FieldEvaluator evaluator(field_context, polys.size());
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);

  int total_polys = 0;
  int total_loops = 0;
  Array<int> offset_data(selection.size() + 1);
  for (const int i_selection : selection.index_range()) {
    const int count = counts[selection[i_selection]];
    offset_data[i_selection] = total_polys;
    total_polys += count;
    total_loops += count * polys[selection[i_selection]].size();
  }
  offset_data[selection.size()] = total_polys;

  const OffsetIndices<int> duplicates(offset_data);

  Mesh *new_mesh = BKE_mesh_new_nomain(total_loops, total_loops, total_polys, total_loops);
  MutableSpan<int2> new_edges = new_mesh->edges_for_write();
  MutableSpan<int> new_poly_offsets = new_mesh->poly_offsets_for_write();
  MutableSpan<int> new_corner_verts = new_mesh->corner_verts_for_write();
  MutableSpan<int> new_corner_edges = new_mesh->corner_edges_for_write();

  Array<int> vert_mapping(new_mesh->totvert);
  Array<int> edge_mapping(new_edges.size());
  Array<int> loop_mapping(total_loops);

  int poly_index = 0;
  int loop_index = 0;
  for (const int i_selection : selection.index_range()) {
    const IndexRange poly_range = duplicates[i_selection];

    const IndexRange source = polys[selection[i_selection]];
    for ([[maybe_unused]] const int i_duplicate : IndexRange(poly_range.size())) {
      new_poly_offsets[poly_index] = loop_index;
      for (const int i_loops : IndexRange(source.size())) {
        const int src_corner = source[i_loops];
        loop_mapping[loop_index] = src_corner;
        vert_mapping[loop_index] = corner_verts[src_corner];
        new_edges[loop_index] = edges[corner_edges[src_corner]];
        edge_mapping[loop_index] = corner_edges[src_corner];
        new_edges[loop_index][0] = loop_index;
        if (i_loops + 1 != source.size()) {
          new_edges[loop_index][1] = loop_index + 1;
        }
        else {
          new_edges[loop_index][1] = new_poly_offsets[poly_index];
        }
        new_corner_verts[loop_index] = loop_index;
        new_corner_edges[loop_index] = loop_index;
        loop_index++;
      }
      poly_index++;
    }
  }

  new_mesh->tag_loose_verts_none();
  new_mesh->loose_edges_tag_none();

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
                                     ATTR_DOMAIN_FACE,
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
    const IndexMask selection,
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
      case ATTR_DOMAIN_EDGE:
        threaded_slice_fill(offsets, selection, attribute.src, attribute.dst.span);
        break;
      case ATTR_DOMAIN_POINT:
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
                                 const IndexMask selection,
                                 const OffsetIndices<int> offsets,
                                 const bke::AttributeAccessor src_attributes,
                                 bke::MutableAttributeAccessor dst_attributes)
{
  GAttributeReader src_attribute = src_attributes.lookup("id");
  if (!src_attribute) {
    return;
  }
  GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_only_span(
      "id", ATTR_DOMAIN_POINT, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  const Span<int2> edges = mesh.edges();

  VArraySpan<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.span.typed<int>();
  threading::parallel_for(IndexRange(selection.size()), 1024, [&](IndexRange range) {
    for (const int i_selection : range) {
      const IndexRange edge_range = offsets[i_selection];
      if (edge_range.size() == 0) {
        continue;
      }
      const int2 &edge = edges[selection[i_selection]];
      const IndexRange vert_range = {edge_range.start() * 2, edge_range.size() * 2};

      dst[vert_range[0]] = src[edge[0]];
      dst[vert_range[1]] = src[edge[1]];
      for (const int i_duplicate : IndexRange(1, edge_range.size() - 1)) {
        dst[vert_range[i_duplicate * 2]] = noise::hash(src[edge[0]], i_duplicate);
        dst[vert_range[i_duplicate * 2 + 1]] = noise::hash(src[edge[1]], i_duplicate);
      }
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
  const Mesh &mesh = *geometry_set.get_mesh_for_read();
  const Span<int2> edges = mesh.edges();

  const bke::MeshFieldContext field_context{mesh, ATTR_DOMAIN_EDGE};
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
  threading::parallel_for(selection.index_range(), 1024, [&](IndexRange range) {
    for (const int i_selection : range) {
      const int2 &edge = edges[selection[i_selection]];
      const IndexRange edge_range = duplicates[i_selection];
      const IndexRange vert_range(edge_range.start() * 2, edge_range.size() * 2);

      for (const int i_duplicate : IndexRange(edge_range.size())) {
        vert_orig_indices[vert_range[i_duplicate * 2]] = edge[0];
        vert_orig_indices[vert_range[i_duplicate * 2 + 1]] = edge[1];
      }
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
                                     ATTR_DOMAIN_EDGE,
                                     selection,
                                     attribute_outputs,
                                     duplicates);
  }

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
  const Curves &src_curves_id = *geometry_set.get_curves_for_read();
  const bke::CurvesGeometry &src_curves = src_curves_id.geometry.wrap();
  if (src_curves.points_num() == 0) {
    return;
  }

  const bke::CurvesFieldContext field_context{src_curves, ATTR_DOMAIN_POINT};
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
  MutableSpan<int> new_curve_offsets = new_curves.offsets_for_write();
  std::iota(new_curve_offsets.begin(), new_curve_offsets.end(), 0);

  for (auto &attribute : bke::retrieve_attributes_for_transfer(src_curves.attributes(),
                                                               new_curves.attributes_for_write(),
                                                               ATTR_DOMAIN_MASK_ALL,
                                                               propagation_info,
                                                               {"id"}))
  {
    switch (attribute.meta_data.domain) {
      case ATTR_DOMAIN_CURVE:
        bke::attribute_math::convert_to_static_type(attribute.src.type(), [&](auto dummy) {
          using T = decltype(dummy);
          const Span<T> src = attribute.src.typed<T>();
          MutableSpan<T> dst = attribute.dst.span.typed<T>();
          threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
            for (const int i_selection : range) {
              const T &src_value = src[point_to_curve_map[selection[i_selection]]];
              dst.slice(duplicates[i_selection]).fill(src_value);
            }
          });
        });
        break;
      case ATTR_DOMAIN_POINT:
        threaded_slice_fill(duplicates, selection, attribute.src, attribute.dst.span);
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
    attribute.dst.finish();
  }

  copy_stable_id_point(duplicates, src_curves.attributes(), new_curves.attributes_for_write());

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(new_curves.attributes_for_write(),
                                     ATTR_DOMAIN_POINT,
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
  const Mesh &mesh = *geometry_set.get_mesh_for_read();

  const bke::MeshFieldContext field_context{mesh, ATTR_DOMAIN_POINT};
  FieldEvaluator evaluator{field_context, mesh.totvert};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  Array<int> offset_data;
  const OffsetIndices<int> duplicates = accumulate_counts_to_offsets(
      selection, counts, offset_data);

  Mesh *new_mesh = BKE_mesh_new_nomain(duplicates.total_size(), 0, 0, 0);

  copy_attributes_without_id(duplicates,
                             selection,
                             propagation_info,
                             ATTR_DOMAIN_POINT,
                             mesh.attributes(),
                             new_mesh->attributes_for_write());

  copy_stable_id_point(duplicates, mesh.attributes(), new_mesh->attributes_for_write());

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(new_mesh->attributes_for_write(),
                                     ATTR_DOMAIN_POINT,
                                     selection,
                                     attribute_outputs,
                                     duplicates);
  }

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
  const PointCloud &src_points = *geometry_set.get_pointcloud_for_read();

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

  copy_attributes_without_id(duplicates,
                             selection,
                             propagation_info,
                             ATTR_DOMAIN_POINT,
                             src_points.attributes(),
                             pointcloud->attributes_for_write());

  copy_stable_id_point(duplicates, src_points.attributes(), pointcloud->attributes_for_write());

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(pointcloud->attributes_for_write(),
                                     ATTR_DOMAIN_POINT,
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
  Vector<GeometryComponentType> component_types = geometry_set.gather_component_types(true, true);
  for (const GeometryComponentType component_type : component_types) {
    switch (component_type) {
      case GEO_COMPONENT_TYPE_POINT_CLOUD:
        if (geometry_set.has_pointcloud()) {
          duplicate_points_pointcloud(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
        }
        break;
      case GEO_COMPONENT_TYPE_MESH:
        if (geometry_set.has_mesh()) {
          duplicate_points_mesh(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
        }
        break;
      case GEO_COMPONENT_TYPE_CURVE:
        if (geometry_set.has_curves()) {
          duplicate_points_curve(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
        }
        break;
      default:
        break;
    }
  }
  component_types.append(GEO_COMPONENT_TYPE_INSTANCES);
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

  const bke::Instances &src_instances = *geometry_set.get_instances_for_read();

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
    if (range.size() == 0) {
      continue;
    }
    const int old_handle = src_instances.reference_handles()[i_selection];
    const bke::InstanceReference reference = src_instances.references()[old_handle];
    const int new_handle = dst_instances->add_reference(reference);
    const float4x4 transform = src_instances.transforms()[i_selection];
    dst_instances->transforms().slice(range).fill(transform);
    dst_instances->reference_handles().slice(range).fill(new_handle);
  }

  copy_attributes_without_id(duplicates,
                             selection,
                             propagation_info,
                             ATTR_DOMAIN_INSTANCE,
                             src_instances.attributes(),
                             dst_instances->attributes_for_write());

  if (attribute_outputs.duplicate_index) {
    create_duplicate_index_attribute(dst_instances->attributes_for_write(),
                                     ATTR_DOMAIN_INSTANCE,
                                     selection,
                                     attribute_outputs,
                                     duplicates);
  }

  geometry_set = GeometrySet::create_with_instances(dst_instances.release());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Entry Point
 * \{ */

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  const NodeGeometryDuplicateElements &storage = node_storage(params.node());
  const eAttrDomain duplicate_domain = eAttrDomain(storage.domain);

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

  if (duplicate_domain == ATTR_DOMAIN_INSTANCE) {
    duplicate_instances(
        geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
  }
  else {
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      switch (duplicate_domain) {
        case ATTR_DOMAIN_CURVE:
          duplicate_curves(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
          break;
        case ATTR_DOMAIN_FACE:
          duplicate_faces(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
          break;
        case ATTR_DOMAIN_EDGE:
          duplicate_edges(
              geometry_set, count_field, selection_field, attribute_outputs, propagation_info);
          break;
        case ATTR_DOMAIN_POINT:
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

}  // namespace blender::nodes::node_geo_duplicate_elements_cc

void register_node_type_geo_duplicate_elements()
{
  namespace file_ns = blender::nodes::node_geo_duplicate_elements_cc;
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_DUPLICATE_ELEMENTS, "Duplicate Elements", NODE_CLASS_GEOMETRY);

  node_type_storage(&ntype,
                    "NodeGeometryDuplicateElements",
                    node_free_standard_storage,
                    node_copy_standard_storage);

  ntype.initfunc = file_ns::node_init;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
