/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ConvertOperation.h"

#include "BLI_color.hh"

#include "IMB_colormanagement.hh"

namespace blender::compositor {

ConvertBaseOperation::ConvertBaseOperation()
{
  flags_.can_be_constant = true;
}

void ConvertBaseOperation::hash_output_params() {}

void ConvertBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                        const rcti &area,
                                                        Span<MemoryBuffer *> inputs)
{
  BuffersIterator<float> it = output->iterate_with(inputs, area);
  update_memory_buffer_partial(it);
}

/* ******** Value to Color ******** */

ConvertValueToColorOperation::ConvertValueToColorOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
}

void ConvertValueToColorOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    it.out[0] = it.out[1] = it.out[2] = *it.in(0);
    it.out[3] = 1.0f;
  }
}

/* ******** Color to Value ******** */

ConvertColorToValueOperation::ConvertColorToValueOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Value);
}

void ConvertColorToValueOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    it.out[0] = (in[0] + in[1] + in[2]) / 3.0f;
  }
}

/* ******** Color to BW ******** */

ConvertColorToBWOperation::ConvertColorToBWOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Value);
}

void ConvertColorToBWOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    it.out[0] = IMB_colormanagement_get_luminance(it.in(0));
  }
}

/* ******** Color to Vector ******** */

ConvertColorToVectorOperation::ConvertColorToVectorOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Vector);
}

void ConvertColorToVectorOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    copy_v3_v3(it.out, it.in(0));
  }
}

/* ******** Value to Vector ******** */

ConvertValueToVectorOperation::ConvertValueToVectorOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Vector);
}

void ConvertValueToVectorOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    it.out[0] = it.out[1] = it.out[2] = *it.in(0);
  }
}

/* ******** Vector to Color ******** */

ConvertVectorToColorOperation::ConvertVectorToColorOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Vector);
  this->add_output_socket(DataType::Color);
}

void ConvertVectorToColorOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    copy_v3_v3(it.out, it.in(0));
    it.out[3] = 1.0f;
  }
}

/* ******** Vector to Value ******** */

ConvertVectorToValueOperation::ConvertVectorToValueOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Vector);
  this->add_output_socket(DataType::Value);
}

void ConvertVectorToValueOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    it.out[0] = (in[0] + in[1] + in[2]) / 3.0f;
  }
}

/* ******** RGB to YCC ******** */

ConvertRGBToYCCOperation::ConvertRGBToYCCOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertRGBToYCCOperation::set_mode(int mode)
{
  switch (mode) {
    case 0:
      mode_ = BLI_YCC_ITU_BT601;
      break;
    case 2:
      mode_ = BLI_YCC_JFIF_0_255;
      break;
    case 1:
    default:
      mode_ = BLI_YCC_ITU_BT709;
      break;
  }
}

void ConvertRGBToYCCOperation::hash_output_params()
{
  ConvertBaseOperation::hash_output_params();
  hash_param(mode_);
}

void ConvertRGBToYCCOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    rgb_to_ycc(in[0], in[1], in[2], &it.out[0], &it.out[1], &it.out[2], mode_);

    /* Normalize for viewing (#rgb_to_ycc returns 0-255 values). */
    mul_v3_fl(it.out, 1.0f / 255.0f);
    it.out[3] = in[3];
  }
}

/* ******** YCC to RGB ******** */

ConvertYCCToRGBOperation::ConvertYCCToRGBOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertYCCToRGBOperation::set_mode(int mode)
{
  switch (mode) {
    case 0:
      mode_ = BLI_YCC_ITU_BT601;
      break;
    case 2:
      mode_ = BLI_YCC_JFIF_0_255;
      break;
    case 1:
    default:
      mode_ = BLI_YCC_ITU_BT709;
      break;
  }
}

void ConvertYCCToRGBOperation::hash_output_params()
{
  ConvertBaseOperation::hash_output_params();
  hash_param(mode_);
}

void ConvertYCCToRGBOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    /* Multiply by 255 to un-normalize (#ycc_to_rgb needs input values in 0-255 range). */
    ycc_to_rgb(
        in[0] * 255.0f, in[1] * 255.0f, in[2] * 255.0f, &it.out[0], &it.out[1], &it.out[2], mode_);
    it.out[3] = in[3];
  }
}

/* ******** RGB to YUV ******** */

ConvertRGBToYUVOperation::ConvertRGBToYUVOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertRGBToYUVOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    rgb_to_yuv(in[0], in[1], in[2], &it.out[0], &it.out[1], &it.out[2], BLI_YUV_ITU_BT709);
    it.out[3] = in[3];
  }
}

/* ******** YUV to RGB ******** */

ConvertYUVToRGBOperation::ConvertYUVToRGBOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertYUVToRGBOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    yuv_to_rgb(in[0], in[1], in[2], &it.out[0], &it.out[1], &it.out[2], BLI_YUV_ITU_BT709);
    it.out[3] = in[3];
  }
}

/* ******** RGB to HSV ******** */

ConvertRGBToHSVOperation::ConvertRGBToHSVOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertRGBToHSVOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    rgb_to_hsv_v(in, it.out);
    it.out[3] = in[3];
  }
}

/* ******** HSV to RGB ******** */

ConvertHSVToRGBOperation::ConvertHSVToRGBOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertHSVToRGBOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    hsv_to_rgb_v(in, it.out);
    it.out[0] = max_ff(it.out[0], 0.0f);
    it.out[1] = max_ff(it.out[1], 0.0f);
    it.out[2] = max_ff(it.out[2], 0.0f);
    it.out[3] = in[3];
  }
}

/* ******** RGB to HSL ******** */

ConvertRGBToHSLOperation::ConvertRGBToHSLOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertRGBToHSLOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    rgb_to_hsl_v(in, it.out);
    it.out[3] = in[3];
  }
}

/* ******** HSL to RGB ******** */

ConvertHSLToRGBOperation::ConvertHSLToRGBOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertHSLToRGBOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    hsl_to_rgb_v(in, it.out);
    it.out[0] = max_ff(it.out[0], 0.0f);
    it.out[1] = max_ff(it.out[1], 0.0f);
    it.out[2] = max_ff(it.out[2], 0.0f);
    it.out[3] = in[3];
  }
}

/* ******** Pre-multiplied to Straight ******** */

ConvertPremulToStraightOperation::ConvertPremulToStraightOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertPremulToStraightOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    copy_v4_v4(it.out, ColorSceneLinear4f<eAlpha::Premultiplied>(it.in(0)).unpremultiply_alpha());
  }
}

/* ******** Straight to Pre-multiplied ******** */

ConvertStraightToPremulOperation::ConvertStraightToPremulOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertStraightToPremulOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    copy_v4_v4(it.out, ColorSceneLinear4f<eAlpha::Straight>(it.in(0)).premultiply_alpha());
  }
}

/* ******** Separate Channels ******** */

SeparateChannelOperation::SeparateChannelOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Value);
  flags_.can_be_constant = true;
}

void SeparateChannelOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    it.out[0] = it.in(0)[channel_];
  }
}

/* ******** Combine Channels ******** */

CombineChannelsOperation::CombineChannelsOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);

  flags_.can_be_constant = true;
}

void CombineChannelsOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    it.out[0] = *it.in(0);
    it.out[1] = *it.in(1);
    it.out[2] = *it.in(2);
    it.out[3] = *it.in(3);
  }
}

}  // namespace blender::compositor
