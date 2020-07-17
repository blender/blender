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

#include "SIM_particle_function.hh"
#include "SIM_simulation_update.hh"

#include "BKE_customdata.h"
#include "BKE_simulation.h"

#include "DNA_scene_types.h"
#include "DNA_simulation_types.h"

#include "DEG_depsgraph_query.h"

#include "BLI_array.hh"
#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_rand.h"
#include "BLI_vector.hh"

#include "NOD_node_tree_multi_function.hh"

#include "FN_attributes_ref.hh"
#include "FN_multi_function_network_evaluation.hh"
#include "FN_multi_function_network_optimization.hh"

#include "simulation_solver.hh"

extern "C" {
void WM_clipboard_text_set(const char *buf, bool selection);
}

namespace blender::sim {

static void copy_states_to_cow(Simulation *simulation_orig, Simulation *simulation_cow)
{
  BKE_simulation_state_remove_all(simulation_cow);
  simulation_cow->current_frame = simulation_orig->current_frame;

  LISTBASE_FOREACH (SimulationState *, state_orig, &simulation_orig->states) {
    switch ((eSimulationStateType)state_orig->type) {
      case SIM_STATE_TYPE_PARTICLES: {
        ParticleSimulationState *particle_state_orig = (ParticleSimulationState *)state_orig;
        ParticleSimulationState *particle_state_cow = (ParticleSimulationState *)
            BKE_simulation_state_add(simulation_cow, SIM_STATE_TYPE_PARTICLES, state_orig->name);
        particle_state_cow->tot_particles = particle_state_orig->tot_particles;
        CustomData_copy(&particle_state_orig->attributes,
                        &particle_state_cow->attributes,
                        CD_MASK_ALL,
                        CD_DUPLICATE,
                        particle_state_orig->tot_particles);
        break;
      }
    }
  }
}

static Map<const fn::MFOutputSocket *, std::string> deduplicate_attribute_nodes(
    fn::MFNetwork &network,
    nodes::MFNetworkTreeMap &network_map,
    const nodes::DerivedNodeTree &tree)
{
  Span<const nodes::DNode *> attribute_dnodes = tree.nodes_by_type(
      "SimulationNodeParticleAttribute");
  uint amount = attribute_dnodes.size();
  if (amount == 0) {
    return {};
  }

  Vector<fn::MFInputSocket *> name_sockets;
  for (const nodes::DNode *dnode : attribute_dnodes) {
    fn::MFInputSocket &name_socket = network_map.lookup_dummy(dnode->input(0));
    name_sockets.append(&name_socket);
  }

  fn::MFNetworkEvaluator network_fn{{}, name_sockets.as_span()};

  fn::MFParamsBuilder params{network_fn, 1};

  Array<std::string> attribute_names{amount, NoInitialization()};
  for (uint i : IndexRange(amount)) {
    params.add_uninitialized_single_output(
        fn::GMutableSpan(fn::CPPType::get<std::string>(), attribute_names.data() + i, 1));
  }

  fn::MFContextBuilder context;
  /* Todo: Check that the names don't depend on dummy nodes. */
  network_fn.call({0}, params, context);

  Map<std::pair<std::string, fn::MFDataType>, Vector<fn::MFNode *>>
      attribute_nodes_by_name_and_type;
  for (uint i : IndexRange(amount)) {
    attribute_nodes_by_name_and_type
        .lookup_or_add_default({attribute_names[i], name_sockets[i]->node().output(0).data_type()})
        .append(&name_sockets[i]->node());
  }

  Map<const fn::MFOutputSocket *, std::string> attribute_inputs;
  for (auto item : attribute_nodes_by_name_and_type.items()) {
    StringRef attribute_name = item.key.first;
    fn::MFDataType data_type = item.key.second;
    Span<fn::MFNode *> nodes = item.value;

    fn::MFOutputSocket &new_attribute_socket = network.add_input(
        "Attribute '" + attribute_name + "'", data_type);
    for (fn::MFNode *node : nodes) {
      network.relink(node->output(0), new_attribute_socket);
    }
    network.remove(nodes);

    attribute_inputs.add_new(&new_attribute_socket, attribute_name);
  }

  return attribute_inputs;
}

static std::string dnode_to_path(const nodes::DNode &dnode)
{
  std::string path;
  for (const nodes::DParentNode *parent = dnode.parent(); parent; parent = parent->parent()) {
    path = parent->node_ref().name() + "/" + path;
  }
  path = path + dnode.name();
  return path;
}

static void remove_unused_states(Simulation *simulation, const VectorSet<std::string> &state_names)
{
  LISTBASE_FOREACH_MUTABLE (SimulationState *, state, &simulation->states) {
    if (!state_names.contains(state->name)) {
      BKE_simulation_state_remove(simulation, state);
    }
  }
}

static void reset_states(Simulation *simulation)
{
  LISTBASE_FOREACH (SimulationState *, state, &simulation->states) {
    switch ((eSimulationStateType)state->type) {
      case SIM_STATE_TYPE_PARTICLES: {
        ParticleSimulationState *particle_state = (ParticleSimulationState *)state;
        CustomData_free(&particle_state->attributes, particle_state->tot_particles);
        particle_state->tot_particles = 0;
        break;
      }
    }
  }
}

static SimulationState *try_find_state_by_name(Simulation *simulation, StringRef name)
{
  LISTBASE_FOREACH (SimulationState *, state, &simulation->states) {
    if (state->name == name) {
      return state;
    }
  }
  return nullptr;
}

static void add_missing_particle_states(Simulation *simulation, Span<std::string> state_names)
{
  for (StringRefNull name : state_names) {
    SimulationState *state = try_find_state_by_name(simulation, name);
    if (state != nullptr) {
      BLI_assert(state->type == SIM_STATE_TYPE_PARTICLES);
      continue;
    }

    BKE_simulation_state_add(simulation, SIM_STATE_TYPE_PARTICLES, name.c_str());
  }
}

static void reinitialize_empty_simulation_states(Simulation *simulation,
                                                 const nodes::DerivedNodeTree &tree)
{
  VectorSet<std::string> state_names;
  for (const nodes::DNode *dnode : tree.nodes_by_type("SimulationNodeParticleSimulation")) {
    state_names.add(dnode_to_path(*dnode));
  }

  remove_unused_states(simulation, state_names);
  reset_states(simulation);
  add_missing_particle_states(simulation, state_names);
}

static void update_simulation_state_list(Simulation *simulation,
                                         const nodes::DerivedNodeTree &tree)
{
  VectorSet<std::string> state_names;
  for (const nodes::DNode *dnode : tree.nodes_by_type("SimulationNodeParticleSimulation")) {
    state_names.add(dnode_to_path(*dnode));
  }

  remove_unused_states(simulation, state_names);
  add_missing_particle_states(simulation, state_names);
}

class ParticleAttributeInput : public ParticleFunctionInput {
 private:
  std::string attribute_name_;
  const fn::CPPType &attribute_type_;

