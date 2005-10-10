/**
 * fluidsim.c
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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */



#include <math.h>
#include <stdlib.h>
#include <string.h>


#include "MEM_guardedalloc.h"

/* types */
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_object_fluidsim.h"	
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"
#include "DNA_screen_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "MTC_matrixops.h"

#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_key.h"
#include "BKE_scene.h"
#include "BKE_object.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "LBM_fluidsim.h"

#include "BLI_editVert.h"
#include "BIF_editdeform.h"
#include "BIF_gl.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_cursors.h"

#include "mydevice.h"

#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_mutex.h"
#include <sys/stat.h>

#ifdef WIN32	/* Windos */
//#include "BLI_winstuff.h"
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif
// SDL redefines main for SDL_main, not needed here...
#undef main

#ifdef __APPLE__	/* MacOS X */
#undef main
#endif

/* from header info.c */
extern int start_progress_bar(void);
extern void end_progress_bar(void);
extern int progress_bar(float done, char *busy_info);

double fluidsimViscosityPreset[6] = {
	-1.0,	/* unused */
	-1.0,	/* manual */
	1.0e-6, /* water */
	5.0e-5, /* some (thick) oil */
	2.0e-3, /* ca. honey */
	-1.0	/* end */
};

char* fluidsimViscosityPresetString[6] = {
	"UNUSED",	/* unused */
	"UNUSED",	/* manual */
	"  = 1.0 * 10^-6", /* water */
	"  = 5.0 * 10^-5", /* some (thick) oil */
	"  = 2.0 * 10^-3", /* ca. honey */
	"INVALID"	/* end */
};

typedef struct {
	DerivedMesh dm;

	// similar to MeshDerivedMesh
	struct Object *ob;	// pointer to parent object
	float *extverts, *nors; // face normals, colors?
	Mesh *fsmesh;	// mesh struct to display (either surface, or original one)
	char meshFree;	// free the mesh afterwards? (boolean)
} fluidsimDerivedMesh;


/* ********************** fluid sim settings struct functions ********************** */

/* allocates and initializes general main data */
FluidsimSettings *fluidsimSettingsNew(struct Object *srcob)
{
	char blendDir[FILE_MAXDIR], blendFile[FILE_MAXFILE];
	FluidsimSettings *fss;
	fss= MEM_callocN( sizeof(FluidsimSettings), "fluidsimsettings memory");
	
	fss->type = 0;
	fss->show_advancedoptions = 0;

	fss->resolutionxyz = 50;
	fss->previewresxyz = 25;
	fss->realsize = 0.03;
	fss->guiDisplayMode = 2; // preview
	fss->renderDisplayMode = 3; // render

	fss->viscosityMode = 2; // default to water
	fss->viscosityValue = 0.1;
	fss->viscosityExponent = 6;
	fss->gravx = 0.0;
	fss->gravy = 0.0;
	fss->gravz = -9.81;
	fss->animStart = 0.0; 
	fss->animEnd = 0.30;
	fss->gstar = 0.005; // used as normgstar
	fss->maxRefine = -1;
	// maxRefine is set according to resolutionxyz during bake

	// fluid settings
	fss->iniVelx = 
	fss->iniVely = 
	fss->iniVelz = 0.0;

	strcpy(fss->surfdataDir,"//"); // current dir
	// fss->surfdataPrefix take from .blend filename
	strcpy(blendDir, G.sce);
	BLI_splitdirstring(blendDir, blendFile);
	snprintf(fss->surfdataPrefix,FILE_MAXFILE,"%s_%s", blendFile, srcob->id.name);
	
	fss->orgMesh = (Mesh *)srcob->data;
	return fss;
}

/* free struct */
void fluidsimSettingsFree(FluidsimSettings *fss)
{
	MEM_freeN(fss);
}


/* helper function */
void getGeometryObjFilename(struct Object *ob, char *dst, char *srcname) {
	snprintf(dst,FILE_MAXFILE, "%s_cfgdata_%s.bobj.gz", srcname, ob->id.name);
}


