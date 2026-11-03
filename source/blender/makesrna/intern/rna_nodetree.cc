/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "BLI_linear_allocator.hh"

#include "BLT_translation.hh"

#include "BKE_animsys.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"

#include "IMB_colormanagement.hh"

#include "NOD_common.hh"

#include "RNA_enum_types.hh"
#include "RNA_path.hh"

#include "UI_interface_c.hh"

#include "rna_internal.hh"
#include "rna_nodetree_common.hh"

namespace blender {

const EnumPropertyItem rna_enum_node_socket_in_out_items[] = {{SOCK_IN, "IN", 0, "Input", ""},
                                                              {SOCK_OUT, "OUT", 0, "Output", ""},
                                                              {0, nullptr, 0, nullptr, nullptr}};

const EnumPropertyItem rna_enum_node_socket_data_type_items[] = {
    {SOCK_FLOAT, "FLOAT", ICON_NODE_SOCKET_FLOAT, "Float", ""},
    {SOCK_INT, "INT", ICON_NODE_SOCKET_INT, "Integer", ""},
    {SOCK_BOOLEAN, "BOOLEAN", ICON_NODE_SOCKET_BOOLEAN, "Boolean", ""},
    {SOCK_VECTOR, "VECTOR", ICON_NODE_SOCKET_VECTOR, "Vector", ""},
    {SOCK_RGBA, "RGBA", ICON_NODE_SOCKET_RGBA, "Color", ""},
    {SOCK_ROTATION, "ROTATION", ICON_NODE_SOCKET_ROTATION, "Rotation", ""},
    {SOCK_MATRIX, "MATRIX", ICON_NODE_SOCKET_MATRIX, "Matrix", ""},
    {SOCK_STRING, "STRING", ICON_NODE_SOCKET_STRING, "String", ""},
    {SOCK_MENU, "MENU", ICON_NODE_SOCKET_MENU, "Menu", ""},
    {SOCK_SHADER, "SHADER", ICON_NODE_SOCKET_SHADER, "Shader", ""},
    {SOCK_OBJECT, "OBJECT", ICON_NODE_SOCKET_OBJECT, "Object", ""},
    {SOCK_IMAGE, "IMAGE", ICON_NODE_SOCKET_IMAGE, "Image", ""},
    {SOCK_GEOMETRY, "GEOMETRY", ICON_NODE_SOCKET_GEOMETRY, "Geometry", ""},
    {SOCK_COLLECTION, "COLLECTION", ICON_NODE_SOCKET_COLLECTION, "Collection", ""},
    {SOCK_TEXTURE, "TEXTURE", ICON_NODE_SOCKET_TEXTURE, "Texture", ""},
    {SOCK_MATERIAL, "MATERIAL", ICON_NODE_SOCKET_MATERIAL, "Material", ""},
    {SOCK_BUNDLE, "BUNDLE", ICON_NODE_SOCKET_BUNDLE, "Bundle", ""},
    {SOCK_CLOSURE, "CLOSURE", ICON_NODE_SOCKET_CLOSURE, "Closure", ""},
    {SOCK_FONT, "FONT", ICON_NODE_SOCKET_FONT, "Font", ""},
    {SOCK_SCENE, "SCENE", ICON_NODE_SOCKET_SCENE, "Scene", ""},
    {SOCK_TEXT_ID, "TEXT", ICON_NODE_SOCKET_TEXT, "Text", ""},
    {SOCK_MASK, "MASK", ICON_NODE_SOCKET_MASK, "Mask", ""},
    {SOCK_SOUND, "SOUND", ICON_NODE_SOCKET_SOUND, "Sound", ""},
    {SOCK_INT_VECTOR, "INT_VECTOR", ICON_NODE_SOCKET_INT_VECTOR, "Integer Vector", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_color_tag_items[] = {
    {int(bke::NodeColorTag::None),
     "NONE",
     0,
     "None",
     "Default color tag for new nodes and node groups"},
    {int(bke::NodeColorTag::Attribute), "ATTRIBUTE", 0, "Attribute", ""},
    {int(bke::NodeColorTag::Color), "COLOR", 0, "Color", ""},
    {int(bke::NodeColorTag::Converter), "CONVERTER", 0, "Converter", ""},
    {int(bke::NodeColorTag::Distort), "DISTORT", 0, "Distort", ""},
    {int(bke::NodeColorTag::Filter), "FILTER", 0, "Filter", ""},
    {int(bke::NodeColorTag::Geometry), "GEOMETRY", 0, "Geometry", ""},
    {int(bke::NodeColorTag::Input), "INPUT", 0, "Input", ""},
    {int(bke::NodeColorTag::Matte), "MATTE", 0, "Matte", ""},
    {int(bke::NodeColorTag::Output), "OUTPUT", 0, "Output", ""},
    {int(bke::NodeColorTag::Script), "SCRIPT", 0, "Script", ""},
    {int(bke::NodeColorTag::Shader), "SHADER", 0, "Shader", ""},
    {int(bke::NodeColorTag::Texture), "TEXTURE", 0, "Texture", ""},
    {int(bke::NodeColorTag::Vector), "VECTOR", 0, "Vector", ""},
    {int(bke::NodeColorTag::Pattern), "PATTERN", 0, "Pattern", ""},
    {int(bke::NodeColorTag::Interface), "INTERFACE", 0, "Interface", ""},
    {int(bke::NodeColorTag::Group), "GROUP", 0, "Group", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_math_items[] = {
    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_ID_NODETREE, "Functions"), nullptr),
    {NODE_MATH_ADD, "ADD", 0, "Add", "A + B"},
    {NODE_MATH_SUBTRACT, "SUBTRACT", 0, "Subtract", "A - B"},
    {NODE_MATH_MULTIPLY, "MULTIPLY", 0, "Multiply", "A * B"},
    {NODE_MATH_DIVIDE, "DIVIDE", 0, "Divide", "A / B"},
    {NODE_MATH_MULTIPLY_ADD, "MULTIPLY_ADD", 0, "Multiply Add", "A * B + C"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_MATH_POWER, "POWER", 0, "Power", "A power B"},
    {NODE_MATH_LOGARITHM, "LOGARITHM", 0, "Logarithm", "Logarithm A base B"},
    {NODE_MATH_SQRT, "SQRT", 0, "Square Root", "Square root of A"},
    {NODE_MATH_INV_SQRT, "INVERSE_SQRT", 0, "Inverse Square Root", "1 / Square root of A"},
    {NODE_MATH_ABSOLUTE, "ABSOLUTE", 0, "Absolute", "Magnitude of A"},
    {NODE_MATH_EXPONENT, "EXPONENT", 0, "Exponent", "exp(A)"},
    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_ID_NODETREE, "Comparison"), nullptr),
    {NODE_MATH_MINIMUM, "MINIMUM", 0, "Minimum", "The minimum from A and B"},
    {NODE_MATH_MAXIMUM, "MAXIMUM", 0, "Maximum", "The maximum from A and B"},
    {NODE_MATH_LESS_THAN, "LESS_THAN", 0, "Less Than", "1 if A < B else 0"},
    {NODE_MATH_GREATER_THAN, "GREATER_THAN", 0, "Greater Than", "1 if A > B else 0"},
    {NODE_MATH_SIGN, "SIGN", 0, "Sign", "Returns the sign of A"},
    {NODE_MATH_COMPARE, "COMPARE", 0, "Compare", "1 if (A == B) within tolerance C else 0"},
    {NODE_MATH_SMOOTH_MIN,
     "SMOOTH_MIN",
     0,
     "Smooth Minimum",
     "The minimum from A and B with smoothing C"},
    {NODE_MATH_SMOOTH_MAX,
     "SMOOTH_MAX",
     0,
     "Smooth Maximum",
     "The maximum from A and B with smoothing C"},
    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_ID_NODETREE, "Rounding"), nullptr),
    {NODE_MATH_ROUND,
     "ROUND",
     0,
     "Round",
     "Round A to the nearest integer. Round upward if the fraction part is 0.5"},
    {NODE_MATH_FLOOR, "FLOOR", 0, "Floor", "The largest integer smaller than or equal A"},
    {NODE_MATH_CEIL, "CEIL", 0, "Ceil", "The smallest integer greater than or equal A"},
    {NODE_MATH_TRUNC, "TRUNC", 0, "Truncate", "The integer part of A, removing fractional digits"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_MATH_FRACTION, "FRACT", 0, "Fraction", "The fraction part of A"},
    {NODE_MATH_MODULO,
     "MODULO",
     0,
     "Truncated Modulo",
     "The remainder of truncated division using fmod(A,B)"},
    {NODE_MATH_FLOORED_MODULO,
     "FLOORED_MODULO",
     0,
     "Floored Modulo",
     "The remainder of floored division"},
    {NODE_MATH_WRAP, "WRAP", 0, "Wrap", "Wrap value to range, wrap(A,B)"},
    {NODE_MATH_SNAP, "SNAP", 0, "Snap", "Snap to increment, snap(A,B)"},
    {NODE_MATH_PINGPONG,
     "PINGPONG",
     0,
     "Ping-Pong",
     "Wraps a value and reverses every other cycle (A,B)"},
    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_ID_NODETREE, "Trigonometric"), nullptr),
    {NODE_MATH_SINE, "SINE", 0, "Sine", "sin(A)"},
    {NODE_MATH_COSINE, "COSINE", 0, "Cosine", "cos(A)"},
    {NODE_MATH_TANGENT, "TANGENT", 0, "Tangent", "tan(A)"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_MATH_ARCSINE, "ARCSINE", 0, "Arcsine", "arcsin(A)"},
    {NODE_MATH_ARCCOSINE, "ARCCOSINE", 0, "Arccosine", "arccos(A)"},
    {NODE_MATH_ARCTANGENT, "ARCTANGENT", 0, "Arctangent", "arctan(A)"},
    {NODE_MATH_ARCTAN2, "ARCTAN2", 0, "Arctan2", "The signed angle arctan(A / B)"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_MATH_SINH, "SINH", 0, "Hyperbolic Sine", "sinh(A)"},
    {NODE_MATH_COSH, "COSH", 0, "Hyperbolic Cosine", "cosh(A)"},
    {NODE_MATH_TANH, "TANH", 0, "Hyperbolic Tangent", "tanh(A)"},
    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_ID_NODETREE, "Conversion"), nullptr),
    {NODE_MATH_RADIANS, "RADIANS", 0, "To Radians", "Convert from degrees to radians"},
    {NODE_MATH_DEGREES, "DEGREES", 0, "To Degrees", "Convert from radians to degrees"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_boolean_math_items[] = {
    {NODE_BOOLEAN_MATH_AND, "AND", 0, "And", "True when both inputs are true"},
    {NODE_BOOLEAN_MATH_OR, "OR", 0, "Or", "True when at least one input is true"},
    {NODE_BOOLEAN_MATH_NOT, "NOT", 0, "Not", "Opposite of the input"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_BOOLEAN_MATH_NAND, "NAND", 0, "Not And", "True when at least one input is false"},
    {NODE_BOOLEAN_MATH_NOR, "NOR", 0, "Nor", "True when both inputs are false"},
    {NODE_BOOLEAN_MATH_XNOR,
     "XNOR",
     0,
     "Equal",
     "True when both inputs are equal (exclusive nor)"},
    {NODE_BOOLEAN_MATH_XOR,
     "XOR",
     0,
     "Not Equal",
     "True when both inputs are different (exclusive or)"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_BOOLEAN_MATH_IMPLY,
     "IMPLY",
     0,
     "Imply",
     "True unless the first input is true and the second is false"},
    {NODE_BOOLEAN_MATH_NIMPLY,
     "NIMPLY",
     0,
     "Subtract",
     "True when the first input is true and the second is false (not imply)"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_integer_math_items[] = {
    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_ID_NODETREE, "Functions"), nullptr),
    {NODE_INTEGER_MATH_ADD, "ADD", 0, "Add", "A + B"},
    {NODE_INTEGER_MATH_SUBTRACT, "SUBTRACT", 0, "Subtract", "A - B"},
    {NODE_INTEGER_MATH_MULTIPLY, "MULTIPLY", 0, "Multiply", "A * B"},
    {NODE_INTEGER_MATH_DIVIDE, "DIVIDE", 0, "Divide", "A / B"},
    {NODE_INTEGER_MATH_MULTIPLY_ADD, "MULTIPLY_ADD", 0, "Multiply Add", "A * B + C"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_INTEGER_MATH_ABSOLUTE, "ABSOLUTE", 0, "Absolute", "Non-negative value of A, abs(A)"},
    {NODE_INTEGER_MATH_NEGATE, "NEGATE", 0, "Negate", "-A"},
    {NODE_INTEGER_MATH_POWER, "POWER", 0, "Power", "A power B, pow(A,B)"},
    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_ID_NODETREE, "Comparison"), nullptr),
    {NODE_INTEGER_MATH_MINIMUM,
     "MINIMUM",
     0,
     "Minimum",
     "The minimum value from A and B, min(A,B)"},
    {NODE_INTEGER_MATH_MAXIMUM,
     "MAXIMUM",
     0,
     "Maximum",
     "The maximum value from A and B, max(A,B)"},
    {NODE_INTEGER_MATH_SIGN, "SIGN", 0, "Sign", "Return the sign of A, sign(A)"},
    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_ID_NODETREE, "Rounding"), nullptr),
    {NODE_INTEGER_MATH_DIVIDE_ROUND,
     "DIVIDE_ROUND",
     0,
     "Divide Round",
     "Divide and round result toward zero"},
    {NODE_INTEGER_MATH_DIVIDE_FLOOR,
     "DIVIDE_FLOOR",
     0,
     "Divide Floor",
     "Divide and floor to the largest integer smaller than or equal to the result"},
    {NODE_INTEGER_MATH_DIVIDE_CEIL,
     "DIVIDE_CEIL",
     0,
     "Divide Ceiling",
     "Divide and ceil to the smallest integer greater than or equal to the result"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_INTEGER_MATH_FLOORED_MODULO,
     "FLOORED_MODULO",
     0,
     "Floored Modulo",
     "Modulo that is periodic for both negative and positive operands"},
    {NODE_INTEGER_MATH_MODULO, "MODULO", 0, "Modulo", "Modulo which is the remainder of A / B"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_INTEGER_MATH_GCD,
     "GCD",
     0,
     "Greatest Common Divisor",
     "The largest positive integer that divides into each of the values A and B, "
     "e.g. GCD(8,12) = 4"},
    {NODE_INTEGER_MATH_LCM,
     "LCM",
     0,
     "Least Common Multiple",
     "The smallest positive integer that is divisible by both A and B, e.g. LCM(6,10) = 30"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_compare_operation_items[] = {
    {NODE_COMPARE_LESS_THAN,
     "LESS_THAN",
     0,
     "Less Than",
     "True when the first input is smaller than second input"},
    {NODE_COMPARE_LESS_EQUAL,
     "LESS_EQUAL",
     0,
     "Less Than or Equal",
     "True when the first input is smaller than the second input or equal"},
    {NODE_COMPARE_GREATER_THAN,
     "GREATER_THAN",
     0,
     "Greater Than",
     "True when the first input is greater than the second input"},
    {NODE_COMPARE_GREATER_EQUAL,
     "GREATER_EQUAL",
     0,
     "Greater Than or Equal",
     "True when the first input is greater than the second input or equal"},
    {NODE_COMPARE_EQUAL, "EQUAL", 0, "Equal", "True when both inputs are approximately equal"},
    {NODE_COMPARE_NOT_EQUAL,
     "NOT_EQUAL",
     0,
     "Not Equal",
     "True when both inputs are not approximately equal"},
    {NODE_COMPARE_COLOR_BRIGHTER,
     "BRIGHTER",
     0,
     "Brighter",
     "True when the first input is brighter"},
    {NODE_COMPARE_COLOR_DARKER, "DARKER", 0, "Darker", "True when the first input is darker"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_combsep_color_items[] = {
    {NODE_COMBSEP_COLOR_RGB,
     "RGB",
     ICON_NONE,
     "RGB",
     "Use RGB (Red, Green, Blue) color processing"},
    {NODE_COMBSEP_COLOR_HSV,
     "HSV",
     ICON_NONE,
     "HSV",
     "Use HSV (Hue, Saturation, Value) color processing"},
    {NODE_COMBSEP_COLOR_HSL,
     "HSL",
     ICON_NONE,
     "HSL",
     "Use HSL (Hue, Saturation, Lightness) color processing"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_geometry_mesh_circle_fill_type_items[] = {
    {GEO_NODE_MESH_CIRCLE_FILL_NONE, "NONE", 0, "None", ""},
    {GEO_NODE_MESH_CIRCLE_FILL_NGON, "NGON", 0, "N-Gon", ""},
    {GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN, "TRIANGLE_FAN", 0, "Triangles", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_geometry_nodes_gizmo_color_items[] = {
    {GEO_NODE_GIZMO_COLOR_PRIMARY, "PRIMARY", 0, "Primary", ""},
    {GEO_NODE_GIZMO_COLOR_SECONDARY, "SECONDARY", 0, "Secondary", ""},
    {GEO_NODE_GIZMO_COLOR_X, "X", 0, "X", ""},
    {GEO_NODE_GIZMO_COLOR_Y, "Y", 0, "Y", ""},
    {GEO_NODE_GIZMO_COLOR_Z, "Z", 0, "Z", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_geometry_nodes_linear_gizmo_draw_style_items[] = {
    {GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_ARROW, "ARROW", 0, "Arrow", ""},
    {GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_CROSS, "CROSS", 0, "Cross", ""},
    {GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_BOX, "BOX", 0, "Box", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_compositor_extension_items[] = {
    {CMP_NODE_EXTENSION_MODE_CLIP,
     "CLIP",
     0,
     N_("Clip"),
     N_("Areas outside of the image are filled with zero")},
    {CMP_NODE_EXTENSION_MODE_EXTEND,
     "EXTEND",
     0,
     N_("Extend"),
     N_("Areas outside of the image are filled with the closest boundary pixel in the image")},
    {CMP_NODE_EXTENSION_MODE_REPEAT,
     "REPEAT",
     0,
     N_("Repeat"),
     N_("Areas outside of the image are filled with repetitions of the image")},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_compositor_interpolation_items[] = {
    {CMP_NODE_INTERPOLATION_NEAREST, "NEAREST", 0, N_("Nearest"), N_("Use Nearest interpolation")},
    {CMP_NODE_INTERPOLATION_BILINEAR,
     "BILINEAR",
     0,
     N_("Bilinear"),
     N_("Use Bilinear interpolation")},
    {CMP_NODE_INTERPOLATION_BICUBIC,
     "BICUBIC",
     0,
     N_("Bicubic"),
     N_("Use Cubic B-Spline interpolation")},
    {CMP_NODE_INTERPOLATION_ANISOTROPIC,
     "ANISOTROPIC",
     0,
     N_("Anisotropic"),
     N_("Use Anisotropic interpolation")},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifndef RNA_RUNTIME

const EnumPropertyItem prop_shader_output_target_items[] = {
    {SHD_OUTPUT_ALL,
     "ALL",
     0,
     "All",
     "Use shaders for all renderers and viewports, unless there exists a more specific output"},
    {SHD_OUTPUT_EEVEE, "EEVEE", 0, "EEVEE", "Use shaders for EEVEE renderer"},
    {SHD_OUTPUT_CYCLES, "CYCLES", 0, "Cycles", "Use shaders for Cycles renderer"},
    {0, nullptr, 0, nullptr, nullptr},
};

#endif /* !RNA_RUNTIME */

#undef ITEM_ATTRIBUTE
#undef ITEM_FLOAT
#undef ITEM_VECTOR
#undef ITEM_COLOR
#undef ITEM_BOOLEAN

}  // namespace blender

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BLF_api.hh"

#  include "BKE_scene.hh"

#  include "DEG_depsgraph_query.hh"

#  include "NOD_composite.hh"
#  include "NOD_geo_bundle.hh"
#  include "NOD_geo_closure.hh"
#  include "NOD_geometry.hh"
#  include "NOD_geometry_nodes_lazy_function.hh"
#  include "NOD_shader.h"
#  include "NOD_texture.h"

namespace blender {

using nodes::ClosureInputItemsAccessor;
using nodes::ClosureOutputItemsAccessor;
using nodes::CombineBundleItemsAccessor;
using nodes::EvaluateClosureInputItemsAccessor;
using nodes::EvaluateClosureOutputItemsAccessor;
using nodes::SeparateBundleItemsAccessor;

extern FunctionRNA *rna_NodeTree_poll_func;
extern FunctionRNA *rna_NodeTree_update_func;
extern FunctionRNA *rna_NodeTree_get_from_context_func;
extern FunctionRNA *rna_NodeTree_valid_socket_type_func;
extern FunctionRNA *rna_Node_poll_func;
extern FunctionRNA *rna_Node_poll_instance_func;
extern FunctionRNA *rna_Node_update_func;
extern FunctionRNA *rna_Node_insert_link_func;
extern FunctionRNA *rna_Node_init_func;
extern FunctionRNA *rna_Node_copy_func;
extern FunctionRNA *rna_Node_free_func;
extern FunctionRNA *rna_Node_draw_buttons_func;
extern FunctionRNA *rna_Node_draw_buttons_ext_func;
extern FunctionRNA *rna_Node_draw_label_func;

int rna_node_tree_idname_to_enum(const char *idname)
{
  Span<const bke::bNodeTreeType *> types = bke::node_tree_types_get();
  for (const int i : types.index_range()) {
    const bke::bNodeTreeType *nt = types[i];
    if (nt->idname == idname) {
      return i;
    }
  }
  return -1;
}

bke::bNodeTreeType *rna_node_tree_type_from_enum(int value)
{
  Span<bke::bNodeTreeType *> types = bke::node_tree_types_get();
  return types.index_range().contains(value) ? types[value] : nullptr;
}

const EnumPropertyItem *rna_node_tree_type_itemf(void *data,
                                                 bool (*poll)(void *data, bke::bNodeTreeType *),
                                                 bool *r_free)
{
  EnumPropertyItem tmp = {0};
  EnumPropertyItem *item = nullptr;
  int totitem = 0;

  const Span<bke::bNodeTreeType *> types = bke::node_tree_types_get();
  for (const int i : types.index_range()) {
    bke::bNodeTreeType *nt = types[i];
    if (poll && !poll(data, nt)) {
      continue;
    }

    tmp.value = i;
    tmp.identifier = nt->idname.c_str();
    tmp.icon = nt->ui_icon;
    tmp.name = nt->ui_name.c_str();
    tmp.description = nt->ui_description.c_str();

    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  if (totitem == 0) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

int rna_node_socket_idname_to_enum(const char *idname)
{
  bke::bNodeSocketType *socket_type = bke::node_socket_type_find(idname);
  if (socket_type == nullptr) {
    /* May happen when reading newer files with undefined types. In this case the socket is
     * undefined and no enum item will be selected. */
    return -1;
  }

  /* Regular socket types use the base type as their enum value.
   * Custom sockets don't have a base type and are used directly as the enum entry. */
  if (socket_type->type == SOCK_CUSTOM) {
    return bke::node_socket_types_get().first_index(socket_type);
  }

  bke::bNodeSocketType *base_socket_type = bke::node_socket_type_find_static(socket_type->type,
                                                                             PROP_NONE);
  BLI_assert(base_socket_type != nullptr);
  return bke::node_socket_types_get().first_index(base_socket_type);
}

bke::bNodeSocketType *rna_node_socket_type_from_enum(int value)
{
  Span<bke::bNodeSocketType *> types = bke::node_socket_types_get();
  return types.index_range().contains(value) ? types[value] : nullptr;
}

const EnumPropertyItem *rna_node_socket_type_itemf(
    void *data, bool (*poll)(void *data, bke::bNodeSocketType *), bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0};
  int totitem = 0;
  StructRNA *srna;

  const Span<bke::bNodeSocketType *> types = bke::node_socket_types_get();
  for (const int i : types.index_range()) {
    bke::bNodeSocketType *stype = types[i];
    if (poll && !poll(data, stype)) {
      continue;
    }

    srna = stype->ext_socket.srna;
    tmp.value = i;
    tmp.identifier = stype->idname.c_str();
    tmp.icon = RNA_struct_ui_icon(srna);
    tmp.name = bke::node_socket_type_label(*stype).c_str();
    tmp.description = RNA_struct_ui_description(srna);

    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  if (totitem == 0) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static const char *get_legacy_node_type(const PointerRNA *ptr)
{
  const bNode *node = ptr->data_as<bNode>();
  const bke::bNodeType *ntype = node->typeinfo;
  if (ntype->type_legacy == NODE_CUSTOM) {
    return "CUSTOM";
  }
  if (ntype->type_legacy == NODE_CUSTOM_GROUP) {
    return "CUSTOM GROUP";
  }
  if (ntype->type_legacy == NODE_UNDEFINED) {
    return "UNDEFINED";
  }
  if (ntype->enum_name_legacy) {
    return ntype->enum_name_legacy;
  }
  return ntype->idname.c_str();
}

static int rna_node_type_length(PointerRNA *ptr)
{
  const char *legacy_type = get_legacy_node_type(ptr);
  BLI_assert(legacy_type);
  return strlen(legacy_type);
}

static void rna_node_type_get(PointerRNA *ptr, char *value)
{
  const char *legacy_type = get_legacy_node_type(ptr);
  BLI_assert(legacy_type);
  strcpy(value, legacy_type);
}

static void rna_Node_bl_idname_get(PointerRNA *ptr, char *value)
{
  const bNode *node = ptr->data_as<bNode>();
  const bke::bNodeType *ntype = node->typeinfo;
  ntype->idname.ref().copy_unsafe(value);
}

static int rna_Node_bl_idname_length(PointerRNA *ptr)
{
  const bNode *node = ptr->data_as<bNode>();
  const bke::bNodeType *ntype = node->typeinfo;
  return ntype->idname.size();
}

static void rna_Node_bl_idname_set(PointerRNA *ptr, const char *value)
{
  bNode *node = ptr->data_as<bNode>();
  bke::bNodeType *ntype = node->typeinfo;
  ntype->idname = UString(value);
}

static void rna_Node_bl_label_get(PointerRNA *ptr, char *value)
{
  const bNode *node = ptr->data_as<bNode>();
  const bke::bNodeType *ntype = node->typeinfo;
  StringRef(ntype->ui_name).copy_unsafe(value);
}

static int rna_Node_bl_label_length(PointerRNA *ptr)
{
  const bNode *node = ptr->data_as<bNode>();
  const bke::bNodeType *ntype = node->typeinfo;
  return ntype->ui_name.size();
}

static void rna_Node_bl_label_set(PointerRNA *ptr, const char *value)
{
  bNode *node = ptr->data_as<bNode>();
  bke::bNodeType *ntype = node->typeinfo;
  ntype->ui_name = value;
}

static void rna_Node_bl_description_get(PointerRNA *ptr, char *value)
{
  const bNode *node = ptr->data_as<bNode>();
  const bke::bNodeType *ntype = node->typeinfo;
  StringRef(ntype->ui_description).copy_unsafe(value);
}

static int rna_Node_bl_description_length(PointerRNA *ptr)
{
  const bNode *node = ptr->data_as<bNode>();
  const bke::bNodeType *ntype = node->typeinfo;
  return ntype->ui_description.size();
}

static void rna_Node_bl_description_set(PointerRNA *ptr, const char *value)
{
  bNode *node = ptr->data_as<bNode>();
  bke::bNodeType *ntype = node->typeinfo;
  ntype->ui_description = value;
}

static float2 node_parent_offset(const bNode &node)
{
  return node.parent ? float2(node.parent->location[0], node.parent->location[1]) : float2(0);
}

static void rna_Node_location_get(PointerRNA *ptr, float *value)
{
  const bNode *node = ptr->data_as<bNode>();
  copy_v2_v2(value, float2(node->location[0], node->location[1]) - node_parent_offset(*node));
}

static void move_child_nodes(bNode &node, const float2 &delta)
{
  for (bNode *child : node.direct_children_in_frame()) {
    child->location[0] += delta.x;
    child->location[1] += delta.y;
    if (child->is_frame()) {
      move_child_nodes(*child, delta);
    }
  }
}

static void rna_Node_location_set(PointerRNA *ptr, const float *value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();

  ntree->ensure_topology_cache();
  const float2 new_location = float2(value) + node_parent_offset(*node);
  if (node->is_frame()) {
    move_child_nodes(*node, new_location - float2(node->location[0], node->location[1]));
  }
  node->location[0] = new_location.x;
  node->location[1] = new_location.y;
}

/* ******** Node Tree ******** */

static StructRNA *rna_NodeTree_refine(PointerRNA *ptr)
{
  bNodeTree *ntree = ptr->data_as<bNodeTree>();

  if (ntree->typeinfo->rna_ext.srna) {
    return ntree->typeinfo->rna_ext.srna;
  }
  else {
    return RNA_NodeTree;
  }
}

static bool rna_NodeTree_poll(const bContext *C, bke::bNodeTreeType *ntreetype)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool visible;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, ntreetype->rna_ext.srna, nullptr); /* dummy */
  func = rna_NodeTree_poll_func;                  /* RNA_struct_find_function(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  ntreetype->rna_ext.call(const_cast<bContext *>(C), &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "visible", &ret);
  visible = *static_cast<bool *>(ret);

  RNA_parameter_list_free(&list);

  return visible;
}

static void rna_NodeTree_update_reg(bNodeTree *ntree)
{
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_id_pointer_create(&ntree->id);
  func = rna_NodeTree_update_func; /* RNA_struct_find_function(&ptr, "update"); */

  RNA_parameter_list_create(&list, &ptr, func);
  ntree->typeinfo->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_NodeTree_get_from_context(
    const bContext *C, bke::bNodeTreeType *ntreetype, bNodeTree **r_ntree, ID **r_id, ID **r_from)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret1, *ret2, *ret3;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, ntreetype->rna_ext.srna, nullptr); /* dummy */
  // RNA_struct_find_function(&ptr, "get_from_context");
  func = rna_NodeTree_get_from_context_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  ntreetype->rna_ext.call(const_cast<bContext *>(C), &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "result_1", &ret1);
  RNA_parameter_get_lookup(&list, "result_2", &ret2);
  RNA_parameter_get_lookup(&list, "result_3", &ret3);
  *r_ntree = *static_cast<bNodeTree **>(ret1);
  *r_id = *static_cast<ID **>(ret2);
  *r_from = *static_cast<ID **>(ret3);

  RNA_parameter_list_free(&list);
}

static bool rna_NodeTree_valid_socket_type(bke::bNodeTreeType *ntreetype,
                                           bke::bNodeSocketType *socket_type)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool valid;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, ntreetype->rna_ext.srna, nullptr); /* dummy */
  func = rna_NodeTree_valid_socket_type_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "idname", socket_type->idname.c_str());
  ntreetype->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "valid", &ret);
  valid = *static_cast<bool *>(ret);

  RNA_parameter_list_free(&list);

  return valid;
}

static bool rna_NodeTree_unregister(Main *bmain, StructRNA *type)
{
  bke::bNodeTreeType *nt = static_cast<bke::bNodeTreeType *>(RNA_struct_blender_type_get(type));

  if (!nt) {
    return false;
  }
  ui::refresh_for_srna_unregister(bmain, type);

  RNA_struct_free_extension(type, &nt->rna_ext);
  RNA_struct_free(&RNA_blender_rna_get(), type);

  bke::node_tree_type_free_link(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  BKE_main_ensure_invariants(*bmain);
  return true;
}

static StructRNA *rna_NodeTree_register(Main *bmain,
                                        ReportList *reports,
                                        void *data,
                                        const char *identifier,
                                        StructValidateFunc validate,
                                        StructCallbackFunc call,
                                        StructFreeFunc free)
{
  bke::bNodeTreeType *nt;
  bNodeTree dummy_ntree = {};
  bool have_function[4];

  /* setup dummy tree & tree type to store static properties in */
  bke::bNodeTreeType dummy_nt = {};
  dummy_ntree.typeinfo = &dummy_nt;
  PointerRNA dummy_ntree_ptr = RNA_pointer_create_discrete(nullptr, RNA_NodeTree, &dummy_ntree);

  /* validate the python class */
  if (validate(&dummy_ntree_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_ntree.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering node tree class: '%s' is too long, maximum length is %d",
                identifier,
                int(sizeof(dummy_ntree.idname)));
    return nullptr;
  }

  /* check if we have registered this tree type before, and remove it */
  nt = bke::node_tree_type_find(dummy_nt.idname.ref());
  if (nt) {
    BKE_reportf(reports,
                RPT_INFO,
                "Registering node tree class: '%s', bl_idname '%s' has been registered before, "
                "unregistering previous",
                identifier,
                dummy_nt.idname.c_str());

    /* NOTE: unlike most types `nt->rna_ext.srna` doesn't need to be checked for nullptr. */
    if (!rna_NodeTree_unregister(bmain, nt->rna_ext.srna)) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Registering node tree class: '%s', bl_idname '%s' could not be unregistered",
                  identifier,
                  dummy_nt.idname.c_str());
      return nullptr;
    }
  }

  /* create a new node tree type */
  nt = MEM_new<bke::bNodeTreeType>(__func__, dummy_nt);

  nt->type = NTREE_CUSTOM;

  nt->rna_ext.srna = RNA_def_struct_ptr(&RNA_blender_rna_get(), nt->idname.c_str(), RNA_NodeTree);
  nt->rna_ext.data = data;
  nt->rna_ext.call = call;
  nt->rna_ext.free = free;
  RNA_struct_blender_type_set(nt->rna_ext.srna, nt);

  RNA_def_struct_ui_text(nt->rna_ext.srna, nt->ui_name.c_str(), nt->ui_description.c_str());
  RNA_def_struct_ui_icon(nt->rna_ext.srna, nt->ui_icon);

  nt->poll = (have_function[0]) ? rna_NodeTree_poll : nullptr;
  nt->update = (have_function[1]) ? rna_NodeTree_update_reg : nullptr;
  nt->get_from_context = (have_function[2]) ? rna_NodeTree_get_from_context : nullptr;
  nt->valid_socket_type = (have_function[3]) ? rna_NodeTree_valid_socket_type : nullptr;

  bke::node_tree_type_add(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  BKE_main_ensure_invariants(*bmain);
  return nt->rna_ext.srna;
}

static bool rna_NodeTree_check(bNodeTree *ntree, ReportList *reports)
{
  if (!bke::node_tree_is_registered(*ntree)) {
    if (reports) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Node tree '%s' has undefined type %s",
                  ntree->id.name + 2,
                  ntree->idname);
    }
    return false;
  }
  else {
    return true;
  }
}

static void rna_NodeTree_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);

  WM_main_add_notifier(NC_NODE | NA_EDITED, &ntree->id);
  WM_main_add_notifier(NC_SCENE | ND_NODES, &ntree->id);

  BKE_main_ensure_invariants(*bmain, ntree->id);
}

static void rna_NodeTree_update_asset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_NodeTree_update(bmain, scene, ptr);
  WM_main_add_notifier(NC_NODE | ND_NODE_ASSET_DATA, nullptr);
  bke::node_update_asset_metadata(*reinterpret_cast<bNodeTree *>(ptr->owner_id));
}

static const EnumPropertyItem *rna_NodeTree_color_tag_itemf(bContext * /*C*/,
                                                            PointerRNA *ptr,
                                                            PropertyRNA * /*prop*/,
                                                            bool *r_free)
{
  const bNodeTree &ntree = *reinterpret_cast<const bNodeTree *>(ptr->owner_id);

  EnumPropertyItem *items = nullptr;
  int items_num = 0;

  for (const EnumPropertyItem *item = rna_enum_node_color_tag_items; item->identifier; item++) {
    switch (bke::NodeColorTag(item->value)) {
      case bke::NodeColorTag::Attribute:
      case bke::NodeColorTag::Geometry: {
        if (ntree.type == NTREE_GEOMETRY) {
          RNA_enum_item_add(&items, &items_num, item);
        }
        break;
      }
      case bke::NodeColorTag::Shader:
      case bke::NodeColorTag::Script: {
        if (ntree.type == NTREE_SHADER) {
          RNA_enum_item_add(&items, &items_num, item);
        }
        break;
      }
      case bke::NodeColorTag::Distort:
      case bke::NodeColorTag::Filter:
      case bke::NodeColorTag::Matte: {
        if (ntree.type == NTREE_COMPOSIT) {
          RNA_enum_item_add(&items, &items_num, item);
        }
        break;
      }
      case bke::NodeColorTag::Pattern:
      case bke::NodeColorTag::Interface:
      case bke::NodeColorTag::Group: {
        break;
      }
      default: {
        RNA_enum_item_add(&items, &items_num, item);
        break;
      }
    }
  }

  RNA_enum_item_end(&items, &items_num);

  *r_free = true;
  return items;
}

static bNode *rna_NodeTree_node_new(bNodeTree *ntree,
                                    bContext *C,
                                    ReportList *reports,
                                    StringRefNull type)
{
  bke::bNodeType *ntype;
  bNode *node;

  if (!rna_NodeTree_check(ntree, reports)) {
    return nullptr;
  }

  /* If the given idname is an alias, translate it to the proper idname. */
  type = bke::node_type_find_alias(UString(type)).ref();

  ntype = bke::node_type_find(UString(type));
  if (!ntype) {
    BKE_reportf(reports, RPT_ERROR, "Node type %s undefined", type.c_str());
    return nullptr;
  }

  const char *disabled_hint = nullptr;
  if (ntype->poll && !ntype->poll(ntype, ntree, &disabled_hint)) {
    if (disabled_hint) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Cannot add node of type %s to node tree '%s'\n  %s",
                  type.c_str(),
                  ntree->id.name + 2,
                  disabled_hint);
      return nullptr;
    }
    else {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Cannot add node of type %s to node tree '%s'",
                  type.c_str(),
                  ntree->id.name + 2);
      return nullptr;
    }
  }

  node = bke::node_add_node(C, *ntree, UString(type));
  BLI_assert(node && node->typeinfo);

  if (ntree->type == NTREE_TEXTURE) {
    ntreeTexCheckCyclics(ntree);
  }

  Main *bmain = CTX_data_main(C);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

  return node;
}

static void rna_NodeTree_node_remove(bNodeTree *ntree,
                                     Main *bmain,
                                     ReportList *reports,
                                     PointerRNA *node_ptr)
{
  bNode *node = node_ptr->data_as<bNode>();

  if (!rna_NodeTree_check(ntree, reports)) {
    return;
  }

  if (BLI_findindex(&ntree->nodes, node) == -1) {
    BKE_reportf(reports, RPT_ERROR, "Unable to locate node '%s' in node tree", node->name);
    return;
  }

  bke::node_remove_node(bmain, *ntree, *node, true);

  node_ptr->invalidate();

  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTree_node_clear(bNodeTree *ntree, Main *bmain, ReportList *reports)
{
  bNode *node = ntree->nodes.first();

  if (!rna_NodeTree_check(ntree, reports)) {
    return;
  }

  while (node) {
    bNode *next_node = node->next;

    bke::node_remove_node(bmain, *ntree, *node, true);

    node = next_node;
  }

  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static PointerRNA rna_NodeTree_active_node_get(PointerRNA *ptr)
{
  bNodeTree *ntree = ptr->data_as<bNodeTree>();
  bNode *node = bke::node_get_active(*ntree);
  return RNA_pointer_create_with_parent(*ptr, RNA_Node, node);
}

static void rna_NodeTree_active_node_set(PointerRNA *ptr,
                                         const PointerRNA value,
                                         ReportList * /*reports*/)
{
  bNodeTree *ntree = ptr->data_as<bNodeTree>();
  bNode *node = static_cast<bNode *>(value.data);

  if (node && BLI_findindex(&ntree->nodes, node) != -1) {
    bke::node_set_active(*ntree, *node);

    /* Handle NODE_DO_OUTPUT as well. */
    if (node->typeinfo->nclass == NODE_CLASS_OUTPUT && node->type_legacy != CMP_NODE_OUTPUT_FILE) {
      /* If this node becomes the active output, the others of the same type can't be the active
       * output anymore. */
      for (bNode *other_node : ntree->all_nodes()) {
        if (other_node->type_legacy == node->type_legacy) {
          other_node->flag &= ~NODE_DO_OUTPUT;
        }
      }
      node->flag |= NODE_DO_OUTPUT;
      bke::node_tree_set_output(*ntree);
      BKE_ntree_update_tag_active_output_changed(ntree);
    }
  }
  else {
    bke::node_clear_active(*ntree);
  }
}

static void node_viewer_set_shortcut_fn(bNode *node, bNodeTree &ntree, int value)
{
  /* Avoid having two nodes with the same shortcut. */
  for (bNode *other_node : ntree.all_nodes()) {
    if ((other_node->is_type("CompositorNodeViewer"_ustr) ||
         other_node->is_type("GeometryNodeViewer"_ustr)) &&
        other_node->custom1 == value)
    {
      other_node->custom1 = NODE_VIEWER_SHORTCUT_NONE;
    }
  }
  node->custom1 = value;
}

void rna_Node_Viewer_shortcut_node_set(PointerRNA *ptr, PropertyRNA * /*prop*/, int value)
{
  bNode *curr_node = ptr->data_as<bNode>();
  bNodeTree &ntree = curr_node->owner_tree();

  node_viewer_set_shortcut_fn(curr_node, ntree, value);
}

void rna_Node_Viewer_shortcut_node_set(PointerRNA *ptr, int value)
{
  bNode *curr_node = ptr->data_as<bNode>();
  bNodeTree &ntree = curr_node->owner_tree();

  node_viewer_set_shortcut_fn(curr_node, ntree, value);
}

static bNodeLink *rna_NodeTree_link_new(bNodeTree *ntree,
                                        Main *bmain,
                                        ReportList *reports,
                                        bNodeSocket *fromsock,
                                        bNodeSocket *tosock,
                                        bool verify_limits,
                                        bool handle_dynamic_sockets)
{
  if (!rna_NodeTree_check(ntree, reports)) {
    return nullptr;
  }

  bNode *fromnode = bke::node_find_node_try(*ntree, *fromsock);
  bNode *tonode = bke::node_find_node_try(*ntree, *tosock);
  /* check validity of the sockets:
   * if sockets from different trees are passed in this will fail!
   */
  if (!fromnode || !tonode) {
    return nullptr;
  }

  if (fromsock->in_out == tosock->in_out) {
    BKE_report(reports, RPT_ERROR, "Same input/output direction of sockets");
    return nullptr;
  }

  if (fromsock->in_out == SOCK_IN) {
    std::swap(fromsock, tosock);
    std::swap(fromnode, tonode);
  }

  if (handle_dynamic_sockets) {
    bNodeLink new_link{};
    new_link.fromnode = fromnode;
    new_link.fromsock = fromsock;
    new_link.tonode = tonode;
    new_link.tosock = tosock;

    if (fromnode->typeinfo->insert_link) {
      bke::NodeInsertLinkParams params{*ntree, *fromnode, new_link};
      if (!fromnode->typeinfo->insert_link(params)) {
        return nullptr;
      }
    }
    if (tonode->typeinfo->insert_link) {
      bke::NodeInsertLinkParams params{*ntree, *tonode, new_link};
      if (!tonode->typeinfo->insert_link(params)) {
        return nullptr;
      }
    }

    fromsock = new_link.fromsock;
    tosock = new_link.tosock;
  }

  if (verify_limits) {
    /* remove other socket links if limit is exceeded */
    if (bke::node_count_socket_links(*ntree, *fromsock) + 1 >
        bke::node_socket_link_limit(*fromsock))
    {
      bke::node_remove_socket_links(*ntree, *fromsock);
    }
    if (bke::node_count_socket_links(*ntree, *tosock) + 1 > bke::node_socket_link_limit(*tosock)) {
      bke::node_remove_socket_links(*ntree, *tosock);
    }
    if (tosock->flag & SOCK_MULTI_INPUT) {
      for (bNodeLink &link : ntree->links.items_mutable()) {
        if (link.fromsock == fromsock && link.tosock == tosock) {
          bke::node_remove_link(ntree, link);
        }
      }
    }
  }

  bNodeLink &ret = bke::node_add_link(*ntree, *fromnode, *fromsock, *tonode, *tosock);

  /* not an issue from the UI, clear hidden from API to keep valid state. */
  fromsock->flag &= ~SOCK_HIDDEN;
  tosock->flag &= ~SOCK_HIDDEN;

  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

  return &ret;
}

static void rna_NodeTree_link_remove(bNodeTree *ntree,
                                     Main *bmain,
                                     ReportList *reports,
                                     PointerRNA *link_ptr)
{
  bNodeLink *link = link_ptr->data_as<bNodeLink>();

  if (!rna_NodeTree_check(ntree, reports)) {
    return;
  }

  if (BLI_findindex(&ntree->links, link) == -1) {
    BKE_report(reports, RPT_ERROR, "Unable to locate link in node tree");
    return;
  }

  bke::node_remove_link(ntree, *link);
  link_ptr->invalidate();

  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTree_link_clear(bNodeTree *ntree, Main *bmain, ReportList *reports)
{
  bNodeLink *link = ntree->links.first();

  if (!rna_NodeTree_check(ntree, reports)) {
    return;
  }

  while (link) {
    bNodeLink *next_link = link->next;

    bke::node_remove_link(ntree, *link);

    link = next_link;
  }
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static bool rna_NodeTree_contains_tree(bNodeTree *tree, bNodeTree *sub_tree)
{
  return bke::node_tree_contains_tree(*tree, *sub_tree);
}

static void rna_NodeTree_bl_idname_get(PointerRNA *ptr, char *value)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const bke::bNodeTreeType *ntype = node->typeinfo;
  ntype->idname.ref().copy_unsafe(value);
}

static int rna_NodeTree_bl_idname_length(PointerRNA *ptr)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const bke::bNodeTreeType *ntype = node->typeinfo;
  return ntype->idname.size();
}

static void rna_NodeTree_bl_idname_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *node = ptr->data_as<bNodeTree>();
  bke::bNodeTreeType *ntype = node->typeinfo;
  ntype->idname = UString(value);
}

static void rna_NodeTree_bl_label_get(PointerRNA *ptr, char *value)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const bke::bNodeTreeType *ntype = node->typeinfo;
  StringRef(ntype->ui_name).copy_unsafe(value);
}

static int rna_NodeTree_bl_label_length(PointerRNA *ptr)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const bke::bNodeTreeType *ntype = node->typeinfo;
  return ntype->ui_name.size();
}

