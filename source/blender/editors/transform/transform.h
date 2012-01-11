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


#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "ED_transform.h"
#include "ED_numinput.h"

#include "DNA_listBase.h"

#include "BLI_editVert.h"

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
struct bPose;
struct bConstraint;
struct BezTriple;
struct wmOperatorType;
struct wmOperator;
struct wmWindowManager;
struct wmKeyMap;
struct wmKeyConfig;
struct bContext;
struct wmEvent;
struct wmTimer;
struct ARegion;
struct ReportList;

typedef struct TransSnapPoint {
	struct TransSnapPoint *next,*prev;
	float co[3];
} TransSnapPoint;

typedef struct TransSnap {
	short	mode;
	short	target;
	short	modePoint;
	short	modeSelect;
	short	align;
	char	project;
	char	snap_self;
	short	peel;
	short  	status;
	float	snapPoint[3]; /* snapping from this point */
	float	snapTarget[3]; /* to this point */
	float	snapNormal[3];
	float	snapTangent[3];
	ListBase points;
	TransSnapPoint	*selectedPoint;
	float	dist; // Distance from snapPoint to snapTarget
	double	last;
	void  (*applySnap)(struct TransInfo *, float *);
	void  (*calcSnap)(struct TransInfo *, float *);
	void  (*targetSnap)(struct TransInfo *);
	float  (*distance)(struct TransInfo *, float p1[3], float p2[3]); // Get the transform distance between two points (used by Closest snap)
} TransSnap;

typedef struct TransCon {
	short orientation;	 /**/
	char  text[50];      /* Description of the Constraint for header_print                            */
	float mtx[3][3];     /* Matrix of the Constraint space                                            */
	float imtx[3][3];    /* Inverse Matrix of the Constraint space                                    */
	float pmtx[3][3];    /* Projection Constraint Matrix (same as imtx with some axis == 0)           */
	float center[3];     /* transformation center to define where to draw the view widget
							ALWAYS in global space. Unlike the transformation center                  */
	int   imval[2];	     /* initial mouse value for visual calculation                                */
						 /* the one in TransInfo is not garanty to stay the same (Rotates change it)  */
	int   mode;          /* Mode flags of the Constraint                                              */
	void  (*drawExtra)(struct TransInfo *);
						 /* For constraints that needs to draw differently from the other
							uses this instead of the generic draw function							  */
	void  (*applyVec)(struct TransInfo *, struct TransData *, float *, float *, float *);
						 /* Apply function pointer for linear vectorial transformation                */
						 /* The last three parameters are pointers to the in/out/printable vectors    */
	void  (*applySize)(struct TransInfo *, struct TransData *, float [3][3]);
						 /* Apply function pointer for size transformation */
	void  (*applyRot)(struct TransInfo *, struct TransData *, float [3], float *);
						 /* Apply function pointer for rotation transformation */
} TransCon;

typedef struct TransDataExtension {
	float drot[3];		 /* Initial object drot */
	// float drotAngle;	 /* Initial object drotAngle,    TODO: not yet implimented */
	// float drotAxis[3];	 /* Initial object drotAxis, TODO: not yet implimented */
	float dquat[4];		 /* Initial object dquat */
	float dscale[3];     /* Initial object dscale */
	float *rot;          /* Rotation of the data to transform (Faculative)                                 */
	float  irot[3];      /* Initial rotation                                                               */
	float *quat;         /* Rotation quaternion of the data to transform (Faculative)                      */
	float  iquat[4];	 /* Initial rotation quaternion                                                    */
	float *rotAngle;	 /* Rotation angle of the data to transform (Faculative) 						 */
	float  irotAngle;	 /* Initial rotation angle 												 */
	float *rotAxis;		 /* Rotation axis of the data to transform (Faculative) 						 */
	float  irotAxis[4];	 /* Initial rotation axis 													 */
	float *size;         /* Size of the data to transform (Faculative)                                     */
	float  isize[3];	 /* Initial size                                                                   */
	float  obmat[4][4];	 /* Object matrix */
	float  l_smtx[3][3]; /* use instead of td->smtx, It is the same but without the 'bone->bone_mat', see TD_PBONE_LOCAL_MTX_C */
	int    rotOrder;	/* rotation mode,  as defined in eRotationModes (DNA_action_types.h) */
} TransDataExtension;

typedef struct TransData2D {
	float loc[3];		/* Location of data used to transform (x,y,0) */
	float *loc2d;		/* Pointer to real 2d location of data */

	float *h1, *h2;     /* Pointer to handle locations, if handles aren't being moved independently */
	float ih1[2], ih2[2];
} TransData2D;

