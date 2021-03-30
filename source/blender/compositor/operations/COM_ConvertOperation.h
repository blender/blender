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

#pragma once

#include "COM_NodeOperation.h"

namespace blender::compositor {

class ConvertBaseOperation : public NodeOperation {
 protected:
  SocketReader *m_inputOperation;

 public:
  ConvertBaseOperation();

  void initExecution() override;
  void deinitExecution() override;
};

class ConvertValueToColorOperation : public ConvertBaseOperation {
 public:
  ConvertValueToColorOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertColorToValueOperation : public ConvertBaseOperation {
 public:
  ConvertColorToValueOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertColorToBWOperation : public ConvertBaseOperation {
 public:
  ConvertColorToBWOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertColorToVectorOperation : public ConvertBaseOperation {
 public:
  ConvertColorToVectorOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertValueToVectorOperation : public ConvertBaseOperation {
 public:
  ConvertValueToVectorOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertVectorToColorOperation : public ConvertBaseOperation {
 public:
  ConvertVectorToColorOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertVectorToValueOperation : public ConvertBaseOperation {
 public:
  ConvertVectorToValueOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertRGBToYCCOperation : public ConvertBaseOperation {
 private:
  /** YCbCr mode (Jpeg, ITU601, ITU709) */
  int m_mode;

 public:
  ConvertRGBToYCCOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  /** Set the YCC mode */
  void setMode(int mode);
};

class ConvertYCCToRGBOperation : public ConvertBaseOperation {
 private:
  /** YCbCr mode (Jpeg, ITU601, ITU709) */
  int m_mode;

 public:
  ConvertYCCToRGBOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  /** Set the YCC mode */
  void setMode(int mode);
};

class ConvertRGBToYUVOperation : public ConvertBaseOperation {
 public:
  ConvertRGBToYUVOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertYUVToRGBOperation : public ConvertBaseOperation {
 public:
  ConvertYUVToRGBOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertRGBToHSVOperation : public ConvertBaseOperation {
 public:
  ConvertRGBToHSVOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertHSVToRGBOperation : public ConvertBaseOperation {
 public:
  ConvertHSVToRGBOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertPremulToStraightOperation : public ConvertBaseOperation {
 public:
  ConvertPremulToStraightOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class ConvertStraightToPremulOperation : public ConvertBaseOperation {
 public:
  ConvertStraightToPremulOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class SeparateChannelOperation : public NodeOperation {
 private:
  SocketReader *m_inputOperation;
  int m_channel;

 public:
  SeparateChannelOperation();
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  void initExecution() override;
  void deinitExecution() override;

  void setChannel(int channel)
  {
    this->m_channel = channel;
  }
};

class CombineChannelsOperation : public NodeOperation {
 private:
  SocketReader *m_inputChannel1Operation;
  SocketReader *m_inputChannel2Operation;
  SocketReader *m_inputChannel3Operation;
  SocketReader *m_inputChannel4Operation;

 public:
  CombineChannelsOperation();
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  void initExecution() override;
  void deinitExecution() override;
};

}  // namespace blender::compositor
