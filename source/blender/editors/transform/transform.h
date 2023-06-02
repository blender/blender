/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#pragma once

#include "ED_numinput.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "DNA_listBase.h"
#include "DNA_object_enums.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"

#include "transform_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Types/
 * \{ */

struct ARegion;
struct Depsgraph;
struct NumInput;
struct Object;
struct RNG;
struct ReportList;
struct Scene;
struct ScrArea;
struct SnapObjectContext;
struct TransConvertTypeInfo;
struct TransDataContainer;
struct TransInfo;
struct TransSnap;
struct ViewLayer;
struct ViewOpsData;
struct bContext;
struct wmEvent;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperator;
struct wmTimer;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enums and Flags
 * \{ */

/** #TransInfo.options */
typedef enum {
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
  /** Enable edge scrolling in 2D views */
  CTX_VIEW2D_EDGE_PAN = (1 << 15),
} eTContext;

/** #TransInfo.flag */
typedef enum {
  /** \note We could remove 'T_EDIT' and use 'obedit_type', for now ensure they're in sync. */
  T_EDIT = 1 << 0,
  /** Transform points, having no rotation/scale. */
  T_POINTS = 1 << 1,
  /** restrictions flags */
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

  /** No cursor wrapping on region bounds */
  T_NO_CURSOR_WRAP = 1 << 23,

  /** Do not display Xform gizmo even though it is available. */
  T_NO_GIZMO = 1 << 24,
} eTFlag;
ENUM_OPERATORS(eTFlag, T_NO_GIZMO);

#define T_ALL_RESTRICTIONS (T_NO_CONSTRAINT | T_NULL_ONE)
#define T_PROP_EDIT_ALL (T_PROP_EDIT | T_PROP_CONNECTED | T_PROP_PROJECTED)

/** #TransInfo.modifiers */
typedef enum {
  MOD_CONSTRAINT_SELECT_AXIS = 1 << 0,
  MOD_PRECISION = 1 << 1,
  MOD_SNAP = 1 << 2,
  MOD_SNAP_INVERT = 1 << 3,
  MOD_CONSTRAINT_SELECT_PLANE = 1 << 4,
  MOD_NODE_ATTACH = 1 << 5,
  MOD_SNAP_FORCED = 1 << 6,
} eTModifier;
ENUM_OPERATORS(eTModifier, MOD_NODE_ATTACH)

/** #TransSnap.status */
typedef enum eTSnap {
  SNAP_RESETTED = 0,
  SNAP_SOURCE_FOUND = 1 << 0,
  /* Special flag for snap to grid. */
  SNAP_TARGET_GRID_FOUND = 1 << 1,
  SNAP_TARGET_FOUND = 1 << 2,
  SNAP_MULTI_POINTS = 1 << 3,
} eTSnap;
ENUM_OPERATORS(eTSnap, SNAP_MULTI_POINTS)

/** #TransCon.mode, #TransInfo.con.mode */
typedef enum {
  /** When set constraints are in use. */
  CON_APPLY = 1 << 0,
  /** These are only used for modal execution. */
  CON_AXIS0 = 1 << 1,
  CON_AXIS1 = 1 << 2,
  CON_AXIS2 = 1 << 3,
  CON_SELECT = 1 << 4,
  /** Does not reorient vector to face viewport when on. */
  CON_NOFLIP = 1 << 5,
  CON_USER = 1 << 6,
} eTConstraint;

/** #TransInfo.state */
typedef enum {
  TRANS_STARTING = 0,
  TRANS_RUNNING = 1,
  TRANS_CONFIRM = 2,
  TRANS_CANCEL = 3,
} eTState;

/** #TransInfo.redraw */
typedef enum {
  TREDRAW_NOTHING = 0,
  TREDRAW_SOFT = (1 << 0),
  TREDRAW_HARD = (1 << 1) | TREDRAW_SOFT,
} eRedrawFlag;
ENUM_OPERATORS(eRedrawFlag, TREDRAW_HARD)

/** #TransInfo.helpline */
typedef enum {
  HLP_NONE = 0,
  HLP_SPRING = 1,
  HLP_ANGLE = 2,
  HLP_HARROW = 3,
  HLP_VARROW = 4,
  HLP_CARROW = 5,
  HLP_TRACKBALL = 6,
} eTHelpline;

typedef enum {
  O_DEFAULT = 0,
  O_SCENE,
  O_SET,
} eTOType;

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

  /* 18 and 19 used by number-input, defined in `ED_numinput.h`. */
  // NUM_MODAL_INCREMENT_UP = 18,
  // NUM_MODAL_INCREMENT_DOWN = 19,

  TFM_MODAL_PROPSIZE_UP = 20,
  TFM_MODAL_PROPSIZE_DOWN = 21,
  TFM_MODAL_AUTOIK_LEN_INC = 22,
  TFM_MODAL_AUTOIK_LEN_DEC = 23,

  TFM_MODAL_NODE_ATTACH_ON = 24,
  TFM_MODAL_NODE_ATTACH_OFF = 25,

  /** For analog input, like track-pad. */
  TFM_MODAL_PROPSIZE = 26,
  /** Node editor insert offset (also called auto-offset) direction toggle. */
  TFM_MODAL_INSERTOFS_TOGGLE_DIR = 27,

  TFM_MODAL_AUTOCONSTRAINT = 28,
  TFM_MODAL_AUTOCONSTRAINTPLANE = 29,

  TFM_MODAL_PRECISION = 30,

  TFM_MODAL_VERT_EDGE_SLIDE = 31,
  TFM_MODAL_TRACKBALL = 32,
  TFM_MODAL_ROTATE_NORMALS = 33,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Types
 * \{ */

typedef struct TransSnapPoint {
  struct TransSnapPoint *next, *prev;
  float co[3];
} TransSnapPoint;

typedef struct TransSnap {
  /* Snapping options stored as flags */
  eSnapFlag flag;
  /* Method(s) used for snapping source to target */
  eSnapMode mode;
  /* Part of source to snap to target */
  eSnapSourceOP source_operation;
  /* Determines which objects are possible target */
  eSnapTargetOP target_operation;
  short face_nearest_steps;
  eTSnap status;
  /* Snapped Element Type (currently for objects only). */
  eSnapMode snapElem;
  /** snapping from this point (in global-space). */
  float snap_source[3];
  /** to this point (in global-space). */
  float snap_target[3];
  float snap_target_grid[3];
  float snapNormal[3];
  char snapNodeBorder;
  ListBase points;
  TransSnapPoint *selectedPoint;
  double last;
  void (*snap_target_fn)(struct TransInfo *, float *);
  void (*snap_source_fn)(struct TransInfo *);

  /**
   * Re-usable snap context data.
   */
  union {
    struct SnapObjectContext *object_context;
    struct TransSeqSnapData *seq_context;
  };
} TransSnap;

typedef struct TransCon {
  /** Description of the constraint for header_print. */
  char text[50];
  /** Projection constraint matrix (same as #imtx with some axis == 0). */
  float pmtx[3][3];
  /** Initial mouse value for visual calculation
   * the one in #TransInfo is not guarantee to stay the same (Rotates change it). */
  int imval[2];
  /** Mode flags of the constraint. */
  eTConstraint mode;
  void (*drawExtra)(struct TransInfo *t);

  /* NOTE: if 'tc' is NULL, 'td' must also be NULL.
   * For constraints that needs to draw differently from the other
   * uses this instead of the generic draw function. */

  /** Apply function pointer for linear vectorial transformation
   * The last three parameters are pointers to the in/out/printable vectors. */
  void (*applyVec)(const struct TransInfo *t,
                   const struct TransDataContainer *tc,
                   const struct TransData *td,
                   const float in[3],
                   float r_out[3]);
  /** Apply function pointer for size transformation. */
  void (*applySize)(const struct TransInfo *t,
                    const struct TransDataContainer *tc,
                    const struct TransData *td,
                    float r_smat[3][3]);
  /** Apply function pointer for rotation transformation */
  void (*applyRot)(const struct TransInfo *t,
                   const struct TransDataContainer *tc,
                   const struct TransData *td,
                   float r_axis[3],
                   float *r_angle);
} TransCon;

typedef struct MouseInput {
  void (*apply)(struct TransInfo *t, struct MouseInput *mi, const double mval[2], float output[3]);
  void (*post)(struct TransInfo *t, float values[3]);

  /** Initial mouse position. */
  int imval[2];
  float imval_unproj[3];
  float center[2];
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
    double prev[2];
    double accum[2];
  } virtual_mval;
} MouseInput;

typedef struct TransCustomData {
  void *data;
  void (*free_cb)(struct TransInfo *,
                  struct TransDataContainer *tc,
                  struct TransCustomData *custom_data);
  unsigned int use_free : 1;
} TransCustomData;

typedef struct TransCenterData {
  float global[3];
  unsigned int is_set : 1;
} TransCenterData;

/**
 * Rule of thumb for choosing between mode/type:
 * - If transform mode uses the data, assign to `mode`
 *   (typically in transform.c).
 * - If conversion uses the data as an extension to the #TransData, assign to `type`
 *   (typically in transform_conversion.c).
 */
typedef struct TransCustomDataContainer {
  /** Owned by the mode (grab, scale, bend... ). */
  union {
    TransCustomData mode, first_elem;
  };
  TransCustomData type;
} TransCustomDataContainer;
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
typedef struct TransDataContainer {
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

  struct Object *obedit;

  float mat[4][4];
  float imat[4][4];
  /** 3x3 copies of matrices above. */
  float mat3[3][3];
  float imat3[3][3];

  /** Normalized 'mat3' */
  float mat3_unit[3][3];

  /** if 't->flag & T_POSE', this denotes pose object */
  struct Object *poseobj;

  /** Center of transformation (in local-space), Calculated from #TransInfo.center_global. */
  float center_local[3];

  /**
   * Use for cases we care about the active, eg: active vert of active mesh.
   * if set this will _always_ be the first item in the array.
   */
  bool is_active;

  /**
   * Store matrix, this avoids having to have duplicate check all over
   * Typically: 'obedit->object_to_world' or 'poseobj->object_to_world', but may be used elsewhere
   * too.
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
} TransDataContainer;

typedef struct TransInfo {
  TransDataContainer *data_container;
  int data_container_len;

  /** Combine length of all #TransDataContainer.data_len
   * Use to check if nothing is selected or if we have a single selection. */
  int data_len_all;

  /** TODO: It should be a member of #TransDataContainer. */
  struct TransConvertTypeInfo *data_type;

  /** Mode indicator as set for the operator.
   * NOTE: A same `mode_info` can have different `mode`s. */
  eTfmMode mode;
  struct TransModeInfo *mode_info;

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

  /** proportional circle radius. */
  float prop_size;
  /** proportional falloff text. */
  char proptext[20];
  /**
   * Spaces using non 1:1 aspect, (uv's, f-curve, movie-clip... etc)
   * use for conversion and snapping.
   */
  float aspect[3];
  /** center of transformation (in global-space) */
  float center_global[3];
  /** center in screen coordinates. */
  float center2d[2];
  /** maximum index on the input vector. */
  short idx_max;
  /** Snapping Gears. */
  float snap[2];
  /** Spatial snapping gears(even when rotating, scaling... etc). */
  float snap_spatial[3];
  /**
   * Precision factor that is multiplied to snap_spatial when precision
   * modifier is enabled for snap to grid or incremental snap.
   */
  float snap_spatial_precision;
  /** Mouse side of the current frame, 'L', 'R' or 'B' */
  char frame_side;

  /** copy from #RegionView3D, prevents feedback. */
  float viewmat[4][4];
  /** and to make sure we don't have to. */
  float viewinv[4][4];
  /** Access #RegionView3D from other space types. */
  float persmat[4][4];
  float persinv[4][4];
  short persp;
  short around;
  /** space-type where transforming is. */
  char spacetype;
  /** Type of active object being edited. */
  short obedit_type;

  /** translation, to show for widget. */
  float vec[3];
  /** Rotate/re-scale, to show for widget. */
  float mat[3][3];

  /** orientation matrix of the current space. */
  float spacemtx[3][3];
  float spacemtx_inv[3][3];
  /** name of the current space, MAX_NAME. */
  char spacename[64];

  /*************** NEW STUFF *********************/
  /** event type used to launch transform. */
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

  /** remove elements if operator is canceled. */
  bool remove_on_cancel;

  void *view;
  /** Only valid (non null) during an operator called function. */
  struct bContext *context;
  struct wmMsgBus *mbus;
  struct ScrArea *area;
  struct ARegion *region;
  struct Depsgraph *depsgraph;
  struct Scene *scene;
  struct ViewLayer *view_layer;
  struct ToolSettings *settings;
  struct wmTimer *animtimer;
  /** Needed so we can perform a look up for header text. */
  struct wmKeyMap *keymap;
  /** assign from the operator, or can be NULL. */
  struct ReportList *reports;
  /** current mouse position. */
  int mval[2];
  /** use for 3d view. */
  float zfac;
  void *draw_handle_view;
  void *draw_handle_pixel;
  void *draw_handle_cursor;

  /** Currently only used for random curve of proportional editing. */
  struct RNG *rng;

  struct ViewOpsData *vod;

  /** Typically for mode settings. */
  TransCustomDataContainer custom;

  /* Needed for sculpt transform. */
  const char *undo_name;
} TransInfo;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Transform API
 * \{ */

/**
 * \note  caller needs to free `t` on a 0 return
 * \warning \a event might be NULL (when tweaking from redo panel)
 * \see #saveTransform which writes these values back.
 */
bool initTransform(struct bContext *C,
                   struct TransInfo *t,
                   struct wmOperator *op,
                   const struct wmEvent *event,
                   int mode);
/**
 * \see #initTransform which reads values from the operator.
 */
void saveTransform(struct bContext *C, struct TransInfo *t, struct wmOperator *op);
int transformEvent(TransInfo *t, const struct wmEvent *event);
void transformApply(struct bContext *C, TransInfo *t);
int transformEnd(struct bContext *C, TransInfo *t);

void setTransformViewMatrices(TransInfo *t);
void setTransformViewAspect(TransInfo *t, float r_aspect[3]);
void convertViewVec(TransInfo *t, float r_vec[3], double dx, double dy);
void projectIntViewEx(TransInfo *t, const float vec[3], int adr[2], eV3DProjTest flag);
void projectIntView(TransInfo *t, const float vec[3], int adr[2]);
void projectFloatViewEx(TransInfo *t, const float vec[3], float adr[2], eV3DProjTest flag);
void projectFloatView(TransInfo *t, const float vec[3], float adr[2]);

void applyAspectRatio(TransInfo *t, float vec[2]);
void removeAspectRatio(TransInfo *t, float vec[2]);

/**
 * Called in transform_ops.c, on each regeneration of key-maps.
 */
struct wmKeyMap *transform_modal_keymap(struct wmKeyConfig *keyconf);

/**
 * Transform a single matrix using the current `t->final_values`.
 */
bool transform_apply_matrix(TransInfo *t, float mat[4][4]);
void transform_final_value_get(const TransInfo *t, float *value, int value_num);

/** \} */

/* -------------------------------------------------------------------- */
/** \name TransData Creation and General Handling
 * \{ */

bool transdata_check_local_islands(TransInfo *t, short around);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse Input
 * \{ */

typedef enum {
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
} MouseInputMode;

void initMouseInput(
    TransInfo *t, MouseInput *mi, const float center[2], const int mval[2], bool precision);
void initMouseInputMode(TransInfo *t, MouseInput *mi, MouseInputMode mode);
void applyMouseInput(struct TransInfo *t,
                     struct MouseInput *mi,
                     const int mval[2],
                     float output[3]);
void transform_input_update(TransInfo *t, const float fac);
void transform_input_virtual_mval_reset(TransInfo *t);

void setCustomPoints(TransInfo *t, MouseInput *mi, const int start[2], const int end[2]);
void setCustomPointsFromDirection(TransInfo *t, MouseInput *mi, const float dir[2]);
void setInputPostFct(MouseInput *mi, void (*post)(struct TransInfo *t, float values[3]));

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
void initTransInfo(struct bContext *C,
                   TransInfo *t,
                   struct wmOperator *op,
                   const struct wmEvent *event);
/**
 * Needed for mode switching.
 */
void freeTransCustomDataForMode(TransInfo *t);
/**
 * Here I would suggest only #TransInfo related issues, like free data & reset vars. Not redraws.
 */
void postTrans(struct bContext *C, TransInfo *t);
/**
 * Free data before switching to another mode.
 */
void resetTransModal(TransInfo *t);
void resetTransRestrictions(TransInfo *t);

/* DRAWLINE options flags */
#define DRAWLIGHT 1

void applyTransObjects(TransInfo *t);
void restoreTransObjects(TransInfo *t);

void calculateCenter2D(TransInfo *t);
void calculateCenterLocal(TransInfo *t, const float center_global[3]);

void calculateCenter(TransInfo *t);
/**
 * Called every time the view changes due to navigation.
 * Adjusts the mouse position relative to the object.
 */
void tranformViewUpdate(TransInfo *t);

/* API functions for getting center points */
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
void transform_data_ext_rotate(TransData *td, float mat[3][3], bool use_drot);

struct Object *transform_object_deform_pose_armature_get(const TransInfo *t, struct Object *ob);

void freeCustomNormalArray(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data);

/* TODO: move to: `transform_query.c`. */
bool checkUseAxisMatrix(TransInfo *t);

#define TRANSFORM_SNAP_MAX_PX 100.0f
#define TRANSFORM_DIST_INVALID -FLT_MAX

/* Temp macros. */

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

#ifdef __cplusplus
}
#endif
