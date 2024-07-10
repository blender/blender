/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_compute_context.hh"
#include "BLI_hash.hh"
#include "BLI_struct_equality_utils.hh"

struct bNode;
struct bNodeSocket;

namespace blender::nodes {

/**
 * Utility struct to pair a node with a compute context. This uniquely identifies a node in an
 * node-tree evaluation.
 */
struct NodeInContext {
  const ComputeContext *context = nullptr;
  const bNode *node = nullptr;

  uint64_t hash() const
  {
    return get_default_hash(this->context_hash(), this->node);
  }

  ComputeContextHash context_hash() const
  {
    return context ? context->hash() : ComputeContextHash{};
  }

  /**
   * Two nodes in context compare equal if their context hash is equal, not the pointer to the
   * context. This is important as the same compute context may be constructed multiple times.
   */
  BLI_STRUCT_EQUALITY_OPERATORS_2(NodeInContext, context_hash(), node)
};

/**
 * Utility struct to pair a socket with a compute context. This unique identifies a socket in a
 * node-tree evaluation.
 */
struct SocketInContext {
  const ComputeContext *context = nullptr;
  const bNodeSocket *socket = nullptr;

  uint64_t hash() const
  {
    return get_default_hash(this->context_hash(), this->socket);
  }

  ComputeContextHash context_hash() const
  {
    return context ? context->hash() : ComputeContextHash{};
  }

  /**
   * Two sockets in context compare equal if their context hash is equal, not the pointer to the
   * context. This is important as the same compute context may be constructed multiple times.
   */
  BLI_STRUCT_EQUALITY_OPERATORS_2(SocketInContext, context_hash(), socket)
};

}  // namespace blender::nodes
