/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup EEVEE
 *
 * This file implements Cryptomatte for EEVEE. Cryptomatte is used to extract mattes using
 * information already available at render time. See
 * https://raw.githubusercontent.com/Psyop/Cryptomatte/master/specification/IDmattes_poster.pdf
 * for reference to the cryptomatte specification.
 *
 * The challenge with cryptomatte in EEVEE is the merging and sorting of the samples.
 * User can enable up to 3 cryptomatte layers (Object, Material and Asset).
 *
 * Process
 *
 * - Cryptomatte sample: Rendering of a cryptomatte sample is stored in a GPUBuffer. The buffer
 * holds a single float per pixel per number of active cryptomatte layers. The float is the
 * cryptomatte hash of each layer. After drawing the cryptomatte sample the intermediate result is
 * downloaded to a CPU buffer (`cryptomatte_download_buffer`).
 *
 * Accurate mode
 *
 * There are two accuracy modes. The difference between the two is the number of render samples
 * they take into account to create the render passes. When accurate mode is off the number of
 * levels is used as the number of cryptomatte samples to take. When accuracy mode is on the number
 * of render samples is used.
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "BKE_cryptomatte.h"

#include "GPU_batch.h"

#include "RE_pipeline.h"

#include "BLI_alloca.h"
#include "BLI_math_bits.h"
#include "BLI_rect.h"

#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "IMB_imbuf_types.h"

#include "eevee_private.h"

/* -------------------------------------------------------------------- */
/** \name Data Management cryptomatte accum buffer
 * \{ */

BLI_INLINE eViewLayerCryptomatteFlags eevee_cryptomatte_active_layers(const ViewLayer *view_layer)
{
  const eViewLayerCryptomatteFlags cryptomatte_layers = eViewLayerCryptomatteFlags(
      view_layer->cryptomatte_flag & VIEW_LAYER_CRYPTOMATTE_ALL);
  return cryptomatte_layers;
}

/* The number of cryptomatte layers that are enabled */
BLI_INLINE int eevee_cryptomatte_layers_count(const ViewLayer *view_layer)
{
  const eViewLayerCryptomatteFlags cryptomatte_layers = eevee_cryptomatte_active_layers(
      view_layer);
  return count_bits_i(cryptomatte_layers);
}

/* The number of render result passes are needed to store a single cryptomatte layer. Per
 * render-pass 2 cryptomatte samples can be stored. */
BLI_INLINE int eevee_cryptomatte_passes_per_layer(const ViewLayer *view_layer)
{
  const int num_cryptomatte_levels = view_layer->cryptomatte_levels;
  const int num_cryptomatte_passes = (num_cryptomatte_levels + 1) / 2;
  return num_cryptomatte_passes;
}

BLI_INLINE int eevee_cryptomatte_layer_stride(const ViewLayer *view_layer)
{
  return view_layer->cryptomatte_levels;
}

BLI_INLINE int eevee_cryptomatte_layer_offset(const ViewLayer *view_layer, const int layer)
{
  return view_layer->cryptomatte_levels * layer;
}

