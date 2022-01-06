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
 */

/** \file
 * \ingroup obj
 */

#include "BKE_image.h"
#include "BKE_node.h"

#include "BLI_float3.hh"
#include "BLI_map.hh"
#include "BLI_path_util.h"

#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "NOD_node_tree_ref.hh"

#include "obj_export_mesh.hh"
#include "obj_export_mtl.hh"

namespace blender::io::obj {

/**
 * Copy a float property of the given type from the bNode to given buffer.
 */
static void copy_property_from_node(const eNodeSocketDatatype property_type,
                                    const bNode *node,
                                    const char *identifier,
                                    MutableSpan<float> r_property)
{
  if (!node) {
    return;
  }
  bNodeSocket *socket{nodeFindSocket(node, SOCK_IN, identifier)};
  BLI_assert(socket && socket->type == property_type);
  if (!socket) {
    return;
  }
  switch (property_type) {
    case SOCK_FLOAT: {
      BLI_assert(r_property.size() == 1);
      bNodeSocketValueFloat *socket_def_value = static_cast<bNodeSocketValueFloat *>(
          socket->default_value);
      r_property[0] = socket_def_value->value;
      break;
    }
    case SOCK_RGBA: {
      BLI_assert(r_property.size() == 3);
      bNodeSocketValueRGBA *socket_def_value = static_cast<bNodeSocketValueRGBA *>(
          socket->default_value);
      copy_v3_v3(r_property.data(), socket_def_value->value);
      break;
    }
    case SOCK_VECTOR: {
      BLI_assert(r_property.size() == 3);
      bNodeSocketValueVector *socket_def_value = static_cast<bNodeSocketValueVector *>(
          socket->default_value);
      copy_v3_v3(r_property.data(), socket_def_value->value);
      break;
    }
    default: {
      /* Other socket types are not handled here. */
      BLI_assert(0);
      break;
    }
  }
}

/**
 * Collect all the source sockets linked to the destination socket in a destination node.
 */
static void linked_sockets_to_dest_id(const bNode *dest_node,
                                      const nodes::NodeTreeRef &node_tree,
                                      StringRefNull dest_socket_id,
                                      Vector<const nodes::OutputSocketRef *> &r_linked_sockets)
{
  r_linked_sockets.clear();
  if (!dest_node) {
    return;
  }
  Span<const nodes::NodeRef *> object_dest_nodes = node_tree.nodes_by_type(dest_node->idname);
  Span<const nodes::InputSocketRef *> dest_inputs = object_dest_nodes.first()->inputs();
  const nodes::InputSocketRef *dest_socket = nullptr;
  for (const nodes::InputSocketRef *curr_socket : dest_inputs) {
    if (STREQ(curr_socket->bsocket()->identifier, dest_socket_id.c_str())) {
      dest_socket = curr_socket;
      break;
    }
  }
  if (dest_socket) {
    Span<const nodes::OutputSocketRef *> linked_sockets = dest_socket->directly_linked_sockets();
    r_linked_sockets.resize(linked_sockets.size());
    r_linked_sockets = linked_sockets;
  }
}

/**
 * From a list of sockets, get the parent node which is of the given node type.
 */
static const bNode *get_node_of_type(Span<const nodes::OutputSocketRef *> sockets_list,
                                     const int node_type)
{
  for (const nodes::SocketRef *socket : sockets_list) {
    const bNode *parent_node = socket->bnode();
    if (parent_node->typeinfo->type == node_type) {
      return parent_node;
    }
  }
  return nullptr;
}

/**
 * From a texture image shader node, get the image's filepath.
 * Returned filepath is stripped of initial "//". If packed image is found,
 * only the file "name" is returned.
 */
static const char *get_image_filepath(const bNode *tex_node)
{
  if (!tex_node) {
    return nullptr;
  }
  Image *tex_image = reinterpret_cast<Image *>(tex_node->id);
  if (!tex_image || !BKE_image_has_filepath(tex_image)) {
    return nullptr;
  }
  const char *path = tex_image->filepath;
  if (BKE_image_has_packedfile(tex_image)) {
    /* Put image in the same directory as the .MTL file. */
    path = BLI_path_slash_rfind(path) + 1;
    fprintf(stderr,
            "Packed image found:'%s'. Unpack and place the image in the same "
            "directory as the .MTL file.\n",
            path);
  }
  if (path[0] == '/' && path[1] == '/') {
    path += 2;
  }
  return path;
}

/**
 * Find the Principled-BSDF Node in nodetree.
 * We only want one that feeds directly into a Material Output node
 * (that is the behavior of the legacy Python exporter).
 */
static const nodes::NodeRef *find_bsdf_node(const nodes::NodeTreeRef *nodetree)
{
  if (!nodetree) {
    return nullptr;
  }
  for (const nodes::NodeRef *node : nodetree->nodes_by_type("ShaderNodeOutputMaterial")) {
    const nodes::InputSocketRef *node_input_socket0 = node->inputs()[0];
    for (const nodes::OutputSocketRef *out_sock : node_input_socket0->directly_linked_sockets()) {
      const nodes::NodeRef &in_node = out_sock->node();
      if (in_node.typeinfo()->type == SH_NODE_BSDF_PRINCIPLED) {
        return &in_node;
      }
    }
  }
  return nullptr;
}

/**
 * Store properties found either in bNode or material into r_mtl_mat.
 */
static void store_bsdf_properties(const nodes::NodeRef *bsdf_node,
                                  const Material *material,
                                  MTLMaterial &r_mtl_mat)
{
  const bNode *bnode = nullptr;
  if (bsdf_node) {
    bnode = bsdf_node->bnode();
  }

  /* If p-BSDF is not present, fallback to #Object.Material. */
  float roughness = material->roughness;
  if (bnode) {
    copy_property_from_node(SOCK_FLOAT, bnode, "Roughness", {&roughness, 1});
  }
  /* Empirical approximation. Importer should use the inverse of this method. */
  float spec_exponent = (1.0f - roughness) * 30;
  spec_exponent *= spec_exponent;

  float specular = material->spec;
  if (bnode) {
    copy_property_from_node(SOCK_FLOAT, bnode, "Specular", {&specular, 1});
  }

  float metallic = material->metallic;
  if (bnode) {
    copy_property_from_node(SOCK_FLOAT, bnode, "Metallic", {&metallic, 1});
  }

  float refraction_index = 1.0f;
  if (bnode) {
    copy_property_from_node(SOCK_FLOAT, bnode, "IOR", {&refraction_index, 1});
  }

  float dissolved = material->a;
  if (bnode) {
    copy_property_from_node(SOCK_FLOAT, bnode, "Alpha", {&dissolved, 1});
  }
  const bool transparent = dissolved != 1.0f;

  float3 diffuse_col = {material->r, material->g, material->b};
  if (bnode) {
    copy_property_from_node(SOCK_RGBA, bnode, "Base Color", {diffuse_col, 3});
  }

  float3 emission_col{0.0f};
  float emission_strength = 0.0f;
  if (bnode) {
    copy_property_from_node(SOCK_FLOAT, bnode, "Emission Strength", {&emission_strength, 1});
    copy_property_from_node(SOCK_RGBA, bnode, "Emission", {emission_col, 3});
  }
  mul_v3_fl(emission_col, emission_strength);

  /* See https://wikipedia.org/wiki/Wavefront_.obj_file for all possible values of `illum`. */
  /* Highlight on. */
  int illum = 2;
  if (specular == 0.0f) {
    /* Color on and Ambient on. */
    illum = 1;
  }
  else if (metallic > 0.0f) {
    /* Metallic ~= Reflection. */
    if (transparent) {
      /* Transparency: Refraction on, Reflection: ~~Fresnel off and Ray trace~~ on. */
      illum = 6;
    }
    else {
      /* Reflection on and Ray trace on. */
      illum = 3;
    }
  }
  else if (transparent) {
    /* Transparency: Glass on, Reflection: Ray trace off */
    illum = 9;
  }
  r_mtl_mat.Ns = spec_exponent;
  if (metallic != 0.0f) {
    r_mtl_mat.Ka = {metallic, metallic, metallic};
  }
  else {
    r_mtl_mat.Ka = {1.0f, 1.0f, 1.0f};
  }
  r_mtl_mat.Kd = diffuse_col;
  r_mtl_mat.Ks = {specular, specular, specular};
  r_mtl_mat.Ke = emission_col;
  r_mtl_mat.Ni = refraction_index;
  r_mtl_mat.d = dissolved;
  r_mtl_mat.illum = illum;
}

/**
 * Store image texture options and file-paths in `r_mtl_mat`.
 */
static void store_image_textures(const nodes::NodeRef *bsdf_node,
                                 const nodes::NodeTreeRef *node_tree,
                                 const Material *material,
                                 MTLMaterial &r_mtl_mat)
{
  if (!material || !node_tree || !bsdf_node) {
    /* No nodetree, no images, or no Principled BSDF node. */
    return;
  }
  const bNode *bnode = bsdf_node->bnode();

  /* Normal Map Texture has two extra tasks of:
   * - finding a Normal Map node before finding a texture node.
   * - finding "Strength" property of the node for `-bm` option.
   */

  for (Map<const eMTLSyntaxElement, tex_map_XX>::MutableItem texture_map :
       r_mtl_mat.texture_maps.items()) {
    Vector<const nodes::OutputSocketRef *> linked_sockets;
    const bNode *normal_map_node{nullptr};

    if (texture_map.key == eMTLSyntaxElement::map_Bump) {
      /* Find sockets linked to destination "Normal" socket in P-BSDF node. */
      linked_sockets_to_dest_id(bnode, *node_tree, "Normal", linked_sockets);
      /* Among the linked sockets, find Normal Map shader node. */
      normal_map_node = get_node_of_type(linked_sockets, SH_NODE_NORMAL_MAP);

      /* Find sockets linked to "Color" socket in normal map node. */
      linked_sockets_to_dest_id(normal_map_node, *node_tree, "Color", linked_sockets);
    }
    else if (texture_map.key == eMTLSyntaxElement::map_Ke) {
      float emission_strength = 0.0f;
      copy_property_from_node(SOCK_FLOAT, bnode, "Emission Strength", {&emission_strength, 1});
      if (emission_strength == 0.0f) {
        continue;
      }
    }
    else {
      /* Find sockets linked to the destination socket of interest, in P-BSDF node. */
      linked_sockets_to_dest_id(
          bnode, *node_tree, texture_map.value.dest_socket_id, linked_sockets);
    }

    /* Among the linked sockets, find Image Texture shader node. */
    const bNode *tex_node{get_node_of_type(linked_sockets, SH_NODE_TEX_IMAGE)};
    if (!tex_node) {
      continue;
    }
    const char *tex_image_filepath = get_image_filepath(tex_node);
    if (!tex_image_filepath) {
      continue;
    }

    /* Find "Mapping" node if connected to texture node. */
    linked_sockets_to_dest_id(tex_node, *node_tree, "Vector", linked_sockets);
    const bNode *mapping = get_node_of_type(linked_sockets, SH_NODE_MAPPING);

    if (normal_map_node) {
      copy_property_from_node(
          SOCK_FLOAT, normal_map_node, "Strength", {&r_mtl_mat.map_Bump_strength, 1});
    }
    /* Texture transform options. Only translation (origin offset, "-o") and scale
     * ("-o") are supported. */
    copy_property_from_node(SOCK_VECTOR, mapping, "Location", {texture_map.value.translation, 3});
    copy_property_from_node(SOCK_VECTOR, mapping, "Scale", {texture_map.value.scale, 3});

    texture_map.value.image_path = tex_image_filepath;
  }
}

MTLMaterial mtlmaterial_for_material(const Material *material)
{
  BLI_assert(material != nullptr);
  MTLMaterial mtlmat;
  mtlmat.name = std::string(material->id.name + 2);
  std::replace(mtlmat.name.begin(), mtlmat.name.end(), ' ', '_');
  const nodes::NodeTreeRef *nodetree = nullptr;
  if (material->nodetree) {
    nodetree = new nodes::NodeTreeRef(material->nodetree);
  }
  const nodes::NodeRef *bsdf_node = find_bsdf_node(nodetree);
  store_bsdf_properties(bsdf_node, material, mtlmat);
  store_image_textures(bsdf_node, nodetree, material, mtlmat);
  if (nodetree) {
    delete nodetree;
  }
  return mtlmat;
}

}  // namespace blender::io::obj
