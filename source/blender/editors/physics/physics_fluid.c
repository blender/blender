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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_object_types.h"
#include "DNA_object_fluidsim.h"	

#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_math.h"

#include "BKE_animsys.h"
#include "BKE_armature.h"
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
#include "BKE_unit.h"


#include "LBM_fluidsim.h"

#include "BIF_gl.h"

#include "ED_screen.h"

#include "WM_types.h"

#include "physics_intern.h" // own include

/* enable/disable overall compilation */
#ifndef DISABLE_ELBEEM

#include "WM_api.h"

#include "DNA_scene_types.h"
#include "DNA_ipo_types.h"
#include "DNA_mesh_types.h"

#include "PIL_time.h"


static float get_fluid_viscosity(FluidsimSettings *settings)
{
	switch (settings->viscosityMode) {
		case 0:		/* unused */
			return -1.0;
		case 2:		/* water */
			return 1.0e-6;
		case 3:		/* some (thick) oil */
			return 5.0e-5;
		case 4:		/* ca. honey */
			return 2.0e-3;
		case 1:		/* manual */
		default:
			return (1.0/pow(10.0, settings->viscosityExponent)) * settings->viscosityValue;
	}
}

static void get_fluid_gravity(float *gravity, Scene *scene, FluidsimSettings *fss)
{
	if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
		copy_v3_v3(gravity, scene->physics_settings.gravity);
	} else {
		copy_v3_v3(gravity, &fss->gravx);
	}
}

static float get_fluid_size_m(Scene *scene, Object *domainob, FluidsimSettings *fss)
{
	if (!scene->unit.system) {
		return fss->realsize;
	} else {
		float dim[3];
		float longest_axis;
		
		object_get_dimensions(domainob, dim);
		longest_axis = MAX3(dim[0], dim[1], dim[2]);
		
		return longest_axis * scene->unit.scale_length;
	}
}

static int fluid_is_animated_mesh(FluidsimSettings *fss)
{
	return ((fss->type == OB_FLUIDSIM_CONTROL) || fss->domainNovecgen);
}

/* ********************** fluid sim settings struct functions ********************** */

#if 0
/* helper function */
void fluidsimGetGeometryObjFilename(Object *ob, char *dst) { //, char *srcname) {
	//snprintf(dst,FILE_MAXFILE, "%s_cfgdata_%s.bobj.gz", srcname, ob->id.name);
	snprintf(dst,FILE_MAXFILE, "fluidcfgdata_%s.bobj.gz", ob->id.name);
}
#endif


/* ********************** fluid sim channel helper functions ********************** */

typedef struct FluidAnimChannels {
	int length;
	
	double aniFrameTime;
	
	float *timeAtFrame;
	float *DomainTime;
	float *DomainGravity;
	float *DomainViscosity;
} FluidAnimChannels;

typedef struct FluidObject {
	struct FluidObject *next, *prev;
	
	struct Object *object;
	
	float *Translation;
	float *Rotation;
	float *Scale;
	float *Active;
	
	float *InitialVelocity;
	
	float *AttractforceStrength;
	float *AttractforceRadius;
	float *VelocityforceStrength;
	float *VelocityforceRadius;
	
	float *VertexCache;
	int numVerts, numTris;
} FluidObject;

// no. of entries for the two channel sizes
#define CHANNEL_FLOAT 1
#define CHANNEL_VEC   3

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


/* Note: fluid anim channel data layout
 * ------------------------------------
 * CHANNEL_FLOAT:
 * frame 1     |frame 2
 * [dataF][time][dataF][time]
 *
 * CHANNEL_VEC:
 * frame 1                   |frame 2
 * [dataX][dataY][dataZ][time][dataX][dataY][dataZ][time]
 *
 */

static void init_time(FluidsimSettings *domainSettings, FluidAnimChannels *channels)
{
	int i;
	
	channels->timeAtFrame = MEM_callocN( (channels->length+1)*sizeof(float), "timeAtFrame channel");
	
	channels->timeAtFrame[0] = channels->timeAtFrame[1] = domainSettings->animStart; // start at index 1
	
	for(i=2; i<=channels->length; i++) {
		channels->timeAtFrame[i] = channels->timeAtFrame[i-1] + channels->aniFrameTime;
	}
}

