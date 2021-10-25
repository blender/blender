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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 * \brief Object is a sort of wrapper for general info.
 */

#pragma once

#include "DNA_object_enums.h"

#include "DNA_customdata_types.h"
#include "DNA_defs.h"
#include "DNA_lineart_types.h"
#include "DNA_listBase.h"

#include "DNA_ID.h"
#include "DNA_action_types.h" /* bAnimVizSettings */
#include "DNA_customdata_types.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct BoundBox;
struct Curve;
struct FluidsimSettings;
struct GeometrySet;
struct Ipo;
struct Material;
struct Mesh;
struct Object;
struct PartDeflect;
struct Path;
struct RigidBodyOb;
struct SculptSession;
struct SoftBody;
struct bGPdata;

/* Vertex Groups - Name Info */
typedef struct bDeformGroup {
  struct bDeformGroup *next, *prev;
  /** MAX_VGROUP_NAME. */
  char name[64];
  /* need this flag for locking weights */
  char flag, _pad0[7];
} bDeformGroup;

/* Face Maps. */
typedef struct bFaceMap {
  struct bFaceMap *next, *prev;
  /** MAX_VGROUP_NAME. */
  char name[64];
  char flag;
  char _pad0[7];
} bFaceMap;

#define MAX_VGROUP_NAME 64

/* bDeformGroup->flag */
#define DG_LOCK_WEIGHT 1

/**
 * The following illustrates the orientation of the
 * bounding box in local space
 *
 * <pre>
 *
 * Z  Y
 * | /
 * |/
 * .-----X
 *     2----------6
 *    /|         /|
 *   / |        / |
 *  1----------5  |
 *  |  |       |  |
 *  |  3-------|--7
 *  | /        | /
 *  |/         |/
 *  0----------4
 * </pre>
 */
typedef struct BoundBox {
  float vec[8][3];
  int flag;
  char _pad0[4];
} BoundBox;

/* boundbox flag */
enum {
  BOUNDBOX_DISABLED = (1 << 0),
  BOUNDBOX_DIRTY = (1 << 1),
};

struct CustomData_MeshMasks;

/* Not saved in file! */
typedef struct Object_Runtime {
  /**
   * The custom data layer mask that was last used
   * to calculate data_eval and mesh_deform_eval.
   */
  CustomData_MeshMasks last_data_mask;

  /** Did last modifier stack generation need mapping support? */
  char last_need_mapping;

  char _pad0[3];

  /** Only used for drawing the parent/child help-line. */
  float parent_display_origin[3];

  /**
   * Selection id of this object. It might differ between an evaluated and its original object,
   * when the object is being instanced.
   */
  int select_id;
  char _pad1[3];

  /**
   * Denotes whether the evaluated data is owned by this object or is referenced and owned by
   * somebody else.
   */
  char is_data_eval_owned;

  /** Start time of the mode transfer overlay animation. */
  double overlay_mode_transfer_start_time;

  /** Axis aligned boundbox (in localspace). */
  struct BoundBox *bb;

  /**
   * Original data pointer, before object->data was changed to point
   * to data_eval.
   * Is assigned by dependency graph's copy-on-write evaluation.
   */
  struct ID *data_orig;
  /**
   * Object data structure created during object evaluation. It has all modifiers applied.
   * The type is determined by the type of the original object.
   */
  struct ID *data_eval;

  /**
   * Objects can evaluate to a geometry set instead of a single ID. In those cases, the evaluated
   * geometry set will be stored here. An ID of the correct type is still stored in #data_eval.
   * #geometry_set_eval might reference the ID pointed to by #data_eval as well, but does not own
   * the data.
   */
  struct GeometrySet *geometry_set_eval;

  /**
   * Mesh structure created during object evaluation.
   * It has deformation only modifiers applied on it.
   */
  struct Mesh *mesh_deform_eval;

  /**
   * Original grease pencil bGPdata pointer, before object->data was changed to point
   * to gpd_eval.
   * Is assigned by dependency graph's copy-on-write evaluation.
   */
  struct bGPdata *gpd_orig;
  /**
   * bGPdata structure created during object evaluation.
   * It has all modifiers applied.
   */
  struct bGPdata *gpd_eval;

  /**
   * This is a mesh representation of corresponding object.
   * It created when Python calls `object.to_mesh()`.
   */
  struct Mesh *object_as_temp_mesh;

  /**
   * This is a curve representation of corresponding object.
   * It created when Python calls `object.to_curve()`.
   */
  struct Curve *object_as_temp_curve;

  /** Runtime evaluated curve-specific data, not stored in the file. */
  struct CurveCache *curve_cache;

  unsigned short local_collections_bits;
  short _pad2[3];
} Object_Runtime;

