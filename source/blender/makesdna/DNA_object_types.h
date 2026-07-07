/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 * \brief Object is a sort of wrapper for general info.
 */

#pragma once

#include "BLI_enum_flags.hh"
#include "BLI_math_constants.h"
#include "BLI_math_matrix_types.hh"

#include "DNA_object_enums.h"

#include "DNA_ID.h"
#include "DNA_action_types.h" /* bAnimVizSettings */
#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_vec_defaults.h"

namespace blender {

namespace bke {
struct ObjectRuntime;
}

struct AnimData;
struct BoundBox;
struct Collection;
struct Curve;
struct Effect;
struct FluidsimSettings;
struct GpencilModifierData;
struct ImageUser;
struct LightgroupMembership;
struct Material;
struct ModifierData;
struct ObHook;
struct Object;
struct PartDeflect;
struct ParticleSystem;
struct Path;
struct RigidBodyOb;
struct ShaderFxData;
struct SoftBody;
struct bGPdata;
struct bFaceMap;

#define MAX_VGROUP_NAME 64

/** #bDeformGroup::flag */
enum {
  DG_LOCK_WEIGHT = 1,
};

/* **************** BASE ********************* */

/** #Base::flag_legacy (also used for #Object::flag). */
enum {
  BA_WAS_SEL = (1 << 1),
  /* NOTE: BA_HAS_RECALC_DATA can be re-used later if freed in `readfile.cc`. */
  // BA_HAS_RECALC_OB = 1 << 2, /* DEPRECATED */
  // BA_HAS_RECALC_DATA = 1 << 3, /* DEPRECATED */
  /** DEPRECATED, was runtime only, but was reusing an older flag. */
  BA_SNAP_FIX_DEPS_FIASCO = (1 << 2),

  /** NOTE: this was used as a proper setting in past, so nullify before using */
  BA_TEMP_TAG = 1 << 5,
  /**
   * Even if this is tagged for transform, this flag means it's being locked in place.
   * Use for #SCE_XFORM_SKIP_CHILDREN.
   */
  BA_TRANSFORM_LOCKED_IN_PLACE = 1 << 7,

  /** Child of a transformed object. */
  BA_TRANSFORM_CHILD = 1 << 8,
  /** Parent of a transformed object. */
  BA_TRANSFORM_PARENT = 1 << 13,

  OB_FROMDUPLI = 1 << 9,
  /** Unknown state, clear before use. */
  OB_DONE = 1 << 10,
  OB_FLAG_USE_SIMULATION_CACHE = 1 << 11,
  /** Used for the clipboard to mark the active object. */
  OB_FLAG_ACTIVE_CLIPBOARD = 1 << 12,
};

/* **************** OBJECT ********************* */

/** #Object.type */
enum ObjectType {
  OB_EMPTY = 0,
  OB_MESH = 1,
  /** Curve object is still used but replaced by "Curves" for the future (see #95355). */
  OB_CURVES_LEGACY = 2,
  OB_SURF = 3,
  OB_FONT = 4,
  OB_MBALL = 5,

  OB_LAMP = 10,
  OB_CAMERA = 11,

  OB_SPEAKER = 12,
  OB_LIGHTPROBE = 13,

  OB_LATTICE = 22,

  OB_ARMATURE = 25,

  OB_GPENCIL_LEGACY = 26,

  OB_CURVES = 27,

  OB_POINTCLOUD = 28,

  OB_VOLUME = 29,

  OB_GREASE_PENCIL = 30,

  /* Keep last. */
  OB_TYPE_MAX,
};

/** #Object.partype: first 4 bits: type. */
enum {
  PARTYPE = (1 << 4) - 1,
  PAROBJECT = 0,
  PARSKEL = 4,
  PARVERT1 = 5,
  PARVERT3 = 6,
  PARBONE = 7,

};

/** #Object.transflag (short) */
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
  /* when calculating vertex parent position, ignore CD_ORIGINDEX layer */
  OB_PARENT_USE_FINAL_INDICES = 1 << 14,

  OB_DUPLI = OB_DUPLIVERTS | OB_DUPLICOLLECTION | OB_DUPLIFACES | OB_DUPLIPARTS,
};

/** #Object.trackflag / #Object.upflag (short) */
enum {
  OB_POSX = 0,
  OB_POSY = 1,
  OB_POSZ = 2,
  OB_NEGX = 3,
  OB_NEGY = 4,
  OB_NEGZ = 5,
};

