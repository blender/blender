/*
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Campbell barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/bpath.c
 *  \ingroup bli
 */


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

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h" /* to get the current frame */
#include "DNA_image_types.h"
#include "DNA_texture_types.h"
#include "DNA_text_types.h"
#include "DNA_sound_types.h"
#include "DNA_sequence_types.h"
#include "DNA_vfont_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_bpath.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_image.h" /* so we can check the image's type */
#include "BKE_sequencer.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"
#include "BKE_report.h"

//XXX #include "BIF_screen.h" /* only for wait cursor */
//
/* for sequence */
//XXX #include "BSE_sequence.h"
//XXX define below from BSE_sequence.h - otherwise potentially odd behaviour


typedef struct BPathIteratorSeqData {
	int totseq;
	int seq;
	struct Sequence **seqar; /* Sequence */
	struct Scene *scene;			/* Current scene */
} BPathIteratorSeqData;

typedef struct BPathIterator {
	char*	_path; /* never access directly, use BLI_bpathIterator_getPath */
	const char*	_lib;
	const char*	_name;
	void*	data;
	int		len;
	int		type;
	int		flag; /* iterator options */

	void (*setpath_callback)(struct BPathIterator *, const char *);
	void (*getpath_callback)(struct BPathIterator *, char *);

	const char*	base_path; /* base path, the directry the blend file is in - normally bmain->name */

	Main *bmain;

	/* only for seq data */
	struct BPathIteratorSeqData seqdata;
} BPathIterator;

#define FILE_MAX			240


/* TODO - BPATH_PLUGIN, BPATH_SEQ */
enum BPathTypes {
	BPATH_IMAGE= 0,
	BPATH_TEXTURE,
	BPATH_TEXT,
	BPATH_SOUND,
	BPATH_FONT,
	BPATH_LIB,
	BPATH_SEQ,
	BPATH_CDATA,

	 BPATH_DONE
};

void BLI_bpathIterator_init(struct BPathIterator **bpi_pt, Main *bmain, const char *basedir, const int flag)
{
	BPathIterator *bpi;

	bpi= MEM_mallocN(sizeof(BPathIterator), "BLI_bpathIterator_init");
	*bpi_pt= bpi;

	bpi->type= BPATH_IMAGE;
	bpi->data= NULL;
	
	bpi->getpath_callback= NULL;
	bpi->setpath_callback= NULL;
	
	/* Sequencer specific */
	bpi->seqdata.totseq= 0;
	bpi->seqdata.seq= 0;
	bpi->seqdata.seqar= NULL;
	bpi->seqdata.scene= NULL;

	bpi->flag= flag;

	bpi->base_path= basedir; /* normally bmain->name */
	bpi->bmain= bmain;

	BLI_bpathIterator_step(bpi);
}

#if 0
static void BLI_bpathIterator_alloc(struct BPathIterator **bpi) {
	*bpi= MEM_mallocN(sizeof(BPathIterator), "BLI_bpathIterator_alloc");
}
#endif

void BLI_bpathIterator_free(struct BPathIterator *bpi) {
	if (bpi->seqdata.seqar)
		MEM_freeN((void *)bpi->seqdata.seqar);
	bpi->seqdata.seqar= NULL;
	bpi->seqdata.scene= NULL;
	
	MEM_freeN(bpi);
}

void BLI_bpathIterator_getPath(struct BPathIterator *bpi, char *path) {
	if (bpi->getpath_callback) {
		bpi->getpath_callback(bpi, path);
	} else {
		strcpy(path, bpi->_path); /* warning, we assume 'path' are long enough */
	}
}

void BLI_bpathIterator_setPath(struct BPathIterator *bpi, const char *path) {
	if (bpi->setpath_callback) {
		bpi->setpath_callback(bpi, path);
	} else {
		strcpy(bpi->_path, path); /* warning, we assume 'path' are long enough */
	}
}

