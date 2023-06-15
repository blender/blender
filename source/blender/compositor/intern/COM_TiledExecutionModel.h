/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Enums.h"
#include "COM_ExecutionModel.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

class ExecutionGroup;

/**
 * Operations are executed from outputs to inputs grouped in execution groups and rendered in
 * tiles.
 */
class TiledExecutionModel : public ExecutionModel {
 private:
  Span<ExecutionGroup *> groups_;

 public:
  TiledExecutionModel(CompositorContext &context,
                      Span<NodeOperation *> operations,
                      Span<ExecutionGroup *> groups);

  void execute(ExecutionSystem &exec_system) override;

 private:
  void execute_groups(eCompositorPriority priority, ExecutionSystem &exec_system);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:TiledExecutionModel")
#endif
};

}  // namespace blender::compositor
