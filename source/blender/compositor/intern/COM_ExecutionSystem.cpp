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

#include "COM_ExecutionSystem.h"

#include "BLI_utildefines.h"
#include "PIL_time.h"

#include "BKE_node.h"

#include "BLT_translation.h"

#include "COM_Converter.h"
#include "COM_Debug.h"
#include "COM_ExecutionGroup.h"
#include "COM_NodeOperation.h"
#include "COM_NodeOperationBuilder.h"
#include "COM_ReadBufferOperation.h"
#include "COM_WorkScheduler.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

ExecutionSystem::ExecutionSystem(RenderData *rd,
                                 Scene *scene,
                                 bNodeTree *editingtree,
                                 bool rendering,
                                 bool fastcalculation,
                                 const ColorManagedViewSettings *viewSettings,
                                 const ColorManagedDisplaySettings *displaySettings,
                                 const char *viewName)
{
  this->m_context.setViewName(viewName);
  this->m_context.setScene(scene);
  this->m_context.setbNodeTree(editingtree);
  this->m_context.setPreviewHash(editingtree->previews);
  this->m_context.setFastCalculation(fastcalculation);
  /* initialize the CompositorContext */
  if (rendering) {
    this->m_context.setQuality((CompositorQuality)editingtree->render_quality);
  }
  else {
    this->m_context.setQuality((CompositorQuality)editingtree->edit_quality);
  }
  this->m_context.setRendering(rendering);
  this->m_context.setHasActiveOpenCLDevices(WorkScheduler::has_gpu_devices() &&
                                            (editingtree->flag & NTREE_COM_OPENCL));

  this->m_context.setRenderData(rd);
  this->m_context.setViewSettings(viewSettings);
  this->m_context.setDisplaySettings(displaySettings);

  {
    NodeOperationBuilder builder(&m_context, editingtree);
    builder.convertToOperations(this);
  }

  unsigned int index;
  unsigned int resolution[2];

  rctf *viewer_border = &editingtree->viewer_border;
  bool use_viewer_border = (editingtree->flag & NTREE_VIEWER_BORDER) &&
                           viewer_border->xmin < viewer_border->xmax &&
                           viewer_border->ymin < viewer_border->ymax;

  editingtree->stats_draw(editingtree->sdh, TIP_("Compositing | Determining resolution"));

  for (index = 0; index < this->m_groups.size(); index++) {
    resolution[0] = 0;
    resolution[1] = 0;
    ExecutionGroup *executionGroup = this->m_groups[index];
    executionGroup->determineResolution(resolution);

    if (rendering) {
      /* case when cropping to render border happens is handled in
       * compositor output and render layer nodes
       */
      if ((rd->mode & R_BORDER) && !(rd->mode & R_CROP)) {
        executionGroup->setRenderBorder(
            rd->border.xmin, rd->border.xmax, rd->border.ymin, rd->border.ymax);
      }
    }

    if (use_viewer_border) {
      executionGroup->setViewerBorder(
          viewer_border->xmin, viewer_border->xmax, viewer_border->ymin, viewer_border->ymax);
    }
  }

  //  DebugInfo::graphviz(this);
}

ExecutionSystem::~ExecutionSystem()
{
  unsigned int index;
  for (index = 0; index < this->m_operations.size(); index++) {
    NodeOperation *operation = this->m_operations[index];
    delete operation;
  }
  this->m_operations.clear();
  for (index = 0; index < this->m_groups.size(); index++) {
    ExecutionGroup *group = this->m_groups[index];
    delete group;
  }
  this->m_groups.clear();
}

void ExecutionSystem::set_operations(const blender::Vector<NodeOperation *> &operations,
                                     const blender::Vector<ExecutionGroup *> &groups)
{
  m_operations = operations;
  m_groups = groups;
}

void ExecutionSystem::execute()
{
  const bNodeTree *editingtree = this->m_context.getbNodeTree();
  editingtree->stats_draw(editingtree->sdh, TIP_("Compositing | Initializing execution"));

  DebugInfo::execute_started(this);

  unsigned int order = 0;
  for (NodeOperation *operation : m_operations) {
    if (operation->isReadBufferOperation()) {
      ReadBufferOperation *readOperation = (ReadBufferOperation *)operation;
      readOperation->setOffset(order);
      order++;
    }
  }
  unsigned int index;

  // First allocale all write buffer
  for (index = 0; index < this->m_operations.size(); index++) {
    NodeOperation *operation = this->m_operations[index];
    if (operation->isWriteBufferOperation()) {
      operation->setbNodeTree(this->m_context.getbNodeTree());
      operation->initExecution();
    }
  }
  // Connect read buffers to their write buffers
  for (index = 0; index < this->m_operations.size(); index++) {
    NodeOperation *operation = this->m_operations[index];
    if (operation->isReadBufferOperation()) {
      ReadBufferOperation *readOperation = (ReadBufferOperation *)operation;
      readOperation->updateMemoryBuffer();
    }
  }
  // initialize other operations
  for (index = 0; index < this->m_operations.size(); index++) {
    NodeOperation *operation = this->m_operations[index];
    if (!operation->isWriteBufferOperation()) {
      operation->setbNodeTree(this->m_context.getbNodeTree());
      operation->initExecution();
    }
  }
  for (index = 0; index < this->m_groups.size(); index++) {
    ExecutionGroup *executionGroup = this->m_groups[index];
    executionGroup->setChunksize(this->m_context.getChunksize());
    executionGroup->initExecution();
  }

  WorkScheduler::start(this->m_context);

  execute_groups(COM_PRIORITY_HIGH);
  if (!this->getContext().isFastCalculation()) {
    execute_groups(COM_PRIORITY_MEDIUM);
    execute_groups(COM_PRIORITY_LOW);
  }

  WorkScheduler::finish();
  WorkScheduler::stop();

  editingtree->stats_draw(editingtree->sdh, TIP_("Compositing | De-initializing execution"));
  for (index = 0; index < this->m_operations.size(); index++) {
    NodeOperation *operation = this->m_operations[index];
    operation->deinitExecution();
  }
  for (index = 0; index < this->m_groups.size(); index++) {
    ExecutionGroup *executionGroup = this->m_groups[index];
    executionGroup->deinitExecution();
  }
}

void ExecutionSystem::execute_groups(CompositorPriority priority)
{
  blender::Vector<ExecutionGroup *> execution_groups = find_output_execution_groups(priority);
  for (ExecutionGroup *group : execution_groups) {
    group->execute(this);
  }
}

blender::Vector<ExecutionGroup *> ExecutionSystem::find_output_execution_groups(
    CompositorPriority priority) const
{
  blender::Vector<ExecutionGroup *> result;

  for (ExecutionGroup *group : m_groups) {
    if (group->isOutputExecutionGroup() && group->getRenderPriotrity() == priority) {
      result.append(group);
    }
  }
  return result;
}
