/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"

#include "DNA_listBase.h"

/* for FOREACH_NODETREE_BEGIN */
#include "DNA_node_types.h"

#include "RNA_types.h"

#ifdef __cplusplus
#  include "BLI_map.hh"
#  include "BLI_string_ref.hh"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* not very important, but the stack solver likes to know a maximum */
#define MAX_SOCKET 512

struct BlendDataReader;
struct BlendExpander;
struct BlendLibReader;
struct BlendWriter;
struct FreestyleLineStyle;
struct GPUMaterial;
struct GPUNodeStack;
struct ID;
struct ImBuf;
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
typedef struct bNodeSocketTemplate {
  int type;
  char name[64];                /* MAX_NAME */
  float val1, val2, val3, val4; /* default alloc value for inputs */
  float min, max;
  int subtype; /* would use PropertySubType but this is a bad level include to use RNA */
  int flag;

  /* after this line is used internal only */
  struct bNodeSocket *sock; /* used to hold verified socket */
  char identifier[64];      /* generated from name */
} bNodeSocketTemplate;

/* Use `void *` for callbacks that require C++. This is rather ugly, but works well for now. This
 * would not be necessary if we would use bNodeSocketType and bNodeType only in C++ code.
 * However, achieving this requires quite a few changes currently. */
#ifdef __cplusplus
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
}  // namespace nodes
namespace realtime_compositor {
class Context;
class NodeOperation;
class ShaderNode;
}  // namespace realtime_compositor
}  // namespace blender

using CPPTypeHandle = blender::CPPType;
using NodeMultiFunctionBuildFunction = void (*)(blender::nodes::NodeMultiFunctionBuilder &builder);
using NodeGeometryExecFunction = void (*)(blender::nodes::GeoNodeExecParams params);
using NodeDeclareFunction = void (*)(blender::nodes::NodeDeclarationBuilder &builder);
using NodeDeclareDynamicFunction = void (*)(const bNodeTree &tree,
                                            const bNode &node,
                                            blender::nodes::NodeDeclaration &r_declaration);
using SocketGetCPPValueFunction = void (*)(const struct bNodeSocket &socket, void *r_value);
using SocketGetGeometryNodesCPPValueFunction = void (*)(const struct bNodeSocket &socket,
                                                        void *r_value);

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

#else
typedef void *NodeGetCompositorOperationFunction;
typedef void *NodeGetCompositorShaderNodeFunction;
typedef void *NodeMultiFunctionBuildFunction;
typedef void *NodeGeometryExecFunction;
typedef void *NodeDeclareFunction;
typedef void *NodeDeclareDynamicFunction;
typedef void *NodeGatherSocketLinkOperationsFunction;
typedef void *NodeGatherAddOperationsFunction;
typedef void *SocketGetCPPTypeFunction;
typedef void *SocketGetGeometryNodesCPPTypeFunction;
typedef void *SocketGetGeometryNodesCPPValueFunction;
typedef void *SocketGetCPPValueFunction;
typedef struct CPPTypeHandle CPPTypeHandle;
#endif

/**
 * \brief Defines a socket type.
 *
 * Defines the appearance and behavior of a socket in the UI.
 */
typedef struct bNodeSocketType {
  /** Identifier name. */
  char idname[64];
  /** Type label. */
  char label[64];
  /** Sub-type label. */
  char subtype_label[64];

  void (*draw)(struct bContext *C,
               struct uiLayout *layout,
               struct PointerRNA *ptr,
               struct PointerRNA *node_ptr,
               const char *text);
  void (*draw_color)(struct bContext *C,
                     struct PointerRNA *ptr,
                     struct PointerRNA *node_ptr,
                     float *r_color);

  void (*interface_draw)(struct bContext *C, struct uiLayout *layout, struct PointerRNA *ptr);
  void (*interface_draw_color)(struct bContext *C, struct PointerRNA *ptr, float *r_color);
  void (*interface_init_socket)(struct bNodeTree *ntree,
                                const struct bNodeSocket *interface_socket,
                                struct bNode *node,
                                struct bNodeSocket *sock,
                                const char *data_path);
  void (*interface_from_socket)(struct bNodeTree *ntree,
                                struct bNodeSocket *interface_socket,
                                const struct bNode *node,
                                const struct bNodeSocket *sock);

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
  void (*free_self)(struct bNodeSocketType *stype);

  /* Return the CPPType of this socket. */
  const CPPTypeHandle *base_cpp_type;
  /* Get the value of this socket in a generic way. */
  SocketGetCPPValueFunction get_base_cpp_value;
  /* Get geometry nodes cpp type. */
  const CPPTypeHandle *geometry_nodes_cpp_type;
  /* Get geometry nodes cpp value. */
  SocketGetGeometryNodesCPPValueFunction get_geometry_nodes_cpp_value;
} bNodeSocketType;

