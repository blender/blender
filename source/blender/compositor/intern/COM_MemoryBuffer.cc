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

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"
#include "MEM_guardedalloc.h"

#define ASSERT_BUFFER_CONTAINS_AREA(buf, area) \
  BLI_assert(BLI_rcti_inside_rcti(&(buf)->get_rect(), &(area)))

#define ASSERT_BUFFER_CONTAINS_AREA_AT_COORDS(buf, area, x, y) \
  BLI_assert((buf)->get_rect().xmin <= (x)); \
  BLI_assert((buf)->get_rect().ymin <= (y)); \
  BLI_assert((buf)->get_rect().xmax >= (x) + BLI_rcti_size_x(&(area))); \
  BLI_assert((buf)->get_rect().ymax >= (y) + BLI_rcti_size_y(&(area)))

#define ASSERT_VALID_ELEM_SIZE(buf, channel_offset, elem_size) \
  BLI_assert((buf)->get_num_channels() <= (channel_offset) + (elem_size))

namespace blender::compositor {

static rcti create_rect(const int width, const int height)
{
  rcti rect;
  BLI_rcti_init(&rect, 0, width, 0, height);
  return rect;
}

MemoryBuffer::MemoryBuffer(MemoryProxy *memoryProxy, const rcti &rect, MemoryBufferState state)
{
  m_rect = rect;
  this->m_is_a_single_elem = false;
  this->m_memoryProxy = memoryProxy;
  this->m_num_channels = COM_data_type_num_channels(memoryProxy->getDataType());
  this->m_buffer = (float *)MEM_mallocN_aligned(
      sizeof(float) * buffer_len() * this->m_num_channels, 16, "COM_MemoryBuffer");
  owns_data_ = true;
  this->m_state = state;
  this->m_datatype = memoryProxy->getDataType();

  set_strides();
}

MemoryBuffer::MemoryBuffer(DataType dataType, const rcti &rect, bool is_a_single_elem)
{
  m_rect = rect;
  this->m_is_a_single_elem = is_a_single_elem;
  this->m_memoryProxy = nullptr;
  this->m_num_channels = COM_data_type_num_channels(dataType);
  this->m_buffer = (float *)MEM_mallocN_aligned(
      sizeof(float) * buffer_len() * this->m_num_channels, 16, "COM_MemoryBuffer");
  owns_data_ = true;
  this->m_state = MemoryBufferState::Temporary;
  this->m_datatype = dataType;

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
  m_rect = rect;
  m_is_a_single_elem = is_a_single_elem;
  m_memoryProxy = nullptr;
  m_num_channels = num_channels;
  m_datatype = COM_num_channels_data_type(num_channels);
  m_buffer = buffer;
  owns_data_ = false;
  m_state = MemoryBufferState::Temporary;

  set_strides();
}

MemoryBuffer::MemoryBuffer(const MemoryBuffer &src)
    : MemoryBuffer(src.m_datatype, src.m_rect, false)
{
  m_memoryProxy = src.m_memoryProxy;
  /* src may be single elem buffer */
  fill_from(src);
}

void MemoryBuffer::set_strides()
{
  if (m_is_a_single_elem) {
    this->elem_stride = 0;
    this->row_stride = 0;
  }
  else {
    this->elem_stride = m_num_channels;
    this->row_stride = getWidth() * m_num_channels;
  }
}

void MemoryBuffer::clear()
{
  memset(m_buffer, 0, buffer_len() * m_num_channels * sizeof(float));
}

/**
 * Converts a single elem buffer to a full size buffer (allocates memory for all
 * elements in resolution).
 */
MemoryBuffer *MemoryBuffer::inflate() const
{
  BLI_assert(is_a_single_elem());
  MemoryBuffer *inflated = new MemoryBuffer(this->m_datatype, this->m_rect, false);
  inflated->copy_from(this, this->m_rect);
  return inflated;
}

float MemoryBuffer::get_max_value() const
{
  float result = this->m_buffer[0];
  const unsigned int size = this->buffer_len();
  unsigned int i;

  const float *fp_src = this->m_buffer;

  for (i = 0; i < size; i++, fp_src += this->m_num_channels) {
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
  BLI_rcti_isect(&rect, &this->m_rect, &rect_clamp);

  if (!BLI_rcti_is_empty(&rect_clamp)) {
    MemoryBuffer temp_buffer(this->m_datatype, rect_clamp);
    temp_buffer.fill_from(*this);
    return temp_buffer.get_max_value();
  }

  BLI_assert(0);
  return 0.0f;
}

MemoryBuffer::~MemoryBuffer()
{
  if (this->m_buffer && owns_data_) {
    MEM_freeN(this->m_buffer);
    this->m_buffer = nullptr;
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
  copy_from(src, area, 0, this->get_num_channels(), this->get_num_channels(), 0);
}

void MemoryBuffer::copy_from(const uchar *src,
                             const rcti &area,
                             const int channel_offset,
                             const int elem_size,
                             const int elem_stride,
                             const int to_channel_offset)
{
  copy_from(
      src, area, channel_offset, elem_size, elem_stride, area.xmin, area.ymin, to_channel_offset);
}

void MemoryBuffer::copy_from(const uchar *src,
                             const rcti &area,
                             const int channel_offset,
                             const int elem_size,
                             const int elem_stride,
                             const int to_x,
                             const int to_y,
                             const int to_channel_offset)
{
  ASSERT_BUFFER_CONTAINS_AREA_AT_COORDS(this, area, to_x, to_y);
  ASSERT_VALID_ELEM_SIZE(this, to_channel_offset, elem_size);

  const int width = BLI_rcti_size_x(&area);
  const int height = BLI_rcti_size_y(&area);
  const int src_row_stride = width * elem_stride;
  const uchar *const src_start = src + area.ymin * src_row_stride + channel_offset;
  for (int y = 0; y < height; y++) {
    const uchar *from_elem = src_start + y * src_row_stride;
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
  if (buf->getWidth() == width) {
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
    copy_from(uc_buf, area, channel_offset, elem_size, elem_stride, to_x, to_y, to_channel_offset);
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
  overlap.xmin = MAX2(this->m_rect.xmin, src.m_rect.xmin);
  overlap.xmax = MIN2(this->m_rect.xmax, src.m_rect.xmax);
  overlap.ymin = MAX2(this->m_rect.ymin, src.m_rect.ymin);
  overlap.ymax = MIN2(this->m_rect.ymax, src.m_rect.ymax);
  copy_from(&src, overlap);
}

void MemoryBuffer::writePixel(int x, int y, const float color[4])
{
  if (x >= this->m_rect.xmin && x < this->m_rect.xmax && y >= this->m_rect.ymin &&
      y < this->m_rect.ymax) {
    const int offset = get_coords_offset(x, y);
    memcpy(&this->m_buffer[offset], color, sizeof(float) * this->m_num_channels);
  }
}

void MemoryBuffer::addPixel(int x, int y, const float color[4])
{
  if (x >= this->m_rect.xmin && x < this->m_rect.xmax && y >= this->m_rect.ymin &&
      y < this->m_rect.ymax) {
    const int offset = get_coords_offset(x, y);
    float *dst = &this->m_buffer[offset];
    const float *src = color;
    for (int i = 0; i < this->m_num_channels; i++, dst++, src++) {
      *dst += *src;
    }
  }
}

static void read_ewa_pixel_sampled(void *userdata, int x, int y, float result[4])
{
  MemoryBuffer *buffer = (MemoryBuffer *)userdata;
  buffer->read(result, x, y);
}

void MemoryBuffer::readEWA(float *result, const float uv[2], const float derivatives[2][2])
{
  if (m_is_a_single_elem) {
    memcpy(result, m_buffer, sizeof(float) * this->m_num_channels);
  }
  else {
    BLI_assert(this->m_datatype == DataType::Color);
    float inv_width = 1.0f / (float)this->getWidth(), inv_height = 1.0f / (float)this->getHeight();
    /* TODO(sergey): Render pipeline uses normalized coordinates and derivatives,
     * but compositor uses pixel space. For now let's just divide the values and
     * switch compositor to normalized space for EWA later.
     */
    float uv_normal[2] = {uv[0] * inv_width, uv[1] * inv_height};
    float du_normal[2] = {derivatives[0][0] * inv_width, derivatives[0][1] * inv_height};
    float dv_normal[2] = {derivatives[1][0] * inv_width, derivatives[1][1] * inv_height};

    BLI_ewa_filter(this->getWidth(),
                   this->getHeight(),
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
