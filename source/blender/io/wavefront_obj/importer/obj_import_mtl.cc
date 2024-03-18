/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BKE_image.h"
#include "BKE_main.hh"
#include "BKE_node.hh"

#include "BLI_map.hh"
#include "BLI_math_vector.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "NOD_shader.h"

#include "obj_export_mtl.hh"
#include "obj_import_mtl.hh"
#include "obj_import_string_utils.hh"

#include <iostream>

namespace blender::io::obj {

/**
 * Set the socket's (of given ID) value to the given number(s).
 * Only float value(s) can be set using this method.
 */
static void set_property_of_socket(eNodeSocketDatatype property_type,
                                   const char *socket_id,
                                   Span<float> value,
                                   bNode *r_node)
{
  BLI_assert(r_node);
  bNodeSocket *socket{nodeFindSocket(r_node, SOCK_IN, socket_id)};
  BLI_assert(socket && socket->type == property_type);
  switch (property_type) {
    case SOCK_FLOAT: {
      BLI_assert(value.size() == 1);
      static_cast<bNodeSocketValueFloat *>(socket->default_value)->value = value[0];
      break;
    }
    case SOCK_RGBA: {
      /* Alpha will be added manually. It is not read from the MTL file either. */
      BLI_assert(value.size() == 3);
      copy_v3_v3(static_cast<bNodeSocketValueRGBA *>(socket->default_value)->value, value.data());
      static_cast<bNodeSocketValueRGBA *>(socket->default_value)->value[3] = 1.0f;
      break;
    }
    case SOCK_VECTOR: {
      BLI_assert(value.size() == 3);
      copy_v4_v4(static_cast<bNodeSocketValueVector *>(socket->default_value)->value,
                 value.data());
      break;
    }
    default: {
      BLI_assert(0);
      break;
    }
  }
}

static Image *load_image_at_path(Main *bmain, const std::string &path, bool relative_paths)
{
  Image *image = BKE_image_load_exists(bmain, path.c_str());
  if (!image) {
    fprintf(stderr, "Cannot load image file: '%s'\n", path.c_str());
    return nullptr;
  }
  fprintf(stderr, "Loaded image from: '%s'\n", path.c_str());
  if (relative_paths) {
    BLI_path_rel(image->filepath, BKE_main_blendfile_path(bmain));
  }
  return image;
}

static Image *create_placeholder_image(Main *bmain, const std::string &path)
{
  const float color[4] = {0, 0, 0, 1};
  Image *image = BKE_image_add_generated(bmain,
                                         32,
                                         32,
                                         BLI_path_basename(path.c_str()),
                                         24,
                                         false,
                                         IMA_GENTYPE_BLANK,
                                         color,
                                         false,
                                         false,
                                         false);
  STRNCPY(image->filepath, path.c_str());
  image->source = IMA_SRC_FILE;
  return image;
}

static Image *load_texture_image(Main *bmain, const MTLTexMap &tex_map, bool relative_paths)
{
  Image *image = nullptr;

  /* Remove quotes. */
  std::string image_path{tex_map.image_path};
  auto end_pos = std::remove(image_path.begin(), image_path.end(), '"');
  image_path.erase(end_pos, image_path.end());

  /* First try treating texture path as relative. */
  std::string tex_path{tex_map.mtl_dir_path + image_path};
  image = load_image_at_path(bmain, tex_path, relative_paths);
  if (image != nullptr) {
    return image;
  }
  /* Then try using it directly as absolute path. */
  image = load_image_at_path(bmain, image_path, relative_paths);
  if (image != nullptr) {
    return image;
  }
  /* Try replacing underscores with spaces. */
  std::string no_underscore_path{image_path};
  std::replace(no_underscore_path.begin(), no_underscore_path.end(), '_', ' ');
  if (!ELEM(no_underscore_path, image_path, tex_path)) {
    image = load_image_at_path(bmain, no_underscore_path, relative_paths);
    if (image != nullptr) {
      return image;
    }
  }
  /* Try taking just the basename from input path. */
  std::string base_path{tex_map.mtl_dir_path + BLI_path_basename(image_path.c_str())};
  if (base_path != tex_path) {
    image = load_image_at_path(bmain, base_path, relative_paths);
    if (image != nullptr) {
      return image;
    }
  }

  image = create_placeholder_image(bmain, tex_path);
  return image;
}

/* Nodes are arranged in columns by type, with manually placed x coordinates
 * based on node widths. */
const float node_locx_texcoord = -880.0f;
const float node_locx_mapping = -680.0f;
const float node_locx_image = -480.0f;
const float node_locx_normalmap = -200.0f;
const float node_locx_bsdf = 0.0f;
const float node_locx_output = 280.0f;

/* Nodes are arranged in rows; one row for each image being used. */
const float node_locy_top = 300.0f;
const float node_locy_step = 300.0f;

/* Add a node of the given type at the given location. */
static bNode *add_node(bNodeTree *ntree, int type, float x, float y)
{
  bNode *node = nodeAddStaticNode(nullptr, ntree, type);
  node->locx = x;
  node->locy = y;
  return node;
}

static void link_sockets(bNodeTree *ntree,
                         bNode *from_node,
                         const char *from_node_id,
                         bNode *to_node,
                         const char *to_node_id)
{
  bNodeSocket *from_sock{nodeFindSocket(from_node, SOCK_OUT, from_node_id)};
  bNodeSocket *to_sock{nodeFindSocket(to_node, SOCK_IN, to_node_id)};
  BLI_assert(from_sock && to_sock);
  nodeAddLink(ntree, from_node, from_sock, to_node, to_sock);
}

static void set_bsdf_socket_values(bNode *bsdf, Material *mat, const MTLMaterial &mtl_mat)
{
  const int illum = mtl_mat.illum_mode;
  bool do_highlight = false;
  bool do_tranparency = false;
  bool do_reflection = false;
  bool do_glass = false;
  /* See https://wikipedia.org/wiki/Wavefront_.obj_file for possible values of illum. */
  switch (illum) {
    case -1:
    case 1:
      /* Base color on, ambient on. */
      break;
    case 2: {
      /* Highlight on. */
      do_highlight = true;
      break;
    }
    case 3: {
      /* Reflection on and Ray trace on. */
      do_reflection = true;
      break;
    }
    case 4: {
      /* Transparency: Glass on, Reflection: Ray trace on. */
      do_glass = true;
      do_reflection = true;
      do_tranparency = true;
      break;
    }
    case 5: {
      /* Reflection: Fresnel on and Ray trace on. */
      do_reflection = true;
      break;
    }
    case 6: {
      /* Transparency: Refraction on, Reflection: Fresnel off and Ray trace on. */
      do_reflection = true;
      do_tranparency = true;
      break;
    }
    case 7: {
      /* Transparency: Refraction on, Reflection: Fresnel on and Ray trace on. */
      do_reflection = true;
      do_tranparency = true;
      break;
    }
    case 8: {
      /* Reflection on and Ray trace off. */
      do_reflection = true;
      break;
    }
    case 9: {
      /* Transparency: Glass on, Reflection: Ray trace off. */
      do_glass = true;
      do_reflection = false;
      do_tranparency = true;
      break;
    }
    default: {
      std::cerr << "Warning! illum value = " << illum
                << "is not supported by the Principled-BSDF shader." << std::endl;
      break;
    }
  }
  /* Approximations for trying to map obj/mtl material model into
   * Principled BSDF: */
  /* Specular: average of Ks components. */
  float specular = (mtl_mat.spec_color[0] + mtl_mat.spec_color[1] + mtl_mat.spec_color[2]) / 3;
  if (specular < 0.0f) {
    specular = do_highlight ? 1.0f : 0.0f;
  }
  /* Roughness: map 0..1000 range to 1..0 and apply non-linearity. */
  float roughness;
  if (mtl_mat.spec_exponent < 0.0f) {
    roughness = do_highlight ? 0.0f : 1.0f;
  }
  else {
    float clamped_ns = std::max(0.0f, std::min(1000.0f, mtl_mat.spec_exponent));
    roughness = 1.0f - sqrt(clamped_ns / 1000.0f);
  }
  /* Metallic: average of `Ka` components. */
  float metallic = (mtl_mat.ambient_color[0] + mtl_mat.ambient_color[1] +
                    mtl_mat.ambient_color[2]) /
                   3;
  if (do_reflection) {
    if (metallic < 0.0f) {
      metallic = 1.0f;
    }
  }
  else {
    metallic = 0.0f;
  }

  float ior = mtl_mat.ior;
  if (ior < 0) {
    if (do_tranparency) {
      ior = 1.0f;
    }
    if (do_glass) {
      ior = 1.5f;
    }
  }
  float alpha = mtl_mat.alpha;
  if (do_tranparency && alpha < 0) {
    alpha = 1.0f;
  }

  /* PBR values, when present, override the ones calculated above. */
  if (mtl_mat.roughness >= 0) {
    roughness = mtl_mat.roughness;
  }
  if (mtl_mat.metallic >= 0) {
    metallic = mtl_mat.metallic;
  }

  float3 base_color = mtl_mat.color;
  if (base_color.x >= 0 && base_color.y >= 0 && base_color.z >= 0) {
    set_property_of_socket(SOCK_RGBA, "Base Color", {base_color, 3}, bsdf);
    /* Viewport shading uses legacy r,g,b base color. */
    mat->r = base_color.x;
    mat->g = base_color.y;
    mat->b = base_color.z;
  }

  float3 emission_color = mtl_mat.emission_color;
  if (emission_color.x >= 0 && emission_color.y >= 0 && emission_color.z >= 0) {
    set_property_of_socket(SOCK_RGBA, "Emission Color", {emission_color, 3}, bsdf);
  }
  if (mtl_mat.tex_map_of_type(MTLTexMapType::Emission).is_valid()) {
    set_property_of_socket(SOCK_FLOAT, "Emission Strength", {1.0f}, bsdf);
  }
  set_property_of_socket(SOCK_FLOAT, "Specular IOR Level", {specular}, bsdf);
  set_property_of_socket(SOCK_FLOAT, "Roughness", {roughness}, bsdf);
  mat->roughness = roughness;
  set_property_of_socket(SOCK_FLOAT, "Metallic", {metallic}, bsdf);
  mat->metallic = metallic;
  if (ior != -1) {
    set_property_of_socket(SOCK_FLOAT, "IOR", {ior}, bsdf);
  }
  if (alpha != -1) {
    set_property_of_socket(SOCK_FLOAT, "Alpha", {alpha}, bsdf);
  }
  if (do_tranparency || (alpha >= 0.0f && alpha < 1.0f)) {
    mat->blend_method = MA_BM_BLEND;
    mat->blend_flag |= MA_BL_HIDE_BACKFACE;
  }

  if (mtl_mat.sheen >= 0) {
    set_property_of_socket(SOCK_FLOAT, "Sheen Weight", {mtl_mat.sheen}, bsdf);
  }
  if (mtl_mat.cc_thickness >= 0) {
    /* Clearcoat used to include an implicit 0.25 factor, so stay compatible to old versions. */
    set_property_of_socket(SOCK_FLOAT, "Coat Weight", {0.25f * mtl_mat.cc_thickness}, bsdf);
  }
  if (mtl_mat.cc_roughness >= 0) {
    set_property_of_socket(SOCK_FLOAT, "Coat Roughness", {mtl_mat.cc_roughness}, bsdf);
  }
  if (mtl_mat.aniso >= 0) {
    set_property_of_socket(SOCK_FLOAT, "Anisotropic", {mtl_mat.aniso}, bsdf);
  }
  if (mtl_mat.aniso_rot >= 0) {
    set_property_of_socket(SOCK_FLOAT, "Anisotropic Rotation", {mtl_mat.aniso_rot}, bsdf);
  }

  /* Transmission: average of transmission color. */
  float transmission = (mtl_mat.transmit_color[0] + mtl_mat.transmit_color[1] +
                        mtl_mat.transmit_color[2]) /
                       3;
  if (transmission >= 0) {
    set_property_of_socket(SOCK_FLOAT, "Transmission Weight", {transmission}, bsdf);
  }
}

static void add_image_textures(Main *bmain,
                               bNodeTree *ntree,
                               bNode *bsdf,
                               Material *mat,
                               const MTLMaterial &mtl_mat,
                               bool relative_paths)
{
  float node_locy = node_locy_top;
  for (int key = 0; key < int(MTLTexMapType::Count); ++key) {
    const MTLTexMap &value = mtl_mat.texture_maps[key];
    if (!value.is_valid()) {
      /* No Image texture node of this map type can be added to this material. */
      continue;
    }

    Image *image = load_texture_image(bmain, value, relative_paths);
    if (image == nullptr) {
      continue;
    }

    bNode *image_node = add_node(ntree, SH_NODE_TEX_IMAGE, node_locx_image, node_locy);
    BLI_assert(image_node);
    image_node->id = &image->id;
    static_cast<NodeTexImage *>(image_node->storage)->projection = value.projection_type;

    /* Add normal map node if needed. */
    bNode *normal_map = nullptr;
    if (key == int(MTLTexMapType::Normal)) {
      normal_map = add_node(ntree, SH_NODE_NORMAL_MAP, node_locx_normalmap, node_locy);
      const float bump = std::max(0.0f, mtl_mat.normal_strength);
      set_property_of_socket(SOCK_FLOAT, "Strength", {bump}, normal_map);
    }

    /* Add UV mapping & coordinate nodes only if needed. */
    if (value.translation != float3(0, 0, 0) || value.scale != float3(1, 1, 1)) {
      bNode *texcoord = add_node(ntree, SH_NODE_TEX_COORD, node_locx_texcoord, node_locy);
      bNode *mapping = add_node(ntree, SH_NODE_MAPPING, node_locx_mapping, node_locy);
      set_property_of_socket(SOCK_VECTOR, "Location", {value.translation, 3}, mapping);
      set_property_of_socket(SOCK_VECTOR, "Scale", {value.scale, 3}, mapping);

      link_sockets(ntree, texcoord, "UV", mapping, "Vector");
      link_sockets(ntree, mapping, "Vector", image_node, "Vector");
    }

    if (normal_map) {
      link_sockets(ntree, image_node, "Color", normal_map, "Color");
      link_sockets(ntree, normal_map, "Normal", bsdf, "Normal");
    }
    else if (key == int(MTLTexMapType::Alpha)) {
      link_sockets(ntree, image_node, "Alpha", bsdf, tex_map_type_to_socket_id[key]);
      mat->blend_method = MA_BM_BLEND;
      mat->blend_flag |= MA_BL_HIDE_BACKFACE;
    }
    else {
      link_sockets(ntree, image_node, "Color", bsdf, tex_map_type_to_socket_id[key]);
    }

    /* Next layout row: goes downwards on the screen. */
    node_locy -= node_locy_step;
  }
}

bNodeTree *create_mtl_node_tree(Main *bmain,
                                const MTLMaterial &mtl,
                                Material *mat,
                                bool relative_paths)
{
  bNodeTree *ntree = blender::bke::ntreeAddTreeEmbedded(
      nullptr, &mat->id, "Shader Nodetree", ntreeType_Shader->idname);

  bNode *bsdf = add_node(ntree, SH_NODE_BSDF_PRINCIPLED, node_locx_bsdf, node_locy_top);
  bNode *output = add_node(ntree, SH_NODE_OUTPUT_MATERIAL, node_locx_output, node_locy_top);

  set_bsdf_socket_values(bsdf, mat, mtl);
  add_image_textures(bmain, ntree, bsdf, mat, mtl, relative_paths);
  link_sockets(ntree, bsdf, "BSDF", output, "Surface");
  nodeSetActive(ntree, output);

  return ntree;
}

}  // namespace blender::io::obj
