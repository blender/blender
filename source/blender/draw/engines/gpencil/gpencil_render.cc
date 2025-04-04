/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_rect.h"

#include "DRW_render.hh"

#include "BKE_object.hh"

#include "DEG_depsgraph_query.hh"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "IMB_imbuf_types.hh"

#include "gpencil_engine_private.hh"

namespace blender::draw::gpencil {

/* Remap depth from views-pace to [0..1] to be able to use it with as GPU depth buffer. */
static void remap_depth(const View &view, MutableSpan<float> pix_z)
{
  if (view.is_persp()) {
    const float4x4 &winmat = view.winmat();
    for (auto &pix : pix_z) {
      pix = (-winmat[3][2] / -pix) - winmat[2][2];
      pix = clamp_f(pix * 0.5f + 0.5f, 0.0f, 1.0f);
    }
  }
  else {
    /* Keep in mind, near and far distance are negatives. */
    const float near = view.near_clip();
    const float far = view.far_clip();
    const float range_inv = 1.0f / fabsf(far - near);
    for (auto &pix : pix_z) {
      pix = (pix + near) * range_inv;
      pix = clamp_f(pix, 0.0f, 1.0f);
    }
  }
}

static void render_set_view(RenderEngine *engine,
                            const Depsgraph *depsgraph,
                            float2 aa_offset = float2{0.0f})
{
  Object *camera = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));

  float4x4 winmat, viewinv;
  RE_GetCameraWindow(engine->re, camera, winmat.ptr());
  RE_GetCameraModelMatrix(engine->re, camera, viewinv.ptr());

  window_translate_m4(winmat.ptr(), winmat.ptr(), UNPACK2(aa_offset));

  View::default_set(math::invert(viewinv), winmat);
}

static void render_init_buffers(const DRWContext *draw_ctx,
                                Instance &inst,
                                RenderEngine *engine,
                                RenderLayer *render_layer,
                                const Depsgraph *depsgraph,
                                const rcti *rect)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  const int2 size = int2(draw_ctx->viewport_size_get());
  View &view = View::default_get();

  /* Create depth texture & color texture from render result. */
  const char *viewname = RE_GetActiveRenderView(engine->re);
  RenderPass *rpass_z_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_Z, viewname);
  RenderPass *rpass_col_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_COMBINED, viewname);

  float *pix_z = (rpass_z_src) ? rpass_z_src->ibuf->float_buffer.data : nullptr;
  float *pix_col = (rpass_col_src) ? rpass_col_src->ibuf->float_buffer.data : nullptr;

  if (!pix_z || !pix_col) {
    RE_engine_set_error_message(engine,
                                "Warning: To render Grease Pencil, enable Combined and Z passes.");
  }

  if (pix_z) {
    /* Depth need to be remapped to [0..1] range. */
    pix_z = static_cast<float *>(MEM_dupallocN(pix_z));
    remap_depth(view, {pix_z, rpass_z_src->rectx * rpass_z_src->recty});
  }

  const bool do_region = (scene->r.mode & R_BORDER) != 0;
  const bool do_clear_z = !pix_z || do_region;
  const bool do_clear_col = !pix_col || do_region;

  /* FIXME(fclem): we have a precision loss in the depth buffer because of this re-upload.
   * Find where it comes from! */
  /* In multi view render the textures can be reused. */
  if (inst.render_depth_tx.is_valid() && !do_clear_z) {
    GPU_texture_update(inst.render_depth_tx, GPU_DATA_FLOAT, pix_z);
  }
  else {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT |
                             GPU_TEXTURE_USAGE_HOST_READ;
    inst.render_depth_tx.ensure_2d(
        GPU_DEPTH_COMPONENT24, int2(size), usage, do_region ? nullptr : pix_z);
  }
  if (inst.render_color_tx.is_valid() && !do_clear_col) {
    GPU_texture_update(inst.render_color_tx, GPU_DATA_FLOAT, pix_col);
  }
  else {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT |
                             GPU_TEXTURE_USAGE_HOST_READ;
    inst.render_color_tx.ensure_2d(GPU_RGBA16F, int2(size), usage, do_region ? nullptr : pix_col);
  }

  inst.render_fb.ensure(GPU_ATTACHMENT_TEXTURE(inst.render_depth_tx),
                        GPU_ATTACHMENT_TEXTURE(inst.render_color_tx));

  if (do_clear_z || do_clear_col) {
    /* To avoid unpredictable result, clear buffers that have not be initialized. */
    GPU_framebuffer_bind(inst.render_fb);
    if (do_clear_col) {
      const float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      GPU_framebuffer_clear_color(inst.render_fb, clear_col);
    }
    if (do_clear_z) {
      GPU_framebuffer_clear_depth(inst.render_fb, 1.0f);
    }
  }

  if (do_region) {
    int x = rect->xmin;
    int y = rect->ymin;
    int w = BLI_rcti_size_x(rect);
    int h = BLI_rcti_size_y(rect);
    if (pix_col) {
      GPU_texture_update_sub(inst.render_color_tx, GPU_DATA_FLOAT, pix_col, x, y, 0, w, h, 0);
    }
    if (pix_z) {
      GPU_texture_update_sub(inst.render_depth_tx, GPU_DATA_FLOAT, pix_z, x, y, 0, w, h, 0);
    }
  }

  MEM_SAFE_FREE(pix_z);
}