BLI_INLINE int eevee_cryptomatte_pixel_stride(const ViewLayer *view_layer)
{
  return eevee_cryptomatte_layer_stride(view_layer) * eevee_cryptomatte_layers_count(view_layer);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init Render-Passes
 * \{ */

void EEVEE_cryptomatte_renderpasses_init(EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  ViewLayer *view_layer = draw_ctx->view_layer;

  /* Cryptomatte is only rendered for final image renders */
  if (!DRW_state_is_scene_render()) {
    return;
  }
  const eViewLayerCryptomatteFlags active_layers = eevee_cryptomatte_active_layers(view_layer);
  if (active_layers) {
    CryptomatteSession *session = BKE_cryptomatte_init();
    if ((active_layers & VIEW_LAYER_CRYPTOMATTE_OBJECT) != 0) {
      BKE_cryptomatte_add_layer(session, "CryptoObject");
    }
    if ((active_layers & VIEW_LAYER_CRYPTOMATTE_MATERIAL) != 0) {
      BKE_cryptomatte_add_layer(session, "CryptoMaterial");
    }
    if ((active_layers & VIEW_LAYER_CRYPTOMATTE_ASSET) != 0) {
      BKE_cryptomatte_add_layer(session, "CryptoAsset");
    }
    g_data->cryptomatte_session = session;

    g_data->render_passes = eViewLayerEEVEEPassType(
        g_data->render_passes | EEVEE_RENDER_PASS_CRYPTOMATTE | EEVEE_RENDER_PASS_VOLUME_LIGHT);
    g_data->cryptomatte_accurate_mode = (view_layer->cryptomatte_flag &
                                         VIEW_LAYER_CRYPTOMATTE_ACCURATE) != 0;
  }
}

void EEVEE_cryptomatte_output_init(EEVEE_ViewLayerData * /*sldata*/,
                                   EEVEE_Data *vedata,
                                   int /*tot_samples*/)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;

  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const ViewLayer *view_layer = draw_ctx->view_layer;

  const int num_cryptomatte_layers = eevee_cryptomatte_layers_count(view_layer);
  eGPUTextureFormat format = (num_cryptomatte_layers == 1) ? GPU_R32F :
                             (num_cryptomatte_layers == 2) ? GPU_RG32F :
                                                             GPU_RGBA32F;
  const float *viewport_size = DRW_viewport_size_get();
  const int buffer_size = viewport_size[0] * viewport_size[1];

  if (g_data->cryptomatte_accum_buffer == nullptr) {
    g_data->cryptomatte_accum_buffer = static_cast<EEVEE_CryptomatteSample *>(
        MEM_calloc_arrayN(buffer_size * eevee_cryptomatte_pixel_stride(view_layer),
                          sizeof(EEVEE_CryptomatteSample),
                          __func__));
    /* Download buffer should store a float per active cryptomatte layer. */
    g_data->cryptomatte_download_buffer = static_cast<float *>(
        MEM_malloc_arrayN(buffer_size * num_cryptomatte_layers, sizeof(float), __func__));
  }
  else {
    /* During multiview rendering the `cryptomatte_accum_buffer` is deallocated after all views
     * have been rendered. Clear it here to be reused by the next view. */
    memset(g_data->cryptomatte_accum_buffer,
           0,
           buffer_size * eevee_cryptomatte_pixel_stride(view_layer) *
               sizeof(EEVEE_CryptomatteSample));
  }

  DRW_texture_ensure_fullscreen_2d(&txl->cryptomatte, format, DRWTextureFlag(0));
  GPU_framebuffer_ensure_config(&fbl->cryptomatte_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                    GPU_ATTACHMENT_TEXTURE(txl->cryptomatte),
                                });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Populate Cache
 * \{ */

void EEVEE_cryptomatte_cache_init(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  if ((vedata->stl->g_data->render_passes & EEVEE_RENDER_PASS_CRYPTOMATTE) != 0) {
    DRW_PASS_CREATE(psl->cryptomatte_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);
  }
}

static DRWShadingGroup *eevee_cryptomatte_shading_group_create(
    EEVEE_Data *vedata, EEVEE_ViewLayerData *sldata, Object *ob, Material *material, bool is_hair)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const ViewLayer *view_layer = draw_ctx->view_layer;
  const eViewLayerCryptomatteFlags cryptomatte_layers = eevee_cryptomatte_active_layers(
      view_layer);
  EEVEE_PrivateData *g_data = vedata->stl->g_data;
  float cryptohash[4] = {0.0f};

  EEVEE_PassList *psl = vedata->psl;
  int layer_offset = 0;
  if ((cryptomatte_layers & VIEW_LAYER_CRYPTOMATTE_OBJECT) != 0) {
    uint32_t cryptomatte_hash = BKE_cryptomatte_object_hash(
        g_data->cryptomatte_session, "CryptoObject", ob);
    float cryptomatte_color_value = BKE_cryptomatte_hash_to_float(cryptomatte_hash);
    cryptohash[layer_offset] = cryptomatte_color_value;
    layer_offset++;
  }
  if ((cryptomatte_layers & VIEW_LAYER_CRYPTOMATTE_MATERIAL) != 0) {
    uint32_t cryptomatte_hash = BKE_cryptomatte_material_hash(
        g_data->cryptomatte_session, "CryptoMaterial", material);
    float cryptomatte_color_value = BKE_cryptomatte_hash_to_float(cryptomatte_hash);
    cryptohash[layer_offset] = cryptomatte_color_value;
    layer_offset++;
  }
  if ((cryptomatte_layers & VIEW_LAYER_CRYPTOMATTE_ASSET) != 0) {
    uint32_t cryptomatte_hash = BKE_cryptomatte_asset_hash(
        g_data->cryptomatte_session, "CryptoAsset", ob);
    float cryptomatte_color_value = BKE_cryptomatte_hash_to_float(cryptomatte_hash);
    cryptohash[layer_offset] = cryptomatte_color_value;
    layer_offset++;
  }

  DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_cryptomatte_sh_get(is_hair),
                                            psl->cryptomatte_ps);
  DRW_shgroup_uniform_vec4_copy(grp, "cryptohash", cryptohash);
  DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);

  return grp;
}

