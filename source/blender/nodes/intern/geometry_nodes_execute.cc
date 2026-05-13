/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include <cfloat>

#include "BLI_listbase.h"
#include "BLI_math_euler.hh"
#include "BLI_string.h"

#include "NOD_geometry.hh"
#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_execute.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_geometry_nodes_srna.hh"
#include "NOD_menu_value.hh"
#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"

#include "GEO_foreach_geometry.hh"

#include "BKE_geometry_fields.hh"
#include "BKE_geometry_nodes_reference_set.hh"
#include "BKE_geometry_set.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_node_enum.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_socket_value.hh"

#include "FN_lazy_function_execute.hh"

#include "DNA_collection_types.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_vfont_types.h"

#include "RNA_access.hh"

#include "UI_resources.hh"

namespace blender {

namespace lf = fn::lazy_function;

namespace nodes {

bool socket_type_has_attribute_toggle(const eNodeSocketDatatype type)
{
  return socket_type_supports_attributes(type);
}

bool input_has_attribute_toggle(const bNodeTree &node_tree, const int socket_index)
{
  node_tree.ensure_interface_cache();
  const bke::bNodeSocketType *typeinfo =
      node_tree.interface_inputs()[socket_index]->socket_typeinfo();
  if (!typeinfo || !socket_type_has_attribute_toggle(typeinfo->type)) {
    return false;
  }

  BLI_assert(node_tree.runtime->field_inferencing_interface);
  const FieldInferencingInterface &field_interface =
      *node_tree.runtime->field_inferencing_interface;
  return field_interface.inputs[socket_index] != InputSocketFieldType::None;
}

template<typename T>
[[nodiscard]] static std::optional<bke::SocketValueVariant> load_attribute_field_input(
    PointerRNA &input_props_ptr)
{
  const std::string attribute_name = RNA_string_get(&input_props_ptr, "attribute_name");
  if (!bke::allow_procedural_attribute_access(attribute_name)) {
    return std::nullopt;
  }
  return bke::SocketValueVariant::From(bke::AttributeFieldInput::from<T>(attribute_name));
}

template<typename T>
static bke::SocketValueVariant load_data_block_input(const GeoNodesCallData *call_data,
                                                     PointerRNA &input_props_ptr)
{
  PropertyRNA &prop = *RNA_struct_find_property(&input_props_ptr, "value");
  if (RNA_property_type(&prop) == PROP_STRING) {
    if (!call_data) {
      return bke::SocketValueVariant::From(static_cast<T *>(nullptr));
    }
    BLI_assert(call_data->operator_data);
    const std::string name = RNA_string_get(&input_props_ptr, "value");
    const ID *id_orig = call_data->operator_data->input_ids->lookup_default(name, nullptr);
    if (!id_orig) {
      return bke::SocketValueVariant::From(static_cast<T *>(nullptr));
    }
    const ID *id_eval = call_data->operator_data->depsgraphs->get_evaluated_id(*id_orig);
    return bke::SocketValueVariant::From(id_cast<T *>(const_cast<ID *>(id_eval)));
  }

  BLI_assert(RNA_property_type(&prop) == PROP_POINTER);
  T *data_block = id_cast<T *>(RNA_pointer_get(&input_props_ptr, "value").owner_id);
  return bke::SocketValueVariant::From(data_block);
}

static bke::SocketValueVariant init_socket_cpp_value(const GeoNodesCallData *call_data,
                                                     PointerRNA *input_props_ptr,
                                                     const bNodeTreeInterfaceSocket &io_socket)
{
  const bke::bNodeSocketType *stype = io_socket.socket_typeinfo();
  const eNodeSocketDatatype socket_type = stype->type;
  switch (socket_type) {
    case SOCK_FLOAT: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        const float value = RNA_float_get(input_props_ptr, "value");
        return bke::SocketValueVariant(value);
      }
      if (type == GeometryNodesInputType::Attribute) {
        if (std::optional<bke::SocketValueVariant> value = load_attribute_field_input<float>(
                *input_props_ptr))
        {
          return std::move(*value);
        }
      }
      break;
    }
    case SOCK_VECTOR: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        float3 value;
        RNA_float_get_array(input_props_ptr, "value", value);
        return bke::SocketValueVariant(value);
      }
      if (type == GeometryNodesInputType::Attribute) {
        if (std::optional<bke::SocketValueVariant> value = load_attribute_field_input<float3>(
                *input_props_ptr))
        {
          return std::move(*value);
        }
      }
      break;
    }
    case SOCK_RGBA: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        ColorGeometry4f value;
        RNA_float_get_array(input_props_ptr, "value", value);
        return bke::SocketValueVariant(value);
      }
      if (type == GeometryNodesInputType::Attribute) {
        if (std::optional<bke::SocketValueVariant> value =
                load_attribute_field_input<ColorGeometry4f>(*input_props_ptr))
        {
          return std::move(*value);
        }
      }
      break;
    }
    case SOCK_BOOLEAN: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        const bool value = RNA_boolean_get(input_props_ptr, "value");
        return bke::SocketValueVariant(value);
      }
      if (type == GeometryNodesInputType::Attribute) {
        if (std::optional<bke::SocketValueVariant> value = load_attribute_field_input<bool>(
                *input_props_ptr))
        {
          return std::move(*value);
        }
      }
      if (type == GeometryNodesInputType::Layer) {
        const std::string layer_name = RNA_string_get(input_props_ptr, "layer_name");
        return bke::SocketValueVariant::From(
            fn::GField::from_input<bke::NamedLayerSelectionFieldInput>(layer_name));
      }
      break;
    }
    case SOCK_INT: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        const int value = RNA_int_get(input_props_ptr, "value");
        return bke::SocketValueVariant(value);
      }
      if (type == GeometryNodesInputType::Attribute) {
        if (std::optional<bke::SocketValueVariant> value = load_attribute_field_input<int>(
                *input_props_ptr))
        {
          return std::move(*value);
        }
      }
      break;
    }
    case SOCK_ROTATION: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        float3 value_euler;
        RNA_float_get_array(input_props_ptr, "value", value_euler);
        math::Quaternion value_rotation = math::to_quaternion(math::EulerXYZ(value_euler));
        return bke::SocketValueVariant(value_rotation);
      }
      if (type == GeometryNodesInputType::Attribute) {
        if (std::optional<bke::SocketValueVariant> value =
                load_attribute_field_input<math::Quaternion>(*input_props_ptr))
        {
          return std::move(*value);
        }
      }
      break;
    }
    case SOCK_MENU: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        const int value = RNA_enum_get(input_props_ptr, "value");
        return bke::SocketValueVariant::From(MenuValue(value));
      }
      break;
    }
    case SOCK_STRING: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        const std::string value = RNA_string_get(input_props_ptr, "value");
        return bke::SocketValueVariant(value);
      }
      break;
    }
    case SOCK_OBJECT: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        return load_data_block_input<Object>(call_data, *input_props_ptr);
      }
      break;
    }
    case SOCK_IMAGE: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        return load_data_block_input<Image>(call_data, *input_props_ptr);
      }
      break;
    }
    case SOCK_COLLECTION: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        return load_data_block_input<Collection>(call_data, *input_props_ptr);
      }
      break;
    }
    case SOCK_TEXTURE: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        return load_data_block_input<Tex>(call_data, *input_props_ptr);
      }
      break;
    }
    case SOCK_MATERIAL: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        return load_data_block_input<Material>(call_data, *input_props_ptr);
      }
      break;
    }
    case SOCK_FONT: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        return load_data_block_input<VFont>(call_data, *input_props_ptr);
      }
      break;
    }
    case SOCK_SCENE: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        return load_data_block_input<Scene>(call_data, *input_props_ptr);
      }
      break;
    }
    case SOCK_TEXT_ID: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        return load_data_block_input<Text>(call_data, *input_props_ptr);
      }
      break;
    }
    case SOCK_MASK: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        return load_data_block_input<Mask>(call_data, *input_props_ptr);
      }
      break;
    }
    case SOCK_SOUND: {
      const auto type = GeometryNodesInputType(RNA_enum_get(input_props_ptr, "type"));
      if (type == GeometryNodesInputType::Value) {
        return load_data_block_input<bSound>(call_data, *input_props_ptr);
      }
      break;
    }
    case SOCK_GEOMETRY:
    case SOCK_MATRIX:
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
    case SOCK_SHADER:
    case SOCK_CUSTOM:
    case SOCK_INT_VECTOR:
      break;
  }

  return *stype->geometry_nodes_default_value;
}

