/**
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
* Contributor(s): Campbell Barton <ideasman42@gmail.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_cloth_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"

#include "BKE_cloth.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"

#include "blendef.h"

/* needed for directory lookup */
#ifndef WIN32
  #include <dirent.h>
#else
  #include "BLI_winstuff.h"
#endif

/* untitled blend's need getpid for a unique name */
#ifdef WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

/* Creating ID's */

void BKE_ptcache_id_from_softbody(PTCacheID *pid, Object *ob, SoftBody *sb)
{
	ParticleSystemModifierData *psmd;
	ModifierData *md;
	int a;

	memset(pid, 0, sizeof(PTCacheID));

	pid->ob= ob;
	pid->data= sb;
	pid->type= PTCACHE_TYPE_SOFTBODY;
	pid->cache= sb->pointcache;

	if(sb->particles) {
		psmd= psys_get_modifier(ob, sb->particles);
		pid->stack_index= modifiers_indexInObject(ob, (ModifierData*)psmd);
	}
	else {
		for(a=0, md=ob->modifiers.first; md; md=md->next, a++) {
			if(md->type == eModifierType_Softbody) {
				pid->stack_index = a;
				break;
			}
		}
	}
}

void BKE_ptcache_id_from_particles(PTCacheID *pid, Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd= psys_get_modifier(ob, psys);

	memset(pid, 0, sizeof(PTCacheID));

	pid->ob= ob;
	pid->data= psys;
	pid->type= PTCACHE_TYPE_PARTICLES;
	pid->stack_index= modifiers_indexInObject(ob, (ModifierData *)psmd);
	pid->cache= psys->pointcache;
}

void BKE_ptcache_id_from_cloth(PTCacheID *pid, Object *ob, ClothModifierData *clmd)
{
	memset(pid, 0, sizeof(PTCacheID));

	pid->ob= ob;
	pid->data= clmd;
	pid->type= PTCACHE_TYPE_CLOTH;
	pid->stack_index= modifiers_indexInObject(ob, (ModifierData *)clmd);
	pid->cache= clmd->point_cache;
}

void BKE_ptcache_ids_from_object(ListBase *lb, Object *ob)
{
	PTCacheID *pid;
	ParticleSystem *psys;
	ModifierData *md;

	lb->first= lb->last= NULL;

	if(ob->soft) {
		pid= MEM_callocN(sizeof(PTCacheID), "PTCacheID");
		BKE_ptcache_id_from_softbody(pid, ob, ob->soft);
		BLI_addtail(lb, pid);
	}

	for(psys=ob->particlesystem.first; psys; psys=psys->next) {
		pid= MEM_callocN(sizeof(PTCacheID), "PTCacheID");
		BKE_ptcache_id_from_particles(pid, ob, psys);
		BLI_addtail(lb, pid);

		if(psys->soft) {
			pid= MEM_callocN(sizeof(PTCacheID), "PTCacheID");
			BKE_ptcache_id_from_softbody(pid, ob, psys->soft);
			BLI_addtail(lb, pid);
		}
	}

	for(md=ob->modifiers.first; md; md=md->next) {
		if(md->type == eModifierType_Cloth) {
			pid= MEM_callocN(sizeof(PTCacheID), "PTCacheID");
			BKE_ptcache_id_from_cloth(pid, ob, (ClothModifierData*)md);
			BLI_addtail(lb, pid);
		}
	}
}

/*	Takes an Object ID and returns a unique name
	- id: object id
	- cfra: frame for the cache, can be negative
	- stack_index: index in the modifier stack. we can have cache for more then one stack_index
*/

