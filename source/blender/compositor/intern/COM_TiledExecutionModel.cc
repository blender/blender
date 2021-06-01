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

#include "COM_TiledExecutionModel.h"
#include "COM_Debug.h"
#include "COM_ExecutionGroup.h"
#include "COM_ReadBufferOperation.h"
#include "COM_WorkScheduler.h"

#include "BLT_translation.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

TiledExecutionModel::TiledExecutionModel(CompositorContext &context,
                                         Span<NodeOperation *> operations,
                                         Span<ExecutionGroup *> groups)
    : ExecutionModel(context, operations), groups_(groups)
{
  const bNodeTree *node_tree = context.getbNodeTree();
  node_tree->stats_draw(node_tree->sdh, TIP_("Compositing | Determining resolution"));

  unsigned int resolution[2];
  for (ExecutionGroup *group : groups_) {
    resolution[0] = 0;
    resolution[1] = 0;
    group->determineResolution(resolution);

    if (border_.use_render_border) {
      const rctf *render_border = border_.viewer_border;
      group->setRenderBorder(
          render_border->xmin, render_border->xmax, render_border->ymin, render_border->ymax);
    }

    if (border_.use_viewer_border) {
      const rctf *viewer_border = border_.viewer_border;
      group->setViewerBorder(
          viewer_border->xmin, viewer_border->xmax, viewer_border->ymin, viewer_border->ymax);
    }
  }
}

static void update_read_buffer_offset(Span<NodeOperation *> operations)
{
  unsigned int order = 0;
  for (NodeOperation *operation : operations) {
    if (operation->get_flags().is_read_buffer_operation) {
      ReadBufferOperation *readOperation = (ReadBufferOperation *)operation;
      readOperation->setOffset(order);
      order++;
    }
  }
}

static void init_write_operations_for_execution(Span<NodeOperation *> operations,
                                                const bNodeTree *bTree)
{
  for (NodeOperation *operation : operations) {
    if (operation->get_flags().is_write_buffer_operation) {
      operation->setbNodeTree(bTree);
      operation->initExecution();
    }
  }
}

static void link_write_buffers(Span<NodeOperation *> operations)
{
  for (NodeOperation *operation : operations) {
    if (operation->get_flags().is_read_buffer_operation) {
      ReadBufferOperation *readOperation = static_cast<ReadBufferOperation *>(operation);
      readOperation->updateMemoryBuffer();
    }
  }
}

static void init_non_write_operations_for_execution(Span<NodeOperation *> operations,
                                                    const bNodeTree *bTree)
{
  for (NodeOperation *operation : operations) {
    if (!operation->get_flags().is_write_buffer_operation) {
      operation->setbNodeTree(bTree);
      operation->initExecution();
    }
  }
}

static void init_execution_groups_for_execution(Span<ExecutionGroup *> groups,
                                                const int chunk_size)
{
  for (ExecutionGroup *execution_group : groups) {
    execution_group->setChunksize(chunk_size);
    execution_group->initExecution();
  }
}

void TiledExecutionModel::execute(ExecutionSystem &exec_system)
{
  const bNodeTree *editingtree = this->context_.getbNodeTree();

  editingtree->stats_draw(editingtree->sdh, TIP_("Compositing | Initializing execution"));

  update_read_buffer_offset(operations_);

  init_write_operations_for_execution(operations_, context_.getbNodeTree());
  link_write_buffers(operations_);
  init_non_write_operations_for_execution(operations_, context_.getbNodeTree());
  init_execution_groups_for_execution(groups_, context_.getChunksize());

  WorkScheduler::start(context_);
  execute_groups(eCompositorPriority::High, exec_system);
  if (!context_.isFastCalculation()) {
    execute_groups(eCompositorPriority::Medium, exec_system);
    execute_groups(eCompositorPriority::Low, exec_system);
  }
  WorkScheduler::finish();
  WorkScheduler::stop();

  editingtree->stats_draw(editingtree->sdh, TIP_("Compositing | De-initializing execution"));

  for (NodeOperation *operation : operations_) {
    operation->deinitExecution();
  }

  for (ExecutionGroup *execution_group : groups_) {
    execution_group->deinitExecution();
  }
}

void TiledExecutionModel::execute_groups(eCompositorPriority priority,
                                         ExecutionSystem &exec_system)
{
  for (ExecutionGroup *execution_group : groups_) {
    if (execution_group->get_flags().is_output &&
        execution_group->getRenderPriority() == priority) {
      execution_group->execute(&exec_system);
    }
  }
}

}  // namespace blender::compositor
