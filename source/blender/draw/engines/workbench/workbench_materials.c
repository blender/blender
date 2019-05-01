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

#include "BIF_gl.h"

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
                                    WORKBENCH_MaterialData *data)
{
  /* When V3D_SHADING_TEXTURE_COLOR is active, use V3D_SHADING_MATERIAL_COLOR as fallback when no
   * texture could be determined */
  int color_type = wpd->shading.color_type == V3D_SHADING_TEXTURE_COLOR ?
                       V3D_SHADING_MATERIAL_COLOR :
                       wpd->shading.color_type;
  copy_v3_fl3(data->diffuse_color, 0.8f, 0.8f, 0.8f);
  copy_v3_v3(data->base_color, data->diffuse_color);
  copy_v3_fl3(data->specular_color, 0.05f, 0.05f, 0.05f); /* Dielectric: 5% reflective. */
  data->metallic = 0.0f;
  data->roughness = 0.5f; /* sqrtf(0.25f); */

  if (color_type == V3D_SHADING_SINGLE_COLOR) {
    copy_v3_v3(data->diffuse_color, wpd->shading.single_color);
    copy_v3_v3(data->base_color, data->diffuse_color);
  }
  else if (color_type == V3D_SHADING_RANDOM_COLOR) {
    uint hash = BLI_ghashutil_strhash_p_murmur(ob->id.name);
    if (ob->id.lib) {
      hash = (hash * 13) ^ BLI_ghashutil_strhash_p_murmur(ob->id.lib->name);
    }

    float hue = BLI_hash_int_01(hash);
    float hsv[3] = {hue, HSV_SATURATION, HSV_VALUE};
    hsv_to_rgb_v(hsv, data->diffuse_color);
    copy_v3_v3(data->base_color, data->diffuse_color);
  }
  else if (ELEM(color_type, V3D_SHADING_OBJECT_COLOR, V3D_SHADING_VERTEX_COLOR)) {
    copy_v3_v3(data->diffuse_color, ob->color);
    copy_v3_v3(data->base_color, data->diffuse_color);
  }
  else {
    /* V3D_SHADING_MATERIAL_COLOR */
    if (mat) {
      if (SPECULAR_HIGHLIGHT_ENABLED(wpd)) {
        copy_v3_v3(data->base_color, &mat->r);
        mul_v3_v3fl(data->diffuse_color, &mat->r, 1.0f - mat->metallic);
        mul_v3_v3fl(data->specular_color, &mat->r, mat->metallic);
        add_v3_fl(data->specular_color, 0.05f * (1.0f - mat->metallic));
        data->metallic = mat->metallic;
        data->roughness = sqrtf(mat->roughness); /* Remap to disney roughness. */
      }
      else {
        copy_v3_v3(data->base_color, &mat->r);
        copy_v3_v3(data->diffuse_color, &mat->r);
      }
    }
  }
}

