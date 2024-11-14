/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_node.hh"
#include "BKE_node_tree_update.hh"
#include "DNA_node_types.h"
#include "NOD_shader.h"

bNodeTree *BKE_npr_tree_add(Main *bmain, const char *name)
{
  bNodeTree *ntree = blender::bke::node_tree_add_tree(bmain, name, ntreeType_Shader->idname);
  ntree->shader_node_traits->type = SH_TREE_TYPE_NPR;

  bNode *input = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_NPR_INPUT);
  bNode *output = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_NPR_OUTPUT);

  /* Weird offsets, but these are the same as the default material node trees. */
  input->locx = 10.0f;
  input->locy = 300.0f;
  output->locx = 300.0f;
  output->locy = 300.0f;

  blender::bke::node_add_link(ntree,
                              input,
                              blender::bke::node_find_socket(input, SOCK_OUT, "Combined Color"),
                              output,
                              blender::bke::node_find_socket(output, SOCK_IN, "Color"));

  BKE_ntree_update_main_tree(bmain, ntree, nullptr);

  return ntree;
}
