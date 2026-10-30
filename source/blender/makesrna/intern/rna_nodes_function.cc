/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "BLT_translation.hh"

#include "BKE_node.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_c.hh"

#include "rna_internal.hh"
#include "rna_nodetree_common.hh"

namespace blender {

const EnumPropertyItem rna_enum_node_float_to_int_items[] = {
    {FN_NODE_FLOAT_TO_INT_ROUND,
     "ROUND",
     0,
     "Round",
     "Round the float up or down to the nearest integer"},
    {FN_NODE_FLOAT_TO_INT_FLOOR,
     "FLOOR",
     0,
     "Floor",
     "Round the float down to the next smallest integer"},
    {FN_NODE_FLOAT_TO_INT_CEIL,
     "CEILING",
     0,
     "Ceiling",
     "Round the float up to the next largest integer"},
    {FN_NODE_FLOAT_TO_INT_TRUNCATE,
     "TRUNCATE",
     0,
     "Truncate",
     "Round the float to the closest integer in the direction of zero (floor if positive; ceiling "
     "if negative)"},
    {0, nullptr, 0, nullptr, nullptr},
};

}  // namespace blender

#ifdef RNA_RUNTIME

#  include "NOD_fn_format_string.hh"

namespace blender {

using nodes::FormatStringItemsAccessor;

static bool random_value_type_supported(const EnumPropertyItem *item)
{
  return ELEM(item->value, CD_PROP_FLOAT, CD_PROP_FLOAT3, CD_PROP_BOOL, CD_PROP_INT32);
}

static const EnumPropertyItem *rna_FunctionNodeRandomValue_type_itemf(bContext * /*C*/,
                                                                      PointerRNA * /*ptr*/,
                                                                      PropertyRNA * /*prop*/,
                                                                      bool *r_free)
{
  *r_free = true;
  return itemf_function_check(rna_enum_attribute_type_items, random_value_type_supported);
}

static void rna_NodeInputVector_vector_get(PointerRNA *ptr, float *values)
{
  const bNode &node = *ptr->data_as<bNode>();
  const auto &storage = *static_cast<NodeInputVector *>(node.storage);
  memcpy(values, storage.vector, sizeof(float) * storage.dimensions);
}

static void rna_NodeInputVector_vector_set(PointerRNA *ptr, const float *values)
{
  bNode &node = *ptr->data_as<bNode>();
  auto &storage = *static_cast<NodeInputVector *>(node.storage);
  memcpy(storage.vector, values, sizeof(float) * storage.dimensions);
}

static int rna_NodeInputVector_vector_get_array_length(const PointerRNA *ptr,
                                                       int length[RNA_MAX_ARRAY_DIMENSION])
{
  const bNode &node = *ptr->data_as<bNode>();
  const auto &storage = *static_cast<NodeInputVector *>(node.storage);
  length[0] = storage.dimensions;
  return storage.dimensions;
}

static void rna_NodeInputIntVector_vector_get(PointerRNA *ptr, int *values)
{
  const bNode &node = *ptr->data_as<bNode>();
  const auto &storage = *static_cast<NodeInputIntVector *>(node.storage);
  memcpy(values, storage.vector, sizeof(int) * storage.dimensions);
}

static void rna_NodeInputIntVector_vector_set(PointerRNA *ptr, const int *values)
{
  bNode &node = *ptr->data_as<bNode>();
  auto &storage = *static_cast<NodeInputIntVector *>(node.storage);
  memcpy(storage.vector, values, sizeof(int) * storage.dimensions);
}

static int rna_NodeInputIntVector_vector_get_array_length(const PointerRNA *ptr,
                                                          int length[RNA_MAX_ARRAY_DIMENSION])
{
  const bNode &node = *ptr->data_as<bNode>();
  const auto &storage = *static_cast<NodeInputIntVector *>(node.storage);
  length[0] = storage.dimensions;
  return storage.dimensions;
}

const EnumPropertyItem *rna_NodeInputMenu_menu_itemf(bContext * /*C*/,
                                                     PointerRNA *ptr,
                                                     PropertyRNA *prop,
                                                     bool *r_free)
{
  const bNode *node = static_cast<bNode *>(ptr->data);
  const bNodeSocket *socket = node->outputs.first();
  if (!socket) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }
  const bNodeSocketValueMenu *data = static_cast<bNodeSocketValueMenu *>(socket->default_value);
  if (!data->enum_items) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }
  const char *socket_translation_context = bke::node_socket_translation_context(*socket);
  RNA_def_property_translation_context(prop, socket_translation_context);
  return RNA_node_enum_definition_itemf(*data->enum_items, r_free);
}

}  // namespace blender

#else

