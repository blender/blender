/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#include "COM_ConvertOperation.h"

#include "BLI_color.hh"

#include "IMB_colormanagement.h"

namespace blender::compositor {

ConvertBaseOperation::ConvertBaseOperation()
{
  input_operation_ = nullptr;
  flags_.can_be_constant = true;
}

void ConvertBaseOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
}

void ConvertBaseOperation::deinit_execution()
{
  input_operation_ = nullptr;
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

void ConvertValueToColorOperation::execute_pixel_sampled(float output[4],
                                                         float x,
                                                         float y,
                                                         PixelSampler sampler)
{
  float value;
  input_operation_->read_sampled(&value, x, y, sampler);
  output[0] = output[1] = output[2] = value;
  output[3] = 1.0f;
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

void ConvertColorToValueOperation::execute_pixel_sampled(float output[4],
                                                         float x,
                                                         float y,
                                                         PixelSampler sampler)
{
  float input_color[4];
  input_operation_->read_sampled(input_color, x, y, sampler);
  output[0] = (input_color[0] + input_color[1] + input_color[2]) / 3.0f;
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

void ConvertColorToBWOperation::execute_pixel_sampled(float output[4],
                                                      float x,
                                                      float y,
                                                      PixelSampler sampler)
{
  float input_color[4];
  input_operation_->read_sampled(input_color, x, y, sampler);
  output[0] = IMB_colormanagement_get_luminance(input_color);
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

void ConvertColorToVectorOperation::execute_pixel_sampled(float output[4],
                                                          float x,
                                                          float y,
                                                          PixelSampler sampler)
{
  float color[4];
  input_operation_->read_sampled(color, x, y, sampler);
  copy_v3_v3(output, color);
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

void ConvertValueToVectorOperation::execute_pixel_sampled(float output[4],
                                                          float x,
                                                          float y,
                                                          PixelSampler sampler)
{
  float value;
  input_operation_->read_sampled(&value, x, y, sampler);
  output[0] = output[1] = output[2] = value;
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

void ConvertVectorToColorOperation::execute_pixel_sampled(float output[4],
                                                          float x,
                                                          float y,
                                                          PixelSampler sampler)
{
  input_operation_->read_sampled(output, x, y, sampler);
  output[3] = 1.0f;
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

void ConvertVectorToValueOperation::execute_pixel_sampled(float output[4],
                                                          float x,
                                                          float y,
                                                          PixelSampler sampler)
{
  float input[4];
  input_operation_->read_sampled(input, x, y, sampler);
  output[0] = (input[0] + input[1] + input[2]) / 3.0f;
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

void ConvertRGBToYCCOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_color[4];
  float color[3];

  input_operation_->read_sampled(input_color, x, y, sampler);
  rgb_to_ycc(
      input_color[0], input_color[1], input_color[2], &color[0], &color[1], &color[2], mode_);

  /* divided by 255 to normalize for viewing in */
  /* R,G,B --> Y,Cb,Cr */
  mul_v3_v3fl(output, color, 1.0f / 255.0f);
  output[3] = input_color[3];
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

void ConvertYCCToRGBOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_color[4];
  input_operation_->read_sampled(input_color, x, y, sampler);

  /* need to un-normalize the data */
  /* R,G,B --> Y,Cb,Cr */
  mul_v3_fl(input_color, 255.0f);

  ycc_to_rgb(
      input_color[0], input_color[1], input_color[2], &output[0], &output[1], &output[2], mode_);
  output[3] = input_color[3];
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

void ConvertRGBToYUVOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_color[4];
  input_operation_->read_sampled(input_color, x, y, sampler);
  rgb_to_yuv(input_color[0],
             input_color[1],
             input_color[2],
             &output[0],
             &output[1],
             &output[2],
             BLI_YUV_ITU_BT709);
  output[3] = input_color[3];
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

void ConvertYUVToRGBOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_color[4];
  input_operation_->read_sampled(input_color, x, y, sampler);
  yuv_to_rgb(input_color[0],
             input_color[1],
             input_color[2],
             &output[0],
             &output[1],
             &output[2],
             BLI_YUV_ITU_BT709);
  output[3] = input_color[3];
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

void ConvertRGBToHSVOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_color[4];
  input_operation_->read_sampled(input_color, x, y, sampler);
  rgb_to_hsv_v(input_color, output);
  output[3] = input_color[3];
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

void ConvertHSVToRGBOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_color[4];
  input_operation_->read_sampled(input_color, x, y, sampler);
  hsv_to_rgb_v(input_color, output);
  output[0] = max_ff(output[0], 0.0f);
  output[1] = max_ff(output[1], 0.0f);
  output[2] = max_ff(output[2], 0.0f);
  output[3] = input_color[3];
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

void ConvertRGBToHSLOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_color[4];
  input_operation_->read_sampled(input_color, x, y, sampler);
  rgb_to_hsl_v(input_color, output);
  output[3] = input_color[3];
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

void ConvertHSLToRGBOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_color[4];
  input_operation_->read_sampled(input_color, x, y, sampler);
  hsl_to_rgb_v(input_color, output);
  output[0] = max_ff(output[0], 0.0f);
  output[1] = max_ff(output[1], 0.0f);
  output[2] = max_ff(output[2], 0.0f);
  output[3] = input_color[3];
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

/* ******** Premul to Straight ******** */

ConvertPremulToStraightOperation::ConvertPremulToStraightOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertPremulToStraightOperation::execute_pixel_sampled(float output[4],
                                                             float x,
                                                             float y,
                                                             PixelSampler sampler)
{
  ColorSceneLinear4f<eAlpha::Premultiplied> input;
  input_operation_->read_sampled(input, x, y, sampler);
  ColorSceneLinear4f<eAlpha::Straight> converted = input.unpremultiply_alpha();
  copy_v4_v4(output, converted);
}

void ConvertPremulToStraightOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    copy_v4_v4(it.out, ColorSceneLinear4f<eAlpha::Premultiplied>(it.in(0)).unpremultiply_alpha());
  }
}

/* ******** Straight to Premul ******** */

ConvertStraightToPremulOperation::ConvertStraightToPremulOperation() : ConvertBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ConvertStraightToPremulOperation::execute_pixel_sampled(float output[4],
                                                             float x,
                                                             float y,
                                                             PixelSampler sampler)
{
  ColorSceneLinear4f<eAlpha::Straight> input;
  input_operation_->read_sampled(input, x, y, sampler);
  ColorSceneLinear4f<eAlpha::Premultiplied> converted = input.premultiply_alpha();
  copy_v4_v4(output, converted);
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
  input_operation_ = nullptr;
}
void SeparateChannelOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
}

void SeparateChannelOperation::deinit_execution()
{
  input_operation_ = nullptr;
}

void SeparateChannelOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input[4];
  input_operation_->read_sampled(input, x, y, sampler);
  output[0] = input[channel_];
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
  input_channel1_operation_ = nullptr;
  input_channel2_operation_ = nullptr;
  input_channel3_operation_ = nullptr;
  input_channel4_operation_ = nullptr;
}

void CombineChannelsOperation::init_execution()
{
  input_channel1_operation_ = this->get_input_socket_reader(0);
  input_channel2_operation_ = this->get_input_socket_reader(1);
  input_channel3_operation_ = this->get_input_socket_reader(2);
  input_channel4_operation_ = this->get_input_socket_reader(3);
}

void CombineChannelsOperation::deinit_execution()
{
  input_channel1_operation_ = nullptr;
  input_channel2_operation_ = nullptr;
  input_channel3_operation_ = nullptr;
  input_channel4_operation_ = nullptr;
}

void CombineChannelsOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input[4];
  if (input_channel1_operation_) {
    input_channel1_operation_->read_sampled(input, x, y, sampler);
    output[0] = input[0];
  }
  if (input_channel2_operation_) {
    input_channel2_operation_->read_sampled(input, x, y, sampler);
    output[1] = input[0];
  }
  if (input_channel3_operation_) {
    input_channel3_operation_->read_sampled(input, x, y, sampler);
    output[2] = input[0];
  }
  if (input_channel4_operation_) {
    input_channel4_operation_->read_sampled(input, x, y, sampler);
    output[3] = input[0];
  }
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
