/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BKE_node.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct bNodeTreeType *ntreeType_Shader;

void register_node_type_sh_custom_group(bNodeType *ntype);

struct bNodeTreeExec *ntreeShaderBeginExecTree(struct bNodeTree *ntree);
void ntreeShaderEndExecTree(struct bNodeTreeExec *exec);

/**
 * Find an output node of the shader tree.
 *
 * \note it will only return output which is NOT in the group, which isn't how
 * render engines works but it's how the GPU shader compilation works. This we
 * can change in the future and make it a generic function, but for now it stays
 * private here.
 */
struct bNode *ntreeShaderOutputNode(struct bNodeTree *ntree, int target);

/**
 * This one needs to work on a local tree.
 */
void ntreeGPUMaterialNodes(struct bNodeTree *localtree, struct GPUMaterial *mat);

#ifdef __cplusplus
}
#endif
