/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "DNA_node_tree_interface_types.h"
#include "DNA_node_types.h"

#include "BKE_animsys.h"
#include "BKE_node.hh"

namespace blender::bke::node_interface {

/**
 * Create a constant value node for the socket type.
 * \return Constant value input node or null if the socket type is not supported.
 */
bNode *create_proxy_const_input_node(eNodeSocketDatatype socket_type,
                                     const bNodeTree &src_tree,
                                     const bNodeSocket &src_socket,
                                     bContext &C,
                                     bNodeTree &dst_tree,
                                     Vector<AnimationBasePathChange> &anim_basepaths);

/**
 * Create an implicit input node for the socket type.
 * \return Implicit input field node or null if the socket type or default input is not supported.
 */
bNode *create_proxy_implicit_input_node(eNodeSocketDatatype socket_type,
                                        NodeDefaultInputType default_input,
                                        bContext &C,
                                        bNodeTree &tree);

/**
 * Create a type conversion node for the socket type.
 * \return Converter node or null if the socket type is not supported.
 */
bNode *create_proxy_converter_node(eNodeSocketDatatype socket_type,
                                   const bNodeTree &src_tree,
                                   const bNodeSocket *src_socket,
                                   bContext &C,
                                   bNodeTree &dst_tree,
                                   Vector<AnimationBasePathChange> &anim_basepaths);

}  // namespace blender::bke::node_interface
