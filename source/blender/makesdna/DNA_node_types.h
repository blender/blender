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
 * Contributor(s): Bob Holcomb, Xavier Thomas
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_node_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_NODE_TYPES_H__
#define __DNA_NODE_TYPES_H__

#include "DNA_ID.h"
#include "DNA_vec_types.h"
#include "DNA_listBase.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"

struct ID;
struct ListBase;
struct SpaceNode;
struct bNodeLink;
struct bNodeType;
struct bNodeTreeExec;
struct AnimData;
struct bGPdata;
struct uiBlock;
struct Image;

#define NODE_MAXSTR 64

typedef struct bNodeStack {
	float vec[4];
	float min, max;
	void *data;
	short hasinput;			/* when input has link, tagged before executing */
	short hasoutput;		/* when output is linked, tagged before executing */
	short datatype;			/* type of data pointer */
	short sockettype;		/* type of socket stack comes from, to remap linking different sockets */
	short is_copy;			/* data is a copy of external data (no freeing) */
	short external;			/* data is used by external nodes (no freeing) */
	short pad[2];
} bNodeStack;

/* ns->datatype, shadetree only */
#define NS_OSA_VECTORS		1
#define NS_OSA_VALUES		2

/* node socket/node socket type -b conversion rules */
#define NS_CR_CENTER		0
#define NS_CR_NONE			1
#define NS_CR_FIT_WIDTH		2
#define NS_CR_FIT_HEIGHT	3
#define NS_CR_FIT			4
#define NS_CR_STRETCH		5

typedef struct bNodeSocket {
	struct bNodeSocket *next, *prev, *new_sock;
	
	char name[64];	/* MAX_NAME */
	
	void *storage;				/* custom storage */
	
	short type, flag;
	short limit;				/* max. number of links */
	short pad1;
	
	float locx, locy;
	
	void *default_value;		/* default input value used for unlinked sockets */
	
	/* execution data */
	short stack_index;			/* local stack index */
	/* XXX deprecated, kept for forward compatibility */
	short stack_type  DNA_DEPRECATED;
	int resizemode;				/* compositor resize mode of the socket */
	void *cache;				/* cached data from execution */
	
	/* internal data to retrieve relations and groups */
	int own_index;				/* group socket identifiers, to find matching pairs after reading files */
	/* XXX deprecated, only used for restoring old group node links */
	int to_index  DNA_DEPRECATED;
	struct bNodeSocket *groupsock;
	
	struct bNodeLink *link;		/* a link pointer, set in ntreeUpdateTree */

	/* XXX deprecated, socket input values are stored in default_value now. kept for forward compatibility */
	bNodeStack ns  DNA_DEPRECATED;	/* custom data for inputs, only UI writes in this */
} bNodeSocket;

/* sock->type */
#define SOCK_FLOAT			0
#define SOCK_VECTOR			1
#define SOCK_RGBA			2
#define SOCK_SHADER			3
#define SOCK_BOOLEAN		4
#define SOCK_MESH			5
#define SOCK_INT			6
#define NUM_SOCKET_TYPES	7	/* must be last! */

/* socket side (input/output) */
#define SOCK_IN		1
#define SOCK_OUT	2

/* sock->flag, first bit is select */
	/* hidden is user defined, to hide unused */
#define SOCK_HIDDEN				2
	/* only used now for groups... */
#define SOCK_IN_USE				4	/* XXX deprecated */
	/* unavailable is for dynamic sockets */
#define SOCK_UNAVAIL			8
	/* dynamic socket (can be modified by user) */
#define SOCK_DYNAMIC			16
	/* group socket should not be exposed */
#define SOCK_INTERNAL			32
	/* socket collapsed in UI */
#define SOCK_COLLAPSED			64
	/* hide socket value, if it gets auto default */
#define SOCK_HIDE_VALUE			128
	/* socket hidden automatically, to distinguish from manually hidden */
	/* DEPRECATED, only kept here to avoid reusing the flag */
#define SOCK_AUTO_HIDDEN__DEPRECATED	256

typedef struct bNodePreview {
	unsigned char *rect;
	short xsize, ysize;
	int pad;
} bNodePreview;

