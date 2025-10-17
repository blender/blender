/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#pragma once

#include "BLI_enum_flags.hh"
#include "BLI_function_ref.hh"
#include "BLI_math_vector_types.hh"

#include "ED_numinput.hh"
#include "ED_transform.hh"
#include "ED_view3d.hh"

#include "DNA_listBase.h"
#include "DNA_windowmanager_enums.h"

#include "DEG_depsgraph.hh"

/* -------------------------------------------------------------------- */
/** \name Macros/
 * \{ */

#define T_ALL_RESTRICTIONS (T_NO_CONSTRAINT | T_NULL_ONE)
#define T_PROP_EDIT_ALL (T_PROP_EDIT | T_PROP_CONNECTED | T_PROP_PROJECTED)

/* Hard min/max for proportional size. */
#define T_PROP_SIZE_MIN 1e-6f
#define T_PROP_SIZE_MAX 1e12f

#define TRANSFORM_SNAP_MAX_PX 100.0f
#define TRANSFORM_DIST_INVALID -FLT_MAX

#define TRANS_DATA_CONTAINER_FIRST_OK(t) (&(t)->data_container[0])
/* For cases we _know_ there is only one handle. */
#define TRANS_DATA_CONTAINER_FIRST_SINGLE(t) \
  (BLI_assert((t)->data_container_len == 1), (&(t)->data_container[0]))

#define FOREACH_TRANS_DATA_CONTAINER(t, th) \
  for (TransDataContainer *tc = (t)->data_container, \
                          *tc_end = (t)->data_container + (t)->data_container_len; \
       th != tc_end; \
       th++)

