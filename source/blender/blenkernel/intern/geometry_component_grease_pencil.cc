/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_id.hh"

#include "DNA_grease_pencil_types.h"

#include "attribute_access_intern.hh"

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

GreasePencilComponent::GreasePencilComponent() : GeometryComponent(Type::GreasePencil) {}

GreasePencilComponent::~GreasePencilComponent()
{
  this->clear();
}

GeometryComponentPtr GreasePencilComponent::copy() const
{
  GreasePencilComponent *new_component = new GreasePencilComponent();
  if (grease_pencil_ != nullptr) {
    new_component->grease_pencil_ = BKE_grease_pencil_copy_for_eval(grease_pencil_);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return GeometryComponentPtr(new_component);
}

void GreasePencilComponent::clear()
{
  BLI_assert(this->is_mutable() || this->is_expired());
  if (grease_pencil_ != nullptr) {
    if (ownership_ == GeometryOwnershipType::Owned) {
      BKE_id_free(nullptr, grease_pencil_);
    }
    grease_pencil_ = nullptr;
  }
}

bool GreasePencilComponent::has_grease_pencil() const
{
  return grease_pencil_ != nullptr;
}

void GreasePencilComponent::replace(GreasePencil *grease_pencil, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  grease_pencil_ = grease_pencil;
  ownership_ = ownership;
}

GreasePencil *GreasePencilComponent::release()
{
  BLI_assert(this->is_mutable());
  GreasePencil *grease_pencil = grease_pencil_;
  grease_pencil_ = nullptr;
  return grease_pencil;
}

const GreasePencil *GreasePencilComponent::get() const
{
  return grease_pencil_;
}

GreasePencil *GreasePencilComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    grease_pencil_ = BKE_grease_pencil_copy_for_eval(grease_pencil_);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return grease_pencil_;
}

bool GreasePencilComponent::is_empty() const
{
  return grease_pencil_ == nullptr;
}

bool GreasePencilComponent::owns_direct_data() const
{
  return ownership_ == GeometryOwnershipType::Owned;
}

void GreasePencilComponent::ensure_owns_direct_data()
{
  BLI_assert(this->is_mutable());
  if (ownership_ != GeometryOwnershipType::Owned) {
    if (grease_pencil_) {
      grease_pencil_ = BKE_grease_pencil_copy_for_eval(grease_pencil_);
    }
    ownership_ = GeometryOwnershipType::Owned;
  }
}

static ComponentAttributeProviders create_attribute_providers_for_grease_pencil()
{
  static CustomDataAccessInfo layers_access = {
      [](void *owner) -> CustomData * {
        GreasePencil &grease_pencil = *static_cast<GreasePencil *>(owner);
        return &grease_pencil.layers_data;
      },
      [](const void *owner) -> const CustomData * {
        const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(owner);
        return &grease_pencil.layers_data;
      },
      [](const void *owner) -> int {
        const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(owner);
        return grease_pencil.layers().size();
      }};

  static CustomDataAttributeProvider layer_custom_data(AttrDomain::Layer, layers_access);

  return ComponentAttributeProviders({}, {&layer_custom_data});
}

static GVArray adapt_grease_pencil_attribute_domain(const GreasePencil & /*grease_pencil*/,
                                                    const GVArray &varray,
                                                    const AttrDomain from,
                                                    const AttrDomain to)
{
  if (from == to) {
    return varray;
  }
  return {};
}

static AttributeAccessorFunctions get_grease_pencil_accessor_functions()
{
  static const ComponentAttributeProviders providers =
      create_attribute_providers_for_grease_pencil();
  AttributeAccessorFunctions fn =
      attribute_accessor_functions::accessor_functions_for_providers<providers>();
  fn.domain_size = [](const void *owner, const AttrDomain domain) {
    if (owner == nullptr) {
      return 0;
    }
    const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(owner);
    switch (domain) {
      case AttrDomain::Layer:
        return int(grease_pencil.layers().size());
      default:
        return 0;
    }
  };
  fn.domain_supported = [](const void * /*owner*/, const AttrDomain domain) {
    return domain == AttrDomain::Layer;
  };
  fn.adapt_domain = [](const void *owner,
                       const GVArray &varray,
                       const AttrDomain from_domain,
                       const AttrDomain to_domain) -> GVArray {
    if (owner == nullptr) {
      return {};
    }
    const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(owner);
    return adapt_grease_pencil_attribute_domain(grease_pencil, varray, from_domain, to_domain);
  };
  return fn;
}

static const AttributeAccessorFunctions &get_grease_pencil_accessor_functions_ref()
{
  static const AttributeAccessorFunctions fn = get_grease_pencil_accessor_functions();
  return fn;
}

}  // namespace blender::bke

blender::bke::AttributeAccessor GreasePencil::attributes() const
{
  return blender::bke::AttributeAccessor(this,
                                         blender::bke::get_grease_pencil_accessor_functions_ref());
}

blender::bke::MutableAttributeAccessor GreasePencil::attributes_for_write()
{
  return blender::bke::MutableAttributeAccessor(
      this, blender::bke::get_grease_pencil_accessor_functions_ref());
}

namespace blender::bke {

std::optional<AttributeAccessor> GreasePencilComponent::attributes() const
{
  return AttributeAccessor(grease_pencil_, get_grease_pencil_accessor_functions_ref());
}

std::optional<MutableAttributeAccessor> GreasePencilComponent::attributes_for_write()
{
  GreasePencil *grease_pencil = this->get_for_write();
  return MutableAttributeAccessor(grease_pencil, get_grease_pencil_accessor_functions_ref());
}

}  // namespace blender::bke
