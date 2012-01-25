/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Bob Holcomb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_NODE_H
#define BKE_NODE_H

/** \file BKE_node.h
 *  \ingroup bke
 */

#include "DNA_listBase.h"

#include "RNA_types.h"

/* not very important, but the stack solver likes to know a maximum */
#define MAX_SOCKET	64

struct bContext;
struct bNode;
struct bNodeLink;
struct bNodeSocket;
struct bNodeStack;
struct bNodeTree;
struct bNodeTreeExec;
struct GPUMaterial;
struct GPUNode;
struct GPUNodeStack;
struct ID;
struct ListBase;
struct Main;
struct uiBlock;
struct uiLayout;
struct MTex;
struct PointerRNA;
struct rctf;
struct RenderData;
struct Scene;
struct Tex;
struct SpaceNode;
struct ARegion;
struct Object;

/* ************** NODE TYPE DEFINITIONS ***** */

/** Compact definition of a node socket.
 * Can be used to quickly define a list of static sockets for a node,
 * which are added to each new node of that type. 
 *
 * \deprecated New nodes should add default sockets in the initialization
 * function instead. This struct is mostly kept for old nodes and should
 * be removed some time.
 */
typedef struct bNodeSocketTemplate {
	int type, limit;
	char name[64];	/* MAX_NAME */
	float val1, val2, val3, val4;   /* default alloc value for inputs */
	float min, max;
	PropertySubType subtype;
	int flag;
	
	/* after this line is used internal only */
	struct bNodeSocket *sock;		/* used to hold verified socket */
} bNodeSocketTemplate;

typedef void (*NodeSocketButtonFunction)(const struct bContext *C, struct uiBlock *block, 
										 struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock,
										 const char *name, int x, int y, int width);

/** Defines a socket type.
 * Defines the appearance and behavior of a socket in the UI.
 */
typedef struct bNodeSocketType {
	int type;
	char ui_name[64];	/* MAX_NAME */
	char ui_description[128];
	int ui_icon;
	char ui_color[4];
	
	const char *value_structname;
	int value_structsize;
	
	NodeSocketButtonFunction buttonfunc;
} bNodeSocketType;

/** Template for creating a node.
 * Stored required parameters to make a new node of a specific type.
 */
typedef struct bNodeTemplate {
	int type;
	
	/* group tree */
	struct bNodeTree *ngroup;
} bNodeTemplate;

/** Defines a node type.
 * Initial attributes and constants for a node as well as callback functions
 * implementing the node behavior.
 */
