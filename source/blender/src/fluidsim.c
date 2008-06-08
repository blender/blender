/**
 * fluidsim.c
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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
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
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h" 

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "MTC_matrixops.h"

#include "BKE_customdata.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_key.h"
#include "BKE_scene.h"
#include "BKE_object.h"
#include "BKE_softbody.h"
#include "BKE_DerivedMesh.h"
#include "BKE_ipo.h"
#include "LBM_fluidsim.h"
// warning - double elbeem.h in intern/extern...
#include "elbeem.h"

#include "BLI_editVert.h"
#include "BIF_editdeform.h"
#include "BIF_gl.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_cursors.h"
#include "BIF_interface.h"
#include "BSE_headerbuttons.h"

#include "mydevice.h"
#include "blendef.h"
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

// from DerivedMesh.c
void initElbeemMesh(struct Object *ob, int *numVertices, float **vertices, int *numTriangles, int **triangles, int useGlobalCoords);

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



/* enable/disable overall compilation */
#ifndef DISABLE_ELBEEM


/* ********************** fluid sim settings struct functions ********************** */

/* allocates and initializes general main data */

FluidsimSettings *fluidsimSettingsNew(struct Object *srcob)
{
	//char blendDir[FILE_MAXDIR], blendFile[FILE_MAXFILE];
	FluidsimSettings *fss;
	
	/* this call uses derivedMesh methods... */
	if(srcob->type!=OB_MESH) return NULL;
	
	fss= MEM_callocN( sizeof(FluidsimSettings), "fluidsimsettings memory");
	
	fss->type = 0;
	fss->show_advancedoptions = 0;

	fss->resolutionxyz = 50;
	fss->previewresxyz = 25;
	fss->realsize = 0.03;
	fss->guiDisplayMode = 2; // preview
	fss->renderDisplayMode = 3; // render

	fss->viscosityMode = 2; // default to water
	fss->viscosityValue = 1.0;
	fss->viscosityExponent = 6;
	fss->gravx = 0.0;
	fss->gravy = 0.0;
	fss->gravz = -9.81;
	fss->animStart = 0.0; 
	fss->animEnd = 0.30;
	fss->gstar = 0.005; // used as normgstar
	fss->maxRefine = -1;
	// maxRefine is set according to resolutionxyz during bake

	// fluid/inflow settings
	fss->iniVelx = 
	fss->iniVely = 
	fss->iniVelz = 0.0;

	/*  elubie: changed this to default to the same dir as the render output
		to prevent saving to C:\ on Windows */
	BLI_strncpy(fss->surfdataPath, btempdir, FILE_MAX); 
	fss->orgMesh = (Mesh *)srcob->data;
	fss->meshSurface = NULL;
	fss->meshBB = NULL;
	fss->meshSurfNormals = NULL;

	// first init of bounding box
	fss->bbStart[0] = 0.0;
	fss->bbStart[1] = 0.0;
	fss->bbStart[2] = 0.0;
	fss->bbSize[0] = 1.0;
	fss->bbSize[1] = 1.0;
	fss->bbSize[2] = 1.0;
	fluidsimGetAxisAlignedBB(srcob->data, srcob->obmat, fss->bbStart, fss->bbSize, &fss->meshBB);
	
	// todo - reuse default init from elbeem!
	fss->typeFlags = 0;
	fss->domainNovecgen = 0;
	fss->volumeInitType = 1; // volume
	fss->partSlipValue = 0.0;

	fss->generateTracers = 0;
	fss->generateParticles = 0.0;
	fss->surfaceSmoothing = 1.0;
	fss->surfaceSubdivs = 1.0;
	fss->particleInfSize = 0.0;
	fss->particleInfAlpha = 0.0;

	return fss;
}

/* duplicate struct, analogous to free */
static Mesh *fluidsimCopyMesh(Mesh *me)
{
	Mesh *dup = MEM_dupallocN(me);

	CustomData_copy(&me->vdata, &dup->vdata, CD_MASK_MESH, CD_DUPLICATE, me->totvert);
	CustomData_copy(&me->edata, &dup->edata, CD_MASK_MESH, CD_DUPLICATE, me->totedge);
	CustomData_copy(&me->fdata, &dup->fdata, CD_MASK_MESH, CD_DUPLICATE, me->totface);

	return dup;
}

FluidsimSettings* fluidsimSettingsCopy(FluidsimSettings *fss)
{
	FluidsimSettings *dupfss;

	if(!fss) return NULL;
	dupfss = MEM_dupallocN(fss);

	if(fss->meshSurface)
		dupfss->meshSurface = fluidsimCopyMesh(fss->meshSurface);
	if(fss->meshBB)
		dupfss->meshBB = fluidsimCopyMesh(fss->meshBB);

	if(fss->meshSurfNormals) dupfss->meshSurfNormals = MEM_dupallocN(fss->meshSurfNormals);

	return dupfss;
}

/* free struct */
static void fluidsimFreeMesh(Mesh *me)
{
	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);

	MEM_freeN(me);
}

void fluidsimSettingsFree(FluidsimSettings *fss)
{
	if(fss->meshSurface) {
		fluidsimFreeMesh(fss->meshSurface);
		fss->meshSurface = NULL;
	}
	if(fss->meshBB) {
		fluidsimFreeMesh(fss->meshBB);
		fss->meshBB = NULL;
	}

	if(fss->meshSurfNormals){ MEM_freeN(fss->meshSurfNormals); fss->meshSurfNormals=NULL; } 

	MEM_freeN(fss);
}


