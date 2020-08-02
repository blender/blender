/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "simulation_collect_influences.hh"
#include "particle_function.hh"
#include "particle_mesh_emitter.hh"

#include "FN_attributes_ref.hh"
#include "FN_multi_function_network_evaluation.hh"
#include "FN_multi_function_network_optimization.hh"

#include "NOD_node_tree_multi_function.hh"

#include "DEG_depsgraph_query.h"

#include "BLI_hash.h"
#include "BLI_rand.hh"
#include "BLI_set.hh"

namespace blender::sim {

using fn::GVSpan;
using fn::MFContextBuilder;
using fn::MFDataType;
using fn::MFDummyNode;
using fn::MFFunctionNode;
using fn::MFInputSocket;
using fn::MFNetwork;
using fn::MFNetworkEvaluator;
using fn::MFNode;
using fn::MFOutputSocket;
using fn::MFParamsBuilder;
using fn::MFParamType;
using fn::MultiFunction;
using fn::VSpan;
using nodes::DerivedNodeTree;
using nodes::DInputSocket;
using nodes::DNode;
using nodes::DOutputSocket;
using nodes::DParentNode;
using nodes::MFNetworkTreeMap;
using nodes::NodeTreeRefMap;

struct DummyDataSources {
  Map<const MFOutputSocket *, std::string> particle_attributes;
  Set<const MFOutputSocket *> simulation_time;
  Set<const MFOutputSocket *> scene_time;
};

extern "C" {
void WM_clipboard_text_set(const char *buf, bool selection);
}

static std::string dnode_to_path(const DNode &dnode)
{
  std::string path;
  for (const DParentNode *parent = dnode.parent(); parent; parent = parent->parent()) {
    path = parent->node_ref().name() + "/" + path;
  }
  path = path + dnode.name();
  return path;
}

struct CollectContext : NonCopyable, NonMovable {
  SimulationInfluences &influences;
  RequiredStates &required_states;
  ResourceCollector &resources;
  MFNetworkTreeMap &network_map;
  MFNetwork &network;
  const DerivedNodeTree &tree;

  DummyDataSources data_sources;
  Span<const DNode *> particle_simulation_nodes;
  Map<const DNode *, std::string> node_paths;

