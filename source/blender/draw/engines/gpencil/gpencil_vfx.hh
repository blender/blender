/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */
#include "BKE_camera.h"
#include "BLI_listbase_wrapper.hh"
#include "DNA_camera_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_shader_fx_types.h"

#include "draw_manager.hh"
#include "draw_pass.hh"

#include "gpencil_engine.h"
#include "gpencil_shader.hh"
#include "gpencil_shader_shared.h"

namespace blender::draw::greasepencil {

using namespace draw;

struct VfxContext {
  PassMain::Sub *object_subpass;
  SwapChain<GPUFrameBuffer **, 2> vfx_fb;
  SwapChain<GPUTexture **, 2> color_tx;
  SwapChain<GPUTexture **, 2> reveal_tx;
  bool is_viewport;

  VfxContext(PassMain::Sub &object_subpass_,
             Framebuffer &layer_fb,
             Framebuffer &object_fb,
             TextureFromPool &object_color_tx,
             TextureFromPool &layer_color_tx,
             TextureFromPool &object_reveal_tx,
             TextureFromPool &layer_reveal_tx,
             bool is_render_)
  {
    object_subpass = &object_subpass_;
    /* These may not be allocated yet, use address of future pointer. */
    vfx_fb.current() = &layer_fb;
    vfx_fb.next() = &object_fb;

    color_tx.current() = &object_color_tx;
    color_tx.next() = &layer_color_tx;
    reveal_tx.current() = &object_reveal_tx;
    reveal_tx.next() = &layer_reveal_tx;

    is_viewport = (is_render_ == false);
  }

  PassMain::Sub &create_vfx_pass(const char *name, GPUShader *shader)
  {
    PassMain::Sub &sub = object_subpass->sub(name);
    sub.framebuffer_set(vfx_fb.current());
    sub.shader_set(shader);
    sub.bind_texture("colorBuf", color_tx.current());
    sub.bind_texture("revealBuf", reveal_tx.current());

    vfx_fb.swap();
    color_tx.swap();
    reveal_tx.swap();

    return sub;
  }

  /* Verify if the given fx is active. */
  bool effect_is_active(const bGPdata *gpd, const ShaderFxData *fx)
  {
    if (fx == NULL) {
      return false;
    }

    if (gpd == NULL) {
      return false;
    }

    bool is_edit = GPENCIL_ANY_EDIT_MODE(gpd);
    if (((fx->mode & eShaderFxMode_Editmode) == 0) && (is_edit) && (is_viewport)) {
      return false;
    }

    if (((fx->mode & eShaderFxMode_Realtime) && (is_viewport == true)) ||
        ((fx->mode & eShaderFxMode_Render) && (is_viewport == false)))
    {
      return true;
    }

    return false;
  }
};

class VfxModule {
 private:
  ShaderModule &shaders;
  /* Global switch for all vfx. */
  bool vfx_enabled_ = false;
  /* Global switch for all Depth Of Field blur. */
  bool dof_enabled_ = false;
  /* Pseudo depth of field parameter. Used to scale blur radius. */
  float dof_parameters_[2];

 public:
  VfxModule(ShaderModule &shaders_) : shaders(shaders_){};

  void init(bool enable, const Object *camera_object, const RegionView3D *rv3d)
  {
    vfx_enabled_ = enable;

    const Camera *camera = (camera_object != nullptr) ?
                               static_cast<const Camera *>(camera_object->data) :
                               nullptr;

    /* Pseudo DOF setup. */
    if (camera && (camera->dof.flag & CAM_DOF_ENABLED)) {
      const float *vp_size = DRW_viewport_size_get();
      float fstop = camera->dof.aperture_fstop;
      float sensor = BKE_camera_sensor_size(
          camera->sensor_fit, camera->sensor_x, camera->sensor_y);
      float focus_dist = BKE_camera_object_dof_distance(camera_object);
      float focal_len = camera->lens;

      const float scale_camera = 0.001f;
      /* We want radius here for the aperture number. */
      float aperture = 0.5f * scale_camera * focal_len / fstop;
      float focal_len_scaled = scale_camera * focal_len;
      float sensor_scaled = scale_camera * sensor;

      if (rv3d != nullptr) {
        sensor_scaled *= rv3d->viewcamtexcofac[0];
      }

      dof_parameters_[1] = aperture * fabsf(focal_len_scaled / (focus_dist - focal_len_scaled));
      dof_parameters_[1] *= vp_size[0] / sensor_scaled;
      dof_parameters_[0] = -focus_dist * dof_parameters_[1];
    }
    else {
      /* Disable DoF blur scaling. Produce Circle of Confusion of 0 pixel. */
      dof_parameters_[0] = dof_parameters_[1] = 0.0f;
    }
  }

