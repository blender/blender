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

#include "COM_ConvertOperation.h"

#include "BLI_color.hh"

#include "IMB_colormanagement.h"

namespace blender::compositor {

ConvertBaseOperation::ConvertBaseOperation()
{
  m_inputOperation = nullptr;
  this->flags.can_be_constant = true;
}

void ConvertBaseOperation::initExecution()
{
  m_inputOperation = this->getInputSocketReader(0);
}

void ConvertBaseOperation::deinitExecution()
{
  m_inputOperation = nullptr;
}

void ConvertBaseOperation::hash_output_params()
{
}

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
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);
}

void ConvertValueToColorOperation::executePixelSampled(float output[4],
                                                       float x,
                                                       float y,
                                                       PixelSampler sampler)
{
  float value;
  m_inputOperation->readSampled(&value, x, y, sampler);
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
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Value);
}

void ConvertColorToValueOperation::executePixelSampled(float output[4],
                                                       float x,
                                                       float y,
                                                       PixelSampler sampler)
{
  float inputColor[4];
  m_inputOperation->readSampled(inputColor, x, y, sampler);
  output[0] = (inputColor[0] + inputColor[1] + inputColor[2]) / 3.0f;
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
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Value);
}

void ConvertColorToBWOperation::executePixelSampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler sampler)
{
  float inputColor[4];
  m_inputOperation->readSampled(inputColor, x, y, sampler);
  output[0] = IMB_colormanagement_get_luminance(inputColor);
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
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Vector);
}

void ConvertColorToVectorOperation::executePixelSampled(float output[4],
                                                        float x,
                                                        float y,
                                                        PixelSampler sampler)
{
  float color[4];
  m_inputOperation->readSampled(color, x, y, sampler);
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
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Vector);
}

void ConvertValueToVectorOperation::executePixelSampled(float output[4],
                                                        float x,
                                                        float y,
                                                        PixelSampler sampler)
{
  float value;
  m_inputOperation->readSampled(&value, x, y, sampler);
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
  this->addInputSocket(DataType::Vector);
  this->addOutputSocket(DataType::Color);
}

void ConvertVectorToColorOperation::executePixelSampled(float output[4],
                                                        float x,
                                                        float y,
                                                        PixelSampler sampler)
{
  m_inputOperation->readSampled(output, x, y, sampler);
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
  this->addInputSocket(DataType::Vector);
  this->addOutputSocket(DataType::Value);
}

void ConvertVectorToValueOperation::executePixelSampled(float output[4],
                                                        float x,
                                                        float y,
                                                        PixelSampler sampler)
{
  float input[4];
  m_inputOperation->readSampled(input, x, y, sampler);
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
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
}

void ConvertRGBToYCCOperation::setMode(int mode)
{
  switch (mode) {
    case 0:
      m_mode = BLI_YCC_ITU_BT601;
      break;
    case 2:
      m_mode = BLI_YCC_JFIF_0_255;
      break;
    case 1:
    default:
      m_mode = BLI_YCC_ITU_BT709;
      break;
  }
}

void ConvertRGBToYCCOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float inputColor[4];
  float color[3];

  m_inputOperation->readSampled(inputColor, x, y, sampler);
  rgb_to_ycc(
      inputColor[0], inputColor[1], inputColor[2], &color[0], &color[1], &color[2], m_mode);

  /* divided by 255 to normalize for viewing in */
  /* R,G,B --> Y,Cb,Cr */
  mul_v3_v3fl(output, color, 1.0f / 255.0f);
  output[3] = inputColor[3];
}

void ConvertRGBToYCCOperation::hash_output_params()
{
  ConvertBaseOperation::hash_output_params();
  hash_param(m_mode);
}

void ConvertRGBToYCCOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    rgb_to_ycc(in[0], in[1], in[2], &it.out[0], &it.out[1], &it.out[2], m_mode);

    /* Normalize for viewing (#rgb_to_ycc returns 0-255 values). */
    mul_v3_fl(it.out, 1.0f / 255.0f);
    it.out[3] = in[3];
  }
}

/* ******** YCC to RGB ******** */

ConvertYCCToRGBOperation::ConvertYCCToRGBOperation() : ConvertBaseOperation()
{
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
}

void ConvertYCCToRGBOperation::setMode(int mode)
{
  switch (mode) {
    case 0:
      m_mode = BLI_YCC_ITU_BT601;
      break;
    case 2:
      m_mode = BLI_YCC_JFIF_0_255;
      break;
    case 1:
    default:
      m_mode = BLI_YCC_ITU_BT709;
      break;
  }
}

void ConvertYCCToRGBOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float inputColor[4];
  m_inputOperation->readSampled(inputColor, x, y, sampler);

  /* need to un-normalize the data */
  /* R,G,B --> Y,Cb,Cr */
  mul_v3_fl(inputColor, 255.0f);

  ycc_to_rgb(inputColor[0],
             inputColor[1],
             inputColor[2],
             &output[0],
             &output[1],
             &output[2],
             m_mode);
  output[3] = inputColor[3];
}

