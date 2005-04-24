/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "BIF_transform.h"

/* ************************** Types ***************************** */

struct TransInfo;
struct TransData;
struct NumInput;

typedef struct NumInput {
    short  idx;
    short  idx_max;
    short  flag;        /* Different flags to indicate different behaviors                                */
    float  val[3];       /* Direct value of the input                                                      */
    short  ctrl[3];      /* Control to indicate what to do with the numbers that are typed                 */
} NumInput ;

/*
	The ctrl value has different meaning:
		0			: No value has been typed
		
		otherwise, |value| - 1 is where the cursor is located after the period
		Positive	: number is positive
		Negative	: number is negative
*/

typedef struct TransCon {
    char  text[50];      /* Description of the Constraint for header_print                            */
    float mtx[3][3];     /* Matrix of the Constraint space                                            */
    float imtx[3][3];    /* Inverse Matrix of the Constraint space                                    */
    float pmtx[3][3];    /* Projection Constraint Matrix (same as imtx with some axis == 0)           */
    float center[3];     /* transformation centre to define where to draw the view widget             
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
	float  obmat[3][3];	 /* Object matrix */  
	void *bone;			/* ARGH! old transform demanded it, added for now (ton) */
} TransDataExtension;

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
	TransDataExtension *ext;	/* for objects, poses. 1 single malloc per TransInfo! */
	TransDataIpokey *tdi;		/* for objects, ipo keys. per transdata a malloc */
    int    flag;         /* Various flags */
} TransData;

typedef struct TransInfo {
    int         mode;           /* current mode                         */
    int         context;        /* current context                      */
    int       (*transform)(struct TransInfo *, short *);
                                /* transform function pointer           */
    char        redraw;         /* redraw flag                          */
    int	        flag;          /* generic flags for special behaviors  */
    int         total;          /* total number of transformed data     */
	float		propsize;		/* proportional circle radius           */
	char		proptext[20];	/* proportional falloff text			*/
    float       center[3];      /* center of transformation             */
    short       center2d[2];    /* center in screen coordinates         */
    short       imval[2];       /* initial mouse position               */
	short		shiftmval[2];	/* mouse position when shift was pressed */
	short       idx_max;
	float		snap[3];		/* Snapping Gears						*/
    TransData  *data;           /* transformed data (array)             */
	TransDataExtension *ext;	/* transformed data extension (array)   */
    TransCon    con;            /* transformed constraint               */
    NumInput    num;            /* numerical input                      */
    float       val;            /* init value for some transformations (and rotation angle)  */
    float       fac;            /* factor for distance based transform  */
	
	float		viewmat[4][4];	/* copy from G.vd, prevents feedback    */
	float		viewinv[4][4];
	float		persinv[4][4];
	
	float		vec[3];			/* translation, to show for widget   	*/
	float		mat[3][3];		/* rot/rescale, to show for widget   	*/
} TransInfo;


/* ******************** Macros & Prototypes *********************** */

/* NUMINPUT FLAGS */
#define NUM_NULL_ONE		2
#define NUM_NO_NEGATIVE		4
#define	NUM_NO_ZERO			8
#define NUM_NO_FRACTION		16
#define	NUM_AFFECT_ALL		32

/* transinfo->flag */
#define T_OBJECT		1
#define T_EDIT			2
#define T_POSE			4
#define T_TEXTURE		8
#define T_CAMERA		16
		// when shift pressed, higher resolution transform. cannot rely on G.qual, need event!
#define T_SHIFT_MOD		32
		// for manipulator exceptions, like scaling using center point, drawing help lines
#define T_USES_MANIPULATOR	128

/* restrictions flags */
#define T_ALL_RESTRICTIONS	(256|512|1024)
#define T_NO_CONSTRAINT	256
#define T_NULL_ONE		512
#define T_NO_ZERO		1024

#define T_PROP_EDIT		 2048
#define T_PROP_CONNECTED 4096


/* transinfo->con->mode */
#define CON_APPLY		1
#define CON_AXIS0		2
#define CON_AXIS1		4
#define CON_AXIS2		8
#define CON_SELECT		16
#define CON_NOFLIP		32	/* does not reorient vector to face viewport when on */

/* transdata->flag */
#define TD_SELECTED		1
#define	TD_NOACTION		2
#define	TD_USEQUAT		4
#define TD_NOTCONNECTED		8
#define TD_SINGLESIZE		16	/* used for scaling of MetaElem->rad */

void initWarp(TransInfo *t);
int Warp(TransInfo *t, short mval[2]);

void initShear(TransInfo *t);
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

void initTrackball(TransInfo *t);
int Trackball(TransInfo *t, short mval[2]);

void initPushPull(TransInfo *t);
int PushPull(TransInfo *t, short mval[2]);

void initCrease(TransInfo *t);
int Crease(TransInfo *t, short mval[2]);

/* exported from transform.c */
struct ListBase;
void count_bone_select(TransInfo *t, struct ListBase *lb, int *counter);

/* exported from transform_manipulator.c */
struct ScrArea;
void draw_manipulator_ext(struct ScrArea *sa, int type, char axis, int col, float vec[3], float mat[][3]);

/*********************** TransData Creation and General Handling */
void createTransData(TransInfo *t);
void sort_trans_data_dist(TransInfo *t);
void clear_trans_object_base_flags(void);
void add_tdi_poin(float *poin, float *old, float delta);

/*********************** Constraints *****************************/
void getConstraintMatrix(TransInfo *t);
void setConstraint(TransInfo *t, float space[3][3], int mode, const char text[]);
void setLocalConstraint(TransInfo *t, int mode, const char text[]);

void constraintNumInput(TransInfo *t, float vec[3]);

//void drawConstraint(TransCon *t);
void drawConstraint();

//void drawPropCircle(TransInfo *t);
void drawPropCircle();

void initConstraint(TransInfo *t);
void startConstraint(TransInfo *t);
void stopConstraint(TransInfo *t);

void getConstraintMatrix(TransInfo *t);

void initSelectConstraint(TransInfo *t, float mtx[3][3]);
void selectConstraint(TransInfo *t);
void postSelectConstraint(TransInfo *t);

int getConstraintSpaceDimension(TransInfo *t);

void setNearestAxis(TransInfo *t);


/*********************** Generics ********************************/
void recalcData(TransInfo *t);

void initTransModeFlags(TransInfo *t, int mode);

void drawLine(float *center, float *dir, char axis, short options);

/* DRAWLINE options flags */
#define DRAWLIGHT	1
#define DRAWDASHED	2
#define DRAWBOLD	4

void applyTransObjects(TransInfo *t);
void restoreTransObjects(TransInfo *t);

void initTrans(TransInfo *t);
void postTrans (TransInfo *t);

void calculateCenterBound(TransInfo *t);
void calculateCenterMedian(TransInfo *t);
void calculateCenterCursor(TransInfo *t);

void calculateCenter(TransInfo *t);

void calculatePropRatio(TransInfo *t);

void snapGrid(TransInfo *t, float *val);

void getViewVector(float coord[3], float vec[3]);

TransInfo * BIF_GetTransInfo(void);


/*********************** NumInput ********************************/
void outputNumInput(NumInput *n, char *str);

short hasNumInput(NumInput *n);

void applyNumInput(NumInput *n, float *vec);

char handleNumInput(NumInput *n, unsigned short event);
#endif

