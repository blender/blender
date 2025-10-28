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
#  include <string>

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
typedef struct bNodeTreeRuntimeHandle bNodeTreeRuntimeHandle;
typedef struct bNodeRuntimeHandle bNodeRuntimeHandle;
typedef struct bNodeSocketRuntimeHandle bNodeSocketRuntimeHandle;
typedef struct RuntimeNodeEnumItemsHandle RuntimeNodeEnumItemsHandle;
typedef struct NodeInstanceHashHandle NodeInstanceHashHandle;
typedef struct bNodeTreeTypeHandle bNodeTreeTypeHandle;
typedef struct bNodeTypeHandle bNodeTypeHandle;
typedef struct bNodeSocketTypeHandle bNodeSocketTypeHandle;
#endif

struct AnimData;
struct Collection;
struct GeometryNodeAssetTraits;
struct ID;
struct Image;
struct ImBuf;
struct ListBase;
struct Material;
struct PreviewImage;
struct Tex;
struct bGPdata;
struct bNodeLink;
struct bNode;
struct NodeEnumDefinition;

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

typedef struct bNodeSocket {
  struct bNodeSocket *next, *prev;

  /** User-defined properties. */
  IDProperty *prop;

  /** Unique identifier for mapping. */
  char identifier[64];

  char name[/*MAX_NAME*/ 64];

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
  bNodeSocketTypeHandle *typeinfo;
  /** Runtime type identifier. */
  char idname[64];

  /** Default input value used for unlinked sockets. */
  void *default_value;

  /** Local stack index for "node_exec". */
  int stack_index;
  char display_shape;

  /* #AttrDomain used when the geometry nodes modifier creates an attribute for a group
   * output. */
  char attribute_domain;

  char _pad[2];

  /** Custom dynamic defined label. */
  char label[/*MAX_NAME*/ 64];
  char description[/*MAX_NAME*/ 64];

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

  /** A link pointer, set in #BKE_ntree_update. */
  struct bNodeLink *link;

  /* XXX deprecated, socket input values are stored in default_value now.
   * kept for forward compatibility */
  /** Custom data for inputs, only UI writes in this. */
  bNodeStack ns DNA_DEPRECATED;

  bNodeSocketRuntimeHandle *runtime;

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
} bNodeSocket;

/** #bNodeSocket.type & #bNodeSocketType.type */
typedef enum eNodeSocketDatatype {
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
} eNodeSocketDatatype;

/** Socket shape. */
typedef enum eNodeSocketDisplayShape {
  SOCK_DISPLAY_SHAPE_CIRCLE = 0,
  SOCK_DISPLAY_SHAPE_SQUARE = 1,
  SOCK_DISPLAY_SHAPE_DIAMOND = 2,
  SOCK_DISPLAY_SHAPE_CIRCLE_DOT = 3,
  SOCK_DISPLAY_SHAPE_SQUARE_DOT = 4,
  SOCK_DISPLAY_SHAPE_DIAMOND_DOT = 5,
  SOCK_DISPLAY_SHAPE_LINE = 6,
  SOCK_DISPLAY_SHAPE_VOLUME_GRID = 7,
  SOCK_DISPLAY_SHAPE_LIST = 8,
} eNodeSocketDisplayShape;

/** Socket side (input/output). */
typedef enum eNodeSocketInOut {
  SOCK_IN = 1 << 0,
  SOCK_OUT = 1 << 1,
} eNodeSocketInOut;
ENUM_OPERATORS(eNodeSocketInOut);

/** #bNodeSocket.flag, first bit is selection. */
typedef enum eNodeSocketFlag {
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
} eNodeSocketFlag;

typedef enum eNodePanelFlag {
  /* Panel is collapsed (user setting). */
  NODE_PANEL_COLLAPSED = (1 << 0),
  /* The parent panel is collapsed. */
  NODE_PANEL_PARENT_COLLAPSED = (1 << 1),
  /* The panel has visible content. */
  NODE_PANEL_CONTENT_VISIBLE = (1 << 2),
} eNodePanelFlag;

typedef struct bNodePanelState {
  /* Unique identifier for validating state against panels in node declaration. */
  int identifier;
  /* eNodePanelFlag */
  char flag;
  char _pad[3];

#ifdef __cplusplus
  bool is_collapsed() const;
  bool is_parent_collapsed() const;
  bool has_visible_content() const;
#endif
} bNodePanelState;

typedef enum eViewerNodeShortcut {
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
} eViewerNodeShortcut;

typedef enum NodeWarningPropagation {
  NODE_WARNING_PROPAGATION_ALL = 0,
  NODE_WARNING_PROPAGATION_NONE = 1,
  NODE_WARNING_PROPAGATION_ONLY_ERRORS = 2,
  NODE_WARNING_PROPAGATION_ONLY_ERRORS_AND_WARNINGS = 3,
} NodeWarningPropagation;

typedef struct bNode {
  struct bNode *next, *prev;

  /* Input and output #bNodeSocket. */
  ListBase inputs, outputs;

  /** The node's name for unique identification and string lookup. */
  char name[/*MAX_NAME*/ 64];

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
  bNodeTypeHandle *typeinfo;

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
  int16_t type_legacy;

  /**
   * Depth of the node in the node editor, used to keep recently selected nodes at the front, and
   * to order frame nodes properly.
   */
  int16_t ui_order;

  /** Used for some builtin nodes that store properties but don't have a storage struct. */
  int16_t custom1, custom2;
  float custom3, custom4;

  /**
   * #NodeWarningPropagation.
   */
  int8_t warning_propagation;
  char _pad[7];

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

  /**
   * System-defined properties, used e.g. to store data for custom node types.
   */
  IDProperty *system_properties;

  /** Parent node (for frame nodes). */
  struct bNode *parent;

  /** The location of the top left corner of the node on the canvas. */
  float location[2];
  /**
   * Custom width and height controlled by users. Height is calculate automatically for most
   * nodes.
   */
  float width, height;
  float locx_legacy, locy_legacy;
  float offsetx_legacy, offsety_legacy;

  /** Custom user-defined label. */
  char label[/*MAX_NAME*/ 64];

  /** Custom user-defined color. */
  float color[3];

  /** Panel states for this node instance. */
  int num_panel_states;
  bNodePanelState *panel_states_array;

  bNodeRuntimeHandle *runtime;

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
} bNode;

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

/**
 * Unique hash key for identifying node instances
 * Defined as a struct because DNA does not support other typedefs.
 */
typedef struct bNodeInstanceKey {
  unsigned int value;

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
} bNodeInstanceKey;

/**
 * Base struct for entries in node instance hash.
 *
 * \warning pointers are cast to this struct internally,
 * it must be first member in hash entry structs!
 */
#
#
typedef struct bNodeInstanceHashEntry {
  bNodeInstanceKey key;

  /** Tags for cleaning the cache. */
  short tag;
} bNodeInstanceHashEntry;

typedef struct bNodeLink {
  struct bNodeLink *next, *prev;

  bNode *fromnode, *tonode;
  bNodeSocket *fromsock, *tosock;

  int flag;
  /**
   * Determines the order in which links are connected to a multi-input socket.
   * For historical reasons, larger ids come before lower ids.
   * Usually, this should not be accessed directly. One can instead use e.g.
   * `socket.directly_linked_links()` to get the links in the correct order.
   */
  int multi_input_sort_id;

#ifdef __cplusplus
  bool is_muted() const;
  bool is_available() const;
  /** Both linked sockets are available and the link is not muted. */
  bool is_used() const;
#endif

} bNodeLink;

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

