/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_bake_items_socket.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_node.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_volume_grid.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_list.hh"

namespace blender::bke::bake {

static void capture_field_on_geometry_components(GeometrySet &geometry,
                                                 const fn::GField &field,
                                                 const AttrDomain domain,
                                                 const StringRef attribute_name)
{
  if (geometry.has_pointcloud()) {
    PointCloudComponent &component = geometry.get_component_for_write<PointCloudComponent>();
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
  if (geometry.has_grease_pencil()) {
    GreasePencilComponent &component = geometry.get_component_for_write<GreasePencilComponent>();
    try_capture_field_on_geometry(component, attribute_name, domain, field);
  }
  if (geometry.has_instances()) {
    InstancesComponent &component = geometry.get_component_for_write<InstancesComponent>();
    try_capture_field_on_geometry(component, attribute_name, domain, field);
  }
}

static std::unique_ptr<BakeItem> move_common_socket_value_to_bake_item(
    const bNodeSocketType &stype,
    SocketValueVariant &socket_value,
    std::optional<StringRef> name,
    Vector<GeometryBakeItem *> &r_geometry_bake_items);

static void move_bundle_socket_value_to_bake_item(
    const nodes::Bundle &bundle,
    BundleBakeItem &bundle_bake_item,
    Vector<GeometryBakeItem *> &r_geometry_bake_items)
{
  for (const nodes::Bundle::StoredItem &bundle_item : bundle.items()) {
    if (const auto *item_socket_value = std::get_if<nodes::BundleItemSocketValue>(
            &bundle_item.value.value))
    {
      SocketValueVariant value_variant = item_socket_value->value;
      if (std::unique_ptr<BakeItem> bake_item = move_common_socket_value_to_bake_item(
              *item_socket_value->type, value_variant, std::nullopt, r_geometry_bake_items))
      {
        bundle_bake_item.items.append(BundleBakeItem::Item{
            bundle_item.key,
            BundleBakeItem::SocketValue{item_socket_value->type->idname, std::move(bake_item)}});
      }
    }
    else if (const auto *internal_value = std::get_if<nodes::BundleItemInternalValue>(
                 &bundle_item.value.value))
    {
      const ImplicitSharingInfo *sharing_info = internal_value->value.get();
      if (sharing_info) {
        sharing_info->add_user();
      }
      bundle_bake_item.items.append(BundleBakeItem::Item{
          bundle_item.key, BundleBakeItem::InternalValue{ImplicitSharingPtr<>{sharing_info}}});
    }
  }
}

static std::unique_ptr<BakeItem> move_common_socket_value_to_bake_item(
    const bNodeSocketType &stype,
    SocketValueVariant &socket_value,
    std::optional<StringRef> name,
    Vector<GeometryBakeItem *> &r_geometry_bake_items)
{
  switch (stype.type) {
    case SOCK_GEOMETRY: {
      GeometrySet geometry = socket_value.extract<GeometrySet>();
      auto item = std::make_unique<GeometryBakeItem>(std::move(geometry));
      r_geometry_bake_items.append(item.get());
      return item;
    }
    case SOCK_STRING: {
      if (socket_value.is_list()) {
        return std::make_unique<ListBakeItem>(socket_value.extract<nodes::ListPtr>());
      }
      return std::make_unique<StringBakeItem>(socket_value.extract<std::string>());
    }
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_INT:
    case SOCK_BOOLEAN:
    case SOCK_ROTATION:
    case SOCK_MATRIX:
    case SOCK_RGBA: {
      if (socket_value.is_context_dependent_field()) {
        /* Not supported here because it's not known which geometry this field belongs to. */
        return {};
      }
      if (socket_value.is_list()) {
        return std::make_unique<ListBakeItem>(socket_value.extract<nodes::ListPtr>());
      }
#ifdef WITH_OPENVDB
      if (socket_value.is_volume_grid()) {
        bke::GVolumeGrid grid = socket_value.get<bke::GVolumeGrid>();
        if (name) {
          grid.get_for_write().set_name(*name);
        }
        return std::make_unique<VolumeGridBakeItem>(
            std::make_unique<bke::GVolumeGrid>(std::move(grid)));
      }
#else
      UNUSED_VARS(name);
#endif

      socket_value.convert_to_single();
      GPointer value = socket_value.get_single_ptr();
      return std::make_unique<PrimitiveBakeItem>(*value.type(), value.get());
    }
    case SOCK_BUNDLE: {
      if (socket_value.is_list()) {
        const nodes::ListPtr list = socket_value.extract<nodes::ListPtr>();
        Vector<BundleBakeItem> bake_item_list(list->size());
        const VArray<nodes::BundlePtr> bundle_varray = list->varray<nodes::BundlePtr>();
        for (const int i : bake_item_list.index_range()) {
          nodes::BundlePtr bundle_ptr = bundle_varray[i];
          if (bundle_ptr) {
            move_bundle_socket_value_to_bake_item(
                *bundle_ptr, bake_item_list[i], r_geometry_bake_items);
          }
        }
        return std::make_unique<ListBakeItem>(std::move(bake_item_list));
      }

      nodes::BundlePtr bundle_ptr = socket_value.extract<nodes::BundlePtr>();
      auto bundle_bake_item = std::make_unique<BundleBakeItem>();
      if (bundle_ptr) {
        move_bundle_socket_value_to_bake_item(
            *bundle_ptr, *bundle_bake_item, r_geometry_bake_items);
      }
      return bundle_bake_item;
    }
    default:
      return {};
  }
}

Array<std::unique_ptr<BakeItem>> move_socket_values_to_bake_items(
    MutableSpan<SocketValueVariant> socket_values,
    const BakeSocketConfig &config,
    BakeDataBlockMap *data_block_map)
{
  BLI_assert(socket_values.size() == config.types.size());
  BLI_assert(socket_values.size() == config.geometries_by_attribute.size());

  Array<std::unique_ptr<BakeItem>> bake_items(socket_values.size());

  Vector<GeometryBakeItem *> geometry_bake_items;

  /* Create geometry bake items first because they are used for field evaluation. */
  for (const int i : socket_values.index_range()) {
    const eNodeSocketDatatype socket_type = config.types[i];
    if (socket_type != SOCK_GEOMETRY) {
      continue;
    }
    GeometrySet geometry = socket_values[i].extract<GeometrySet>();
    auto geometry_item = std::make_unique<GeometryBakeItem>(std::move(geometry));
    geometry_bake_items.append(geometry_item.get());
    bake_items[i] = std::move(geometry_item);
  }

  for (const int i : socket_values.index_range()) {
    const eNodeSocketDatatype socket_type = config.types[i];
    const bNodeSocketType &stype = *node_socket_type_find_static(socket_type);
    SocketValueVariant &socket_value = socket_values[i];
    switch (socket_type) {
      case SOCK_GEOMETRY: {
        /* Handled already. */
        break;
      }
      case SOCK_STRING: {
        bake_items[i] = move_common_socket_value_to_bake_item(
            stype, socket_value, config.names[i], geometry_bake_items);
        break;
      }
      case SOCK_FLOAT:
      case SOCK_VECTOR:
      case SOCK_INT:
      case SOCK_BOOLEAN:
      case SOCK_ROTATION:
      case SOCK_MATRIX:
      case SOCK_RGBA: {
        if (socket_value.is_context_dependent_field()) {
          const fn::GField &field = socket_value.get<fn::GField>();
          const AttrDomain domain = config.domains[i];
          const std::string attribute_name = ".bake_" + std::to_string(i);
          const Span<int> geometry_indices = config.geometries_by_attribute[i];
          for (const int geometry_i : geometry_indices) {
            BLI_assert(config.types[geometry_i] == SOCK_GEOMETRY);
            GeometrySet &geometry =
                static_cast<GeometryBakeItem *>(bake_items[geometry_i].get())->geometry;
            capture_field_on_geometry_components(geometry, field, domain, attribute_name);
          }
          bake_items[i] = std::make_unique<AttributeBakeItem>(attribute_name);
        }
        else {
          bake_items[i] = move_common_socket_value_to_bake_item(
              stype, socket_value, config.names[i], geometry_bake_items);
        }
        break;
      }
      case SOCK_BUNDLE: {
        bake_items[i] = move_common_socket_value_to_bake_item(
            stype, socket_value, config.names[i], geometry_bake_items);
        break;
      }
      default:
        break;
    }
  }

  /* Cleanup geometries after fields have been evaluated. */
  for (GeometryBakeItem *geometry_item : geometry_bake_items) {
    GeometryBakeItem::prepare_geometry_for_bake(geometry_item->geometry, data_block_map);
  }

  for (const int i : bake_items.index_range()) {
    if (std::unique_ptr<BakeItem> &item = bake_items[i]) {
      item->name = config.names[i];
    }
  }

  return bake_items;
}

[[nodiscard]] static std::optional<SocketValueVariant> copy_bake_item_to_socket_value(
    const BakeItem &bake_item,
    const eNodeSocketDatatype socket_type,
    const FunctionRef<std::shared_ptr<AttributeFieldInput>(const CPPType &type)>
        make_attribute_field,
    BakeDataBlockMap *data_block_map,
    Map<std::string, std::string> &r_attribute_map);

static bool copy_bundle_bake_item_to_socket_value(const BundleBakeItem &bundle_bake_item,
                                                  nodes::Bundle &bundle,
                                                  BakeDataBlockMap *data_block_map,
                                                  Map<std::string, std::string> &r_attribute_map)
{
  for (const BundleBakeItem::Item &item : bundle_bake_item.items) {
    if (const auto *socket_value = std::get_if<BundleBakeItem::SocketValue>(&item.value)) {
      const bNodeSocketType *stype = node_socket_type_find(socket_value->socket_idname);
      if (!stype) {
        return false;
      }
      if (!stype->geometry_nodes_default_value) {
        return false;
      }
      if (std::optional<SocketValueVariant> child_value_variant = copy_bake_item_to_socket_value(
              *socket_value->value, stype->type, {}, data_block_map, r_attribute_map))
      {
        bundle.add(item.key, nodes::BundleItemSocketValue{stype, *child_value_variant});
      }
      else {
        return false;
      }
    }
    if (const auto *internal_value = std::get_if<BundleBakeItem::InternalValue>(&item.value)) {
      const auto *internal_data = dynamic_cast<const nodes::BundleItemInternalValueMixin *>(
          internal_value->value.get());
      if (!internal_data) {
        continue;
      }
      internal_data->add_user();
      bundle.add(item.key, nodes::BundleItemInternalValue{ImplicitSharingPtr{internal_data}});
    }
  }
  return true;
}

/**
 * \return True if #r_value has been constructed.
 */
[[nodiscard]] static std::optional<SocketValueVariant> copy_bake_item_to_socket_value(
    const BakeItem &bake_item,
    const eNodeSocketDatatype socket_type,
    const FunctionRef<std::shared_ptr<AttributeFieldInput>(const CPPType &type)>
        make_attribute_field,
    BakeDataBlockMap *data_block_map,
    Map<std::string, std::string> &r_attribute_map)
{
  switch (socket_type) {
    case SOCK_GEOMETRY: {
      if (const auto *item = dynamic_cast<const GeometryBakeItem *>(&bake_item)) {
        bke::GeometrySet geometry = item->geometry;
        GeometryBakeItem::try_restore_data_blocks(geometry, data_block_map);
        return SocketValueVariant::From(std::move(geometry));
      }
      return std::nullopt;
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
          SocketValueVariant value_variant;
          value_variant.store_single(socket_type, item->value());
          return value_variant;
        }
        return std::nullopt;
      }
      if (const auto *item = dynamic_cast<const AttributeBakeItem *>(&bake_item)) {
        if (!make_attribute_field) {
          return std::nullopt;
        }
        std::shared_ptr<AttributeFieldInput> attribute_field = make_attribute_field(base_type);
        r_attribute_map.add(item->name(), attribute_field->attribute_name());
        fn::GField field{attribute_field};
        return SocketValueVariant::From(std::move(field));
      }
      if (const auto *item = dynamic_cast<const ListBakeItem *>(&bake_item)) {
        if (const auto *simple_list = std::get_if<nodes::ListPtr>(&item->value)) {
          if (*simple_list && (*simple_list)->cpp_type() == base_type) {
            return SocketValueVariant::From(std::move(*simple_list));
          }
        }
        return std::nullopt;
      }
#ifdef WITH_OPENVDB
      if (const auto *item = dynamic_cast<const VolumeGridBakeItem *>(&bake_item)) {
        const GVolumeGrid &grid = *item->grid;
        const VolumeGridType grid_type = grid->grid_type();
        const std::optional<eNodeSocketDatatype> grid_socket_type = grid_type_to_socket_type(
            grid_type);
        if (!grid_socket_type) {
          return std::nullopt;
        }
        if (grid_socket_type == socket_type) {
          return bke::SocketValueVariant::From(*item->grid);
        }
        return std::nullopt;
      }
#endif
      return std::nullopt;
    }
    case SOCK_STRING: {
      if (const auto *item = dynamic_cast<const StringBakeItem *>(&bake_item)) {
        return SocketValueVariant(std::string(item->value()));
      }
      if (const auto *item = dynamic_cast<const ListBakeItem *>(&bake_item)) {
        if (const auto *simple_list = std::get_if<nodes::ListPtr>(&item->value)) {
          if (*simple_list && (*simple_list)->cpp_type() == CPPType::get<std::string>()) {
            return SocketValueVariant::From(std::move(*simple_list));
          }
        }
        return std::nullopt;
      }
      return std::nullopt;
    }
    case SOCK_BUNDLE: {
      if (const auto *item = dynamic_cast<const BundleBakeItem *>(&bake_item)) {
        nodes::BundlePtr bundle_ptr = nodes::Bundle::create();
        nodes::Bundle &bundle = const_cast<nodes::Bundle &>(*bundle_ptr);
        if (!copy_bundle_bake_item_to_socket_value(*item, bundle, data_block_map, r_attribute_map))
        {
          return std::nullopt;
        }
        return bke::SocketValueVariant::From(std::move(bundle_ptr));
      }
      if (const auto *item = dynamic_cast<const ListBakeItem *>(&bake_item)) {
        if (const auto *bundle_list = std::get_if<ListBakeItem::BundleList>(&item->value)) {
          const CPPType &type = CPPType::get<nodes::BundlePtr>();
          const int count = bundle_list->size();
          auto array_data = nodes::List::ArrayData::ForDefaultValue(type, count);
          MutableSpan array_span(static_cast<nodes::BundlePtr *>(array_data.data), count);
          for (const int i : IndexRange(count)) {
            array_span[i] = nodes::Bundle::create();
            nodes::Bundle &bundle = const_cast<nodes::Bundle &>(*array_span[i]);
            copy_bundle_bake_item_to_socket_value(
                (*bundle_list)[i], bundle, data_block_map, r_attribute_map);
          }
          nodes::ListPtr list_ptr = nodes::List::create(type, std::move(array_data), count);
          return bke::SocketValueVariant::From(std::move(list_ptr));
        }
        return std::nullopt;
      }
      return std::nullopt;
    }
    default:
      return std::nullopt;
  }
  return std::nullopt;
}

