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