  CollectContext(SimulationInfluences &influences,
                 RequiredStates &required_states,
                 ResourceCollector &resources,
                 MFNetworkTreeMap &network_map)
      : influences(influences),
        required_states(required_states),
        resources(resources),
        network_map(network_map),
        network(network_map.network()),
        tree(network_map.tree())
  {
    particle_simulation_nodes = tree.nodes_by_type("SimulationNodeParticleSimulation");
  }
};

static const ParticleAction *create_particle_action(CollectContext &context,
                                                    const DOutputSocket &dsocket,
                                                    Span<StringRefNull> particle_names);

static const ParticleAction *create_particle_action(CollectContext &context,
                                                    const DInputSocket &dsocket,
                                                    Span<StringRefNull> particle_names)
{
  BLI_assert(dsocket.bsocket()->type == SOCK_CONTROL_FLOW);
  if (dsocket.linked_sockets().size() != 1) {
    return nullptr;
  }
  return create_particle_action(context, *dsocket.linked_sockets()[0], particle_names);
}

static StringRefNull get_identifier(CollectContext &context, const DNode &dnode)
{
  return context.node_paths.lookup_or_add_cb(&dnode, [&]() { return dnode_to_path(dnode); });
}

static Span<const DNode *> nodes_by_type(CollectContext &context, StringRefNull idname)
{
  return context.tree.nodes_by_type(idname);
}

static Array<StringRefNull> find_linked_particle_simulations(CollectContext &context,
                                                             const DOutputSocket &output_socket)
{
  VectorSet<StringRefNull> names;
  for (const DInputSocket *target_socket : output_socket.linked_sockets()) {
    if (target_socket->node().idname() == "SimulationNodeParticleSimulation") {
      names.add(get_identifier(context, target_socket->node()));
    }
  }
  return names.as_span();
}

/* Returns true on success. */
static bool compute_global_inputs(MFNetworkTreeMap &network_map,
                                  ResourceCollector &resources,
                                  Span<const MFInputSocket *> sockets,
                                  MutableSpan<GMutableSpan> r_results)
{
  int amount = sockets.size();
  if (amount == 0) {
    return true;
  }

  if (network_map.network().have_dummy_or_unlinked_dependencies(sockets)) {
    return false;
  }

  MFNetworkEvaluator network_fn{{}, sockets};
  MFParamsBuilder params{network_fn, 1};
  for (int param_index : network_fn.param_indices()) {
    MFParamType param_type = network_fn.param_type(param_index);
    BLI_assert(param_type.category() == MFParamType::Category::SingleOutput); /* For now. */
    const CPPType &type = param_type.data_type().single_type();
    void *buffer = resources.linear_allocator().allocate(type.size(), type.alignment());
    resources.add(buffer, type.destruct_cb(), AT);
    GMutableSpan span{type, buffer, 1};
    r_results[param_index] = span;
    params.add_uninitialized_single_output(span);
  }
  MFContextBuilder context;
  network_fn.call(IndexRange(1), params, context);
  return true;
}

static std::optional<Array<std::string>> compute_global_string_inputs(
    MFNetworkTreeMap &network_map, Span<const MFInputSocket *> sockets)
{
  ResourceCollector local_resources;
  Array<GMutableSpan> computed_values(sockets.size(), NoInitialization());
  if (!compute_global_inputs(network_map, local_resources, sockets, computed_values)) {
    return {};
  }

  Array<std::string> strings(sockets.size());
  for (int i : sockets.index_range()) {
    strings[i] = std::move(computed_values[i].typed<std::string>()[0]);
  }
  return strings;
}

/**
 * This will find all the particle attribute input nodes. Then it will compute the attribute names
 * by evaluating the network (those names should not depend on per particle data). In the end,
 * input nodes that access the same attribute are combined.
 */
static void prepare_particle_attribute_nodes(CollectContext &context)
{
  Span<const DNode *> attribute_dnodes = nodes_by_type(context, "SimulationNodeParticleAttribute");

  Vector<MFInputSocket *> name_sockets;
  for (const DNode *dnode : attribute_dnodes) {
    MFInputSocket &name_socket = context.network_map.lookup_dummy(dnode->input(0));
    name_sockets.append(&name_socket);
  }

  std::optional<Array<std::string>> attribute_names = compute_global_string_inputs(
      context.network_map, name_sockets);
  if (!attribute_names.has_value()) {
    return;
  }

  MultiValueMap<std::pair<std::string, MFDataType>, MFNode *> attribute_nodes_by_name_and_type;
  for (int i : attribute_names->index_range()) {
    attribute_nodes_by_name_and_type.add(
        {(*attribute_names)[i], name_sockets[i]->node().output(0).data_type()},
        &name_sockets[i]->node());
  }

  Map<const MFOutputSocket *, std::string> attribute_inputs;
  for (auto item : attribute_nodes_by_name_and_type.items()) {
    StringRef attribute_name = item.key.first;
    MFDataType data_type = item.key.second;
    Span<MFNode *> nodes = item.value;

    MFOutputSocket &new_attribute_socket = context.network.add_input(
        "Attribute '" + attribute_name + "'", data_type);
    for (MFNode *node : nodes) {
      context.network.relink(node->output(0), new_attribute_socket);
    }
    context.network.remove(nodes);

    context.data_sources.particle_attributes.add_new(&new_attribute_socket, attribute_name);
  }
}

static void prepare_time_input_nodes(CollectContext &context)
{
  Span<const DNode *> time_input_dnodes = nodes_by_type(context, "SimulationNodeTime");
  Vector<const DNode *> simulation_time_inputs;
  Vector<const DNode *> scene_time_inputs;
  for (const DNode *dnode : time_input_dnodes) {
    NodeSimInputTimeType type = (NodeSimInputTimeType)dnode->node_ref().bnode()->custom1;
    switch (type) {
      case NODE_SIM_INPUT_SIMULATION_TIME: {
        simulation_time_inputs.append(dnode);
        break;
      }
      case NODE_SIM_INPUT_SCENE_TIME: {
        scene_time_inputs.append(dnode);
        break;
      }
    }
  }

  if (simulation_time_inputs.size() > 0) {
    MFOutputSocket &new_socket = context.network.add_input("Simulation Time",
                                                           MFDataType::ForSingle<float>());
    for (const DNode *dnode : simulation_time_inputs) {
      MFOutputSocket &old_socket = context.network_map.lookup_dummy(dnode->output(0));
      context.network.relink(old_socket, new_socket);
      context.network.remove(old_socket.node());
    }
    context.data_sources.simulation_time.add(&new_socket);
  }
  if (scene_time_inputs.size() > 0) {
    MFOutputSocket &new_socket = context.network.add_input("Scene Time",
                                                           MFDataType::ForSingle<float>());
    for (const DNode *dnode : scene_time_inputs) {
      MFOutputSocket &old_socket = context.network_map.lookup_dummy(dnode->output(0));
      context.network.relink(old_socket, new_socket);
      context.network.remove(old_socket.node());
    }
    context.data_sources.scene_time.add(&new_socket);
  }
}

class ParticleAttributeInput : public ParticleFunctionInput {
 private:
  std::string attribute_name_;
  const CPPType &attribute_type_;

