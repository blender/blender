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

#include "COM_Debug.h"
#include "COM_ExecutionGroup.h"
#include "COM_FullFrameExecutionModel.h"
#include "COM_NodeOperation.h"
#include "COM_NodeOperationBuilder.h"
#include "COM_TiledExecutionModel.h"
#include "COM_WorkPackage.h"
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
                                 const ColorManagedViewSettings *view_settings,
                                 const ColorManagedDisplaySettings *display_settings,
                                 const char *view_name)
{
  num_work_threads_ = WorkScheduler::get_num_cpu_threads();
  context_.set_view_name(view_name);
  context_.set_scene(scene);
  context_.set_bnodetree(editingtree);
  context_.set_preview_hash(editingtree->previews);
  context_.set_fast_calculation(fastcalculation);
  /* initialize the CompositorContext */
  if (rendering) {
    context_.set_quality((eCompositorQuality)editingtree->render_quality);
  }
  else {
    context_.set_quality((eCompositorQuality)editingtree->edit_quality);
  }
  context_.set_rendering(rendering);
  context_.setHasActiveOpenCLDevices(WorkScheduler::has_gpu_devices() &&
                                     (editingtree->flag & NTREE_COM_OPENCL));

  context_.set_render_data(rd);
  context_.set_view_settings(view_settings);
  context_.set_display_settings(display_settings);

  BLI_mutex_init(&work_mutex_);
  BLI_condition_init(&work_finished_cond_);

  {
    NodeOperationBuilder builder(&context_, editingtree, this);
    builder.convert_to_operations(this);
  }

  switch (context_.get_execution_model()) {
    case eExecutionModel::Tiled:
      execution_model_ = new TiledExecutionModel(context_, operations_, groups_);
      break;
    case eExecutionModel::FullFrame:
      execution_model_ = new FullFrameExecutionModel(context_, active_buffers_, operations_);
      break;
    default:
      BLI_assert_msg(0, "Non implemented execution model");
      break;
  }
}

ExecutionSystem::~ExecutionSystem()
{
  BLI_condition_end(&work_finished_cond_);
  BLI_mutex_end(&work_mutex_);

  delete execution_model_;

  for (NodeOperation *operation : operations_) {
    delete operation;
  }
  operations_.clear();

  for (ExecutionGroup *group : groups_) {
    delete group;
  }
  groups_.clear();
}

void ExecutionSystem::set_operations(const Vector<NodeOperation *> &operations,
                                     const Vector<ExecutionGroup *> &groups)
{
  operations_ = operations;
  groups_ = groups;
}

void ExecutionSystem::execute()
{
  DebugInfo::execute_started(this);
  for (NodeOperation *op : operations_) {
    op->init_data();
  }
  execution_model_->execute(*this);
}

/**
 * Multi-threadedly execute given work function passing work_rect splits as argument.
 */
void ExecutionSystem::execute_work(const rcti &work_rect,
                                   std::function<void(const rcti &split_rect)> work_func)
{
  if (is_breaked()) {
    return;
  }

  /* Split work vertically to maximize continuous memory. */
  const int work_height = BLI_rcti_size_y(&work_rect);
  const int num_sub_works = MIN2(num_work_threads_, work_height);
  const int split_height = num_sub_works == 0 ? 0 : work_height / num_sub_works;
  int remaining_height = work_height - split_height * num_sub_works;

  Vector<WorkPackage> sub_works(num_sub_works);
  int sub_work_y = work_rect.ymin;
  int num_sub_works_finished = 0;
  for (int i = 0; i < num_sub_works; i++) {
    int sub_work_height = split_height;

    /* Distribute remaining height between sub-works. */
    if (remaining_height > 0) {
      sub_work_height++;
      remaining_height--;
    }

    WorkPackage &sub_work = sub_works[i];
    sub_work.type = eWorkPackageType::CustomFunction;
    sub_work.execute_fn = [=, &work_func, &work_rect]() {
      if (is_breaked()) {
        return;
      }
      rcti split_rect;
      BLI_rcti_init(
          &split_rect, work_rect.xmin, work_rect.xmax, sub_work_y, sub_work_y + sub_work_height);
      work_func(split_rect);
    };
    sub_work.executed_fn = [&]() {
      BLI_mutex_lock(&work_mutex_);
      num_sub_works_finished++;
      if (num_sub_works_finished == num_sub_works) {
        BLI_condition_notify_one(&work_finished_cond_);
      }
      BLI_mutex_unlock(&work_mutex_);
    };
    WorkScheduler::schedule(&sub_work);
    sub_work_y += sub_work_height;
  }
  BLI_assert(sub_work_y == work_rect.ymax);

  WorkScheduler::finish();

  /* Ensure all sub-works finished.
   * TODO: This a workaround for WorkScheduler::finish() not waiting all works on queue threading
   * model. Sync code should be removed once it's fixed. */
  BLI_mutex_lock(&work_mutex_);
  if (num_sub_works_finished < num_sub_works) {
    BLI_condition_wait(&work_finished_cond_, &work_mutex_);
  }
  BLI_mutex_unlock(&work_mutex_);
}

bool ExecutionSystem::is_breaked() const
{
  const bNodeTree *btree = context_.get_bnodetree();
  return btree->test_break(btree->tbh);
}

}  // namespace blender::compositor
