/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BLI_string_ref.hh"

struct bNode;
struct bNodeSocket;
namespace blender::nodes {
class NodeDeclarationBuilder;
}  // namespace blender::nodes

bNodeSocket *node_group_find_input_socket(bNode *groupnode, blender::StringRef identifier);
bNodeSocket *node_group_find_output_socket(bNode *groupnode, blender::StringRef identifier);

bNodeSocket *node_group_input_find_socket(bNode *node, blender::StringRef identifier);
bNodeSocket *node_group_output_find_socket(bNode *node, blender::StringRef identifier);

namespace blender::nodes {

void node_group_declare(NodeDeclarationBuilder &b);

}  // namespace blender::nodes