 public:
  ParticleAttributeInput(std::string attribute_name, const CPPType &attribute_type)
      : attribute_name_(std::move(attribute_name)), attribute_type_(attribute_type)
  {
  }

  void add_input(ParticleFunctionInputContext &context,
                 MFParamsBuilder &params,
                 ResourceCollector &UNUSED(resources)) const override
  {
    std::optional<GSpan> span = context.particles.attributes.try_get(attribute_name_,
                                                                     attribute_type_);
    if (span.has_value()) {
      params.add_readonly_single_input(*span);
    }
    else {
      params.add_readonly_single_input(GVSpan::FromDefault(attribute_type_));
    }
  }
};

class SceneTimeInput : public ParticleFunctionInput {
  void add_input(ParticleFunctionInputContext &context,
                 MFParamsBuilder &params,
                 ResourceCollector &resources) const override
  {
    const float time = DEG_get_ctime(&context.solve_context.depsgraph);
    float *time_ptr = &resources.construct<float>(AT, time);
    params.add_readonly_single_input(time_ptr);
  }
};

class SimulationTimeInput : public ParticleFunctionInput {
  void add_input(ParticleFunctionInputContext &context,
                 MFParamsBuilder &params,
                 ResourceCollector &resources) const override
  {
    /* TODO: Vary this per particle. */
    const float time = context.solve_context.solve_interval.stop();
    float *time_ptr = &resources.construct<float>(AT, time);
    params.add_readonly_single_input(time_ptr);
  }
};

static const ParticleFunction *create_particle_function_for_inputs(
    CollectContext &context, Span<const MFInputSocket *> sockets_to_compute)
{
  BLI_assert(sockets_to_compute.size() >= 1);
  const MFNetwork &network = sockets_to_compute[0]->node().network();

  VectorSet<const MFOutputSocket *> dummy_deps;
  VectorSet<const MFInputSocket *> unlinked_input_deps;
  network.find_dependencies(sockets_to_compute, dummy_deps, unlinked_input_deps);
  BLI_assert(unlinked_input_deps.size() == 0);

  Vector<const ParticleFunctionInput *> per_particle_inputs;
  for (const MFOutputSocket *socket : dummy_deps) {
    if (context.data_sources.particle_attributes.contains(socket)) {
      const std::string *attribute_name = context.data_sources.particle_attributes.lookup_ptr(
          socket);
      if (attribute_name == nullptr) {
        return nullptr;
      }
      per_particle_inputs.append(&context.resources.construct<ParticleAttributeInput>(
          AT, *attribute_name, socket->data_type().single_type()));
    }
    else if (context.data_sources.scene_time.contains(socket)) {
      per_particle_inputs.append(&context.resources.construct<SceneTimeInput>(AT));
    }
    else if (context.data_sources.simulation_time.contains(socket)) {
      per_particle_inputs.append(&context.resources.construct<SimulationTimeInput>(AT));
    }
  }

  const MultiFunction &per_particle_fn = context.resources.construct<MFNetworkEvaluator>(
      AT, dummy_deps.as_span(), sockets_to_compute);

  Array<bool> output_is_global(sockets_to_compute.size(), false);

  const ParticleFunction &particle_fn = context.resources.construct<ParticleFunction>(
      AT,
      nullptr,
      &per_particle_fn,
      Span<const ParticleFunctionInput *>(),
      per_particle_inputs.as_span(),
      output_is_global.as_span());

  return &particle_fn;
}

static const ParticleFunction *create_particle_function_for_inputs(
    CollectContext &context, Span<const DInputSocket *> dsockets_to_compute)
{
  Vector<const MFInputSocket *> sockets_to_compute;
  for (const DInputSocket *dsocket : dsockets_to_compute) {
    const MFInputSocket &socket = context.network_map.lookup_dummy(*dsocket);
    sockets_to_compute.append(&socket);
  }
  return create_particle_function_for_inputs(context, sockets_to_compute);
}

class ParticleFunctionForce : public ParticleForce {
 private:
  const ParticleFunction &particle_fn_;

