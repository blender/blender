/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2018, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "workbench_private.h"

#include "BKE_image.h"
#include "BKE_node.h"

#include "BLI_dynstr.h"
#include "BLI_hash.h"

#include "DNA_node_types.h"
#include "DNA_mesh_types.h"

#include "ED_uvedit.h"

#define HSV_SATURATION 0.5
#define HSV_VALUE 0.8

void workbench_material_update_data(WORKBENCH_PrivateData *wpd,
                                    Object *ob,
                                    Material *mat,
                                    WORKBENCH_MaterialData *data,
                                    int color_type)
{
  data->metallic = 0.0f;
  data->roughness = 0.632455532f; /* sqrtf(0.4f); */
  data->alpha = wpd->shading.xray_alpha;

  if (color_type == V3D_SHADING_SINGLE_COLOR) {
    copy_v3_v3(data->base_color, wpd->shading.single_color);
  }
  else if (color_type == V3D_SHADING_ERROR_COLOR) {
    copy_v3_fl3(data->base_color, 0.8, 0.0, 0.8);
  }
  else if (color_type == V3D_SHADING_RANDOM_COLOR) {
    uint hash = BLI_ghashutil_strhash_p_murmur(ob->id.name);
    if (ob->id.lib) {
      hash = (hash * 13) ^ BLI_ghashutil_strhash_p_murmur(ob->id.lib->name);
    }

    float hue = BLI_hash_int_01(hash);
    float hsv[3] = {hue, HSV_SATURATION, HSV_VALUE};
    hsv_to_rgb_v(hsv, data->base_color);
  }
  else if (ELEM(color_type, V3D_SHADING_OBJECT_COLOR, V3D_SHADING_VERTEX_COLOR)) {
    data->alpha *= ob->color[3];
    copy_v3_v3(data->base_color, ob->color);
  }
  else {
    /* V3D_SHADING_MATERIAL_COLOR or V3D_SHADING_TEXTURE_COLOR */
    if (mat) {
      data->alpha *= mat->a;
      copy_v3_v3(data->base_color, &mat->r);
      if (workbench_is_specular_highlight_enabled(wpd)) {
        data->metallic = mat->metallic;
        data->roughness = sqrtf(mat->roughness); /* Remap to disney roughness. */
      }
    }
    else {
      copy_v3_fl(data->base_color, 0.8f);
    }
  }
}

char *workbench_material_build_defines(WORKBENCH_PrivateData *wpd,
                                       bool is_uniform_color,
                                       bool is_hair,
                                       bool is_tiled,
                                       const WORKBENCH_ColorOverride color_override)
{
  char *str = NULL;
  bool use_textures = (wpd->shading.color_type == V3D_SHADING_TEXTURE_COLOR) && !is_uniform_color;
  bool use_vertex_colors = (wpd->shading.color_type == V3D_SHADING_VERTEX_COLOR) &&
                           !is_uniform_color;

  switch (color_override) {
    case WORKBENCH_COLOR_OVERRIDE_TEXTURE:
      use_textures = true;
      use_vertex_colors = false;
      is_hair = false;
      break;
    case WORKBENCH_COLOR_OVERRIDE_VERTEX:
      use_textures = false;
      use_vertex_colors = true;
      is_hair = false;
      is_tiled = false;
      break;
    case WORKBENCH_COLOR_OVERRIDE_OFF:
      break;
  }

  DynStr *ds = BLI_dynstr_new();

  if (wpd->shading.flag & V3D_SHADING_OBJECT_OUTLINE) {
    BLI_dynstr_append(ds, "#define V3D_SHADING_OBJECT_OUTLINE\n");
  }
  if (wpd->shading.flag & V3D_SHADING_SHADOW) {
    BLI_dynstr_append(ds, "#define V3D_SHADING_SHADOW\n");
  }
  if (SSAO_ENABLED(wpd) || CURVATURE_ENABLED(wpd)) {
    BLI_dynstr_append(ds, "#define WB_CAVITY\n");
  }
  if (workbench_is_specular_highlight_enabled(wpd)) {
    BLI_dynstr_append(ds, "#define V3D_SHADING_SPECULAR_HIGHLIGHT\n");
  }
  if (STUDIOLIGHT_ENABLED(wpd)) {
    BLI_dynstr_append(ds, "#define V3D_LIGHTING_STUDIO\n");
  }
  if (FLAT_ENABLED(wpd)) {
    BLI_dynstr_append(ds, "#define V3D_LIGHTING_FLAT\n");
  }
  if (MATCAP_ENABLED(wpd)) {
    BLI_dynstr_append(ds, "#define V3D_LIGHTING_MATCAP\n");
  }
  if (OBJECT_ID_PASS_ENABLED(wpd)) {
    BLI_dynstr_append(ds, "#define OBJECT_ID_PASS_ENABLED\n");
  }
  if (workbench_is_matdata_pass_enabled(wpd)) {
    BLI_dynstr_append(ds, "#define MATDATA_PASS_ENABLED\n");
  }
  if (NORMAL_VIEWPORT_PASS_ENABLED(wpd)) {
    BLI_dynstr_append(ds, "#define NORMAL_VIEWPORT_PASS_ENABLED\n");
  }
  if (use_vertex_colors) {
    BLI_dynstr_append(ds, "#define V3D_SHADING_VERTEX_COLOR\n");
  }
  if (use_textures) {
    BLI_dynstr_append(ds, "#define V3D_SHADING_TEXTURE_COLOR\n");
  }
  if (NORMAL_ENCODING_ENABLED()) {
    BLI_dynstr_append(ds, "#define WORKBENCH_ENCODE_NORMALS\n");
  }
  if (is_hair) {
    BLI_dynstr_append(ds, "#define HAIR_SHADER\n");
  }
  if (use_textures && is_tiled) {
    BLI_dynstr_append(ds, "#define TEXTURE_IMAGE_ARRAY\n");
  }

  str = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);
  return str;
}

