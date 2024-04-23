/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <climits>
#include <cstdlib>
#include <cstring>

#include <utility>

#include "BLI_function_ref.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_math_rotation.h"
#include "BLI_string.h"
#include "BLI_string_utf8_symbols.h"
#include "BLI_utildefines.h"

#include "BLF_api.hh"

#include "BLT_translation.hh"

#include "DNA_curves_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_text_types.h"
#include "DNA_texture_types.h"

#include "BKE_animsys.h"
#include "BKE_attribute.hh"
#include "BKE_cryptomatte.h"
#include "BKE_geometry_set.hh"
#include "BKE_image.h"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_texture.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"
#include "rna_internal_types.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "WM_types.hh"

#include "MEM_guardedalloc.h"

#include "RE_texture.h"

#include "NOD_composite.hh"
#include "NOD_geometry.hh"
#include "NOD_socket.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "BLI_string_utils.hh"

const EnumPropertyItem rna_enum_node_socket_in_out_items[] = {{SOCK_IN, "IN", 0, "Input", ""},
                                                              {SOCK_OUT, "OUT", 0, "Output", ""},
                                                              {0, nullptr, 0, nullptr, nullptr}};

const EnumPropertyItem rna_enum_node_socket_data_type_items[] = {
    {SOCK_FLOAT, "FLOAT", 0, "Float", ""},
    {SOCK_INT, "INT", 0, "Integer", ""},
    {SOCK_BOOLEAN, "BOOLEAN", 0, "Boolean", ""},
    {SOCK_VECTOR, "VECTOR", 0, "Vector", ""},
    {SOCK_ROTATION, "ROTATION", 0, "Rotation", ""},
    {SOCK_MATRIX, "MATRIX", 0, "Matrix", ""},
    {SOCK_STRING, "STRING", 0, "String", ""},
    {SOCK_MENU, "MENU", 0, "Menu", ""},
    {SOCK_RGBA, "RGBA", 0, "Color", ""},
    {SOCK_OBJECT, "OBJECT", 0, "Object", ""},
    {SOCK_IMAGE, "IMAGE", 0, "Image", ""},
    {SOCK_GEOMETRY, "GEOMETRY", 0, "Geometry", ""},
    {SOCK_COLLECTION, "COLLECTION", 0, "Collection", ""},
    {SOCK_TEXTURE, "TEXTURE", 0, "Texture", ""},
    {SOCK_MATERIAL, "MATERIAL", 0, "Material", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem node_quality_items[] = {
    {NTREE_QUALITY_HIGH, "HIGH", 0, "High", "High quality"},
    {NTREE_QUALITY_MEDIUM, "MEDIUM", 0, "Medium", "Medium quality"},
    {NTREE_QUALITY_LOW, "LOW", 0, "Low", "Low quality"},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

static const EnumPropertyItem rna_enum_execution_mode_items[] = {
    {NTREE_EXECUTION_MODE_CPU, "CPU", 0, "CPU", ""},
    {NTREE_EXECUTION_MODE_GPU, "GPU", 0, "GPU", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_precision_items[] = {
    {NODE_TREE_COMPOSITOR_PRECISION_AUTO,
     "AUTO",
     0,
     "Auto",
     "Full precision for final renders, half precision otherwise"},
    {NODE_TREE_COMPOSITOR_PRECISION_FULL, "FULL", 0, "Full", "Full precision"},
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
     "Transform a direction vector. Location is ignored"},
    {NODE_MAPPING_TYPE_NORMAL,
     "NORMAL",
     0,
     "Normal",
     "Transform a unit normal vector. Location is ignored"},
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
     "Reflect A around the normal B. B doesn't need to be normalized"},
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

const EnumPropertyItem rna_enum_node_float_compare_items[] = {
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
    {2, "2D", 0, "2D", "Use the 2D vector (X, Y) as input. The Z component is ignored"},
    {3, "3D", 0, "3D", "Use the 3D vector (X, Y, Z) as input"},
    {4, "4D", 0, "4D", "Use the 4D vector (X, Y, Z, W) as input"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_filter_items[] = {
    {0, "SOFTEN", 0, "Soften", ""},
    {1, "SHARPEN", 0, "Box Sharpen", "An aggressive sharpening filter"},
    {7, "SHARPEN_DIAMOND", 0, "Diamond Sharpen", "A moderate sharpening filter"},
    {2, "LAPLACE", 0, "Laplace", ""},
    {3, "SOBEL", 0, "Sobel", ""},
    {4, "PREWITT", 0, "Prewitt", ""},
    {5, "KIRSCH", 0, "Kirsch", ""},
    {6, "SHADOW", 0, "Shadow", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_node_geometry_curve_handle_type_items[] = {
    {GEO_NODE_CURVE_HANDLE_FREE,
     "FREE",
     ICON_HANDLE_FREE,
     "Free",
     "The handle can be moved anywhere, and doesn't influence the point's other handle"},
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
    {NODE_COMBSEP_COLOR_RGB, "RGB", ICON_NONE, "RGB", "Use RGB color processing"},
    {NODE_COMBSEP_COLOR_HSV, "HSV", ICON_NONE, "HSV", "Use HSV color processing"},
    {NODE_COMBSEP_COLOR_HSL, "HSL", ICON_NONE, "HSL", "Use HSL color processing"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_mix_data_type_items[] = {
    {SOCK_FLOAT, "FLOAT", 0, "Float", ""},
    {SOCK_VECTOR, "VECTOR", 0, "Vector", ""},
    {SOCK_RGBA, "RGBA", 0, "Color", ""},
    {SOCK_ROTATION, "ROTATION", 0, "Rotation", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_node_geometry_mesh_circle_fill_type_items[] = {
    {GEO_NODE_MESH_CIRCLE_FILL_NONE, "NONE", 0, "None", ""},
    {GEO_NODE_MESH_CIRCLE_FILL_NGON, "NGON", 0, "N-Gon", ""},
    {GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN, "TRIANGLE_FAN", 0, "Triangles", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem node_sampler_type_items[] = {
    {0, "NEAREST", 0, "Nearest", ""},
    {1, "BILINEAR", 0, "Bilinear", ""},
    {2, "BICUBIC", 0, "Bicubic", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

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

#  include "BLI_linklist.h"
#  include "BLI_string.h"

#  include "BKE_context.hh"
#  include "BKE_idprop.hh"

#  include "BKE_global.hh"

#  include "ED_node.hh"
#  include "ED_render.hh"

#  include "GPU_material.hh"

#  include "NOD_common.h"
#  include "NOD_composite.hh"
#  include "NOD_geo_bake.hh"
#  include "NOD_geo_index_switch.hh"
#  include "NOD_geo_repeat.hh"
#  include "NOD_geo_simulation.hh"
#  include "NOD_geometry.hh"
#  include "NOD_shader.h"
#  include "NOD_socket.hh"
#  include "NOD_socket_items.hh"
#  include "NOD_texture.h"

#  include "RE_engine.h"
#  include "RE_pipeline.h"

#  include "DNA_scene_types.h"
#  include "WM_api.hh"

using blender::nodes::BakeItemsAccessor;
using blender::nodes::IndexSwitchItemsAccessor;
using blender::nodes::RepeatItemsAccessor;
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
  int i = 0, result = -1;
  NODE_TREE_TYPES_BEGIN (nt) {
    if (STREQ(nt->idname, idname)) {
      result = i;
      break;
    }
    i++;
  }
  NODE_TREE_TYPES_END;
  return result;
}

bNodeTreeType *rna_node_tree_type_from_enum(int value)
{
  int i = 0;
  bNodeTreeType *result = nullptr;
  NODE_TREE_TYPES_BEGIN (nt) {
    if (i == value) {
      result = nt;
      break;
    }
    i++;
  }
  NODE_TREE_TYPES_END;
  return result;
}

const EnumPropertyItem *rna_node_tree_type_itemf(void *data,
                                                 bool (*poll)(void *data, bNodeTreeType *),
                                                 bool *r_free)
{
  EnumPropertyItem tmp = {0};
  EnumPropertyItem *item = nullptr;
  int totitem = 0, i = 0;

  NODE_TREE_TYPES_BEGIN (nt) {
    if (poll && !poll(data, nt)) {
      i++;
      continue;
    }

    tmp.value = i;
    tmp.identifier = nt->idname;
    tmp.icon = nt->ui_icon;
    tmp.name = nt->ui_name;
    tmp.description = nt->ui_description;

    RNA_enum_item_add(&item, &totitem, &tmp);

    i++;
  }
  NODE_TREE_TYPES_END;

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
  int i = 0, result = -1;
  NODE_SOCKET_TYPES_BEGIN (stype) {
    if (STREQ(stype->idname, idname)) {
      result = i;
      break;
    }
    i++;
  }
  NODE_SOCKET_TYPES_END;
  return result;
}

bNodeSocketType *rna_node_socket_type_from_enum(int value)
{
  int i = 0;
  bNodeSocketType *result = nullptr;
  NODE_SOCKET_TYPES_BEGIN (stype) {
    if (i == value) {
      result = stype;
      break;
    }
    i++;
  }
  NODE_SOCKET_TYPES_END;
  return result;
}

const EnumPropertyItem *rna_node_socket_type_itemf(void *data,
                                                   bool (*poll)(void *data, bNodeSocketType *),
                                                   bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0};
  int totitem = 0, i = 0;
  StructRNA *srna;

  NODE_SOCKET_TYPES_BEGIN (stype) {
    if (poll && !poll(data, stype)) {
      i++;
      continue;
    }

    srna = stype->ext_socket.srna;
    tmp.value = i;
    tmp.identifier = stype->idname;
    tmp.icon = RNA_struct_ui_icon(srna);
    tmp.name = nodeSocketTypeLabel(stype);
    tmp.description = RNA_struct_ui_description(srna);

    RNA_enum_item_add(&item, &totitem, &tmp);

    i++;
  }
  NODE_SOCKET_TYPES_END;

  if (totitem == 0) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static const EnumPropertyItem *rna_node_static_type_itemf(bContext * /*C*/,
                                                          PointerRNA *ptr,
                                                          PropertyRNA * /*prop*/,
                                                          bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp;
  int totitem = 0;

  /* hack, don't want to add include path to RNA just for this, since in the future RNA types
   * for nodes should be defined locally at runtime anyway ...
   */

  tmp.value = NODE_CUSTOM;
  tmp.identifier = "CUSTOM";
  tmp.name = N_("Custom");
  tmp.description = N_("Custom Node");
  tmp.icon = ICON_NONE;
  RNA_enum_item_add(&item, &totitem, &tmp);

  tmp.value = NODE_CUSTOM_GROUP;
  tmp.identifier = "CUSTOM GROUP";
  tmp.name = N_("CustomGroup");
  tmp.description = N_("Custom Group Node");
  tmp.icon = ICON_NONE;
  RNA_enum_item_add(&item, &totitem, &tmp);

  tmp.value = NODE_UNDEFINED;
  tmp.identifier = "UNDEFINED";
  tmp.name = N_("UNDEFINED");
  tmp.description = "";
  tmp.icon = ICON_NONE;
  RNA_enum_item_add(&item, &totitem, &tmp);

  const char *category = "";
  if (RNA_struct_is_a(ptr->type, &RNA_ShaderNode)) {
    category = "ShaderNode";
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_CompositorNode)) {
    category = "CompositorNode";
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_TextureNode)) {
    category = "TextureNode";
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_GeometryNode)) {
    category = "GeometryNode";
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_FunctionNode)) {
    category = "FunctionNode";
  }

  NODE_TYPES_BEGIN (ntype) {
    if (ntype->enum_name_legacy &&
        (BLI_str_startswith(ntype->idname, "Node") || BLI_str_startswith(ntype->idname, category)))
    {
      tmp.value = ntype->type;
      tmp.identifier = ntype->enum_name_legacy;
      tmp.name = ntype->ui_name;
      tmp.description = ntype->ui_description;
      tmp.icon = ICON_NONE;
      RNA_enum_item_add(&item, &totitem, &tmp);
    }
  }
  NODE_TYPES_END;

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

/* ******** Node Tree ******** */

static StructRNA *rna_NodeTree_refine(PointerRNA *ptr)
{
  bNodeTree *ntree = static_cast<bNodeTree *>(ptr->data);

  if (ntree->typeinfo->rna_ext.srna) {
    return ntree->typeinfo->rna_ext.srna;
  }
  else {
    return &RNA_NodeTree;
  }
}

static bool rna_NodeTree_poll(const bContext *C, bNodeTreeType *ntreetype)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool visible;

  PointerRNA ptr = RNA_pointer_create(nullptr, ntreetype->rna_ext.srna, nullptr); /* dummy */
  func = &rna_NodeTree_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

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

static void rna_NodeTree_get_from_context(
    const bContext *C, bNodeTreeType *ntreetype, bNodeTree **r_ntree, ID **r_id, ID **r_from)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret1, *ret2, *ret3;

  PointerRNA ptr = RNA_pointer_create(nullptr, ntreetype->rna_ext.srna, nullptr); /* dummy */
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

static bool rna_NodeTree_valid_socket_type(bNodeTreeType *ntreetype, bNodeSocketType *socket_type)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool valid;

  PointerRNA ptr = RNA_pointer_create(nullptr, ntreetype->rna_ext.srna, nullptr); /* dummy */
  func = &rna_NodeTree_valid_socket_type_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "idname", &socket_type->idname);
  ntreetype->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "valid", &ret);
  valid = *static_cast<bool *>(ret);

  RNA_parameter_list_free(&list);

  return valid;
}

static bool rna_NodeTree_unregister(Main * /*bmain*/, StructRNA *type)
{
  bNodeTreeType *nt = static_cast<bNodeTreeType *>(RNA_struct_blender_type_get(type));

  if (!nt) {
    return false;
  }

  RNA_struct_free_extension(type, &nt->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  ntreeTypeFreeLink(nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
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
  bNodeTreeType *nt, dummy_nt;
  bNodeTree dummy_ntree;
  bool have_function[4];

  /* setup dummy tree & tree type to store static properties in */
  memset(&dummy_nt, 0, sizeof(bNodeTreeType));
  memset(&dummy_ntree, 0, sizeof(bNodeTree));
  dummy_ntree.typeinfo = &dummy_nt;
  PointerRNA dummy_ntree_ptr = RNA_pointer_create(nullptr, &RNA_NodeTree, &dummy_ntree);

  /* validate the python class */
  if (validate(&dummy_ntree_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_nt.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering node tree class: '%s' is too long, maximum length is %d",
                identifier,
                int(sizeof(dummy_nt.idname)));
    return nullptr;
  }

  /* check if we have registered this tree type before, and remove it */
  nt = ntreeTypeFind(dummy_nt.idname);
  if (nt) {
    BKE_reportf(reports,
                RPT_INFO,
                "Registering node tree class: '%s', bl_idname '%s' has been registered before, "
                "unregistering previous",
                identifier,
                dummy_nt.idname);

    /* NOTE: unlike most types `nt->rna_ext.srna` doesn't need to be checked for nullptr. */
    if (!rna_NodeTree_unregister(bmain, nt->rna_ext.srna)) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Registering node tree class: '%s', bl_idname '%s' could not be unregistered",
                  identifier,
                  dummy_nt.idname);
      return nullptr;
    }
  }

  /* create a new node tree type */
  nt = static_cast<bNodeTreeType *>(MEM_mallocN(sizeof(bNodeTreeType), "node tree type"));
  memcpy(nt, &dummy_nt, sizeof(dummy_nt));

  nt->type = NTREE_CUSTOM;

  nt->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, nt->idname, &RNA_NodeTree);
  nt->rna_ext.data = data;
  nt->rna_ext.call = call;
  nt->rna_ext.free = free;
  RNA_struct_blender_type_set(nt->rna_ext.srna, nt);

  RNA_def_struct_ui_text(nt->rna_ext.srna, nt->ui_name, nt->ui_description);
  RNA_def_struct_ui_icon(nt->rna_ext.srna, nt->ui_icon);

  nt->poll = (have_function[0]) ? rna_NodeTree_poll : nullptr;
  nt->update = (have_function[1]) ? rna_NodeTree_update_reg : nullptr;
  nt->get_from_context = (have_function[2]) ? rna_NodeTree_get_from_context : nullptr;
  nt->valid_socket_type = (have_function[3]) ? rna_NodeTree_valid_socket_type : nullptr;

  ntreeTypeAdd(nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

static bool rna_NodeTree_check(bNodeTree *ntree, ReportList *reports)
{
  if (!ntreeIsRegistered(ntree)) {
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

  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

static void rna_NodeTree_update_asset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_NodeTree_update(bmain, scene, ptr);
  WM_main_add_notifier(NC_NODE | ND_NODE_ASSET_DATA, nullptr);
  blender::bke::node_update_asset_metadata(*reinterpret_cast<bNodeTree *>(ptr->owner_id));
}

static bNode *rna_NodeTree_node_new(bNodeTree *ntree,
                                    bContext *C,
                                    ReportList *reports,
                                    const char *type)
{
  bNodeType *ntype;
  bNode *node;

  if (!rna_NodeTree_check(ntree, reports)) {
    return nullptr;
  }

  /* If the given idname is an alias, translate it to the proper idname. */
  type = nodeTypeFindAlias(type);

  ntype = nodeTypeFind(type);
  if (!ntype) {
    BKE_reportf(reports, RPT_ERROR, "Node type %s undefined", type);
    return nullptr;
  }

  const char *disabled_hint = nullptr;
  if (ntype->poll && !ntype->poll(ntype, ntree, &disabled_hint)) {
    if (disabled_hint) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Cannot add node of type %s to node tree '%s'\n  %s",
                  type,
                  ntree->id.name + 2,
                  disabled_hint);
      return nullptr;
    }
    else {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Cannot add node of type %s to node tree '%s'",
                  type,
                  ntree->id.name + 2);
      return nullptr;
    }
  }

  node = nodeAddNode(C, ntree, type);
  BLI_assert(node && node->typeinfo);

  if (ntree->type == NTREE_TEXTURE) {
    ntreeTexCheckCyclics(ntree);
  }

  Main *bmain = CTX_data_main(C);
  ED_node_tree_propagate_change(C, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

  return node;
}

static void rna_NodeTree_node_remove(bNodeTree *ntree,
                                     Main *bmain,
                                     ReportList *reports,
                                     PointerRNA *node_ptr)
{
  bNode *node = static_cast<bNode *>(node_ptr->data);

  if (!rna_NodeTree_check(ntree, reports)) {
    return;
  }

  if (BLI_findindex(&ntree->nodes, node) == -1) {
    BKE_reportf(reports, RPT_ERROR, "Unable to locate node '%s' in node tree", node->name);
    return;
  }

  nodeRemoveNode(bmain, ntree, node, true);

  RNA_POINTER_INVALIDATE(node_ptr);

  ED_node_tree_propagate_change(nullptr, bmain, ntree);
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

    nodeRemoveNode(bmain, ntree, node, true);

    node = next_node;
  }

  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static PointerRNA rna_NodeTree_active_node_get(PointerRNA *ptr)
{
  bNodeTree *ntree = static_cast<bNodeTree *>(ptr->data);
  bNode *node = nodeGetActive(ntree);
  return rna_pointer_inherit_refine(ptr, &RNA_Node, node);
}

static void rna_NodeTree_active_node_set(PointerRNA *ptr,
                                         const PointerRNA value,
                                         ReportList * /*reports*/)
{
  bNodeTree *ntree = static_cast<bNodeTree *>(ptr->data);
  bNode *node = static_cast<bNode *>(value.data);

  if (node && BLI_findindex(&ntree->nodes, node) != -1) {
    nodeSetActive(ntree, node);

    /* Handle NODE_DO_OUTPUT as well. */
    if (node->typeinfo->nclass == NODE_CLASS_OUTPUT && node->type != CMP_NODE_OUTPUT_FILE) {
      /* If this node becomes the active output, the others of the same type can't be the active
       * output anymore. */
      LISTBASE_FOREACH (bNode *, other_node, &ntree->nodes) {
        if (other_node->type == node->type) {
          other_node->flag &= ~NODE_DO_OUTPUT;
        }
      }
      node->flag |= NODE_DO_OUTPUT;
      ntreeSetOutput(ntree);
      BKE_ntree_update_tag_active_output_changed(ntree);
    }
  }
  else {
    nodeClearActive(ntree);
  }
}

static bNodeLink *rna_NodeTree_link_new(bNodeTree *ntree,
                                        Main *bmain,
                                        ReportList *reports,
                                        bNodeSocket *fromsock,
                                        bNodeSocket *tosock,
                                        bool verify_limits)
{
  bNodeLink *ret;
  bNode *fromnode = nullptr, *tonode = nullptr;

  if (!rna_NodeTree_check(ntree, reports)) {
    return nullptr;
  }

  nodeFindNodeTry(ntree, fromsock, &fromnode, nullptr);
  nodeFindNodeTry(ntree, tosock, &tonode, nullptr);
  /* check validity of the sockets:
   * if sockets from different trees are passed in this will fail!
   */
  if (!fromnode || !tonode) {
    return nullptr;
  }

  if (&fromsock->in_out == &tosock->in_out) {
    BKE_report(reports, RPT_ERROR, "Same input/output direction of sockets");
    return nullptr;
  }

  if (verify_limits) {
    /* remove other socket links if limit is exceeded */
    if (nodeCountSocketLinks(ntree, fromsock) + 1 > nodeSocketLinkLimit(fromsock)) {
      nodeRemSocketLinks(ntree, fromsock);
    }
    if (nodeCountSocketLinks(ntree, tosock) + 1 > nodeSocketLinkLimit(tosock)) {
      nodeRemSocketLinks(ntree, tosock);
    }
    if (tosock->flag & SOCK_MULTI_INPUT) {
      LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
        if (link->fromsock == fromsock && link->tosock == tosock) {
          nodeRemLink(ntree, link);
        }
      }
    }
  }

  ret = nodeAddLink(ntree, fromnode, fromsock, tonode, tosock);

  if (ret) {

    /* not an issue from the UI, clear hidden from API to keep valid state. */
    fromsock->flag &= ~SOCK_HIDDEN;
    tosock->flag &= ~SOCK_HIDDEN;

    ED_node_tree_propagate_change(nullptr, bmain, ntree);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }
  return ret;
}

static void rna_NodeTree_link_remove(bNodeTree *ntree,
                                     Main *bmain,
                                     ReportList *reports,
                                     PointerRNA *link_ptr)
{
  bNodeLink *link = static_cast<bNodeLink *>(link_ptr->data);

  if (!rna_NodeTree_check(ntree, reports)) {
    return;
  }

  if (BLI_findindex(&ntree->links, link) == -1) {
    BKE_report(reports, RPT_ERROR, "Unable to locate link in node tree");
    return;
  }

  nodeRemLink(ntree, link);
  RNA_POINTER_INVALIDATE(link_ptr);

  ED_node_tree_propagate_change(nullptr, bmain, ntree);
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

    nodeRemLink(ntree, link);

    link = next_link;
  }
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static bool rna_NodeTree_contains_tree(bNodeTree *tree, bNodeTree *sub_tree)
{
  return ntreeContainsTree(tree, sub_tree);
}

static void rna_NodeTree_interface_update(bNodeTree *ntree, bContext *C)
{
  Main *bmain = CTX_data_main(C);
  ntree->tree_interface.tag_items_changed();
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

/* ******** NodeLink ******** */

static bool rna_NodeLink_is_hidden_get(PointerRNA *ptr)
{
  bNodeLink *link = static_cast<bNodeLink *>(ptr->data);
  return nodeLinkIsHidden(link);
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
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

/* ******** Node ******** */

static StructRNA *rna_Node_refine(PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);

  if (node->typeinfo->rna_ext.srna) {
    return node->typeinfo->rna_ext.srna;
  }
  else {
    return ptr->type;
  }
}

static std::optional<std::string> rna_Node_path(const PointerRNA *ptr)
{
  const bNode *node = static_cast<bNode *>(ptr->data);
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
    switch (node->type) {
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

static bool rna_Node_poll(const bNodeType *ntype,
                          const bNodeTree *ntree,
                          const char ** /*r_disabled_hint*/)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool visible;

  PointerRNA ptr = RNA_pointer_create(nullptr, ntype->rna_ext.srna, nullptr); /* dummy */
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

  PointerRNA ptr = RNA_pointer_create(
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

  PointerRNA ptr = RNA_pointer_create(
      reinterpret_cast<ID *>(ntree), node->typeinfo->rna_ext.srna, node);
  func = &rna_Node_update_func; /* RNA_struct_find_function(&ptr, "update"); */

  RNA_parameter_list_create(&list, &ptr, func);
  node->typeinfo->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static bool rna_Node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create(
      reinterpret_cast<ID *>(ntree), node->typeinfo->rna_ext.srna, node);
  func = &rna_Node_insert_link_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "link", &link);
  node->typeinfo->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
  return true;
}

static void rna_Node_init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  ParameterList list;
  FunctionRNA *func;

  func = &rna_Node_init_func; /* RNA_struct_find_function(&ptr, "init"); */

  RNA_parameter_list_create(&list, ptr, func);
  node->typeinfo->rna_ext.call(const_cast<bContext *>(C), ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_Node_copy(PointerRNA *ptr, const bNode *copynode)
{
  bNode *node = static_cast<bNode *>(ptr->data);
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
  bNode *node = static_cast<bNode *>(ptr->data);
  ParameterList list;
  FunctionRNA *func;

  func = &rna_Node_free_func; /* RNA_struct_find_function(&ptr, "free"); */

  RNA_parameter_list_create(&list, ptr, func);
  node->typeinfo->rna_ext.call(nullptr, ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_Node_draw_buttons(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
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
  bNode *node = static_cast<bNode *>(ptr->data);
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

  PointerRNA ptr = RNA_pointer_create(
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

static bool rna_Node_is_builtin(bNodeType *nt)
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

static bool rna_Node_unregister(Main * /*bmain*/, StructRNA *type)
{
  bNodeType *nt = static_cast<bNodeType *>(RNA_struct_blender_type_get(type));

  if (!nt || rna_Node_is_builtin(nt)) {
    return false;
  }

  RNA_struct_free_extension(type, &nt->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  /* this also frees the allocated nt pointer, no MEM_free call needed! */
  nodeUnregisterType(nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  return true;
}

/* Generic internal registration function.
 * Can be used to implement callbacks for registerable RNA node sub-types.
 */
static bNodeType *rna_Node_register_base(Main *bmain,
                                         ReportList *reports,
                                         StructRNA *basetype,
                                         void *data,
                                         const char *identifier,
                                         StructValidateFunc validate,
                                         StructCallbackFunc call,
                                         StructFreeFunc free)
{
  bNodeType *nt, dummy_nt;
  bNode dummy_node;
  FunctionRNA *func;
  PropertyRNA *parm;
  bool have_function[10];

  /* setup dummy node & node type to store static properties in */
  memset(&dummy_nt, 0, sizeof(bNodeType));
  /* this does some additional initialization of default values */
  node_type_base_custom(&dummy_nt, identifier, "", "CUSTOM", 0);

  memset(&dummy_node, 0, sizeof(bNode));
  dummy_node.typeinfo = &dummy_nt;
  PointerRNA dummy_node_ptr = RNA_pointer_create(nullptr, basetype, &dummy_node);

  /* validate the python class */
  if (validate(&dummy_node_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_nt.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering node class: '%s' is too long, maximum length is %d",
                identifier,
                int(sizeof(dummy_nt.idname)));
    return nullptr;
  }

  /* check if we have registered this node type before, and remove it */
  nt = nodeTypeFind(dummy_nt.idname);
  if (nt) {
    /* If it's an internal node, we cannot proceed. */
    if (rna_Node_is_builtin(nt)) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Registering node class: '%s', bl_idname '%s' is a builtin node",
                  identifier,
                  dummy_nt.idname);
      return nullptr;
    }

    BKE_reportf(reports,
                RPT_INFO,
                "Registering node class: '%s', bl_idname '%s' has been registered before, "
                "unregistering previous",
                identifier,
                dummy_nt.idname);

    /* NOTE: unlike most types `nt->rna_ext.srna` doesn't need to be checked for nullptr. */
    if (!rna_Node_unregister(bmain, nt->rna_ext.srna)) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Registering node class: '%s', bl_idname '%s' could not be unregistered",
                  identifier,
                  dummy_nt.idname);
      return nullptr;
    }
  }

  /* create a new node type */
  nt = static_cast<bNodeType *>(MEM_mallocN(sizeof(bNodeType), "node type"));
  memcpy(nt, &dummy_nt, sizeof(dummy_nt));
  nt->free_self = reinterpret_cast<void (*)(bNodeType *)>(MEM_freeN);

  nt->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, nt->idname, basetype);
  nt->rna_ext.data = data;
  nt->rna_ext.call = call;
  nt->rna_ext.free = free;
  RNA_struct_blender_type_set(nt->rna_ext.srna, nt);

  RNA_def_struct_ui_text(nt->rna_ext.srna, nt->ui_name, nt->ui_description);
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
  bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_Node, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  nodeRegisterType(nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

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
  const bNodeTree *ntree = static_cast<const bNodeTree *>(ptr->data);
  if (!ntree->geometry_node_asset_traits) {
    return false;
  }
  return ntree->geometry_node_asset_traits->flag & flag;
}

static void geometry_node_asset_trait_flag_set(PointerRNA *ptr,
                                               const GeometryNodeAssetTraitFlag flag,
                                               const bool value)
{
  bNodeTree *ntree = static_cast<bNodeTree *>(ptr->data);
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

static bool rna_GeometryNodeTree_is_type_point_cloud_get(PointerRNA *ptr)
{
  return geometry_node_asset_trait_flag_get(ptr, GEO_NODE_ASSET_POINT_CLOUD);
}
static void rna_GeometryNodeTree_is_type_point_cloud_set(PointerRNA *ptr, bool value)
{
  geometry_node_asset_trait_flag_set(ptr, GEO_NODE_ASSET_POINT_CLOUD, value);
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
    if (!U.experimental.use_grease_pencil_version3 && item->value == int(bke::AttrDomain::Layer)) {
      continue;
    }
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
  bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_ShaderNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  nodeRegisterType(nt);

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
  bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_CompositorNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  nodeRegisterType(nt);

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
  bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_TextureNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  nodeRegisterType(nt);

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
  bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_GeometryNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  nodeRegisterType(nt);

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
  bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_FunctionNode, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  nodeRegisterType(nt);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

static IDProperty **rna_Node_idprops(PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  return &node->prop;
}

static void rna_Node_parent_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  bNode *parent = static_cast<bNode *>(value.data);
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);

  if (!parent) {
    nodeDetachNode(ntree, node);
    return;
  }

  /* XXX only Frame node allowed for now,
   * in the future should have a poll function or so to test possible attachment.
   */
  if (parent->type != NODE_FRAME) {
    return;
  }

  if (nodeIsParentAndChild(node, parent)) {
    return;
  }

  nodeDetachNode(ntree, node);
  nodeAttachNode(ntree, node, parent);
}

static void rna_Node_internal_links_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  bNodeLink *begin;
  int len;
  nodeInternalLinks(node, &begin, &len);
  rna_iterator_array_begin(iter, begin, sizeof(bNodeLink), len, false, nullptr);
}

/**
 * Forbid identifier lookup in nodes whose identifiers are likely to change soon because
 * dynamically typed sockets are joined into one.
 */
static bool allow_identifier_lookup(const bNode &node)
{
  switch (node.type) {
    case GEO_NODE_SWITCH:
    case GEO_NODE_ACCUMULATE_FIELD:
    case GEO_NODE_CAPTURE_ATTRIBUTE:
    case GEO_NODE_ATTRIBUTE_STATISTIC:
    case GEO_NODE_BLUR_ATTRIBUTE:
    case GEO_NODE_SAMPLE_CURVE:
    case GEO_NODE_EVALUATE_AT_INDEX:
    case GEO_NODE_EVALUATE_ON_DOMAIN:
    case GEO_NODE_INPUT_NAMED_ATTRIBUTE:
    case GEO_NODE_RAYCAST:
    case GEO_NODE_SAMPLE_INDEX:
    case GEO_NODE_SAMPLE_NEAREST_SURFACE:
    case FN_NODE_RANDOM_VALUE:
    case GEO_NODE_SAMPLE_UV_SURFACE:
    case GEO_NODE_STORE_NAMED_ATTRIBUTE:
    case GEO_NODE_VIEWER:
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
  bNode *node = static_cast<bNode *>(ptr->data);
  if (bNodeSocket *socket = find_socket_by_key(*node, SOCK_IN, key)) {
    *r_ptr = RNA_pointer_create(ptr->owner_id, &RNA_NodeSocket, socket);
    return true;
  }
  return false;
}

static bool rna_NodeOutputs_lookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  if (bNodeSocket *socket = find_socket_by_key(*node, SOCK_OUT, key)) {
    *r_ptr = RNA_pointer_create(ptr->owner_id, &RNA_NodeSocket, socket);
    return true;
  }
  return false;
}

static bool rna_Node_parent_poll(PointerRNA *ptr, PointerRNA value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  bNode *parent = static_cast<bNode *>(value.data);

  /* XXX only Frame node allowed for now,
   * in the future should have a poll function or so to test possible attachment.
   */
  if (parent->type != NODE_FRAME) {
    return false;
  }

  if (node->type == NODE_FRAME && nodeIsParentAndChild(node, parent)) {
    return false;
  }

  return true;
}

void rna_Node_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = static_cast<bNode *>(ptr->data);
  BKE_ntree_update_tag_node_property(ntree, node);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

void rna_Node_update_relations(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Node_update(bmain, scene, ptr);
  DEG_relations_tag_update(bmain);
}

static void rna_Node_socket_value_update(ID *id, bNode * /*node*/, bContext *C)
{
  BKE_ntree_update_tag_all(reinterpret_cast<bNodeTree *>(id));
  ED_node_tree_propagate_change(C, CTX_data_main(C), reinterpret_cast<bNodeTree *>(id));
}

static void rna_Node_select_set(PointerRNA *ptr, bool value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  nodeSetSelected(node, value);
}

static void rna_Node_name_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = static_cast<bNode *>(ptr->data);
  char oldname[sizeof(node->name)];

  /* make a copy of the old name first */
  STRNCPY(oldname, node->name);
  /* set new name */
  STRNCPY_UTF8(node->name, value);

  nodeUniqueName(ntree, node);

  /* fix all the animation data which may link to this */
  BKE_animdata_fix_paths_rename_all(nullptr, "nodes", oldname, node->name);
}

static bool allow_changing_sockets(bNode *node)
{
  return ELEM(node->type, NODE_CUSTOM, SH_NODE_SCRIPT, CMP_NODE_OUTPUT_FILE);
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
  if (!allow_changing_sockets(node)) {
    BKE_report(reports, RPT_ERROR, "Cannot add socket to built-in node");
    return nullptr;
  }

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  bNodeSocket *sock = nodeAddSocket(ntree, node, SOCK_IN, type, identifier, name);

  if (sock == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to create socket");
  }
  else {
    if (use_multi_input) {
      sock->flag |= SOCK_MULTI_INPUT;
    }
    ED_node_tree_propagate_change(nullptr, bmain, ntree);
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
  if (!allow_changing_sockets(node)) {
    BKE_report(reports, RPT_ERROR, "Cannot add socket to built-in node");
    return nullptr;
  }

  if (use_multi_input) {
    BKE_report(reports, RPT_ERROR, "Output sockets cannot be multi-input");
    return nullptr;
  }

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  bNodeSocket *sock = nodeAddSocket(ntree, node, SOCK_OUT, type, identifier, name);

  if (sock == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to create socket");
  }
  else {
    ED_node_tree_propagate_change(nullptr, bmain, ntree);
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
    nodeRemoveSocket(ntree, node, sock);

    ED_node_tree_propagate_change(nullptr, bmain, ntree);
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
    nodeRemoveSocket(ntree, node, sock);
  }

  ED_node_tree_propagate_change(nullptr, bmain, ntree);
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
    nodeRemoveSocket(ntree, node, sock);
  }

  ED_node_tree_propagate_change(nullptr, bmain, ntree);
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
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
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
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_Node_width_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  *min = *softmin = node->typeinfo->minwidth;
  *max = *softmax = node->typeinfo->maxwidth;
}

static void rna_Node_height_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  *min = *softmin = node->typeinfo->minheight;
  *max = *softmax = node->typeinfo->maxheight;
}

static void rna_Node_dimensions_get(PointerRNA *ptr, float *value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  nodeDimensionsGet(node, &value[0], &value[1]);
}

/* ******** Node Types ******** */

static void rna_NodeInternalSocketTemplate_name_get(PointerRNA *ptr, char *value)
{
  bNodeSocketTemplate *stemp = static_cast<bNodeSocketTemplate *>(ptr->data);
  strcpy(value, stemp->name);
}

static int rna_NodeInternalSocketTemplate_name_length(PointerRNA *ptr)
{
  bNodeSocketTemplate *stemp = static_cast<bNodeSocketTemplate *>(ptr->data);
  return strlen(stemp->name);
}

static void rna_NodeInternalSocketTemplate_identifier_get(PointerRNA *ptr, char *value)
{
  bNodeSocketTemplate *stemp = static_cast<bNodeSocketTemplate *>(ptr->data);
  strcpy(value, stemp->identifier);
}

static int rna_NodeInternalSocketTemplate_identifier_length(PointerRNA *ptr)
{
  bNodeSocketTemplate *stemp = static_cast<bNodeSocketTemplate *>(ptr->data);
  return strlen(stemp->identifier);
}

static int rna_NodeInternalSocketTemplate_type_get(PointerRNA *ptr)
{
  bNodeSocketTemplate *stemp = static_cast<bNodeSocketTemplate *>(ptr->data);
  return stemp->type;
}

static PointerRNA rna_NodeInternal_input_template(StructRNA *srna, int index)
{
  bNodeType *ntype = static_cast<bNodeType *>(RNA_struct_blender_type_get(srna));
  if (ntype && ntype->inputs) {
    bNodeSocketTemplate *stemp = ntype->inputs;
    int i = 0;
    while (i < index && stemp->type >= 0) {
      i++;
      stemp++;
    }
    if (i == index && stemp->type >= 0) {
      PointerRNA ptr = RNA_pointer_create(nullptr, &RNA_NodeInternalSocketTemplate, stemp);
      return ptr;
    }
  }
  return PointerRNA_NULL;
}

static PointerRNA rna_NodeInternal_output_template(StructRNA *srna, int index)
{
  bNodeType *ntype = static_cast<bNodeType *>(RNA_struct_blender_type_get(srna));
  if (ntype && ntype->outputs) {
    bNodeSocketTemplate *stemp = ntype->outputs;
    int i = 0;
    while (i < index && stemp->type >= 0) {
      i++;
      stemp++;
    }
    if (i == index && stemp->type >= 0) {
      PointerRNA ptr = RNA_pointer_create(nullptr, &RNA_NodeInternalSocketTemplate, stemp);
      return ptr;
    }
  }
  return PointerRNA_NULL;
}

static bool rna_NodeInternal_poll(StructRNA *srna, bNodeTree *ntree)
{
  bNodeType *ntype = static_cast<bNodeType *>(RNA_struct_blender_type_get(srna));
  const char *disabled_hint;
  return ntype && (!ntype->poll || ntype->poll(ntype, ntree, &disabled_hint));
}

static bool rna_NodeInternal_poll_instance(bNode *node, bNodeTree *ntree)
{
  bNodeType *ntype = node->typeinfo;
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
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

static void rna_NodeInternal_draw_buttons(ID *id, bNode *node, bContext *C, uiLayout *layout)
{
  if (node->typeinfo->draw_buttons) {
    PointerRNA ptr = RNA_pointer_create(id, &RNA_Node, node);
    node->typeinfo->draw_buttons(layout, C, &ptr);
  }
}

static void rna_NodeInternal_draw_buttons_ext(ID *id, bNode *node, bContext *C, uiLayout *layout)
{
  if (node->typeinfo->draw_buttons_ex) {
    PointerRNA ptr = RNA_pointer_create(id, &RNA_Node, node);
    node->typeinfo->draw_buttons_ex(layout, C, &ptr);
  }
  else if (node->typeinfo->draw_buttons) {
    PointerRNA ptr = RNA_pointer_create(id, &RNA_Node, node);
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
  bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_NodeCustomGroup, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  nodeRegisterType(nt);

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
  bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_GeometryNodeCustomGroup, data, identifier, validate, call, free);

  if (!nt) {
    return nullptr;
  }

  nt->type = NODE_CUSTOM_GROUP;

  register_node_type_geo_custom_group(nt);

  nodeRegisterType(nt);

  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return nt->rna_ext.srna;
}

void register_node_type_geo_custom_group(bNodeType *ntype);

static StructRNA *rna_ShaderNodeCustomGroup_register(Main *bmain,
                                                     ReportList *reports,
                                                     void *data,
                                                     const char *identifier,
                                                     StructValidateFunc validate,
                                                     StructCallbackFunc call,
                                                     StructFreeFunc free)
{
  bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_ShaderNodeCustomGroup, data, identifier, validate, call, free);

  if (!nt) {
    return nullptr;
  }

  nt->type = NODE_CUSTOM_GROUP;

  register_node_type_sh_custom_group(nt);

  nodeRegisterType(nt);

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
  bNodeType *nt = rna_Node_register_base(
      bmain, reports, &RNA_CompositorNodeCustomGroup, data, identifier, validate, call, free);
  if (!nt) {
    return nullptr;
  }

  nt->type = NODE_CUSTOM_GROUP;

  register_node_type_cmp_custom_group(nt);

  nodeRegisterType(nt);

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
  bNode *node = static_cast<bNode *>(ptr->data);

  BKE_ntree_update_tag_node_property(ntree, node);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_IMAGE, nullptr);
}

static void rna_NodeGroup_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = static_cast<bNode *>(ptr->data);

  BKE_ntree_update_tag_node_property(ntree, node);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  DEG_relations_tag_update(bmain);
}

static void rna_NodeGroup_node_tree_set(PointerRNA *ptr,
                                        const PointerRNA value,
                                        ReportList * /*reports*/)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = static_cast<bNode *>(ptr->data);
  bNodeTree *ngroup = static_cast<bNodeTree *>(value.data);

  const char *disabled_hint = nullptr;
  if (nodeGroupPoll(ntree, ngroup, &disabled_hint)) {
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
  return nodeGroupPoll(ntree, ngroup, &disabled_hint);
}

static void rna_distance_matte_t1_set(PointerRNA *ptr, float value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeChroma *chroma = static_cast<NodeChroma *>(node->storage);

  chroma->t1 = value;
}

static void rna_distance_matte_t2_set(PointerRNA *ptr, float value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeChroma *chroma = static_cast<NodeChroma *>(node->storage);

  chroma->t2 = value;
}

static void rna_difference_matte_t1_set(PointerRNA *ptr, float value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeChroma *chroma = static_cast<NodeChroma *>(node->storage);

  chroma->t1 = value;
}

static void rna_difference_matte_t2_set(PointerRNA *ptr, float value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeChroma *chroma = static_cast<NodeChroma *>(node->storage);

  chroma->t2 = value;
}

/* Button Set Functions for Matte Nodes */
static void rna_Matte_t1_set(PointerRNA *ptr, float value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeChroma *chroma = static_cast<NodeChroma *>(node->storage);

  chroma->t1 = value;

  if (value < chroma->t2) {
    chroma->t2 = value;
  }
}

static void rna_Matte_t2_set(PointerRNA *ptr, float value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeChroma *chroma = static_cast<NodeChroma *>(node->storage);

  if (value > chroma->t1) {
    value = chroma->t1;
  }

  chroma->t2 = value;
}

static void rna_Node_scene_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  bNode *node = static_cast<bNode *>(ptr->data);

  if (node->id) {
    id_us_min(node->id);
    node->id = nullptr;
  }

  node->id = static_cast<ID *>(value.data);

  id_us_plus(node->id);
}

static void rna_Node_image_layer_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  Image *ima = reinterpret_cast<Image *>(node->id);
  ImageUser *iuser = static_cast<ImageUser *>(node->storage);

  if (node->type == CMP_NODE_CRYPTOMATTE && node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) {
    return;
  }

  BKE_image_multilayer_index(ima->rr, iuser);
  BKE_image_signal(bmain, ima, iuser, IMA_SIGNAL_SRC_CHANGE);

  rna_Node_update(bmain, scene, ptr);

  if (scene != nullptr && scene->nodetree != nullptr) {
    ntreeCompositUpdateRLayers(scene->nodetree);
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
  bNode *node = static_cast<bNode *>(ptr->data);
  Image *ima = reinterpret_cast<Image *>(node->id);
  const EnumPropertyItem *item = nullptr;
  RenderLayer *rl;

  if (node->type == CMP_NODE_CRYPTOMATTE && node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) {
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
  bNode *node = static_cast<bNode *>(ptr->data);
  Image *ima = reinterpret_cast<Image *>(node->id);

  if (node->type == CMP_NODE_CRYPTOMATTE && node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) {
    return false;
  }

  if (!ima || !(ima->rr)) {
    return false;
  }

  return RE_layers_have_name(ima->rr);
}

static bool rna_Node_image_has_views_get(PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  Image *ima = reinterpret_cast<Image *>(node->id);

  if (node->type == CMP_NODE_CRYPTOMATTE && node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) {
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
  bNode *node = static_cast<bNode *>(ptr->data);
  Image *ima = reinterpret_cast<Image *>(node->id);
  const EnumPropertyItem *item = nullptr;
  RenderView *rv;

  if (node->type == CMP_NODE_CRYPTOMATTE && node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) {
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
  bNode *node = static_cast<bNode *>(ptr->data);
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
  rna_Node_update(bmain, scene, ptr);
  if (scene != nullptr && scene->nodetree != nullptr) {
    ntreeCompositUpdateRLayers(scene->nodetree);
  }
}

static const EnumPropertyItem *rna_Node_channel_itemf(bContext * /*C*/,
                                                      PointerRNA *ptr,
                                                      PropertyRNA * /*prop*/,
                                                      bool *r_free)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0};
  int totitem = 0;

  switch (node->custom1) {
    case CMP_NODE_CHANNEL_MATTE_CS_RGB:
      tmp.identifier = "R";
      tmp.name = "R";
      tmp.value = 1;
      RNA_enum_item_add(&item, &totitem, &tmp);
      tmp.identifier = "G";
      tmp.name = "G";
      tmp.value = 2;
      RNA_enum_item_add(&item, &totitem, &tmp);
      tmp.identifier = "B";
      tmp.name = "B";
      tmp.value = 3;
      RNA_enum_item_add(&item, &totitem, &tmp);
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_HSV:
      tmp.identifier = "H";
      tmp.name = "H";
      tmp.value = 1;
      RNA_enum_item_add(&item, &totitem, &tmp);
      tmp.identifier = "S";
      tmp.name = "S";
      tmp.value = 2;
      RNA_enum_item_add(&item, &totitem, &tmp);
      tmp.identifier = "V";
      tmp.name = "V";
      tmp.value = 3;
      RNA_enum_item_add(&item, &totitem, &tmp);
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_YUV:
      tmp.identifier = "Y";
      tmp.name = "Y";
      tmp.value = 1;
      RNA_enum_item_add(&item, &totitem, &tmp);
      tmp.identifier = "G";
      tmp.name = "U";
      tmp.value = 2;
      RNA_enum_item_add(&item, &totitem, &tmp);
      tmp.identifier = "V";
      tmp.name = "V";
      tmp.value = 3;
      RNA_enum_item_add(&item, &totitem, &tmp);
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_YCC:
      tmp.identifier = "Y";
      tmp.name = "Y";
      tmp.value = 1;
      RNA_enum_item_add(&item, &totitem, &tmp);
      tmp.identifier = "CB";
      tmp.name = "Cr";
      tmp.value = 2;
      RNA_enum_item_add(&item, &totitem, &tmp);
      tmp.identifier = "CR";
      tmp.name = "Cb";
      tmp.value = 3;
      RNA_enum_item_add(&item, &totitem, &tmp);
      break;
    default:
      return rna_enum_dummy_NULL_items;
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static void rna_Image_Node_update_id(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);

  nodeTagUpdateID(node);
  rna_Node_update(bmain, scene, ptr);
}

static void rna_NodeOutputFile_slots_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  rna_iterator_listbase_begin(iter, &node->inputs, nullptr);
}

static PointerRNA rna_NodeOutputFile_slot_file_get(CollectionPropertyIterator *iter)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(rna_iterator_listbase_get(iter));
  PointerRNA ptr = RNA_pointer_create(
      iter->parent.owner_id, &RNA_NodeOutputFileSlotFile, sock->storage);
  return ptr;
}

static void rna_NodeColorBalance_update_lgg(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ntreeCompositColorBalanceSyncFromLGG(reinterpret_cast<bNodeTree *>(ptr->owner_id),
                                       static_cast<bNode *>(ptr->data));
  rna_Node_update(bmain, scene, ptr);
}

static void rna_NodeColorBalance_update_cdl(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ntreeCompositColorBalanceSyncFromCDL(reinterpret_cast<bNodeTree *>(ptr->owner_id),
                                       static_cast<bNode *>(ptr->data));
  rna_Node_update(bmain, scene, ptr);
}

static void rna_NodeCryptomatte_source_set(PointerRNA *ptr, int value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  if (node->id && node->custom1 != value) {
    id_us_min(node->id);
    node->id = nullptr;
  }
  node->custom1 = value;
}

static int rna_NodeCryptomatte_layer_name_get(PointerRNA *ptr)
{
  int index = 0;
  bNode *node = static_cast<bNode *>(ptr->data);
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
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeCryptomatte *storage = static_cast<NodeCryptomatte *>(node->storage);

  CryptomatteLayer *layer = static_cast<CryptomatteLayer *>(
      BLI_findlink(&storage->runtime.layers, new_value));
  if (layer) {
    STRNCPY(storage->layer_name, layer->name);
  }
}

static const EnumPropertyItem *rna_NodeCryptomatte_layer_name_itemf(bContext *C,
                                                                    PointerRNA *ptr,
                                                                    PropertyRNA * /*prop*/,
                                                                    bool *r_free)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeCryptomatte *storage = static_cast<NodeCryptomatte *>(node->storage);
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem temp = {0, "", 0, "", ""};
  int totitem = 0;

  ntreeCompositCryptomatteUpdateLayerNames(CTX_data_scene(C), node);
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
  bNode *node = static_cast<bNode *>(ptr->data);

  Scene *scene = (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER) ?
                     reinterpret_cast<Scene *>(node->id) :
                     nullptr;
  return rna_pointer_inherit_refine(ptr, &RNA_Scene, scene);
}

static void rna_NodeCryptomatte_scene_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  bNode *node = static_cast<bNode *>(ptr->data);

  if (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER) {
    rna_Node_scene_set(ptr, value, reports);
  }
}

static PointerRNA rna_NodeCryptomatte_image_get(PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);

  Image *image = (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) ?
                     reinterpret_cast<Image *>(node->id) :
                     nullptr;
  return rna_pointer_inherit_refine(ptr, &RNA_Image, image);
}

static void rna_NodeCryptomatte_image_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          ReportList * /*reports*/)
{
  bNode *node = static_cast<bNode *>(ptr->data);

  if (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) {
    if (node->id)
      id_us_min(node->id);
    if (value.data)
      id_us_plus(static_cast<ID *>(value.data));

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
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
  char *matte_id = BKE_cryptomatte_entries_to_matte_id(nc);
  strcpy(value, matte_id);
  MEM_freeN(matte_id);
}

static int rna_NodeCryptomatte_matte_length(PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
  char *matte_id = BKE_cryptomatte_entries_to_matte_id(nc);
  int result = strlen(matte_id);
  MEM_freeN(matte_id);
  return result;
}

static void rna_NodeCryptomatte_matte_set(PointerRNA *ptr, const char *value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
  BKE_cryptomatte_matte_id_to_entries(nc, value);
}

static void rna_NodeCryptomatte_update_add(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ntreeCompositCryptomatteSyncFromAdd(scene, static_cast<bNode *>(ptr->data));
  rna_Node_update(bmain, scene, ptr);
}

static void rna_NodeCryptomatte_update_remove(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ntreeCompositCryptomatteSyncFromRemove(static_cast<bNode *>(ptr->data));
  rna_Node_update(bmain, scene, ptr);
}

static PointerRNA rna_Node_paired_output_get(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = static_cast<bNode *>(ptr->data);
  const blender::bke::bNodeZoneType &zone_type = *blender::bke::zone_type_by_node_type(node->type);
  bNode *output_node = zone_type.get_corresponding_output(*ntree, *node);
  PointerRNA r_ptr = RNA_pointer_create(&ntree->id, &RNA_Node, output_node);
  return r_ptr;
}

static bool rna_Node_pair_with_output(
    ID *id, bNode *node, bContext *C, ReportList *reports, bNode *output_node)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  const blender::bke::bNodeZoneType &zone_type = *blender::bke::zone_type_by_node_type(node->type);
  if (output_node->type != zone_type.output_type) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Can't pair zone input node %s with %s because it does not have the same zone type",
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
  ED_node_tree_propagate_change(C, CTX_data_main(C), ntree);
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
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

template<typename Accessor> static void rna_Node_ItemArray_clear(ID *id, bNode *node, Main *bmain)
{
  blender::nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  blender::dna::array::clear(ref.items, ref.items_num, ref.active_index, Accessor::destruct_item);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
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
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

template<typename Accessor> static PointerRNA rna_Node_ItemArray_active_get(PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  blender::nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  typename Accessor::ItemT *active_item = nullptr;
  const int active_index = *ref.active_index;
  const int items_num = *ref.items_num;
  if (active_index >= 0 && active_index < items_num) {
    active_item = &(*ref.items)[active_index];
  }
  return RNA_pointer_create(ptr->owner_id, Accessor::item_srna, active_item);
}
template<typename Accessor>
static void rna_Node_ItemArray_active_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          ReportList * /*reports*/)
{
  using ItemT = typename Accessor::ItemT;
  bNode *node = static_cast<bNode *>(ptr->data);
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
  ED_node_tree_propagate_change(nullptr, bmain, &ntree);
}

template<typename Accessor>
static const EnumPropertyItem *rna_Node_ItemArray_socket_type_itemf(bContext * /*C*/,
                                                                    PointerRNA * /*ptr*/,
                                                                    PropertyRNA * /*prop*/,
                                                                    bool *r_free)
{
  *r_free = true;
  return itemf_function_check(
      rna_enum_node_socket_data_type_items, [](const EnumPropertyItem *item) {
        return Accessor::supports_socket_type(eNodeSocketDatatype(item->value));
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
  const char *socket_type_idname = nodeStaticSocketType(*Accessors::get_socket_type(item), 0);
  ED_node_type_draw_color(socket_type_idname, values);
}

template<typename Accessor>
typename Accessor::ItemT *rna_Node_ItemArray_new_with_socket_and_name(
    ID *id, bNode *node, Main *bmain, ReportList *reports, int socket_type, const char *name)
{
  using ItemT = typename Accessor::ItemT;
  if (!Accessor::supports_socket_type(eNodeSocketDatatype(socket_type))) {
    BKE_report(reports, RPT_ERROR, "Unable to create item with this socket type");
    return nullptr;
  }
  ItemT *new_item = blender::nodes::socket_items::add_item_with_socket_and_name<Accessor>(
      *node, eNodeSocketDatatype(socket_type), name);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

  return new_item;
}

static IndexSwitchItem *rna_NodeIndexSwitchItems_new(ID *id, bNode *node, Main *bmain)
{
  IndexSwitchItem *new_item = blender::nodes::socket_items::add_item<IndexSwitchItemsAccessor>(
      *node);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

  return new_item;
}

/* ******** Node Socket Types ******** */

static PointerRNA rna_NodeOutputFile_slot_layer_get(CollectionPropertyIterator *iter)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(rna_iterator_listbase_get(iter));
  PointerRNA ptr = RNA_pointer_create(
      iter->parent.owner_id, &RNA_NodeOutputFileSlotLayer, sock->storage);
  return ptr;
}

static int rna_NodeOutputFileSocket_find_node(bNodeTree *ntree,
                                              NodeImageMultiFileSocket *data,
                                              bNode **nodep,
                                              bNodeSocket **sockp)
{
  bNode *node;
  bNodeSocket *sock;

  for (node = static_cast<bNode *>(ntree->nodes.first); node; node = node->next) {
    for (sock = static_cast<bNodeSocket *>(node->inputs.first); sock; sock = sock->next) {
      NodeImageMultiFileSocket *sockdata = static_cast<NodeImageMultiFileSocket *>(sock->storage);
      if (sockdata == data) {
        *nodep = node;
        *sockp = sock;
        return 1;
      }
    }
  }

  *nodep = nullptr;
  *sockp = nullptr;
  return 0;
}

static void rna_NodeOutputFileSlotFile_path_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  NodeImageMultiFileSocket *sockdata = static_cast<NodeImageMultiFileSocket *>(ptr->data);
  bNode *node;
  bNodeSocket *sock;

  if (rna_NodeOutputFileSocket_find_node(ntree, sockdata, &node, &sock)) {
    ntreeCompositOutputFileSetPath(node, sock, value);
  }
}

static void rna_NodeOutputFileSlotLayer_name_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  NodeImageMultiFileSocket *sockdata = static_cast<NodeImageMultiFileSocket *>(ptr->data);
  bNode *node;
  bNodeSocket *sock;

  if (rna_NodeOutputFileSocket_find_node(ntree, sockdata, &node, &sock)) {
    ntreeCompositOutputFileSetLayer(node, sock, value);
  }
}

static bNodeSocket *rna_NodeOutputFile_slots_new(
    ID *id, bNode *node, bContext *C, ReportList * /*reports*/, const char *name)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  Scene *scene = CTX_data_scene(C);
  ImageFormatData *im_format = nullptr;
  bNodeSocket *sock;
  if (scene) {
    im_format = &scene->r.im_format;
  }

  sock = ntreeCompositOutputFileAddSocket(ntree, node, name, im_format);

  ED_node_tree_propagate_change(C, CTX_data_main(C), ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

  return sock;
}

static void rna_FrameNode_label_size_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  BLF_cache_clear();
  rna_Node_update(bmain, scene, ptr);
}

static void rna_ShaderNodeTexIES_mode_set(PointerRNA *ptr, int value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeShaderTexIES *nss = static_cast<NodeShaderTexIES *>(node->storage);

  if (nss->mode != value) {
    nss->mode = value;
    nss->filepath[0] = '\0';

    /* replace text datablock by filepath */
    if (node->id) {
      Text *text = reinterpret_cast<Text *>(node->id);

      if (value == NODE_IES_EXTERNAL && text->filepath) {
        STRNCPY(nss->filepath, text->filepath);
        BLI_path_rel(nss->filepath, BKE_main_blendfile_path_from_global());
      }

      id_us_min(node->id);
      node->id = nullptr;
    }
  }
}

static void rna_ShaderNodeScript_mode_set(PointerRNA *ptr, int value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
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
        BLI_path_rel(nss->filepath, BKE_main_blendfile_path_from_global());
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
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);

  strcpy(value, (nss->bytecode) ? nss->bytecode : "");
}

static int rna_ShaderNodeScript_bytecode_length(PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);

  return (nss->bytecode) ? strlen(nss->bytecode) : 0;
}

