/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_mix_geometries.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_instances.hh"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

namespace blender::geometry {

static bool sharing_info_equal(const ImplicitSharingInfo *a, const ImplicitSharingInfo *b)
{
  if (!a || !b) {
    return false;
  }
  return a == b;
}

template<typename T>
void mix_with_indices(MutableSpan<T> a,
                      const VArray<T> &b,
                      const Span<int> index_map,
                      const float factor)
{
  threading::parallel_for(a.index_range(), 1024, [&](const IndexRange range) {
    devirtualize_varray(b, [&](const auto b) {
      for (const int i : range) {
        if (index_map[i] != -1) {
          a[i] = bke::attribute_math::mix2(factor, a[i], b[index_map[i]]);
        }
      }
    });
  });
}

static void mix_with_indices(GMutableSpan a,
                             const GVArray &b,
                             const Span<int> index_map,
                             const float factor)
{
  bke::attribute_math::convert_to_static_type(a.type(), [&](auto dummy) {
    using T = decltype(dummy);
    mix_with_indices(a.typed<T>(), b.typed<T>(), index_map, factor);
  });
}

template<typename T> static void mix(MutableSpan<T> a, const VArray<T> &b, const float factor)
{
  threading::parallel_for(a.index_range(), 1024, [&](const IndexRange range) {
    devirtualize_varray(b, [&](const auto b) {
      for (const int i : range) {
        a[i] = bke::attribute_math::mix2(factor, a[i], b[i]);
      }
    });
  });
}

static void mix(GMutableSpan a, const GVArray &b, const float factor)
{
  bke::attribute_math::convert_to_static_type(a.type(), [&](auto dummy) {
    using T = decltype(dummy);
    mix(a.typed<T>(), b.typed<T>(), factor);
  });
}

static void mix_attributes(bke::MutableAttributeAccessor attributes_a,
                           const bke::AttributeAccessor b_attributes,
                           const Span<int> index_map,
                           const bke::AttrDomain mix_domain,
                           const float factor,
                           const Set<std::string> &names_to_skip = {})
{
  Set<StringRefNull> ids = attributes_a.all_ids();
  ids.remove("id");
  for (const StringRef name : names_to_skip) {
    ids.remove_as(name);
  }

  for (const StringRef id : ids) {
    const bke::GAttributeReader attribute_a = attributes_a.lookup(id);
    const bke::AttrDomain domain = attribute_a.domain;
    if (domain != mix_domain) {
      continue;
    }
    const bke::AttrType type = bke::cpp_type_to_attribute_type(attribute_a.varray.type());
    if (ELEM(type, bke::AttrType::String, bke::AttrType::Bool)) {
      /* String attributes can't be mixed, and there's no point in mixing boolean attributes. */
      continue;
    }
    const bke::GAttributeReader attribute_b = b_attributes.lookup(id, attribute_a.domain, type);
    if (sharing_info_equal(attribute_a.sharing_info, attribute_b.sharing_info)) {
      continue;
    }
    if (!index_map.is_empty()) {
      bke::GSpanAttributeWriter dst = attributes_a.lookup_for_write_span(id);
      /* If there's an ID attribute, use its values to mix with potentially changed indices. */
      mix_with_indices(dst.span, *attribute_b, index_map, factor);
      dst.finish();
    }
    else if (attributes_a.domain_size(domain) == b_attributes.domain_size(domain)) {
      bke::GSpanAttributeWriter dst = attributes_a.lookup_for_write_span(id);
      /* With no ID attribute to find matching elements, we can only support mixing when the domain
       * size (topology) is the same. Other options like mixing just the start of arrays might work
       * too, but give bad results too. */
      mix(dst.span, attribute_b.varray, factor);
      dst.finish();
    }
  }
}

static Map<int, int> create_value_to_first_index_map(const Span<int> values)
{
  Map<int, int> map;
  map.reserve(values.size());
  for (const int i : values.index_range()) {
    map.add(values[i], i);
  }
  return map;
}

static Array<int> create_id_index_map(const bke::AttributeAccessor attributes_a,
                                      const bke::AttributeAccessor b_attributes,
                                      const bke::AttrDomain id_domain)
{
  const bke::GAttributeReader ids_a = attributes_a.lookup("id");
  const bke::GAttributeReader ids_b = b_attributes.lookup("id");
  if (!ids_a || !ids_b) {
    return {};
  }
  if (!ids_a.varray.type().is<int>() || !ids_b.varray.type().is<int>()) {
    return {};
  }
  if (ids_a.domain != id_domain || ids_b.domain != id_domain) {
    return {};
  }
  if (sharing_info_equal(ids_a.sharing_info, ids_b.sharing_info)) {
    return {};
  }

  const VArraySpan ids_span_a(ids_a.varray.typed<int>());
  const VArraySpan ids_span_b(ids_b.varray.typed<int>());

  const Map<int, int> id_map_b = create_value_to_first_index_map(ids_span_b);
  Array<int> index_map(ids_span_a.size());
  threading::parallel_for(ids_span_a.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      index_map[i] = id_map_b.lookup_default(ids_span_a[i], -1);
    }
  });
  return index_map;
}

bke::GeometrySet mix_geometries(bke::GeometrySet a, const bke::GeometrySet &b, const float factor)
{
  if (Mesh *mesh_a = a.get_mesh_for_write()) {
    if (const Mesh *mesh_b = b.get_mesh()) {
      Array<int> vert_map = create_id_index_map(
          mesh_a->attributes(), mesh_b->attributes(), bke::AttrDomain::Point);
      mix_attributes(mesh_a->attributes_for_write(),
                     mesh_b->attributes(),
                     vert_map,
                     bke::AttrDomain::Point,
                     factor,
                     {});
    }
  }
  if (PointCloud *points_a = a.get_pointcloud_for_write()) {
    if (const PointCloud *points_b = b.get_pointcloud()) {
      const Array<int> index_map = create_id_index_map(
          points_a->attributes(), points_b->attributes(), bke::AttrDomain::Point);
      mix_attributes(points_a->attributes_for_write(),
                     points_b->attributes(),
                     index_map,
                     bke::AttrDomain::Point,
                     factor);
    }
  }
  if (Curves *curves_a = a.get_curves_for_write()) {
    if (const Curves *curves_b = b.get_curves()) {
      bke::MutableAttributeAccessor a = curves_a->geometry.wrap().attributes_for_write();
      const bke::AttributeAccessor b = curves_b->geometry.wrap().attributes();
      const Array<int> index_map = create_id_index_map(a, b, bke::AttrDomain::Point);
      mix_attributes(
          a,
          b,
          index_map,
          bke::AttrDomain::Point,
          factor,
          {"curve_type", "nurbs_order", "knots_mode", "handle_type_left", "handle_type_right"});
    }
  }
  if (bke::Instances *instances_a = a.get_instances_for_write()) {
    if (const bke::Instances *instances_b = b.get_instances()) {
      const Array<int> index_map = create_id_index_map(
          instances_a->attributes(), instances_b->attributes(), bke::AttrDomain::Instance);
      mix_attributes(instances_a->attributes_for_write(),
                     instances_b->attributes(),
                     index_map,
                     bke::AttrDomain::Instance,
                     factor,
                     {".reference_index"});
    }
  }
  return a;
}

}  // namespace blender::geometry
