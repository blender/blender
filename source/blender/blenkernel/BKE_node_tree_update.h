/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

struct ID;
struct ImageUser;
struct Main;
struct bNode;
struct bNodeLink;
struct bNodeSocket;
struct bNodeTree;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tag tree as changed without providing any more information about what has changed exactly.
 * The update process has to assume that everything may have changed.
 *
 * Using one of the methods below to tag the tree after changes is preferred when possible.
 */
void BKE_ntree_update_tag_all(struct bNodeTree *ntree);

/**
 * More specialized tag functions that may result in a more efficient update.
 */

void BKE_ntree_update_tag_node_property(struct bNodeTree *ntree, struct bNode *node);
void BKE_ntree_update_tag_node_new(struct bNodeTree *ntree, struct bNode *node);
void BKE_ntree_update_tag_node_removed(struct bNodeTree *ntree);
void BKE_ntree_update_tag_node_reordered(struct bNodeTree *ntree);
void BKE_ntree_update_tag_node_mute(struct bNodeTree *ntree, struct bNode *node);
void BKE_ntree_update_tag_node_internal_link(struct bNodeTree *ntree, struct bNode *node);

void BKE_ntree_update_tag_socket_property(struct bNodeTree *ntree, struct bNodeSocket *socket);
void BKE_ntree_update_tag_socket_new(struct bNodeTree *ntree, struct bNodeSocket *socket);
void BKE_ntree_update_tag_socket_type(struct bNodeTree *ntree, struct bNodeSocket *socket);
void BKE_ntree_update_tag_socket_availability(struct bNodeTree *ntree, struct bNodeSocket *socket);
void BKE_ntree_update_tag_socket_removed(struct bNodeTree *ntree);

void BKE_ntree_update_tag_link_changed(struct bNodeTree *ntree);
void BKE_ntree_update_tag_link_removed(struct bNodeTree *ntree);
void BKE_ntree_update_tag_link_added(struct bNodeTree *ntree, struct bNodeLink *link);
void BKE_ntree_update_tag_link_mute(struct bNodeTree *ntree, struct bNodeLink *link);

/** Used when the a new output node becomes active and therefore changes the output. */
void BKE_ntree_update_tag_active_output_changed(struct bNodeTree *ntree);
/** Used after file loading when run-time data on the tree has not been initialized yet. */
void BKE_ntree_update_tag_missing_runtime_data(struct bNodeTree *ntree);
/** Used when the interface sockets/values have changed. */
void BKE_ntree_update_tag_interface(struct bNodeTree *ntree);
/** Used when change parent node. */
void BKE_ntree_update_tag_parent_change(struct bNodeTree *ntree, struct bNode *node);
/** Used when an id data block changed that might be used by nodes that need to be updated. */
void BKE_ntree_update_tag_id_changed(struct Main *bmain, struct ID *id);
/** Used when an image user is updated that is used by any part of the node tree. */
void BKE_ntree_update_tag_image_user_changed(struct bNodeTree *ntree, struct ImageUser *iuser);

typedef struct NodeTreeUpdateExtraParams {
  /**
   * Data passed into the callbacks.
   */
  void *user_data;

  /**
   * Called for every tree that has been changed during the update. This can be used to send
   * notifiers to trigger redraws or depsgraph updates.
   */
  void (*tree_changed_fn)(struct ID *, struct bNodeTree *, void *user_data);

  /**
   * Called for every tree whose output value may have changed based on the provided update tags.
   * This can be used to tag the depsgraph if necessary.
   */
  void (*tree_output_changed_fn)(struct ID *, struct bNodeTree *, void *user_data);
} NodeTreeUpdateExtraParams;

/**
 * Updates #bmain based on changes to node trees.
 */
void BKE_ntree_update_main(struct Main *bmain, struct NodeTreeUpdateExtraParams *params);

/**
 * Same as #BKE_ntree_update_main, but will first only look at the provided tree and only looks
 * at #bmain when something relevant for other data-blocks changed. This avoids scanning #bmain in
 * many cases.
 *
 * If #bmain is null, only the provided tree is updated. This should only be used in very rare
 * cases because it may result it incorrectly synced data in DNA.
 *
 * If #tree is null, this is the same as calling #BKE_ntree_update_main.
 */
void BKE_ntree_update_main_tree(struct Main *bmain,
                                struct bNodeTree *ntree,
                                struct NodeTreeUpdateExtraParams *params);

#ifdef __cplusplus
}
#endif