/** #Object.dtx draw type extra flags (short) */
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

/** #Object.empty_drawtype: no flags */
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

/**
 * Grease-pencil add types.
 * TODO: doesn't need to be DNA, local to `OBJECT_OT_gpencil_add`.
 */
enum {
  GP_EMPTY = 0,
  GP_STROKE = 1,
  GP_MONKEY = 2,
  GREASE_PENCIL_LINEART_SCENE = 3,
  GREASE_PENCIL_LINEART_OBJECT = 4,
  GREASE_PENCIL_LINEART_COLLECTION = 5,
};

/** #Object.boundtype */
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

/** #Object.visibility_flag */
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
  OB_SHADOW_CATCHER = 1 << 10,
  OB_HIDE_PROBE_VOLUME = 1 << 11,
  OB_HIDE_PROBE_CUBEMAP = 1 << 12,
  OB_HIDE_PROBE_PLANAR = 1 << 13,
  OB_HIDE_SURFACE_PICK = 1 << 14,
};

/** #Object.shapeflag */
enum {
  OB_SHAPE_LOCK = 1 << 0,
#ifdef DNA_DEPRECATED_ALLOW
  OB_SHAPE_FLAG_UNUSED_1 = 1 << 1, /* cleared */
#endif
  OB_SHAPE_EDIT_MODE = 1 << 2,
};

/** #Object.nlaflag */
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

/** #Object.protectflag */
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

/** #Object.duplicator_visibility_flag */
enum {
  OB_DUPLI_FLAG_VIEWPORT = 1 << 0,
  OB_DUPLI_FLAG_RENDER = 1 << 1,
};

/** #Object.empty_image_depth */
enum {
  OB_EMPTY_IMAGE_DEPTH_DEFAULT = 0,
  OB_EMPTY_IMAGE_DEPTH_FRONT = 1,
  OB_EMPTY_IMAGE_DEPTH_BACK = 2,
};

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

enum ObjectModifierFlag {
  OB_MODIFIER_FLAG_ADD_REST_POSITION = 1 << 0,
};

/** Vertex Groups - Name Info */
struct bDeformGroup {
  struct bDeformGroup *next = nullptr, *prev = nullptr;
  char name[/*MAX_VGROUP_NAME*/ 64] = "";
  /* need this flag for locking weights */
  char flag = 0, _pad0[7] = {};
};

#ifdef DNA_DEPRECATED_ALLOW
struct bFaceMap {
  struct bFaceMap *next = nullptr, *prev = nullptr;
  char name[/*MAX_VGROUP_NAME*/ 64] = "";
  char flag = 0;
  char _pad0[7] = {};
};
#endif

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
struct BoundBox {
  float vec[8][3] = {};
};

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
  OBJECT_LRT_FORCE_INTERSECTION = (1 << 5),
};
ENUM_OPERATORS(eObjectLineArt_Usage);

enum eObjectLineArt_Flags {
  OBJECT_LRT_OWN_CREASE = (1 << 0),
  OBJECT_LRT_OWN_INTERSECTION_PRIORITY = (1 << 1),
};

struct ObjectLineArt {
  short usage = 0;
  short flags = 0;

  /** if OBJECT_LRT_OWN_CREASE is set */
  float crease_threshold = DEG2RAD(140.0f);

  unsigned char intersection_priority = 0;

  char _pad[7] = {};
};

/* Evaluated light linking state needed for the render engines integration. */
struct LightLinkingRuntime {

  /* For objects that emit light: a bitmask of light sets this emitter is part of for the light
   * linking.
   * A light set is a combination of emitters used by one or more receiver objects.
   *
   * If there is no light linking in the scene or if the emitter does not specify light linking all
   * bits are set.
   *
   * NOTE: There can only be 64 light sets in a scene. */
  uint64_t light_set_membership = 0;

  /* For objects that emit light: a bitmask of light sets this emitter is part of for the shadow
   * linking.
   * A light set is a combination of emitters from which a blocked object does not cast a shadow.
   *
   * If there is no shadow linking in the scene or if the emitter does not specify shadow linking
   * all bits are set.
   *
   * NOTE: There can only be 64 light sets in a scene. */
  uint64_t shadow_set_membership = 0;

  /* For receiver objects: the index of the light set from which this object receives light.
   *
   * If there is no light linking in the scene or the receiver is not linked to any light this is
   * assigned zero. */
  uint8_t receiver_light_set = 0;

