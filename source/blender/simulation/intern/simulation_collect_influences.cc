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

namespace blender::sim {

struct DummyDataSources {
  Map<const fn::MFOutputSocket *, std::string> particle_attributes;
};

extern "C" {
void WM_clipboard_text_set(const char *buf, bool selection);
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

static Span<const nodes::DNode *> get_particle_simulation_nodes(const nodes::DerivedNodeTree &tree)
{
  return tree.nodes_by_type("SimulationNodeParticleSimulation");
}

/* Returns true on success. */
static bool compute_global_inputs(nodes::MFNetworkTreeMap &network_map,
                                  ResourceCollector &resources,
                                  Span<const fn::MFInputSocket *> sockets,
                                  MutableSpan<fn::GMutableSpan> r_results)
{
  int amount = sockets.size();
  if (amount == 0) {
    return true;
  }

  if (network_map.network().have_dummy_or_unlinked_dependencies(sockets)) {
    return false;
  }

  fn::MFNetworkEvaluator network_fn{{}, sockets};
  fn::MFParamsBuilder params{network_fn, 1};
  for (int param_index : network_fn.param_indices()) {
    fn::MFParamType param_type = network_fn.param_type(param_index);
    BLI_assert(param_type.category() == fn::MFParamType::Category::SingleOutput); /* For now. */
    const fn::CPPType &type = param_type.data_type().single_type();
    void *buffer = resources.linear_allocator().allocate(type.size(), type.alignment());
    resources.add(buffer, type.destruct_cb(), AT);
    fn::GMutableSpan span{type, buffer, 1};
    r_results[param_index] = span;
    params.add_uninitialized_single_output(span);
  }
  fn::MFContextBuilder context;
  network_fn.call(IndexRange(1), params, context);
  return true;
}

static std::optional<Array<std::string>> compute_global_string_inputs(
    nodes::MFNetworkTreeMap &network_map, Span<const fn::MFInputSocket *> sockets)
{
  ResourceCollector local_resources;
  Array<fn::GMutableSpan> computed_values(sockets.size(), NoInitialization());
  if (!compute_global_inputs(network_map, local_resources, sockets, computed_values)) {
    return {};
  }

  Array<std::string> strings(sockets.size());
  for (int i : sockets.index_range()) {
    strings[i] = std::move(computed_values[i].typed<std::string>()[0]);
  }
  return strings;
}

static void find_and_deduplicate_particle_attribute_nodes(nodes::MFNetworkTreeMap &network_map,
                                                          DummyDataSources &r_data_sources)
{
  fn::MFNetwork &network = network_map.network();
  const nodes::DerivedNodeTree &tree = network_map.tree();

  Span<const nodes::DNode *> attribute_dnodes = tree.nodes_by_type(
      "SimulationNodeParticleAttribute");

  Vector<fn::MFInputSocket *> name_sockets;
  for (const nodes::DNode *dnode : attribute_dnodes) {
    fn::MFInputSocket &name_socket = network_map.lookup_dummy(dnode->input(0));
    name_sockets.append(&name_socket);
  }

  std::optional<Array<std::string>> attribute_names = compute_global_string_inputs(network_map,
                                                                                   name_sockets);
  if (!attribute_names.has_value()) {
    return;
  }

  MultiValueMap<std::pair<std::string, fn::MFDataType>, fn::MFNode *>
      attribute_nodes_by_name_and_type;
  for (int i : attribute_names->index_range()) {
    attribute_nodes_by_name_and_type.add(
        {(*attribute_names)[i], name_sockets[i]->node().output(0).data_type()},
        &name_sockets[i]->node());
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

    r_data_sources.particle_attributes.add_new(&new_attribute_socket, attribute_name);
  }
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
    DummyDataSources &data_sources)
{
  BLI_assert(sockets_to_compute.size() >= 1);
  const fn::MFNetwork &network = sockets_to_compute[0]->node().network();

  VectorSet<const fn::MFOutputSocket *> dummy_deps;
  VectorSet<const fn::MFInputSocket *> unlinked_input_deps;
  network.find_dependencies(sockets_to_compute, dummy_deps, unlinked_input_deps);
  BLI_assert(unlinked_input_deps.size() == 0);

  Vector<const ParticleFunctionInput *> per_particle_inputs;
  for (const fn::MFOutputSocket *socket : dummy_deps) {
    const std::string *attribute_name = data_sources.particle_attributes.lookup_ptr(socket);
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

  void add_force(ParticleForceContext &context) const override
  {
    IndexMask mask = context.particle_chunk_context.index_mask;
    MutableSpan<float3> r_combined_force = context.force_dst;

    ParticleFunctionEvaluator evaluator{
        particle_fn_, context.solve_context, context.particle_chunk_context};
    evaluator.compute();
    fn::VSpan<float3> forces = evaluator.get<float3>(0, "Force");

    for (int64_t i : mask) {
      r_combined_force[i] += forces[i];
    }
  }
};

static void create_forces_for_particle_simulation(const nodes::DNode &simulation_node,
                                                  nodes::MFNetworkTreeMap &network_map,
                                                  ResourceCollector &resources,
                                                  DummyDataSources &data_sources,
                                                  SimulationInfluences &r_influences)
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
        {&force_socket}, resources, data_sources);

    if (particle_fn == nullptr) {
      continue;
    }

    const ParticleForce &force = resources.construct<ParticleFunctionForce>(AT, *particle_fn);
    forces.append(&force);
  }

