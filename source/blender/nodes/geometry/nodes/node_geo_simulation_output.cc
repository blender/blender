/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_task.hh"

#include "BKE_attribute_math.hh"
#include "BKE_bake_geometry_nodes_modifier.hh"
#include "BKE_bake_items_socket.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_modifier.hh"
#include "BKE_node_socket_value_cpp_type.hh"
#include "BKE_object.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.hh"

#include "UI_interface.hh"

#include "NOD_common.h"
#include "NOD_geometry.hh"
#include "NOD_socket.hh"
#include "NOD_zone_socket_items.hh"

#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_space_types.h"

#include "ED_node.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "MOD_nodes.hh"

#include "BLT_translation.h"

#include "WM_api.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

const CPPType &get_simulation_item_cpp_type(const eNodeSocketDatatype socket_type)
{
  const char *socket_idname = nodeStaticSocketType(socket_type, 0);
  const bNodeSocketType *typeinfo = nodeSocketTypeFind(socket_idname);
  BLI_assert(typeinfo);
  BLI_assert(typeinfo->geometry_nodes_cpp_type);
  return *typeinfo->geometry_nodes_cpp_type;
}

const CPPType &get_simulation_item_cpp_type(const NodeSimulationItem &item)
{
  return get_simulation_item_cpp_type(eNodeSocketDatatype(item.socket_type));
}

static bke::bake::BakeSocketConfig make_bake_socket_config(
    const Span<NodeSimulationItem> node_simulation_items)
{
  bke::bake::BakeSocketConfig config;
  const int items_num = node_simulation_items.size();
  config.domains.resize(items_num);
  config.types.resize(items_num);
  config.geometries_by_attribute.resize(items_num);

  int last_geometry_index = -1;
  for (const int item_i : node_simulation_items.index_range()) {
    const NodeSimulationItem &item = node_simulation_items[item_i];
    config.types[item_i] = eNodeSocketDatatype(item.socket_type);
    config.domains[item_i] = eAttrDomain(item.attribute_domain);
    if (item.socket_type == SOCK_GEOMETRY) {
      last_geometry_index = item_i;
    }
    else if (last_geometry_index != -1) {
      config.geometries_by_attribute[item_i].append(last_geometry_index);
    }
  }
  return config;
}

static std::shared_ptr<AnonymousAttributeFieldInput> make_attribute_field(
    const Object &self_object,
    const ComputeContext &compute_context,
    const bNode &node,
    const NodeSimulationItem &item,
    const CPPType &type)
{
  AnonymousAttributeIDPtr attribute_id = MEM_new<NodeAnonymousAttributeID>(
      __func__, self_object, compute_context, node, std::to_string(item.identifier), item.name);
  return std::make_shared<AnonymousAttributeFieldInput>(attribute_id, type, node.label_or_name());
}

void move_simulation_state_to_values(const Span<NodeSimulationItem> node_simulation_items,
                                     bke::bake::BakeState zone_state,
                                     const Object &self_object,
                                     const ComputeContext &compute_context,
                                     const bNode &node,
                                     Span<void *> r_output_values)
{
  const bke::bake::BakeSocketConfig config = make_bake_socket_config(node_simulation_items);
  Vector<bke::bake::BakeItem *> bake_items;
  for (const NodeSimulationItem &item : node_simulation_items) {
    std::unique_ptr<bke::bake::BakeItem> *bake_item = zone_state.items_by_id.lookup_ptr(
        item.identifier);
    bake_items.append(bake_item ? bake_item->get() : nullptr);
  }

  bke::bake::move_bake_items_to_socket_values(
      bake_items,
      config,
      [&](const int i, const CPPType &type) {
        return make_attribute_field(
            self_object, compute_context, node, node_simulation_items[i], type);
      },
      r_output_values);
}

void copy_simulation_state_to_values(const Span<NodeSimulationItem> node_simulation_items,
                                     const bke::bake::BakeStateRef &zone_state,
                                     const Object &self_object,
                                     const ComputeContext &compute_context,
                                     const bNode &node,
                                     Span<void *> r_output_values)
{
  const bke::bake::BakeSocketConfig config = make_bake_socket_config(node_simulation_items);
  Vector<const bke::bake::BakeItem *> bake_items;
  for (const NodeSimulationItem &item : node_simulation_items) {
    const bke::bake::BakeItem *const *bake_item = zone_state.items_by_id.lookup_ptr(
        item.identifier);
    bake_items.append(bake_item ? *bake_item : nullptr);
  }

  bke::bake::copy_bake_items_to_socket_values(
      bake_items,
      config,
      [&](const int i, const CPPType &type) {
        return make_attribute_field(
            self_object, compute_context, node, node_simulation_items[i], type);
      },
      r_output_values);
}