  /* Return true if any vfx is needed */
  bool object_sync(Framebuffer &main_fb,
                   ObjectRef &object_ref,
                   VfxContext &vfx_ctx,
                   bool do_material_holdout)
  {
    Object *object = object_ref.object;
    bGPdata *gpd = (bGPdata *)object->data;

    int vfx_count = 0;

    if (vfx_enabled_) {
      for (const ShaderFxData *fx : ListBaseWrapper<const ShaderFxData>(&object->shader_fx)) {
        if (!vfx_ctx.effect_is_active(gpd, fx)) {
          continue;
        }
        switch (fx->type) {
          case eShaderFxType_Blur:
            vfx_count += vfx_blur(*(const BlurShaderFxData *)fx, object, vfx_ctx);
            break;
          case eShaderFxType_Colorize:
            vfx_count += vfx_colorize(*(const ColorizeShaderFxData *)fx, object, vfx_ctx);
            break;
          case eShaderFxType_Flip:
            vfx_count += vfx_flip(*(const FlipShaderFxData *)fx, object, vfx_ctx);
            break;
          case eShaderFxType_Pixel:
            vfx_count += vfx_pixelize(*(const PixelShaderFxData *)fx, object, vfx_ctx);
            break;
          case eShaderFxType_Rim:
            vfx_count += vfx_rim(*(const RimShaderFxData *)fx, object, vfx_ctx);
            break;
          case eShaderFxType_Shadow:
            vfx_count += vfx_shadow(*(const ShadowShaderFxData *)fx, object, vfx_ctx);
            break;
          case eShaderFxType_Glow:
            vfx_count += vfx_glow(*(const GlowShaderFxData *)fx, object, vfx_ctx);
            break;
          case eShaderFxType_Swirl:
            vfx_count += vfx_swirl(*(const SwirlShaderFxData *)fx, object, vfx_ctx);
            break;
          case eShaderFxType_Wave:
            vfx_count += vfx_wave(*(const WaveShaderFxData *)fx, object, vfx_ctx);
            break;
          default:
            break;
        }
      }
    }

    if (do_material_holdout) {
      vfx_count += 1;
    }

    if (vfx_count > 0) {
      /* We need an extra pass to combine result to main buffer. */
      merge_sync(main_fb, vfx_ctx);
    }

    return vfx_count > 0;
  }