struct OutputAttributeInfo {
  fn::GField field;
  std::string name;
};

struct OutputAttributeToStore {
  bke::GeometryComponent::Type component_type;
  bke::AttrDomain domain;
  std::string name;
  GMutableSpan data;
};

/**
 * The output attributes are organized based on their domain, because attributes on the same domain
 * can be evaluated together.
 */
static MultiValueMap<bke::AttrDomain, OutputAttributeInfo> find_output_attributes_to_store(
    const bNodeTree &tree, const PointerRNA &properties_ptr, Span<GMutablePointer> output_values)
{
  PropertyRNA *outputs_prop = RNA_struct_find_property(const_cast<PointerRNA *>(&properties_ptr),
                                                       "outputs");
  if (!outputs_prop) {
    return {};
  }
  PointerRNA outputs_ptr = RNA_property_pointer_get(const_cast<PointerRNA *>(&properties_ptr),
                                                    outputs_prop);

  const bNode &output_node = *tree.group_output_node();
  MultiValueMap<bke::AttrDomain, OutputAttributeInfo> outputs_by_domain;
  for (const bNodeSocket *socket : output_node.input_sockets().drop_front(1).drop_back(1)) {
    if (!socket_type_has_attribute_toggle(eNodeSocketDatatype(socket->type))) {
      continue;
    }
    PointerRNA output_props_ptr = RNA_pointer_get(&outputs_ptr, socket->identifier);
    const std::string attribute_name = RNA_string_get(&output_props_ptr, "attribute_name");
    if (attribute_name.empty()) {
      continue;
    }
    if (!bke::allow_procedural_attribute_access(attribute_name)) {
      continue;
    }

    const int index = socket->index();
    bke::SocketValueVariant &value_variant = *output_values[index].get<bke::SocketValueVariant>();
    const fn::GField field = value_variant.get<fn::GField>();

    const bNodeTreeInterfaceSocket *interface_socket = tree.interface_outputs()[index];
    const bke::AttrDomain domain = bke::AttrDomain(interface_socket->attribute_domain);
    OutputAttributeInfo output_info{.field = std::move(field), .name = attribute_name};
    outputs_by_domain.add(domain, std::move(output_info));
  }
  return outputs_by_domain;
}