bke::bake::BakeState move_values_to_simulation_state(
    const Span<NodeSimulationItem> node_simulation_items, const Span<void *> input_values)
{
  const bke::bake::BakeSocketConfig config = make_bake_socket_config(node_simulation_items);

  Array<std::unique_ptr<bke::bake::BakeItem>> bake_items =
      bke::bake::move_socket_values_to_bake_items(input_values, config);

  bke::bake::BakeState bake_state;
  for (const int i : node_simulation_items.index_range()) {
    const NodeSimulationItem &item = node_simulation_items[i];
    std::unique_ptr<bke::bake::BakeItem> &bake_item = bake_items[i];
    if (bake_item) {
      bake_state.items_by_id.add_new(item.identifier, std::move(bake_item));
    }
  }
  return bake_state;
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_simulation_output_cc {

NODE_STORAGE_FUNCS(NodeGeometrySimulationOutput);

static bool sharing_info_equal(const ImplicitSharingInfo *a, const ImplicitSharingInfo *b)
{
  if (!a || !b) {
    return false;
  }
  return a == b;
}

template<typename T>
void mix_with_indices(MutableSpan<T> prev,
                      const VArray<T> &next,
                      const Span<int> index_map,
                      const float factor)
{
  threading::parallel_for(prev.index_range(), 1024, [&](const IndexRange range) {
    devirtualize_varray(next, [&](const auto next) {
      for (const int i : range) {
        if (index_map[i] != -1) {
          prev[i] = bke::attribute_math::mix2(factor, prev[i], next[index_map[i]]);
        }
      }
    });
  });
}

static void mix_with_indices(GMutableSpan prev,
                             const GVArray &next,
                             const Span<int> index_map,
                             const float factor)
{
  bke::attribute_math::convert_to_static_type(prev.type(), [&](auto dummy) {
    using T = decltype(dummy);
    mix_with_indices(prev.typed<T>(), next.typed<T>(), index_map, factor);
  });
}

template<typename T> void mix(MutableSpan<T> prev, const VArray<T> &next, const float factor)
{
  threading::parallel_for(prev.index_range(), 1024, [&](const IndexRange range) {
    devirtualize_varray(next, [&](const auto next) {
      for (const int i : range) {
        prev[i] = bke::attribute_math::mix2(factor, prev[i], next[i]);
      }
    });
  });
}

static void mix(GMutableSpan prev, const GVArray &next, const float factor)
{
  bke::attribute_math::convert_to_static_type(prev.type(), [&](auto dummy) {
    using T = decltype(dummy);
    mix(prev.typed<T>(), next.typed<T>(), factor);
  });
}

static void mix(MutableSpan<float4x4> prev, const Span<float4x4> next, const float factor)
{
  threading::parallel_for(prev.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      prev[i] = math::interpolate(prev[i], next[i], factor);
    }
  });
}

static void mix_with_indices(MutableSpan<float4x4> prev,
                             const Span<float4x4> next,
                             const Span<int> index_map,
                             const float factor)
{
  threading::parallel_for(prev.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      if (index_map[i] != -1) {
        prev[i] = math::interpolate(prev[i], next[index_map[i]], factor);
      }
    }
  });
}

