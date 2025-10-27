/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <climits>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "BLI_linear_allocator.hh"
#include "BLI_math_rotation.h"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"

#include "BKE_animsys.h"
#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_geometry_set.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"
#include "rna_internal_types.hh"

#include "IMB_colormanagement.hh"

#include "WM_types.hh"

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
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_color_tag_items[] = {
    {int(blender::bke::NodeColorTag::None),
     "NONE",
     0,
     "None",
     "Default color tag for new nodes and node groups"},
    {int(blender::bke::NodeColorTag::Attribute), "ATTRIBUTE", 0, "Attribute", ""},
    {int(blender::bke::NodeColorTag::Color), "COLOR", 0, "Color", ""},
    {int(blender::bke::NodeColorTag::Converter), "CONVERTER", 0, "Converter", ""},
    {int(blender::bke::NodeColorTag::Distort), "DISTORT", 0, "Distort", ""},
    {int(blender::bke::NodeColorTag::Filter), "FILTER", 0, "Filter", ""},
    {int(blender::bke::NodeColorTag::Geometry), "GEOMETRY", 0, "Geometry", ""},
    {int(blender::bke::NodeColorTag::Input), "INPUT", 0, "Input", ""},
    {int(blender::bke::NodeColorTag::Matte), "MATTE", 0, "Matte", ""},
    {int(blender::bke::NodeColorTag::Output), "OUTPUT", 0, "Output", ""},
    {int(blender::bke::NodeColorTag::Script), "SCRIPT", 0, "Script", ""},
    {int(blender::bke::NodeColorTag::Shader), "SHADER", 0, "Shader", ""},
    {int(blender::bke::NodeColorTag::Texture), "TEXTURE", 0, "Texture", ""},
    {int(blender::bke::NodeColorTag::Vector), "VECTOR", 0, "Vector", ""},
    {int(blender::bke::NodeColorTag::Pattern), "PATTERN", 0, "Pattern", ""},
    {int(blender::bke::NodeColorTag::Interface), "INTERFACE", 0, "Interface", ""},
    {int(blender::bke::NodeColorTag::Group), "GROUP", 0, "Group", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_mapping_type_items[] = {
    {NODE_MAPPING_TYPE_POINT, "POINT", 0, "Point", "Transform a point"},
    {NODE_MAPPING_TYPE_TEXTURE,
     "TEXTURE",
     0,
     "Texture",
     "Transform a texture by inverse mapping the texture coordinate"},
    {NODE_MAPPING_TYPE_VECTOR,
     "VECTOR",
     0,
     "Vector",
     "Transform a direction vector (Location is ignored)"},
    {NODE_MAPPING_TYPE_NORMAL,
     "NORMAL",
     0,
     "Normal",
     "Transform a unit normal vector (Location is ignored)"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_vector_rotate_type_items[] = {
    {NODE_VECTOR_ROTATE_TYPE_AXIS,
     "AXIS_ANGLE",
     0,
     "Axis Angle",
     "Rotate a point using axis angle"},
    {NODE_VECTOR_ROTATE_TYPE_AXIS_X, "X_AXIS", 0, "X Axis", "Rotate a point using X axis"},
    {NODE_VECTOR_ROTATE_TYPE_AXIS_Y, "Y_AXIS", 0, "Y Axis", "Rotate a point using Y axis"},
    {NODE_VECTOR_ROTATE_TYPE_AXIS_Z, "Z_AXIS", 0, "Z Axis", "Rotate a point using Z axis"},
    {NODE_VECTOR_ROTATE_TYPE_EULER_XYZ, "EULER_XYZ", 0, "Euler", "Rotate a point using XYZ order"},
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

const EnumPropertyItem rna_enum_node_vec_math_items[] = {
    {NODE_VECTOR_MATH_ADD, "ADD", 0, "Add", "A + B"},
    {NODE_VECTOR_MATH_SUBTRACT, "SUBTRACT", 0, "Subtract", "A - B"},
    {NODE_VECTOR_MATH_MULTIPLY, "MULTIPLY", 0, "Multiply", "Entry-wise multiply"},
    {NODE_VECTOR_MATH_DIVIDE, "DIVIDE", 0, "Divide", "Entry-wise divide"},
    {NODE_VECTOR_MATH_MULTIPLY_ADD, "MULTIPLY_ADD", 0, "Multiply Add", "A * B + C"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_VECTOR_MATH_CROSS_PRODUCT, "CROSS_PRODUCT", 0, "Cross Product", "A cross B"},
    {NODE_VECTOR_MATH_PROJECT, "PROJECT", 0, "Project", "Project A onto B"},
    {NODE_VECTOR_MATH_REFLECT,
     "REFLECT",
     0,
     "Reflect",
     "Reflect A around the normal B. B does not need to be normalized."},
    {NODE_VECTOR_MATH_REFRACT,
     "REFRACT",
     0,
     "Refract",
     "For a given incident vector A, surface normal B and ratio of indices of refraction, Ior, "
     "refract returns the refraction vector, R"},
    {NODE_VECTOR_MATH_FACEFORWARD,
     "FACEFORWARD",
     0,
     "Faceforward",
     "Orients a vector A to point away from a surface B as defined by its normal C. "
     "Returns (dot(B, C) < 0) ? A : -A"},
    {NODE_VECTOR_MATH_DOT_PRODUCT, "DOT_PRODUCT", 0, "Dot Product", "A dot B"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_VECTOR_MATH_DISTANCE, "DISTANCE", 0, "Distance", "Distance between A and B"},
    {NODE_VECTOR_MATH_LENGTH, "LENGTH", 0, "Length", "Length of A"},
    {NODE_VECTOR_MATH_SCALE, "SCALE", 0, "Scale", "A multiplied by Scale"},
    {NODE_VECTOR_MATH_NORMALIZE, "NORMALIZE", 0, "Normalize", "Normalize A"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_VECTOR_MATH_ABSOLUTE, "ABSOLUTE", 0, "Absolute", "Entry-wise absolute"},
    {NODE_VECTOR_MATH_POWER, "POWER", 0, "Power", "Entry-wise power"},
    {NODE_VECTOR_MATH_SIGN, "SIGN", 0, "Sign", "Entry-wise sign"},
    {NODE_VECTOR_MATH_MINIMUM, "MINIMUM", 0, "Minimum", "Entry-wise minimum"},
    {NODE_VECTOR_MATH_MAXIMUM, "MAXIMUM", 0, "Maximum", "Entry-wise maximum"},
    {NODE_VECTOR_MATH_FLOOR, "FLOOR", 0, "Floor", "Entry-wise floor"},
    {NODE_VECTOR_MATH_CEIL, "CEIL", 0, "Ceil", "Entry-wise ceil"},
    {NODE_VECTOR_MATH_FRACTION, "FRACTION", 0, "Fraction", "The fraction part of A entry-wise"},
    {NODE_VECTOR_MATH_MODULO, "MODULO", 0, "Modulo", "Entry-wise modulo using fmod(A,B)"},
    {NODE_VECTOR_MATH_WRAP, "WRAP", 0, "Wrap", "Entry-wise wrap(A,B)"},
    {NODE_VECTOR_MATH_SNAP,
     "SNAP",
     0,
     "Snap",
     "Round A to the largest integer multiple of B less than or equal A"},
    RNA_ENUM_ITEM_SEPR,
    {NODE_VECTOR_MATH_SINE, "SINE", 0, "Sine", "Entry-wise sin(A)"},
    {NODE_VECTOR_MATH_COSINE, "COSINE", 0, "Cosine", "Entry-wise cos(A)"},
    {NODE_VECTOR_MATH_TANGENT, "TANGENT", 0, "Tangent", "Entry-wise tan(A)"},
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
     "Divide and floor result, the largest integer smaller than or equal A"},
    {NODE_INTEGER_MATH_DIVIDE_CEIL,
     "DIVIDE_CEIL",
     0,
     "Divide Ceiling",
     "Divide and ceil result, the smallest integer greater than or equal A"},
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

const EnumPropertyItem rna_enum_node_map_range_items[] = {
    {NODE_MAP_RANGE_LINEAR,
     "LINEAR",
     0,
     "Linear",
     "Linear interpolation between From Min and From Max values"},
    {NODE_MAP_RANGE_STEPPED,
     "STEPPED",
     0,
     "Stepped Linear",
     "Stepped linear interpolation between From Min and From Max values"},
    {NODE_MAP_RANGE_SMOOTHSTEP,
     "SMOOTHSTEP",
     0,
     "Smooth Step",
     "Smooth Hermite edge interpolation between From Min and From Max values"},
    {NODE_MAP_RANGE_SMOOTHERSTEP,
     "SMOOTHERSTEP",
     0,
     "Smoother Step",
     "Smoother Hermite edge interpolation between From Min and From Max values"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_clamp_items[] = {
    {NODE_CLAMP_MINMAX, "MINMAX", 0, "Min Max", "Constrain value between min and max"},
    {NODE_CLAMP_RANGE,
     "RANGE",
     0,
     "Range",
     "Constrain value between min and max, swapping arguments when min > max"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_node_tex_dimensions_items[] = {
    {1, "1D", 0, "1D", "Use the scalar value W as input"},
    {2, "2D", 0, "2D", "Use the 2D vector (X, Y) as input. The Z component is ignored."},
    {3, "3D", 0, "3D", "Use the 3D vector (X, Y, Z) as input"},
    {4, "4D", 0, "4D", "Use the 4D vector (X, Y, Z, W) as input"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_node_geometry_curve_handle_type_items[] = {
    {GEO_NODE_CURVE_HANDLE_FREE,
     "FREE",
     ICON_HANDLE_FREE,
     "Free",
     "The handle can be moved anywhere, and does not influence the point's other handle"},
    {GEO_NODE_CURVE_HANDLE_AUTO,
     "AUTO",
     ICON_HANDLE_AUTO,
     "Auto",
     "The location is automatically calculated to be smooth"},
    {GEO_NODE_CURVE_HANDLE_VECTOR,
     "VECTOR",
     ICON_HANDLE_VECTOR,
     "Vector",
     "The location is calculated to point to the next/previous control point"},
    {GEO_NODE_CURVE_HANDLE_ALIGN,
     "ALIGN",
     ICON_HANDLE_ALIGNED,
     "Align",
     "The location is constrained to point in the opposite direction as the other handle"},
    {0, nullptr, 0, nullptr, nullptr}};

const EnumPropertyItem rna_enum_node_geometry_curve_handle_side_items[] = {
    {GEO_NODE_CURVE_HANDLE_LEFT, "LEFT", ICON_NONE, "Left", "Use the left handles"},
    {GEO_NODE_CURVE_HANDLE_RIGHT, "RIGHT", ICON_NONE, "Right", "Use the right handles"},
    {0, nullptr, 0, nullptr, nullptr}};

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

static const EnumPropertyItem rna_enum_mix_data_type_items[] = {
    {SOCK_FLOAT, "FLOAT", ICON_NODE_SOCKET_FLOAT, "Float", ""},
    {SOCK_VECTOR, "VECTOR", ICON_NODE_SOCKET_VECTOR, "Vector", ""},
    {SOCK_RGBA, "RGBA", ICON_NODE_SOCKET_RGBA, "Color", ""},
    {SOCK_ROTATION, "ROTATION", ICON_NODE_SOCKET_ROTATION, "Rotation", ""},
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

static const EnumPropertyItem prop_shader_output_target_items[] = {
    {SHD_OUTPUT_ALL,
     "ALL",
     0,
     "All",
     "Use shaders for all renderers and viewports, unless there exists a more specific output"},
    {SHD_OUTPUT_EEVEE, "EEVEE", 0, "EEVEE", "Use shaders for EEVEE renderer"},
    {SHD_OUTPUT_CYCLES, "CYCLES", 0, "Cycles", "Use shaders for Cycles renderer"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_cryptomatte_layer_name_items[] = {
    {0, "CryptoObject", 0, "Object", "Use Object layer"},
    {1, "CryptoMaterial", 0, "Material", "Use Material layer"},
    {2, "CryptoAsset", 0, "Asset", "Use Asset layer"},
    {0, nullptr, 0, nullptr, nullptr},
};

#endif /* !RNA_RUNTIME */

#undef ITEM_ATTRIBUTE
#undef ITEM_FLOAT
#undef ITEM_VECTOR
#undef ITEM_COLOR
#undef ITEM_BOOLEAN

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BLI_string.h"
#  include "BLI_string_utf8.h"

#  include "BKE_context.hh"
#  include "BKE_cryptomatte.hh"
#  include "BKE_global.hh"
#  include "BKE_image.hh"
#  include "BKE_main_invariants.hh"
#  include "BKE_node_legacy_types.hh"
#  include "BKE_node_runtime.hh"
#  include "BKE_node_tree_update.hh"
#  include "BKE_report.hh"
#  include "BKE_scene.hh"
#  include "BKE_texture.h"

#  include "BLF_api.hh"

#  include "ED_node.hh"
#  include "ED_render.hh"

#  include "GPU_material.hh"

#  include "NOD_common.hh"
#  include "NOD_composite.hh"
#  include "NOD_compositor_file_output.hh"
#  include "NOD_fn_format_string.hh"
#  include "NOD_geo_bake.hh"
#  include "NOD_geo_bundle.hh"
#  include "NOD_geo_capture_attribute.hh"
#  include "NOD_geo_closure.hh"
#  include "NOD_geo_field_to_grid.hh"
#  include "NOD_geo_foreach_geometry_element.hh"
#  include "NOD_geo_index_switch.hh"
#  include "NOD_geo_menu_switch.hh"
#  include "NOD_geo_repeat.hh"
#  include "NOD_geo_simulation.hh"
#  include "NOD_geo_viewer.hh"
#  include "NOD_geometry.hh"
#  include "NOD_geometry_nodes_lazy_function.hh"
#  include "NOD_rna_define.hh"
#  include "NOD_shader.h"
#  include "NOD_socket.hh"
#  include "NOD_socket_items.hh"
#  include "NOD_texture.h"

#  include "RE_engine.h"
#  include "RE_pipeline.h"
#  include "RE_texture.h"

#  include "DNA_scene_types.h"
#  include "DNA_text_types.h"

#  include "WM_api.hh"

#  include "DEG_depsgraph_query.hh"

using blender::float2;
using blender::nodes::BakeItemsAccessor;
using blender::nodes::CaptureAttributeItemsAccessor;
using blender::nodes::ClosureInputItemsAccessor;
using blender::nodes::ClosureOutputItemsAccessor;
using blender::nodes::CombineBundleItemsAccessor;
using blender::nodes::EvaluateClosureInputItemsAccessor;
using blender::nodes::EvaluateClosureOutputItemsAccessor;
using blender::nodes::FieldToGridItemsAccessor;
using blender::nodes::FileOutputItemsAccessor;
using blender::nodes::ForeachGeometryElementGenerationItemsAccessor;
using blender::nodes::ForeachGeometryElementInputItemsAccessor;
using blender::nodes::ForeachGeometryElementMainItemsAccessor;
using blender::nodes::FormatStringItemsAccessor;
using blender::nodes::GeoViewerItemsAccessor;
using blender::nodes::IndexSwitchItemsAccessor;
using blender::nodes::MenuSwitchItemsAccessor;
using blender::nodes::RepeatItemsAccessor;
using blender::nodes::SeparateBundleItemsAccessor;
using blender::nodes::SimulationItemsAccessor;

extern FunctionRNA rna_NodeTree_poll_func;
extern FunctionRNA rna_NodeTree_update_func;
extern FunctionRNA rna_NodeTree_get_from_context_func;
extern FunctionRNA rna_NodeTree_valid_socket_type_func;
extern FunctionRNA rna_Node_poll_func;
extern FunctionRNA rna_Node_poll_instance_func;
extern FunctionRNA rna_Node_update_func;
extern FunctionRNA rna_Node_insert_link_func;
extern FunctionRNA rna_Node_init_func;
extern FunctionRNA rna_Node_copy_func;
extern FunctionRNA rna_Node_free_func;
extern FunctionRNA rna_Node_draw_buttons_func;
extern FunctionRNA rna_Node_draw_buttons_ext_func;
extern FunctionRNA rna_Node_draw_label_func;

void rna_Node_socket_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr);

int rna_node_tree_idname_to_enum(const char *idname)
{
  using namespace blender;
  Span<const bke::bNodeTreeType *> types = bke::node_tree_types_get();
  for (const int i : types.index_range()) {
    const bke::bNodeTreeType *nt = types[i];
    if (nt->idname == idname) {
      return i;
    }
  }
  return -1;
}

blender::bke::bNodeTreeType *rna_node_tree_type_from_enum(int value)
{
  blender::Span<blender::bke::bNodeTreeType *> types = blender::bke::node_tree_types_get();
  return types.index_range().contains(value) ? types[value] : nullptr;
}

const EnumPropertyItem *rna_node_tree_type_itemf(
    void *data, bool (*poll)(void *data, blender::bke::bNodeTreeType *), bool *r_free)
{
  using namespace blender;
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
  using namespace blender;

  bke::bNodeSocketType *socket_type = bke::node_socket_type_find(idname);

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

blender::bke::bNodeSocketType *rna_node_socket_type_from_enum(int value)
{
  blender::Span<blender::bke::bNodeSocketType *> types = blender::bke::node_socket_types_get();
  return types.index_range().contains(value) ? types[value] : nullptr;
}

const EnumPropertyItem *rna_node_socket_type_itemf(
    void *data, bool (*poll)(void *data, blender::bke::bNodeSocketType *), bool *r_free)
{
  using namespace blender;
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
    tmp.name = blender::bke::node_socket_type_label(*stype).c_str();
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
  const blender::bke::bNodeType *ntype = node->typeinfo;
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
  const blender::bke::bNodeType *ntype = node->typeinfo;
  blender::StringRef(ntype->idname).copy_unsafe(value);
}

static int rna_Node_bl_idname_length(PointerRNA *ptr)
{
  const bNode *node = ptr->data_as<bNode>();
  const blender::bke::bNodeType *ntype = node->typeinfo;
  return ntype->idname.size();
}

static void rna_Node_bl_idname_set(PointerRNA *ptr, const char *value)
{
  bNode *node = ptr->data_as<bNode>();
  blender::bke::bNodeType *ntype = node->typeinfo;
  ntype->idname = value;
}

static void rna_Node_bl_label_get(PointerRNA *ptr, char *value)
{
  const bNode *node = ptr->data_as<bNode>();
  const blender::bke::bNodeType *ntype = node->typeinfo;
  blender::StringRef(ntype->ui_name).copy_unsafe(value);
}

static int rna_Node_bl_label_length(PointerRNA *ptr)
{
  const bNode *node = ptr->data_as<bNode>();
  const blender::bke::bNodeType *ntype = node->typeinfo;
  return ntype->ui_name.size();
}

static void rna_Node_bl_label_set(PointerRNA *ptr, const char *value)
{
  bNode *node = ptr->data_as<bNode>();
  blender::bke::bNodeType *ntype = node->typeinfo;
  ntype->ui_name = value;
}

static void rna_Node_bl_description_get(PointerRNA *ptr, char *value)
{
  const bNode *node = ptr->data_as<bNode>();
  const blender::bke::bNodeType *ntype = node->typeinfo;
  blender::StringRef(ntype->ui_description).copy_unsafe(value);
}

static int rna_Node_bl_description_length(PointerRNA *ptr)
{
  const bNode *node = ptr->data_as<bNode>();
  const blender::bke::bNodeType *ntype = node->typeinfo;
  return ntype->ui_description.size();
}

static void rna_Node_bl_description_set(PointerRNA *ptr, const char *value)
{
  bNode *node = ptr->data_as<bNode>();
  blender::bke::bNodeType *ntype = node->typeinfo;
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
  bNode *node = ptr->data_as<bNode>();
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
    return &RNA_NodeTree;
  }
}

static bool rna_NodeTree_poll(const bContext *C, blender::bke::bNodeTreeType *ntreetype)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool visible;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, ntreetype->rna_ext.srna, nullptr); /* dummy */
  func = &rna_NodeTree_poll_func;                 /* RNA_struct_find_function(&ptr, "poll"); */

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
  func = &rna_NodeTree_update_func; /* RNA_struct_find_function(&ptr, "update"); */

  RNA_parameter_list_create(&list, &ptr, func);
  ntree->typeinfo->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_NodeTree_get_from_context(const bContext *C,
                                          blender::bke::bNodeTreeType *ntreetype,
                                          bNodeTree **r_ntree,
                                          ID **r_id,
                                          ID **r_from)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret1, *ret2, *ret3;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, ntreetype->rna_ext.srna, nullptr); /* dummy */
  // RNA_struct_find_function(&ptr, "get_from_context");
  func = &rna_NodeTree_get_from_context_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  ntreetype->rna_ext.call(const_cast<bContext *>(C), &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "result_1", &ret1);
  RNA_parameter_get_lookup(&list, "result_2", &ret2);
  RNA_parameter_get_lookup(&list, "result_3", &ret3);
  *r_ntree = *(bNodeTree **)ret1;
  *r_id = *(ID **)ret2;
  *r_from = *(ID **)ret3;

  RNA_parameter_list_free(&list);
}

static bool rna_NodeTree_valid_socket_type(blender::bke::bNodeTreeType *ntreetype,
                                           blender::bke::bNodeSocketType *socket_type)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool valid;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, ntreetype->rna_ext.srna, nullptr); /* dummy */
  func = &rna_NodeTree_valid_socket_type_func;

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
  blender::bke::bNodeTreeType *nt = static_cast<blender::bke::bNodeTreeType *>(
      RNA_struct_blender_type_get(type));

  if (!nt) {
    return false;
  }

  RNA_struct_free_extension(type, &nt->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  blender::bke::node_tree_type_free_link(*nt);

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
  blender::bke::bNodeTreeType *nt;
  bNodeTree dummy_ntree = {};
  bool have_function[4];

  /* setup dummy tree & tree type to store static properties in */
  blender::bke::bNodeTreeType dummy_nt = {};
  dummy_ntree.typeinfo = &dummy_nt;
  PointerRNA dummy_ntree_ptr = RNA_pointer_create_discrete(nullptr, &RNA_NodeTree, &dummy_ntree);

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
  nt = blender::bke::node_tree_type_find(dummy_nt.idname);
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
  nt = MEM_new<blender::bke::bNodeTreeType>(__func__, dummy_nt);

  nt->type = NTREE_CUSTOM;

  nt->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, nt->idname.c_str(), &RNA_NodeTree);
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

  blender::bke::node_tree_type_add(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  BKE_main_ensure_invariants(*bmain);
  return nt->rna_ext.srna;
}

static bool rna_NodeTree_check(bNodeTree *ntree, ReportList *reports)
{
  if (!blender::bke::node_tree_is_registered(*ntree)) {
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
  blender::bke::node_update_asset_metadata(*reinterpret_cast<bNodeTree *>(ptr->owner_id));
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
    switch (blender::bke::NodeColorTag(item->value)) {
      case blender::bke::NodeColorTag::Attribute:
      case blender::bke::NodeColorTag::Geometry: {
        if (ntree.type == NTREE_GEOMETRY) {
          RNA_enum_item_add(&items, &items_num, item);
        }
        break;
      }
      case blender::bke::NodeColorTag::Shader:
      case blender::bke::NodeColorTag::Script: {
        if (ntree.type == NTREE_SHADER) {
          RNA_enum_item_add(&items, &items_num, item);
        }
        break;
      }
      case blender::bke::NodeColorTag::Distort:
      case blender::bke::NodeColorTag::Filter:
      case blender::bke::NodeColorTag::Matte: {
        if (ntree.type == NTREE_COMPOSIT) {
          RNA_enum_item_add(&items, &items_num, item);
        }
        break;
      }
      case blender::bke::NodeColorTag::Pattern:
      case blender::bke::NodeColorTag::Interface:
      case blender::bke::NodeColorTag::Group: {
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
                                    blender::StringRefNull type)
{
  blender::bke::bNodeType *ntype;
  bNode *node;

  if (!rna_NodeTree_check(ntree, reports)) {
    return nullptr;
  }

  /* If the given idname is an alias, translate it to the proper idname. */
  type = blender::bke::node_type_find_alias(type);

  ntype = blender::bke::node_type_find(type);
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

  node = blender::bke::node_add_node(C, *ntree, type);
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

  blender::bke::node_remove_node(bmain, *ntree, *node, true);

  node_ptr->invalidate();

  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTree_node_clear(bNodeTree *ntree, Main *bmain, ReportList *reports)
{
  bNode *node = static_cast<bNode *>(ntree->nodes.first);

  if (!rna_NodeTree_check(ntree, reports)) {
    return;
  }

  while (node) {
    bNode *next_node = node->next;

    blender::bke::node_remove_node(bmain, *ntree, *node, true);

    node = next_node;
  }

  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static PointerRNA rna_NodeTree_active_node_get(PointerRNA *ptr)
{
  bNodeTree *ntree = ptr->data_as<bNodeTree>();
  bNode *node = blender::bke::node_get_active(*ntree);
  return RNA_pointer_create_with_parent(*ptr, &RNA_Node, node);
}

static void rna_NodeTree_active_node_set(PointerRNA *ptr,
                                         const PointerRNA value,
                                         ReportList * /*reports*/)
{
  bNodeTree *ntree = ptr->data_as<bNodeTree>();
  bNode *node = static_cast<bNode *>(value.data);

  if (node && BLI_findindex(&ntree->nodes, node) != -1) {
    blender::bke::node_set_active(*ntree, *node);

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
      blender::bke::node_tree_set_output(*ntree);
      BKE_ntree_update_tag_active_output_changed(ntree);
    }
  }
  else {
    blender::bke::node_clear_active(*ntree);
  }
}

static void node_viewer_set_shortcut_fn(bNode *node, bNodeTree &ntree, int value)
{
  /* Avoid having two nodes with the same shortcut. */
  for (bNode *other_node : ntree.all_nodes()) {
    if ((other_node->is_type("CompositorNodeViewer") ||
         other_node->is_type("GeometryNodeViewer")) &&
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

  bNode *fromnode = blender::bke::node_find_node_try(*ntree, *fromsock);
  bNode *tonode = blender::bke::node_find_node_try(*ntree, *tosock);
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
      blender::bke::NodeInsertLinkParams params{*ntree, *fromnode, new_link};
      if (!fromnode->typeinfo->insert_link(params)) {
        return nullptr;
      }
    }
    if (tonode->typeinfo->insert_link) {
      blender::bke::NodeInsertLinkParams params{*ntree, *tonode, new_link};
      if (!tonode->typeinfo->insert_link(params)) {
        return nullptr;
      }
    }

    fromsock = new_link.fromsock;
    tosock = new_link.tosock;
  }

  if (verify_limits) {
    /* remove other socket links if limit is exceeded */
    if (blender::bke::node_count_socket_links(*ntree, *fromsock) + 1 >
        blender::bke::node_socket_link_limit(*fromsock))
    {
      blender::bke::node_remove_socket_links(*ntree, *fromsock);
    }
    if (blender::bke::node_count_socket_links(*ntree, *tosock) + 1 >
        blender::bke::node_socket_link_limit(*tosock))
    {
      blender::bke::node_remove_socket_links(*ntree, *tosock);
    }
    if (tosock->flag & SOCK_MULTI_INPUT) {
      LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
        if (link->fromsock == fromsock && link->tosock == tosock) {
          blender::bke::node_remove_link(ntree, *link);
        }
      }
    }
  }

  bNodeLink &ret = blender::bke::node_add_link(*ntree, *fromnode, *fromsock, *tonode, *tosock);

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

  blender::bke::node_remove_link(ntree, *link);
  link_ptr->invalidate();

  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTree_link_clear(bNodeTree *ntree, Main *bmain, ReportList *reports)
{
  bNodeLink *link = static_cast<bNodeLink *>(ntree->links.first);

  if (!rna_NodeTree_check(ntree, reports)) {
    return;
  }

  while (link) {
    bNodeLink *next_link = link->next;

    blender::bke::node_remove_link(ntree, *link);

    link = next_link;
  }
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static bool rna_NodeTree_contains_tree(bNodeTree *tree, bNodeTree *sub_tree)
{
  return blender::bke::node_tree_contains_tree(*tree, *sub_tree);
}

static void rna_NodeTree_bl_idname_get(PointerRNA *ptr, char *value)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const blender::bke::bNodeTreeType *ntype = node->typeinfo;
  blender::StringRef(ntype->idname).copy_unsafe(value);
}

static int rna_NodeTree_bl_idname_length(PointerRNA *ptr)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const blender::bke::bNodeTreeType *ntype = node->typeinfo;
  return ntype->idname.size();
}

static void rna_NodeTree_bl_idname_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *node = ptr->data_as<bNodeTree>();
  blender::bke::bNodeTreeType *ntype = node->typeinfo;
  ntype->idname = value;
}

static void rna_NodeTree_bl_label_get(PointerRNA *ptr, char *value)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const blender::bke::bNodeTreeType *ntype = node->typeinfo;
  blender::StringRef(ntype->ui_name).copy_unsafe(value);
}

static int rna_NodeTree_bl_label_length(PointerRNA *ptr)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const blender::bke::bNodeTreeType *ntype = node->typeinfo;
  return ntype->ui_name.size();
}

static void rna_NodeTree_bl_label_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *node = ptr->data_as<bNodeTree>();
  blender::bke::bNodeTreeType *ntype = node->typeinfo;
  ntype->ui_name = value;
}

static void rna_NodeTree_bl_description_get(PointerRNA *ptr, char *value)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const blender::bke::bNodeTreeType *ntype = node->typeinfo;
  blender::StringRef(ntype->ui_description).copy_unsafe(value);
}

static int rna_NodeTree_bl_description_length(PointerRNA *ptr)
{
  const bNodeTree *node = ptr->data_as<bNodeTree>();
  const blender::bke::bNodeTreeType *ntype = node->typeinfo;
  return ntype->ui_description.size();
}

static void rna_NodeTree_bl_description_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *node = ptr->data_as<bNodeTree>();
  blender::bke::bNodeTreeType *ntype = node->typeinfo;
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
  tree->runtime->logged_zone_graphs = std::make_unique<blender::bke::LoggedZoneGraphs>();
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
  return blender::bke::node_link_is_hidden(*link);
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

static StructRNA *rna_Node_refine(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();

  if (node->typeinfo->rna_ext.srna) {
    return node->typeinfo->rna_ext.srna;
  }
  else {
    return ptr->type;
  }
}

static std::optional<std::string> rna_Node_path(const PointerRNA *ptr)
{
  const bNode *node = ptr->data_as<bNode>();
  char name_esc[sizeof(node->name) * 2];

  BLI_str_escape(name_esc, node->name, sizeof(name_esc));
  return fmt::format("nodes[\"{}\"]", name_esc);
}

std::optional<std::string> rna_Node_ImageUser_path(const PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (!ELEM(ntree->type, NTREE_SHADER, NTREE_CUSTOM)) {
    return std::nullopt;
  }

  for (bNode *node = static_cast<bNode *>(ntree->nodes.first); node; node = node->next) {
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
    BLI_str_escape(name_esc, node->name, sizeof(name_esc));
    return fmt::format("nodes[\"{}\"].image_user", name_esc);
  }

  return std::nullopt;
}

static bool rna_Node_poll(const blender::bke::bNodeType *ntype,
                          const bNodeTree *ntree,
                          const char ** /*r_disabled_hint*/)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool visible;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, ntype->rna_ext.srna, nullptr); /* dummy */
  func = &rna_Node_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

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
  func = &rna_Node_poll_instance_func; /* RNA_struct_find_function(&ptr, "poll_instance"); */

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
  func = &rna_Node_update_func; /* RNA_struct_find_function(&ptr, "update"); */

  RNA_parameter_list_create(&list, &ptr, func);
  node->typeinfo->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static bool rna_Node_insert_link(blender::bke::NodeInsertLinkParams &params)
{
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(&params.ntree), params.node.typeinfo->rna_ext.srna, &params.node);
  func = &rna_Node_insert_link_func;

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

  func = &rna_Node_init_func; /* RNA_struct_find_function(&ptr, "init"); */

  RNA_parameter_list_create(&list, ptr, func);
  node->typeinfo->rna_ext.call(const_cast<bContext *>(C), ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_Node_copy(PointerRNA *ptr, const bNode *copynode)
{
  bNode *node = ptr->data_as<bNode>();
  ParameterList list;
  FunctionRNA *func;

  func = &rna_Node_copy_func; /* RNA_struct_find_function(&ptr, "copy"); */

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

  func = &rna_Node_free_func; /* RNA_struct_find_function(&ptr, "free"); */

  RNA_parameter_list_create(&list, ptr, func);
  node->typeinfo->rna_ext.call(nullptr, ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_Node_draw_buttons(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  ParameterList list;
  FunctionRNA *func;

  func = &rna_Node_draw_buttons_func; /* RNA_struct_find_function(&ptr, "draw_buttons"); */

  RNA_parameter_list_create(&list, ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "layout", &layout);
  node->typeinfo->rna_ext.call(C, ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_Node_draw_buttons_ext(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  ParameterList list;
  FunctionRNA *func;

  func = &rna_Node_draw_buttons_ext_func; /* RNA_struct_find_function(&ptr, "draw_buttons_ext"); */

  RNA_parameter_list_create(&list, ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "layout", &layout);
  node->typeinfo->rna_ext.call(C, ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_Node_draw_label(const bNodeTree *ntree,
                                const bNode *node,
                                char *label,
                                int label_maxncpy)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  char *rlabel;

  func = &rna_Node_draw_label_func; /* RNA_struct_find_function(&ptr, "draw_label"); */

  PointerRNA ptr = RNA_pointer_create_discrete(
      const_cast<ID *>(&ntree->id), &RNA_Node, const_cast<bNode *>(node));
  RNA_parameter_list_create(&list, &ptr, func);
  node->typeinfo->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "label", &ret);
  rlabel = static_cast<char *>(ret);
  BLI_strncpy(label, rlabel != nullptr ? rlabel : "", label_maxncpy);

  RNA_parameter_list_free(&list);
}

static bool rna_Node_is_registered_node_type(StructRNA *type)
{
  return (RNA_struct_blender_type_get(type) != nullptr);
}

static bool rna_Node_is_builtin(blender::bke::bNodeType *nt)
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

static bool rna_Node_unregister(Main *bmain, StructRNA *type)
{
  blender::bke::bNodeType *nt = static_cast<blender::bke::bNodeType *>(
      RNA_struct_blender_type_get(type));

  if (!nt || rna_Node_is_builtin(nt)) {
    return false;
  }

  RNA_struct_free_extension(type, &nt->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  /* this also frees the allocated nt pointer, no MEM_free call needed! */
  blender::bke::node_unregister_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  BKE_main_ensure_invariants(*bmain);
  return true;
}

/* Generic internal registration function.
 * Can be used to implement callbacks for registerable RNA node sub-types.
 */
static blender::bke::bNodeType *rna_Node_register_base(Main *bmain,
                                                       ReportList *reports,
                                                       StructRNA *basetype,
                                                       void *data,
                                                       const char *identifier,
                                                       StructValidateFunc validate,
                                                       StructCallbackFunc call,
                                                       StructFreeFunc free)
{
  blender::bke::bNodeType *nt;
  bNode dummy_node = {};
  FunctionRNA *func;
  PropertyRNA *parm;
  bool have_function[10];

  /* setup dummy node & node type to store static properties in */
  blender::bke::bNodeType dummy_nt = {};
  /* this does some additional initialization of default values */
  blender::bke::node_type_base_custom(dummy_nt, identifier, "", "CUSTOM", 0);

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
  nt = blender::bke::node_type_find(dummy_nt.idname);
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
  nt = MEM_new<blender::bke::bNodeType>(__func__, dummy_nt);
  nt->free_self = [](blender::bke::bNodeType *type) { MEM_delete(type); };

  nt->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, nt->idname.c_str(), basetype);
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
  CLAMP(nt->width, nt->minwidth, nt->maxwidth);
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
  blender::bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_Node, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  blender::bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  BKE_main_ensure_invariants(*bmain);
  return nt->rna_ext.srna;
}

static const EnumPropertyItem *itemf_function_check(
    const EnumPropertyItem *original_item_array,
    blender::FunctionRef<bool(const EnumPropertyItem *item)> value_supported)
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

static void geometry_node_asset_trait_flag_set(PointerRNA *ptr,
                                               const GeometryNodeAssetTraitFlag flag,
                                               const bool value)
{
  bNodeTree *ntree = ptr->data_as<bNodeTree>();
  if (!ntree->geometry_node_asset_traits) {
    ntree->geometry_node_asset_traits = MEM_callocN<GeometryNodeAssetTraits>(__func__);
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

static bool generic_attribute_type_supported(const EnumPropertyItem *item)
{
  return ELEM(item->value,
              CD_PROP_FLOAT,
              CD_PROP_FLOAT2,
              CD_PROP_FLOAT3,
              CD_PROP_COLOR,
              CD_PROP_BOOL,
              CD_PROP_INT32,
              CD_PROP_BYTE_COLOR,
              CD_PROP_QUATERNION,
              CD_PROP_FLOAT4X4);
}

static bool generic_attribute_type_supported_with_socket(const EnumPropertyItem *item)
{
  return generic_attribute_type_supported(item) &&
         !ELEM(item->value, CD_PROP_BYTE_COLOR, CD_PROP_FLOAT2);
}

static const EnumPropertyItem *rna_GeometryNodeAttributeType_type_with_socket_itemf(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free)
{
  *r_free = true;
  return itemf_function_check(rna_enum_attribute_type_items,
                              generic_attribute_type_supported_with_socket);
}

static const EnumPropertyItem *rna_GeometryNodeAttributeDomain_attribute_domain_itemf(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free)
{
  using namespace blender;
  EnumPropertyItem *item_array = nullptr;
  int items_len = 0;

  for (const EnumPropertyItem *item = rna_enum_attribute_domain_items; item->identifier != nullptr;
       item++)
  {
    RNA_enum_item_add(&item_array, &items_len, item);
  }
  RNA_enum_item_end(&item_array, &items_len);

  *r_free = true;
  return item_array;
}

static StructRNA *rna_ShaderNode_register(Main *bmain,
                                          ReportList *reports,
                                          void *data,
                                          const char *identifier,
                                          StructValidateFunc validate,
                                          StructCallbackFunc call,
                                          StructFreeFunc free)
{
  blender::bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_ShaderNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  blender::bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

static StructRNA *rna_CompositorNode_register(Main *bmain,
                                              ReportList *reports,
                                              void *data,
                                              const char *identifier,
                                              StructValidateFunc validate,
                                              StructCallbackFunc call,
                                              StructFreeFunc free)
{
  blender::bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_CompositorNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  blender::bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

static StructRNA *rna_TextureNode_register(Main *bmain,
                                           ReportList *reports,
                                           void *data,
                                           const char *identifier,
                                           StructValidateFunc validate,
                                           StructCallbackFunc call,
                                           StructFreeFunc free)
{
  blender::bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_TextureNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  blender::bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

static StructRNA *rna_GeometryNode_register(Main *bmain,
                                            ReportList *reports,
                                            void *data,
                                            const char *identifier,
                                            StructValidateFunc validate,
                                            StructCallbackFunc call,
                                            StructFreeFunc free)
{
  blender::bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_GeometryNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  blender::bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

static StructRNA *rna_FunctionNode_register(Main *bmain,
                                            ReportList *reports,
                                            void *data,
                                            const char *identifier,
                                            StructValidateFunc validate,
                                            StructCallbackFunc call,
                                            StructFreeFunc free)
{
  blender::bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_FunctionNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  blender::bke::node_register_type(*nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

static IDProperty **rna_Node_idprops(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  return &node->prop;
}

static IDProperty **rna_Node_system_idprops(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  return &node->system_properties;
}

static void rna_Node_parent_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  bNode *node = ptr->data_as<bNode>();
  bNode *parent = static_cast<bNode *>(value.data);
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);

  if (!parent) {
    blender::bke::node_detach_node(*ntree, *node);
    return;
  }

  /* XXX only Frame node allowed for now,
   * in the future should have a poll function or so to test possible attachment.
   */
  if (!parent->is_frame()) {
    return;
  }

  if (blender::bke::node_is_parent_and_child(*node, *parent)) {
    return;
  }

  blender::bke::node_detach_node(*ntree, *node);
  blender::bke::node_attach_node(*ntree, *node, *parent);
}

static void rna_Node_internal_links_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  bNodeLink *begin;
  int len;
  blender::bke::node_internal_links(*node, &begin, &len);
  rna_iterator_array_begin(iter, ptr, begin, sizeof(bNodeLink), len, false, nullptr);
}

/**
 * Forbid identifier lookup in nodes whose identifiers are likely to change soon because
 * dynamically typed sockets are joined into one.
 */
static bool allow_identifier_lookup(const bNode &node)
{
  switch (node.type_legacy) {
    case FN_NODE_RANDOM_VALUE:
    case SH_NODE_MIX:
    case FN_NODE_COMPARE:
    case SH_NODE_MAP_RANGE:
      return false;
    default:
      return true;
  }
}

static bNodeSocket *find_socket_by_key(bNode &node,
                                       const eNodeSocketInOut in_out,
                                       const blender::StringRef key)
{
  ListBase *sockets = in_out == SOCK_IN ? &node.inputs : &node.outputs;
  if (allow_identifier_lookup(node)) {
    LISTBASE_FOREACH (bNodeSocket *, socket, sockets) {
      if (socket->is_available()) {
        if (socket->identifier == key) {
          return socket;
        }
      }
    }
  }
  LISTBASE_FOREACH (bNodeSocket *, socket, sockets) {
    if (socket->is_available()) {
      if (socket->name == key) {
        return socket;
      }
    }
  }
  return nullptr;
}

static bool rna_NodeInputs_lookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
  bNode *node = ptr->data_as<bNode>();
  if (bNodeSocket *socket = find_socket_by_key(*node, SOCK_IN, key)) {
    rna_pointer_create_with_ancestors(*ptr, &RNA_NodeSocket, socket, *r_ptr);
    return true;
  }
  return false;
}

static bool rna_NodeOutputs_lookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
  bNode *node = ptr->data_as<bNode>();
  if (bNodeSocket *socket = find_socket_by_key(*node, SOCK_OUT, key)) {
    rna_pointer_create_with_ancestors(*ptr, &RNA_NodeSocket, socket, *r_ptr);
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

  if (node->is_frame() && blender::bke::node_is_parent_and_child(*node, *parent)) {
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
  blender::bke::node_set_selected(*node, value);
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

  blender::bke::node_unique_name(*ntree, *node);

  /* fix all the animation data which may link to this */
  BKE_animdata_fix_paths_rename_all(nullptr, "nodes", oldname, node->name);
}

static int rna_Node_color_tag_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  return int(blender::bke::node_color_tag(*node));
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
  bNodeSocket *sock = blender::bke::node_add_socket(
      *ntree, *node, SOCK_IN, type, identifier, name);

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
  bNodeSocket *sock = blender::bke::node_add_socket(
      *ntree, *node, SOCK_OUT, type, identifier, name);

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
    blender::bke::node_remove_socket(*ntree, *node, *sock);

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

  for (sock = static_cast<bNodeSocket *>(node->inputs.first); sock; sock = nextsock) {
    nextsock = sock->next;
    blender::bke::node_remove_socket(*ntree, *node, *sock);
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

  for (sock = static_cast<bNodeSocket *>(node->outputs.first); sock; sock = nextsock) {
    nextsock = sock->next;
    blender::bke::node_remove_socket(*ntree, *node, *sock);
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
  const float2 dimensions = blender::bke::node_dimensions_get(*node);
  value[0] = dimensions[0];
  value[1] = dimensions[1];
}

/* ******** Node Types ******** */

static void rna_NodeInternalSocketTemplate_name_get(PointerRNA *ptr, char *value)
{
  blender::bke::bNodeSocketTemplate *stemp = static_cast<blender::bke::bNodeSocketTemplate *>(
      ptr->data);
  strcpy(value, stemp->name);
}

static int rna_NodeInternalSocketTemplate_name_length(PointerRNA *ptr)
{
  blender::bke::bNodeSocketTemplate *stemp = static_cast<blender::bke::bNodeSocketTemplate *>(
      ptr->data);
  return strlen(stemp->name);
}

static void rna_NodeInternalSocketTemplate_identifier_get(PointerRNA *ptr, char *value)
{
  blender::bke::bNodeSocketTemplate *stemp = static_cast<blender::bke::bNodeSocketTemplate *>(
      ptr->data);
  strcpy(value, stemp->identifier);
}

static int rna_NodeInternalSocketTemplate_identifier_length(PointerRNA *ptr)
{
  blender::bke::bNodeSocketTemplate *stemp = static_cast<blender::bke::bNodeSocketTemplate *>(
      ptr->data);
  return strlen(stemp->identifier);
}

static int rna_NodeInternalSocketTemplate_type_get(PointerRNA *ptr)
{
  blender::bke::bNodeSocketTemplate *stemp = static_cast<blender::bke::bNodeSocketTemplate *>(
      ptr->data);
  return stemp->type;
}

static PointerRNA rna_NodeInternal_input_template(StructRNA *srna, int index)
{
  blender::bke::bNodeType *ntype = static_cast<blender::bke::bNodeType *>(
      RNA_struct_blender_type_get(srna));
  if (ntype && ntype->inputs) {
    blender::bke::bNodeSocketTemplate *stemp = ntype->inputs;
    int i = 0;
    while (i < index && stemp->type >= 0) {
      i++;
      stemp++;
    }
    if (i == index && stemp->type >= 0) {
      PointerRNA ptr = RNA_pointer_create_discrete(
          nullptr, &RNA_NodeInternalSocketTemplate, stemp);
      return ptr;
    }
  }
  return PointerRNA_NULL;
}

static PointerRNA rna_NodeInternal_output_template(StructRNA *srna, int index)
{
  blender::bke::bNodeType *ntype = static_cast<blender::bke::bNodeType *>(
      RNA_struct_blender_type_get(srna));
  if (ntype && ntype->outputs) {
    blender::bke::bNodeSocketTemplate *stemp = ntype->outputs;
    int i = 0;
    while (i < index && stemp->type >= 0) {
      i++;
      stemp++;
    }
    if (i == index && stemp->type >= 0) {
      PointerRNA ptr = RNA_pointer_create_discrete(
          nullptr, &RNA_NodeInternalSocketTemplate, stemp);
      return ptr;
    }
  }
  return PointerRNA_NULL;
}

static bool rna_NodeInternal_poll(StructRNA *srna, bNodeTree *ntree)
{
  blender::bke::bNodeType *ntype = static_cast<blender::bke::bNodeType *>(
      RNA_struct_blender_type_get(srna));
  const char *disabled_hint;
  return ntype && (!ntype->poll || ntype->poll(ntype, ntree, &disabled_hint));
}

static bool rna_NodeInternal_poll_instance(bNode *node, bNodeTree *ntree)
{
  blender::bke::bNodeType *ntype = node->typeinfo;
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

static void rna_NodeInternal_draw_buttons(ID *id, bNode *node, bContext *C, uiLayout *layout)
{
  if (node->typeinfo->draw_buttons) {
    PointerRNA ptr = RNA_pointer_create_discrete(id, &RNA_Node, node);
    node->typeinfo->draw_buttons(layout, C, &ptr);
  }
}

static void rna_NodeInternal_draw_buttons_ext(ID *id, bNode *node, bContext *C, uiLayout *layout)
{
  if (node->typeinfo->draw_buttons_ex) {
    PointerRNA ptr = RNA_pointer_create_discrete(id, &RNA_Node, node);
    node->typeinfo->draw_buttons_ex(layout, C, &ptr);
  }
  else if (node->typeinfo->draw_buttons) {
    PointerRNA ptr = RNA_pointer_create_discrete(id, &RNA_Node, node);
    node->typeinfo->draw_buttons(layout, C, &ptr);
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
  blender::bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_NodeCustomGroup, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  blender::bke::node_register_type(*nt);

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
  blender::bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_GeometryNodeCustomGroup, data, identifier, validate, call, free);

  if (!nt) {
    return nullptr;
  }

  nt->type_legacy = NODE_CUSTOM_GROUP;

  register_node_type_geo_custom_group(nt);

  blender::bke::node_register_type(*nt);

  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

void register_node_type_geo_custom_group(blender::bke::bNodeType *ntype);

static StructRNA *rna_ShaderNodeCustomGroup_register(Main *bmain,
                                                     ReportList *reports,
                                                     void *data,
                                                     const char *identifier,
                                                     StructValidateFunc validate,
                                                     StructCallbackFunc call,
                                                     StructFreeFunc free)
{
  blender::bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_ShaderNodeCustomGroup, data, identifier, validate, call, free);

  if (!nt) {
    return nullptr;
  }

  nt->type_legacy = NODE_CUSTOM_GROUP;

  register_node_type_sh_custom_group(nt);

  blender::bke::node_register_type(*nt);

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
  blender::bke::bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_CompositorNodeCustomGroup, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  nt->type_legacy = NODE_CUSTOM_GROUP;

  register_node_type_cmp_custom_group(nt);

  blender::bke::node_register_type(*nt);

  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

static void rna_CompositorNode_tag_need_exec(bNode *node)
{
  ntreeCompositTagNeedExec(node);
}

static void rna_Node_tex_image_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();

  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_IMAGE, nullptr);
}

static void rna_NodeGroup_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();

  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  DEG_relations_tag_update(bmain);
}

static void rna_NodeGroup_node_tree_set(PointerRNA *ptr,
                                        const PointerRNA value,
                                        ReportList * /*reports*/)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();
  bNodeTree *ngroup = static_cast<bNodeTree *>(value.data);

  const char *disabled_hint = nullptr;
  if (blender::bke::node_group_poll(ntree, ngroup, &disabled_hint)) {
    if (node->id) {
      id_us_min(node->id);
    }
    if (ngroup) {
      id_us_plus(&ngroup->id);
    }

    node->id = &ngroup->id;
  }
}

static bool rna_NodeGroup_node_tree_poll(PointerRNA *ptr, const PointerRNA value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeTree *ngroup = static_cast<bNodeTree *>(value.data);

  /* only allow node trees of the same type as the group node's tree */
  if (ngroup->type != ntree->type) {
    return false;
  }

  const char *disabled_hint = nullptr;
  return blender::bke::node_group_poll(ntree, ngroup, &disabled_hint);
}

static void rna_Node_scene_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  bNode *node = ptr->data_as<bNode>();

  if (node->id) {
    id_us_min(node->id);
    node->id = nullptr;
  }

  node->id = static_cast<ID *>(value.data);

  id_us_plus(node->id);
}

static void rna_Node_image_layer_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  Image *ima = reinterpret_cast<Image *>(node->id);
  ImageUser *iuser = static_cast<ImageUser *>(node->storage);

  if (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
      node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE)
  {
    return;
  }

  BKE_image_multilayer_index(ima->rr, iuser);
  BKE_image_signal(bmain, ima, iuser, IMA_SIGNAL_SRC_CHANGE);

  rna_Node_update(bmain, scene, ptr);

  if (scene != nullptr && scene->compositing_node_group != nullptr) {
    ntreeCompositUpdateRLayers(scene->compositing_node_group);
  }
}

static const EnumPropertyItem *renderresult_layers_add_enum(RenderLayer *rl)
{
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0};
  int i = 0, totitem = 0;

  while (rl) {
    tmp.identifier = rl->name;
    /* Little trick: using space char instead empty string
     * makes the item selectable in the drop-down. */
    if (rl->name[0] == '\0') {
      tmp.name = " ";
    }
    else {
      tmp.name = rl->name;
    }
    tmp.value = i++;
    RNA_enum_item_add(&item, &totitem, &tmp);
    rl = rl->next;
  }

  RNA_enum_item_end(&item, &totitem);

  return item;
}

static const EnumPropertyItem *rna_ShaderNodeMix_data_type_itemf(bContext * /*C*/,
                                                                 PointerRNA *ptr,
                                                                 PropertyRNA * /*prop*/,
                                                                 bool *r_free)
{
  *r_free = true;

  const auto rotation_supported_mix = [&](const EnumPropertyItem *item) -> bool {
    const eNodeSocketDatatype data_type = eNodeSocketDatatype(item->value);
    if (data_type == SOCK_ROTATION) {
      const bNodeTree *tree = reinterpret_cast<const bNodeTree *>(ptr->owner_id);
      if (tree->type == NTREE_GEOMETRY) {
        return true;
      }
    }
    return ELEM(data_type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA);
  };

  return itemf_function_check(rna_enum_mix_data_type_items, rotation_supported_mix);
}

static const EnumPropertyItem *rna_Node_image_layer_itemf(bContext * /*C*/,
                                                          PointerRNA *ptr,
                                                          PropertyRNA * /*prop*/,
                                                          bool *r_free)
{
  bNode *node = ptr->data_as<bNode>();
  Image *ima = reinterpret_cast<Image *>(node->id);
  const EnumPropertyItem *item = nullptr;
  RenderLayer *rl;

  if (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
      node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE)
  {
    return rna_enum_dummy_NULL_items;
  }

  if (ima == nullptr || ima->rr == nullptr) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }

  rl = static_cast<RenderLayer *>(ima->rr->layers.first);
  item = renderresult_layers_add_enum(rl);

  *r_free = true;

  return item;
}

static bool rna_Node_image_has_layers_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  Image *ima = reinterpret_cast<Image *>(node->id);

  if (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
      node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE)
  {
    return false;
  }

  if (!ima || !(ima->rr)) {
    return false;
  }

  return RE_layers_have_name(ima->rr);
}

static bool rna_Node_image_has_views_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  Image *ima = reinterpret_cast<Image *>(node->id);

  if (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
      node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE)
  {
    return false;
  }

  if (!ima || !(ima->rr)) {
    return false;
  }

  return BLI_listbase_count_at_most(&ima->rr->views, 2) > 1;
}

static const EnumPropertyItem *renderresult_views_add_enum(RenderView *rv)
{
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0, "ALL", 0, "All", ""};
  int i = 1, totitem = 0;

  /* option to use all views */
  RNA_enum_item_add(&item, &totitem, &tmp);

  while (rv) {
    tmp.identifier = rv->name;
    /* Little trick: using space char instead empty string
     * makes the item selectable in the drop-down. */
    if (rv->name[0] == '\0') {
      tmp.name = " ";
    }
    else {
      tmp.name = rv->name;
    }
    tmp.value = i++;
    RNA_enum_item_add(&item, &totitem, &tmp);
    rv = rv->next;
  }

  RNA_enum_item_end(&item, &totitem);

  return item;
}

static const EnumPropertyItem *rna_Node_image_view_itemf(bContext * /*C*/,
                                                         PointerRNA *ptr,
                                                         PropertyRNA * /*prop*/,
                                                         bool *r_free)
{
  bNode *node = ptr->data_as<bNode>();
  Image *ima = reinterpret_cast<Image *>(node->id);
  const EnumPropertyItem *item = nullptr;
  RenderView *rv;

  if (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
      node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE)
  {
    return rna_enum_dummy_NULL_items;
  }

  if (ima == nullptr || ima->rr == nullptr) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }

  rv = static_cast<RenderView *>(ima->rr->views.first);
  item = renderresult_views_add_enum(rv);

  *r_free = true;

  return item;
}

static const EnumPropertyItem *rna_Node_view_layer_itemf(bContext * /*C*/,
                                                         PointerRNA *ptr,
                                                         PropertyRNA * /*prop*/,
                                                         bool *r_free)
{
  bNode *node = ptr->data_as<bNode>();
  Scene *sce = reinterpret_cast<Scene *>(node->id);
  const EnumPropertyItem *item = nullptr;
  RenderLayer *rl;

  if (sce == nullptr) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }

  rl = static_cast<RenderLayer *>(sce->view_layers.first);
  item = renderresult_layers_add_enum(rl);

  *r_free = true;

  return item;
}

static void rna_Node_view_layer_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Node_update_relations(bmain, scene, ptr);
  if (scene != nullptr && scene->compositing_node_group != nullptr) {
    ntreeCompositUpdateRLayers(scene->compositing_node_group);
  }
}

static void rna_Image_Node_update_id(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();

  blender::bke::node_tag_update_id(*node);
  rna_Node_update_relations(bmain, scene, ptr);
}

/* --------------------------------------------------------------------
 * White Balance Node.
 */

static void rna_NodeColorBalance_input_whitepoint_get(PointerRNA *ptr, float value[3])
{
  bNode *node = ptr->data_as<bNode>();
  bNodeSocket *temperature_input = blender::bke::node_find_socket(
      *node, SOCK_IN, "Input Temperature");
  bNodeSocket *tint_input = blender::bke::node_find_socket(*node, SOCK_IN, "Input Tint");
  IMB_colormanagement_get_whitepoint(
      temperature_input->default_value_typed<bNodeSocketValueFloat>()->value,
      tint_input->default_value_typed<bNodeSocketValueFloat>()->value,
      value);
}

static void rna_NodeColorBalance_input_whitepoint_set(PointerRNA *ptr, const float value[3])
{
  bNode *node = ptr->data_as<bNode>();
  bNodeSocket *temperature_input = blender::bke::node_find_socket(
      *node, SOCK_IN, "Input Temperature");
  bNodeSocket *tint_input = blender::bke::node_find_socket(*node, SOCK_IN, "Input Tint");
  IMB_colormanagement_set_whitepoint(
      value,
      temperature_input->default_value_typed<bNodeSocketValueFloat>()->value,
      tint_input->default_value_typed<bNodeSocketValueFloat>()->value);
}

static void rna_NodeColorBalance_output_whitepoint_get(PointerRNA *ptr, float value[3])
{
  bNode *node = ptr->data_as<bNode>();
  bNodeSocket *temperature_input = blender::bke::node_find_socket(
      *node, SOCK_IN, "Output Temperature");
  bNodeSocket *tint_input = blender::bke::node_find_socket(*node, SOCK_IN, "Output Tint");
  IMB_colormanagement_get_whitepoint(
      temperature_input->default_value_typed<bNodeSocketValueFloat>()->value,
      tint_input->default_value_typed<bNodeSocketValueFloat>()->value,
      value);
}

static void rna_NodeColorBalance_output_whitepoint_set(PointerRNA *ptr, const float value[3])
{
  bNode *node = ptr->data_as<bNode>();
  bNodeSocket *temperature_input = blender::bke::node_find_socket(
      *node, SOCK_IN, "Output Temperature");
  bNodeSocket *tint_input = blender::bke::node_find_socket(*node, SOCK_IN, "Output Tint");
  IMB_colormanagement_set_whitepoint(
      value,
      temperature_input->default_value_typed<bNodeSocketValueFloat>()->value,
      tint_input->default_value_typed<bNodeSocketValueFloat>()->value);
}

static void rna_NodeCryptomatte_source_set(PointerRNA *ptr, int value)
{
  bNode *node = ptr->data_as<bNode>();
  if (node->id && node->custom1 != value) {
    id_us_min(node->id);
    node->id = nullptr;
  }
  node->custom1 = value;
}

static int rna_NodeCryptomatte_layer_name_get(PointerRNA *ptr)
{
  int index = 0;
  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *storage = static_cast<NodeCryptomatte *>(node->storage);
  LISTBASE_FOREACH_INDEX (CryptomatteLayer *, layer, &storage->runtime.layers, index) {
    if (STREQLEN(storage->layer_name, layer->name, sizeof(storage->layer_name))) {
      return index;
    }
  }
  return 0;
}

static void rna_NodeCryptomatte_layer_name_set(PointerRNA *ptr, int new_value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *storage = static_cast<NodeCryptomatte *>(node->storage);

  CryptomatteLayer *layer = static_cast<CryptomatteLayer *>(
      BLI_findlink(&storage->runtime.layers, new_value));
  if (layer) {
    STRNCPY(storage->layer_name, layer->name);
  }
}

static const EnumPropertyItem *rna_NodeCryptomatte_layer_name_itemf(bContext * /*C*/,
                                                                    PointerRNA *ptr,
                                                                    PropertyRNA * /*prop*/,
                                                                    bool *r_free)
{
  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *storage = static_cast<NodeCryptomatte *>(node->storage);
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem temp = {0, "", 0, "", ""};
  int totitem = 0;

  int layer_index;
  LISTBASE_FOREACH_INDEX (CryptomatteLayer *, layer, &storage->runtime.layers, layer_index) {
    temp.value = layer_index;
    temp.identifier = layer->name;
    temp.name = layer->name;
    RNA_enum_item_add(&item, &totitem, &temp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static PointerRNA rna_NodeCryptomatte_scene_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();

  ID *scene = (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER) ? node->id : nullptr;
  return RNA_id_pointer_create(scene);
}

static void rna_NodeCryptomatte_scene_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  bNode *node = ptr->data_as<bNode>();

  if (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER) {
    rna_Node_scene_set(ptr, value, reports);
  }
}

static PointerRNA rna_NodeCryptomatte_image_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();

  ID *image = (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) ? node->id : nullptr;
  return RNA_id_pointer_create(image);
}

static void rna_NodeCryptomatte_image_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          ReportList * /*reports*/)
{
  bNode *node = ptr->data_as<bNode>();

  if (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) {
    if (node->id) {
      id_us_min(node->id);
    }
    if (value.data) {
      id_us_plus(static_cast<ID *>(value.data));
    }
    node->id = static_cast<ID *>(value.data);
  }
}

static bool rna_NodeCryptomatte_image_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  Image *image = reinterpret_cast<Image *>(value.owner_id);
  return image->type == IMA_TYPE_MULTILAYER;
}

static void rna_NodeCryptomatte_matte_get(PointerRNA *ptr, char *value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
  char *matte_id = BKE_cryptomatte_entries_to_matte_id(nc);
  strcpy(value, matte_id);
  MEM_freeN(matte_id);
}

static int rna_NodeCryptomatte_matte_length(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
  char *matte_id = BKE_cryptomatte_entries_to_matte_id(nc);
  int result = strlen(matte_id);
  MEM_freeN(matte_id);
  return result;
}

static void rna_NodeCryptomatte_matte_set(PointerRNA *ptr, const char *value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
  BKE_cryptomatte_matte_id_to_entries(nc, value);
}

static void rna_NodeCryptomatte_update_add(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ntreeCompositCryptomatteSyncFromAdd(ptr->data_as<bNode>());
  rna_Node_update(bmain, scene, ptr);
}

static void rna_NodeCryptomatte_update_remove(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ntreeCompositCryptomatteSyncFromRemove(ptr->data_as<bNode>());
  rna_Node_update(bmain, scene, ptr);
}

static PointerRNA rna_Node_paired_output_get(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();
  const blender::bke::bNodeZoneType &zone_type = *blender::bke::zone_type_by_node_type(
      node->type_legacy);
  bNode *output_node = zone_type.get_corresponding_output(*ntree, *node);
  PointerRNA ptr_result = RNA_pointer_create_discrete(&ntree->id, &RNA_Node, output_node);
  return ptr_result;
}

static bool rna_Node_pair_with_output(
    ID *id, bNode *node, bContext *C, ReportList *reports, bNode *output_node)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  const blender::bke::bNodeZoneType &zone_type = *blender::bke::zone_type_by_node_type(
      node->type_legacy);
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

template<typename Accessor>
static void rna_Node_ItemArray_remove(ID *id,
                                      bNode *node,
                                      Main *bmain,
                                      ReportList *reports,
                                      typename Accessor::ItemT *item_to_remove)
{
  blender::nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  if (item_to_remove < *ref.items || item_to_remove >= *ref.items + *ref.items_num) {
    if constexpr (Accessor::has_name) {
      char **name_ptr = Accessor::get_name(*item_to_remove);
      if (name_ptr && *name_ptr) {
        BKE_reportf(reports, RPT_ERROR, "Unable to locate item '%s' in node", *name_ptr);
        return;
      }
    }
    else {
      BKE_report(reports, RPT_ERROR, "Unable to locate item in node");
    }
    return;
  }
  const int remove_index = item_to_remove - *ref.items;
  blender::dna::array::remove_index(
      ref.items, ref.items_num, ref.active_index, remove_index, Accessor::destruct_item);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

template<typename Accessor> static void rna_Node_ItemArray_clear(ID *id, bNode *node, Main *bmain)
{
  blender::nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  blender::dna::array::clear(ref.items, ref.items_num, ref.active_index, Accessor::destruct_item);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

template<typename Accessor>
static void rna_Node_ItemArray_move(
    ID *id, bNode *node, Main *bmain, const int from_index, const int to_index)
{
  blender::nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  const int items_num = *ref.items_num;
  if (from_index < 0 || to_index < 0 || from_index >= items_num || to_index >= items_num) {
    return;
  }
  blender::dna::array::move_index(*ref.items, items_num, from_index, to_index);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

template<typename Accessor> static PointerRNA rna_Node_ItemArray_active_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  blender::nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  typename Accessor::ItemT *active_item = nullptr;
  const int active_index = *ref.active_index;
  const int items_num = *ref.items_num;
  if (active_index >= 0 && active_index < items_num) {
    active_item = &(*ref.items)[active_index];
  }
  return RNA_pointer_create_discrete(ptr->owner_id, Accessor::item_srna, active_item);
}
template<typename Accessor>
static void rna_Node_ItemArray_active_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          ReportList * /*reports*/)
{
  using ItemT = typename Accessor::ItemT;
  bNode *node = ptr->data_as<bNode>();
  ItemT *item = static_cast<ItemT *>(value.data);

  blender::nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  if (item >= *ref.items && item < *ref.items + *ref.items_num) {
    *ref.active_index = item - *ref.items;
  }
}

template<typename Accessor>
static void rna_Node_ItemArray_item_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  using ItemT = typename Accessor::ItemT;
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  ItemT &item = *static_cast<ItemT *>(ptr->data);
  bNode *node = blender::nodes::socket_items::find_node_by_item<Accessor>(ntree, item);
  BLI_assert(node != nullptr);

  BKE_ntree_update_tag_node_property(&ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree.id);
}

template<typename Accessor>
static const EnumPropertyItem *rna_Node_ItemArray_socket_type_itemf(bContext * /*C*/,
                                                                    PointerRNA *ptr,
                                                                    PropertyRNA * /*prop*/,
                                                                    bool *r_free)
{
  *r_free = true;
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  return itemf_function_check(
      rna_enum_node_socket_data_type_items, [&](const EnumPropertyItem *item) {
        return Accessor::supports_socket_type(eNodeSocketDatatype(item->value), ntree.type);
      });
}

template<typename Accessor>
static void rna_Node_ItemArray_item_name_set(PointerRNA *ptr, const char *value)
{
  using ItemT = typename Accessor::ItemT;
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  ItemT &item = *static_cast<ItemT *>(ptr->data);
  bNode *node = blender::nodes::socket_items::find_node_by_item<Accessor>(ntree, item);
  BLI_assert(node != nullptr);
  blender::nodes::socket_items::set_item_name_and_make_unique<Accessor>(*node, item, value);
}

template<typename Accessors>
static void rna_Node_ItemArray_item_color_get(PointerRNA *ptr, float *values)
{
  using ItemT = typename Accessors::ItemT;
  ItemT &item = *static_cast<ItemT *>(ptr->data);
  const blender::StringRefNull socket_type_idname = *blender::bke::node_static_socket_type(
      Accessors::get_socket_type(item), 0);
  ED_node_type_draw_color(socket_type_idname.c_str(), values);
}

template<typename Accessor>
typename Accessor::ItemT *rna_Node_ItemArray_new_with_socket_and_name(
    ID *id, bNode *node, Main *bmain, ReportList *reports, int socket_type, const char *name)
{
  using ItemT = typename Accessor::ItemT;

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  if (!Accessor::supports_socket_type(eNodeSocketDatatype(socket_type), ntree->type)) {
    BKE_report(reports, RPT_ERROR, "Unable to create item with this socket type");
    return nullptr;
  }
  ItemT *new_item = blender::nodes::socket_items::add_item_with_socket_type_and_name<Accessor>(
      *ntree, *node, eNodeSocketDatatype(socket_type), name);

  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

  return new_item;
}

template<typename Accessor>
static const EnumPropertyItem *rna_Node_ItemArray_structure_type_itemf(bContext * /*C*/,
                                                                       PointerRNA *ptr,
                                                                       PropertyRNA * /*prop*/,
                                                                       bool *r_free)
{
  using ItemT = typename Accessor::ItemT;

  const bNodeTree *ntree = reinterpret_cast<const bNodeTree *>(ptr->owner_id);
  const ItemT &item = *static_cast<const ItemT *>(ptr->data);
  const eNodeSocketDatatype socket_type = Accessor::get_socket_type(item);
  return rna_NodeSocket_structure_type_item_filter(ntree, socket_type, r_free);
}

static IndexSwitchItem *rna_NodeIndexSwitchItems_new(ID *id, bNode *node, Main *bmain)
{
  IndexSwitchItem *new_item = blender::nodes::socket_items::add_item<IndexSwitchItemsAccessor>(
      *node);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

  return new_item;
}

/* The same as #grid_socket_type_items_filter_fn. */
static const EnumPropertyItem *rna_NodeFieldToGridItem_data_type_itemf(bContext * /*C*/,
                                                                       PointerRNA * /*ptr*/,
                                                                       PropertyRNA * /*prop*/,
                                                                       bool *r_free)
{
  *r_free = true;
  return itemf_function_check(
      rna_enum_node_socket_data_type_items, [](const EnumPropertyItem *item) {
        return blender::nodes::socket_type_supports_grids(eNodeSocketDatatype(item->value));
      });
}

static const EnumPropertyItem *rna_NodeGeometryCaptureAttributeItem_data_type_itemf(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free)
{
  *r_free = true;
  /* See #attribute_type_type_with_socket_fn. */
  return itemf_function_check(rna_enum_attribute_type_items, [](const EnumPropertyItem *item) {
    return ELEM(item->value,
                CD_PROP_FLOAT,
                CD_PROP_FLOAT3,
                CD_PROP_COLOR,
                CD_PROP_BOOL,
                CD_PROP_INT32,
                CD_PROP_QUATERNION,
                CD_PROP_FLOAT4X4);
  });
}

/* ******** Node Socket Types ******** */

static void rna_FrameNode_label_size_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  BLF_cache_clear();
  rna_Node_update(bmain, scene, ptr);
}

static void rna_ShaderNodeTexIES_mode_set(PointerRNA *ptr, int value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeShaderTexIES *nss = static_cast<NodeShaderTexIES *>(node->storage);

  if (nss->mode != value) {
    nss->mode = value;
    nss->filepath[0] = '\0';

    /* replace text datablock by filepath */
    if (node->id) {
      Text *text = reinterpret_cast<Text *>(node->id);

      if (value == NODE_IES_EXTERNAL && text->filepath) {
        STRNCPY(nss->filepath, text->filepath);
        BLI_path_abs(nss->filepath, ID_BLEND_PATH_FROM_GLOBAL(&text->id));
        BLI_path_rel(nss->filepath, ID_BLEND_PATH_FROM_GLOBAL(ptr->owner_id));
      }

      id_us_min(node->id);
      node->id = nullptr;
    }
  }
}

static void rna_ShaderNodeScript_mode_set(PointerRNA *ptr, int value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);

  if (nss->mode != value) {
    nss->mode = value;
    nss->filepath[0] = '\0';
    nss->flag &= ~NODE_SCRIPT_AUTO_UPDATE;

    /* replace text data-block by filepath */
    if (node->id) {
      Text *text = reinterpret_cast<Text *>(node->id);

      if (value == NODE_SCRIPT_EXTERNAL && text->filepath) {
        STRNCPY(nss->filepath, text->filepath);
        BLI_path_abs(nss->filepath, ID_BLEND_PATH_FROM_GLOBAL(&text->id));
        BLI_path_rel(nss->filepath, ID_BLEND_PATH_FROM_GLOBAL(ptr->owner_id));
      }

      id_us_min(node->id);
      node->id = nullptr;
    }

    /* remove any bytecode */
    if (nss->bytecode) {
      MEM_freeN(nss->bytecode);
      nss->bytecode = nullptr;
    }

    nss->bytecode_hash[0] = '\0';
  }
}

static void rna_ShaderNodeScript_bytecode_get(PointerRNA *ptr, char *value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);

  strcpy(value, (nss->bytecode) ? nss->bytecode : "");
}

