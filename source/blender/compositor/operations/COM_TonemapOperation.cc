/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_TonemapOperation.h"

#include "COM_ExecutionSystem.h"

#include "IMB_colormanagement.h"

namespace blender::compositor {

TonemapOperation::TonemapOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::Align);
  this->add_output_socket(DataType::Color);
  image_reader_ = nullptr;
  data_ = nullptr;
  cached_instance_ = nullptr;
  flags_.complex = true;
}
void TonemapOperation::init_execution()
{
  image_reader_ = this->get_input_socket_reader(0);
  NodeOperation::init_mutex();
}

void TonemapOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  AvgLogLum *avg = (AvgLogLum *)data;

  image_reader_->read(output, x, y, nullptr);
  mul_v3_fl(output, avg->al);
  float dr = output[0] + data_->offset;
  float dg = output[1] + data_->offset;
  float db = output[2] + data_->offset;
  output[0] /= ((dr == 0.0f) ? 1.0f : dr);
  output[1] /= ((dg == 0.0f) ? 1.0f : dg);
  output[2] /= ((db == 0.0f) ? 1.0f : db);
  const float igm = avg->igm;
  if (igm != 0.0f) {
    output[0] = powf(MAX2(output[0], 0.0f), igm);
    output[1] = powf(MAX2(output[1], 0.0f), igm);
    output[2] = powf(MAX2(output[2], 0.0f), igm);
  }
}
void PhotoreceptorTonemapOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  AvgLogLum *avg = (AvgLogLum *)data;
  const NodeTonemap *ntm = data_;

  const float f = expf(-data_->f);
  const float m = (ntm->m > 0.0f) ? ntm->m : (0.3f + 0.7f * powf(avg->auto_key, 1.4f));
  const float ic = 1.0f - ntm->c, ia = 1.0f - ntm->a;

  image_reader_->read(output, x, y, nullptr);

  const float L = IMB_colormanagement_get_luminance(output);
  float I_l = output[0] + ic * (L - output[0]);
  float I_g = avg->cav[0] + ic * (avg->lav - avg->cav[0]);
  float I_a = I_l + ia * (I_g - I_l);
  output[0] /= (output[0] + powf(f * I_a, m));
  I_l = output[1] + ic * (L - output[1]);
  I_g = avg->cav[1] + ic * (avg->lav - avg->cav[1]);
  I_a = I_l + ia * (I_g - I_l);
  output[1] /= (output[1] + powf(f * I_a, m));
  I_l = output[2] + ic * (L - output[2]);
  I_g = avg->cav[2] + ic * (avg->lav - avg->cav[2]);
  I_a = I_l + ia * (I_g - I_l);
  output[2] /= (output[2] + powf(f * I_a, m));
}

void TonemapOperation::deinit_execution()
{
  image_reader_ = nullptr;
  delete cached_instance_;
  NodeOperation::deinit_mutex();
}

bool TonemapOperation::determine_depending_area_of_interest(rcti * /*input*/,
                                                            ReadBufferOperation *read_operation,
                                                            rcti *output)
{
  rcti image_input;

  NodeOperation *operation = get_input_operation(0);
  image_input.xmax = operation->get_width();
  image_input.xmin = 0;
  image_input.ymax = operation->get_height();
  image_input.ymin = 0;
  if (operation->determine_depending_area_of_interest(&image_input, read_operation, output)) {
    return true;
  }
  return false;
}

