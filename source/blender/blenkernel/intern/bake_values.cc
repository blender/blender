/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "MEM_guardedalloc.h"

#include "BLT_translation.hh"

#include "BKE_anonymous_attribute_make.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_bake_attribute_field.hh"
#include "BKE_bake_values.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_mesh_types.hh"
#include "BKE_node.hh"
#include "BKE_node_socket_value_iter.hh"
#include "BKE_pointcloud.hh"
#include "BKE_volume.hh"

#include "DNA_curves_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_volume_types.h"

#include "FN_field.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_closure.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_geometry_nodes_values.hh"

namespace blender::bke::bake {

/**
 * Anonymous attributes are renamed before they are written to the bake. This simplifies the names
 * written to the bake and ensures that anonymous attributes are always handled explicitly.
 */
static constexpr StringRef anonymous_bake_attribute_prefix = ".bake_";

static std::unique_ptr<BakeMaterialsList> materials_to_weak_references(
    Material ***materials, short *materials_num, BakeDataBlockMap *data_block_map)
{
  if (*materials_num == 0) {
    return {};
  }
  auto materials_list = std::make_unique<BakeMaterialsList>();
  materials_list->resize(*materials_num);
  for (const int i : materials_list->index_range()) {
    Material *material = (*materials)[i];
    if (material) {
      (*materials_list)[i] = BakeDataBlockID(material->id);
      if (data_block_map) {
        data_block_map->try_add(material->id);
      }
    }
  }

  MEM_delete(*materials);
  *materials = nullptr;
  *materials_num = 0;

  return materials_list;
}

static void restore_materials(Material ***materials,
                              short *materials_num,
                              std::unique_ptr<BakeMaterialsList> materials_list,
                              BakeDataBlockMap *data_block_map)
{
  if (!materials_list) {
    return;
  }
  BLI_assert(*materials == nullptr);
  *materials_num = materials_list->size();
  *materials = MEM_new_array_zeroed<Material *>(materials_list->size(), __func__);
  if (!data_block_map) {
    return;
  }

  for (const int i : materials_list->index_range()) {
    const std::optional<BakeDataBlockID> &data_block_id = (*materials_list)[i];
    if (data_block_id) {
      (*materials)[i] = reinterpret_cast<Material *>(
          data_block_map->lookup_or_remember_missing(*data_block_id));
    }
  }
}

/**
 * Utility class to recursively convert run-time data to bake data.
 */
class RuntimeToBakeValue {
 private:
  Vector<BakeValues::InputValue> &root_values_;
  Map<std::string, std::string> referenced_anonymous_attributes_;
  int attribute_field_count_ = 0;
  BakeDataBlockMap *data_block_map_ = nullptr;

 public:
  RuntimeToBakeValue(Vector<BakeValues::InputValue> &root_values, BakeDataBlockMap *data_block_map)
      : root_values_(root_values), data_block_map_(data_block_map)
  {
  }

  void convert()
  {
    for (BakeValues::InputValue &input_value : root_values_) {
      input_value.value.ensure_owns_direct_data();
    }

    this->top_level_fields_to_attributes();

    /* As a pre-pass, gather all directly referenced anonymous attributes, because those will be
     * kept on the geometries. */
    {
      using namespace socket_value_visitor;
      auto scan_field = [&](const fn::GField &field) {
        if (const auto *attribute_field = field.get_input_if<AttributeFieldInput>()) {
          const StringRef attribute_name = attribute_field->attribute_name();
          if (attribute_name_is_anonymous(attribute_name)) {
            referenced_anonymous_attributes_.lookup_or_add_cb_as(
                attribute_name, [&]() { return this->get_next_bake_attribute_name(); });
          }
        }
        return VisitParams::continue_check(true);
      };
      VisitParams visit_params;
      visit_params.check_GField = scan_field;
      for (const BakeValues::InputValue &input_value : root_values_) {
        check_recursive(input_value.value, visit_params);
      }
    }

    /* Now process all data to be stored in a bake. This involves removing data that can't be
     * baked. */
    Vector<int> inputs_to_remove;
    for (const int i : root_values_.index_range()) {
      if (!this->runtime_to_bake__SocketValueVariant(root_values_[i].value)) {
        inputs_to_remove.append(i);
      }
    }
    /* Remove invalid values. */
    for (int i = inputs_to_remove.size() - 1; i >= 0; i--) {
      root_values_.remove_and_reorder(inputs_to_remove[i]);
    }
  }

