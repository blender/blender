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
#include <sys/stat.h>

#ifdef WIN32	/* Windos */
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

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
#include "BLI_threads.h"
#include "BLI_arithb.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_softbody.h"

#include "PIL_time.h"

#include "LBM_fluidsim.h"

#include "BIF_gl.h"

#include "ED_fluidsim.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "physics_intern.h" // own include

/* enable/disable overall compilation */
#ifndef DISABLE_ELBEEM

/* XXX */
/* from header info.c */
static int start_progress_bar(void) {return 0;};
static void end_progress_bar(void) {};
static void waitcursor(int val) {};
static int progress_bar(float done, char *busy_info) {return 0;}
static int pupmenu() {return 0;}
/* XXX */


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

/* ********************** fluid sim settings struct functions ********************** */

/* helper function */
void fluidsimGetGeometryObjFilename(Object *ob, char *dst) { //, char *srcname) {
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
		FS_FREE_ONECHANNEL(channelAttractforceStrength[i],"channelAttractforceStrength"); \
		FS_FREE_ONECHANNEL(channelAttractforceRadius[i],"channelAttractforceRadius"); \
		FS_FREE_ONECHANNEL(channelVelocityforceStrength[i],"channelVelocityforceStrength"); \
		FS_FREE_ONECHANNEL(channelVelocityforceRadius[i],"channelVelocityforceRadius"); \
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

static void fluidsimInitChannel(Scene *scene, float **setchannel, int size, float *time, 
		int *icuIds, float *defaults, Ipo* ipo, int entries) 
{

	int i, j;
	char *cstr = NULL;
	float *channel = NULL;
	
	cstr = "fluidsiminit_channelfloat";
	if(entries>1) cstr = "fluidsiminit_channelvec";
	channel = MEM_callocN( size* (entries+1)* sizeof(float), cstr );
	
	/* defaults  for now */
	for(j=0; j<entries; j++) {
		for(i=1; i<=size; i++) {
			channel[(i-1)*(entries+1) + j] = defaults[j];
		}	
	}
	
	for(i=1; i<=size; i++) {
		channel[(i-1)*(entries+1) + entries] = time[i];
	}

	*setchannel = channel;

#if 0
	/* goes away completely */
	int i,j;
	IpoCurve* icus[3];
	char *cstr = NULL;
	float *channel = NULL;
	float aniFrlen = scene->r.framelen;
	int current_frame = scene->r.cfra;
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
				scene->r.cfra = floor(aniFrlen*((float)i));
				
				// XXX calc_icu(icus[j], aniFrlen*((float)i) );
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
	scene->r.cfra = current_frame;
	*setchannel = channel;
#endif
}

static void fluidsimInitMeshChannel(bContext *C, float **setchannel, int size, Object *obm, int vertices, 
									float *time, int modifierIndex) 
{
	Scene *scene= CTX_data_scene(C);
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
		scene->r.cfra = frame;
		ED_update_for_newframe(C, 1);

		initElbeemMesh(scene, obm, &numVerts, &verts, &numTris, &tris, 1, modifierIndex);
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

static volatile int	globalBakeState = 0; // 0 everything ok, -1 abort simulation, -2 sim error, 1 sim done
static volatile int	globalBakeFrame = 0;
static volatile int g_break= 0;

// run simulation in seperate thread
static void *fluidsimSimulateThread(void *unused) { // *ptr) {
	//char* fnameCfgPath = (char*)(ptr);
	int ret=0;
	
	ret = elbeemSimulate();
	BLI_lock_thread(LOCK_CUSTOM1);
	if(globalBakeState==0) {
		if(ret==0) {
			// if no error, set to normal exit
			globalBakeState = 1;
		} else {
			// simulation failed, display error
			globalBakeState = -2;
		}
	}
	BLI_unlock_thread(LOCK_CUSTOM1);
	return NULL;
}


int runSimulationCallback(void *data, int status, int frame) {
	//elbeemSimulationSettings *settings = (elbeemSimulationSettings*)data;
	//printf("elbeem blender cb s%d, f%d, domainid:%d \n", status,frame, settings->domainId ); // DEBUG
	int state = 0;
	if(status==FLUIDSIM_CBSTATUS_NEWFRAME) {
		BLI_lock_thread(LOCK_CUSTOM1);
		globalBakeFrame = frame-1;
		BLI_unlock_thread(LOCK_CUSTOM1);
	}
	
	//if((frameCounter==3) && (!frameStop)) { frameStop=1; return 1; }
		
	BLI_lock_thread(LOCK_CUSTOM1);
	state = globalBakeState;
	BLI_unlock_thread(LOCK_CUSTOM1);
	
	if(state!=0) {
		return FLUIDSIM_CBRET_ABORT;
	}
	
	return FLUIDSIM_CBRET_CONTINUE;
}


/* ******************************************************************************** */
/* ********************** write fluidsim config to file ************************* */
/* ******************************************************************************** */

int fluidsimBake(bContext *C, ReportList *reports, Object *ob)
{
	Scene *scene= CTX_data_scene(C);
	FILE *fileCfg;
	int i;
	Object *fsDomain = NULL;
	FluidsimSettings *domainSettings;
	Object *obit = NULL; /* object iterator */
	Base *base;
	int origFrame = scene->r.cfra;
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
	int   startFrame = 1;  // dont use scene->r.sfra here, always start with frame 1
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
	
	/* fluid control channels */
	float *channelAttractforceStrength[256];
	float *channelAttractforceRadius[256];
	float *channelVelocityforceStrength[256];
	float *channelVelocityforceRadius[256];
	FluidsimModifierData *fluidmd = NULL;
	Mesh *mesh = NULL;
	
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
	// old: noFrames = scene->r.efra - scene->r.sfra +1;
	noFrames = scene->r.efra - 0;
	if(noFrames<=0) {
		BKE_report(reports, RPT_ERROR, "No frames to export - check your animation range settings.");
		return 0;
	}

	/* no object pointer, find in selected ones.. */
	if(!ob) {
		for(base=scene->base.first; base; base= base->next) {
			if ((base)->flag & SELECT) 
			{
				FluidsimModifierData *fluidmdtmp = (FluidsimModifierData *)modifiers_findByType(base->object, eModifierType_Fluidsim);
				
				if(fluidmdtmp && (base->object->type==OB_MESH)) 
				{
					if(fluidmdtmp->fss->type == OB_FLUIDSIM_DOMAIN) 
					{
						ob = base->object;
						break;
					}
				}
			}
		}
		// no domains found?
		if(!ob) return 0;
	}
	
	channelObjCount = 0;
	for(base=scene->base.first; base; base= base->next) 
	{
		FluidsimModifierData *fluidmdtmp = (FluidsimModifierData *)modifiers_findByType(base->object, eModifierType_Fluidsim);
		obit = base->object;
		if( fluidmdtmp && 
			(obit->type==OB_MESH) &&
			(fluidmdtmp->fss->type != OB_FLUIDSIM_DOMAIN) &&  // if has to match 3 places! // CHECKMATCH
			(fluidmdtmp->fss->type != OB_FLUIDSIM_PARTICLE) ) 
		{
			channelObjCount++;
		}
	}
	
	if (channelObjCount>=255) {
		BKE_report(reports, RPT_ERROR, "Cannot bake with more then 256 objects.");
		return 0;
	}

	/* check if there's another domain... */
	for(base=scene->base.first; base; base= base->next) 
	{
		FluidsimModifierData *fluidmdtmp = (FluidsimModifierData *)modifiers_findByType(base->object, eModifierType_Fluidsim);
		obit = base->object;
		if( fluidmdtmp &&(obit->type==OB_MESH)) 
		{
			if(fluidmdtmp->fss->type == OB_FLUIDSIM_DOMAIN) 
			{
				if(obit != ob) 
				{
					BKE_report(reports, RPT_ERROR, "There should be only one domain object.");
					return 0;
				}
			}
		}
	}
	
	// check if theres any fluid
	// abort baking if not...
	for(base=scene->base.first; base; base= base->next) 
	{
		FluidsimModifierData *fluidmdtmp = (FluidsimModifierData *)modifiers_findByType(base->object, eModifierType_Fluidsim);
		obit = base->object;
		if( fluidmdtmp && 
			(obit->type==OB_MESH) && 
			((fluidmdtmp->fss->type == OB_FLUIDSIM_FLUID) ||
			(fluidmdtmp->fss->type == OB_FLUIDSIM_INFLOW) ))
		{
			haveSomeFluid = 1;
			break;
		}
	}
	if(!haveSomeFluid) {
		BKE_report(reports, RPT_ERROR, "No fluid objects in scene.");
		return 0;
	}
	
	/* these both have to be valid, otherwise we wouldnt be here */
	/* dont use ob here after...*/
	fsDomain = ob;
	fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
	domainSettings = fluidmd->fss;
	ob = NULL;
	mesh = fsDomain->data;
	
	// calculate bounding box
	fluid_get_bb(mesh->mvert, mesh->totvert, fsDomain->obmat, domainSettings->bbStart, domainSettings->bbSize);
	
	// reset last valid frame
	domainSettings->lastgoodframe = -1;
	
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
		if(selection<1) return 0; // 0 from menu, or -1 aborted
		strcpy(targetDir, newSurfdataPath);
		strncpy(domainSettings->surfdataPath, newSurfdataPath, FILE_MAXDIR);
		BLI_convertstringcode(targetDir, G.sce); // fixed #frame-no 
	}
	
	// --------------------------------------------------------------------------------------------
	// dump data for start frame 
	// CHECK more reasonable to number frames according to blender?
	// dump data for frame 0
	scene->r.cfra = startFrame;
	ED_update_for_newframe(C, 1);
	
	// init common export vars for both file export and run
	for(i=0; i<256; i++) {
		channelObjMove[i][0] = channelObjMove[i][1] = channelObjMove[i][2] = NULL;
		channelObjInivel[i] = NULL;
		channelObjActive[i] = NULL;
		channelAttractforceStrength[i] = NULL;
		channelAttractforceRadius[i] = NULL;
		channelVelocityforceStrength[i] = NULL;
		channelVelocityforceRadius[i] = NULL;
	}
	allchannelSize = scene->r.efra; // always use till last frame
	aniFrameTime = (domainSettings->animEnd - domainSettings->animStart)/(double)noFrames;
	// blender specific - scale according to map old/new settings in anim panel:
	aniFrlen = scene->r.framelen;
	if(domainSettings->viscosityMode==1) {
		/* manual mode, visc=value/(10^-vexp) */
		calcViscosity = (1.0/pow(10.0,domainSettings->viscosityExponent)) * domainSettings->viscosityValue;
	} else {
		calcViscosity = fluidsimViscosityPreset[ domainSettings->viscosityMode ];
	}

	bbStart = domainSettings->bbStart;
	bbSize = domainSettings->bbSize;

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
		for(i=0; i<=scene->r.efra; i++) {
			timeAtIndex[i] = (float)(i-startFrame);
		}
		fluidsimInitChannel(scene, &channelDomainTime, allchannelSize, timeAtIndex, timeIcu,timeDef, domainSettings->ipo, CHANNEL_FLOAT ); // NDEB
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

		fluidsimInitChannel(scene, &channelDomainViscosity, allchannelSize, timeAtFrame, viscIcu,viscDef, domainSettings->ipo, CHANNEL_FLOAT ); // NDEB
		if(channelDomainViscosity) {
			for(i=0; i<allchannelSize; i++) { channelDomainViscosity[i*2+0] = calcViscosity * channelDomainViscosity[i*2+0]; }
		}
		fluidsimInitChannel(scene, &channelDomainGravity, allchannelSize, timeAtFrame, gravIcu,gravDef, domainSettings->ipo, CHANNEL_VEC );
	} // domain channel init
	
	// init obj movement channels
	channelObjCount=0;
	for(base=scene->base.first; base; base= base->next) 
	{
		FluidsimModifierData *fluidmdtmp = (FluidsimModifierData *)modifiers_findByType(base->object, eModifierType_Fluidsim);
		obit = base->object;
		
		if( fluidmdtmp && 
			(obit->type==OB_MESH) &&
			(fluidmdtmp->fss->type != OB_FLUIDSIM_DOMAIN) &&  // if has to match 3 places! // CHECKMATCH
			(fluidmdtmp->fss->type != OB_FLUIDSIM_PARTICLE) ) {

			//  cant use fluidsimInitChannel for obj channels right now, due
			//  to the special DXXX channels, and the rotation specialities
			IpoCurve *icuex[3][3];
			//IpoCurve *par_icuex[3][3];
#if 0
			int icuIds[3][3] = { 
				{OB_LOC_X,  OB_LOC_Y,  OB_LOC_Z},
				{OB_ROT_X,  OB_ROT_Y,  OB_ROT_Z},
				{OB_SIZE_X, OB_SIZE_Y, OB_SIZE_Z} 
			};
			int icudIds[3][3] = { 
				{OB_DLOC_X,  OB_DLOC_Y,  OB_DLOC_Z},
				{OB_DROT_X,  OB_DROT_Y,  OB_DROT_Z},
				{OB_DSIZE_X, OB_DSIZE_Y, OB_DSIZE_Z} 
			};
#endif
			// relative ipos
			IpoCurve *icudex[3][3];
			//IpoCurve *par_icudex[3][3];
			int j,k;
			float vals[3] = {0.0,0.0,0.0}; 
			int o = channelObjCount;
			int   inivelIcu[3] =  { FLUIDSIM_VEL_X, FLUIDSIM_VEL_Y, FLUIDSIM_VEL_Z };
			float inivelDefs[3];
			int   activeIcu[1] =  { FLUIDSIM_ACTIVE };
			float activeDefs[1] = { 1 }; // default to on

			inivelDefs[0] = fluidmdtmp->fss->iniVelx;
			inivelDefs[1] = fluidmdtmp->fss->iniVely;
			inivelDefs[2] = fluidmdtmp->fss->iniVelz;

			// check & init loc,rot,size
			for(j=0; j<3; j++) {
				for(k=0; k<3; k++) {
					// XXX prevent invalid memory access until this works
					icuex[j][k]= NULL;
					icudex[j][k]= NULL;

					// XXX icuex[j][k]  = find_ipocurve(obit->ipo, icuIds[j][k] );
					// XXX icudex[j][k] = find_ipocurve(obit->ipo, icudIds[j][k] );
					// XXX lines below were already disabled!
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
							// XXX calc_icu(icuex[j][k], aniFrlen*((float)i) );
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
							// XXX calc_icu(icudex[j][k], aniFrlen*((float)i) );
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
			
			{
				int   attrFSIcu[1] =  { FLUIDSIM_ATTR_FORCE_STR };
				int   attrFRIcu[1] =  { FLUIDSIM_ATTR_FORCE_RADIUS };
				int   velFSIcu[1] =  { FLUIDSIM_VEL_FORCE_STR };
				int   velFRIcu[1] =  { FLUIDSIM_VEL_FORCE_RADIUS };

				float attrFSDefs[1];
				float attrFRDefs[1];
				float velFSDefs[1];
				float velFRDefs[1];
				
				attrFSDefs[0] = fluidmdtmp->fss->attractforceStrength;
				attrFRDefs[0] = fluidmdtmp->fss->attractforceRadius;
				velFSDefs[0] = fluidmdtmp->fss->velocityforceStrength;
				velFRDefs[0] = fluidmdtmp->fss->velocityforceRadius;
				
				fluidsimInitChannel(scene, &channelAttractforceStrength[o], allchannelSize, timeAtFrame, attrFSIcu,attrFSDefs, fluidmdtmp->fss->ipo, CHANNEL_FLOAT );
				fluidsimInitChannel(scene, &channelAttractforceRadius[o], allchannelSize, timeAtFrame, attrFRIcu,attrFRDefs, fluidmdtmp->fss->ipo, CHANNEL_FLOAT );
				fluidsimInitChannel(scene, &channelVelocityforceStrength[o], allchannelSize, timeAtFrame, velFSIcu,velFSDefs, fluidmdtmp->fss->ipo, CHANNEL_FLOAT );
				fluidsimInitChannel(scene, &channelVelocityforceRadius[o], allchannelSize, timeAtFrame, velFRIcu,velFRDefs, fluidmdtmp->fss->ipo, CHANNEL_FLOAT );
			}
			
			fluidsimInitChannel(scene, &channelObjInivel[o], allchannelSize, timeAtFrame, inivelIcu,inivelDefs, fluidmdtmp->fss->ipo, CHANNEL_VEC );
			fluidsimInitChannel(scene, &channelObjActive[o], allchannelSize, timeAtFrame, activeIcu,activeDefs, fluidmdtmp->fss->ipo, CHANNEL_FLOAT );
		

			channelObjCount++;

		}
	}

	// init trafo matrix
	Mat4CpyMat4(domainMat, fsDomain->obmat);
	if(!Mat4Invert(invDomMat, domainMat)) {
		snprintf(debugStrBuffer,256,"fluidsimBake::error - Invalid obj matrix?\n"); 
		elbeemDebugOut(debugStrBuffer);
		BKE_report(reports, RPT_ERROR, "Invalid object matrix."); 
		// FIXME add fatal msg
		FS_FREE_CHANNELS;
		return 0;
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
		ListBase threads;

		// perform simulation with El'Beem api and threads
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
		fsset.noOfFrames = noFrames; // is otherwise subtracted in parser
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
		for(base=scene->base.first; base; base= base->next) {
			FluidsimModifierData *fluidmdtmp = (FluidsimModifierData *)modifiers_findByType(base->object, eModifierType_Fluidsim);
			obit = base->object;
			//{ snprintf(debugStrBuffer,256,"DEBUG object name=%s, type=%d ...\n", obit->id.name, obit->type); elbeemDebugOut(debugStrBuffer); } // DEBUG
			if( fluidmdtmp &&  // if has to match 3 places! // CHECKMATCH
				(obit->type==OB_MESH) &&
				(fluidmdtmp->fss->type != OB_FLUIDSIM_DOMAIN) &&
				(fluidmdtmp->fss->type != OB_FLUIDSIM_PARTICLE)) 
			{
				float *verts=NULL;
				int *tris=NULL;
				int numVerts=0, numTris=0;
				int o = channelObjCount;
				int	deform = (fluidmdtmp->fss->domainNovecgen); // misused value
				// todo - use blenderInitElbeemMesh
				int modifierIndex = modifiers_indexInObject(obit, (ModifierData *)fluidmdtmp);
				
				elbeemMesh fsmesh;
				elbeemResetMesh( &fsmesh );
				fsmesh.type = fluidmdtmp->fss->type;
				// get name of object for debugging solver
				fsmesh.name = obit->id.name; 

				initElbeemMesh(scene, obit, &numVerts, &verts, &numTris, &tris, 0, modifierIndex);
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
				(fsmesh.type == OB_FLUIDSIM_INFLOW)) {
					fsmesh.channelInitialVel       = channelObjInivel[o];
					fsmesh.localInivelCoords = ((fluidmdtmp->fss->typeFlags&OB_FSINFLOW_LOCALCOORD)?1:0);
				} 

				if(     (fluidmdtmp->fss->typeFlags&OB_FSBND_NOSLIP))   fsmesh.obstacleType = FLUIDSIM_OBSTACLE_NOSLIP;
				else if((fluidmdtmp->fss->typeFlags&OB_FSBND_PARTSLIP)) fsmesh.obstacleType = FLUIDSIM_OBSTACLE_PARTSLIP;
				else if((fluidmdtmp->fss->typeFlags&OB_FSBND_FREESLIP)) fsmesh.obstacleType = FLUIDSIM_OBSTACLE_FREESLIP;
				fsmesh.obstaclePartslip = fluidmdtmp->fss->partSlipValue;
				fsmesh.volumeInitType = fluidmdtmp->fss->volumeInitType;
				fsmesh.obstacleImpactFactor = fluidmdtmp->fss->surfaceSmoothing; // misused value
				
				if(fsmesh.type == OB_FLUIDSIM_CONTROL)
				{
					// control fluids will get exported as whole
					deform = 1;
					
					fsmesh.cpsTimeStart = fluidmdtmp->fss->cpsTimeStart;
					fsmesh.cpsTimeEnd = fluidmdtmp->fss->cpsTimeEnd;
					fsmesh.cpsQuality = fluidmdtmp->fss->cpsQuality;
					fsmesh.obstacleType = (fluidmdtmp->fss->flag & OB_FLUIDSIM_REVERSE);
					
					fsmesh.channelSizeAttractforceRadius = 
					fsmesh.channelSizeVelocityforceStrength = 
					fsmesh.channelSizeVelocityforceRadius = 
					fsmesh.channelSizeAttractforceStrength = allchannelSize;
					
					fsmesh.channelAttractforceStrength = channelAttractforceStrength[o];
					fsmesh.channelAttractforceRadius = channelAttractforceRadius[o];
					fsmesh.channelVelocityforceStrength = channelVelocityforceStrength[o];
					fsmesh.channelVelocityforceRadius = channelVelocityforceRadius[o];
				}
				else 
				{
					// set channels to 0
					fsmesh.channelAttractforceStrength =
					fsmesh.channelAttractforceRadius = 
					fsmesh.channelVelocityforceStrength = 
					fsmesh.channelVelocityforceRadius = NULL; 
				}

				// animated meshes
				if(deform) {
					fsmesh.channelSizeVertices = allchannelSize;
					fluidsimInitMeshChannel(C, &fsmesh.channelVertices, allchannelSize, obit, numVerts, timeAtFrame, modifierIndex);
					scene->r.cfra = startFrame;
					ED_update_for_newframe(C, 1);
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
		
		// set to neutral, -1 means user abort, -2 means init error
		globalBakeState = 0;
		globalBakeFrame = 0;
		BLI_init_threads(&threads, fluidsimSimulateThread, 1);
		BLI_insert_thread(&threads, targetFile);
		
		{
			int done = 0;
			float noFramesf = (float)noFrames;
			float percentdone = 0.0;
			int lastRedraw = -1;
			
			g_break= 0;
			G.afbreek= 0;	/* blender_test_break uses this global */
			
			start_progress_bar();

			while(done==0) {
				char busy_mess[80];
				
				waitcursor(1);
				
				// lukep we add progress bar as an interim mesure
				percentdone = globalBakeFrame / noFramesf;
				sprintf(busy_mess, "baking fluids %d / %d       |||", globalBakeFrame, (int) noFramesf);
				progress_bar(percentdone, busy_mess );
				
				// longer delay to prevent frequent redrawing
				PIL_sleep_ms(2000);
				
				BLI_lock_thread(LOCK_CUSTOM1);
				if(globalBakeState != 0) done = 1; // 1=ok, <0=error/abort
				BLI_unlock_thread(LOCK_CUSTOM1);

				if (!G.background) {
					g_break= blender_test_break();
					
					if(g_break)
					{
						// abort...
						BLI_lock_thread(LOCK_CUSTOM1);
						
						if(domainSettings)
							domainSettings->lastgoodframe = startFrame+globalBakeFrame;
						
						done = -1;
						globalBakeFrame = 0;
						globalBakeState = -1;
						simAborted = 1;
						BLI_unlock_thread(LOCK_CUSTOM1);
						break;
					}
				} 

				// redraw the 3D for showing progress once in a while...
				if(lastRedraw!=globalBakeFrame) {
#if 0					
					ScrArea *sa;
					scene->r.cfra = startFrame+globalBakeFrame;
					lastRedraw = globalBakeFrame;
					ED_update_for_newframe(C, 1);
					sa= G.curscreen->areabase.first;
					while(sa) {
						if(sa->spacetype == SPACE_VIEW3D) { scrarea_do_windraw(sa); }
						sa= sa->next;	
					} 
					screen_swapbuffers();
#endif
				} // redraw
			}
			end_progress_bar();
		}
		BLI_end_threads(&threads);
	} // El'Beem API init, thread creation 
	// --------------------------------------------------------------------------------------------
	else
	{ // write config file to be run with command line simulator
		BKE_report(reports, RPT_WARNING, "Config file export not supported.");
	} // config file export done!

	// --------------------------------------------------------------------------------------------
	FS_FREE_CHANNELS;

	// go back to "current" blender time
	waitcursor(0);
	
	if(globalBakeState >= 0)
	{
		if(domainSettings)
			domainSettings->lastgoodframe = startFrame+globalBakeFrame;
	}
	
	scene->r.cfra = origFrame;
	ED_update_for_newframe(C, 1);

	if(!simAborted) {
		char elbeemerr[256];

		// check if some error occurred
		if(globalBakeState==-2) {
			elbeemGetErrorString(elbeemerr);
			BKE_reportf(reports, RPT_ERROR, "Failed to initialize [Msg: %s]", elbeemerr);
			return 0;
		} // init error
	}
	
	// elbeemFree();
	return 1;
}

void fluidsimFreeBake(Object *ob)
{
	/* not implemented yet */
}

#else /* DISABLE_ELBEEM */

/* compile dummy functions for disabled fluid sim */

FluidsimSettings *fluidsimSettingsNew(Object *srcob)
{
	return NULL;
}

void fluidsimSettingsFree(FluidsimSettings *fss)
{
}

FluidsimSettings* fluidsimSettingsCopy(FluidsimSettings *fss)
{
	return NULL;
}

/* only compile dummy functions */
int fluidsimBake(bContext *C, ReportList *reports, Object *ob)
{
	return 0;
}

void fluidsimFreeBake(Object *ob)
{
}

#endif /* DISABLE_ELBEEM */

/***************************** Operators ******************************/

static int fluid_bake_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);

	// XXX TODO redraw, escape, non-blocking, ..
	if(!fluidsimBake(C, op->reports, ob))
		return OPERATOR_CANCELLED;

	return OPERATOR_FINISHED;
}

void FLUID_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Fluid Simulation Bake";
	ot->idname= "FLUID_OT_bake";
	
	/* api callbacks */
	ot->exec= fluid_bake_exec;
	ot->poll= ED_operator_object_active;
}

