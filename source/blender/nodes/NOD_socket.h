/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "DNA_listBase.h"

#include "BLI_utildefines.h"

#include "BKE_node.h"

#include "RNA_types.h"

struct bNode;
struct bNodeTree;

#ifdef __cplusplus
extern "C" {
#endif

struct bNodeSocket *node_add_socket_from_template(struct bNodeTree *ntree,
                                                  struct bNode *node,
                                                  struct bNodeSocketTemplate *stemp,
                                                  eNodeSocketInOut in_out);

void node_verify_sockets(struct bNodeTree *ntree, struct bNode *node, bool do_id_user);

void node_socket_init_default_value(struct bNodeSocket *sock);
void node_socket_copy_default_value(struct bNodeSocket *to, const struct bNodeSocket *from);
void register_standard_node_socket_types(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace blender::nodes {

void update_node_declaration_and_sockets(bNodeTree &ntree, bNode &node);

}  // namespace blender::nodes

#endif
