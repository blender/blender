/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Outline Effect:
 *
 * Simple effect that just samples an object id buffer to detect objects outlines.
 */

#include "workbench_private.hh"

namespace blender::workbench {

OutlinePass::~OutlinePass()
{
  DRW_SHADER_FREE_SAFE(sh_);
}

void OutlinePass::init(const SceneState &scene_state)
{
  enabled_ = scene_state.draw_outline;
  if (!enabled_) {
    return;
  }

  if (sh_ == nullptr) {
    sh_ = GPU_shader_create_from_info_name("workbench_effect_outline");
  }
}

void OutlinePass::sync(SceneResources &resources)
{
  if (!enabled_) {
    return;
  }

  ps_.init();
  ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL);
  ps_.shader_set(sh_);
  ps_.bind_ubo("world_data", resources.world_buf);
  ps_.bind_texture("objectIdBuffer", &resources.object_id_tx);
  ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void OutlinePass::draw(Manager &manager, SceneResources &resources)
{
  if (!enabled_) {
    return;
  }

  fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(resources.color_tx));
  fb_.bind();
  manager.submit(ps_);
}

}  // namespace blender::workbench
