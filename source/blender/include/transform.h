/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "BIF_transform.h"

/* ************************** Types ***************************** */

struct TransInfo;
struct TransData;
struct TransSnap;
struct NumInput;
struct Object;
struct View3D;
struct ScrArea;
struct bPose;
struct bConstraint;
struct BezTriple;

typedef struct NDofInput {
	int		flag;
	int		axis;
	float	fval[7];
	float	factor[3];
} NDofInput;

typedef struct NumInput {
    short  idx;
    short  idx_max;
    short  flag;        /* Different flags to indicate different behaviors                                */
    float  val[3];       /* Direct value of the input                                                      */
    int  ctrl[3];      /* Control to indicate what to do with the numbers that are typed                 */
} NumInput ;

/*
	The ctrl value has different meaning:
		0			: No value has been typed
		
		otherwise, |value| - 1 is where the cursor is located after the period
		Positive	: number is positive
		Negative	: number is negative
*/

typedef struct TransSnap {
	short	modePoint;
	short	modeTarget;
	int  	status;
	float	snapPoint[3];
	float	snapTarget[3];
	float	snapNormal[3];
	float	snapTangent[3];
	float	dist; // Distance from snapPoint to snapTarget
	double	last;
	void  (*applySnap)(struct TransInfo *, float *);
	void  (*calcSnap)(struct TransInfo *, float *);
	void  (*targetSnap)(struct TransInfo *);
	float  (*distance)(struct TransInfo *, float p1[3], float p2[3]); // Get the transform distance between two points (used by Closest snap)
} TransSnap;

typedef struct TransCon {
    char  text[50];      /* Description of the Constraint for header_print                            */
    float mtx[3][3];     /* Matrix of the Constraint space                                            */
    float imtx[3][3];    /* Inverse Matrix of the Constraint space                                    */
    float pmtx[3][3];    /* Projection Constraint Matrix (same as imtx with some axis == 0)           */
    float center[3];     /* transformation center to define where to draw the view widget             
                            ALWAYS in global space. Unlike the transformation center                  */
	short imval[2];	     /* initial mouse value for visual calculation                                */
	                     /* the one in TransInfo is not garanty to stay the same (Rotates change it)  */
    int   mode;          /* Mode flags of the Constraint                                              */
	void  (*drawExtra)(struct TransInfo *);
						 /* For constraints that needs to draw differently from the other
							uses this instead of the generic draw function							  */
    void  (*applyVec)(struct TransInfo *, struct TransData *, float *, float *, float *);
                         /* Apply function pointer for linear vectorial transformation                */
                         /* The last three parameters are pointers to the in/out/printable vectors    */
    void  (*applySize)(struct TransInfo *, struct TransData *, float [3][3]);
                         /* Apply function pointer for rotation transformation (prototype will change */
    void  (*applyRot)(struct TransInfo *, struct TransData *, float [3]);
                         /* Apply function pointer for rotation transformation (prototype will change */
} TransCon;

typedef struct TransDataIpokey {
	int flag;					/* which keys */
	float *locx, *locy, *locz;	/* channel pointers */
	float *rotx, *roty, *rotz;
	float *quatx, *quaty, *quatz, *quatw;
	float *sizex, *sizey, *sizez;
	float oldloc[9];			/* storage old values */
	float oldrot[9];
	float oldsize[9];
	float oldquat[12];
} TransDataIpokey;

typedef struct TransDataExtension {
	float drot[3];		 /* Initial object drot */
	float dsize[3];		 /* Initial object dsize */
    float *rot;          /* Rotation of the data to transform (Faculative)                                 */
    float  irot[3];      /* Initial rotation                                                               */
    float *quat;         /* Rotation quaternion of the data to transform (Faculative)                      */
    float  iquat[4];	 /* Initial rotation quaternion                                                    */
    float *size;         /* Size of the data to transform (Faculative)                                     */
    float  isize[3];	 /* Initial size                                                                   */
	float  obmat[4][4];	 /* Object matrix */  
} TransDataExtension;

typedef struct TransData2D {
	float loc[3];		/* Location of data used to transform (x,y,0) */
	float *loc2d;		/* Pointer to real 2d location of data */
} TransData2D;

/* we need to store 2 handles for each transdata incase the other handle wasnt selected */
typedef struct TransDataCurveHandleFlags {
	short ih1, ih2;
	short *h1, *h2;
} TransDataCurveHandleFlags;


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
	TransDataIpokey *tdi;		/* for objects, ipo keys. per transdata a malloc */
	TransDataCurveHandleFlags *hdata; /* for curves, stores handle flags for modification/cancel */
	void *tdmir;		 /* mirrored element pointer, in editmode mesh to EditVert */
    short  flag;         /* Various flags */
	short  protectflag;	 /* If set, copy of Object or PoseChannel protection */