/* limit data in bNode to what we want to see saved? */
typedef struct bNode {
	struct bNode *next, *prev, *new_node;
	
	char name[64];	/* MAX_NAME */
	int flag;
	short type, pad2;
	short done, level;		/* both for dependency and sorting */
	short lasty, menunr;	/* lasty: check preview render status, menunr: browse ID blocks */
	short stack_index;		/* for groupnode, offset in global caller stack */
	short nr;				/* number of this node in list, used for UI exec events */
	float color[3];			/* custom user-defined color */
	
	ListBase inputs, outputs;
	struct bNode *parent;	/* parent node */
	struct ID *id;			/* optional link to libdata */
	void *storage;			/* custom data, must be struct, for storage in file */
	struct bNode *original;	/* the original node in the tree (for localized tree) */
	
	float locx, locy;		/* root offset for drawing */
	float width, height;	/* node custom width and height */
	float miniwidth;		/* node width if hidden */
	float offsetx, offsety;	/* additional offset from loc */
	
	int update;				/* update flags */
	
	char label[64];			/* custom user-defined label, MAX_NAME */
	short custom1, custom2;	/* to be abused for buttons */
	float custom3, custom4;

	short need_exec, exec;	/* need_exec is set as UI execution event, exec is flag during exec */
	void *threaddata;		/* optional extra storage for use in thread (read only then!) */
	rctf totr;				/* entire boundbox */
	rctf butr;				/* optional buttons area */
	rctf prvr;				/* optional preview area */
	bNodePreview *preview;	/* optional preview image */
	struct uiBlock *block;	/* runtime during drawing */
	
	struct bNodeType *typeinfo;	/* lookup of callbacks and defaults */
} bNode;

/* node->flag */
#define NODE_SELECT			1
#define NODE_OPTIONS		2
#define NODE_PREVIEW		4
#define NODE_HIDDEN			8
#define NODE_ACTIVE			16
#define NODE_ACTIVE_ID		32
#define NODE_DO_OUTPUT		64
#define NODE_GROUP_EDIT		128
	/* free test flag, undefined */
#define NODE_TEST			256
	/* node is disabled */
#define NODE_MUTED			512
#define NODE_CUSTOM_NAME	1024	/* deprecated! */
	/* group node types: use const outputs by default */
#define NODE_CONST_OUTPUT	(1<<11)
	/* node is always behind others */
#define NODE_BACKGROUND		(1<<12)
	/* automatic flag for nodes included in transforms */
#define NODE_TRANSFORM		(1<<13)
	/* node is active texture */
#define NODE_ACTIVE_TEXTURE	(1<<14)
	/* use a custom color for the node */
#define NODE_CUSTOM_COLOR	(1<<15)

/* node->update */
/* XXX NODE_UPDATE is a generic update flag. More fine-grained updates
 * might be used in the future, but currently all work the same way.
 */
#define NODE_UPDATE			0xFFFF	/* generic update flag (includes all others) */
#define NODE_UPDATE_ID		1		/* associated id data block has changed */

typedef struct bNodeLink {
	struct bNodeLink *next, *prev;
	
	bNode *fromnode, *tonode;
	bNodeSocket *fromsock, *tosock;
	
	int flag;
	int pad;
} bNodeLink;

/* link->flag */
#define NODE_LINKFLAG_HILITE	1		/* link has been successfully validated */
#define NODE_LINK_VALID			2

/* tree->edit_quality/tree->render_quality */
#define NTREE_QUALITY_HIGH    0
#define NTREE_QUALITY_MEDIUM  1
#define NTREE_QUALITY_LOW     2

/* tree->chunksize */
#define NTREE_CHUNCKSIZE_32 32
#define NTREE_CHUNCKSIZE_64 64
#define NTREE_CHUNCKSIZE_128 128
#define NTREE_CHUNCKSIZE_256 256
#define NTREE_CHUNCKSIZE_512 512
#define NTREE_CHUNCKSIZE_1024 1024

