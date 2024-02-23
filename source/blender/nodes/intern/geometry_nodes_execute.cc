/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include "BLI_math_color.hh"
#include "BLI_math_euler.hh"
#include "BLI_math_quaternion.hh"
#include "BLI_string.h"

#include "NOD_geometry.hh"
#include "NOD_geometry_nodes_execute.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"

#include "BKE_compute_contexts.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"
#include "BKE_idprop.hh"
#include "BKE_node_enum.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_type_conversions.hh"

#include "FN_lazy_function_execute.hh"

#include "UI_resources.hh"

namespace lf = blender::fn::lazy_function;
namespace geo_log = blender::nodes::geo_eval_log;

namespace blender::nodes {

static void add_used_ids_from_sockets(const ListBase &sockets, Set<ID *> &ids)
{
  LISTBASE_FOREACH (const bNodeSocket *, socket, &sockets) {
    switch (socket->type) {
      case SOCK_OBJECT: {
        if (Object *object = ((bNodeSocketValueObject *)socket->default_value)->value) {
          ids.add(reinterpret_cast<ID *>(object));
        }
        break;
      }
      case SOCK_COLLECTION: {
        if (Collection *collection = ((bNodeSocketValueCollection *)socket->default_value)->value)
        {
          ids.add(reinterpret_cast<ID *>(collection));
        }
        break;
      }
      case SOCK_MATERIAL: {
        if (Material *material = ((bNodeSocketValueMaterial *)socket->default_value)->value) {
          ids.add(reinterpret_cast<ID *>(material));
        }
        break;
      }
      case SOCK_TEXTURE: {
        if (Tex *texture = ((bNodeSocketValueTexture *)socket->default_value)->value) {
          ids.add(reinterpret_cast<ID *>(texture));
        }
        break;
      }
      case SOCK_IMAGE: {
        if (Image *image = ((bNodeSocketValueImage *)socket->default_value)->value) {
          ids.add(reinterpret_cast<ID *>(image));
        }
        break;
      }
    }
  }
}

/**
 * \note We can only check properties here that cause the dependency graph to update relations when
 * they are changed, otherwise there may be a missing relation after editing. So this could check
 * more properties like whether the node is muted, but we would have to accept the cost of updating
 * relations when those properties are changed.
 */
static bool node_needs_own_transform_relation(const bNode &node)
{
  if (node.type == GEO_NODE_COLLECTION_INFO) {
    const NodeGeometryCollectionInfo &storage = *static_cast<const NodeGeometryCollectionInfo *>(
        node.storage);
    return storage.transform_space == GEO_NODE_TRANSFORM_SPACE_RELATIVE;
  }

  if (node.type == GEO_NODE_OBJECT_INFO) {
    const NodeGeometryObjectInfo &storage = *static_cast<const NodeGeometryObjectInfo *>(
        node.storage);
    return storage.transform_space == GEO_NODE_TRANSFORM_SPACE_RELATIVE;
  }
  if (node.type == GEO_NODE_SELF_OBJECT) {
    return true;
  }
  if (node.type == GEO_NODE_DEFORM_CURVES_ON_SURFACE) {
    return true;
  }

  return false;
}

static void process_nodes_for_depsgraph(const bNodeTree &tree,
                                        Set<ID *> &ids,
                                        bool &r_needs_own_transform_relation,
                                        bool &r_needs_scene_camera_relation,
                                        Set<const bNodeTree *> &checked_groups)
{
  if (!checked_groups.add(&tree)) {
    return;
  }

  tree.ensure_topology_cache();
  for (const bNode *node : tree.all_nodes()) {
    add_used_ids_from_sockets(node->inputs, ids);
    add_used_ids_from_sockets(node->outputs, ids);
    r_needs_own_transform_relation |= node_needs_own_transform_relation(*node);
    r_needs_scene_camera_relation |= (node->type == GEO_NODE_INPUT_ACTIVE_CAMERA);
  }

  for (const bNode *node : tree.group_nodes()) {
    if (const bNodeTree *sub_tree = reinterpret_cast<const bNodeTree *>(node->id)) {
      process_nodes_for_depsgraph(*sub_tree,
                                  ids,
                                  r_needs_own_transform_relation,
                                  r_needs_scene_camera_relation,
                                  checked_groups);
    }
  }
}

void find_node_tree_dependencies(const bNodeTree &tree,
                                 Set<ID *> &r_ids,
                                 bool &r_needs_own_transform_relation,
                                 bool &r_needs_scene_camera_relation)
{
  Set<const bNodeTree *> checked_groups;
  process_nodes_for_depsgraph(
      tree, r_ids, r_needs_own_transform_relation, r_needs_scene_camera_relation, checked_groups);
}

StringRef input_use_attribute_suffix()
{
  return "_use_attribute";
}

StringRef input_attribute_name_suffix()
{
  return "_attribute_name";
}

bool socket_type_has_attribute_toggle(const eNodeSocketDatatype type)
{
  return socket_type_supports_fields(type);
}

bool input_has_attribute_toggle(const bNodeTree &node_tree, const int socket_index)
{
  node_tree.ensure_interface_cache();
  const bNodeSocketType *typeinfo = node_tree.interface_inputs()[socket_index]->socket_typeinfo();
  if (ELEM(typeinfo->type, SOCK_MENU)) {
    return false;
  }

  BLI_assert(node_tree.runtime->field_inferencing_interface);
  const nodes::FieldInferencingInterface &field_interface =
      *node_tree.runtime->field_inferencing_interface;
  return field_interface.inputs[socket_index] != nodes::InputSocketFieldType::None;
}

static void id_property_int_update_enum_items(const bNodeSocketValueMenu *value,
                                              IDPropertyUIDataInt *ui_data)
{
  int idprop_items_num = 0;
  IDPropertyUIDataEnumItem *idprop_items = nullptr;

  if (value->enum_items && !value->enum_items->items.is_empty()) {
    const Span<bke::RuntimeNodeEnumItem> items = value->enum_items->items;
    idprop_items_num = items.size();
    idprop_items = MEM_cnew_array<IDPropertyUIDataEnumItem>(items.size(), __func__);
    for (const int i : items.index_range()) {
      const bke::RuntimeNodeEnumItem &item = items[i];
      IDPropertyUIDataEnumItem &idprop_item = idprop_items[i];
      idprop_item.value = item.identifier;
      /* TODO: The name may not be unique!
       * We require a unique identifier string for IDProperty and RNA enums,
       * so node enums should probably have this too. */
      idprop_item.identifier = BLI_strdup_null(item.name.c_str());
      idprop_item.name = BLI_strdup_null(item.name.c_str());
      idprop_item.description = BLI_strdup_null(item.description.c_str());
      idprop_item.icon = ICON_NONE;
    }
  }

  /* Fallback: if no items are defined, use a dummy item so the id property is not shown as a plain
   * int value. */
  if (idprop_items_num == 0) {
    idprop_items_num = 1;
    idprop_items = MEM_cnew_array<IDPropertyUIDataEnumItem>(1, __func__);
    idprop_items->value = 0;
    idprop_items->identifier = BLI_strdup("DUMMY");
    idprop_items->name = BLI_strdup("");
    idprop_items->description = BLI_strdup("");
    idprop_items->icon = ICON_NONE;
  }

  /* Node enum definitions should already be valid. */
  BLI_assert(IDP_EnumItemsValidate(idprop_items, idprop_items_num, nullptr));
  ui_data->enum_items = idprop_items;
  ui_data->enum_items_num = idprop_items_num;
}

std::unique_ptr<IDProperty, bke::idprop::IDPropertyDeleter> id_property_create_from_socket(
    const bNodeTreeInterfaceSocket &socket)
{
  const StringRefNull identifier = socket.identifier;
  const bNodeSocketType *typeinfo = socket.socket_typeinfo();
  const eNodeSocketDatatype type = typeinfo ? eNodeSocketDatatype(typeinfo->type) : SOCK_CUSTOM;
  switch (type) {
    case SOCK_FLOAT: {
      const bNodeSocketValueFloat *value = static_cast<const bNodeSocketValueFloat *>(
          socket.socket_data);
      auto property = bke::idprop::create(identifier, value->value);
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(property.get());
      ui_data->base.rna_subtype = value->subtype;
      ui_data->soft_min = double(value->min);
      ui_data->soft_max = double(value->max);
      ui_data->default_value = value->value;
      return property;
    }
    case SOCK_INT: {
      const bNodeSocketValueInt *value = static_cast<const bNodeSocketValueInt *>(
          socket.socket_data);
      auto property = bke::idprop::create(identifier, value->value);
      IDPropertyUIDataInt *ui_data = (IDPropertyUIDataInt *)IDP_ui_data_ensure(property.get());
      ui_data->base.rna_subtype = value->subtype;
      ui_data->soft_min = value->min;
      ui_data->soft_max = value->max;
      ui_data->default_value = value->value;
      return property;
    }
    case SOCK_VECTOR: {
      const bNodeSocketValueVector *value = static_cast<const bNodeSocketValueVector *>(
          socket.socket_data);
      auto property = bke::idprop::create(
          identifier, Span<float>{value->value[0], value->value[1], value->value[2]});
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
          socket.socket_data);
      auto property = bke::idprop::create(
          identifier,
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
      if (is_layer_selection_field(socket)) {
        /* We can't use the value from the socket here since it doesn't storing a string. */
        return bke::idprop::create(identifier, "");
      }
      const bNodeSocketValueBoolean *value = static_cast<const bNodeSocketValueBoolean *>(
          socket.socket_data);
      auto property = bke::idprop::create_bool(identifier, value->value);
      IDPropertyUIDataBool *ui_data = (IDPropertyUIDataBool *)IDP_ui_data_ensure(property.get());
      ui_data->default_value = value->value != 0;
      return property;
    }
    case SOCK_ROTATION: {
      const bNodeSocketValueRotation *value = static_cast<const bNodeSocketValueRotation *>(
          socket.socket_data);
      auto property = bke::idprop::create(
          identifier,
          Span<float>{value->value_euler[0], value->value_euler[1], value->value_euler[2]});
      IDPropertyUIDataFloat *ui_data = reinterpret_cast<IDPropertyUIDataFloat *>(
          IDP_ui_data_ensure(property.get()));
      ui_data->base.rna_subtype = PROP_EULER;
      return property;
    }
    case SOCK_STRING: {
      const bNodeSocketValueString *value = static_cast<const bNodeSocketValueString *>(
          socket.socket_data);
      auto property = bke::idprop::create(identifier, value->value);
      IDPropertyUIDataString *ui_data = (IDPropertyUIDataString *)IDP_ui_data_ensure(
          property.get());
      ui_data->default_value = BLI_strdup(value->value);
      return property;
    }
    case SOCK_MENU: {
      const bNodeSocketValueMenu *value = static_cast<const bNodeSocketValueMenu *>(
          socket.socket_data);
      auto property = bke::idprop::create(identifier, value->value);
      IDPropertyUIDataInt *ui_data = (IDPropertyUIDataInt *)IDP_ui_data_ensure(property.get());
      id_property_int_update_enum_items(value, ui_data);
      return property;
    }
    case SOCK_OBJECT: {
      const bNodeSocketValueObject *value = static_cast<const bNodeSocketValueObject *>(
          socket.socket_data);
      auto property = bke::idprop::create(identifier, reinterpret_cast<ID *>(value->value));
      IDPropertyUIDataID *ui_data = (IDPropertyUIDataID *)IDP_ui_data_ensure(property.get());
      ui_data->id_type = ID_OB;
      return property;
    }
    case SOCK_COLLECTION: {
      const bNodeSocketValueCollection *value = static_cast<const bNodeSocketValueCollection *>(
          socket.socket_data);
      return bke::idprop::create(identifier, reinterpret_cast<ID *>(value->value));
    }
    case SOCK_TEXTURE: {
      const bNodeSocketValueTexture *value = static_cast<const bNodeSocketValueTexture *>(
          socket.socket_data);
      return bke::idprop::create(identifier, reinterpret_cast<ID *>(value->value));
    }
    case SOCK_IMAGE: {
      const bNodeSocketValueImage *value = static_cast<const bNodeSocketValueImage *>(
          socket.socket_data);
      return bke::idprop::create(identifier, reinterpret_cast<ID *>(value->value));
    }
    case SOCK_MATERIAL: {
      const bNodeSocketValueMaterial *value = static_cast<const bNodeSocketValueMaterial *>(
          socket.socket_data);
      return bke::idprop::create(identifier, reinterpret_cast<ID *>(value->value));
    }
    case SOCK_MATRIX:
    case SOCK_CUSTOM:
    case SOCK_GEOMETRY:
    case SOCK_SHADER:
      return nullptr;
  }
  return nullptr;
}

bool id_property_type_matches_socket(const bNodeTreeInterfaceSocket &socket,
                                     const IDProperty &property)
{
  const bNodeSocketType *typeinfo = socket.socket_typeinfo();
  const eNodeSocketDatatype type = typeinfo ? eNodeSocketDatatype(typeinfo->type) : SOCK_CUSTOM;
  switch (type) {
    case SOCK_FLOAT:
      return ELEM(property.type, IDP_FLOAT, IDP_DOUBLE);
    case SOCK_INT:
      return property.type == IDP_INT;
    case SOCK_VECTOR:
    case SOCK_ROTATION:
      return property.type == IDP_ARRAY &&
             ELEM(property.subtype, IDP_INT, IDP_FLOAT, IDP_DOUBLE) && property.len == 3;
    case SOCK_RGBA:
      return property.type == IDP_ARRAY &&
             ELEM(property.subtype, IDP_INT, IDP_FLOAT, IDP_DOUBLE) && property.len == 4;
    case SOCK_BOOLEAN:
      if (is_layer_selection_field(socket)) {
        return property.type == IDP_STRING;
      }
      return property.type == IDP_BOOLEAN;
    case SOCK_STRING:
      return property.type == IDP_STRING;
    case SOCK_MENU:
      return property.type == IDP_INT;
    case SOCK_OBJECT:
    case SOCK_COLLECTION:
    case SOCK_TEXTURE:
    case SOCK_IMAGE:
    case SOCK_MATERIAL:
      return property.type == IDP_ID;
    case SOCK_CUSTOM:
    case SOCK_MATRIX:
    case SOCK_GEOMETRY:
    case SOCK_SHADER:
      return false;
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
      new (r_value) bke::SocketValueVariant(value);
      break;
    }
    case SOCK_INT: {
      int value = IDP_Int(&property);
      new (r_value) bke::SocketValueVariant(value);
      break;
    }
    case SOCK_VECTOR: {
      const void *property_array = IDP_Array(&property);
      float3 value;
      if (property.subtype == IDP_FLOAT) {
        value = float3(static_cast<const float *>(property_array));
      }
      else if (property.subtype == IDP_INT) {
        value = float3(int3(static_cast<const int *>(property_array)));
      }
      else {
        BLI_assert(property.subtype == IDP_DOUBLE);
        value = float3(double3(static_cast<const double *>(property_array)));
      }
      new (r_value) bke::SocketValueVariant(value);
      break;
    }
    case SOCK_RGBA: {
      const void *property_array = IDP_Array(&property);
      float4 vec;
      if (property.subtype == IDP_FLOAT) {
        vec = float4(static_cast<const float *>(property_array));
      }
      else if (property.subtype == IDP_INT) {
        vec = float4(int4(static_cast<const int *>(property_array)));
      }
      else {
        BLI_assert(property.subtype == IDP_DOUBLE);
        vec = float4(double4(static_cast<const double *>(property_array)));
      }
      ColorGeometry4f value(vec);
      new (r_value) bke::SocketValueVariant(value);
      break;
    }
    case SOCK_BOOLEAN: {
      const bool value = IDP_Bool(&property);
      new (r_value) bke::SocketValueVariant(value);
      break;
    }
    case SOCK_ROTATION: {
      const void *property_array = IDP_Array(&property);
      float3 vec;
      if (property.subtype == IDP_FLOAT) {
        vec = float3(static_cast<const float *>(property_array));
      }
      else if (property.subtype == IDP_INT) {
        vec = float3(int3(static_cast<const int *>(property_array)));
      }
      else {
        BLI_assert(property.subtype == IDP_DOUBLE);
        vec = float3(double3(static_cast<const double *>(property_array)));
      }
      const math::EulerXYZ euler_value = math::EulerXYZ(vec);
      new (r_value) bke::SocketValueVariant(math::to_quaternion(euler_value));
      break;
    }
    case SOCK_STRING: {
      std::string value = IDP_String(&property);
      new (r_value) bke::SocketValueVariant(std::move(value));
      break;
    }
    case SOCK_MENU: {
      int value = IDP_Int(&property);
      new (r_value) bke::SocketValueVariant(std::move(value));
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

std::optional<StringRef> input_attribute_name_get(const IDProperty &props,
                                                  const bNodeTreeInterfaceSocket &io_input)
{
  IDProperty *use_attribute = IDP_GetPropertyFromGroup(
      &props, (std::string(io_input.identifier) + input_use_attribute_suffix()).c_str());
  if (!use_attribute) {
    return std::nullopt;
  }
  if (use_attribute->type == IDP_INT) {
    if (IDP_Int(use_attribute) == 0) {
      return std::nullopt;
    }
  }
  if (use_attribute->type == IDP_BOOLEAN) {
    if (!IDP_Bool(use_attribute)) {
      return std::nullopt;
    }
  }

  const IDProperty *property_attribute_name = IDP_GetPropertyFromGroup(
      &props, (io_input.identifier + input_attribute_name_suffix()).c_str());

  return IDP_String(property_attribute_name);
}

static void initialize_group_input(const bNodeTree &tree,
                                   const IDProperty *properties,
                                   const int input_index,
                                   void *r_value)
{
  const bNodeTreeInterfaceSocket &io_input = *tree.interface_inputs()[input_index];
  const bNodeSocketType *typeinfo = io_input.socket_typeinfo();
  const eNodeSocketDatatype socket_data_type = typeinfo ? eNodeSocketDatatype(typeinfo->type) :
                                                          SOCK_CUSTOM;
  if (properties == nullptr) {
    typeinfo->get_geometry_nodes_cpp_value(io_input.socket_data, r_value);
    return;
  }
  const IDProperty *property = IDP_GetPropertyFromGroup(properties, io_input.identifier);
  if (property == nullptr) {
    typeinfo->get_geometry_nodes_cpp_value(io_input.socket_data, r_value);
    return;
  }
  if (!id_property_type_matches_socket(io_input, *property)) {
    typeinfo->get_geometry_nodes_cpp_value(io_input.socket_data, r_value);
    return;
  }

  if (!input_has_attribute_toggle(tree, input_index)) {
    init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
    return;
  }

  const std::optional<StringRef> attribute_name = input_attribute_name_get(*properties, io_input);
  if (attribute_name && bke::allow_procedural_attribute_access(*attribute_name)) {
    fn::GField attribute_field = bke::AttributeFieldInput::Create(*attribute_name,
                                                                  *typeinfo->base_cpp_type);
    new (r_value) bke::SocketValueVariant(std::move(attribute_field));
  }
  else if (is_layer_selection_field(io_input)) {
    const IDProperty *property_layer_name = IDP_GetPropertyFromGroup(properties,
                                                                     io_input.identifier);
    StringRef layer_name = IDP_String(property_layer_name);
    const fn::GField selection_field(
        std::make_shared<bke::NamedLayerSelectionFieldInput>(layer_name), 0);
    new (r_value) bke::SocketValueVariant(std::move(selection_field));
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
  bke::GeometryComponent::Type component_type;
  bke::AttrDomain domain;
  StringRefNull name;
  GMutableSpan data;
};

/**
 * The output attributes are organized based on their domain, because attributes on the same domain
 * can be evaluated together.
 */
static MultiValueMap<bke::AttrDomain, OutputAttributeInfo> find_output_attributes_to_store(
    const bNodeTree &tree, const IDProperty *properties, Span<GMutablePointer> output_values)
{
  const bNode &output_node = *tree.group_output_node();
  MultiValueMap<bke::AttrDomain, OutputAttributeInfo> outputs_by_domain;
  for (const bNodeSocket *socket : output_node.input_sockets().drop_front(1).drop_back(1)) {
    if (!socket_type_has_attribute_toggle(eNodeSocketDatatype(socket->type))) {
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
    bke::SocketValueVariant &value_variant = *output_values[index].get<bke::SocketValueVariant>();
    const fn::GField field = value_variant.extract<fn::GField>();

    const bNodeTreeInterfaceSocket *interface_socket = tree.interface_outputs()[index];
    const bke::AttrDomain domain = bke::AttrDomain(interface_socket->attribute_domain);
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
    const bke::GeometrySet &geometry,
    const MultiValueMap<bke::AttrDomain, OutputAttributeInfo> &outputs_by_domain)
{
  Vector<OutputAttributeToStore> attributes_to_store;
  for (const auto component_type : {bke::GeometryComponent::Type::Mesh,
                                    bke::GeometryComponent::Type::PointCloud,
                                    bke::GeometryComponent::Type::Curve,
                                    bke::GeometryComponent::Type::Instance})
  {
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
                MEM_mallocN_aligned(type.size() * domain_size, type.alignment(), __func__),
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

static void store_output_attributes(bke::GeometrySet &geometry,
                                    const bNodeTree &tree,
                                    const IDProperty *properties,
                                    Span<GMutablePointer> output_values)
{
  /* All new attribute values have to be computed before the geometry is actually changed. This is
   * necessary because some fields might depend on attributes that are overwritten. */
  MultiValueMap<bke::AttrDomain, OutputAttributeInfo> outputs_by_domain =
      find_output_attributes_to_store(tree, properties, output_values);
  Vector<OutputAttributeToStore> attributes_to_store = compute_attributes_to_store(
      geometry, outputs_by_domain);
  store_computed_output_attributes(geometry, attributes_to_store);
}

bke::GeometrySet execute_geometry_nodes_on_geometry(const bNodeTree &btree,
                                                    const IDProperty *properties,
                                                    const ComputeContext &base_compute_context,
                                                    GeoNodesCallData &call_data,
                                                    bke::GeometrySet input_geometry)
{
  const nodes::GeometryNodesLazyFunctionGraphInfo &lf_graph_info =
      *nodes::ensure_geometry_nodes_lazy_function_graph(btree);
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

  nodes::GeoNodesLFUserData user_data;
  user_data.call_data = &call_data;
  call_data.root_ntree = &btree;

  user_data.compute_context = &base_compute_context;

  LinearAllocator<> allocator;
  Vector<GMutablePointer> inputs_to_destruct;

  btree.ensure_interface_cache();

  /* Prepare main inputs. */
  for (const int i : btree.interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket &interface_socket = *btree.interface_inputs()[i];
    const bNodeSocketType *typeinfo = interface_socket.socket_typeinfo();
    const eNodeSocketDatatype socket_type = typeinfo ? eNodeSocketDatatype(typeinfo->type) :
                                                       SOCK_CUSTOM;
    if (socket_type == SOCK_GEOMETRY && i == 0) {
      param_inputs[function.inputs.main[0]] = &input_geometry;
      continue;
    }

    const CPPType *type = typeinfo->geometry_nodes_cpp_type;
    BLI_assert(type != nullptr);
    void *value = allocator.allocate(type->size(), type->alignment());
    initialize_group_input(btree, properties, i, value);
    param_inputs[function.inputs.main[i]] = {type, value};
    inputs_to_destruct.append({type, value});
  }

  /* Prepare used-outputs inputs. */
  Array<bool> output_used_inputs(btree.interface_outputs().size(), true);
  for (const int i : btree.interface_outputs().index_range()) {
    param_inputs[function.inputs.output_usages[i]] = &output_used_inputs[i];
  }

  /* No anonymous attributes have to be propagated. */
  Array<bke::AnonymousAttributeSet> attributes_to_propagate(
      function.inputs.attributes_to_propagate.geometry_outputs.size());
  for (const int i : attributes_to_propagate.index_range()) {
    param_inputs[function.inputs.attributes_to_propagate.range[i]] = &attributes_to_propagate[i];
  }

  /* Prepare memory for output values. */
  for (const int i : IndexRange(num_outputs)) {
    const lf::Output &lf_output = lazy_function.outputs()[i];
    const CPPType &type = *lf_output.type;
    void *buffer = allocator.allocate(type.size(), type.alignment());
    param_outputs[i] = {type, buffer};
  }

  nodes::GeoNodesLFLocalUserData local_user_data(user_data);

  lf::Context lf_context(lazy_function.init_storage(allocator), &user_data, &local_user_data);
  lf::BasicParams lf_params{lazy_function,
                            param_inputs,
                            param_outputs,
                            param_input_usages,
                            param_output_usages,
                            param_set_outputs};
  lazy_function.execute(lf_params, lf_context);
  lazy_function.destruct_storage(lf_context.storage);

  for (GMutablePointer &ptr : inputs_to_destruct) {
    ptr.destruct();
  }

  bke::GeometrySet output_geometry = std::move(*param_outputs[0].get<bke::GeometrySet>());
  store_output_attributes(output_geometry, btree, properties, param_outputs);

  for (const int i : IndexRange(num_outputs)) {
    if (param_set_outputs[i]) {
      GMutablePointer &ptr = param_outputs[i];
      ptr.destruct();
    }
  }

  return output_geometry;
}

void update_input_properties_from_node_tree(const bNodeTree &tree,
                                            const IDProperty *old_properties,
                                            const bool use_bool_for_use_attribute,
                                            IDProperty &properties)
{
  tree.ensure_interface_cache();
  const Span<const bNodeTreeInterfaceSocket *> tree_inputs = tree.interface_inputs();
  for (const int i : tree_inputs.index_range()) {
    const bNodeTreeInterfaceSocket &socket = *tree_inputs[i];
    const StringRefNull socket_identifier = socket.identifier;
    const bNodeSocketType *typeinfo = socket.socket_typeinfo();
    const eNodeSocketDatatype socket_type = typeinfo ? eNodeSocketDatatype(typeinfo->type) :
                                                       SOCK_CUSTOM;
    IDProperty *new_prop = nodes::id_property_create_from_socket(socket).release();
    if (new_prop == nullptr) {
      /* Out of the set of supported input sockets, only
       * geometry sockets aren't added to the modifier. */
      BLI_assert(ELEM(socket_type, SOCK_GEOMETRY, SOCK_MATRIX));
      continue;
    }

    new_prop->flag |= IDP_FLAG_OVERRIDABLE_LIBRARY;
    if (socket.description && socket.description[0] != '\0') {
      IDPropertyUIData *ui_data = IDP_ui_data_ensure(new_prop);
      ui_data->description = BLI_strdup(socket.description);
    }
    IDP_AddToGroup(&properties, new_prop);

    if (old_properties != nullptr) {
      const IDProperty *old_prop = IDP_GetPropertyFromGroup(old_properties,
                                                            socket_identifier.c_str());
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

    if (nodes::socket_type_has_attribute_toggle(eNodeSocketDatatype(socket_type))) {
      const std::string use_attribute_id = socket_identifier + input_use_attribute_suffix();
      const std::string attribute_name_id = socket_identifier + input_attribute_name_suffix();

      IDPropertyTemplate idprop = {0};
      IDProperty *use_attribute_prop = IDP_New(
          use_bool_for_use_attribute ? IDP_BOOLEAN : IDP_INT, &idprop, use_attribute_id.c_str());
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
  const Span<const bNodeTreeInterfaceSocket *> tree_outputs = tree.interface_outputs();
  for (const int i : tree_outputs.index_range()) {
    const bNodeTreeInterfaceSocket &socket = *tree_outputs[i];
    const StringRefNull socket_identifier = socket.identifier;
    const bNodeSocketType *typeinfo = socket.socket_typeinfo();
    const eNodeSocketDatatype socket_type = typeinfo ? eNodeSocketDatatype(typeinfo->type) :
                                                       SOCK_CUSTOM;
    if (!nodes::socket_type_has_attribute_toggle(socket_type)) {
      continue;
    }

    const std::string idprop_name = socket_identifier + input_attribute_name_suffix();
    IDProperty *new_prop = IDP_NewStringMaxSize("", MAX_NAME, idprop_name.c_str());
    if (socket.description && socket.description[0] != '\0') {
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
