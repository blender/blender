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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform.h
 *  \ingroup edtransform
 */


#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__

#include "ED_transform.h"
#include "ED_numinput.h"
#include "ED_view3d.h"

#include "DNA_listBase.h"

#include "BLI_smallhash.h"
#include "BKE_editmesh.h"

/* ************************** Types ***************************** */

struct TransInfo;
struct TransData;
struct TransformOrientation;
struct TransSnap;
struct NumInput;
struct Object;
struct View3D;
struct ScrArea;
struct Scene;
struct bConstraint;
struct wmKeyMap;
struct wmKeyConfig;
struct bContext;
struct wmEvent;
struct wmTimer;
struct ARegion;
struct ReportList;
struct EditBone;

/* transinfo->redraw */
typedef enum {
	TREDRAW_NOTHING   = 0,
	TREDRAW_HARD      = 1,
	TREDRAW_SOFT      = 2,
} eRedrawFlag;

typedef struct TransSnapPoint {
	struct TransSnapPoint *next, *prev;
	float co[3];
} TransSnapPoint;

typedef struct TransSnap {
	short	mode;
	short	target;
	short	modePoint;
	short	modeSelect;
	bool	align;
	bool	project;
	bool	snap_self;
	bool	peel;
	short  	status;
	float	snapPoint[3]; /* snapping from this point */
	float	snapTarget[3]; /* to this point */
	float	snapNormal[3];
	float	snapTangent[3];
	char	snapNodeBorder;
	ListBase points;
	TransSnapPoint	*selectedPoint;
	float	dist; // Distance from snapPoint to snapTarget
	double	last;
	void  (*applySnap)(struct TransInfo *, float *);
	void  (*calcSnap)(struct TransInfo *, float *);
	void  (*targetSnap)(struct TransInfo *);
	/* Get the transform distance between two points (used by Closest snap) */
	float  (*distance)(struct TransInfo *, const float p1[3], const float p2[3]);
} TransSnap;

typedef struct TransCon {
	short orientation;	 /**/
	char  text[50];      /* Description of the Constraint for header_print                            */
	float mtx[3][3];     /* Matrix of the Constraint space                                            */
	float imtx[3][3];    /* Inverse Matrix of the Constraint space                                    */
	float pmtx[3][3];    /* Projection Constraint Matrix (same as imtx with some axis == 0)           */
	float center[3];     /* transformation center to define where to draw the view widget
	                      * ALWAYS in global space. Unlike the transformation center                  */
	int   imval[2];	     /* initial mouse value for visual calculation                                */
	                     /* the one in TransInfo is not garanty to stay the same (Rotates change it)  */
	int   mode;          /* Mode flags of the Constraint                                              */
	void  (*drawExtra)(struct TransInfo *t);
	                     /* For constraints that needs to draw differently from the other
	                      * uses this instead of the generic draw function                            */
	void  (*applyVec)(struct TransInfo *t, struct TransData *td, const float in[3], float out[3], float pvec[3]);
	                     /* Apply function pointer for linear vectorial transformation                */
	                     /* The last three parameters are pointers to the in/out/printable vectors    */
	void  (*applySize)(struct TransInfo *t, struct TransData *td, float smat[3][3]);
	                     /* Apply function pointer for size transformation */
	void  (*applyRot)(struct TransInfo *t, struct TransData *td, float vec[3], float *angle);
	                     /* Apply function pointer for rotation transformation */
} TransCon;

