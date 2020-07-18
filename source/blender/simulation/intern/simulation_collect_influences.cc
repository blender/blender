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
#include "SIM_particle_function.hh"

#include "FN_attributes_ref.hh"
#include "FN_multi_function_network_evaluation.hh"
#include "FN_multi_function_network_optimization.hh"

#include "NOD_node_tree_multi_function.hh"

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

static std::optional<Array<std::string>> compute_global_string_inputs(
    nodes::MFNetworkTreeMap &network_map, Span<const fn::MFInputSocket *> sockets)
{
  uint amount = sockets.size();
  if (amount == 0) {
    return Array<std::string>();
  }

  if (network_map.network().have_dummy_or_unlinked_dependencies(sockets)) {
    return {};
  }

  fn::MFNetworkEvaluator network_fn{{}, sockets};

  fn::MFParamsBuilder params{network_fn, 1};

  Array<std::string> strings(amount, NoInitialization());
  for (uint i : IndexRange(amount)) {
    params.add_uninitialized_single_output(
        fn::GMutableSpan(fn::CPPType::get<std::string>(), strings.data() + i, 1));
  }

  fn::MFContextBuilder context;
  network_fn.call({0}, params, context);

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

  Map<std::pair<std::string, fn::MFDataType>, Vector<fn::MFNode *>>
      attribute_nodes_by_name_and_type;
  for (uint i : attribute_names->index_range()) {
    attribute_nodes_by_name_and_type
        .lookup_or_add_default(
            {(*attribute_names)[i], name_sockets[i]->node().output(0).data_type()})
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
    DummyDataSources &data_sources)
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
  return forces;
}

static void collect_forces(nodes::MFNetworkTreeMap &network_map,
                           ResourceCollector &resources,
                           DummyDataSources &data_sources,
                           SimulationInfluences &r_influences)
{
  for (const nodes::DNode *dnode :
       network_map.tree().nodes_by_type("SimulationNodeParticleSimulation")) {
    std::string name = dnode_to_path(*dnode);
    Vector<const ParticleForce *> forces = create_forces_for_particle_simulation(
        *dnode, network_map, resources, data_sources);
    r_influences.particle_forces.add_new(std::move(name), std::move(forces));
  }
}

void collect_simulation_influences(Simulation &simulation,
                                   ResourceCollector &resources,
                                   SimulationInfluences &r_influences,
                                   SimulationStatesInfo &r_states_info)
{
  nodes::NodeTreeRefMap tree_refs;
  const nodes::DerivedNodeTree tree{simulation.nodetree, tree_refs};

  fn::MFNetwork &network = resources.construct<fn::MFNetwork>(AT);
  nodes::MFNetworkTreeMap network_map = insert_node_tree_into_mf_network(network, tree, resources);

  DummyDataSources data_sources;
  find_and_deduplicate_particle_attribute_nodes(network_map, data_sources);

  fn::mf_network_optimization::constant_folding(network, resources);
  fn::mf_network_optimization::common_subnetwork_elimination(network);
  fn::mf_network_optimization::dead_node_removal(network);
  // WM_clipboard_text_set(network.to_dot().c_str(), false);

  collect_forces(network_map, resources, data_sources, r_influences);

  for (const nodes::DNode *dnode : tree.nodes_by_type("SimulationNodeParticleSimulation")) {
    r_states_info.particle_simulation_names.add(dnode_to_path(*dnode));
  }
}

}  // namespace blender::sim
