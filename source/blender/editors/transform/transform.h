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
 * \ingroup edtransform
 */

#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__

#include "ED_numinput.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "RE_engine.h"

#include "DNA_listBase.h"
#include "DNA_object_enums.h"

#include "DEG_depsgraph.h"

#include "transform_data.h"

/* ************************** Types ***************************** */

struct ARegion;
struct Depsgraph;
struct NumInput;
struct Object;
struct RNG;
struct ReportList;
struct Scene;
struct ScrArea;
struct SnapObjectContext;
struct TransDataContainer;
struct TransInfo;
struct TransSnap;
struct TransformOrientation;
struct ViewLayer;
struct bContext;
struct wmEvent;
struct wmKeyConfig;
struct wmKeyMap;
struct wmTimer;

/* transinfo->redraw */
typedef enum {
  TREDRAW_NOTHING = 0,
  TREDRAW_HARD = 1,
  TREDRAW_SOFT = 2,
} eRedrawFlag;

typedef struct TransSnapPoint {
  struct TransSnapPoint *next, *prev;
  float co[3];
} TransSnapPoint;

typedef struct TransSnap {
  short mode;
  short target;
  short modePoint;
  short modeSelect;
  bool align;
  bool project;
  bool snap_self;
  bool peel;
  bool snap_spatial_grid;
  char status;
  /* Snapped Element Type (currently for objects only). */
  char snapElem;
  /** snapping from this point (in global-space). */
  float snapPoint[3];
  /** to this point (in global-space). */
  float snapTarget[3];
  float snapNormal[3];
  char snapNodeBorder;
  ListBase points;
  TransSnapPoint *selectedPoint;
  double last;
  void (*applySnap)(struct TransInfo *, float *);
  void (*calcSnap)(struct TransInfo *, float *);
  void (*targetSnap)(struct TransInfo *);
  /**
   * Get the transform distance between two points (used by Closest snap)
   *
   * \note Return value can be anything,
   * where the smallest absolute value defines what's closest.
   */
  float (*distance)(struct TransInfo *t, const float p1[3], const float p2[3]);

  /**
   * Re-usable snap context data.
   */
  struct SnapObjectContext *object_context;
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
  int mode;
  void (*drawExtra)(struct TransInfo *t);

  /* Note: if 'tc' is NULL, 'td' must also be NULL.
   * For constraints that needs to draw differently from the other
   * uses this instead of the generic draw function. */

  /** Apply function pointer for linear vectorial transformation
   * The last three parameters are pointers to the in/out/printable vectors. */
  void (*applyVec)(struct TransInfo *t,
                   struct TransDataContainer *tc,
                   struct TransData *td,
                   const float in[3],
                   float out[3]);
  /** Apply function pointer for size transformation. */
  void (*applySize)(struct TransInfo *t,
                    struct TransDataContainer *tc,
                    struct TransData *td,
                    float smat[3][3]);
  /** Apply function pointer for rotation transformation */
  void (*applyRot)(struct TransInfo *t,
                   struct TransDataContainer *tc,
                   struct TransData *td,
                   float vec[3],
                   float *angle);
} TransCon;