typedef struct bNodeType {
	void *next,*prev;
	short needs_free;		/* set for allocated types that need to be freed */
	
	int type;
	char name[64];	/* MAX_NAME */
	float width, minwidth, maxwidth;
	float height, minheight, maxheight;
	short nclass, flag, compatibility;
	
	/* templates for static sockets */
	bNodeSocketTemplate *inputs, *outputs;
	
	char storagename[64];			/* struct name for DNA */
	
	/// Main draw function for the node.
	void (*drawfunc)(const struct bContext *C, struct ARegion *ar, struct SpaceNode *snode, struct bNodeTree *ntree, struct bNode *node);
	/// Updates the node geometry attributes according to internal state before actual drawing.
	void (*drawupdatefunc)(const struct bContext *C, struct bNodeTree *ntree, struct bNode *node);
	/// Draw the option buttons on the node.
	void (*uifunc)(struct uiLayout *, struct bContext *C, struct PointerRNA *ptr);
	/// Additional parameters in the side panel.
	void (*uifuncbut)(struct uiLayout *, struct bContext *C, struct PointerRNA *ptr);
	/// Optional custom label function for the node header.
	const char *(*labelfunc)(struct bNode *);
	/// Optional custom resize handle polling.
	int (*resize_area_func)(struct bNode *node, int x, int y);
	
	/// Called when the node is updated in the editor.
	void (*updatefunc)(struct bNodeTree *ntree, struct bNode *node);
	/// Check and update if internal ID data has changed.
	void (*verifyfunc)(struct bNodeTree *ntree, struct bNode *node, struct ID *id);
	
	/// Initialize a new node instance of this type after creation.
	void (*initfunc)(struct bNodeTree *ntree, struct bNode *node, struct bNodeTemplate *ntemp);
	/// Free the custom storage data.
	void (*freestoragefunc)(struct bNode *node);
	/// Make a copy of the custom storage data.
	void (*copystoragefunc)(struct bNode *node, struct bNode *target);
	
	/// Create a template from an existing node.
	struct bNodeTemplate (*templatefunc)(struct bNode *);
	/** If a node can be made from the template in the given node tree.
	 * \example Node groups can not be created inside their own node tree.
	 */
	int (*validfunc)(struct bNodeTree *ntree, struct bNodeTemplate *ntemp);
	
	/// Initialize a node tree associated to this node type.
	void (*inittreefunc)(struct bNodeTree *ntree);
	/// Update a node tree associated to this node type.
	void (*updatetreefunc)(struct bNodeTree *ntree);
	
	/* group edit callbacks for operators */
	/* XXX this is going to be changed as required by the UI */
	struct bNodeTree *(*group_edit_get)(struct bNode *node);
	struct bNodeTree *(*group_edit_set)(struct bNode *node, int edit);
	void (*group_edit_clear)(struct bNode *node);
	
	
	/* **** execution callbacks **** */
	void *(*initexecfunc)(struct bNode *node);
	void (*freeexecfunc)(struct bNode *node, void *nodedata);
	void (*execfunc)(void *data, struct bNode *, struct bNodeStack **, struct bNodeStack **);
	/* XXX this alternative exec function has been added to avoid changing all node types.
	 * when a final generic version of execution code is defined, this will be changed anyway
	 */
	void (*newexecfunc)(void *data, int thread, struct bNode *, void *nodedata, struct bNodeStack **, struct bNodeStack **);
	/* This is the muting callback.
	 * XXX Mimics the newexecfunc signature... Not sure all of this will be useful, we will see.
	 */
	void (*mutefunc)(void *data, int thread, struct bNode *, void *nodedata, struct bNodeStack **, struct bNodeStack **);
	/* And the muting util.
	 * Returns links as a ListBase, as pairs of bNodeStack* if in/out bNodeStacks were provided,
	 * else as pairs of bNodeSocket* if node tree was provided.
	 */
	ListBase (*mutelinksfunc)(struct bNodeTree *, struct bNode *, struct bNodeStack **, struct bNodeStack **,
	                          struct GPUNodeStack *, struct GPUNodeStack *);
	/* gpu */
	int (*gpufunc)(struct GPUMaterial *mat, struct bNode *node, struct GPUNodeStack *in, struct GPUNodeStack *out);
	/* extended gpu function */
	int (*gpuextfunc)(struct GPUMaterial *mat, struct bNode *node, void *nodedata, struct GPUNodeStack *in, struct GPUNodeStack *out);
	/* This is the muting gpu callback.
	 * XXX Mimics the gpuextfunc signature... Not sure all of this will be useful, we will see.
	 */
	int (*gpumutefunc)(struct GPUMaterial *, struct bNode *, void *, struct GPUNodeStack *, struct GPUNodeStack *);
} bNodeType;

/* node->exec, now in use for composites (#define for break is same as ready yes) */
#define NODE_PROCESSING	1
#define NODE_READY		2
#define NODE_BREAK		2
#define NODE_FINISHED	4
#define NODE_FREEBUFS	8
#define NODE_SKIPPED	16

/* sim_exec return value */
#define NODE_EXEC_FINISHED	0
#define NODE_EXEC_SUSPEND	1

/* nodetype->nclass, for add-menu and themes */
#define NODE_CLASS_INPUT			0
#define NODE_CLASS_OUTPUT			1
#define NODE_CLASS_OP_COLOR			3
#define NODE_CLASS_OP_VECTOR		4
#define NODE_CLASS_OP_FILTER		5
#define NODE_CLASS_GROUP			6
#define NODE_CLASS_FILE				7
#define NODE_CLASS_CONVERTOR		8
#define NODE_CLASS_MATTE			9
#define NODE_CLASS_DISTORT			10
#define NODE_CLASS_OP_DYNAMIC		11
#define NODE_CLASS_PATTERN			12
#define NODE_CLASS_TEXTURE			13
#define NODE_CLASS_EXECUTION		14
#define NODE_CLASS_GETDATA			15
#define NODE_CLASS_SETDATA			16
#define NODE_CLASS_MATH				17
#define NODE_CLASS_MATH_VECTOR		18
#define NODE_CLASS_MATH_ROTATION	19
#define NODE_CLASS_PARTICLES		25
#define NODE_CLASS_TRANSFORM		30
#define NODE_CLASS_COMBINE			31
#define NODE_CLASS_SHADER 			40
#define NODE_CLASS_LAYOUT			100

