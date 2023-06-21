/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>

#include "BLI_assert.h"
#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "IMB_colormanagement.h"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_node_declaration.hh"

#include "BKE_node.h"

#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_node_operation.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

NodeOperation::NodeOperation(Context &context, DNode node) : Operation(context), node_(node)
{
  for (const bNodeSocket *output : node->output_sockets()) {
    const ResultType result_type = get_node_socket_result_type(output);
    const Result result = Result(result_type, texture_pool());
    populate_result(output->identifier, result);
  }

  for (const bNodeSocket *input : node->input_sockets()) {
    const InputDescriptor input_descriptor = input_descriptor_from_input_socket(input);
    declare_input_descriptor(input->identifier, input_descriptor);
  }
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

void NodeOperation::compute_preview()
{
  if (!(node()->flag & NODE_PREVIEW)) {
    return;
  }

  if (node()->flag & NODE_HIDDEN) {
    return;
  }

  /* Only compute previews for nodes in the active context. */
  if (node().context()->instance_key().value !=
      node().context()->derived_tree().active_context().instance_key().value)
  {
    return;
  }

  /* Initialize node tree previews if not already initialized. */
  bNodeTree *root_tree = const_cast<bNodeTree *>(
      &node().context()->derived_tree().root_context().btree());
  if (!root_tree->previews) {
    root_tree->previews = BKE_node_instance_hash_new("node previews");
  }

  Result *preview_result = get_preview_result();
  const int2 preview_size = compute_preview_size(preview_result->domain().size);
  node()->runtime->preview_xsize = preview_size.x;
  node()->runtime->preview_ysize = preview_size.y;

  bNodePreview *preview = bke::node_preview_verify(
      root_tree->previews, node().instance_key(), preview_size.x, preview_size.y, true);

  write_preview_from_result(*preview, *preview_result);
}

Result *NodeOperation::get_preview_result()
{
  /* Find the first linked output. */
  for (const bNodeSocket *output : node()->output_sockets()) {
    Result &output_result = get_result(output->identifier);
    if (output_result.should_compute()) {
      return &output_result;
    }
  }

  /* No linked outputs, find the first allocated input. */
  for (const bNodeSocket *input : node()->input_sockets()) {
    Result &input_result = get_input(input->identifier);
    if (input_result.is_allocated()) {
      return &input_result;
    }
  }

  BLI_assert_unreachable();
  return nullptr;
}

void NodeOperation::write_preview_from_result(bNodePreview &preview, Result &input_result)
{
  GPUShader *shader = shader_manager().get("compositor_compute_preview");
  GPU_shader_bind(shader);

  if (input_result.type() == ResultType::Float) {
    GPU_texture_swizzle_set(input_result.texture(), "rrr1");
  }

  input_result.bind_as_texture(shader, "input_tx");

  const int2 preview_size = int2(preview.xsize, preview.ysize);
  Result preview_result = Result::Temporary(ResultType::Color, texture_pool());
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
      &context().get_scene().view_settings, &context().get_scene().display_settings);

  threading::parallel_for(IndexRange(preview_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(preview_size.x)) {
        const int index = (y * preview_size.x + x) * 4;
        IMB_colormanagement_processor_apply_v4(color_processor, preview_pixels + index);
        rgba_float_to_uchar(preview.rect + index, preview_pixels + index);
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

void NodeOperation::compute_results_reference_counts(const Schedule &schedule)
{
  for (const bNodeSocket *output : this->node()->output_sockets()) {
    const DOutputSocket doutput{node().context(), output};

    const int reference_count = number_of_inputs_linked_to_output_conditioned(
        doutput, [&](DInputSocket input) { return schedule.contains(input.node()); });

    get_result(doutput->identifier).set_initial_reference_count(reference_count);
  }
}

const DNode &NodeOperation::node() const
{
  return node_;
}

const bNode &NodeOperation::bnode() const
{
  return *node_;
}

bool NodeOperation::should_compute_output(StringRef identifier)
{
  return get_result(identifier).should_compute();
}

}  // namespace blender::realtime_compositor
