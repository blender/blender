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

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

/* path/file handeling stuff */
#ifndef WIN32
  #include <dirent.h>
  #include <unistd.h>
#else
  #include <io.h>
  #include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "DNA_ID.h" /* Library */
#include "DNA_customdata_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h" /* to get the current frame */
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_vfont_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_bpath.h"

#include "BKE_global.h"
#include "BKE_image.h" /* so we can check the image's type */
#include "BKE_main.h" /* so we can access G.main->*.first */
#include "BKE_sequencer.h"
#include "BKE_utildefines.h"
#include "BKE_report.h"

//XXX #include "BIF_screen.h" /* only for wait cursor */
//
/* for sequence */
//XXX #include "BSE_sequence.h"
//XXX define below from BSE_sequence.h - otherwise potentially odd behaviour
#define SEQ_HAS_PATH(seq) (seq->type==SEQ_MOVIE || seq->type==SEQ_IMAGE)



#define FILE_MAX			240

/* TODO - BPATH_PLUGIN, BPATH_SEQ */
enum BPathTypes {
	BPATH_IMAGE = 0,
	BPATH_SOUND,
	BPATH_FONT,
	BPATH_LIB,
	BPATH_SEQ,
	BPATH_CDATA,

 	BPATH_DONE
};

void BLI_bpathIterator_init( struct BPathIterator *bpi, char *base_path ) {
	bpi->type = BPATH_IMAGE;
	bpi->data = NULL;
	
	bpi->getpath_callback = NULL;
	bpi->setpath_callback = NULL;
	
	/* Sequencer specific */
	bpi->seqdata.totseq = 0;
	bpi->seqdata.seq = 0;
	bpi->seqdata.seqar = NULL;
	bpi->seqdata.scene = NULL;
	
	bpi->base_path= base_path ? base_path : G.sce;

	BLI_bpathIterator_step(bpi);
}

void BLI_bpathIterator_free( struct BPathIterator *bpi ) {
	if (bpi->seqdata.seqar)
		MEM_freeN((void *)bpi->seqdata.seqar);
	bpi->seqdata.seqar = NULL;
	bpi->seqdata.scene = NULL;
}

void BLI_bpathIterator_getPath( struct BPathIterator *bpi, char *path) {
	if (bpi->getpath_callback) {
		bpi->getpath_callback( bpi, path );
	} else {
		strcpy(path, bpi->path); /* warning, we assume 'path' are long enough */
	}
}

void BLI_bpathIterator_setPath( struct BPathIterator *bpi, char *path) {
	if (bpi->setpath_callback) {
		bpi->setpath_callback( bpi, path );
	} else {
		strcpy(bpi->path, path); /* warning, we assume 'path' are long enough */
	}
}

void BLI_bpathIterator_getPathExpanded( struct BPathIterator *bpi, char *path_expanded) {
	char *libpath;
	
	BLI_bpathIterator_getPath(bpi, path_expanded);
	libpath = BLI_bpathIterator_getLib(bpi);
	
	if (libpath) { /* check the files location relative to its library path */
		BLI_path_abs(path_expanded, libpath);
	} else { /* local data, use the blend files path */
		BLI_path_abs(path_expanded, bpi->base_path);
	}
}
char* BLI_bpathIterator_getLib( struct BPathIterator *bpi) {
	return bpi->lib;
}
char* BLI_bpathIterator_getName( struct BPathIterator *bpi) {
	return bpi->name;
}
int	BLI_bpathIterator_getType( struct BPathIterator *bpi) {
	return bpi->type;
}
int	BLI_bpathIterator_getPathMaxLen( struct BPathIterator *bpi) {
	return bpi->len;
}

/* gets the first or the next image that has a path - not a viewer node or generated image */
static struct Image *ima_stepdata__internal(struct Image *ima, int step_next) {
	if (ima==NULL)
		return NULL;
	
	if (step_next)
		ima = ima->id.next;
	
	while (ima) {
		if (ima->packedfile==NULL && ELEM3(ima->source, IMA_SRC_FILE, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE))
			break;
		/* image is not a image with a path, skip it */
		ima = ima->id.next;
	}	
	return ima;
}

