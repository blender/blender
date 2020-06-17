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

/** \file
 * \ingroup DNA
 */

#ifndef __DNA_NODE_TYPES_H__
#define __DNA_NODE_TYPES_H__

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_vec_types.h"

struct AnimData;
struct ID;
struct Image;
struct ListBase;
struct bGPdata;
struct bNodeInstanceHash;
struct bNodeLink;
struct bNodePreview;
struct bNodeTreeExec;
struct bNodeType;
struct uiBlock;

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

/* ns->datatype, shadetree only */
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
  struct bNodeSocket *next, *prev, *new_sock;

  /** User-defined properties. */
  IDProperty *prop;

  /** Unique identifier for mapping. */
  char identifier[64];

  /** MAX_NAME. */
  char name[64];

  /* XXX deprecated, only used for the Image and OutputFile nodes,
   * should be removed at some point.
   */
  /** Custom storage. */
  void *storage;

  short type, flag;
  /** Max. number of links. Read via nodeSocketLinkLimit, because the limit might be defined on the
   * socket type. */
  short limit;
  /** Input/output type. */
  short in_out;
  /** Runtime type information. */
  struct bNodeSocketType *typeinfo;
  /** Runtime type identifier. */
  char idname[64];

  float locx, locy;

  /** Default input value used for unlinked sockets. */
  void *default_value;

  /* execution data */
  /** Local stack index. */
  short stack_index;
  /* XXX deprecated, kept for forward compatibility */
  short stack_type DNA_DEPRECATED;
  char display_shape;
  char _pad[3];

  /** Custom dynamic defined label, MAX_NAME. */
  char label[64];

  /** Cached data from execution. */
  void *cache;

  /* internal data to retrieve relations and groups
   * DEPRECATED, now uses the generic identifier string instead
   */
  /** Group socket identifiers, to find matching pairs after reading files. */
  int own_index DNA_DEPRECATED;
  /* XXX deprecated, only used for restoring old group node links */
  int to_index DNA_DEPRECATED;
  /* XXX deprecated, still forward compatible since verification
   * restores pointer from matching own_index. */
  struct bNodeSocket *groupsock DNA_DEPRECATED;

  /** A link pointer, set in ntreeUpdateTree. */
  struct bNodeLink *link;

  /* XXX deprecated, socket input values are stored in default_value now.
   * kept for forward compatibility */
  /** Custom data for inputs, only UI writes in this. */
  bNodeStack ns DNA_DEPRECATED;
} bNodeSocket;

/* sock->type */
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
  SOCK_EMITTERS = 10,
  SOCK_EVENTS = 11,
  SOCK_FORCES = 12,
  SOCK_CONTROL_FLOW = 13,
} eNodeSocketDatatype;

/* socket shape */
typedef enum eNodeSocketDisplayShape {
  SOCK_DISPLAY_SHAPE_CIRCLE = 0,
  SOCK_DISPLAY_SHAPE_SQUARE = 1,
  SOCK_DISPLAY_SHAPE_DIAMOND = 2,
  SOCK_DISPLAY_SHAPE_CIRCLE_DOT = 3,
  SOCK_DISPLAY_SHAPE_SQUARE_DOT = 4,
  SOCK_DISPLAY_SHAPE_DIAMOND_DOT = 5,
} eNodeSocketDisplayShape;

/* socket side (input/output) */
typedef enum eNodeSocketInOut {
  SOCK_IN = 1 << 0,
  SOCK_OUT = 1 << 1,
} eNodeSocketInOut;

/* sock->flag, first bit is select */
typedef enum eNodeSocketFlag {
  /** hidden is user defined, to hide unused */
  SOCK_HIDDEN = (1 << 1),
  /** for quick check if socket is linked */
  SOCK_IN_USE = (1 << 2),
  /** unavailable is for dynamic sockets */
  SOCK_UNAVAIL = (1 << 3),
  // /** DEPRECATED  dynamic socket (can be modified by user) */
  // SOCK_DYNAMIC = (1 << 4),
  // /** DEPRECATED  group socket should not be exposed */
  // SOCK_INTERNAL = (1 << 5),
  /** socket collapsed in UI */
  SOCK_COLLAPSED = (1 << 6),
  /** hide socket value, if it gets auto default */
  SOCK_HIDE_VALUE = (1 << 7),
  /** socket hidden automatically, to distinguish from manually hidden */
  SOCK_AUTO_HIDDEN__DEPRECATED = (1 << 8),
  SOCK_NO_INTERNAL_LINK = (1 << 9),
  /** Draw socket in a more compact form. */
  SOCK_COMPACT = (1 << 10),
} eNodeSocketFlag;

