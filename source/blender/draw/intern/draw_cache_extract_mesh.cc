/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Extraction of Mesh data into VBO to feed to GPU.
 */

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BLI_task.h"

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

struct MeshRenderDataUpdateTaskData {
  std::unique_ptr<MeshRenderData> mr;
  MeshBufferCache &cache;
};

static void mesh_extract_render_data_node_exec(void *__restrict task_data)
{
  auto *update_task_data = static_cast<MeshRenderDataUpdateTaskData *>(task_data);
  MeshRenderData &mr = *update_task_data->mr;
  MeshBufferList &buffers = update_task_data->cache.buff;

  const bool request_face_normals = DRW_vbo_requested(buffers.vbo.nor) ||
                                    DRW_vbo_requested(buffers.vbo.fdots_nor) ||
                                    DRW_vbo_requested(buffers.vbo.edge_fac) ||
                                    DRW_vbo_requested(buffers.vbo.mesh_analysis);
  const bool request_corner_normals = DRW_vbo_requested(buffers.vbo.nor);
  const bool force_corner_normals = DRW_vbo_requested(buffers.vbo.tan);

  if (request_face_normals) {
    mesh_render_data_update_face_normals(mr);
  }
  if ((request_corner_normals && mr.normals_domain == bke::MeshNormalDomain::Corner &&
       !mr.use_simplify_normals) ||
      force_corner_normals)
  {
    mesh_render_data_update_corner_normals(mr);
  }

  const bool calc_loose_geom = DRW_ibo_requested(buffers.ibo.lines) ||
                               DRW_ibo_requested(buffers.ibo.lines_loose) ||
                               DRW_ibo_requested(buffers.ibo.points) ||
                               DRW_vbo_requested(buffers.vbo.pos) ||
                               DRW_vbo_requested(buffers.vbo.edit_data) ||
                               DRW_vbo_requested(buffers.vbo.vnor) ||
                               DRW_vbo_requested(buffers.vbo.vert_idx) ||
                               DRW_vbo_requested(buffers.vbo.edge_idx) ||
                               DRW_vbo_requested(buffers.vbo.edge_fac);

  if (calc_loose_geom) {
    mesh_render_data_update_loose_geom(mr, update_task_data->cache);
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Loop
 * \{ */

static bool any_attr_requested(const MeshBufferList &buffers)
{
  for (const int i : IndexRange(ARRAY_SIZE(buffers.vbo.attr))) {
    if (DRW_vbo_requested(buffers.vbo.attr[i])) {
      return true;
    }
  }
  return false;
}

void mesh_buffer_cache_create_requested(TaskGraph &task_graph,
                                        MeshBatchCache &cache,
                                        MeshBufferCache &mbc,
                                        Object &object,
                                        Mesh &mesh,
                                        const bool is_editmode,
                                        const bool is_paint_mode,
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

  MeshBufferList &buffers = mbc.buff;
  const bool attrs_requested = any_attr_requested(buffers);
  if (!DRW_ibo_requested(buffers.ibo.lines) && !DRW_ibo_requested(buffers.ibo.lines_loose) &&
      !DRW_ibo_requested(buffers.ibo.tris) && !DRW_ibo_requested(buffers.ibo.points) &&
      !DRW_ibo_requested(buffers.ibo.fdots) && !DRW_vbo_requested(buffers.vbo.pos) &&
      !DRW_vbo_requested(buffers.vbo.fdots_pos) && !DRW_vbo_requested(buffers.vbo.nor) &&
      !DRW_vbo_requested(buffers.vbo.vnor) && !DRW_vbo_requested(buffers.vbo.fdots_nor) &&
      !DRW_vbo_requested(buffers.vbo.edge_fac) && !DRW_vbo_requested(buffers.vbo.tan) &&
      !DRW_vbo_requested(buffers.vbo.edit_data) && !DRW_vbo_requested(buffers.vbo.face_idx) &&
      !DRW_vbo_requested(buffers.vbo.edge_idx) && !DRW_vbo_requested(buffers.vbo.vert_idx) &&
      !DRW_vbo_requested(buffers.vbo.fdot_idx) && !DRW_vbo_requested(buffers.vbo.weights) &&
      !DRW_vbo_requested(buffers.vbo.fdots_uv) &&
      !DRW_vbo_requested(buffers.vbo.fdots_edituv_data) && !DRW_vbo_requested(buffers.vbo.uv) &&
      !DRW_vbo_requested(buffers.vbo.edituv_stretch_area) &&
      !DRW_vbo_requested(buffers.vbo.edituv_stretch_angle) &&
      !DRW_vbo_requested(buffers.vbo.edituv_data) && !DRW_ibo_requested(buffers.ibo.edituv_tris) &&
      !DRW_ibo_requested(buffers.ibo.edituv_lines) &&
      !DRW_ibo_requested(buffers.ibo.edituv_points) &&
      !DRW_ibo_requested(buffers.ibo.edituv_fdots) &&
      !DRW_ibo_requested(buffers.ibo.lines_paint_mask) &&
      !DRW_ibo_requested(buffers.ibo.lines_adjacency) &&
      !DRW_vbo_requested(buffers.vbo.skin_roots) && !DRW_vbo_requested(buffers.vbo.sculpt_data) &&
      !DRW_vbo_requested(buffers.vbo.orco) && !DRW_vbo_requested(buffers.vbo.mesh_analysis) &&
      !DRW_vbo_requested(buffers.vbo.attr_viewer) && !attrs_requested)
  {
    return;
  }

#ifdef DEBUG_TIME
  double rdata_start = BLI_time_now_seconds();
#endif

  std::unique_ptr<MeshRenderData> mr_ptr = mesh_render_data_create(object,
                                                                   mesh,
                                                                   is_editmode,
                                                                   is_paint_mode,
                                                                   object_to_world,
                                                                   do_final,
                                                                   do_uvedit,
                                                                   use_hide,
                                                                   ts);
  MeshRenderData *mr = mr_ptr.get();
  mr->use_subsurf_fdots = mr->mesh && !mr->mesh->runtime->subsurf_face_dot_tags.is_empty();
  mr->use_final_mesh = do_final;
  mr->use_simplify_normals = (scene.r.mode & R_SIMPLIFY) && (scene.r.mode & R_SIMPLIFY_NORMALS);

#ifdef DEBUG_TIME
  double rdata_end = BLI_time_now_seconds();
#endif

  TaskNode *task_node_mesh_render_data = BLI_task_graph_node_create(
      &task_graph,
      mesh_extract_render_data_node_exec,
      new MeshRenderDataUpdateTaskData{std::move(mr_ptr), mbc},
      [](void *task_data) { delete static_cast<MeshRenderDataUpdateTaskData *>(task_data); });

  if (DRW_vbo_requested(buffers.vbo.pos)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferCache &mbc;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_positions(data.mr, *data.mbc.buff.vbo.pos);
        },
        new TaskData{*mr, mbc},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.fdots_pos)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferCache &mbc;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_face_dots_position(data.mr, *data.mbc.buff.vbo.fdots_pos);
        },
        new TaskData{*mr, mbc},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.nor)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferCache &mbc;
      bool do_hq_normals;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_normals(data.mr, data.do_hq_normals, *data.mbc.buff.vbo.nor);
        },
        new TaskData{*mr, mbc, do_hq_normals},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.vnor)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_vert_normals(data.mr, *data.buffers.vbo.vnor);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.fdots_nor)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferCache &mbc;
      bool do_hq_normals;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_face_dot_normals(data.mr, data.do_hq_normals, *data.mbc.buff.vbo.fdots_nor);
        },
        new TaskData{*mr, mbc, do_hq_normals},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.edge_fac)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferCache &mbc;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_edge_factor(data.mr, *data.mbc.buff.vbo.edge_fac);
        },
        new TaskData{*mr, mbc},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
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
  if (DRW_ibo_requested(buffers.ibo.points)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_points(data.mr, *data.buffers.ibo.points);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_ibo_requested(buffers.ibo.fdots)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_face_dots(data.mr, *data.buffers.ibo.fdots);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.edit_data)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_edit_data(data.mr, *data.buffers.vbo.edit_data);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.tan)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
      MeshBatchCache &cache;
      bool do_hq_normals;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_tangents(data.mr, data.cache, data.do_hq_normals, *data.buffers.vbo.tan);
        },
        new TaskData{*mr, buffers, cache, do_hq_normals},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.face_idx) || DRW_vbo_requested(buffers.vbo.edge_idx) ||
      DRW_vbo_requested(buffers.vbo.vert_idx) || DRW_vbo_requested(buffers.vbo.fdot_idx))
  {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          if (DRW_vbo_requested(data.buffers.vbo.vert_idx)) {
            extract_vert_index(data.mr, *data.buffers.vbo.vert_idx);
          }
          if (DRW_vbo_requested(data.buffers.vbo.edge_idx)) {
            extract_edge_index(data.mr, *data.buffers.vbo.edge_idx);
          }
          if (DRW_vbo_requested(data.buffers.vbo.face_idx)) {
            extract_face_index(data.mr, *data.buffers.vbo.face_idx);
          }
          if (DRW_vbo_requested(data.buffers.vbo.fdot_idx)) {
            extract_face_dot_index(data.mr, *data.buffers.vbo.fdot_idx);
          }
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.weights)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
      MeshBatchCache &cache;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_weights(data.mr, data.cache, *data.buffers.vbo.weights);
        },
        new TaskData{*mr, buffers, cache},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.fdots_uv)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_face_dots_uv(data.mr, *data.buffers.vbo.fdots_uv);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.fdots_edituv_data)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_face_dots_edituv_data(data.mr, *data.buffers.vbo.fdots_edituv_data);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.uv)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
      MeshBatchCache &cache;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_uv_maps(data.mr, data.cache, *data.buffers.vbo.uv);
        },
        new TaskData{*mr, buffers, cache},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.edituv_stretch_area)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
      MeshBatchCache &cache;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_edituv_stretch_area(data.mr,
                                      *data.buffers.vbo.edituv_stretch_area,
                                      data.cache.tot_area,
                                      data.cache.tot_uv_area);
        },
        new TaskData{*mr, buffers, cache},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.edituv_stretch_angle)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_edituv_stretch_angle(data.mr, *data.buffers.vbo.edituv_stretch_angle);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.edituv_data)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_edituv_data(data.mr, *data.buffers.vbo.edituv_data);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_ibo_requested(buffers.ibo.edituv_tris)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_edituv_tris(data.mr, *data.buffers.ibo.edituv_tris);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_ibo_requested(buffers.ibo.edituv_lines)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_edituv_lines(data.mr, *data.buffers.ibo.edituv_lines);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_ibo_requested(buffers.ibo.edituv_points)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_edituv_points(data.mr, *data.buffers.ibo.edituv_points);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_ibo_requested(buffers.ibo.edituv_fdots)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_edituv_face_dots(data.mr, *data.buffers.ibo.edituv_fdots);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_ibo_requested(buffers.ibo.lines_paint_mask)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_lines_paint_mask(data.mr, *data.buffers.ibo.lines_paint_mask);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_ibo_requested(buffers.ibo.lines_adjacency)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
      MeshBatchCache &cache;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_lines_adjacency(
              data.mr, *data.buffers.ibo.lines_adjacency, data.cache.is_manifold);
        },
        new TaskData{*mr, buffers, cache},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.skin_roots)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_skin_roots(data.mr, *data.buffers.vbo.skin_roots);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.sculpt_data)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_sculpt_data(data.mr, *data.buffers.vbo.sculpt_data);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.orco)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_orco(data.mr, *data.buffers.vbo.orco);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.mesh_analysis)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_mesh_analysis(data.mr, *data.buffers.vbo.mesh_analysis);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (attrs_requested) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
      MeshBatchCache &cache;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_attributes(data.mr,
                             {data.cache.attr_used.requests, GPU_MAX_ATTR},
                             {data.buffers.vbo.attr, GPU_MAX_ATTR});
        },
        new TaskData{*mr, buffers, cache},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  if (DRW_vbo_requested(buffers.vbo.attr_viewer)) {
    struct TaskData {
      MeshRenderData &mr;
      MeshBufferList &buffers;
    };
    TaskNode *task_node = BLI_task_graph_node_create(
        &task_graph,
        [](void *__restrict task_data) {
          const TaskData &data = *static_cast<TaskData *>(task_data);
          extract_attr_viewer(data.mr, *data.buffers.vbo.attr_viewer);
        },
        new TaskData{*mr, buffers},
        [](void *task_data) { delete static_cast<TaskData *>(task_data); });
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
  MeshBufferList &buffers = mbc.buff;
  const bool attrs_requested = any_attr_requested(buffers);
  if (!DRW_ibo_requested(buffers.ibo.lines) && !DRW_ibo_requested(buffers.ibo.lines_loose) &&
      !DRW_ibo_requested(buffers.ibo.tris) && !DRW_ibo_requested(buffers.ibo.points) &&
      !DRW_vbo_requested(buffers.vbo.pos) && !DRW_vbo_requested(buffers.vbo.orco) &&
      !DRW_vbo_requested(buffers.vbo.nor) && !DRW_vbo_requested(buffers.vbo.edge_fac) &&
      !DRW_vbo_requested(buffers.vbo.tan) && !DRW_vbo_requested(buffers.vbo.edit_data) &&
      !DRW_vbo_requested(buffers.vbo.face_idx) && !DRW_vbo_requested(buffers.vbo.edge_idx) &&
      !DRW_vbo_requested(buffers.vbo.vert_idx) && !DRW_vbo_requested(buffers.vbo.weights) &&
      !DRW_vbo_requested(buffers.vbo.fdots_nor) && !DRW_vbo_requested(buffers.vbo.fdots_pos) &&
      !DRW_ibo_requested(buffers.ibo.fdots) && !DRW_vbo_requested(buffers.vbo.uv) &&
      !DRW_vbo_requested(buffers.vbo.edituv_stretch_area) &&
      !DRW_vbo_requested(buffers.vbo.edituv_stretch_angle) &&
      !DRW_vbo_requested(buffers.vbo.edituv_data) && !DRW_ibo_requested(buffers.ibo.edituv_tris) &&
      !DRW_ibo_requested(buffers.ibo.edituv_lines) &&
      !DRW_ibo_requested(buffers.ibo.edituv_points) &&
      !DRW_ibo_requested(buffers.ibo.lines_paint_mask) &&
      !DRW_ibo_requested(buffers.ibo.lines_adjacency) &&
      !DRW_vbo_requested(buffers.vbo.sculpt_data) && !attrs_requested)
  {
    return;
  }

  mesh_render_data_update_corner_normals(mr);
  mesh_render_data_update_loose_geom(mr, mbc);
  DRW_subdivide_loose_geom(subdiv_cache, mbc);

  if (DRW_vbo_requested(buffers.vbo.pos) || DRW_vbo_requested(buffers.vbo.orco)) {
    extract_positions_subdiv(subdiv_cache, mr, *buffers.vbo.pos, buffers.vbo.orco);
  }
  if (DRW_vbo_requested(buffers.vbo.nor)) {
    /* The corner normals calculation uses positions and normals stored in the `pos` VBO. */
    extract_normals_subdiv(mr, subdiv_cache, *buffers.vbo.pos, *buffers.vbo.nor);
  }
  if (DRW_vbo_requested(buffers.vbo.edge_fac)) {
    extract_edge_factor_subdiv(subdiv_cache, mr, *buffers.vbo.pos, *buffers.vbo.edge_fac);
  }
  if (DRW_ibo_requested(buffers.ibo.lines) || DRW_ibo_requested(buffers.ibo.lines_loose)) {
    extract_lines_subdiv(
        subdiv_cache, mr, buffers.ibo.lines, buffers.ibo.lines_loose, cache.no_loose_wire);
  }
  if (DRW_ibo_requested(buffers.ibo.tris)) {
    extract_tris_subdiv(subdiv_cache, cache, *buffers.ibo.tris);
  }
  if (DRW_ibo_requested(buffers.ibo.points)) {
    extract_points_subdiv(mr, subdiv_cache, *buffers.ibo.points);
  }
  if (DRW_vbo_requested(buffers.vbo.edit_data)) {
    extract_edit_data_subdiv(mr, subdiv_cache, *buffers.vbo.edit_data);
  }
  if (DRW_vbo_requested(buffers.vbo.tan)) {
    extract_tangents_subdiv(mr, subdiv_cache, cache, *buffers.vbo.tan);
  }
  if (DRW_vbo_requested(buffers.vbo.vert_idx)) {
    extract_vert_index_subdiv(subdiv_cache, mr, *buffers.vbo.vert_idx);
  }
  if (DRW_vbo_requested(buffers.vbo.edge_idx)) {
    extract_edge_index_subdiv(subdiv_cache, mr, *buffers.vbo.edge_idx);
  }
  if (DRW_vbo_requested(buffers.vbo.face_idx)) {
    extract_face_index_subdiv(subdiv_cache, mr, *buffers.vbo.face_idx);
  }
  if (DRW_vbo_requested(buffers.vbo.weights)) {
    extract_weights_subdiv(mr, subdiv_cache, cache, *buffers.vbo.weights);
  }
  if (DRW_vbo_requested(buffers.vbo.fdots_nor) || DRW_vbo_requested(buffers.vbo.fdots_pos) ||
      DRW_ibo_requested(buffers.ibo.fdots))
  {
    /* We use only one extractor for face dots, as the work is done in a single compute shader. */
    extract_face_dots_subdiv(
        subdiv_cache, *buffers.vbo.fdots_pos, buffers.vbo.fdots_nor, *buffers.ibo.fdots);
  }
  if (DRW_ibo_requested(buffers.ibo.lines_paint_mask)) {
    extract_lines_paint_mask_subdiv(mr, subdiv_cache, *buffers.ibo.lines_paint_mask);
  }
  if (DRW_ibo_requested(buffers.ibo.lines_adjacency)) {
    extract_lines_adjacency_subdiv(subdiv_cache, *buffers.ibo.lines_adjacency, cache.is_manifold);
  }
  if (DRW_vbo_requested(buffers.vbo.sculpt_data)) {
    extract_sculpt_data_subdiv(mr, subdiv_cache, *buffers.vbo.sculpt_data);
  }
  if (DRW_vbo_requested(buffers.vbo.uv)) {
    /* Make sure UVs are computed before edituv stuffs. */
    extract_uv_maps_subdiv(subdiv_cache, cache, *buffers.vbo.uv);
  }
  if (DRW_vbo_requested(buffers.vbo.edituv_stretch_area)) {
    extract_edituv_stretch_area_subdiv(
        mr, subdiv_cache, *buffers.vbo.edituv_stretch_area, cache.tot_area, cache.tot_uv_area);
  }
  if (DRW_vbo_requested(buffers.vbo.edituv_stretch_area)) {
    extract_edituv_stretch_angle_subdiv(
        mr, subdiv_cache, cache, *buffers.vbo.edituv_stretch_angle);
  }
  if (DRW_vbo_requested(buffers.vbo.edituv_data)) {
    extract_edituv_data_subdiv(mr, subdiv_cache, *buffers.vbo.edituv_data);
  }
  if (DRW_ibo_requested(buffers.ibo.edituv_tris)) {
    extract_edituv_tris_subdiv(mr, subdiv_cache, *buffers.ibo.edituv_tris);
  }
  if (DRW_ibo_requested(buffers.ibo.edituv_lines)) {
    extract_edituv_lines_subdiv(mr, subdiv_cache, *buffers.ibo.edituv_lines);
  }
  if (DRW_ibo_requested(buffers.ibo.edituv_points)) {
    extract_edituv_points_subdiv(mr, subdiv_cache, *buffers.ibo.edituv_points);
  }
  if (attrs_requested) {
    extract_attributes_subdiv(mr,
                              subdiv_cache,
                              {cache.attr_used.requests, GPU_MAX_ATTR},
                              {buffers.vbo.attr, GPU_MAX_ATTR});
  }
}

/** \} */

}  // namespace blender::draw
