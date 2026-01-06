/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BLI_string_ref.hh"

namespace blender {

struct bNode;
struct bNodeSocket;
namespace nodes {
class NodeDeclarationBuilder;
}  // namespace nodes

bNodeSocket *node_group_find_input_socket(bNode *groupnode, StringRef identifier);
bNodeSocket *node_group_find_output_socket(bNode *groupnode, StringRef identifier);

bNodeSocket *node_group_input_find_socket(bNode *node, StringRef identifier);
bNodeSocket *node_group_output_find_socket(bNode *node, StringRef identifier);

int node_group_ui_class(const bNode *node);

namespace nodes {

void node_group_declare(NodeDeclarationBuilder &b);

}  // namespace nodes
}  // namespace blender