 private:
  /** Evaluate fields on the preceding geometry if necessary. */
  void top_level_fields_to_attributes()
  {
    GeometrySet *prev_geo = nullptr;
    for (const int value_i : root_values_.index_range()) {
      BakeValues::InputValue &input_value = root_values_[value_i];
      if (input_value.value.is_single()) {
        const GMutablePointer value_ptr = input_value.value.get_single_ptr();
        if (value_ptr.is_type<GeometrySet>()) {
          prev_geo = value_ptr.get<GeometrySet>();
          continue;
        }
      }
      if (prev_geo && input_value.field_domain.has_value() &&
          input_value.value.is_context_dependent_field())
      {
        const fn::GField field = input_value.value.get<fn::GField>();
        if (field.get_input_if<AttributeFieldInput>()) {
          continue;
        }
        std::string attribute_name = this->get_next_bake_attribute_name();
        bool any_success = false;
        for (const GeometryComponent::Type type : {
                 GeometryComponent::Type::Mesh,
                 GeometryComponent::Type::PointCloud,
                 GeometryComponent::Type::GreasePencil,
                 GeometryComponent::Type::Curve,
                 GeometryComponent::Type::Instance,
             })
        {
          if (!prev_geo->has(type)) {
            continue;
          }
          GeometryComponent &component = prev_geo->get_component_for_write(type);
          any_success |= try_capture_field_on_geometry(
              component, attribute_name, *input_value.field_domain, field);
        }
        if (any_success) {
          /* Replace the field with the one that was just captured. */
          input_value.value.set(AttributeFieldInput::from(attribute_name, field.cpp_type()));
        }
      }
    }
  }

  std::string get_next_bake_attribute_name()
  {
    return fmt::format("{}{}", anonymous_bake_attribute_prefix, attribute_field_count_++);
  }

  [[nodiscard]] bool runtime_to_bake__SocketValueVariant(SocketValueVariant &value_variant)
  {
    if (value_variant.is_context_dependent_field()) {
      const fn::GField field = value_variant.get<fn::GField>();
      if (const auto *attribute_field = field.get_input_if<AttributeFieldInput>()) {
        if (const std::string *new_name = referenced_anonymous_attributes_.lookup_ptr(
                attribute_field->attribute_name()))
        {
          value_variant.set(AttributeFieldInput::from(*new_name, field.cpp_type()));
        }
      }
      else {
        /* Only attribute fields can be baked. Other fields are discarded. */
        value_variant.convert_to_single();
      }
      return true;
    }
    if (value_variant.is_single()) {
      GMutablePointer value_ptr = value_variant.get_single_ptr();
      return this->runtime_to_bake__GMutablePointer(value_ptr);
    }
    if (value_variant.is_list()) {
      nodes::GListPtr list_ptr = value_variant.extract<nodes::GListPtr>();
      if (list_ptr) {
        nodes::GList &list = list_ptr.get_for_write();
        if (!this->runtime_to_bake__List(list)) {
          return false;
        }
      }
      value_variant.set(std::move(list_ptr));
      return true;
    }
    if (value_variant.is_volume_grid()) {
      return true;
    }
    return false;
  }

  [[nodiscard]] bool runtime_to_bake__List(nodes::GList &list)
  {
    const CPPType &list_cpp_type = list.cpp_type();
    if (list_cpp_type.is<SocketValueVariant>()) {
      bool found_invalid = true;
      list.typed<SocketValueVariant>().foreach_for_write([&](SocketValueVariant &value_variant) {
        if (!this->runtime_to_bake__SocketValueVariant(value_variant)) {
          found_invalid = true;
        }
      });
      return !found_invalid;
    }
    if (list_cpp_type.is<GeometrySet>()) {
      list.typed<GeometrySet>().foreach_for_write(
          [&](GeometrySet &geometry) { this->runtime_to_bake__GeometrySet(geometry); });
      return true;
    }
    if (list_cpp_type.is<nodes::BundlePtr>()) {
      list.typed<nodes::BundlePtr>().foreach_for_write([&](nodes::BundlePtr &bundle_ptr) {
        this->runtime_to_bake__Bundle(bundle_ptr.ensure_mutable_inplace());
      });
      return true;
    }
    if (this->is_bakeable_single_value_type(list_cpp_type)) {
      return true;
    }
    return false;
  }

