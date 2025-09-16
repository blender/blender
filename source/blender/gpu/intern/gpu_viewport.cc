/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * System that manages viewport drawing.
 */

#include <cstring>

#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"

#include "BKE_colortools.hh"

#include "DNA_color_types.h"
#include "IMB_colormanagement.hh"

#include "DNA_vec_types.h"

#include "GPU_capabilities.hh"
#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"
#include "GPU_viewport.hh"

#include "DRW_engine.hh"

#include "MEM_guardedalloc.h"

/* Struct storing a viewport specific blender::gpu::Batch.
 * The end-goal is to have a single batch shared across viewport and use a model matrix to place
 * the batch. Due to OCIO and Image/UV editor we are not able to use an model matrix yet. */
struct GPUViewportBatch {
  blender::gpu::Batch *batch;
  struct {
    rctf rect_pos;
    rctf rect_uv;
  } last_used_parameters;
};

static struct {
  GPUVertFormat format;
  struct {
    uint pos, tex_coord;
  } attr_id;
} g_viewport = {{0}};

struct GPUViewport {
  blender::int2 size;
  int flag;

  /* Set the active view (for stereoscopic viewport rendering). */
  int active_view;

  /* Viewport Resources. */
  DRWData *draw_data;
  /** Color buffers, one for each stereo view. Only one if not stereo viewport. */
  blender::gpu::Texture *color_render_tx[2];
  blender::gpu::Texture *color_overlay_tx[2];
  /** Depth buffer. Can be shared with GPUOffscreen. */
  blender::gpu::Texture *depth_tx;
  /** Compositing framebuffer for stereo viewport. */
  blender::gpu::FrameBuffer *stereo_comp_fb;
  /** Color render and overlay frame-buffers for drawing outside of DRW module.
   * The render framebuffer is expected to be in the linear space and viewport will perform color
   * management on it to bring it to the display space.
   * The overlay frame-buffer is expected to be in the display space and viewport does not do any
   * color management on it. */
  blender::gpu::FrameBuffer *render_fb;
  blender::gpu::FrameBuffer *overlay_fb;

  /* Color management. */
  ColorManagedViewSettings view_settings;
  ColorManagedDisplaySettings display_settings;
  bool use_hdr_display;
  CurveMapping *orig_curve_mapping;
  float dither;
  /* TODO(@fclem): the UV-image display use the viewport but do not set any view transform for the
   * moment. The end goal would be to let the GPUViewport do the color management. */
  bool do_color_management;
  GPUViewportBatch batch;
};

enum {
  DO_UPDATE = (1 << 0),
  GPU_VIEWPORT_STEREO = (1 << 1),
};

void GPU_viewport_tag_update(GPUViewport *viewport)
{
  viewport->flag |= DO_UPDATE;
}

bool GPU_viewport_do_update(GPUViewport *viewport)
{
  bool ret = (viewport->flag & DO_UPDATE);
  viewport->flag &= ~DO_UPDATE;
  return ret;
}

GPUViewport *GPU_viewport_create()
{
  GPUViewport *viewport = MEM_callocN<GPUViewport>("GPUViewport");
  viewport->do_color_management = false;
  viewport->size[0] = viewport->size[1] = -1;
  viewport->active_view = 0;
  return viewport;
}

GPUViewport *GPU_viewport_stereo_create()
{
  GPUViewport *viewport = GPU_viewport_create();
  viewport->flag = GPU_VIEWPORT_STEREO;
  return viewport;
}

DRWData **GPU_viewport_data_get(GPUViewport *viewport)
{
  return &viewport->draw_data;
}