typedef struct MouseInput {
  void (*apply)(struct TransInfo *t, struct MouseInput *mi, const double mval[2], float output[3]);
  void (*post)(struct TransInfo *t, float values[3]);

  /** Initial mouse position. */
  int imval[2];
  bool precision;
  float precision_factor;
  float center[2];
  float factor;
  /** Additional data, if needed by the particular function. */
  void *data;

  /**
   * Use virtual cursor, which takes precision into account
   * keeping track of the cursors 'virtual' location,
   * to avoid jumping values when its toggled.
   *
   * This works well for scaling drag motion,
   * but not for rotating around a point (rotaton needs its own custom accumulator)
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
  /** Owned by the mode (grab, scale, bend... ).*/
  union {
    TransCustomData mode, first_elem;
  };
  TransCustomData type;
} TransCustomDataContainer;
#define TRANS_CUSTOM_DATA_ELEM_MAX (sizeof(TransCustomDataContainer) / sizeof(TransCustomData))

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
   * Typically: 'obedit->obmat' or 'poseobj->obmat', but may be used elsewhere too.
   */
  bool use_local_mat;

  /**  Mirror option. */
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

  /** eTransConvertType
   * TODO: It should be a member of TransDataContainer. */
  int data_type;

  /** Combine length of all #TransDataContainer.data_len
   * Use to check if nothing is selected or if we have a single selection. */
  int data_len_all;

  /** Current mode. */
  int mode;
  /** Generic flags for special behaviors. */
  int flag;
  /** Special modifiers, by function, not key. */
  int modifiers;
  /** Current state (running, canceled. */
  short state;
  /** Current context/options for transform. */
  int options;
  /** Init value for some transformations (and rotation angle). */
  float val;
  void (*transform)(struct TransInfo *, const int[2]);
  /** Transform function pointer. */
  eRedrawFlag (*handleEvent)(struct TransInfo *, const struct wmEvent *);
  /* event handler function pointer  RETURN 1 if redraw is needed */
  /** transformed constraint. */
  TransCon con;
  TransSnap tsnap;
  /** numerical input. */
  NumInput num;
  /** mouse input. */
  MouseInput mouse;
  /** redraw flag. */
  eRedrawFlag redraw;
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
  /* Lazy initialize center data for when we need other center values.
   * V3D_AROUND_ACTIVE + 1 (static assert checks this) */
  TransCenterData center_cache[5];
  /** maximum index on the input vector. */
  short idx_max;
  /** Snapping Gears. */
  float snap[3];
  /** Spatial snapping gears(even when rotating, scaling... etc). */
  float snap_spatial[3];
  /** Mouse side of the cfra, 'L', 'R' or 'B' */
  char frame_side;

  /** copy from G.vd, prevents feedback. */
  float viewmat[4][4];
  /** and to make sure we don't have to. */
  float viewinv[4][4];
  /** access G.vd from other space types. */
  float persmat[4][4];
  float persinv[4][4];
  short persp;
  short around;
  /** spacetype where transforming is. */
  char spacetype;
  /** Choice of custom cursor with or without a help line from the gizmo to the mouse position. */
  char helpline;
  /** Avoid looking inside TransDataContainer obedit. */
  short obedit_type;

  /** translation, to show for widget. */
  float vec[3];
  /** rot/rescale, to show for widget. */
  float mat[3][3];

  /** orientation matrix of the current space. */
  float spacemtx[3][3];
  float spacemtx_inv[3][3];
  /** name of the current space, MAX_NAME. */
  char spacename[64];

  /*************** NEW STUFF *********************/
  /** event type used to launch transform. */
  short launch_event;
  /** Is the actual launch event a tweak event? (launch_event above is set to the corresponding
   * mouse button then.) */
  bool is_launch_event_tweak;

  struct {
    short type;
    float matrix[3][3];
  } orient[3];
  short orient_curr;

  /** backup from view3d, to restore on end. */
  short gizmo_flag;

  short prop_mode;

  /** Value taken as input, either through mouse coordinates or entered as a parameter. */
  float values[4];

  /** Offset applied ontop of modal input. */
  float values_modal_offset[4];

  /** Final value of the transformation (displayed in the redo panel).
   * If the operator is executed directly (not modal), this value is usually the
   * value of the input parameter, except when a constrain is entered. */
  float values_final[4];

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
  /** so we can do lookups for header text. */
  struct wmKeyMap *keymap;
  /** assign from the operator, or can be NULL. */
  struct ReportList *reports;
  /** current mouse position. */
  int mval[2];
  /** use for 3d view. */
  float zfac;
  void *draw_handle_apply;
  void *draw_handle_view;
  void *draw_handle_pixel;
  void *draw_handle_cursor;

  /** Currently only used for random curve of proportional editing. */
  struct RNG *rng;

  /** Typically for mode settings. */
  TransCustomDataContainer custom;
} TransInfo;

/* ******************** Macros & Prototypes *********************** */

/* transinfo->state */
enum {
  TRANS_STARTING = 0,
  TRANS_RUNNING = 1,
  TRANS_CONFIRM = 2,
  TRANS_CANCEL = 3,
};

/* transinfo->flag */
enum {
  T_OBJECT = 1 << 0,
  /** \note We could remove 'T_EDIT' and use 'obedit_type', for now ensure they're in sync. */
  T_EDIT = 1 << 1,
  T_POSE = 1 << 2,
  T_TEXTURE = 1 << 3,
  /** Transforming the 3d view. */
  T_CAMERA = 1 << 4,
  /** Transforming the 3D cursor. */
  T_CURSOR = 1 << 5,
  /** Transform points, having no rotation/scale. */
  T_POINTS = 1 << 6,
  /** restrictions flags */
  T_NO_CONSTRAINT = 1 << 7,
  T_NULL_ONE = 1 << 8,
  T_NO_ZERO = 1 << 9,
  T_ALL_RESTRICTIONS = T_NO_CONSTRAINT | T_NULL_ONE | T_NO_ZERO,

