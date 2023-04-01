/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"

#include "BKE_compute_contexts.hh"

namespace blender::bke {

ModifierComputeContext::ModifierComputeContext(const ComputeContext *parent,
                                               std::string modifier_name)
    : ComputeContext(s_static_type, parent), modifier_name_(std::move(modifier_name))
{
  hash_.mix_in(s_static_type, strlen(s_static_type));
  hash_.mix_in(modifier_name_.data(), modifier_name_.size());
}

void ModifierComputeContext::print_current_in_line(std::ostream &stream) const
{
  stream << "Modifier: " << modifier_name_;
}

NodeGroupComputeContext::NodeGroupComputeContext(
    const ComputeContext *parent,
    const int32_t node_id,
    const std::optional<ComputeContextHash> &cached_hash)
    : ComputeContext(s_static_type, parent), node_id_(node_id)
{
  if (cached_hash.has_value()) {
    hash_ = *cached_hash;
  }
  else {
    /* Mix static type and node id into a single buffer so that only a single call to #mix_in is
     * necessary. */
    const int type_size = strlen(s_static_type);
    const int buffer_size = type_size + 1 + sizeof(int32_t);
    DynamicStackBuffer<64, 8> buffer_owner(buffer_size, 8);
    char *buffer = static_cast<char *>(buffer_owner.buffer());
    memcpy(buffer, s_static_type, type_size + 1);
    memcpy(buffer + type_size + 1, &node_id_, sizeof(int32_t));
    hash_.mix_in(buffer, buffer_size);
  }
}

NodeGroupComputeContext::NodeGroupComputeContext(const ComputeContext *parent, const bNode &node)
    : NodeGroupComputeContext(parent, node.identifier)
{
#ifdef DEBUG
  debug_node_name_ = node.name;
#endif
}

void NodeGroupComputeContext::print_current_in_line(std::ostream &stream) const
{
#ifdef DEBUG
  if (!debug_node_name_.empty()) {
    stream << "Node: " << debug_node_name_;
    return;
  }
#endif
  stream << "Node ID: " << node_id_;
}

}  // namespace blender::bke
