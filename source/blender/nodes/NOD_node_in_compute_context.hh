/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_compute_context.hh"
#include "BLI_hash.hh"
#include "BLI_struct_equality_utils.hh"

#include "BKE_node_runtime.hh"

struct bNode;
struct bNodeSocket;

namespace blender::nodes {

struct NodeInContext;
struct SocketInContext;

/**
 * Utility struct to pair a node with a compute context. This uniquely identifies a node in an
 * node-tree evaluation.
 */
struct NodeInContext {
  const ComputeContext *context = nullptr;
  const bNode *node = nullptr;

  uint64_t hash() const;
  ComputeContextHash context_hash() const;
  const bNode *operator->() const;
  const bNode &operator*() const;
  operator bool() const;

  SocketInContext input_socket(int index) const;
  SocketInContext output_socket(int index) const;

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

  uint64_t hash() const;
  ComputeContextHash context_hash() const;
  const bNodeSocket *operator->() const;
  const bNodeSocket &operator*() const;
  operator bool() const;

  NodeInContext owner_node() const;

  /**
   * Two sockets in context compare equal if their context hash is equal, not the pointer to the
   * context. This is important as the same compute context may be constructed multiple times.
   */
  BLI_STRUCT_EQUALITY_OPERATORS_2(SocketInContext, context_hash(), socket)
};

/* -------------------------------------------------------------------- */
/** \name #NodeInContext Inline Methods
 * \{ */

inline uint64_t NodeInContext::hash() const
{
  return get_default_hash(this->context_hash(), this->node);
}

inline ComputeContextHash NodeInContext::context_hash() const
{
  return context ? context->hash() : ComputeContextHash{};
}

inline const bNode *NodeInContext::operator->() const
{
  return this->node;
}

inline const bNode &NodeInContext::operator*() const
{
  return *this->node;
}

inline NodeInContext::operator bool() const
{
  return this->node != nullptr;
}

inline SocketInContext NodeInContext::input_socket(const int index) const
{
  return {this->context, &this->node->input_socket(index)};
}

inline SocketInContext NodeInContext::output_socket(const int index) const
{
  return {this->context, &this->node->output_socket(index)};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #SocketInContext Inline Methods
 * \{ */

inline uint64_t SocketInContext::hash() const
{
  return get_default_hash(this->context_hash(), this->socket);
}

inline ComputeContextHash SocketInContext::context_hash() const
{
  return context ? context->hash() : ComputeContextHash{};
}

inline const bNodeSocket *SocketInContext::operator->() const
{
  return this->socket;
}

inline const bNodeSocket &SocketInContext::operator*() const
{
  return *this->socket;
}

inline SocketInContext::operator bool() const
{
  return this->socket != nullptr;
}

inline NodeInContext SocketInContext::owner_node() const
{
  return {this->context, &this->socket->owner_node()};
}

/** \} */

}  // namespace blender::nodes
