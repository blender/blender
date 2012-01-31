/*
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Campbell barton, Alex Fraser
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/bpath.c
 *  \ingroup bli
 */

/* TODO,
 * currently there are some cases we dont support.
 * - passing output paths to the visitor?, like render out.
 * - passing sequence strips with many images.
 * - passing directory paths - visitors dont know which path is a dir or a file.
 * */

#include <sys/stat.h>

#include <string.h>
#include <assert.h>

/* path/file handeling stuff */
#ifndef WIN32
  #include <dirent.h>
  #include <unistd.h>
#else
  #include <io.h>
  #include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_fluidsim.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_texture_types.h"
#include "DNA_vfont_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"

#include "BLI_blenlib.h"
#include "BLI_bpath.h"
#include "BLI_utildefines.h"

#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_sequencer.h"
#include "BKE_utildefines.h"
#include "BKE_image.h" /* so we can check the image's type */

static int checkMissingFiles_visit_cb(void *userdata, char *UNUSED(path_dst), const char *path_src)
{
	ReportList *reports= (ReportList *)userdata;

	if (!BLI_exists(path_src)) {
		BKE_reportf(reports, RPT_WARNING, "Path Not Found \"%s\"", path_src);
	}

	return FALSE;
}

/* high level function */
void checkMissingFiles(Main *bmain, ReportList *reports)
{
	bpath_traverse_main(bmain, checkMissingFiles_visit_cb, BPATH_TRAVERSE_ABS, reports);
}

typedef struct BPathRemap_Data
{
	const char *basedir;
	ReportList *reports;

	int count_tot;
	int count_changed;
	int count_failed;
} BPathRemap_Data;

static int makeFilesRelative_visit_cb(void *userdata, char *path_dst, const char *path_src)
{
	BPathRemap_Data *data= (BPathRemap_Data *)userdata;

	data->count_tot++;

	if(strncmp(path_src, "//", 2)==0) {
		return FALSE; /* already relative */
	}
	else {
		strcpy(path_dst, path_src);
		BLI_path_rel(path_dst, data->basedir);
		if (strncmp(path_dst, "//", 2)==0) {
			data->count_changed++;
		}
		else {
			BKE_reportf(data->reports, RPT_WARNING, "Path cant be made relative \"%s\"", path_src);
			data->count_failed++;
		}
		return TRUE;
	}
}

void makeFilesRelative(Main *bmain, const char *basedir, ReportList *reports)
{
	BPathRemap_Data data= {NULL};

	if(basedir[0] == '\0') {
		printf("%s: basedir='', this is a bug\n", __func__);
		return;
	}

	data.basedir= basedir;
	data.reports= reports;

	bpath_traverse_main(bmain, makeFilesRelative_visit_cb, 0, (void *)&data);

	BKE_reportf(reports, data.count_failed ? RPT_WARNING : RPT_INFO,
	            "Total files %d|Changed %d|Failed %d",
	            data.count_tot, data.count_changed, data.count_failed);
}

static int makeFilesAbsolute_visit_cb(void *userdata, char *path_dst, const char *path_src)
{
	BPathRemap_Data *data= (BPathRemap_Data *)userdata;

	data->count_tot++;

	if(strncmp(path_src, "//", 2)!=0) {
		return FALSE; /* already absolute */
	}
	else {
		strcpy(path_dst, path_src);
		BLI_path_abs(path_dst, data->basedir);
		if (strncmp(path_dst, "//", 2)!=0) {
			data->count_changed++;
		}
		else {
			BKE_reportf(data->reports, RPT_WARNING, "Path cant be made absolute \"%s\"", path_src);
			data->count_failed++;
		}
		return TRUE;
	}
}

/* similar to makeFilesRelative - keep in sync! */
void makeFilesAbsolute(Main *bmain, const char *basedir, ReportList *reports)
{
	BPathRemap_Data data= {NULL};

	if(basedir[0] == '\0') {
		printf("%s: basedir='', this is a bug\n", __func__);
		return;
	}

	data.basedir= basedir;
	data.reports= reports;

	bpath_traverse_main(bmain, makeFilesAbsolute_visit_cb, 0, (void *)&data);

	BKE_reportf(reports, data.count_failed ? RPT_WARNING : RPT_INFO,
	            "Total files %d|Changed %d|Failed %d",
	            data.count_tot, data.count_changed, data.count_failed);
}


/* find this file recursively, use the biggest file so thumbnails dont get used by mistake
 * - dir: subdir to search
 * - filename: set this filename
 * - filesize: filesize for the file
 *
 * return found: 1/0.
 */