/* if this is slow, can replace with faster, less readable code */
static void set_channel(float *channel, float time, float *value, int i, int size)
{
	if (size == CHANNEL_FLOAT) {
		channel[(i * 2) + 0] = value[0];
		channel[(i * 2) + 1] = time;
	}
	else if (size == CHANNEL_VEC) {
		channel[(i * 4) + 0] = value[0];
		channel[(i * 4) + 1] = value[1];
		channel[(i * 4) + 2] = value[2];
		channel[(i * 4) + 3] = time;
	}
}

static void set_vertex_channel(float *channel, float time, struct Scene *scene, struct FluidObject *fobj, int i)
{
	Object *ob = fobj->object;
	FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
	float *verts;
	int *tris=NULL, numVerts=0, numTris=0;
	int modifierIndex = modifiers_indexInObject(ob, (ModifierData *)fluidmd);
	int framesize = (3*fobj->numVerts) + 1;
	int j;
	
	if (channel == NULL)
		return;
	
	initElbeemMesh(scene, ob, &numVerts, &verts, &numTris, &tris, 1, modifierIndex);
	
	/* don't allow mesh to change number of verts in anim sequence */
	if (numVerts != fobj->numVerts) {
		MEM_freeN(channel);
		channel = NULL;
		return;
	}
	
	/* fill frame of channel with vertex locations */
	for(j=0; j < (3*numVerts); j++) {
		channel[i*framesize + j] = verts[j];
	}
	channel[i*framesize + framesize-1] = time;
	
	MEM_freeN(verts);
	MEM_freeN(tris);
}

static void free_domain_channels(FluidAnimChannels *channels)
{
	if (!channels->timeAtFrame)
		return;
	MEM_freeN(channels->timeAtFrame);
	channels->timeAtFrame = NULL;
	MEM_freeN(channels->DomainGravity);
	channels->DomainGravity = NULL;
	MEM_freeN(channels->DomainViscosity);
	channels->DomainViscosity = NULL;
}

static void free_all_fluidobject_channels(ListBase *fobjects)
{
	FluidObject *fobj;
	
	for (fobj=fobjects->first; fobj; fobj=fobj->next) {
		if (fobj->Translation) {
			MEM_freeN(fobj->Translation);
			fobj->Translation = NULL;
			MEM_freeN(fobj->Rotation);
			fobj->Rotation = NULL;
			MEM_freeN(fobj->Scale);
			fobj->Scale = NULL;
			MEM_freeN(fobj->Active);
			fobj->Active = NULL;
			MEM_freeN(fobj->InitialVelocity);
			fobj->InitialVelocity = NULL;
		}
		
		if (fobj->AttractforceStrength) {
			MEM_freeN(fobj->AttractforceStrength);
			fobj->AttractforceStrength = NULL;
			MEM_freeN(fobj->AttractforceRadius);
			fobj->AttractforceRadius = NULL;
			MEM_freeN(fobj->VelocityforceStrength);
			fobj->VelocityforceStrength = NULL;
			MEM_freeN(fobj->VelocityforceRadius);
			fobj->VelocityforceRadius = NULL;
		}
		
		if (fobj->VertexCache) {
			MEM_freeN(fobj->VertexCache);
			fobj->VertexCache = NULL;
		}
	}
}

