/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <climits>

#include "COM_FastGaussianBlurOperation.h"

namespace blender::compositor {

FastGaussianBlurOperation::FastGaussianBlurOperation() : BlurBaseOperation(DataType::Color)
{
  iirgaus_ = nullptr;
  data_.filtertype = R_FILTER_FAST_GAUSS;
}

void FastGaussianBlurOperation::init_data()
{
  BlurBaseOperation::init_data();
  sx_ = data_.sizex * size_ / 2.0f;
  sy_ = data_.sizey * size_ / 2.0f;
}

void FastGaussianBlurOperation::init_execution()
{
  BlurBaseOperation::init_execution();
}

void FastGaussianBlurOperation::deinit_execution()
{
  if (iirgaus_) {
    delete iirgaus_;
    iirgaus_ = nullptr;
  }
}

void FastGaussianBlurOperation::set_size(int size_x, int size_y)
{
  /* TODO: there should be a better way to use the operation without knowing specifics of the blur
   * node (i.e. data_). We could use factory pattern to solve this problem. */
  data_.sizex = size_x;
  data_.sizey = size_y;
  sizeavailable_ = true;
}

void FastGaussianBlurOperation::IIR_gauss(MemoryBuffer *src, float sigma, uint chan, uint xy)
{
  BLI_assert(!src->is_a_single_elem());
  double q, q2, sc, cf[4], tsM[9], tsu[3], tsv[3];
  double *X, *Y, *W;
  const uint src_width = src->get_width();
  const uint src_height = src->get_height();
  uint x, y, src_dim_max;
  uint i;
  float *buffer = src->get_buffer();
  const uint8_t num_channels = src->get_num_channels();

  /* <0.5 not valid, though can have a possibly useful sort of sharpening effect. */
  if (sigma < 0.5f) {
    return;
  }

  if ((xy < 1) || (xy > 3)) {
    xy = 3;
  }

  /* XXX The YVV macro defined below explicitly expects sources of at least 3x3 pixels,
   *     so just skipping blur along faulty direction if src's def is below that limit! */
  if (src_width < 3) {
    xy &= ~1;
  }
  if (src_height < 3) {
    xy &= ~2;
  }
  if (xy < 1) {
    return;
  }

  /* See "Recursive Gabor Filtering" by Young/VanVliet
   * all factors here in double-precision.
   * Required, because for single-precision floating point seems to blow up if `sigma > ~200`. */
  if (sigma >= 3.556f) {
    q = 0.9804f * (sigma - 3.556f) + 2.5091f;
  }
  else { /* `sigma >= 0.5`. */
    q = (0.0561f * sigma + 0.5784f) * sigma - 0.2568f;
  }
  q2 = q * q;
  sc = (1.1668 + q) * (3.203729649 + (2.21566 + q) * q);
  /* No gabor filtering here, so no complex multiplies, just the regular coefficients.
   * all negated here, so as not to have to recalc Triggs/Sdika matrix. */
  cf[1] = q * (5.788961737 + (6.76492 + 3.0 * q) * q) / sc;
  cf[2] = -q2 * (3.38246 + 3.0 * q) / sc;
  /* 0 & 3 unchanged. */
  cf[3] = q2 * q / sc;
  cf[0] = 1.0 - cf[1] - cf[2] - cf[3];

  /* Triggs/Sdika border corrections,
   * it seems to work, not entirely sure if it is actually totally correct,
   * Besides J.M.Geusebroek's `anigauss.c` (see http://www.science.uva.nl/~mark),
   * found one other implementation by Cristoph Lampert,
   * but neither seem to be quite the same, result seems to be ok so far anyway.
   * Extra scale factor here to not have to do it in filter,
   * though maybe this had something to with the precision errors */
  sc = cf[0] / ((1.0 + cf[1] - cf[2] + cf[3]) * (1.0 - cf[1] - cf[2] - cf[3]) *
                (1.0 + cf[2] + (cf[1] - cf[3]) * cf[3]));
  tsM[0] = sc * (-cf[3] * cf[1] + 1.0 - cf[3] * cf[3] - cf[2]);
  tsM[1] = sc * ((cf[3] + cf[1]) * (cf[2] + cf[3] * cf[1]));
  tsM[2] = sc * (cf[3] * (cf[1] + cf[3] * cf[2]));
  tsM[3] = sc * (cf[1] + cf[3] * cf[2]);
  tsM[4] = sc * (-(cf[2] - 1.0) * (cf[2] + cf[3] * cf[1]));
  tsM[5] = sc * (-(cf[3] * cf[1] + cf[3] * cf[3] + cf[2] - 1.0) * cf[3]);
  tsM[6] = sc * (cf[3] * cf[1] + cf[2] + cf[1] * cf[1] - cf[2] * cf[2]);
  tsM[7] = sc * (cf[1] * cf[2] + cf[3] * cf[2] * cf[2] - cf[1] * cf[3] * cf[3] -
                 cf[3] * cf[3] * cf[3] - cf[3] * cf[2] + cf[3]);
  tsM[8] = sc * (cf[3] * (cf[1] + cf[3] * cf[2]));

#define YVV(L) \
  { \
    W[0] = cf[0] * X[0] + cf[1] * X[0] + cf[2] * X[0] + cf[3] * X[0]; \
    W[1] = cf[0] * X[1] + cf[1] * W[0] + cf[2] * X[0] + cf[3] * X[0]; \
    W[2] = cf[0] * X[2] + cf[1] * W[1] + cf[2] * W[0] + cf[3] * X[0]; \
    for (i = 3; i < L; i++) { \
      W[i] = cf[0] * X[i] + cf[1] * W[i - 1] + cf[2] * W[i - 2] + cf[3] * W[i - 3]; \
    } \
    tsu[0] = W[L - 1] - X[L - 1]; \
    tsu[1] = W[L - 2] - X[L - 1]; \
    tsu[2] = W[L - 3] - X[L - 1]; \
    tsv[0] = tsM[0] * tsu[0] + tsM[1] * tsu[1] + tsM[2] * tsu[2] + X[L - 1]; \
    tsv[1] = tsM[3] * tsu[0] + tsM[4] * tsu[1] + tsM[5] * tsu[2] + X[L - 1]; \
    tsv[2] = tsM[6] * tsu[0] + tsM[7] * tsu[1] + tsM[8] * tsu[2] + X[L - 1]; \
    Y[L - 1] = cf[0] * W[L - 1] + cf[1] * tsv[0] + cf[2] * tsv[1] + cf[3] * tsv[2]; \
    Y[L - 2] = cf[0] * W[L - 2] + cf[1] * Y[L - 1] + cf[2] * tsv[0] + cf[3] * tsv[1]; \
    Y[L - 3] = cf[0] * W[L - 3] + cf[1] * Y[L - 2] + cf[2] * Y[L - 1] + cf[3] * tsv[0]; \
    /* `i != UINT_MAX` is really `i >= 0`, but necessary for `uint` wrapping. */ \
    for (i = L - 4; i != UINT_MAX; i--) { \
      Y[i] = cf[0] * W[i] + cf[1] * Y[i + 1] + cf[2] * Y[i + 2] + cf[3] * Y[i + 3]; \
    } \
  } \
  (void)0

  /* Intermediate buffers. */
  src_dim_max = std::max(src_width, src_height);
  X = (double *)MEM_callocN(src_dim_max * sizeof(double), "IIR_gauss X buf");
  Y = (double *)MEM_callocN(src_dim_max * sizeof(double), "IIR_gauss Y buf");
  W = (double *)MEM_callocN(src_dim_max * sizeof(double), "IIR_gauss W buf");
  if (xy & 1) { /* H. */
    int offset;
    for (y = 0; y < src_height; y++) {
      const int yx = y * src_width;
      offset = yx * num_channels + chan;
      for (x = 0; x < src_width; x++) {
        X[x] = buffer[offset];
        offset += num_channels;
      }
      YVV(src_width);
      offset = yx * num_channels + chan;
      for (x = 0; x < src_width; x++) {
        buffer[offset] = Y[x];
        offset += num_channels;
      }
    }
  }
  if (xy & 2) { /* V. */
    int offset;
    const int add = src_width * num_channels;

    for (x = 0; x < src_width; x++) {
      offset = x * num_channels + chan;
      for (y = 0; y < src_height; y++) {
        X[y] = buffer[offset];
        offset += add;
      }
      YVV(src_height);
      offset = x * num_channels + chan;
      for (y = 0; y < src_height; y++) {
        buffer[offset] = Y[y];
        offset += add;
      }
    }
  }

  MEM_freeN(X);
  MEM_freeN(W);
  MEM_freeN(Y);
#undef YVV
}

void FastGaussianBlurOperation::get_area_of_interest(const int input_idx,
                                                     const rcti &output_area,
                                                     rcti &r_input_area)
{
  switch (input_idx) {
    case IMAGE_INPUT_INDEX:
      r_input_area = this->get_canvas();
      break;
    default:
      BlurBaseOperation::get_area_of_interest(input_idx, output_area, r_input_area);
      return;
  }
}

void FastGaussianBlurOperation::update_memory_buffer_started(MemoryBuffer *output,
                                                             const rcti &area,
                                                             Span<MemoryBuffer *> inputs)
{
  /* TODO(manzanilla): Add a render test and make #IIR_gauss multi-threaded with support for
   * an output buffer. */
  const MemoryBuffer *input = inputs[IMAGE_INPUT_INDEX];
  MemoryBuffer *image = nullptr;
  const bool is_full_output = BLI_rcti_compare(&output->get_rect(), &area);
  if (is_full_output) {
    image = output;
  }
  else {
    image = new MemoryBuffer(get_output_socket()->get_data_type(), area);
  }
  image->copy_from(input, area);

  if ((sx_ == sy_) && (sx_ > 0.0f)) {
    for (const int c : IndexRange(COM_DATA_TYPE_COLOR_CHANNELS)) {
      IIR_gauss(image, sx_, c, 3);
    }
  }
  else {
    if (sx_ > 0.0f) {
      for (const int c : IndexRange(COM_DATA_TYPE_COLOR_CHANNELS)) {
        IIR_gauss(image, sx_, c, 1);
      }
    }
    if (sy_ > 0.0f) {
      for (const int c : IndexRange(COM_DATA_TYPE_COLOR_CHANNELS)) {
        IIR_gauss(image, sy_, c, 2);
      }
    }
  }

  if (!is_full_output) {
    output->copy_from(image, area);
    delete image;
  }
}

FastGaussianBlurValueOperation::FastGaussianBlurValueOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  iirgaus_ = nullptr;
  sigma_ = 1.0f;
  overlay_ = 0;
}

