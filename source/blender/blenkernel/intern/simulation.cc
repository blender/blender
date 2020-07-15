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

/** \file
 * \ingroup bke
 */

#include <iostream>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_defaults.h"
#include "DNA_scene_types.h"
#include "DNA_simulation_types.h"

#include "BLI_compiler_compat.h"
#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_customdata.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_node_tree_multi_function.hh"
#include "BKE_pointcache.h"
#include "BKE_simulation.h"

#include "NOD_simulation.h"

#include "BLT_translation.h"

#include "FN_attributes_ref.hh"
#include "FN_cpp_types.hh"
#include "FN_multi_function_network_evaluation.hh"
#include "FN_multi_function_network_optimization.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

extern "C" {
void WM_clipboard_text_set(const char *buf, bool selection);
}

static void simulation_init_data(ID *id)
{
  Simulation *simulation = (Simulation *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(simulation, id));

  MEMCPY_STRUCT_AFTER(simulation, DNA_struct_default_get(Simulation), id);

  bNodeTree *ntree = ntreeAddTree(nullptr, "Simulation Nodetree", ntreeType_Simulation->idname);
  simulation->nodetree = ntree;
}

static void simulation_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Simulation *simulation_dst = (Simulation *)id_dst;
  Simulation *simulation_src = (Simulation *)id_src;

  /* We always need allocation of our private ID data. */
  const int flag_private_id_data = flag & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (simulation_src->nodetree) {
    BKE_id_copy_ex(bmain,
                   (ID *)simulation_src->nodetree,
                   (ID **)&simulation_dst->nodetree,
                   flag_private_id_data);
  }

  BLI_listbase_clear(&simulation_dst->states);
}

static void free_simulation_state_head(SimulationState *state)
{
  MEM_freeN(state->name);
}

static void free_particle_simulation_state(ParticleSimulationState *state)
{
  free_simulation_state_head(&state->head);
  CustomData_free(&state->attributes, state->tot_particles);
  BKE_ptcache_free_list(&state->ptcaches);
  MEM_freeN(state);
}

static void simulation_free_data(ID *id)
{
  Simulation *simulation = (Simulation *)id;

  BKE_animdata_free(&simulation->id, false);

  if (simulation->nodetree) {
    ntreeFreeEmbeddedTree(simulation->nodetree);
    MEM_freeN(simulation->nodetree);
    simulation->nodetree = nullptr;
  }

  LISTBASE_FOREACH_MUTABLE (SimulationState *, state, &simulation->states) {
    switch ((eSimulationStateType)state->type) {
      case SIM_STATE_TYPE_PARTICLES: {
        free_particle_simulation_state((ParticleSimulationState *)state);
        break;
      }
    }
  }
}

static void simulation_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Simulation *simulation = (Simulation *)id;
  if (simulation->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_library_foreach_ID_embedded(data, (ID **)&simulation->nodetree);
  }
}

IDTypeInfo IDType_ID_SIM = {
    /* id_code */ ID_SIM,
    /* id_filter */ FILTER_ID_SIM,
    /* main_listbase_index */ INDEX_ID_SIM,
    /* struct_size */ sizeof(Simulation),
    /* name */ "Simulation",
    /* name_plural */ "simulations",
    /* translation_context */ BLT_I18NCONTEXT_ID_SIMULATION,
    /* flags */ 0,

    /* init_data */ simulation_init_data,
    /* copy_data */ simulation_copy_data,
    /* free_data */ simulation_free_data,
    /* make_local */ nullptr,
    /* foreach_id */ simulation_foreach_id,
};

void *BKE_simulation_add(Main *bmain, const char *name)
{
  Simulation *simulation = (Simulation *)BKE_libblock_alloc(bmain, ID_SIM, name, 0);

  simulation_init_data(&simulation->id);

  return simulation;
}