/* helper function */
void fluidsimGetGeometryObjFilename(struct Object *ob, char *dst) { //, char *srcname) {
	//snprintf(dst,FILE_MAXFILE, "%s_cfgdata_%s.bobj.gz", srcname, ob->id.name);
	snprintf(dst,FILE_MAXFILE, "fluidcfgdata_%s.bobj.gz", ob->id.name);
}




/* ******************************************************************************** */
/* ********************** fluid sim channel helper functions ********************** */
/* ******************************************************************************** */

// no. of entries for the two channel sizes
#define CHANNEL_FLOAT 1
#define CHANNEL_VEC   3

#define FS_FREE_ONECHANNEL(c,str) { \
	if(c){ MEM_freeN(c); c=NULL; } \
} // end ONE CHANN, debug: fprintf(stderr,"freeing " str " \n"); 

#define FS_FREE_CHANNELS { \
	FS_FREE_ONECHANNEL(timeAtIndex,"timeAtIndex");\
	FS_FREE_ONECHANNEL(timeAtFrame,"timeAtFrame");\
	FS_FREE_ONECHANNEL(channelDomainTime,"channelDomainTime"); \
	FS_FREE_ONECHANNEL(channelDomainGravity,"channelDomainGravity");\
	FS_FREE_ONECHANNEL(channelDomainViscosity,"channelDomainViscosity");\
	for(i=0;i<256;i++) { \
		FS_FREE_ONECHANNEL(channelObjMove[i][0],"channelObjMove0"); \
		FS_FREE_ONECHANNEL(channelObjMove[i][1],"channelObjMove1"); \
		FS_FREE_ONECHANNEL(channelObjMove[i][2],"channelObjMove2"); \
		FS_FREE_ONECHANNEL(channelObjInivel[i],"channelObjInivel"); \
		FS_FREE_ONECHANNEL(channelObjActive[i],"channelObjActive"); \
	}  \
} // end FS FREE CHANNELS


// simplify channels before printing
// for API this is done anyway upon init
#if 0
static void fluidsimPrintChannel(FILE *file, float *channel, int paramsize, char *str, int entries) 
{ 
	int i,j; 
	int channelSize = paramsize; 

	if(entries==3) {
		elbeemSimplifyChannelVec3( channel, &channelSize); 
	} else if(entries==1) {
		elbeemSimplifyChannelFloat( channel, &channelSize); 
	} else {
		// invalid, cant happen?
	}

	fprintf(file, "      CHANNEL %s = \n", str); 
	for(i=0; i<channelSize;i++) { 
		fprintf(file,"        ");  
		for(j=0;j<=entries;j++) {  // also print time value
			fprintf(file," %f ", channel[i*(entries+1)+j] ); 
			if(j==entries-1){ fprintf(file,"  "); }
		} 
		fprintf(file," \n");  
	} 

	fprintf(file,  "      ; \n" ); 
}
#endif

static void fluidsimInitChannel(float **setchannel, int size, float *time, 
		int *icuIds, float *defaults, Ipo* ipo, int entries) {
	int i,j;
	IpoCurve* icus[3];
	char *cstr = NULL;
	float *channel = NULL;
	float aniFrlen = G.scene->r.framelen;
	int current_frame = G.scene->r.cfra;
	if((entries<1) || (entries>3)) {
		printf("fluidsimInitChannel::Error - invalid no. of entries: %d\n",entries);
		entries = 1;
	}

	cstr = "fluidsiminit_channelfloat";
	if(entries>1) cstr = "fluidsiminit_channelvec";
	channel = MEM_callocN( size* (entries+1)* sizeof(float), cstr );
	
	if(ipo) {
		for(j=0; j<entries; j++) icus[j]  = find_ipocurve(ipo, icuIds[j] );
	} else {
		for(j=0; j<entries; j++) icus[j]  = NULL; 
	}
	
	for(j=0; j<entries; j++) {
		if(icus[j]) { 
			for(i=1; i<=size; i++) {
				/* Bugfix to make python drivers working
				// which uses Blender.get("curframe") 
				*/
				G.scene->r.cfra = floor(aniFrlen*((float)i));
				
				calc_icu(icus[j], aniFrlen*((float)i) );
				channel[(i-1)*(entries+1) + j] = icus[j]->curval;
			}
		}  else {
			for(i=1; i<=size; i++) { channel[(i-1)*(entries+1) + j] = defaults[j]; }
		}
		//printf("fluidsimInitChannel entry:%d , ",j); for(i=1; i<=size; i++) { printf(" val%d:%f ",i, channel[(i-1)*(entries+1) + j] ); } printf(" \n"); // DEBUG
	}
	// set time values
	for(i=1; i<=size; i++) {
		channel[(i-1)*(entries+1) + entries] = time[i];
	}
	G.scene->r.cfra = current_frame;
	*setchannel = channel;
}

static void fluidsimInitMeshChannel(float **setchannel, int size, Object *obm, int vertices, float *time) {
	float *channel = NULL;
	int mallsize = size* (3*vertices+1);
	int frame,i;
	int numVerts=0, numTris=0;
	int setsize = 3*vertices+1;

	channel = MEM_callocN( mallsize* sizeof(float), "fluidsim_meshchannel" );

	//fprintf(stderr,"\n\nfluidsimInitMeshChannel size%d verts%d mallsize%d \n\n\n",size,vertices,mallsize);
	for(frame=1; frame<=size; frame++) {
		float *verts=NULL;
		int *tris=NULL;
		G.scene->r.cfra = frame;
		scene_update_for_newframe(G.scene, G.scene->lay);

		initElbeemMesh(obm, &numVerts, &verts, &numTris, &tris, 1);
		//fprintf(stderr,"\nfluidsimInitMeshChannel frame%d verts%d/%d \n\n",frame,vertices,numVerts);
		for(i=0; i<3*vertices;i++) {
			channel[(frame-1)*setsize + i] = verts[i];
			//fprintf(stdout," frame%d vert%d=%f \n",frame,i,verts[i]);
			//if(i%3==2) fprintf(stdout,"\n");
		}
		channel[(frame-1)*setsize + setsize-1] = time[frame];

		MEM_freeN(verts);
		MEM_freeN(tris);
	}
	*setchannel = channel;
}