static void mix_attributes(MutableAttributeAccessor prev_attributes,
                           const AttributeAccessor next_attributes,
                           const Span<int> index_map,
                           const eAttrDomain mix_domain,
                           const float factor,
                           const Set<std::string> &names_to_skip = {})
{
  Set<AttributeIDRef> ids = prev_attributes.all_ids();
  ids.remove("id");
  for (const StringRef name : names_to_skip) {
    ids.remove(name);
  }

  for (const AttributeIDRef &id : ids) {
    const GAttributeReader prev = prev_attributes.lookup(id);
    const eAttrDomain domain = prev.domain;
    if (domain != mix_domain) {
      continue;
    }
    const eCustomDataType type = bke::cpp_type_to_custom_data_type(prev.varray.type());
    if (ELEM(type, CD_PROP_STRING, CD_PROP_BOOL)) {
      /* String attributes can't be mixed, and there's no point in mixing boolean attributes. */
      continue;
    }
    const GAttributeReader next = next_attributes.lookup(id, prev.domain, type);
    if (sharing_info_equal(prev.sharing_info, next.sharing_info)) {
      continue;
    }
    GSpanAttributeWriter dst = prev_attributes.lookup_for_write_span(id);
    if (!index_map.is_empty()) {
      /* If there's an ID attribute, use its values to mix with potentially changed indices. */
      mix_with_indices(dst.span, *next, index_map, factor);
    }
    else if (prev_attributes.domain_size(domain) == next_attributes.domain_size(domain)) {
      /* With no ID attribute to find matching elements, we can only support mixing when the domain
       * size (topology) is the same. Other options like mixing just the start of arrays might work
       * too, but give bad results too. */
      mix(dst.span, next.varray, factor);
    }
    dst.finish();
  }
}

static Map<int, int> create_value_to_first_index_map(const Span<int> values)
{
  Map<int, int> map;
  map.reserve(values.size());
  for (const int i : values.index_range()) {
    map.add(values[i], i);
  }
  return map;
}

static Array<int> create_id_index_map(const AttributeAccessor prev_attributes,
                                      const AttributeAccessor next_attributes)
{
  const AttributeReader<int> prev_ids = prev_attributes.lookup<int>("id");
  const AttributeReader<int> next_ids = next_attributes.lookup<int>("id");
  if (!prev_ids || !next_ids) {
    return {};
  }
  if (sharing_info_equal(prev_ids.sharing_info, next_ids.sharing_info)) {
    return {};
  }

  const VArraySpan prev(*prev_ids);
  const VArraySpan next(*next_ids);

  const Map<int, int> next_id_map = create_value_to_first_index_map(VArraySpan(*next_ids));
  Array<int> index_map(prev.size());
  threading::parallel_for(prev.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      index_map[i] = next_id_map.lookup_default(prev[i], -1);
    }
  });
  return index_map;
}

static void mix_geometries(GeometrySet &prev, const GeometrySet &next, const float factor)
{
  if (Mesh *mesh_prev = prev.get_mesh_for_write()) {
    if (const Mesh *mesh_next = next.get_mesh()) {
      Array<int> vert_map = create_id_index_map(mesh_prev->attributes(), mesh_next->attributes());
      mix_attributes(mesh_prev->attributes_for_write(),
                     mesh_next->attributes(),
                     vert_map,
                     ATTR_DOMAIN_POINT,
                     factor,
                     {});
    }
  }
  if (PointCloud *points_prev = prev.get_pointcloud_for_write()) {
    if (const PointCloud *points_next = next.get_pointcloud()) {
      const Array<int> index_map = create_id_index_map(points_prev->attributes(),
                                                       points_next->attributes());
      mix_attributes(points_prev->attributes_for_write(),
                     points_next->attributes(),
                     index_map,
                     ATTR_DOMAIN_POINT,
                     factor);
    }
  }
  if (Curves *curves_prev = prev.get_curves_for_write()) {
    if (const Curves *curves_next = next.get_curves()) {
      MutableAttributeAccessor prev = curves_prev->geometry.wrap().attributes_for_write();
      const AttributeAccessor next = curves_next->geometry.wrap().attributes();
      const Array<int> index_map = create_id_index_map(prev, next);
      mix_attributes(prev,
                     next,
                     index_map,
                     ATTR_DOMAIN_POINT,
                     factor,
                     {"handle_type_left", "handle_type_right"});
    }
  }
  if (bke::Instances *instances_prev = prev.get_instances_for_write()) {
    if (const bke::Instances *instances_next = next.get_instances()) {
      const Array<int> index_map = create_id_index_map(instances_prev->attributes(),
                                                       instances_next->attributes());
      mix_attributes(instances_prev->attributes_for_write(),
                     instances_next->attributes(),
                     index_map,
                     ATTR_DOMAIN_INSTANCE,
                     factor,
                     {"position"});
      if (index_map.is_empty()) {
        mix(instances_prev->transforms(), instances_next->transforms(), factor);
      }
      else {
        mix_with_indices(
            instances_prev->transforms(), instances_next->transforms(), index_map, factor);
      }
    }
  }
}

