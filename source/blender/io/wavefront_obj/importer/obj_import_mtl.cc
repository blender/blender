/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "BLI_map.hh"
#include "BLI_math_vector.h"
#include "BLI_path_util.h"

#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "NOD_shader.h"

#include "obj_export_mtl.hh"
#include "obj_import_mtl.hh"
#include "obj_import_string_utils.hh"

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

  /* First try treating texture path as relative. */
  std::string tex_path{tex_map.mtl_dir_path + tex_map.image_path};
  image = load_image_at_path(bmain, tex_path, relative_paths);
  if (image != nullptr) {
    return image;
  }
  /* Then try using it directly as absolute path. */
  std::string raw_path{tex_map.image_path};
  image = load_image_at_path(bmain, raw_path, relative_paths);
  if (image != nullptr) {
    return image;
  }
  /* Try removing quotes. */
  std::string no_quote_path{tex_path};
  auto end_pos = std::remove(no_quote_path.begin(), no_quote_path.end(), '"');
  no_quote_path.erase(end_pos, no_quote_path.end());
  if (no_quote_path != tex_path) {
    image = load_image_at_path(bmain, no_quote_path, relative_paths);
    if (image != nullptr) {
      return image;
    }
  }
  /* Try replacing underscores with spaces. */
  std::string no_underscore_path{no_quote_path};
  std::replace(no_underscore_path.begin(), no_underscore_path.end(), '_', ' ');
  if (no_underscore_path != no_quote_path && no_underscore_path != tex_path) {
    image = load_image_at_path(bmain, no_underscore_path, relative_paths);
    if (image != nullptr) {
      return image;
    }
  }
  /* Try taking just the basename from input path. */
  std::string base_path{tex_map.mtl_dir_path + BLI_path_basename(tex_map.image_path.c_str())};
  if (base_path != tex_path) {
    image = load_image_at_path(bmain, base_path, relative_paths);
    if (image != nullptr) {
      return image;
    }
  }

  image = create_placeholder_image(bmain, tex_path);
  return image;
}

typedef Vector<std::pair<int, int>> NodeLocations;

static std::pair<float, float> calc_location(int column, NodeLocations &r_locations)
{
  const float node_size = 300.f;
  int row = 0;
  bool found = false;
  while (true) {
    for (const auto &location : r_locations) {
      if (location.first == column && location.second == row) {
        row += 1;
        found = true;
      }
      else {
        found = false;
      }
    }
    if (!found) {
      r_locations.append({column, row});
      return {column * node_size, row * node_size * 2.0 / 3.0};
    }
  }
}

/* Node layout columns:
 * Texture Coordinates -> Mapping -> Image -> Normal Map -> BSDF -> Output */
static void link_sockets(bNodeTree *nodetree,
                         bNode *from_node,
                         const char *from_node_id,
                         bNode *to_node,
                         const char *to_node_id,
                         const int from_node_column,
                         NodeLocations &r_locations)
{
  std::tie(from_node->locx, from_node->locy) = calc_location(from_node_column, r_locations);
  std::tie(to_node->locx, to_node->locy) = calc_location(from_node_column + 1, r_locations);
  bNodeSocket *from_sock{nodeFindSocket(from_node, SOCK_OUT, from_node_id)};
  bNodeSocket *to_sock{nodeFindSocket(to_node, SOCK_IN, to_node_id)};
  BLI_assert(from_sock && to_sock);
  nodeAddLink(nodetree, from_node, from_sock, to_node, to_sock);
}

static void set_bsdf_socket_values(bNode *bsdf, Material *mat, const MTLMaterial &mtl_mat)
{
  const int illum = mtl_mat.illum;
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
  float specular = (mtl_mat.Ks[0] + mtl_mat.Ks[1] + mtl_mat.Ks[2]) / 3;
  if (specular < 0.0f) {
    specular = do_highlight ? 1.0f : 0.0f;
  }
  /* Roughness: map 0..1000 range to 1..0 and apply non-linearity. */
  float roughness;
  if (mtl_mat.Ns < 0.0f) {
    roughness = do_highlight ? 0.0f : 1.0f;
  }
  else {
    float clamped_ns = std::max(0.0f, std::min(1000.0f, mtl_mat.Ns));
    roughness = 1.0f - sqrt(clamped_ns / 1000.0f);
  }
  /* Metallic: average of Ka components. */
  float metallic = (mtl_mat.Ka[0] + mtl_mat.Ka[1] + mtl_mat.Ka[2]) / 3;
  if (do_reflection) {
    if (metallic < 0.0f) {
      metallic = 1.0f;
    }
  }
  else {
    metallic = 0.0f;
  }

  float ior = mtl_mat.Ni;
  if (ior < 0) {
    if (do_tranparency) {
      ior = 1.0f;
    }
    if (do_glass) {
      ior = 1.5f;
    }
  }
  float alpha = mtl_mat.d;
  if (do_tranparency && alpha < 0) {
    alpha = 1.0f;
  }

  float3 base_color = {mtl_mat.Kd[0], mtl_mat.Kd[1], mtl_mat.Kd[2]};
  if (base_color.x >= 0 && base_color.y >= 0 && base_color.z >= 0) {
    set_property_of_socket(SOCK_RGBA, "Base Color", {base_color, 3}, bsdf);
    /* Viewport shading uses legacy r,g,b base color. */
    mat->r = base_color.x;
    mat->g = base_color.y;
    mat->b = base_color.z;
  }

  float3 emission_color = {mtl_mat.Ke[0], mtl_mat.Ke[1], mtl_mat.Ke[2]};
  if (emission_color.x >= 0 && emission_color.y >= 0 && emission_color.z >= 0) {
    set_property_of_socket(SOCK_RGBA, "Emission", {emission_color, 3}, bsdf);
  }
  if (mtl_mat.tex_map_of_type(MTLTexMapType::Ke).is_valid()) {
    set_property_of_socket(SOCK_FLOAT, "Emission Strength", {1.0f}, bsdf);
  }
  set_property_of_socket(SOCK_FLOAT, "Specular", {specular}, bsdf);
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
  }
}