typedef struct ObjectLineArt {
  short usage;
  short flags;

  /** if OBJECT_LRT_OWN_CREASE is set */
  float crease_threshold;
} ObjectLineArt;

/**
 * \warning while the values seem to be flags, they aren't treated as flags.
 */
enum eObjectLineArt_Usage {
  OBJECT_LRT_INHERIT = 0,
  OBJECT_LRT_INCLUDE = (1 << 0),
  OBJECT_LRT_OCCLUSION_ONLY = (1 << 1),
  OBJECT_LRT_EXCLUDE = (1 << 2),
  OBJECT_LRT_INTERSECTION_ONLY = (1 << 3),
  OBJECT_LRT_NO_INTERSECTION = (1 << 4),
};

enum eObjectLineArt_Flags {
  OBJECT_LRT_OWN_CREASE = (1 << 0),
};

typedef struct Object {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;
  /** Runtime (must be immediately after id for utilities to use it). */
  struct DrawDataList drawdata;

  struct SculptSession *sculpt;

  short type, partype;
  /** Can be vertexnrs. */
  int par1, par2, par3;
  /** String describing subobject info, MAX_ID_NAME-2. */
  char parsubstr[64];
  struct Object *parent, *track;
  /* if ob->proxy (or proxy_group), this object is proxy for object ob->proxy */
  /* proxy_from is set in target back to the proxy. */
  struct Object *proxy, *proxy_group, *proxy_from;
  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;
  /* struct Path *path; */
  struct bAction *action DNA_DEPRECATED; /* XXX deprecated... old animation system */
  struct bAction *poselib;
  /** Pose data, armature objects only. */
  struct bPose *pose;
  /** Pointer to objects data - an 'ID' or NULL. */
  void *data;

  /** Grease Pencil data. */
  struct bGPdata *gpd
      DNA_DEPRECATED; /* XXX deprecated... replaced by gpencil object, keep for readfile */

  /** Settings for visualization of object-transform animation. */
  bAnimVizSettings avs;
  /** Motion path cache for this object. */
  bMotionPath *mpath;
  void *_pad0;

  ListBase constraintChannels DNA_DEPRECATED; /* XXX deprecated... old animation system */
  ListBase effect DNA_DEPRECATED;             /* XXX deprecated... keep for readfile */
  ListBase defbase DNA_DEPRECATED;            /* Only for versioning, moved to object data. */
  /** List of ModifierData structures. */
  ListBase modifiers;
  /** List of GpencilModifierData structures. */
  ListBase greasepencil_modifiers;
  /** List of facemaps. */
  ListBase fmaps;
  /** List of viewport effects. Actually only used by grease pencil. */
  ListBase shader_fx;

  /** Local object mode. */
  int mode;
  int restore_mode;

  /* materials */
  /** Material slots. */
  struct Material **mat;
  /** A boolean field, with each byte 1 if corresponding material is linked to object. */
  char *matbits;
  /** Copy of mesh, curve & meta struct member of same name (keep in sync). */
  int totcol;
  /** Currently selected material in the UI. */
  int actcol;

  /* rot en drot have to be together! (transform('r' en 's')) */
  float loc[3], dloc[3];
  /** Scale (can be negative). */
  float scale[3];
  /** DEPRECATED, 2.60 and older only. */
  float dsize[3] DNA_DEPRECATED;
  /** Ack!, changing. */
  float dscale[3];
  /** Euler rotation. */
  float rot[3], drot[3];
  /** Quaternion rotation. */
  float quat[4], dquat[4];
  /** Axis angle rotation - axis part. */
  float rotAxis[3], drotAxis[3];
  /** Axis angle rotation - angle part. */
  float rotAngle, drotAngle;
  /** Final worldspace matrix with constraints & animsys applied. */
  float obmat[4][4];
  /** Inverse result of parent, so that object doesn't 'stick' to parent. */
  float parentinv[4][4];
  /** Inverse result of constraints.
   * doesn't include effect of parent or object local transform. */
  float constinv[4][4];
  /**
   * Inverse matrix of 'obmat' for any other use than rendering!
   *
   * \note this isn't assured to be valid as with 'obmat',
   * before using this value you should do...
   * invert_m4_m4(ob->imat, ob->obmat);
   */
  float imat[4][4];

  /** Copy of Base's layer in the scene. */
  unsigned int lay DNA_DEPRECATED;

  /** Copy of Base. */
  short flag;
  /** Deprecated, use 'matbits'. */
  short colbits DNA_DEPRECATED;

  /** Transformation settings and transform locks. */
  short transflag, protectflag;
  short trackflag, upflag;
  /** Used for DopeSheet filtering settings (expanded/collapsed). */
  short nlaflag;

  char _pad1;
  char duplicator_visibility_flag;

  /* Depsgraph */
  /** Used by depsgraph, flushed from base. */
  short base_flag;
  /** Used by viewport, synced from base. */
  unsigned short base_local_view_bits;

  /** Collision mask settings */
  unsigned short col_group, col_mask;

  /** Rotation mode - uses defines set out in DNA_action_types.h for PoseChannel rotations.... */
  short rotmode;

  /** Bounding box use for drawing. */
  char boundtype;
  /** Bounding box type used for collision. */
  char collision_boundtype;

  /** Viewport draw extra settings. */
  short dtx;
  /** Viewport draw type. */
  char dt;
  char empty_drawtype;
  float empty_drawsize;
  /** Dupliface scale. */
  float instance_faces_scale;

  /** Custom index, for renderpasses. */
  short index;
  /** Current deformation group, NOTE: index starts at 1. */
  unsigned short actdef DNA_DEPRECATED;
  /** Current face map, NOTE: index starts at 1. */
  unsigned short actfmap;
  char _pad2[2];
  /** Object color (in most cases the material color is used for drawing). */
  float color[4];

  /** Softbody settings. */
  short softflag;

  /** For restricting view, select, render etc. accessible in outliner. */
  short visibility_flag;

  /** Current shape key for menu or pinned. */
  short shapenr;
  /** Flag for pinning. */
  char shapeflag;

  char _pad3[1];

  /** Object constraints. */
  ListBase constraints;
  ListBase nlastrips DNA_DEPRECATED; /* XXX deprecated... old animation system */
  ListBase hooks DNA_DEPRECATED;     /* XXX deprecated... old animation system */
  /** Particle systems. */
  ListBase particlesystem;

  /** Particle deflector/attractor/collision data. */
  struct PartDeflect *pd;
  /** If exists, saved in file. */
  struct SoftBody *soft;
  /** Object duplicator for group. */
  struct Collection *instance_collection;

  /** If fluidsim enabled, store additional settings. */
  struct FluidsimSettings *fluidsimSettings
      DNA_DEPRECATED; /* XXX deprecated... replaced by mantaflow, keep for readfile */

  ListBase pc_ids;

  /** Settings for Bullet rigid body. */
  struct RigidBodyOb *rigidbody_object;
  /** Settings for Bullet constraint. */
  struct RigidBodyCon *rigidbody_constraint;

  /** Offset for image empties. */
  float ima_ofs[2];
  /** Must be non-null when object is an empty image. */
  ImageUser *iuser;
  char empty_image_visibility_flag;
  char empty_image_depth;
  char empty_image_flag;
  char _pad8[5];

  struct PreviewImage *preview;

  ObjectLineArt lineart;

  /** Runtime evaluation data (keep last). */
  void *_pad9;
  Object_Runtime runtime;
} Object;

