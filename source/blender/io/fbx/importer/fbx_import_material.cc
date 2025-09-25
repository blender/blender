/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"

#include "BLI_math_vector.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "DNA_material_types.h"

#include "NOD_shader.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf_types.hh"

#include "fbx_import_material.hh"

#include "ufbx.h"

namespace blender::io::fbx {

/* Nodes are arranged in columns by type, with manually placed x coordinates
 * based on node widths. */
static constexpr float node_locx_texcoord = -880.0f;
static constexpr float node_locx_mapping = -680.0f;
static constexpr float node_locx_image = -480.0f;
static constexpr float node_locx_normalmap = -200.0f;
static constexpr float node_locx_bsdf = 0.0f;
static constexpr float node_locx_output = 280.0f;

/* Nodes are arranged in rows; one row for each image being used. */
static constexpr float node_locy_top = 300.0f;
static constexpr float node_locy_step = 300.0f;

/* Add a node of the given type at the given location. */
static bNode *add_node(bNodeTree *ntree, int type, float x, float y)
{
  bNode *node = bke::node_add_static_node(nullptr, *ntree, type);
  node->location[0] = x;
  node->location[1] = y;
  return node;
}

static void link_sockets(bNodeTree *ntree,
                         bNode *from_node,
                         const char *from_node_id,
                         bNode *to_node,
                         const char *to_node_id)
{
  bNodeSocket *from_sock{bke::node_find_socket(*from_node, SOCK_OUT, from_node_id)};
  bNodeSocket *to_sock{bke::node_find_socket(*to_node, SOCK_IN, to_node_id)};
  BLI_assert(from_sock && to_sock);
  bke::node_add_link(*ntree, *from_node, *from_sock, *to_node, *to_sock);
}

static void set_socket_float(const char *socket_id, const float value, bNode *node)
{
  bNodeSocket *socket{bke::node_find_socket(*node, SOCK_IN, socket_id)};
  BLI_assert(socket && socket->type == SOCK_FLOAT);
  bNodeSocketValueFloat *dst = socket->default_value_typed<bNodeSocketValueFloat>();
  dst->value = value;
}

static void set_socket_rgb(const char *socket_id, float vr, float vg, float vb, bNode *node)
{
  bNodeSocket *socket{bke::node_find_socket(*node, SOCK_IN, socket_id)};
  BLI_assert(socket && socket->type == SOCK_RGBA);
  bNodeSocketValueRGBA *dst = socket->default_value_typed<bNodeSocketValueRGBA>();
  dst->value[0] = vr;
  dst->value[1] = vg;
  dst->value[2] = vb;
  dst->value[3] = 1.0f;
}

static void set_socket_vector(const char *socket_id, float vx, float vy, float vz, bNode *node)
{
  bNodeSocket *socket{bke::node_find_socket(*node, SOCK_IN, socket_id)};
  BLI_assert(socket && socket->type == SOCK_VECTOR);
  bNodeSocketValueVector *dst = socket->default_value_typed<bNodeSocketValueVector>();
  dst->value[0] = vx;
  dst->value[1] = vy;
  dst->value[2] = vz;
}

static float set_bsdf_float_param(bNode *bsdf,
                                  const ufbx_material_map &umap,
                                  const char *socket,
                                  float def,
                                  float min = 0.0f,
                                  float max = 1.0f,
                                  float multiplier = 1.0f)
{
  if (!umap.has_value) {
    return def * multiplier;
  }
  float value = umap.value_real * multiplier;
  value = math::clamp(value, min, max);
  set_socket_float(socket, value, bsdf);
  return value;
}

static float3 set_bsdf_color_param(bNode *bsdf,
                                   const ufbx_material_map &umap,
                                   const char *socket,
                                   float3 def,
                                   float3 min = float3(0.0f),
                                   float3 max = float3(1.0f))
{
  if (!umap.has_value || umap.value_components < 3) {
    return def;
  }
  float3 value = float3(umap.value_vec3.x, umap.value_vec3.y, umap.value_vec3.z);
  value = math::clamp(value, min, max);
  set_socket_rgb(socket, value.x, value.y, value.z, bsdf);
  return value;
}

static void set_bsdf_socket_values(bNode *bsdf, Material *mat, const ufbx_material &fmat)
{
  float3 base_color = set_bsdf_color_param(bsdf, fmat.pbr.base_color, "Base Color", float3(0.8f));
  mat->r = base_color.x;
  mat->g = base_color.y;
  mat->b = base_color.z;

  float roughness = set_bsdf_float_param(bsdf, fmat.pbr.roughness, "Roughness", 0.5f);
  mat->roughness = roughness;

  float metallic = set_bsdf_float_param(bsdf, fmat.pbr.metalness, "Metallic", 0.0f);
  mat->metallic = metallic;

  set_bsdf_float_param(bsdf, fmat.pbr.specular_ior, "IOR", 1.5f, 1.0f, 1000.0f);

  set_bsdf_float_param(bsdf, fmat.pbr.opacity, "Alpha", 1.0f);

  set_bsdf_float_param(bsdf, fmat.pbr.diffuse_roughness, "Diffuse Roughness", 0.0f);

  set_bsdf_float_param(bsdf, fmat.pbr.subsurface_factor, "Subsurface Weight", 0.0f);
  set_bsdf_float_param(bsdf, fmat.pbr.subsurface_scale, "Subsurface Scale", 0.05f);
  set_bsdf_float_param(bsdf, fmat.pbr.subsurface_anisotropy, "Subsurface Anisotropy", 0.0f);

  if (fmat.features.specular.enabled) {
    float spec = set_bsdf_float_param(
        bsdf, fmat.pbr.specular_factor, "Specular IOR Level", 0.25f, 0.0f, 1.0f, 2.0f);
    mat->spec = spec;
    set_bsdf_color_param(bsdf, fmat.pbr.specular_color, "Specular Tint", float3(1.0f));
    set_bsdf_float_param(bsdf, fmat.pbr.specular_anisotropy, "Anisotropic", 0.0f);
    set_bsdf_float_param(bsdf, fmat.pbr.specular_rotation, "Anisotropic Rotation", 0.0f);
  }

  if (ELEM(fmat.shader_type,
           UFBX_SHADER_OSL_STANDARD_SURFACE,
           UFBX_SHADER_ARNOLD_STANDARD_SURFACE,
           UFBX_SHADER_3DS_MAX_PHYSICAL_MATERIAL,
           UFBX_SHADER_3DS_MAX_PBR_METAL_ROUGH,
           UFBX_SHADER_3DS_MAX_PBR_SPEC_GLOSS,
           UFBX_SHADER_GLTF_MATERIAL,
           UFBX_SHADER_BLENDER_PHONG) &&
      fmat.features.transmission.enabled)
  {
    set_bsdf_float_param(bsdf, fmat.pbr.transmission_factor, "Transmission Weight", 0.0f);
  }

  if (fmat.features.coat.enabled) {
    set_bsdf_float_param(bsdf, fmat.pbr.coat_factor, "Coat Weight", 0.0f);
    set_bsdf_float_param(bsdf, fmat.pbr.coat_roughness, "Coat Roughness", 0.03f);
    set_bsdf_float_param(bsdf, fmat.pbr.coat_ior, "Coat IOR", 1.5f, 1.0f, 4.0f);
    set_bsdf_color_param(bsdf, fmat.pbr.coat_color, "Coat Tint", float3(1.0f));
  }

  if (fmat.features.sheen.enabled) {
    set_bsdf_float_param(bsdf, fmat.pbr.sheen_factor, "Sheen Weight", 0.0f);
    set_bsdf_float_param(bsdf, fmat.pbr.sheen_roughness, "Sheen Roughness", 0.5f);
    set_bsdf_color_param(bsdf, fmat.pbr.sheen_color, "Sheen Tint", float3(1.0f));
  }

  set_bsdf_float_param(
      bsdf, fmat.pbr.emission_factor, "Emission Strength", 0.0f, 0.0f, 1000000.0f);
  set_bsdf_color_param(bsdf,
                       fmat.pbr.emission_color,
                       "Emission Color",
                       float3(0.0f),
                       float3(0.0f),
                       float3(1000000.0f));

  set_bsdf_float_param(
      bsdf, fmat.pbr.thin_film_thickness, "Thin Film Thickness", 0.0f, 0.0f, 100000.0f);
  set_bsdf_float_param(bsdf, fmat.pbr.thin_film_ior, "Thin Film IOR", 1.33f, 1.0f, 1000.0f);
}

static Image *create_placeholder_image(Main *bmain, const std::string &path)
{
  const float color[4] = {0, 0, 0, 1};
  const char *name = BLI_path_basename(path.c_str());
  Image *image = BKE_image_add_generated(
      bmain, 32, 32, name, 24, false, IMA_GENTYPE_BLANK, color, false, false, false);
  STRNCPY(image->filepath, path.c_str());
  image->source = IMA_SRC_FILE;
  return image;
}

static Image *load_texture_image(Main *bmain, const std::string &file_dir, const ufbx_texture &tex)
{
  /* Check with filename directly. */
  Image *image = BKE_image_load_exists(bmain, tex.filename.data);
  /* Try loading as a relative path. */
  if (image == nullptr) {
    std::string path = file_dir + "/" + tex.filename.data;
    image = BKE_image_load_exists(bmain, path.c_str());
  }
  /* Try loading with absolute path from FBX. */
  if (image == nullptr) {
    image = BKE_image_load_exists(bmain, tex.absolute_filename.data);
  }

  /* If still not found, try taking progressively longer parts of the absolute path,
   * as relative to the file. */
  if (image == nullptr) {
    size_t pos = tex.absolute_filename.length;
    do {
      const char *parent_path = BLI_path_parent_dir_end(tex.absolute_filename.data, pos);
      if (parent_path == nullptr) {
        break;
      }
      char path[FILE_MAX];
      BLI_path_join(path, sizeof(path), file_dir.c_str(), parent_path);
      BLI_path_normalize(path);
      image = BKE_image_load_exists(bmain, path);
      pos = parent_path - tex.absolute_filename.data;
    } while (image == nullptr);
  }

  /* Create dummy/placeholder image. */
  if (image == nullptr) {
    image = create_placeholder_image(bmain, tex.filename.data);
  }

  /* Use embedded data for this image, if we haven't done that yet. */
  if (tex.content.size > 0 && (image == nullptr || !BKE_image_has_packedfile(image))) {
    BKE_image_free_buffers(image); /* Free cached placeholder images. */
    char *data_dup = MEM_malloc_arrayN<char>(tex.content.size, __func__);
    memcpy(data_dup, tex.content.data, tex.content.size);
    BKE_image_packfiles_from_mem(nullptr, image, data_dup, tex.content.size);

    /* Make sure the image is not marked as "generated". */
    image->source = IMA_SRC_FILE;
    image->type = IMA_TYPE_IMAGE;
  }

  return image;
}

struct FbxPbrTextureToSocket {
  ufbx_material_pbr_map slot;
  const char *socket;
};
static const FbxPbrTextureToSocket fbx_pbr_to_socket[] = {
    {UFBX_MATERIAL_PBR_BASE_COLOR, "Base Color"},
    {UFBX_MATERIAL_PBR_ROUGHNESS, "Roughness"},
    {UFBX_MATERIAL_PBR_METALNESS, "Metallic"},
    {UFBX_MATERIAL_PBR_DIFFUSE_ROUGHNESS, "Diffuse Roughness"},
    {UFBX_MATERIAL_PBR_SPECULAR_FACTOR, "Specular IOR Level"},
    {UFBX_MATERIAL_PBR_SPECULAR_COLOR, "Specular Tint"},
    {UFBX_MATERIAL_PBR_SPECULAR_IOR, "IOR"},
    {UFBX_MATERIAL_PBR_SPECULAR_ANISOTROPY, "Anisotropic"},
    {UFBX_MATERIAL_PBR_SPECULAR_ROTATION, "Anisotropic Rotation"},
    {UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR, "Transmission Weight"},
    {UFBX_MATERIAL_PBR_SUBSURFACE_FACTOR, "Subsurface Weight"},
    {UFBX_MATERIAL_PBR_SUBSURFACE_SCALE, "Subsurface Scale"},
    {UFBX_MATERIAL_PBR_SUBSURFACE_ANISOTROPY, "Subsurface Anisotropy"},
    {UFBX_MATERIAL_PBR_SHEEN_FACTOR, "Sheen Weight"},
    {UFBX_MATERIAL_PBR_SHEEN_COLOR, "Sheen Tint"},
    {UFBX_MATERIAL_PBR_SHEEN_ROUGHNESS, "Sheen Roughness"},
    {UFBX_MATERIAL_PBR_COAT_FACTOR, "Coat Weight"},
    {UFBX_MATERIAL_PBR_COAT_COLOR, "Coat Tint"},
    {UFBX_MATERIAL_PBR_COAT_ROUGHNESS, "Coat Roughness"},
    {UFBX_MATERIAL_PBR_COAT_IOR, "Coat IOR"},
    {UFBX_MATERIAL_PBR_COAT_NORMAL, "Coat Normal"},
    {UFBX_MATERIAL_PBR_THIN_FILM_THICKNESS, "Thin Film Thickness"},
    {UFBX_MATERIAL_PBR_THIN_FILM_IOR, "Thin Film IOR"},
    {UFBX_MATERIAL_PBR_EMISSION_FACTOR, "Emission Strength"},
    {UFBX_MATERIAL_PBR_EMISSION_COLOR, "Emission Color"},
    {UFBX_MATERIAL_PBR_OPACITY, "Alpha"},
    {UFBX_MATERIAL_PBR_NORMAL_MAP, "Normal"},
    {UFBX_MATERIAL_PBR_TANGENT_MAP, "Tangent"},
};

struct FbxStdTextureToSocket {
  ufbx_material_fbx_map slot;
  const char *socket;
};
static const FbxStdTextureToSocket fbx_std_to_socket[] = {
    {UFBX_MATERIAL_FBX_TRANSPARENCY_FACTOR, "Alpha"},
    {UFBX_MATERIAL_FBX_TRANSPARENCY_COLOR, "Alpha"},
    {UFBX_MATERIAL_FBX_BUMP, "Normal"},
};

static void add_image_texture(Main *bmain,
                              const std::string &file_dir,
                              bNodeTree *ntree,
                              bNode *bsdf,
                              const ufbx_material &fmat,
                              const ufbx_texture *ftex,
                              const char *socket_name,
                              float node_locy,
                              Set<StringRefNull> &done_bsdf_inputs)
{
  Image *image = load_texture_image(bmain, file_dir, *ftex);
  BLI_assert(image != nullptr);

  /* Set "non-color" color space for all "data" textures. */
  if (!STR_ELEM(
          socket_name, "Base Color", "Specular Tint", "Sheen Tint", "Coat Tint", "Emission Color"))
  {
    STRNCPY_UTF8(image->colorspace_settings.name,
                 IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DATA));
  }

