/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_function_ref.hh"
#include "BLI_index_range.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_node_declaration.hh"

#include "GPU_compute.h"
#include "GPU_shader.h"

#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;
using TargetSocketPathInfo = DOutputSocket::TargetSocketPathInfo;

DSocket get_input_origin_socket(DInputSocket input)
{
  /* The input is unlinked. Return the socket itself. */
  if (!input->is_logically_linked()) {
    return input;
  }

  /* Only a single origin socket is guaranteed to exist. */
  DSocket socket;
  input.foreach_origin_socket([&](const DSocket origin) { socket = origin; });
  return socket;
}

DOutputSocket get_output_linked_to_input(DInputSocket input)
{
  /* Get the origin socket of this input, which will be an output socket if the input is linked
   * to an output. */
  const DSocket origin = get_input_origin_socket(input);

  /* If the origin socket is an input, that means the input is unlinked, so return a null output
   * socket. */
  if (origin->is_input()) {
    return DOutputSocket();
  }

  /* Now that we know the origin is an output, return a derived output from it. */
  return DOutputSocket(origin);
}

ResultType get_node_socket_result_type(const bNodeSocket *socket)
{
  switch (socket->type) {
    case SOCK_FLOAT:
      return ResultType::Float;
    case SOCK_VECTOR:
      return ResultType::Vector;
    case SOCK_RGBA:
      return ResultType::Color;
    default:
      BLI_assert_unreachable();
      return ResultType::Float;
  }
}

bool is_output_linked_to_node_conditioned(DOutputSocket output, FunctionRef<bool(DNode)> condition)
{
  bool condition_satisfied = false;
  output.foreach_target_socket(
      [&](DInputSocket target, const TargetSocketPathInfo & /*path_info*/) {
        if (condition(target.node())) {
          condition_satisfied = true;
          return;
        }
      });
  return condition_satisfied;
}

int number_of_inputs_linked_to_output_conditioned(DOutputSocket output,
                                                  FunctionRef<bool(DInputSocket)> condition)
{
  int count = 0;
  output.foreach_target_socket(
      [&](DInputSocket target, const TargetSocketPathInfo & /*path_info*/) {
        if (condition(target)) {
          count++;
        }
      });
  return count;
}

bool is_shader_node(DNode node)
{
  return node->typeinfo->get_compositor_shader_node;
}

bool is_node_supported(DNode node)
{
  return node->typeinfo->get_compositor_operation || node->typeinfo->get_compositor_shader_node;
}

InputDescriptor input_descriptor_from_input_socket(const bNodeSocket *socket)
{
  using namespace nodes;
  InputDescriptor input_descriptor;
  input_descriptor.type = get_node_socket_result_type(socket);
  const NodeDeclaration *node_declaration = socket->owner_node().declaration();
  /* Not every node has a declaration, in which case we assume the default values for the rest of
   * the properties. */
  if (!node_declaration) {
    return input_descriptor;
  }
  const SocketDeclarationPtr &socket_declaration = node_declaration->inputs[socket->index()];
  input_descriptor.domain_priority = socket_declaration->compositor_domain_priority();
  input_descriptor.expects_single_value = socket_declaration->compositor_expects_single_value();

  input_descriptor.realization_options.realize_on_operation_domain = bool(
      socket_declaration->compositor_realization_options() &
      CompositorInputRealizationOptions::RealizeOnOperationDomain);
  input_descriptor.realization_options.realize_rotation = bool(
      socket_declaration->compositor_realization_options() &
      CompositorInputRealizationOptions::RealizeRotation);
  input_descriptor.realization_options.realize_scale = bool(
      socket_declaration->compositor_realization_options() &
      CompositorInputRealizationOptions::RealizeScale);

  return input_descriptor;
}

void compute_dispatch_threads_at_least(GPUShader *shader, int2 threads_range, int2 local_size)
{
  /* If the threads range is divisible by the local size, dispatch the number of needed groups,
   * which is their division. If it is not divisible, then dispatch an extra group to cover the
   * remaining invocations, which means the actual threads range of the dispatch will be a bit
   * larger than the given one. */
  const int2 groups_to_dispatch = math::divide_ceil(threads_range, local_size);
  GPU_compute_dispatch(shader, groups_to_dispatch.x, groups_to_dispatch.y, 1);
}

bool is_node_preview_needed(const DNode &node)
{
  if (!(node->flag & NODE_PREVIEW)) {
    return false;
  }

  if (node->flag & NODE_HIDDEN) {
    return false;
  }

  /* Only compute previews for nodes in the active context. */
  if (node.context()->instance_key().value !=
      node.context()->derived_tree().active_context().instance_key().value)
  {
    return false;
  }

  return true;
}

/* Given the size of a result, compute a lower resolution size for a preview. The greater dimension
 * will be assigned an arbitrarily chosen size of 128, while the other dimension will get the size
 * that maintains the same aspect ratio. */
static int2 compute_preview_size(int2 size)
{
  const int greater_dimension_size = 128;
  if (size.x > size.y) {
    return int2(greater_dimension_size, int(greater_dimension_size * (float(size.y) / size.x)));
  }
  else {
    return int2(int(greater_dimension_size * (float(size.x) / size.y)), greater_dimension_size);
  }
}

void compute_preview_from_result(Context &context, const DNode &node, Result &input_result)
{
  /* Initialize node tree previews if not already initialized. */
  bNodeTree *root_tree = const_cast<bNodeTree *>(
      &node.context()->derived_tree().root_context().btree());
  if (!root_tree->previews) {
    root_tree->previews = BKE_node_instance_hash_new("node previews");
  }

  const int2 preview_size = compute_preview_size(input_result.domain().size);
  node->runtime->preview_xsize = preview_size.x;
  node->runtime->preview_ysize = preview_size.y;

  bNodePreview *preview = bke::node_preview_verify(
      root_tree->previews, node.instance_key(), preview_size.x, preview_size.y, true);

  GPUShader *shader = context.shader_manager().get("compositor_compute_preview");
  GPU_shader_bind(shader);

  if (input_result.type() == ResultType::Float) {
    GPU_texture_swizzle_set(input_result.texture(), "rrr1");
  }

  input_result.bind_as_texture(shader, "input_tx");

  Result preview_result = Result::Temporary(ResultType::Color, context.texture_pool());
  preview_result.allocate_texture(Domain(preview_size));
  preview_result.bind_as_image(shader, "preview_img");

  compute_dispatch_threads_at_least(shader, preview_size);

  input_result.unbind_as_texture();
  preview_result.unbind_as_image();
  GPU_shader_unbind();

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);
  float *preview_pixels = static_cast<float *>(
      GPU_texture_read(preview_result.texture(), GPU_DATA_FLOAT, 0));
  preview_result.release();

  ColormanageProcessor *color_processor = IMB_colormanagement_display_processor_new(
      &context.get_scene().view_settings, &context.get_scene().display_settings);

  threading::parallel_for(IndexRange(preview_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(preview_size.x)) {
        const int index = (y * preview_size.x + x) * 4;
        IMB_colormanagement_processor_apply_v4(color_processor, preview_pixels + index);
        rgba_float_to_uchar(preview->ibuf->byte_buffer.data + index, preview_pixels + index);
      }
    }
  });

  /* Restore original swizzle mask set above. */
  if (input_result.type() == ResultType::Float) {
    GPU_texture_swizzle_set(input_result.texture(), "rgba");
  }

  IMB_colormanagement_processor_free(color_processor);
  MEM_freeN(preview_pixels);
}

}  // namespace blender::realtime_compositor
