/**
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */


/* global includes */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include <direct.h>
#endif   
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_storage_types.h"
#include "BLI_threads.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BLO_readfile.h"

#include "DNA_space_types.h"
#include "DNA_ipo_types.h"
#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_listBase.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "ED_datafiles.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "PIL_time.h"

#include "UI_interface.h"

#include "filelist.h"

/* Elubie: VERY, really very ugly and evil! Remove asap!!! */
/* for state of file */
#define ACTIVE				2

/* max length of library group name within filesel */
#define GROUP_MAX 32

static void *exec_loadimages(void *list_v);

struct FileList;

typedef struct FileImage {
	struct FileImage *next, *prev;
	int index;
	short lock;
	short done;
	struct FileList* filelist;
} FileImage;

typedef struct FileList
{
	struct direntry *filelist;
	int *fidx;
	int numfiles;
	int numfiltered;
	char dir[FILE_MAX];
	short prv_w;
	short prv_h;
	short hide_dot;
	unsigned int filter;
	short changed;

	struct BlendHandle *libfiledata;
	short hide_parent;

	void (*read)(struct FileList *);

	ListBase loadimages;
	ListBase threads;
} FileList;

typedef struct FolderList
{
	struct FolderList *next, *prev;
	char *foldername;
} FolderList;

#define SPECIAL_IMG_SIZE 48
#define SPECIAL_IMG_ROWS 4
#define SPECIAL_IMG_COLS 4

#define SPECIAL_IMG_FOLDER 0
#define SPECIAL_IMG_PARENT 1
#define SPECIAL_IMG_REFRESH 2
#define SPECIAL_IMG_BLENDFILE 3
#define SPECIAL_IMG_SOUNDFILE 4
#define SPECIAL_IMG_MOVIEFILE 5
#define SPECIAL_IMG_PYTHONFILE 6
#define SPECIAL_IMG_TEXTFILE 7
#define SPECIAL_IMG_FONTFILE 8
#define SPECIAL_IMG_UNKNOWNFILE 9
#define SPECIAL_IMG_LOADING 10
#define SPECIAL_IMG_MAX SPECIAL_IMG_LOADING + 1

static ImBuf* gSpecialFileImages[SPECIAL_IMG_MAX];


/* ******************* SORT ******************* */

static int compare_name(const void *a1, const void *a2)
{
	const struct direntry *entry1=a1, *entry2=a2;

	/* type is is equal to stat.st_mode */

	if (S_ISDIR(entry1->type)){
		if (S_ISDIR(entry2->type)==0) return (-1);
	} else{
		if (S_ISDIR(entry2->type)) return (1);
	}
	if (S_ISREG(entry1->type)){
		if (S_ISREG(entry2->type)==0) return (-1);
	} else{
		if (S_ISREG(entry2->type)) return (1);
	}
	if ((entry1->type & S_IFMT) < (entry2->type & S_IFMT)) return (-1);
	if ((entry1->type & S_IFMT) > (entry2->type & S_IFMT)) return (1);
	
	/* make sure "." and ".." are always first */
	if( strcmp(entry1->relname, ".")==0 ) return (-1);
	if( strcmp(entry2->relname, ".")==0 ) return (1);
	if( strcmp(entry1->relname, "..")==0 ) return (-1);
	if( strcmp(entry2->relname, "..")==0 ) return (1);
	
	return (BLI_natstrcmp(entry1->relname,entry2->relname));
}

static int compare_date(const void *a1, const void *a2)	
{
	const struct direntry *entry1=a1, *entry2=a2;
	
	/* type is equal to stat.st_mode */

	if (S_ISDIR(entry1->type)){
		if (S_ISDIR(entry2->type)==0) return (-1);
	} else{
		if (S_ISDIR(entry2->type)) return (1);
	}
	if (S_ISREG(entry1->type)){
		if (S_ISREG(entry2->type)==0) return (-1);
	} else{
		if (S_ISREG(entry2->type)) return (1);
	}
	if ((entry1->type & S_IFMT) < (entry2->type & S_IFMT)) return (-1);
	if ((entry1->type & S_IFMT) > (entry2->type & S_IFMT)) return (1);

	/* make sure "." and ".." are always first */
	if( strcmp(entry1->relname, ".")==0 ) return (-1);
	if( strcmp(entry2->relname, ".")==0 ) return (1);
	if( strcmp(entry1->relname, "..")==0 ) return (-1);
	if( strcmp(entry2->relname, "..")==0 ) return (1);
	
	if ( entry1->s.st_mtime < entry2->s.st_mtime) return 1;
	if ( entry1->s.st_mtime > entry2->s.st_mtime) return -1;
	
	else return BLI_natstrcmp(entry1->relname,entry2->relname);
}

static int compare_size(const void *a1, const void *a2)	
{
	const struct direntry *entry1=a1, *entry2=a2;

	/* type is equal to stat.st_mode */

	if (S_ISDIR(entry1->type)){
		if (S_ISDIR(entry2->type)==0) return (-1);
	} else{
		if (S_ISDIR(entry2->type)) return (1);
	}
	if (S_ISREG(entry1->type)){
		if (S_ISREG(entry2->type)==0) return (-1);
	} else{
		if (S_ISREG(entry2->type)) return (1);
	}
	if ((entry1->type & S_IFMT) < (entry2->type & S_IFMT)) return (-1);
	if ((entry1->type & S_IFMT) > (entry2->type & S_IFMT)) return (1);

	/* make sure "." and ".." are always first */
	if( strcmp(entry1->relname, ".")==0 ) return (-1);
	if( strcmp(entry2->relname, ".")==0 ) return (1);
	if( strcmp(entry1->relname, "..")==0 ) return (-1);
	if( strcmp(entry2->relname, "..")==0 ) return (1);
	
	if ( entry1->s.st_size < entry2->s.st_size) return 1;
	if ( entry1->s.st_size > entry2->s.st_size) return -1;
	else return BLI_natstrcmp(entry1->relname,entry2->relname);
}

