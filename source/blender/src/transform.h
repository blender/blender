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

#include "transform_numinput.h"
#include "BIF_transform.h"

/* ************************** Types ***************************** */

struct TransInfo;
struct TransData;

typedef struct TransCon {
    char  text[50];      /* Description of the Constraint for header_print                            */
    float mtx[3][3];     /* Matrix of the Constraint space                                            */
    float imtx[3][3];    /* Inverse Matrix of the Constraint space                                    */
    float center[3];     /* transformation centre to define where to draw the view widget             
                            ALWAYS in global space. Unlike the transformation center                  */
    int   mode;          /* Mode flags of the Constraint                                              */
    void  (*applyVec)(struct TransInfo *, struct TransData *, float *, float *);
                         /* Apply function pointer for linear vectorial transformation                */
                         /* The last two parameters are pointers to the in/out vectors                */
    void  (*applyRot)(struct TransInfo *, struct TransData *, float [3]);
                         /* Apply function pointer for rotation transformation (prototype will change */
} TransCon;

typedef struct TransDataExtension {
    float *rot;          /* Rotation of the data to transform (Faculative)                                 */
    float  irot[3];      /* Initial rotation                                                               */
    float *quat;         /* Rotation quaternion of the data to transform (Faculative)                      */
    float  iquat[4];	 /* Initial rotation quaternion                                                    */
    float *size;         /* Size of the data to transform (Faculative)                                     */
    float  isize[3];	 /* Initial size                                                                   */
	float  obmat[3][3];	 /* Object matrix */
} TransDataExtension;

typedef struct TransData {
	float  dist;         /* Distance to the nearest element (for Proportionnal Editing)                    */
	float  factor;       /* Factor of the transformation (for Proportionnal Editing)                       */
    float *loc;          /* Location of the data to transform                                              */
    float  iloc[3];      /* Initial location                                                               */
    float  center[3];
    float  mtx[3][3];    /* Transformation matrix from data space to global space                          */
    float  smtx[3][3];   /* Transformation matrix from global space to data space                          */
	struct Object *ob;
	struct TransDataExtension *ext;
    int    flag;         /* Various flags */

/* NOTE TO TON: THESE MOVES TO TransDataExtension. NEED TO CHANGE POSEMODE */
    float *rot;          /* Rotation of the data to transform (Faculative)                                 */
    float  irot[3];      /* Initial rotation                                                               */
    float *quat;         /* Rotation quaternion of the data to transform (Faculative)                      */
    float  iquat[4];	 /* Initial rotation quaternion                                                    */
    float *size;         /* Size of the data to transform (Faculative)                                     */
    float  isize[3];	 /* Initial size                                                                   */
	
	void *bone;			/* BWARGH! old transform demanded it, added for now (ton) */
} TransData;

typedef struct TransInfo {
    int         mode;           /* current mode                         */
    int       (*transform)(struct TransInfo *, short *);
                                /* transform function pointer           */
    char        redraw;         /* redraw flag                          */
    int	        flags;          /* generic flags for special behaviors  */
    int         total;          /* total number of transformed data     */
	float		propsize;		/* proportional circle radius           */
	char		proptext[20];	/* proportional falloff text			*/
    float       center[3];      /* center of transformation             */
    short       center2d[2];    /* center in screen coordinates         */
    short       imval[2];       /* initial mouse position               */
    TransData  *data;           /* transformed data (array)             */
    TransCon    con;            /* transformed constraint               */
    NumInput    num;            /* numerical input                      */
    float       val;            /* init value for some transformations  */
    float       fac;            /* factor for distance based transform  */
} TransInfo;


/* ******************** Macros & Prototypes *********************** */

/* MODE AND NUMINPUT FLAGS */
#define NOCONSTRAINT	1
#define NULLONE			2
#define NONEGATIVE		4
#define	NOZERO			8
#define NOFRACTION		16

/* transinfo->mode */
#define TFM_REPEAT			0
#define TFM_TRANSLATION		1
#define TFM_ROTATION		2
#define TFM_RESIZE			3
#define TFM_TOSPHERE		4
#define TFM_SHEAR			5
#define TFM_LAMP_ENERGY		6

#define APPLYCON		1
#define CONAXIS0		2
#define CONAXIS1		4
#define CONAXIS2		8

#define PROP_SHARP		0
#define PROP_SMOOTH		1
#define PROP_ROOT		2
#define PROP_LIN		3
#define PROP_CONST		4

/* transdata->flag */
#define TD_SELECTED		1
#define	TD_NOACTION		2
#define	TD_USEQUAT		4
#define TD_OBJECT		8

void Transform(int mode);

void initWrap(TransInfo *t);
int Wrap(TransInfo *t, short mval[2]);

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

#endif