static void rna_NodeTree_bl_label_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *node = ptr->data_as<bNodeTree>();
  bke::bNodeTreeType *ntype = node->typeinfo;
  ntype->ui_name = value;
}

static void rna_NodeTree_bl_description_get(PointerRNA *ptr, char *value)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const bke::bNodeTreeType *ntype = node->typeinfo;
  StringRef(ntype->ui_description).copy_unsafe(value);
}

static int rna_NodeTree_bl_description_length(PointerRNA *ptr)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const bke::bNodeTreeType *ntype = node->typeinfo;
  return ntype->ui_description.size();
}

static void rna_NodeTree_bl_description_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *node = ptr->data_as<bNodeTree>();
  bke::bNodeTreeType *ntype = node->typeinfo;
  ntype->ui_description = value;
}

static void rna_NodeTree_debug_lazy_function_graph(bNodeTree *tree,
                                                   bContext *C,
                                                   const char **r_str,
                                                   int *r_len)
{
  *r_len = 0;
  *r_str = nullptr;
  if (DEG_is_original(tree)) {
    /* The graph is only stored on the evaluated data. */
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    tree = DEG_get_evaluated(depsgraph, tree);
  };
  if (tree->runtime->geometry_nodes_lazy_function_graph_info_mutex.is_dirty()) {
    return;
  }
  std::string dot_str = tree->runtime->geometry_nodes_lazy_function_graph_info->graph.to_dot();
  *r_str = BLI_strdup(dot_str.c_str());
  *r_len = dot_str.size();
}

