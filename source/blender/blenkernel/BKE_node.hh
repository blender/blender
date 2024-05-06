/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "DNA_listBase.h"

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
struct bNodeExecData;
struct bNodeInstanceHash;
struct bNodeLink;
struct bNodeSocket;
struct bNodeStack;
struct bNodeTree;
struct bNodeTreeExec;
struct bNodeTreeType;
struct uiLayout;

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
  char name[64];                /* MAX_NAME */
  float val1, val2, val3, val4; /* default alloc value for inputs */
  float min, max;
  int subtype; /* would use PropertySubType but this is a bad level include to use RNA */
  int flag;

  /* after this line is used internal only */
  bNodeSocket *sock;   /* used to hold verified socket */
  char identifier[64]; /* generated from name */
};

/* Use `void *` for callbacks that require C++. This is rather ugly, but works well for now. This
 * would not be necessary if we would use bNodeSocketType and bNodeType only in C++ code.
 * However, achieving this requires quite a few changes currently. */
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
}  // namespace nodes
namespace realtime_compositor {
class Context;
class NodeOperation;
class ShaderNode;
}  // namespace realtime_compositor
}  // namespace blender

using NodeMultiFunctionBuildFunction = void (*)(blender::nodes::NodeMultiFunctionBuilder &builder);
using NodeGeometryExecFunction = void (*)(blender::nodes::GeoNodeExecParams params);
using NodeDeclareFunction = void (*)(blender::nodes::NodeDeclarationBuilder &builder);
using NodeDeclareDynamicFunction = void (*)(const bNodeTree &tree,
                                            const bNode &node,
                                            blender::nodes::NodeDeclarationBuilder &builder);
using SocketGetCPPValueFunction = void (*)(const void *socket_value, void *r_value);
using SocketGetGeometryNodesCPPValueFunction = void (*)(const void *socket_value, void *r_value);

/* Adds socket link operations that are specific to this node type. */
using NodeGatherSocketLinkOperationsFunction =
    void (*)(blender::nodes::GatherLinkSearchOpParams &params);

/* Adds node add menu operations that are specific to this node type. */
using NodeGatherAddOperationsFunction =
    void (*)(blender::nodes::GatherAddNodeSearchParams &params);

using NodeGetCompositorOperationFunction = blender::realtime_compositor::NodeOperation
    *(*)(blender::realtime_compositor::Context &context, blender::nodes::DNode node);
using NodeGetCompositorShaderNodeFunction =
    blender::realtime_compositor::ShaderNode *(*)(blender::nodes::DNode node);
using NodeExtraInfoFunction = void (*)(blender::nodes::NodeExtraInfoParams &params);

/**
 * \brief Defines a socket type.
 *
 * Defines the appearance and behavior of a socket in the UI.
 */
struct bNodeSocketType {
  /** Identifier name. */
  char idname[64];
  /** Type label. */
  char label[64];
  /** Sub-type label. */
  char subtype_label[64];

  void (*draw)(
      bContext *C, uiLayout *layout, PointerRNA *ptr, PointerRNA *node_ptr, const char *text);
  void (*draw_color)(bContext *C, PointerRNA *ptr, PointerRNA *node_ptr, float *r_color);
  void (*draw_color_simple)(const bNodeSocketType *socket_type, float *r_color);

  void (*interface_draw)(ID *id, bNodeTreeInterfaceSocket *socket, bContext *C, uiLayout *layout);
  void (*interface_init_socket)(ID *id,
                                const bNodeTreeInterfaceSocket *interface_socket,
                                bNode *node,
                                bNodeSocket *socket,
                                const char *data_path);
  void (*interface_from_socket)(ID *id,
                                bNodeTreeInterfaceSocket *interface_socket,
                                const bNode *node,
                                const bNodeSocket *socket);

  /* RNA integration */
  ExtensionRNA ext_socket;
  ExtensionRNA ext_interface;

  /* for standard socket types in C */
  int type, subtype;

  /* When set, bNodeSocket->limit does not have any effect anymore. */
  bool use_link_limits_of_type;
  int input_link_limit;
  int output_link_limit;

  /* Callback to free the socket type. */
  void (*free_self)(bNodeSocketType *stype);

  /* Return the CPPType of this socket. */
  const blender::CPPType *base_cpp_type;
  /* Get the value of this socket in a generic way. */
  SocketGetCPPValueFunction get_base_cpp_value;
  /* Get geometry nodes cpp type. */
  const blender::CPPType *geometry_nodes_cpp_type;
  /* Get geometry nodes cpp value. */
  SocketGetGeometryNodesCPPValueFunction get_geometry_nodes_cpp_value;
  /* Default value for this socket type. */
  const void *geometry_nodes_default_cpp_value;
};

using NodeInitExecFunction = void *(*)(bNodeExecContext *context,
                                       bNode *node,
                                       bNodeInstanceKey key);
using NodeFreeExecFunction = void (*)(void *nodedata);
using NodeExecFunction = void (*)(
    void *data, int thread, bNode *, bNodeExecData *execdata, bNodeStack **in, bNodeStack **out);
using NodeGPUExecFunction = int (*)(
    GPUMaterial *mat, bNode *node, bNodeExecData *execdata, GPUNodeStack *in, GPUNodeStack *out);
using NodeMaterialXFunction = void (*)(void *data, bNode *node, bNodeSocket *out);

/**
 * \brief Defines a node type.
 *
 * Initial attributes and constants for a node as well as callback functions
 * implementing the node behavior.
 */
struct bNodeType {
  char idname[64]; /* identifier name */
  int type;

  char ui_name[64]; /* MAX_NAME */
  char ui_description[256];
  int ui_icon;
  /** Should usually use the idname instead, but this enum type is still exposed in Python. */
  const char *enum_name_legacy;

  float width, minwidth, maxwidth;
  float height, minheight, maxheight;
  short nclass, flag;

  /* templates for static sockets */
  bNodeSocketTemplate *inputs, *outputs;

  char storagename[64]; /* struct name for DNA */

  /* Draw the option buttons on the node */
  void (*draw_buttons)(uiLayout *, bContext *C, PointerRNA *ptr);
  /* Additional parameters in the side panel */
  void (*draw_buttons_ex)(uiLayout *, bContext *C, PointerRNA *ptr);

  /* Additional drawing on backdrop */
  void (*draw_backdrop)(SpaceNode *snode, ImBuf *backdrop, bNode *node, int x, int y);

  /**
   * Optional custom label function for the node header.
   * \note Used as a fallback when #bNode.label isn't set.
   */
  void (*labelfunc)(const bNodeTree *ntree, const bNode *node, char *label, int label_maxncpy);

  /** Optional override for node class, used for drawing node header. */
  int (*ui_class)(const bNode *node);

  /** Called when the node is updated in the editor. */
  void (*updatefunc)(bNodeTree *ntree, bNode *node);
  /** Check and update if internal ID data has changed. */
  void (*group_update_func)(bNodeTree *ntree, bNode *node);

  /**
   * Initialize a new node instance of this type after creation.
   *
   * \note Assignments to `node->id` must not increment the user of the ID.
   * This is handled by the caller of this callback.
   */
  void (*initfunc)(bNodeTree *ntree, bNode *node);
  /**
   * Free the node instance.
   *
   * \note Access to `node->id` must be avoided in this function as this is called
   * while freeing #Main, the state of this ID is undefined.
   * Higher level logic to remove the node handles the user-count.
   */
  void (*freefunc)(bNode *node);
  /** Make a copy of the node instance. */
  void (*copyfunc)(bNodeTree *dest_ntree, bNode *dest_node, const bNode *src_node);

  /* Registerable API callback versions, called in addition to C callbacks */
  void (*initfunc_api)(const bContext *C, PointerRNA *ptr);
  void (*freefunc_api)(PointerRNA *ptr);
  void (*copyfunc_api)(PointerRNA *ptr, const bNode *src_node);

  /**
   * An additional poll test for deciding whether nodes should be an option in search menus.
   * Potentially more strict poll than #poll(), but doesn't have to check the same things.
   */
  bool (*add_ui_poll)(const bContext *C);