#define MAX_RECUR 16
static int findFileRecursive(char *filename_new,
                             const char *dirname,
                             const char *filename,
                             int *filesize,
                             int *recur_depth)
{
	/* file searching stuff */
	DIR *dir;
	struct dirent *de;
	struct stat status;
	char path[FILE_MAX];
	int size;
	int found = FALSE;

	filename_new[0] = '\0';

	dir= opendir(dirname);

	if (dir==NULL)
		return found;

	if (*filesize == -1)
		*filesize= 0; /* dir opened fine */

	while ((de= readdir(dir)) != NULL) {

		if (strcmp(".", de->d_name)==0 || strcmp("..", de->d_name)==0)
			continue;

		BLI_join_dirfile(path, sizeof(path), dirname, de->d_name);

		if (stat(path, &status) != 0)
			continue; /* cant stat, dont bother with this file, could print debug info here */

		if (S_ISREG(status.st_mode)) { /* is file */
			if (strncmp(filename, de->d_name, FILE_MAX)==0) { /* name matches */
				/* open the file to read its size */
				size= status.st_size;
				if ((size > 0) && (size > *filesize)) { /* find the biggest file */
					*filesize= size;
					BLI_strncpy(filename_new, path, FILE_MAX);
					found = TRUE;
				}
			}
		}
		else if (S_ISDIR(status.st_mode)) { /* is subdir */
			if (*recur_depth <= MAX_RECUR) {
				(*recur_depth)++;
				found |= findFileRecursive(filename_new, path, filename, filesize, recur_depth);
				(*recur_depth)--;
			}
		}
	}
	closedir(dir);
	return found;
}

typedef struct BPathFind_Data
{
	const char *basedir;
	char searchdir[FILE_MAX];
	ReportList *reports;
} BPathFind_Data;

static int findMissingFiles_visit_cb(void *userdata, char *path_dst, const char *path_src)
{
	BPathFind_Data *data= (BPathFind_Data *)userdata;
	char filename_new[FILE_MAX];

	int filesize= -1;
	int recur_depth= 0;
	int found;

	found = findFileRecursive(filename_new,
	                          data->searchdir, BLI_path_basename((char *)path_src),
	                          &filesize, &recur_depth);

	if (filesize == -1) { /* could not open dir */
		BKE_reportf(data->reports, RPT_WARNING,
		            "Could open directory \"%s\"",
		            BLI_path_basename(data->searchdir));
		return FALSE;
	}
	else if (found == FALSE) {
		BKE_reportf(data->reports, RPT_WARNING,
		            "Could not find \"%s\" in \"%s\"",
		            BLI_path_basename((char *)path_src), data->searchdir);
		return FALSE;
	}
	else {
		BLI_strncpy(path_dst, filename_new, FILE_MAX);
		return TRUE;
	}
}

void findMissingFiles(Main *bmain, const char *searchpath, ReportList *reports)
{
	struct BPathFind_Data data= {NULL};

	data.reports= reports;
	BLI_split_dir_part(searchpath, data.searchdir, sizeof(data.searchdir));

	bpath_traverse_main(bmain, findMissingFiles_visit_cb, 0, (void *)&data);
}

