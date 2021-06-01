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

#include "COM_Debug.h"
#include "COM_FullFrameExecutionModel.h"
#include "COM_NodeOperation.h"
#include "COM_NodeOperationBuilder.h"
#include "COM_TiledExecutionModel.h"
#include "COM_WorkScheduler.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

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
    this->m_context.setQuality((eCompositorQuality)editingtree->render_quality);
  }
  else {
    this->m_context.setQuality((eCompositorQuality)editingtree->edit_quality);
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

  switch (m_context.get_execution_model()) {
    case eExecutionModel::Tiled:
      execution_model_ = new TiledExecutionModel(m_context, m_operations, m_groups);
      break;
    case eExecutionModel::FullFrame:
      execution_model_ = new FullFrameExecutionModel(m_context, active_buffers_, m_operations);
      break;
    default:
      BLI_assert(!"Non implemented execution model");
      break;
  }
}

ExecutionSystem::~ExecutionSystem()
{
  delete execution_model_;

  for (NodeOperation *operation : m_operations) {
    delete operation;
  }
  this->m_operations.clear();

  for (ExecutionGroup *group : m_groups) {
    delete group;
  }
  this->m_groups.clear();
}

void ExecutionSystem::set_operations(const Vector<NodeOperation *> &operations,
                                     const Vector<ExecutionGroup *> &groups)
{
  m_operations = operations;
  m_groups = groups;
}

void ExecutionSystem::execute()
{
  DebugInfo::execute_started(this);
  execution_model_->execute(*this);
}

void ExecutionSystem::execute_work(const rcti &work_rect,
                                   std::function<void(const rcti &split_rect)> work_func)
{
  execution_model_->execute_work(work_rect, work_func);
}

}  // namespace blender::compositor