static void rna_NodeTree_debug_zone_body_lazy_function_graph(
    ID *tree_id, bNode *node, bContext *C, const char **r_str, int *r_len)
{
  *r_len = 0;
  *r_str = nullptr;
  bNodeTree *tree = reinterpret_cast<bNodeTree *>(tree_id);
  if (DEG_is_original(tree)) {
    /* The graph is only stored on the evaluated data. */
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    tree = DEG_get_evaluated(depsgraph, tree);
  }
  if (tree->runtime->geometry_nodes_lazy_function_graph_info_mutex.is_dirty()) {
    return;
  }
  const auto *graph = tree->runtime->geometry_nodes_lazy_function_graph_info
                          ->debug_zone_body_graphs.lookup_default(node->identifier, nullptr);
  if (!graph) {
    return;
  }
  std::string dot_str = graph->to_dot();
  *r_str = BLI_strdup(dot_str.c_str());
  *r_len = dot_str.size();
}

static void rna_NodeTree_debug_zone_lazy_function_graph(
    ID *tree_id, bNode *node, bContext *C, const char **r_str, int *r_len)
{
  *r_len = 0;
  *r_str = nullptr;
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  bNodeTree *tree = reinterpret_cast<bNodeTree *>(tree_id);

  if (tree->type != NTREE_GEOMETRY) {
    return;
  }
  /* By creating this data we tell the evaluation that we want to log it. */
  tree->runtime->logged_zone_graphs = std::make_unique<bke::LoggedZoneGraphs>();
  BLI_SCOPED_DEFER([&]() { tree->runtime->logged_zone_graphs.reset(); })

  /* Make sure that dependencies of this tree will be evaluated. */
  DEG_id_tag_update_for_side_effect_request(depsgraph, &tree->id, ID_RECALC_NTREE_OUTPUT);
  /* Actually do evaluation. */
  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

  /* Get logged graph if it was evaluated. */
  std::optional<std::string> dot_str = tree->runtime->logged_zone_graphs->graph_by_zone_id.pop_try(
      node->identifier);
  if (!dot_str) {
    return;
  }
  *r_str = BLI_strdup(dot_str->c_str());
  *r_len = dot_str->size();
}

static void rna_NodeTree_interface_update(bNodeTree *ntree, bContext *C)
{
  Main *bmain = CTX_data_main(C);
  ntree->tree_interface.tag_items_changed_generic();
  BKE_main_ensure_invariants(*bmain, ntree->id);
}

/* ******** NodeLink ******** */

static bool rna_NodeLink_is_hidden_get(PointerRNA *ptr)
{
  bNodeLink *link = static_cast<bNodeLink *>(ptr->data);
  return bke::node_link_is_hidden(*link);
}

static void rna_NodeLink_swap_multi_input_sort_id(
    ID *id, bNodeLink *self, Main *bmain, ReportList *reports, bNodeLink *other)
{
  if (self->tosock != other->tosock) {
    BKE_report(reports, RPT_ERROR_INVALID_INPUT, "The links must be siblings");
    return;
  }

  std::swap(self->multi_input_sort_id, other->multi_input_sort_id);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_link_changed(ntree);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

/* ******** Node ******** */

StructRNA *rna_Node_refine(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();

  if (node->typeinfo->rna_ext.srna) {
    return node->typeinfo->rna_ext.srna;
  }
  else {
    return ptr->type;
  }
}

std::optional<std::string> rna_Node_path(const PointerRNA *ptr)
{
  const bNode *node = ptr->data_as<bNode>();
  char name_esc[sizeof(node->name) * 2];

  BLI_str_escape(name_esc, node->name);
  return fmt::format("nodes[\"{}\"]", name_esc);
}

std::optional<std::string> rna_Node_ImageUser_path(const PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (!ELEM(ntree->type, NTREE_SHADER, NTREE_CUSTOM)) {
    return std::nullopt;
  }

  for (bNode *node = ntree->nodes.first(); node; node = node->next) {
    switch (node->type_legacy) {
      case SH_NODE_TEX_ENVIRONMENT: {
        NodeTexEnvironment *data = static_cast<NodeTexEnvironment *>(node->storage);
        if (&data->iuser != ptr->data) {
          continue;
        }
        break;
      }
      case SH_NODE_TEX_IMAGE: {
        NodeTexImage *data = static_cast<NodeTexImage *>(node->storage);
        if (&data->iuser != ptr->data) {
          continue;
        }
        break;
      }
      default:
        continue;
    }

    char name_esc[sizeof(node->name) * 2];
    BLI_str_escape(name_esc, node->name);
    return fmt::format("nodes[\"{}\"].image_user", name_esc);
  }

  return std::nullopt;
}

static bool rna_Node_poll(const bke::bNodeType *ntype,
                          const bNodeTree *ntree,
                          const char ** /*r_disabled_hint*/)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool visible;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, ntype->rna_ext.srna, nullptr); /* dummy */
  func = rna_Node_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "node_tree", &ntree);
  ntype->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "visible", &ret);
  visible = *static_cast<bool *>(ret);

  RNA_parameter_list_free(&list);

  return visible;
}

static bool rna_Node_poll_instance(const bNode *node,
                                   const bNodeTree *ntree,
                                   const char ** /*disabled_info*/)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool visible;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, node->typeinfo->rna_ext.srna, const_cast<bNode *>(node)); /* dummy */
  func = rna_Node_poll_instance_func; /* RNA_struct_find_function(&ptr, "poll_instance"); */

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "node_tree", &ntree);
  node->typeinfo->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "visible", &ret);
  visible = *static_cast<bool *>(ret);

  RNA_parameter_list_free(&list);

  return visible;
}

static bool rna_Node_poll_instance_default(const bNode *node,
                                           const bNodeTree *ntree,
                                           const char **disabled_info)
{
  /* use the basic poll function */
  return rna_Node_poll(node->typeinfo, ntree, disabled_info);
}

static void rna_Node_update_reg(bNodeTree *ntree, bNode *node)
{
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(ntree), node->typeinfo->rna_ext.srna, node);
  func = rna_Node_update_func; /* RNA_struct_find_function(&ptr, "update"); */

  RNA_parameter_list_create(&list, &ptr, func);
  node->typeinfo->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static bool rna_Node_insert_link(bke::NodeInsertLinkParams &params)
{
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(&params.ntree), params.node.typeinfo->rna_ext.srna, &params.node);
  func = rna_Node_insert_link_func;

  RNA_parameter_list_create(&list, &ptr, func);
  bNodeLink *link = &params.link;
  RNA_parameter_set_lookup(&list, "link", &link);
  params.node.typeinfo->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
  return true;
}

