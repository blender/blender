/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_KuwaharaAnisotropicOperation.h"

#include "BLI_math_base.hh"
#include "BLI_vector.hh"
#include "IMB_colormanagement.h"

namespace blender::compositor {

KuwaharaAnisotropicOperation::KuwaharaAnisotropicOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);

  this->add_output_socket(DataType::Color);

  this->n_div_ = 8;
  this->set_kernel_size(5);

  this->flags_.is_fullframe_operation = true;
}

void KuwaharaAnisotropicOperation::init_execution()
{
  image_reader_ = this->get_input_socket_reader(0);
  s_xx_reader_ = this->get_input_socket_reader(1);
  s_yy_reader_ = this->get_input_socket_reader(2);
  s_xy_reader_ = this->get_input_socket_reader(3);
}

void KuwaharaAnisotropicOperation::deinit_execution()
{
  image_reader_ = nullptr;
}

void KuwaharaAnisotropicOperation::execute_pixel_sampled(float output[4],
                                                         float x,
                                                         float y,
                                                         PixelSampler sampler)
{
  const int width = this->get_width();
  const int height = this->get_height();

  BLI_assert(width == s_xx_reader_->get_width());
  BLI_assert(height == s_xx_reader_->get_height());
  BLI_assert(width == s_yy_reader_->get_width());
  BLI_assert(height == s_yy_reader_->get_height());
  BLI_assert(width == s_xy_reader_->get_width());
  BLI_assert(height == s_xy_reader_->get_height());

  /* Values recommended by authors in original paper. */
  const float angle = 2.0 * M_PI / n_div_;
  const float q = 3.0;
  const float EPS = 1.0e-10;

  /* All channels are identical. Take first channel for simplicity. */
  float tmp[4];
  s_xx_reader_->read(tmp, x, y, nullptr);
  const float a = tmp[1];
  s_xy_reader_->read(tmp, x, y, nullptr);
  const float b = tmp[1];
  s_yy_reader_->read(tmp, x, y, nullptr);
  const float c = tmp[1];

  /* Compute egenvalues of structure tensor. */
  const double tr = a + c;
  const double discr = sqrt((a - b) * (a - b) + 4 * b * c);
  const double lambda1 = (tr + discr) / 2;
  const double lambda2 = (tr - discr) / 2;

  /* Compute orientation and its strength based on structure tensor. */
  const double orientation = 0.5 * atan2(2 * b, a - c);
  const double strength = (lambda1 == 0 && lambda2 == 0) ?
                              0 :
                              (lambda1 - lambda2) / (lambda1 + lambda2);

  Vector<double> mean(n_div_);
  Vector<double> sum(n_div_);
  Vector<double> var(n_div_);
  Vector<double> weight(n_div_);

  for (int ch = 0; ch < 3; ch++) {
    mean.fill(0.0);
    sum.fill(0.0);
    var.fill(0.0);
    weight.fill(0.0);

    double sx = 1.0f / (strength + 1.0f);
    double sy = (1.0f + strength) / 1.0f;
    double theta = -orientation;

    for (int dy = -kernel_size_; dy <= kernel_size_; dy++) {
      for (int dx = -kernel_size_; dx <= kernel_size_; dx++) {
        if (dx == 0 && dy == 0)
          continue;

        /* Rotate and scale the kernel. This is the "anisotropic" part. */
        int dx2 = int(sx * (cos(theta) * dx - sin(theta) * dy));
        int dy2 = int(sy * (sin(theta) * dx + cos(theta) * dy));

        /* Clamp image to avoid artifacts at borders. */
        const int xx = math::clamp(int(x) + dx2, 0, width - 1);
        const int yy = math::clamp(int(y) + dy2, 0, height - 1);

        const double ddx2 = double(dx2);
        const double ddy2 = double(dy2);
        const double theta = atan2(ddy2, ddx2) + M_PI;
        const int t = int(floor(theta / angle)) % n_div_;
        double d2 = dx2 * dx2 + dy2 * dy2;
        double g = exp(-d2 / (2.0 * kernel_size_));
        float color[4];
        image_reader_->read(color, xx, yy, nullptr);
        const double v = color[ch];
        /* TODO(@zazizizou): only compute lum once per region */
        const float lum = IMB_colormanagement_get_luminance(color);
        /* TODO(@zazizizou): only compute mean for the selected region */
        mean[t] += g * v;
        sum[t] += g * lum;
        var[t] += g * lum * lum;
        weight[t] += g;
      }
    }

    /* Calculate weighted average */
    double de = 0.0;
    double nu = 0.0;
    for (int i = 0; i < n_div_; i++) {
      double weight_inv = 1.0 / weight[i];
      mean[i] = weight[i] != 0 ? mean[i] * weight_inv : 0.0;
      sum[i] = weight[i] != 0 ? sum[i] * weight_inv : 0.0;
      var[i] = weight[i] != 0 ? var[i] * weight_inv : 0.0;
      var[i] = var[i] - sum[i] * sum[i];
      var[i] = var[i] > FLT_EPSILON ? sqrt(var[i]) : FLT_EPSILON;
      double w = powf(var[i], -q);

      de += mean[i] * w;
      nu += w;
    }

    double val = nu > EPS ? de / nu : 0.0;
    output[ch] = val;
  }

  /* No changes for alpha channel. */
  image_reader_->read_sampled(tmp, x, y, sampler);
  output[3] = tmp[3];
}