void *TonemapOperation::initialize_tile_data(rcti *rect)
{
  lock_mutex();
  if (cached_instance_ == nullptr) {
    MemoryBuffer *tile = (MemoryBuffer *)image_reader_->initialize_tile_data(rect);
    AvgLogLum *data = new AvgLogLum();

    float *buffer = tile->get_buffer();

    float lsum = 0.0f;
    int p = tile->get_width() * tile->get_height();
    float *bc = buffer;
    float avl, maxl = -1e10f, minl = 1e10f;
    const float sc = 1.0f / p;
    float Lav = 0.0f;
    float cav[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    while (p--) {
      float L = IMB_colormanagement_get_luminance(bc);
      Lav += L;
      add_v3_v3(cav, bc);
      lsum += logf(MAX2(L, 0.0f) + 1e-5f);
      maxl = (L > maxl) ? L : maxl;
      minl = (L < minl) ? L : minl;
      bc += 4;
    }
    data->lav = Lav * sc;
    mul_v3_v3fl(data->cav, cav, sc);
    maxl = log(double(maxl) + 1e-5);
    minl = log(double(minl) + 1e-5);
    avl = lsum * sc;
    data->auto_key = (maxl > minl) ? ((maxl - avl) / (maxl - minl)) : 1.0f;
    float al = exp(double(avl));
    data->al = (al == 0.0f) ? 0.0f : (data_->key / al);
    data->igm = (data_->gamma == 0.0f) ? 1 : (1.0f / data_->gamma);
    cached_instance_ = data;
  }
  unlock_mutex();
  return cached_instance_;
}

void TonemapOperation::deinitialize_tile_data(rcti * /*rect*/, void * /*data*/)
{
  /* pass */
}

void TonemapOperation::get_area_of_interest(const int input_idx,
                                            const rcti & /*output_area*/,
                                            rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  r_input_area = get_input_operation(input_idx)->get_canvas();
}

struct Luminance {
  float sum;
  float color_sum[3];
  float log_sum;
  float min;
  float max;
  int num_pixels;
};

static Luminance calc_area_luminance(const MemoryBuffer *input, const rcti &area)
{
  Luminance lum = {0};
  for (const float *elem : input->get_buffer_area(area)) {
    const float lu = IMB_colormanagement_get_luminance(elem);
    lum.sum += lu;
    add_v3_v3(lum.color_sum, elem);
    lum.log_sum += logf(MAX2(lu, 0.0f) + 1e-5f);
    lum.max = MAX2(lu, lum.max);
    lum.min = MIN2(lu, lum.min);
    lum.num_pixels++;
  }
  return lum;
}

void TonemapOperation::update_memory_buffer_started(MemoryBuffer * /*output*/,
                                                    const rcti & /*area*/,
                                                    Span<MemoryBuffer *> inputs)
{
  if (cached_instance_ == nullptr) {
    Luminance lum = {0};
    const MemoryBuffer *input = inputs[0];
    exec_system_->execute_work<Luminance>(
        input->get_rect(),
        [=](const rcti &split) { return calc_area_luminance(input, split); },
        lum,
        [](Luminance &join, const Luminance &chunk) {
          join.sum += chunk.sum;
          add_v3_v3(join.color_sum, chunk.color_sum);
          join.log_sum += chunk.log_sum;
          join.max = MAX2(join.max, chunk.max);
          join.min = MIN2(join.min, chunk.min);
          join.num_pixels += chunk.num_pixels;
        });

    AvgLogLum *avg = new AvgLogLum();
    avg->lav = lum.sum / lum.num_pixels;
    mul_v3_v3fl(avg->cav, lum.color_sum, 1.0f / lum.num_pixels);
    const float max_log = log(double(lum.max) + 1e-5);
    const float min_log = log(double(lum.min) + 1e-5);
    const float avg_log = lum.log_sum / lum.num_pixels;
    avg->auto_key = (max_log > min_log) ? ((max_log - avg_log) / (max_log - min_log)) : 1.0f;
    const float al = exp(double(avg_log));
    avg->al = (al == 0.0f) ? 0.0f : (data_->key / al);
    avg->igm = (data_->gamma == 0.0f) ? 1 : (1.0f / data_->gamma);
    cached_instance_ = avg;
  }
}

void TonemapOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                    const rcti &area,
                                                    Span<MemoryBuffer *> inputs)
{
  AvgLogLum *avg = cached_instance_;
  const float igm = avg->igm;
  const float offset = data_->offset;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    copy_v4_v4(it.out, it.in(0));
    mul_v3_fl(it.out, avg->al);
    float dr = it.out[0] + offset;
    float dg = it.out[1] + offset;
    float db = it.out[2] + offset;
    it.out[0] /= ((dr == 0.0f) ? 1.0f : dr);
    it.out[1] /= ((dg == 0.0f) ? 1.0f : dg);
    it.out[2] /= ((db == 0.0f) ? 1.0f : db);
    if (igm != 0.0f) {
      it.out[0] = powf(MAX2(it.out[0], 0.0f), igm);
      it.out[1] = powf(MAX2(it.out[1], 0.0f), igm);
      it.out[2] = powf(MAX2(it.out[2], 0.0f), igm);
    }
  }
}

void PhotoreceptorTonemapOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                 const rcti &area,
                                                                 Span<MemoryBuffer *> inputs)
{
  AvgLogLum *avg = cached_instance_;
  const NodeTonemap *ntm = data_;
  const float f = expf(-data_->f);
  const float m = (ntm->m > 0.0f) ? ntm->m : (0.3f + 0.7f * powf(avg->auto_key, 1.4f));
  const float ic = 1.0f - ntm->c;
  const float ia = 1.0f - ntm->a;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    copy_v4_v4(it.out, it.in(0));
    const float L = IMB_colormanagement_get_luminance(it.out);
    float I_l = it.out[0] + ic * (L - it.out[0]);
    float I_g = avg->cav[0] + ic * (avg->lav - avg->cav[0]);
    float I_a = I_l + ia * (I_g - I_l);
    it.out[0] /= (it.out[0] + powf(f * I_a, m));
    I_l = it.out[1] + ic * (L - it.out[1]);
    I_g = avg->cav[1] + ic * (avg->lav - avg->cav[1]);
    I_a = I_l + ia * (I_g - I_l);
    it.out[1] /= (it.out[1] + powf(f * I_a, m));
    I_l = it.out[2] + ic * (L - it.out[2]);
    I_g = avg->cav[2] + ic * (avg->lav - avg->cav[2]);
    I_a = I_l + ia * (I_g - I_l);
    it.out[2] /= (it.out[2] + powf(f * I_a, m));
  }
}

}  // namespace blender::compositor