static int ptcache_path(PTCacheID *pid, char *filename)
{
	Library *lib;
	int i;

	lib= (pid)? pid->ob->id.lib: NULL;

	if (G.relbase_valid || lib) {
		char file[FILE_MAX]; /* we dont want the dir, only the file */
		char *blendfilename;

		blendfilename= (lib)? lib->filename: G.sce;

		BLI_split_dirfile_basic(blendfilename, NULL, file);
		i = strlen(file);
		
		/* remove .blend */
		if (i > 6)
			file[i-6] = '\0';
		
		sprintf(filename, "//"PTCACHE_PATH"%s", file); /* add blend file name to pointcache dir */
		BLI_add_slash(filename);
		BLI_convertstringcode(filename, blendfilename);
		return strlen(filename);
	}
	
	/* use the temp path. this is weak but better then not using point cache at all */
	/* btempdir is assumed to exist and ALWAYS has a trailing slash */
	sprintf(filename, "%s"PTCACHE_PATH"%d", btempdir, abs(getpid()));
	BLI_add_slash(filename);
	return strlen(filename);
}

static int BKE_ptcache_id_filename(PTCacheID *pid, char *filename, int cfra, short do_path, short do_ext)
{
	int len=0;
	char *idname;
	char *newname;
	filename[0] = '\0';
	newname = filename;
	
	/*if (!G.relbase_valid) return 0; *//* save blend file before using pointcache */
	
	/* start with temp dir */
	if (do_path) {
		len = ptcache_path(pid, filename);
		newname += len;
	}
	idname = (pid->ob->id.name+2);
	/* convert chars to hex so they are always a valid filename */
	while('\0' != *idname) {
		sprintf(newname, "%02X", (char)(*idname++));
		newname+=2;
		len += 2;
	}
	
	if (do_ext) {
		sprintf(newname, "_%06d_%02d"PTCACHE_EXT, cfra, pid->stack_index); /* always 6 chars */
		len += 16;
	}
	
	return len; /* make sure the above string is always 16 chars */
}

/* youll need to close yourself after! */
PTCacheFile *BKE_ptcache_file_open(PTCacheID *pid, int mode, int cfra)
{
	PTCacheFile *pf;
	FILE *fp = NULL;
	char filename[(FILE_MAXDIR+FILE_MAXFILE)*2];

	/* don't allow writing for linked objects */
	if(pid->ob->id.lib && mode == PTCACHE_FILE_WRITE)
		return NULL;

	/*if (!G.relbase_valid) return NULL; *//* save blend file before using pointcache */
	
	BKE_ptcache_id_filename(pid, filename, cfra, 1, 1);

	if (mode==PTCACHE_FILE_READ) {
		if (!BLI_exists(filename)) {
			return NULL;
		}
 		fp = fopen(filename, "rb");
	} else if (mode==PTCACHE_FILE_WRITE) {
		BLI_make_existing_file(filename); /* will create the dir if needs be, same as //textures is created */
		fp = fopen(filename, "wb");
	}

 	if (!fp)
 		return NULL;
	
	pf= MEM_mallocN(sizeof(PTCacheFile), "PTCacheFile");
	pf->fp= fp;
 	
 	return pf;
}

void BKE_ptcache_file_close(PTCacheFile *pf)
{
	fclose(pf->fp);
	MEM_freeN(pf);
}

int BKE_ptcache_file_read_floats(PTCacheFile *pf, float *f, int tot)
{
	return (fread(f, sizeof(float), tot, pf->fp) == tot);
}

int BKE_ptcache_file_write_floats(PTCacheFile *pf, float *f, int tot)
{
	return (fwrite(f, sizeof(float), tot, pf->fp) == tot);
}

/* youll need to close yourself after!
 * mode - PTCACHE_CLEAR_ALL, 

*/