typedef struct bNestedNodePath {
  /** ID of the node that is or contains the nested node. */
  int32_t node_id;
  /** Unused if the node is the final nested node, otherwise an id inside of the (group) node. */
  int32_t id_in_node;

#ifdef __cplusplus
  uint64_t hash() const;
  friend bool operator==(const bNestedNodePath &a, const bNestedNodePath &b);
#endif
} bNestedNodePath;

typedef struct bNestedNodeRef {
  /** Identifies a potentially nested node. This ID remains stable even if the node is moved into
   * and out of node groups. */
  int32_t id;
  char _pad[4];
  /** Where to find the nested node in the current node tree. */
  bNestedNodePath path;
} bNestedNodeRef;

/**
 * The basis for a Node tree, all links and nodes reside internal here.
 *
 * Only re-usable node trees are in the library though,
 * materials and textures allocate their own tree struct.
 */
typedef struct bNodeTree {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_NT;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /** The ID owning this node tree, in case it is an embedded one. */
  ID *owner_id;

  /** Runtime type information. */
  bNodeTreeTypeHandle *typeinfo;
  /** Runtime type identifier. */
  char idname[64];
  /** User-defined description of the node tree. */
  char *description;

  /** Grease pencil data. */
  struct bGPdata *gpd;
  /** Node tree stores its own offset for consistent editor view. */
  float view_center[2];

  ListBase nodes, links;

  int type;

  /**
   * Sockets in groups have unique identifiers, adding new sockets always
   * will increase this counter.
   */
  int cur_index;
  int flag;

  /** Tile size for compositor engine. */
  int chunksize DNA_DEPRECATED;
  /** Execution mode to use for compositor engine. */
  int execution_mode DNA_DEPRECATED;
  /** Precision used by the GPU execution of the compositor tree. */
  int precision DNA_DEPRECATED;

  /** #blender::bke::NodeColorTag. */
  int color_tag;

  /**
   * Default width of a group node created for this group. May be zero, in which case this value
   * should be ignored.
   */
  int default_group_node_width;

  rctf viewer_border;

  /**
   * Lists of #bNodeSocket to hold default values and own_index.
   * Warning! Don't make links to these sockets, input/output nodes are used for that.
   * These sockets are used only for generating external interfaces.
   */
  ListBase inputs_legacy DNA_DEPRECATED, outputs_legacy DNA_DEPRECATED;

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
  int nested_node_refs_num;
  bNestedNodeRef *nested_node_refs;

  struct GeometryNodeAssetTraits *geometry_node_asset_traits;

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
} bNodeTree;

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
  /* Only some of the values might be used depending on the dimensions. */
  float value[4];
  float min, max;
  /* The number of dimensions of the vector. Can be 2, 3, or 4. */
  int dimensions;
} bNodeSocketValueVector;

typedef struct bNodeSocketValueRotation {
  float value_euler[3];
} bNodeSocketValueRotation;

typedef struct bNodeSocketValueRGBA {
  float value[4];
} bNodeSocketValueRGBA;

typedef struct bNodeSocketValueString {
  int subtype;
  char _pad[4];
  char value[/*FILE_MAX*/ 1024];
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

typedef struct bNodeSocketValueMenu {
  /* Default input enum identifier. */
  int value;
  /* #NodeSocketValueMenuRuntimeFlag */
  int runtime_flag;
  /* Immutable runtime enum definition. */
  const RuntimeNodeEnumItemsHandle *enum_items;

#ifdef __cplusplus
  bool has_conflict() const;
#endif
} bNodeSocketValueMenu;

typedef struct GeometryNodeAssetTraits {
  int flag;
} GeometryNodeAssetTraits;

typedef enum GeometryNodeAssetTraitFlag {
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
} GeometryNodeAssetTraitFlag;
ENUM_OPERATORS(GeometryNodeAssetTraitFlag);

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

  /** We may want multiple aspect options, exposed as an rna enum. */
  CMP_NODE_MASK_FLAG_SIZE_FIXED = (1 << 8),
  CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE = (1 << 9),
} CMPNodeMaskFlags;

typedef struct NodeFrame {
  short flag;
  short label_size;
} NodeFrame;

typedef struct NodeReroute {
  /** Name of the socket type (e.g. `NodeSocketFloat`). */
  char type_idname[64];

} NodeReroute;

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
  float saturation DNA_DEPRECATED;
  float contrast DNA_DEPRECATED;
  float gamma DNA_DEPRECATED;
  float gain DNA_DEPRECATED;
  float lift DNA_DEPRECATED;
  char _pad[4];
} ColorCorrectionData;

typedef struct NodeColorCorrection {
  ColorCorrectionData master DNA_DEPRECATED;
  ColorCorrectionData shadows DNA_DEPRECATED;
  ColorCorrectionData midtones DNA_DEPRECATED;
  ColorCorrectionData highlights DNA_DEPRECATED;
  float startmidtones DNA_DEPRECATED;
  float endmidtones DNA_DEPRECATED;
} NodeColorCorrection;

typedef struct NodeBokehImage {
  float angle DNA_DEPRECATED;
  int flaps DNA_DEPRECATED;
  float rounding DNA_DEPRECATED;
  float catadioptric DNA_DEPRECATED;
  float lensshift DNA_DEPRECATED;
} NodeBokehImage;

typedef struct NodeBoxMask {
  float x DNA_DEPRECATED;
  float y DNA_DEPRECATED;
  float rotation DNA_DEPRECATED;
  float height DNA_DEPRECATED;
  float width DNA_DEPRECATED;
  char _pad[4];
} NodeBoxMask;

typedef struct NodeEllipseMask {
  float x DNA_DEPRECATED;
  float y DNA_DEPRECATED;
  float rotation DNA_DEPRECATED;
  float height DNA_DEPRECATED;
  float width DNA_DEPRECATED;
  char _pad[4];
} NodeEllipseMask;

/** Layer info for image node outputs. */
typedef struct NodeImageLayer {
  /** Index in the `image->layers->passes` lists. */
  int pass_index DNA_DEPRECATED;
  /* render pass name */
  /** Amount defined in IMB_openexr.hh. */
  char pass_name[64];
} NodeImageLayer;

typedef struct NodeBlurData {
  short sizex DNA_DEPRECATED;
  short sizey DNA_DEPRECATED;
  short samples DNA_DEPRECATED;
  short maxspeed DNA_DEPRECATED;
  short minspeed DNA_DEPRECATED;
  short relative DNA_DEPRECATED;
  short aspect DNA_DEPRECATED;
  short curved DNA_DEPRECATED;
  float fac DNA_DEPRECATED;
  float percentx DNA_DEPRECATED;
  float percenty DNA_DEPRECATED;
  short filtertype DNA_DEPRECATED;
  char bokeh DNA_DEPRECATED;
  char gamma DNA_DEPRECATED;
} NodeBlurData;

typedef struct NodeDBlurData {
  float center_x DNA_DEPRECATED;
  float center_y DNA_DEPRECATED;
  float distance DNA_DEPRECATED;
  float angle DNA_DEPRECATED;
  float spin DNA_DEPRECATED;
  float zoom DNA_DEPRECATED;
  short iter DNA_DEPRECATED;
  char _pad[2];
} NodeDBlurData;

typedef struct NodeBilateralBlurData {
  float sigma_color DNA_DEPRECATED;
  float sigma_space DNA_DEPRECATED;
  short iter DNA_DEPRECATED;
  char _pad[2];
} NodeBilateralBlurData;

typedef struct NodeKuwaharaData {
  short size DNA_DEPRECATED;
  short variation DNA_DEPRECATED;
  int uniformity DNA_DEPRECATED;
  float sharpness DNA_DEPRECATED;
  float eccentricity DNA_DEPRECATED;
  char high_precision DNA_DEPRECATED;
  char _pad[3];
} NodeKuwaharaData;

