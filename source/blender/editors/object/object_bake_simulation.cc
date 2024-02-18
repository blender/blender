/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fstream>
#include <iomanip>
#include <random>

#include "BLI_fileops.hh"
#include "BLI_path_util.h"
#include "BLI_serialize.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"

#include "DNA_array_utils.hh"
#include "DNA_modifier_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_bake_geometry_nodes_modifier.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"
#include "BKE_node_runtime.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "DEG_depsgraph.hh"

#include "MOD_nodes.hh"

#include "object_intern.h"

#include "WM_api.hh"

#include "UI_interface.hh"

namespace bake = blender::bke::bake;

namespace blender::ed::object::bake_simulation {

static bool simulate_to_frame_poll(bContext *C)
{
  if (!ED_operator_object_active(C)) {
    return false;
  }
  return true;
}

struct SimulateToFrameJob {
  wmWindowManager *wm;
  Main *bmain;
  Depsgraph *depsgraph;
  Scene *scene;
  Vector<Object *> objects;
  int start_frame;
  int end_frame;
};

static void simulate_to_frame_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  SimulateToFrameJob &job = *static_cast<SimulateToFrameJob *>(customdata);
  G.is_rendering = true;
  G.is_break = false;
  WM_set_locked_interface(job.wm, true);

  Vector<Object *> objects_to_calc;
  for (Object *object : job.objects) {
    if (!BKE_id_is_editable(job.bmain, &object->id)) {
      continue;
    }
    LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
      if (md->type != eModifierType_Nodes) {
        continue;
      }
      NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
      if (!nmd->runtime->cache) {
        continue;
      }
      for (auto item : nmd->runtime->cache->simulation_cache_by_id.items()) {
        if (item.value->cache_status != bake::CacheStatus::Baked) {
          item.value->reset();
        }
      }
    }
    objects_to_calc.append(object);
  }

  worker_status->progress = 0.0f;
  worker_status->do_update = true;

  const float frame_step_size = 1.0f;
  const float progress_per_frame = 1.0f /
                                   (float(job.end_frame - job.start_frame + 1) / frame_step_size);
  const int old_frame = job.scene->r.cfra;

  for (float frame_f = job.start_frame; frame_f <= job.end_frame; frame_f += frame_step_size) {
    const SubFrame frame{frame_f};

    if (G.is_break || worker_status->stop) {
      break;
    }

    job.scene->r.cfra = frame.frame();
    job.scene->r.subframe = frame.subframe();

    BKE_scene_graph_update_for_newframe(job.depsgraph);

    worker_status->progress += progress_per_frame;
    worker_status->do_update = true;
  }

  job.scene->r.cfra = old_frame;
  DEG_time_tag_update(job.bmain);

  worker_status->progress = 1.0f;
  worker_status->do_update = true;
}

static void simulate_to_frame_endjob(void *customdata)
{
  SimulateToFrameJob &job = *static_cast<SimulateToFrameJob *>(customdata);
  WM_set_locked_interface(job.wm, false);
  G.is_rendering = false;
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, nullptr);
}

static int simulate_to_frame_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Main *bmain = CTX_data_main(C);

  SimulateToFrameJob *job = MEM_new<SimulateToFrameJob>(__func__);
  job->wm = wm;
  job->bmain = bmain;
  job->depsgraph = depsgraph;
  job->scene = scene;
  job->start_frame = scene->r.sfra;
  job->end_frame = scene->r.cfra;

  if (RNA_boolean_get(op->ptr, "selected")) {
    CTX_DATA_BEGIN (C, Object *, object, selected_objects) {
      job->objects.append(object);
    }
    CTX_DATA_END;
  }
  else {
    if (Object *object = CTX_data_active_object(C)) {
      job->objects.append(object);
    }
  }

  wmJob *wm_job = WM_jobs_get(wm,
                              CTX_wm_window(C),
                              CTX_data_scene(C),
                              "Calculate Simulation",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_CALCULATE_SIMULATION_NODES);

  WM_jobs_customdata_set(
      wm_job, job, [](void *job) { MEM_delete(static_cast<SimulateToFrameJob *>(job)); });
  WM_jobs_timer(wm_job, 0.1, NC_OBJECT | ND_MODIFIER, NC_OBJECT | ND_MODIFIER);
  WM_jobs_callbacks(
      wm_job, simulate_to_frame_startjob, nullptr, nullptr, simulate_to_frame_endjob);

  WM_jobs_start(CTX_wm_manager(C), wm_job);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int simulate_to_frame_modal(bContext *C, wmOperator * /*op*/, const wmEvent * /*event*/)
{
  if (!WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_CALCULATE_SIMULATION_NODES))
  {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }
  return OPERATOR_PASS_THROUGH;
}