 public:
  ParticleAttributeInput(std::string attribute_name, const fn::CPPType &attribute_type)
      : attribute_name_(std::move(attribute_name)), attribute_type_(attribute_type)
  {
  }

  void add_input(fn::AttributesRef attributes,
                 fn::MFParamsBuilder &params,
                 ResourceCollector &UNUSED(resources)) const override
  {
    std::optional<fn::GSpan> span = attributes.try_get(attribute_name_, attribute_type_);
    if (span.has_value()) {
      params.add_readonly_single_input(*span);
    }
    else {
      params.add_readonly_single_input(fn::GVSpan::FromDefault(attribute_type_));
    }
  }
};

static const ParticleFunction *create_particle_function_for_inputs(
    Span<const fn::MFInputSocket *> sockets_to_compute,
    ResourceCollector &resources,
    const Map<const fn::MFOutputSocket *, std::string> &attribute_inputs)
{
  BLI_assert(sockets_to_compute.size() >= 1);
  const fn::MFNetwork &network = sockets_to_compute[0]->node().network();

  VectorSet<const fn::MFOutputSocket *> dummy_deps;
  VectorSet<const fn::MFInputSocket *> unlinked_input_deps;
  network.find_dependencies(sockets_to_compute, dummy_deps, unlinked_input_deps);
  BLI_assert(unlinked_input_deps.size() == 0);

  Vector<const ParticleFunctionInput *> per_particle_inputs;
  for (const fn::MFOutputSocket *socket : dummy_deps) {
    const std::string *attribute_name = attribute_inputs.lookup_ptr(socket);
    if (attribute_name == nullptr) {
      return nullptr;
    }
    per_particle_inputs.append(&resources.construct<ParticleAttributeInput>(
        AT, *attribute_name, socket->data_type().single_type()));
  }

  const fn::MultiFunction &per_particle_fn = resources.construct<fn::MFNetworkEvaluator>(
      AT, dummy_deps.as_span(), sockets_to_compute);

  Array<bool> output_is_global(sockets_to_compute.size(), false);

  const ParticleFunction &particle_fn = resources.construct<ParticleFunction>(
      AT,
      nullptr,
      &per_particle_fn,
      Span<const ParticleFunctionInput *>(),
      per_particle_inputs.as_span(),
      output_is_global.as_span());

  return &particle_fn;
}

class ParticleFunctionForce : public ParticleForce {
 private:
  const ParticleFunction &particle_fn_;

