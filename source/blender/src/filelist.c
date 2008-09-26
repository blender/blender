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

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BIF_filelist.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BLO_readfile.h"

#include "DNA_space_types.h"
#include "DNA_ipo_types.h"
#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "PIL_time.h"

#include "datatoc.h"

/* Elubie: VERY, really very ugly and evil! Remove asap!!! */
/* for state of file */
#define ACTIVE				2

/* max length of library group name within filesel */
#define GROUP_MAX 32

typedef struct FileList
{
	struct direntry *filelist;
	int *fidx;

	int numfiles;
	int numfiltered;
	char dir[FILE_MAX];
	short type;
	short ipotype;
	struct BlendHandle *libfiledata;
	int has_func;
	short prv_w;
	short prv_h;
	short hide_dot;
	unsigned int filter;
} FileList;

int BIF_groupname_to_code(char *group)
{
	char buf[32];
	char *lslash;
	
	BLI_strncpy(buf, group, 31);
	lslash= BLI_last_slash(buf);
	if (lslash)
		lslash[0]= '\0';

	return BLO_idcode_from_name(buf);
}


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
#define SPECIAL_IMG_MAX SPECIAL_IMG_UNKNOWNFILE + 1

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
	
	return (BLI_strcasecmp(entry1->relname,entry2->relname));
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
	
	if ( entry1->s.st_mtime < entry2->s.st_mtime) return 1;
	if ( entry1->s.st_mtime > entry2->s.st_mtime) return -1;
	
	else return BLI_strcasecmp(entry1->relname,entry2->relname);
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
	
	if ( entry1->s.st_size < entry2->s.st_size) return 1;
	if ( entry1->s.st_size > entry2->s.st_size) return -1;
	else return BLI_strcasecmp(entry1->relname,entry2->relname);
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
	if( strcmp(entry2->relname, "..")==0 ) return (-1);
	
	return (BLI_strcasecmp(sufix1, sufix2));
}

void BIF_filelist_filter(FileList* filelist)
{
	char dir[FILE_MAX], group[GROUP_MAX];
	int num_filtered = 0;
	int i, j;
	
	if (!filelist->filelist)
		return;
	
	if ( ( (filelist->type == FILE_LOADLIB) &&  BIF_filelist_islibrary(filelist, dir, group)) 
		|| (filelist->type == FILE_MAIN) ) {
		filelist->filter = 0;
	}

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

void BIF_filelist_init_icons()
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

void BIF_filelist_free_icons()
{
	int i;
	for (i=0; i < SPECIAL_IMG_MAX; ++i) {
		IMB_freeImBuf(gSpecialFileImages[i]);
		gSpecialFileImages[i] = NULL;
	}
}

struct FileList*	BIF_filelist_new()
{
	FileList* p = MEM_callocN( sizeof(FileList), "filelist" );
	p->filelist = 0;
	p->numfiles = 0;
	p->dir[0] = '\0';
	p->libfiledata = 0;
	p->type = 0;
	p->has_func = 0;
	p->filter = 0;
	return p;
}

struct FileList*	BIF_filelist_copy(struct FileList* filelist)
{
	FileList* p = BIF_filelist_new();
	BLI_strncpy(p->dir, filelist->dir, FILE_MAX);
	p->filelist = NULL;
	p->fidx = NULL;
	p->type = filelist->type;
	p->ipotype = filelist->ipotype;
	p->has_func = filelist->has_func;

	return p;
}

void BIF_filelist_free(struct FileList* filelist)
{
	int i;

	if (!filelist) {
		printf("Attemtping to delete empty filelist.\n");
		return;
	}

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
}

void BIF_filelist_freelib(struct FileList* filelist)
{
	if(filelist->libfiledata)	
		BLO_blendhandle_close(filelist->libfiledata);
	filelist->libfiledata= 0;
}

struct BlendHandle *BIF_filelist_lib(struct FileList* filelist)
{
	return filelist->libfiledata;
}

int	BIF_filelist_numfiles(struct FileList* filelist)
{
	return filelist->numfiltered;
}

const char * BIF_filelist_dir(struct FileList* filelist)
{
	return filelist->dir;
}

void BIF_filelist_setdir(struct FileList* filelist, const char *dir)
{
	BLI_strncpy(filelist->dir, dir, FILE_MAX);
}

void BIF_filelist_imgsize(struct FileList* filelist, short w, short h)
{
	filelist->prv_w = w;
	filelist->prv_h = h;
}

void BIF_filelist_loadimage(struct FileList* filelist, int index)
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
		if (filelist->type != FILE_MAIN)
		{
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
				
				IMB_scaleImBuf(imb, ex, ey);

			} 
			filelist->filelist[fidx].image = imb;
			
		}
	}
}