void KuwaharaAnisotropicOperation::set_kernel_size(int kernel_size)
{
  /* Filter will be split into n_div.
   * Add n_div / 2 to avoid artifacts such as random black pixels in image. */
  kernel_size_ = kernel_size + n_div_ / 2;
}

int KuwaharaAnisotropicOperation::get_kernel_size()
{
  return kernel_size_;
}

int KuwaharaAnisotropicOperation::get_n_div()
{
  return n_div_;
}

void KuwaharaAnisotropicOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                const rcti &area,
                                                                Span<MemoryBuffer *> inputs)
{
  /* Implementation based on Kyprianidis, Jan & Kang, Henry & Döllner, Jürgen. (2009).
   * "Image and Video Abstraction by Anisotropic Kuwahara Filtering".
   * Comput. Graph. Forum. 28. 1955-1963. 10.1111/j.1467-8659.2009.01574.x.
   * Used reference implementation from lime image processing library (MIT license). */

  MemoryBuffer *image = inputs[0];
  MemoryBuffer *s_xx = inputs[1];
  MemoryBuffer *s_yy = inputs[2];
  MemoryBuffer *s_xy = inputs[3];

  const int width = image->get_width();
  const int height = image->get_height();

  BLI_assert(width == s_xx->get_width());
  BLI_assert(height == s_xx->get_height());
  BLI_assert(width == s_yy->get_width());
  BLI_assert(height == s_yy->get_height());
  BLI_assert(width == s_xy->get_width());
  BLI_assert(height == s_xy->get_height());

  /* Values recommended by authors in original paper. */
  const float angle = 2.0 * M_PI / n_div_;
  const float q = 3.0;
  const float EPS = 1.0e-10;

  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;

    /* All channels are identical. Take first channel for simplicity. */
    const float a = s_xx->get_value(x, y, 0);
    const float b = s_xy->get_value(x, y, 0);
    const float c = s_yy->get_value(x, y, 0);

    /* Compute egenvalues of structure tensor */
    const double tr = a + c;
    const double discr = sqrt((a - b) * (a - b) + 4 * b * c);
    const double lambda1 = (tr + discr) / 2;
    const double lambda2 = (tr - discr) / 2;

    /* Compute orientation and its strength based on structure tensor. */
    const double orientation = 0.5 * atan2(2 * b, a - c);
    const double strength = (lambda1 == 0 && lambda2 == 0) ?
                                0 :
                                (lambda1 - lambda2) / (lambda1 + lambda2);

    Vector<double> mean(n_div_);
    Vector<double> sum(n_div_);
    Vector<double> var(n_div_);
    Vector<double> weight(n_div_);

    for (int ch = 0; ch < 3; ch++) {
      mean.fill(0.0);
      sum.fill(0.0);
      var.fill(0.0);
      weight.fill(0.0);

      double sx = 1.0f / (strength + 1.0f);
      double sy = (1.0f + strength) / 1.0f;
      double theta = -orientation;

      for (int dy = -kernel_size_; dy <= kernel_size_; dy++) {
        for (int dx = -kernel_size_; dx <= kernel_size_; dx++) {
          if (dx == 0 && dy == 0)
            continue;

          /* Rotate and scale the kernel. This is the "anisotropic" part. */
          int dx2 = int(sx * (cos(theta) * dx - sin(theta) * dy));
          int dy2 = int(sy * (sin(theta) * dx + cos(theta) * dy));

          /* Clamp image to avoid artifacts at borders. */
          const int xx = math::clamp(x + dx2, 0, width - 1);
          const int yy = math::clamp(y + dy2, 0, height - 1);

          const double ddx2 = double(dx2);
          const double ddy2 = double(dy2);
          const double theta = atan2(ddy2, ddx2) + M_PI;
          const int t = int(floor(theta / angle)) % n_div_;
          double d2 = dx2 * dx2 + dy2 * dy2;
          double g = exp(-d2 / (2.0 * kernel_size_));
          const double v = image->get_value(xx, yy, ch);
          float color[4];
          image->read_elem(xx, yy, color);
          /* TODO(@zazizizou): only compute lum once per region. */
          const float lum = IMB_colormanagement_get_luminance(color);
          /* TODO(@zazizizou): only compute mean for the selected region. */
          mean[t] += g * v;
          sum[t] += g * lum;
          var[t] += g * lum * lum;
          weight[t] += g;
        }
      }

      /* Calculate weighted average. */
      double de = 0.0;
      double nu = 0.0;
      for (int i = 0; i < n_div_; i++) {
        double weight_inv = 1.0 / weight[i];
        mean[i] = weight[i] != 0 ? mean[i] * weight_inv : 0.0;
        sum[i] = weight[i] != 0 ? sum[i] * weight_inv : 0.0;
        var[i] = weight[i] != 0 ? var[i] * weight_inv : 0.0;
        var[i] = var[i] - sum[i] * sum[i];
        var[i] = var[i] > FLT_EPSILON ? sqrt(var[i]) : FLT_EPSILON;
        double w = powf(var[i], -q);

        de += mean[i] * w;
        nu += w;
      }

      double val = nu > EPS ? de / nu : 0.0;
      it.out[ch] = val;
    }

    /* No changes for alpha channel. */
    it.out[3] = image->get_value(x, y, 3);
  }
}

}  // namespace blender::compositor
