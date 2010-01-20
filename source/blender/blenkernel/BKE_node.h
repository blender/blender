/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

/* not very important, but the stack solver likes to know a maximum */
#define MAX_SOCKET	64

struct ID;
struct bNodeTree;
struct bNode;
struct bNodeLink;
struct bNodeSocket;
struct bNodeStack;
struct uiLayout;
struct rctf;
struct ListBase;
struct RenderData;
struct Scene;
struct Main;
struct Tex;
struct GPUMaterial;
struct GPUNode;
struct GPUNodeStack;
struct PointerRNA;
struct bContext;

/* ************** NODE TYPE DEFINITIONS ***** */

typedef struct bNodeSocketType {
	int type, limit;
	char *name;
	float val1, val2, val3, val4;	/* default alloc value for inputs */
	float min, max;					/* default range for inputs */
	
	/* after this line is used internal only */
	struct bNodeSocket *sock;		/* used during verify_types */
	struct bNodeSocket *internsock;	/* group nodes, the internal socket counterpart */
	int own_index;					/* verify group nodes */
	
} bNodeSocketType;

typedef struct bNodeType {
	void *next,*prev;
	int type;
	char *name;
	float width, minwidth, maxwidth;
	short nclass, flag;
	
	bNodeSocketType *inputs, *outputs;
	
	char storagename[64];			/* struct name for DNA */
	
	void (*execfunc)(void *data, struct bNode *, struct bNodeStack **, struct bNodeStack **);
	
	/* this line is set on startup of blender */
	void (*uifunc)(struct uiLayout *, struct bContext *C, struct PointerRNA *ptr);

	void (*initfunc)(struct bNode *);
	void (*freestoragefunc)(struct bNode *);
	void (*copystoragefunc)(struct bNode *, struct bNode *);
	
	/* for use with dynamic typedefs */
	ID *id;
	void *pynode; /* holds pointer to python script */
	void *pydict; /* holds pointer to python script dictionary (scope)*/

	/* gpu */
	int (*gpufunc)(struct GPUMaterial *mat, struct bNode *node, struct GPUNodeStack *in, struct GPUNodeStack *out);

} bNodeType;

/* node->exec, now in use for composites (#define for break is same as ready yes) */
#define NODE_PROCESSING	1
#define NODE_READY		2
#define NODE_BREAK		2
#define NODE_FINISHED	4
#define NODE_FREEBUFS	8
#define NODE_SKIPPED	16

/* nodetype->nclass, for add-menu and themes */
#define NODE_CLASS_INPUT		0
#define NODE_CLASS_OUTPUT		1
#define NODE_CLASS_OP_COLOR		3
#define NODE_CLASS_OP_VECTOR	4
#define NODE_CLASS_OP_FILTER	5
#define NODE_CLASS_GROUP		6
#define NODE_CLASS_FILE			7
#define NODE_CLASS_CONVERTOR	8
#define NODE_CLASS_MATTE		9
#define NODE_CLASS_DISTORT		10
#define NODE_CLASS_OP_DYNAMIC	11
#define NODE_CLASS_PATTERN 12
#define NODE_CLASS_TEXTURE 13

/* ************** GENERIC API, TREES *************** */

void			ntreeVerifyTypes(struct bNodeTree *ntree);

struct bNodeTree *ntreeAddTree(int type);
void			ntreeInitTypes(struct bNodeTree *ntree);

void			ntreeMakeOwnType(struct bNodeTree *ntree);
void			ntreeUpdateType(struct bNodeTree *ntree, struct bNodeType *ntype);
void			ntreeFreeTree(struct bNodeTree *ntree);
struct bNodeTree *ntreeCopyTree(struct bNodeTree *ntree, int internal_select);
void			ntreeMakeLocal(struct bNodeTree *ntree);

void			ntreeSocketUseFlags(struct bNodeTree *ntree);

void			ntreeSolveOrder(struct bNodeTree *ntree);

void			ntreeBeginExecTree(struct bNodeTree *ntree);
void			ntreeExecTree(struct bNodeTree *ntree, void *callerdata, int thread);
void			ntreeCompositExecTree(struct bNodeTree *ntree, struct RenderData *rd, int do_previews);
void			ntreeEndExecTree(struct bNodeTree *ntree);

void			ntreeInitPreview(struct bNodeTree *, int xsize, int ysize);
void			ntreeClearPreview(struct bNodeTree *ntree);

void			ntreeFreeCache(struct bNodeTree *ntree);
				
				/* calls allowing threaded composite */
struct bNodeTree *ntreeLocalize(struct bNodeTree *ntree);
void			ntreeLocalSync(struct bNodeTree *localtree, struct bNodeTree *ntree);
void			ntreeLocalMerge(struct bNodeTree *localtree, struct bNodeTree *ntree);

/* ************** GENERIC API, NODES *************** */

void			nodeVerifyType(struct bNodeTree *ntree, struct bNode *node);

void			nodeAddToPreview(struct bNode *, float *, int, int);

