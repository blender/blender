/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_bake_items_socket.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_node.hh"
#include "BKE_node_socket_value.hh"

namespace blender::bke::bake {

Array<std::unique_ptr<BakeItem>> move_socket_values_to_bake_items(const Span<void *> socket_values,
                                                                  const BakeSocketConfig &config,
                                                                  BakeDataBlockMap *data_block_map)
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
        auto &value_variant = *static_cast<SocketValueVariant *>(socket_value);
        bake_items[i] = std::make_unique<StringBakeItem>(value_variant.extract<std::string>());
        break;
      }
      case SOCK_FLOAT:
      case SOCK_VECTOR:
      case SOCK_INT:
      case SOCK_BOOLEAN:
      case SOCK_ROTATION:
      case SOCK_MATRIX:
      case SOCK_RGBA: {
        auto &value_variant = *static_cast<SocketValueVariant *>(socket_value);
        if (value_variant.is_context_dependent_field()) {
          const fn::GField &field = value_variant.get<fn::GField>();
          const AttrDomain domain = config.domains[i];
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
              InstancesComponent &component =
                  geometry.get_component_for_write<InstancesComponent>();
              try_capture_field_on_geometry(component, attribute_name, domain, field);
            }
          }
          bake_items[i] = std::make_unique<AttributeBakeItem>(attribute_name);
        }
        else {
          value_variant.convert_to_single();
          GPointer value = value_variant.get_single_ptr();
          bake_items[i] = std::make_unique<PrimitiveBakeItem>(*value.type(), value.get());
        }
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
    GeometryBakeItem::prepare_geometry_for_bake(geometry, data_block_map);
  }

  for (const int i : bake_items.index_range()) {
    if (std::unique_ptr<BakeItem> &item = bake_items[i]) {
      item->name = config.names[i];
    }
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
    case SOCK_MATRIX:
    case SOCK_RGBA: {
      const CPPType &base_type = *socket_type_to_geo_nodes_base_cpp_type(socket_type);
      if (const auto *item = dynamic_cast<const PrimitiveBakeItem *>(&bake_item)) {
        if (item->type() == base_type) {
          auto *value_variant = new (r_value) SocketValueVariant();
          value_variant->store_single(socket_type, item->value());
          return true;
        }
        return false;
      }
      if (const auto *item = dynamic_cast<const AttributeBakeItem *>(&bake_item)) {
        std::shared_ptr<AnonymousAttributeFieldInput> attribute_field = make_attribute_field(
            base_type);
        const AnonymousAttributeIDPtr &attribute_id = attribute_field->anonymous_id();
        fn::GField field{attribute_field};
        new (r_value) SocketValueVariant(std::move(field));
        r_attribute_map.add(item->name(), attribute_id);
        return true;
      }
      return false;
    }
    case SOCK_STRING: {
      if (const auto *item = dynamic_cast<const StringBakeItem *>(&bake_item)) {
        new (r_value) SocketValueVariant(std::string(item->value()));
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
           attribute_map.items())
      {
        attributes.rename(attribute_item.key, *attribute_item.value);
      }
    }
  }
}

static void restore_data_blocks(const Span<GeometrySet *> geometries,
                                BakeDataBlockMap *data_block_map)
{
  for (GeometrySet *main_geometry : geometries) {
    GeometryBakeItem::try_restore_data_blocks(*main_geometry, data_block_map);
  }
}

static void default_initialize_socket_value(const eNodeSocketDatatype socket_type, void *r_value)
{
  const char *socket_idname = nodeStaticSocketType(socket_type, 0);
  const bNodeSocketType *typeinfo = nodeSocketTypeFind(socket_idname);
  if (typeinfo->geometry_nodes_default_cpp_value) {
    typeinfo->geometry_nodes_cpp_type->copy_construct(typeinfo->geometry_nodes_default_cpp_value,
                                                      r_value);
  }
  else {
    typeinfo->geometry_nodes_cpp_type->value_initialize(r_value);
  }
}

void move_bake_items_to_socket_values(
    const Span<BakeItem *> bake_items,
    const BakeSocketConfig &config,
    BakeDataBlockMap *data_block_map,
    FunctionRef<std::shared_ptr<AnonymousAttributeFieldInput>(int, const CPPType &)>
        make_attribute_field,
    const Span<void *> r_socket_values)
{
  Map<std::string, AnonymousAttributeIDPtr> attribute_map;

  Vector<GeometrySet *> geometries;

  for (const int i : bake_items.index_range()) {
    const eNodeSocketDatatype socket_type = config.types[i];
    BakeItem *bake_item = bake_items[i];
    void *r_socket_value = r_socket_values[i];
    if (bake_item == nullptr) {
      default_initialize_socket_value(socket_type, r_socket_value);
      continue;
    }
    if (!copy_bake_item_to_socket_value(
            *bake_item,
            socket_type,
            [&](const CPPType &attr_type) { return make_attribute_field(i, attr_type); },
            attribute_map,
            r_socket_value))
    {
      default_initialize_socket_value(socket_type, r_socket_value);
      continue;
    }
    if (socket_type == SOCK_GEOMETRY) {
      auto &item = *static_cast<GeometryBakeItem *>(bake_item);
      item.geometry.clear();
      geometries.append(static_cast<GeometrySet *>(r_socket_value));
    }
  }

  rename_attributes(geometries, attribute_map);
  restore_data_blocks(geometries, data_block_map);
}

void copy_bake_items_to_socket_values(
    const Span<const BakeItem *> bake_items,
    const BakeSocketConfig &config,
    BakeDataBlockMap *data_block_map,
    FunctionRef<std::shared_ptr<AnonymousAttributeFieldInput>(int, const CPPType &)>
        make_attribute_field,
    const Span<void *> r_socket_values)
{
  Map<std::string, AnonymousAttributeIDPtr> attribute_map;
  Vector<GeometrySet *> geometries;

  for (const int i : bake_items.index_range()) {
    const eNodeSocketDatatype socket_type = config.types[i];
    const BakeItem *bake_item = bake_items[i];
    void *r_socket_value = r_socket_values[i];
    if (bake_item == nullptr) {
      default_initialize_socket_value(socket_type, r_socket_value);
      continue;
    }
    if (!copy_bake_item_to_socket_value(
            *bake_item,
            socket_type,
            [&](const CPPType &attr_type) { return make_attribute_field(i, attr_type); },
            attribute_map,
            r_socket_value))
    {
      default_initialize_socket_value(socket_type, r_socket_value);
      continue;
    }
    if (socket_type == SOCK_GEOMETRY) {
      geometries.append(static_cast<GeometrySet *>(r_socket_value));
    }
  }

  rename_attributes(geometries, attribute_map);
  restore_data_blocks(geometries, data_block_map);
}

}  // namespace blender::bke::bake
