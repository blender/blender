/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_scene_types.h" /* for #ImageFormatData */
#include "DNA_vec_types.h"   /* for #rctf */

/** Workaround to forward-declare C++ type in C header. */
#ifdef __cplusplus
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
using NodeDeclarationHandle = blender::nodes::NodeDeclaration;
using SocketDeclarationHandle = blender::nodes::SocketDeclaration;
using bNodeTreeRuntimeHandle = blender::bke::bNodeTreeRuntime;
using bNodeRuntimeHandle = blender::bke::bNodeRuntime;
using bNodeSocketRuntimeHandle = blender::bke::bNodeSocketRuntime;
#else
typedef struct NodeDeclarationHandle NodeDeclarationHandle;
typedef struct SocketDeclarationHandle SocketDeclarationHandle;
typedef struct bNodeTreeRuntimeHandle bNodeTreeRuntimeHandle;
typedef struct bNodeRuntimeHandle bNodeRuntimeHandle;
typedef struct bNodeSocketRuntimeHandle bNodeSocketRuntimeHandle;
#endif

struct AnimData;
struct Collection;
struct ID;
struct Image;
struct ListBase;
struct Material;
struct PreviewImage;
struct Tex;
struct bGPdata;
struct bNodeInstanceHash;
struct bNodeLink;
struct bNodePreview;
struct bNodeType;
struct bNode;

#define NODE_MAXSTR 64

typedef struct bNodeStack {
  float vec[4];
  float min, max;
  void *data;
  /** When input has link, tagged before executing. */
  short hasinput;
  /** When output is linked, tagged before executing. */
  short hasoutput;
  /** Type of data pointer. */
  short datatype;
  /** Type of socket stack comes from, to remap linking different sockets. */
  short sockettype;
  /** Data is a copy of external data (no freeing). */
  short is_copy;
  /** Data is used by external nodes (no freeing). */
  short external;
  char _pad[4];
} bNodeStack;

/** #bNodeStack.datatype (shade-tree only). */
#define NS_OSA_VECTORS 1
#define NS_OSA_VALUES 2

/* node socket/node socket type -b conversion rules */
#define NS_CR_CENTER 0
#define NS_CR_NONE 1
#define NS_CR_FIT_WIDTH 2
#define NS_CR_FIT_HEIGHT 3
#define NS_CR_FIT 4
#define NS_CR_STRETCH 5

typedef struct bNodeSocket {
  struct bNodeSocket *next, *prev;

  /** User-defined properties. */
  IDProperty *prop;

  /** Unique identifier for mapping. */
  char identifier[64];

  /** MAX_NAME. */
  char name[64];

  /** Only used for the Image and OutputFile nodes, should be removed at some point. */
  void *storage;

  /**
   * The socket's data type. #eNodeSocketDatatype.
   */
  short type;
  /** #eNodeSocketFlag */
  short flag;
  /**
   * Maximum number of links that can connect to the socket. Read via #nodeSocketLinkLimit, because
   * the limit might be defined on the socket type, in which case this value does not have any
   * effect. It is necessary to store this in the socket because it is exposed as an RNA property
   * for custom nodes.
   */
  short limit;
  /** Input/output type. */
  short in_out;
  /** Runtime type information. */
  struct bNodeSocketType *typeinfo;
  /** Runtime type identifier. */
  char idname[64];

  /** Default input value used for unlinked sockets. */
  void *default_value;

  /** Local stack index for "node_exec". */
  short stack_index;
  char display_shape;

  /* #eAttrDomain used when the geometry nodes modifier creates an attribute for a group
   * output. */
  char attribute_domain;

  char _pad[4];

  /** Custom dynamic defined label, MAX_NAME. */
  char label[64];
  char description[64];

  /**
   * The default attribute name to use for geometry nodes modifier output attribute sockets.
   * \note Storing this pointer in every single socket exposes the bad design of using sockets
   * to describe group inputs and outputs. In the future, it should be stored in socket
   * declarations.
   */
  char *default_attribute_name;

  /* internal data to retrieve relations and groups
   * DEPRECATED, now uses the generic identifier string instead
   */
  /** Group socket identifiers, to find matching pairs after reading files. */
  int own_index DNA_DEPRECATED;
  /* XXX deprecated, only used for restoring old group node links */
  int to_index DNA_DEPRECATED;

  /** A link pointer, set in #BKE_ntree_update_main. */
  struct bNodeLink *link;

  /* XXX deprecated, socket input values are stored in default_value now.
   * kept for forward compatibility */
  /** Custom data for inputs, only UI writes in this. */
  bNodeStack ns DNA_DEPRECATED;

  bNodeSocketRuntimeHandle *runtime;

#ifdef __cplusplus
  bool is_hidden() const;
  bool is_available() const;
  bool is_visible() const;
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
} bNodeSocket;

/** #bNodeSocket.type & #bNodeSocketType.type */
typedef enum eNodeSocketDatatype {
  SOCK_CUSTOM = -1, /* socket has no integer type */
  SOCK_FLOAT = 0,
  SOCK_VECTOR = 1,
  SOCK_RGBA = 2,
  SOCK_SHADER = 3,
  SOCK_BOOLEAN = 4,
  __SOCK_MESH = 5, /* deprecated */
  SOCK_INT = 6,
  SOCK_STRING = 7,
  SOCK_OBJECT = 8,
  SOCK_IMAGE = 9,
  SOCK_GEOMETRY = 10,
  SOCK_COLLECTION = 11,
  SOCK_TEXTURE = 12,
  SOCK_MATERIAL = 13,
} eNodeSocketDatatype;

/** Socket shape. */
typedef enum eNodeSocketDisplayShape {
  SOCK_DISPLAY_SHAPE_CIRCLE = 0,
  SOCK_DISPLAY_SHAPE_SQUARE = 1,
  SOCK_DISPLAY_SHAPE_DIAMOND = 2,
  SOCK_DISPLAY_SHAPE_CIRCLE_DOT = 3,
  SOCK_DISPLAY_SHAPE_SQUARE_DOT = 4,
  SOCK_DISPLAY_SHAPE_DIAMOND_DOT = 5,
} eNodeSocketDisplayShape;

/** Socket side (input/output). */
typedef enum eNodeSocketInOut {
  SOCK_IN = 1 << 0,
  SOCK_OUT = 1 << 1,
} eNodeSocketInOut;
ENUM_OPERATORS(eNodeSocketInOut, SOCK_OUT);

/** #bNodeSocket.flag, first bit is selection. */
typedef enum eNodeSocketFlag {
  /** Hidden is user defined, to hide unused sockets. */
  SOCK_HIDDEN = (1 << 1),
  /** For quick check if socket is linked. */
  SOCK_IS_LINKED = (1 << 2),
  /** Unavailable is for dynamic sockets. */
  SOCK_UNAVAIL = (1 << 3),
  // /** DEPRECATED  dynamic socket (can be modified by user) */
  // SOCK_DYNAMIC = (1 << 4),
  // /** DEPRECATED  group socket should not be exposed */
  // SOCK_INTERNAL = (1 << 5),
  /** Socket collapsed in UI. */
  SOCK_COLLAPSED = (1 << 6),
  /** Hide socket value, if it gets auto default. */
  SOCK_HIDE_VALUE = (1 << 7),
  /** Socket hidden automatically, to distinguish from manually hidden. */
  SOCK_AUTO_HIDDEN__DEPRECATED = (1 << 8),
  SOCK_NO_INTERNAL_LINK = (1 << 9),
  /** Draw socket in a more compact form. */
  SOCK_COMPACT = (1 << 10),
  /** Make the input socket accept multiple incoming links in the UI. */
  SOCK_MULTI_INPUT = (1 << 11),
  /**
   * Don't show the socket's label in the interface, for situations where the
   * type is obvious and the name takes up too much space.
   */
  SOCK_HIDE_LABEL = (1 << 12),
  /**
   * Only used for geometry nodes. Don't show the socket value in the modifier interface.
   */
  SOCK_HIDE_IN_MODIFIER = (1 << 13),
} eNodeSocketFlag;

