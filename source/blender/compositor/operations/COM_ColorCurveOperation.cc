/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorCurveOperation.h"

#include "BKE_colortools.hh"

namespace blender::compositor {

ColorCurveOperation::ColorCurveOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);

  this->set_canvas_input_index(1);
}
void ColorCurveOperation::init_execution()
{
  CurveBaseOperation::init_execution();

  BKE_curvemapping_premultiply(curve_mapping_, false);
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

  this->set_canvas_input_index(1);
}
void ConstantLevelColorCurveOperation::init_execution()
{
  CurveBaseOperation::init_execution();

  BKE_curvemapping_premultiply(curve_mapping_, false);

  BKE_curvemapping_set_black_white(curve_mapping_, black_, white_);
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
