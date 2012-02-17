/*
 *
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2004-2005 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_object_fluidsim.h
 *  \ingroup DNA
 */

#ifndef __DNA_OBJECT_FLUIDSIM_H__
#define __DNA_OBJECT_FLUIDSIM_H__

#include "DNA_ID.h"

#ifdef __cplusplus
extern "C" {
#endif
	
struct Mesh;
struct Ipo;

typedef struct FluidVertexVelocity {
	float vel[3];
} FluidVertexVelocity;
	
typedef struct FluidsimSettings {
	struct FluidsimModifierData *fmd; /* for fast RNA access */
	/* domain,fluid or obstacle */
	short type;
	/* display advanced options in fluid sim tab (on=1,off=0)*/
	short show_advancedoptions;

	/* domain object settings */
	/* resolutions */
	short resolutionxyz;
	short previewresxyz;
	/* size of the domain in real units (meters along largest resolution x,y,z extent) */
	float realsize;
	/* show original meshes, preview or final sim */
	short guiDisplayMode;
	short renderDisplayMode;

	/* fluid properties */
	float viscosityValue;
	short viscosityMode;
	short viscosityExponent;
	/* gravity strength */
	float grav[3];
	/* anim start end time (in seconds) */
	float animStart, animEnd;
	/* bake start end time (in blender frames) */
	int bakeStart, bakeEnd;
	/* offset for baked frames */
	int frameOffset;
	int pad;
	/* g star param (LBM compressibility) */
	float gstar;
	/* activate refinement? */
	int maxRefine;
	
	/* fluid object type settings */
	/* gravity strength */
	float iniVelx,iniVely,iniVelz;

	/* store pointer to original mesh (for replacing the current one) */
	struct Mesh *orgMesh;
	/* a mesh to display the bounding box used for simulation */
	struct Mesh *meshBB;

	/* store output path, and file prefix for baked fluid surface */
	/* strlens; 256= FILE_MAXFILE, 768= FILE_MAXDIR */
	char surfdataPath[1024];

	/* store start coords of axis aligned bounding box together with size */
	/* values are inited during derived mesh display */
	float bbStart[3], bbSize[3];

	/* animated params */
	struct Ipo *ipo;

	/* additional flags depending on the type, lower short contains flags
	 * to check validity, higher short additional flags */
	short typeFlags;
	/* switch off velocity genration, volume init type for fluid/obstacles (volume=1,shell=2,both=3) */
	char  domainNovecgen,volumeInitType;

	/* boundary "stickiness" for part slip values */
	float partSlipValue;

	/* number of tracers to generate */
	int generateTracers;
	/* particle generation - on if >0, then determines amount (experimental...) */
	float generateParticles;
	/* smooth fluid surface? */
	float surfaceSmoothing;
	/* number of surface subdivisions*/
	int surfaceSubdivs;
	int flag; /* GUI flags */

	/* particle display - size scaling, and alpha influence */
	float particleInfSize, particleInfAlpha;
	/* testing vars */
	float farFieldSize;

	/* vertex velocities of simulated fluid mesh */
	struct FluidVertexVelocity *meshVelocities;
	/* number of vertices in simulated fluid mesh */
	int totvert;
	
	/* Fluid control settings */
	float cpsTimeStart;
	float cpsTimeEnd;
	float cpsQuality;
	
	float attractforceStrength;
	float attractforceRadius;
	float velocityforceStrength;
	float velocityforceRadius;

	int lastgoodframe;
	
	/* Simulation/flow rate control (i.e. old "Fac-Time") */
	float animRate;
} FluidsimSettings;

/* ob->fluidsimSettings defines */
#define OB_FLUIDSIM_ENABLE			1
#define OB_FLUIDSIM_DOMAIN			2
#define OB_FLUIDSIM_FLUID			4
#define OB_FLUIDSIM_OBSTACLE		8
#define OB_FLUIDSIM_INFLOW			16
#define OB_FLUIDSIM_OUTFLOW			32
#define OB_FLUIDSIM_PARTICLE		64
#define OB_FLUIDSIM_CONTROL			128

#define OB_TYPEFLAG_START       7
#define OB_FSGEO_THIN           (1<<(OB_TYPEFLAG_START+1))
#define OB_FSBND_NOSLIP         (1<<(OB_TYPEFLAG_START+2))
#define OB_FSBND_PARTSLIP       (1<<(OB_TYPEFLAG_START+3))
#define OB_FSBND_FREESLIP       (1<<(OB_TYPEFLAG_START+4))
#define OB_FSINFLOW_LOCALCOORD  (1<<(OB_TYPEFLAG_START+5))

/* surface generation flag (part of enabling chapter 6 of "Free Surface Flows with Moving and Deforming Objects for LBM") */
#define OB_FSSG_NOOBS			(1<<(OB_TYPEFLAG_START+6))

// guiDisplayMode particle flags
#define OB_FSDOM_GEOM     1
#define OB_FSDOM_PREVIEW  2
#define OB_FSDOM_FINAL    3
#define OB_FSPART_BUBBLE  (1<<1)
#define OB_FSPART_DROP    (1<<2)
#define OB_FSPART_NEWPART (1<<3)
#define OB_FSPART_FLOAT   (1<<4)
#define OB_FSPART_TRACER  (1<<5)

// new fluid bit flags for fss->flags
#define OB_FLUIDSIM_REVERSE			(1 << 0)
#define OB_FLUIDSIM_ACTIVE			(1 << 1)
#define OB_FLUIDSIM_OVERRIDE_TIME	(1 << 2)

#define OB_FLUIDSIM_SURF_PREVIEW_OBJ_FNAME "fluidsurface_preview_####.bobj.gz"
#define OB_FLUIDSIM_SURF_FINAL_OBJ_FNAME   "fluidsurface_final_####.bobj.gz"
#define OB_FLUIDSIM_SURF_FINAL_VEL_FNAME   "fluidsurface_final_####.bvel.gz"
#define OB_FLUIDSIM_SURF_PARTICLES_FNAME   "fluidsurface_particles_####.gz"

#ifdef __cplusplus
}
#endif

#endif


