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

#include "COM_MemoryBuffer.h"

#include "COM_MemoryProxy.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"

#define ASSERT_BUFFER_CONTAINS_AREA(buf, area) \
  BLI_assert(BLI_rcti_inside_rcti(&(buf)->get_rect(), &(area)))

#define ASSERT_BUFFER_CONTAINS_AREA_AT_COORDS(buf, area, x, y) \
  BLI_assert((buf)->get_rect().xmin <= (x)); \
  BLI_assert((buf)->get_rect().ymin <= (y)); \
  BLI_assert((buf)->get_rect().xmax >= (x) + BLI_rcti_size_x(&(area))); \
  BLI_assert((buf)->get_rect().ymax >= (y) + BLI_rcti_size_y(&(area)))

#define ASSERT_VALID_ELEM_SIZE(buf, channel_offset, elem_size) \
  BLI_assert((buf)->get_num_channels() >= (channel_offset) + (elem_size))

namespace blender::compositor {

static rcti create_rect(const int width, const int height)
{
  rcti rect;
  BLI_rcti_init(&rect, 0, width, 0, height);
  return rect;
}

MemoryBuffer::MemoryBuffer(MemoryProxy *memory_proxy, const rcti &rect, MemoryBufferState state)
{
  rect_ = rect;
  is_a_single_elem_ = false;
  memory_proxy_ = memory_proxy;
  num_channels_ = COM_data_type_num_channels(memory_proxy->get_data_type());
  buffer_ = (float *)MEM_mallocN_aligned(
      sizeof(float) * buffer_len() * num_channels_, 16, "COM_MemoryBuffer");
  owns_data_ = true;
  state_ = state;
  datatype_ = memory_proxy->get_data_type();

  set_strides();
}

MemoryBuffer::MemoryBuffer(DataType data_type, const rcti &rect, bool is_a_single_elem)
{
  rect_ = rect;
  is_a_single_elem_ = is_a_single_elem;
  memory_proxy_ = nullptr;
  num_channels_ = COM_data_type_num_channels(data_type);
  buffer_ = (float *)MEM_mallocN_aligned(
      sizeof(float) * buffer_len() * num_channels_, 16, "COM_MemoryBuffer");
  owns_data_ = true;
  state_ = MemoryBufferState::Temporary;
  datatype_ = data_type;

  set_strides();
}

/**
 * Construct MemoryBuffer from a float buffer. MemoryBuffer is not responsible for
 * freeing it.
 */
MemoryBuffer::MemoryBuffer(
    float *buffer, int num_channels, int width, int height, bool is_a_single_elem)
    : MemoryBuffer(buffer, num_channels, create_rect(width, height), is_a_single_elem)
{
}

/**
 * Construct MemoryBuffer from a float buffer area. MemoryBuffer is not responsible for
 * freeing given buffer.
 */
MemoryBuffer::MemoryBuffer(float *buffer,
                           const int num_channels,
                           const rcti &rect,
                           const bool is_a_single_elem)
{
  rect_ = rect;
  is_a_single_elem_ = is_a_single_elem;
  memory_proxy_ = nullptr;
  num_channels_ = num_channels;
  datatype_ = COM_num_channels_data_type(num_channels);
  buffer_ = buffer;
  owns_data_ = false;
  state_ = MemoryBufferState::Temporary;

  set_strides();
}

MemoryBuffer::MemoryBuffer(const MemoryBuffer &src) : MemoryBuffer(src.datatype_, src.rect_, false)
{
  memory_proxy_ = src.memory_proxy_;
  /* src may be single elem buffer */
  fill_from(src);
}

void MemoryBuffer::set_strides()
{
  if (is_a_single_elem_) {
    this->elem_stride = 0;
    this->row_stride = 0;
  }
  else {
    this->elem_stride = num_channels_;
    this->row_stride = get_width() * num_channels_;
  }
  to_positive_x_stride_ = rect_.xmin < 0 ? -rect_.xmin + 1 : (rect_.xmin == 0 ? 1 : 0);
  to_positive_y_stride_ = rect_.ymin < 0 ? -rect_.ymin + 1 : (rect_.ymin == 0 ? 1 : 0);
}

void MemoryBuffer::clear()
{
  memset(buffer_, 0, buffer_len() * num_channels_ * sizeof(float));
}

BuffersIterator<float> MemoryBuffer::iterate_with(Span<MemoryBuffer *> inputs)
{
  return iterate_with(inputs, rect_);
}

BuffersIterator<float> MemoryBuffer::iterate_with(Span<MemoryBuffer *> inputs, const rcti &area)
{
  BuffersIteratorBuilder<float> builder(buffer_, rect_, area, elem_stride);
  for (MemoryBuffer *input : inputs) {
    builder.add_input(input->get_buffer(), input->get_rect(), input->elem_stride);
  }
  return builder.build();
}

/**
 * Converts a single elem buffer to a full size buffer (allocates memory for all
 * elements in resolution).
 */
MemoryBuffer *MemoryBuffer::inflate() const
{
  BLI_assert(is_a_single_elem());
  MemoryBuffer *inflated = new MemoryBuffer(datatype_, rect_, false);
  inflated->copy_from(this, rect_);
  return inflated;
}

float MemoryBuffer::get_max_value() const
{
  float result = buffer_[0];
  const unsigned int size = this->buffer_len();
  unsigned int i;

  const float *fp_src = buffer_;

  for (i = 0; i < size; i++, fp_src += num_channels_) {
    float value = *fp_src;
    if (value > result) {
      result = value;
    }
  }

  return result;
}

float MemoryBuffer::get_max_value(const rcti &rect) const
{
  rcti rect_clamp;

  /* first clamp the rect by the bounds or we get un-initialized values */
  BLI_rcti_isect(&rect, &rect_, &rect_clamp);

  if (!BLI_rcti_is_empty(&rect_clamp)) {
    MemoryBuffer temp_buffer(datatype_, rect_clamp);
    temp_buffer.fill_from(*this);
    return temp_buffer.get_max_value();
  }

  BLI_assert(0);
  return 0.0f;
}

MemoryBuffer::~MemoryBuffer()
{
  if (buffer_ && owns_data_) {
    MEM_freeN(buffer_);
    buffer_ = nullptr;
  }
}

void MemoryBuffer::copy_from(const MemoryBuffer *src, const rcti &area)
{
  copy_from(src, area, area.xmin, area.ymin);
}

void MemoryBuffer::copy_from(const MemoryBuffer *src,
                             const rcti &area,
                             const int to_x,
                             const int to_y)
{
  BLI_assert(this->get_num_channels() == src->get_num_channels());
  copy_from(src, area, 0, src->get_num_channels(), to_x, to_y, 0);
}

void MemoryBuffer::copy_from(const MemoryBuffer *src,
                             const rcti &area,
                             const int channel_offset,
                             const int elem_size,
                             const int to_channel_offset)
{
  copy_from(src, area, channel_offset, elem_size, area.xmin, area.ymin, to_channel_offset);
}

void MemoryBuffer::copy_from(const MemoryBuffer *src,
                             const rcti &area,
                             const int channel_offset,
                             const int elem_size,
                             const int to_x,
                             const int to_y,
                             const int to_channel_offset)
{
  if (this->is_a_single_elem()) {
    copy_single_elem_from(src, channel_offset, elem_size, to_channel_offset);
  }
  else if (!src->is_a_single_elem() && elem_size == src->get_num_channels() &&
           elem_size == this->get_num_channels()) {
    BLI_assert(to_channel_offset == 0);
    BLI_assert(channel_offset == 0);
    copy_rows_from(src, area, to_x, to_y);
  }
  else {
    copy_elems_from(src, area, channel_offset, elem_size, to_x, to_y, to_channel_offset);
  }
}

void MemoryBuffer::copy_from(const uchar *src, const rcti &area)
{
  const int elem_stride = this->get_num_channels();
  const int row_stride = elem_stride * get_width();
  copy_from(src, area, 0, this->get_num_channels(), elem_stride, row_stride, 0);
}

void MemoryBuffer::copy_from(const uchar *src,
                             const rcti &area,
                             const int channel_offset,
                             const int elem_size,
                             const int elem_stride,
                             const int row_stride,
                             const int to_channel_offset)
{
  copy_from(src,
            area,
            channel_offset,
            elem_size,
            elem_stride,
            row_stride,
            area.xmin,
            area.ymin,
            to_channel_offset);
}

void MemoryBuffer::copy_from(const uchar *src,
                             const rcti &area,
                             const int channel_offset,
                             const int elem_size,
                             const int elem_stride,
                             const int row_stride,
                             const int to_x,
                             const int to_y,
                             const int to_channel_offset)
{
  ASSERT_BUFFER_CONTAINS_AREA_AT_COORDS(this, area, to_x, to_y);
  ASSERT_VALID_ELEM_SIZE(this, to_channel_offset, elem_size);

  const int width = BLI_rcti_size_x(&area);
  const int height = BLI_rcti_size_y(&area);
  const uchar *const src_start = src + area.ymin * row_stride + channel_offset;
  for (int y = 0; y < height; y++) {
    const uchar *from_elem = src_start + y * row_stride + area.xmin * elem_stride;
    float *to_elem = &this->get_value(to_x, to_y + y, to_channel_offset);
    const float *row_end = to_elem + width * this->elem_stride;
    while (to_elem < row_end) {
      for (int i = 0; i < elem_size; i++) {
        to_elem[i] = ((float)from_elem[i]) * (1.0f / 255.0f);
      }
      to_elem += this->elem_stride;
      from_elem += elem_stride;
    }
  }
}

static void colorspace_to_scene_linear(MemoryBuffer *buf, const rcti &area, ColorSpace *colorspace)
{
  const int width = BLI_rcti_size_x(&area);
  const int height = BLI_rcti_size_y(&area);
  float *out = buf->get_elem(area.xmin, area.ymin);
  /* If area allows continuous memory do conversion in one step. Otherwise per row. */
  if (buf->get_width() == width) {
    IMB_colormanagement_colorspace_to_scene_linear(
        out, width, height, buf->get_num_channels(), colorspace, false);
  }
  else {
    for (int y = 0; y < height; y++) {
      IMB_colormanagement_colorspace_to_scene_linear(
          out, width, 1, buf->get_num_channels(), colorspace, false);
      out += buf->row_stride;
    }
  }
}

void MemoryBuffer::copy_from(const ImBuf *src, const rcti &area, const bool ensure_linear_space)
{
  copy_from(src, area, 0, this->get_num_channels(), 0, ensure_linear_space);
}

void MemoryBuffer::copy_from(const ImBuf *src,
                             const rcti &area,
                             const int channel_offset,
                             const int elem_size,
                             const int to_channel_offset,
                             const bool ensure_linear_space)
{
  copy_from(src,
            area,
            channel_offset,
            elem_size,
            area.xmin,
            area.ymin,
            to_channel_offset,
            ensure_linear_space);
}

void MemoryBuffer::copy_from(const ImBuf *src,
                             const rcti &area,
                             const int channel_offset,
                             const int elem_size,
                             const int to_x,
                             const int to_y,
                             const int to_channel_offset,
                             const bool ensure_linear_space)
{
  if (src->rect_float) {
    const MemoryBuffer mem_buf(src->rect_float, src->channels, src->x, src->y, false);
    copy_from(&mem_buf, area, channel_offset, elem_size, to_x, to_y, to_channel_offset);
  }
  else if (src->rect) {
    const uchar *uc_buf = (uchar *)src->rect;
    const int elem_stride = src->channels;
    const int row_stride = elem_stride * src->x;
    copy_from(uc_buf,
              area,
              channel_offset,
              elem_size,
              elem_stride,
              row_stride,
              to_x,
              to_y,
              to_channel_offset);
    if (ensure_linear_space) {
      colorspace_to_scene_linear(this, area, src->rect_colorspace);
    }
  }
  else {
    /* Empty ImBuf source. Fill destination with empty values. */
    const float *zero_elem = new float[elem_size]{0};
    fill(area, to_channel_offset, zero_elem, elem_size);
    delete[] zero_elem;
  }
}

void MemoryBuffer::fill(const rcti &area, const float *value)
{
  fill(area, 0, value, this->get_num_channels());
}

void MemoryBuffer::fill(const rcti &area,
                        const int channel_offset,
                        const float *value,
                        const int value_size)
{
  const MemoryBuffer single_elem(const_cast<float *>(value), value_size, this->get_rect(), true);
  copy_from(&single_elem, area, 0, value_size, area.xmin, area.ymin, channel_offset);
}

void MemoryBuffer::fill_from(const MemoryBuffer &src)
{
  rcti overlap;
  overlap.xmin = MAX2(rect_.xmin, src.rect_.xmin);
  overlap.xmax = MIN2(rect_.xmax, src.rect_.xmax);
  overlap.ymin = MAX2(rect_.ymin, src.rect_.ymin);
  overlap.ymax = MIN2(rect_.ymax, src.rect_.ymax);
  copy_from(&src, overlap);
}

void MemoryBuffer::write_pixel(int x, int y, const float color[4])
{
  if (x >= rect_.xmin && x < rect_.xmax && y >= rect_.ymin && y < rect_.ymax) {
    const int offset = get_coords_offset(x, y);
    memcpy(&buffer_[offset], color, sizeof(float) * num_channels_);
  }
}

void MemoryBuffer::add_pixel(int x, int y, const float color[4])
{
  if (x >= rect_.xmin && x < rect_.xmax && y >= rect_.ymin && y < rect_.ymax) {
    const int offset = get_coords_offset(x, y);
    float *dst = &buffer_[offset];
    const float *src = color;
    for (int i = 0; i < num_channels_; i++, dst++, src++) {
      *dst += *src;
    }
  }
}

static void read_ewa_elem(void *userdata, int x, int y, float result[4])
{
  const MemoryBuffer *buffer = static_cast<const MemoryBuffer *>(userdata);
  buffer->read_elem_checked(x, y, result);
}

void MemoryBuffer::read_elem_filtered(
    const float x, const float y, float dx[2], float dy[2], float *out) const
{
  BLI_assert(datatype_ == DataType::Color);

  const float deriv[2][2] = {{dx[0], dx[1]}, {dy[0], dy[1]}};

  float inv_width = 1.0f / (float)this->get_width(), inv_height = 1.0f / (float)this->get_height();
  /* TODO(sergey): Render pipeline uses normalized coordinates and derivatives,
   * but compositor uses pixel space. For now let's just divide the values and
   * switch compositor to normalized space for EWA later.
   */
  float uv_normal[2] = {get_relative_x(x) * inv_width, get_relative_y(y) * inv_height};
  float du_normal[2] = {deriv[0][0] * inv_width, deriv[0][1] * inv_height};
  float dv_normal[2] = {deriv[1][0] * inv_width, deriv[1][1] * inv_height};

  BLI_ewa_filter(this->get_width(),
                 this->get_height(),
                 false,
                 true,
                 uv_normal,
                 du_normal,
                 dv_normal,
                 read_ewa_elem,
                 const_cast<MemoryBuffer *>(this),
                 out);
}

/* TODO(manzanilla): to be removed with tiled implementation. */
static void read_ewa_pixel_sampled(void *userdata, int x, int y, float result[4])
{
  MemoryBuffer *buffer = (MemoryBuffer *)userdata;
  buffer->read(result, x, y);
}

/* TODO(manzanilla): to be removed with tiled implementation. */
void MemoryBuffer::readEWA(float *result, const float uv[2], const float derivatives[2][2])
{
  if (is_a_single_elem_) {
    memcpy(result, buffer_, sizeof(float) * num_channels_);
  }
  else {
    BLI_assert(datatype_ == DataType::Color);
    float inv_width = 1.0f / (float)this->get_width(),
          inv_height = 1.0f / (float)this->get_height();
    /* TODO(sergey): Render pipeline uses normalized coordinates and derivatives,
     * but compositor uses pixel space. For now let's just divide the values and
     * switch compositor to normalized space for EWA later.
     */
    float uv_normal[2] = {uv[0] * inv_width, uv[1] * inv_height};
    float du_normal[2] = {derivatives[0][0] * inv_width, derivatives[0][1] * inv_height};
    float dv_normal[2] = {derivatives[1][0] * inv_width, derivatives[1][1] * inv_height};

    BLI_ewa_filter(this->get_width(),
                   this->get_height(),
                   false,
                   true,
                   uv_normal,
                   du_normal,
                   dv_normal,
                   read_ewa_pixel_sampled,
                   this,
                   result);
  }
}

void MemoryBuffer::copy_single_elem_from(const MemoryBuffer *src,
                                         const int channel_offset,
                                         const int elem_size,
                                         const int to_channel_offset)
{
  ASSERT_VALID_ELEM_SIZE(this, to_channel_offset, elem_size);
  ASSERT_VALID_ELEM_SIZE(src, channel_offset, elem_size);
  BLI_assert(this->is_a_single_elem());

  float *to_elem = &this->get_value(
      this->get_rect().xmin, this->get_rect().ymin, to_channel_offset);
  const float *from_elem = &src->get_value(
      src->get_rect().xmin, src->get_rect().ymin, channel_offset);
  const int elem_bytes = elem_size * sizeof(float);
  memcpy(to_elem, from_elem, elem_bytes);
}

void MemoryBuffer::copy_rows_from(const MemoryBuffer *src,
                                  const rcti &area,
                                  const int to_x,
                                  const int to_y)
{
  ASSERT_BUFFER_CONTAINS_AREA(src, area);
  ASSERT_BUFFER_CONTAINS_AREA_AT_COORDS(this, area, to_x, to_y);
  BLI_assert(this->get_num_channels() == src->get_num_channels());
  BLI_assert(!this->is_a_single_elem());
  BLI_assert(!src->is_a_single_elem());

  const int width = BLI_rcti_size_x(&area);
  const int height = BLI_rcti_size_y(&area);
  const int row_bytes = this->get_num_channels() * width * sizeof(float);
  for (int y = 0; y < height; y++) {
    float *to_row = this->get_elem(to_x, to_y + y);
    const float *from_row = src->get_elem(area.xmin, area.ymin + y);
    memcpy(to_row, from_row, row_bytes);
  }
}

void MemoryBuffer::copy_elems_from(const MemoryBuffer *src,
                                   const rcti &area,
                                   const int channel_offset,
                                   const int elem_size,
                                   const int to_x,
                                   const int to_y,
                                   const int to_channel_offset)
{
  ASSERT_BUFFER_CONTAINS_AREA(src, area);
  ASSERT_BUFFER_CONTAINS_AREA_AT_COORDS(this, area, to_x, to_y);
  ASSERT_VALID_ELEM_SIZE(this, to_channel_offset, elem_size);
  ASSERT_VALID_ELEM_SIZE(src, channel_offset, elem_size);

  const int width = BLI_rcti_size_x(&area);
  const int height = BLI_rcti_size_y(&area);
  const int elem_bytes = elem_size * sizeof(float);
  for (int y = 0; y < height; y++) {
    float *to_elem = &this->get_value(to_x, to_y + y, to_channel_offset);
    const float *from_elem = &src->get_value(area.xmin, area.ymin + y, channel_offset);
    const float *row_end = to_elem + width * this->elem_stride;
    while (to_elem < row_end) {
      memcpy(to_elem, from_elem, elem_bytes);
      to_elem += this->elem_stride;
      from_elem += src->elem_stride;
    }
  }
}

}  // namespace blender::compositor
