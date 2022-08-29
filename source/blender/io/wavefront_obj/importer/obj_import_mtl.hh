/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include <array>

#include "BLI_map.hh"
#include "BLI_math_vec_types.hh"
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
  /**
   * Initializes a nodetree with a p-BSDF node's BSDF socket connected to shader output node's
   * surface socket.
   */
  ShaderNodetreeWrap(Main *bmain, const MTLMaterial &mtl_mat, Material *mat, bool relative_paths);
  ~ShaderNodetreeWrap();

  /**
   * Release nodetree for materials to own it. nodetree has its unique deleter
   * if destructor is not reached for some reason.
   */
  bNodeTree *get_nodetree();

 private:
  /**
   * Add a new static node to the tree.
   * No two nodes are linked here.
   */
  bNode *add_node_to_tree(const int node_type);
  /**
   * Return x-y coordinates for a node where y is determined by other nodes present in
   * the same vertical column.
   */
  std::pair<float, float> set_node_locations(const int pos_x);
  /**
   * Link two nodes by the sockets of given IDs.
   * Also releases the ownership of the "from" node for nodetree to free it.
   * \param from_node_pos_x: 0 to 4 value as per nodetree arrangement.
   */
  void link_sockets(bNode *from_node,
                    const char *from_node_id,
                    bNode *to_node,
                    const char *to_node_id,
                    const int from_node_pos_x);
  /**
   * Set values of sockets in p-BSDF node of the nodetree.
   */
  void set_bsdf_socket_values(Material *mat);
  /**
   * Create image texture, vector and normal mapping nodes from MTL materials and link the
   * nodes to p-BSDF node.
   */
  void add_image_textures(Main *bmain, Material *mat, bool relative_paths);
};

}  // namespace blender::io::obj