  /**
   * Can this node type be added to a node tree?
   * \param r_disabled_hint: Hint to display in the UI when the poll fails.
   *                         The callback can set this to a static string without having to
   *                         null-check it (or without setting it to null if it's not used).
   *                         The caller must pass a valid `const char **` and null-initialize it
   *                         when it's not just a dummy, that is, if it actually wants to access
   *                         the returned disabled-hint (null-check needed!).
   */
  bool (*poll)(const bNodeType *ntype, const bNodeTree *nodetree, const char **r_disabled_hint);
  /**
   * Can this node be added to a node tree?
   * \param r_disabled_hint: See `poll()`.
   */
  bool (*poll_instance)(const bNode *node,
                        const bNodeTree *nodetree,
                        const char **r_disabled_hint);

  /* Optional handling of link insertion. Returns false if the link shouldn't be created. */
  bool (*insert_link)(bNodeTree *ntree, bNode *node, bNodeLink *link);

  void (*free_self)(bNodeType *ntype);

  /* **** execution callbacks **** */
  NodeInitExecFunction init_exec_fn;
  NodeFreeExecFunction free_exec_fn;
  NodeExecFunction exec_fn;
  /* gpu */
  NodeGPUExecFunction gpu_fn;
  /* MaterialX */
  NodeMaterialXFunction materialx_fn;

  /* Get an instance of this node's compositor operation. Freeing the instance is the
   * responsibility of the caller. */
  NodeGetCompositorOperationFunction get_compositor_operation;

  /* Get an instance of this node's compositor shader node. Freeing the instance is the
   * responsibility of the caller. */
  NodeGetCompositorShaderNodeFunction get_compositor_shader_node;

  /* A message to display in the node header for unsupported realtime compositor nodes. The message
   * is assumed to be static and thus require no memory handling. This field is to be removed when
   * all nodes are supported. */
  const char *realtime_compositor_unsupported_message;

  /* Build a multi-function for this node. */
  NodeMultiFunctionBuildFunction build_multi_function;

  /* Execute a geometry node. */
  NodeGeometryExecFunction geometry_node_execute;

  /**
   * Declares which sockets and panels the node has. It has to be able to generate a declaration
   * with and without a specific node context. If the declaration depends on the node, but the node
   * is not provided, then the declaration should be generated as much as possible and everything
   * that depends on the node context should be skipped.
   */
  NodeDeclareFunction declare;

  /**
   * Declaration of the node outside of any context. If the node declaration is never dependent on
   * the node context, this declaration is also shared with the corresponding node instances.
   * Otherwise, it mainly allows checking what sockets a node will have, without having to create
   * the node. In this case, the static declaration is mostly just a hint, and does not have to
   * match with the final node.
   */
  blender::nodes::NodeDeclaration *static_declaration;

  /**
   * Add to the list of search names and operations gathered by node link drag searching.
   * Usually it isn't necessary to override the default behavior here, but a node type can have
   * custom behavior here like adding custom search items.
   */
  NodeGatherSocketLinkOperationsFunction gather_link_search_ops;

  /** Get extra information that is drawn next to the node. */
  NodeExtraInfoFunction get_extra_info;

  /**
   * Registers operators that are specific to this node. This allows nodes to be more
   * self-contained compared to the alternative to registering all operators in a more central
   * place.
   */
  void (*register_operators)();

  /** True when the node cannot be muted. */
  bool no_muting;
  /** True when the node still works but it's usage is discouraged. */
  const char *deprecation_notice;

  /* RNA integration */
  ExtensionRNA rna_ext;
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

struct bNodeTreeExec;

using bNodeClassCallback = void (*)(void *calldata, int nclass, const char *name);
struct bNodeTreeType {
  int type;        /* type identifier */
  char idname[64]; /* identifier name */

  /* The ID name of group nodes for this type. */
  char group_idname[64];

  char ui_name[64];
  char ui_description[256];
  int ui_icon;

  /* callbacks */
  /* Iteration over all node classes. */
  void (*foreach_nodeclass)(Scene *scene, void *calldata, bNodeClassCallback func);
  /* Check visibility in the node editor */
  bool (*poll)(const bContext *C, bNodeTreeType *ntreetype);
  /* Select a node tree from the context */
  void (*get_from_context)(
      const bContext *C, bNodeTreeType *ntreetype, bNodeTree **r_ntree, ID **r_id, ID **r_from);

  /* calls allowing threaded composite */
  void (*localize)(bNodeTree *localtree, bNodeTree *ntree);
  void (*local_merge)(Main *bmain, bNodeTree *localtree, bNodeTree *ntree);

  /* Tree update. Overrides `nodetype->updatetreefunc`. */
  void (*update)(bNodeTree *ntree);

  bool (*validate_link)(eNodeSocketDatatype from, eNodeSocketDatatype to);

  void (*node_add_init)(bNodeTree *ntree, bNode *bnode);

  /* Check if the socket type is valid for this tree type. */
  bool (*valid_socket_type)(bNodeTreeType *ntreetype, bNodeSocketType *socket_type);

