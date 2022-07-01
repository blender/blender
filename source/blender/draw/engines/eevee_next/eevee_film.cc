/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * A film is a fullscreen buffer (usually at output extent)
 * that will be able to accumulate sample in any distorted camera_type
 * using a pixel filter.
 *
 * Input needs to be jittered so that the filter converges to the right result.
 */

#include "BLI_hash.h"
#include "BLI_rect.h"

#include "GPU_framebuffer.h"
#include "GPU_texture.h"

#include "DRW_render.h"
#include "RE_pipeline.h"

#include "eevee_film.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

ENUM_OPERATORS(eViewLayerEEVEEPassType, 1 << EEVEE_RENDER_PASS_MAX_BIT)

/* -------------------------------------------------------------------- */
/** \name Arbitrary Output Variables
 * \{ */

void Film::init_aovs()
{
  Vector<ViewLayerAOV *> aovs;

  aovs_info.display_id = -1;
  aovs_info.display_is_value = false;
  aovs_info.value_len = aovs_info.color_len = 0;

  if (inst_.is_viewport()) {
    /* Viewport case. */
    if (inst_.v3d->shading.render_pass == EEVEE_RENDER_PASS_AOV) {
      /* AOV display, request only a single AOV. */
      ViewLayerAOV *aov = (ViewLayerAOV *)BLI_findstring(
          &inst_.view_layer->aovs, inst_.v3d->shading.aov_name, offsetof(ViewLayerAOV, name));

      if (aov == nullptr) {
        /* AOV not found in view layer. */
        return;
      }

      aovs.append(aov);
      aovs_info.display_id = 0;
      aovs_info.display_is_value = (aov->type == AOV_TYPE_VALUE);
    }
    else {
      /* TODO(fclem): The realtime compositor could ask for several AOVs. */
    }
  }
  else {
    /* Render case. */
    LISTBASE_FOREACH (ViewLayerAOV *, aov, &inst_.view_layer->aovs) {
      aovs.append(aov);
    }
  }

  if (aovs.size() > AOV_MAX) {
    inst_.info = "Error: Too many AOVs";
    return;
  }

  for (ViewLayerAOV *aov : aovs) {
    bool is_value = (aov->type == AOV_TYPE_VALUE);
    uint &index = is_value ? aovs_info.value_len : aovs_info.color_len;
    uint &hash = is_value ? aovs_info.hash_value[index] : aovs_info.hash_color[index];
    hash = BLI_hash_string(aov->name);
    index++;
  }
}

