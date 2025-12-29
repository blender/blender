/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_node_tree_interface_types.h"
#include "DNA_scene_types.h" /* for #ImageFormatData */
#include "DNA_texture_types.h"
#include "DNA_vec_types.h" /* for #rctf */

#include "BLI_enum_flags.hh"

/** Workaround to forward-declare C++ type in C header. */
#ifdef __cplusplus
#  include "BLI_vector.hh"

namespace blender {
template<typename T> class Span;
template<typename T> class MutableSpan;
class IndexRange;
class StringRef;
class StringRefNull;
}  // namespace blender
namespace blender::nodes {
class NodeDeclaration;
class SocketDeclaration;
}  // namespace blender::nodes
namespace blender::bke {
class bNodeTreeRuntime;
class bNodeRuntime;
class bNodeSocketRuntime;
}  // namespace blender::bke
namespace blender::bke {
class bNodeTreeZones;
class bNodeTreeZone;
struct bNodeTreeType;
struct bNodeType;
struct bNodeSocketType;
}  // namespace blender::bke
namespace blender::bke {
struct RuntimeNodeEnumItems;
}  // namespace blender::bke
using bNodeTreeRuntimeHandle = blender::bke::bNodeTreeRuntime;

using bNodeRuntimeHandle = blender::bke::bNodeRuntime;
using bNodeSocketRuntimeHandle = blender::bke::bNodeSocketRuntime;
using RuntimeNodeEnumItemsHandle = blender::bke::RuntimeNodeEnumItems;
using bNodeTreeTypeHandle = blender::bke::bNodeTreeType;
using bNodeTypeHandle = blender::bke::bNodeType;
using bNodeSocketTypeHandle = blender::bke::bNodeSocketType;
#else

struct bNodeTreeRuntimeHandle;
struct bNodeRuntimeHandle;
struct bNodeSocketRuntimeHandle;
struct RuntimeNodeEnumItemsHandle;
struct NodeInstanceHashHandle;
struct bNodeTreeTypeHandle;
struct bNodeTypeHandle;
struct bNodeSocketTypeHandle;
#endif

struct AnimData;
struct Collection;
struct GeometryNodeAssetTraits;
struct ID;
struct Image;
struct ImBuf;
struct Material;
struct PreviewImage;
struct Tex;
struct bGPdata;
struct bNodeLink;
struct bNode;
struct NodeEnumDefinition;

#define NODE_MAXSTR 64

/** #bNodeStack.datatype (shade-tree only). */
enum {
  NS_OSA_VECTORS = 1,
  NS_OSA_VALUES = 2,
};

/* node socket/node socket type -b conversion rules */
enum {
  NS_CR_CENTER = 0,
  NS_CR_NONE = 1,
  NS_CR_FIT_WIDTH = 2,
  NS_CR_FIT_HEIGHT = 3,
  NS_CR_FIT = 4,
  NS_CR_STRETCH = 5,
};

/** #bNodeSocket.type & #bNodeSocketType.type */
enum eNodeSocketDatatype {
  SOCK_CUSTOM = -1, /* socket has no integer type */
  SOCK_FLOAT = 0,
  SOCK_VECTOR = 1,
  SOCK_RGBA = 2,
  SOCK_SHADER = 3,
  SOCK_BOOLEAN = 4,
  SOCK_INT = 6,
  SOCK_STRING = 7,
  SOCK_OBJECT = 8,
  SOCK_IMAGE = 9,
  SOCK_GEOMETRY = 10,
  SOCK_COLLECTION = 11,
  SOCK_TEXTURE = 12,
  SOCK_MATERIAL = 13,
  SOCK_ROTATION = 14,
  SOCK_MENU = 15,
  SOCK_MATRIX = 16,
  SOCK_BUNDLE = 17,
  SOCK_CLOSURE = 18,
  SOCK_FONT = 19,
  SOCK_SCENE = 20,
  /** Has _ID suffix to avoid using it instead of SOCK_STRING accidentally. */
  SOCK_TEXT_ID = 21,
  SOCK_MASK = 22,
  SOCK_SOUND = 23,
};

/** Socket shape. */
enum eNodeSocketDisplayShape {
  SOCK_DISPLAY_SHAPE_CIRCLE = 0,
  SOCK_DISPLAY_SHAPE_SQUARE = 1,
  SOCK_DISPLAY_SHAPE_DIAMOND = 2,
  SOCK_DISPLAY_SHAPE_CIRCLE_DOT = 3,
  SOCK_DISPLAY_SHAPE_SQUARE_DOT = 4,
  SOCK_DISPLAY_SHAPE_DIAMOND_DOT = 5,
  SOCK_DISPLAY_SHAPE_LINE = 6,
  SOCK_DISPLAY_SHAPE_VOLUME_GRID = 7,
  SOCK_DISPLAY_SHAPE_LIST = 8,
};

/** Socket side (input/output). */
enum eNodeSocketInOut {
  SOCK_IN = 1 << 0,
  SOCK_OUT = 1 << 1,
};
ENUM_OPERATORS(eNodeSocketInOut);

/** #bNodeSocket.flag, first bit is selection. */
enum eNodeSocketFlag {
  /** Hidden is user defined, to hide unused sockets. */
  SOCK_HIDDEN = (1 << 1),
  /** For quick check if socket is linked. */
  SOCK_IS_LINKED = (1 << 2),
  /** Unavailable is for dynamic sockets. */
  SOCK_UNAVAIL = (1 << 3),
  SOCK_GIZMO_PIN = (1 << 4),
  // /** DEPRECATED  group socket should not be exposed */
  // SOCK_INTERNAL = (1 << 5),
  /** Socket collapsed in UI. */
  SOCK_COLLAPSED = (1 << 6),
  /** Hide socket value, if it gets auto default. */
  SOCK_HIDE_VALUE = (1 << 7),
  /** Socket hidden automatically, to distinguish from manually hidden. */
  SOCK_AUTO_HIDDEN__DEPRECATED = (1 << 8),
  /** Not used anymore but may still be set in files. */
  SOCK_NO_INTERNAL_LINK_LEGACY = (1 << 9),
  /** Not used anymore but may still be set in files. */
  SOCK_COMPACT_LEGACY = (1 << 10),
  /** Make the input socket accept multiple incoming links in the UI. */
  SOCK_MULTI_INPUT = (1 << 11),
  /** Not used anymore but may still be set in files. */
  SOCK_HIDE_LABEL_LEGACY = (1 << 12),
  /**
   * Only used for geometry nodes. Don't show the socket value in the modifier interface.
   */
  SOCK_HIDE_IN_MODIFIER = (1 << 13),
  /** The panel containing the socket is collapsed. */
  SOCK_PANEL_COLLAPSED = (1 << 14),
};

enum eNodePanelFlag {
  /* Panel is collapsed (user setting). */
  NODE_PANEL_COLLAPSED = (1 << 0),
  /* The parent panel is collapsed. */
  NODE_PANEL_PARENT_COLLAPSED = (1 << 1),
  /* The panel has visible content. */
  NODE_PANEL_CONTENT_VISIBLE = (1 << 2),
};

enum eViewerNodeShortcut {
  NODE_VIEWER_SHORTCUT_NONE = 0,
  /* Users can set custom keys to shortcuts,
   * but shortcuts should always be referred to as enums. */
  NODE_VIEWER_SHORCTUT_SLOT_1 = 1,
  NODE_VIEWER_SHORCTUT_SLOT_2 = 2,
  NODE_VIEWER_SHORCTUT_SLOT_3 = 3,
  NODE_VIEWER_SHORCTUT_SLOT_4 = 4,
  NODE_VIEWER_SHORCTUT_SLOT_5 = 5,
  NODE_VIEWER_SHORCTUT_SLOT_6 = 6,
  NODE_VIEWER_SHORCTUT_SLOT_7 = 7,
  NODE_VIEWER_SHORCTUT_SLOT_8 = 8,
  NODE_VIEWER_SHORCTUT_SLOT_9 = 9
};

enum NodeWarningPropagation {
  NODE_WARNING_PROPAGATION_ALL = 0,
  NODE_WARNING_PROPAGATION_NONE = 1,
  NODE_WARNING_PROPAGATION_ONLY_ERRORS = 2,
  NODE_WARNING_PROPAGATION_ONLY_ERRORS_AND_WARNINGS = 3,
};

/** #bNode::flag */
enum {
  NODE_SELECT = 1 << 0,
  NODE_OPTIONS = 1 << 1,
  NODE_PREVIEW = 1 << 2,
  NODE_COLLAPSED = 1 << 3,
  NODE_ACTIVE = 1 << 4,
  // NODE_ACTIVE_ID = 1 << 5, /* Deprecated. */
  /** Used to indicate which group output node is used and which viewer node is active. */
  NODE_DO_OUTPUT = 1 << 6,
  // NODE_GROUP_EDIT = 1 << 7, /* Deprecated, dirty. */
  NODE_TEST = 1 << 8,
  /** Node is disabled. */
  NODE_MUTED = 1 << 9,
  // NODE_CUSTOM_NAME = 1 << 10, /* Deprecated, dirty. */
  // NODE_CONST_OUTPUT = 1 << 11, /* Deprecated, dirty. */
  /** Node is always behind others. */
  NODE_BACKGROUND = 1 << 12,
  /** Automatic flag for nodes included in transforms */
  // NODE_TRANSFORM = 1 << 13, /* Deprecated, dirty. */

  /**
   * Node is active texture.
   *
   * NOTE(@ideasman42): take care with this flag since its possible it gets `stuck`
   * inside/outside the active group - which makes buttons window texture not update,
   * we try to avoid it by clearing the flag when toggling group editing.
   */
  NODE_ACTIVE_TEXTURE = 1 << 14,
  /** Use a custom color for the node. */
  NODE_CUSTOM_COLOR = 1 << 15,
  /**
   * Node has been initialized
   * This flag indicates the `node->typeinfo->init` function has been called.
   * In case of undefined type at creation time this can be delayed until
   * until the node type is registered.
   */
  NODE_INIT = 1 << 16,
  /** A preview for the data in this node can be displayed in the spreadsheet editor. */
  // NODE_ACTIVE_PREVIEW = 1 << 18, /* deprecated */
  /** Active node that is used to paint on. */
  NODE_ACTIVE_PAINT_CANVAS = 1 << 19,
};

/** bNode::update */
enum {
  /** Associated id data block has changed. */
  NODE_UPDATE_ID = 1,
};

/** #bNodeLink::flag */
enum {
  /** Node should be inserted on this link on drop. */
  NODE_LINK_INSERT_TARGET = 1 << 0,
  /** Link has been successfully validated. */
  NODE_LINK_VALID = 1 << 1,
  /** Free test flag, undefined. */
  NODE_LINK_TEST = 1 << 2,
  /** Link is highlighted for picking. */
  NODE_LINK_TEMP_HIGHLIGHT = 1 << 3,
  /** Link is muted. */
  NODE_LINK_MUTED = 1 << 4,
  /**
   * The dragged node would be inserted here, but this link is ignored because it's not compatible
   * with the node.
   */
  NODE_LINK_INSERT_TARGET_INVALID = 1 << 5,
};

/** #NodeTree.type, index */

enum {
  /** Represents #NodeTreeTypeUndefined type. */
  NTREE_UNDEFINED = -2,
  /** For dynamically registered custom types. */
  NTREE_CUSTOM = -1,
  NTREE_SHADER = 0,
  NTREE_COMPOSIT = 1,
  NTREE_TEXTURE = 2,
  NTREE_GEOMETRY = 3,
};

/** #NodeTree.flag */
enum {
  /** For animation editors. */
  NTREE_DS_EXPAND = 1 << 0,
  /** Two pass. */
  NTREE_UNUSED_2 = 1 << 2, /* cleared */
  /** Use a border for viewer nodes. */
  NTREE_VIEWER_BORDER = 1 << 4,
  /**
   * Tree is localized copy, free when deleting node groups.
   * NOTE: DEPRECATED, use (id->tag & ID_TAG_LOCALIZED) instead.
   */
  // NTREE_IS_LOCALIZED = 1 << 5,
};

enum eNodeTreeRuntimeFlag {
  /** There is a node that references an image with animation. */
  NTREE_RUNTIME_FLAG_HAS_IMAGE_ANIMATION = 1 << 0,
  /** There is a material output node in the group. */
  NTREE_RUNTIME_FLAG_HAS_MATERIAL_OUTPUT = 1 << 1,
  /** There is a simulation zone in the group. */
  NTREE_RUNTIME_FLAG_HAS_SIMULATION_ZONE = 1 << 2,
};

enum GeometryNodeAssetTraitFlag {
  GEO_NODE_ASSET_TOOL = (1 << 0),
  GEO_NODE_ASSET_EDIT = (1 << 1),
  GEO_NODE_ASSET_SCULPT = (1 << 2),
  GEO_NODE_ASSET_MESH = (1 << 3),
  GEO_NODE_ASSET_CURVE = (1 << 4),
  GEO_NODE_ASSET_POINTCLOUD = (1 << 5),
  GEO_NODE_ASSET_MODIFIER = (1 << 6),
  GEO_NODE_ASSET_OBJECT = (1 << 7),
  GEO_NODE_ASSET_WAIT_FOR_CURSOR = (1 << 8),
  GEO_NODE_ASSET_GREASE_PENCIL = (1 << 9),
  /* Only used by Grease Pencil for now. */
  GEO_NODE_ASSET_PAINT = (1 << 10),
  GEO_NODE_ASSET_HIDE_MODIFIER_MANAGE_PANEL = (1 << 11),
};
ENUM_OPERATORS(GeometryNodeAssetTraitFlag);

/* Data structs, for `node->storage`. */

enum CMPNodeMaskType {
  CMP_NODE_MASKTYPE_ADD = 0,
  CMP_NODE_MASKTYPE_SUBTRACT = 1,
  CMP_NODE_MASKTYPE_MULTIPLY = 2,
  CMP_NODE_MASKTYPE_NOT = 3,
};

enum CMPNodeDilateErodeMethod {
  CMP_NODE_DILATE_ERODE_STEP = 0,
  CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD = 1,
  CMP_NODE_DILATE_ERODE_DISTANCE = 2,
  CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER = 3,
};

enum {
  CMP_NODE_INPAINT_SIMPLE = 0,
};

enum CMPNodeMaskFlags {
  /* CMP_NODEFLAG_MASK_AA          = (1 << 0), */ /* DEPRECATED */
  CMP_NODE_MASK_FLAG_NO_FEATHER = (1 << 1),
  CMP_NODE_MASK_FLAG_MOTION_BLUR = (1 << 2),

  /** We may want multiple aspect options, exposed as an rna enum. */
  CMP_NODE_MASK_FLAG_SIZE_FIXED = (1 << 8),
  CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE = (1 << 9),
};

/* Glare Node. Stored in NodeGlare.quality. */
enum CMPNodeGlareQuality {
  CMP_NODE_GLARE_QUALITY_HIGH = 0,
  CMP_NODE_GLARE_QUALITY_MEDIUM = 1,
  CMP_NODE_GLARE_QUALITY_LOW = 2,
};

enum NodeGeometryViewerItemFlag {
  /**
   * Automatically remove the viewer item when there is no link connected to it. This simplifies
   * working with viewers when one adds and removes values to view all the time.
   *
   * This is a flag instead of always being used, because sometimes the user or some script sets up
   * multiple inputs which shouldn't be deleted immediately. This flag is automatically set when
   * viewer items are added interactively in the node editor.
   */
  NODE_GEO_VIEWER_ITEM_FLAG_AUTO_REMOVE = (1 << 0),
};

enum NodeClosureFlag {
  NODE_CLOSURE_FLAG_DEFINE_SIGNATURE = (1 << 0),
};

enum NodeEvaluateClosureFlag {
  NODE_EVALUATE_CLOSURE_FLAG_DEFINE_SIGNATURE = (1 << 0),
};

enum NodeGeometryTransformGizmoFlag {
  GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_X = 1 << 0,
  GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Y = 1 << 1,
  GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Z = 1 << 2,
  GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_X = 1 << 3,
  GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Y = 1 << 4,
  GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Z = 1 << 5,
  GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_X = 1 << 6,
  GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Y = 1 << 7,
  GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Z = 1 << 8,
};

#define GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_ALL \
  (GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_X | GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Y | \
   GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Z)

#define GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_ALL \
  (GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_X | GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Y | \
   GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Z)

#define GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_ALL \
  (GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_X | GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Y | \
   GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Z)

enum NodeGeometryBakeItemFlag {
  GEO_NODE_BAKE_ITEM_IS_ATTRIBUTE = (1 << 0),
};

