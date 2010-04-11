/*
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
* along with this program; if not, write to the Free Software  Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2005 by the Blender Foundation.
* All rights reserved.
*
* Contributor(s): Daniel Dunbar
*                 Ton Roosendaal,
*                 Ben Batt,
*                 Brecht Van Lommel,
*                 Campbell Barton
*
* ***** END GPL LICENSE BLOCK *****
*
*/

#include "stddef.h"
#include "string.h"
#include "stdarg.h"
#include "math.h"
#include "float.h"

// headers for fluidsim bobj meshes
#include <stdlib.h>
#include "LBM_fluidsim.h"
#include <zlib.h>
#include <string.h>
#include <stdio.h>

#include "BLI_kdtree.h"
#include "BLI_rand.h"
#include "BLI_uvproject.h"

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_object_fluidsim.h"


#include "BKE_action.h"
#include "BKE_bmesh.h"
#include "BKE_cloth.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_multires.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_smoke.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"

#include "depsgraph_private.h"
#include "BKE_deform.h"
#include "BKE_shrinkwrap.h"

#include "MOD_modifiertypes.h"
#include "MOD_fluidsim_util.h"


#include "BLI_storage.h" /* _LARGEFILE_SOURCE */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_fluidsim.h"
#include "DNA_object_force.h" // for pointcache
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h" // N_T

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"
#include "BKE_pointcache.h"
#include "BKE_utildefines.h"

#include "BLO_sys_types.h"

void fluidsim_init(FluidsimModifierData *fluidmd)
{
#ifndef DISABLE_ELBEEM
	if(fluidmd)
	{
		FluidsimSettings *fss = MEM_callocN(sizeof(FluidsimSettings), "fluidsimsettings");
		
		fluidmd->fss = fss;
		
		if(!fss)
			return;
		
		fss->fmd = fluidmd;
		fss->type = OB_FLUIDSIM_ENABLE;
		fss->show_advancedoptions = 0;

		fss->resolutionxyz = 65;
		fss->previewresxyz = 45;
		fss->realsize = 0.5;
		fss->guiDisplayMode = 2; // preview
		fss->renderDisplayMode = 3; // render

		fss->viscosityMode = 2; // default to water
		fss->viscosityValue = 1.0;
		fss->viscosityExponent = 6;
		
		// dg TODO: change this to []
		fss->gravx = 0.0;
		fss->gravy = 0.0;
		fss->gravz = -9.81;
		fss->animStart = 0.0; 
		fss->animEnd = 4.0;
		fss->gstar = 0.005; // used as normgstar
		fss->maxRefine = -1;
		// maxRefine is set according to resolutionxyz during bake

		// fluid/inflow settings
		// fss->iniVel --> automatically set to 0

		/*  elubie: changed this to default to the same dir as the render output
		to prevent saving to C:\ on Windows */
		BLI_strncpy(fss->surfdataPath, btempdir, FILE_MAX);

		// first init of bounding box
		// no bounding box needed
		
		// todo - reuse default init from elbeem!
		fss->typeFlags = OB_FSBND_PARTSLIP;
		fss->domainNovecgen = 0;
		fss->volumeInitType = 1; // volume
		fss->partSlipValue = 0.2;

		fss->generateTracers = 0;
		fss->generateParticles = 0.0;
		fss->surfaceSmoothing = 1.0;
		fss->surfaceSubdivs = 0.0;
		fss->particleInfSize = 0.0;
		fss->particleInfAlpha = 0.0;
	
		// init fluid control settings
		fss->attractforceStrength = 0.2;
		fss->attractforceRadius = 0.75;
		fss->velocityforceStrength = 0.2;
		fss->velocityforceRadius = 0.75;
		fss->cpsTimeStart = fss->animStart;
		fss->cpsTimeEnd = fss->animEnd;
		fss->cpsQuality = 10.0; // 1.0 / 10.0 => means 0.1 width
		
		/*
		BAD TODO: this is done in buttons_object.c in the moment 
		Mesh *mesh = ob->data;
		// calculate bounding box
		fluid_get_bb(mesh->mvert, mesh->totvert, ob->obmat, fss->bbStart, fss->bbSize);	
		*/
		
		// (ab)used to store velocities
		fss->meshSurfNormals = NULL;
		
		fss->lastgoodframe = -1;
		
		fss->flag |= OB_FLUIDSIM_ACTIVE;

	}
#endif
	return;
}