  [[nodiscard]] bool runtime_to_bake__GMutablePointer(GMutablePointer value_ptr)
  {
    const CPPType &type = *value_ptr.type();
    if (type.is<GeometrySet>()) {
      GeometrySet &geometry = *value_ptr.get<GeometrySet>();
      this->runtime_to_bake__GeometrySet(geometry);
      return true;
    }
    if (type.is<nodes::BundlePtr>()) {
      nodes::BundlePtr &bundle_ptr = *value_ptr.get<nodes::BundlePtr>();
      if (bundle_ptr) {
        nodes::Bundle &bundle = bundle_ptr.ensure_mutable_inplace();
        this->runtime_to_bake__Bundle(bundle);
      }
      return true;
    }
    if (this->is_bakeable_single_value_type(type)) {
      return true;
    }
    return false;
  }

  void runtime_to_bake__GeometrySet(GeometrySet &geometry)
  {
    geometry.ensure_owns_all_data();
    if (geometry.has_bundle()) {
      nodes::BundlePtr &bundle_ptr = geometry.bundle_ptr();
      nodes::Bundle &bundle = bundle_ptr.ensure_mutable_inplace();
      this->runtime_to_bake__Bundle(bundle);
    }
    if (geometry.has_instances()) {
      Instances &instances = *geometry.get_instances_for_write();
      instances.ensure_geometry_instances();
      this->runtime_to_bake__AttributeStorage(instances.attribute_storage());
      for (bke::InstanceReference &reference : instances.references_for_write()) {
        GeometrySet &geometry = reference.geometry_set();
        this->runtime_to_bake__GeometrySet(geometry);
      }
    }
    if (geometry.has_mesh()) {
      Mesh &mesh = *geometry.get_mesh_for_write();
      this->runtime_to_bake__AttributeStorage(mesh.attribute_storage.wrap());
      mesh.runtime->bake_materials = materials_to_weak_references(
          &mesh.mat, &mesh.totcol, data_block_map_);
    }
    if (geometry.has_curves()) {
      Curves &curves = *geometry.get_curves_for_write();
      this->runtime_to_bake__AttributeStorage(curves.geometry.attribute_storage.wrap());
      curves.geometry.runtime->bake_materials = materials_to_weak_references(
          &curves.mat, &curves.totcol, data_block_map_);
    }
    if (geometry.has_pointcloud()) {
      PointCloud &pointcloud = *geometry.get_pointcloud_for_write();
      this->runtime_to_bake__AttributeStorage(pointcloud.attribute_storage.wrap());
      pointcloud.runtime->bake_materials = materials_to_weak_references(
          &pointcloud.mat, &pointcloud.totcol, data_block_map_);
    }
    if (geometry.has_grease_pencil()) {
      GreasePencil &grease_pencil = *geometry.get_grease_pencil_for_write();
      this->runtime_to_bake__AttributeStorage(grease_pencil.attribute_storage.wrap());
      for (GreasePencilDrawingBase *base : grease_pencil.drawings()) {
        if (base->type != GP_DRAWING) {
          continue;
        }
        greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
        this->runtime_to_bake__AttributeStorage(
            drawing.strokes_for_write().attribute_storage.wrap());
      }
      grease_pencil.runtime->bake_materials = materials_to_weak_references(
          &grease_pencil.material_array, &grease_pencil.material_array_num, data_block_map_);
    }
    if (geometry.has_volume()) {
      Volume &volume = *geometry.get_volume_for_write();
      volume.runtime->bake_materials = materials_to_weak_references(
          &volume.mat, &volume.totcol, data_block_map_);
    }
  }

