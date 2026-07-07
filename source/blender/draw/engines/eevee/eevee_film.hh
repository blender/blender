/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
 *
 * The Film module uses the following terms to refer to different spaces/extents:
 *
 * - Display: The full output extent (matches the full viewport or the final image resolution).
 *
 * - Film: The same extent as display, or a subset of it when a Render Region is used.
 *
 * - Render: The extent used internally by the engine for rendering the main views.
 *   Equals to the full display extent + overscan (even when a Render Region is used)
 *   and its resolution can be scaled.
 */

#pragma once

#include <string>

#include "BLI_math_vector.hh"
#include "BLI_set.hh"

#include "DNA_scene_types.h"
#include "DRW_render.hh"

#include "draw_pass.hh"

#include "eevee_film_shared.hh"
#include "eevee_renderbuffers_shared.hh"

#include <sstream>

namespace blender::eevee {

using namespace draw;

class Instance;

/* -------------------------------------------------------------------- */
/** \name Film
 * \{ */

class Film {
 public:
  /** Stores indirection table of AOVs based on their name hash and their type. */
  StorageBuffer<AOVsInfoData> aovs_info;
  /** For debugging purpose but could be a user option in the future. */
  static constexpr bool use_box_filter = false;

  struct DepthState {
    /** Set to 0 if reverse Z is supported, 1 otherwise. */
    float clear_value = 1.0f;
    /** Set to DRW_STATE_DEPTH_GREATER_EQUAL if reverse Z is supported, DRW_STATE_DEPTH_LESS_EQUAL
     * otherwise. */
    DRWState test_state = DRW_STATE_DEPTH_LESS_EQUAL;
  } depth;

 private:
  Instance &inst_;

  /** Incoming combined buffer with post FX applied (motion blur + depth of field). */
  gpu::Texture *combined_final_tx_ = nullptr;

  /** Are we using the compute shader/pipeline. */
  bool use_compute_ = false;

  /** Copy of v3d->shading properties used to detect viewport settings update. */
  eViewLayerEEVEEPassType ui_render_pass_ = eViewLayerEEVEEPassType(0);
  std::string ui_aov_name_;

  /**
   * Main accumulation textures containing every render-pass except depth, cryptomatte and
   * combined.
   */
  Texture color_accum_tx_;
  Texture value_accum_tx_;
  /** Depth accumulation texture. Separated because using a different format. */
  Texture depth_tx_;
  /** Cryptomatte texture. Separated because it requires full floats. */
  Texture cryptomatte_tx_;
  /** Combined "Color" buffer. Double buffered to allow re-projection. */
  SwapChain<Texture, 2> combined_tx_;
  /** Weight buffers. Double buffered to allow updating it during accumulation. */
  SwapChain<Texture, 2> weight_tx_;

  PassSimple accumulate_ps_ = {"Film.Accumulate"};
  PassSimple copy_ps_ = {"Film.Copy"};
  PassSimple cryptomatte_post_ps_ = {"Film.Cryptomatte.Post"};

  FilmData &data_;
  int2 display_extent = int2(-1);

  eViewLayerEEVEEPassType enabled_passes_ = eViewLayerEEVEEPassType(0);
  /* Store the pass types needed by the viewport compositor separately, because some passes might
   * be enabled but not used by the viewport compositor, so they needn't be written. */
  eViewLayerEEVEEPassType viewport_compositor_enabled_passes_ = eViewLayerEEVEEPassType(0);
  PassCategory enabled_categories_ = PassCategory(0);
  bool use_reprojection_ = false;
  bool is_valid_render_extent_ = true;

 public:
  Film(Instance &inst, FilmData &data) : inst_(inst), data_(data) {};
  ~Film() {};

  void init(const int2 &full_extent, const rcti *output_rect);

  void sync();
  void end_sync();

  const FilmData &get_data()
  {
    return data_;
  }

  /** Accumulate the newly rendered sample contained in #RenderBuffers and blit to display. */
  void accumulate(View &view, gpu::Texture *combined_final_tx);

  /** Sort and normalize cryptomatte samples. */
  void cryptomatte_sort();

  /** Blit to display. No rendered sample needed. */
  void display();

  float *read_pass(eViewLayerEEVEEPassType pass_type, int layer_offset);
  float *read_aov(ViewLayerAOV *aov);

  gpu::Texture *get_pass_texture(eViewLayerEEVEEPassType pass_type, int layer_offset);
  gpu::Texture *get_aov_texture(ViewLayerAOV *aov);

  void write_viewport_compositor_passes();

  /** Returns shading views internal resolution. Includes overscan pixels. */
  int2 render_extent_get() const
  {
    return data_.render_extent;
  }
  inline bool is_valid_render_extent() const
  {
    return is_valid_render_extent_;
  }

  /** Size and offset of the film (taking into account render region). */
  int2 film_extent_get() const
  {
    return data_.extent;
  }
  int2 film_offset_get() const
  {
    return data_.offset;
  }

  /** Size of the whole viewport or the render, disregarding the render region. */
  int2 display_extent_get() const
  {
    return display_extent;
  }

  /** Number of padding pixels around the render target. Included inside `render_extent_get`. */
  int render_overscan_get() const
  {
    return data_.overscan;
  }

  /** Returns number of overscan pixels for the given parameters. */
  static int overscan_pixels_get(float overscan, int2 extent)
  {
    return math::ceil(max_ff(0.0f, overscan) * math::reduce_max(extent));
  }

  int scaling_factor_get() const
  {
    return data_.scaling_factor;
  }

  float2 pixel_jitter_get() const;

  float background_opacity_get() const
  {
    return data_.background_opacity;
  }