/* Warning, this is not used anymore because hooks are now modifiers */
typedef struct ObHook {
  struct ObHook *next, *prev;

  struct Object *parent;
  /** Matrix making current transform unmodified. */
  float parentinv[4][4];
  /** Temp matrix while hooking. */
  float mat[4][4];
  /** Visualization of hook. */
  float cent[3];
  /** If not zero, falloff is distance where influence zero. */
  float falloff;

  /** MAX_NAME. */
  char name[64];

  int *indexar;
  /** Curindex is cache for fast lookup. */
  int totindex, curindex;
  /** Active is only first hook, for button menu. */
  short type, active;
  float force;
} ObHook;

/* **************** OBJECT ********************* */

/* used many places, should be specialized. */
#define SELECT 1

/* type */
enum {
  OB_EMPTY = 0,
  OB_MESH = 1,
  OB_CURVE = 2,
  OB_SURF = 3,
  OB_FONT = 4,
  OB_MBALL = 5,

  OB_LAMP = 10,
  OB_CAMERA = 11,

  OB_SPEAKER = 12,
  OB_LIGHTPROBE = 13,

  OB_LATTICE = 22,

  OB_ARMATURE = 25,

  /** Grease Pencil object used in 3D view but not used for annotation in 2D. */
  OB_GPENCIL = 26,