enum NodeCombineBundleFlag {
  NODE_COMBINE_BUNDLE_FLAG_DEFINE_SIGNATURE = (1 << 0),
};

enum NodeSeparateBundleFlag {
  NODE_SEPARATE_BUNDLE_FLAG_DEFINE_SIGNATURE = (1 << 0),
};

/* script node mode */
enum {
  NODE_SCRIPT_INTERNAL = 0,
  NODE_SCRIPT_EXTERNAL = 1,
};

/* script node flag */
enum {
  NODE_SCRIPT_AUTO_UPDATE = 1,
};

/* IES node mode. */
enum {
  NODE_IES_INTERNAL = 0,
  NODE_IES_EXTERNAL = 1,
};

/* Frame node flags. */

enum {
  /** Keep the bounding box minimal. */
  NODE_FRAME_SHRINK = 1,
  /** Test flag, if frame can be resized by user. */
  NODE_FRAME_RESIZEABLE = 2,
};

/* Proxy node flags. */

enum {
  /** Automatically change output type based on link. */
  NODE_PROXY_AUTOTYPE = 1,
};

/* Conductive fresnel types */
enum {
  SHD_PHYSICAL_CONDUCTOR = 0,
  SHD_CONDUCTOR_F82 = 1,
};

/* glossy distributions */
enum {
  SHD_GLOSSY_BECKMANN = 0,
  SHD_GLOSSY_SHARP_DEPRECATED = 1, /* deprecated */
  SHD_GLOSSY_GGX = 2,
  SHD_GLOSSY_ASHIKHMIN_SHIRLEY = 3,
  SHD_GLOSSY_MULTI_GGX = 4,
};

/* sheen distributions */
#define SHD_SHEEN_ASHIKHMIN 0
#define SHD_SHEEN_MICROFIBER 1

/* vector transform */
enum {
  SHD_VECT_TRANSFORM_TYPE_VECTOR = 0,
  SHD_VECT_TRANSFORM_TYPE_POINT = 1,
  SHD_VECT_TRANSFORM_TYPE_NORMAL = 2,
};

enum {
  SHD_VECT_TRANSFORM_SPACE_WORLD = 0,
  SHD_VECT_TRANSFORM_SPACE_OBJECT = 1,
  SHD_VECT_TRANSFORM_SPACE_CAMERA = 2,
};

/** #NodeShaderAttribute.type */
enum {
  SHD_ATTRIBUTE_GEOMETRY = 0,
  SHD_ATTRIBUTE_OBJECT = 1,
  SHD_ATTRIBUTE_INSTANCER = 2,
  SHD_ATTRIBUTE_VIEW_LAYER = 3,
};

/* toon modes */
enum {
  SHD_TOON_DIFFUSE = 0,
  SHD_TOON_GLOSSY = 1,
};

/* hair components */
enum {
  SHD_HAIR_REFLECTION = 0,
  SHD_HAIR_TRANSMISSION = 1,
};

/* principled hair models */
enum {
  SHD_PRINCIPLED_HAIR_CHIANG = 0,
  SHD_PRINCIPLED_HAIR_HUANG = 1,
};

/* principled hair color parametrization */
enum {
  SHD_PRINCIPLED_HAIR_REFLECTANCE = 0,
  SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION = 1,
  SHD_PRINCIPLED_HAIR_DIRECT_ABSORPTION = 2,
};

/* blend texture */
enum {
  SHD_BLEND_LINEAR = 0,
  SHD_BLEND_QUADRATIC = 1,
  SHD_BLEND_EASING = 2,
  SHD_BLEND_DIAGONAL = 3,
  SHD_BLEND_RADIAL = 4,
  SHD_BLEND_QUADRATIC_SPHERE = 5,
  SHD_BLEND_SPHERICAL = 6,
};

/* noise basis for textures */
enum {
  SHD_NOISE_PERLIN = 0,
  SHD_NOISE_VORONOI_F1 = 1,
  SHD_NOISE_VORONOI_F2 = 2,
  SHD_NOISE_VORONOI_F3 = 3,
  SHD_NOISE_VORONOI_F4 = 4,
  SHD_NOISE_VORONOI_F2_F1 = 5,
  SHD_NOISE_VORONOI_CRACKLE = 6,
  SHD_NOISE_CELL_NOISE = 7,
};

enum {
  SHD_NOISE_SOFT = 0,
  SHD_NOISE_HARD = 1,
};

/* Voronoi Texture */

enum {
  SHD_VORONOI_EUCLIDEAN = 0,
  SHD_VORONOI_MANHATTAN = 1,
  SHD_VORONOI_CHEBYCHEV = 2,
  SHD_VORONOI_MINKOWSKI = 3,
};

enum {
  SHD_VORONOI_F1 = 0,
  SHD_VORONOI_F2 = 1,
  SHD_VORONOI_SMOOTH_F1 = 2,
  SHD_VORONOI_DISTANCE_TO_EDGE = 3,
  SHD_VORONOI_N_SPHERE_RADIUS = 4,
};

/* Deprecated Musgrave Texture. Keep for Versioning */
enum {
  SHD_MUSGRAVE_MULTIFRACTAL = 0,
  SHD_MUSGRAVE_FBM = 1,
  SHD_MUSGRAVE_HYBRID_MULTIFRACTAL = 2,
  SHD_MUSGRAVE_RIDGED_MULTIFRACTAL = 3,
  SHD_MUSGRAVE_HETERO_TERRAIN = 4,
};

/* Noise Texture */
enum {
  SHD_NOISE_MULTIFRACTAL = 0,
  SHD_NOISE_FBM = 1,
  SHD_NOISE_HYBRID_MULTIFRACTAL = 2,
  SHD_NOISE_RIDGED_MULTIFRACTAL = 3,
  SHD_NOISE_HETERO_TERRAIN = 4,
};

/* wave texture */
enum {
  SHD_WAVE_BANDS = 0,
  SHD_WAVE_RINGS = 1,
};

enum {
  SHD_WAVE_BANDS_DIRECTION_X = 0,
  SHD_WAVE_BANDS_DIRECTION_Y = 1,
  SHD_WAVE_BANDS_DIRECTION_Z = 2,
  SHD_WAVE_BANDS_DIRECTION_DIAGONAL = 3,
};

enum {
  SHD_WAVE_RINGS_DIRECTION_X = 0,
  SHD_WAVE_RINGS_DIRECTION_Y = 1,
  SHD_WAVE_RINGS_DIRECTION_Z = 2,
  SHD_WAVE_RINGS_DIRECTION_SPHERICAL = 3,
};

enum {
  SHD_WAVE_PROFILE_SIN = 0,
  SHD_WAVE_PROFILE_SAW = 1,
  SHD_WAVE_PROFILE_TRI = 2,
};

/* sky texture */
enum {
  SHD_SKY_PREETHAM = 0,
  SHD_SKY_HOSEK = 1,
  SHD_SKY_SINGLE_SCATTERING = 2,
  SHD_SKY_MULTIPLE_SCATTERING = 3,
};

/* environment texture */
enum {
  SHD_PROJ_EQUIRECTANGULAR = 0,
  SHD_PROJ_MIRROR_BALL = 1,
};

enum NodeGaborType {
  SHD_GABOR_TYPE_2D = 0,
  SHD_GABOR_TYPE_3D = 1,
};

enum {
  SHD_IMAGE_EXTENSION_REPEAT = 0,
  SHD_IMAGE_EXTENSION_EXTEND = 1,
  SHD_IMAGE_EXTENSION_CLIP = 2,
  SHD_IMAGE_EXTENSION_MIRROR = 3,
};

/* image texture */
enum {
  SHD_PROJ_FLAT = 0,
  SHD_PROJ_BOX = 1,
  SHD_PROJ_SPHERE = 2,
  SHD_PROJ_TUBE = 3,
};

/* image texture interpolation */
enum {
  SHD_INTERP_LINEAR = 0,
  SHD_INTERP_CLOSEST = 1,
  SHD_INTERP_CUBIC = 2,
  SHD_INTERP_SMART = 3,
};

/* tangent */
enum {
  SHD_TANGENT_RADIAL = 0,
  SHD_TANGENT_UVMAP = 1,
};

/* tangent */
enum {
  SHD_TANGENT_AXIS_X = 0,
  SHD_TANGENT_AXIS_Y = 1,
  SHD_TANGENT_AXIS_Z = 2,
};

/* normal map, displacement space */
enum {
  SHD_SPACE_TANGENT = 0,
  SHD_SPACE_OBJECT = 1,
  SHD_SPACE_WORLD = 2,
  SHD_SPACE_BLENDER_OBJECT = 3,
  SHD_SPACE_BLENDER_WORLD = 4,
};

enum {
  SHD_AO_INSIDE = 1,
  SHD_AO_LOCAL = 2,
};

/** Mapping node vector types. */
enum {
  NODE_MAPPING_TYPE_POINT = 0,
  NODE_MAPPING_TYPE_TEXTURE = 1,
  NODE_MAPPING_TYPE_VECTOR = 2,
  NODE_MAPPING_TYPE_NORMAL = 3,
};

/** Rotation node vector types. */
enum {
  NODE_VECTOR_ROTATE_TYPE_AXIS = 0,
  NODE_VECTOR_ROTATE_TYPE_AXIS_X = 1,
  NODE_VECTOR_ROTATE_TYPE_AXIS_Y = 2,
  NODE_VECTOR_ROTATE_TYPE_AXIS_Z = 3,
  NODE_VECTOR_ROTATE_TYPE_EULER_XYZ = 4,
};

/* math node clamp */
enum {
  SHD_MATH_CLAMP = 1,
};

enum NodeMathOperation {
  NODE_MATH_ADD = 0,
  NODE_MATH_SUBTRACT = 1,
  NODE_MATH_MULTIPLY = 2,
  NODE_MATH_DIVIDE = 3,
  NODE_MATH_SINE = 4,
  NODE_MATH_COSINE = 5,
  NODE_MATH_TANGENT = 6,
  NODE_MATH_ARCSINE = 7,
  NODE_MATH_ARCCOSINE = 8,
  NODE_MATH_ARCTANGENT = 9,
  NODE_MATH_POWER = 10,
  NODE_MATH_LOGARITHM = 11,
  NODE_MATH_MINIMUM = 12,
  NODE_MATH_MAXIMUM = 13,
  NODE_MATH_ROUND = 14,
  NODE_MATH_LESS_THAN = 15,
  NODE_MATH_GREATER_THAN = 16,
  NODE_MATH_MODULO = 17,
  NODE_MATH_ABSOLUTE = 18,
  NODE_MATH_ARCTAN2 = 19,
  NODE_MATH_FLOOR = 20,
  NODE_MATH_CEIL = 21,
  NODE_MATH_FRACTION = 22,
  NODE_MATH_SQRT = 23,
  NODE_MATH_INV_SQRT = 24,
  NODE_MATH_SIGN = 25,
  NODE_MATH_EXPONENT = 26,
  NODE_MATH_RADIANS = 27,
  NODE_MATH_DEGREES = 28,
  NODE_MATH_SINH = 29,
  NODE_MATH_COSH = 30,
  NODE_MATH_TANH = 31,
  NODE_MATH_TRUNC = 32,
  NODE_MATH_SNAP = 33,
  NODE_MATH_WRAP = 34,
  NODE_MATH_COMPARE = 35,
  NODE_MATH_MULTIPLY_ADD = 36,
  NODE_MATH_PINGPONG = 37,
  NODE_MATH_SMOOTH_MIN = 38,
  NODE_MATH_SMOOTH_MAX = 39,
  NODE_MATH_FLOORED_MODULO = 40,
};

enum NodeVectorMathOperation {
  NODE_VECTOR_MATH_ADD = 0,
  NODE_VECTOR_MATH_SUBTRACT = 1,
  NODE_VECTOR_MATH_MULTIPLY = 2,
  NODE_VECTOR_MATH_DIVIDE = 3,

  NODE_VECTOR_MATH_CROSS_PRODUCT = 4,
  NODE_VECTOR_MATH_PROJECT = 5,
  NODE_VECTOR_MATH_REFLECT = 6,
  NODE_VECTOR_MATH_DOT_PRODUCT = 7,

  NODE_VECTOR_MATH_DISTANCE = 8,
  NODE_VECTOR_MATH_LENGTH = 9,
  NODE_VECTOR_MATH_SCALE = 10,
  NODE_VECTOR_MATH_NORMALIZE = 11,

  NODE_VECTOR_MATH_SNAP = 12,
  NODE_VECTOR_MATH_FLOOR = 13,
  NODE_VECTOR_MATH_CEIL = 14,
  NODE_VECTOR_MATH_MODULO = 15,
  NODE_VECTOR_MATH_FRACTION = 16,
  NODE_VECTOR_MATH_ABSOLUTE = 17,
  NODE_VECTOR_MATH_MINIMUM = 18,
  NODE_VECTOR_MATH_MAXIMUM = 19,
  NODE_VECTOR_MATH_WRAP = 20,
  NODE_VECTOR_MATH_SINE = 21,
  NODE_VECTOR_MATH_COSINE = 22,
  NODE_VECTOR_MATH_TANGENT = 23,
  NODE_VECTOR_MATH_REFRACT = 24,
  NODE_VECTOR_MATH_FACEFORWARD = 25,
  NODE_VECTOR_MATH_MULTIPLY_ADD = 26,
  NODE_VECTOR_MATH_POWER = 27,
  NODE_VECTOR_MATH_SIGN = 28,
};

enum NodeBooleanMathOperation {
  NODE_BOOLEAN_MATH_AND = 0,
  NODE_BOOLEAN_MATH_OR = 1,
  NODE_BOOLEAN_MATH_NOT = 2,

  NODE_BOOLEAN_MATH_NAND = 3,
  NODE_BOOLEAN_MATH_NOR = 4,
  NODE_BOOLEAN_MATH_XNOR = 5,
  NODE_BOOLEAN_MATH_XOR = 6,

  NODE_BOOLEAN_MATH_IMPLY = 7,
  NODE_BOOLEAN_MATH_NIMPLY = 8,
};

enum NodeShaderMixMode {
  NODE_MIX_MODE_UNIFORM = 0,
  NODE_MIX_MODE_NON_UNIFORM = 1,
};

enum NodeCompareMode {
  NODE_COMPARE_MODE_ELEMENT = 0,
  NODE_COMPARE_MODE_LENGTH = 1,
  NODE_COMPARE_MODE_AVERAGE = 2,
  NODE_COMPARE_MODE_DOT_PRODUCT = 3,
  NODE_COMPARE_MODE_DIRECTION = 4
};

enum NodeCompareOperation {
  NODE_COMPARE_LESS_THAN = 0,
  NODE_COMPARE_LESS_EQUAL = 1,
  NODE_COMPARE_GREATER_THAN = 2,
  NODE_COMPARE_GREATER_EQUAL = 3,
  NODE_COMPARE_EQUAL = 4,
  NODE_COMPARE_NOT_EQUAL = 5,
  NODE_COMPARE_COLOR_BRIGHTER = 6,
  NODE_COMPARE_COLOR_DARKER = 7,
};

enum NodeIntegerMathOperation {
  NODE_INTEGER_MATH_ADD = 0,
  NODE_INTEGER_MATH_SUBTRACT = 1,
  NODE_INTEGER_MATH_MULTIPLY = 2,
  NODE_INTEGER_MATH_DIVIDE = 3,
  NODE_INTEGER_MATH_MULTIPLY_ADD = 4,
  NODE_INTEGER_MATH_POWER = 5,
  NODE_INTEGER_MATH_FLOORED_MODULO = 6,
  NODE_INTEGER_MATH_ABSOLUTE = 7,
  NODE_INTEGER_MATH_MINIMUM = 8,
  NODE_INTEGER_MATH_MAXIMUM = 9,
  NODE_INTEGER_MATH_GCD = 10,
  NODE_INTEGER_MATH_LCM = 11,
  NODE_INTEGER_MATH_NEGATE = 12,
  NODE_INTEGER_MATH_SIGN = 13,
  NODE_INTEGER_MATH_DIVIDE_FLOOR = 14,
  NODE_INTEGER_MATH_DIVIDE_CEIL = 15,
  NODE_INTEGER_MATH_DIVIDE_ROUND = 16,
  NODE_INTEGER_MATH_MODULO = 17,
};

enum FloatToIntRoundingMode {
  FN_NODE_FLOAT_TO_INT_ROUND = 0,
  FN_NODE_FLOAT_TO_INT_FLOOR = 1,
  FN_NODE_FLOAT_TO_INT_CEIL = 2,
  FN_NODE_FLOAT_TO_INT_TRUNCATE = 3,
};