float *Film::read_aov(ViewLayerAOV *aov)
{
  bool is_value = (aov->type == AOV_TYPE_VALUE);
  Texture &accum_tx = is_value ? value_accum_tx_ : color_accum_tx_;

  Span<uint> aovs_hash(is_value ? aovs_info.hash_value : aovs_info.hash_color,
                       is_value ? aovs_info.value_len : aovs_info.color_len);
  /* Find AOV index. */
  uint hash = BLI_hash_string(aov->name);
  int aov_index = -1;
  int i = 0;
  for (uint candidate_hash : aovs_hash) {
    if (candidate_hash == hash) {
      aov_index = i;
      break;
    }
    i++;
  }

  accum_tx.ensure_layer_views();

  int index = aov_index + (is_value ? data_.aov_value_id : data_.aov_color_id);
  GPUTexture *pass_tx = accum_tx.layer_view(index);

  return (float *)GPU_texture_read(pass_tx, GPU_DATA_FLOAT, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mist Pass
 * \{ */

void Film::sync_mist()
{
  const CameraData &cam = inst_.camera.data_get();
  const ::World *world = inst_.scene->world;
  float mist_start = world ? world->miststa : cam.clip_near;
  float mist_distance = world ? world->mistdist : fabsf(cam.clip_far - cam.clip_near);
  int mist_type = world ? world->mistype : (int)WO_MIST_LINEAR;

  switch (mist_type) {
    case WO_MIST_QUADRATIC:
      data_.mist_exponent = 2.0f;
      break;
    case WO_MIST_LINEAR:
      data_.mist_exponent = 1.0f;
      break;
    case WO_MIST_INVERSE_QUADRATIC:
      data_.mist_exponent = 0.5f;
      break;
  }

  data_.mist_scale = 1.0 / mist_distance;
  data_.mist_bias = -mist_start / mist_distance;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FilmData
 * \{ */

inline bool operator==(const FilmData &a, const FilmData &b)
{
  return (a.extent == b.extent) && (a.offset == b.offset) && (a.filter_size == b.filter_size) &&
         (a.scaling_factor == b.scaling_factor);
}

inline bool operator!=(const FilmData &a, const FilmData &b)
{
  return !(a == b);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Film
 * \{ */

void Film::init(const int2 &extent, const rcti *output_rect)
{
  init_aovs();

  {
    /* Enable passes that need to be rendered. */
    eViewLayerEEVEEPassType render_passes;

    if (inst_.is_viewport()) {
      /* Viewport Case. */
      render_passes = eViewLayerEEVEEPassType(inst_.v3d->shading.render_pass);

      if (inst_.overlays_enabled() || inst_.gpencil_engine_enabled) {
        /* Overlays and Grease Pencil needs the depth for correct compositing.
         * Using the render pass ensure we store the center depth. */
        render_passes |= EEVEE_RENDER_PASS_Z;
      }
    }
    else {
      /* Render Case. */
      render_passes = eViewLayerEEVEEPassType(inst_.view_layer->eevee.render_passes);

      render_passes |= EEVEE_RENDER_PASS_COMBINED;

#define ENABLE_FROM_LEGACY(name_legacy, name_eevee) \
  SET_FLAG_FROM_TEST(render_passes, \
                     (inst_.view_layer->passflag & SCE_PASS_##name_legacy) != 0, \
                     EEVEE_RENDER_PASS_##name_eevee);

      ENABLE_FROM_LEGACY(Z, Z)
      ENABLE_FROM_LEGACY(MIST, MIST)
      ENABLE_FROM_LEGACY(NORMAL, NORMAL)
      ENABLE_FROM_LEGACY(SHADOW, SHADOW)
      ENABLE_FROM_LEGACY(AO, AO)
      ENABLE_FROM_LEGACY(EMIT, EMIT)
      ENABLE_FROM_LEGACY(ENVIRONMENT, ENVIRONMENT)
      ENABLE_FROM_LEGACY(DIFFUSE_COLOR, DIFFUSE_COLOR)
      ENABLE_FROM_LEGACY(GLOSSY_COLOR, SPECULAR_COLOR)
      ENABLE_FROM_LEGACY(DIFFUSE_DIRECT, DIFFUSE_LIGHT)
      ENABLE_FROM_LEGACY(GLOSSY_DIRECT, SPECULAR_LIGHT)
      ENABLE_FROM_LEGACY(ENVIRONMENT, ENVIRONMENT)

#undef ENABLE_FROM_LEGACY
    }

    /* Filter obsolete passes. */
    render_passes &= ~(EEVEE_RENDER_PASS_UNUSED_8 | EEVEE_RENDER_PASS_BLOOM);

    /* TODO(@fclem): Can't we rely on depsgraph update notification? */
    if (assign_if_different(enabled_passes_, render_passes)) {
      inst_.sampling.reset();
    }
  }
  {
    rcti fallback_rect;
    if (BLI_rcti_is_empty(output_rect)) {
      BLI_rcti_init(&fallback_rect, 0, extent[0], 0, extent[1]);
      output_rect = &fallback_rect;
    }

    FilmData data = data_;
    data.extent = int2(BLI_rcti_size_x(output_rect), BLI_rcti_size_y(output_rect));
    data.offset = int2(output_rect->xmin, output_rect->ymin);
    data.filter_size = clamp_f(inst_.scene->r.gauss, 0.0f, 100.0f);
    /* TODO(fclem): parameter hidden in experimental.
     * We need to figure out LOD bias first in order to preserve texture crispiness. */
    data.scaling_factor = 1;

    FilmData &data_prev_ = data_;
    if (assign_if_different(data_prev_, data)) {
      inst_.sampling.reset();
    }

    const eViewLayerEEVEEPassType data_passes = EEVEE_RENDER_PASS_Z | EEVEE_RENDER_PASS_NORMAL |
                                                EEVEE_RENDER_PASS_VECTOR;
    const eViewLayerEEVEEPassType color_passes_1 = EEVEE_RENDER_PASS_DIFFUSE_LIGHT |
                                                   EEVEE_RENDER_PASS_SPECULAR_LIGHT |
                                                   EEVEE_RENDER_PASS_VOLUME_LIGHT |
                                                   EEVEE_RENDER_PASS_EMIT;
    const eViewLayerEEVEEPassType color_passes_2 = EEVEE_RENDER_PASS_DIFFUSE_COLOR |
                                                   EEVEE_RENDER_PASS_SPECULAR_COLOR |
                                                   EEVEE_RENDER_PASS_ENVIRONMENT |
                                                   EEVEE_RENDER_PASS_MIST |
                                                   EEVEE_RENDER_PASS_SHADOW | EEVEE_RENDER_PASS_AO;

    data_.exposure = 1.0f /* TODO */;
    data_.has_data = (enabled_passes_ & data_passes) != 0;
    data_.any_render_pass_1 = (enabled_passes_ & color_passes_1) != 0;
    data_.any_render_pass_2 = (enabled_passes_ & color_passes_2) != 0;
  }
  {
    /* Set pass offsets.  */

    data_.display_id = aovs_info.display_id;
    data_.display_is_value = aovs_info.display_is_value;

    /* Combined is in a separate buffer. */
    data_.combined_id = (enabled_passes_ & EEVEE_RENDER_PASS_COMBINED) ? 0 : -1;
    /* Depth is in a separate buffer. */
    data_.depth_id = (enabled_passes_ & EEVEE_RENDER_PASS_Z) ? 0 : -1;

    data_.color_len = 0;
    data_.value_len = 0;

    auto pass_index_get = [&](eViewLayerEEVEEPassType pass_type) {
      bool is_value = pass_is_value(pass_type);
      int index = (enabled_passes_ & pass_type) ?
                      (is_value ? data_.value_len : data_.color_len)++ :
                      -1;
      if (inst_.is_viewport() && inst_.v3d->shading.render_pass == pass_type) {
        data_.display_id = index;
        data_.display_is_value = is_value;
      }
      return index;
    };

    data_.mist_id = pass_index_get(EEVEE_RENDER_PASS_MIST);
    data_.normal_id = pass_index_get(EEVEE_RENDER_PASS_NORMAL);
    data_.vector_id = pass_index_get(EEVEE_RENDER_PASS_VECTOR);
    data_.diffuse_light_id = pass_index_get(EEVEE_RENDER_PASS_DIFFUSE_LIGHT);
    data_.diffuse_color_id = pass_index_get(EEVEE_RENDER_PASS_DIFFUSE_COLOR);
    data_.specular_light_id = pass_index_get(EEVEE_RENDER_PASS_SPECULAR_LIGHT);
    data_.specular_color_id = pass_index_get(EEVEE_RENDER_PASS_SPECULAR_COLOR);
    data_.volume_light_id = pass_index_get(EEVEE_RENDER_PASS_VOLUME_LIGHT);
    data_.emission_id = pass_index_get(EEVEE_RENDER_PASS_EMIT);
    data_.environment_id = pass_index_get(EEVEE_RENDER_PASS_ENVIRONMENT);
    data_.shadow_id = pass_index_get(EEVEE_RENDER_PASS_SHADOW);
    data_.ambient_occlusion_id = pass_index_get(EEVEE_RENDER_PASS_AO);

    data_.aov_color_id = data_.color_len;
    data_.aov_value_id = data_.value_len;

    data_.aov_color_len = aovs_info.color_len;
    data_.aov_value_len = aovs_info.value_len;

    data_.color_len += data_.aov_color_len;
    data_.value_len += data_.aov_value_len;
  }
  {
    /* TODO(@fclem): Over-scans. */

    render_extent_ = math::divide_ceil(extent, int2(data_.scaling_factor));
    int2 weight_extent = inst_.camera.is_panoramic() ? data_.extent : int2(data_.scaling_factor);

    eGPUTextureFormat color_format = GPU_RGBA16F;
    eGPUTextureFormat float_format = GPU_R16F;
    eGPUTextureFormat weight_format = GPU_R32F;
    eGPUTextureFormat depth_format = GPU_R32F;

    int reset = 0;
    reset += depth_tx_.ensure_2d(depth_format, data_.extent);
    reset += combined_tx_.current().ensure_2d(color_format, data_.extent);
    reset += combined_tx_.next().ensure_2d(color_format, data_.extent);
    /* Two layers, one for nearest sample weight and one for weight accumulation. */
    reset += weight_tx_.current().ensure_2d_array(weight_format, weight_extent, 2);
    reset += weight_tx_.next().ensure_2d_array(weight_format, weight_extent, 2);
    reset += color_accum_tx_.ensure_2d_array(color_format,
                                             (data_.color_len > 0) ? data_.extent : int2(1),
                                             (data_.color_len > 0) ? data_.color_len : 1);
    reset += value_accum_tx_.ensure_2d_array(float_format,
                                             (data_.value_len > 0) ? data_.extent : int2(1),
                                             (data_.value_len > 0) ? data_.value_len : 1);

    if (reset > 0) {
      inst_.sampling.reset();
      data_.use_history = 0;
      data_.use_reprojection = 0;

      /* Avoid NaN in uninitialized texture memory making history blending dangerous. */
      color_accum_tx_.clear(float4(0.0f));
      value_accum_tx_.clear(float4(0.0f));
      combined_tx_.current().clear(float4(0.0f));
      weight_tx_.current().clear(float4(0.0f));
      depth_tx_.clear(float4(0.0f));
    }
  }
}

void Film::sync()
{
  /* We use a fragment shader for viewport because we need to output the depth. */
  bool use_compute = (inst_.is_viewport() == false);

  eShaderType shader = use_compute ? FILM_COMP : FILM_FRAG;

  /* TODO(fclem): Shader variation for panoramic & scaled resolution. */

  RenderBuffers &rbuffers = inst_.render_buffers;

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS;
  accumulate_ps_ = DRW_pass_create("Film.Accumulate", state);
  GPUShader *sh = inst_.shaders.static_shader_get(shader);
  DRWShadingGroup *grp = DRW_shgroup_create(sh, accumulate_ps_);
  DRW_shgroup_uniform_block_ref(grp, "film_buf", &data_);
  DRW_shgroup_uniform_texture_ref(grp, "depth_tx", &rbuffers.depth_tx);
  DRW_shgroup_uniform_texture_ref(grp, "combined_tx", &rbuffers.combined_tx);
  DRW_shgroup_uniform_texture_ref(grp, "normal_tx", &rbuffers.normal_tx);
  DRW_shgroup_uniform_texture_ref(grp, "vector_tx", &rbuffers.vector_tx);
  DRW_shgroup_uniform_texture_ref(grp, "diffuse_light_tx", &rbuffers.diffuse_light_tx);
  DRW_shgroup_uniform_texture_ref(grp, "diffuse_color_tx", &rbuffers.diffuse_color_tx);
  DRW_shgroup_uniform_texture_ref(grp, "specular_light_tx", &rbuffers.specular_light_tx);
  DRW_shgroup_uniform_texture_ref(grp, "specular_color_tx", &rbuffers.specular_color_tx);
  DRW_shgroup_uniform_texture_ref(grp, "volume_light_tx", &rbuffers.volume_light_tx);
  DRW_shgroup_uniform_texture_ref(grp, "emission_tx", &rbuffers.emission_tx);
  DRW_shgroup_uniform_texture_ref(grp, "environment_tx", &rbuffers.environment_tx);
  DRW_shgroup_uniform_texture_ref(grp, "shadow_tx", &rbuffers.shadow_tx);
  DRW_shgroup_uniform_texture_ref(grp, "ambient_occlusion_tx", &rbuffers.ambient_occlusion_tx);
  DRW_shgroup_uniform_texture_ref(grp, "aov_color_tx", &rbuffers.aov_color_tx);
  DRW_shgroup_uniform_texture_ref(grp, "aov_value_tx", &rbuffers.aov_value_tx);
  /* NOTE(@fclem): 16 is the max number of sampled texture in many implementations.
   * If we need more, we need to pack more of the similar passes in the same textures as arrays or
   * use image binding instead. */
  DRW_shgroup_uniform_image_ref(grp, "in_weight_img", &weight_tx_.current());
  DRW_shgroup_uniform_image_ref(grp, "out_weight_img", &weight_tx_.next());
  DRW_shgroup_uniform_image_ref(grp, "in_combined_img", &combined_tx_.current());
  DRW_shgroup_uniform_image_ref(grp, "out_combined_img", &combined_tx_.next());
  DRW_shgroup_uniform_image_ref(grp, "depth_img", &depth_tx_);
  DRW_shgroup_uniform_image_ref(grp, "color_accum_img", &color_accum_tx_);
  DRW_shgroup_uniform_image_ref(grp, "value_accum_img", &value_accum_tx_);
  /* Sync with rendering passes. */
  DRW_shgroup_barrier(grp, GPU_BARRIER_TEXTURE_FETCH);
  /* Sync with rendering passes. */
  DRW_shgroup_barrier(grp, GPU_BARRIER_SHADER_IMAGE_ACCESS);
  if (use_compute) {
    int2 dispatch_size = math::divide_ceil(data_.extent, int2(FILM_GROUP_SIZE));
    DRW_shgroup_call_compute(grp, UNPACK2(dispatch_size), 1);
  }
  else {
    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
  }
}

void Film::end_sync()
{
  if (inst_.sampling.is_reset()) {
    data_.use_history = 0;
  }

  // if (camera.changed_type) {
  //   data_.use_reprojection = false;
  // }

  aovs_info.push_update();

  sync_mist();
}

float2 Film::pixel_jitter_get() const
{
  float2 jitter = inst_.sampling.rng_2d_get(SAMPLING_FILTER_U);

  if (data_.filter_size < M_SQRT1_2 && !inst_.camera.is_panoramic()) {
    /* For filter size less than a pixel, change sampling strategy and use a uniform disk
     * distribution covering the filter shape. This avoids putting samples in areas without any
     * weights. */
    /* TODO(fclem): Importance sampling could be a better option here. */
    jitter = Sampling::sample_disk(jitter) * data_.filter_size;
  }
  else {
    /* Jitter the size of a whole pixel. */
    jitter = jitter * 2.0f - 1.0f;
  }
  /* TODO(fclem): Mixed-resolution rendering: We need to offset to each of the target pixel covered
   * by a render pixel, ideally, by choosing one randomly using another sampling dimension, or by
   * repeating the same sample RNG sequence for each pixel offset. */
  return jitter;
}

void Film::update_sample_table()
{
  data_.subpixel_offset = pixel_jitter_get();

  int filter_size_ceil = ceilf(data_.filter_size);
  float filter_size_sqr = square_f(data_.filter_size);

  data_.samples_len = 0;
  if (data_.filter_size < 0.01f) {
    /* Disable filtering. */
    data_.samples[0].texel = int2(0, 0);
    data_.samples[0].weight = 1.0f;
    data_.samples_weight_total = 1.0f;
    data_.samples_len = 1;
  }
  /* NOTE: Threshold determined by hand until we don't hit the assert bellow. */
  else if (data_.filter_size < 2.20f) {
    /* Small filter Size. */
    int closest_index = 0;
    float closest_distance = FLT_MAX;
    data_.samples_weight_total = 0.0f;
    /* TODO(fclem): For optimization, could try Z-tile ordering. */
    for (int y = -filter_size_ceil; y <= filter_size_ceil; y++) {
      for (int x = -filter_size_ceil; x <= filter_size_ceil; x++) {
        float2 pixel_offset = float2(x, y) - data_.subpixel_offset;
        float distance_sqr = math::length_squared(pixel_offset);
        if (distance_sqr < filter_size_sqr) {
          if (data_.samples_len >= FILM_PRECOMP_SAMPLE_MAX) {
            BLI_assert_msg(0, "Precomputed sample table is too small.");
            break;
          }
          FilmSample &sample = data_.samples[data_.samples_len];
          sample.texel = int2(x, y);
          sample.weight = film_filter_weight(data_.filter_size, distance_sqr);
          data_.samples_weight_total += sample.weight;

          if (distance_sqr < closest_distance) {
            closest_distance = distance_sqr;
            closest_index = data_.samples_len;
          }
          data_.samples_len++;
        }
      }
    }
    /* Put the closest one in first position. */
    if (closest_index != 0) {
      SWAP(FilmSample, data_.samples[closest_index], data_.samples[0]);
    }
  }
  else {
    /* Large Filter Size. */
    MutableSpan<FilmSample> sample_table(data_.samples, FILM_PRECOMP_SAMPLE_MAX);
    /* To avoid hitting driver TDR and slowing rendering too much we use random sampling. */
    /* TODO(fclem): This case needs more work. We could distribute the samples better to avoid
     * loading the same pixel twice. */
    data_.samples_len = sample_table.size();
    data_.samples_weight_total = 0.0f;

    int i = 0;
    for (FilmSample &sample : sample_table) {
      /* TODO(fclem): Own RNG. */
      float2 random_2d = inst_.sampling.rng_2d_get(SAMPLING_FILTER_U);
      /* This randomization makes sure we converge to the right result but also makes nearest
       * neighbor filtering not converging rapidly. */
      random_2d.x = (random_2d.x + i) / float(FILM_PRECOMP_SAMPLE_MAX);

      float2 pixel_offset = math::floor(Sampling::sample_spiral(random_2d) * data_.filter_size);
      sample.texel = int2(pixel_offset);

      float distance_sqr = math::length_squared(pixel_offset - data_.subpixel_offset);
      sample.weight = film_filter_weight(data_.filter_size, distance_sqr);
      data_.samples_weight_total += sample.weight;
      i++;
    }
  }
}

void Film::accumulate(const DRWView *view)
{
  if (inst_.is_viewport()) {
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    GPU_framebuffer_bind(dfbl->default_fb);
    GPU_framebuffer_viewport_set(dfbl->default_fb, UNPACK2(data_.offset), UNPACK2(data_.extent));
  }

  update_sample_table();

  data_.display_only = false;
  data_.push_update();

  DRW_view_set_active(view);
  DRW_draw_pass(accumulate_ps_);

  combined_tx_.swap();
  weight_tx_.swap();

  /* Use history after first sample. */
  if (data_.use_history == 0) {
    data_.use_history = 1;
    data_.use_reprojection = 1;
  }
}

void Film::display()
{
  BLI_assert(inst_.is_viewport());

  /* Acquire dummy render buffers for correct binding. They will not be used. */
  inst_.render_buffers.acquire(int2(1), (void *)this);

  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  GPU_framebuffer_bind(dfbl->default_fb);
  GPU_framebuffer_viewport_set(dfbl->default_fb, UNPACK2(data_.offset), UNPACK2(data_.extent));

  data_.display_only = true;
  data_.push_update();

  DRW_view_set_active(nullptr);
  DRW_draw_pass(accumulate_ps_);

  inst_.render_buffers.release();

  /* IMPORTANT: Do not swap! No accumulation has happened. */
}

float *Film::read_pass(eViewLayerEEVEEPassType pass_type)
{

  bool is_value = pass_is_value(pass_type);
  Texture &accum_tx = (pass_type == EEVEE_RENDER_PASS_COMBINED) ?
                          combined_tx_.current() :
                      (pass_type == EEVEE_RENDER_PASS_Z) ?
                          depth_tx_ :
                          (is_value ? value_accum_tx_ : color_accum_tx_);

  accum_tx.ensure_layer_views();

  int index = pass_id_get(pass_type);
  GPUTexture *pass_tx = accum_tx.layer_view(index);

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  return (float *)GPU_texture_read(pass_tx, GPU_DATA_FLOAT, 0);
}

/** \} */

}  // namespace blender::eevee