/* the basis for a Node tree, all links and nodes reside internal here */
/* only re-usable node trees are in the library though, materials and textures allocate own tree struct */
typedef struct bNodeTree {
	ID id;
	struct AnimData *adt;		/* animation data (must be immediately after id for utilities to use it) */ 
	
	struct bGPdata *gpd;		/* grease pencil data */
	
	ListBase nodes, links;
	
	int type, init;					/* set init on fileread */
	int cur_index;					/* sockets in groups have unique identifiers, adding new sockets always 
									 * will increase this counter */
	int flag;
	int update;						/* update flags */
	
	int nodetype;					/* specific node type this tree is used for */

	short edit_quality;				/* Quality setting when editing */
	short render_quality;				/* Quality setting when rendering */
	int chunksize;					/* tile size for compositor engine */
	
	ListBase inputs, outputs;		/* external sockets for group nodes */
	
	/* execution data */
	/* XXX It would be preferable to completely move this data out of the underlying node tree,
	 * so node tree execution could finally run independent of the tree itself. This would allow node trees
	 * to be merely linked by other data (materials, textures, etc.), as ID data is supposed to.
	 * Execution data is generated from the tree once at execution start and can then be used
	 * as long as necessary, even while the tree is being modified.
	 */
	struct bNodeTreeExec *execdata;
	
	/* callbacks */
	void (*progress)(void *, float progress);
	void (*stats_draw)(void *, char *str);
	int (*test_break)(void *);
	void *tbh, *prh, *sdh;
	
} bNodeTree;

/* ntree->type, index */
#define NTREE_SHADER		0
#define NTREE_COMPOSIT		1
#define NTREE_TEXTURE		2
#define NUM_NTREE_TYPES		3

/* ntree->init, flag */
#define NTREE_TYPE_INIT		1

/* ntree->flag */
#define NTREE_DS_EXPAND		1	/* for animation editors */
#define NTREE_COM_OPENCL	2	/* use opencl */
#define NTREE_TWO_PASS		4	/* two pass */
/* XXX not nice, but needed as a temporary flags
 * for group updates after library linking.
 */
#define NTREE_DO_VERSIONS_GROUP_EXPOSE	1024

/* ntree->update */
#define NTREE_UPDATE			0xFFFF	/* generic update flag (includes all others) */
#define NTREE_UPDATE_LINKS		1		/* links have been added or removed */
#define NTREE_UPDATE_NODES		2		/* nodes or sockets have been added or removed */
#define NTREE_UPDATE_GROUP_IN	16		/* group inputs have changed */
#define NTREE_UPDATE_GROUP_OUT	32		/* group outputs have changed */
#define NTREE_UPDATE_GROUP		48		/* group has changed (generic flag including all other group flags) */


/* socket value structs for input buttons */

typedef struct bNodeSocketValueInt {
	int subtype;				/* RNA subtype */
	int value;
	int min, max;
} bNodeSocketValueInt;

typedef struct bNodeSocketValueFloat {
	int subtype;				/* RNA subtype */
	float value;
	float min, max;
} bNodeSocketValueFloat;

typedef struct bNodeSocketValueBoolean {
	char value;
	char pad[3];
} bNodeSocketValueBoolean;

typedef struct bNodeSocketValueVector {
	int subtype;				/* RNA subtype */
	float value[3];
	float min, max;
} bNodeSocketValueVector;

typedef struct bNodeSocketValueRGBA {
	float value[4];
} bNodeSocketValueRGBA;


/* data structs, for node->storage */
enum {
	CMP_NODE_MASKTYPE_ADD         = 0,
	CMP_NODE_MASKTYPE_SUBTRACT    = 1,
	CMP_NODE_MASKTYPE_MULTIPLY    = 2,
	CMP_NODE_MASKTYPE_NOT         = 3
};

enum {
	CMP_NODE_LENSFLARE_GHOST   = 1,
	CMP_NODE_LENSFLARE_GLOW    = 2,
	CMP_NODE_LENSFLARE_CIRCLE  = 4,
	CMP_NODE_LENSFLARE_STREAKS = 8
};

enum {
	CMP_NODE_DILATEERODE_STEP             = 0,
	CMP_NODE_DILATEERODE_DISTANCE_THRESH  = 1,
	CMP_NODE_DILATEERODE_DISTANCE         = 2,
	CMP_NODE_DILATEERODE_DISTANCE_FEATHER = 3
};

enum {
	CMP_NODEFLAG_MASK_AA         = (1 << 0),
	CMP_NODEFLAG_MASK_NO_FEATHER = (1 << 1)
};

typedef struct NodeFrame {
	short flag;
	short label_size;
} NodeFrame;

/* this one has been replaced with ImageUser, keep it for do_versions() */
typedef struct NodeImageAnim {
	int frames, sfra, nr;
	char cyclic, movie;
	short pad;
} NodeImageAnim;

typedef struct ColorCorrectionData {
	float saturation;
	float contrast;
	float gamma;
	float gain;
	float lift;
	int pad;
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
	int pad;
} NodeBoxMask;