  OB_HAIR = 27,

  OB_POINTCLOUD = 28,

  OB_VOLUME = 29,

  /* Keep last. */
  OB_TYPE_MAX,
};

/* check if the object type supports materials */
#define OB_TYPE_SUPPORT_MATERIAL(_type) \
  (((_type) >= OB_MESH && (_type) <= OB_MBALL) || ((_type) >= OB_GPENCIL && (_type) <= OB_VOLUME))
/** Does the object have some render-able geometry (unlike empties, cameras, etc.). */
#define OB_TYPE_IS_GEOMETRY(_type) \
  (ELEM(_type, OB_MESH, OB_SURF, OB_FONT, OB_MBALL, OB_GPENCIL, OB_HAIR, OB_POINTCLOUD, OB_VOLUME))
#define OB_TYPE_SUPPORT_VGROUP(_type) (ELEM(_type, OB_MESH, OB_LATTICE, OB_GPENCIL))
#define OB_TYPE_SUPPORT_EDITMODE(_type) \
  (ELEM(_type, OB_MESH, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL, OB_LATTICE, OB_ARMATURE))
#define OB_TYPE_SUPPORT_PARVERT(_type) (ELEM(_type, OB_MESH, OB_SURF, OB_CURVE, OB_LATTICE))

/** Matches #OB_TYPE_SUPPORT_EDITMODE. */
#define OB_DATA_SUPPORT_EDITMODE(_type) (ELEM(_type, ID_ME, ID_CU, ID_MB, ID_LT, ID_AR))

/* is this ID type used as object data */
#define OB_DATA_SUPPORT_ID(_id_type) \
  (ELEM(_id_type, \
        ID_ME, \
        ID_CU, \
        ID_MB, \
        ID_LA, \
        ID_SPK, \
        ID_LP, \
        ID_CA, \
        ID_LT, \
        ID_GD, \
        ID_AR, \
        ID_HA, \
        ID_PT, \
        ID_VO))

#define OB_DATA_SUPPORT_ID_CASE \
  ID_ME: \
  case ID_CU: \
  case ID_MB: \
  case ID_LA: \
  case ID_SPK: \
  case ID_LP: \
  case ID_CA: \
  case ID_LT: \
  case ID_GD: \
  case ID_AR: \
  case ID_HA: \
  case ID_PT: \
  case ID_VO

/* partype: first 4 bits: type */
enum {
  PARTYPE = (1 << 4) - 1,
  PAROBJECT = 0,
  PARSKEL = 4,
  PARVERT1 = 5,
  PARVERT3 = 6,
  PARBONE = 7,

};

/* (short) transflag */
enum {
  OB_TRANSFORM_ADJUST_ROOT_PARENT_FOR_VIEW_LOCK = 1 << 0,
  OB_TRANSFLAG_UNUSED_1 = 1 << 1, /* cleared */
  OB_NEG_SCALE = 1 << 2,
  OB_TRANSFLAG_UNUSED_3 = 1 << 3, /* cleared */
  OB_DUPLIVERTS = 1 << 4,
  OB_DUPLIROT = 1 << 5,
  OB_TRANSFLAG_UNUSED_6 = 1 << 6, /* cleared */
  /* runtime, calculate derivedmesh for dupli before it's used */
  OB_TRANSFLAG_UNUSED_7 = 1 << 7, /* dirty */
  OB_DUPLICOLLECTION = 1 << 8,
  OB_DUPLIFACES = 1 << 9,
  OB_DUPLIFACES_SCALE = 1 << 10,
  OB_DUPLIPARTS = 1 << 11,
  OB_TRANSFLAG_UNUSED_12 = 1 << 12, /* cleared */
  /* runtime constraints disable */
  OB_NO_CONSTRAINTS = 1 << 13,

