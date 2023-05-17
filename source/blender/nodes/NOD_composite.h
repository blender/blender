/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BKE_node.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct bNodeTreeType *ntreeType_Composite;

void node_cmp_rlayers_outputs(struct bNodeTree *ntree, struct bNode *node);
void node_cmp_rlayers_register_pass(struct bNodeTree *ntree,
                                    struct bNode *node,
                                    struct Scene *scene,
                                    struct ViewLayer *view_layer,
                                    const char *name,
                                    eNodeSocketDatatype type);
const char *node_cmp_rlayers_sock_to_pass(int sock_index);

void register_node_type_cmp_custom_group(bNodeType *ntype);

void ntreeCompositExecTree(struct Scene *scene,
                           struct bNodeTree *ntree,
                           struct RenderData *rd,
                           int rendering,
                           int do_previews,
                           const char *view_name);

/**
 * Called from render pipeline, to tag render input and output.
 * need to do all scenes, to prevent errors when you re-render 1 scene.
 */
void ntreeCompositTagRender(struct Scene *scene);

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
void ntreeCompositUpdateRLayers(struct bNodeTree *ntree);

void ntreeCompositClearTags(struct bNodeTree *ntree);

struct bNodeSocket *ntreeCompositOutputFileAddSocket(struct bNodeTree *ntree,
                                                     struct bNode *node,
                                                     const char *name,
                                                     const struct ImageFormatData *im_format);

int ntreeCompositOutputFileRemoveActiveSocket(struct bNodeTree *ntree, struct bNode *node);
void ntreeCompositOutputFileSetPath(struct bNode *node,
                                    struct bNodeSocket *sock,
                                    const char *name);
void ntreeCompositOutputFileSetLayer(struct bNode *node,
                                     struct bNodeSocket *sock,
                                     const char *name);
/* needed in do_versions */
void ntreeCompositOutputFileUniquePath(struct ListBase *list,
                                       struct bNodeSocket *sock,
                                       const char defname[],
                                       char delim);
void ntreeCompositOutputFileUniqueLayer(struct ListBase *list,
                                        struct bNodeSocket *sock,
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
struct CryptomatteSession *ntreeCompositCryptomatteSession(const Scene *scene, bNode *node);

#ifdef __cplusplus
}
#endif
