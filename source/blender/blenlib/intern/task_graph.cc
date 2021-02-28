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
 */

/** \file
 * \ingroup bli
 *
 * Task graph.
 */

#include "MEM_guardedalloc.h"

#include "BLI_task.h"

#include <memory>
#include <vector>

#ifdef WITH_TBB
#  include <tbb/flow_graph.h>
#endif

/* Task Graph */
struct TaskGraph {
#ifdef WITH_TBB
  tbb::flow::graph tbb_graph;
#endif
  std::vector<std::unique_ptr<TaskNode>> nodes;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("task_graph:TaskGraph")
#endif
};

/* TaskNode - a node in the task graph. */
struct TaskNode {
  /* TBB Node. */
#ifdef WITH_TBB
  tbb::flow::continue_node<tbb::flow::continue_msg> tbb_node;
#endif
  /* Successors to execute after this task, for serial execution fallback. */
  std::vector<TaskNode *> successors;

  /* User function to be executed with given task data. */
  TaskGraphNodeRunFunction run_func;
  void *task_data;
  /* Optional callback to free task data along with the graph. If task data
   * is shared between nodes, only a single task node should free the data. */
  TaskGraphNodeFreeFunction free_func;

  TaskNode(TaskGraph *task_graph,
           TaskGraphNodeRunFunction run_func,
           void *task_data,
           TaskGraphNodeFreeFunction free_func)
      :
#ifdef WITH_TBB
        tbb_node(task_graph->tbb_graph,
                 tbb::flow::unlimited,
                 [&](const tbb::flow::continue_msg input) { run(input); }),
#endif
        run_func(run_func),
        task_data(task_data),
        free_func(free_func)
  {
#ifndef WITH_TBB
    UNUSED_VARS(task_graph);
#endif
  }

  TaskNode(const TaskNode &other) = delete;
  TaskNode &operator=(const TaskNode &other) = delete;

  ~TaskNode()
  {
    if (task_data && free_func) {
      free_func(task_data);
    }
  }

#ifdef WITH_TBB
  tbb::flow::continue_msg run(const tbb::flow::continue_msg UNUSED(input))
  {
    tbb::this_task_arena::isolate([this] { run_func(task_data); });
    return tbb::flow::continue_msg();
  }
#endif

  void run_serial()
  {
    run_func(task_data);
    for (TaskNode *successor : successors) {
      successor->run_serial();
    }
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("task_graph:TaskNode")
#endif
};

TaskGraph *BLI_task_graph_create(void)
{
  return new TaskGraph();
}

void BLI_task_graph_free(TaskGraph *task_graph)
{
  delete task_graph;
}

void BLI_task_graph_work_and_wait(TaskGraph *task_graph)
{
#ifdef WITH_TBB
  task_graph->tbb_graph.wait_for_all();
#else
  UNUSED_VARS(task_graph);
#endif
}

struct TaskNode *BLI_task_graph_node_create(struct TaskGraph *task_graph,
                                            TaskGraphNodeRunFunction run,
                                            void *user_data,
                                            TaskGraphNodeFreeFunction free_func)
{
  TaskNode *task_node = new TaskNode(task_graph, run, user_data, free_func);
  task_graph->nodes.push_back(std::unique_ptr<TaskNode>(task_node));
  return task_node;
}

bool BLI_task_graph_node_push_work(struct TaskNode *task_node)
{
#ifdef WITH_TBB
  if (BLI_task_scheduler_num_threads() > 1) {
    return task_node->tbb_node.try_put(tbb::flow::continue_msg());
  }
#endif

  task_node->run_serial();
  return true;
}

void BLI_task_graph_edge_create(struct TaskNode *from_node, struct TaskNode *to_node)
{
#ifdef WITH_TBB
  if (BLI_task_scheduler_num_threads() > 1) {
    tbb::flow::make_edge(from_node->tbb_node, to_node->tbb_node);
    return;
  }
#endif

  from_node->successors.push_back(to_node);
}