static void fluid_init_all_channels(bContext *C, Object *fsDomain, FluidsimSettings *domainSettings, FluidAnimChannels *channels, ListBase *fobjects)
{
	Scene *scene = CTX_data_scene(C);
	Base *base;
	int i;
	int length = channels->length;
	float eval_time;
	
	/* XXX: first init time channel - temporary for now */
	/* init time values (should be done after evaluating animated time curve) */
	init_time(domainSettings, channels);
	
	/* allocate domain animation channels */
	channels->DomainGravity = MEM_callocN( length * (CHANNEL_VEC+1) * sizeof(float), "channel DomainGravity");
	channels->DomainViscosity = MEM_callocN( length * (CHANNEL_FLOAT+1) * sizeof(float), "channel DomainViscosity");
	//channels->DomainTime = MEM_callocN( length * (CHANNEL_FLOAT+1) * sizeof(float), "channel DomainTime");
	
	/* allocate fluid objects */
	for (base=scene->base.first; base; base= base->next) {
		Object *ob = base->object;
		FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
		
		if (fluidmd) {
			FluidObject *fobj = MEM_callocN(sizeof(FluidObject), "Fluid Object");
			fobj->object = ob;
			
			if (ELEM(fluidmd->fss->type, OB_FLUIDSIM_DOMAIN, OB_FLUIDSIM_PARTICLE)) {
				BLI_addtail(fobjects, fobj);
				continue;
			}
			
			fobj->Translation = MEM_callocN( length * (CHANNEL_VEC+1) * sizeof(float), "fluidobject Translation");
			fobj->Rotation = MEM_callocN( length * (CHANNEL_VEC+1) * sizeof(float), "fluidobject Rotation");
			fobj->Scale = MEM_callocN( length * (CHANNEL_VEC+1) * sizeof(float), "fluidobject Scale");
			fobj->Active = MEM_callocN( length * (CHANNEL_FLOAT+1) * sizeof(float), "fluidobject Active");
			fobj->InitialVelocity = MEM_callocN( length * (CHANNEL_VEC+1) * sizeof(float), "fluidobject InitialVelocity");
			
			if (fluidmd->fss->type == OB_FLUIDSIM_CONTROL) {
				fobj->AttractforceStrength = MEM_callocN( length * (CHANNEL_FLOAT+1) * sizeof(float), "fluidobject AttractforceStrength");
				fobj->AttractforceRadius = MEM_callocN( length * (CHANNEL_FLOAT+1) * sizeof(float), "fluidobject AttractforceRadius");
				fobj->VelocityforceStrength = MEM_callocN( length * (CHANNEL_FLOAT+1) * sizeof(float), "fluidobject VelocityforceStrength");
				fobj->VelocityforceRadius = MEM_callocN( length * (CHANNEL_FLOAT+1) * sizeof(float), "fluidobject VelocityforceRadius");
			}
			
			if (fluid_is_animated_mesh(fluidmd->fss)) {
				float *verts=NULL;
				int *tris=NULL, modifierIndex = modifiers_indexInObject(ob, (ModifierData *)fluidmd);

				initElbeemMesh(scene, ob, &fobj->numVerts, &verts, &fobj->numTris, &tris, 0, modifierIndex);
				fobj->VertexCache = MEM_callocN( length *((fobj->numVerts*CHANNEL_VEC)+1) * sizeof(float), "fluidobject VertexCache");
				
				MEM_freeN(verts);
				MEM_freeN(tris);
			}
			
			BLI_addtail(fobjects, fobj);
		}
	}
	
	/* now we loop over the frames and fill the allocated channels with data */
	for (i=0; i<channels->length; i++) {
		FluidObject *fobj;
		float viscosity, gravity[3];
		float timeAtFrame;
		
		eval_time = domainSettings->bakeStart + i;
		timeAtFrame = channels->timeAtFrame[i+1];
		
		/* XXX: This can't be used due to an anim sys optimisation that ignores recalc object animation,
		 * leaving it for the depgraph (this ignores object animation such as modifier properties though... :/ )
		 * --> BKE_animsys_evaluate_all_animation(G.main, eval_time);
		 * This doesn't work with drivers:
		 * --> BKE_animsys_evaluate_animdata(&fsDomain->id, fsDomain->adt, eval_time, ADT_RECALC_ALL);
		 */
		
		/* Modifying the global scene isn't nice, but we can do it in 
		 * this part of the process before a threaded job is created */
		scene->r.cfra = (int)eval_time;
		ED_update_for_newframe(C, 1);
		
		/* now scene data should be current according to animation system, so we fill the channels */
		
		/* Domain properties - gravity/viscosity/time */
		get_fluid_gravity(gravity, scene, domainSettings);
		set_channel(channels->DomainGravity, timeAtFrame, gravity, i, CHANNEL_VEC);
		viscosity = get_fluid_viscosity(domainSettings);
		set_channel(channels->DomainViscosity, timeAtFrame, &viscosity, i, CHANNEL_FLOAT);
		// XXX : set_channel(channels->DomainTime, timeAtFrame, &time, i, CHANNEL_VEC);
		
		/* object movement */
		for (fobj=fobjects->first; fobj; fobj=fobj->next) {
			Object *ob = fobj->object;
			FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
			float active= (float)(fluidmd->fss->flag & OB_FLUIDSIM_ACTIVE);
			float rot_d[3], rot_360[3] = {360.f, 360.f, 360.f};
			
			if (ELEM(fluidmd->fss->type, OB_FLUIDSIM_DOMAIN, OB_FLUIDSIM_PARTICLE))
				continue;
			
			/* init euler rotation values and convert to elbeem format */
			BKE_rotMode_change_values(ob->quat, ob->rot, ob->rotAxis, &ob->rotAngle, ob->rotmode, ROT_MODE_EUL);
			mul_v3_v3fl(rot_d, ob->rot, 180.f/M_PI);
			sub_v3_v3v3(rot_d, rot_360, rot_d);
			
			set_channel(fobj->Translation, timeAtFrame, ob->loc, i, CHANNEL_VEC);
			set_channel(fobj->Rotation, timeAtFrame, rot_d, i, CHANNEL_VEC);
			set_channel(fobj->Scale, timeAtFrame, ob->size, i, CHANNEL_VEC);
			set_channel(fobj->Active, timeAtFrame, &active, i, CHANNEL_FLOAT);
			set_channel(fobj->InitialVelocity, timeAtFrame, &fluidmd->fss->iniVelx, i, CHANNEL_VEC);
			
			if (fluidmd->fss->type == OB_FLUIDSIM_CONTROL) {
				set_channel(fobj->AttractforceStrength, timeAtFrame, &fluidmd->fss->attractforceStrength, i, CHANNEL_FLOAT);
				set_channel(fobj->AttractforceRadius, timeAtFrame, &fluidmd->fss->attractforceRadius, i, CHANNEL_FLOAT);
				set_channel(fobj->VelocityforceStrength, timeAtFrame, &fluidmd->fss->velocityforceStrength, i, CHANNEL_FLOAT);
				set_channel(fobj->VelocityforceRadius, timeAtFrame, &fluidmd->fss->velocityforceRadius, i, CHANNEL_FLOAT);
			}
			
			if (fluid_is_animated_mesh(fluidmd->fss)) {
				set_vertex_channel(fobj->VertexCache, timeAtFrame, scene, fobj, i);
			}
		}
	}
}