/* ******************************************************************************** */
/* ********************** simulation thread             ************************* */
/* ******************************************************************************** */

SDL_mutex	*globalBakeLock=NULL;
int			globalBakeState = 0; // 0 everything ok, -1 abort simulation, -2 sim error, 1 sim done
int			globalBakeFrame = 0;

// run simulation in seperate thread
static int fluidsimSimulateThread(void *unused) { // *ptr) {
	//char* fnameCfgPath = (char*)(ptr);
	int ret=0;
	
	ret = elbeemSimulate();
	SDL_mutexP(globalBakeLock);
	if(globalBakeState==0) {
		if(ret==0) {
			// if no error, set to normal exit
			globalBakeState = 1;
		} else {
			// simulation failed, display error
			globalBakeState = -2;
		}
	}
	SDL_mutexV(globalBakeLock);
	return ret;
}


int runSimulationCallback(void *data, int status, int frame) {
	//elbeemSimulationSettings *settings = (elbeemSimulationSettings*)data;
	//printf("elbeem blender cb s%d, f%d, domainid:%d \n", status,frame, settings->domainId ); // DEBUG
	
	if(!globalBakeLock) return FLUIDSIM_CBRET_ABORT;
	if(status==FLUIDSIM_CBSTATUS_NEWFRAME) {
		SDL_mutexP(globalBakeLock);
		globalBakeFrame = frame-1;
		SDL_mutexV(globalBakeLock);
	}
	
	//if((frameCounter==3) && (!frameStop)) { frameStop=1; return 1; }
		
	SDL_mutexP(globalBakeLock);
	if(globalBakeState!=0) {
		return FLUIDSIM_CBRET_ABORT;
	}
	SDL_mutexV(globalBakeLock);
	return FLUIDSIM_CBRET_CONTINUE;
}


/* ******************************************************************************** */
/* ********************** write fluidsim config to file ************************* */
/* ******************************************************************************** */

