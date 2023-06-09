/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

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
  Map<std::pair<mf::DataType, mf::DataType>, ConversionFunctions> conversions_;

 public:
  void add(mf::DataType from_type,
           mf::DataType to_type,
           const mf::MultiFunction &fn,
           void (*convert_single_to_initialized)(const void *src, void *dst),
           void (*convert_single_to_uninitialized)(const void *src, void *dst))
  {
    conversions_.add_new({from_type, to_type},
                         {&fn, convert_single_to_initialized, convert_single_to_uninitialized});
  }

  const ConversionFunctions *get_conversion_functions(mf::DataType from, mf::DataType to) const
  {
    return conversions_.lookup_ptr({from, to});
  }

  const ConversionFunctions *get_conversion_functions(const CPPType &from, const CPPType &to) const
  {
    return this->get_conversion_functions(mf::DataType::ForSingle(from),
                                          mf::DataType::ForSingle(to));
  }

  const mf::MultiFunction *get_conversion_multi_function(mf::DataType from, mf::DataType to) const
  {
    const ConversionFunctions *functions = this->get_conversion_functions(from, to);
    return functions ? functions->multi_function : nullptr;
  }

  bool is_convertible(const CPPType &from_type, const CPPType &to_type) const
  {
    return conversions_.contains(
        {mf::DataType::ForSingle(from_type), mf::DataType::ForSingle(to_type)});
  }

  void convert_to_uninitialized(const CPPType &from_type,
                                const CPPType &to_type,
                                const void *from_value,
                                void *to_value) const;

  void convert_to_initialized_n(GSpan from_span, GMutableSpan to_span) const;

  GVArray try_convert(GVArray varray, const CPPType &to_type) const;
  GVMutableArray try_convert(GVMutableArray varray, const CPPType &to_type) const;
  fn::GField try_convert(fn::GField field, const CPPType &to_type) const;
};

const DataTypeConversions &get_implicit_type_conversions();

}  // namespace blender::bke