 public:
  ParticleFunctionForce(const ParticleFunction &particle_fn) : particle_fn_(particle_fn)
  {
  }

  void add_force(fn::AttributesRef attributes, MutableSpan<float3> r_combined_force) const override
  {
    IndexMask mask = IndexRange(attributes.size());
    ParticleFunctionEvaluator evaluator{particle_fn_, mask, attributes};
    evaluator.compute();
    fn::VSpan<float3> forces = evaluator.get<float3>(0, "Force");
    for (uint i : mask) {
      r_combined_force[i] += forces[i];
    }
  }
};

static Vector<const ParticleForce *> create_forces_for_particle_simulation(
    const nodes::DNode &simulation_node,
    nodes::MFNetworkTreeMap &network_map,
    ResourceCollector &resources,
    const Map<const fn::MFOutputSocket *, std::string> &attribute_inputs)
{
  Vector<const ParticleForce *> forces;
  for (const nodes::DOutputSocket *origin_socket :
       simulation_node.input(2, "Forces").linked_sockets()) {
    const nodes::DNode &origin_node = origin_socket->node();
    if (origin_node.idname() != "SimulationNodeForce") {
      continue;
    }

    const fn::MFInputSocket &force_socket = network_map.lookup_dummy(
        origin_node.input(0, "Force"));

    const ParticleFunction *particle_fn = create_particle_function_for_inputs(
        {&force_socket}, resources, attribute_inputs);

    if (particle_fn == nullptr) {
      continue;
    }

    const ParticleForce &force = resources.construct<ParticleFunctionForce>(AT, *particle_fn);
    forces.append(&force);
  }
  return forces;
}

static void collect_forces(Simulation &simulation,
                           nodes::MFNetworkTreeMap &network_map,
                           ResourceCollector &resources,
                           const Map<const fn::MFOutputSocket *, std::string> &attribute_inputs,
                           SimulationInfluences &r_influences)
{
  for (const nodes::DNode *dnode :
       network_map.tree().nodes_by_type("SimulationNodeParticleSimulation")) {
    std::string name = dnode_to_path(*dnode);
    Vector<const ParticleForce *> forces = create_forces_for_particle_simulation(
        *dnode, network_map, resources, attribute_inputs);
    ParticleSimulationState *state = (ParticleSimulationState *)try_find_state_by_name(&simulation,
                                                                                       name);
    r_influences.particle_forces.add_new(state, std::move(forces));
  }
}

void update_simulation_in_depsgraph(Depsgraph *depsgraph,
                                    Scene *scene_cow,
                                    Simulation *simulation_cow)
{
  int current_frame = scene_cow->r.cfra;
  if (simulation_cow->current_frame == current_frame) {
    return;
  }

  /* Below we modify the original state/cache. Only the active depsgraph is allowed to do that. */
  if (!DEG_is_active(depsgraph)) {
    return;
  }

  Simulation *simulation_orig = (Simulation *)DEG_get_original_id(&simulation_cow->id);

  nodes::NodeTreeRefMap tree_refs;
  /* TODO: Use simulation_cow, but need to add depsgraph relations before that. */
  const nodes::DerivedNodeTree tree{simulation_orig->nodetree, tree_refs};
  fn::MFNetwork network;
  ResourceCollector resources;
  nodes::MFNetworkTreeMap network_map = insert_node_tree_into_mf_network(network, tree, resources);
  Map<const fn::MFOutputSocket *, std::string> attribute_inputs = deduplicate_attribute_nodes(
      network, network_map, tree);
  fn::mf_network_optimization::constant_folding(network, resources);
  fn::mf_network_optimization::common_subnetwork_elimination(network);
  fn::mf_network_optimization::dead_node_removal(network);
  // WM_clipboard_text_set(network.to_dot().c_str(), false);

  SimulationInfluences simulation_influences;
  collect_forces(
      *simulation_orig, network_map, resources, attribute_inputs, simulation_influences);

  if (current_frame == 1) {
    reinitialize_empty_simulation_states(simulation_orig, tree);

    initialize_simulation_states(*simulation_orig, *depsgraph, simulation_influences);
    simulation_orig->current_frame = 1;

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
  else if (current_frame == simulation_orig->current_frame + 1) {
    update_simulation_state_list(simulation_orig, tree);

    float time_step = 1.0f / 24.0f;
    solve_simulation_time_step(*simulation_orig, *depsgraph, simulation_influences, time_step);
    simulation_orig->current_frame = current_frame;

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
}

}  // namespace blender::sim