typedef struct bNode {
  struct bNode *next, *prev;

  /* Input and output #bNodeSocket. */
  ListBase inputs, outputs;

  /** The node's name for unique identification and string lookup. MAX_NAME. */
  char name[64];

  /**
   * A value that uniquely identifies a node in a node tree even when the name changes.
   * This also allows referencing nodes more efficiently than with strings.
   *
   * Must be set whenever a node is added to a tree, besides a simple tree copy.
   * Must always be positive.
   */
  int32_t identifier;

  int flag;

  /**
   * String identifier of the type like "FunctionNodeCompare". Stored in files to allow retrieving
   * the node type for node types including custom nodes defined in Python by addons.
   */
  char idname[64];

  /** Type information retrieved from the #idname. TODO: Move to runtime data. */
  struct bNodeType *typeinfo;

  /**
   * Integer type used for builtin nodes, allowing cheaper lookup and changing ID names with
   * versioning code. Avoid using directly if possible, since may not match runtime node type if it
   * wasn't found.
   */
  int16_t type;

  char _pad1[2];

  /** Used for some builtin nodes that store properties but don't have a storage struct . */
  int16_t custom1, custom2;
  float custom3, custom4;

  /**
   * Optional link to libdata.
   *
   * \see #bNodeType::initfunc & #bNodeType::freefunc for details on ID user-count.
   */
  struct ID *id;

  /** Custom data struct for node properties for storage in files. */
  void *storage;

  /**
   * Custom properties often defined by addons to store arbitrary data on nodes. A non-builtin
   * equivalent to #storage.
   */
  IDProperty *prop;

  /** Parent node (for frame nodes). */
  struct bNode *parent;

  /** Root location in the node canvas (in parent space). */
  float locx, locy;
  /**
   * Custom width and height controlled by users. Height is calculate automatically for most
   * nodes.
   */
  float width, height;
  /** Additional offset from loc. TODO: Redundant with #locx and #locy, remove/deprecate. */
  float offsetx, offsety;

  /** Custom user-defined label, MAX_NAME. */
  char label[64];

  /** Custom user-defined color. */
  float color[3];

  char _pad2[4];

  bNodeRuntimeHandle *runtime;

#ifdef __cplusplus
  /** The index in the owner node tree. */
  int index() const;
  blender::StringRefNull label_or_name() const;
  bool is_muted() const;
  bool is_reroute() const;
  bool is_frame() const;
  bool is_group() const;
  bool is_group_input() const;
  bool is_group_output() const;
  const blender::nodes::NodeDeclaration *declaration() const;
  /** A span containing all internal links when the node is muted. */
  blender::Span<bNodeLink> internal_links() const;

  /* The following methods are only available when #bNodeTree.ensure_topology_cache has been
   * called. */

  /** A span containing all input sockets of the node (including unavailable sockets). */
  blender::Span<bNodeSocket *> input_sockets();
  blender::Span<const bNodeSocket *> input_sockets() const;
  /** A span containing all output sockets of the node (including unavailable sockets). */
  blender::Span<bNodeSocket *> output_sockets();
  blender::Span<const bNodeSocket *> output_sockets() const;
  /** Utility to get an input socket by its index. */
  bNodeSocket &input_socket(int index);
  const bNodeSocket &input_socket(int index) const;
  /** Utility to get an output socket by its index. */
  bNodeSocket &output_socket(int index);
  const bNodeSocket &output_socket(int index) const;
  /** Lookup socket of this node by its identifier. */
  const bNodeSocket &input_by_identifier(blender::StringRef identifier) const;
  const bNodeSocket &output_by_identifier(blender::StringRef identifier) const;
  bNodeSocket &input_by_identifier(blender::StringRef identifier);
  bNodeSocket &output_by_identifier(blender::StringRef identifier);
  /** If node is frame, will return all children nodes. */
  blender::Span<bNode *> direct_children_in_frame() const;
  /** Node tree this node belongs to. */
  const bNodeTree &owner_tree() const;
#endif
} bNode;

/* node->flag */
#define NODE_SELECT 1
#define NODE_OPTIONS 2
#define NODE_PREVIEW 4
#define NODE_HIDDEN 8
#define NODE_ACTIVE 16
// #define NODE_ACTIVE_ID 32 /* deprecated */
/* Used to indicate which group output node is used and which viewer node is active. */
#define NODE_DO_OUTPUT 64
#define __NODE_GROUP_EDIT 128 /* DEPRECATED */
/* free test flag, undefined */
#define NODE_TEST 256
/* node is disabled */
#define NODE_MUTED 512
// #define NODE_CUSTOM_NAME 1024    /* deprecated! */
// #define NODE_CONST_OUTPUT (1 << 11) /* deprecated */
/* node is always behind others */
#define NODE_BACKGROUND (1 << 12)
/* automatic flag for nodes included in transforms */
// #define NODE_TRANSFORM (1 << 13) /* deprecated */
/* node is active texture */

/* NOTE: take care with this flag since its possible it gets
 * `stuck` inside/outside the active group - which makes buttons
 * window texture not update, we try to avoid it by clearing the
 * flag when toggling group editing - Campbell */
#define NODE_ACTIVE_TEXTURE (1 << 14)
/* use a custom color for the node */
#define NODE_CUSTOM_COLOR (1 << 15)
/* Node has been initialized
 * This flag indicates the node->typeinfo->init function has been called.
 * In case of undefined type at creation time this can be delayed until
 * until the node type is registered.
 */
#define NODE_INIT (1 << 16)

/* do recalc of output, used to skip recalculation of unwanted
 * composite out nodes when editing tree
 */
#define NODE_DO_OUTPUT_RECALC (1 << 17)
/* A preview for the data in this node can be displayed in the spreadsheet editor. */
#define __NODE_ACTIVE_PREVIEW (1 << 18) /* deprecated */
/* Active node that is used to paint on. */
#define NODE_ACTIVE_PAINT_CANVAS (1 << 19)

/* node->update */
#define NODE_UPDATE_ID 1       /* associated id data block has changed */
#define NODE_UPDATE_OPERATOR 2 /* node update triggered from update operator */

/* Unique hash key for identifying node instances
 * Defined as a struct because DNA does not support other typedefs.
 */
typedef struct bNodeInstanceKey {
  unsigned int value;
} bNodeInstanceKey;

/* Base struct for entries in node instance hash.
 * WARNING: pointers are cast to this struct internally,
 * it must be first member in hash entry structs!
 */
#
#
typedef struct bNodeInstanceHashEntry {
  bNodeInstanceKey key;

  /* tags for cleaning the cache */
  short tag;
} bNodeInstanceHashEntry;

#
#
typedef struct bNodePreview {
  /** Must be first. */
  bNodeInstanceHashEntry hash_entry;

  unsigned char *rect;
  short xsize, ysize;
} bNodePreview;

typedef struct bNodeLink {
  struct bNodeLink *next, *prev;

  bNode *fromnode, *tonode;
  bNodeSocket *fromsock, *tosock;

  int flag;
  int multi_input_socket_index;

#ifdef __cplusplus
  bool is_muted() const;
  bool is_available() const;
  /** Both linked sockets are available and the link is not muted. */
  bool is_used() const;
#endif

} bNodeLink;

/* link->flag */
#define NODE_LINKFLAG_HILITE (1 << 0) /* link has been successfully validated */
#define NODE_LINK_VALID (1 << 1)
#define NODE_LINK_TEST (1 << 2)           /* free test flag, undefined */
#define NODE_LINK_TEMP_HIGHLIGHT (1 << 3) /* Link is highlighted for picking. */
#define NODE_LINK_MUTED (1 << 4)          /* Link is muted. */

/* tree->edit_quality/tree->render_quality */
#define NTREE_QUALITY_HIGH 0
#define NTREE_QUALITY_MEDIUM 1
#define NTREE_QUALITY_LOW 2

/* tree->chunksize */
#define NTREE_CHUNKSIZE_32 32
#define NTREE_CHUNKSIZE_64 64
#define NTREE_CHUNKSIZE_128 128
#define NTREE_CHUNKSIZE_256 256
#define NTREE_CHUNKSIZE_512 512
#define NTREE_CHUNKSIZE_1024 1024

/* the basis for a Node tree, all links and nodes reside internal here */
/* only re-usable node trees are in the library though,
 * materials and textures allocate own tree struct */
typedef struct bNodeTree {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /** The ID owning this node tree, in case it is an embedded one. */
  ID *owner_id;

  /** Runtime type information. */
  struct bNodeTreeType *typeinfo;
  /** Runtime type identifier. */
  char idname[64];

  /** Grease pencil data. */
  struct bGPdata *gpd;
  /** Node tree stores own offset for consistent editor view. */
  float view_center[2];

  ListBase nodes, links;

  int type;

  /**
   * Sockets in groups have unique identifiers, adding new sockets always
   * will increase this counter.
   */
  int cur_index;
  int flag;

  /** Quality setting when editing. */
  short edit_quality;
  /** Quality setting when rendering. */
  short render_quality;
  /** Tile size for compositor engine. */
  int chunksize;
  /** Execution mode to use for compositor engine. */
  int execution_mode;

  rctf viewer_border;

  /* Lists of bNodeSocket to hold default values and own_index.
   * Warning! Don't make links to these sockets, input/output nodes are used for that.
   * These sockets are used only for generating external interfaces.
   */
  ListBase inputs, outputs;

  /* Node preview hash table
   * Only available in base node trees (e.g. scene->node_tree)
   */
  struct bNodeInstanceHash *previews;
  /* Defines the node tree instance to use for the "active" context,
   * in case multiple different editors are used and make context ambiguous.
   */
  bNodeInstanceKey active_viewer_key;

  char _pad[4];

  /** Image representing what the node group does. */
  struct PreviewImage *preview;

  bNodeTreeRuntimeHandle *runtime;

#ifdef __cplusplus

  /** A span containing all nodes in the node tree. */
  blender::Span<bNode *> all_nodes();
  blender::Span<const bNode *> all_nodes() const;

  /** Retrieve a node based on its persistent integer identifier. */
  struct bNode *node_by_id(int32_t identifier);
  const struct bNode *node_by_id(int32_t identifier) const;

  /**
   * Update a run-time cache for the node tree based on it's current state. This makes many methods
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
  blender::Span<const bNode *> group_input_nodes() const;
  /** Inputs and outputs of the entire node group. */
  blender::Span<const bNodeSocket *> interface_inputs() const;
  blender::Span<const bNodeSocket *> interface_outputs() const;
#endif
} bNodeTree;

/** #NodeTree.type, index */

#define NTREE_UNDEFINED -2 /* Represents #NodeTreeTypeUndefined type. */
#define NTREE_CUSTOM -1    /* for dynamically registered custom types */
#define NTREE_SHADER 0
#define NTREE_COMPOSIT 1
#define NTREE_TEXTURE 2
#define NTREE_GEOMETRY 3

/** #NodeTree.flag */
#define NTREE_DS_EXPAND (1 << 0)            /* for animation editors */
#define NTREE_COM_OPENCL (1 << 1)           /* Use OPENCL. */
#define NTREE_TWO_PASS (1 << 2)             /* two pass */
#define NTREE_COM_GROUPNODE_BUFFER (1 << 3) /* Use group-node buffers. */
#define NTREE_VIEWER_BORDER (1 << 4)        /* use a border for viewer nodes */
/* NOTE: DEPRECATED, use (id->tag & LIB_TAG_LOCALIZED) instead. */

/* tree is localized copy, free when deleting node groups */
/* #define NTREE_IS_LOCALIZED           (1 << 5) */