  OB_DUPLI = OB_DUPLIVERTS | OB_DUPLICOLLECTION | OB_DUPLIFACES | OB_DUPLIPARTS,
};

/* (short) trackflag / upflag */
enum {
  OB_POSX = 0,
  OB_POSY = 1,
  OB_POSZ = 2,
  OB_NEGX = 3,
  OB_NEGY = 4,
  OB_NEGZ = 5,
};

/* dtx: flags (short) */
enum {
  OB_DRAWBOUNDOX = 1 << 0,
  OB_AXIS = 1 << 1,
  OB_TEXSPACE = 1 << 2,
  OB_DRAWNAME = 1 << 3,
  /* OB_DRAWIMAGE = 1 << 4, */ /* UNUSED */
  /* for solid+wire display */
  OB_DRAWWIRE = 1 << 5,
  /* For overdrawing. */
  OB_DRAW_IN_FRONT = 1 << 6,
  /* Enable transparent draw. */
  OB_DRAWTRANSP = 1 << 7,
  OB_DRAW_ALL_EDGES = 1 << 8, /* only for meshes currently */
  OB_DRAW_NO_SHADOW_CAST = 1 << 9,
  /* Enable lights for grease pencil. */
  OB_USE_GPENCIL_LIGHTS = 1 << 10,
};

/* empty_drawtype: no flags */
enum {
  OB_ARROWS = 1,
  OB_PLAINAXES = 2,
  OB_CIRCLE = 3,
  OB_SINGLE_ARROW = 4,
  OB_CUBE = 5,
  OB_EMPTY_SPHERE = 6,
  OB_EMPTY_CONE = 7,
  OB_EMPTY_IMAGE = 8,
};

/* gpencil add types */
enum {
  GP_EMPTY = 0,
  GP_STROKE = 1,
  GP_MONKEY = 2,
  GP_LRT_SCENE = 3,
  GP_LRT_OBJECT = 4,
  GP_LRT_COLLECTION = 5,
};

/* boundtype */
enum {
  OB_BOUND_BOX = 0,
  OB_BOUND_SPHERE = 1,
  OB_BOUND_CYLINDER = 2,
  OB_BOUND_CONE = 3,
  // OB_BOUND_TRIANGLE_MESH = 4, /* UNUSED */
  // OB_BOUND_CONVEX_HULL = 5,   /* UNUSED */
  // OB_BOUND_DYN_MESH = 6,      /* UNUSED */
  OB_BOUND_CAPSULE = 7,
};

/* **************** BASE ********************* */

/* base->flag_legacy */
enum {
  BA_WAS_SEL = (1 << 1),
  /* NOTE: BA_HAS_RECALC_DATA can be re-used later if freed in readfile.c. */
  // BA_HAS_RECALC_OB = (1 << 2),  /* DEPRECATED */
  // BA_HAS_RECALC_DATA =  (1 << 3),  /* DEPRECATED */
  /** DEPRECATED, was runtime only, but was reusing an older flag. */
  BA_SNAP_FIX_DEPS_FIASCO = (1 << 2),
};

/* NOTE: this was used as a proper setting in past, so nullify before using */
#define BA_TEMP_TAG (1 << 5)

/**
 * Even if this is tagged for transform, this flag means it's being locked in place.
 * Use for #SCE_XFORM_SKIP_CHILDREN.
 */
#define BA_TRANSFORM_LOCKED_IN_PLACE (1 << 7)

#define BA_TRANSFORM_CHILD (1 << 8)   /* child of a transformed object */
#define BA_TRANSFORM_PARENT (1 << 13) /* parent of a transformed object */

#define OB_FROMDUPLI (1 << 9)
#define OB_DONE (1 << 10) /* unknown state, clear before use */
#ifdef DNA_DEPRECATED_ALLOW
#  define OB_FLAG_UNUSED_11 (1 << 11) /* cleared */
#  define OB_FLAG_UNUSED_12 (1 << 12) /* cleared */
#endif

