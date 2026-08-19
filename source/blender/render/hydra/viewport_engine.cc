/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "viewport_engine.hh"
#include "camera.hh"

#include <pxr/base/gf/camera.h>
#include <pxr/imaging/glf/drawTarget.h>
#include <pxr/usd/usdGeom/camera.h>

#include "DNA_camera_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_vec_types.h" /* This include must be before `BKE_camera.h` due to `rctf` type. */
#include "DNA_view3d_types.h"

#include "BLI_math_matrix_c.hh"
#include "BLI_time.hh"
#include "BLI_timecode.hh"

#include "BKE_camera.h"
#include "BKE_context.hh"

#include "GPU_matrix.hh"
#include "GPU_texture.hh"

#include "DEG_depsgraph_query.hh"

#include "RE_engine.h"

namespace blender::render::hydra {

struct ViewSettings {
  int screen_width;
  int screen_height;
  pxr::GfVec4i border;
  pxr::GfCamera camera;

  ViewSettings(bContext *context);

  int width();
  int height();
};

ViewSettings::ViewSettings(bContext *context)
{
  View3D *view3d = CTX_wm_view3d(context);
  RegionView3D *region_data = static_cast<RegionView3D *>(CTX_wm_region_data(context));
  ARegion *region = CTX_wm_region(context);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(context);
  Scene *scene = DEG_get_evaluated_scene(depsgraph);

  screen_width = region->winx;
  screen_height = region->winy;

  /* Apply the render border. */
  int x1 = 0, y1 = 0;
  int x2 = screen_width, y2 = screen_height;

  rctf border_rect;
  if (BKE_camera_view_render_border(scene,
                                    depsgraph,
                                    view3d,
                                    region_data,
                                    screen_width,
                                    screen_height,
                                    &border_rect,
                                    nullptr))
  {
    x1 = std::max(std::min(int(border_rect.xmin), screen_width), 0);
    x2 = std::max(std::min(int(border_rect.xmax), screen_width), 0);
    y1 = std::max(std::min(int(border_rect.ymin), screen_height), 0);
    y2 = std::max(std::min(int(border_rect.ymax), screen_height), 0);
  }

  border = pxr::GfVec4i(x1, y1, x2, y2);

  camera = gf_camera(depsgraph,
                     view3d,
                     region,
                     pxr::GfVec4f(float(border[0]) / screen_width,
                                  float(border[1]) / screen_height,
                                  float(width()) / screen_width,
                                  float(height()) / screen_height));
}

int ViewSettings::width()
{
  return border[2] - border[0];
}

int ViewSettings::height()
{
  return border[3] - border[1];
}

DrawTexture::DrawTexture()
{
  float coords[8] = {0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0};

  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  GPU_vertformat_attr_add(&format, "texCoord", gpu::VertAttrType::SFLOAT_32_32);
  gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(*vbo, 4);
  GPU_vertbuf_attr_fill(vbo, 0, coords);
  GPU_vertbuf_attr_fill(vbo, 1, coords);

  batch_ = GPU_batch_create_ex(GPU_PRIM_TRI_FAN, vbo, nullptr, GPU_BATCH_OWNS_VBO);
}

DrawTexture::~DrawTexture()
{
  if (texture_) {
    GPU_texture_free(texture_);
  }
  GPU_batch_discard(batch_);
}

void DrawTexture::create_from_buffer(pxr::HdRenderBuffer *buffer)
{
  if (buffer == nullptr) {
    return;
  }

  gpu::TextureFormat texture_format;
  eGPUDataFormat data_format;

  if (buffer->GetFormat() == pxr::HdFormat::HdFormatFloat16Vec4) {
    texture_format = gpu::TextureFormat::SFLOAT_16_16_16_16;
    data_format = GPU_DATA_HALF_FLOAT;
  }
  else {
    texture_format = gpu::TextureFormat::SFLOAT_32_32_32_32;
    data_format = GPU_DATA_FLOAT;
  }

  if (texture_ && (GPU_texture_width(texture_) != buffer->GetWidth() ||
                   GPU_texture_height(texture_) != buffer->GetHeight() ||
                   GPU_texture_format(texture_) != texture_format))
  {
    GPU_texture_free(texture_);
    texture_ = nullptr;
  }

  if (texture_ == nullptr) {
    texture_ = GPU_texture_create_2d("tex_hydra_render_viewport",
                                     buffer->GetWidth(),
                                     buffer->GetHeight(),
                                     1,
                                     texture_format,
                                     GPU_TEXTURE_USAGE_GENERAL,
                                     nullptr);
  }

  void *data = buffer->Map();
  GPU_texture_update(texture_, data_format, data);
  buffer->Unmap();
}

void DrawTexture::draw(gpu::Shader *shader, const pxr::GfVec4d &viewport, gpu::Texture *tex)
{
  if (!tex) {
    tex = texture_;
  }
  int slot = GPU_shader_get_sampler_binding(shader, "image");
  GPU_texture_bind(tex, slot);
  GPU_shader_uniform_1i(shader, "image", slot);

  GPU_matrix_push();
  GPU_matrix_translate_2f(viewport[0], viewport[1]);
  GPU_matrix_scale_2f(viewport[2] - viewport[0], viewport[3] - viewport[1]);
  GPU_batch_set_shader(batch_, shader);
  GPU_batch_draw(batch_);
  GPU_matrix_pop();
}

gpu::Texture *DrawTexture::texture() const
{
  return texture_;
}

void ViewportEngine::render()
{
  ViewSettings view_settings(context_);
  if (view_settings.width() * view_settings.height() == 0) {
    return;
  };

  free_camera_delegate_->SetCamera(view_settings.camera);

  pxr::GfVec4d viewport(0.0, 0.0, view_settings.width(), view_settings.height());
  render_task_delegate_->set_viewport(viewport);
  if (light_tasks_delegate_) {
    light_tasks_delegate_->set_viewport(viewport);
  }

  render_task_delegate_->add_aov(pxr::HdAovTokens->color);
  render_task_delegate_->add_aov(pxr::HdAovTokens->depth);

  gpu::FrameBuffer *view_framebuffer = GPU_framebuffer_active_get();
  render_task_delegate_->bind();

  auto t = tasks();
  engine_->Execute(render_index_.get(), &t);

  render_task_delegate_->unbind();

  GPU_framebuffer_bind(view_framebuffer);
  gpu::Shader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_IMAGE);
  GPU_shader_bind(shader);