typedef struct NodeAntiAliasingData {
  float threshold DNA_DEPRECATED;
  float contrast_limit DNA_DEPRECATED;
  float corner_rounding DNA_DEPRECATED;
} NodeAntiAliasingData;

/** \note Only for do-version code. */
typedef struct NodeHueSat {
  float hue DNA_DEPRECATED;
  float sat DNA_DEPRECATED;
  float val DNA_DEPRECATED;
} NodeHueSat;

typedef struct NodeImageFile {
  char name[/*FILE_MAX*/ 1024];
  struct ImageFormatData im_format;
  int sfra, efra;
} NodeImageFile;

typedef struct NodeCompositorFileOutputItem {
  /* The unique identifier of the item used to construct the socket identifier. */
  int identifier;
  /* The type of socket for the item, which is limited to the types listed in the
   * FileOutputItemsAccessor::supports_socket_type. */
  int16_t socket_type;
  /* The number of dimensions in the vector socket if the socket type is vector, otherwise, it is
   * unused, */
  char vector_socket_dimensions;
  /* If true and the node is saving individual files, the format an save_as_render members of this
   * struct will be used, otherwise, the members of the NodeCompositorFileOutput struct will be
   * used for all items. */
  char override_node_format;
  /* Apply the render part of the display transform when saving non-linear images. Unused if
   * override_node_format is false or the node is saving multi-layer images. */
  char save_as_render;
  char _pad[7];
  /* The unique name of the item. It is used as the file name when saving individual files and used
   * as the layer name when saving multi-layer images. */
  char *name;
  /* The image format to use when saving individual images and override_node_format is true. */
  ImageFormatData format;
} NodeCompositorFileOutputItem;

typedef struct NodeCompositorFileOutput {
  char directory[/*FILE_MAX*/ 1024];
  /* The base name of the file. Can be nullptr. */
  char *file_name;
  /* The image format to use when saving the images. */
  ImageFormatData format;
  /* The file output images. They can represent individual images or layers depending on whether
   * multi-layer images are being saved. */
  NodeCompositorFileOutputItem *items;
  /* The number of file output items. */
  int items_count;
  /* The currently active file output item. */
  int active_item_index;
  /* Apply the render part of the display transform when saving non-linear images. */
  char save_as_render;
  char _pad[7];
} NodeCompositorFileOutput;

typedef struct NodeImageMultiFileSocket {
  /* single layer file output */
  short use_render_format DNA_DEPRECATED;
  /** Use overall node image format. */
  short use_node_format DNA_DEPRECATED;
  char save_as_render DNA_DEPRECATED;
  char _pad1[3];
  char path[/*FILE_MAX*/ 1024] DNA_DEPRECATED;
  ImageFormatData format DNA_DEPRECATED;

  /* Multi-layer output. */
  /** Subtract 2 because '.' and channel char are appended. */
  char layer[/*EXR_TOT_MAXNAME - 2*/ 62] DNA_DEPRECATED;
  char _pad2[2];
} NodeImageMultiFileSocket;

typedef struct NodeChroma {
  float t1 DNA_DEPRECATED;
  float t2 DNA_DEPRECATED;
  float t3 DNA_DEPRECATED;
  float fsize DNA_DEPRECATED;
  float fstrength DNA_DEPRECATED;
  float falpha DNA_DEPRECATED;
  float key[4] DNA_DEPRECATED;
  short algorithm DNA_DEPRECATED;
  short channel DNA_DEPRECATED;
} NodeChroma;

typedef struct NodeTwoXYs {
  short x1 DNA_DEPRECATED;
  short x2 DNA_DEPRECATED;
  short y1 DNA_DEPRECATED;
  short y2 DNA_DEPRECATED;
  float fac_x1 DNA_DEPRECATED;
  float fac_x2 DNA_DEPRECATED;
  float fac_y1 DNA_DEPRECATED;
  float fac_y2 DNA_DEPRECATED;
} NodeTwoXYs;

typedef struct NodeTwoFloats {
  float x DNA_DEPRECATED;
  float y DNA_DEPRECATED;
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
  char bktype;
  char gamco DNA_DEPRECATED;
  char no_zbuf;
  char _pad0;
  float fstop;
  float maxblur;
  float scale;
  float rotation;
} NodeDefocus;

typedef struct NodeScriptDict {
  /** For PyObject *dict. */
  void *dict;
  /** For BPy_Node *node. */
  void *node;
} NodeScriptDict;

/** glare node. */
typedef struct NodeGlare {
  char type DNA_DEPRECATED;
  char quality DNA_DEPRECATED;
  char iter DNA_DEPRECATED;
  char angle DNA_DEPRECATED;
  char _pad0;
  char size DNA_DEPRECATED;
  char star_45 DNA_DEPRECATED;
  char streaks DNA_DEPRECATED;
  float colmod DNA_DEPRECATED;
  float mix DNA_DEPRECATED;
  float threshold DNA_DEPRECATED;
  float fade DNA_DEPRECATED;
  float angle_ofs DNA_DEPRECATED;
  char _pad1[4];
} NodeGlare;

/* Glare Node. Stored in NodeGlare.quality. */
typedef enum CMPNodeGlareQuality {
  CMP_NODE_GLARE_QUALITY_HIGH = 0,
  CMP_NODE_GLARE_QUALITY_MEDIUM = 1,
  CMP_NODE_GLARE_QUALITY_LOW = 2,
} CMPNodeGlareQuality;

/** Tone-map node. */
typedef struct NodeTonemap {
  float key DNA_DEPRECATED;
  float offset DNA_DEPRECATED;
  float gamma DNA_DEPRECATED;
  float f DNA_DEPRECATED;
  float m DNA_DEPRECATED;
  float a DNA_DEPRECATED;
  float c DNA_DEPRECATED;
  int type DNA_DEPRECATED;
} NodeTonemap;

/* Lens Distortion node. */
typedef struct NodeLensDist {
  short jit DNA_DEPRECATED;
  short proj DNA_DEPRECATED;
  short fit DNA_DEPRECATED;
  char _pad[2];
  int distortion_type DNA_DEPRECATED;
} NodeLensDist;

typedef struct NodeColorBalance {
  /* ASC CDL parameters. */
  float slope[3] DNA_DEPRECATED;
  float offset[3] DNA_DEPRECATED;
  float power[3] DNA_DEPRECATED;
  float offset_basis DNA_DEPRECATED;
  char _pad[4];

  /* LGG parameters. */
  float lift[3] DNA_DEPRECATED;
  float gamma[3] DNA_DEPRECATED;
  float gain[3] DNA_DEPRECATED;

  /* White-point parameters. */
  float input_temperature DNA_DEPRECATED;
  float input_tint DNA_DEPRECATED;
  float output_temperature DNA_DEPRECATED;
  float output_tint DNA_DEPRECATED;
} NodeColorBalance;

typedef struct NodeColorspill {
  short limchan DNA_DEPRECATED;
  short unspill DNA_DEPRECATED;
  float limscale DNA_DEPRECATED;
  float uspillr DNA_DEPRECATED;
  float uspillg DNA_DEPRECATED;
  float uspillb DNA_DEPRECATED;
} NodeColorspill;

typedef struct NodeConvertColorSpace {
  char from_color_space[64];
  char to_color_space[64];
} NodeConvertColorSpace;

typedef struct NodeConvertToDisplay {
  ColorManagedDisplaySettings display_settings;
  ColorManagedViewSettings view_settings;
} NodeConvertToDisplay;

typedef struct NodeDilateErode {
  char falloff;
} NodeDilateErode;

typedef struct NodeMask {
  int size_x DNA_DEPRECATED;
  int size_y DNA_DEPRECATED;
} NodeMask;

