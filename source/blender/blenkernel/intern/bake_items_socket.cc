/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_bake_items_socket.hh"

#include "BKE_geometry_fields.hh"

#include "BKE_node.hh"

#include "FN_field_cpp_type.hh"

namespace blender::bke {

static const CPPType &get_socket_cpp_type(const eNodeSocketDatatype socket_type)
{
  const char *socket_idname = nodeStaticSocketType(socket_type, 0);
  const bNodeSocketType *typeinfo = nodeSocketTypeFind(socket_idname);
  BLI_assert(typeinfo);
  BLI_assert(typeinfo->geometry_nodes_cpp_type);
  return *typeinfo->geometry_nodes_cpp_type;
}

Array<std::unique_ptr<BakeItem>> move_socket_values_to_bake_items(const Span<void *> socket_values,
                                                                  const BakeSocketConfig &config)
{
  BLI_assert(socket_values.size() == config.types.size());
  BLI_assert(socket_values.size() == config.geometries_by_attribute.size());

  Array<std::unique_ptr<BakeItem>> bake_items(socket_values.size());

  /* Create geometry bake items first because they are used for field evaluation. */
  for (const int i : socket_values.index_range()) {
    const eNodeSocketDatatype socket_type = config.types[i];
    if (socket_type != SOCK_GEOMETRY) {
      continue;
    }
    void *socket_value = socket_values[i];
    GeometrySet &geometry = *static_cast<GeometrySet *>(socket_value);
    bake_items[i] = std::make_unique<GeometryBakeItem>(std::move(geometry));
  }

  for (const int i : socket_values.index_range()) {
    const eNodeSocketDatatype socket_type = config.types[i];
    void *socket_value = socket_values[i];
    switch (socket_type) {
      case SOCK_GEOMETRY: {
        /* Handled already. */
        break;
      }
      case SOCK_STRING: {
        const fn::ValueOrField<std::string> &value =
            *static_cast<const fn::ValueOrField<std::string> *>(socket_value);
        bake_items[i] = std::make_unique<StringBakeItem>(value.as_value());
        break;
      }
      case SOCK_FLOAT:
      case SOCK_VECTOR:
      case SOCK_INT:
      case SOCK_BOOLEAN:
      case SOCK_ROTATION:
      case SOCK_RGBA: {
        const CPPType &type = get_socket_cpp_type(socket_type);
        const fn::ValueOrFieldCPPType &value_or_field_type =
            *fn::ValueOrFieldCPPType::get_from_self(type);
        const CPPType &base_type = value_or_field_type.value;
        if (!value_or_field_type.is_field(socket_value)) {
          const void *value = value_or_field_type.get_value_ptr(socket_value);
          bake_items[i] = std::make_unique<PrimitiveBakeItem>(base_type, value);
          break;
        }
        const fn::GField &field = *value_or_field_type.get_field_ptr(socket_value);
        if (!field.node().depends_on_input()) {
          BUFFER_FOR_CPP_TYPE_VALUE(base_type, value);
          fn::evaluate_constant_field(field, value);
          bake_items[i] = std::make_unique<PrimitiveBakeItem>(base_type, value);
          base_type.destruct(value);
          break;
        }
        const eAttrDomain domain = config.domains[i];
        const std::string attribute_name = ".bake_" + std::to_string(i);
        const Span<int> geometry_indices = config.geometries_by_attribute[i];
        for (const int geometry_i : geometry_indices) {
          BLI_assert(config.types[geometry_i] == SOCK_GEOMETRY);
          GeometrySet &geometry =
              static_cast<GeometryBakeItem *>(bake_items[geometry_i].get())->geometry;
          if (geometry.has_pointcloud()) {
            PointCloudComponent &component =
                geometry.get_component_for_write<PointCloudComponent>();
            try_capture_field_on_geometry(component, attribute_name, domain, field);
          }
          if (geometry.has_mesh()) {
            MeshComponent &component = geometry.get_component_for_write<MeshComponent>();
            try_capture_field_on_geometry(component, attribute_name, domain, field);
          }
          if (geometry.has_curves()) {
            CurveComponent &component = geometry.get_component_for_write<CurveComponent>();
            try_capture_field_on_geometry(component, attribute_name, domain, field);
          }
          if (geometry.has_instances()) {
            InstancesComponent &component = geometry.get_component_for_write<InstancesComponent>();
            try_capture_field_on_geometry(component, attribute_name, domain, field);
          }
        }
        bake_items[i] = std::make_unique<AttributeBakeItem>(attribute_name);
        break;
      }
      default:
        break;
    }
  }

  /* Cleanup geometries after fields have been evaluated. */
  for (const int i : config.types.index_range()) {
    const eNodeSocketDatatype socket_type = config.types[i];
    if (socket_type != SOCK_GEOMETRY) {
      continue;
    }
    GeometrySet &geometry = static_cast<GeometryBakeItem *>(bake_items[i].get())->geometry;
    GeometryBakeItem::cleanup_geometry(geometry);
  }

  return bake_items;
}

[[nodiscard]] static bool copy_bake_item_to_socket_value(
    const BakeItem &bake_item,
    const eNodeSocketDatatype socket_type,
    const FunctionRef<std::shared_ptr<AnonymousAttributeFieldInput>(const CPPType &type)>
        make_attribute_field,
    Map<std::string, AnonymousAttributeIDPtr> &r_attribute_map,
    void *r_value)
{
  const CPPType &type = get_socket_cpp_type(socket_type);
  switch (socket_type) {
    case SOCK_GEOMETRY: {
      if (const auto *item = dynamic_cast<const GeometryBakeItem *>(&bake_item)) {
        new (r_value) GeometrySet(item->geometry);
        return true;
      }
      return false;
    }
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_INT:
    case SOCK_BOOLEAN:
    case SOCK_ROTATION:
    case SOCK_RGBA: {
      const fn::ValueOrFieldCPPType &value_or_field_type = *fn::ValueOrFieldCPPType::get_from_self(
          type);
      const CPPType &base_type = value_or_field_type.value;
      if (const auto *item = dynamic_cast<const PrimitiveBakeItem *>(&bake_item)) {
        if (item->type() == base_type) {
          value_or_field_type.construct_from_value(r_value, item->value());
          return true;
        }
        return false;
      }
      if (const auto *item = dynamic_cast<const AttributeBakeItem *>(&bake_item)) {
        std::shared_ptr<AnonymousAttributeFieldInput> attribute_field = make_attribute_field(
            base_type);
        const AnonymousAttributeIDPtr &attribute_id = attribute_field->anonymous_id();
        fn::GField field{attribute_field};
        value_or_field_type.construct_from_field(r_value, std::move(field));
        r_attribute_map.add(item->name(), attribute_id);
        return true;
      }
      return false;
    }
    case SOCK_STRING: {
      if (const auto *item = dynamic_cast<const StringBakeItem *>(&bake_item)) {
        new (r_value) fn::ValueOrField<std::string>(item->value());
        return true;
      }
      return false;
    }
    default:
      return false;
  }
  return false;
}

static void rename_attributes(const Span<GeometrySet *> geometries,
                              const Map<std::string, AnonymousAttributeIDPtr> &attribute_map)
{
  for (GeometrySet *geometry : geometries) {
    for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                               GeometryComponent::Type::Curve,
                                               GeometryComponent::Type::PointCloud,
                                               GeometryComponent::Type::Instance})
    {
      if (!geometry->has(type)) {
        continue;
      }
      /* Avoid write access on the geometry when unnecessary to avoid copying data-blocks. */
      const AttributeAccessor attributes_read_only = *geometry->get_component(type)->attributes();
      if (std::none_of(attribute_map.keys().begin(),
                       attribute_map.keys().end(),
                       [&](const StringRef name) { return attributes_read_only.contains(name); }))
      {
        continue;
      }

      GeometryComponent &component = geometry->get_component_for_write(type);
      MutableAttributeAccessor attributes = *component.attributes_for_write();
      for (const MapItem<std::string, AnonymousAttributeIDPtr> &attribute_item :
           attribute_map.items()) {
        attributes.rename(attribute_item.key, *attribute_item.value);
      }
    }
  }
}