static int compare_extension(const void *a1, const void *a2) {
	const struct direntry *entry1=a1, *entry2=a2;
	char *sufix1, *sufix2;
	char *nil="";

	if (!(sufix1= strstr (entry1->relname, ".blend.gz"))) 
		sufix1= strrchr (entry1->relname, '.');
	if (!(sufix2= strstr (entry2->relname, ".blend.gz")))
		sufix2= strrchr (entry2->relname, '.');
	if (!sufix1) sufix1= nil;
	if (!sufix2) sufix2= nil;

	/* type is is equal to stat.st_mode */

	if (S_ISDIR(entry1->type)){
		if (S_ISDIR(entry2->type)==0) return (-1);
	} else{
		if (S_ISDIR(entry2->type)) return (1);
	}
	if (S_ISREG(entry1->type)){
		if (S_ISREG(entry2->type)==0) return (-1);
	} else{
		if (S_ISREG(entry2->type)) return (1);
	}
	if ((entry1->type & S_IFMT) < (entry2->type & S_IFMT)) return (-1);
	if ((entry1->type & S_IFMT) > (entry2->type & S_IFMT)) return (1);
	
	/* make sure "." and ".." are always first */
	if( strcmp(entry1->relname, ".")==0 ) return (-1);
	if( strcmp(entry2->relname, ".")==0 ) return (1);
	if( strcmp(entry1->relname, "..")==0 ) return (-1);
	if( strcmp(entry2->relname, "..")==0 ) return (1);
	
	return (BLI_strcasecmp(sufix1, sufix2));
}

void filelist_filter(FileList* filelist)
{
	/* char dir[FILE_MAX], group[GROUP_MAX]; XXXXX */
	int num_filtered = 0;
	int i, j;
	
	if (!filelist->filelist)
		return;
	
	/* XXXXX TODO: check if the filter can be handled outside the filelist 
	if ( ( (filelist->type == FILE_LOADLIB) &&  BIF_filelist_islibrary(filelist, dir, group)) 
		|| (filelist->type == FILE_MAIN) ) {
		filelist->filter = 0;
	}
	*/

	if (!filelist->filter) {
		if (filelist->fidx) {
			MEM_freeN(filelist->fidx);
			filelist->fidx = NULL;
		}
		filelist->fidx = (int *)MEM_callocN(filelist->numfiles*sizeof(int), "filteridx");
		for (i = 0; i < filelist->numfiles; ++i) {
			filelist->fidx[i] = i;
		}
		filelist->numfiltered = filelist->numfiles;
		return;
	}

	// How many files are left after filter ?
	for (i = 0; i < filelist->numfiles; ++i) {
		if (filelist->filelist[i].flags & filelist->filter) {
			num_filtered++;
		} 
		else if (filelist->filelist[i].type & S_IFDIR) {
			if (filelist->filter & FOLDERFILE) {
				num_filtered++;
			}
		}		
	}
	
	if (filelist->fidx) {
			MEM_freeN(filelist->fidx);
			filelist->fidx = NULL;
	}
	filelist->fidx = (int *)MEM_callocN(num_filtered*sizeof(int), "filteridx");
	filelist->numfiltered = num_filtered;

	for (i = 0, j=0; i < filelist->numfiles; ++i) {
		if (filelist->filelist[i].flags & filelist->filter) {
			filelist->fidx[j++] = i;
		}
		else if (filelist->filelist[i].type & S_IFDIR) {
			if (filelist->filter & FOLDERFILE) {
				filelist->fidx[j++] = i;
			}
		}  
	}
}

void filelist_init_icons()
{
	short x, y, k;
	ImBuf *bbuf;
	ImBuf *ibuf;
	bbuf = IMB_ibImageFromMemory((int *)datatoc_prvicons, datatoc_prvicons_size, IB_rect);
	if (bbuf) {
		for (y=0; y<SPECIAL_IMG_ROWS; y++) {
			for (x=0; x<SPECIAL_IMG_COLS; x++) {
				int tile = SPECIAL_IMG_COLS*y + x; 
				if (tile < SPECIAL_IMG_MAX) {
					ibuf = IMB_allocImBuf(SPECIAL_IMG_SIZE, SPECIAL_IMG_SIZE, 32, IB_rect, 0);
					for (k=0; k<SPECIAL_IMG_SIZE; k++) {
						memcpy(&ibuf->rect[k*SPECIAL_IMG_SIZE], &bbuf->rect[(k+y*SPECIAL_IMG_SIZE)*SPECIAL_IMG_SIZE*SPECIAL_IMG_COLS+x*SPECIAL_IMG_SIZE], SPECIAL_IMG_SIZE*sizeof(int));
					}
					gSpecialFileImages[tile] = ibuf;
				}
			}
		}
		IMB_freeImBuf(bbuf);
	}
}