typedef struct NodeSetAlpha {
  char mode DNA_DEPRECATED;
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
  float aerosol_density;
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

typedef struct NodeTexGabor {
  NodeTexBase base;
  /* Stores NodeGaborType. */
  char type;
  char _pad[7];
} NodeTexGabor;

typedef struct NodeTexGradient {
  NodeTexBase base;
  int gradient_type;
  char _pad[4];
} NodeTexGradient;

typedef struct NodeTexNoise {
  NodeTexBase base;
  int dimensions;
  uint8_t type;
  uint8_t normalize;
  char _pad[2];
} NodeTexNoise;

typedef struct NodeTexVoronoi {
  NodeTexBase base;
  int dimensions;
  int feature;
  int distance;
  int normalize;
  int coloring DNA_DEPRECATED;
  char _pad[4];
} NodeTexVoronoi;

typedef struct NodeTexMusgrave {
  NodeTexBase base DNA_DEPRECATED;
  int musgrave_type DNA_DEPRECATED;
  int dimensions DNA_DEPRECATED;
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
  char name[256];
  int type;
  char _pad[4];
} NodeShaderAttribute;

typedef struct NodeShaderVectTransform {
  int type;
  int convert_from, convert_to;
  char _pad[4];
} NodeShaderVectTransform;

typedef struct NodeShaderPrincipled {
  char use_subsurface_auto_radius;
  char _pad[3];
} NodeShaderPrincipled;

typedef struct NodeShaderHairPrincipled {
  short model;
  short parametrization;
  char _pad[4];
} NodeShaderHairPrincipled;

/** TEX_output. */
typedef struct TexNodeOutput {
  char name[64];
} TexNodeOutput;

typedef struct NodeKeyingScreenData {
  char tracking_object[/*MAX_NAME*/ 64];
  float smoothness DNA_DEPRECATED;
} NodeKeyingScreenData;

typedef struct NodeKeyingData {
  float screen_balance DNA_DEPRECATED;
  float despill_factor DNA_DEPRECATED;
  float despill_balance DNA_DEPRECATED;
  int edge_kernel_radius DNA_DEPRECATED;
  float edge_kernel_tolerance DNA_DEPRECATED;
  float clip_black DNA_DEPRECATED;
  float clip_white DNA_DEPRECATED;
  int dilate_distance DNA_DEPRECATED;
  int feather_distance DNA_DEPRECATED;
  int feather_falloff DNA_DEPRECATED;
  int blur_pre DNA_DEPRECATED;
  int blur_post DNA_DEPRECATED;
} NodeKeyingData;

typedef struct NodeTrackPosData {
  char tracking_object[/*MAX_NAME*/ 64];
  char track_name[64];
} NodeTrackPosData;

typedef struct NodeTransformData {
  short interpolation DNA_DEPRECATED;
  char extension_x DNA_DEPRECATED;
  char extension_y DNA_DEPRECATED;
} NodeTransformData;

typedef struct NodeTranslateData {
  char wrap_axis DNA_DEPRECATED;
  char relative DNA_DEPRECATED;
  short extension_x DNA_DEPRECATED;
  short extension_y DNA_DEPRECATED;
  short interpolation DNA_DEPRECATED;
} NodeTranslateData;

typedef struct NodeRotateData {
  short interpolation DNA_DEPRECATED;
  char extension_x DNA_DEPRECATED;
  char extension_y DNA_DEPRECATED;
} NodeRotateData;

typedef struct NodeScaleData {
  short interpolation DNA_DEPRECATED;
  char extension_x DNA_DEPRECATED;
  char extension_y DNA_DEPRECATED;
} NodeScaleData;

typedef struct NodeCornerPinData {
  short interpolation DNA_DEPRECATED;
  char extension_x DNA_DEPRECATED;
  char extension_y DNA_DEPRECATED;
} NodeCornerPinData;

typedef struct NodeDisplaceData {
  short interpolation DNA_DEPRECATED;
  char extension_x DNA_DEPRECATED;
  char extension_y DNA_DEPRECATED;
} NodeDisplaceData;

typedef struct NodeMapUVData {
  short interpolation DNA_DEPRECATED;
  char extension_x DNA_DEPRECATED;
  char extension_y DNA_DEPRECATED;
} NodeMapUVData;

typedef struct NodePlaneTrackDeformData {
  char tracking_object[64];
  char plane_track_name[64];
  char flag DNA_DEPRECATED;
  char motion_blur_samples DNA_DEPRECATED;
  char _pad[2];
  float motion_blur_shutter DNA_DEPRECATED;
} NodePlaneTrackDeformData;

typedef struct NodeShaderScript {
  int mode;
  int flag;

  char filepath[/*FILE_MAX*/ 1024];

  char bytecode_hash[64];
  char *bytecode;
} NodeShaderScript;

typedef struct NodeShaderTangent {
  int direction_type;
  int axis;
  char uv_map[/*MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX*/ 64];
} NodeShaderTangent;

typedef struct NodeShaderNormalMap {
  int space;
  char uv_map[/*MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX*/ 64];
} NodeShaderNormalMap;

typedef struct NodeRadialTiling {
  uint8_t normalize;
  char _pad[7];
} NodeRadialTiling;

typedef struct NodeShaderUVMap {
  char uv_map[/*MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX*/ 64];
} NodeShaderUVMap;

typedef struct NodeShaderVertexColor {
  char layer_name[/*MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX*/ 64];
} NodeShaderVertexColor;

typedef struct NodeShaderTexIES {
  int mode;

  char filepath[/*FILE_MAX*/ 1024];
} NodeShaderTexIES;

typedef struct NodeShaderOutputAOV {
  char name[/*MAX_NAME*/ 64];
} NodeShaderOutputAOV;

typedef struct NodeSunBeams {
  float source[2] DNA_DEPRECATED;
  float ray_length DNA_DEPRECATED;
} NodeSunBeams;

typedef struct CryptomatteEntry {
  struct CryptomatteEntry *next, *prev;
  float encoded_hash;
  char name[/*MAX_NAME*/ 64];
  char _pad[4];
} CryptomatteEntry;

typedef struct CryptomatteLayer {
  struct CryptomatteEntry *next, *prev;
  char name[64];
} CryptomatteLayer;

typedef struct NodeCryptomatte_Runtime {
  /** Contains #CryptomatteLayer. */
  ListBase layers;
  /** Temp storage for the crypto-matte picker. */
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

  /** Contains #CryptomatteEntry. */
  ListBase entries;

  char layer_name[/*MAX_NAME*/ 64];
  /** Stores `entries` as a string for opening in 2.80-2.91. */
  char *matte_id;

  /* Legacy attributes. */
  /** Number of input sockets. */
  int inputs_num;

  char _pad[4];
  NodeCryptomatte_Runtime runtime;
} NodeCryptomatte;

typedef struct NodeDenoise {
  char hdr DNA_DEPRECATED;
  char prefilter DNA_DEPRECATED;
  char quality DNA_DEPRECATED;
  char _pad[1];
} NodeDenoise;

typedef struct NodeMapRange {
  /** #eCustomDataType */
  uint8_t data_type;

  /** #NodeMapRangeType. */
  uint8_t interpolation_type;
  uint8_t clamp;
  char _pad[5];
} NodeMapRange;

typedef struct NodeRandomValue {
  /** #eCustomDataType. */
  uint8_t data_type;
} NodeRandomValue;

typedef struct NodeAccumulateField {
  /** #eCustomDataType. */
  uint8_t data_type;
  /** #AttrDomain. */
  uint8_t domain;
} NodeAccumulateField;

typedef struct NodeInputBool {
  uint8_t boolean;
} NodeInputBool;

typedef struct NodeInputInt {
  int integer;
} NodeInputInt;

typedef struct NodeInputRotation {
  float rotation_euler[3];
} NodeInputRotation;

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
  /** #GeometryNodeExtrudeMeshMode */
  uint8_t mode;
} NodeGeometryExtrudeMesh;