  std::string particle_name = dnode_to_path(simulation_node);
  r_influences.particle_forces.add_multiple(std::move(particle_name), forces);
}

static void collect_forces(nodes::MFNetworkTreeMap &network_map,
                           ResourceCollector &resources,
                           DummyDataSources &data_sources,
                           SimulationInfluences &r_influences)
{
  for (const nodes::DNode *dnode : get_particle_simulation_nodes(network_map.tree())) {
    create_forces_for_particle_simulation(
        *dnode, network_map, resources, data_sources, r_influences);
  }
}

static Vector<const nodes::DNode *> find_linked_particle_simulations(
    const nodes::DOutputSocket &output_socket)
{
  Vector<const nodes::DNode *> simulation_nodes;
  for (const nodes::DInputSocket *target_socket : output_socket.linked_sockets()) {
    if (target_socket->node().idname() == "SimulationNodeParticleSimulation") {
      simulation_nodes.append(&target_socket->node());
    }
  }
  return simulation_nodes;
}

static ParticleEmitter *create_particle_emitter(const nodes::DNode &dnode,
                                                ResourceCollector &resources,
                                                nodes::MFNetworkTreeMap &network_map,
                                                RequiredStates &r_required_states)
{
  Vector<const nodes::DNode *> simulation_dnodes = find_linked_particle_simulations(
      dnode.output(0));
  if (simulation_dnodes.size() == 0) {
    return nullptr;
  }

  Array<std::string> names{simulation_dnodes.size()};
  for (int i : simulation_dnodes.index_range()) {
    names[i] = dnode_to_path(*simulation_dnodes[i]);
  }

  Array<const fn::MFInputSocket *> input_sockets{dnode.inputs().size()};
  for (int i : input_sockets.index_range()) {
    input_sockets[i] = &network_map.lookup_dummy(dnode.input(i));
  }

  if (network_map.network().have_dummy_or_unlinked_dependencies(input_sockets)) {
    return nullptr;
  }

  fn::MultiFunction &inputs_fn = resources.construct<fn::MFNetworkEvaluator>(
      AT, Span<const fn::MFOutputSocket *>(), input_sockets.as_span());

  std::string own_state_name = dnode_to_path(dnode);
  r_required_states.add(own_state_name, SIM_TYPE_NAME_PARTICLE_MESH_EMITTER);
  ParticleEmitter &emitter = resources.construct<ParticleMeshEmitter>(
      AT, std::move(own_state_name), std::move(names), inputs_fn);
  return &emitter;
}