/* we need to store 2 handles for each transdata incase the other handle wasnt selected */
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
struct EditEdge;
struct EditVert;
struct GHash;
typedef struct TransDataSlideUv {
	float origuv[2];
	float *uv_up, *uv_down;
	//float *fuv[4];
	struct LinkNode *fuv_list;
} TransDataSlideUv;

typedef struct TransDataSlideVert {
	struct EditEdge *up, *down;
	struct EditVert origvert;
} TransDataSlideVert;

typedef struct SlideData {
	TransDataSlideUv *slideuv, *suv_last;
	int totuv, uvlay_tot;
	struct GHash *vhash, **uvhash;
	struct EditVert *nearest;
	struct LinkNode *edgelist, *vertlist;
	int start[2], end[2];
} SlideData;

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
	void  *extra;		 /* extra data (mirrored element pointer, in editmode mesh to EditVert) (editbone for roll fixing) (...) */
	int  flag;         /* Various flags */
	short  protectflag;	 /* If set, copy of Object or PoseChannel protection */
} TransData;

typedef struct MouseInput {
	void	(*apply)(struct TransInfo *, struct MouseInput *, const int [2], float [3]);
	void	(*post)(struct TransInfo *, float [3]);

	int     imval[2];       	/* initial mouse position                */
	char	precision;
	int     precision_mval[2];	/* mouse position when precision key was pressed */
	int		center[2];
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
	int       (*transform)(struct TransInfo *, const int *);
								/* transform function pointer           */
	int       (*handleEvent)(struct TransInfo *, struct wmEvent *);
								/* event handler function pointer  RETURN 1 if redraw is needed */
	int         total;          /* total number of transformed data     */
	TransData  *data;           /* transformed data (array)             */
	TransDataExtension *ext;	/* transformed data extension (array)   */
	TransData2D *data2d;		/* transformed data for 2d (array)      */
	TransCon    con;            /* transformed constraint               */
	TransSnap	tsnap;
	NumInput    num;            /* numerical input                      */
	MouseInput	mouse;			/* mouse input                          */
	char        redraw;         /* redraw flag                          */
	float		prop_size;		/* proportional circle radius           */
	char		proptext[20];	/* proportional falloff text			*/
	float       center[3];      /* center of transformation             */
	int         center2d[2];    /* center in screen coordinates         */
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

	char		*undostr;		/* if set, uses this string for undo		*/
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

	void		*view;
	struct bContext *context; /* Only valid (non null) during an operator called function. */
	struct ScrArea	*sa;
	struct ARegion	*ar;
	struct Scene	*scene;
	struct ToolSettings *settings;
	struct wmTimer *animtimer;
	int         mval[2];        /* current mouse position               */
	struct Object   *obedit;
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

/* transinfo->redraw */
#define TREDRAW_NOTHING  	0
#define TREDRAW_HARD		1
#define TREDRAW_SOFT		2


/* transinfo->flag */
#define T_OBJECT		(1 << 0)
#define T_EDIT			(1 << 1)
#define T_POSE			(1 << 2)
#define T_TEXTURE		(1 << 3)
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
#define TD_ACTIVE			(1 << 1)
#define	TD_NOACTION			(1 << 2)
#define	TD_USEQUAT			(1 << 3)
#define TD_NOTCONNECTED		(1 << 4)
#define TD_SINGLESIZE		(1 << 5)	/* used for scaling of MetaElem->rad */
/*#define TD_TIMEONLY			(1 << 8) */ /*UNUSED*/
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

void TRANSFORM_OT_transform(struct wmOperatorType *ot);

int initTransform(struct bContext *C, struct TransInfo *t, struct wmOperator *op, struct wmEvent *event, int mode);
void saveTransform(struct bContext *C, struct TransInfo *t, struct wmOperator *op);
int  transformEvent(TransInfo *t, struct wmEvent *event);
void transformApply(struct bContext *C, TransInfo *t);
int  transformEnd(struct bContext *C, TransInfo *t);

void setTransformViewMatrices(TransInfo *t);
void convertViewVec(TransInfo *t, float *vec, int dx, int dy);
void projectIntView(TransInfo *t, float *vec, int *adr);
void projectFloatView(TransInfo *t, float *vec, float *adr);

void applyAspectRatio(TransInfo *t, float *vec);
void removeAspectRatio(TransInfo *t, float *vec);

void initWarp(TransInfo *t);
int handleEventWarp(TransInfo *t, struct wmEvent *event);
int Warp(TransInfo *t, const int mval[2]);

void initShear(TransInfo *t);
int handleEventShear(TransInfo *t, struct wmEvent *event);
int Shear(TransInfo *t, const int mval[2]);

void initResize(TransInfo *t);
int Resize(TransInfo *t, const int mval[2]);

void initTranslation(TransInfo *t);
int Translation(TransInfo *t, const int mval[2]);

void initToSphere(TransInfo *t);
int ToSphere(TransInfo *t, const int mval[2]);

void initRotation(TransInfo *t);
int Rotation(TransInfo *t, const int mval[2]);

void initShrinkFatten(TransInfo *t);
int ShrinkFatten(TransInfo *t, const int mval[2]);

void initTilt(TransInfo *t);
int Tilt(TransInfo *t, const int mval[2]);

void initCurveShrinkFatten(TransInfo *t);
int CurveShrinkFatten(TransInfo *t, const int mval[2]);

void initTrackball(TransInfo *t);
int Trackball(TransInfo *t, const int mval[2]);

void initPushPull(TransInfo *t);
int PushPull(TransInfo *t, const int mval[2]);

void initBevel(TransInfo *t);
int handleEventBevel(TransInfo *t, struct wmEvent *event);
int Bevel(TransInfo *t, const int mval[2]);

void initBevelWeight(TransInfo *t);
int BevelWeight(TransInfo *t, const int mval[2]);

void initCrease(TransInfo *t);
int Crease(TransInfo *t, const int mval[2]);

void initBoneSize(TransInfo *t);
int BoneSize(TransInfo *t, const int mval[2]);

void initBoneEnvelope(TransInfo *t);
int BoneEnvelope(TransInfo *t, const int mval[2]);

void initBoneRoll(TransInfo *t);
int BoneRoll(TransInfo *t, const int mval[2]);

void initEdgeSlide(TransInfo *t);
int EdgeSlide(TransInfo *t, const int mval[2]);

void initTimeTranslate(TransInfo *t);
int TimeTranslate(TransInfo *t, const int mval[2]);

void initTimeSlide(TransInfo *t);
int TimeSlide(TransInfo *t, const int mval[2]);

void initTimeScale(TransInfo *t);
int TimeScale(TransInfo *t, const int mval[2]);

void initBakeTime(TransInfo *t);
int BakeTime(TransInfo *t, const int mval[2]);

void initMirror(TransInfo *t);
int Mirror(TransInfo *t, const int mval[2]);

void initAlign(TransInfo *t);
int Align(TransInfo *t, const int mval[2]);

void initSeqSlide(TransInfo *t);
int SeqSlide(TransInfo *t, const int mval[2]);

void drawPropCircle(const struct bContext *C, TransInfo *t);

struct wmKeyMap *transform_modal_keymap(struct wmKeyConfig *keyconf);


/*********************** transform_conversions.c ********** */
struct ListBase;

void flushTransGPactionData(TransInfo *t);
void flushTransGraphData(TransInfo *t);
void remake_graph_transdata(TransInfo *t, struct ListBase *anim_data);
void flushTransUVs(TransInfo *t);
void flushTransParticles(TransInfo *t);
int clipUVTransform(TransInfo *t, float *vec, int resize);
void flushTransNodes(TransInfo *t);
void flushTransSeq(TransInfo *t);
void flushTransTracking(TransInfo *t);

/*********************** exported from transform_manipulator.c ********** */
int gimbal_axis(struct Object *ob, float gmat[][3]); /* return 0 when no gimbal for selection */
int calc_manipulator_stats(const struct bContext *C);

/*********************** TransData Creation and General Handling *********** */
void createTransData(struct bContext *C, TransInfo *t);
void sort_trans_data_dist(TransInfo *t);
void add_tdi_poin(float *poin, float *old, float delta);
void special_aftertrans_update(struct bContext *C, TransInfo *t);

void transform_autoik_update(TransInfo *t, short mode);

int count_set_pose_transflags(int *out_mode, short around, struct Object *ob);

/* auto-keying stuff used by special_aftertrans_update */
void autokeyframe_ob_cb_func(struct bContext *C, struct Scene *scene, struct View3D *v3d, struct Object *ob, int tmode);
void autokeyframe_pose_cb_func(struct bContext *C, struct Scene *scene, struct View3D *v3d, struct Object *ob, int tmode, short targetless_ik);

/*********************** Constraints *****************************/

void drawConstraint(TransInfo *t);

void getConstraintMatrix(TransInfo *t);
void setConstraint(TransInfo *t, float space[3][3], int mode, const char text[]);
void setLocalConstraint(TransInfo *t, int mode, const char text[]);
void setUserConstraint(TransInfo *t, short orientation, int mode, const char text[]);

void constraintNumInput(TransInfo *t, float vec[3]);

int isLockConstraint(TransInfo *t);
int getConstraintSpaceDimension(TransInfo *t);
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

void snapGrid(TransInfo *t, float *val);
void snapGridAction(TransInfo *t, float *val, GearsType action);

int activeSnap(TransInfo *t);
int validSnap(TransInfo *t);

void initSnapping(struct TransInfo *t, struct wmOperator *op);
void applyProject(TransInfo *t);
void applySnapping(TransInfo *t, float *vec);
void resetSnapping(TransInfo *t);
int  handleSnapping(TransInfo *t, struct wmEvent *event);
void drawSnapping(const struct bContext *C, TransInfo *t);
int usingSnappingNormal(TransInfo *t);
int validSnappingNormal(TransInfo *t);

void getSnapPoint(TransInfo *t, float vec[3]);
void addSnapPoint(TransInfo *t);
int updateSelectedSnapPoint(TransInfo *t);
void removeSnapPoint(TransInfo *t);

/********************** Mouse Input ******************************/

typedef enum {
	INPUT_NONE,
	INPUT_VECTOR,
	INPUT_SPRING,
	INPUT_SPRING_FLIP,
	INPUT_ANGLE,
	INPUT_TRACKBALL,
	INPUT_HORIZONTAL_RATIO,
	INPUT_HORIZONTAL_ABSOLUTE,
	INPUT_VERTICAL_RATIO,
	INPUT_VERTICAL_ABSOLUTE,
	INPUT_CUSTOM_RATIO
} MouseInputMode;

void initMouseInput(TransInfo *t, MouseInput *mi, int center[2], int mval[2]);
void initMouseInputMode(TransInfo *t, MouseInput *mi, MouseInputMode mode);
int handleMouseInput(struct TransInfo *t, struct MouseInput *mi, struct wmEvent *event);
void applyMouseInput(struct TransInfo *t, struct MouseInput *mi, const int mval[2], float output[3]);

void setCustomPoints(TransInfo *t, MouseInput *mi, int start[2], int end[2]);
void setInputPostFct(MouseInput *mi, void	(*post)(struct TransInfo *, float [3]));

/*********************** Generics ********************************/

int initTransInfo(struct bContext *C, TransInfo *t, struct wmOperator *op, struct wmEvent *event);
void postTrans (struct bContext *C, TransInfo *t);
void resetTransRestrictions(TransInfo *t);

void drawLine(TransInfo *t, float *center, float *dir, char axis, short options);

TransDataCurveHandleFlags *initTransDataCurveHandes(TransData *td, struct BezTriple *bezt);

/* DRAWLINE options flags */
#define DRAWLIGHT	1

void applyTransObjects(TransInfo *t);
void restoreTransObjects(TransInfo *t);
void restoreTransNodes(TransInfo *t);
void recalcData(TransInfo *t);

void calculateCenter(TransInfo *t);
void calculateCenter2D(TransInfo *t);
void calculateCenterBound(TransInfo *t);
void calculateCenterMedian(TransInfo *t);
void calculateCenterCursor(TransInfo *t);

void calculateCenterCursor2D(TransInfo *t);
void calculatePropRatio(TransInfo *t);

void getViewVector(TransInfo *t, float coord[3], float vec[3]);

/*********************** Transform Orientations ******************************/

void initTransformOrientation(struct bContext *C, TransInfo *t);

struct TransformOrientation *createObjectSpace(struct bContext *C, struct ReportList *reports, char *name, int overwrite);
struct TransformOrientation *createMeshSpace(struct bContext *C, struct ReportList *reports, char *name, int overwrite);
struct TransformOrientation *createBoneSpace(struct bContext *C, struct ReportList *reports, char *name, int overwrite);

/* Those two fill in mat and return non-zero on success */
int createSpaceNormal(float mat[3][3], float normal[3]);
int createSpaceNormalTangent(float mat[3][3], float normal[3], float tangent[3]);

struct TransformOrientation *addMatrixSpace(struct bContext *C, float mat[3][3], char name[], int overwrite);
int addObjectSpace(struct bContext *C, struct Object *ob);
void applyTransformOrientation(const struct bContext *C, float mat[3][3], char *name);

#define ORIENTATION_NONE	0
#define ORIENTATION_NORMAL	1
#define ORIENTATION_VERT	2
#define ORIENTATION_EDGE	3
#define ORIENTATION_FACE	4

int getTransformOrientation(const struct bContext *C, float normal[3], float plane[3], int activeOnly);

void freeSlideVerts(TransInfo *t);

#endif