static bool bake_simulation_poll(bContext *C)
{
  if (!ED_operator_object_active(C)) {
    return false;
  }
  Main *bmain = CTX_data_main(C);
  const StringRefNull path = BKE_main_blendfile_path(bmain);
  if (path.is_empty()) {
    CTX_wm_operator_poll_msg_set(C, "File must be saved before baking");
    return false;
  }
  return true;
}

struct NodeBakeRequest {
  Object *object;
  NodesModifierData *nmd;
  int bake_id;
  int node_type;

  bake::BakePath path;
  int frame_start;
  int frame_end;
  std::unique_ptr<bake::BlobWriteSharing> blob_sharing;
};

struct BakeGeometryNodesJob {
  wmWindowManager *wm;
  Main *bmain;
  Depsgraph *depsgraph;
  Scene *scene;
  Vector<NodeBakeRequest> bake_requests;
};

static void request_bakes_in_modifier_cache(BakeGeometryNodesJob &job)
{
  for (NodeBakeRequest &request : job.bake_requests) {
    request.nmd->runtime->cache->requested_bakes.add(request.bake_id);
    /* Using #DEG_id_tag_update would tag this as user-modified which is not the case here and has
     * the issue that it invalidates simulation caches. */
    DEG_id_tag_update_for_side_effect_request(
        job.depsgraph, &request.object->id, ID_RECALC_GEOMETRY);
  }
}

static void clear_requested_bakes_in_modifier_cache(BakeGeometryNodesJob &job)
{
  for (NodeBakeRequest &request : job.bake_requests) {
    request.nmd->runtime->cache->requested_bakes.clear();
  }
}