static void gpu_viewport_textures_create(GPUViewport *viewport)
{
  int *size = viewport->size;
  float const empty_pixel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;

  if (viewport->color_render_tx[0] == nullptr) {

    /* NOTE: dtxl_color texture requires write support as it may be written to by the viewport
     * compositor. */
    viewport->color_render_tx[0] = GPU_texture_create_2d(
        "dtxl_color",
        UNPACK2(size),
        1,
        blender::gpu::TextureFormat::SFLOAT_16_16_16_16,
        usage | GPU_TEXTURE_USAGE_SHADER_WRITE,
        nullptr);
    viewport->color_overlay_tx[0] = GPU_texture_create_2d(
        "dtxl_color_overlay",
        UNPACK2(size),
        1,
        blender::gpu::TextureFormat::SRGBA_8_8_8_8,
        usage,
        nullptr);

    GPU_texture_clear(viewport->color_render_tx[0], GPU_DATA_FLOAT, empty_pixel);
    GPU_texture_clear(viewport->color_overlay_tx[0], GPU_DATA_FLOAT, empty_pixel);
  }

  if ((viewport->flag & GPU_VIEWPORT_STEREO) != 0 && viewport->color_render_tx[1] == nullptr) {
    viewport->color_render_tx[1] = GPU_texture_create_2d(
        "dtxl_color_stereo",
        UNPACK2(size),
        1,
        blender::gpu::TextureFormat::SFLOAT_16_16_16_16,
        usage | GPU_TEXTURE_USAGE_SHADER_WRITE,
        nullptr);
    viewport->color_overlay_tx[1] = GPU_texture_create_2d(
        "dtxl_color_overlay_stereo",
        UNPACK2(size),
        1,
        blender::gpu::TextureFormat::SRGBA_8_8_8_8,
        usage,
        nullptr);

    GPU_texture_clear(viewport->color_render_tx[1], GPU_DATA_FLOAT, empty_pixel);
    GPU_texture_clear(viewport->color_overlay_tx[1], GPU_DATA_FLOAT, empty_pixel);
  }

  /* Can be shared with #GPUOffscreen. */
  if (viewport->depth_tx == nullptr) {
    /* Depth texture can be read back by gizmos #view3d_depths_create. */
    /* Swizzle flag is needed by Workbench Volumes to read the stencil view. */
    viewport->depth_tx = GPU_texture_create_2d("dtxl_depth",
                                               UNPACK2(size),
                                               1,
                                               blender::gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8,
                                               usage | GPU_TEXTURE_USAGE_HOST_READ |
                                                   GPU_TEXTURE_USAGE_FORMAT_VIEW,
                                               nullptr);
    const int depth_clear = 0;
    GPU_texture_clear(viewport->depth_tx, GPU_DATA_UINT_24_8_DEPRECATED, &depth_clear);
  }

  if (!viewport->depth_tx || !viewport->color_render_tx[0] || !viewport->color_overlay_tx[0]) {
    GPU_viewport_free(viewport);
  }
}