  /* Add texture node and any UV transformations if needed. */
  bNode *image_node = add_node(ntree, SH_NODE_TEX_IMAGE, node_locx_image, node_locy);
  BLI_assert(image_node);
  image_node->id = &image->id;
  NodeTexImage *tex_image = static_cast<NodeTexImage *>(image_node->storage);

  /* Wrap mode. */
  tex_image->extension = SHD_IMAGE_EXTENSION_REPEAT;
  if (ftex->wrap_u == UFBX_WRAP_CLAMP || ftex->wrap_v == UFBX_WRAP_CLAMP) {
    tex_image->extension = SHD_IMAGE_EXTENSION_EXTEND;
  }

  /* UV transform. */
  if (ftex->has_uv_transform) {
    /* TODO: which UV set to use. */
    bNode *uvmap = add_node(ntree, SH_NODE_UVMAP, node_locx_texcoord, node_locy);
    bNode *mapping = add_node(ntree, SH_NODE_MAPPING, node_locx_mapping, node_locy);
    mapping->custom1 = TEXMAP_TYPE_TEXTURE;
    set_socket_vector("Location",
                      ftex->uv_transform.translation.x,
                      ftex->uv_transform.translation.y,
                      ftex->uv_transform.translation.z,
                      mapping);
    ufbx_vec3 rot = ufbx_quat_to_euler(ftex->uv_transform.rotation, UFBX_ROTATION_ORDER_XYZ);
    set_socket_vector("Rotation", -rot.x, -rot.y, -rot.z, mapping);
    set_socket_vector("Scale",
                      1.0f / ftex->uv_transform.scale.x,
                      1.0f / ftex->uv_transform.scale.y,
                      1.0f / ftex->uv_transform.scale.z,
                      mapping);

    link_sockets(ntree, uvmap, "UV", mapping, "Vector");
    link_sockets(ntree, mapping, "Vector", image_node, "Vector");
  }

