/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct bNodeTreeType *ntreeType_Particles;

void register_node_tree_type_particles(void);

void register_node_type_particles_add_shape(void);

#ifdef __cplusplus
}
#endif
