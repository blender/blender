/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.hh"

extern bNodeTreeType *ntreeType_Geometry;

void register_node_tree_type_geo();
void register_node_type_geo_custom_group(bNodeType *ntype);

/**
 * Returns true if the socket is a Named Layer Selection field.
 */
bool is_layer_selection_field(const bNodeTreeInterfaceSocket &socket);