struct ImBuf * BIF_filelist_getimage(struct FileList* filelist, int index)
{
	ImBuf* ibuf = NULL;
	int fidx = 0;	
	if ( (index < 0) || (index >= filelist->numfiltered) ) {
		return NULL;
	}
	fidx = filelist->fidx[index];
	ibuf = filelist->filelist[fidx].image;

	if (ibuf == NULL) {
		struct direntry *file = &filelist->filelist[fidx];
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
		} 
	}
	return ibuf;
}

struct direntry * BIF_filelist_file(struct FileList* filelist, int index)
{
	int fidx = 0;
	
	if ( (index < 0) || (index >= filelist->numfiltered) ) {
		return NULL;
	}
	fidx = filelist->fidx[index];

	return &filelist->filelist[fidx];
}

int BIF_filelist_find(struct FileList* filelist, char *file)
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

void BIF_filelist_hidedot(struct FileList* filelist, short hide)
{
	filelist->hide_dot = hide;
}

void BIF_filelist_setfilter(struct FileList* filelist, unsigned int filter)
{
	filelist->filter = filter;
}

void BIF_filelist_readdir(struct FileList* filelist)
{
	char wdir[FILE_MAX];
	int finished = 0;

	if (!filelist) return;
	filelist->fidx = 0;
	filelist->filelist = 0;

	if(filelist->type==FILE_MAIN) {
		BIF_filelist_from_main(filelist);
		finished = 1;
	} else if(filelist->type==FILE_LOADLIB) {
		BLI_cleanup_dir(G.sce, filelist->dir);
		BIF_filelist_from_library(filelist);
		if(filelist->libfiledata) {
			finished = 1;
		}
	}

	if (!finished) {
		BLI_getwdN(wdir);	 
		
		BLI_cleanup_dir(G.sce, filelist->dir);
		BLI_hide_dot_files(filelist->hide_dot);
		filelist->numfiles = BLI_getdir(filelist->dir, &(filelist->filelist));

		chdir(wdir);
		BIF_filelist_setfiletypes(filelist, G.have_quicktime);
		BIF_filelist_filter(filelist);

	}
}

int BIF_filelist_empty(struct FileList* filelist)
{	
	return filelist->filelist == 0;
}

void BIF_filelist_parent(struct FileList* filelist)
{
	BLI_parent_dir(filelist->dir);
	BLI_make_exist(filelist->dir);
	BIF_filelist_readdir(filelist);
}

void BIF_filelist_setfiletypes(struct FileList* filelist, short has_quicktime)
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
			if(filelist->type==FILE_LOADLIB) {		
				char name[FILE_MAXDIR+FILE_MAXFILE];
				BLI_strncpy(name, filelist->dir, sizeof(name));
				strcat(name, file->relname);
				
				/* prevent current file being used as acceptable dir */
				if (BLI_streq(G.main->name, name)==0) {
					file->type &= ~S_IFMT;
					file->type |= S_IFDIR;
				}
			}
		} else if(BLI_testextensie(file->relname, ".py")) {
				file->flags |= PYSCRIPTFILE;
		} else if(BLI_testextensie(file->relname, ".txt")) {
				file->flags |= TEXTFILE;
		} else if( BLI_testextensie(file->relname, ".ttf")
					|| BLI_testextensie(file->relname, ".ttc")
					|| BLI_testextensie(file->relname, ".pfb")
					|| BLI_testextensie(file->relname, ".otf")
					|| BLI_testextensie(file->relname, ".otc")) {
				file->flags |= FTFONTFILE;			
		} else if (has_quicktime){
			if(		BLI_testextensie(file->relname, ".int")
				||  BLI_testextensie(file->relname, ".inta")
				||  BLI_testextensie(file->relname, ".jpg")
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
				||	BLI_testextensie(file->relname, ".mv")) {
				file->flags |= MOVIEFILE;			
			}
			else if(BLI_testextensie(file->relname, ".wav")) {
				file->flags |= SOUNDFILE;
			}
		} else { // no quicktime
			if(BLI_testextensie(file->relname, ".int")
				||	BLI_testextensie(file->relname, ".inta")
				||	BLI_testextensie(file->relname, ".jpg")
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
				||	BLI_testextensie(file->relname, ".mp4")
				||	BLI_testextensie(file->relname, ".mv")) {
				file->flags |= MOVIEFILE;			
			}
			else if(BLI_testextensie(file->relname, ".wav")) {
				file->flags |= SOUNDFILE;
			}
		}
	}
}

