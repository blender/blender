/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BKE_node_runtime.hh"

#include "COM_ocio_color_space_conversion_shader.hh"
#include "COM_realize_on_domain_operation.hh"
#include "COM_utilities.hh"

#include "GPU_state.hh"
#include "GPU_texture_pool.hh"

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

void CompositorContext::create_result_from_input(compositor::Result &result, ImBuf &input)
{
  const bool gpu = this->use_gpu();
  const int2 size = int2(input.x, input.y);
  if (!gpu) {
    /* CPU path: ensure input is linear float. */
    ensure_ibuf_is_linear_space(&input, true);
    BLI_assert(input.float_data());
    result.share_data(input.float_data(), size);
    return;
  }

  /* GPU path: do necessary color space conversions (if any) to linear space on the GPU. */
  const bool input_is_byte = input.float_data() == nullptr;
  const char *input_colorspace = input_is_byte ? IMB_colormanagement_get_byte_colorspace(&input) :
                                                 IMB_colormanagement_get_float_colorspace(&input);
  const char *linear_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);

  bool use_fallback = true;

  if (input_is_byte || !STREQ(input_colorspace, linear_colorspace)) {
    /* Need to convert data format or colorspace: upload input into temporary texture,
     * convert into compositor result. */

    /* Get the conversion shader. */
    compositor::OCIOColorSpaceConversionShader &ocio_shader =
        this->cache_manager().ocio_color_space_conversion_shaders.get(
            *this, input_colorspace, linear_colorspace);
    gpu::Shader *shader = ocio_shader.bind_shader_and_resources();
    if (shader) {

      /* Upload input image into a GPU texture. */
      gpu::TexturePool &pool = gpu::TexturePool::get();
      gpu::Texture *input_tex = pool.acquire_texture(size,
                                                     input_is_byte ?
                                                         gpu::TextureFormat::UNORM_8_8_8_8 :
                                                         gpu::TextureFormat::SFLOAT_32_32_32_32,
                                                     GPU_TEXTURE_USAGE_SHADER_READ,
                                                     "seq_comp_input");
      if (input_tex) {
        if (input_is_byte) {
          GPU_texture_update(input_tex, GPU_DATA_UBYTE, input.byte_data());
        }
        else {
          GPU_texture_update(input_tex, GPU_DATA_FLOAT, input.float_data());
        }

        /* Allocate compositor result texture. We use global compositor precision even
         * for byte inputs. In theory Half precision should be enough, but that leads to potential
         * small differences between CPU & GPU paths. */
        result.set_precision(get_precision());
        result.allocate_texture(size);

        /* Convert input texture into the compositor result texture. */
        GPU_texture_bind(input_tex,
                         GPU_shader_get_sampler_binding(shader, ocio_shader.input_sampler_name()));
        result.bind_as_image(shader, ocio_shader.output_image_name());

        GPU_shader_uniform_1b(shader, "premultiply_output", input_is_byte);

        compositor::compute_dispatch_threads_at_least(shader, size);

        GPU_texture_unbind(input_tex);
        result.unbind_as_image();
        pool.release_texture(input_tex);

        use_fallback = false;
      }
      ocio_shader.unbind_shader_and_resources();
    }
  }

  /* Colorspace conversion was not needed or failed: upload input float data into
   * compositor result. */
  if (use_fallback) {
    /* This is a no-op if input is already linear float; otherwise this step might be needed
     * if conversion above has failed. */
    ensure_ibuf_is_linear_space(&input, true);
    result.allocate_texture(size);
    GPU_texture_update(result, GPU_DATA_FLOAT, input.float_data());
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

  result_translation_ = result.domain().transformation.location();
  const int output_size_x = result.domain().data_size.x;
  const int output_size_y = result.domain().data_size.y;
  if (output_size_x != image.x || output_size_y != image.y || !image.float_buffer.data) {
    /* Output size is different (e.g. image is blurred with expanded bounds);
     * need to allocate appropriately sized buffer. */
    IMB_free_all_data(&image);
    image.x = output_size_x;
    image.y = output_size_y;
    IMB_alloc_float_pixels(&image, 4, false);
  }

  if (this->use_gpu()) {
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_read(result.gpu_texture(), GPU_DATA_FLOAT, 0, image.float_data_for_write());
  }
  else {
    std::memcpy(image.float_data_for_write(),
                result.cpu_data().data(),
                IMB_get_pixel_count(&image) * sizeof(float) * 4);
  }
  const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);
  IMB_colormanagement_assign_float_colorspace(&image, to_colorspace);
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

}  // namespace blender::seq