  void runtime_to_bake__AttributeStorage(AttributeStorage &attributes)
  {
    Set<const Attribute *> attributes_to_remove;
    Map<Attribute *, StringRef> attributes_to_rename;
    for (Attribute &attribute : attributes) {
      const StringRef attribute_name = attribute.name();
      if (attribute_name_is_anonymous(attribute_name) &&
          !attribute_name.startswith(anonymous_bake_attribute_prefix))
      {
        const std::string *new_name = referenced_anonymous_attributes_.lookup_ptr(attribute_name);
        if (new_name) {
          attributes_to_rename.add_new(&attribute, *new_name);
        }
        else {
          attributes_to_remove.add_new(&attribute);
        }
      }
    }
    attributes.remove(attributes_to_remove);
    attributes.rename(attributes_to_rename);
  }

  void runtime_to_bake__Bundle(nodes::Bundle &bundle)
  {
    Vector<UString> values_to_remove;
    for (const auto &item : bundle.items()) {
      if (auto *socket_value = std::get_if<nodes::BundleItemSocketValue>(&item.value.value)) {
        if (!this->runtime_to_bake__SocketValueVariant(socket_value->value)) {
          values_to_remove.append(item.key);
        }
      }
    }
    for (const UString &value_to_remove : values_to_remove) {
      bundle.remove(value_to_remove);
    }
  }

  bool is_bakeable_single_value_type(const CPPType &type) const
  {
    if (CPPType::get<std::string>() == type) {
      return true;
    }
    return cpp_type_to_custom_data_type(type).has_value();
  }
};

/**
 * Utility class to recursively convert bake data to run-time data.
 */
class BakeToRuntimeValue {
 private:
  /** Used to make newly created anonymous attributes unique. */
  std::string anonymous_attribute_name_mixin_;
  /** Stores how bake attributes are renamed to anonymous attributes again. */
  Map<std::string, std::string> runtime_name_by_bake_attribute_;
  /** The types of every bake attributes to create a proper new field for them. */
  Map<std::string, const CPPType *> attribute_field_types_;
  /** Provided by the caller to restore data-block references if possible. */
  BakeDataBlockMap *data_block_map_ = nullptr;

 public:
  BakeToRuntimeValue(std::string anonymous_attribute_name_mixin, BakeDataBlockMap *data_block_map)
      : anonymous_attribute_name_mixin_(std::move(anonymous_attribute_name_mixin)),
        data_block_map_(data_block_map)
  {
  }

  void scan(const SocketValueVariant &root_value)
  {
    using namespace socket_value_visitor;
    auto scan_attributes = [&](const AttributeAccessor &attributes) {
      attributes.foreach_attribute([&](const AttributeIter &iter) {
        if (iter.name.startswith(anonymous_bake_attribute_prefix)) {
          const AttrType attr_type = iter.data_type;
          const CPPType &attr_cpp_type = attribute_type_to_cpp_type(attr_type);
          this->attribute_field_types_.add(iter.name, &attr_cpp_type);
        }
      });
      return VisitParams::continue_check(true);
    };
    VisitParams visit_params;
    visit_params.check_AttributeAccessor = scan_attributes;
    check_recursive(root_value, visit_params);
  }

  void bake_to_runtime(SocketValueVariant &root_value, const StringRef name)
  {
    this->bake_to_runtime__SocketValueVariant(root_value, name);
  }

 private:
  void bake_to_runtime__SocketValueVariant(SocketValueVariant &value_variant, const StringRef name)
  {
    if (value_variant.is_context_dependent_field()) {
      const fn::GField field = value_variant.get<fn::GField>();
      std::string socket_inspection = nodes::make_anonymous_attribute_socket_inspection_string(
          TIP_("Bake"), name);
      if (const auto *attribute_field = field.get_input_if<AttributeFieldInput>()) {
        const StringRef bake_attribute_name = attribute_field->attribute_name();
        if (bake_attribute_name.startswith(anonymous_bake_attribute_prefix)) {
          std::string anonymous_attribute_name = this->get_anonymous_attribute_name(
              bake_attribute_name);
          value_variant.set(AttributeFieldInput::from(std::move(anonymous_attribute_name),
                                                      attribute_field->cpp_type(),
                                                      std::move(socket_inspection)));
        }
      }
      else if (const auto *attribute_field = field.get_input_if<DeferredTypeAttributeFieldInput>())
      {
        const StringRef bake_attribute_name = attribute_field->attribute_name;
        if (bake_attribute_name.startswith(anonymous_bake_attribute_prefix)) {
          if (const CPPType *cpp_type = attribute_field_types_.lookup_default(bake_attribute_name,
                                                                              nullptr))
          {
            std::string anonymous_attribute_name = this->get_anonymous_attribute_name(
                bake_attribute_name);
            value_variant.set(AttributeFieldInput::from(
                std::move(anonymous_attribute_name), *cpp_type, std::move(socket_inspection)));
          }
        }
      }
      return;
    }
    if (value_variant.is_single()) {
      GMutablePointer value_ptr = value_variant.get_single_ptr();
      this->bake_to_runtime__GMutablePointer(value_ptr);
      return;
    }
    if (value_variant.is_list()) {
      nodes::GListPtr list_ptr = value_variant.extract<nodes::GListPtr>();
      if (list_ptr) {
        nodes::GList &list = list_ptr.get_for_write();
        this->bake_to_runtime__GList(list);
      }
      value_variant.set(std::move(list_ptr));
    }
  }

