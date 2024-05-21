/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "BKE_editmesh.hh"
#include "BKE_object.hh"

#include "GPU_capabilities.hh"

#include "draw_cache_extract.hh"
#include "draw_cache_inline.hh"
#include "draw_subdivision.hh"

#include "mesh_extractors/extract_mesh.hh"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_time_utildefines.h"
#endif

namespace blender::draw {

int mesh_render_mat_len_get(const Object &object, const Mesh &mesh)
{
  if (mesh.runtime->edit_mesh != nullptr) {
    const Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(&object);
    if (editmesh_eval_final != nullptr) {
      return std::max<int>(1, editmesh_eval_final->totcol);
    }
  }
  return std::max<int>(1, mesh.totcol);
}

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
  uint32_t data_offset = 0;

  ExtractorRunData(const MeshExtract *extractor) : extractor(extractor) {}

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DRAW:ExtractorRunData")
#endif
};

class ExtractorRunDatas : public Vector<ExtractorRunData> {
 public:
  void filter_into(ExtractorRunDatas &result, eMRIterType iter_type, const bool is_mesh) const
  {
    for (const ExtractorRunData &data : *this) {
      const MeshExtract *extractor = data.extractor;
      if ((iter_type & MR_ITER_CORNER_TRI) && *(&extractor->iter_looptri_bm + is_mesh)) {
        result.append(data);
        continue;
      }
      if ((iter_type & MR_ITER_POLY) && *(&extractor->iter_face_bm + is_mesh)) {
        result.append(data);
        continue;
      }
      if ((iter_type & MR_ITER_LOOSE_EDGE) && *(&extractor->iter_loose_edge_bm + is_mesh)) {
        result.append(data);
        continue;
      }
      if ((iter_type & MR_ITER_LOOSE_VERT) && *(&extractor->iter_loose_vert_bm + is_mesh)) {
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

  uint iter_types_len() const
  {
    const eMRIterType iter_type = iter_types();
    uint bits = uint(iter_type);
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

  size_t data_size_total()
  {
    size_t data_size = 0;
    for (const ExtractorRunData &data : *this) {
      const MeshExtract *extractor = data.extractor;
      data_size += extractor->data_size;
    }
    return data_size;
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DRAW:ExtractorRunDatas")
#endif
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name ExtractTaskData
 * \{ */

struct ExtractTaskData {
  const MeshRenderData *mr = nullptr;
  MeshBatchCache *cache = nullptr;
  ExtractorRunDatas *extractors = nullptr;
  MeshBufferList *buffers = nullptr;

  eMRIterType iter_type;
  bool use_threading = false;

  ExtractTaskData(const MeshRenderData &mr,
                  MeshBatchCache &cache,
                  ExtractorRunDatas *extractors,
                  MeshBufferList *buffers,
                  const bool use_threading)
      : mr(&mr),
        cache(&cache),
        extractors(extractors),
        buffers(buffers),
        use_threading(use_threading)
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

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Init and Finish
 * \{ */

BLI_INLINE void extract_init(const MeshRenderData &mr,
                             MeshBatchCache &cache,
                             ExtractorRunDatas &extractors,
                             MeshBufferList *buffers,
                             void *data_stack)
{
  uint32_t data_offset = 0;
  for (ExtractorRunData &run_data : extractors) {
    const MeshExtract *extractor = run_data.extractor;
    run_data.buffer = mesh_extract_buffer_get(extractor, buffers);
    run_data.data_offset = data_offset;
    extractor->init(mr, cache, run_data.buffer, POINTER_OFFSET(data_stack, data_offset));
    data_offset += uint32_t(extractor->data_size);
  }
}

BLI_INLINE void extract_finish(const MeshRenderData &mr,
                               MeshBatchCache &cache,
                               const ExtractorRunDatas &extractors,
                               void *data_stack)
{
  for (const ExtractorRunData &run_data : extractors) {
    const MeshExtract *extractor = run_data.extractor;
    if (extractor->finish) {
      extractor->finish(
          mr, cache, run_data.buffer, POINTER_OFFSET(data_stack, run_data.data_offset));
    }
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract In Parallel Ranges
 * \{ */

struct ExtractorIterData {
  ExtractorRunDatas extractors;
  const MeshRenderData *mr = nullptr;
  const void *elems = nullptr;
  const int *loose_elems = nullptr;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DRW:MeshRenderDataUpdateTaskData")
#endif
};

static void extract_task_reduce(const void *__restrict userdata,
                                void *__restrict chunk_to,
                                void *__restrict chunk_from)
{
  const ExtractorIterData *data = static_cast<const ExtractorIterData *>(userdata);
  for (const ExtractorRunData &run_data : data->extractors) {
    const MeshExtract *extractor = run_data.extractor;
    if (extractor->task_reduce) {
      extractor->task_reduce(POINTER_OFFSET(chunk_to, run_data.data_offset),
                             POINTER_OFFSET(chunk_from, run_data.data_offset));
    }
  }
}

static void extract_range_iter_looptri_bm(void *__restrict userdata,
                                          const int iter,
                                          const TaskParallelTLS *__restrict tls)
{
  const ExtractorIterData *data = static_cast<ExtractorIterData *>(userdata);
  void *extract_data = tls->userdata_chunk;
  const MeshRenderData &mr = *data->mr;
  BMLoop **elt = ((BMLoop * (*)[3]) data->elems)[iter];
  BLI_assert(iter < mr.edit_bmesh->looptris.size());
  for (const ExtractorRunData &run_data : data->extractors) {
    run_data.extractor->iter_looptri_bm(
        mr, elt, iter, POINTER_OFFSET(extract_data, run_data.data_offset));
  }
}

static void extract_range_iter_corner_tri_mesh(void *__restrict userdata,
                                               const int iter,
                                               const TaskParallelTLS *__restrict tls)
{
  void *extract_data = tls->userdata_chunk;

  const ExtractorIterData *data = static_cast<ExtractorIterData *>(userdata);
  const MeshRenderData &mr = *data->mr;
  const int3 &tri = ((const int3 *)data->elems)[iter];
  for (const ExtractorRunData &run_data : data->extractors) {
    run_data.extractor->iter_corner_tri_mesh(
        mr, tri, iter, POINTER_OFFSET(extract_data, run_data.data_offset));
  }
}

static void extract_range_iter_face_bm(void *__restrict userdata,
                                       const int iter,
                                       const TaskParallelTLS *__restrict tls)
{
  void *extract_data = tls->userdata_chunk;

  const ExtractorIterData *data = static_cast<ExtractorIterData *>(userdata);
  const MeshRenderData &mr = *data->mr;
  const BMFace *f = ((const BMFace **)data->elems)[iter];
  for (const ExtractorRunData &run_data : data->extractors) {
    run_data.extractor->iter_face_bm(
        mr, f, iter, POINTER_OFFSET(extract_data, run_data.data_offset));
  }
}

static void extract_range_iter_face_mesh(void *__restrict userdata,
                                         const int iter,
                                         const TaskParallelTLS *__restrict tls)
{
  void *extract_data = tls->userdata_chunk;

  const ExtractorIterData *data = static_cast<ExtractorIterData *>(userdata);
  const MeshRenderData &mr = *data->mr;
  for (const ExtractorRunData &run_data : data->extractors) {
    run_data.extractor->iter_face_mesh(
        mr, iter, POINTER_OFFSET(extract_data, run_data.data_offset));
  }
}

static void extract_range_iter_loose_edge_bm(void *__restrict userdata,
                                             const int iter,
                                             const TaskParallelTLS *__restrict tls)
{
  void *extract_data = tls->userdata_chunk;

  const ExtractorIterData *data = static_cast<ExtractorIterData *>(userdata);
  const MeshRenderData &mr = *data->mr;
  const int loose_edge_i = data->loose_elems[iter];
  const BMEdge *eed = ((const BMEdge **)data->elems)[loose_edge_i];
  for (const ExtractorRunData &run_data : data->extractors) {
    run_data.extractor->iter_loose_edge_bm(
        mr, eed, iter, POINTER_OFFSET(extract_data, run_data.data_offset));
  }
}

static void extract_range_iter_loose_edge_mesh(void *__restrict userdata,
                                               const int iter,
                                               const TaskParallelTLS *__restrict tls)
{
  void *extract_data = tls->userdata_chunk;

  const ExtractorIterData *data = static_cast<ExtractorIterData *>(userdata);
  const MeshRenderData &mr = *data->mr;
  const int loose_edge_i = data->loose_elems[iter];
  const int2 edge = ((const int2 *)data->elems)[loose_edge_i];
  for (const ExtractorRunData &run_data : data->extractors) {
    run_data.extractor->iter_loose_edge_mesh(
        mr, edge, iter, POINTER_OFFSET(extract_data, run_data.data_offset));
  }
}

static void extract_range_iter_loose_vert_bm(void *__restrict userdata,
                                             const int iter,
                                             const TaskParallelTLS *__restrict tls)
{
  void *extract_data = tls->userdata_chunk;

  const ExtractorIterData *data = static_cast<ExtractorIterData *>(userdata);
  const MeshRenderData &mr = *data->mr;
  const int loose_vert_i = data->loose_elems[iter];
  const BMVert *eve = ((const BMVert **)data->elems)[loose_vert_i];
  for (const ExtractorRunData &run_data : data->extractors) {
    run_data.extractor->iter_loose_vert_bm(
        mr, eve, iter, POINTER_OFFSET(extract_data, run_data.data_offset));
  }
}

static void extract_range_iter_loose_vert_mesh(void *__restrict userdata,
                                               const int iter,
                                               const TaskParallelTLS *__restrict tls)
{
  void *extract_data = tls->userdata_chunk;

  const ExtractorIterData *data = static_cast<ExtractorIterData *>(userdata);
  const MeshRenderData &mr = *data->mr;
  for (const ExtractorRunData &run_data : data->extractors) {
    run_data.extractor->iter_loose_vert_mesh(
        mr, iter, POINTER_OFFSET(extract_data, run_data.data_offset));
  }
}

BLI_INLINE void extract_task_range_run_iter(const MeshRenderData &mr,
                                            ExtractorRunDatas *extractors,
                                            const eMRIterType iter_type,
                                            bool is_mesh,
                                            TaskParallelSettings *settings)
{
  ExtractorIterData range_data;
  range_data.mr = &mr;

  TaskParallelRangeFunc func;
  int stop;
  switch (iter_type) {
    case MR_ITER_CORNER_TRI:
      range_data.elems = is_mesh ? mr.mesh->corner_tris().data() :
                                   (void *)mr.edit_bmesh->looptris.data();
      func = is_mesh ? extract_range_iter_corner_tri_mesh : extract_range_iter_looptri_bm;
      stop = mr.corner_tris_num;
      break;
    case MR_ITER_POLY:
      range_data.elems = is_mesh ? mr.faces.data().data() : (void *)mr.bm->ftable;
      func = is_mesh ? extract_range_iter_face_mesh : extract_range_iter_face_bm;
      stop = mr.faces_num;
      break;
    case MR_ITER_LOOSE_EDGE:
      range_data.loose_elems = mr.loose_edges.data();
      range_data.elems = is_mesh ? mr.edges.data() : (void *)mr.bm->etable;
      func = is_mesh ? extract_range_iter_loose_edge_mesh : extract_range_iter_loose_edge_bm;
      stop = mr.loose_edges_num;
      break;
    case MR_ITER_LOOSE_VERT:
      range_data.loose_elems = mr.loose_verts.data();
      range_data.elems = is_mesh ? mr.vert_positions.data() : (void *)mr.bm->vtable;
      func = is_mesh ? extract_range_iter_loose_vert_mesh : extract_range_iter_loose_vert_bm;
      stop = mr.loose_verts_num;
      break;
    default:
      BLI_assert(false);
      return;
  }

  extractors->filter_into(range_data.extractors, iter_type, is_mesh);
  BLI_task_parallel_range(0, stop, &range_data, func, settings);
}

static void extract_task_range_run(void *__restrict taskdata)
{
  ExtractTaskData *data = (ExtractTaskData *)taskdata;
  const eMRIterType iter_type = data->iter_type;
  const bool is_mesh = data->mr->extract_type != MR_EXTRACT_BMESH;

  size_t userdata_chunk_size = data->extractors->data_size_total();
  void *userdata_chunk = MEM_callocN(userdata_chunk_size, __func__);

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = data->use_threading;
  settings.userdata_chunk = userdata_chunk;
  settings.userdata_chunk_size = userdata_chunk_size;
  settings.func_reduce = extract_task_reduce;
  settings.min_iter_per_thread = MIN_RANGE_LEN;

  extract_init(*data->mr, *data->cache, *data->extractors, data->buffers, userdata_chunk);

  if (iter_type & MR_ITER_CORNER_TRI) {
    extract_task_range_run_iter(
        *data->mr, data->extractors, MR_ITER_CORNER_TRI, is_mesh, &settings);
  }
  if (iter_type & MR_ITER_POLY) {
    extract_task_range_run_iter(*data->mr, data->extractors, MR_ITER_POLY, is_mesh, &settings);
  }
  if (iter_type & MR_ITER_LOOSE_EDGE) {
    extract_task_range_run_iter(
        *data->mr, data->extractors, MR_ITER_LOOSE_EDGE, is_mesh, &settings);
  }
  if (iter_type & MR_ITER_LOOSE_VERT) {
    extract_task_range_run_iter(
        *data->mr, data->extractors, MR_ITER_LOOSE_VERT, is_mesh, &settings);
  }

  extract_finish(*data->mr, *data->cache, *data->extractors, userdata_chunk);
  MEM_freeN(userdata_chunk);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract In Parallel Ranges
 * \{ */

static TaskNode *extract_task_node_create(TaskGraph *task_graph,
                                          const MeshRenderData &mr,
                                          MeshBatchCache &cache,
                                          ExtractorRunDatas *extractors,
                                          MeshBufferList *buffers,
                                          const bool use_threading)
{
  ExtractTaskData *taskdata = new ExtractTaskData(mr, cache, extractors, buffers, use_threading);
  TaskNode *task_node = BLI_task_graph_node_create(
      task_graph, extract_task_range_run, taskdata, [](void *data) {
        delete static_cast<ExtractTaskData *>(data);
      });
  return task_node;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Task Node - Update Mesh Render Data
 * \{ */

struct MeshRenderDataUpdateTaskData {
  MeshRenderData *mr = nullptr;
  MeshBufferCache *cache = nullptr;
  eMRIterType iter_type;
  eMRDataType data_flag;

  MeshRenderDataUpdateTaskData(MeshRenderData &mr,
                               MeshBufferCache &cache,
                               eMRIterType iter_type,
                               eMRDataType data_flag)
      : mr(&mr), cache(&cache), iter_type(iter_type), data_flag(data_flag)
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
  MeshRenderData &mr = *update_task_data->mr;
  const eMRIterType iter_type = update_task_data->iter_type;
  const eMRDataType data_flag = update_task_data->data_flag;

  mesh_render_data_update_normals(mr, data_flag);
  mesh_render_data_update_loose_geom(mr, *update_task_data->cache, iter_type, data_flag);
}

static TaskNode *mesh_extract_render_data_node_create(TaskGraph *task_graph,
                                                      MeshRenderData &mr,
                                                      MeshBufferCache &cache,
                                                      const eMRIterType iter_type,
                                                      const eMRDataType data_flag)
{
  MeshRenderDataUpdateTaskData *task_data = new MeshRenderDataUpdateTaskData(
      mr, cache, iter_type, data_flag);

  TaskNode *task_node = BLI_task_graph_node_create(
      task_graph,
      mesh_extract_render_data_node_exec,
      task_data,
      (TaskGraphNodeFreeFunction)mesh_render_data_update_task_data_free);
  return task_node;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Loop
 * \{ */

void mesh_buffer_cache_create_requested(TaskGraph &task_graph,
                                        MeshBatchCache &cache,
                                        MeshBufferCache &mbc,
                                        Object &object,
                                        Mesh &mesh,
                                        const bool is_editmode,
                                        const bool is_paint_mode,
                                        const bool edit_mode_active,
                                        const float4x4 &object_to_world,
                                        const bool do_final,
                                        const bool do_uvedit,
                                        const Scene &scene,
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
  const bool do_hq_normals = (scene.r.perf_flag & SCE_PERF_HQ_NORMALS) != 0 ||
                             GPU_use_hq_normals_workaround();

  /* Create an array containing all the extractors that needs to be executed. */
  ExtractorRunDatas extractors;

  MeshBufferList &buffers = mbc.buff;

#define EXTRACT_ADD_REQUESTED(type, name) \
  do { \
    if (DRW_##type##_requested(buffers.type.name)) { \
      const MeshExtract *extractor = mesh_extract_override_get(&extract_##name, do_hq_normals); \
      extractors.append(extractor); \
    } \
  } while (0)

  EXTRACT_ADD_REQUESTED(vbo, pos);
  EXTRACT_ADD_REQUESTED(vbo, nor);
  EXTRACT_ADD_REQUESTED(vbo, uv);
  EXTRACT_ADD_REQUESTED(vbo, tan);
  EXTRACT_ADD_REQUESTED(vbo, sculpt_data);
  EXTRACT_ADD_REQUESTED(vbo, orco);
  EXTRACT_ADD_REQUESTED(vbo, edge_fac);
  EXTRACT_ADD_REQUESTED(vbo, weights);
  EXTRACT_ADD_REQUESTED(vbo, edit_data);
  EXTRACT_ADD_REQUESTED(vbo, edituv_data);
  EXTRACT_ADD_REQUESTED(vbo, edituv_stretch_area);
  EXTRACT_ADD_REQUESTED(vbo, edituv_stretch_angle);
  EXTRACT_ADD_REQUESTED(vbo, mesh_analysis);
  EXTRACT_ADD_REQUESTED(vbo, fdots_pos);
  EXTRACT_ADD_REQUESTED(vbo, fdots_nor);
  EXTRACT_ADD_REQUESTED(vbo, fdots_uv);
  EXTRACT_ADD_REQUESTED(vbo, fdots_edituv_data);
  EXTRACT_ADD_REQUESTED(vbo, face_idx);
  EXTRACT_ADD_REQUESTED(vbo, edge_idx);
  EXTRACT_ADD_REQUESTED(vbo, vert_idx);
  EXTRACT_ADD_REQUESTED(vbo, fdot_idx);
  EXTRACT_ADD_REQUESTED(vbo, skin_roots);
  for (int i = 0; i < GPU_MAX_ATTR; i++) {
    EXTRACT_ADD_REQUESTED(vbo, attr[i]);
  }
  EXTRACT_ADD_REQUESTED(vbo, attr_viewer);
  EXTRACT_ADD_REQUESTED(vbo, vnor);

  EXTRACT_ADD_REQUESTED(ibo, points);
  EXTRACT_ADD_REQUESTED(ibo, fdots);
  EXTRACT_ADD_REQUESTED(ibo, lines_paint_mask);
  EXTRACT_ADD_REQUESTED(ibo, lines_adjacency);
  EXTRACT_ADD_REQUESTED(ibo, edituv_tris);
  EXTRACT_ADD_REQUESTED(ibo, edituv_lines);
  EXTRACT_ADD_REQUESTED(ibo, edituv_points);
  EXTRACT_ADD_REQUESTED(ibo, edituv_fdots);

#undef EXTRACT_ADD_REQUESTED

  if (extractors.is_empty() && !DRW_ibo_requested(buffers.ibo.lines) &&
      !DRW_ibo_requested(buffers.ibo.lines_loose) && !DRW_ibo_requested(buffers.ibo.tris))
  {
    return;
  }

#ifdef DEBUG_TIME
  double rdata_start = BLI_time_now_seconds();
#endif

  MeshRenderData *mr = mesh_render_data_create(object,
                                               mesh,
                                               is_editmode,
                                               is_paint_mode,
                                               edit_mode_active,
                                               object_to_world,
                                               do_final,
                                               do_uvedit,
                                               use_hide,
                                               ts);
  mr->use_subsurf_fdots = mr->mesh && !mr->mesh->runtime->subsurf_face_dot_tags.is_empty();
  mr->use_final_mesh = do_final;
  mr->use_simplify_normals = (scene.r.mode & R_SIMPLIFY) && (scene.r.mode & R_SIMPLIFY_NORMALS);

#ifdef DEBUG_TIME
  double rdata_end = BLI_time_now_seconds();
#endif

  eMRIterType iter_type = extractors.iter_types();
  eMRDataType data_flag = extractors.data_types();

  TaskNode *task_node_mesh_render_data = mesh_extract_render_data_node_create(
      &task_graph, *mr, mbc, iter_type, data_flag);

  /* Simple heuristic. */
  const bool use_thread = (mr->corners_num + mr->loose_indices_num) > MIN_RANGE_LEN;

  if (DRW_ibo_requested(buffers.ibo.tris)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferCache &mbc;
      MeshBatchCache &cache;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          const SortedFaceData &face_sorted = mesh_render_data_faces_sorted_ensure(data.mr,
                                                                                   data.mbc);
          extract_tris(data.mr, face_sorted, data.cache, *data.mbc.buff.ibo.tris);
        },
        new TaskData{*mr, mbc, cache},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_ibo_requested(buffers.ibo.lines) || DRW_ibo_requested(buffers.ibo.lines_loose)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
      MeshBatchCache &cache;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_lines(data.mr,
                        data.buffers.ibo.lines,
                        data.buffers.ibo.lines_loose,
                        data.cache.no_loose_wire);
        },
        new TaskData{*mr, buffers, cache},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }

  if (use_thread) {
    /* First run the requested extractors that do not support asynchronous ranges. */
    for (const ExtractorRunData &run_data : extractors) {
      const MeshExtract *extractor = run_data.extractor;
      if (!extractor->use_threading) {
        ExtractorRunDatas *single_threaded_extractors = new ExtractorRunDatas();
        single_threaded_extractors->append(extractor);
        TaskNode *task_node = extract_task_node_create(
            &task_graph, *mr, cache, single_threaded_extractors, &buffers, false);

        BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
      }
    }

    /* Distribute the remaining extractors into ranges per core. */
    ExtractorRunDatas *multi_threaded_extractors = new ExtractorRunDatas();
    extractors.filter_threaded_extractors_into(*multi_threaded_extractors);
    if (!multi_threaded_extractors->is_empty()) {
      TaskNode *task_node = extract_task_node_create(
          &task_graph, *mr, cache, multi_threaded_extractors, &buffers, true);

      BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
    }
    else {
      /* No tasks created freeing extractors list. */
      delete multi_threaded_extractors;
    }
  }
  else {
    /* Run all requests on the same thread. */
    ExtractorRunDatas *extractors_copy = new ExtractorRunDatas(extractors);
    TaskNode *task_node = extract_task_node_create(
        &task_graph, *mr, cache, extractors_copy, &buffers, false);

    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }

  /* Trigger the sub-graph for this mesh. */
  BLI_task_graph_node_push_work(task_node_mesh_render_data);

#ifdef DEBUG_TIME
  BLI_task_graph_work_and_wait(task_graph);
  double end = BLI_time_now_seconds();

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

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Subdivision Extract Loop
 * \{ */

void mesh_buffer_cache_create_requested_subdiv(MeshBatchCache &cache,
                                               MeshBufferCache &mbc,
                                               DRWSubdivCache &subdiv_cache,
                                               MeshRenderData &mr)
{
  /* Create an array containing all the extractors that needs to be executed. */
  ExtractorRunDatas extractors;

  MeshBufferList &buffers = mbc.buff;

#define EXTRACT_ADD_REQUESTED(type, name) \
  do { \
    if (DRW_##type##_requested(buffers.type.name)) { \
      const MeshExtract *extractor = &extract_##name; \
      extractors.append(extractor); \
    } \
  } while (0)

  /* Orcos are extracted at the same time as positions. */
  if (DRW_vbo_requested(buffers.vbo.pos) || DRW_vbo_requested(buffers.vbo.orco)) {
    extractors.append(&extract_pos);
  }

  EXTRACT_ADD_REQUESTED(vbo, nor);
  for (int i = 0; i < GPU_MAX_ATTR; i++) {
    EXTRACT_ADD_REQUESTED(vbo, attr[i]);
  }

  /* We use only one extractor for face dots, as the work is done in a single compute shader. */
  if (DRW_vbo_requested(buffers.vbo.fdots_nor) || DRW_vbo_requested(buffers.vbo.fdots_pos) ||
      DRW_ibo_requested(buffers.ibo.fdots))
  {
    extractors.append(&extract_fdots_pos);
  }

  EXTRACT_ADD_REQUESTED(ibo, edituv_points);
  EXTRACT_ADD_REQUESTED(ibo, edituv_tris);
  EXTRACT_ADD_REQUESTED(ibo, edituv_lines);
  EXTRACT_ADD_REQUESTED(vbo, vert_idx);
  EXTRACT_ADD_REQUESTED(vbo, edge_idx);
  EXTRACT_ADD_REQUESTED(vbo, face_idx);
  EXTRACT_ADD_REQUESTED(vbo, edge_fac);
  EXTRACT_ADD_REQUESTED(ibo, points);
  EXTRACT_ADD_REQUESTED(vbo, edit_data);
  EXTRACT_ADD_REQUESTED(vbo, edituv_data);
  /* Make sure UVs are computed before edituv stuffs. */
  EXTRACT_ADD_REQUESTED(vbo, uv);
  EXTRACT_ADD_REQUESTED(vbo, tan);
  EXTRACT_ADD_REQUESTED(vbo, edituv_stretch_area);
  EXTRACT_ADD_REQUESTED(vbo, edituv_stretch_angle);
  EXTRACT_ADD_REQUESTED(ibo, lines_paint_mask);
  EXTRACT_ADD_REQUESTED(ibo, lines_adjacency);
  EXTRACT_ADD_REQUESTED(vbo, weights);
  EXTRACT_ADD_REQUESTED(vbo, sculpt_data);

#undef EXTRACT_ADD_REQUESTED

  if (extractors.is_empty() && !DRW_ibo_requested(buffers.ibo.lines) &&
      !DRW_ibo_requested(buffers.ibo.lines_loose) && !DRW_ibo_requested(buffers.ibo.tris))
  {
    return;
  }

  mesh_render_data_update_normals(mr, MR_DATA_TAN_LOOP_NOR);
  mesh_render_data_update_loose_geom(
      mr, mbc, MR_ITER_LOOSE_EDGE | MR_ITER_LOOSE_VERT, MR_DATA_LOOSE_GEOM);
  DRW_subdivide_loose_geom(subdiv_cache, mbc);

  if (DRW_ibo_requested(buffers.ibo.lines) || DRW_ibo_requested(buffers.ibo.lines_loose)) {
    extract_lines_subdiv(
        subdiv_cache, mr, buffers.ibo.lines, buffers.ibo.lines_loose, cache.no_loose_wire);
  }
  if (DRW_ibo_requested(buffers.ibo.tris)) {
    extract_tris_subdiv(subdiv_cache, cache, *buffers.ibo.tris);
  }

  void *data_stack = MEM_mallocN(extractors.data_size_total(), __func__);
  uint32_t data_offset = 0;
  for (const ExtractorRunData &run_data : extractors) {
    const MeshExtract *extractor = run_data.extractor;
    void *buffer = mesh_extract_buffer_get(extractor, &buffers);
    void *data = POINTER_OFFSET(data_stack, data_offset);

    extractor->init_subdiv(subdiv_cache, mr, cache, buffer, data);

    if (extractor->iter_subdiv_mesh || extractor->iter_subdiv_bm) {
      int *subdiv_loop_face_index = subdiv_cache.subdiv_loop_face_index;
      if (mr.extract_type == MR_EXTRACT_BMESH) {
        for (uint i = 0; i < subdiv_cache.num_subdiv_quads; i++) {
          /* Multiply by 4 to have the start index of the quad's loop, as subdiv_loop_face_index is
           * based on the subdivision loops. */
          const int poly_origindex = subdiv_loop_face_index[i * 4];
          const BMFace *efa = BM_face_at_index(mr.bm, poly_origindex);
          extractor->iter_subdiv_bm(subdiv_cache, mr, data, i, efa);
        }
      }
      else {
        for (uint i = 0; i < subdiv_cache.num_subdiv_quads; i++) {
          /* Multiply by 4 to have the start index of the quad's loop, as subdiv_loop_face_index is
           * based on the subdivision loops. */
          const int poly_origindex = subdiv_loop_face_index[i * 4];
          extractor->iter_subdiv_mesh(subdiv_cache, mr, data, i, poly_origindex);
        }
      }
    }

    if (extractor->iter_loose_geom_subdiv) {
      extractor->iter_loose_geom_subdiv(subdiv_cache, mr, buffer, data);
    }

    if (extractor->finish_subdiv) {
      extractor->finish_subdiv(subdiv_cache, mr, cache, buffer, data);
    }
  }
  MEM_freeN(data_stack);
}

/** \} */

}  // namespace blender::draw
