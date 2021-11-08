/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

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
extern "C" {
#endif

/* not very important, but the stack solver likes to know a maximum */
#define MAX_SOCKET 512

struct ARegion;
struct BlendDataReader;
struct BlendExpander;
struct BlendLibReader;
struct BlendWriter;
struct ColorManagedDisplaySettings;
struct ColorManagedViewSettings;
struct CryptomatteSession;
struct FreestyleLineStyle;
struct GPUMaterial;
struct GPUNodeStack;
struct ID;
struct ImBuf;
struct ImageFormatData;
struct Light;
struct ListBase;
struct MTex;
struct Main;
struct Material;
struct PointerRNA;
struct RenderData;
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
namespace nodes {
class NodeMultiFunctionBuilder;
class GeoNodeExecParams;
class NodeDeclarationBuilder;
}  // namespace nodes
namespace fn {
class CPPType;
class MFDataType;
}  // namespace fn
}  // namespace blender

using NodeMultiFunctionBuildFunction = void (*)(blender::nodes::NodeMultiFunctionBuilder &builder);
using NodeGeometryExecFunction = void (*)(blender::nodes::GeoNodeExecParams params);
using NodeDeclareFunction = void (*)(blender::nodes::NodeDeclarationBuilder &builder);
using SocketGetCPPTypeFunction = const blender::fn::CPPType *(*)();
using SocketGetCPPValueFunction = void (*)(const struct bNodeSocket &socket, void *r_value);
using SocketGetGeometryNodesCPPTypeFunction = const blender::fn::CPPType *(*)();
using SocketGetGeometryNodesCPPValueFunction = void (*)(const struct bNodeSocket &socket,
                                                        void *r_value);

#else
typedef void *NodeMultiFunctionBuildFunction;
typedef void *NodeGeometryExecFunction;
typedef void *NodeDeclareFunction;
typedef void *SocketGetCPPTypeFunction;
typedef void *SocketGetGeometryNodesCPPTypeFunction;
typedef void *SocketGetGeometryNodesCPPValueFunction;
typedef void *SocketGetCPPValueFunction;
#endif

/**
 * \brief Defines a socket type.
 *
 * Defines the appearance and behavior of a socket in the UI.
 */
