/**
 *
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
 * The Original Code is Copyright (C) 2004-2005 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef DNA_OBJECT_FORCE_H
#define DNA_OBJECT_FORCE_H

#ifdef __cplusplus
extern "C" {
#endif
	
typedef struct PartDeflect {
	short deflect;		/* Deflection flag - does mesh deflect particles*/
	short forcefield;	/* Force field type, do the vertices attract / repel particles ? */
	short flag;			/* general settings flag */
	short pad;
	
	float pdef_damp;	/* Damping factor for particle deflection       */
	float pdef_rdamp;	/* Random element of damping for deflection     */
	float pdef_perm;	/* Chance of particle passing through mesh      */

	float f_strength;	/* The strength of the force (+ or - )       */
	float f_power;		/* The power law - real gravitation is 2 (square)  */
	float maxdist;		/* if indicated, use this maximum */
	
	float pdef_sbdamp;	/* Damping factor for softbody deflection       */
	float pdef_sbift;	/* inner face thickness for softbody deflection */
	float pdef_sboft;	/* outer face thickness for softbody deflection */
} PartDeflect;


typedef struct SBVertex {
	float vec[4];
} SBVertex;

typedef struct SoftBody {
	/* dynamic data */
	int totpoint, totspring;
	struct BodyPoint *bpoint;		/* not saved in file */
	struct BodySpring *bspring;		/* not saved in file */
	float ctime;					/* last time calculated */
	
	/* part of UI: */
	
	/* general options */
	float nodemass;		/* softbody mass of *vertex* */
	float grav;			/* softbody amount of gravitaion to apply */
	float mediafrict;	/* friction to env */
	float rklimit;		/* error limit for ODE solver */
	float physics_speed;/* user control over simulation speed */
	
	/* goal */
	float goalspring;	/* softbody goal springs */
	float goalfrict;	/* softbody goal springs friction */
	float mingoal;		/* quick limits for goal */
	float maxgoal;
	float defgoal;		/* default goal for vertices without vgroup */
	short vertgroup;	/* index starting at 1 */
  
	short fuzzyness;      /* */
	
	/* springs */
	float inspring;		/* softbody inner springs */
	float infrict;		/* softbody inner springs friction */
 	
	/* baking */
	int sfra, efra;
	int interval;
	short local, solverflags;		/* local==1: use local coords for baking */
	
	SBVertex **keys;			/* array of size totpointkey */
	int totpointkey, totkey;	/* if totpointkey != totpoint or totkey!- (efra-sfra)/interval -> free keys */
	float secondspring;

	/* self collision*/
	float colball;		/* fixed collision ball size if > 0 */
	float balldamp;		/* cooling down collision response  */
	float ballstiff;	/* pressure the ball is loaded with  */
	short sbc_mode;
    short aeroedge,
		minloops,
		maxloops,
		choke,
		pad3,pad4,pad5
		;   

	struct SBScratch *scratch;	/* scratch pad/cache on live time not saved in file */
} SoftBody;

/* pd->forcefield:  Effector Fields types */
#define PFIELD_FORCE	1
#define PFIELD_VORTEX	2
#define PFIELD_MAGNET	3
#define PFIELD_WIND		4
#define PFIELD_GUIDE	5

/* pd->flag: various settings */
#define PFIELD_USEMAX			1
#define PDEFLE_DEFORM			2
#define PFIELD_GUIDE_PATH_ADD	4

/* ob->softflag */
#define OB_SB_ENABLE	1
#define OB_SB_GOAL		2
#define OB_SB_EDGES		4
#define OB_SB_QUADS		8
#define OB_SB_POSTDEF	16
#define OB_SB_REDO		32
#define OB_SB_BAKESET	64
#define OB_SB_BAKEDO	128
#define OB_SB_RESET		256
#define OB_SB_SELF		512
#define OB_SB_FACECOLL  1024
#define OB_SB_EDGECOLL  2048
#define OB_SB_COLLFINAL 4096

#define SBSO_MONITOR    1 
#define SBSO_OLDERR     2 

#ifdef __cplusplus
}
#endif

#endif