static struct VFont *vf_stepdata__internal(struct VFont *vf, int step_next) {
	if (vf==NULL)
		return NULL;
	
	if (step_next)
		vf = vf->id.next;
	
	while (vf) {
		if (vf->packedfile==NULL && BLI_streq(vf->name, "<builtin>")==0) {
			break;
		}
		
		/* font with no path, skip it */
		vf = vf->id.next;
	}	
	return vf;
}

static struct bSound *snd_stepdata__internal(struct bSound *snd, int step_next) {
	if (snd==NULL)
		return NULL;
	
	if (step_next)
		snd = snd->id.next;
	
	while (snd) {
		if (snd->packedfile==NULL) {
			break;
		}
		
		/* font with no path, skip it */
		snd = snd->id.next;
	}	
	return snd;
}

static struct Sequence *seq_stepdata__internal(struct BPathIterator *bpi, int step_next)
{
	Editing *ed;
	Sequence *seq;
	
	/* Initializing */
	if (bpi->seqdata.scene==NULL) {
		bpi->seqdata.scene= G.main->scene.first;
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
				bpi->seqdata.seq = 0;
			}
			
			if (bpi->seqdata.seq >= bpi->seqdata.totseq) {
				seq = NULL;
			} else {
				seq = bpi->seqdata.seqar[bpi->seqdata.seq];
				while (!SEQ_HAS_PATH(seq)) {
					bpi->seqdata.seq++;
					if (bpi->seqdata.seq >= bpi->seqdata.totseq) {
						seq = NULL;
						break;
					}
					seq = bpi->seqdata.seqar[bpi->seqdata.seq];
				}
			}
			if (seq) {
				return seq;
			} else {
				/* keep looking through the next scene, reallocate seq array */
				if (bpi->seqdata.seqar) {
					MEM_freeN((void *)bpi->seqdata.seqar);
					bpi->seqdata.seqar = NULL;
				}
				bpi->seqdata.scene = bpi->seqdata.scene->id.next;
			}
		} else {
			/* no seq data in this scene, next */
			bpi->seqdata.scene = bpi->seqdata.scene->id.next;
		}
	}
	
	return NULL;
}

static void seq_getpath(struct BPathIterator *bpi, char *path) {
	Sequence *seq = (Sequence *)bpi->data;

	
	path[0] = '\0'; /* incase we cant get the path */
	if (seq==NULL) return;
	if (SEQ_HAS_PATH(seq)) {
		if (seq->type == SEQ_IMAGE || seq->type == SEQ_MOVIE) {
			BLI_strncpy(path, seq->strip->dir, FILE_MAX);
			BLI_add_slash(path); /* incase its missing */
			if (seq->strip->stripdata) { /* should always be true! */
				/* Using the first image is weak for image sequences */
				strcat(path, seq->strip->stripdata->name);
			} 
		} else {
			/* simple case */
			BLI_strncpy(seq->strip->dir, path, sizeof(seq->strip->dir));
		}
	}
}

static void seq_setpath(struct BPathIterator *bpi, char *path) {
	Sequence *seq = (Sequence *)bpi->data;
	if (seq==NULL) return; 
	
	if (SEQ_HAS_PATH(seq)) {
		if (seq->type == SEQ_IMAGE || seq->type == SEQ_MOVIE) {
			BLI_split_dirfile(path, seq->strip->dir, seq->strip->stripdata->name);
		} else {
			/* simple case */
			BLI_strncpy(seq->strip->dir, path, sizeof(seq->strip->dir));
		}
	}
}

static struct Mesh *cdata_stepdata__internal(struct Mesh *me, int step_next) {
	if (me==NULL)
		return NULL;
	
	if (step_next)
		me = me->id.next;
	
	while (me) {
		if (me->fdata.external) {
			break;
		}
		
		me = me->id.next;
	}	
	return me;
}

static void bpi_type_step__internal( struct BPathIterator *bpi) {
	bpi->type++; /* advance to the next type */
	bpi->data = NULL;
	
	switch (bpi->type) {
	case BPATH_SEQ:
		bpi->getpath_callback = seq_getpath;
		bpi->setpath_callback = seq_setpath;
		break;
	default:
		bpi->getpath_callback = NULL;
		bpi->setpath_callback = NULL;
		break;
	}
}

