/** \file elbeem/intern/elbeem_control.h
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2006 Nils Thuerey
 *
 * Control API header
 */
#ifndef ELBEEMCONTROL_API_H
#define ELBEEMCONTROL_API_H

// a single control particle set
typedef struct elbeemControl {
	/* influence forces */
	float influenceAttraction;
	float *channelInfluenceAttraction;
	float channelSizeInfluenceAttraction;

	float influenceVelocity;
	float *channelInfluenceVelocity;
	float channelSizeInfluenceVelocity;

	float influenceMaxdist;
	float *channelInfluenceMaxdist;
	float channelSizeInfluenceMaxdist;

	/* influence force radii */
	float radiusAttraction;
	float *channelRadiusAttraction;
	float channelSizeRadiusAttraction;

	float radiusVelocity;
	float *channelRadiusVelocity;
	float channelSizeRadiusVelocity;

	float radiusMindist;
	float *channelRadiusMindist;
	float channelSizeRadiusMindist;
	float radiusMaxdist;
	float *channelRadiusMaxdist;
	float channelSizeRadiusMaxdist;

	/* control particle positions/scale */
	float offset[3];
	float *channelOffset;
	float channelSizeOffset;
	
	float scale[3];
	float *channelScale;
	float channelSizeScale;
	
} elbeemControl;


// add mesh as fluidsim object
int elbeemControlAddSet(struct elbeemControl*);

// sample & track mesh control particles, TODO add return type...
int elbeemControlComputeMesh(struct elbeemMesh*);

#endif // ELBEEMCONTROL_API_H
