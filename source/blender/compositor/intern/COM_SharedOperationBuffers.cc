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
 * Copyright 2021, Blender Foundation.
 */

#include "COM_SharedOperationBuffers.h"
#include "COM_NodeOperation.h"

namespace blender::compositor {

SharedOperationBuffers::BufferData::BufferData()
    : buffer(nullptr), registered_reads(0), received_reads(0), is_rendered(false)
{
}

SharedOperationBuffers::BufferData &SharedOperationBuffers::get_buffer_data(NodeOperation *op)
{
  return buffers_.lookup_or_add_cb(op, []() { return BufferData(); });
}

/**
 * Whether given operation area to render is already registered.
 * TODO: Possibly refactor to "request_area". Current implementation is incomplete: partial
 * overlapping, etc. Leading to more rendering than necessary.
 */
bool SharedOperationBuffers::is_area_registered(NodeOperation *op, const rcti &area_to_render)
{
  BufferData &buf_data = get_buffer_data(op);
  for (rcti &reg_rect : buf_data.render_areas) {
    if (BLI_rcti_inside_rcti(&reg_rect, &area_to_render)) {
      return true;
    }
  }
  return false;
}

/**
 * Registers an operation area to render.
 */
void SharedOperationBuffers::register_area(NodeOperation *op, const rcti &area_to_render)
{
  get_buffer_data(op).render_areas.append(area_to_render);
}

/**
 * Whether given operation has any registered reads (other operation registered it depends on given
 * operation).
 */
bool SharedOperationBuffers::has_registered_reads(NodeOperation *op)
{
  return get_buffer_data(op).registered_reads > 0;
}

/**
 * Registers an operation read (other operation depends on given operation).
 */
void SharedOperationBuffers::register_read(NodeOperation *read_op)
{
  get_buffer_data(read_op).registered_reads++;
}

/**
 * Get registered areas given operation needs to render.
 */
Vector<rcti> SharedOperationBuffers::get_areas_to_render(NodeOperation *op,
                                                         const int offset_x,
                                                         const int offset_y)
{
  Span<rcti> render_areas = get_buffer_data(op).render_areas.as_span();
  Vector<rcti> dst_areas;
  for (rcti dst : render_areas) {
    BLI_rcti_translate(&dst, offset_x, offset_y);
    dst_areas.append(std::move(dst));
  }
  return dst_areas;
}

/**
 * Whether this operation buffer has already been rendered.
 */
bool SharedOperationBuffers::is_operation_rendered(NodeOperation *op)
{
  return get_buffer_data(op).is_rendered;
}

/**
 * Stores given operation rendered buffer.
 */
void SharedOperationBuffers::set_rendered_buffer(NodeOperation *op,
                                                 std::unique_ptr<MemoryBuffer> buffer)
{
  BufferData &buf_data = get_buffer_data(op);
  BLI_assert(buf_data.received_reads == 0);
  BLI_assert(buf_data.buffer == nullptr);
  buf_data.buffer = std::move(buffer);
  buf_data.is_rendered = true;
}

/**
 * Get given operation rendered buffer.
 */
MemoryBuffer *SharedOperationBuffers::get_rendered_buffer(NodeOperation *op)
{
  BLI_assert(is_operation_rendered(op));
  return get_buffer_data(op).buffer.get();
}

/**
 * Reports an operation has finished reading given operation. If all given operation dependencies
 * have finished its buffer will be disposed.
 */
void SharedOperationBuffers::read_finished(NodeOperation *read_op)
{
  BufferData &buf_data = get_buffer_data(read_op);
  buf_data.received_reads++;
  BLI_assert(buf_data.received_reads > 0 && buf_data.received_reads <= buf_data.registered_reads);
  if (buf_data.received_reads == buf_data.registered_reads) {
    /* Dispose buffer. */
    buf_data.buffer = nullptr;
  }
}

}  // namespace blender::compositor
