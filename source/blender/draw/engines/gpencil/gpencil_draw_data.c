/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "DNA_light_types.h"

#include "BKE_image.h"

#include "BLI_hash.h"
#include "BLI_math_color.h"
#include "BLI_memblock.h"

#include "GPU_uniform_buffer.h"

#include "IMB_imbuf_types.h"

#include "gpencil_engine.h"

/* -------------------------------------------------------------------- */
/** \name Material
 * \{ */

static GPENCIL_MaterialPool *gpencil_material_pool_add(GPENCIL_PrivateData *pd)
{
  GPENCIL_MaterialPool *matpool = BLI_memblock_alloc(pd->gp_material_pool);
  matpool->next = NULL;
  matpool->used_count = 0;
  if (matpool->ubo == NULL) {
    matpool->ubo = GPU_uniformbuf_create(sizeof(matpool->mat_data));
  }
  pd->last_material_pool = matpool;
  return matpool;
}

static GPUTexture *gpencil_image_texture_get(Image *image, bool *r_alpha_premult)
{
  ImBuf *ibuf;
  ImageUser iuser = {NULL};
  GPUTexture *gpu_tex = NULL;
  void *lock;

  ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

  if (ibuf != NULL && ibuf->byte_buffer.data != NULL) {
    gpu_tex = BKE_image_get_gpu_texture(image, &iuser, ibuf);
    *r_alpha_premult = (image->alpha_mode == IMA_ALPHA_PREMUL);
  }
  BKE_image_release_ibuf(image, ibuf, lock);

  return gpu_tex;
}

static void gpencil_uv_transform_get(const float ofs[2],
                                     const float scale[2],
                                     const float rotation,
                                     float r_rot_scale[2][2],
                                     float r_offset[2])
{
  /* OPTI this could use 3x2 matrices and reduce the number of operations drastically. */
  float mat[4][4];
  unit_m4(mat);
  /* Offset to center. */
  translate_m4(mat, 0.5f, 0.5f, 0.0f);
  /* Reversed order. */
  rescale_m4(mat, (float[3]){1.0f / scale[0], 1.0f / scale[1], 0.0});
  rotate_m4(mat, 'Z', -rotation);
  translate_m4(mat, ofs[0], ofs[1], 0.0f);
  /* Convert to 3x2 */
  copy_v2_v2(r_rot_scale[0], mat[0]);
  copy_v2_v2(r_rot_scale[1], mat[1]);
  copy_v2_v2(r_offset, mat[3]);
}

static void gpencil_shade_color(float color[3])
{
  /* This is scene refereed color, not gamma corrected and not per perceptual.
   * So we lower the threshold a bit. (1.0 / 3.0) */
  if (color[0] + color[1] + color[2] > 1.1) {
    add_v3_fl(color, -0.25f);
  }
  else {
    add_v3_fl(color, 0.15f);
  }
  CLAMP3(color, 0.0f, 1.0f);
}

