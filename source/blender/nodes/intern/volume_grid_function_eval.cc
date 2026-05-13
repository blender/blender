/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function.hh"

#include "BKE_node_socket_value.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_grid_multi_function_eval.hh"

#include "BLT_translation.hh"

#include "volume_grid_function_eval.hh"

namespace blender::nodes {

namespace grid = bke::volume_grid;

#ifdef WITH_OPENVDB

bool execute_multi_function_on_value_variant__volume_grid(
    const mf::MultiFunction &fn,
    const Span<bke::SocketValueVariant *> input_values,
    const Span<bke::SocketValueVariant *> output_values,
    std::string &r_error_message)
{
  using namespace bke::volume_grid::multi_function_eval;

  const int inputs_num = input_values.size();

  Vector<bke::volume_grid::multi_function_eval::InputVariant> inputs(inputs_num);
  Array<bke::volume_grid::GVolumeGrid> input_grids(inputs_num);
  Array<std::optional<GField>> input_fields(inputs_num);
  Array<bke::VolumeTreeAccessToken> input_tree_tokens(inputs_num);

  for (const int i : input_values.index_range()) {
    bke::SocketValueVariant &input_value = *input_values[i];
    if (input_value.is_volume_grid()) {
      input_grids[i] = input_value.extract<bke::volume_grid::GVolumeGrid>();
      inputs[i] = &input_grids[i]->grid(input_tree_tokens[i]);
    }
    else if (input_value.is_context_dependent_field()) {
      input_fields[i] = input_value.extract<GField>();
      inputs[i] = &*input_fields[i];
    }
    else {
      input_value.convert_to_single();
      inputs[i] = input_value.get_single_ptr();
    }
  }

  Array<bool> output_usages(output_values.size());
  for (const int i : output_values.index_range()) {
    output_usages[i] = output_values[i] != nullptr;
  }

  EvalResult result = evaluate_multi_function_on_grid(fn, inputs, output_usages);

  if (const auto *failure = std::get_if<EvalResult::Failure>(&result.result)) {
    r_error_message = failure->error_message;
    return false;
  }
  auto &success = std::get<EvalResult::Success>(result.result);
  for (const int i : output_values.index_range()) {
    if (output_usages[i]) {
      output_values[i]->set(bke::GVolumeGrid(std::move(success.output_grids[i])));
    }
  }

  return true;
}

#else

bool execute_multi_function_on_value_variant__volume_grid(
    const mf::MultiFunction & /*fn*/,
    const Span<bke::SocketValueVariant *> /*input_values*/,
    const Span<bke::SocketValueVariant *> /*output_values*/,
    std::string &r_error_message)
{
  r_error_message = TIP_("Compiled without OpenVDB");
  return false;
}

#endif

}  // namespace blender::nodes