/* nodetype->compatibility */
#define NODE_OLD_SHADING	1
#define NODE_NEW_SHADING	2

/* enum values for input/output */
#define SOCK_IN		1
#define SOCK_OUT	2

struct bNodeTreeExec;

typedef void (*bNodeTreeCallback)(void *calldata, struct ID *owner_id, struct bNodeTree *ntree);
typedef void (*bNodeClassCallback)(void *calldata, int nclass, const char *name);
typedef struct bNodeTreeType
{
	int type;						/* type identifier */
	char idname[64];				/* id name for RNA identification */
	
	ListBase node_types;			/* type definitions */
	
	/* callbacks */
	void (*free_cache)(struct bNodeTree *ntree);
	void (*free_node_cache)(struct bNodeTree *ntree, struct bNode *node);
	void (*foreach_nodetree)(struct Main *main, void *calldata, bNodeTreeCallback func);		/* iteration over all node trees */
	void (*foreach_nodeclass)(struct Scene *scene, void *calldata, bNodeClassCallback func);	/* iteration over all node classes */

	/* calls allowing threaded composite */
	void (*localize)(struct bNodeTree *localtree, struct bNodeTree *ntree);
	void (*local_sync)(struct bNodeTree *localtree, struct bNodeTree *ntree);
	void (*local_merge)(struct bNodeTree *localtree, struct bNodeTree *ntree);

	/* Tree update. Overrides nodetype->updatetreefunc! */
	void (*update)(struct bNodeTree *ntree);
	/* Node update. Overrides nodetype->updatefunc! */
	void (*update_node)(struct bNodeTree *ntree, struct bNode *node);
	
	int (*validate_link)(struct bNodeTree *ntree, struct bNodeLink *link);

	/* Default muting pointers. */
	void (*mutefunc)(void *data, int thread, struct bNode *, void *nodedata, struct bNodeStack **, struct bNodeStack **);
	ListBase (*mutelinksfunc)(struct bNodeTree *, struct bNode *, struct bNodeStack **, struct bNodeStack **,
	                          struct GPUNodeStack *, struct GPUNodeStack *);
	/* gpu */
	int (*gpumutefunc)(struct GPUMaterial *, struct bNode *, void *, struct GPUNodeStack *, struct GPUNodeStack *);
} bNodeTreeType;

/* ************** GENERIC API, TREES *************** */

struct bNodeTreeType *ntreeGetType(int type);
struct bNodeType *ntreeGetNodeType(struct bNodeTree *ntree);
struct bNodeSocketType *ntreeGetSocketType(int type);

struct bNodeTree *ntreeAddTree(const char *name, int type, int nodetype);
void			ntreeInitTypes(struct bNodeTree *ntree);

void			ntreeFreeTree(struct bNodeTree *ntree);
struct bNodeTree *ntreeCopyTree(struct bNodeTree *ntree);
void			ntreeSwitchID(struct bNodeTree *ntree, struct ID *sce_from, struct ID *sce_to);
void			ntreeMakeLocal(struct bNodeTree *ntree);
int				ntreeHasType(struct bNodeTree *ntree, int type);

void			ntreeUpdateTree(struct bNodeTree *ntree);
/* XXX Currently each tree update call does call to ntreeVerifyNodes too.
 * Some day this should be replaced by a decent depsgraph automatism!
 */
void			ntreeVerifyNodes(struct Main *main, struct ID *id);

void			ntreeGetDependencyList(struct bNodeTree *ntree, struct bNode ***deplist, int *totnodes);

/* XXX old trees handle output flags automatically based on special output node types and last active selection.
 * new tree types have a per-output socket flag to indicate the final output to use explicitly.
 */
void			ntreeSetOutput(struct bNodeTree *ntree);
void			ntreeInitPreview(struct bNodeTree *, int xsize, int ysize);
void			ntreeClearPreview(struct bNodeTree *ntree);

void			ntreeFreeCache(struct bNodeTree *ntree);

int				ntreeNodeExists(struct bNodeTree *ntree, struct bNode *testnode);
int				ntreeOutputExists(struct bNode *node, struct bNodeSocket *testsock);
struct bNodeTree *ntreeLocalize(struct bNodeTree *ntree);
void			ntreeLocalSync(struct bNodeTree *localtree, struct bNodeTree *ntree);
void			ntreeLocalMerge(struct bNodeTree *localtree, struct bNodeTree *ntree);