 public:
  ParticleFunctionForce(const ParticleFunction &particle_fn) : particle_fn_(particle_fn)
  {
  }

  void add_force(ParticleForceContext &context) const override
  {
    IndexMask mask = context.particles.index_mask;
    MutableSpan<float3> r_combined_force = context.force_dst;

    ParticleFunctionEvaluator evaluator{particle_fn_, context.solve_context, context.particles};
    evaluator.compute();
    VSpan<float3> forces = evaluator.get<float3>(0, "Force");

    for (int64_t i : mask) {
      r_combined_force[i] += forces[i];
    }
  }
};

static void create_forces_for_particle_simulation(CollectContext &context,
                                                  const DNode &simulation_node)
{
  Vector<const ParticleForce *> forces;
  for (const DOutputSocket *origin_socket : simulation_node.input(2, "Forces").linked_sockets()) {
    const DNode &origin_node = origin_socket->node();
    if (origin_node.idname() != "SimulationNodeForce") {
      continue;
    }

    const ParticleFunction *particle_fn = create_particle_function_for_inputs(
        context, {&origin_node.input(0, "Force")});

    if (particle_fn == nullptr) {
      continue;
    }

    const ParticleForce &force = context.resources.construct<ParticleFunctionForce>(AT,
                                                                                    *particle_fn);
    forces.append(&force);
  }

  StringRef particle_name = get_identifier(context, simulation_node);
  context.influences.particle_forces.add_multiple_as(particle_name, forces);
}

static void collect_forces(CollectContext &context)
{
  for (const DNode *dnode : context.particle_simulation_nodes) {
    create_forces_for_particle_simulation(context, *dnode);
  }
}

static ParticleEmitter *create_particle_emitter(CollectContext &context, const DNode &dnode)
{
  Array<StringRefNull> names = find_linked_particle_simulations(context, dnode.output(0));
  if (names.size() == 0) {
    return nullptr;
  }

  Array<const MFInputSocket *> input_sockets{2};
  for (int i : input_sockets.index_range()) {
    input_sockets[i] = &context.network_map.lookup_dummy(dnode.input(i));
  }

  if (context.network.have_dummy_or_unlinked_dependencies(input_sockets)) {
    return nullptr;
  }

  MultiFunction &inputs_fn = context.resources.construct<MFNetworkEvaluator>(
      AT, Span<const MFOutputSocket *>(), input_sockets.as_span());

  const ParticleAction *birth_action = create_particle_action(
      context, dnode.input(2, "Execute"), names);

  StringRefNull own_state_name = get_identifier(context, dnode);
  context.required_states.add(own_state_name, SIM_TYPE_NAME_PARTICLE_MESH_EMITTER);
  ParticleEmitter &emitter = context.resources.construct<ParticleMeshEmitter>(
      AT, own_state_name, names.as_span(), inputs_fn, birth_action);
  return &emitter;
}

static void collect_emitters(CollectContext &context)
{
  for (const DNode *dnode : nodes_by_type(context, "SimulationNodeParticleMeshEmitter")) {
    ParticleEmitter *emitter = create_particle_emitter(context, *dnode);
    if (emitter != nullptr) {
      context.influences.particle_emitters.append(emitter);
    }
  }
}

static void collect_birth_events(CollectContext &context)
{
  for (const DNode *event_dnode : nodes_by_type(context, "SimulationNodeParticleBirthEvent")) {
    const DInputSocket &execute_input = event_dnode->input(0);
    if (execute_input.linked_sockets().size() != 1) {
      continue;
    }

    Array<StringRefNull> particle_names = find_linked_particle_simulations(context,
                                                                           event_dnode->output(0));

    const DOutputSocket &execute_source = *execute_input.linked_sockets()[0];
    const ParticleAction *action = create_particle_action(context, execute_source, particle_names);
    if (action == nullptr) {
      continue;
    }

    for (StringRefNull particle_name : particle_names) {
      context.influences.particle_birth_actions.add_as(particle_name, action);
    }
  }
}

static void collect_time_step_events(CollectContext &context)
{
  for (const DNode *event_dnode : nodes_by_type(context, "SimulationNodeParticleTimeStepEvent")) {
    const DInputSocket &execute_input = event_dnode->input(0);
    Array<StringRefNull> particle_names = find_linked_particle_simulations(context,
                                                                           event_dnode->output(0));

    const ParticleAction *action = create_particle_action(context, execute_input, particle_names);
    if (action == nullptr) {
      continue;
    }

    NodeSimParticleTimeStepEventType type =
        (NodeSimParticleTimeStepEventType)event_dnode->node_ref().bnode()->custom1;
    if (type == NODE_PARTICLE_TIME_STEP_EVENT_BEGIN) {
      for (StringRefNull particle_name : particle_names) {
        context.influences.particle_time_step_begin_actions.add_as(particle_name, action);
      }
    }
    else {
      for (StringRefNull particle_name : particle_names) {
        context.influences.particle_time_step_end_actions.add_as(particle_name, action);
      }
    }
  }
}

class SequenceParticleAction : public ParticleAction {
 private:
  Vector<const ParticleAction *> actions_;

