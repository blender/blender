/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_rect.h"

#include "BKE_colortools.hh"

#include "DRW_render.hh"

#include "BKE_object.hh"

#include "DEG_depsgraph_query.hh"

#include "RE_engine.h"
#include "RE_pipeline.h"
#include "render_types.h"

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
                            const float2 aa_offset = float2{0.0f})
{
  Object *camera = DEG_get_evaluated(depsgraph, RE_GetCamera(engine->re));

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
                                const rcti *rect,
                                const bool use_separated_pass)
{
  const int2 size = int2(draw_ctx->viewport_size_get());
  View &view = View::default_get();

  /* Create depth texture & color texture from render result. */
  const char *viewname = RE_GetActiveRenderView(engine->re);
  RenderPass *rpass_z_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_DEPTH, viewname);
  RenderPass *rpass_col_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_COMBINED, viewname);

  float *pix_z = (rpass_z_src) ? rpass_z_src->ibuf->float_buffer.data : nullptr;
  float *pix_col = (rpass_col_src) ? rpass_col_src->ibuf->float_buffer.data : nullptr;

  if (!pix_z || !pix_col) {
    RE_engine_set_error_message(
        engine, "Warning: To render Grease Pencil, enable Combined and Depth passes.");
  }

  if (pix_z) {
    /* Depth need to be remapped to [0..1] range. */
    pix_z = static_cast<float *>(MEM_dupallocN(pix_z));
    remap_depth(view, {pix_z, rpass_z_src->rectx * rpass_z_src->recty});
  }

  const bool do_region = (!use_separated_pass) && !(rect->xmin == 0 && rect->ymin == 0 &&
                                                    rect->xmax == size.x && rect->ymax == size.y);
  const bool do_clear_z = !pix_z || do_region;
  const bool do_clear_col = use_separated_pass || (!pix_col) || do_region;

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
        gpu::TextureFormat::SFLOAT_32_DEPTH, int2(size), usage, do_region ? nullptr : pix_z);
  }
  if (inst.render_color_tx.is_valid() && !do_clear_col) {
    GPU_texture_update(inst.render_color_tx, GPU_DATA_FLOAT, pix_col);
  }
  else {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT |
                             GPU_TEXTURE_USAGE_HOST_READ;
    inst.render_color_tx.ensure_2d(
        gpu::TextureFormat::SFLOAT_16_16_16_16, int2(size), usage, do_region ? nullptr : pix_col);
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
  if ((view_layer->passflag & SCE_PASS_DEPTH) == 0) {
    return;
  }
  RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_DEPTH, viewname);
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

static void render_result_separated_pass(float *data, Instance &instance, const rcti *rect)
{
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
                             data);
}

/* This is taken from blender::eevee::Sampling::cdf_from_curvemapping. */
static void cdf_from_curvemapping(const CurveMapping &curve, Array<float> &cdf)
{
  BLI_assert(cdf.size() > 1);
  cdf[0] = 0.0f;
  /* Actual CDF evaluation. */
  for (const int u : IndexRange(cdf.size() - 1)) {
    const float x = float(u + 1) / float(cdf.size() - 1);
    cdf[u + 1] = cdf[u] + BKE_curvemapping_evaluateF(&curve, 0, x);
  }
  /* Normalize the CDF. */
  for (const int u : cdf.index_range()) {
    cdf[u] /= cdf.last();
  }
  /* Just to make sure. */
  cdf.last() = 1.0f;
}

/* This is taken from blender::eevee::Sampling::cdf_invert. */
static void cdf_invert(Array<float> &cdf, Array<float> &inverted_cdf)
{
  BLI_assert(cdf.first() == 0.0f && cdf.last() == 1.0f);
  for (const int u : inverted_cdf.index_range()) {
    const float x = clamp_f(u / float(inverted_cdf.size() - 1), 1e-5f, 1.0f - 1e-5f);
    for (const int i : cdf.index_range().drop_front(1)) {
      if (cdf[i] >= x) {
        const float t = (x - cdf[i]) / (cdf[i] - cdf[i - 1]);
        inverted_cdf[u] = (float(i) + t) / float(cdf.size() - 1);
        break;
      }
    }
  }
}

/* This is taken from blender::eevee::MotionBlurModule::shutter_time_to_scene_time. */
static float shutter_time_to_scene_time(const int shutter_position,
                                        const float shutter_time,
                                        const float frame_time,
                                        float time)
{
  switch (shutter_position) {
    case SCE_MB_START:
      /* No offset. */
      break;
    case SCE_MB_CENTER:
      time -= 0.5f;
      break;
    case SCE_MB_END:
      time -= 1.0;
      break;
    default:
      BLI_assert_msg(false, "Invalid motion blur position enum!");
      break;
  }
  time *= shutter_time;
  time += frame_time;
  return time;
}