static void mix_simulation_state(const NodeSimulationItem &item,
                                 void *prev,
                                 const void *next,
                                 const float factor)
{
  switch (eNodeSocketDatatype(item.socket_type)) {
    case SOCK_GEOMETRY: {
      mix_geometries(
          *static_cast<GeometrySet *>(prev), *static_cast<const GeometrySet *>(next), factor);
      break;
    }
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_INT:
    case SOCK_BOOLEAN:
    case SOCK_ROTATION:
    case SOCK_RGBA: {
      const CPPType &type = get_simulation_item_cpp_type(item);
      const bke::ValueOrFieldCPPType &value_or_field_type =
          *bke::ValueOrFieldCPPType::get_from_self(type);
      if (value_or_field_type.is_field(prev) || value_or_field_type.is_field(next)) {
        /* Fields are evaluated on geometries and are mixed there. */
        break;
      }

      void *prev_value = value_or_field_type.get_value_ptr(prev);
      const void *next_value = value_or_field_type.get_value_ptr(next);
      bke::attribute_math::convert_to_static_type(value_or_field_type.value, [&](auto dummy) {
        using T = decltype(dummy);
        *static_cast<T *>(prev_value) = bke::attribute_math::mix2(
            factor, *static_cast<T *>(prev_value), *static_cast<const T *>(next_value));
      });
      break;
    }
    default:
      break;
  }
}

class LazyFunctionForSimulationOutputNode final : public LazyFunction {
  const bNode &node_;
  Span<NodeSimulationItem> simulation_items_;
  int skip_input_index_;
  /**
   * Start index of the simulation state inputs that are used when the simulation is skipped. Those
   * inputs are linked directly to the simulation input node. Those inputs only exist internally,
   * but not in the UI.
   */
  int skip_inputs_offset_;
  /**
   * Start index of the simulation state inputs that are used when the simulation is actually
   * computed. Those correspond to the sockets that are visible in the UI.
   */
  int solve_inputs_offset_;