static int rna_ShaderNodeScript_bytecode_length(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);

  return (nss->bytecode) ? strlen(nss->bytecode) : 0;
}

static void rna_ShaderNodeScript_bytecode_set(PointerRNA *ptr, const char *value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);

  if (nss->bytecode) {
    MEM_freeN(nss->bytecode);
  }

  if (value && value[0]) {
    nss->bytecode = BLI_strdup(value);
  }
  else {
    nss->bytecode = nullptr;
  }
}

static void rna_ShaderNodeScript_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();
  RenderEngineType *engine_type = (scene != nullptr) ? RE_engines_find(scene->r.engine) : nullptr;

  if (engine_type && engine_type->update_script_node) {
    /* auto update node */
    RenderEngine *engine = RE_engine_create(engine_type);
    engine_type->update_script_node(engine, ntree, node);
    RE_engine_free(engine);
  }

  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
}

static void rna_ShaderNode_socket_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Node_update(bmain, scene, ptr);
}

void rna_Node_socket_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Node_update(bmain, scene, ptr);
}

static void rna_ShaderNode_is_active_output_set(PointerRNA *ptr, bool value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = ptr->data_as<bNode>();
  if (value) {
    /* If this node becomes the active output, the others of the same type can't be the active
     * output anymore. */
    for (bNode *other_node : ntree->all_nodes()) {
      if (other_node->type_legacy == node->type_legacy) {
        other_node->flag &= ~NODE_DO_OUTPUT;
      }
    }
    node->flag |= NODE_DO_OUTPUT;
  }
  else {
    node->flag &= ~NODE_DO_OUTPUT;
  }
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

static int rna_NodeConvertColorSpace_from_color_space_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  return IMB_colormanagement_colorspace_get_named_index(node_storage->from_color_space);
}

