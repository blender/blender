/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_string_utils.h"
#include "BLI_task.hh"

#include "BKE_attribute_math.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "UI_interface.h"

#include "NOD_common.h"
#include "NOD_socket.h"

#include "FN_field_cpp_type.hh"

#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "NOD_add_node_search.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

std::string socket_identifier_for_simulation_item(const NodeSimulationItem &item)
{
  return "Item_" + std::to_string(item.identifier);
}

static std::unique_ptr<SocketDeclaration> socket_declaration_for_simulation_item(
    const NodeSimulationItem &item, const eNodeSocketInOut in_out, const int index)
{
  const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
  BLI_assert(NOD_geometry_simulation_output_item_socket_type_supported(socket_type));

  std::unique_ptr<SocketDeclaration> decl;
  switch (socket_type) {
    case SOCK_FLOAT:
      decl = std::make_unique<decl::Float>();
      decl->input_field_type = InputSocketFieldType::IsSupported;
      decl->output_field_dependency = OutputFieldDependency::ForPartiallyDependentField({index});
      break;
    case SOCK_VECTOR:
      decl = std::make_unique<decl::Vector>();
      decl->input_field_type = InputSocketFieldType::IsSupported;
      decl->output_field_dependency = OutputFieldDependency::ForPartiallyDependentField({index});
      break;
    case SOCK_RGBA:
      decl = std::make_unique<decl::Color>();
      decl->input_field_type = InputSocketFieldType::IsSupported;
      decl->output_field_dependency = OutputFieldDependency::ForPartiallyDependentField({index});
      break;
    case SOCK_BOOLEAN:
      decl = std::make_unique<decl::Bool>();
      decl->input_field_type = InputSocketFieldType::IsSupported;
      decl->output_field_dependency = OutputFieldDependency::ForPartiallyDependentField({index});
      break;
    case SOCK_ROTATION:
      decl = std::make_unique<decl::Rotation>();
      decl->input_field_type = InputSocketFieldType::IsSupported;
      decl->output_field_dependency = OutputFieldDependency::ForPartiallyDependentField({index});
      break;
    case SOCK_INT:
      decl = std::make_unique<decl::Int>();
      decl->input_field_type = InputSocketFieldType::IsSupported;
      decl->output_field_dependency = OutputFieldDependency::ForPartiallyDependentField({index});
      break;
    case SOCK_STRING:
      decl = std::make_unique<decl::String>();
      break;
    case SOCK_GEOMETRY:
      decl = std::make_unique<decl::Geometry>();
      break;
    default:
      BLI_assert_unreachable();
  }

  decl->name = item.name ? item.name : "";
  decl->identifier = socket_identifier_for_simulation_item(item);
  decl->in_out = in_out;
  return decl;
}

void socket_declarations_for_simulation_items(const Span<NodeSimulationItem> items,
                                              NodeDeclaration &r_declaration)
{
  for (const int i : items.index_range()) {
    const NodeSimulationItem &item = items[i];
    r_declaration.inputs.append(socket_declaration_for_simulation_item(item, SOCK_IN, i));
    r_declaration.outputs.append(socket_declaration_for_simulation_item(item, SOCK_OUT, i));
  }
  r_declaration.inputs.append(decl::create_extend_declaration(SOCK_IN));
  r_declaration.outputs.append(decl::create_extend_declaration(SOCK_OUT));
}

struct SimulationItemsUniqueNameArgs {
  NodeGeometrySimulationOutput *sim;
  const NodeSimulationItem *item;
};

static bool simulation_items_unique_name_check(void *arg, const char *name)
{
  const SimulationItemsUniqueNameArgs &args = *static_cast<const SimulationItemsUniqueNameArgs *>(
      arg);
  for (const NodeSimulationItem &item : args.sim->items_span()) {
    if (&item != args.item) {
      if (STREQ(item.name, name)) {
        return true;
      }
    }
  }
  if (STREQ(name, "Delta Time")) {
    return true;
  }
  return false;
}

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