typedef struct TransDataExtension {
	float drot[3];		 /* Initial object drot */
	// float drotAngle;	 /* Initial object drotAngle,    TODO: not yet implemented */
	// float drotAxis[3];	 /* Initial object drotAxis, TODO: not yet implemented */
	float dquat[4];		 /* Initial object dquat */
	float dscale[3];     /* Initial object dscale */
	float *rot;          /* Rotation of the data to transform                                              */
	float  irot[3];      /* Initial rotation                                                               */
	float *quat;         /* Rotation quaternion of the data to transform                                   */
	float  iquat[4];	 /* Initial rotation quaternion                                                    */
	float *rotAngle;	 /* Rotation angle of the data to transform                                        */
	float  irotAngle;	 /* Initial rotation angle                                                         */
	float *rotAxis;		 /* Rotation axis of the data to transform                                         */
	float  irotAxis[4];	 /* Initial rotation axis                                                          */
	float *size;         /* Size of the data to transform                                                  */
	float  isize[3];	 /* Initial size                                                                   */
	float  obmat[4][4];	 /* Object matrix */
	float  l_smtx[3][3]; /* use instead of td->smtx, It is the same but without the 'bone->bone_mat', see TD_PBONE_LOCAL_MTX_C */
	float  r_mtx[3][3];  /* The rotscale matrix of pose bone, to allow using snap-align in translation mode,
	                      * when td->mtx is the loc pose bone matrix (and hence can't be used to apply rotation in some cases,
	                      * namely when a bone is in "NoLocal" or "Hinge" mode)... */
	float  r_smtx[3][3]; /* Invers of previous one. */
	int    rotOrder;	/* rotation mode,  as defined in eRotationModes (DNA_action_types.h) */
	float oloc[3], orot[3], oquat[4], orotAxis[3], orotAngle; /* Original object transformation used for rigid bodies */
} TransDataExtension;

typedef struct TransData2D {
	float loc[3];		/* Location of data used to transform (x,y,0) */
	float *loc2d;		/* Pointer to real 2d location of data */

	float *h1, *h2;     /* Pointer to handle locations, if handles aren't being moved independently */
	float ih1[2], ih2[2];
} TransData2D;

/* we need to store 2 handles for each transdata in case the other handle wasnt selected */
typedef struct TransDataCurveHandleFlags {
	char ih1, ih2;
	char *h1, *h2;
} TransDataCurveHandleFlags;

/* for sequencer transform */
typedef struct TransDataSeq {
	struct Sequence *seq;
	int flag;		/* a copy of seq->flag that may be modified for nested strips */
	int start_offset; /* use this so we can have transform data at the strips start, but apply correctly to the start frame  */
	short sel_flag; /* one of SELECT, SEQ_LEFTSEL and SEQ_RIGHTSEL */

} TransDataSeq;

/* for NLA transform (stored in td->extra pointer) */
typedef struct TransDataNla {
	ID *id;						/* ID-block NLA-data is attached to */
	
	struct NlaTrack *oldTrack;	/* Original NLA-Track that the strip belongs to */
	struct NlaTrack *nlt;		/* Current NLA-Track that the strip belongs to */
	
	struct NlaStrip *strip;		/* NLA-strip this data represents */
	
	/* dummy values for transform to write in - must have 3 elements... */
	float h1[3];				/* start handle */
	float h2[3];				/* end handle */
	
	int trackIndex;				/* index of track that strip is currently in */
	int handle;					/* handle-index: 0 for dummy entry, -1 for start, 1 for end, 2 for both ends */
} TransDataNla;

struct LinkNode;
struct GHash;

typedef struct TransDataEdgeSlideVert {
	struct BMVert *v_a, *v_b;
	struct BMVert *v;
	float v_co_orig[3];

	float edge_len;

	/* add origvert.co to get the original locations */
	float dir_a[3], dir_b[3];

	int loop_nr;
} TransDataEdgeSlideVert;

typedef struct EdgeSlideData {
	TransDataEdgeSlideVert *sv;
	int totsv;
	
	struct GHash *origfaces;

	int mval_start[2], mval_end[2];
	struct BMEditMesh *em;

	/* flag that is set when origfaces is initialized */
	bool use_origfaces;
	struct BMesh *bm_origfaces;

	float perc;

	bool is_proportional;
	bool flipped_vtx;

	int curr_sv_index;
} EdgeSlideData;