static void bake_geometry_nodes_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  BakeGeometryNodesJob &job = *static_cast<BakeGeometryNodesJob *>(customdata);
  G.is_rendering = true;
  G.is_break = false;
  WM_set_locked_interface(job.wm, true);

  int global_bake_start_frame = INT32_MAX;
  int global_bake_end_frame = INT32_MIN;

  for (NodeBakeRequest &request : job.bake_requests) {
    global_bake_start_frame = std::min(global_bake_start_frame, request.frame_start);
    global_bake_end_frame = std::max(global_bake_end_frame, request.frame_end);
  }

  worker_status->progress = 0.0f;
  worker_status->do_update = true;

  const int frames_to_bake = global_bake_end_frame - global_bake_start_frame + 1;

  const float frame_step_size = 1.0f;
  const float progress_per_frame = frame_step_size / frames_to_bake;
  const int old_frame = job.scene->r.cfra;

  for (float frame_f = global_bake_start_frame; frame_f <= global_bake_end_frame;
       frame_f += frame_step_size)
  {
    const SubFrame frame{frame_f};

    if (G.is_break || worker_status->stop) {
      break;
    }

    job.scene->r.cfra = frame.frame();
    job.scene->r.subframe = frame.subframe();

    request_bakes_in_modifier_cache(job);

    BKE_scene_graph_update_for_newframe(job.depsgraph);

    clear_requested_bakes_in_modifier_cache(job);

    const std::string frame_file_name = bake::frame_to_file_name(frame);

    for (NodeBakeRequest &request : job.bake_requests) {
      NodesModifierData &nmd = *request.nmd;
      bake::ModifierCache &modifier_cache = *nmd.runtime->cache;
      const bake::NodeBakeCache *bake_cache = modifier_cache.get_node_bake_cache(request.bake_id);
      if (bake_cache == nullptr) {
        continue;
      }
      if (bake_cache->frames.is_empty()) {
        continue;
      }
      const bake::FrameCache &frame_cache = *bake_cache->frames.last();
      if (frame_cache.frame != frame) {
        continue;
      }

      const bake::BakePath path = request.path;

      char meta_path[FILE_MAX];
      BLI_path_join(meta_path,
                    sizeof(meta_path),
                    path.meta_dir.c_str(),
                    (frame_file_name + ".json").c_str());
      BLI_file_ensure_parent_dir_exists(meta_path);
      bake::DiskBlobWriter blob_writer{path.blobs_dir, frame_file_name};
      fstream meta_file{meta_path, std::ios::out};
      bake::serialize_bake(frame_cache.state, blob_writer, *request.blob_sharing, meta_file);
    }

    worker_status->progress += progress_per_frame;
    worker_status->do_update = true;
  }

  /* Tag simulations as being baked. */
  for (NodeBakeRequest &request : job.bake_requests) {
    if (request.node_type != GEO_NODE_SIMULATION_OUTPUT) {
      continue;
    }
    NodesModifierData &nmd = *request.nmd;
    if (bake::SimulationNodeCache *node_cache = nmd.runtime->cache->get_simulation_node_cache(
            request.bake_id))
    {
      if (!node_cache->bake.frames.is_empty()) {
        /* Tag the caches as being baked so that they are not changed anymore. */
        node_cache->cache_status = bake::CacheStatus::Baked;
      }
    }
    DEG_id_tag_update(&request.object->id, ID_RECALC_GEOMETRY);
  }

  job.scene->r.cfra = old_frame;
  DEG_time_tag_update(job.bmain);

  worker_status->progress = 1.0f;
  worker_status->do_update = true;
}

static void bake_geometry_nodes_endjob(void *customdata)
{
  BakeGeometryNodesJob &job = *static_cast<BakeGeometryNodesJob *>(customdata);
  WM_set_locked_interface(job.wm, false);
  G.is_rendering = false;
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, nullptr);
  WM_main_add_notifier(NC_NODE | ND_DISPLAY, nullptr);
}

static void reset_old_bake(NodeBakeRequest &request)
{
  switch (request.node_type) {
    case GEO_NODE_SIMULATION_OUTPUT: {
      if (bake::SimulationNodeCache *node_cache =
              request.nmd->runtime->cache->get_simulation_node_cache(request.bake_id))
      {
        node_cache->reset();
      }
      break;
    }
    case GEO_NODE_BAKE: {
      if (bake::BakeNodeCache *node_cache = request.nmd->runtime->cache->get_bake_node_cache(
              request.bake_id))
      {
        node_cache->reset();
      }
      break;
    }
  }
}

enum class BakeRequestsMode {
  /**
   * Bake all requests before returning from the function.
   */
  Sync,
  /**
   * Start a parallel job and return before the baking is done.
   */
  Async
};

