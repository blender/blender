/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.hh"

#include "GPU_capabilities.hh"

#include "IMB_imbuf.hh"

#include "BKE_callbacks.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"
#include "BKE_scene_runtime.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_debug.hh"
#include "DEG_depsgraph_query.hh"

#include "RE_compositor.hh"

#include "NOD_eval_log.hh"

#include "ED_image.hh"
#include "ED_node.hh"
#include "ED_screen.hh"

#include "COM_node_group_operation.hh"

namespace blender {

struct CompositorJob {
  wmWindowManager *window_manager;
  Main *bmain;
  Scene *scene;
  ViewLayer *view_layer;
  bNodeTree *evaluated_node_tree;
  Render *render;
  compositor::NodeGroupOutputTypes needed_outputs;
  /* Identifies if the compositor is executing due to the user making a modification or if it is
   * executing due to playback or rendering. */
  bool triggered_by_user = false;
};

/* Suspend or resume animation playback if animation is playing. */
static void set_animation_playback(wmWindowManager *window_manager, const bool enabled)
{
  wmWindow *animation_playback_window = ED_window_animation_playing_no_scrub(window_manager);
  if (animation_playback_window) {
    bScreen *screen = WM_window_get_active_screen(animation_playback_window);
    WM_event_timer_sleep(window_manager, animation_playback_window, screen->animtimer, !enabled);
  }
}

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

  /* Suspend animation playback (if any) until the compositor is done to allow frames to be fully
   * processed. */
  set_animation_playback(compositor_job->window_manager, false);
}

static void compositor_job_start(void *compositor_job_data, wmJobWorkerStatus *worker_status)
{
  CompositorJob *compositor_job = static_cast<CompositorJob *>(compositor_job_data);

  RE_test_break_cb(compositor_job->render, &worker_status->stop, [](void *should_stop) -> bool {
    return *static_cast<bool *>(should_stop) || G.is_break;
  });

  BKE_callback_exec_id(
      compositor_job->bmain, &compositor_job->scene->id, BKE_CB_EVT_COMPOSITE_PRE);

  bke::CompositorRuntime &compositor_runtime = compositor_job->scene->runtime->compositor;
  Scene *evaluated_scene = DEG_get_evaluated_scene(compositor_runtime.preview_depsgraph);
  render::CompositorInputData input_data(*compositor_job->render,
                                         *compositor_job->bmain,
                                         *evaluated_scene,
                                         evaluated_scene->r,
                                         *compositor_job->evaluated_node_tree,
                                         "",
                                         nullptr,
                                         compositor_job->needed_outputs,
                                         compositor_job->triggered_by_user);
  if (!(evaluated_scene->r.scemode & R_MULTIVIEW)) {
    RE_compositor_execute(input_data);
  }
  else {
    for (SceneRenderView &scene_render_view : evaluated_scene->r.views) {
      if (!BKE_scene_multiview_is_render_view_active(&evaluated_scene->r, &scene_render_view)) {
        continue;
      }
      input_data.view_name = scene_render_view.name;
      RE_compositor_execute(input_data);
    }
  }
}

static void compositor_job_complete(void *compositor_job_data)
{
  CompositorJob *compositor_job = static_cast<CompositorJob *>(compositor_job_data);

  Scene *scene = compositor_job->scene;
  BKE_callback_exec_id(compositor_job->bmain, &scene->id, BKE_CB_EVT_COMPOSITE_POST);

  Scene *evaluated_scene = DEG_get_evaluated_scene(scene->runtime->compositor.preview_depsgraph);
  scene->runtime->compositor.nodes_evaluation_log = std::move(
      evaluated_scene->runtime->compositor.nodes_evaluation_log);

  WM_main_add_notifier(NC_SCENE | ND_COMPO_RESULT, nullptr);

  /* Resume animation playback (if any) after the compositor is done. */
  set_animation_playback(compositor_job->window_manager, true);
}

static void compositor_job_cancel(void *compositor_job_data)
{
  CompositorJob *compositor_job = static_cast<CompositorJob *>(compositor_job_data);

  Scene *scene = compositor_job->scene;
  BKE_callback_exec_id(compositor_job->bmain, &scene->id, BKE_CB_EVT_COMPOSITE_CANCEL);

  /* Resume animation playback (if any) after the compositor is done. */
  set_animation_playback(compositor_job->window_manager, true);
}

static void compositor_job_free(void *compositor_job_data)
{
  MEM_delete(static_cast<CompositorJob *>(compositor_job_data));
}

static bool is_compositing_possible(const Scene *scene)
{
  if (G.background) {
    return false;
  }

  if (G.is_rendering) {
    return false;
  }

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
  if (width > 8192 || height > 8192) {
    WM_global_report(RPT_ERROR, "Render size too large for GPU, use CPU compositor instead");
    return false;
  }

  return true;
}

/* Returns the compositor outputs that need to be computed because their result is visible to the
 * user or required by the render pipeline. */
static compositor::NodeGroupOutputTypes get_compositor_needed_outputs(
    const wmWindowManager *window_manager, Scene *scene)
{
  if (G.background) {
    return compositor::NodeGroupOutputTypes::None;
  }

  compositor::NodeGroupOutputTypes needed_outputs = compositor::NodeGroupOutputTypes::None;

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

void ED_node_compositor_job(Main *bmain,
                            Scene *scene,
                            ViewLayer *view_layer,
                            const bool triggered_by_user)
{
  if (!is_compositing_possible(scene)) {
    return;
  }

  wmWindowManager *window_manager = static_cast<wmWindowManager *>(bmain->wm.first);
  const compositor::NodeGroupOutputTypes needed_outputs = get_compositor_needed_outputs(
      window_manager, scene);
  if (needed_outputs == compositor::NodeGroupOutputTypes::None) {
    return;
  }

  Image *render_result_image = BKE_image_ensure_viewer(bmain, IMA_TYPE_R_RESULT, "Render Result");
  BKE_image_backup_render(scene, render_result_image, false);

  wmWindow *window = window_manager->runtime->winactive ?
                         window_manager->runtime->winactive :
                         static_cast<wmWindow *>(window_manager->windows.first);
  wmJob *job = WM_jobs_get(window_manager,
                           window,
                           scene,
                           "Compositing...",
                           WM_JOB_EXCL_RENDER | WM_JOB_PROGRESS,
                           WM_JOB_TYPE_COMPOSITE);

  CompositorJob *compositor_job = MEM_new<CompositorJob>("Compositor Job");
  compositor_job->window_manager = window_manager;
  compositor_job->bmain = bmain;
  compositor_job->scene = scene;
  compositor_job->view_layer = view_layer;
  compositor_job->needed_outputs = needed_outputs;
  compositor_job->triggered_by_user = triggered_by_user;

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
  WM_jobs_start(window_manager, job);
}

}  // namespace blender
