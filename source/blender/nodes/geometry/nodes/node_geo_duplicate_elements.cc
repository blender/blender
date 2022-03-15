/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_map.hh"
#include "BLI_noise.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_mesh.h"
#include "BKE_pointcloud.h"
#include "BKE_spline.hh"

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_geo_duplicate_elements_cc {

NODE_STORAGE_FUNCS(NodeGeometryDuplicateElements);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Bool>(N_("Selection")).hide_value().default_value(true).supports_field();
  b.add_input<decl::Int>(N_("Amount"))
      .min(0)
      .default_value(1)
      .supports_field()
      .description(N_("The number of duplicates to create for each element"));

  b.add_output<decl::Geometry>(N_("Geometry"))
      .description(
          N_("The duplicated geometry only. The output does not contain the original geometry"));
  b.add_output<decl::Int>(N_("Duplicate Index"))
      .field_source()
      .description(N_("The indices of the duplicates for each element"));
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryDuplicateElements *data = MEM_cnew<NodeGeometryDuplicateElements>(__func__);
  data->domain = ATTR_DOMAIN_POINT;
  node->storage = data;
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
}

struct IndexAttributes {
  StrongAnonymousAttributeID duplicate_index;
};

/* -------------------------------------------------------------------- */
/** \name Attribute Copy/Creation Functions
 * \{ */

static void gather_attributes_without_id(const GeometrySet &geometry_set,
                                         const GeometryComponentType component_type,
                                         const Span<std::string> skip_attributes,
                                         const bool include_instances,
                                         Map<AttributeIDRef, AttributeKind> &r_gathered_attributes)
{
  geometry_set.gather_attributes_for_propagation(
      {component_type}, component_type, include_instances, r_gathered_attributes);
  for (const std::string &attribute : skip_attributes) {
    r_gathered_attributes.remove(attribute);
  }
  r_gathered_attributes.remove("id");
};

static IndexRange range_for_offsets_index(const Span<int> offsets, const int index)
{
  return {offsets[index], offsets[index + 1] - offsets[index]};
}

static Array<int> accumulate_counts_to_offsets(const IndexMask selection,
                                               const VArray<int> &counts)
{
  Array<int> offsets(selection.size() + 1);
  int dst_points_size = 0;
  for (const int i_point : selection.index_range()) {
    offsets[i_point] = dst_points_size;
    dst_points_size += std::max(counts[selection[i_point]], 0);
  }
  offsets.last() = dst_points_size;
  return offsets;
}

/* Utility functions for threaded copying of attribute data where possible. */
template<typename T>
static void threaded_slice_fill(Span<int> offsets, Span<T> src, MutableSpan<T> dst)
{
  BLI_assert(offsets.last() == dst.size());
  threading::parallel_for(IndexRange(offsets.size() - 1), 512, [&](IndexRange range) {
    for (const int i : range) {
      dst.slice(offsets[i], offsets[i + 1] - offsets[i]).fill(src[i]);
    }
  });
}

template<typename T>
static void threaded_mapped_copy(const Span<int> mapping, const Span<T> src, MutableSpan<T> dst)
{
  threading::parallel_for(mapping.index_range(), 512, [&](IndexRange range) {
    for (const int i : range) {
      dst[i] = src[mapping[i]];
    }
  });
}

static void threaded_id_offset_copy(const Span<int> offsets,
                                    const Span<int> src,
                                    MutableSpan<int> dst)
{
  BLI_assert(offsets.last() == dst.size());
  threading::parallel_for(IndexRange(offsets.size() - 1), 512, [&](IndexRange range) {
    for (const int i : range) {
      dst[offsets[i]] = src[i];
      const int count = offsets[i + 1] - offsets[i];
      for (const int i_duplicate : IndexRange(1, count - 1)) {
        dst[offsets[i] + i_duplicate] = noise::hash(src[i], i_duplicate);
      }
    }
  });
}

/** Create the copy indices for the duplication domain. */
static void create_duplicate_index_attribute(GeometryComponent &component,
                                             const AttributeDomain output_domain,
                                             const IndexMask selection,
                                             const IndexAttributes &attributes,
                                             const Span<int> offsets)
{
  OutputAttribute_Typed<int> copy_attribute = component.attribute_try_get_for_output_only<int>(
      attributes.duplicate_index.get(), output_domain);
  MutableSpan<int> duplicate_indices = copy_attribute.as_span();
  for (const int i : IndexRange(selection.size())) {
    const IndexRange range = range_for_offsets_index(offsets, i);
    MutableSpan<int> indices = duplicate_indices.slice(range);
    for (const int i : indices.index_range()) {
      indices[i] = i;
    }
  }
  copy_attribute.save();
}