static void rna_Node_init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  ParameterList list;
  FunctionRNA *func;

  func = rna_Node_init_func; /* RNA_struct_find_function(&ptr, "init"); */

  RNA_parameter_list_create(&list, ptr, func);
  node->typeinfo->rna_ext.call(const_cast<bContext *>(C), ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_Node_copy(PointerRNA *ptr, const bNode *copynode)
{
  bNode *node = ptr->data_as<bNode>();
  ParameterList list;
  FunctionRNA *func;

  func = rna_Node_copy_func; /* RNA_struct_find_function(&ptr, "copy"); */

  RNA_parameter_list_create(&list, ptr, func);
  RNA_parameter_set_lookup(&list, "node", &copynode);
  node->typeinfo->rna_ext.call(nullptr, ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_Node_free(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  ParameterList list;
  FunctionRNA *func;

  func = rna_Node_free_func; /* RNA_struct_find_function(&ptr, "free"); */

  RNA_parameter_list_create(&list, ptr, func);
  node->typeinfo->rna_ext.call(nullptr, ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_Node_draw_buttons(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  ParameterList list;
  FunctionRNA *func;

  func = rna_Node_draw_buttons_func; /* RNA_struct_find_function(&ptr, "draw_buttons"); */

  RNA_parameter_list_create(&list, ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  ui::Layout *layout_ptr = &layout;
  RNA_parameter_set_lookup(&list, "layout", &layout_ptr);
  node->typeinfo->rna_ext.call(C, ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_Node_draw_buttons_ext(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  ParameterList list;
  FunctionRNA *func;

  func = rna_Node_draw_buttons_ext_func; /* RNA_struct_find_function(&ptr, "draw_buttons_ext"); */

  RNA_parameter_list_create(&list, ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  ui::Layout *layout_ptr = &layout;
  RNA_parameter_set_lookup(&list, "layout", &layout_ptr);
  node->typeinfo->rna_ext.call(C, ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_Node_draw_label(const bNodeTree *ntree, const bNode *node, MutableCStr label)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  char *rlabel;

  func = rna_Node_draw_label_func; /* RNA_struct_find_function(&ptr, "draw_label"); */

  PointerRNA ptr = RNA_pointer_create_discrete(
      const_cast<ID *>(&ntree->id), RNA_Node, const_cast<bNode *>(node));
  RNA_parameter_list_create(&list, &ptr, func);
  node->typeinfo->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "label", &ret);
  rlabel = static_cast<char *>(ret);
  STRNCPY(label, rlabel != nullptr ? rlabel : "");

  RNA_parameter_list_free(&list);
}

bool rna_Node_is_registered_node_type(StructRNA *type)
{
  return (RNA_struct_blender_type_get(type) != nullptr);
}

static bool rna_Node_is_builtin(bke::bNodeType *nt)
{
  BLI_assert(nt);

  /* `nt->rna_ext.data` is the python object. If it's nullptr then it's a
   * builtin node. */
  return nt->rna_ext.data == nullptr;
}

static void rna_Node_is_registered_node_type_runtime(bContext * /*C*/,
                                                     ReportList * /*reports*/,
                                                     PointerRNA *ptr,
                                                     ParameterList *parms)
{
  int result = (RNA_struct_blender_type_get(ptr->type) != nullptr);
  RNA_parameter_set_lookup(parms, "result", &result);
}

bool rna_Node_unregister(Main *bmain, StructRNA *type)
{
  bke::bNodeType *nt = static_cast<bke::bNodeType *>(RNA_struct_blender_type_get(type));

  if (!nt || rna_Node_is_builtin(nt)) {
    return false;
  }
  ui::refresh_for_srna_unregister(bmain, type);

  RNA_struct_free_extension(type, &nt->rna_ext);
  RNA_struct_free(&RNA_blender_rna_get(), type);

  /* this also frees the allocated nt pointer, no MEM_free call needed! */
  bke::node_unregister_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  BKE_main_ensure_invariants(*bmain);
  return true;
}

/* Generic internal registration function.
 * Can be used to implement callbacks for registerable RNA node sub-types.
 */
static bke::bNodeType *rna_Node_register_base(Main *bmain,
                                              ReportList *reports,
                                              StructRNA *basetype,
                                              void *data,
                                              const char *identifier,
                                              StructValidateFunc validate,
                                              StructCallbackFunc call,
                                              StructFreeFunc free)
{
  bke::bNodeType *nt;
  bNode dummy_node = {};
  FunctionRNA *func;
  PropertyRNA *parm;
  bool have_function[10];

  /* setup dummy node & node type to store static properties in */
  bke::bNodeType dummy_nt = {};
  /* this does some additional initialization of default values */
  bke::node_type_base_custom(dummy_nt, identifier, "", "CUSTOM", 0);

  dummy_node.typeinfo = &dummy_nt;
  PointerRNA dummy_node_ptr = RNA_pointer_create_discrete(nullptr, basetype, &dummy_node);

  /* validate the python class */
  if (validate(&dummy_node_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_node.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering node class: '%s' is too long, maximum length is %d",
                identifier,
                int(sizeof(dummy_node.idname)));
    return nullptr;
  }

  /* check if we have registered this node type before, and remove it */
  nt = bke::node_type_find(dummy_nt.idname);
  if (nt) {
    /* If it's an internal node, we cannot proceed. */
    if (rna_Node_is_builtin(nt)) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Registering node class: '%s', bl_idname '%s' is a builtin node",
                  identifier,
                  dummy_nt.idname.c_str());
      return nullptr;
    }

    BKE_reportf(reports,
                RPT_INFO,
                "Registering node class: '%s', bl_idname '%s' has been registered before, "
                "unregistering previous",
                identifier,
                dummy_nt.idname.c_str());

    /* NOTE: unlike most types `nt->rna_ext.srna` doesn't need to be checked for nullptr. */
    if (!rna_Node_unregister(bmain, nt->rna_ext.srna)) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Registering node class: '%s', bl_idname '%s' could not be unregistered",
                  identifier,
                  dummy_nt.idname.c_str());
      return nullptr;
    }
  }

  /* create a new node type */
  nt = MEM_new<bke::bNodeType>(__func__, dummy_nt);
  nt->free_self = [](bke::bNodeType *type) { MEM_delete(type); };

  nt->rna_ext.srna = RNA_def_struct_ptr(&RNA_blender_rna_get(), nt->idname.c_str(), basetype);
  nt->rna_ext.data = data;
  nt->rna_ext.call = call;
  nt->rna_ext.free = free;
  RNA_struct_blender_type_set(nt->rna_ext.srna, nt);

  RNA_def_struct_ui_text(nt->rna_ext.srna, nt->ui_name.c_str(), nt->ui_description.c_str());
  RNA_def_struct_ui_icon(nt->rna_ext.srna, nt->ui_icon);

  func = RNA_def_function_runtime(
      nt->rna_ext.srna, "is_registered_node_type", rna_Node_is_registered_node_type_runtime);
  RNA_def_function_ui_description(func, "True if a registered node type");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
  parm = RNA_def_boolean(func, "result", false, "Result", "");
  RNA_def_function_return(func, parm);

  /* XXX bad level call! needed to initialize the basic draw functions ... */
  ED_init_custom_node_type(nt);

  nt->poll = (have_function[0]) ? rna_Node_poll : nullptr;
  nt->poll_instance = (have_function[1]) ? rna_Node_poll_instance : rna_Node_poll_instance_default;
  nt->updatefunc = (have_function[2]) ? rna_Node_update_reg : nullptr;
  nt->insert_link = (have_function[3]) ? rna_Node_insert_link : nullptr;
  nt->initfunc_api = (have_function[4]) ? rna_Node_init : nullptr;
  nt->copyfunc_api = (have_function[5]) ? rna_Node_copy : nullptr;
  nt->freefunc_api = (have_function[6]) ? rna_Node_free : nullptr;
  nt->draw_buttons = (have_function[7]) ? rna_Node_draw_buttons : nullptr;
  nt->draw_buttons_ex = (have_function[8]) ? rna_Node_draw_buttons_ext : nullptr;
  nt->labelfunc = (have_function[9]) ? rna_Node_draw_label : nullptr;

  /* sanitize size values in case not all have been registered */
  if (nt->maxwidth < nt->minwidth) {
    nt->maxwidth = nt->minwidth;
  }
  if (nt->maxheight < nt->minheight) {
    nt->maxheight = nt->minheight;
  }
  CLAMP(nt->default_width, nt->minwidth, nt->maxwidth);
  CLAMP(nt->height, nt->minheight, nt->maxheight);

  return nt;
}

static StructRNA *rna_Node_register(Main *bmain,
                                    ReportList *reports,
                                    void *data,
                                    const char *identifier,
                                    StructValidateFunc validate,
                                    StructCallbackFunc call,
                                    StructFreeFunc free)
{
  bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, RNA_Node, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  BKE_main_ensure_invariants(*bmain);
  return nt->rna_ext.srna;
}

static bool compositor_node_asset_trait_flag_get(PointerRNA *ptr,
                                                 const CompositorNodeAssetTraitFlag flag)
{
  const bNodeTree *ntree = ptr->data_as<bNodeTree>();
  if (!ntree->compositor_node_asset_traits) {
    return false;
  }
  return ntree->compositor_node_asset_traits->flag & flag;
}
static bool rna_CompositorNodeTree_is_strip_modifier_get(PointerRNA *ptr)
{
  return compositor_node_asset_trait_flag_get(ptr, COMPOSIT_NODE_ASSET_STRIP_MODIFIER);
}
static void compositor_node_asset_trait_flag_set(PointerRNA *ptr,
                                                 const CompositorNodeAssetTraitFlag flag,
                                                 const bool value)
{
  bNodeTree *ntree = ptr->data_as<bNodeTree>();
  if (!ntree->compositor_node_asset_traits) {
    ntree->compositor_node_asset_traits = MEM_new<CompositorNodeAssetTraits>(__func__);
  }
  SET_FLAG_FROM_TEST(ntree->compositor_node_asset_traits->flag, value, flag);
}
static void rna_CompositorNodeTree_is_strip_modifier_set(PointerRNA *ptr, bool value)
{
  compositor_node_asset_trait_flag_set(ptr, COMPOSIT_NODE_ASSET_STRIP_MODIFIER, value);
}

static bool rna_CompositorNodeTree_allow_usage_in_scene_compositor_effect_get(PointerRNA *ptr)
{
  return compositor_node_asset_trait_flag_get(ptr, COMPOSIT_NODE_ASSET_SCENE_EFFECT);
}

static void rna_CompositorNodeTree_allow_usage_in_scene_compositor_effect_set(PointerRNA *ptr,
                                                                              bool value)
{
  compositor_node_asset_trait_flag_set(ptr, COMPOSIT_NODE_ASSET_SCENE_EFFECT, value);
}

const EnumPropertyItem *itemf_function_check(
    const EnumPropertyItem *original_item_array,
    FunctionRef<bool(const EnumPropertyItem *item)> value_supported)
{
  EnumPropertyItem *item_array = nullptr;
  int items_len = 0;

  for (const EnumPropertyItem *item = original_item_array; item->identifier != nullptr; item++) {
    if (value_supported(item)) {
      RNA_enum_item_add(&item_array, &items_len, item);
    }
  }

  RNA_enum_item_end(&item_array, &items_len);
  return item_array;
}

static bool geometry_node_asset_trait_flag_get(PointerRNA *ptr,
                                               const GeometryNodeAssetTraitFlag flag)
{
  const bNodeTree *ntree = ptr->data_as<bNodeTree>();
  if (!ntree->geometry_node_asset_traits) {
    return false;
  }
  return ntree->geometry_node_asset_traits->flag & flag;
}

static void rna_GeometryNodeTree_node_tool_idname_get(PointerRNA *ptr, char *value)
{
  bNodeTree *ntree = ptr->data_as<bNodeTree>();
  if (!ntree->geometry_node_asset_traits) {
    value[0] = '\0';
    return;
  }
  const char *idname = ntree->geometry_node_asset_traits->node_tool_idname;
  strcpy(value, idname ? idname : "");
}

static int rna_GeometryNodeTree_node_tool_idname_length(PointerRNA *ptr)
{
  bNodeTree *ntree = ptr->data_as<bNodeTree>();
  if (!ntree->geometry_node_asset_traits) {
    return 0;
  }
  const char *idname = ntree->geometry_node_asset_traits->node_tool_idname;
  return idname ? strlen(idname) : 0;
}

static void rna_GeometryNodeTree_node_tool_idname_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *ntree = ptr->data_as<bNodeTree>();
  if (!ntree->geometry_node_asset_traits) {
    ntree->geometry_node_asset_traits = MEM_new<GeometryNodeAssetTraits>(__func__);
  }
  if (ntree->geometry_node_asset_traits->node_tool_idname) {
    MEM_delete(ntree->geometry_node_asset_traits->node_tool_idname);
  }
  if (value && value[0]) {
    ntree->geometry_node_asset_traits->node_tool_idname = BLI_strdup(value);
  }
  else {
    ntree->geometry_node_asset_traits->node_tool_idname = nullptr;
  }
}

static void geometry_node_asset_trait_flag_set(PointerRNA *ptr,
                                               const GeometryNodeAssetTraitFlag flag,
                                               const bool value)
{
  bNodeTree *ntree = ptr->data_as<bNodeTree>();
  if (!ntree->geometry_node_asset_traits) {
    ntree->geometry_node_asset_traits = MEM_new<GeometryNodeAssetTraits>(__func__);
  }
  SET_FLAG_FROM_TEST(ntree->geometry_node_asset_traits->flag, value, flag);
}

static bool rna_GeometryNodeTree_is_tool_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_TOOL);
}
static void rna_GeometryNodeTree_is_tool_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_TOOL, value);
}

static bool rna_GeometryNodeTree_is_modifier_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_MODIFIER);
}
static void rna_GeometryNodeTree_is_modifier_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_MODIFIER, value);
}

static bool rna_GeometryNodeTree_is_mode_object_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_OBJECT);
}
static void rna_GeometryNodeTree_is_mode_object_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_OBJECT, value);
}

static bool rna_GeometryNodeTree_is_mode_edit_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_EDIT);
}
static void rna_GeometryNodeTree_is_mode_edit_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_EDIT, value);
}

static bool rna_GeometryNodeTree_is_mode_sculpt_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_SCULPT);
}
static void rna_GeometryNodeTree_is_mode_sculpt_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_SCULPT, value);
}

static bool rna_GeometryNodeTree_is_mode_paint_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_PAINT);
}
static void rna_GeometryNodeTree_is_mode_paint_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_PAINT, value);
}

static bool rna_GeometryNodeTree_is_type_mesh_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_MESH);
}
static void rna_GeometryNodeTree_is_type_mesh_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_MESH, value);
}

static bool rna_GeometryNodeTree_is_type_curve_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_CURVE);
}
static void rna_GeometryNodeTree_is_type_curve_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_CURVE, value);
}

static bool rna_GeometryNodeTree_is_type_pointcloud_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_POINTCLOUD);
}
static void rna_GeometryNodeTree_is_type_pointcloud_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_POINTCLOUD, value);
}

static bool rna_GeometryNodeTree_use_wait_for_click_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_WAIT_FOR_CURSOR);
}
static void rna_GeometryNodeTree_use_wait_for_click_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_WAIT_FOR_CURSOR, value);
}

static bool rna_GeometryNodeTree_is_type_grease_pencil_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_GREASE_PENCIL);
}
static void rna_GeometryNodeTree_is_type_grease_pencil_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_GREASE_PENCIL, value);
}

static bool rna_GeometryNodeTree_modifier_manage_panel_get(PointerRNA *ptr)
{
  return !geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_HIDE_MODIFIER_MANAGE_PANEL);
}
static void rna_GeometryNodeTree_modifier_manage_panel_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_HIDE_MODIFIER_MANAGE_PANEL, !value);
}

StructRNA *rna_ShaderNode_register(Main *bmain,
                                   ReportList *reports,
                                   void *data,
                                   const char *identifier,
                                   StructValidateFunc validate,
                                   StructCallbackFunc call,
                                   StructFreeFunc free)
{
  bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, RNA_ShaderNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

StructRNA *rna_CompositorNode_register(Main *bmain,
                                       ReportList *reports,
                                       void *data,
                                       const char *identifier,
                                       StructValidateFunc validate,
                                       StructCallbackFunc call,
                                       StructFreeFunc free)
{
  bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, RNA_CompositorNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

StructRNA *rna_TextureNode_register(Main *bmain,
                                    ReportList *reports,
                                    void *data,
                                    const char *identifier,
                                    StructValidateFunc validate,
                                    StructCallbackFunc call,
                                    StructFreeFunc free)
{
  bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, RNA_TextureNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

StructRNA *rna_GeometryNode_register(Main *bmain,
                                     ReportList *reports,
                                     void *data,
                                     const char *identifier,
                                     StructValidateFunc validate,
                                     StructCallbackFunc call,
                                     StructFreeFunc free)
{
  bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, RNA_GeometryNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

StructRNA *rna_FunctionNode_register(Main *bmain,
                                     ReportList *reports,
                                     void *data,
                                     const char *identifier,
                                     StructValidateFunc validate,
                                     StructCallbackFunc call,
                                     StructFreeFunc free)
{
  bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, RNA_FunctionNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

IDProperty **rna_Node_idprops(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  return &node->prop;
}

IDProperty **rna_Node_system_idprops(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  return &node->system_properties;
}

static void rna_Node_parent_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  bNode *node = ptr->data_as<bNode>();
  bNode *parent = value.data_as<bNode>();
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  const bNodeTree *parent_ntree = reinterpret_cast<bNodeTree *>(value.owner_id);

  if (!parent) {
    bke::node_detach_node(*ntree, *node);
    return;
  }

  /* XXX only Frame node allowed for now,
   * in the future should have a poll function or so to test possible attachment.
   */
  if (!parent->is_frame()) {
    return;
  }

  if (bke::node_is_parent_and_child(*node, *parent)) {
    return;
  }

  /* Only nodes from the same tree can be used as parents. */
  if (parent_ntree != ntree) {
    return;
  }

  bke::node_detach_node(*ntree, *node);
  bke::node_attach_node(*ntree, *node, *parent);
}

static void rna_Node_internal_links_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  rna_iterator_array_begin(iter,
                           ptr,
                           node->runtime->internal_links.data(),
                           sizeof(bNodeInternalLink),
                           node->runtime->internal_links.size(),
                           false,
                           nullptr);
}

static PointerRNA rna_NodeInternalLink_from_socket_get(PointerRNA *ptr)
{
  bNodeInternalLink *link = static_cast<bNodeInternalLink *>(ptr->data);
  return RNA_pointer_create_id_subdata(*ptr->owner_id, RNA_NodeSocket, link->in);
}

static PointerRNA rna_NodeInternalLink_to_socket_get(PointerRNA *ptr)
{
  bNodeInternalLink *link = static_cast<bNodeInternalLink *>(ptr->data);
  return RNA_pointer_create_id_subdata(*ptr->owner_id, RNA_NodeSocket, link->out);
}

/**
 * Forbid identifier lookup in nodes whose identifiers are likely to change soon because
 * dynamically typed sockets are joined into one.
 */
static bool allow_identifier_lookup(const bNode &node)
{
  switch (node.type_legacy) {
    case SH_NODE_MIX:
    case SH_NODE_MAP_RANGE:
      return false;
    default:
      return true;
  }
}

static bNodeSocket *find_socket_by_key(bNode &node,
                                       const eNodeSocketInOut in_out,
                                       const StringRef key)
{
  ListBaseT<bNodeSocket> *sockets = in_out == SOCK_IN ? &node.inputs : &node.outputs;
  if (allow_identifier_lookup(node)) {
    for (bNodeSocket &socket : *sockets) {
      if (socket.is_available()) {
        if (socket.identifier == key) {
          return &socket;
        }
      }
    }
  }
  for (bNodeSocket &socket : *sockets) {
    if (socket.is_available()) {
      if (socket.name == key) {
        return &socket;
      }
    }
  }
  return nullptr;
}

static bool rna_NodeInputs_lookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
  bNode *node = ptr->data_as<bNode>();
  if (bNodeSocket *socket = find_socket_by_key(*node, SOCK_IN, key)) {
    rna_pointer_create_with_ancestors(*ptr, RNA_NodeSocket, socket, *r_ptr);
    return true;
  }
  return false;
}

static bool rna_NodeOutputs_lookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
  bNode *node = ptr->data_as<bNode>();
  if (bNodeSocket *socket = find_socket_by_key(*node, SOCK_OUT, key)) {
    rna_pointer_create_with_ancestors(*ptr, RNA_NodeSocket, socket, *r_ptr);
    return true;
  }
  return false;
}

static bool rna_Node_parent_poll(PointerRNA *ptr, PointerRNA value)
{
  bNode *node = ptr->data_as<bNode>();
  bNode *parent = static_cast<bNode *>(value.data);

  /* XXX only Frame node allowed for now,
   * in the future should have a poll function or so to test possible attachment.
   */
  if (!parent->is_frame()) {
    return false;
  }

  if (node->is_frame() && bke::node_is_parent_and_child(*node, *parent)) {
    return false;
  }

  return true;
}

void rna_Node_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();
  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
}

void rna_Node_update_relations(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Node_update(bmain, scene, ptr);
  DEG_relations_tag_update(bmain);
}

static void rna_Node_socket_value_update(ID *id, bNode * /*node*/, bContext *C)
{
  BKE_ntree_update_tag_all(reinterpret_cast<bNodeTree *>(id));
  BKE_main_ensure_invariants(*CTX_data_main(C), *id);
}

static void rna_Node_select_set(PointerRNA *ptr, bool value)
{
  bNode *node = ptr->data_as<bNode>();
  bke::node_set_selected(*node, value);
}

static void rna_Node_mute_set(PointerRNA *ptr, bool value)
{
  bNode *node = ptr->data_as<bNode>();
  if (!node->typeinfo->no_muting) {
    SET_FLAG_FROM_TEST(node->flag, value, NODE_MUTED);
  }
}

static void rna_Node_name_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();
  char oldname[sizeof(node->name)];

  /* make a copy of the old name first */
  STRNCPY(oldname, node->name);
  /* set new name */
  STRNCPY_UTF8(node->name, value);

  bke::node_unique_name(*ntree, *node);

  /* fix all the animation data which may link to this */
  if (Main *bmain = ntree->id.owner_bmain_for_write()) {
    BKE_animdata_fix_paths(ntree->id,
                           "nodes",
                           RNA_path_name_to_infix(oldname),
                           RNA_path_name_to_infix(node->name),
                           /*verify_paths=*/true,
                           *bmain);
  }
}

static int rna_Node_color_tag_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  return int(bke::node_color_tag(*node));
}

