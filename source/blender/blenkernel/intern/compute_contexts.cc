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

NodeGroupComputeContext::NodeGroupComputeContext(const ComputeContext *parent, const int node_id)
    : ComputeContext(s_static_type, parent), node_id_(node_id)
{
  hash_.mix_in(s_static_type, strlen(s_static_type));
  hash_.mix_in(&node_id_, sizeof(int32_t));
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