namespace blender::bke {

static void ensure_attributes_exist(ParticleSimulationState *state)
{
  if (CustomData_get_layer_named(&state->attributes, CD_PROP_FLOAT3, "Position") == nullptr) {
    CustomData_add_layer_named(
        &state->attributes, CD_PROP_FLOAT3, CD_CALLOC, nullptr, state->tot_particles, "Position");
  }
  if (CustomData_get_layer_named(&state->attributes, CD_PROP_FLOAT3, "Velocity") == nullptr) {
    CustomData_add_layer_named(
        &state->attributes, CD_PROP_FLOAT3, CD_CALLOC, nullptr, state->tot_particles, "Velocity");
  }
  if (CustomData_get_layer_named(&state->attributes, CD_PROP_INT32, "ID") == nullptr) {
    CustomData_add_layer_named(
        &state->attributes, CD_PROP_INT32, CD_CALLOC, nullptr, state->tot_particles, "ID");
  }
}

static void copy_states_to_cow(Simulation *simulation_orig, Simulation *simulation_cow)
{
  LISTBASE_FOREACH_MUTABLE (SimulationState *, state_cow, &simulation_cow->states) {
    switch ((eSimulationStateType)state_cow->type) {
      case SIM_STATE_TYPE_PARTICLES: {
        BLI_remlink(&simulation_cow->states, state_cow);
        free_particle_simulation_state((ParticleSimulationState *)state_cow);
        break;
      }
    }
  }
  simulation_cow->current_frame = simulation_orig->current_frame;

  LISTBASE_FOREACH (SimulationState *, state_orig, &simulation_orig->states) {
    switch ((eSimulationStateType)state_orig->type) {
      case SIM_STATE_TYPE_PARTICLES: {
        ParticleSimulationState *particle_state_orig = (ParticleSimulationState *)state_orig;
        ParticleSimulationState *particle_state_cow = (ParticleSimulationState *)MEM_callocN(
            sizeof(*particle_state_cow), AT);
        particle_state_cow->tot_particles = particle_state_orig->tot_particles;
        particle_state_cow->head.name = BLI_strdup(state_orig->name);
        CustomData_copy(&particle_state_orig->attributes,
                        &particle_state_cow->attributes,
                        CD_MASK_ALL,
                        CD_DUPLICATE,
                        particle_state_orig->tot_particles);
        BLI_addtail(&simulation_cow->states, particle_state_cow);
        break;
      }
    }
  }
}

static Map<const fn::MFOutputSocket *, std::string> deduplicate_attribute_nodes(
    fn::MFNetwork &network, MFNetworkTreeMap &network_map, const DerivedNodeTree &tree)
{
  Span<const DNode *> attribute_dnodes = tree.nodes_by_type("SimulationNodeParticleAttribute");
  uint amount = attribute_dnodes.size();
  if (amount == 0) {
    return {};
  }

  Vector<fn::MFInputSocket *> name_sockets;
  for (const DNode *dnode : attribute_dnodes) {
    fn::MFInputSocket &name_socket = network_map.lookup_dummy(dnode->input(0));
    name_sockets.append(&name_socket);
  }

  fn::MFNetworkEvaluator network_fn{{}, name_sockets.as_span()};

  fn::MFParamsBuilder params{network_fn, 1};

  Array<std::string> attribute_names{amount, NoInitialization()};
  for (uint i : IndexRange(amount)) {
    params.add_uninitialized_single_output(
        fn::GMutableSpan(fn::CPPType_string, attribute_names.data() + i, 1));
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

class CustomDataAttributesRef {
 private:
  Vector<void *> buffers_;
  uint size_;
  std::unique_ptr<fn::AttributesInfo> info_;

 public:
  CustomDataAttributesRef(CustomData &custom_data, uint size)
  {
    fn::AttributesInfoBuilder builder;
    for (const CustomDataLayer &layer : Span(custom_data.layers, custom_data.totlayer)) {
      buffers_.append(layer.data);
      switch (layer.type) {
        case CD_PROP_INT32: {
          builder.add<int32_t>(layer.name, 0);
          break;
        }
        case CD_PROP_FLOAT3: {
          builder.add<float3>(layer.name, {0, 0, 0});
          break;
        }
      }
    }
    info_ = std::make_unique<fn::AttributesInfo>(builder);
    size_ = size;
  }

  operator fn::MutableAttributesRef()
  {
    return fn::MutableAttributesRef(*info_, buffers_, size_);
  }

  operator fn::AttributesRef() const
  {
    return fn::AttributesRef(*info_, buffers_, size_);
  }
};

static std::string dnode_to_path(const DNode &dnode)
{
  std::string path;
  for (const DParentNode *parent = dnode.parent(); parent; parent = parent->parent()) {
    path = parent->node_ref().name() + "/" + path;
  }
  path = path + dnode.name();
  return path;
}

static void remove_unused_states(Simulation *simulation, const VectorSet<std::string> &state_names)
{
  LISTBASE_FOREACH_MUTABLE (SimulationState *, state, &simulation->states) {
    if (!state_names.contains(state->name)) {
      BLI_remlink(&simulation->states, state);
      free_particle_simulation_state((ParticleSimulationState *)state);
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

    ParticleSimulationState *particle_state = (ParticleSimulationState *)MEM_callocN(
        sizeof(*particle_state), AT);
    particle_state->head.type = SIM_STATE_TYPE_PARTICLES;
    particle_state->head.name = BLI_strdup(name.data());
    CustomData_reset(&particle_state->attributes);
    particle_state->point_cache = BKE_ptcache_add(&particle_state->ptcaches);
    BLI_addtail(&simulation->states, particle_state);
  }
}

static void reinitialize_empty_simulation_states(Simulation *simulation,
                                                 const DerivedNodeTree &tree)
{
  VectorSet<std::string> state_names;
  for (const DNode *dnode : tree.nodes_by_type("SimulationNodeParticleSimulation")) {
    state_names.add(dnode_to_path(*dnode));
  }

  remove_unused_states(simulation, state_names);
  reset_states(simulation);
  add_missing_particle_states(simulation, state_names);
}

static void update_simulation_state_list(Simulation *simulation, const DerivedNodeTree &tree)
{
  VectorSet<std::string> state_names;
  for (const DNode *dnode : tree.nodes_by_type("SimulationNodeParticleSimulation")) {
    state_names.add(dnode_to_path(*dnode));
  }

  remove_unused_states(simulation, state_names);
  add_missing_particle_states(simulation, state_names);
}

class ParticleFunctionInput {
 public:
  virtual ~ParticleFunctionInput() = default;
  virtual void add_input(fn::AttributesRef attributes,
                         fn::MFParamsBuilder &params,
                         ResourceCollector &resources) const = 0;
};

class ParticleFunction {
 private:
  const fn::MultiFunction *global_fn_;
  const fn::MultiFunction *per_particle_fn_;
  Array<const ParticleFunctionInput *> global_inputs_;
  Array<const ParticleFunctionInput *> per_particle_inputs_;
  Array<bool> output_is_global_;
  Vector<uint> global_output_indices_;
  Vector<uint> per_particle_output_indices_;
  Vector<fn::MFDataType> output_types_;
  Vector<StringRefNull> output_names_;

  friend class ParticleFunctionEvaluator;

 public:
  ParticleFunction(const fn::MultiFunction *global_fn,
                   const fn::MultiFunction *per_particle_fn,
                   Span<const ParticleFunctionInput *> global_inputs,
                   Span<const ParticleFunctionInput *> per_particle_inputs,
                   Span<bool> output_is_global)
      : global_fn_(global_fn),
        per_particle_fn_(per_particle_fn),
        global_inputs_(global_inputs),
        per_particle_inputs_(per_particle_inputs),
        output_is_global_(output_is_global)
  {
    for (uint i : output_is_global_.index_range()) {
      if (output_is_global_[i]) {
        uint param_index = global_inputs_.size() + global_output_indices_.size();
        fn::MFParamType param_type = global_fn_->param_type(param_index);
        BLI_assert(param_type.is_output());
        output_types_.append(param_type.data_type());
        output_names_.append(global_fn_->param_name(param_index));
        global_output_indices_.append(i);
      }
      else {
        uint param_index = per_particle_inputs_.size() + per_particle_output_indices_.size();
        fn::MFParamType param_type = per_particle_fn_->param_type(param_index);
        BLI_assert(param_type.is_output());
        output_types_.append(param_type.data_type());
        output_names_.append(per_particle_fn_->param_name(param_index));
        per_particle_output_indices_.append(i);
      }
    }
  }
};

class ParticleFunctionEvaluator {
 private:
  ResourceCollector resources_;
  const ParticleFunction &particle_fn_;
  IndexMask mask_;
  fn::MFContextBuilder global_context_;
  fn::MFContextBuilder per_particle_context_;
  fn::AttributesRef particle_attributes_;
  Vector<void *> outputs_;
  bool is_computed_ = false;

 public:
  ParticleFunctionEvaluator(const ParticleFunction &particle_fn,
                            IndexMask mask,
                            fn::AttributesRef particle_attributes)
      : particle_fn_(particle_fn),
        mask_(mask),
        particle_attributes_(particle_attributes),
        outputs_(particle_fn_.output_types_.size(), nullptr)
  {
  }

  ~ParticleFunctionEvaluator()
  {
    for (uint output_index : outputs_.index_range()) {
      void *buffer = outputs_[output_index];
      fn::MFDataType data_type = particle_fn_.output_types_[output_index];
      BLI_assert(data_type.is_single()); /* For now. */
      const fn::CPPType &type = data_type.single_type();

      if (particle_fn_.output_is_global_[output_index]) {
        type.destruct(buffer);
      }
      else {
        type.destruct_indices(outputs_[0], mask_);
      }
    }
  }

  void compute()
  {
    BLI_assert(!is_computed_);
    this->compute_globals();
    this->compute_per_particle();
    is_computed_ = true;
  }

  template<typename T> fn::VSpan<T> get(uint output_index, StringRef expected_name) const
  {
    return this->get(output_index, expected_name).typed<T>();
  }

  fn::GVSpan get(uint output_index, StringRef expected_name) const
  {
#ifdef DEBUG
    StringRef real_name = particle_fn_.output_names_[output_index];
    BLI_assert(expected_name == real_name);
    BLI_assert(is_computed_);
#endif
    UNUSED_VARS_NDEBUG(expected_name);
    const void *buffer = outputs_[output_index];
    const fn::CPPType &type = particle_fn_.output_types_[output_index].single_type();
    if (particle_fn_.output_is_global_[output_index]) {
      return fn::GVSpan::FromSingleWithMaxSize(type, buffer);
    }
    else {
      return fn::GVSpan(fn::GSpan(type, buffer, mask_.min_array_size()));
    }
  }

 private:
  void compute_globals()
  {
    if (particle_fn_.global_fn_ == nullptr) {
      return;
    }

    fn::MFParamsBuilder params(*particle_fn_.global_fn_, mask_.min_array_size());

    /* Add input parameters. */
    for (const ParticleFunctionInput *input : particle_fn_.global_inputs_) {
      input->add_input(particle_attributes_, params, resources_);
    }

    /* Add output parameters. */
    for (uint output_index : particle_fn_.global_output_indices_) {
      fn::MFDataType data_type = particle_fn_.output_types_[output_index];
      BLI_assert(data_type.is_single()); /* For now. */

      const fn::CPPType &type = data_type.single_type();
      void *buffer = resources_.linear_allocator().allocate(type.size(), type.alignment());
      params.add_uninitialized_single_output(fn::GMutableSpan(type, buffer, 1));
      outputs_[output_index] = buffer;
    }

    particle_fn_.global_fn_->call({0}, params, global_context_);
  }

  void compute_per_particle()
  {
    if (particle_fn_.per_particle_fn_ == nullptr) {
      return;
    }

    fn::MFParamsBuilder params(*particle_fn_.per_particle_fn_, mask_.min_array_size());

    /* Add input parameters. */
    for (const ParticleFunctionInput *input : particle_fn_.per_particle_inputs_) {
      input->add_input(particle_attributes_, params, resources_);
    }

    /* Add output parameters. */
    for (uint output_index : particle_fn_.per_particle_output_indices_) {
      fn::MFDataType data_type = particle_fn_.output_types_[output_index];
      BLI_assert(data_type.is_single()); /* For now. */

      const fn::CPPType &type = data_type.single_type();
      void *buffer = resources_.linear_allocator().allocate(type.size() * mask_.min_array_size(),
                                                            type.alignment());
      params.add_uninitialized_single_output(
          fn::GMutableSpan(type, buffer, mask_.min_array_size()));
      outputs_[output_index] = buffer;
    }

    particle_fn_.per_particle_fn_->call(mask_, params, global_context_);
  }
};

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
    StringRef attribute_name = attribute_inputs.lookup(socket);
    per_particle_inputs.append(&resources.construct<ParticleAttributeInput>(
        AT, attribute_name, socket->data_type().single_type()));
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

class ParticleForce {
 public:
  virtual ~ParticleForce() = default;
  virtual void add_force(fn::AttributesRef attributes,
                         MutableSpan<float3> r_combined_force) const = 0;
};

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
    const DNode &simulation_node,
    MFNetworkTreeMap &network_map,
    ResourceCollector &resources,
    const Map<const fn::MFOutputSocket *, std::string> &attribute_inputs)
{
  Vector<const ParticleForce *> forces;
  for (const DOutputSocket *origin_socket : simulation_node.input(2, "Forces").linked_sockets()) {
    const DNode &origin_node = origin_socket->node();
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

static Map<std::string, Vector<const ParticleForce *>> collect_forces(
    MFNetworkTreeMap &network_map,
    ResourceCollector &resources,
    const Map<const fn::MFOutputSocket *, std::string> &attribute_inputs)
{
  Map<std::string, Vector<const ParticleForce *>> forces_by_simulation;
  for (const DNode *dnode : network_map.tree().nodes_by_type("SimulationNodeParticleSimulation")) {
    std::string name = dnode_to_path(*dnode);
    Vector<const ParticleForce *> forces = create_forces_for_particle_simulation(
        *dnode, network_map, resources, attribute_inputs);
    forces_by_simulation.add_new(std::move(name), std::move(forces));
  }
  return forces_by_simulation;
}

static void simulation_data_update(Depsgraph *depsgraph, Scene *scene, Simulation *simulation_cow)
{
  int current_frame = scene->r.cfra;
  if (simulation_cow->current_frame == current_frame) {
    return;
  }

  /* Below we modify the original state/cache. Only the active depsgraph is allowed to do that. */
  if (!DEG_is_active(depsgraph)) {
    return;
  }

  Simulation *simulation_orig = (Simulation *)DEG_get_original_id(&simulation_cow->id);

  NodeTreeRefMap tree_refs;
  /* TODO: Use simulation_cow, but need to add depsgraph relations before that. */
  const DerivedNodeTree tree{simulation_orig->nodetree, tree_refs};
  fn::MFNetwork network;
  ResourceCollector resources;
  MFNetworkTreeMap network_map = insert_node_tree_into_mf_network(network, tree, resources);
  // WM_clipboard_text_set(tree.to_dot().c_str(), false);
  Map<const fn::MFOutputSocket *, std::string> attribute_inputs = deduplicate_attribute_nodes(
      network, network_map, tree);
  fn::mf_network_optimization::constant_folding(network, resources);
  fn::mf_network_optimization::common_subnetwork_elimination(network);
  fn::mf_network_optimization::dead_node_removal(network);

  Map<std::string, Vector<const ParticleForce *>> forces_by_simulation = collect_forces(
      network_map, resources, attribute_inputs);

  if (current_frame == 1) {
    reinitialize_empty_simulation_states(simulation_orig, tree);

    RNG *rng = BLI_rng_new(0);

    simulation_orig->current_frame = 1;
    LISTBASE_FOREACH (ParticleSimulationState *, state, &simulation_orig->states) {
      state->tot_particles = 1000;
      CustomData_realloc(&state->attributes, state->tot_particles);
      ensure_attributes_exist(state);

      CustomDataAttributesRef custom_data_attributes{state->attributes,
                                                     (uint)state->tot_particles};

      fn::MutableAttributesRef attributes = custom_data_attributes;
      MutableSpan<float3> positions = attributes.get<float3>("Position");
      MutableSpan<float3> velocities = attributes.get<float3>("Velocity");
      MutableSpan<int32_t> ids = attributes.get<int32_t>("ID");

      for (uint i : positions.index_range()) {
        positions[i] = {i / 100.0f, 0, 0};
        velocities[i] = {0, BLI_rng_get_float(rng), BLI_rng_get_float(rng) * 2 + 1};
        ids[i] = i;
      }
    }

    BLI_rng_free(rng);

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
  else if (current_frame == simulation_orig->current_frame + 1) {
    update_simulation_state_list(simulation_orig, tree);
    float time_step = 1.0f / 24.0f;
    simulation_orig->current_frame = current_frame;

    LISTBASE_FOREACH (ParticleSimulationState *, state, &simulation_orig->states) {
      ensure_attributes_exist(state);

      CustomDataAttributesRef custom_data_attributes{state->attributes,
                                                     (uint)state->tot_particles};

      fn::MutableAttributesRef attributes = custom_data_attributes;
      MutableSpan<float3> positions = attributes.get<float3>("Position");
      MutableSpan<float3> velocities = attributes.get<float3>("Velocity");

      Array<float3> force_vectors{(uint)state->tot_particles, {0, 0, 0}};
      Span<const ParticleForce *> forces = forces_by_simulation.lookup_as(state->head.name);
      for (const ParticleForce *force : forces) {
        force->add_force(attributes, force_vectors);
      }

      for (uint i : positions.index_range()) {
        velocities[i] += force_vectors[i] * time_step;
        positions[i] += velocities[i] * time_step;
      }
    }

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
}

}  // namespace blender::bke

void BKE_simulation_data_update(Depsgraph *depsgraph, Scene *scene, Simulation *simulation)
{
  blender::bke::simulation_data_update(depsgraph, scene, simulation);
}