void fluidsimBake(struct Object *ob)
{
	FILE *fileCfg;
	int i;
	struct Object *fsDomain = NULL;
	FluidsimSettings *domainSettings;
	struct Object *obit = NULL; /* object iterator */
	Base *base;
	int origFrame = G.scene->r.cfra;
	char debugStrBuffer[256];
	int dirExist = 0;
	int gridlevels = 0;
	int simAborted = 0; // was the simulation aborted by user?
	int  doExportOnly = 0;
	char *exportEnvStr = "BLENDER_ELBEEMEXPORTONLY";
	const char *strEnvName = "BLENDER_ELBEEMDEBUG"; // from blendercall.cpp
	//char *channelNames[3] = { "translation","rotation","scale" };

	char *suffixConfig = "fluidsim.cfg";
	char *suffixSurface = "fluidsurface";
	char newSurfdataPath[FILE_MAXDIR+FILE_MAXFILE]; // modified output settings
	char targetDir[FILE_MAXDIR+FILE_MAXFILE];  // store & modify output settings
	char targetFile[FILE_MAXDIR+FILE_MAXFILE]; // temp. store filename from targetDir for access
	int  outStringsChanged = 0;             // modified? copy back before baking
	int  haveSomeFluid = 0;                 // check if any fluid objects are set

	// config vars, inited before either export or run...
	double calcViscosity = 0.0;
	int noFrames;
	double aniFrameTime;
	float aniFrlen;
	int   channelObjCount;
	float *bbStart = NULL;
	float *bbSize = NULL;
	float domainMat[4][4];
	float invDomMat[4][4];
	// channel data
	int   allchannelSize; // fixed by no. of frames
	int   startFrame = 1;  // dont use G.scene->r.sfra here, always start with frame 1
	// easy frame -> sim time calc
	float *timeAtFrame=NULL, *timeAtIndex=NULL;
	// domain
	float *channelDomainTime = NULL;
	float *channelDomainViscosity = NULL; 
	float *channelDomainGravity = NULL;
	// objects (currently max. 256 objs)
	float *channelObjMove[256][3]; // object movments , 0=trans, 1=rot, 2=scale
	float *channelObjInivel[256];    // initial velocities
	float *channelObjActive[256];    // obj active channel
	
	if(getenv(strEnvName)) {
		int dlevel = atoi(getenv(strEnvName));
		elbeemSetDebugLevel(dlevel);
		snprintf(debugStrBuffer,256,"fluidsimBake::msg: Debug messages activated due to envvar '%s'\n",strEnvName); 
		elbeemDebugOut(debugStrBuffer);
	}
	if(getenv(exportEnvStr)) {
		doExportOnly = atoi(getenv(exportEnvStr));
		snprintf(debugStrBuffer,256,"fluidsimBake::msg: Exporting mode set to '%d' due to envvar '%s'\n",doExportOnly, exportEnvStr); 
		elbeemDebugOut(debugStrBuffer);
	}

	// make sure it corresponds to startFrame setting
	// old: noFrames = G.scene->r.efra - G.scene->r.sfra +1;
	noFrames = G.scene->r.efra - 0;
	if(noFrames<=0) {
		pupmenu("Fluidsim Bake Error%t|No frames to export - check your animation range settings. Aborted%x0");
		return;
	}

	/* no object pointer, find in selected ones.. */
	if(!ob) {
		for(base=G.scene->base.first; base; base= base->next) {
			if ( ((base)->flag & SELECT) 
					// ignore layer setting for now? && ((base)->lay & G.vd->lay) 
				 ) {
				if((!ob)&&(base->object->fluidsimFlag & OB_FLUIDSIM_ENABLE)&&(base->object->type==OB_MESH)) {
					if(base->object->fluidsimSettings->type == OB_FLUIDSIM_DOMAIN) {
						ob = base->object;
					}
				}
			}
		}
		// no domains found?
		if(!ob) return;
	}

	channelObjCount = 0;
	for(base=G.scene->base.first; base; base= base->next) {
		obit = base->object;
		//{ snprintf(debugStrBuffer,256,"DEBUG object name=%s, type=%d ...\n", obit->id.name, obit->type); elbeemDebugOut(debugStrBuffer); } // DEBUG
		if( (obit->fluidsimFlag & OB_FLUIDSIM_ENABLE) && 
				(obit->type==OB_MESH) &&
				(obit->fluidsimSettings->type != OB_FLUIDSIM_DOMAIN) &&  // if has to match 3 places! // CHECKMATCH
				(obit->fluidsimSettings->type != OB_FLUIDSIM_PARTICLE) ) {
			channelObjCount++;
		}
	}
	
	if (channelObjCount>=255) {
		pupmenu("Fluidsim Bake Error%t|Cannot bake with more then 256 objects");
		return;
	}

	/* check if there's another domain... */
	for(base=G.scene->base.first; base; base= base->next) {
		obit = base->object;
		if((obit->fluidsimFlag & OB_FLUIDSIM_ENABLE)&&(obit->type==OB_MESH)) {
			if(obit->fluidsimSettings->type == OB_FLUIDSIM_DOMAIN) {
				if(obit != ob) {
					//snprintf(debugStrBuffer,256,"fluidsimBake::warning - More than one domain!\n"); elbeemDebugOut(debugStrBuffer);
					pupmenu("Fluidsim Bake Error%t|There should be only one domain object! Aborted%x0");
					return;
				}
			}
		}
	}
	/* these both have to be valid, otherwise we wouldnt be here */
	/* dont use ob here after...*/
	fsDomain = ob;
	domainSettings = ob->fluidsimSettings;
	ob = NULL;
	/* rough check of settings... */
	if(domainSettings->previewresxyz > domainSettings->resolutionxyz) {
		snprintf(debugStrBuffer,256,"fluidsimBake::warning - Preview (%d) >= Resolution (%d)... setting equal.\n", domainSettings->previewresxyz ,  domainSettings->resolutionxyz); 
		elbeemDebugOut(debugStrBuffer);
		domainSettings->previewresxyz = domainSettings->resolutionxyz;
	}
	// set adaptive coarsening according to resolutionxyz
	// this should do as an approximation, with in/outflow
	// doing this more accurate would be overkill
	// perhaps add manual setting?
	if(domainSettings->maxRefine <0) {
		if(domainSettings->resolutionxyz>128) {
			gridlevels = 2;
		} else
		if(domainSettings->resolutionxyz>64) {
			gridlevels = 1;
		} else {
			gridlevels = 0;
		}
	} else {
		gridlevels = domainSettings->maxRefine;
	}
	snprintf(debugStrBuffer,256,"fluidsimBake::msg: Baking %s, refine: %d\n", fsDomain->id.name , gridlevels ); 
	elbeemDebugOut(debugStrBuffer);
	
	// check if theres any fluid
	// abort baking if not...
	for(base=G.scene->base.first; base; base= base->next) {
		obit = base->object;
		if( (obit->fluidsimFlag & OB_FLUIDSIM_ENABLE) && 
				(obit->type==OB_MESH) && (
			  (obit->fluidsimSettings->type == OB_FLUIDSIM_FLUID) ||
			  (obit->fluidsimSettings->type == OB_FLUIDSIM_INFLOW) )
				) {
			haveSomeFluid = 1;
		}
	}
	if(!haveSomeFluid) {
		pupmenu("Fluidsim Bake Error%t|No fluid objects in scene... Aborted%x0");
		return;
	}

	// prepare names...
	strncpy(targetDir, domainSettings->surfdataPath, FILE_MAXDIR);
	strncpy(newSurfdataPath, domainSettings->surfdataPath, FILE_MAXDIR);
	BLI_convertstringcode(targetDir, G.sce); // fixed #frame-no 

	strcpy(targetFile, targetDir);
	strcat(targetFile, suffixConfig);
	if(!doExportOnly) { strcat(targetFile,".tmp"); }  // dont overwrite/delete original file
	// make sure all directories exist
	// as the bobjs use the same dir, this only needs to be checked
	// for the cfg output
	BLI_make_existing_file(targetFile);

	// check selected directory
	// simply try to open cfg file for writing to test validity of settings
	fileCfg = fopen(targetFile, "w");
	if(fileCfg) { 
		dirExist = 1; fclose(fileCfg); 
		// remove cfg dummy from  directory test
		if(!doExportOnly) { BLI_delete(targetFile, 0,0); }
	}

	if((strlen(targetDir)<1) || (!dirExist)) {
		char blendDir[FILE_MAXDIR+FILE_MAXFILE], blendFile[FILE_MAXDIR+FILE_MAXFILE];
		// invalid dir, reset to current/previous
		strcpy(blendDir, G.sce);
		BLI_splitdirstring(blendDir, blendFile);
		if(strlen(blendFile)>6){
			int len = strlen(blendFile);
			if( (blendFile[len-6]=='.')&& (blendFile[len-5]=='b')&& (blendFile[len-4]=='l')&&
					(blendFile[len-3]=='e')&& (blendFile[len-2]=='n')&& (blendFile[len-1]=='d') ){
				blendFile[len-6] = '\0';
			}
		}
		// todo... strip .blend ?
		snprintf(newSurfdataPath,FILE_MAXFILE+FILE_MAXDIR,"//fluidsimdata/%s_%s_", blendFile, fsDomain->id.name);

		snprintf(debugStrBuffer,256,"fluidsimBake::error - warning resetting output dir to '%s'\n", newSurfdataPath);
		elbeemDebugOut(debugStrBuffer);
		outStringsChanged=1;
	}

	// check if modified output dir is ok
	if(outStringsChanged) {
		char dispmsg[FILE_MAXDIR+FILE_MAXFILE+256];
		int  selection=0;
		strcpy(dispmsg,"Output settings set to: '");
		strcat(dispmsg, newSurfdataPath);
		strcat(dispmsg, "'%t|Continue with changed settings%x1|Discard and abort%x0");

		// ask user if thats what he/she wants...
		selection = pupmenu(dispmsg);
		if(selection<1) return; // 0 from menu, or -1 aborted
		strcpy(targetDir, newSurfdataPath);
		strncpy(domainSettings->surfdataPath, newSurfdataPath, FILE_MAXDIR);
		BLI_convertstringcode(targetDir, G.sce); // fixed #frame-no 
	}
	
	// --------------------------------------------------------------------------------------------
	// dump data for start frame 
	// CHECK more reasonable to number frames according to blender?
	// dump data for frame 0
  G.scene->r.cfra = startFrame;
  scene_update_for_newframe(G.scene, G.scene->lay);
	
	// init common export vars for both file export and run
	for(i=0; i<256; i++) {
		channelObjMove[i][0] = channelObjMove[i][1] = channelObjMove[i][2] = NULL;
		channelObjInivel[i] = NULL;
		channelObjActive[i] = NULL;
	}
	allchannelSize = G.scene->r.efra; // always use till last frame
	aniFrameTime = (domainSettings->animEnd - domainSettings->animStart)/(double)noFrames;
	// blender specific - scale according to map old/new settings in anim panel:
	aniFrlen = G.scene->r.framelen;
	if(domainSettings->viscosityMode==1) {
		/* manual mode, visc=value/(10^-vexp) */
		calcViscosity = (1.0/pow(10.0,domainSettings->viscosityExponent)) * domainSettings->viscosityValue;
	} else {
		calcViscosity = fluidsimViscosityPreset[ domainSettings->viscosityMode ];
	}

	bbStart = fsDomain->fluidsimSettings->bbStart; 
	bbSize = fsDomain->fluidsimSettings->bbSize;
	fluidsimGetAxisAlignedBB(fsDomain->data, fsDomain->obmat, bbStart, bbSize, &domainSettings->meshBB);

	// always init
	{ int timeIcu[1] = { FLUIDSIM_TIME };
		float timeDef[1] = { 1. };
		int gravIcu[3] = { FLUIDSIM_GRAV_X, FLUIDSIM_GRAV_Y, FLUIDSIM_GRAV_Z };
		float gravDef[3];
		int viscIcu[1] = { FLUIDSIM_VISC };
		float viscDef[1] = { 1. };

		gravDef[0] = domainSettings->gravx;
		gravDef[1] = domainSettings->gravy;
		gravDef[2] = domainSettings->gravz;

		// time channel is a bit special, init by hand...
		timeAtIndex = MEM_callocN( (allchannelSize+1)*1*sizeof(float), "fluidsiminit_timeatindex");
		for(i=0; i<=G.scene->r.efra; i++) {
			timeAtIndex[i] = (float)(i-startFrame);
		}
		fluidsimInitChannel( &channelDomainTime, allchannelSize, timeAtIndex, timeIcu,timeDef, domainSettings->ipo, CHANNEL_FLOAT ); // NDEB
		// time channel is a multiplicator for aniFrameTime
		if(channelDomainTime) {
			for(i=0; i<allchannelSize; i++) { 
				channelDomainTime[i*2+0] = aniFrameTime * channelDomainTime[i*2+0]; 
				if(channelDomainTime[i*2+0]<0.) channelDomainTime[i*2+0] = 0.;
			}
		}
		timeAtFrame = MEM_callocN( (allchannelSize+1)*1*sizeof(float), "fluidsiminit_timeatframe");
		timeAtFrame[0] = timeAtFrame[1] = domainSettings->animStart; // start at index 1
		if(channelDomainTime) {
			for(i=2; i<=allchannelSize; i++) {
				timeAtFrame[i] = timeAtFrame[i-1]+channelDomainTime[(i-1)*2+0];
			}
		} else {
			for(i=2; i<=allchannelSize; i++) { timeAtFrame[i] = timeAtFrame[i-1]+aniFrameTime; }
		}

		fluidsimInitChannel( &channelDomainViscosity, allchannelSize, timeAtFrame, viscIcu,viscDef, domainSettings->ipo, CHANNEL_FLOAT ); // NDEB
		if(channelDomainViscosity) {
			for(i=0; i<allchannelSize; i++) { channelDomainViscosity[i*2+0] = calcViscosity * channelDomainViscosity[i*2+0]; }
		}
		fluidsimInitChannel( &channelDomainGravity, allchannelSize, timeAtFrame, gravIcu,gravDef, domainSettings->ipo, CHANNEL_VEC );
	} // domain channel init
	
	// init obj movement channels
	channelObjCount=0;
	for(base=G.scene->base.first; base; base= base->next) {
		obit = base->object;
		//{ snprintf(debugStrBuffer,256,"DEBUG object name=%s, type=%d ...\n", obit->id.name, obit->type); elbeemDebugOut(debugStrBuffer); } // DEBUG
		if( (obit->fluidsimFlag & OB_FLUIDSIM_ENABLE) && 
				(obit->type==OB_MESH) &&
				(obit->fluidsimSettings->type != OB_FLUIDSIM_DOMAIN) &&  // if has to match 3 places! // CHECKMATCH
				(obit->fluidsimSettings->type != OB_FLUIDSIM_PARTICLE) ) {

			//  cant use fluidsimInitChannel for obj channels right now, due
			//  to the special DXXX channels, and the rotation specialities
			IpoCurve *icuex[3][3];
			//IpoCurve *par_icuex[3][3];
			int icuIds[3][3] = { 
				{OB_LOC_X,  OB_LOC_Y,  OB_LOC_Z},
				{OB_ROT_X,  OB_ROT_Y,  OB_ROT_Z},
				{OB_SIZE_X, OB_SIZE_Y, OB_SIZE_Z} 
			};
			// relative ipos
			IpoCurve *icudex[3][3];
			//IpoCurve *par_icudex[3][3];
			int icudIds[3][3] = { 
				{OB_DLOC_X,  OB_DLOC_Y,  OB_DLOC_Z},
				{OB_DROT_X,  OB_DROT_Y,  OB_DROT_Z},
				{OB_DSIZE_X, OB_DSIZE_Y, OB_DSIZE_Z} 
			};
			int j,k;
			float vals[3] = {0.0,0.0,0.0}; 
			int o = channelObjCount;
			int   inivelIcu[3] =  { FLUIDSIM_VEL_X, FLUIDSIM_VEL_Y, FLUIDSIM_VEL_Z };
			float inivelDefs[3];
			int   activeIcu[1] =  { FLUIDSIM_ACTIVE };
			float activeDefs[1] = { 1 }; // default to on

			inivelDefs[0] = obit->fluidsimSettings->iniVelx;
			inivelDefs[1] = obit->fluidsimSettings->iniVely;
			inivelDefs[2] = obit->fluidsimSettings->iniVelz;

			// check & init loc,rot,size
			for(j=0; j<3; j++) {
				for(k=0; k<3; k++) {
					icuex[j][k]  = find_ipocurve(obit->ipo, icuIds[j][k] );
					icudex[j][k] = find_ipocurve(obit->ipo, icudIds[j][k] );
					//if(obit->parent) {
						//par_icuex[j][k]  = find_ipocurve(obit->parent->ipo, icuIds[j][k] );
						//par_icudex[j][k] = find_ipocurve(obit->parent->ipo, icudIds[j][k] );
					//}
				}
			}

			for(j=0; j<3; j++) {
				channelObjMove[o][j] = MEM_callocN( allchannelSize*4*sizeof(float), "fluidsiminit_objmovchannel");
				for(i=1; i<=allchannelSize; i++) {

					for(k=0; k<3; k++) {
						if(icuex[j][k]) { 
							// IPO exists, use it ...
							calc_icu(icuex[j][k], aniFrlen*((float)i) );
							vals[k] = icuex[j][k]->curval; 
							if(obit->parent) {
								// add parent transform, multiply scaling, add trafo&rot
								//calc_icu(par_icuex[j][k], aniFrlen*((float)i) );
								//if(j==2) { vals[k] *= par_icuex[j][k]->curval; }
								//else { vals[k] += par_icuex[j][k]->curval; }
							}
						} else {
							// use defaults from static values
							float setval=0.0;
							if(j==0) { 
								setval = obit->loc[k];
								if(obit->parent){ setval += obit->parent->loc[k]; }
							} else if(j==1) { 
								setval = ( 180.0*obit->rot[k] )/( 10.0*M_PI );
								if(obit->parent){ setval = ( 180.0*(obit->rot[k]+obit->parent->rot[k]) )/( 10.0*M_PI ); }
							} else { 
								setval = obit->size[k]; 
								if(obit->parent){ setval *= obit->parent->size[k]; }
							}
							vals[k] = setval;
						}
						if(icudex[j][k]) { 
							calc_icu(icudex[j][k], aniFrlen*((float)i) );
							//vals[k] += icudex[j][k]->curval; 
							// add transform, multiply scaling, add trafo&rot
							if(j==2) { vals[k] *= icudex[j][k]->curval; }
							else { vals[k] += icudex[j][k]->curval; }
							if(obit->parent) {
								// add parent transform, multiply scaling, add trafo&rot
								//calc_icu(par_icuex[j][k], aniFrlen*((float)i) );
								//if(j==2) { vals[k] *= par_icudex[j][k]->curval; }
								//else { vals[k] += par_icudex[j][k]->curval; }
							}
						} 
					} // k

					for(k=0; k<3; k++) {
						float set = vals[k];
						if(j==1) { // rot is downscaled by 10 for ipo !?
							set = 360.0 - (10.0*set);
						}
						channelObjMove[o][j][(i-1)*4 + k] = set;
					} // k
					channelObjMove[o][j][(i-1)*4 + 3] = timeAtFrame[i];
				}
			}

			fluidsimInitChannel( &channelObjInivel[o], allchannelSize, timeAtFrame, inivelIcu,inivelDefs, obit->fluidsimSettings->ipo, CHANNEL_VEC );
			fluidsimInitChannel( &channelObjActive[o], allchannelSize, timeAtFrame, activeIcu,activeDefs, obit->fluidsimSettings->ipo, CHANNEL_FLOAT );

			channelObjCount++;

		}
	}

	// init trafo matrix
	MTC_Mat4CpyMat4(domainMat, fsDomain->obmat);
	if(!Mat4Invert(invDomMat, domainMat)) {
		snprintf(debugStrBuffer,256,"fluidsimBake::error - Invalid obj matrix?\n"); 
		elbeemDebugOut(debugStrBuffer);
		// FIXME add fatal msg
		FS_FREE_CHANNELS;
		return;
	}


	// --------------------------------------------------------------------------------------------
	// start writing / exporting
	strcpy(targetFile, targetDir);
	strcat(targetFile, suffixConfig);
	if(!doExportOnly) { strcat(targetFile,".tmp"); }  // dont overwrite/delete original file
	// make sure these directories exist as well
	if(outStringsChanged) {
		BLI_make_existing_file(targetFile);
	}

	if(!doExportOnly) {
		SDL_Thread *simthr = NULL;

		// perform simulation with El'Beem api and SDL threads
		elbeemSimulationSettings fsset;
		elbeemResetSettings(&fsset);
		fsset.version = 1;

		// setup global settings
		for(i=0 ; i<3; i++) fsset.geoStart[i] = bbStart[i];
		for(i=0 ; i<3; i++) fsset.geoSize[i] = bbSize[i];
		// simulate with 50^3
		fsset.resolutionxyz = (int)domainSettings->resolutionxyz;
		fsset.previewresxyz = (int)domainSettings->previewresxyz;
		// 10cm water domain
		fsset.realsize = domainSettings->realsize;
		fsset.viscosity = calcViscosity;
		// earth gravity
		fsset.gravity[0] = domainSettings->gravx;
		fsset.gravity[1] = domainSettings->gravy;
		fsset.gravity[2] = domainSettings->gravz;
		// simulate 5 frames, each 0.03 seconds, output to ./apitest_XXX.bobj.gz
		fsset.animStart = domainSettings->animStart;
		fsset.aniFrameTime = aniFrameTime;
		fsset.noOfFrames = noFrames - 1; // is otherwise subtracted in parser
		strcpy(targetFile, targetDir);
		strcat(targetFile, suffixSurface);
		// defaults for compressibility and adaptive grids
		fsset.gstar = domainSettings->gstar;
		fsset.maxRefine = domainSettings->maxRefine; // check <-> gridlevels
		fsset.generateParticles = domainSettings->generateParticles; 
		fsset.numTracerParticles = domainSettings->generateTracers; 
		fsset.surfaceSmoothing = domainSettings->surfaceSmoothing; 
		fsset.surfaceSubdivs = domainSettings->surfaceSubdivs; 
		fsset.farFieldSize = domainSettings->farFieldSize; 
		strcpy( fsset.outputPath, targetFile);

		// domain channels
		fsset.channelSizeFrameTime = 
		fsset.channelSizeViscosity = 
		fsset.channelSizeGravity =  allchannelSize;
		fsset.channelFrameTime = channelDomainTime;
		fsset.channelViscosity = channelDomainViscosity;
		fsset.channelGravity = channelDomainGravity;

		fsset.runsimCallback = &runSimulationCallback;
		fsset.runsimUserData = &fsset;

		if(     (domainSettings->typeFlags&OB_FSBND_NOSLIP))   fsset.domainobsType = FLUIDSIM_OBSTACLE_NOSLIP;
		else if((domainSettings->typeFlags&OB_FSBND_PARTSLIP)) fsset.domainobsType = FLUIDSIM_OBSTACLE_PARTSLIP;
		else if((domainSettings->typeFlags&OB_FSBND_FREESLIP)) fsset.domainobsType = FLUIDSIM_OBSTACLE_FREESLIP;
		fsset.domainobsPartslip = domainSettings->partSlipValue;
		fsset.generateVertexVectors = (domainSettings->domainNovecgen==0);

		// init blender trafo matrix
 		// fprintf(stderr,"elbeemInit - mpTrafo:\n");
		{ int j; 
		for(i=0; i<4; i++) {
			for(j=0; j<4; j++) {
				fsset.surfaceTrafo[i*4+j] = invDomMat[j][i];
 				// fprintf(stderr,"elbeemInit - mpTrafo %d %d = %f (%d) \n", i,j, fsset.surfaceTrafo[i*4+j] , (i*4+j) );
			}
		} }

	  // init solver with settings
		elbeemInit();
		elbeemAddDomain(&fsset);
		
		// init objects
		channelObjCount = 0;
		for(base=G.scene->base.first; base; base= base->next) {
			obit = base->object;
			//{ snprintf(debugStrBuffer,256,"DEBUG object name=%s, type=%d ...\n", obit->id.name, obit->type); elbeemDebugOut(debugStrBuffer); } // DEBUG
			if( (obit->fluidsimFlag & OB_FLUIDSIM_ENABLE) &&  // if has to match 3 places! // CHECKMATCH
					(obit->type==OB_MESH) &&
					(obit->fluidsimSettings->type != OB_FLUIDSIM_DOMAIN) &&
					(obit->fluidsimSettings->type != OB_FLUIDSIM_PARTICLE)
				) {
				float *verts=NULL;
				int *tris=NULL;
				int numVerts=0, numTris=0;
				int o = channelObjCount;
				int	deform = (obit->fluidsimSettings->domainNovecgen); // misused value
				// todo - use blenderInitElbeemMesh
				elbeemMesh fsmesh;
				elbeemResetMesh( &fsmesh );
				fsmesh.type = obit->fluidsimSettings->type;;
				// get name of object for debugging solver
				fsmesh.name = obit->id.name; 

				initElbeemMesh(obit, &numVerts, &verts, &numTris, &tris, 0);
				fsmesh.numVertices   = numVerts;
				fsmesh.numTriangles  = numTris;
				fsmesh.vertices      = verts;
				fsmesh.triangles     = tris;

				fsmesh.channelSizeTranslation  = 
				fsmesh.channelSizeRotation     = 
				fsmesh.channelSizeScale        = 
				fsmesh.channelSizeInitialVel   = 
				fsmesh.channelSizeActive       = allchannelSize;

				fsmesh.channelTranslation      = channelObjMove[o][0];
				fsmesh.channelRotation         = channelObjMove[o][1];
				fsmesh.channelScale            = channelObjMove[o][2];
				fsmesh.channelActive           = channelObjActive[o];
				if( (fsmesh.type == OB_FLUIDSIM_FLUID) ||
						(fsmesh.type == OB_FLUIDSIM_INFLOW) ) {
					fsmesh.channelInitialVel       = channelObjInivel[o];
				  fsmesh.localInivelCoords = ((obit->fluidsimSettings->typeFlags&OB_FSINFLOW_LOCALCOORD)?1:0);
				} 

				if(     (obit->fluidsimSettings->typeFlags&OB_FSBND_NOSLIP))   fsmesh.obstacleType = FLUIDSIM_OBSTACLE_NOSLIP;
				else if((obit->fluidsimSettings->typeFlags&OB_FSBND_PARTSLIP)) fsmesh.obstacleType = FLUIDSIM_OBSTACLE_PARTSLIP;
				else if((obit->fluidsimSettings->typeFlags&OB_FSBND_FREESLIP)) fsmesh.obstacleType = FLUIDSIM_OBSTACLE_FREESLIP;
				fsmesh.obstaclePartslip = obit->fluidsimSettings->partSlipValue;
				fsmesh.volumeInitType = obit->fluidsimSettings->volumeInitType;
				fsmesh.obstacleImpactFactor = obit->fluidsimSettings->surfaceSmoothing; // misused value

				// animated meshes
				if(deform) {
					fsmesh.channelSizeVertices = allchannelSize;
					fluidsimInitMeshChannel( &fsmesh.channelVertices, allchannelSize, obit, numVerts, timeAtFrame);
					G.scene->r.cfra = startFrame;
					scene_update_for_newframe(G.scene, G.scene->lay);
					// remove channels
					fsmesh.channelTranslation      = 
					fsmesh.channelRotation         = 
					fsmesh.channelScale            = NULL; 
				} 

				elbeemAddMesh(&fsmesh);

				if(verts) MEM_freeN(verts);
				if(tris) MEM_freeN(tris);
				if(fsmesh.channelVertices) MEM_freeN(fsmesh.channelVertices);
				channelObjCount++;
			} // valid mesh
		} // objects
		//domainSettings->type = OB_FLUIDSIM_DOMAIN; // enable for bake display again
		//fsDomain->fluidsimFlag = OB_FLUIDSIM_ENABLE; // disable during bake
		
		globalBakeLock = SDL_CreateMutex();
		// set to neutral, -1 means user abort, -2 means init error
		globalBakeState = 0;
		globalBakeFrame = 0;
		simthr = SDL_CreateThread(fluidsimSimulateThread, targetFile);

		if(!simthr) {
			snprintf(debugStrBuffer,256,"fluidsimBake::error: Unable to create thread... running without one.\n"); 
			elbeemDebugOut(debugStrBuffer);
			set_timecursor(0);
			elbeemSimulate();
		} else {
			int done = 0;
			unsigned short event=0;
			short val;
			float noFramesf = (float)noFrames;
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
				if(globalBakeState != 0) done = 1; // 1=ok, <0=error/abort
				SDL_mutexV(globalBakeLock);

				while(qtest()) {
					event = extern_qread(&val);
					if(event == ESCKEY) {
						// abort...
						SDL_mutexP(globalBakeLock);
						done = -1;
						globalBakeFrame = 0;
						globalBakeState = -1;
						simAborted = 1;
						SDL_mutexV(globalBakeLock);
						break;
					}
				} 

				// redraw the 3D for showing progress once in a while...
				if(lastRedraw!=globalBakeFrame) {
					ScrArea *sa;
					G.scene->r.cfra = startFrame+globalBakeFrame;
					lastRedraw = globalBakeFrame;
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
	} // El'Beem API init, thread creation 
	// --------------------------------------------------------------------------------------------
	else
	{ // write config file to be run with command line simulator
		pupmenu("Fluidsim Bake Message%t|Config file export not supported.%x0");
	} // config file export done!

	// --------------------------------------------------------------------------------------------
	FS_FREE_CHANNELS;

	// go back to "current" blender time
	waitcursor(0);
  G.scene->r.cfra = origFrame;
  scene_update_for_newframe(G.scene, G.scene->lay);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);

	if(!simAborted) {
		char fsmessage[512];
		char elbeemerr[256];
		strcpy(fsmessage,"Fluidsim Bake Error: ");
		// check if some error occurred
		if(globalBakeState==-2) {
			strcat(fsmessage,"Failed to initialize [Msg: ");

			elbeemGetErrorString(elbeemerr);
			strcat(fsmessage,elbeemerr);

			strcat(fsmessage,"] |OK%x0");
			pupmenu(fsmessage);
		} // init error
	}
}

void fluidsimFreeBake(struct Object *ob)
{
	/* not implemented yet */
}


#else /* DISABLE_ELBEEM */

/* compile dummy functions for disabled fluid sim */

FluidsimSettings *fluidsimSettingsNew(struct Object *srcob) {
	return NULL;
}

void fluidsimSettingsFree(FluidsimSettings *fss) {
}

FluidsimSettings* fluidsimSettingsCopy(FluidsimSettings *fss) {
	return NULL;
}

/* only compile dummy functions */
void fluidsimBake(struct Object *ob) {
}

void fluidsimFreeBake(struct Object *ob) {
}

#endif /* DISABLE_ELBEEM */

