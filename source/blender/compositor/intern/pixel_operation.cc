/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>
#include <string>

#include "BLI_map.hh"
#include "BLI_string_ref.hh"

#include "NOD_derived_node_tree.hh"

#include "COM_algorithm_compute_preview.hh"
#include "COM_context.hh"
#include "COM_multi_function_procedure_operation.hh"
#include "COM_operation.hh"
#include "COM_pixel_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_shader_operation.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

PixelOperation::PixelOperation(Context &context,
                               PixelCompileUnit &compile_unit,
                               const Schedule &schedule)
    : Operation(context), compile_unit_(compile_unit), schedule_(schedule)
{
}

int PixelOperation::maximum_number_of_outputs(Context &context)
{
  if (context.use_gpu()) {
    /* The GPU module currently only supports up to 8 output images in shaders, but once this
     * limitation is lifted, we can replace that with GPU_max_images(). */
    return 8;
  }

  return std::numeric_limits<int>::max();
}

void PixelOperation::compute_preview()
{
  for (const DOutputSocket &output : preview_outputs_) {
    Result &result = get_result(get_output_identifier_from_output_socket(output));
    compositor::compute_preview(context(), output.node(), result);
    /* Preview results gets as an extra reference in pixel operations as can be seen in the
     * compute_results_reference_counts method, so release it after computing preview. */
    result.release();
  }
}

StringRef PixelOperation::get_output_identifier_from_output_socket(DOutputSocket output_socket)
{
  return output_sockets_to_output_identifiers_map_.lookup(output_socket);
}

Map<std::string, DOutputSocket> &PixelOperation::get_inputs_to_linked_outputs_map()
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
        item.key, [&](DInputSocket input) {
          /* We only consider inputs that are not part of the pixel operations, because inputs
           * that are part of the pixel operations are internal and do not deal with the result
           * directly. */
          return schedule.contains(input.node()) && !compile_unit_.contains(input.node());
        });

    if (preview_outputs_.contains(item.key)) {
      reference_count++;
    }

    get_result(item.value).set_reference_count(reference_count);
  }
}

}  // namespace blender::compositor
