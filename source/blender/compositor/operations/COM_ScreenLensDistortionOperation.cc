/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ScreenLensDistortionOperation.h"

#include "COM_ConstantOperation.h"

#include "BLI_rand.h"
#include "BLI_time.h"

namespace blender::compositor {

ScreenLensDistortionOperation::ScreenLensDistortionOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  flags_.can_be_constant = true;
  distortion_ = 0.0f;
  dispersion_ = 0.0f;
  distortion_const_ = false;
  dispersion_const_ = false;
  variables_ready_ = false;
}

void ScreenLensDistortionOperation::set_distortion(float distortion)
{
  distortion_ = distortion;
  distortion_const_ = true;
}

void ScreenLensDistortionOperation::set_dispersion(float dispersion)
{
  dispersion_ = dispersion;
  dispersion_const_ = true;
}

void ScreenLensDistortionOperation::init_data()
{
  cx_ = 0.5f * float(get_width());
  cy_ = 0.5f * float(get_height());

  NodeOperation *distortion_op = get_input_operation(1);
  NodeOperation *dispersion_op = get_input_operation(2);
  if (!distortion_const_ && distortion_op->get_flags().is_constant_operation) {
    distortion_ = static_cast<ConstantOperation *>(distortion_op)->get_constant_elem()[0];
  }
  if (!dispersion_const_ && distortion_op->get_flags().is_constant_operation) {
    dispersion_ = static_cast<ConstantOperation *>(dispersion_op)->get_constant_elem()[0];
  }
  update_variables(distortion_, dispersion_);
}

void ScreenLensDistortionOperation::init_execution()
{
  SocketReader *input_reader = this->get_input_socket_reader(0);

  uint rng_seed = uint(BLI_time_now_seconds_i() & UINT_MAX);
  rng_seed ^= uint(POINTER_AS_INT(input_reader));
  rng_ = BLI_rng_new(rng_seed);
}

void ScreenLensDistortionOperation::get_uv(const float xy[2], float uv[2]) const
{
  uv[0] = sc_ * ((xy[0] + 0.5f) - cx_) / cx_;
  uv[1] = sc_ * ((xy[1] + 0.5f) - cy_) / cy_;
}

void ScreenLensDistortionOperation::distort_uv(const float uv[2], float t, float xy[2]) const
{
  float d = 1.0f / (1.0f + sqrtf(t));
  xy[0] = (uv[0] * d + 0.5f) * get_width() - 0.5f;
  xy[1] = (uv[1] * d + 0.5f) * get_height() - 0.5f;
}

bool ScreenLensDistortionOperation::get_delta(float r_sq,
                                              float k4,
                                              const float uv[2],
                                              float delta[2]) const
{
  float t = 1.0f - k4 * r_sq;
  if (t >= 0.0f) {
    distort_uv(uv, t, delta);
    return true;
  }

  return false;
}

void ScreenLensDistortionOperation::accumulate(const MemoryBuffer *buffer,
                                               int a,
                                               int b,
                                               float r_sq,
                                               const float uv[2],
                                               const float delta[3][2],
                                               float sum[4],
                                               int count[3]) const
{
  float color[4];

  float dsf = len_v2v2(delta[a], delta[b]) + 1.0f;
  int ds = jitter_ ? (dsf < 4.0f ? 2 : int(sqrtf(dsf))) : int(dsf);
  float sd = 1.0f / float(ds);

  float k4 = k4_[a];
  float dk4 = dk4_[a];

  for (float z = 0; z < ds; z++) {
    float tz = (z + (jitter_ ? BLI_rng_get_float(rng_) : 0.5f)) * sd;
    float t = 1.0f - (k4 + tz * dk4) * r_sq;

    float xy[2];
    distort_uv(uv, t, xy);
    buffer->read_elem_bilinear(xy[0], xy[1], color);

    sum[a] += (1.0f - tz) * color[a];
    sum[b] += (tz)*color[b];
    count[a]++;
    count[b]++;
  }
}

void ScreenLensDistortionOperation::deinit_execution()
{
  BLI_rng_free(rng_);
}