/**
 * Copy the stable ids to the first duplicate and create new ids based on a hash of the original id
 * and the duplicate number. This function is used for the point domain elements.
 */
static void copy_stable_id_point(const Span<int> offsets,
                                 const GeometryComponent &src_component,
                                 GeometryComponent &dst_component)
{
  ReadAttributeLookup src_attribute = src_component.attribute_try_get_for_read("id");
  if (!src_attribute) {
    return;
  }
  OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
      "id", ATTR_DOMAIN_POINT, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  VArray_Span<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.as_span<int>();
  threaded_id_offset_copy(offsets, src, dst);
  dst_attribute.save();
}

/**
 * Copy the stable ids to the first duplicate and create new ids based on a hash of the original id
 * and the duplicate number. This function is used for points when duplicating the edge domain.
 */
static void copy_stable_id_edges(const Mesh &mesh,
                                 const IndexMask selection,
                                 const Span<int> edge_offsets,
                                 const GeometryComponent &src_component,
                                 GeometryComponent &dst_component)
{
  ReadAttributeLookup src_attribute = src_component.attribute_try_get_for_read("id");
  if (!src_attribute) {
    return;
  }
  OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
      "id", ATTR_DOMAIN_POINT, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  Span<MEdge> edges(mesh.medge, mesh.totedge);

  VArray_Span<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.as_span<int>();
  threading::parallel_for(IndexRange(selection.size()), 1024, [&](IndexRange range) {
    for (const int i_edge : range) {
      const IndexRange edge_range = range_for_offsets_index(edge_offsets, i_edge);
      if (edge_range.size() == 0) {
        continue;
      }
      const MEdge &edge = edges[i_edge];
      const IndexRange vert_range = {edge_range.start() * 2, edge_range.size() * 2};

      dst[vert_range[0]] = src[edge.v1];
      dst[vert_range[1]] = src[edge.v2];
      for (const int i_duplicate : IndexRange(1, edge_range.size() - 1)) {
        dst[vert_range[i_duplicate * 2]] = noise::hash(src[edge.v1], i_duplicate);
        dst[vert_range[i_duplicate * 2 + 1]] = noise::hash(src[edge.v2], i_duplicate);
      }
    }
  });
  dst_attribute.save();
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
                                 const Span<int> poly_offsets,
                                 const Span<int> vert_mapping,
                                 const GeometryComponent &src_component,
                                 GeometryComponent &dst_component)
{
  ReadAttributeLookup src_attribute = src_component.attribute_try_get_for_read("id");
  if (!src_attribute) {
    return;
  }
  OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
      "id", ATTR_DOMAIN_POINT, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  VArray_Span<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.as_span<int>();

  Span<MPoly> polys(mesh.mpoly, mesh.totpoly);
  int loop_index = 0;
  for (const int i_poly : selection.index_range()) {
    const IndexRange range = range_for_offsets_index(poly_offsets, i_poly);
    if (range.size() == 0) {
      continue;
    }
    const MPoly &source = polys[i_poly];
    for ([[maybe_unused]] const int i_duplicate : IndexRange(range.size())) {
      for ([[maybe_unused]] const int i_loops : IndexRange(source.totloop)) {
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

  dst_attribute.save();
}

/**
 * Copy the stable ids to the first duplicate and create new ids based on a hash of the original id
 * and the duplicate number. In the spline case, copy the entire spline's points to the
 * destination,
 * then loop over the remaining ones point by point, hashing their ids to the new ids.
 */
static void copy_stable_id_splines(const CurveEval &curve,
                                   const IndexMask selection,
                                   const Span<int> curve_offsets,
                                   const GeometryComponent &src_component,
                                   GeometryComponent &dst_component)
{
  ReadAttributeLookup src_attribute = src_component.attribute_try_get_for_read("id");
  if (!src_attribute) {
    return;
  }
  OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
      "id", ATTR_DOMAIN_POINT, CD_PROP_INT32);
  if (!dst_attribute) {
    return;
  }

  Array<int> control_point_offsets = curve.control_point_offsets();
  VArray_Span<int> src{src_attribute.varray.typed<int>()};
  MutableSpan<int> dst = dst_attribute.as_span<int>();

  Array<int> curve_point_offsets(selection.size() + 1);
  int dst_point_size = 0;
  for (const int i_curve : selection.index_range()) {
    const int spline_size = curve.splines()[i_curve]->size();
    const IndexRange curve_range = range_for_offsets_index(curve_offsets, i_curve);

    curve_point_offsets[i_curve] = dst_point_size;
    dst_point_size += curve_range.size() * spline_size;
  }
  curve_point_offsets.last() = dst_point_size;

  threading::parallel_for(IndexRange(curve_point_offsets.size() - 1), 512, [&](IndexRange range) {
    for (const int i_curve : range) {
      const int spline_size = curve.splines()[i_curve]->size();
      const IndexRange curve_range = range_for_offsets_index(curve_offsets, i_curve);

      dst.slice(curve_point_offsets[i_curve], spline_size)
          .copy_from(src.slice(control_point_offsets[i_curve], spline_size));
      for (const int i_duplicate : IndexRange(1, curve_range.size() - 1)) {
        for (const int i_point : IndexRange(spline_size)) {
          dst[curve_point_offsets[i_curve] + i_duplicate * spline_size + i_point] = noise::hash(
              src[control_point_offsets[i_curve] + i_point], i_duplicate);
        }
      }
    }
  });
  dst_attribute.save();
}

/* The attributes for the point (also instance) duplicated elements are stored sequentially
 * (1,1,1,2,2,2,3,3,3,etc) They can be copied by using a simple offset array. For each domain, if
 * elements are ordered differently a custom function is called to copy the attributes.
 */

static void copy_point_attributes_without_id(GeometrySet &geometry_set,
                                             const GeometryComponentType component_type,
                                             const bool include_instances,
                                             const Span<int> offsets,
                                             const GeometryComponent &src_component,
                                             GeometryComponent &dst_component)
{
  Map<AttributeIDRef, AttributeKind> gathered_attributes;
  gather_attributes_without_id(
      geometry_set, component_type, {}, include_instances, gathered_attributes);

  for (const Map<AttributeIDRef, AttributeKind>::Item entry : gathered_attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    ReadAttributeLookup src_attribute = src_component.attribute_try_get_for_read(attribute_id);
    if (!src_attribute || src_attribute.domain != ATTR_DOMAIN_POINT) {
      continue;
    }
    AttributeDomain out_domain = src_attribute.domain;
    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(
        src_attribute.varray.type());
    OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
        attribute_id, out_domain, data_type);
    if (!dst_attribute) {
      continue;
    }
    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArray_Span<T> src = src_attribute.varray.typed<T>();
      MutableSpan<T> dst = dst_attribute.as_span<T>();
      threaded_slice_fill<T>(offsets, src, dst);
    });
    dst_attribute.save();
  }
}