static bool allow_adding_sockets(const bNode &node)
{
  return ELEM(node.type_legacy, NODE_CUSTOM, SH_NODE_SCRIPT);
}

static bool allow_changing_sockets(bNode *node)
{
  return ELEM(node->type_legacy, NODE_CUSTOM, SH_NODE_SCRIPT, CMP_NODE_OUTPUT_FILE);
}

static bNodeSocket *rna_Node_inputs_new(ID *id,
                                        bNode *node,
                                        Main *bmain,
                                        ReportList *reports,
                                        const char *type,
                                        const char *name,
                                        const char *identifier,
                                        const bool use_multi_input)
{
  if (!allow_adding_sockets(*node)) {
    BKE_report(reports, RPT_ERROR, "Cannot add socket to built-in node");
    return nullptr;
  }
  if (identifier == nullptr) {
    /* Use the name as default identifier if no separate identifier is provided. */
    identifier = name;
  }

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  bNodeSocket *sock = bke::node_add_socket(*ntree, *node, SOCK_IN, type, identifier, name);

  if (sock == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to create socket");
  }
  else {
    if (use_multi_input) {
      sock->flag |= SOCK_MULTI_INPUT;
    }
    BKE_main_ensure_invariants(*bmain, ntree->id);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }

  return sock;
}

static bNodeSocket *rna_Node_outputs_new(ID *id,
                                         bNode *node,
                                         Main *bmain,
                                         ReportList *reports,
                                         const char *type,
                                         const char *name,
                                         const char *identifier,
                                         const bool use_multi_input)
{
  if (!allow_adding_sockets(*node)) {
    BKE_report(reports, RPT_ERROR, "Cannot add socket to built-in node");
    return nullptr;
  }

  if (use_multi_input) {
    BKE_report(reports, RPT_ERROR, "Output sockets cannot be multi-input");
    return nullptr;
  }

  if (identifier == nullptr) {
    /* Use the name as default identifier if no separate identifier is provided. */
    identifier = name;
  }

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  bNodeSocket *sock = bke::node_add_socket(*ntree, *node, SOCK_OUT, type, identifier, name);

  if (sock == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to create socket");
  }
  else {
    BKE_main_ensure_invariants(*bmain, ntree->id);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }

  return sock;
}

static void rna_Node_socket_remove(
    ID *id, bNode *node, Main *bmain, ReportList *reports, bNodeSocket *sock)
{
  if (!allow_changing_sockets(node)) {
    BKE_report(reports, RPT_ERROR, "Unable to remove socket from built-in node");
    return;
  }

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);

  if (BLI_findindex(&node->inputs, sock) == -1 && BLI_findindex(&node->outputs, sock) == -1) {
    BKE_reportf(reports, RPT_ERROR, "Unable to locate socket '%s' in node", sock->identifier);
  }
  else {
    bke::node_remove_socket(*ntree, *node, *sock);

    BKE_main_ensure_invariants(*bmain, ntree->id);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }
}

static void rna_Node_inputs_clear(ID *id, bNode *node, Main *bmain, ReportList *reports)
{
  if (!allow_changing_sockets(node)) {
    BKE_report(reports, RPT_ERROR, "Unable to remove sockets from built-in node");
    return;
  }

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  bNodeSocket *sock, *nextsock;

  for (sock = node->inputs.first(); sock; sock = nextsock) {
    nextsock = sock->next;
    bke::node_remove_socket(*ntree, *node, *sock);
  }

  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_Node_outputs_clear(ID *id, bNode *node, Main *bmain, ReportList *reports)
{
  if (!allow_changing_sockets(node)) {
    BKE_report(reports, RPT_ERROR, "Unable to remove socket from built-in node");
    return;
  }

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  bNodeSocket *sock, *nextsock;

  for (sock = node->outputs.first(); sock; sock = nextsock) {
    nextsock = sock->next;
    bke::node_remove_socket(*ntree, *node, *sock);
  }

  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_Node_inputs_move(
    ID *id, bNode *node, Main *bmain, ReportList *reports, int from_index, int to_index)
{
  if (!allow_changing_sockets(node)) {
    BKE_report(reports, RPT_ERROR, "Unable to move sockets in built-in node");
    return;
  }

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  bNodeSocket *sock;

  if (from_index == to_index) {
    return;
  }
  if (from_index < 0 || to_index < 0) {
    return;
  }

  sock = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, from_index));
  if (to_index < from_index) {
    bNodeSocket *nextsock = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, to_index));
    if (nextsock) {
      BLI_remlink(&node->inputs, sock);
      BLI_insertlinkbefore(&node->inputs, nextsock, sock);
    }
  }
  else {
    bNodeSocket *prevsock = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, to_index));
    if (prevsock) {
      BLI_remlink(&node->inputs, sock);
      BLI_insertlinkafter(&node->inputs, prevsock, sock);
    }
  }

  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_Node_outputs_move(
    ID *id, bNode *node, Main *bmain, ReportList *reports, int from_index, int to_index)
{
  if (!allow_changing_sockets(node)) {
    BKE_report(reports, RPT_ERROR, "Unable to move sockets in built-in node");
    return;
  }

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  bNodeSocket *sock;

  if (from_index == to_index) {
    return;
  }
  if (from_index < 0 || to_index < 0) {
    return;
  }

  sock = static_cast<bNodeSocket *>(BLI_findlink(&node->outputs, from_index));
  if (to_index < from_index) {
    bNodeSocket *nextsock = static_cast<bNodeSocket *>(BLI_findlink(&node->outputs, to_index));
    if (nextsock) {
      BLI_remlink(&node->outputs, sock);
      BLI_insertlinkbefore(&node->outputs, nextsock, sock);
    }
  }
  else {
    bNodeSocket *prevsock = static_cast<bNodeSocket *>(BLI_findlink(&node->outputs, to_index));
    if (prevsock) {
      BLI_remlink(&node->outputs, sock);
      BLI_insertlinkafter(&node->outputs, prevsock, sock);
    }
  }

  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_Node_width_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  bNode *node = ptr->data_as<bNode>();
  *min = *softmin = node->typeinfo->minwidth;
  *max = *softmax = node->typeinfo->maxwidth;
}

static void rna_Node_height_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  bNode *node = ptr->data_as<bNode>();
  *min = *softmin = node->typeinfo->minheight;
  *max = *softmax = node->typeinfo->maxheight;
}

static void rna_Node_dimensions_get(PointerRNA *ptr, float *value)
{
  bNode *node = ptr->data_as<bNode>();
  const float2 dimensions = bke::node_dimensions_get(*node);
  value[0] = dimensions[0];
  value[1] = dimensions[1];
}

/* ******** Node Types ******** */

static void rna_NodeInternalSocketTemplate_name_get(PointerRNA *ptr, char *value)
{
  bke::bNodeSocketTemplate *stemp = static_cast<bke::bNodeSocketTemplate *>(ptr->data);
  strcpy(value, stemp->name);
}

static int rna_NodeInternalSocketTemplate_name_length(PointerRNA *ptr)
{
  bke::bNodeSocketTemplate *stemp = static_cast<bke::bNodeSocketTemplate *>(ptr->data);
  return strlen(stemp->name);
}

static void rna_NodeInternalSocketTemplate_identifier_get(PointerRNA *ptr, char *value)
{
  bke::bNodeSocketTemplate *stemp = static_cast<bke::bNodeSocketTemplate *>(ptr->data);
  strcpy(value, stemp->identifier);
}

static int rna_NodeInternalSocketTemplate_identifier_length(PointerRNA *ptr)
{
  bke::bNodeSocketTemplate *stemp = static_cast<bke::bNodeSocketTemplate *>(ptr->data);
  return strlen(stemp->identifier);
}

static int rna_NodeInternalSocketTemplate_type_get(PointerRNA *ptr)
{
  bke::bNodeSocketTemplate *stemp = static_cast<bke::bNodeSocketTemplate *>(ptr->data);
  return stemp->type;
}

PointerRNA rna_NodeInternal_input_template(StructRNA *srna, int index)
{
  bke::bNodeType *ntype = static_cast<bke::bNodeType *>(RNA_struct_blender_type_get(srna));
  if (ntype && ntype->inputs) {
    bke::bNodeSocketTemplate *stemp = ntype->inputs;
    int i = 0;
    while (i < index && stemp->type >= 0) {
      i++;
      stemp++;
    }
    if (i == index && stemp->type >= 0) {
      PointerRNA ptr = RNA_pointer_create_discrete(nullptr, RNA_NodeInternalSocketTemplate, stemp);
      return ptr;
    }
  }
  return {};
}

PointerRNA rna_NodeInternal_output_template(StructRNA *srna, int index)
{
  bke::bNodeType *ntype = static_cast<bke::bNodeType *>(RNA_struct_blender_type_get(srna));
  if (ntype && ntype->outputs) {
    bke::bNodeSocketTemplate *stemp = ntype->outputs;
    int i = 0;
    while (i < index && stemp->type >= 0) {
      i++;
      stemp++;
    }
    if (i == index && stemp->type >= 0) {
      PointerRNA ptr = RNA_pointer_create_discrete(nullptr, RNA_NodeInternalSocketTemplate, stemp);
      return ptr;
    }
  }
  return {};
}

static bool rna_NodeInternal_poll(StructRNA *srna, bNodeTree *ntree)
{
  bke::bNodeType *ntype = static_cast<bke::bNodeType *>(RNA_struct_blender_type_get(srna));
  const char *disabled_hint;
  return ntype && (!ntype->poll || ntype->poll(ntype, ntree, &disabled_hint));
}

static bool rna_NodeInternal_poll_instance(bNode *node, bNodeTree *ntree)
{
  bke::bNodeType *ntype = node->typeinfo;
  const char *disabled_hint;
  if (ntype->poll_instance) {
    return ntype->poll_instance(node, ntree, &disabled_hint);
  }
  else {
    /* fall back to basic poll function */
    return !ntype->poll || ntype->poll(ntype, ntree, &disabled_hint);
  }
}

static void rna_NodeInternal_update(ID *id, bNode *node, Main *bmain)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
}

static void rna_NodeInternal_draw_buttons(ID *id, bNode *node, bContext *C, ui::Layout *layout)
{
  if (node->typeinfo->draw_buttons) {
    PointerRNA ptr = RNA_pointer_create_discrete(id, RNA_Node, node);
    node->typeinfo->draw_buttons(*layout, C, &ptr);
  }
}

static void rna_NodeInternal_draw_buttons_ext(ID *id, bNode *node, bContext *C, ui::Layout *layout)
{
  if (node->typeinfo->draw_buttons_ex) {
    PointerRNA ptr = RNA_pointer_create_discrete(id, RNA_Node, node);
    node->typeinfo->draw_buttons_ex(*layout, C, &ptr);
  }
  else if (node->typeinfo->draw_buttons) {
    PointerRNA ptr = RNA_pointer_create_discrete(id, RNA_Node, node);
    node->typeinfo->draw_buttons(*layout, C, &ptr);
  }
}

static StructRNA *rna_NodeCustomGroup_register(Main *bmain,
                                               ReportList *reports,
                                               void *data,
                                               const char *identifier,
                                               StructValidateFunc validate,
                                               StructCallbackFunc call,
                                               StructFreeFunc free)
{
  bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, RNA_NodeCustomGroup, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

static StructRNA *rna_GeometryNodeCustomGroup_register(Main *bmain,
                                                       ReportList *reports,
                                                       void *data,
                                                       const char *identifier,
                                                       StructValidateFunc validate,
                                                       StructCallbackFunc call,
                                                       StructFreeFunc free)
{
  bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, RNA_GeometryNodeCustomGroup, data, identifier, validate, call, free);

  if (!nt) {
    return nullptr;
  }

  nt->type_legacy = NODE_CUSTOM_GROUP;
  nt->ui_class = node_group_ui_class;

  register_node_type_geo_custom_group(nt);

  bke::node_register_type(*nt);

  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

void register_node_type_geo_custom_group(bke::bNodeType *ntype);

static StructRNA *rna_ShaderNodeCustomGroup_register(Main *bmain,
                                                     ReportList *reports,
                                                     void *data,
                                                     const char *identifier,
                                                     StructValidateFunc validate,
                                                     StructCallbackFunc call,
                                                     StructFreeFunc free)
{
  bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, RNA_ShaderNodeCustomGroup, data, identifier, validate, call, free);

  if (!nt) {
    return nullptr;
  }

  nt->type_legacy = NODE_CUSTOM_GROUP;
  nt->ui_class = node_group_ui_class;

  register_node_type_sh_custom_group(nt);

  bke::node_register_type(*nt);

  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

static StructRNA *rna_CompositorNodeCustomGroup_register(Main *bmain,
                                                         ReportList *reports,
                                                         void *data,
                                                         const char *identifier,
                                                         StructValidateFunc validate,
                                                         StructCallbackFunc call,
                                                         StructFreeFunc free)
{
  bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, RNA_CompositorNodeCustomGroup, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  nt->type_legacy = NODE_CUSTOM_GROUP;
  nt->ui_class = node_group_ui_class;

  register_node_type_cmp_custom_group(nt);

  bke::node_register_type(*nt);

  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

void rna_NodeGroup_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();

  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  DEG_relations_tag_update(bmain);
}

void rna_NodeGroup_node_tree_set(PointerRNA *ptr, const PointerRNA value, ReportList * /*reports*/)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();
  bNodeTree *ngroup = static_cast<bNodeTree *>(value.data);

  const char *disabled_hint = nullptr;
  if (bke::node_group_poll(ntree, ngroup, &disabled_hint)) {
    if (node->id) {
      id_us_min(node->id);
    }
    if (ngroup) {
      id_us_plus(&ngroup->id);
    }

    node->id = &ngroup->id;
  }
}

bool rna_NodeGroup_node_tree_poll(PointerRNA *ptr, const PointerRNA value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeTree *ngroup = static_cast<bNodeTree *>(value.data);

  /* only allow node trees of the same type as the group node's tree */
  if (ngroup->type != ntree->type) {
    return false;
  }

  const char *disabled_hint = nullptr;
  return bke::node_group_poll(ntree, ngroup, &disabled_hint);
}

PointerRNA rna_Node_paired_output_get(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();
  const bke::bNodeZoneType &zone_type = *bke::zone_type_by_node_type(node->type_legacy);
  bNode *output_node = zone_type.get_corresponding_output(*ntree, *node);
  PointerRNA ptr_result = RNA_pointer_create_discrete(&ntree->id, RNA_Node, output_node);
  return ptr_result;
}

bool rna_Node_pair_with_output(
    ID *id, bNode *node, bContext *C, ReportList *reports, bNode *output_node)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  const bke::bNodeZoneType &zone_type = *bke::zone_type_by_node_type(node->type_legacy);
  if (output_node->type_legacy != zone_type.output_type) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Cannot pair zone input node %s with %s because it does not have the same zone type",
        node->name,
        output_node->name);
    return false;
  }
  for (const bNode *other_input_node : ntree->nodes_by_type(zone_type.input_idname)) {
    if (other_input_node != node) {
      if (zone_type.get_corresponding_output(*ntree, *other_input_node) == output_node) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "The output node %s is already paired with %s",
                    output_node->name,
                    other_input_node->name);
        return false;
      }
    }
  }
  int &output_node_id = zone_type.get_corresponding_output_id(*node);
  output_node_id = output_node->identifier;

  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*CTX_data_main(C), ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  return true;
}

/* ******** Node Socket Types ******** */

static void rna_FrameNode_label_size_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  BLF_cache_clear();
  rna_Node_update(bmain, scene, ptr);
}

void rna_ShaderNode_socket_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Node_update(bmain, scene, ptr);
}

void rna_Node_socket_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Node_update(bmain, scene, ptr);
}

static void rna_GroupOutput_is_active_output_set(PointerRNA *ptr, bool value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();
  if (value) {
    /* Make sure that no other group output is active at the same time. */
    for (bNode *other_node : ntree->all_nodes()) {
      if (other_node->is_group_output()) {
        other_node->flag &= ~NODE_DO_OUTPUT;
      }
    }
    node->flag |= NODE_DO_OUTPUT;
  }
  else {
    node->flag &= ~NODE_DO_OUTPUT;
  }
}

static void rna_reroute_node_socket_type_set(PointerRNA *ptr, const char *value)
{
  const bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bke::bNodeTreeType *ntree_type = ntree.typeinfo;

  bNode &node = *ptr->data_as<bNode>();

  if (value == nullptr) {
    return;
  }
  bke::bNodeSocketType *socket_type = bke::node_socket_type_find(value);
  if (socket_type == nullptr) {
    return;
  }
  if (socket_type->subtype != PROP_NONE) {
    return;
  }
  if (ntree_type->valid_socket_type && !ntree_type->valid_socket_type(ntree_type, socket_type)) {
    return;
  }
  NodeReroute *storage = static_cast<NodeReroute *>(node.storage);
  STRNCPY(storage->type_idname, value);
}

static void rna_implicit_conversion_node_socket_type_set(PointerRNA *ptr, const char *value)
{
  const bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bke::bNodeTreeType *ntree_type = ntree.typeinfo;

  bNode &node = *ptr->data_as<bNode>();

  if (value == nullptr) {
    return;
  }
  bke::bNodeSocketType *socket_type = bke::node_socket_type_find(value);
  if (socket_type == nullptr) {
    return;
  }
  if (socket_type->subtype != PROP_NONE) {
    return;
  }
  if (ntree_type->valid_socket_type && !ntree_type->valid_socket_type(ntree_type, socket_type)) {
    return;
  }
  NodeImplicitConversion *storage = static_cast<NodeImplicitConversion *>(node.storage);
  STRNCPY(storage->type_idname, value);
}

static int rna_NodeImplicitConversion_data_type_get(PointerRNA *ptr)
{
  const bNode &node = *ptr->data_as<bNode>();
  const NodeImplicitConversion &data = *static_cast<NodeImplicitConversion *>(node.storage);
  const bke::bNodeSocketType *socket_type = bke::node_socket_type_find(data.type_idname);
  return socket_type ? socket_type->type : SOCK_CUSTOM;
}

static void rna_NodeImplicitConversion_data_type_set(PointerRNA *ptr, const int value)
{
  const bke::bNodeSocketType *socket_type = bke::node_socket_type_find_static(value);
  if (socket_type) {
    bNode &node = *ptr->data_as<bNode>();
    NodeImplicitConversion &data = *static_cast<NodeImplicitConversion *>(node.storage);
    STRNCPY(data.type_idname, socket_type->idname.c_str());
  }
}

static const EnumPropertyItem *rna_NodeImplicitConversion_data_type_itemf(bContext * /*C*/,
                                                                          PointerRNA *ptr,
                                                                          PropertyRNA * /*prop*/,
                                                                          bool *r_free)
{
  *r_free = true;
  bNodeTree &ntree = *id_cast<bNodeTree *>(ptr->owner_id);
  return itemf_function_check(
      rna_enum_node_socket_data_type_items, [&](const EnumPropertyItem *item) {
        bke::bNodeSocketType *socket_type = bke::node_socket_type_find_static(item->value);
        if (!socket_type) {
          return false;
        }
        if (ntree.typeinfo->valid_socket_type &&
            !ntree.typeinfo->valid_socket_type(ntree.typeinfo, socket_type))
        {
          return false;
        }
        return true;
      });
}

}  // namespace blender

#else