 public:
  LazyFunctionForSimulationOutputNode(const bNode &node,
                                      GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
      : node_(node)
  {
    debug_name_ = "Simulation Output";
    const NodeGeometrySimulationOutput &storage = node_storage(node);
    simulation_items_ = {storage.items, storage.items_num};

    MutableSpan<int> lf_index_by_bsocket = own_lf_graph_info.mapping.lf_index_by_bsocket;

    const bNodeSocket &skip_bsocket = node.input_socket(0);
    skip_input_index_ = inputs_.append_and_get_index_as(
        "Skip", *skip_bsocket.typeinfo->geometry_nodes_cpp_type, lf::ValueUsage::Maybe);
    lf_index_by_bsocket[skip_bsocket.index_in_tree()] = skip_input_index_;

    skip_inputs_offset_ = inputs_.size();

    /* Add the skip inputs that are linked to the simulation input node. */
    for (const int i : simulation_items_.index_range()) {
      const NodeSimulationItem &item = simulation_items_[i];
      const CPPType &type = get_simulation_item_cpp_type(item);
      inputs_.append_as(item.name, type, lf::ValueUsage::Maybe);
    }

    solve_inputs_offset_ = inputs_.size();

    /* Add the solve inputs that correspond to the simulation state inputs in the UI. */
    for (const int i : simulation_items_.index_range()) {
      const NodeSimulationItem &item = simulation_items_[i];
      const bNodeSocket &input_bsocket = node.input_socket(i + 1);
      const bNodeSocket &output_bsocket = node.output_socket(i);

      const CPPType &type = get_simulation_item_cpp_type(item);

      lf_index_by_bsocket[input_bsocket.index_in_tree()] = inputs_.append_and_get_index_as(
          item.name, type, lf::ValueUsage::Maybe);
      lf_index_by_bsocket[output_bsocket.index_in_tree()] = outputs_.append_and_get_index_as(
          item.name, type);
    }
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const final
  {
    GeoNodesLFUserData &user_data = *static_cast<GeoNodesLFUserData *>(context.user_data);
    if (!user_data.modifier_data) {
      params.set_default_remaining_outputs();
      return;
    }
    const GeoNodesModifierData &modifier_data = *user_data.modifier_data;
    if (!modifier_data.simulation_params) {
      params.set_default_remaining_outputs();
      return;
    }
    std::optional<FoundNestedNodeID> found_id = find_nested_node_id(user_data, node_.identifier);
    if (!found_id) {
      params.set_default_remaining_outputs();
      return;
    }
    if (found_id->is_in_loop) {
      params.set_default_remaining_outputs();
      return;
    }
    SimulationZoneBehavior *zone_behavior = modifier_data.simulation_params->get(found_id->id);
    if (!zone_behavior) {
      params.set_default_remaining_outputs();
      return;
    }
    sim_output::Behavior &output_behavior = zone_behavior->output;
    if (auto *info = std::get_if<sim_output::ReadSingle>(&output_behavior)) {
      this->output_cached_state(params, user_data, info->state);
    }
    else if (auto *info = std::get_if<sim_output::ReadInterpolated>(&output_behavior)) {
      this->output_mixed_cached_state(params,
                                      *modifier_data.self_object,
                                      *user_data.compute_context,
                                      info->prev_state,
                                      info->next_state,
                                      info->mix_factor);
    }
    else if (std::get_if<sim_output::PassThrough>(&output_behavior)) {
      this->pass_through(params, user_data);
    }
    else if (auto *info = std::get_if<sim_output::StoreNewState>(&output_behavior)) {
      this->store_new_state(params, user_data, *info);
    }
    else {
      BLI_assert_unreachable();
    }
  }

  void output_cached_state(lf::Params &params,
                           GeoNodesLFUserData &user_data,
                           const bke::bake::BakeStateRef &state) const
  {
    Array<void *> output_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    copy_simulation_state_to_values(simulation_items_,
                                    state,
                                    *user_data.modifier_data->self_object,
                                    *user_data.compute_context,
                                    node_,
                                    output_values);
    for (const int i : simulation_items_.index_range()) {
      params.output_set(i);
    }
  }

  void output_mixed_cached_state(lf::Params &params,
                                 const Object &self_object,
                                 const ComputeContext &compute_context,
                                 const bke::bake::BakeStateRef &prev_state,
                                 const bke::bake::BakeStateRef &next_state,
                                 const float mix_factor) const
  {
    Array<void *> output_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    copy_simulation_state_to_values(
        simulation_items_, prev_state, self_object, compute_context, node_, output_values);

    Array<void *> next_values(simulation_items_.size());
    LinearAllocator<> allocator;
    for (const int i : simulation_items_.index_range()) {
      const CPPType &type = *outputs_[i].type;
      next_values[i] = allocator.allocate(type.size(), type.alignment());
    }
    copy_simulation_state_to_values(
        simulation_items_, next_state, self_object, compute_context, node_, next_values);

    for (const int i : simulation_items_.index_range()) {
      mix_simulation_state(simulation_items_[i], output_values[i], next_values[i], mix_factor);
    }

    for (const int i : simulation_items_.index_range()) {
      const CPPType &type = *outputs_[i].type;
      type.destruct(next_values[i]);
    }

    for (const int i : simulation_items_.index_range()) {
      params.output_set(i);
    }
  }

  void pass_through(lf::Params &params, GeoNodesLFUserData &user_data) const
  {
    std::optional<bke::bake::BakeState> bake_state = this->get_bake_state_from_inputs(params,
                                                                                      true);
    if (!bake_state) {
      /* Wait for inputs to be computed. */
      return;
    }

    Array<void *> output_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    move_simulation_state_to_values(simulation_items_,
                                    std::move(*bake_state),
                                    *user_data.modifier_data->self_object,
                                    *user_data.compute_context,
                                    node_,
                                    output_values);
    for (const int i : simulation_items_.index_range()) {
      params.output_set(i);
    }
  }

  void store_new_state(lf::Params &params,
                       GeoNodesLFUserData &user_data,
                       const sim_output::StoreNewState &info) const
  {
    const bool *skip = params.try_get_input_data_ptr_or_request<bool>(skip_input_index_);
    if (skip == nullptr) {
      /* Wait for skip input to be computed. */
      return;
    }

    /* Instead of outputting the values directly, convert them to a bake state and then back. This
     * ensures that some geometry processing happens on the data consistently (e.g. removing
     * anonymous attributes). */
    std::optional<bke::bake::BakeState> bake_state = this->get_bake_state_from_inputs(params,
                                                                                      *skip);
    if (!bake_state) {
      /* Wait for inputs to be computed. */
      return;
    }
    this->output_cached_state(params, user_data, *bake_state);
    info.store_fn(std::move(*bake_state));
  }

  std::optional<bke::bake::BakeState> get_bake_state_from_inputs(lf::Params &params,
                                                                 const bool skip) const
  {
    /* Choose which set of input parameters to use. The others are ignored. */
    const int params_offset = skip ? skip_inputs_offset_ : solve_inputs_offset_;
    Array<void *> input_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      input_values[i] = params.try_get_input_data_ptr_or_request(i + params_offset);
    }
    if (input_values.as_span().contains(nullptr)) {
      /* Wait for inputs to be computed. */
      return std::nullopt;
    }

    return move_values_to_simulation_state(simulation_items_, input_values);
  }
};

}  // namespace blender::nodes::node_geo_simulation_output_cc