void BIF_filelist_swapselect(struct FileList* filelist)
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

int BIF_filelist_islibrary(struct FileList* filelist, char* dir, char* group)
{
	 /* return ok when a blenderfile, in dir is the filename,
	 * in group the type of libdata
	 */
	int len;
	char *fd;
	
	strcpy(dir, filelist->dir);
	len= strlen(dir);
	if(len<7) return 0;
	if( dir[len-1] != '/' && dir[len-1] != '\\') return 0;
		
	group[0]= 0;
	dir[len-1]= 0;

	/* Find the last slash */
	fd= (strrchr(dir, '/')>strrchr(dir, '\\'))?strrchr(dir, '/'):strrchr(dir, '\\');

	if(fd==0) return 0;
	*fd= 0;
	if(BLO_has_bfile_extension(fd+1)) {
		*fd= '/';
	}
	else {
		char *gp = fd+1; // in case we have a .blend file, gp points to the group

		/* Find the last slash */
		fd= (strrchr(dir, '/')>strrchr(dir, '\\'))?strrchr(dir, '/'):strrchr(dir, '\\');
		if (!fd || !BLO_has_bfile_extension(fd+1)) return 0;

		/* now we know that we are in a blend file and it is safe to 
		   assume that gp actually points to a group */
		BLI_strncpy(group, gp, GROUP_MAX);
	}
	return 1;
}

void BIF_filelist_from_library(struct FileList* filelist)
{
	LinkNode *l, *names, *previews;
	struct ImBuf* ima;
	int ok, i, nnames, idcode;
	char filename[FILE_MAXDIR+FILE_MAXFILE];
	char dir[FILE_MAX], group[GROUP_MAX];	
	
	filelist->type = FILE_LOADLIB;

	/* name test */
	ok= BIF_filelist_islibrary(filelist, dir, group);
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
	
	idcode= BIF_groupname_to_code(group);

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

	filelist->numfiles= nnames + 2;
	filelist->filelist= malloc(filelist->numfiles * sizeof(*filelist->filelist));
	memset(filelist->filelist, 0, filelist->numfiles * sizeof(*filelist->filelist));

	filelist->filelist[0].relname= BLI_strdup(".");
	filelist->filelist[0].type |= S_IFDIR;
	filelist->filelist[1].relname= BLI_strdup("..");
	filelist->filelist[1].type |= S_IFDIR;
		
	for (i=0, l= names; i<nnames; i++, l= l->next) {
		char *blockname= l->link;

		filelist->filelist[i + 2].relname= BLI_strdup(blockname);
		if (!idcode)
			filelist->filelist[i + 2].type |= S_IFDIR;
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
					filelist->filelist[i + 2].image = ima;
					filelist->filelist[i + 2].flags = IMAGEFILE;
				}
			}
		}
	}

	BLI_linklist_free(names, free);
	if (previews) BLI_linklist_free(previews, (void(*)(void*)) MEM_freeN);

	BIF_filelist_sort(filelist, FILE_SORTALPHA);

	BLI_strncpy(G.sce, filename, sizeof(filename));	// prevent G.sce to change

	filelist->filter = 0;
	BIF_filelist_filter(filelist);
}

void BIF_filelist_append_library(struct FileList *filelist, char *dir, char *file, short flag, int idcode)
{
	BLO_library_append_(&filelist->libfiledata, filelist->filelist, filelist->numfiles, dir, file, flag, idcode);
}