/* limit data in bNode to what we want to see saved? */
typedef struct bNode {
  struct bNode *next, *prev, *new_node;

  /** User-defined properties. */
  IDProperty *prop;

  /** Runtime type information. */
  struct bNodeType *typeinfo;
  /** Runtime type identifier. */
  char idname[64];

  /** MAX_NAME. */
  char name[64];
  int flag;
  short type;
  char _pad[2];
  /** Both for dependency and sorting. */
  short done, level;
  /** Lasty: check preview render status, menunr: browse ID blocks. */
  short lasty, menunr;
  /** For groupnode, offset in global caller stack. */
  short stack_index;
  /** Number of this node in list, used for UI exec events. */
  short nr;
  /** Custom user-defined color. */
  float color[3];

  ListBase inputs, outputs;
  /** Parent node. */
  struct bNode *parent;
  /** Optional link to libdata. */
  struct ID *id;
  /** Custom data, must be struct, for storage in file. */
  void *storage;
  /** The original node in the tree (for localized tree). */
  struct bNode *original;
  /** List of cached internal links (input to output), for muted nodes and operators. */
  ListBase internal_links;

  /** Root offset for drawing (parent space). */
  float locx, locy;
  /** Node custom width and height. */
  float width, height;
  /** Node width if hidden. */
  float miniwidth;
  /** Additional offset from loc. */
  float offsetx, offsety;
  /** Initial locx for insert offset animation. */
  float anim_init_locx;
  /** Offset that will be added to locx for insert offset animation. */
  float anim_ofsx;

  /** Update flags. */
  int update;

  /** Custom user-defined label, MAX_NAME. */
  char label[64];
  /** To be abused for buttons. */
  short custom1, custom2;
  float custom3, custom4;

  /** Need_exec is set as UI execution event, exec is flag during exec. */
  short need_exec, exec;
  /** Optional extra storage for use in thread (read only then!). */
  void *threaddata;
  /** Entire boundbox (worldspace). */
  rctf totr;
  /** Optional buttons area. */
  rctf butr;
  /** Optional preview area. */
  rctf prvr;
  /**
   * XXX TODO
   * Node totr size depends on the prvr size, which in turn is determined from preview size.
   * In earlier versions bNodePreview was stored directly in nodes, but since now there can be
   * multiple instances using different preview images it is possible that required node size
   * varies between instances. preview_xsize, preview_ysize defines a common reserved size for
   * preview rect for now, could be replaced by more accurate node instance drawing,
   * but that requires removing totr from DNA and replacing all uses with per-instance data.
   */
  /** Reserved size of the preview rect. */
  short preview_xsize, preview_ysize;
  /** Used at runtime when going through the tree. Initialize before use. */
  short tmp_flag;
  /** Used at runtime to tag derivatives branches. EEVEE only. */
  char branch_tag;
  /** Used at runtime when iterating over node branches. */
  char iter_flag;
  /** Runtime during drawing. */
  struct uiBlock *block;

  /**
   * XXX: eevee only, id of screen space reflection layer,
   * needs to be a float to feed GPU_uniform.
   */
  float ssr_id;
  /**
   * XXX: eevee only, id of screen subsurface scatter layer,
   * needs to be a float to feed GPU_uniform.
   */
  float sss_id;
} bNode;

/* node->flag */
#define NODE_SELECT 1
#define NODE_OPTIONS 2
#define NODE_PREVIEW 4
#define NODE_HIDDEN 8
#define NODE_ACTIVE 16
#define NODE_ACTIVE_ID 32
#define NODE_DO_OUTPUT 64
#define __NODE_GROUP_EDIT 128 /* DEPRECATED */
/* free test flag, undefined */
#define NODE_TEST 256
/* node is disabled */
#define NODE_MUTED 512
// #define NODE_CUSTOM_NAME 1024    /* deprecated! */
/* group node types: use const outputs by default */
#define NODE_CONST_OUTPUT (1 << 11)
/* node is always behind others */
#define NODE_BACKGROUND (1 << 12)
/* automatic flag for nodes included in transforms */
#define NODE_TRANSFORM (1 << 13)
/* node is active texture */