typedef struct NodeGeometryObjectInfo {
  /** #GeometryNodeTransformSpace. */
  uint8_t transform_space;
} NodeGeometryObjectInfo;

typedef struct NodeGeometryPointsToVolume {
  /** #GeometryNodePointsToVolumeResolutionMode */
  uint8_t resolution_mode;
} NodeGeometryPointsToVolume;

typedef struct NodeGeometryCollectionInfo {
  /** #GeometryNodeTransformSpace. */
  uint8_t transform_space;
} NodeGeometryCollectionInfo;

typedef struct NodeGeometryProximity {
  /** #GeometryNodeProximityTargetType. */
  uint8_t target_element;
} NodeGeometryProximity;

typedef struct NodeGeometryVolumeToMesh {
  /** #VolumeToMeshResolutionMode */
  uint8_t resolution_mode;
} NodeGeometryVolumeToMesh;

typedef struct NodeGeometryMeshToVolume {
  /** #MeshToVolumeModifierResolutionMode */
  uint8_t resolution_mode;
} NodeGeometryMeshToVolume;

typedef struct NodeGeometrySubdivisionSurface {
  /** #eSubsurfUVSmooth. */
  uint8_t uv_smooth;
  /** #eSubsurfBoundarySmooth. */
  uint8_t boundary_smooth;
} NodeGeometrySubdivisionSurface;

typedef struct NodeGeometryMeshCircle {
  /** #GeometryNodeMeshCircleFillType. */
  uint8_t fill_type;
} NodeGeometryMeshCircle;

typedef struct NodeGeometryMeshCylinder {
  /** #GeometryNodeMeshCircleFillType. */
  uint8_t fill_type;
} NodeGeometryMeshCylinder;

typedef struct NodeGeometryMeshCone {
  /** #GeometryNodeMeshCircleFillType. */
  uint8_t fill_type;
} NodeGeometryMeshCone;

typedef struct NodeGeometryMergeByDistance {
  /** #GeometryNodeMergeByDistanceMode. */
  uint8_t mode;
} NodeGeometryMergeByDistance;

typedef struct NodeGeometryMeshLine {
  /** #GeometryNodeMeshLineMode. */
  uint8_t mode;
  /** #GeometryNodeMeshLineCountMode. */
  uint8_t count_mode;
} NodeGeometryMeshLine;

typedef struct NodeSwitch {
  /** #eNodeSocketDatatype. */
  uint8_t input_type;
} NodeSwitch;

typedef struct NodeEnumItem {
  char *name;
  char *description;
  /* Immutable unique identifier. */
  int32_t identifier;
  char _pad[4];
} NodeEnumItem;

typedef struct NodeEnumDefinition {
  /* User-defined enum items owned and managed by this node. */
  NodeEnumItem *items_array;
  int items_num;
  int active_index;
  uint32_t next_identifier;
  char _pad[4];

#ifdef __cplusplus
  blender::Span<NodeEnumItem> items() const;
  blender::MutableSpan<NodeEnumItem> items();
#endif
} NodeEnumDefinition;

typedef struct NodeMenuSwitch {
  NodeEnumDefinition enum_definition;

  /** #eNodeSocketDatatype. */
  uint8_t data_type;
  char _pad[7];
} NodeMenuSwitch;

typedef struct NodeGeometryCurveSplineType {
  /** #GeometryNodeSplineType. */
  uint8_t spline_type;
} NodeGeometryCurveSplineType;

typedef struct NodeGeometrySetCurveHandlePositions {
  /** #GeometryNodeCurveHandleMode. */
  uint8_t mode;
} NodeGeometrySetCurveHandlePositions;

typedef struct NodeGeometryCurveSetHandles {
  /** #GeometryNodeCurveHandleType. */
  uint8_t handle_type;
  /** #GeometryNodeCurveHandleMode. */
  uint8_t mode;
} NodeGeometryCurveSetHandles;

typedef struct NodeGeometryCurveSelectHandles {
  /** #GeometryNodeCurveHandleType. */
  uint8_t handle_type;
  /** #GeometryNodeCurveHandleMode. */
  uint8_t mode;
} NodeGeometryCurveSelectHandles;

typedef struct NodeGeometryCurvePrimitiveArc {
  /** #GeometryNodeCurvePrimitiveArcMode. */
  uint8_t mode;
} NodeGeometryCurvePrimitiveArc;

typedef struct NodeGeometryCurvePrimitiveLine {
  /** #GeometryNodeCurvePrimitiveLineMode. */
  uint8_t mode;
} NodeGeometryCurvePrimitiveLine;

typedef struct NodeGeometryCurvePrimitiveBezierSegment {
  /** #GeometryNodeCurvePrimitiveBezierSegmentMode. */
  uint8_t mode;
} NodeGeometryCurvePrimitiveBezierSegment;

typedef struct NodeGeometryCurvePrimitiveCircle {
  /** #GeometryNodeCurvePrimitiveMode. */
  uint8_t mode;
} NodeGeometryCurvePrimitiveCircle;

typedef struct NodeGeometryCurvePrimitiveQuad {
  /** #GeometryNodeCurvePrimitiveQuadMode. */
  uint8_t mode;
} NodeGeometryCurvePrimitiveQuad;

typedef struct NodeGeometryCurveResample {
  /** #GeometryNodeCurveResampleMode. */
  uint8_t mode;
  /**
   * If false, curves may be collapsed to a single point. This is unexpected and is only supported
   * for compatibility reasons (#102598).
   */
  uint8_t keep_last_segment;
} NodeGeometryCurveResample;

typedef struct NodeGeometryCurveFillet {
  /** #GeometryNodeCurveFilletMode. */
  uint8_t mode;
} NodeGeometryCurveFillet;

typedef struct NodeGeometryCurveTrim {
  /** #GeometryNodeCurveSampleMode. */
  uint8_t mode;
} NodeGeometryCurveTrim;

typedef struct NodeGeometryCurveToPoints {
  /** #GeometryNodeCurveResampleMode. */
  uint8_t mode;
} NodeGeometryCurveToPoints;

typedef struct NodeGeometryCurveSample {
  /** #GeometryNodeCurveSampleMode. */
  uint8_t mode;
  int8_t use_all_curves;
  /** #eCustomDataType. */
  int8_t data_type;
  char _pad[1];
} NodeGeometryCurveSample;

typedef struct NodeGeometryTransferAttribute {
  /** #eCustomDataType. */
  int8_t data_type;
  /** #AttrDomain. */
  int8_t domain;
  /** #GeometryNodeAttributeTransferMode. */
  uint8_t mode;
  char _pad[1];
} NodeGeometryTransferAttribute;

typedef struct NodeGeometrySampleIndex {
  /** #eCustomDataType. */
  int8_t data_type;
  /** #AttrDomain. */
  int8_t domain;
  int8_t clamp;
  char _pad[1];
} NodeGeometrySampleIndex;

typedef struct NodeGeometryRaycast {
  /** #GeometryNodeRaycastMapMode. */
  uint8_t mapping;

  /** #eCustomDataType. */
  int8_t data_type;
} NodeGeometryRaycast;

typedef struct NodeGeometryCurveFill {
  uint8_t mode;
} NodeGeometryCurveFill;

typedef struct NodeGeometryMeshToPoints {
  /** #GeometryNodeMeshToPointsMode */
  uint8_t mode;
} NodeGeometryMeshToPoints;

typedef struct NodeGeometryAttributeCaptureItem {
  /** #eCustomDataType. */
  int8_t data_type;
  char _pad[3];
  /**
   * If the identifier is zero, the item supports forward-compatibility with older versions of
   * Blender when it was only possible to capture a single attribute at a time.
   */
  int identifier;
  char *name;
} NodeGeometryAttributeCaptureItem;