static void add_image_textures(Main *bmain,
                               bNodeTree *nodetree,
                               bNode *bsdf,
                               Material *mat,
                               const MTLMaterial &mtl_mat,
                               bool relative_paths,
                               NodeLocations &r_locations)
{
  for (int key = 0; key < (int)MTLTexMapType::Count; ++key) {
    const MTLTexMap &value = mtl_mat.texture_maps[key];
    if (!value.is_valid()) {
      /* No Image texture node of this map type can be added to this material. */
      continue;
    }

    bNode *image_texture = nodeAddStaticNode(nullptr, nodetree, SH_NODE_TEX_IMAGE);
    BLI_assert(image_texture);
    Image *image = load_texture_image(bmain, value, relative_paths);
    if (image == nullptr) {
      continue;
    }
    image_texture->id = &image->id;
    static_cast<NodeTexImage *>(image_texture->storage)->projection = value.projection_type;

    /* Add normal map node if needed. */
    bNode *normal_map = nullptr;
    if (key == (int)MTLTexMapType::bump) {
      normal_map = nodeAddStaticNode(nullptr, nodetree, SH_NODE_NORMAL_MAP);
      const float bump = std::max(0.0f, mtl_mat.map_Bump_strength);
      set_property_of_socket(SOCK_FLOAT, "Strength", {bump}, normal_map);
    }

    /* Add UV mapping & coordinate nodes only if needed. */
    if (value.translation != float3(0, 0, 0) || value.scale != float3(1, 1, 1)) {
      bNode *mapping = nodeAddStaticNode(nullptr, nodetree, SH_NODE_MAPPING);
      bNode *texture_coordinate = nodeAddStaticNode(nullptr, nodetree, SH_NODE_TEX_COORD);
      set_property_of_socket(SOCK_VECTOR, "Location", {value.translation, 3}, mapping);
      set_property_of_socket(SOCK_VECTOR, "Scale", {value.scale, 3}, mapping);

      link_sockets(nodetree, texture_coordinate, "UV", mapping, "Vector", 0, r_locations);
      link_sockets(nodetree, mapping, "Vector", image_texture, "Vector", 1, r_locations);
    }

    if (normal_map) {
      link_sockets(nodetree, image_texture, "Color", normal_map, "Color", 2, r_locations);
      link_sockets(nodetree, normal_map, "Normal", bsdf, "Normal", 3, r_locations);
    }
    else if (key == (int)MTLTexMapType::d) {
      link_sockets(
          nodetree, image_texture, "Alpha", bsdf, tex_map_type_to_socket_id[key], 2, r_locations);
      mat->blend_method = MA_BM_BLEND;
    }
    else {
      link_sockets(
          nodetree, image_texture, "Color", bsdf, tex_map_type_to_socket_id[key], 2, r_locations);
    }
  }
}

bNodeTree *create_mtl_node_tree(Main *bmain,
                                const MTLMaterial &mtl,
                                Material *mat,
                                bool relative_paths)
{
  bNodeTree *nodetree = ntreeAddTree(nullptr, "Shader Nodetree", ntreeType_Shader->idname);
  bNode *bsdf = nodeAddStaticNode(nullptr, nodetree, SH_NODE_BSDF_PRINCIPLED);
  bNode *shader_output = nodeAddStaticNode(nullptr, nodetree, SH_NODE_OUTPUT_MATERIAL);

  NodeLocations node_locations;
  set_bsdf_socket_values(bsdf, mat, mtl);
  add_image_textures(bmain, nodetree, bsdf, mat, mtl, relative_paths, node_locations);
  link_sockets(nodetree, bsdf, "BSDF", shader_output, "Surface", 4, node_locations);
  nodeSetActive(nodetree, shader_output);

  return nodetree;
}

}  // namespace blender::io::obj
