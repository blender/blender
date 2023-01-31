/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup nodes
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bNode;
struct bNodeTree;

/* data for initializing node execution */
typedef struct bNodeExecContext {
  struct bNodeInstanceHash *previews;
} bNodeExecContext;

typedef struct bNodeExecData {
  void *data;                   /* custom data storage */
  struct bNodePreview *preview; /* optional preview image */
} bNodeExecData;

/**** Storage Data ****/

void node_free_curves(struct bNode *node);
void node_free_standard_storage(struct bNode *node);

void node_copy_curves(struct bNodeTree *dest_ntree,
                      struct bNode *dest_node,
                      const struct bNode *src_node);
void node_copy_standard_storage(struct bNodeTree *dest_ntree,
                                struct bNode *dest_node,
                                const struct bNode *src_node);
void *node_initexec_curves(struct bNodeExecContext *context,
                           struct bNode *node,
                           bNodeInstanceKey key);

/**** Updates ****/
void node_sock_label(struct bNodeSocket *sock, const char *name);
void node_sock_label_clear(struct bNodeSocket *sock);
void node_math_update(struct bNodeTree *ntree, struct bNode *node);

/**** Labels ****/
void node_blend_label(const struct bNodeTree *ntree,
                      const struct bNode *node,
                      char *label,
                      int maxlen);
void node_image_label(const struct bNodeTree *ntree,
                      const struct bNode *node,
                      char *label,
                      int maxlen);
void node_math_label(const struct bNodeTree *ntree,
                     const struct bNode *node,
                     char *label,
                     int maxlen);
void node_vector_math_label(const struct bNodeTree *ntree,
                            const struct bNode *node,
                            char *label,
                            int maxlen);
void node_filter_label(const struct bNodeTree *ntree,
                       const struct bNode *node,
                       char *label,
                       int maxlen);
void node_combsep_color_label(const ListBase *sockets, NodeCombSepColorMode mode);

/*** Link Handling */

/**
 * By default there are no links we don't want to connect, when inserting.
 */
bool node_insert_link_default(struct bNodeTree *ntree, struct bNode *node, struct bNodeLink *link);

float node_socket_get_float(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock);
void node_socket_set_float(struct bNodeTree *ntree,
                           struct bNode *node,
                           struct bNodeSocket *sock,
                           float value);
void node_socket_get_color(struct bNodeTree *ntree,
                           struct bNode *node,
                           struct bNodeSocket *sock,
                           float *value);
void node_socket_set_color(struct bNodeTree *ntree,
                           struct bNode *node,
                           struct bNodeSocket *sock,
                           const float *value);
void node_socket_get_vector(struct bNodeTree *ntree,
                            struct bNode *node,
                            struct bNodeSocket *sock,
                            float *value);
void node_socket_set_vector(struct bNodeTree *ntree,
                            struct bNode *node,
                            struct bNodeSocket *sock,
                            const float *value);

#ifdef __cplusplus
}
#endif
