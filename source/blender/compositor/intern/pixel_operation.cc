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
                               const ComputeContext &compute_context,
                               const bool is_single_value)
    : Operation(context),
      compile_unit_(compile_unit),
      schedule_(schedule),
      compute_context_(compute_context),
      is_single_value_(is_single_value)
{
}

static destruct_ptr<nodes::eval_log::ImageInfoLog> get_image_info_log(
    LinearAllocator<> *allocator, const Domain &domain, const ResultPrecision &precision)
{
  return allocator->construct<nodes::eval_log::ImageInfoLog>(
      domain.data_size,
      domain.display_size,
      domain.data_offset,
      domain.transformation,
      to_string(domain.realization_options.interpolation),
      to_string(domain.realization_options.extension_x),
      to_string(domain.realization_options.extension_y),
      to_string(precision));
}

void PixelOperation::log_data()
{
  nodes::eval_log::NodesEvalLog *log = this->context().nodes_evaluation_log();
  if (!log) {
    return;
  }
  nodes::eval_log::NodeTreeLogger &tree_logger = log->get_local_tree_logger(compute_context_);

  if (is_single_value_) {
    for (const bNodeSocket *output_socket : logged_outputs_) {
      Result &result = this->get_result(
          this->get_output_identifier_from_output_socket(*output_socket));
      tree_logger.log_value(output_socket->owner_node(), *output_socket, result.single_value());

      /* Logged results gets as an extra reference in pixel operations as can be seen in the
       * compute_results_reference_counts method, so release it after logging. */
      result.release();
    }

    return;
  }

  const Domain domain = this->compute_domain();

  /* All inputs and outputs of pixel operations operate in the same domain, so the operation domain
   * should be logged for all. The exception is inputs that are single values, in which case, their
   * value is simply logged. */
  for (const bNode *node : compile_unit_) {
    /* Log output values. */
    for (const bNodeSocket *output_socket : node->output_sockets()) {
      if (!is_socket_available(output_socket)) {
        continue;
      }

      if (!output_socket->is_logically_linked()) {
        continue;
      }

      tree_logger.output_socket_values.append(
          *tree_logger.allocator,
          {node->identifier,
           output_socket->index(),
           get_image_info_log(tree_logger.allocator, domain, this->context().get_precision())});
    }

    /* Log input values. */
    for (const bNodeSocket *input_socket : node->input_sockets()) {
      if (!is_socket_available(input_socket)) {
        continue;
      }

      if (!input_socket->is_logically_linked()) {
        continue;
      }

      /* The input is linked to a node that is inside the pixel operation, so skip it since it will
       * inherit its value from an output that was logged above. */
      const bNodeSocket &linked_output = *input_socket->logically_linked_sockets()[0];
      if (compile_unit_.contains(&linked_output.owner_node())) {
        continue;
      }

      /* Otherwise, it is linked to a node that is outside of the compile unit. If it is a single
       * value, log that single value, if not, we log the operation domain. */
      const std::string &input_identifier = outputs_to_declared_inputs_map_.lookup(&linked_output);
      const Result &input = this->get_input(input_identifier);
      if (input.is_single_value()) {
        tree_logger.log_value(*node, *input_socket, input.single_value());
        continue;
      }

      tree_logger.input_socket_values.append(
          *tree_logger.allocator,
          {node->identifier,
           input_socket->index(),
           get_image_info_log(tree_logger.allocator, domain, this->context().get_precision())});
    }
  }

  for (const bNodeSocket *output : preview_outputs_) {
    Result &result = this->get_result(get_output_identifier_from_output_socket(*output));
    ImBuf *preview = compositor::compute_preview(context(), result);
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

Map<ImplicitInputType, std::string> &PixelOperation::get_implicit_inputs_to_input_identifiers_map()
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

    if (logged_outputs_.contains(item.key)) {
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
