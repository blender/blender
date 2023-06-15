/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_KuwaharaClassicOperation.h"

#include "BLI_math_vector_types.hh"
#include "IMB_colormanagement.h"

namespace blender::compositor {

KuwaharaClassicOperation::KuwaharaClassicOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  this->set_kernel_size(4);

  this->flags_.is_fullframe_operation = true;
}

void KuwaharaClassicOperation::init_execution()
{
  image_reader_ = this->get_input_socket_reader(0);
}

void KuwaharaClassicOperation::deinit_execution()
{
  image_reader_ = nullptr;
}

void KuwaharaClassicOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  Vector<float3> mean(4, float3(0.0f));
  float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float var[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  int cnt[4] = {0, 0, 0, 0};

  /* Split surroundings of pixel into 4 overlapping regions. */
  for (int dy = -kernel_size_; dy <= kernel_size_; dy++) {
    for (int dx = -kernel_size_; dx <= kernel_size_; dx++) {

      int xx = x + dx;
      int yy = y + dy;
      if (xx >= 0 && yy >= 0 && xx < this->get_width() && yy < this->get_height()) {

        float4 color;
        image_reader_->read_sampled(color, xx, yy, sampler);
        const float3 v = color.xyz();

        const float lum = IMB_colormanagement_get_luminance(color);

        if (dx <= 0 && dy <= 0) {
          mean[0] += v;
          sum[0] += lum;
          var[0] += lum * lum;
          cnt[0]++;
        }

        if (dx >= 0 && dy <= 0) {
          mean[1] += v;
          sum[1] += lum;
          var[1] += lum * lum;
          cnt[1]++;
        }

        if (dx <= 0 && dy >= 0) {
          mean[2] += v;
          sum[2] += lum;
          var[2] += lum * lum;
          cnt[2]++;
        }

        if (dx >= 0 && dy >= 0) {
          mean[3] += v;
          sum[3] += lum;
          var[3] += lum * lum;
          cnt[3]++;
        }
      }
    }
  }

  /* Compute region variances. */
  for (int i = 0; i < 4; i++) {
    mean[i] = cnt[i] != 0 ? mean[i] / cnt[i] : float3{0.0f, 0.0f, 0.0f};
    sum[i] = cnt[i] != 0 ? sum[i] / cnt[i] : 0.0f;
    var[i] = cnt[i] != 0 ? var[i] / cnt[i] : 0.0f;
    const float temp = sum[i] * sum[i];
    var[i] = var[i] > temp ? sqrt(var[i] - temp) : 0.0f;
  }

  /* Choose the region with lowest variance. */
  float min_var = FLT_MAX;
  int min_index = 0;
  for (int i = 0; i < 4; i++) {
    if (var[i] < min_var) {
      min_var = var[i];
      min_index = i;
    }
  }
  output[0] = mean[min_index].x;
  output[1] = mean[min_index].y;
  output[2] = mean[min_index].z;

  /* No changes for alpha channel. */
  float tmp[4];
  image_reader_->read_sampled(tmp, x, y, sampler);
  output[3] = tmp[3];
}

void KuwaharaClassicOperation::set_kernel_size(int kernel_size)
{
  kernel_size_ = kernel_size;
}

int KuwaharaClassicOperation::get_kernel_size()
{
  return kernel_size_;
}

void KuwaharaClassicOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *image = inputs[0];

  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;

    Vector<float3> mean(4, float3(0.0f));
    float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float var[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int cnt[4] = {0, 0, 0, 0};

    /* Split surroundings of pixel into 4 overlapping regions. */
    for (int dy = -kernel_size_; dy <= kernel_size_; dy++) {
      for (int dx = -kernel_size_; dx <= kernel_size_; dx++) {

        int xx = x + dx;
        int yy = y + dy;
        if (xx >= 0 && yy >= 0 && xx < image->get_width() && yy < image->get_height()) {

          float4 color;
          image->read_elem(xx, yy, &color.x);
          const float3 v = color.xyz();

          const float lum = IMB_colormanagement_get_luminance(color);

          if (dx <= 0 && dy <= 0) {
            mean[0] += v;
            sum[0] += lum;
            var[0] += lum * lum;
            cnt[0]++;
          }

          if (dx >= 0 && dy <= 0) {
            mean[1] += v;
            sum[1] += lum;
            var[1] += lum * lum;
            cnt[1]++;
          }

          if (dx <= 0 && dy >= 0) {
            mean[2] += v;
            sum[2] += lum;
            var[2] += lum * lum;
            cnt[2]++;
          }

          if (dx >= 0 && dy >= 0) {
            mean[3] += v;
            sum[3] += lum;
            var[3] += lum * lum;
            cnt[3]++;
          }
        }
      }
    }

    /* Compute region variances. */
    for (int i = 0; i < 4; i++) {
      mean[i] = cnt[i] != 0 ? mean[i] / cnt[i] : float3{0.0f, 0.0f, 0.0f};
      sum[i] = cnt[i] != 0 ? sum[i] / cnt[i] : 0.0f;
      var[i] = cnt[i] != 0 ? var[i] / cnt[i] : 0.0f;
      const float temp = sum[i] * sum[i];
      var[i] = var[i] > temp ? sqrt(var[i] - temp) : 0.0f;
    }

    /* Choose the region with lowest variance. */
    float min_var = FLT_MAX;
    int min_index = 0;
    for (int i = 0; i < 4; i++) {
      if (var[i] < min_var) {
        min_var = var[i];
        min_index = i;
      }
    }
    it.out[0] = mean[min_index].x;
    it.out[1] = mean[min_index].y;
    it.out[2] = mean[min_index].z;

    /* No changes for alpha channel. */
    it.out[3] = image->get_value(x, y, 3);
  }
}

}  // namespace blender::compositor
