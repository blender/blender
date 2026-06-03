/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "FN_multi_function.hh"

#include "NOD_geometry_exec.hh"
#include "NOD_geometry_nodes_list.hh"

namespace blender::nodes {

class ListFieldContext : public FieldContext {
 public:
  ListFieldContext() = default;

  GVArray get_varray_for_input(const FieldInput &field_input,
                               const IndexMask &mask,
                               ResourceScope & /*scope*/) const override;
};

class SampleIndexFunction : public mf::MultiFunction {
  GListPtr list_;
  mf::Signature signature_;

 public:
  SampleIndexFunction(GListPtr list);

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override;
  void hash_unique(UniqueHashBytes &hash) const override;
};

void execute_multi_function_on_value_variant__list(const MultiFunction &fn,
                                                   const Span<SocketValueVariant *> input_values,
                                                   const Span<SocketValueVariant *> output_values,
                                                   GeoNodesUserData *user_data);

GListPtr evaluate_field_to_list(GField field, const int64_t count);

}  // namespace blender::nodes