namespace blender {

const EnumPropertyItem prop_image_extension[] = {
    {SHD_IMAGE_EXTENSION_REPEAT,
     "REPEAT",
     0,
     "Repeat",
     "Cause the image to repeat horizontally and vertically"},
    {SHD_IMAGE_EXTENSION_EXTEND,
     "EXTEND",
     0,
     "Extend",
     "Extend by repeating edge pixels of the image"},
    {SHD_IMAGE_EXTENSION_CLIP,
     "CLIP",
     0,
     "Clip",
     "Clip to image size and set exterior pixels as transparent"},
    {SHD_IMAGE_EXTENSION_MIRROR,
     "MIRROR",
     0,
     "Mirror",
     "Repeatedly flip the image horizontally and vertically"},
    {0, nullptr, 0, nullptr, nullptr},
};

void rna_def_node_item_array_socket_item_common(StructRNA *srna,
                                                const char *accessor,
                                                const bool add_socket_type,
                                                const bool add_vector_socket_dimensions)
{
  static LinearAllocator<> allocator;
  PropertyRNA *prop;

  char name_set_func[128];
  SNPRINTF(name_set_func, "rna_Node_ItemArray_item_name_set<%s>", accessor);

  char item_update_func[128];
  SNPRINTF(item_update_func, "rna_Node_ItemArray_item_update<%s>", accessor);
  const char *item_update_func_ptr = allocator.copy_string(item_update_func).c_str();

  char socket_type_itemf[128];
  SNPRINTF(socket_type_itemf, "rna_Node_ItemArray_socket_type_itemf<%s>", accessor);

  char color_get_func[128];
  SNPRINTF(color_get_func, "rna_Node_ItemArray_item_color_get<%s>", accessor);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, nullptr, nullptr, allocator.copy_string(name_set_func).c_str());
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, item_update_func_ptr);

  if (add_socket_type) {
    prop = RNA_def_property(srna, "socket_type", PROP_ENUM, PROP_NONE);
    RNA_def_property_enum_items(prop, rna_enum_node_socket_data_type_items);
    RNA_def_property_enum_funcs(
        prop, nullptr, nullptr, allocator.copy_string(socket_type_itemf).c_str());
    RNA_def_property_ui_text(prop, "Socket Type", "");
    RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
    RNA_def_property_update(prop, NC_NODE | NA_EDITED, item_update_func_ptr);

    if (add_vector_socket_dimensions) {
      prop = RNA_def_property(srna, "vector_socket_dimensions", PROP_INT, PROP_NONE);
      RNA_def_property_int_sdna(prop, nullptr, "vector_socket_dimensions");
      RNA_def_property_range(prop, 2, 4);
      RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
      RNA_def_property_ui_text(prop, "Dimensions", "Dimensions of the vector socket");
      RNA_def_property_update(prop, NC_NODE | NA_EDITED, item_update_func_ptr);
    }
  }

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_funcs(
      prop, allocator.copy_string(color_get_func).c_str(), nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Color", "Color of the corresponding socket type in the node editor");
}

void rna_def_node_item_array_common_functions(StructRNA *srna,
                                              const char *item_name,
                                              const char *accessor_name)
{
  static LinearAllocator<> allocator;
  PropertyRNA *parm;
  FunctionRNA *func;

  char remove_call[128];
  SNPRINTF(remove_call, "rna_Node_ItemArray_remove<%s>", accessor_name);
  char clear_call[128];
  SNPRINTF(clear_call, "rna_Node_ItemArray_clear<%s>", accessor_name);
  char move_call[128];
  SNPRINTF(move_call, "rna_Node_ItemArray_move<%s>", accessor_name);

  func = RNA_def_function(srna, "remove", allocator.copy_string(remove_call).c_str());
  RNA_def_function_ui_description(func, "Remove an item");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "item", item_name, "Item", "The item to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "clear", allocator.copy_string(clear_call).c_str());
  RNA_def_function_ui_description(func, "Remove all items");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);

  func = RNA_def_function(srna, "move", allocator.copy_string(move_call).c_str());
  RNA_def_function_ui_description(func, "Move an item to another position");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_int(
      func, "from_index", -1, 0, INT_MAX, "From Index", "Index of the item to move", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(
      func, "to_index", -1, 0, INT_MAX, "To Index", "Target index for the item", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void rna_def_node_item_array_new_with_socket_and_name(StructRNA *srna,
                                                      const char *item_name,
                                                      const char *accessor_name)
{
  static LinearAllocator<> allocator;
  PropertyRNA *parm;
  FunctionRNA *func;

  char name[128];
  SNPRINTF(name, "rna_Node_ItemArray_new_with_socket_and_name<%s>", accessor_name);

  func = RNA_def_function(srna, "new", allocator.copy_string(name).c_str());
  RNA_def_function_ui_description(func, "Add an item at the end");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_enum(func,
                      "socket_type",
                      rna_enum_node_socket_data_type_items,
                      SOCK_GEOMETRY,
                      "Socket Type",
                      "Socket type of the item");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "name", nullptr, MAX_NAME, "Name", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return value */
  parm = RNA_def_pointer(func, "item", item_name, "Item", "New item");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, ParameterFlag(0));
  RNA_def_function_return(func, parm);
}

/* -- Common nodes ---------------------------------------------------------- */

static void def_group_input(BlenderRNA * /*brna*/, StructRNA * /*srna*/) {}

static void def_group_output(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "is_active_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_DO_OUTPUT);
  RNA_def_property_ui_text(
      prop, "Active Output", "True if this node is used as the active group output");
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_GroupOutput_is_active_output_set");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

void def_group(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "NodeTree");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_NodeGroup_node_tree_set", nullptr, "rna_NodeGroup_node_tree_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Node Tree", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeGroup_update");
}

static void def_custom_group(BlenderRNA *brna,
                             const char *struct_name,
                             const char *base_name,
                             const char *ui_name,
                             const char *ui_desc,
                             const char *reg_func)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, struct_name, base_name);
  RNA_def_struct_ui_text(srna, ui_name, ui_desc);
  RNA_def_struct_sdna(srna, "bNode");

  RNA_def_struct_register_funcs(srna, reg_func, "rna_Node_unregister", nullptr);

  def_group(brna, srna);
}

static void def_frame(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "text", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Text");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Text", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeFrame", "storage");
  RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_ID_NODETREE);

  prop = RNA_def_property(srna, "shrink", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_FRAME_SHRINK);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Shrink", "Shrink the frame to minimal bounding box");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "label_size", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "label_size");
  RNA_def_property_range(prop, 8, 64);
  RNA_def_property_int_default(prop, 20);
  RNA_def_property_ui_text(prop, "Label Font Size", "Font size to use for displaying the label");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_FrameNode_label_size_update");
}

