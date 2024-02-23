/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "final_engine.hh"
#include "camera.hh"

#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/renderBuffer.h>

#include "DNA_scene_types.h"

#include "BLI_time.h"
#include "BLI_timecode.h"

#include "BKE_lib_id.hh"

#include "DEG_depsgraph_query.hh"

#include "IMB_imbuf_types.hh"

#include "RE_engine.h"

namespace blender::render::hydra {

void FinalEngine::render()
{
  const ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph_);

  char scene_name[MAX_ID_FULL_NAME];
  BKE_id_full_name_get(scene_name, &scene_->id, 0);

  const RenderData &r = scene_->r;
  pxr::GfVec4f border(0, 0, 1, 1);
  if (r.mode & R_BORDER) {
    border.Set(r.border.xmin,
               r.border.ymin,
               r.border.xmax - r.border.xmin,
               r.border.ymax - r.border.ymin);
  }
  pxr::GfVec2i image_res(r.xsch * r.size / 100, r.ysch * r.size / 100);
  int width = image_res[0] * border[2];
  int height = image_res[1] * border[3];

  pxr::GfCamera camera = gf_camera(scene_->camera, image_res, border);

  free_camera_delegate_->SetCamera(camera);
  render_task_delegate_->set_viewport(pxr::GfVec4d(0, 0, width, height));
  if (light_tasks_delegate_) {
    light_tasks_delegate_->set_viewport(pxr::GfVec4d(0, 0, width, height));
  }

  RenderResult *rr = RE_engine_get_result(bl_engine_);
  RenderLayer *rlayer = (RenderLayer *)rr->layers.first;
  LISTBASE_FOREACH (RenderPass *, rpass, &rlayer->passes) {
    pxr::TfToken *aov_token = aov_tokens_.lookup_ptr(rpass->name);
    if (!aov_token) {
      CLOG_WARN(LOG_HYDRA_RENDER, "Couldn't find AOV token for render pass: %s", rpass->name);
      continue;
    }
    render_task_delegate_->add_aov(*aov_token);
  }
  if (bl_engine_->type->flag & RE_USE_GPU_CONTEXT) {
    /* For GPU context engine color and depth AOVs has to be added anyway */
    render_task_delegate_->add_aov(pxr::HdAovTokens->color);
    render_task_delegate_->add_aov(pxr::HdAovTokens->depth);
  }

  render_task_delegate_->bind();

  auto t = tasks();
  engine_->Execute(render_index_.get(), &t);

  char elapsed_time[32];
  double time_begin = BLI_time_now_seconds();
  float percent_done = 0.0;

  while (true) {
    if (RE_engine_test_break(bl_engine_)) {
      break;
    }

    percent_done = renderer_percent_done();
    BLI_timecode_string_from_time_simple(
        elapsed_time, sizeof(elapsed_time), BLI_time_now_seconds() - time_begin);
    notify_status(percent_done / 100.0,
                  std::string(scene_name) + ": " + view_layer->name,
                  std::string("Render Time: ") + elapsed_time +
                      " | Done: " + std::to_string(int(percent_done)) + "%");

    if (render_task_delegate_->is_converged()) {
      break;
    }

    update_render_result(width, height, view_layer->name);
  }

  update_render_result(width, height, view_layer->name);
  render_task_delegate_->unbind();
}

void FinalEngine::set_render_setting(const std::string &key, const pxr::VtValue &val)
{
  if (STRPREFIX(key.c_str(), "aovToken:")) {
    aov_tokens_.add_overwrite(key.substr(key.find(":") + 1),
                              pxr::TfToken(val.UncheckedGet<std::string>()));
    return;
  }
  Engine::set_render_setting(key, val);
}

void FinalEngine::notify_status(float progress, const std::string &title, const std::string &info)
{
  RE_engine_update_progress(bl_engine_, progress);
  RE_engine_update_stats(bl_engine_, title.c_str(), info.c_str());
}

void FinalEngine::update_render_result(int width, int height, const char *layer_name)
{
  RenderResult *rr = RE_engine_begin_result(bl_engine_, 0, 0, width, height, layer_name, nullptr);

  RenderLayer *rlayer = static_cast<RenderLayer *>(
      BLI_findstring(&rr->layers, layer_name, offsetof(RenderLayer, name)));

  if (rlayer) {
    LISTBASE_FOREACH (RenderPass *, rpass, &rlayer->passes) {
      pxr::TfToken *aov_token = aov_tokens_.lookup_ptr(rpass->name);
      if (aov_token) {
        render_task_delegate_->read_aov(*aov_token, rpass->ibuf->float_buffer.data);
      }
    }
  }

  RE_engine_end_result(bl_engine_, rr, false, false, false);
}

}  // namespace blender::render::hydra