 public:
  SequenceParticleAction(Span<const ParticleAction *> actions) : actions_(std::move(actions))
  {
  }

  void execute(ParticleActionContext &context) const override
  {
    for (const ParticleAction *action : actions_) {
      action->execute(context);
    }
  }
};

class SetParticleAttributeAction : public ParticleAction {
 private:
  std::string attribute_name_;
  const CPPType &cpp_type_;
  const ParticleFunction &inputs_fn_;

 public:
  SetParticleAttributeAction(std::string attribute_name,
                             const CPPType &cpp_type,
                             const ParticleFunction &inputs_fn)
      : attribute_name_(std::move(attribute_name)), cpp_type_(cpp_type), inputs_fn_(inputs_fn)
  {
  }

  void execute(ParticleActionContext &context) const override
  {
    std::optional<GMutableSpan> attribute_array = context.particles.attributes.try_get(
        attribute_name_, cpp_type_);
    if (!attribute_array.has_value()) {
      return;
    }

    ParticleFunctionEvaluator evaluator{inputs_fn_, context.solve_context, context.particles};
    evaluator.compute();
    GVSpan values = evaluator.get(0);

    if (values.is_single_element()) {
      cpp_type_.fill_initialized_indices(
          values.as_single_element(), attribute_array->data(), context.particles.index_mask);
    }
    else {
      GSpan value_array = values.as_full_array();
      cpp_type_.copy_to_initialized_indices(
          value_array.data(), attribute_array->data(), context.particles.index_mask);
    }

    if (attribute_name_ == "Velocity") {
      context.particles.update_diffs_after_velocity_change();
    }
  }
};

static const ParticleAction *concatenate_actions(CollectContext &context,
                                                 Span<const ParticleAction *> actions)
{
  Vector<const ParticleAction *> non_null_actions;
  for (const ParticleAction *action : actions) {
    if (action != nullptr) {
      non_null_actions.append(action);
    }
  }
  if (non_null_actions.size() == 0) {
    return nullptr;
  }
  if (non_null_actions.size() == 1) {
    return non_null_actions[0];
  }
  return &context.resources.construct<SequenceParticleAction>(AT, std::move(non_null_actions));
}

static const ParticleAction *create_set_particle_attribute_action(
    CollectContext &context, const DOutputSocket &dsocket, Span<StringRefNull> particle_names)
{
  const DNode &dnode = dsocket.node();

  const ParticleAction *previous_action = create_particle_action(
      context, dnode.input(0), particle_names);

  MFInputSocket &name_socket = context.network_map.lookup_dummy(dnode.input(1));
  MFInputSocket &value_socket = name_socket.node().input(1);
  std::optional<Array<std::string>> names = compute_global_string_inputs(context.network_map,
                                                                         {&name_socket});
  if (!names.has_value()) {
    return previous_action;
  }

  std::string attribute_name = (*names)[0];
  if (attribute_name.empty()) {
    return previous_action;
  }
  const CPPType &attribute_type = value_socket.data_type().single_type();

  const ParticleFunction *inputs_fn = create_particle_function_for_inputs(context,
                                                                          {&value_socket});
  if (inputs_fn == nullptr) {
    return previous_action;
  }

  for (StringRef particle_name : particle_names) {
    context.influences.particle_attributes_builder.lookup_as(particle_name)
        ->add(attribute_name, attribute_type);
  }

  ParticleAction &this_action = context.resources.construct<SetParticleAttributeAction>(
      AT, attribute_name, attribute_type, *inputs_fn);

  return concatenate_actions(context, {previous_action, &this_action});
}

class ParticleConditionAction : public ParticleAction {
 private:
  const ParticleFunction &inputs_fn_;
  const ParticleAction *action_true_;
  const ParticleAction *action_false_;