void def_math(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_math_items);
  RNA_def_property_ui_text(prop, "Operation", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_NODETREE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_clamp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom2", SHD_MATH_CLAMP);
  RNA_def_property_ui_text(prop, "Clamp", "Clamp result of the node to 0.0 to 1.0 range");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

void def_time(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

void def_rgb_curve(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

void def_colorramp(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "ColorRamp");
  RNA_def_property_ui_text(prop, "Color Ramp", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

void def_mix_rgb(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_ramp_blend_items);
  RNA_def_property_ui_text(prop, "Blending Mode", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom2", SHD_MIXRGB_USE_ALPHA);
  RNA_def_property_ui_text(prop, "Alpha", "Include alpha of second input in this operation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_clamp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom2", SHD_MIXRGB_CLAMP);
  RNA_def_property_ui_text(prop, "Clamp", "Clamp result of the node to 0.0 to 1.0 range");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* -- Compositor Nodes ------------------------------------------------------ */

static void def_cmp_cryptomatte_entry(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CryptomatteEntry", nullptr);
  RNA_def_struct_sdna(srna, "CryptomatteEntry");

  prop = RNA_def_property(srna, "encoded_hash", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_sdna(prop, nullptr, "encoded_hash");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
}

/* -- Geometry Nodes --------------------------------------------------------- */

void def_common_zone_input(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  prop = RNA_def_property(srna, "paired_output", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Node");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, "rna_Node_paired_output_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Paired Output", "Zone output node that this input node is paired with");

  func = RNA_def_function(srna, "pair_with_output", "rna_Node_pair_with_output");
  RNA_def_function_ui_description(func, "Pair a zone input node with an output node.");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS | FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(
      func, "output_node", "NodeInternal", "Output Node", "Zone output node to pair with");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return value */
  parm = RNA_def_boolean(
      func, "result", false, "Result", "True if pairing the node was successful");
  RNA_def_function_return(func, parm);
}

static void def_closure_input(BlenderRNA *brna, StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeClosureInput", "storage");

  def_common_zone_input(brna, srna);
}

static void rna_def_closure_input_item(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeClosureInputItem", nullptr);
  RNA_def_struct_ui_text(srna, "Closure Input Item", "");
  RNA_def_struct_sdna(srna, "NodeClosureInputItem");

  rna_def_node_item_array_socket_item_common(srna, "ClosureInputItemsAccessor", true);

  prop = RNA_def_property(srna, "structure_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_structure_type_items);
  RNA_def_property_enum_funcs(
      prop,
      nullptr,
      nullptr,
      "rna_Node_ItemArray_structure_type_itemf<ClosureInputItemsAccessor>");
  RNA_def_property_ui_text(
      prop,
      "Structure Type",
      "What kind of higher order types are expected to flow through this socket");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<ClosureInputItemsAccessor>");
}

static void rna_def_closure_input_items(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "NodeClosureInputItems", nullptr);
  RNA_def_struct_ui_text(srna, "Closure Input Items", "");
  RNA_def_struct_sdna(srna, "bNode");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeClosureInputItem", "ClosureInputItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "NodeClosureInputItem", "ClosureInputItemsAccessor");
}

static void rna_def_closure_output_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "NodeClosureOutputItem", nullptr);
  RNA_def_struct_ui_text(srna, "Closure Output Item", "");
  RNA_def_struct_sdna(srna, "NodeClosureOutputItem");

  rna_def_node_item_array_socket_item_common(srna, "ClosureOutputItemsAccessor", true);

  prop = RNA_def_property(srna, "structure_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_structure_type_items);
  RNA_def_property_enum_funcs(
      prop,
      nullptr,
      nullptr,
      "rna_Node_ItemArray_structure_type_itemf<ClosureOutputItemsAccessor>");
  RNA_def_property_ui_text(
      prop,
      "Structure Type",
      "What kind of higher order types are expected to flow through this socket");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<ClosureOutputItemsAccessor>");
}

static void rna_def_closure_output_items(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "NodeClosureOutputItems", nullptr);
  RNA_def_struct_ui_text(srna, "Closure Output Items", "");
  RNA_def_struct_sdna(srna, "bNode");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeClosureOutputItem", "ClosureOutputItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "NodeClosureOutputItem", "ClosureOutputItemsAccessor");
}

static void def_closure_output(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_closure_input_item(brna);
  rna_def_closure_input_items(brna);

  rna_def_closure_output_item(brna);
  rna_def_closure_output_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeClosureOutput", "storage");

  prop = RNA_def_property(srna, "input_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "input_items.items", "input_items.items_num");
  RNA_def_property_struct_type(prop, "NodeClosureInputItem");
  RNA_def_property_srna(prop, "NodeClosureInputItems");

  prop = RNA_def_property(srna, "output_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "output_items.items", "output_items.items_num");
  RNA_def_property_struct_type(prop, "NodeClosureOutputItem");
  RNA_def_property_srna(prop, "NodeClosureOutputItems");

  prop = RNA_def_property(srna, "active_input_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "input_items.active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_output_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "output_items.active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "define_signature", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_CLOSURE_FLAG_DEFINE_SIGNATURE);
  RNA_def_property_ui_text(
      prop,
      "Define Signature",
      "This zone defines a closure signature that should be used by other nodes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, "rna_Node_update");
}

static void rna_def_evaluate_closure_input_item(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeEvaluateClosureInputItem", nullptr);
  RNA_def_struct_ui_text(srna, "Input Item", "");

  rna_def_node_item_array_socket_item_common(srna, "EvaluateClosureInputItemsAccessor", true);

  prop = RNA_def_property(srna, "structure_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_structure_type_items);
  RNA_def_property_enum_funcs(
      prop,
      nullptr,
      nullptr,
      "rna_Node_ItemArray_structure_type_itemf<EvaluateClosureInputItemsAccessor>");
  RNA_def_property_ui_text(
      prop,
      "Structure Type",
      "What kind of higher order types are expected to flow through this socket");
  RNA_def_property_update(prop,
                          NC_NODE | NA_EDITED,
                          "rna_Node_ItemArray_item_update<EvaluateClosureInputItemsAccessor>");
}

static void rna_def_evaluate_closure_input_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeEvaluateClosureInputItems", nullptr);
  RNA_def_struct_ui_text(srna, "Input Items", "");
  RNA_def_struct_sdna(srna, "bNode");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeEvaluateClosureInputItem", "EvaluateClosureInputItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "NodeEvaluateClosureInputItem", "EvaluateClosureInputItemsAccessor");
}

static void rna_def_evaluate_closure_output_item(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeEvaluateClosureOutputItem", nullptr);
  RNA_def_struct_ui_text(srna, "Output Item", "");

  rna_def_node_item_array_socket_item_common(srna, "EvaluateClosureOutputItemsAccessor", true);

  prop = RNA_def_property(srna, "structure_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_structure_type_items);
  RNA_def_property_enum_funcs(
      prop,
      nullptr,
      nullptr,
      "rna_Node_ItemArray_structure_type_itemf<EvaluateClosureOutputItemsAccessor>");
  RNA_def_property_ui_text(
      prop,
      "Structure Type",
      "What kind of higher order types are expected to flow through this socket");
  RNA_def_property_update(prop,
                          NC_NODE | NA_EDITED,
                          "rna_Node_ItemArray_item_update<EvaluateClosureOutputItemsAccessor>");
}

static void rna_def_evaluate_closure_output_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeEvaluateClosureOutputItems", nullptr);
  RNA_def_struct_ui_text(srna, "Output Items", "");
  RNA_def_struct_sdna(srna, "bNode");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeEvaluateClosureOutputItem", "EvaluateClosureOutputItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "NodeEvaluateClosureOutputItem", "EvaluateClosureOutputItemsAccessor");
}

static void def_evaluate_closure(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_evaluate_closure_input_item(brna);
  rna_def_evaluate_closure_input_items(brna);

  rna_def_evaluate_closure_output_item(brna);
  rna_def_evaluate_closure_output_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeEvaluateClosure", "storage");

  prop = RNA_def_property(srna, "input_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "input_items.items", "input_items.items_num");
  RNA_def_property_struct_type(prop, "NodeEvaluateClosureInputItem");
  RNA_def_property_ui_text(prop, "Input Items", "");
  RNA_def_property_srna(prop, "NodeEvaluateClosureInputItems");

  prop = RNA_def_property(srna, "output_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "output_items.items", "output_items.items_num");
  RNA_def_property_struct_type(prop, "NodeEvaluateClosureOutputItem");
  RNA_def_property_ui_text(prop, "Output Items", "");
  RNA_def_property_srna(prop, "NodeEvaluateClosureOutputItems");

  prop = RNA_def_property(srna, "active_input_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "input_items.active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_output_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "output_items.active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "define_signature", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "flag", NODE_EVALUATE_CLOSURE_FLAG_DEFINE_SIGNATURE);
  RNA_def_property_ui_text(
      prop,
      "Define Signature",
      "This node defines a closure signature that should be used by other nodes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, "rna_Node_update");
}

static void rna_def_combine_bundle_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "NodeCombineBundleItem", nullptr);
  RNA_def_struct_ui_text(srna, "Combine Bundle Item", "");

  rna_def_node_item_array_socket_item_common(srna, "CombineBundleItemsAccessor", true);

  prop = RNA_def_property(srna, "structure_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_structure_type_items);
  RNA_def_property_enum_funcs(
      prop,
      nullptr,
      nullptr,
      "rna_Node_ItemArray_structure_type_itemf<CombineBundleItemsAccessor>");
  RNA_def_property_ui_text(
      prop,
      "Structure Type",
      "What kind of higher order types are expected to flow through this socket");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<CombineBundleItemsAccessor>");
}

static void rna_def_combine_bundle_items(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "NodeCombineBundleItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of combine bundle items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeCombineBundleItem", "CombineBundleItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "NodeCombineBundleItem", "CombineBundleItemsAccessor");
}

static void def_combine_bundle(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_combine_bundle_item(brna);
  rna_def_combine_bundle_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeCombineBundle", "storage");

  prop = RNA_def_property(srna, "bundle_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_num");
  RNA_def_property_struct_type(prop, "NodeCombineBundleItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeCombineBundleItems");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "define_signature", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_COMBINE_BUNDLE_FLAG_DEFINE_SIGNATURE);
  RNA_def_property_ui_text(
      prop,
      "Define Signature",
      "This node defines a bundle signature that should be used by other nodes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, "rna_Node_update");
}

static void rna_def_separate_bundle_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "NodeSeparateBundleItem", nullptr);
  RNA_def_struct_ui_text(srna, "Separate Bundle Item", "");

  rna_def_node_item_array_socket_item_common(srna, "SeparateBundleItemsAccessor", true);

  prop = RNA_def_property(srna, "structure_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_structure_type_items);
  RNA_def_property_enum_funcs(
      prop,
      nullptr,
      nullptr,
      "rna_Node_ItemArray_structure_type_itemf<SeparateBundleItemsAccessor>");
  RNA_def_property_ui_text(
      prop,
      "Structure Type",
      "What kind of higher order types are expected to flow through this socket");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<SeparateBundleItemsAccessor>");
}

static void rna_def_separate_bundle_items(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "NodeSeparateBundleItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of separate bundle items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeSeparateBundleItem", "SeparateBundleItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "NodeSeparateBundleItem", "SeparateBundleItemsAccessor");
}

static void def_separate_bundle(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_separate_bundle_item(brna);
  rna_def_separate_bundle_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeSeparateBundle", "storage");

  prop = RNA_def_property(srna, "bundle_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_num");
  RNA_def_property_struct_type(prop, "NodeSeparateBundleItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeSeparateBundleItems");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "define_signature", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_SEPARATE_BUNDLE_FLAG_DEFINE_SIGNATURE);
  RNA_def_property_ui_text(
      prop,
      "Define Signature",
      "This node defines a bundle signature that should be used by other nodes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, "rna_Node_update");
}

static void rna_def_shader_node(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "ShaderNode", "NodeInternal");
  RNA_def_struct_ui_text(srna, "Shader Node", "Material shader node");
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_register_funcs(srna, "rna_ShaderNode_register", "rna_Node_unregister", nullptr);
}

static void rna_def_compositor_node(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "CompositorNode", "NodeInternal");
  RNA_def_struct_ui_text(srna, "Compositor Node", "");
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_register_funcs(
      srna, "rna_CompositorNode_register", "rna_Node_unregister", nullptr);

  def_cmp_cryptomatte_entry(brna);
}

static void rna_def_texture_node(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "TextureNode", "NodeInternal");
  RNA_def_struct_ui_text(srna, "Texture Node", "");
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_register_funcs(srna, "rna_TextureNode_register", "rna_Node_unregister", nullptr);
}

static void rna_def_geometry_node(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "GeometryNode", "NodeInternal");
  RNA_def_struct_ui_text(srna, "Geometry Node", "");
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_register_funcs(srna, "rna_GeometryNode_register", "rna_Node_unregister", nullptr);
}

static void rna_def_function_node(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "FunctionNode", "NodeInternal");
  RNA_def_struct_ui_text(srna, "Function Node", "");
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_register_funcs(srna, "rna_FunctionNode_register", "rna_Node_unregister", nullptr);
}

/* -------------------------------------------------------------------------- */

static void def_reroute(BlenderRNA * /*brna*/, StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeReroute", "storage");

  PropertyRNA *prop = RNA_def_property(srna, "socket_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type_idname");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_reroute_node_socket_type_set");
  RNA_def_property_ui_text(prop, "Type of socket", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void def_implicit_conversion(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeImplicitConversion", "storage");

  prop = RNA_def_property(srna, "socket_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type_idname");
  RNA_def_property_string_funcs(
      prop, nullptr, nullptr, "rna_implicit_conversion_node_socket_type_set");
  RNA_def_property_ui_text(prop, "Type of socket", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_data_type_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_NodeImplicitConversion_data_type_get",
                              "rna_NodeImplicitConversion_data_type_set",
                              "rna_NodeImplicitConversion_data_type_itemf");
  RNA_def_property_enum_default(prop, SOCK_RGBA);
  RNA_def_property_ui_text(prop, "Data Type", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void def_comment(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeComment", "storage");

  prop = RNA_def_property(srna, "text", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Text", "Text to show in the node");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);
}

static void rna_def_internal_node(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop, *parm;
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "NodeInternalSocketTemplate", nullptr);
  RNA_def_struct_ui_text(srna, "Socket Template", "Type and default value of a node socket");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeInternalSocketTemplate_name_get",
                                "rna_NodeInternalSocketTemplate_name_length",
                                nullptr);
  RNA_def_property_ui_text(prop, "Name", "Name of the socket");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeInternalSocketTemplate_identifier_get",
                                "rna_NodeInternalSocketTemplate_identifier_length",
                                nullptr);
  RNA_def_property_ui_text(prop, "Identifier", "Identifier of the socket");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop, "rna_NodeInternalSocketTemplate_type_get", nullptr, nullptr);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_type_items);
  RNA_def_property_ui_text(prop, "Type", "Data type of the socket");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* XXX Workaround: Registered functions are not exposed in python by bpy,
   * it expects them to be registered from python and use the native implementation.
   *
   * However, the standard node types are not registering these functions from python,
   * so in order to call them in py scripts we need to overload and
   * replace them with plain C callbacks.
   * This type provides a usable basis for node types defined in C.
   */

  srna = RNA_def_struct(brna, "NodeInternal", "Node");
  RNA_def_struct_sdna(srna, "bNode");

  /* poll */
  func = RNA_def_function(srna, "poll", "rna_NodeInternal_poll");
  RNA_def_function_ui_description(
      func, "If non-null output is returned, the node type can be added to the tree");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
  parm = RNA_def_pointer(func, "node_tree", "NodeTree", "Node Tree", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "poll_instance", "rna_NodeInternal_poll_instance");
  RNA_def_function_ui_description(
      func, "If non-null output is returned, the node can be added to the tree");
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
  parm = RNA_def_pointer(func, "node_tree", "NodeTree", "Node Tree", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* update */
  func = RNA_def_function(srna, "update", "rna_NodeInternal_update");
  RNA_def_function_ui_description(
      func, "Update on node graph topology changes (adding or removing nodes and links)");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_ALLOW_WRITE);

  /* draw buttons */
  func = RNA_def_function(srna, "draw_buttons", "rna_NodeInternal_draw_buttons");
  RNA_def_function_ui_description(func, "Draw node buttons");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "UILayout");
  RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* draw buttons extended */
  func = RNA_def_function(srna, "draw_buttons_ext", "rna_NodeInternal_draw_buttons_ext");
  RNA_def_function_ui_description(func, "Draw node buttons in the sidebar");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "UILayout");
  RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

static void rna_def_node_sockets_api(BlenderRNA *brna, PropertyRNA *cprop, int in_out)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;
  const char *structtype = (in_out == SOCK_IN ? "NodeInputs" : "NodeOutputs");
  const char *uiname = (in_out == SOCK_IN ? "Node Inputs" : "Node Outputs");
  const char *newfunc = (in_out == SOCK_IN ? "rna_Node_inputs_new" : "rna_Node_outputs_new");
  const char *clearfunc = (in_out == SOCK_IN ? "rna_Node_inputs_clear" : "rna_Node_outputs_clear");
  const char *movefunc = (in_out == SOCK_IN ? "rna_Node_inputs_move" : "rna_Node_outputs_move");

  RNA_def_property_srna(cprop, structtype);
  srna = RNA_def_struct(brna, structtype, nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, uiname, "Collection of Node Sockets");

  func = RNA_def_function(srna, "new", newfunc);
  RNA_def_function_ui_description(func, "Add a socket to this node");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "type", nullptr, MAX_NAME, "Type", "Data type");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "name", nullptr, MAX_NAME, "Name", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_string(func, "identifier", nullptr, MAX_NAME, "Identifier", "Unique socket identifier");
  RNA_def_boolean(
      func, "use_multi_input", false, "", "Make the socket multi-input (valid for inputs only)");
  /* return value */
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "New socket");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, ParameterFlag(0));
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Node_socket_remove");
  RNA_def_function_ui_description(func, "Remove a socket from this node");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "The socket to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "clear", clearfunc);
  RNA_def_function_ui_description(func, "Remove all sockets from this node");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);

  func = RNA_def_function(srna, "move", movefunc);
  RNA_def_function_ui_description(func, "Move a socket to another position");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "from_index", -1, 0, INT_MAX, "From Index", "Index of the socket to move", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(
      func, "to_index", -1, 0, INT_MAX, "To Index", "Target index for the socket", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_node_panel_state(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodePanelState", nullptr);
  RNA_def_struct_sdna(srna, "bNodePanelState");
  RNA_def_struct_ui_text(srna, "Node Panel State", "Expansion state of a panel in a node");

  prop = RNA_def_property(srna, "identifier", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "identifier");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Identifier", "Unique identifier matching the panel's persistent_uid");

  prop = RNA_def_property(srna, "is_collapsed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_PANEL_COLLAPSED);
  RNA_def_property_ui_text(prop, "Is Collapsed", "Whether the panel is collapsed");
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);
}

static void rna_def_node(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem warning_propagation_items[] = {
      {NODE_WARNING_PROPAGATION_ALL,
       "ALL",
       0,
       "All Messages",
       "Propagate every info, error, and warning message upstream"},
      {NODE_WARNING_PROPAGATION_ONLY_ERRORS_AND_WARNINGS,
       "ERRORS_AND_WARNINGS",
       0,
       "Errors and Warnings",
       "Propagate only error and warning messages upstream"},
      {NODE_WARNING_PROPAGATION_ONLY_ERRORS,
       "ERRORS",
       0,
       "Errors",
       "Propagate only error messages upstream"},
      {NODE_WARNING_PROPAGATION_NONE, "NONE", 0, "None", "Do not propagate any messages upstream"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Node", nullptr);
  RNA_def_struct_ui_text(srna, "Node", "Node in a node tree");
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_icon(srna, ICON_NODE);
  RNA_def_struct_refine_func(srna, "rna_Node_refine");
  RNA_def_struct_path_func(srna, "rna_Node_path");
  RNA_def_struct_register_funcs(srna, "rna_Node_register", "rna_Node_unregister", nullptr);
  RNA_def_struct_idprops_func(srna, "rna_Node_idprops");
  RNA_def_struct_system_idprops_func(srna, "rna_Node_system_idprops");

  prop = RNA_def_property(srna, "type", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, "rna_node_type_get", "rna_node_type_length", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Type", "Legacy unique node type identifier, redundant with bl_idname property");

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_funcs(prop, "rna_Node_location_get", "rna_Node_location_set", nullptr);
  RNA_def_property_range(prop, -1000000.0f, 1000000.0f);
  RNA_def_property_ui_text(prop, "Location", "Location of the node within its parent frame");
  RNA_def_property_update(prop, NC_NODE, "rna_Node_update");

  prop = RNA_def_property(srna, "location_absolute", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "location");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, -1000000.0f, 1000000.0f);
  RNA_def_property_ui_text(prop, "Absolute Location", "Location of the node in the entire canvas");
  RNA_def_property_update(prop, NC_NODE, "rna_Node_update");

  prop = RNA_def_property(srna, "width", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "width");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_Node_width_range");
  RNA_def_property_ui_text(prop, "Width", "Width of the node");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "height");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_Node_height_range");
  RNA_def_property_ui_text(
      prop,
      "Height",
      "Height of the node. This property holds true data only under certain circumstances, "
      "e.g. for a Frame node after the node graph was displayed. For most types of nodes, "
      "the displayed height is based on the node's contents and not reflected in this property.");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "dimensions", PROP_FLOAT, PROP_XYZ_LENGTH);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_funcs(prop, "rna_Node_dimensions_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Dimensions", "Absolute bounding box dimensions of the node after it was displayed");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Unique node identifier");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_Node_name_set");
  RNA_def_property_editable_func(prop, "rna_ID_owner_editable_if_in_bmain");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "label");
  RNA_def_property_ui_text(prop, "Label", "Optional custom node label");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "inputs", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "inputs", nullptr);
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_NodeInputs_lookup_string",
                                    nullptr);
  RNA_def_property_struct_type(prop, "NodeSocket");
  RNA_def_property_flag_hide_from_ui_workaround(prop);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Inputs", "");
  rna_def_node_sockets_api(brna, prop, SOCK_IN);

  prop = RNA_def_property(srna, "outputs", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "outputs", nullptr);
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_NodeOutputs_lookup_string",
                                    nullptr);
  RNA_def_property_struct_type(prop, "NodeSocket");
  RNA_def_property_flag_hide_from_ui_workaround(prop);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Outputs", "");
  rna_def_node_sockets_api(brna, prop, SOCK_OUT);

  prop = RNA_def_property(srna, "panel_states", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "panel_states_array", "num_panel_states");
  RNA_def_property_struct_type(prop, "NodePanelState");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Panel States", "Expansion state of each panel in the node");

  prop = RNA_def_property(srna, "internal_links", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Node_internal_links_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "NodeInternalLink");
  RNA_def_property_ui_text(
      prop, "Internal Links", "Internal input-to-output connections for muting");

  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "parent");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_Node_parent_set", nullptr, "rna_Node_parent_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_struct_type(prop, "Node");
  RNA_def_property_ui_text(prop, "Parent", "Parent this node is attached to, e.g. a Frame node");

  prop = RNA_def_property(srna, "warning_propagation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, warning_propagation_items);
  RNA_def_property_ui_text(
      prop,
      "Warning Propagation",
      "The kinds of messages that should be propagated from this node to the parent group node");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "use_custom_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_CUSTOM_COLOR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Custom Color", "Use custom color for the node");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Color", "Custom color of the node body");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "color_tag", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_color_tag_items);
  RNA_def_property_enum_funcs(prop, "rna_Node_color_tag_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Color Tag", "Node header color tag");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SELECT);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Node_select_set");
  RNA_def_property_ui_text(prop, "Select", "Node selection state");
  RNA_def_property_update(prop, NC_NODE | NA_SELECTED, nullptr);

  prop = RNA_def_property(srna, "show_options", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_OPTIONS);
  RNA_def_property_ui_text(
      prop,
      "Show Options",
      "Whether the node options are visible, e.g. the selected data-block of a node group node");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "show_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_PREVIEW);
  RNA_def_property_ui_text(prop, "Show Preview", "");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_COLLAPSED);
  RNA_def_property_ui_text(prop, "Collapse", "Node collapsed state");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_MUTED);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Node_mute_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Mute", "");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "show_texture", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_ACTIVE_TEXTURE);
  RNA_def_property_ui_text(prop, "Show Texture", "Display node in viewport textured shading mode");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  /* generic property update function */
  func = RNA_def_function(srna, "socket_value_update", "rna_Node_socket_value_update");
  RNA_def_function_ui_description(func, "Update after property changes");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "is_registered_node_type", "rna_Node_is_registered_node_type");
  RNA_def_function_ui_description(func, "True if a registered node type");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
  parm = RNA_def_boolean(func, "result", false, "Result", "");
  RNA_def_function_return(func, parm);

  /* registration */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_Node_bl_idname_get", "rna_Node_bl_idname_length", "rna_Node_bl_idname_set");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "ID Name", "");

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_Node_bl_label_get", "rna_Node_bl_label_length", "rna_Node_bl_label_set");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Label", "The node label");

  prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_TRANSLATION);
  RNA_def_property_string_funcs(prop,
                                "rna_Node_bl_description_get",
                                "rna_Node_bl_description_length",
                                "rna_Node_bl_description_set");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "typeinfo->ui_icon");
  RNA_def_property_enum_items(prop, rna_enum_icon_items);
  RNA_def_property_enum_default(prop, ICON_NODE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Icon", "The node icon");

  prop = RNA_def_property(srna, "bl_static_type", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, "rna_node_type_get", "rna_node_type_length", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Static Type",
      "Legacy unique node type identifier, redundant with bl_idname property");

  /* type-based size properties */
  prop = RNA_def_property(srna, "bl_width_default", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->default_width");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Default Width", "Default width of the node when it is created");

  prop = RNA_def_property(srna, "bl_width_min", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->minwidth");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop, "Minimum Width", "When changing the node's size, it has at least this width");

  prop = RNA_def_property(srna, "bl_width_max", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->maxwidth");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop, "Maximum Width", "When changing the node's size, it can have at most this width");

  prop = RNA_def_property(srna, "bl_height_default", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->height");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Default Height",
      "Default height of the node when it is created (mostly unused, see Height)");

  prop = RNA_def_property(srna, "bl_height_min", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->minheight");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Minimum Height",
      "When changing the node's size, it has at least this height (mostly unused, see Height)");

  prop = RNA_def_property(srna, "bl_height_max", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->maxheight");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop,
                           "Maximum Height",
                           "When changing the node's size, it can have at most this height "
                           "(mostly unused, see Height)");

  /* poll */
  func = RNA_def_function(srna, "poll", nullptr);
  RNA_def_function_ui_description(
      func, "If non-null output is returned, the node type can be added to the tree");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER);
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
  parm = RNA_def_pointer(func, "node_tree", "NodeTree", "Node Tree", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "poll_instance", nullptr);
  RNA_def_function_ui_description(
      func, "If non-null output is returned, the node can be added to the tree");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
  parm = RNA_def_pointer(func, "node_tree", "NodeTree", "Node Tree", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* update */
  func = RNA_def_function(srna, "update", nullptr);
  RNA_def_function_ui_description(
      func, "Update on node graph topology changes (adding or removing nodes and links)");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);

  /* insert_link */
  func = RNA_def_function(srna, "insert_link", nullptr);
  RNA_def_function_ui_description(func, "Handle creation of a link to or from the node");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "link", "NodeLink", "Link", "Node link that will be inserted");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* init */
  func = RNA_def_function(srna, "init", nullptr);
  RNA_def_function_ui_description(func, "Initialize a new instance of this node");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* copy */
  func = RNA_def_function(srna, "copy", nullptr);
  RNA_def_function_ui_description(func,
                                  "Initialize a new instance of this node from an existing node");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Existing node to copy");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* free */
  func = RNA_def_function(srna, "free", nullptr);
  RNA_def_function_ui_description(func, "Clean up node on removal");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);

  /* draw buttons */
  func = RNA_def_function(srna, "draw_buttons", nullptr);
  RNA_def_function_ui_description(func, "Draw node buttons");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "UILayout");
  RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* draw buttons extended */
  func = RNA_def_function(srna, "draw_buttons_ext", nullptr);
  RNA_def_function_ui_description(func, "Draw node buttons in the sidebar");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "UILayout");
  RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* dynamic label */
  func = RNA_def_function(srna, "draw_label", nullptr);
  RNA_def_function_ui_description(func, "Returns a dynamic label string");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_string(func, "label", nullptr, MAX_NAME, "Label", "");
  RNA_def_parameter_flags(
      parm, PROP_THICK_WRAP, ParameterFlag(0)); /* needed for string return value */
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna,
                          "debug_zone_body_lazy_function_graph",
                          "rna_NodeTree_debug_zone_body_lazy_function_graph");
  RNA_def_function_ui_description(
      func, "Get the internal lazy-function graph for the body of this zone");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT);
  parm = RNA_def_string(func, "dot_graph", nullptr, INT32_MAX, "Dot Graph", "Graph in dot format");
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, ParameterFlag(0));
  RNA_def_parameter_clear_flags(parm, PROP_NEVER_NULL, ParameterFlag(0));
  RNA_def_function_output(func, parm);

  func = RNA_def_function(
      srna, "debug_zone_lazy_function_graph", "rna_NodeTree_debug_zone_lazy_function_graph");
  RNA_def_function_ui_description(func, "Get the internal lazy-function graph for this zone");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT);
  parm = RNA_def_string(func, "dot_graph", nullptr, INT32_MAX, "Dot Graph", "Graph in dot format");
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, ParameterFlag(0));
  RNA_def_parameter_clear_flags(parm, PROP_NEVER_NULL, ParameterFlag(0));
  RNA_def_function_output(func, parm);
}

static void rna_def_node_internal_link(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeInternalLink", nullptr);
  RNA_def_struct_ui_text(srna,
                         "Node Internal Link",
                         "Internal link used by muted nodes to connect input to output sockets");

  prop = RNA_def_property(srna, "from_socket", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "NodeSocket");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_pointer_funcs(
      prop, "rna_NodeInternalLink_from_socket_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "From Socket", "");

  prop = RNA_def_property(srna, "to_socket", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "NodeSocket");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_pointer_funcs(
      prop, "rna_NodeInternalLink_to_socket_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "To Socket", "");
}

static void rna_def_node_link(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "NodeLink", nullptr);
  RNA_def_struct_ui_text(srna, "NodeLink", "Link between nodes in a node tree");
  RNA_def_struct_sdna(srna, "bNodeLink");
  RNA_def_struct_ui_icon(srna, ICON_NODE);

  prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_LINK_VALID);
  RNA_def_property_ui_text(prop, "Valid", "Link is valid");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "is_muted", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_LINK_MUTED);
  RNA_def_property_ui_text(prop, "Muted", "Link is muted and can be ignored");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "from_node", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "fromnode");
  RNA_def_property_struct_type(prop, "Node");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(prop, "From node", "");

  prop = RNA_def_property(srna, "to_node", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "tonode");
  RNA_def_property_struct_type(prop, "Node");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(prop, "To node", "");

  prop = RNA_def_property(srna, "from_socket", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "fromsock");
  RNA_def_property_struct_type(prop, "NodeSocket");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(prop, "From socket", "");

  prop = RNA_def_property(srna, "to_socket", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "tosock");
  RNA_def_property_struct_type(prop, "NodeSocket");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(prop, "To socket", "");

  prop = RNA_def_property(srna, "is_hidden", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_NodeLink_is_hidden_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(prop, "Is Hidden", "Link is hidden due to invisible sockets");

  prop = RNA_def_property(srna, "multi_input_sort_id", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Multi Input Sort ID",
      "Used to sort multiple links coming into the same input. The highest ID is at the top.");

  func = RNA_def_function(
      srna, "swap_multi_input_sort_id", "rna_NodeLink_swap_multi_input_sort_id");
  RNA_def_function_ui_description(
      func, "Swap the order of two links connected to the same multi-input socket");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func,
                         "other",
                         "NodeLink",
                         "Other",
                         "The other link. Must link to the same multi-input socket.");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