/* Run a visitor on a string, replacing the contents of the string as needed. */
static int rewrite_path_fixed(char *path, BPathVisitor visit_cb, const char *absbase, void *userdata)
{
	char path_src_buf[FILE_MAX];
	const char *path_src;
	char path_dst[FILE_MAX];

	if (absbase) {
		BLI_strncpy(path_src_buf, path, sizeof(path_src_buf));
		BLI_path_abs(path_src_buf, absbase);
		path_src= path_src_buf;
	}
	else {
		path_src= path;
	}

	if (visit_cb(userdata, path_dst, path_src)) {
		BLI_strncpy(path, path_dst, FILE_MAX);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

static int rewrite_path_fixed_dirfile(char path_dir[FILE_MAXDIR],
                                      char path_file[FILE_MAXFILE],
                                      BPathVisitor visit_cb,
                                      const char *absbase,
                                      void *userdata)
{
	char path_src[FILE_MAX];
	char path_dst[FILE_MAX];

	BLI_join_dirfile(path_src, sizeof(path_src), path_dir, path_file);

	if (absbase) {
		BLI_path_abs(path_src, absbase);
	}

	if (visit_cb(userdata, path_dst, (const char *)path_src)) {
		BLI_split_dirfile(path_dst, path_dir, path_file, FILE_MAXDIR, FILE_MAXFILE);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

static int rewrite_path_alloc(char **path, BPathVisitor visit_cb, const char *absbase, void *userdata)
{
	char path_src_buf[FILE_MAX];
	const char *path_src;
	char path_dst[FILE_MAX];

	if (absbase) {
		BLI_strncpy(path_src_buf, *path, sizeof(path_src_buf));
		BLI_path_abs(path_src_buf, absbase);
		path_src= path_src_buf;
	}
	else {
		path_src= *path;
	}

	if (visit_cb(userdata, path_dst, path_src)) {
		MEM_freeN((*path));
		(*path)= BLI_strdup(path_dst);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

/* Run visitor function 'visit' on all paths contained in 'id'. */
void bpath_traverse_id(Main *bmain, ID *id, BPathVisitor visit_cb, const int flag, void *bpath_user_data)
{
	Image *ima;
	const char *absbase= (flag & BPATH_TRAVERSE_ABS) ? ID_BLEND_PATH(bmain, id) : NULL;

	if ((flag & BPATH_TRAVERSE_SKIP_LIBRARY) && id->lib) {
		return;
	}

	switch(GS(id->name)) {
	case ID_IM:
		ima= (Image *)id;
		if (ima->packedfile == NULL || (flag & BPATH_TRAVERSE_SKIP_PACKED) == 0) {
			if (ELEM3(ima->source, IMA_SRC_FILE, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
				rewrite_path_fixed(ima->name, visit_cb, absbase, bpath_user_data);
			}
		}
		break;
	case ID_BR:
		{
			Brush *brush= (Brush *)id;
			if (brush->icon_filepath[0]) {
				rewrite_path_fixed(brush->icon_filepath, visit_cb, absbase, bpath_user_data);
			}
		}
		break;
	case ID_OB:

#define BPATH_TRAVERSE_POINTCACHE(ptcaches)                                    \
	{                                                                          \
		PointCache *cache;                                                     \
		for(cache= (ptcaches).first; cache; cache= cache->next) {              \
			if(cache->flag & PTCACHE_DISK_CACHE) {                             \
				rewrite_path_fixed(cache->path,                                \
				                   visit_cb,                                   \
				                   absbase,                                    \
				                   bpath_user_data);                           \
			}                                                                  \
		}                                                                      \
	}                                                                          \


		{
			Object *ob= (Object *)id;
			ModifierData *md;
			ParticleSystem *psys;


			/* do via modifiers instead */
#if 0
			if (ob->fluidsimSettings) {
				rewrite_path_fixed(ob->fluidsimSettings->surfdataPath, visit_cb, absbase, bpath_user_data);
			}
#endif

			for (md= ob->modifiers.first; md; md= md->next) {
				if (md->type == eModifierType_Fluidsim) {
					FluidsimModifierData *fluidmd= (FluidsimModifierData *)md;
					if (fluidmd->fss) {
						rewrite_path_fixed(fluidmd->fss->surfdataPath, visit_cb, absbase, bpath_user_data);
					}
				}
				else if (md->type == eModifierType_Smoke) {
					SmokeModifierData *smd= (SmokeModifierData *)md;
					if(smd->type & MOD_SMOKE_TYPE_DOMAIN) {
						BPATH_TRAVERSE_POINTCACHE(smd->domain->ptcaches[0]);
					}
				}
				else if (md->type==eModifierType_Cloth) {
					ClothModifierData *clmd= (ClothModifierData*) md;
					BPATH_TRAVERSE_POINTCACHE(clmd->ptcaches);
				}
				else if (md->type==eModifierType_Ocean) {
					OceanModifierData *omd= (OceanModifierData*) md;
					rewrite_path_fixed(omd->cachepath, visit_cb, absbase, bpath_user_data);
				}
			}

			if (ob->soft) {
				BPATH_TRAVERSE_POINTCACHE(ob->soft->ptcaches);
			}

			for (psys= ob->particlesystem.first; psys; psys= psys->next) {
				BPATH_TRAVERSE_POINTCACHE(psys->ptcaches);
			}
		}

#undef BPATH_TRAVERSE_POINTCACHE

		break;
	case ID_SO:
		{
			bSound *sound= (bSound *)id;
			if (sound->packedfile == NULL || (flag & BPATH_TRAVERSE_SKIP_PACKED) == 0) {
				rewrite_path_fixed(sound->name, visit_cb, absbase, bpath_user_data);
			}
		}
		break;
	case ID_TXT:
		if (((Text*)id)->name) {
			rewrite_path_alloc(&((Text *)id)->name, visit_cb, absbase, bpath_user_data);
		}
		break;
	case ID_VF:
		{
			VFont *vf= (VFont *)id;
			if (vf->packedfile == NULL || (flag & BPATH_TRAVERSE_SKIP_PACKED) == 0) {
				if (strcmp(vf->name, FO_BUILTIN_NAME) != 0) {
					rewrite_path_fixed(((VFont *)id)->name, visit_cb, absbase, bpath_user_data);
				}
			}
		}
		break;
	case ID_TE:
		{
			Tex *tex = (Tex *)id;
			if (tex->plugin) {
				/* FIXME: rewrite_path assumes path length of FILE_MAX, but
					   tex->plugin->name is 160. ... is this field even a path? */
				//rewrite_path(tex->plugin->name, visit_cb, bpath_user_data);
			}
			if (tex->type == TEX_VOXELDATA && TEX_VD_IS_SOURCE_PATH(tex->vd->file_format)) {
				rewrite_path_fixed(tex->vd->source_path, visit_cb, absbase, bpath_user_data);
			}
		}
		break;

	case ID_SCE:
		{
			Scene *scene= (Scene *)id;
			if (scene->ed) {
				Sequence *seq;

				SEQ_BEGIN(scene->ed, seq) {
					if (SEQ_HAS_PATH(seq)) {
						if (ELEM(seq->type, SEQ_MOVIE, SEQ_SOUND)) {
							rewrite_path_fixed_dirfile(seq->strip->dir, seq->strip->stripdata->name,
							                           visit_cb, absbase, bpath_user_data);
						}
						else if (seq->type == SEQ_IMAGE) {
							/* might want an option not to loop over all strips */
							StripElem *se= seq->strip->stripdata;
							int len= MEM_allocN_len(se) / sizeof(*se);
							int i;

							if (flag & BPATH_TRAVERSE_SKIP_MULTIFILE) {
								/* only operate on one path */
								len= MIN2(1, len);
							}

							for(i= 0; i < len; i++, se++) {
								rewrite_path_fixed_dirfile(seq->strip->dir, se->name,
								                           visit_cb, absbase, bpath_user_data);
							}
						}
						else {
							/* simple case */
							rewrite_path_fixed(seq->strip->dir, visit_cb, absbase, bpath_user_data);
						}
					}
					else if (seq->plugin) {
						rewrite_path_fixed(seq->plugin->name, visit_cb, absbase, bpath_user_data);
					}

				}
				SEQ_END
			}
		}
		break;
	case ID_ME:
		{
			Mesh *me= (Mesh *)id;
			if (me->fdata.external) {
				rewrite_path_fixed(me->fdata.external->filename, visit_cb, absbase, bpath_user_data);
			}
		}
		break;
	case ID_LI:
		{
			Library *lib= (Library *)id;
			if(rewrite_path_fixed(lib->name, visit_cb, absbase, bpath_user_data)) {
				BKE_library_filepath_set(lib, lib->name);
			}
		}
		break;
	case ID_MC:
		{
			MovieClip *clip= (MovieClip *)id;
			rewrite_path_fixed(clip->name, visit_cb, absbase, bpath_user_data);
		}
		break;
	default:
		/* Nothing to do for other IDs that don't contain file paths. */
		break;
	}
}

void bpath_traverse_id_list(Main *bmain, ListBase *lb, BPathVisitor visit_cb, const int flag, void *bpath_user_data)
{
	ID *id;
	for(id= lb->first; id; id= id->next) {
		bpath_traverse_id(bmain, id, visit_cb, flag, bpath_user_data);
	}
}

void bpath_traverse_main(Main *bmain, BPathVisitor visit_cb, const int flag, void *bpath_user_data)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int a= set_listbasepointers(bmain, lbarray);
	while(a--) bpath_traverse_id_list(bmain, lbarray[a], visit_cb, flag, bpath_user_data);
}

/* Rewrites a relative path to be relative to the main file - unless the path is
   absolute, in which case it is not altered. */
int bpath_relocate_visitor(void *pathbase_v, char *path_dst, const char *path_src)
{
	/* be sure there is low chance of the path being too short */
	char filepath[(FILE_MAXDIR * 2) + FILE_MAXFILE];
	const char *base_new= ((char **)pathbase_v)[0];
	const char *base_old= ((char **)pathbase_v)[1];

	if (strncmp(base_old, "//", 2) == 0) {
		printf("%s: error, old base path '%s' is not absolute.\n",
		       __func__, base_old);
		return FALSE;
	}

	/* Make referenced file absolute. This would be a side-effect of
	   BLI_cleanup_file, but we do it explicitely so we know if it changed. */
	BLI_strncpy(filepath, path_src, FILE_MAX);
	if (BLI_path_abs(filepath, base_old)) {
		/* Path was relative and is now absolute. Remap.
		 * Important BLI_cleanup_dir runs before the path is made relative
		 * because it wont work for paths that start with "//../" */
		BLI_cleanup_file(base_new, filepath);
		BLI_path_rel(filepath, base_new);
		BLI_strncpy(path_dst, filepath, FILE_MAX);
		return TRUE;
	}
	else {
		/* Path was not relative to begin with. */
		return FALSE;
	}
}
