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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 *
 * \brief Extraction of Mesh data into VBO to feed to GPU.
 */
#include "MEM_guardedalloc.h"

#include <optional>

#include "atomic_ops.h"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BLI_array.hh"
#include "BLI_math_bits.h"
#include "BLI_task.h"
#include "BLI_vector.hh"

#include "BKE_editmesh.h"

#include "GPU_capabilities.h"

#include "draw_cache_extract.h"
#include "draw_cache_extract_mesh_private.h"
#include "draw_cache_inline.h"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "PIL_time_utildefines.h"
#endif

#define CHUNK_SIZE 8192

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Mesh Elements Extract Struct
 * \{ */
using TaskId = int;
using TaskLen = int;

struct ExtractorRunData {
  /* Extractor where this run data belongs to. */
  const MeshExtract *extractor;
  /* During iteration the VBO/IBO that is being build. */
  void *buffer = nullptr;
  /* User data during iteration. Created in MeshExtract.init and passed along to other MeshExtract
   * functions. */
  void *user_data = nullptr;
  std::optional<Array<void *>> task_user_datas;

  ExtractorRunData(const MeshExtract *extractor) : extractor(extractor)
  {
  }

  void init_task_user_datas(const TaskLen task_len)
  {
    task_user_datas = Array<void *>(task_len);
  }

  void *&operator[](const TaskId task_id)
  {
    BLI_assert(task_user_datas);
    return (*task_user_datas)[task_id];
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DRAW:ExtractorRunData")
#endif
};

class ExtractorRunDatas : public Vector<ExtractorRunData> {
 public:
  void filter_into(ExtractorRunDatas &result, eMRIterType iter_type) const
  {
    for (const ExtractorRunData &data : *this) {
      const MeshExtract *extractor = data.extractor;
      if ((iter_type & MR_ITER_LOOPTRI) && extractor->iter_looptri_bm) {
        BLI_assert(extractor->iter_looptri_mesh);
        result.append(data);
        continue;
      }
      if ((iter_type & MR_ITER_POLY) && extractor->iter_poly_bm) {
        BLI_assert(extractor->iter_poly_mesh);
        result.append(data);
        continue;
      }
      if ((iter_type & MR_ITER_LEDGE) && extractor->iter_ledge_bm) {
        BLI_assert(extractor->iter_ledge_mesh);
        result.append(data);
        continue;
      }
      if ((iter_type & MR_ITER_LVERT) && extractor->iter_lvert_bm) {
        BLI_assert(extractor->iter_lvert_mesh);
        result.append(data);
        continue;
      }
    }
  }

  void filter_threaded_extractors_into(ExtractorRunDatas &result)
  {
    for (const ExtractorRunData &data : *this) {
      const MeshExtract *extractor = data.extractor;
      if (extractor->use_threading) {
        result.append(extractor);
      }
    }
  }

  eMRIterType iter_types() const
  {
    eMRIterType iter_type = static_cast<eMRIterType>(0);

    for (const ExtractorRunData &data : *this) {
      const MeshExtract *extractor = data.extractor;
      iter_type |= mesh_extract_iter_type(extractor);
    }
    return iter_type;
  }

  const uint iter_types_len() const
  {
    const eMRIterType iter_type = iter_types();
    uint bits = static_cast<uint>(iter_type);
    return count_bits_i(bits);
  }

  eMRDataType data_types()
  {
    eMRDataType data_type = static_cast<eMRDataType>(0);
    for (const ExtractorRunData &data : *this) {
      const MeshExtract *extractor = data.extractor;
      data_type |= extractor->data_type;
    }
    return data_type;
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DRAW:ExtractorRunDatas")
#endif
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract
 * \{ */

BLI_INLINE void extract_init(const MeshRenderData *mr,
                             struct MeshBatchCache *cache,
                             ExtractorRunDatas &extractors,
                             MeshBufferCache *mbc)
{
  /* Multi thread. */
  for (ExtractorRunData &run_data : extractors) {
    const MeshExtract *extractor = run_data.extractor;
    run_data.buffer = mesh_extract_buffer_get(extractor, mbc);
    run_data.user_data = extractor->init(mr, cache, run_data.buffer);
  }
}

BLI_INLINE void extract_iter_looptri_bm(const MeshRenderData *mr,
                                        const ExtractTriBMesh_Params *params,
                                        const ExtractorRunDatas &all_extractors,
                                        const TaskId task_id)
{
  ExtractorRunDatas extractors;
  all_extractors.filter_into(extractors, MR_ITER_LOOPTRI);

  EXTRACT_TRIS_LOOPTRI_FOREACH_BM_BEGIN(elt, elt_index, params)
  {
    for (ExtractorRunData &run_data : extractors) {
      run_data.extractor->iter_looptri_bm(mr, elt, elt_index, run_data[task_id]);
    }
  }
  EXTRACT_TRIS_LOOPTRI_FOREACH_BM_END;
}

BLI_INLINE void extract_iter_looptri_mesh(const MeshRenderData *mr,
                                          const ExtractTriMesh_Params *params,
                                          const ExtractorRunDatas &all_extractors,
                                          const TaskId task_id)
{
  ExtractorRunDatas extractors;
  all_extractors.filter_into(extractors, MR_ITER_LOOPTRI);

  EXTRACT_TRIS_LOOPTRI_FOREACH_MESH_BEGIN(mlt, mlt_index, params)
  {
    for (ExtractorRunData &run_data : extractors) {
      run_data.extractor->iter_looptri_mesh(mr, mlt, mlt_index, run_data[task_id]);
    }
  }
  EXTRACT_TRIS_LOOPTRI_FOREACH_MESH_END;
}

BLI_INLINE void extract_iter_poly_bm(const MeshRenderData *mr,
                                     const ExtractPolyBMesh_Params *params,
                                     const ExtractorRunDatas &all_extractors,
                                     const TaskId task_id)
{
  ExtractorRunDatas extractors;
  all_extractors.filter_into(extractors, MR_ITER_POLY);

  EXTRACT_POLY_FOREACH_BM_BEGIN(f, f_index, params, mr)
  {
    for (ExtractorRunData &run_data : extractors) {
      run_data.extractor->iter_poly_bm(mr, f, f_index, run_data[task_id]);
    }
  }
  EXTRACT_POLY_FOREACH_BM_END;
}

BLI_INLINE void extract_iter_poly_mesh(const MeshRenderData *mr,
                                       const ExtractPolyMesh_Params *params,
                                       const ExtractorRunDatas &all_extractors,
                                       const TaskId task_id)
{
  ExtractorRunDatas extractors;
  all_extractors.filter_into(extractors, MR_ITER_POLY);

  EXTRACT_POLY_FOREACH_MESH_BEGIN(mp, mp_index, params, mr)
  {
    for (ExtractorRunData &run_data : extractors) {
      run_data.extractor->iter_poly_mesh(mr, mp, mp_index, run_data[task_id]);
    }
  }
  EXTRACT_POLY_FOREACH_MESH_END;
}

BLI_INLINE void extract_iter_ledge_bm(const MeshRenderData *mr,
                                      const ExtractLEdgeBMesh_Params *params,
                                      const ExtractorRunDatas &all_extractors,
                                      const TaskId task_id)
{
  ExtractorRunDatas extractors;
  all_extractors.filter_into(extractors, MR_ITER_LEDGE);

  EXTRACT_LEDGE_FOREACH_BM_BEGIN(eed, ledge_index, params)
  {
    for (ExtractorRunData &run_data : extractors) {
      run_data.extractor->iter_ledge_bm(mr, eed, ledge_index, run_data[task_id]);
    }
  }
  EXTRACT_LEDGE_FOREACH_BM_END;
}

BLI_INLINE void extract_iter_ledge_mesh(const MeshRenderData *mr,
                                        const ExtractLEdgeMesh_Params *params,
                                        const ExtractorRunDatas &all_extractors,
                                        const TaskId task_id)
{
  ExtractorRunDatas extractors;
  all_extractors.filter_into(extractors, MR_ITER_LEDGE);

  EXTRACT_LEDGE_FOREACH_MESH_BEGIN(med, ledge_index, params, mr)
  {
    for (ExtractorRunData &run_data : extractors) {
      run_data.extractor->iter_ledge_mesh(mr, med, ledge_index, run_data[task_id]);
    }
  }
  EXTRACT_LEDGE_FOREACH_MESH_END;
}

BLI_INLINE void extract_iter_lvert_bm(const MeshRenderData *mr,
                                      const ExtractLVertBMesh_Params *params,
                                      const ExtractorRunDatas &all_extractors,
                                      const TaskId task_id)
{
  ExtractorRunDatas extractors;
  all_extractors.filter_into(extractors, MR_ITER_LVERT);

  EXTRACT_LVERT_FOREACH_BM_BEGIN(eve, lvert_index, params)
  {
    for (ExtractorRunData &run_data : extractors) {
      run_data.extractor->iter_lvert_bm(mr, eve, lvert_index, run_data[task_id]);
    }
  }
  EXTRACT_LVERT_FOREACH_BM_END;
}

BLI_INLINE void extract_iter_lvert_mesh(const MeshRenderData *mr,
                                        const ExtractLVertMesh_Params *params,
                                        const ExtractorRunDatas &all_extractors,
                                        const TaskId task_id)
{
  ExtractorRunDatas extractors;
  all_extractors.filter_into(extractors, MR_ITER_LVERT);

  EXTRACT_LVERT_FOREACH_MESH_BEGIN(mv, lvert_index, params, mr)
  {
    for (ExtractorRunData &run_data : extractors) {
      run_data.extractor->iter_lvert_mesh(mr, mv, lvert_index, run_data[task_id]);
    }
  }
  EXTRACT_LVERT_FOREACH_MESH_END;
}

BLI_INLINE void extract_finish(const MeshRenderData *mr,
                               struct MeshBatchCache *cache,
                               const ExtractorRunDatas &extractors)
{
  for (const ExtractorRunData &run_data : extractors) {
    const MeshExtract *extractor = run_data.extractor;
    if (extractor->finish) {
      extractor->finish(mr, cache, run_data.buffer, run_data.user_data);
    }
  }
}

BLI_INLINE void extract_task_init(ExtractorRunDatas &extractors, const TaskLen task_len)
{
  for (ExtractorRunData &run_data : extractors) {
    run_data.init_task_user_datas(task_len);
    const MeshExtract *extractor = run_data.extractor;
    for (TaskId task_id = 0; task_id < task_len; task_id++) {
      void *user_task_data = run_data.user_data;
      if (extractor->task_init) {
        user_task_data = extractor->task_init(run_data.user_data);
      }
      run_data[task_id] = user_task_data;
    }
  }
}

BLI_INLINE void extract_task_finish(ExtractorRunDatas &extractors, const TaskLen task_len)
{
  for (ExtractorRunData &run_data : extractors) {
    const MeshExtract *extractor = run_data.extractor;
    if (extractor->task_finish) {
      for (TaskId task_id = 0; task_id < task_len; task_id++) {
        void *task_user_data = run_data[task_id];
        extractor->task_finish(run_data.user_data, task_user_data);
        run_data[task_id] = nullptr;
      }
    }
  }
}

/* Single Thread. */
BLI_INLINE void extract_run_single_threaded(const MeshRenderData *mr,
                                            struct MeshBatchCache *cache,
                                            ExtractorRunDatas &extractors,
                                            eMRIterType iter_type,
                                            MeshBufferCache *mbc)
{
  const TaskLen task_len = 1;
  const TaskId task_id = 0;

  extract_init(mr, cache, extractors, mbc);
  extract_task_init(extractors, task_len);

  bool is_mesh = mr->extract_type != MR_EXTRACT_BMESH;
  if (iter_type & MR_ITER_LOOPTRI) {
    if (is_mesh) {
      ExtractTriMesh_Params params;
      params.mlooptri = mr->mlooptri;
      params.tri_range[0] = 0;
      params.tri_range[1] = mr->tri_len;
      extract_iter_looptri_mesh(mr, &params, extractors, task_id);
    }
    else {
      ExtractTriBMesh_Params params;
      params.looptris = mr->edit_bmesh->looptris;
      params.tri_range[0] = 0;
      params.tri_range[1] = mr->tri_len;
      extract_iter_looptri_bm(mr, &params, extractors, task_id);
    }
  }
  if (iter_type & MR_ITER_POLY) {
    if (is_mesh) {
      ExtractPolyMesh_Params params;
      params.poly_range[0] = 0;
      params.poly_range[1] = mr->poly_len;
      extract_iter_poly_mesh(mr, &params, extractors, task_id);
    }
    else {
      ExtractPolyBMesh_Params params;
      params.poly_range[0] = 0;
      params.poly_range[1] = mr->poly_len;
      extract_iter_poly_bm(mr, &params, extractors, task_id);
    }
  }
  if (iter_type & MR_ITER_LEDGE) {
    if (is_mesh) {
      ExtractLEdgeMesh_Params params;
      params.ledge = mr->ledges;
      params.ledge_range[0] = 0;
      params.ledge_range[1] = mr->edge_loose_len;
      extract_iter_ledge_mesh(mr, &params, extractors, task_id);
    }
    else {
      ExtractLEdgeBMesh_Params params;
      params.ledge = mr->ledges;
      params.ledge_range[0] = 0;
      params.ledge_range[1] = mr->edge_loose_len;
      extract_iter_ledge_bm(mr, &params, extractors, task_id);
    }
  }
  if (iter_type & MR_ITER_LVERT) {
    if (is_mesh) {
      ExtractLVertMesh_Params params;
      params.lvert = mr->lverts;
      params.lvert_range[0] = 0;
      params.lvert_range[1] = mr->vert_loose_len;
      extract_iter_lvert_mesh(mr, &params, extractors, task_id);
    }
    else {
      ExtractLVertBMesh_Params params;
      params.lvert = mr->lverts;
      params.lvert_range[0] = 0;
      params.lvert_range[1] = mr->vert_loose_len;
      extract_iter_lvert_bm(mr, &params, extractors, task_id);
    }
  }
  extract_task_finish(extractors, task_len);
  extract_finish(mr, cache, extractors);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name ExtractTaskData
 * \{ */
struct ExtractTaskData {
  const MeshRenderData *mr = nullptr;
  MeshBatchCache *cache = nullptr;
  /* #UserData is shared between the iterations as it holds counters to detect if the
   * extraction is finished. To make sure the duplication of the user_data does not create a new
   * instance of the counters we allocate the user_data in its own container.
   *
   * This structure makes sure that when extract_init is called, that the user data of all
   * iterations are updated. */

  ExtractorRunDatas *extractors = nullptr;
  MeshBufferCache *mbc = nullptr;
  int32_t *task_counter = nullptr;

  /* Total number of tasks that are created for multi threaded extraction.
   * (= 1 for single threaded extractors). */
  uint task_len;
  /* Task id of the extraction task. Must never exceed task_len. (= 0 for single threaded
   * extractors). */
  uint task_id = 0;

  eMRIterType iter_type;
  int start = 0;
  int end = INT_MAX;
  /** Decremented each time a task is finished. */

  ExtractTaskData(const MeshRenderData *mr,
                  struct MeshBatchCache *cache,
                  ExtractorRunDatas *extractors,
                  MeshBufferCache *mbc,
                  int32_t *task_counter,
                  const uint task_len)
      : mr(mr),
        cache(cache),
        extractors(extractors),
        mbc(mbc),
        task_counter(task_counter),
        task_len(task_len)
  {
    iter_type = extractors->iter_types();
  };

  ExtractTaskData(const ExtractTaskData &src) = default;

  ~ExtractTaskData()
  {
    delete extractors;
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DRW:ExtractTaskData")
#endif
};

static void extract_task_data_free(void *data)
{
  ExtractTaskData *task_data = static_cast<ExtractTaskData *>(data);
  delete task_data;
}

static void extract_task_data_free_ex(void *data)
{
  ExtractTaskData *task_data = static_cast<ExtractTaskData *>(data);
  task_data->extractors = nullptr;
  delete task_data;
}

BLI_INLINE void mesh_extract_iter(const MeshRenderData *mr,
                                  const eMRIterType iter_type,
                                  int start,
                                  int end,
                                  ExtractorRunDatas &extractors,
                                  const TaskId task_id)
{
  switch (mr->extract_type) {
    case MR_EXTRACT_BMESH:
      if (iter_type & MR_ITER_LOOPTRI) {
        ExtractTriBMesh_Params params;
        params.looptris = mr->edit_bmesh->looptris;
        params.tri_range[0] = start;
        params.tri_range[1] = min_ii(mr->tri_len, end);
        extract_iter_looptri_bm(mr, &params, extractors, task_id);
      }
      if (iter_type & MR_ITER_POLY) {
        ExtractPolyBMesh_Params params;
        params.poly_range[0] = start;
        params.poly_range[1] = min_ii(mr->poly_len, end);
        extract_iter_poly_bm(mr, &params, extractors, task_id);
      }
      if (iter_type & MR_ITER_LEDGE) {
        ExtractLEdgeBMesh_Params params;
        params.ledge = mr->ledges;
        params.ledge_range[0] = start;
        params.ledge_range[1] = min_ii(mr->edge_loose_len, end);
        extract_iter_ledge_bm(mr, &params, extractors, task_id);
      }
      if (iter_type & MR_ITER_LVERT) {
        ExtractLVertBMesh_Params params;
        params.lvert = mr->lverts;
        params.lvert_range[0] = start;
        params.lvert_range[1] = min_ii(mr->vert_loose_len, end);
        extract_iter_lvert_bm(mr, &params, extractors, task_id);
      }
      break;
    case MR_EXTRACT_MAPPED:
    case MR_EXTRACT_MESH:
      if (iter_type & MR_ITER_LOOPTRI) {
        ExtractTriMesh_Params params;
        params.mlooptri = mr->mlooptri;
        params.tri_range[0] = start;
        params.tri_range[1] = min_ii(mr->tri_len, end);
        extract_iter_looptri_mesh(mr, &params, extractors, task_id);
      }
      if (iter_type & MR_ITER_POLY) {
        ExtractPolyMesh_Params params;
        params.poly_range[0] = start;
        params.poly_range[1] = min_ii(mr->poly_len, end);
        extract_iter_poly_mesh(mr, &params, extractors, task_id);
      }
      if (iter_type & MR_ITER_LEDGE) {
        ExtractLEdgeMesh_Params params;
        params.ledge = mr->ledges;
        params.ledge_range[0] = start;
        params.ledge_range[1] = min_ii(mr->edge_loose_len, end);
        extract_iter_ledge_mesh(mr, &params, extractors, task_id);
      }
      if (iter_type & MR_ITER_LVERT) {
        ExtractLVertMesh_Params params;
        params.lvert = mr->lverts;
        params.lvert_range[0] = start;
        params.lvert_range[1] = min_ii(mr->vert_loose_len, end);
        extract_iter_lvert_mesh(mr, &params, extractors, task_id);
      }
      break;
  }
}

static void extract_task_init(ExtractTaskData *data)
{
  extract_init(data->mr, data->cache, *data->extractors, data->mbc);
  extract_task_init(*data->extractors, data->task_len);
}

static void extract_task_run(void *__restrict taskdata)
{
  ExtractTaskData *data = (ExtractTaskData *)taskdata;
  mesh_extract_iter(
      data->mr, data->iter_type, data->start, data->end, *data->extractors, data->task_id);

  /* If this is the last task, we do the finish function. */
  int remainin_tasks = atomic_sub_and_fetch_int32(data->task_counter, 1);
  if (remainin_tasks == 0) {
    extract_task_finish(*data->extractors, data->task_len);
    extract_finish(data->mr, data->cache, *data->extractors);
  }
}

static void extract_task_init_and_run(void *__restrict taskdata)
{
  ExtractTaskData *data = (ExtractTaskData *)taskdata;
  extract_run_single_threaded(
      data->mr, data->cache, *data->extractors, data->iter_type, data->mbc);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Task Node - Update Mesh Render Data
 * \{ */
struct MeshRenderDataUpdateTaskData {
  MeshRenderData *mr = nullptr;
  eMRIterType iter_type;
  eMRDataType data_flag;

  MeshRenderDataUpdateTaskData(MeshRenderData *mr, eMRIterType iter_type, eMRDataType data_flag)
      : mr(mr), iter_type(iter_type), data_flag(data_flag)
  {
  }

  ~MeshRenderDataUpdateTaskData()
  {
    mesh_render_data_free(mr);
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DRW:MeshRenderDataUpdateTaskData")
#endif
};

static void mesh_render_data_update_task_data_free(void *data)
{
  MeshRenderDataUpdateTaskData *taskdata = static_cast<MeshRenderDataUpdateTaskData *>(data);
  BLI_assert(taskdata);
  delete taskdata;
}

static void mesh_extract_render_data_node_exec(void *__restrict task_data)
{
  MeshRenderDataUpdateTaskData *update_task_data = static_cast<MeshRenderDataUpdateTaskData *>(
      task_data);
  MeshRenderData *mr = update_task_data->mr;
  const eMRIterType iter_type = update_task_data->iter_type;
  const eMRDataType data_flag = update_task_data->data_flag;

  mesh_render_data_update_normals(mr, data_flag);
  mesh_render_data_update_looptris(mr, iter_type, data_flag);
}

static struct TaskNode *mesh_extract_render_data_node_create(struct TaskGraph *task_graph,
                                                             MeshRenderData *mr,
                                                             const eMRIterType iter_type,
                                                             const eMRDataType data_flag)
{
  MeshRenderDataUpdateTaskData *task_data = new MeshRenderDataUpdateTaskData(
      mr, iter_type, data_flag);

  struct TaskNode *task_node = BLI_task_graph_node_create(
      task_graph,
      mesh_extract_render_data_node_exec,
      task_data,
      (TaskGraphNodeFreeFunction)mesh_render_data_update_task_data_free);
  return task_node;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Task Node - Extract Single Threaded
 * \{ */

static struct TaskNode *extract_single_threaded_task_node_create(struct TaskGraph *task_graph,
                                                                 ExtractTaskData *task_data)
{
  struct TaskNode *task_node = BLI_task_graph_node_create(
      task_graph,
      extract_task_init_and_run,
      task_data,
      (TaskGraphNodeFreeFunction)extract_task_data_free);
  return task_node;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Task Node - UserData Initializer
 * \{ */
struct UserDataInitTaskData {
  ExtractTaskData *td = nullptr;
  int32_t task_counter = 0;

  ~UserDataInitTaskData()
  {
    extract_task_data_free(td);
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DRW:UserDataInitTaskData")
#endif
};

static void user_data_init_task_data_free(void *data)
{
  UserDataInitTaskData *taskdata = static_cast<UserDataInitTaskData *>(data);
  delete taskdata;
}

static void user_data_init_task_data_exec(void *__restrict task_data)
{
  UserDataInitTaskData *extract_task_data = static_cast<UserDataInitTaskData *>(task_data);
  ExtractTaskData *taskdata_base = extract_task_data->td;
  extract_task_init(taskdata_base);
}

static struct TaskNode *user_data_init_task_node_create(struct TaskGraph *task_graph,
                                                        UserDataInitTaskData *task_data)
{
  struct TaskNode *task_node = BLI_task_graph_node_create(
      task_graph,
      user_data_init_task_data_exec,
      task_data,
      (TaskGraphNodeFreeFunction)user_data_init_task_data_free);
  return task_node;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Loop
 * \{ */

static void extract_range_task_create(struct TaskGraph *task_graph,
                                      struct TaskNode *task_node_user_data_init,
                                      ExtractTaskData *taskdata,
                                      const eMRIterType type,
                                      int start,
                                      int length)
{
  taskdata = new ExtractTaskData(*taskdata);
  taskdata->task_id = atomic_fetch_and_add_int32(taskdata->task_counter, 1);
  BLI_assert(taskdata->task_id < taskdata->task_len);
  taskdata->iter_type = type;
  taskdata->start = start;
  taskdata->end = start + length;
  struct TaskNode *task_node = BLI_task_graph_node_create(
      task_graph, extract_task_run, taskdata, extract_task_data_free_ex);
  BLI_task_graph_edge_create(task_node_user_data_init, task_node);
}

static int extract_range_task_num_elements_get(const MeshRenderData *mr,
                                               const eMRIterType iter_type)
{
  /* Divide task into sensible chunks. */
  int iter_len = 0;
  if (iter_type & MR_ITER_LOOPTRI) {
    iter_len += mr->tri_len;
  }
  if (iter_type & MR_ITER_POLY) {
    iter_len += mr->poly_len;
  }
  if (iter_type & MR_ITER_LEDGE) {
    iter_len += mr->edge_loose_len;
  }
  if (iter_type & MR_ITER_LVERT) {
    iter_len += mr->vert_loose_len;
  }
  return iter_len;
}

static int extract_range_task_chunk_size_get(const MeshRenderData *mr,
                                             const eMRIterType iter_type,
                                             const int num_threads)
{
  /* Divide task into sensible chunks. */
  const int num_elements = extract_range_task_num_elements_get(mr, iter_type);
  int range_len = (num_elements + num_threads) / num_threads;
  CLAMP_MIN(range_len, CHUNK_SIZE);
  return range_len;
}

static void extract_task_in_ranges_create(struct TaskGraph *task_graph,
                                          struct TaskNode *task_node_user_data_init,
                                          ExtractTaskData *taskdata_base,
                                          const int num_threads)
{
  const MeshRenderData *mr = taskdata_base->mr;
  const int range_len = extract_range_task_chunk_size_get(
      mr, taskdata_base->iter_type, num_threads);

  if (taskdata_base->iter_type & MR_ITER_LOOPTRI) {
    for (int i = 0; i < mr->tri_len; i += range_len) {
      extract_range_task_create(
          task_graph, task_node_user_data_init, taskdata_base, MR_ITER_LOOPTRI, i, range_len);
    }
  }
  if (taskdata_base->iter_type & MR_ITER_POLY) {
    for (int i = 0; i < mr->poly_len; i += range_len) {
      extract_range_task_create(
          task_graph, task_node_user_data_init, taskdata_base, MR_ITER_POLY, i, range_len);
    }
  }
  if (taskdata_base->iter_type & MR_ITER_LEDGE) {
    for (int i = 0; i < mr->edge_loose_len; i += range_len) {
      extract_range_task_create(
          task_graph, task_node_user_data_init, taskdata_base, MR_ITER_LEDGE, i, range_len);
    }
  }
  if (taskdata_base->iter_type & MR_ITER_LVERT) {
    for (int i = 0; i < mr->vert_loose_len; i += range_len) {
      extract_range_task_create(
          task_graph, task_node_user_data_init, taskdata_base, MR_ITER_LVERT, i, range_len);
    }
  }
}

static void mesh_buffer_cache_create_requested(struct TaskGraph *task_graph,
                                               MeshBatchCache *cache,
                                               MeshBufferCache *mbc,
                                               MeshBufferExtractionCache *extraction_cache,
                                               Mesh *me,

                                               const bool is_editmode,
                                               const bool is_paint_mode,
                                               const bool is_mode_active,
                                               const float obmat[4][4],
                                               const bool do_final,
                                               const bool do_uvedit,
                                               const bool use_subsurf_fdots,
                                               const Scene *scene,
                                               const ToolSettings *ts,
                                               const bool use_hide)
{
  /* For each mesh where batches needs to be updated a sub-graph will be added to the task_graph.
   * This sub-graph starts with an extract_render_data_node. This fills/converts the required
   * data from Mesh.
   *
   * Small extractions and extractions that can't be multi-threaded are grouped in a single
   * `extract_single_threaded_task_node`.
   *
   * Other extractions will create a node for each loop exceeding 8192 items. these nodes are
   * linked to the `user_data_init_task_node`. the `user_data_init_task_node` prepares the
   * user_data needed for the extraction based on the data extracted from the mesh.
   * counters are used to check if the finalize of a task has to be called.
   *
   *                           Mesh extraction sub graph
   *
   *                                                       +----------------------+
   *                                               +-----> | extract_task1_loop_1 |
   *                                               |       +----------------------+
   * +------------------+     +----------------------+     +----------------------+
   * | mesh_render_data | --> |                      | --> | extract_task1_loop_2 |
   * +------------------+     |                      |     +----------------------+
   *   |                      |                      |     +----------------------+
   *   |                      |    user_data_init    | --> | extract_task2_loop_1 |
   *   v                      |                      |     +----------------------+
   * +------------------+     |                      |     +----------------------+
   * | single_threaded  |     |                      | --> | extract_task2_loop_2 |
   * +------------------+     +----------------------+     +----------------------+
   *                                               |       +----------------------+
   *                                               +-----> | extract_task2_loop_3 |
   *                                                       +----------------------+
   */
  const bool do_hq_normals = (scene->r.perf_flag & SCE_PERF_HQ_NORMALS) != 0 ||
                             GPU_use_hq_normals_workaround();

  /* Create an array containing all the extractors that needs to be executed. */
  ExtractorRunDatas extractors;

#define EXTRACT_ADD_REQUESTED(type, type_lowercase, name) \
  do { \
    if (DRW_##type_lowercase##_requested(mbc->type_lowercase.name)) { \
      const MeshExtract *extractor = mesh_extract_override_get(&extract_##name, do_hq_normals); \
      extractors.append(extractor); \
    } \
  } while (0)

  EXTRACT_ADD_REQUESTED(VBO, vbo, pos_nor);
  EXTRACT_ADD_REQUESTED(VBO, vbo, lnor);
  EXTRACT_ADD_REQUESTED(VBO, vbo, uv);
  EXTRACT_ADD_REQUESTED(VBO, vbo, tan);
  EXTRACT_ADD_REQUESTED(VBO, vbo, vcol);
  EXTRACT_ADD_REQUESTED(VBO, vbo, sculpt_data);
  EXTRACT_ADD_REQUESTED(VBO, vbo, orco);
  EXTRACT_ADD_REQUESTED(VBO, vbo, edge_fac);
  EXTRACT_ADD_REQUESTED(VBO, vbo, weights);
  EXTRACT_ADD_REQUESTED(VBO, vbo, edit_data);
  EXTRACT_ADD_REQUESTED(VBO, vbo, edituv_data);
  EXTRACT_ADD_REQUESTED(VBO, vbo, edituv_stretch_area);
  EXTRACT_ADD_REQUESTED(VBO, vbo, edituv_stretch_angle);
  EXTRACT_ADD_REQUESTED(VBO, vbo, mesh_analysis);
  EXTRACT_ADD_REQUESTED(VBO, vbo, fdots_pos);
  EXTRACT_ADD_REQUESTED(VBO, vbo, fdots_nor);
  EXTRACT_ADD_REQUESTED(VBO, vbo, fdots_uv);
  EXTRACT_ADD_REQUESTED(VBO, vbo, fdots_edituv_data);
  EXTRACT_ADD_REQUESTED(VBO, vbo, poly_idx);
  EXTRACT_ADD_REQUESTED(VBO, vbo, edge_idx);
  EXTRACT_ADD_REQUESTED(VBO, vbo, vert_idx);
  EXTRACT_ADD_REQUESTED(VBO, vbo, fdot_idx);
  EXTRACT_ADD_REQUESTED(VBO, vbo, skin_roots);

  EXTRACT_ADD_REQUESTED(IBO, ibo, tris);
  if (DRW_ibo_requested(mbc->ibo.lines)) {
    const MeshExtract *extractor;
    if (mbc->ibo.lines_loose != nullptr) {
      /* Update #lines_loose ibo. */
      extractor = &extract_lines_with_lines_loose;
    }
    else {
      extractor = &extract_lines;
    }
    extractors.append(extractor);
  }
  else if (DRW_ibo_requested(mbc->ibo.lines_loose)) {
    /* Note: #ibo.lines must have been created first. */
    const MeshExtract *extractor = &extract_lines_loose_only;
    extractors.append(extractor);
  }
  EXTRACT_ADD_REQUESTED(IBO, ibo, points);
  EXTRACT_ADD_REQUESTED(IBO, ibo, fdots);
  EXTRACT_ADD_REQUESTED(IBO, ibo, lines_paint_mask);
  EXTRACT_ADD_REQUESTED(IBO, ibo, lines_adjacency);
  EXTRACT_ADD_REQUESTED(IBO, ibo, edituv_tris);
  EXTRACT_ADD_REQUESTED(IBO, ibo, edituv_lines);
  EXTRACT_ADD_REQUESTED(IBO, ibo, edituv_points);
  EXTRACT_ADD_REQUESTED(IBO, ibo, edituv_fdots);

#undef EXTRACT_ADD_REQUESTED

  if (extractors.is_empty()) {
    return;
  }

#ifdef DEBUG_TIME
  double rdata_start = PIL_check_seconds_timer();
#endif

  eMRIterType iter_type = extractors.iter_types();
  eMRDataType data_flag = extractors.data_types();

  MeshRenderData *mr = mesh_render_data_create(me,
                                               extraction_cache,
                                               is_editmode,
                                               is_paint_mode,
                                               is_mode_active,
                                               obmat,
                                               do_final,
                                               do_uvedit,
                                               ts,
                                               iter_type);
  mr->use_hide = use_hide;
  mr->use_subsurf_fdots = use_subsurf_fdots;
  mr->use_final_mesh = do_final;

#ifdef DEBUG_TIME
  double rdata_end = PIL_check_seconds_timer();
#endif

  struct TaskNode *task_node_mesh_render_data = mesh_extract_render_data_node_create(
      task_graph, mr, iter_type, data_flag);

  /* Simple heuristic. */
  const bool use_thread = (mr->loop_len + mr->loop_loose_len) > CHUNK_SIZE;

  if (use_thread) {
    uint single_threaded_extractors_len = 0;

    /* First run the requested extractors that do not support asynchronous ranges. */
    for (const ExtractorRunData &run_data : extractors) {
      const MeshExtract *extractor = run_data.extractor;
      if (!extractor->use_threading) {
        ExtractorRunDatas *single_threaded_extractors = new ExtractorRunDatas();
        single_threaded_extractors->append(extractor);
        ExtractTaskData *taskdata = new ExtractTaskData(
            mr, cache, single_threaded_extractors, mbc, nullptr, 1);
        struct TaskNode *task_node = extract_single_threaded_task_node_create(task_graph,
                                                                              taskdata);
        BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
        single_threaded_extractors_len++;
      }
    }

    /* Distribute the remaining extractors into ranges per core. */
    ExtractorRunDatas *multi_threaded_extractors = new ExtractorRunDatas();
    extractors.filter_threaded_extractors_into(*multi_threaded_extractors);
    if (!multi_threaded_extractors->is_empty()) {
      /*
       * Determine the number of thread to use for multithreading.
       * Thread can be used for single threaded tasks. These typically take longer to execute so
       * fill the rest of the threads for range operations.
       */
      int num_threads = BLI_task_scheduler_num_threads();
      num_threads -= single_threaded_extractors_len % num_threads;
      const int max_multithreaded_task_len = multi_threaded_extractors->iter_types_len() +
                                             num_threads;

      UserDataInitTaskData *user_data_init_task_data = new UserDataInitTaskData();
      struct TaskNode *task_node_user_data_init = user_data_init_task_node_create(
          task_graph, user_data_init_task_data);

      user_data_init_task_data->td = new ExtractTaskData(mr,
                                                         cache,
                                                         multi_threaded_extractors,
                                                         mbc,
                                                         &user_data_init_task_data->task_counter,
                                                         max_multithreaded_task_len);

      extract_task_in_ranges_create(
          task_graph, task_node_user_data_init, user_data_init_task_data->td, num_threads);

      BLI_task_graph_edge_create(task_node_mesh_render_data, task_node_user_data_init);
    }
    else {
      /* No tasks created freeing extractors list. */
      delete multi_threaded_extractors;
    }
  }
  else {
    /* Run all requests on the same thread. */
    ExtractorRunDatas *extractors_copy = new ExtractorRunDatas(extractors);
    ExtractTaskData *taskdata = new ExtractTaskData(mr, cache, extractors_copy, mbc, nullptr, 1);

    struct TaskNode *task_node = extract_single_threaded_task_node_create(task_graph, taskdata);
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }

  /* Trigger the sub-graph for this mesh. */
  BLI_task_graph_node_push_work(task_node_mesh_render_data);

#ifdef DEBUG_TIME
  BLI_task_graph_work_and_wait(task_graph);
  double end = PIL_check_seconds_timer();

  static double avg = 0;
  static double avg_fps = 0;
  static double avg_rdata = 0;
  static double end_prev = 0;

  if (end_prev == 0) {
    end_prev = end;
  }

  avg = avg * 0.95 + (end - rdata_end) * 0.05;
  avg_fps = avg_fps * 0.95 + (end - end_prev) * 0.05;
  avg_rdata = avg_rdata * 0.95 + (rdata_end - rdata_start) * 0.05;

  printf(
      "rdata %.0fms iter %.0fms (frame %.0fms)\n", avg_rdata * 1000, avg * 1000, avg_fps * 1000);

  end_prev = end;
#endif
}

}  // namespace blender::draw

extern "C" {
void mesh_buffer_cache_create_requested(struct TaskGraph *task_graph,
                                        MeshBatchCache *cache,
                                        MeshBufferCache *mbc,
                                        MeshBufferExtractionCache *extraction_cache,
                                        Mesh *me,

                                        const bool is_editmode,
                                        const bool is_paint_mode,
                                        const bool is_mode_active,
                                        const float obmat[4][4],
                                        const bool do_final,
                                        const bool do_uvedit,
                                        const bool use_subsurf_fdots,
                                        const Scene *scene,
                                        const ToolSettings *ts,
                                        const bool use_hide)
{
  blender::draw::mesh_buffer_cache_create_requested(task_graph,
                                                    cache,
                                                    mbc,
                                                    extraction_cache,
                                                    me,
                                                    is_editmode,
                                                    is_paint_mode,
                                                    is_mode_active,
                                                    obmat,
                                                    do_final,
                                                    do_uvedit,
                                                    use_subsurf_fdots,
                                                    scene,
                                                    ts,
                                                    use_hide);
}

}  // extern "C"

/** \} */