void filelist_free_icons()
{
	int i;
	for (i=0; i < SPECIAL_IMG_MAX; ++i) {
		IMB_freeImBuf(gSpecialFileImages[i]);
		gSpecialFileImages[i] = NULL;
	}
}

//-----------------FOLDERLIST (previous/next) --------------//
struct ListBase* folderlist_new()
{
	ListBase* p = MEM_callocN( sizeof(ListBase), "folderlist" );
	return p;
}

void folderlist_popdir(struct ListBase* folderlist, char *dir)
{
	const char *prev_dir;
	struct FolderList *folder;
	folder = folderlist->last;

	if(folder){
		// remove the current directory
		MEM_freeN(folder->foldername);
		BLI_freelinkN(folderlist, folder);

		folder = folderlist->last;
		if(folder){
			prev_dir = folder->foldername;
			BLI_strncpy(dir, prev_dir, FILE_MAXDIR);
		}
	}
	// delete the folder next or use setdir directly before PREVIOUS OP
}

void folderlist_pushdir(ListBase* folderlist, const char *dir)
{
	struct FolderList *folder, *previous_folder;
	previous_folder = folderlist->last;

	// check if already exists
	if(previous_folder && previous_folder->foldername){
		if(! strcmp(previous_folder->foldername, dir)){
			return;
		}
	}

	// create next folder element
	folder = (FolderList*)MEM_mallocN(sizeof(FolderList),"FolderList");
	folder->foldername = (char*)MEM_mallocN(sizeof(char)*(strlen(dir)+1), "foldername");
	folder->foldername[0] = '\0';

	BLI_strncpy(folder->foldername, dir, FILE_MAXDIR);

	// add it to the end of the list
	BLI_addtail(folderlist, folder);
}

int folderlist_clear_next(struct SpaceFile *sfile)
{
	struct FolderList *folder;

	// if there is no folder_next there is nothing we can clear
	if (!sfile->folders_next)
		return 0;

	// if previous_folder, next_folder or refresh_folder operators are executed it doesn't clear folder_next
	folder = sfile->folders_prev->last;
	if ((!folder) ||(!strcmp(folder->foldername, sfile->params->dir)))
		return 0;

	// eventually clear flist->folders_next
	return 1;
}

/* not listbase itself */
void folderlist_free(ListBase* folderlist)
{
	FolderList *folder;
	if (folderlist){
		for(folder= folderlist->first; folder; folder= folder->next)
			MEM_freeN(folder->foldername);
		BLI_freelistN(folderlist);
	}
	folderlist= NULL;
}

ListBase *folderlist_duplicate(ListBase* folderlist)
{
	
	if (folderlist) {
		ListBase *folderlistn= MEM_callocN(sizeof(ListBase), "copy folderlist");
		FolderList *folder;
		
		BLI_duplicatelist(folderlistn, folderlist);
		
		for(folder= folderlistn->first; folder; folder= folder->next) {
			folder->foldername= MEM_dupallocN(folder->foldername);
		}
		return folderlistn;
	}
	return NULL;
}


static void filelist_read_main(struct FileList* filelist);
static void filelist_read_library(struct FileList* filelist);
static void filelist_read_dir(struct FileList* filelist);

//------------------FILELIST------------------------//
struct FileList*	filelist_new(short type)
{
	FileList* p = MEM_callocN( sizeof(FileList), "filelist" );
	switch(type) {
		case FILE_MAIN:
			p->read = filelist_read_main;
			break;
		case FILE_LOADLIB:
			p->read = filelist_read_library;
			break;
		default:
			p->read = filelist_read_dir;

	}
	return p;
}


void filelist_free(struct FileList* filelist)
{
	int i;

	if (!filelist) {
		printf("Attempting to delete empty filelist.\n");
		return;
	}

	BLI_end_threads(&filelist->threads);
	BLI_freelistN(&filelist->loadimages);
	
	if (filelist->fidx) {
		MEM_freeN(filelist->fidx);
		filelist->fidx = NULL;
	}

	for (i = 0; i < filelist->numfiles; ++i) {
		if (filelist->filelist[i].image) {			
			IMB_freeImBuf(filelist->filelist[i].image);
		}
		filelist->filelist[i].image = 0;
		if (filelist->filelist[i].relname)
			MEM_freeN(filelist->filelist[i].relname);
		if (filelist->filelist[i].path)
			MEM_freeN(filelist->filelist[i].path);
		filelist->filelist[i].relname = 0;
		if (filelist->filelist[i].string)
			MEM_freeN(filelist->filelist[i].string);
		filelist->filelist[i].string = 0;
	}
	
	filelist->numfiles = 0;
	free(filelist->filelist);
	filelist->filelist = 0;	
	filelist->filter = 0;
	filelist->numfiltered =0;
	filelist->hide_dot =0;
}

void filelist_freelib(struct FileList* filelist)
{
	if(filelist->libfiledata)	
		BLO_blendhandle_close(filelist->libfiledata);
	filelist->libfiledata= 0;
}

struct BlendHandle *filelist_lib(struct FileList* filelist)
{
	return filelist->libfiledata;
}

int	filelist_numfiles(struct FileList* filelist)
{
	return filelist->numfiltered;
}

const char * filelist_dir(struct FileList* filelist)
{
	return filelist->dir;
}

void filelist_setdir(struct FileList* filelist, const char *dir)
{
	BLI_strncpy(filelist->dir, dir, FILE_MAX);
}

