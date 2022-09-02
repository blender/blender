/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * The film class handles accumulation of samples with any distorted camera_type
 * using a pixel filter. Inputs needs to be jittered so that the filter converges to the right
 * result.
 *
 * In viewport, we switch between 2 accumulation mode depending on the scene state.
 * - For static scene, we use a classic weighted accumulation.
 * - For dynamic scene (if an update is detected), we use a more temporally stable accumulation
 *   following the Temporal Anti-Aliasing method (a.k.a. Temporal Super-Sampling). This does
 *   history reprojection and rectification to avoid most of the flickering.
 */

#pragma once

#include "DRW_render.h"

#include "eevee_shader_shared.hh"

namespace blender::eevee {

class Instance;

/* -------------------------------------------------------------------- */
/** \name Film
 * \{ */

class Film {
 public:
  /** Stores indirection table of AOVs based on their name hash and their type. */
  AOVsInfoDataBuf aovs_info;
  /** For debugging purpose but could be a user option in the future. */
  static constexpr bool use_box_filter = false;

 private:
  Instance &inst_;

  /** Incoming combined buffer with post FX applied (motion blur + depth of field). */
  GPUTexture *combined_final_tx_ = nullptr;

  /** Main accumulation textures containing every render-pass except depth and combined. */
  Texture color_accum_tx_;
  Texture value_accum_tx_;
  /** Depth accumulation texture. Separated because using a different format. */
  Texture depth_tx_;
  /** Combined "Color" buffer. Double buffered to allow re-projection. */
  SwapChain<Texture, 2> combined_tx_;
  /** Weight buffers. Double buffered to allow updating it during accumulation. */
  SwapChain<Texture, 2> weight_tx_;
  /** User setting to disable reprojection. Useful for debugging or have a more precise render. */
  bool force_disable_reprojection_ = false;

  PassSimple accumulate_ps_ = {"Film.Accumulate"};

  FilmDataBuf data_;

  eViewLayerEEVEEPassType enabled_passes_ = eViewLayerEEVEEPassType(0);

 public:
  Film(Instance &inst) : inst_(inst){};
  ~Film(){};

  void init(const int2 &full_extent, const rcti *output_rect);

  void sync();
  void end_sync();

  /** Accumulate the newly rendered sample contained in #RenderBuffers and blit to display. */
  void accumulate(const DRWView *view, GPUTexture *combined_final_tx);

  /** Blit to display. No rendered sample needed. */
  void display();

  float *read_pass(eViewLayerEEVEEPassType pass_type);
  float *read_aov(ViewLayerAOV *aov);

  /** Returns shading views internal resolution. */
  int2 render_extent_get() const
  {
    return data_.render_extent;
  }

  float2 pixel_jitter_get() const;

  float background_opacity_get() const
  {
    return data_.background_opacity;
  }

  eViewLayerEEVEEPassType enabled_passes_get() const;

  static bool pass_is_value(eViewLayerEEVEEPassType pass_type)
  {
    switch (pass_type) {
      case EEVEE_RENDER_PASS_Z:
      case EEVEE_RENDER_PASS_MIST:
      case EEVEE_RENDER_PASS_SHADOW:
      case EEVEE_RENDER_PASS_AO:
        return true;
      default:
        return false;
    }
  }

  static bool pass_is_float3(eViewLayerEEVEEPassType pass_type)
  {
    switch (pass_type) {
      case EEVEE_RENDER_PASS_NORMAL:
      case EEVEE_RENDER_PASS_DIFFUSE_LIGHT:
      case EEVEE_RENDER_PASS_DIFFUSE_COLOR:
      case EEVEE_RENDER_PASS_SPECULAR_LIGHT:
      case EEVEE_RENDER_PASS_SPECULAR_COLOR:
      case EEVEE_RENDER_PASS_VOLUME_LIGHT:
      case EEVEE_RENDER_PASS_EMIT:
      case EEVEE_RENDER_PASS_ENVIRONMENT:
        return true;
      default:
        return false;
    }
  }