/**
 * Copies the attributes for spline duplicates. If copying the spline domain, the attributes are
 * copied with an offset fill, otherwise a mapping is used.
 */
static void copy_spline_attributes_without_id(const GeometrySet &geometry_set,
                                              const Span<int> point_mapping,
                                              const Span<int> offsets,
                                              const Span<std::string> attributes_to_ignore,
                                              const GeometryComponent &src_component,
                                              GeometryComponent &dst_component)
{
  Map<AttributeIDRef, AttributeKind> gathered_attributes;
  gather_attributes_without_id(
      geometry_set, GEO_COMPONENT_TYPE_CURVE, attributes_to_ignore, false, gathered_attributes);

  for (const Map<AttributeIDRef, AttributeKind>::Item entry : gathered_attributes.items()) {

    const AttributeIDRef attribute_id = entry.key;
    ReadAttributeLookup src_attribute = src_component.attribute_try_get_for_read(attribute_id);
    if (!src_attribute) {
      continue;
    }

    AttributeDomain out_domain = src_attribute.domain;
    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(
        src_attribute.varray.type());
    OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
        attribute_id, out_domain, data_type);
    if (!dst_attribute) {
      continue;
    }

    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArray_Span<T> src{src_attribute.varray.typed<T>()};
      MutableSpan<T> dst = dst_attribute.as_span<T>();

      switch (out_domain) {
        case ATTR_DOMAIN_CURVE:
          threaded_slice_fill<T>(offsets, src, dst);
          break;
        case ATTR_DOMAIN_POINT:
          threaded_mapped_copy<T>(point_mapping, src, dst);
          break;
        default:
          break;
      }
    });
    dst_attribute.save();
  }
}

/**
 * Copies the attributes for edge duplicates. If copying the edge domain, the attributes are
 * copied with an offset fill, for point domain a mapping is used.
 */
static void copy_edge_attributes_without_id(GeometrySet &geometry_set,
                                            const Span<int> point_mapping,
                                            const Span<int> offsets,
                                            const GeometryComponent &src_component,
                                            GeometryComponent &dst_component)
{
  Map<AttributeIDRef, AttributeKind> gathered_attributes;
  gather_attributes_without_id(
      geometry_set, GEO_COMPONENT_TYPE_MESH, {}, false, gathered_attributes);

  for (const Map<AttributeIDRef, AttributeKind>::Item entry : gathered_attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    ReadAttributeLookup src_attribute = src_component.attribute_try_get_for_read(attribute_id);
    if (!src_attribute) {
      continue;
    }

    const AttributeDomain out_domain = src_attribute.domain;
    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(
        src_attribute.varray.type());
    OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
        attribute_id, out_domain, data_type);
    if (!dst_attribute) {
      continue;
    }
    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArray_Span<T> src{src_attribute.varray.typed<T>()};
      MutableSpan<T> dst = dst_attribute.as_span<T>();

      switch (out_domain) {
        case ATTR_DOMAIN_EDGE:
          threaded_slice_fill<T>(offsets, src, dst);
          break;
        case ATTR_DOMAIN_POINT:
          threaded_mapped_copy<T>(point_mapping, src, dst);
          break;
        default:
          break;
      }
    });
    dst_attribute.save();
  }
}