 public:
  ParticleConditionAction(const ParticleFunction &inputs_fn,
                          const ParticleAction *action_true,
                          const ParticleAction *action_false)
      : inputs_fn_(inputs_fn), action_true_(action_true), action_false_(action_false)
  {
  }

  void execute(ParticleActionContext &context) const override
  {
    ParticleFunctionEvaluator evaluator{inputs_fn_, context.solve_context, context.particles};
    evaluator.compute();
    VSpan<bool> conditions = evaluator.get<bool>(0, "Condition");

    if (conditions.is_single_element()) {
      const bool condition = conditions.as_single_element();
      if (condition) {
        if (action_true_ != nullptr) {
          action_true_->execute(context);
        }
      }
      else {
        if (action_false_ != nullptr) {
          action_false_->execute(context);
        }
      }
    }
    else {
      Span<bool> conditions_array = conditions.as_full_array();

      Vector<int64_t> true_indices;
      Vector<int64_t> false_indices;
      for (int i : context.particles.index_mask) {
        if (conditions_array[i]) {
          true_indices.append(i);
        }
        else {
          false_indices.append(i);
        }
      }

      if (action_true_ != nullptr) {
        ParticleChunkContext chunk_context{context.particles.state,
                                           true_indices.as_span(),
                                           context.particles.attributes,
                                           context.particles.integration};
        ParticleActionContext action_context{context.solve_context, chunk_context};
        action_true_->execute(action_context);
      }
      if (action_false_ != nullptr) {
        ParticleChunkContext chunk_context{context.particles.state,
                                           false_indices.as_span(),
                                           context.particles.attributes,
                                           context.particles.integration};
        ParticleActionContext action_context{context.solve_context, chunk_context};
        action_false_->execute(action_context);
      }
    }
  }
};

static const ParticleAction *create_particle_condition_action(CollectContext &context,
                                                              const DOutputSocket &dsocket,
                                                              Span<StringRefNull> particle_names)
{
  const DNode &dnode = dsocket.node();

  const ParticleFunction *inputs_fn = create_particle_function_for_inputs(
      context, {&dnode.input(0, "Condition")});
  if (inputs_fn == nullptr) {
    return nullptr;
  }

  const ParticleAction *true_action = create_particle_action(
      context, dnode.input(1), particle_names);
  const ParticleAction *false_action = create_particle_action(
      context, dnode.input(2), particle_names);

  if (true_action == nullptr && false_action == nullptr) {
    return nullptr;
  }
  return &context.resources.construct<ParticleConditionAction>(
      AT, *inputs_fn, true_action, false_action);
}

class KillParticleAction : public ParticleAction {
 public:
  void execute(ParticleActionContext &context) const override
  {
    MutableSpan<int> dead_states = context.particles.attributes.get<int>("Dead");
    for (int i : context.particles.index_mask) {
      dead_states[i] = true;
    }
  }
};

static const ParticleAction *create_particle_action(CollectContext &context,
                                                    const DOutputSocket &dsocket,
                                                    Span<StringRefNull> particle_names)
{
  const DNode &dnode = dsocket.node();
  StringRef idname = dnode.idname();
  if (idname == "SimulationNodeSetParticleAttribute") {
    return create_set_particle_attribute_action(context, dsocket, particle_names);
  }
  if (idname == "SimulationNodeExecuteCondition") {
    return create_particle_condition_action(context, dsocket, particle_names);
  }
  if (idname == "SimulationNodeKillParticle") {
    return &context.resources.construct<KillParticleAction>(AT);
  }
  return nullptr;
}

static void initialize_particle_attribute_builders(CollectContext &context)
{
  for (const DNode *dnode : context.particle_simulation_nodes) {
    StringRef name = get_identifier(context, *dnode);
    AttributesInfoBuilder &attributes_builder = context.resources.construct<AttributesInfoBuilder>(
        AT);
    attributes_builder.add<float3>("Position", {0, 0, 0});
    attributes_builder.add<float3>("Velocity", {0, 0, 0});
    attributes_builder.add<int>("ID", 0);
    /* TODO: Use bool property, but need to add CD_PROP_BOOL first. */
    attributes_builder.add<int>("Dead", 0);
    /* TODO: Use uint32_t, but we don't have a corresponding custom property type. */
    attributes_builder.add<int>("Hash", 0);
    attributes_builder.add<float>("Birth Time", 0.0f);
    attributes_builder.add<float>("Radius", 0.02f);
    context.influences.particle_attributes_builder.add_new(name, &attributes_builder);
  }
}

static void optimize_function_network(CollectContext &context)
{
  fn::mf_network_optimization::constant_folding(context.network, context.resources);
  fn::mf_network_optimization::common_subnetwork_elimination(context.network);
  fn::mf_network_optimization::dead_node_removal(context.network);
  // WM_clipboard_text_set(network.to_dot().c_str(), false);
}

class AgeReachedEvent : public ParticleEvent {
 private:
  std::string attribute_name_;
  const ParticleFunction &inputs_fn_;
  const ParticleAction &action_;