/* ************** GENERIC API, NODES *************** */

struct bNodeSocket *nodeAddSocket(struct bNodeTree *ntree, struct bNode *node, int in_out, const char *name, int type);
struct bNodeSocket *nodeInsertSocket(struct bNodeTree *ntree, struct bNode *node, int in_out, struct bNodeSocket *next_sock, const char *name, int type);
void nodeRemoveSocket(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock);
void nodeRemoveAllSockets(struct bNodeTree *ntree, struct bNode *node);

void			nodeAddToPreview(struct bNode *, float *, int, int, int);

struct bNode	*nodeAddNode(struct bNodeTree *ntree, struct bNodeTemplate *ntemp);
void			nodeUnlinkNode(struct bNodeTree *ntree, struct bNode *node);
void			nodeUniqueName(struct bNodeTree *ntree, struct bNode *node);

void			nodeRegisterType(struct bNodeTreeType *ttype, struct bNodeType *ntype) ;
void			nodeMakeDynamicType(struct bNode *node);
int				nodeDynamicUnlinkText(struct ID *txtid);

void			nodeFreeNode(struct bNodeTree *ntree, struct bNode *node);
struct bNode	*nodeCopyNode(struct bNodeTree *ntree, struct bNode *node);

struct bNodeLink *nodeAddLink(struct bNodeTree *ntree, struct bNode *fromnode, struct bNodeSocket *fromsock, struct bNode *tonode, struct bNodeSocket *tosock);
void			nodeRemLink(struct bNodeTree *ntree, struct bNodeLink *link);
void			nodeRemSocketLinks(struct bNodeTree *ntree, struct bNodeSocket *sock);

void			nodeSpaceCoords(struct bNode *node, float *locx, float *locy);
void			nodeAttachNode(struct bNode *node, struct bNode *parent);
void			nodeDetachNode(struct bNode *node);

struct bNode	*nodeFindNodebyName(struct bNodeTree *ntree, const char *name);
int				nodeFindNode(struct bNodeTree *ntree, struct bNodeSocket *sock, struct bNode **nodep, int *sockindex, int *in_out);

struct bNodeLink *nodeFindLink(struct bNodeTree *ntree, struct bNodeSocket *from, struct bNodeSocket *to);
int				nodeCountSocketLinks(struct bNodeTree *ntree, struct bNodeSocket *sock);

void			nodeSetActive(struct bNodeTree *ntree, struct bNode *node);
struct bNode	*nodeGetActive(struct bNodeTree *ntree);
struct bNode	*nodeGetActiveID(struct bNodeTree *ntree, short idtype);
int				nodeSetActiveID(struct bNodeTree *ntree, short idtype, struct ID *id);
void			nodeClearActiveID(struct bNodeTree *ntree, short idtype);
struct bNode	*nodeGetActiveTexture(struct bNodeTree *ntree);

void			nodeUpdate(struct bNodeTree *ntree, struct bNode *node);
int				nodeUpdateID(struct bNodeTree *ntree, struct ID *id);

void			nodeFreePreview(struct bNode *node);

int				nodeSocketIsHidden(struct bNodeSocket *sock);

/* ************** NODE TYPE ACCESS *************** */

struct bNodeTemplate nodeMakeTemplate(struct bNode *node);
int				nodeValid(struct bNodeTree *ntree, struct bNodeTemplate *ntemp);
const char*		nodeLabel(struct bNode *node);
struct bNodeTree *nodeGroupEditGet(struct bNode *node);
struct bNodeTree *nodeGroupEditSet(struct bNode *node, int edit);
void			nodeGroupEditClear(struct bNode *node);

/* Init a new node type struct with default values and callbacks */
void			node_type_base(struct bNodeTreeType *ttype, struct bNodeType *ntype, int type,
                               const char *name, short nclass, short flag);
void			node_type_socket_templates(struct bNodeType *ntype, struct bNodeSocketTemplate *inputs, struct bNodeSocketTemplate *outputs);
void			node_type_size(struct bNodeType *ntype, int width, int minwidth, int maxwidth);
void			node_type_init(struct bNodeType *ntype, void (*initfunc)(struct bNodeTree *ntree, struct bNode *node, struct bNodeTemplate *ntemp));
void			node_type_valid(struct bNodeType *ntype, int (*validfunc)(struct bNodeTree *ntree, struct bNodeTemplate *ntemp));
void			node_type_storage(struct bNodeType *ntype,
								  const char *storagename,
								  void (*freestoragefunc)(struct bNode *),
								  void (*copystoragefunc)(struct bNode *, struct bNode *));
