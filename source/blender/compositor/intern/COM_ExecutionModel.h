/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

  virtual ~ExecutionModel() {}

  virtual void execute(ExecutionSystem &exec_system) = 0;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:BaseExecutionModel")
#endif
};

}  // namespace blender::compositor