static void remove_materials(Material ***materials, short *materials_num)
{
  MEM_SAFE_FREE(*materials);
  *materials_num = 0;
}

/**
 * Removes parts of the geometry that can't be stored in the simulation state:
 * - Anonymous attributes can't be stored because it is not known which of them will or will not be
 *   used in the future.
 * - Materials can't be stored directly, because they are linked ID data blocks that can't be
 *   restored from baked data currently.
 */
static void cleanup_geometry_for_simulation_state(GeometrySet &main_geometry)
{
  main_geometry.modify_geometry_sets([&](GeometrySet &geometry) {
    if (Mesh *mesh = geometry.get_mesh_for_write()) {
      mesh->attributes_for_write().remove_anonymous();
      remove_materials(&mesh->mat, &mesh->totcol);
    }
    if (Curves *curves = geometry.get_curves_for_write()) {
      curves->geometry.wrap().attributes_for_write().remove_anonymous();
      remove_materials(&curves->mat, &curves->totcol);
    }
    if (PointCloud *pointcloud = geometry.get_pointcloud_for_write()) {
      pointcloud->attributes_for_write().remove_anonymous();
      remove_materials(&pointcloud->mat, &pointcloud->totcol);
    }
    if (bke::Instances *instances = geometry.get_instances_for_write()) {
      instances->attributes_for_write().remove_anonymous();
    }
    geometry.keep_only_during_modify({GeometryComponent::Type::Mesh,
                                      GeometryComponent::Type::Curve,
                                      GeometryComponent::Type::PointCloud,
                                      GeometryComponent::Type::Instance});
  });
}

void simulation_state_to_values(const Span<NodeSimulationItem> node_simulation_items,
                                const bke::sim::SimulationZoneState &zone_state,
                                const Object &self_object,
                                const ComputeContext &compute_context,
                                const bNode &node,
                                Span<void *> r_output_values)
{
  /* Some attributes stored in the simulation state become anonymous attributes in geometry nodes.
   * This maps attribute names to their corresponding anonymous attribute ids. */
  Map<std::string, AnonymousAttributeIDPtr> attribute_map;
  Vector<GeometrySet *> geometries;

  for (const int i : node_simulation_items.index_range()) {
    const NodeSimulationItem &item = node_simulation_items[i];
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    const CPPType &cpp_type = get_simulation_item_cpp_type(socket_type);

    void *r_output_value = r_output_values[i];

    if (!zone_state.item_by_identifier.contains(item.identifier)) {
      cpp_type.value_initialize(r_output_value);
      continue;
    }
    const bke::sim::SimulationStateItem &state_item = *zone_state.item_by_identifier.lookup(
        item.identifier);

    switch (socket_type) {
      case SOCK_GEOMETRY: {
        if (const auto *geo_state_item =
                dynamic_cast<const bke::sim::GeometrySimulationStateItem *>(&state_item))
        {
          GeometrySet *geometry = new (r_output_value) GeometrySet(geo_state_item->geometry);
          geometries.append(geometry);
        }
        else {
          cpp_type.value_initialize(r_output_value);
        }
        break;
      }
      case SOCK_FLOAT:
      case SOCK_VECTOR:
      case SOCK_INT:
      case SOCK_BOOLEAN:
      case SOCK_ROTATION:
      case SOCK_RGBA: {
        const fn::ValueOrFieldCPPType &value_or_field_type =
            *fn::ValueOrFieldCPPType::get_from_self(cpp_type);
        if (const auto *primitive_state_item =
                dynamic_cast<const bke::sim::PrimitiveSimulationStateItem *>(&state_item))
        {
          if (primitive_state_item->type() == value_or_field_type.value) {
            value_or_field_type.construct_from_value(r_output_value,
                                                     primitive_state_item->value());
          }
          else {
            cpp_type.value_initialize(r_output_value);
          }
        }
        else if (const auto *attribute_state_item =
                     dynamic_cast<const bke::sim::AttributeSimulationStateItem *>(&state_item))
        {
          AnonymousAttributeIDPtr attribute_id = MEM_new<NodeAnonymousAttributeID>(
              __func__,
              self_object,
              compute_context,
              node,
              std::to_string(item.identifier),
              item.name);
          GField field{std::make_shared<AnonymousAttributeFieldInput>(
              attribute_id, value_or_field_type.value, node.label_or_name())};
          value_or_field_type.construct_from_field(r_output_value, std::move(field));
          attribute_map.add(attribute_state_item->name(), std::move(attribute_id));
        }
        else {
          cpp_type.value_initialize(r_output_value);
        }
        break;
      }
      case SOCK_STRING: {
        if (const auto *string_state_item =
                dynamic_cast<const bke::sim::StringSimulationStateItem *>(&state_item))
        {
          new (r_output_value) ValueOrField<std::string>(string_state_item->value());
        }
        else {
          cpp_type.value_initialize(r_output_value);
        }
        break;
      }
      default: {
        cpp_type.value_initialize(r_output_value);
        break;
      }
    }
  }

  /* Make some attributes anonymous. */
  for (GeometrySet *geometry : geometries) {
    for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                               GeometryComponent::Type::Curve,
                                               GeometryComponent::Type::PointCloud,
                                               GeometryComponent::Type::Instance})
    {
      if (!geometry->has(type)) {
        continue;
      }
      GeometryComponent &component = geometry->get_component_for_write(type);
      MutableAttributeAccessor attributes = *component.attributes_for_write();
      for (const MapItem<std::string, AnonymousAttributeIDPtr> &attribute_item :
           attribute_map.items()) {
        attributes.rename(attribute_item.key, *attribute_item.value);
      }
    }
  }
}