static void rna_NodeConvertColorSpace_from_color_space_set(PointerRNA *ptr, int value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  const char *name = IMB_colormanagement_colorspace_get_indexed_name(value);

  if (name && name[0]) {
    STRNCPY_UTF8(node_storage->from_color_space, name);
  }
}
static int rna_NodeConvertColorSpace_to_color_space_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  return IMB_colormanagement_colorspace_get_named_index(node_storage->to_color_space);
}

static void rna_NodeConvertColorSpace_to_color_space_set(PointerRNA *ptr, int value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  const char *name = IMB_colormanagement_colorspace_get_indexed_name(value);

  if (name && name[0]) {
    STRNCPY_UTF8(node_storage->to_color_space, name);
  }
}

static void rna_reroute_node_socket_type_set(PointerRNA *ptr, const char *value)
{
  const bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  blender::bke::bNodeTreeType *ntree_type = ntree.typeinfo;

  bNode &node = *ptr->data_as<bNode>();

  if (value == nullptr) {
    return;
  }
  blender::bke::bNodeSocketType *socket_type = blender::bke::node_socket_type_find(value);
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

static const EnumPropertyItem *rna_NodeConvertColorSpace_color_space_itemf(bContext * /*C*/,
                                                                           PointerRNA * /*ptr*/,
                                                                           PropertyRNA * /*prop*/,
                                                                           bool *r_free)
{
  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  IMB_colormanagement_colorspace_items_add(&items, &totitem);
  RNA_enum_item_end(&items, &totitem);

  *r_free = true;

  return items;
}

static NodeEnumItem *rna_NodeMenuSwitchItems_new(ID *id,
                                                 bNode *node,
                                                 Main *bmain,
                                                 const char *name)
{
  NodeEnumItem *new_item =
      blender::nodes::socket_items::add_item_with_name<MenuSwitchItemsAccessor>(*node, name);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

  return new_item;
}

static PointerRNA rna_NodeMenuSwitch_enum_definition_get(PointerRNA *ptr)
{
  /* Return node itself. The data is now directly available on the node and does not have to be
   * accessed through "enum_definition". */
  return *ptr;
}

#else

static const EnumPropertyItem prop_image_layer_items[] = {
    {0, "PLACEHOLDER", 0, "Placeholder", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem prop_image_view_items[] = {
    {0, "ALL", 0, "All", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem prop_view_layer_items[] = {
    {0, "PLACEHOLDER", 0, "Placeholder", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_ycc_items[] = {
    {0, "ITUBT601", 0, "ITU 601", ""},
    {1, "ITUBT709", 0, "ITU 709", ""},
    {2, "JFIF", 0, "JPEG", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_metallic_distribution_items[] = {
    {SHD_GLOSSY_BECKMANN, "BECKMANN", 0, "Beckmann", ""},
    {SHD_GLOSSY_GGX, "GGX", 0, "GGX", ""},
    {SHD_GLOSSY_MULTI_GGX,
     "MULTI_GGX",
     0,
     "Multiscatter GGX",
     "GGX with additional correction to account for multiple scattering, preserve energy and "
     "prevent unexpected darkening at high roughness"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_metallic_fresnel_type_items[] = {
    {SHD_PHYSICAL_CONDUCTOR,
     "PHYSICAL_CONDUCTOR",
     0,
     "Physical Conductor",
     "Fresnel conductor based on the complex refractive index per color channel"},
    {SHD_CONDUCTOR_F82,
     "F82",
     0,
     "F82 Tint",
     "An approximation of the Fresnel conductor curve based on the colors at perpendicular and "
     "near-grazing (roughly 82°) angles"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_glossy_items[] = {
    {SHD_GLOSSY_BECKMANN, "BECKMANN", 0, "Beckmann", ""},
    {SHD_GLOSSY_GGX, "GGX", 0, "GGX", ""},
    {SHD_GLOSSY_ASHIKHMIN_SHIRLEY, "ASHIKHMIN_SHIRLEY", 0, "Ashikhmin-Shirley", ""},
    {SHD_GLOSSY_MULTI_GGX,
     "MULTI_GGX",
     0,
     "Multiscatter GGX",
     "GGX with additional correction to account for multiple scattering, preserve energy and "
     "prevent unexpected darkening at high roughness"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_glass_items[] = {
    {SHD_GLOSSY_BECKMANN, "BECKMANN", 0, "Beckmann", ""},
    {SHD_GLOSSY_GGX, "GGX", 0, "GGX", ""},
    {SHD_GLOSSY_MULTI_GGX,
     "MULTI_GGX",
     0,
     "Multiscatter GGX",
     "GGX with additional correction to account for multiple scattering, preserve energy and "
     "prevent unexpected darkening at high roughness"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_refraction_items[] = {
    {SHD_GLOSSY_BECKMANN, "BECKMANN", 0, "Beckmann", ""},
    {SHD_GLOSSY_GGX, "GGX", 0, "GGX", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_sheen_items[] = {
    {SHD_SHEEN_ASHIKHMIN, "ASHIKHMIN", 0, "Ashikhmin", "Classic Ashikhmin velvet (legacy model)"},
    {SHD_SHEEN_MICROFIBER,
     "MICROFIBER",
     0,
     "Microfiber",
     "Microflake-based model of multiple scattering between normal-oriented fibers"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_toon_items[] = {
    {SHD_TOON_DIFFUSE, "DIFFUSE", 0, "Diffuse", "Use diffuse BSDF"},
    {SHD_TOON_GLOSSY, "GLOSSY", 0, "Glossy", "Use glossy BSDF"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_hair_items[] = {
    {SHD_HAIR_REFLECTION,
     "Reflection",
     0,
     "Reflection",
     "The light that bounces off the surface of the hair"},
    {SHD_HAIR_TRANSMISSION,
     "Transmission",
     0,
     "Transmission",
     "The light that passes through the hair and exits on the other side"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_principled_hair_model_items[] = {
    {SHD_PRINCIPLED_HAIR_CHIANG,
     "CHIANG",
     0,
     "Chiang",
     "Near-field hair scattering model by Chiang et al. 2016, suitable for close-up looks, but is "
     "more noisy when viewing from a distance."},
    {SHD_PRINCIPLED_HAIR_HUANG,
     "HUANG",
     0,
     "Huang",
     "Multi-scale hair scattering model by Huang et al. 2022, suitable for viewing both up close "
     "and from a distance, supports elliptical cross-sections and has more precise highlight in "
     "forward scattering directions."},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_principled_hair_parametrization_items[] = {
    {SHD_PRINCIPLED_HAIR_DIRECT_ABSORPTION,
     "ABSORPTION",
     0,
     "Absorption Coefficient",
     "Directly set the absorption coefficient \"sigma_a\" (this is not the most intuitive way to "
     "color hair)"},
    {SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION,
     "MELANIN",
     0,
     "Melanin Concentration",
     "Define the melanin concentrations below to get the most realistic-looking hair (you can get "
     "the concentrations for different types of hair online)"},
    {SHD_PRINCIPLED_HAIR_REFLECTANCE,
     "COLOR",
     0,
     "Direct Coloring",
     "Choose the color of your preference, and the shader will approximate the absorption "
     "coefficient to render lookalike hair"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_script_mode_items[] = {
    {NODE_SCRIPT_INTERNAL, "INTERNAL", 0, "Internal", "Use internal text data-block"},
    {NODE_SCRIPT_EXTERNAL, "EXTERNAL", 0, "External", "Use external .osl or .oso file"},
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem node_ies_mode_items[] = {
    {NODE_IES_INTERNAL, "INTERNAL", 0, "Internal", "Use internal text data-block"},
    {NODE_IES_EXTERNAL, "EXTERNAL", 0, "External", "Use external .ies file"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_principled_distribution_items[] = {
    {SHD_GLOSSY_GGX, "GGX", 0, "GGX", ""},
    {SHD_GLOSSY_MULTI_GGX,
     "MULTI_GGX",
     0,
     "Multiscatter GGX",
     "GGX with additional correction to account for multiple scattering, preserve energy and "
     "prevent unexpected darkening at high roughness"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_subsurface_method_items[] = {
    {SHD_SUBSURFACE_BURLEY,
     "BURLEY",
     0,
     "Christensen-Burley",
     "Approximation to physically based volume scattering"},
    {SHD_SUBSURFACE_RANDOM_WALK,
     "RANDOM_WALK",
     0,
     "Random Walk",
     "Volumetric approximation to physically based volume scattering, using the scattering radius "
     "as specified"},
    {SHD_SUBSURFACE_RANDOM_WALK_SKIN,
     "RANDOM_WALK_SKIN",
     0,
     "Random Walk (Skin)",
     "Volumetric approximation to physically based volume scattering, with scattering radius "
     "automatically adjusted to match color textures. Designed for skin shading."},
    {0, nullptr, 0, nullptr, nullptr}};

static const EnumPropertyItem prop_image_extension[] = {
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

static const EnumPropertyItem node_scatter_phase_items[] = {
    {SHD_PHASE_HENYEY_GREENSTEIN,
     "HENYEY_GREENSTEIN",
     0,
     "Henyey-Greenstein",
     "Henyey-Greenstein, default phase function for the scattering of light"},
    {SHD_PHASE_FOURNIER_FORAND,
     "FOURNIER_FORAND",
     0,
     "Fournier-Forand",
     "Fournier-Forand phase function, used for the scattering of light in underwater "
     "environments"},
    {SHD_PHASE_DRAINE,
     "DRAINE",
     0,
     "Draine",
     "Draine phase functions, mostly used for the scattering of light in interstellar dust"},
    {SHD_PHASE_RAYLEIGH,
     "RAYLEIGH",
     0,
     "Rayleigh",
     "Rayleigh phase function, mostly used for particles smaller than the wavelength of light, "
     "such as scattering of sunlight in earth's atmosphere"},
    {SHD_PHASE_MIE,
     "MIE",
     0,
     "Mie",
     "Approximation of Mie scattering in water droplets, used for scattering in clouds and fog"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rna_def_node_item_array_socket_item_common(
    StructRNA *srna,
    const char *accessor,
    const bool add_socket_type,
    const bool add_vector_socket_dimensions = false)
{
  static blender::LinearAllocator<> allocator;
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

static void rna_def_node_item_array_common_functions(StructRNA *srna,
                                                     const char *item_name,
                                                     const char *accessor_name)
{
  static blender::LinearAllocator<> allocator;
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

static void rna_def_node_item_array_new_with_socket_and_name(StructRNA *srna,
                                                             const char *item_name,
                                                             const char *accessor_name)
{
  static blender::LinearAllocator<> allocator;
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

static void def_group(BlenderRNA * /*brna*/, StructRNA *srna)
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
  RNA_def_property_ui_text(prop, "Shrink", "Shrink the frame to minimal bounding box");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "label_size", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "label_size");
  RNA_def_property_range(prop, 8, 64);
  RNA_def_property_ui_text(prop, "Label Font Size", "Font size to use for displaying the label");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_FrameNode_label_size_update");
}

static void def_clamp(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clamp_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_clamp_items);
  RNA_def_property_ui_text(prop, "Clamp Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_map_range(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem rna_enum_data_type_items[] = {
      {CD_PROP_FLOAT, "FLOAT", ICON_NODE_SOCKET_FLOAT, "Float", "Floating-point value"},
      {CD_PROP_FLOAT3,
       "FLOAT_VECTOR",
       ICON_NODE_SOCKET_VECTOR,
       "Vector",
       "3D vector with floating-point values"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_struct_sdna_from(srna, "NodeMapRange", "storage");

  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clamp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "clamp", 1);
  RNA_def_property_ui_text(prop, "Clamp", "Clamp the result to the target range [To Min, To Max]");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "interpolation_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "interpolation_type");
  RNA_def_property_enum_items(prop, rna_enum_node_map_range_items);
  RNA_def_property_ui_text(prop, "Interpolation Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "data_type");
  RNA_def_property_enum_items(prop, rna_enum_data_type_items);
  RNA_def_property_ui_text(prop, "Data Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_math(BlenderRNA * /*brna*/, StructRNA *srna)
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

static void def_sh_mix(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem rna_enum_mix_mode_items[] = {
      {NODE_MIX_MODE_UNIFORM, "UNIFORM", 0, "Uniform", "Use a single factor for all components"},
      {NODE_MIX_MODE_NON_UNIFORM, "NON_UNIFORM", 0, "Non-Uniform", "Per component factor"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeShaderMix", "storage");

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_ShaderNodeMix_data_type_itemf");
  RNA_def_property_enum_items(prop, rna_enum_mix_data_type_items);
  RNA_def_property_enum_default(prop, SOCK_FLOAT);
  RNA_def_property_ui_text(prop, "Data Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "factor_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_mix_mode_items);
  RNA_def_property_enum_default(prop, NODE_MIX_MODE_UNIFORM);
  RNA_def_property_ui_text(prop, "Factor Mode", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "blend_type");
  RNA_def_property_enum_items(prop, rna_enum_ramp_blend_items);
  RNA_def_property_ui_text(prop, "Blending Mode", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "clamp_factor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "clamp_factor", 1);
  RNA_def_property_ui_text(prop, "Clamp Factor", "Clamp the factor to [0,1] range");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "clamp_result", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "clamp_result", 1);
  RNA_def_property_ui_text(prop, "Clamp Result", "Clamp the result to [0,1] range");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

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

static void def_vector_math(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_vec_math_items);
  RNA_def_property_ui_text(prop, "Operation", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_NODETREE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
}

static void def_rgb_curve(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_vector_curve(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_float_curve(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_time(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_colorramp(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "ColorRamp");
  RNA_def_property_ui_text(prop, "Color Ramp", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_mix_rgb(BlenderRNA * /*brna*/, StructRNA *srna)
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

static void def_texture(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Texture");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Texture", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  prop = RNA_def_property(srna, "node_output", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_ui_text(
      prop, "Node Output", "For node-based textures, which output node to use");
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
  RNA_def_property_int_default(prop, 1);
  RNA_def_property_ui_text(prop, "Integer", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
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
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_sdna(prop, nullptr, "vector");
  RNA_def_property_ui_text(prop, "Vector", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_string(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputString", "storage");

  prop = RNA_def_property(srna, "string", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "String", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* -- Shader Nodes ---------------------------------------------------------- */

static void def_sh_output(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "is_active_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_DO_OUTPUT);
  RNA_def_property_ui_text(
      prop, "Active Output", "True if this node is used as the active output");
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_ShaderNode_is_active_output_set");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "target", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, prop_shader_output_target_items);
  RNA_def_property_ui_text(
      prop, "Target", "Which renderer and viewport shading types to use the shaders for");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_output_linestyle(BlenderRNA *brna, StructRNA *srna)
{
  def_sh_output(brna, srna);
  def_mix_rgb(brna, srna);
}

static void def_sh_mapping(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "vector_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_mapping_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of vector that the mapping transforms");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_sh_vector_rotate(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "rotation_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_vector_rotate_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of angle input");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");

  prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom2", 0);
  RNA_def_property_ui_text(prop, "Invert", "Invert the rotation angle");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_attribute(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem prop_attribute_type[] = {
      {SHD_ATTRIBUTE_GEOMETRY,
       "GEOMETRY",
       0,
       "Geometry",
       "The attribute is associated with the object geometry, and its value "
       "varies from vertex to vertex, or within the object volume"},
      {SHD_ATTRIBUTE_OBJECT,
       "OBJECT",
       0,
       "Object",
       "The attribute is associated with the object or mesh data-block itself, "
       "and its value is uniform"},
      {SHD_ATTRIBUTE_INSTANCER,
       "INSTANCER",
       0,
       "Instancer",
       "The attribute is associated with the instancer particle system or object, "
       "falling back to the Object mode if the attribute isn't found, or the object "
       "is not instanced"},
      {SHD_ATTRIBUTE_VIEW_LAYER,
       "VIEW_LAYER",
       0,
       "View Layer",
       "The attribute is associated with the View Layer, Scene or World that is being rendered"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeShaderAttribute", "storage");

  prop = RNA_def_property(srna, "attribute_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, prop_attribute_type);
  RNA_def_property_ui_text(prop, "Attribute Type", "General type of the attribute");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  prop = RNA_def_property(srna, "attribute_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Attribute Name", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");
}

static void def_sh_tex(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "texture_mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "base.tex_mapping");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Texture Mapping", "Texture coordinate mapping settings");

  prop = RNA_def_property(srna, "color_mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "base.color_mapping");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Color Mapping", "Color mapping settings");
}

static void def_sh_tex_sky(BlenderRNA *brna, StructRNA *srna)
{
  static const EnumPropertyItem prop_sky_type[] = {
      {SHD_SKY_SINGLE_SCATTERING,
       "SINGLE_SCATTERING",
       0,
       "Single Scattering",
       "Single scattering sky model"},
      {SHD_SKY_MULTIPLE_SCATTERING,
       "MULTIPLE_SCATTERING",
       0,
       "Multiple Scattering",
       "Multiple scattering sky model (more accurate)"},
      {SHD_SKY_PREETHAM, "PREETHAM", 0, "Preetham", "Preetham 1999 (Legacy)"},
      {SHD_SKY_HOSEK, "HOSEK_WILKIE", 0, "Hosek / Wilkie", "Hosek / Wilkie 2012 (Legacy)"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static float default_dir[3] = {0.0f, 0.0f, 1.0f};

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeTexSky", "storage");
  def_sh_tex(brna, srna);

  prop = RNA_def_property(srna, "sky_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "sky_model");
  RNA_def_property_enum_items(prop, prop_sky_type);
  RNA_def_property_ui_text(prop, "Sky Type", "Which sky model should be used");
  RNA_def_property_update(prop, 0, "rna_ShaderNode_socket_update");

  /* Nishita parameters. */
  prop = RNA_def_property(srna, "sun_disc", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Sun Disc", "Include the sun itself in the output");
  RNA_def_property_boolean_sdna(prop, nullptr, "sun_disc", 1);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_update(prop, 0, "rna_ShaderNode_socket_update");

  prop = RNA_def_property(srna, "sun_size", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_ui_text(prop, "Sun Size", "Size of sun disc");
  RNA_def_property_range(prop, 0.0f, M_PI_2);
  RNA_def_property_float_default(prop, DEG2RADF(0.545));
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "sun_intensity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Sun Intensity", "Strength of Sun");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "sun_elevation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_ui_text(prop, "Sun Elevation", "Sun angle from horizon");
  RNA_def_property_float_default(prop, DEG2RADF(15.0f));
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "sun_rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_ui_text(prop, "Sun Rotation", "Rotation of sun around zenith");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "altitude", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_ui_text(prop, "Altitude", "Height from sea level");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 100000.0f, 10, 1);
  RNA_def_property_float_default(prop, 100.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "air_density", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Air",
                           "Density of air molecules.\n"
                           "0 means no air, 1 means urban city air");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "aerosol_density", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Aerosols",
                           "Density of dust, pollution and water droplets.\n"
                           "0 means no aerosols, 1 means urban city aerosols");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "ozone_density", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Ozone",
                           "Density of ozone layer.\n"
                           "0 means no ozone, 1 means urban city ozone");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  /* Legacy parameters. */
  prop = RNA_def_property(srna, "sun_direction", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_ui_text(prop, "Sun Direction", "Direction from where the sun is shining");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_dir);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "turbidity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 1.0f, 10.0f);
  RNA_def_property_ui_range(prop, 1.0f, 10.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Turbidity", "Atmospheric turbidity");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "ground_albedo", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Ground Albedo", "Ground color that is subtly reflected in the sky");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static const EnumPropertyItem sh_tex_prop_interpolation_items[] = {
    {SHD_INTERP_LINEAR, "Linear", 0, "Linear", "Linear interpolation"},
    {SHD_INTERP_CLOSEST, "Closest", 0, "Closest", "No interpolation (sample closest texel)"},
    {SHD_INTERP_CUBIC, "Cubic", 0, "Cubic", "Cubic interpolation"},
    {SHD_INTERP_SMART, "Smart", 0, "Smart", "Bicubic when magnifying, else bilinear (OSL only)"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void def_sh_tex_environment(BlenderRNA *brna, StructRNA *srna)
{
  static const EnumPropertyItem prop_projection_items[] = {
      {SHD_PROJ_EQUIRECTANGULAR,
       "EQUIRECTANGULAR",
       0,
       "Equirectangular",
       "Equirectangular or latitude-longitude projection"},
      {SHD_PROJ_MIRROR_BALL,
       "MIRROR_BALL",
       0,
       "Mirror Ball",
       "Projection from an orthographic photo of a mirror ball"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_tex_image_update");

  RNA_def_struct_sdna_from(srna, "NodeTexEnvironment", "storage");
  def_sh_tex(brna, srna);

  prop = RNA_def_property(srna, "projection", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_projection_items);
  RNA_def_property_ui_text(prop, "Projection", "Projection of the input image");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, sh_tex_prop_interpolation_items);
  RNA_def_property_ui_text(prop, "Interpolation", "Texture interpolation");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "iuser");
  RNA_def_property_ui_text(
      prop,
      "Image User",
      "Parameters defining which layer, pass and frame of the image is displayed");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_gabor(BlenderRNA *brna, StructRNA *srna)
{
  static const EnumPropertyItem prop_gabor_types[] = {
      {SHD_GABOR_TYPE_2D,
       "2D",
       0,
       "2D",
       "Use the 2D vector (X, Y) as input. The Z component is ignored."},
      {SHD_GABOR_TYPE_3D, "3D", 0, "3D", "Use the 3D vector (X, Y, Z) as input"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_struct_sdna_from(srna, "NodeTexGabor", "storage");
  def_sh_tex(brna, srna);

  PropertyRNA *prop;
  prop = RNA_def_property(srna, "gabor_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, prop_gabor_types);
  RNA_def_property_ui_text(prop, "Type", "The type of Gabor noise to evaluate");
  RNA_def_property_update(prop, 0, "rna_ShaderNode_socket_update");
}

static void def_sh_tex_image(BlenderRNA *brna, StructRNA *srna)
{
  static const EnumPropertyItem prop_projection_items[] = {
      {SHD_PROJ_FLAT,
       "FLAT",
       0,
       "Flat",
       "Image is projected flat using the X and Y coordinates of the texture vector"},
      {SHD_PROJ_BOX,
       "BOX",
       0,
       "Box",
       "Image is projected using different components for each side of the object space bounding "
       "box"},
      {SHD_PROJ_SPHERE,
       "SPHERE",
       0,
       "Sphere",
       "Image is projected spherically using the Z axis as central"},
      {SHD_PROJ_TUBE,
       "TUBE",
       0,
       "Tube",
       "Image is projected from the tube using the Z axis as central"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_tex_image_update");

  RNA_def_struct_sdna_from(srna, "NodeTexImage", "storage");
  def_sh_tex(brna, srna);

  prop = RNA_def_property(srna, "projection", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_projection_items);
  RNA_def_property_ui_text(
      prop, "Projection", "Method to project 2D image on object with a 3D texture vector");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_IMAGE);
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, sh_tex_prop_interpolation_items);
  RNA_def_property_ui_text(prop, "Interpolation", "Texture interpolation");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "projection_blend", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(
      prop, "Projection Blend", "For box projection, amount of blend to use between sides");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "extension", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_image_extension);
  RNA_def_property_ui_text(
      prop, "Extension", "How the image is extrapolated past its original bounds");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_IMAGE);
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "iuser");
  RNA_def_property_ui_text(
      prop,
      "Image User",
      "Parameters defining which layer, pass and frame of the image is displayed");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_tex_combsep_color(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_combsep_color_items);
  RNA_def_property_ui_text(prop, "Mode", "Mode of color processing");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void def_geo_image_texture(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem fn_tex_prop_interpolation_items[] = {
      {SHD_INTERP_LINEAR, "Linear", 0, "Linear", "Linear interpolation"},
      {SHD_INTERP_CLOSEST, "Closest", 0, "Closest", "No interpolation (sample closest texel)"},
      {SHD_INTERP_CUBIC, "Cubic", 0, "Cubic", "Cubic interpolation"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeGeometryImageTexture", "storage");

  prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, fn_tex_prop_interpolation_items);
  RNA_def_property_ui_text(prop, "Interpolation", "Method for smoothing values between pixels");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "extension", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_image_extension);
  RNA_def_property_ui_text(
      prop, "Extension", "How the image is extrapolated past its original bounds");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_IMAGE);
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void rna_def_geo_gizmo_transform(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeGeometryTransformGizmo", "storage");

  prop = RNA_def_property(srna, "use_translation_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_X);
  RNA_def_property_ui_text(prop, "Use Translation X", nullptr);
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "use_translation_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Y);
  RNA_def_property_ui_text(prop, "Use Translation Y", nullptr);
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "use_translation_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Z);
  RNA_def_property_ui_text(prop, "Use Translation Z", nullptr);
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "use_rotation_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_X);
  RNA_def_property_ui_text(prop, "Use Rotation X", nullptr);
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "use_rotation_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Y);
  RNA_def_property_ui_text(prop, "Use Rotation Y", nullptr);
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "use_rotation_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Z);
  RNA_def_property_ui_text(prop, "Use Rotation Z", nullptr);
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "use_scale_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_X);
  RNA_def_property_ui_text(prop, "Use Scale X", nullptr);
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "use_scale_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Y);
  RNA_def_property_ui_text(prop, "Use Scale Y", nullptr);
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "use_scale_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Z);
  RNA_def_property_ui_text(prop, "Use Scale Z", nullptr);
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_gradient(BlenderRNA *brna, StructRNA *srna)
{
  static const EnumPropertyItem prop_gradient_type[] = {
      {SHD_BLEND_LINEAR, "LINEAR", 0, "Linear", "Create a linear progression"},
      {SHD_BLEND_QUADRATIC, "QUADRATIC", 0, "Quadratic", "Create a quadratic progression"},
      {SHD_BLEND_EASING,
       "EASING",
       0,
       "Easing",
       "Create a progression easing from one step to the next"},
      {SHD_BLEND_DIAGONAL, "DIAGONAL", 0, "Diagonal", "Create a diagonal progression"},
      {SHD_BLEND_SPHERICAL, "SPHERICAL", 0, "Spherical", "Create a spherical progression"},
      {SHD_BLEND_QUADRATIC_SPHERE,
       "QUADRATIC_SPHERE",
       0,
       "Quadratic Sphere",
       "Create a quadratic progression in the shape of a sphere"},
      {SHD_BLEND_RADIAL, "RADIAL", 0, "Radial", "Create a radial progression"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeTexGradient", "storage");
  def_sh_tex(brna, srna);

  prop = RNA_def_property(srna, "gradient_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_gradient_type);
  RNA_def_property_ui_text(prop, "Gradient Type", "Style of the color blending");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_noise(BlenderRNA *brna, StructRNA *srna)
{
  static const EnumPropertyItem prop_noise_type[] = {
      {SHD_NOISE_MULTIFRACTAL,
       "MULTIFRACTAL",
       0,
       "Multifractal",
       "More uneven result (varies with location), more similar to a real terrain"},
      {SHD_NOISE_RIDGED_MULTIFRACTAL,
       "RIDGED_MULTIFRACTAL",
       0,
       "Ridged Multifractal",
       "Create sharp peaks"},
      {SHD_NOISE_HYBRID_MULTIFRACTAL,
       "HYBRID_MULTIFRACTAL",
       0,
       "Hybrid Multifractal",
       "Create peaks and valleys with different roughness values"},
      {SHD_NOISE_FBM, "FBM", 0, "fBM", "The standard fractal Perlin noise"},
      {SHD_NOISE_HETERO_TERRAIN,
       "HETERO_TERRAIN",
       0,
       "Hetero Terrain",
       "Similar to Hybrid Multifractal creates a heterogeneous terrain, but with the likeness of "
       "river channels"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeTexNoise", "storage");
  def_sh_tex(brna, srna);

  prop = RNA_def_property(srna, "noise_dimensions", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "dimensions");
  RNA_def_property_enum_items(prop, rna_enum_node_tex_dimensions_items);
  RNA_def_property_ui_text(prop, "Dimensions", "Number of dimensions to output noise for");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXTURE);
  RNA_def_property_update(prop, 0, "rna_ShaderNode_socket_update");

  prop = RNA_def_property(srna, "noise_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, prop_noise_type);
  RNA_def_property_ui_text(prop, "Type", "Type of the Noise texture");
  RNA_def_property_update(prop, 0, "rna_ShaderNode_socket_update");

  prop = RNA_def_property(srna, "normalize", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "normalize", 0);
  RNA_def_property_ui_text(prop, "Normalize", "Normalize outputs to 0.0 to 1.0 range");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_tex_checker(BlenderRNA *brna, StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeTexChecker", "storage");
  def_sh_tex(brna, srna);
}

static void def_sh_tex_brick(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeTexBrick", "storage");
  def_sh_tex(brna, srna);

  prop = RNA_def_property(srna, "offset_frequency", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "offset_freq");
  RNA_def_property_int_default(prop, 2);
  RNA_def_property_range(prop, 1, 99);
  RNA_def_property_ui_text(
      prop,
      "Offset Frequency",
      "How often rows are offset. A value of 2 gives an even/uneven pattern of rows.");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "squash_frequency", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "squash_freq");
  RNA_def_property_int_default(prop, 2);
  RNA_def_property_range(prop, 1, 99);
  RNA_def_property_ui_text(
      prop, "Squash Frequency", "How often rows consist of \"squished\" bricks");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "offset");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Offset Amount", "Determines the brick offset of the various rows");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "squash", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "squash");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.0f, 99.0f);
  RNA_def_property_ui_text(
      prop,
      "Squash Amount",
      "Factor to adjust the brick's width for particular rows determined by the Offset Frequency");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_magic(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeTexMagic", "storage");
  def_sh_tex(brna, srna);

  prop = RNA_def_property(srna, "turbulence_depth", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "depth");
  RNA_def_property_range(prop, 0, 10);
  RNA_def_property_ui_text(prop, "Depth", "Level of detail in the added turbulent noise");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_voronoi(BlenderRNA *brna, StructRNA *srna)
{
  static EnumPropertyItem prop_distance_items[] = {
      {SHD_VORONOI_EUCLIDEAN, "EUCLIDEAN", 0, "Euclidean", "Euclidean distance"},
      {SHD_VORONOI_MANHATTAN, "MANHATTAN", 0, "Manhattan", "Manhattan distance"},
      {SHD_VORONOI_CHEBYCHEV, "CHEBYCHEV", 0, "Chebychev", "Chebychev distance"},
      {SHD_VORONOI_MINKOWSKI, "MINKOWSKI", 0, "Minkowski", "Minkowski distance"},
      {0, nullptr, 0, nullptr, nullptr}};

  static EnumPropertyItem prop_feature_items[] = {
      {SHD_VORONOI_F1,
       "F1",
       0,
       "F1",
       "Computes the distance to the closest point as well as its position and color"},
      {SHD_VORONOI_F2,
       "F2",
       0,
       "F2",
       "Computes the distance to the second closest point as well as its position and color"},
      {SHD_VORONOI_SMOOTH_F1,
       "SMOOTH_F1",
       0,
       "Smooth F1",
       "Smoothed version of F1. Weighted sum of neighbor voronoi cells."},
      {SHD_VORONOI_DISTANCE_TO_EDGE,
       "DISTANCE_TO_EDGE",
       0,
       "Distance to Edge",
       "Computes the distance to the edge of the voronoi cell"},
      {SHD_VORONOI_N_SPHERE_RADIUS,
       "N_SPHERE_RADIUS",
       0,
       "N-Sphere Radius",
       "Computes the radius of the n-sphere inscribed in the voronoi cell"},
      {0, nullptr, 0, nullptr, nullptr}};

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeTexVoronoi", "storage");
  def_sh_tex(brna, srna);

  prop = RNA_def_property(srna, "voronoi_dimensions", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "dimensions");
  RNA_def_property_enum_items(prop, rna_enum_node_tex_dimensions_items);
  RNA_def_property_ui_text(prop, "Dimensions", "Number of dimensions to output noise for");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXTURE);
  RNA_def_property_update(prop, 0, "rna_ShaderNode_socket_update");

  prop = RNA_def_property(srna, "distance", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "distance");
  RNA_def_property_enum_items(prop, prop_distance_items);
  RNA_def_property_ui_text(
      prop, "Distance Metric", "The distance metric used to compute the texture");
  RNA_def_property_update(prop, 0, "rna_ShaderNode_socket_update");

  prop = RNA_def_property(srna, "feature", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "feature");
  RNA_def_property_enum_items(prop, prop_feature_items);
  RNA_def_property_ui_text(
      prop, "Feature Output", "The Voronoi feature that the node will compute");
  RNA_def_property_update(prop, 0, "rna_ShaderNode_socket_update");

  prop = RNA_def_property(srna, "normalize", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "normalize", 0);
  RNA_def_property_ui_text(prop, "Normalize", "Normalize output Distance to 0.0 to 1.0 range");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_tex_wave(BlenderRNA *brna, StructRNA *srna)
{
  static const EnumPropertyItem prop_wave_type_items[] = {
      {SHD_WAVE_BANDS, "BANDS", 0, "Bands", "Use standard wave texture in bands"},
      {SHD_WAVE_RINGS, "RINGS", 0, "Rings", "Use wave texture in rings"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static EnumPropertyItem prop_wave_bands_direction_items[] = {
      {SHD_WAVE_BANDS_DIRECTION_X, "X", 0, "X", "Bands across X axis"},
      {SHD_WAVE_BANDS_DIRECTION_Y, "Y", 0, "Y", "Bands across Y axis"},
      {SHD_WAVE_BANDS_DIRECTION_Z, "Z", 0, "Z", "Bands across Z axis"},
      {SHD_WAVE_BANDS_DIRECTION_DIAGONAL, "DIAGONAL", 0, "Diagonal", "Bands across diagonal axis"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static EnumPropertyItem prop_wave_rings_direction_items[] = {
      {SHD_WAVE_RINGS_DIRECTION_X, "X", 0, "X", "Rings along X axis"},
      {SHD_WAVE_RINGS_DIRECTION_Y, "Y", 0, "Y", "Rings along Y axis"},
      {SHD_WAVE_RINGS_DIRECTION_Z, "Z", 0, "Z", "Rings along Z axis"},
      {SHD_WAVE_RINGS_DIRECTION_SPHERICAL,
       "SPHERICAL",
       0,
       "Spherical",
       "Rings along spherical distance"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_wave_profile_items[] = {
      {SHD_WAVE_PROFILE_SIN, "SIN", 0, "Sine", "Use a standard sine profile"},
      {SHD_WAVE_PROFILE_SAW, "SAW", 0, "Saw", "Use a sawtooth profile"},
      {SHD_WAVE_PROFILE_TRI, "TRI", 0, "Triangle", "Use a triangle profile"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeTexWave", "storage");
  def_sh_tex(brna, srna);

  prop = RNA_def_property(srna, "wave_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "wave_type");
  RNA_def_property_enum_items(prop, prop_wave_type_items);
  RNA_def_property_ui_text(prop, "Wave Type", "");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "bands_direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "bands_direction");
  RNA_def_property_enum_items(prop, prop_wave_bands_direction_items);
  RNA_def_property_ui_text(prop, "Bands Direction", "");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "rings_direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "rings_direction");
  RNA_def_property_enum_items(prop, prop_wave_rings_direction_items);
  RNA_def_property_ui_text(prop, "Rings Direction", "");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "wave_profile", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "wave_profile");
  RNA_def_property_enum_items(prop, prop_wave_profile_items);
  RNA_def_property_ui_text(prop, "Wave Profile", "");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_white_noise(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "noise_dimensions", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_tex_dimensions_items);
  RNA_def_property_ui_text(prop, "Dimensions", "Number of dimensions to output noise for");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXTURE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_sh_tex_coord(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Object", "Use coordinates from this object (for object texture coordinates output)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  prop = RNA_def_property(srna, "from_instancer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(
      prop, "From Instancer", "Use the parent of the instance object if possible");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_vect_transform(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem prop_vect_type_items[] = {
      {SHD_VECT_TRANSFORM_TYPE_POINT, "POINT", 0, "Point", "Transform a point"},
      {SHD_VECT_TRANSFORM_TYPE_VECTOR, "VECTOR", 0, "Vector", "Transform a direction vector"},
      {SHD_VECT_TRANSFORM_TYPE_NORMAL,
       "NORMAL",
       0,
       "Normal",
       "Transform a normal vector with unit length"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_vect_space_items[] = {
      {SHD_VECT_TRANSFORM_SPACE_WORLD, "WORLD", 0, "World", ""},
      {SHD_VECT_TRANSFORM_SPACE_OBJECT, "OBJECT", 0, "Object", ""},
      {SHD_VECT_TRANSFORM_SPACE_CAMERA, "CAMERA", 0, "Camera", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeShaderVectTransform", "storage");

  prop = RNA_def_property(srna, "vector_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, prop_vect_type_items);
  RNA_def_property_ui_text(prop, "Type", "");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "convert_from", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_vect_space_items);
  RNA_def_property_ui_text(prop, "Convert From", "Space to convert from");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "convert_to", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_vect_space_items);
  RNA_def_property_ui_text(prop, "Convert To", "Space to convert to");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_wireframe(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_pixel_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(prop, "Pixel Size", "Use screen pixel size instead of world units");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_metallic(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_metallic_distribution_items);
  RNA_def_property_ui_text(prop, "Distribution", "Light scattering distribution on rough surface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "fresnel_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom2");
  RNA_def_property_enum_items(prop, node_metallic_fresnel_type_items);
  RNA_def_property_ui_text(prop, "Fresnel Type", "Fresnel method used to tint the metal");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_glossy(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_glossy_items);
  RNA_def_property_ui_text(prop, "Distribution", "Light scattering distribution on rough surface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_glass(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_glass_items);
  RNA_def_property_ui_text(prop, "Distribution", "Light scattering distribution on rough surface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sheen(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_sheen_items);
  RNA_def_property_ui_text(prop, "Distribution", "Sheen shading model");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_principled(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_principled_distribution_items);
  RNA_def_property_ui_text(prop, "Distribution", "Light scattering distribution on rough surface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");

  prop = RNA_def_property(srna, "subsurface_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom2");
  RNA_def_property_enum_items(prop, node_subsurface_method_items);
  RNA_def_property_ui_text(
      prop, "Subsurface Method", "Method for rendering subsurface scattering");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_refraction(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_refraction_items);
  RNA_def_property_ui_text(prop, "Distribution", "Light scattering distribution on rough surface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_scatter(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "phase", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_scatter_phase_items);
  RNA_def_property_ui_text(prop, "Phase", "Phase function for the scattered light");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_volume_coefficients(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "phase", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_scatter_phase_items);
  RNA_def_property_ui_text(prop, "Phase", "Phase function for the scattered light");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_toon(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "component", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_toon_items);
  RNA_def_property_ui_text(prop, "Component", "Toon BSDF component to use");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_bump(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(
      prop, "Invert", "Invert the bump mapping direction to push into the surface instead of out");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_hair(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "component", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_hair_items);
  RNA_def_property_ui_text(prop, "Component", "Hair BSDF component to use");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* RNA initialization for the custom properties. */
static void def_hair_principled(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeShaderHairPrincipled", "storage");

  prop = RNA_def_property(srna, "model", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "model");
  RNA_def_property_ui_text(prop, "Scattering model", "Select from Chiang or Huang model");
  RNA_def_property_enum_items(prop, node_principled_hair_model_items);
  RNA_def_property_enum_default(prop, SHD_PRINCIPLED_HAIR_HUANG);
  /* Upon editing, update both the node data AND the UI representation */
  /* (This effectively shows/hides the relevant sockets) */
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");

  prop = RNA_def_property(srna, "parametrization", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "parametrization");
  RNA_def_property_ui_text(
      prop, "Color Parametrization", "Select the shader's color parametrization");
  RNA_def_property_enum_items(prop, node_principled_hair_parametrization_items);
  RNA_def_property_enum_default(prop, SHD_PRINCIPLED_HAIR_REFLECTANCE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_sh_uvmap(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "from_instancer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(
      prop, "From Instancer", "Use the parent of the instance object if possible");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeShaderUVMap", "storage");

  prop = RNA_def_property(srna, "uv_map", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "UV Map", "UV coordinates to be used for mapping");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "bNode", nullptr);
}

static void def_sh_vertex_color(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeShaderVertexColor", "storage");

  prop = RNA_def_property(srna, "layer_name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Color Attribute", "Color Attribute");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "bNode", nullptr);
}

static void def_sh_uvalongstroke(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_tips", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(
      prop, "Use Tips", "Lower half of the texture is for tips of the stroke");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_normal_map(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem prop_space_items[] = {
      {SHD_SPACE_TANGENT, "TANGENT", 0, "Tangent Space", "Tangent space normal mapping"},
      {SHD_SPACE_OBJECT, "OBJECT", 0, "Object Space", "Object space normal mapping"},
      {SHD_SPACE_WORLD, "WORLD", 0, "World Space", "World space normal mapping"},
      {SHD_SPACE_BLENDER_OBJECT,
       "BLENDER_OBJECT",
       0,
       "Blender Object Space",
       "Object space normal mapping, compatible with Blender render baking"},
      {SHD_SPACE_BLENDER_WORLD,
       "BLENDER_WORLD",
       0,
       "Blender World Space",
       "World space normal mapping, compatible with Blender render baking"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeShaderNormalMap", "storage");

  prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_space_items);
  RNA_def_property_ui_text(prop, "Space", "Space of the input normal");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "uv_map", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "UV Map", "UV Map for tangent space maps");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "bNode", nullptr);
}

static void def_sh_radial_tiling(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeRadialTiling", "storage");

  prop = RNA_def_property(srna, "normalize", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "normalize", 0);
  RNA_def_property_ui_text(
      prop,
      "Normalize",
      "Normalize the X coordinate of the Segment Coordinates output to a [0, 1] interval and "
      "offset the Y coordinate into a [0, infinity) interval. When checked, the textures are "
      "stretched to fit into each angular segment. When not checked, the parts of the textures "
      "that don't fit into each angular segment are cropped");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_displacement(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem prop_space_items[] = {
      {SHD_SPACE_OBJECT,
       "OBJECT",
       0,
       "Object Space",
       "Displacement is in object space, affected by object scale"},
      {SHD_SPACE_WORLD,
       "WORLD",
       0,
       "World Space",
       "Displacement is in world space, not affected by object scale"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, prop_space_items);
  RNA_def_property_ui_text(prop, "Space", "Space of the input height");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "bNode", nullptr);
}

static void def_sh_vector_displacement(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem prop_space_items[] = {
      {SHD_SPACE_TANGENT,
       "TANGENT",
       0,
       "Tangent Space",
       "Tangent space vector displacement mapping"},
      {SHD_SPACE_OBJECT, "OBJECT", 0, "Object Space", "Object space vector displacement mapping"},
      {SHD_SPACE_WORLD, "WORLD", 0, "World Space", "World space vector displacement mapping"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, prop_space_items);
  RNA_def_property_ui_text(prop, "Space", "Space of the input height");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "bNode", nullptr);
}

static void def_sh_tangent(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem prop_direction_type_items[] = {
      {SHD_TANGENT_RADIAL, "RADIAL", 0, "Radial", "Radial tangent around the X, Y or Z axis"},
      {SHD_TANGENT_UVMAP, "UV_MAP", 0, "UV Map", "Tangent from UV map"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_axis_items[] = {
      {SHD_TANGENT_AXIS_X, "X", 0, "X", "X axis"},
      {SHD_TANGENT_AXIS_Y, "Y", 0, "Y", "Y axis"},
      {SHD_TANGENT_AXIS_Z, "Z", 0, "Z", "Z axis"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeShaderTangent", "storage");

  prop = RNA_def_property(srna, "direction_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_direction_type_items);
  RNA_def_property_ui_text(prop, "Direction", "Method to use for the tangent");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_axis_items);
  RNA_def_property_ui_text(prop, "Axis", "Axis for radial tangents");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "uv_map", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "UV Map", "UV Map for tangent generated from UV");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "bNode", nullptr);
}

static void def_sh_bevel(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "samples", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_range(prop, 2, 128);
  RNA_def_property_ui_range(prop, 2, 16, 1, 1);
  RNA_def_property_ui_text(prop, "Samples", "Number of rays to trace per shader evaluation");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_ambient_occlusion(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "samples", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_range(prop, 1, 128);
  RNA_def_property_ui_text(prop, "Samples", "Number of rays to trace per shader evaluation");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "inside", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom2", SHD_AO_INSIDE);
  RNA_def_property_ui_text(prop, "Inside", "Trace rays towards the inside of the object");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "only_local", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom2", SHD_AO_LOCAL);
  RNA_def_property_ui_text(
      prop, "Only Local", "Only consider the object itself when computing AO");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_subsurface(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "falloff", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_subsurface_method_items);
  RNA_def_property_ui_text(prop, "Method", "Method for rendering subsurface scattering");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_sh_tex_ies(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "ies", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Text");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "IES Text", "Internal IES file");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeShaderTexIES", "storage");

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "File Path", "IES light path");
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_ShaderNodeTexIES_mode_set", nullptr);
  RNA_def_property_enum_items(prop, node_ies_mode_items);
  RNA_def_property_ui_text(
      prop, "Source", "Whether the IES file is loaded from disk or from a text data-block");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "bNode", nullptr);
}

static void def_sh_output_aov(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeShaderOutputAOV", "storage");

  prop = RNA_def_property(srna, "aov_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Name", "Name of the AOV that this output writes to");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "bNode", nullptr);
}

static void def_sh_combsep_color(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeCombSepColor", "storage");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_combsep_color_items);
  RNA_def_property_ui_text(prop, "Mode", "Mode of color processing");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void def_sh_script(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "script", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Text");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Script", "Internal shader script to define the shader");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNodeScript_update");

  RNA_def_struct_sdna_from(srna, "NodeShaderScript", "storage");

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "File Path", "Shader script path");
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNodeScript_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_ShaderNodeScript_mode_set", nullptr);
  RNA_def_property_enum_items(prop, node_script_mode_items);
  RNA_def_property_ui_text(prop, "Script Source", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_auto_update", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_SCRIPT_AUTO_UPDATE);
  RNA_def_property_ui_text(
      prop,
      "Auto Update",
      "Automatically update the shader when the .osl file changes (external scripts only)");

  prop = RNA_def_property(srna, "bytecode", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ShaderNodeScript_bytecode_get",
                                "rna_ShaderNodeScript_bytecode_length",
                                "rna_ShaderNodeScript_bytecode_set");
  RNA_def_property_ui_text(prop, "Bytecode", "Compile bytecode for shader script node");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "bytecode_hash", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Bytecode Hash", "Hash of compile bytecode, for quick equality checking");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  /* needs to be reset to avoid bad pointer type in API functions below */
  RNA_def_struct_sdna_from(srna, "bNode", nullptr);

  /* API functions */

#  if 0 /* XXX TODO: use general node API for this. */
  func = RNA_def_function(srna, "find_socket", "rna_ShaderNodeScript_find_socket");
  RNA_def_function_ui_description(func, "Find a socket by name");
  parm = RNA_def_string(func, "name", nullptr, 0, "Socket name", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /*parm =*/RNA_def_boolean(func, "is_output", false, "Output", "Whether the socket is an output");
  parm = RNA_def_pointer(func, "result", "NodeSocket", "", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "add_socket", "rna_ShaderNodeScript_add_socket");
  RNA_def_function_ui_description(func, "Add a socket");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_enum(func, "type", rna_enum_node_socket_type_items, SOCK_FLOAT, "Type", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /*parm =*/RNA_def_boolean(func, "is_output", false, "Output", "Whether the socket is an output");
  parm = RNA_def_pointer(func, "result", "NodeSocket", "", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove_socket", "rna_ShaderNodeScript_remove_socket");
  RNA_def_function_ui_description(func, "Remove a socket");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_pointer(func, "sock", "NodeSocket", "Socket", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
#  endif
}

/* -- Compositor Nodes ------------------------------------------------------ */

static void def_node_image_user(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "frame_duration", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "frames");
  RNA_def_property_range(prop, 0, MAXFRAMEF);
  RNA_def_property_ui_text(
      prop, "Frames", "Number of images of a movie to use"); /* copied from the rna_image.cc */
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "sfra");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  /* copied from the rna_image.cc */
  RNA_def_property_ui_text(
      prop,
      "Start Frame",
      "Global starting frame of the movie/sequence, assuming first picture has a #1");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "frame_offset", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "offset");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  /* copied from the rna_image.cc */
  RNA_def_property_ui_text(
      prop, "Offset", "Offset the number of the frame to use in the animation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cycl", 1);
  RNA_def_property_ui_text(
      prop, "Cyclic", "Cycle the images in the movie"); /* copied from the rna_image.cc */
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_auto_refresh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", IMA_ANIM_ALWAYS);
  /* copied from the rna_image.cc */
  RNA_def_property_ui_text(prop, "Auto-Refresh", "Always refresh image on frame changes");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "layer", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "layer");
  RNA_def_property_enum_items(prop, prop_image_layer_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Node_image_layer_itemf");
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  RNA_def_property_ui_text(prop, "Layer", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_image_layer_update");

  prop = RNA_def_property(srna, "has_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Node_image_has_layers_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Has Layers", "True if this image has any named layer");

  prop = RNA_def_property(srna, "view", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "view");
  RNA_def_property_enum_items(prop, prop_image_view_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Node_image_view_itemf");
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  RNA_def_property_ui_text(prop, "View", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "has_views", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Node_image_has_views_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Has View", "True if this image has multiple views");
}

static void def_cmp_image(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Image_Node_update_id");

  /* NOTE: Image user properties used in the UI are redefined in def_node_image_user,
   * to trigger correct updates of the node editor. RNA design problem that prevents
   * updates from nested structs. */
  RNA_def_struct_sdna_from(srna, "ImageUser", "storage");
  def_node_image_user(brna, srna);
}

static void def_cmp_render_layers(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_Node_scene_set", nullptr, nullptr);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Scene", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_view_layer_update");

  prop = RNA_def_property(srna, "layer", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, prop_view_layer_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Node_view_layer_itemf");
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  RNA_def_property_ui_text(prop, "Layer", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_view_layer_update");
}

static void rna_def_cmp_file_output_item(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "NodeCompositorFileOutputItem", nullptr);
  RNA_def_struct_ui_text(srna, "File Output Item", "");

  rna_def_node_item_array_socket_item_common(srna, "FileOutputItemsAccessor", true, true);

  PropertyRNA *prop = RNA_def_property(srna, "override_node_format", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "override_node_format", 1);
  RNA_def_property_ui_text(prop,
                           "Override Node Format",
                           "Use a different format instead of the node format for this file");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "save_as_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "save_as_render", 1);
  RNA_def_property_ui_text(
      prop, "Save as Render", "Apply render part of display transform when saving byte image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "format", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ImageFormatSettings");
}

static void rna_def_cmp_file_output_items(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "NodeCompositorFileOutputItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of file output items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeCompositorFileOutputItem", "FileOutputItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "NodeCompositorFileOutputItem", "FileOutputItemsAccessor");
}

static void def_cmp_file_output(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_cmp_file_output_item(brna);
  rna_def_cmp_file_output_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeCompositorFileOutput", "storage");

  prop = RNA_def_property(srna, "file_output_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_count");
  RNA_def_property_struct_type(prop, "NodeCompositorFileOutputItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeCompositorFileOutputItems");

  prop = RNA_def_property(srna, "active_item_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_item_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, nullptr, "directory");
  RNA_def_property_ui_text(prop, "Directory", "The directory where the image will be written");
  RNA_def_property_flag(
      prop, PROP_PATH_OUTPUT | PROP_PATH_SUPPORTS_BLEND_RELATIVE | PROP_PATH_SUPPORTS_TEMPLATES);
  RNA_def_property_path_template_type(prop, PROP_VARIABLES_RENDER_OUTPUT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "file_name", PROP_STRING, PROP_FILENAME);
  RNA_def_property_string_sdna(prop, nullptr, "file_name");
  RNA_def_property_ui_text(prop,
                           "File Name",
                           "The base name of the file. Other information might be included in the "
                           "final file name depending on the node options");
  RNA_def_property_flag(prop, PROP_PATH_OUTPUT | PROP_PATH_SUPPORTS_TEMPLATES);
  RNA_def_property_path_template_type(prop, PROP_VARIABLES_RENDER_OUTPUT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "format", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ImageFormatSettings");

  prop = RNA_def_property(srna, "save_as_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "save_as_render", 1);
  RNA_def_property_ui_text(
      prop, "Save as Render", "Apply render part of display transform when saving byte image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);
}

static void def_cmp_convert_color_space(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;
  RNA_def_struct_sdna_from(srna, "NodeConvertColorSpace", "storage");

  prop = RNA_def_property(srna, "from_color_space", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_enum_items(prop, rna_enum_color_space_convert_default_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_NodeConvertColorSpace_from_color_space_get",
                              "rna_NodeConvertColorSpace_from_color_space_set",
                              "rna_NodeConvertColorSpace_color_space_itemf");
  RNA_def_property_ui_text(prop, "From", "Color space of the input image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "to_color_space", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_enum_items(prop, rna_enum_color_space_convert_default_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_NodeConvertColorSpace_to_color_space_get",
                              "rna_NodeConvertColorSpace_to_color_space_set",
                              "rna_NodeConvertColorSpace_color_space_itemf");
  RNA_def_property_ui_text(prop, "To", "Color space of the output image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_convert_to_display(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;
  RNA_def_struct_sdna_from(srna, "NodeConvertToDisplay", "storage");

  prop = RNA_def_property(srna, "view_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "view_settings");
  RNA_def_property_struct_type(prop, "ColorManagedViewSettings");
  RNA_def_property_ui_text(prop, "View Settings", "Color management view transform settings");

  prop = RNA_def_property(srna, "display_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "display_settings");
  RNA_def_property_struct_type(prop, "ColorManagedDisplaySettings");
  RNA_def_property_ui_text(prop, "Display Settings", "Color management display device settings");
}

static void def_cmp_defocus(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem bokeh_items[] = {
      {8, "OCTAGON", 0, "Octagonal", "8 sides"},
      {7, "HEPTAGON", 0, "Heptagonal", "7 sides"},
      {6, "HEXAGON", 0, "Hexagonal", "6 sides"},
      {5, "PENTAGON", 0, "Pentagonal", "5 sides"},
      {4, "SQUARE", 0, "Square", "4 sides"},
      {3, "TRIANGLE", 0, "Triangular", "3 sides"},
      {0, "CIRCLE", 0, "Circular", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_Node_scene_set", nullptr, nullptr);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Scene", "Scene from which to select the active camera (render scene if undefined)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "NodeDefocus", "storage");

  prop = RNA_def_property(srna, "bokeh", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "bktype");
  RNA_def_property_enum_items(prop, bokeh_items);
  RNA_def_property_ui_text(prop, "Bokeh Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "rotation");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(90.0f));
  RNA_def_property_ui_text(prop, "Angle", "Bokeh shape rotation offset");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "f_stop", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fstop");
  RNA_def_property_range(prop, 0.0f, 128.0f);
  RNA_def_property_ui_text(
      prop,
      "F-Stop",
      "Amount of focal blur, 128 (infinity) is perfect focus, half the value doubles "
      "the blur radius");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "blur_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "maxblur");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Max Blur", "Blur limit, maximum CoC radius");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_zbuffer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "no_zbuf", 1);
  RNA_def_property_ui_text(prop,
                           "Use Z-Buffer",
                           "Disable when using an image as input instead of actual z-buffer "
                           "(auto enabled if node not image based, eg. time node)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "z_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "scale");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_ui_text(
      prop,
      "Z-Scale",
      "Scale the Z input when not using a z-buffer, controls maximum blur designated "
      "by the color white or input value 1");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_colorbalance(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "input_whitepoint", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop,
                               "rna_NodeColorBalance_input_whitepoint_get",
                               "rna_NodeColorBalance_input_whitepoint_set",
                               nullptr);
  RNA_def_property_ui_text(prop,
                           "Input White Point",
                           "The color which gets mapped to white "
                           "(automatically converted to/from temperature and tint)");
  RNA_def_property_update(prop, NC_WINDOW, "rna_Node_update");

  prop = RNA_def_property(srna, "output_whitepoint", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop,
                               "rna_NodeColorBalance_output_whitepoint_get",
                               "rna_NodeColorBalance_output_whitepoint_set",
                               nullptr);
  RNA_def_property_ui_text(prop,
                           "Output White Point",
                           "The color which gets white gets mapped to "
                           "(automatically converted to/from temperature and tint)");
  RNA_def_property_update(prop, NC_WINDOW, "rna_Node_update");
}

static void def_cmp_huecorrect(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_combsep_color(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem mode_items[] = {
      {CMP_NODE_COMBSEP_COLOR_RGB,
       "RGB",
       ICON_NONE,
       "RGB",
       "Use RGB (Red, Green, Blue) color processing"},
      {CMP_NODE_COMBSEP_COLOR_HSV,
       "HSV",
       ICON_NONE,
       "HSV",
       "Use HSV (Hue, Saturation, Value) color processing"},
      {CMP_NODE_COMBSEP_COLOR_HSL,
       "HSL",
       ICON_NONE,
       "HSL",
       "Use HSL (Hue, Saturation, Lightness) color processing"},
      {CMP_NODE_COMBSEP_COLOR_YCC,
       "YCC",
       ICON_NONE,
       "YCbCr",
       "Use YCbCr (Y - luma, Cb - blue-difference chroma, Cr - red-difference chroma) color "
       "processing"},
      {CMP_NODE_COMBSEP_COLOR_YUV,
       "YUV",
       ICON_NONE,
       "YUV",
       "Use YUV (Y - luma, U V - chroma) color processing"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeCMPCombSepColor", "storage");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Mode of color processing");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "ycc_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, node_ycc_items);
  RNA_def_property_ui_text(prop, "Color Space", "Color space used for YCbCrA processing");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_movieclip(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "MovieClipUser", "storage");
}

static void def_cmp_stabilize2d(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");
}

static void def_cmp_moviedistortion(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");
}

static void def_cmp_mask(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mask", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Mask");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Mask", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");
}

static void def_cmp_viewer(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop = RNA_def_property(srna, "ui_shortcut", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_int_funcs(prop, nullptr, "rna_Node_Viewer_shortcut_node_set", nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_int_default(prop, NODE_VIEWER_SHORTCUT_NONE);
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);
}

static void def_cmp_keyingscreen(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "NodeKeyingScreenData", "storage");

  prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "tracking_object");
  RNA_def_property_ui_text(prop, "Tracking Object", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_trackpos(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "NodeTrackPosData", "storage");

  prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "tracking_object");
  RNA_def_property_ui_text(prop, "Tracking Object", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "track_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "track_name");
  RNA_def_property_ui_text(prop, "Track", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_planetrackdeform(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "NodePlaneTrackDeformData", "storage");

  prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "tracking_object");
  RNA_def_property_ui_text(prop, "Tracking Object", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "plane_track_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "plane_track_name");
  RNA_def_property_ui_text(prop, "Plane Track", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

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

static void def_cmp_cryptomatte_common(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;
  static float default_1[3] = {1.0f, 1.0f, 1.0f};

  prop = RNA_def_property(srna, "matte_id", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeCryptomatte_matte_get",
                                "rna_NodeCryptomatte_matte_length",
                                "rna_NodeCryptomatte_matte_set");
  RNA_def_property_ui_text(
      prop, "Matte Objects", "List of object and material crypto IDs to include in matte");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "add", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "runtime.add");
  RNA_def_property_float_array_default(prop, default_1);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Add", "Add object or material to matte, by picking a color from the Pick output");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeCryptomatte_update_add");

  prop = RNA_def_property(srna, "remove", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "runtime.remove");
  RNA_def_property_float_array_default(prop, default_1);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_text(
      prop,
      "Remove",
      "Remove object or material from matte, by picking a color from the Pick output");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeCryptomatte_update_remove");
}

static void def_cmp_cryptomatte_legacy(BlenderRNA *brna, StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeCryptomatte", "storage");
  def_cmp_cryptomatte_common(brna, srna);
}

static void def_cmp_cryptomatte(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem cryptomatte_source_items[] = {
      {CMP_NODE_CRYPTOMATTE_SOURCE_RENDER,
       "RENDER",
       0,
       "Render",
       "Use Cryptomatte passes from a render"},
      {CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE,
       "IMAGE",
       0,
       "Image",
       "Use Cryptomatte passes from an image"},
      {0, nullptr, 0, nullptr, nullptr}};

  prop = RNA_def_property(srna, "source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, cryptomatte_source_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_NodeCryptomatte_source_set", nullptr);
  RNA_def_property_ui_text(prop, "Source", "Where the Cryptomatte passes are loaded from");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(
      prop, "rna_NodeCryptomatte_scene_get", "rna_NodeCryptomatte_scene_set", nullptr, nullptr);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Scene", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(prop,
                                 "rna_NodeCryptomatte_image_get",
                                 "rna_NodeCryptomatte_image_set",
                                 nullptr,
                                 "rna_NodeCryptomatte_image_poll");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "NodeCryptomatte", "storage");
  def_cmp_cryptomatte_common(brna, srna);

  prop = RNA_def_property(srna, "layer_name", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, node_cryptomatte_layer_name_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_NodeCryptomatte_layer_name_get",
                              "rna_NodeCryptomatte_layer_name_set",
                              "rna_NodeCryptomatte_layer_name_itemf");
  RNA_def_property_ui_text(prop, "Cryptomatte Layer", "What Cryptomatte layer is used");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "entries", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "entries", nullptr);
  RNA_def_property_struct_type(prop, "CryptomatteEntry");
  RNA_def_property_ui_text(prop, "Mattes", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* Included here instead of defining image_user as a property of the node,
   * see def_cmp_image for details.
   * As mentioned in DNA_node_types.h, iuser is the first member of the Cryptomatte
   * storage type, so we can cast node->storage to ImageUser.
   * That is required since we can't define the properties from storage->iuser directly... */
  RNA_def_struct_sdna_from(srna, "ImageUser", "storage");
  def_node_image_user(brna, srna);
}

/* -- Texture Nodes --------------------------------------------------------- */

static void def_tex_output(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "TexNodeOutput", "storage");

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Output Name", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_tex_image(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "ImageUser");
  RNA_def_property_ui_text(
      prop, "Image User", "Parameters defining the image duration, offset and related settings");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_tex_bricks(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom3");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Offset Amount", "Determines the brick offset of the various rows");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "offset_frequency", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_range(prop, 2, 99);
  RNA_def_property_ui_text(prop, "Offset Frequency", "Offset every N rows");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "squash", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom4");
  RNA_def_property_range(prop, 0.0f, 99.0f);
  RNA_def_property_ui_text(
      prop,
      "Squash Amount",
      "Factor to adjust the brick's width for particular rows determined by the Offset Frequency");

  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "squash_frequency", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom2");
  RNA_def_property_range(prop, 2, 99);
  RNA_def_property_ui_text(prop, "Squash Frequency", "Squash every N rows");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* -- Geometry Nodes --------------------------------------------------------- */

static void def_geo_curve_sample(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static EnumPropertyItem mode_items[] = {
      {GEO_NODE_CURVE_SAMPLE_FACTOR,
       "FACTOR",
       0,
       "Factor",
       "Find sample positions on the curve using a factor of its total length"},
      {GEO_NODE_CURVE_SAMPLE_LENGTH,
       "LENGTH",
       0,
       "Length",
       "Find sample positions on the curve using a distance from its beginning"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_struct_sdna_from(srna, "NodeGeometryCurveSample", "storage");

  PropertyRNA *prop;
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Method for sampling input");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "use_all_curves", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "All Curves",
                           "Sample lengths based on the total length of all curves, rather than "
                           "using a length inside each selected curve");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_type_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_GeometryNodeAttributeType_type_with_socket_itemf");
  RNA_def_property_enum_default(prop, CD_PROP_FLOAT);
  RNA_def_property_ui_text(prop, "Data Type", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
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

static void def_geo_distribute_points_on_faces(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem rna_node_geometry_distribute_points_on_faces_mode_items[] = {
      {GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM,
       "RANDOM",
       0,
       "Random",
       "Distribute points randomly on the surface"},
      {GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON,
       "POISSON",
       0,
       "Poisson Disk",
       "Distribute the points randomly on the surface while taking a minimum distance between "
       "points into account"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "distribute_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_node_geometry_distribute_points_on_faces_mode_items);
  RNA_def_property_enum_default(prop, GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM);
  RNA_def_property_ui_text(prop, "Distribution Method", "Method to use for scattering points");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "use_legacy_normal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom2", 1);
  RNA_def_property_ui_text(prop,
                           "Legacy Normal",
                           "Output the normal and rotation values that have been output "
                           "before the node started taking smooth normals into account");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void def_geo_curve_set_handle_type(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeGeometryCurveSetHandles", "storage");

  prop = RNA_def_property(srna, "handle_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "handle_type");
  RNA_def_property_ui_text(prop, "Handle Type", "");
  RNA_def_property_enum_items(prop, rna_node_geometry_curve_handle_type_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, rna_enum_node_geometry_curve_handle_side_items);
  RNA_def_property_ui_text(prop, "Mode", "Whether to update left and right handles");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void def_common_zone_input(BlenderRNA * /*brna*/, StructRNA *srna)
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

static void def_geo_simulation_input(BlenderRNA *brna, StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeGeometrySimulationInput", "storage");

  def_common_zone_input(brna, srna);
}

static void def_geo_repeat_input(BlenderRNA *brna, StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeGeometryRepeatInput", "storage");

  def_common_zone_input(brna, srna);
}

static void def_geo_foreach_geometry_element_input(BlenderRNA *brna, StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeGeometryForeachGeometryElementInput", "storage");

  def_common_zone_input(brna, srna);
}

static void def_closure_input(BlenderRNA *brna, StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeClosureInput", "storage");

  def_common_zone_input(brna, srna);
}

static void rna_def_geo_simulation_state_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "SimulationStateItem", nullptr);
  RNA_def_struct_ui_text(srna, "Simulation Item", "");
  RNA_def_struct_sdna(srna, "NodeSimulationItem");

  rna_def_node_item_array_socket_item_common(srna, "SimulationItemsAccessor", true);

  prop = RNA_def_property(srna, "attribute_domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_GeometryNodeAttributeDomain_attribute_domain_itemf");
  RNA_def_property_ui_text(
      prop,
      "Attribute Domain",
      "Attribute domain where the attribute is stored in the simulation state");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<SimulationItemsAccessor>");
}

static void rna_def_geo_simulation_state_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeGeometrySimulationOutputItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of simulation items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "SimulationStateItem", "SimulationItemsAccessor");
  rna_def_node_item_array_common_functions(srna, "SimulationStateItem", "SimulationItemsAccessor");
}

static void def_geo_simulation_output(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_geo_simulation_state_item(brna);
  rna_def_geo_simulation_state_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeGeometrySimulationOutput", "storage");

  prop = RNA_def_property(srna, "state_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_num");
  RNA_def_property_struct_type(prop, "SimulationStateItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeGeometrySimulationOutputItems");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SimulationStateItem");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Node_ItemArray_active_get<SimulationItemsAccessor>",
                                 "rna_Node_ItemArray_active_set<SimulationItemsAccessor>",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NO_DEG_UPDATE);
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_update(prop, NC_NODE, nullptr);
}

static void rna_def_geo_repeat_item(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "RepeatItem", nullptr);
  RNA_def_struct_ui_text(srna, "Repeat Item", "");
  RNA_def_struct_sdna(srna, "NodeRepeatItem");

  rna_def_node_item_array_socket_item_common(srna, "RepeatItemsAccessor", true);
}

static void rna_def_geo_repeat_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeGeometryRepeatOutputItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of repeat items");

  rna_def_node_item_array_new_with_socket_and_name(srna, "RepeatItem", "RepeatItemsAccessor");
  rna_def_node_item_array_common_functions(srna, "RepeatItem", "RepeatItemsAccessor");
}

static void def_geo_repeat_output(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_geo_repeat_item(brna);
  rna_def_geo_repeat_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeGeometryRepeatOutput", "storage");

  prop = RNA_def_property(srna, "repeat_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_num");
  RNA_def_property_struct_type(prop, "RepeatItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeGeometryRepeatOutputItems");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "RepeatItem");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Node_ItemArray_active_get<RepeatItemsAccessor>",
                                 "rna_Node_ItemArray_active_set<RepeatItemsAccessor>",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NO_DEG_UPDATE);
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "inspection_index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_range(prop, 0, INT32_MAX, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Inspection Index",
                           "Iteration index that is used by inspection features like the viewer "
                           "node or socket inspection");
  RNA_def_property_update(prop, NC_NODE, "rna_Node_update");
}

static void rna_def_geo_viewer_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "NodeGeometryViewerItem", nullptr);
  RNA_def_struct_ui_text(srna, "Viewer Item", "");
  RNA_def_struct_sdna(srna, "NodeGeometryViewerItem");

  rna_def_node_item_array_socket_item_common(srna, "GeoViewerItemsAccessor", true);

  prop = RNA_def_property(srna, "auto_remove", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_GEO_VIEWER_ITEM_FLAG_AUTO_REMOVE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Auto Remove", "Remove the item automatically when it is unlinked");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<GeoViewerItemsAccessor>");
}

static void rna_def_geo_viewer_items(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "NodeGeometryViewerItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of viewer items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeGeometryViewerItem", "GeoViewerItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "NodeGeometryViewerItem", "GeoViewerItemsAccessor");
}

static void rna_def_geo_viewer(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_geo_viewer_item(brna);
  rna_def_geo_viewer_items(brna);

  prop = RNA_def_property(srna, "ui_shortcut", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_int_funcs(prop, nullptr, "rna_Node_Viewer_shortcut_node_set", nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_int_default(prop, NODE_VIEWER_SHORTCUT_NONE);
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  RNA_def_struct_sdna_from(srna, "NodeGeometryViewer", "storage");

  prop = RNA_def_property(srna, "viewer_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_num");
  RNA_def_property_struct_type(prop, "NodeGeometryViewerItem");
  RNA_def_property_ui_text(prop, "Viewer Items", "");
  RNA_def_property_srna(prop, "NodeGeometryViewerItems");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "NodeGeometryViewerItem");
  RNA_def_property_ui_text(prop, "Active Item", "");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NO_DEG_UPDATE);
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Node_ItemArray_active_get<GeoViewerItemsAccessor>",
                                 "rna_Node_ItemArray_active_set<GeoViewerItemsAccessor>",
                                 nullptr,
                                 nullptr);

  prop = RNA_def_property(srna, "domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_with_auto_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Domain", "Domain to evaluate fields on");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void rna_def_geo_foreach_geometry_element_input_item(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "ForeachGeometryElementInputItem", nullptr);
  RNA_def_struct_ui_text(srna, "For Each Geometry Element Item", "");
  RNA_def_struct_sdna(srna, "NodeForeachGeometryElementInputItem");

  rna_def_node_item_array_socket_item_common(
      srna, "ForeachGeometryElementInputItemsAccessor", true);
}

static void rna_def_geo_foreach_geometry_element_input_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeGeometryForeachGeometryElementInputItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Input Items", "Collection of input items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "ForeachGeometryElementInputItem", "ForeachGeometryElementInputItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "ForeachGeometryElementInputItem", "ForeachGeometryElementInputItemsAccessor");
}

static void rna_def_geo_foreach_geometry_element_main_item(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "ForeachGeometryElementMainItem", nullptr);
  RNA_def_struct_ui_text(srna, "For Each Geometry Element Item", "");
  RNA_def_struct_sdna(srna, "NodeForeachGeometryElementMainItem");

  rna_def_node_item_array_socket_item_common(
      srna, "ForeachGeometryElementMainItemsAccessor", true);
}

static void rna_def_geo_foreach_geometry_element_main_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeGeometryForeachGeometryElementMainItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Main Items", "Collection of main items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "ForeachGeometryElementMainItem", "ForeachGeometryElementMainItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "ForeachGeometryElementMainItem", "ForeachGeometryElementMainItemsAccessor");
}

static void rna_def_geo_foreach_geometry_element_generation_item(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ForeachGeometryElementGenerationItem", nullptr);
  RNA_def_struct_ui_text(srna, "For Each Geometry Element Item", "");
  RNA_def_struct_sdna(srna, "NodeForeachGeometryElementGenerationItem");

  rna_def_node_item_array_socket_item_common(
      srna, "ForeachGeometryElementGenerationItemsAccessor", true);

  prop = RNA_def_property(srna, "domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Domain", "Domain that the field is evaluated on");
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_update(
      prop,
      NC_NODE | NA_EDITED,
      "rna_Node_ItemArray_item_update<ForeachGeometryElementGenerationItemsAccessor>");
}

static void rna_def_geo_foreach_geometry_element_generation_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeGeometryForeachGeometryElementGenerationItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Generation Items", "Collection of generation items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna,
      "ForeachGeometryElementGenerationItem",
      "ForeachGeometryElementGenerationItemsAccessor");
  rna_def_node_item_array_common_functions(srna,
                                           "ForeachGeometryElementGenerationItem",
                                           "ForeachGeometryElementGenerationItemsAccessor");
}

static void def_geo_foreach_geometry_element_output(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_geo_foreach_geometry_element_input_item(brna);
  rna_def_geo_foreach_geometry_element_input_items(brna);

  rna_def_geo_foreach_geometry_element_main_item(brna);
  rna_def_geo_foreach_geometry_element_main_items(brna);

  rna_def_geo_foreach_geometry_element_generation_items(brna);
  rna_def_geo_foreach_geometry_element_generation_item(brna);

  RNA_def_struct_sdna_from(srna, "NodeGeometryForeachGeometryElementOutput", "storage");

  prop = RNA_def_property(srna, "input_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "input_items.items", "input_items.items_num");
  RNA_def_property_struct_type(prop, "ForeachGeometryElementInputItem");
  RNA_def_property_srna(prop, "NodeGeometryForeachGeometryElementInputItems");

  prop = RNA_def_property(srna, "main_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "main_items.items", "main_items.items_num");
  RNA_def_property_struct_type(prop, "ForeachGeometryElementMainItem");
  RNA_def_property_srna(prop, "NodeGeometryForeachGeometryElementMainItems");

  prop = RNA_def_property(srna, "generation_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(
      prop, nullptr, "generation_items.items", "generation_items.items_num");
  RNA_def_property_struct_type(prop, "ForeachGeometryElementGenerationItem");
  RNA_def_property_srna(prop, "NodeGeometryForeachGeometryElementGenerationItems");

  prop = RNA_def_property(srna, "active_input_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "input_items.active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_generation_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "generation_items.active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_main_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "main_items.active_index");
  RNA_def_property_ui_text(prop, "Active Main Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Domain", "Geometry domain that is iterated over");
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "inspection_index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_range(prop, 0, INT32_MAX, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Inspection Index",
                           "Iteration index that is used by inspection features like the viewer "
                           "node or socket inspection");
  RNA_def_property_update(prop, NC_NODE, "rna_Node_update");
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

static void rna_def_geo_capture_attribute_item(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "NodeGeometryCaptureAttributeItem", nullptr);
  RNA_def_struct_ui_text(srna, "Capture Attribute Item", "");
  RNA_def_struct_sdna(srna, "NodeGeometryAttributeCaptureItem");

  rna_def_node_item_array_socket_item_common(srna, "CaptureAttributeItemsAccessor", false);
  PropertyRNA *prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_type_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_NodeGeometryCaptureAttributeItem_data_type_itemf");
  RNA_def_property_ui_text(prop, "Data Type", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<CaptureAttributeItemsAccessor>");
}

static void rna_def_geo_capture_attribute_items(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "NodeGeometryCaptureAttributeItems", nullptr);
  RNA_def_struct_ui_text(srna, "Items", "Collection of capture attribute items");
  RNA_def_struct_sdna(srna, "bNode");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeGeometryCaptureAttributeItem", "CaptureAttributeItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "NodeGeometryCaptureAttributeItem", "CaptureAttributeItemsAccessor");
}

static void rna_def_geo_capture_attribute(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_geo_capture_attribute_item(brna);
  rna_def_geo_capture_attribute_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeGeometryAttributeCapture", "storage");

  prop = RNA_def_property(srna, "capture_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "capture_items", "capture_items_num");
  RNA_def_property_struct_type(prop, "NodeGeometryCaptureAttributeItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeGeometryCaptureAttributeItems");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "RepeatItem");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Node_ItemArray_active_get<CaptureAttributeItemsAccessor>",
                                 "rna_Node_ItemArray_active_set<CaptureAttributeItemsAccessor>",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NO_DEG_UPDATE);
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Domain", "Which domain to store the data in");
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_GeometryNodeAttributeDomain_attribute_domain_itemf");
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

static void rna_def_geo_bake_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "NodeGeometryBakeItem", nullptr);
  RNA_def_struct_ui_text(srna, "Bake Item", "");

  rna_def_node_item_array_socket_item_common(srna, "BakeItemsAccessor", true);

  prop = RNA_def_property(srna, "attribute_domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_GeometryNodeAttributeDomain_attribute_domain_itemf");
  RNA_def_property_ui_text(prop,
                           "Attribute Domain",
                           "Attribute domain where the attribute is stored in the baked data");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<BakeItemsAccessor>");

  prop = RNA_def_property(srna, "is_attribute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_BAKE_ITEM_IS_ATTRIBUTE);
  RNA_def_property_ui_text(prop, "Is Attribute", "Bake item is an attribute stored on a geometry");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<BakeItemsAccessor>");
}

static void rna_def_geo_bake_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeGeometryBakeItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of bake items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeGeometryBakeItem", "BakeItemsAccessor");
  rna_def_node_item_array_common_functions(srna, "NodeGeometryBakeItem", "BakeItemsAccessor");
}

static void rna_def_geo_bake(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_geo_bake_item(brna);
  rna_def_geo_bake_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeGeometryBake", "storage");

  prop = RNA_def_property(srna, "bake_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_num");
  RNA_def_property_struct_type(prop, "NodeGeometryBakeItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeGeometryBakeItems");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "RepeatItem");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Node_ItemArray_active_get<BakeItemsAccessor>",
                                 "rna_Node_ItemArray_active_set<BakeItemsAccessor>",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NO_DEG_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_update(prop, NC_NODE, nullptr);
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

static void rna_def_index_switch_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "IndexSwitchItem", nullptr);
  RNA_def_struct_ui_text(srna, "Index Switch Item", "");
  RNA_def_struct_sdna(srna, "IndexSwitchItem");

  prop = RNA_def_property(srna, "identifier", PROP_INT, PROP_NONE);
  RNA_def_property_ui_range(prop, 0, INT32_MAX, 1, -1);
  RNA_def_property_ui_text(prop, "Identifier", "Consistent identifier used for the item");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_NODE, "rna_Node_update");
}

static void rna_def_geo_index_switch_items(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "NodeIndexSwitchItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of index_switch items");

  func = RNA_def_function(srna, "new", "rna_NodeIndexSwitchItems_new");
  RNA_def_function_ui_description(func, "Add an item at the end");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  /* Return value. */
  parm = RNA_def_pointer(func, "item", "IndexSwitchItem", "Item", "New item");
  RNA_def_function_return(func, parm);

  rna_def_node_item_array_common_functions(srna, "IndexSwitchItem", "IndexSwitchItemsAccessor");
}

static void def_geo_index_switch(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_index_switch_item(brna);
  rna_def_geo_index_switch_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeIndexSwitch", "storage");

  prop = RNA_def_property(srna, "index_switch_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_num");
  RNA_def_property_struct_type(prop, "IndexSwitchItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeIndexSwitchItems");
}

static void rna_def_geo_field_to_grid_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "GeometryNodeFieldToGridItem", nullptr);
  RNA_def_struct_ui_text(srna, "Field to Grid Item", "");
  RNA_def_struct_sdna(srna, "GeometryNodeFieldToGridItem");

  rna_def_node_item_array_socket_item_common(srna, "FieldToGridItemsAccessor", false);
  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_data_type_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_NodeFieldToGridItem_data_type_itemf");
  RNA_def_property_ui_text(prop, "Data Type", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<FieldToGridItemsAccessor>");

  prop = RNA_def_property(srna, "identifier", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_geo_field_to_grid_items(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "GeometryNodeFieldToGridItems", nullptr);
  RNA_def_struct_ui_text(srna, "Items", "Collection of field to grid items");
  RNA_def_struct_sdna(srna, "bNode");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "GeometryNodeFieldToGridItem", "FieldToGridItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "GeometryNodeFieldToGridItem", "FieldToGridItemsAccessor");
}

static void def_geo_field_to_grid(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_geo_field_to_grid_item(brna);
  rna_def_geo_field_to_grid_items(brna);

  RNA_def_struct_sdna_from(srna, "GeometryNodeFieldToGrid", "storage");

  prop = RNA_def_property(srna, "grid_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_num");
  RNA_def_property_struct_type(prop, "GeometryNodeFieldToGridItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "GeometryNodeFieldToGridItems");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "RepeatItem");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Node_ItemArray_active_get<FieldToGridItemsAccessor>",
                                 "rna_Node_ItemArray_active_set<FieldToGridItemsAccessor>",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NO_DEG_UPDATE);
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_data_type_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_NodeFieldToGridItem_data_type_itemf");
  RNA_def_property_ui_text(prop, "Data Type", "Data type for topology grid");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
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

static void def_geo_curve_handle_type_selection(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeGeometryCurveSelectHandles", "storage");

  prop = RNA_def_property(srna, "handle_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "handle_type");
  RNA_def_property_ui_text(prop, "Handle Type", "");
  RNA_def_property_enum_items(prop, rna_node_geometry_curve_handle_type_items);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_enum_items(prop, rna_enum_node_geometry_curve_handle_side_items);
  RNA_def_property_ui_text(prop, "Mode", "Whether to check the type of left and right handles");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
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

static void def_geo_sample_index(BlenderRNA * /*brna*/, StructRNA *srna)
{
  using namespace blender;
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeGeometrySampleIndex", "storage");

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_type_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_GeometryNodeAttributeType_type_with_socket_itemf");
  RNA_def_property_enum_default(prop, CD_PROP_FLOAT);
  RNA_def_property_ui_text(prop, "Data Type", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_GeometryNodeAttributeDomain_attribute_domain_itemf");
  RNA_def_property_enum_default(prop, int(bke::AttrDomain::Point));
  RNA_def_property_ui_text(prop, "Domain", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "clamp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Clamp",
                           "Clamp the indices to the size of the attribute domain instead of "
                           "outputting a default value for invalid indices");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_geo_input_material(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_geo_input_collection(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Collection", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_geo_input_normal(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop = RNA_def_property(srna, "legacy_corner_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(
      prop,
      "Flat Corner Normals",
      "Always use face normals for the face corner domain, matching old behavior of the node");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_geo_input_object(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Object", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_geo_image(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_geo_string_to_curves(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem rna_node_geometry_string_to_curves_overflow_items[] = {
      {GEO_NODE_STRING_TO_CURVES_MODE_OVERFLOW,
       "OVERFLOW",
       ICON_NONE,
       "Overflow",
       "Let the text use more space than the specified height"},
      {GEO_NODE_STRING_TO_CURVES_MODE_SCALE_TO_FIT,
       "SCALE_TO_FIT",
       ICON_NONE,
       "Scale To Fit",
       "Scale the text size to fit inside the width and height"},
      {GEO_NODE_STRING_TO_CURVES_MODE_TRUNCATE,
       "TRUNCATE",
       ICON_NONE,
       "Truncate",
       "Only output curves that fit within the width and height. Output the remainder to the "
       "\"Remainder\" output."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem rna_node_geometry_string_to_curves_align_x_items[] = {
      {GEO_NODE_STRING_TO_CURVES_ALIGN_X_LEFT,
       "LEFT",
       ICON_ALIGN_LEFT,
       "Left",
       "Align text to the left"},
      {GEO_NODE_STRING_TO_CURVES_ALIGN_X_CENTER,
       "CENTER",
       ICON_ALIGN_CENTER,
       "Center",
       "Align text to the center"},
      {GEO_NODE_STRING_TO_CURVES_ALIGN_X_RIGHT,
       "RIGHT",
       ICON_ALIGN_RIGHT,
       "Right",
       "Align text to the right"},
      {GEO_NODE_STRING_TO_CURVES_ALIGN_X_JUSTIFY,
       "JUSTIFY",
       ICON_ALIGN_JUSTIFY,
       "Justify",
       "Align text to the left and the right"},
      {GEO_NODE_STRING_TO_CURVES_ALIGN_X_FLUSH,
       "FLUSH",
       ICON_ALIGN_FLUSH,
       "Flush",
       "Align text to the left and the right, with equal character spacing"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem rna_node_geometry_string_to_curves_align_y_items[] = {
      {GEO_NODE_STRING_TO_CURVES_ALIGN_Y_TOP,
       "TOP",
       ICON_ALIGN_TOP,
       "Top",
       "Align text to the top"},
      {GEO_NODE_STRING_TO_CURVES_ALIGN_Y_TOP_BASELINE,
       "TOP_BASELINE",
       ICON_ALIGN_TOP,
       "Top Baseline",
       "Align text to the top line's baseline"},
      {GEO_NODE_STRING_TO_CURVES_ALIGN_Y_MIDDLE,
       "MIDDLE",
       ICON_ALIGN_MIDDLE,
       "Middle",
       "Align text to the middle"},
      {GEO_NODE_STRING_TO_CURVES_ALIGN_Y_BOTTOM_BASELINE,
       "BOTTOM_BASELINE",
       ICON_ALIGN_BOTTOM,
       "Bottom Baseline",
       "Align text to the bottom line's baseline"},
      {GEO_NODE_STRING_TO_CURVES_ALIGN_Y_BOTTOM,
       "BOTTOM",
       ICON_ALIGN_BOTTOM,
       "Bottom",
       "Align text to the bottom"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem rna_node_geometry_string_to_curves_pivot_mode[] = {
      {GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_MIDPOINT, "MIDPOINT", 0, "Midpoint", "Midpoint"},
      {GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_LEFT, "TOP_LEFT", 0, "Top Left", "Top Left"},
      {GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_CENTER,
       "TOP_CENTER",
       0,
       "Top Center",
       "Top Center"},
      {GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_RIGHT, "TOP_RIGHT", 0, "Top Right", "Top Right"},
      {GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_LEFT,
       "BOTTOM_LEFT",
       0,
       "Bottom Left",
       "Bottom Left"},
      {GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_CENTER,
       "BOTTOM_CENTER",
       0,
       "Bottom Center",
       "Bottom Center"},
      {GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_RIGHT,
       "BOTTOM_RIGHT",
       0,
       "Bottom Right",
       "Bottom Right"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  prop = RNA_def_property(srna, "font", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "VectorFont");
  RNA_def_property_ui_text(
      prop, "Font", "Font of the text. Falls back to the UI font by default.");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeGeometryStringToCurves", "storage");

  prop = RNA_def_property(srna, "overflow", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "overflow");
  RNA_def_property_enum_items(prop, rna_node_geometry_string_to_curves_overflow_items);
  RNA_def_property_enum_default(prop, GEO_NODE_STRING_TO_CURVES_MODE_OVERFLOW);
  RNA_def_property_ui_text(
      prop, "Textbox Overflow", "Handle the text behavior when it does not fit in the text boxes");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "align_x", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "align_x");
  RNA_def_property_enum_items(prop, rna_node_geometry_string_to_curves_align_x_items);
  RNA_def_property_enum_default(prop, GEO_NODE_STRING_TO_CURVES_ALIGN_X_LEFT);
  RNA_def_property_ui_text(prop,
                           "Horizontal Alignment",
                           "Text horizontal alignment from the object or text box center");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "align_y", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "align_y");
  RNA_def_property_enum_items(prop, rna_node_geometry_string_to_curves_align_y_items);
  RNA_def_property_enum_default(prop, GEO_NODE_STRING_TO_CURVES_ALIGN_Y_TOP_BASELINE);
  RNA_def_property_ui_text(
      prop, "Vertical Alignment", "Text vertical alignment from the object center");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "pivot_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "pivot_mode");
  RNA_def_property_enum_items(prop, rna_node_geometry_string_to_curves_pivot_mode);
  RNA_def_property_enum_default(prop, GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_LEFT);
  RNA_def_property_ui_text(prop, "Pivot Point", "Pivot point position relative to character");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void rna_def_geo_menu_switch_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "NodeEnumItem", nullptr);
  RNA_def_struct_ui_text(srna, "Enum Item", "");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, nullptr, nullptr, "rna_Node_ItemArray_item_name_set<MenuSwitchItemsAccessor>");
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<MenuSwitchItemsAccessor>");

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "description");
  RNA_def_property_ui_text(prop, "Description", "");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_Node_ItemArray_item_update<MenuSwitchItemsAccessor>");
}

static void rna_def_geo_menu_switch_items(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "NodeMenuSwitchItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(
      srna, "Enum Definition Items", "Collection of items that make up an enum");

  func = RNA_def_function(srna, "new", "rna_NodeMenuSwitchItems_new");
  RNA_def_function_ui_description(func, "Add an a new enum item");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_string(func, "name", nullptr, MAX_NAME, "Name", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return value */
  parm = RNA_def_pointer(func, "item", "NodeEnumItem", "Item", "New item");
  RNA_def_function_return(func, parm);

  rna_def_node_item_array_common_functions(srna, "NodeEnumItem", "MenuSwitchItemsAccessor");
}

static void def_geo_menu_switch(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_geo_menu_switch_item(brna);
  rna_def_geo_menu_switch_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeMenuSwitch", "storage");

  prop = RNA_def_property(srna, "enum_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(
      prop, nullptr, "enum_definition.items_array", "enum_definition.items_num");
  RNA_def_property_struct_type(prop, "NodeEnumItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeMenuSwitchItems");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "enum_definition.active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "NodeEnumItem");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Node_ItemArray_active_get<MenuSwitchItemsAccessor>",
                                 "rna_Node_ItemArray_active_set<MenuSwitchItemsAccessor>",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Item", "Active item");
  RNA_def_property_update(prop, NC_NODE, nullptr);

  /* This exists only for backward compatibility. */
  prop = RNA_def_property(srna, "enum_definition", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Node");
  RNA_def_property_pointer_funcs(
      prop, "rna_NodeMenuSwitch_enum_definition_get", nullptr, nullptr, nullptr);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(prop,
                           "Enum Definition (deprecated)",
                           "The enum definition can now be accessed directly on the node. This "
                           "exists for backward compatibility.");
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
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "CompositorNode", "NodeInternal");
  RNA_def_struct_ui_text(srna, "Compositor Node", "");
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_register_funcs(
      srna, "rna_CompositorNode_register", "rna_Node_unregister", nullptr);

  /* compositor node need_exec flag */
  func = RNA_def_function(srna, "tag_need_exec", "rna_CompositorNode_tag_need_exec");
  RNA_def_function_ui_description(func, "Tag the node for compositor update");

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

static void rna_def_node(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem warning_propagation_items[] = {
      {NODE_WARNING_PROPAGATION_ALL, "ALL", 0, "All", ""},
      {NODE_WARNING_PROPAGATION_NONE, "NONE", 0, "None", ""},
      {NODE_WARNING_PROPAGATION_ONLY_ERRORS, "ERRORS", 0, "Errors", ""},
      {NODE_WARNING_PROPAGATION_ONLY_ERRORS_AND_WARNINGS,
       "ERRORS_AND_WARNINGS",
       0,
       "Errors and Warnings",
       ""},
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
  RNA_def_property_ui_text(prop, "Height", "Height of the node");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "dimensions", PROP_FLOAT, PROP_XYZ_LENGTH);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_funcs(prop, "rna_Node_dimensions_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Dimensions", "Absolute bounding box dimensions of the node");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Unique node identifier");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_Node_name_set");
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
  RNA_def_property_struct_type(prop, "NodeLink");
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
  RNA_def_property_ui_text(prop, "Parent", "Parent this node is attached to");

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
  RNA_def_property_ui_text(prop, "Show Options", "");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "show_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_PREVIEW);
  RNA_def_property_ui_text(prop, "Show Preview", "");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_COLLAPSED);
  RNA_def_property_ui_text(prop, "Collapse", "");
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
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->width");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_width_min", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->minwidth");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_width_max", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->maxwidth");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_height_default", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->height");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_height_min", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->minheight");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_height_max", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "typeinfo->minheight");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

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
  RNA_def_property_int_default(prop, GROUP_NODE_DEFAULT_WIDTH);
  RNA_def_property_range(prop, GROUP_NODE_MIN_WIDTH, GROUP_NODE_MAX_WIDTH);
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
  RNA_def_property_ui_text(
      prop, "Viewer Region", "Use boundaries for viewer nodes and composite backdrop");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update");
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

static StructRNA *define_specific_node(BlenderRNA *brna,
                                       const char *struct_name,
                                       const char *base_name)
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

static void rna_def_node_instance_hash(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeInstanceHash", nullptr);
  RNA_def_struct_ui_text(srna, "Node Instance Hash", "Hash table containing node instance data");

  /* XXX This type is a stub for now, only used to store instance hash in the context.
   * Eventually could use a StructRNA pointer to define a specific data type
   * and expose lookup functions.
   */
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

  define("NodeInternal", "NodeClosureInput", def_closure_input);
  define("NodeInternal", "NodeClosureOutput", def_closure_output);
  define("NodeInternal", "NodeCombineBundle", def_combine_bundle);
  define("NodeInternal", "NodeEnableOutput");
  define("NodeInternal", "NodeEvaluateClosure", def_evaluate_closure);
  define("NodeInternal", "NodeJoinBundle");
  define("NodeInternal", "NodeSeparateBundle", def_separate_bundle);

  define("ShaderNode", "ShaderNodeAddShader");
  define("ShaderNode", "ShaderNodeAmbientOcclusion", def_sh_ambient_occlusion);
  define("ShaderNode", "ShaderNodeAttribute", def_sh_attribute);
  define("ShaderNode", "ShaderNodeBackground");
  define("ShaderNode", "ShaderNodeBevel", def_sh_bevel);
  define("ShaderNode", "ShaderNodeBlackbody");
  define("ShaderNode", "ShaderNodeBrightContrast");
  define("ShaderNode", "ShaderNodeBsdfAnisotropic", def_glossy);
  define("ShaderNode", "ShaderNodeBsdfDiffuse");
  define("ShaderNode", "ShaderNodeBsdfGlass", def_glass);
  define("ShaderNode", "ShaderNodeBsdfHair", def_hair);
  define("ShaderNode", "ShaderNodeBsdfHairPrincipled", def_hair_principled);
  define("ShaderNode", "ShaderNodeBsdfMetallic", def_metallic);
  define("ShaderNode", "ShaderNodeBsdfPrincipled", def_principled);
  define("ShaderNode", "ShaderNodeBsdfRayPortal");
  define("ShaderNode", "ShaderNodeBsdfRefraction", def_refraction);
  define("ShaderNode", "ShaderNodeBsdfSheen", def_sheen);
  define("ShaderNode", "ShaderNodeBsdfToon", def_toon);
  define("ShaderNode", "ShaderNodeBsdfTranslucent");
  define("ShaderNode", "ShaderNodeBsdfTransparent");
  define("ShaderNode", "ShaderNodeBump", def_sh_bump);
  define("ShaderNode", "ShaderNodeCameraData");
  define("ShaderNode", "ShaderNodeClamp", def_clamp);
  define("ShaderNode", "ShaderNodeCombineColor", def_sh_combsep_color);
  define("ShaderNode", "ShaderNodeCombineXYZ");
  define("ShaderNode", "ShaderNodeDisplacement", def_sh_displacement);
  define("ShaderNode", "ShaderNodeEeveeSpecular");
  define("ShaderNode", "ShaderNodeEmission");
  define("ShaderNode", "ShaderNodeFloatCurve", def_float_curve);
  define("ShaderNode", "ShaderNodeFresnel");
  define("ShaderNode", "ShaderNodeGamma");
  define("ShaderNode", "ShaderNodeHairInfo");
  define("ShaderNode", "ShaderNodeHoldout");
  define("ShaderNode", "ShaderNodeHueSaturation");
  define("ShaderNode", "ShaderNodeInvert");
  define("ShaderNode", "ShaderNodeLayerWeight");
  define("ShaderNode", "ShaderNodeLightFalloff");
  define("ShaderNode", "ShaderNodeLightPath");
  define("ShaderNode", "ShaderNodeMapping", def_sh_mapping);
  define("ShaderNode", "ShaderNodeMapRange", def_map_range);
  define("ShaderNode", "ShaderNodeMath", def_math);
  define("ShaderNode", "ShaderNodeMix", def_sh_mix);
  define("ShaderNode", "ShaderNodeMixRGB", def_mix_rgb);
  define("ShaderNode", "ShaderNodeMixShader");
  define("ShaderNode", "ShaderNodeNewGeometry");
  define("ShaderNode", "ShaderNodeNormal");
  define("ShaderNode", "ShaderNodeNormalMap", def_sh_normal_map);
  define("ShaderNode", "ShaderNodeObjectInfo");
  define("ShaderNode", "ShaderNodeOutputAOV", def_sh_output_aov);
  define("ShaderNode", "ShaderNodeOutputLight", def_sh_output);
  define("ShaderNode", "ShaderNodeOutputLineStyle", def_sh_output_linestyle);
  define("ShaderNode", "ShaderNodeOutputMaterial", def_sh_output);
  define("ShaderNode", "ShaderNodeOutputWorld", def_sh_output);
  define("ShaderNode", "ShaderNodeParticleInfo");
  define("ShaderNode", "ShaderNodePointInfo");
  define("ShaderNode", "ShaderNodeRadialTiling", def_sh_radial_tiling);
  define("ShaderNode", "ShaderNodeRGB");
  define("ShaderNode", "ShaderNodeRGBCurve", def_rgb_curve);
  define("ShaderNode", "ShaderNodeRGBToBW");
  define("ShaderNode", "ShaderNodeScript", def_sh_script);
  define("ShaderNode", "ShaderNodeSeparateColor", def_sh_combsep_color);
  define("ShaderNode", "ShaderNodeSeparateXYZ");
  define("ShaderNode", "ShaderNodeShaderToRGB");
  define("ShaderNode", "ShaderNodeSqueeze");
  define("ShaderNode", "ShaderNodeSubsurfaceScattering", def_sh_subsurface);
  define("ShaderNode", "ShaderNodeTangent", def_sh_tangent);
  define("ShaderNode", "ShaderNodeTexBrick", def_sh_tex_brick);
  define("ShaderNode", "ShaderNodeTexChecker", def_sh_tex_checker);
  define("ShaderNode", "ShaderNodeTexCoord", def_sh_tex_coord);
  define("ShaderNode", "ShaderNodeTexEnvironment", def_sh_tex_environment);
  define("ShaderNode", "ShaderNodeTexGabor", def_sh_tex_gabor);
  define("ShaderNode", "ShaderNodeTexGradient", def_sh_tex_gradient);
  define("ShaderNode", "ShaderNodeTexIES", def_sh_tex_ies);
  define("ShaderNode", "ShaderNodeTexImage", def_sh_tex_image);
  define("ShaderNode", "ShaderNodeTexMagic", def_sh_tex_magic);
  define("ShaderNode", "ShaderNodeTexNoise", def_sh_tex_noise);
  define("ShaderNode", "ShaderNodeTexSky", def_sh_tex_sky);
  define("ShaderNode", "ShaderNodeTexVoronoi", def_sh_tex_voronoi);
  define("ShaderNode", "ShaderNodeTexWave", def_sh_tex_wave);
  define("ShaderNode", "ShaderNodeTexWhiteNoise", def_sh_tex_white_noise);
  define("ShaderNode", "ShaderNodeUVAlongStroke", def_sh_uvalongstroke);
  define("ShaderNode", "ShaderNodeUVMap", def_sh_uvmap);
  define("ShaderNode", "ShaderNodeValToRGB", def_colorramp);
  define("ShaderNode", "ShaderNodeValue");
  define("ShaderNode", "ShaderNodeVectorCurve", def_vector_curve);
  define("ShaderNode", "ShaderNodeVectorDisplacement", def_sh_vector_displacement);
  define("ShaderNode", "ShaderNodeVectorMath", def_vector_math);
  define("ShaderNode", "ShaderNodeVectorRotate", def_sh_vector_rotate);
  define("ShaderNode", "ShaderNodeVectorTransform", def_sh_vect_transform);
  define("ShaderNode", "ShaderNodeVertexColor", def_sh_vertex_color);
  define("ShaderNode", "ShaderNodeVolumeAbsorption");
  define("ShaderNode", "ShaderNodeVolumeInfo");
  define("ShaderNode", "ShaderNodeVolumePrincipled");
  define("ShaderNode", "ShaderNodeVolumeScatter", def_scatter);
  define("ShaderNode", "ShaderNodeVolumeCoefficients", def_volume_coefficients);
  define("ShaderNode", "ShaderNodeWavelength");
  define("ShaderNode", "ShaderNodeWireframe", def_sh_tex_wireframe);

  define("CompositorNode", "CompositorNodeAlphaOver");
  define("CompositorNode", "CompositorNodeAntiAliasing");
  define("CompositorNode", "CompositorNodeBilateralblur");
  define("CompositorNode", "CompositorNodeBlur");
  define("CompositorNode", "CompositorNodeBokehBlur");
  define("CompositorNode", "CompositorNodeBokehImage");
  define("CompositorNode", "CompositorNodeBoxMask");
  define("CompositorNode", "CompositorNodeBrightContrast");
  define("CompositorNode", "CompositorNodeChannelMatte");
  define("CompositorNode", "CompositorNodeChromaMatte");
  define("CompositorNode", "CompositorNodeColorBalance", def_cmp_colorbalance);
  define("CompositorNode", "CompositorNodeColorCorrection");
  define("CompositorNode", "CompositorNodeColorMatte");
  define("CompositorNode", "CompositorNodeColorSpill");
  define("CompositorNode", "CompositorNodeConvolve");
  define("CompositorNode", "CompositorNodeCombineColor", def_cmp_combsep_color);
  define("CompositorNode", "CompositorNodeConvertColorSpace", def_cmp_convert_color_space);
  define("CompositorNode", "CompositorNodeConvertToDisplay", def_cmp_convert_to_display);
  define("CompositorNode", "CompositorNodeCornerPin");
  define("CompositorNode", "CompositorNodeCrop");
  define("CompositorNode", "CompositorNodeCryptomatte", def_cmp_cryptomatte_legacy);
  define("CompositorNode", "CompositorNodeCryptomatteV2", def_cmp_cryptomatte);
  define("CompositorNode", "CompositorNodeCurveRGB", def_rgb_curve);
  define("CompositorNode", "CompositorNodeDBlur");
  define("CompositorNode", "CompositorNodeDefocus", def_cmp_defocus);
  define("CompositorNode", "CompositorNodeDenoise");
  define("CompositorNode", "CompositorNodeDespeckle");
  define("CompositorNode", "CompositorNodeDiffMatte");
  define("CompositorNode", "CompositorNodeDilateErode");
  define("CompositorNode", "CompositorNodeDisplace");
  define("CompositorNode", "CompositorNodeDistanceMatte");
  define("CompositorNode", "CompositorNodeDoubleEdgeMask");
  define("CompositorNode", "CompositorNodeEllipseMask");
  define("CompositorNode", "CompositorNodeExposure");
  define("CompositorNode", "CompositorNodeFilter");
  define("CompositorNode", "CompositorNodeFlip");
  define("CompositorNode", "CompositorNodeGamma");
  define("CompositorNode", "CompositorNodeGlare");
  define("CompositorNode", "CompositorNodeHueCorrect", def_cmp_huecorrect);
  define("CompositorNode", "CompositorNodeHueSat");
  define("CompositorNode", "CompositorNodeIDMask");
  define("CompositorNode", "CompositorNodeImage", def_cmp_image);
  define("CompositorNode", "CompositorNodeImageCoordinates");
  define("CompositorNode", "CompositorNodeImageInfo");
  define("CompositorNode", "CompositorNodeInpaint");
  define("CompositorNode", "CompositorNodeInvert");
  define("CompositorNode", "CompositorNodeKeying");
  define("CompositorNode", "CompositorNodeKeyingScreen", def_cmp_keyingscreen);
  define("CompositorNode", "CompositorNodeKuwahara");
  define("CompositorNode", "CompositorNodeLensdist");
  define("CompositorNode", "CompositorNodeLevels");
  define("CompositorNode", "CompositorNodeLumaMatte");
  define("CompositorNode", "CompositorNodeMapUV");
  define("CompositorNode", "CompositorNodeMask", def_cmp_mask);
  define("CompositorNode", "CompositorNodeMovieClip", def_cmp_movieclip);
  define("CompositorNode", "CompositorNodeMovieDistortion", def_cmp_moviedistortion);
  define("CompositorNode", "CompositorNodeNormal");
  define("CompositorNode", "CompositorNodeNormalize");
  define("CompositorNode", "CompositorNodeOutputFile", def_cmp_file_output);
  define("CompositorNode", "CompositorNodePixelate");
  define("CompositorNode", "CompositorNodePlaneTrackDeform", def_cmp_planetrackdeform);
  define("CompositorNode", "CompositorNodePosterize");
  define("CompositorNode", "CompositorNodePremulKey");
  define("CompositorNode", "CompositorNodeRelativeToPixel");
  define("CompositorNode", "CompositorNodeRGB");
  define("CompositorNode", "CompositorNodeRGBToBW");
  define("CompositorNode", "CompositorNodeRLayers", def_cmp_render_layers);
  define("CompositorNode", "CompositorNodeRotate");
  define("CompositorNode", "CompositorNodeScale");
  define("CompositorNode", "CompositorNodeSceneTime");
  define("CompositorNode", "CompositorNodeSeparateColor", def_cmp_combsep_color);
  define("CompositorNode", "CompositorNodeSetAlpha");
  define("CompositorNode", "CompositorNodeSplit");
  define("CompositorNode", "CompositorNodeStabilize", def_cmp_stabilize2d);
  define("CompositorNode", "CompositorNodeSequencerStripInfo");
  define("CompositorNode", "CompositorNodeSwitch");
  define("CompositorNode", "CompositorNodeSwitchView");
  define("CompositorNode", "CompositorNodeTime", def_time);
  define("CompositorNode", "CompositorNodeTonemap");
  define("CompositorNode", "CompositorNodeTrackPos", def_cmp_trackpos);
  define("CompositorNode", "CompositorNodeTransform");
  define("CompositorNode", "CompositorNodeTranslate");
  define("CompositorNode", "CompositorNodeVecBlur");
  define("CompositorNode", "CompositorNodeViewer", def_cmp_viewer);
  define("CompositorNode", "CompositorNodeZcombine");

  define("TextureNode", "TextureNodeAt");
  define("TextureNode", "TextureNodeBricks", def_tex_bricks);
  define("TextureNode", "TextureNodeChecker");
  define("TextureNode", "TextureNodeCombineColor", def_tex_combsep_color);
  define("TextureNode", "TextureNodeCompose");
  define("TextureNode", "TextureNodeCoordinates");
  define("TextureNode", "TextureNodeCurveRGB", def_rgb_curve);
  define("TextureNode", "TextureNodeCurveTime", def_time);
  define("TextureNode", "TextureNodeDecompose");
  define("TextureNode", "TextureNodeDistance");
  define("TextureNode", "TextureNodeHueSaturation");
  define("TextureNode", "TextureNodeImage", def_tex_image);
  define("TextureNode", "TextureNodeInvert");
  define("TextureNode", "TextureNodeMath", def_math);
  define("TextureNode", "TextureNodeMixRGB", def_mix_rgb);
  define("TextureNode", "TextureNodeOutput", def_tex_output);
  define("TextureNode", "TextureNodeRGBToBW");
  define("TextureNode", "TextureNodeRotate");
  define("TextureNode", "TextureNodeScale");
  define("TextureNode", "TextureNodeSeparateColor", def_tex_combsep_color);
  define("TextureNode", "TextureNodeTexBlend");
  define("TextureNode", "TextureNodeTexClouds");
  define("TextureNode", "TextureNodeTexDistNoise");
  define("TextureNode", "TextureNodeTexMagic");
  define("TextureNode", "TextureNodeTexMarble");
  define("TextureNode", "TextureNodeTexMusgrave");
  define("TextureNode", "TextureNodeTexNoise");
  define("TextureNode", "TextureNodeTexStucci");
  define("TextureNode", "TextureNodeTexture", def_texture);
  define("TextureNode", "TextureNodeTexVoronoi");
  define("TextureNode", "TextureNodeTexWood");
  define("TextureNode", "TextureNodeTranslate");
  define("TextureNode", "TextureNodeValToNor");
  define("TextureNode", "TextureNodeValToRGB", def_colorramp);
  define("TextureNode", "TextureNodeViewer");

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
  define("FunctionNode", "FunctionNodeHashValue");
  define("FunctionNode", "FunctionNodeInputBool", def_fn_input_bool);
  define("FunctionNode", "FunctionNodeInputColor", def_fn_input_color);
  define("FunctionNode", "FunctionNodeInputInt", def_fn_input_int);
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
  define("FunctionNode", "FunctionNodeProjectPoint");
  define("FunctionNode", "FunctionNodeQuaternionToRotation");
  define("FunctionNode", "FunctionNodeRandomValue", def_fn_random_value);
  define("FunctionNode", "FunctionNodeReplaceString");
  define("FunctionNode", "FunctionNodeRotateEuler", def_fn_rotate_euler);
  define("FunctionNode", "FunctionNodeRotateRotation");
  define("FunctionNode", "FunctionNodeRotateVector");
  define("FunctionNode", "FunctionNodeRotationToAxisAngle");
  define("FunctionNode", "FunctionNodeRotationToEuler");
  define("FunctionNode", "FunctionNodeRotationToQuaternion");
  define("FunctionNode", "FunctionNodeSeparateColor");
  define("FunctionNode", "FunctionNodeSeparateMatrix");
  define("FunctionNode", "FunctionNodeSeparateTransform");
  define("FunctionNode", "FunctionNodeSliceString");
  define("FunctionNode", "FunctionNodeStringLength");
  define("FunctionNode", "FunctionNodeStringToValue");
  define("FunctionNode", "FunctionNodeTransformDirection");
  define("FunctionNode", "FunctionNodeTransformPoint");
  define("FunctionNode", "FunctionNodeTransposeMatrix");
  define("FunctionNode", "FunctionNodeValueToString");

  define("GeometryNode", "GeometryNodeAccumulateField");
  define("GeometryNode", "GeometryNodeAttributeDomainSize");
  define("GeometryNode", "GeometryNodeAttributeStatistic");
  define("GeometryNode", "GeometryNodeBake", rna_def_geo_bake);
  define("GeometryNode", "GeometryNodeBlurAttribute");
  define("GeometryNode", "GeometryNodeBoundBox");
  define("GeometryNode", "GeometryNodeCameraInfo");
  define("GeometryNode", "GeometryNodeCaptureAttribute", rna_def_geo_capture_attribute);

  define("GeometryNode", "GeometryNodeCollectionInfo");
  define("GeometryNode", "GeometryNodeConvexHull");
  define("GeometryNode", "GeometryNodeCornersOfEdge");
  define("GeometryNode", "GeometryNodeCornersOfFace");
  define("GeometryNode", "GeometryNodeCornersOfVertex");
  define("GeometryNode", "GeometryNodeCurveArc");
  define("GeometryNode", "GeometryNodeCurveEndpointSelection");
  define("GeometryNode", "GeometryNodeCurveHandleTypeSelection", def_geo_curve_handle_type_selection);
  define("GeometryNode", "GeometryNodeCurveLength");
  define("GeometryNode", "GeometryNodeCurveOfPoint");
  define("GeometryNode", "GeometryNodeCurvePrimitiveBezierSegment");
  define("GeometryNode", "GeometryNodeCurvePrimitiveCircle");
  define("GeometryNode", "GeometryNodeCurvePrimitiveLine");
  define("GeometryNode", "GeometryNodeCurvePrimitiveQuadrilateral");
  define("GeometryNode", "GeometryNodeCurveQuadraticBezier");
  define("GeometryNode", "GeometryNodeCurveSetHandles", def_geo_curve_set_handle_type);
  define("GeometryNode", "GeometryNodeCurveSpiral");
  define("GeometryNode", "GeometryNodeCurveSplineType");
  define("GeometryNode", "GeometryNodeCurveStar");
  define("GeometryNode", "GeometryNodeCurvesToGreasePencil");
  define("GeometryNode", "GeometryNodeCurveToMesh");
  define("GeometryNode", "GeometryNodeCurveToPoints");
  define("GeometryNode", "GeometryNodeDeformCurvesOnSurface");
  define("GeometryNode", "GeometryNodeDeleteGeometry");
  define("GeometryNode", "GeometryNodeDistributePointsInGrid");
  define("GeometryNode", "GeometryNodeDistributePointsInVolume");
  define("GeometryNode", "GeometryNodeDistributePointsOnFaces", def_geo_distribute_points_on_faces);
  define("GeometryNode", "GeometryNodeDualMesh");
  define("GeometryNode", "GeometryNodeDuplicateElements");
  define("GeometryNode", "GeometryNodeEdgePathsToCurves");
  define("GeometryNode", "GeometryNodeEdgePathsToSelection");
  define("GeometryNode", "GeometryNodeEdgesOfCorner");
  define("GeometryNode", "GeometryNodeEdgesOfVertex");
  define("GeometryNode", "GeometryNodeEdgesToFaceGroups");
  define("GeometryNode", "GeometryNodeExtrudeMesh");
  define("GeometryNode", "GeometryNodeFaceOfCorner");
  define("GeometryNode", "GeometryNodeFieldAtIndex");
  define("GeometryNode", "GeometryNodeFieldAverage");
  define("GeometryNode", "GeometryNodeFieldMinAndMax");
  define("GeometryNode", "GeometryNodeFieldOnDomain");
  define("GeometryNode", "GeometryNodeFieldToGrid", def_geo_field_to_grid);
  define("GeometryNode", "GeometryNodeFieldVariance");
  define("GeometryNode", "GeometryNodeFillCurve");
  define("GeometryNode", "GeometryNodeFilletCurve");
  define("GeometryNode", "GeometryNodeFlipFaces");
  define("GeometryNode", "GeometryNodeForeachGeometryElementInput", def_geo_foreach_geometry_element_input);
  define("GeometryNode", "GeometryNodeForeachGeometryElementOutput", def_geo_foreach_geometry_element_output);
  define("GeometryNode", "GeometryNodeGeometryToInstance");
  define("GeometryNode", "GeometryNodeGetNamedGrid");
  define("GeometryNode", "GeometryNodeGizmoDial");
  define("GeometryNode", "GeometryNodeGizmoLinear");
  define("GeometryNode", "GeometryNodeGizmoTransform", rna_def_geo_gizmo_transform);
  define("GeometryNode", "GeometryNodeGreasePencilToCurves");
  define("GeometryNode", "GeometryNodeGridAdvect");
  define("GeometryNode", "GeometryNodeGridCurl");
  define("GeometryNode", "GeometryNodeGridDivergence");
  define("GeometryNode", "GeometryNodeGridGradient");
  define("GeometryNode", "GeometryNodeGridInfo");
  define("GeometryNode", "GeometryNodeGridLaplacian");
  define("GeometryNode", "GeometryNodeGridPrune");
  define("GeometryNode", "GeometryNodeGridToMesh");
  define("GeometryNode", "GeometryNodeGridVoxelize");
  define("GeometryNode", "GeometryNodeImageInfo");
  define("GeometryNode", "GeometryNodeImageTexture", def_geo_image_texture);
  define("GeometryNode", "GeometryNodeImportCSV");
  define("GeometryNode", "GeometryNodeImportOBJ");
  define("GeometryNode", "GeometryNodeImportPLY");
  define("GeometryNode", "GeometryNodeImportSTL");
  define("GeometryNode", "GeometryNodeImportText");
  define("GeometryNode", "GeometryNodeImportVDB");
  define("GeometryNode", "GeometryNodeIndexOfNearest");
  define("GeometryNode", "GeometryNodeIndexSwitch", def_geo_index_switch);
  define("GeometryNode", "GeometryNodeInputActiveCamera");
  define("GeometryNode", "GeometryNodeInputCollection", def_geo_input_collection);
  define("GeometryNode", "GeometryNodeInputCurveHandlePositions");
  define("GeometryNode", "GeometryNodeInputCurveTilt");
  define("GeometryNode", "GeometryNodeInputEdgeSmooth");
  define("GeometryNode", "GeometryNodeInputID");
  define("GeometryNode", "GeometryNodeInputImage", def_geo_image);
  define("GeometryNode", "GeometryNodeInputIndex");
  define("GeometryNode", "GeometryNodeInputInstanceBounds");
  define("GeometryNode", "GeometryNodeInputInstanceRotation");
  define("GeometryNode", "GeometryNodeInputInstanceScale");
  define("GeometryNode", "GeometryNodeInputMaterial", def_geo_input_material);
  define("GeometryNode", "GeometryNodeInputMaterialIndex");
  define("GeometryNode", "GeometryNodeInputMeshEdgeAngle");
  define("GeometryNode", "GeometryNodeInputMeshEdgeNeighbors");
  define("GeometryNode", "GeometryNodeInputMeshEdgeVertices");
  define("GeometryNode", "GeometryNodeInputMeshFaceArea");
  define("GeometryNode", "GeometryNodeInputMeshFaceIsPlanar");
  define("GeometryNode", "GeometryNodeInputMeshFaceNeighbors");
  define("GeometryNode", "GeometryNodeInputMeshIsland");
  define("GeometryNode", "GeometryNodeInputMeshVertexNeighbors");
  define("GeometryNode", "GeometryNodeInputNamedAttribute");
  define("GeometryNode", "GeometryNodeInputNamedLayerSelection");
  define("GeometryNode", "GeometryNodeInputNormal", def_geo_input_normal);
  define("GeometryNode", "GeometryNodeInputObject", def_geo_input_object);
  define("GeometryNode", "GeometryNodeInputPosition");
  define("GeometryNode", "GeometryNodeInputRadius");
  define("GeometryNode", "GeometryNodeInputSceneTime");
  define("GeometryNode", "GeometryNodeInputShadeSmooth");
  define("GeometryNode", "GeometryNodeInputShortestEdgePaths");
  define("GeometryNode", "GeometryNodeInputSplineCyclic");
  define("GeometryNode", "GeometryNodeInputSplineResolution");
  define("GeometryNode", "GeometryNodeInputTangent");
  define("GeometryNode", "GeometryNodeInputVoxelIndex");
  define("GeometryNode", "GeometryNodeInstanceOnPoints");
  define("GeometryNode", "GeometryNodeInstancesToPoints");
  define("GeometryNode", "GeometryNodeInstanceTransform");
  define("GeometryNode", "GeometryNodeInterpolateCurves");
  define("GeometryNode", "GeometryNodeIsViewport");
  define("GeometryNode", "GeometryNodeJoinGeometry");
  define("GeometryNode", "GeometryNodeList");
  define("GeometryNode", "GeometryNodeListGetItem");
  define("GeometryNode", "GeometryNodeListLength");
  define("GeometryNode", "GeometryNodeMaterialSelection");
  define("GeometryNode", "GeometryNodeMenuSwitch", def_geo_menu_switch);
  define("GeometryNode", "GeometryNodeMergeByDistance");
  define("GeometryNode", "GeometryNodeMergeLayers");
  define("GeometryNode", "GeometryNodeMeshBoolean");
  define("GeometryNode", "GeometryNodeMeshCircle");
  define("GeometryNode", "GeometryNodeMeshCone");
  define("GeometryNode", "GeometryNodeMeshCube");
  define("GeometryNode", "GeometryNodeMeshCylinder");
  define("GeometryNode", "GeometryNodeMeshFaceSetBoundaries");
  define("GeometryNode", "GeometryNodeMeshGrid");
  define("GeometryNode", "GeometryNodeMeshIcoSphere");
  define("GeometryNode", "GeometryNodeMeshLine");
  define("GeometryNode", "GeometryNodeMeshToCurve");
  define("GeometryNode", "GeometryNodeMeshToDensityGrid");
  define("GeometryNode", "GeometryNodeMeshToPoints");
  define("GeometryNode", "GeometryNodeMeshToSDFGrid");
  define("GeometryNode", "GeometryNodeMeshToVolume");
  define("GeometryNode", "GeometryNodeMeshUVSphere");
  define("GeometryNode", "GeometryNodeObjectInfo");
  define("GeometryNode", "GeometryNodeOffsetCornerInFace");
  define("GeometryNode", "GeometryNodeOffsetPointInCurve");
  define("GeometryNode", "GeometryNodePoints");
  define("GeometryNode", "GeometryNodePointsOfCurve");
  define("GeometryNode", "GeometryNodePointsToCurves");
  define("GeometryNode", "GeometryNodePointsToSDFGrid");
  define("GeometryNode", "GeometryNodePointsToVertices");
  define("GeometryNode", "GeometryNodePointsToVolume");
  define("GeometryNode", "GeometryNodeProximity");
  define("GeometryNode", "GeometryNodeRaycast");
  define("GeometryNode", "GeometryNodeRealizeInstances");
  define("GeometryNode", "GeometryNodeRemoveAttribute");
  define("GeometryNode", "GeometryNodeRepeatInput", def_geo_repeat_input);
  define("GeometryNode", "GeometryNodeRepeatOutput", def_geo_repeat_output);
  define("GeometryNode", "GeometryNodeReplaceMaterial");
  define("GeometryNode", "GeometryNodeResampleCurve");
  define("GeometryNode", "GeometryNodeReverseCurve");
  define("GeometryNode", "GeometryNodeRotateInstances");
  define("GeometryNode", "GeometryNodeSampleCurve", def_geo_curve_sample);
  define("GeometryNode", "GeometryNodeSampleGrid");
  define("GeometryNode", "GeometryNodeSampleGridIndex");
  define("GeometryNode", "GeometryNodeSampleIndex", def_geo_sample_index);
  define("GeometryNode", "GeometryNodeSampleNearest");
  define("GeometryNode", "GeometryNodeSampleNearestSurface");
  define("GeometryNode", "GeometryNodeSampleUVSurface");
  define("GeometryNode", "GeometryNodeScaleElements");
  define("GeometryNode", "GeometryNodeScaleInstances");
  define("GeometryNode", "GeometryNodeSDFGridBoolean");
  define("GeometryNode", "GeometryNodeSDFGridFillet");
  define("GeometryNode", "GeometryNodeSDFGridLaplacian");
  define("GeometryNode", "GeometryNodeSDFGridMean");
  define("GeometryNode", "GeometryNodeSDFGridMeanCurvature");
  define("GeometryNode", "GeometryNodeSDFGridMedian");
  define("GeometryNode", "GeometryNodeSDFGridOffset");
  define("GeometryNode", "GeometryNodeSelfObject");
  define("GeometryNode", "GeometryNodeSeparateComponents");
  define("GeometryNode", "GeometryNodeSeparateGeometry");
  define("GeometryNode", "GeometryNodeSetCurveHandlePositions");
  define("GeometryNode", "GeometryNodeSetCurveNormal");
  define("GeometryNode", "GeometryNodeSetCurveRadius");
  define("GeometryNode", "GeometryNodeSetCurveTilt");
  define("GeometryNode", "GeometryNodeSetGeometryName");
  define("GeometryNode", "GeometryNodeSetGreasePencilColor");
  define("GeometryNode", "GeometryNodeSetGreasePencilDepth");
  define("GeometryNode", "GeometryNodeSetGreasePencilSoftness");
  define("GeometryNode", "GeometryNodeSetGridBackground");
  define("GeometryNode", "GeometryNodeSetGridTransform");
  define("GeometryNode", "GeometryNodeSetID");
  define("GeometryNode", "GeometryNodeSetInstanceTransform");
  define("GeometryNode", "GeometryNodeSetMaterial");
  define("GeometryNode", "GeometryNodeSetMaterialIndex");
  define("GeometryNode", "GeometryNodeSetMeshNormal");
  define("GeometryNode", "GeometryNodeSetPointRadius");
  define("GeometryNode", "GeometryNodeSetPosition");
  define("GeometryNode", "GeometryNodeSetShadeSmooth");
  define("GeometryNode", "GeometryNodeSetSplineCyclic");
  define("GeometryNode", "GeometryNodeSetSplineResolution");
  define("GeometryNode", "GeometryNodeSimulationInput", def_geo_simulation_input);
  define("GeometryNode", "GeometryNodeSimulationOutput", def_geo_simulation_output);
  define("GeometryNode", "GeometryNodeSortElements");
  define("GeometryNode", "GeometryNodeSplineLength");
  define("GeometryNode", "GeometryNodeSplineParameter");
  define("GeometryNode", "GeometryNodeSplitEdges");
  define("GeometryNode", "GeometryNodeSplitToInstances");
  define("GeometryNode", "GeometryNodeStoreNamedAttribute");
  define("GeometryNode", "GeometryNodeStoreNamedGrid");
  define("GeometryNode", "GeometryNodeStringJoin");
  define("GeometryNode", "GeometryNodeStringToCurves", def_geo_string_to_curves);
  define("GeometryNode", "GeometryNodeSubdivideCurve");
  define("GeometryNode", "GeometryNodeSubdivideMesh");
  define("GeometryNode", "GeometryNodeSubdivisionSurface");
  define("GeometryNode", "GeometryNodeSwitch");
  define("GeometryNode", "GeometryNodeTool3DCursor");
  define("GeometryNode", "GeometryNodeToolActiveElement");
  define("GeometryNode", "GeometryNodeToolFaceSet");
  define("GeometryNode", "GeometryNodeToolMousePosition");
  define("GeometryNode", "GeometryNodeToolSelection");
  define("GeometryNode", "GeometryNodeToolSetFaceSet");
  define("GeometryNode", "GeometryNodeToolSetSelection");
  define("GeometryNode", "GeometryNodeTransform");
  define("GeometryNode", "GeometryNodeTranslateInstances");
  define("GeometryNode", "GeometryNodeTriangulate");
  define("GeometryNode", "GeometryNodeTrimCurve");
  define("GeometryNode", "GeometryNodeUVPackIslands");
  define("GeometryNode", "GeometryNodeUVTangent");
  define("GeometryNode", "GeometryNodeUVUnwrap");
  define("GeometryNode", "GeometryNodeVertexOfCorner");
  define("GeometryNode", "GeometryNodeViewer", rna_def_geo_viewer);
  define("GeometryNode", "GeometryNodeViewportTransform");
  define("GeometryNode", "GeometryNodeVolumeCube");
  define("GeometryNode", "GeometryNodeVolumeToMesh");
  define("GeometryNode", "GeometryNodeWarning");


  /* Node group types are currently defined for each tree type individually. */
  define("ShaderNode", "ShaderNodeGroup", def_group);
  define("CompositorNode", "CompositorNodeGroup", def_group);
  define("TextureNode", "TextureNodeGroup", def_group);
  define("GeometryNode", "GeometryNodeGroup", def_group);

  /* clang-format on */
}

void RNA_def_nodetree(BlenderRNA *brna)
{
  rna_def_node(brna);
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

  rna_def_node_instance_hash(brna);
}

/* clean up macro definition */
#  undef NODE_DEFINE_SUBTYPES

#endif
