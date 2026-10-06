/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_map.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_zones.hh"

#include "COM_closure_zone_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"
#include "COM_zone_tree_operation.hh"

namespace blender::compositor {

void ClosureZoneOperation::execute()
{
  Result &output = this->get_result("Closure");
  output.allocate_single_value();
  output.set_single_value(
      Closure::create(this->zone(), this->get_captured_values(), compute_context_));
}

Map<std::string, Result *> ClosureZoneOperation::get_captured_values()
{
  Map<std::string, Result *> captured_values;
  for (const bNodeLink *link : this->zone().border_links) {
    const bNodeSocket *output = get_output_linked_to_input(*link->tosock);
    if (!output) {
      continue;
    }

    const std::string identifier = ZoneTreeOperation::get_external_input_identifier(*output);
    if (captured_values.contains(identifier)) {
      continue;
    }

    const Result &input_result = this->get_input(identifier);
    Result *captured_input = MEM_new<Result>(
        __func__, this->context().create_result(input_result.type(), input_result.precision()));
    captured_input->share_data(input_result);
    captured_values.add_new(identifier, captured_input);
  }
  return captured_values;
}

}  // namespace blender::compositor
