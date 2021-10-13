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

#include "COM_DirectionalBlurOperation.h"
#include "COM_OpenCLDevice.h"

namespace blender::compositor {

DirectionalBlurOperation::DirectionalBlurOperation()
{
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
  flags.complex = true;
  flags.open_cl = true;
  m_inputProgram = nullptr;
}

void DirectionalBlurOperation::initExecution()
{
  m_inputProgram = getInputSocketReader(0);
  QualityStepHelper::initExecution(COM_QH_INCREASE);
  const float angle = m_data->angle;
  const float zoom = m_data->zoom;
  const float spin = m_data->spin;
  const float iterations = m_data->iter;
  const float distance = m_data->distance;
  const float center_x = m_data->center_x;
  const float center_y = m_data->center_y;
  const float width = getWidth();
  const float height = getHeight();

  const float a = angle;
  const float itsc = 1.0f / powf(2.0f, (float)iterations);
  float D;

  D = distance * sqrtf(width * width + height * height);
  m_center_x_pix = center_x * width;
  m_center_y_pix = center_y * height;

  m_tx = itsc * D * cosf(a);
  m_ty = -itsc * D * sinf(a);
  m_sc = itsc * zoom;
  m_rot = itsc * spin;
}

void DirectionalBlurOperation::executePixel(float output[4], int x, int y, void * /*data*/)
{
  const int iterations = pow(2.0f, m_data->iter);
  float col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float col2[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  m_inputProgram->readSampled(col2, x, y, PixelSampler::Bilinear);
  float ltx = m_tx;
  float lty = m_ty;
  float lsc = m_sc;
  float lrot = m_rot;
  /* blur the image */
  for (int i = 0; i < iterations; i++) {
    const float cs = cosf(lrot), ss = sinf(lrot);
    const float isc = 1.0f / (1.0f + lsc);

    const float v = isc * (y - m_center_y_pix) + lty;
    const float u = isc * (x - m_center_x_pix) + ltx;

    m_inputProgram->readSampled(col,
                                cs * u + ss * v + m_center_x_pix,
                                cs * v - ss * u + m_center_y_pix,
                                PixelSampler::Bilinear);

    add_v4_v4(col2, col);

    /* double transformations */
    ltx += m_tx;
    lty += m_ty;
    lrot += m_rot;
    lsc += m_sc;
  }

  mul_v4_v4fl(output, col2, 1.0f / (iterations + 1));
}

void DirectionalBlurOperation::executeOpenCL(OpenCLDevice *device,
                                             MemoryBuffer *outputMemoryBuffer,
                                             cl_mem clOutputBuffer,
                                             MemoryBuffer **inputMemoryBuffers,
                                             std::list<cl_mem> *clMemToCleanUp,
                                             std::list<cl_kernel> * /*clKernelsToCleanUp*/)
{
  cl_kernel directionalBlurKernel = device->COM_clCreateKernel("directionalBlurKernel", nullptr);

  cl_int iterations = pow(2.0f, m_data->iter);
  cl_float2 ltxy = {{m_tx, m_ty}};
  cl_float2 centerpix = {{m_center_x_pix, m_center_y_pix}};
  cl_float lsc = m_sc;
  cl_float lrot = m_rot;

  device->COM_clAttachMemoryBufferToKernelParameter(
      directionalBlurKernel, 0, -1, clMemToCleanUp, inputMemoryBuffers, m_inputProgram);
  device->COM_clAttachOutputMemoryBufferToKernelParameter(
      directionalBlurKernel, 1, clOutputBuffer);
  device->COM_clAttachMemoryBufferOffsetToKernelParameter(
      directionalBlurKernel, 2, outputMemoryBuffer);
  clSetKernelArg(directionalBlurKernel, 3, sizeof(cl_int), &iterations);
  clSetKernelArg(directionalBlurKernel, 4, sizeof(cl_float), &lsc);
  clSetKernelArg(directionalBlurKernel, 5, sizeof(cl_float), &lrot);
  clSetKernelArg(directionalBlurKernel, 6, sizeof(cl_float2), &ltxy);
  clSetKernelArg(directionalBlurKernel, 7, sizeof(cl_float2), &centerpix);

  device->COM_clEnqueueRange(directionalBlurKernel, outputMemoryBuffer, 8, this);
}

void DirectionalBlurOperation::deinitExecution()
{
  m_inputProgram = nullptr;
}

bool DirectionalBlurOperation::determineDependingAreaOfInterest(rcti * /*input*/,
                                                                ReadBufferOperation *readOperation,
                                                                rcti *output)
{
  rcti newInput;

  newInput.xmax = this->getWidth();
  newInput.xmin = 0;
  newInput.ymax = this->getHeight();
  newInput.ymin = 0;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void DirectionalBlurOperation::get_area_of_interest(const int input_idx,
                                                    const rcti &UNUSED(output_area),
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
  const int iterations = pow(2.0f, m_data->iter);
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;
    float color_accum[4];
    input->read_elem_bilinear(x, y, color_accum);

    /* Blur pixel. */
    /* TODO(manzanilla): Many values used on iterations can be calculated beforehand. Create a
     * table on operation initialization. */
    float ltx = m_tx;
    float lty = m_ty;
    float lsc = m_sc;
    float lrot = m_rot;
    for (int i = 0; i < iterations; i++) {
      const float cs = cosf(lrot), ss = sinf(lrot);
      const float isc = 1.0f / (1.0f + lsc);

      const float v = isc * (y - m_center_y_pix) + lty;
      const float u = isc * (x - m_center_x_pix) + ltx;

      float color[4];
      input->read_elem_bilinear(
          cs * u + ss * v + m_center_x_pix, cs * v - ss * u + m_center_y_pix, color);
      add_v4_v4(color_accum, color);

      /* Double transformations. */
      ltx += m_tx;
      lty += m_ty;
      lrot += m_rot;
      lsc += m_sc;
    }

    mul_v4_v4fl(it.out, color_accum, 1.0f / (iterations + 1));
  }
}

}  // namespace blender::compositor