#define FOREACH_TRANS_DATA_CONTAINER_INDEX(t, th, i) \
  for (TransDataContainer *tc = ((i = 0), (t)->data_container), \
                          *tc_end = (t)->data_container + (t)->data_container_len; \
       th != tc_end; \
       th++, i++)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Types/
 * \{ */

struct ARegion;
struct bConstraint;
struct Depsgraph;
struct NumInput;
struct Object;
struct RNG;
struct ReportList;
struct Scene;
struct ScrArea;
struct ViewLayer;
struct ViewOpsData;
struct bContext;
struct wmEvent;
struct wmKeyConfig;
struct wmKeyMap;
struct wmMsgBus;
struct wmOperator;
struct wmTimer;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enums and Flags
 * \{ */

namespace blender::ed::transform {

struct TransSnap;
struct TransConvertTypeInfo;
struct TransDataContainer;
struct TransInfo;
struct TransModeInfo;
struct TransSeqSnapData;
struct SnapObjectContext;

/** #TransInfo.options */
enum eTContext {
  CTX_NONE = 0,

  /* These are similar to TransInfo::data_type. */
  CTX_CAMERA = (1 << 0),
  CTX_CURSOR = (1 << 1),
  CTX_EDGE_DATA = (1 << 2),
  CTX_GPENCIL_STROKES = (1 << 3),
  CTX_MASK = (1 << 4),
  CTX_MOVIECLIP = (1 << 5),
  CTX_OBJECT = (1 << 6),
  CTX_PAINT_CURVE = (1 << 7),
  CTX_POSE_BONE = (1 << 8),
  CTX_TEXTURE_SPACE = (1 << 9),
  CTX_SEQUENCER_IMAGE = (1 << 10),

  CTX_NO_PET = (1 << 11),
  CTX_AUTOCONFIRM = (1 << 12),
  /** When transforming object's, adjust the object data so it stays in the same place. */
  CTX_OBMODE_XFORM_OBDATA = (1 << 13),
  /** Transform object parents without moving their children. */
  CTX_OBMODE_XFORM_SKIP_CHILDREN = (1 << 14),
  /** Enable edge scrolling in 2D views. */
  CTX_VIEW2D_EDGE_PAN = (1 << 15),
};
ENUM_OPERATORS(eTContext)

/** #TransInfo.flag */
enum eTFlag {
  /** \note We could remove 'T_EDIT' and use 'obedit_type', for now ensure they're in sync. */
  T_EDIT = 1 << 0,
  /** Transform points, having no rotation/scale. */
  T_POINTS = 1 << 1,
  /** Restrictions flags. */
  T_NO_CONSTRAINT = 1 << 2,
  T_NULL_ONE = 1 << 3,

  T_PROP_EDIT = 1 << 4,
  T_PROP_CONNECTED = 1 << 5,
  T_PROP_PROJECTED = 1 << 6,

  T_V3D_ALIGN = 1 << 7,
  /** For 2D views such as UV or f-curve. */
  T_2D_EDIT = 1 << 8,
  T_CLIP_UV = 1 << 9,

  /** Auto-IK is on. */
  T_AUTOIK = 1 << 10,

  /** Don't use mirror even if the data-block option is set. */
  T_NO_MIRROR = 1 << 11,

  /** To indicate that the value set in the `value` parameter is the final
   * value of the transformation, modified only by the constrain. */
  T_INPUT_IS_VALUES_FINAL = 1 << 12,

  /** To specify if we save back settings at the end. */
  T_MODAL = 1 << 13,

  /** No re-topology (projection). */
  T_NO_PROJECT = 1 << 14,

  T_RELEASE_CONFIRM = 1 << 15,

  /** Alternative transformation. used to add offset to tracking markers. */
  T_ALT_TRANSFORM = 1 << 16,

  /** #TransInfo.center has been set, don't change it. */
  T_OVERRIDE_CENTER = 1 << 17,

  T_MODAL_CURSOR_SET = 1 << 18,

  T_CLNOR_REBUILD = 1 << 19,

  /** Merges unselected into selected after transforming (runs after transforming). */
  T_AUTOMERGE = 1 << 20,
  /** Runs auto-merge & splits. */
  T_AUTOSPLIT = 1 << 21,

  /** Use drag-start position of the event, otherwise use the cursor coordinates (unmodified). */
  T_EVENT_DRAG_START = 1 << 22,

  /** No cursor wrapping on region bounds. */
  T_NO_CURSOR_WRAP = 1 << 23,

  /** Do not display Xform gizmo even though it is available. */
  T_NO_GIZMO = 1 << 24,

  T_DRAW_SNAP_SOURCE = 1 << 25,

  /** Special flag for when the transform code is called after keys have been duplicated. */
  T_DUPLICATED_KEYFRAMES = 1 << 26,

  /** Transform origin. */
  T_ORIGIN = 1 << 27,
};
ENUM_OPERATORS(eTFlag);

/** #TransInfo.modifiers */
enum eTModifier {
  MOD_CONSTRAINT_SELECT_AXIS = 1 << 0,
  MOD_PRECISION = 1 << 1,
  MOD_SNAP = 1 << 2,
  MOD_SNAP_INVERT = 1 << 3,
  MOD_CONSTRAINT_SELECT_PLANE = 1 << 4,
  MOD_NODE_ATTACH = 1 << 5,
  MOD_SNAP_FORCED = 1 << 6,
  MOD_EDIT_SNAP_SOURCE = 1 << 7,
  MOD_NODE_FRAME = 1 << 8,
  MOD_STRIP_CLAMP_HOLDS = 1 << 9,
};
ENUM_OPERATORS(eTModifier)

/** #TransSnap.status */
enum eTSnap {
  SNAP_RESETTED = 0,
  SNAP_SOURCE_FOUND = 1 << 0,
  /* Special flag for snap to grid. */
  SNAP_TARGET_FOUND = 1 << 1,
  SNAP_MULTI_POINTS = 1 << 2,
};
ENUM_OPERATORS(eTSnap)

/** #TransSnap.direction */
enum eSnapDir {
  DIR_GLOBAL_X = (1 << 0),
  DIR_GLOBAL_Y = (1 << 1),
  DIR_GLOBAL_Z = (1 << 2),
};
ENUM_OPERATORS(eSnapDir)

/** #TransCon.mode, #TransInfo.con.mode */
enum eTConstraint {
  /** When set constraints are in use. */
  CON_APPLY = 1 << 0,
  /** These are only used for modal execution. */
  CON_AXIS0 = 1 << 1,
  CON_AXIS1 = 1 << 2,
  CON_AXIS2 = 1 << 3,
  CON_SELECT = 1 << 4,
  CON_USER = 1 << 5,
};
ENUM_OPERATORS(eTConstraint)

/** #TransInfo.state */
enum eTState {
  TRANS_STARTING = 0,
  TRANS_RUNNING = 1,
  TRANS_CONFIRM = 2,
  TRANS_CANCEL = 3,
};

/** #TransInfo.redraw */
enum eRedrawFlag {
  TREDRAW_NOTHING = 0,
  TREDRAW_SOFT = (1 << 0),
  TREDRAW_HARD = (1 << 1) | TREDRAW_SOFT,
};
ENUM_OPERATORS(eRedrawFlag)

/** #TransInfo.helpline */
enum eTHelpline {
  HLP_NONE = 0,
  HLP_SPRING = 1,
  HLP_ANGLE = 2,
  HLP_HARROW = 3,
  HLP_VARROW = 4,
  HLP_CARROW = 5,
  HLP_TRACKBALL = 6,
  HLP_ERROR = 7,
  HLP_ERROR_DASH = 8,
};

enum eTOType {
  O_DEFAULT = 0,
  O_SCENE,
  O_SET,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keymap Modal Items
 *
 * \note these values are saved in key-map files, do not change then but just add new ones.
 * \{ */

enum {
  TFM_MODAL_CANCEL = 1,
  TFM_MODAL_CONFIRM = 2,
  TFM_MODAL_TRANSLATE = 3,
  TFM_MODAL_ROTATE = 4,
  TFM_MODAL_RESIZE = 5,
  TFM_MODAL_SNAP_INV_ON = 6,
  TFM_MODAL_SNAP_INV_OFF = 7,
  TFM_MODAL_SNAP_TOGGLE = 8,
  TFM_MODAL_AXIS_X = 9,
  TFM_MODAL_AXIS_Y = 10,
  TFM_MODAL_AXIS_Z = 11,
  TFM_MODAL_PLANE_X = 12,
  TFM_MODAL_PLANE_Y = 13,
  TFM_MODAL_PLANE_Z = 14,
  TFM_MODAL_CONS_OFF = 15,
  TFM_MODAL_ADD_SNAP = 16,
  TFM_MODAL_REMOVE_SNAP = 17,

  /* 18 and 19 used by number-input, defined in `ED_numinput.hh`. */
  // NUM_MODAL_INCREMENT_UP = 18,
  // NUM_MODAL_INCREMENT_DOWN = 19,

  TFM_MODAL_PROPSIZE_UP = 20,
  TFM_MODAL_PROPSIZE_DOWN = 21,
  TFM_MODAL_AUTOIK_LEN_INC = 22,
  TFM_MODAL_AUTOIK_LEN_DEC = 23,

  TFM_MODAL_NODE_ATTACH_ON = 24,
  TFM_MODAL_NODE_ATTACH_OFF = 25,

  /** For analog input, like trackpad. */
  TFM_MODAL_PROPSIZE = 26,
  /** Node editor insert offset (also called auto-offset) direction toggle. */
  TFM_MODAL_INSERTOFS_TOGGLE_DIR = 27,

  TFM_MODAL_AUTOCONSTRAINT = 28,
  TFM_MODAL_AUTOCONSTRAINTPLANE = 29,

  TFM_MODAL_PRECISION = 30,

  TFM_MODAL_VERT_EDGE_SLIDE = 31,
  TFM_MODAL_TRACKBALL = 32,
  TFM_MODAL_ROTATE_NORMALS = 33,

  TFM_MODAL_EDIT_SNAP_SOURCE_ON = 34,
  TFM_MODAL_EDIT_SNAP_SOURCE_OFF = 35,

  TFM_MODAL_PASSTHROUGH_NAVIGATE = 36,

  TFM_MODAL_NODE_FRAME = 37,

  TFM_MODAL_STRIP_CLAMP = 38,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Data
 * \{ */

/** #TransData.flag */
enum {
  TD_SELECTED = 1 << 0,
  TD_USEQUAT = 1 << 1,
  /* TD_NOTCONNECTED = 1 << 2, */
  /** Used for scaling of #MetaElem.rad. */
  TD_SINGLE_SCALE = 1 << 3,
  /** Scale relative to individual element center. */
  TD_INDIVIDUAL_SCALE = 1 << 4,
  TD_NOCENTER = 1 << 5,
  /** #TransData.ext abused for particle key timing. */
  TD_NO_EXT = 1 << 6,
  /** Don't transform this data. */
  TD_SKIP = 1 << 7,
  /**
   * If this is a bezier triple, we need to restore the handles,
   * if this is set #TransData.hdata needs freeing.
   */
  TD_BEZTRIPLE = 1 << 8,
  /** When this is set, don't apply translation changes to this element. */
  TD_NO_LOC = 1 << 9,
  /** For Graph Editor auto-snap, indicates that point should not undergo auto-snapping. */
  TD_NOTIMESNAP = 1 << 10,
  /**
   * For Graph Editor - curves that can only have int-values
   * need their keyframes tagged with this.
   */
  TD_INTVALUES = 1 << 11,
  /** For edit-mode mirror. */
  TD_MIRROR_X = 1 << 12,
  TD_MIRROR_Y = 1 << 13,
  TD_MIRROR_Z = 1 << 14,
#define TD_MIRROR_EDGE_AXIS_SHIFT 12
  /** For edit-mode mirror, clamp axis to 0. */
  TD_MIRROR_EDGE_X = 1 << 12,
  TD_MIRROR_EDGE_Y = 1 << 13,
  TD_MIRROR_EDGE_Z = 1 << 14,
  /** For F-curve handles, move them along with their keyframes. */
  TD_MOVEHANDLE1 = 1 << 15,
  TD_MOVEHANDLE2 = 1 << 16,
  /**
   * Exceptional case with pose bone rotating when a parent bone has 'Local Location'
   * option enabled and rotating also transforms it.
   */
  TD_PBONE_LOCAL_MTX_P = 1 << 17,
  /** Same as #TD_PBONE_LOCAL_MTX_P but for a child bone. */
  TD_PBONE_LOCAL_MTX_C = 1 << 18,
  /* Grease pencil layer frames. */
  TD_GREASE_PENCIL_FRAME = 1 << 19,
};

struct TransDataBasic {
  /** Extra data (mirrored element pointer, in edit-mode mesh to #BMVert) \
   * (edit-bone for roll fixing) (...). */
  void *extra;
  /** Location of the data to transform. */
  float *loc;
  /** Initial location. */
  float iloc[3];
  /** Individual data center. */
  float center[3];
  /** Value pointer for special transforms. */
  float *val;
  /** Old value. */
  float ival;
  /** Various flags. */
  int flag;
};

struct TransDataMirror : public TransDataBasic {
  /** Location of the data to transform. */
  float *loc_src;
};

/** For objects, poses. 1 single allocation per #TransInfo! */
struct TransDataExtension {
  /** Initial object drot. */
  float drot[3];
#if 0 /* TODO: not yet implemented. */
  /* Initial object `drotAngle`. */
  float drotAngle;
  /* Initial object `drotAxis`. */
  float drotAxis[3];
#endif
  /** Initial object delta quat. */
  float dquat[4];
  /** Initial object delta scale. */
  float dscale[3];
  /** Rotation of the data to transform. */
  float *rot;
  /** Initial rotation. */
  float irot[3];
  /** Rotation quaternion of the data to transform. */
  float *quat;
  /** Initial rotation quaternion. */
  float iquat[4];
  /** Rotation angle of the data to transform. */
  float *rotAngle;
  /** Initial rotation angle. */
  float irotAngle;
  /** Rotation axis of the data to transform. */
  float *rotAxis;
  /** Initial rotation axis. */
  float irotAxis[4];
  /**
   * Scale of the data to transform.
   * Note that in some cases this is used for "size" (meta-balls & texture-space for example).
   */
  float *scale;
  /** Initial scale / size. */
  float iscale[3];
  /** Object matrix. */
  float obmat[4][4];
  /** Use for #V3D_ORIENT_GIMBAL orientation. */
  float axismtx_gimbal[3][3];
  /** Use instead of #TransData.smtx,
   * It is the same but without the #Bone.bone_mat, see #TD_PBONE_LOCAL_MTX_C. */
  float l_smtx[3][3];
  /**
   * The rotation & scale matrix of pose bone, to allow using snap-align in translation mode,
   * when #TransData.mtx is the location pose bone matrix (and hence can't be used to apply
   * rotation in some cases, namely when a bone is in "No-Local" or "Hinge" mode).
   */
  float r_mtx[3][3];
  /** Inverse of previous one. */
  float r_smtx[3][3];
  /** Rotation mode, as defined in #eRotationModes (DNA_action_types.h). */
  int rotOrder;
  /** Original object transformation used for rigid bodies. */
  float oloc[3], orot[3], oquat[4], orotAxis[3], orotAngle;

  /**
   * Use when #TransDataBasic::center has been overridden but the real center is still needed
   * for internal calculations.
   */
  float center_no_override[3];
};

struct TransData2D {
  /** Location of data used to transform (x,y,0). */
  float loc[3];
  union {
    /** Pointer to real 2d location of data. */
    float *loc2d;
    int *loc2d_i;
  };
  /** Pointer to handle locations, if handles aren't being moved independently. */
  float *h1, *h2;
  float ih1[2], ih2[2];
};

/**
 * Used to store 2 handles for each #TransData in case the other handle wasn't selected.
 * Also to unset temporary flags.
 */
struct TransDataCurveHandleFlags {
  uint8_t ih1, ih2;
  uint8_t *h1, *h2;
};

struct TransData : public TransDataBasic {
  /** Distance needed to affect element (for Proportional Editing). */
  float dist;
  /** Distance to the nearest element (for Proportional Editing). */
  float rdist;
  /** Factor of the transformation (for Proportional Editing). */
  float factor;
  /** Transformation matrix from data space to global space. */
  float mtx[3][3];
  /** Transformation matrix from global space to data space. */
  float smtx[3][3];
  /** Axis orientation matrix of the data. */
  float axismtx[3][3];
  /** For objects/bones, the first constraint in its constraint stack. */
  bConstraint *con;
  /** For curves, stores handle flags for modification/cancel. */
  TransDataCurveHandleFlags *hdata;
  /** If set, copy of Object or #bPoseChannel protection. */
  short protectflag;
};

/* -------------------------------------------------------------------- */
/** \name Transform Types
 * \{ */

struct TransSnapPoint {
  TransSnapPoint *next, *prev;
  float co[3];
};

struct TransSnap {
  /* Snapping options stored as flags. */
  eSnapFlag flag;
  /* Method(s) used for snapping source to target. */
  eSnapMode mode;
  /* Part of source to snap to target. */
  eSnapSourceOP source_operation;
  /* Determines which objects are possible target. */
  eSnapTargetOP target_operation;
  short face_nearest_steps;
  eTSnap status;
  /* Snapped Element Type (currently for objects only). */
  eSnapMode source_type;
  eSnapMode target_type;
  /* For independent snapping in different directions (currently used only by VSE preview). */
  eSnapDir direction;
  /** Snapping from this point (in global-space). */
  float snap_source[3];
  /** To this point (in global-space). */
  float snap_target[3];
  float snapNormal[3];
  ListBase points;
  TransSnapPoint *selectedPoint;
  double last;
  void (*snap_target_fn)(TransInfo *, float *);
  void (*snap_source_fn)(TransInfo *);

  /**
   * Re-usable snap context data.
   */
  union {
    SnapObjectContext *object_context;
    TransSeqSnapData *seq_context;
  };
};

struct TransCon {
  /** Description of the constraint for header_print. */
  char text[50];
  /** Projection constraint matrix (same as #imtx with some axis == 0). */
  float pmtx[3][3];
  /** Mode flags of the constraint. */
  eTConstraint mode;
  void (*drawExtra)(TransInfo *t);

  /* NOTE: if 'tc' is NULL, 'td' must also be NULL.
   * For constraints that needs to draw differently from the other
   * uses this instead of the generic draw function. */

  /** Apply function pointer for linear vectorial transformation
   * The last three parameters are pointers to the in/out/printable vectors. */
  void (*applyVec)(const TransInfo *t,
                   const TransDataContainer *tc,
                   const TransData *td,
                   const float in[3],
                   float r_out[3]);
  /** Apply function pointer for size transformation. */
  void (*applySize)(const TransInfo *t,
                    const TransDataContainer *tc,
                    const TransData *td,
                    float r_smat[3][3]);
  /** Apply function pointer for rotation transformation. */
  void (*applyRot)(const TransInfo *t,
                   const TransDataContainer *tc,
                   const TransData *td,
                   float r_axis[3]);
};

struct MouseInput {
  void (*apply)(TransInfo *t, MouseInput *mi, const double mval[2], float output[3]);
  void (*post)(TransInfo *t, float values[3]);

  /** Initial mouse position. */
  float2 imval;
  float2 center;
  float factor;
  float precision_factor;
  bool precision;

  /** Additional data, if needed by the particular function. */
  void *data;

  /**
   * Use virtual cursor, which takes precision into account
   * keeping track of the cursors 'virtual' location,
   * to avoid jumping values when its toggled.
   *
   * This works well for scaling drag motion,
   * but not for rotating around a point (rotation needs its own custom accumulator)
   */
  bool use_virtual_mval;
  struct {
    double2 prev;
    double2 accum;
  } virtual_mval;
};

struct TransCustomData {
  void *data;
  void (*free_cb)(TransInfo *, TransDataContainer *tc, TransCustomData *custom_data);
  unsigned int use_free : 1;
};

/**
 * Rule of thumb for choosing between mode/type:
 * - If transform mode uses the data, assign to `mode`
 *   (typically in `transform.cc`).
 * - If conversion uses the data as an extension to the #TransData, assign to `type`
 *   (typically in transform_conversion.c).
 */
struct TransCustomDataContainer {
  /** Owned by the mode (grab, scale, bend... ). */
  union {
    TransCustomData mode, first_elem;
  };
  TransCustomData type;
};
#define TRANS_CUSTOM_DATA_ELEM_MAX (sizeof(TransCustomDataContainer) / sizeof(TransCustomData))

/**
 * Container for Transform Data
 *
 * Used to implement multi-object modes, so each object can have its
 * own data array as well as object matrix, local center etc.
 *
 * Anything that can't be shared between all objects
 * and doesn't make sense to store for every vertex (in the #TransDataContainer.data).
 *
 * \note at some point this could be used to store non object containers
 * although this only makes sense if each container has its own matrices,
 * otherwise all elements may as well be stored in one array (#TransDataContainer.data),
 * as is already done for curve-objects, f-curves. etc.
 */
struct TransDataContainer {
  /** Transformed data (array). */
  TransData *data;
  /** Transformed data extension (array). */
  TransDataExtension *data_ext;
  /** Transformed data for 2d (array). */
  TransData2D *data_2d;
  /** Transformed data for mirror elements (array). */
  TransDataMirror *data_mirror;

  /** Total number of transformed data, data_ext, data_2d. */
  int data_len;
  /** Total number of transformed data_mirror. */
  int data_mirror_len;
  /** Total number of transformed gp-frames. */
  int data_gpf_len;

  Object *obedit;

  float mat[4][4];
  float imat[4][4];
  /** 3x3 copies of matrices above. */
  float mat3[3][3];
  float imat3[3][3];

  /** Normalized #mat3. */
  float mat3_unit[3][3];

  /** If `t->flag & T_POSE`, this denotes pose object. */
  Object *poseobj;

  /** Center of transformation (in local-space), Calculated from #TransInfo.center_global. */
  float center_local[3];

  /**
   * Use for cases we care about the active, eg: active vert of active mesh.
   * if set this will _always_ be the first item in the array.
   */
  bool is_active;

  /**
   * Store matrix, this avoids having to have duplicate check all over
   * Typically: 'obedit->object_to_world().ptr()' or 'poseobj->object_to_world().ptr()', but may be
   * used elsewhere too.
   */
  bool use_local_mat;

  /** Mirror option. */
  union {
    struct {
      uint use_mirror_axis_x : 1;
      uint use_mirror_axis_y : 1;
      uint use_mirror_axis_z : 1;
    };
    /* For easy checking. */
    char use_mirror_axis_any;
  };

  TransCustomDataContainer custom;

  /**
   * Array of indices for the `data`, `data_ext`, and `data_2d` arrays.
   *
   * When using this index map to traverse the arrays, they will be sorted primarily by selection
   * state (selected before unselected). Depending on the sort function used (see below),
   * unselected items are then sorted by their "distance" for proportional editing.
   *
   * At the moment of writing, this map is only used in cases where `tc->data` has a mixture of
   * selected and unselected items (as far as I, Sybren, know, just for proportial editing).
   * Without `tc->sorted_index_map`, all items in `tc->data` are expected to be selected.
   *
   * NOTE: this is set to `nullptr` by default; use one of the sorting functions below to
   * initialize the array.
   *
   * \see #sort_trans_data_selected_first Sorts only by selection state.
   * \see #sort_trans_data_dist Sorts by selection state and distance.
   */
  int *sorted_index_map;

  /**
   * Call the given function for each index in the data. This index can then be
   * used to access the `data`, `data_ext`, and `data_2d` arrays.
   *
   * If there is a `sorted_index_map` (see above), this will be used. Otherwise
   * it is assumed that the arrays can be iterated in their natural array order.
   *
   * \param fn: function that's called for each index. The function should
   * return whether to keep looping (true) or break out of the loop (false).
   *
   * \return whether the end of the loop was reached.
   */
  bool foreach_index(FunctionRef<bool(int)> fn) const
  {
    if (this->sorted_index_map) {
      for (const int i : Span(this->sorted_index_map, this->data_len)) {
        if (!fn(i)) {
          return false;
        }
      }
    }
    else {
      for (const int i : IndexRange(this->data_len)) {
        if (!fn(i)) {
          return false;
        }
      }
    }
    return true;
  }

  /**
   * Call \a fn only for indices of selected items.
   * Apart from that, this is the same as `index_map()` above.
   *
   * \param fn: function that's called for each index. Contrary to the `index_map()` function, it
   * is assumed that all selected items should be visited, and so for simplicity there is no `bool`
   * to return.
   */
  void foreach_index_selected(FunctionRef<void(int)> fn) const
  {
    this->foreach_index([&](const int i) {
      const bool is_selected = (this->data[i].flag & TD_SELECTED);
      if (!is_selected) {
        /* Selected items are sorted first. Either this is trivially true
         * (proportional editing off, so the only transformed data is the
         * selected data) or it's handled by `sorted_index_map`. */
        return false;
      }
      fn(i);
      return true;
    });
  }
};

struct TransInfo {
  TransDataContainer *data_container;
  int data_container_len;

  /** Combine length of all #TransDataContainer.data_len
   * Use to check if nothing is selected or if we have a single selection. */
  int data_len_all;

  /** TODO: It should be a member of #TransDataContainer. */
  TransConvertTypeInfo *data_type;

  /** Mode indicator as set for the operator.
   * NOTE: A same `mode_info` can have different `mode`s. */
  eTfmMode mode;
  TransModeInfo *mode_info;

  /** Current context/options for transform. */
  eTContext options;
  /** Generic flags for special behaviors. */
  eTFlag flag;
  /** Special modifiers, by function, not key. */
  eTModifier modifiers;
  /** Current state (running, canceled. */
  eTState state;
  /** Redraw flag. */
  eRedrawFlag redraw;
  /** Choice of custom cursor with or without a help line from the gizmo to the mouse position. */
  eTHelpline helpline;

  /** Constraint Data. */
  TransCon con;

  /** Snap Data. */
  TransSnap tsnap;

  /** Numerical input. */
  NumInput num;

  /** Mouse input. */
  MouseInput mouse;

  /** Proportional circle radius. */
  float prop_size;
  /** Proportional falloff text. */
  char proptext[20];
  /**
   * Spaces using non 1:1 aspect, (UV's, F-curve, movie-clip... etc).
   * use for conversion and snapping.
   */
  float aspect[3];
  /** Center of transformation (in global-space). */
  float center_global[3];
  /** Center in screen coordinates. */
  float center2d[2];
  /** Maximum index on the input vector. */
  short idx_max;
  /** Increment value for incremental snapping. */
  float3 increment;
  float increment_precision;
  /** Spatial snapping gears(even when rotating, scaling... etc). */
  float snap_spatial[3];
  /**
   * Precision factor that is multiplied to snap_spatial when precision
   * modifier is enabled for snap to grid.
   */
  float snap_spatial_precision;
  /** Mouse side of the current frame, 'L', 'R' or 'B'. */
  char frame_side;

  /** Copy from #RegionView3D, prevents feedback. */
  float viewmat[4][4];
  /** And to make sure we don't have to. */
  float viewinv[4][4];
  /** Access #RegionView3D from other space types. */
  float persmat[4][4];
  float persinv[4][4];
  short persp;
  short around;
  /** Space-type where transforming is. */
  char spacetype;
  /** Type of active object being edited. */
  short obedit_type;

  /** Translation, to show for widget. */
  float vec[3];
  /** Rotate/re-scale, to show for widget. */
  float mat[3][3];

  /** Orientation matrix of the current space. */
  float spacemtx[3][3];
  float spacemtx_inv[3][3];
  /** Name of the current space. */
  char spacename[/*MAX_NAME*/ 64];

  /*************** NEW STUFF *********************/
  /** Event type used to launch transform. */
  short launch_event;
  /**
   * Is the actual launch event a drag event?
   * (`launch_event` is set to the corresponding mouse button then.)
   */
  bool is_launch_event_drag;

  bool is_orient_default_overwrite;

  struct {
    short type;
    float matrix[3][3];
  } orient[3];

  eTOType orient_curr;

  /**
   * All values from `TransInfo.orient[].type` converted into a flag
   * to allow quickly checking which orientation types are used.
   */
  int orient_type_mask;

  short prop_mode;

  /** Value taken as input, either through mouse coordinates or entered as a parameter. */
  float values[4];

  /** Offset applied on top of modal input. */
  float values_modal_offset[4];

  /** Final value of the transformation (displayed in the redo panel).
   * If the operator is executed directly (not modal), this value is usually the
   * value of the input parameter, except when a constrain is entered. */
  float values_final[4];

  /** Cache safe value for constraints that require iteration or are slow to calculate. */
  float values_inside_constraints[4];

  /* Axis members for modes that use an axis separate from the orientation (rotate & shear). */

  /** Primary axis, rotate only uses this. */
  int orient_axis;
  /** Secondary axis, shear uses this. */
  int orient_axis_ortho;

  /** Remove elements if operator is canceled. */
  bool remove_on_cancel;

  void *view;
  /** Only valid (non null) during an operator called function. */
  bContext *context;
  wmMsgBus *mbus;
  ScrArea *area;
  ARegion *region;
  Depsgraph *depsgraph;
  Scene *scene;
  ViewLayer *view_layer;
  ToolSettings *settings;
  wmTimer *animtimer;
  /** Needed so we can perform a look up for header text. */
  wmKeyMap *keymap;
  /** Assign from the operator, or can be NULL. */
  ReportList *reports;
  /** Current mouse position. */
  float2 mval;
  /** Use for 3d view. */
  float zfac;
  void *draw_handle_view;
  void *draw_handle_pixel;
  void *draw_handle_cursor;

  /** Currently only used for random curve of proportional editing. */
  RNG *rng;

  ViewOpsData *vod;

  /** Typically for mode settings. */
  TransCustomDataContainer custom;

  /* Needed for sculpt transform. */
  const char *undo_name;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Transform API
 * \{ */

/**
 * \note Caller needs to free `t` on a 0 return.
 * \warning \a event might be NULL (when tweaking from redo panel)
 * \see #saveTransform which writes these values back.
 */
bool initTransform(bContext *C, TransInfo *t, wmOperator *op, const wmEvent *event, int mode);
/**
 * \see #initTransform which reads values from the operator.
 */
void saveTransform(bContext *C, TransInfo *t, wmOperator *op);
wmOperatorStatus transformEvent(TransInfo *t, wmOperator *op, const wmEvent *event);
void transformApply(bContext *C, TransInfo *t);
wmOperatorStatus transformEnd(bContext *C, TransInfo *t);

void setTransformViewMatrices(TransInfo *t);
void setTransformViewAspect(TransInfo *t, float r_aspect[3]);
void convertViewVec(TransInfo *t, float r_vec[3], double dx, double dy);
/**
 * If viewport projection fails, calculate a usable fallback.
 */
void projectFloatViewCenterFallback(TransInfo *t, float adr[2]);
void projectIntViewEx(TransInfo *t, const float vec[3], int adr[2], eV3DProjTest flag);
void projectIntView(TransInfo *t, const float vec[3], int adr[2]);
void projectFloatViewEx(TransInfo *t, const float vec[3], float adr[2], eV3DProjTest flag);
void projectFloatView(TransInfo *t, const float vec[3], float adr[2]);

void applyAspectRatio(TransInfo *t, float vec[2]);
void removeAspectRatio(TransInfo *t, float vec[2]);

/**
 * Called in `transform_ops.cc`, on each regeneration of key-maps.
 */
wmKeyMap *transform_modal_keymap(wmKeyConfig *keyconf);

/**
 * Transform a single matrix using the current `t->final_values`.
 */
bool transform_apply_matrix(TransInfo *t, float mat[4][4]);
void transform_final_value_get(const TransInfo *t, float *value, int value_num);
void view_vector_calc(const TransInfo *t, const float focus[3], float r_vec[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name General Utils
 * \{ */

/** Calculates projection vector based on a location. */
void transform_view_vector_calc(const TransInfo *t, const float focus[3], float r_vec[3]);
bool transdata_check_local_islands(TransInfo *t, short around);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse Input
 * \{ */

enum MouseInputMode {
  INPUT_NONE,
  INPUT_VECTOR,
  INPUT_SPRING,
  INPUT_SPRING_FLIP,
  INPUT_SPRING_DELTA,
  INPUT_ANGLE,
  INPUT_ANGLE_SPRING,
  INPUT_TRACKBALL,
  INPUT_HORIZONTAL_RATIO,
  INPUT_HORIZONTAL_ABSOLUTE,
  INPUT_VERTICAL_RATIO,
  INPUT_VERTICAL_ABSOLUTE,
  INPUT_CUSTOM_RATIO,
  INPUT_CUSTOM_RATIO_FLIP,
  INPUT_ERROR,
  INPUT_ERROR_DASH,
};

void initMouseInput(
    TransInfo *t, MouseInput *mi, const float2 &center, const float2 &mval, bool precision);
void initMouseInputMode(TransInfo *t, MouseInput *mi, MouseInputMode mode);
void applyMouseInput(TransInfo *t, MouseInput *mi, const float2 &mval, float output[3]);
void transform_input_update(TransInfo *t, const float fac);
void transform_input_virtual_mval_reset(TransInfo *t);
void transform_input_reset(TransInfo *t, const float2 &mval);

void setCustomPoints(TransInfo *t, MouseInput *mi, const int mval_start[2], const int mval_end[2]);
void setCustomPointsFromDirection(TransInfo *t, MouseInput *mi, const float2 &dir);
void setInputPostFct(MouseInput *mi, void (*post)(TransInfo *t, float values[3]));

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generics
 * \{ */

/**
 * Setup internal data, mouse, vectors
 *
 * \note \a op and \a event can be NULL
 *
 * \see #saveTransform does the reverse.
 */
void initTransInfo(bContext *C, TransInfo *t, wmOperator *op, const wmEvent *event);
/**
 * Needed for mode switching.
 */
void freeTransCustomDataForMode(TransInfo *t);
/**
 * Here I would suggest only #TransInfo related issues, like free data & reset variables.
 * Not redraws.
 */
void postTrans(bContext *C, TransInfo *t);
/**
 * Free data before switching to another mode.
 */
void resetTransModal(TransInfo *t);
void resetTransRestrictions(TransInfo *t);

void restoreTransObjects(TransInfo *t);

void calculateCenter2D(TransInfo *t);
void calculateCenterLocal(TransInfo *t, const float center_global[3]);

void calculateCenter(TransInfo *t);
/**
 * Called every time the view changes due to navigation.
 * Adjusts the mouse position relative to the object.
 */
void transformViewUpdate(TransInfo *t);

/* API functions for getting center points. */
void calculateCenterBound(TransInfo *t, float r_center[3]);
void calculateCenterMedian(TransInfo *t, float r_center[3]);
void calculateCenterCursor(TransInfo *t, float r_center[3]);
void calculateCenterCursor2D(TransInfo *t, float r_center[2]);
void calculateCenterCursorGraph2D(TransInfo *t, float r_center[2]);
/**
 * \param select_only: only get active center from data being transformed.
 */
bool calculateCenterActive(TransInfo *t, bool select_only, float r_center[3]);

void calculatePropRatio(TransInfo *t);

/**
 * Rotate an element, low level code, ignore protected channels.
 * (use for objects or pose-bones)
 * Similar to #ElementRotation.
 */
void transform_data_ext_rotate(TransData *td,
                               TransDataExtension *td_ext,
                               float mat[3][3],
                               bool use_drot);

Object *transform_object_deform_pose_armature_get(const TransInfo *t, Object *ob);

void freeCustomNormalArray(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data);

/* TODO: move to: `transform_query.c`. */
bool checkUseAxisMatrix(TransInfo *t);

/** \} */

}  // namespace blender::ed::transform