static void gpu_viewport_textures_free(GPUViewport *viewport)
{
  GPU_FRAMEBUFFER_FREE_SAFE(viewport->stereo_comp_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(viewport->render_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(viewport->overlay_fb);

  for (int i = 0; i < 2; i++) {
    GPU_TEXTURE_FREE_SAFE(viewport->color_render_tx[i]);
    GPU_TEXTURE_FREE_SAFE(viewport->color_overlay_tx[i]);
  }

  GPU_TEXTURE_FREE_SAFE(viewport->depth_tx);
}

void GPU_viewport_bind(GPUViewport *viewport, int view, const rcti *rect)
{
  blender::int2 rect_size;
  /* add one pixel because of scissor test */
  rect_size[0] = BLI_rcti_size_x(rect) + 1;
  rect_size[1] = BLI_rcti_size_y(rect) + 1;

  DRW_gpu_context_enable();

  if (viewport->size != rect_size) {
    copy_v2_v2_int(viewport->size, rect_size);
    gpu_viewport_textures_free(viewport);
    gpu_viewport_textures_create(viewport);
  }

  viewport->active_view = view;
}

void GPU_viewport_bind_from_offscreen(GPUViewport *viewport, GPUOffScreen *ofs, bool is_xr_surface)
{
  blender::gpu::Texture *color, *depth;
  blender::gpu::FrameBuffer *fb;
  viewport->size[0] = GPU_offscreen_width(ofs);
  viewport->size[1] = GPU_offscreen_height(ofs);

  GPU_offscreen_viewport_data_get(ofs, &fb, &color, &depth);

  /* XR surfaces will already check for texture size changes and free if necessary (see
   * #wm_xr_session_surface_offscreen_ensure()), so don't free here as it has a significant
   * performance impact (leads to texture re-creation in #gpu_viewport_textures_create() every VR
   * drawing iteration). */
  if (!is_xr_surface) {
    gpu_viewport_textures_free(viewport);
  }

  /* This is the only texture we can share. */
  viewport->depth_tx = depth;

  gpu_viewport_textures_create(viewport);
}

void GPU_viewport_colorspace_set(GPUViewport *viewport,
                                 const ColorManagedViewSettings *view_settings,
                                 const ColorManagedDisplaySettings *display_settings,
                                 float dither)
{
  /**
   * HACK(fclem): We copy the settings here to avoid use after free if an update frees the scene
   * and the viewport stays cached (see #75443). But this means the OCIO curve-mapping caching
   * (which is based on #CurveMap pointer address) cannot operate correctly and it will create
   * a different OCIO processor for each viewport. We try to only reallocate the curve-map copy
   * if needed to avoid unneeded cache invalidation.
   */
  if (view_settings->curve_mapping) {
    if (viewport->view_settings.curve_mapping) {
      if (view_settings->curve_mapping->changed_timestamp !=
          viewport->view_settings.curve_mapping->changed_timestamp)
      {
        BKE_color_managed_view_settings_free(&viewport->view_settings);
      }
    }
  }

  if (viewport->orig_curve_mapping != view_settings->curve_mapping) {
    viewport->orig_curve_mapping = view_settings->curve_mapping;
    BKE_color_managed_view_settings_free(&viewport->view_settings);
  }
  /* Don't copy the curve mapping already. */
  BKE_color_managed_view_settings_copy_keep_curve_mapping(&viewport->view_settings, view_settings);
  /* Only copy curve-mapping if needed. Avoid unneeded OCIO cache miss. */
  if (view_settings->curve_mapping && viewport->view_settings.curve_mapping == nullptr) {
    BKE_color_managed_view_settings_free(&viewport->view_settings);
    viewport->view_settings.curve_mapping = BKE_curvemapping_copy(view_settings->curve_mapping);
  }

  BKE_color_managed_display_settings_copy(&viewport->display_settings, display_settings);
  viewport->dither = dither;
  viewport->do_color_management = true;
  viewport->use_hdr_display = IMB_colormanagement_display_is_hdr(
      &viewport->display_settings, viewport->view_settings.view_transform);
}

void GPU_viewport_stereo_composite(GPUViewport *viewport, Stereo3dFormat *stereo_format)
{
  if (!ELEM(stereo_format->display_mode, S3D_DISPLAY_ANAGLYPH, S3D_DISPLAY_INTERLACE)) {
    /* Early Exit: the other display modes need access to the full screen and cannot be
     * done from a single viewport. See `wm_stereo.cc`. */
    return;
  }
  /* The composite framebuffer object needs to be created in the window context. */
  GPU_framebuffer_ensure_config(
      &viewport->stereo_comp_fb,
      {
          GPU_ATTACHMENT_NONE,
          /* We need the sRGB attachment to be first for GL_FRAMEBUFFER_SRGB to be turned on.
           * Note that this is the opposite of what the texture binding is. */
          GPU_ATTACHMENT_TEXTURE(viewport->color_overlay_tx[0]),
          GPU_ATTACHMENT_TEXTURE(viewport->color_render_tx[0]),
      });

  GPUVertFormat *vert_format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(vert_format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  GPU_framebuffer_bind(viewport->stereo_comp_fb);
  GPU_matrix_push();
  GPU_matrix_push_projection();
  GPU_matrix_identity_set();
  GPU_matrix_identity_projection_set();
  immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_OVERLAYS_STEREO_MERGE);
  int settings = stereo_format->display_mode;
  if (settings == S3D_DISPLAY_ANAGLYPH) {
    switch (stereo_format->anaglyph_type) {
      case S3D_ANAGLYPH_REDCYAN:
        GPU_color_mask(false, true, true, true);
        break;
      case S3D_ANAGLYPH_GREENMAGENTA:
        GPU_color_mask(true, false, true, true);
        break;
      case S3D_ANAGLYPH_YELLOWBLUE:
        GPU_color_mask(false, false, true, true);
        break;
    }
  }
  else if (settings == S3D_DISPLAY_INTERLACE) {
    settings |= stereo_format->interlace_type << 3;
    SET_FLAG_FROM_TEST(settings, stereo_format->flag & S3D_INTERLACE_SWAP, 1 << 6);
  }
  immUniform1i("stereoDisplaySettings", settings);

  GPU_texture_bind(viewport->color_render_tx[1], 0);
  GPU_texture_bind(viewport->color_overlay_tx[1], 1);

  immBegin(GPU_PRIM_TRI_STRIP, 4);

  immVertex2f(pos, -1.0f, -1.0f);
  immVertex2f(pos, 1.0f, -1.0f);
  immVertex2f(pos, -1.0f, 1.0f);
  immVertex2f(pos, 1.0f, 1.0f);

  immEnd();

  GPU_texture_unbind(viewport->color_render_tx[1]);
  GPU_texture_unbind(viewport->color_overlay_tx[1]);

  immUnbindProgram();
  GPU_matrix_pop_projection();
  GPU_matrix_pop();

  if (settings == S3D_DISPLAY_ANAGLYPH) {
    GPU_color_mask(true, true, true, true);
  }

  GPU_framebuffer_restore();
}
/* -------------------------------------------------------------------- */
/** \name Viewport Batches
 * \{ */

static const GPUVertFormat &gpu_viewport_batch_format()
{
  if (g_viewport.format.attr_len == 0) {
    GPUVertFormat *format = &g_viewport.format;
    g_viewport.attr_id.pos = GPU_vertformat_attr_add(
        format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    g_viewport.attr_id.tex_coord = GPU_vertformat_attr_add(
        format, "texCoord", blender::gpu::VertAttrType::SFLOAT_32_32);
  }
  return g_viewport.format;
}

static blender::gpu::Batch *gpu_viewport_batch_create(const rctf *rect_pos, const rctf *rect_uv)
{
  blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(gpu_viewport_batch_format());
  const uint vbo_len = 4;
  GPU_vertbuf_data_alloc(*vbo, vbo_len);

  GPUVertBufRaw pos_step, tex_coord_step;
  GPU_vertbuf_attr_get_raw_data(vbo, g_viewport.attr_id.pos, &pos_step);
  GPU_vertbuf_attr_get_raw_data(vbo, g_viewport.attr_id.tex_coord, &tex_coord_step);

  copy_v2_fl2(
      static_cast<float *>(GPU_vertbuf_raw_step(&pos_step)), rect_pos->xmin, rect_pos->ymin);
  copy_v2_fl2(
      static_cast<float *>(GPU_vertbuf_raw_step(&tex_coord_step)), rect_uv->xmin, rect_uv->ymin);
  copy_v2_fl2(
      static_cast<float *>(GPU_vertbuf_raw_step(&pos_step)), rect_pos->xmax, rect_pos->ymin);
  copy_v2_fl2(
      static_cast<float *>(GPU_vertbuf_raw_step(&tex_coord_step)), rect_uv->xmax, rect_uv->ymin);
  copy_v2_fl2(
      static_cast<float *>(GPU_vertbuf_raw_step(&pos_step)), rect_pos->xmin, rect_pos->ymax);
  copy_v2_fl2(
      static_cast<float *>(GPU_vertbuf_raw_step(&tex_coord_step)), rect_uv->xmin, rect_uv->ymax);
  copy_v2_fl2(
      static_cast<float *>(GPU_vertbuf_raw_step(&pos_step)), rect_pos->xmax, rect_pos->ymax);
  copy_v2_fl2(
      static_cast<float *>(GPU_vertbuf_raw_step(&tex_coord_step)), rect_uv->xmax, rect_uv->ymax);

  return GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, nullptr, GPU_BATCH_OWNS_VBO);
}

static blender::gpu::Batch *gpu_viewport_batch_get(GPUViewport *viewport,
                                                   const rctf *rect_pos,
                                                   const rctf *rect_uv)
{
  const float compare_limit = 0.0001f;
  const bool parameters_changed =
      (!BLI_rctf_compare(
           &viewport->batch.last_used_parameters.rect_pos, rect_pos, compare_limit) ||
       !BLI_rctf_compare(&viewport->batch.last_used_parameters.rect_uv, rect_uv, compare_limit));

  if (viewport->batch.batch && parameters_changed) {
    GPU_batch_discard(viewport->batch.batch);
    viewport->batch.batch = nullptr;
  }

  if (!viewport->batch.batch) {
    viewport->batch.batch = gpu_viewport_batch_create(rect_pos, rect_uv);
    viewport->batch.last_used_parameters.rect_pos = *rect_pos;
    viewport->batch.last_used_parameters.rect_uv = *rect_uv;
  }
  return viewport->batch.batch;
}

static void gpu_viewport_batch_free(GPUViewport *viewport)
{
  if (viewport->batch.batch) {
    GPU_batch_discard(viewport->batch.batch);
    viewport->batch.batch = nullptr;
  }
}

/** \} */

static void gpu_viewport_draw_colormanaged(GPUViewport *viewport,
                                           int view,
                                           const rctf *rect_pos,
                                           const rctf *rect_uv,
                                           bool display_colorspace,
                                           bool do_overlay_merge)
{
  blender::gpu::Texture *color = viewport->color_render_tx[view];
  blender::gpu::Texture *color_overlay = viewport->color_overlay_tx[view];

  bool use_ocio = false;

  if (viewport->do_color_management && display_colorspace) {
    /* During the binding process the last used VertexFormat is tested and can assert as it is not
     * valid. By calling the `immVertexFormat` the last used VertexFormat is reset and the assert
     * does not happen. This solves a chicken and egg problem when using GPUBatches. GPUBatches
     * contain the correct vertex format, but can only bind after the shader is bound.
     *
     * Image/UV editor still uses imm, after that has been changed we could move this fix to the
     * OCIO. */
    immVertexFormat();
    use_ocio = IMB_colormanagement_setup_glsl_draw_from_space(&viewport->view_settings,
                                                              &viewport->display_settings,
                                                              nullptr,
                                                              viewport->dither,
                                                              false,
                                                              do_overlay_merge);
  }

  blender::gpu::Batch *batch = gpu_viewport_batch_get(viewport, rect_pos, rect_uv);
  if (use_ocio) {
    GPU_batch_program_set_imm_shader(batch);
  }
  else {
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_IMAGE_OVERLAYS_MERGE);
    GPU_batch_uniform_1i(batch, "overlay", do_overlay_merge);
    GPU_batch_uniform_1i(batch, "display_transform", display_colorspace);
    GPU_batch_uniform_1i(batch, "use_hdr_display", viewport->use_hdr_display);
  }

  GPU_texture_bind(color, 0);
  GPU_texture_bind(color_overlay, 1);
  GPU_batch_draw(batch);
  GPU_texture_unbind(color);
  GPU_texture_unbind(color_overlay);

  if (use_ocio) {
    IMB_colormanagement_finish_glsl_draw();
  }
}

void GPU_viewport_draw_to_screen_ex(GPUViewport *viewport,
                                    int view,
                                    const rcti *rect,
                                    bool display_colorspace,
                                    bool do_overlay_merge)
{
  blender::gpu::Texture *color = viewport->color_render_tx[view];

  if (color == nullptr) {
    return;
  }

  const float w = float(GPU_texture_width(color));
  const float h = float(GPU_texture_height(color));

  /* We allow rects with min/max swapped, but we also need correctly assigned coordinates. */
  rcti sanitized_rect = *rect;
  BLI_rcti_sanitize(&sanitized_rect);

  BLI_assert(w == BLI_rcti_size_x(&sanitized_rect) + 1);
  BLI_assert(h == BLI_rcti_size_y(&sanitized_rect) + 1);

  /* wmOrtho for the screen has this same offset */
  const float halfx = GLA_PIXEL_OFS / w;
  const float halfy = GLA_PIXEL_OFS / h;

  rctf pos_rect{};
  pos_rect.xmin = sanitized_rect.xmin;
  pos_rect.ymin = sanitized_rect.ymin;
  pos_rect.xmax = sanitized_rect.xmin + w;
  pos_rect.ymax = sanitized_rect.ymin + h;

  rctf uv_rect{};
  uv_rect.xmin = halfx;
  uv_rect.ymin = halfy;
  uv_rect.xmax = halfx + 1.0f;
  uv_rect.ymax = halfy + 1.0f;

  /* Mirror the UV rect in case axis-swapped drawing is requested (by passing a rect with min and
   * max values swapped). */
  if (BLI_rcti_size_x(rect) < 0) {
    std::swap(uv_rect.xmin, uv_rect.xmax);
  }
  if (BLI_rcti_size_y(rect) < 0) {
    std::swap(uv_rect.ymin, uv_rect.ymax);
  }

  gpu_viewport_draw_colormanaged(
      viewport, view, &pos_rect, &uv_rect, display_colorspace, do_overlay_merge);
}

void GPU_viewport_draw_to_screen(GPUViewport *viewport, int view, const rcti *rect)
{
  GPU_viewport_draw_to_screen_ex(viewport, view, rect, true, true);
}

void GPU_viewport_unbind_from_offscreen(GPUViewport *viewport,
                                        GPUOffScreen *ofs,
                                        bool display_colorspace,
                                        bool do_overlay_merge)
{
  const int view = 0;

  if (viewport->color_render_tx[view] == nullptr) {
    return;
  }

  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_offscreen_bind(ofs, false);

  rctf pos_rect{};
  pos_rect.xmin = -1.0f;
  pos_rect.ymin = -1.0f;
  pos_rect.xmax = 1.0f;
  pos_rect.ymax = 1.0f;

  rctf uv_rect{};
  uv_rect.xmin = 0.0f;
  uv_rect.ymin = 0.0f;
  uv_rect.xmax = 1.0f;
  uv_rect.ymax = 1.0f;

  gpu_viewport_draw_colormanaged(
      viewport, view, &pos_rect, &uv_rect, display_colorspace, do_overlay_merge);

  /* This one is from the offscreen. Don't free it with the viewport. */
  viewport->depth_tx = nullptr;
}

void GPU_viewport_unbind(GPUViewport * /*viewport*/)
{
  GPU_framebuffer_restore();
  DRW_gpu_context_disable();
}

int GPU_viewport_active_view_get(GPUViewport *viewport)
{
  return viewport->active_view;
}

bool GPU_viewport_is_stereo_get(GPUViewport *viewport)
{
  return (viewport->flag & GPU_VIEWPORT_STEREO) != 0;
}

blender::gpu::Texture *GPU_viewport_color_texture(GPUViewport *viewport, int view)
{
  return viewport->color_render_tx[view];
}

blender::gpu::Texture *GPU_viewport_overlay_texture(GPUViewport *viewport, int view)
{
  return viewport->color_overlay_tx[view];
}

blender::gpu::Texture *GPU_viewport_depth_texture(GPUViewport *viewport)
{
  return viewport->depth_tx;
}

blender::gpu::FrameBuffer *GPU_viewport_framebuffer_render_get(GPUViewport *viewport)
{
  GPU_framebuffer_ensure_config(
      &viewport->render_fb,
      {
          GPU_ATTACHMENT_TEXTURE(viewport->depth_tx),
          GPU_ATTACHMENT_TEXTURE(viewport->color_render_tx[viewport->active_view]),
      });
  return viewport->render_fb;
}

blender::gpu::FrameBuffer *GPU_viewport_framebuffer_overlay_get(GPUViewport *viewport)
{
  GPU_framebuffer_ensure_config(
      &viewport->overlay_fb,
      {
          GPU_ATTACHMENT_TEXTURE(viewport->depth_tx),
          GPU_ATTACHMENT_TEXTURE(viewport->color_overlay_tx[viewport->active_view]),
      });
  return viewport->overlay_fb;
}

void GPU_viewport_free(GPUViewport *viewport)
{
  if (viewport->draw_data) {
    DRW_viewport_data_free(viewport->draw_data);
  }

  gpu_viewport_textures_free(viewport);

  BKE_color_managed_view_settings_free(&viewport->view_settings);
  gpu_viewport_batch_free(viewport);

  MEM_freeN(viewport);
}
