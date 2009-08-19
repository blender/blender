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
* The Original Code is Copyright (C) 2006 by NaN Holding BV.
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): Daniel Genrich (Genscher)
*
* ***** END GPL LICENSE BLOCK *****
*/
#ifndef DNA_SMOKE_TYPES_H
#define DNA_SMOKE_TYPES_H

/* flags */
#define MOD_SMOKE_HIGHRES (1<<1) /* compute high resolution */
#define MOD_SMOKE_DISSOLVE (1<<2) /* let smoke dissolve */
#define MOD_SMOKE_DISSOLVE_LOG (1<<3) /* using 1/x for dissolve */

/* noise */
#define MOD_SMOKE_NOISEWAVE (1<<0)
#define MOD_SMOKE_NOISEFFT (1<<1)
#define MOD_SMOKE_NOISECURL (1<<2)
/* viewsettings */
#define MOD_SMOKE_VIEW_X (1<<0)
#define MOD_SMOKE_VIEW_Y (1<<1)
#define MOD_SMOKE_VIEW_Z (1<<2)
#define MOD_SMOKE_VIEW_SMALL (1<<3)
#define MOD_SMOKE_VIEW_BIG (1<<4)
#define MOD_SMOKE_VIEW_CHANGETOBIG (1<<5)
#define MOD_SMOKE_VIEW_REDRAWNICE (1<<6)
#define MOD_SMOKE_VIEW_REDRAWALL (1<<7)
#define MOD_SMOKE_VIEW_USEBIG (1<<8)

typedef struct SmokeDomainSettings {
	struct SmokeModifierData *smd; /* for fast RNA access */
	struct FLUID_3D *fluid;
	struct Group *fluid_group;
	struct Group *eff_group; // effector group for e.g. wind force
	struct Group *coll_group; // collision objects group
	unsigned int *bind;
	float *tvox;
	float *tray;
	float *tvoxbig;
	float *traybig;
	float p0[3]; /* start point of BB */
	float p1[3]; /* end point of BB */
	float dx; /* edge length of one cell */
	float firstframe;
	float lastframe;
	float omega; /* smoke color - from 0 to 1 */
	float temp; /* fluid temperature */
	float tempAmb; /* ambient temperature */
	float alpha;
	float beta;
	int res[3]; /* domain resolution */
	int amplify; /* wavelet amplification */
	int maxres; /* longest axis on the BB gets this resolution assigned */
	int flags; /* show up-res or low res, etc */
	int visibility; /* how many billboards to show (every 2nd, 3rd, 4th,..) */
	int viewsettings;
	int max_textures;
	short noise; /* noise type: wave, curl, anisotropic */
	short diss_percent; 
	int diss_speed;/* in frames */
	float strength;
	struct WTURBULENCE *wt; // WTURBULENCE object, if active
} SmokeDomainSettings;


/* inflow / outflow */

/* type */
#define MOD_SMOKE_FLOW_TYPE_OUTFLOW (1<<1)

typedef struct SmokeFlowSettings {
	struct SmokeModifierData *smd; /* for fast RNA access */
	struct ParticleSystem *psys;
	float density;
	float temp; /* delta temperature (temp - ambient temp) */
	float velocity[3]; /* UNUSED, velocity taken from particles */
	float vgrp_heat_scale[2]; /* min and max scaling for vgroup_heat */
	short vgroup_flow; /* where inflow/outflow happens - red=1=action */
	short vgroup_density;
	short vgroup_heat;
	short type; /* inflow =0 or outflow = 1 */
	int pad;
} SmokeFlowSettings;

/*
	struct BVHTreeFromMesh *bvh;
	float mat[4][4];
	float mat_old[4][4];
	*/

/* collision objects (filled with smoke) */
typedef struct SmokeCollSettings {
	struct SmokeModifierData *smd; /* for fast RNA access */
	struct BVHTree *bvhtree; /* bounding volume hierarchy for this cloth object */
	struct DerivedMesh *dm;
	float *points;
	float *points_old;
	float *vel;
	float mat[4][4];
	float mat_old[4][4];
	int numpoints;
	int numverts; // check if mesh changed
	short type; // static = 0, rigid = 1, dynamic = 2
	short pad;
	int pad2;
} SmokeCollSettings;

#endif
