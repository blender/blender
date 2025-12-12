/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BKE_node.hh"

#include "NOD_derived_node_tree.hh"

namespace blender::compositor {
class RenderContext;
class Profiler;
class Context;
class NodeOperation;
}  // namespace blender::compositor
namespace blender::bke {
struct bNodeTreeType;
}  // namespace blender::bke

struct CryptomatteSession;
struct Scene;
struct RenderData;
struct Render;
struct ViewLayer;

extern blender::bke::bNodeTreeType *ntreeType_Composite;

void register_node_tree_type_cmp();
void register_node_type_cmp_custom_group(blender::bke::bNodeType *ntype);

void node_cmp_rlayers_outputs(bNodeTree *ntree, bNode *node);

/**
 * Called from render pipeline, to tag render input and output.
 * need to do all scenes, to prevent errors when you re-render 1 scene.
 */
void ntreeCompositTagRender(Scene *scene);

void ntreeCompositTagNeedExec(bNode *node);

void ntreeCompositClearTags(bNodeTree *ntree);

void ntreeCompositCryptomatteSyncFromAdd(bNode *node);
void ntreeCompositCryptomatteSyncFromRemove(bNode *node);
void ntreeCompositCryptomatteAddSocket(bNode *node);
bool ntreeCompositCryptomatteRemoveSocket(bNode *node);
void ntreeCompositCryptomatteLayerPrefix(const bNode *node, char *r_prefix, size_t prefix_maxncpy);

/**
 * Update the runtime layer names with the crypto-matte layer names of the references render layer
 * or image.
 */
void ntreeCompositCryptomatteUpdateLayerNames(bNode *node);
CryptomatteSession *ntreeCompositCryptomatteSession(bNode *node);

namespace blender::nodes {

compositor::NodeOperation *get_group_input_compositor_operation(compositor::Context &context,
                                                                DNode node);
compositor::NodeOperation *get_group_output_compositor_operation(compositor::Context &context,
                                                                 DNode node);
void get_compositor_group_output_extra_info(blender::nodes::NodeExtraInfoParams &parameters);
void get_compositor_group_input_extra_info(blender::nodes::NodeExtraInfoParams &parameters);

}  // namespace blender::nodes