void			node_type_label(struct bNodeType *ntype, const char *(*labelfunc)(struct bNode *));
void			node_type_template(struct bNodeType *ntype, struct bNodeTemplate (*templatefunc)(struct bNode *));
void			node_type_update(struct bNodeType *ntype,
								 void (*updatefunc)(struct bNodeTree *ntree, struct bNode *node),
								 void (*verifyfunc)(struct bNodeTree *ntree, struct bNode *node, struct ID *id));
void			node_type_tree(struct bNodeType *ntype,
							   void (*inittreefunc)(struct bNodeTree *),
							   void (*updatetreefunc)(struct bNodeTree *));
void			node_type_group_edit(struct bNodeType *ntype,
									 struct bNodeTree *(*group_edit_get)(struct bNode *node),
									 struct bNodeTree *(*group_edit_set)(struct bNode *node, int edit),
									 void (*group_edit_clear)(struct bNode *node));

void			node_type_exec(struct bNodeType *ntype, void (*execfunc)(void *data, struct bNode *, struct bNodeStack **,
                                                                         struct bNodeStack **));
void			node_type_exec_new(struct bNodeType *ntype,
								   void *(*initexecfunc)(struct bNode *node),
								   void (*freeexecfunc)(struct bNode *node, void *nodedata),
								   void (*newexecfunc)(void *data, int thread, struct bNode *, void *nodedata,
								                       struct bNodeStack **, struct bNodeStack **));
void			node_type_mute(struct bNodeType *ntype,
                               void (*mutefunc)(void *data, int thread, struct bNode *, void *nodedata,
                                                struct bNodeStack **, struct bNodeStack **),
                               ListBase (*mutelinksfunc)(struct bNodeTree *, struct bNode *, struct bNodeStack **,
                                                         struct bNodeStack **, struct GPUNodeStack*, struct GPUNodeStack*));
void			node_type_gpu(struct bNodeType *ntype, int (*gpufunc)(struct GPUMaterial *mat, struct bNode *node,
                                                                      struct GPUNodeStack *in, struct GPUNodeStack *out));
void			node_type_gpu_ext(struct bNodeType *ntype, int (*gpuextfunc)(struct GPUMaterial *mat, struct bNode *node,
                                                                             void *nodedata, struct GPUNodeStack *in,
                                                                             struct GPUNodeStack *out));
void			node_type_gpu_mute(struct bNodeType *ntype, int (*gpumutefunc)(struct GPUMaterial *, struct bNode *, void *,
                                                                               struct GPUNodeStack *, struct GPUNodeStack *));
void			node_type_compatibility(struct bNodeType *ntype, short compatibility);

/* ************** COMMON NODES *************** */

#define NODE_GROUP		2
#define NODE_FORLOOP	3
#define NODE_WHILELOOP	4
#define NODE_FRAME		5
#define NODE_GROUP_MENU		10000
#define NODE_DYNAMIC_MENU	20000

/* look up a socket on a group node by the internal group socket */
struct bNodeSocket *node_group_find_input(struct bNode *gnode, struct bNodeSocket *gsock);
struct bNodeSocket *node_group_find_output(struct bNode *gnode, struct bNodeSocket *gsock);

struct bNodeSocket *node_group_add_socket(struct bNodeTree *ngroup, const char *name, int type, int in_out);
struct bNodeSocket *node_group_expose_socket(struct bNodeTree *ngroup, struct bNodeSocket *sock, int in_out);
void node_group_expose_all_sockets(struct bNodeTree *ngroup);
void node_group_remove_socket(struct bNodeTree *ngroup, struct bNodeSocket *gsock, int in_out);

struct bNode	*node_group_make_from_selected(struct bNodeTree *ntree);
int				node_group_ungroup(struct bNodeTree *ntree, struct bNode *gnode);

/* in node_common.c */
void register_node_type_frame(struct bNodeTreeType *ttype);

/* ************** SHADER NODES *************** */

struct ShadeInput;
struct ShadeResult;

/* note: types are needed to restore callbacks, don't change values */
/* range 1 - 100 is reserved for common nodes */
/* using toolbox, we add node groups by assuming the values below don't exceed NODE_GROUP_MENU for now */

#define SH_NODE_OUTPUT		1