/**
 * The computed values are stored in newly allocated arrays. They still have to be moved to the
 * actual geometry.
 */
static Vector<OutputAttributeToStore> compute_attributes_to_store(
    const bke::GeometrySet &geometry,
    const MultiValueMap<bke::AttrDomain, OutputAttributeInfo> &outputs_by_domain,
    const Span<const bke::GeometryComponent::Type> component_types)
{
  Vector<OutputAttributeToStore> attributes_to_store;
  for (const auto component_type : component_types) {
    if (!geometry.has(component_type)) {
      continue;
    }
    const bke::GeometryComponent &component = *geometry.get_component(component_type);
    const bke::AttributeAccessor attributes = *component.attributes();
    for (const auto item : outputs_by_domain.items()) {
      const bke::AttrDomain domain = item.key;
      const Span<OutputAttributeInfo> outputs_info = item.value;
      if (!attributes.domain_supported(domain)) {
        continue;
      }
      const int domain_size = attributes.domain_size(domain);
      bke::GeometryFieldContext field_context{component, domain};
      fn::FieldEvaluator field_evaluator{field_context, domain_size};
      for (const OutputAttributeInfo &output_info : outputs_info) {
        const CPPType &type = output_info.field.cpp_type();
        const bke::AttributeValidator validator = attributes.lookup_validator(output_info.name);

        OutputAttributeToStore store{
            component_type,
            domain,
            output_info.name,
            GMutableSpan{
                type,
                MEM_new_uninitialized_aligned(type.size * domain_size, type.alignment, __func__),
                domain_size}};
        fn::GField field = validator.validate_field_if_necessary(output_info.field);
        field_evaluator.add_with_destination(std::move(field), store.data);
        attributes_to_store.append(store);
      }
      field_evaluator.evaluate();
    }
  }
  return attributes_to_store;
}

static void store_computed_output_attributes(
    bke::GeometrySet &geometry, const Span<OutputAttributeToStore> attributes_to_store)
{
  for (const OutputAttributeToStore &store : attributes_to_store) {
    bke::GeometryComponent &component = geometry.get_component_for_write(store.component_type);
    bke::MutableAttributeAccessor attributes = *component.attributes_for_write();

    const bke::AttrType data_type = bke::cpp_type_to_attribute_type(store.data.type());
    const std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(
        store.name);

    /* Attempt to remove the attribute if it already exists but the domain and type don't match.
     * Removing the attribute won't succeed if it is built in and non-removable. */
    if (meta_data.has_value() &&
        (meta_data->domain != store.domain || meta_data->data_type != data_type))
    {
      attributes.remove(store.name);
    }

    /* Try to create the attribute reusing the stored buffer. This will only succeed if the
     * attribute didn't exist before, or if it existed but was removed above. */
    if (attributes.add(store.name,
                       store.domain,
                       bke::cpp_type_to_attribute_type(store.data.type()),
                       bke::AttributeInitMoveArray(store.data.data())))
    {
      continue;
    }

    bke::GAttributeWriter attribute = attributes.lookup_or_add_for_write(
        store.name, store.domain, data_type);
    if (attribute) {
      attribute.varray.set_all(store.data.data());
      attribute.finish();
    }

    /* We were unable to reuse the data, so it must be destructed and freed. */
    store.data.type().destruct_n(store.data.data(), store.data.size());
    MEM_delete_void(store.data.data());
  }
}