/* ********************** simulation thread             ************************* */
SDL_mutex	*globalBakeLock=NULL;
int			globalBakeState = 0; // 0 everything ok, -1 abort simulation, 1 sim done
int			globalBakeFrame = 0;

// run simulation in seperate thread
int simulateThread(void *ptr) {
	char* fnameCfgPath = (char*)(ptr);
	int ret;
	
 	ret = performElbeemSimulation(fnameCfgPath);
	SDL_mutexP(globalBakeLock);
	globalBakeState = 1;
	SDL_mutexV(globalBakeLock);
	return ret;
}

// called by simulation to set frame no.
void simulateThreadIncreaseFrame(void) {
	if(!globalBakeLock) return;
	if(globalBakeState<0) return; // this means abort...
	SDL_mutexP(globalBakeLock);
	globalBakeFrame++;
	SDL_mutexV(globalBakeLock);
}

/* ********************** write fluidsim config to file ************************* */
void fluidsimBake(struct Object *ob)
{
	char fnameCfg[FILE_MAXFILE], fnameCfgPath[FILE_MAXFILE+FILE_MAXDIR];
	FILE *fileCfg;
	struct Object *fsDomain = NULL;
	FluidsimSettings *fssDomain;
	struct Object *obit = NULL; /* object iterator */
	int origFrame = G.scene->r.cfra;
	char blendDir[FILE_MAXDIR], blendFile[FILE_MAXFILE];
	char curWd[FILE_MAXDIR];
	char debugStrBuffer[256];
	int dirExist = 0;
	const int maxRes = 200;
	int gridlevels = 0;

	const char *strEnvName = "BLENDER_ELBEEMDEBUG"; // from blendercall.cpp

	// test section
	// int nr= pupmenu("Continue?%t|Yes%x1|No%x0");
	// if(nr==0) return;

	if(getenv(strEnvName)) {
		int dlevel = atoi(getenv(strEnvName));
		elbeemSetDebugLevel(dlevel);
		//if((dlevel>0) && (dlevel<=10)) debugBake = 1;
		snprintf(debugStrBuffer,256,"fluidsimBake::msg: Debug messages activated due to  envvar '%s'\n",strEnvName); 
		elbeemDebugOut(debugStrBuffer);
	}

	/* check if there's another domain... */
	for(obit= G.main->object.first; obit; obit= obit->id.next) {
		if((obit->fluidsimFlag & OB_FLUIDSIM_ENABLE)&&(obit->type==OB_MESH)) {
			if(obit->fluidsimSettings->type == OB_FLUIDSIM_DOMAIN) {
				if(obit != ob) {
					snprintf(debugStrBuffer,256,"fluidsimBake::warning - More than one domain!\n"); 
					elbeemDebugOut(debugStrBuffer);
				}
			}
		}
	}
	/* these both have to be valid, otherwise we wouldnt be here...*/
	fsDomain = ob;
	fssDomain = ob->fluidsimSettings;
	/* rough check of settings... */
	if(fssDomain->resolutionxyz>maxRes) {
		fssDomain->resolutionxyz = maxRes;
		snprintf(debugStrBuffer,256,"fluidsimBake::warning - Resolution (%d) > %d^3, this requires more than 600MB of memory... restricting to %d^3 for now.\n",  fssDomain->resolutionxyz, maxRes, maxRes); 
		elbeemDebugOut(debugStrBuffer);
	}
	if(fssDomain->previewresxyz > fssDomain->resolutionxyz) {
		snprintf(debugStrBuffer,256,"fluidsimBake::warning - Preview (%d) >= Resolution (%d)... setting equal.\n", fssDomain->previewresxyz ,  fssDomain->resolutionxyz); 
		elbeemDebugOut(debugStrBuffer);
		fssDomain->previewresxyz = fssDomain->resolutionxyz;
	}
	// set adaptive coarsening according to resolutionxyz
	// this should do as an approximation, with in/outflow
	// doing this more accurate would be overkill
	// perhaps add manual setting?
	if(fssDomain->maxRefine <0) {
		if(fssDomain->resolutionxyz>128) {
			gridlevels = 2;
		} else
		if(fssDomain->resolutionxyz>64) {
			gridlevels = 1;
		} else {
			gridlevels = 0;
		}
	} else {
		gridlevels = fssDomain->maxRefine;
	}
	snprintf(debugStrBuffer,256,"fluidsimBake::msg: Baking %s, refine: %d\n", fsDomain->id.name , gridlevels ); 
	elbeemDebugOut(debugStrBuffer);
	

	// prepare names...
	strcpy(curWd, G.sce);
	BLI_splitdirstring(curWd, blendFile);
	if(strlen(curWd)<1) {
		BLI_getwdN(curWd);
	}
	if(strlen(fsDomain->fluidsimSettings->surfdataPrefix)<1) {
		// make new from current .blend filename , and domain object name
		strcpy(blendDir, G.sce);
		BLI_splitdirstring(blendDir, blendFile);
		// todo... strip .blend 
		snprintf(fsDomain->fluidsimSettings->surfdataPrefix,FILE_MAXFILE,"%s_%s", blendFile, fsDomain->id.name);
		snprintf(debugStrBuffer,256,"fluidsimBake::error - warning resetting output prefix to '%s'\n", fsDomain->fluidsimSettings->surfdataPrefix); 
		elbeemDebugOut(debugStrBuffer);
	}

	// check selected directory
#ifdef WIN32
	// windows workaroung because stat seems to be broken...
	// simply try to open cfg file for writing
	snprintf(fnameCfg,FILE_MAXFILE,"%s.cfg", fsDomain->fluidsimSettings->surfdataPrefix);
	BLI_make_file_string(curWd, fnameCfgPath, fsDomain->fluidsimSettings->surfdataDir, fnameCfg);
	fileCfg = fopen(fnameCfgPath, "w");
	if(fileCfg) {
		dirExist = 1;
		fclose(fileCfg);
	}
#else // WIN32
	BLI_make_file_string(curWd, fnameCfgPath, fsDomain->fluidsimSettings->surfdataDir, "");
	if(S_ISDIR(BLI_exist(fnameCfgPath))) dirExist = 1;
#endif // WIN32

	if((strlen(fsDomain->fluidsimSettings->surfdataDir)<1) || (!dirExist)) {
		// invalid dir, reset to current
		strcpy(fsDomain->fluidsimSettings->surfdataDir, "//");
		snprintf(debugStrBuffer,256,"fluidsimBake::error - warning resetting output dir to '%s'\n", fsDomain->fluidsimSettings->surfdataDir);
		elbeemDebugOut(debugStrBuffer);
	}
	
	// dump data for frame 0
  G.scene->r.cfra = 0;
  scene_update_for_newframe(G.scene, G.scene->lay);

	snprintf(fnameCfg,FILE_MAXFILE,"%s.cfg", fsDomain->fluidsimSettings->surfdataPrefix);
	BLI_make_file_string(curWd, fnameCfgPath, fsDomain->fluidsimSettings->surfdataDir, fnameCfg);

	// start writing
	fileCfg = fopen(fnameCfgPath, "w");
	if(!fileCfg) {
		snprintf(debugStrBuffer,256,"fluidsimBake::error - Unable to open file for writing '%s'\n", fnameCfgPath); 
		elbeemDebugOut(debugStrBuffer);
		return;
	}

	fprintf(fileCfg, "# Blender ElBeem File , Source %s , Frame %d, to %s \n\n\n", G.sce, -1, fnameCfg );

	// FIXME set aniframetime from no. frames and duration
	/* output simulation  settings */
	{
		int noFrames = G.scene->r.efra - G.scene->r.sfra;
		double calcViscosity = 0.0;
		double animFrameTime = (fssDomain->animEnd - fssDomain->animStart)/(double)noFrames;
		char *simString = "\n"
		"attribute \"simulation1\" { \n" 
		
		"  p_domainsize  = " "%f" /* 0 realsize */ "; \n" 
		"  p_anistart    = " "%f" /* 1 aniStart*/ "; #cfgset \n" 
		"  p_aniframetime = " "%f" /* 2 aniFrameTime*/ "; #cfgset \n" 
		"  solver = \"fsgr\"; \n"  "\n" 
		"  initsurfsmooth = 0; \n"  "\n" 
		"  debugvelscale = 0.005; \n"  "\n" 
		"  isovalue =  0.4900; \n" 
		"  isoweightmethod = 1; \n"  "\n" 
		"  disable_stfluidinit = 0; \n"  "\n" 
		
		"  geoinit   = 1; \n" 
		"  geoinitid = 1;  \n"  "\n" 
		"  p_gravity = " "%f %f %f" /* 3,4,5 pGravity*/ "; #cfgset \n"  "\n" 
		
		"  timeadap = 1;  \n" 
		"  p_tadapmaxomega = 2.0; \n" 
		"  p_normgstar = %f; \n"  /* 6b use gstar param? */
		"  p_viscosity = " "%f" /* 7 pViscosity*/ "; #cfgset \n"  "\n" 
		
		"  maxrefine = " "%d" /* 8 maxRefine*/ "; #cfgset  \n" 
		"  size = " "%d" /* 9 gridSize*/ "; #cfgset  \n" 
		"  surfacepreview = " "%d" /* 10 previewSize*/ "; #cfgset \n" 
		"  smoothsurface = 1.0;  \n"
		"\n" 
		//"  //forcetadaprefine = 0; maxrefine = 0; \n" 
		"} \n" ;
    
		if(fssDomain->viscosityMode==1) {
			/* manual mode */
			calcViscosity = (1.0/(fssDomain->viscosityExponent*10)) * fssDomain->viscosityValue;
		} else {
			calcViscosity = fluidsimViscosityPreset[ fssDomain->viscosityMode ];
		}
		fprintf(fileCfg, simString,
				(double)fssDomain->realsize, 
				(double)fssDomain->animStart, animFrameTime ,
				(double)fssDomain->gravx, (double)fssDomain->gravy, (double)fssDomain->gravz,
				(double)fssDomain->gstar,
				calcViscosity,
				gridlevels, (int)fssDomain->resolutionxyz, (int)fssDomain->previewresxyz 
				);
	}

	// output blender object transformation
	{
		float domainMat[4][4];
		float invDomMat[4][4];
		char* blendattrString = "\n" 
			"attribute \"btrafoattr\" { \n"
			"  transform = %f %f %f %f   "
			           "   %f %f %f %f   "
			           "   %f %f %f %f   "
			           "   %f %f %f %f ;\n"
			"} \n";

		MTC_Mat4CpyMat4(domainMat, fsDomain->obmat);
		if(!Mat4Invert(invDomMat, domainMat)) {
			snprintf(debugStrBuffer,256,"fluidsimBake::error - Invalid obj matrix?\n"); 
			elbeemDebugOut(debugStrBuffer);
			// FIXME add fatal msg
			return;
		}

		fprintf(fileCfg, blendattrString,
				invDomMat[0][0],invDomMat[1][0],invDomMat[2][0],invDomMat[3][0], 
				invDomMat[0][1],invDomMat[1][1],invDomMat[2][1],invDomMat[3][1], 
				invDomMat[0][2],invDomMat[1][2],invDomMat[2][2],invDomMat[3][2], 
				invDomMat[0][3],invDomMat[1][3],invDomMat[2][3],invDomMat[3][3] );
	}



	fprintf(fileCfg, "raytracing {\n");

	/* output picture settings for preview renders */
	{
		char *rayString = "\n" 
			"  anistart=     0; \n" 
			"  aniframes=    " "%d" /*1 frameEnd-frameStart+0*/ "; #cfgset \n" 
			"  frameSkip=    false; \n" 
			"  filename=     \"" "%s" /* rayPicFilename*/  "\"; #cfgset \n" 
			"  aspect      1.0; \n" 
			"  resolution  " "%d %d" /*2,3 blendResx,blendResy*/ "; #cfgset \n" 
			"  antialias       1; \n" 
			"  ambientlight    (1, 1, 1); \n" 
			"  maxRayDepth       6; \n" 
			"  treeMaxDepth     25; \n" 
			"  treeMaxTriangles  8; \n" 
			"  background  (0.08,  0.08, 0.20); \n" 
			"  eyepoint= (" "%f %f %f"/*4,5,6 eyep*/ "); #cfgset  \n" 
			"  lookat= (" "%f %f %f"/*7,8,9 lookatp*/ "); #cfgset  \n" 
			"  upvec= (0 0 1);  \n" 
			"  fovy=  " "%f" /*blendFov*/ "; #cfgset \n" 
			"  blenderattr= \"btrafoattr\"; \n"
			"\n\n";

		char *lightString = "\n" 
			"  light { \n" 
			"    type= omni; \n" 
			"    active=     1; \n" 
			"    color=      (1.0,  1.0,  1.0); \n" 
			"    position=   (" "%f %f %f"/*1,2,3 eyep*/ "); #cfgset \n" 
			"    castShadows= 1; \n"  
			"  } \n\n" ;

		int noFrames = (G.scene->r.efra - G.scene->r.sfra) +1; // FIXME - check no. of frames...
		struct Object *cam = G.scene->camera;
		float  eyex=2.0, eyey=2.0, eyez=2.0;
		int    resx = 200, resy=200;
		float  lookatx=0.0, lookaty=0.0, lookatz=0.0;
		float  fov = 45.0;
		char   fnamePreview[FILE_MAXFILE];
		char   fnamePreviewPath[FILE_MAXFILE+FILE_MAXDIR];

		snprintf(fnamePreview,FILE_MAXFILE,"%s_surface", fsDomain->fluidsimSettings->surfdataPrefix );
		BLI_make_file_string(curWd, fnamePreviewPath, fsDomain->fluidsimSettings->surfdataDir, fnamePreview);
		resx = G.scene->r.xsch;
		resy = G.scene->r.ysch;
		if((cam) && (cam->type == OB_CAMERA)) {
			Camera *camdata= G.scene->camera->data;
			double lens = camdata->lens;
			double imgRatio = (double)resx/(double)resy;
			fov = 360.0 * atan(16.0*imgRatio/lens) / M_PI;
			//R.near= camdata->clipsta; R.far= camdata->clipend;

			eyex = cam->loc[0];
			eyey = cam->loc[1];
			eyez = cam->loc[2];
			// TODO - place lookat in middle of domain?
		}

		fprintf(fileCfg, rayString,
				noFrames, fnamePreviewPath, resx,resy,
				eyex, eyey, eyez ,
				lookatx, lookaty, lookatz,
				fov
				);
		fprintf(fileCfg, lightString, 
				eyex, eyey, eyez );
	}


	/* output fluid domain */
	{
		float bbsx=0.0, bbsy=0.0, bbsz=0.0;
		float bbex=1.0, bbey=1.0, bbez=1.0;
		char * domainString = "\n" 
			"  geometry { \n" 
			"    type= fluidlbm; \n" 
			"    name = \""   "%s" /*name*/   "\"; #cfgset \n" 
			"    visible=  1; \n" 
			"    attributes=  \"simulation1\"; \n" 
			//"    define { material_surf  = \"fluidblue\"; } \n" 
			"    start= " "%f %f %f" /*bbstart*/ "; #cfgset \n" 
			"    end  = " "%f %f %f" /*bbend  */ "; #cfgset \n" 
			"  } \n" 
			"\n";
		Mesh *mesh = fsDomain->data; 
		//BoundBox *bb = fsDomain->bb;
		//if(!bb) { bb = mesh->bb; }
		//bb = NULL; // TODO test existing bounding box...

		//if(!bb && (mesh->totvert>0) ) 
		{ 
			int i;
			float vec[3];
			VECCOPY(vec, mesh->mvert[0].co); 
			Mat4MulVecfl(fsDomain->obmat, vec);
			bbsx = vec[0]; bbsy = vec[1]; bbsz = vec[2];
			bbex = vec[0]; bbey = vec[1]; bbez = vec[2];
			for(i=1; i<mesh->totvert;i++) {
				VECCOPY(vec, mesh->mvert[i].co); /* get transformed point */
				Mat4MulVecfl(fsDomain->obmat, vec);

				if(vec[0] < bbsx){ bbsx= vec[0]; }
				if(vec[1] < bbsy){ bbsy= vec[1]; }
				if(vec[2] < bbsz){ bbsz= vec[2]; }
				if(vec[0] > bbex){ bbex= vec[0]; }
				if(vec[1] > bbey){ bbey= vec[1]; }
				if(vec[2] > bbez){ bbez= vec[2]; }
			}
		}
		fprintf(fileCfg, domainString,
			fsDomain->id.name, 
			bbsx, bbsy, bbsz,
			bbex, bbey, bbez
		);
	}

    
	/* setup geometry */
	{
		char *objectStringStart = 
			"  geometry { \n" 
			"    type= objmodel; \n" 
			"    name = \""   "%s" /* name */   "\"; #cfgset \n" 
			// DEBUG , also obs invisible?
			"    visible=  0; \n" 
			"    define { \n" ;
		char *obstacleString = 
			"      geoinittype= \"" "%s" /* type */  "\"; #cfgset \n" 
			"      filename= \""   "%s" /* data  filename */  "\"; #cfgset \n" ;
		char *fluidString = 
			"      geoinittype= \"" "%s" /* type */  "\"; \n" 
			"      filename= \""   "%s" /* data  filename */  "\"; #cfgset \n" 
			"      initial_velocity= "   "%f %f %f" /* vel vector */  "; #cfgset \n" ;
		char *objectStringEnd = 
			"      geoinit_intersect = 1; \n"  /* always use accurate init here */
			"      geoinitid= 1; \n" 
			"    } \n" 
			"  } \n" 
			"\n" ;
		char fnameObjdat[FILE_MAXFILE];
		char bobjPath[FILE_MAXFILE+FILE_MAXDIR];
        
		for(obit= G.main->object.first; obit; obit= obit->id.next) {
			//{ snprintf(debugStrBuffer,256,"DEBUG object name=%s, type=%d ...\n", obit->id.name, obit->type); elbeemDebugOut(debugStrBuffer); } // DEBUG
			if( (obit->fluidsimFlag & OB_FLUIDSIM_ENABLE) && 
					(obit->type==OB_MESH) &&
				  (obit->fluidsimSettings->type != OB_FLUIDSIM_DOMAIN)
				) {
					getGeometryObjFilename(obit, fnameObjdat, fsDomain->fluidsimSettings->surfdataPrefix);
					BLI_make_file_string(curWd, bobjPath, fsDomain->fluidsimSettings->surfdataDir, fnameObjdat);
					fprintf(fileCfg, objectStringStart, obit->id.name ); // abs path
					if(obit->fluidsimSettings->type == OB_FLUIDSIM_FLUID) {
						fprintf(fileCfg, fluidString, "fluid", bobjPath, // do use absolute paths?
							(double)obit->fluidsimSettings->iniVelx, (double)obit->fluidsimSettings->iniVely, (double)obit->fluidsimSettings->iniVelz );
					}
					if(obit->fluidsimSettings->type == OB_FLUIDSIM_INFLOW) {
						fprintf(fileCfg, fluidString, "inflow", bobjPath, // do use absolute paths?
							(double)obit->fluidsimSettings->iniVelx, (double)obit->fluidsimSettings->iniVely, (double)obit->fluidsimSettings->iniVelz );
					}
					if(obit->fluidsimSettings->type == OB_FLUIDSIM_OUTFLOW) {
						fprintf(fileCfg, fluidString, "outflow", bobjPath, // do use absolute paths?
							(double)obit->fluidsimSettings->iniVelx, (double)obit->fluidsimSettings->iniVely, (double)obit->fluidsimSettings->iniVelz );
					}
					if(obit->fluidsimSettings->type == OB_FLUIDSIM_OBSTACLE) {
						fprintf(fileCfg, obstacleString, "bnd_no" , bobjPath); // abs path
					}
					fprintf(fileCfg, objectStringEnd ); // abs path
					writeBobjgz(bobjPath, obit);
			}
		}
	}
  
	/* fluid material */
	fprintf(fileCfg, 
		"  material { \n"
		"    type= phong; \n"
		"    name=          \"fluidblue\"; \n"
		"    diffuse=       0.3 0.5 0.9; \n"
		"    ambient=       0.1 0.1 0.1; \n"
		"    specular=      0.2  10.0; \n"
		"  } \n" );



	fprintf(fileCfg, "} // end raytracing\n");
	fclose(fileCfg);
	snprintf(debugStrBuffer,256,"fluidsimBake::msg: Wrote %s\n", fnameCfg); 
	elbeemDebugOut(debugStrBuffer);

	// perform simulation
	{
		SDL_Thread *simthr = NULL;
		globalBakeLock = SDL_CreateMutex();
		globalBakeState = 0;
		globalBakeFrame = 1;
		simthr = SDL_CreateThread(simulateThread, fnameCfgPath);
#ifndef WIN32
		// DEBUG for win32 debugging, dont use threads...
#endif // WIN32
		if(!simthr) {
			snprintf(debugStrBuffer,256,"fluidsimBake::error: Unable to create thread... running without one.\n"); 
			elbeemDebugOut(debugStrBuffer);
			set_timecursor(0);
			performElbeemSimulation(fnameCfgPath);
		} else {
			int done = 0;
			unsigned short event=0;
			short val;
			float noFramesf = G.scene->r.efra - G.scene->r.sfra +1;
			float percentdone = 0.0;
			int lastRedraw = -1;
			
			start_progress_bar();

			while(done==0) { 	    
				char busy_mess[80];
				
				waitcursor(1);
				
				// lukep we add progress bar as an interim mesure
				percentdone = globalBakeFrame / noFramesf;
				sprintf(busy_mess, "baking fluids %d / %d       |||", globalBakeFrame, (int) noFramesf);
				progress_bar(percentdone, busy_mess );
				
				SDL_Delay(2000); // longer delay to prevent frequent redrawing
				SDL_mutexP(globalBakeLock);
				if(globalBakeState == 1) done = 1;
				SDL_mutexV(globalBakeLock);

				while(qtest()) {
					event = extern_qread(&val);
					if(event == ESCKEY) {
						// abort...
						SDL_mutexP(globalBakeLock);
						done = -1;
						globalBakeFrame = 0;
						globalBakeState = -1;
						SDL_mutexV(globalBakeLock);
						break;
					}
				} 

				// redraw the 3D for showing progress once in a while...
				if(lastRedraw!=globalBakeFrame) {
					ScrArea *sa;
					G.scene->r.cfra = lastRedraw = globalBakeFrame;
					update_for_newframe_muted();
					sa= G.curscreen->areabase.first;
					while(sa) {
						if(sa->spacetype == SPACE_VIEW3D) { scrarea_do_windraw(sa); }
						sa= sa->next;	
					} 
					screen_swapbuffers();
				} // redraw
			}
			SDL_WaitThread(simthr,NULL);
			end_progress_bar();
		}
		SDL_DestroyMutex(globalBakeLock);
		globalBakeLock = NULL;
	} // thread creation

	// go back to "current" blender time
	waitcursor(0);
  G.scene->r.cfra = origFrame;
  scene_update_for_newframe(G.scene, G.scene->lay);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
}