typedef void *(*NodeInitExecFunction)(struct bNodeExecContext *context,
                                      struct bNode *node,
                                      bNodeInstanceKey key);
typedef void (*NodeFreeExecFunction)(void *nodedata);
typedef void (*NodeExecFunction)(void *data,
                                 int thread,
                                 struct bNode *,
                                 struct bNodeExecData *execdata,
                                 struct bNodeStack **in,
                                 struct bNodeStack **out);
typedef int (*NodeGPUExecFunction)(struct GPUMaterial *mat,
                                   struct bNode *node,
                                   struct bNodeExecData *execdata,
                                   struct GPUNodeStack *in,
                                   struct GPUNodeStack *out);

/**
 * \brief Defines a node type.
 *
 * Initial attributes and constants for a node as well as callback functions
 * implementing the node behavior.
 */
typedef struct bNodeType {
  char idname[64]; /* identifier name */
  int type;

  char ui_name[64]; /* MAX_NAME */
  char ui_description[256];
  int ui_icon;

  float width, minwidth, maxwidth;
  float height, minheight, maxheight;
  short nclass, flag;

  /* templates for static sockets */
  bNodeSocketTemplate *inputs, *outputs;

  char storagename[64]; /* struct name for DNA */

  /* Draw the option buttons on the node */
  void (*draw_buttons)(struct uiLayout *, struct bContext *C, struct PointerRNA *ptr);
  /* Additional parameters in the side panel */
  void (*draw_buttons_ex)(struct uiLayout *, struct bContext *C, struct PointerRNA *ptr);

  /* Additional drawing on backdrop */
  void (*draw_backdrop)(
      struct SpaceNode *snode, struct ImBuf *backdrop, struct bNode *node, int x, int y);

  /**
   * Optional custom label function for the node header.
   * \note Used as a fallback when #bNode.label isn't set.
   */
  void (*labelfunc)(const struct bNodeTree *ntree,
                    const struct bNode *node,
                    char *label,
                    int label_maxncpy);

  /** Optional override for node class, used for drawing node header. */
  int (*ui_class)(const struct bNode *node);

  /** Called when the node is updated in the editor. */
  void (*updatefunc)(struct bNodeTree *ntree, struct bNode *node);
  /** Check and update if internal ID data has changed. */
  void (*group_update_func)(struct bNodeTree *ntree, struct bNode *node);

  /**
   * Initialize a new node instance of this type after creation.
   *
   * \note Assignments to `node->id` must not increment the user of the ID.
   * This is handled by the caller of this callback.
   */
  void (*initfunc)(struct bNodeTree *ntree, struct bNode *node);
  /**
   * Free the node instance.
   *
   * \note Access to `node->id` must be avoided in this function as this is called
   * while freeing #Main, the state of this ID is undefined.
   * Higher level logic to remove the node handles the user-count.
   */
  void (*freefunc)(struct bNode *node);
  /** Make a copy of the node instance. */
  void (*copyfunc)(struct bNodeTree *dest_ntree,
                   struct bNode *dest_node,
                   const struct bNode *src_node);

  /* Registerable API callback versions, called in addition to C callbacks */
  void (*initfunc_api)(const struct bContext *C, struct PointerRNA *ptr);
  void (*freefunc_api)(struct PointerRNA *ptr);
  void (*copyfunc_api)(struct PointerRNA *ptr, const struct bNode *src_node);

  /**
   * An additional poll test for deciding whether nodes should be an option in search menus.
   * Potentially more strict poll than #poll(), but doesn't have to check the same things.
   */
  bool (*add_ui_poll)(const struct bContext *C);

  /**
   * Can this node type be added to a node tree?
   * \param r_disabled_hint: Hint to display in the UI when the poll fails.
   *                         The callback can set this to a static string without having to
   *                         null-check it (or without setting it to null if it's not used).
   *                         The caller must pass a valid `const char **` and null-initialize it
   *                         when it's not just a dummy, that is, if it actually wants to access
   *                         the returned disabled-hint (null-check needed!).
   */
  bool (*poll)(const struct bNodeType *ntype,
               const struct bNodeTree *nodetree,
               const char **r_disabled_hint);
  /**
   * Can this node be added to a node tree?
   * \param r_disabled_hint: See `poll()`.
   */
  bool (*poll_instance)(const struct bNode *node,
                        const struct bNodeTree *nodetree,
                        const char **r_disabled_hint);

  /* Optional handling of link insertion. Returns false if the link shouldn't be created. */
  bool (*insert_link)(struct bNodeTree *ntree, struct bNode *node, struct bNodeLink *link);

  void (*free_self)(struct bNodeType *ntype);

  /* **** execution callbacks **** */
  NodeInitExecFunction init_exec_fn;
  NodeFreeExecFunction free_exec_fn;
  NodeExecFunction exec_fn;
  /* gpu */
  NodeGPUExecFunction gpu_fn;

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

  /* Declares which sockets the node has. */
  NodeDeclareFunction declare;
  /**
   * Declare which sockets the node has for declarations that aren't static per node type.
   * In other words, defining this callback means that different nodes of this type can have
   * different declarations and different sockets.
   */
  NodeDeclareDynamicFunction declare_dynamic;

  /* Declaration to be used when it is not dynamic. */
  NodeDeclarationHandle *fixed_declaration;

  /**
   * Add to the list of search names and operations gathered by node link drag searching.
   * Usually it isn't necessary to override the default behavior here, but a node type can have
   * custom behavior here like adding custom search items.
   */
  NodeGatherSocketLinkOperationsFunction gather_link_search_ops;

  /**
   * Add to the list of search items gathered by the add-node search. The default behavior of
   * adding a single item with the node name is usually enough, but node types can have any number
   * of custom search items.
   */
  NodeGatherAddOperationsFunction gather_add_node_search_ops;

  /** True when the node cannot be muted. */
  bool no_muting;

  /* RNA integration */
  ExtensionRNA rna_ext;
} bNodeType;

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