typedef struct TransDataVertSlideVert {
	BMVert *v;
	float   co_orig_3d[3];
	float   co_orig_2d[2];
	float (*co_link_orig_3d)[3];
	float (*co_link_orig_2d)[2];
	int     co_link_tot;
	int     co_link_curr;
} TransDataVertSlideVert;

typedef struct VertSlideData {
	TransDataVertSlideVert *sv;
	int totsv;

	struct BMEditMesh *em;

	float perc;

	bool is_proportional;
	bool flipped_vtx;

	int curr_sv_index;
} VertSlideData;

typedef struct BoneInitData {
	struct EditBone *bone;
	float tail[3];
	float rad_tail;
	float roll;
	float head[3];
	float dist;
	float xwidth;
	float zwidth;
} BoneInitData;

typedef struct TransData {
	float  dist;         /* Distance needed to affect element (for Proportionnal Editing)                  */
	float  rdist;        /* Distance to the nearest element (for Proportionnal Editing)                    */
	float  factor;       /* Factor of the transformation (for Proportionnal Editing)                       */
	float *loc;          /* Location of the data to transform                                              */
	float  iloc[3];      /* Initial location                                                               */
	float *val;          /* Value pointer for special transforms */
	float  ival;         /* Old value*/
	float  center[3];	 /* Individual data center                                                         */
	float  mtx[3][3];    /* Transformation matrix from data space to global space                          */
	float  smtx[3][3];   /* Transformation matrix from global space to data space                          */
	float  axismtx[3][3];/* Axis orientation matrix of the data                                            */
	struct Object *ob;
	struct bConstraint *con;	/* for objects/bones, the first constraint in its constraint stack */
	TransDataExtension *ext;	/* for objects, poses. 1 single malloc per TransInfo! */
	TransDataCurveHandleFlags *hdata; /* for curves, stores handle flags for modification/cancel */
	void  *extra;		 /* extra data (mirrored element pointer, in editmode mesh to BMVert) (editbone for roll fixing) (...) */
	int  flag;           /* Various flags */
	short  protectflag;	 /* If set, copy of Object or PoseChannel protection */
} TransData;

typedef struct MouseInput {
	void	(*apply)(struct TransInfo *t, struct MouseInput *mi, const int mval[2], float output[3]);
	void	(*post)(struct TransInfo *t, float values[3]);

	int     imval[2];       	/* initial mouse position                */
	bool	precision;
	int     precision_mval[2];	/* mouse position when precision key was pressed */
	float	center[2];
	float	factor;
	void 	*data; /* additional data, if needed by the particular function */
} MouseInput;