void BKE_ptcache_id_clear(PTCacheID *pid, int mode, int cfra)
{
	int len; /* store the length of the string */

	/* mode is same as fopen's modes */
	DIR *dir; 
	struct dirent *de;
	char path[FILE_MAX];
	char filename[(FILE_MAXDIR+FILE_MAXFILE)*2];
	char path_full[(FILE_MAXDIR+FILE_MAXFILE)*2];

	if(!pid->cache)
		return;

	/* don't allow clearing for linked objects */
	if(pid->ob->id.lib)
		return;

	/*if (!G.relbase_valid) return; *//* save blend file before using pointcache */
	
	/* clear all files in the temp dir with the prefix of the ID and the ".bphys" suffix */
	switch (mode) {
	case PTCACHE_CLEAR_ALL:
	case PTCACHE_CLEAR_BEFORE:	
	case PTCACHE_CLEAR_AFTER:
		ptcache_path(pid, path);
		
		len = BKE_ptcache_id_filename(pid, filename, cfra, 0, 0); /* no path */
		
		dir = opendir(path);
		if (dir==NULL)
			return;
		
		while ((de = readdir(dir)) != NULL) {
			if (strstr(de->d_name, PTCACHE_EXT)) { /* do we have the right extension?*/
				if (strncmp(filename, de->d_name, len ) == 0) { /* do we have the right prefix */
					if (mode == PTCACHE_CLEAR_ALL) {
						BLI_join_dirfile(path_full, path, de->d_name);
						BLI_delete(path_full, 0, 0);
					} else {
						/* read the number of the file */
						int frame, len2 = strlen(de->d_name);
						char num[7];
						if (len2 > 15) { /* could crash if trying to copy a string out of this range*/
							strncpy(num, de->d_name + (strlen(de->d_name) - 15), 6);
							frame = atoi(num);
							
							if((mode==PTCACHE_CLEAR_BEFORE && frame < cfra)	|| 
							   (mode==PTCACHE_CLEAR_AFTER && frame > cfra)	) {
								
								BLI_join_dirfile(path_full, path, de->d_name);
								BLI_delete(path_full, 0, 0);
							}
						}
					}
				}
			}
		}
		closedir(dir);
		break;
		
	case PTCACHE_CLEAR_FRAME:
		len = BKE_ptcache_id_filename(pid, filename, cfra, 1, 1); /* no path */
		BLI_delete(filename, 0, 0);
		break;
	}
}

int BKE_ptcache_id_exist(PTCacheID *pid, int cfra)
{
	char filename[(FILE_MAXDIR+FILE_MAXFILE)*2];

	if(!pid->cache)
		return 0;
	
	BKE_ptcache_id_filename(pid, filename, cfra, 1, 1);

	return BLI_exists(filename);
}

void BKE_ptcache_id_time(PTCacheID *pid, float cfra, int *startframe, int *endframe, float *timescale)
{
	Object *ob;
	PointCache *cache;
	float offset, time, nexttime;

	/* time handling for point cache:
	 * - simulation time is scaled by result of bsystem_time
	 * - for offsetting time only time offset is taken into account, since
	 *   that's always the same and can't be animated. a timeoffset which
	 *   varies over time is not simpe to support.
	 * - field and motion blur offsets are currently ignored, proper solution
	 *   is probably to interpolate results from two frames for that ..
	 */

	ob= pid->ob;
	cache= pid->cache;

	if(timescale) {
		time= bsystem_time(ob, cfra, 0.0f);
		nexttime= bsystem_time(ob, cfra+1.0f, 0.0f);

		*timescale= MAX2(nexttime - time, 0.0f);
	}

	if(startframe && endframe) {
		*startframe= cache->startframe;
		*endframe= cache->endframe;

		if ((ob->ipoflag & OB_OFFS_PARENT) && (ob->partype & PARSLOW)==0) {
			offset= give_timeoffset(ob);

			*startframe += (int)(offset+0.5f);
			*endframe += (int)(offset+0.5f);
		}
	}
}

int BKE_ptcache_id_reset(PTCacheID *pid, int mode)
{
	PointCache *cache;
	int reset, clear;

	if(!pid->cache)
		return 0;

	cache= pid->cache;
	reset= 0;
	clear= 0;

	if(mode == PTCACHE_RESET_DEPSGRAPH) {
		if(!(cache->flag & PTCACHE_BAKED) && !BKE_ptcache_get_continue_physics()) {
			reset= 1;
			clear= 1;
		}
		else
			cache->flag |= PTCACHE_OUTDATED;
	}
	else if(mode == PTCACHE_RESET_BAKED) {
		if(!BKE_ptcache_get_continue_physics()) {
			reset= 1;
			clear= 1;
		}
		else
			cache->flag |= PTCACHE_OUTDATED;
	}
	else if(mode == PTCACHE_RESET_OUTDATED) {
		reset = 1;

		if(cache->flag & PTCACHE_OUTDATED)
			if(!(cache->flag & PTCACHE_BAKED))
				clear= 1;
	}

	if(reset) {
		cache->flag &= ~(PTCACHE_OUTDATED|PTCACHE_SIMULATION_VALID);
		cache->simframe= 0;

		if(pid->type == PTCACHE_TYPE_CLOTH)
			cloth_free_modifier(pid->ob, pid->data);
		else if(pid->type == PTCACHE_TYPE_SOFTBODY)
			sbFreeSimulation(pid->data);
		else if(pid->type == PTCACHE_TYPE_PARTICLES)
			psys_reset(pid->data, PSYS_RESET_DEPSGRAPH);
	}
	if(clear)
		BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);

	return (reset || clear);
}

