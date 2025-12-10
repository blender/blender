/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

struct bContext;
struct bNodeTree;
struct ID;
struct Main;
struct Scene;

namespace blender::nodes {

/**
 * Assumes nothing being done in ntree yet, sets the default in/out node.
 * Called from shading buttons or header.
 */
void node_tree_shader_default(const bContext *C, Main *bmain, ID *id);

/**
 * Assumes nothing being done in ntree yet, sets the default in/out node.
 * Called from compositing buttons or header.
 */
void node_tree_composit_default(const bContext *C, Scene *sce);

/**
 * Initializes an empty compositing node tree with default nodes.
 */
void node_tree_composit_default_init(const bContext *C, bNodeTree *ntree);

}  // namespace blender::nodes