/**
 * Copies the attributes for face duplicates. If copying the face domain, the attributes are
 * copied with an offset fill, otherwise a mapping is used.
 */
static void copy_face_attributes_without_id(GeometrySet &geometry_set,
                                            const Span<int> edge_mapping,
                                            const Span<int> vert_mapping,
                                            const Span<int> loop_mapping,
                                            const Span<int> offsets,
                                            const GeometryComponent &src_component,
                                            GeometryComponent &dst_component)
{
  Map<AttributeIDRef, AttributeKind> gathered_attributes;
  gather_attributes_without_id(
      geometry_set, GEO_COMPONENT_TYPE_MESH, {}, false, gathered_attributes);

  for (const Map<AttributeIDRef, AttributeKind>::Item entry : gathered_attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    ReadAttributeLookup src_attribute = src_component.attribute_try_get_for_read(attribute_id);
    if (!src_attribute) {
      continue;
    }

    AttributeDomain out_domain = src_attribute.domain;
    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(
        src_attribute.varray.type());
    OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
        attribute_id, out_domain, data_type);
    if (!dst_attribute) {
      continue;
    }

    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArray_Span<T> src{src_attribute.varray.typed<T>()};
      MutableSpan<T> dst = dst_attribute.as_span<T>();

      switch (out_domain) {
        case ATTR_DOMAIN_FACE:
          threaded_slice_fill<T>(offsets, src, dst);
          break;
        case ATTR_DOMAIN_EDGE:
          threaded_mapped_copy<T>(edge_mapping, src, dst);
          break;
        case ATTR_DOMAIN_POINT:
          threaded_mapped_copy<T>(vert_mapping, src, dst);
          break;
        case ATTR_DOMAIN_CORNER:
          threaded_mapped_copy<T>(loop_mapping, src, dst);
          break;
        default:
          break;
      }
    });
    dst_attribute.save();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplication Functions
 * \{ */

static void duplicate_splines(GeometrySet &geometry_set,
                              const Field<int> &count_field,
                              const Field<bool> &selection_field,
                              IndexAttributes &attributes)
{
  if (!geometry_set.has_curves()) {
    geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
    return;
  }
  geometry_set.keep_only({GEO_COMPONENT_TYPE_CURVE, GEO_COMPONENT_TYPE_INSTANCES});

  const GeometryComponent &src_component = *geometry_set.get_component_for_read(
      GEO_COMPONENT_TYPE_CURVE);
  const std::unique_ptr<CurveEval> curve = curves_to_curve_eval(
      *geometry_set.get_curves_for_read());
  const int domain_size = src_component.attribute_domain_size(ATTR_DOMAIN_CURVE);
  GeometryComponentFieldContext field_context{src_component, ATTR_DOMAIN_CURVE};
  FieldEvaluator evaluator{field_context, domain_size};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  Array<int> curve_offsets(selection.size() + 1);

  int dst_splines_size = 0;
  int dst_points_size = 0;
  for (const int i_spline : selection.index_range()) {
    int count = std::max(counts[selection[i_spline]], 0);
    curve_offsets[i_spline] = dst_splines_size;
    dst_splines_size += count;
    dst_points_size += count * curve->splines()[selection[i_spline]]->size();
  }
  curve_offsets.last() = dst_splines_size;

  Array<int> control_point_offsets = curve->control_point_offsets();
  Array<int> point_mapping(dst_points_size);

  std::unique_ptr<CurveEval> new_curve = std::make_unique<CurveEval>();
  int point_index = 0;
  for (const int i_spline : selection.index_range()) {
    const IndexRange spline_range = range_for_offsets_index(curve_offsets, i_spline);
    for ([[maybe_unused]] const int i_duplicate : IndexRange(spline_range.size())) {
      SplinePtr spline = curve->splines()[selection[i_spline]]->copy();
      for (const int i_point : IndexRange(curve->splines()[selection[i_spline]]->size())) {
        point_mapping[point_index++] = control_point_offsets[selection[i_spline]] + i_point;
      }
      new_curve->add_spline(std::move(spline));
    }
  }
  new_curve->attributes.reallocate(new_curve->splines().size());

  CurveComponent dst_component;
  dst_component.replace(curve_eval_to_curves(*new_curve), GeometryOwnershipType::Editable);

  Vector<std::string> skip(
      {"position", "radius", "resolution", "cyclic", "tilt", "handle_left", "handle_right"});

  copy_spline_attributes_without_id(
      geometry_set, point_mapping, curve_offsets, skip, src_component, dst_component);

  copy_stable_id_splines(*curve, selection, curve_offsets, src_component, dst_component);

  if (attributes.duplicate_index) {
    create_duplicate_index_attribute(
        dst_component, ATTR_DOMAIN_CURVE, selection, attributes, curve_offsets);
  }

  geometry_set.replace_curves(dst_component.get_for_write());
}

static void duplicate_faces(GeometrySet &geometry_set,
                            const Field<int> &count_field,
                            const Field<bool> &selection_field,
                            IndexAttributes &attributes)
{
  if (!geometry_set.has_mesh()) {
    geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
    return;
  }
  geometry_set.keep_only({GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_INSTANCES});

  GeometryComponent &component = geometry_set.get_component_for_write(GEO_COMPONENT_TYPE_MESH);
  const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_FACE);

  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_FACE};
  FieldEvaluator evaluator(field_context, domain_size);

  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);

  MeshComponent &mesh_component = static_cast<MeshComponent &>(component);
  const Mesh &mesh = *mesh_component.get_for_read();
  Span<MVert> verts(mesh.mvert, mesh.totvert);
  Span<MEdge> edges(mesh.medge, mesh.totedge);
  Span<MPoly> polys(mesh.mpoly, mesh.totpoly);
  Span<MLoop> loops(mesh.mloop, mesh.totloop);

  int total_polys = 0;
  int total_loops = 0;
  Array<int> offsets(selection.size() + 1);
  for (const int i_selection : selection.index_range()) {
    const int count = std::max(counts[selection[i_selection]], 0);
    offsets[i_selection] = total_polys;
    total_polys += count;
    total_loops += count * polys[selection[i_selection]].totloop;
  }
  offsets[selection.size()] = total_polys;

  Array<int> vert_mapping(total_loops);
  Array<int> edge_mapping(total_loops);
  Array<int> loop_mapping(total_loops);

  Mesh *new_mesh = BKE_mesh_new_nomain(total_loops, total_loops, 0, total_loops, total_polys);

  MutableSpan<MVert> new_verts(new_mesh->mvert, new_mesh->totvert);
  MutableSpan<MEdge> new_edges(new_mesh->medge, new_mesh->totedge);
  MutableSpan<MLoop> new_loops(new_mesh->mloop, new_mesh->totloop);
  MutableSpan<MPoly> new_poly(new_mesh->mpoly, new_mesh->totpoly);

  int poly_index = 0;
  int loop_index = 0;
  for (const int i_selection : selection.index_range()) {
    const IndexRange poly_range = range_for_offsets_index(offsets, i_selection);

    const MPoly &source = polys[selection[i_selection]];
    for ([[maybe_unused]] const int i_duplicate : IndexRange(poly_range.size())) {
      new_poly[poly_index] = source;
      new_poly[poly_index].loopstart = loop_index;
      for (const int i_loops : IndexRange(source.totloop)) {
        const MLoop &current_loop = loops[source.loopstart + i_loops];
        loop_mapping[loop_index] = source.loopstart + i_loops;
        new_verts[loop_index] = verts[current_loop.v];
        vert_mapping[loop_index] = current_loop.v;
        new_edges[loop_index] = edges[current_loop.e];
        edge_mapping[loop_index] = current_loop.e;
        new_edges[loop_index].v1 = loop_index;
        if (i_loops + 1 != source.totloop) {
          new_edges[loop_index].v2 = loop_index + 1;
        }
        else {
          new_edges[loop_index].v2 = new_poly[poly_index].loopstart;
        }
        new_loops[loop_index].v = loop_index;
        new_loops[loop_index].e = loop_index;
        loop_index++;
      }
      poly_index++;
    }
  }
  MeshComponent dst_component;
  dst_component.replace(new_mesh, GeometryOwnershipType::Editable);

  copy_face_attributes_without_id(geometry_set,
                                  edge_mapping,
                                  vert_mapping,
                                  loop_mapping,
                                  offsets,
                                  mesh_component,
                                  dst_component);

  copy_stable_id_faces(mesh, selection, offsets, vert_mapping, mesh_component, dst_component);
  mesh_component.replace(dst_component.get_for_write());

  if (attributes.duplicate_index) {
    create_duplicate_index_attribute(
        dst_component, ATTR_DOMAIN_FACE, selection, attributes, offsets);
  }
}