namespace blender::nodes {

std::unique_ptr<LazyFunction> get_simulation_output_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
{
  namespace file_ns = blender::nodes::node_geo_simulation_output_cc;
  BLI_assert(node.type == GEO_NODE_SIMULATION_OUTPUT);
  return std::make_unique<file_ns::LazyFunctionForSimulationOutputNode>(node, own_lf_graph_info);
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_simulation_output_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>("Skip").description(
      "Forward the output of the simulation input node directly to the output node and ignore "
      "the nodes in the simulation zone");

  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }

  const NodeGeometrySimulationOutput &storage = node_storage(*node);

  for (const int i : IndexRange(storage.items_num)) {
    const NodeSimulationItem &item = storage.items[i];
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    const StringRef name = item.name;
    const std::string identifier = SimulationItemsAccessor::socket_identifier_for_item(item);
    auto &input_decl = b.add_input(socket_type, name, identifier);
    auto &output_decl = b.add_output(socket_type, name, identifier);
    if (socket_type_supports_fields(socket_type)) {
      input_decl.supports_field();
      output_decl.dependent_field({input_decl.input_index()});
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Extend>("", "__extend__");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySimulationOutput *data = MEM_cnew<NodeGeometrySimulationOutput>(__func__);

  data->next_identifier = 0;

  data->items = MEM_cnew_array<NodeSimulationItem>(1, __func__);
  data->items[0].name = BLI_strdup(DATA_("Geometry"));
  data->items[0].socket_type = SOCK_GEOMETRY;
  data->items[0].identifier = data->next_identifier++;
  data->items_num = 1;

  node->storage = data;
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<SimulationItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometrySimulationOutput &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_new<NodeGeometrySimulationOutput>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<SimulationItemsAccessor>(*src_node, *dst_node);
}

static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  const bNode *node = static_cast<bNode *>(ptr->data);
  Scene *scene = CTX_data_scene(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return;
  }
  std::optional<ed::space_node::ObjectAndModifier> object_and_modifier =
      ed::space_node::get_modifier_for_node_editor(*snode);
  if (!object_and_modifier.has_value()) {
    return;
  }
  const Object &object = *object_and_modifier->object;
  const NodesModifierData &nmd = *object_and_modifier->nmd;
  const std::optional<int32_t> bake_id = ed::space_node::find_nested_node_id_in_root(*snode,
                                                                                     *node);
  if (!bake_id.has_value()) {
    return;
  }
  const NodesModifierBake *bake = nullptr;
  for (const NodesModifierBake &iter_bake : Span{nmd.bakes, nmd.bakes_num}) {
    if (iter_bake.id == *bake_id) {
      bake = &iter_bake;
      break;
    }
  }
  if (bake == nullptr) {
    return;
  }

  PointerRNA bake_rna = RNA_pointer_create(
      const_cast<ID *>(&object.id), &RNA_NodesModifierBake, (void *)bake);

  const std::optional<IndexRange> simulation_range = bke::bake::get_node_bake_frame_range(
      *scene, object, nmd, *bake_id);

  std::optional<IndexRange> baked_range;
  if (nmd.runtime->cache) {
    const bke::bake::ModifierCache &cache = *nmd.runtime->cache;
    std::lock_guard lock{cache.mutex};
    if (const std::unique_ptr<bke::bake::NodeCache> *node_cache_ptr = cache.cache_by_id.lookup_ptr(
            *bake_id))
    {
      const bke::bake::NodeCache &node_cache = **node_cache_ptr;
      if (node_cache.cache_status == bke::bake::CacheStatus::Baked &&
          !node_cache.frame_caches.is_empty())
      {
        const int first_frame = node_cache.frame_caches.first()->frame.frame();
        const int last_frame = node_cache.frame_caches.last()->frame.frame();
        baked_range = IndexRange(first_frame, last_frame - first_frame + 1);
      }
    }
  }
  bool is_baked = baked_range.has_value();

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiLayout *row = uiLayoutRow(col, true);
    {
      char bake_label[1024] = N_("Bake");

      PointerRNA ptr;
      uiItemFullO(row,
                  "OBJECT_OT_simulation_nodes_cache_bake_single",
                  bake_label,
                  ICON_NONE,
                  nullptr,
                  WM_OP_INVOKE_DEFAULT,
                  UI_ITEM_NONE,
                  &ptr);
      WM_operator_properties_id_lookup_set_from_id(&ptr, &object.id);
      RNA_string_set(&ptr, "modifier_name", nmd.modifier.name);
      RNA_int_set(&ptr, "bake_id", bake->id);
    }
    {
      PointerRNA ptr;
      uiItemFullO(row,
                  "OBJECT_OT_simulation_nodes_cache_delete_single",
                  "",
                  ICON_TRASH,
                  nullptr,
                  WM_OP_INVOKE_DEFAULT,
                  UI_ITEM_NONE,
                  &ptr);
      WM_operator_properties_id_lookup_set_from_id(&ptr, &object.id);
      RNA_string_set(&ptr, "modifier_name", nmd.modifier.name);
      RNA_int_set(&ptr, "bake_id", bake->id);
    }
    if (is_baked) {
      char baked_range_label[64];
      SNPRINTF(baked_range_label,
               N_("Baked %d - %d"),
               int(baked_range->first()),
               int(baked_range->last()));
      uiItemL(layout, baked_range_label, ICON_NONE);
    }
    else if (simulation_range.has_value()) {
      char simulation_range_label[64];
      SNPRINTF(simulation_range_label,
               N_("Frames %d - %d"),
               int(simulation_range->first()),
               int(simulation_range->last()));
      uiItemL(layout, simulation_range_label, ICON_NONE);
    }
  }
  {
    uiLayout *settings_col = uiLayoutColumn(layout, false);
    uiLayoutSetActive(settings_col, !is_baked);
    {
      uiLayout *col = uiLayoutColumn(settings_col, true);
      uiLayoutSetActive(col, !is_baked);
      uiItemR(col, &bake_rna, "use_custom_path", UI_ITEM_NONE, "Custom Path", ICON_NONE);
      uiLayout *subcol = uiLayoutColumn(col, true);
      uiLayoutSetActive(subcol, bake->flag & NODES_MODIFIER_BAKE_CUSTOM_PATH);
      uiItemR(subcol, &bake_rna, "directory", UI_ITEM_NONE, "Path", ICON_NONE);
    }
    {
      uiLayout *col = uiLayoutColumn(settings_col, true);
      uiItemR(col,
              &bake_rna,
              "use_custom_simulation_frame_range",
              UI_ITEM_NONE,
              "Custom Range",
              ICON_NONE);
      uiLayout *subcol = uiLayoutColumn(col, true);
      uiLayoutSetActive(subcol, bake->flag & NODES_MODIFIER_BAKE_CUSTOM_SIMULATION_FRAME_RANGE);
      uiItemR(subcol, &bake_rna, "frame_start", UI_ITEM_NONE, "Start", ICON_NONE);
      uiItemR(subcol, &bake_rna, "frame_end", UI_ITEM_NONE, "End", ICON_NONE);
    }
  }
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  return socket_items::try_add_item_via_any_extend_socket<SimulationItemsAccessor>(
      *ntree, *node, *node, *link);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SIMULATION_OUTPUT, "Simulation Output", NODE_CLASS_INTERFACE);
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = nullptr;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  node_type_storage(&ntype, "NodeGeometrySimulationOutput", node_free_storage, node_copy_storage);
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_simulation_output_cc

blender::Span<NodeSimulationItem> NodeGeometrySimulationOutput::items_span() const
{
  return blender::Span<NodeSimulationItem>(items, items_num);
}

blender::MutableSpan<NodeSimulationItem> NodeGeometrySimulationOutput::items_span()
{
  return blender::MutableSpan<NodeSimulationItem>(items, items_num);
}