static int start_bake_job(bContext *C,
                          Vector<NodeBakeRequest> requests,
                          wmOperator *op,
                          const BakeRequestsMode mode)
{
  for (NodeBakeRequest &request : requests) {
    reset_old_bake(request);
  }

  BakeGeometryNodesJob *job = MEM_new<BakeGeometryNodesJob>(__func__);
  job->wm = CTX_wm_manager(C);
  job->bmain = CTX_data_main(C);
  job->depsgraph = CTX_data_depsgraph_pointer(C);
  job->scene = CTX_data_scene(C);
  job->bake_requests = std::move(requests);

  if (mode == BakeRequestsMode::Sync) {
    wmJobWorkerStatus worker_status{};
    bake_geometry_nodes_startjob(job, &worker_status);
    bake_geometry_nodes_endjob(job);
    MEM_delete(job);
    return OPERATOR_FINISHED;
  }

  wmJob *wm_job = WM_jobs_get(job->wm,
                              CTX_wm_window(C),
                              job->scene,
                              "Bake Nodes",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_BAKE_GEOMETRY_NODES);

  WM_jobs_customdata_set(
      wm_job, job, [](void *job) { MEM_delete(static_cast<BakeGeometryNodesJob *>(job)); });
  WM_jobs_timer(wm_job, 0.1, NC_OBJECT | ND_MODIFIER, NC_OBJECT | ND_MODIFIER);
  WM_jobs_callbacks(
      wm_job, bake_geometry_nodes_startjob, nullptr, nullptr, bake_geometry_nodes_endjob);

  WM_jobs_start(CTX_wm_manager(C), wm_job);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static Vector<NodeBakeRequest> collect_simulations_to_bake(Main &bmain,
                                                           Scene &scene,
                                                           const Span<Object *> objects)
{
  Vector<NodeBakeRequest> requests;
  for (Object *object : objects) {
    if (!BKE_id_is_editable(&bmain, &object->id)) {
      continue;
    }
    LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
      if (md->type != eModifierType_Nodes) {
        continue;
      }
      if (!BKE_modifier_is_enabled(&scene, md, eModifierMode_Realtime)) {
        continue;
      }
      NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
      if (!nmd->node_group) {
        continue;
      }
      if (!nmd->runtime->cache) {
        continue;
      }
      for (const bNestedNodeRef &nested_node_ref : nmd->node_group->nested_node_refs_span()) {
        const int id = nested_node_ref.id;
        const bNode *node = nmd->node_group->find_nested_node(id);
        if (node->type != GEO_NODE_SIMULATION_OUTPUT) {
          continue;
        }
        NodeBakeRequest request;
        request.object = object;
        request.nmd = nmd;
        request.bake_id = id;
        request.node_type = node->type;
        request.blob_sharing = std::make_unique<bake::BlobWriteSharing>();
        std::optional<bake::BakePath> path = bake::get_node_bake_path(bmain, *object, *nmd, id);
        if (!path) {
          continue;
        }
        std::optional<IndexRange> frame_range = bake::get_node_bake_frame_range(
            scene, *object, *nmd, id);
        if (!frame_range) {
          continue;
        }
        request.path = std::move(*path);
        request.frame_start = frame_range->first();
        request.frame_end = frame_range->last();

        requests.append(std::move(request));
      }
    }
  }
  return requests;
}

static Vector<NodeBakeRequest> bake_simulation_gather_requests(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Main *bmain = CTX_data_main(C);

  Vector<Object *> objects;
  if (RNA_boolean_get(op->ptr, "selected")) {
    CTX_DATA_BEGIN (C, Object *, object, selected_objects) {
      objects.append(object);
    }
    CTX_DATA_END;
  }
  else {
    if (Object *object = CTX_data_active_object(C)) {
      objects.append(object);
    }
  }

  return collect_simulations_to_bake(*bmain, *scene, objects);
}

static int bake_simulation_exec(bContext *C, wmOperator *op)
{
  Vector<NodeBakeRequest> requests = bake_simulation_gather_requests(C, op);
  return start_bake_job(C, std::move(requests), op, BakeRequestsMode::Sync);
}

struct PathStringHash {
  uint64_t operator()(const StringRef s) const
  {
    /* Normalize the paths so we can compare them. */
    DynamicStackBuffer<256> norm_buf(s.size() + 1, 8);
    memcpy(norm_buf.buffer(), s.data(), s.size() + 1);
    char *norm = static_cast<char *>(norm_buf.buffer());

    BLI_path_slash_native(norm);

    /* Strip ending slash. */
    BLI_path_slash_rstrip(norm);

    BLI_path_normalize(norm);
    return get_default_hash(norm);
  }
};

struct PathStringEquality {
  bool operator()(const StringRef a, const StringRef b) const
  {
    return BLI_path_cmp_normalized(a.data(), b.data()) == 0;
  }
};