void BIF_filelist_from_main(struct FileList *filelist)
{
	ID *id;
	struct direntry *files, *firstlib = NULL;
	ListBase *lb;
	int a, fake, idcode, ok, totlib, totbl;
	
	filelist->type = FILE_MAIN;

	if(filelist->dir[0]=='/') filelist->dir[0]= 0;
	
	if(filelist->dir[0]) {
		idcode= BIF_groupname_to_code(filelist->dir);
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
		filelist->filelist[1].relname= BLI_strdup(".");
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
		BIF_filelist_sort(filelist, FILE_SORTALPHA);
	}
	else {

		/* make files */
		idcode= BIF_groupname_to_code(filelist->dir);
		
		lb= wich_libbase(G.main, idcode );
		if(lb==0) return;
		
		id= lb->first;
		filelist->numfiles= 0;
		while(id) {
			
			if(filelist->has_func && idcode==ID_IP) {
				if(filelist->ipotype== ((Ipo *)id)->blocktype) filelist->numfiles++;
			}
			else if (!filelist->hide_dot || id->name[2] != '.') {
				filelist->numfiles++;
			}
			
			id= id->next;
		}
		
		if(!filelist->has_func) filelist->numfiles+= 2;
		filelist->filelist= (struct direntry *)malloc(filelist->numfiles * sizeof(struct direntry));
		
		files = filelist->filelist;
		
		if(!filelist->has_func) {
			memset( &(filelist->filelist[0]), 0 , sizeof(struct direntry));
			filelist->filelist[0].relname= BLI_strdup(".");
			filelist->filelist[0].type |= S_IFDIR;
			memset( &(filelist->filelist[1]), 0 , sizeof(struct direntry));
			filelist->filelist[1].relname= BLI_strdup("..");
			filelist->filelist[1].type |= S_IFDIR;
		
			files+= 2;
		}
		
		id= lb->first;
		totlib= totbl= 0;
		
		while(id) {
			
			ok= 0;
			if(filelist->has_func && idcode==ID_IP) {
				if(filelist->ipotype== ((Ipo *)id)->blocktype) ok= 1;
			}
			else ok= 1;
			
			if(ok) {
				/* TODO: hide dot files - elubie */
				memset( files, 0 , sizeof(struct direntry));
				if(id->lib==NULL)
					files->relname= BLI_strdup(id->name+2);
				else {
					files->relname= MEM_mallocN(FILE_MAXDIR+FILE_MAXFILE+32, "filename for lib");
					sprintf(files->relname, "%s | %s", id->lib->name, id->name+2);
				}
				/* files->type |= S_IFDIR; */
				if(!filelist->has_func) { /* F4 DATA BROWSE */
					if(idcode==ID_OB) {
						if( ((Object *)id)->flag & SELECT) files->flags |= ACTIVE;
					}
					else if(idcode==ID_SCE) {
						if( ((Scene *)id)->r.scemode & R_BG_RENDER) files->flags |= ACTIVE;
					}					
				}
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
	BIF_filelist_filter(filelist);
}


void BIF_filelist_settype(struct FileList* filelist, int type)
{
	filelist->type = type;
}

short BIF_filelist_gettype(struct FileList* filelist)
{
	return filelist->type;
}

void BIF_filelist_sort(struct FileList* filelist, short sort)
{
	struct direntry *file;
	int num;/*  , act= 0; */

	switch(sort) {
	case FILE_SORTALPHA:
		qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_name);	
		break;
	case FILE_SORTDATE:
		qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_date);	
		break;
	case FILE_SORTSIZE:
		qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_size);	
		break;
	case FILE_SORTEXTENS:
		qsort(filelist->filelist, filelist->numfiles, sizeof(struct direntry), compare_extension);	
	}

	file= filelist->filelist;
	for(num=0; num<filelist->numfiles; num++, file++) {
		file->flags &= ~HILITE;
	}
	BIF_filelist_filter(filelist);
}


void BIF_filelist_setipotype(struct FileList* filelist, short ipotype)
{
	filelist->ipotype = ipotype;
}

void BIF_filelist_hasfunc(struct FileList* filelist, int has_func)
{
	filelist->has_func = has_func;
}