/* note: take care with this flag since its possible it gets
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

/* node->update */
/* XXX NODE_UPDATE is a generic update flag. More fine-grained updates
 * might be used in the future, but currently all work the same way.
 */
#define NODE_UPDATE 0xFFFF     /* generic update flag (includes all others) */
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
  char _pad[4];
} bNodeLink;

/* link->flag */
#define NODE_LINKFLAG_HILITE (1 << 0) /* link has been successfully validated */
#define NODE_LINK_VALID (1 << 1)
#define NODE_LINK_TEST (1 << 2) /* free test flag, undefined */

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

  /** Runtime type information. */
  struct bNodeTreeType *typeinfo;
  /** Runtime type identifier. */
  char idname[64];

  /** Runtime RNA type of the group interface. */
  struct StructRNA *interface_type;

  /** Grease pencil data. */
  struct bGPdata *gpd;
  /** Node tree stores own offset for consistent editor view. */
  float view_center[2];

  ListBase nodes, links;

  /** Set init on fileread. */
  int type, init;
  /**
   * Sockets in groups have unique identifiers, adding new sockets always
   * will increase this counter.
   */
  int cur_index;
  int flag;
  /** Update flags. */
  int update;
  /** Flag to prevent re-entrant update calls. */
  short is_updating;
  /** Generic temporary flag for recursion check (DFS/BFS). */
  short done;
  char _pad2[4];

  /** Specific node type this tree is used for. */
  int nodetype DNA_DEPRECATED;

  /** Quality setting when editing. */
  short edit_quality;
  /** Quality setting when rendering. */
  short render_quality;
  /** Tile size for compositor engine. */
  int chunksize;

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

  /** Execution data.
   *
   * XXX It would be preferable to completely move this data out of the underlying node tree,
   * so node tree execution could finally run independent of the tree itself.
   * This would allow node trees to be merely linked by other data (materials, textures, etc.),
   * as ID data is supposed to.
   * Execution data is generated from the tree once at execution start and can then be used
   * as long as necessary, even while the tree is being modified.
   */
  struct bNodeTreeExec *execdata;

  /* callbacks */
  void (*progress)(void *, float progress);
  /** \warning may be called by different threads */
  void (*stats_draw)(void *, const char *str);
  int (*test_break)(void *);
  void (*update_draw)(void *);
  void *tbh, *prh, *sdh, *udh;
} bNodeTree;

/* ntree->type, index */
#define NTREE_CUSTOM -1 /* for dynamically registered custom types */
#define NTREE_SHADER 0
#define NTREE_COMPOSIT 1
#define NTREE_TEXTURE 2
#define NTREE_SIMULATION 3

/* ntree->init, flag */
#define NTREE_TYPE_INIT 1

/* ntree->flag */
#define NTREE_DS_EXPAND (1 << 0)            /* for animation editors */
#define NTREE_COM_OPENCL (1 << 1)           /* use opencl */
#define NTREE_TWO_PASS (1 << 2)             /* two pass */
#define NTREE_COM_GROUPNODE_BUFFER (1 << 3) /* use groupnode buffers */
#define NTREE_VIEWER_BORDER (1 << 4)        /* use a border for viewer nodes */
/* NOTE: DEPRECATED, use (id->tag & LIB_TAG_LOCALIZED) instead. */

/* tree is localized copy, free when deleting node groups */
/* #define NTREE_IS_LOCALIZED           (1 << 5) */

/* ntree->update */
typedef enum eNodeTreeUpdate {
  NTREE_UPDATE = 0xFFFF,             /* generic update flag (includes all others) */
  NTREE_UPDATE_LINKS = (1 << 0),     /* links have been added or removed */
  NTREE_UPDATE_NODES = (1 << 1),     /* nodes or sockets have been added or removed */
  NTREE_UPDATE_GROUP_IN = (1 << 4),  /* group inputs have changed */
  NTREE_UPDATE_GROUP_OUT = (1 << 5), /* group outputs have changed */
  /* group has changed (generic flag including all other group flags) */
  NTREE_UPDATE_GROUP = (NTREE_UPDATE_GROUP_IN | NTREE_UPDATE_GROUP_OUT),
} eNodeTreeUpdate;

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
  char _pad[3];
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

/* data structs, for node->storage */
enum {
  CMP_NODE_MASKTYPE_ADD = 0,
  CMP_NODE_MASKTYPE_SUBTRACT = 1,
  CMP_NODE_MASKTYPE_MULTIPLY = 2,
  CMP_NODE_MASKTYPE_NOT = 3,
};

