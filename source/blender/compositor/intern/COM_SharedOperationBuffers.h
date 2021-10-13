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

#pragma once

#include "BLI_map.hh"
#include "BLI_vector.hh"

#include "DNA_vec_types.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

class MemoryBuffer;
class NodeOperation;

/**
 * Stores and shares operations rendered buffers including render data. Buffers are
 * disposed once all dependent operations have finished reading them.
 */
class SharedOperationBuffers {
 private:
  typedef struct BufferData {
   public:
    BufferData();
    std::unique_ptr<MemoryBuffer> buffer;
    blender::Vector<rcti> render_areas;
    int registered_reads;
    int received_reads;
    bool is_rendered;
  } BufferData;
  blender::Map<NodeOperation *, BufferData> buffers_;

 public:
  bool is_area_registered(NodeOperation *op, const rcti &area_to_render);
  void register_area(NodeOperation *op, const rcti &area_to_render);

  bool has_registered_reads(NodeOperation *op);
  void register_read(NodeOperation *read_op);

  Vector<rcti> get_areas_to_render(NodeOperation *op, int offset_x, int offset_y);
  bool is_operation_rendered(NodeOperation *op);
  void set_rendered_buffer(NodeOperation *op, std::unique_ptr<MemoryBuffer> buffer);
  MemoryBuffer *get_rendered_buffer(NodeOperation *op);

  void read_finished(NodeOperation *read_op);

 private:
  BufferData &get_buffer_data(NodeOperation *op);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:SharedOperationBuffers")
#endif
};

}  // namespace blender::compositor