#define SH_NODE_MATERIAL	100
#define SH_NODE_RGB			101
#define SH_NODE_VALUE		102
#define SH_NODE_MIX_RGB		103
#define SH_NODE_VALTORGB	104
#define SH_NODE_RGBTOBW		105
#define SH_NODE_TEXTURE		106
#define SH_NODE_NORMAL		107
#define SH_NODE_GEOMETRY	108
#define SH_NODE_MAPPING		109
#define SH_NODE_CURVE_VEC	110
#define SH_NODE_CURVE_RGB	111
#define SH_NODE_CAMERA		114
#define SH_NODE_MATH		115
#define SH_NODE_VECT_MATH	116
#define SH_NODE_SQUEEZE		117
#define SH_NODE_MATERIAL_EXT	118
#define SH_NODE_INVERT		119
#define SH_NODE_SEPRGB		120
#define SH_NODE_COMBRGB		121
#define SH_NODE_HUE_SAT		122
#define NODE_DYNAMIC		123

#define SH_NODE_OUTPUT_MATERIAL			124
#define SH_NODE_OUTPUT_WORLD			125
#define SH_NODE_OUTPUT_LAMP				126
#define SH_NODE_FRESNEL					127
#define SH_NODE_MIX_SHADER				128
#define SH_NODE_ATTRIBUTE				129
#define SH_NODE_BACKGROUND				130
#define SH_NODE_BSDF_ANISOTROPIC		131
#define SH_NODE_BSDF_DIFFUSE			132
#define SH_NODE_BSDF_GLOSSY				133
#define SH_NODE_BSDF_GLASS				134
#define SH_NODE_BSDF_TRANSLUCENT		137
#define SH_NODE_BSDF_TRANSPARENT		138
#define SH_NODE_BSDF_VELVET				139
#define SH_NODE_EMISSION				140
#define SH_NODE_NEW_GEOMETRY			141
#define SH_NODE_LIGHT_PATH				142
#define SH_NODE_TEX_IMAGE				143
#define SH_NODE_TEX_SKY					145
#define SH_NODE_TEX_GRADIENT			146
#define SH_NODE_TEX_VORONOI				147
#define SH_NODE_TEX_MAGIC				148
#define SH_NODE_TEX_WAVE				149
#define SH_NODE_TEX_NOISE				150
#define SH_NODE_TEX_MUSGRAVE			152
#define SH_NODE_TEX_COORD				155
#define SH_NODE_ADD_SHADER				156
#define SH_NODE_TEX_ENVIRONMENT			157
#define SH_NODE_OUTPUT_TEXTURE			158
#define SH_NODE_HOLDOUT					159
#define SH_NODE_LAYER_WEIGHT			160
#define SH_NODE_VOLUME_TRANSPARENT		161
#define SH_NODE_VOLUME_ISOTROPIC		162
#define SH_NODE_GAMMA				163
#define SH_NODE_TEX_CHECKER			164
#define SH_NODE_BRIGHTCONTRAST			165

/* custom defines options for Material node */
#define SH_NODE_MAT_DIFF   1
#define SH_NODE_MAT_SPEC   2
#define SH_NODE_MAT_NEG    4
/* custom defines: states for Script node. These are bit indices */
#define NODE_DYNAMIC_READY	0 /* 1 */
#define NODE_DYNAMIC_LOADED	1 /* 2 */
#define NODE_DYNAMIC_NEW	2 /* 4 */
#define NODE_DYNAMIC_UPDATED	3 /* 8 */
#define NODE_DYNAMIC_ADDEXIST	4 /* 16 */
#define NODE_DYNAMIC_ERROR	5 /* 32 */
#define NODE_DYNAMIC_REPARSE	6 /* 64 */
#define NODE_DYNAMIC_SET	15 /* sign */

/* API */

struct bNodeTreeExec *ntreeShaderBeginExecTree(struct bNodeTree *ntree, int use_tree_data);
void			ntreeShaderEndExecTree(struct bNodeTreeExec *exec, int use_tree_data);
void			ntreeShaderExecTree(struct bNodeTree *ntree, struct ShadeInput *shi, struct ShadeResult *shr);
void			ntreeShaderGetTexcoMode(struct bNodeTree *ntree, int osa, short *texco, int *mode);
void			nodeShaderSynchronizeID(struct bNode *node, int copyto);

				/* switch material render loop */
extern void (*node_shader_lamp_loop)(struct ShadeInput *, struct ShadeResult *);
void			set_node_shader_lamp_loop(void (*lamp_loop_func)(struct ShadeInput *, struct ShadeResult *));