  /* For blocker objects: the index of the light set from which this object casts shadow from.
   *
   * If there is no shadow in the scene or the blocker is not linked to any emitter this is
   * assigned zero. */
  uint8_t blocker_shadow_set = 0;

  uint8_t _pad[6] = {};
};

struct LightLinking {
  /* Collections which contains objects (possibly via nested collection indirection) which defines
   * the light linking relation: such as whether objects are included or excluded from being lit by
   * this emitter (receiver_collection), or whether they block light from this emitter
   * (blocker_collection).
   *
   * If the collection is a null pointer then all objects from the current scene are receiving
   * light from this emitter, and nothing is excluded from receiving the light and shadows.
   *
   * The emitter in this context is assumed to be either object of lamp type, or objects with
   * surface which has emissive shader. */
  struct Collection *receiver_collection = nullptr;
  struct Collection *blocker_collection = nullptr;

  LightLinkingRuntime runtime;
};

struct Object {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(Object)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_OB;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt = nullptr;

  short type = OB_EMPTY; /* #ObjectType */
  short partype = 0;
  /** Can be vertex indices. */
  int par1 = 0, par2 = 0, par3 = 0;
  /** String describing sub-object info. */
  char parsubstr[/*MAX_NAME*/ 64] = "";
  struct Object *parent = nullptr, *track = nullptr;
  /* Proxy pointer are deprecated, only kept for conversion to liboverrides. */
  DNA_DEPRECATED struct Object *proxy = nullptr;
  DNA_DEPRECATED struct Object *proxy_group = nullptr;
  DNA_DEPRECATED struct Object *proxy_from = nullptr;
  // struct Path *path = nullptr;
  struct bAction *poselib DNA_DEPRECATED =
      nullptr; /* Pre-Blender 3.0 pose library, deprecated in 3.5. */
  /** Pose data, armature objects only. */
  struct bPose *pose = nullptr;
  /** Pointer to objects data - an 'ID' or NULL. */
  ID *data = nullptr;

  /** Grease Pencil data. */
  struct bGPdata *gpd DNA_DEPRECATED =
      nullptr; /* XXX deprecated... replaced by gpencil object, keep for readfile */

  /** Settings for visualization of object-transform animation. */
  bAnimVizSettings avs;
  /** Motion path cache for this object. */
  bMotionPath *mpath = nullptr;

  ListBaseT<Effect> effect = {nullptr, nullptr}; /* XXX deprecated... keep for readfile */
  ListBaseT<bDeformGroup> defbase = {nullptr,
                                     nullptr}; /* Only for versioning, moved to object data. */
  ListBaseT<bFaceMap> fmaps = {nullptr,
                               nullptr}; /* For versioning, moved to generic attributes. */
  /** List of ModifierData structures. */
  ListBaseT<ModifierData> modifiers = {nullptr, nullptr};
  /** List of GpencilModifierData structures. */
  ListBaseT<GpencilModifierData> greasepencil_modifiers = {nullptr, nullptr};
  /** List of viewport effects. Actually only used by grease pencil. */
  ListBaseT<ShaderFxData> shader_fx = {nullptr, nullptr};

  /** Local object mode. */
  int mode = 0;
  int restore_mode = 0;

  /* materials */
  /** Material slots. */
  struct Material **mat = nullptr;
  /** A boolean field, with each byte 1 if corresponding material is linked to object. */
  char *matbits = nullptr;
  /** Copy of mesh, curve & meta struct member of same name (keep in sync). */
  int totcol = 0;
  /** Currently selected material in the UI (one-based). */
  int actcol = 0;

  /* rot en drot have to be together! (transform('r' en 's')) */
  float loc[3] = {}, dloc[3] = {};
  /** Scale (can be negative). */
  float scale[3] = {1, 1, 1};
  /** DEPRECATED, 2.60 and older only. */
  DNA_DEPRECATED float dsize[3] = {};
  /** Ack!, changing. */
  float dscale[3] = {1, 1, 1};
  /** Euler rotation. */
  float rot[3] = {}, drot[3] = {};
  /** Quaternion rotation. */
  float quat[4] = _DNA_DEFAULT_UNIT_QT;
  float dquat[4] = _DNA_DEFAULT_UNIT_QT;
  /** Axis angle rotation - axis part. */
  float rotAxis[3] = {0, 1, 0}, drotAxis[3] = {0, 1, 0};
  /** Axis angle rotation - angle part. */
  float rotAngle = 0, drotAngle = 0;
  /** Inverse result of parent, so that object doesn't 'stick' to parent. */
  float parentinv[4][4] = _DNA_DEFAULT_UNIT_M4;
  /** Inverse result of constraints.
   * doesn't include effect of parent or object local transform. */
  float constinv[4][4] = _DNA_DEFAULT_UNIT_M4;