static bool bake_directory_has_data(const StringRefNull absolute_bake_dir)
{
  char meta_dir[FILE_MAX];
  BLI_path_join(meta_dir, sizeof(meta_dir), absolute_bake_dir.c_str(), "meta");
  char blobs_dir[FILE_MAX];
  BLI_path_join(blobs_dir, sizeof(blobs_dir), absolute_bake_dir.c_str(), "blobs");

  if (!BLI_is_dir(meta_dir) || !BLI_is_dir(blobs_dir)) {
    return false;
  }

  return true;
}

static void bake_simulation_validate_paths(bContext *C,
                                           wmOperator *op,
                                           const Span<Object *> objects)
{
  Main *bmain = CTX_data_main(C);

  for (Object *object : objects) {
    if (!BKE_id_is_editable(bmain, &object->id)) {
      continue;
    }

    LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
      if (md->type != eModifierType_Nodes) {
        continue;
      }

      NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
      if (StringRef(nmd->bake_directory).is_empty()) {
        BKE_reportf(op->reports,
                    RPT_INFO,
                    "Bake directory of object %s, modifier %s is empty, setting default path",
                    object->id.name + 2,
                    md->name);

        nmd->bake_directory = BLI_strdup(
            bake::get_default_modifier_bake_directory(*bmain, *object, *nmd).c_str());
      }
    }
  }
}

/* Map for counting path references. */
using PathUsersMap = Map<std::string,
                         int,
                         default_inline_buffer_capacity(sizeof(std::string)),
                         DefaultProbingStrategy,
                         PathStringHash,
                         PathStringEquality>;

static PathUsersMap bake_simulation_get_path_users(bContext *C, const Span<Object *> objects)
{
  Main *bmain = CTX_data_main(C);

  PathUsersMap path_users;
  for (const Object *object : objects) {
    const char *base_path = ID_BLEND_PATH(bmain, &object->id);

    LISTBASE_FOREACH (const ModifierData *, md, &object->modifiers) {
      if (md->type != eModifierType_Nodes) {
        continue;
      }
      const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
      if (StringRef(nmd->bake_directory).is_empty()) {
        continue;
      }

      char absolute_bake_dir[FILE_MAX];
      STRNCPY(absolute_bake_dir, nmd->bake_directory);
      BLI_path_abs(absolute_bake_dir, base_path);
      path_users.add_or_modify(
          absolute_bake_dir, [](int *value) { *value = 1; }, [](int *value) { ++(*value); });
    }
  }

  return path_users;
}

static int bake_simulation_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Vector<Object *> objects;
  if (RNA_boolean_get(op->ptr, "selected")) {
    CTX_DATA_BEGIN (C, Object *, object, selected_objects) {
      objects.append(object);
    }
    CTX_DATA_END;
  }
  else {
    if (Object *object = CTX_data_active_object(C)) {
      objects.append(object);
    }
  }

  /* Set empty paths to default. */
  bake_simulation_validate_paths(C, op, objects);

  PathUsersMap path_users = bake_simulation_get_path_users(C, objects);
  bool has_path_conflict = false;
  bool has_existing_bake_data = false;
  for (const auto &item : path_users.items()) {
    /* Check if multiple caches are writing to the same bake directory. */
    if (item.value > 1) {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Path conflict: %d caches set to path %s",
                  item.value,
                  item.key.data());
      has_path_conflict = true;
    }

    /* Check if path exists and contains bake data already. */
    if (bake_directory_has_data(item.key.data())) {
      has_existing_bake_data = true;
    }
  }

  if (has_path_conflict) {
    UI_popup_menu_reports(C, op->reports);
    return OPERATOR_CANCELLED;
  }
  if (has_existing_bake_data) {
    return WM_operator_confirm_message(C, op, "Overwrite existing bake data");
  }
  Vector<NodeBakeRequest> requests = bake_simulation_gather_requests(C, op);
  return start_bake_job(C, std::move(requests), op, BakeRequestsMode::Async);
}

static int bake_simulation_modal(bContext *C, wmOperator * /*op*/, const wmEvent * /*event*/)
{
  if (!WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_BAKE_GEOMETRY_NODES)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }
  return OPERATOR_PASS_THROUGH;
}