static void export_fluid_objects(ListBase *fobjects, Scene *scene, int length)
{
	FluidObject *fobj;
	
	for (fobj=fobjects->first; fobj; fobj=fobj->next) {
		Object *ob = fobj->object;
		FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
		int modifierIndex = modifiers_indexInObject(ob, (ModifierData *)fluidmd);
		
		float *verts=NULL;
		int *tris=NULL;
		int numVerts=0, numTris=0;
		int deform = fluid_is_animated_mesh(fluidmd->fss);
		
		elbeemMesh fsmesh;
		
		if (ELEM(fluidmd->fss->type, OB_FLUIDSIM_DOMAIN, OB_FLUIDSIM_PARTICLE))
			continue;
		
		elbeemResetMesh( &fsmesh );
		
		fsmesh.type = fluidmd->fss->type;
		fsmesh.name = ob->id.name;
		
		initElbeemMesh(scene, ob, &numVerts, &verts, &numTris, &tris, 0, modifierIndex);
		
		fsmesh.numVertices   = numVerts;
		fsmesh.numTriangles  = numTris;
		fsmesh.vertices      = verts;
		fsmesh.triangles     = tris;
		
		fsmesh.channelSizeTranslation  = 
		fsmesh.channelSizeRotation     = 
		fsmesh.channelSizeScale        = 
		fsmesh.channelSizeInitialVel   = 
		fsmesh.channelSizeActive       = length;
		
		fsmesh.channelTranslation      = fobj->Translation;
		fsmesh.channelRotation         = fobj->Rotation;
		fsmesh.channelScale            = fobj->Scale;
		fsmesh.channelActive           = fobj->Active;
		
		if( ELEM(fsmesh.type, OB_FLUIDSIM_FLUID, OB_FLUIDSIM_INFLOW)) {
			fsmesh.channelInitialVel = fobj->InitialVelocity;
			fsmesh.localInivelCoords = ((fluidmd->fss->typeFlags & OB_FSINFLOW_LOCALCOORD)?1:0);
		} 
		
		if(fluidmd->fss->typeFlags & OB_FSBND_NOSLIP)
			fsmesh.obstacleType = FLUIDSIM_OBSTACLE_NOSLIP;
		else if(fluidmd->fss->typeFlags & OB_FSBND_PARTSLIP)
			fsmesh.obstacleType = FLUIDSIM_OBSTACLE_PARTSLIP;
		else if(fluidmd->fss->typeFlags & OB_FSBND_FREESLIP)
			fsmesh.obstacleType = FLUIDSIM_OBSTACLE_FREESLIP;
		
		fsmesh.obstaclePartslip = fluidmd->fss->partSlipValue;
		fsmesh.volumeInitType = fluidmd->fss->volumeInitType;
		fsmesh.obstacleImpactFactor = fluidmd->fss->surfaceSmoothing; // misused value
		
		if (fsmesh.type == OB_FLUIDSIM_CONTROL)	{
			fsmesh.cpsTimeStart = fluidmd->fss->cpsTimeStart;
			fsmesh.cpsTimeEnd = fluidmd->fss->cpsTimeEnd;
			fsmesh.cpsQuality = fluidmd->fss->cpsQuality;
			fsmesh.obstacleType = (fluidmd->fss->flag & OB_FLUIDSIM_REVERSE);
			
			fsmesh.channelSizeAttractforceRadius = 
			fsmesh.channelSizeVelocityforceStrength = 
			fsmesh.channelSizeVelocityforceRadius = 
			fsmesh.channelSizeAttractforceStrength = length;
			
			fsmesh.channelAttractforceStrength = fobj->AttractforceStrength;
			fsmesh.channelAttractforceRadius = fobj->AttractforceRadius;
			fsmesh.channelVelocityforceStrength = fobj->VelocityforceStrength;
			fsmesh.channelVelocityforceRadius = fobj->VelocityforceRadius;
		}
		else {
			fsmesh.channelAttractforceStrength =
			fsmesh.channelAttractforceRadius = 
			fsmesh.channelVelocityforceStrength = 
			fsmesh.channelVelocityforceRadius = NULL; 
		}
		
		/* animated meshes */
		if(deform) {
			fsmesh.channelSizeVertices = length;
			fsmesh.channelVertices = fobj->VertexCache;
				
			// remove channels
			fsmesh.channelTranslation      = 
			fsmesh.channelRotation         = 
			fsmesh.channelScale            = NULL; 
		}
		
		elbeemAddMesh(&fsmesh);
		
		if(verts) MEM_freeN(verts);
		if(tris) MEM_freeN(tris);
		if(fsmesh.channelVertices) MEM_freeN(fsmesh.channelVertices);
	}
}