  void bake_to_runtime__GMutablePointer(GMutablePointer value_ptr)
  {
    const CPPType &type = *value_ptr.type();
    if (type.is<GeometrySet>()) {
      GeometrySet &geometry = *value_ptr.get<GeometrySet>();
      this->bake_to_runtime__GeometrySet(geometry);
    }
    if (type.is<nodes::BundlePtr>()) {
      nodes::BundlePtr &bundle_ptr = *value_ptr.get<nodes::BundlePtr>();
      if (bundle_ptr) {
        nodes::Bundle &bundle = bundle_ptr.ensure_mutable_inplace();
        this->bake_to_runtime__Bundle(bundle);
      }
    }
  }

  void bake_to_runtime__GeometrySet(GeometrySet &geometry)
  {
    if (geometry.has_bundle()) {
      nodes::BundlePtr &bundle_ptr = geometry.bundle_ptr();
      nodes::Bundle &bundle = bundle_ptr.ensure_mutable_inplace();
      this->bake_to_runtime__Bundle(bundle);
    }
    if (geometry.has_instances()) {
      Instances &instances = *geometry.get_instances_for_write();
      instances.ensure_geometry_instances();
      for (bke::InstanceReference &reference : instances.references_for_write()) {
        GeometrySet &geometry = reference.geometry_set();
        this->bake_to_runtime__GeometrySet(geometry);
      }
      this->bake_to_runtime__AttributeStorage(instances.attribute_storage());
    }
    if (geometry.has_mesh()) {
      Mesh &mesh = *geometry.get_mesh_for_write();
      this->bake_to_runtime__AttributeStorage(mesh.attribute_storage.wrap());
      restore_materials(
          &mesh.mat, &mesh.totcol, std::move(mesh.runtime->bake_materials), data_block_map_);
    }
    if (geometry.has_curves()) {
      Curves &curves = *geometry.get_curves_for_write();
      this->bake_to_runtime__AttributeStorage(curves.geometry.attribute_storage.wrap());
      restore_materials(&curves.mat,
                        &curves.totcol,
                        std::move(curves.geometry.runtime->bake_materials),
                        data_block_map_);
    }
    if (geometry.has_pointcloud()) {
      PointCloud &pointcloud = *geometry.get_pointcloud_for_write();
      this->bake_to_runtime__AttributeStorage(pointcloud.attribute_storage.wrap());
      restore_materials(&pointcloud.mat,
                        &pointcloud.totcol,
                        std::move(pointcloud.runtime->bake_materials),
                        data_block_map_);
    }
    if (geometry.has_grease_pencil()) {
      GreasePencil &grease_pencil = *geometry.get_grease_pencil_for_write();
      this->bake_to_runtime__AttributeStorage(grease_pencil.attribute_storage.wrap());
      restore_materials(&grease_pencil.material_array,
                        &grease_pencil.material_array_num,
                        std::move(grease_pencil.runtime->bake_materials),
                        data_block_map_);
      for (GreasePencilDrawingBase *base : grease_pencil.drawings()) {
        if (base->type != GP_DRAWING) {
          continue;
        }
        greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
        this->bake_to_runtime__AttributeStorage(
            drawing.strokes_for_write().attribute_storage.wrap());
      }
    }
    if (geometry.has_volume()) {
      Volume &volume = *geometry.get_volume_for_write();
      restore_materials(
          &volume.mat, &volume.totcol, std::move(volume.runtime->bake_materials), data_block_map_);
    }
  }