void fluidsim_free(FluidsimModifierData *fluidmd)
{
#ifndef DISABLE_ELBEEM
	if(fluidmd)
	{
		if(fluidmd->fss->meshSurfNormals)
		{
			MEM_freeN(fluidmd->fss->meshSurfNormals);
			fluidmd->fss->meshSurfNormals = NULL;
		}
		MEM_freeN(fluidmd->fss);
	}
#endif
	return;
}

#ifndef DISABLE_ELBEEM
/* read .bobj.gz file into a fluidsimDerivedMesh struct */
DerivedMesh *fluidsim_read_obj(char *filename)
{
	int wri,i,j;
	float wrf;
	int gotBytes;
	gzFile gzf;
	int numverts = 0, numfaces = 0;
	DerivedMesh *dm = NULL;
	MFace *mface;
	MVert *mvert;
	short *normals;

	// ------------------------------------------------
	// get numverts + numfaces first
	// ------------------------------------------------
	gzf = gzopen(filename, "rb");
	if (!gzf)
	{
		return NULL;
	}

	// read numverts
	gotBytes = gzread(gzf, &wri, sizeof(wri));
	numverts = wri;

	// skip verts
	for(i=0; i<numverts*3; i++)
	{
		gotBytes = gzread(gzf, &wrf, sizeof( wrf ));
	}

	// read number of normals
	gotBytes = gzread(gzf, &wri, sizeof(wri));

	// skip normals
	for(i=0; i<numverts*3; i++)
	{
		gotBytes = gzread(gzf, &wrf, sizeof( wrf ));
	}

	/* get no. of triangles */
	gotBytes = gzread(gzf, &wri, sizeof(wri));
	numfaces = wri;

	gzclose( gzf );
	// ------------------------------------------------

	if(!numfaces || !numverts)
		return NULL;

	gzf = gzopen(filename, "rb");
	if (!gzf)
	{
		return NULL;
	}

	dm = CDDM_new(numverts, 0, numfaces);

	if(!dm)
	{
		gzclose( gzf );
		return NULL;
	}

	// read numverts
	gotBytes = gzread(gzf, &wri, sizeof(wri));

	// read vertex position from file
	mvert = CDDM_get_verts(dm);
	for(i=0; i<numverts; i++)
	{
		MVert *mv = &mvert[i];

		for(j=0; j<3; j++)
		{
			gotBytes = gzread(gzf, &wrf, sizeof( wrf ));
			mv->co[j] = wrf;
		}
	}

	// should be the same as numverts
	gotBytes = gzread(gzf, &wri, sizeof(wri));
	if(wri != numverts)
	{
		if(dm)
			dm->release(dm);
		gzclose( gzf );
		return NULL;
	}

	normals = MEM_callocN(sizeof(short) * numverts * 3, "fluid_tmp_normals" );
	if(!normals)
	{
		if(dm)
			dm->release(dm);
		gzclose( gzf );
		return NULL;
	}

	// read normals from file (but don't save them yet)
	for(i=0; i<numverts*3; i++)
	{
		gotBytes = gzread(gzf, &wrf, sizeof( wrf ));
		normals[i] = (short)(wrf*32767.0f);
	}

	/* read no. of triangles */
	gotBytes = gzread(gzf, &wri, sizeof(wri));

	if(wri!=numfaces)
		printf("Fluidsim: error in reading data from file.\n");

	// read triangles from file
	mface = CDDM_get_faces(dm);
	for(i=0; i<numfaces; i++)
	{
		int face[4];
		MFace *mf = &mface[i];

		gotBytes = gzread(gzf, &(face[0]), sizeof( face[0] ));
		gotBytes = gzread(gzf, &(face[1]), sizeof( face[1] ));
		gotBytes = gzread(gzf, &(face[2]), sizeof( face[2] ));
		face[3] = 0;

		// check if 3rd vertex has index 0 (not allowed in blender)
		if(face[2])
		{
			mf->v1 = face[0];
			mf->v2 = face[1];
			mf->v3 = face[2];
		}
		else
		{
			mf->v1 = face[1];
			mf->v2 = face[2];
			mf->v3 = face[0];
		}
		mf->v4 = face[3];

		test_index_face(mf, NULL, 0, 3);
	}

	gzclose( gzf );

	CDDM_calc_edges(dm);

	CDDM_apply_vert_normals(dm, (short (*)[3])normals);
	MEM_freeN(normals);

	// CDDM_calc_normals(result);

	return dm;
}