typedef struct NodeGeometryAttributeCapture {
  /** #eCustomDataType. */
  int8_t data_type_legacy;
  /** #AttrDomain. */
  int8_t domain;
  char _pad[2];
  int next_identifier;
  NodeGeometryAttributeCaptureItem *capture_items;
  int capture_items_num;
  int active_index;
} NodeGeometryAttributeCapture;

typedef struct NodeGeometryStoreNamedAttribute {
  /** #eCustomDataType. */
  int8_t data_type;
  /** #AttrDomain. */
  int8_t domain;
} NodeGeometryStoreNamedAttribute;

typedef struct NodeGeometryInputNamedAttribute {
  /** #eCustomDataType. */
  int8_t data_type;
} NodeGeometryInputNamedAttribute;

typedef struct NodeGeometryStringToCurves {
  /** #GeometryNodeStringToCurvesOverflowMode */
  uint8_t overflow;
  /** #GeometryNodeStringToCurvesAlignXMode */
  uint8_t align_x;
  /** #GeometryNodeStringToCurvesAlignYMode */
  uint8_t align_y;
  /** #GeometryNodeStringToCurvesPivotMode */
  uint8_t pivot_mode;
} NodeGeometryStringToCurves;

typedef struct NodeGeometryDeleteGeometry {
  /** #AttrDomain. */
  int8_t domain;
  /** #GeometryNodeDeleteGeometryMode. */
  int8_t mode;
} NodeGeometryDeleteGeometry;

typedef struct NodeGeometryDuplicateElements {
  /** #AttrDomain. */
  int8_t domain;
} NodeGeometryDuplicateElements;

typedef struct NodeGeometryMergeLayers {
  /** #MergeLayerMode. */
  int8_t mode;
} NodeGeometryMergeLayers;

typedef struct NodeGeometrySeparateGeometry {
  /** #AttrDomain. */
  int8_t domain;
} NodeGeometrySeparateGeometry;

typedef struct NodeGeometryImageTexture {
  int8_t interpolation;
  int8_t extension;
} NodeGeometryImageTexture;

typedef enum NodeGeometryViewerItemFlag {
  /**
   * Automatically remove the viewer item when there is no link connected to it. This simplifies
   * working with viewers when one adds and removes values to view all the time.
   *
   * This is a flag instead of always being used, because sometimes the user or some script sets up
   * multiple inputs which shouldn't be deleted immediately. This flag is automatically set when
   * viewer items are added interactively in the node editor.
   */
  NODE_GEO_VIEWER_ITEM_FLAG_AUTO_REMOVE = (1 << 0),
} NodeGeometryViewerItemFlag;

typedef struct NodeGeometryViewerItem {
  char *name;
  /** #eNodeSocketDatatype. */
  short socket_type;
  uint8_t flag;
  char _pad[1];
  /**
   * Generated unique identifier for sockets which stays the same even when the item order or
   * names change.
   */
  int identifier;
} NodeGeometryViewerItem;

typedef struct NodeGeometryViewer {
  NodeGeometryViewerItem *items;
  int items_num;
  int active_index;
  int next_identifier;

  /** #eCustomDataType. */
  int8_t data_type_legacy;
  /** #AttrDomain. */
  int8_t domain;

  char _pad[2];
} NodeGeometryViewer;

typedef struct NodeGeometryUVUnwrap {
  /** #GeometryNodeUVUnwrapMethod. */
  uint8_t method;
} NodeGeometryUVUnwrap;

typedef struct NodeSimulationItem {
  char *name;
  /** #eNodeSocketDatatype. */
  short socket_type;
  /** #AttrDomain. */
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
  blender::MutableSpan<NodeSimulationItem> items_span();
#endif
} NodeGeometrySimulationOutput;

typedef struct NodeRepeatItem {
  char *name;
  /** #eNodeSocketDatatype. */
  short socket_type;
  char _pad[2];
  /**
   * Generated unique identifier for sockets which stays the same even when the item order or
   * names change.
   */
  int identifier;
} NodeRepeatItem;

typedef struct NodeGeometryRepeatInput {
  /** bNode.identifier of the corresponding output node. */
  int32_t output_node_id;
} NodeGeometryRepeatInput;

typedef struct NodeGeometryRepeatOutput {
  NodeRepeatItem *items;
  int items_num;
  int active_index;
  /** Identifier to give to the next repeat item. */
  int next_identifier;
  int inspection_index;

#ifdef __cplusplus
  blender::Span<NodeRepeatItem> items_span() const;
  blender::MutableSpan<NodeRepeatItem> items_span();
#endif
} NodeGeometryRepeatOutput;

typedef struct NodeGeometryForeachGeometryElementInput {
  /** bNode.identifier of the corresponding output node. */
  int32_t output_node_id;
} NodeGeometryForeachGeometryElementInput;

typedef struct NodeForeachGeometryElementInputItem {
  char *name;
  /** #eNodeSocketDatatype. */
  short socket_type;
  char _pad[2];
  /** Generated identifier that stays the same even when the name or order changes. */
  int identifier;
} NodeForeachGeometryElementInputItem;

typedef struct NodeForeachGeometryElementMainItem {
  char *name;
  /** #eNodeSocketDatatype. */
  short socket_type;
  char _pad[2];
  /** Generated identifier that stays the same even when the name or order changes. */
  int identifier;
} NodeForeachGeometryElementMainItem;

typedef struct NodeForeachGeometryElementGenerationItem {
  char *name;
  /** #eNodeSocketDatatype. */
  short socket_type;
  /** #AttrDomain. */
  uint8_t domain;
  char _pad[1];
  /** Generated identifier that stays the same even when the name or order changes. */
  int identifier;
} NodeForeachGeometryElementGenerationItem;

typedef struct NodeForeachGeometryElementInputItems {
  NodeForeachGeometryElementInputItem *items;
  int items_num;
  int active_index;
  int next_identifier;
  char _pad[4];
} NodeForeachGeometryElementInputItems;

typedef struct NodeForeachGeometryElementMainItems {
  NodeForeachGeometryElementMainItem *items;
  int items_num;
  int active_index;
  int next_identifier;
  char _pad[4];
} NodeForeachGeometryElementMainItems;

typedef struct NodeForeachGeometryElementGenerationItems {
  NodeForeachGeometryElementGenerationItem *items;
  int items_num;
  int active_index;
  int next_identifier;
  char _pad[4];
} NodeForeachGeometryElementGenerationItems;

typedef struct NodeGeometryForeachGeometryElementOutput {
  /**
   * The `foreach` zone has three sets of dynamic sockets.
   * One on the input node and two on the output node.
   * All settings are stored centrally in the output node storage though.
   */
  NodeForeachGeometryElementInputItems input_items;
  NodeForeachGeometryElementMainItems main_items;
  NodeForeachGeometryElementGenerationItems generation_items;
  /** This index is used when displaying socket values or using the viewer node. */
  int inspection_index;
  /** #AttrDomain. This is the domain that is iterated over. */
  uint8_t domain;
  char _pad[3];
} NodeGeometryForeachGeometryElementOutput;

typedef struct NodeClosureInput {
  /** bNode.identifier of the corresponding output node. */
  int32_t output_node_id;
} NodeClosureInput;

typedef struct NodeClosureInputItem {
  char *name;
  /** #eNodeSocketDatatype. */
  short socket_type;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type;
  char _pad[1];
  int identifier;
} NodeClosureInputItem;

typedef struct NodeClosureOutputItem {
  char *name;
  /** #eNodeSocketDatatype. */
  short socket_type;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type;
  char _pad[1];
  int identifier;
} NodeClosureOutputItem;