void BLI_bpathIterator_getPathExpanded(struct BPathIterator *bpi, char *path_expanded) {
	const char *libpath;
	
	BLI_bpathIterator_getPath(bpi, path_expanded);
	libpath= BLI_bpathIterator_getLib(bpi);
	
	if (libpath) { /* check the files location relative to its library path */
		BLI_path_abs(path_expanded, libpath);
	} else { /* local data, use the blend files path */
		BLI_path_abs(path_expanded, bpi->base_path);
	}
	BLI_cleanup_file(NULL, path_expanded);
}
const char* BLI_bpathIterator_getLib(struct BPathIterator *bpi) {
	return bpi->_lib;
}
const char* BLI_bpathIterator_getName(struct BPathIterator *bpi) {
	return bpi->_name;
}
int	BLI_bpathIterator_getType(struct BPathIterator *bpi) {
	return bpi->type;
}
unsigned int	BLI_bpathIterator_getPathMaxLen(struct BPathIterator *bpi) {
	return bpi->len;
}
const char* BLI_bpathIterator_getBasePath(struct BPathIterator *bpi) {
	return bpi->base_path;
}

/* gets the first or the next image that has a path - not a viewer node or generated image */
static struct Image *ima_stepdata__internal(struct Image *ima, const int step_next, const int flag)
{
	if (ima==NULL)
		return NULL;
	
	if (step_next)
		ima= ima->id.next;
	
	while (ima) {
		if (ELEM3(ima->source, IMA_SRC_FILE, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
			if(ima->packedfile==NULL || (flag & BPATH_USE_PACKED)) {
				break;
			}
		}
		/* image is not a image with a path, skip it */
		ima= ima->id.next;
	}	
	return ima;
}

static struct Tex *tex_stepdata__internal(struct Tex *tex, const int step_next, const int UNUSED(flag))
{
	if (tex==NULL)
		return NULL;

	if (step_next)
		tex= tex->id.next;

	while (tex) {
		if (tex->type == TEX_VOXELDATA && TEX_VD_IS_SOURCE_PATH(tex->vd->file_format))
			break;
		/* image is not a image with a path, skip it */
		tex= tex->id.next;
	}	
	return tex;
}

static struct Text *text_stepdata__internal(struct Text *text, const int step_next, const int UNUSED(flag))
{
	if (text==NULL)
		return NULL;

	if (step_next)
		text= text->id.next;

	while (text) {
		if (text->name)
			break;
		/* image is not a image with a path, skip it */
		text= text->id.next;
	}	
	return text;
}

static struct VFont *vf_stepdata__internal(struct VFont *vf, const int step_next, const int flag)
{
	if (vf==NULL)
		return NULL;
	
	if (step_next)
		vf= vf->id.next;
	
	while (vf) {
		if (strcmp(vf->name, FO_BUILTIN_NAME)!=0) {
			if(vf->packedfile==NULL || (flag & BPATH_USE_PACKED)) {
				break;
			}
		}
		
		/* font with no path, skip it */
		vf= vf->id.next;
	}	
	return vf;
}

static struct bSound *snd_stepdata__internal(struct bSound *snd, int step_next, const int flag)
{
	if (snd==NULL)
		return NULL;
	
	if (step_next)
		snd= snd->id.next;
	
	while (snd) {
		if(snd->packedfile==NULL || (flag & BPATH_USE_PACKED)) {
			break;
		}

		/* font with no path, skip it */
		snd= snd->id.next;
	}	
	return snd;
}

static struct Sequence *seq_stepdata__internal(struct BPathIterator *bpi, int step_next)
{
	Editing *ed;
	Sequence *seq;
	
	/* Initializing */
	if (bpi->seqdata.scene==NULL) {
		bpi->seqdata.scene= bpi->bmain->scene.first;
	}
	
	if (step_next) {
		bpi->seqdata.seq++;
	}
	