static void duplicate_edges(GeometrySet &geometry_set,
                            const Field<int> &count_field,
                            const Field<bool> &selection_field,
                            IndexAttributes &attributes)
{
  if (!geometry_set.has_mesh()) {
    geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
    return;
  };
  const GeometryComponent &src_component = *geometry_set.get_component_for_read(
      GEO_COMPONENT_TYPE_MESH);
  const int domain_size = src_component.attribute_domain_size(ATTR_DOMAIN_EDGE);

  GeometryComponentFieldContext field_context{src_component, ATTR_DOMAIN_EDGE};
  FieldEvaluator evaluator{field_context, domain_size};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  Array<int> edge_offsets = accumulate_counts_to_offsets(selection, counts);

  const Mesh *mesh = geometry_set.get_mesh_for_read();
  Span<MVert> verts(mesh->mvert, mesh->totvert);
  Span<MEdge> edges(mesh->medge, mesh->totedge);

  Mesh *new_mesh = BKE_mesh_new_nomain(edge_offsets.last() * 2, edge_offsets.last(), 0, 0, 0);
  MutableSpan<MVert> new_verts(new_mesh->mvert, new_mesh->totvert);
  MutableSpan<MEdge> new_edges(new_mesh->medge, new_mesh->totedge);

  Array<int> vert_orig_indices(edge_offsets.last() * 2);
  threading::parallel_for(selection.index_range(), 1024, [&](IndexRange range) {
    for (const int i_edge : range) {
      const MEdge &edge = edges[i_edge];
      const IndexRange edge_range = range_for_offsets_index(edge_offsets, i_edge);
      const IndexRange vert_range(edge_range.start() * 2, edge_range.size() * 2);

      for (const int i_duplicate : IndexRange(edge_range.size())) {
        vert_orig_indices[vert_range[i_duplicate * 2]] = edge.v1;
        vert_orig_indices[vert_range[i_duplicate * 2 + 1]] = edge.v2;
      }
    }
  });

  threading::parallel_for(selection.index_range(), 1024, [&](IndexRange range) {
    for (const int i_edge : range) {
      const IndexRange edge_range = range_for_offsets_index(edge_offsets, i_edge);
      const IndexRange vert_range(edge_range.start() * 2, edge_range.size() * 2);
      for (const int i_duplicate : IndexRange(edge_range.size())) {
        MEdge &new_edge = new_edges[edge_range[i_duplicate]];
        new_edge.v1 = vert_range[i_duplicate * 2];
        new_edge.v2 = vert_range[i_duplicate * 2] + 1;
      }
    }
  });

  MeshComponent dst_component;
  dst_component.replace(new_mesh, GeometryOwnershipType::Editable);

  copy_edge_attributes_without_id(
      geometry_set, vert_orig_indices, edge_offsets, src_component, dst_component);

  copy_stable_id_edges(*mesh, selection, edge_offsets, src_component, dst_component);

  if (attributes.duplicate_index) {
    create_duplicate_index_attribute(
        dst_component, ATTR_DOMAIN_EDGE, selection, attributes, edge_offsets);
  }

  MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
  mesh_component.replace(dst_component.get_for_write());
}