  pxr::GfVec4d draw_viewport(view_settings.border[0],
                             view_settings.border[1],
                             view_settings.border[2],
                             view_settings.border[3]);
  GPURenderTaskDelegate *gpu_task = dynamic_cast<GPURenderTaskDelegate *>(
      render_task_delegate_.get());
  if (gpu_task) {
    draw_texture_.draw(shader, draw_viewport, gpu_task->get_aov_texture(pxr::HdAovTokens->color));
  }
  else {
    draw_texture_.create_from_buffer(
        render_task_delegate_->get_aov_buffer(pxr::HdAovTokens->color));
    draw_texture_.draw(shader, draw_viewport);
  }

  GPU_shader_unbind();

  if (renderer_percent_done() == 0.0f) {
    time_begin_ = BLI_time_now_seconds();
  }

  char elapsed_time[32];

  BLI_timecode_string_from_time_simple(
      elapsed_time, sizeof(elapsed_time), BLI_time_now_seconds() - time_begin_);

  float percent_done = renderer_percent_done();
  if (!render_task_delegate_->is_converged()) {
    notify_status(percent_done / 100.0,
                  std ::string("Time: ") + elapsed_time +
                      " | Done: " + std::to_string(int(percent_done)) + "%",
                  "Render");
    bl_engine_->flag |= RE_ENGINE_DO_DRAW;
  }
  else {
    notify_status(percent_done / 100.0, std::string("Time: ") + elapsed_time, "Rendering Done");
  }
}

void ViewportEngine::render(bContext *context)
{
  context_ = context;
  render();
}

void ViewportEngine::notify_status(float /*progress*/,
                                   const std::string &info,
                                   const std::string &status)
{
  RE_engine_update_stats(bl_engine_, status.c_str(), info.c_str());
}

}  // namespace blender::render::hydra