uint workbench_material_get_hash(WORKBENCH_MaterialData *mat, bool is_ghost)
{
  union {
    struct {
      /* WHATCH: Keep in sync with View3DShading.color_type max value. */
      uchar color_type;
      uchar diff_r;
      uchar diff_g;
      uchar diff_b;

      uchar alpha;
      uchar ghost;
      uchar metal;
      uchar roughness;

      void *ima;
    };
    /* HACK to ensure input is 4 uint long. */
    uint a[4];
  } input = {.color_type = (uchar)(mat->color_type),
             .diff_r = (uchar)(mat->base_color[0] * 0xFF),
             .diff_g = (uchar)(mat->base_color[1] * 0xFF),
             .diff_b = (uchar)(mat->base_color[2] * 0xFF),

             .alpha = (uint)(mat->alpha * 0xFF),
             .ghost = (uchar)is_ghost,
             .metal = (uchar)(mat->metallic * 0xFF),
             .roughness = (uchar)(mat->roughness * 0xFF),

             .ima = mat->ima};

  BLI_assert(sizeof(input) == sizeof(uint) * 4);

  return BLI_ghashutil_uinthash_v4((uint *)&input);
}

int workbench_material_get_composite_shader_index(WORKBENCH_PrivateData *wpd)
{
  /* NOTE: change MAX_COMPOSITE_SHADERS accordingly when modifying this function. */
  int index = 0;
  /* 2 bits FLAT/STUDIO/MATCAP + Specular highlight */
  index = wpd->shading.light;
  SET_FLAG_FROM_TEST(index, wpd->shading.flag & V3D_SHADING_SHADOW, 1 << 2);
  SET_FLAG_FROM_TEST(index, wpd->shading.flag & V3D_SHADING_CAVITY, 1 << 3);
  SET_FLAG_FROM_TEST(index, wpd->shading.flag & V3D_SHADING_OBJECT_OUTLINE, 1 << 4);
  SET_FLAG_FROM_TEST(index, workbench_is_matdata_pass_enabled(wpd), 1 << 5);
  SET_FLAG_FROM_TEST(index, workbench_is_specular_highlight_enabled(wpd), 1 << 6);
  BLI_assert(index < MAX_COMPOSITE_SHADERS);
  return index;
}