	while (bpi->seqdata.scene) {
		ed= seq_give_editing(bpi->seqdata.scene, 0);
		if (ed) {
			if (bpi->seqdata.seqar == NULL) {
				/* allocate the sequencer array */
				seq_array(ed, &bpi->seqdata.seqar, &bpi->seqdata.totseq, 0);
				bpi->seqdata.seq= 0;
			}
			
			if (bpi->seqdata.seq >= bpi->seqdata.totseq) {
				seq= NULL;
			} else {
				seq= bpi->seqdata.seqar[bpi->seqdata.seq];
				while (!SEQ_HAS_PATH(seq) && seq->plugin==NULL) {
					bpi->seqdata.seq++;
					if (bpi->seqdata.seq >= bpi->seqdata.totseq) {
						seq= NULL;
						break;
					}
					seq= bpi->seqdata.seqar[bpi->seqdata.seq];
				}
			}
			if (seq) {
				return seq;
			} else {
				/* keep looking through the next scene, reallocate seq array */
				if (bpi->seqdata.seqar) {
					MEM_freeN((void *)bpi->seqdata.seqar);
					bpi->seqdata.seqar= NULL;
				}
				bpi->seqdata.scene= bpi->seqdata.scene->id.next;
			}
		} else {
			/* no seq data in this scene, next */
			bpi->seqdata.scene= bpi->seqdata.scene->id.next;
		}
	}
	
	return NULL;
}

static void seq_getpath(struct BPathIterator *bpi, char *path) {
	Sequence *seq= (Sequence *)bpi->data;

	
	path[0]= '\0'; /* incase we cant get the path */
	if (seq==NULL) return;
	if (SEQ_HAS_PATH(seq)) {
		if (ELEM3(seq->type, SEQ_IMAGE, SEQ_MOVIE, SEQ_SOUND)) {
			BLI_strncpy(path, seq->strip->dir, FILE_MAX);
			BLI_add_slash(path); /* incase its missing */
			if (seq->strip->stripdata) { /* should always be true! */
				/* Using the first image is weak for image sequences */
				strcat(path, seq->strip->stripdata->name);
			} 
		}
		else {
			/* simple case */
			BLI_strncpy(seq->strip->dir, path, sizeof(seq->strip->dir));
		}
	}
	else if (seq->plugin) {
		BLI_strncpy(seq->plugin->name, path, sizeof(seq->plugin->name));
	}
}

static void seq_setpath(struct BPathIterator *bpi, const char *path) {
	Sequence *seq= (Sequence *)bpi->data;
	if (seq==NULL) return; 
	
	if (SEQ_HAS_PATH(seq)) {
		if (ELEM3(seq->type, SEQ_IMAGE, SEQ_MOVIE, SEQ_SOUND)) {
			BLI_split_dirfile(path, seq->strip->dir, seq->strip->stripdata->name);
		}
		else {
			/* simple case */
			BLI_strncpy(seq->strip->dir, path, sizeof(seq->strip->dir));
		}
	}
	else if (seq->plugin) {
		BLI_strncpy(seq->plugin->name, path, sizeof(seq->plugin->name));
	}
}

static void text_getpath(struct BPathIterator *bpi, char *path) {
	Text *text= (Text *)bpi->data;
	path[0]= '\0'; /* incase we cant get the path */
	if(text->name) {
		strcpy(path, text->name);
	}
}

static void text_setpath(struct BPathIterator *bpi, const char *path) {
	Text *text= (Text *)bpi->data;
	if (text==NULL) return; 

	if(text->name) {
		MEM_freeN(text->name);
	}

	text->name= BLI_strdup(path);
}

static struct Mesh *cdata_stepdata__internal(struct Mesh *me, int step_next) {
	if (me==NULL)
		return NULL;
	
	if (step_next)
		me= me->id.next;
	