void filelist_imgsize(struct FileList* filelist, short w, short h)
{
	filelist->prv_w = w;
	filelist->prv_h = h;
}


static void *exec_loadimages(void *list_v)
{
	FileImage* img = (FileImage*)list_v;
	struct FileList *filelist = img->filelist;

	ImBuf *imb = NULL;
	int fidx = img->index;
	
	if ( filelist->filelist[fidx].flags & IMAGEFILE ) {				
		imb = IMB_thumb_manage(filelist->dir, filelist->filelist[fidx].relname, THB_NORMAL, THB_SOURCE_IMAGE);
	} else if ( filelist->filelist[fidx].flags & MOVIEFILE ) {				
		imb = IMB_thumb_manage(filelist->dir, filelist->filelist[fidx].relname, THB_NORMAL, THB_SOURCE_MOVIE);
		if (!imb) {
			/* remember that file can't be loaded via IMB_open_anim */
			filelist->filelist[fidx].flags &= ~MOVIEFILE;
			filelist->filelist[fidx].flags |= MOVIEFILE_ICON;
		}
	}
	if (imb) {
		IMB_freeImBuf(imb);
	}
	img->done=1;
	return 0;
}

short filelist_changed(struct FileList* filelist)
{
	return filelist->changed;
}

void filelist_loadimage_timer(struct FileList* filelist)
{
	FileImage *limg = filelist->loadimages.first;
	short refresh=0;

	// as long as threads are available and there is work to do
	while (limg) {
		if (BLI_available_threads(&filelist->threads)>0) {
			if (!limg->lock) {
				limg->lock=1;
				BLI_insert_thread(&filelist->threads, limg);
			}
		}
		if (limg->done) {
			FileImage *oimg = limg;
			BLI_remove_thread(&filelist->threads, oimg);
			/* brecht: keep failed images in the list, otherwise
			   it keeps trying to load them over and over?
			BLI_remlink(&filelist->loadimages, oimg);
			MEM_freeN(oimg);*/
			limg = oimg->next;
			refresh = 1;
		} else {
			limg= limg->next;
		}
	}
	filelist->changed=refresh;
}

void filelist_loadimage(struct FileList* filelist, int index)
{
	ImBuf *imb = NULL;
	int imgwidth = filelist->prv_w;
	int imgheight = filelist->prv_h;
	short ex, ey, dx, dy;
	float scaledx, scaledy;
	int fidx = 0;
	
	if ( (index < 0) || (index >= filelist->numfiltered) ) {
		return;
	}
	fidx = filelist->fidx[index];

	if (!filelist->filelist[fidx].image)
	{

		if ( (filelist->filelist[fidx].flags & IMAGEFILE) || (filelist->filelist[fidx].flags & MOVIEFILE) ) {				
			imb = IMB_thumb_read(filelist->dir, filelist->filelist[fidx].relname, THB_NORMAL);
		} 
		if (imb) {
			if (imb->x > imb->y) {
				scaledx = (float)imgwidth;
				scaledy =  ( (float)imb->y/(float)imb->x )*imgwidth;
			}
			else {
				scaledy = (float)imgheight;
				scaledx =  ( (float)imb->x/(float)imb->y )*imgheight;
			}
			ex = (short)scaledx;
			ey = (short)scaledy;
			
			dx = imgwidth - ex;
			dy = imgheight - ey;
			
			// IMB_scaleImBuf(imb, ex, ey);
			filelist->filelist[fidx].image = imb;
		} else {
			/* prevent loading image twice */
			FileImage* limg = filelist->loadimages.first;
			short found= 0;
			while(limg) {
				if (limg->index == fidx) {
					found= 1;
					break;
				}
				limg= limg->next;
			}
			if (!found) {
				FileImage* limg = MEM_callocN(sizeof(struct FileImage), "loadimage");
				limg->index= fidx;
				limg->lock= 0;
				limg->filelist= filelist;
				BLI_addtail(&filelist->loadimages, limg);
			}
		}		
	}
}

struct ImBuf * filelist_getimage(struct FileList* filelist, int index)
{
	ImBuf* ibuf = NULL;
	int fidx = 0;	
	if ( (index < 0) || (index >= filelist->numfiltered) ) {
		return NULL;
	}
	fidx = filelist->fidx[index];
	ibuf = filelist->filelist[fidx].image;

	return ibuf;
}

struct ImBuf * filelist_geticon(struct FileList* filelist, int index)
{
	ImBuf* ibuf= NULL;
	struct direntry *file= NULL;
	int fidx = 0;	
	if ( (index < 0) || (index >= filelist->numfiltered) ) {
		return NULL;
	}
	fidx = filelist->fidx[index];
	file = &filelist->filelist[fidx];
	if (file->type & S_IFDIR) {
			if ( strcmp(filelist->filelist[fidx].relname, "..") == 0) {
				ibuf = gSpecialFileImages[SPECIAL_IMG_PARENT];
			} else if  ( strcmp(filelist->filelist[fidx].relname, ".") == 0) {
				ibuf = gSpecialFileImages[SPECIAL_IMG_REFRESH];
			} else {
		ibuf = gSpecialFileImages[SPECIAL_IMG_FOLDER];
			}
	} else {
		ibuf = gSpecialFileImages[SPECIAL_IMG_UNKNOWNFILE];
	}