void fluid_get_bb(MVert *mvert, int totvert, float obmat[][4],
		 /*RET*/ float start[3], /*RET*/ float size[3] )
{
	float bbsx=0.0, bbsy=0.0, bbsz=0.0;
	float bbex=1.0, bbey=1.0, bbez=1.0;
	int i;
	float vec[3];

	if(totvert == 0) {
		zero_v3(start);
		zero_v3(size);
		return;
	}

	VECCOPY(vec, mvert[0].co);
	mul_m4_v3(obmat, vec);
	bbsx = vec[0]; bbsy = vec[1]; bbsz = vec[2];
	bbex = vec[0]; bbey = vec[1]; bbez = vec[2];

	for(i = 1; i < totvert; i++) {
		VECCOPY(vec, mvert[i].co);
		mul_m4_v3(obmat, vec);

		if(vec[0] < bbsx){ bbsx= vec[0]; }
		if(vec[1] < bbsy){ bbsy= vec[1]; }
		if(vec[2] < bbsz){ bbsz= vec[2]; }
		if(vec[0] > bbex){ bbex= vec[0]; }
		if(vec[1] > bbey){ bbey= vec[1]; }
		if(vec[2] > bbez){ bbez= vec[2]; }
	}

	// return values...
	if(start) {
		start[0] = bbsx;
		start[1] = bbsy;
		start[2] = bbsz;
	}
	if(size) {
		size[0] = bbex-bbsx;
		size[1] = bbey-bbsy;
		size[2] = bbez-bbsz;
	}
}

//-------------------------------------------------------------------------------
// old interface
//-------------------------------------------------------------------------------

void fluid_estimate_memory(Object *ob, FluidsimSettings *fss, char *value)
{
	Mesh *mesh;

	value[0]= '\0';

	if(ob->type == OB_MESH) {
		/* use mesh bounding box and object scaling */
		mesh= ob->data;

		fluid_get_bb(mesh->mvert, mesh->totvert, ob->obmat, fss->bbStart, fss->bbSize);
		elbeemEstimateMemreq(fss->resolutionxyz, fss->bbSize[0],fss->bbSize[1],fss->bbSize[2], fss->maxRefine, value);
	}
}


/* read zipped fluidsim velocities into the co's of the fluidsimsettings normals struct */
void fluidsim_read_vel_cache(FluidsimModifierData *fluidmd, DerivedMesh *dm, char *filename)
{
	int wri, i, j;
	float wrf;
	gzFile gzf;
	FluidsimSettings *fss = fluidmd->fss;
	int len = strlen(filename);
	int totvert = dm->getNumVerts(dm);
	float *velarray = NULL;

	// mesh and vverts have to be valid from loading...

	if(fss->meshSurfNormals)
		MEM_freeN(fss->meshSurfNormals);

	if(len<7)
	{
		return;
	}

	if(fss->domainNovecgen>0) return;

	// abusing pointer to hold an array of 3d-velocities
	fss->meshSurfNormals = MEM_callocN(sizeof(float)*3*dm->getNumVerts(dm), "Fluidsim_velocities");
	// abusing pointer to hold an INT
	fss->meshSurface = SET_INT_IN_POINTER(totvert);

	velarray = (float *)fss->meshSurfNormals;

	// .bobj.gz , correct filename
	// 87654321
	filename[len-6] = 'v';
	filename[len-5] = 'e';
	filename[len-4] = 'l';

	gzf = gzopen(filename, "rb");
	if (!gzf)
	{
		MEM_freeN(fss->meshSurfNormals);
		fss->meshSurfNormals = NULL;
		return;
	}

	gzread(gzf, &wri, sizeof( wri ));
	if(wri != totvert)
	{
		MEM_freeN(fss->meshSurfNormals);
		fss->meshSurfNormals = NULL;
		return;
	}

	for(i=0; i<totvert;i++)
	{
		for(j=0; j<3; j++)
		{
			gzread(gzf, &wrf, sizeof( wrf ));
			velarray[3*i + j] = wrf;
		}
	}

	gzclose(gzf);
}