int workbench_material_get_prepass_shader_index(WORKBENCH_PrivateData *wpd,
                                                bool is_uniform_color,
                                                bool is_hair,
                                                bool is_tiled,
                                                const WORKBENCH_ColorOverride color_override)
{
  bool use_textures = (wpd->shading.color_type == V3D_SHADING_TEXTURE_COLOR) && !is_uniform_color;
  bool use_vertex_colors = (wpd->shading.color_type == V3D_SHADING_VERTEX_COLOR) &&
                           !is_uniform_color;

  switch (color_override) {
    case WORKBENCH_COLOR_OVERRIDE_TEXTURE:
      use_textures = true;
      use_vertex_colors = false;
      break;
    case WORKBENCH_COLOR_OVERRIDE_VERTEX:
      use_textures = false;
      use_vertex_colors = true;
      is_tiled = false;
      break;
    case WORKBENCH_COLOR_OVERRIDE_OFF:
      break;
  }

  /* NOTE: change MAX_PREPASS_SHADERS accordingly when modifying this function. */
  int index = 0;
  SET_FLAG_FROM_TEST(index, is_hair, 1 << 0);
  SET_FLAG_FROM_TEST(index, workbench_is_matdata_pass_enabled(wpd), 1 << 1);
  SET_FLAG_FROM_TEST(index, OBJECT_ID_PASS_ENABLED(wpd), 1 << 2);
  SET_FLAG_FROM_TEST(index, NORMAL_VIEWPORT_PASS_ENABLED(wpd), 1 << 3);
  SET_FLAG_FROM_TEST(index, MATCAP_ENABLED(wpd), 1 << 4);
  SET_FLAG_FROM_TEST(index, use_textures, 1 << 5);
  SET_FLAG_FROM_TEST(index, use_vertex_colors, 1 << 6);
  SET_FLAG_FROM_TEST(index, is_tiled && use_textures, 1 << 7);
  BLI_assert(index < MAX_PREPASS_SHADERS);
  return index;
}

int workbench_material_get_accum_shader_index(WORKBENCH_PrivateData *wpd,
                                              bool is_uniform_color,
                                              bool is_hair,
                                              bool is_tiled,
                                              const WORKBENCH_ColorOverride color_override)
{
  bool use_textures = (wpd->shading.color_type == V3D_SHADING_TEXTURE_COLOR) && !is_uniform_color;
  bool use_vertex_colors = (wpd->shading.color_type == V3D_SHADING_VERTEX_COLOR) &&
                           !is_uniform_color;

  switch (color_override) {
    case WORKBENCH_COLOR_OVERRIDE_TEXTURE:
      use_textures = true;
      use_vertex_colors = false;
      is_hair = false;
      break;
    case WORKBENCH_COLOR_OVERRIDE_VERTEX:
      use_textures = false;
      use_vertex_colors = true;
      is_hair = false;
      is_tiled = false;
      break;
    case WORKBENCH_COLOR_OVERRIDE_OFF:
      break;
  }

  /* NOTE: change MAX_ACCUM_SHADERS accordingly when modifying this function. */
  int index = 0;
  /* 2 bits FLAT/STUDIO/MATCAP + Specular highlight */
  index = wpd->shading.light;
  SET_FLAG_FROM_TEST(index, use_textures, 1 << 2);
  SET_FLAG_FROM_TEST(index, use_vertex_colors, 1 << 3);
  SET_FLAG_FROM_TEST(index, is_hair, 1 << 4);
  /* 1 bits SHADOWS (only facing factor) */
  SET_FLAG_FROM_TEST(index, SHADOW_ENABLED(wpd), 1 << 5);
  SET_FLAG_FROM_TEST(index, workbench_is_specular_highlight_enabled(wpd), 1 << 6);
  SET_FLAG_FROM_TEST(index, is_tiled && use_textures, 1 << 7);
  BLI_assert(index < MAX_ACCUM_SHADERS);
  return index;
}

