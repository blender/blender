/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <ostream>

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"

#include "BKE_compute_context_cache.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_lib_id.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

namespace blender::bke {

ModifierComputeContext::ModifierComputeContext(const ComputeContext *parent,
                                               const NodesModifierData &nmd)
    : ModifierComputeContext(parent, nmd.modifier.persistent_uid)
{
  nmd_ = &nmd;
}

ModifierComputeContext::ModifierComputeContext(const ComputeContext *parent,
                                               const int modifier_uid)
    : ComputeContext(parent), modifier_uid_(std::move(modifier_uid))
{
}

ComputeContextHash ModifierComputeContext::compute_hash() const
{
  return ComputeContextHash::from(parent_, "MODIFIER", modifier_uid_);
}

void ModifierComputeContext::print_current_in_line(std::ostream &stream) const
{
  if (nmd_) {
    stream << "Modifier: " << nmd_->modifier.name;
  }
}

NodeComputeContext::NodeComputeContext(const ComputeContext *parent,
                                       int32_t node_id,
                                       const bNodeTree *tree)
    : ComputeContext(parent), node_id_(node_id), tree_(tree)
{
}

ComputeContextHash NodeComputeContext::compute_hash() const
{
  return ComputeContextHash::from(parent_, "NODE", node_id_);
}

const bNode *NodeComputeContext::node() const
{
  if (tree_) {
    return tree_->node_by_id(node_id_);
  }
  return nullptr;
}

void NodeComputeContext::print_current_in_line(std::ostream &stream) const
{
  if (tree_) {
    if (const bNode *node = tree_->node_by_id(node_id_)) {
      stream << "Node: " << node_label(*tree_, *node);
      return;
    }
  }
  stream << "Node ID: " << node_id_;
}

SimulationZoneComputeContext::SimulationZoneComputeContext(const ComputeContext *parent,
                                                           const int32_t output_node_id)
    : ComputeContext(parent), output_node_id_(output_node_id)
{
}

SimulationZoneComputeContext::SimulationZoneComputeContext(const ComputeContext *parent,
                                                           const bNode &node)
    : SimulationZoneComputeContext(parent, node.identifier)
{
}

ComputeContextHash SimulationZoneComputeContext::compute_hash() const
{
  return ComputeContextHash::from(parent_, "SIM_ZONE", output_node_id_);
}

void SimulationZoneComputeContext::print_current_in_line(std::ostream &stream) const
{
  stream << "Simulation Zone ID: " << output_node_id_;
}

RepeatZoneComputeContext::RepeatZoneComputeContext(const ComputeContext *parent,
                                                   const int32_t output_node_id,
                                                   const int iteration)
    : ComputeContext(parent), output_node_id_(output_node_id), iteration_(iteration)
{
}

RepeatZoneComputeContext::RepeatZoneComputeContext(const ComputeContext *parent,
                                                   const bNode &node,
                                                   const int iteration)
    : RepeatZoneComputeContext(parent, node.identifier, iteration)
{
}

ComputeContextHash RepeatZoneComputeContext::compute_hash() const
{
  return ComputeContextHash::from(parent_, "REPEAT_ZONE", output_node_id_, iteration_);
}

void RepeatZoneComputeContext::print_current_in_line(std::ostream &stream) const
{
  stream << "Repeat Zone ID: " << output_node_id_;
}

ForeachGeometryElementZoneComputeContext::ForeachGeometryElementZoneComputeContext(
    const ComputeContext *parent, const int32_t output_node_id, const int index)
    : ComputeContext(parent), output_node_id_(output_node_id), index_(index)
{
}

ForeachGeometryElementZoneComputeContext::ForeachGeometryElementZoneComputeContext(
    const ComputeContext *parent, const bNode &node, const int index)
    : ForeachGeometryElementZoneComputeContext(parent, node.identifier, index)
{
}

ComputeContextHash ForeachGeometryElementZoneComputeContext::compute_hash() const
{
  return ComputeContextHash::from(parent_, "FOREACH_GEOMETRY_ELEMENT", output_node_id_, index_);
}

void ForeachGeometryElementZoneComputeContext::print_current_in_line(std::ostream &stream) const
{
  stream << "Foreach Geometry Element Zone ID: " << output_node_id_;
}

EvaluateClosureComputeContext::EvaluateClosureComputeContext(
    const ComputeContext *parent,
    int32_t node_id,
    const bNodeTree *tree,
    const std::optional<nodes::ClosureSourceLocation> &closure_source_location)
    : NodeComputeContext(parent, node_id, tree), closure_source_location_(closure_source_location)
{
}

bool EvaluateClosureComputeContext::is_recursive() const
{
  if (!closure_source_location_) {
    /* Can't determine recursiveness in this case. */
    return false;
  }
  for (const ComputeContext *parent = parent_; parent; parent = parent->parent()) {
    if (const auto *evaluate_closure_compute_context =
            dynamic_cast<const EvaluateClosureComputeContext *>(parent))
    {
      if (!evaluate_closure_compute_context->closure_source_location_) {
        continue;
      }
      if (evaluate_closure_compute_context->closure_source_location_->tree ==
              closure_source_location_->tree &&
          evaluate_closure_compute_context->closure_source_location_->closure_output_node_id ==
              closure_source_location_->closure_output_node_id)
      {
        return true;
      }
    }
  }
  return false;
}

OperatorComputeContext::OperatorComputeContext() : OperatorComputeContext(nullptr) {}

OperatorComputeContext::OperatorComputeContext(const ComputeContext *parent)
    : ComputeContext(parent)
{
}

OperatorComputeContext::OperatorComputeContext(const ComputeContext *parent, const bNodeTree &tree)
    : OperatorComputeContext(parent)
{
  tree_ = &tree;
}

ComputeContextHash OperatorComputeContext::compute_hash() const
{
  return ComputeContextHash::from(parent_, "OPERATOR");
}

void OperatorComputeContext::print_current_in_line(std::ostream &stream) const
{
  stream << "Operator";
}

ShaderComputeContext::ShaderComputeContext(const ComputeContext *parent, const bNodeTree *tree)
    : ComputeContext(parent), tree_(tree)
{
}

ComputeContextHash ShaderComputeContext::compute_hash() const
{
  return ComputeContextHash::from(parent_, "SHADER");
}

void ShaderComputeContext::print_current_in_line(std::ostream &stream) const
{
  stream << "Shader ";
  if (tree_) {
    stream << BKE_id_name(tree_->id);
  }
}

const ModifierComputeContext &ComputeContextCache::for_modifier(const ComputeContext *parent,
                                                                const NodesModifierData &nmd)
{
  return *modifier_contexts_cache_.lookup_or_add_cb(
      std::pair{parent, nmd.modifier.persistent_uid},
      [&]() { return &this->for_any_uncached<ModifierComputeContext>(parent, nmd); });
}

const ModifierComputeContext &ComputeContextCache::for_modifier(const ComputeContext *parent,
                                                                const int modifier_uid)
{
  return *modifier_contexts_cache_.lookup_or_add_cb(std::pair{parent, modifier_uid}, [&]() {
    return &this->for_any_uncached<ModifierComputeContext>(parent, modifier_uid);
  });
}

const OperatorComputeContext &ComputeContextCache::for_operator(const ComputeContext *parent)
{
  return *operator_contexts_cache_.lookup_or_add_cb(
      parent, [&]() { return &this->for_any_uncached<OperatorComputeContext>(parent); });
}

const OperatorComputeContext &ComputeContextCache::for_operator(const ComputeContext *parent,
                                                                const bNodeTree &tree)
{
  return *operator_contexts_cache_.lookup_or_add_cb(
      parent, [&]() { return &this->for_any_uncached<OperatorComputeContext>(parent, tree); });
}

const ShaderComputeContext &ComputeContextCache::for_shader(const ComputeContext *parent,
                                                            const bNodeTree *tree)
{
  return *shader_contexts_cache_.lookup_or_add_cb(
      parent, [&]() { return &this->for_any_uncached<ShaderComputeContext>(parent, tree); });
}

const GroupNodeComputeContext &ComputeContextCache::for_group_node(const ComputeContext *parent,
                                                                   const int32_t node_id,
                                                                   const bNodeTree *tree)
{
  return *group_node_contexts_cache_.lookup_or_add_cb(std::pair{parent, node_id}, [&]() {
    return &this->for_any_uncached<GroupNodeComputeContext>(parent, node_id, tree);
  });
}

const SimulationZoneComputeContext &ComputeContextCache::for_simulation_zone(
    const ComputeContext *parent, int output_node_id)
{
  return *simulation_zone_contexts_cache_.lookup_or_add_cb(
      std::pair{parent, output_node_id}, [&]() {
        return &this->for_any_uncached<SimulationZoneComputeContext>(parent, output_node_id);
      });
}

const SimulationZoneComputeContext &ComputeContextCache::for_simulation_zone(
    const ComputeContext *parent, const bNode &output_node)
{
  return *simulation_zone_contexts_cache_.lookup_or_add_cb(
      std::pair{parent, output_node.identifier}, [&]() {
        return &this->for_any_uncached<SimulationZoneComputeContext>(parent, output_node);
      });
}

const RepeatZoneComputeContext &ComputeContextCache::for_repeat_zone(const ComputeContext *parent,
                                                                     int32_t output_node_id,
                                                                     int iteration)
{
  return *repeat_zone_contexts_cache_.lookup_or_add_cb(
      std::pair{parent, std::pair{output_node_id, iteration}}, [&]() {
        return &this->for_any_uncached<RepeatZoneComputeContext>(
            parent, output_node_id, iteration);
      });
}

const RepeatZoneComputeContext &ComputeContextCache::for_repeat_zone(const ComputeContext *parent,
                                                                     const bNode &output_node,
                                                                     int iteration)
{
  return *repeat_zone_contexts_cache_.lookup_or_add_cb(
      std::pair{parent, std::pair{output_node.identifier, iteration}}, [&]() {
        return &this->for_any_uncached<RepeatZoneComputeContext>(parent, output_node, iteration);
      });
}

const ForeachGeometryElementZoneComputeContext &ComputeContextCache::
    for_foreach_geometry_element_zone(const ComputeContext *parent,
                                      int32_t output_node_id,
                                      int index)
{
  return *foreach_geometry_element_zone_contexts_cache_.lookup_or_add_cb(
      std::pair{parent, std::pair{output_node_id, index}}, [&]() {
        return &this->for_any_uncached<ForeachGeometryElementZoneComputeContext>(
            parent, output_node_id, index);
      });
}

const ForeachGeometryElementZoneComputeContext &ComputeContextCache::
    for_foreach_geometry_element_zone(const ComputeContext *parent,
                                      const bNode &output_node,
                                      int index)
{
  return *foreach_geometry_element_zone_contexts_cache_.lookup_or_add_cb(
      std::pair{parent, std::pair{output_node.identifier, index}}, [&]() {
        return &this->for_any_uncached<ForeachGeometryElementZoneComputeContext>(
            parent, output_node, index);
      });
}

const EvaluateClosureComputeContext &ComputeContextCache::for_evaluate_closure(
    const ComputeContext *parent,
    const int32_t node_id,
    const bNodeTree *tree,
    const std::optional<nodes::ClosureSourceLocation> &closure_source_location)
{
  return *evaluate_closure_contexts_cache_.lookup_or_add_cb(std::pair{parent, node_id}, [&]() {
    return &this->for_any_uncached<EvaluateClosureComputeContext>(
        parent, node_id, tree, closure_source_location);
  });
}

}  // namespace blender::bke
