/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_field.hh"

namespace blender::bke::bake {

/**
 * When reading an attribute field from a bake, the type of the field may not be known immediately
 * because it used to not be stored. However, it can be derived later when all actually existing
 * attributes are known.
 */
class DeferredTypeAttributeFieldInput : public fn::FieldInput {
 public:
  std::string attribute_name;
  using DummyT = float;

  DeferredTypeAttributeFieldInput(std::string attribute_name)
      : fn::FieldInput(CPPType::get<DummyT>()), attribute_name(std::move(attribute_name))
  {
  }

  GVArray get_varray_for_context(const fn::FieldContext & /*context*/,
                                 const IndexMask &mask,
                                 ResourceScope & /*scope*/) const override
  {
    return VArray<DummyT>::from_single({}, mask.min_array_size());
  }
};

}  // namespace blender::bke::bake