typedef struct NodeEllipseMask {
	float x;
	float y;
	float rotation;
	float height;
	float width;
	int pad;
} NodeEllipseMask;

/* layer info for image node outputs */
typedef struct NodeImageLayer {
	/* index in the Image->layers->passes lists */
	int pass_index;
	/* render pass flag, in case this is an original render pass */
	int pass_flag;
} NodeImageLayer;

typedef struct NodeBlurData {
	short sizex, sizey;
	short samples, maxspeed, minspeed, relative, aspect;
	short curved;
	float fac, percentx, percenty;
	short filtertype;
	char bokeh, gamma;
	int image_in_width, image_in_height; /* needed for absolute/relative conversions */
} NodeBlurData;

typedef struct NodeDBlurData {
	float center_x, center_y, distance, angle, spin, zoom;
	short iter;
	char wrap, pad;
} NodeDBlurData;

typedef struct NodeBilateralBlurData {
	float sigma_color, sigma_space;
	short iter, pad;
} NodeBilateralBlurData;

typedef struct NodeHueSat {
	float hue, sat, val;
} NodeHueSat;

typedef struct NodeImageFile {
	char name[1024]; /* 1024 = FILE_MAX */
	struct ImageFormatData im_format;
	int sfra, efra;
} NodeImageFile;