/* tree->execution_mode */
typedef enum eNodeTreeExecutionMode {
  NTREE_EXECUTION_MODE_TILED = 0,
  NTREE_EXECUTION_MODE_FULL_FRAME = 1,
} eNodeTreeExecutionMode;

typedef enum eNodeTreeRuntimeFlag {
  /** There is a node that references an image with animation. */
  NTREE_RUNTIME_FLAG_HAS_IMAGE_ANIMATION = 1 << 0,
  /** There is a material output node in the group. */
  NTREE_RUNTIME_FLAG_HAS_MATERIAL_OUTPUT = 1 << 1,
  /** There is a simulation zone in the group. */
  NTREE_RUNTIME_FLAG_HAS_SIMULATION_ZONE = 1 << 2,
} eNodeTreeRuntimeFlag;

/* socket value structs for input buttons
 * DEPRECATED now using ID properties
 */

typedef struct bNodeSocketValueInt {
  /** RNA subtype. */
  int subtype;
  int value;
  int min, max;
} bNodeSocketValueInt;

typedef struct bNodeSocketValueFloat {
  /** RNA subtype. */
  int subtype;
  float value;
  float min, max;
} bNodeSocketValueFloat;

typedef struct bNodeSocketValueBoolean {
  char value;
} bNodeSocketValueBoolean;

typedef struct bNodeSocketValueVector {
  /** RNA subtype. */
  int subtype;
  float value[3];
  float min, max;
} bNodeSocketValueVector;

typedef struct bNodeSocketValueRGBA {
  float value[4];
} bNodeSocketValueRGBA;

typedef struct bNodeSocketValueString {
  int subtype;
  char _pad[4];
  /** 1024 = FILEMAX. */
  char value[1024];
} bNodeSocketValueString;

typedef struct bNodeSocketValueObject {
  struct Object *value;
} bNodeSocketValueObject;

typedef struct bNodeSocketValueImage {
  struct Image *value;
} bNodeSocketValueImage;

typedef struct bNodeSocketValueCollection {
  struct Collection *value;
} bNodeSocketValueCollection;

typedef struct bNodeSocketValueTexture {
  struct Tex *value;
} bNodeSocketValueTexture;

typedef struct bNodeSocketValueMaterial {
  struct Material *value;
} bNodeSocketValueMaterial;

/* Data structs, for `node->storage`. */

typedef enum CMPNodeMaskType {
  CMP_NODE_MASKTYPE_ADD = 0,
  CMP_NODE_MASKTYPE_SUBTRACT = 1,
  CMP_NODE_MASKTYPE_MULTIPLY = 2,
  CMP_NODE_MASKTYPE_NOT = 3,
} CMPNodeMaskType;

typedef enum CMPNodeDilateErodeMethod {
  CMP_NODE_DILATE_ERODE_STEP = 0,
  CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD = 1,
  CMP_NODE_DILATE_ERODE_DISTANCE = 2,
  CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER = 3,
} CMPNodeDilateErodeMethod;

enum {
  CMP_NODE_INPAINT_SIMPLE = 0,
};

typedef enum CMPNodeMaskFlags {
  /* CMP_NODEFLAG_MASK_AA          = (1 << 0), */ /* DEPRECATED */
  CMP_NODE_MASK_FLAG_NO_FEATHER = (1 << 1),
  CMP_NODE_MASK_FLAG_MOTION_BLUR = (1 << 2),

  /* We may want multiple aspect options, exposed as an rna enum. */
  CMP_NODE_MASK_FLAG_SIZE_FIXED = (1 << 8),
  CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE = (1 << 9),
} CMPNodeMaskFlags;

enum {
  CMP_NODEFLAG_BLUR_VARIABLE_SIZE = (1 << 0),
  CMP_NODEFLAG_BLUR_EXTEND_BOUNDS = (1 << 1),
};

typedef struct NodeFrame {
  short flag;
  short label_size;
} NodeFrame;

/** \note This one has been replaced with #ImageUser, keep it for do_versions(). */
typedef struct NodeImageAnim {
  int frames DNA_DEPRECATED;
  int sfra DNA_DEPRECATED;
  int nr DNA_DEPRECATED;
  char cyclic DNA_DEPRECATED;
  char movie DNA_DEPRECATED;
  char _pad[2];
} NodeImageAnim;

typedef struct ColorCorrectionData {
  float saturation;
  float contrast;
  float gamma;
  float gain;
  float lift;
  char _pad[4];
} ColorCorrectionData;

typedef struct NodeColorCorrection {
  ColorCorrectionData master;
  ColorCorrectionData shadows;
  ColorCorrectionData midtones;
  ColorCorrectionData highlights;
  float startmidtones;
  float endmidtones;
} NodeColorCorrection;

typedef struct NodeBokehImage {
  float angle;
  int flaps;
  float rounding;
  float catadioptric;
  float lensshift;
} NodeBokehImage;

typedef struct NodeBoxMask {
  float x;
  float y;
  float rotation;
  float height;
  float width;
  char _pad[4];
} NodeBoxMask;

typedef struct NodeEllipseMask {
  float x;
  float y;
  float rotation;
  float height;
  float width;
  char _pad[4];
} NodeEllipseMask;

/** Layer info for image node outputs. */
typedef struct NodeImageLayer {
  /* index in the Image->layers->passes lists */
  int pass_index DNA_DEPRECATED;
  /* render pass name */
  /** Amount defined in IMB_openexr.h. */
  char pass_name[64];
} NodeImageLayer;

typedef struct NodeBlurData {
  short sizex, sizey;
  short samples, maxspeed, minspeed, relative, aspect;
  short curved;
  float fac, percentx, percenty;
  short filtertype;
  char bokeh, gamma;
  /** Needed for absolute/relative conversions. */
  int image_in_width, image_in_height;
} NodeBlurData;

typedef struct NodeDBlurData {
  float center_x, center_y, distance, angle, spin, zoom;
  short iter;
  char _pad[2];
} NodeDBlurData;

typedef struct NodeBilateralBlurData {
  float sigma_color, sigma_space;
  short iter;
  char _pad[2];
} NodeBilateralBlurData;

typedef struct NodeAntiAliasingData {
  float threshold;
  float contrast_limit;
  float corner_rounding;
} NodeAntiAliasingData;

/** \note Only for do-version code. */
typedef struct NodeHueSat {
  float hue, sat, val;
} NodeHueSat;

typedef struct NodeImageFile {
  /** 1024 = FILE_MAX. */
  char name[1024];
  struct ImageFormatData im_format;
  int sfra, efra;
} NodeImageFile;

/**
 * XXX: first struct fields should match #NodeImageFile to ensure forward compatibility.
 */
typedef struct NodeImageMultiFile {
  /** 1024 = FILE_MAX. */
  char base_path[1024];
  ImageFormatData format;
  /** XXX old frame rand values from NodeImageFile for forward compatibility. */
  int sfra DNA_DEPRECATED, efra DNA_DEPRECATED;
  /** Selected input in details view list. */
  int active_input;
  char _pad[4];
} NodeImageMultiFile;
typedef struct NodeImageMultiFileSocket {
  /* single layer file output */
  short use_render_format DNA_DEPRECATED;
  /** Use overall node image format. */
  short use_node_format;
  char save_as_render;
  char _pad1[3];
  /** 1024 = FILE_MAX. */
  char path[1024];
  ImageFormatData format;

  /* Multi-layer output. */
  /** EXR_TOT_MAXNAME-2 ('.' and channel char are appended). */
  char layer[30];
  char _pad2[2];
} NodeImageMultiFileSocket;

typedef struct NodeChroma {
  float t1, t2, t3;
  float fsize, fstrength, falpha;
  float key[4];
  short algorithm, channel;
} NodeChroma;

typedef struct NodeTwoXYs {
  short x1, x2, y1, y2;
  float fac_x1, fac_x2, fac_y1, fac_y2;
} NodeTwoXYs;

typedef struct NodeTwoFloats {
  float x, y;
} NodeTwoFloats;

typedef struct NodeVertexCol {
  char name[64];
} NodeVertexCol;

typedef struct NodeCMPCombSepColor {
  /* CMPNodeCombSepColorMode */
  uint8_t mode;
  uint8_t ycc_mode;
} NodeCMPCombSepColor;

/** Defocus blur node. */
typedef struct NodeDefocus {
  char bktype, _pad0, preview, gamco;
  short samples, no_zbuf;
  float fstop, maxblur, bthresh, scale;
  float rotation;
  char _pad1[4];
} NodeDefocus;

typedef struct NodeScriptDict {
  /** For PyObject *dict. */
  void *dict;
  /** For BPy_Node *node. */
  void *node;
} NodeScriptDict;

/** glare node. */
typedef struct NodeGlare {
  char quality, type, iter;
  /* XXX angle is only kept for backward/forward compatibility,
   * was used for two different things, see #50736. */
  char angle DNA_DEPRECATED, _pad0, size, star_45, streaks;
  float colmod, mix, threshold, fade;
  float angle_ofs;
  char _pad1[4];
} NodeGlare;

/** Tone-map node. */
typedef struct NodeTonemap {
  float key, offset, gamma;
  float f, m, a, c;
  int type;
} NodeTonemap;

/** Lens distortion node. */
typedef struct NodeLensDist {
  short jit, proj, fit;
  char _pad[2];
} NodeLensDist;

typedef struct NodeColorBalance {
  /* ASC CDL parameters */
  float slope[3];
  float offset[3];
  float power[3];
  float offset_basis;
  char _pad[4];

  /* LGG parameters */
  float lift[3];
  float gamma[3];
  float gain[3];
} NodeColorBalance;

typedef struct NodeColorspill {
  short limchan, unspill;
  float limscale;
  float uspillr, uspillg, uspillb;
} NodeColorspill;