typedef struct TransInfo {
	int         mode;           /* current mode                         */
	int	        flag;           /* generic flags for special behaviors  */
	int			modifiers;		/* special modifiers, by function, not key */
	short		state;			/* current state (running, canceled,...)*/
	int         options;        /* current context/options for transform                      */
	float       val;            /* init value for some transformations (and rotation angle)  */
	float       fac;            /* factor for distance based transform  */
	void      (*transform)(struct TransInfo *, const int[2]);
								/* transform function pointer           */
	eRedrawFlag (*handleEvent)(struct TransInfo *, const struct wmEvent *);
								/* event handler function pointer  RETURN 1 if redraw is needed */
	int         total;          /* total number of transformed data     */
	TransData  *data;           /* transformed data (array)             */
	TransDataExtension *ext;	/* transformed data extension (array)   */
	TransData2D *data2d;		/* transformed data for 2d (array)      */
	TransCon    con;            /* transformed constraint               */
	TransSnap	tsnap;
	NumInput    num;            /* numerical input                      */
	MouseInput	mouse;			/* mouse input                          */
	eRedrawFlag redraw;         /* redraw flag                          */
	float		prop_size;		/* proportional circle radius           */
	char		proptext[20];	/* proportional falloff text			*/
	float       center[3];      /* center of transformation             */
	float       center2d[2];    /* center in screen coordinates         */
	int         imval[2];       /* initial mouse position               */
	short		event_type;		/* event->type used to invoke transform */
	short       idx_max;		/* maximum index on the input vector	*/
	float		snap[3];		/* Snapping Gears						*/
	char		frame_side;		/* Mouse side of the cfra, 'L', 'R' or 'B' */

	float		viewmat[4][4];	/* copy from G.vd, prevents feedback,   */
	float		viewinv[4][4];  /* and to make sure we don't have to    */
	float		persmat[4][4];  /* access G.vd from other space types   */
	float		persinv[4][4];
	short		persp;
	short		around;
	char		spacetype;		/* spacetype where transforming is      */
	char		helpline;		/* helpline modes (not to be confused with hotline) */

	float		vec[3];			/* translation, to show for widget   	*/
	float		mat[3][3];		/* rot/rescale, to show for widget   	*/

	float		spacemtx[3][3];	/* orientation matrix of the current space	*/
	char		spacename[64];	/* name of the current space, MAX_NAME		*/

	struct Object *poseobj;		/* if t->flag & T_POSE, this denotes pose object */

	void       *customData;		/* Per Transform custom data */
	void  	  (*customFree)(struct TransInfo *); /* if a special free function is needed */

	/*************** NEW STUFF *********************/
	short		launch_event; 	/* event type used to launch transform */

	short		current_orientation;
	short		twtype;			/* backup from view3d, to restore on end */

	short		prop_mode;
	
	short		mirror;

	float		values[4];
	float		auto_values[4];
	float		axis[3];
	float		axis_orig[3];	/* TransCon can change 'axis', store the original value here */

	bool		remove_on_cancel; /* remove elements if operator is canceled */

	void		*view;
	struct bContext *context; /* Only valid (non null) during an operator called function. */
	struct ScrArea	*sa;
	struct ARegion	*ar;
	struct Scene	*scene;
	struct ToolSettings *settings;
	struct wmTimer *animtimer;
	struct wmKeyMap *keymap;  /* so we can do lookups for header text */
	struct ReportList *reports;  /* assign from the operator, or can be NULL */
	int         mval[2];        /* current mouse position               */
	float       zfac;           /* use for 3d view */
	struct Object *obedit;
	float          obedit_mat[3][3]; /* normalized editmode matrix (T_EDIT only) */
	void		*draw_handle_apply;
	void		*draw_handle_view;
	void		*draw_handle_pixel;
	void		*draw_handle_cursor;
} TransInfo;


/* ******************** Macros & Prototypes *********************** */

/* transinfo->state */
#define TRANS_STARTING  0
#define TRANS_RUNNING	1
#define TRANS_CONFIRM	2
#define TRANS_CANCEL	3

/* transinfo->flag */
#define T_OBJECT		(1 << 0)
#define T_EDIT			(1 << 1)
#define T_POSE			(1 << 2)
#define T_TEXTURE		(1 << 3)
	/* transforming the camera while in camera view */
#define T_CAMERA		(1 << 4)
		 // trans on points, having no rotation/scale
#define T_POINTS		(1 << 6)
		// for manipulator exceptions, like scaling using center point, drawing help lines
#define T_USES_MANIPULATOR	(1 << 7)

	/* restrictions flags */
#define T_ALL_RESTRICTIONS	((1 << 8)|(1 << 9)|(1 << 10))
#define T_NO_CONSTRAINT		(1 << 8)
#define T_NULL_ONE			(1 << 9)
#define T_NO_ZERO			(1 << 10)

#define T_PROP_EDIT			(1 << 11)
#define T_PROP_CONNECTED	(1 << 12)
#define T_PROP_PROJECTED	(1 << 25)
#define T_PROP_EDIT_ALL		(T_PROP_EDIT | T_PROP_CONNECTED | T_PROP_PROJECTED)

#define T_V3D_ALIGN			(1 << 14)
	/* for 2d views like uv or ipo */
