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
#include "BKE_scene.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"

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

#ifdef _WIN32
#ifndef snprintf
#define snprintf _snprintf
#endif
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

#define MAX_PTCACHE_PATH FILE_MAX
#define MAX_PTCACHE_FILE ((FILE_MAXDIR+FILE_MAXFILE)*2)

static int ptcache_path(PTCacheID *pid, char *filename)
{
	Library *lib;
	int i;

	lib= (pid)? pid->ob->id.lib: NULL;

	if(pid->cache->flag & PTCACHE_EXTERNAL) {
		strcpy(filename, pid->cache->path);
		return BLI_add_slash(filename); /* new strlen() */
	}
	else if (G.relbase_valid || lib) {
		char file[MAX_PTCACHE_PATH]; /* we dont want the dir, only the file */
		char *blendfilename;

		blendfilename= (lib)? lib->filename: G.sce;

		BLI_split_dirfile_basic(blendfilename, NULL, file);
		i = strlen(file);
		
		/* remove .blend */
		if (i > 6)
			file[i-6] = '\0';
		
		snprintf(filename, MAX_PTCACHE_PATH, "//"PTCACHE_PATH"%s", file); /* add blend file name to pointcache dir */
		BLI_convertstringcode(filename, blendfilename);
		return BLI_add_slash(filename); /* new strlen() */
	}
	
	/* use the temp path. this is weak but better then not using point cache at all */
	/* btempdir is assumed to exist and ALWAYS has a trailing slash */
	snprintf(filename, MAX_PTCACHE_PATH, "%s"PTCACHE_PATH"%d", btempdir, abs(getpid()));
	
	return BLI_add_slash(filename); /* new strlen() */
}