/* ob->visibility_flag */
enum {
  OB_HIDE_VIEWPORT = 1 << 0,
  OB_HIDE_SELECT = 1 << 1,
  OB_HIDE_RENDER = 1 << 2,
  OB_HIDE_CAMERA = 1 << 3,
  OB_HIDE_DIFFUSE = 1 << 4,
  OB_HIDE_GLOSSY = 1 << 5,
  OB_HIDE_TRANSMISSION = 1 << 6,
  OB_HIDE_VOLUME_SCATTER = 1 << 7,
  OB_HIDE_SHADOW = 1 << 8,
  OB_HOLDOUT = 1 << 9,
  OB_SHADOW_CATCHER = 1 << 10
};

/* ob->shapeflag */
enum {
  OB_SHAPE_LOCK = 1 << 0,
#ifdef DNA_DEPRECATED_ALLOW
  OB_SHAPE_FLAG_UNUSED_1 = 1 << 1, /* cleared */
#endif
  OB_SHAPE_EDIT_MODE = 1 << 2,
};

/* ob->nlaflag */
enum {
  OB_ADS_UNUSED_1 = 1 << 0, /* cleared */
  OB_ADS_UNUSED_2 = 1 << 1, /* cleared */
  /* object-channel expanded status */
  OB_ADS_COLLAPSED = 1 << 10,
  /* object's ipo-block */
  /* OB_ADS_SHOWIPO = 1 << 11, */ /* UNUSED */
  /* object's constraint channels */
  /* OB_ADS_SHOWCONS = 1 << 12, */ /* UNUSED */
  /* object's material channels */
  /* OB_ADS_SHOWMATS = 1 << 13, */ /* UNUSED */
  /* object's particle channels */
  /* OB_ADS_SHOWPARTS = 1 << 14, */ /* UNUSED */
};

/* ob->protectflag */
enum {
  OB_LOCK_LOCX = 1 << 0,
  OB_LOCK_LOCY = 1 << 1,
  OB_LOCK_LOCZ = 1 << 2,
  OB_LOCK_LOC = OB_LOCK_LOCX | OB_LOCK_LOCY | OB_LOCK_LOCZ,
  OB_LOCK_ROTX = 1 << 3,
  OB_LOCK_ROTY = 1 << 4,
  OB_LOCK_ROTZ = 1 << 5,
  OB_LOCK_ROT = OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ,
  OB_LOCK_SCALEX = 1 << 6,
  OB_LOCK_SCALEY = 1 << 7,
  OB_LOCK_SCALEZ = 1 << 8,
  OB_LOCK_SCALE = OB_LOCK_SCALEX | OB_LOCK_SCALEY | OB_LOCK_SCALEZ,
  OB_LOCK_ROTW = 1 << 9,
  OB_LOCK_ROT4D = 1 << 10,
};

/* ob->duplicator_visibility_flag */
enum {
  OB_DUPLI_FLAG_VIEWPORT = 1 << 0,
  OB_DUPLI_FLAG_RENDER = 1 << 1,
};

/* ob->empty_image_depth */
#define OB_EMPTY_IMAGE_DEPTH_DEFAULT 0
#define OB_EMPTY_IMAGE_DEPTH_FRONT 1
#define OB_EMPTY_IMAGE_DEPTH_BACK 2

/** #Object.empty_image_visibility_flag */
enum {
  OB_EMPTY_IMAGE_HIDE_PERSPECTIVE = 1 << 0,
  OB_EMPTY_IMAGE_HIDE_ORTHOGRAPHIC = 1 << 1,
  OB_EMPTY_IMAGE_HIDE_BACK = 1 << 2,
  OB_EMPTY_IMAGE_HIDE_FRONT = 1 << 3,
  OB_EMPTY_IMAGE_HIDE_NON_AXIS_ALIGNED = 1 << 4,
};

/** #Object.empty_image_flag */
enum {
  OB_EMPTY_IMAGE_USE_ALPHA_BLEND = 1 << 0,
};

#define MAX_DUPLI_RECUR 8

#ifdef __cplusplus
}
#endif