  done_bsdf_inputs.add(socket_name);
  if (STREQ(socket_name, "Normal")) {
    bNode *normal_node = add_node(ntree, SH_NODE_NORMAL_MAP, node_locx_normalmap, node_locy);
    link_sockets(ntree, image_node, "Color", normal_node, "Color");
    link_sockets(ntree, normal_node, "Normal", bsdf, "Normal");

    /* Normal strength: Blender exports it as BumpFactor in FBX built-in properties. */
    float normal_strength = 1.0f;
    if (fmat.fbx.bump_factor.has_value) {
      normal_strength = fmat.fbx.bump_factor.value_real;
    }
    set_socket_float("Strength", normal_strength, normal_node);
  }
  else {
    link_sockets(ntree, image_node, "Color", bsdf, socket_name);

    if (STREQ(socket_name, "Base Color") && !done_bsdf_inputs.contains("Alpha")) {
      /* Link base color alpha (if we have one) to output alpha. */
      void *lock;
      ImBuf *ibuf = BKE_image_acquire_ibuf(image, nullptr, &lock);
      bool has_alpha = ibuf != nullptr && ibuf->planes == R_IMF_PLANES_RGBA;
      BKE_image_release_ibuf(image, ibuf, lock);

      if (has_alpha) {
        link_sockets(ntree, image_node, "Alpha", bsdf, "Alpha");
        done_bsdf_inputs.add("Alpha");
      }
    }
  }
}