static void try_delete_bake(
    bContext *C, Object &object, NodesModifierData &nmd, const int bake_id, ReportList *reports)
{
  Main *bmain = CTX_data_main(C);
  if (!nmd.runtime->cache) {
    return;
  }
  bake::ModifierCache &modifier_cache = *nmd.runtime->cache;
  std::lock_guard lock{modifier_cache.mutex};
  if (auto *node_cache = modifier_cache.simulation_cache_by_id.lookup_ptr(bake_id)) {
    (*node_cache)->reset();
  }
  else if (auto *node_cache = modifier_cache.bake_cache_by_id.lookup_ptr(bake_id)) {
    (*node_cache)->reset();
  }
  NodesModifierBake *bake = nmd.find_bake(bake_id);
  if (!bake) {
    return;
  }
  dna::array::clear<NodesModifierDataBlock>(&bake->data_blocks,
                                            &bake->data_blocks_num,
                                            &bake->active_data_block,
                                            [](NodesModifierDataBlock *data_block) {
                                              nodes_modifier_data_block_destruct(data_block, true);
                                            });

  const std::optional<bake::BakePath> bake_path = bake::get_node_bake_path(
      *bmain, object, nmd, bake_id);
  if (!bake_path) {
    return;
  }
  const char *meta_dir = bake_path->meta_dir.c_str();
  if (BLI_exists(meta_dir)) {
    if (BLI_delete(meta_dir, true, true)) {
      BKE_reportf(reports, RPT_ERROR, "Failed to remove metadata directory %s", meta_dir);
    }
  }
  const char *blobs_dir = bake_path->blobs_dir.c_str();
  if (BLI_exists(blobs_dir)) {
    if (BLI_delete(blobs_dir, true, true)) {
      BKE_reportf(reports, RPT_ERROR, "Failed to remove blobs directory %s", blobs_dir);
    }
  }
  if (bake_path->bake_dir.has_value()) {
    const char *zone_bake_dir = bake_path->bake_dir->c_str();
    /* Try to delete zone bake directory if it is empty. */
    BLI_delete(zone_bake_dir, true, false);
  }
  if (const std::optional<std::string> modifier_bake_dir = bake::get_modifier_bake_path(
          *bmain, object, nmd))
  {
    /* Try to delete modifier bake directory if it is empty. */
    BLI_delete(modifier_bake_dir->c_str(), true, false);
  }
}

static int delete_baked_simulation_exec(bContext *C, wmOperator *op)
{
  Vector<Object *> objects;
  if (RNA_boolean_get(op->ptr, "selected")) {
    CTX_DATA_BEGIN (C, Object *, object, selected_objects) {
      objects.append(object);
    }
    CTX_DATA_END;
  }
  else {
    if (Object *object = CTX_data_active_object(C)) {
      objects.append(object);
    }
  }

  if (objects.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  for (Object *object : objects) {
    LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
      if (md->type == eModifierType_Nodes) {
        NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
        for (const NodesModifierBake &bake : Span(nmd->bakes, nmd->bakes_num)) {
          try_delete_bake(C, *object, *nmd, bake.id, op->reports);
        }
      }
    }

    DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, nullptr);

  return OPERATOR_FINISHED;
}