typedef struct NodeConvertColorSpace {
  char from_color_space[64];
  char to_color_space[64];
} NodeConvertColorSpace;

typedef struct NodeDilateErode {
  char falloff;
} NodeDilateErode;

typedef struct NodeMask {
  int size_x, size_y;
} NodeMask;

typedef struct NodeSetAlpha {
  char mode;
} NodeSetAlpha;

typedef struct NodeTexBase {
  TexMapping tex_mapping;
  ColorMapping color_mapping;
} NodeTexBase;

typedef struct NodeTexSky {
  NodeTexBase base;
  int sky_model;
  float sun_direction[3];
  float turbidity;
  float ground_albedo;
  float sun_size;
  float sun_intensity;
  float sun_elevation;
  float sun_rotation;
  float altitude;
  float air_density;
  float dust_density;
  float ozone_density;
  char sun_disc;
  char _pad[7];
} NodeTexSky;

typedef struct NodeTexImage {
  NodeTexBase base;
  ImageUser iuser;
  int color_space DNA_DEPRECATED;
  int projection;
  float projection_blend;
  int interpolation;
  int extension;
  char _pad[4];
} NodeTexImage;

typedef struct NodeTexChecker {
  NodeTexBase base;
} NodeTexChecker;

typedef struct NodeTexBrick {
  NodeTexBase base;
  int offset_freq, squash_freq;
  float offset, squash;
} NodeTexBrick;

typedef struct NodeTexEnvironment {
  NodeTexBase base;
  ImageUser iuser;
  int color_space DNA_DEPRECATED;
  int projection;
  int interpolation;
  char _pad[4];
} NodeTexEnvironment;

typedef struct NodeTexGradient {
  NodeTexBase base;
  int gradient_type;
  char _pad[4];
} NodeTexGradient;

typedef struct NodeTexNoise {
  NodeTexBase base;
  int dimensions;
  char _pad[4];
} NodeTexNoise;

typedef struct NodeTexVoronoi {
  NodeTexBase base;
  int dimensions;
  int feature;
  int distance;
  int coloring DNA_DEPRECATED;
} NodeTexVoronoi;

typedef struct NodeTexMusgrave {
  NodeTexBase base;
  int musgrave_type;
  int dimensions;
} NodeTexMusgrave;

typedef struct NodeTexWave {
  NodeTexBase base;
  int wave_type;
  int bands_direction;
  int rings_direction;
  int wave_profile;
} NodeTexWave;

typedef struct NodeTexMagic {
  NodeTexBase base;
  int depth;
  char _pad[4];
} NodeTexMagic;

typedef struct NodeShaderAttribute {
  char name[64];
  int type;
  char _pad[4];
} NodeShaderAttribute;

typedef struct NodeShaderVectTransform {
  int type;
  int convert_from, convert_to;
  char _pad[4];
} NodeShaderVectTransform;

typedef struct NodeShaderTexPointDensity {
  NodeTexBase base;
  short point_source;
  char _pad[2];
  int particle_system;
  float radius;
  int resolution;
  short space;
  short interpolation;
  short color_source;
  short ob_color_source;
  /* Used at runtime only by sampling RNA API. */
  PointDensity pd;
  int cached_resolution;
  /** Vertex attribute layer for color source, MAX_CUSTOMDATA_LAYER_NAME. */
  char vertex_attribute_name[68];
} NodeShaderTexPointDensity;

typedef struct NodeShaderPrincipled {
  char use_subsurface_auto_radius;
  char _pad[3];
} NodeShaderPrincipled;

/** TEX_output. */
typedef struct TexNodeOutput {
  char name[64];
} TexNodeOutput;

typedef struct NodeKeyingScreenData {
  char tracking_object[64];
} NodeKeyingScreenData;

typedef struct NodeKeyingData {
  float screen_balance;
  float despill_factor;
  float despill_balance;
  int edge_kernel_radius;
  float edge_kernel_tolerance;
  float clip_black, clip_white;
  int dilate_distance;
  int feather_distance;
  int feather_falloff;
  int blur_pre, blur_post;
} NodeKeyingData;

typedef struct NodeTrackPosData {
  char tracking_object[64];
  char track_name[64];
} NodeTrackPosData;

typedef struct NodeTranslateData {
  char wrap_axis;
  char relative;
} NodeTranslateData;

typedef struct NodePlaneTrackDeformData {
  char tracking_object[64];
  char plane_track_name[64];
  char flag;
  char motion_blur_samples;
  char _pad[2];
  float motion_blur_shutter;
} NodePlaneTrackDeformData;

typedef struct NodeShaderScript {
  int mode;
  int flag;

  /** 1024 = FILE_MAX. */
  char filepath[1024];

  char bytecode_hash[64];
  char *bytecode;
} NodeShaderScript;

typedef struct NodeShaderTangent {
  int direction_type;
  int axis;
  char uv_map[64];
} NodeShaderTangent;

typedef struct NodeShaderNormalMap {
  int space;
  char uv_map[64];
} NodeShaderNormalMap;

typedef struct NodeShaderUVMap {
  char uv_map[64];
} NodeShaderUVMap;

typedef struct NodeShaderVertexColor {
  char layer_name[64];
} NodeShaderVertexColor;

typedef struct NodeShaderTexIES {
  int mode;

  /** 1024 = FILE_MAX. */
  char filepath[1024];
} NodeShaderTexIES;

typedef struct NodeShaderOutputAOV {
  char name[64];
} NodeShaderOutputAOV;

typedef struct NodeSunBeams {
  float source[2];

  float ray_length;
} NodeSunBeams;

typedef struct CryptomatteEntry {
  struct CryptomatteEntry *next, *prev;
  float encoded_hash;
  /** MAX_NAME. */
  char name[64];
  char _pad[4];
} CryptomatteEntry;

typedef struct CryptomatteLayer {
  struct CryptomatteEntry *next, *prev;
  char name[64];
} CryptomatteLayer;

typedef struct NodeCryptomatte_Runtime {
  /* Contains `CryptomatteLayer`. */
  ListBase layers;
  /* Temp storage for the crypto-matte picker. */
  float add[3];
  float remove[3];
} NodeCryptomatte_Runtime;

typedef struct NodeCryptomatte {
  /**
   * `iuser` needs to be first element due to RNA limitations.
   * When we define the #ImageData properties, we can't define them from
   * `storage->iuser`, so storage needs to be cast to #ImageUser directly.
   */
  ImageUser iuser;

  /* Contains `CryptomatteEntry`. */
  ListBase entries;

  /* MAX_NAME */
  char layer_name[64];
  /* Stores `entries` as a string for opening in 2.80-2.91. */
  char *matte_id;

  /** Legacy attributes */
  /* Number of input sockets. */
  int inputs_num;

  char _pad[4];
  NodeCryptomatte_Runtime runtime;
} NodeCryptomatte;

typedef struct NodeDenoise {
  char hdr;
  char prefilter;
} NodeDenoise;

typedef struct NodeMapRange {
  /* eCustomDataType */
  uint8_t data_type;

  /* NodeMapRangeType. */
  uint8_t interpolation_type;
  uint8_t clamp;
  char _pad[5];
} NodeMapRange;

typedef struct NodeRandomValue {
  /* eCustomDataType. */
  uint8_t data_type;
} NodeRandomValue;

typedef struct NodeAccumulateField {
  /* eCustomDataType. */
  uint8_t data_type;
  /* eAttrDomain. */
  uint8_t domain;
} NodeAccumulateField;

typedef struct NodeInputBool {
  uint8_t boolean;
} NodeInputBool;

typedef struct NodeInputInt {
  int integer;
} NodeInputInt;

typedef struct NodeInputVector {
  float vector[3];
} NodeInputVector;

typedef struct NodeInputColor {
  float color[4];
} NodeInputColor;

typedef struct NodeInputString {
  char *string;
} NodeInputString;

typedef struct NodeGeometryExtrudeMesh {
  /* GeometryNodeExtrudeMeshMode */
  uint8_t mode;
} NodeGeometryExtrudeMesh;

typedef struct NodeGeometryObjectInfo {
  /* GeometryNodeTransformSpace. */
  uint8_t transform_space;
} NodeGeometryObjectInfo;

typedef struct NodeGeometryPointsToVolume {
  /* GeometryNodePointsToVolumeResolutionMode */
  uint8_t resolution_mode;
  /* GeometryNodeAttributeInputMode */
  uint8_t input_type_radius;
} NodeGeometryPointsToVolume;

typedef struct NodeGeometryCollectionInfo {
  /* GeometryNodeTransformSpace. */
  uint8_t transform_space;
} NodeGeometryCollectionInfo;

typedef struct NodeGeometryProximity {
  /* GeometryNodeProximityTargetType. */
  uint8_t target_element;
} NodeGeometryProximity;

typedef struct NodeGeometryVolumeToMesh {
  /* VolumeToMeshResolutionMode */
  uint8_t resolution_mode;
} NodeGeometryVolumeToMesh;

typedef struct NodeGeometryMeshToVolume {
  /* MeshToVolumeModifierResolutionMode */
  uint8_t resolution_mode;
} NodeGeometryMeshToVolume;

typedef struct NodeGeometrySubdivisionSurface {
  /* eSubsurfUVSmooth. */
  uint8_t uv_smooth;
  /* eSubsurfBoundarySmooth. */
  uint8_t boundary_smooth;
} NodeGeometrySubdivisionSurface;

typedef struct NodeGeometryMeshCircle {
  /* GeometryNodeMeshCircleFillType. */
  uint8_t fill_type;
} NodeGeometryMeshCircle;

typedef struct NodeGeometryMeshCylinder {
  /* GeometryNodeMeshCircleFillType. */
  uint8_t fill_type;
} NodeGeometryMeshCylinder;