/** Clamp node types. */
enum {
  NODE_CLAMP_MINMAX = 0,
  NODE_CLAMP_RANGE = 1,
};

/** Map range node types. */
enum {
  NODE_MAP_RANGE_LINEAR = 0,
  NODE_MAP_RANGE_STEPPED = 1,
  NODE_MAP_RANGE_SMOOTHSTEP = 2,
  NODE_MAP_RANGE_SMOOTHERSTEP = 3,
};

/* mix rgb node flags */
enum {
  SHD_MIXRGB_USE_ALPHA = 1,
  SHD_MIXRGB_CLAMP = 2,
};

/* Subsurface. */

enum {
#ifdef DNA_DEPRECATED_ALLOW
  SHD_SUBSURFACE_COMPATIBLE = 0, /* Deprecated */
  SHD_SUBSURFACE_CUBIC = 1,
  SHD_SUBSURFACE_GAUSSIAN = 2,
#endif
  SHD_SUBSURFACE_BURLEY = 3,
  SHD_SUBSURFACE_RANDOM_WALK = 4,
  SHD_SUBSURFACE_RANDOM_WALK_SKIN = 5,
};

/* blur node */
enum {
  CMP_NODE_BLUR_ASPECT_NONE = 0,
  CMP_NODE_BLUR_ASPECT_Y = 1,
  CMP_NODE_BLUR_ASPECT_X = 2,
};

enum CMPNodeTranslateRepeatAxis {
  CMP_NODE_TRANSLATE_REPEAT_AXIS_NONE = 0,
  CMP_NODE_TRANSLATE_REPEAT_AXIS_X = 1,
  CMP_NODE_TRANSLATE_REPEAT_AXIS_Y = 2,
  CMP_NODE_TRANSLATE_REPEAT_AXIS_XY = 3,
};

enum CMPExtensionMode {
  CMP_NODE_EXTENSION_MODE_CLIP = 0,
  CMP_NODE_EXTENSION_MODE_EXTEND = 1,
  CMP_NODE_EXTENSION_MODE_REPEAT = 2,
};

#define CMP_NODE_MASK_MBLUR_SAMPLES_MAX 64

/* viewer and composite output. */
enum {
  CMP_NODE_OUTPUT_IGNORE_ALPHA = 1,
};

/** Color Balance Node. Stored in `custom1`. */
enum CMPNodeColorBalanceMethod {
  CMP_NODE_COLOR_BALANCE_LGG = 0,
  CMP_NODE_COLOR_BALANCE_ASC_CDL = 1,
  CMP_NODE_COLOR_BALANCE_WHITEPOINT = 2,
};

/** Alpha Convert Node. Stored in `custom1`. */
enum CMPNodeAlphaConvertMode {
  CMP_NODE_ALPHA_CONVERT_PREMULTIPLY = 0,
  CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY = 1,
};

/** Distance Matte Node. Stored in #NodeChroma.channel. */
enum CMPNodeDistanceMatteColorSpace {
  CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA = 0,
  CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_YCCA = 1,
};

/** Color Spill Node. Stored in `custom2`. */
enum CMPNodeColorSpillLimitAlgorithm {
  CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_SINGLE = 0,
  CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_AVERAGE = 1,
};

/** Channel Matte Node. Stored in #NodeChroma.algorithm. */
enum CMPNodeChannelMatteLimitAlgorithm {
  CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE = 0,
  CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_MAX = 1,
};

/* Flip Node. Stored in custom1. */
enum CMPNodeFlipMode {
  CMP_NODE_FLIP_X = 0,
  CMP_NODE_FLIP_Y = 1,
  CMP_NODE_FLIP_X_Y = 2,
};

/* Scale Node. Stored in custom1. */
enum CMPNodeScaleMethod {
  CMP_NODE_SCALE_RELATIVE = 0,
  CMP_NODE_SCALE_ABSOLUTE = 1,
  CMP_NODE_SCALE_RENDER_PERCENT = 2,
  CMP_NODE_SCALE_RENDER_SIZE = 3,
};

/* Scale Node. Stored in custom2. */
enum CMPNodeScaleRenderSizeMethod {
  CMP_NODE_SCALE_RENDER_SIZE_STRETCH = 0,
  CMP_NODE_SCALE_RENDER_SIZE_FIT = 1,
  CMP_NODE_SCALE_RENDER_SIZE_CROP = 2,
};

/* Filter Node. Stored in custom1. */
enum CMPNodeFilterMethod {
  CMP_NODE_FILTER_SOFT = 0,
  CMP_NODE_FILTER_SHARP_BOX = 1,
  CMP_NODE_FILTER_LAPLACE = 2,
  CMP_NODE_FILTER_SOBEL = 3,
  CMP_NODE_FILTER_PREWITT = 4,
  CMP_NODE_FILTER_KIRSCH = 5,
  CMP_NODE_FILTER_SHADOW = 6,
  CMP_NODE_FILTER_SHARP_DIAMOND = 7,
};

/* Levels Node. Stored in custom1. */
enum CMPNodeLevelsChannel {
  CMP_NODE_LEVLES_LUMINANCE = 1,
  CMP_NODE_LEVLES_RED = 2,
  CMP_NODE_LEVLES_GREEN = 3,
  CMP_NODE_LEVLES_BLUE = 4,
  CMP_NODE_LEVLES_LUMINANCE_BT709 = 5,
};

/* Tone Map Node. Stored in NodeTonemap.type. */
enum CMPNodeToneMapType {
  CMP_NODE_TONE_MAP_SIMPLE = 0,
  CMP_NODE_TONE_MAP_PHOTORECEPTOR = 1,
};

/* Track Position Node. Stored in custom1. */
enum CMPNodeTrackPositionMode {
  CMP_NODE_TRACK_POSITION_ABSOLUTE = 0,
  CMP_NODE_TRACK_POSITION_RELATIVE_START = 1,
  CMP_NODE_TRACK_POSITION_RELATIVE_FRAME = 2,
  CMP_NODE_TRACK_POSITION_ABSOLUTE_FRAME = 3,
};

/* Glare Node. Stored in NodeGlare.type. */
enum CMPNodeGlareType {
  CMP_NODE_GLARE_SIMPLE_STAR = 0,
  CMP_NODE_GLARE_FOG_GLOW = 1,
  CMP_NODE_GLARE_STREAKS = 2,
  CMP_NODE_GLARE_GHOST = 3,
  CMP_NODE_GLARE_BLOOM = 4,
  CMP_NODE_GLARE_SUN_BEAMS = 5,
  CMP_NODE_GLARE_KERNEL = 6,
};

/* Kuwahara Node. Stored in variation */
enum CMPNodeKuwahara {
  CMP_NODE_KUWAHARA_CLASSIC = 0,
  CMP_NODE_KUWAHARA_ANISOTROPIC = 1,
};

/* Shared between nodes with interpolation option. */
enum CMPNodeInterpolation {
  CMP_NODE_INTERPOLATION_NEAREST = 0,
  CMP_NODE_INTERPOLATION_BILINEAR = 1,
  CMP_NODE_INTERPOLATION_BICUBIC = 2,
  CMP_NODE_INTERPOLATION_ANISOTROPIC = 3,
};

/** #NodeSetAlpha.mode */
enum CMPNodeSetAlphaMode {
  CMP_NODE_SETALPHA_MODE_APPLY = 0,
  CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA = 1,
};

/** #NodeBlur.type */
enum CMPNodeBlurType {
  CMP_NODE_BLUR_TYPE_BOX = 0,
  CMP_NODE_BLUR_TYPE_TENT = 1,
  CMP_NODE_BLUR_TYPE_QUAD = 2,
  CMP_NODE_BLUR_TYPE_CUBIC = 3,
  CMP_NODE_BLUR_TYPE_CATROM = 4,
  CMP_NODE_BLUR_TYPE_GAUSS = 5,
  CMP_NODE_BLUR_TYPE_MITCH = 6,
  CMP_NODE_BLUR_TYPE_FAST_GAUSS = 7,
};

/** #NodeDenoise.prefilter */
enum CMPNodeDenoisePrefilter {
  CMP_NODE_DENOISE_PREFILTER_FAST = 0,
  CMP_NODE_DENOISE_PREFILTER_NONE = 1,
  CMP_NODE_DENOISE_PREFILTER_ACCURATE = 2
};

/** #NodeDenoise.quality */
enum CMPNodeDenoiseQuality {
  CMP_NODE_DENOISE_QUALITY_SCENE = 0,
  CMP_NODE_DENOISE_QUALITY_HIGH = 1,
  CMP_NODE_DENOISE_QUALITY_BALANCED = 2,
  CMP_NODE_DENOISE_QUALITY_FAST = 3,
};

/* Color combine/separate modes */

enum CMPNodeCombSepColorMode {
  CMP_NODE_COMBSEP_COLOR_RGB = 0,
  CMP_NODE_COMBSEP_COLOR_HSV = 1,
  CMP_NODE_COMBSEP_COLOR_HSL = 2,
  CMP_NODE_COMBSEP_COLOR_YCC = 3,
  CMP_NODE_COMBSEP_COLOR_YUV = 4,
};

/* Cryptomatte node source. */
enum CMPNodeCryptomatteSource {
  CMP_NODE_CRYPTOMATTE_SOURCE_RENDER = 0,
  CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE = 1,
};

/* Channel Matte node, stored in custom1. */
enum CMPNodeChannelMatteColorSpace {
  CMP_NODE_CHANNEL_MATTE_CS_RGB = 0,
  CMP_NODE_CHANNEL_MATTE_CS_HSV = 1,
  CMP_NODE_CHANNEL_MATTE_CS_YUV = 2,
  CMP_NODE_CHANNEL_MATTE_CS_YCC = 3,
};

/* NodeLensDist.distortion_type. */
enum CMPNodeLensDistortionType {
  CMP_NODE_LENS_DISTORTION_RADIAL = 0,
  CMP_NODE_LENS_DISTORTION_HORIZONTAL = 1,
};

/* Alpha Over node. Stored in custom1. */
enum CMPNodeAlphaOverOperationType {
  CMP_NODE_ALPHA_OVER_OPERATION_TYPE_OVER = 0,
  CMP_NODE_ALPHA_OVER_OPERATION_TYPE_DISJOINT_OVER = 1,
  CMP_NODE_ALPHA_OVER_OPERATION_TYPE_CONJOINT_OVER = 2,
};

/* Relative To Pixel node. Stored in custom1. */
enum CMPNodeRelativeToPixelDataType {
  CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT = 0,
  CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_VECTOR = 1,
};

/* Relative To Pixel node. Stored in custom2. */
enum CMPNodeRelativeToPixelReferenceDimension {
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_PER_DIMENSION = 0,
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_X = 1,
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_Y = 2,
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_GREATER = 3,
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_SMALLER = 4,
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_DIAGONAL = 5,
};

/* Scattering phase functions */
enum {
  SHD_PHASE_HENYEY_GREENSTEIN = 0,
  SHD_PHASE_FOURNIER_FORAND = 1,
  SHD_PHASE_DRAINE = 2,
  SHD_PHASE_RAYLEIGH = 3,
  SHD_PHASE_MIE = 4,
};

/* Output shader node */

enum NodeShaderOutputTarget {
  SHD_OUTPUT_ALL = 0,
  SHD_OUTPUT_EEVEE = 1,
  SHD_OUTPUT_CYCLES = 2,
};

/* Geometry Nodes */

enum GeometryNodeProximityTargetType {
  GEO_NODE_PROX_TARGET_POINTS = 0,
  GEO_NODE_PROX_TARGET_EDGES = 1,
  GEO_NODE_PROX_TARGET_FACES = 2,
};

enum GeometryNodeCurvePrimitiveCircleMode {
  GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_POINTS = 0,
  GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_RADIUS = 1
};

enum GeometryNodeCurveHandleType {
  GEO_NODE_CURVE_HANDLE_FREE = 0,
  GEO_NODE_CURVE_HANDLE_AUTO = 1,
  GEO_NODE_CURVE_HANDLE_VECTOR = 2,
  GEO_NODE_CURVE_HANDLE_ALIGN = 3
};

enum GeometryNodeCurveHandleMode {
  GEO_NODE_CURVE_HANDLE_LEFT = (1 << 0),
  GEO_NODE_CURVE_HANDLE_RIGHT = (1 << 1)
};

enum GeometryNodeDistributePointsInVolumeMode {
  GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_RANDOM = 0,
  GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_GRID = 1,
};

enum GeometryNodeDistributePointsOnFacesMode {
  GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM = 0,
  GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON = 1,
};

enum GeometryNodeExtrudeMeshMode {
  GEO_NODE_EXTRUDE_MESH_VERTICES = 0,
  GEO_NODE_EXTRUDE_MESH_EDGES = 1,
  GEO_NODE_EXTRUDE_MESH_FACES = 2,
};

enum FunctionNodeRotateEulerType {
  FN_NODE_ROTATE_EULER_TYPE_EULER = 0,
  FN_NODE_ROTATE_EULER_TYPE_AXIS_ANGLE = 1,
};

enum FunctionNodeRotateEulerSpace {
  FN_NODE_ROTATE_EULER_SPACE_OBJECT = 0,
  FN_NODE_ROTATE_EULER_SPACE_LOCAL = 1,
};

enum NodeAlignEulerToVectorAxis {
  FN_NODE_ALIGN_EULER_TO_VECTOR_AXIS_X = 0,
  FN_NODE_ALIGN_EULER_TO_VECTOR_AXIS_Y = 1,
  FN_NODE_ALIGN_EULER_TO_VECTOR_AXIS_Z = 2,
};

enum NodeAlignEulerToVectorPivotAxis {
  FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_AUTO = 0,
  FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_X = 1,
  FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_Y = 2,
  FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_Z = 3,
};

enum GeometryNodeTransformSpace {
  GEO_NODE_TRANSFORM_SPACE_ORIGINAL = 0,
  GEO_NODE_TRANSFORM_SPACE_RELATIVE = 1,
};

enum GeometryNodePointsToVolumeResolutionMode {
  GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT = 0,
  GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE = 1,
};

enum GeometryNodeMeshCircleFillType {
  GEO_NODE_MESH_CIRCLE_FILL_NONE = 0,
  GEO_NODE_MESH_CIRCLE_FILL_NGON = 1,
  GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN = 2,
};

enum GeometryNodeMergeByDistanceMode {
  GEO_NODE_MERGE_BY_DISTANCE_MODE_ALL = 0,
  GEO_NODE_MERGE_BY_DISTANCE_MODE_CONNECTED = 1,
};

enum GeometryNodeUVUnwrapMethod {
  GEO_NODE_UV_UNWRAP_METHOD_ANGLE_BASED = 0,
  GEO_NODE_UV_UNWRAP_METHOD_CONFORMAL = 1,
  GEO_NODE_UV_UNWRAP_METHOD_MINIMUM_STRETCH = 2,
};

enum GeometryNodeRealizeInstanceFlag {
  GEO_NODE_REALIZE_TO_POINT_DOMAIN = (1 << 0),
};

enum GeometryNodeMeshLineMode {
  GEO_NODE_MESH_LINE_MODE_END_POINTS = 0,
  GEO_NODE_MESH_LINE_MODE_OFFSET = 1,
};

enum GeometryNodeMeshLineCountMode {
  GEO_NODE_MESH_LINE_COUNT_TOTAL = 0,
  GEO_NODE_MESH_LINE_COUNT_RESOLUTION = 1,
};

enum GeometryNodeCurvePrimitiveArcMode {
  GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_POINTS = 0,
  GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_RADIUS = 1,
};

enum GeometryNodeCurvePrimitiveLineMode {
  GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_POINTS = 0,
  GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_DIRECTION = 1
};

enum GeometryNodeCurvePrimitiveQuadMode {
  GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_RECTANGLE = 0,
  GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_PARALLELOGRAM = 1,
  GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_TRAPEZOID = 2,
  GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_KITE = 3,
  GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_POINTS = 4,
};

enum GeometryNodeCurvePrimitiveBezierSegmentMode {
  GEO_NODE_CURVE_PRIMITIVE_BEZIER_SEGMENT_POSITION = 0,
  GEO_NODE_CURVE_PRIMITIVE_BEZIER_SEGMENT_OFFSET = 1,
};

enum GeometryNodeCurveResampleMode {
  GEO_NODE_CURVE_RESAMPLE_COUNT = 0,
  GEO_NODE_CURVE_RESAMPLE_LENGTH = 1,
  GEO_NODE_CURVE_RESAMPLE_EVALUATED = 2,
};

