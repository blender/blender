/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include <array>

#include "BLI_map.hh"
#include "BLI_math_vec_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "MEM_guardedalloc.h"

#include "obj_export_mtl.hh"

namespace blender::io::obj {

struct UniqueNodetreeDeleter {
  void operator()(bNodeTree *node)
  {
    MEM_freeN(node);
  }
};

using unique_nodetree_ptr = std::unique_ptr<bNodeTree, UniqueNodetreeDeleter>;

class ShaderNodetreeWrap {
 private:
  /* Node arrangement:
   * Texture Coordinates -> Mapping -> Image Texture -> (optional) Normal Map -> p-BSDF -> Material
   * Output. */
  unique_nodetree_ptr nodetree_;
  bNode *bsdf_;
  bNode *shader_output_;
  const MTLMaterial &mtl_mat_;

  /* List of all locations occupied by nodes. */
  Vector<std::array<int, 2>> node_locations;
  const float node_size_{300.f};

 public:
  ShaderNodetreeWrap(Main *bmain, const MTLMaterial &mtl_mat, Material *mat);
  ~ShaderNodetreeWrap();

  bNodeTree *get_nodetree();

 private:
  bNode *add_node_to_tree(const int node_type);
  std::pair<float, float> set_node_locations(const int pos_x);
  void link_sockets(bNode *from_node,
                    StringRef from_node_id,
                    bNode *to_node,
                    StringRef to_node_id,
                    const int from_node_pos_x);
  void set_bsdf_socket_values();
  void add_image_textures(Main *bmain, Material *mat);
};

constexpr eMTLSyntaxElement mtl_line_key_str_to_enum(const std::string_view key_str)
{
  if (key_str == "map_Kd") {
    return eMTLSyntaxElement::map_Kd;
  }
  if (key_str == "map_Ks") {
    return eMTLSyntaxElement::map_Ks;
  }
  if (key_str == "map_Ns") {
    return eMTLSyntaxElement::map_Ns;
  }
  if (key_str == "map_d") {
    return eMTLSyntaxElement::map_d;
  }
  if (key_str == "refl" || key_str == "map_refl") {
    return eMTLSyntaxElement::map_refl;
  }
  if (key_str == "map_Ke") {
    return eMTLSyntaxElement::map_Ke;
  }
  if (key_str == "map_Bump" || key_str == "bump") {
    return eMTLSyntaxElement::map_Bump;
  }
  return eMTLSyntaxElement::string;
}
}  // namespace blender::io::obj
