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

#include "BLI_span.hh"

#include "DNA_vec_types.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

class CompositorContext;
class ExecutionSystem;
class NodeOperation;

/**
 * Base class for execution models. Contains shared implementation.
 */
class ExecutionModel {
 protected:
  /**
   * Render and viewer border info. Coordinates are normalized.
   */
  struct {
    bool use_render_border;
    const rctf *render_border;
    bool use_viewer_border;
    const rctf *viewer_border;
  } border_;

  /**
   * Context used during execution.
   */
  CompositorContext &context_;

  /**
   * All operations being executed.
   */
  Span<NodeOperation *> operations_;

 public:
  ExecutionModel(CompositorContext &context, Span<NodeOperation *> operations);

  virtual ~ExecutionModel()
  {
  }

  virtual void execute(ExecutionSystem &exec_system) = 0;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:BaseExecutionModel")
#endif
};

}  // namespace blender::compositor