 public:
  AgeReachedEvent(std::string attribute_name,
                  const ParticleFunction &inputs_fn,
                  const ParticleAction &action)
      : attribute_name_(std::move(attribute_name)), inputs_fn_(inputs_fn), action_(action)
  {
  }

  void filter(ParticleEventFilterContext &context) const override
  {
    Span<float> birth_times = context.particles.attributes.get<float>("Birth Time");
    std::optional<Span<int>> has_been_triggered = context.particles.attributes.try_get<int>(
        attribute_name_);
    if (!has_been_triggered.has_value()) {
      return;
    }

    ParticleFunctionEvaluator evaluator{inputs_fn_, context.solve_context, context.particles};
    evaluator.compute();
    VSpan<float> trigger_ages = evaluator.get<float>(0, "Age");

    const float end_time = context.particles.integration->end_time;
    for (int i : context.particles.index_mask) {
      if ((*has_been_triggered)[i]) {
        continue;
      }
      const float trigger_age = trigger_ages[i];
      const float birth_time = birth_times[i];
      const float trigger_time = birth_time + trigger_age;
      if (trigger_time > end_time) {
        continue;
      }

      const float duration = context.particles.integration->durations[i];
      TimeInterval interval(end_time - duration, duration);
      const float time_factor = interval.safe_factor_at_time(trigger_time);

      context.factor_dst[i] = std::max<float>(0.0f, time_factor);
    }
  }

