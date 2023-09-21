/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "DNA_listBase.h"

#include "BKE_node.h"

/* for FOREACH_NODETREE_BEGIN */
#include "DNA_node_types.h"

#include "RNA_types.hh"

#include "BLI_map.hh"
#include "BLI_string_ref.hh"

namespace blender::bke {

bNodeTree *ntreeAddTreeEmbedded(Main *bmain, ID *owner_id, const char *name, const char *idname);

/* Copy/free functions, need to manage ID users. */

/**
 * Free (or release) any data used by this node-tree.
 * Does not free the node-tree itself and does no ID user counting.
 */
void ntreeFreeTree(bNodeTree *ntree);

bNodeTree *ntreeCopyTree_ex(const bNodeTree *ntree, Main *bmain, bool do_id_user);
bNodeTree *ntreeCopyTree(Main *bmain, const bNodeTree *ntree);

void ntreeFreeLocalNode(bNodeTree *ntree, bNode *node);

void ntreeUpdateAllNew(Main *main);

/** Update asset meta-data cache of data-block properties. */
void node_update_asset_metadata(bNodeTree &node_tree);

void ntreeNodeFlagSet(const bNodeTree *ntree, int flag, bool enable);

/**
 * Merge local tree results back, and free local tree.
 *
 * We have to assume the editor already changed completely.
 */
void ntreeLocalMerge(Main *bmain, bNodeTree *localtree, bNodeTree *ntree);

/**
 * \note `ntree` itself has been read!
 */
void ntreeBlendReadData(BlendDataReader *reader, ID *owner_id, bNodeTree *ntree);

bool node_type_is_undefined(const bNode *node);

bool nodeIsStaticSocketType(const bNodeSocketType *stype);

const char *nodeSocketSubTypeLabel(int subtype);

void nodeRemoveSocketEx(bNodeTree *ntree, bNode *node, bNodeSocket *sock, bool do_id_user);

void nodeModifySocketType(bNodeTree *ntree, bNode *node, bNodeSocket *sock, const char *idname);

/**
 * \note Goes over entire tree.
 */
void nodeUnlinkNode(bNodeTree *ntree, bNode *node);

/**
 * Rebuild the `node_by_id` runtime vector set. Call after removing a node if not handled
 * separately. This is important instead of just using `nodes_by_id.remove()` since it maintains
 * the node order.
 */
void nodeRebuildIDVector(bNodeTree *node_tree);

/**
 * \note keeps socket list order identical, for copying links.
 * \param use_unique: If true, make sure the node's identifier and name are unique in the new
 * tree. Must be *true* if the \a dst_tree had nodes that weren't in the source node's tree.
 * Must be *false* when simply copying a node tree, so that identifiers don't change.
 */
bNode *node_copy_with_mapping(bNodeTree *dst_tree,
                              const bNode &node_src,
                              int flag,
                              bool use_unique,
                              Map<const bNodeSocket *, bNodeSocket *> &new_socket_map);

bNode *node_copy(bNodeTree *dst_tree, const bNode &src_node, int flag, bool use_unique);

/**
 * Move socket default from \a src (input socket) to locations specified by \a dst (output socket).
 * Result value moved in specific location. (potentially multiple group nodes socket values, if \a
 * dst is a group input node).
 * \note Conceptually, the effect should be such that the evaluation of
 * this graph again returns the value in src.
 */
void node_socket_move_default_value(Main &bmain,
                                    bNodeTree &tree,
                                    bNodeSocket &src,
                                    bNodeSocket &dst);

/**
 * Free the node itself.
 *
 * \note ID user reference-counting and changing the `nodes_by_id` vector are up to the caller.
 */
void node_free_node(bNodeTree *tree, bNode *node);

/**
 * Set the mute status of a single link.
 */
void nodeLinkSetMute(bNodeTree *ntree, bNodeLink *link, const bool muted);

bool nodeLinkIsSelected(const bNodeLink *link);

void nodeInternalRelink(bNodeTree *ntree, bNode *node);

float2 nodeToView(const bNode *node, float2 loc);

float2 nodeFromView(const bNode *node, float2 view_loc);

void nodePositionRelative(bNode *from_node,
                          const bNode *to_node,
                          const bNodeSocket *from_sock,
                          const bNodeSocket *to_sock);

void nodePositionPropagate(bNode *node);

/**
 * \note Recursive.
 */
bNode *nodeFindRootParent(bNode *node);

/**
 * Iterate over a chain of nodes, starting with \a node_start, executing
 * \a callback for each node (which can return false to end iterator).
 *
 * \param reversed: for backwards iteration
 * \note Recursive
 */
void nodeChainIter(const bNodeTree *ntree,
                   const bNode *node_start,
                   bool (*callback)(bNode *, bNode *, void *, const bool),
                   void *userdata,
                   bool reversed);

/**
 * Iterate over a chain of nodes, starting with \a node_start, executing
 * \a callback for each node (which can return false to end iterator).
 *
 * Faster than nodeChainIter. Iter only once per node.
 * Can be called recursively (using another nodeChainIterBackwards) by
 * setting the recursion_lvl accordingly.
 *
 * \note Needs updated socket links (ntreeUpdateTree).
 * \note Recursive
 */
void nodeChainIterBackwards(const bNodeTree *ntree,
                            const bNode *node_start,
                            bool (*callback)(bNode *, bNode *, void *),
                            void *userdata,
                            int recursion_lvl);

/**
 * Iterate over all parents of \a node, executing \a callback for each parent
 * (which can return false to end iterator)
 *
 * \note Recursive
 */
void nodeParentsIter(bNode *node, bool (*callback)(bNode *, void *), void *userdata);

/**
 * A dangling reroute node is a reroute node that does *not* have a "data source", i.e. no
 * non-reroute node is connected to its input.
 */
bool nodeIsDanglingReroute(const bNodeTree *ntree, const bNode *node);

bNode *nodeGetActivePaintCanvas(bNodeTree *ntree);

/**
 * \brief Does the given node supports the sub active flag.
 *
 * \param sub_active: The active flag to check. #NODE_ACTIVE_TEXTURE / #NODE_ACTIVE_PAINT_CANVAS.
 */
bool nodeSupportsActiveFlag(const bNode *node, int sub_active);

void nodeSetSocketAvailability(bNodeTree *ntree, bNodeSocket *sock, bool is_available);

/**
 * If the node implements a `declare` function, this function makes sure that `node->declaration`
 * is up to date. It is expected that the sockets of the node are up to date already.
 */
bool nodeDeclarationEnsure(bNodeTree *ntree, bNode *node);

/**
 * Just update `node->declaration` if necessary. This can also be called on nodes that may not be
 * up to date (e.g. because the need versioning or are dynamic).
 */
bool nodeDeclarationEnsureOnOutdatedNode(bNodeTree *ntree, bNode *node);

/**
 * Update `socket->declaration` for all sockets in the node. This assumes that the node declaration
 * and sockets are up to date already.
 */
void nodeSocketDeclarationsUpdate(bNode *node);

using bNodeInstanceHashIterator = GHashIterator;

BLI_INLINE bNodeInstanceHashIterator *node_instance_hash_iterator_new(bNodeInstanceHash *hash)
{
  return BLI_ghashIterator_new(hash->ghash);
}

BLI_INLINE void node_instance_hash_iterator_init(bNodeInstanceHashIterator *iter,
                                                 bNodeInstanceHash *hash)
{
  BLI_ghashIterator_init(iter, hash->ghash);
}

BLI_INLINE void node_instance_hash_iterator_free(bNodeInstanceHashIterator *iter)
{
  BLI_ghashIterator_free(iter);
}

BLI_INLINE bNodeInstanceKey node_instance_hash_iterator_get_key(bNodeInstanceHashIterator *iter)
{
  return *(bNodeInstanceKey *)BLI_ghashIterator_getKey(iter);
}

BLI_INLINE void *node_instance_hash_iterator_get_value(bNodeInstanceHashIterator *iter)
{
  return BLI_ghashIterator_getValue(iter);
}

BLI_INLINE void node_instance_hash_iterator_step(bNodeInstanceHashIterator *iter)
{
  BLI_ghashIterator_step(iter);
}

BLI_INLINE bool node_instance_hash_iterator_done(bNodeInstanceHashIterator *iter)
{
  return BLI_ghashIterator_done(iter);
}

#define NODE_INSTANCE_HASH_ITER(iter_, hash_) \
  for (blender::bke::node_instance_hash_iterator_init(&iter_, hash_); \
       blender::bke::node_instance_hash_iterator_done(&iter_) == false; \
       blender::bke::node_instance_hash_iterator_step(&iter_))

/* Node Previews */
bool node_preview_used(const bNode *node);

bNodePreview *node_preview_verify(
    bNodeInstanceHash *previews, bNodeInstanceKey key, int xsize, int ysize, bool create);

bNodePreview *node_preview_copy(bNodePreview *preview);

void node_preview_free(bNodePreview *preview);

void node_preview_init_tree(bNodeTree *ntree, int xsize, int ysize);

void node_preview_remove_unused(bNodeTree *ntree);

void node_preview_clear(bNodePreview *preview);

void node_preview_merge_tree(bNodeTree *to_ntree, bNodeTree *from_ntree, bool remove_old);

/* -------------------------------------------------------------------- */
/** \name Node Type Access
 * \{ */

void nodeLabel(const bNodeTree *ntree, const bNode *node, char *label, int maxlen);

/**
 * Get node socket label if it is set.
 */
const char *nodeSocketLabel(const bNodeSocket *sock);

/**
 * Initialize a new node type struct with default values and callbacks.
 */
void node_type_base(bNodeType *ntype, int type, const char *name, short nclass);

void node_type_socket_templates(bNodeType *ntype,
                                bNodeSocketTemplate *inputs,
                                bNodeSocketTemplate *outputs);

void node_type_size(bNodeType *ntype, int width, int minwidth, int maxwidth);

enum class eNodeSizePreset : int8_t {
  DEFAULT,
  SMALL,
  MIDDLE,
  LARGE,
};

void node_type_size_preset(bNodeType *ntype, eNodeSizePreset size);

/* -------------------------------------------------------------------- */
/** \name Node Generic Functions
 * \{ */

bool node_is_connected_to_output(const bNodeTree *ntree, const bNode *node);

bNodeSocket *node_find_enabled_socket(bNode &node, eNodeSocketInOut in_out, StringRef name);

bNodeSocket *node_find_enabled_input_socket(bNode &node, StringRef name);

bNodeSocket *node_find_enabled_output_socket(bNode &node, StringRef name);

extern bNodeTreeType NodeTreeTypeUndefined;
extern bNodeType NodeTypeUndefined;
extern bNodeSocketType NodeSocketTypeUndefined;

/**
 * Contains information about a specific kind of zone (e.g. simulation or repeat zone in geometry
 * nodes). This allows writing code that works for all kinds of zones automatically, reducing
 * redundancy and the amount of boilerplate needed when adding a new zone type.
 */
class bNodeZoneType {
 public:
  std::string input_idname;
  std::string output_idname;
  int input_type;
  int output_type;
  int theme_id;

