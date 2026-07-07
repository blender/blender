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

#include "COM_algorithm_compute_preview.hh"
#include "COM_context.hh"
#include "COM_operation.hh"
#include "COM_pixel_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

PixelOperation::PixelOperation(Context &context,
                               PixelCompileUnit &compile_unit,
                               const VectorSet<const bNode *> &schedule)
    : Operation(context), compile_unit_(compile_unit), schedule_(schedule)
{
}

void PixelOperation::compute_preview()
{
  for (const bNodeSocket *output : preview_outputs_) {
    Result &result = get_result(get_output_identifier_from_output_socket(*output));
    const bNodeInstanceKey instance_key = bke::node_instance_key(
        instance_key_, &output->owner_node().owner_tree(), &output->owner_node());
    compositor::compute_preview(context(), node_previews_, instance_key, result);
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

void PixelOperation::compute_results_reference_counts(const VectorSet<const bNode *> &schedule)
{
  for (const auto item : output_sockets_to_output_identifiers_map_.items()) {
    int reference_count = number_of_inputs_linked_to_output_conditioned(
        *item.key, [&](const bNodeSocket &input) {
          /* We only consider inputs that are not part of the pixel operations, because inputs
           * that are part of the pixel operations are internal and do not deal with the result
           * directly. */
          return schedule.contains(&input.owner_node()) &&
                 !compile_unit_.contains(&input.owner_node());
        });

    if (preview_outputs_.contains(item.key)) {
      reference_count++;
    }

    get_result(item.value).set_reference_count(reference_count);
  }
}

void PixelOperation::set_instance_key(const bNodeInstanceKey &instance_key)
{
  instance_key_ = instance_key;
}

bNodeInstanceKey PixelOperation::get_instance_key()
{
  return instance_key_;
}

void PixelOperation::set_node_previews(Map<bNodeInstanceKey, bke::bNodePreview> *node_previews)
{
  node_previews_ = node_previews;
}

Map<bNodeInstanceKey, bke::bNodePreview> *PixelOperation::get_node_previews()
{
  return node_previews_;
}

}  // namespace blender::compositor