void FastGaussianBlurValueOperation::init_execution() {}

void FastGaussianBlurValueOperation::deinit_execution()
{
  if (iirgaus_) {
    delete iirgaus_;
    iirgaus_ = nullptr;
  }
}

void FastGaussianBlurValueOperation::get_area_of_interest(const int /*input_idx*/,
                                                          const rcti & /*output_area*/,
                                                          rcti &r_input_area)
{
  r_input_area = this->get_canvas();
}

void FastGaussianBlurValueOperation::update_memory_buffer_started(MemoryBuffer * /*output*/,
                                                                  const rcti & /*area*/,
                                                                  Span<MemoryBuffer *> inputs)
{
  if (iirgaus_ == nullptr) {
    const MemoryBuffer *image = inputs[0];
    MemoryBuffer *gauss = new MemoryBuffer(*image);
    FastGaussianBlurOperation::IIR_gauss(gauss, sigma_, 0, 3);
    iirgaus_ = gauss;
  }
}

void FastGaussianBlurValueOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                  const rcti &area,
                                                                  Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *image = inputs[0];
  BuffersIterator<float> it = output->iterate_with({image, iirgaus_}, area);
  if (overlay_ == FAST_GAUSS_OVERLAY_MIN) {
    for (; !it.is_end(); ++it) {
      *it.out = std::min(*it.in(0), *it.in(1));
    }
  }
  else if (overlay_ == FAST_GAUSS_OVERLAY_MAX) {
    for (; !it.is_end(); ++it) {
      *it.out = std::max(*it.in(0), *it.in(1));
    }
  }
}

}  // namespace blender::compositor