  T_PROP_EDIT = 1 << 10,
  T_PROP_CONNECTED = 1 << 11,
  T_PROP_PROJECTED = 1 << 12,
  T_PROP_EDIT_ALL = T_PROP_EDIT | T_PROP_CONNECTED | T_PROP_PROJECTED,

  T_V3D_ALIGN = 1 << 13,
  /** For 2d views like uv or fcurve. */
  T_2D_EDIT = 1 << 14,
  T_CLIP_UV = 1 << 15,

  /** Auto-ik is on. */
  T_AUTOIK = 1 << 16,

  /** Don't use mirror even if the data-block option is set. */
  T_NO_MIRROR = 1 << 17,

  /** To indicate that the value set in the `value` parameter is the final
   * value of the transformation, modified only by the constrain. */
  T_INPUT_IS_VALUES_FINAL = 1 << 18,

  /** To specify if we save back settings at the end. */
  T_MODAL = 1 << 19,

  /** No retopo. */
  T_NO_PROJECT = 1 << 20,

  T_RELEASE_CONFIRM = 1 << 21,

  /** Alternative transformation. used to add offset to tracking markers. */
  T_ALT_TRANSFORM = 1 << 22,

  /** #TransInfo.center has been set, don't change it. */
  T_OVERRIDE_CENTER = 1 << 23,

  T_MODAL_CURSOR_SET = 1 << 24,

  T_CLNOR_REBUILD = 1 << 25,

  /* Special Aftertrans. */
  T_AUTOMERGE = 1 << 26,
  T_AUTOSPLIT = 1 << 27,
};

/** #TransInfo.modifiers */
enum {
  MOD_CONSTRAINT_SELECT = 1 << 0,
  MOD_PRECISION = 1 << 1,
  MOD_SNAP = 1 << 2,
  MOD_SNAP_INVERT = 1 << 3,
  MOD_CONSTRAINT_PLANE = 1 << 4,
};

/* use node center for transform instead of upper-left corner.
 * disabled since it makes absolute snapping not work so nicely
 */
// #define USE_NODE_CENTER

/* ******************************************************************************** */

/** #TransInfo.helpline */
enum {
  HLP_NONE = 0,
  HLP_SPRING = 1,
  HLP_ANGLE = 2,
  HLP_HARROW = 3,
  HLP_VARROW = 4,
  HLP_CARROW = 5,
  HLP_TRACKBALL = 6,
};

/** #TransCon.mode, #TransInfo.con.mode */
enum {
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
};

/** #TransSnap.status */
enum {
  SNAP_FORCED = 1 << 0,
  TARGET_INIT = 1 << 1,
  POINT_INIT = 1 << 2,
  MULTI_POINTS = 1 << 3,
};

/** keymap modal items */
/* NOTE: these values are saved in keymap files, do not change then but just add new ones. */
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

  /* 18 and 19 used by numinput, defined in transform.h */

  TFM_MODAL_PROPSIZE_UP = 20,
  TFM_MODAL_PROPSIZE_DOWN = 21,
  TFM_MODAL_AUTOIK_LEN_INC = 22,
  TFM_MODAL_AUTOIK_LEN_DEC = 23,

  TFM_MODAL_EDGESLIDE_UP = 24,
  TFM_MODAL_EDGESLIDE_DOWN = 25,

  /* for analog input, like trackpad */
  TFM_MODAL_PROPSIZE = 26,
  /* node editor insert offset (aka auto-offset) direction toggle */
  TFM_MODAL_INSERTOFS_TOGGLE_DIR = 27,
};

bool initTransform(struct bContext *C,
                   struct TransInfo *t,
                   struct wmOperator *op,
                   const struct wmEvent *event,
                   int mode);
void saveTransform(struct bContext *C, struct TransInfo *t, struct wmOperator *op);
int transformEvent(TransInfo *t, const struct wmEvent *event);
void transformApply(struct bContext *C, TransInfo *t);
int transformEnd(struct bContext *C, TransInfo *t);

void setTransformViewMatrices(TransInfo *t);
void setTransformViewAspect(TransInfo *t, float r_aspect[3]);
void convertViewVec(TransInfo *t, float r_vec[3], double dx, double dy);
void projectIntViewEx(TransInfo *t, const float vec[3], int adr[2], const eV3DProjTest flag);
void projectIntView(TransInfo *t, const float vec[3], int adr[2]);
void projectFloatViewEx(TransInfo *t, const float vec[3], float adr[2], const eV3DProjTest flag);
void projectFloatView(TransInfo *t, const float vec[3], float adr[2]);

void applyAspectRatio(TransInfo *t, float vec[2]);
void removeAspectRatio(TransInfo *t, float vec[2]);