static void collect_emitters(nodes::MFNetworkTreeMap &network_map,
                             ResourceCollector &resources,
                             SimulationInfluences &r_influences,
                             RequiredStates &r_required_states)
{
  for (const nodes::DNode *dnode :
       network_map.tree().nodes_by_type("SimulationNodeParticleMeshEmitter")) {
    ParticleEmitter *emitter = create_particle_emitter(
        *dnode, resources, network_map, r_required_states);
    if (emitter != nullptr) {
      r_influences.particle_emitters.append(emitter);
    }
  }
}

class RandomizeVelocityAction : public ParticleAction {
 public:
  void execute(ParticleActionContext &context) const override
  {
    MutableSpan<int> hashes = context.particle_chunk_context.attributes.get<int>("Hash");
    MutableSpan<float3> velocities = context.particle_chunk_context.attributes.get<float3>(
        "Velocity");
    for (int i : context.particle_chunk_context.index_mask) {
      const float x = BLI_hash_int_01((uint32_t)hashes[i] ^ 23423523u) - 0.5f;
      const float y = BLI_hash_int_01((uint32_t)hashes[i] ^ 76463521u) - 0.5f;
      const float z = BLI_hash_int_01((uint32_t)hashes[i] ^ 43523762u) - 0.5f;
      float3 vector{x, y, z};
      vector.normalize();
      velocities[i] += vector * 0.3;
    }
  }
};

static void collect_birth_events(nodes::MFNetworkTreeMap &network_map,
                                 ResourceCollector &resources,
                                 SimulationInfluences &r_influences)
{
  RandomizeVelocityAction &action = resources.construct<RandomizeVelocityAction>(AT);
  for (const nodes::DNode *dnode : get_particle_simulation_nodes(network_map.tree())) {
    std::string particle_name = dnode_to_path(*dnode);
    r_influences.particle_birth_actions.add_as(std::move(particle_name), &action);
  }
}

static void prepare_particle_attribute_builders(nodes::MFNetworkTreeMap &network_map,
                                                ResourceCollector &resources,
                                                SimulationInfluences &r_influences)
{
  for (const nodes::DNode *dnode : get_particle_simulation_nodes(network_map.tree())) {
    std::string name = dnode_to_path(*dnode);
    fn::AttributesInfoBuilder &builder = resources.construct<fn::AttributesInfoBuilder>(AT);
    builder.add<float3>("Position", {0, 0, 0});
    builder.add<float3>("Velocity", {0, 0, 0});
    builder.add<int>("ID", 0);
    /* TODO: Use bool property, but need to add CD_PROP_BOOL first. */
    builder.add<int>("Dead", 0);
    /* TODO: Use uint32_t, but we don't have a corresponding custom property type. */
    builder.add<int>("Hash", 0);
    builder.add<float>("Birth Time", 0.0f);
    r_influences.particle_attributes_builder.add_new(std::move(name), &builder);
  }
}

void collect_simulation_influences(Simulation &simulation,
                                   ResourceCollector &resources,
                                   SimulationInfluences &r_influences,
                                   RequiredStates &r_required_states)
{
  nodes::NodeTreeRefMap tree_refs;
  const nodes::DerivedNodeTree tree{simulation.nodetree, tree_refs};

  fn::MFNetwork &network = resources.construct<fn::MFNetwork>(AT);
  nodes::MFNetworkTreeMap network_map = insert_node_tree_into_mf_network(network, tree, resources);

  prepare_particle_attribute_builders(network_map, resources, r_influences);

  DummyDataSources data_sources;
  find_and_deduplicate_particle_attribute_nodes(network_map, data_sources);

  fn::mf_network_optimization::constant_folding(network, resources);
  fn::mf_network_optimization::common_subnetwork_elimination(network);
  fn::mf_network_optimization::dead_node_removal(network);
  // WM_clipboard_text_set(network.to_dot().c_str(), false);

  collect_forces(network_map, resources, data_sources, r_influences);
  collect_emitters(network_map, resources, r_influences, r_required_states);
  collect_birth_events(network_map, resources, r_influences);

  for (const nodes::DNode *dnode : get_particle_simulation_nodes(tree)) {
    r_required_states.add(dnode_to_path(*dnode), SIM_TYPE_NAME_PARTICLE_SIMULATION);
  }
}

}  // namespace blender::sim