void			ntreeGPUMaterialNodes(struct bNodeTree *ntree, struct GPUMaterial *mat);


/* ************** COMPOSITE NODES *************** */

/* output socket defines */
#define RRES_OUT_IMAGE				0
#define RRES_OUT_ALPHA				1
#define RRES_OUT_Z					2
#define RRES_OUT_NORMAL				3
#define RRES_OUT_UV					4
#define RRES_OUT_VEC				5
#define RRES_OUT_RGBA				6
#define RRES_OUT_DIFF				7
#define RRES_OUT_SPEC				8
#define RRES_OUT_SHADOW				9
#define RRES_OUT_AO					10
#define RRES_OUT_REFLECT			11
#define RRES_OUT_REFRACT			12
#define RRES_OUT_INDIRECT			13
#define RRES_OUT_INDEXOB			14
#define RRES_OUT_INDEXMA			15
#define RRES_OUT_MIST				16
#define RRES_OUT_EMIT				17
#define RRES_OUT_ENV				18
#define RRES_OUT_DIFF_DIRECT		19
#define RRES_OUT_DIFF_INDIRECT		20
#define RRES_OUT_DIFF_COLOR			21
#define RRES_OUT_GLOSSY_DIRECT		22
#define RRES_OUT_GLOSSY_INDIRECT	23
#define RRES_OUT_GLOSSY_COLOR		24
#define RRES_OUT_TRANSM_DIRECT		25
#define RRES_OUT_TRANSM_INDIRECT	26
#define RRES_OUT_TRANSM_COLOR		27

/* note: types are needed to restore callbacks, don't change values */
#define CMP_NODE_VIEWER		201
#define CMP_NODE_RGB		202
#define CMP_NODE_VALUE		203
#define CMP_NODE_MIX_RGB	204
#define CMP_NODE_VALTORGB	205
#define CMP_NODE_RGBTOBW	206
#define CMP_NODE_NORMAL		207
#define CMP_NODE_CURVE_VEC	208
#define CMP_NODE_CURVE_RGB	209
#define CMP_NODE_ALPHAOVER	210
#define CMP_NODE_BLUR		211
#define CMP_NODE_FILTER		212
#define CMP_NODE_MAP_VALUE	213
#define CMP_NODE_TIME		214
#define CMP_NODE_VECBLUR	215
#define CMP_NODE_SEPRGBA	216
#define CMP_NODE_SEPHSVA	217
#define CMP_NODE_SETALPHA	218
#define CMP_NODE_HUE_SAT	219
#define CMP_NODE_IMAGE		220
#define CMP_NODE_R_LAYERS	221
#define CMP_NODE_COMPOSITE	222
#define CMP_NODE_OUTPUT_FILE	223
#define CMP_NODE_TEXTURE	224
#define CMP_NODE_TRANSLATE	225
#define CMP_NODE_ZCOMBINE	226
#define CMP_NODE_COMBRGBA	227
#define CMP_NODE_DILATEERODE	228
#define CMP_NODE_ROTATE		229
#define CMP_NODE_SCALE		230
#define CMP_NODE_SEPYCCA	231
#define CMP_NODE_COMBYCCA	232
#define CMP_NODE_SEPYUVA	233
#define CMP_NODE_COMBYUVA	234
#define CMP_NODE_DIFF_MATTE	235
#define CMP_NODE_COLOR_SPILL	236
#define CMP_NODE_CHROMA_MATTE	237
#define CMP_NODE_CHANNEL_MATTE	238
#define CMP_NODE_FLIP		239
#define CMP_NODE_SPLITVIEWER	240
#define CMP_NODE_INDEX_MASK	241
#define CMP_NODE_MAP_UV		242
#define CMP_NODE_ID_MASK	243
#define CMP_NODE_DEFOCUS	244
#define CMP_NODE_DISPLACE	245
#define CMP_NODE_COMBHSVA	246
#define CMP_NODE_MATH		247
#define CMP_NODE_LUMA_MATTE	248
#define CMP_NODE_BRIGHTCONTRAST 249
#define CMP_NODE_GAMMA		250
#define CMP_NODE_INVERT		251
#define CMP_NODE_NORMALIZE      252
#define CMP_NODE_CROP		253
#define CMP_NODE_DBLUR		254
#define CMP_NODE_BILATERALBLUR  255
#define CMP_NODE_PREMULKEY  256
#define CMP_NODE_DIST_MATTE	257
#define CMP_NODE_VIEW_LEVELS    258
#define CMP_NODE_COLOR_MATTE 259
#define CMP_NODE_COLORBALANCE 260
#define CMP_NODE_HUECORRECT 261
#define CMP_NODE_MOVIECLIP	262
#define CMP_NODE_STABILIZE2D	263
#define CMP_NODE_TRANSFORM	264
#define CMP_NODE_MOVIEDISTORTION	265
#define CMP_NODE_DOUBLEEDGEMASK    266