void ScreenLensDistortionOperation::determineUV(float result[6], float x, float y) const
{
  const float xy[2] = {x, y};
  float uv[2];
  get_uv(xy, uv);
  float uv_dot = len_squared_v2(uv);

  copy_v2_v2(result + 0, xy);
  copy_v2_v2(result + 2, xy);
  copy_v2_v2(result + 4, xy);
  get_delta(uv_dot, k4_[0], uv, result + 0);
  get_delta(uv_dot, k4_[1], uv, result + 2);
  get_delta(uv_dot, k4_[2], uv, result + 4);
}

void ScreenLensDistortionOperation::update_variables(float distortion, float dispersion)
{
  k_[1] = max_ff(min_ff(distortion, 1.0f), -0.999f);
  /* Smaller dispersion range for somewhat more control. */
  float d = 0.25f * max_ff(min_ff(dispersion, 1.0f), 0.0f);
  k_[0] = max_ff(min_ff((k_[1] + d), 1.0f), -0.999f);
  k_[2] = max_ff(min_ff((k_[1] - d), 1.0f), -0.999f);
  maxk_ = max_fff(k_[0], k_[1], k_[2]);
  sc_ = (fit_ && (maxk_ > 0.0f)) ? (1.0f / (1.0f + 2.0f * maxk_)) : (1.0f / (1.0f + maxk_));
  dk4_[0] = 4.0f * (k_[1] - k_[0]);
  dk4_[1] = 4.0f * (k_[2] - k_[1]);
  dk4_[2] = 0.0f; /* unused */

  mul_v3_v3fl(k4_, k_, 4.0f);
}

void ScreenLensDistortionOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  set_determined_canvas_modifier([=](rcti &canvas) {
    /* Ensure screen space. */
    BLI_rcti_translate(&canvas, -canvas.xmin, -canvas.ymin);
  });

  NodeOperation::determine_canvas(preferred_area, r_area);
}

void ScreenLensDistortionOperation::get_area_of_interest(const int input_idx,
                                                         const rcti & /*output_area*/,
                                                         rcti &r_input_area)
{
  if (input_idx != 0) {
    /* Dispersion and distortion inputs are used as constants only. */
    r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
  }

  /* XXX the original method of estimating the area-of-interest does not work
   * it assumes a linear increase/decrease of mapped coordinates, which does not
   * yield correct results for the area and leaves uninitialized buffer areas.
   * So now just use the full image area, which may not be as efficient but works at least ...
   */
  NodeOperation *image = get_input_operation(0);
  r_input_area = image->get_canvas();
}

void ScreenLensDistortionOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                 const rcti &area,
                                                                 Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_image = inputs[0];
  if (input_image->is_a_single_elem()) {
    copy_v4_v4(output->get_elem(0, 0), input_image->get_elem(0, 0));
    return;
  }
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float xy[2] = {float(it.x), float(it.y)};
    float uv[2];
    get_uv(xy, uv);
    const float uv_dot = len_squared_v2(uv);

    float delta[3][2];
    const bool valid_r = get_delta(uv_dot, k4_[0], uv, delta[0]);
    const bool valid_g = get_delta(uv_dot, k4_[1], uv, delta[1]);
    const bool valid_b = get_delta(uv_dot, k4_[2], uv, delta[2]);
    if (!(valid_r && valid_g && valid_b)) {
      zero_v4(it.out);
      continue;
    }

    int count[3] = {0, 0, 0};
    float sum[4] = {0, 0, 0, 0};
    accumulate(input_image, 0, 1, uv_dot, uv, delta, sum, count);
    accumulate(input_image, 1, 2, uv_dot, uv, delta, sum, count);

    if (count[0]) {
      it.out[0] = 2.0f * sum[0] / float(count[0]);
    }
    if (count[1]) {
      it.out[1] = 2.0f * sum[1] / float(count[1]);
    }
    if (count[2]) {
      it.out[2] = 2.0f * sum[2] / float(count[2]);
    }

    /* Set alpha. */
    it.out[3] = 1.0f;
  }
}

}  // namespace blender::compositor