static int fluid_validate_scene(ReportList *reports, Scene *scene, Object *fsDomain)
{
	Base *base;
	Object *newdomain = NULL;
	int channelObjCount = 0;
	int fluidInputCount = 0;

	for(base=scene->base.first; base; base= base->next)
	{
		Object *ob = base->object;
		FluidsimModifierData *fluidmdtmp = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);			

		/* only find objects with fluid modifiers */
		if (!fluidmdtmp || ob->type != OB_MESH) continue;
			
		if(fluidmdtmp->fss->type == OB_FLUIDSIM_DOMAIN) {
			/* if no initial domain object given, find another potential domain */
			if (!fsDomain) {
				newdomain = ob;
			}
			/* if there's more than one domain, cancel */
			else if (fsDomain && ob != fsDomain) {
				BKE_report(reports, RPT_ERROR, "There should be only one domain object.");
				return 0;
			}
		}
		
		/* count number of objects needed for animation channels */
		if ( !ELEM(fluidmdtmp->fss->type, OB_FLUIDSIM_DOMAIN, OB_FLUIDSIM_PARTICLE) )
			channelObjCount++;
		
		/* count number of fluid input objects */
		if (ELEM(fluidmdtmp->fss->type, OB_FLUIDSIM_FLUID, OB_FLUIDSIM_INFLOW))
			fluidInputCount++;
	}

	if (newdomain)
		fsDomain = newdomain;
	
	if (!fsDomain) {
		BKE_report(reports, RPT_ERROR, "No domain object found.");
		return 0;
	}
	
	if (channelObjCount>=255) {
		BKE_report(reports, RPT_ERROR, "Cannot bake with more then 256 objects.");
		return 0;
	}
	
	if (fluidInputCount == 0) {
		BKE_report(reports, RPT_ERROR, "No fluid input objects in the scene.");
		return 0;
	}
	
	return 1;
}


#define FLUID_SUFFIX_CONFIG		"fluidsim.cfg"
#define FLUID_SUFFIX_SURFACE	"fluidsurface"

