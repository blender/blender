/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "GEO_join_geometries.hh"
#include "GEO_realize_instances.hh"

#include "BKE_instances.hh"

namespace blender::geometry {

using bke::AttributeIDRef;
using bke::AttributeMetaData;
using bke::GeometryComponent;
using bke::GeometrySet;

static Map<AttributeIDRef, AttributeMetaData> get_final_attribute_info(
    const Span<const GeometryComponent *> components, const Span<StringRef> ignored_attributes)
{
  Map<AttributeIDRef, AttributeMetaData> info;

  for (const GeometryComponent *component : components) {
    component->attributes()->for_all(
        [&](const bke::AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
          if (ignored_attributes.contains(attribute_id.name())) {
            return true;
          }
          if (meta_data.data_type == CD_PROP_STRING) {
            return true;
          }
          info.add_or_modify(
              attribute_id,
              [&](AttributeMetaData *meta_data_final) { *meta_data_final = meta_data; },
              [&](AttributeMetaData *meta_data_final) {
                meta_data_final->data_type = bke::attribute_data_type_highest_complexity(
                    {meta_data_final->data_type, meta_data.data_type});
                meta_data_final->domain = bke::attribute_domain_highest_priority(
                    {meta_data_final->domain, meta_data.domain});
              });
          return true;
        });
  }

  return info;
}

static void fill_new_attribute(const Span<const GeometryComponent *> src_components,
                               const AttributeIDRef &attribute_id,
                               const eCustomDataType data_type,
                               const bke::AttrDomain domain,
                               GMutableSpan dst_span)
{
  const CPPType *cpp_type = bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);

  int offset = 0;
  for (const GeometryComponent *component : src_components) {
    const int domain_num = component->attribute_domain_size(domain);
    if (domain_num == 0) {
      continue;
    }
    GVArray read_attribute = *component->attributes()->lookup_or_default(
        attribute_id, domain, data_type, nullptr);

    GVArraySpan src_span{read_attribute};
    const void *src_buffer = src_span.data();
    void *dst_buffer = dst_span[offset];
    cpp_type->copy_assign_n(src_buffer, dst_buffer, domain_num);

    offset += domain_num;
  }
}

static void join_attributes(const Span<const GeometryComponent *> src_components,
                            GeometryComponent &result,
                            const Span<StringRef> ignored_attributes = {})
{
  const Map<AttributeIDRef, AttributeMetaData> info = get_final_attribute_info(src_components,
                                                                               ignored_attributes);

  for (const MapItem<AttributeIDRef, AttributeMetaData> item : info.items()) {
    const AttributeIDRef attribute_id = item.key;
    const AttributeMetaData &meta_data = item.value;

    bke::GSpanAttributeWriter write_attribute =
        result.attributes_for_write()->lookup_or_add_for_write_only_span(
            attribute_id, meta_data.domain, meta_data.data_type);
    if (!write_attribute) {
      continue;
    }
    fill_new_attribute(
        src_components, attribute_id, meta_data.data_type, meta_data.domain, write_attribute.span);
    write_attribute.finish();
  }
}

static void join_instances(const Span<const GeometryComponent *> src_components,
                           GeometrySet &result)
{
  Array<int> offsets_data(src_components.size() + 1);
  for (const int i : src_components.index_range()) {
    const auto &src_component = static_cast<const bke::InstancesComponent &>(*src_components[i]);
    offsets_data[i] = src_component.get()->instances_num();
  }
  const OffsetIndices offsets = offset_indices::accumulate_counts_to_offsets(offsets_data);

  std::unique_ptr<bke::Instances> dst_instances = std::make_unique<bke::Instances>();
  dst_instances->resize(offsets.total_size());

  MutableSpan<int> all_handles = dst_instances->reference_handles_for_write();

  for (const int i : src_components.index_range()) {
    const auto &src_component = static_cast<const bke::InstancesComponent &>(*src_components[i]);
    const bke::Instances &src_instances = *src_component.get();

    const Span<bke::InstanceReference> src_references = src_instances.references();
    Array<int> handle_map(src_references.size());
    for (const int src_handle : src_references.index_range()) {
      handle_map[src_handle] = dst_instances->add_reference(src_references[src_handle]);
    }

    const IndexRange dst_range = offsets[i];

    const Span<int> src_handles = src_instances.reference_handles();
    array_utils::gather(handle_map.as_span(), src_handles, all_handles.slice(dst_range));
  }

  result.replace_instances(dst_instances.release());
  auto &dst_component = result.get_component_for_write<bke::InstancesComponent>();
  join_attributes(src_components, dst_component, {".reference_index"});
}

static void join_volumes(const Span<const GeometryComponent *> /*src_components*/,
                         GeometrySet & /*result*/)
{
  /* Not yet supported. Joining volume grids with the same name requires resampling of at least one
   * of the grids. The cell size of the resulting volume has to be determined somehow. */
}

static void join_component_type(const bke::GeometryComponent::Type component_type,
                                const Span<GeometrySet> src_geometry_sets,
                                const bke::AnonymousAttributePropagationInfo &propagation_info,
                                GeometrySet &result)
{
  Vector<const GeometryComponent *> components;
  for (const GeometrySet &geometry_set : src_geometry_sets) {
    const GeometryComponent *component = geometry_set.get_component(component_type);
    if (component != nullptr && !component->is_empty()) {
      components.append(component);
    }
  }

  if (components.is_empty()) {
    return;
  }
  if (components.size() == 1) {
    result.add(*components.first());
    return;
  }

  switch (component_type) {
    case bke::GeometryComponent::Type::Instance:
      join_instances(components, result);
      return;
    case bke::GeometryComponent::Type::Volume:
      join_volumes(components, result);
      return;
    default:
      break;
  }

  std::unique_ptr<bke::Instances> instances = std::make_unique<bke::Instances>();
  instances->resize(components.size());
  instances->transforms_for_write().fill(float4x4::identity());
  MutableSpan<int> handles = instances->reference_handles_for_write();
  for (const int i : components.index_range()) {
    GeometrySet tmp_geo;
    tmp_geo.add(*components[i]);
    handles[i] = instances->add_reference(bke::InstanceReference{tmp_geo});
  }

  RealizeInstancesOptions options;
  options.keep_original_ids = true;
  options.realize_instance_attributes = false;
  options.propagation_info = propagation_info;
  GeometrySet joined_components = realize_instances(
      GeometrySet::from_instances(instances.release()), options);
  result.add(joined_components.get_component_for_write(component_type));
}

GeometrySet join_geometries(const Span<GeometrySet> geometries,
                            const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  GeometrySet result;
  static const Array<GeometryComponent::Type> supported_types({GeometryComponent::Type::Mesh,
                                                               GeometryComponent::Type::PointCloud,
                                                               GeometryComponent::Type::Instance,
                                                               GeometryComponent::Type::Volume,
                                                               GeometryComponent::Type::Curve,
                                                               GeometryComponent::Type::Edit});
  for (const GeometryComponent::Type type : supported_types) {
    join_component_type(type, geometries, propagation_info, result);
  }

  return result;
}

}  // namespace blender::geometry