  void execute(ParticleActionContext &context) const override
  {
    MutableSpan<int> has_been_triggered = context.particles.attributes.get<int>(attribute_name_);
    for (int i : context.particles.index_mask) {
      has_been_triggered[i] = 1;
    }
    action_.execute(context);
  }
};

static void collect_age_reached_events(CollectContext &context)
{
  for (const DNode *dnode : nodes_by_type(context, "SimulationNodeAgeReachedEvent")) {
    const DInputSocket &age_input = dnode->input(0, "Age");
    const DInputSocket &execute_input = dnode->input(1, "Execute");
    Array<StringRefNull> particle_names = find_linked_particle_simulations(context,
                                                                           dnode->output(0));
    const ParticleAction *action = create_particle_action(context, execute_input, particle_names);
    if (action == nullptr) {
      continue;
    }
    const ParticleFunction *inputs_fn = create_particle_function_for_inputs(context, {&age_input});
    if (inputs_fn == nullptr) {
      continue;
    }

    std::string attribute_name = get_identifier(context, *dnode);
    const ParticleEvent &event = context.resources.construct<AgeReachedEvent>(
        AT, attribute_name, *inputs_fn, *action);
    for (StringRefNull particle_name : particle_names) {
      const bool added_attribute = context.influences.particle_attributes_builder
                                       .lookup_as(particle_name)
                                       ->add<int>(attribute_name, 0);
      if (added_attribute) {
        context.influences.particle_events.add_as(particle_name, &event);
      }
    }
  }
}

void collect_simulation_influences(Simulation &simulation,
                                   ResourceCollector &resources,
                                   SimulationInfluences &r_influences,
                                   RequiredStates &r_required_states)
{
  NodeTreeRefMap tree_refs;
  const DerivedNodeTree tree{simulation.nodetree, tree_refs};

  MFNetwork &network = resources.construct<MFNetwork>(AT);
  MFNetworkTreeMap network_map = insert_node_tree_into_mf_network(network, tree, resources);

  CollectContext context{r_influences, r_required_states, resources, network_map};
  initialize_particle_attribute_builders(context);

  prepare_particle_attribute_nodes(context);
  prepare_time_input_nodes(context);

  collect_forces(context);
  collect_emitters(context);
  collect_birth_events(context);
  collect_time_step_events(context);
  collect_age_reached_events(context);

  optimize_function_network(context);

  for (const DNode *dnode : context.particle_simulation_nodes) {
    r_required_states.add(get_identifier(context, *dnode), SIM_TYPE_NAME_PARTICLE_SIMULATION);
  }
}

}  // namespace blender::sim
