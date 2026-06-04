/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_node_socket_value_iter.hh"

#include "DNA_grease_pencil_types.h"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_closure.hh"
#include "NOD_geometry_nodes_list.hh"

namespace blender::bke::socket_value_visitor {

class RecursiveVisitor {
  const VisitParams &params_;

 public:
  RecursiveVisitor(const VisitParams &visitors) : params_(visitors) {}

  bool check_SocketValueVariant(const SocketValueVariant &value)
  {
    if (value.is_single()) {
      const GPointer value_ptr = value.get_single_ptr();
      if (this->check_GPointer(value_ptr)) {
        return true;
      }
    }
    else if (value.is_field()) {
      const fn::GField field = value.get<fn::GField>();
      if (this->check_GField(field)) {
        return true;
      }
    }
    else if (value.is_list()) {
      const nodes::GListPtr list = value.get<nodes::GListPtr>();
      if (list) {
        if (this->check_GList(*list)) {
          return true;
        }
      }
    }
    return false;
  }

  void edit_SocketValueVariant(SocketValueVariant &value)
  {
    if (value.is_single()) {
      GMutablePointer value_ptr = value.get_single_ptr();
      this->edit_GPointer(value_ptr);
      return;
    }
    if (value.is_field()) {
      fn::GField field = value.extract<fn::GField>();
      this->edit_GField(field);
      value.set(std::move(field));
      return;
    }
    if (value.is_list()) {
      nodes::GListPtr list = value.extract<nodes::GListPtr>();
      if (list) {
        this->edit_GList(list.get_for_write());
      }
      value.set(std::move(list));
      return;
    }
  }

  bool check_GPointer(const GPointer value_ptr)
  {
    const CPPType &type = *value_ptr.type();
    if (type.is<GeometrySet>()) {
      return this->check_GeometrySet(*value_ptr.get<GeometrySet>());
    }
    if (type.is<nodes::BundlePtr>()) {
      const nodes::BundlePtr &bundle_ptr = *value_ptr.get<nodes::BundlePtr>();
      if (bundle_ptr) {
        return this->check_Bundle(*bundle_ptr);
      }
      return false;
    }
    if (type.is<nodes::ClosurePtr>()) {
      if (params_.check_non_editable) {
        const nodes::ClosurePtr &closure_ptr = *value_ptr.get<nodes::ClosurePtr>();
        if (closure_ptr) {
          return this->check_Closure(*closure_ptr);
        }
      }
      return false;
    }
    return false;
  }

  void edit_GPointer(GMutablePointer value)
  {
    const CPPType &type = *value.type();
    if (type.is<GeometrySet>()) {
      this->edit_GeometrySet(*value.get<GeometrySet>());
      return;
    }
    if (type.is<nodes::BundlePtr>()) {
      nodes::BundlePtr &bundle_ptr = *value.get<nodes::BundlePtr>();
      if (bundle_ptr) {
        nodes::Bundle &bundle = bundle_ptr.ensure_mutable_inplace();
        this->edit_Bundle(bundle);
      }
      return;
    }
    if (type.is<nodes::ClosurePtr>()) {
      /* Can't edit closures. */
      return;
    }
  }

