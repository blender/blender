/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_compute_contexts.hh"

#include "BLI_map.hh"
#include "BLI_vector.hh"

namespace blender::bke {

/**
 * When traversing the computation of a node tree (like in `socket_usage_inference.cc` or
 * `partial_eval.cc`) one often enters and exists the same compute contexts. The cache implemented
 * here avoids re-creating the same compute contexts over and over again. While requiring less
 * memory and having potentially better performance, it can also be used to ensure that the same
 * compute context will always have the same pointer, even if it's created in two different places.
 *
 * Constructing compute contexts through this cache can also be a bit more convenient.
 */
class ComputeContextCache {
  /** Allocator used to allocate the compute contexts. */
  LinearAllocator<> allocator_;
  /** The allocated computed contexts that need to be destructed in the end. */
  Vector<destruct_ptr<ComputeContext>> cache_;

  Map<std::pair<const ComputeContext *, StringRef>, const ModifierComputeContext *>
      modifier_contexts_cache_;
  Map<std::pair<const ComputeContext *, int32_t>, const GroupNodeComputeContext *>
      group_node_contexts_cache_;

 public:
  const ModifierComputeContext &for_modifier(const ComputeContext *parent,
                                             const NodesModifierData &nmd);
  const ModifierComputeContext &for_modifier(const ComputeContext *parent,
                                             StringRef modifier_name);

  const GroupNodeComputeContext &for_group_node(const ComputeContext *parent, int32_t node_id);
  const GroupNodeComputeContext &for_group_node(const ComputeContext *parent,
                                                const bNode &caller_group_node,
                                                const bNodeTree &caller_tree);

  /**
   * A fallback that does not use caching and can be used for any compute context.
   * More constructors like the ones above can be added as they become necessary.
   */
  template<typename T, typename... Args> const T &for_any_uncached(Args &&...args)
  {
    destruct_ptr<T> compute_context = allocator_.construct<T>(std::forward<Args>(args)...);
    const T &result = *compute_context;
    cache_.append(std::move(compute_context));
    return result;
  }
};

}  // namespace blender::bke
