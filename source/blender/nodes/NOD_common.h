/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BKE_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal functions for editor. */

struct bNodeSocket *node_group_find_input_socket(struct bNode *groupnode, const char *identifier);
struct bNodeSocket *node_group_find_output_socket(struct bNode *groupnode, const char *identifier);

struct bNodeSocket *node_group_input_find_socket(struct bNode *node, const char *identifier);
struct bNodeSocket *node_group_output_find_socket(struct bNode *node, const char *identifier);

void node_internal_links_create(struct bNodeTree *ntree, struct bNode *node);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace blender::nodes {

void node_group_declare_dynamic(const bNodeTree &node_tree,
                                const bNode &node,
                                NodeDeclaration &r_declaration);

}  // namespace blender::nodes

#endif