enum GeometryNodeCurveSampleMode {
  GEO_NODE_CURVE_SAMPLE_FACTOR = 0,
  GEO_NODE_CURVE_SAMPLE_LENGTH = 1,
};

enum GeometryNodeCurveFilletMode {
  GEO_NODE_CURVE_FILLET_BEZIER = 0,
  GEO_NODE_CURVE_FILLET_POLY = 1,
};

enum GeometryNodeAttributeTransferMode {
  GEO_NODE_ATTRIBUTE_TRANSFER_NEAREST_FACE_INTERPOLATED = 0,
  GEO_NODE_ATTRIBUTE_TRANSFER_NEAREST = 1,
  GEO_NODE_ATTRIBUTE_TRANSFER_INDEX = 2,
};

enum GeometryNodeRaycastMapMode {
  GEO_NODE_RAYCAST_INTERPOLATED = 0,
  GEO_NODE_RAYCAST_NEAREST = 1,
};

enum GeometryNodeCurveFillMode {
  GEO_NODE_CURVE_FILL_MODE_TRIANGULATED = 0,
  GEO_NODE_CURVE_FILL_MODE_NGONS = 1,
};

enum GeometryNodeMeshToPointsMode {
  GEO_NODE_MESH_TO_POINTS_VERTICES = 0,
  GEO_NODE_MESH_TO_POINTS_EDGES = 1,
  GEO_NODE_MESH_TO_POINTS_FACES = 2,
  GEO_NODE_MESH_TO_POINTS_CORNERS = 3,
};

enum GeometryNodeStringToCurvesOverflowMode {
  GEO_NODE_STRING_TO_CURVES_MODE_OVERFLOW = 0,
  GEO_NODE_STRING_TO_CURVES_MODE_SCALE_TO_FIT = 1,
  GEO_NODE_STRING_TO_CURVES_MODE_TRUNCATE = 2,
};

enum GeometryNodeStringToCurvesAlignXMode {
  GEO_NODE_STRING_TO_CURVES_ALIGN_X_LEFT = 0,
  GEO_NODE_STRING_TO_CURVES_ALIGN_X_CENTER = 1,
  GEO_NODE_STRING_TO_CURVES_ALIGN_X_RIGHT = 2,
  GEO_NODE_STRING_TO_CURVES_ALIGN_X_JUSTIFY = 3,
  GEO_NODE_STRING_TO_CURVES_ALIGN_X_FLUSH = 4,
};

enum GeometryNodeStringToCurvesAlignYMode {
  GEO_NODE_STRING_TO_CURVES_ALIGN_Y_TOP_BASELINE = 0,
  GEO_NODE_STRING_TO_CURVES_ALIGN_Y_TOP = 1,
  GEO_NODE_STRING_TO_CURVES_ALIGN_Y_MIDDLE = 2,
  GEO_NODE_STRING_TO_CURVES_ALIGN_Y_BOTTOM_BASELINE = 3,
  GEO_NODE_STRING_TO_CURVES_ALIGN_Y_BOTTOM = 4,
};

enum GeometryNodeStringToCurvesPivotMode {
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_MIDPOINT = 0,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_LEFT = 1,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_CENTER = 2,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_RIGHT = 3,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_LEFT = 4,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_CENTER = 5,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_RIGHT = 6,
};

enum GeometryNodeDeleteGeometryMode {
  GEO_NODE_DELETE_GEOMETRY_MODE_ALL = 0,
  GEO_NODE_DELETE_GEOMETRY_MODE_EDGE_FACE = 1,
  GEO_NODE_DELETE_GEOMETRY_MODE_ONLY_FACE = 2,
};

enum GeometryNodeScaleElementsMode {
  GEO_NODE_SCALE_ELEMENTS_UNIFORM = 0,
  GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS = 1,
};

enum NodeCombSepColorMode {
  NODE_COMBSEP_COLOR_RGB = 0,
  NODE_COMBSEP_COLOR_HSV = 1,
  NODE_COMBSEP_COLOR_HSL = 2,
};

enum GeometryNodeGizmoColor {
  GEO_NODE_GIZMO_COLOR_PRIMARY = 0,
  GEO_NODE_GIZMO_COLOR_SECONDARY = 1,
  GEO_NODE_GIZMO_COLOR_X = 2,
  GEO_NODE_GIZMO_COLOR_Y = 3,
  GEO_NODE_GIZMO_COLOR_Z = 4,
};

enum GeometryNodeLinearGizmoDrawStyle {
  GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_ARROW = 0,
  GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_CROSS = 1,
  GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_BOX = 2,
};

enum NodeGeometryTransformMode {
  GEO_NODE_TRANSFORM_MODE_COMPONENTS = 0,
  GEO_NODE_TRANSFORM_MODE_MATRIX = 1,
};

struct bNodeStack {
  float vec[4] = {};
  float min = 0, max = 0;
  void *data = nullptr;
  /** When input has link, tagged before executing. */
  short hasinput = 0;
  /** When output is linked, tagged before executing. */
  short hasoutput = 0;
  /** Type of data pointer. */
  short datatype = 0;
  /** Type of socket stack comes from, to remap linking different sockets. */
  short sockettype = 0;
  /** Data is a copy of external data (no freeing). */
  short is_copy = 0;
  /** Data is used by external nodes (no freeing). */
  short external = 0;
  char _pad[4] = {};
};

struct bNodeSocket {
  struct bNodeSocket *next = nullptr, *prev = nullptr;

  /** User-defined properties. */
  IDProperty *prop = nullptr;

  /** Unique identifier for mapping. */
  char identifier[64] = "";

  char name[/*MAX_NAME*/ 64] = "";

  /** Only used for the Image and OutputFile nodes, should be removed at some point. */
  void *storage = nullptr;

  /**
   * The socket's data type. #eNodeSocketDatatype.
   */
  short type = 0;
  /** #eNodeSocketFlag */
  short flag = 0;
  /**
   * Maximum number of links that can connect to the socket. Read via #nodeSocketLinkLimit, because
   * the limit might be defined on the socket type, in which case this value does not have any
   * effect. It is necessary to store this in the socket because it is exposed as an RNA property
   * for custom nodes.
   */
  short limit = 0;
  /** Input/output type. */
  short in_out = 0;
  /** Runtime type information. */
  bNodeSocketTypeHandle *typeinfo = nullptr;
  /** Runtime type identifier. */
  char idname[64] = "";

  /** Default input value used for unlinked sockets. */
  void *default_value = nullptr;

  /** Local stack index for "node_exec". */
  int stack_index = 0;
  char display_shape = 0;

  /* #AttrDomain used when the geometry nodes modifier creates an attribute for a group
   * output. */
  char attribute_domain = 0;

  char _pad[2] = {};

  /** Custom dynamic defined label. */
  char label[/*MAX_NAME*/ 64] = "";
  char description[/*MAX_NAME*/ 64] = "";

  /**
   * The default attribute name to use for geometry nodes modifier output attribute sockets.
   * \note Storing this pointer in every single socket exposes the bad design of using sockets
   * to describe group inputs and outputs. In the future, it should be stored in socket
   * declarations.
   */
  char *default_attribute_name = nullptr;

  /* internal data to retrieve relations and groups
   * DEPRECATED, now uses the generic identifier string instead
   */
  /** Group socket identifiers, to find matching pairs after reading files. */
  DNA_DEPRECATED int own_index = 0;
  /* XXX deprecated, only used for restoring old group node links */
  DNA_DEPRECATED int to_index = 0;

  /** A link pointer, set in #BKE_ntree_update. */
  struct bNodeLink *link = nullptr;

  /* XXX deprecated, socket input values are stored in default_value now.
   * kept for forward compatibility */
  /** Custom data for inputs, only UI writes in this. */
  DNA_DEPRECATED bNodeStack ns;

  bNodeSocketRuntimeHandle *runtime = nullptr;

#ifdef __cplusplus
  /**
   * Whether the socket is hidden in a way that the user can control.
   *
   * \note: This is not the exact opposite of `is_visible()` which takes other things into account.
   */
  bool is_user_hidden() const;
  /**
   * Socket visibility depends on a few different factors like whether it's hidden by the user,
   * it's available or it's inferred to be hidden based on other inputs.
   *
   * A visible socket has a valid #bNodeSocketRuntime::location. However, it may not actually be
   * drawn as stand-alone socket if it's in a collapsed panel. To check for that, use
   * #is_icon_visible.
   */
  bool is_visible() const;
  /**
   * The socket is visible and it's drawn as a stand-alone icon in the node editor. So any parent
   * panel is open.
   */
  bool is_icon_visible() const;
  /**
   * Unavailable sockets are usually treated as if they don't exist. It's not something that can be
   * controlled by users for built-in nodes.
   */
  bool is_available() const;
  /**
   * Whether this socket is in a collapsed panel.
   */
  bool is_panel_collapsed() const;
  /**
   * Inputs may be grayed out if they are detected to be not affecting the output and the node is
   * not itself some kind of output node.
   */
  bool is_inactive() const;
  /**
   * False when this input socket definitely does not affect the output.
   */
  bool affects_node_output() const;
  /**
   * This becomes false when it is detected that the socket is unused and should be hidden.
   * Inputs: An input should be hidden if it's unused and its usage depends on a menu input (as
   *   opposed to e.g. a boolean input).
   * Outputs: An output is unused if it outputs the socket types fallback value as a constant given
   *   the current set of menu inputs and its value depends on a menu input.
   */
  bool inferred_socket_visibility() const;
  /**
   * True when the value of this socket may be a field. This is inferred during structure type
   * inferencing.
   */
  bool may_be_field() const;

  bool is_multi_input() const;
  bool is_input() const;
  bool is_output() const;

  /** Utility to access the value of the socket. */
  template<typename T> T *default_value_typed();
  template<typename T> const T *default_value_typed() const;

  /* The following methods are only available when #bNodeTree.ensure_topology_cache has been
   * called. */

  /** Zero based index for every input and output socket. */
  int index() const;
  /** Socket index in the entire node tree. Inputs and outputs share the same index space. */
  int index_in_tree() const;
  /** Socket index in the entire node tree. All inputs share the same index space. */
  int index_in_all_inputs() const;
  /** Socket index in the entire node tree. All outputs share the same index space. */
  int index_in_all_outputs() const;
  /** Node this socket belongs to. */
  bNode &owner_node();
  const bNode &owner_node() const;
  /** Node tree this socket belongs to. */
  bNodeTree &owner_tree();
  const bNodeTree &owner_tree() const;

  /** Links which are incident to this socket. */
  blender::Span<bNodeLink *> directly_linked_links();
  blender::Span<const bNodeLink *> directly_linked_links() const;
  /** Sockets which are connected to this socket with a link. */
  blender::Span<bNodeSocket *> directly_linked_sockets();
  blender::Span<const bNodeSocket *> directly_linked_sockets() const;
  bool is_directly_linked() const;
  /**
   * Sockets which are connected to this socket when reroutes and muted nodes are taken into
   * account.
   */
  blender::Span<const bNodeSocket *> logically_linked_sockets() const;
  bool is_logically_linked() const;

  /**
   * For output sockets, this is the corresponding input socket the value of which should be
   * forwarded when the node is muted.
   */
  const bNodeSocket *internal_link_input() const;

#endif
};

struct bNodePanelState {
  /* Unique identifier for validating state against panels in node declaration. */
  int identifier = 0;
  /* eNodePanelFlag */
  char flag = 0;
  char _pad[3] = {};

#ifdef __cplusplus
  bool is_collapsed() const;
  bool is_parent_collapsed() const;
  bool has_visible_content() const;
#endif
};

struct bNode {
  struct bNode *next = nullptr, *prev = nullptr;

  /* Input and output #bNodeSocket. */
  ListBaseT<bNodeSocket> inputs, outputs;

  /** The node's name for unique identification and string lookup. */
  char name[/*MAX_NAME*/ 64] = "";

  /**
   * A value that uniquely identifies a node in a node tree even when the name changes.
   * This also allows referencing nodes more efficiently than with strings.
   *
   * Must be set whenever a node is added to a tree, besides a simple tree copy.
   * Must always be positive.
   */
  int32_t identifier = 0;

  int flag = 0;

  /**
   * String identifier of the type like "FunctionNodeCompare". Stored in files to allow retrieving
   * the node type for node types including custom nodes defined in Python by addons.
   */
  char idname[64] = "";

  /** Type information retrieved from the #idname. TODO: Move to runtime data. */
  bNodeTypeHandle *typeinfo = nullptr;

  /**
   * Legacy integer type for nodes. It does not uniquely identify a node type, only the `idname`
   * does that. For example, all custom nodes use #NODE_CUSTOM but do have different idnames.
   * This is mainly kept for compatibility reasons.
   *
   * Currently, this type is also used in many parts of Blender, but that should slowly be phased
   * out by either relying on idnames, accessor methods like `node.is_reroute()`.
   *
   * Older node types have a stable legacy-type (defined in `BKE_node_legacy_types.hh`). However,
   * the legacy type of newer types is generated at runtime and is not guaranteed to be stable over
   * time.
   *
   * A main benefit of this integer type over using idnames currently is that integer comparison is
   * much cheaper than string comparison, especially if many idnames have the same prefix (e.g.
   * "GeometryNode"). Eventually, we could introduce cheap-to-compare runtime identifier for node
   * types. That could mean e.g. using `ustring` for idnames (where string comparison is just
   * pointer comparison), or using a run-time generated integer that is automatically assigned when
   * node types are registered.
   */
  int16_t type_legacy = 0;

  /**
   * Depth of the node in the node editor, used to keep recently selected nodes at the front, and
   * to order frame nodes properly.
   */
  int16_t ui_order = 0;

  /** Used for some builtin nodes that store properties but don't have a storage struct. */
  int16_t custom1 = 0, custom2 = 0;
  float custom3 = 0, custom4 = 0;

  /**
   * #NodeWarningPropagation.
   */
  int8_t warning_propagation = 0;
  char _pad[7] = {};

  /**
   * Optional link to libdata.
   *
   * \see #bNodeType::initfunc & #bNodeType::freefunc for details on ID user-count.
   */
  struct ID *id = nullptr;

  /** Custom data struct for node properties for storage in files. */
  void *storage = nullptr;

  /**
   * Custom properties often defined by addons to store arbitrary data on nodes. A non-builtin
   * equivalent to #storage.
   */
  IDProperty *prop = nullptr;

  /**
   * System-defined properties, used e.g. to store data for custom node types.
   */
  IDProperty *system_properties = nullptr;

  /** Parent node (for frame nodes). */
  struct bNode *parent = nullptr;

  /** The location of the top left corner of the node on the canvas. */
  float location[2] = {};
  /**
   * Custom width and height controlled by users. Height is calculate automatically for most
   * nodes.
   */
  float width = 0, height = 0;
  float locx_legacy = 0, locy_legacy = 0;
  float offsetx_legacy = 0, offsety_legacy = 0;

  /** Custom user-defined label. */
  char label[/*MAX_NAME*/ 64] = "";

  /** Custom user-defined color. */
  float color[3] = {};

  /** Panel states for this node instance. */
  int num_panel_states = 0;
  bNodePanelState *panel_states_array = nullptr;

  bNodeRuntimeHandle *runtime = nullptr;

#ifdef __cplusplus
  /** The index in the owner node tree. */
  int index() const;
  blender::StringRefNull label_or_name() const;
  bool is_muted() const;
  bool is_reroute() const;
  bool is_frame() const;
  bool is_group() const;
  bool is_custom_group() const;
  bool is_group_input() const;
  bool is_group_output() const;
  bool is_undefined() const;

  /**
   * Check if the node has the given idname.
   *
   * Note: This function assumes that the given idname is a valid registered idname. This is done
   * to catch typos earlier. One can compare with `bNodeType::idname` directly if the idname might
   * not be registered.
   */
  bool is_type(blender::StringRef query_idname) const;

  const blender::nodes::NodeDeclaration *declaration() const;
  /** A span containing all internal links when the node is muted. */
  blender::Span<bNodeLink> internal_links() const;

  /* This node is reroute which is not logically connected to any source of value. */
  bool is_dangling_reroute() const;

  /* The following methods are only available when #bNodeTree.ensure_topology_cache has been
   * called. */

