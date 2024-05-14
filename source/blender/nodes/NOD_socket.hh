/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BKE_node.hh"

struct bNode;
struct bNodeTree;

bNodeSocket *node_add_socket_from_template(bNodeTree *ntree,
                                           bNode *node,
                                           blender::bke::bNodeSocketTemplate *stemp,
                                           eNodeSocketInOut in_out);

void node_verify_sockets(bNodeTree *ntree, bNode *node, bool do_id_user);

void node_socket_init_default_value_data(eNodeSocketDatatype datatype, int subtype, void **data);
void node_socket_copy_default_value_data(eNodeSocketDatatype datatype, void *to, const void *from);
void node_socket_init_default_value(bNodeSocket *sock);
void node_socket_copy_default_value(bNodeSocket *to, const bNodeSocket *from);
void register_standard_node_socket_types();

namespace blender::nodes {

void update_node_declaration_and_sockets(bNodeTree &ntree, bNode &node);
bool socket_type_supports_fields(eNodeSocketDatatype socket_type);
bool socket_type_supports_grids(eNodeSocketDatatype socket_type);

}  // namespace blender::nodes