typedef struct NodeClosureInputItems {
  NodeClosureInputItem *items;
  int items_num;
  int active_index;
  int next_identifier;
  char _pad[4];
} NodeClosureInputItems;

typedef struct NodeClosureOutputItems {
  NodeClosureOutputItem *items;
  int items_num;
  int active_index;
  int next_identifier;
  char _pad[4];
} NodeClosureOutputItems;

typedef enum NodeClosureFlag {
  NODE_CLOSURE_FLAG_DEFINE_SIGNATURE = (1 << 0),
} NodeClosureFlag;

typedef struct NodeClosureOutput {
  NodeClosureInputItems input_items;
  NodeClosureOutputItems output_items;
  /** #NodeClosureFlag. */
  uint8_t flag;
  char _pad[7];
} NodeClosureOutput;

typedef struct NodeEvaluateClosureInputItem {
  char *name;
  /** #eNodeSocketDatatype */
  short socket_type;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type;
  char _pad[1];
  int identifier;
} NodeEvaluateClosureInputItem;

typedef struct NodeEvaluateClosureOutputItem {
  char *name;
  /** #eNodeSocketDatatype */
  short socket_type;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type;
  char _pad[1];
  int identifier;
} NodeEvaluateClosureOutputItem;

typedef enum NodeEvaluateClosureFlag {
  NODE_EVALUATE_CLOSURE_FLAG_DEFINE_SIGNATURE = (1 << 0),
} NodeEvaluateClosureFlag;

typedef struct NodeEvaluateClosureInputItems {
  NodeEvaluateClosureInputItem *items;
  int items_num;
  int active_index;
  int next_identifier;
  char _pad[4];
} NodeEvaluateClosureInputItems;

typedef struct NodeEvaluateClosureOutputItems {
  NodeEvaluateClosureOutputItem *items;
  int items_num;
  int active_index;
  int next_identifier;
  char _pad[4];
} NodeEvaluateClosureOutputItems;

typedef struct NodeEvaluateClosure {
  NodeEvaluateClosureInputItems input_items;
  NodeEvaluateClosureOutputItems output_items;
  /** #NodeEvaluateClosureFlag. */
  uint8_t flag;
  char _pad[7];
} NodeEvaluateClosure;

typedef struct IndexSwitchItem {
  /** Generated unique identifier which stays the same even when the item order or names change. */
  int identifier;
} IndexSwitchItem;

typedef struct NodeIndexSwitch {
  IndexSwitchItem *items;
  int items_num;

  /* #eNodeSocketDataType. */
  int data_type;
  /** Identifier to give to the next item. */
  int next_identifier;

  char _pad[4];
#ifdef __cplusplus
  blender::Span<IndexSwitchItem> items_span() const;
  blender::MutableSpan<IndexSwitchItem> items_span();
#endif
} NodeIndexSwitch;

typedef struct GeometryNodeFieldToGridItem {
  /** #eNodeSocketDatatype. */
  int8_t data_type;
  char _pad[3];
  int identifier;
  char *name;
} GeometryNodeFieldToGridItem;

typedef struct GeometryNodeFieldToGrid {
  /** #eNodeSocketDatatype. */
  int8_t data_type;
  char _pad[3];
  int next_identifier;
  GeometryNodeFieldToGridItem *items;
  int items_num;
  int active_index;
} GeometryNodeFieldToGrid;

typedef struct NodeGeometryDistributePointsInVolume {
  /** #GeometryNodePointDistributeVolumeMode. */
  uint8_t mode;
} NodeGeometryDistributePointsInVolume;

typedef struct NodeFunctionCompare {
  /** #NodeCompareOperation */
  int8_t operation;
  /** #eNodeSocketDatatype */
  int8_t data_type;
  /** #NodeCompareMode */
  int8_t mode;
  char _pad[1];
} NodeFunctionCompare;

typedef struct NodeCombSepColor {
  /** #NodeCombSepColorMode */
  int8_t mode;
} NodeCombSepColor;

typedef struct NodeShaderMix {
  /** #eNodeSocketDatatype */
  int8_t data_type;
  /** #NodeShaderMixMode */
  int8_t factor_mode;
  int8_t clamp_factor;
  int8_t clamp_result;
  int8_t blend_type;
  char _pad[3];
} NodeShaderMix;

typedef struct NodeGeometryLinearGizmo {
  /** #GeometryNodeGizmoColor. */
  int color_id;
  /** #GeometryNodeLinearGizmoDrawStyle. */
  int draw_style;
} NodeGeometryLinearGizmo;

typedef struct NodeGeometryDialGizmo {
  /** #GeometryNodeGizmoColor. */
  int color_id;
} NodeGeometryDialGizmo;

typedef struct NodeGeometryTransformGizmo {
  /** #NodeGeometryTransformGizmoFlag. */
  uint32_t flag;
} NodeGeometryTransformGizmo;

typedef enum NodeGeometryTransformGizmoFlag {
  GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_X = 1 << 0,
  GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Y = 1 << 1,
  GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Z = 1 << 2,
  GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_X = 1 << 3,
  GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Y = 1 << 4,
  GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Z = 1 << 5,
  GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_X = 1 << 6,
  GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Y = 1 << 7,
  GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Z = 1 << 8,
} NodeGeometryTransformGizmoFlag;

#define GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_ALL \
  (GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_X | GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Y | \
   GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_Z)

#define GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_ALL \
  (GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_X | GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Y | \
   GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_Z)

#define GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_ALL \
  (GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_X | GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Y | \
   GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_Z)

typedef struct NodeGeometryBakeItem {
  char *name;
  int16_t socket_type;
  int16_t attribute_domain;
  int identifier;
  int32_t flag;
  char _pad[4];
} NodeGeometryBakeItem;

typedef enum NodeGeometryBakeItemFlag {
  GEO_NODE_BAKE_ITEM_IS_ATTRIBUTE = (1 << 0),
} NodeGeometryBakeItemFlag;

typedef struct NodeGeometryBake {
  NodeGeometryBakeItem *items;
  int items_num;
  int next_identifier;
  int active_index;
  char _pad[4];
} NodeGeometryBake;

typedef struct NodeCombineBundleItem {
  char *name;
  int identifier;
  int16_t socket_type;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type;
  char _pad[1];
} NodeCombineBundleItem;

typedef enum NodeCombineBundleFlag {
  NODE_COMBINE_BUNDLE_FLAG_DEFINE_SIGNATURE = (1 << 0),
} NodeCombineBundleFlag;

typedef struct NodeCombineBundle {
  NodeCombineBundleItem *items;
  int items_num;
  int next_identifier;
  int active_index;
  /** #NodeCombineBundleFlag. */
  uint8_t flag;
  char _pad[3];
} NodeCombineBundle;

typedef struct NodeSeparateBundleItem {
  char *name;
  int identifier;
  int16_t socket_type;
  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type;
  char _pad[1];
} NodeSeparateBundleItem;

typedef enum NodeSeparateBundleFlag {
  NODE_SEPARATE_BUNDLE_FLAG_DEFINE_SIGNATURE = (1 << 0),
} NodeSeparateBundleFlag;

typedef struct NodeSeparateBundle {
  NodeSeparateBundleItem *items;
  int items_num;
  int next_identifier;
  int active_index;
  /** #NodeSeparateBundleFlag. */
  uint8_t flag;
  char _pad[3];
} NodeSeparateBundle;

typedef struct NodeFunctionFormatStringItem {
  char *name;
  int identifier;
  int16_t socket_type;
  char _pad[2];
} NodeFunctionFormatStringItem;

typedef struct NodeFunctionFormatString {
  NodeFunctionFormatStringItem *items;
  int items_num;
  int next_identifier;
  int active_index;
  char _pad[4];
} NodeFunctionFormatString;

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