	while (me) {
		if (me->fdata.external) {
			break;
		}
		
		me= me->id.next;
	}	
	return me;
}

static void bpi_type_step__internal(struct BPathIterator *bpi) {
	bpi->type++; /* advance to the next type */
	bpi->data= NULL;
	
	switch (bpi->type) {
	case BPATH_SEQ:
		bpi->getpath_callback= seq_getpath;
		bpi->setpath_callback= seq_setpath;
		break;
	case BPATH_TEXT: /* path is malloc'd */
		bpi->getpath_callback= text_getpath;
		bpi->setpath_callback= text_setpath;
		break;
	default:
		bpi->getpath_callback= NULL;
		bpi->setpath_callback= NULL;
		break;
	}
}

void BLI_bpathIterator_step(struct BPathIterator *bpi) {
	while (bpi->type != BPATH_DONE) {
		
		if  ((bpi->type) == BPATH_IMAGE) {
			/*if (bpi->data)	bpi->data= ((ID *)bpi->data)->next;*/
			if (bpi->data)	bpi->data= ima_stepdata__internal((Image *)bpi->data, 1, bpi->flag); /* must skip images that have no path */
			else 			bpi->data= ima_stepdata__internal(bpi->bmain->image.first, 0, bpi->flag);
			
			if (bpi->data) {
				/* get the path info from this datatype */
				Image *ima= (Image *)bpi->data;
				
				bpi->_lib= ima->id.lib ? ima->id.lib->filepath : NULL;
				bpi->_path= ima->name;
				bpi->_name= ima->id.name+2;
				bpi->len= sizeof(ima->name);
				
				/* we are done, advancing to the next item, this type worked fine */
				break;

			} else {
				bpi_type_step__internal(bpi);
			}
		}

		if  ((bpi->type) == BPATH_TEXTURE) {
			/*if (bpi->data)	bpi->data= ((ID *)bpi->data)->next;*/
			if (bpi->data)	bpi->data= tex_stepdata__internal( (Tex *)bpi->data, 1, bpi->flag); /* must skip images that have no path */
			else 			bpi->data= tex_stepdata__internal(bpi->bmain->tex.first, 0, bpi->flag);

			if (bpi->data) {
				/* get the path info from this datatype */
				Tex *tex= (Tex *)bpi->data;

				if(tex->type == TEX_VOXELDATA) {
					bpi->_lib= tex->id.lib ? tex->id.lib->filepath : NULL;
					bpi->_path= tex->vd->source_path;
					bpi->_name= tex->id.name+2;
					bpi->len= sizeof(tex->vd->source_path);
				}
				else {
					assert(!"Texture has no path, incorrect step 'tex_stepdata__internal'");
				}

				/* we are done, advancing to the next item, this type worked fine */
				break;

			} else {
				bpi_type_step__internal(bpi);
			}
		}

		if  ((bpi->type) == BPATH_TEXT) {
			/*if (bpi->data)	bpi->data= ((ID *)bpi->data)->next;*/
			if (bpi->data)	bpi->data= text_stepdata__internal((Text *)bpi->data, 1, bpi->flag); /* must skip images that have no path */
			else 			bpi->data= text_stepdata__internal(bpi->bmain->text.first, 0, bpi->flag);

			if (bpi->data) {
				/* get the path info from this datatype */
				Text *text= (Text *)bpi->data;

				bpi->_lib= text->id.lib ? text->id.lib->filepath : NULL;
				bpi->_path= NULL; /* bpi->path= text->name; */ /* get/set functions override. */
				bpi->_name= text->id.name+2;
				bpi->len= FILE_MAX; /* malloc'd but limit anyway since large paths may mess up other areas */

				/* we are done, advancing to the next item, this type worked fine */
				break;

			} else {
				bpi_type_step__internal(bpi);
			}
		}

		else if  ((bpi->type) == BPATH_SOUND) {
			if (bpi->data)	bpi->data= snd_stepdata__internal((bSound *)bpi->data, 1, bpi->flag); /* must skip images that have no path */
			else 			bpi->data= snd_stepdata__internal(bpi->bmain->sound.first, 0, bpi->flag);

			if (bpi->data) {
				/* get the path info from this datatype */
				bSound *snd= (bSound *)bpi->data;

				bpi->_lib= snd->id.lib ? snd->id.lib->filepath : NULL;
				bpi->_path= snd->name;
				bpi->_name= snd->id.name+2;
				bpi->len= sizeof(snd->name);

				/* we are done, advancing to the next item, this type worked fine */
				break;
			} else {
				bpi_type_step__internal(bpi);
			}
			
			
		} else if  ((bpi->type) == BPATH_FONT) {
			
			if (bpi->data)	bpi->data= vf_stepdata__internal((VFont *)bpi->data, 1, bpi->flag);
			else 			bpi->data= vf_stepdata__internal(bpi->bmain->vfont.first, 0, bpi->flag);
			
			if (bpi->data) {
				/* get the path info from this datatype */
				VFont *vf= (VFont *)bpi->data;

				bpi->_lib= vf->id.lib ? vf->id.lib->filepath : NULL;
				bpi->_path= vf->name;
				bpi->_name= vf->id.name+2;
				bpi->len= sizeof(vf->name);

				/* we are done, advancing to the next item, this type worked fine */
				break;
			} else {
				bpi_type_step__internal(bpi);
			}

		} else if  ((bpi->type) == BPATH_LIB) {
			if (bpi->data)	bpi->data= ((ID *)bpi->data)->next;
			else 			bpi->data= bpi->bmain->library.first;
			
			if (bpi->data) {
				/* get the path info from this datatype */
				Library *lib= (Library *)bpi->data;
				
				bpi->_lib= NULL;
				bpi->_path= lib->name;
				bpi->_name= NULL;
				bpi->len= sizeof(lib->name);
				
				/* we are done, advancing to the next item, this type worked fine */
				break;
			} else {
				bpi_type_step__internal(bpi);
			}
		} else if  ((bpi->type) == BPATH_SEQ) {
			if (bpi->data)	bpi->data= seq_stepdata__internal( bpi, 1 );
			else 			bpi->data= seq_stepdata__internal( bpi, 0 );
			if (bpi->data) {
				Sequence *seq= (Sequence *)bpi->data;
				bpi->_lib= NULL;
				bpi->_name= seq->name+2;
				bpi->len= seq->plugin ? sizeof(seq->plugin->name) : sizeof(seq->strip->dir) + sizeof(seq->strip->stripdata->name);
				break;
			} else {
				bpi_type_step__internal(bpi);
			}
		} else if  ((bpi->type) == BPATH_CDATA) {
			if (bpi->data)	bpi->data= cdata_stepdata__internal( bpi->data, 1 );
			else 			bpi->data= cdata_stepdata__internal( bpi->bmain->mesh.first, 0 );

			if (bpi->data) {
				Mesh *me= (Mesh *)bpi->data;
				bpi->_lib= me->id.lib ? me->id.lib->filepath : NULL;
				bpi->_path= me->fdata.external->filename;
				bpi->_name= me->id.name+2;
				bpi->len= sizeof(me->fdata.external->filename);
				break;
			} else {
				bpi_type_step__internal(bpi);
			}
		}
	}
}

int BLI_bpathIterator_isDone( struct BPathIterator *bpi) {
	return bpi->type==BPATH_DONE;
}

/* include the path argument */
static void bpath_as_report(struct BPathIterator *bpi, const char *message, ReportList *reports)
{
	const char *prefix;
	const char *name;
	char path_expanded[FILE_MAXDIR*2];
	
	if(reports==NULL)
		return;

	switch(BLI_bpathIterator_getType(bpi)) {
	case BPATH_IMAGE:
		prefix= "Image";
		break;
	case BPATH_TEXTURE:
		prefix= "Texture";
		break;
	case BPATH_TEXT:
		prefix= "Text";
		break;
	case BPATH_SOUND:
		prefix= "Sound";
		break;
	case BPATH_FONT:
		prefix= "Font";
		break;
	case BPATH_LIB:
		prefix= "Library";
		break;
	case BPATH_SEQ:
		prefix= "Sequence";
		break;
	case BPATH_CDATA:
		prefix= "Mesh Data";
		break;
	default:
		prefix= "Unknown";
		break;
	}
	
	name= BLI_bpathIterator_getName(bpi);
	BLI_bpathIterator_getPathExpanded(bpi, path_expanded);

	if(reports) {
		if (name)	BKE_reportf(reports, RPT_WARNING, "%s \"%s\", \"%s\": %s", prefix, name, path_expanded, message);
		else		BKE_reportf(reports, RPT_WARNING, "%s \"%s\": %s", prefix, path_expanded, message);
	}

}

/* high level function */
void checkMissingFiles(Main *bmain, ReportList *reports) {
	struct BPathIterator *bpi;
	
	/* be sure there is low chance of the path being too short */
	char filepath_expanded[FILE_MAXDIR*2]; 
	
	BLI_bpathIterator_init(&bpi, bmain, bmain->name, 0);
	while (!BLI_bpathIterator_isDone(bpi)) {
		BLI_bpathIterator_getPathExpanded(bpi, filepath_expanded);
		
		if (!BLI_exists(filepath_expanded))
			bpath_as_report(bpi, "file not found", reports);

		BLI_bpathIterator_step(bpi);
	}
	BLI_bpathIterator_free(bpi);
}

/* dont log any errors at the moment, should probably do this */
void makeFilesRelative(Main *bmain, const char *basedir, ReportList *reports) {
	int tot= 0, changed= 0, failed= 0, linked= 0;
	struct BPathIterator *bpi;
	char filepath[FILE_MAX];
	const char *libpath;
	
	/* be sure there is low chance of the path being too short */
	char filepath_relative[(FILE_MAXDIR * 2) + FILE_MAXFILE];
	
	BLI_bpathIterator_init(&bpi, bmain, basedir, 0);
	while (!BLI_bpathIterator_isDone(bpi)) {
		BLI_bpathIterator_getPath(bpi, filepath);
		libpath= BLI_bpathIterator_getLib(bpi);
		
		if(strncmp(filepath, "//", 2)) {
			if (libpath) { /* cant make relative if we are library - TODO, LOG THIS */
				linked++;
			} else { /* local data, use the blend files path */
				BLI_strncpy(filepath_relative, filepath, sizeof(filepath_relative));
				/* Important BLI_cleanup_dir runs before the path is made relative
				 * because it wont work for paths that start with "//../" */ 
				BLI_cleanup_file(bpi->base_path, filepath_relative); /* fix any /foo/../foo/ */
				BLI_path_rel(filepath_relative, bpi->base_path);
				/* be safe and check the length */
				if (BLI_bpathIterator_getPathMaxLen(bpi) <= strlen(filepath_relative)) {
					bpath_as_report(bpi, "couldn't make path relative (too long)", reports);
					failed++;
				} else {
					if(strncmp(filepath_relative, "//", 2)==0) {
						BLI_bpathIterator_setPath(bpi, filepath_relative);
						changed++;
					} else {
						bpath_as_report(bpi, "couldn't make path relative", reports);
						failed++;
					}
				}
			}
		}
		BLI_bpathIterator_step(bpi);
		tot++;
	}
	BLI_bpathIterator_free(bpi);

	if(reports)
		BKE_reportf(reports, failed ? RPT_ERROR : RPT_INFO, "Total files %i|Changed %i|Failed %i|Linked %i", tot, changed, failed, linked);
}

/* dont log any errors at the moment, should probably do this -
 * Verry similar to makeFilesRelative - keep in sync! */
void makeFilesAbsolute(Main *bmain, const char *basedir, ReportList *reports)
{
	int tot= 0, changed= 0, failed= 0, linked= 0;

	struct BPathIterator *bpi;
	char filepath[FILE_MAX];
	const char *libpath;
	
	/* be sure there is low chance of the path being too short */
	char filepath_absolute[(FILE_MAXDIR * 2) + FILE_MAXFILE];
	
	BLI_bpathIterator_init(&bpi, bmain, basedir, 0);
	while (!BLI_bpathIterator_isDone(bpi)) {
		BLI_bpathIterator_getPath(bpi, filepath);
		libpath= BLI_bpathIterator_getLib(bpi);
		
		if(strncmp(filepath, "//", 2)==0) {
			if (libpath) { /* cant make absolute if we are library - TODO, LOG THIS */
				linked++;
			} else { /* get the expanded path and check it is relative or too long */
				BLI_bpathIterator_getPathExpanded(bpi, filepath_absolute);
				BLI_cleanup_file(bpi->base_path, filepath_absolute); /* fix any /foo/../foo/ */
				/* to be safe, check the length */
				if (BLI_bpathIterator_getPathMaxLen(bpi) <= strlen(filepath_absolute)) {
					bpath_as_report(bpi, "couldn't make absolute (too long)", reports);
					failed++;
				} else {
					if(strncmp(filepath_absolute, "//", 2)) {
						BLI_bpathIterator_setPath(bpi, filepath_absolute);
						changed++;
					} else {
						bpath_as_report(bpi, "couldn't make absolute", reports);
						failed++;
					}
				}
			}
		}
		BLI_bpathIterator_step(bpi);
		tot++;
	}
	BLI_bpathIterator_free(bpi);

	if(reports)
		BKE_reportf(reports, failed ? RPT_ERROR : RPT_INFO, "Total files %i|Changed %i|Failed %i|Linked %i", tot, changed, failed, linked);
}


/* find this file recursively, use the biggest file so thumbnails dont get used by mistake
 - dir: subdir to search
 - filename: set this filename
 - filesize: filesize for the file
*/
#define MAX_RECUR 16
static int findFileRecursive(char *filename_new, const char *dirname, const char *filename, int *filesize, int *recur_depth)
{
	/* file searching stuff */
	DIR *dir;
	struct dirent *de;
	struct stat status;
	char path[FILE_MAX];
	int size;
	
	dir= opendir(dirname);
	
	if (dir==NULL)
		return 0;
	
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
				}
			}
		} else if (S_ISDIR(status.st_mode)) { /* is subdir */
			if (*recur_depth <= MAX_RECUR) {
				(*recur_depth)++;
				findFileRecursive(filename_new, path, filename, filesize, recur_depth);
				(*recur_depth)--;
			}
		}
	}
	closedir(dir);
	return 1;
}