static Vector<NodeBakeRequest> bake_single_node_gather_bake_request(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *object = reinterpret_cast<Object *>(
      WM_operator_properties_id_lookup_from_name_or_session_uid(bmain, op->ptr, ID_OB));
  if (object == nullptr) {
    return {};
  }
  char *modifier_name = RNA_string_get_alloc(op->ptr, "modifier_name", nullptr, 0, nullptr);
  if (modifier_name == nullptr) {
    return {};
  }
  BLI_SCOPED_DEFER([&]() { MEM_SAFE_FREE(modifier_name); });

  ModifierData *md = BKE_modifiers_findby_name(object, modifier_name);
  if (md == nullptr) {
    return {};
  }
  NodesModifierData &nmd = *reinterpret_cast<NodesModifierData *>(md);
  if (nmd.node_group == nullptr) {
    return {};
  }
  if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
    BKE_report(op->reports, RPT_ERROR, "Modifier containing the node is disabled");
    return {};
  }

  if (StringRef(nmd.bake_directory).is_empty()) {
    const std::string directory = bake::get_default_modifier_bake_directory(*bmain, *object, nmd);
    nmd.bake_directory = BLI_strdup(directory.c_str());
  }

  const int bake_id = RNA_int_get(op->ptr, "bake_id");
  const bNode *node = nmd.node_group->find_nested_node(bake_id);
  if (node == nullptr) {
    return {};
  }
  if (!ELEM(node->type, GEO_NODE_SIMULATION_OUTPUT, GEO_NODE_BAKE)) {
    return {};
  }

  NodeBakeRequest request;
  request.object = object;
  request.nmd = &nmd;
  request.bake_id = bake_id;
  request.node_type = node->type;
  request.blob_sharing = std::make_unique<bake::BlobWriteSharing>();

  const NodesModifierBake *bake = nmd.find_bake(bake_id);
  if (!bake) {
    return {};
  }
  const std::optional<bake::BakePath> bake_path = bake::get_node_bake_path(
      *bmain, *object, nmd, bake_id);
  if (!bake_path.has_value()) {
    BKE_report(op->reports, RPT_ERROR, "Cannot determine bake location on disk");
    return {};
  }
  request.path = std::move(*bake_path);

  if (bake->bake_mode == NODES_MODIFIER_BAKE_MODE_STILL) {
    const int current_frame = scene->r.cfra;
    request.frame_start = current_frame;
    request.frame_end = current_frame;
    /* Delete old bake because otherwise this wouldn't be a still frame bake. This is not done for
     * other bakes to avoid loosing data when starting a bake. */
    try_delete_bake(C, *object, nmd, bake_id, op->reports);
  }
  else {
    const std::optional<IndexRange> frame_range = bake::get_node_bake_frame_range(
        *scene, *object, nmd, bake_id);
    if (!frame_range.has_value()) {
      return {};
    }
    if (frame_range->is_empty()) {
      return {};
    }
    request.frame_start = frame_range->first();
    request.frame_end = frame_range->last();
  }

  Vector<NodeBakeRequest> requests;
  requests.append(std::move(request));
  return requests;
}

static int bake_single_node_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Vector<NodeBakeRequest> requests = bake_single_node_gather_bake_request(C, op);
  if (requests.is_empty()) {
    return OPERATOR_CANCELLED;
  }
  return start_bake_job(C, std::move(requests), op, BakeRequestsMode::Async);
}

static int bake_single_node_exec(bContext *C, wmOperator *op)
{
  Vector<NodeBakeRequest> requests = bake_single_node_gather_bake_request(C, op);
  if (requests.is_empty()) {
    return OPERATOR_CANCELLED;
  }
  return start_bake_job(C, std::move(requests), op, BakeRequestsMode::Sync);
}

static int bake_single_node_modal(bContext *C, wmOperator * /*op*/, const wmEvent * /*event*/)
{
  if (!WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_BAKE_GEOMETRY_NODES)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }
  return OPERATOR_PASS_THROUGH;
}

static int delete_single_bake_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *object = reinterpret_cast<Object *>(
      WM_operator_properties_id_lookup_from_name_or_session_uid(bmain, op->ptr, ID_OB));
  if (object == nullptr) {
    return OPERATOR_CANCELLED;
  }
  char *modifier_name = RNA_string_get_alloc(op->ptr, "modifier_name", nullptr, 0, nullptr);
  if (modifier_name == nullptr) {
    return OPERATOR_CANCELLED;
  }
  BLI_SCOPED_DEFER([&]() { MEM_SAFE_FREE(modifier_name); });

  ModifierData *md = BKE_modifiers_findby_name(object, modifier_name);
  if (md == nullptr) {
    return OPERATOR_CANCELLED;
  }
  NodesModifierData &nmd = *reinterpret_cast<NodesModifierData *>(md);
  const int bake_id = RNA_int_get(op->ptr, "bake_id");

  try_delete_bake(C, *object, nmd, bake_id, op->reports);

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, nullptr);
  WM_main_add_notifier(NC_NODE, nullptr);
  return OPERATOR_FINISHED;
}

