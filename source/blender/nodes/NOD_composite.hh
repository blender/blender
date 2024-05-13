/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BKE_node.hh"

namespace blender::realtime_compositor {
class RenderContext;
}
namespace blender::compositor {
class ProfilerData;
}
namespace blender::bke {
struct bNodeTreeType;
}  // namespace blender::bke

struct CryptomatteSession;
struct Scene;
struct RenderData;
struct Render;
struct ViewLayer;

extern blender::bke::bNodeTreeType *ntreeType_Composite;

void node_cmp_rlayers_outputs(bNodeTree *ntree, bNode *node);
void node_cmp_rlayers_register_pass(bNodeTree *ntree,
                                    bNode *node,
                                    Scene *scene,
                                    ViewLayer *view_layer,
                                    const char *name,
                                    eNodeSocketDatatype type);
const char *node_cmp_rlayers_sock_to_pass(int sock_index);

void register_node_type_cmp_custom_group(blender::bke::bNodeType *ntype);

void ntreeCompositExecTree(Render *render,
                           Scene *scene,
                           bNodeTree *ntree,
                           RenderData *rd,
                           const char *view_name,
                           blender::realtime_compositor::RenderContext *render_context,
                           blender::compositor::ProfilerData &profiler_data);

/**
 * Called from render pipeline, to tag render input and output.
 * need to do all scenes, to prevent errors when you re-render 1 scene.
 */
void ntreeCompositTagRender(Scene *scene);

void ntreeCompositTagNeedExec(bNode *node);

/**
 * Update the outputs of the render layer nodes.
 * Since the outputs depend on the render engine, this part is a bit complex:
 * - #ntreeCompositUpdateRLayers is called and loops over all render layer nodes.
 * - Each render layer node calls the update function of the
 *   render engine that's used for its scene.
 * - The render engine calls RE_engine_register_pass for each pass.
 * - #RE_engine_register_pass calls #node_cmp_rlayers_register_pass.
 */
void ntreeCompositUpdateRLayers(bNodeTree *ntree);

void ntreeCompositClearTags(bNodeTree *ntree);

bNodeSocket *ntreeCompositOutputFileAddSocket(bNodeTree *ntree,
                                              bNode *node,
                                              const char *name,
                                              const ImageFormatData *im_format);

int ntreeCompositOutputFileRemoveActiveSocket(bNodeTree *ntree, bNode *node);
void ntreeCompositOutputFileSetPath(bNode *node, bNodeSocket *sock, const char *name);
void ntreeCompositOutputFileSetLayer(bNode *node, bNodeSocket *sock, const char *name);
/* needed in do_versions */
void ntreeCompositOutputFileUniquePath(ListBase *list,
                                       bNodeSocket *sock,
                                       const char defname[],
                                       char delim);
void ntreeCompositOutputFileUniqueLayer(ListBase *list,
                                        bNodeSocket *sock,
                                        const char defname[],
                                        char delim);

void ntreeCompositColorBalanceSyncFromLGG(bNodeTree *ntree, bNode *node);
void ntreeCompositColorBalanceSyncFromCDL(bNodeTree *ntree, bNode *node);

void ntreeCompositCryptomatteSyncFromAdd(const Scene *scene, bNode *node);
void ntreeCompositCryptomatteSyncFromRemove(bNode *node);
bNodeSocket *ntreeCompositCryptomatteAddSocket(bNodeTree *ntree, bNode *node);
int ntreeCompositCryptomatteRemoveSocket(bNodeTree *ntree, bNode *node);
void ntreeCompositCryptomatteLayerPrefix(const Scene *scene,
                                         const bNode *node,
                                         char *r_prefix,
                                         size_t prefix_maxncpy);

/**
 * Update the runtime layer names with the crypto-matte layer names of the references render layer
 * or image.
 */
void ntreeCompositCryptomatteUpdateLayerNames(const Scene *scene, bNode *node);
CryptomatteSession *ntreeCompositCryptomatteSession(const Scene *scene, bNode *node);