void values_to_simulation_state(const Span<NodeSimulationItem> node_simulation_items,
                                const Span<void *> input_values,
                                bke::sim::SimulationZoneState &r_zone_state)
{
  Vector<GeometrySet *> stored_geometries;

  for (const int i : node_simulation_items.index_range()) {
    const NodeSimulationItem &item = node_simulation_items[i];
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    void *input_value = input_values[i];

    std::unique_ptr<bke::sim::SimulationStateItem> state_item;
    switch (socket_type) {
      case SOCK_GEOMETRY: {
        GeometrySet &geometry = *static_cast<GeometrySet *>(input_value);
        auto geometry_state_item = std::make_unique<bke::sim::GeometrySimulationStateItem>(
            std::move(geometry));
        stored_geometries.append(&geometry_state_item->geometry);
        state_item = std::move(geometry_state_item);
        break;
      }
      case SOCK_FLOAT:
      case SOCK_VECTOR:
      case SOCK_INT:
      case SOCK_BOOLEAN:
      case SOCK_ROTATION:
      case SOCK_RGBA: {
        const CPPType &type = get_simulation_item_cpp_type(item);
        const fn::ValueOrFieldCPPType &value_or_field_type =
            *fn::ValueOrFieldCPPType::get_from_self(type);
        if (value_or_field_type.is_field(input_value)) {
          /* Fields are evaluated and stored as attributes. */
          if (!stored_geometries.is_empty()) {
            /* Possible things to consider:
             * - Store attributes on multiple/all geometries.
             * - If the attribute is an anonymous attribute, just rename it for the simulation
             *   state, without considering the domain. This would allow e.g. having the attribute
             *   only on some parts of the geometry set.
             */
            GeometrySet &geometry = *stored_geometries.last();
            const GField &field = *value_or_field_type.get_field_ptr(input_value);
            const eAttrDomain domain = eAttrDomain(item.attribute_domain);
            const std::string attribute_name = ".sim_" + std::to_string(item.identifier);
            if (geometry.has_pointcloud()) {
              PointCloudComponent &component =
                  geometry.get_component_for_write<PointCloudComponent>();
              bke::try_capture_field_on_geometry(component, attribute_name, domain, field);
            }
            if (geometry.has_mesh()) {
              MeshComponent &component = geometry.get_component_for_write<MeshComponent>();
              bke::try_capture_field_on_geometry(component, attribute_name, domain, field);
            }
            if (geometry.has_curves()) {
              CurveComponent &component = geometry.get_component_for_write<CurveComponent>();
              bke::try_capture_field_on_geometry(component, attribute_name, domain, field);
            }
            if (geometry.has_instances()) {
              InstancesComponent &component =
                  geometry.get_component_for_write<InstancesComponent>();
              bke::try_capture_field_on_geometry(component, attribute_name, domain, field);
            }
            state_item = std::make_unique<bke::sim::AttributeSimulationStateItem>(attribute_name);
          }
        }
        else {
          const void *value = value_or_field_type.get_value_ptr(input_value);
          state_item = std::make_unique<bke::sim::PrimitiveSimulationStateItem>(
              value_or_field_type.value, value);
        }
        break;
      }
      case SOCK_STRING: {
        const ValueOrField<std::string> &value = *static_cast<const ValueOrField<std::string> *>(
            input_value);
        state_item = std::make_unique<bke::sim::StringSimulationStateItem>(value.as_value());
        break;
      }
      default:
        break;
    }

    if (state_item) {
      r_zone_state.item_by_identifier.add_new(item.identifier, std::move(state_item));
    }
  }

  for (GeometrySet *geometry : stored_geometries) {
    cleanup_geometry_for_simulation_state(*geometry);
    geometry->ensure_owns_all_data();
  }
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_simulation_output_cc {

NODE_STORAGE_FUNCS(NodeGeometrySimulationOutput);

struct EvalData {
  bool is_first_evaluation = true;
};

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
    if (const Mesh *mesh_next = next.get_mesh_for_read()) {
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
    if (const PointCloud *points_next = next.get_pointcloud_for_read()) {
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
    if (const Curves *curves_next = next.get_curves_for_read()) {
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
    if (const bke::Instances *instances_next = next.get_instances_for_read()) {
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
      const fn::ValueOrFieldCPPType &value_or_field_type = *fn::ValueOrFieldCPPType::get_from_self(
          type);
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

 public:
  LazyFunctionForSimulationOutputNode(const bNode &node,
                                      GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
      : node_(node)
  {
    debug_name_ = "Simulation Output";
    const NodeGeometrySimulationOutput &storage = node_storage(node);
    simulation_items_ = {storage.items, storage.items_num};

    MutableSpan<int> lf_index_by_bsocket = own_lf_graph_info.mapping.lf_index_by_bsocket;

    for (const int i : simulation_items_.index_range()) {
      const NodeSimulationItem &item = simulation_items_[i];
      const bNodeSocket &input_bsocket = node.input_socket(i);
      const bNodeSocket &output_bsocket = node.output_socket(i);

      const CPPType &type = get_simulation_item_cpp_type(item);

      lf_index_by_bsocket[input_bsocket.index_in_tree()] = inputs_.append_and_get_index_as(
          item.name, type, lf::ValueUsage::Maybe);
      lf_index_by_bsocket[output_bsocket.index_in_tree()] = outputs_.append_and_get_index_as(
          item.name, type);
    }
  }

  void *init_storage(LinearAllocator<> &allocator) const
  {
    return allocator.construct<EvalData>().release();
  }

  void destruct_storage(void *storage) const
  {
    std::destroy_at(static_cast<EvalData *>(storage));
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const final
  {
    GeoNodesLFUserData &user_data = *static_cast<GeoNodesLFUserData *>(context.user_data);
    GeoNodesModifierData &modifier_data = *user_data.modifier_data;
    EvalData &eval_data = *static_cast<EvalData *>(context.storage);
    BLI_SCOPED_DEFER([&]() { eval_data.is_first_evaluation = false; });

    const bke::sim::SimulationZoneID zone_id = get_simulation_zone_id(*user_data.compute_context,
                                                                      node_.identifier);

    const bke::sim::SimulationZoneState *current_zone_state =
        modifier_data.current_simulation_state ?
            modifier_data.current_simulation_state->get_zone_state(zone_id) :
            nullptr;
    if (eval_data.is_first_evaluation && current_zone_state != nullptr) {
      /* Common case when data is cached already. */
      this->output_cached_state(
          params, *modifier_data.self_object, *user_data.compute_context, *current_zone_state);
      return;
    }

    if (modifier_data.current_simulation_state_for_write == nullptr) {
      const bke::sim::SimulationZoneState *prev_zone_state =
          modifier_data.prev_simulation_state ?
              modifier_data.prev_simulation_state->get_zone_state(zone_id) :
              nullptr;
      if (prev_zone_state == nullptr) {
        /* There is no previous simulation state and we also don't create a new one, so just
         * output defaults. */
        params.set_default_remaining_outputs();
        return;
      }
      const bke::sim::SimulationZoneState *next_zone_state =
          modifier_data.next_simulation_state ?
              modifier_data.next_simulation_state->get_zone_state(zone_id) :
              nullptr;
      if (next_zone_state == nullptr) {
        /* Output the last cached simulation state. */
        this->output_cached_state(
            params, *modifier_data.self_object, *user_data.compute_context, *prev_zone_state);
        return;
      }
      /* A previous and next frame is cached already, but the current frame is not. */
      this->output_mixed_cached_state(params,
                                      *modifier_data.self_object,
                                      *user_data.compute_context,
                                      *prev_zone_state,
                                      *next_zone_state,
                                      modifier_data.simulation_state_mix_factor);
      return;
    }

    bke::sim::SimulationZoneState &new_zone_state =
        modifier_data.current_simulation_state_for_write->get_zone_state_for_write(zone_id);
    if (eval_data.is_first_evaluation) {
      new_zone_state.item_by_identifier.clear();
    }

    Array<void *> input_values(simulation_items_.size(), nullptr);
    for (const int i : simulation_items_.index_range()) {
      input_values[i] = params.try_get_input_data_ptr_or_request(i);
    }
    if (input_values.as_span().contains(nullptr)) {
      /* Wait until all inputs are available. */
      return;
    }
    values_to_simulation_state(simulation_items_, input_values, new_zone_state);
    this->output_cached_state(
        params, *modifier_data.self_object, *user_data.compute_context, new_zone_state);
  }

  void output_cached_state(lf::Params &params,
                           const Object &self_object,
                           const ComputeContext &compute_context,
                           const bke::sim::SimulationZoneState &state) const
  {
    Array<void *> output_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    simulation_state_to_values(
        simulation_items_, state, self_object, compute_context, node_, output_values);
    for (const int i : simulation_items_.index_range()) {
      params.output_set(i);
    }
  }

  void output_mixed_cached_state(lf::Params &params,
                                 const Object &self_object,
                                 const ComputeContext &compute_context,
                                 const bke::sim::SimulationZoneState &prev_state,
                                 const bke::sim::SimulationZoneState &next_state,
                                 const float mix_factor) const
  {
    Array<void *> output_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    simulation_state_to_values(
        simulation_items_, prev_state, self_object, compute_context, node_, output_values);

    Array<void *> next_values(simulation_items_.size());
    LinearAllocator<> allocator;
    for (const int i : simulation_items_.index_range()) {
      const CPPType &type = *outputs_[i].type;
      next_values[i] = allocator.allocate(type.size(), type.alignment());
    }
    simulation_state_to_values(
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

bke::sim::SimulationZoneID get_simulation_zone_id(const ComputeContext &compute_context,
                                                  const int output_node_id)
{
  bke::sim::SimulationZoneID zone_id;
  for (const ComputeContext *context = &compute_context; context != nullptr;
       context = context->parent())
  {
    if (const auto *node_context = dynamic_cast<const bke::NodeGroupComputeContext *>(context)) {
      zone_id.node_ids.append(node_context->node_id());
    }
  }
  std::reverse(zone_id.node_ids.begin(), zone_id.node_ids.end());
  zone_id.node_ids.append(output_node_id);
  return zone_id;
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_simulation_output_cc {

static void node_declare_dynamic(const bNodeTree & /*node_tree*/,
                                 const bNode &node,
                                 NodeDeclaration &r_declaration)
{
  const NodeGeometrySimulationOutput &storage = node_storage(node);
  socket_declarations_for_simulation_items({storage.items, storage.items_num}, r_declaration);
}

static void search_node_add_ops(GatherAddNodeSearchParams &params)
{
  AddNodeItem item;
  item.ui_name = IFACE_("Simulation Zone");
  item.description = TIP_("Add a new simulation input and output nodes to the node tree");
  item.add_fn = [](const bContext &C, bNodeTree &node_tree, float2 cursor) {
    bNode *input = nodeAddNode(&C, &node_tree, "GeometryNodeSimulationInput");
    bNode *output = nodeAddNode(&C, &node_tree, "GeometryNodeSimulationOutput");
    static_cast<NodeGeometrySimulationInput *>(input->storage)->output_node_id =
        output->identifier;

    NodeSimulationItem &item = node_storage(*output).items[0];

    update_node_declaration_and_sockets(node_tree, *input);
    update_node_declaration_and_sockets(node_tree, *output);

    nodeAddLink(
        &node_tree,
        input,
        nodeFindSocket(input, SOCK_OUT, socket_identifier_for_simulation_item(item).c_str()),
        output,
        nodeFindSocket(output, SOCK_IN, socket_identifier_for_simulation_item(item).c_str()));

    input->locx = cursor.x / UI_SCALE_FAC - 150;
    input->locy = cursor.y / UI_SCALE_FAC + 20;
    output->locx = cursor.x / UI_SCALE_FAC + 150;
    output->locy = cursor.y / UI_SCALE_FAC + 20;

    return Vector<bNode *>({input, output});
  };
  params.add_item(std::move(item));
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
  if (!node->storage) {
    return;
  }
  NodeGeometrySimulationOutput &storage = node_storage(*node);
  for (NodeSimulationItem &item : MutableSpan(storage.items, storage.items_num)) {
    MEM_SAFE_FREE(item.name);
  }
  MEM_SAFE_FREE(storage.items);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometrySimulationOutput &src_storage = node_storage(*src_node);
  NodeGeometrySimulationOutput *dst_storage = MEM_cnew<NodeGeometrySimulationOutput>(__func__);

  dst_storage->items = MEM_cnew_array<NodeSimulationItem>(src_storage.items_num, __func__);
  dst_storage->items_num = src_storage.items_num;
  dst_storage->active_index = src_storage.active_index;
  dst_storage->next_identifier = src_storage.next_identifier;
  for (const int i : IndexRange(src_storage.items_num)) {
    if (char *name = src_storage.items[i].name) {
      dst_storage->items[i].identifier = src_storage.items[i].identifier;
      dst_storage->items[i].name = BLI_strdup(name);
      dst_storage->items[i].socket_type = src_storage.items[i].socket_type;
      dst_storage->items[i].attribute_domain = src_storage.items[i].attribute_domain;
    }
  }

  dst_node->storage = dst_storage;
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  NodeGeometrySimulationOutput &storage = node_storage(*node);
  if (link->tonode == node) {
    if (link->tosock->identifier == StringRef("__extend__")) {
      if (const NodeSimulationItem *item = NOD_geometry_simulation_output_add_item_from_socket(
              &storage, link->fromnode, link->fromsock))
      {
        update_node_declaration_and_sockets(*ntree, *node);
        link->tosock = nodeFindSocket(
            node, SOCK_IN, socket_identifier_for_simulation_item(*item).c_str());
      }
      else {
        return false;
      }
    }
  }
  else {
    BLI_assert(link->fromnode == node);
    if (link->fromsock->identifier == StringRef("__extend__")) {
      if (const NodeSimulationItem *item = NOD_geometry_simulation_output_add_item_from_socket(
              &storage, link->fromnode, link->tosock))
      {
        update_node_declaration_and_sockets(*ntree, *node);
        link->fromsock = nodeFindSocket(
            node, SOCK_OUT, socket_identifier_for_simulation_item(*item).c_str());
      }
      else {
        return false;
      }
    }
  }
  return true;
}

}  // namespace blender::nodes::node_geo_simulation_output_cc

void register_node_type_geo_simulation_output()
{
  namespace file_ns = blender::nodes::node_geo_simulation_output_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SIMULATION_OUTPUT, "Simulation Output", NODE_CLASS_INTERFACE);
  ntype.initfunc = file_ns::node_init;
  ntype.declare_dynamic = file_ns::node_declare_dynamic;
  ntype.gather_add_node_search_ops = file_ns::search_node_add_ops;
  ntype.gather_link_search_ops = nullptr;
  ntype.insert_link = file_ns::node_insert_link;
  node_type_storage(&ntype,
                    "NodeGeometrySimulationOutput",
                    file_ns::node_free_storage,
                    file_ns::node_copy_storage);
  nodeRegisterType(&ntype);
}

blender::Span<NodeSimulationItem> NodeGeometrySimulationOutput::items_span() const
{
  return blender::Span<NodeSimulationItem>(items, items_num);
}

blender::MutableSpan<NodeSimulationItem> NodeGeometrySimulationOutput::items_span_for_write()
{
  return blender::MutableSpan<NodeSimulationItem>(items, items_num);
}

blender::IndexRange NodeGeometrySimulationOutput::items_range() const
{
  return blender::IndexRange(items_num);
}

bool NOD_geometry_simulation_output_item_socket_type_supported(
    const eNodeSocketDatatype socket_type)
{
  return ELEM(socket_type,
              SOCK_FLOAT,
              SOCK_VECTOR,
              SOCK_RGBA,
              SOCK_BOOLEAN,
              SOCK_ROTATION,
              SOCK_INT,
              SOCK_STRING,
              SOCK_GEOMETRY);
}

bNode *NOD_geometry_simulation_output_find_node_by_item(bNodeTree *ntree,
                                                        const NodeSimulationItem *item)
{
  ntree->ensure_topology_cache();
  for (bNode *node : ntree->nodes_by_type("GeometryNodeSimulationOutput")) {
    NodeGeometrySimulationOutput *sim = static_cast<NodeGeometrySimulationOutput *>(node->storage);
    if (sim->items_span().contains_ptr(item)) {
      return node;
    }
  }
  return nullptr;
}

bool NOD_geometry_simulation_output_item_set_unique_name(NodeGeometrySimulationOutput *sim,
                                                         NodeSimulationItem *item,
                                                         const char *name,
                                                         const char *defname)
{
  char unique_name[MAX_NAME + 4];
  STRNCPY(unique_name, name);

  blender::nodes::SimulationItemsUniqueNameArgs args{sim, item};
  const bool name_changed = BLI_uniquename_cb(blender::nodes::simulation_items_unique_name_check,
                                              &args,
                                              defname,
                                              '.',
                                              unique_name,
                                              ARRAY_SIZE(unique_name));
  item->name = BLI_strdup(unique_name);
  return name_changed;
}

bool NOD_geometry_simulation_output_contains_item(NodeGeometrySimulationOutput *sim,
                                                  const NodeSimulationItem *item)
{
  return sim->items_span().contains_ptr(item);
}

NodeSimulationItem *NOD_geometry_simulation_output_get_active_item(
    NodeGeometrySimulationOutput *sim)
{
  if (!sim->items_range().contains(sim->active_index)) {
    return nullptr;
  }
  return &sim->items[sim->active_index];
}

void NOD_geometry_simulation_output_set_active_item(NodeGeometrySimulationOutput *sim,
                                                    NodeSimulationItem *item)
{
  if (sim->items_span().contains_ptr(item)) {
    sim->active_index = item - sim->items;
  }
}

NodeSimulationItem *NOD_geometry_simulation_output_find_item(NodeGeometrySimulationOutput *sim,
                                                             const char *name)
{
  for (NodeSimulationItem &item : sim->items_span_for_write()) {
    if (STREQ(item.name, name)) {
      return &item;
    }
  }
  return nullptr;
}

NodeSimulationItem *NOD_geometry_simulation_output_add_item(NodeGeometrySimulationOutput *sim,
                                                            const short socket_type,
                                                            const char *name)
{
  return NOD_geometry_simulation_output_insert_item(sim, socket_type, name, sim->items_num);
}

NodeSimulationItem *NOD_geometry_simulation_output_insert_item(NodeGeometrySimulationOutput *sim,
                                                               const short socket_type,
                                                               const char *name,
                                                               int index)
{
  if (!NOD_geometry_simulation_output_item_socket_type_supported(eNodeSocketDatatype(socket_type)))
  {
    return nullptr;
  }

  NodeSimulationItem *old_items = sim->items;
  sim->items = MEM_cnew_array<NodeSimulationItem>(sim->items_num + 1, __func__);
  for (const int i : blender::IndexRange(index)) {
    sim->items[i] = old_items[i];
  }
  for (const int i : blender::IndexRange(index, sim->items_num - index)) {
    sim->items[i + 1] = old_items[i];
  }

  const char *defname = nodeStaticSocketLabel(socket_type, 0);
  NodeSimulationItem &added_item = sim->items[index];
  added_item.identifier = sim->next_identifier++;
  NOD_geometry_simulation_output_item_set_unique_name(sim, &added_item, name, defname);
  added_item.socket_type = socket_type;

  sim->items_num++;
  MEM_SAFE_FREE(old_items);

  return &added_item;
}

NodeSimulationItem *NOD_geometry_simulation_output_add_item_from_socket(
    NodeGeometrySimulationOutput *sim, const bNode * /*from_node*/, const bNodeSocket *from_sock)
{
  return NOD_geometry_simulation_output_insert_item(
      sim, from_sock->type, from_sock->name, sim->items_num);
}

NodeSimulationItem *NOD_geometry_simulation_output_insert_item_from_socket(
    NodeGeometrySimulationOutput *sim,
    const bNode * /*from_node*/,
    const bNodeSocket *from_sock,
    int index)
{
  return NOD_geometry_simulation_output_insert_item(sim, from_sock->type, from_sock->name, index);
}

void NOD_geometry_simulation_output_remove_item(NodeGeometrySimulationOutput *sim,
                                                NodeSimulationItem *item)
{
  const int index = item - sim->items;
  if (index < 0 || index >= sim->items_num) {
    return;
  }

  NodeSimulationItem *old_items = sim->items;
  sim->items = MEM_cnew_array<NodeSimulationItem>(sim->items_num - 1, __func__);
  for (const int i : blender::IndexRange(index)) {
    sim->items[i] = old_items[i];
  }
  for (const int i : blender::IndexRange(index, sim->items_num - index).drop_front(1)) {
    sim->items[i - 1] = old_items[i];
  }

  MEM_SAFE_FREE(old_items[index].name);

  sim->items_num--;
  MEM_SAFE_FREE(old_items);
}

void NOD_geometry_simulation_output_clear_items(NodeGeometrySimulationOutput *sim)
{
  for (NodeSimulationItem &item : sim->items_span_for_write()) {
    MEM_SAFE_FREE(item.name);
  }
  MEM_SAFE_FREE(sim->items);
  sim->items = nullptr;
  sim->items_num = 0;
}

void NOD_geometry_simulation_output_move_item(NodeGeometrySimulationOutput *sim,
                                              int from_index,
                                              int to_index)
{
  BLI_assert(from_index >= 0 && from_index < sim->items_num);
  BLI_assert(to_index >= 0 && to_index < sim->items_num);

  if (from_index == to_index) {
    return;
  }

  if (from_index < to_index) {
    const NodeSimulationItem tmp = sim->items[from_index];
    for (int i = from_index; i < to_index; ++i) {
      sim->items[i] = sim->items[i + 1];
    }
    sim->items[to_index] = tmp;
  }
  else /* from_index > to_index */ {
    const NodeSimulationItem tmp = sim->items[from_index];
    for (int i = from_index; i > to_index; --i) {
      sim->items[i] = sim->items[i - 1];
    }
    sim->items[to_index] = tmp;
  }
}
