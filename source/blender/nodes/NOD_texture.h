/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BKE_node.hh"

#ifdef __cplusplus
extern "C" {
#endif

extern struct blender::bke::bNodeTreeType *ntreeType_Texture;

void ntreeTexCheckCyclics(struct bNodeTree *ntree);
struct bNodeTreeExec *ntreeTexBeginExecTree(struct bNodeTree *ntree);
void ntreeTexEndExecTree(struct bNodeTreeExec *exec);
int ntreeTexExecTree(struct bNodeTree *ntree,
                     struct TexResult *target,
                     const float co[3],
                     float dxt[3],
                     float dyt[3],
                     int osatex,
                     short thread,
                     const struct Tex *tex,
                     short which_output,
                     int cfra,
                     int preview,
                     struct MTex *mtex);

#ifdef __cplusplus
}
#endif