int BKE_ptcache_object_reset(Object *ob, int mode)
{
	PTCacheID pid;
	ParticleSystem *psys;
	ModifierData *md;
	int reset;

	reset= 0;

	if(ob->soft) {
		BKE_ptcache_id_from_softbody(&pid, ob, ob->soft);
		reset |= BKE_ptcache_id_reset(&pid, mode);
	}

	for(psys=ob->particlesystem.first; psys; psys=psys->next) {
		BKE_ptcache_id_from_particles(&pid, ob, psys);
		reset |= BKE_ptcache_id_reset(&pid, mode);

		if(psys->soft) {
			BKE_ptcache_id_from_softbody(&pid, ob, psys->soft);
			reset |= BKE_ptcache_id_reset(&pid, mode);
		}
	}

	for(md=ob->modifiers.first; md; md=md->next) {
		if(md->type == eModifierType_Cloth) {
			BKE_ptcache_id_from_cloth(&pid, ob, (ClothModifierData*)md);
			reset |= BKE_ptcache_id_reset(&pid, mode);
		}
	}

	return reset;
}

/* Use this when quitting blender, with unsaved files */
void BKE_ptcache_remove(void)
{
	char path[FILE_MAX];
	char path_full[FILE_MAX];
	int rmdir = 1;
	
	ptcache_path(NULL, path);

	if (BLI_exist(path)) {
		/* The pointcache dir exists? - remove all pointcache */

		DIR *dir; 
		struct dirent *de;

		dir = opendir(path);
		if (dir==NULL)
			return;
		
		while ((de = readdir(dir)) != NULL) {
			if( strcmp(de->d_name, ".")==0 || strcmp(de->d_name, "..")==0) {
				/* do nothing */
			} else if (strstr(de->d_name, PTCACHE_EXT)) { /* do we have the right extension?*/
				BLI_join_dirfile(path_full, path, de->d_name);
				BLI_delete(path_full, 0, 0);
			} else {
				rmdir = 0; /* unknown file, dont remove the dir */
			}
		}

		closedir(dir);
	} else { 
		rmdir = 0; /* path dosnt exist  */
	}
	
	if (rmdir) {
		BLI_delete(path, 1, 0);
	}
}

/* Continuous Interaction */

static int CONTINUE_PHYSICS = 0;

void BKE_ptcache_set_continue_physics(int enable)
{
	Object *ob;

	if(CONTINUE_PHYSICS != enable) {
		CONTINUE_PHYSICS = enable;

		if(CONTINUE_PHYSICS == 0) {
			for(ob=G.main->object.first; ob; ob=ob->id.next)
				if(BKE_ptcache_object_reset(ob, PTCACHE_RESET_OUTDATED))
					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		}
	}
}

int BKE_ptcache_get_continue_physics()
{
	return CONTINUE_PHYSICS;
}

/* Point Cache */

PointCache *BKE_ptcache_add()
{
	PointCache *cache;

	cache= MEM_callocN(sizeof(PointCache), "PointCache");
	cache->startframe= 1;
	cache->endframe= 250;

	return cache;
}

void BKE_ptcache_free(PointCache *cache)
{
	MEM_freeN(cache);
}

PointCache *BKE_ptcache_copy(PointCache *cache)
{
	PointCache *ncache;

	ncache= MEM_dupallocN(cache);

	ncache->flag= 0;
	ncache->simframe= 0;

	return ncache;
}