namespace blender {

static void def_float_to_int(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "rounding_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_float_to_int_items);
  RNA_def_property_ui_text(
      prop, "Rounding Mode", "Method used to convert the float to an integer");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_NODETREE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_color(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputColor", "storage");

  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_sdna(prop, nullptr, "color");
  RNA_def_property_ui_text(prop, "Color", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_bool(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputBool", "storage");

  prop = RNA_def_property(srna, "boolean", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "boolean", 1);
  RNA_def_property_ui_text(prop, "Boolean", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_int(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputInt", "storage");

  prop = RNA_def_property(srna, "integer", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "integer");
  RNA_def_property_ui_text(prop, "Integer", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_menu(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputMenu", "storage");

  prop = RNA_def_property(srna, "value", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_NULL_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_NodeInputMenu_menu_itemf");
  RNA_def_property_ui_text(prop, "Menu", "Value of the output socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
}

static void def_fn_input_rotation(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputRotation", "storage");

  prop = RNA_def_property(srna, "rotation_euler", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, nullptr, "rotation_euler");
  RNA_def_property_ui_text(prop, "Rotation", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_vector(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputVector", "storage");

  prop = RNA_def_property(srna, "vector", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  /* Note: Must be called before function definitions below, so that arraydimension is set. */
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_funcs(
      prop, "rna_NodeInputVector_vector_get", "rna_NodeInputVector_vector_set", nullptr);
  RNA_def_property_dynamic_array_funcs(prop, "rna_NodeInputVector_vector_get_array_length");
  RNA_def_property_ui_text(prop, "Vector", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "vector_dimensions", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "dimensions");
  RNA_def_property_range(prop, 2, 4);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Dimensions", "Dimensions of the vector socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_int_vector(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputIntVector", "storage");

  prop = RNA_def_property(srna, "vector", PROP_INT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  /* Note: Must be called before function definitions below, so that arraydimension is set. */
  RNA_def_property_array(prop, 3);
  RNA_def_property_int_funcs(
      prop, "rna_NodeInputIntVector_vector_get", "rna_NodeInputIntVector_vector_set", nullptr);
  RNA_def_property_dynamic_array_funcs(prop, "rna_NodeInputIntVector_vector_get_array_length");
  RNA_def_property_ui_text(prop, "Vector", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "vector_dimensions", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "dimensions");
  RNA_def_property_range(prop, 2, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Dimensions", "Dimensions of the vector socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_string(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputString", "storage");

  prop = RNA_def_property(srna, "string", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "String", "");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "textbox_state", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "TextboxState");
  RNA_def_property_pointer_sdna(prop, nullptr, "textbox_state");
}

static void def_fn_random_value(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeRandomValue", "storage");

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "data_type");
  RNA_def_property_enum_items(prop, rna_enum_attribute_type_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_FunctionNodeRandomValue_type_itemf");
  RNA_def_property_enum_default(prop, CD_PROP_FLOAT);
  RNA_def_property_ui_text(prop, "Data Type", "Type of data stored in attribute");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void rna_def_fn_format_string_item(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeFunctionFormatStringItem", nullptr);
  RNA_def_struct_ui_text(srna, "Format String Item", "");

  rna_def_node_item_array_socket_item_common(srna, "FormatStringItemsAccessor", true);
}

static void rna_def_fn_format_string_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeFunctionFormatStringItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of format string items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeFunctionFormatStringItem", "FormatStringItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "NodeFunctionFormatStringItem", "FormatStringItemsAccessor");
}

static void def_fn_format_string(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_fn_format_string_item(brna);
  rna_def_fn_format_string_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeFunctionFormatString", "storage");

  prop = RNA_def_property(srna, "format_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_num");
  RNA_def_property_struct_type(prop, "NodeFunctionFormatStringItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeFunctionFormatStringItems");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);
}

static void def_fn_rotate_euler(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem type_items[] = {
      {FN_NODE_ROTATE_EULER_TYPE_AXIS_ANGLE,
       "AXIS_ANGLE",
       ICON_NONE,
       "Axis Angle",
       "Rotate around an axis by an angle"},
      {FN_NODE_ROTATE_EULER_TYPE_EULER,
       "EULER",
       ICON_NONE,
       "Euler",
       "Rotate around the X, Y, and Z axes"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem space_items[] = {
      {FN_NODE_ROTATE_EULER_SPACE_OBJECT,
       "OBJECT",
       ICON_NONE,
       "Object",
       "Rotate the input rotation in the local space of the object"},
      {FN_NODE_ROTATE_EULER_SPACE_LOCAL,
       "LOCAL",
       ICON_NONE,
       "Local",
       "Rotate the input rotation in its local space"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  prop = RNA_def_property(srna, "rotation_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, type_items);
  RNA_def_property_ui_text(prop, "Type", "Method used to describe the rotation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom2");
  RNA_def_property_enum_items(prop, space_items);
  RNA_def_property_ui_text(prop, "Space", "Base orientation for rotation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

void RNA_def_nodes_function(BlenderRNA *brna)
{
  const auto define = [&](const char *base_name,
                          const char *struct_name,
                          void (*func)(BlenderRNA *, StructRNA *) = nullptr) {
    StructRNA *srna = define_specific_node(brna, struct_name, base_name);
    if (func) {
      func(brna, srna);
    }
  };

  /* clang-format off */

  define("FunctionNode", "FunctionNodeAlignEulerToVector");
  define("FunctionNode", "FunctionNodeAlignRotationToVector");
  define("FunctionNode", "FunctionNodeAxesToRotation");
  define("FunctionNode", "FunctionNodeAxisAngleToRotation");
  define("FunctionNode", "FunctionNodeBitMath");
  define("FunctionNode", "FunctionNodeBooleanMath");
  define("FunctionNode", "FunctionNodeCombineColor");
  define("FunctionNode", "FunctionNodeCombineMatrix");
  define("FunctionNode", "FunctionNodeCombineTransform");
  define("FunctionNode", "FunctionNodeCompare");
  define("FunctionNode", "FunctionNodeEulerToRotation");
  define("FunctionNode", "FunctionNodeFindInString");
  define("FunctionNode", "FunctionNodeFloatToInt", def_float_to_int);
  define("FunctionNode", "FunctionNodeFormatString", def_fn_format_string);
  define("FunctionNode", "FunctionNodeGetVectorComponent");
  define("FunctionNode", "FunctionNodeHashValue");
  define("FunctionNode", "FunctionNodeInputBool", def_fn_input_bool);
  define("FunctionNode", "FunctionNodeInputColor", def_fn_input_color);
  define("FunctionNode", "FunctionNodeInputInt", def_fn_input_int);
  define("FunctionNode", "FunctionNodeInputIntVector", def_fn_input_int_vector);
  define("FunctionNode", "FunctionNodeInputMenu", def_fn_input_menu);
  define("FunctionNode", "FunctionNodeInputRotation", def_fn_input_rotation);
  define("FunctionNode", "FunctionNodeInputSpecialCharacters");
  define("FunctionNode", "FunctionNodeInputString", def_fn_input_string);
  define("FunctionNode", "FunctionNodeInputVector", def_fn_input_vector);
  define("FunctionNode", "FunctionNodeIntegerMath");
  define("FunctionNode", "FunctionNodeInvertMatrix");
  define("FunctionNode", "FunctionNodeInvertRotation");
  define("FunctionNode", "FunctionNodeMatchString");
  define("FunctionNode", "FunctionNodeMatrixDeterminant");
  define("FunctionNode", "FunctionNodeMatrixMultiply");
  define("FunctionNode", "FunctionNodeMatrixSVD");
  define("FunctionNode", "FunctionNodeProjectPoint");
  define("FunctionNode", "FunctionNodeQuaternionToRotation");
  define("FunctionNode", "FunctionNodeRandomValue", def_fn_random_value);
  define("FunctionNode", "FunctionNodeReplaceString");
  define("FunctionNode", "FunctionNodeReverseString");
  define("FunctionNode", "FunctionNodeRotateEuler", def_fn_rotate_euler);
  define("FunctionNode", "FunctionNodeRotateRotation");
  define("FunctionNode", "FunctionNodeRotateVector");
  define("FunctionNode", "FunctionNodeRotationToAxisAngle");
  define("FunctionNode", "FunctionNodeRotationToEuler");
  define("FunctionNode", "FunctionNodeRotationToQuaternion");
  define("FunctionNode", "FunctionNodeSeparateColor");
  define("FunctionNode", "FunctionNodeSeparateMatrix");
  define("FunctionNode", "FunctionNodeSeparateTransform");
  define("FunctionNode", "FunctionNodeSetStringCase");
  define("FunctionNode", "FunctionNodeSliceString");
  define("FunctionNode", "FunctionNodeSplitString");
  define("FunctionNode", "FunctionNodeStringLength");
  define("FunctionNode", "FunctionNodeStringToValue");
  define("FunctionNode", "FunctionNodeTransformDirection");
  define("FunctionNode", "FunctionNodeTransformPoint");
  define("FunctionNode", "FunctionNodeTransposeMatrix");
  define("FunctionNode", "FunctionNodeTrimString");
  define("FunctionNode", "FunctionNodeValueToString");

  /* clang-format on */
}

}  // namespace blender

#endif