static void store_output_attributes(bke::GeometrySet &geometry,
                                    const bNodeTree &tree,
                                    const PointerRNA &properties_ptr,
                                    Span<GMutablePointer> output_values)
{
  /* All new attribute values have to be computed before the geometry is actually changed. This is
   * necessary because some fields might depend on attributes that are overwritten. */
  MultiValueMap<bke::AttrDomain, OutputAttributeInfo> outputs_by_domain =
      find_output_attributes_to_store(tree, properties_ptr, output_values);
  if (outputs_by_domain.size() == 0) {
    return;
  }

  {
    /* Handle top level instances separately first. */
    Vector<OutputAttributeToStore> attributes_to_store = compute_attributes_to_store(
        geometry, outputs_by_domain, {bke::GeometryComponent::Type::Instance});
    store_computed_output_attributes(geometry, attributes_to_store);
  }

  const bool only_instance_attributes = outputs_by_domain.size() == 1 &&
                                        *outputs_by_domain.keys().begin() ==
                                            bke::AttrDomain::Instance;
  if (only_instance_attributes) {
    /* No need to call #foreach_real_geometry when only adding attributes to top-level instances.
     * This avoids some unnecessary data copies currently if some sub-geometries are not yet owned
     * by the geometry set, i.e. they use #GeometryOwnershipType::Editable/ReadOnly. */
    return;
  }

  geometry::foreach_real_geometry(geometry, [&](bke::GeometrySet &instance_geometry) {
    /* Instance attributes should only be created for the top-level geometry. */
    Vector<OutputAttributeToStore> attributes_to_store = compute_attributes_to_store(
        instance_geometry,
        outputs_by_domain,
        {bke::GeometryComponent::Type::Mesh,
         bke::GeometryComponent::Type::PointCloud,
         bke::GeometryComponent::Type::Curve});
    store_computed_output_attributes(instance_geometry, attributes_to_store);
  });
}