struct wmKeyMap *transform_modal_keymap(struct wmKeyConfig *keyconf);

/*********************** transform_gizmo.c ********** */

#define GIZMO_AXIS_LINE_WIDTH 2.0f

/* return 0 when no gimbal for selection */
bool gimbal_axis(struct Object *ob, float gmat[3][3]);
void drawDial3d(const TransInfo *t);

/*********************** TransData Creation and General Handling *********** */
bool transdata_check_local_islands(TransInfo *t, short around);

/********************** Mouse Input ******************************/

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
    TransInfo *t, MouseInput *mi, const float center[2], const int mval[2], const bool precision);
void initMouseInputMode(TransInfo *t, MouseInput *mi, MouseInputMode mode);
eRedrawFlag handleMouseInput(struct TransInfo *t,
                             struct MouseInput *mi,
                             const struct wmEvent *event);
void applyMouseInput(struct TransInfo *t,
                     struct MouseInput *mi,
                     const int mval[2],
                     float output[3]);

void setCustomPoints(TransInfo *t, MouseInput *mi, const int start[2], const int end[2]);
void setCustomPointsFromDirection(TransInfo *t, MouseInput *mi, const float dir[2]);
void setInputPostFct(MouseInput *mi, void (*post)(struct TransInfo *t, float values[3]));

/*********************** Generics ********************************/

void initTransDataContainers_FromObjectData(TransInfo *t,
                                            struct Object *obact,
                                            struct Object **objects,
                                            uint objects_len);
void initTransInfo(struct bContext *C,
                   TransInfo *t,
                   struct wmOperator *op,
                   const struct wmEvent *event);
void freeTransCustomDataForMode(TransInfo *t);
void postTrans(struct bContext *C, TransInfo *t);
void resetTransModal(TransInfo *t);
void resetTransRestrictions(TransInfo *t);

void drawLine(TransInfo *t, const float center[3], const float dir[3], char axis, short options);

/* DRAWLINE options flags */
#define DRAWLIGHT 1

void applyTransObjects(TransInfo *t);
void restoreTransObjects(TransInfo *t);
void recalcData(TransInfo *t);

void calculateCenter2D(TransInfo *t);
void calculateCenterLocal(TransInfo *t, const float center_global[3]);

const TransCenterData *transformCenter_from_type(TransInfo *t, int around);
void calculateCenter(TransInfo *t);

/* API functions for getting center points */
void calculateCenterBound(TransInfo *t, float r_center[3]);
void calculateCenterMedian(TransInfo *t, float r_center[3]);
void calculateCenterCursor(TransInfo *t, float r_center[3]);
void calculateCenterCursor2D(TransInfo *t, float r_center[2]);
void calculateCenterCursorGraph2D(TransInfo *t, float r_center[2]);
bool calculateCenterActive(TransInfo *t, bool select_only, float r_center[3]);

void calculatePropRatio(TransInfo *t);

void getViewVector(const TransInfo *t, const float coord[3], float vec[3]);

void transform_data_ext_rotate(TransData *td, float mat[3][3], bool use_drot);

/*********************** Transform Orientations ******************************/
short transform_orientation_matrix_get(struct bContext *C,
                                       TransInfo *t,
                                       const short orientation,
                                       const float custom[3][3],
                                       float r_spacemtx[3][3]);
const char *transform_orientations_spacename_get(TransInfo *t, const short orient_type);
void transform_orientations_current_set(struct TransInfo *t, const short orient_index);

/* Those two fill in mat and return non-zero on success */
bool createSpaceNormal(float mat[3][3], const float normal[3]);
bool createSpaceNormalTangent(float mat[3][3], const float normal[3], const float tangent[3]);

struct TransformOrientation *addMatrixSpace(struct bContext *C,
                                            float mat[3][3],
                                            const char *name,
                                            const bool overwrite);
void applyTransformOrientation(const struct TransformOrientation *ts,
                               float r_mat[3][3],
                               char r_name[64]);

enum {
  ORIENTATION_NONE = 0,
  ORIENTATION_NORMAL = 1,
  ORIENTATION_VERT = 2,
  ORIENTATION_EDGE = 3,
  ORIENTATION_FACE = 4,
};
#define ORIENTATION_USE_PLANE(ty) ELEM(ty, ORIENTATION_NORMAL, ORIENTATION_EDGE, ORIENTATION_FACE)

int getTransformOrientation_ex(const struct bContext *C,
                               float normal[3],
                               float plane[3],
                               const short around);
int getTransformOrientation(const struct bContext *C, float normal[3], float plane[3]);

void freeCustomNormalArray(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data);

/* TODO. transform_query.c */
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

#endif