static void eevee_cryptomatte_curves_cache_populate(EEVEE_Data *vedata,
                                                    EEVEE_ViewLayerData *sldata,
                                                    Object *ob,
                                                    ParticleSystem *psys,
                                                    ModifierData *md,
                                                    Material *material)
{
  DRWShadingGroup *grp = eevee_cryptomatte_shading_group_create(
      vedata, sldata, ob, material, true);
  DRW_shgroup_hair_create_sub(ob, psys, md, grp, nullptr);
}

void EEVEE_cryptomatte_object_curves_cache_populate(EEVEE_Data *vedata,
                                                    EEVEE_ViewLayerData *sldata,
                                                    Object *ob)
{
  BLI_assert(ob->type == OB_CURVES);
  Material *material = BKE_object_material_get_eval(ob, CURVES_MATERIAL_NR);
  DRWShadingGroup *grp = eevee_cryptomatte_shading_group_create(
      vedata, sldata, ob, material, true);
  DRW_shgroup_curves_create_sub(ob, grp, nullptr);
}

void EEVEE_cryptomatte_particle_hair_cache_populate(EEVEE_Data *vedata,
                                                    EEVEE_ViewLayerData *sldata,
                                                    Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();

  if (ob->type == OB_MESH) {
    if (ob != draw_ctx->object_edit) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type != eModifierType_ParticleSystem) {
          continue;
        }
        ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;
        if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
          continue;
        }
        ParticleSettings *part = psys->part;
        const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;
        if (draw_as != PART_DRAW_PATH) {
          continue;
        }
        Material *material = BKE_object_material_get_eval(ob, part->omat);
        eevee_cryptomatte_curves_cache_populate(vedata, sldata, ob, psys, md, material);
      }
    }
  }
}