/*#ifdef WITH_VERSE*/
	void *verse;			/* pointer at verse data struct (VerseVert, etc.) */
/*#endif*/
} TransData;

typedef struct TransInfo {
    int         mode;           /* current mode                         */
    int	        flag;           /* generic flags for special behaviors  */
	short		state;			/* current state (running, canceled,...)*/
    int         context;        /* current context                      */
    float       val;            /* init value for some transformations (and rotation angle)  */
    float       fac;            /* factor for distance based transform  */
    int       (*transform)(struct TransInfo *, short *);
                                /* transform function pointer           */
	int       (*handleEvent)(struct TransInfo *, unsigned short event, short val);
								/* event handler function pointer  RETURN 1 if redraw is needed */
    int         total;          /* total number of transformed data     */
    TransData  *data;           /* transformed data (array)             */
	TransDataExtension *ext;	/* transformed data extension (array)   */
	TransData2D *data2d;		/* transformed data for 2d (array)      */
    TransCon    con;            /* transformed constraint               */
    TransSnap	tsnap;
    NumInput    num;            /* numerical input                      */
    NDofInput   ndof;           /* ndof input                           */
    char        redraw;         /* redraw flag                          */
	float		propsize;		/* proportional circle radius           */
	char		proptext[20];	/* proportional falloff text			*/
    float       center[3];      /* center of transformation             */
    int         center2d[2];    /* center in screen coordinates         */
    short       imval[2];       /* initial mouse position               */
	short		shiftmval[2];	/* mouse position when shift was pressed */
	short       idx_max;		/* maximum index on the input vector	*/
	float		snap[3];		/* Snapping Gears						*/
	
	float		viewmat[4][4];	/* copy from G.vd, prevents feedback,   */
	float		viewinv[4][4];  /* and to make sure we don't have to    */
	float		persmat[4][4];  /* access G.vd from other space types   */
	float		persinv[4][4];
	short		persp;
	short		around;
	char		spacetype;		/* spacetype where transforming is      */
	
	float		vec[3];			/* translation, to show for widget   	*/
	float		mat[3][3];		/* rot/rescale, to show for widget   	*/
	
	char		*undostr;		/* if set, uses this string for undo		*/
	float		spacemtx[3][3];	/* orientation matrix of the current space	*/
	char		spacename[32];	/* name of the current space				*/
	
	struct Object *poseobj;		/* if t->flag & T_POSE, this denotes pose object */
	
	void       *customData;		/* Per Transform custom data */
} TransInfo;


/* ******************** Macros & Prototypes *********************** */

/* NUMINPUT FLAGS */
#define NUM_NULL_ONE		2
#define NUM_NO_NEGATIVE		4
#define	NUM_NO_ZERO			8
#define NUM_NO_FRACTION		16
#define	NUM_AFFECT_ALL		32

/* NDOFINPUT FLAGS */
#define NDOF_INIT			1

/* transinfo->state */
#define TRANS_RUNNING	0
#define TRANS_CONFIRM	1
#define TRANS_CANCEL	2

/* transinfo->flag */
#define T_OBJECT		(1 << 0)
#define T_EDIT			(1 << 1)
#define T_POSE			(1 << 2)
#define T_TEXTURE		(1 << 3)
#define T_CAMERA		(1 << 4)
		// when shift pressed, higher resolution transform. cannot rely on G.qual, need event!
#define T_SHIFT_MOD		(1 << 5)
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

	/* if MMB is pressed or not */
#define	T_MMB_PRESSED		(1 << 13)

#define T_V3D_ALIGN			(1 << 14)
	/* for 2d views like uv or ipo */
#define T_2D_EDIT			(1 << 15) 
#define T_CLIP_UV			(1 << 16)

#define T_FREE_CUSTOMDATA	(1 << 17)
	/* auto-ik is on */
#define T_AUTOIK			(1 << 18)

/* ******************************************************************************** */

/* transinfo->con->mode */
#define CON_APPLY		1
#define CON_AXIS0		2
#define CON_AXIS1		4
#define CON_AXIS2		8
#define CON_SELECT		16
#define CON_NOFLIP		32	/* does not reorient vector to face viewport when on */
#define CON_LOCAL		64
#define CON_USER		128