 private:
  int vfx_blur(const BlurShaderFxData &fx, const Object *object, VfxContext &vfx_ctx)
  {
    if ((fx.flag & FX_BLUR_DOF_MODE) && !dof_enabled_) {
      /* No blur outside camera view (or when DOF is disabled on the camera). */
      return 0;
    }

    float winmat[4][4], persmat[4][4];
    float2 blur_size = {fx.radius[0], fx.radius[1]};

    /* TODO(fclem): Replace by draw::View. */
    DRW_view_persmat_get(nullptr, persmat, false);
    const float w = fabsf(mul_project_m4_v3_zfac(persmat, object->object_to_world().location()));

    if (fx.flag & FX_BLUR_DOF_MODE) {
      /* Compute circle of confusion size. */
      float coc = (dof_parameters_[0] / -w) - dof_parameters_[1];
      blur_size = float2(fabsf(coc));
    }
    else {
      /* Modify by distance to camera and object scale. */
      /* TODO(fclem): Replace by draw::View. */
      DRW_view_winmat_get(nullptr, winmat, false);
      /* TODO(fclem): Replace by this->render_size. */
      const float *vp_size = DRW_viewport_size_get();

      float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
      float scale = mat4_to_scale(object->object_to_world().ptr());
      float distance_factor = world_pixel_scale * scale * winmat[1][1] * vp_size[1] / w;
      blur_size *= distance_factor;
    }

    if ((fx.samples == 0.0f) || (blur_size[0] == 0.0f && blur_size[1] == 0.0f)) {
      return 0;
    }

    GPUShader *sh = shaders.static_shader_get(eShaderType::FX_BLUR);

    const float rot_sin = sin(fx.rotation);
    const float rot_cos = cos(fx.rotation);

    if (blur_size[0] > 0.0f) {
      PassMain::Sub &sub = vfx_ctx.create_vfx_pass("Fx Blur H", sh);
      sub.state_set(DRW_STATE_WRITE_COLOR);
      sub.push_constant("offset", float2(blur_size[0] * rot_cos, blur_size[0] * rot_sin));
      sub.push_constant("sampCount", max_ii(1, min_ii(fx.samples, blur_size[0])));
      sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }
    if (blur_size[1] > 0.0f) {
      PassMain::Sub &sub = vfx_ctx.create_vfx_pass("Fx Blur V", sh);
      sub.state_set(DRW_STATE_WRITE_COLOR);
      sub.push_constant("offset", float2(-blur_size[1] * rot_sin, blur_size[1] * rot_cos));
      sub.push_constant("sampCount", max_ii(1, min_ii(fx.samples, blur_size[1])));
      sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }

    /* Return number of passes. */
    return int(blur_size[0] > 0.0f) + int(blur_size[1] > 0.0f);
  }

  int vfx_colorize(const ColorizeShaderFxData &fx, const Object *object, VfxContext &vfx_ctx)
  {
    UNUSED_VARS(fx, object, vfx_ctx);
    return 0;
  }

  int vfx_flip(const FlipShaderFxData &fx, const Object *object, VfxContext &vfx_ctx)
  {
    UNUSED_VARS(fx, object, vfx_ctx);
    return 0;
  }

  int vfx_pixelize(const PixelShaderFxData &fx, const Object *object, VfxContext &vfx_ctx)
  {
    UNUSED_VARS(fx, object, vfx_ctx);
    return 0;
  }

  int vfx_rim(const RimShaderFxData &fx, const Object *object, VfxContext &vfx_ctx)
  {
    UNUSED_VARS(fx, object, vfx_ctx);
    return 0;
  }

  int vfx_shadow(const ShadowShaderFxData &fx, const Object *object, VfxContext &vfx_ctx)
  {
    UNUSED_VARS(fx, object, vfx_ctx);
    return 0;
  }

  int vfx_glow(const GlowShaderFxData &fx, const Object *object, VfxContext &vfx_ctx)
  {
    UNUSED_VARS(fx, object, vfx_ctx);
    return 0;
  }

  int vfx_swirl(const SwirlShaderFxData &fx, const Object *object, VfxContext &vfx_ctx)
  {
    UNUSED_VARS(fx, object, vfx_ctx);
    return 0;
  }

  int vfx_wave(const WaveShaderFxData &fx, const Object *object, VfxContext &vfx_ctx)
  {
    UNUSED_VARS(fx, object, vfx_ctx);
    return 0;
  }

  void merge_sync(Framebuffer &main_fb, VfxContext &vfx_ctx)
  {
    PassMain::Sub &sub = vfx_ctx.object_subpass->sub("GPencil Object Composite");
    sub.framebuffer_set(&main_fb);

    sub.shader_set(shaders.static_shader_get(FX_COMPOSITE));
    sub.bind_texture("colorBuf", vfx_ctx.color_tx.current());
    sub.bind_texture("revealBuf", vfx_ctx.reveal_tx.current());

    sub.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_MUL);
    sub.push_constant("isFirstPass", true);
    sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    /* We cannot do custom blending on multi-target frame-buffers.
     * Workaround by doing 2 passes. */
    sub.state_set(DRW_STATE_WRITE_COLOR, DRW_STATE_BLEND_ADD_FULL);
    sub.push_constant("isFirstPass", false);
    sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
};

}  // namespace blender::draw::greasepencil
