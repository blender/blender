/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>
#include <string>

#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector_set.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "NOD_eval_log.hh"

#include "COM_algorithm_compute_preview.hh"
#include "COM_context.hh"
#include "COM_operation.hh"
#include "COM_pixel_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

PixelOperation::PixelOperation(Context &context,
                               PixelCompileUnit &compile_unit,
                               const Schedule &schedule,
                               const ComputeContext &compute_context)
    : Operation(context),
      compile_unit_(compile_unit),
      schedule_(schedule),
      compute_context_(compute_context)
{
}

void PixelOperation::compute_preview()
{
  for (const bNodeSocket *output : preview_outputs_) {
    Result &result = this->get_result(get_output_identifier_from_output_socket(*output));
    ImBuf *preview = compositor::compute_preview(context(), result);
    nodes::eval_log::NodeTreeLogger &tree_logger =
        this->context().nodes_evaluation_log()->get_local_tree_logger(compute_context_);
    tree_logger.node_image_previews.append(*tree_logger.allocator,
                                           {output->owner_node().identifier, preview});

    /* Preview results gets as an extra reference in pixel operations as can be seen in the
     * compute_results_reference_counts method, so release it after computing preview. */
    result.release();
  }
}

StringRef PixelOperation::get_output_identifier_from_output_socket(
    const bNodeSocket &output_socket)
{
  return output_sockets_to_output_identifiers_map_.lookup(&output_socket);
}

Map<std::string, const bNodeSocket *> &PixelOperation::get_inputs_to_linked_outputs_map()
{
  return inputs_to_linked_outputs_map_;
}

Map<ImplicitInput, std::string> &PixelOperation::get_implicit_inputs_to_input_identifiers_map()
{
  return implicit_inputs_to_input_identifiers_map_;
}

int PixelOperation::get_internal_input_reference_count(const StringRef &identifier)
{
  return inputs_to_reference_counts_map_.lookup(identifier);
}

void PixelOperation::compute_results_reference_counts(const Schedule &schedule)
{
  for (const auto item : output_sockets_to_output_identifiers_map_.items()) {
    int reference_count = number_of_inputs_linked_to_output_conditioned(
        *item.key, [&](const bNodeSocket &input) {
          /* We only consider inputs that are not part of the pixel operations, because inputs
           * that are part of the pixel operations are internal and do not deal with the result
           * directly. */
          return schedule.nodes.contains(&input.owner_node()) &&
                 !schedule.unneeded_inputs.contains(&input) &&
                 !compile_unit_.contains(&input.owner_node());
        });

    if (preview_outputs_.contains(item.key)) {
      reference_count++;
    }

    get_result(item.value).set_reference_count(reference_count);
  }
}

void PixelOperation::set_needs_node_previews(const bool needed)
{
  needs_node_previews_ = needed;
}

}  // namespace blender::compositor