bke::GeometrySet execute_geometry_nodes_on_geometry(const bNodeTree &btree,
                                                    const PointerRNA &properties_ptr,
                                                    const ComputeContext &base_compute_context,
                                                    GeoNodesCallData &call_data,
                                                    bke::GeometrySet input_geometry)
{
  const GeometryNodesLazyFunctionGraphInfo &lf_graph_info =
      *ensure_geometry_nodes_lazy_function_graph(btree);
  const GeometryNodesGroupFunction &function = lf_graph_info.function;
  const lf::LazyFunction &lazy_function = *function.function;
  const int num_inputs = lazy_function.inputs().size();
  const int num_outputs = lazy_function.outputs().size();

  Array<GMutablePointer> param_inputs(num_inputs);
  Array<GMutablePointer> param_outputs(num_outputs);
  Array<std::optional<lf::ValueUsage>> param_input_usages(num_inputs);
  Array<lf::ValueUsage> param_output_usages(num_outputs);
  Array<bool> param_set_outputs(num_outputs, false);

  /* We want to evaluate the main outputs, but don't care about which inputs are used for now. */
  param_output_usages.as_mutable_span().slice(function.outputs.main).fill(lf::ValueUsage::Used);
  param_output_usages.as_mutable_span()
      .slice(function.outputs.input_usages)
      .fill(lf::ValueUsage::Unused);

  call_data.call_depth_limit = U.geometry_nodes_stack_limit;

  GeoNodesUserData user_data;
  user_data.call_data = &call_data;
  call_data.root_ntree = &btree;

  user_data.compute_context = &base_compute_context;

  ResourceScope scope;
  LinearAllocator<> &allocator = scope.allocator();

  btree.ensure_interface_cache();

  PointerRNA inputs_ptr = RNA_pointer_get(const_cast<PointerRNA *>(&properties_ptr), "inputs");

  /* Prepare main inputs. */
  for (const int i : btree.interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket &interface_socket = *btree.interface_inputs()[i];
    const bke::bNodeSocketType *typeinfo = interface_socket.socket_typeinfo();
    const eNodeSocketDatatype socket_type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    if (socket_type == SOCK_GEOMETRY && i == 0) {
      bke::SocketValueVariant &value = scope.construct<bke::SocketValueVariant>();
      value.set(std::move(input_geometry));
      param_inputs[function.inputs.main[0]] = &value;
      continue;
    }

    PointerRNA input_props_ptr = RNA_pointer_get(&inputs_ptr, interface_socket.identifier);
    bke::SocketValueVariant value = init_socket_cpp_value(
        &call_data, &input_props_ptr, interface_socket);
    param_inputs[function.inputs.main[i]] = &scope.construct<bke::SocketValueVariant>(
        std::move(value));
  }

  /* Prepare used-outputs inputs. */
  Array<bool> output_used_inputs(btree.interface_outputs().size(), true);
  for (const int i : btree.interface_outputs().index_range()) {
    param_inputs[function.inputs.output_usages[i]] = &output_used_inputs[i];
  }

  /* No anonymous attributes have to be propagated. */
  Array<bke::GeometryNodesReferenceSet> references_to_propagate(
      function.inputs.references_to_propagate.geometry_outputs.size());
  for (const int i : references_to_propagate.index_range()) {
    param_inputs[function.inputs.references_to_propagate.range[i]] = &references_to_propagate[i];
  }

  /* Prepare memory for output values. */
  for (const int i : IndexRange(num_outputs)) {
    const lf::Output &lf_output = lazy_function.outputs()[i];
    const CPPType &type = *lf_output.type;
    void *buffer = allocator.allocate(type);
    param_outputs[i] = {type, buffer};
  }

  GeoNodesLocalUserData local_user_data(user_data);

  lf::Context lf_context(lazy_function.init_storage(allocator), &user_data, &local_user_data);
  lf::BasicParams lf_params{lazy_function,
                            param_inputs,
                            param_outputs,
                            param_input_usages,
                            param_output_usages,
                            param_set_outputs};
  {
    ScopedComputeContextTimer timer{lf_context};
    lazy_function.execute(lf_params, lf_context);
  }
  lazy_function.destruct_storage(lf_context.storage);

  bke::GeometrySet output_geometry =
      param_outputs[0].get<bke::SocketValueVariant>()->extract<bke::GeometrySet>();
  store_output_attributes(output_geometry, btree, properties_ptr, param_outputs);

  for (const int i : IndexRange(num_outputs)) {
    if (param_set_outputs[i]) {
      GMutablePointer &ptr = param_outputs[i];
      ptr.destruct();
    }
  }

  if (output_geometry.has_bundle()) {
    /* Ensure that the bundle data is properly owned by the geometry. Do not call this in the
     * geometry itself because it may just be referenced during modifier evaluation and an
     * unnecessary copy can be avoided. See #GeometryOwnershipType::Editable. */
    output_geometry.bundle_for_write().ensure_owns_direct_data();
  }
  return output_geometry;
}

Vector<InferenceValue> get_geometry_nodes_input_inference_values(const bNodeTree &btree,
                                                                 const PointerRNA &properties_ptr,
                                                                 ResourceScope &scope)
{
  /* Assume that all inputs have unknown values by default. */
  Vector<InferenceValue> inference_values(btree.interface_inputs().size(),
                                          InferenceValue::Unknown());

  PointerRNA inputs_ptr = RNA_pointer_get(const_cast<PointerRNA *>(&properties_ptr), "inputs");

  btree.ensure_interface_cache();
  for (const int input_i : btree.interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket &io_input = *btree.interface_inputs()[input_i];
    const bke::bNodeSocketType *stype = io_input.socket_typeinfo();
    if (!stype) {
      continue;
    }
    if (!stype->base_cpp_type || !stype->geometry_nodes_default_value) {
      continue;
    }
    PointerRNA socket_props_ptr = RNA_pointer_get(&inputs_ptr, io_input.identifier);
    const auto input_type = [&]() {
      if (PropertyRNA *prop = RNA_struct_find_property(&socket_props_ptr, "type")) {
        return GeometryNodesInputType(RNA_property_enum_get(&socket_props_ptr, prop));
      }
      return GeometryNodesInputType::Fallback;
    }();
    if (input_type != GeometryNodesInputType::Value) {
      continue;
    }

    bke::SocketValueVariant &value = scope.add_value(
        init_socket_cpp_value(nullptr, &socket_props_ptr, io_input));
    if (!value.is_single()) {
      continue;
    }
    const GPointer single_value = value.get_single_ptr();
    BLI_assert(single_value.type() == stype->base_cpp_type);
    inference_values[input_i] = InferenceValue::from_primitive(single_value.get());
  }
  return inference_values;
}

}  // namespace nodes
}  // namespace blender