  /** A span containing all input sockets of the node (including unavailable sockets). */
  blender::Span<bNodeSocket *> input_sockets();
  blender::Span<const bNodeSocket *> input_sockets() const;
  blender::IndexRange input_socket_indices_in_tree() const;
  blender::IndexRange input_socket_indices_in_all_inputs() const;
  /** A span containing all output sockets of the node (including unavailable sockets). */
  blender::Span<bNodeSocket *> output_sockets();
  blender::Span<const bNodeSocket *> output_sockets() const;
  blender::IndexRange output_socket_indices_in_tree() const;
  blender::IndexRange output_socket_indices_in_all_outputs() const;
  /** Utility to get an input socket by its index. */
  bNodeSocket &input_socket(int index);
  const bNodeSocket &input_socket(int index) const;
  /** Utility to get an output socket by its index. */
  bNodeSocket &output_socket(int index);
  const bNodeSocket &output_socket(int index) const;
  /** Lookup socket of this node by its identifier. */
  const bNodeSocket *input_by_identifier(blender::StringRef identifier) const;
  const bNodeSocket *output_by_identifier(blender::StringRef identifier) const;
  bNodeSocket *input_by_identifier(blender::StringRef identifier);
  bNodeSocket *output_by_identifier(blender::StringRef identifier);
  /** Lookup socket by its declaration. */
  const bNodeSocket &socket_by_decl(const blender::nodes::SocketDeclaration &decl) const;
  bNodeSocket &socket_by_decl(const blender::nodes::SocketDeclaration &decl);
  /** If node is frame, will return all children nodes. */
  blender::Span<bNode *> direct_children_in_frame() const;
  blender::Span<bNodePanelState> panel_states() const;
  blender::MutableSpan<bNodePanelState> panel_states();
  /** Node tree this node belongs to. */
  const bNodeTree &owner_tree() const;
  bNodeTree &owner_tree();
#endif
};

/**
 * Unique hash key for identifying node instances
 * Defined as a struct because DNA does not support other typedefs.
 */
struct bNodeInstanceKey {
  unsigned int value = 0;

#ifdef __cplusplus
  inline bool operator==(const bNodeInstanceKey &other) const
  {
    return value == other.value;
  }
  inline bool operator!=(const bNodeInstanceKey &other) const
  {
    return !(*this == other);
  }

  inline uint64_t hash() const
  {
    return value;
  }
#endif
};

/**
 * Base struct for entries in node instance hash.
 *
 * \warning pointers are cast to this struct internally,
 * it must be first member in hash entry structs!
 */
#
#
struct bNodeInstanceHashEntry {
  bNodeInstanceKey key;

  /** Tags for cleaning the cache. */
  short tag = 0;
};

struct bNodeLink {
  struct bNodeLink *next = nullptr, *prev = nullptr;

  bNode *fromnode = nullptr, *tonode = nullptr;
  bNodeSocket *fromsock = nullptr, *tosock = nullptr;

  int flag = 0;
  /**
   * Determines the order in which links are connected to a multi-input socket.
   * For historical reasons, larger ids come before lower ids.
   * Usually, this should not be accessed directly. One can instead use e.g.
   * `socket.directly_linked_links()` to get the links in the correct order.
   */
  int multi_input_sort_id = 0;

#ifdef __cplusplus
  bool is_muted() const;
  bool is_available() const;
  /** Both linked sockets are available and the link is not muted. */
  bool is_used() const;
#endif
};

struct bNestedNodePath {
  /** ID of the node that is or contains the nested node. */
  int32_t node_id = 0;
  /** Unused if the node is the final nested node, otherwise an id inside of the (group) node. */
  int32_t id_in_node = 0;

#ifdef __cplusplus
  uint64_t hash() const;
  friend bool operator==(const bNestedNodePath &a, const bNestedNodePath &b);
#endif
};

struct bNestedNodeRef {
  /** Identifies a potentially nested node. This ID remains stable even if the node is moved into
   * and out of node groups. */
  int32_t id = 0;
  char _pad[4] = {};
  /** Where to find the nested node in the current node tree. */
  bNestedNodePath path;
};

/**
 * The basis for a Node tree, all links and nodes reside internal here.
 *
 * Only re-usable node trees are in the library though,
 * materials and textures allocate their own tree struct.
 */
struct bNodeTree {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_NT;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt = nullptr;

  /** The ID owning this node tree, in case it is an embedded one. */
  ID *owner_id = nullptr;

  /** Runtime type information. */
  bNodeTreeTypeHandle *typeinfo = nullptr;
  /** Runtime type identifier. */
  char idname[64] = "";
  /** User-defined description of the node tree. */
  char *description = nullptr;

  /** Grease pencil data. */
  struct bGPdata *gpd = nullptr;
  /** Node tree stores its own offset for consistent editor view. */
  float view_center[2] = {};

  ListBaseT<bNode> nodes;
  ListBaseT<bNodeLink> links;

  int type = 0;

  /**
   * Sockets in groups have unique identifiers, adding new sockets always
   * will increase this counter.
   */
  int cur_index = 0;
  int flag = 0;

  /** Tile size for compositor engine. */
  DNA_DEPRECATED int chunksize = 0;
  /** Execution mode to use for compositor engine. */
  DNA_DEPRECATED int execution_mode = 0;
  /** Precision used by the GPU execution of the compositor tree. */
  DNA_DEPRECATED int precision = 0;

  /** #blender::bke::NodeColorTag. */
  int color_tag = 0;

  /**
   * Default width of a group node created for this group. May be zero, in which case this value
   * should be ignored.
   */
  int default_group_node_width = 0;

  rctf viewer_border = {};

  /**
   * Lists of #bNodeSocket to hold default values and own_index.
   * Warning! Don't make links to these sockets, input/output nodes are used for that.
   * These sockets are used only for generating external interfaces.
   */
  DNA_DEPRECATED ListBaseT<bNodeSocket> inputs_legacy;
  DNA_DEPRECATED ListBaseT<bNodeSocket> outputs_legacy;

  bNodeTreeInterface tree_interface;

  /**
   * Defines the node tree instance to use for the "active" context,
   * in case multiple different editors are used and make context ambiguous.
   */
  bNodeInstanceKey active_viewer_key;

  /**
   * Used to maintain stable IDs for a subset of nested nodes. For example, every simulation zone
   * that is in the node tree has a unique entry here.
   */
  int nested_node_refs_num = 0;
  bNestedNodeRef *nested_node_refs = nullptr;

  struct GeometryNodeAssetTraits *geometry_node_asset_traits = nullptr;

  /** Image representing what the node group does. */
  struct PreviewImage *preview = nullptr;

  bNodeTreeRuntimeHandle *runtime = nullptr;

#ifdef __cplusplus

  /** A span containing all nodes in the node tree. */
  blender::Span<bNode *> all_nodes();
  blender::Span<const bNode *> all_nodes() const;

  /** Retrieve a node based on its persistent integer identifier. */
  struct bNode *node_by_id(int32_t identifier);
  const struct bNode *node_by_id(int32_t identifier) const;

  blender::MutableSpan<bNestedNodeRef> nested_node_refs_span();
  blender::Span<bNestedNodeRef> nested_node_refs_span() const;

  const bNestedNodeRef *find_nested_node_ref(int32_t nested_node_id) const;
  /** Conversions between node id paths and their corresponding nested node ref. */
  const bNestedNodeRef *nested_node_ref_from_node_id_path(blender::Span<int> node_ids) const;
  [[nodiscard]] bool node_id_path_from_nested_node_ref(const int32_t nested_node_id,
                                                       blender::Vector<int32_t> &r_node_ids) const;
  const bNode *find_nested_node(int32_t nested_node_id, const bNodeTree **r_tree = nullptr) const;

  /**
   * Update a run-time cache for the node tree based on its current state. This makes many methods
   * available which allow efficient lookup for topology information (like neighboring sockets).
   */
  void ensure_topology_cache() const;

  /* The following methods are only available when #bNodeTree.ensure_topology_cache has been
   * called. */

  /** A span containing all group nodes in the node tree. */
  blender::Span<bNode *> group_nodes();
  blender::Span<const bNode *> group_nodes() const;
  /** A span containing all input sockets in the node tree. */
  blender::Span<bNodeSocket *> all_input_sockets();
  blender::Span<const bNodeSocket *> all_input_sockets() const;
  /** A span containing all output sockets in the node tree. */
  blender::Span<bNodeSocket *> all_output_sockets();
  blender::Span<const bNodeSocket *> all_output_sockets() const;
  /** A span containing all sockets in the node tree. */
  blender::Span<bNodeSocket *> all_sockets();
  blender::Span<const bNodeSocket *> all_sockets() const;
  /** Efficient lookup of all nodes with a specific type. */
  blender::Span<bNode *> nodes_by_type(blender::StringRefNull type_idname);
  blender::Span<const bNode *> nodes_by_type(blender::StringRefNull type_idname) const;
  /** Frame nodes without any parents. */
  blender::Span<bNode *> root_frames() const;
  /** A span containing all links in the node tree. */
  blender::Span<bNodeLink *> all_links();
  blender::Span<const bNodeLink *> all_links() const;
  /**
   * Cached toposort of all nodes. If there are cycles, the returned array is not actually a
   * toposort. However, if a connected component does not contain a cycle, this component is sorted
   * correctly. Use #has_available_link_cycle to check for cycles.
   */
  blender::Span<bNode *> toposort_left_to_right();
  blender::Span<const bNode *> toposort_left_to_right() const;
  blender::Span<bNode *> toposort_right_to_left();
  blender::Span<const bNode *> toposort_right_to_left() const;
  /** True when there are any cycles in the node tree. */
  bool has_available_link_cycle() const;
  /**
   * True when there are nodes or sockets in the node tree that don't use a known type. This can
   * happen when nodes don't exist in the current Blender version that existed in the version where
   * this node tree was saved.
   */
  bool has_undefined_nodes_or_sockets() const;
  /** Get the active group output node. */
  bNode *group_output_node();
  const bNode *group_output_node() const;
  /** Get all input nodes of the node group. */
  blender::Span<bNode *> group_input_nodes();
  blender::Span<const bNode *> group_input_nodes() const;

  /** Zones in the node tree. Currently there are only simulation zones in geometry nodes. */
  const blender::bke::bNodeTreeZones *zones() const;

  /**
   * Update a run-time cache for the node tree interface based on its current state.
   * This should be done before accessing interface item spans below.
   */
  void ensure_interface_cache() const;

  /* Cached interface item lists. */
  blender::Span<bNodeTreeInterfaceSocket *> interface_inputs();
  blender::Span<const bNodeTreeInterfaceSocket *> interface_inputs() const;
  blender::Span<bNodeTreeInterfaceSocket *> interface_outputs();
  blender::Span<const bNodeTreeInterfaceSocket *> interface_outputs() const;
  blender::Span<bNodeTreeInterfaceItem *> interface_items();
  blender::Span<const bNodeTreeInterfaceItem *> interface_items() const;

  int interface_input_index(const bNodeTreeInterfaceSocket &io_socket) const;
  int interface_output_index(const bNodeTreeInterfaceSocket &io_socket) const;
  int interface_item_index(const bNodeTreeInterfaceItem &io_item) const;
#endif
};

/* socket value structs for input buttons
 * DEPRECATED now using ID properties
 */

struct bNodeSocketValueInt {
  /** RNA subtype. */
  int subtype = 0;
  int value = 0;
  int min = 0, max = 0;
};

struct bNodeSocketValueFloat {
  /** RNA subtype. */
  int subtype = 0;
  float value = 0;
  float min = 0, max = 0;
};

struct bNodeSocketValueBoolean {
  char value = 0;
};

struct bNodeSocketValueVector {
  /** RNA subtype. */
  int subtype = 0;
  /* Only some of the values might be used depending on the dimensions. */
  float value[4] = {};
  float min = 0, max = 0;
  /* The number of dimensions of the vector. Can be 2, 3, or 4. */
  int dimensions = 0;
};

struct bNodeSocketValueRotation {
  float value_euler[3] = {};
};

struct bNodeSocketValueRGBA {
  float value[4] = {};
};

struct bNodeSocketValueString {
  int subtype = 0;
  char _pad[4] = {};
  char value[/*FILE_MAX*/ 1024] = "";
};

struct bNodeSocketValueObject {
  struct Object *value = nullptr;
};

struct bNodeSocketValueImage {
  struct Image *value = nullptr;
};

struct bNodeSocketValueCollection {
  struct Collection *value = nullptr;
};

struct bNodeSocketValueTexture {
  struct Tex *value = nullptr;
};

struct bNodeSocketValueMaterial {
  struct Material *value = nullptr;
};

struct bNodeSocketValueFont {
  struct VFont *value = nullptr;
};

struct bNodeSocketValueScene {
  struct Scene *value = nullptr;
};

struct bNodeSocketValueText {
  struct Text *value = nullptr;
};

struct bNodeSocketValueMask {
  struct Mask *value = nullptr;
};

struct bNodeSocketValueSound {
  struct bSound *value = nullptr;
};

struct bNodeSocketValueMenu {
  /* Default input enum identifier. */
  int value = 0;
  /* #NodeSocketValueMenuRuntimeFlag */
  int runtime_flag = 0;
  /* Immutable runtime enum definition. */
  const RuntimeNodeEnumItemsHandle *enum_items = nullptr;

#ifdef __cplusplus
  bool has_conflict() const;
#endif
};

struct GeometryNodeAssetTraits {
  int flag = 0;
  char _pad[4] = {};
  char *node_tool_idname = nullptr;
};

struct NodeFrame {
  DNA_DEFINE_CXX_METHODS(NodeFrame)

  short flag = 0;
  short label_size = 0;
};

struct NodeReroute {
  DNA_DEFINE_CXX_METHODS(NodeReroute)

  /** Name of the socket type (e.g. `NodeSocketFloat`). */
  char type_idname[64] = "";
};

/** \note This one has been replaced with #ImageUser, keep it for do_versions(). */
struct NodeImageAnim {
  DNA_DEFINE_CXX_METHODS(NodeImageAnim)

  DNA_DEPRECATED int frames = 0;
  DNA_DEPRECATED int sfra = 0;
  DNA_DEPRECATED int nr = 0;
  DNA_DEPRECATED char cyclic = 0;
  DNA_DEPRECATED char movie = 0;
  char _pad[2] = {};
};

struct ColorCorrectionData {
  DNA_DEFINE_CXX_METHODS(ColorCorrectionData)

  DNA_DEPRECATED float saturation = 0;
  DNA_DEPRECATED float contrast = 0;
  DNA_DEPRECATED float gamma = 0;
  DNA_DEPRECATED float gain = 0;
  DNA_DEPRECATED float lift = 0;
  char _pad[4] = {};
};

struct NodeColorCorrection {
  DNA_DEFINE_CXX_METHODS(NodeColorCorrection)

  DNA_DEPRECATED ColorCorrectionData master;
  DNA_DEPRECATED ColorCorrectionData shadows;
  DNA_DEPRECATED ColorCorrectionData midtones;
  DNA_DEPRECATED ColorCorrectionData highlights;
  DNA_DEPRECATED float startmidtones = 0;
  DNA_DEPRECATED float endmidtones = 0;
};

struct NodeBokehImage {
  DNA_DEFINE_CXX_METHODS(NodeBokehImage)

  DNA_DEPRECATED float angle = 0;
  DNA_DEPRECATED int flaps = 0;
  DNA_DEPRECATED float rounding = 0;
  DNA_DEPRECATED float catadioptric = 0;
  DNA_DEPRECATED float lensshift = 0;
};

struct NodeBoxMask {
  DNA_DEFINE_CXX_METHODS(NodeBoxMask)

  DNA_DEPRECATED float x = 0;
  DNA_DEPRECATED float y = 0;
  DNA_DEPRECATED float rotation = 0;
  DNA_DEPRECATED float height = 0;
  DNA_DEPRECATED float width = 0;
  char _pad[4] = {};
};

struct NodeEllipseMask {
  DNA_DEFINE_CXX_METHODS(NodeEllipseMask)

  DNA_DEPRECATED float x = 0;
  DNA_DEPRECATED float y = 0;
  DNA_DEPRECATED float rotation = 0;
  DNA_DEPRECATED float height = 0;
  DNA_DEPRECATED float width = 0;
  char _pad[4] = {};
};

/** Layer info for image node outputs. */
struct NodeImageLayer {
  DNA_DEFINE_CXX_METHODS(NodeImageLayer)

  /** Index in the `image->layers->passes` lists. */
  DNA_DEPRECATED int pass_index = 0;
  /* render pass name */
  /** Amount defined in IMB_openexr.hh. */
  char pass_name[64] = "";
};

struct NodeBlurData {
  DNA_DEFINE_CXX_METHODS(NodeBlurData)