void BLI_bpathIterator_step( struct BPathIterator *bpi) {
	while (bpi->type != BPATH_DONE) {
		
		if  ((bpi->type) == BPATH_IMAGE) {
			/*if (bpi->data)	bpi->data = ((ID *)bpi->data)->next;*/
			if (bpi->data)	bpi->data = ima_stepdata__internal( (Image *)bpi->data, 1 ); /* must skip images that have no path */
			else 			bpi->data = ima_stepdata__internal(G.main->image.first, 0);
			
			if (bpi->data) {
				/* get the path info from this datatype */
				Image *ima = (Image *)bpi->data;
				
				bpi->lib = ima->id.lib ? ima->id.lib->filename : NULL;
				bpi->path = ima->name;
				bpi->name = ima->id.name+2;
				bpi->len = sizeof(ima->name);
				
				/* we are done, advancing to the next item, this type worked fine */
				break;
				
			} else {
				bpi_type_step__internal(bpi);
			}
			
			
		} else if  ((bpi->type) == BPATH_SOUND) {
			if (bpi->data)	bpi->data = snd_stepdata__internal( (bSound *)bpi->data, 1 ); /* must skip images that have no path */
			else 			bpi->data = snd_stepdata__internal(G.main->sound.first, 0);
			
			if (bpi->data) {
				/* get the path info from this datatype */
				bSound *snd = (bSound *)bpi->data;
				
				bpi->lib = snd->id.lib ? snd->id.lib->filename : NULL;
				bpi->path = snd->name;
				bpi->name = snd->id.name+2;
				bpi->len = sizeof(snd->name);
				
				/* we are done, advancing to the next item, this type worked fine */
				break;
			} else {
				bpi_type_step__internal(bpi);
			}
			
			
		} else if  ((bpi->type) == BPATH_FONT) {
			
			if (bpi->data)	bpi->data = vf_stepdata__internal( (VFont *)bpi->data, 1 );
			else 			bpi->data = vf_stepdata__internal( G.main->vfont.first, 0 );
			
			if (bpi->data) {
				/* get the path info from this datatype */
				VFont *vf = (VFont *)bpi->data;
				
				bpi->lib = vf->id.lib ? vf->id.lib->filename : NULL;
				bpi->path = vf->name;
				bpi->name = vf->id.name+2;
				bpi->len = sizeof(vf->name);
				
				/* we are done, advancing to the next item, this type worked fine */
				break;
			} else {
				bpi_type_step__internal(bpi);
			}
			
		} else if  ((bpi->type) == BPATH_LIB) {
			if (bpi->data)	bpi->data = ((ID *)bpi->data)->next;
			else 			bpi->data = G.main->library.first;
			
			if (bpi->data) {
				/* get the path info from this datatype */
				Library *lib = (Library *)bpi->data;
				
				bpi->lib = NULL;
				bpi->path = lib->name;
				bpi->name = NULL;
				bpi->len = sizeof(lib->name);
				
				/* we are done, advancing to the next item, this type worked fine */
				break;
			} else {
				bpi_type_step__internal(bpi);
			}
		} else if  ((bpi->type) == BPATH_SEQ) {
			if (bpi->data)	bpi->data = seq_stepdata__internal( bpi, 1 );
			else 			bpi->data = seq_stepdata__internal( bpi, 0 );
			if (bpi->data) {
				Sequence *seq = (Sequence *)bpi->data;
				bpi->lib = NULL;
				bpi->name = seq->name+2;
				bpi->len = sizeof(seq->strip->stripdata->name);
				break;
			} else {
				bpi_type_step__internal(bpi);
			}
		} else if  ((bpi->type) == BPATH_CDATA) {
			if (bpi->data)	bpi->data = cdata_stepdata__internal( bpi->data, 1 );
			else 			bpi->data = cdata_stepdata__internal( G.main->mesh.first, 0 );

			if (bpi->data) {
				Mesh *me = (Mesh *)bpi->data;
				bpi->lib = me->id.lib ? me->id.lib->filename : NULL;
				bpi->path = me->fdata.external->filename;
				bpi->name = me->id.name+2;
				bpi->len = sizeof(me->fdata.external->filename);
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
	char *prefix;
	char *name;
	char path_expanded[FILE_MAXDIR*2];
	
	if(reports==NULL)
		return;

	switch(BLI_bpathIterator_getType(bpi)) {
	case BPATH_IMAGE:
		prefix= "Image";
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
	
	name = BLI_bpathIterator_getName(bpi);
	BLI_bpathIterator_getPathExpanded(bpi, path_expanded);

	if(reports) {
		if (name)	BKE_reportf(reports, RPT_WARNING, "%s \"%s\", \"%s\": %s", prefix, name, path_expanded, message);
		else		BKE_reportf(reports, RPT_WARNING, "%s \"%s\": %s", prefix, path_expanded, message);
	}

}

/* high level function */
void checkMissingFiles(char *basepath, ReportList *reports) {
	struct BPathIterator bpi;
	
	/* be sure there is low chance of the path being too short */
	char filepath_expanded[FILE_MAXDIR*2]; 
	
	BLI_bpathIterator_init(&bpi, basepath);
	while (!BLI_bpathIterator_isDone(&bpi)) {
		BLI_bpathIterator_getPathExpanded( &bpi, filepath_expanded );
		
		if (!BLI_exists(filepath_expanded))
			bpath_as_report(&bpi, "file not found", reports);

		BLI_bpathIterator_step(&bpi);
	}
	BLI_bpathIterator_free(&bpi);
}

/* dont log any errors at the moment, should probably do this */
void makeFilesRelative(char *basepath, ReportList *reports) {
	int tot= 0, changed= 0, failed= 0, linked= 0;
	struct BPathIterator bpi;
	char filepath[FILE_MAX], *libpath;
	
	/* be sure there is low chance of the path being too short */
	char filepath_relative[(FILE_MAXDIR * 2) + FILE_MAXFILE];
	
	BLI_bpathIterator_init(&bpi, basepath);
	while (!BLI_bpathIterator_isDone(&bpi)) {
		BLI_bpathIterator_getPath(&bpi, filepath);
		libpath = BLI_bpathIterator_getLib(&bpi);
		
		if(strncmp(filepath, "//", 2)) {
			if (libpath) { /* cant make relative if we are library - TODO, LOG THIS */
				linked++;
			} else { /* local data, use the blend files path */
				BLI_strncpy(filepath_relative, filepath, sizeof(filepath_relative));
				/* Important BLI_cleanup_dir runs before the path is made relative
				 * because it wont work for paths that start with "//../" */ 
				BLI_cleanup_file(bpi.base_path, filepath_relative); /* fix any /foo/../foo/ */
				BLI_path_rel(filepath_relative, bpi.base_path);
				/* be safe and check the length */
				if (BLI_bpathIterator_getPathMaxLen(&bpi) <= strlen(filepath_relative)) {
					bpath_as_report(&bpi, "couldn't make path relative (too long)", reports);
					failed++;
				} else {
					if(strncmp(filepath_relative, "//", 2)==0) {
						BLI_bpathIterator_setPath(&bpi, filepath_relative);
						changed++;
					} else {
						bpath_as_report(&bpi, "couldn't make path relative", reports);
						failed++;
					}
				}
			}
		}
		BLI_bpathIterator_step(&bpi);
		tot++;
	}
	BLI_bpathIterator_free(&bpi);

	if(reports)
		BKE_reportf(reports, failed ? RPT_ERROR : RPT_INFO, "Total files %i|Changed %i|Failed %i|Linked %i", tot, changed, failed, linked);
}

/* dont log any errors at the moment, should probably do this -
 * Verry similar to makeFilesRelative - keep in sync! */
void makeFilesAbsolute(char *basepath, ReportList *reports)
{
	int tot= 0, changed= 0, failed= 0, linked= 0;

	struct BPathIterator bpi;
	char filepath[FILE_MAX], *libpath;
	
	/* be sure there is low chance of the path being too short */
	char filepath_absolute[(FILE_MAXDIR * 2) + FILE_MAXFILE];
	
	BLI_bpathIterator_init(&bpi, basepath);
	while (!BLI_bpathIterator_isDone(&bpi)) {
		BLI_bpathIterator_getPath(&bpi, filepath);
		libpath = BLI_bpathIterator_getLib(&bpi);
		
		if(strncmp(filepath, "//", 2)==0) {
			if (libpath) { /* cant make absolute if we are library - TODO, LOG THIS */
				linked++;
			} else { /* get the expanded path and check it is relative or too long */
				BLI_bpathIterator_getPathExpanded( &bpi, filepath_absolute );
				BLI_cleanup_file(bpi.base_path, filepath_absolute); /* fix any /foo/../foo/ */
				/* to be safe, check the length */
				if (BLI_bpathIterator_getPathMaxLen(&bpi) <= strlen(filepath_absolute)) {
					bpath_as_report(&bpi, "couldn't make absolute (too long)", reports);
					failed++;
				} else {
					if(strncmp(filepath_absolute, "//", 2)) {
						BLI_bpathIterator_setPath(&bpi, filepath_absolute);
						changed++;
					} else {
						bpath_as_report(&bpi, "couldn't make absolute", reports);
						failed++;
					}
				}
			}
		}
		BLI_bpathIterator_step(&bpi);
		tot++;
	}
	BLI_bpathIterator_free(&bpi);

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
	
	dir = opendir(dirname);
	
	if (dir==0)
		return 0;
	
	if (*filesize == -1)
		*filesize = 0; /* dir opened fine */
	
	while ((de = readdir(dir)) != NULL) {
		
		if (strncmp(".", de->d_name, 2)==0 || strncmp("..", de->d_name, 3)==0)
			continue;
		
		BLI_join_dirfile(path, dirname, de->d_name);
		
		if (stat(path, &status) != 0)
			continue; /* cant stat, dont bother with this file, could print debug info here */
		
		if (S_ISREG(status.st_mode)) { /* is file */
			if (strncmp(filename, de->d_name, FILE_MAX)==0) { /* name matches */
				/* open the file to read its size */
				size = BLI_filepathsize(path);
				if ((size > 0) && (size > *filesize)) { /* find the biggest file */
					*filesize = size;
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
void findMissingFiles(char *basepath, char *str) {
	struct BPathIterator bpi;
	
	/* be sure there is low chance of the path being too short */
	char filepath_expanded[FILE_MAXDIR*2]; 
	char filepath[FILE_MAX], *libpath;
	int filesize, recur_depth;
	
	char dirname[FILE_MAX], filename[FILE_MAX], filename_new[FILE_MAX];
	
	//XXX waitcursor( 1 );
	
	BLI_split_dirfile(str, dirname, NULL);
	
	BLI_bpathIterator_init(&bpi, basepath);
	
	while (!BLI_bpathIterator_isDone(&bpi)) {
		BLI_bpathIterator_getPath(&bpi, filepath);
		libpath = BLI_bpathIterator_getLib(&bpi);
		
		/* Check if esc was pressed because searching files can be slow */
		/*XXX if (blender_test_break()) {
			break;
		}*/
		
		if (libpath==NULL) {
			
			BLI_bpathIterator_getPathExpanded( &bpi, filepath_expanded );
			
			if (!BLI_exists(filepath_expanded)) {
				/* can the dir be opened? */
				filesize = -1;
				recur_depth = 0;
				BLI_split_dirfile(filepath, NULL, filename); /* the file to find */
				
				findFileRecursive(filename_new, dirname, filename, &filesize, &recur_depth);
				if (filesize == -1) { /* could not open dir */
					printf("Could not open dir \"%s\"\n", dirname);
					return;
				}
				
				if (filesize > 0) {
					
					if (BLI_bpathIterator_getPathMaxLen( &bpi ) < strlen(filename_new)) { 
						printf("cannot set path \"%s\" too long!", filename_new);
					} else {
						/* copy the found path into the old one */
						if (G.relbase_valid)
							BLI_path_rel(filename_new, bpi.base_path);
						
						BLI_bpathIterator_setPath( &bpi, filename_new );
					}
				}
			}
		}
		BLI_bpathIterator_step(&bpi);
	}
	BLI_bpathIterator_free(&bpi);
	
	//XXX waitcursor( 0 );
}
