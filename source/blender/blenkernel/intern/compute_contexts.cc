/* SPDX-License-Identifier: GPL-2.0-or-later */

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

NodeGroupComputeContext::NodeGroupComputeContext(const ComputeContext *parent,
                                                 std::string node_name)
    : ComputeContext(s_static_type, parent), node_name_(std::move(node_name))
{
  hash_.mix_in(s_static_type, strlen(s_static_type));
  hash_.mix_in(node_name_.data(), node_name_.size());
}

StringRefNull NodeGroupComputeContext::node_name() const
{
  return node_name_;
}

void NodeGroupComputeContext::print_current_in_line(std::ostream &stream) const
{
  stream << "Node: " << node_name_;
}

}  // namespace blender::bke