static void render_frame(RenderEngine *engine,
                         Depsgraph *depsgraph,
                         const DRWContext *draw_ctx,
                         RenderLayer *render_layer,
                         const rcti rect,
                         gpencil::Instance &inst,
                         Manager &manager,
                         const bool separated_pass)
{
  Scene *scene = draw_ctx->scene;

  const float aa_radius = clamp_f(scene->r.gauss, 0.0f, 100.0f);

  const bool motion_blur_enabled = (scene->r.mode & R_MBLUR) != 0 &&
                                   (draw_ctx->view_layer->layflag & SCE_LAY_MOTION_BLUR) != 0 &&
                                   scene->grease_pencil_settings.motion_blur_steps > 0;

  const int motion_steps_count =
      motion_blur_enabled ? max_ii(1, scene->grease_pencil_settings.motion_blur_steps) * 2 + 1 : 1;
  const int total_step_count = ceil_to_multiple_u(scene->grease_pencil_settings.aa_samples,
                                                  motion_steps_count);
  const int aa_per_step = total_step_count / motion_steps_count;

  const int shutter_position = scene->r.motion_blur_position;
  const float shutter_time = scene->r.motion_blur_shutter;

  const int initial_frame = scene->r.cfra;
  const float initial_subframe = scene->r.subframe;
  const float frame_time = initial_frame + initial_subframe;

  Array<float> time_steps(motion_steps_count);
  if (motion_blur_enabled) {
    BKE_curvemapping_changed(&scene->r.mblur_shutter_curve, false);

    Array<float> cdf(CM_TABLE);
    cdf_from_curvemapping(scene->r.mblur_shutter_curve, cdf);
    cdf_invert(cdf, time_steps);

    for (float &scene_time : time_steps) {
      scene_time = shutter_time_to_scene_time(
          shutter_position, shutter_time, frame_time, scene_time);
    }
  }
  else {
    BLI_assert(time_steps.size() == 1);
    time_steps.first() = frame_time;
  }

  int sample_i = 0;
  for (const float time : time_steps) {
    inst.init();

    if (motion_blur_enabled) {
      DRW_render_set_time(engine, depsgraph, floorf(time), fractf(time));
    }

    inst.camera = DEG_get_evaluated(depsgraph, RE_GetCamera(engine->re));

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

    for ([[maybe_unused]] const int i : IndexRange(aa_per_step)) {
      const float2 aa_sample = Instance::antialiasing_sample_get(sample_i, total_step_count) *
                               aa_radius;
      const float2 aa_offset = 2.0f * aa_sample / float2(inst.render_color_tx.size());
      render_set_view(engine, depsgraph, aa_offset);
      render_init_buffers(draw_ctx, inst, engine, render_layer, &rect, separated_pass);

      /* Render the gpencil object and merge the result to the underlying render. */
      inst.draw(manager);

      /* Weight of this render SSAA sample. The sum of previous samples is weighted by
       * `1 - weight`. This diminishes after each new sample as we want all samples to be equally
       * weighted inside the final result (inside the combined buffer). This weighting scheme
       * allows to always store the resolved result making it ready for in-progress display or
       * read-back. */
      const float weight = 1.0f / (1.0f + sample_i);
      inst.antialiasing_accumulate(manager, weight);

      sample_i++;
    }
  }

  if (motion_blur_enabled) {
    /* Restore original frame number. This is because the render pipeline expects it. */
    RE_engine_frame_set(engine, initial_frame, initial_subframe);
  }
}

void Engine::render_to_image(RenderEngine *engine, RenderLayer *render_layer, const rcti rect)
{
  const char *viewname = RE_GetActiveRenderView(engine->re);

  const DRWContext *draw_ctx = DRW_context_get();
  Depsgraph *depsgraph = draw_ctx->depsgraph;

  if (draw_ctx->view_layer->grease_pencil_flags & GREASE_PENCIL_AS_SEPARATE_PASS) {
    Render *re = engine->re;
    RE_create_render_pass(
        re->result, RE_PASSNAME_GREASE_PENCIL, 4, "RGBA", render_layer->name, viewname, true);
  }

  gpencil::Instance inst;

  Manager &manager = *DRW_manager_get();

  render_set_view(engine, depsgraph);
  render_init_buffers(draw_ctx, inst, engine, render_layer, &rect, false);

  render_frame(engine, depsgraph, draw_ctx, render_layer, rect, inst, manager, false);
  render_result_combined(render_layer, viewname, inst, &rect);

  float *pass_data = RE_RenderLayerGetPass(render_layer, RE_PASSNAME_GREASE_PENCIL, viewname);
  if (pass_data) {
    render_frame(engine, depsgraph, draw_ctx, render_layer, rect, inst, manager, true);
    render_result_separated_pass(pass_data, inst, &rect);
  }

  /* Transfer depth in the last step, because if we need to render separate pass, we need original
   * untouched depth buffer. */
  render_result_z(draw_ctx, render_layer, viewname, inst, &rect);
}

}  // namespace blender::draw::gpencil