  void bake_to_runtime__AttributeStorage(AttributeStorage &attributes)
  {
    Vector<std::pair<std::string, std::string>> attributes_to_rename;
    for (const Attribute &attribute : attributes) {
      const StringRef attribute_name = attribute.name();
      if (attribute_name.startswith(anonymous_bake_attribute_prefix)) {
        attributes_to_rename.append(
            {attribute_name, this->get_anonymous_attribute_name(attribute_name)});
      }
    }
    for (const std::pair<std::string, std::string> &attribute_to_rename : attributes_to_rename) {
      attributes.rename(attribute_to_rename.first, attribute_to_rename.second);
    }
  }

  void bake_to_runtime__Bundle(nodes::Bundle &bundle)
  {
    for (auto &&item : bundle.items()) {
      if (auto *socket_value = std::get_if<nodes::BundleItemSocketValue>(&item.value.value)) {
        this->bake_to_runtime__SocketValueVariant(socket_value->value, item.key.ref());
      }
    }
  }

  void bake_to_runtime__GList(nodes::GList &list)
  {
    const CPPType &list_cpp_type = list.cpp_type();
    if (list_cpp_type.is<SocketValueVariant>()) {
      list.typed<SocketValueVariant>().foreach_for_write([&](SocketValueVariant &value_variant) {
        this->bake_to_runtime__SocketValueVariant(value_variant, TIP_("List Item"));
      });
    }
    else if (list_cpp_type.is<GeometrySet>()) {
      list.typed<GeometrySet>().foreach_for_write(
          [&](GeometrySet &geometry) { this->bake_to_runtime__GeometrySet(geometry); });
    }
    else if (list_cpp_type.is<nodes::BundlePtr>()) {
      list.typed<nodes::BundlePtr>().foreach_for_write([&](nodes::BundlePtr &bundle_ptr) {
        this->bake_to_runtime__Bundle(bundle_ptr.ensure_mutable_inplace());
      });
    }
  }

  std::string get_anonymous_attribute_name(const StringRef bake_attribute_name)
  {
    return runtime_name_by_bake_attribute_.lookup_or_add_cb(bake_attribute_name, [&]() {
      return hash_to_anonymous_attribute_name(anonymous_attribute_name_mixin_,
                                              bake_attribute_name);
    });
  }
};

BakeValues BakeValues::from_runtime_values(Vector<InputValue> runtime_values,
                                           BakeDataBlockMap *data_block_map)
{
  RuntimeToBakeValue preparation{runtime_values, data_block_map};
  /* This may also remove runtime values that can't be baked. */
  preparation.convert();

  BakeValues bake_values;
  for (InputValue &input_value : runtime_values) {
    bake_values.values_by_id_.add(input_value.id,
                                  Item{std::move(input_value.value), std::move(input_value.name)});
  }
  return bake_values;
}

Vector<SocketValueVariant> BakeValues::to_runtime_values(const Span<OutputKey> keys,
                                                         const ComputeContext &compute_context,
                                                         BakeDataBlockMap *data_block_map) const
{
  Vector<SocketValueVariant> output_values(keys.size());
  std::stringstream ss;
  ss << compute_context.hash();
  BakeToRuntimeValue bake_to_runtime_op(ss.str(), data_block_map);
  for (const Item &item : values_by_id_.values()) {
    bake_to_runtime_op.scan(item.value);
  }
  for (const int i : keys.index_range()) {
    const OutputKey &key = keys[i];
    bke::bNodeSocketType *stype = node_socket_type_find_static(key.type);
    if (!stype || !stype->geometry_nodes_default_value) {
      continue;
    }
    SocketValueVariant &output_value = output_values[i];
    const Item *item = values_by_id_.lookup_ptr(key.id);
    if (!item) {
      output_value = *stype->geometry_nodes_default_value;
      continue;
    }
    output_value = item->value;
    bake_to_runtime_op.bake_to_runtime(output_value, item->name.value_or(""));
    if (!output_value.valid_for_socket(key.type)) {
      output_value = *stype->geometry_nodes_default_value;
      continue;
    }
  }
  return output_values;
}

}  // namespace blender::bke::bake