enum {
  CMP_NODE_DILATEERODE_STEP = 0,
  CMP_NODE_DILATEERODE_DISTANCE_THRESH = 1,
  CMP_NODE_DILATEERODE_DISTANCE = 2,
  CMP_NODE_DILATEERODE_DISTANCE_FEATHER = 3,
};

enum {
  CMP_NODE_INPAINT_SIMPLE = 0,
};

enum {
  /* CMP_NODEFLAG_MASK_AA          = (1 << 0), */ /* DEPRECATED */
  CMP_NODEFLAG_MASK_NO_FEATHER = (1 << 1),
  CMP_NODEFLAG_MASK_MOTION_BLUR = (1 << 2),

  /* we may want multiple aspect options, exposed as an rna enum */
  CMP_NODEFLAG_MASK_FIXED = (1 << 8),
  CMP_NODEFLAG_MASK_FIXED_SCENE = (1 << 9),
};

enum {
  CMP_NODEFLAG_BLUR_VARIABLE_SIZE = (1 << 0),
  CMP_NODEFLAG_BLUR_EXTEND_BOUNDS = (1 << 1),
};

typedef struct NodeFrame {
  short flag;
  short label_size;
} NodeFrame;

/* this one has been replaced with ImageUser, keep it for do_versions() */
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

/* layer info for image node outputs */
typedef struct NodeImageLayer {
  /* index in the Image->layers->passes lists */
  int pass_index DNA_DEPRECATED;
  /* render pass name */
  /** Amount defined in openexr_multi.h. */
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
  char wrap, _pad;
} NodeDBlurData;

typedef struct NodeBilateralBlurData {
  float sigma_color, sigma_space;
  short iter;
  char _pad[2];
} NodeBilateralBlurData;

/* NOTE: Only for do-version code. */
typedef struct NodeHueSat {
  float hue, sat, val;
} NodeHueSat;

typedef struct NodeImageFile {
  /** 1024 = FILE_MAX. */
  char name[1024];
  struct ImageFormatData im_format;
  int sfra, efra;
} NodeImageFile;

/* XXX first struct fields should match NodeImageFile to ensure forward compatibility */
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
  char _pad1[4];
  /** 1024 = FILE_MAX. */
  char path[1024];
  ImageFormatData format;

  /* multilayer output */
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

/* qdn: Defocus blur node */
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

/* qdn: glare node */
typedef struct NodeGlare {
  char quality, type, iter;
  /* XXX angle is only kept for backward/forward compatibility,
   * was used for two different things, see T50736. */
  char angle DNA_DEPRECATED, _pad0, size, star_45, streaks;
  float colmod, mix, threshold, fade;
  float angle_ofs;
  char _pad1[4];
} NodeGlare;

/* qdn: tonemap node */
typedef struct NodeTonemap {
  float key, offset, gamma;
  float f, m, a, c;
  int type;
} NodeTonemap;

/* qdn: lens distortion node */
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

typedef struct NodeDilateErode {
  char falloff;
  char _pad[7];
} NodeDilateErode;

typedef struct NodeMask {
  int size_x, size_y;
} NodeMask;

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
  float sun_elevation;
  float sun_rotation;
  int altitude;
  float air_density;
  float dust_density;
  float ozone_density;
  char sun_disc;
  char _pad[3];
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
  /** Vertex attribute layer for color source, MAX_CUSTOMDATA_LAYER_NAME. */
  char vertex_attribute_name[64];
  /* Used at runtime only by sampling RNA API. */
  PointDensity pd;
  int cached_resolution;
  char _pad2[4];
} NodeShaderTexPointDensity;

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