/* Apply all overrides from the solid viewport mode to the GPencil material. */
static MaterialGPencilStyle *gpencil_viewport_material_overrides(
    GPENCIL_PrivateData *pd,
    Object *ob,
    int color_type,
    MaterialGPencilStyle *gp_style,
    const eV3DShadingLightingMode lighting_mode)
{
  static MaterialGPencilStyle gp_style_tmp;

  switch (color_type) {
    case V3D_SHADING_MATERIAL_COLOR:
    case V3D_SHADING_RANDOM_COLOR:
      /* Random uses a random color by layer and this is done using the tint
       * layer. A simple color by object, like meshes, is not practical in
       * grease pencil. */
      copy_v4_v4(gp_style_tmp.stroke_rgba, gp_style->stroke_rgba);
      copy_v4_v4(gp_style_tmp.fill_rgba, gp_style->fill_rgba);
      gp_style = &gp_style_tmp;
      gp_style->stroke_style = GP_MATERIAL_STROKE_STYLE_SOLID;
      gp_style->fill_style = GP_MATERIAL_FILL_STYLE_SOLID;
      break;
    case V3D_SHADING_TEXTURE_COLOR:
      memcpy(&gp_style_tmp, gp_style, sizeof(*gp_style));
      gp_style = &gp_style_tmp;
      if ((gp_style->stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE) && (gp_style->sima)) {
        copy_v4_fl(gp_style->stroke_rgba, 1.0f);
        gp_style->mix_stroke_factor = 0.0f;
      }

      if ((gp_style->fill_style == GP_MATERIAL_FILL_STYLE_TEXTURE) && (gp_style->ima)) {
        copy_v4_fl(gp_style->fill_rgba, 1.0f);
        gp_style->mix_factor = 0.0f;
      }
      else if (gp_style->fill_style == GP_MATERIAL_FILL_STYLE_GRADIENT) {
        /* gp_style->fill_rgba is needed for correct gradient. */
        gp_style->mix_factor = 0.0f;
      }
      break;
    case V3D_SHADING_SINGLE_COLOR:
      gp_style = &gp_style_tmp;
      gp_style->stroke_style = GP_MATERIAL_STROKE_STYLE_SOLID;
      gp_style->fill_style = GP_MATERIAL_FILL_STYLE_SOLID;
      copy_v3_v3(gp_style->fill_rgba, pd->v3d_single_color);
      gp_style->fill_rgba[3] = 1.0f;
      copy_v4_v4(gp_style->stroke_rgba, gp_style->fill_rgba);
      if (lighting_mode != V3D_LIGHTING_FLAT) {
        gpencil_shade_color(gp_style->fill_rgba);
      }
      break;
    case V3D_SHADING_OBJECT_COLOR:
      gp_style = &gp_style_tmp;
      gp_style->stroke_style = GP_MATERIAL_STROKE_STYLE_SOLID;
      gp_style->fill_style = GP_MATERIAL_FILL_STYLE_SOLID;
      copy_v4_v4(gp_style->fill_rgba, ob->color);
      copy_v4_v4(gp_style->stroke_rgba, ob->color);
      if (lighting_mode != V3D_LIGHTING_FLAT) {
        gpencil_shade_color(gp_style->fill_rgba);
      }
      break;
    case V3D_SHADING_VERTEX_COLOR:
      gp_style = &gp_style_tmp;
      gp_style->stroke_style = GP_MATERIAL_STROKE_STYLE_SOLID;
      gp_style->fill_style = GP_MATERIAL_FILL_STYLE_SOLID;
      copy_v4_fl(gp_style->fill_rgba, 1.0f);
      copy_v4_fl(gp_style->stroke_rgba, 1.0f);
      break;
    default:
      break;
  }
  return gp_style;
}