DerivedMesh *fluidsim_read_cache(Object *ob, DerivedMesh *orgdm, FluidsimModifierData *fluidmd, int framenr, int useRenderParams)
{
	int displaymode = 0;
	int curFrame = framenr - 1 /*scene->r.sfra*/; /* start with 0 at start frame */
	char targetDir[FILE_MAXFILE+FILE_MAXDIR], targetFile[FILE_MAXFILE+FILE_MAXDIR];
	FluidsimSettings *fss = fluidmd->fss;
	DerivedMesh *dm = NULL;
	MFace *mface;
	int numfaces;
	int mat_nr, flag, i;

	if(!useRenderParams) {
		displaymode = fss->guiDisplayMode;
	} else {
		displaymode = fss->renderDisplayMode;
	}

	strncpy(targetDir, fss->surfdataPath, FILE_MAXDIR);

	// use preview or final mesh?
	if(displaymode==1)
	{
		// just display original object
		return NULL;
	}
	else if(displaymode==2)
	{
		strcat(targetDir,"fluidsurface_preview_####");
	}
	else
	{ // 3
		strcat(targetDir,"fluidsurface_final_####");
	}

	BLI_path_abs(targetDir, G.sce);
	BLI_path_frame(targetDir, curFrame, 0); // fixed #frame-no

	strcpy(targetFile,targetDir);
	strcat(targetFile, ".bobj.gz");

	dm = fluidsim_read_obj(targetFile);

	if(!dm)
	{
		// switch, abort background rendering when fluidsim mesh is missing
		const char *strEnvName2 = "BLENDER_ELBEEMBOBJABORT"; // from blendercall.cpp

		if(G.background==1) {
			if(getenv(strEnvName2)) {
				int elevel = atoi(getenv(strEnvName2));
				if(elevel>0) {
					printf("Env. var %s set, fluid sim mesh '%s' not found, aborting render...\n",strEnvName2, targetFile);
					exit(1);
				}
			}
		}

		// display org. object upon failure which is in dm
		return NULL;
	}

	// assign material + flags to new dm
	mface = orgdm->getFaceArray(orgdm);
	mat_nr = mface[0].mat_nr;
	flag = mface[0].flag;

	mface = dm->getFaceArray(dm);
	numfaces = dm->getNumFaces(dm);
	for(i=0; i<numfaces; i++)
	{
		mface[i].mat_nr = mat_nr;
		mface[i].flag = flag;
	}

	// load vertex velocities, if they exist...
	// TODO? use generate flag as loading flag as well?
	// warning, needs original .bobj.gz mesh loading filename
	if(displaymode==3)
	{
		fluidsim_read_vel_cache(fluidmd, dm, targetFile);
	}
	else
	{
		if(fss->meshSurfNormals)
			MEM_freeN(fss->meshSurfNormals);

		fss->meshSurfNormals = NULL;
	}

	return dm;
}

#endif // DISABLE_ELBEEM

DerivedMesh *fluidsimModifier_do(FluidsimModifierData *fluidmd, Scene *scene, Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc)
{
#ifndef DISABLE_ELBEEM
	DerivedMesh *result = NULL;
	int framenr;
	FluidsimSettings *fss = NULL;

	framenr= (int)scene->r.cfra;
	
	// only handle fluidsim domains
	if(fluidmd && fluidmd->fss && (fluidmd->fss->type != OB_FLUIDSIM_DOMAIN))
		return dm;
	
	// sanity check
	if(!fluidmd || (fluidmd && !fluidmd->fss))
		return dm;
	
	fss = fluidmd->fss;
	
	// timescale not supported yet
	// clmd->sim_parms->timescale= timescale;
	
	// support reversing of baked fluid frames here
	if((fss->flag & OB_FLUIDSIM_REVERSE) && (fss->lastgoodframe >= 0))
	{
		framenr = fss->lastgoodframe - framenr + 1;
		CLAMP(framenr, 1, fss->lastgoodframe);
	}
	
	/* try to read from cache */
	if(((fss->lastgoodframe >= framenr) || (fss->lastgoodframe < 0)) && (result = fluidsim_read_cache(ob, dm, fluidmd, framenr, useRenderParams)))
	{
		// fss->lastgoodframe = framenr; // set also in src/fluidsim.c
		return result;
	}
	else
	{	
		// display last known good frame
		if(fss->lastgoodframe >= 0)
		{
			if((result = fluidsim_read_cache(ob, dm, fluidmd, fss->lastgoodframe, useRenderParams))) 
			{
				return result;
			}
			
			// it was supposed to be a valid frame but it isn't!
			fss->lastgoodframe = framenr - 1;
			
			
			// this could be likely the case when you load an old fluidsim
			if((result = fluidsim_read_cache(ob, dm, fluidmd, fss->lastgoodframe, useRenderParams))) 
			{
				return result;
			}
		}
		
		result = CDDM_copy(dm);

		if(result) 
		{
			return result;
		}
	}
	
	return dm;
#else
	return NULL;
#endif
}