static void rename_attributes(const Span<GeometrySet *> geometries,
                              const Map<std::string, std::string> &attribute_map)
{
  for (GeometrySet *geometry : geometries) {
    for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                               GeometryComponent::Type::Curve,
                                               GeometryComponent::Type::GreasePencil,
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
      for (const MapItem<std::string, std::string> &attribute_item : attribute_map.items()) {
        attributes.rename(attribute_item.key, attribute_item.value);
      }
    }
  }
}

static SocketValueVariant default_initialize_socket_value(const eNodeSocketDatatype socket_type)
{
  const bke::bNodeSocketType *typeinfo = bke::node_socket_type_find_static(socket_type);
  return *static_cast<const SocketValueVariant *>(typeinfo->geometry_nodes_default_value);
}

Vector<SocketValueVariant> move_bake_items_to_socket_values(
    const Span<BakeItem *> bake_items,
    const BakeSocketConfig &config,
    BakeDataBlockMap *data_block_map,
    FunctionRef<std::shared_ptr<AttributeFieldInput>(int, const CPPType &)> make_attribute_field)
{
  Map<std::string, std::string> attribute_map;
  Vector<SocketValueVariant> socket_values;
  socket_values.reserve(bake_items.size());

  for (const int i : bake_items.index_range()) {
    const eNodeSocketDatatype socket_type = config.types[i];
    BakeItem *bake_item = bake_items[i];
    if (bake_item == nullptr) {
      socket_values.append(default_initialize_socket_value(socket_type));
      continue;
    }
    if (std::optional<SocketValueVariant> socket_value = copy_bake_item_to_socket_value(
            *bake_item,
            socket_type,
            [&](const CPPType &attr_type) { return make_attribute_field(i, attr_type); },
            data_block_map,
            attribute_map))
    {
      socket_values.append(std::move(*socket_value));
    }
    else {
      socket_values.append(default_initialize_socket_value(socket_type));
      continue;
    }
    if (socket_type == SOCK_GEOMETRY) {
      auto &item = *static_cast<GeometryBakeItem *>(bake_item);
      item.geometry.clear();
    }
  }

  for (SocketValueVariant &socket_value : socket_values) {
    if (socket_value.valid_for_socket(SOCK_GEOMETRY)) {
      GeometrySet *geometry = socket_value.get_single_ptr().get<GeometrySet>();
      rename_attributes({geometry}, attribute_map);
    }
  }

  return socket_values;
}