typedef struct NodeTranslateData {
  char wrap_axis;
  char relative;
  char _pad[6];
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

typedef struct NodeCryptomatte {
  float add[3];
  float remove[3];
  char *matte_id;
  int num_inputs;
  char _pad[4];
} NodeCryptomatte;

typedef struct NodeDenoise {
  char hdr;
  char _pad[7];
} NodeDenoise;

/* script node mode */
#define NODE_SCRIPT_INTERNAL 0
#define NODE_SCRIPT_EXTERNAL 1

/* script node flag */
#define NODE_SCRIPT_AUTO_UPDATE 1

/* ies node mode */
#define NODE_IES_INTERNAL 0
#define NODE_IES_EXTERNAL 1

/* frame node flags */
#define NODE_FRAME_SHRINK 1     /* keep the bounding box minimal */
#define NODE_FRAME_RESIZEABLE 2 /* test flag, if frame can be resized by user */

/* proxy node flags */
#define NODE_PROXY_AUTOTYPE 1 /* automatically change output type based on link */

/* comp channel matte */
#define CMP_NODE_CHANNEL_MATTE_CS_RGB 1
#define CMP_NODE_CHANNEL_MATTE_CS_HSV 2
#define CMP_NODE_CHANNEL_MATTE_CS_YUV 3
#define CMP_NODE_CHANNEL_MATTE_CS_YCC 4

/* glossy distributions */
#define SHD_GLOSSY_BECKMANN 0
#define SHD_GLOSSY_SHARP 1
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

/* Mapping node vector types */
enum {
  NODE_MAPPING_TYPE_POINT = 0,
  NODE_MAPPING_TYPE_TEXTURE = 1,
  NODE_MAPPING_TYPE_VECTOR = 2,
  NODE_MAPPING_TYPE_NORMAL = 3,
};

/* Rotation node vector types */
enum {
  NODE_VECTOR_ROTATE_TYPE_AXIS = 0,
  NODE_VECTOR_ROTATE_TYPE_AXIS_X = 1,
  NODE_VECTOR_ROTATE_TYPE_AXIS_Y = 2,
  NODE_VECTOR_ROTATE_TYPE_AXIS_Z = 3,
  NODE_VECTOR_ROTATE_TYPE_EULER_XYZ = 4,
};

/* math node clamp */
#define SHD_MATH_CLAMP 1

/* Math node operations. */
enum {
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
};

/* Vector Math node operations. */
enum {
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
};

/* Boolean math node operations. */
enum {
  NODE_BOOLEAN_MATH_AND = 0,
  NODE_BOOLEAN_MATH_OR = 1,
  NODE_BOOLEAN_MATH_NOT = 2,
};

/* Float compare node operations. */
enum {
  NODE_FLOAT_COMPARE_LESS_THAN = 0,
  NODE_FLOAT_COMPARE_LESS_EQUAL = 1,
  NODE_FLOAT_COMPARE_GREATER_THAN = 2,
  NODE_FLOAT_COMPARE_GREATER_EQUAL = 3,
  NODE_FLOAT_COMPARE_EQUAL = 4,
  NODE_FLOAT_COMPARE_NOT_EQUAL = 5,
};

/* Clamp node types. */
enum {
  NODE_CLAMP_MINMAX = 0,
  NODE_CLAMP_RANGE = 1,
};

/* Map range node types. */
enum {
  NODE_MAP_RANGE_LINEAR = 0,
  NODE_MAP_RANGE_STEPPED = 1,
  NODE_MAP_RANGE_SMOOTHSTEP = 2,
  NODE_MAP_RANGE_SMOOTHERSTEP = 3,
};

/* mix rgb node flags */
#define SHD_MIXRGB_USE_ALPHA 1
#define SHD_MIXRGB_CLAMP 2

/* subsurface */
enum {
#ifdef DNA_DEPRECATED_ALLOW
  SHD_SUBSURFACE_COMPATIBLE = 0,  // Deprecated
#endif
  SHD_SUBSURFACE_CUBIC = 1,
  SHD_SUBSURFACE_GAUSSIAN = 2,
  SHD_SUBSURFACE_BURLEY = 3,
  SHD_SUBSURFACE_RANDOM_WALK = 4,
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

/* viewer and cmposite output */
#define CMP_NODE_OUTPUT_IGNORE_ALPHA 1

/* Plane track deform node */
enum {
  CMP_NODEFLAG_PLANETRACKDEFORM_MOTION_BLUR = 1,
};

/* Stabilization node */
enum {
  CMP_NODEFLAG_STABILIZE_INVERSE = 1,
};

#define CMP_NODE_PLANETRACKDEFORM_MBLUR_SAMPLES_MAX 64

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

/* Particle Time Step Event node */
typedef enum NodeSimParticleTimeStepEventType {
  NODE_PARTICLE_TIME_STEP_EVENT_BEGIN = 0,
  NODE_PARTICLE_TIME_STEP_EVENT_END = 1,
} NodeSimParticleTimeStepEventType;

/* Simulation Time node */
typedef enum NodeSimInputTimeType {
  NODE_SIM_INPUT_SIMULATION_TIME = 0,
  NODE_SIM_INPUT_SCENE_TIME = 1,
} NodeSimInputTimeType;

#endif
