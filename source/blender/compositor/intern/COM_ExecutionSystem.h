/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <functional>

#include "atomic_ops.h"

#include "BLI_index_range.hh"
#include "BLI_threads.h"
#include "BLI_vector.hh"

#include "COM_CompositorContext.h"
#include "COM_SharedOperationBuffers.h"

#include "DNA_color_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_vec_types.h"

namespace blender::compositor {

/**
 * \page execution Execution model
 * In order to get to an efficient model for execution, several steps are being done. these steps
 * are explained below.
 *
 * \section EM_Step1 Step 1: translating blender node system to the new compositor system
 * Blenders node structure is based on C structs (DNA). These structs are not efficient in the new
 * architecture. We want to use classes in order to simplify the system. during this step the
 * blender node_tree is evaluated and converted to a CPP node system.
 *
 * \see ExecutionSystem
 * \see Converter.convert
 * \see Node
 *
 * \section EM_Step2 Step2: translating nodes to operations
 * Ungrouping the GroupNodes. Group nodes are node_tree's in node_tree's.
 * The new system only supports a single level of node_tree.
 * We will 'flatten' the system in a single level.
 * \see GroupNode
 * \see ExecutionSystemHelper.ungroup
 *
 * Every node has the ability to convert itself to operations. The node itself is responsible to
 * create a correct NodeOperation setup based on its internal settings. Most Node only need to
 * convert it to its NodeOperation. Like a ColorToBWNode doesn't check anything, but replaces
 * itself with a ConvertColorToBWOperation. More complex nodes can use different NodeOperation
 * based on settings; like MixNode. based on the selected Mixtype a different operation will be
 * used. for more information see the page about creating new Nodes. [@subpage newnode]
 *
 * \see ExecutionSystem.convert_to_operations
 * \see Node.convert_to_operations
 * \see NodeOperation base class for all operations in the system
 *
 * \section EM_Step3 Step3: add additional conversions to the operation system
 *   - Data type conversions: the system has 3 data types DataType::Value, DataType::Vector,
 * DataType::Color. The user can connect a Value socket to a color socket. As values are ordered
 * differently than colors a conversion happens.
 *
 *   - Image size conversions: the system can automatically convert when resolutions do not match.
 *     An NodeInput has a resize mode. This can be any of the following settings.
 *     - [@ref InputSocketResizeMode.ResizeMode::Center]:
 *       The center of both images are aligned
 *     - [@ref InputSocketResizeMode.ResizeMode::FitWidth]:
 *       The width of both images are aligned
 *     - [@ref InputSocketResizeMode.ResizeMode::FitHeight]:
 *       The height of both images are aligned
 *     - [@ref InputSocketResizeMode.ResizeMode::FitAny]:
 *       The width, or the height of both images are aligned to make sure that it fits.
 *     - [@ref InputSocketResizeMode.ResizeMode::Stretch]:
 *       The width and the height of both images are aligned.
 *     - [@ref InputSocketResizeMode.ResizeMode::None]:
 *       Bottom left of the images are aligned.
 *
 * \see COM_convert_data_type Datatype conversions
 * \see Converter.convert_resolution Image size conversions
 *
 * \section EM_Step4 Step4: group operations in executions groups
 * ExecutionGroup are groups of operations that are calculated as being one bigger operation.
 * All operations will be part of an ExecutionGroup.
 * Complex nodes will be added to separate groups. Between ExecutionGroup's the data will be stored
 * in MemoryBuffers. ReadBufferOperations and WriteBufferOperations are added where needed.
 *
 * <pre>
 *
 *        +------------------------------+      +----------------+
 *        | ExecutionGroup A             |      |ExecutionGroup B|   ExecutionGroup
 *        | +----------+     +----------+|      |+----------+    |
 *   /----->| Operation|---->| Operation|-\ /--->| Operation|-\  |   NodeOperation
 *   |    | | A        |     | B        ||| |   || C        | |  |
 *   |    | | cFFA     |  /->| cFFA     ||| |   || cFFA     | |  |
 *   |    | +----------+  |  +----------+|| |   |+----------+ |  |
 *   |    +---------------|--------------+v |   +-------------v--+
 * +-*----+           +---*--+         +--*-*--+           +--*----+
 * |inputA|           |inputB|         |outputA|           |outputB| MemoryBuffer
 * |cFAA  |           |cFAA  |         |cFAA   |           |cFAA   |
 * +------+           +------+         +-------+           +-------+
 * </pre>
 * \see ExecutionSystem.group_operations method doing this step
 * \see ExecutionSystem.add_read_write_buffer_operations
 * \see NodeOperation.is_complex
 * \see ExecutionGroup class representing the ExecutionGroup
 */

/* Forward declarations. */
class ExecutionGroup;
class ExecutionModel;
class NodeOperation;

/**
 * \brief the ExecutionSystem contains the whole compositor tree.
 */
class ExecutionSystem {
 private:
  /**
   * Contains operations active buffers data. Buffers will be disposed once reader operations are
   * finished.
   */
  SharedOperationBuffers active_buffers_;

