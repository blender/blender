/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorCurveOperation.h"

#include "BKE_colortools.h"

namespace blender::compositor {

ColorCurveOperation::ColorCurveOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);

  input_fac_program_ = nullptr;
  input_image_program_ = nullptr;
  input_black_program_ = nullptr;
  input_white_program_ = nullptr;

  this->set_canvas_input_index(1);
}
void ColorCurveOperation::init_execution()
{
  CurveBaseOperation::init_execution();
  input_fac_program_ = this->get_input_socket_reader(0);
  input_image_program_ = this->get_input_socket_reader(1);
  input_black_program_ = this->get_input_socket_reader(2);
  input_white_program_ = this->get_input_socket_reader(3);

  BKE_curvemapping_premultiply(curve_mapping_, false);
}

void ColorCurveOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  CurveMapping *cumap = curve_mapping_;

  float fac[4];
  float image[4];

  /* local versions of cumap->black, cumap->white, cumap->bwmul */
  float black[4];
  float white[4];
  float bwmul[3];

  input_black_program_->read_sampled(black, x, y, sampler);
  input_white_program_->read_sampled(white, x, y, sampler);

  /* get our own local bwmul value,
   * since we can't be threadsafe and use cumap->bwmul & friends */
  BKE_curvemapping_set_black_white_ex(black, white, bwmul);

  input_fac_program_->read_sampled(fac, x, y, sampler);
  input_image_program_->read_sampled(image, x, y, sampler);

  if (*fac >= 1.0f) {
    BKE_curvemapping_evaluate_premulRGBF_ex(cumap, output, image, black, bwmul);
  }
  else if (*fac <= 0.0f) {
    copy_v3_v3(output, image);
  }
  else {
    float col[4];
    BKE_curvemapping_evaluate_premulRGBF_ex(cumap, col, image, black, bwmul);
    interp_v3_v3v3(output, image, col, *fac);
  }
  output[3] = image[3];
}

void ColorCurveOperation::deinit_execution()
{
  CurveBaseOperation::deinit_execution();
  input_fac_program_ = nullptr;
  input_image_program_ = nullptr;
  input_black_program_ = nullptr;
  input_white_program_ = nullptr;
}

void ColorCurveOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  CurveMapping *cumap = curve_mapping_;
  float bwmul[3];
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    /* Local versions of `cumap->black` and `cumap->white`. */
    const float *black = it.in(2);
    const float *white = it.in(3);
    /* Get a local `bwmul` value, it's not threadsafe using `cumap->bwmul` and others. */
    BKE_curvemapping_set_black_white_ex(black, white, bwmul);

    const float fac = *it.in(0);
    const float *image = it.in(1);
    if (fac >= 1.0f) {
      BKE_curvemapping_evaluate_premulRGBF_ex(cumap, it.out, image, black, bwmul);
    }
    else if (fac <= 0.0f) {
      copy_v3_v3(it.out, image);
    }
    else {
      float col[4];
      BKE_curvemapping_evaluate_premulRGBF_ex(cumap, col, image, black, bwmul);
      interp_v3_v3v3(it.out, image, col, fac);
    }
    it.out[3] = image[3];
  }
}

/* Constant level curve mapping. */

ConstantLevelColorCurveOperation::ConstantLevelColorCurveOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);

  input_fac_program_ = nullptr;
  input_image_program_ = nullptr;

  this->set_canvas_input_index(1);
}
void ConstantLevelColorCurveOperation::init_execution()
{
  CurveBaseOperation::init_execution();
  input_fac_program_ = this->get_input_socket_reader(0);
  input_image_program_ = this->get_input_socket_reader(1);

  BKE_curvemapping_premultiply(curve_mapping_, false);

  BKE_curvemapping_set_black_white(curve_mapping_, black_, white_);
}

void ConstantLevelColorCurveOperation::execute_pixel_sampled(float output[4],
                                                             float x,
                                                             float y,
                                                             PixelSampler sampler)
{
  float fac[4];
  float image[4];

  input_fac_program_->read_sampled(fac, x, y, sampler);
  input_image_program_->read_sampled(image, x, y, sampler);

  if (*fac >= 1.0f) {
    BKE_curvemapping_evaluate_premulRGBF(curve_mapping_, output, image);
  }
  else if (*fac <= 0.0f) {
    copy_v3_v3(output, image);
  }
  else {
    float col[4];
    BKE_curvemapping_evaluate_premulRGBF(curve_mapping_, col, image);
    interp_v3_v3v3(output, image, col, *fac);
  }
  output[3] = image[3];
}

void ConstantLevelColorCurveOperation::deinit_execution()
{
  CurveBaseOperation::deinit_execution();
  input_fac_program_ = nullptr;
  input_image_program_ = nullptr;
}

void ConstantLevelColorCurveOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                    const rcti &area,
                                                                    Span<MemoryBuffer *> inputs)
{
  CurveMapping *cumap = curve_mapping_;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float fac = *it.in(0);
    const float *image = it.in(1);
    if (fac >= 1.0f) {
      BKE_curvemapping_evaluate_premulRGBF(cumap, it.out, image);
    }
    else if (fac <= 0.0f) {
      copy_v3_v3(it.out, image);
    }
    else {
      float col[4];
      BKE_curvemapping_evaluate_premulRGBF(cumap, col, image);
      interp_v3_v3v3(it.out, image, col, fac);
    }
    it.out[3] = image[3];
  }
}

}  // namespace blender::compositor
