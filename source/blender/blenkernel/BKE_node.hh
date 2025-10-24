/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <optional>

#include "BLI_compiler_compat.h"
#include "BLI_span.hh"

#include "BKE_node_socket_value_fwd.hh"
#include "BKE_volume_enums.hh"

/* for FOREACH_NODETREE_BEGIN */
#include "DNA_node_types.h"

#include "RNA_types.hh"

#include "BLI_map.hh"
#include "BLI_string_ref.hh"

/* not very important, but the stack solver likes to know a maximum */
#define MAX_SOCKET 512

struct BlendDataReader;
struct BlendWriter;
struct FreestyleLineStyle;
struct GPUMaterial;
struct GPUNodeStack;
struct ID;
struct ImBuf;
struct LibraryForeachIDData;
struct Light;
struct Main;
struct Material;
struct PointerRNA;
struct Scene;
struct SpaceNode;
struct Tex;
struct World;
struct bContext;
struct bNode;
struct bNodeExecContext;
struct bNodeTreeExec;
struct bNodeExecData;
struct bNodeLink;
struct bNodeSocket;
struct bNodeStack;
struct bNodeTree;
struct bNodeTreeExec;
struct uiLayout;

namespace blender {
class CPPType;
namespace nodes {
class DNode;
class NodeMultiFunctionBuilder;
class GeoNodeExecParams;
class NodeDeclaration;
class NodeDeclarationBuilder;
class GatherAddNodeSearchParams;
class GatherLinkSearchOpParams;
struct NodeExtraInfoParams;
namespace value_elem {
class InverseElemEvalParams;
class ElemEvalParams;
}  // namespace value_elem
namespace inverse_eval {
class InverseEvalParams;
}  // namespace inverse_eval
}  // namespace nodes
namespace compositor {
class Context;
class NodeOperation;
}  // namespace compositor
}  // namespace blender

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Node Type Definitions
 * \{ */

/**
 * \brief Compact definition of a node socket.
 *
 * Can be used to quickly define a list of static sockets for a node,
 * which are added to each new node of that type.
 *
 * \deprecated This struct is used by C nodes to define templates as simple
 * static struct lists. These are converted to the new template collections
 * in RNA types automatically.
 */
struct bNodeSocketTemplate {
  int type;
  char name[/*MAX_NAME*/ 64];
  /** Default alloc value for inputs. */
  float val1, val2, val3, val4;
  float min, max;
  /** Would use PropertySubType but this is a bad level include to use RNA. */
  int subtype;
  int flag;

  /* After this line is used internal only. */

  /** Used to hold verified socket. */
  bNodeSocket *sock;
  /** Generated from name. */
  char identifier[/*MAX_NAME*/ 64];
};

/* Use `void *` for callbacks that require C++. This is rather ugly, but works well for now. This
 * would not be necessary if we would use bNodeSocketType and bNodeType only in C++ code.
 * However, achieving this requires quite a few changes currently. */
using NodeMultiFunctionBuildFunction = void (*)(blender::nodes::NodeMultiFunctionBuilder &builder);
using NodeGeometryExecFunction = void (*)(blender::nodes::GeoNodeExecParams params);
using NodeDeclareFunction = void (*)(blender::nodes::NodeDeclarationBuilder &builder);
using NodeDeclareDynamicFunction = void (*)(const bNodeTree &tree,
                                            const bNode &node,
                                            blender::nodes::NodeDeclarationBuilder &builder);
using SocketGetCPPValueFunction = void (*)(const void *socket_value, void *r_value);
using SocketGetGeometryNodesCPPValueFunction = SocketValueVariant (*)(const void *socket_value);

/* Adds socket link operations that are specific to this node type. */
using NodeGatherSocketLinkOperationsFunction =
    void (*)(blender::nodes::GatherLinkSearchOpParams &params);

/* Adds node add menu operations that are specific to this node type. */
using NodeGatherAddOperationsFunction =
    void (*)(blender::nodes::GatherAddNodeSearchParams &params);

using NodeGetCompositorOperationFunction =
    blender::compositor::NodeOperation *(*)(blender::compositor::Context & context,
                                            blender::nodes::DNode node);
using NodeExtraInfoFunction = void (*)(blender::nodes::NodeExtraInfoParams &params);
using NodeInverseElemEvalFunction =
    void (*)(blender::nodes::value_elem::InverseElemEvalParams &params);
using NodeElemEvalFunction = void (*)(blender::nodes::value_elem::ElemEvalParams &params);
using NodeInverseEvalFunction = void (*)(blender::nodes::inverse_eval::InverseEvalParams &params);
using NodeInternallyLinkedInputFunction = const bNodeSocket *(*)(const bNodeTree &tree,
                                                                 const bNode &node,
                                                                 const bNodeSocket &output_socket);
using NodeBlendWriteFunction = void (*)(const bNodeTree &tree,
                                        const bNode &node,
                                        BlendWriter &writer);
using NodeBlendDataReadFunction = void (*)(bNodeTree &tree, bNode &node, BlendDataReader &reader);

/**
 * \brief Defines a socket type.
 *
 * Defines the appearance and behavior of a socket in the UI.
 */
struct bNodeSocketType {
  /** Identifier name. */
  std::string idname;
  /** Type label. */
  std::string label;
  /** Sub-type label. */
  std::string subtype_label;

  void (*draw)(bContext *C,
               uiLayout *layout,
               PointerRNA *ptr,
               PointerRNA *node_ptr,
               StringRef text) = nullptr;
  void (*draw_color)(bContext *C, PointerRNA *ptr, PointerRNA *node_ptr, float *r_color) = nullptr;
  void (*draw_color_simple)(const bNodeSocketType *socket_type, float *r_color) = nullptr;

  void (*interface_draw)(ID *id,
                         bNodeTreeInterfaceSocket *socket,
                         bContext *C,
                         uiLayout *layout) = nullptr;
  void (*interface_init_socket)(ID *id,
                                const bNodeTreeInterfaceSocket *interface_socket,
                                bNode *node,
                                bNodeSocket *socket,
                                StringRefNull data_path) = nullptr;
  void (*interface_from_socket)(ID *id,
                                bNodeTreeInterfaceSocket *interface_socket,
                                const bNode *node,
                                const bNodeSocket *socket) = nullptr;

  /* RNA integration */
  ExtensionRNA ext_socket = {};
  ExtensionRNA ext_interface = {};

  /* for standard socket types in C */
  eNodeSocketDatatype type = eNodeSocketDatatype(0);
  int subtype = 0;

  /* When set, bNodeSocket->limit does not have any effect anymore. */
  bool use_link_limits_of_type = false;
  int input_link_limit = 0;
  int output_link_limit = 0;

  /* Callback to free the socket type. */
  void (*free_self)(bNodeSocketType *stype) = nullptr;

  /* Return the CPPType of this socket. */
  const blender::CPPType *base_cpp_type = nullptr;
  /* Get the value of this socket in a generic way. */
  SocketGetCPPValueFunction get_base_cpp_value = nullptr;
  /* Get geometry nodes cpp value. */
  SocketGetGeometryNodesCPPValueFunction get_geometry_nodes_cpp_value = nullptr;
  /* Default value for this socket type. */
  const SocketValueVariant *geometry_nodes_default_value = nullptr;
};

using NodeInitExecFunction = void *(*)(bNodeExecContext * context,
                                       bNode *node,
                                       bNodeInstanceKey key);
using NodeFreeExecFunction = void (*)(void *nodedata);
using NodeExecFunction = void (*)(
    void *data, int thread, bNode *, bNodeExecData *execdata, bNodeStack **in, bNodeStack **out);
using NodeGPUExecFunction = int (*)(
    GPUMaterial *mat, bNode *node, bNodeExecData *execdata, GPUNodeStack *in, GPUNodeStack *out);
using NodeMaterialXFunction = void (*)(void *data, bNode *node, bNodeSocket *out);

struct NodeInsertLinkParams {
  bNodeTree &ntree;
  bNode &node;
  bNodeLink &link;
  /** Optional context to allow for more advanced link insertion functionality. */
  bContext *C = nullptr;
};

/**
 * \brief Defines a node type.
 *
 * Initial attributes and constants for a node as well as callback functions
 * implementing the node behavior.
 */
struct bNodeType {
  std::string idname;
  /** See bNode::type_legacy. */
  int type_legacy;

  std::string ui_name;
  std::string ui_description;
  int ui_icon;
  /** Should usually use the idname instead, but this enum type is still exposed in Python. */
  const char *enum_name_legacy = nullptr;

  float width = 0.0f, minwidth = 0.0f, maxwidth = 0.0f;
  float height = 0.0f, minheight = 0.0f, maxheight = 0.0f;
  short nclass = 0, flag = 0;

  /* templates for static sockets */
  bNodeSocketTemplate *inputs = nullptr, *outputs = nullptr;

  std::string storagename; /* struct name for DNA */

  /* Draw the option buttons on the node */
  void (*draw_buttons)(uiLayout *, bContext *C, PointerRNA *ptr) = nullptr;
  /* Additional parameters in the side panel */
  void (*draw_buttons_ex)(uiLayout *, bContext *C, PointerRNA *ptr) = nullptr;

  /* Additional drawing on backdrop */
  void (*draw_backdrop)(SpaceNode *snode, ImBuf *backdrop, bNode *node, int x, int y) = nullptr;

  /**
   * Optional custom label function for the node header.
   * \note Used as a fallback when #bNode.label isn't set.
   */
  void (*labelfunc)(const bNodeTree *ntree,
                    const bNode *node,
                    char *label,
                    int label_maxncpy) = nullptr;

  /** Optional override for node class, used for drawing node header. */
  int (*ui_class)(const bNode *node) = nullptr;
  /** Optional dynamic description of what the node group does. */
  std::string (*ui_description_fn)(const bNode &node) = nullptr;

  /** Called when the node is updated in the editor. */
  void (*updatefunc)(bNodeTree *ntree, bNode *node) = nullptr;

  /**
   * Initialize a new node instance of this type after creation.
   *
   * \note Assignments to `node->id` must not increment the user of the ID.
   * This is handled by the caller of this callback.
   */
  void (*initfunc)(bNodeTree *ntree, bNode *node) = nullptr;
  /**
   * Free the node instance.
   *
   * \note Access to `node->id` must be avoided in this function as this is called
   * while freeing #Main, the state of this ID is undefined.
   * Higher level logic to remove the node handles the user-count.
   */
  void (*freefunc)(bNode *node) = nullptr;
  /** Make a copy of the node instance. */
  void (*copyfunc)(bNodeTree *dest_ntree, bNode *dest_node, const bNode *src_node) = nullptr;

  /* Registerable API callback versions, called in addition to C callbacks */
  void (*initfunc_api)(const bContext *C, PointerRNA *ptr) = nullptr;
  void (*freefunc_api)(PointerRNA *ptr) = nullptr;
  void (*copyfunc_api)(PointerRNA *ptr, const bNode *src_node) = nullptr;

  /**
   * An additional poll test for deciding whether nodes should be an option in search menus.
   * Potentially more strict poll than #poll(), but doesn't have to check the same things.
   */
  bool (*add_ui_poll)(const bContext *C) = nullptr;

  /**
   * Can this node type be added to a node tree?
   * \param r_disabled_hint: Hint to display in the UI when the poll fails.
   *                         The callback can set this to a static string without having to
   *                         null-check it (or without setting it to null if it's not used).
   *                         The caller must pass a valid `const char **` and null-initialize it
   *                         when it's not just a dummy, that is, if it actually wants to access
   *                         the returned disabled-hint (null-check needed!).
   */
  bool (*poll)(const bNodeType *ntype,
               const bNodeTree *nodetree,
               const char **r_disabled_hint) = nullptr;
  /**
   * Can this node be added to a node tree?
   * \param r_disabled_hint: See `poll()`.
   */
  bool (*poll_instance)(const bNode *node,
                        const bNodeTree *nodetree,
                        const char **r_disabled_hint) = nullptr;

  /* Optional handling of link insertion. Returns false if the link shouldn't be created. */
  bool (*insert_link)(NodeInsertLinkParams &params) = nullptr;

  void (*free_self)(bNodeType *ntype) = nullptr;

  /* **** execution callbacks **** */
  NodeInitExecFunction init_exec_fn = nullptr;
  NodeFreeExecFunction free_exec_fn = nullptr;
  NodeExecFunction exec_fn = nullptr;
  /* gpu */
  NodeGPUExecFunction gpu_fn = nullptr;
  /* MaterialX */
  NodeMaterialXFunction materialx_fn = nullptr;

  /* Get an instance of this node's compositor operation. Freeing the instance is the
   * responsibility of the caller. */
  NodeGetCompositorOperationFunction get_compositor_operation = nullptr;

  /* Build a multi-function for this node. */
  NodeMultiFunctionBuildFunction build_multi_function = nullptr;

  /* Execute a geometry node. */
  NodeGeometryExecFunction geometry_node_execute = nullptr;

  /**
   * Declares which sockets and panels the node has. It has to be able to generate a declaration
   * with and without a specific node context. If the declaration depends on the node, but the node
   * is not provided, then the declaration should be generated as much as possible and everything
   * that depends on the node context should be skipped.
   */
  NodeDeclareFunction declare = nullptr;

  /**
   * Declaration of the node outside of any context. If the node declaration is never dependent on
   * the node context, this declaration is also shared with the corresponding node instances.
   * Otherwise, it mainly allows checking what sockets a node will have, without having to create
   * the node. In this case, the static declaration is mostly just a hint, and does not have to
   * match with the final node.
   */
  blender::nodes::NodeDeclaration *static_declaration = nullptr;

  /**
   * Add to the list of search names and operations gathered by node link drag searching.
   * Usually it isn't necessary to override the default behavior here, but a node type can have
   * custom behavior here like adding custom search items.
   */
  NodeGatherSocketLinkOperationsFunction gather_link_search_ops = nullptr;

  /** Get extra information that is drawn next to the node. */
  NodeExtraInfoFunction get_extra_info = nullptr;

  /** Get the internally linked input socket for the case when the node is muted. */
  NodeInternallyLinkedInputFunction internally_linked_input = nullptr;

  /**
   * Read and write the content of the node storage. Writing the storage struct itself is handled
   * by generic code by reading and writing bNodeType::storagename.
   */
  NodeBlendWriteFunction blend_write_storage_content = nullptr;
  NodeBlendDataReadFunction blend_data_read_storage_content = nullptr;

  /**
   * "Abstract" evaluation of the node. It tells the caller which parts of the inputs affect which
   * parts of the outputs.
   */
  NodeElemEvalFunction eval_elem = nullptr;

  /**
   * Similar to #eval_elem but tells the caller which parts of the inputs have to be modified to
   * modify the outputs.
   */
  NodeInverseElemEvalFunction eval_inverse_elem = nullptr;

  /**
   * Evaluates the inverse of the node if possible. This evaluation has access to logged values of
   * all input sockets as well as new values for output sockets. Based on that, it should determine
   * how one or more of the inputs should change so that the output becomes the given one.
   */
  NodeInverseEvalFunction eval_inverse = nullptr;

  /**
   * Registers operators that are specific to this node. This allows nodes to be more
   * self-contained compared to the alternative to registering all operators in a more central
   * place.
   */
  void (*register_operators)() = nullptr;

  /** True when the node cannot be muted. */
  bool no_muting = false;
  /** Some nodes should ignore the inferred visibility for improved UX. */
  bool ignore_inferred_input_socket_visibility = false;
  /** True when the node still works but it's usage is discouraged. */
  const char *deprecation_notice = nullptr;

  /**
   * In some nodes the set of sockets depends on other data like linked nodes. For example, the
   * Separate Bundle node can adapt based on what the bundle contains that is linked to it. When
   * this function returns true, a sync button should be shown for the node that updates the node.
   */
  bool (*can_sync_sockets)(const bContext &C, const bNodeTree &tree, const bNode &node) = nullptr;

  /* RNA integration */
  ExtensionRNA rna_ext = {};

  /**
   * Check if type has the given idname.
   *
   * Note: This function assumes that the given idname is a valid registered idname. This is done
   * to catch typos earlier. One can compare with `bNodeType::idname` directly if the idname might
   * not be registered.
   */
  bool is_type(StringRef query_idname) const;
};

/** #bNodeType.nclass (for add-menu and themes). */
#define NODE_CLASS_INPUT 0
#define NODE_CLASS_OUTPUT 1
#define NODE_CLASS_OP_COLOR 3
#define NODE_CLASS_OP_VECTOR 4
#define NODE_CLASS_OP_FILTER 5
#define NODE_CLASS_GROUP 6
#define NODE_CLASS_CONVERTER 8
#define NODE_CLASS_MATTE 9
#define NODE_CLASS_DISTORT 10
#define NODE_CLASS_PATTERN 12
#define NODE_CLASS_TEXTURE 13
#define NODE_CLASS_SCRIPT 32
#define NODE_CLASS_INTERFACE 33
#define NODE_CLASS_SHADER 40
#define NODE_CLASS_GEOMETRY 41
#define NODE_CLASS_ATTRIBUTE 42
#define NODE_CLASS_LAYOUT 100

/**
 * Color tag stored per node group. This affects the header color of group nodes.
 * Note that these values are written to DNA.
 *
 * This is separate from the `NODE_CLASS_*` enum, because those have some additional items and are
 * not purely color tags. Some classes also have functional effects (e.g. `NODE_CLASS_INPUT`).
 */
enum class NodeColorTag {
  None = 0,
  Attribute = 1,
  Color = 2,
  Converter = 3,
  Distort = 4,
  Filter = 5,
  Geometry = 6,
  Input = 7,
  Matte = 8,
  Output = 9,
  Script = 10,
  Shader = 11,
  Texture = 12,
  Vector = 13,
  Pattern = 14,
  Interface = 15,
  Group = 16,
};

using bNodeClassCallback = void (*)(void *calldata, int nclass, StringRefNull name);

struct bNodeTreeType {
  int type = 0;       /* type identifier */
  std::string idname; /* identifier name */

  /* The ID name of group nodes for this type. */
  std::string group_idname;

  std::string ui_name;
  std::string ui_description;
  int ui_icon = 0;

  /* callbacks */
  /* Iteration over all node classes. */
  void (*foreach_nodeclass)(void *calldata, bNodeClassCallback func) = nullptr;
  /* Check visibility in the node editor */
  bool (*poll)(const bContext *C, bNodeTreeType *ntreetype) = nullptr;
  /* Select a node tree from the context */
  void (*get_from_context)(const bContext *C,
                           bNodeTreeType *ntreetype,
                           bNodeTree **r_ntree,
                           ID **r_id,
                           ID **r_from) = nullptr;

  /* calls allowing threaded composite */
  void (*localize)(bNodeTree *localtree, bNodeTree *ntree) = nullptr;
  void (*local_merge)(Main *bmain, bNodeTree *localtree, bNodeTree *ntree) = nullptr;

  /* Tree update. Overrides `nodetype->updatetreefunc`. */
  void (*update)(bNodeTree *ntree) = nullptr;

  bool (*validate_link)(eNodeSocketDatatype from, eNodeSocketDatatype to) = nullptr;

  void (*node_add_init)(bNodeTree *ntree, bNode *bnode) = nullptr;

  /* Check if the socket type is valid for this tree type. */
  bool (*valid_socket_type)(bNodeTreeType *ntreetype, bNodeSocketType *socket_type) = nullptr;

  /**
   * If true, then some UI elements related to building node groups will be hidden.
   * This can be used by Python-defined custom node tree types.
   *
   * This is a uint8_t instead of bool to avoid compiler warnings in generated RNA code.
   */
  uint8_t no_group_interface = 0;

  /* RNA integration */
  ExtensionRNA rna_ext = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic API, Trees
 * \{ */

bNodeTreeType *node_tree_type_find(StringRef idname);
void node_tree_type_add(bNodeTreeType &nt);
void node_tree_type_free_link(const bNodeTreeType &nt);
bool node_tree_is_registered(const bNodeTree &ntree);

Span<bNodeTreeType *> node_tree_types_get();

/**
 * Try to initialize all type-info in a node tree.
 *
 * \note In general undefined type-info is a perfectly valid case,
 * the type may just be registered later.
 * In that case the update_typeinfo function will set type-info on registration
 * and do necessary updates.
 */
void node_tree_set_type(bNodeTree &ntree);

bNodeTree *node_tree_add_tree(Main *bmain, StringRef name, StringRef idname);

/**
 * Add a new (non-embedded) node tree, like #node_tree_add_tree, but allows to create it inside a
 * given library. Used mainly by readfile code when versioning linked data.
 */
bNodeTree *node_tree_add_in_lib(Main *bmain,
                                Library *owner_library,
                                StringRefNull name,
                                StringRefNull idname);

/**
 * Free tree which is embedded into another data-block.
 */
void node_tree_free_embedded_tree(bNodeTree *ntree);

/**
 * Get address of potential node-tree pointer of given ID.
 *
 * \warning Using this function directly is potentially dangerous, if you don't know or are not
 * sure, please use `node_tree_from_id()` instead.
 */
bNodeTree **node_tree_ptr_from_id(ID *id);

/**
 * Returns the private NodeTree object of the data-block, if it has one.
 */
bNodeTree *node_tree_from_id(ID *id);

/**
 * Check recursively if a node tree contains another.
 */
bool node_tree_contains_tree(const bNodeTree &tree_to_search_in,
                             const bNodeTree &tree_to_search_for);

void node_tree_update_all_users(Main *main, ID *id);

/**
 * XXX: old trees handle output flags automatically based on special output
 * node types and last active selection.
 * New tree types have a per-output socket flag to indicate the final output to use explicitly.
 */
void node_tree_set_output(bNodeTree &ntree);

/**
 * Returns localized tree for execution in threads.
 *
 * \param new_owner_id: the owner ID of the localized nodetree, may be nullopt if unknown or
 * irrelevant.
 */
bNodeTree *node_tree_localize(bNodeTree *ntree, std::optional<ID *> new_owner_id);

/**
 * This is only direct data, tree itself should have been written.
 */
void node_tree_blend_write(BlendWriter *writer, bNodeTree *ntree);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic API, Nodes
 * \{ */

bNodeType *node_type_find(StringRef idname);
StringRefNull node_type_find_alias(StringRefNull alias);
void node_register_type(bNodeType &ntype);
void node_unregister_type(bNodeType &ntype);
void node_register_alias(bNodeType &nt, StringRef alias);

Span<bNodeType *> node_types_get();

bNodeSocketType *node_socket_type_find(StringRef idname);
bNodeSocketType *node_socket_type_find_static(int type, int subtype = 0);
void node_register_socket_type(bNodeSocketType &stype);
void node_unregister_socket_type(bNodeSocketType &stype);
bool node_socket_is_registered(const bNodeSocket &sock);
StringRefNull node_socket_type_label(const bNodeSocketType &stype);

/* The optional dimensions argument can be provided for types that support multiple possible
 * dimensions like Vector. It is expected to be in the range [2, 4] and if not provided, 3 will be
 * assumed. */
std::optional<StringRefNull> node_static_socket_type(int type,
                                                     int subtype,
                                                     std::optional<int> dimensions = std::nullopt);
std::optional<StringRefNull> node_static_socket_interface_type_new(
    int type, int subtype, std::optional<int> dimensions = std::nullopt);

std::optional<StringRefNull> node_static_socket_label(int type, int subtype);

Span<bNodeSocketType *> node_socket_types_get();

bNodeSocket *node_find_socket(bNode &node, eNodeSocketInOut in_out, StringRef identifier);
const bNodeSocket *node_find_socket(const bNode &node,
                                    eNodeSocketInOut in_out,
                                    StringRef identifier);
bNodeSocket *node_add_socket(bNodeTree &ntree,
                             bNode &node,
                             eNodeSocketInOut in_out,
                             StringRefNull idname,
                             StringRefNull identifier,
                             StringRefNull name);
bNodeSocket *node_add_static_socket(bNodeTree &ntree,
                                    bNode &node,
                                    eNodeSocketInOut in_out,
                                    int type,
                                    int subtype,
                                    StringRefNull identifier,
                                    StringRefNull name);
void node_remove_socket(bNodeTree &ntree, bNode &node, bNodeSocket &sock);

void node_modify_socket_type_static(
    bNodeTree *ntree, bNode *node, bNodeSocket *sock, int type, int subtype);

bNode *node_add_node(const bContext *C,
                     bNodeTree &ntree,
                     StringRef idname,
                     std::optional<int> unique_identifier = std::nullopt);
bNode *node_add_static_node(const bContext *C, bNodeTree &ntree, int type);

/**
 * Find the first available, non-duplicate name for a given node.
 */
void node_unique_name(bNodeTree &ntree, bNode &node);
/**
 * Create a new unique integer identifier for the node. Also set the node's
 * index in the tree, which is an eagerly maintained cache.
 */
void node_unique_id(bNodeTree &ntree, bNode &node);

/**
 * Delete node, associated animation data and ID user count.
 */
void node_remove_node(
    Main *bmain, bNodeTree &ntree, bNode &node, bool do_id_user, bool remove_animation = true);

float2 node_dimensions_get(const bNode &node);
void node_tag_update_id(bNode &node);
void node_internal_links(bNode &node, bNodeLink **r_links, int *r_len);

/**
 * Also used via RNA API, so we check for proper input output direction.
 */
bNodeLink &node_add_link(
    bNodeTree &ntree, bNode &fromnode, bNodeSocket &fromsock, bNode &tonode, bNodeSocket &tosock);
void node_remove_link(bNodeTree *ntree, bNodeLink &link);
void node_remove_socket_links(bNodeTree &ntree, bNodeSocket &sock);

bool node_link_is_hidden(const bNodeLink &link);

void node_attach_node(bNodeTree &ntree, bNode &node, bNode &parent);
void node_detach_node(bNodeTree &ntree, bNode &node);

/**
 * Finds a node based on given socket, returning null in the case where the socket is not part of
 * the node tree.
 */
bNode *node_find_node_try(bNodeTree &ntree, bNodeSocket &socket);

/**
 * Find the node that contains the given socket. This uses the node topology cache, meaning
 * subsequent access after changing the node tree will be more expensive, but amortized over time,
 * the cost is constant.
 */
bNode &node_find_node(bNodeTree &ntree, bNodeSocket &socket);
const bNode &node_find_node(const bNodeTree &ntree, const bNodeSocket &socket);

/**
 * Finds a node based on its name.
 */
bNode *node_find_node_by_name(bNodeTree &ntree, StringRefNull name);

/** Try to find an input item with the given identifier in the entire node interface tree. */
const bNodeTreeInterfaceSocket *node_find_interface_input_by_identifier(const bNodeTree &ntree,
                                                                        StringRef identifier);

bool node_is_parent_and_child(const bNode &parent, const bNode &child);

int node_count_socket_links(const bNodeTree &ntree, const bNodeSocket &sock);

/**
 * Selects or deselects the node. If the node is deselected, all its sockets are deselected too.
 * \return True if any selection was changed.
 */
bool node_set_selected(bNode &node, bool select);
/**
 * Two active flags, ID nodes have special flag for buttons display.
 */
void node_set_active(bNodeTree &ntree, bNode &node);
bNode *node_get_active(bNodeTree &ntree);
void node_clear_active(bNodeTree &ntree);
/**
 * Two active flags, ID nodes have special flag for buttons display.
 */
bNode *node_get_active_texture(bNodeTree &ntree);

int node_socket_link_limit(const bNodeSocket &sock);

/**
 * Magic number for initial hash key.
 */
extern const bNodeInstanceKey NODE_INSTANCE_KEY_BASE;
extern const bNodeInstanceKey NODE_INSTANCE_KEY_NONE;

bNodeInstanceKey node_instance_key(bNodeInstanceKey parent_key,
                                   const bNodeTree *ntree,
                                   const bNode *node);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Type Access
 * \{ */

bool node_group_poll(const bNodeTree *nodetree,
                     const bNodeTree *grouptree,
                     const char **r_disabled_hint);

void node_type_base_custom(bNodeType &ntype,
                           StringRefNull idname,
                           StringRefNull name,
                           StringRefNull enum_name,
                           short nclass);

/**
 * \warning Nodes defining a storage type _must_ allocate this for new nodes.
 * Otherwise nodes will reload as undefined (#46619).
 * #storagename is optional due to some compositor nodes use non-DNA storage type.
 */
void node_type_storage(bNodeType &ntype,
                       std::optional<StringRefNull> storagename,
                       void (*freefunc)(bNode *node),
                       void (*copyfunc)(bNodeTree *dest_ntree,
                                        bNode *dest_node,
                                        const bNode *src_node));

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common Node Types
 *
 * Defined here rather than #BKE_node_legacy_types.hh for inline usage.
 * \{ */

#define NODE_UNDEFINED -2 /* node type is not registered */
#define NODE_CUSTOM -1    /* for dynamically registered custom types */
#define NODE_GROUP 2
#define NODE_FRAME 5
#define NODE_REROUTE 6
#define NODE_GROUP_INPUT 7
#define NODE_GROUP_OUTPUT 8
#define NODE_CUSTOM_GROUP 9

#define NODE_LEGACY_TYPE_GENERATION_START 5000

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Tree Iterator
 *
 * Utility macro for visiting every node tree in the library data,
 * including local bNodeTree blocks in other IDs.
 * This avoids the need for callback functions and allows executing code
 * in a single inner code block.
 *
 * Variables:
 *
 * - nodetree:
 *   The actual bNodeTree data block.
 *   Check `nodetree->idname` or `nodetree->typeinfo` to use only specific types.
 *
 * - id:
 *   The owner of the bNodeTree data block.
 *   Same as nodetree if it's a linkable node tree from the library.
 *
 * Examples:
 *
 * \code{.c}
 * FOREACH_NODETREE_BEGIN(bmain, nodetree, id) {
 *     if (id == nodetree)
 *         printf("This is a linkable node tree");
 * } FOREACH_NODETREE_END;
 *
 * FOREACH_NODETREE_BEGIN(bmain, nodetree, id) {
 *     if (nodetree->idname == "ShaderNodeTree")
 *         printf("This is a shader node tree);
 *     if (GS(id) == ID_MA)
 *         printf(" and it's owned by a material");
 * } FOREACH_NODETREE_END;
 * \endcode
 *
 * \{ */

/* should be an opaque type, only for internal use by BKE_node_tree_iter_*** */
struct NodeTreeIterStore {
  bNodeTree *ngroup;
  Scene *scene;
  Material *mat;
  Tex *tex;
  Light *light;
  World *world;
  FreestyleLineStyle *linestyle;
};

void node_tree_iterator_init(NodeTreeIterStore *ntreeiter, Main *bmain);
bool node_tree_iterator_step(NodeTreeIterStore *ntreeiter, bNodeTree **r_nodetree, ID **r_id);

#define FOREACH_NODETREE_BEGIN(bmain, _nodetree, _id) \
  { \
    blender::bke::NodeTreeIterStore _nstore; \
    bNodeTree *_nodetree; \
    ID *_id; \
    /* avoid compiler warning about unused variables */ \
    blender::bke::node_tree_iterator_init(&_nstore, bmain); \
    while (blender::bke::node_tree_iterator_step(&_nstore, &_nodetree, &_id) == true) { \
      if (_nodetree) {

#define FOREACH_NODETREE_END \
  } \
  } \
  } \
  ((void)0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Tree
 */

void node_tree_remove_layer_n(bNodeTree *ntree, Scene *scene, int layer_index);

void node_system_init();
void node_system_exit();

bNodeTree *node_tree_add_tree_embedded(Main *bmain,
                                       ID *owner_id,
                                       StringRefNull name,
                                       StringRefNull idname);

/* Copy/free functions, need to manage ID users. */

/**
 * Free (or release) any data used by this node-tree.
 * Does not free the node-tree itself and does no ID user counting.
 */
void node_tree_free_tree(bNodeTree &ntree);

bNodeTree *node_tree_copy_tree_ex(const bNodeTree &ntree, Main *bmain, bool do_id_user);
bNodeTree *node_tree_copy_tree(Main *bmain, const bNodeTree &ntree);

void node_tree_free_local_node(bNodeTree &ntree, bNode &node);

void node_tree_update_all_new(Main &main);

/** Update asset meta-data cache of data-block properties. */
void node_update_asset_metadata(bNodeTree &node_tree);

void node_tree_node_flag_set(bNodeTree &ntree, int flag, bool enable);

/**
 * Merge local tree results back, and free local tree.
 *
 * We have to assume the editor already changed completely.
 */
void node_tree_local_merge(Main *bmain, bNodeTree *localtree, bNodeTree *ntree);

/**
 * \note `ntree` itself has been read!
 */
void node_tree_blend_read_data(BlendDataReader *reader, ID *owner_id, bNodeTree *ntree);

bool node_is_static_socket_type(const bNodeSocketType &stype);

StringRefNull node_socket_sub_type_label(int subtype);

void node_remove_socket_ex(bNodeTree &ntree, bNode &node, bNodeSocket &sock, bool do_id_user);

void node_modify_socket_type(bNodeTree &ntree,
                             bNode &node,
                             bNodeSocket &sock,
                             StringRefNull idname);

/**
 * \note Goes over entire tree.
 */
void node_unlink_node(bNodeTree &ntree, bNode &node);

void node_unlink_attached(bNodeTree *ntree, const bNode *parent);

/**
 * Rebuild the `node_by_id` runtime vector set. Call after removing a node if not handled
 * separately. This is important instead of just using `nodes_by_id.remove()` since it maintains
 * the node order.
 */
void node_rebuild_id_vector(bNodeTree &node_tree);

/**
 * \note keeps socket list order identical, for copying links.
 * \param dst_name: The name of the copied node. This is expected to be unique in the destination
 *   tree if provided. If not provided, the src name is used and is made unique unless
 *   allow_duplicate_names is true.
 * \param dst_identifier: Same ad dst_name, but for the identifier.
 */
bNode *node_copy_with_mapping(bNodeTree *dst_tree,
                              const bNode &node_src,
                              int flag,
                              std::optional<StringRefNull> dst_unique_name,
                              std::optional<int> dst_unique_identifier,
                              Map<const bNodeSocket *, bNodeSocket *> &new_socket_map,
                              bool allow_duplicate_names = false);

/**
 * Move socket default from \a src (input socket) to locations specified by \a dst (output socket).
 * Result value moved in specific location. (potentially multiple group nodes socket values, if \a
 * dst is a group input node).
 * \note Conceptually, the effect should be such that the evaluation of
 * this graph again returns the value in src.
 */
void node_socket_move_default_value(Main &bmain,
                                    bNodeTree &tree,
                                    bNodeSocket &src,
                                    bNodeSocket &dst);

/**
 * Free the node itself.
 *
 * \note ID user reference-counting and changing the `nodes_by_id` vector are up to the caller.
 */
void node_free_node(bNodeTree *tree, bNode &node);

/**
 * Iterate over all ID usages of the given node.
 * Can be used with #BKE_library_foreach_subdata_ID_link.
 *
 * See BKE_lib_query.hh and #IDTypeInfo.foreach_id for more details.
 */
void node_node_foreach_id(bNode *node, LibraryForeachIDData *data);

/**
 * Set the mute status of a single link.
 */
void node_link_set_mute(bNodeTree &ntree, bNodeLink &link, const bool muted);

bool node_link_is_selected(const bNodeLink &link);

void node_internal_relink(bNodeTree &ntree, bNode &node);

void node_position_relative(bNode &from_node,
                            const bNode &to_node,
                            const bNodeSocket *from_sock,
                            const bNodeSocket &to_sock);

void node_position_propagate(bNode &node);

/**
 * \note Recursive.
 */
bNode *node_find_root_parent(bNode &node);

/**
 * Iterate over a chain of nodes, starting with \a node_start, executing
 * \a callback for each node (which can return false to end iterator).
 *
 * \param reversed: for backwards iteration
 * \note Recursive
 */
void node_chain_iterator(const bNodeTree *ntree,
                         const bNode *node_start,
                         bool (*callback)(bNode *, bNode *, void *, const bool),
                         void *userdata,
                         bool reversed);

/**
 * Iterate over a chain of nodes, starting with \a node_start, executing
 * \a callback for each node (which can return false to end iterator).
 *
 * Faster than node_chain_iterator. Iter only once per node.
 * Can be called recursively (using another node_chain_iteratorBackwards) by
 * setting the recursion_lvl accordingly.
 *
 * WARN: No node is guaranteed to be iterated as a to_node,
 * since it could have been iterated earlier as a from_node.
 *
 * \note Needs updated socket links (ntreeUpdateTree).
 * \note Recursive
 */
void node_chain_iterator_backwards(const bNodeTree *ntree,
                                   bNode *node_start,
                                   bool (*callback)(bNode *, bNode *, void *),
                                   void *userdata,
                                   int recursion_lvl);

/**
 * Iterate over all parents of \a node, executing \a callback for each parent
 * (which can return false to end iterator)
 *
 * \note Recursive
 */
void node_parents_iterator(bNode *node, bool (*callback)(bNode *, void *), void *userdata);

/**
 * A dangling reroute node is a reroute node that does *not* have a "data source", i.e. no
 * non-reroute node is connected to its input.
 */
bool node_is_dangling_reroute(const bNodeTree &ntree, const bNode &node);

bNode *node_get_active_paint_canvas(bNodeTree &ntree);

/**
 * \brief Does the given node supports the sub active flag.
 *
 * \param sub_active: The active flag to check. #NODE_ACTIVE_TEXTURE / #NODE_ACTIVE_PAINT_CANVAS.
 */
bool node_supports_active_flag(const bNode &node, int sub_activity);

void node_set_socket_availability(bNodeTree &ntree, bNodeSocket &sock, bool is_available);

/**
 * If the node implements a `declare` function, this function makes sure that `node->declaration`
 * is up to date. It is expected that the sockets of the node are up to date already.
 */
bool node_declaration_ensure(bNodeTree &ntree, bNode &node);

/**
 * Just update `node->declaration` if necessary. This can also be called on nodes that may not be
 * up to date (e.g. because the need versioning or are dynamic).
 */
bool node_declaration_ensure_on_outdated_node(bNodeTree &ntree, bNode &node);

/**
 * Update `socket->declaration` for all sockets in the node. This assumes that the node declaration
 * and sockets are up to date already.
 */
void node_socket_declarations_update(bNode *node);

/* Node Previews */
bool node_preview_used(const bNode &node);

struct bNodePreview {
  ImBuf *ibuf = nullptr;

  bNodePreview() = default;
  bNodePreview(const bNodePreview &other);
  bNodePreview(bNodePreview &&other);
  ~bNodePreview();
};

bNodePreview *node_preview_verify(Map<bNodeInstanceKey, bNodePreview> &previews,
                                  bNodeInstanceKey key,
                                  int xsize,
                                  int ysize,
                                  bool create);

void node_preview_init_tree(bNodeTree *ntree, int xsize, int ysize);

void node_preview_remove_unused(bNodeTree *ntree);

void node_preview_merge_tree(bNodeTree *to_ntree, bNodeTree *from_ntree, bool remove_old);

/* -------------------------------------------------------------------- */
/** \name Node Type Access
 * \{ */

std::string node_label(const bNodeTree &ntree, const bNode &node);

/**
 * Get node socket label if it is set.
 */
StringRefNull node_socket_label(const bNodeSocket &sock);

/**
 * Get node socket short label if it is set.
 * It is used when grouping sockets under panels, to avoid redundancy in the label.
 */
std::optional<StringRefNull> node_socket_short_label(const bNodeSocket &sock);

/**
 * Get node socket translation context if it is set.
 */
const char *node_socket_translation_context(const bNodeSocket &sock);

NodeColorTag node_color_tag(const bNode &node);

/**
 * Initialize a new node type struct with default values and callbacks.
 */
void node_type_base(bNodeType &ntype,
                    std::string idname,
                    std::optional<int16_t> legacy_type = std::nullopt);

void node_type_socket_templates(bNodeType *ntype,
                                bNodeSocketTemplate *inputs,
                                bNodeSocketTemplate *outputs);

void node_type_size(bNodeType &ntype, int width, int minwidth, int maxwidth);

enum class eNodeSizePreset : int8_t {
  Default,
  Small,
  Middle,
  Large,
};

void node_type_size_preset(bNodeType &ntype, eNodeSizePreset size);

/* -------------------------------------------------------------------- */
/** \name Node Generic Functions
 * \{ */

bool node_is_connected_to_output(const bNodeTree &ntree, const bNode &node);

bNodeSocket *node_find_enabled_socket(bNode &node, eNodeSocketInOut in_out, StringRef name);

bNodeSocket *node_find_enabled_input_socket(bNode &node, StringRef name);

bNodeSocket *node_find_enabled_output_socket(bNode &node, StringRef name);

extern bNodeTreeType NodeTreeTypeUndefined;
extern bNodeType NodeTypeUndefined;
extern bNodeSocketType NodeSocketTypeUndefined;

std::optional<eCustomDataType> socket_type_to_custom_data_type(eNodeSocketDatatype type);
std::optional<eNodeSocketDatatype> custom_data_type_to_socket_type(eCustomDataType type);
const CPPType *socket_type_to_geo_nodes_base_cpp_type(eNodeSocketDatatype type);
std::optional<eNodeSocketDatatype> geo_nodes_base_cpp_type_to_socket_type(const CPPType &type);
std::optional<VolumeGridType> socket_type_to_grid_type(eNodeSocketDatatype type);
std::optional<eNodeSocketDatatype> grid_type_to_socket_type(VolumeGridType type);

/**
 * Contains information about a specific kind of zone (e.g. simulation or repeat zone in geometry
 * nodes). This allows writing code that works for all kinds of zones automatically, reducing
 * redundancy and the amount of boilerplate needed when adding a new zone type.
 */
class bNodeZoneType {
 public:
  std::string input_idname;
  std::string output_idname;
  int input_type;
  int output_type;
  int theme_id;

  virtual ~bNodeZoneType() = default;

  virtual const int &get_corresponding_output_id(const bNode &input_bnode) const = 0;

  int &get_corresponding_output_id(bNode &input_bnode) const
  {
    return const_cast<int &>(
        this->get_corresponding_output_id(const_cast<const bNode &>(input_bnode)));
  }

  const bNode *get_corresponding_input(const bNodeTree &tree, const bNode &output_bnode) const;
  bNode *get_corresponding_input(bNodeTree &tree, const bNode &output_bnode) const;

  const bNode *get_corresponding_output(const bNodeTree &tree, const bNode &input_bnode) const;
  bNode *get_corresponding_output(bNodeTree &tree, const bNode &input_bnode) const;
};

void register_node_zone_type(const bNodeZoneType &zone_type);

Span<const bNodeZoneType *> all_zone_types();
Span<int> all_zone_node_types();
Span<int> all_zone_input_node_types();
Span<int> all_zone_output_node_types();
const bNodeZoneType *zone_type_by_node_type(const int node_type);

inline bool bNodeType::is_type(const StringRef query_idname) const
{
  /* Ensure that the given idname exists to check for typos. */
  BLI_assert(node_type_find(query_idname) != nullptr);
  return this->idname == query_idname;
}

}  // namespace blender::bke

#define NODE_STORAGE_FUNCS(StorageT) \
  [[maybe_unused]] static StorageT &node_storage(bNode &node) \
  { \
    return *static_cast<StorageT *>(node.storage); \
  } \
  [[maybe_unused]] static const StorageT &node_storage(const bNode &node) \
  { \
    return *static_cast<const StorageT *>(node.storage); \
  }

constexpr int NODE_DEFAULT_MAX_WIDTH = 700;
constexpr int GROUP_NODE_DEFAULT_WIDTH = 140;
constexpr int GROUP_NODE_MAX_WIDTH = NODE_DEFAULT_MAX_WIDTH;
constexpr int GROUP_NODE_MIN_WIDTH = 60;
