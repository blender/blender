/**
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Campbell barton
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "BLI_bpath.h"
#include "BKE_global.h"
#include "DNA_ID.h" /* Library */
#include "DNA_vfont_types.h"
#include "DNA_image_types.h"
#include "DNA_sound_types.h"
#include "DNA_scene_types.h" /* to get the current frame */
#include <stdlib.h>
#include <string.h>

#include "BKE_main.h" /* so we can access G.main->*.first */
#include "BKE_image.h" /* so we can check the image's type */

#include "blendef.h"
#include "BKE_utildefines.h"

/* for writing to a textblock */
#include "BKE_text.h" 
#include "BLI_blenlib.h"
#include "DNA_text_types.h"

/* path/file handeling stuff */
#ifndef WIN32
  #include <dirent.h>
  #include <unistd.h>
#else
  #include "BLI_winstuff.h"
  #include <io.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define FILE_MAX			240


/* TODO - BPATH_PLUGIN, BPATH_SEQ */
enum BPathTypes {
	BPATH_IMAGE = 0,
	BPATH_SOUND,
	BPATH_FONT,
	BPATH_LIB,

 	BPATH_DONE
};


void BLI_bpathIterator_init( struct BPathIterator *bpi ) {
	bpi->type = BPATH_IMAGE;
	bpi->data = NULL;
	BLI_bpathIterator_step(bpi);
}