static void render_result_z(const DRWContext *draw_ctx,
                            RenderLayer *rl,
                            const char *viewname,
                            Instance &instance,
                            const rcti *rect)
{
  ViewLayer *view_layer = draw_ctx->view_layer;
  if ((view_layer->passflag & SCE_PASS_Z) == 0) {
    return;
  }
  RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_Z, viewname);
  if (rp == nullptr) {
    return;
  }

  float *ro_buffer_data = rp->ibuf->float_buffer.data;

  GPU_framebuffer_read_depth(instance.render_fb,
                             rect->xmin,
                             rect->ymin,
                             BLI_rcti_size_x(rect),
                             BLI_rcti_size_y(rect),
                             GPU_DATA_FLOAT,
                             ro_buffer_data);

  float4x4 winmat = View::default_get().winmat();

  int pix_num = BLI_rcti_size_x(rect) * BLI_rcti_size_y(rect);

  /* Convert GPU depth [0..1] to view Z [near..far] */
  if (View::default_get().is_persp()) {
    for (int i = 0; i < pix_num; i++) {
      if (ro_buffer_data[i] == 1.0f) {
        ro_buffer_data[i] = 1e10f; /* Background */
      }
      else {
        ro_buffer_data[i] = ro_buffer_data[i] * 2.0f - 1.0f;
        ro_buffer_data[i] = winmat[3][2] / (ro_buffer_data[i] + winmat[2][2]);
      }
    }
  }
  else {
    /* Keep in mind, near and far distance are negatives. */
    float near = View::default_get().near_clip();
    float far = View::default_get().far_clip();
    float range = fabsf(far - near);

    for (int i = 0; i < pix_num; i++) {
      if (ro_buffer_data[i] == 1.0f) {
        ro_buffer_data[i] = 1e10f; /* Background */
      }
      else {
        ro_buffer_data[i] = ro_buffer_data[i] * range - near;
      }
    }
  }
}

static void render_result_combined(RenderLayer *rl,
                                   const char *viewname,
                                   Instance &instance,
                                   const rcti *rect)
{
  RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_COMBINED, viewname);

  Framebuffer read_fb;
  read_fb.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(instance.accumulation_tx));
  GPU_framebuffer_bind(read_fb);
  GPU_framebuffer_read_color(read_fb,
                             rect->xmin,
                             rect->ymin,
                             BLI_rcti_size_x(rect),
                             BLI_rcti_size_y(rect),
                             4,
                             0,
                             GPU_DATA_FLOAT,
                             rp->ibuf->float_buffer.data);
}

void Engine::render_to_image(RenderEngine *engine, RenderLayer *render_layer, const rcti rect)
{
  const char *viewname = RE_GetActiveRenderView(engine->re);

  const DRWContext *draw_ctx = DRW_context_get();
  Depsgraph *depsgraph = draw_ctx->depsgraph;

  gpencil::Instance inst;

  Manager &manager = *DRW_manager_get();

  render_set_view(engine, depsgraph);
  render_init_buffers(draw_ctx, inst, engine, render_layer, depsgraph, &rect);
  inst.init();

  inst.camera = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));

  manager.begin_sync();

  /* Loop over all objects and create draw structure. */
  inst.begin_sync();
  DRW_render_object_iter(engine, depsgraph, [&](ObjectRef &ob_ref, RenderEngine *, Depsgraph *) {
    if (!ELEM(ob_ref.object->type, OB_GREASE_PENCIL, OB_LAMP)) {
      return;
    }
    if (!(DRW_object_visibility_in_active_context(ob_ref.object) & OB_VISIBLE_SELF)) {
      return;
    }
    inst.object_sync(ob_ref, manager);
  });
  inst.end_sync();

  manager.end_sync();

  const float aa_radius = clamp_f(draw_ctx->scene->r.gauss, 0.0f, 100.0f);
  const int sample_count = draw_ctx->scene->grease_pencil_settings.aa_samples;
  for (auto i : IndexRange(sample_count)) {
    float2 aa_offset = Instance::antialiasing_sample_get(i, sample_count) * aa_radius;
    aa_offset = 2.0f * aa_offset / float2(inst.render_color_tx.size());
    render_set_view(engine, depsgraph, aa_offset);
    render_init_buffers(draw_ctx, inst, engine, render_layer, depsgraph, &rect);

    /* Render the gpencil object and merge the result to the underlying render. */
    inst.draw(manager);

    /* Weight of this render SSAA sample. The sum of previous samples is weighted by `1 - weight`.
     * This diminishes after each new sample as we want all samples to be equally weighted inside
     * the final result (inside the combined buffer). This weighting scheme allows to always store
     * the resolved result making it ready for in-progress display or read-back. */
    const float weight = 1.0f / (1.0f + i);
    inst.antialiasing_accumulate(manager, weight);
  }

  render_result_combined(render_layer, viewname, inst, &rect);
  render_result_z(draw_ctx, render_layer, viewname, inst, &rect);
}

}  // namespace blender::draw::gpencil
