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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_NODE_H
#define BKE_NODE_H

struct bNodeTree;
struct bNode;
struct bNodeLink;
struct bNodeSocket;
struct bNodeStack;
struct uiBlock;
struct rctf;
struct ListBase;
struct RenderData;

#define SOCK_IN		1
#define SOCK_OUT	2

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
	int type;
	char *name;
	float width, minwidth, maxwidth;
	short nclass, flag;
	
	bNodeSocketType *inputs, *outputs;
	
	char storagename[64];			/* struct name for DNA */
	
	void (*execfunc)(void *data, struct bNode *, struct bNodeStack **, struct bNodeStack **);
	
	/* after this line is set on startup of blender */
	int (*butfunc)(struct uiBlock *, struct bNodeTree *, struct bNode *, struct rctf *);

} bNodeType;

/* nodetype->nclass, also for themes */
#define NODE_CLASS_INPUT		0
#define NODE_CLASS_OUTPUT		1
#define NODE_CLASS_GENERATOR	2
#define NODE_CLASS_OPERATOR		3
#define NODE_CLASS_GROUP		4
#define NODE_CLASS_FILE			5

/* ************** GENERIC API, TREES *************** */

void			ntreeVerifyTypes(struct bNodeTree *ntree);

struct bNodeTree *ntreeAddTree(int type);
void			ntreeInitTypes(struct bNodeTree *ntree);
void			ntreeMakeOwnType(struct bNodeTree *ntree);
void			ntreeFreeTree(struct bNodeTree *ntree);
struct bNodeTree *ntreeCopyTree(struct bNodeTree *ntree, int internal_select);

void			ntreeSocketUseFlags(struct bNodeTree *ntree);

void			ntreeSolveOrder(struct bNodeTree *ntree);

void			ntreeBeginExecTree(struct bNodeTree *ntree);
void			ntreeExecTree(struct bNodeTree *ntree, void *callerdata, int thread);
void			ntreeCompositExecTree(struct bNodeTree *ntree, struct RenderData *rd, int do_previews);
void			ntreeEndExecTree(struct bNodeTree *ntree);

void			ntreeInitPreview(struct bNodeTree *, int xsize, int ysize);

/* ************** GENERIC API, NODES *************** */

void			nodeVerifyType(struct bNodeTree *ntree, struct bNode *node);

void			nodeAddToPreview(struct bNode *, float *, int, int);

struct bNode	*nodeAddNodeType(struct bNodeTree *ntree, int type, struct bNodeTree *ngroup);
void			nodeFreeNode(struct bNodeTree *ntree, struct bNode *node);
struct bNode	*nodeCopyNode(struct bNodeTree *ntree, struct bNode *node);

struct bNodeLink *nodeAddLink(struct bNodeTree *ntree, struct bNode *fromnode, struct bNodeSocket *fromsock, struct bNode *tonode, struct bNodeSocket *tosock);
void			nodeRemLink(struct bNodeTree *ntree, struct bNodeLink *link);

struct bNodeLink *nodeFindLink(struct bNodeTree *ntree, struct bNodeSocket *from, struct bNodeSocket *to);
int				nodeCountSocketLinks(struct bNodeTree *ntree, struct bNodeSocket *sock);

void			nodeSetActive(struct bNodeTree *ntree, struct bNode *node);
struct bNode	*nodeGetActive(struct bNodeTree *ntree);
struct bNode	*nodeGetActiveID(struct bNodeTree *ntree, short idtype);
void			nodeClearActiveID(struct bNodeTree *ntree, short idtype);

void			NodeTagChanged(struct bNodeTree *ntree, struct bNode *node);

/* ************** Groups ****************** */

struct bNode	*nodeMakeGroupFromSelected(struct bNodeTree *ntree);
int				nodeGroupUnGroup(struct bNodeTree *ntree, struct bNode *gnode);

void			nodeVerifyGroup(struct bNodeTree *ngroup);
void			nodeGroupSocketUseFlags(struct bNodeTree *ngroup);

/* ************** COMMON NODES *************** */

#define NODE_GROUP		2

extern bNodeType node_group_typeinfo;


/* ************** SHADER NODES *************** */

struct ShadeInput;
struct ShadeResult;

/* note: types are needed to restore callbacks, don't change values */
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

/* custom defines: options for Material node */
#define SH_NODE_MAT_DIFF	1
#define SH_NODE_MAT_SPEC	2
#define SH_NODE_MAT_NEG		4

/* the type definitions array */
extern bNodeType *node_all_shaders[];

/* API */

void			ntreeShaderExecTree(struct bNodeTree *ntree, struct ShadeInput *shi, struct ShadeResult *shr);
int				ntreeShaderGetTexco(struct bNodeTree *ntree, int osa);
void			nodeShaderSynchronizeID(struct bNode *node, int copyto);

				/* switch material render loop */
void			set_node_shader_lamp_loop(void (*lamp_loop_func)(struct ShadeInput *, struct ShadeResult *));

/* ************** COMPOSIT NODES *************** */

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

#define CMP_NODE_IMAGE			220
#define CMP_NODE_R_RESULT		221
#define CMP_NODE_COMPOSITE		222
#define CMP_NODE_OUTPUT_FILE	223


/* filter types */
#define CMP_FILT_SOFT		0
#define CMP_FILT_SHARP		1
#define CMP_FILT_LAPLACE	2
#define CMP_FILT_SOBEL		3
#define CMP_FILT_PREWITT	4
#define CMP_FILT_KIRSCH		5
#define CMP_FILT_SHADOW		6


/* the type definitions array */
extern bNodeType *node_all_composit[];

/* API */
struct CompBuf;
int ntreeCompositNeedsRender(struct bNodeTree *ntree);
void ntreeCompositTagRender(struct bNodeTree *ntree);

void free_compbuf(struct CompBuf *cbuf); /* internal...*/

#endif