char* BLI_bpathIterator_getPath( struct BPathIterator *bpi) {
	return bpi->path;
}
void BLI_bpathIterator_copyPathExpanded( struct BPathIterator *bpi, char *path_expanded) {
	char *filepath, *libpath;
	
	filepath = BLI_bpathIterator_getPath(bpi);
	libpath = BLI_bpathIterator_getLib(bpi);
	
	BLI_strncpy(path_expanded, filepath, FILE_MAXDIR*2);
	
	if (libpath) { /* check the files location relative to its library path */
		BLI_convertstringcode(path_expanded, libpath, G.scene->r.cfra);
	} else { /* local data, use the blend files path */
		BLI_convertstringcode(path_expanded, G.sce, G.scene->r.cfra);
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
static struct Image *ima_getpath__internal(struct Image *ima, int step_next) {
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

static struct VFont *vf_getpath__internal(struct VFont *vf, int step_next) {
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

static struct bSound *snd_getpath__internal(struct bSound *snd, int step_next) {
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

void BLI_bpathIterator_step( struct BPathIterator *bpi) {
	while (bpi->type != BPATH_DONE) {
		
		if  ((bpi->type) == BPATH_IMAGE) {
			/*if (bpi->data)	bpi->data = ((ID *)bpi->data)->next;*/
			if (bpi->data)	bpi->data = ima_getpath__internal( (Image *)bpi->data, 1 ); /* must skip images that have no path */
			else 			bpi->data = ima_getpath__internal(G.main->image.first, 0);
			
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
				bpi->type+=1; /* advance to the next type */
			}
			
			
		} else if  ((bpi->type) == BPATH_SOUND) {
			if (bpi->data)	bpi->data = snd_getpath__internal( (bSound *)bpi->data, 1 ); /* must skip images that have no path */
			else 			bpi->data = snd_getpath__internal(G.main->sound.first, 0);
			
			if (bpi->data) {
				/* get the path info from this datatype */
				bSound *snd = (bSound *)bpi->data;
				
				bpi->lib = snd->id.lib ? snd->id.lib->filename : NULL;
				bpi->path = snd->sample->name;
				bpi->name = snd->id.name+2;
				bpi->len = sizeof(snd->sample->name);
				
				/* we are done, advancing to the next item, this type worked fine */
				break;
			} else {
				bpi->type+=1; /* advance to the next type */
			}
			
			
		} else if  ((bpi->type) == BPATH_FONT) {
			
			if (bpi->data)	bpi->data = vf_getpath__internal( (VFont *)bpi->data, 1 );
			else 			bpi->data = vf_getpath__internal( G.main->vfont.first, 0 );
			
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
				bpi->type+=1; /* advance to the next type */
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
				bpi->type+=1; /* advance to the next type */
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
	default:
		txt_insert_buf( btxt, "Unknown \"" );
		break;
	}
	
	name = BLI_bpathIterator_getName(bpi);
	
	if (name) {
		txt_insert_buf( btxt, name );
	}
	txt_insert_buf( btxt, "\" " );
	
	BLI_bpathIterator_copyPathExpanded(bpi, path_expanded);
	
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
	char *filepath, *libpath;
	int files_missing = 0;
	
	BLI_bpathIterator_init(&bpi);
	while (!BLI_bpathIterator_isDone(&bpi)) {
		filepath = BLI_bpathIterator_getPath(&bpi);
		libpath = BLI_bpathIterator_getLib(&bpi);
		
		BLI_bpathIterator_copyPathExpanded( &bpi, filepath_expanded );
		
		if (!BLI_exists(filepath_expanded)) {
			if (!btxt) {
				btxt = add_empty_text( "missing_files.log" );
				if (txtname) {
					BLI_strncpy(txtname, btxt->id.name+2, 24);
				}
			}
			bpathToText(btxt, &bpi);
			files_missing = 1;
		}
		BLI_bpathIterator_step(&bpi);
	}
}

/* dont log any errors at the moment, should probably do this */
void makeFilesRelative(char *txtname, int *tot, int *changed, int *failed, int *linked) {
	struct BPathIterator bpi;
	char *filepath, *libpath;
	
	/* be sure there is low chance of the path being too short */
	char filepath_relative[(FILE_MAXDIR * 2) + FILE_MAXFILE];
	
	Text *btxt = NULL;
	
	*tot = *changed = *failed = *linked = 0;
	
	BLI_bpathIterator_init(&bpi);
	while (!BLI_bpathIterator_isDone(&bpi)) {
		filepath = BLI_bpathIterator_getPath(&bpi);
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
						strcpy(filepath, filepath_relative);
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
}

/* dont log any errors at the moment, should probably do this -
 * Verry similar to makeFilesRelative - keep in sync! */
void makeFilesAbsolute(char *txtname, int *tot, int *changed, int *failed, int *linked) {
	struct BPathIterator bpi;
	char *filepath, *libpath;
	
	/* be sure there is low chance of the path being too short */
	char filepath_absolute[(FILE_MAXDIR * 2) + FILE_MAXFILE];
	
	Text *btxt = NULL;
	
	*tot = *changed = *failed = *linked = 0;
	
	BLI_bpathIterator_init(&bpi);
	while (!BLI_bpathIterator_isDone(&bpi)) {
		filepath = BLI_bpathIterator_getPath(&bpi);
		libpath = BLI_bpathIterator_getLib(&bpi);
		
		if(strncmp(filepath, "//", 2)==0) {
			if (libpath) { /* cant make absolute if we are library - TODO, LOG THIS */
				(*linked)++;
			} else { /* get the expanded path and check it is relative or too long */
				BLI_bpathIterator_copyPathExpanded( &bpi, filepath_absolute );
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
						strcpy(filepath, filepath_absolute);
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
	char *filepath, *libpath;
	int filesize, recur_depth;
	
	char dirname[FILE_MAX], filename[FILE_MAX], filename_new[FILE_MAX], dummyname[FILE_MAX];
	
	BLI_split_dirfile(str, dirname, dummyname);
	
	BLI_bpathIterator_init(&bpi);
	
	while (!BLI_bpathIterator_isDone(&bpi)) {
		filepath = BLI_bpathIterator_getPath(&bpi);
		libpath = BLI_bpathIterator_getLib(&bpi);
		
		if (libpath==NULL) {
			
			BLI_bpathIterator_copyPathExpanded( &bpi, filepath_expanded );
			
			if (!BLI_exists(filepath_expanded)) {
				/* can the dir be opened? */
				filesize = -1;
				recur_depth = 0;
				BLI_split_dirfile(filepath, dummyname, filename); /* the file to find */
				
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
						
						strcpy( BLI_bpathIterator_getPath( &bpi ), filename_new );
					}
				}
			}
		}
		BLI_bpathIterator_step(&bpi);
	}
}