static bool bake_poll(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  if (BKE_main_blendfile_path(bmain)[0] == '\0') {
    /* Saving the .blend file is not technically necessary in all cases but only when the bake path
     * depends on the .blend file path (which is the case by default). */
    CTX_wm_operator_poll_msg_set(C, "File must be saved before baking");
    return false;
  }
  return true;
}

static bool bake_delete_poll(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  if (BKE_main_blendfile_path(bmain)[0] == '\0') {
    return false;
  }
  return true;
}

}  // namespace blender::ed::object::bake_simulation

void OBJECT_OT_simulation_nodes_cache_calculate_to_frame(wmOperatorType *ot)
{
  using namespace blender::ed::object::bake_simulation;

  ot->name = "Calculate Simulation to Frame";
  ot->description =
      "Calculate simulations in geometry nodes modifiers from the start to current frame";
  ot->idname = __func__;

  ot->invoke = simulate_to_frame_invoke;
  ot->modal = simulate_to_frame_modal;
  ot->poll = simulate_to_frame_poll;

  RNA_def_boolean(ot->srna,
                  "selected",
                  false,
                  "Selected",
                  "Calculate all selected objects instead of just the active object");
}

void OBJECT_OT_simulation_nodes_cache_bake(wmOperatorType *ot)
{
  using namespace blender::ed::object::bake_simulation;

  ot->name = "Bake Simulation";
  ot->description = "Bake simulations in geometry nodes modifiers";
  ot->idname = __func__;

  ot->exec = bake_simulation_exec;
  ot->invoke = bake_simulation_invoke;
  ot->modal = bake_simulation_modal;
  ot->poll = bake_simulation_poll;

  RNA_def_boolean(ot->srna, "selected", false, "Selected", "Bake cache on all selected objects");
}

void OBJECT_OT_simulation_nodes_cache_delete(wmOperatorType *ot)
{
  using namespace blender::ed::object::bake_simulation;

  ot->name = "Delete Cached Simulation";
  ot->description = "Delete cached/baked simulations in geometry nodes modifiers";
  ot->idname = __func__;

  ot->exec = delete_baked_simulation_exec;
  ot->poll = ED_operator_object_active;

  RNA_def_boolean(ot->srna, "selected", false, "Selected", "Delete cache on all selected objects");
}

static void single_bake_operator_props(wmOperatorType *ot)
{
  WM_operator_properties_id_lookup(ot, false);

  RNA_def_string(ot->srna,
                 "modifier_name",
                 nullptr,
                 0,
                 "Modifier Name",
                 "Name of the modifier that contains the node");
  RNA_def_int(
      ot->srna, "bake_id", 0, 0, INT32_MAX, "Bake ID", "Nested node id of the node", 0, INT32_MAX);
}

void OBJECT_OT_geometry_node_bake_single(wmOperatorType *ot)
{
  using namespace blender::ed::object::bake_simulation;

  ot->name = "Bake Geometry Node";
  ot->description = "Bake a single bake node or simulation";
  ot->idname = "OBJECT_OT_geometry_node_bake_single";

  ot->poll = bake_poll;
  ot->invoke = bake_single_node_invoke;
  ot->exec = bake_single_node_exec;
  ot->modal = bake_single_node_modal;

  single_bake_operator_props(ot);
}

void OBJECT_OT_geometry_node_bake_delete_single(wmOperatorType *ot)
{
  using namespace blender::ed::object::bake_simulation;

  ot->name = "Delete Geometry Node Bake";
  ot->description = "Delete baked data of a single bake node or simulation";
  ot->idname = "OBJECT_OT_geometry_node_bake_delete_single";

  ot->poll = bake_delete_poll;
  ot->exec = delete_single_bake_exec;

  single_bake_operator_props(ot);
}
