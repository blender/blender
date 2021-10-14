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
  const bNodeTree *node_tree = context.get_bnodetree();
  node_tree->stats_draw(node_tree->sdh, TIP_("Compositing | Determining resolution"));

  unsigned int resolution[2];
  for (ExecutionGroup *group : groups_) {
    resolution[0] = 0;
    resolution[1] = 0;
    group->determine_resolution(resolution);

    if (border_.use_render_border) {
      const rctf *render_border = border_.render_border;
      group->set_render_border(
          render_border->xmin, render_border->xmax, render_border->ymin, render_border->ymax);
    }

    if (border_.use_viewer_border) {
      const rctf *viewer_border = border_.viewer_border;
      group->set_viewer_border(
          viewer_border->xmin, viewer_border->xmax, viewer_border->ymin, viewer_border->ymax);
    }
  }
}

static void update_read_buffer_offset(Span<NodeOperation *> operations)
{
  unsigned int order = 0;
  for (NodeOperation *operation : operations) {
    if (operation->get_flags().is_read_buffer_operation) {
      ReadBufferOperation *read_operation = (ReadBufferOperation *)operation;
      read_operation->set_offset(order);
      order++;
    }
  }
}

static void init_write_operations_for_execution(Span<NodeOperation *> operations,
                                                const bNodeTree *bTree)
{
  for (NodeOperation *operation : operations) {
    if (operation->get_flags().is_write_buffer_operation) {
      operation->set_bnodetree(bTree);
      operation->init_execution();
    }
  }
}

static void link_write_buffers(Span<NodeOperation *> operations)
{
  for (NodeOperation *operation : operations) {
    if (operation->get_flags().is_read_buffer_operation) {
      ReadBufferOperation *read_operation = static_cast<ReadBufferOperation *>(operation);
      read_operation->update_memory_buffer();
    }
  }
}

static void init_non_write_operations_for_execution(Span<NodeOperation *> operations,
                                                    const bNodeTree *bTree)
{
  for (NodeOperation *operation : operations) {
    if (!operation->get_flags().is_write_buffer_operation) {
      operation->set_bnodetree(bTree);
      operation->init_execution();
    }
  }
}

static void init_execution_groups_for_execution(Span<ExecutionGroup *> groups,
                                                const int chunk_size)
{
  for (ExecutionGroup *execution_group : groups) {
    execution_group->set_chunksize(chunk_size);
    execution_group->init_execution();
  }
}

void TiledExecutionModel::execute(ExecutionSystem &exec_system)
{
  const bNodeTree *editingtree = this->context_.get_bnodetree();

  editingtree->stats_draw(editingtree->sdh, TIP_("Compositing | Initializing execution"));

  update_read_buffer_offset(operations_);

  init_write_operations_for_execution(operations_, context_.get_bnodetree());
  link_write_buffers(operations_);
  init_non_write_operations_for_execution(operations_, context_.get_bnodetree());
  init_execution_groups_for_execution(groups_, context_.get_chunksize());

  WorkScheduler::start(context_);
  execute_groups(eCompositorPriority::High, exec_system);
  if (!context_.is_fast_calculation()) {
    execute_groups(eCompositorPriority::Medium, exec_system);
    execute_groups(eCompositorPriority::Low, exec_system);
  }
  WorkScheduler::finish();
  WorkScheduler::stop();

  editingtree->stats_draw(editingtree->sdh, TIP_("Compositing | De-initializing execution"));

  for (NodeOperation *operation : operations_) {
    operation->deinit_execution();
  }

  for (ExecutionGroup *execution_group : groups_) {
    execution_group->deinit_execution();
  }
}

void TiledExecutionModel::execute_groups(eCompositorPriority priority,
                                         ExecutionSystem &exec_system)
{
  for (ExecutionGroup *execution_group : groups_) {
    if (execution_group->get_flags().is_output &&
        execution_group->get_render_priority() == priority) {
      execution_group->execute(&exec_system);
    }
  }
}

}  // namespace blender::compositor