typedef struct NodeGeometryMeshCone {
  /* GeometryNodeMeshCircleFillType. */
  uint8_t fill_type;
} NodeGeometryMeshCone;

typedef struct NodeGeometryMergeByDistance {
  /* GeometryNodeMergeByDistanceMode. */
  uint8_t mode;
} NodeGeometryMergeByDistance;

typedef struct NodeGeometryMeshLine {
  /* GeometryNodeMeshLineMode. */
  uint8_t mode;
  /* GeometryNodeMeshLineCountMode. */
  uint8_t count_mode;
} NodeGeometryMeshLine;

typedef struct NodeSwitch {
  /* NodeSwitch. */
  uint8_t input_type;
} NodeSwitch;

typedef struct NodeGeometryCurveSplineType {
  /* GeometryNodeSplineType. */
  uint8_t spline_type;
} NodeGeometryCurveSplineType;

typedef struct NodeGeometrySetCurveHandlePositions {
  /* GeometryNodeCurveHandleMode. */
  uint8_t mode;
} NodeGeometrySetCurveHandlePositions;

typedef struct NodeGeometryCurveSetHandles {
  /* GeometryNodeCurveHandleType. */
  uint8_t handle_type;
  /* GeometryNodeCurveHandleMode. */
  uint8_t mode;
} NodeGeometryCurveSetHandles;

typedef struct NodeGeometryCurveSelectHandles {
  /* GeometryNodeCurveHandleType. */
  uint8_t handle_type;
  /* GeometryNodeCurveHandleMode. */
  uint8_t mode;
} NodeGeometryCurveSelectHandles;

typedef struct NodeGeometryCurvePrimitiveArc {
  /* GeometryNodeCurvePrimitiveArcMode. */
  uint8_t mode;
} NodeGeometryCurvePrimitiveArc;

typedef struct NodeGeometryCurvePrimitiveLine {
  /* GeometryNodeCurvePrimitiveLineMode. */
  uint8_t mode;
} NodeGeometryCurvePrimitiveLine;

typedef struct NodeGeometryCurvePrimitiveBezierSegment {
  /* GeometryNodeCurvePrimitiveBezierSegmentMode. */
  uint8_t mode;
} NodeGeometryCurvePrimitiveBezierSegment;

typedef struct NodeGeometryCurvePrimitiveCircle {
  /* GeometryNodeCurvePrimitiveMode. */
  uint8_t mode;
} NodeGeometryCurvePrimitiveCircle;

typedef struct NodeGeometryCurvePrimitiveQuad {
  /* GeometryNodeCurvePrimitiveQuadMode. */
  uint8_t mode;
} NodeGeometryCurvePrimitiveQuad;

typedef struct NodeGeometryCurveResample {
  /* GeometryNodeCurveResampleMode. */
  uint8_t mode;
} NodeGeometryCurveResample;

typedef struct NodeGeometryCurveFillet {
  /* GeometryNodeCurveFilletMode. */
  uint8_t mode;
} NodeGeometryCurveFillet;

typedef struct NodeGeometryCurveTrim {
  /* GeometryNodeCurveSampleMode. */
  uint8_t mode;
} NodeGeometryCurveTrim;

typedef struct NodeGeometryCurveToPoints {
  /* GeometryNodeCurveResampleMode. */
  uint8_t mode;
} NodeGeometryCurveToPoints;

typedef struct NodeGeometryCurveSample {
  /* GeometryNodeCurveSampleMode. */
  uint8_t mode;
  int8_t use_all_curves;
  /* eCustomDataType. */
  int8_t data_type;
  char _pad[1];
} NodeGeometryCurveSample;

typedef struct NodeGeometryTransferAttribute {
  /* eCustomDataType. */
  int8_t data_type;
  /* eAttrDomain. */
  int8_t domain;
  /* GeometryNodeAttributeTransferMode. */
  uint8_t mode;
  char _pad[1];
} NodeGeometryTransferAttribute;

typedef struct NodeGeometrySampleIndex {
  /* eCustomDataType. */
  int8_t data_type;
  /* eAttrDomain. */
  int8_t domain;
  int8_t clamp;
  char _pad[1];
} NodeGeometrySampleIndex;

typedef struct NodeGeometryRaycast {
  /* GeometryNodeRaycastMapMode. */
  uint8_t mapping;

  /* eCustomDataType. */
  int8_t data_type;
} NodeGeometryRaycast;

typedef struct NodeGeometryCurveFill {
  uint8_t mode;
} NodeGeometryCurveFill;

typedef struct NodeGeometryMeshToPoints {
  /* GeometryNodeMeshToPointsMode */
  uint8_t mode;
} NodeGeometryMeshToPoints;

typedef struct NodeGeometryAttributeCapture {
  /* eCustomDataType. */
  int8_t data_type;
  /* eAttrDomain. */
  int8_t domain;
} NodeGeometryAttributeCapture;

typedef struct NodeGeometryStoreNamedAttribute {
  /* eCustomDataType. */
  int8_t data_type;
  /* eAttrDomain. */
  int8_t domain;
} NodeGeometryStoreNamedAttribute;

typedef struct NodeGeometryInputNamedAttribute {
  /* eCustomDataType. */
  int8_t data_type;
} NodeGeometryInputNamedAttribute;

typedef struct NodeGeometryStringToCurves {
  /* GeometryNodeStringToCurvesOverflowMode */
  uint8_t overflow;
  /* GeometryNodeStringToCurvesAlignXMode */
  uint8_t align_x;
  /* GeometryNodeStringToCurvesAlignYMode */
  uint8_t align_y;
  /* GeometryNodeStringToCurvesPivotMode */
  uint8_t pivot_mode;
} NodeGeometryStringToCurves;

typedef struct NodeGeometryDeleteGeometry {
  /* eAttrDomain. */
  int8_t domain;
  /* GeometryNodeDeleteGeometryMode. */
  int8_t mode;
} NodeGeometryDeleteGeometry;

typedef struct NodeGeometryDuplicateElements {
  /* eAttrDomain. */
  int8_t domain;
} NodeGeometryDuplicateElements;

typedef struct NodeGeometrySeparateGeometry {
  /* eAttrDomain. */
  int8_t domain;
} NodeGeometrySeparateGeometry;

typedef struct NodeGeometryImageTexture {
  int8_t interpolation;
  int8_t extension;
} NodeGeometryImageTexture;

typedef struct NodeGeometryViewer {
  /* eCustomDataType. */
  int8_t data_type;
  /* eAttrDomain. */
  int8_t domain;
} NodeGeometryViewer;

typedef struct NodeGeometryUVUnwrap {
  /* GeometryNodeUVUnwrapMethod. */
  uint8_t method;
} NodeGeometryUVUnwrap;

typedef struct NodeSimulationItem {
  char *name;
  /** #eNodeSocketDatatype. */
  short socket_type;
  /** #eAttrDomain. */
  short attribute_domain;
  /**
   * Generates unique identifier for sockets which stays the same even when the item order or
   * names change.
   */
  int identifier;
} NodeSimulationItem;

typedef struct NodeGeometrySimulationInput {
  /** bNode.identifier of the corresponding output node. */
  int32_t output_node_id;
} NodeGeometrySimulationInput;

typedef struct NodeGeometrySimulationOutput {
  NodeSimulationItem *items;
  int items_num;
  int active_index;
  /** Number to give unique IDs to state items. */
  int next_identifier;
  int _pad;

#ifdef __cplusplus
  blender::Span<NodeSimulationItem> items_span() const;
  blender::MutableSpan<NodeSimulationItem> items_span_for_write();
  blender::IndexRange items_range() const;
#endif
} NodeGeometrySimulationOutput;

typedef struct NodeGeometryDistributePointsInVolume {
  /* GeometryNodePointDistributeVolumeMode. */
  uint8_t mode;
} NodeGeometryDistributePointsInVolume;

typedef struct NodeGeometrySampleVolume {
  /* eCustomDataType. */
  int8_t grid_type;
  /* GeometryNodeSampleVolumeInterpolationMode */
  int8_t interpolation_mode;
} NodeGeometrySampleVolume;

typedef struct NodeFunctionCompare {
  /* NodeCompareOperation */
  int8_t operation;
  /* eNodeSocketDatatype */
  int8_t data_type;
  /* NodeCompareMode */
  int8_t mode;
  char _pad[1];
} NodeFunctionCompare;

typedef struct NodeCombSepColor {
  /* NodeCombSepColorMode */
  int8_t mode;
} NodeCombSepColor;

typedef struct NodeShaderMix {
  /* eNodeSocketDatatype */
  int8_t data_type;
  /* NodeShaderMixMode */
  int8_t factor_mode;
  int8_t clamp_factor;
  int8_t clamp_result;
  int8_t blend_type;
  char _pad[3];
} NodeShaderMix;

/* script node mode */
#define NODE_SCRIPT_INTERNAL 0
#define NODE_SCRIPT_EXTERNAL 1

/* script node flag */
#define NODE_SCRIPT_AUTO_UPDATE 1

/* IES node mode. */
#define NODE_IES_INTERNAL 0
#define NODE_IES_EXTERNAL 1

/* Frame node flags. */

#define NODE_FRAME_SHRINK 1     /* keep the bounding box minimal */
#define NODE_FRAME_RESIZEABLE 2 /* test flag, if frame can be resized by user */

/* Proxy node flags. */

#define NODE_PROXY_AUTOTYPE 1 /* automatically change output type based on link */

/* Comp channel matte. */

#define CMP_NODE_CHANNEL_MATTE_CS_RGB 1
#define CMP_NODE_CHANNEL_MATTE_CS_HSV 2
#define CMP_NODE_CHANNEL_MATTE_CS_YUV 3
#define CMP_NODE_CHANNEL_MATTE_CS_YCC 4