static void duplicate_points_curve(const GeometryComponentType component_type,
                                   const Field<int> &count_field,
                                   const Field<bool> &selection_field,
                                   GeometrySet &geometry_set,
                                   IndexAttributes &attributes)
{
  const GeometryComponent &src_component = *geometry_set.get_component_for_read(component_type);
  const int domain_size = src_component.attribute_domain_size(ATTR_DOMAIN_POINT);
  if (domain_size == 0) {
    return;
  }

  GeometryComponentFieldContext field_context{src_component, ATTR_DOMAIN_POINT};
  FieldEvaluator evaluator{field_context, domain_size};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  Array<int> offsets = accumulate_counts_to_offsets(selection, counts);

  CurveComponent &curve_component = geometry_set.get_component_for_write<CurveComponent>();
  const std::unique_ptr<CurveEval> curve = curves_to_curve_eval(
      *geometry_set.get_curves_for_read());
  Array<int> control_point_offsets = curve->control_point_offsets();
  std::unique_ptr<CurveEval> new_curve = std::make_unique<CurveEval>();

  Array<int> parent(domain_size);
  int spline = 0;
  for (const int i_spline : IndexRange(domain_size)) {
    if (i_spline == control_point_offsets[spline + 1]) {
      spline++;
    }
    parent[i_spline] = spline;
  }

  for (const int i_point : selection) {
    const IndexRange point_range = range_for_offsets_index(offsets, i_point);
    for ([[maybe_unused]] const int i_duplicate : IndexRange(point_range.size())) {
      const SplinePtr &parent_spline = curve->splines()[parent[i_point]];
      switch (parent_spline->type()) {
        case CurveType::CURVE_TYPE_BEZIER: {
          std::unique_ptr<BezierSpline> spline = std::make_unique<BezierSpline>();
          spline->resize(1);
          spline->set_resolution(2);
          new_curve->add_spline(std::move(spline));
          break;
        }
        case CurveType::CURVE_TYPE_NURBS: {
          std::unique_ptr<NURBSpline> spline = std::make_unique<NURBSpline>();
          spline->resize(1);
          spline->set_resolution(2);
          new_curve->add_spline(std::move(spline));
          break;
        }
        case CurveType::CURVE_TYPE_POLY: {
          std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();
          spline->resize(1);
          new_curve->add_spline(std::move(spline));
          break;
        }
        case CurveType::CURVE_TYPE_CATMULL_ROM: {
          /* Catmull Rom curves are not supported yet. */
          break;
        }
      }
    }
  }
  new_curve->attributes.reallocate(new_curve->splines().size());
  CurveComponent dst_component;
  dst_component.replace(curve_eval_to_curves(*new_curve), GeometryOwnershipType::Editable);

  copy_point_attributes_without_id(
      geometry_set, GEO_COMPONENT_TYPE_CURVE, false, offsets, src_component, dst_component);

  copy_stable_id_point(offsets, src_component, dst_component);

  if (attributes.duplicate_index) {
    create_duplicate_index_attribute(
        dst_component, ATTR_DOMAIN_POINT, selection, attributes, offsets.as_span());
  }

  curve_component.replace(dst_component.get_for_write());
}