  bool check_GeometrySet(const GeometrySet &geometry_set)
  {
    for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                               GeometryComponent::Type::PointCloud,
                                               GeometryComponent::Type::Instance,
                                               GeometryComponent::Type::Volume,
                                               GeometryComponent::Type::Curve,
                                               GeometryComponent::Type::Edit,
                                               GeometryComponent::Type::GreasePencil})
    {
      if (!geometry_set.has(type)) {
        continue;
      }
      const GeometryComponent &component = *geometry_set.get_component(type);
      if (this->check_GeometryComponent(component)) {
        return true;
      }
    }
    if (geometry_set.has_bundle()) {
      if (this->check_Bundle(*geometry_set.bundle())) {
        return true;
      }
    }
    return false;
  }

  void edit_GeometrySet(GeometrySet &geometry_set)
  {
    if (geometry_set.has_bundle()) {
      if (this->check_Bundle(*geometry_set.bundle())) {
        this->edit_Bundle(geometry_set.bundle_for_write());
      }
    }
    for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                               GeometryComponent::Type::PointCloud,
                                               GeometryComponent::Type::Instance,
                                               GeometryComponent::Type::Volume,
                                               GeometryComponent::Type::Curve,
                                               GeometryComponent::Type::Edit,
                                               GeometryComponent::Type::GreasePencil})
    {
      if (!geometry_set.has(type)) {
        continue;
      }
      {
        const GeometryComponent &component = *geometry_set.get_component(type);
        if (!this->check_GeometryComponent(component)) {
          continue;
        }
      }
      GeometryComponent &component = geometry_set.get_component_for_write(type);
      this->edit_GeometryComponent(component);
    }
  }

  bool check_GeometryComponent(const GeometryComponent &component)
  {
    if (const std::optional<bke::AttributeAccessor> attributes = component.attributes()) {
      if (this->check_AttributeAccessor(*attributes)) {
        return true;
      }
    }
    if (component.type() == GeometryComponent::Type::Instance) {
      const InstancesComponent &instance_component = static_cast<const InstancesComponent &>(
          component);
      if (const Instances *instances = instance_component.get()) {
        for (const InstanceReference &reference : instances->references()) {
          if (this->check_InstanceReference(reference)) {
            return true;
          }
        }
      }
    }
    if (component.type() == GeometryComponent::Type::GreasePencil) {
      const GreasePencilComponent &grease_pencil_component =
          static_cast<const GreasePencilComponent &>(component);
      if (const GreasePencil *grease_pencil = grease_pencil_component.get()) {
        for (const greasepencil::Layer *layer : grease_pencil->layers()) {
          if (this->check_Layer(*grease_pencil, *layer)) {
            return true;
          }
        }
      }
    }
    return false;
  }

  void edit_GeometryComponent(GeometryComponent &component)
  {
    if (std::optional<bke::MutableAttributeAccessor> attributes = component.attributes_for_write())
    {
      this->edit_AttributeAccessor(*attributes);
    }
    if (component.type() == GeometryComponent::Type::Instance) {
      InstancesComponent &instance_component = static_cast<InstancesComponent &>(component);
      if (Instances *instances = instance_component.get_for_write()) {
        for (InstanceReference &reference : instances->references_for_write()) {
          this->edit_InstanceReference(reference);
        }
      }
    }
    if (component.type() == GeometryComponent::Type::GreasePencil) {
      GreasePencilComponent &grease_pencil_component = static_cast<GreasePencilComponent &>(
          component);
      if (GreasePencil *grease_pencil = grease_pencil_component.get_for_write()) {
        for (greasepencil::Layer *layer : grease_pencil->layers_for_write()) {
          this->edit_Layer(*grease_pencil, *layer);
        }
      }
    }
  }

  bool check_AttributeAccessor(const bke::AttributeAccessor &accessor)
  {
    if (params_.check_AttributeAccessor) {
      if (params_.check_AttributeAccessor(accessor)) {
        return true;
      }
    }
    return false;
  }

  void edit_AttributeAccessor(bke::MutableAttributeAccessor &accessor)
  {
    if (params_.edit_AttributeAccessor) {
      params_.edit_AttributeAccessor(accessor);
    }
  }

  bool check_Layer(const GreasePencil &grease_pencil, const greasepencil::Layer &layer)
  {
    if (const greasepencil::Drawing *drawing = grease_pencil.get_eval_drawing(layer)) {
      const CurvesGeometry &curves = drawing->strokes();
      if (this->check_AttributeAccessor(curves.attributes())) {
        return true;
      }
    }
    return false;
  }

  void edit_Layer(GreasePencil &grease_pencil, greasepencil::Layer &layer)
  {
    if (greasepencil::Drawing *drawing = grease_pencil.get_eval_drawing(layer)) {
      CurvesGeometry &curves = drawing->strokes_for_write();
      MutableAttributeAccessor attributes = curves.attributes_for_write();
      this->edit_AttributeAccessor(attributes);
    }
  }

  bool check_InstanceReference(const bke::InstanceReference &reference)
  {
    if (reference.type() == bke::InstanceReference::Type::GeometrySet) {
      if (this->check_GeometrySet(reference.geometry_set())) {
        return true;
      }
      return false;
    }
    if (!params_.ignore_non_geometry_instances) {
      GeometrySet geometry_set;
      reference.to_geometry_set(geometry_set);
      if (this->check_GeometrySet(geometry_set)) {
        return true;
      }
    }
    return false;
  }

  void edit_InstanceReference(bke::InstanceReference &reference)
  {
    if (reference.type() == bke::InstanceReference::Type::GeometrySet) {
      this->edit_GeometrySet(reference.geometry_set());
    }
  }

  bool check_Bundle(const nodes::Bundle &bundle)
  {
    for (const auto &item : bundle.items()) {
      if (const auto *socket_value = std::get_if<nodes::BundleItemSocketValue>(&item.value.value))
      {
        if (this->check_SocketValueVariant(socket_value->value)) {
          return true;
        }
      }
    }
    return false;
  }

  void edit_Bundle(nodes::Bundle &bundle)
  {
    for (auto &&item : bundle.items()) {
      if (auto *socket_value = std::get_if<nodes::BundleItemSocketValue>(&item.value.value)) {
        this->edit_SocketValueVariant(socket_value->value);
      }
    }
  }

  bool check_Closure(const nodes::Closure &closure)
  {
    for (const SocketValueVariant *value : closure.captured_values()) {
      if (this->check_SocketValueVariant(*value)) {
        return true;
      }
    }
    for (const SocketValueVariant &default_value : closure.default_input_values()) {
      if (this->check_SocketValueVariant(default_value)) {
        return true;
      }
    }
    return false;
  }

  bool check_GField(const fn::GField &field)
  {
    if (params_.check_GField) {
      if (params_.check_GField(field)) {
        return true;
      }
    }
    return false;
  }

  void edit_GField(fn::GField &field)
  {
    if (params_.edit_GField) {
      params_.edit_GField(field);
    }
  }

  bool check_GList(const nodes::GList &list)
  {
    const CPPType &type = list.cpp_type();
    if (type.is<SocketValueVariant>()) {
      bool need_edit = false;
      list.typed<SocketValueVariant>().foreach([&](const SocketValueVariant &value_variant) {
        if (!need_edit) {
          need_edit = this->check_SocketValueVariant(value_variant);
        }
      });
      return need_edit;
    }
    if (type.is<GeometrySet>()) {
      bool need_edit = false;
      list.typed<GeometrySet>().foreach([&](const GeometrySet &geometry) {
        if (!need_edit) {
          need_edit = this->check_GeometrySet(geometry);
        }
      });
      return need_edit;
    }
    if (type.is<nodes::BundlePtr>()) {
      bool need_edit = false;
      list.typed<nodes::BundlePtr>().foreach([&](const nodes::BundlePtr &bundle_ptr) {
        if (!need_edit) {
          need_edit = this->check_Bundle(*bundle_ptr);
        }
      });
      return need_edit;
    }
    if (type.is<nodes::ClosurePtr>()) {
      bool need_edit = false;
      list.typed<nodes::ClosurePtr>().foreach([&](const nodes::ClosurePtr &closure_ptr) {
        if (!need_edit) {
          need_edit = this->check_Closure(*closure_ptr);
        }
      });
      return need_edit;
    }
    return false;
  }

  void edit_GList(nodes::GList &list)
  {
    const CPPType &type = list.cpp_type();
    if (type.is<SocketValueVariant>()) {
      list.typed<SocketValueVariant>().foreach_for_write([&](SocketValueVariant &value_variant) {
        this->edit_SocketValueVariant(value_variant);
      });
    }
    else if (type.is<GeometrySet>()) {
      list.typed<GeometrySet>().foreach_for_write(
          [&](GeometrySet &geometry) { this->edit_GeometrySet(geometry); });
    }
    else if (type.is<nodes::BundlePtr>()) {
      list.typed<nodes::BundlePtr>().foreach_for_write([&](nodes::BundlePtr &bundle_ptr) {
        if (bundle_ptr) {
          this->edit_Bundle(bundle_ptr.ensure_mutable_inplace());
        }
      });
    }
  }
};

void edit_recursive(SocketValueVariant &value, const VisitParams &params)
{
  RecursiveVisitor visitor{params};
  visitor.edit_SocketValueVariant(value);
}

void edit_recursive(GeometrySet &value, const VisitParams &params)
{
  RecursiveVisitor visitor{params};
  visitor.edit_GeometrySet(value);
}

void check_recursive(const SocketValueVariant &value, const VisitParams &params)
{
  RecursiveVisitor visitor{params};
  visitor.check_SocketValueVariant(value);
}

void check_recursive(const GeometrySet &value, const VisitParams &params)
{
  RecursiveVisitor visitor{params};
  visitor.check_GeometrySet(value);
}

}  // namespace blender::bke::socket_value_visitor
