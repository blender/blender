/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include "NOD_geometry_nodes_execute.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_node_declaration.hh"

#include "BKE_compute_contexts.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"
#include "BKE_idprop.hh"
#include "BKE_node_runtime.hh"
#include "BKE_type_conversions.hh"

#include "FN_field_cpp_type.hh"
#include "FN_lazy_function_execute.hh"

namespace lf = blender::fn::lazy_function;
namespace geo_log = blender::nodes::geo_eval_log;

namespace blender::nodes {

StringRef input_use_attribute_suffix()
{
  return "_use_attribute";
}

StringRef input_attribute_name_suffix()
{
  return "_attribute_name";
}

bool socket_type_has_attribute_toggle(const bNodeSocket &socket)
{
  return ELEM(socket.type, SOCK_FLOAT, SOCK_VECTOR, SOCK_BOOLEAN, SOCK_RGBA, SOCK_INT);
}

bool input_has_attribute_toggle(const bNodeTree &node_tree, const int socket_index)
{
  BLI_assert(node_tree.runtime->field_inferencing_interface);
  const nodes::FieldInferencingInterface &field_interface =
      *node_tree.runtime->field_inferencing_interface;
  return field_interface.inputs[socket_index] != nodes::InputSocketFieldType::None;
}

std::unique_ptr<IDProperty, bke::idprop::IDPropertyDeleter> id_property_create_from_socket(
    const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT: {
      const bNodeSocketValueFloat *value = static_cast<const bNodeSocketValueFloat *>(
          socket.default_value);
      auto property = bke::idprop::create(socket.identifier, value->value);
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(property.get());
      ui_data->base.rna_subtype = value->subtype;
      ui_data->soft_min = double(value->min);
      ui_data->soft_max = double(value->max);
      ui_data->default_value = value->value;
      return property;
    }
    case SOCK_INT: {
      const bNodeSocketValueInt *value = static_cast<const bNodeSocketValueInt *>(
          socket.default_value);
      auto property = bke::idprop::create(socket.identifier, value->value);
      IDPropertyUIDataInt *ui_data = (IDPropertyUIDataInt *)IDP_ui_data_ensure(property.get());
      ui_data->base.rna_subtype = value->subtype;
      ui_data->soft_min = value->min;
      ui_data->soft_max = value->max;
      ui_data->default_value = value->value;
      return property;
    }
    case SOCK_VECTOR: {
      const bNodeSocketValueVector *value = static_cast<const bNodeSocketValueVector *>(
          socket.default_value);
      auto property = bke::idprop::create(
          socket.identifier, Span<float>{value->value[0], value->value[1], value->value[2]});
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(property.get());
      ui_data->base.rna_subtype = value->subtype;
      ui_data->soft_min = double(value->min);
      ui_data->soft_max = double(value->max);
      ui_data->default_array = (double *)MEM_mallocN(sizeof(double[3]), "mod_prop_default");
      ui_data->default_array_len = 3;
      for (const int i : IndexRange(3)) {
        ui_data->default_array[i] = double(value->value[i]);
      }
      return property;
    }
    case SOCK_RGBA: {
      const bNodeSocketValueRGBA *value = static_cast<const bNodeSocketValueRGBA *>(
          socket.default_value);
      auto property = bke::idprop::create(
          socket.identifier,
          Span<float>{value->value[0], value->value[1], value->value[2], value->value[3]});
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(property.get());
      ui_data->base.rna_subtype = PROP_COLOR;
      ui_data->default_array = (double *)MEM_mallocN(sizeof(double[4]), __func__);
      ui_data->default_array_len = 4;
      ui_data->min = 0.0;
      ui_data->max = FLT_MAX;
      ui_data->soft_min = 0.0;
      ui_data->soft_max = 1.0;
      for (const int i : IndexRange(4)) {
        ui_data->default_array[i] = double(value->value[i]);
      }
      return property;
    }
    case SOCK_BOOLEAN: {
      const bNodeSocketValueBoolean *value = static_cast<const bNodeSocketValueBoolean *>(
          socket.default_value);
      auto property = bke::idprop::create_bool(socket.identifier, value->value);
      IDPropertyUIDataBool *ui_data = (IDPropertyUIDataBool *)IDP_ui_data_ensure(property.get());
      ui_data->default_value = value->value != 0;
      return property;
    }
    case SOCK_STRING: {
      const bNodeSocketValueString *value = static_cast<const bNodeSocketValueString *>(
          socket.default_value);
      auto property = bke::idprop::create(socket.identifier, value->value);
      IDPropertyUIDataString *ui_data = (IDPropertyUIDataString *)IDP_ui_data_ensure(
          property.get());
      ui_data->default_value = BLI_strdup(value->value);
      return property;
    }
    case SOCK_OBJECT: {
      const bNodeSocketValueObject *value = static_cast<const bNodeSocketValueObject *>(
          socket.default_value);
      auto property = bke::idprop::create(socket.identifier, reinterpret_cast<ID *>(value->value));
      IDPropertyUIDataID *ui_data = (IDPropertyUIDataID *)IDP_ui_data_ensure(property.get());
      ui_data->id_type = ID_OB;
      return property;
    }
    case SOCK_COLLECTION: {
      const bNodeSocketValueCollection *value = static_cast<const bNodeSocketValueCollection *>(
          socket.default_value);
      return bke::idprop::create(socket.identifier, reinterpret_cast<ID *>(value->value));
    }
    case SOCK_TEXTURE: {
      const bNodeSocketValueTexture *value = static_cast<const bNodeSocketValueTexture *>(
          socket.default_value);
      return bke::idprop::create(socket.identifier, reinterpret_cast<ID *>(value->value));
    }
    case SOCK_IMAGE: {
      const bNodeSocketValueImage *value = static_cast<const bNodeSocketValueImage *>(
          socket.default_value);
      return bke::idprop::create(socket.identifier, reinterpret_cast<ID *>(value->value));
    }
    case SOCK_MATERIAL: {
      const bNodeSocketValueMaterial *value = static_cast<const bNodeSocketValueMaterial *>(
          socket.default_value);
      return bke::idprop::create(socket.identifier, reinterpret_cast<ID *>(value->value));
    }
  }
  return nullptr;
}