/* glossy distributions */
#define SHD_GLOSSY_BECKMANN 0
#define SHD_GLOSSY_SHARP_DEPRECATED 1 /* deprecated */
#define SHD_GLOSSY_GGX 2
#define SHD_GLOSSY_ASHIKHMIN_SHIRLEY 3
#define SHD_GLOSSY_MULTI_GGX 4

/* vector transform */
#define SHD_VECT_TRANSFORM_TYPE_VECTOR 0
#define SHD_VECT_TRANSFORM_TYPE_POINT 1
#define SHD_VECT_TRANSFORM_TYPE_NORMAL 2

#define SHD_VECT_TRANSFORM_SPACE_WORLD 0
#define SHD_VECT_TRANSFORM_SPACE_OBJECT 1
#define SHD_VECT_TRANSFORM_SPACE_CAMERA 2

/** #NodeShaderAttribute.type */
enum {
  SHD_ATTRIBUTE_GEOMETRY = 0,
  SHD_ATTRIBUTE_OBJECT = 1,
  SHD_ATTRIBUTE_INSTANCER = 2,
  SHD_ATTRIBUTE_VIEW_LAYER = 3,
};

/* toon modes */
#define SHD_TOON_DIFFUSE 0
#define SHD_TOON_GLOSSY 1

/* hair components */
#define SHD_HAIR_REFLECTION 0
#define SHD_HAIR_TRANSMISSION 1

/* principled hair parametrization */
#define SHD_PRINCIPLED_HAIR_REFLECTANCE 0
#define SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION 1
#define SHD_PRINCIPLED_HAIR_DIRECT_ABSORPTION 2

/* blend texture */
#define SHD_BLEND_LINEAR 0
#define SHD_BLEND_QUADRATIC 1
#define SHD_BLEND_EASING 2
#define SHD_BLEND_DIAGONAL 3
#define SHD_BLEND_RADIAL 4
#define SHD_BLEND_QUADRATIC_SPHERE 5
#define SHD_BLEND_SPHERICAL 6

/* noise basis for textures */
#define SHD_NOISE_PERLIN 0
#define SHD_NOISE_VORONOI_F1 1
#define SHD_NOISE_VORONOI_F2 2
#define SHD_NOISE_VORONOI_F3 3
#define SHD_NOISE_VORONOI_F4 4
#define SHD_NOISE_VORONOI_F2_F1 5
#define SHD_NOISE_VORONOI_CRACKLE 6
#define SHD_NOISE_CELL_NOISE 7

#define SHD_NOISE_SOFT 0
#define SHD_NOISE_HARD 1

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

/* musgrave texture */
#define SHD_MUSGRAVE_MULTIFRACTAL 0
#define SHD_MUSGRAVE_FBM 1
#define SHD_MUSGRAVE_HYBRID_MULTIFRACTAL 2
#define SHD_MUSGRAVE_RIDGED_MULTIFRACTAL 3
#define SHD_MUSGRAVE_HETERO_TERRAIN 4

/* wave texture */
#define SHD_WAVE_BANDS 0
#define SHD_WAVE_RINGS 1

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
#define SHD_SKY_PREETHAM 0
#define SHD_SKY_HOSEK 1
#define SHD_SKY_NISHITA 2

/* environment texture */
#define SHD_PROJ_EQUIRECTANGULAR 0
#define SHD_PROJ_MIRROR_BALL 1

#define SHD_IMAGE_EXTENSION_REPEAT 0
#define SHD_IMAGE_EXTENSION_EXTEND 1
#define SHD_IMAGE_EXTENSION_CLIP 2
#define SHD_IMAGE_EXTENSION_MIRROR 3

/* image texture */
#define SHD_PROJ_FLAT 0
#define SHD_PROJ_BOX 1
#define SHD_PROJ_SPHERE 2
#define SHD_PROJ_TUBE 3

/* image texture interpolation */
#define SHD_INTERP_LINEAR 0
#define SHD_INTERP_CLOSEST 1
#define SHD_INTERP_CUBIC 2
#define SHD_INTERP_SMART 3

/* tangent */
#define SHD_TANGENT_RADIAL 0
#define SHD_TANGENT_UVMAP 1

/* tangent */
#define SHD_TANGENT_AXIS_X 0
#define SHD_TANGENT_AXIS_Y 1
#define SHD_TANGENT_AXIS_Z 2

/* normal map, displacement space */
#define SHD_SPACE_TANGENT 0
#define SHD_SPACE_OBJECT 1
#define SHD_SPACE_WORLD 2
#define SHD_SPACE_BLENDER_OBJECT 3
#define SHD_SPACE_BLENDER_WORLD 4

#define SHD_AO_INSIDE 1
#define SHD_AO_LOCAL 2

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
#define SHD_MATH_CLAMP 1

typedef enum NodeMathOperation {
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
} NodeMathOperation;

typedef enum NodeVectorMathOperation {
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
} NodeVectorMathOperation;

typedef enum NodeBooleanMathOperation {
  NODE_BOOLEAN_MATH_AND = 0,
  NODE_BOOLEAN_MATH_OR = 1,
  NODE_BOOLEAN_MATH_NOT = 2,

  NODE_BOOLEAN_MATH_NAND = 3,
  NODE_BOOLEAN_MATH_NOR = 4,
  NODE_BOOLEAN_MATH_XNOR = 5,
  NODE_BOOLEAN_MATH_XOR = 6,

  NODE_BOOLEAN_MATH_IMPLY = 7,
  NODE_BOOLEAN_MATH_NIMPLY = 8,
} NodeBooleanMathOperation;

typedef enum NodeShaderMixMode {
  NODE_MIX_MODE_UNIFORM = 0,
  NODE_MIX_MODE_NON_UNIFORM = 1,
} NodeShaderMixMode;

typedef enum NodeCompareMode {
  NODE_COMPARE_MODE_ELEMENT = 0,
  NODE_COMPARE_MODE_LENGTH = 1,
  NODE_COMPARE_MODE_AVERAGE = 2,
  NODE_COMPARE_MODE_DOT_PRODUCT = 3,
  NODE_COMPARE_MODE_DIRECTION = 4
} NodeCompareMode;

typedef enum NodeCompareOperation {
  NODE_COMPARE_LESS_THAN = 0,
  NODE_COMPARE_LESS_EQUAL = 1,
  NODE_COMPARE_GREATER_THAN = 2,
  NODE_COMPARE_GREATER_EQUAL = 3,
  NODE_COMPARE_EQUAL = 4,
  NODE_COMPARE_NOT_EQUAL = 5,
  NODE_COMPARE_COLOR_BRIGHTER = 6,
  NODE_COMPARE_COLOR_DARKER = 7,
} NodeCompareOperation;

typedef enum FloatToIntRoundingMode {
  FN_NODE_FLOAT_TO_INT_ROUND = 0,
  FN_NODE_FLOAT_TO_INT_FLOOR = 1,
  FN_NODE_FLOAT_TO_INT_CEIL = 2,
  FN_NODE_FLOAT_TO_INT_TRUNCATE = 3,
} FloatToIntRoundingMode;

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
#define SHD_MIXRGB_USE_ALPHA 1
#define SHD_MIXRGB_CLAMP 2

/* Subsurface. */

enum {
#ifdef DNA_DEPRECATED_ALLOW
  SHD_SUBSURFACE_COMPATIBLE = 0, /* Deprecated */
  SHD_SUBSURFACE_CUBIC = 1,
  SHD_SUBSURFACE_GAUSSIAN = 2,
#endif
  SHD_SUBSURFACE_BURLEY = 3,
  SHD_SUBSURFACE_RANDOM_WALK_FIXED_RADIUS = 4,
  SHD_SUBSURFACE_RANDOM_WALK = 5,
};

/* blur node */
#define CMP_NODE_BLUR_ASPECT_NONE 0
#define CMP_NODE_BLUR_ASPECT_Y 1
#define CMP_NODE_BLUR_ASPECT_X 2

/* wrapping */
#define CMP_NODE_WRAP_NONE 0
#define CMP_NODE_WRAP_X 1
#define CMP_NODE_WRAP_Y 2
#define CMP_NODE_WRAP_XY 3

#define CMP_NODE_MASK_MBLUR_SAMPLES_MAX 64

/* image */
#define CMP_NODE_IMAGE_USE_STRAIGHT_OUTPUT 1

/* viewer and composite output. */
#define CMP_NODE_OUTPUT_IGNORE_ALPHA 1

/** Split Viewer Node. Stored in `custom2`. */
typedef enum CMPNodeSplitViewerAxis {
  CMP_NODE_SPLIT_VIEWER_HORIZONTAL = 0,
  CMP_NODE_SPLIT_VIEWER_VERTICAL = 1,
} CMPNodeSplitViewerAxis;

/** Color Balance Node. Stored in `custom1`. */
typedef enum CMPNodeColorBalanceMethod {
  CMP_NODE_COLOR_BALANCE_LGG = 0,
  CMP_NODE_COLOR_BALANCE_ASC_CDL = 1,
} CMPNodeColorBalanceMethod;

/** Alpha Convert Node. Stored in `custom1`. */
typedef enum CMPNodeAlphaConvertMode {
  CMP_NODE_ALPHA_CONVERT_PREMULTIPLY = 0,
  CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY = 1,
} CMPNodeAlphaConvertMode;

/** Distance Matte Node. Stored in #NodeChroma.channel. */
typedef enum CMPNodeDistanceMatteColorSpace {
  CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_YCCA = 0,
  CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA = 1,
} CMPNodeDistanceMatteColorSpace;

/** Color Spill Node. Stored in `custom2`. */
typedef enum CMPNodeColorSpillLimitAlgorithm {
  CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_SINGLE = 0,
  CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_AVERAGE = 1,
} CMPNodeColorSpillLimitAlgorithm;