static int fluid_init_filepaths(Object *fsDomain, char *targetDir, char *targetFile, char *debugStrBuffer)
{
	FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(fsDomain, eModifierType_Fluidsim);
	FluidsimSettings *domainSettings= fluidmd->fss;	
	FILE *fileCfg;
	int dirExist = 0;
	char newSurfdataPath[FILE_MAXDIR+FILE_MAXFILE]; // modified output settings
	char *suffixConfig = FLUID_SUFFIX_CONFIG;
	int outStringsChanged = 0;
	
	// prepare names...
	strncpy(targetDir, domainSettings->surfdataPath, FILE_MAXDIR);
	strncpy(newSurfdataPath, domainSettings->surfdataPath, FILE_MAXDIR);
	BLI_path_abs(targetDir, G.sce); // fixed #frame-no 
	
	strcpy(targetFile, targetDir);
	strcat(targetFile, suffixConfig);
	strcat(targetFile,".tmp"); // dont overwrite/delete original file
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
		BLI_delete(targetFile, 0,0);
	}
	
	if((strlen(targetDir)<1) || (!dirExist)) {
		char blendDir[FILE_MAXDIR+FILE_MAXFILE];
		char blendFile[FILE_MAXDIR+FILE_MAXFILE];
		
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
#if 0
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
		BLI_path_abs(targetDir, G.sce); // fixed #frame-no 
	}
#endif	
	return outStringsChanged;
}

/* ******************************************************************************** */
/* ********************** write fluidsim config to file ************************* */
/* ******************************************************************************** */

typedef struct FluidBakeJob {
	/* from wmJob */
	void *owner;
	short *stop, *do_update;
	float *progress;
	int current_frame;
	elbeemSimulationSettings *settings;
} FluidBakeJob;

static void fluidbake_free(void *customdata)
{
	FluidBakeJob *fb= customdata;
	MEM_freeN(fb);
}

/* called by fluidbake, only to check job 'stop' value */
static int fluidbake_breakjob(void *customdata)
{
	//FluidBakeJob *fb= (FluidBakeJob *)customdata;
	//return *(fb->stop);
	
	/* this is not nice yet, need to make the jobs list template better 
	 * for identifying/acting upon various different jobs */
	/* but for now we'll reuse the render break... */
	return (G.afbreek);
}

/* called by fluidbake, wmJob sends notifier */
static void fluidbake_updatejob(void *customdata, float progress)
{
	FluidBakeJob *fb= customdata;
	
	*(fb->do_update)= 1;
	*(fb->progress)= progress;
}

static void fluidbake_startjob(void *customdata, short *stop, short *do_update, float *progress)
{
	FluidBakeJob *fb= customdata;
	
	fb->stop= stop;
	fb->do_update = do_update;
	fb->progress = progress;
	
	G.afbreek= 0;	/* XXX shared with render - replace with job 'stop' switch */
	
	elbeemSimulate();
	*do_update= 1;
	*stop = 0;
}

static void fluidbake_endjob(void *customdata)
{
	FluidBakeJob *fb= customdata;
	
	if (fb->settings) {
		MEM_freeN(fb->settings);
		fb->settings = NULL;
	}
}

int runSimulationCallback(void *data, int status, int frame) {
	FluidBakeJob *fb = (FluidBakeJob *)data;
	elbeemSimulationSettings *settings = fb->settings;
	
	if (status == FLUIDSIM_CBSTATUS_NEWFRAME) {
		fluidbake_updatejob(fb, frame / (float)settings->noOfFrames);
		//printf("elbeem blender cb s%d, f%d, domainid:%d noOfFrames: %d \n", status,frame, settings->domainId, settings->noOfFrames ); // DEBUG
	}
	
	if (fluidbake_breakjob(fb))  {
		return FLUIDSIM_CBRET_ABORT;
	}
	
	return FLUIDSIM_CBRET_CONTINUE;
}

static void fluidbake_free_data(FluidAnimChannels *channels, ListBase *fobjects, elbeemSimulationSettings *fsset, FluidBakeJob *fb)
{
	free_domain_channels(channels);
	MEM_freeN(channels);
	channels = NULL;

	free_all_fluidobject_channels(fobjects);
	BLI_freelistN(fobjects);
	MEM_freeN(fobjects);
	fobjects = NULL;
	
	if (fsset) {
		MEM_freeN(fsset);
		fsset = NULL;
	}
	
	if (fb) {
		MEM_freeN(fb);
		fb = NULL;
	}
}