  eViewLayerEEVEEPassType enabled_passes_get() const;
  int cryptomatte_layer_len_get() const;

  /** WARNING: Film and RenderBuffers use different storage types for AO and Shadow. */
  static ePassStorageType pass_storage_type(eViewLayerEEVEEPassType pass_type)
  {
    switch (pass_type) {
      case EEVEE_RENDER_PASS_DEPTH:
      case EEVEE_RENDER_PASS_MIST:
        return PASS_STORAGE_VALUE;
      case EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT:
      case EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET:
      case EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL:
        return PASS_STORAGE_CRYPTOMATTE;
      default:
        return PASS_STORAGE_COLOR;
    }
  }

  static bool pass_is_float3(eViewLayerEEVEEPassType pass_type)
  {
    return pass_storage_type(pass_type) == PASS_STORAGE_COLOR &&
           !ELEM(pass_type,
                 EEVEE_RENDER_PASS_COMBINED,
                 EEVEE_RENDER_PASS_VECTOR,
                 EEVEE_RENDER_PASS_TRANSPARENT);
  }

  /* Returns layer offset in the accumulation texture. -1 if the pass is not enabled. */
  int pass_id_get(eViewLayerEEVEEPassType pass_type) const
  {
    switch (pass_type) {
      case EEVEE_RENDER_PASS_COMBINED:
        return data_.combined_id;
      case EEVEE_RENDER_PASS_DEPTH:
        return data_.depth_id;
      case EEVEE_RENDER_PASS_MIST:
        return data_.mist_id;
      case EEVEE_RENDER_PASS_NORMAL:
        return data_.normal_id;
      case EEVEE_RENDER_PASS_POSITION:
        return data_.position_id;
      case EEVEE_RENDER_PASS_VECTOR:
        return data_.vector_id;
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
      case EEVEE_RENDER_PASS_TRANSPARENT:
        return data_.transparent_id;
      case EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT:
        return data_.cryptomatte_object_id;
      case EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET:
        return data_.cryptomatte_asset_id;
      case EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL:
        return data_.cryptomatte_material_id;
      default:
        return -1;
    }
  }

  static const Vector<std::string> pass_to_render_pass_names(eViewLayerEEVEEPassType pass_type,
                                                             const ViewLayer *view_layer)
  {
    Vector<std::string> result;

    auto build_cryptomatte_passes = [&](const char *pass_name) {
      const int num_cryptomatte_passes = (view_layer->cryptomatte_levels + 1) / 2;
      for (int pass = 0; pass < num_cryptomatte_passes; pass++) {
        std::stringstream ss;
        ss.fill('0');
        ss << pass_name;
        ss.width(2);
        ss << pass;
        result.append(ss.str());
      }
    };

    switch (pass_type) {
      case EEVEE_RENDER_PASS_COMBINED:
        result.append(RE_PASSNAME_COMBINED);
        break;
      case EEVEE_RENDER_PASS_DEPTH:
        result.append(RE_PASSNAME_DEPTH);
        break;
      case EEVEE_RENDER_PASS_MIST:
        result.append(RE_PASSNAME_MIST);
        break;
      case EEVEE_RENDER_PASS_NORMAL:
        result.append(RE_PASSNAME_NORMAL);
        break;
      case EEVEE_RENDER_PASS_POSITION:
        result.append(RE_PASSNAME_POSITION);
        break;
      case EEVEE_RENDER_PASS_VECTOR:
        result.append(RE_PASSNAME_VECTOR);
        break;
      case EEVEE_RENDER_PASS_DIFFUSE_LIGHT:
        result.append(RE_PASSNAME_DIFFUSE_DIRECT);
        break;
      case EEVEE_RENDER_PASS_DIFFUSE_COLOR:
        result.append(RE_PASSNAME_DIFFUSE_COLOR);
        break;
      case EEVEE_RENDER_PASS_SPECULAR_LIGHT:
        result.append(RE_PASSNAME_GLOSSY_DIRECT);
        break;
      case EEVEE_RENDER_PASS_SPECULAR_COLOR:
        result.append(RE_PASSNAME_GLOSSY_COLOR);
        break;
      case EEVEE_RENDER_PASS_VOLUME_LIGHT:
        result.append(RE_PASSNAME_VOLUME_LIGHT);
        break;
      case EEVEE_RENDER_PASS_EMIT:
        result.append(RE_PASSNAME_EMIT);
        break;
      case EEVEE_RENDER_PASS_ENVIRONMENT:
        result.append(RE_PASSNAME_ENVIRONMENT);
        break;
      case EEVEE_RENDER_PASS_SHADOW:
        result.append(RE_PASSNAME_SHADOW);
        break;
      case EEVEE_RENDER_PASS_AO:
        result.append(RE_PASSNAME_AO);
        break;
      case EEVEE_RENDER_PASS_TRANSPARENT:
        result.append(RE_PASSNAME_TRANSPARENT);
        break;
      case EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT:
        build_cryptomatte_passes(RE_PASSNAME_CRYPTOMATTE_OBJECT);
        break;
      case EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET:
        build_cryptomatte_passes(RE_PASSNAME_CRYPTOMATTE_ASSET);
        break;
      case EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL:
        build_cryptomatte_passes(RE_PASSNAME_CRYPTOMATTE_MATERIAL);
        break;
      default:
        BLI_assert(0);
        break;
    }
    return result;
  }

 private:
  void init_aovs(const Set<std::string> &passes_used_by_viewport_compositor);
  void sync_mist();

  /**
   * Precompute sample weights if they are uniform across the whole film extent.
   */
  void update_sample_table();

  void init_pass(PassSimple &pass, gpu::Shader *sh);
};

/** \} */

}  // namespace blender::eevee