/* XXX first struct fields should match NodeImageFile to ensure forward compatibility */
typedef struct NodeImageMultiFile {
	char base_path[1024];	/* 1024 = FILE_MAX */
	ImageFormatData format;
	int sfra DNA_DEPRECATED, efra DNA_DEPRECATED;	/* XXX old frame rand values from NodeImageFile for forward compatibility */
	int active_input;		/* selected input in details view list */
	int pad;
} NodeImageMultiFile;
typedef struct NodeImageMultiFileSocket {
	/* single layer file output */
	short use_render_format  DNA_DEPRECATED;
	short use_node_format;	/* use overall node image format */
	int pad1;
	char path[1024];		/* 1024 = FILE_MAX */
	ImageFormatData format;
	
	/* multilayer output */
	char layer[30];		/* EXR_TOT_MAXNAME-2 ('.' and channel char are appended) */
	char pad2[2];
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

typedef struct NodeGeometry {
	char uvname[64];	/* MAX_CUSTOMDATA_LAYER_NAME */
	char colname[64];
} NodeGeometry;

typedef struct NodeVertexCol {
	char name[64];
} NodeVertexCol;

/* qdn: Defocus blur node */
typedef struct NodeDefocus {
	char bktype, pad_c1, preview, gamco;
	short samples, no_zbuf;
	float fstop, maxblur, bthresh, scale;
	float rotation, pad_f1;
} NodeDefocus;

typedef struct NodeScriptDict {
	void *dict; /* for PyObject *dict */
	void *node; /* for BPy_Node *node */
} NodeScriptDict;

/* qdn: glare node */
typedef struct NodeGlare {
	char quality, type, iter;
	char angle, pad_c1, size, pad[2];
	float colmod, mix, threshold, fade;
	float angle_ofs, pad_f1;
} NodeGlare;

/* qdn: tonemap node */
typedef struct NodeTonemap {
	float key, offset, gamma;
	float f, m, a, c;
	int type;
} NodeTonemap;

/* qdn: lens distortion node */
typedef struct NodeLensDist {
	short jit, proj, fit, pad;
} NodeLensDist;

typedef struct NodeColorBalance {
	/* for processing */
	float slope[3];
	float offset[3];
	float power[3];
	
	/* for ui representation */
	float lift[3];
	float gamma[3];
	float gain[3];

	/* temp storage for inverted lift */
	float lift_lgg[3];
	float gamma_inv[3];
} NodeColorBalance;

typedef struct NodeColorspill {
	short limchan, unspill;
	float limscale;
	float uspillr, uspillg, uspillb;
} NodeColorspill;

typedef struct NodeDilateErode {
	char falloff;
	char pad[7];
} NodeDilateErode;

typedef struct NodeTexBase {
	TexMapping tex_mapping;
	ColorMapping color_mapping;
} NodeTexBase;

typedef struct NodeTexSky {
	NodeTexBase base;
	float sun_direction[3];
	float turbidity;
} NodeTexSky;

typedef struct NodeTexImage {
	NodeTexBase base;
	ImageUser iuser;
	int color_space, pad;
} NodeTexImage;

typedef struct NodeTexChecker {
	NodeTexBase base;
} NodeTexChecker;

typedef struct NodeTexEnvironment {
	NodeTexBase base;
	ImageUser iuser;
	int color_space, projection;
} NodeTexEnvironment;

typedef struct NodeTexGradient {
	NodeTexBase base;
	int gradient_type;
	int pad;
} NodeTexGradient;

typedef struct NodeTexNoise {
	NodeTexBase base;
} NodeTexNoise;

typedef struct NodeTexVoronoi {
	NodeTexBase base;
	int coloring;
	int pad;
} NodeTexVoronoi;

typedef struct NodeTexMusgrave {
	NodeTexBase base;
	int musgrave_type;
	int pad;
} NodeTexMusgrave;

typedef struct NodeTexWave {
	NodeTexBase base;
	int wave_type;
	int pad;
} NodeTexWave;

typedef struct NodeTexMagic {
	NodeTexBase base;
	int depth;
	int pad;
} NodeTexMagic;

typedef struct NodeShaderAttribute {
	char name[64];
} NodeShaderAttribute;

/* TEX_output */
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

/* frame node flags */
#define NODE_FRAME_SHRINK		1	/* keep the bounding box minimal */
#define NODE_FRAME_RESIZEABLE	2	/* test flag, if frame can be resized by user */

/* comp channel matte */
#define CMP_NODE_CHANNEL_MATTE_CS_RGB	1
#define CMP_NODE_CHANNEL_MATTE_CS_HSV	2
#define CMP_NODE_CHANNEL_MATTE_CS_YUV	3
#define CMP_NODE_CHANNEL_MATTE_CS_YCC	4

/* glossy distributions */
#define SHD_GLOSSY_BECKMANN	0
#define SHD_GLOSSY_SHARP	1
#define SHD_GLOSSY_GGX		2

/* blend texture */
#define SHD_BLEND_LINEAR			0
#define SHD_BLEND_QUADRATIC			1
#define SHD_BLEND_EASING			2
#define SHD_BLEND_DIAGONAL			3
#define SHD_BLEND_RADIAL			4
#define SHD_BLEND_QUADRATIC_SPHERE	5
#define SHD_BLEND_SPHERICAL			6

/* noise basis for textures */
#define SHD_NOISE_PERLIN			0
#define SHD_NOISE_VORONOI_F1		1
#define SHD_NOISE_VORONOI_F2		2
#define SHD_NOISE_VORONOI_F3		3
#define SHD_NOISE_VORONOI_F4		4
#define SHD_NOISE_VORONOI_F2_F1		5
#define SHD_NOISE_VORONOI_CRACKLE	6
#define SHD_NOISE_CELL_NOISE		7

#define SHD_NOISE_SOFT	0
#define SHD_NOISE_HARD	1

/* voronoi texture */
#define SHD_VORONOI_DISTANCE_SQUARED	0
#define SHD_VORONOI_ACTUAL_DISTANCE		1
#define SHD_VORONOI_MANHATTAN			2
#define SHD_VORONOI_CHEBYCHEV			3
#define SHD_VORONOI_MINKOVSKY_H			4
#define SHD_VORONOI_MINKOVSKY_4			5
#define SHD_VORONOI_MINKOVSKY			6

#define SHD_VORONOI_INTENSITY	0
#define SHD_VORONOI_CELLS		1

/* musgrave texture */
#define SHD_MUSGRAVE_MULTIFRACTAL			0
#define SHD_MUSGRAVE_FBM					1
#define SHD_MUSGRAVE_HYBRID_MULTIFRACTAL	2
#define SHD_MUSGRAVE_RIDGED_MULTIFRACTAL	3
#define SHD_MUSGRAVE_HETERO_TERRAIN			4

/* wave texture */
#define SHD_WAVE_BANDS		0
#define SHD_WAVE_RINGS		1

#define SHD_WAVE_SINE	0
#define SHD_WAVE_SAW	1
#define SHD_WAVE_TRI	2

/* image/environment texture */
#define SHD_COLORSPACE_NONE		0
#define SHD_COLORSPACE_COLOR	1

/* environment texture */
#define SHD_PROJ_EQUIRECTANGULAR	0
#define SHD_PROJ_MIRROR_BALL		1

/* blur node */
#define CMP_NODE_BLUR_ASPECT_NONE		0
#define CMP_NODE_BLUR_ASPECT_Y			1
#define CMP_NODE_BLUR_ASPECT_X			2

#endif