GPENCIL_MaterialPool *gpencil_material_pool_create(GPENCIL_PrivateData *pd, Object *ob, int *ofs)
{
  GPENCIL_MaterialPool *matpool = pd->last_material_pool;

  int mat_len = max_ii(1, BKE_object_material_count_eval(ob));

  bool reuse_matpool = matpool && ((matpool->used_count + mat_len) <= GPENCIL_MATERIAL_BUFFER_LEN);

  if (reuse_matpool) {
    /* Share the matpool with other objects. Return offset to first material. */
    *ofs = matpool->used_count;
  }
  else {
    matpool = gpencil_material_pool_add(pd);
    *ofs = 0;
  }

  /* Force vertex color in solid mode with vertex paint mode. Same behavior as meshes. */
  bGPdata *gpd = (bGPdata *)ob->data;
  int color_type = (pd->v3d_color_type != -1 && GPENCIL_VERTEX_MODE(gpd)) ?
                       V3D_SHADING_VERTEX_COLOR :
                       pd->v3d_color_type;
  const eV3DShadingLightingMode lighting_mode = (pd->v3d != NULL) ? pd->v3d->shading.light :
                                                                    V3D_LIGHTING_STUDIO;

  GPENCIL_MaterialPool *pool = matpool;
  for (int i = 0; i < mat_len; i++) {
    if ((i > 0) && (pool->used_count == GPENCIL_MATERIAL_BUFFER_LEN)) {
      pool->next = gpencil_material_pool_add(pd);
      pool = pool->next;
    }
    int mat_id = pool->used_count++;

    gpMaterial *mat_data = &pool->mat_data[mat_id];
    MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, i + 1);

    if (gp_style->mode == GP_MATERIAL_MODE_LINE) {
      mat_data->flag = 0;
    }
    else {
      switch (gp_style->alignment_mode) {
        case GP_MATERIAL_FOLLOW_PATH:
          mat_data->flag = GP_STROKE_ALIGNMENT_STROKE;
          break;
        case GP_MATERIAL_FOLLOW_OBJ:
          mat_data->flag = GP_STROKE_ALIGNMENT_OBJECT;
          break;
        case GP_MATERIAL_FOLLOW_FIXED:
        default:
          mat_data->flag = GP_STROKE_ALIGNMENT_FIXED;
          break;
      }

      if (gp_style->mode == GP_MATERIAL_MODE_DOT) {
        mat_data->flag |= GP_STROKE_DOTS;
      }
    }

    if ((gp_style->mode != GP_MATERIAL_MODE_LINE) ||
        (gp_style->flag & GP_MATERIAL_DISABLE_STENCIL)) {
      mat_data->flag |= GP_STROKE_OVERLAP;
    }

    /* Material with holdout. */
    if (gp_style->flag & GP_MATERIAL_IS_STROKE_HOLDOUT) {
      mat_data->flag |= GP_STROKE_HOLDOUT;
    }
    if (gp_style->flag & GP_MATERIAL_IS_FILL_HOLDOUT) {
      mat_data->flag |= GP_FILL_HOLDOUT;
    }

    if (gp_style->flag & GP_MATERIAL_STROKE_SHOW) {
      mat_data->flag |= GP_SHOW_STROKE;
    }
    if (gp_style->flag & GP_MATERIAL_FILL_SHOW) {
      mat_data->flag |= GP_SHOW_FILL;
    }

    gp_style = gpencil_viewport_material_overrides(pd, ob, color_type, gp_style, lighting_mode);

    /* Dots or Squares rotation. */
    mat_data->alignment_rot[0] = cosf(gp_style->alignment_rotation);
    mat_data->alignment_rot[1] = sinf(gp_style->alignment_rotation);

    /* Stroke Style */
    if ((gp_style->stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE) && (gp_style->sima)) {
      bool premul;
      pool->tex_stroke[mat_id] = gpencil_image_texture_get(gp_style->sima, &premul);
      mat_data->flag |= pool->tex_stroke[mat_id] ? GP_STROKE_TEXTURE_USE : 0;
      mat_data->flag |= premul ? GP_STROKE_TEXTURE_PREMUL : 0;
      copy_v4_v4(mat_data->stroke_color, gp_style->stroke_rgba);
      mat_data->stroke_texture_mix = 1.0f - gp_style->mix_stroke_factor;
      mat_data->stroke_u_scale = 500.0f / gp_style->texture_pixsize;
    }
    else /* if (gp_style->stroke_style == GP_MATERIAL_STROKE_STYLE_SOLID) */ {
      pool->tex_stroke[mat_id] = NULL;
      mat_data->flag &= ~GP_STROKE_TEXTURE_USE;
      copy_v4_v4(mat_data->stroke_color, gp_style->stroke_rgba);
      mat_data->stroke_texture_mix = 0.0f;
    }

    /* Fill Style */
    if ((gp_style->fill_style == GP_MATERIAL_FILL_STYLE_TEXTURE) && (gp_style->ima)) {
      bool use_clip = (gp_style->flag & GP_MATERIAL_TEX_CLAMP) != 0;
      bool premul;
      pool->tex_fill[mat_id] = gpencil_image_texture_get(gp_style->ima, &premul);
      mat_data->flag |= pool->tex_fill[mat_id] ? GP_FILL_TEXTURE_USE : 0;
      mat_data->flag |= premul ? GP_FILL_TEXTURE_PREMUL : 0;
      mat_data->flag |= use_clip ? GP_FILL_TEXTURE_CLIP : 0;
      gpencil_uv_transform_get(gp_style->texture_offset,
                               gp_style->texture_scale,
                               gp_style->texture_angle,
                               (float(*)[2])mat_data->fill_uv_rot_scale,
                               mat_data->fill_uv_offset);
      copy_v4_v4(mat_data->fill_color, gp_style->fill_rgba);
      mat_data->fill_texture_mix = 1.0f - gp_style->mix_factor;
    }
    else if (gp_style->fill_style == GP_MATERIAL_FILL_STYLE_GRADIENT) {
      bool use_radial = (gp_style->gradient_type == GP_MATERIAL_GRADIENT_RADIAL);
      pool->tex_fill[mat_id] = NULL;
      mat_data->flag |= GP_FILL_GRADIENT_USE;
      mat_data->flag |= use_radial ? GP_FILL_GRADIENT_RADIAL : 0;
      gpencil_uv_transform_get(gp_style->texture_offset,
                               gp_style->texture_scale,
                               gp_style->texture_angle,
                               (float(*)[2])mat_data->fill_uv_rot_scale,
                               mat_data->fill_uv_offset);
      copy_v4_v4(mat_data->fill_color, gp_style->fill_rgba);
      copy_v4_v4(mat_data->fill_mix_color, gp_style->mix_rgba);
      mat_data->fill_texture_mix = 1.0f - gp_style->mix_factor;
      if (gp_style->flag & GP_MATERIAL_FLIP_FILL) {
        swap_v4_v4(mat_data->fill_color, mat_data->fill_mix_color);
      }
    }
    else /* if (gp_style->fill_style == GP_MATERIAL_FILL_STYLE_SOLID) */ {
      pool->tex_fill[mat_id] = NULL;
      copy_v4_v4(mat_data->fill_color, gp_style->fill_rgba);
      mat_data->fill_texture_mix = 0.0f;
    }
  }

  return matpool;
}