  /**
   * \brief the context used during execution
   */
  CompositorContext context_;

  /**
   * \brief vector of operations
   */
  Vector<NodeOperation *> operations_;

  /**
   * \brief vector of groups
   */
  Vector<ExecutionGroup *> groups_;

  /**
   * Active execution model implementation.
   */
  ExecutionModel *execution_model_;

  /**
   * Number of cpu threads available for work execution.
   */
  int num_work_threads_;

  ThreadMutex work_mutex_;
  ThreadCondition work_finished_cond_;

 public:
  /**
   * \brief Create a new ExecutionSystem and initialize it with the
   * editingtree.
   *
   * \param editingtree: [bNodeTree *]
   * \param rendering: [true false]
   */
  ExecutionSystem(RenderData *rd,
                  Scene *scene,
                  bNodeTree *editingtree,
                  bool rendering,
                  bool fastcalculation,
                  const char *view_name);

  /**
   * Destructor
   */
  ~ExecutionSystem();

  void set_operations(const Vector<NodeOperation *> &operations,
                      const Vector<ExecutionGroup *> &groups);

  /**
   * \brief execute this system
   * - initialize the NodeOperation's and ExecutionGroup's
   * - schedule the output ExecutionGroup's based on their priority
   * - deinitialize the ExecutionGroup's and NodeOperation's
   */
  void execute();

  /**
   * \brief get the reference to the compositor context
   */
  const CompositorContext &get_context() const
  {
    return context_;
  }

  SharedOperationBuffers &get_active_buffers()
  {
    return active_buffers_;
  }

  /**
   * Multi-threadedly execute given work function passing work_rect splits as argument.
   */
  void execute_work(const rcti &work_rect, std::function<void(const rcti &split_rect)> work_func);

  /**
   * Multi-threaded execution of given work function passing work_rect splits as argument.
   * Once finished, caller thread will call reduce_func for each thread result.
   */
  template<typename TResult>
  void execute_work(const rcti &work_rect,
                    std::function<TResult(const rcti &split_rect)> work_func,
                    TResult &join,
                    std::function<void(TResult &join, const TResult &chunk)> reduce_func)
  {
    Array<TResult> chunks(num_work_threads_);
    int num_started = 0;
    execute_work(work_rect, [&](const rcti &split_rect) {
      const int current = atomic_fetch_and_add_int32(&num_started, 1);
      chunks[current] = work_func(split_rect);
    });
    for (const int i : IndexRange(num_started)) {
      reduce_func(join, chunks[i]);
    }
  }

  bool is_breaked() const;

 private:
  /* allow the DebugInfo class to look at internals */
  friend class DebugInfo;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:ExecutionSystem")
#endif
};

}  // namespace blender::compositor
