#pragma once

#include "BLI_task_c.hh"

#include "bmesh_class.hh"

namespace blender {

/**
 * \brief Parallel (threaded) iterator,
 * only available for most basic iteration-types (verts/edges/faces of mesh).
 *
 * Uses #BLI_task_parallel_mempool to iterate over all items of underlying matching mempool.
 */
ATTR_NONNULL(1)
void BM_iter_parallel(BMesh *bm,
                      const char itype,
                      TaskParallelMempoolFunc func,
                      void *userdata,
                      const TaskParallelSettings *settings);

}  // namespace blender