/* transdata->flag */
#define TD_SELECTED			1
#define TD_ACTIVE			(1 << 1)
#define	TD_NOACTION			(1 << 2)
#define	TD_USEQUAT			(1 << 3)
#define TD_NOTCONNECTED		(1 << 4)
#define TD_SINGLESIZE		(1 << 5)	/* used for scaling of MetaElem->rad */
#ifdef WITH_VERSE
	#define TD_VERSE_OBJECT		(1 << 6)
	#define TD_VERSE_VERT		(1 << 7)
#endif
#define TD_TIMEONLY			(1 << 8)
#define TD_NOCENTER			(1 << 9)
#define TD_NO_EXT			(1 << 10)	/* ext abused for particle key timing */
#define TD_SKIP				(1 << 11)	/* don't transform this data */
#define TD_BEZTRIPLE		(1 << 12)	/* if this is a bez triple, we need to restore the handles, if this is set transdata->misc.hdata needs freeing */

/* transsnap->status */
#define SNAP_ON			1
#define TARGET_INIT		2
#define POINT_INIT		4

/* transsnap->modePoint */
#define SNAP_GRID			0
#define SNAP_GEO			1

/* transsnap->modeTarget */
#define SNAP_CLOSEST		0
#define SNAP_CENTER			1
#define SNAP_MEDIAN			2
#define SNAP_ACTIVE			3

void checkFirstTime(void);

void setTransformViewMatrices(TransInfo *t);
void convertViewVec(TransInfo *t, float *vec, short dx, short dy);
void projectIntView(TransInfo *t, float *vec, int *adr);
void projectFloatView(TransInfo *t, float *vec, float *adr);

void convertVecToDisplayNum(float *vec, float *num);
void convertDisplayNumToVec(float *num, float *vec);

void initWarp(TransInfo *t);
int handleEventWarp(TransInfo *t, unsigned short event, short val);
int Warp(TransInfo *t, short mval[2]);

void initShear(TransInfo *t);
int handleEventShear(TransInfo *t, unsigned short evenl, short val);
int Shear(TransInfo *t, short mval[2]);

void initResize(TransInfo *t);
int Resize(TransInfo *t, short mval[2]);

void initTranslation(TransInfo *t);
int Translation(TransInfo *t, short mval[2]);

void initToSphere(TransInfo *t);
int ToSphere(TransInfo *t, short mval[2]);

void initRotation(TransInfo *t);
int Rotation(TransInfo *t, short mval[2]);

void initShrinkFatten(TransInfo *t);
int ShrinkFatten(TransInfo *t, short mval[2]);

void initTilt(TransInfo *t);
int Tilt(TransInfo *t, short mval[2]);

void initCurveShrinkFatten(TransInfo *t);
int CurveShrinkFatten(TransInfo *t, short mval[2]);

void initTrackball(TransInfo *t);
int Trackball(TransInfo *t, short mval[2]);

void initPushPull(TransInfo *t);
int PushPull(TransInfo *t, short mval[2]);

void initBevel(TransInfo *t);
int handleEventBevel(TransInfo *t, unsigned short evenl, short val);
int Bevel(TransInfo *t, short mval[2]);

void initBevelWeight(TransInfo *t);
int BevelWeight(TransInfo *t, short mval[2]);

void initCrease(TransInfo *t);
int Crease(TransInfo *t, short mval[2]);

void initBoneSize(TransInfo *t);
int BoneSize(TransInfo *t, short mval[2]);

void initBoneEnvelope(TransInfo *t);
int BoneEnvelope(TransInfo *t, short mval[2]);

void initBoneRoll(TransInfo *t);
int BoneRoll(TransInfo *t, short mval[2]);

void initTimeTranslate(TransInfo *t);
int TimeTranslate(TransInfo *t, short mval[2]);

void initTimeSlide(TransInfo *t);
int TimeSlide(TransInfo *t, short mval[2]);

void initTimeScale(TransInfo *t);
int TimeScale(TransInfo *t, short mval[2]);

void initBakeTime(TransInfo *t);
int BakeTime(TransInfo *t, short mval[2]);

void initMirror(TransInfo *t);
int Mirror(TransInfo *t, short mval[2]);

void initAlign(TransInfo *t);
int Align(TransInfo *t, short mval[2]);

/*********************** transform_conversions.c ********** */
struct ListBase;
void flushTransIpoData(TransInfo *t);
void flushTransUVs(TransInfo *t);
void flushTransParticles(TransInfo *t);
int clipUVTransform(TransInfo *t, float *vec, int resize);