void ConvertYCCToRGBOperation::hash_output_params()
{
  ConvertBaseOperation::hash_output_params();
  hash_param(m_mode);
}

void ConvertYCCToRGBOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float *in = it.in(0);
    /* Multiply by 255 to un-normalize (#ycc_to_rgb needs input values in 0-255 range). */
    ycc_to_rgb(in[0] * 255.0f,
               in[1] * 255.0f,
               in[2] * 255.0f,
               &it.out[0],
               &it.out[1],
               &it.out[2],
               m_mode);
    it.out[3] = in[3];
  }
}

/* ******** RGB to YUV ******** */

ConvertRGBToYUVOperation::ConvertRGBToYUVOperation() : ConvertBaseOperation()
{
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
}

void ConvertRGBToYUVOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float inputColor[4];
  m_inputOperation->readSampled(inputColor, x, y, sampler);
  rgb_to_yuv(inputColor[0],
             inputColor[1],
             inputColor[2],
             &output[0],
             &output[1],
             &output[2],
             BLI_YUV_ITU_BT709);
  output[3] = inputColor[3];
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
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
}

void ConvertYUVToRGBOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float inputColor[4];
  m_inputOperation->readSampled(inputColor, x, y, sampler);
  yuv_to_rgb(inputColor[0],
             inputColor[1],
             inputColor[2],
             &output[0],
             &output[1],
             &output[2],
             BLI_YUV_ITU_BT709);
  output[3] = inputColor[3];
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
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
}

void ConvertRGBToHSVOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float inputColor[4];
  m_inputOperation->readSampled(inputColor, x, y, sampler);
  rgb_to_hsv_v(inputColor, output);
  output[3] = inputColor[3];
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
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
}

void ConvertHSVToRGBOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float inputColor[4];
  m_inputOperation->readSampled(inputColor, x, y, sampler);
  hsv_to_rgb_v(inputColor, output);
  output[0] = max_ff(output[0], 0.0f);
  output[1] = max_ff(output[1], 0.0f);
  output[2] = max_ff(output[2], 0.0f);
  output[3] = inputColor[3];
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

/* ******** Premul to Straight ******** */

ConvertPremulToStraightOperation::ConvertPremulToStraightOperation() : ConvertBaseOperation()
{
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
}

void ConvertPremulToStraightOperation::executePixelSampled(float output[4],
                                                           float x,
                                                           float y,
                                                           PixelSampler sampler)
{
  ColorSceneLinear4f<eAlpha::Premultiplied> input;
  m_inputOperation->readSampled(input, x, y, sampler);
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
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
}

void ConvertStraightToPremulOperation::executePixelSampled(float output[4],
                                                           float x,
                                                           float y,
                                                           PixelSampler sampler)
{
  ColorSceneLinear4f<eAlpha::Straight> input;
  m_inputOperation->readSampled(input, x, y, sampler);
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
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Value);
  m_inputOperation = nullptr;
}
void SeparateChannelOperation::initExecution()
{
  m_inputOperation = this->getInputSocketReader(0);
}

void SeparateChannelOperation::deinitExecution()
{
  m_inputOperation = nullptr;
}

void SeparateChannelOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float input[4];
  m_inputOperation->readSampled(input, x, y, sampler);
  output[0] = input[m_channel];
}

void SeparateChannelOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    it.out[0] = it.in(0)[m_channel];
  }
}

/* ******** Combine Channels ******** */

CombineChannelsOperation::CombineChannelsOperation()
{
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);
  this->set_canvas_input_index(0);
  m_inputChannel1Operation = nullptr;
  m_inputChannel2Operation = nullptr;
  m_inputChannel3Operation = nullptr;
  m_inputChannel4Operation = nullptr;
}

void CombineChannelsOperation::initExecution()
{
  m_inputChannel1Operation = this->getInputSocketReader(0);
  m_inputChannel2Operation = this->getInputSocketReader(1);
  m_inputChannel3Operation = this->getInputSocketReader(2);
  m_inputChannel4Operation = this->getInputSocketReader(3);
}

void CombineChannelsOperation::deinitExecution()
{
  m_inputChannel1Operation = nullptr;
  m_inputChannel2Operation = nullptr;
  m_inputChannel3Operation = nullptr;
  m_inputChannel4Operation = nullptr;
}

void CombineChannelsOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float input[4];
  if (m_inputChannel1Operation) {
    m_inputChannel1Operation->readSampled(input, x, y, sampler);
    output[0] = input[0];
  }
  if (m_inputChannel2Operation) {
    m_inputChannel2Operation->readSampled(input, x, y, sampler);
    output[1] = input[0];
  }
  if (m_inputChannel3Operation) {
    m_inputChannel3Operation->readSampled(input, x, y, sampler);
    output[2] = input[0];
  }
  if (m_inputChannel4Operation) {
    m_inputChannel4Operation->readSampled(input, x, y, sampler);
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
