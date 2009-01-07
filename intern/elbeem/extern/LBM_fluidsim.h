/**
 * BKE_fluidsim.h 
 *	
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef LBM_FLUIDSIM_H
#define LBM_FLUIDSIM_H

struct Mesh;
struct DerivedMesh;
struct Object;
struct fluidsimDerivedMesh;

extern double fluidsimViscosityPreset[6];
extern char* fluidsimViscosityPresetString[6];

/* allocates and initializes fluidsim data */
struct FluidsimSettings* fluidsimSettingsNew(struct Object *srcob);

/* frees internal data itself */
void fluidsimSettingsFree(struct FluidsimSettings* sb);

/* duplicate internal data */
struct FluidsimSettings* fluidsimSettingsCopy(struct FluidsimSettings* sb);

/* export blender geometry to fluid solver */
void fluidsimBake(struct Object* ob);

/* read & write bobj / bobj.gz files (e.g. for fluid sim surface meshes) */
void writeBobjgz(char *filename, struct Object *ob, int useGlobalCoords, int append, float time);
struct Mesh* readBobjgz(char *filename, struct Mesh *orgmesh, float* bbstart, float *bbsize);

/* create derived mesh for fluid sim objects */
// WARNING - currently implemented in DerivedMesh.c!
void loadFluidsimMesh(struct Object *srcob, int useRenderParams);

/* run simulation with given config file */
// WARNING - implemented in intern/elbeem/blendercall.cpp
int performElbeemSimulation(char *cfgfilename);


#endif