typedef struct bNodeSocketType {
  /* Identifier name */
  char idname[64];
  /* Type label */
  char label[64];

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
  void (*interface_register_properties)(struct bNodeTree *ntree,
                                        struct bNodeSocket *stemp,
                                        struct StructRNA *data_srna);
  void (*interface_init_socket)(struct bNodeTree *ntree,
                                struct bNodeSocket *stemp,
                                struct bNode *node,
                                struct bNodeSocket *sock,
                                const char *data_path);
  void (*interface_verify_socket)(struct bNodeTree *ntree,
                                  struct bNodeSocket *stemp,
                                  struct bNode *node,
                                  struct bNodeSocket *sock,
                                  const char *data_path);
  void (*interface_from_socket)(struct bNodeTree *ntree,
                                struct bNodeSocket *stemp,
                                struct bNode *node,
                                struct bNodeSocket *sock);

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
  SocketGetCPPTypeFunction get_base_cpp_type;
  /* Get the value of this socket in a generic way. */
  SocketGetCPPValueFunction get_base_cpp_value;
  /* Get geometry nodes cpp type. */
  SocketGetGeometryNodesCPPTypeFunction get_geometry_nodes_cpp_type;
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
  void *next, *prev;

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

  /* Main draw function for the node */
  void (*draw_nodetype)(const struct bContext *C,
                        struct ARegion *region,
                        struct SpaceNode *snode,
                        struct bNodeTree *ntree,
                        struct bNode *node,
                        bNodeInstanceKey key);
  /* Updates the node geometry attributes according to internal state before actual drawing */
  void (*draw_nodetype_prepare)(const struct bContext *C,
                                struct bNodeTree *ntree,
                                struct bNode *node);

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
  void (*labelfunc)(struct bNodeTree *ntree, struct bNode *node, char *label, int maxlen);
  /** Optional custom resize handle polling. */
  int (*resize_area_func)(struct bNode *node, int x, int y);
  /** Optional selection area polling. */
  int (*select_area_func)(struct bNode *node, int x, int y);
  /** Optional tweak area polling (for grabbing). */
  int (*tweak_area_func)(struct bNode *node, int x, int y);

  /** Called when the node is updated in the editor. */
  void (*updatefunc)(struct bNodeTree *ntree, struct bNode *node);
  /** Check and update if internal ID data has changed. */
  void (*group_update_func)(struct bNodeTree *ntree, struct bNode *node);

  /** Initialize a new node instance of this type after creation. */
  void (*initfunc)(struct bNodeTree *ntree, struct bNode *node);
  /** Free the node instance. */
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
   * Can this node type be added to a node tree?
   * \param r_disabled_hint: Optional hint to display in the UI when the poll fails.
   *                         The callback can set this to a static string without having to
   *                         null-check it (or without setting it to null if it's not used).
   *                         The caller must pass a valid `const char **` and null-initialize it
   *                         when it's not just a dummy, that is, if it actually wants to access
   *                         the returned disabled-hint (null-check needed!).
   */
  bool (*poll)(struct bNodeType *ntype, struct bNodeTree *nodetree, const char **r_disabled_hint);
  /** Can this node be added to a node tree?
   * \param r_disabled_hint: See `poll()`.
   */
  bool (*poll_instance)(struct bNode *node,
                        struct bNodeTree *nodetree,
                        const char **r_disabled_hint);

  /* optional handling of link insertion */
  void (*insert_link)(struct bNodeTree *ntree, struct bNode *node, struct bNodeLink *link);
  /* Update the internal links list, for muting and disconnect operators. */
  void (*update_internal_links)(struct bNodeTree *, struct bNode *node);

  void (*free_self)(struct bNodeType *ntype);

  /* **** execution callbacks **** */
  NodeInitExecFunction init_exec_fn;
  NodeFreeExecFunction free_exec_fn;
  NodeExecFunction exec_fn;
  /* gpu */
  NodeGPUExecFunction gpu_fn;

  /* Build a multi-function for this node. */
  NodeMultiFunctionBuildFunction build_multi_function;

  /* Execute a geometry node. */
  NodeGeometryExecFunction geometry_node_execute;
  bool geometry_node_execute_supports_laziness;

  /* Declares which sockets the node has. */
  NodeDeclareFunction declare;
  /* Different nodes of this type can have different declarations. */
  bool declaration_is_dynamic;
  /* Declaration to be used when it is not dynamic. */
  NodeDeclarationHandle *fixed_declaration;

  /* RNA integration */
  ExtensionRNA rna_ext;
} bNodeType;

/* nodetype->nclass, for add-menu and themes */
#define NODE_CLASS_INPUT 0
#define NODE_CLASS_OUTPUT 1
#define NODE_CLASS_OP_COLOR 3
#define NODE_CLASS_OP_VECTOR 4
#define NODE_CLASS_OP_FILTER 5
#define NODE_CLASS_GROUP 6
// #define NODE_CLASS_FILE              7
#define NODE_CLASS_CONVERTER 8
#define NODE_CLASS_MATTE 9
#define NODE_CLASS_DISTORT 10
// #define NODE_CLASS_OP_DYNAMIC        11 /* deprecated */
#define NODE_CLASS_PATTERN 12
#define NODE_CLASS_TEXTURE 13
// #define NODE_CLASS_EXECUTION     14
// #define NODE_CLASS_GETDATA           15
// #define NODE_CLASS_SETDATA           16
// #define NODE_CLASS_MATH              17
// #define NODE_CLASS_MATH_VECTOR       18
// #define NODE_CLASS_MATH_ROTATION 19
// #define NODE_CLASS_PARTICLES     25
// #define NODE_CLASS_TRANSFORM     30
// #define NODE_CLASS_COMBINE           31
#define NODE_CLASS_SCRIPT 32
#define NODE_CLASS_INTERFACE 33
#define NODE_CLASS_SHADER 40
#define NODE_CLASS_GEOMETRY 41
#define NODE_CLASS_ATTRIBUTE 42
#define NODE_CLASS_LAYOUT 100

/* node resize directions */
#define NODE_RESIZE_TOP 1
#define NODE_RESIZE_BOTTOM 2
#define NODE_RESIZE_RIGHT 4
#define NODE_RESIZE_LEFT 8

typedef enum eNodeSizePreset {
  NODE_SIZE_DEFAULT,
  NODE_SIZE_SMALL,
  NODE_SIZE_MIDDLE,
  NODE_SIZE_LARGE,
} eNodeSizePreset;

struct bNodeTreeExec;

typedef void (*bNodeClassCallback)(void *calldata, int nclass, const char *name);
typedef struct bNodeTreeType {
  int type;        /* type identifier */
  char idname[64]; /* identifier name */

  char ui_name[64];
  char ui_description[256];
  int ui_icon;

  /* callbacks */
  void (*free_cache)(struct bNodeTree *ntree);
  void (*free_node_cache)(struct bNodeTree *ntree, struct bNode *node);
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
  void (*local_sync)(struct bNodeTree *localtree, struct bNodeTree *ntree);
  void (*local_merge)(struct Main *bmain, struct bNodeTree *localtree, struct bNodeTree *ntree);

  /* Tree update. Overrides `nodetype->updatetreefunc` ! */
  void (*update)(struct bNodeTree *ntree);

  bool (*validate_link)(struct bNodeTree *ntree, struct bNodeLink *link);

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
bool ntreeIsRegistered(struct bNodeTree *ntree);
struct GHashIterator *ntreeTypeGetIterator(void);

/* Helper macros for iterating over tree types. */
#define NODE_TREE_TYPES_BEGIN(ntype) \
  { \
    GHashIterator *__node_tree_type_iter__ = ntreeTypeGetIterator(); \
    for (; !BLI_ghashIterator_done(__node_tree_type_iter__); \
         BLI_ghashIterator_step(__node_tree_type_iter__)) { \
      bNodeTreeType *ntype = (bNodeTreeType *)BLI_ghashIterator_getValue(__node_tree_type_iter__);

#define NODE_TREE_TYPES_END \
  } \
  BLI_ghashIterator_free(__node_tree_type_iter__); \
  } \
  (void)0

void ntreeSetTypes(const struct bContext *C, struct bNodeTree *ntree);

struct bNodeTree *ntreeAddTree(struct Main *bmain, const char *name, const char *idname);

/* copy/free funcs, need to manage ID users */
void ntreeFreeTree(struct bNodeTree *ntree);
/* Free tree which is embedded into another datablock. */
void ntreeFreeEmbeddedTree(struct bNodeTree *ntree);
struct bNodeTree *ntreeCopyTree_ex(const struct bNodeTree *ntree,
                                   struct Main *bmain,
                                   const bool do_id_user);
struct bNodeTree *ntreeCopyTree(struct Main *bmain, const struct bNodeTree *ntree);

struct bNodeTree **BKE_ntree_ptr_from_id(struct ID *id);
struct bNodeTree *ntreeFromID(struct ID *id);

void ntreeFreeLocalNode(struct bNodeTree *ntree, struct bNode *node);
void ntreeFreeLocalTree(struct bNodeTree *ntree);
struct bNode *ntreeFindType(const struct bNodeTree *ntree, int type);
bool ntreeHasType(const struct bNodeTree *ntree, int type);
bool ntreeHasTree(const struct bNodeTree *ntree, const struct bNodeTree *lookup);
void ntreeUpdateTree(struct Main *main, struct bNodeTree *ntree);
void ntreeUpdateAllNew(struct Main *main);
void ntreeUpdateAllUsers(struct Main *main, struct ID *id, int tree_update_flag);

void ntreeGetDependencyList(struct bNodeTree *ntree,
                            struct bNode ***r_deplist,
                            int *r_deplist_len);

/* XXX old trees handle output flags automatically based on special output
 * node types and last active selection.
 * New tree types have a per-output socket flag to indicate the final output to use explicitly.
 */
void ntreeSetOutput(struct bNodeTree *ntree);

void ntreeFreeCache(struct bNodeTree *ntree);

bool ntreeNodeExists(const struct bNodeTree *ntree, const struct bNode *testnode);
bool ntreeOutputExists(const struct bNode *node, const struct bNodeSocket *testsock);
void ntreeNodeFlagSet(const bNodeTree *ntree, const int flag, const bool enable);
struct bNodeTree *ntreeLocalize(struct bNodeTree *ntree);
void ntreeLocalSync(struct bNodeTree *localtree, struct bNodeTree *ntree);
void ntreeLocalMerge(struct Main *bmain, struct bNodeTree *localtree, struct bNodeTree *ntree);

void ntreeBlendWrite(struct BlendWriter *writer, struct bNodeTree *ntree);
void ntreeBlendReadData(struct BlendDataReader *reader, struct bNodeTree *ntree);
void ntreeBlendReadLib(struct BlendLibReader *reader, struct bNodeTree *ntree);
void ntreeBlendReadExpand(struct BlendExpander *expander, struct bNodeTree *ntree);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Tree Interface
 * \{ */
struct bNodeSocket *ntreeFindSocketInterface(struct bNodeTree *ntree,
                                             eNodeSocketInOut in_out,
                                             const char *identifier);
struct bNodeSocket *ntreeAddSocketInterface(struct bNodeTree *ntree,
                                            eNodeSocketInOut in_out,
                                            const char *idname,
                                            const char *name);
struct bNodeSocket *ntreeInsertSocketInterface(struct bNodeTree *ntree,
                                               eNodeSocketInOut in_out,
                                               const char *idname,
                                               struct bNodeSocket *next_sock,
                                               const char *name);
struct bNodeSocket *ntreeAddSocketInterfaceFromSocket(struct bNodeTree *ntree,
                                                      struct bNode *from_node,
                                                      struct bNodeSocket *from_sock);
struct bNodeSocket *ntreeInsertSocketInterfaceFromSocket(struct bNodeTree *ntree,
                                                         struct bNodeSocket *next_sock,
                                                         struct bNode *from_node,
                                                         struct bNodeSocket *from_sock);
void ntreeRemoveSocketInterface(struct bNodeTree *ntree, struct bNodeSocket *sock);

struct StructRNA *ntreeInterfaceTypeGet(struct bNodeTree *ntree, bool create);
void ntreeInterfaceTypeFree(struct bNodeTree *ntree);
void ntreeInterfaceTypeUpdate(struct bNodeTree *ntree);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic API, Nodes
 * \{ */

struct bNodeType *nodeTypeFind(const char *idname);
void nodeRegisterType(struct bNodeType *ntype);
void nodeUnregisterType(struct bNodeType *ntype);
bool nodeTypeUndefined(struct bNode *node);
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
bool nodeSocketIsRegistered(struct bNodeSocket *sock);
struct GHashIterator *nodeSocketTypeGetIterator(void);
const char *nodeSocketTypeLabel(const bNodeSocketType *stype);

bool nodeIsStaticSocketType(const struct bNodeSocketType *stype);
const char *nodeStaticSocketType(int type, int subtype);
const char *nodeStaticSocketInterfaceType(int type, int subtype);
const char *nodeStaticSocketLabel(int type, int subtype);

/* Helper macros for iterating over node types. */
#define NODE_SOCKET_TYPES_BEGIN(stype) \
  { \
    GHashIterator *__node_socket_type_iter__ = nodeSocketTypeGetIterator(); \
    for (; !BLI_ghashIterator_done(__node_socket_type_iter__); \
         BLI_ghashIterator_step(__node_socket_type_iter__)) { \
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
struct bNodeSocket *nodeInsertSocket(struct bNodeTree *ntree,
                                     struct bNode *node,
                                     eNodeSocketInOut in_out,
                                     const char *idname,
                                     struct bNodeSocket *next_sock,
                                     const char *identifier,
                                     const char *name);
struct bNodeSocket *nodeAddStaticSocket(struct bNodeTree *ntree,
                                        struct bNode *node,
                                        eNodeSocketInOut in_out,
                                        int type,
                                        int subtype,
                                        const char *identifier,
                                        const char *name);
struct bNodeSocket *nodeInsertStaticSocket(struct bNodeTree *ntree,
                                           struct bNode *node,
                                           eNodeSocketInOut in_out,
                                           int type,
                                           int subtype,
                                           struct bNodeSocket *next_sock,
                                           const char *identifier,
                                           const char *name);
void nodeRemoveSocket(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock);
void nodeRemoveSocketEx(struct bNodeTree *ntree,
                        struct bNode *node,
                        struct bNodeSocket *sock,
                        bool do_id_user);
void nodeRemoveAllSockets(struct bNodeTree *ntree, struct bNode *node);
void nodeModifySocketType(struct bNodeTree *ntree,
                          struct bNode *node,
                          struct bNodeSocket *sock,
                          const char *idname);
void nodeModifySocketTypeStatic(
    struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock, int type, int subtype);

struct bNode *nodeAddNode(const struct bContext *C, struct bNodeTree *ntree, const char *idname);
struct bNode *nodeAddStaticNode(const struct bContext *C, struct bNodeTree *ntree, int type);
void nodeUnlinkNode(struct bNodeTree *ntree, struct bNode *node);
void nodeUniqueName(struct bNodeTree *ntree, struct bNode *node);

/* Delete node, associated animation data and ID user count. */
void nodeRemoveNode(struct Main *bmain,
                    struct bNodeTree *ntree,
                    struct bNode *node,
                    bool do_id_user);

struct bNode *BKE_node_copy_ex(struct bNodeTree *ntree,
                               const struct bNode *node_src,
                               const int flag,
                               const bool unique_name);

/* Same as BKE_node_copy_ex() but stores pointers to a new node and its sockets in the source
 * node.
 *
 * NOTE: DANGER ZONE!
 *
 * TODO(sergey): Maybe it's better to make BKE_node_copy_ex() return a mapping from old node and
 * sockets to new one. */
struct bNode *BKE_node_copy_store_new_pointers(struct bNodeTree *ntree,
                                               struct bNode *node_src,
                                               const int flag);
struct bNodeTree *ntreeCopyTree_ex_new_pointers(const struct bNodeTree *ntree,
                                                struct Main *bmain,
                                                const bool do_id_user);

struct bNodeLink *nodeAddLink(struct bNodeTree *ntree,
                              struct bNode *fromnode,
                              struct bNodeSocket *fromsock,
                              struct bNode *tonode,
                              struct bNodeSocket *tosock);
void nodeRemLink(struct bNodeTree *ntree, struct bNodeLink *link);
void nodeRemSocketLinks(struct bNodeTree *ntree, struct bNodeSocket *sock);
void nodeMuteLinkToggle(struct bNodeTree *ntree, struct bNodeLink *link);
bool nodeLinkIsHidden(const struct bNodeLink *link);
void nodeInternalRelink(struct bNodeTree *ntree, struct bNode *node);

void nodeToView(const struct bNode *node, float x, float y, float *rx, float *ry);
void nodeFromView(const struct bNode *node, float x, float y, float *rx, float *ry);
bool nodeAttachNodeCheck(const struct bNode *node, const struct bNode *parent);
void nodeAttachNode(struct bNode *node, struct bNode *parent);
void nodeDetachNode(struct bNode *node);

void nodePositionRelative(struct bNode *from_node,
                          struct bNode *to_node,
                          struct bNodeSocket *from_sock,
                          struct bNodeSocket *to_sock);
void nodePositionPropagate(struct bNode *node);

struct bNode *nodeFindNodebyName(struct bNodeTree *ntree, const char *name);
bool nodeFindNode(struct bNodeTree *ntree,
                  struct bNodeSocket *sock,
                  struct bNode **r_node,
                  int *r_sockindex);
struct bNode *nodeFindRootParent(bNode *node);

bool nodeIsChildOf(const bNode *parent, const bNode *child);

void nodeChainIter(const bNodeTree *ntree,
                   const bNode *node_start,
                   bool (*callback)(bNode *, bNode *, void *, const bool),
                   void *userdata,
                   const bool reversed);
void nodeChainIterBackwards(const bNodeTree *ntree,
                            const bNode *node_start,
                            bool (*callback)(bNode *, bNode *, void *),
                            void *userdata,
                            int recursion_lvl);
void nodeParentsIter(bNode *node, bool (*callback)(bNode *, void *), void *userdata);

struct bNodeLink *nodeFindLink(struct bNodeTree *ntree,
                               const struct bNodeSocket *from,
                               const struct bNodeSocket *to);
int nodeCountSocketLinks(const struct bNodeTree *ntree, const struct bNodeSocket *sock);

void nodeSetSelected(struct bNode *node, bool select);
void nodeSetActive(struct bNodeTree *ntree, struct bNode *node);
struct bNode *nodeGetActive(struct bNodeTree *ntree);
struct bNode *nodeGetActiveID(struct bNodeTree *ntree, short idtype);
bool nodeSetActiveID(struct bNodeTree *ntree, short idtype, struct ID *id);
void nodeClearActive(struct bNodeTree *ntree);
void nodeClearActiveID(struct bNodeTree *ntree, short idtype);
struct bNode *nodeGetActiveTexture(struct bNodeTree *ntree);

void nodeUpdate(struct bNodeTree *ntree, struct bNode *node);
bool nodeUpdateID(struct bNodeTree *ntree, struct ID *id);
void nodeUpdateInternalLinks(struct bNodeTree *ntree, struct bNode *node);

int nodeSocketIsHidden(const struct bNodeSocket *sock);
void ntreeTagUsedSockets(struct bNodeTree *ntree);
void nodeSetSocketAvailability(struct bNodeSocket *sock, bool is_available);

int nodeSocketLinkLimit(const struct bNodeSocket *sock);

bool nodeDeclarationEnsure(struct bNodeTree *ntree, struct bNode *node);
bool nodeDeclarationEnsureOnOutdatedNode(struct bNodeTree *ntree, struct bNode *node);
void nodeSocketDeclarationsUpdate(struct bNode *node);

/* Node Clipboard */
void BKE_node_clipboard_init(const struct bNodeTree *ntree);
void BKE_node_clipboard_clear(void);
void BKE_node_clipboard_free(void);
bool BKE_node_clipboard_validate(void);
void BKE_node_clipboard_add_node(struct bNode *node);
void BKE_node_clipboard_add_link(struct bNodeLink *link);
const struct ListBase *BKE_node_clipboard_get_nodes(void);
const struct ListBase *BKE_node_clipboard_get_links(void);
int BKE_node_clipboard_get_type(void);

/* Node Instance Hash */
typedef struct bNodeInstanceHash {
  /** XXX should be made a direct member, #GHash allocation needs to support it */
  GHash *ghash;
} bNodeInstanceHash;

typedef void (*bNodeInstanceValueFP)(void *value);

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

typedef GHashIterator bNodeInstanceHashIterator;

BLI_INLINE bNodeInstanceHashIterator *BKE_node_instance_hash_iterator_new(bNodeInstanceHash *hash)
{
  return BLI_ghashIterator_new(hash->ghash);
}
BLI_INLINE void BKE_node_instance_hash_iterator_init(bNodeInstanceHashIterator *iter,
                                                     bNodeInstanceHash *hash)
{
  BLI_ghashIterator_init(iter, hash->ghash);
}
BLI_INLINE void BKE_node_instance_hash_iterator_free(bNodeInstanceHashIterator *iter)
{
  BLI_ghashIterator_free(iter);
}
BLI_INLINE bNodeInstanceKey
BKE_node_instance_hash_iterator_get_key(bNodeInstanceHashIterator *iter)
{
  return *(bNodeInstanceKey *)BLI_ghashIterator_getKey(iter);
}
BLI_INLINE void *BKE_node_instance_hash_iterator_get_value(bNodeInstanceHashIterator *iter)
{
  return BLI_ghashIterator_getValue(iter);
}
BLI_INLINE void BKE_node_instance_hash_iterator_step(bNodeInstanceHashIterator *iter)
{
  BLI_ghashIterator_step(iter);
}
BLI_INLINE bool BKE_node_instance_hash_iterator_done(bNodeInstanceHashIterator *iter)
{
  return BLI_ghashIterator_done(iter);
}

#define NODE_INSTANCE_HASH_ITER(iter_, hash_) \
  for (BKE_node_instance_hash_iterator_init(&iter_, hash_); \
       BKE_node_instance_hash_iterator_done(&iter_) == false; \
       BKE_node_instance_hash_iterator_step(&iter_))

/* Node Previews */

bool BKE_node_preview_used(const struct bNode *node);
bNodePreview *BKE_node_preview_verify(
    struct bNodeInstanceHash *previews, bNodeInstanceKey key, int xsize, int ysize, bool create);
bNodePreview *BKE_node_preview_copy(struct bNodePreview *preview);
void BKE_node_preview_free(struct bNodePreview *preview);
void BKE_node_preview_init_tree(struct bNodeTree *ntree,
                                int xsize,
                                int ysize,
                                bool create_previews);
void BKE_node_preview_free_tree(struct bNodeTree *ntree);
void BKE_node_preview_remove_unused(struct bNodeTree *ntree);
void BKE_node_preview_clear(struct bNodePreview *preview);
void BKE_node_preview_clear_tree(struct bNodeTree *ntree);

void BKE_node_preview_sync_tree(struct bNodeTree *to_ntree, struct bNodeTree *from_ntree);
void BKE_node_preview_merge_tree(struct bNodeTree *to_ntree,
                                 struct bNodeTree *from_ntree,
                                 bool remove_old);

void BKE_node_preview_set_pixel(
    struct bNodePreview *preview, const float col[4], int x, int y, bool do_manage);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Type Access
 * \{ */

void nodeLabel(struct bNodeTree *ntree, struct bNode *node, char *label, int maxlen);
const char *nodeSocketLabel(const struct bNodeSocket *sock);

bool nodeGroupPoll(struct bNodeTree *nodetree,
                   struct bNodeTree *grouptree,
                   const char **r_disabled_hint);

/* Init a new node type struct with default values and callbacks */
void node_type_base(struct bNodeType *ntype, int type, const char *name, short nclass, short flag);
void node_type_base_custom(
    struct bNodeType *ntype, const char *idname, const char *name, short nclass, short flag);
void node_type_socket_templates(struct bNodeType *ntype,
                                struct bNodeSocketTemplate *inputs,
                                struct bNodeSocketTemplate *outputs);
void node_type_size(struct bNodeType *ntype, int width, int minwidth, int maxwidth);
void node_type_size_preset(struct bNodeType *ntype, eNodeSizePreset size);
void node_type_init(struct bNodeType *ntype,
                    void (*initfunc)(struct bNodeTree *ntree, struct bNode *node));
void node_type_storage(struct bNodeType *ntype,
                       const char *storagename,
                       void (*freefunc)(struct bNode *node),
                       void (*copyfunc)(struct bNodeTree *dest_ntree,
                                        struct bNode *dest_node,
                                        const struct bNode *src_node));
void node_type_label(
    struct bNodeType *ntype,
    void (*labelfunc)(struct bNodeTree *ntree, struct bNode *, char *label, int maxlen));
void node_type_update(struct bNodeType *ntype,
                      void (*updatefunc)(struct bNodeTree *ntree, struct bNode *node));
void node_type_group_update(struct bNodeType *ntype,
                            void (*group_update_func)(struct bNodeTree *ntree,
                                                      struct bNode *node));

void node_type_exec(struct bNodeType *ntype,
                    NodeInitExecFunction init_exec_fn,
                    NodeFreeExecFunction free_exec_fn,
                    NodeExecFunction exec_fn);
void node_type_gpu(struct bNodeType *ntype, NodeGPUExecFunction gpu_fn);
void node_type_internal_links(struct bNodeType *ntype,
                              void (*update_internal_links)(struct bNodeTree *, struct bNode *));

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Generic Functions
 * \{ */

bool BKE_node_is_connected_to_output(struct bNodeTree *ntree, struct bNode *node);

/* ************** COMMON NODES *************** */

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

void BKE_node_tree_unlink_id(ID *id, struct bNodeTree *ntree);

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
 *   Check nodetree->idname or nodetree->typeinfo to use only specific types.
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

void BKE_nodetree_remove_layer_n(struct bNodeTree *ntree,
                                 struct Scene *scene,
                                 const int layer_index);

/* -------------------------------------------------------------------- */
/** \name Shader Nodes
 * \{ */

/* NOTE: types are needed to restore callbacks, don't change values. */
/* range 1 - 100 is reserved for common nodes */
/* using toolbox, we add node groups by assuming the values below
 * don't exceed NODE_GROUP_MENU for now. */

//#define SH_NODE_OUTPUT        1

//#define SH_NODE_MATERIAL  100
#define SH_NODE_RGB 101
#define SH_NODE_VALUE 102
#define SH_NODE_MIX_RGB 103
#define SH_NODE_VALTORGB 104
#define SH_NODE_RGBTOBW 105
#define SH_NODE_SHADERTORGB 106
//#define SH_NODE_TEXTURE       106
#define SH_NODE_NORMAL 107
//#define SH_NODE_GEOMETRY  108
#define SH_NODE_MAPPING 109
#define SH_NODE_CURVE_VEC 110
#define SH_NODE_CURVE_RGB 111
#define SH_NODE_CAMERA 114
#define SH_NODE_MATH 115
#define SH_NODE_VECTOR_MATH 116
#define SH_NODE_SQUEEZE 117
//#define SH_NODE_MATERIAL_EXT  118
#define SH_NODE_INVERT 119
#define SH_NODE_SEPRGB 120
#define SH_NODE_COMBRGB 121
#define SH_NODE_HUE_SAT 122
#define NODE_DYNAMIC 123

#define SH_NODE_OUTPUT_MATERIAL 124
#define SH_NODE_OUTPUT_WORLD 125
#define SH_NODE_OUTPUT_LIGHT 126
#define SH_NODE_FRESNEL 127
#define SH_NODE_MIX_SHADER 128
#define SH_NODE_ATTRIBUTE 129
#define SH_NODE_BACKGROUND 130
#define SH_NODE_BSDF_ANISOTROPIC 131
#define SH_NODE_BSDF_DIFFUSE 132
#define SH_NODE_BSDF_GLOSSY 133
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
#define SH_NODE_SEPHSV 183
#define SH_NODE_COMBHSV 184
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

/* custom defines options for Material node */
// #define SH_NODE_MAT_DIFF 1
// #define SH_NODE_MAT_SPEC 2
// #define SH_NODE_MAT_NEG 4

/* API */

struct bNodeTreeExec *ntreeShaderBeginExecTree(struct bNodeTree *ntree);
void ntreeShaderEndExecTree(struct bNodeTreeExec *exec);
struct bNode *ntreeShaderOutputNode(struct bNodeTree *ntree, int target);

void ntreeGPUMaterialNodes(struct bNodeTree *localtree,
                           struct GPUMaterial *mat,
                           bool *has_surface_output,
                           bool *has_volume_output);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Composite Nodes
 * \{ */

/* output socket defines */
#define RRES_OUT_IMAGE 0
#define RRES_OUT_ALPHA 1
// #define RRES_OUT_Z 2
// #define RRES_OUT_NORMAL 3
// #define RRES_OUT_UV 4
// #define RRES_OUT_VEC 5
// #define RRES_OUT_RGBA 6
#define RRES_OUT_DIFF 7
// #define RRES_OUT_SPEC 8
// #define RRES_OUT_SHADOW 9
// #define RRES_OUT_AO 10
// #define RRES_OUT_REFLECT 11
// #define RRES_OUT_REFRACT 12
// #define RRES_OUT_INDIRECT 13
// #define RRES_OUT_INDEXOB 14
// #define RRES_OUT_INDEXMA 15
// #define RRES_OUT_MIST 16
// #define RRES_OUT_EMIT 17
// #define RRES_OUT_ENV 18
// #define RRES_OUT_DIFF_DIRECT 19
// #define RRES_OUT_DIFF_INDIRECT 20
// #define RRES_OUT_DIFF_COLOR 21
// #define RRES_OUT_GLOSSY_DIRECT 22
// #define RRES_OUT_GLOSSY_INDIRECT 23
// #define RRES_OUT_GLOSSY_COLOR 24
// #define RRES_OUT_TRANSM_DIRECT 25
// #define RRES_OUT_TRANSM_INDIRECT 26
// #define RRES_OUT_TRANSM_COLOR 27
// #define RRES_OUT_SUBSURFACE_DIRECT 28
// #define RRES_OUT_SUBSURFACE_INDIRECT 29
// #define RRES_OUT_SUBSURFACE_COLOR 30
// #define RRES_OUT_DEBUG 31

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
#define CMP_NODE_SEPRGBA 216
#define CMP_NODE_SEPHSVA 217
#define CMP_NODE_SETALPHA 218
#define CMP_NODE_HUE_SAT 219
#define CMP_NODE_IMAGE 220
#define CMP_NODE_R_LAYERS 221
#define CMP_NODE_COMPOSITE 222
#define CMP_NODE_OUTPUT_FILE 223
#define CMP_NODE_TEXTURE 224
#define CMP_NODE_TRANSLATE 225
#define CMP_NODE_ZCOMBINE 226
#define CMP_NODE_COMBRGBA 227
#define CMP_NODE_DILATEERODE 228
#define CMP_NODE_ROTATE 229
#define CMP_NODE_SCALE 230
#define CMP_NODE_SEPYCCA 231
#define CMP_NODE_COMBYCCA 232
#define CMP_NODE_SEPYUVA 233
#define CMP_NODE_COMBYUVA 234
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
#define CMP_NODE_COMBHSVA 246
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

/* channel toggles */
#define CMP_CHAN_RGB 1
#define CMP_CHAN_A 2

/* filter types */
#define CMP_FILT_SOFT 0
#define CMP_FILT_SHARP 1
#define CMP_FILT_LAPLACE 2
#define CMP_FILT_SOBEL 3
#define CMP_FILT_PREWITT 4
#define CMP_FILT_KIRSCH 5
#define CMP_FILT_SHADOW 6

/* scale node type, in custom1 */
#define CMP_SCALE_RELATIVE 0
#define CMP_SCALE_ABSOLUTE 1
#define CMP_SCALE_SCENEPERCENT 2
#define CMP_SCALE_RENDERPERCENT 3
/* custom2 */
#define CMP_SCALE_RENDERSIZE_FRAME_ASPECT (1 << 0)
#define CMP_SCALE_RENDERSIZE_FRAME_CROP (1 << 1)

/* track position node, in custom1 */
#define CMP_TRACKPOS_ABSOLUTE 0
#define CMP_TRACKPOS_RELATIVE_START 1
#define CMP_TRACKPOS_RELATIVE_FRAME 2
#define CMP_TRACKPOS_ABSOLUTE_FRAME 3

/* Cryptomatte source. */
#define CMP_CRYPTOMATTE_SRC_RENDER 0
#define CMP_CRYPTOMATTE_SRC_IMAGE 1

/* Default SMAA configuration values. */
#define CMP_DEFAULT_SMAA_THRESHOLD 1.0f
#define CMP_DEFAULT_SMAA_CONTRAST_LIMIT 0.2f
#define CMP_DEFAULT_SMAA_CORNER_ROUNDING 0.25f

/* API */
void ntreeCompositExecTree(struct Scene *scene,
                           struct bNodeTree *ntree,
                           struct RenderData *rd,
                           int rendering,
                           int do_previews,
                           const struct ColorManagedViewSettings *view_settings,
                           const struct ColorManagedDisplaySettings *display_settings,
                           const char *view_name);
void ntreeCompositTagRender(struct Scene *scene);
void ntreeCompositUpdateRLayers(struct bNodeTree *ntree);
void ntreeCompositRegisterPass(struct bNodeTree *ntree,
                               struct Scene *scene,
                               struct ViewLayer *view_layer,
                               const char *name,
                               eNodeSocketDatatype type);
void ntreeCompositClearTags(struct bNodeTree *ntree);

struct bNodeSocket *ntreeCompositOutputFileAddSocket(struct bNodeTree *ntree,
                                                     struct bNode *node,
                                                     const char *name,
                                                     struct ImageFormatData *im_format);
int ntreeCompositOutputFileRemoveActiveSocket(struct bNodeTree *ntree, struct bNode *node);
void ntreeCompositOutputFileSetPath(struct bNode *node,
                                    struct bNodeSocket *sock,
                                    const char *name);
void ntreeCompositOutputFileSetLayer(struct bNode *node,
                                     struct bNodeSocket *sock,
                                     const char *name);
/* needed in do_versions */
void ntreeCompositOutputFileUniquePath(struct ListBase *list,
                                       struct bNodeSocket *sock,
                                       const char defname[],
                                       char delim);
void ntreeCompositOutputFileUniqueLayer(struct ListBase *list,
                                        struct bNodeSocket *sock,
                                        const char defname[],
                                        char delim);

void ntreeCompositColorBalanceSyncFromLGG(bNodeTree *ntree, bNode *node);
void ntreeCompositColorBalanceSyncFromCDL(bNodeTree *ntree, bNode *node);

void ntreeCompositCryptomatteSyncFromAdd(const Scene *scene, bNode *node);
void ntreeCompositCryptomatteSyncFromRemove(bNode *node);
bNodeSocket *ntreeCompositCryptomatteAddSocket(bNodeTree *ntree, bNode *node);
int ntreeCompositCryptomatteRemoveSocket(bNodeTree *ntree, bNode *node);
void ntreeCompositCryptomatteLayerPrefix(const Scene *scene,
                                         const bNode *node,
                                         char *r_prefix,
                                         size_t prefix_len);
/* Update the runtime layer names with the crypto-matte layer names of the references
 * render layer or image. */
void ntreeCompositCryptomatteUpdateLayerNames(const Scene *scene, bNode *node);
struct CryptomatteSession *ntreeCompositCryptomatteSession(const Scene *scene, bNode *node);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Nodes
 * \{ */

struct TexResult;

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
#define TEX_NODE_COMPOSE 419
#define TEX_NODE_DECOMPOSE 420
#define TEX_NODE_VALTONOR 421
#define TEX_NODE_SCALE 422
#define TEX_NODE_AT 423

/* 501-599 reserved. Use like this: TEX_NODE_PROC + TEX_CLOUDS, etc */
#define TEX_NODE_PROC 500
#define TEX_NODE_PROC_MAX 600

/* API */
void ntreeTexCheckCyclics(struct bNodeTree *ntree);

struct bNodeTreeExec *ntreeTexBeginExecTree(struct bNodeTree *ntree);
void ntreeTexEndExecTree(struct bNodeTreeExec *exec);
int ntreeTexExecTree(struct bNodeTree *ntree,
                     struct TexResult *target,
                     const float co[3],
                     float dxt[3],
                     float dyt[3],
                     int osatex,
                     const short thread,
                     const struct Tex *tex,
                     short which_output,
                     int cfra,
                     int preview,
                     struct MTex *mtex);
/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry Nodes
 * \{ */

#define GEO_NODE_TRIANGULATE 1000
#define GEO_NODE_LEGACY_EDGE_SPLIT 1001
#define GEO_NODE_TRANSFORM 1002
#define GEO_NODE_MESH_BOOLEAN 1003
#define GEO_NODE_LEGACY_POINT_DISTRIBUTE 1004
#define GEO_NODE_LEGACY_POINT_INSTANCE 1005
#define GEO_NODE_LEGACY_SUBDIVISION_SURFACE 1006
#define GEO_NODE_OBJECT_INFO 1007
#define GEO_NODE_LEGACY_ATTRIBUTE_RANDOMIZE 1008
#define GEO_NODE_LEGACY_ATTRIBUTE_MATH 1009
#define GEO_NODE_JOIN_GEOMETRY 1010
#define GEO_NODE_LEGACY_ATTRIBUTE_FILL 1011
#define GEO_NODE_LEGACY_ATTRIBUTE_MIX 1012
#define GEO_NODE_LEGACY_ATTRIBUTE_COLOR_RAMP 1013
#define GEO_NODE_LEGACY_POINT_SEPARATE 1014
#define GEO_NODE_LEGACY_ATTRIBUTE_COMPARE 1015
#define GEO_NODE_LEGACY_POINT_ROTATE 1016
#define GEO_NODE_LEGACY_ATTRIBUTE_VECTOR_MATH 1017
#define GEO_NODE_LEGACY_ALIGN_ROTATION_TO_VECTOR 1018
#define GEO_NODE_LEGACY_POINT_TRANSLATE 1019
#define GEO_NODE_LEGACY_POINT_SCALE 1020
#define GEO_NODE_LEGACY_ATTRIBUTE_SAMPLE_TEXTURE 1021
#define GEO_NODE_LEGACY_POINTS_TO_VOLUME 1022
#define GEO_NODE_COLLECTION_INFO 1023
#define GEO_NODE_IS_VIEWPORT 1024
#define GEO_NODE_LEGACY_ATTRIBUTE_PROXIMITY 1025
#define GEO_NODE_LEGACY_VOLUME_TO_MESH 1026
#define GEO_NODE_LEGACY_ATTRIBUTE_COMBINE_XYZ 1027
#define GEO_NODE_LEGACY_ATTRIBUTE_SEPARATE_XYZ 1028
#define GEO_NODE_SUBDIVIDE_MESH 1029
#define GEO_NODE_ATTRIBUTE_REMOVE 1030
#define GEO_NODE_LEGACY_ATTRIBUTE_CONVERT 1031
#define GEO_NODE_MESH_PRIMITIVE_CUBE 1032
#define GEO_NODE_MESH_PRIMITIVE_CIRCLE 1033
#define GEO_NODE_MESH_PRIMITIVE_UV_SPHERE 1034
#define GEO_NODE_MESH_PRIMITIVE_CYLINDER 1035
#define GEO_NODE_MESH_PRIMITIVE_ICO_SPHERE 1036
#define GEO_NODE_MESH_PRIMITIVE_CONE 1037
#define GEO_NODE_MESH_PRIMITIVE_LINE 1038
#define GEO_NODE_MESH_PRIMITIVE_GRID 1039
#define GEO_NODE_LEGACY_ATTRIBUTE_MAP_RANGE 1040
#define GEO_NODE_LEGACY_ATTRIBUTE_CLAMP 1041
#define GEO_NODE_BOUNDING_BOX 1042
#define GEO_NODE_SWITCH 1043
#define GEO_NODE_LEGACY_ATTRIBUTE_TRANSFER 1044
#define GEO_NODE_CURVE_TO_MESH 1045
#define GEO_NODE_LEGACY_ATTRIBUTE_CURVE_MAP 1046
#define GEO_NODE_RESAMPLE_CURVE 1047
#define GEO_NODE_LEGACY_ATTRIBUTE_VECTOR_ROTATE 1048
#define GEO_NODE_LEGACY_MATERIAL_ASSIGN 1049
#define GEO_NODE_INPUT_MATERIAL 1050
#define GEO_NODE_REPLACE_MATERIAL 1051
#define GEO_NODE_LEGACY_MESH_TO_CURVE 1052
#define GEO_NODE_LEGACY_DELETE_GEOMETRY 1053
#define GEO_NODE_CURVE_LENGTH 1054
#define GEO_NODE_LEGACY_SELECT_BY_MATERIAL 1055
#define GEO_NODE_CONVEX_HULL 1056
#define GEO_NODE_LEGACY_CURVE_TO_POINTS 1057
#define GEO_NODE_LEGACY_CURVE_REVERSE 1058
#define GEO_NODE_SEPARATE_COMPONENTS 1059
#define GEO_NODE_LEGACY_CURVE_SUBDIVIDE 1060
#define GEO_NODE_LEGACY_RAYCAST 1061
#define GEO_NODE_CURVE_PRIMITIVE_STAR 1062
#define GEO_NODE_CURVE_PRIMITIVE_SPIRAL 1063
#define GEO_NODE_CURVE_PRIMITIVE_QUADRATIC_BEZIER 1064
#define GEO_NODE_CURVE_PRIMITIVE_BEZIER_SEGMENT 1065
#define GEO_NODE_CURVE_PRIMITIVE_CIRCLE 1066
#define GEO_NODE_VIEWER 1067
#define GEO_NODE_CURVE_PRIMITIVE_LINE 1068
#define GEO_NODE_LEGACY_CURVE_ENDPOINTS 1069
#define GEO_NODE_CURVE_PRIMITIVE_QUADRILATERAL 1070
#define GEO_NODE_TRIM_CURVE 1071
#define GEO_NODE_LEGACY_CURVE_SET_HANDLES 1072
#define GEO_NODE_LEGACY_CURVE_SPLINE_TYPE 1073
#define GEO_NODE_LEGACY_CURVE_SELECT_HANDLES 1074
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
#define GEO_NODE_CURVE_PARAMETER 1088
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
#define GEO_NODE_CURVE_SET_HANDLES 1100
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
#define GEO_NODE_TRANSFER_ATTRIBUTE 1125
#define GEO_NODE_SUBDIVISION_SURFACE 1126
#define GEO_NODE_CURVE_ENDPOINT_SELECTION 1127
#define GEO_NODE_RAYCAST 1128
#define GEO_NODE_CURVE_TO_POINTS 1130
#define GEO_NODE_INSTANCES_TO_POINTS 1131
#define GEO_NODE_IMAGE_TEXTURE 1132
#define GEO_NODE_VOLUME_TO_MESH 1133
#define GEO_NODE_INPUT_ID 1134
#define GEO_NODE_SET_ID 1135

/** \} */

/* -------------------------------------------------------------------- */
/** \name Function Nodes
 * \{ */

#define FN_NODE_BOOLEAN_MATH 1200
#define FN_NODE_COMPARE_FLOATS 1202
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

/** \} */

void BKE_node_system_init(void);
void BKE_node_system_exit(void);

/* -------------------------------------------------------------------- */
/* evaluation support, */

struct Depsgraph;

void BKE_nodetree_shading_params_eval(struct Depsgraph *depsgraph,
                                      struct bNodeTree *ntree_dst,
                                      const struct bNodeTree *ntree_src);

extern struct bNodeType NodeTypeUndefined;
extern struct bNodeSocketType NodeSocketTypeUndefined;

#ifdef __cplusplus
}
#endif