#define T_2D_EDIT			(1 << 15)
#define T_CLIP_UV			(1 << 16)

#define T_FREE_CUSTOMDATA	(1 << 17)
	/* auto-ik is on */
#define T_AUTOIK			(1 << 18)

#define T_MIRROR			(1 << 19)

#define T_AUTOVALUES		(1 << 20)

	/* to specificy if we save back settings at the end */
#define	T_MODAL				(1 << 21)

	/* no retopo */
#define T_NO_PROJECT		(1 << 22)

#define T_RELEASE_CONFIRM	(1 << 23)

	/* alternative transformation. used to add offset to tracking markers */
#define T_ALT_TRANSFORM		(1 << 24)

/* TransInfo->modifiers */
#define	MOD_CONSTRAINT_SELECT	0x01
#define	MOD_PRECISION			0x02
#define	MOD_SNAP				0x04
#define	MOD_SNAP_INVERT			0x08
#define	MOD_CONSTRAINT_PLANE	0x10

/* use node center for transform instead of upper-left corner.
 * disabled since it makes absolute snapping not work so nicely
 */
// #define USE_NODE_CENTER


/* ******************************************************************************** */

/* transinfo->helpline */
#define HLP_NONE		0
#define HLP_SPRING		1
#define HLP_ANGLE		2
#define HLP_HARROW		3
#define HLP_VARROW		4
#define HLP_TRACKBALL	5

/* transinfo->con->mode */
#define CON_APPLY		1
#define CON_AXIS0		2
#define CON_AXIS1		4
#define CON_AXIS2		8
#define CON_SELECT		16
#define CON_NOFLIP		32	/* does not reorient vector to face viewport when on */
#define CON_USER		64

/* transdata->flag */
#define TD_SELECTED			1
#define	TD_NOACTION			(1 << 2)
#define	TD_USEQUAT			(1 << 3)
#define TD_NOTCONNECTED		(1 << 4)
#define TD_SINGLESIZE		(1 << 5)	/* used for scaling of MetaElem->rad */
#define TD_INDIVIDUAL_SCALE	(1 << 8) /* Scale relative to individual element center */
#define TD_NOCENTER			(1 << 9)
#define TD_NO_EXT			(1 << 10)	/* ext abused for particle key timing */
#define TD_SKIP				(1 << 11)	/* don't transform this data */
#define TD_BEZTRIPLE		(1 << 12)	/* if this is a bez triple, we need to restore the handles, if this is set transdata->misc.hdata needs freeing */
#define TD_NO_LOC			(1 << 13)	/* when this is set, don't apply translation changes to this element */
#define TD_NOTIMESNAP		(1 << 14)	/* for Graph Editor autosnap, indicates that point should not undergo autosnapping */
#define TD_INTVALUES		(1 << 15) 	/* for Graph Editor - curves that can only have int-values need their keyframes tagged with this */
#define TD_MIRROR_EDGE		(1 << 16) 	/* For editmode mirror, clamp to x = 0 */
#define TD_MOVEHANDLE1		(1 << 17)	/* For fcurve handles, move them along with their keyframes */
#define TD_MOVEHANDLE2		(1 << 18)
#define TD_PBONE_LOCAL_MTX_P (1 << 19)	/* exceptional case with pose bone rotating when a parent bone has 'Local Location' option enabled and rotating also transforms it. */
#define TD_PBONE_LOCAL_MTX_C (1 << 20)	/* same as above but for a child bone */

/* transsnap->status */
#define SNAP_FORCED		1
#define TARGET_INIT		2
#define POINT_INIT		4
#define MULTI_POINTS	8

bool initTransform(struct bContext *C, struct TransInfo *t, struct wmOperator *op, const struct wmEvent *event, int mode);
void saveTransform(struct bContext *C, struct TransInfo *t, struct wmOperator *op);
int  transformEvent(TransInfo *t, const struct wmEvent *event);
void transformApply(struct bContext *C, TransInfo *t);
int  transformEnd(struct bContext *C, TransInfo *t);

