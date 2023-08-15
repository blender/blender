/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

struct bNode;
struct bNodeInstanceHash;
struct bNodeTree;

/* data for initializing node execution */
struct bNodeExecContext {
  bNodeInstanceHash *previews;
};

struct bNodeExecData {
  void *data;            /* custom data storage */
  bNodePreview *preview; /* optional preview image */
};

/**** Storage Data ****/

void node_free_curves(bNode *node);
void node_free_standard_storage(bNode *node);

void node_copy_curves(bNodeTree *dest_ntree, bNode *dest_node, const bNode *src_node);
void node_copy_standard_storage(bNodeTree *dest_ntree, bNode *dest_node, const bNode *src_node);
void *node_initexec_curves(bNodeExecContext *context, bNode *node, bNodeInstanceKey key);

/**** Updates ****/
void node_sock_label(bNodeSocket *sock, const char *name);
void node_sock_label_clear(bNodeSocket *sock);
void node_math_update(bNodeTree *ntree, bNode *node);

/**** Labels ****/
void node_blend_label(const bNodeTree *ntree, const bNode *node, char *label, int label_maxncpy);
void node_image_label(const bNodeTree *ntree, const bNode *node, char *label, int label_maxncpy);
void node_math_label(const bNodeTree *ntree, const bNode *node, char *label, int label_maxncpy);
void node_vector_math_label(const bNodeTree *ntree,
                            const bNode *node,
                            char *label,
                            int label_maxncpy);
void node_filter_label(const bNodeTree *ntree, const bNode *node, char *label, int label_maxncpy);
void node_combsep_color_label(const ListBase *sockets, NodeCombSepColorMode mode);

/*** Link Handling */

/**
 * By default there are no links we don't want to connect, when inserting.
 */
bool node_insert_link_default(bNodeTree *ntree, bNode *node, bNodeLink *link);

float node_socket_get_float(bNodeTree *ntree, bNode *node, bNodeSocket *sock);
void node_socket_set_float(bNodeTree *ntree, bNode *node, bNodeSocket *sock, float value);
void node_socket_get_color(bNodeTree *ntree, bNode *node, bNodeSocket *sock, float *value);
void node_socket_set_color(bNodeTree *ntree, bNode *node, bNodeSocket *sock, const float *value);
void node_socket_get_vector(bNodeTree *ntree, bNode *node, bNodeSocket *sock, float *value);
void node_socket_set_vector(bNodeTree *ntree, bNode *node, bNodeSocket *sock, const float *value);
