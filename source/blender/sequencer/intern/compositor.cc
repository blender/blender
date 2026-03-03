/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BKE_node_runtime.hh"
#include "COM_realize_on_domain_operation.hh"
#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "compositor.hh"

namespace blender::seq {

compositor::ResultPrecision CompositorContext::get_precision() const
{
  switch (this->render_data_.scene->r.compositor_precision) {
    case SCE_COMPOSITOR_PRECISION_AUTO:
      /* Auto uses full precision for final renders and half precision otherwise. */
      return this->render_data_.render ? compositor::ResultPrecision::Full :
                                         compositor::ResultPrecision::Half;
    case SCE_COMPOSITOR_PRECISION_FULL:
      return compositor::ResultPrecision::Full;
  }
  BLI_assert_unreachable();
  return compositor::ResultPrecision::Half;
}

void CompositorContext::create_result_from_input(compositor::Result &result,
                                                 const ImBuf &input) const
{
  BLI_assert(input.float_buffer.data);
  const bool gpu = this->use_gpu();
  const int2 size = int2(input.x, input.y);
  if (!gpu) {
    result.wrap_external(input.float_buffer.data, size);
  }
  else {
    result.allocate_texture(size);
    GPU_texture_update(result, GPU_DATA_FLOAT, input.float_buffer.data);
  }
}

void CompositorContext::write_output(const compositor::Result &result, ImBuf &image)
{
  /* Do not write the output if the viewer output was already written. */
  if (viewer_was_written_) {
    return;
  }

  if (result.is_single_value()) {
    IMB_rectfill(&image, result.get_single_value<compositor::Color>());
    return;
  }

  compositor::Result result_cpu = this->use_gpu() ? result.download_to_cpu() : result;

  result_translation_ = result_cpu.domain().transformation.location();
  const int output_size_x = result.domain().data_size.x;
  const int output_size_y = result.domain().data_size.y;
  if (output_size_x != image.x || output_size_y != image.y) {
    /* Output size is different (e.g. image is blurred with expanded bounds);
     * need to allocate appropriately sized buffer. */
    IMB_free_all_data(&image);
    image.x = output_size_x;
    image.y = output_size_y;
    IMB_alloc_float_pixels(&image, 4, false);
  }
  std::memcpy(image.float_buffer.data,
              result_cpu.cpu_data().data(),
              IMB_get_pixel_count(&image) * sizeof(float) * 4);

  if (this->use_gpu()) {
    result_cpu.release();
  }
}

void CompositorContext::write_outputs(const bNodeTree &node_group,
                                      compositor::NodeGroupOperation &node_group_operation,
                                      ImBuf &output_image)
{
  using namespace compositor;
  for (const bNodeTreeInterfaceSocket *output_socket : node_group.interface_outputs()) {
    Result &output_result = node_group_operation.get_result(output_socket->identifier);
    if (!output_result.should_compute()) {
      continue;
    }

    /* Realize the output transforms if needed. */
    const InputDescriptor input_descriptor = {ResultType::Color,
                                              InputRealizationMode::OperationDomain};
    SimpleOperation *realization_operation = RealizeOnDomainOperation::construct_if_needed(
        *this, output_result, input_descriptor, output_result.domain());
    if (realization_operation) {
      realization_operation->map_input_to_result(&output_result);
      realization_operation->evaluate();
      Result &realized_output_result = realization_operation->get_result();
      this->write_output(realized_output_result, output_image);
      realized_output_result.release();
      delete realization_operation;
      continue;
    }

    this->write_output(output_result, output_image);
    output_result.release();
  }
}

void CompositorContext::set_output_refcount(const bNodeTree &node_group,
                                            compositor::NodeGroupOperation &node_group_operation)
{
  using namespace compositor;
  /* Set the reference count for the outputs, only the first color output is actually needed,
   * while the rest are ignored. */
  node_group.ensure_interface_cache();
  for (const bNodeTreeInterfaceSocket *output_socket : node_group.interface_outputs()) {
    const bool is_first_output = output_socket == node_group.interface_outputs().first();
    Result &output_result = node_group_operation.get_result(output_socket->identifier);
    const bool is_color = output_result.type() == ResultType::Color;
    output_result.set_reference_count(is_first_output && is_color ? 1 : 0);
  }
}

bool is_linear_float_buffer(const ImBuf *image_buffer)
{
  return image_buffer->float_buffer.data &&
         IMB_colormanagement_space_is_scene_linear(image_buffer->float_buffer.colorspace);
}

}  // namespace blender::seq