static void duplicate_points_mesh(const GeometryComponentType component_type,
                                  const Field<int> &count_field,
                                  const Field<bool> &selection_field,
                                  GeometrySet &geometry_set,
                                  IndexAttributes &attributes)
{
  const GeometryComponent &src_component = *geometry_set.get_component_for_read(component_type);
  const int domain_size = src_component.attribute_domain_size(ATTR_DOMAIN_POINT);

  GeometryComponentFieldContext field_context{src_component, ATTR_DOMAIN_POINT};
  FieldEvaluator evaluator{field_context, domain_size};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  Array<int> offsets = accumulate_counts_to_offsets(selection, counts);

  const Mesh *mesh = geometry_set.get_mesh_for_read();
  Span<MVert> src_verts(mesh->mvert, mesh->totvert);

  Mesh *new_mesh = BKE_mesh_new_nomain(offsets.last(), 0, 0, 0, 0);
  MutableSpan<MVert> dst_verts(new_mesh->mvert, new_mesh->totvert);

  threaded_slice_fill<MVert>(offsets.as_span(), src_verts, dst_verts);

  MeshComponent dst_component;
  dst_component.replace(new_mesh, GeometryOwnershipType::Editable);
  copy_point_attributes_without_id(
      geometry_set, GEO_COMPONENT_TYPE_MESH, false, offsets, src_component, dst_component);

  copy_stable_id_point(offsets, src_component, dst_component);

  if (attributes.duplicate_index) {
    create_duplicate_index_attribute(
        dst_component, ATTR_DOMAIN_POINT, selection, attributes, offsets.as_span());
  }

  MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
  mesh_component.replace(dst_component.get_for_write());
}

static void duplicate_points_pointcloud(const GeometryComponentType component_type,
                                        const Field<int> &count_field,
                                        const Field<bool> &selection_field,
                                        GeometrySet &geometry_set,
                                        IndexAttributes &attributes)
{
  const GeometryComponent &src_component = *geometry_set.get_component_for_read(component_type);
  const int domain_size = src_component.attribute_domain_size(ATTR_DOMAIN_POINT);

  GeometryComponentFieldContext field_context{src_component, ATTR_DOMAIN_POINT};
  FieldEvaluator evaluator{field_context, domain_size};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  Array<int> offsets = accumulate_counts_to_offsets(selection, counts);

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(offsets.last());
  PointCloudComponent dst_component;
  dst_component.replace(pointcloud, GeometryOwnershipType::Editable);

  copy_point_attributes_without_id(
      geometry_set, GEO_COMPONENT_TYPE_POINT_CLOUD, false, offsets, src_component, dst_component);

  copy_stable_id_point(offsets, src_component, dst_component);

  if (attributes.duplicate_index) {
    create_duplicate_index_attribute(
        dst_component, ATTR_DOMAIN_POINT, selection, attributes, offsets);
  }
  geometry_set.replace_pointcloud(pointcloud);
}