	if (file->flags & BLENDERFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_BLENDFILE];
	} else if ( (file->flags & MOVIEFILE) || (file->flags & MOVIEFILE_ICON) ) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_MOVIEFILE];
	} else if (file->flags & SOUNDFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_SOUNDFILE];
	} else if (file->flags & PYSCRIPTFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_PYTHONFILE];
	} else if (file->flags & FTFONTFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_FONTFILE];
	} else if (file->flags & TEXTFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_TEXTFILE];
	} else if (file->flags & IMAGEFILE) {
		ibuf = gSpecialFileImages[SPECIAL_IMG_LOADING];
	}

	return ibuf;
}

struct direntry * filelist_file(struct FileList* filelist, int index)
{
	int fidx = 0;
	
	if ( (index < 0) || (index >= filelist->numfiltered) ) {
		return NULL;
	}
	fidx = filelist->fidx[index];

	return &filelist->filelist[fidx];
}

int filelist_find(struct FileList* filelist, char *file)
{
	int index = -1;
	int i;
	int fidx = -1;
	
	if (!filelist->fidx) 
		return fidx;

	
	for (i = 0; i < filelist->numfiles; ++i) {
		if ( strcmp(filelist->filelist[i].relname, file) == 0) {
			index = i;
			break;
		}
	}

	for (i = 0; i < filelist->numfiltered; ++i) {
		if (filelist->fidx[i] == index) {
			fidx = i;
			break;
		}
	}
	return fidx;
}

void filelist_hidedot(struct FileList* filelist, short hide)
{
	filelist->hide_dot = hide;
}

void filelist_setfilter(struct FileList* filelist, unsigned int filter)
{
	filelist->filter = filter;
}

static void filelist_read_dir(struct FileList* filelist)
{
	char wdir[FILE_MAX];
	if (!filelist) return;

	filelist->fidx = 0;
	filelist->filelist = 0;

	BLI_getwdN(wdir);	 

	BLI_cleanup_dir(G.sce, filelist->dir);
	BLI_hide_dot_files(filelist->hide_dot);
	filelist->numfiles = BLI_getdir(filelist->dir, &(filelist->filelist));

	chdir(wdir);
	filelist_setfiletypes(filelist, G.have_quicktime);
	filelist_filter(filelist);

	if (!filelist->threads.first) {
		BLI_init_threads(&filelist->threads, exec_loadimages, 2);
	}
}

static void filelist_read_main(struct FileList* filelist)
{
	if (!filelist) return;
	filelist_from_main(filelist);
}

static void filelist_read_library(struct FileList* filelist)
{
	if (!filelist) return;
	BLI_cleanup_dir(G.sce, filelist->dir);
	filelist_from_library(filelist);
	if(!filelist->libfiledata) {
		int num;
		struct direntry *file;

		BLI_make_exist(filelist->dir);
		filelist_read_dir(filelist);
		file = filelist->filelist;
		for(num=0; num<filelist->numfiles; num++, file++) {
			if(BLO_has_bfile_extension(file->relname)) {
				char name[FILE_MAXDIR+FILE_MAXFILE];
			
				BLI_strncpy(name, filelist->dir, sizeof(name));
				strcat(name, file->relname);
				
				/* prevent current file being used as acceptable dir */
				if (BLI_streq(G.main->name, name)==0) {
					file->type &= ~S_IFMT;
					file->type |= S_IFDIR;
				}
			}
		}
	}
}

void filelist_readdir(struct FileList* filelist)
{
	filelist->read(filelist);
}

int filelist_empty(struct FileList* filelist)
{	
	return filelist->filelist == 0;
}

void filelist_parent(struct FileList* filelist)
{
	BLI_parent_dir(filelist->dir);
	BLI_make_exist(filelist->dir);
	filelist_readdir(filelist);
}