  DNA_DEPRECATED short sizex = 0;
  DNA_DEPRECATED short sizey = 0;
  DNA_DEPRECATED short samples = 0;
  DNA_DEPRECATED short maxspeed = 0;
  DNA_DEPRECATED short minspeed = 0;
  DNA_DEPRECATED short relative = 0;
  DNA_DEPRECATED short aspect = 0;
  DNA_DEPRECATED short curved = 0;
  DNA_DEPRECATED float fac = 0;
  DNA_DEPRECATED float percentx = 0;
  DNA_DEPRECATED float percenty = 0;
  DNA_DEPRECATED short filtertype = 0; /* CMPNodeBlurType */
  DNA_DEPRECATED char bokeh = 0;
  DNA_DEPRECATED char gamma = 0;
};

struct NodeDBlurData {
  DNA_DEFINE_CXX_METHODS(NodeDBlurData)

  DNA_DEPRECATED float center_x = 0;
  DNA_DEPRECATED float center_y = 0;
  DNA_DEPRECATED float distance = 0;
  DNA_DEPRECATED float angle = 0;
  DNA_DEPRECATED float spin = 0;
  DNA_DEPRECATED float zoom = 0;
  DNA_DEPRECATED short iter = 0;
  char _pad[2] = {};
};

struct NodeBilateralBlurData {
  DNA_DEFINE_CXX_METHODS(NodeBilateralBlurData)

  DNA_DEPRECATED float sigma_color = 0;
  DNA_DEPRECATED float sigma_space = 0;
  DNA_DEPRECATED short iter = 0;
  char _pad[2] = {};
};

struct NodeKuwaharaData {
  DNA_DEFINE_CXX_METHODS(NodeKuwaharaData)

  DNA_DEPRECATED short size = 0;
  DNA_DEPRECATED short variation = 0;
  DNA_DEPRECATED int uniformity = 0;
  DNA_DEPRECATED float sharpness = 0;
  DNA_DEPRECATED float eccentricity = 0;
  DNA_DEPRECATED char high_precision = 0;
  char _pad[3] = {};
};

struct NodeAntiAliasingData {
  DNA_DEFINE_CXX_METHODS(NodeAntiAliasingData)

  DNA_DEPRECATED float threshold = 0;
  DNA_DEPRECATED float contrast_limit = 0;
  DNA_DEPRECATED float corner_rounding = 0;
};

/** \note Only for do-version code. */
struct NodeHueSat {
  DNA_DEFINE_CXX_METHODS(NodeHueSat)

  DNA_DEPRECATED float hue = 0;
  DNA_DEPRECATED float sat = 0;
  DNA_DEPRECATED float val = 0;
};

struct NodeImageFile {
  DNA_DEFINE_CXX_METHODS(NodeImageFile)

  char name[/*FILE_MAX*/ 1024] = "";
  ImageFormatData im_format;
  int sfra = 0, efra = 0;
};

struct NodeCompositorFileOutputItem {
  /* The unique identifier of the item used to construct the socket identifier. */
  int identifier = 0;
  /* The type of socket for the item, which is limited to the types listed in the
   * FileOutputItemsAccessor::supports_socket_type. */
  int16_t socket_type = 0;
  /* The number of dimensions in the vector socket if the socket type is vector, otherwise, it is
   * unused, */
  char vector_socket_dimensions = 0;
  /* If true and the node is saving individual files, the format an save_as_render members of this
   * struct will be used, otherwise, the members of the NodeCompositorFileOutput struct will be
   * used for all items. */
  char override_node_format = 0;
  /* Apply the render part of the display transform when saving non-linear images. Unused if
   * override_node_format is false or the node is saving multi-layer images. */
  char save_as_render = 0;
  char _pad[7] = {};
  /* The unique name of the item. It is used as the file name when saving individual files and used
   * as the layer name when saving multi-layer images. */
  char *name = nullptr;
  /* The image format to use when saving individual images and override_node_format is true. */
  ImageFormatData format;
};

struct NodeCompositorFileOutput {
  DNA_DEFINE_CXX_METHODS(NodeCompositorFileOutput)

  char directory[/*FILE_MAX*/ 1024] = "";
  /* The base name of the file. Can be nullptr. */
  char *file_name = nullptr;
  /* The image format to use when saving the images. */
  ImageFormatData format;
  /* The file output images. They can represent individual images or layers depending on whether
   * multi-layer images are being saved. */
  NodeCompositorFileOutputItem *items = nullptr;
  /* The number of file output items. */
  int items_count = 0;
  /* The currently active file output item. */
  int active_item_index = 0;
  /* Apply the render part of the display transform when saving non-linear images. */
  char save_as_render = 0;
  char _pad[7] = {};
};

struct NodeImageMultiFileSocket {
  DNA_DEFINE_CXX_METHODS(NodeImageMultiFileSocket)

  /* single layer file output */
  DNA_DEPRECATED short use_render_format = 0;
  /** Use overall node image format. */
  DNA_DEPRECATED short use_node_format = 0;
  DNA_DEPRECATED char save_as_render = 0;
  char _pad1[3] = {};
  DNA_DEPRECATED char path[/*FILE_MAX*/ 1024];
  DNA_DEPRECATED ImageFormatData format;

  /* Multi-layer output. */
  /** Subtract 2 because '.' and channel char are appended. */
  DNA_DEPRECATED char layer[/*EXR_TOT_MAXNAME - 2*/ 62];
  char _pad2[2] = {};
};

struct NodeChroma {
  DNA_DEFINE_CXX_METHODS(NodeChroma)

  DNA_DEPRECATED float t1 = 0;
  DNA_DEPRECATED float t2 = 0;
  DNA_DEPRECATED float t3 = 0;
  DNA_DEPRECATED float fsize = 0;
  DNA_DEPRECATED float fstrength = 0;
  DNA_DEPRECATED float falpha = 0;
  DNA_DEPRECATED float key[4] = {};
  DNA_DEPRECATED short algorithm = 0;
  DNA_DEPRECATED short channel = 0;
};

struct NodeTwoXYs {
  DNA_DEFINE_CXX_METHODS(NodeTwoXYs)

  DNA_DEPRECATED short x1 = 0;
  DNA_DEPRECATED short x2 = 0;
  DNA_DEPRECATED short y1 = 0;
  DNA_DEPRECATED short y2 = 0;
  DNA_DEPRECATED float fac_x1 = 0;
  DNA_DEPRECATED float fac_x2 = 0;
  DNA_DEPRECATED float fac_y1 = 0;
  DNA_DEPRECATED float fac_y2 = 0;
};

struct NodeTwoFloats {
  DNA_DEFINE_CXX_METHODS(NodeTwoFloats)

  DNA_DEPRECATED float x = 0;
  DNA_DEPRECATED float y = 0;
};

struct NodeVertexCol {
  DNA_DEFINE_CXX_METHODS(NodeVertexCol)

  char name[64] = "";
};

struct NodeCMPCombSepColor {
  DNA_DEFINE_CXX_METHODS(NodeCMPCombSepColor)

  /* CMPNodeCombSepColorMode */
  uint8_t mode = 0;
  uint8_t ycc_mode = 0;
};

/** Defocus blur node. */
struct NodeDefocus {
  DNA_DEFINE_CXX_METHODS(NodeDefocus)

  char bktype = 0;
  DNA_DEPRECATED char gamco = 0;
  char no_zbuf = 0;
  char _pad0 = {};
  float fstop = 0;
  float maxblur = 0;
  float scale = 0;
  float rotation = 0;
};

struct NodeScriptDict {
  DNA_DEFINE_CXX_METHODS(NodeScriptDict)

  /** For PyObject *dict. */
  void *dict = nullptr;
  /** For BPy_Node *node. */
  void *node = nullptr;
};

/** glare node. */
struct NodeGlare {
  DNA_DEFINE_CXX_METHODS(NodeGlare)

  DNA_DEPRECATED char type = 0;
  DNA_DEPRECATED char quality = 0;
  DNA_DEPRECATED char iter = 0;
  DNA_DEPRECATED char angle = 0;
  char _pad0 = {};
  DNA_DEPRECATED char size = 0;
  DNA_DEPRECATED char star_45 = 0;
  DNA_DEPRECATED char streaks = 0;
  DNA_DEPRECATED float colmod = 0;
  DNA_DEPRECATED float mix = 0;
  DNA_DEPRECATED float threshold = 0;
  DNA_DEPRECATED float fade = 0;
  DNA_DEPRECATED float angle_ofs = 0;
  char _pad1[4] = {};
};

/** Tone-map node. */
struct NodeTonemap {
  DNA_DEFINE_CXX_METHODS(NodeTonemap)

  DNA_DEPRECATED float key = 0;
  DNA_DEPRECATED float offset = 0;
  DNA_DEPRECATED float gamma = 0;
  DNA_DEPRECATED float f = 0;
  DNA_DEPRECATED float m = 0;
  DNA_DEPRECATED float a = 0;
  DNA_DEPRECATED float c = 0;
  DNA_DEPRECATED int type = 0;
};

/* Lens Distortion node. */
struct NodeLensDist {
  DNA_DEFINE_CXX_METHODS(NodeLensDist)

  DNA_DEPRECATED short jit = 0;
  DNA_DEPRECATED short proj = 0;
  DNA_DEPRECATED short fit = 0;
  char _pad[2] = {};
  DNA_DEPRECATED int distortion_type = 0;
};

struct NodeColorBalance {
  DNA_DEFINE_CXX_METHODS(NodeColorBalance)

  /* ASC CDL parameters. */
  DNA_DEPRECATED float slope[3] = {};
  DNA_DEPRECATED float offset[3] = {};
  DNA_DEPRECATED float power[3] = {};
  DNA_DEPRECATED float offset_basis = 0;
  char _pad[4] = {};

  /* LGG parameters. */
  DNA_DEPRECATED float lift[3] = {};
  DNA_DEPRECATED float gamma[3] = {};
  DNA_DEPRECATED float gain[3] = {};

  /* White-point parameters. */
  DNA_DEPRECATED float input_temperature = 0;
  DNA_DEPRECATED float input_tint = 0;
  DNA_DEPRECATED float output_temperature = 0;
  DNA_DEPRECATED float output_tint = 0;
};

struct NodeColorspill {
  DNA_DEFINE_CXX_METHODS(NodeColorspill)

  DNA_DEPRECATED short limchan = 0;
  DNA_DEPRECATED short unspill = 0;
  DNA_DEPRECATED float limscale = 0;
  DNA_DEPRECATED float uspillr = 0;
  DNA_DEPRECATED float uspillg = 0;
  DNA_DEPRECATED float uspillb = 0;
};

struct NodeConvertColorSpace {
  DNA_DEFINE_CXX_METHODS(NodeConvertColorSpace)

  char from_color_space[64] = "";
  char to_color_space[64] = "";
};

struct NodeConvertToDisplay {
  DNA_DEFINE_CXX_METHODS(NodeConvertToDisplay)

  ColorManagedDisplaySettings display_settings;
  ColorManagedViewSettings view_settings;
};

struct NodeDilateErode {
  DNA_DEFINE_CXX_METHODS(NodeDilateErode)

  char falloff = 0;
};

struct NodeMask {
  DNA_DEFINE_CXX_METHODS(NodeMask)

  DNA_DEPRECATED int size_x = 0;
  DNA_DEPRECATED int size_y = 0;
};

struct NodeSetAlpha {
  DNA_DEFINE_CXX_METHODS(NodeSetAlpha)

  DNA_DEPRECATED char mode = 0;
};

struct NodeTexBase {
  TexMapping tex_mapping;
  ColorMapping color_mapping;
};

struct NodeTexSky {
  DNA_DEFINE_CXX_METHODS(NodeTexSky)

  NodeTexBase base;
  int sky_model = 0;
  float sun_direction[3] = {0.0f, 0.0f, 1.0f};
  float turbidity = 0;
  float ground_albedo = 0;
  float sun_size = 0;
  float sun_intensity = 0;
  float sun_elevation = 0;
  float sun_rotation = 0;
  float altitude = 0;
  float air_density = 0;
  float aerosol_density = 0;
  float ozone_density = 0;
  char sun_disc = 0;
  char _pad[7] = {};
};

struct NodeTexImage {
  DNA_DEFINE_CXX_METHODS(NodeTexImage)

  NodeTexBase base;
  ImageUser iuser;
  DNA_DEPRECATED int color_space = 0;
  int projection = 0;
  float projection_blend = 0;
  int interpolation = 0;
  int extension = 0;
  char _pad[4] = {};
};

struct NodeTexChecker {
  DNA_DEFINE_CXX_METHODS(NodeTexChecker)

  NodeTexBase base;
};

struct NodeTexBrick {
  DNA_DEFINE_CXX_METHODS(NodeTexBrick)

  NodeTexBase base;
  int offset_freq = 0, squash_freq = 0;
  float offset = 0, squash = 0;
};

struct NodeTexEnvironment {
  DNA_DEFINE_CXX_METHODS(NodeTexEnvironment)

  NodeTexBase base;
  ImageUser iuser;
  DNA_DEPRECATED int color_space = 0;
  int projection = 0;
  int interpolation = 0;
  char _pad[4] = {};
};

struct NodeTexGabor {
  DNA_DEFINE_CXX_METHODS(NodeTexGabor)

  NodeTexBase base;
  /* Stores NodeGaborType. */
  char type = 0;
  char _pad[7] = {};
};

struct NodeTexGradient {
  DNA_DEFINE_CXX_METHODS(NodeTexGradient)

  NodeTexBase base;
  int gradient_type = 0;
  char _pad[4] = {};
};

struct NodeTexNoise {
  DNA_DEFINE_CXX_METHODS(NodeTexNoise)

  NodeTexBase base;
  int dimensions = 0;
  uint8_t type = 0;
  uint8_t normalize = 0;
  char _pad[2] = {};
};

struct NodeTexVoronoi {
  DNA_DEFINE_CXX_METHODS(NodeTexVoronoi)

  NodeTexBase base;
  int dimensions = 0;
  int feature = 0;
  int distance = 0;
  int normalize = 0;
  DNA_DEPRECATED int coloring = 0;
  char _pad[4] = {};
};

struct NodeTexMusgrave {
  DNA_DEFINE_CXX_METHODS(NodeTexMusgrave)

  DNA_DEPRECATED NodeTexBase base;
  DNA_DEPRECATED int musgrave_type = 0;
  DNA_DEPRECATED int dimensions = 0;
};

struct NodeTexWave {
  DNA_DEFINE_CXX_METHODS(NodeTexWave)

  NodeTexBase base;
  int wave_type = 0;
  int bands_direction = 0;
  int rings_direction = 0;
  int wave_profile = 0;
};

struct NodeTexMagic {
  DNA_DEFINE_CXX_METHODS(NodeTexMagic)

  NodeTexBase base;
  int depth = 0;
  char _pad[4] = {};
};

struct NodeShaderAttribute {
  DNA_DEFINE_CXX_METHODS(NodeShaderAttribute)

  char name[256] = "";
  int type = 0;
  char _pad[4] = {};
};

struct NodeShaderVectTransform {
  DNA_DEFINE_CXX_METHODS(NodeShaderVectTransform)

  int type = 0;
  int convert_from = 0, convert_to = 0;
  char _pad[4] = {};
};

struct NodeShaderPrincipled {
  DNA_DEFINE_CXX_METHODS(NodeShaderPrincipled)

  char use_subsurface_auto_radius = 0;
  char _pad[3] = {};
};

struct NodeShaderHairPrincipled {
  DNA_DEFINE_CXX_METHODS(NodeShaderHairPrincipled)

  short model = 0;
  short parametrization = 0;
  char _pad[4] = {};
};

/** TEX_output. */
struct TexNodeOutput {
  DNA_DEFINE_CXX_METHODS(TexNodeOutput)

  char name[64] = "";
};

struct NodeKeyingScreenData {
  DNA_DEFINE_CXX_METHODS(NodeKeyingScreenData)

  char tracking_object[/*MAX_NAME*/ 64] = "";
  DNA_DEPRECATED float smoothness = 0;
};

struct NodeKeyingData {
  DNA_DEFINE_CXX_METHODS(NodeKeyingData)

  DNA_DEPRECATED float screen_balance = 0;
  DNA_DEPRECATED float despill_factor = 0;
  DNA_DEPRECATED float despill_balance = 0;
  DNA_DEPRECATED int edge_kernel_radius = 0;
  DNA_DEPRECATED float edge_kernel_tolerance = 0;
  DNA_DEPRECATED float clip_black = 0;
  DNA_DEPRECATED float clip_white = 0;
  DNA_DEPRECATED int dilate_distance = 0;
  DNA_DEPRECATED int feather_distance = 0;
  DNA_DEPRECATED int feather_falloff = 0;
  DNA_DEPRECATED int blur_pre = 0;
  DNA_DEPRECATED int blur_post = 0;
};