/* high level function - call from fileselector */
void findMissingFiles(Main *bmain, const char *str) {
	struct BPathIterator *bpi;
	
	/* be sure there is low chance of the path being too short */
	char filepath_expanded[FILE_MAXDIR*2]; 
	char filepath[FILE_MAX];
	const char *libpath;
	int filesize, recur_depth;
	
	char dirname[FILE_MAX], filename_new[FILE_MAX];
	
	//XXX waitcursor( 1 );
	
	BLI_split_dirfile(str, dirname, NULL);
	
	BLI_bpathIterator_init(&bpi, bmain, bmain->name, 0);
	
	while (!BLI_bpathIterator_isDone(bpi)) {
		BLI_bpathIterator_getPath(bpi, filepath);
		libpath= BLI_bpathIterator_getLib(bpi);
		
		/* Check if esc was pressed because searching files can be slow */
		/*XXX if (blender_test_break()) {
			break;
		}*/
		
		if (libpath==NULL) {
			
			BLI_bpathIterator_getPathExpanded(bpi, filepath_expanded);
			
			if (!BLI_exists(filepath_expanded)) {
				/* can the dir be opened? */
				filesize= -1;
				recur_depth= 0;
				
				findFileRecursive(filename_new, dirname, BLI_path_basename(filepath), &filesize, &recur_depth);
				if (filesize == -1) { /* could not open dir */
					printf("Could not open dir \"%s\"\n", dirname);
					return;
				}
				
				if (filesize > 0) {
					
					if (BLI_bpathIterator_getPathMaxLen(bpi) < strlen(filename_new)) { 
						printf("cannot set path \"%s\" too long!", filename_new);
					} else {
						/* copy the found path into the old one */
						if (G.relbase_valid)
							BLI_path_rel(filename_new, bpi->base_path);
						
						BLI_bpathIterator_setPath(bpi, filename_new);
					}
				}
			}
		}
		BLI_bpathIterator_step(bpi);
	}
	BLI_bpathIterator_free(bpi);
	
	//XXX waitcursor( 0 );
}