void			nodeUnlinkNode(struct bNodeTree *ntree, struct bNode *node);
void			nodeUniqueName(struct bNodeTree *ntree, struct bNode *node);
void			nodeAddSockets(struct bNode *node, struct bNodeType *ntype);
struct bNode	*nodeAddNodeType(struct bNodeTree *ntree, int type, struct bNodeTree *ngroup, struct ID *id);
void			nodeRegisterType(struct ListBase *typelist, const struct bNodeType *ntype) ;
void			nodeUpdateType(struct bNodeTree *ntree, struct bNode* node, struct bNodeType *ntype);
void			nodeMakeDynamicType(struct bNode *node);
int				nodeDynamicUnlinkText(struct ID *txtid);
void			nodeFreeNode(struct bNodeTree *ntree, struct bNode *node);
struct bNode	*nodeCopyNode(struct bNodeTree *ntree, struct bNode *node, int internal);

struct bNodeLink *nodeAddLink(struct bNodeTree *ntree, struct bNode *fromnode, struct bNodeSocket *fromsock, struct bNode *tonode, struct bNodeSocket *tosock);
void			nodeRemLink(struct bNodeTree *ntree, struct bNodeLink *link);
void			nodeRemSocketLinks(struct bNodeTree *ntree, struct bNodeSocket *sock);

struct bNode	*nodeFindNodebyName(struct bNodeTree *ntree, const char *name);
int			nodeFindNode(struct bNodeTree *ntree, struct bNodeSocket *sock, struct bNode **nodep, int *sockindex);

struct bNodeLink *nodeFindLink(struct bNodeTree *ntree, struct bNodeSocket *from, struct bNodeSocket *to);
int				nodeCountSocketLinks(struct bNodeTree *ntree, struct bNodeSocket *sock);

void			nodeSetActive(struct bNodeTree *ntree, struct bNode *node);
struct bNode	*nodeGetActive(struct bNodeTree *ntree);
struct bNode	*nodeGetActiveID(struct bNodeTree *ntree, short idtype);
int				nodeSetActiveID(struct bNodeTree *ntree, short idtype, struct ID *id);
void			nodeClearActiveID(struct bNodeTree *ntree, short idtype);

void			NodeTagChanged(struct bNodeTree *ntree, struct bNode *node);
void			NodeTagIDChanged(struct bNodeTree *ntree, struct ID *id);

/* ************** Groups ****************** */

struct bNode	*nodeMakeGroupFromSelected(struct bNodeTree *ntree);
int				nodeGroupUnGroup(struct bNodeTree *ntree, struct bNode *gnode);

void			nodeVerifyGroup(struct bNodeTree *ngroup);
void			nodeGroupSocketUseFlags(struct bNodeTree *ngroup);

void			nodeCopyGroup(struct bNode *gnode);

/* ************** COMMON NODES *************** */

#define NODE_GROUP		2
#define NODE_GROUP_MENU		1000
#define NODE_DYNAMIC_MENU	4000

extern bNodeType node_group_typeinfo;


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

/* the type definitions array */
extern struct ListBase node_all_shaders;

/* API */

void			ntreeShaderExecTree(struct bNodeTree *ntree, struct ShadeInput *shi, struct ShadeResult *shr);
void			ntreeShaderGetTexcoMode(struct bNodeTree *ntree, int osa, short *texco, int *mode);
void			nodeShaderSynchronizeID(struct bNode *node, int copyto);

				/* switch material render loop */
extern void (*node_shader_lamp_loop)(struct ShadeInput *, struct ShadeResult *);
void			set_node_shader_lamp_loop(void (*lamp_loop_func)(struct ShadeInput *, struct ShadeResult *));

void			ntreeGPUMaterialNodes(struct bNodeTree *ntree, struct GPUMaterial *mat);


/* ************** COMPOSITE NODES *************** */

/* output socket defines */
#define RRES_OUT_IMAGE		0
#define RRES_OUT_ALPHA		1
#define RRES_OUT_Z			2
#define RRES_OUT_NORMAL		3
#define RRES_OUT_UV			4
#define RRES_OUT_VEC		5
#define RRES_OUT_RGBA		6
#define RRES_OUT_DIFF		7
#define RRES_OUT_SPEC		8
#define RRES_OUT_SHADOW		9
#define RRES_OUT_AO			10
#define RRES_OUT_REFLECT	11
#define RRES_OUT_REFRACT	12
#define RRES_OUT_RADIO		13
#define RRES_OUT_INDEXOB	14
#define RRES_OUT_MIST		15

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


/* the type definitions array */
extern struct ListBase node_all_composit;

/* API */
struct CompBuf;
void ntreeCompositTagRender(struct Scene *sce);
int ntreeCompositTagAnimated(struct bNodeTree *ntree);
void ntreeCompositTagGenerators(struct bNodeTree *ntree);
void ntreeCompositForceHidden(struct bNodeTree *ntree, struct Scene *scene);

void free_compbuf(struct CompBuf *cbuf); /* internal...*/


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

extern struct ListBase node_all_textures;

/* API */
int  ntreeTexTagAnimated(struct bNodeTree *ntree);
void ntreeTexSetPreviewFlag(int);
void ntreeTexExecTree(struct bNodeTree *ntree, struct TexResult *target, float *coord, float *dxt, float *dyt, short thread, struct Tex *tex, short which_output, int cfra, int preview);
void ntreeTexCheckCyclics(struct bNodeTree *ntree);
char* ntreeTexOutputMenu(struct bNodeTree *ntree);


/**/

void init_nodesystem(void);
void free_nodesystem(void);

/**/

void clear_scene_in_nodes(struct Main *bmain, struct Scene *sce);

#endif