bool id_property_type_matches_socket(const bNodeSocket &socket, const IDProperty &property)
{
  switch (socket.type) {
    case SOCK_FLOAT:
      return ELEM(property.type, IDP_FLOAT, IDP_DOUBLE);
    case SOCK_INT:
      return property.type == IDP_INT;
    case SOCK_VECTOR:
      return property.type == IDP_ARRAY && property.subtype == IDP_FLOAT && property.len == 3;
    case SOCK_RGBA:
      return property.type == IDP_ARRAY && property.subtype == IDP_FLOAT && property.len == 4;
    case SOCK_BOOLEAN:
      return property.type == IDP_BOOLEAN;
    case SOCK_STRING:
      return property.type == IDP_STRING;
    case SOCK_OBJECT:
    case SOCK_COLLECTION:
    case SOCK_TEXTURE:
    case SOCK_IMAGE:
    case SOCK_MATERIAL:
      return property.type == IDP_ID;
  }
  BLI_assert_unreachable();
  return false;
}

static void init_socket_cpp_value_from_property(const IDProperty &property,
                                                const eNodeSocketDatatype socket_value_type,
                                                void *r_value)
{
  switch (socket_value_type) {
    case SOCK_FLOAT: {
      float value = 0.0f;
      if (property.type == IDP_FLOAT) {
        value = IDP_Float(&property);
      }
      else if (property.type == IDP_DOUBLE) {
        value = float(IDP_Double(&property));
      }
      new (r_value) fn::ValueOrField<float>(value);
      break;
    }
    case SOCK_INT: {
      int value = IDP_Int(&property);
      new (r_value) fn::ValueOrField<int>(value);
      break;
    }
    case SOCK_VECTOR: {
      float3 value = (const float *)IDP_Array(&property);
      new (r_value) fn::ValueOrField<float3>(value);
      break;
    }
    case SOCK_RGBA: {
      ColorGeometry4f value = (const float *)IDP_Array(&property);
      new (r_value) fn::ValueOrField<ColorGeometry4f>(value);
      break;
    }
    case SOCK_BOOLEAN: {
      const bool value = IDP_Bool(&property);
      new (r_value) fn::ValueOrField<bool>(value);
      break;
    }
    case SOCK_STRING: {
      std::string value = IDP_String(&property);
      new (r_value) fn::ValueOrField<std::string>(std::move(value));
      break;
    }
    case SOCK_OBJECT: {
      ID *id = IDP_Id(&property);
      Object *object = (id && GS(id->name) == ID_OB) ? (Object *)id : nullptr;
      *(Object **)r_value = object;
      break;
    }
    case SOCK_COLLECTION: {
      ID *id = IDP_Id(&property);
      Collection *collection = (id && GS(id->name) == ID_GR) ? (Collection *)id : nullptr;
      *(Collection **)r_value = collection;
      break;
    }
    case SOCK_TEXTURE: {
      ID *id = IDP_Id(&property);
      Tex *texture = (id && GS(id->name) == ID_TE) ? (Tex *)id : nullptr;
      *(Tex **)r_value = texture;
      break;
    }
    case SOCK_IMAGE: {
      ID *id = IDP_Id(&property);
      Image *image = (id && GS(id->name) == ID_IM) ? (Image *)id : nullptr;
      *(Image **)r_value = image;
      break;
    }
    case SOCK_MATERIAL: {
      ID *id = IDP_Id(&property);
      Material *material = (id && GS(id->name) == ID_MA) ? (Material *)id : nullptr;
      *(Material **)r_value = material;
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }
}

static void initialize_group_input(const bNodeTree &tree,
                                   const IDProperty *properties,
                                   const int input_index,
                                   void *r_value)
{
  const bNodeSocket &io_input = *tree.interface_inputs()[input_index];
  const bNodeSocketType &socket_type = *io_input.typeinfo;
  const eNodeSocketDatatype socket_data_type = static_cast<eNodeSocketDatatype>(io_input.type);
  if (properties == nullptr) {
    socket_type.get_geometry_nodes_cpp_value(io_input, r_value);
    return;
  }
  const IDProperty *property = IDP_GetPropertyFromGroup(properties, io_input.identifier);
  if (property == nullptr) {
    socket_type.get_geometry_nodes_cpp_value(io_input, r_value);
    return;
  }
  if (!id_property_type_matches_socket(io_input, *property)) {
    socket_type.get_geometry_nodes_cpp_value(io_input, r_value);
    return;
  }

  if (!input_has_attribute_toggle(tree, input_index)) {
    init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
    return;
  }

  const IDProperty *property_use_attribute = IDP_GetPropertyFromGroup(
      properties, (io_input.identifier + input_use_attribute_suffix()).c_str());
  const IDProperty *property_attribute_name = IDP_GetPropertyFromGroup(
      properties, (io_input.identifier + input_attribute_name_suffix()).c_str());
  if (property_use_attribute == nullptr || property_attribute_name == nullptr) {
    init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
    return;
  }

  const bool use_attribute = IDP_Int(property_use_attribute) != 0;
  if (use_attribute) {
    const StringRef attribute_name{IDP_String(property_attribute_name)};
    if (!bke::allow_procedural_attribute_access(attribute_name)) {
      init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
      return;
    }
    fn::GField attribute_field = bke::AttributeFieldInput::Create(attribute_name,
                                                                  *socket_type.base_cpp_type);
    const auto *value_or_field_cpp_type = fn::ValueOrFieldCPPType::get_from_self(
        *socket_type.geometry_nodes_cpp_type);
    BLI_assert(value_or_field_cpp_type != nullptr);
    value_or_field_cpp_type->construct_from_field(r_value, std::move(attribute_field));
  }
  else {
    init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
  }
}

struct OutputAttributeInfo {
  fn::GField field;
  StringRefNull name;
};

struct OutputAttributeToStore {
  GeometryComponentType component_type;
  eAttrDomain domain;
  StringRefNull name;
  GMutableSpan data;
};

/**
 * The output attributes are organized based on their domain, because attributes on the same domain
 * can be evaluated together.
 */
static MultiValueMap<eAttrDomain, OutputAttributeInfo> find_output_attributes_to_store(
    const bNodeTree &tree, const IDProperty *properties, Span<GMutablePointer> output_values)
{
  const bNode &output_node = *tree.group_output_node();
  MultiValueMap<eAttrDomain, OutputAttributeInfo> outputs_by_domain;
  for (const bNodeSocket *socket : output_node.input_sockets().drop_front(1).drop_back(1)) {
    if (!socket_type_has_attribute_toggle(*socket)) {
      continue;
    }

    const std::string prop_name = socket->identifier + input_attribute_name_suffix();
    const IDProperty *prop = IDP_GetPropertyFromGroup(properties, prop_name.c_str());
    if (prop == nullptr) {
      continue;
    }
    const StringRefNull attribute_name = IDP_String(prop);
    if (attribute_name.is_empty()) {
      continue;
    }
    if (!bke::allow_procedural_attribute_access(attribute_name)) {
      continue;
    }

    const int index = socket->index();
    const GPointer value = output_values[index];
    const auto *value_or_field_type = fn::ValueOrFieldCPPType::get_from_self(*value.type());
    BLI_assert(value_or_field_type != nullptr);
    const fn::GField field = value_or_field_type->as_field(value.get());

    const bNodeSocket *interface_socket = (const bNodeSocket *)BLI_findlink(&tree.outputs, index);
    const eAttrDomain domain = (eAttrDomain)interface_socket->attribute_domain;
    OutputAttributeInfo output_info;
    output_info.field = std::move(field);
    output_info.name = attribute_name;
    outputs_by_domain.add(domain, std::move(output_info));
  }
  return outputs_by_domain;
}

/**
 * The computed values are stored in newly allocated arrays. They still have to be moved to the
 * actual geometry.
 */
static Vector<OutputAttributeToStore> compute_attributes_to_store(
    const GeometrySet &geometry,
    const MultiValueMap<eAttrDomain, OutputAttributeInfo> &outputs_by_domain)
{
  Vector<OutputAttributeToStore> attributes_to_store;
  for (const GeometryComponentType component_type : {GEO_COMPONENT_TYPE_MESH,
                                                     GEO_COMPONENT_TYPE_POINT_CLOUD,
                                                     GEO_COMPONENT_TYPE_CURVE,
                                                     GEO_COMPONENT_TYPE_INSTANCES})
  {
    if (!geometry.has(component_type)) {
      continue;
    }
    const GeometryComponent &component = *geometry.get_component_for_read(component_type);
    const bke::AttributeAccessor attributes = *component.attributes();
    for (const auto item : outputs_by_domain.items()) {
      const eAttrDomain domain = item.key;
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
                type, MEM_malloc_arrayN(domain_size, type.size(), __func__), domain_size}};
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
    GeometrySet &geometry, const Span<OutputAttributeToStore> attributes_to_store)
{
  for (const OutputAttributeToStore &store : attributes_to_store) {
    GeometryComponent &component = geometry.get_component_for_write(store.component_type);
    bke::MutableAttributeAccessor attributes = *component.attributes_for_write();

    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(store.data.type());
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
                       bke::cpp_type_to_custom_data_type(store.data.type()),
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
    MEM_freeN(store.data.data());
  }
}

static void store_output_attributes(GeometrySet &geometry,
                                    const bNodeTree &tree,
                                    const IDProperty *properties,
                                    Span<GMutablePointer> output_values)
{
  /* All new attribute values have to be computed before the geometry is actually changed. This is
   * necessary because some fields might depend on attributes that are overwritten. */
  MultiValueMap<eAttrDomain, OutputAttributeInfo> outputs_by_domain =
      find_output_attributes_to_store(tree, properties, output_values);
  Vector<OutputAttributeToStore> attributes_to_store = compute_attributes_to_store(
      geometry, outputs_by_domain);
  store_computed_output_attributes(geometry, attributes_to_store);
}

GeometrySet execute_geometry_nodes_on_geometry(
    const bNodeTree &btree,
    const IDProperty *properties,
    const ComputeContext &base_compute_context,
    GeometrySet input_geometry,
    const FunctionRef<void(nodes::GeoNodesLFUserData &)> fill_user_data)
{
  const nodes::GeometryNodesLazyFunctionGraphInfo &lf_graph_info =
      *nodes::ensure_geometry_nodes_lazy_function_graph(btree);
  const nodes::GeometryNodeLazyFunctionGraphMapping &mapping = lf_graph_info.mapping;

  Vector<const lf::OutputSocket *> graph_inputs = mapping.group_input_sockets;
  graph_inputs.extend(mapping.group_output_used_sockets);
  graph_inputs.extend(mapping.attribute_set_by_geometry_output.values().begin(),
                      mapping.attribute_set_by_geometry_output.values().end());
  Vector<const lf::InputSocket *> graph_outputs = mapping.standard_group_output_sockets;

  Array<GMutablePointer> param_inputs(graph_inputs.size());
  Array<GMutablePointer> param_outputs(graph_outputs.size());
  Array<std::optional<lf::ValueUsage>> param_input_usages(graph_inputs.size());
  Array<lf::ValueUsage> param_output_usages(graph_outputs.size(), lf::ValueUsage::Used);
  Array<bool> param_set_outputs(graph_outputs.size(), false);

  nodes::GeometryNodesLazyFunctionLogger lf_logger(lf_graph_info);
  nodes::GeometryNodesLazyFunctionSideEffectProvider lf_side_effect_provider;

  lf::GraphExecutor graph_executor{
      lf_graph_info.graph, graph_inputs, graph_outputs, &lf_logger, &lf_side_effect_provider};

  nodes::GeoNodesLFUserData user_data;
  fill_user_data(user_data);
  user_data.compute_context = &base_compute_context;

  LinearAllocator<> allocator;
  Vector<GMutablePointer> inputs_to_destruct;

  int input_index = -1;
  for (const int i : btree.interface_inputs().index_range()) {
    input_index++;
    const bNodeSocket &interface_socket = *btree.interface_inputs()[i];
    if (interface_socket.type == SOCK_GEOMETRY && input_index == 0) {
      param_inputs[input_index] = &input_geometry;
      continue;
    }

    const CPPType *type = interface_socket.typeinfo->geometry_nodes_cpp_type;
    BLI_assert(type != nullptr);
    void *value = allocator.allocate(type->size(), type->alignment());
    initialize_group_input(btree, properties, i, value);
    param_inputs[input_index] = {type, value};
    inputs_to_destruct.append({type, value});
  }

  Array<bool> output_used_inputs(btree.interface_outputs().size(), true);
  for (const int i : btree.interface_outputs().index_range()) {
    input_index++;
    param_inputs[input_index] = &output_used_inputs[i];
  }

  Array<bke::AnonymousAttributeSet> attributes_to_propagate(
      mapping.attribute_set_by_geometry_output.size());
  for (const int i : attributes_to_propagate.index_range()) {
    input_index++;
    param_inputs[input_index] = &attributes_to_propagate[i];
  }

  for (const int i : graph_outputs.index_range()) {
    const lf::InputSocket &socket = *graph_outputs[i];
    const CPPType &type = socket.type();
    void *buffer = allocator.allocate(type.size(), type.alignment());
    param_outputs[i] = {type, buffer};
  }

  nodes::GeoNodesLFLocalUserData local_user_data(user_data);

  lf::Context lf_context(graph_executor.init_storage(allocator), &user_data, &local_user_data);
  lf::BasicParams lf_params{graph_executor,
                            param_inputs,
                            param_outputs,
                            param_input_usages,
                            param_output_usages,
                            param_set_outputs};
  graph_executor.execute(lf_params, lf_context);
  graph_executor.destruct_storage(lf_context.storage);

  for (GMutablePointer &ptr : inputs_to_destruct) {
    ptr.destruct();
  }

  GeometrySet output_geometry = std::move(*param_outputs[0].get<GeometrySet>());
  store_output_attributes(output_geometry, btree, properties, param_outputs);

  for (GMutablePointer &ptr : param_outputs) {
    ptr.destruct();
  }

  return output_geometry;
}

void update_input_properties_from_node_tree(const bNodeTree &tree,
                                            const IDProperty *old_properties,
                                            IDProperty &properties)
{
  tree.ensure_topology_cache();
  const Span<const bNodeSocket *> tree_inputs = tree.interface_inputs();
  for (const int i : tree_inputs.index_range()) {
    const bNodeSocket &socket = *tree_inputs[i];
    IDProperty *new_prop = nodes::id_property_create_from_socket(socket).release();
    if (new_prop == nullptr) {
      /* Out of the set of supported input sockets, only
       * geometry sockets aren't added to the modifier. */
      BLI_assert(socket.type == SOCK_GEOMETRY);
      continue;
    }

    new_prop->flag |= IDP_FLAG_OVERRIDABLE_LIBRARY;
    if (socket.description[0] != '\0') {
      IDPropertyUIData *ui_data = IDP_ui_data_ensure(new_prop);
      ui_data->description = BLI_strdup(socket.description);
    }
    IDP_AddToGroup(&properties, new_prop);

    if (old_properties != nullptr) {
      const IDProperty *old_prop = IDP_GetPropertyFromGroup(old_properties, socket.identifier);
      if (old_prop != nullptr) {
        if (nodes::id_property_type_matches_socket(socket, *old_prop)) {
          /* #IDP_CopyPropertyContent replaces the UI data as well, which we don't (we only
           * want to replace the values). So release it temporarily and replace it after. */
          IDPropertyUIData *ui_data = new_prop->ui_data;
          new_prop->ui_data = nullptr;
          IDP_CopyPropertyContent(new_prop, old_prop);
          if (new_prop->ui_data != nullptr) {
            IDP_ui_data_free(new_prop);
          }
          new_prop->ui_data = ui_data;
        }
        else if (old_prop->type == IDP_INT && new_prop->type == IDP_BOOLEAN) {
          /* Support versioning from integer to boolean property values. The actual value is stored
           * in the same variable for both types. */
          new_prop->data.val = old_prop->data.val != 0;
        }
      }
    }

    if (nodes::socket_type_has_attribute_toggle(socket)) {
      const std::string use_attribute_id = socket.identifier + input_use_attribute_suffix();
      const std::string attribute_name_id = socket.identifier + input_attribute_name_suffix();

      IDPropertyTemplate idprop = {0};
      IDProperty *use_attribute_prop = IDP_New(IDP_INT, &idprop, use_attribute_id.c_str());
      IDP_AddToGroup(&properties, use_attribute_prop);

      IDProperty *attribute_prop = IDP_New(IDP_STRING, &idprop, attribute_name_id.c_str());
      IDP_AddToGroup(&properties, attribute_prop);

      if (old_properties == nullptr) {
        if (socket.default_attribute_name && socket.default_attribute_name[0] != '\0') {
          IDP_AssignStringMaxSize(attribute_prop, socket.default_attribute_name, MAX_NAME);
          IDP_Int(use_attribute_prop) = 1;
        }
      }
      else {
        IDProperty *old_prop_use_attribute = IDP_GetPropertyFromGroup(old_properties,
                                                                      use_attribute_id.c_str());
        if (old_prop_use_attribute != nullptr) {
          IDP_CopyPropertyContent(use_attribute_prop, old_prop_use_attribute);
        }

        IDProperty *old_attribute_name_prop = IDP_GetPropertyFromGroup(old_properties,
                                                                       attribute_name_id.c_str());
        if (old_attribute_name_prop != nullptr) {
          IDP_CopyPropertyContent(attribute_prop, old_attribute_name_prop);
        }
      }
    }
  }
}

void update_output_properties_from_node_tree(const bNodeTree &tree,
                                             const IDProperty *old_properties,
                                             IDProperty &properties)
{
  tree.ensure_topology_cache();
  const Span<const bNodeSocket *> tree_outputs = tree.interface_outputs();
  for (const int i : tree_outputs.index_range()) {
    const bNodeSocket &socket = *tree_outputs[i];
    if (!nodes::socket_type_has_attribute_toggle(socket)) {
      continue;
    }

    const std::string idprop_name = socket.identifier + input_attribute_name_suffix();
    IDProperty *new_prop = IDP_NewStringMaxSize("", idprop_name.c_str(), MAX_NAME);
    if (socket.description[0] != '\0') {
      IDPropertyUIData *ui_data = IDP_ui_data_ensure(new_prop);
      ui_data->description = BLI_strdup(socket.description);
    }
    IDP_AddToGroup(&properties, new_prop);

    if (old_properties == nullptr) {
      if (socket.default_attribute_name && socket.default_attribute_name[0] != '\0') {
        IDP_AssignStringMaxSize(new_prop, socket.default_attribute_name, MAX_NAME);
      }
    }
    else {
      IDProperty *old_prop = IDP_GetPropertyFromGroup(old_properties, idprop_name.c_str());
      if (old_prop != nullptr) {
        /* #IDP_CopyPropertyContent replaces the UI data as well, which we don't (we only
         * want to replace the values). So release it temporarily and replace it after. */
        IDPropertyUIData *ui_data = new_prop->ui_data;
        new_prop->ui_data = nullptr;
        IDP_CopyPropertyContent(new_prop, old_prop);
        if (new_prop->ui_data != nullptr) {
          IDP_ui_data_free(new_prop);
        }
        new_prop->ui_data = ui_data;
      }
    }
  }
}

}  // namespace blender::nodes