Vector<SocketValueVariant> copy_bake_items_to_socket_values(
    const Span<const BakeItem *> bake_items,
    const BakeSocketConfig &config,
    BakeDataBlockMap *data_block_map,
    FunctionRef<std::shared_ptr<AttributeFieldInput>(int, const CPPType &)> make_attribute_field)
{
  Map<std::string, std::string> attribute_map;
  Vector<SocketValueVariant> socket_values;
  socket_values.reserve(bake_items.size());

  for (const int i : bake_items.index_range()) {
    const eNodeSocketDatatype socket_type = config.types[i];
    const BakeItem *bake_item = bake_items[i];
    if (bake_item == nullptr) {
      socket_values.append(default_initialize_socket_value(socket_type));
      continue;
    }

    if (std::optional<SocketValueVariant> socket_value = copy_bake_item_to_socket_value(
            *bake_item,
            socket_type,
            [&](const CPPType &attr_type) { return make_attribute_field(i, attr_type); },
            data_block_map,
            attribute_map))
    {
      socket_values.append(std::move(*socket_value));
    }
    else {
      socket_values.append(default_initialize_socket_value(socket_type));
    }
  }

  for (SocketValueVariant &socket_value : socket_values) {
    if (socket_value.valid_for_socket(SOCK_GEOMETRY)) {
      GeometrySet *geometry = socket_value.get_single_ptr().get<GeometrySet>();
      rename_attributes({geometry}, attribute_map);
    }
  }

  return socket_values;
}

}  // namespace blender::bke::bake
