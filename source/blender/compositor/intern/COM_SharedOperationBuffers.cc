/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

bool SharedOperationBuffers::is_area_registered(NodeOperation *op, const rcti &area_to_render)
{
  /* TODO: Possibly refactor to "request_area". Current implementation is incomplete:
   * partial overlapping, etc. Leading to more rendering than necessary. */

  BufferData &buf_data = get_buffer_data(op);
  for (rcti &reg_rect : buf_data.render_areas) {
    if (BLI_rcti_inside_rcti(&reg_rect, &area_to_render)) {
      return true;
    }
  }
  return false;
}

void SharedOperationBuffers::register_area(NodeOperation *op, const rcti &area_to_render)
{
  get_buffer_data(op).render_areas.append(area_to_render);
}

bool SharedOperationBuffers::has_registered_reads(NodeOperation *op)
{
  return get_buffer_data(op).registered_reads > 0;
}

void SharedOperationBuffers::register_read(NodeOperation *read_op)
{
  get_buffer_data(read_op).registered_reads++;
}

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

bool SharedOperationBuffers::is_operation_rendered(NodeOperation *op)
{
  return get_buffer_data(op).is_rendered;
}

void SharedOperationBuffers::set_rendered_buffer(NodeOperation *op,
                                                 std::unique_ptr<MemoryBuffer> buffer)
{
  BufferData &buf_data = get_buffer_data(op);
  BLI_assert(buf_data.received_reads == 0);
  BLI_assert(buf_data.buffer == nullptr);
  buf_data.buffer = std::move(buffer);
  buf_data.is_rendered = true;
}

MemoryBuffer *SharedOperationBuffers::get_rendered_buffer(NodeOperation *op)
{
  BLI_assert(is_operation_rendered(op));
  return get_buffer_data(op).buffer.get();
}

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