typedef enum NodeGaborType {
  SHD_GABOR_TYPE_2D = 0,
  SHD_GABOR_TYPE_3D = 1,
} NodeGaborType;

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
  NODE_MATH_FLOORED_MODULO = 40,
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
  NODE_VECTOR_MATH_POWER = 27,
  NODE_VECTOR_MATH_SIGN = 28,
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

typedef enum NodeIntegerMathOperation {
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
} NodeIntegerMathOperation;

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

typedef enum CMPNodeTranslateRepeatAxis {
  CMP_NODE_TRANSLATE_REPEAT_AXIS_NONE = 0,
  CMP_NODE_TRANSLATE_REPEAT_AXIS_X = 1,
  CMP_NODE_TRANSLATE_REPEAT_AXIS_Y = 2,
  CMP_NODE_TRANSLATE_REPEAT_AXIS_XY = 3,
} CMPNodeTranslateRepeatAxis;

typedef enum CMPExtensionMode {
  CMP_NODE_EXTENSION_MODE_CLIP = 0,
  CMP_NODE_EXTENSION_MODE_EXTEND = 1,
  CMP_NODE_EXTENSION_MODE_REPEAT = 2,
} CMPNodeBorderCondition;

#define CMP_NODE_MASK_MBLUR_SAMPLES_MAX 64

/* viewer and composite output. */
enum {
  CMP_NODE_OUTPUT_IGNORE_ALPHA = 1,
};

/** Color Balance Node. Stored in `custom1`. */
typedef enum CMPNodeColorBalanceMethod {
  CMP_NODE_COLOR_BALANCE_LGG = 0,
  CMP_NODE_COLOR_BALANCE_ASC_CDL = 1,
  CMP_NODE_COLOR_BALANCE_WHITEPOINT = 2,
} CMPNodeColorBalanceMethod;

/** Alpha Convert Node. Stored in `custom1`. */
typedef enum CMPNodeAlphaConvertMode {
  CMP_NODE_ALPHA_CONVERT_PREMULTIPLY = 0,
  CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY = 1,
} CMPNodeAlphaConvertMode;

/** Distance Matte Node. Stored in #NodeChroma.channel. */
typedef enum CMPNodeDistanceMatteColorSpace {
  CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA = 0,
  CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_YCCA = 1,
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
  CMP_NODE_GLARE_BLOOM = 4,
  CMP_NODE_GLARE_SUN_BEAMS = 5,
  CMP_NODE_GLARE_KERNEL = 6,
} CMPNodeGlareType;

/* Kuwahara Node. Stored in variation */
typedef enum CMPNodeKuwahara {
  CMP_NODE_KUWAHARA_CLASSIC = 0,
  CMP_NODE_KUWAHARA_ANISOTROPIC = 1,
} CMPNodeKuwahara;

/* Shared between nodes with interpolation option. */
typedef enum CMPNodeInterpolation {
  CMP_NODE_INTERPOLATION_NEAREST = 0,
  CMP_NODE_INTERPOLATION_BILINEAR = 1,
  CMP_NODE_INTERPOLATION_BICUBIC = 2,
  CMP_NODE_INTERPOLATION_ANISOTROPIC = 3,
} CMPNodeInterpolation;

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

/** #NodeDenoise.quality */
typedef enum CMPNodeDenoiseQuality {
  CMP_NODE_DENOISE_QUALITY_SCENE = 0,
  CMP_NODE_DENOISE_QUALITY_HIGH = 1,
  CMP_NODE_DENOISE_QUALITY_BALANCED = 2,
  CMP_NODE_DENOISE_QUALITY_FAST = 3,
} CMPNodeDenoiseQuality;

/* Color combine/separate modes */

typedef enum CMPNodeCombSepColorMode {
  CMP_NODE_COMBSEP_COLOR_RGB = 0,
  CMP_NODE_COMBSEP_COLOR_HSV = 1,
  CMP_NODE_COMBSEP_COLOR_HSL = 2,
  CMP_NODE_COMBSEP_COLOR_YCC = 3,
  CMP_NODE_COMBSEP_COLOR_YUV = 4,
} CMPNodeCombSepColorMode;

/* Cryptomatte node source. */
typedef enum CMPNodeCryptomatteSource {
  CMP_NODE_CRYPTOMATTE_SOURCE_RENDER = 0,
  CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE = 1,
} CMPNodeCryptomatteSource;

/* Channel Matte node, stored in custom1. */
typedef enum CMPNodeChannelMatteColorSpace {
  CMP_NODE_CHANNEL_MATTE_CS_RGB = 0,
  CMP_NODE_CHANNEL_MATTE_CS_HSV = 1,
  CMP_NODE_CHANNEL_MATTE_CS_YUV = 2,
  CMP_NODE_CHANNEL_MATTE_CS_YCC = 3,
} CMPNodeChannelMatteColorSpace;

/* NodeLensDist.distortion_type. */
typedef enum CMPNodeLensDistortionType {
  CMP_NODE_LENS_DISTORTION_RADIAL = 0,
  CMP_NODE_LENS_DISTORTION_HORIZONTAL = 1,
} CMPNodeLensDistortionType;

/* Alpha Over node. Stored in custom1. */
typedef enum CMPNodeAlphaOverOperationType {
  CMP_NODE_ALPHA_OVER_OPERATION_TYPE_OVER = 0,
  CMP_NODE_ALPHA_OVER_OPERATION_TYPE_DISJOINT_OVER = 1,
  CMP_NODE_ALPHA_OVER_OPERATION_TYPE_CONJOINT_OVER = 2,
} CMPNodeAlphaOverOperationType;

/* Relative To Pixel node. Stored in custom1. */
typedef enum CMPNodeRelativeToPixelDataType {
  CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT = 0,
  CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_VECTOR = 1,
} CMPNodeRelativeToPixelDataType;

/* Relative To Pixel node. Stored in custom2. */
typedef enum CMPNodeRelativeToPixelReferenceDimension {
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_PER_DIMENSION = 0,
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_X = 1,
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_Y = 2,
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_GREATER = 3,
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_SMALLER = 4,
  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_DIAGONAL = 5,
} CMPNodeRelativeToPixelReferenceDimension;

/* Scattering phase functions */
enum {
  SHD_PHASE_HENYEY_GREENSTEIN = 0,
  SHD_PHASE_FOURNIER_FORAND = 1,
  SHD_PHASE_DRAINE = 2,
  SHD_PHASE_RAYLEIGH = 3,
  SHD_PHASE_MIE = 4,
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

typedef enum NodeCombSepColorMode {
  NODE_COMBSEP_COLOR_RGB = 0,
  NODE_COMBSEP_COLOR_HSV = 1,
  NODE_COMBSEP_COLOR_HSL = 2,
} NodeCombSepColorMode;

typedef enum GeometryNodeGizmoColor {
  GEO_NODE_GIZMO_COLOR_PRIMARY = 0,
  GEO_NODE_GIZMO_COLOR_SECONDARY = 1,
  GEO_NODE_GIZMO_COLOR_X = 2,
  GEO_NODE_GIZMO_COLOR_Y = 3,
  GEO_NODE_GIZMO_COLOR_Z = 4,
} GeometryNodeGizmoColor;

typedef enum GeometryNodeLinearGizmoDrawStyle {
  GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_ARROW = 0,
  GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_CROSS = 1,
  GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_BOX = 2,
} GeometryNodeLinearGizmoDrawStyle;

typedef enum NodeGeometryTransformMode {
  GEO_NODE_TRANSFORM_MODE_COMPONENTS = 0,
  GEO_NODE_TRANSFORM_MODE_MATRIX = 1,
} NodeGeometryTransformMode;