void setTransformViewMatrices(TransInfo *t);
void convertViewVec(TransInfo *t, float r_vec[3], int dx, int dy);
void projectIntViewEx(TransInfo *t, const float vec[3], int adr[2], const eV3DProjTest flag);
void projectIntView(TransInfo *t, const float vec[3], int adr[2]);
void projectFloatViewEx(TransInfo *t, const float vec[3], float adr[2], const eV3DProjTest flag);
void projectFloatView(TransInfo *t, const float vec[3], float adr[2]);

void applyAspectRatio(TransInfo *t, float vec[2]);
void removeAspectRatio(TransInfo *t, float vec[2]);

void drawPropCircle(const struct bContext *C, TransInfo *t);

struct wmKeyMap *transform_modal_keymap(struct wmKeyConfig *keyconf);


/*********************** transform_conversions.c ********** */
struct ListBase;

void flushTransIntFrameActionData(TransInfo *t);
void flushTransGraphData(TransInfo *t);
void remake_graph_transdata(TransInfo *t, struct ListBase *anim_data);
void flushTransUVs(TransInfo *t);
void flushTransParticles(TransInfo *t);
bool clipUVTransform(TransInfo *t, float vec[2], const bool resize);
void clipUVData(TransInfo *t);
void flushTransNodes(TransInfo *t);
void flushTransSeq(TransInfo *t);
void flushTransTracking(TransInfo *t);
void flushTransMasking(TransInfo *t);
void flushTransPaintCurve(TransInfo *t);
void restoreBones(TransInfo *t);

/*********************** exported from transform_manipulator.c ********** */
bool gimbal_axis(struct Object *ob, float gmat[3][3]); /* return 0 when no gimbal for selection */

/*********************** TransData Creation and General Handling *********** */
void createTransData(struct bContext *C, TransInfo *t);
void sort_trans_data_dist(TransInfo *t);
void special_aftertrans_update(struct bContext *C, TransInfo *t);
int  special_transform_moving(TransInfo *t);

void transform_autoik_update(TransInfo *t, short mode);

int count_set_pose_transflags(int *out_mode, short around, struct Object *ob);

/* auto-keying stuff used by special_aftertrans_update */
void autokeyframe_ob_cb_func(struct bContext *C, struct Scene *scene, struct View3D *v3d, struct Object *ob, int tmode);
void autokeyframe_pose_cb_func(struct bContext *C, struct Scene *scene, struct View3D *v3d, struct Object *ob, int tmode, short targetless_ik);

/*********************** Constraints *****************************/

void drawConstraint(TransInfo *t);

void getConstraintMatrix(TransInfo *t);
void setConstraint(TransInfo *t, float space[3][3], int mode, const char text[]);
void setAxisMatrixConstraint(TransInfo *t, int mode, const char text[]);
void setLocalConstraint(TransInfo *t, int mode, const char text[]);
void setUserConstraint(TransInfo *t, short orientation, int mode, const char text[]);

void constraintNumInput(TransInfo *t, float vec[3]);

bool isLockConstraint(TransInfo *t);
int  getConstraintSpaceDimension(TransInfo *t);
char constraintModeToChar(TransInfo *t);

void startConstraint(TransInfo *t);
void stopConstraint(TransInfo *t);

void initSelectConstraint(TransInfo *t, float mtx[3][3]);
void selectConstraint(TransInfo *t);
void postSelectConstraint(TransInfo *t);

void setNearestAxis(TransInfo *t);

/*********************** Snapping ********************************/

typedef enum {
	NO_GEARS 	= 0,
	BIG_GEARS	= 1,
	SMALL_GEARS	= 2
} GearsType;

void snapGridIncrement(TransInfo *t, float *val);
void snapGridIncrementAction(TransInfo *t, float *val, GearsType action);

bool activeSnap(TransInfo *t);
bool validSnap(TransInfo *t);

