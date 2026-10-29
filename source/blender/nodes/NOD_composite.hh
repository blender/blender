/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BLI_span.hh"

#include "BKE_node.hh"

namespace blender {

struct CryptomatteSession;
struct Scene;

extern bke::bNodeTreeType *ntreeType_Composite;

void register_node_tree_type_cmp();
void register_node_type_cmp_custom_group(bke::bNodeType *ntype);

void node_cmp_rlayers_outputs(bNodeTree *ntree, bNode *node);

/**
 * Called from render pipeline, to tag render input and output.
 * need to do all scenes, to prevent errors when you re-render 1 scene.
 */
void ntreeCompositTagRender(Scene *scene);

/**
 * \note: Requires that viewlayers are in sync (call `BKE_main_view_layers_synced_ensure` or
 * similar before).
 */
void ntreeCompositCryptomatteSyncFromAdd(bNode *node);

void ntreeCompositCryptomatteSyncFromRemove(bNode *node);
void ntreeCompositCryptomatteAddSocket(bNode *node);
bool ntreeCompositCryptomatteRemoveSocket(bNode *node);
void ntreeCompositCryptomatteLayerPrefix(const bNode *node, MutableCStr r_prefix);

/**
 * Update the runtime layer names with the crypto-matte layer names of the references render layer
 * or image.
 */
void ntreeCompositCryptomatteUpdateLayerNames(bNode *node);

/**
 * \note: Requires that viewlayers are in sync (call `BKE_main_view_layers_synced_ensure` or
 * similar before).
 */
CryptomatteSession *ntreeCompositCryptomatteSession(bNode *node);

}  // namespace blender