/*********************** exported from transform_manipulator.c ********** */
void draw_manipulator_ext(struct ScrArea *sa, int type, char axis, int col, float vec[3], float mat[][3]);
int calc_manipulator_stats(struct ScrArea *sa);
float get_drawsize(struct View3D *v3d, float *co);

/*********************** TransData Creation and General Handling *********** */
void createTransData(TransInfo *t);
void sort_trans_data_dist(TransInfo *t);
void add_tdi_poin(float *poin, float *old, float delta);
void special_aftertrans_update(TransInfo *t);

void transform_autoik_update(TransInfo *t, short mode);

/* auto-keying stuff used by special_aftertrans_update */
short autokeyframe_cfra_can_key(struct Object *ob);
void autokeyframe_ob_cb_func(struct Object *ob, int tmode);
void autokeyframe_pose_cb_func(struct Object *ob, int tmode, short targetless_ik);

/*********************** Constraints *****************************/

void getConstraintMatrix(TransInfo *t);
void setConstraint(TransInfo *t, float space[3][3], int mode, const char text[]);
void setLocalConstraint(TransInfo *t, int mode, const char text[]);
void setUserConstraint(TransInfo *t, int mode, const char text[]);

void constraintNumInput(TransInfo *t, float vec[3]);

void getConstraintMatrix(TransInfo *t);
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

void initSnapping(struct TransInfo *t);
void applySnapping(TransInfo *t, float *vec);
void resetSnapping(TransInfo *t);
int  handleSnapping(TransInfo *t, int event);
void drawSnapping(TransInfo *t);
int usingSnappingNormal(TransInfo *t);
int validSnappingNormal(TransInfo *t);

/*********************** Generics ********************************/

void initTrans(TransInfo *t);
void initTransModeFlags(TransInfo *t, int mode);
void postTrans (TransInfo *t);

void drawLine(float *center, float *dir, char axis, short options);

TransDataCurveHandleFlags *initTransDataCurveHandes(TransData *td, struct BezTriple *bezt);

/* DRAWLINE options flags */
#define DRAWLIGHT	1
#define DRAWDASHED	2
#define DRAWBOLD	4

void applyTransObjects(TransInfo *t);
void restoreTransObjects(TransInfo *t);
void recalcData(TransInfo *t);

void calculateCenter(TransInfo *t);
void calculateCenter2D(TransInfo *t);
void calculateCenterBound(TransInfo *t);
void calculateCenterMedian(TransInfo *t);
void calculateCenterCursor(TransInfo *t);

void calculateCenterCursor2D(TransInfo *t);
void calculatePropRatio(TransInfo *t);

void getViewVector(float coord[3], float vec[3]);
void getViewRay(short mval[2], float p[3], float d[3]);

TransInfo * BIF_GetTransInfo(void);

/*********************** NumInput ********************************/

void outputNumInput(NumInput *n, char *str);
short hasNumInput(NumInput *n);
void applyNumInput(NumInput *n, float *vec);
char handleNumInput(NumInput *n, unsigned short event);

/*********************** NDofInput ********************************/

void initNDofInput(NDofInput *n);
int hasNDofInput(NDofInput *n);
void applyNDofInput(NDofInput *n, float *vec);
int handleNDofInput(NDofInput *n, unsigned short event, short val);

/* handleNDofInput return values */
#define NDOF_REFRESH	1
#define NDOF_NOMOVE		2
#define NDOF_CONFIRM	3
#define NDOF_CANCEL		4


/*********************** TransSpace ******************************/

int manageObjectSpace(int confirm, int set);
int manageMeshSpace(int confirm, int set);
int manageBoneSpace(int confirm, int set);

/* Those two fill in mat and return non-zero on success */
int createSpaceNormal(float mat[3][3], float normal[3]);
int createSpaceNormalTangent(float mat[3][3], float normal[3], float tangent[3]);

int addMatrixSpace(float mat[3][3], char name[]);
int addObjectSpace(struct Object *ob);
void applyTransformOrientation(void);


#define ORIENTATION_NONE	0
#define ORIENTATION_NORMAL	1
#define ORIENTATION_VERT	2
#define ORIENTATION_EDGE	3
#define ORIENTATION_FACE	4

int getTransformOrientation(float normal[3], float plane[3], int activeOnly);
int createSpaceNormal(float mat[3][3], float normal[3]);
int createSpaceNormalTangent(float mat[3][3], float normal[3], float tangent[3]);

#endif