static void rna_def_nodetree_nodes_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm, *prop;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "Nodes");
  srna = RNA_def_struct(brna, "Nodes", nullptr);
  RNA_def_struct_sdna(srna, "bNodeTree");
  RNA_def_struct_ui_text(srna, "Nodes", "Collection of Nodes");

  func = RNA_def_function(srna, "new", "rna_NodeTree_node_new");
  RNA_def_function_ui_description(func, "Add a node to this node tree");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  /* XXX warning note should eventually be removed,
   * added this here to avoid frequent confusion with API changes from "type" to "bl_idname"
   */
  parm = RNA_def_string(
      func,
      "type",
      nullptr,
      MAX_NAME,
      "Type",
      "Type of node to add (Warning: should be same as node.bl_idname, not node.type!)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return value */
  parm = RNA_def_pointer(func, "node", "Node", "", "New node");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, ParameterFlag(0));
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_NodeTree_node_remove");
  RNA_def_function_ui_description(func, "Remove a node from this node tree");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "node", "Node", "", "The node to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "clear", "rna_NodeTree_node_clear");
  RNA_def_function_ui_description(func, "Remove all nodes from this node tree");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Node");
  RNA_def_property_pointer_funcs(
      prop, "rna_NodeTree_active_node_get", "rna_NodeTree_active_node_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Node", "Active node in this tree");
  RNA_def_property_update(prop, NC_SCENE | ND_OB_ACTIVE, "rna_NodeTree_update");
}

static void rna_def_nodetree_link_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "NodeLinks");
  srna = RNA_def_struct(brna, "NodeLinks", nullptr);
  RNA_def_struct_sdna(srna, "bNodeTree");
  RNA_def_struct_ui_text(srna, "Node Links", "Collection of Node Links");

  func = RNA_def_function(srna, "new", "rna_NodeTree_link_new");
  RNA_def_function_ui_description(func, "Add a node link to this node tree");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "input", "NodeSocket", "", "The input socket");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "output", "NodeSocket", "", "The output socket");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_boolean(func,
                  "verify_limits",
                  true,
                  "Verify Limits",
                  "Remove existing links if connection limit is exceeded");
  RNA_def_boolean(func,
                  "handle_dynamic_sockets",
                  false,
                  "Handle Dynamic Sockets",
                  "Handle node specific features like virtual sockets");
  /* return */
  parm = RNA_def_pointer(func, "link", "NodeLink", "", "New node link");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_NodeTree_link_remove");
  RNA_def_function_ui_description(func, "remove a node link from the node tree");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "link", "NodeLink", "", "The node link to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "clear", "rna_NodeTree_link_clear");
  RNA_def_function_ui_description(func, "remove all node links from the node tree");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
}

static void rna_def_nodetree(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem static_type_items[] = {
      {NTREE_UNDEFINED,
       "UNDEFINED",
       ICON_QUESTION,
       "Undefined",
       "Undefined type of nodes (can happen e.g. when a linked node tree goes missing)"},
      {NTREE_CUSTOM, "CUSTOM", ICON_NONE, "Custom", "Custom nodes"},
      {NTREE_SHADER, "SHADER", ICON_MATERIAL, "Shader", "Shader nodes"},
      {NTREE_TEXTURE, "TEXTURE", ICON_TEXTURE, "Texture", "Texture nodes"},
      {NTREE_COMPOSIT, "COMPOSITING", ICON_RENDERLAYERS, "Compositing", "Compositing nodes"},
      {NTREE_GEOMETRY, "GEOMETRY", ICON_GEOMETRY_NODES, "Geometry", "Geometry nodes"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "NodeTree", "ID");
  RNA_def_struct_ui_text(
      srna,
      "Node Tree",
      "Node tree consisting of linked nodes used for shading, textures and compositing");
  RNA_def_struct_sdna(srna, "bNodeTree");
  RNA_def_struct_ui_icon(srna, ICON_NODETREE);
  RNA_def_struct_refine_func(srna, "rna_NodeTree_refine");
  RNA_def_struct_register_funcs(srna, "rna_NodeTree_register", "rna_NodeTree_unregister", nullptr);

  prop = RNA_def_property(srna, "color_tag", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_color_tag_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_NodeTree_color_tag_itemf");
  RNA_def_property_ui_text(
      prop, "Color Tag", "Color tag of the node group which influences the header color");
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "default_group_node_width", PROP_INT, PROP_NONE);
  RNA_def_property_int_default(prop, bke::NodeWidth::Default);
  RNA_def_property_range(prop, bke::NodeWidth::GroupMin, bke::NodeWidth::DefaultMax);
  RNA_def_property_ui_text(
      prop, "Default Group Node Width", "The width for newly created group nodes");
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "view_center", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_sdna(prop, nullptr, "view_center");
  RNA_def_property_ui_text(
      prop, "View Center", "The current location (offset) of the view for this Node Tree");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Description", "Description of the node tree");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  /* AnimData */
  rna_def_animdata_common(srna);

  /* Nodes Collection */
  prop = RNA_def_property(srna, "nodes", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "nodes", nullptr);
  RNA_def_property_struct_type(prop, "Node");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Nodes", "");
  rna_def_nodetree_nodes_api(brna, prop);

  /* NodeLinks Collection */
  prop = RNA_def_property(srna, "links", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "links", nullptr);
  RNA_def_property_struct_type(prop, "NodeLink");
  RNA_def_property_ui_text(prop, "Links", "");
  rna_def_nodetree_link_api(brna, prop);

  /* Annotation */
  prop = RNA_def_property(srna, "annotation", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gpd");
  RNA_def_property_struct_type(prop, "Annotation");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_GPencil_datablocks_annotations_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Annotation", "Annotation data");
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, static_type_items);
  RNA_def_property_ui_text(
      prop,
      "Type",
      "Node Tree type (deprecated, bl_idname is the actual node tree type identifier)");

  prop = RNA_def_property(srna, "interface", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "tree_interface");
  RNA_def_property_struct_type(prop, "NodeTreeInterface");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Interface", "Interface declaration for this node tree");
  /* exposed as a function for runtime interface type properties */
  func = RNA_def_function(srna, "interface_update", "rna_NodeTree_interface_update");
  RNA_def_function_ui_description(func, "Updated node group interface");
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "contains_tree", "rna_NodeTree_contains_tree");
  RNA_def_function_ui_description(
      func,
      "Check if the node tree contains another. Used to avoid creating recursive node groups.");
  parm = RNA_def_pointer(
      func, "sub_tree", "NodeTree", "Node Tree", "Node tree for recursive check");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "contained", PROP_BOOLEAN, PROP_NONE);
  RNA_def_function_return(func, parm);

  /* registration */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeTree_bl_idname_get",
                                "rna_NodeTree_bl_idname_length",
                                "rna_NodeTree_bl_idname_set");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "ID Name", "");

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeTree_bl_label_get",
                                "rna_NodeTree_bl_label_length",
                                "rna_NodeTree_bl_label_set");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Label", "The node tree label");

  prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_TRANSLATION);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeTree_bl_description_get",
                                "rna_NodeTree_bl_description_length",
                                "rna_NodeTree_bl_description_set");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "typeinfo->ui_icon");
  RNA_def_property_enum_items(prop, rna_enum_icon_items);
  RNA_def_property_enum_default(prop, ICON_NODETREE);
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Icon", "The node tree icon");

  prop = RNA_def_property(srna, "bl_use_group_interface", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "typeinfo->no_group_interface", 1);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop,
                           "Use Group Interface",
                           "Determines the visibility of some UI elements related to node groups");

  /* poll */
  func = RNA_def_function(srna, "poll", nullptr);
  RNA_def_function_ui_description(func, "Check visibility in the editor");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));

  /* update */
  func = RNA_def_function(srna, "update", nullptr);
  RNA_def_function_ui_description(func, "Update on editor changes");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);

  /* get a node tree from context */
  func = RNA_def_function(srna, "get_from_context", nullptr);
  RNA_def_function_ui_description(func, "Get a node tree from the context");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "result_1", "NodeTree", "Node Tree", "Active node tree from context");
  RNA_def_function_output(func, parm);
  parm = RNA_def_pointer(
      func, "result_2", "ID", "Owner ID", "ID data-block that owns the node tree");
  RNA_def_function_output(func, parm);
  parm = RNA_def_pointer(
      func, "result_3", "ID", "From ID", "Original ID data-block selected from the context");
  RNA_def_function_output(func, parm);

  /* Check for support of a socket type with a type identifier. */
  func = RNA_def_function(srna, "valid_socket_type", nullptr);
  RNA_def_function_ui_description(func, "Check if the socket type is valid for the node tree");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_string(
      func, "idname", "NodeSocket", MAX_NAME, "Socket Type", "Identifier of the socket type");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL | PROP_THICK_WRAP, PARM_REQUIRED);
  RNA_def_function_return(func, RNA_def_boolean(func, "valid", false, "", ""));

  func = RNA_def_function(
      srna, "debug_lazy_function_graph", "rna_NodeTree_debug_lazy_function_graph");
  RNA_def_function_ui_description(func, "Get the internal lazy-function graph for this node tree");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_string(func, "dot_graph", nullptr, INT32_MAX, "Dot Graph", "Graph in dot format");
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, ParameterFlag(0));
  RNA_def_parameter_clear_flags(parm, PROP_NEVER_NULL, ParameterFlag(0));
  RNA_def_function_output(func, parm);
}

static void rna_def_composite_nodetree(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CompositorNodeTree", "NodeTree");
  RNA_def_struct_ui_text(
      srna, "Compositor Node Tree", "Node tree consisting of linked nodes used for compositing");
  RNA_def_struct_sdna(srna, "bNodeTree");
  RNA_def_struct_ui_icon(srna, ICON_RENDERLAYERS);

  prop = RNA_def_property(srna, "use_viewer_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NTREE_VIEWER_BORDER);
  RNA_def_property_ui_text(prop,
                           "Viewer Region",
                           "Unused but kept for compatibility reasons. Use boundaries for viewer "
                           "nodes and composite backdrop");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update");

  prop = RNA_def_property(srna, "is_strip_modifier", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Strip Modifier", "The node group is used as a sequencer strip modifier");
  RNA_def_property_boolean_funcs(prop,
                                 "rna_CompositorNodeTree_is_strip_modifier_get",
                                 "rna_CompositorNodeTree_is_strip_modifier_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "allow_usage_in_scene_compositor_effect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Scene Effect", "The node group can be used as a scene compositor effect");
  RNA_def_property_boolean_funcs(
      prop,
      "rna_CompositorNodeTree_allow_usage_in_scene_compositor_effect_get",
      "rna_CompositorNodeTree_allow_usage_in_scene_compositor_effect_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");
}

static void rna_def_shader_nodetree(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "ShaderNodeTree", "NodeTree");
  RNA_def_struct_ui_text(
      srna,
      "Shader Node Tree",
      "Node tree consisting of linked nodes used for materials (and other shading data-blocks)");
  RNA_def_struct_sdna(srna, "bNodeTree");
  RNA_def_struct_ui_icon(srna, ICON_MATERIAL);

  func = RNA_def_function(srna, "get_output_node", "ntreeShaderOutputNode");
  RNA_def_function_ui_description(func,
                                  "Return active shader output node for the specified target");
  parm = RNA_def_enum(
      func, "target", prop_shader_output_target_items, SHD_OUTPUT_ALL, "Target", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "node", "ShaderNode", "Node", "");
  RNA_def_function_return(func, parm);
}

static void rna_def_texture_nodetree(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "TextureNodeTree", "NodeTree");
  RNA_def_struct_ui_text(
      srna, "Texture Node Tree", "Node tree consisting of linked nodes used for textures");
  RNA_def_struct_sdna(srna, "bNodeTree");
  RNA_def_struct_ui_icon(srna, ICON_TEXTURE);
}

static void rna_def_geometry_nodetree(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GeometryNodeTree", "NodeTree");
  RNA_def_struct_ui_text(
      srna, "Geometry Node Tree", "Node tree consisting of linked nodes used for geometries");
  RNA_def_struct_sdna(srna, "bNodeTree");
  RNA_def_struct_ui_icon(srna, ICON_NODETREE);

  prop = RNA_def_property(srna, "is_tool", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Tool", "The node group is used as a tool");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_tool_get", "rna_GeometryNodeTree_is_tool_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "node_tool_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Node Tool Identifier", "Unique operator identifier for the node tool");
  RNA_def_property_string_funcs(prop,
                                "rna_GeometryNodeTree_node_tool_idname_get",
                                "rna_GeometryNodeTree_node_tool_idname_length",
                                "rna_GeometryNodeTree_node_tool_idname_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_modifier", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Modifier", "The node group is used as a geometry modifier");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_modifier_get", "rna_GeometryNodeTree_is_modifier_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_mode_object", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Edit", "The node group is used in object mode");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_mode_object_get", "rna_GeometryNodeTree_is_mode_object_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_mode_edit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Edit", "The node group is used in edit mode");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_mode_edit_get", "rna_GeometryNodeTree_is_mode_edit_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_mode_sculpt", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Sculpt", "The node group is used in sculpt mode");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_mode_sculpt_get", "rna_GeometryNodeTree_is_mode_sculpt_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_mode_paint", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Paint", "The node group is used in paint mode");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_mode_paint_get", "rna_GeometryNodeTree_is_mode_paint_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_type_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Mesh", "The node group is used for meshes");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_type_mesh_get", "rna_GeometryNodeTree_is_type_mesh_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_type_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Curves", "The node group is used for curves");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_type_curve_get", "rna_GeometryNodeTree_is_type_curve_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_type_pointcloud", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Point Cloud", "The node group is used for point clouds");
  RNA_def_property_boolean_funcs(prop,
                                 "rna_GeometryNodeTree_is_type_pointcloud_get",
                                 "rna_GeometryNodeTree_is_type_pointcloud_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "use_wait_for_click", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Wait for Click",
                           "Wait for mouse click input before running the operator from a menu");
  RNA_def_property_boolean_funcs(prop,
                                 "rna_GeometryNodeTree_use_wait_for_click_get",
                                 "rna_GeometryNodeTree_use_wait_for_click_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_type_grease_pencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Grease Pencil", "The node group is used for Grease Pencil");
  RNA_def_property_boolean_funcs(prop,
                                 "rna_GeometryNodeTree_is_type_grease_pencil_get",
                                 "rna_GeometryNodeTree_is_type_grease_pencil_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "show_modifier_manage_panel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_GeometryNodeTree_modifier_manage_panel_get",
                                 "rna_GeometryNodeTree_modifier_manage_panel_set");
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", 0);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Show Manage Panel",
      "Turn on the option to display the manage panel when creating a modifier");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update");
}

StructRNA *define_specific_node(BlenderRNA *brna, const char *struct_name, const char *base_name)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, struct_name, base_name);
  RNA_def_struct_sdna(srna, "bNode");

  func = RNA_def_function(srna, "is_registered_node_type", "rna_Node_is_registered_node_type");
  RNA_def_function_ui_description(func, "True if a registered node type");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
  parm = RNA_def_boolean(func, "result", false, "Result", "");
  RNA_def_function_return(func, parm);

  /* Exposes the socket template type lists in RNA for use in scripts
   * Only used in the C nodes and not exposed in the base class to
   * keep the namespace clean for py-nodes. */
  func = RNA_def_function(srna, "input_template", "rna_NodeInternal_input_template");
  RNA_def_function_ui_description(func, "Input socket template");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
  parm = RNA_def_property(func, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(parm, "Index", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "result", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "NodeInternalSocketTemplate");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "output_template", "rna_NodeInternal_output_template");
  RNA_def_function_ui_description(func, "Output socket template");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
  parm = RNA_def_property(func, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(parm, "Index", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "result", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "NodeInternalSocketTemplate");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  return srna;
}

static void rna_def_nodes(BlenderRNA *brna)
{
  const auto define = [&](const char *base_name,
                          const char *struct_name,
                          void (*func)(BlenderRNA *, StructRNA *) = nullptr) {
    StructRNA *srna = define_specific_node(brna, struct_name, base_name);
    if (func) {
      func(brna, srna);
    }
  };

  /* Disabling clang-format because:
   * - It's more readable when the lines are aligned.
   * - It's easier to sort the lines alphabetically with automated tools. Keeping the lines sorted
   *   avoids merge conflicts in many cases when multiple people add nodes at the same time.
   */
  /* clang-format off */

  define("NodeInternal", "NodeFrame", def_frame);
  define("NodeInternal", "NodeGroup", def_group);
  define("NodeInternal", "NodeGroupInput", def_group_input);
  define("NodeInternal", "NodeGroupOutput", def_group_output);
  define("NodeInternal", "NodeReroute", def_reroute);
  define("NodeInternal", "NodeImplicitConversion", def_implicit_conversion);
  define ("NodeInternal", "NodeComment", def_comment);

  define("NodeInternal", "NodeClosureInput", def_closure_input);
  define("NodeInternal", "NodeClosureOutput", def_closure_output);
  define("NodeInternal", "NodeCombineBundle", def_combine_bundle);
  define("NodeInternal", "NodeEnableOutput");
  define("NodeInternal", "NodeEvaluateClosure", def_evaluate_closure);
  define("NodeInternal", "NodeGetBundleItem");
  define("NodeInternal", "NodeGetNestedBundlePaths");
  define("NodeInternal", "NodeJoinBundle");
  define("NodeInternal", "NodeSeparateBundle", def_separate_bundle);
  define("NodeInternal", "NodeStoreBundleItem");

  /* Node group types are currently defined for each tree type individually. */

  /* clang-format on */
}

void RNA_def_nodetree(BlenderRNA *brna)
{
  rna_def_node_panel_state(brna);
  rna_def_node(brna);
  rna_def_node_internal_link(brna);
  rna_def_node_link(brna);

  rna_def_internal_node(brna);
  rna_def_shader_node(brna);
  rna_def_compositor_node(brna);
  rna_def_texture_node(brna);
  rna_def_geometry_node(brna);
  rna_def_function_node(brna);

  rna_def_nodetree(brna);

  rna_def_composite_nodetree(brna);
  rna_def_shader_nodetree(brna);
  rna_def_texture_nodetree(brna);
  rna_def_geometry_nodetree(brna);

  rna_def_nodes(brna);

  def_custom_group(brna,
                   "ShaderNodeCustomGroup",
                   "ShaderNode",
                   "Shader Custom Group",
                   "Custom Shader Group Node for Python nodes",
                   "rna_ShaderNodeCustomGroup_register");
  def_custom_group(brna,
                   "CompositorNodeCustomGroup",
                   "CompositorNode",
                   "Compositor Custom Group",
                   "Custom Compositor Group Node for Python nodes",
                   "rna_CompositorNodeCustomGroup_register");
  def_custom_group(brna,
                   "NodeCustomGroup",
                   "Node",
                   "Custom Group",
                   "Base node type for custom registered node group types",
                   "rna_NodeCustomGroup_register");
  def_custom_group(brna,
                   "GeometryNodeCustomGroup",
                   "GeometryNode",
                   "Geometry Custom Group",
                   "Custom Geometry Group Node for Python nodes",
                   "rna_GeometryNodeCustomGroup_register");
}

/* clean up macro definition */
#  undef NODE_DEFINE_SUBTYPES

}  // namespace blender

#endif