void EEVEE_cryptomatte_cache_populate(EEVEE_Data *vedata, EEVEE_ViewLayerData *sldata, Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const ViewLayer *view_layer = draw_ctx->view_layer;
  const eViewLayerCryptomatteFlags cryptomatte_layers = eevee_cryptomatte_active_layers(
      view_layer);

  if ((cryptomatte_layers & VIEW_LAYER_CRYPTOMATTE_MATERIAL) != 0) {
    const int materials_len = DRW_cache_object_material_count_get(ob);
    GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);
    memset(gpumat_array, 0, sizeof(*gpumat_array) * materials_len);
    GPUBatch **geoms = DRW_cache_object_surface_material_get(ob, gpumat_array, materials_len);
    if (geoms) {
      for (int i = 0; i < materials_len; i++) {
        GPUBatch *geom = geoms[i];
        if (geom == nullptr) {
          continue;
        }
        Material *material = BKE_object_material_get_eval(ob, i + 1);
        DRWShadingGroup *grp = eevee_cryptomatte_shading_group_create(
            vedata, sldata, ob, material, false);
        DRW_shgroup_call(grp, geom, ob);
      }
    }
  }
  else {
    GPUBatch *geom = DRW_cache_object_surface_get(ob);
    if (geom) {
      DRWShadingGroup *grp = eevee_cryptomatte_shading_group_create(
          vedata, sldata, ob, nullptr, false);
      DRW_shgroup_call(grp, geom, ob);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Accumulate Samples
 * \{ */

/* Downloads cryptomatte sample buffer from the GPU and integrate the samples with the accumulated
 * cryptomatte samples. */
static void eevee_cryptomatte_download_buffer(EEVEE_Data *vedata, GPUFrameBuffer *framebuffer)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const ViewLayer *view_layer = draw_ctx->view_layer;
  const int num_cryptomatte_layers = eevee_cryptomatte_layers_count(view_layer);
  const int num_levels = view_layer->cryptomatte_levels;
  const float *viewport_size = DRW_viewport_size_get();
  const int buffer_size = viewport_size[0] * viewport_size[1];

  EEVEE_CryptomatteSample *accum_buffer = g_data->cryptomatte_accum_buffer;
  float *download_buffer = g_data->cryptomatte_download_buffer;

  BLI_assert(accum_buffer);
  BLI_assert(download_buffer);

  GPU_framebuffer_read_color(framebuffer,
                             0,
                             0,
                             viewport_size[0],
                             viewport_size[1],
                             num_cryptomatte_layers,
                             0,
                             GPU_DATA_FLOAT,
                             download_buffer);

  /* Integrate download buffer into the accum buffer.
   * The download buffer contains up to 3 floats per pixel (one float per cryptomatte layer.
   *
   * NOTE: here we deviate from the cryptomatte standard. During integration the standard always
   * sort the samples by its weight to make sure that samples with the lowest weight
   * are discarded first. In our case the weight of each sample is always 1 as we don't have
   * subsamples and apply the coverage during the post processing. When there is no room for new
   * samples the new samples has a weight of 1 and will always be discarded. */
  int download_pixel_index = 0;
  int accum_pixel_index = 0;
  int accum_pixel_stride = eevee_cryptomatte_pixel_stride(view_layer);
  for (int pixel_index = 0; pixel_index < buffer_size; pixel_index++) {
    for (int layer = 0; layer < num_cryptomatte_layers; layer++) {
      const int layer_offset = eevee_cryptomatte_layer_offset(view_layer, layer);
      float download_hash = download_buffer[download_pixel_index++];
      for (int level = 0; level < num_levels; level++) {
        EEVEE_CryptomatteSample *sample = &accum_buffer[accum_pixel_index + layer_offset + level];
        if (sample->hash == download_hash) {
          sample->weight += 1.0f;
          break;
        }
        /* We test against weight as hash 0.0f is used for samples hitting the world background. */
        if (sample->weight == 0.0f) {
          sample->hash = download_hash;
          sample->weight = 1.0f;
          break;
        }
      }
    }
    accum_pixel_index += accum_pixel_stride;
  }
}

void EEVEE_cryptomatte_output_accumulate(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_PassList *psl = vedata->psl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const ViewLayer *view_layer = draw_ctx->view_layer;
  const int cryptomatte_levels = view_layer->cryptomatte_levels;
  const int current_sample = effects->taa_current_sample;

  /* In accurate mode all render samples are evaluated. In inaccurate mode this is limited to the
   * number of cryptomatte levels. This will reduce the overhead of downloading the GPU buffer and
   * integrating it into the accum buffer. */
  if (g_data->cryptomatte_accurate_mode || current_sample < cryptomatte_levels) {
    static float clear_color[4] = {0.0};
    GPU_framebuffer_bind(fbl->cryptomatte_fb);
    GPU_framebuffer_clear_color(fbl->cryptomatte_fb, clear_color);
    DRW_draw_pass(psl->cryptomatte_ps);

    eevee_cryptomatte_download_buffer(vedata, fbl->cryptomatte_fb);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update Render Passes
 * \{ */

void EEVEE_cryptomatte_update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer)
{
  /* NOTE: Name channels lowercase rgba so that compression rules check in OpenEXR DWA code uses
   * lossless compression. Reportedly this naming is the only one which works good from the
   * interoperability point of view. Using XYZW naming is not portable. */

  char cryptomatte_pass_name[MAX_NAME];
  const short num_passes = eevee_cryptomatte_passes_per_layer(view_layer);
  if ((view_layer->cryptomatte_flag & VIEW_LAYER_CRYPTOMATTE_OBJECT) != 0) {
    for (short pass = 0; pass < num_passes; pass++) {
      SNPRINTF_RLEN(cryptomatte_pass_name, "CryptoObject%02d", pass);
      RE_engine_register_pass(
          engine, scene, view_layer, cryptomatte_pass_name, 4, "rgba", SOCK_RGBA);
    }
  }
  if ((view_layer->cryptomatte_flag & VIEW_LAYER_CRYPTOMATTE_MATERIAL) != 0) {
    for (short pass = 0; pass < num_passes; pass++) {
      SNPRINTF_RLEN(cryptomatte_pass_name, "CryptoMaterial%02d", pass);
      RE_engine_register_pass(
          engine, scene, view_layer, cryptomatte_pass_name, 4, "rgba", SOCK_RGBA);
    }
  }
  if ((view_layer->cryptomatte_flag & VIEW_LAYER_CRYPTOMATTE_ASSET) != 0) {
    for (short pass = 0; pass < num_passes; pass++) {
      SNPRINTF_RLEN(cryptomatte_pass_name, "CryptoAsset%02d", pass);
      RE_engine_register_pass(
          engine, scene, view_layer, cryptomatte_pass_name, 4, "rgba", SOCK_RGBA);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Construct Render Result
 * \{ */

/* Compare function for cryptomatte samples. Samples with the highest weight will be at the
 * beginning of the list. */
static int eevee_cryptomatte_sample_cmp_reverse(const void *a_, const void *b_)
{
  const EEVEE_CryptomatteSample *a = static_cast<const EEVEE_CryptomatteSample *>(a_);
  const EEVEE_CryptomatteSample *b = static_cast<const EEVEE_CryptomatteSample *>(b_);
  if (a->weight < b->weight) {
    return 1;
  }
  if (a->weight > b->weight) {
    return -1;
  }

  return 0;
}

/* Post process the weights. The accumulated weights buffer adds one to each weight per sample.
 * During post processing ensure that the total of weights per sample is between 0 and 1. */
static void eevee_cryptomatte_postprocess_weights(EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_TextureList *txl = vedata->txl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const ViewLayer *view_layer = draw_ctx->view_layer;
  const int num_cryptomatte_layers = eevee_cryptomatte_layers_count(view_layer);
  const int num_levels = view_layer->cryptomatte_levels;
  const float *viewport_size = DRW_viewport_size_get();
  const int buffer_size = viewport_size[0] * viewport_size[1];

  EEVEE_CryptomatteSample *accum_buffer = g_data->cryptomatte_accum_buffer;
  BLI_assert(accum_buffer);
  float *volumetric_transmittance_buffer = nullptr;
  if ((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) {
    volumetric_transmittance_buffer = static_cast<float *>(
        GPU_texture_read(txl->volume_transmittance_accum, GPU_DATA_FLOAT, 0));
  }
  const int num_samples = effects->taa_current_sample - 1;

  int accum_pixel_index = 0;
  int accum_pixel_stride = eevee_cryptomatte_pixel_stride(view_layer);

  for (int pixel_index = 0; pixel_index < buffer_size;
       pixel_index++, accum_pixel_index += accum_pixel_stride)
  {
    float coverage = 1.0f;
    if (volumetric_transmittance_buffer != nullptr) {
      coverage = (volumetric_transmittance_buffer[pixel_index * 4] +
                  volumetric_transmittance_buffer[pixel_index * 4 + 1] +
                  volumetric_transmittance_buffer[pixel_index * 4 + 2]) /
                 (3.0f * num_samples);
    }
    for (int layer = 0; layer < num_cryptomatte_layers; layer++) {
      const int layer_offset = eevee_cryptomatte_layer_offset(view_layer, layer);
      /* Calculate the total weight of the sample. */
      float total_weight = 0.0f;
      for (int level = 0; level < num_levels; level++) {
        EEVEE_CryptomatteSample *sample = &accum_buffer[accum_pixel_index + layer_offset + level];
        total_weight += sample->weight;
      }
      BLI_assert(total_weight > 0.0f);

      float total_weight_inv = coverage / total_weight;
      if (total_weight_inv > 0.0f) {
        for (int level = 0; level < num_levels; level++) {
          EEVEE_CryptomatteSample *sample =
              &accum_buffer[accum_pixel_index + layer_offset + level];
          /* Remove background samples. These samples were used to determine the correct weight
           * but won't be part of the final result. */
          if (sample->hash == 0.0f) {
            sample->weight = 0.0f;
          }
          sample->weight *= total_weight_inv;
        }

        /* Sort accum buffer by coverage of each sample. */
        qsort(&accum_buffer[accum_pixel_index + layer_offset],
              num_levels,
              sizeof(EEVEE_CryptomatteSample),
              eevee_cryptomatte_sample_cmp_reverse);
      }
      else {
        /* This pixel doesn't have any weight, so clear it fully. */
        for (int level = 0; level < num_levels; level++) {
          EEVEE_CryptomatteSample *sample =
              &accum_buffer[accum_pixel_index + layer_offset + level];
          sample->weight = 0.0f;
          sample->hash = 0.0f;
        }
      }
    }
  }

  if (volumetric_transmittance_buffer) {
    MEM_freeN(volumetric_transmittance_buffer);
  }
}

/* Extract cryptomatte layer from the cryptomatte_accum_buffer to render passes. */
static void eevee_cryptomatte_extract_render_passes(
    RenderLayer *rl,
    const char *viewname,
    const char *render_pass_name_format,
    EEVEE_CryptomatteSample *accum_buffer,
    /* number of render passes per cryptomatte layer. */
    const int num_cryptomatte_passes,
    const int num_cryptomatte_levels,
    const int accum_pixel_stride,
    const int layer_stride,
    const int layer_index,
    const int rect_width,
    const int rect_height,
    const int rect_offset_x,
    const int rect_offset_y,
    const int viewport_width)
{
  char cryptomatte_pass_name[MAX_NAME];
  /* A pass can store 2 levels. Technically the last pass can have a single level if the number of
   * levels is an odd number. This parameter counts the number of levels it has processed. */
  int levels_done = 0;
  for (int pass = 0; pass < num_cryptomatte_passes; pass++) {
    /* Each pass holds 2 cryptomatte samples. */
    const int pass_offset = pass * 2;
    SNPRINTF_RLEN(cryptomatte_pass_name, render_pass_name_format, pass);
    RenderPass *rp_object = RE_pass_find_by_name(rl, cryptomatte_pass_name, viewname);
    float *rp_buffer_data = rp_object->ibuf->float_buffer.data;
    for (int y = 0; y < rect_height; y++) {
      for (int x = 0; x < rect_width; x++) {
        const int accum_buffer_offset = (rect_offset_x + x +
                                         (rect_offset_y + y) * viewport_width) *
                                            accum_pixel_stride +
                                        layer_index * layer_stride + pass_offset;
        const int render_pass_offset = (y * rect_width + x) * 4;
        rp_buffer_data[render_pass_offset] = accum_buffer[accum_buffer_offset].hash;
        rp_buffer_data[render_pass_offset + 1] = accum_buffer[accum_buffer_offset].weight;
        if (levels_done + 1 < num_cryptomatte_levels) {
          rp_buffer_data[render_pass_offset + 2] = accum_buffer[accum_buffer_offset + 1].hash;
          rp_buffer_data[render_pass_offset + 3] = accum_buffer[accum_buffer_offset + 1].weight;
        }
        else {
          rp_buffer_data[render_pass_offset + 2] = 0.0f;
          rp_buffer_data[render_pass_offset + 3] = 0.0f;
        }
      }
    }
    levels_done++;
  }
}

void EEVEE_cryptomatte_render_result(RenderLayer *rl,
                                     const char *viewname,
                                     const rcti *rect,
                                     EEVEE_Data *vedata,
                                     EEVEE_ViewLayerData * /*sldata*/)
{
  EEVEE_PrivateData *g_data = vedata->stl->g_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const ViewLayer *view_layer = draw_ctx->view_layer;
  const eViewLayerCryptomatteFlags cryptomatte_layers = eViewLayerCryptomatteFlags(
      view_layer->cryptomatte_flag & VIEW_LAYER_CRYPTOMATTE_ALL);

  eevee_cryptomatte_postprocess_weights(vedata);

  const int rect_width = BLI_rcti_size_x(rect);
  const int rect_height = BLI_rcti_size_y(rect);
  const int rect_offset_x = vedata->stl->g_data->overscan_pixels + rect->xmin;
  const int rect_offset_y = vedata->stl->g_data->overscan_pixels + rect->ymin;
  const float *viewport_size = DRW_viewport_size_get();
  const int viewport_width = viewport_size[0];
  EEVEE_CryptomatteSample *accum_buffer = g_data->cryptomatte_accum_buffer;
  BLI_assert(accum_buffer);
  const int num_cryptomatte_levels = view_layer->cryptomatte_levels;
  const int num_cryptomatte_passes = eevee_cryptomatte_passes_per_layer(view_layer);
  const int layer_stride = eevee_cryptomatte_layer_stride(view_layer);
  const int accum_pixel_stride = eevee_cryptomatte_pixel_stride(view_layer);

  int layer_index = 0;
  if ((cryptomatte_layers & VIEW_LAYER_CRYPTOMATTE_OBJECT) != 0) {
    eevee_cryptomatte_extract_render_passes(rl,
                                            viewname,
                                            "CryptoObject%02d",
                                            accum_buffer,
                                            num_cryptomatte_passes,
                                            num_cryptomatte_levels,
                                            accum_pixel_stride,
                                            layer_stride,
                                            layer_index,
                                            rect_width,
                                            rect_height,
                                            rect_offset_x,
                                            rect_offset_y,
                                            viewport_width);
    layer_index++;
  }
  if ((cryptomatte_layers & VIEW_LAYER_CRYPTOMATTE_MATERIAL) != 0) {
    eevee_cryptomatte_extract_render_passes(rl,
                                            viewname,
                                            "CryptoMaterial%02d",
                                            accum_buffer,
                                            num_cryptomatte_passes,
                                            num_cryptomatte_levels,
                                            accum_pixel_stride,
                                            layer_stride,
                                            layer_index,
                                            rect_width,
                                            rect_height,
                                            rect_offset_x,
                                            rect_offset_y,
                                            viewport_width);
    layer_index++;
  }
  if ((cryptomatte_layers & VIEW_LAYER_CRYPTOMATTE_ASSET) != 0) {
    eevee_cryptomatte_extract_render_passes(rl,
                                            viewname,
                                            "CryptoAsset%02d",
                                            accum_buffer,
                                            num_cryptomatte_passes,
                                            num_cryptomatte_levels,
                                            accum_pixel_stride,
                                            layer_stride,
                                            layer_index,
                                            rect_width,
                                            rect_height,
                                            rect_offset_x,
                                            rect_offset_y,
                                            viewport_width);
    layer_index++;
  }
}

void EEVEE_cryptomatte_store_metadata(EEVEE_Data *vedata, RenderResult *render_result)
{
  EEVEE_PrivateData *g_data = vedata->stl->g_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const ViewLayer *view_layer = draw_ctx->view_layer;
  BLI_assert(g_data->cryptomatte_session);

  BKE_cryptomatte_store_metadata(g_data->cryptomatte_session, render_result, view_layer);
}

/** \} */

void EEVEE_cryptomatte_free(EEVEE_Data *vedata)
{
  EEVEE_PrivateData *g_data = vedata->stl->g_data;
  MEM_SAFE_FREE(g_data->cryptomatte_accum_buffer);
  MEM_SAFE_FREE(g_data->cryptomatte_download_buffer);
  if (g_data->cryptomatte_session) {
    BKE_cryptomatte_free(g_data->cryptomatte_session);
    g_data->cryptomatte_session = nullptr;
  }
}