void filelist_setfiletypes(struct FileList* filelist, short has_quicktime)
{
	struct direntry *file;
	int num;

	file= filelist->filelist;

	for(num=0; num<filelist->numfiles; num++, file++) {
		file->flags= 0;
		file->type= file->s.st_mode;	/* restore the mess below */ 

			/* Don't check extensions for directories */ 
		if (file->type & S_IFDIR)
			continue;
				
		
		
		if(BLO_has_bfile_extension(file->relname)) {
			file->flags |= BLENDERFILE;
		} else if(BLI_testextensie(file->relname, ".py")) {
				file->flags |= PYSCRIPTFILE;
		} else if(BLI_testextensie(file->relname, ".txt")
					|| BLI_testextensie(file->relname, ".glsl")
					|| BLI_testextensie(file->relname, ".data")) {
				file->flags |= TEXTFILE;
		} else if( BLI_testextensie(file->relname, ".ttf")
					|| BLI_testextensie(file->relname, ".ttc")
					|| BLI_testextensie(file->relname, ".pfb")
					|| BLI_testextensie(file->relname, ".otf")
					|| BLI_testextensie(file->relname, ".otc")) {
				file->flags |= FTFONTFILE;			
		} else if(BLI_testextensie(file->relname, ".btx")) {
				file->flags |= BTXFILE;
		} else if (has_quicktime){
			if(		BLI_testextensie(file->relname, ".int")
				||  BLI_testextensie(file->relname, ".inta")
				||  BLI_testextensie(file->relname, ".jpg")
#ifdef WITH_OPENJPEG
				||  BLI_testextensie(file->relname, ".jp2")
#endif
				||	BLI_testextensie(file->relname, ".jpeg")
				||	BLI_testextensie(file->relname, ".tga")
				||	BLI_testextensie(file->relname, ".rgb")
				||	BLI_testextensie(file->relname, ".rgba")
				||	BLI_testextensie(file->relname, ".bmp")
				||	BLI_testextensie(file->relname, ".png")
				||	BLI_testextensie(file->relname, ".iff")
				||	BLI_testextensie(file->relname, ".lbm")
				||	BLI_testextensie(file->relname, ".gif")
				||	BLI_testextensie(file->relname, ".psd")
				||	BLI_testextensie(file->relname, ".tif")
				||	BLI_testextensie(file->relname, ".tiff")
				||	BLI_testextensie(file->relname, ".pct")
				||	BLI_testextensie(file->relname, ".pict")
				||	BLI_testextensie(file->relname, ".pntg") //macpaint
				||	BLI_testextensie(file->relname, ".qtif")
				||	BLI_testextensie(file->relname, ".sgi")
				||	BLI_testextensie(file->relname, ".hdr")
#ifdef WITH_DDS
				||	BLI_testextensie(file->relname, ".dds")
#endif
#ifdef WITH_OPENEXR
				||	BLI_testextensie(file->relname, ".exr")
#endif
			    ) {
				file->flags |= IMAGEFILE;			
			}
			else if(BLI_testextensie(file->relname, ".avi")
				||	BLI_testextensie(file->relname, ".flc")
				||	BLI_testextensie(file->relname, ".mov")
				||	BLI_testextensie(file->relname, ".movie")
				||	BLI_testextensie(file->relname, ".mp4")
				||	BLI_testextensie(file->relname, ".m4v")
				||	BLI_testextensie(file->relname, ".mv")
				||	BLI_testextensie(file->relname, ".wmv")
				||	BLI_testextensie(file->relname, ".ogv")
				||	BLI_testextensie(file->relname, ".mpeg")
				||	BLI_testextensie(file->relname, ".mpg")
				||	BLI_testextensie(file->relname, ".mpg2")
				||	BLI_testextensie(file->relname, ".vob")
				||	BLI_testextensie(file->relname, ".mkv")
				||	BLI_testextensie(file->relname, ".flv")
				||	BLI_testextensie(file->relname, ".divx")
				||	BLI_testextensie(file->relname, ".xvid")) {
				file->flags |= MOVIEFILE;			
			}
			else if(BLI_testextensie(file->relname, ".wav")
				||	BLI_testextensie(file->relname, ".ogg")
				||	BLI_testextensie(file->relname, ".oga")
				||	BLI_testextensie(file->relname, ".mp3")
				||	BLI_testextensie(file->relname, ".mp2")
				||	BLI_testextensie(file->relname, ".ac3")
				||	BLI_testextensie(file->relname, ".aac")
				||	BLI_testextensie(file->relname, ".flac")
				||	BLI_testextensie(file->relname, ".wma")
				||	BLI_testextensie(file->relname, ".eac3")) {
				file->flags |= SOUNDFILE;
			}
		} else { // no quicktime
			if(BLI_testextensie(file->relname, ".int")
				||	BLI_testextensie(file->relname, ".inta")
				||	BLI_testextensie(file->relname, ".jpg")
				||  BLI_testextensie(file->relname, ".jpeg")
#ifdef WITH_OPENJPEG
				||  BLI_testextensie(file->relname, ".jp2")
#endif
				||	BLI_testextensie(file->relname, ".tga")
				||	BLI_testextensie(file->relname, ".rgb")
				||	BLI_testextensie(file->relname, ".rgba")
				||	BLI_testextensie(file->relname, ".bmp")
				||	BLI_testextensie(file->relname, ".png")
				||	BLI_testextensie(file->relname, ".iff")
				||	BLI_testextensie(file->relname, ".tif")
				||	BLI_testextensie(file->relname, ".tiff")
				||	BLI_testextensie(file->relname, ".hdr")
#ifdef WITH_DDS
				||	BLI_testextensie(file->relname, ".dds")
#endif
#ifdef WITH_OPENEXR
				||	BLI_testextensie(file->relname, ".exr")
#endif
				||	BLI_testextensie(file->relname, ".lbm")
				||	BLI_testextensie(file->relname, ".sgi")) {
				file->flags |= IMAGEFILE;			
			}
			else if(BLI_testextensie(file->relname, ".avi")
				||	BLI_testextensie(file->relname, ".flc")
				||	BLI_testextensie(file->relname, ".mov")
				||	BLI_testextensie(file->relname, ".movie")
				||	BLI_testextensie(file->relname, ".mp4")
				||	BLI_testextensie(file->relname, ".m4v")
				||	BLI_testextensie(file->relname, ".mv")
				||	BLI_testextensie(file->relname, ".wmv")
				||	BLI_testextensie(file->relname, ".ogv")
				||	BLI_testextensie(file->relname, ".mpeg")
				||	BLI_testextensie(file->relname, ".mpg")
				||	BLI_testextensie(file->relname, ".mpg2")
				||	BLI_testextensie(file->relname, ".vob")
				||	BLI_testextensie(file->relname, ".mkv")
				||	BLI_testextensie(file->relname, ".flv")
				||	BLI_testextensie(file->relname, ".divx")
				||	BLI_testextensie(file->relname, ".xvid")) {
				file->flags |= MOVIEFILE;			
			}
			else if(BLI_testextensie(file->relname, ".wav")
				||	BLI_testextensie(file->relname, ".ogg")
				||	BLI_testextensie(file->relname, ".oga")
				||	BLI_testextensie(file->relname, ".mp3")
				||	BLI_testextensie(file->relname, ".mp2")
				||	BLI_testextensie(file->relname, ".ac3")
				||	BLI_testextensie(file->relname, ".aac")
				||	BLI_testextensie(file->relname, ".flac")
				||	BLI_testextensie(file->relname, ".wma")
				||	BLI_testextensie(file->relname, ".eac3")) {
				file->flags |= SOUNDFILE;
			}
		}
	}
}

