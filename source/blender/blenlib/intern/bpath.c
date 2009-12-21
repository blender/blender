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
#include "DNA_vfont_types.h"
#include "DNA_image_types.h"
#include "DNA_sound_types.h"
#include "DNA_scene_types.h" /* to get the current frame */
#include "DNA_sequence_types.h"
#include "DNA_text_types.h"

#include "BLI_blenlib.h"
#include "BLI_bpath.h"

#include "BKE_global.h"
#include "BKE_image.h" /* so we can check the image's type */
#include "BKE_main.h" /* so we can access G.main->*.first */
#include "BKE_sequencer.h"
#include "BKE_text.h" /* for writing to a textblock */
#include "BKE_utildefines.h"

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

 	BPATH_DONE
};

void BLI_bpathIterator_init( struct BPathIterator *bpi ) {
	bpi->type = BPATH_IMAGE;
	bpi->data = NULL;
	
	bpi->getpath_callback = NULL;
	bpi->setpath_callback = NULL;
	
	/* Sequencer spesific */
	bpi->seqdata.totseq = 0;
	bpi->seqdata.seq = 0;
	bpi->seqdata.seqar = NULL;
	bpi->seqdata.scene = NULL;
	
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
		BLI_convertstringcode(path_expanded, libpath);
	} else { /* local data, use the blend files path */
		BLI_convertstringcode(path_expanded, G.sce);
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
			BLI_split_dirfile_basic(path, seq->strip->dir, seq->strip->stripdata->name);
		} else {
			/* simple case */
			BLI_strncpy(seq->strip->dir, path, sizeof(seq->strip->dir));
		}
	}
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
		}
	}
}

int BLI_bpathIterator_isDone( struct BPathIterator *bpi) {
	return bpi->type==BPATH_DONE;
}

/* include the path argument */
static void bpathToText(Text *btxt, struct BPathIterator *bpi)
{
	char *name;
	char path_expanded[FILE_MAXDIR*2];
	
	switch(BLI_bpathIterator_getType(bpi)) {
	case BPATH_IMAGE:
		txt_insert_buf( btxt, "Image \"" );
		break;
	case BPATH_SOUND:
		txt_insert_buf( btxt, "Sound \"" );
		break;
	case BPATH_FONT:
		txt_insert_buf( btxt, "Font \"" );
		break;
	case BPATH_LIB:
		txt_insert_buf( btxt, "Library \"" );
		break;
	case BPATH_SEQ:
		txt_insert_buf( btxt, "Sequence \"" );
		break;
	default:
		txt_insert_buf( btxt, "Unknown \"" );
		break;
	}
	
	name = BLI_bpathIterator_getName(bpi);
	
	if (name) {
		txt_insert_buf( btxt, name );
	}
	txt_insert_buf( btxt, "\" " );
	
	BLI_bpathIterator_getPathExpanded(bpi, path_expanded);
	
	txt_insert_buf( btxt, path_expanded );
	txt_insert_buf( btxt, "\n" );
	txt_move_eof( btxt, 0 );
}

/* high level function */
void checkMissingFiles( char *txtname ) {
	Text *btxt = NULL;
	struct BPathIterator bpi;
	
	/* be sure there is low chance of the path being too short */
	char filepath_expanded[FILE_MAXDIR*2]; 
	
	BLI_bpathIterator_init(&bpi);
	while (!BLI_bpathIterator_isDone(&bpi)) {
		BLI_bpathIterator_getPathExpanded( &bpi, filepath_expanded );
		
		if (!BLI_exists(filepath_expanded)) {
			if (!btxt) {
				btxt = add_empty_text( "missing_files.log" );
				if (txtname) {
					BLI_strncpy(txtname, btxt->id.name+2, 24);
				}
			}
			bpathToText(btxt, &bpi);
		}
		BLI_bpathIterator_step(&bpi);
	}
	BLI_bpathIterator_free(&bpi);
}

