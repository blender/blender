/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_grease_pencil.hh"

#include "DNA_grease_pencil_types.h"

#include "attribute_access_intern.hh"

namespace blender ::bke::greasepencil {

static GeometryAttributeProviders create_attribute_providers_for_grease_pencil()
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

  return GeometryAttributeProviders({}, {&layer_custom_data});
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
  static const GeometryAttributeProviders providers =
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

const AttributeAccessorFunctions &get_attribute_accessor_functions()
{
  static const AttributeAccessorFunctions fn = get_grease_pencil_accessor_functions();
  return fn;
}

}  // namespace blender::bke::greasepencil