void filelist_swapselect(struct FileList* filelist)
{
	struct direntry *file;
	int num, act= 0;
	
	file= filelist->filelist;
	for(num=0; num<filelist->numfiles; num++, file++) {
		if(file->flags & ACTIVE) {
			act= 1;
			break;
		}
	}
	file= filelist->filelist+2;
	for(num=2; num<filelist->numfiles; num++, file++) {
		if(act) file->flags &= ~ACTIVE;
		else file->flags |= ACTIVE;
	}
}

void filelist_sort(struct FileList* filelist, short sort)
{
	switch(sort) {
	case FILE_SORT_ALPHA:
		qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_name);	
		break;
	case FILE_SORT_TIME:
		qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_date);	
		break;
	case FILE_SORT_SIZE:
		qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_size);	
		break;
	case FILE_SORT_EXTENSION:
		qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_extension);	
	}

	filelist_filter(filelist);
}


int filelist_islibrary(struct FileList* filelist, char* dir, char* group)
{
	return BLO_is_a_library(filelist->dir, dir, group);
}

static int groupname_to_code(char *group)
{
	char buf[32];
	char *lslash;
	
	BLI_strncpy(buf, group, 31);
	lslash= BLI_last_slash(buf);
	if (lslash)
		lslash[0]= '\0';

	return BLO_idcode_from_name(buf);
}

void filelist_from_library(struct FileList* filelist)
{
	LinkNode *l, *names, *previews;
	struct ImBuf* ima;
	int ok, i, nnames, idcode;
	char filename[FILE_MAXDIR+FILE_MAXFILE];
	char dir[FILE_MAX], group[GROUP_MAX];	
	
	/* name test */
	ok= filelist_islibrary(filelist, dir, group);
	if (!ok) {
		/* free */
		if(filelist->libfiledata) BLO_blendhandle_close(filelist->libfiledata);
		filelist->libfiledata= 0;
		return;
	}
	
	BLI_strncpy(filename, G.sce, sizeof(filename));	// G.sce = last file loaded, for UI

	/* there we go */
	/* for the time being only read filedata when libfiledata==0 */
	if (filelist->libfiledata==0) {
		filelist->libfiledata= BLO_blendhandle_from_file(dir);
		if(filelist->libfiledata==0) return;
	}
	
	idcode= groupname_to_code(group);

		// memory for strings is passed into filelist[i].relname
		// and free'd in freefilelist
	previews = NULL;
	if (idcode) {
		previews= BLO_blendhandle_get_previews(filelist->libfiledata, idcode);
		names= BLO_blendhandle_get_datablock_names(filelist->libfiledata, idcode);
		/* ugh, no rewind, need to reopen */
		BLO_blendhandle_close(filelist->libfiledata);
		filelist->libfiledata= BLO_blendhandle_from_file(dir);
		
	} else {
		names= BLO_blendhandle_get_linkable_groups(filelist->libfiledata);
	}
	
	nnames= BLI_linklist_length(names);

	filelist->numfiles= nnames + 1;
	filelist->filelist= malloc(filelist->numfiles * sizeof(*filelist->filelist));
	memset(filelist->filelist, 0, filelist->numfiles * sizeof(*filelist->filelist));

	filelist->filelist[0].relname= BLI_strdup("..");
	filelist->filelist[0].type |= S_IFDIR;
		
	for (i=0, l= names; i<nnames; i++, l= l->next) {
		char *blockname= l->link;

		filelist->filelist[i + 1].relname= BLI_strdup(blockname);
		if (!idcode)
			filelist->filelist[i + 1].type |= S_IFDIR;
	}
	
	if(previews) {
		for (i=0, l= previews; i<nnames; i++, l= l->next) {
			PreviewImage *img= l->link;
			
			if (img) {
				unsigned int w = img->w[PREVIEW_MIPMAP_LARGE];
				unsigned int h = img->h[PREVIEW_MIPMAP_LARGE];
				unsigned int *rect = img->rect[PREVIEW_MIPMAP_LARGE];

				/* first allocate imbuf for copying preview into it */
				if (w > 0 && h > 0 && rect) {
					ima = IMB_allocImBuf(w, h, 32, IB_rect, 0);
					memcpy(ima->rect, rect, w*h*sizeof(unsigned int));
					filelist->filelist[i + 1].image = ima;
					filelist->filelist[i + 1].flags = IMAGEFILE;
				}
			}
		}
	}

	BLI_linklist_free(names, free);
	if (previews) BLI_linklist_free(previews, (void(*)(void*)) MEM_freeN);

	filelist_sort(filelist, FILE_SORT_ALPHA);

	BLI_strncpy(G.sce, filename, sizeof(filename));	// prevent G.sce to change

	filelist->filter = 0;
	filelist_filter(filelist);
}

void filelist_hideparent(struct FileList* filelist, short hide)
{
	filelist->hide_parent = hide;
}