static void rna_ShaderNodeScript_bytecode_set(PointerRNA *ptr, const char *value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
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
  bNode *node = static_cast<bNode *>(ptr->data);
  RenderEngineType *engine_type = (scene != nullptr) ? RE_engines_find(scene->r.engine) : nullptr;

  if (engine_type && engine_type->update_script_node) {
    /* auto update node */
    RenderEngine *engine = RE_engine_create(engine_type);
    engine_type->update_script_node(engine, ntree, node);
    RE_engine_free(engine);
  }

  BKE_ntree_update_tag_node_property(ntree, node);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

static void rna_ShaderNode_socket_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Node_update(bmain, scene, ptr);
}

void rna_Node_socket_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Node_update(bmain, scene, ptr);
}

static void rna_CompositorNodeScale_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Node_update(bmain, scene, ptr);
}

static void rna_ShaderNode_is_active_output_set(PointerRNA *ptr, bool value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode *node = static_cast<bNode *>(ptr->data);
  if (value) {
    /* If this node becomes the active output, the others of the same type can't be the active
     * output anymore. */
    LISTBASE_FOREACH (bNode *, other_node, &ntree->nodes) {
      if (other_node->type == node->type) {
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
  bNode *node = static_cast<bNode *>(ptr->data);
  if (value) {
    /* Make sure that no other group output is active at the same time. */
    LISTBASE_FOREACH (bNode *, other_node, &ntree->nodes) {
      if (other_node->type == NODE_GROUP_OUTPUT) {
        other_node->flag &= ~NODE_DO_OUTPUT;
      }
    }
    node->flag |= NODE_DO_OUTPUT;
  }
  else {
    node->flag &= ~NODE_DO_OUTPUT;
  }
}

static PointerRNA rna_ShaderNodePointDensity_psys_get(PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeShaderTexPointDensity *shader_point_density = static_cast<NodeShaderTexPointDensity *>(
      node->storage);
  Object *ob = reinterpret_cast<Object *>(node->id);
  ParticleSystem *psys = nullptr;

  if (ob && shader_point_density->particle_system) {
    psys = static_cast<ParticleSystem *>(
        BLI_findlink(&ob->particlesystem, shader_point_density->particle_system - 1));
  }

  PointerRNA value = RNA_pointer_create(&ob->id, &RNA_ParticleSystem, psys);
  return value;
}

static void rna_ShaderNodePointDensity_psys_set(PointerRNA *ptr,
                                                PointerRNA value,
                                                ReportList * /*reports*/)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeShaderTexPointDensity *shader_point_density = static_cast<NodeShaderTexPointDensity *>(
      node->storage);
  Object *ob = reinterpret_cast<Object *>(node->id);

  if (ob && value.owner_id == &ob->id) {
    shader_point_density->particle_system = BLI_findindex(&ob->particlesystem, value.data) + 1;
  }
  else {
    shader_point_density->particle_system = 0;
  }
}

static int point_density_particle_color_source_from_shader(
    NodeShaderTexPointDensity *shader_point_density)
{
  switch (shader_point_density->color_source) {
    case SHD_POINTDENSITY_COLOR_PARTAGE:
      return TEX_PD_COLOR_PARTAGE;
    case SHD_POINTDENSITY_COLOR_PARTSPEED:
      return TEX_PD_COLOR_PARTSPEED;
    case SHD_POINTDENSITY_COLOR_PARTVEL:
      return TEX_PD_COLOR_PARTVEL;
    default:
      BLI_assert_msg(0, "Unknown color source");
      return TEX_PD_COLOR_CONSTANT;
  }
}

static int point_density_vertex_color_source_from_shader(
    NodeShaderTexPointDensity *shader_point_density)
{
  switch (shader_point_density->ob_color_source) {
    case SHD_POINTDENSITY_COLOR_VERTCOL:
      return TEX_PD_COLOR_VERTCOL;
    case SHD_POINTDENSITY_COLOR_VERTWEIGHT:
      return TEX_PD_COLOR_VERTWEIGHT;
    case SHD_POINTDENSITY_COLOR_VERTNOR:
      return TEX_PD_COLOR_VERTNOR;
    default:
      BLI_assert_msg(0, "Unknown color source");
      return TEX_PD_COLOR_CONSTANT;
  }
}

void rna_ShaderNodePointDensity_density_cache(bNode *self, Depsgraph *depsgraph)
{
  NodeShaderTexPointDensity *shader_point_density = static_cast<NodeShaderTexPointDensity *>(
      self->storage);
  PointDensity *pd = &shader_point_density->pd;

  if (depsgraph == nullptr) {
    return;
  }

  /* Make sure there's no cached data. */
  BKE_texture_pointdensity_free_data(pd);
  RE_point_density_free(pd);

  /* Create PointDensity structure from node for sampling. */
  BKE_texture_pointdensity_init_data(pd);
  pd->object = reinterpret_cast<Object *>(self->id);
  pd->radius = shader_point_density->radius;
  if (shader_point_density->point_source == SHD_POINTDENSITY_SOURCE_PSYS) {
    pd->source = TEX_PD_PSYS;
    pd->psys = shader_point_density->particle_system;
    pd->psys_cache_space = TEX_PD_OBJECTSPACE;
    pd->color_source = point_density_particle_color_source_from_shader(shader_point_density);
  }
  else {
    BLI_assert(shader_point_density->point_source == SHD_POINTDENSITY_SOURCE_OBJECT);
    pd->source = TEX_PD_OBJECT;
    pd->ob_cache_space = TEX_PD_OBJECTSPACE;
    pd->ob_color_source = point_density_vertex_color_source_from_shader(shader_point_density);
    STRNCPY(pd->vertex_attribute_name, shader_point_density->vertex_attribute_name);
  }

  /* Store resolution, so it can be changed in the UI. */
  shader_point_density->cached_resolution = shader_point_density->resolution;

  /* Single-threaded sampling of the voxel domain. */
  RE_point_density_cache(depsgraph, pd);
}

void rna_ShaderNodePointDensity_density_calc(bNode *self,
                                             Depsgraph *depsgraph,
                                             float **values,
                                             int *values_num)
{
  NodeShaderTexPointDensity *shader_point_density = static_cast<NodeShaderTexPointDensity *>(
      self->storage);
  PointDensity *pd = &shader_point_density->pd;
  const int resolution = shader_point_density->cached_resolution;

  if (depsgraph == nullptr) {
    *values_num = 0;
    return;
  }

  /* TODO(sergey): Will likely overflow, but how to pass size_t via RNA? */
  *values_num = 4 * resolution * resolution * resolution;

  if (*values == nullptr) {
    *values = static_cast<float *>(
        MEM_mallocN(sizeof(float) * (*values_num), "point density dynamic array"));
  }

  /* Single-threaded sampling of the voxel domain. */
  RE_point_density_sample(depsgraph, pd, resolution, *values);

  /* We're done, time to clean up. */
  BKE_texture_pointdensity_free_data(pd);
  *pd = blender::dna::shallow_zero_initialize();

  shader_point_density->cached_resolution = 0.0f;
}

void rna_ShaderNodePointDensity_density_minmax(bNode *self,
                                               Depsgraph *depsgraph,
                                               float r_min[3],
                                               float r_max[3])
{
  NodeShaderTexPointDensity *shader_point_density = static_cast<NodeShaderTexPointDensity *>(
      self->storage);
  PointDensity *pd = &shader_point_density->pd;

  if (depsgraph == nullptr) {
    zero_v3(r_min);
    zero_v3(r_max);
    return;
  }

  RE_point_density_minmax(depsgraph, pd, r_min, r_max);
}

static int rna_NodeConvertColorSpace_from_color_space_get(PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  return IMB_colormanagement_colorspace_get_named_index(node_storage->from_color_space);
}

static void rna_NodeConvertColorSpace_from_color_space_set(PointerRNA *ptr, int value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  const char *name = IMB_colormanagement_colorspace_get_indexed_name(value);

  if (name && name[0]) {
    STRNCPY(node_storage->from_color_space, name);
  }
}
static int rna_NodeConvertColorSpace_to_color_space_get(PointerRNA *ptr)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  return IMB_colormanagement_colorspace_get_named_index(node_storage->to_color_space);
}

static void rna_NodeConvertColorSpace_to_color_space_set(PointerRNA *ptr, int value)
{
  bNode *node = static_cast<bNode *>(ptr->data);
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  const char *name = IMB_colormanagement_colorspace_get_indexed_name(value);

  if (name && name[0]) {
    STRNCPY(node_storage->to_color_space, name);
  }
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

static bNode *find_node_by_enum_definition(bNodeTree *ntree, const NodeEnumDefinition *enum_def)
{
  ntree->ensure_topology_cache();
  for (bNode *node : ntree->nodes_by_type("GeometryNodeMenuSwitch")) {
    NodeMenuSwitch *storage = static_cast<NodeMenuSwitch *>(node->storage);
    if (&storage->enum_definition == enum_def) {
      return node;
    }
  }
  return nullptr;
}

[[maybe_unused]] static bNode *find_node_by_enum_item(bNodeTree *ntree, const NodeEnumItem *item)
{
  ntree->ensure_topology_cache();
  for (bNode *node : ntree->nodes_by_type("GeometryNodeMenuSwitch")) {
    NodeMenuSwitch *storage = static_cast<NodeMenuSwitch *>(node->storage);
    if (storage->enum_definition.items().contains_ptr(item)) {
      return node;
    }
  }
  return nullptr;
}

static NodeEnumDefinition *find_enum_definition_by_item(bNodeTree *ntree, const NodeEnumItem *item)
{
  ntree->ensure_topology_cache();
  for (bNode *node : ntree->nodes_by_type("GeometryNodeMenuSwitch")) {
    NodeMenuSwitch *storage = static_cast<NodeMenuSwitch *>(node->storage);
    if (storage->enum_definition.items().contains_ptr(item)) {
      return &storage->enum_definition;
    }
  }
  return nullptr;
}

/* Tag the node owning the enum definition to ensure propagation of the enum. */
static void rna_NodeEnumDefinition_tag_changed(bNodeTree *ntree, NodeEnumDefinition *enum_def)
{
  bNode *node = find_node_by_enum_definition(ntree, enum_def);
  BLI_assert(node != nullptr);
  BKE_ntree_update_tag_node_property(ntree, node);
}

static void rna_NodeEnumItem_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  BLI_assert(find_node_by_enum_item(ntree, static_cast<NodeEnumItem *>(ptr->data)) != nullptr);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

static void rna_NodeEnumItem_name_set(PointerRNA *ptr, const char *value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  NodeEnumItem *item = static_cast<NodeEnumItem *>(ptr->data);
  NodeEnumDefinition *enum_def = find_enum_definition_by_item(ntree, item);
  enum_def->set_item_name(*item, value);
  rna_NodeEnumDefinition_tag_changed(ntree, enum_def);
}

static NodeEnumItem *rna_NodeEnumDefinition_new(
    ID *id, NodeEnumDefinition *enum_def, Main *bmain, ReportList *reports, const char *name)
{
  NodeEnumItem *item = enum_def->add_item(name);
  if (item == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to create enum item");
  }
  else {
    bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
    rna_NodeEnumDefinition_tag_changed(ntree, enum_def);
    ED_node_tree_propagate_change(nullptr, bmain, ntree);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }
  return item;
}

static void rna_NodeEnumDefinition_remove(
    ID *id, NodeEnumDefinition *enum_def, Main *bmain, ReportList *reports, NodeEnumItem *item)
{
  if (!enum_def->remove_item(*item)) {
    BKE_reportf(reports, RPT_ERROR, "Unable to remove item '%s' from enum definition", item->name);
  }
  else {
    bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
    rna_NodeEnumDefinition_tag_changed(ntree, enum_def);
    ED_node_tree_propagate_change(nullptr, bmain, ntree);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }
}

static void rna_NodeEnumDefinition_clear(ID *id, NodeEnumDefinition *enum_def, Main *bmain)
{
  enum_def->clear();
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  rna_NodeEnumDefinition_tag_changed(ntree, enum_def);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeEnumDefinition_move(
    ID *id, NodeEnumDefinition *enum_def, Main *bmain, int from_index, int to_index)
{
  enum_def->move_item(from_index, to_index);
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  rna_NodeEnumDefinition_tag_changed(ntree, enum_def);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static PointerRNA rna_NodeEnumDefinition_active_item_get(PointerRNA *ptr)
{
  NodeEnumDefinition *enum_def = static_cast<NodeEnumDefinition *>(ptr->data);
  NodeEnumItem *item = enum_def->active_item();
  PointerRNA r_ptr = RNA_pointer_create(ptr->owner_id, &RNA_NodeEnumItem, item);
  return r_ptr;
}

static void rna_NodeEnumDefinition_active_item_set(PointerRNA *ptr,
                                                   PointerRNA value,
                                                   ReportList * /*reports*/)
{
  NodeEnumDefinition *enum_def = static_cast<NodeEnumDefinition *>(ptr->data);
  NodeEnumItem *item = static_cast<NodeEnumItem *>(value.data);
  enum_def->active_item_set(item);
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

static const EnumPropertyItem prop_tri_channel_items[] = {
    {1, "R", 0, "R", "Red"},
    {2, "G", 0, "G", "Green"},
    {3, "B", 0, "B", "Blue"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_flip_items[] = {
    {0, "X", 0, "Flip X", ""},
    {1, "Y", 0, "Flip Y", ""},
    {2, "XY", 0, "Flip X & Y", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_ycc_items[] = {
    {0, "ITUBT601", 0, "ITU 601", ""},
    {1, "ITUBT709", 0, "ITU 709", ""},
    {2, "JFIF", 0, "JPEG", ""},
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
     "more noisy when viewing from a distance"},
    {SHD_PRINCIPLED_HAIR_HUANG,
     "HUANG",
     0,
     "Huang",
     "Multi-scale hair scattering model by Huang et al. 2022, suitable for viewing both up close "
     "and from a distance, supports elliptical cross-sections and has more precise highlight in "
     "forward scattering directions"},
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
     "automatically adjusted to match color textures. Designed for skin shading"},
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

/* -- Common nodes ---------------------------------------------------------- */

static void def_group_input(StructRNA * /*srna*/) {}

static void def_group_output(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "is_active_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_DO_OUTPUT);
  RNA_def_property_ui_text(
      prop, "Active Output", "True if this node is used as the active group output");
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_GroupOutput_is_active_output_set");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_group(StructRNA *srna)
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

  def_group(srna);
}

static void def_frame(StructRNA *srna)
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

static void def_clamp(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clamp_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_clamp_items);
  RNA_def_property_ui_text(prop, "Clamp Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_map_range(StructRNA *srna)
{
  static const EnumPropertyItem rna_enum_data_type_items[] = {
      {CD_PROP_FLOAT, "FLOAT", 0, "Float", "Floating-point value"},
      {CD_PROP_FLOAT3, "FLOAT_VECTOR", 0, "Vector", "3D vector with floating-point values"},
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

static void def_math(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_math_items);
  RNA_def_property_ui_text(prop, "Operation", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_NODETREE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");

  prop = RNA_def_property(srna, "use_clamp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom2", SHD_MATH_CLAMP);
  RNA_def_property_ui_text(prop, "Clamp", "Clamp result of the node to 0.0 to 1.0 range");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_mix(StructRNA *srna)
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

static void def_float_to_int(StructRNA *srna)
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

static void def_vector_math(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_vec_math_items);
  RNA_def_property_ui_text(prop, "Operation", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_NODETREE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_rgb_curve(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_vector_curve(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_float_curve(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_time(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_ui_text(prop, "Start Frame", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom2");
  RNA_def_property_ui_text(prop, "End Frame", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_colorramp(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "ColorRamp");
  RNA_def_property_ui_text(prop, "Color Ramp", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_mix_rgb(StructRNA *srna)
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

static void def_texture(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Texture");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Texture", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "node_output", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_ui_text(
      prop, "Node Output", "For node-based textures, which output node to use");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_color(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputColor", "storage");

  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_sdna(prop, nullptr, "color");
  RNA_def_property_ui_text(prop, "Color", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_bool(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputBool", "storage");

  prop = RNA_def_property(srna, "boolean", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "boolean", 1);
  RNA_def_property_ui_text(prop, "Boolean", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_int(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputInt", "storage");

  prop = RNA_def_property(srna, "integer", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "integer");
  RNA_def_property_int_default(prop, 1);
  RNA_def_property_ui_text(prop, "Integer", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_vector(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputVector", "storage");

  prop = RNA_def_property(srna, "vector", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_sdna(prop, nullptr, "vector");
  RNA_def_property_ui_text(prop, "Vector", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_fn_input_string(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeInputString", "storage");

  prop = RNA_def_property(srna, "string", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "String", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* -- Shader Nodes ---------------------------------------------------------- */

static void def_sh_output(StructRNA *srna)
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

static void def_sh_output_linestyle(StructRNA *srna)
{
  def_sh_output(srna);
  def_mix_rgb(srna);
}

static void def_sh_mapping(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "vector_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_mapping_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of vector that the mapping transforms");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_sh_vector_rotate(StructRNA *srna)
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

static void def_sh_attribute(StructRNA *srna)
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
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "attribute_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Attribute Name", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_tex(StructRNA *srna)
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

static void def_sh_tex_sky(StructRNA *srna)
{
  static const EnumPropertyItem prop_sky_type[] = {
      {SHD_SKY_PREETHAM, "PREETHAM", 0, "Preetham", "Preetham 1999"},
      {SHD_SKY_HOSEK, "HOSEK_WILKIE", 0, "Hosek / Wilkie", "Hosek / Wilkie 2012"},
      {SHD_SKY_NISHITA, "NISHITA", 0, "Nishita", "Nishita 1993 improved"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static float default_dir[3] = {0.0f, 0.0f, 1.0f};

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeTexSky", "storage");
  def_sh_tex(srna);

  prop = RNA_def_property(srna, "sky_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "sky_model");
  RNA_def_property_enum_items(prop, prop_sky_type);
  RNA_def_property_ui_text(prop, "Sky Type", "Which sky model should be used");
  RNA_def_property_update(prop, 0, "rna_ShaderNode_socket_update");

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
  RNA_def_property_ui_text(prop, "Sun Intensity", "Strength of sun");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "sun_elevation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_ui_text(prop, "Sun Elevation", "Sun angle from horizon");
  RNA_def_property_float_default(prop, M_PI_2);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "sun_rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_ui_text(prop, "Sun Rotation", "Rotation of sun around zenith");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "altitude", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_ui_text(prop, "Altitude", "Height from sea level");
  RNA_def_property_range(prop, 0.0f, 60000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 60000.0f, 10, 1);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "air_density", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop, "Air", "Density of air molecules");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "dust_density", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop, "Dust", "Density of dust molecules and water droplets");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "ozone_density", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop, "Ozone", "Density of ozone layer");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static const EnumPropertyItem sh_tex_prop_interpolation_items[] = {
    {SHD_INTERP_LINEAR, "Linear", 0, "Linear", "Linear interpolation"},
    {SHD_INTERP_CLOSEST, "Closest", 0, "Closest", "No interpolation (sample closest texel)"},
    {SHD_INTERP_CUBIC, "Cubic", 0, "Cubic", "Cubic interpolation"},
    {SHD_INTERP_SMART, "Smart", 0, "Smart", "Bicubic when magnifying, else bilinear (OSL only)"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void def_sh_tex_environment(StructRNA *srna)
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
  def_sh_tex(srna);

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

static void def_sh_tex_image(StructRNA *srna)
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
  def_sh_tex(srna);

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

static void def_tex_combsep_color(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_combsep_color_items);
  RNA_def_property_ui_text(prop, "Mode", "Mode of color processing");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void def_geo_image_texture(StructRNA *srna)
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

static void def_sh_tex_gradient(StructRNA *srna)
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
  def_sh_tex(srna);

  prop = RNA_def_property(srna, "gradient_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_gradient_type);
  RNA_def_property_ui_text(prop, "Gradient Type", "Style of the color blending");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_noise(StructRNA *srna)
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
  def_sh_tex(srna);

  prop = RNA_def_property(srna, "noise_dimensions", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "dimensions");
  RNA_def_property_enum_items(prop, rna_enum_node_tex_dimensions_items);
  RNA_def_property_ui_text(prop, "Dimensions", "Number of dimensions to output noise for");
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

static void def_sh_tex_checker(StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeTexChecker", "storage");
  def_sh_tex(srna);
}

static void def_sh_tex_brick(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeTexBrick", "storage");
  def_sh_tex(srna);

  prop = RNA_def_property(srna, "offset_frequency", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "offset_freq");
  RNA_def_property_int_default(prop, 2);
  RNA_def_property_range(prop, 1, 99);
  RNA_def_property_ui_text(prop, "Offset Frequency", "");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "squash_frequency", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "squash_freq");
  RNA_def_property_int_default(prop, 2);
  RNA_def_property_range(prop, 1, 99);
  RNA_def_property_ui_text(prop, "Squash Frequency", "");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "offset");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Offset Amount", "");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "squash", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "squash");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.0f, 99.0f);
  RNA_def_property_ui_text(prop, "Squash Amount", "");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_magic(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeTexMagic", "storage");
  def_sh_tex(srna);

  prop = RNA_def_property(srna, "turbulence_depth", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "depth");
  RNA_def_property_range(prop, 0, 10);
  RNA_def_property_ui_text(prop, "Depth", "Level of detail in the added turbulent noise");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_voronoi(StructRNA *srna)
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
       "Smoothed version of F1. Weighted sum of neighbor voronoi cells"},
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
  def_sh_tex(srna);

  prop = RNA_def_property(srna, "voronoi_dimensions", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "dimensions");
  RNA_def_property_enum_items(prop, rna_enum_node_tex_dimensions_items);
  RNA_def_property_ui_text(prop, "Dimensions", "Number of dimensions to output noise for");
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

static void def_sh_tex_wave(StructRNA *srna)
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
  def_sh_tex(srna);

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

static void def_sh_tex_white_noise(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "noise_dimensions", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_tex_dimensions_items);
  RNA_def_property_ui_text(prop, "Dimensions", "Number of dimensions to output noise for");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_sh_tex_coord(StructRNA *srna)
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

static void def_sh_vect_transform(StructRNA *srna)
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

static void def_sh_tex_wireframe(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_pixel_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(prop, "Pixel Size", "Use screen pixel size instead of world units");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_tex_pointdensity(StructRNA *srna)
{
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem point_source_items[] = {
      {SHD_POINTDENSITY_SOURCE_PSYS,
       "PARTICLE_SYSTEM",
       0,
       "Particle System",
       "Generate point density from a particle system"},
      {SHD_POINTDENSITY_SOURCE_OBJECT,
       "OBJECT",
       0,
       "Object Vertices",
       "Generate point density from an object's vertices"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_interpolation_items[] = {
      {SHD_INTERP_CLOSEST, "Closest", 0, "Closest", "No interpolation (sample closest texel)"},
      {SHD_INTERP_LINEAR, "Linear", 0, "Linear", "Linear interpolation"},
      {SHD_INTERP_CUBIC, "Cubic", 0, "Cubic", "Cubic interpolation"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem space_items[] = {
      {SHD_POINTDENSITY_SPACE_OBJECT, "OBJECT", 0, "Object Space", ""},
      {SHD_POINTDENSITY_SPACE_WORLD, "WORLD", 0, "World Space", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem particle_color_source_items[] = {
      {SHD_POINTDENSITY_COLOR_PARTAGE,
       "PARTICLE_AGE",
       0,
       "Particle Age",
       "Lifetime mapped as 0.0 to 1.0 intensity"},
      {SHD_POINTDENSITY_COLOR_PARTSPEED,
       "PARTICLE_SPEED",
       0,
       "Particle Speed",
       "Particle speed (absolute magnitude of velocity) mapped as 0.0 to 1.0 intensity"},
      {SHD_POINTDENSITY_COLOR_PARTVEL,
       "PARTICLE_VELOCITY",
       0,
       "Particle Velocity",
       "XYZ velocity mapped to RGB colors"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem vertex_color_source_items[] = {
      {SHD_POINTDENSITY_COLOR_VERTCOL, "VERTEX_COLOR", 0, "Vertex Color", "Vertex color layer"},
      {SHD_POINTDENSITY_COLOR_VERTWEIGHT,
       "VERTEX_WEIGHT",
       0,
       "Vertex Weight",
       "Vertex group weight"},
      {SHD_POINTDENSITY_COLOR_VERTNOR,
       "VERTEX_NORMAL",
       0,
       "Vertex Normal",
       "XYZ normal vector mapped to RGB colors"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Object", "Object to take point data from");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeShaderTexPointDensity", "storage");

  prop = RNA_def_property(srna, "point_source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, point_source_items);
  RNA_def_property_ui_text(prop, "Point Source", "Point data to use as renderable point density");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Particle System", "Particle System to render as points");
  RNA_def_property_struct_type(prop, "ParticleSystem");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_ShaderNodePointDensity_psys_get",
                                 "rna_ShaderNodePointDensity_psys_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "resolution", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 32768);
  RNA_def_property_ui_text(
      prop, "Resolution", "Resolution used by the texture holding the point density");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "radius");
  RNA_def_property_range(prop, 0.001, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Radius", "Radius from the shaded sample to look for points within");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, space_items);
  RNA_def_property_ui_text(prop, "Space", "Coordinate system to calculate voxels in");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_interpolation_items);
  RNA_def_property_ui_text(prop, "Interpolation", "Texture interpolation");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "particle_color_source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "color_source");
  RNA_def_property_enum_items(prop, particle_color_source_items);
  RNA_def_property_ui_text(prop, "Color Source", "Data to derive color results from");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "vertex_color_source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "ob_color_source");
  RNA_def_property_enum_items(prop, vertex_color_source_items);
  RNA_def_property_ui_text(prop, "Color Source", "Data to derive color results from");
  RNA_def_property_update(prop, 0, "rna_Node_update");

  prop = RNA_def_property(srna, "vertex_attribute_name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Vertex Attribute Name", "Vertex attribute to use for color");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  func = RNA_def_function(srna, "cache_point_density", "rna_ShaderNodePointDensity_density_cache");
  RNA_def_function_ui_description(func, "Cache point density data for later calculation");
  RNA_def_pointer(func, "depsgraph", "Depsgraph", "", "");

  func = RNA_def_function(srna, "calc_point_density", "rna_ShaderNodePointDensity_density_calc");
  RNA_def_function_ui_description(func, "Calculate point density");
  RNA_def_pointer(func, "depsgraph", "Depsgraph", "", "");
  /* TODO: See how array size of 0 works, this shouldn't be used. */
  parm = RNA_def_float_array(func, "rgba_values", 1, nullptr, 0, 0, "", "RGBA Values", 0, 0);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, ParameterFlag(0));
  RNA_def_function_output(func, parm);

  func = RNA_def_function(
      srna, "calc_point_density_minmax", "rna_ShaderNodePointDensity_density_minmax");
  RNA_def_function_ui_description(func, "Calculate point density");
  RNA_def_pointer(func, "depsgraph", "Depsgraph", "", "");
  parm = RNA_def_property(func, "min", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_array(parm, 3);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
  parm = RNA_def_property(func, "max", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_array(parm, 3);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
}

static void def_glossy(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_glossy_items);
  RNA_def_property_ui_text(prop, "Distribution", "Light scattering distribution on rough surface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_glass(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_glass_items);
  RNA_def_property_ui_text(prop, "Distribution", "Light scattering distribution on rough surface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sheen(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_sheen_items);
  RNA_def_property_ui_text(prop, "Distribution", "Sheen shading model");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_principled(StructRNA *srna)
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

static void def_refraction(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_refraction_items);
  RNA_def_property_ui_text(prop, "Distribution", "Light scattering distribution on rough surface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_toon(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "component", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_toon_items);
  RNA_def_property_ui_text(prop, "Component", "Toon BSDF component to use");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_bump(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(
      prop, "Invert", "Invert the bump mapping direction to push into the surface instead of out");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_hair(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "component", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_hair_items);
  RNA_def_property_ui_text(prop, "Component", "Hair BSDF component to use");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* RNA initialization for the custom properties. */
static void def_hair_principled(StructRNA *srna)
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

static void def_sh_uvmap(StructRNA *srna)
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

static void def_sh_vertex_color(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeShaderVertexColor", "storage");

  prop = RNA_def_property(srna, "layer_name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Color Attribute", "Color Attribute");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "bNode", nullptr);
}

static void def_sh_uvalongstroke(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_tips", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(
      prop, "Use Tips", "Lower half of the texture is for tips of the stroke");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_normal_map(StructRNA *srna)
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

static void def_sh_displacement(StructRNA *srna)
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

static void def_sh_vector_displacement(StructRNA *srna)
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

static void def_sh_tangent(StructRNA *srna)
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

static void def_sh_bevel(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "samples", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_range(prop, 2, 128);
  RNA_def_property_ui_range(prop, 2, 16, 1, 1);
  RNA_def_property_ui_text(prop, "Samples", "Number of rays to trace per shader evaluation");
  RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_ambient_occlusion(StructRNA *srna)
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

static void def_sh_subsurface(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "falloff", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_subsurface_method_items);
  RNA_def_property_ui_text(prop, "Method", "Method for rendering subsurface scattering");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNode_socket_update");
}

static void def_sh_tex_ies(StructRNA *srna)
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
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_ShaderNodeTexIES_mode_set", nullptr);
  RNA_def_property_enum_items(prop, node_ies_mode_items);
  RNA_def_property_ui_text(
      prop, "Source", "Whether the IES file is loaded from disk or from a text data-block");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "bNode", nullptr);
}

static void def_sh_output_aov(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeShaderOutputAOV", "storage");

  prop = RNA_def_property(srna, "aov_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Name", "Name of the AOV that this output writes to");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "bNode", nullptr);
}

static void def_sh_combsep_color(StructRNA *srna)
{
  static const EnumPropertyItem type_items[] = {
      {NODE_COMBSEP_COLOR_RGB, "RGB", ICON_NONE, "RGB", "Use RGB color processing"},
      {NODE_COMBSEP_COLOR_HSV, "HSV", ICON_NONE, "HSV", "Use HSV color processing"},
      {NODE_COMBSEP_COLOR_HSL, "HSL", ICON_NONE, "HSL", "Use HSL color processing"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeCombSepColor", "storage");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, type_items);
  RNA_def_property_ui_text(prop, "Mode", "Mode of color processing");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void def_sh_script(StructRNA *srna)
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

#  if 0 /* XXX TODO: use general node api for this. */
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

static void def_cmp_alpha_over(StructRNA *srna)
{
  PropertyRNA *prop;

  /* XXX: Tooltip */
  prop = RNA_def_property(srna, "use_premultiply", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(prop, "Convert Premultiplied", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeTwoFloats", "storage");

  prop = RNA_def_property(srna, "premul", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Premultiplied", "Mix Factor");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_blur(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem filter_type_items[] = {
      {R_FILTER_BOX, "FLAT", 0, "Flat", ""},
      {R_FILTER_TENT, "TENT", 0, "Tent", ""},
      {R_FILTER_QUAD, "QUAD", 0, "Quadratic", ""},
      {R_FILTER_CUBIC, "CUBIC", 0, "Cubic", ""},
      {R_FILTER_GAUSS, "GAUSS", 0, "Gaussian", ""},
      {R_FILTER_FAST_GAUSS, "FAST_GAUSS", 0, "Fast Gaussian", ""},
      {R_FILTER_CATROM, "CATROM", 0, "Catrom", ""},
      {R_FILTER_MITCH, "MITCH", 0, "Mitch", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem aspect_correction_type_items[] = {
      {CMP_NODE_BLUR_ASPECT_NONE, "NONE", 0, "None", ""},
      {CMP_NODE_BLUR_ASPECT_Y, "Y", 0, "Y", ""},
      {CMP_NODE_BLUR_ASPECT_X, "X", 0, "X", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* duplicated in def_cmp_bokehblur */
  prop = RNA_def_property(srna, "use_variable_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", CMP_NODEFLAG_BLUR_VARIABLE_SIZE);
  RNA_def_property_ui_text(
      prop, "Variable Size", "Support variable blur per pixel when using an image for size input");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_extended_bounds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", CMP_NODEFLAG_BLUR_EXTEND_BOUNDS);
  RNA_def_property_ui_text(
      prop, "Extend Bounds", "Extend bounds of the input image to fully fit blurred image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeBlurData", "storage");

  prop = RNA_def_property(srna, "size_x", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "sizex");
  RNA_def_property_range(prop, 0, 2048);
  RNA_def_property_ui_text(prop, "Size X", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "size_y", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "sizey");
  RNA_def_property_range(prop, 0, 2048);
  RNA_def_property_ui_text(prop, "Size Y", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_relative", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "relative", 1);
  RNA_def_property_ui_text(
      prop, "Relative", "Use relative (percent) values to define blur radius");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "aspect_correction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "aspect");
  RNA_def_property_enum_items(prop, aspect_correction_type_items);
  RNA_def_property_ui_text(prop, "Aspect Correction", "Type of aspect correction to use");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fac");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_text(prop, "Factor", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "factor_x", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, nullptr, "percentx");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Relative Size X", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "factor_y", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, nullptr, "percenty");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Relative Size Y", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "filtertype");
  RNA_def_property_enum_items(prop, filter_type_items);
  RNA_def_property_ui_text(prop, "Filter Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_NODETREE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_bokeh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bokeh", 1);
  RNA_def_property_ui_text(prop, "Bokeh", "Use circular filter (slower)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_gamma_correction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gamma", 1);
  RNA_def_property_ui_text(prop, "Gamma", "Apply filter on gamma corrected values");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_filter(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_filter_items);
  RNA_def_property_ui_text(prop, "Filter Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_map_value(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "TexMapping", "storage");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "loc");
  RNA_def_property_array(prop, 1);
  RNA_def_property_range(prop, -1000.0f, 1000.0f);
  RNA_def_property_ui_text(prop, "Offset", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "size");
  RNA_def_property_array(prop, 1);
  RNA_def_property_range(prop, -1000.0f, 1000.0f);
  RNA_def_property_ui_text(prop, "Size", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_min", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TEXMAP_CLIP_MIN);
  RNA_def_property_ui_text(prop, "Use Minimum", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_max", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TEXMAP_CLIP_MAX);
  RNA_def_property_ui_text(prop, "Use Maximum", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "min");
  RNA_def_property_array(prop, 1);
  RNA_def_property_range(prop, -1000.0f, 1000.0f);
  RNA_def_property_ui_text(prop, "Minimum", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max");
  RNA_def_property_array(prop, 1);
  RNA_def_property_range(prop, -1000.0f, 1000.0f);
  RNA_def_property_ui_text(prop, "Maximum", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_map_range(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_clamp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(prop, "Clamp", "Clamp the result of the node to the target range");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_vector_blur(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeBlurData", "storage");

  prop = RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "samples");
  RNA_def_property_range(prop, 1, 256);
  RNA_def_property_ui_text(prop, "Samples", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "speed_min", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "minspeed");
  RNA_def_property_range(prop, 0, 1024);
  RNA_def_property_ui_text(
      prop,
      "Min Speed",
      "Minimum speed for a pixel to be blurred (used to separate background from foreground)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "speed_max", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "maxspeed");
  RNA_def_property_range(prop, 0, 1024);
  RNA_def_property_ui_text(prop, "Max Speed", "Maximum speed, or zero for none");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fac");
  RNA_def_property_range(prop, 0.0, 20.0);
  RNA_def_property_ui_range(prop, 0.0, 2.0, 1.0, 2);
  RNA_def_property_ui_text(
      prop,
      "Blur Factor",
      "Scaling factor for motion vectors (actually, 'shutter speed', in frames)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_curved", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "curved", 1);
  RNA_def_property_ui_text(
      prop, "Curved", "Interpolate between frames in a Bézier curve, rather than linearly");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_set_alpha(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem mode_items[] = {
      {CMP_NODE_SETALPHA_MODE_APPLY,
       "APPLY",
       0,
       "Apply Mask",
       "Multiply the input image's RGBA channels by the alpha input value"},
      {CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA,
       "REPLACE_ALPHA",
       0,
       "Replace Alpha",
       "Replace the input image's alpha channel by the alpha input value"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_struct_sdna_from(srna, "NodeSetAlpha", "storage");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_levels(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem channel_items[] = {
      {CMP_NODE_LEVLES_LUMINANCE, "COMBINED_RGB", 0, "Combined", "Combined RGB"},
      {CMP_NODE_LEVLES_RED, "RED", 0, "Red", "Red Channel"},
      {CMP_NODE_LEVLES_GREEN, "GREEN", 0, "Green", "Green Channel"},
      {CMP_NODE_LEVLES_BLUE, "BLUE", 0, "Blue", "Blue Channel"},
      {CMP_NODE_LEVLES_LUMINANCE_BT709, "LUMINANCE", 0, "Luminance", "Luminance Channel"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "channel", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, channel_items);
  RNA_def_property_ui_text(prop, "Channel", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_node_image_user(StructRNA *srna)
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

static void def_cmp_image(StructRNA *srna)
{
  PropertyRNA *prop;

#  if 0
  static const EnumPropertyItem type_items[] = {
      {IMA_SRC_FILE, "IMAGE", 0, "Image", ""},
      {IMA_SRC_MOVIE, "MOVIE", "Movie", ""},
      {IMA_SRC_SEQUENCE, "SEQUENCE", "Sequence", ""},
      {IMA_SRC_GENERATED, "GENERATED", "Generated", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
#  endif

  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Image_Node_update_id");

  prop = RNA_def_property(srna, "use_straight_alpha_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", CMP_NODE_IMAGE_USE_STRAIGHT_OUTPUT);
  RNA_def_property_ui_text(prop,
                           "Straight Alpha Output",
                           "Put node output buffer to straight alpha instead of premultiplied");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  /* NOTE: Image user properties used in the UI are redefined in def_node_image_user,
   * to trigger correct updates of the node editor. RNA design problem that prevents
   * updates from nested structs. */
  RNA_def_struct_sdna_from(srna, "ImageUser", "storage");
  def_node_image_user(srna);
}

static void def_cmp_render_layers(StructRNA *srna)
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

static void rna_def_cmp_output_file_slot_file(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeOutputFileSlotFile", nullptr);
  RNA_def_struct_sdna(srna, "NodeImageMultiFileSocket");
  RNA_def_struct_ui_text(
      srna, "Output File Slot", "Single layer file slot of the file output node");

  prop = RNA_def_property(srna, "use_node_format", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_node_format", 1);
  RNA_def_property_ui_text(prop, "Use Node Format", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "save_as_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "save_as_render", 1);
  RNA_def_property_ui_text(
      prop, "Save as Render", "Apply render part of display transform when saving byte image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "format", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ImageFormatSettings");

  prop = RNA_def_property(srna, "path", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "path");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_NodeOutputFileSlotFile_path_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_ui_text(prop, "Path", "Subpath used for this slot");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_FILEBROWSER);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);
}
static void rna_def_cmp_output_file_slot_layer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeOutputFileSlotLayer", nullptr);
  RNA_def_struct_sdna(srna, "NodeImageMultiFileSocket");
  RNA_def_struct_ui_text(
      srna, "Output File Layer Slot", "Multilayer slot of the file output node");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layer");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_NodeOutputFileSlotLayer_name_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_ui_text(prop, "Name", "OpenEXR layer name used for this slot");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);
}
static void rna_def_cmp_output_file_slots_api(BlenderRNA *brna,
                                              PropertyRNA *cprop,
                                              const char *struct_name)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, struct_name);
  srna = RNA_def_struct(brna, struct_name, nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "File Output Slots", "Collection of File Output node slots");

  func = RNA_def_function(srna, "new", "rna_NodeOutputFile_slots_new");
  RNA_def_function_ui_description(func, "Add a file slot to this node");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS | FUNC_USE_CONTEXT);
  parm = RNA_def_string(func, "name", nullptr, MAX_NAME, "Name", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return value */
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "New socket");
  RNA_def_function_return(func, parm);

  /* NOTE: methods below can use the standard node socket API functions,
   * included here for completeness. */

  func = RNA_def_function(srna, "remove", "rna_Node_socket_remove");
  RNA_def_function_ui_description(func, "Remove a file slot from this node");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "The socket to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "clear", "rna_Node_inputs_clear");
  RNA_def_function_ui_description(func, "Remove all file slots from this node");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);

  func = RNA_def_function(srna, "move", "rna_Node_inputs_move");
  RNA_def_function_ui_description(func, "Move a file slot to another position");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "from_index", -1, 0, INT_MAX, "From Index", "Index of the socket to move", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(
      func, "to_index", -1, 0, INT_MAX, "To Index", "Target index for the socket", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
static void def_cmp_output_file(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeImageMultiFile", "storage");

  prop = RNA_def_property(srna, "base_path", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, nullptr, "base_path");
  RNA_def_property_ui_text(prop, "Base Path", "Base output path for the image");
  RNA_def_property_flag(prop, PROP_PATH_OUTPUT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "active_input_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "active_input");
  RNA_def_property_ui_text(prop, "Active Input Index", "Active input index in details view list");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "format", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ImageFormatSettings");

  /* XXX using two different collections here for the same basic DNA list!
   * Details of the output slots depend on whether the node is in Multilayer EXR mode.
   */

  prop = RNA_def_property(srna, "file_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_NodeOutputFile_slots_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_NodeOutputFile_slot_file_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "NodeOutputFileSlotFile");
  RNA_def_property_ui_text(prop, "File Slots", "");
  rna_def_cmp_output_file_slots_api(brna, prop, "CompositorNodeOutputFileFileSlots");

  prop = RNA_def_property(srna, "layer_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_NodeOutputFile_slots_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_NodeOutputFile_slot_layer_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "NodeOutputFileSlotLayer");
  RNA_def_property_ui_text(prop, "EXR Layer Slots", "");
  rna_def_cmp_output_file_slots_api(brna, prop, "CompositorNodeOutputFileLayerSlots");
}

static void def_cmp_dilate_erode(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem mode_items[] = {
      {CMP_NODE_DILATE_ERODE_STEP, "STEP", 0, "Steps", ""},
      {CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD, "THRESHOLD", 0, "Threshold", ""},
      {CMP_NODE_DILATE_ERODE_DISTANCE, "DISTANCE", 0, "Distance", ""},
      {CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER, "FEATHER", 0, "Feather", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Growing/shrinking mode");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "distance", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom2");
  RNA_def_property_range(prop, -5000, 5000);
  RNA_def_property_ui_range(prop, -100, 100, 1, -1);
  RNA_def_property_ui_text(prop, "Distance", "Distance to grow/shrink (number of iterations)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  /* CMP_NODE_DILATE_ERODE_DISTANCE_THRESH only */
  prop = RNA_def_property(srna, "edge", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom3");
  RNA_def_property_range(prop, -100, 100);
  RNA_def_property_ui_text(prop, "Edge", "Edge to inset");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_IMAGE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeDilateErode", "storage");

  /* CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER only */
  prop = RNA_def_property(srna, "falloff", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "falloff");
  RNA_def_property_enum_items(prop, rna_enum_proportional_falloff_curve_only_items);
  RNA_def_property_ui_text(prop, "Falloff", "Falloff type of the feather");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_inpaint(StructRNA *srna)
{
  PropertyRNA *prop;

#  if 0
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);

  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of inpaint algorithm");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
#  endif

  prop = RNA_def_property(srna, "distance", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom2");
  RNA_def_property_range(prop, 0, 10000);
  RNA_def_property_ui_text(prop, "Distance", "Distance to inpaint (number of iterations)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_despeckle(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom3");
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(prop, "Threshold", "Threshold for detecting pixels to despeckle");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "threshold_neighbor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom4");
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(
      prop, "Neighbor", "Threshold for the number of neighbor pixels that must match");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_scale(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem space_items[] = {
      {CMP_NODE_SCALE_RELATIVE, "RELATIVE", 0, "Relative", ""},
      {CMP_NODE_SCALE_ABSOLUTE, "ABSOLUTE", 0, "Absolute", ""},
      {CMP_NODE_SCALE_RENDER_PERCENT, "SCENE_SIZE", 0, "Scene Size", ""},
      {CMP_NODE_SCALE_RENDER_SIZE, "RENDER_SIZE", 0, "Render Size", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* matching bgpic_camera_frame_items[] */
  static const EnumPropertyItem space_frame_items[] = {
      {CMP_NODE_SCALE_RENDER_SIZE_STRETCH, "STRETCH", 0, "Stretch", ""},
      {CMP_NODE_SCALE_RENDER_SIZE_FIT, "FIT", 0, "Fit", ""},
      {CMP_NODE_SCALE_RENDER_SIZE_CROP, "CROP", 0, "Crop", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, space_items);
  RNA_def_property_ui_text(prop, "Space", "Coordinate space to scale relative to");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_CompositorNodeScale_update");

  /* expose 2 flags as a enum of 3 items */
  prop = RNA_def_property(srna, "frame_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "custom2");
  RNA_def_property_enum_items(prop, space_frame_items);
  RNA_def_property_ui_text(prop, "Frame Method", "How the image fits in the camera frame");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "offset_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom3");
  RNA_def_property_ui_text(prop, "X Offset", "Offset image horizontally (factor of image size)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "offset_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom4");
  RNA_def_property_ui_text(prop, "Y Offset", "Offset image vertically (factor of image size)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_rotate(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_sampler_type_items);
  RNA_def_property_ui_text(prop, "Filter", "Method to use to filter rotation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_diff_matte(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");

  prop = RNA_def_property(srna, "tolerance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t1");
  RNA_def_property_float_funcs(prop, nullptr, "rna_difference_matte_t1_set", nullptr);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Tolerance", "Color distances below this threshold are keyed");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t2");
  RNA_def_property_float_funcs(prop, nullptr, "rna_difference_matte_t2_set", nullptr);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Falloff", "Color distances below this additional threshold are partially keyed");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_color_matte(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");

  prop = RNA_def_property(srna, "color_hue", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t1");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "H", "Hue tolerance for colors to be considered a keying color");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "color_saturation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t2");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "S", "Saturation tolerance for the color");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "color_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t3");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "V", "Value tolerance for the color");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_distance_matte(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem color_space_items[] = {
      {1, "RGB", 0, "RGB", "RGB color space"},
      {2, "YCC", 0, "YCC", "YCbCr suppression"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");

  prop = RNA_def_property(srna, "channel", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "channel");
  RNA_def_property_enum_items(prop, color_space_items);
  RNA_def_property_ui_text(prop, "Channel", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "tolerance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t1");
  RNA_def_property_float_funcs(prop, nullptr, "rna_distance_matte_t1_set", nullptr);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Tolerance", "Color distances below this threshold are keyed");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t2");
  RNA_def_property_float_funcs(prop, nullptr, "rna_distance_matte_t2_set", nullptr);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Falloff", "Color distances below this additional threshold are partially keyed");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_convert_color_space(StructRNA *srna)
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

static void def_cmp_color_spill(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem channel_items[] = {
      {1, "R", 0, "R", "Red spill suppression"},
      {2, "G", 0, "G", "Green spill suppression"},
      {3, "B", 0, "B", "Blue spill suppression"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem limit_channel_items[] = {
      {0, "R", 0, "R", "Limit by red"},
      {1, "G", 0, "G", "Limit by green"},
      {2, "B", 0, "B", "Limit by blue"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem algorithm_items[] = {
      {0, "SIMPLE", 0, "Simple", "Simple limit algorithm"},
      {1, "AVERAGE", 0, "Average", "Average limit algorithm"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "channel", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, channel_items);
  RNA_def_property_ui_text(prop, "Channel", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "limit_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom2");
  RNA_def_property_enum_items(prop, algorithm_items);
  RNA_def_property_ui_text(prop, "Algorithm", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeColorspill", "storage");

  prop = RNA_def_property(srna, "limit_channel", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "limchan");
  RNA_def_property_enum_items(prop, limit_channel_items);
  RNA_def_property_ui_text(prop, "Limit Channel", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "ratio", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "limscale");
  RNA_def_property_range(prop, 0.5f, 1.5f);
  RNA_def_property_ui_text(prop, "Ratio", "Scale limit by value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_unspill", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "unspill", 0);
  RNA_def_property_ui_text(prop, "Unspill", "Compensate all channels (differently) by hand");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "unspill_red", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "uspillr");
  RNA_def_property_range(prop, 0.0f, 1.5f);
  RNA_def_property_ui_text(prop, "R", "Red spillmap scale");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "unspill_green", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "uspillg");
  RNA_def_property_range(prop, 0.0f, 1.5f);
  RNA_def_property_ui_text(prop, "G", "Green spillmap scale");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "unspill_blue", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "uspillb");
  RNA_def_property_range(prop, 0.0f, 1.5f);
  RNA_def_property_ui_text(prop, "B", "Blue spillmap scale");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_luma_matte(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");

  prop = RNA_def_property(srna, "limit_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t1");
  RNA_def_property_float_funcs(prop, nullptr, "rna_Matte_t1_set", nullptr);
  RNA_def_property_ui_range(prop, 0, 1, 0.1f, 3);
  RNA_def_property_ui_text(prop, "High", "Values higher than this setting are 100% opaque");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "limit_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t2");
  RNA_def_property_float_funcs(prop, nullptr, "rna_Matte_t2_set", nullptr);
  RNA_def_property_ui_range(prop, 0, 1, 0.1f, 3);
  RNA_def_property_ui_text(prop, "Low", "Values lower than this setting are 100% keyed");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_brightcontrast(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_premultiply", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(prop, "Convert Premultiplied", "Keep output image premultiplied alpha");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_chroma_matte(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");

  prop = RNA_def_property(srna, "tolerance", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "t1");
  RNA_def_property_float_funcs(prop, nullptr, "rna_Matte_t1_set", nullptr);
  RNA_def_property_range(prop, DEG2RADF(1.0f), DEG2RADF(80.0f));
  RNA_def_property_ui_text(
      prop, "Acceptance", "Tolerance for a color to be considered a keying color");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "t2");
  RNA_def_property_float_funcs(prop, nullptr, "rna_Matte_t2_set", nullptr);
  RNA_def_property_range(prop, 0.0f, DEG2RADF(30.0f));
  RNA_def_property_ui_text(
      prop, "Cutoff", "Tolerance below which colors will be considered as exact matches");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "lift", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fsize");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Lift", "Alpha lift");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "gain", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fstrength");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Falloff", "Alpha falloff");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "shadow_adjust", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t3");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Shadow Adjust", "Adjusts the brightness of any shadows captured");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_channel_matte(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem color_space_items[] = {
      {CMP_NODE_CHANNEL_MATTE_CS_RGB, "RGB", 0, "RGB", "RGB color space"},
      {CMP_NODE_CHANNEL_MATTE_CS_HSV, "HSV", 0, "HSV", "HSV color space"},
      {CMP_NODE_CHANNEL_MATTE_CS_YUV, "YUV", 0, "YUV", "YUV color space"},
      {CMP_NODE_CHANNEL_MATTE_CS_YCC, "YCC", 0, "YCbCr", "YCbCr color space"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem algorithm_items[] = {
      {0, "SINGLE", 0, "Single", "Limit by single channel"},
      {1, "MAX", 0, "Max", "Limit by maximum of other channels"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "color_space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, color_space_items);
  RNA_def_property_ui_text(prop, "Color Space", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "matte_channel", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom2");
  RNA_def_property_enum_items(prop, prop_tri_channel_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Node_channel_itemf");
  RNA_def_property_ui_text(prop, "Channel", "Channel used to determine matte");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");

  prop = RNA_def_property(srna, "limit_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "algorithm");
  RNA_def_property_enum_items(prop, algorithm_items);
  RNA_def_property_ui_text(prop, "Algorithm", "Algorithm to use to limit channel");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "limit_channel", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "channel");
  RNA_def_property_enum_items(prop, prop_tri_channel_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Node_channel_itemf");
  RNA_def_property_ui_text(prop, "Limit Channel", "Limit by this channel's value");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "limit_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t1");
  RNA_def_property_float_funcs(prop, nullptr, "rna_Matte_t1_set", nullptr);
  RNA_def_property_ui_range(prop, 0, 1, 0.1f, 3);
  RNA_def_property_ui_text(prop, "High", "Values higher than this setting are 100% opaque");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "limit_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "t2");
  RNA_def_property_float_funcs(prop, nullptr, "rna_Matte_t2_set", nullptr);
  RNA_def_property_ui_range(prop, 0, 1, 0.1f, 3);
  RNA_def_property_ui_text(prop, "Low", "Values lower than this setting are 100% keyed");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_flip(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_flip_items);
  RNA_def_property_ui_text(prop, "Axis", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_split(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom2");
  RNA_def_property_enum_items(prop, rna_enum_axis_xy_items);
  RNA_def_property_ui_text(prop, "Axis", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "factor", PROP_INT, PROP_FACTOR);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Factor", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_id_mask(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_range(prop, 0, 32767);
  RNA_def_property_ui_text(prop, "Index", "Pass index number to convert to alpha");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_antialiasing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom2", 0);
  RNA_def_property_ui_text(prop, "Anti-Aliasing", "Apply an anti-aliasing filter to the mask");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_double_edge_mask(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem BufEdgeMode_items[] = {
      {0, "BLEED_OUT", 0, "Bleed Out", "Allow mask pixels to bleed along edges"},
      {1, "KEEP_IN", 0, "Keep In", "Restrict mask pixels from touching edges"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem InnerEdgeMode_items[] = {
      {0, "ALL", 0, "All", "All pixels on inner mask edge are considered during mask calculation"},
      {1,
       "ADJACENT_ONLY",
       0,
       "Adjacent Only",
       "Only inner mask pixels adjacent to outer mask pixels are considered during mask "
       "calculation"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "inner_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, InnerEdgeMode_items);
  RNA_def_property_ui_text(prop, "Inner Edge Mode", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "edge_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom2");
  RNA_def_property_enum_items(prop, BufEdgeMode_items);
  RNA_def_property_ui_text(prop, "Buffer Edge Mode", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_map_uv(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem filter_type_items[] = {
      {CMP_NODE_MAP_UV_FILTERING_NEAREST, "NEAREST", 0, "Nearest", ""},
      {CMP_NODE_MAP_UV_FILTERING_ANISOTROPIC, "ANISOTROPIC", 0, "Anisotropic", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "alpha", PROP_INT, PROP_FACTOR);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Alpha", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom2");
  RNA_def_property_enum_items(prop, filter_type_items);
  RNA_def_property_ui_text(prop, "Filter Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_NODETREE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_defocus(StructRNA *srna)
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
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

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

  prop = RNA_def_property(srna, "use_gamma_correction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gamco", 1);
  RNA_def_property_ui_text(
      prop, "Gamma Correction", "Enable gamma correction before and after main process");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  /* TODO */
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

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "bthresh");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(
      prop,
      "Threshold",
      "CoC radius threshold, prevents background bleed on in-focus midground, 0 is disabled");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "preview", 1);
  RNA_def_property_ui_text(prop, "Preview", "Enable low quality mode, useful for preview");
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

static void def_cmp_invert(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "invert_rgb", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", CMP_CHAN_RGB);
  RNA_def_property_ui_text(prop, "RGB", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "invert_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", CMP_CHAN_A);
  RNA_def_property_ui_text(prop, "Alpha", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_crop(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_crop_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_ui_text(prop, "Crop Image Size", "Whether to crop the size of the input image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "relative", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom2", 1);
  RNA_def_property_ui_text(prop, "Relative", "Use relative values to crop image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeTwoXYs", "storage");

  prop = RNA_def_property(srna, "min_x", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "x1");
  RNA_def_property_range(prop, 0, 10000);
  RNA_def_property_ui_text(prop, "X1", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "max_x", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "x2");
  RNA_def_property_range(prop, 0, 10000);
  RNA_def_property_ui_text(prop, "X2", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "min_y", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "y1");
  RNA_def_property_range(prop, 0, 10000);
  RNA_def_property_ui_text(prop, "Y1", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "max_y", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "y2");
  RNA_def_property_range(prop, 0, 10000);
  RNA_def_property_ui_text(prop, "Y2", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "rel_min_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fac_x1");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "X1", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "rel_max_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fac_x2");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "X2", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "rel_min_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fac_y1");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Y1", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "rel_max_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fac_y2");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Y2", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_dblur(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeDBlurData", "storage");

  prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "iter");
  RNA_def_property_range(prop, 1, 32);
  RNA_def_property_ui_text(prop, "Iterations", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "center_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "center_x");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Center X", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "center_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "center_y");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Center Y", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "distance");
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Distance", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "angle");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(360.0f));
  RNA_def_property_ui_text(prop, "Angle", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "spin", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "spin");
  RNA_def_property_range(prop, DEG2RADF(-360.0f), DEG2RADF(360.0f));
  RNA_def_property_ui_text(prop, "Spin", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "zoom", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "zoom");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Zoom", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_bilateral_blur(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeBilateralBlurData", "storage");

  prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "iter");
  RNA_def_property_range(prop, 1, 128);
  RNA_def_property_ui_text(prop, "Iterations", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "sigma_color", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sigma_color");
  RNA_def_property_range(prop, 0.01f, 3.0f);
  RNA_def_property_ui_text(prop, "Color Sigma", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "sigma_space", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sigma_space");
  RNA_def_property_range(prop, 0.01f, 30.0f);
  RNA_def_property_ui_text(prop, "Space Sigma", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_premul_key(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem type_items[] = {
      {0, "STRAIGHT_TO_PREMUL", 0, "To Premultiplied", "Convert straight to premultiplied"},
      {1, "PREMUL_TO_STRAIGHT", 0, "To Straight", "Convert premultiplied to straight"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "mapping", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, type_items);
  RNA_def_property_ui_text(
      prop, "Mapping", "Conversion between premultiplied alpha and key alpha");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_glare(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem type_items[] = {
      {CMP_NODE_GLARE_BLOOM, "BLOOM", 0, "Bloom", ""},
      {CMP_NODE_GLARE_GHOST, "GHOSTS", 0, "Ghosts", ""},
      {CMP_NODE_GLARE_STREAKS, "STREAKS", 0, "Streaks", ""},
      {CMP_NODE_GLARE_FOG_GLOW, "FOG_GLOW", 0, "Fog Glow", ""},
      {CMP_NODE_GLARE_SIMPLE_STAR, "SIMPLE_STAR", 0, "Simple Star", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem quality_items[] = {
      {0, "HIGH", 0, "High", ""},
      {1, "MEDIUM", 0, "Medium", ""},
      {2, "LOW", 0, "Low", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_struct_sdna_from(srna, "NodeGlare", "storage");

  prop = RNA_def_property(srna, "glare_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, type_items);
  RNA_def_property_ui_text(prop, "Glare Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "quality", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "quality");
  RNA_def_property_enum_items(prop, quality_items);
  RNA_def_property_ui_text(
      prop,
      "Quality",
      "If not set to high quality, the effect will be applied to a low-res copy "
      "of the source image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "iter");
  RNA_def_property_range(prop, 2, 5);
  RNA_def_property_ui_text(prop, "Iterations", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "color_modulation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "colmod");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Color Modulation",
      "Amount of Color Modulation, modulates colors of streaks and ghosts for "
      "a spectral dispersion effect");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "mix", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "mix");
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Mix", "-1 is original image only, 0 is exact 50/50 mix, 1 is processed image only");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "threshold");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_ui_text(
      prop,
      "Threshold",
      "The glare filter will only be applied to pixels brighter than this value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "streaks", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "streaks");
  RNA_def_property_range(prop, 1, 16);
  RNA_def_property_ui_text(prop, "Streaks", "Total number of streaks");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "angle_offset", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "angle_ofs");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_text(prop, "Angle Offset", "Streak angle offset");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "fade", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fade");
  RNA_def_property_range(prop, 0.75f, 1.0f);
  RNA_def_property_ui_text(prop, "Fade", "Streak fade-out factor");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_rotate_45", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "star_45", 0);
  RNA_def_property_ui_text(prop, "Rotate 45", "Simple star filter: add 45 degree rotation offset");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "size", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "size");
  RNA_def_property_range(prop, 6, 9);
  RNA_def_property_ui_text(
      prop,
      "Size",
      "Glow/glare size (not actual size; relative to initial size of bright area of pixels)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  /* TODO */
}

static void def_cmp_tonemap(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem type_items[] = {
      {1,
       "RD_PHOTORECEPTOR",
       0,
       "R/D Photoreceptor",
       "More advanced algorithm based on eye physiology, by Reinhard and Devlin"},
      {0, "RH_SIMPLE", 0, "Rh Simple", "Simpler photographic algorithm by Reinhard"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_struct_sdna_from(srna, "NodeTonemap", "storage");

  prop = RNA_def_property(srna, "tonemap_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, type_items);
  RNA_def_property_ui_text(prop, "Tonemap Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "key", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "key");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Key", "The value the average luminance is mapped to");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "offset");
  RNA_def_property_range(prop, 0.001f, 10.0f);
  RNA_def_property_ui_text(
      prop,
      "Offset",
      "Normally always 1, but can be used as an extra control to alter the brightness curve");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "gamma");
  RNA_def_property_range(prop, 0.001f, 3.0f);
  RNA_def_property_ui_text(prop, "Gamma", "If not used, set to 1");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "intensity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "f");
  RNA_def_property_range(prop, -8.0f, 8.0f);
  RNA_def_property_ui_text(
      prop, "Intensity", "If less than zero, darkens image; otherwise, makes it brighter");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "contrast", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "m");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Contrast", "Set to 0 to use estimate from input image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "adaptation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "a");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Adaptation", "If 0, global; if 1, based on pixel intensity");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "correction", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "c");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Color Correction", "If 0, same for all channels; if 1, each independent");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_lensdist(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeLensDist", "storage");

  prop = RNA_def_property(srna, "use_projector", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "proj", 1);
  RNA_def_property_ui_text(
      prop,
      "Projector",
      "Enable/disable projector mode (the effect is applied in horizontal direction only)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_jitter", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "jit", 1);
  RNA_def_property_ui_text(prop, "Jitter", "Enable/disable jittering (faster, but also noisier)");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_NODETREE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_fit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "fit", 1);
  RNA_def_property_ui_text(
      prop,
      "Fit",
      "For positive distortion factor only: scale image such that black areas are not visible");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_colorbalance(StructRNA *srna)
{
  PropertyRNA *prop;
  static float default_1[3] = {1.0f, 1.0f, 1.0f};

  static const EnumPropertyItem type_items[] = {
      {0, "LIFT_GAMMA_GAIN", 0, "Lift/Gamma/Gain", ""},
      {1,
       "OFFSET_POWER_SLOPE",
       0,
       "Offset/Power/Slope (ASC-CDL)",
       "ASC-CDL standard color correction"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "correction_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, type_items);
  RNA_def_property_ui_text(prop, "Correction Formula", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeColorBalance", "storage");

  prop = RNA_def_property(srna, "lift", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "lift");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_1);
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_ui_text(prop, "Lift", "Correction for shadows");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_lgg");

  prop = RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "gamma");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_1);
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_ui_text(prop, "Gamma", "Correction for midtones");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_lgg");

  prop = RNA_def_property(srna, "gain", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "gain");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_1);
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_ui_text(prop, "Gain", "Correction for highlights");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_lgg");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "offset");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
  RNA_def_property_ui_text(prop, "Offset", "Correction for entire tonal range");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_cdl");

  prop = RNA_def_property(srna, "power", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "power");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_1);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_ui_text(prop, "Power", "Correction for midtones");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_cdl");

  prop = RNA_def_property(srna, "slope", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "slope");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_1);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_ui_text(prop, "Slope", "Correction for highlights");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_cdl");

  prop = RNA_def_property(srna, "offset_basis", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -1.0, 1.0, 1.0, 2);
  RNA_def_property_ui_text(prop, "Basis", "Support negative color by using this as the RGB basis");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_cdl");
}

static void def_cmp_huecorrect(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_zcombine(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 0);
  RNA_def_property_ui_text(
      prop, "Use Alpha", "Take alpha channel into account when doing the Z operation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_antialias_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "custom2", 0);
  RNA_def_property_ui_text(
      prop,
      "Anti-Alias Z",
      "Anti-alias the z-buffer to try to avoid artifacts, mostly useful for Blender renders");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_ycc(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_ycc_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_combsep_color(StructRNA *srna)
{
  static const EnumPropertyItem mode_items[] = {
      {CMP_NODE_COMBSEP_COLOR_RGB, "RGB", ICON_NONE, "RGB", "Use RGB color processing"},
      {CMP_NODE_COMBSEP_COLOR_HSV, "HSV", ICON_NONE, "HSV", "Use HSV color processing"},
      {CMP_NODE_COMBSEP_COLOR_HSL, "HSL", ICON_NONE, "HSL", "Use HSL color processing"},
      {CMP_NODE_COMBSEP_COLOR_YCC, "YCC", ICON_NONE, "YCbCr", "Use YCbCr color processing"},
      {CMP_NODE_COMBSEP_COLOR_YUV, "YUV", ICON_NONE, "YUV", "Use YUV color processing"},
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

static void def_cmp_movieclip(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "MovieClipUser", "storage");
}

static void def_cmp_stabilize2d(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_sampler_type_items);
  RNA_def_property_ui_text(prop, "Filter", "Method to use to filter stabilization");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom2", CMP_NODE_STABILIZE_FLAG_INVERSE);
  RNA_def_property_ui_text(
      prop, "Invert", "Invert stabilization to re-introduce motion to the frame");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_moviedistortion(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem distortion_type_items[] = {
      {0, "UNDISTORT", 0, "Undistort", ""},
      {1, "DISTORT", 0, "Distort", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "distortion_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, distortion_type_items);
  RNA_def_property_ui_text(prop, "Distortion", "Distortion to use to filter image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_mask(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem aspect_type_items[] = {
      {0, "SCENE", 0, "Scene Size", ""},
      {CMP_NODE_MASK_FLAG_SIZE_FIXED, "FIXED", 0, "Fixed", "Use pixel size for the buffer"},
      {CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE,
       "FIXED_SCENE",
       0,
       "Fixed/Scene",
       "Pixel size scaled by scene percentage"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "mask", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Mask");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Mask", "");

  prop = RNA_def_property(srna, "use_feather", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "custom1", CMP_NODE_MASK_FLAG_NO_FEATHER);
  RNA_def_property_ui_text(prop, "Feather", "Use feather information from the mask");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_motion_blur", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", CMP_NODE_MASK_FLAG_MOTION_BLUR);
  RNA_def_property_ui_text(prop, "Motion Blur", "Use multi-sampled motion blur of the mask");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "motion_blur_samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom2");
  RNA_def_property_range(prop, 1, CMP_NODE_MASK_MBLUR_SAMPLES_MAX);
  RNA_def_property_ui_text(prop, "Samples", "Number of motion blur samples");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "motion_blur_shutter", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom3");
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(prop, "Shutter", "Exposure for motion blur as a factor of FPS");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "size_source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, aspect_type_items);
  RNA_def_property_ui_text(
      prop, "Size Source", "Where to get the mask size from for aspect/size information");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeMask", "storage");

  prop = RNA_def_property(srna, "size_x", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "X", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "size_y", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Y", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void dev_cmd_transform(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_sampler_type_items);
  RNA_def_property_ui_text(prop, "Filter", "Method to use to filter transform");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* -- Compositor Nodes ------------------------------------------------------ */

static const EnumPropertyItem node_masktype_items[] = {
    {0, "ADD", 0, "Add", ""},
    {1, "SUBTRACT", 0, "Subtract", ""},
    {2, "MULTIPLY", 0, "Multiply", ""},
    {3, "NOT", 0, "Not", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void def_cmp_boxmask(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mask_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_masktype_items);
  RNA_def_property_ui_text(prop, "Mask Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeBoxMask", "storage");

  prop = RNA_def_property(srna, "x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, -1.0f, 2.0f);
  RNA_def_property_ui_text(prop, "X", "X position of the middle of the box");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "y");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, -1.0f, 2.0f);
  RNA_def_property_ui_text(prop, "Y", "Y position of the middle of the box");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "mask_width", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "width");
  RNA_def_property_float_default(prop, 0.3f);
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_text(prop, "Width", "Width of the box");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "mask_height", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "height");
  RNA_def_property_float_default(prop, 0.2f);
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_text(prop, "Height", "Height of the box");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "rotation");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, DEG2RADF(-1800.0f), DEG2RADF(1800.0f));
  RNA_def_property_ui_text(prop, "Rotation", "Rotation angle of the box");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_ellipsemask(StructRNA *srna)
{
  PropertyRNA *prop;
  prop = RNA_def_property(srna, "mask_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, node_masktype_items);
  RNA_def_property_ui_text(prop, "Mask Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeEllipseMask", "storage");

  prop = RNA_def_property(srna, "x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, -1.0f, 2.0f);
  RNA_def_property_ui_text(prop, "X", "X position of the middle of the ellipse");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "y");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, -1.0f, 2.0f);
  RNA_def_property_ui_text(prop, "Y", "Y position of the middle of the ellipse");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "mask_width", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "width");
  RNA_def_property_float_default(prop, 0.3f);
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_text(prop, "Width", "Width of the ellipse");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "mask_height", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "height");
  RNA_def_property_float_default(prop, 0.2f);
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_text(prop, "Height", "Height of the ellipse");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "rotation");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, DEG2RADF(-1800.0f), DEG2RADF(1800.0f));
  RNA_def_property_ui_text(prop, "Rotation", "Rotation angle of the ellipse");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_bokehblur(StructRNA *srna)
{
  PropertyRNA *prop;

  /* duplicated in def_cmp_blur */
  prop = RNA_def_property(srna, "use_variable_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", CMP_NODEFLAG_BLUR_VARIABLE_SIZE);
  RNA_def_property_ui_text(
      prop, "Variable Size", "Support variable blur per pixel when using an image for size input");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_extended_bounds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", CMP_NODEFLAG_BLUR_EXTEND_BOUNDS);
  RNA_def_property_ui_text(
      prop, "Extend Bounds", "Extend bounds of the input image to fully fit blurred image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

#  if 0
  prop = RNA_def_property(srna, "f_stop", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom3");
  RNA_def_property_range(prop, 0.0f, 128.0f);
  RNA_def_property_ui_text(
      prop,
      "F-Stop",
      "Amount of focal blur, 128 (infinity) is perfect focus, half the value doubles "
      "the blur radius");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
#  endif

  prop = RNA_def_property(srna, "blur_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom4");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Max Blur", "Blur limit, maximum CoC radius");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_bokehimage(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeBokehImage", "storage");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "angle");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, DEG2RADF(-720.0f), DEG2RADF(720.0f));
  RNA_def_property_ui_text(prop, "Angle", "Angle of the bokeh");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "flaps", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "flaps");
  RNA_def_property_int_default(prop, 5);
  RNA_def_property_range(prop, 3, 24);
  RNA_def_property_ui_text(prop, "Flaps", "Number of flaps");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "rounding", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rounding");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, -0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Rounding", "Level of rounding of the bokeh");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "catadioptric", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "catadioptric");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, -0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Catadioptric", "Level of catadioptric of the bokeh");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "shift", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "lensshift");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Lens Shift", "Shift of the lens components");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_switch(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "check", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 0);
  RNA_def_property_ui_text(prop, "Switch", "Off: first socket, On: second socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_switch_view(StructRNA * /*srna*/) {}

static void def_cmp_colorcorrection(StructRNA *srna)
{
  PropertyRNA *prop;
  prop = RNA_def_property(srna, "red", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 1);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Red", "Red channel active");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "green", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 2);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Green", "Green channel active");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "blue", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "custom1", 4);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Blue", "Blue channel active");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeColorCorrection", "storage");

  prop = RNA_def_property(srna, "midtones_start", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "startmidtones");
  RNA_def_property_float_default(prop, 0.2f);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Midtones Start", "Start of midtones");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "midtones_end", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "endmidtones");
  RNA_def_property_float_default(prop, 0.7f);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Midtones End", "End of midtones");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "master_saturation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "master.saturation");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Master Saturation", "Master saturation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "master_contrast", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "master.contrast");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Master Contrast", "Master contrast");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "master_gamma", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "master.gamma");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Master Gamma", "Master gamma");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "master_gain", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "master.gain");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Master Gain", "Master gain");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "master_lift", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "master.lift");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, -1, 1);
  RNA_def_property_ui_text(prop, "Master Lift", "Master lift");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  //
  prop = RNA_def_property(srna, "shadows_saturation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "shadows.saturation");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Shadows Saturation", "Shadows saturation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "shadows_contrast", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "shadows.contrast");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Shadows Contrast", "Shadows contrast");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "shadows_gamma", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "shadows.gamma");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Shadows Gamma", "Shadows gamma");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "shadows_gain", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "shadows.gain");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Shadows Gain", "Shadows gain");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "shadows_lift", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "shadows.lift");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, -1, 1);
  RNA_def_property_ui_text(prop, "Shadows Lift", "Shadows lift");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
  //
  prop = RNA_def_property(srna, "midtones_saturation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "midtones.saturation");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Midtones Saturation", "Midtones saturation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "midtones_contrast", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "midtones.contrast");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Midtones Contrast", "Midtones contrast");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "midtones_gamma", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "midtones.gamma");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Midtones Gamma", "Midtones gamma");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "midtones_gain", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "midtones.gain");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Midtones Gain", "Midtones gain");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "midtones_lift", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "midtones.lift");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, -1, 1);
  RNA_def_property_ui_text(prop, "Midtones Lift", "Midtones lift");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
  //
  prop = RNA_def_property(srna, "highlights_saturation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "highlights.saturation");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Highlights Saturation", "Highlights saturation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "highlights_contrast", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "highlights.contrast");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Highlights Contrast", "Highlights contrast");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "highlights_gamma", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "highlights.gamma");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Highlights Gamma", "Highlights gamma");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "highlights_gain", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "highlights.gain");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0, 4);
  RNA_def_property_ui_text(prop, "Highlights Gain", "Highlights gain");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "highlights_lift", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "highlights.lift");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, -1, 1);
  RNA_def_property_ui_text(prop, "Highlights Lift", "Highlights lift");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_viewer(StructRNA *srna)
{
  PropertyRNA *prop;
  static const EnumPropertyItem tileorder_items[] = {
      {0, "CENTEROUT", 0, "Center", "Expand from center"},
      {1, "RANDOM", 0, "Random", "Random tiles"},
      {2, "BOTTOMUP", 0, "Bottom Up", "Expand from bottom"},
      {3, "RULE_OF_THIRDS", 0, "Rule of Thirds", "Expand from 9 places"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "tile_order", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, tileorder_items);
  RNA_def_property_ui_text(prop, "Tile Order", "Tile order");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "center_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom3");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "X", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "center_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom4");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Y", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "custom2", CMP_NODE_OUTPUT_IGNORE_ALPHA);
  RNA_def_property_ui_text(
      prop,
      "Use Alpha",
      "Colors are treated alpha premultiplied, or colors output straight (alpha gets set to 1)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_composite(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "custom2", CMP_NODE_OUTPUT_IGNORE_ALPHA);
  RNA_def_property_ui_text(
      prop,
      "Use Alpha",
      "Colors are treated alpha premultiplied, or colors output straight (alpha gets set to 1)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_keyingscreen(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeKeyingScreenData", "storage");

  prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "tracking_object");
  RNA_def_property_ui_text(prop, "Tracking Object", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "smoothness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "smoothness");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Smoothness", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_keying(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeKeyingData", "storage");

  prop = RNA_def_property(srna, "screen_balance", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "screen_balance");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Screen Balance",
      "Balance between two non-primary channels primary channel is comparing against");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "despill_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "despill_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Despill Factor", "Factor of despilling screen color from image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "despill_balance", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "despill_balance");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Despill Balance",
      "Balance between non-key colors used to detect amount of key color to be removed");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "clip_black", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "clip_black");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Clip Black",
      "Value of non-scaled matte pixel which considers as fully background pixel");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "clip_white", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "clip_white");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Clip White",
      "Value of non-scaled matte pixel which considers as fully foreground pixel");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "blur_pre", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "blur_pre");
  RNA_def_property_range(prop, 0, 2048);
  RNA_def_property_ui_text(
      prop, "Pre Blur", "Chroma pre-blur size which applies before running keyer");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "blur_post", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "blur_post");
  RNA_def_property_range(prop, 0, 2048);
  RNA_def_property_ui_text(
      prop, "Post Blur", "Matte blur size which applies after clipping and dilate/eroding");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "dilate_distance", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "dilate_distance");
  RNA_def_property_range(prop, -100, 100);
  RNA_def_property_ui_text(prop, "Dilate/Erode", "Distance to grow/shrink the matte");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "edge_kernel_radius", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "edge_kernel_radius");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(
      prop, "Edge Kernel Radius", "Radius of kernel used to detect whether pixel belongs to edge");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "edge_kernel_tolerance", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "edge_kernel_tolerance");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Edge Kernel Tolerance",
      "Tolerance to pixels inside kernel which are treating as belonging to the same plane");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "feather_falloff", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "feather_falloff");
  RNA_def_property_enum_items(prop, rna_enum_proportional_falloff_curve_only_items);
  RNA_def_property_ui_text(prop, "Feather Falloff", "Falloff type of the feather");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "feather_distance", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "feather_distance");
  RNA_def_property_range(prop, -100, 100);
  RNA_def_property_ui_text(prop, "Feather Distance", "Distance to grow/shrink the feather");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_trackpos(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem position_items[] = {
      {CMP_NODE_TRACK_POSITION_ABSOLUTE,
       "ABSOLUTE",
       0,
       "Absolute",
       "Output absolute position of a marker"},
      {CMP_NODE_TRACK_POSITION_RELATIVE_START,
       "RELATIVE_START",
       0,
       "Relative Start",
       "Output position of a marker relative to first marker of a track"},
      {CMP_NODE_TRACK_POSITION_RELATIVE_FRAME,
       "RELATIVE_FRAME",
       0,
       "Relative Frame",
       "Output position of a marker relative to marker at given frame number"},
      {CMP_NODE_TRACK_POSITION_ABSOLUTE_FRAME,
       "ABSOLUTE_FRAME",
       0,
       "Absolute Frame",
       "Output absolute position of a marker at given frame number"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "position", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, position_items);
  RNA_def_property_ui_text(prop, "Position", "Which marker position to use for output");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "frame_relative", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom2");
  RNA_def_property_ui_text(prop, "Frame", "Frame to be used for relative position");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

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

static void def_cmp_pixelate(StructRNA *srna)
{
  PropertyRNA *prop;

  /* The range of the pixel size is chosen so that it is positive and above zero, and also does not
   * exceed the underlying int16_t type. The size limit matches the maximum size used by blur
   * nodes. */
  prop = RNA_def_property(srna, "pixel_size", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_range(prop, 1, 2048);
  RNA_def_property_int_default(prop, 1);
  RNA_def_property_ui_text(prop, "Pixel Size", "Pixel size of the output image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_translate(StructRNA *srna)
{
  static const EnumPropertyItem translate_items[] = {
      {CMP_NODE_WRAP_NONE, "NONE", 0, "None", "No wrapping on X and Y"},
      {CMP_NODE_WRAP_X, "XAXIS", 0, "X Axis", "Wrap all pixels on the X axis"},
      {CMP_NODE_WRAP_Y, "YAXIS", 0, "Y Axis", "Wrap all pixels on the Y axis"},
      {CMP_NODE_WRAP_XY, "BOTH", 0, "Both Axes", "Wrap all pixels on both axes"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeTranslateData", "storage");

  prop = RNA_def_property(srna, "use_relative", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "relative", 1);
  RNA_def_property_ui_text(
      prop,
      "Relative",
      "Use relative (fraction of input image size) values to define translation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "wrap_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "wrap_axis");
  RNA_def_property_enum_items(prop, translate_items);
  RNA_def_property_ui_text(prop, "Wrapping", "Wrap image on a specific axis");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_planetrackdeform(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodePlaneTrackDeformData", "storage");

  prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "tracking_object");
  RNA_def_property_ui_text(prop, "Tracking Object", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "plane_track_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "plane_track_name");
  RNA_def_property_ui_text(prop, "Plane Track", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_motion_blur", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "flag", CMP_NODE_PLANE_TRACK_DEFORM_FLAG_MOTION_BLUR);
  RNA_def_property_ui_text(prop, "Motion Blur", "Use multi-sampled motion blur of the mask");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "motion_blur_samples", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, CMP_NODE_PLANE_TRACK_DEFORM_MOTION_BLUR_SAMPLES_MAX);
  RNA_def_property_ui_text(prop, "Samples", "Number of motion blur samples");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "motion_blur_shutter", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(prop, "Shutter", "Exposure for motion blur as a factor of FPS");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_sunbeams(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeSunBeams", "storage");

  prop = RNA_def_property(srna, "source", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "source");
  RNA_def_property_range(prop, -100.0f, 100.0f);
  RNA_def_property_ui_range(prop, -10.0f, 10.0f, 10, 3);
  RNA_def_property_ui_text(
      prop, "Source", "Source point of rays as a factor of the image width and height");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "ray_length", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "ray_length");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Ray Length", "Length of rays as a factor of the image size");
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

static void def_cmp_cryptomatte_common(StructRNA *srna)
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
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeCryptomatte_update_add");

  prop = RNA_def_property(srna, "remove", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "runtime.remove");
  RNA_def_property_float_array_default(prop, default_1);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_text(
      prop,
      "Remove",
      "Remove object or material from matte, by picking a color from the Pick output");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeCryptomatte_update_remove");
}

static void def_cmp_cryptomatte_legacy(StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeCryptomatte", "storage");
  def_cmp_cryptomatte_common(srna);
}

static void def_cmp_cryptomatte(StructRNA *srna)
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

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(
      prop, "rna_NodeCryptomatte_scene_get", "rna_NodeCryptomatte_scene_set", nullptr, nullptr);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Scene", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

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
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeCryptomatte", "storage");
  def_cmp_cryptomatte_common(srna);

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
  def_node_image_user(srna);
}

static void def_cmp_denoise(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem prefilter_items[] = {
      {CMP_NODE_DENOISE_PREFILTER_NONE,
       "NONE",
       0,
       "None",
       "No prefiltering, use when guiding passes are noise-free"},
      {CMP_NODE_DENOISE_PREFILTER_FAST,
       "FAST",
       0,
       "Fast",
       "Denoise image and guiding passes together. Improves quality when guiding passes are noisy "
       "using least amount of extra processing time"},
      {CMP_NODE_DENOISE_PREFILTER_ACCURATE,
       "ACCURATE",
       0,
       "Accurate",
       "Prefilter noisy guiding passes before denoising image. Improves quality when guiding "
       "passes are noisy using extra processing time"},
      {0, nullptr, 0, nullptr, nullptr}};

  RNA_def_struct_sdna_from(srna, "NodeDenoise", "storage");

  prop = RNA_def_property(srna, "use_hdr", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "hdr", 0);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "HDR", "Process HDR images");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "prefilter", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prefilter_items);
  RNA_def_property_enum_default(prop, CMP_NODE_DENOISE_PREFILTER_ACCURATE);
  RNA_def_property_ui_text(prop, "", "Denoising prefilter");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_kuwahara(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeKuwaharaData", "storage");

  static const EnumPropertyItem variation_items[] = {
      {0, "CLASSIC", 0, "Classic", "Fast but less accurate variation"},
      {1, "ANISOTROPIC", 0, "Anisotropic", "Accurate but slower variation"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "variation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "variation");
  RNA_def_property_enum_items(prop, variation_items);
  RNA_def_property_ui_text(prop, "", "Variation of Kuwahara filter to use");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_high_precision", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "high_precision", 1);
  RNA_def_property_ui_text(
      prop,
      "High Precision",
      "Uses a more precise but slower method. Use if the output contains undesirable noise");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "uniformity", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "uniformity");
  RNA_def_property_range(prop, 0.0, 50.0);
  RNA_def_property_ui_range(prop, 0, 50, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Uniformity",
                           "Controls the uniformity of the direction of the filter. Higher values "
                           "produces more uniform directions");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "sharpness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "sharpness");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_ui_text(prop,
                           "Sharpness",
                           "Controls the sharpness of the filter. 0 means completely smooth while "
                           "1 means completely sharp");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "eccentricity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "eccentricity");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 0.1, 3);
  RNA_def_property_ui_text(
      prop,
      "Eccentricity",
      "Controls how directional the filter is. 0 means the filter is completely omnidirectional "
      "while 2 means it is maximally directed along the edges of the image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_antialiasing(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeAntiAliasingData", "storage");

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "threshold");
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_ui_text(
      prop,
      "Threshold",
      "Threshold to detect edges (smaller threshold makes more sensitive detection)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "contrast_limit", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "contrast_limit");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_ui_text(
      prop,
      "Contrast Limit",
      "How much to eliminate spurious edges to avoid artifacts (the larger value makes less "
      "active; the value 2.0, for example, means discard a detected edge if there is a "
      "neighboring edge that has 2.0 times bigger contrast than the current one)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "corner_rounding", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "corner_rounding");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Corner Rounding", "How much sharp corners will be rounded");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* -- Texture Nodes --------------------------------------------------------- */

static void def_tex_output(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "TexNodeOutput", "storage");

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Output Name", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_tex_image(StructRNA *srna)
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

static void def_tex_bricks(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom3");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Offset Amount", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "offset_frequency", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_range(prop, 2, 99);
  RNA_def_property_ui_text(prop, "Offset Frequency", "Offset every N rows");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "squash", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom4");
  RNA_def_property_range(prop, 0.0f, 99.0f);
  RNA_def_property_ui_text(prop, "Squash Amount", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "squash_frequency", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom2");
  RNA_def_property_range(prop, 2, 99);
  RNA_def_property_ui_text(prop, "Squash Frequency", "Squash every N rows");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* -- Geometry Nodes --------------------------------------------------------- */

static void def_geo_curve_sample(StructRNA *srna)
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

static void def_fn_random_value(StructRNA *srna)
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

static void def_geo_distribute_points_on_faces(StructRNA *srna)
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

static void def_geo_curve_set_handle_type(StructRNA *srna)
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
  RNA_def_property_enum_items(prop, rna_enum_node_geometry_curve_handle_side_items);
  RNA_def_property_ui_text(prop, "Mode", "Whether to update left and right handles");
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void def_common_zone_input(StructRNA *srna)
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
      func, "output_node", "GeometryNode", "Output Node", "Zone output node to pair with");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return value */
  parm = RNA_def_boolean(
      func, "result", false, "Result", "True if pairing the node was successful");
  RNA_def_function_return(func, parm);
}

static void def_geo_simulation_input(StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeGeometrySimulationInput", "storage");

  def_common_zone_input(srna);
}

static void def_geo_repeat_input(StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeGeometryRepeatInput", "storage");

  def_common_zone_input(srna);
}

static void rna_def_node_item_array_socket_item_common(StructRNA *srna, const char *accessor)
{
  static blender::LinearAllocator<> allocator;
  PropertyRNA *prop;

  char name_set_func[64];
  SNPRINTF(name_set_func, "rna_Node_ItemArray_item_name_set<%s>", accessor);

  char item_update_func[64];
  SNPRINTF(item_update_func, "rna_Node_ItemArray_item_update<%s>", accessor);
  const char *item_update_func_ptr = allocator.copy_string(item_update_func).c_str();

  char socket_type_itemf[64];
  SNPRINTF(socket_type_itemf, "rna_Node_ItemArray_socket_type_itemf<%s>", accessor);

  char color_get_func[64];
  SNPRINTF(color_get_func, "rna_Node_ItemArray_item_color_get<%s>", accessor);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, nullptr, nullptr, allocator.copy_string(name_set_func).c_str());
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, item_update_func_ptr);

  prop = RNA_def_property(srna, "socket_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_data_type_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, allocator.copy_string(socket_type_itemf).c_str());
  RNA_def_property_ui_text(prop, "Socket Type", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, item_update_func_ptr);

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

  char remove_call[64];
  SNPRINTF(remove_call, "rna_Node_ItemArray_remove<%s>", accessor_name);
  char clear_call[64];
  SNPRINTF(clear_call, "rna_Node_ItemArray_clear<%s>", accessor_name);
  char move_call[64];
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

static void rna_def_simulation_state_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "SimulationStateItem", nullptr);
  RNA_def_struct_ui_text(srna, "Simulation Item", "");
  RNA_def_struct_sdna(srna, "NodeSimulationItem");

  rna_def_node_item_array_socket_item_common(srna, "SimulationItemsAccessor");

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

static void rna_def_geo_simulation_output_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeGeometrySimulationOutputItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of simulation items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "SimulationStateItem", "SimulationItemsAccessor");
  rna_def_node_item_array_common_functions(srna, "SimulationStateItem", "SimulationItemsAccessor");
}

static void def_geo_simulation_output(StructRNA *srna)
{
  PropertyRNA *prop;

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

static void rna_def_repeat_item(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "RepeatItem", nullptr);
  RNA_def_struct_ui_text(srna, "Repeat Item", "");
  RNA_def_struct_sdna(srna, "NodeRepeatItem");

  rna_def_node_item_array_socket_item_common(srna, "RepeatItemsAccessor");
}

static void rna_def_geo_repeat_output_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeGeometryRepeatOutputItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of repeat items");

  rna_def_node_item_array_new_with_socket_and_name(srna, "RepeatItem", "RepeatItemsAccessor");
  rna_def_node_item_array_common_functions(srna, "RepeatItem", "RepeatItemsAccessor");
}

static void def_geo_repeat_output(StructRNA *srna)
{
  PropertyRNA *prop;

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

static void rna_def_geo_bake_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "NodeGeometryBakeItem", nullptr);
  RNA_def_struct_ui_text(srna, "Bake Item", "");

  rna_def_node_item_array_socket_item_common(srna, "BakeItemsAccessor");

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

static void rna_def_bake_items(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "NodeGeometryBakeItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of bake items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeGeometryBakeItem", "BakeItemsAccessor");
  rna_def_node_item_array_common_functions(srna, "NodeGeometryBakeItem", "BakeItemsAccessor");
}

static void rna_def_geo_bake(StructRNA *srna)
{
  PropertyRNA *prop;

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

static void def_geo_index_switch(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeIndexSwitch", "storage");

  prop = RNA_def_property(srna, "index_switch_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_num");
  RNA_def_property_struct_type(prop, "IndexSwitchItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeIndexSwitchItems");
}

static void def_geo_curve_handle_type_selection(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeGeometryCurveSelectHandles", "storage");

  prop = RNA_def_property(srna, "handle_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "handle_type");
  RNA_def_property_ui_text(prop, "Handle Type", "");
  RNA_def_property_enum_items(prop, rna_node_geometry_curve_handle_type_items);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_geometry_curve_handle_side_items);
  RNA_def_property_ui_text(prop, "Mode", "Whether to check the type of left and right handles");
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void def_fn_rotate_euler(StructRNA *srna)
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

static void def_geo_sample_index(StructRNA *srna)
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

static void def_geo_input_material(StructRNA *srna)
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

static void def_geo_image(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_geo_string_to_curves(StructRNA *srna)
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
       "\"Remainder\" output"},
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
  RNA_def_property_ui_text(prop, "Font", "Font of the text. Falls back to the UI font by default");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  RNA_def_struct_sdna_from(srna, "NodeGeometryStringToCurves", "storage");

  prop = RNA_def_property(srna, "overflow", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "overflow");
  RNA_def_property_enum_items(prop, rna_node_geometry_string_to_curves_overflow_items);
  RNA_def_property_enum_default(prop, GEO_NODE_STRING_TO_CURVES_MODE_OVERFLOW);
  RNA_def_property_ui_text(
      prop, "Textbox Overflow", "Handle the text behavior when it doesn't fit in the text boxes");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "align_x", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "align_x");
  RNA_def_property_enum_items(prop, rna_node_geometry_string_to_curves_align_x_items);
  RNA_def_property_enum_default(prop, GEO_NODE_STRING_TO_CURVES_ALIGN_X_LEFT);
  RNA_def_property_ui_text(
      prop, "Horizontal Alignment", "Text horizontal alignment from the object center");
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

static void rna_def_node_enum_item(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "NodeEnumItem", nullptr);
  RNA_def_struct_ui_text(srna, "Enum Item", "");
  RNA_def_struct_sdna(srna, "NodeEnumItem");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_NodeEnumItem_name_set");
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeEnumItem_update");

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "description");
  RNA_def_property_ui_text(prop, "Description", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeEnumItem_update");
}

static void rna_def_node_enum_definition_items(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "NodeEnumDefinitionItems", nullptr);
  RNA_def_struct_sdna(srna, "NodeEnumDefinition");
  RNA_def_struct_ui_text(
      srna, "Enum Definition Items", "Collection of items that make up an enum");

  func = RNA_def_function(srna, "new", "rna_NodeEnumDefinition_new");
  RNA_def_function_ui_description(func, "Add an a new enum item");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "name", nullptr, MAX_NAME, "Name", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return value */
  parm = RNA_def_pointer(func, "item", "NodeEnumItem", "Item", "New item");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_NodeEnumDefinition_remove");
  RNA_def_function_ui_description(func, "Remove an item from this enum");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "item", "NodeEnumItem", "Item", "The item to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "clear", "rna_NodeEnumDefinition_clear");
  RNA_def_function_ui_description(func, "Remove all items from this enum");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);

  func = RNA_def_function(srna, "move", "rna_NodeEnumDefinition_move");
  RNA_def_function_ui_description(func, "Move an item to another position");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_int(
      func, "from_index", -1, 0, INT_MAX, "From Index", "Index of the item to move", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(
      func, "to_index", -1, 0, INT_MAX, "To Index", "Target index for the item", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_node_enum_definition(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeEnumDefinition", nullptr);
  RNA_def_struct_sdna(srna, "NodeEnumDefinition");
  RNA_def_struct_ui_text(srna, "Enum Definition", "Definition of an enumeration for nodes");

  prop = RNA_def_property(srna, "enum_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items_array", "items_num");
  RNA_def_property_struct_type(prop, "NodeEnumItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeEnumDefinitionItems");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "NodeEnumItem");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_NodeEnumDefinition_active_item_get",
                                 "rna_NodeEnumDefinition_active_item_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Item", "Active item");
  RNA_def_property_update(prop, NC_NODE, nullptr);
}

static void def_geo_menu_switch(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeMenuSwitch", "storage");

  prop = RNA_def_property(srna, "enum_definition", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "enum_definition");
  RNA_def_property_struct_type(prop, "NodeEnumDefinition");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Enum Definition", "Definition of enum items");
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
      func, "use_multi_input", false, "", "Make the socket a multi-input. Only valid for inputs");
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

  static const EnumPropertyItem dummy_static_type_items[] = {
      {NODE_CUSTOM, "CUSTOM", 0, "Custom", "Custom Node"},
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

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, dummy_static_type_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_node_static_type_itemf");
  RNA_def_property_enum_default(prop, NODE_CUSTOM);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Type",
      "Node type (deprecated, use bl_static_type or bl_idname for the actual identifier string)");

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "locx");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, -100000.0f, 100000.0f);
  RNA_def_property_ui_text(prop, "Location", "");
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
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_HIDDEN);
  RNA_def_property_ui_text(prop, "Hide", "");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_MUTED);
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
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "ID Name", "");

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->ui_name");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Label", "The node label");

  prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_TRANSLATION);
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->ui_description");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "typeinfo->ui_icon");
  RNA_def_property_enum_items(prop, rna_enum_icon_items);
  RNA_def_property_enum_default(prop, ICON_NODE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Icon", "The node icon");

  prop = RNA_def_property(srna, "bl_static_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "typeinfo->type");
  RNA_def_property_enum_items(prop, dummy_static_type_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_node_static_type_itemf");
  RNA_def_property_enum_default(prop, NODE_CUSTOM);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Static Type", "Node type (deprecated, use with care)");

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
      "Used to sort multiple links coming into the same input. The highest ID is at the top");

  func = RNA_def_function(
      srna, "swap_multi_input_sort_id", "rna_NodeLink_swap_multi_input_sort_id");
  RNA_def_function_ui_description(
      func, "Swap the order of two links connected to the same multi-input socket");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func,
                         "other",
                         "NodeLink",
                         "Other",
                         "The other link. Must link to the same multi input socket");
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

  prop = RNA_def_property(srna, "view_center", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_sdna(prop, nullptr, "view_center");
  RNA_def_property_ui_text(
      prop, "", "The current location (offset) of the view for this Node Tree");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

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

  /* Grease Pencil */
  prop = RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gpd");
  RNA_def_property_struct_type(prop, "GreasePencil");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_GPencil_datablocks_annotations_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Grease Pencil Data", "Grease Pencil data-block");
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
      "Check if the node tree contains another. Used to avoid creating recursive node groups");
  parm = RNA_def_pointer(
      func, "sub_tree", "NodeTree", "Node Tree", "Node tree for recursive check");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "contained", PROP_BOOLEAN, PROP_NONE);
  RNA_def_function_return(func, parm);

  /* registration */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "ID Name", "");

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->ui_name");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Label", "The node tree label");

  prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_TRANSLATION);
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->ui_description");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "typeinfo->ui_icon");
  RNA_def_property_enum_items(prop, rna_enum_icon_items);
  RNA_def_property_enum_default(prop, ICON_NODETREE);
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Icon", "The node tree icon");

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

  prop = RNA_def_property(srna, "precision", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "precision");
  RNA_def_property_enum_items(prop, rna_enum_precision_items);
  RNA_def_property_ui_text(prop, "Precision", "The precision of compositor intermediate result");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update");

  prop = RNA_def_property(srna, "execution_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "execution_mode");
  RNA_def_property_enum_items(prop, rna_enum_execution_mode_items);
  RNA_def_property_ui_text(prop, "Execution Mode", "Set how compositing is executed");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update");

  prop = RNA_def_property(srna, "render_quality", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "render_quality");
  RNA_def_property_enum_items(prop, node_quality_items);
  RNA_def_property_ui_text(prop, "Render Quality", "Quality when rendering");

  prop = RNA_def_property(srna, "edit_quality", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "edit_quality");
  RNA_def_property_enum_items(prop, node_quality_items);
  RNA_def_property_ui_text(prop, "Edit Quality", "Quality when editing");

  prop = RNA_def_property(srna, "use_two_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NTREE_TWO_PASS);
  RNA_def_property_ui_text(prop,
                           "Two Pass",
                           "Use two pass execution during editing: first calculate fast nodes, "
                           "second pass calculate all nodes");

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
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_ASSET_TOOL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Tool", "The node group is used as a tool");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_tool_get", "rna_GeometryNodeTree_is_tool_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_modifier", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_ASSET_MODIFIER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Modifier", "The node group is used as a geometry modifier");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_modifier_get", "rna_GeometryNodeTree_is_modifier_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_mode_object", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_ASSET_EDIT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Edit", "The node group is used in object mode");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_mode_object_get", "rna_GeometryNodeTree_is_mode_object_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_mode_edit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_ASSET_EDIT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Edit", "The node group is used in edit mode");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_mode_edit_get", "rna_GeometryNodeTree_is_mode_edit_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_mode_sculpt", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_ASSET_SCULPT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Sculpt", "The node group is used in sculpt mode");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_mode_sculpt_get", "rna_GeometryNodeTree_is_mode_sculpt_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_type_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_ASSET_MESH);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Mesh", "The node group is used for meshes");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_type_mesh_get", "rna_GeometryNodeTree_is_type_mesh_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_type_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_ASSET_CURVE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Curves", "The node group is used for curves");
  RNA_def_property_boolean_funcs(
      prop, "rna_GeometryNodeTree_is_type_curve_get", "rna_GeometryNodeTree_is_type_curve_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");

  prop = RNA_def_property(srna, "is_type_point_cloud", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GEO_NODE_ASSET_POINT_CLOUD);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Point Cloud", "The node group is used for point clouds");
  RNA_def_property_boolean_funcs(prop,
                                 "rna_GeometryNodeTree_is_type_point_cloud_get",
                                 "rna_GeometryNodeTree_is_type_point_cloud_set");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update_asset");
}

static StructRNA *define_specific_node(BlenderRNA *brna,
                                       const char *struct_name,
                                       const char *base_name,
                                       void (*def_func)(StructRNA *))
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  /* XXX hack, want to avoid "NodeInternal" prefix,
   * so use "Node" in NOD_static_types.h and replace here */
  if (STREQ(base_name, "Node")) {
    base_name = "NodeInternal";
  }

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

  if (def_func) {
    def_func(srna);
  }

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

void RNA_def_nodetree(BlenderRNA *brna)
{
  StructRNA *srna;

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

  rna_def_simulation_state_item(brna);
  rna_def_repeat_item(brna);
  rna_def_index_switch_item(brna);
  rna_def_geo_bake_item(brna);

#  define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
    { \
      srna = define_specific_node(brna, #Category #StructName, #Category, DefFunc); \
      if (ID == CMP_NODE_OUTPUT_FILE) { \
        /* needs brna argument, can't use NOD_static_types.h */ \
        def_cmp_output_file(brna, srna); \
      } \
    }

  /* hack, don't want to add include path to RNA just for this, since in the future RNA types
   * for nodes should be defined locally at runtime anyway ...
   */
#  include "../../nodes/NOD_static_types.h"

  /* Node group types need to be defined for shader, compositor, texture, geometry nodes
   * individually. Cannot use the static types header for this, since they share the same int id.
   */
  define_specific_node(brna, "ShaderNodeGroup", "ShaderNode", def_group);
  define_specific_node(brna, "CompositorNodeGroup", "CompositorNode", def_group);
  define_specific_node(brna, "TextureNodeGroup", "TextureNode", def_group);
  define_specific_node(brna, "GeometryNodeGroup", "GeometryNode", def_group);
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

  /* special socket types */
  rna_def_cmp_output_file_slot_file(brna);
  rna_def_cmp_output_file_slot_layer(brna);
  rna_def_geo_simulation_output_items(brna);
  rna_def_geo_repeat_output_items(brna);
  rna_def_geo_index_switch_items(brna);
  rna_def_bake_items(brna);
  rna_def_node_enum_item(brna);
  rna_def_node_enum_definition_items(brna);
  rna_def_node_enum_definition(brna);

  rna_def_node_instance_hash(brna);
}

/* clean up macro definition */
#  undef NODE_DEFINE_SUBTYPES

#endif