/* dont log any errors at the moment, should probably do this */
void makeFilesRelative(char *txtname, int *tot, int *changed, int *failed, int *linked) {
	struct BPathIterator bpi;
	char filepath[FILE_MAX], *libpath;
	
	/* be sure there is low chance of the path being too short */
	char filepath_relative[(FILE_MAXDIR * 2) + FILE_MAXFILE];
	
	Text *btxt = NULL;
	
	*tot = *changed = *failed = *linked = 0;
	
	BLI_bpathIterator_init(&bpi);
	while (!BLI_bpathIterator_isDone(&bpi)) {
		BLI_bpathIterator_getPath(&bpi, filepath);
		libpath = BLI_bpathIterator_getLib(&bpi);
		
		if(strncmp(filepath, "//", 2)) {
			if (libpath) { /* cant make relative if we are library - TODO, LOG THIS */
				(*linked)++;
			} else { /* local data, use the blend files path */
				BLI_strncpy(filepath_relative, filepath, sizeof(filepath_relative));
				/* Important BLI_cleanup_dir runs before the path is made relative
				 * because it wont work for paths that start with "//../" */ 
				BLI_cleanup_file(G.sce, filepath_relative); /* fix any /foo/../foo/ */
				BLI_makestringcode(G.sce, filepath_relative);
				/* be safe and check the length */
				if (BLI_bpathIterator_getPathMaxLen(&bpi) <= strlen(filepath_relative)) {
					if (!btxt) {
						btxt = add_empty_text( "missing_no_rel.log" );
						if (txtname) {
							BLI_strncpy(txtname, btxt->id.name+2, 24);
						}
					}
					bpathToText(btxt, &bpi);
					(*failed)++;
				} else {
					if(strncmp(filepath_relative, "//", 2)==0) {
						BLI_bpathIterator_setPath(&bpi, filepath_relative);
						(*changed)++;
					} else {
						if (!btxt) {
							btxt = add_empty_text( "missing_no_rel.log" );
							if (txtname) {
								BLI_strncpy(txtname, btxt->id.name+2, 24);
							}
						}
						bpathToText(btxt, &bpi);
						(*failed)++;
					}
				}
			}
		}
		BLI_bpathIterator_step(&bpi);
		(*tot)++;
	}
	BLI_bpathIterator_free(&bpi);
}

/* dont log any errors at the moment, should probably do this -
 * Verry similar to makeFilesRelative - keep in sync! */
void makeFilesAbsolute(char *txtname, int *tot, int *changed, int *failed, int *linked) {
	struct BPathIterator bpi;
	char filepath[FILE_MAX], *libpath;
	
	/* be sure there is low chance of the path being too short */
	char filepath_absolute[(FILE_MAXDIR * 2) + FILE_MAXFILE];
	
	Text *btxt = NULL;
	
	*tot = *changed = *failed = *linked = 0;
	
	BLI_bpathIterator_init(&bpi);
	while (!BLI_bpathIterator_isDone(&bpi)) {
		BLI_bpathIterator_getPath(&bpi, filepath);
		libpath = BLI_bpathIterator_getLib(&bpi);
		
		if(strncmp(filepath, "//", 2)==0) {
			if (libpath) { /* cant make absolute if we are library - TODO, LOG THIS */
				(*linked)++;
			} else { /* get the expanded path and check it is relative or too long */
				BLI_bpathIterator_getPathExpanded( &bpi, filepath_absolute );
				BLI_cleanup_file(G.sce, filepath_absolute); /* fix any /foo/../foo/ */
				/* to be safe, check the length */
				if (BLI_bpathIterator_getPathMaxLen(&bpi) <= strlen(filepath_absolute)) {
					if (!btxt) {
						btxt = add_empty_text( "missing_no_abs.log" );
						if (txtname) {
							BLI_strncpy(txtname, btxt->id.name+2, 24);
						}
					}
					bpathToText(btxt, &bpi);
					(*failed)++;
				} else {
					if(strncmp(filepath_absolute, "//", 2)) {
						BLI_bpathIterator_setPath(&bpi, filepath_absolute);
						(*changed)++;
					} else {
						if (!btxt) {
							btxt = add_empty_text( "missing_no_abs.log" );
							if (txtname) {
								BLI_strncpy(txtname, btxt->id.name+2, 24);
							}
						}
						bpathToText(btxt, &bpi);
						(*failed)++;
					}
				}
			}
		}
		BLI_bpathIterator_step(&bpi);
		(*tot)++;
	}
	BLI_bpathIterator_free(&bpi);
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
void findMissingFiles(char *str) {
	struct BPathIterator bpi;
	
	/* be sure there is low chance of the path being too short */
	char filepath_expanded[FILE_MAXDIR*2]; 
	char filepath[FILE_MAX], *libpath;
	int filesize, recur_depth;
	
	char dirname[FILE_MAX], filename[FILE_MAX], filename_new[FILE_MAX];
	
	//XXX waitcursor( 1 );
	
	BLI_split_dirfile_basic(str, dirname, NULL);
	
	BLI_bpathIterator_init(&bpi);
	
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
				BLI_split_dirfile_basic(filepath, NULL, filename); /* the file to find */
				
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
							BLI_makestringcode(G.sce, filename_new);
						
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