void filelist_from_main(struct FileList *filelist)
{
	ID *id;
	struct direntry *files, *firstlib = NULL;
	ListBase *lb;
	int a, fake, idcode, ok, totlib, totbl;
	
	// filelist->type = FILE_MAIN; // XXXXX TODO: add modes to filebrowser

	if(filelist->dir[0]=='/') filelist->dir[0]= 0;
	
	if(filelist->dir[0]) {
		idcode= groupname_to_code(filelist->dir);
		if(idcode==0) filelist->dir[0]= 0;
	}
	
	if( filelist->dir[0]==0) {
		
		/* make directories */
		filelist->numfiles= 23;
		filelist->filelist= (struct direntry *)malloc(filelist->numfiles * sizeof(struct direntry));
		
		for(a=0; a<filelist->numfiles; a++) {
			memset( &(filelist->filelist[a]), 0 , sizeof(struct direntry));
			filelist->filelist[a].type |= S_IFDIR;
		}
		
		filelist->filelist[0].relname= BLI_strdup("..");
		filelist->filelist[2].relname= BLI_strdup("Scene");
		filelist->filelist[3].relname= BLI_strdup("Object");
		filelist->filelist[4].relname= BLI_strdup("Mesh");
		filelist->filelist[5].relname= BLI_strdup("Curve");
		filelist->filelist[6].relname= BLI_strdup("Metaball");
		filelist->filelist[7].relname= BLI_strdup("Material");
		filelist->filelist[8].relname= BLI_strdup("Texture");
		filelist->filelist[9].relname= BLI_strdup("Image");
		filelist->filelist[10].relname= BLI_strdup("Ika");
		filelist->filelist[11].relname= BLI_strdup("Wave");
		filelist->filelist[12].relname= BLI_strdup("Lattice");
		filelist->filelist[13].relname= BLI_strdup("Lamp");
		filelist->filelist[14].relname= BLI_strdup("Camera");
		filelist->filelist[15].relname= BLI_strdup("Ipo");
		filelist->filelist[16].relname= BLI_strdup("World");
		filelist->filelist[17].relname= BLI_strdup("Screen");
		filelist->filelist[18].relname= BLI_strdup("VFont");
		filelist->filelist[19].relname= BLI_strdup("Text");
		filelist->filelist[20].relname= BLI_strdup("Armature");
		filelist->filelist[21].relname= BLI_strdup("Action");
		filelist->filelist[22].relname= BLI_strdup("NodeTree");
		filelist_sort(filelist, FILE_SORT_ALPHA);
	}
	else {

		/* make files */
		idcode= groupname_to_code(filelist->dir);
		
		lb= wich_libbase(G.main, idcode );
		if(lb==0) return;
		
		id= lb->first;
		filelist->numfiles= 0;
		while(id) {
			if (!filelist->hide_dot || id->name[2] != '.') {
				filelist->numfiles++;
			}
			
			id= id->next;
		}
		
		/* XXXXX TODO: if databrowse F4 or append/link filelist->hide_parent has to be set */
		if (!filelist->hide_parent) filelist->numfiles+= 1;
		filelist->filelist= (struct direntry *)malloc(filelist->numfiles * sizeof(struct direntry));
		
		files = filelist->filelist;
		
		if (!filelist->hide_parent) {
			memset( &(filelist->filelist[0]), 0 , sizeof(struct direntry));
			filelist->filelist[0].relname= BLI_strdup("..");
			filelist->filelist[0].type |= S_IFDIR;
		
			files++;
		}
		
		id= lb->first;
		totlib= totbl= 0;
		
		while(id) {
			ok = 1;
			if(ok) {
				if (!filelist->hide_dot || id->name[2] != '.') {
					memset( files, 0 , sizeof(struct direntry));
					if(id->lib==NULL)
						files->relname= BLI_strdup(id->name+2);
					else {
						files->relname= MEM_mallocN(FILE_MAXDIR+FILE_MAXFILE+32, "filename for lib");
						sprintf(files->relname, "%s | %s", id->lib->name, id->name+2);
					}
					/* files->type |= S_IFDIR; */
#if 0				// XXXXX TODO show the selection status of the objects
					if(!filelist->has_func) { /* F4 DATA BROWSE */
						if(idcode==ID_OB) {
							if( ((Object *)id)->flag & SELECT) files->flags |= ACTIVE;
						}
						else if(idcode==ID_SCE) {
							if( ((Scene *)id)->r.scemode & R_BG_RENDER) files->flags |= ACTIVE;
						}					
					}
#endif
					files->nr= totbl+1;
					files->poin= id;
					fake= id->flag & LIB_FAKEUSER;
					if(idcode == ID_MA || idcode == ID_TE || idcode == ID_LA || idcode == ID_WO || idcode == ID_IM) {
						files->flags |= IMAGEFILE;
					}
					if(id->lib && fake) sprintf(files->extra, "LF %d", id->us);
					else if(id->lib) sprintf(files->extra, "L    %d", id->us);
					else if(fake) sprintf(files->extra, "F    %d", id->us);
					else sprintf(files->extra, "      %d", id->us);
					
					if(id->lib) {
						if(totlib==0) firstlib= files;
						totlib++;
					}
					
					files++;
				}
				totbl++;
			}
			
			id= id->next;
		}
		
		/* only qsort of library blocks */
		if(totlib>1) {
			qsort(firstlib, totlib, sizeof(struct direntry), compare_name);
		}
	}
	filelist->filter = 0;
	filelist_filter(filelist);
}