char *workbench_material_build_defines(WORKBENCH_PrivateData *wpd,
                                       bool is_uniform_color,
                                       bool is_hair)
{
  char *str = NULL;
  bool use_textures = (wpd->shading.color_type == V3D_SHADING_TEXTURE_COLOR) && !is_uniform_color;
  bool use_vertex_colors = (wpd->shading.color_type == V3D_SHADING_VERTEX_COLOR) &&
                           !is_uniform_color;

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
  if (SPECULAR_HIGHLIGHT_ENABLED(wpd)) {
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
  if (MATDATA_PASS_ENABLED(wpd)) {
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

  str = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);
  return str;
}

uint workbench_material_get_hash(WORKBENCH_MaterialData *material_template, bool is_ghost)
{
  uint input[4];
  uint result;
  float *color = material_template->diffuse_color;
  input[0] = (uint)(color[0] * 512);
  input[1] = (uint)(color[1] * 512);
  input[2] = (uint)(color[2] * 512);
  input[3] = material_template->object_id;
  result = BLI_ghashutil_uinthash_v4_murmur(input);

  color = material_template->specular_color;
  input[0] = (uint)(color[0] * 512);
  input[1] = (uint)(color[1] * 512);
  input[2] = (uint)(color[2] * 512);
  input[3] = (uint)(material_template->roughness * 512);
  result += BLI_ghashutil_uinthash_v4_murmur(input);

  result += BLI_ghashutil_uinthash((uint)is_ghost);
  result += BLI_ghashutil_uinthash(material_template->color_type);

  /* add texture reference */
  if (material_template->ima) {
    result += BLI_ghashutil_inthash_p_murmur(material_template->ima);
  }

  return result;
}

int workbench_material_get_composite_shader_index(WORKBENCH_PrivateData *wpd)
{
  /* NOTE: change MAX_COMPOSITE_SHADERS accordingly when modifying this function. */
  int index = 0;
  /* 2 bits FLAT/STUDIO/MATCAP + Specular highlight */
  index = SPECULAR_HIGHLIGHT_ENABLED(wpd) ? 3 : wpd->shading.light;
  SET_FLAG_FROM_TEST(index, wpd->shading.flag & V3D_SHADING_SHADOW, 1 << 2);
  SET_FLAG_FROM_TEST(index, wpd->shading.flag & V3D_SHADING_CAVITY, 1 << 3);
  SET_FLAG_FROM_TEST(index, wpd->shading.flag & V3D_SHADING_OBJECT_OUTLINE, 1 << 4);
  SET_FLAG_FROM_TEST(index, MATDATA_PASS_ENABLED(wpd), 1 << 5);
  BLI_assert(index < MAX_COMPOSITE_SHADERS);
  return index;
}

int workbench_material_get_prepass_shader_index(WORKBENCH_PrivateData *wpd,
                                                bool is_uniform_color,
                                                bool is_hair)
{
  bool use_textures = (wpd->shading.color_type == V3D_SHADING_TEXTURE_COLOR) && !is_uniform_color;
  bool use_vertex_colors = (wpd->shading.color_type == V3D_SHADING_VERTEX_COLOR) &&
                           !is_uniform_color;
  /* NOTE: change MAX_PREPASS_SHADERS accordingly when modifying this function. */
  int index = 0;
  SET_FLAG_FROM_TEST(index, is_hair, 1 << 0);
  SET_FLAG_FROM_TEST(index, MATDATA_PASS_ENABLED(wpd), 1 << 1);
  SET_FLAG_FROM_TEST(index, OBJECT_ID_PASS_ENABLED(wpd), 1 << 2);
  SET_FLAG_FROM_TEST(index, NORMAL_VIEWPORT_PASS_ENABLED(wpd), 1 << 3);
  SET_FLAG_FROM_TEST(index, MATCAP_ENABLED(wpd), 1 << 4);
  SET_FLAG_FROM_TEST(index, use_textures, 1 << 5);
  SET_FLAG_FROM_TEST(index, use_vertex_colors, 1 << 6);
  BLI_assert(index < MAX_PREPASS_SHADERS);
  return index;
}

int workbench_material_get_accum_shader_index(WORKBENCH_PrivateData *wpd,
                                              bool is_uniform_color,
                                              bool is_hair)
{
  bool use_textures = (wpd->shading.color_type == V3D_SHADING_TEXTURE_COLOR) && !is_uniform_color;
  bool use_vertex_colors = (wpd->shading.color_type == V3D_SHADING_VERTEX_COLOR) &&
                           !is_uniform_color;
  /* NOTE: change MAX_ACCUM_SHADERS accordingly when modifying this function. */
  int index = 0;
  /* 2 bits FLAT/STUDIO/MATCAP + Specular highlight */
  index = SPECULAR_HIGHLIGHT_ENABLED(wpd) ? 3 : wpd->shading.light;
  SET_FLAG_FROM_TEST(index, use_textures, 1 << 2);
  SET_FLAG_FROM_TEST(index, use_vertex_colors, 1 << 3);
  SET_FLAG_FROM_TEST(index, is_hair, 1 << 4);
  /* 1 bits SHADOWS (only facing factor) */
  SET_FLAG_FROM_TEST(index, SHADOW_ENABLED(wpd), 1 << 5);
  BLI_assert(index < MAX_ACCUM_SHADERS);
  return index;
}

int workbench_material_determine_color_type(WORKBENCH_PrivateData *wpd,
                                            Image *ima,
                                            Object *ob,
                                            bool is_sculpt_mode)
{
  int color_type = wpd->shading.color_type;
  const Mesh *me = (ob->type == OB_MESH) ? ob->data : NULL;

  if ((color_type == V3D_SHADING_TEXTURE_COLOR && (ima == NULL || is_sculpt_mode)) ||
      (ob->dt < OB_TEXTURE)) {
    color_type = V3D_SHADING_MATERIAL_COLOR;
  }
  if (color_type == V3D_SHADING_VERTEX_COLOR && (me == NULL || me->mloopcol == NULL)) {
    color_type = V3D_SHADING_OBJECT_COLOR;
  }
  return color_type;
}

void workbench_material_get_image_and_mat(
    Object *ob, int mat_nr, Image **r_image, ImageUser **r_iuser, int *r_interp, Material **r_mat)
{
  bNode *node;
  *r_mat = give_current_material(ob, mat_nr);
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
                                        const bool use_metallic,
                                        const bool deferred,
                                        const int interp)
{
  if (deferred && !MATDATA_PASS_ENABLED(wpd)) {
    return;
  }

  if (workbench_material_determine_color_type(wpd, material->ima, ob, false) ==
      V3D_SHADING_TEXTURE_COLOR) {
    ImBuf *ibuf = BKE_image_acquire_ibuf(material->ima, material->iuser, NULL);
    const bool do_color_correction = wpd->use_color_management &&
                                     (ibuf &&
                                      (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA) == 0);
    BKE_image_release_ibuf(material->ima, ibuf, NULL);
    GPUTexture *tex = GPU_texture_from_blender(
        material->ima, material->iuser, GL_TEXTURE_2D, false);
    DRW_shgroup_uniform_texture(grp, "image", tex);
    DRW_shgroup_uniform_bool_copy(grp, "imageSrgb", do_color_correction);
    DRW_shgroup_uniform_bool_copy(grp, "imageNearest", (interp == SHD_INTERP_CLOSEST));
  }
  else {
    DRW_shgroup_uniform_vec3(grp,
                             "materialDiffuseColor",
                             (use_metallic) ? material->base_color : material->diffuse_color,
                             1);
  }

  if (SPECULAR_HIGHLIGHT_ENABLED(wpd)) {
    if (use_metallic) {
      DRW_shgroup_uniform_float(grp, "materialMetallic", &material->metallic, 1);
    }
    else {
      DRW_shgroup_uniform_vec3(grp, "materialSpecularColor", material->specular_color, 1);
    }
    DRW_shgroup_uniform_float(grp, "materialRoughness", &material->roughness, 1);
  }

  if (WORLD_CLIPPING_ENABLED(wpd)) {
    DRW_shgroup_uniform_vec4(grp, "WorldClipPlanes", wpd->world_clip_planes[0], 6);
    DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
  }
}

void workbench_material_copy(WORKBENCH_MaterialData *dest_material,
                             const WORKBENCH_MaterialData *source_material)
{
  dest_material->object_id = source_material->object_id;
  copy_v3_v3(dest_material->base_color, source_material->base_color);
  copy_v3_v3(dest_material->diffuse_color, source_material->diffuse_color);
  copy_v3_v3(dest_material->specular_color, source_material->specular_color);
  dest_material->metallic = source_material->metallic;
  dest_material->roughness = source_material->roughness;
  dest_material->ima = source_material->ima;
  dest_material->iuser = source_material->iuser;
}
