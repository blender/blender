/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"

#include "GPU_capabilities.hh"

#include "IMB_imbuf.hh"

#include "BKE_callbacks.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_scene.hh"
#include "BKE_scene_runtime.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_debug.hh"
#include "DEG_depsgraph_query.hh"

#include "RE_compositor.hh"

#include "ED_image.hh"
#include "ED_node.hh"
#include "ED_screen.hh"

#include "COM_node_group_operation.hh"
#include "COM_profiler.hh"

namespace blender {

struct CompositorJob {
  Main *bmain;
  Scene *scene;
  ViewLayer *view_layer;
  bNodeTree *evaluated_node_tree;
  Render *render;
  compositor::Profiler profiler;
  compositor::NodeGroupOutputTypes needed_outputs;
  bool is_animation_playing;
};

static void compositor_job_init(void *compositor_job_data)
{
  CompositorJob *compositor_job = static_cast<CompositorJob *>(compositor_job_data);

  Main *bmain = compositor_job->bmain;
  Scene *scene = compositor_job->scene;
  ViewLayer *view_layer = compositor_job->view_layer;

  bke::CompositorRuntime &compositor_runtime = scene->runtime->compositor;

  if (!compositor_runtime.preview_depsgraph) {
    compositor_runtime.preview_depsgraph = DEG_graph_new(
        bmain, scene, view_layer, DAG_EVAL_RENDER);
    DEG_debug_name_set(compositor_runtime.preview_depsgraph, "COMPOSITOR");
  }

  /* Update the viewer layer of the compositor if it changed since the depsgraph was created. */
  if (DEG_get_input_view_layer(compositor_runtime.preview_depsgraph) != view_layer) {
    DEG_graph_replace_owners(compositor_runtime.preview_depsgraph, bmain, scene, view_layer);
    DEG_graph_tag_relations_update(compositor_runtime.preview_depsgraph);
  }

  DEG_graph_build_for_compositor_preview(compositor_runtime.preview_depsgraph);

  /* NOTE: Don't update animation to preserve unkeyed changes, this means can not use
   * evaluate_on_framechange. */
  DEG_evaluate_on_refresh(compositor_runtime.preview_depsgraph);

  compositor_job->evaluated_node_tree = DEG_get_evaluated(compositor_runtime.preview_depsgraph,
                                                          scene->compositing_node_group);

  compositor_job->render = RE_NewInteractiveCompositorRender(scene);
  if (scene->r.compositor_device == SCE_COMPOSITOR_DEVICE_GPU) {
    RE_display_ensure_gpu_context(compositor_job->render);
    IMB_ensure_gpu_context();
  }
}

static void compositor_job_start(void *compositor_job_data, wmJobWorkerStatus *worker_status)
{
  CompositorJob *compositor_job = static_cast<CompositorJob *>(compositor_job_data);

  /* If animation is playing, do not respect the job worker stop status, because if the job for the
   * current frame did not finish before the next frame's job is scheduled, it will be stopped in
   * favor of the new frame, and this will likely happen for all future frame jobs so we will be
   * essentially doing nothing. So we just prefer to finish the job at hand and ignore the future
   * jobs. This will appear to be frame-dropping for the user. */
  if (compositor_job->is_animation_playing) {
    RE_test_break_cb(
        compositor_job->render, nullptr, [](void * /*handle*/) -> bool { return G.is_break; });
  }
  else {
    RE_test_break_cb(compositor_job->render, &worker_status->stop, [](void *should_stop) -> bool {
      return *static_cast<bool *>(should_stop) || G.is_break;
    });
  }

  BKE_callback_exec_id(
      compositor_job->bmain, &compositor_job->scene->id, BKE_CB_EVT_COMPOSITE_PRE);

  bke::CompositorRuntime &compositor_runtime = compositor_job->scene->runtime->compositor;
  Scene *evaluated_scene = DEG_get_evaluated_scene(compositor_runtime.preview_depsgraph);
  if (!(evaluated_scene->r.scemode & R_MULTIVIEW)) {
    RE_compositor_execute(*compositor_job->render,
                          *evaluated_scene,
                          evaluated_scene->r,
                          *compositor_job->evaluated_node_tree,
                          "",
                          nullptr,
                          &compositor_job->profiler,
                          compositor_job->needed_outputs);
  }
  else {
    for (SceneRenderView &scene_render_view : evaluated_scene->r.views) {
      if (!BKE_scene_multiview_is_render_view_active(&evaluated_scene->r, &scene_render_view)) {
        continue;
      }
      RE_compositor_execute(*compositor_job->render,
                            *evaluated_scene,
                            evaluated_scene->r,
                            *compositor_job->evaluated_node_tree,
                            scene_render_view.name,
                            nullptr,
                            &compositor_job->profiler,
                            compositor_job->needed_outputs);
    }
  }
}

static void compositor_job_complete(void *compositor_job_data)
{
  CompositorJob *compositor_job = static_cast<CompositorJob *>(compositor_job_data);

  Scene *scene = compositor_job->scene;
  BKE_callback_exec_id(compositor_job->bmain, &scene->id, BKE_CB_EVT_COMPOSITE_POST);

  bke::node_preview_merge_tree(
      scene->compositing_node_group, compositor_job->evaluated_node_tree, true);
  scene->runtime->compositor.per_node_execution_time =
      compositor_job->profiler.get_nodes_evaluation_times();
  WM_main_add_notifier(NC_SCENE | ND_COMPO_RESULT, nullptr);
}

static void compositor_job_cancel(void *compositor_job_data)
{
  CompositorJob *compositor_job = static_cast<CompositorJob *>(compositor_job_data);

  /* If animation is playing, jobs can only be canceled by the user, that is, through G.is_break,
   * so if we are not breaked, consider the job to be complete. See comment in compositor_job_start
   * breaking callbacks. */
  if (compositor_job->is_animation_playing && !G.is_break) {
    compositor_job_complete(compositor_job);
    return;
  }

  Scene *scene = compositor_job->scene;
  BKE_callback_exec_id(compositor_job->bmain, &scene->id, BKE_CB_EVT_COMPOSITE_CANCEL);
}

static void compositor_job_free(void *compositor_job_data)
{
  MEM_delete(static_cast<CompositorJob *>(compositor_job_data));
}

static bool is_compositing_possible(const bContext *C)
{
  if (G.is_rendering) {
    return false;
  }

  Scene *scene = CTX_data_scene(C);
  if (!scene->compositing_node_group) {
    return false;
  }

  /* CPU compositor can always run. */
  if (scene->r.compositor_device != SCE_COMPOSITOR_DEVICE_GPU) {
    return true;
  }

  /* The render size exceeds what can be allocated as a GPU texture. */
  int width, height;
  BKE_render_resolution(&scene->r, false, &width, &height);
  if (!GPU_is_safe_texture_size(width, height)) {
    WM_global_report(RPT_ERROR, "Render size too large for GPU, use CPU compositor instead");
    return false;
  }

  return true;
}

/* Returns the compositor outputs that need to be computed because their result is visible to the
 * user or required by the render pipeline. */
static compositor::NodeGroupOutputTypes get_compositor_needed_outputs(const bContext *C)
{
  compositor::NodeGroupOutputTypes needed_outputs = compositor::NodeGroupOutputTypes::None;

  Scene *scene = CTX_data_scene(C);
  wmWindowManager *window_manager = CTX_wm_manager(C);
  for (wmWindow &window : window_manager->windows) {
    bScreen *screen = WM_window_get_active_screen(&window);
    for (ScrArea &area : screen->areabase) {
      SpaceLink *space_link = static_cast<SpaceLink *>(area.spacedata.first);
      if (!space_link || !ELEM(space_link->spacetype, SPACE_NODE, SPACE_IMAGE)) {
        continue;
      }
      if (space_link->spacetype == SPACE_NODE) {
        const SpaceNode *space_node = reinterpret_cast<const SpaceNode *>(space_link);
        if (space_node->flag & SNODE_BACKDRAW) {
          needed_outputs |= compositor::NodeGroupOutputTypes::ViewerNode;
        }
        if (space_node->overlay.flag & SN_OVERLAY_SHOW_PREVIEWS) {
          needed_outputs |= compositor::NodeGroupOutputTypes::NodePreviews;
        }
      }
      else if (space_link->spacetype == SPACE_IMAGE) {
        const SpaceImage *space_image = reinterpret_cast<const SpaceImage *>(space_link);
        Image *image = ED_space_image(space_image);
        if (!image || image->source != IMA_SRC_VIEWER) {
          continue;
        }
        /* Do not override the Render Result if compositing is disabled in the render pipeline or
         * if the sequencer is enabled. */
        if (image->type == IMA_TYPE_R_RESULT && scene->r.scemode & R_DOCOMP &&
            !RE_seq_render_active(scene, &scene->r))
        {
          needed_outputs |= compositor::NodeGroupOutputTypes::GroupOutputNode;
        }
        else if (image->type == IMA_TYPE_COMPOSITE) {
          needed_outputs |= compositor::NodeGroupOutputTypes::ViewerNode;
        }
      }
      else if (space_link->spacetype == SPACE_SEQ) {
        const SpaceSeq *space_sequencer = reinterpret_cast<const SpaceSeq *>(space_link);
        if (ELEM(space_sequencer->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW)) {
          needed_outputs |= compositor::NodeGroupOutputTypes::ViewerNode;
        }
      }

      /* All outputs are already needed, return early. */
      if (needed_outputs == (compositor::NodeGroupOutputTypes::GroupOutputNode |
                             compositor::NodeGroupOutputTypes::ViewerNode |
                             compositor::NodeGroupOutputTypes::NodePreviews))
      {
        return needed_outputs;
      }
    }
  }

  /* None of the outputs are needed except node previews but they are a secondary output that needs
   * another output to be computed with, so this is practically none. */
  if (needed_outputs == compositor::NodeGroupOutputTypes::NodePreviews) {
    return compositor::NodeGroupOutputTypes::None;
  }

  return needed_outputs;
}

void ED_node_compositor_job(const bContext *C)
{
  if (!is_compositing_possible(C)) {
    return;
  }

  compositor::NodeGroupOutputTypes needed_outputs = get_compositor_needed_outputs(C);
  if (needed_outputs == compositor::NodeGroupOutputTypes::None) {
    return;
  }

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Image *render_result_image = BKE_image_ensure_viewer(bmain, IMA_TYPE_R_RESULT, "Render Result");
  BKE_image_backup_render(scene, render_result_image, false);

  wmJob *job = WM_jobs_get(CTX_wm_manager(C),
                           CTX_wm_window(C),
                           scene,
                           "Compositing...",
                           WM_JOB_EXCL_RENDER | WM_JOB_PROGRESS,
                           WM_JOB_TYPE_COMPOSITE);

  CompositorJob *compositor_job = MEM_new<CompositorJob>("Compositor Job");
  compositor_job->bmain = bmain;
  compositor_job->scene = scene;
  compositor_job->view_layer = CTX_data_view_layer(C);
  compositor_job->needed_outputs = needed_outputs;
  compositor_job->is_animation_playing = ED_window_animation_playing_no_scrub(CTX_wm_manager(C));

  WM_jobs_customdata_set(job, compositor_job, compositor_job_free);
  WM_jobs_timer(job, 0.1, 0, 0);
  WM_jobs_callbacks_ex(job,
                       compositor_job_start,
                       compositor_job_init,
                       nullptr,
                       nullptr,
                       compositor_job_complete,
                       compositor_job_cancel);

  G.is_break = false;
  WM_jobs_start(CTX_wm_manager(C), job);
}

}  // namespace blender