  /** Copy of Base's layer in the scene. */
  DNA_DEPRECATED unsigned int lay = 0;

  /** Copy of Base. */
  short flag = OB_FLAG_USE_SIMULATION_CACHE;
  /** Deprecated, use 'matbits'. */
  DNA_DEPRECATED short colbits = 0;

  /** Transformation settings and transform locks. */
  short transflag = 0, protectflag = OB_LOCK_ROT4D;
  short trackflag = 0, upflag = 0;
  /** Used for DopeSheet filtering settings (expanded/collapsed). */
  short nlaflag = 0;

  char _pad1 = {};
  char duplicator_visibility_flag = OB_DUPLI_FLAG_VIEWPORT | OB_DUPLI_FLAG_RENDER;

  /* Depsgraph */
  /** Used by depsgraph, flushed from base. */
  short base_flag = 0;
  /** Used by viewport, synced from base. */
  unsigned short base_local_view_bits = 0;

  /** Collision mask settings */
  unsigned short col_group = 0x01, col_mask = 0xffff;

  /** Rotation mode - uses defines set out in DNA_action_types.h for PoseChannel rotations.... */
  short rotmode = ROT_MODE_EUL;

  /** Bounding box use for drawing. */
  char boundtype = 0;
  /** Bounding box type used for collision. */
  char collision_boundtype = 0;

  /** Viewport draw extra settings. */
  short dtx = 0;
  /** Viewport draw type. */
  char dt = OB_TEXTURE;
  char empty_drawtype = OB_PLAINAXES;
  float empty_drawsize = 1.0;
  /** Dupliface scale. */
  float instance_faces_scale = 1;

  /** Custom index, for render-passes. */
  short index = 0;
  /** Current deformation group, NOTE: index starts at 1. */
  DNA_DEPRECATED unsigned short actdef = 0;
  /** Current face map, NOTE: index starts at 1. */
  char _pad2[4] = {};
  /** Object color (in most cases the material color is used for drawing). */
  float color[4] = {1, 1, 1, 1};

  /** Softbody settings. */
  short softflag = 0;

  /** For restricting view, select, render etc. accessible in outliner. */
  short visibility_flag = 0;

  /** Current shape key for menu or pinned. */
  short shapenr = 0;
  /** Flag for pinning. */
  char shapeflag = 0;

  char _pad3[1] = {};

  /** Object constraints. */
  ListBaseT<bConstraint> constraints = {nullptr, nullptr};
  ListBaseT<struct ObHook> hooks = {nullptr, nullptr};
  /** Particle systems. */
  ListBaseT<struct ParticleSystem> particlesystem = {nullptr, nullptr};

  /** Particle deflector/attractor/collision data. */
  struct PartDeflect *pd = nullptr;
  /** If exists, saved in file. */
  struct SoftBody *soft = nullptr;
  /** Object duplicator for group. */
  struct Collection *instance_collection = nullptr;

  /** If fluidsim enabled, store additional settings. */
  struct FluidsimSettings *fluidsimSettings DNA_DEPRECATED =
      nullptr; /* XXX deprecated... replaced by mantaflow, keep for readfile */

  ListBaseT<LinkData> pc_ids = {nullptr, nullptr};

  /** Settings for Bullet rigid body. */
  struct RigidBodyOb *rigidbody_object = nullptr;
  /** Settings for Bullet constraint. */
  struct RigidBodyCon *rigidbody_constraint = nullptr;

  /** Offset for image empties. */
  float ima_ofs[2] = {-0.5, -0.5};
  /** Must be non-null when object is an empty image. */
  ImageUser *iuser = nullptr;
  char empty_image_visibility_flag = 0;
  char empty_image_depth = OB_EMPTY_IMAGE_DEPTH_DEFAULT;
  char empty_image_flag = 0;

  /** ObjectModifierFlag */
  uint8_t modifier_flag = 0;

  float shadow_terminator_normal_offset = 0;
  float shadow_terminator_geometry_offset = 0.1f;
  float shadow_terminator_shading_offset = 0;

  struct PreviewImage *preview = nullptr;