int fluidsimBake(bContext *C, ReportList *reports, Object *fsDomain)
{
	Scene *scene= CTX_data_scene(C);
	int i;
	FluidsimSettings *domainSettings;

	char debugStrBuffer[256];
	
	int gridlevels = 0;
	const char *strEnvName = "BLENDER_ELBEEMDEBUG"; // from blendercall.cpp
	char *suffixConfig = FLUID_SUFFIX_CONFIG;
	char *suffixSurface = FLUID_SUFFIX_SURFACE;

	char targetDir[FILE_MAXDIR+FILE_MAXFILE];  // store & modify output settings
	char targetFile[FILE_MAXDIR+FILE_MAXFILE]; // temp. store filename from targetDir for access
	int  outStringsChanged = 0;             // modified? copy back before baking

	float domainMat[4][4];
	float invDomMat[4][4];

	int noFrames;
	int origFrame = scene->r.cfra;
	
	FluidAnimChannels *channels = MEM_callocN(sizeof(FluidAnimChannels), "fluid domain animation channels");
	ListBase *fobjects = MEM_callocN(sizeof(ListBase), "fluid objects");
	FluidsimModifierData *fluidmd = NULL;
	Mesh *mesh = NULL;
	
	wmJob *steve;
	FluidBakeJob *fb;
	elbeemSimulationSettings *fsset= MEM_callocN(sizeof(elbeemSimulationSettings), "Fluid sim settings");
	
	steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, "Fluid Simulation", WM_JOB_PROGRESS);
	fb= MEM_callocN(sizeof(FluidBakeJob), "fluid bake job");
	
	if(getenv(strEnvName)) {
		int dlevel = atoi(getenv(strEnvName));
		elbeemSetDebugLevel(dlevel);
		snprintf(debugStrBuffer,256,"fluidsimBake::msg: Debug messages activated due to envvar '%s'\n",strEnvName); 
		elbeemDebugOut(debugStrBuffer);
	}
	
	/* make sure it corresponds to startFrame setting (old: noFrames = scene->r.efra - scene->r.sfra +1) */;
	noFrames = scene->r.efra - 0;
	if(noFrames<=0) {
		BKE_report(reports, RPT_ERROR, "No frames to export - check your animation range settings.");
		fluidbake_free_data(channels, fobjects, fsset, fb);
		return 0;
	}
	
	/* check scene for sane object/modifier settings */
	if (!fluid_validate_scene(reports, scene, fsDomain)) {
		fluidbake_free_data(channels, fobjects, fsset, fb);
		return 0;
	}
	
	/* these both have to be valid, otherwise we wouldnt be here */
	fluidmd = (FluidsimModifierData *)modifiers_findByType(fsDomain, eModifierType_Fluidsim);
	domainSettings = fluidmd->fss;
	mesh = fsDomain->data;
	
	domainSettings->bakeStart = 1;
	domainSettings->bakeEnd = scene->r.efra;
	
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
	
	
	
	/* ******** prepare output file paths ******** */
	outStringsChanged = fluid_init_filepaths(fsDomain, targetDir, targetFile, debugStrBuffer);
	channels->length = scene->r.efra;
	channels->aniFrameTime = (domainSettings->animEnd - domainSettings->animStart)/(double)noFrames;
	
	/* ******** initialise and allocate animation channels ******** */
	fluid_init_all_channels(C, fsDomain, domainSettings, channels, fobjects);

	/* reset to original current frame */
	scene->r.cfra = origFrame;
	ED_update_for_newframe(C, 1);
	
	
	/* ---- XXX: No Time animation curve for now, leaving this code here for reference 
	 
	{ int timeIcu[1] = { FLUIDSIM_TIME };
		float timeDef[1] = { 1. };

		// time channel is a bit special, init by hand...
		timeAtIndex = MEM_callocN( (allchannelSize+1)*1*sizeof(float), "fluidsiminit_timeatindex");
		for(i=0; i<=scene->r.efra; i++) {
			timeAtIndex[i] = (float)(i-startFrame);
		}
		fluidsimInitChannel(scene, &channelDomainTime, allchannelSize, timeAtIndex, timeIcu,timeDef, domainSettings->ipo, CHANNEL_FLOAT ); // NDEB
		// time channel is a multiplicator for 
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
		fsset->} else {
			for(i=2; i<=allchannelSize; i++) { timeAtFrame[i] = timeAtFrame[i-1]+aniFrameTime; }
		}

	} // domain channel init
	*/
		
	/* ******** init domain object's matrix ******** */
	copy_m4_m4(domainMat, fsDomain->obmat);
	if(!invert_m4_m4(invDomMat, domainMat)) {
		snprintf(debugStrBuffer,256,"fluidsimBake::error - Invalid obj matrix?\n"); 
		elbeemDebugOut(debugStrBuffer);
		BKE_report(reports, RPT_ERROR, "Invalid object matrix."); 

		fluidbake_free_data(channels, fobjects, fsset, fb);
		return 0;
	}

	/* ********  start writing / exporting ******** */
	strcpy(targetFile, targetDir);
	strcat(targetFile, suffixConfig);
	strcat(targetFile,".tmp");  // dont overwrite/delete original file
	
	// make sure these directories exist as well
	if(outStringsChanged) {
		BLI_make_existing_file(targetFile);
	}

	/* ******** export domain to elbeem ******** */
	elbeemResetSettings(fsset);
	fsset->version = 1;

	// setup global settings
	copy_v3_v3(fsset->geoStart, domainSettings->bbStart);
	copy_v3_v3(fsset->geoSize, domainSettings->bbSize);
	
	// simulate with 50^3
	fsset->resolutionxyz = (int)domainSettings->resolutionxyz;
	fsset->previewresxyz = (int)domainSettings->previewresxyz;

	fsset->realsize = get_fluid_size_m(scene, fsDomain, domainSettings);
	fsset->viscosity = get_fluid_viscosity(domainSettings);
	get_fluid_gravity(fsset->gravity, scene, domainSettings);

	// simulate 5 frames, each 0.03 seconds, output to ./apitest_XXX.bobj.gz
	fsset->animStart = domainSettings->animStart;
	fsset->aniFrameTime = channels->aniFrameTime;
	fsset->noOfFrames = noFrames; // is otherwise subtracted in parser

	strcpy(targetFile, targetDir);
	strcat(targetFile, suffixSurface);
	// defaults for compressibility and adaptive grids
	fsset->gstar = domainSettings->gstar;
	fsset->maxRefine = domainSettings->maxRefine; // check <-> gridlevels
	fsset->generateParticles = domainSettings->generateParticles; 
	fsset->numTracerParticles = domainSettings->generateTracers; 
	fsset->surfaceSmoothing = domainSettings->surfaceSmoothing; 
	fsset->surfaceSubdivs = domainSettings->surfaceSubdivs; 
	fsset->farFieldSize = domainSettings->farFieldSize; 
	strcpy( fsset->outputPath, targetFile);

	// domain channels
	fsset->channelSizeFrameTime = 
	fsset->channelSizeViscosity = 
	fsset->channelSizeGravity = channels->length;
	fsset->channelFrameTime = channels->DomainTime;
	fsset->channelViscosity = channels->DomainViscosity;
	fsset->channelGravity = channels->DomainGravity;
	
	fsset->runsimCallback = &runSimulationCallback;
	fsset->runsimUserData = fb;

	if (domainSettings->typeFlags & OB_FSBND_NOSLIP)		fsset->domainobsType = FLUIDSIM_OBSTACLE_NOSLIP;
	else if (domainSettings->typeFlags&OB_FSBND_PARTSLIP)	fsset->domainobsType = FLUIDSIM_OBSTACLE_PARTSLIP;
	else if (domainSettings->typeFlags&OB_FSBND_FREESLIP)	fsset->domainobsType = FLUIDSIM_OBSTACLE_FREESLIP;
	fsset->domainobsPartslip = domainSettings->partSlipValue;
	fsset->generateVertexVectors = (domainSettings->domainNovecgen==0);

	// init blender domain transform matrix
	{ int j; 
	for(i=0; i<4; i++) {
		for(j=0; j<4; j++) {
			fsset->surfaceTrafo[i*4+j] = invDomMat[j][i];
		}
	} }

	/* ******** init solver with settings ******** */
	elbeemInit();
	elbeemAddDomain(fsset);
	
	/* ******** export all fluid objects to elbeem ******** */
	export_fluid_objects(fobjects, scene, channels->length);
	
	/* custom data for fluid bake job */
	fb->settings = fsset;
	
	/* setup job */
	WM_jobs_customdata(steve, fb, fluidbake_free);
	WM_jobs_timer(steve, 0.1, NC_SCENE|ND_FRAME, NC_SCENE|ND_FRAME);
	WM_jobs_callbacks(steve, fluidbake_startjob, NULL, NULL, fluidbake_endjob);
	
	WM_jobs_start(CTX_wm_manager(C), steve);

	/* ******** free stored animation data ******** */
	fluidbake_free_data(channels, fobjects, NULL, NULL);

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

	if(!fluidsimBake(C, op->reports, ob))
		return OPERATOR_CANCELLED;

	return OPERATOR_FINISHED;
}

void FLUID_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Fluid Simulation Bake";
	ot->description= "Bake fluid simulation";
	ot->idname= "FLUID_OT_bake";
	
	/* api callbacks */
	ot->exec= fluid_bake_exec;
	ot->poll= ED_operator_object_active_editable;
}

