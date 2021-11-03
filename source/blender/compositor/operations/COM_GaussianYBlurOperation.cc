/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

#include "COM_GaussianYBlurOperation.h"
#include "COM_OpenCLDevice.h"

namespace blender::compositor {

GaussianYBlurOperation::GaussianYBlurOperation() : GaussianBlurBaseOperation(eDimension::Y)
{
}

void *GaussianYBlurOperation::initialize_tile_data(rcti * /*rect*/)
{
  lock_mutex();
  if (!sizeavailable_) {
    update_gauss();
  }
  void *buffer = get_input_operation(0)->initialize_tile_data(nullptr);
  unlock_mutex();
  return buffer;
}

void GaussianYBlurOperation::init_execution()
{
  GaussianBlurBaseOperation::init_execution();

  init_mutex();

  if (sizeavailable_ && execution_model_ == eExecutionModel::Tiled) {
    float rad = max_ff(size_ * data_.sizey, 0.0f);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    gausstab_ = BlurBaseOperation::make_gausstab(rad, filtersize_);
#ifdef BLI_HAVE_SSE2
    gausstab_sse_ = BlurBaseOperation::convert_gausstab_sse(gausstab_, filtersize_);
#endif
  }
}

void GaussianYBlurOperation::update_gauss()
{
  if (gausstab_ == nullptr) {
    update_size();
    float rad = max_ff(size_ * data_.sizey, 0.0f);
    rad = min_ff(rad, MAX_GAUSSTAB_RADIUS);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    gausstab_ = BlurBaseOperation::make_gausstab(rad, filtersize_);
#ifdef BLI_HAVE_SSE2
    gausstab_sse_ = BlurBaseOperation::convert_gausstab_sse(gausstab_, filtersize_);
#endif
  }
}

void GaussianYBlurOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  float ATTR_ALIGN(16) color_accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float multiplier_accum = 0.0f;
  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  const rcti &input_rect = input_buffer->get_rect();
  float *buffer = input_buffer->get_buffer();
  int bufferwidth = input_buffer->get_width();
  int bufferstartx = input_rect.xmin;
  int bufferstarty = input_rect.ymin;

  int xmin = max_ii(x, input_rect.xmin);
  int ymin = max_ii(y - filtersize_, input_rect.ymin);
  int ymax = min_ii(y + filtersize_ + 1, input_rect.ymax);

  int index;
  int step = get_step();
  const int buffer_indexx = ((xmin - bufferstartx) * 4);

#ifdef BLI_HAVE_SSE2
  __m128 accum_r = _mm_load_ps(color_accum);
  for (int ny = ymin; ny < ymax; ny += step) {
    index = (ny - y) + filtersize_;
    int bufferindex = buffer_indexx + ((ny - bufferstarty) * 4 * bufferwidth);
    const float multiplier = gausstab_[index];
    __m128 reg_a = _mm_load_ps(&buffer[bufferindex]);
    reg_a = _mm_mul_ps(reg_a, gausstab_sse_[index]);
    accum_r = _mm_add_ps(accum_r, reg_a);
    multiplier_accum += multiplier;
  }
  _mm_store_ps(color_accum, accum_r);
#else
  for (int ny = ymin; ny < ymax; ny += step) {
    index = (ny - y) + filtersize_;
    int bufferindex = buffer_indexx + ((ny - bufferstarty) * 4 * bufferwidth);
    const float multiplier = gausstab_[index];
    madd_v4_v4fl(color_accum, &buffer[bufferindex], multiplier);
    multiplier_accum += multiplier;
  }
#endif
  mul_v4_v4fl(output, color_accum, 1.0f / multiplier_accum);
}

void GaussianYBlurOperation::execute_opencl(OpenCLDevice *device,
                                            MemoryBuffer *output_memory_buffer,
                                            cl_mem cl_output_buffer,
                                            MemoryBuffer **input_memory_buffers,
                                            std::list<cl_mem> *cl_mem_to_clean_up,
                                            std::list<cl_kernel> * /*cl_kernels_to_clean_up*/)
{
  cl_kernel gaussian_yblur_operation_kernel = device->COM_cl_create_kernel(
      "gaussian_yblur_operation_kernel", nullptr);
  cl_int filter_size = filtersize_;

  cl_mem gausstab = clCreateBuffer(device->get_context(),
                                   CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                   sizeof(float) * (filtersize_ * 2 + 1),
                                   gausstab_,
                                   nullptr);

  device->COM_cl_attach_memory_buffer_to_kernel_parameter(gaussian_yblur_operation_kernel,
                                                          0,
                                                          1,
                                                          cl_mem_to_clean_up,
                                                          input_memory_buffers,
                                                          input_program_);
  device->COM_cl_attach_output_memory_buffer_to_kernel_parameter(
      gaussian_yblur_operation_kernel, 2, cl_output_buffer);
  device->COM_cl_attach_memory_buffer_offset_to_kernel_parameter(
      gaussian_yblur_operation_kernel, 3, output_memory_buffer);
  clSetKernelArg(gaussian_yblur_operation_kernel, 4, sizeof(cl_int), &filter_size);
  device->COM_cl_attach_size_to_kernel_parameter(gaussian_yblur_operation_kernel, 5, this);
  clSetKernelArg(gaussian_yblur_operation_kernel, 6, sizeof(cl_mem), &gausstab);

  device->COM_cl_enqueue_range(gaussian_yblur_operation_kernel, output_memory_buffer, 7, this);

  clReleaseMemObject(gausstab);
}

void GaussianYBlurOperation::deinit_execution()
{
  GaussianBlurBaseOperation::deinit_execution();

  if (gausstab_) {
    MEM_freeN(gausstab_);
    gausstab_ = nullptr;
  }
#ifdef BLI_HAVE_SSE2
  if (gausstab_sse_) {
    MEM_freeN(gausstab_sse_);
    gausstab_sse_ = nullptr;
  }
#endif

  deinit_mutex();
}

bool GaussianYBlurOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;

  if (!sizeavailable_) {
    rcti size_input;
    size_input.xmin = 0;
    size_input.ymin = 0;
    size_input.xmax = 5;
    size_input.ymax = 5;
    NodeOperation *operation = this->get_input_operation(1);
    if (operation->determine_depending_area_of_interest(&size_input, read_operation, output)) {
      return true;
    }
  }
  {
    if (sizeavailable_ && gausstab_ != nullptr) {
      new_input.xmax = input->xmax;
      new_input.xmin = input->xmin;
      new_input.ymax = input->ymax + filtersize_ + 1;
      new_input.ymin = input->ymin - filtersize_ - 1;
    }
    else {
      new_input.xmax = this->get_width();
      new_input.xmin = 0;
      new_input.ymax = this->get_height();
      new_input.ymin = 0;
    }
    return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
  }
}

}  // namespace blender::compositor