  /* RNA integration */
  ExtensionRNA rna_ext;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic API, Trees
 * \{ */

bNodeTreeType *ntreeTypeFind(const char *idname);
void ntreeTypeAdd(bNodeTreeType *nt);
void ntreeTypeFreeLink(const bNodeTreeType *nt);
bool ntreeIsRegistered(const bNodeTree *ntree);
GHashIterator *ntreeTypeGetIterator();

/* Helper macros for iterating over tree types. */
#define NODE_TREE_TYPES_BEGIN(ntype) \
  { \
    GHashIterator *__node_tree_type_iter__ = ntreeTypeGetIterator(); \
    for (; !BLI_ghashIterator_done(__node_tree_type_iter__); \
         BLI_ghashIterator_step(__node_tree_type_iter__)) \
    { \
      bNodeTreeType *ntype = (bNodeTreeType *)BLI_ghashIterator_getValue(__node_tree_type_iter__);

#define NODE_TREE_TYPES_END \
  } \
  BLI_ghashIterator_free(__node_tree_type_iter__); \
  } \
  (void)0

/**
 * Try to initialize all type-info in a node tree.
 *
 * \note In general undefined type-info is a perfectly valid case,
 * the type may just be registered later.
 * In that case the update_typeinfo function will set type-info on registration
 * and do necessary updates.
 */
void ntreeSetTypes(const bContext *C, bNodeTree *ntree);

bNodeTree *ntreeAddTree(Main *bmain, const char *name, const char *idname);

/**
 * Add a new (non-embedded) node tree, like #ntreeAddTree, but allows to create it inside a given
 * library. Used mainly by readfile code when versioning linked data.
 */
bNodeTree *BKE_node_tree_add_in_lib(Main *bmain,
                                    Library *owner_library,
                                    const char *name,
                                    const char *idname);

/**
 * Free tree which is embedded into another data-block.
 */
void ntreeFreeEmbeddedTree(bNodeTree *ntree);

/**
 * Get address of potential node-tree pointer of given ID.
 *
 * \warning Using this function directly is potentially dangerous, if you don't know or are not
 * sure, please use `ntreeFromID()` instead.
 */
bNodeTree **BKE_ntree_ptr_from_id(ID *id);

/**
 * Returns the private NodeTree object of the data-block, if it has one.
 */
bNodeTree *ntreeFromID(ID *id);

void ntreeFreeLocalTree(bNodeTree *ntree);

/**
 * Check recursively if a node tree contains another.
 */
bool ntreeContainsTree(const bNodeTree *tree_to_search_in, const bNodeTree *tree_to_search_for);

void ntreeUpdateAllUsers(Main *main, ID *id);

/**
 * XXX: old trees handle output flags automatically based on special output
 * node types and last active selection.
 * New tree types have a per-output socket flag to indicate the final output to use explicitly.
 */
void ntreeSetOutput(bNodeTree *ntree);

/**
 * Returns localized tree for execution in threads.
 *
 * \param new_owner_id: the owner ID of the localized nodetree, may be null if unknown or
 * irrelevant.
 */
bNodeTree *ntreeLocalize(bNodeTree *ntree, ID *new_owner_id);

/**
 * This is only direct data, tree itself should have been written.
 */
void ntreeBlendWrite(BlendWriter *writer, bNodeTree *ntree);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic API, Nodes
 * \{ */

bNodeType *nodeTypeFind(const char *idname);
const char *nodeTypeFindAlias(const char *idname);
void nodeRegisterType(bNodeType *ntype);
void nodeUnregisterType(bNodeType *ntype);
void nodeRegisterAlias(bNodeType *nt, const char *alias);
GHashIterator *nodeTypeGetIterator();

/* Helper macros for iterating over node types. */
#define NODE_TYPES_BEGIN(ntype) \
  { \
    GHashIterator *__node_type_iter__ = nodeTypeGetIterator(); \
    for (; !BLI_ghashIterator_done(__node_type_iter__); \
         BLI_ghashIterator_step(__node_type_iter__)) { \
      bNodeType *ntype = (bNodeType *)BLI_ghashIterator_getValue(__node_type_iter__);

#define NODE_TYPES_END \
  } \
  BLI_ghashIterator_free(__node_type_iter__); \
  } \
  ((void)0)

bNodeSocketType *nodeSocketTypeFind(const char *idname);
void nodeRegisterSocketType(bNodeSocketType *stype);
void nodeUnregisterSocketType(bNodeSocketType *stype);
bool nodeSocketIsRegistered(const bNodeSocket *sock);
GHashIterator *nodeSocketTypeGetIterator();
const char *nodeSocketTypeLabel(const bNodeSocketType *stype);

const char *nodeStaticSocketType(int type, int subtype);
const char *nodeStaticSocketInterfaceTypeNew(int type, int subtype);
const char *nodeStaticSocketLabel(int type, int subtype);

/* Helper macros for iterating over node types. */
#define NODE_SOCKET_TYPES_BEGIN(stype) \
  { \
    GHashIterator *__node_socket_type_iter__ = nodeSocketTypeGetIterator(); \
    for (; !BLI_ghashIterator_done(__node_socket_type_iter__); \
         BLI_ghashIterator_step(__node_socket_type_iter__)) \
    { \
      bNodeSocketType *stype = (bNodeSocketType *)BLI_ghashIterator_getValue( \
          __node_socket_type_iter__);

#define NODE_SOCKET_TYPES_END \
  } \
  BLI_ghashIterator_free(__node_socket_type_iter__); \
  } \
  ((void)0)

bNodeSocket *nodeFindSocket(const bNode *node, eNodeSocketInOut in_out, const char *identifier);
bNodeSocket *nodeAddSocket(bNodeTree *ntree,
                           bNode *node,
                           eNodeSocketInOut in_out,
                           const char *idname,
                           const char *identifier,
                           const char *name);
bNodeSocket *nodeAddStaticSocket(bNodeTree *ntree,
                                 bNode *node,
                                 eNodeSocketInOut in_out,
                                 int type,
                                 int subtype,
                                 const char *identifier,
                                 const char *name);
void nodeRemoveSocket(bNodeTree *ntree, bNode *node, bNodeSocket *sock);

void nodeModifySocketTypeStatic(
    bNodeTree *ntree, bNode *node, bNodeSocket *sock, int type, int subtype);

bNode *nodeAddNode(const bContext *C, bNodeTree *ntree, const char *idname);
bNode *nodeAddStaticNode(const bContext *C, bNodeTree *ntree, int type);

/**
 * Find the first available, non-duplicate name for a given node.
 */
void nodeUniqueName(bNodeTree *ntree, bNode *node);
/**
 * Create a new unique integer identifier for the node. Also set the node's
 * index in the tree, which is an eagerly maintained cache.
 */
void nodeUniqueID(bNodeTree *ntree, bNode *node);

/**
 * Delete node, associated animation data and ID user count.
 */
void nodeRemoveNode(Main *bmain, bNodeTree *ntree, bNode *node, bool do_id_user);

void nodeDimensionsGet(const bNode *node, float *r_width, float *r_height);
void nodeTagUpdateID(bNode *node);
void nodeInternalLinks(bNode *node, bNodeLink **r_links, int *r_len);

/**
 * Also used via RNA API, so we check for proper input output direction.
 */
bNodeLink *nodeAddLink(
    bNodeTree *ntree, bNode *fromnode, bNodeSocket *fromsock, bNode *tonode, bNodeSocket *tosock);
void nodeRemLink(bNodeTree *ntree, bNodeLink *link);
void nodeRemSocketLinks(bNodeTree *ntree, bNodeSocket *sock);

bool nodeLinkIsHidden(const bNodeLink *link);

void nodeAttachNode(bNodeTree *ntree, bNode *node, bNode *parent);
void nodeDetachNode(bNodeTree *ntree, bNode *node);

/**
 * Same as above but expects that the socket definitely is in the node tree.
 */
void nodeFindNode(bNodeTree *ntree, bNodeSocket *sock, bNode **r_node, int *r_sockindex);
/**
 * Finds a node based on its name.
 */
bNode *nodeFindNodebyName(bNodeTree *ntree, const char *name);
/**
 * Finds a node based on given socket and returns true on success.
 */
bool nodeFindNodeTry(bNodeTree *ntree, bNodeSocket *sock, bNode **r_node, int *r_sockindex);

bool nodeIsParentAndChild(const bNode *parent, const bNode *child);

int nodeCountSocketLinks(const bNodeTree *ntree, const bNodeSocket *sock);

/**
 * Selects or deselects the node. If the node is deselected, all its sockets are deselected too.
 * \return True if any selection was changed.
 */
bool nodeSetSelected(bNode *node, bool select);
/**
 * Two active flags, ID nodes have special flag for buttons display.
 */
void nodeSetActive(bNodeTree *ntree, bNode *node);
bNode *nodeGetActive(bNodeTree *ntree);
void nodeClearActive(bNodeTree *ntree);
/**
 * Two active flags, ID nodes have special flag for buttons display.
 */
bNode *nodeGetActiveTexture(bNodeTree *ntree);

int nodeSocketLinkLimit(const bNodeSocket *sock);

/**
 * Node Instance Hash.
 */
struct bNodeInstanceHash {
  /** XXX should be made a direct member, #GHash allocation needs to support it */
  GHash *ghash;
};

using bNodeInstanceValueFP = void (*)(void *value);

/**
 * Magic number for initial hash key.
 */
extern const bNodeInstanceKey NODE_INSTANCE_KEY_BASE;
extern const bNodeInstanceKey NODE_INSTANCE_KEY_NONE;

bNodeInstanceKey BKE_node_instance_key(bNodeInstanceKey parent_key,
                                       const bNodeTree *ntree,
                                       const bNode *node);

bNodeInstanceHash *BKE_node_instance_hash_new(const char *info);
void BKE_node_instance_hash_free(bNodeInstanceHash *hash, bNodeInstanceValueFP valfreefp);
void BKE_node_instance_hash_insert(bNodeInstanceHash *hash, bNodeInstanceKey key, void *value);
void *BKE_node_instance_hash_lookup(bNodeInstanceHash *hash, bNodeInstanceKey key);
int BKE_node_instance_hash_remove(bNodeInstanceHash *hash,
                                  bNodeInstanceKey key,
                                  bNodeInstanceValueFP valfreefp);
void BKE_node_instance_hash_clear(bNodeInstanceHash *hash, bNodeInstanceValueFP valfreefp);
void *BKE_node_instance_hash_pop(bNodeInstanceHash *hash, bNodeInstanceKey key);
int BKE_node_instance_hash_haskey(bNodeInstanceHash *hash, bNodeInstanceKey key);
int BKE_node_instance_hash_size(bNodeInstanceHash *hash);

void BKE_node_instance_hash_clear_tags(bNodeInstanceHash *hash);
void BKE_node_instance_hash_tag(bNodeInstanceHash *hash, void *value);
bool BKE_node_instance_hash_tag_key(bNodeInstanceHash *hash, bNodeInstanceKey key);
void BKE_node_instance_hash_remove_untagged(bNodeInstanceHash *hash,
                                            bNodeInstanceValueFP valfreefp);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Type Access
 * \{ */

bool nodeGroupPoll(const bNodeTree *nodetree,
                   const bNodeTree *grouptree,
                   const char **r_disabled_hint);

void node_type_base_custom(
    bNodeType *ntype, const char *idname, const char *name, const char *enum_name, short nclass);

/**
 * \warning Nodes defining a storage type _must_ allocate this for new nodes.
 * Otherwise nodes will reload as undefined (#46619).
 */
void node_type_storage(bNodeType *ntype,
                       const char *storagename,
                       void (*freefunc)(bNode *node),
                       void (*copyfunc)(bNodeTree *dest_ntree,
                                        bNode *dest_node,
                                        const bNode *src_node));

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common Node Types
 * \{ */

#define NODE_UNDEFINED -2 /* node type is not registered */
#define NODE_CUSTOM -1    /* for dynamically registered custom types */
#define NODE_GROUP 2
// #define NODE_FORLOOP 3       /* deprecated */
// #define NODE_WHILELOOP   4   /* deprecated */
#define NODE_FRAME 5
#define NODE_REROUTE 6
#define NODE_GROUP_INPUT 7
#define NODE_GROUP_OUTPUT 8
#define NODE_CUSTOM_GROUP 9

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

void BKE_node_tree_iter_init(NodeTreeIterStore *ntreeiter, Main *bmain);
bool BKE_node_tree_iter_step(NodeTreeIterStore *ntreeiter, bNodeTree **r_nodetree, ID **r_id);

#define FOREACH_NODETREE_BEGIN(bmain, _nodetree, _id) \
  { \
    NodeTreeIterStore _nstore; \
    bNodeTree *_nodetree; \
    ID *_id; \
    /* avoid compiler warning about unused variables */ \
    BKE_node_tree_iter_init(&_nstore, bmain); \
    while (BKE_node_tree_iter_step(&_nstore, &_nodetree, &_id) == true) { \
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

void BKE_nodetree_remove_layer_n(bNodeTree *ntree, Scene *scene, int layer_index);

/* -------------------------------------------------------------------- */
/** \name Shader Nodes
 * \{ */

/* NOTE: types are needed to restore callbacks, don't change values. */

// #define SH_NODE_MATERIAL  100
#define SH_NODE_RGB 101
#define SH_NODE_VALUE 102
#define SH_NODE_MIX_RGB_LEGACY 103
#define SH_NODE_VALTORGB 104
#define SH_NODE_RGBTOBW 105
#define SH_NODE_SHADERTORGB 106
// #define SH_NODE_TEXTURE       106
#define SH_NODE_NORMAL 107
// #define SH_NODE_GEOMETRY  108
#define SH_NODE_MAPPING 109
#define SH_NODE_CURVE_VEC 110
#define SH_NODE_CURVE_RGB 111
#define SH_NODE_CAMERA 114
#define SH_NODE_MATH 115
#define SH_NODE_VECTOR_MATH 116
#define SH_NODE_SQUEEZE 117
// #define SH_NODE_MATERIAL_EXT  118
#define SH_NODE_INVERT 119
#define SH_NODE_SEPRGB_LEGACY 120
#define SH_NODE_COMBRGB_LEGACY 121
#define SH_NODE_HUE_SAT 122

#define SH_NODE_OUTPUT_MATERIAL 124
#define SH_NODE_OUTPUT_WORLD 125
#define SH_NODE_OUTPUT_LIGHT 126
#define SH_NODE_FRESNEL 127
#define SH_NODE_MIX_SHADER 128
#define SH_NODE_ATTRIBUTE 129
#define SH_NODE_BACKGROUND 130
#define SH_NODE_BSDF_GLOSSY 131
#define SH_NODE_BSDF_DIFFUSE 132
#define SH_NODE_BSDF_GLOSSY_LEGACY 133
#define SH_NODE_BSDF_GLASS 134
#define SH_NODE_BSDF_TRANSLUCENT 137
#define SH_NODE_BSDF_TRANSPARENT 138
#define SH_NODE_BSDF_SHEEN 139
#define SH_NODE_EMISSION 140
#define SH_NODE_NEW_GEOMETRY 141
#define SH_NODE_LIGHT_PATH 142
#define SH_NODE_TEX_IMAGE 143
#define SH_NODE_TEX_SKY 145
#define SH_NODE_TEX_GRADIENT 146
#define SH_NODE_TEX_VORONOI 147
#define SH_NODE_TEX_MAGIC 148
#define SH_NODE_TEX_WAVE 149
#define SH_NODE_TEX_NOISE 150
#define SH_NODE_TEX_MUSGRAVE_DEPRECATED 152
#define SH_NODE_TEX_COORD 155
#define SH_NODE_ADD_SHADER 156
#define SH_NODE_TEX_ENVIRONMENT 157
// #define SH_NODE_OUTPUT_TEXTURE 158
#define SH_NODE_HOLDOUT 159
#define SH_NODE_LAYER_WEIGHT 160
#define SH_NODE_VOLUME_ABSORPTION 161
#define SH_NODE_VOLUME_SCATTER 162
#define SH_NODE_GAMMA 163
#define SH_NODE_TEX_CHECKER 164
#define SH_NODE_BRIGHTCONTRAST 165
#define SH_NODE_LIGHT_FALLOFF 166
#define SH_NODE_OBJECT_INFO 167
#define SH_NODE_PARTICLE_INFO 168
#define SH_NODE_TEX_BRICK 169
#define SH_NODE_BUMP 170
#define SH_NODE_SCRIPT 171
#define SH_NODE_AMBIENT_OCCLUSION 172
#define SH_NODE_BSDF_REFRACTION 173
#define SH_NODE_TANGENT 174
#define SH_NODE_NORMAL_MAP 175
#define SH_NODE_HAIR_INFO 176
#define SH_NODE_SUBSURFACE_SCATTERING 177
#define SH_NODE_WIREFRAME 178
#define SH_NODE_BSDF_TOON 179
#define SH_NODE_WAVELENGTH 180
#define SH_NODE_BLACKBODY 181
#define SH_NODE_VECT_TRANSFORM 182
#define SH_NODE_SEPHSV_LEGACY 183
#define SH_NODE_COMBHSV_LEGACY 184
#define SH_NODE_BSDF_HAIR 185
// #define SH_NODE_LAMP 186
#define SH_NODE_UVMAP 187
#define SH_NODE_SEPXYZ 188
#define SH_NODE_COMBXYZ 189
#define SH_NODE_OUTPUT_LINESTYLE 190
#define SH_NODE_UVALONGSTROKE 191
#define SH_NODE_TEX_POINTDENSITY 192
#define SH_NODE_BSDF_PRINCIPLED 193
#define SH_NODE_TEX_IES 194
#define SH_NODE_EEVEE_SPECULAR 195
#define SH_NODE_BEVEL 197
#define SH_NODE_DISPLACEMENT 198
#define SH_NODE_VECTOR_DISPLACEMENT 199
#define SH_NODE_VOLUME_PRINCIPLED 200
/* 201..700 occupied by other node types, continue from 701 */
#define SH_NODE_BSDF_HAIR_PRINCIPLED 701
#define SH_NODE_MAP_RANGE 702
#define SH_NODE_CLAMP 703
#define SH_NODE_TEX_WHITE_NOISE 704
#define SH_NODE_VOLUME_INFO 705
#define SH_NODE_VERTEX_COLOR 706
#define SH_NODE_OUTPUT_AOV 707
#define SH_NODE_VECTOR_ROTATE 708
#define SH_NODE_CURVE_FLOAT 709
#define SH_NODE_POINT_INFO 710
#define SH_NODE_COMBINE_COLOR 711
#define SH_NODE_SEPARATE_COLOR 712
#define SH_NODE_MIX 713
#define SH_NODE_BSDF_RAY_PORTAL 714

/** \} */

/* -------------------------------------------------------------------- */
/** \name Composite Nodes
 * \{ */

/* output socket defines */
#define RRES_OUT_IMAGE 0
#define RRES_OUT_ALPHA 1

/* NOTE: types are needed to restore callbacks, don't change values. */
#define CMP_NODE_VIEWER 201
#define CMP_NODE_RGB 202
#define CMP_NODE_VALUE 203
#define CMP_NODE_MIX_RGB 204
#define CMP_NODE_VALTORGB 205
#define CMP_NODE_RGBTOBW 206
#define CMP_NODE_NORMAL 207
#define CMP_NODE_CURVE_VEC 208
#define CMP_NODE_CURVE_RGB 209
#define CMP_NODE_ALPHAOVER 210
#define CMP_NODE_BLUR 211
#define CMP_NODE_FILTER 212
#define CMP_NODE_MAP_VALUE 213
#define CMP_NODE_TIME 214
#define CMP_NODE_VECBLUR 215
#define CMP_NODE_SEPRGBA_LEGACY 216
#define CMP_NODE_SEPHSVA_LEGACY 217
#define CMP_NODE_SETALPHA 218
#define CMP_NODE_HUE_SAT 219
#define CMP_NODE_IMAGE 220
#define CMP_NODE_R_LAYERS 221
#define CMP_NODE_COMPOSITE 222
#define CMP_NODE_OUTPUT_FILE 223
#define CMP_NODE_TEXTURE 224
#define CMP_NODE_TRANSLATE 225
#define CMP_NODE_ZCOMBINE 226
#define CMP_NODE_COMBRGBA_LEGACY 227
#define CMP_NODE_DILATEERODE 228
#define CMP_NODE_ROTATE 229
#define CMP_NODE_SCALE 230
#define CMP_NODE_SEPYCCA_LEGACY 231
#define CMP_NODE_COMBYCCA_LEGACY 232
#define CMP_NODE_SEPYUVA_LEGACY 233
#define CMP_NODE_COMBYUVA_LEGACY 234
#define CMP_NODE_DIFF_MATTE 235
#define CMP_NODE_COLOR_SPILL 236
#define CMP_NODE_CHROMA_MATTE 237
#define CMP_NODE_CHANNEL_MATTE 238
#define CMP_NODE_FLIP 239
/* Split viewer node is now a regular split node: CMP_NODE_SPLIT. */
#define CMP_NODE_SPLITVIEWER__DEPRECATED 240
// #define CMP_NODE_INDEX_MASK  241
#define CMP_NODE_MAP_UV 242
#define CMP_NODE_ID_MASK 243
#define CMP_NODE_DEFOCUS 244
#define CMP_NODE_DISPLACE 245
#define CMP_NODE_COMBHSVA_LEGACY 246
#define CMP_NODE_MATH 247
#define CMP_NODE_LUMA_MATTE 248
#define CMP_NODE_BRIGHTCONTRAST 249
#define CMP_NODE_GAMMA 250
#define CMP_NODE_INVERT 251
#define CMP_NODE_NORMALIZE 252
#define CMP_NODE_CROP 253
#define CMP_NODE_DBLUR 254
#define CMP_NODE_BILATERALBLUR 255
#define CMP_NODE_PREMULKEY 256
#define CMP_NODE_DIST_MATTE 257
#define CMP_NODE_VIEW_LEVELS 258
#define CMP_NODE_COLOR_MATTE 259
#define CMP_NODE_COLORBALANCE 260
#define CMP_NODE_HUECORRECT 261
#define CMP_NODE_MOVIECLIP 262
#define CMP_NODE_STABILIZE2D 263
#define CMP_NODE_TRANSFORM 264
#define CMP_NODE_MOVIEDISTORTION 265
#define CMP_NODE_DOUBLEEDGEMASK 266
#define CMP_NODE_OUTPUT_MULTI_FILE__DEPRECATED \
  267 /* DEPRECATED multi file node has been merged into regular CMP_NODE_OUTPUT_FILE */
#define CMP_NODE_MASK 268
#define CMP_NODE_KEYINGSCREEN 269
#define CMP_NODE_KEYING 270
#define CMP_NODE_TRACKPOS 271
#define CMP_NODE_INPAINT 272
#define CMP_NODE_DESPECKLE 273
#define CMP_NODE_ANTIALIASING 274
#define CMP_NODE_KUWAHARA 275
#define CMP_NODE_SPLIT 276

#define CMP_NODE_GLARE 301
#define CMP_NODE_TONEMAP 302
#define CMP_NODE_LENSDIST 303
#define CMP_NODE_SUNBEAMS 304

#define CMP_NODE_COLORCORRECTION 312
#define CMP_NODE_MASK_BOX 313
#define CMP_NODE_MASK_ELLIPSE 314
#define CMP_NODE_BOKEHIMAGE 315
#define CMP_NODE_BOKEHBLUR 316
#define CMP_NODE_SWITCH 317
#define CMP_NODE_PIXELATE 318

#define CMP_NODE_MAP_RANGE 319
#define CMP_NODE_PLANETRACKDEFORM 320
#define CMP_NODE_CORNERPIN 321
#define CMP_NODE_SWITCH_VIEW 322
#define CMP_NODE_CRYPTOMATTE_LEGACY 323
#define CMP_NODE_DENOISE 324
#define CMP_NODE_EXPOSURE 325
#define CMP_NODE_CRYPTOMATTE 326
#define CMP_NODE_POSTERIZE 327
#define CMP_NODE_CONVERT_COLOR_SPACE 328
#define CMP_NODE_SCENE_TIME 329
#define CMP_NODE_SEPARATE_XYZ 330
#define CMP_NODE_COMBINE_XYZ 331
#define CMP_NODE_COMBINE_COLOR 332
#define CMP_NODE_SEPARATE_COLOR 333

/* channel toggles */
#define CMP_CHAN_RGB 1
#define CMP_CHAN_A 2

/* Default SMAA configuration values. */
#define CMP_DEFAULT_SMAA_THRESHOLD 1.0f
#define CMP_DEFAULT_SMAA_CONTRAST_LIMIT 0.2f
#define CMP_DEFAULT_SMAA_CORNER_ROUNDING 0.25f

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Nodes
 * \{ */

#define TEX_NODE_OUTPUT 401
#define TEX_NODE_CHECKER 402
#define TEX_NODE_TEXTURE 403
#define TEX_NODE_BRICKS 404
#define TEX_NODE_MATH 405
#define TEX_NODE_MIX_RGB 406
#define TEX_NODE_RGBTOBW 407
#define TEX_NODE_VALTORGB 408
#define TEX_NODE_IMAGE 409
#define TEX_NODE_CURVE_RGB 410
#define TEX_NODE_INVERT 411
#define TEX_NODE_HUE_SAT 412
#define TEX_NODE_CURVE_TIME 413
#define TEX_NODE_ROTATE 414
#define TEX_NODE_VIEWER 415
#define TEX_NODE_TRANSLATE 416
#define TEX_NODE_COORD 417
#define TEX_NODE_DISTANCE 418
#define TEX_NODE_COMPOSE_LEGACY 419
#define TEX_NODE_DECOMPOSE_LEGACY 420
#define TEX_NODE_VALTONOR 421
#define TEX_NODE_SCALE 422
#define TEX_NODE_AT 423
#define TEX_NODE_COMBINE_COLOR 424
#define TEX_NODE_SEPARATE_COLOR 425

/* 501-599 reserved. Use like this: TEX_NODE_PROC + TEX_CLOUDS, etc */
#define TEX_NODE_PROC 500
#define TEX_NODE_PROC_MAX 600

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry Nodes
 * \{ */

#define GEO_NODE_TRIANGULATE 1000
#define GEO_NODE_TRANSFORM_GEOMETRY 1002
#define GEO_NODE_MESH_BOOLEAN 1003
#define GEO_NODE_OBJECT_INFO 1007
#define GEO_NODE_JOIN_GEOMETRY 1010
#define GEO_NODE_COLLECTION_INFO 1023
#define GEO_NODE_IS_VIEWPORT 1024
#define GEO_NODE_SUBDIVIDE_MESH 1029
#define GEO_NODE_MESH_PRIMITIVE_CUBE 1032
#define GEO_NODE_MESH_PRIMITIVE_CIRCLE 1033
#define GEO_NODE_MESH_PRIMITIVE_UV_SPHERE 1034
#define GEO_NODE_MESH_PRIMITIVE_CYLINDER 1035
#define GEO_NODE_MESH_PRIMITIVE_ICO_SPHERE 1036
#define GEO_NODE_MESH_PRIMITIVE_CONE 1037
#define GEO_NODE_MESH_PRIMITIVE_LINE 1038
#define GEO_NODE_MESH_PRIMITIVE_GRID 1039
#define GEO_NODE_BOUNDING_BOX 1042
#define GEO_NODE_SWITCH 1043
#define GEO_NODE_CURVE_TO_MESH 1045
#define GEO_NODE_RESAMPLE_CURVE 1047
#define GEO_NODE_INPUT_MATERIAL 1050
#define GEO_NODE_REPLACE_MATERIAL 1051
#define GEO_NODE_CURVE_LENGTH 1054
#define GEO_NODE_CONVEX_HULL 1056
#define GEO_NODE_SEPARATE_COMPONENTS 1059
#define GEO_NODE_CURVE_PRIMITIVE_STAR 1062
#define GEO_NODE_CURVE_PRIMITIVE_SPIRAL 1063
#define GEO_NODE_CURVE_PRIMITIVE_QUADRATIC_BEZIER 1064
#define GEO_NODE_CURVE_PRIMITIVE_BEZIER_SEGMENT 1065
#define GEO_NODE_CURVE_PRIMITIVE_CIRCLE 1066
#define GEO_NODE_VIEWER 1067
#define GEO_NODE_CURVE_PRIMITIVE_LINE 1068
#define GEO_NODE_CURVE_PRIMITIVE_QUADRILATERAL 1070
#define GEO_NODE_TRIM_CURVE 1071
#define GEO_NODE_FILL_CURVE 1075
#define GEO_NODE_INPUT_POSITION 1076
#define GEO_NODE_SET_POSITION 1077
#define GEO_NODE_INPUT_INDEX 1078
#define GEO_NODE_INPUT_NORMAL 1079
#define GEO_NODE_CAPTURE_ATTRIBUTE 1080
#define GEO_NODE_MATERIAL_SELECTION 1081
#define GEO_NODE_SET_MATERIAL 1082
#define GEO_NODE_REALIZE_INSTANCES 1083
#define GEO_NODE_ATTRIBUTE_STATISTIC 1084
#define GEO_NODE_SAMPLE_CURVE 1085
#define GEO_NODE_INPUT_TANGENT 1086
#define GEO_NODE_STRING_JOIN 1087
#define GEO_NODE_CURVE_SPLINE_PARAMETER 1088
#define GEO_NODE_FILLET_CURVE 1089
#define GEO_NODE_DISTRIBUTE_POINTS_ON_FACES 1090
#define GEO_NODE_STRING_TO_CURVES 1091
#define GEO_NODE_INSTANCE_ON_POINTS 1092
#define GEO_NODE_MESH_TO_POINTS 1093
#define GEO_NODE_POINTS_TO_VERTICES 1094
#define GEO_NODE_REVERSE_CURVE 1095
#define GEO_NODE_PROXIMITY 1096
#define GEO_NODE_SUBDIVIDE_CURVE 1097
#define GEO_NODE_INPUT_SPLINE_LENGTH 1098
#define GEO_NODE_CURVE_SPLINE_TYPE 1099
#define GEO_NODE_CURVE_SET_HANDLE_TYPE 1100
#define GEO_NODE_POINTS_TO_VOLUME 1101
#define GEO_NODE_CURVE_HANDLE_TYPE_SELECTION 1102
#define GEO_NODE_DELETE_GEOMETRY 1103
#define GEO_NODE_SEPARATE_GEOMETRY 1104
#define GEO_NODE_INPUT_RADIUS 1105
#define GEO_NODE_INPUT_CURVE_TILT 1106
#define GEO_NODE_INPUT_CURVE_HANDLES 1107
#define GEO_NODE_INPUT_FACE_SMOOTH 1108
#define GEO_NODE_INPUT_SPLINE_RESOLUTION 1109
#define GEO_NODE_INPUT_SPLINE_CYCLIC 1110
#define GEO_NODE_SET_CURVE_RADIUS 1111
#define GEO_NODE_SET_CURVE_TILT 1112
#define GEO_NODE_SET_CURVE_HANDLES 1113
#define GEO_NODE_SET_SHADE_SMOOTH 1114
#define GEO_NODE_SET_SPLINE_RESOLUTION 1115
#define GEO_NODE_SET_SPLINE_CYCLIC 1116
#define GEO_NODE_SET_POINT_RADIUS 1117
#define GEO_NODE_INPUT_MATERIAL_INDEX 1118
#define GEO_NODE_SET_MATERIAL_INDEX 1119
#define GEO_NODE_TRANSLATE_INSTANCES 1120
#define GEO_NODE_SCALE_INSTANCES 1121
#define GEO_NODE_ROTATE_INSTANCES 1122
#define GEO_NODE_SPLIT_EDGES 1123
#define GEO_NODE_MESH_TO_CURVE 1124
#define GEO_NODE_TRANSFER_ATTRIBUTE_DEPRECATED 1125
#define GEO_NODE_SUBDIVISION_SURFACE 1126
#define GEO_NODE_CURVE_ENDPOINT_SELECTION 1127
#define GEO_NODE_RAYCAST 1128
#define GEO_NODE_CURVE_TO_POINTS 1130
#define GEO_NODE_INSTANCES_TO_POINTS 1131
#define GEO_NODE_IMAGE_TEXTURE 1132
#define GEO_NODE_VOLUME_TO_MESH 1133
#define GEO_NODE_INPUT_ID 1134
#define GEO_NODE_SET_ID 1135
#define GEO_NODE_ATTRIBUTE_DOMAIN_SIZE 1136
#define GEO_NODE_DUAL_MESH 1137
#define GEO_NODE_INPUT_MESH_EDGE_VERTICES 1138
#define GEO_NODE_INPUT_MESH_FACE_AREA 1139
#define GEO_NODE_INPUT_MESH_FACE_NEIGHBORS 1140
#define GEO_NODE_INPUT_MESH_VERTEX_NEIGHBORS 1141
#define GEO_NODE_GEOMETRY_TO_INSTANCE 1142
#define GEO_NODE_INPUT_MESH_EDGE_NEIGHBORS 1143
#define GEO_NODE_INPUT_MESH_ISLAND 1144
#define GEO_NODE_INPUT_SCENE_TIME 1145
#define GEO_NODE_ACCUMULATE_FIELD 1146
#define GEO_NODE_INPUT_MESH_EDGE_ANGLE 1147
#define GEO_NODE_EVALUATE_AT_INDEX 1148
#define GEO_NODE_CURVE_PRIMITIVE_ARC 1149
#define GEO_NODE_FLIP_FACES 1150
#define GEO_NODE_SCALE_ELEMENTS 1151
#define GEO_NODE_EXTRUDE_MESH 1152
#define GEO_NODE_MERGE_BY_DISTANCE 1153
#define GEO_NODE_DUPLICATE_ELEMENTS 1154
#define GEO_NODE_INPUT_MESH_FACE_IS_PLANAR 1155
#define GEO_NODE_STORE_NAMED_ATTRIBUTE 1156
#define GEO_NODE_INPUT_NAMED_ATTRIBUTE 1157
#define GEO_NODE_REMOVE_ATTRIBUTE 1158
#define GEO_NODE_INPUT_INSTANCE_ROTATION 1159
#define GEO_NODE_INPUT_INSTANCE_SCALE 1160
#define GEO_NODE_VOLUME_CUBE 1161
#define GEO_NODE_POINTS 1162
#define GEO_NODE_EVALUATE_ON_DOMAIN 1163
#define GEO_NODE_MESH_TO_VOLUME 1164
#define GEO_NODE_UV_UNWRAP 1165
#define GEO_NODE_UV_PACK_ISLANDS 1166
#define GEO_NODE_DEFORM_CURVES_ON_SURFACE 1167
#define GEO_NODE_INPUT_SHORTEST_EDGE_PATHS 1168
#define GEO_NODE_EDGE_PATHS_TO_CURVES 1169
#define GEO_NODE_EDGE_PATHS_TO_SELECTION 1170
#define GEO_NODE_MESH_FACE_GROUP_BOUNDARIES 1171
#define GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME 1172
#define GEO_NODE_SELF_OBJECT 1173
#define GEO_NODE_SAMPLE_INDEX 1174
#define GEO_NODE_SAMPLE_NEAREST 1175
#define GEO_NODE_SAMPLE_NEAREST_SURFACE 1176
#define GEO_NODE_OFFSET_POINT_IN_CURVE 1177
#define GEO_NODE_CURVE_TOPOLOGY_CURVE_OF_POINT 1178
#define GEO_NODE_CURVE_TOPOLOGY_POINTS_OF_CURVE 1179
#define GEO_NODE_MESH_TOPOLOGY_OFFSET_CORNER_IN_FACE 1180
#define GEO_NODE_MESH_TOPOLOGY_CORNERS_OF_FACE 1181
#define GEO_NODE_MESH_TOPOLOGY_CORNERS_OF_VERTEX 1182
#define GEO_NODE_MESH_TOPOLOGY_EDGES_OF_CORNER 1183
#define GEO_NODE_MESH_TOPOLOGY_EDGES_OF_VERTEX 1184
#define GEO_NODE_MESH_TOPOLOGY_FACE_OF_CORNER 1185
#define GEO_NODE_MESH_TOPOLOGY_VERTEX_OF_CORNER 1186
#define GEO_NODE_SAMPLE_UV_SURFACE 1187
#define GEO_NODE_SET_CURVE_NORMAL 1188
#define GEO_NODE_IMAGE_INFO 1189
#define GEO_NODE_BLUR_ATTRIBUTE 1190
#define GEO_NODE_IMAGE 1191
#define GEO_NODE_INTERPOLATE_CURVES 1192
#define GEO_NODE_EDGES_TO_FACE_GROUPS 1193
// #define GEO_NODE_POINTS_TO_SDF_VOLUME 1194
// #define GEO_NODE_MESH_TO_SDF_VOLUME 1195
// #define GEO_NODE_SDF_VOLUME_SPHERE 1196
// #define GEO_NODE_MEAN_FILTER_SDF_VOLUME 1197
// #define GEO_NODE_OFFSET_SDF_VOLUME 1198
#define GEO_NODE_INDEX_OF_NEAREST 1199
/* Function nodes use the range starting at 1200. */
#define GEO_NODE_SIMULATION_INPUT 2100
#define GEO_NODE_SIMULATION_OUTPUT 2101
// #define GEO_NODE_INPUT_SIGNED_DISTANCE 2102
// #define GEO_NODE_SAMPLE_VOLUME 2103
#define GEO_NODE_MESH_TOPOLOGY_CORNERS_OF_EDGE 2104
/* Leaving out two indices to avoid crashes with files that were created during the development of
 * the repeat zone. */
#define GEO_NODE_REPEAT_INPUT 2107
#define GEO_NODE_REPEAT_OUTPUT 2108
#define GEO_NODE_TOOL_SELECTION 2109
#define GEO_NODE_TOOL_SET_SELECTION 2110
#define GEO_NODE_TOOL_3D_CURSOR 2111
#define GEO_NODE_TOOL_FACE_SET 2112
#define GEO_NODE_TOOL_SET_FACE_SET 2113
#define GEO_NODE_POINTS_TO_CURVES 2114
#define GEO_NODE_INPUT_EDGE_SMOOTH 2115
#define GEO_NODE_SPLIT_TO_INSTANCES 2116
#define GEO_NODE_INPUT_NAMED_LAYER_SELECTION 2117
#define GEO_NODE_INDEX_SWITCH 2118
#define GEO_NODE_INPUT_ACTIVE_CAMERA 2119
#define GEO_NODE_BAKE 2120
#define GEO_NODE_GET_NAMED_GRID 2121
#define GEO_NODE_STORE_NAMED_GRID 2122
#define GEO_NODE_SORT_ELEMENTS 2123
#define GEO_NODE_MENU_SWITCH 2124
#define GEO_NODE_SAMPLE_GRID 2125
#define GEO_NODE_MESH_TO_DENSITY_GRID 2126
#define GEO_NODE_MESH_TO_SDF_GRID 2127
#define GEO_NODE_POINTS_TO_SDF_GRID 2128
#define GEO_NODE_GRID_TO_MESH 2129
#define GEO_NODE_DISTRIBUTE_POINTS_IN_GRID 2130
#define GEO_NODE_SDF_GRID_BOOLEAN 2131
#define GEO_NODE_TOOL_VIEWPORT_TRANSFORM 2132
#define GEO_NODE_TOOL_MOUSE_POSITION 2133

/** \} */

/* -------------------------------------------------------------------- */
/** \name Function Nodes
 * \{ */

#define FN_NODE_BOOLEAN_MATH 1200
#define FN_NODE_COMPARE 1202
#define FN_NODE_LEGACY_RANDOM_FLOAT 1206
#define FN_NODE_INPUT_VECTOR 1207
#define FN_NODE_INPUT_STRING 1208
#define FN_NODE_FLOAT_TO_INT 1209
#define FN_NODE_VALUE_TO_STRING 1210
#define FN_NODE_STRING_LENGTH 1211
#define FN_NODE_SLICE_STRING 1212
#define FN_NODE_INPUT_SPECIAL_CHARACTERS 1213
#define FN_NODE_RANDOM_VALUE 1214
#define FN_NODE_ROTATE_EULER 1215
#define FN_NODE_ALIGN_EULER_TO_VECTOR 1216
#define FN_NODE_INPUT_COLOR 1217
#define FN_NODE_REPLACE_STRING 1218
#define FN_NODE_INPUT_BOOL 1219
#define FN_NODE_INPUT_INT 1220
#define FN_NODE_SEPARATE_COLOR 1221
#define FN_NODE_COMBINE_COLOR 1222
#define FN_NODE_AXIS_ANGLE_TO_ROTATION 1223
#define FN_NODE_EULER_TO_ROTATION 1224
#define FN_NODE_QUATERNION_TO_ROTATION 1225
#define FN_NODE_ROTATION_TO_AXIS_ANGLE 1226
#define FN_NODE_ROTATION_TO_EULER 1227
#define FN_NODE_ROTATION_TO_QUATERNION 1228
#define FN_NODE_ROTATE_VECTOR 1229
#define FN_NODE_ROTATE_ROTATION 1230
#define FN_NODE_INVERT_ROTATION 1231
#define FN_NODE_TRANSFORM_POINT 1232
#define FN_NODE_TRANSFORM_DIRECTION 1233
#define FN_NODE_MATRIX_MULTIPLY 1234
#define FN_NODE_COMBINE_TRANSFORM 1235
#define FN_NODE_SEPARATE_TRANSFORM 1236
#define FN_NODE_INVERT_MATRIX 1237
#define FN_NODE_TRANSPOSE_MATRIX 1238
#define FN_NODE_PROJECT_POINT 1239
#define FN_NODE_ALIGN_ROTATION_TO_VECTOR 1240
#define FN_NODE_COMBINE_MATRIX 1241
#define FN_NODE_SEPARATE_MATRIX 1242
#define FN_NODE_INPUT_ROTATION 1243

/** \} */

void BKE_node_system_init();
void BKE_node_system_exit();

namespace blender::bke {

bNodeTree *ntreeAddTreeEmbedded(Main *bmain, ID *owner_id, const char *name, const char *idname);

/* Copy/free functions, need to manage ID users. */

/**
 * Free (or release) any data used by this node-tree.
 * Does not free the node-tree itself and does no ID user counting.
 */
void ntreeFreeTree(bNodeTree *ntree);

bNodeTree *ntreeCopyTree_ex(const bNodeTree *ntree, Main *bmain, bool do_id_user);
bNodeTree *ntreeCopyTree(Main *bmain, const bNodeTree *ntree);

void ntreeFreeLocalNode(bNodeTree *ntree, bNode *node);

void ntreeUpdateAllNew(Main *main);

/** Update asset meta-data cache of data-block properties. */
void node_update_asset_metadata(bNodeTree &node_tree);

void ntreeNodeFlagSet(const bNodeTree *ntree, int flag, bool enable);

/**
 * Merge local tree results back, and free local tree.
 *
 * We have to assume the editor already changed completely.
 */
void ntreeLocalMerge(Main *bmain, bNodeTree *localtree, bNodeTree *ntree);

/**
 * \note `ntree` itself has been read!
 */
void ntreeBlendReadData(BlendDataReader *reader, ID *owner_id, bNodeTree *ntree);

bool node_type_is_undefined(const bNode *node);

bool nodeIsStaticSocketType(const bNodeSocketType *stype);

const char *nodeSocketSubTypeLabel(int subtype);

void nodeRemoveSocketEx(bNodeTree *ntree, bNode *node, bNodeSocket *sock, bool do_id_user);

void nodeModifySocketType(bNodeTree *ntree, bNode *node, bNodeSocket *sock, const char *idname);

/**
 * \note Goes over entire tree.
 */
void nodeUnlinkNode(bNodeTree *ntree, bNode *node);

/**
 * Rebuild the `node_by_id` runtime vector set. Call after removing a node if not handled
 * separately. This is important instead of just using `nodes_by_id.remove()` since it maintains
 * the node order.
 */
void nodeRebuildIDVector(bNodeTree *node_tree);

/**
 * \note keeps socket list order identical, for copying links.
 * \param use_unique: If true, make sure the node's identifier and name are unique in the new
 * tree. Must be *true* if the \a dst_tree had nodes that weren't in the source node's tree.
 * Must be *false* when simply copying a node tree, so that identifiers don't change.
 */
bNode *node_copy_with_mapping(bNodeTree *dst_tree,
                              const bNode &node_src,
                              int flag,
                              bool use_unique,
                              Map<const bNodeSocket *, bNodeSocket *> &new_socket_map);

bNode *node_copy(bNodeTree *dst_tree, const bNode &src_node, int flag, bool use_unique);

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
void node_free_node(bNodeTree *tree, bNode *node);

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
void nodeLinkSetMute(bNodeTree *ntree, bNodeLink *link, const bool muted);

bool nodeLinkIsSelected(const bNodeLink *link);

void nodeInternalRelink(bNodeTree *ntree, bNode *node);

float2 nodeToView(const bNode *node, float2 loc);

float2 nodeFromView(const bNode *node, float2 view_loc);

void nodePositionRelative(bNode *from_node,
                          const bNode *to_node,
                          const bNodeSocket *from_sock,
                          const bNodeSocket *to_sock);

void nodePositionPropagate(bNode *node);

/**
 * \note Recursive.
 */
bNode *nodeFindRootParent(bNode *node);

/**
 * Iterate over a chain of nodes, starting with \a node_start, executing
 * \a callback for each node (which can return false to end iterator).
 *
 * \param reversed: for backwards iteration
 * \note Recursive
 */
void nodeChainIter(const bNodeTree *ntree,
                   const bNode *node_start,
                   bool (*callback)(bNode *, bNode *, void *, const bool),
                   void *userdata,
                   bool reversed);

/**
 * Iterate over a chain of nodes, starting with \a node_start, executing
 * \a callback for each node (which can return false to end iterator).
 *
 * Faster than nodeChainIter. Iter only once per node.
 * Can be called recursively (using another nodeChainIterBackwards) by
 * setting the recursion_lvl accordingly.
 *
 * WARN: No node is guaranteed to be iterated as a to_node,
 * since it could have been iterated earlier as a from_node.
 *
 * \note Needs updated socket links (ntreeUpdateTree).
 * \note Recursive
 */
void nodeChainIterBackwards(const bNodeTree *ntree,
                            const bNode *node_start,
                            bool (*callback)(bNode *, bNode *, void *),
                            void *userdata,
                            int recursion_lvl);

/**
 * Iterate over all parents of \a node, executing \a callback for each parent
 * (which can return false to end iterator)
 *
 * \note Recursive
 */
void nodeParentsIter(bNode *node, bool (*callback)(bNode *, void *), void *userdata);

/**
 * A dangling reroute node is a reroute node that does *not* have a "data source", i.e. no
 * non-reroute node is connected to its input.
 */
bool nodeIsDanglingReroute(const bNodeTree *ntree, const bNode *node);

bNode *nodeGetActivePaintCanvas(bNodeTree *ntree);

/**
 * \brief Does the given node supports the sub active flag.
 *
 * \param sub_active: The active flag to check. #NODE_ACTIVE_TEXTURE / #NODE_ACTIVE_PAINT_CANVAS.
 */
bool nodeSupportsActiveFlag(const bNode *node, int sub_active);

void nodeSetSocketAvailability(bNodeTree *ntree, bNodeSocket *sock, bool is_available);

/**
 * If the node implements a `declare` function, this function makes sure that `node->declaration`
 * is up to date. It is expected that the sockets of the node are up to date already.
 */
bool nodeDeclarationEnsure(bNodeTree *ntree, bNode *node);

/**
 * Just update `node->declaration` if necessary. This can also be called on nodes that may not be
 * up to date (e.g. because the need versioning or are dynamic).
 */
bool nodeDeclarationEnsureOnOutdatedNode(bNodeTree *ntree, bNode *node);

/**
 * Update `socket->declaration` for all sockets in the node. This assumes that the node declaration
 * and sockets are up to date already.
 */
void nodeSocketDeclarationsUpdate(bNode *node);

using bNodeInstanceHashIterator = GHashIterator;

BLI_INLINE bNodeInstanceHashIterator *node_instance_hash_iterator_new(bNodeInstanceHash *hash)
{
  return BLI_ghashIterator_new(hash->ghash);
}

BLI_INLINE void node_instance_hash_iterator_init(bNodeInstanceHashIterator *iter,
                                                 bNodeInstanceHash *hash)
{
  BLI_ghashIterator_init(iter, hash->ghash);
}

BLI_INLINE void node_instance_hash_iterator_free(bNodeInstanceHashIterator *iter)
{
  BLI_ghashIterator_free(iter);
}

BLI_INLINE bNodeInstanceKey node_instance_hash_iterator_get_key(bNodeInstanceHashIterator *iter)
{
  return *(bNodeInstanceKey *)BLI_ghashIterator_getKey(iter);
}

BLI_INLINE void *node_instance_hash_iterator_get_value(bNodeInstanceHashIterator *iter)
{
  return BLI_ghashIterator_getValue(iter);
}

BLI_INLINE void node_instance_hash_iterator_step(bNodeInstanceHashIterator *iter)
{
  BLI_ghashIterator_step(iter);
}

BLI_INLINE bool node_instance_hash_iterator_done(bNodeInstanceHashIterator *iter)
{
  return BLI_ghashIterator_done(iter);
}

#define NODE_INSTANCE_HASH_ITER(iter_, hash_) \
  for (blender::bke::node_instance_hash_iterator_init(&iter_, hash_); \
       blender::bke::node_instance_hash_iterator_done(&iter_) == false; \
       blender::bke::node_instance_hash_iterator_step(&iter_))

/* Node Previews */
bool node_preview_used(const bNode *node);

bNodePreview *node_preview_verify(
    bNodeInstanceHash *previews, bNodeInstanceKey key, int xsize, int ysize, bool create);

bNodePreview *node_preview_copy(bNodePreview *preview);

void node_preview_free(bNodePreview *preview);

void node_preview_init_tree(bNodeTree *ntree, int xsize, int ysize);

void node_preview_remove_unused(bNodeTree *ntree);

void node_preview_clear(bNodePreview *preview);

void node_preview_merge_tree(bNodeTree *to_ntree, bNodeTree *from_ntree, bool remove_old);

/* -------------------------------------------------------------------- */
/** \name Node Type Access
 * \{ */

void nodeLabel(const bNodeTree *ntree, const bNode *node, char *label, int maxlen);

/**
 * Get node socket label if it is set.
 */
const char *nodeSocketLabel(const bNodeSocket *sock);

/**
 * Get node socket short label if it is set.
 * It is used when grouping sockets under panels, to avoid redundancy in the label.
 */
const char *nodeSocketShortLabel(const bNodeSocket *sock);

/**
 * Initialize a new node type struct with default values and callbacks.
 */
void node_type_base(bNodeType *ntype, int type, const char *name, short nclass);

void node_type_socket_templates(bNodeType *ntype,
                                bNodeSocketTemplate *inputs,
                                bNodeSocketTemplate *outputs);

void node_type_size(bNodeType *ntype, int width, int minwidth, int maxwidth);

enum class eNodeSizePreset : int8_t {
  Default,
  Small,
  Middle,
  Large,
};

void node_type_size_preset(bNodeType *ntype, eNodeSizePreset size);

/* -------------------------------------------------------------------- */
/** \name Node Generic Functions
 * \{ */

bool node_is_connected_to_output(const bNodeTree *ntree, const bNode *node);

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