static void add_image_textures(Main *bmain,
                               const std::string &file_dir,
                               bNodeTree *ntree,
                               bNode *bsdf,
                               const ufbx_material &fmat)
{
  float node_locy = node_locy_top;
  Set<StringRefNull> done_bsdf_inputs;

  /* We primarily use images from "PBR" FBX mapping. */
  for (const FbxPbrTextureToSocket &entry : fbx_pbr_to_socket) {
    BLI_assert(entry.socket != nullptr);
    if (done_bsdf_inputs.contains(entry.socket)) {
      continue; /* Already connected. */
    }

    const ufbx_texture *ftex = fmat.pbr.maps[entry.slot].texture;
    if (ftex == nullptr || !fmat.pbr.maps[entry.slot].texture_enabled) {
      /* No texture used for this slot. */
      continue;
    }

    add_image_texture(
        bmain, file_dir, ntree, bsdf, fmat, ftex, entry.socket, node_locy, done_bsdf_inputs);
    node_locy -= node_locy_step;
  }

  /* But also support several from the legacy/standard "FBX" material model,
   * mostly to match behavior of python importer. */
  for (const FbxStdTextureToSocket &entry : fbx_std_to_socket) {
    BLI_assert(entry.socket != nullptr);
    if (done_bsdf_inputs.contains(entry.socket)) {
      continue; /* Already connected. */
    }

    const ufbx_texture *ftex = fmat.fbx.maps[entry.slot].texture;
    if (ftex == nullptr || !fmat.fbx.maps[entry.slot].texture_enabled) {
      /* No texture used for this slot. */
      continue;
    }

    add_image_texture(
        bmain, file_dir, ntree, bsdf, fmat, ftex, entry.socket, node_locy, done_bsdf_inputs);
    node_locy -= node_locy_step;
  }
}

Material *import_material(Main *bmain, const std::string &base_dir, const ufbx_material &fmat)
{
  Material *mat = BKE_material_add(bmain, fmat.name.data);
  id_us_min(&mat->id);

  bNodeTree *ntree = blender::bke::node_tree_add_tree_embedded(
      nullptr, &mat->id, "Shader Nodetree", ntreeType_Shader->idname);
  bNode *bsdf = add_node(ntree, SH_NODE_BSDF_PRINCIPLED, node_locx_bsdf, node_locy_top);
  bNode *output = add_node(ntree, SH_NODE_OUTPUT_MATERIAL, node_locx_output, node_locy_top);
  set_bsdf_socket_values(bsdf, mat, fmat);
  add_image_textures(bmain, base_dir, ntree, bsdf, fmat);
  link_sockets(ntree, bsdf, "BSDF", output, "Surface");
  bke::node_set_active(*ntree, *output);

  mat->nodetree = ntree;

  BKE_ntree_update_after_single_tree_change(*bmain, *mat->nodetree);

  return mat;
}

}  // namespace blender::io::fbx