struct NodeTrackPosData {
  DNA_DEFINE_CXX_METHODS(NodeTrackPosData)

  char tracking_object[/*MAX_NAME*/ 64] = "";
  char track_name[64] = "";
};

struct NodeTransformData {
  DNA_DEFINE_CXX_METHODS(NodeTransformData)

  DNA_DEPRECATED short interpolation = 0;
  DNA_DEPRECATED char extension_x = 0;
  DNA_DEPRECATED char extension_y = 0;
};

struct NodeTranslateData {
  DNA_DEFINE_CXX_METHODS(NodeTranslateData)

  DNA_DEPRECATED char wrap_axis = 0;
  DNA_DEPRECATED char relative = 0;
  DNA_DEPRECATED short extension_x = 0;
  DNA_DEPRECATED short extension_y = 0;
  DNA_DEPRECATED short interpolation = 0;
};

struct NodeRotateData {
  DNA_DEFINE_CXX_METHODS(NodeRotateData)

  DNA_DEPRECATED short interpolation = 0;
  DNA_DEPRECATED char extension_x = 0;
  DNA_DEPRECATED char extension_y = 0;
};

struct NodeScaleData {
  DNA_DEFINE_CXX_METHODS(NodeScaleData)

  DNA_DEPRECATED short interpolation = 0;
  DNA_DEPRECATED char extension_x = 0;
  DNA_DEPRECATED char extension_y = 0;
};

struct NodeCornerPinData {
  DNA_DEFINE_CXX_METHODS(NodeCornerPinData)

  DNA_DEPRECATED short interpolation = 0;
  DNA_DEPRECATED char extension_x = 0;
  DNA_DEPRECATED char extension_y = 0;
};

struct NodeDisplaceData {
  DNA_DEFINE_CXX_METHODS(NodeDisplaceData)

  DNA_DEPRECATED short interpolation = 0;
  DNA_DEPRECATED char extension_x = 0;
  DNA_DEPRECATED char extension_y = 0;
};

struct NodeMapUVData {
  DNA_DEFINE_CXX_METHODS(NodeMapUVData)

  DNA_DEPRECATED short interpolation = 0;
  DNA_DEPRECATED char extension_x = 0;
  DNA_DEPRECATED char extension_y = 0;
};

struct NodePlaneTrackDeformData {
  DNA_DEFINE_CXX_METHODS(NodePlaneTrackDeformData)

  char tracking_object[64] = "";
  char plane_track_name[64] = "";
  DNA_DEPRECATED char flag = 0;
  DNA_DEPRECATED char motion_blur_samples = 0;
  char _pad[2] = {};
  DNA_DEPRECATED float motion_blur_shutter = 0;
};

struct NodeShaderScript {
  DNA_DEFINE_CXX_METHODS(NodeShaderScript)

  int mode = 0;
  int flag = 0;

  char filepath[/*FILE_MAX*/ 1024] = "";

  char bytecode_hash[64] = "";
  char *bytecode = nullptr;
};

struct NodeShaderTangent {
  DNA_DEFINE_CXX_METHODS(NodeShaderTangent)

  int direction_type = 0;
  int axis = 0;
  char uv_map[/*MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX*/ 64] = "";
};

struct NodeShaderNormalMap {
  DNA_DEFINE_CXX_METHODS(NodeShaderNormalMap)

  int space = 0;
  char uv_map[/*MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX*/ 64] = "";
};

struct NodeRadialTiling {
  DNA_DEFINE_CXX_METHODS(NodeRadialTiling)

  uint8_t normalize = 0;
  char _pad[7] = {};
};

struct NodeShaderUVMap {
  DNA_DEFINE_CXX_METHODS(NodeShaderUVMap)

  char uv_map[/*MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX*/ 64] = "";
};

struct NodeShaderVertexColor {
  DNA_DEFINE_CXX_METHODS(NodeShaderVertexColor)

  char layer_name[/*MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX*/ 64] = "";
};

struct NodeShaderTexIES {
  DNA_DEFINE_CXX_METHODS(NodeShaderTexIES)

  int mode = 0;

  char filepath[/*FILE_MAX*/ 1024] = "";
};

struct NodeShaderOutputAOV {
  DNA_DEFINE_CXX_METHODS(NodeShaderOutputAOV)

  char name[/*MAX_NAME*/ 64] = "";
};

struct NodeSunBeams {
  DNA_DEFINE_CXX_METHODS(NodeSunBeams)

  DNA_DEPRECATED float source[2] = {};
  DNA_DEPRECATED float ray_length = 0;
};

struct CryptomatteEntry {
  DNA_DEFINE_CXX_METHODS(CryptomatteEntry)

  struct CryptomatteEntry *next = nullptr, *prev = nullptr;
  float encoded_hash = 0;
  char name[/*MAX_NAME*/ 64] = "";
  char _pad[4] = {};
};

struct CryptomatteLayer {
  DNA_DEFINE_CXX_METHODS(CryptomatteLayer)

  struct CryptomatteEntry *next = nullptr, *prev = nullptr;
  char name[64] = "";
};

struct NodeCryptomatte_Runtime {
  DNA_DEFINE_CXX_METHODS(NodeCryptomatte_Runtime)

  ListBaseT<CryptomatteLayer> layers = {nullptr, nullptr};
  /** Temp storage for the crypto-matte picker. */
  float add[3] = {1.0f, 1.0f, 1.0f};
  float remove[3] = {1.0f, 1.0f, 1.0f};
};

struct NodeCryptomatte {
  DNA_DEFINE_CXX_METHODS(NodeCryptomatte)

  /**
   * `iuser` needs to be first element due to RNA limitations.
   * When we define the #ImageData properties, we can't define them from
   * `storage->iuser`, so storage needs to be cast to #ImageUser directly.
   */
  ImageUser iuser;

  ListBaseT<CryptomatteEntry> entries = {nullptr, nullptr};

  char layer_name[/*MAX_NAME*/ 64] = "";
  /** Stores `entries` as a string for opening in 2.80-2.91. */
  char *matte_id = nullptr;

  /* Legacy attributes. */
  /** Number of input sockets. */
  int inputs_num = 0;

  char _pad[4] = {};
  NodeCryptomatte_Runtime runtime;
};

struct NodeDenoise {
  DNA_DEFINE_CXX_METHODS(NodeDenoise)

  DNA_DEPRECATED char hdr = 0;
  DNA_DEPRECATED char prefilter = 0;
  DNA_DEPRECATED char quality = 0;
  char _pad[1] = {};
};

struct NodeMapRange {
  DNA_DEFINE_CXX_METHODS(NodeMapRange)

  /** #eCustomDataType */
  uint8_t data_type = 0;

  /** #NodeMapRangeType. */
  uint8_t interpolation_type = 0;
  uint8_t clamp = 0;
  char _pad[5] = {};
};

struct NodeRandomValue {
  DNA_DEFINE_CXX_METHODS(NodeRandomValue)

  /** #eCustomDataType. */
  uint8_t data_type = 0;
};

struct NodeAccumulateField {
  DNA_DEFINE_CXX_METHODS(NodeAccumulateField)

  /** #eCustomDataType. */
  uint8_t data_type = 0;
  /** #AttrDomain. */
  uint8_t domain = 0;
};

struct NodeInputBool {
  DNA_DEFINE_CXX_METHODS(NodeInputBool)

  uint8_t boolean = 0;
};

struct NodeInputInt {
  DNA_DEFINE_CXX_METHODS(NodeInputInt)

  int integer = 0;
};

struct NodeInputRotation {
  DNA_DEFINE_CXX_METHODS(NodeInputRotation)

  float rotation_euler[3] = {};
};

struct NodeInputVector {
  DNA_DEFINE_CXX_METHODS(NodeInputVector)

  float vector[3] = {};
};

struct NodeInputColor {
  DNA_DEFINE_CXX_METHODS(NodeInputColor)

  float color[4] = {};
};

struct NodeInputString {
  DNA_DEFINE_CXX_METHODS(NodeInputString)

  char *string = nullptr;
};

struct NodeGeometryExtrudeMesh {
  DNA_DEFINE_CXX_METHODS(NodeGeometryExtrudeMesh)

  /** #GeometryNodeExtrudeMeshMode */
  uint8_t mode = 0;
};

struct NodeGeometryObjectInfo {
  DNA_DEFINE_CXX_METHODS(NodeGeometryObjectInfo)

  /** #GeometryNodeTransformSpace. */
  uint8_t transform_space = 0;
};

struct NodeGeometryPointsToVolume {
  DNA_DEFINE_CXX_METHODS(NodeGeometryPointsToVolume)

  /** #GeometryNodePointsToVolumeResolutionMode */
  uint8_t resolution_mode = 0;
};

struct NodeGeometryCollectionInfo {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCollectionInfo)

  /** #GeometryNodeTransformSpace. */
  uint8_t transform_space = 0;
};

struct NodeGeometryProximity {
  DNA_DEFINE_CXX_METHODS(NodeGeometryProximity)

  /** #GeometryNodeProximityTargetType. */
  uint8_t target_element = 0;
};

struct NodeGeometryVolumeToMesh {
  DNA_DEFINE_CXX_METHODS(NodeGeometryVolumeToMesh)

  /** #VolumeToMeshResolutionMode */
  uint8_t resolution_mode = 0;
};

struct NodeGeometryMeshToVolume {
  DNA_DEFINE_CXX_METHODS(NodeGeometryMeshToVolume)

  /** #MeshToVolumeModifierResolutionMode */
  uint8_t resolution_mode = 0;
};

struct NodeGeometrySubdivisionSurface {
  DNA_DEFINE_CXX_METHODS(NodeGeometrySubdivisionSurface)

  /** #eSubsurfUVSmooth. */
  uint8_t uv_smooth = 0;
  /** #eSubsurfBoundarySmooth. */
  uint8_t boundary_smooth = 0;
};

struct NodeGeometryMeshCircle {
  DNA_DEFINE_CXX_METHODS(NodeGeometryMeshCircle)

  /** #GeometryNodeMeshCircleFillType. */
  uint8_t fill_type = 0;
};

struct NodeGeometryMeshCylinder {
  DNA_DEFINE_CXX_METHODS(NodeGeometryMeshCylinder)

  /** #GeometryNodeMeshCircleFillType. */
  uint8_t fill_type = 0;
};

struct NodeGeometryMeshCone {
  DNA_DEFINE_CXX_METHODS(NodeGeometryMeshCone)

  /** #GeometryNodeMeshCircleFillType. */
  uint8_t fill_type = 0;
};

struct NodeGeometryMergeByDistance {
  DNA_DEFINE_CXX_METHODS(NodeGeometryMergeByDistance)

  /** #GeometryNodeMergeByDistanceMode. */
  uint8_t mode = 0;
};

struct NodeGeometryMeshLine {
  DNA_DEFINE_CXX_METHODS(NodeGeometryMeshLine)

  /** #GeometryNodeMeshLineMode. */
  uint8_t mode = 0;
  /** #GeometryNodeMeshLineCountMode. */
  uint8_t count_mode = 0;
};

struct NodeSwitch {
  DNA_DEFINE_CXX_METHODS(NodeSwitch)

  /** #eNodeSocketDatatype. */
  uint8_t input_type = 0;
};

struct NodeEnumItem {
  char *name = nullptr;
  char *description = nullptr;
  /* Immutable unique identifier. */
  int32_t identifier = 0;
  char _pad[4] = {};
};

struct NodeEnumDefinition {
  DNA_DEFINE_CXX_METHODS(NodeEnumDefinition)

  /* User-defined enum items owned and managed by this node. */
  NodeEnumItem *items_array = nullptr;
  int items_num = 0;
  int active_index = 0;
  uint32_t next_identifier = 0;
  char _pad[4] = {};

#ifdef __cplusplus
  blender::Span<NodeEnumItem> items() const;
  blender::MutableSpan<NodeEnumItem> items();
#endif
};

struct NodeMenuSwitch {
  DNA_DEFINE_CXX_METHODS(NodeMenuSwitch)

  NodeEnumDefinition enum_definition;

  /** #eNodeSocketDatatype. */
  uint8_t data_type = 0;
  char _pad[7] = {};
};

struct NodeGeometryCurveSplineType {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurveSplineType)

  /** #GeometryNodeSplineType. */
  uint8_t spline_type = 0;
};

struct NodeGeometrySetCurveHandlePositions {
  DNA_DEFINE_CXX_METHODS(NodeGeometrySetCurveHandlePositions)

  /** #GeometryNodeCurveHandleMode. */
  uint8_t mode = 0;
};

struct NodeGeometryCurveSetHandles {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurveSetHandles)

  /** #GeometryNodeCurveHandleType. */
  uint8_t handle_type = 0;
  /** #GeometryNodeCurveHandleMode. */
  uint8_t mode = 0;
};

struct NodeGeometryCurveSelectHandles {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurveSelectHandles)

  /** #GeometryNodeCurveHandleType. */
  uint8_t handle_type = 0;
  /** #GeometryNodeCurveHandleMode. */
  uint8_t mode = 0;
};

struct NodeGeometryCurvePrimitiveArc {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurvePrimitiveArc)

  /** #GeometryNodeCurvePrimitiveArcMode. */
  uint8_t mode = 0;
};

struct NodeGeometryCurvePrimitiveLine {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurvePrimitiveLine)

  /** #GeometryNodeCurvePrimitiveLineMode. */
  uint8_t mode = 0;
};

struct NodeGeometryCurvePrimitiveBezierSegment {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurvePrimitiveBezierSegment)

  /** #GeometryNodeCurvePrimitiveBezierSegmentMode. */
  uint8_t mode = 0;
};

struct NodeGeometryCurvePrimitiveCircle {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurvePrimitiveCircle)

  /** #GeometryNodeCurvePrimitiveMode. */
  uint8_t mode = 0;
};

struct NodeGeometryCurvePrimitiveQuad {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurvePrimitiveQuad)

  /** #GeometryNodeCurvePrimitiveQuadMode. */
  uint8_t mode = 0;
};

struct NodeGeometryCurveResample {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurveResample)

  /** #GeometryNodeCurveResampleMode. */
  uint8_t mode = 0;
  /**
   * If false, curves may be collapsed to a single point. This is unexpected and is only supported
   * for compatibility reasons (#102598).
   */
  uint8_t keep_last_segment = 0;
};

struct NodeGeometryCurveFillet {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurveFillet)

  /** #GeometryNodeCurveFilletMode. */
  uint8_t mode = 0;
};

struct NodeGeometryCurveTrim {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurveTrim)

  /** #GeometryNodeCurveSampleMode. */
  uint8_t mode = 0;
};

struct NodeGeometryCurveToPoints {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurveToPoints)

  /** #GeometryNodeCurveResampleMode. */
  uint8_t mode = 0;
};

struct NodeGeometryCurveSample {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurveSample)

  /** #GeometryNodeCurveSampleMode. */
  uint8_t mode = 0;
  int8_t use_all_curves = 0;
  /** #eCustomDataType. */
  int8_t data_type = 0;
  char _pad[1] = {};
};

struct NodeGeometryTransferAttribute {
  DNA_DEFINE_CXX_METHODS(NodeGeometryTransferAttribute)

  /** #eCustomDataType. */
  int8_t data_type = 0;
  /** #AttrDomain. */
  int8_t domain = 0;
  /** #GeometryNodeAttributeTransferMode. */
  uint8_t mode = 0;
  char _pad[1] = {};
};

struct NodeGeometrySampleIndex {
  DNA_DEFINE_CXX_METHODS(NodeGeometrySampleIndex)

  /** #eCustomDataType. */
  int8_t data_type = 0;
  /** #AttrDomain. */
  int8_t domain = 0;
  int8_t clamp = 0;
  char _pad[1] = {};
};

struct NodeGeometryRaycast {
  DNA_DEFINE_CXX_METHODS(NodeGeometryRaycast)

  /** #GeometryNodeRaycastMapMode. */
  uint8_t mapping = 0;

  /** #eCustomDataType. */
  int8_t data_type = 0;
};

struct NodeGeometryCurveFill {
  DNA_DEFINE_CXX_METHODS(NodeGeometryCurveFill)

  uint8_t mode = 0;
};

struct NodeGeometryMeshToPoints {
  DNA_DEFINE_CXX_METHODS(NodeGeometryMeshToPoints)

  /** #GeometryNodeMeshToPointsMode */
  uint8_t mode = 0;
};