  virtual ~bNodeZoneType() = default;

  virtual const int &get_corresponding_output_id(const bNode &input_bnode) const = 0;

  int &get_corresponding_output_id(bNode &input_bnode) const
  {
    return const_cast<int &>(
        this->get_corresponding_output_id(const_cast<const bNode &>(input_bnode)));
  }

  const bNode *get_corresponding_input(const bNodeTree &tree, const bNode &output_bnode) const;
  bNode *get_corresponding_input(bNodeTree &tree, const bNode &output_bnode) const;

  const bNode *get_corresponding_output(const bNodeTree &tree, const bNode &input_bnode) const;
  bNode *get_corresponding_output(bNodeTree &tree, const bNode &input_bnode) const;
};

void register_node_zone_type(const bNodeZoneType &zone_type);

Span<const bNodeZoneType *> all_zone_types();
Span<int> all_zone_node_types();
Span<int> all_zone_input_node_types();
Span<int> all_zone_output_node_types();
const bNodeZoneType *zone_type_by_node_type(const int node_type);

}  // namespace blender::bke

#define NODE_STORAGE_FUNCS(StorageT) \
  [[maybe_unused]] static StorageT &node_storage(bNode &node) \
  { \
    return *static_cast<StorageT *>(node.storage); \
  } \
  [[maybe_unused]] static const StorageT &node_storage(const bNode &node) \
  { \
    return *static_cast<const StorageT *>(node.storage); \
  }
