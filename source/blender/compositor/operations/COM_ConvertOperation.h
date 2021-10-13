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

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class ConvertBaseOperation : public MultiThreadedOperation {
 protected:
  SocketReader *inputOperation_;

 public:
  ConvertBaseOperation();

  void initExecution() override;
  void deinitExecution() override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) final;

 protected:
  virtual void hash_output_params() override;
  virtual void update_memory_buffer_partial(BuffersIterator<float> &it) = 0;
};

class ConvertValueToColorOperation : public ConvertBaseOperation {
 public:
  ConvertValueToColorOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertColorToValueOperation : public ConvertBaseOperation {
 public:
  ConvertColorToValueOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertColorToBWOperation : public ConvertBaseOperation {
 public:
  ConvertColorToBWOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertColorToVectorOperation : public ConvertBaseOperation {
 public:
  ConvertColorToVectorOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertValueToVectorOperation : public ConvertBaseOperation {
 public:
  ConvertValueToVectorOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertVectorToColorOperation : public ConvertBaseOperation {
 public:
  ConvertVectorToColorOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertVectorToValueOperation : public ConvertBaseOperation {
 public:
  ConvertVectorToValueOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertRGBToYCCOperation : public ConvertBaseOperation {
 private:
  /** YCbCr mode (Jpeg, ITU601, ITU709) */
  int mode_;

 public:
  ConvertRGBToYCCOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  /** Set the YCC mode */
  void setMode(int mode);

 protected:
  void hash_output_params() override;
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertYCCToRGBOperation : public ConvertBaseOperation {
 private:
  /** YCbCr mode (Jpeg, ITU601, ITU709) */
  int mode_;

 public:
  ConvertYCCToRGBOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  /** Set the YCC mode */
  void setMode(int mode);

 protected:
  void hash_output_params() override;
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertRGBToYUVOperation : public ConvertBaseOperation {
 public:
  ConvertRGBToYUVOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertYUVToRGBOperation : public ConvertBaseOperation {
 public:
  ConvertYUVToRGBOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertRGBToHSVOperation : public ConvertBaseOperation {
 public:
  ConvertRGBToHSVOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertHSVToRGBOperation : public ConvertBaseOperation {
 public:
  ConvertHSVToRGBOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertPremulToStraightOperation : public ConvertBaseOperation {
 public:
  ConvertPremulToStraightOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class ConvertStraightToPremulOperation : public ConvertBaseOperation {
 public:
  ConvertStraightToPremulOperation();

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class SeparateChannelOperation : public MultiThreadedOperation {
 private:
  SocketReader *inputOperation_;
  int channel_;

 public:
  SeparateChannelOperation();
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  void initExecution() override;
  void deinitExecution() override;

  void setChannel(int channel)
  {
    channel_ = channel;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class CombineChannelsOperation : public MultiThreadedOperation {
 private:
  SocketReader *inputChannel1Operation_;
  SocketReader *inputChannel2Operation_;
  SocketReader *inputChannel3Operation_;
  SocketReader *inputChannel4Operation_;

 public:
  CombineChannelsOperation();
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  void initExecution() override;
  void deinitExecution() override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