/** Channel Matte Node. Stored in #NodeChroma.algorithm. */
typedef enum CMPNodeChannelMatteLimitAlgorithm {
  CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE = 0,
  CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_MAX = 1,
} CMPNodeChannelMatteLimitAlgorithm;

/* Flip Node. Stored in custom1. */
typedef enum CMPNodeFlipMode {
  CMP_NODE_FLIP_X = 0,
  CMP_NODE_FLIP_Y = 1,
  CMP_NODE_FLIP_X_Y = 2,
} CMPNodeFlipMode;

/* Scale Node. Stored in custom1. */
typedef enum CMPNodeScaleMethod {
  CMP_NODE_SCALE_RELATIVE = 0,
  CMP_NODE_SCALE_ABSOLUTE = 1,
  CMP_NODE_SCALE_RENDER_PERCENT = 2,
  CMP_NODE_SCALE_RENDER_SIZE = 3,
} CMPNodeScaleMethod;

/* Scale Node. Stored in custom2. */
typedef enum CMPNodeScaleRenderSizeMethod {
  CMP_NODE_SCALE_RENDER_SIZE_STRETCH = 0,
  CMP_NODE_SCALE_RENDER_SIZE_FIT = 1,
  CMP_NODE_SCALE_RENDER_SIZE_CROP = 2,
} CMPNodeScaleRenderSizeMethod;

/* Filter Node. Stored in custom1. */
typedef enum CMPNodeFilterMethod {
  CMP_NODE_FILTER_SOFT = 0,
  CMP_NODE_FILTER_SHARP_BOX = 1,
  CMP_NODE_FILTER_LAPLACE = 2,
  CMP_NODE_FILTER_SOBEL = 3,
  CMP_NODE_FILTER_PREWITT = 4,
  CMP_NODE_FILTER_KIRSCH = 5,
  CMP_NODE_FILTER_SHADOW = 6,
  CMP_NODE_FILTER_SHARP_DIAMOND = 7,
} CMPNodeFilterMethod;

/* Levels Node. Stored in custom1. */
typedef enum CMPNodeLevelsChannel {
  CMP_NODE_LEVLES_LUMINANCE = 1,
  CMP_NODE_LEVLES_RED = 2,
  CMP_NODE_LEVLES_GREEN = 3,
  CMP_NODE_LEVLES_BLUE = 4,
  CMP_NODE_LEVLES_LUMINANCE_BT709 = 5,
} CMPNodeLevelsChannel;

/* Tone Map Node. Stored in NodeTonemap.type. */
typedef enum CMPNodeToneMapType {
  CMP_NODE_TONE_MAP_SIMPLE = 0,
  CMP_NODE_TONE_MAP_PHOTORECEPTOR = 1,
} CMPNodeToneMapType;

/* Track Position Node. Stored in custom1. */
typedef enum CMPNodeTrackPositionMode {
  CMP_NODE_TRACK_POSITION_ABSOLUTE = 0,
  CMP_NODE_TRACK_POSITION_RELATIVE_START = 1,
  CMP_NODE_TRACK_POSITION_RELATIVE_FRAME = 2,
  CMP_NODE_TRACK_POSITION_ABSOLUTE_FRAME = 3,
} CMPNodeTrackPositionMode;

/* Glare Node. Stored in NodeGlare.type. */
typedef enum CMPNodeGlareType {
  CMP_NODE_GLARE_SIMPLE_STAR = 0,
  CMP_NODE_GLARE_FOG_GLOW = 1,
  CMP_NODE_GLARE_STREAKS = 2,
  CMP_NODE_GLARE_GHOST = 3,
} CMPNodeGlareType;

/* Stabilize 2D node. Stored in custom1. */
typedef enum CMPNodeStabilizeInterpolation {
  CMP_NODE_STABILIZE_INTERPOLATION_NEAREST = 0,
  CMP_NODE_STABILIZE_INTERPOLATION_BILINEAR = 1,
  CMP_NODE_STABILIZE_INTERPOLATION_BICUBIC = 2,
} CMPNodeStabilizeInterpolation;

/* Stabilize 2D node. Stored in custom2. */
typedef enum CMPNodeStabilizeInverse {
  CMP_NODE_STABILIZE_FLAG_INVERSE = 1,
} CMPNodeStabilizeInverse;

#define CMP_NODE_PLANE_TRACK_DEFORM_MOTION_BLUR_SAMPLES_MAX 64

/* Plane track deform node. */
typedef enum CMPNodePlaneTrackDeformFlags {
  CMP_NODE_PLANE_TRACK_DEFORM_FLAG_MOTION_BLUR = 1,
} CMPNodePlaneTrackDeformFlags;

/* Set Alpha Node. */

/** #NodeSetAlpha.mode */
typedef enum CMPNodeSetAlphaMode {
  CMP_NODE_SETALPHA_MODE_APPLY = 0,
  CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA = 1,
} CMPNodeSetAlphaMode;

/* Denoise Node. */

/** #NodeDenoise.prefilter */
typedef enum CMPNodeDenoisePrefilter {
  CMP_NODE_DENOISE_PREFILTER_FAST = 0,
  CMP_NODE_DENOISE_PREFILTER_NONE = 1,
  CMP_NODE_DENOISE_PREFILTER_ACCURATE = 2
} CMPNodeDenoisePrefilter;

/* Color combine/separate modes */

typedef enum CMPNodeCombSepColorMode {
  CMP_NODE_COMBSEP_COLOR_RGB = 0,
  CMP_NODE_COMBSEP_COLOR_HSV = 1,
  CMP_NODE_COMBSEP_COLOR_HSL = 2,
  CMP_NODE_COMBSEP_COLOR_YCC = 3,
  CMP_NODE_COMBSEP_COLOR_YUV = 4,
} CMPNodeCombSepColorMode;

/* Point Density shader node */

enum {
  SHD_POINTDENSITY_SOURCE_PSYS = 0,
  SHD_POINTDENSITY_SOURCE_OBJECT = 1,
};

enum {
  SHD_POINTDENSITY_SPACE_OBJECT = 0,
  SHD_POINTDENSITY_SPACE_WORLD = 1,
};

enum {
  SHD_POINTDENSITY_COLOR_PARTAGE = 1,
  SHD_POINTDENSITY_COLOR_PARTSPEED = 2,
  SHD_POINTDENSITY_COLOR_PARTVEL = 3,
};

enum {
  SHD_POINTDENSITY_COLOR_VERTCOL = 0,
  SHD_POINTDENSITY_COLOR_VERTWEIGHT = 1,
  SHD_POINTDENSITY_COLOR_VERTNOR = 2,
};

/* Output shader node */

typedef enum NodeShaderOutputTarget {
  SHD_OUTPUT_ALL = 0,
  SHD_OUTPUT_EEVEE = 1,
  SHD_OUTPUT_CYCLES = 2,
} NodeShaderOutputTarget;

/* Geometry Nodes */

typedef enum GeometryNodeProximityTargetType {
  GEO_NODE_PROX_TARGET_POINTS = 0,
  GEO_NODE_PROX_TARGET_EDGES = 1,
  GEO_NODE_PROX_TARGET_FACES = 2,
} GeometryNodeProximityTargetType;

typedef enum GeometryNodeBooleanOperation {
  GEO_NODE_BOOLEAN_INTERSECT = 0,
  GEO_NODE_BOOLEAN_UNION = 1,
  GEO_NODE_BOOLEAN_DIFFERENCE = 2,
} GeometryNodeBooleanOperation;

typedef enum GeometryNodeCurvePrimitiveCircleMode {
  GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_POINTS = 0,
  GEO_NODE_CURVE_PRIMITIVE_CIRCLE_TYPE_RADIUS = 1
} GeometryNodeCurvePrimitiveCircleMode;

typedef enum GeometryNodeCurveHandleType {
  GEO_NODE_CURVE_HANDLE_FREE = 0,
  GEO_NODE_CURVE_HANDLE_AUTO = 1,
  GEO_NODE_CURVE_HANDLE_VECTOR = 2,
  GEO_NODE_CURVE_HANDLE_ALIGN = 3
} GeometryNodeCurveHandleType;

typedef enum GeometryNodeCurveHandleMode {
  GEO_NODE_CURVE_HANDLE_LEFT = (1 << 0),
  GEO_NODE_CURVE_HANDLE_RIGHT = (1 << 1)
} GeometryNodeCurveHandleMode;

typedef enum GeometryNodeTriangulateNGons {
  GEO_NODE_TRIANGULATE_NGON_BEAUTY = 0,
  GEO_NODE_TRIANGULATE_NGON_EARCLIP = 1,
} GeometryNodeTriangulateNGons;

typedef enum GeometryNodeTriangulateQuads {
  GEO_NODE_TRIANGULATE_QUAD_BEAUTY = 0,
  GEO_NODE_TRIANGULATE_QUAD_FIXED = 1,
  GEO_NODE_TRIANGULATE_QUAD_ALTERNATE = 2,
  GEO_NODE_TRIANGULATE_QUAD_SHORTEDGE = 3,
  GEO_NODE_TRIANGULATE_QUAD_LONGEDGE = 4,
} GeometryNodeTriangulateQuads;

typedef enum GeometryNodeDistributePointsInVolumeMode {
  GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_RANDOM = 0,
  GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME_DENSITY_GRID = 1,
} GeometryNodeDistributePointsInVolumeMode;

typedef enum GeometryNodeDistributePointsOnFacesMode {
  GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_RANDOM = 0,
  GEO_NODE_POINT_DISTRIBUTE_POINTS_ON_FACES_POISSON = 1,
} GeometryNodeDistributePointsOnFacesMode;

typedef enum GeometryNodeExtrudeMeshMode {
  GEO_NODE_EXTRUDE_MESH_VERTICES = 0,
  GEO_NODE_EXTRUDE_MESH_EDGES = 1,
  GEO_NODE_EXTRUDE_MESH_FACES = 2,
} GeometryNodeExtrudeMeshMode;

