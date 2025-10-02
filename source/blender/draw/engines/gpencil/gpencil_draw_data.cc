/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.hh"

#include "DNA_light_types.h"
#include "DNA_material_types.h"

#include "BKE_image.hh"
#include "BKE_material.hh"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_memblock.h"

#include "GPU_uniform_buffer.hh"

#include "IMB_imbuf_types.hh"

#include "gpencil_engine_private.hh"

namespace blender::draw::gpencil {

/* -------------------------------------------------------------------- */
/** \name Material
 * \{ */

static MaterialPool *gpencil_material_pool_add(Instance *inst)
{
  MaterialPool *matpool = static_cast<MaterialPool *>(BLI_memblock_alloc(inst->gp_material_pool));
  matpool->next = nullptr;
  matpool->used_count = 0;
  if (matpool->ubo == nullptr) {
    matpool->ubo = GPU_uniformbuf_create(sizeof(matpool->mat_data));
  }
  inst->last_material_pool = matpool;
  return matpool;
}

static gpu::Texture *gpencil_image_texture_get(::Image *image, bool *r_alpha_premult)
{
  ImageUser iuser = {nullptr};
  gpu::Texture *gpu_tex = nullptr;

  gpu_tex = BKE_image_get_gpu_texture(image, &iuser);
  *r_alpha_premult = (gpu_tex) ? (image->alpha_mode == IMA_ALPHA_PREMUL) : false;

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
  rescale_m4(mat, float3{1.0f / scale[0], 1.0f / scale[1], 0.0});
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
  clamp_v3(color, 0.0f, 1.0f);
}

/* Apply all overrides from the solid viewport mode to the GPencil material. */
static MaterialGPencilStyle *gpencil_viewport_material_overrides(
    Instance *inst,
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
      gp_style_tmp = dna::shallow_copy(*gp_style);
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
      copy_v3_v3(gp_style->fill_rgba, inst->v3d_single_color);
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

MaterialPool *gpencil_material_pool_create(Instance *inst,
                                           Object *ob,
                                           int *ofs,
                                           const bool is_vertex_mode)
{
  MaterialPool *matpool = inst->last_material_pool;

  int mat_len = BKE_object_material_used_with_fallback_eval(*ob);

  bool reuse_matpool = matpool && ((matpool->used_count + mat_len) <= GPENCIL_MATERIAL_BUFFER_LEN);

  if (reuse_matpool) {
    /* Share the matpool with other objects. Return offset to first material. */
    *ofs = matpool->used_count;
  }
  else {
    matpool = gpencil_material_pool_add(inst);
    *ofs = 0;
  }

  /* Force vertex color in solid mode with vertex paint mode. Same behavior as meshes. */
  int color_type = (inst->v3d_color_type != -1 && is_vertex_mode) ? V3D_SHADING_VERTEX_COLOR :
                                                                    inst->v3d_color_type;
  const eV3DShadingLightingMode lighting_mode = eV3DShadingLightingMode(
      (inst->v3d != nullptr) ? eV3DShadingLightingMode(inst->v3d->shading.light) :
                               V3D_LIGHTING_STUDIO);

  MaterialPool *pool = matpool;
  for (int i = 0; i < mat_len; i++) {
    if ((i > 0) && (pool->used_count == GPENCIL_MATERIAL_BUFFER_LEN)) {
      pool->next = gpencil_material_pool_add(inst);
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
        (gp_style->flag & GP_MATERIAL_DISABLE_STENCIL))
    {
      mat_data->flag |= GP_STROKE_OVERLAP;
    }

    /* Material with holdout. */
    if (gp_style->flag & GP_MATERIAL_IS_STROKE_HOLDOUT) {
      mat_data->flag |= GP_STROKE_HOLDOUT;
    }
    if (gp_style->flag & GP_MATERIAL_IS_FILL_HOLDOUT) {
      mat_data->flag |= GP_FILL_HOLDOUT;
    }

    gp_style = gpencil_viewport_material_overrides(inst, ob, color_type, gp_style, lighting_mode);

    /* Dots or Squares rotation. */
    mat_data->alignment_rot[0] = cosf(gp_style->alignment_rotation);
    mat_data->alignment_rot[1] = sinf(gp_style->alignment_rotation);

    /* Stroke Style */
    if ((gp_style->stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE) && (gp_style->sima)) {
      bool premul;
      pool->tex_stroke[mat_id] = gpencil_image_texture_get(gp_style->sima, &premul);
      mat_data->flag |= pool->tex_stroke[mat_id] ? GP_STROKE_TEXTURE_USE : GP_FLAG_NONE;
      mat_data->flag |= premul ? GP_STROKE_TEXTURE_PREMUL : GP_FLAG_NONE;
      copy_v4_v4(mat_data->stroke_color, gp_style->stroke_rgba);
      mat_data->stroke_texture_mix = 1.0f - gp_style->mix_stroke_factor;
      mat_data->stroke_u_scale = 500.0f / gp_style->texture_pixsize;
    }
    else /* if (gp_style->stroke_style == GP_MATERIAL_STROKE_STYLE_SOLID) */ {
      pool->tex_stroke[mat_id] = nullptr;
      mat_data->flag &= ~GP_STROKE_TEXTURE_USE;
      copy_v4_v4(mat_data->stroke_color, gp_style->stroke_rgba);
      mat_data->stroke_texture_mix = 0.0f;
    }

    /* Fill Style */
    if ((gp_style->fill_style == GP_MATERIAL_FILL_STYLE_TEXTURE) && (gp_style->ima)) {
      bool use_clip = (gp_style->flag & GP_MATERIAL_TEX_CLAMP) != 0;
      bool premul;
      pool->tex_fill[mat_id] = gpencil_image_texture_get(gp_style->ima, &premul);
      mat_data->flag |= pool->tex_fill[mat_id] ? GP_FILL_TEXTURE_USE : GP_FLAG_NONE;
      mat_data->flag |= premul ? GP_FILL_TEXTURE_PREMUL : GP_FLAG_NONE;
      mat_data->flag |= use_clip ? GP_FILL_TEXTURE_CLIP : GP_FLAG_NONE;
      gpencil_uv_transform_get(gp_style->texture_offset,
                               gp_style->texture_scale,
                               gp_style->texture_angle,
                               reinterpret_cast<float (*)[2]>(&mat_data->fill_uv_rot_scale),
                               mat_data->fill_uv_offset);
      copy_v4_v4(mat_data->fill_color, gp_style->fill_rgba);
      mat_data->fill_texture_mix = 1.0f - gp_style->mix_factor;
    }
    else if (gp_style->fill_style == GP_MATERIAL_FILL_STYLE_GRADIENT) {
      bool use_radial = (gp_style->gradient_type == GP_MATERIAL_GRADIENT_RADIAL);
      pool->tex_fill[mat_id] = nullptr;
      mat_data->flag |= GP_FILL_GRADIENT_USE;
      mat_data->flag |= use_radial ? GP_FILL_GRADIENT_RADIAL : GP_FLAG_NONE;
      gpencil_uv_transform_get(gp_style->texture_offset,
                               gp_style->texture_scale,
                               gp_style->texture_angle,
                               reinterpret_cast<float (*)[2]>(&mat_data->fill_uv_rot_scale),
                               mat_data->fill_uv_offset);
      copy_v4_v4(mat_data->fill_color, gp_style->fill_rgba);
      copy_v4_v4(mat_data->fill_mix_color, gp_style->mix_rgba);
      mat_data->fill_texture_mix = 1.0f - gp_style->mix_factor;
      if (gp_style->flag & GP_MATERIAL_FLIP_FILL) {
        swap_v4_v4(mat_data->fill_color, mat_data->fill_mix_color);
      }
    }
    else /* if (gp_style->fill_style == GP_MATERIAL_FILL_STYLE_SOLID) */ {
      pool->tex_fill[mat_id] = nullptr;
      copy_v4_v4(mat_data->fill_color, gp_style->fill_rgba);
      mat_data->fill_texture_mix = 0.0f;
    }
  }

  return matpool;
}

void gpencil_material_resources_get(MaterialPool *first_pool,
                                    int mat_id,
                                    gpu::Texture **r_tex_stroke,
                                    gpu::Texture **r_tex_fill,
                                    gpu::UniformBuf **r_ubo_mat)
{
  MaterialPool *matpool = first_pool;
  BLI_assert(mat_id >= 0);
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

LightPool *gpencil_light_pool_add(Instance *inst)
{
  LightPool *lightpool = static_cast<LightPool *>(BLI_memblock_alloc(inst->gp_light_pool));
  lightpool->light_used = 0;
  /* Tag light list end. */
  lightpool->light_data[0].color[0] = -1.0;
  if (lightpool->ubo == nullptr) {
    lightpool->ubo = GPU_uniformbuf_create(sizeof(lightpool->light_data));
  }
  inst->last_light_pool = lightpool;
  return lightpool;
}

void gpencil_light_ambient_add(LightPool *lightpool, const float color[3])
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

void gpencil_light_pool_populate(LightPool *lightpool, Object *ob)
{
  Light &light = DRW_object_get_data_for_drawing<Light>(*ob);

  if (lightpool->light_used >= GPENCIL_LIGHT_BUFFER_LEN) {
    return;
  }

  gpLight *gp_light = &lightpool->light_data[lightpool->light_used];
  float (*mat)[4] = reinterpret_cast<float (*)[4]>(&gp_light->right);

  if (light.type == LA_SPOT) {
    copy_m4_m4(mat, ob->world_to_object().ptr());
    gp_light->type = GP_LIGHT_TYPE_SPOT;
    gp_light->spot_size = cosf(light.spotsize * 0.5f);
    gp_light->spot_blend = (1.0f - gp_light->spot_size) * light.spotblend;
  }
  else if (light.type == LA_AREA) {
    /* Simulate area lights using a spot light. */
    normalize_m4_m4(mat, ob->object_to_world().ptr());
    invert_m4(mat);
    gp_light->type = GP_LIGHT_TYPE_SPOT;
    gp_light->spot_size = cosf(M_PI_2);
    gp_light->spot_blend = (1.0f - gp_light->spot_size) * 1.0f;
  }
  else if (light.type == LA_SUN) {
    normalize_v3_v3(gp_light->forward, ob->object_to_world().ptr()[2]);
    gp_light->type = GP_LIGHT_TYPE_SUN;
  }
  else {
    gp_light->type = GP_LIGHT_TYPE_POINT;
  }
  copy_v4_v4(gp_light->position, ob->object_to_world().location());
  copy_v3_v3(gp_light->color, &light.r);
  mul_v3_fl(gp_light->color, light.energy * light_power_get(&light));

  lightpool->light_used++;

  if (lightpool->light_used < GPENCIL_LIGHT_BUFFER_LEN) {
    /* Tag light list end. */
    gp_light[1].color[0] = -1.0f;
  }
}

LightPool *gpencil_light_pool_create(Instance *inst, Object * /*ob*/)
{
  LightPool *lightpool = inst->last_light_pool;

  if (lightpool == nullptr) {
    lightpool = gpencil_light_pool_add(inst);
  }
  /* TODO(fclem): Light linking. */
  // gpencil_light_pool_populate(lightpool, ob);

  return lightpool;
}

/** \} */

}  // namespace blender::draw::gpencil