struct NodeGeometryAttributeCaptureItem {
  /** #eCustomDataType. */
  int8_t data_type = 0;
  char _pad[3] = {};
  /**
   * If the identifier is zero, the item supports forward-compatibility with older versions of
   * Blender when it was only possible to capture a single attribute at a time.
   */
  int identifier = 0;
  char *name = nullptr;
};

struct NodeGeometryAttributeCapture {
  DNA_DEFINE_CXX_METHODS(NodeGeometryAttributeCapture)

  /** #eCustomDataType. */
  int8_t data_type_legacy = 0;
  /** #AttrDomain. */
  int8_t domain = 0;
  char _pad[2] = {};
  int next_identifier = 0;
  NodeGeometryAttributeCaptureItem *capture_items = nullptr;
  int capture_items_num = 0;
  int active_index = 0;
};

struct NodeGeometryStoreNamedAttribute {
  DNA_DEFINE_CXX_METHODS(NodeGeometryStoreNamedAttribute)

  /** #eCustomDataType. */
  int8_t data_type = 0;
  /** #AttrDomain. */
  int8_t domain = 0;
};

struct NodeGeometryInputNamedAttribute {
  DNA_DEFINE_CXX_METHODS(NodeGeometryInputNamedAttribute)

  /** #eCustomDataType. */
  int8_t data_type = 0;
};

struct NodeGeometryStringToCurves {
  DNA_DEFINE_CXX_METHODS(NodeGeometryStringToCurves)

  /** #GeometryNodeStringToCurvesOverflowMode */
  uint8_t overflow = 0;
  /** #GeometryNodeStringToCurvesAlignXMode */
  uint8_t align_x = 0;
  /** #GeometryNodeStringToCurvesAlignYMode */
  uint8_t align_y = 0;
  /** #GeometryNodeStringToCurvesPivotMode */
  uint8_t pivot_mode = 0;
};

struct NodeGeometryDeleteGeometry {
  DNA_DEFINE_CXX_METHODS(NodeGeometryDeleteGeometry)

  /** #AttrDomain. */
  int8_t domain = 0;
  /** #GeometryNodeDeleteGeometryMode. */
  int8_t mode = 0;
};

struct NodeGeometryDuplicateElements {
  DNA_DEFINE_CXX_METHODS(NodeGeometryDuplicateElements)

  /** #AttrDomain. */
  int8_t domain = 0;
};

struct NodeGeometryMergeLayers {
  DNA_DEFINE_CXX_METHODS(NodeGeometryMergeLayers)

  /** #MergeLayerMode. */
  int8_t mode = 0;
};

struct NodeGeometrySeparateGeometry {
  DNA_DEFINE_CXX_METHODS(NodeGeometrySeparateGeometry)

  /** #AttrDomain. */
  int8_t domain = 0;
};

struct NodeGeometryImageTexture {
  DNA_DEFINE_CXX_METHODS(NodeGeometryImageTexture)

  int8_t interpolation = 0;
  int8_t extension = 0;
};

struct NodeGeometryViewerItem {
  char *name = nullptr;
  /** #eNodeSocketDatatype. */
  short socket_type = 0;
  uint8_t flag = 0;
  char _pad[1] = {};
  /**
   * Generated unique identifier for sockets which stays the same even when the item order or
   * names change.
   */
  int identifier = 0;
};

struct NodeGeometryViewer {
  DNA_DEFINE_CXX_METHODS(NodeGeometryViewer)

  NodeGeometryViewerItem *items = nullptr;
  int items_num = 0;
  int active_index = 0;
  int next_identifier = 0;

  /** #eCustomDataType. */
  int8_t data_type_legacy = 0;
  /** #AttrDomain. */
  int8_t domain = 0;

  char _pad[2] = {};
};

struct NodeGeometryUVUnwrap {
  DNA_DEFINE_CXX_METHODS(NodeGeometryUVUnwrap)

  /** #GeometryNodeUVUnwrapMethod. */
  uint8_t method = 0;
};

struct NodeSimulationItem {
  char *name = nullptr;
  /** #eNodeSocketDatatype. */
  short socket_type = 0;
  /** #AttrDomain. */
  short attribute_domain = 0;
  /**
   * Generates unique identifier for sockets which stays the same even when the item order or
   * names change.
   */
  int identifier = 0;
};

struct NodeGeometrySimulationInput {
  DNA_DEFINE_CXX_METHODS(NodeGeometrySimulationInput)

  /** bNode.identifier of the corresponding output node. */
  int32_t output_node_id = 0;
};

struct NodeGeometrySimulationOutput {
  DNA_DEFINE_CXX_METHODS(NodeGeometrySimulationOutput)

  NodeSimulationItem *items = nullptr;
  int items_num = 0;
  int active_index = 0;
  /** Number to give unique IDs to state items. */
  int next_identifier = 0;
  int _pad = {};

#ifdef __cplusplus
  blender::Span<NodeSimulationItem> items_span() const;
  blender::MutableSpan<NodeSimulationItem> items_span();
#endif
};

struct NodeRepeatItem {
  char *name = nullptr;
  /** #eNodeSocketDatatype. */
  short socket_type = 0;
  char _pad[2] = {};
  /**
   * Generated unique identifier for sockets which stays the same even when the item order or
   * names change.
   */
  int identifier = 0;
};

struct NodeGeometryRepeatInput {
  DNA_DEFINE_CXX_METHODS(NodeGeometryRepeatInput)

  /** bNode.identifier of the corresponding output node. */
  int32_t output_node_id = 0;
};

struct NodeGeometryRepeatOutput {
  DNA_DEFINE_CXX_METHODS(NodeGeometryRepeatOutput)

  NodeRepeatItem *items = nullptr;
  int items_num = 0;
  int active_index = 0;
  /** Identifier to give to the next repeat item. */
  int next_identifier = 0;
  int inspection_index = 0;

#ifdef __cplusplus
  blender::Span<NodeRepeatItem> items_span() const;
  blender::MutableSpan<NodeRepeatItem> items_span();
#endif
};

struct NodeGeometryForeachGeometryElementInput {
  DNA_DEFINE_CXX_METHODS(NodeGeometryForeachGeometryElementInput)

  /** bNode.identifier of the corresponding output node. */
  int32_t output_node_id = 0;
};

struct NodeForeachGeometryElementInputItem {
  char *name = nullptr;
  /** #eNodeSocketDatatype. */
  short socket_type = 0;
  char _pad[2] = {};
  /** Generated identifier that stays the same even when the name or order changes. */
  int identifier = 0;
};

struct NodeForeachGeometryElementMainItem {
  char *name = nullptr;
  /** #eNodeSocketDatatype. */
  short socket_type = 0;
  char _pad[2] = {};
  /** Generated identifier that stays the same even when the name or order changes. */
  int identifier = 0;
};

struct NodeForeachGeometryElementGenerationItem {
  char *name = nullptr;
  /** #eNodeSocketDatatype. */
  short socket_type = 0;
  /** #AttrDomain. */
  uint8_t domain = 0;
  char _pad[1] = {};
  /** Generated identifier that stays the same even when the name or order changes. */
  int identifier = 0;
};

struct NodeForeachGeometryElementInputItems {
  DNA_DEFINE_CXX_METHODS(NodeForeachGeometryElementInputItems)

  NodeForeachGeometryElementInputItem *items = nullptr;
  int items_num = 0;
  int active_index = 0;
  int next_identifier = 0;
  char _pad[4] = {};
};

struct NodeForeachGeometryElementMainItems {
  DNA_DEFINE_CXX_METHODS(NodeForeachGeometryElementMainItems)

  NodeForeachGeometryElementMainItem *items = nullptr;
  int items_num = 0;
  int active_index = 0;
  int next_identifier = 0;
  char _pad[4] = {};
};

struct NodeForeachGeometryElementGenerationItems {
  DNA_DEFINE_CXX_METHODS(NodeForeachGeometryElementGenerationItems)

  NodeForeachGeometryElementGenerationItem *items = nullptr;
  int items_num = 0;
  int active_index = 0;
  int next_identifier = 0;
  char _pad[4] = {};
};

struct NodeGeometryForeachGeometryElementOutput {
  DNA_DEFINE_CXX_METHODS(NodeGeometryForeachGeometryElementOutput)

  /**
   * The `foreach` zone has three sets of dynamic sockets.
   * One on the input node and two on the output node.
   * All settings are stored centrally in the output node storage though.
   */
  NodeForeachGeometryElementInputItems input_items;
  NodeForeachGeometryElementMainItems main_items;
  NodeForeachGeometryElementGenerationItems generation_items;
  /** This index is used when displaying socket values or using the viewer node. */
  int inspection_index = 0;
  /** #AttrDomain. This is the domain that is iterated over. */
  uint8_t domain = 0;
  char _pad[3] = {};
};

struct NodeClosureInput {
  DNA_DEFINE_CXX_METHODS(NodeClosureInput)

  /** bNode.identifier of the corresponding output node. */
  int32_t output_node_id = 0;
};

struct NodeClosureInputItem {
  char *name = nullptr;
  /** #eNodeSocketDatatype. */
  short socket_type = 0;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type = 0;
  char _pad[1] = {};
  int identifier = 0;
};

struct NodeClosureOutputItem {
  char *name = nullptr;
  /** #eNodeSocketDatatype. */
  short socket_type = 0;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type = 0;
  char _pad[1] = {};
  int identifier = 0;
};

struct NodeClosureInputItems {
  DNA_DEFINE_CXX_METHODS(NodeClosureInputItems)

  NodeClosureInputItem *items = nullptr;
  int items_num = 0;
  int active_index = 0;
  int next_identifier = 0;
  char _pad[4] = {};
};

struct NodeClosureOutputItems {
  DNA_DEFINE_CXX_METHODS(NodeClosureOutputItems)

  NodeClosureOutputItem *items = nullptr;
  int items_num = 0;
  int active_index = 0;
  int next_identifier = 0;
  char _pad[4] = {};
};

struct NodeClosureOutput {
  DNA_DEFINE_CXX_METHODS(NodeClosureOutput)

  NodeClosureInputItems input_items;
  NodeClosureOutputItems output_items;
  /** #NodeClosureFlag. */
  uint8_t flag = 0;
  char _pad[7] = {};
};

struct NodeEvaluateClosureInputItem {
  char *name = nullptr;
  /** #eNodeSocketDatatype */
  short socket_type = 0;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type = 0;
  char _pad[1] = {};
  int identifier = 0;
};

struct NodeEvaluateClosureOutputItem {
  char *name = nullptr;
  /** #eNodeSocketDatatype */
  short socket_type = 0;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type = 0;
  char _pad[1] = {};
  int identifier = 0;
};

struct NodeEvaluateClosureInputItems {
  DNA_DEFINE_CXX_METHODS(NodeEvaluateClosureInputItems)

  NodeEvaluateClosureInputItem *items = nullptr;
  int items_num = 0;
  int active_index = 0;
  int next_identifier = 0;
  char _pad[4] = {};
};

struct NodeEvaluateClosureOutputItems {
  DNA_DEFINE_CXX_METHODS(NodeEvaluateClosureOutputItems)

  NodeEvaluateClosureOutputItem *items = nullptr;
  int items_num = 0;
  int active_index = 0;
  int next_identifier = 0;
  char _pad[4] = {};
};

struct NodeEvaluateClosure {
  DNA_DEFINE_CXX_METHODS(NodeEvaluateClosure)

  NodeEvaluateClosureInputItems input_items;
  NodeEvaluateClosureOutputItems output_items;
  /** #NodeEvaluateClosureFlag. */
  uint8_t flag = 0;
  char _pad[7] = {};
};

struct IndexSwitchItem {
  /** Generated unique identifier which stays the same even when the item order or names change. */
  int identifier = 0;
};

struct NodeIndexSwitch {
  DNA_DEFINE_CXX_METHODS(NodeIndexSwitch)

  IndexSwitchItem *items = nullptr;
  int items_num = 0;

  /* #eNodeSocketDataType. */
  int data_type = 0;
  /** Identifier to give to the next item. */
  int next_identifier = 0;

  char _pad[4] = {};
#ifdef __cplusplus
  blender::Span<IndexSwitchItem> items_span() const;
  blender::MutableSpan<IndexSwitchItem> items_span();
#endif
};

struct GeometryNodeFieldToGridItem {
  /** #eNodeSocketDatatype. */
  int8_t data_type = 0;
  char _pad[3] = {};
  int identifier = 0;
  char *name = nullptr;
};

struct GeometryNodeFieldToGrid {
  DNA_DEFINE_CXX_METHODS(GeometryNodeFieldToGrid)

  /** #eNodeSocketDatatype. */
  int8_t data_type = 0;
  char _pad[3] = {};
  int next_identifier = 0;
  GeometryNodeFieldToGridItem *items = nullptr;
  int items_num = 0;
  int active_index = 0;
};

struct NodeGeometryDistributePointsInVolume {
  DNA_DEFINE_CXX_METHODS(NodeGeometryDistributePointsInVolume)

  /** #GeometryNodePointDistributeVolumeMode. */
  uint8_t mode = 0;
};

struct NodeFunctionCompare {
  DNA_DEFINE_CXX_METHODS(NodeFunctionCompare)

  /** #NodeCompareOperation */
  int8_t operation = 0;
  /** #eNodeSocketDatatype */
  int8_t data_type = 0;
  /** #NodeCompareMode */
  int8_t mode = 0;
  char _pad[1] = {};
};

struct NodeCombSepColor {
  DNA_DEFINE_CXX_METHODS(NodeCombSepColor)

  /** #NodeCombSepColorMode */
  int8_t mode = 0;
};

struct NodeShaderMix {
  DNA_DEFINE_CXX_METHODS(NodeShaderMix)

  /** #eNodeSocketDatatype */
  int8_t data_type = 0;
  /** #NodeShaderMixMode */
  int8_t factor_mode = 0;
  int8_t clamp_factor = 0;
  int8_t clamp_result = 0;
  int8_t blend_type = 0;
  char _pad[3] = {};
};

struct NodeGeometryLinearGizmo {
  DNA_DEFINE_CXX_METHODS(NodeGeometryLinearGizmo)

  /** #GeometryNodeGizmoColor. */
  int color_id = 0;
  /** #GeometryNodeLinearGizmoDrawStyle. */
  int draw_style = 0;
};

struct NodeGeometryDialGizmo {
  DNA_DEFINE_CXX_METHODS(NodeGeometryDialGizmo)

  /** #GeometryNodeGizmoColor. */
  int color_id = 0;
};

struct NodeGeometryTransformGizmo {
  DNA_DEFINE_CXX_METHODS(NodeGeometryTransformGizmo)

  /** #NodeGeometryTransformGizmoFlag. */
  uint32_t flag = 0;
};

struct NodeGeometryBakeItem {
  char *name = nullptr;
  int16_t socket_type = 0;
  int16_t attribute_domain = 0;
  int identifier = 0;
  int32_t flag = 0;
  char _pad[4] = {};
};

struct NodeGeometryBake {
  DNA_DEFINE_CXX_METHODS(NodeGeometryBake)

  NodeGeometryBakeItem *items = nullptr;
  int items_num = 0;
  int next_identifier = 0;
  int active_index = 0;
  char _pad[4] = {};
};

struct NodeCombineBundleItem {
  char *name = nullptr;
  int identifier = 0;
  int16_t socket_type = 0;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type = 0;
  char _pad[1] = {};
};

struct NodeCombineBundle {
  DNA_DEFINE_CXX_METHODS(NodeCombineBundle)

  NodeCombineBundleItem *items = nullptr;
  int items_num = 0;
  int next_identifier = 0;
  int active_index = 0;
  /** #NodeCombineBundleFlag. */
  uint8_t flag = 0;
  char _pad[3] = {};
};

struct NodeSeparateBundleItem {
  char *name = nullptr;
  int identifier = 0;
  int16_t socket_type = 0;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type = 0;
  char _pad[1] = {};
};

struct NodeSeparateBundle {
  DNA_DEFINE_CXX_METHODS(NodeSeparateBundle)

  NodeSeparateBundleItem *items = nullptr;
  int items_num = 0;
  int next_identifier = 0;
  int active_index = 0;
  /** #NodeSeparateBundleFlag. */
  uint8_t flag = 0;
  char _pad[3] = {};
};

struct NodeFunctionFormatStringItem {
  char *name = nullptr;
  int identifier = 0;
  int16_t socket_type = 0;
  char _pad[2] = {};
};

struct NodeFunctionFormatString {
  DNA_DEFINE_CXX_METHODS(NodeFunctionFormatString)

  NodeFunctionFormatStringItem *items = nullptr;
  int items_num = 0;
  int next_identifier = 0;
  int active_index = 0;
  char _pad[4] = {};
};
