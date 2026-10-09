/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <memory>

#include "BLI_vector.hh"

#include "FN_field.hh"
#include "FN_multi_function.hh"

namespace blender::bke {

struct ConversionFunctions {
  const mf::MultiFunction *multi_function;
  void (*convert_single_to_initialized)(const void *src, void *dst);
  void (*convert_single_to_uninitialized)(const void *src, void *dst);
};

class DataTypeConversions {
 private:
  Vector<std::unique_ptr<ConversionFunctions>> conversions_storage_;
  /* Indexed by from.type_index, then to.type_index. */
  Vector<Vector<ConversionFunctions *>> conversions_by_type_index_;

 public:
  void add(mf::DataType from_type,
           mf::DataType to_type,
           const mf::MultiFunction &fn,
           void (*convert_single_to_initialized)(const void *src, void *dst),
           void (*convert_single_to_uninitialized)(const void *src, void *dst));

  const ConversionFunctions *get_conversion_functions(const CPPType &from, const CPPType &to) const
  {
    if (from.type_index >= conversions_by_type_index_.size()) {
      return nullptr;
    }
    const Vector<ConversionFunctions *> &to_fns = conversions_by_type_index_[from.type_index];
    if (to.type_index >= to_fns.size()) {
      return nullptr;
    }
    return to_fns[to.type_index];
  }

  const ConversionFunctions *get_conversion_functions(mf::DataType from, mf::DataType to) const
  {
    if (!from.is_single() || !to.is_single()) {
      return nullptr;
    }
    return this->get_conversion_functions(from.single_type(), to.single_type());
  }

  const mf::MultiFunction *get_conversion_multi_function(mf::DataType from, mf::DataType to) const
  {
    const ConversionFunctions *functions = this->get_conversion_functions(from, to);
    return functions ? functions->multi_function : nullptr;
  }

  bool is_convertible(const CPPType &from_type, const CPPType &to_type) const
  {
    return this->get_conversion_functions(from_type, to_type) != nullptr;
  }

  void convert_to_uninitialized(const CPPType &from_type,
                                const CPPType &to_type,
                                const void *from_value,
                                void *to_value) const;

  void convert_to_initialized_n(GSpan from_span, GMutableSpan to_span) const;

  GVArray try_convert(GVArray varray, const CPPType &to_type) const;
  GVMutableArray try_convert(GVMutableArray varray, const CPPType &to_type) const;
  std::optional<fn::GField> try_convert(fn::GField field, const CPPType &to_type) const;

  template<typename T> std::optional<fn::Field<T>> try_convert(fn::GField field) const
  {
    if (std::optional<fn::GField> converted_field = this->try_convert(std::move(field),
                                                                      CPPType::get<T>()))
    {
      return std::move(converted_field->typed<T>());
    }
    return std::nullopt;
  }
};

const DataTypeConversions &get_implicit_type_conversions();

}  // namespace blender::bke