typedef enum FunctionNodeRotateEulerType {
  FN_NODE_ROTATE_EULER_TYPE_EULER = 0,
  FN_NODE_ROTATE_EULER_TYPE_AXIS_ANGLE = 1,
} FunctionNodeRotateEulerType;

typedef enum FunctionNodeRotateEulerSpace {
  FN_NODE_ROTATE_EULER_SPACE_OBJECT = 0,
  FN_NODE_ROTATE_EULER_SPACE_LOCAL = 1,
} FunctionNodeRotateEulerSpace;

typedef enum NodeAlignEulerToVectorAxis {
  FN_NODE_ALIGN_EULER_TO_VECTOR_AXIS_X = 0,
  FN_NODE_ALIGN_EULER_TO_VECTOR_AXIS_Y = 1,
  FN_NODE_ALIGN_EULER_TO_VECTOR_AXIS_Z = 2,
} NodeAlignEulerToVectorAxis;

typedef enum NodeAlignEulerToVectorPivotAxis {
  FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_AUTO = 0,
  FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_X = 1,
  FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_Y = 2,
  FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_Z = 3,
} NodeAlignEulerToVectorPivotAxis;

typedef enum GeometryNodeTransformSpace {
  GEO_NODE_TRANSFORM_SPACE_ORIGINAL = 0,
  GEO_NODE_TRANSFORM_SPACE_RELATIVE = 1,
} GeometryNodeTransformSpace;

typedef enum GeometryNodePointsToVolumeResolutionMode {
  GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT = 0,
  GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE = 1,
} GeometryNodePointsToVolumeResolutionMode;

typedef enum GeometryNodeMeshCircleFillType {
  GEO_NODE_MESH_CIRCLE_FILL_NONE = 0,
  GEO_NODE_MESH_CIRCLE_FILL_NGON = 1,
  GEO_NODE_MESH_CIRCLE_FILL_TRIANGLE_FAN = 2,
} GeometryNodeMeshCircleFillType;

typedef enum GeometryNodeMergeByDistanceMode {
  GEO_NODE_MERGE_BY_DISTANCE_MODE_ALL = 0,
  GEO_NODE_MERGE_BY_DISTANCE_MODE_CONNECTED = 1,
} GeometryNodeMergeByDistanceMode;

typedef enum GeometryNodeUVUnwrapMethod {
  GEO_NODE_UV_UNWRAP_METHOD_ANGLE_BASED = 0,
  GEO_NODE_UV_UNWRAP_METHOD_CONFORMAL = 1,
} GeometryNodeUVUnwrapMethod;

typedef enum GeometryNodeMeshLineMode {
  GEO_NODE_MESH_LINE_MODE_END_POINTS = 0,
  GEO_NODE_MESH_LINE_MODE_OFFSET = 1,
} GeometryNodeMeshLineMode;

typedef enum GeometryNodeMeshLineCountMode {
  GEO_NODE_MESH_LINE_COUNT_TOTAL = 0,
  GEO_NODE_MESH_LINE_COUNT_RESOLUTION = 1,
} GeometryNodeMeshLineCountMode;

typedef enum GeometryNodeCurvePrimitiveArcMode {
  GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_POINTS = 0,
  GEO_NODE_CURVE_PRIMITIVE_ARC_TYPE_RADIUS = 1,
} GeometryNodeCurvePrimitiveArcMode;

typedef enum GeometryNodeCurvePrimitiveLineMode {
  GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_POINTS = 0,
  GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_DIRECTION = 1
} GeometryNodeCurvePrimitiveLineMode;

typedef enum GeometryNodeCurvePrimitiveQuadMode {
  GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_RECTANGLE = 0,
  GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_PARALLELOGRAM = 1,
  GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_TRAPEZOID = 2,
  GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_KITE = 3,
  GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_POINTS = 4,
} GeometryNodeCurvePrimitiveQuadMode;

typedef enum GeometryNodeCurvePrimitiveBezierSegmentMode {
  GEO_NODE_CURVE_PRIMITIVE_BEZIER_SEGMENT_POSITION = 0,
  GEO_NODE_CURVE_PRIMITIVE_BEZIER_SEGMENT_OFFSET = 1,
} GeometryNodeCurvePrimitiveBezierSegmentMode;

typedef enum GeometryNodeCurveResampleMode {
  GEO_NODE_CURVE_RESAMPLE_COUNT = 0,
  GEO_NODE_CURVE_RESAMPLE_LENGTH = 1,
  GEO_NODE_CURVE_RESAMPLE_EVALUATED = 2,
} GeometryNodeCurveResampleMode;

typedef enum GeometryNodeCurveSampleMode {
  GEO_NODE_CURVE_SAMPLE_FACTOR = 0,
  GEO_NODE_CURVE_SAMPLE_LENGTH = 1,
} GeometryNodeCurveSampleMode;

typedef enum GeometryNodeCurveFilletMode {
  GEO_NODE_CURVE_FILLET_BEZIER = 0,
  GEO_NODE_CURVE_FILLET_POLY = 1,
} GeometryNodeCurveFilletMode;

typedef enum GeometryNodeAttributeTransferMode {
  GEO_NODE_ATTRIBUTE_TRANSFER_NEAREST_FACE_INTERPOLATED = 0,
  GEO_NODE_ATTRIBUTE_TRANSFER_NEAREST = 1,
  GEO_NODE_ATTRIBUTE_TRANSFER_INDEX = 2,
} GeometryNodeAttributeTransferMode;

typedef enum GeometryNodeRaycastMapMode {
  GEO_NODE_RAYCAST_INTERPOLATED = 0,
  GEO_NODE_RAYCAST_NEAREST = 1,
} GeometryNodeRaycastMapMode;

typedef enum GeometryNodeCurveFillMode {
  GEO_NODE_CURVE_FILL_MODE_TRIANGULATED = 0,
  GEO_NODE_CURVE_FILL_MODE_NGONS = 1,
} GeometryNodeCurveFillMode;

typedef enum GeometryNodeMeshToPointsMode {
  GEO_NODE_MESH_TO_POINTS_VERTICES = 0,
  GEO_NODE_MESH_TO_POINTS_EDGES = 1,
  GEO_NODE_MESH_TO_POINTS_FACES = 2,
  GEO_NODE_MESH_TO_POINTS_CORNERS = 3,
} GeometryNodeMeshToPointsMode;

typedef enum GeometryNodeStringToCurvesOverflowMode {
  GEO_NODE_STRING_TO_CURVES_MODE_OVERFLOW = 0,
  GEO_NODE_STRING_TO_CURVES_MODE_SCALE_TO_FIT = 1,
  GEO_NODE_STRING_TO_CURVES_MODE_TRUNCATE = 2,
} GeometryNodeStringToCurvesOverflowMode;

typedef enum GeometryNodeStringToCurvesAlignXMode {
  GEO_NODE_STRING_TO_CURVES_ALIGN_X_LEFT = 0,
  GEO_NODE_STRING_TO_CURVES_ALIGN_X_CENTER = 1,
  GEO_NODE_STRING_TO_CURVES_ALIGN_X_RIGHT = 2,
  GEO_NODE_STRING_TO_CURVES_ALIGN_X_JUSTIFY = 3,
  GEO_NODE_STRING_TO_CURVES_ALIGN_X_FLUSH = 4,
} GeometryNodeStringToCurvesAlignXMode;

typedef enum GeometryNodeStringToCurvesAlignYMode {
  GEO_NODE_STRING_TO_CURVES_ALIGN_Y_TOP_BASELINE = 0,
  GEO_NODE_STRING_TO_CURVES_ALIGN_Y_TOP = 1,
  GEO_NODE_STRING_TO_CURVES_ALIGN_Y_MIDDLE = 2,
  GEO_NODE_STRING_TO_CURVES_ALIGN_Y_BOTTOM_BASELINE = 3,
  GEO_NODE_STRING_TO_CURVES_ALIGN_Y_BOTTOM = 4,
} GeometryNodeStringToCurvesAlignYMode;

typedef enum GeometryNodeStringToCurvesPivotMode {
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_MIDPOINT = 0,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_LEFT = 1,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_CENTER = 2,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_RIGHT = 3,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_LEFT = 4,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_CENTER = 5,
  GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_RIGHT = 6,
} GeometryNodeStringToCurvesPivotMode;

typedef enum GeometryNodeDeleteGeometryMode {
  GEO_NODE_DELETE_GEOMETRY_MODE_ALL = 0,
  GEO_NODE_DELETE_GEOMETRY_MODE_EDGE_FACE = 1,
  GEO_NODE_DELETE_GEOMETRY_MODE_ONLY_FACE = 2,
} GeometryNodeDeleteGeometryMode;

typedef enum GeometryNodeScaleElementsMode {
  GEO_NODE_SCALE_ELEMENTS_UNIFORM = 0,
  GEO_NODE_SCALE_ELEMENTS_SINGLE_AXIS = 1,
} GeometryNodeScaleElementsMode;

typedef enum GeometryNodeSampleVolumeInterpolationMode {
  GEO_NODE_SAMPLE_VOLUME_INTERPOLATION_MODE_NEAREST = 0,
  GEO_NODE_SAMPLE_VOLUME_INTERPOLATION_MODE_TRILINEAR = 1,
  GEO_NODE_SAMPLE_VOLUME_INTERPOLATION_MODE_TRIQUADRATIC = 2,
} GeometryNodeSampleVolumeInterpolationMode;

typedef enum NodeCombSepColorMode {
  NODE_COMBSEP_COLOR_RGB = 0,
  NODE_COMBSEP_COLOR_HSV = 1,
  NODE_COMBSEP_COLOR_HSL = 2,
} NodeCombSepColorMode;