#define CMP_NODE_GLARE		301
#define CMP_NODE_TONEMAP	302
#define CMP_NODE_LENSDIST	303

/* channel toggles */
#define CMP_CHAN_RGB		1
#define CMP_CHAN_A			2
#define CMP_CHAN_R			4
#define CMP_CHAN_G			8
#define CMP_CHAN_B			16

/* filter types */
#define CMP_FILT_SOFT		0
#define CMP_FILT_SHARP		1
#define CMP_FILT_LAPLACE	2
#define CMP_FILT_SOBEL		3
#define CMP_FILT_PREWITT	4
#define CMP_FILT_KIRSCH		5
#define CMP_FILT_SHADOW		6

/* scale node type, in custom1 */
#define CMP_SCALE_RELATIVE		0
#define CMP_SCALE_ABSOLUTE		1
#define CMP_SCALE_SCENEPERCENT	2
#define CMP_SCALE_RENDERPERCENT 3


/* API */
struct CompBuf;
struct bNodeTreeExec *ntreeCompositBeginExecTree(struct bNodeTree *ntree, int use_tree_data);
void ntreeCompositEndExecTree(struct bNodeTreeExec *exec, int use_tree_data);
void ntreeCompositExecTree(struct bNodeTree *ntree, struct RenderData *rd, int do_previews);
void ntreeCompositTagRender(struct Scene *sce);
int ntreeCompositTagAnimated(struct bNodeTree *ntree);
void ntreeCompositTagGenerators(struct bNodeTree *ntree);
void ntreeCompositForceHidden(struct bNodeTree *ntree, struct Scene *scene);
void ntreeCompositClearTags(struct bNodeTree *ntree);


/* ************** TEXTURE NODES *************** */

struct TexResult;

#define TEX_NODE_OUTPUT     401
#define TEX_NODE_CHECKER    402
#define TEX_NODE_TEXTURE    403
#define TEX_NODE_BRICKS     404
#define TEX_NODE_MATH       405
#define TEX_NODE_MIX_RGB    406
#define TEX_NODE_RGBTOBW    407
#define TEX_NODE_VALTORGB   408
#define TEX_NODE_IMAGE      409
#define TEX_NODE_CURVE_RGB  410
#define TEX_NODE_INVERT     411
#define TEX_NODE_HUE_SAT    412
#define TEX_NODE_CURVE_TIME 413
#define TEX_NODE_ROTATE     414
#define TEX_NODE_VIEWER     415
#define TEX_NODE_TRANSLATE  416
#define TEX_NODE_COORD      417
#define TEX_NODE_DISTANCE   418
#define TEX_NODE_COMPOSE    419
#define TEX_NODE_DECOMPOSE  420
#define TEX_NODE_VALTONOR   421
#define TEX_NODE_SCALE      422
#define TEX_NODE_AT         423

/* 501-599 reserved. Use like this: TEX_NODE_PROC + TEX_CLOUDS, etc */
#define TEX_NODE_PROC      500
#define TEX_NODE_PROC_MAX  600

/* API */
int  ntreeTexTagAnimated(struct bNodeTree *ntree);
void ntreeTexSetPreviewFlag(int);
void ntreeTexCheckCyclics(struct bNodeTree *ntree);
char* ntreeTexOutputMenu(struct bNodeTree *ntree);

struct bNodeTreeExec *ntreeTexBeginExecTree(struct bNodeTree *ntree, int use_tree_data);
void ntreeTexEndExecTree(struct bNodeTreeExec *exec, int use_tree_data);
int ntreeTexExecTree(struct bNodeTree *ntree, struct TexResult *target, float *coord, float *dxt, float *dyt, int osatex, short thread, struct Tex *tex, short which_output, int cfra, int preview, struct ShadeInput *shi, struct MTex *mtex);


/*************************************************/

void init_nodesystem(void);
void free_nodesystem(void);

void clear_scene_in_nodes(struct Main *bmain, struct Scene *sce);

#endif
