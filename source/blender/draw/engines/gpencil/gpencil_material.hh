/* SPDX-FileCopyrightText: 2022 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BKE_gpencil_legacy.h"
#include "BKE_image.h"
#include "DRW_gpu_wrapper.hh"
#include "DRW_render.h"

#include "draw_manager.hh"
#include "draw_pass.hh"

namespace blender::draw::greasepencil {

using namespace draw;

class MaterialModule {
 private:
  /** Contains all materials in the scene. Indexed by gpObject.material_offset + mat_id. */
  StorageVectorBuffer<gpMaterial> materials_buf_ = "gp_materials_buf";
  /** List of all the texture used. */
  Vector<GPUTexture *> texture_pool_;

  int v3d_color_type_ = -1;
  int v3d_lighting_mode_ = V3D_LIGHTING_STUDIO;
  float v3d_xray_alpha_ = 1.0f;
  float3 v3d_single_color_ = {1.0f, 1.0f, 1.0f};

 public:
  void init(const View3D *v3d)
  {
    if (v3d != nullptr) {
      const bool shading_mode_supports_xray = (v3d->shading.type <= OB_SOLID);
      v3d_color_type_ = (v3d->shading.type == OB_SOLID) ? v3d->shading.color_type : -1;
      v3d_lighting_mode_ = v3d->shading.light;
      v3d_xray_alpha_ = (shading_mode_supports_xray && XRAY_ENABLED(v3d)) ? XRAY_ALPHA(v3d) : 1.0f;
      v3d_single_color_ = float3(v3d->shading.single_color);
    }
  }

  void begin_sync()
  {
    materials_buf_.clear();
    texture_pool_.clear();
  }

  void sync(const Object *object, const int mat_slot, bool &do_mat_holdout)
  {
    const MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings((Object *)object,
                                                                         mat_slot + 1);

    MaterialGPencilStyle gp_style_override;

    gp_style = material_override(object, &gp_style_override, gp_style);

    /* Material with holdout. */
    if (gp_style->flag & GP_MATERIAL_IS_STROKE_HOLDOUT) {
      do_mat_holdout = true;
    }
    if (gp_style->flag & GP_MATERIAL_IS_FILL_HOLDOUT) {
      do_mat_holdout = true;
    }

    materials_buf_.append(material_sync(gp_style));
  }

  void end_sync()
  {
    materials_buf_.push_update();
  }

  void bind_resources(PassMain::Sub &sub)
  {
    sub.bind_ssbo(GPENCIL_MATERIAL_SLOT, &materials_buf_);
  }

  uint object_offset_get() const
  {
    return materials_buf_.size();
  }

 private:
  /* Returns the correct flag for this texture. */
  gpMaterialFlag texture_sync(::Image *image, gpMaterialFlag use_flag, gpMaterialFlag premul_flag)
  {
    ImBuf *ibuf;
    ImageUser iuser = {nullptr};
    GPUTexture *gpu_tex = nullptr;
    void *lock;
    bool premul = false;

    if (image == nullptr) {
      texture_pool_.append(nullptr);
      return GP_FLAG_NONE;
    }

    ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

    if (ibuf != nullptr) {
      gpu_tex = BKE_image_get_gpu_texture(image, &iuser, ibuf);
      premul = (image->alpha_mode == IMA_ALPHA_PREMUL) != 0;
    }
    BKE_image_release_ibuf(image, ibuf, lock);

    texture_pool_.append(gpu_tex);

    return gpMaterialFlag(use_flag | (premul ? premul_flag : GP_FLAG_NONE));
  }

  void uv_transform_sync(const float ofs[2],
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
    float3 tmp = {1.0f / scale[0], 1.0f / scale[1], 0.0};
    rescale_m4(mat, tmp);
    rotate_m4(mat, 'Z', -rotation);
    translate_m4(mat, ofs[0], ofs[1], 0.0f);
    /* Convert to 3x2 */
    copy_v2_v2(r_rot_scale[0], mat[0]);
    copy_v2_v2(r_rot_scale[1], mat[1]);
    copy_v2_v2(r_offset, mat[3]);
  }

  /* Amend object fill color in order to avoid completely flat look. */
  void material_shade_color(float color[3])
  {
    if (v3d_lighting_mode_ == V3D_LIGHTING_FLAT) {
      return;
    }
    /* This is scene referred color, not gamma corrected and not per perceptual.
     * So we lower the threshold a bit. (1.0 / 3.0) */
    if (color[0] + color[1] + color[2] > 1.1) {
      add_v3_fl(color, -0.25f);
    }
    else {
      add_v3_fl(color, 0.15f);
    }
    CLAMP3(color, 0.0f, 1.0f);
  }

  const MaterialGPencilStyle *material_override(const Object *object,
                                                MaterialGPencilStyle *gp_style_override,
                                                const MaterialGPencilStyle *gp_style)
  {
    switch (v3d_color_type_) {
      case V3D_SHADING_MATERIAL_COLOR:
      case V3D_SHADING_RANDOM_COLOR:
        /* Random uses a random color per layer and this is done using the layer tint.
         * A simple color by object, like meshes, is not practical in grease pencil. */
        copy_v4_v4(gp_style_override->stroke_rgba, gp_style->stroke_rgba);
        copy_v4_v4(gp_style_override->fill_rgba, gp_style->fill_rgba);
        gp_style = gp_style_override;
        gp_style_override->stroke_style = GP_MATERIAL_STROKE_STYLE_SOLID;
        gp_style_override->fill_style = GP_MATERIAL_FILL_STYLE_SOLID;
        break;
      case V3D_SHADING_TEXTURE_COLOR:
        *gp_style_override = blender::dna::shallow_copy(*gp_style);
        gp_style = gp_style_override;
        if ((gp_style_override->stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE) &&
            (gp_style_override->sima))
        {
          copy_v4_fl(gp_style_override->stroke_rgba, 1.0f);
          gp_style_override->mix_stroke_factor = 0.0f;
        }

        if ((gp_style_override->fill_style == GP_MATERIAL_FILL_STYLE_TEXTURE) &&
            (gp_style_override->ima)) {
          copy_v4_fl(gp_style_override->fill_rgba, 1.0f);
          gp_style_override->mix_factor = 0.0f;
        }
        else if (gp_style_override->fill_style == GP_MATERIAL_FILL_STYLE_GRADIENT) {
          /* gp_style_override->fill_rgba is needed for correct gradient. */
          gp_style_override->mix_factor = 0.0f;
        }
        break;
      case V3D_SHADING_SINGLE_COLOR:
        gp_style = gp_style_override;
        gp_style_override->stroke_style = GP_MATERIAL_STROKE_STYLE_SOLID;
        gp_style_override->fill_style = GP_MATERIAL_FILL_STYLE_SOLID;
        copy_v3_v3(gp_style_override->fill_rgba, v3d_single_color_);
        gp_style_override->fill_rgba[3] = 1.0f;
        copy_v4_v4(gp_style_override->stroke_rgba, gp_style_override->fill_rgba);
        material_shade_color(gp_style_override->fill_rgba);
        break;
      case V3D_SHADING_OBJECT_COLOR:
        gp_style = gp_style_override;
        gp_style_override->stroke_style = GP_MATERIAL_STROKE_STYLE_SOLID;
        gp_style_override->fill_style = GP_MATERIAL_FILL_STYLE_SOLID;
        copy_v4_v4(gp_style_override->fill_rgba, object->color);
        copy_v4_v4(gp_style_override->stroke_rgba, object->color);
        material_shade_color(gp_style_override->fill_rgba);
        break;
      case V3D_SHADING_VERTEX_COLOR:
        gp_style = gp_style_override;
        gp_style_override->stroke_style = GP_MATERIAL_STROKE_STYLE_SOLID;
        gp_style_override->fill_style = GP_MATERIAL_FILL_STYLE_SOLID;
        copy_v4_fl(gp_style_override->fill_rgba, 1.0f);
        copy_v4_fl(gp_style_override->stroke_rgba, 1.0f);
        break;
      default:
        break;
    }
    return gp_style;
  }

  gpMaterial material_sync(const MaterialGPencilStyle *gp_style)
  {
    gpMaterial material;
    material.flag = 0;

    /* Dots/Square alignment. */
    if (gp_style->mode != GP_MATERIAL_MODE_LINE) {
      switch (gp_style->alignment_mode) {
        case GP_MATERIAL_FOLLOW_PATH:
          material.flag = GP_STROKE_ALIGNMENT_STROKE;
          break;
        case GP_MATERIAL_FOLLOW_OBJ:
          material.flag = GP_STROKE_ALIGNMENT_OBJECT;
          break;
        case GP_MATERIAL_FOLLOW_FIXED:
        default:
          material.flag = GP_STROKE_ALIGNMENT_FIXED;
          break;
      }
      if (gp_style->mode == GP_MATERIAL_MODE_DOT) {
        material.flag |= GP_STROKE_DOTS;
      }
    }

    /* Overlap. */
    if ((gp_style->mode != GP_MATERIAL_MODE_LINE) ||
        (gp_style->flag & GP_MATERIAL_DISABLE_STENCIL)) {
      material.flag |= GP_STROKE_OVERLAP;
    }

    /* Material with holdout. */
    if (gp_style->flag & GP_MATERIAL_IS_STROKE_HOLDOUT) {
      material.flag |= GP_STROKE_HOLDOUT;
    }
    if (gp_style->flag & GP_MATERIAL_IS_FILL_HOLDOUT) {
      material.flag |= GP_FILL_HOLDOUT;
    }

    /* Dots or Squares rotation. */
    material.alignment_rot[0] = cosf(gp_style->alignment_rotation);
    material.alignment_rot[1] = sinf(gp_style->alignment_rotation);

    if (gp_style->flag & GP_MATERIAL_STROKE_SHOW) {
      material.flag |= GP_SHOW_STROKE;
    }
    if (gp_style->flag & GP_MATERIAL_FILL_SHOW) {
      material.flag |= GP_SHOW_FILL;
    }

    /* Stroke Style */
    if ((gp_style->stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE) && (gp_style->sima)) {
      material.flag |= texture_sync(
          gp_style->sima, GP_STROKE_TEXTURE_USE, GP_STROKE_TEXTURE_PREMUL);
      copy_v4_v4(material.stroke_color, gp_style->stroke_rgba);
      material.stroke_texture_mix = 1.0f - gp_style->mix_stroke_factor;
      material.stroke_u_scale = 500.0f / gp_style->texture_pixsize;
    }
    else /* if (gp_style->stroke_style == GP_MATERIAL_STROKE_STYLE_SOLID) */ {
      texture_sync(nullptr, GP_FLAG_NONE, GP_FLAG_NONE);
      material.flag &= ~GP_STROKE_TEXTURE_USE;
      copy_v4_v4(material.stroke_color, gp_style->stroke_rgba);
      material.stroke_texture_mix = 0.0f;
    }

    /* Fill Style */
    if ((gp_style->fill_style == GP_MATERIAL_FILL_STYLE_TEXTURE) && (gp_style->ima)) {
      material.flag |= texture_sync(gp_style->ima, GP_FILL_TEXTURE_USE, GP_FILL_TEXTURE_PREMUL);
      material.flag |= (gp_style->flag & GP_MATERIAL_TEX_CLAMP) ? GP_FILL_TEXTURE_CLIP : 0;
      uv_transform_sync(gp_style->texture_offset,
                        gp_style->texture_scale,
                        gp_style->texture_angle,
                        (float(*)[2]) & material.fill_uv_rot_scale[0],
                        material.fill_uv_offset);
      copy_v4_v4(material.fill_color, gp_style->fill_rgba);
      material.fill_texture_mix = 1.0f - gp_style->mix_factor;
    }
    else if (gp_style->fill_style == GP_MATERIAL_FILL_STYLE_GRADIENT) {
      texture_sync(nullptr, GP_FLAG_NONE, GP_FLAG_NONE);
      bool use_radial = (gp_style->gradient_type == GP_MATERIAL_GRADIENT_RADIAL);
      material.flag |= GP_FILL_GRADIENT_USE;
      material.flag |= use_radial ? GP_FILL_GRADIENT_RADIAL : 0;
      uv_transform_sync(gp_style->texture_offset,
                        gp_style->texture_scale,
                        gp_style->texture_angle,
                        (float(*)[2]) & material.fill_uv_rot_scale[0],
                        material.fill_uv_offset);
      copy_v4_v4(material.fill_color, gp_style->fill_rgba);
      copy_v4_v4(material.fill_mix_color, gp_style->mix_rgba);
      material.fill_texture_mix = 1.0f - gp_style->mix_factor;
      if (gp_style->flag & GP_MATERIAL_FLIP_FILL) {
        swap_v4_v4(material.fill_color, material.fill_mix_color);
      }
    }
    else /* if (gp_style->fill_style == GP_MATERIAL_FILL_STYLE_SOLID) */ {
      texture_sync(nullptr, GP_FLAG_NONE, GP_FLAG_NONE);
      copy_v4_v4(material.fill_color, gp_style->fill_rgba);
      material.fill_texture_mix = 0.0f;
    }
    return material;
  }
};

}  // namespace blender::draw::greasepencil
