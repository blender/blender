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
#include "NOD_geometry_nodes_execute.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
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

#include "UI_resources.hh"

namespace lf = blender::fn::lazy_function;
namespace geo_log = blender::nodes::geo_eval_log;

namespace blender::nodes {

bool socket_type_has_attribute_toggle(const eNodeSocketDatatype type)
{
  return socket_type_supports_fields(type);
}

bool input_has_attribute_toggle(const bNodeTree &node_tree, const int socket_index)
{
  node_tree.ensure_interface_cache();
  const bke::bNodeSocketType *typeinfo =
      node_tree.interface_inputs()[socket_index]->socket_typeinfo();
  if (ELEM(typeinfo->type, SOCK_MENU)) {
    return false;
  }

  BLI_assert(node_tree.runtime->field_inferencing_interface);
  const FieldInferencingInterface &field_interface =
      *node_tree.runtime->field_inferencing_interface;
  return field_interface.inputs[socket_index] != InputSocketFieldType::None;
}

static void id_property_int_update_enum_items(const bNodeSocketValueMenu *value,
                                              IDPropertyUIDataInt *ui_data)
{
  int idprop_items_num = 0;
  IDPropertyUIDataEnumItem *idprop_items = nullptr;

  if (value->enum_items && !value->enum_items->items.is_empty()) {
    const Span<bke::RuntimeNodeEnumItem> items = value->enum_items->items;
    idprop_items_num = items.size();
    idprop_items = MEM_calloc_arrayN<IDPropertyUIDataEnumItem>(items.size(), __func__);
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
    idprop_items = MEM_calloc_arrayN<IDPropertyUIDataEnumItem>(1, __func__);
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

static std::unique_ptr<IDProperty, bke::idprop::IDPropertyDeleter> id_name_or_value_prop(
    const StringRefNull identifier, ID *id, const ID_Type id_type, const bool use_name_for_ids)
{
  if (use_name_for_ids) {
    return bke::idprop::create(identifier, id ? id->name + 2 : "");
  }
  auto prop = bke::idprop::create(identifier, id);
  IDPropertyUIDataID *ui_data = (IDPropertyUIDataID *)IDP_ui_data_ensure(prop.get());
  ui_data->id_type = id_type;
  return prop;
}

std::unique_ptr<IDProperty, bke::idprop::IDPropertyDeleter> id_property_create_from_socket(
    const bNodeTreeInterfaceSocket &socket,
    const nodes::StructureType structure_type,
    const bool use_name_for_ids)
{
  if (structure_type == StructureType::Grid) {
    /* Grids currently aren't exposed as properties. */
    return nullptr;
  }
  const StringRefNull identifier = socket.identifier;
  const bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
  const eNodeSocketDatatype type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
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
          identifier,
          Span<float>{value->value[0], value->value[1], value->value[2], value->value[3]}
              .take_front(value->dimensions));
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(property.get());
      ui_data->base.rna_subtype = value->subtype;
      ui_data->soft_min = double(value->min);
      ui_data->soft_max = double(value->max);
      ui_data->default_array = MEM_malloc_arrayN<double>(value->dimensions, "mod_prop_default");
      ui_data->default_array_len = value->dimensions;
      for (const int i : IndexRange(value->dimensions)) {
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
      ui_data->default_array = MEM_malloc_arrayN<double>(4, __func__);
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
      ui_data->base.rna_subtype = value->subtype;
      return property;
    }
    case SOCK_MENU: {
      const bNodeSocketValueMenu *value = static_cast<const bNodeSocketValueMenu *>(
          socket.socket_data);
      auto property = bke::idprop::create(identifier, value->value);
      IDPropertyUIDataInt *ui_data = (IDPropertyUIDataInt *)IDP_ui_data_ensure(property.get());
      id_property_int_update_enum_items(value, ui_data);
      ui_data->default_value = value->value;
      return property;
    }
    case SOCK_OBJECT: {
      const bNodeSocketValueObject *value = static_cast<const bNodeSocketValueObject *>(
          socket.socket_data);
      ID *id = reinterpret_cast<ID *>(value->value);
      auto property = id_name_or_value_prop(identifier, id, ID_OB, use_name_for_ids);
      return property;
    }
    case SOCK_COLLECTION: {
      const bNodeSocketValueCollection *value = static_cast<const bNodeSocketValueCollection *>(
          socket.socket_data);
      ID *id = reinterpret_cast<ID *>(value->value);
      return id_name_or_value_prop(identifier, id, ID_GR, use_name_for_ids);
    }
    case SOCK_TEXTURE: {
      const bNodeSocketValueTexture *value = static_cast<const bNodeSocketValueTexture *>(
          socket.socket_data);
      ID *id = reinterpret_cast<ID *>(value->value);
      return id_name_or_value_prop(identifier, id, ID_TE, use_name_for_ids);
    }
    case SOCK_IMAGE: {
      const bNodeSocketValueImage *value = static_cast<const bNodeSocketValueImage *>(
          socket.socket_data);
      ID *id = reinterpret_cast<ID *>(value->value);
      return id_name_or_value_prop(identifier, id, ID_IM, use_name_for_ids);
    }
    case SOCK_MATERIAL: {
      const bNodeSocketValueMaterial *value = static_cast<const bNodeSocketValueMaterial *>(
          socket.socket_data);
      ID *id = reinterpret_cast<ID *>(value->value);
      return id_name_or_value_prop(identifier, id, ID_MA, use_name_for_ids);
    }
    case SOCK_MATRIX:
    case SOCK_CUSTOM:
    case SOCK_GEOMETRY:
    case SOCK_SHADER:
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
      return nullptr;
  }
  return nullptr;
}

static bool old_id_property_type_matches_socket_convert_to_new_int(const IDProperty &old_property,
                                                                   IDProperty *new_property)
{
  if (old_property.type != IDP_INT) {
    return false;
  }
  if (new_property) {
    BLI_assert(new_property->type == IDP_INT);
    IDP_int_set(new_property, IDP_int_get(&old_property));
  }
  return true;
}

static bool old_id_property_type_matches_socket_convert_to_new_float_vec(
    const IDProperty &old_property, IDProperty *new_property, const int len)
{
  if (!(old_property.type == IDP_ARRAY &&
        ELEM(old_property.subtype, IDP_INT, IDP_FLOAT, IDP_DOUBLE)))
  {
    return false;
  }

  if (new_property) {
    BLI_assert(new_property->type == IDP_ARRAY && new_property->subtype == IDP_FLOAT);

    switch (old_property.subtype) {
      case IDP_DOUBLE: {
        double *const old_value = IDP_array_double_get(&old_property);
        float *new_value = static_cast<float *>(new_property->data.pointer);
        for (int i = 0; i < len; i++) {
          if (i < old_property.len) {
            new_value[i] = float(old_value[i]);
          }
          else {
            new_value[i] = 0.0f;
          }
        }
        break;
      }
      case IDP_INT: {
        int *const old_value = IDP_array_int_get(&old_property);
        float *new_value = static_cast<float *>(new_property->data.pointer);
        for (int i = 0; i < len; i++) {
          if (i < old_property.len) {
            new_value[i] = float(old_value[i]);
          }
          else {
            new_value[i] = 0.0f;
          }
        }
        break;
      }
      case IDP_FLOAT: {
        float *const old_value = IDP_array_float_get(&old_property);
        float *new_value = static_cast<float *>(new_property->data.pointer);
        for (int i = 0; i < len; i++) {
          if (i < old_property.len) {
            new_value[i] = old_value[i];
          }
          else {
            new_value[i] = 0.0f;
          }
        }
        break;
      }
    }
  }
  return true;
}

static bool old_id_property_type_matches_socket_convert_to_new_string(
    const IDProperty &old_property, IDProperty *new_property)
{
  if (old_property.type != IDP_STRING || old_property.subtype != IDP_STRING_SUB_UTF8) {
    return false;
  }
  if (new_property) {
    BLI_assert(new_property->type == IDP_STRING && new_property->subtype == IDP_STRING_SUB_UTF8);
    IDP_AssignString(new_property, IDP_string_get(&old_property));
  }
  return true;
}

/**
 * Check if the given `old_property` property type is compatible with the given `socket` type.
 * E.g. a #SOCK_FLOAT socket can use data from #IDP_FLOAT, #IDP_INT and #IDP_DOUBLE ID-properties.
 *
 * If `new_property` is given, it is expected to be of the 'perfect match' type with the given
 * `socket` (see #id_property_create_from_socket), and its value will be set from the value of
 * `old_property`, if possible.
 */
static bool old_id_property_type_matches_socket_convert_to_new(
    const bNodeTreeInterfaceSocket &socket,
    const IDProperty &old_property,
    IDProperty *new_property,
    const bool use_name_for_ids)
{
  const bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
  const eNodeSocketDatatype type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
  switch (type) {
    case SOCK_FLOAT:
      if (!ELEM(old_property.type, IDP_FLOAT, IDP_INT, IDP_DOUBLE)) {
        return false;
      }
      if (new_property) {
        BLI_assert(new_property->type == IDP_FLOAT);
        switch (old_property.type) {
          case IDP_DOUBLE:
            IDP_float_set(new_property, float(IDP_double_get(&old_property)));
            break;
          case IDP_INT:
            IDP_float_set(new_property, float(IDP_int_get(&old_property)));
            break;
          case IDP_FLOAT:
            IDP_float_set(new_property, IDP_float_get(&old_property));
            break;
        }
      }
      return true;
    case SOCK_INT:
      return old_id_property_type_matches_socket_convert_to_new_int(old_property, new_property);
    case SOCK_VECTOR: {
      const bNodeSocketValueVector *value = static_cast<const bNodeSocketValueVector *>(
          socket.socket_data);
      return old_id_property_type_matches_socket_convert_to_new_float_vec(
          old_property, new_property, value->dimensions);
    }
    case SOCK_ROTATION:
      return old_id_property_type_matches_socket_convert_to_new_float_vec(
          old_property, new_property, 3);
    case SOCK_RGBA:
      return old_id_property_type_matches_socket_convert_to_new_float_vec(
          old_property, new_property, 4);
    case SOCK_BOOLEAN:
      if (is_layer_selection_field(socket)) {
        return old_id_property_type_matches_socket_convert_to_new_string(old_property,
                                                                         new_property);
      }
      if (!ELEM(old_property.type, IDP_BOOLEAN, IDP_INT)) {
        return false;
      }
      /* Exception: Do conversion from old Integer property (for versioning from older data model),
       * but do not consider int idprop as a valid input for a bool socket. */
      if (new_property) {
        BLI_assert(new_property->type == IDP_BOOLEAN);
        switch (old_property.type) {
          case IDP_INT:
            IDP_bool_set(new_property, bool(IDP_int_get(&old_property)));
            break;
          case IDP_BOOLEAN:
            IDP_bool_set(new_property, IDP_bool_get(&old_property));
            break;
        }
      }
      return old_property.type == IDP_BOOLEAN;
    case SOCK_STRING:
      return old_id_property_type_matches_socket_convert_to_new_string(old_property, new_property);
    case SOCK_MENU:
      return old_id_property_type_matches_socket_convert_to_new_int(old_property, new_property);
    case SOCK_OBJECT:
    case SOCK_COLLECTION:
    case SOCK_TEXTURE:
    case SOCK_IMAGE:
    case SOCK_MATERIAL:
      if (use_name_for_ids) {
        return old_id_property_type_matches_socket_convert_to_new_string(old_property,
                                                                         new_property);
      }
      if (old_property.type != IDP_ID) {
        return false;
      }
      if (new_property) {
        BLI_assert(new_property->type == IDP_ID);
        ID *id = IDP_ID_get(&old_property);
        new_property->data.pointer = id;
        id_us_plus(id);
      }
      return true;
    case SOCK_CUSTOM:
    case SOCK_MATRIX:
    case SOCK_GEOMETRY:
    case SOCK_SHADER:
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
      return false;
  }
  BLI_assert_unreachable();
  return false;
}

bool id_property_type_matches_socket(const bNodeTreeInterfaceSocket &socket,
                                     const IDProperty &property,
                                     const bool use_name_for_ids)
{
  return old_id_property_type_matches_socket_convert_to_new(
      socket, property, nullptr, use_name_for_ids);
}

static bke::SocketValueVariant init_socket_cpp_value_from_property(
    const IDProperty &property, const eNodeSocketDatatype socket_value_type)
{
  switch (socket_value_type) {
    case SOCK_FLOAT: {
      float value = 0.0f;
      if (property.type == IDP_FLOAT) {
        value = IDP_float_get(&property);
      }
      else if (property.type == IDP_DOUBLE) {
        value = float(IDP_double_get(&property));
      }
      return bke::SocketValueVariant(value);
    }
    case SOCK_INT: {
      int value = IDP_int_get(&property);
      return bke::SocketValueVariant(value);
    }
    case SOCK_VECTOR: {
      const void *property_array = IDP_array_voidp_get(&property);
      BLI_assert(property.len >= 2 && property.len <= 4);

      float4 values = float4(0.0f);
      if (property.subtype == IDP_FLOAT) {
        for (int i = 0; i < property.len; i++) {
          values[i] = static_cast<const float *>(property_array)[i];
        }
      }
      else if (property.subtype == IDP_INT) {
        for (int i = 0; i < property.len; i++) {
          values[i] = float(static_cast<const int *>(property_array)[i]);
        }
      }
      else if (property.subtype == IDP_DOUBLE) {
        for (int i = 0; i < property.len; i++) {
          values[i] = float(static_cast<const double *>(property_array)[i]);
        }
      }
      else {
        BLI_assert_unreachable();
      }

      /* Only float3 vectors are supported for now. */
      return bke::SocketValueVariant(float3(values));
    }
    case SOCK_RGBA: {
      const void *property_array = IDP_array_voidp_get(&property);
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
      return bke::SocketValueVariant(value);
    }
    case SOCK_BOOLEAN: {
      const bool value = IDP_bool_get(&property);
      return bke::SocketValueVariant(value);
    }
    case SOCK_ROTATION: {
      const void *property_array = IDP_array_voidp_get(&property);
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
      return bke::SocketValueVariant(math::to_quaternion(euler_value));
    }
    case SOCK_STRING: {
      std::string value = IDP_string_get(&property);
      return bke::SocketValueVariant::From(std::move(value));
    }
    case SOCK_MENU: {
      int value = IDP_int_get(&property);
      return bke::SocketValueVariant::From(MenuValue(value));
    }
    case SOCK_OBJECT: {
      ID *id = IDP_ID_get(&property);
      Object *object = (id && GS(id->name) == ID_OB) ? (Object *)id : nullptr;
      return bke::SocketValueVariant::From(object);
    }
    case SOCK_COLLECTION: {
      ID *id = IDP_ID_get(&property);
      Collection *collection = (id && GS(id->name) == ID_GR) ? (Collection *)id : nullptr;
      return bke::SocketValueVariant::From(collection);
    }
    case SOCK_TEXTURE: {
      ID *id = IDP_ID_get(&property);
      Tex *texture = (id && GS(id->name) == ID_TE) ? (Tex *)id : nullptr;
      return bke::SocketValueVariant::From(texture);
    }
    case SOCK_IMAGE: {
      ID *id = IDP_ID_get(&property);
      Image *image = (id && GS(id->name) == ID_IM) ? (Image *)id : nullptr;
      return bke::SocketValueVariant::From(image);
    }
    case SOCK_MATERIAL: {
      ID *id = IDP_ID_get(&property);
      Material *material = (id && GS(id->name) == ID_MA) ? (Material *)id : nullptr;
      return bke::SocketValueVariant::From(material);
    }
    default: {
      BLI_assert_unreachable();
      return {};
    }
  }
}

std::optional<StringRef> input_attribute_name_get(const IDProperty *properties,
                                                  const bNodeTreeInterfaceSocket &io_input)
{
  IDProperty *use_attribute = IDP_GetPropertyFromGroup_null(
      properties, io_input.identifier + input_use_attribute_suffix);
  if (!use_attribute) {
    return std::nullopt;
  }
  if (use_attribute->type == IDP_INT) {
    if (IDP_int_get(use_attribute) == 0) {
      return std::nullopt;
    }
  }
  if (use_attribute->type == IDP_BOOLEAN) {
    if (!IDP_bool_get(use_attribute)) {
      return std::nullopt;
    }
  }

  const IDProperty *property_attribute_name = IDP_GetPropertyFromGroup_null(
      properties, io_input.identifier + input_attribute_name_suffix);

  return IDP_string_get(property_attribute_name);
}

static bke::SocketValueVariant initialize_group_input(const bNodeTree &tree,
                                                      const IDProperty *properties,
                                                      const int input_index)
{
  const bNodeTreeInterfaceSocket &io_input = *tree.interface_inputs()[input_index];
  const bke::bNodeSocketType *typeinfo = io_input.socket_typeinfo();
  const eNodeSocketDatatype socket_data_type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
  const IDProperty *property = IDP_GetPropertyFromGroup_null(properties, io_input.identifier);
  if (property == nullptr) {
    return typeinfo->get_geometry_nodes_cpp_value(io_input.socket_data);
  }
  if (!id_property_type_matches_socket(io_input, *property)) {
    return typeinfo->get_geometry_nodes_cpp_value(io_input.socket_data);
  }

  if (!input_has_attribute_toggle(tree, input_index)) {
    return init_socket_cpp_value_from_property(*property, socket_data_type);
  }

  const std::optional<StringRef> attribute_name = input_attribute_name_get(properties, io_input);
  if (attribute_name && bke::allow_procedural_attribute_access(*attribute_name)) {
    fn::GField attribute_field = bke::AttributeFieldInput::from(*attribute_name,
                                                                *typeinfo->base_cpp_type);
    return bke::SocketValueVariant::From(std::move(attribute_field));
  }
  if (is_layer_selection_field(io_input)) {
    const IDProperty *property_layer_name = IDP_GetPropertyFromGroup_null(properties,
                                                                          io_input.identifier);
    StringRef layer_name = IDP_string_get(property_layer_name);
    fn::GField selection_field(std::make_shared<bke::NamedLayerSelectionFieldInput>(layer_name),
                               0);
    return bke::SocketValueVariant::From(std::move(selection_field));
  }
  return init_socket_cpp_value_from_property(*property, socket_data_type);
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

    const std::string prop_name = socket->identifier + input_attribute_name_suffix;
    const IDProperty *prop = IDP_GetPropertyFromGroup_null(properties, prop_name);
    if (prop == nullptr) {
      continue;
    }
    const StringRefNull attribute_name = IDP_string_get(prop);
    if (attribute_name.is_empty()) {
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
            GMutableSpan{type,
                         MEM_mallocN_aligned(type.size * domain_size, type.alignment, __func__),
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
                                                    const IDProperty *properties,
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

  GeoNodesUserData user_data;
  user_data.call_data = &call_data;
  call_data.root_ntree = &btree;

  user_data.compute_context = &base_compute_context;

  ResourceScope scope;
  LinearAllocator<> &allocator = scope.allocator();

  btree.ensure_interface_cache();

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

    bke::SocketValueVariant value = initialize_group_input(btree, properties, i);
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
                                            IDProperty &properties,
                                            const bool use_name_for_ids)
{
  tree.ensure_interface_cache();
  const Span<const bNodeTreeInterfaceSocket *> tree_inputs = tree.interface_inputs();
  const Span<nodes::StructureType> input_structure_types =
      tree.runtime->structure_type_interface->inputs;
  for (const int i : tree_inputs.index_range()) {
    const bNodeTreeInterfaceSocket &socket = *tree_inputs[i];
    const StringRefNull socket_identifier = socket.identifier;
    const bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
    const eNodeSocketDatatype socket_type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    IDProperty *new_prop = id_property_create_from_socket(
                               socket, input_structure_types[i], use_name_for_ids)
                               .release();
    if (new_prop == nullptr) {
      continue;
    }

    new_prop->flag |= IDP_FLAG_OVERRIDABLE_LIBRARY | IDP_FLAG_STATIC_TYPE;
    if (socket.description && socket.description[0] != '\0') {
      IDPropertyUIData *ui_data = IDP_ui_data_ensure(new_prop);
      ui_data->description = BLI_strdup(socket.description);
    }
    IDP_AddToGroup(&properties, new_prop);

    if (old_properties != nullptr) {
      const IDProperty *old_prop = IDP_GetPropertyFromGroup(old_properties, socket_identifier);
      if (old_prop != nullptr) {
        /* Re-use the value (and only the value!) from the old property if possible, handling
         * conversion to new property's type as needed. */
        old_id_property_type_matches_socket_convert_to_new(
            socket, *old_prop, new_prop, use_name_for_ids);
      }
    }

    if (socket_type_has_attribute_toggle(eNodeSocketDatatype(socket_type))) {
      const std::string use_attribute_id = socket_identifier + input_use_attribute_suffix;
      const std::string attribute_name_id = socket_identifier + input_attribute_name_suffix;

      IDProperty *use_attribute_prop = bke::idprop::create_bool(use_attribute_id, false).release();
      use_attribute_prop->flag |= IDP_FLAG_OVERRIDABLE_LIBRARY | IDP_FLAG_STATIC_TYPE;
      IDP_AddToGroup(&properties, use_attribute_prop);

      IDProperty *attribute_prop = bke::idprop::create(attribute_name_id, "").release();
      attribute_prop->flag |= IDP_FLAG_OVERRIDABLE_LIBRARY | IDP_FLAG_STATIC_TYPE;
      IDP_AddToGroup(&properties, attribute_prop);

      if (old_properties == nullptr) {
        if (socket.default_attribute_name && socket.default_attribute_name[0] != '\0') {
          IDP_AssignStringMaxSize(attribute_prop, socket.default_attribute_name, MAX_NAME);
          IDP_bool_set(use_attribute_prop, true);
        }
      }
      else {
        IDProperty *old_prop_use_attribute = IDP_GetPropertyFromGroup(old_properties,
                                                                      use_attribute_id);
        if (old_prop_use_attribute != nullptr) {
          IDP_CopyPropertyContent(use_attribute_prop, old_prop_use_attribute);
        }

        IDProperty *old_attribute_name_prop = IDP_GetPropertyFromGroup(old_properties,
                                                                       attribute_name_id);
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
    const bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
    const eNodeSocketDatatype socket_type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    if (!socket_type_has_attribute_toggle(socket_type)) {
      continue;
    }

    const std::string idprop_name = socket_identifier + input_attribute_name_suffix;
    IDProperty *new_prop = IDP_NewStringMaxSize("", MAX_NAME, idprop_name);
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
      IDProperty *old_prop = IDP_GetPropertyFromGroup(old_properties, idprop_name);
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

Vector<InferenceValue> get_geometry_nodes_input_inference_values(const bNodeTree &btree,
                                                                 const IDProperty *properties,
                                                                 ResourceScope &scope)
{
  /* Assume that all inputs have unknown values by default. */
  Vector<InferenceValue> inference_values(btree.interface_inputs().size(),
                                          InferenceValue::Unknown());

  btree.ensure_interface_cache();
  for (const int input_i : btree.interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket &io_input = *btree.interface_inputs()[input_i];
    const bke::bNodeSocketType *stype = io_input.socket_typeinfo();
    if (!stype) {
      continue;
    }
    const eNodeSocketDatatype socket_type = stype->type;
    if (!stype->base_cpp_type || !stype->geometry_nodes_default_value) {
      continue;
    }
    const IDProperty *property = IDP_GetPropertyFromGroup_null(properties, io_input.identifier);
    if (!property) {
      continue;
    }
    if (!id_property_type_matches_socket(io_input, *property)) {
      continue;
    }
    if (input_attribute_name_get(properties, io_input).has_value()) {
      /* Attributes don't have a single base value, so ignore them here. */
      continue;
    }
    if (is_layer_selection_field(io_input)) {
      /* Can't get a single value for layer selections. */
      continue;
    }

    bke::SocketValueVariant &value = scope.construct<bke::SocketValueVariant>(
        init_socket_cpp_value_from_property(*property, socket_type));
    if (!value.is_single()) {
      continue;
    }
    const GPointer single_value = value.get_single_ptr();
    BLI_assert(single_value.type() == stype->base_cpp_type);
    inference_values[input_i] = InferenceValue::from_primitive(single_value.get());
  }
  return inference_values;
}

}  // namespace blender::nodes