  /* Returns layer offset in the accumulation texture. -1 if the pass is not enabled. */
  int pass_id_get(eViewLayerEEVEEPassType pass_type) const
  {
    switch (pass_type) {
      case EEVEE_RENDER_PASS_COMBINED:
        return data_.combined_id;
      case EEVEE_RENDER_PASS_Z:
        return data_.depth_id;
      case EEVEE_RENDER_PASS_MIST:
        return data_.mist_id;
      case EEVEE_RENDER_PASS_NORMAL:
        return data_.normal_id;
      case EEVEE_RENDER_PASS_DIFFUSE_LIGHT:
        return data_.diffuse_light_id;
      case EEVEE_RENDER_PASS_DIFFUSE_COLOR:
        return data_.diffuse_color_id;
      case EEVEE_RENDER_PASS_SPECULAR_LIGHT:
        return data_.specular_light_id;
      case EEVEE_RENDER_PASS_SPECULAR_COLOR:
        return data_.specular_color_id;
      case EEVEE_RENDER_PASS_VOLUME_LIGHT:
        return data_.volume_light_id;
      case EEVEE_RENDER_PASS_EMIT:
        return data_.emission_id;
      case EEVEE_RENDER_PASS_ENVIRONMENT:
        return data_.environment_id;
      case EEVEE_RENDER_PASS_SHADOW:
        return data_.shadow_id;
      case EEVEE_RENDER_PASS_AO:
        return data_.ambient_occlusion_id;
      case EEVEE_RENDER_PASS_CRYPTOMATTE:
        return -1; /* TODO */
      case EEVEE_RENDER_PASS_VECTOR:
        return data_.vector_id;
      default:
        return -1;
    }
  }

  static const char *pass_to_render_pass_name(eViewLayerEEVEEPassType pass_type)
  {
    switch (pass_type) {
      case EEVEE_RENDER_PASS_COMBINED:
        return RE_PASSNAME_COMBINED;
      case EEVEE_RENDER_PASS_Z:
        return RE_PASSNAME_Z;
      case EEVEE_RENDER_PASS_MIST:
        return RE_PASSNAME_MIST;
      case EEVEE_RENDER_PASS_NORMAL:
        return RE_PASSNAME_NORMAL;
      case EEVEE_RENDER_PASS_DIFFUSE_LIGHT:
        return RE_PASSNAME_DIFFUSE_DIRECT;
      case EEVEE_RENDER_PASS_DIFFUSE_COLOR:
        return RE_PASSNAME_DIFFUSE_COLOR;
      case EEVEE_RENDER_PASS_SPECULAR_LIGHT:
        return RE_PASSNAME_GLOSSY_DIRECT;
      case EEVEE_RENDER_PASS_SPECULAR_COLOR:
        return RE_PASSNAME_GLOSSY_COLOR;
      case EEVEE_RENDER_PASS_VOLUME_LIGHT:
        return RE_PASSNAME_VOLUME_LIGHT;
      case EEVEE_RENDER_PASS_EMIT:
        return RE_PASSNAME_EMIT;
      case EEVEE_RENDER_PASS_ENVIRONMENT:
        return RE_PASSNAME_ENVIRONMENT;
      case EEVEE_RENDER_PASS_SHADOW:
        return RE_PASSNAME_SHADOW;
      case EEVEE_RENDER_PASS_AO:
        return RE_PASSNAME_AO;
      case EEVEE_RENDER_PASS_CRYPTOMATTE:
        BLI_assert_msg(0, "Cryptomatte is not implemented yet.");
        return ""; /* TODO */
      case EEVEE_RENDER_PASS_VECTOR:
        return RE_PASSNAME_VECTOR;
      default:
        BLI_assert(0);
        return "";
    }
  }

 private:
  void init_aovs();
  void sync_mist();

  /**
   * Precompute sample weights if they are uniform across the whole film extent.
   */
  void update_sample_table();
};

/** \} */

}  // namespace blender::eevee
