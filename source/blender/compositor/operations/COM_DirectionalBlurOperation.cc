/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DirectionalBlurOperation.h"
#include "COM_OpenCLDevice.h"

namespace blender::compositor {

DirectionalBlurOperation::DirectionalBlurOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  flags_.complex = true;
  flags_.open_cl = true;
  input_program_ = nullptr;
}

void DirectionalBlurOperation::init_execution()
{
  input_program_ = get_input_socket_reader(0);
  QualityStepHelper::init_execution(COM_QH_INCREASE);
  const float angle = data_->angle;
  const float zoom = data_->zoom;
  const float spin = data_->spin;
  const float iterations = data_->iter;
  const float distance = data_->distance;
  const float center_x = data_->center_x;
  const float center_y = data_->center_y;
  const float width = get_width();
  const float height = get_height();

  const float a = angle;
  const float itsc = 1.0f / powf(2.0f, float(iterations));
  float D;

  D = distance * sqrtf(width * width + height * height);
  center_x_pix_ = center_x * width;
  center_y_pix_ = center_y * height;

  tx_ = itsc * D * cosf(a);
  ty_ = -itsc * D * sinf(a);
  sc_ = itsc * zoom;
  rot_ = itsc * spin;
}

void DirectionalBlurOperation::execute_pixel(float output[4], int x, int y, void * /*data*/)
{
  const int iterations = pow(2.0f, data_->iter);
  float col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float col2[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  input_program_->read_sampled(col2, x, y, PixelSampler::Bilinear);
  float ltx = tx_;
  float lty = ty_;
  float lsc = sc_;
  float lrot = rot_;
  /* blur the image */
  for (int i = 0; i < iterations; i++) {
    const float cs = cosf(lrot), ss = sinf(lrot);
    const float isc = 1.0f / (1.0f + lsc);

    const float v = isc * (y - center_y_pix_) + lty;
    const float u = isc * (x - center_x_pix_) + ltx;

    input_program_->read_sampled(col,
                                 cs * u + ss * v + center_x_pix_,
                                 cs * v - ss * u + center_y_pix_,
                                 PixelSampler::Bilinear);

    add_v4_v4(col2, col);

    /* double transformations */
    ltx += tx_;
    lty += ty_;
    lrot += rot_;
    lsc += sc_;
  }

  mul_v4_v4fl(output, col2, 1.0f / (iterations + 1));
}

void DirectionalBlurOperation::execute_opencl(OpenCLDevice *device,
                                              MemoryBuffer *output_memory_buffer,
                                              cl_mem cl_output_buffer,
                                              MemoryBuffer **input_memory_buffers,
                                              std::list<cl_mem> *cl_mem_to_clean_up,
                                              std::list<cl_kernel> * /*cl_kernels_to_clean_up*/)
{
  cl_kernel directional_blur_kernel = device->COM_cl_create_kernel("directional_blur_kernel",
                                                                   nullptr);

  cl_int iterations = pow(2.0f, data_->iter);
  cl_float2 ltxy = {{tx_, ty_}};
  cl_float2 centerpix = {{center_x_pix_, center_y_pix_}};
  cl_float lsc = sc_;
  cl_float lrot = rot_;

  device->COM_cl_attach_memory_buffer_to_kernel_parameter(
      directional_blur_kernel, 0, -1, cl_mem_to_clean_up, input_memory_buffers, input_program_);
  device->COM_cl_attach_output_memory_buffer_to_kernel_parameter(
      directional_blur_kernel, 1, cl_output_buffer);
  device->COM_cl_attach_memory_buffer_offset_to_kernel_parameter(
      directional_blur_kernel, 2, output_memory_buffer);
  clSetKernelArg(directional_blur_kernel, 3, sizeof(cl_int), &iterations);
  clSetKernelArg(directional_blur_kernel, 4, sizeof(cl_float), &lsc);
  clSetKernelArg(directional_blur_kernel, 5, sizeof(cl_float), &lrot);
  clSetKernelArg(directional_blur_kernel, 6, sizeof(cl_float2), &ltxy);
  clSetKernelArg(directional_blur_kernel, 7, sizeof(cl_float2), &centerpix);

  device->COM_cl_enqueue_range(directional_blur_kernel, output_memory_buffer, 8, this);
}

void DirectionalBlurOperation::deinit_execution()
{
  input_program_ = nullptr;
}

bool DirectionalBlurOperation::determine_depending_area_of_interest(
    rcti * /*input*/, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;

  new_input.xmax = this->get_width();
  new_input.xmin = 0;
  new_input.ymax = this->get_height();
  new_input.ymin = 0;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void DirectionalBlurOperation::get_area_of_interest(const int input_idx,
                                                    const rcti & /*output_area*/,
                                                    rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area = this->get_canvas();
}

void DirectionalBlurOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  const int iterations = pow(2.0f, data_->iter);
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;
    float color_accum[4];
    input->read_elem_bilinear(x, y, color_accum);

    /* Blur pixel. */
    /* TODO(manzanilla): Many values used on iterations can be calculated beforehand. Create a
     * table on operation initialization. */
    float ltx = tx_;
    float lty = ty_;
    float lsc = sc_;
    float lrot = rot_;
    for (int i = 0; i < iterations; i++) {
      const float cs = cosf(lrot), ss = sinf(lrot);
      const float isc = 1.0f / (1.0f + lsc);

      const float v = isc * (y - center_y_pix_) + lty;
      const float u = isc * (x - center_x_pix_) + ltx;

      float color[4];
      input->read_elem_bilinear(
          cs * u + ss * v + center_x_pix_, cs * v - ss * u + center_y_pix_, color);
      add_v4_v4(color_accum, color);

      /* Double transformations. */
      ltx += tx_;
      lty += ty_;
      lrot += rot_;
      lsc += sc_;
    }

    mul_v4_v4fl(it.out, color_accum, 1.0f / (iterations + 1));
  }
}

}  // namespace blender::compositor