static void duplicate_points(GeometrySet &geometry_set,
                             const Field<int> &count_field,
                             const Field<bool> &selection_field,
                             IndexAttributes &attributes)
{
  if (!geometry_set.has_mesh() && !geometry_set.has_curves() && !geometry_set.has_pointcloud()) {
    geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
    return;
  }

  Vector<GeometryComponentType> component_types = geometry_set.gather_component_types(true, true);
  Vector<GeometryComponentType> types_to_keep;
  for (const GeometryComponentType component_type : component_types) {
    switch (component_type) {
      case GEO_COMPONENT_TYPE_POINT_CLOUD:
        types_to_keep.append(component_type);
        duplicate_points_pointcloud(
            component_type, count_field, selection_field, geometry_set, attributes);
        break;
      case GEO_COMPONENT_TYPE_MESH:
        types_to_keep.append(component_type);
        duplicate_points_mesh(
            component_type, count_field, selection_field, geometry_set, attributes);
        break;
      case GEO_COMPONENT_TYPE_CURVE:
        types_to_keep.append(component_type);
        duplicate_points_curve(
            component_type, count_field, selection_field, geometry_set, attributes);
        break;
      default:
        break;
    }
  }
  types_to_keep.append(GEO_COMPONENT_TYPE_INSTANCES);
  geometry_set.keep_only(types_to_keep);
}

static void duplicate_instances(GeometrySet &geometry_set,
                                const Field<int> &count_field,
                                const Field<bool> &selection_field,
                                IndexAttributes &attributes)
{
  if (!geometry_set.has_instances()) {
    geometry_set.clear();
    return;
  }

  const InstancesComponent &src_instances =
      *geometry_set.get_component_for_read<InstancesComponent>();

  const int domain_size = src_instances.attribute_domain_size(ATTR_DOMAIN_INSTANCE);
  GeometryComponentFieldContext field_context{src_instances, ATTR_DOMAIN_INSTANCE};
  FieldEvaluator evaluator{field_context, domain_size};
  evaluator.add(count_field);
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<int> counts = evaluator.get_evaluated<int>(0);

  Array<int> offsets = accumulate_counts_to_offsets(selection, counts);

  if (offsets.last() == 0) {
    geometry_set.clear();
    return;
  }

  GeometrySet instances_geometry;
  InstancesComponent &dst_instances =
      instances_geometry.get_component_for_write<InstancesComponent>();
  dst_instances.resize(offsets.last());
  for (const int i_selection : selection.index_range()) {
    const int count = offsets[i_selection + 1] - offsets[i_selection];
    if (count == 0) {
      continue;
    }
    const int old_handle = src_instances.instance_reference_handles()[i_selection];
    const InstanceReference reference = src_instances.references()[old_handle];
    const int new_handle = dst_instances.add_reference(reference);
    const float4x4 transform = src_instances.instance_transforms()[i_selection];
    dst_instances.instance_transforms().slice(offsets[i_selection], count).fill(transform);
    dst_instances.instance_reference_handles().slice(offsets[i_selection], count).fill(new_handle);
  }

  copy_point_attributes_without_id(
      geometry_set, GEO_COMPONENT_TYPE_INSTANCES, true, offsets, src_instances, dst_instances);

  if (attributes.duplicate_index) {
    create_duplicate_index_attribute(
        dst_instances, ATTR_DOMAIN_INSTANCE, selection, attributes, offsets);
  }

  geometry_set.remove(GEO_COMPONENT_TYPE_INSTANCES);
  geometry_set.add(dst_instances);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  const NodeGeometryDuplicateElements &storage = node_storage(params.node());
  const AttributeDomain duplicate_domain = AttributeDomain(storage.domain);

  Field<int> count_field = params.extract_input<Field<int>>("Amount");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  IndexAttributes attributes;
  if (params.output_is_required("Duplicate Index")) {
    attributes.duplicate_index = StrongAnonymousAttributeID("duplicate_index");
  }

  if (duplicate_domain == ATTR_DOMAIN_INSTANCE) {
    geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
    duplicate_instances(geometry_set, count_field, selection_field, attributes);
  }
  else {
    if (geometry_set.is_empty()) {
      params.set_default_remaining_outputs();
      return;
    }
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      switch (duplicate_domain) {
        case ATTR_DOMAIN_CURVE:
          duplicate_splines(geometry_set, count_field, selection_field, attributes);
          break;
        case ATTR_DOMAIN_FACE:
          duplicate_faces(geometry_set, count_field, selection_field, attributes);
          break;
        case ATTR_DOMAIN_EDGE:
          duplicate_edges(geometry_set, count_field, selection_field, attributes);
          break;
        case ATTR_DOMAIN_POINT:
          duplicate_points(geometry_set, count_field, selection_field, attributes);
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

  if (attributes.duplicate_index) {
    params.set_output(
        "Duplicate Index",
        AnonymousAttributeFieldInput::Create<int>(std::move(attributes.duplicate_index),
                                                  params.attribute_producer_name()));
  }
  params.set_output("Geometry", geometry_set);
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

  node_type_init(&ntype, file_ns::node_init);
  ntype.draw_buttons = file_ns::node_layout;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