void gpencil_material_resources_get(GPENCIL_MaterialPool *first_pool,
                                    int mat_id,
                                    GPUTexture **r_tex_stroke,
                                    GPUTexture **r_tex_fill,
                                    GPUUniformBuf **r_ubo_mat)
{
  GPENCIL_MaterialPool *matpool = first_pool;
  int pool_id = mat_id / GPENCIL_MATERIAL_BUFFER_LEN;
  for (int i = 0; i < pool_id; i++) {
    matpool = matpool->next;
  }
  mat_id = mat_id % GPENCIL_MATERIAL_BUFFER_LEN;
  *r_ubo_mat = matpool->ubo;
  if (r_tex_fill) {
    *r_tex_fill = matpool->tex_fill[mat_id];
  }
  if (r_tex_stroke) {
    *r_tex_stroke = matpool->tex_stroke[mat_id];
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lights
 * \{ */

GPENCIL_LightPool *gpencil_light_pool_add(GPENCIL_PrivateData *pd)
{
  GPENCIL_LightPool *lightpool = BLI_memblock_alloc(pd->gp_light_pool);
  lightpool->light_used = 0;
  /* Tag light list end. */
  lightpool->light_data[0].color[0] = -1.0;
  if (lightpool->ubo == NULL) {
    lightpool->ubo = GPU_uniformbuf_create(sizeof(lightpool->light_data));
  }
  pd->last_light_pool = lightpool;
  return lightpool;
}

void gpencil_light_ambient_add(GPENCIL_LightPool *lightpool, const float color[3])
{
  if (lightpool->light_used >= GPENCIL_LIGHT_BUFFER_LEN) {
    return;
  }

  gpLight *gp_light = &lightpool->light_data[lightpool->light_used];
  gp_light->type = GP_LIGHT_TYPE_AMBIENT;
  copy_v3_v3(gp_light->color, color);
  lightpool->light_used++;

  if (lightpool->light_used < GPENCIL_LIGHT_BUFFER_LEN) {
    /* Tag light list end. */
    gp_light[1].color[0] = -1.0f;
  }
}

static float light_power_get(const Light *la)
{
  if (la->type == LA_AREA) {
    return 1.0f / (4.0f * M_PI);
  }
  if (ELEM(la->type, LA_SPOT, LA_LOCAL)) {
    return 1.0f / (4.0f * M_PI * M_PI);
  }

  return 1.0f / M_PI;
}

void gpencil_light_pool_populate(GPENCIL_LightPool *lightpool, Object *ob)
{
  Light *la = (Light *)ob->data;

  if (lightpool->light_used >= GPENCIL_LIGHT_BUFFER_LEN) {
    return;
  }

  gpLight *gp_light = &lightpool->light_data[lightpool->light_used];
  float(*mat)[4] = (float(*)[4])gp_light->right;

  if (la->type == LA_SPOT) {
    copy_m4_m4(mat, ob->world_to_object);
    gp_light->type = GP_LIGHT_TYPE_SPOT;
    gp_light->spot_size = cosf(la->spotsize * 0.5f);
    gp_light->spot_blend = (1.0f - gp_light->spot_size) * la->spotblend;
  }
  else if (la->type == LA_AREA) {
    /* Simulate area lights using a spot light. */
    normalize_m4_m4(mat, ob->object_to_world);
    invert_m4(mat);
    gp_light->type = GP_LIGHT_TYPE_SPOT;
    gp_light->spot_size = cosf(M_PI_2);
    gp_light->spot_blend = (1.0f - gp_light->spot_size) * 1.0f;
  }
  else if (la->type == LA_SUN) {
    normalize_v3_v3(gp_light->forward, ob->object_to_world[2]);
    gp_light->type = GP_LIGHT_TYPE_SUN;
  }
  else {
    gp_light->type = GP_LIGHT_TYPE_POINT;
  }
  copy_v4_v4(gp_light->position, ob->object_to_world[3]);
  copy_v3_v3(gp_light->color, &la->r);
  mul_v3_fl(gp_light->color, la->energy * light_power_get(la));

  lightpool->light_used++;

  if (lightpool->light_used < GPENCIL_LIGHT_BUFFER_LEN) {
    /* Tag light list end. */
    gp_light[1].color[0] = -1.0f;
  }
}

GPENCIL_LightPool *gpencil_light_pool_create(GPENCIL_PrivateData *pd, Object *UNUSED(ob))
{
  GPENCIL_LightPool *lightpool = pd->last_light_pool;

  if (lightpool == NULL) {
    lightpool = gpencil_light_pool_add(pd);
  }
  /* TODO(fclem): Light linking. */
  // gpencil_light_pool_populate(lightpool, ob);

  return lightpool;
}

void gpencil_material_pool_free(void *storage)
{
  GPENCIL_MaterialPool *matpool = (GPENCIL_MaterialPool *)storage;
  DRW_UBO_FREE_SAFE(matpool->ubo);
}

void gpencil_light_pool_free(void *storage)
{
  GPENCIL_LightPool *lightpool = (GPENCIL_LightPool *)storage;
  DRW_UBO_FREE_SAFE(lightpool->ubo);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Data
 * \{ */

static void gpencil_view_layer_data_free(void *storage)
{
  GPENCIL_ViewLayerData *vldata = (GPENCIL_ViewLayerData *)storage;

  BLI_memblock_destroy(vldata->gp_light_pool, gpencil_light_pool_free);
  BLI_memblock_destroy(vldata->gp_material_pool, gpencil_material_pool_free);
  BLI_memblock_destroy(vldata->gp_maskbit_pool, NULL);
  BLI_memblock_destroy(vldata->gp_object_pool, NULL);
  BLI_memblock_destroy(vldata->gp_layer_pool, NULL);
  BLI_memblock_destroy(vldata->gp_vfx_pool, NULL);
}

GPENCIL_ViewLayerData *GPENCIL_view_layer_data_ensure(void)
{
  GPENCIL_ViewLayerData **vldata = (GPENCIL_ViewLayerData **)DRW_view_layer_engine_data_ensure(
      &draw_engine_gpencil_type, gpencil_view_layer_data_free);

  /* NOTE(@fclem): Putting this stuff in view-layer means it is shared by all viewports.
   * For now it is ok, but in the future, it could become a problem if we implement
   * the caching system. */
  if (*vldata == NULL) {
    *vldata = MEM_callocN(sizeof(**vldata), "GPENCIL_ViewLayerData");

    (*vldata)->gp_light_pool = BLI_memblock_create(sizeof(GPENCIL_LightPool));
    (*vldata)->gp_material_pool = BLI_memblock_create(sizeof(GPENCIL_MaterialPool));
    (*vldata)->gp_maskbit_pool = BLI_memblock_create(BLI_BITMAP_SIZE(GP_MAX_MASKBITS));
    (*vldata)->gp_object_pool = BLI_memblock_create(sizeof(GPENCIL_tObject));
    (*vldata)->gp_layer_pool = BLI_memblock_create(sizeof(GPENCIL_tLayer));
    (*vldata)->gp_vfx_pool = BLI_memblock_create(sizeof(GPENCIL_tVfx));
  }

  return *vldata;
}

/** \} */