void move_bake_items_to_socket_values(
    const Span<BakeItem *> bake_items,
    const BakeSocketConfig &config,
    FunctionRef<std::shared_ptr<AnonymousAttributeFieldInput>(int, const CPPType &)>
        make_attribute_field,
    const Span<void *> r_socket_values)
{
  Map<std::string, AnonymousAttributeIDPtr> attribute_map;

  Vector<GeometrySet *> geometries;

  for (const int i : bake_items.index_range()) {
    const eNodeSocketDatatype socket_type = config.types[i];
    const CPPType &type = get_socket_cpp_type(socket_type);
    BakeItem *bake_item = bake_items[i];
    void *r_socket_value = r_socket_values[i];
    if (bake_item == nullptr) {
      type.value_initialize(r_socket_value);
      continue;
    }
    if (!copy_bake_item_to_socket_value(
            *bake_item,
            socket_type,
            [&](const CPPType &attr_type) { return make_attribute_field(i, attr_type); },
            attribute_map,
            r_socket_value))
    {
      type.value_initialize(r_socket_value);
      continue;
    }
    if (socket_type == SOCK_GEOMETRY) {
      auto &item = *static_cast<GeometryBakeItem *>(bake_item);
      item.geometry.clear();
      geometries.append(static_cast<GeometrySet *>(r_socket_value));
    }
  }

  rename_attributes(geometries, attribute_map);
}

void copy_bake_items_to_socket_values(
    const Span<const BakeItem *> bake_items,
    const BakeSocketConfig &config,
    FunctionRef<std::shared_ptr<AnonymousAttributeFieldInput>(int, const CPPType &)>
        make_attribute_field,
    const Span<void *> r_socket_values)
{
  Map<std::string, AnonymousAttributeIDPtr> attribute_map;
  Vector<GeometrySet *> geometries;

  for (const int i : bake_items.index_range()) {
    const eNodeSocketDatatype socket_type = config.types[i];
    const CPPType &type = get_socket_cpp_type(socket_type);
    const BakeItem *bake_item = bake_items[i];
    void *r_socket_value = r_socket_values[i];
    if (bake_item == nullptr) {
      type.value_initialize(r_socket_value);
      continue;
    }
    if (!copy_bake_item_to_socket_value(
            *bake_item,
            socket_type,
            [&](const CPPType &attr_type) { return make_attribute_field(i, attr_type); },
            attribute_map,
            r_socket_value))
    {
      type.value_initialize(r_socket_value);
      continue;
    }
    if (socket_type == SOCK_GEOMETRY) {
      geometries.append(static_cast<GeometrySet *>(r_socket_value));
    }
  }

  rename_attributes(geometries, attribute_map);
}

}  // namespace blender::bke
