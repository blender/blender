/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "DNA_node_types.h"

#include "BKE_node.hh"

struct bNode;
struct bNodeTree;
struct bContext;

/* data for initializing node execution */
struct bNodeExecContext {};

struct bNodeExecData {
  void *data; /* custom data storage */
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
void node_combsep_color_label(const ListBase *sockets, NodeCombSepColorMode mode);

/*** Link Handling */

/**
 * By default there are no links we don't want to connect, when inserting.
 */
bool node_insert_link_default(blender::bke::NodeInsertLinkParams &params);

int node_socket_get_int(bNodeTree *ntree, bNode *node, bNodeSocket *sock);
void node_socket_set_int(bNodeTree *ntree, bNode *node, bNodeSocket *sock, int value);
bool node_socket_get_bool(bNodeTree *ntree, bNode *node, bNodeSocket *sock);
void node_socket_set_bool(bNodeTree *ntree, bNode *node, bNodeSocket *sock, bool value);
float node_socket_get_float(bNodeTree *ntree, bNode *node, bNodeSocket *sock);
void node_socket_set_float(bNodeTree *ntree, bNode *node, bNodeSocket *sock, float value);
void node_socket_get_color(bNodeTree *ntree, bNode *node, bNodeSocket *sock, float *value);
void node_socket_set_color(bNodeTree *ntree, bNode *node, bNodeSocket *sock, const float *value);
void node_socket_get_vector(bNodeTree *ntree, bNode *node, bNodeSocket *sock, float *value);
void node_socket_set_vector(bNodeTree *ntree, bNode *node, bNodeSocket *sock, const float *value);