void initSnapping(struct TransInfo *t, struct wmOperator *op);
void applyProject(TransInfo *t);
void applyGridAbsolute(TransInfo *t);
void applySnapping(TransInfo *t, float *vec);
void resetSnapping(TransInfo *t);
eRedrawFlag handleSnapping(TransInfo *t, const struct wmEvent *event);
void drawSnapping(const struct bContext *C, TransInfo *t);
bool usingSnappingNormal(TransInfo *t);
bool validSnappingNormal(TransInfo *t);

void getSnapPoint(TransInfo *t, float vec[3]);
void addSnapPoint(TransInfo *t);
eRedrawFlag updateSelectedSnapPoint(TransInfo *t);
void removeSnapPoint(TransInfo *t);

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

void initMouseInput(TransInfo *t, MouseInput *mi, const float center[2], const int mval[2]);
void initMouseInputMode(TransInfo *t, MouseInput *mi, MouseInputMode mode);
eRedrawFlag handleMouseInput(struct TransInfo *t, struct MouseInput *mi, const struct wmEvent *event);
void applyMouseInput(struct TransInfo *t, struct MouseInput *mi, const int mval[2], float output[3]);

void setCustomPoints(TransInfo *t, MouseInput *mi, const int start[2], const int end[2]);
void setInputPostFct(MouseInput *mi, void	(*post)(struct TransInfo *t, float values[3]));

/*********************** Generics ********************************/

void initTransInfo(struct bContext *C, TransInfo *t, struct wmOperator *op, const struct wmEvent *event);
void postTrans(struct bContext *C, TransInfo *t);
void resetTransModal(TransInfo *t);
void resetTransRestrictions(TransInfo *t);

void drawLine(TransInfo *t, const float center[3], const float dir[3], char axis, short options);

/* DRAWLINE options flags */
#define DRAWLIGHT	1

void applyTransObjects(TransInfo *t);
void restoreTransObjects(TransInfo *t);
void recalcData(TransInfo *t);

void calculateCenter2D(TransInfo *t);

void calculateCenter(TransInfo *t);

/* API functions for getting center points */
void calculateCenterBound(TransInfo *t, float r_center[3]);
void calculateCenterMedian(TransInfo *t, float r_center[3]);
void calculateCenterCursor(TransInfo *t, float r_center[3]);
void calculateCenterCursor2D(TransInfo *t, float r_center[2]);
void calculateCenterCursorGraph2D(TransInfo *t, float r_center[2]);
bool calculateCenterActive(TransInfo *t, bool select_only, float r_center[3]);

void calculatePropRatio(TransInfo *t);

void getViewVector(TransInfo *t, float coord[3], float vec[3]);

/*********************** Transform Orientations ******************************/

void initTransformOrientation(struct bContext *C, TransInfo *t);

/* Those two fill in mat and return non-zero on success */
bool createSpaceNormal(float mat[3][3], const float normal[3]);
bool createSpaceNormalTangent(float mat[3][3], const float normal[3], const float tangent[3]);

struct TransformOrientation *addMatrixSpace(struct bContext *C, float mat[3][3],
                                            const char *name, const bool overwrite);
bool applyTransformOrientation(const struct bContext *C, float mat[3][3], char r_name[64]);

#define ORIENTATION_NONE	0
#define ORIENTATION_NORMAL	1
#define ORIENTATION_VERT	2
#define ORIENTATION_EDGE	3
#define ORIENTATION_FACE	4

int getTransformOrientation(const struct bContext *C, float normal[3], float plane[3], const bool activeOnly);

void freeEdgeSlideTempFaces(EdgeSlideData *sld);
void freeEdgeSlideVerts(TransInfo *t);
void projectEdgeSlideData(TransInfo *t, bool is_final);

void freeVertSlideVerts(TransInfo *t);


/* TODO. transform_queries.c */
bool checkUseAxisMatrix(TransInfo *t);

#endif