static int BKE_ptcache_id_filename(PTCacheID *pid, char *filename, int cfra, short do_path, short do_ext)
{
	int len=0;
	char *idname;
	char *newname;
	filename[0] = '\0';
	newname = filename;
	
	if (!G.relbase_valid && (pid->cache->flag & PTCACHE_EXTERNAL)==0) return 0; /* save blend file before using disk pointcache */
	
	/* start with temp dir */
	if (do_path) {
		len = ptcache_path(pid, filename);
		newname += len;
	}
	if(strcmp(pid->cache->name, "")==0 && (pid->cache->flag & PTCACHE_EXTERNAL)==0) {
		idname = (pid->ob->id.name+2);
		/* convert chars to hex so they are always a valid filename */
		while('\0' != *idname) {
			snprintf(newname, MAX_PTCACHE_FILE, "%02X", (char)(*idname++));
			newname+=2;
			len += 2;
		}
	}
	else {
		int temp = strlen(pid->cache->name); 
		strcpy(newname, pid->cache->name); 
		newname+=temp;
		len += temp;
	}

	if (do_ext) {
		if(pid->cache->flag & PTCACHE_EXTERNAL) {
			if(pid->cache->index >= 0)
				snprintf(newname, MAX_PTCACHE_FILE, "_%06d_%02d"PTCACHE_EXT, cfra, pid->stack_index); /* always 6 chars */
			else
				snprintf(newname, MAX_PTCACHE_FILE, "_%06d"PTCACHE_EXT, cfra); /* always 6 chars */
		}
		else {
			snprintf(newname, MAX_PTCACHE_FILE, "_%06d_%02d"PTCACHE_EXT, cfra, pid->stack_index); /* always 6 chars */
		}
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

	if (!G.relbase_valid && (pid->cache->flag & PTCACHE_EXTERNAL)==0) return NULL; /* save blend file before using disk pointcache */
	
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

static int ptcache_pid_elemsize(PTCacheID *pid)
{
	if(pid->type==PTCACHE_TYPE_SOFTBODY)
		return 6 * sizeof(float);
	else if(pid->type==PTCACHE_TYPE_PARTICLES)
		return sizeof(ParticleKey);
	else if(pid->type==PTCACHE_TYPE_CLOTH)
		return 9 * sizeof(float);

	return 0;
}
static int ptcache_pid_totelem(PTCacheID *pid)
{
	if(pid->type==PTCACHE_TYPE_SOFTBODY) {
		SoftBody *soft = pid->data;
		return soft->totpoint;
	}
	else if(pid->type==PTCACHE_TYPE_PARTICLES) {
		ParticleSystem *psys = pid->data;
		return psys->totpart;
	}
	else if(pid->type==PTCACHE_TYPE_CLOTH) {
		ClothModifierData *clmd = pid->data;
		return clmd->clothObject->numverts;
	}

	return 0;
}

void BKE_ptcache_update_info(PTCacheID *pid)
{
	PointCache *cache = pid->cache;
	int totframes = 0;
	char mem_info[64];

	if(cache->flag & PTCACHE_EXTERNAL) {
		int cfra = cache->startframe;

		for(; cfra<=cache->endframe; cfra++) {
			if(BKE_ptcache_id_exist(pid, cfra))
				totframes++;
		}

		if(totframes)
			sprintf(cache->info, "%i points read for %i frames", cache->totpoint, totframes);
		else
			sprintf(cache->info, "No valid data to read!");
		return;
	}

	if(cache->flag & PTCACHE_DISK_CACHE) {
		int cfra = cache->startframe;

		for(; cfra<=cache->endframe; cfra++) {
			if(BKE_ptcache_id_exist(pid, cfra))
				totframes++;
		}

		sprintf(mem_info, "%i frames on disk", totframes);
	}
	else {
		PTCacheMem *pm = cache->mem_cache.first;		
		float framesize = 0.0f, bytes = 0.0f;
		int mb;

		if(pm)
			framesize = (float)ptcache_pid_elemsize(pid) * (float)pm->totpoint;
		
		for(; pm; pm=pm->next)
			totframes++;

		bytes = totframes * framesize;

		mb = (bytes > 1024.0f * 1024.0f);

		sprintf(mem_info, "%i frames in memory (%.1f %s)",
			totframes,
			bytes / (mb ? 1024.0f * 1024.0f : 1024.0f),
			mb ? "Mb" : "kb");
	}

	if(cache->flag & PTCACHE_OUTDATED) {
		sprintf(cache->info, "%s, cache is outdated!", mem_info);
	}
	else if(cache->flag & PTCACHE_FRAMES_SKIPPED) {
		sprintf(cache->info, "%s, not exact since frame %i.", mem_info, cache->last_exact);
	}
	else
		sprintf(cache->info, "%s.", mem_info);
}
/* reads cache from disk or memory */
/* possible to get old or interpolated result */
int BKE_ptcache_read_cache(PTCacheReader *reader)
{
	PTCacheID *pid = reader->pid;
	PTCacheFile *pf=NULL, *pf2=NULL;
	PTCacheMem *pm=NULL, *pm2=NULL;
	int totelem = reader->totelem;
	float cfra = reader->cfra;
	int cfrai = (int)cfra;
	int elemsize = ptcache_pid_elemsize(pid);
	int i, incr = elemsize / sizeof(float);
	float frs_sec = reader->scene->r.frs_sec;
	int cfra1=0, cfra2;
	int ret = 0;

	if(totelem == 0)
		return 0;


	/* first check if we have the actual frame cached */
	if(cfra == (float)cfrai) {
		if(pid->cache->flag & PTCACHE_DISK_CACHE) {
			pf= BKE_ptcache_file_open(pid, PTCACHE_FILE_READ, cfrai);
		}
		else {
			pm = pid->cache->mem_cache.first;

			for(; pm; pm=pm->next) {
				if(pm->frame == cfrai)
					break;
			}
		}
	}

	/* if found, use exact frame */
	if(pf || pm) {
		float *data;

		if(pm)
			data = pm->data;
		else
			data = MEM_callocN(elemsize, "pointcache read data");

		for(i=0; i<totelem; i++) {
			if(pf) {
				if(!BKE_ptcache_file_read_floats(pf, data, incr)) {
					BKE_ptcache_file_close(pf);
					MEM_freeN(data);
					return 0;
				}

				reader->set_elem(i, reader->calldata, data);
			}
			else {
				reader->set_elem(i, reader->calldata, data);
				data += incr;
			}
		}

		if(pf) {
			BKE_ptcache_file_close(pf);
			pf = NULL;
			MEM_freeN(data);
		}

		ret = PTCACHE_READ_EXACT;
	}

	if(ret)
		;
	/* no exact cache frame found so try to find cached frames around cfra */
	else if(pid->cache->flag & PTCACHE_DISK_CACHE) {
		pf=NULL;
		while(cfrai > pid->cache->startframe && !pf) {
			cfrai--;
			pf= BKE_ptcache_file_open(pid, PTCACHE_FILE_READ, cfrai);
			cfra1 = cfrai;
		}

		if(reader->old_frame)
			*(reader->old_frame) = cfrai;

		cfrai = (int)cfra;
		while(cfrai < pid->cache->endframe && !pf2) {
			cfrai++;
			pf2= BKE_ptcache_file_open(pid, PTCACHE_FILE_READ, cfrai);
			cfra2 = cfrai;
		}
	}
	else if(pid->cache->mem_cache.first){
		pm = pid->cache->mem_cache.first;

		while(pm->next && pm->next->frame < cfra)
			pm= pm->next;

		if(pm) {
			if(reader->old_frame)
				*(reader->old_frame) = pm->frame;
			cfra1 = pm->frame;
		}

		pm2 = pid->cache->mem_cache.last;

		if(pm2 && pm2->frame < cfra)
			pm2 = NULL;
		else {
			while(pm2->prev && pm2->prev->frame > cfra)
				pm2= pm2->prev;

			if(pm2)
				cfra2 = pm2->frame;
		}
	}

	if(ret)
		;
	else if((pf && pf2) || (pm && pm2)) {
		/* interpolate from nearest frames if cache isn't outdated */
		float *data1, *data2;

		if(pm) {
			data1 = pm->data;
			data2 = pm2->data;
		}
		else {
			data1 = MEM_callocN(elemsize, "pointcache read data1");
			data2 = MEM_callocN(elemsize, "pointcache read data2");
		}

		for(i=0; i<totelem; i++) {
			if(pf && pf2) {
				if(!BKE_ptcache_file_read_floats(pf, data1, incr)) {
					BKE_ptcache_file_close(pf);
					BKE_ptcache_file_close(pf2);
					MEM_freeN(data1);
					MEM_freeN(data2);
					return 0;
				}
				if(!BKE_ptcache_file_read_floats(pf2, data2, incr)) {
					BKE_ptcache_file_close(pf);
					BKE_ptcache_file_close(pf2);
					MEM_freeN(data1);
					MEM_freeN(data2);
					return 0;
				}
				reader->interpolate_elem(i, reader->calldata, frs_sec, cfra, (float)cfra1, (float)cfra2, data1, data2);
			}
			else {
				reader->interpolate_elem(i, reader->calldata, frs_sec, cfra, (float)cfra1, (float)cfra2, data1, data2);
				data1 += incr;
				data2 += incr;
			}
		}

		if(pf) {
			BKE_ptcache_file_close(pf);
			pf = NULL;
			BKE_ptcache_file_close(pf2);
			pf2 = NULL;
			MEM_freeN(data1);
			MEM_freeN(data2);
		}

		ret = PTCACHE_READ_INTERPOLATED;
	}
	else if(pf || pm) {
		/* use last valid cache frame */
		float *data;

		/* don't read cache if allready simulated past cached frame */
		if(cfra1 && cfra1 <= pid->cache->simframe) {
			if(pf)
				BKE_ptcache_file_close(pf);
			if(pf2)
				BKE_ptcache_file_close(pf2);

			return 0;
		}

		if(pm)
			data = pm->data;
		else
			data = MEM_callocN(elemsize, "pointcache read data");

		for(i=0; i<totelem; i++) {
			if(pf) {
				if(!BKE_ptcache_file_read_floats(pf, data, incr)) {
					BKE_ptcache_file_close(pf);
					if(pf2)
						BKE_ptcache_file_close(pf2);
					return 0;
				}
				reader->set_elem(i, reader->calldata, data);
			}
			else {
				reader->set_elem(i, reader->calldata, data);
				data += incr;
			}
		}

		if(pf) {
			BKE_ptcache_file_close(pf);
			pf = NULL;
			MEM_freeN(data);
		}
		if(pf2) {
			BKE_ptcache_file_close(pf2);
			pf = NULL;
		}

		ret = PTCACHE_READ_OLD;
	}

	if(pf)
		BKE_ptcache_file_close(pf);
	if(pf2)
		BKE_ptcache_file_close(pf2);

	if((pid->cache->flag & PTCACHE_QUICK_CACHE)==0) {
		/* clear invalid cache frames so that better stuff can be simulated */
		if(pid->cache->flag & PTCACHE_OUTDATED) {
			BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_AFTER, cfra);
		}
		else if(pid->cache->flag & PTCACHE_FRAMES_SKIPPED) {
			if(cfra <= pid->cache->last_exact)
				pid->cache->flag &= ~PTCACHE_FRAMES_SKIPPED;

			BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_AFTER, MAX2(cfra,pid->cache->last_exact));
		}
	}

	return ret;
}
/* writes cache to disk or memory */
int BKE_ptcache_write_cache(PTCacheWriter *writer)
{
	PointCache *cache = writer->pid->cache;
	PTCacheFile *pf= NULL;
	int elemsize = ptcache_pid_elemsize(writer->pid);
	int i, incr = elemsize / sizeof(float);
	int add = 0, overwrite = 0;
	float temp[14];

	if(writer->totelem == 0 || writer->cfra <= 0)
		return 0;

	if(cache->flag & PTCACHE_DISK_CACHE) {
		int cfra = cache->endframe;

		/* allways start from scratch on the first frame */
		if(writer->cfra == cache->startframe) {
			BKE_ptcache_id_clear(writer->pid, PTCACHE_CLEAR_ALL, writer->cfra);
			cache->flag &= ~PTCACHE_REDO_NEEDED;
			add = 1;
		}
		else {
			int ocfra;
			/* find last cached frame */
			while(cfra > cache->startframe && !BKE_ptcache_id_exist(writer->pid, cfra))
				cfra--;

			/* find second last cached frame */
			ocfra = cfra-1;
			while(ocfra > cache->startframe && !BKE_ptcache_id_exist(writer->pid, ocfra))
				ocfra--;

			if(cfra >= cache->startframe && writer->cfra > cfra) {
				if(ocfra >= cache->startframe && cfra - ocfra < cache->step)
					overwrite = 1;
				else
					add = 1;
			}
		}

		if(add || overwrite) {
			if(overwrite)
				BKE_ptcache_id_clear(writer->pid, PTCACHE_CLEAR_FRAME, cfra);

			pf = BKE_ptcache_file_open(writer->pid, PTCACHE_FILE_WRITE, writer->cfra);
			if(!pf)
				return 0;

			for(i=0; i<writer->totelem; i++) {
				writer->set_elem(i, writer->calldata, temp);
				BKE_ptcache_file_write_floats(pf, temp, incr);
			}
		}
	}
	else {
		PTCacheMem *pm;
		PTCacheMem *pm2;
		float *pmdata;

		pm2 = cache->mem_cache.first;
		
		/* allways start from scratch on the first frame */
		if(writer->cfra == cache->startframe) {
			BKE_ptcache_id_clear(writer->pid, PTCACHE_CLEAR_ALL, writer->cfra);
			cache->flag &= ~PTCACHE_REDO_NEEDED;
			add = 1;
		}
		else {
			pm2 = cache->mem_cache.last;

			if(pm2 && writer->cfra > pm2->frame) {
				if(pm2->prev && pm2->frame - pm2->prev->frame < cache->step)
					overwrite = 1;
				else
					add = 1;
			}
		}

		if(overwrite) {
			pm = cache->mem_cache.last;
			pmdata = pm->data;

			for(i=0; i<writer->totelem; i++, pmdata+=incr) {
				writer->set_elem(i, writer->calldata, temp);
				memcpy(pmdata, temp, elemsize);
			}

			pm->frame = writer->cfra;
		}
		else if(add) {
			pm = MEM_callocN(sizeof(PTCacheMem), "Pointcache mem");
			pm->data = MEM_callocN(elemsize * writer->totelem, "Pointcache mem data");
			pmdata = pm->data;

			for(i=0; i<writer->totelem; i++, pmdata+=incr) {
				writer->set_elem(i, writer->calldata, temp);
				memcpy(pmdata, temp, elemsize);
			}

			pm->frame = writer->cfra;
			pm->totpoint = writer->totelem;

			BLI_addtail(&cache->mem_cache, pm);
		}
	}

	if(add || overwrite) {
		if(writer->cfra - cache->last_exact == 1
			|| writer->cfra == cache->startframe) {
			cache->last_exact = writer->cfra;
			cache->flag &= ~PTCACHE_FRAMES_SKIPPED;
		}
		else
			cache->flag |= PTCACHE_FRAMES_SKIPPED;
	}
	
	if(pf)
		BKE_ptcache_file_close(pf);

	BKE_ptcache_update_info(writer->pid);

	return 1;
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
	char path[MAX_PTCACHE_PATH];
	char filename[MAX_PTCACHE_FILE];
	char path_full[MAX_PTCACHE_FILE];
	char ext[MAX_PTCACHE_PATH];

	if(!pid->cache || pid->cache->flag & PTCACHE_BAKED)
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
		if(pid->cache->flag & PTCACHE_DISK_CACHE) {
			ptcache_path(pid, path);
			
			len = BKE_ptcache_id_filename(pid, filename, cfra, 0, 0); /* no path */
			
			dir = opendir(path);
			if (dir==NULL)
				return;

			snprintf(ext, sizeof(ext), "_%02d"PTCACHE_EXT, pid->stack_index);
			
			while ((de = readdir(dir)) != NULL) {
				if (strstr(de->d_name, ext)) { /* do we have the right extension?*/
					if (strncmp(filename, de->d_name, len ) == 0) { /* do we have the right prefix */
						if (mode == PTCACHE_CLEAR_ALL) {
							pid->cache->last_exact = 0;
							BLI_join_dirfile(path_full, path, de->d_name);
							BLI_delete(path_full, 0, 0);
						} else {
							/* read the number of the file */
							int frame, len2 = strlen(de->d_name);
							char num[7];

							if (len2 > 15) { /* could crash if trying to copy a string out of this range*/
								BLI_strncpy(num, de->d_name + (strlen(de->d_name) - 15), sizeof(num));
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
		}
		else {
			PTCacheMem *pm= pid->cache->mem_cache.first;
			PTCacheMem *link= NULL;

			if(mode == PTCACHE_CLEAR_ALL) {
				pid->cache->last_exact = 0;
				for(; pm; pm=pm->next)
					MEM_freeN(pm->data);
				BLI_freelistN(&pid->cache->mem_cache);
			} else {
				while(pm) {
					if((mode==PTCACHE_CLEAR_BEFORE && pm->frame < cfra)	|| 
					(mode==PTCACHE_CLEAR_AFTER && pm->frame > cfra)	) {
						link = pm;
						pm = pm->next;
						MEM_freeN(link->data);
						BLI_freelinkN(&pid->cache->mem_cache, link);
					}
					else
						pm = pm->next;
				}
			}
		}
		break;
		
	case PTCACHE_CLEAR_FRAME:
		if(pid->cache->flag & PTCACHE_DISK_CACHE) {
			if(BKE_ptcache_id_exist(pid, cfra)) {
				BKE_ptcache_id_filename(pid, filename, cfra, 1, 1); /* no path */
				BLI_delete(filename, 0, 0);
			}
		}
		else {
			PTCacheMem *pm = pid->cache->mem_cache.first;

			for(; pm; pm=pm->next) {
				if(pm->frame == cfra) {
					MEM_freeN(pm->data);
					BLI_freelinkN(&pid->cache->mem_cache, pm);
					break;
				}
			}
		}
		break;
	}

	BKE_ptcache_update_info(pid);
}

int BKE_ptcache_id_exist(PTCacheID *pid, int cfra)
{
	if(!pid->cache)
		return 0;
	
	if(pid->cache->flag & PTCACHE_DISK_CACHE) {
		char filename[MAX_PTCACHE_FILE];
		
		BKE_ptcache_id_filename(pid, filename, cfra, 1, 1);

		return BLI_exists(filename);
	}
	else {
		PTCacheMem *pm = pid->cache->mem_cache.first;

		for(; pm; pm=pm->next) {
			if(pm->frame==cfra)
				return 1;
		}
		return 0;
	}
}

void BKE_ptcache_id_time(PTCacheID *pid, Scene *scene, float cfra, int *startframe, int *endframe, float *timescale)
{
	Object *ob;
	PointCache *cache;
	float offset, time, nexttime;

	/* TODO: this has to be sorter out once bsystem_time gets redone, */
	/*       now caches can handle interpolating etc. too - jahka */

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
		time= bsystem_time(scene, ob, cfra, 0.0f);
		nexttime= bsystem_time(scene, ob, cfra+1.0f, 0.0f);

		*timescale= MAX2(nexttime - time, 0.0f);
	}

	if(startframe && endframe) {
		*startframe= cache->startframe;
		*endframe= cache->endframe;

		// XXX ipoflag is depreceated - old animation system stuff
		if (/*(ob->ipoflag & OB_OFFS_PARENT) &&*/ (ob->partype & PARSLOW)==0) {
			offset= give_timeoffset(ob);

			*startframe += (int)(offset+0.5f);
			*endframe += (int)(offset+0.5f);
		}
	}
}

int BKE_ptcache_id_reset(Scene *scene, PTCacheID *pid, int mode)
{
	PointCache *cache;
	int reset, clear, after;

	if(!pid->cache)
		return 0;

	cache= pid->cache;
	reset= 0;
	clear= 0;
	after= 0;

	if(mode == PTCACHE_RESET_DEPSGRAPH) {
		if(!(cache->flag & PTCACHE_BAKED) && !BKE_ptcache_get_continue_physics()) {
			if(cache->flag & PTCACHE_QUICK_CACHE)
				clear= 1;

			after= 1;
		}

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
		cache->flag &= ~(PTCACHE_REDO_NEEDED|PTCACHE_SIMULATION_VALID);
		cache->simframe= 0;
		cache->last_exact= 0;

		if(pid->type == PTCACHE_TYPE_CLOTH)
			cloth_free_modifier(pid->ob, pid->data);
		else if(pid->type == PTCACHE_TYPE_SOFTBODY)
			sbFreeSimulation(pid->data);
		else if(pid->type == PTCACHE_TYPE_PARTICLES)
			psys_reset(pid->data, PSYS_RESET_DEPSGRAPH);
	}
	if(clear)
		BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);
	else if(after)
		BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_AFTER, CFRA);

	return (reset || clear || after);
}

int BKE_ptcache_object_reset(Scene *scene, Object *ob, int mode)
{
	PTCacheID pid;
	ParticleSystem *psys;
	ModifierData *md;
	int reset, skip;

	reset= 0;
	skip= 0;

	if(ob->soft) {
		BKE_ptcache_id_from_softbody(&pid, ob, ob->soft);
		reset |= BKE_ptcache_id_reset(scene, &pid, mode);
	}

	for(psys=ob->particlesystem.first; psys; psys=psys->next) {
		/* Baked softbody hair has to be checked first, because we don't want to reset */
		/* particles or softbody in that case -jahka */
		if(psys->soft) {
			BKE_ptcache_id_from_softbody(&pid, ob, psys->soft);
			if(mode == PSYS_RESET_ALL || !(psys->part->type == PART_HAIR && (pid.cache->flag & PTCACHE_BAKED))) 
				reset |= BKE_ptcache_id_reset(scene, &pid, mode);
			else
				skip = 1;
		}
		else if(psys->recalc & PSYS_RECALC_REDO || psys->recalc & PSYS_RECALC_CHILD)
			skip = 1;

		if(skip == 0) {
			BKE_ptcache_id_from_particles(&pid, ob, psys);
			reset |= BKE_ptcache_id_reset(scene, &pid, mode);
		}
	}

	for(md=ob->modifiers.first; md; md=md->next) {
		if(md->type == eModifierType_Cloth) {
			BKE_ptcache_id_from_cloth(&pid, ob, (ClothModifierData*)md);
			reset |= BKE_ptcache_id_reset(scene, &pid, mode);
		}
	}

	return reset;
}

/* Use this when quitting blender, with unsaved files */
void BKE_ptcache_remove(void)
{
	char path[MAX_PTCACHE_PATH];
	char path_full[MAX_PTCACHE_PATH];
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

void BKE_ptcache_set_continue_physics(Scene *scene, int enable)
{
	Object *ob;

	if(CONTINUE_PHYSICS != enable) {
		CONTINUE_PHYSICS = enable;

		if(CONTINUE_PHYSICS == 0) {
			for(ob=G.main->object.first; ob; ob=ob->id.next)
				if(BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_OUTDATED))
					DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
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
	cache->step= 10;

	return cache;
}

void BKE_ptcache_free(PointCache *cache)
{
	PTCacheMem *pm = cache->mem_cache.first;
	if(pm) {
		for(; pm; pm=pm->next)
			MEM_freeN(pm->data);

		BLI_freelistN(&cache->mem_cache);
	}

	MEM_freeN(cache);
}

PointCache *BKE_ptcache_copy(PointCache *cache)
{
	PointCache *ncache;

	ncache= MEM_dupallocN(cache);

	/* hmm, should these be copied over instead? */
	ncache->mem_cache.first = NULL;
	ncache->mem_cache.last = NULL;

	ncache->flag= 0;
	ncache->simframe= 0;

	return ncache;
}



/* Baking */
static int count_quick_cache(Scene *scene, int *quick_step)
{
	Base *base = scene->base.first;
	PTCacheID *pid;
	ListBase pidlist;
	int autocache_count= 0;

	for(base = scene->base.first; base; base = base->next) {
		if(base->object) {
			BKE_ptcache_ids_from_object(&pidlist, base->object);

			for(pid=pidlist.first; pid; pid=pid->next) {
				if((pid->cache->flag & PTCACHE_BAKED)
					|| (pid->cache->flag & PTCACHE_QUICK_CACHE)==0)
					continue;

				if(pid->cache->flag & PTCACHE_OUTDATED || (pid->cache->flag & PTCACHE_SIMULATION_VALID)==0) {
					if(!autocache_count)
						*quick_step = pid->cache->step;
					else
						*quick_step = MIN2(*quick_step, pid->cache->step);

					autocache_count++;
				}
			}

			BLI_freelistN(&pidlist);
		}
	}

	return autocache_count;
}
void BKE_ptcache_quick_cache_all(Scene *scene)
{
	PTCacheBaker baker;

	baker.bake=0;
	baker.break_data=NULL;
	baker.break_test=NULL;
	baker.pid=NULL;
	baker.progressbar=NULL;
	baker.progresscontext=NULL;
	baker.render=0;
	baker.anim_init = 0;
	baker.scene=scene;

	if(count_quick_cache(scene, &baker.quick_step))
		BKE_ptcache_make_cache(&baker);
}

/* if bake is not given run simulations to current frame */
void BKE_ptcache_make_cache(PTCacheBaker* baker)
{
	Scene *scene = baker->scene;
	Base *base;
	ListBase pidlist;
	PTCacheID *pid = baker->pid;
	PointCache *cache;
	float frameleno = scene->r.framelen;
	int cfrao = CFRA;
	int startframe = MAXFRAME;
	int endframe = baker->anim_init ? scene->r.sfra : CFRA;
	int bake = baker->bake;
	int render = baker->render;
	int step = baker->quick_step;

	G.afbreek = 0;

	/* set caches to baking mode and figure out start frame */
	if(pid) {
		/* cache/bake a single object */
		cache = pid->cache;
		if((cache->flag & PTCACHE_BAKED)==0) {
			if(pid->type==PTCACHE_TYPE_PARTICLES)
				psys_get_pointcache_start_end(scene, pid->data, &cache->startframe, &cache->endframe);

			if(bake || cache->flag & PTCACHE_REDO_NEEDED)
				BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);

			startframe = MAX2(cache->last_exact, cache->startframe);

			if(bake) {
				endframe = cache->endframe;
				cache->flag |= PTCACHE_BAKING;
			}
			else {
				endframe = MIN2(endframe, cache->endframe);
			}

			cache->flag &= ~PTCACHE_BAKED;
		}
	}
	else for(base=scene->base.first; base; base= base->next) {
		/* cache/bake everything in the scene */
		BKE_ptcache_ids_from_object(&pidlist, base->object);

		for(pid=pidlist.first; pid; pid=pid->next) {
			cache = pid->cache;
			if((cache->flag & PTCACHE_BAKED)==0) {
				if(pid->type==PTCACHE_TYPE_PARTICLES) {
					ParticleSystem *psys = (ParticleSystem*)pid->data;
					/* skip hair & keyed particles */
					if(psys->part->type == PART_HAIR || psys->part->phystype == PART_PHYS_KEYED)
						continue;

					psys_get_pointcache_start_end(scene, pid->data, &cache->startframe, &cache->endframe);
				}

				if((cache->flag & PTCACHE_REDO_NEEDED || (cache->flag & PTCACHE_SIMULATION_VALID)==0)
					&& ((cache->flag & PTCACHE_QUICK_CACHE)==0 || render || bake))
					BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);

				startframe = MIN2(startframe, cache->startframe);

				if(bake || render) {
					cache->flag |= PTCACHE_BAKING;

					if(bake)
						endframe = MAX2(endframe, cache->endframe);
				}

				cache->flag &= ~PTCACHE_BAKED;

			}
		}
		BLI_freelistN(&pidlist);
	}

	CFRA= startframe;
	scene->r.framelen = 1.0;

	for(; CFRA <= endframe; CFRA+=step) {
		float prog;

		if(bake)
			prog = (int)(100.0 * (float)(CFRA - startframe)/(float)(endframe-startframe));
		else
			prog = CFRA;

		/* NOTE: baking should not redraw whole ui as this slows things down */
		if(baker->progressbar)
			baker->progressbar(baker->progresscontext, prog);
		
		scene_update_for_newframe(scene, scene->lay);

		/* NOTE: breaking baking should leave calculated frames in cache, not clear it */
		if(baker->break_test && baker->break_test(baker->break_data))
			break;
	}

	/* clear baking flag */
	if(pid) {
		cache->flag &= ~(PTCACHE_BAKING|PTCACHE_REDO_NEEDED);
		cache->flag |= PTCACHE_SIMULATION_VALID;
		if(bake)
			cache->flag |= PTCACHE_BAKED;
	}
	else for(base=scene->base.first; base; base= base->next) {
		BKE_ptcache_ids_from_object(&pidlist, base->object);

		for(pid=pidlist.first; pid; pid=pid->next) {
			/* skip hair particles */
			if(pid->type==PTCACHE_TYPE_PARTICLES && ((ParticleSystem*)pid->data)->part->type == PART_HAIR)
				continue;
		
			cache = pid->cache;

			if(step > 1)
				cache->flag &= ~(PTCACHE_BAKING|PTCACHE_OUTDATED);
			else
				cache->flag &= ~(PTCACHE_BAKING|PTCACHE_REDO_NEEDED);

			cache->flag |= PTCACHE_SIMULATION_VALID;

			if(bake)
				cache->flag |= PTCACHE_BAKED;
		}
		BLI_freelistN(&pidlist);
	}

	scene->r.framelen = frameleno;
	CFRA = cfrao;

	if(bake) /* already on cfra unless baking */
		scene_update_for_newframe(scene, scene->lay);

	/* TODO: call redraw all windows somehow */
}

void BKE_ptcache_toggle_disk_cache(PTCacheID *pid) {
	PointCache *cache = pid->cache;
	PTCacheFile *pf;
	PTCacheMem *pm;
	int totelem=0;
	int float_count=0;
	int tot;
	int last_exact = cache->last_exact;

	if (!G.relbase_valid){
		cache->flag &= ~PTCACHE_DISK_CACHE;
		printf("File must be saved before using disk cache!\n");
		return;
	}

	totelem = ptcache_pid_totelem(pid);
	float_count = ptcache_pid_elemsize(pid) / sizeof(float);

	if(totelem==0 || float_count==0)
		return;

	tot = totelem*float_count;

	/* MEM -> DISK */
	if(cache->flag & PTCACHE_DISK_CACHE) {
		pm = cache->mem_cache.first;

		BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);

		for(; pm; pm=pm->next) {
			pf = BKE_ptcache_file_open(pid, PTCACHE_FILE_WRITE, pm->frame);

			if(pf) {
				if(fwrite(pm->data, sizeof(float), tot, pf->fp) != tot) {
					printf("Error writing to disk cache\n");
					
					cache->flag &= ~PTCACHE_DISK_CACHE;

					BKE_ptcache_file_close(pf);
					return;
				}
				BKE_ptcache_file_close(pf);
			}
			else
				printf("Error creating disk cache file\n");
		}

		cache->flag &= ~PTCACHE_DISK_CACHE;
		BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);
		cache->flag |= PTCACHE_DISK_CACHE;
	}
	/* DISK -> MEM */
	else {
		int cfra;
		int sfra = cache->startframe;
		int efra = cache->endframe;

		BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);

		for(cfra=sfra; cfra <= efra; cfra++) {
			pf = BKE_ptcache_file_open(pid, PTCACHE_FILE_READ, cfra);

			if(pf) {
				pm = MEM_callocN(sizeof(PTCacheMem), "Pointcache mem");
				pm->data = MEM_callocN(sizeof(float)*tot, "Pointcache mem data");

				if(fread(pm->data, sizeof(float), tot, pf->fp)!= tot) {
					printf("Error reading from disk cache\n");

					cache->flag |= PTCACHE_DISK_CACHE;

					MEM_freeN(pm->data);
					MEM_freeN(pm);
					BKE_ptcache_file_close(pf);
					return;
				}

				pm->frame = cfra;
				pm->totpoint = totelem;

				BLI_addtail(&pid->cache->mem_cache, pm);

				BKE_ptcache_file_close(pf);
			}
		}

		cache->flag |= PTCACHE_DISK_CACHE;
		BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_ALL, 0);
		cache->flag &= ~PTCACHE_DISK_CACHE;
	}
	
	cache->last_exact = last_exact;

	BKE_ptcache_update_info(pid);
}