  ObjectLineArt lineart;

  /** Light-group membership information. */
  struct LightgroupMembership *lightgroup = nullptr;

  /** Light linking information. */
  LightLinking *light_linking = nullptr;

  /** Irradiance caches baked for this object (light-probes only). */
  struct LightProbeObjectCache *lightprobe_cache = nullptr;

  bke::ObjectRuntime *runtime = nullptr;

#ifdef __cplusplus
  const float4x4 &object_to_world() const;
  const float4x4 &world_to_object() const;
#endif
};

/** DEPRECATED: this is not used anymore because hooks are now modifiers. */
struct ObHook {
  struct ObHook *next = nullptr, *prev = nullptr;

  struct Object *parent = nullptr;
  /** Matrix making current transform unmodified. */
  float parentinv[4][4] = {};
  /** Temp matrix while hooking. */
  float mat[4][4] = {};
  /** Visualization of hook. */
  float cent[3] = {};
  /** If not zero, falloff is distance where influence zero. */
  float falloff = 0;

  char name[/*MAX_NAME*/ 64] = "";

  int *indexar = nullptr;
  /** Curindex is cache for fast lookup. */
  int totindex = 0, curindex = 0;
  /** Active is only first hook, for button menu. */
  short type = 0, active = 0;
  float force = 0;
};

/**
 * This is used as a flag for many kinds of data that use selections, examples include:
 * - #BezTriple.f1, #BezTriple.f2, #BezTriple.f3
 * - #bNode.flag
 * - #MovieTrackingTrack.flag
 * And more, ideally this would have a generic location.
 */
#define SELECT 1

/* check if the object type supports materials */
#define OB_TYPE_SUPPORT_MATERIAL(_type) \
  (((_type) >= OB_MESH && (_type) <= OB_MBALL) || \
   ((_type) >= OB_CURVES && (_type) <= OB_GREASE_PENCIL))

/**
 * Does the object have some render-able geometry (unlike empties, cameras, etc.). True for
 * #OB_CURVES_LEGACY, since these often evaluate to objects with geometry.
 */
#define OB_TYPE_IS_GEOMETRY(_type) \
  (ELEM(_type, \
        OB_MESH, \
        OB_SURF, \
        OB_FONT, \
        OB_MBALL, \
        OB_CURVES_LEGACY, \
        OB_CURVES, \
        OB_POINTCLOUD, \
        OB_VOLUME, \
        OB_GREASE_PENCIL))

#define OB_TYPE_SUPPORT_VGROUP(_type) (ELEM(_type, OB_MESH, OB_LATTICE, OB_GREASE_PENCIL))

#define OB_TYPE_SUPPORT_EDITMODE(_type) \
  (ELEM(_type, \
        OB_MESH, \
        OB_FONT, \
        OB_CURVES_LEGACY, \
        OB_SURF, \
        OB_MBALL, \
        OB_LATTICE, \
        OB_ARMATURE, \
        OB_CURVES, \
        OB_POINTCLOUD, \
        OB_GREASE_PENCIL))

#define OB_TYPE_SUPPORT_PARVERT(_type) \
  (ELEM(_type, OB_MESH, OB_SURF, OB_CURVES_LEGACY, OB_LATTICE))

/** Matches #OB_TYPE_SUPPORT_EDITMODE. */
#define OB_DATA_SUPPORT_EDITMODE(_type) \
  (ELEM(_type, ID_ME, ID_CU_LEGACY, ID_MB, ID_LT, ID_AR, ID_CV, ID_GP))

/* is this ID type used as object data */
#define OB_DATA_SUPPORT_ID(_id_type) \
  (ELEM(_id_type, \
        ID_ME, \
        ID_CU_LEGACY, \
        ID_MB, \
        ID_LA, \
        ID_SPK, \
        ID_LP, \
        ID_CA, \
        ID_LT, \
        ID_GD_LEGACY, \
        ID_AR, \
        ID_CV, \
        ID_PT, \
        ID_VO, \
        ID_GP))

#define OB_DATA_SUPPORT_ID_CASE \
  ID_ME: \
  case ID_CU_LEGACY: \
  case ID_MB: \
  case ID_LA: \
  case ID_SPK: \
  case ID_LP: \
  case ID_CA: \
  case ID_LT: \
  case ID_GD_LEGACY: \
  case ID_AR: \
  case ID_CV: \
  case ID_PT: \
  case ID_VO: \
  case ID_GP

}  // namespace blender