typedef void (*bNodeClassCallback)(void *calldata, int nclass, const char *name);
typedef struct bNodeTreeType {
  int type;        /* type identifier */
  char idname[64]; /* identifier name */

  /* The ID name of group nodes for this type. */
  char group_idname[64];

  char ui_name[64];
  char ui_description[256];
  int ui_icon;

  /* callbacks */
  /* Iteration over all node classes. */
  void (*foreach_nodeclass)(struct Scene *scene, void *calldata, bNodeClassCallback func);
  /* Check visibility in the node editor */
  bool (*poll)(const struct bContext *C, struct bNodeTreeType *ntreetype);
  /* Select a node tree from the context */
  void (*get_from_context)(const struct bContext *C,
                           struct bNodeTreeType *ntreetype,
                           struct bNodeTree **r_ntree,
                           struct ID **r_id,
                           struct ID **r_from);

  /* calls allowing threaded composite */
  void (*localize)(struct bNodeTree *localtree, struct bNodeTree *ntree);
  void (*local_merge)(struct Main *bmain, struct bNodeTree *localtree, struct bNodeTree *ntree);

  /* Tree update. Overrides `nodetype->updatetreefunc` ! */
  void (*update)(struct bNodeTree *ntree);

  bool (*validate_link)(eNodeSocketDatatype from, eNodeSocketDatatype to);

  void (*node_add_init)(struct bNodeTree *ntree, struct bNode *bnode);

  /* Check if the socket type is valid for this tree type. */
  bool (*valid_socket_type)(struct bNodeTreeType *ntreetype, struct bNodeSocketType *socket_type);

  /* RNA integration */
  ExtensionRNA rna_ext;
} bNodeTreeType;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic API, Trees
 * \{ */

struct bNodeTreeType *ntreeTypeFind(const char *idname);
void ntreeTypeAdd(struct bNodeTreeType *nt);
void ntreeTypeFreeLink(const struct bNodeTreeType *nt);
bool ntreeIsRegistered(const struct bNodeTree *ntree);
struct GHashIterator *ntreeTypeGetIterator(void);

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
void ntreeSetTypes(const struct bContext *C, struct bNodeTree *ntree);

struct bNodeTree *ntreeAddTree(struct Main *bmain, const char *name, const char *idname);

/**
 * Free tree which is embedded into another data-block.
 */
void ntreeFreeEmbeddedTree(struct bNodeTree *ntree);

/**
 * Get address of potential node-tree pointer of given ID.
 *
 * \warning Using this function directly is potentially dangerous, if you don't know or are not
 * sure, please use `ntreeFromID()` instead.
 */
struct bNodeTree **BKE_ntree_ptr_from_id(struct ID *id);

/**
 * Returns the private NodeTree object of the data-block, if it has one.
 */
struct bNodeTree *ntreeFromID(struct ID *id);

void ntreeFreeLocalTree(struct bNodeTree *ntree);

/**
 * Check recursively if a node tree contains another.
 */
bool ntreeContainsTree(const struct bNodeTree *tree_to_search_in,
                       const struct bNodeTree *tree_to_search_for);

void ntreeUpdateAllUsers(struct Main *main, struct ID *id);

/**
 * XXX: old trees handle output flags automatically based on special output
 * node types and last active selection.
 * New tree types have a per-output socket flag to indicate the final output to use explicitly.
 */
void ntreeSetOutput(struct bNodeTree *ntree);

/**
 * Returns localized tree for execution in threads.
 */
struct bNodeTree *ntreeLocalize(struct bNodeTree *ntree);

/**
 * This is only direct data, tree itself should have been written.
 */
void ntreeBlendWrite(struct BlendWriter *writer, struct bNodeTree *ntree);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Tree Interface
 * \{ */

/** Run this after relevant changes to panels to ensure sockets remain sorted by panel. */
void ntreeEnsureSocketInterfacePanelOrder(bNodeTree *ntree);

void ntreeRemoveSocketInterface(bNodeTree *ntree, bNodeSocket *sock);

struct bNodeSocket *ntreeAddSocketInterface(struct bNodeTree *ntree,
                                            eNodeSocketInOut in_out,
                                            const char *idname,
                                            const char *name);

/** Set the panel of the interface socket. */
void ntreeSetSocketInterfacePanel(bNodeTree *ntree, bNodeSocket *sock, bNodePanel *panel);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Tree Socket Panels
 * \{ */

/**
 * Check if a panel is part of the node tree.
 * \return True if the panel is part of the node tree.
 */
bool ntreeContainsPanel(const bNodeTree *ntree, const bNodePanel *panel);

/**
 * Index of a panel in the node tree.
 * \return Index of the panel in the node tree or -1 if the tree does not contain the panel.
 */
int ntreeGetPanelIndex(const bNodeTree *ntree, const bNodePanel *panel);

/**
 * Add a new panel to the node tree.
 * \param name: Name of the new panel.
 */
bNodePanel *ntreeAddPanel(bNodeTree *ntree, const char *name);

/**
 * Insert a new panel in the node tree.
 * \param name: Name of the new panel.
 * \param index: Index at which to insert the panel.
 */
bNodePanel *ntreeInsertPanel(bNodeTree *ntree, const char *name, int index);

/** Remove a panel from the node tree. */
void ntreeRemovePanel(bNodeTree *ntree, bNodePanel *panel);

/** Remove all panels from the node tree. */
void ntreeClearPanels(bNodeTree *ntree);

/**
 * Move a panel up or down in the node tree.
 * \param index: Index to which to move the panel.
 */
void ntreeMovePanel(bNodeTree *ntree, bNodePanel *panel, int new_index);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic API, Nodes
 * \{ */

struct bNodeType *nodeTypeFind(const char *idname);
const char *nodeTypeFindAlias(const char *idname);
void nodeRegisterType(struct bNodeType *ntype);
void nodeUnregisterType(struct bNodeType *ntype);
void nodeRegisterAlias(struct bNodeType *nt, const char *alias);
struct GHashIterator *nodeTypeGetIterator(void);

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

struct bNodeSocketType *nodeSocketTypeFind(const char *idname);
void nodeRegisterSocketType(struct bNodeSocketType *stype);
void nodeUnregisterSocketType(struct bNodeSocketType *stype);
bool nodeSocketIsRegistered(const struct bNodeSocket *sock);
struct GHashIterator *nodeSocketTypeGetIterator(void);
const char *nodeSocketTypeLabel(const bNodeSocketType *stype);

const char *nodeStaticSocketType(int type, int subtype);
const char *nodeStaticSocketInterfaceType(int type, int subtype);
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

struct bNodeSocket *nodeFindSocket(const struct bNode *node,
                                   eNodeSocketInOut in_out,
                                   const char *identifier);
struct bNodeSocket *nodeAddSocket(struct bNodeTree *ntree,
                                  struct bNode *node,
                                  eNodeSocketInOut in_out,
                                  const char *idname,
                                  const char *identifier,
                                  const char *name);
struct bNodeSocket *nodeAddStaticSocket(struct bNodeTree *ntree,
                                        struct bNode *node,
                                        eNodeSocketInOut in_out,
                                        int type,
                                        int subtype,
                                        const char *identifier,
                                        const char *name);
void nodeRemoveSocket(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock);

void nodeModifySocketTypeStatic(
    struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock, int type, int subtype);

struct bNode *nodeAddNode(const struct bContext *C, struct bNodeTree *ntree, const char *idname);
struct bNode *nodeAddStaticNode(const struct bContext *C, struct bNodeTree *ntree, int type);

/**
 * Find the first available, non-duplicate name for a given node.
 */
void nodeUniqueName(struct bNodeTree *ntree, struct bNode *node);
/**
 * Create a new unique integer identifier for the node. Also set the node's
 * index in the tree, which is an eagerly maintained cache.
 */
void nodeUniqueID(struct bNodeTree *ntree, struct bNode *node);

/**
 * Delete node, associated animation data and ID user count.
 */
void nodeRemoveNode(struct Main *bmain,
                    struct bNodeTree *ntree,
                    struct bNode *node,
                    bool do_id_user);

void nodeDimensionsGet(const struct bNode *node, float *r_width, float *r_height);
void nodeTagUpdateID(struct bNode *node);
void nodeInternalLinks(struct bNode *node, struct bNodeLink **r_links, int *r_len);

/**
 * Also used via RNA API, so we check for proper input output direction.
 */
struct bNodeLink *nodeAddLink(struct bNodeTree *ntree,
                              struct bNode *fromnode,
                              struct bNodeSocket *fromsock,
                              struct bNode *tonode,
                              struct bNodeSocket *tosock);
void nodeRemLink(struct bNodeTree *ntree, struct bNodeLink *link);
void nodeRemSocketLinks(struct bNodeTree *ntree, struct bNodeSocket *sock);

bool nodeLinkIsHidden(const struct bNodeLink *link);

void nodeAttachNode(struct bNodeTree *ntree, struct bNode *node, struct bNode *parent);
void nodeDetachNode(struct bNodeTree *ntree, struct bNode *node);

/**
 * Same as above but expects that the socket definitely is in the node tree.
 */
void nodeFindNode(struct bNodeTree *ntree,
                  struct bNodeSocket *sock,
                  struct bNode **r_node,
                  int *r_sockindex);
/**
 * Finds a node based on its name.
 */
struct bNode *nodeFindNodebyName(struct bNodeTree *ntree, const char *name);
/**
 * Finds a node based on given socket and returns true on success.
 */
bool nodeFindNodeTry(struct bNodeTree *ntree,
                     struct bNodeSocket *sock,
                     struct bNode **r_node,
                     int *r_sockindex);

bool nodeIsParentAndChild(const bNode *parent, const bNode *child);

int nodeCountSocketLinks(const struct bNodeTree *ntree, const struct bNodeSocket *sock);

void nodeSetSelected(struct bNode *node, bool select);
/**
 * Two active flags, ID nodes have special flag for buttons display.
 */
void nodeSetActive(struct bNodeTree *ntree, struct bNode *node);
struct bNode *nodeGetActive(struct bNodeTree *ntree);
void nodeClearActive(struct bNodeTree *ntree);
/**
 * Two active flags, ID nodes have special flag for buttons display.
 */
struct bNode *nodeGetActiveTexture(struct bNodeTree *ntree);

int nodeSocketLinkLimit(const struct bNodeSocket *sock);

/**
 * Node Instance Hash.
 */
typedef struct bNodeInstanceHash {
  /** XXX should be made a direct member, #GHash allocation needs to support it */
  GHash *ghash;
} bNodeInstanceHash;

typedef void (*bNodeInstanceValueFP)(void *value);

/**
 * Magic number for initial hash key.
 */
extern const bNodeInstanceKey NODE_INSTANCE_KEY_BASE;
extern const bNodeInstanceKey NODE_INSTANCE_KEY_NONE;

bNodeInstanceKey BKE_node_instance_key(bNodeInstanceKey parent_key,
                                       const struct bNodeTree *ntree,
                                       const struct bNode *node);

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

void BKE_node_preview_clear_tree(struct bNodeTree *ntree);
/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Type Access
 * \{ */

bool nodeGroupPoll(const struct bNodeTree *nodetree,
                   const struct bNodeTree *grouptree,
                   const char **r_disabled_hint);

void node_type_base_custom(struct bNodeType *ntype,
                           const char *idname,
                           const char *name,
                           short nclass);

/**
 * \warning Nodes defining a storage type _must_ allocate this for new nodes.
 * Otherwise nodes will reload as undefined (#46619).
 */
void node_type_storage(struct bNodeType *ntype,
                       const char *storagename,
                       void (*freefunc)(struct bNode *node),
                       void (*copyfunc)(struct bNodeTree *dest_ntree,
                                        struct bNode *dest_node,
                                        const struct bNode *src_node));

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
  struct Material *mat;
  Tex *tex;
  struct Light *light;
  struct World *world;
  struct FreestyleLineStyle *linestyle;
  struct Simulation *simulation;
};

void BKE_node_tree_iter_init(struct NodeTreeIterStore *ntreeiter, struct Main *bmain);
bool BKE_node_tree_iter_step(struct NodeTreeIterStore *ntreeiter,
                             struct bNodeTree **r_nodetree,
                             struct ID **r_id);

#define FOREACH_NODETREE_BEGIN(bmain, _nodetree, _id) \
  { \
    struct NodeTreeIterStore _nstore; \
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

void BKE_nodetree_remove_layer_n(struct bNodeTree *ntree, struct Scene *scene, int layer_index);

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
#define SH_NODE_BSDF_VELVET 139
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
#define SH_NODE_TEX_MUSGRAVE 152
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
#define CMP_NODE_SPLITVIEWER 240
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

/* Cryptomatte source. */
#define CMP_CRYPTOMATTE_SRC_RENDER 0
#define CMP_CRYPTOMATTE_SRC_IMAGE 1

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
#define GEO_NODE_INPUT_SHADE_SMOOTH 1108
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
#define GEO_NODE_POINTS_TO_SDF_VOLUME 1194
#define GEO_NODE_MESH_TO_SDF_VOLUME 1195
#define GEO_NODE_SDF_VOLUME_SPHERE 1196
#define GEO_NODE_MEAN_FILTER_SDF_VOLUME 1197
#define GEO_NODE_OFFSET_SDF_VOLUME 1198
#define GEO_NODE_INDEX_OF_NEAREST 1199
/* Function nodes use the range starting at 1200. */
#define GEO_NODE_SIMULATION_INPUT 2100
#define GEO_NODE_SIMULATION_OUTPUT 2101
#define GEO_NODE_INPUT_SIGNED_DISTANCE 2102
#define GEO_NODE_SAMPLE_VOLUME 2103
#define GEO_NODE_MESH_TOPOLOGY_CORNERS_OF_EDGE 2104

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

/** \} */

void BKE_node_system_init(void);
void BKE_node_system_exit(void);

#ifdef __cplusplus
}
#endif