void BKE_ptcache_load_external(PTCacheID *pid)
{
	PointCache *cache = pid->cache;
	int len; /* store the length of the string */

	/* mode is same as fopen's modes */
	DIR *dir; 
	struct dirent *de;
	char path[MAX_PTCACHE_PATH];
	char filename[MAX_PTCACHE_FILE];
	char path_full[MAX_PTCACHE_FILE];
	char ext[MAX_PTCACHE_PATH];

	if(!cache)
		return;

	cache->startframe = MAXFRAME;
	cache->endframe = -1;
	cache->totpoint = 0;

	ptcache_path(pid, path);
	
	len = BKE_ptcache_id_filename(pid, filename, 1, 0, 0); /* no path */
	
	dir = opendir(path);
	if (dir==NULL)
		return;

	if(cache->index >= 0)
		snprintf(ext, sizeof(ext), "_%02d"PTCACHE_EXT, cache->index);
	else
		strcpy(ext, PTCACHE_EXT);
	
	while ((de = readdir(dir)) != NULL) {
		if (strstr(de->d_name, ext)) { /* do we have the right extension?*/
			if (strncmp(filename, de->d_name, len ) == 0) { /* do we have the right prefix */
				/* read the number of the file */
				int frame, len2 = strlen(de->d_name);
				char num[7];

				if (len2 > 15) { /* could crash if trying to copy a string out of this range*/
					BLI_strncpy(num, de->d_name + (strlen(de->d_name) - 15), sizeof(num));
					frame = atoi(num);

					cache->startframe = MIN2(cache->startframe, frame);
					cache->endframe = MAX2(cache->endframe, frame);
				}
			}
		}
	}
	closedir(dir);

	if(cache->startframe != MAXFRAME) {
		PTCacheFile *pf;
		int elemsize = ptcache_pid_elemsize(pid);
		int	incr = elemsize / sizeof(float);
		float *data = NULL;
		pf= BKE_ptcache_file_open(pid, PTCACHE_FILE_READ, cache->startframe);

		if(pf) {
			data = MEM_callocN(elemsize, "pointcache read data");
			while(BKE_ptcache_file_read_floats(pf, data, incr))
				cache->totpoint++;
			
			BKE_ptcache_file_close(pf);
			MEM_freeN(data);
		}
	}

	cache->flag &= ~(PTCACHE_OUTDATED|PTCACHE_FRAMES_SKIPPED);

	BKE_ptcache_update_info(pid);
}