eV3DShadingColorType workbench_material_determine_color_type(WORKBENCH_PrivateData *wpd,
                                                             Image *ima,
                                                             Object *ob,
                                                             bool use_sculpt_pbvh)
{
  eV3DShadingColorType color_type = wpd->shading.color_type;
  const Mesh *me = (ob->type == OB_MESH) ? ob->data : NULL;

  if ((color_type == V3D_SHADING_TEXTURE_COLOR) &&
      (ima == NULL || use_sculpt_pbvh || (ob->dt < OB_TEXTURE))) {
    color_type = V3D_SHADING_MATERIAL_COLOR;
  }
  if (color_type == V3D_SHADING_VERTEX_COLOR && (me == NULL || me->mloopcol == NULL)) {
    color_type = V3D_SHADING_OBJECT_COLOR;
  }

  switch (workbench_object_color_override_get(ob)) {
    /* Force V3D_SHADING_TEXTURE_COLOR for active object when in texture painting
     * no matter the shading color that the user has chosen, when there is no
     * texture we will render the object with the error color */
    case WORKBENCH_COLOR_OVERRIDE_TEXTURE:
      color_type = ima ? V3D_SHADING_TEXTURE_COLOR : V3D_SHADING_ERROR_COLOR;
      break;

    /* Force V3D_SHADING_VERTEX_COLOR for active object when in vertex painting
     * no matter the shading color that the user has chosen, when there is no
     * vertex color we will render the object with the error color */
    case WORKBENCH_COLOR_OVERRIDE_VERTEX:
      color_type = V3D_SHADING_VERTEX_COLOR;
      break;

    case WORKBENCH_COLOR_OVERRIDE_OFF:
      break;
  }

  return color_type;
}

void workbench_material_get_image_and_mat(
    Object *ob, int mat_nr, Image **r_image, ImageUser **r_iuser, int *r_interp, Material **r_mat)
{
  bNode *node;
  *r_mat = BKE_object_material_get(ob, mat_nr);
  ED_object_get_active_image(ob, mat_nr, r_image, r_iuser, &node, NULL);
  if (node && *r_image) {
    switch (node->type) {
      case SH_NODE_TEX_IMAGE: {
        NodeTexImage *storage = node->storage;
        *r_interp = storage->interpolation;
        break;
      }
      case SH_NODE_TEX_ENVIRONMENT: {
        NodeTexEnvironment *storage = node->storage;
        *r_interp = storage->interpolation;
        break;
      }
      default:
        BLI_assert(!"Node type not supported by workbench");
        *r_interp = 0;
    }
  }
  else {
    *r_interp = 0;
  }
}

void workbench_material_shgroup_uniform(WORKBENCH_PrivateData *wpd,
                                        DRWShadingGroup *grp,
                                        WORKBENCH_MaterialData *material,
                                        Object *ob,
                                        const bool deferred,
                                        const bool is_tiled,
                                        const int interp)
{
  if (deferred && !workbench_is_matdata_pass_enabled(wpd)) {
    return;
  }

  const bool use_highlight = workbench_is_specular_highlight_enabled(wpd);
  const bool use_texture = (V3D_SHADING_TEXTURE_COLOR == workbench_material_determine_color_type(
                                                             wpd, material->ima, ob, false));
  if (use_texture) {
    if (is_tiled) {
      GPUTexture *array_tex = GPU_texture_from_blender(
          material->ima, material->iuser, NULL, GL_TEXTURE_2D_ARRAY);
      GPUTexture *data_tex = GPU_texture_from_blender(
          material->ima, material->iuser, NULL, GL_TEXTURE_1D_ARRAY);
      DRW_shgroup_uniform_texture(grp, "image_tile_array", array_tex);
      DRW_shgroup_uniform_texture(grp, "image_tile_data", data_tex);
    }
    else {
      GPUTexture *tex = GPU_texture_from_blender(
          material->ima, material->iuser, NULL, GL_TEXTURE_2D);
      DRW_shgroup_uniform_texture(grp, "image", tex);
    }
    DRW_shgroup_uniform_bool_copy(
        grp, "imagePremultiplied", (material->ima->alpha_mode == IMA_ALPHA_PREMUL));
    DRW_shgroup_uniform_bool_copy(grp, "imageNearest", (interp == SHD_INTERP_CLOSEST));
  }

  DRW_shgroup_uniform_vec4(grp, "materialColorAndMetal", material->base_color, 1);

  if (use_highlight) {
    DRW_shgroup_uniform_float(grp, "materialRoughness", &material->roughness, 1);
  }
}

void workbench_material_copy(WORKBENCH_MaterialData *dest_material,
                             const WORKBENCH_MaterialData *source_material)
{
  copy_v3_v3(dest_material->base_color, source_material->base_color);
  dest_material->metallic = source_material->metallic;
  dest_material->roughness = source_material->roughness;
  dest_material->ima = source_material->ima;
  dest_material->iuser = source_material->iuser;
}
