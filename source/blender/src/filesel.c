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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <sys/stat.h>
#include <sys/types.h>
#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#ifdef WIN32
#include <io.h>
#include <direct.h>
#include "BLI_winstuff.h"
#else
#include <unistd.h>
#include <sys/times.h>
#endif   

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_linklist.h"
#include "BLI_storage_types.h"
#include "BLI_dynstr.h"

#include "IMB_imbuf.h"

#include "DNA_armature_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"

#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_utildefines.h"

#include "BIF_editview.h"
#include "BIF_filelist.h"
#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_language.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_usiblender.h"

#include "BLO_readfile.h"

#include "BDR_editcurve.h"
#include "BDR_editobject.h"

#include "BSE_filesel.h"
#include "BSE_view.h"

#include "mydevice.h"
#include "blendef.h"
#include "nla.h"

#include "BIF_fsmenu.h"  /* include ourselves */

#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#if defined __BeOS
static int fnmatch(const char *pattern, const char *string, int flags)
{
	return 0;
}
#elif defined WIN32 && !defined _LIBC
	/* use fnmatch included in blenlib */
	#include "BLI_fnmatch.h"
#else
	#include <fnmatch.h>
#endif

#ifndef WIN32
#include <sys/param.h>
#endif

#define FILESELHEAD		60
#define FILESEL_DY		16

/* for events */
#define NOTACTIVE			0
#define ACTIVATE			1
#define INACTIVATE			2
/* for state of file */
#define ACTIVE				2

#define STARTSWITH(x, y) (strncmp(x, y, sizeof(x) - 1) == 0)

/* button events */
#define B_FS_FILENAME	1
#define B_FS_DIRNAME	2
#define B_FS_DIR_MENU	3
#define B_FS_PARDIR	4
#define B_FS_LOAD	5
#define B_FS_CANCEL	6
#define B_FS_LIBNAME	7

/* max length of library group name within filesel */
#define GROUP_MAX 32

static int is_a_library(SpaceFile *sfile, char *dir, char *group);
static void do_library_append(SpaceFile *sfile);
static void library_to_filelist(SpaceFile *sfile);
static void filesel_select_objects(struct SpaceFile *sfile);
static void active_file_object(struct SpaceFile *sfile);
static int groupname_to_code(char *group);

extern void countall(void);

/* very bad local globals */

static rcti scrollrct, textrct, bar;
static int filebuty1, filebuty2, page_ofs, collumwidth, selecting=0;
static int filetoname= 0;
static float pixels_to_ofs;
static char otherdir[FILE_MAX];
static ScrArea *otherarea;

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

/* **************************************** */
static int filesel_has_func(SpaceFile *sfile)
{
	if(sfile->returnfunc || sfile->returnfunc_event || sfile->returnfunc_args)
		return 1;
	return 0;
}

void filesel_statistics(SpaceFile *sfile, int *totfile, int *selfile, float *totlen, float *sellen)
{
	double len;
	int a;
	
	*totfile= *selfile= 0;
	*totlen= *sellen= 0;
	
	if(sfile->filelist==0) return;
	
	for(a=0; a<sfile->totfile; a++) {
		if( (sfile->filelist[a].type & S_IFDIR)==0 ) {
			(*totfile) ++;

			len = sfile->filelist[a].s.st_size;
			(*totlen) += (float)(len/1048576.0); 		

			if(sfile->filelist[a].flags & ACTIVE) {
				(*selfile) ++;
				(*sellen) += (float)(len/1048576.0);
			}
		}
	}
}

/* *************** HELP FUNCTIONS ******************* */


/* not called when browsing .blend itself */
void test_flags_file(SpaceFile *sfile)
{
	struct direntry *file;
	int num;

	file= sfile->filelist;
	
	for(num=0; num<sfile->totfile; num++, file++) {
		file->flags= 0;
		file->type= file->s.st_mode;	/* restore the mess below */ 

			/* Don't check extensions for directories */ 
		if (file->type & S_IFDIR)
			continue;
			
		if(sfile->type==FILE_BLENDER || sfile->type==FILE_LOADLIB) {
			if(BLO_has_bfile_extension(file->relname)) {
				file->flags |= BLENDERFILE;
				
				if(sfile->type==FILE_LOADLIB) {
					char name[FILE_MAX];
					BLI_strncpy(name, sfile->dir, sizeof(name));
					strcat(name, file->relname);
					
					/* prevent current file being used as acceptable dir */
					if (BLI_streq(G.main->name, name)==0) {
						file->type &= ~S_IFMT;
						file->type |= S_IFDIR;
					}
				}
			}
		} else if (sfile->type==FILE_SPECIAL || sfile->type==FILE_LOADFONT){
			if(BLI_testextensie(file->relname, ".py")) {
				file->flags |= PYSCRIPTFILE;			
			} else if( BLI_testextensie(file->relname, ".ttf")
					|| BLI_testextensie(file->relname, ".ttc")
					|| BLI_testextensie(file->relname, ".pfb")
					|| BLI_testextensie(file->relname, ".otf")
					|| BLI_testextensie(file->relname, ".otc")) {
				file->flags |= FTFONTFILE;			
			} else if (G.have_libtiff &&
					(BLI_testextensie(file->relname, ".tif")
					||	BLI_testextensie(file->relname, ".tiff"))) {
					file->flags |= IMAGEFILE;			
			} else if (BLI_testextensie(file->relname, ".exr")) {
					file->flags |= IMAGEFILE;			
			} else if (G.have_quicktime){
				if(		BLI_testextensie(file->relname, ".jpg")
					||	BLI_testextensie(file->relname, ".jpeg")
					||	BLI_testextensie(file->relname, ".hdr")
					||	BLI_testextensie(file->relname, ".exr")
					||	BLI_testextensie(file->relname, ".tga")
					||	BLI_testextensie(file->relname, ".rgb")
					||	BLI_testextensie(file->relname, ".bmp")
					||	BLI_testextensie(file->relname, ".png")
#ifdef WITH_DDS
					||	BLI_testextensie(file->relname, ".dds")
#endif
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
					||  BLI_testextensie(file->relname, ".cin")
					||  BLI_testextensie(file->relname, ".dpx")
					||	BLI_testextensie(file->relname, ".sgi")) {
					file->flags |= IMAGEFILE;			
				}
				else if(BLI_testextensie(file->relname, ".avi")
					||	BLI_testextensie(file->relname, ".flc")
					||	BLI_testextensie(file->relname, ".dv")
					||	BLI_testextensie(file->relname, ".mov")
					||	BLI_testextensie(file->relname, ".movie")
					||	BLI_testextensie(file->relname, ".mv")) {
					file->flags |= MOVIEFILE;			
				}
			} else { // no quicktime
				if(BLI_testextensie(file->relname, ".jpg")
				   ||	BLI_testextensie(file->relname, ".hdr")
				   ||	BLI_testextensie(file->relname, ".exr")
					||	BLI_testextensie(file->relname, ".tga")
					||	BLI_testextensie(file->relname, ".rgb")
					||	BLI_testextensie(file->relname, ".bmp")
					||	BLI_testextensie(file->relname, ".png")
#ifdef WITH_DDS
					||	BLI_testextensie(file->relname, ".dds")
#endif
					||	BLI_testextensie(file->relname, ".iff")
					||	BLI_testextensie(file->relname, ".lbm")
					||  BLI_testextensie(file->relname, ".cin")
					||  BLI_testextensie(file->relname, ".dpx")
					||	BLI_testextensie(file->relname, ".sgi")) {
					file->flags |= IMAGEFILE;			
				}
				else if(BLI_testextensie(file->relname, ".avi")
					||	BLI_testextensie(file->relname, ".mv")) {
					file->flags |= MOVIEFILE;			
				}
				else if(BLI_testextensie(file->relname, ".wav")) {
					file->flags |= SOUNDFILE;
				}				
			}
		}
	}	
}


void sort_filelist(SpaceFile *sfile)
{
	struct direntry *file;
	int num;/*  , act= 0; */
	
	switch(sfile->sort) {
	case FILE_SORTALPHA:
		qsort(sfile->filelist, sfile->totfile, sizeof(struct direntry), compare_name);	
		break;
	case FILE_SORTDATE:
		qsort(sfile->filelist, sfile->totfile, sizeof(struct direntry), compare_date);	
		break;
	case FILE_SORTSIZE:
		qsort(sfile->filelist, sfile->totfile, sizeof(struct direntry), compare_size);	
		break;
	case FILE_SORTEXTENS:
		qsort(sfile->filelist, sfile->totfile, sizeof(struct direntry), compare_extension);	
	}
	
	sfile->act= -1;

	file= sfile->filelist;
	for(num=0; num<sfile->totfile; num++, file++) {
		file->flags &= ~HILITE;
	}

}

void read_dir(SpaceFile *sfile)
{
	int num, len;
	char wdir[FILE_MAX];

	/* sfile->act is used for example in databrowse: double names of library objects */
	sfile->act= -1;

	if(sfile->type==FILE_MAIN) {
		main_to_filelist(sfile);
		return;
	}
	else if(sfile->type==FILE_LOADLIB) {
		library_to_filelist(sfile);
		if(sfile->libfiledata) return;
	}

	BLI_hide_dot_files(sfile->flag & FILE_HIDE_DOT);
	
	BLI_getwdN(wdir);
	sfile->totfile= BLI_getdir(sfile->dir, &(sfile->filelist));
	chdir(wdir);
	
	if(sfile->sort!=FILE_SORTALPHA) sort_filelist(sfile);
	
	sfile->maxnamelen= 0;

	for (num=0; num<sfile->totfile; num++) {
		
		len = BMF_GetStringWidth(G.font, sfile->filelist[num].relname);
		if (len > sfile->maxnamelen) sfile->maxnamelen = len;
		
		if(filetoname) {
			if(strcmp(sfile->file, sfile->filelist[num].relname)==0) {
				
				sfile->ofs= num-( sfile->collums*(curarea->winy-FILESELHEAD-20)/(2*FILESEL_DY));
				filetoname= 0;
			}
		}
	}
	test_flags_file(sfile);
	
	filetoname= 0;
}

void freefilelist(SpaceFile *sfile)
{
	int num;

	num= sfile->totfile-1;

	if (sfile->filelist==0) return;
	
	for(; num>=0; num--){
		MEM_freeN(sfile->filelist[num].relname);
		
		if (sfile->filelist[num].string) MEM_freeN(sfile->filelist[num].string);
	}
	free(sfile->filelist);
	sfile->filelist= 0;
}

static void split_sfile(SpaceFile *sfile, char *s1)
{
	char string[FILE_MAX], dir[FILE_MAX], file[FILE_MAX];

	BLI_strncpy(string, s1, sizeof(string));

	BLI_split_dirfile(string, dir, file);
	
	if(sfile->filelist) {
		if(strcmp(dir, sfile->dir)!=0) {
			freefilelist(sfile);
		}
		else test_flags_file(sfile);
	}
	BLI_strncpy(sfile->file, file, sizeof(sfile->file));
		
	BLI_make_file_string(G.sce, sfile->dir, dir, "");
}


void parent(SpaceFile *sfile)
{
	short a;
	char *dir;
	
	/* if databrowse: no parent */
	if(sfile->type==FILE_MAIN && filesel_has_func(sfile)) return;

	dir= sfile->dir;
	
#ifdef WIN32
	if( (a = strlen(dir)) ) {				/* remove all '/' at the end */
		while(dir[a-1] == '\\') {
			a--;
			dir[a] = 0;
			if (a<=0) break;
		}
	}
	if( (a = strlen(dir)) ) {				/* then remove all until '/' */
		while(dir[a-1] != '\\') {
			a--;
			dir[a] = 0;
			if (a<=0) break;
		}
	}
	if( (a = strlen(dir)) ) {
		if (dir[a-1] != '\\') strcat(dir,"\\");
	}
	else if(sfile->type!=FILE_MAIN) { 
		get_default_root(dir);
	}
#else
	if( (a = strlen(dir)) ) {				/* remove all '/' at the end */
		while(dir[a-1] == '/') {
			a--;
			dir[a] = 0;
			if (a<=0) break;
		}
	}
	if( (a = strlen(dir)) ) {				/* then remove until '/' */
		while(dir[a-1] != '/') {
			a--;
			dir[a] = 0;
			if (a<=0) break;
		}
	}
	if ( (a = strlen(dir)) ) {
		if (dir[a-1] != '/') strcat(dir,"/");
	}
	else if(sfile->type!=FILE_MAIN) strcpy(dir,"/");
#endif
	
	/* to be sure */
	BLI_make_exist(sfile->dir);

	freefilelist(sfile);
	sfile->ofs= 0;
	scrarea_queue_winredraw(curarea);
}

void swapselect_file(SpaceFile *sfile)
{
	struct direntry *file;
	int num, act= 0;
	
	file= sfile->filelist;
	for(num=0; num<sfile->totfile; num++, file++) {
		if(file->flags & ACTIVE) {
			act= 1;
			break;
		}
	}
	file= sfile->filelist+2;
	for(num=2; num<sfile->totfile; num++, file++) {
		if(act) file->flags &= ~ACTIVE;
		else file->flags |= ACTIVE;
	}
}

static int find_active_file(SpaceFile *sfile, short x, short y)
{
	int ofs, act;
	
	if(y > textrct.ymax) y= textrct.ymax;
	if(y <= textrct.ymin) y= textrct.ymin+1;
	
	ofs= (x-textrct.xmin)/collumwidth;
	if(ofs<0) ofs= 0;
	ofs*= (textrct.ymax-textrct.ymin);

	act= sfile->ofs+ (ofs+textrct.ymax-y)/FILESEL_DY;
	
	if(act<0 || act>=sfile->totfile)
		act= -1;
	
	return act;
}


/* ********************** DRAW ******************************* */

static void calc_file_rcts(SpaceFile *sfile)
{
	int tot, h, len;
	float fac, start, totfile;
	
	scrollrct.xmin= 15;
	scrollrct.xmax= 35;
	scrollrct.ymin= 10;
	scrollrct.ymax= curarea->winy-10-FILESELHEAD;
	
	textrct.xmin= scrollrct.xmax+10;
	textrct.xmax= curarea->winx-10;
	textrct.ymin= scrollrct.ymin;
	textrct.ymax= scrollrct.ymax;
	
	if(textrct.xmax-textrct.xmin <60) textrct.xmax= textrct.xmin+60;
	
	len= (textrct.ymax-textrct.ymin) % FILESEL_DY;
	textrct.ymin+= len;
	scrollrct.ymin+= len;
	
	filebuty1= curarea->winy-FILESELHEAD;
	filebuty2= filebuty1+FILESELHEAD/2 -6;
	
	
	/* amount of collums */
	len= sfile->maxnamelen+25;
	
	if(sfile->type==FILE_MAIN) len+= 100;
	else if(sfile->flag & FILE_SHOWSHORT) len+= 100;
	else len+= 380;
	
	sfile->collums= (textrct.xmax-textrct.xmin)/len;
	
	if(sfile->collums<1) sfile->collums= 1;
	else if(sfile->collums>8) sfile->collums= 8;

	/* this flag aint yet defined in user menu, needed? */
//	if((U.flag & USER_FSCOLLUM)==0) sfile->collums= 1;
	
	collumwidth= (textrct.xmax-textrct.xmin)/sfile->collums;
	

	totfile= sfile->totfile + 0.5f;

	tot= (int)(FILESEL_DY*totfile);
	if(tot) fac= ((float)sfile->collums*(scrollrct.ymax-scrollrct.ymin))/( (float)tot);
	else fac= 1.0;
	
	if(sfile->ofs<0) sfile->ofs= 0;
	
	if(tot) start= ( (float)sfile->ofs)/(totfile);
	else start= 0.0;
	if(fac>1.0) fac= 1.0f;

	if(start+fac>1.0) {
		sfile->ofs= (short)ceil((1.0-fac)*totfile);
		start= ( (float)sfile->ofs)/(totfile);
		fac= 1.0f-start;
	}

	bar.xmin= scrollrct.xmin+2;
	bar.xmax= scrollrct.xmax-2;
	h= (scrollrct.ymax-scrollrct.ymin)-4;
	bar.ymax= (int)(scrollrct.ymax-2- start*h);
	bar.ymin= (int)(bar.ymax- fac*h);

	pixels_to_ofs= (totfile)/(float)(h+3);
	page_ofs= (int)(fac*totfile);
}

int filescrollselect= 0;

static void draw_filescroll(SpaceFile *sfile)
{

	if(scrollrct.ymin+10 >= scrollrct.ymax) return;
	
	BIF_ThemeColor(TH_BACK);
	glRecti(scrollrct.xmin,  scrollrct.ymin,  scrollrct.xmax,  scrollrct.ymax);

	uiEmboss(scrollrct.xmin, scrollrct.ymin, scrollrct.xmax, scrollrct.ymax, 1);

	BIF_ThemeColor(TH_HEADER);
	glRecti(bar.xmin+2,  bar.ymin+2,  bar.xmax-2,  bar.ymax-2);

	uiEmboss(bar.xmin+2, bar.ymin+2, bar.xmax-2, bar.ymax-2, filescrollselect);
	
}

static void linerect(int id, int x, int y)
{
	if(id & ACTIVE) {
		if(id & HILITE) BIF_ThemeColorShade(TH_HILITE, 20);
		else BIF_ThemeColor(TH_HILITE);
	}
	else if(id & HILITE) BIF_ThemeColorShade(TH_BACK, 20);
	else BIF_ThemeColor(TH_BACK);
	
	glRects(x-17,  y-3,  x+collumwidth-21,  y+11);

}

static void print_line(SpaceFile *sfile, struct direntry *files, int x, int y)
{
	int boxcol=0;
	char *s;

	boxcol= files->flags & (HILITE + ACTIVE);

	if(boxcol) {
		linerect(boxcol, x, y);
	}

	// this is where the little boxes in the file view are being drawn according to the file type
	if(files->flags & BLENDERFILE) {
		cpack(0xA0A0);
		glRects(x-14,  y,  x-8,  y+7);
	}
	else if(files->flags & PSXFILE) {
		cpack(0xA060B0);
		glRects(x-14,  y,  x-8,  y+7);
	}
	else if(files->flags & IMAGEFILE) {
		cpack(0xF08040);
		glRects(x-14,  y,  x-8,  y+7);
	}
	else if(files->flags & MOVIEFILE) {
		cpack(0x70A070);
		glRects(x-14,  y,  x-8,  y+7);
	}
	else if(files->flags & PYSCRIPTFILE) {
		cpack(0x4477dd);
		glRects(x-14,  y,  x-8,  y+7);
	}
	else if(files->flags & SOUNDFILE) {
		cpack(0xa0a000);
		glRects(x-14,  y,  x-8,  y+7);
	}	
	else if(files->flags & FTFONTFILE) {
		cpack(0xff2371);
		glRects(x-14,  y,  x-8,  y+7);
	}
	
	if(S_ISDIR(files->type)) BIF_ThemeColor(TH_TEXT_HI);
	else BIF_ThemeColor(TH_TEXT);

	s = files->string;
	if(s) {
		glRasterPos2i(x,  y);
#ifdef WITH_ICONV
		{
			struct LANGMenuEntry *lme;
       		lme = find_language(U.language);

			if ((lme !=NULL) && (!strcmp(lme->code, "ja_JP") || 
				!strcmp(lme->code, "zh_CN")))
			{
				BIF_RasterPos((float)x, (float)y);
#ifdef WIN32
				BIF_DrawString(G.font, files->relname, ((U.transopts & USER_TR_MENUS) | CONVERT_TO_UTF8));
#else
				BIF_DrawString(G.font, files->relname, (U.transopts & USER_TR_MENUS));
#endif
			} else {
				BMF_DrawString(G.font, files->relname);
			}
		}
#else
			BMF_DrawString(G.font, files->relname);
#endif /* WITH_ICONV */

		x += sfile->maxnamelen + 100;

		glRasterPos2i(x - BMF_GetStringWidth(G.font, files->size),  y);
		BMF_DrawString(G.font, files->size);

		if(sfile->flag & FILE_SHOWSHORT) return;

#ifndef WIN32
		/* rwx rwx rwx */
			x += 20; glRasterPos2i(x, y); 
			BMF_DrawString(G.font, files->mode1); 
		
			x += 30; glRasterPos2i(x, y); 
			BMF_DrawString(G.font, files->mode2); 
		
			x += 30; glRasterPos2i(x, y); 
			BMF_DrawString(G.font, files->mode3); 
		
		/* owner time date */
			x += 30; glRasterPos2i(x, y); 
			BMF_DrawString(G.font, files->owner); 
#endif
		
			x += 60; glRasterPos2i(x, y); 
			BMF_DrawString(G.font, files->time); 
		
			x += 50; glRasterPos2i(x, y); 
			BMF_DrawString(G.font, files->date); 
	}
	else {
		glRasterPos2i(x,  y);
		BMF_DrawString(G.font, files->relname);
		
		if(files->nr) {	/* extra info */
			x+= sfile->maxnamelen+20;
			glRasterPos2i(x,  y);
			BMF_DrawString(G.font, files->extra);
		}
	}
}


static int calc_filesel_line(SpaceFile *sfile, int nr, int *valx, int *valy)
{
	/* get screen coordinate of a line */
	int val, coll;

	nr-= sfile->ofs;

	/* amount of lines */
	val= (textrct.ymax-textrct.ymin)/FILESEL_DY;
	if (val == 0) coll = 0;
        else coll= nr/val;
	nr -= coll*val;
	
	*valy= textrct.ymax-FILESEL_DY+3 - nr*FILESEL_DY;
	*valx= coll*collumwidth + textrct.xmin+20;
	
	if(nr<0 || coll > sfile->collums) return 0;
	return 1;
}

static void set_active_file(SpaceFile *sfile, int act)
{
	struct direntry *file;
	int num, redraw= 0;
	unsigned int newflag;
	int old=0, newi=0;
	
	file= sfile->filelist;
	if(file==0) return;
	
	for(num=0; num<sfile->totfile; num++, file++) {
		if(num==act) {
			
			if(selecting && num>1) {
				newflag= HILITE | (file->flags & ~ACTIVE);
				if(selecting==ACTIVATE) newflag |= ACTIVE;
			
				if(file->flags != newflag) redraw|= 1;
				file->flags= newflag;
			}
			else {
				if(file->flags & HILITE);
				else {
					file->flags |= HILITE;
					redraw|= 2;
					newi= num;
				}
			}
		}
		else {
			if(file->flags & HILITE) {
				file->flags &= ~HILITE;
				redraw|= 2;
				old= num;
			}
		}
			
	}
	// removed frontbuffer draw here
	if(redraw) {
		scrarea_queue_winredraw(curarea);
	}
}


static void draw_filetext(SpaceFile *sfile)
{
	struct direntry *files;
	int a, x, y;
	short mval[2];
	
	if(textrct.ymin+10 >= textrct.ymax) return;


	/* box */
	BIF_ThemeColor(TH_BACK);
	glRecti(textrct.xmin,  textrct.ymin,  textrct.xmax,  textrct.ymax);

	/* collums */
	x= textrct.xmin+collumwidth;
	for(a=1; a<sfile->collums; a++, x+= collumwidth) {
		cpack(0x303030);
		sdrawline(x,  textrct.ymin,  x,  textrct.ymax); 
		cpack(0xB0B0B0);
		sdrawline(x+1,  textrct.ymin,  x+1,  textrct.ymax); 
	}

	if(sfile->filelist==0) return;
	
	/* test: if mouse is not in area: clear HILITE */
	getmouseco_areawin(mval);

	if(mval[0]<0 || mval[0]>curarea->winx) {
		files= sfile->filelist+sfile->ofs;
		for(a= sfile->ofs; a<sfile->totfile; a++, files++) files->flags &= ~HILITE;
	}
	
	files= sfile->filelist+sfile->ofs;
	for(a= sfile->ofs; a<sfile->totfile; a++, files++) {
	
		if( calc_filesel_line(sfile, a, &x, &y)==0 ) break;
		print_line(sfile, files, x, y);
	}

	/* clear drawing errors, with text at the right hand side: */
	BIF_ThemeColor(TH_HEADER);
	glRecti(textrct.xmax,  textrct.ymin,  textrct.xmax+10,  textrct.ymax);
	uiEmboss(textrct.xmin, textrct.ymin, textrct.xmax, textrct.ymax, 1);
}

static char *library_string(void)
{
	Library *lib;
	char *str;
	int nr=0, tot= BLI_countlist(&G.main->library);
	
	if(tot==0) return NULL;
	str= MEM_callocN(tot*(FILE_MAXDIR+FILE_MAX), "filesel lib menu");
	
	for(tot=0, lib= G.main->library.first; lib; lib= lib->id.next, nr++) {
		tot+= sprintf(str+tot, "%s %%x%d|", lib->name, nr);
	}
	return str;
}

void drawfilespace(ScrArea *sa, void *spacedata)
{
	SpaceFile *sfile;
	uiBlock *block;
	float col[3];
	int act, loadbutton;
	short mval[2];
	char name[20];
	char *menu, *strp= NULL;

	myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);

	BIF_GetThemeColor3fv(TH_HEADER, col);	// basic undrawn color is border
	glClearColor(col[0], col[1], col[2], 0.0); 
	glClear(GL_COLOR_BUFFER_BIT);

	sfile= sa->spacedata.first;	
	if(sfile->filelist==NULL) {
		read_dir(sfile);
		
		calc_file_rcts(sfile);
		
		/* calculate act */ 
		getmouseco_areawin(mval);
		act= find_active_file(sfile, mval[0], mval[1]);
		if(act>=0 && act<sfile->totfile)
			sfile->filelist[act].flags |= HILITE;
	}
	else calc_file_rcts(sfile);

	/* check if we load library, extra button */
	if(sfile->type==FILE_LOADLIB)
		strp= library_string();
	
	/* HEADER */
	sprintf(name, "win %d", sa->win);
	block= uiNewBlock(&sa->uiblocks, name, UI_EMBOSS, UI_HELV, sa->win);
	
	/* browse 1 datablock */
	uiSetButLock( sfile->type==FILE_MAIN && filesel_has_func(sfile), NULL);

	/* space available for load/save buttons? */
	loadbutton= MAX2(80, 20+BMF_GetStringWidth(G.font, sfile->title));
	if(textrct.xmax-textrct.xmin > loadbutton+20) {
		if(sfile->title[0]==0) loadbutton= 0;
	}
	else loadbutton= 0;

	uiBlockBeginAlign(block);
	uiDefBut(block, TEX, B_FS_DIRNAME,"",	textrct.xmin + (strp?20:0), filebuty2, textrct.xmax-textrct.xmin-loadbutton - (strp?20:0), 21, sfile->dir, 0.0, (float)FILE_MAXDIR-1, 0, 0, "Directory, enter a directory and press enter to create it, Substitute ~ for home"); /* Directory input */
	if(loadbutton) {
		uiSetCurFont(block, UI_HELV);
		uiDefBut(block, BUT, B_FS_LOAD, sfile->title,	textrct.xmax-loadbutton, filebuty2, loadbutton, 21, sfile->dir, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	}
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefBut(block, TEX, B_FS_FILENAME,"",	textrct.xmin, filebuty1, textrct.xmax-textrct.xmin-loadbutton, 21, sfile->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, "File, increment version number with (+/-)"); /* File input */
	if(loadbutton) {
		uiSetCurFont(block, UI_HELV);
		uiDefBut(block, BUT, B_FS_CANCEL, "Cancel",	textrct.xmax-loadbutton, filebuty1, loadbutton, 21, sfile->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	}
	uiBlockEndAlign(block);
	
	menu= fsmenu_build_menu();
	if(menu[0])	/* happens when no .Bfs is there, and first time browse */
		uiDefButS(block, MENU, B_FS_DIR_MENU, menu, scrollrct.xmin, filebuty1, scrollrct.xmax-scrollrct.xmin, 21, &sfile->menu, 0, 0, 0, 0, "");
	MEM_freeN(menu);

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_FS_PARDIR, "P", scrollrct.xmin, filebuty2, scrollrct.xmax-scrollrct.xmin, 21, 0, 0, 0, 0, 0, "Move to the parent directory (PKEY)");
	if(strp) {
		uiDefIconTextButS(block, MENU, B_FS_LIBNAME, ICON_LIBRARY_DEHLT, strp, scrollrct.xmin+20, filebuty2, scrollrct.xmax-scrollrct.xmin, 21, &sfile->menu, 0, 0, 0, 0, "");
		MEM_freeN(strp);
	}
			 
	uiDrawBlock(block);

	draw_filescroll(sfile);
	draw_filetext(sfile);
	
	/* others diskfree etc ? */
	scrarea_queue_headredraw(sa);	
	
	myortho2(-0.375, (float)(sa->winx)-0.375, -0.375, (float)(sa->winy)-0.375);
	draw_area_emboss(sa);
	
	sa->win_swap= WIN_BACK_OK;
}


static void do_filescroll(SpaceFile *sfile)
{
	short mval[2], oldy, yo;
	
	calc_file_rcts(sfile);
	
	filescrollselect= 1;
	
	/* for beauty */
	scrarea_do_windraw(curarea);
	screen_swapbuffers();
	
	getmouseco_areawin(mval);
	oldy= yo= mval[1];
	
	while(get_mbut()&L_MOUSE) {
		getmouseco_areawin(mval);
		
		if(yo!=mval[1]) {
			int dy= floor(0.5+((float)(oldy-mval[1]))*pixels_to_ofs);
			
			if(dy) {
				sfile->ofs+= dy;
				if(sfile->ofs<0) {
					sfile->ofs= 0;
					oldy= mval[1];
				}
				else oldy= floor(0.5+ (float)oldy - (float)dy/pixels_to_ofs);
	
				scrarea_do_windraw(curarea);
				screen_swapbuffers();
	
			}
			
			yo= mval[1];
		}
		else BIF_wait_for_statechange();
	}
	filescrollselect= 0;

	/* for beauty */
	scrarea_do_windraw(curarea);
	screen_swapbuffers();
	
}

static void do_filescrollwheel(SpaceFile *sfile, int move)
{
	// by phase
	int lines, rt;

	calc_file_rcts(sfile);

	lines = (int)(textrct.ymax-textrct.ymin)/FILESEL_DY;
	rt = lines * sfile->collums;

	if(sfile->totfile > rt) {
		sfile->ofs+= move;
		if( sfile->ofs + rt > sfile->totfile + 1)
			sfile->ofs = sfile->totfile - rt + 1;
	}

	if(sfile->ofs<0) {
		sfile->ofs= 0;
	}
}

/* the complete call; pulldown menu, and three callback types */
static void activate_fileselect_(int type, char *title, char *file, short *menup, char *pupmenu,
										 void (*func)(char *),
										 void (*func_event)(unsigned short),
										 void (*func_args)(char *, void *arg1, void *arg2),
										 void *arg1, void *arg2)
{
	SpaceFile *sfile;
	char group[GROUP_MAX], name[FILE_MAX], temp[FILE_MAX];
	
	if(curarea==0) return;
	if(curarea->win==0) return;
	
	newspace(curarea, SPACE_FILE);
	scrarea_queue_winredraw(curarea);
	
	/* sometime double, when area already is SPACE_FILE with a different file name */
	if(curarea->headwin) addqueue(curarea->headwin, CHANGED, 1);

	name[2]= 0;
	BLI_strncpy(name, file, sizeof(name));
	BLI_convertstringcode(name, G.sce, G.scene->r.cfra);
	
	sfile= curarea->spacedata.first;

	sfile->returnfunc= func;
	sfile->returnfunc_event= func_event;
	sfile->returnfunc_args= func_args;
	sfile->arg1= arg1;
	sfile->arg2= arg2;
	
	sfile->type= type;
	sfile->ofs= 0;
	
	if(sfile->pupmenu)
		MEM_freeN(sfile->pupmenu);
	sfile->pupmenu= pupmenu;
	sfile->menup= menup;
	
	/* sfile->act is used for databrowse: double names of library objects */
	sfile->act= -1;

	if(G.relbase_valid && U.flag & USER_RELPATHS && type != FILE_BLENDER)
		sfile->flag |= FILE_STRINGCODE;
	else
		sfile->flag &= ~FILE_STRINGCODE;

	if (U.uiflag & USER_HIDE_DOT)
		sfile->flag |= FILE_HIDE_DOT;

	if(type==FILE_MAIN) {
		char *groupname;
		
		BLI_strncpy(sfile->file, name+2, sizeof(sfile->file));

		groupname = BLO_idcode_to_name( GS(name) );
		if (groupname) {
			BLI_strncpy(sfile->dir, groupname, sizeof(sfile->dir) - 1);
			strcat(sfile->dir, "/");
		}

		/* free all */
		if(sfile->libfiledata) BLO_blendhandle_close(sfile->libfiledata);
		sfile->libfiledata= 0;
		
		freefilelist(sfile);
	}
	else if(type==FILE_LOADLIB) {
		BLI_strncpy(sfile->dir, name, sizeof(sfile->dir));
		BLI_cleanup_dir(G.sce, sfile->dir);
		if( is_a_library(sfile, temp, group) ) {
			/* force a reload of the library-filelist */
			freefilelist(sfile);
		}
		else {
			split_sfile(sfile, name);
			if(sfile->libfiledata) BLO_blendhandle_close(sfile->libfiledata);
			sfile->libfiledata= NULL;
		}
	}
	else {	/* FILE_BLENDER or FILE_LOADFONT */
		split_sfile(sfile, name);	/* test filelist too */
		BLI_cleanup_dir(G.sce, sfile->dir);

		/* free: filelist and libfiledata became incorrect */
		if(sfile->libfiledata) BLO_blendhandle_close(sfile->libfiledata);
		sfile->libfiledata= 0;
	}
	BLI_strncpy(sfile->title, title, sizeof(sfile->title));
	filetoname= 1;
}

void activate_fileselect(int type, char *title, char *file, void (*func)(char *))
{
	activate_fileselect_(type, title, file, NULL, NULL, func, NULL, NULL, NULL, NULL);
}

void activate_fileselect_menu(int type, char *title, char *file, char *pupmenu, short *menup, void (*func)(char *))
{
	activate_fileselect_(type, title, file, menup, pupmenu, func, NULL, NULL, NULL, NULL);
}

void activate_fileselect_args(int type, char *title, char *file, void (*func)(char *, void *, void *), void *arg1, void *arg2)
{
	activate_fileselect_(type, title, file, NULL, NULL, NULL, NULL, func, arg1, arg2);
}

void activate_databrowse(ID *id, int idcode, int fromcode, int retval, short *menup, void (*func)(unsigned short))
{
	ListBase *lb;
	SpaceFile *sfile;
	char str[32];
	
	if(id==NULL) {
		lb= wich_libbase(G.main, idcode);
		id= lb->first;
	}
	
	if(id) BLI_strncpy(str, id->name, sizeof(str));
	else return;
	
	activate_fileselect_(FILE_MAIN, "SELECT DATABLOCK", str, menup, NULL, NULL, func, NULL, NULL, NULL);
	
	sfile= curarea->spacedata.first;
	sfile->retval= retval;
	sfile->ipotype= fromcode;
}

void activate_databrowse_args(struct ID *id, int idcode, int fromcode, short *menup, void (*func)(char *, void *, void *), void *arg1, void *arg2)
{
	ListBase *lb;
	SpaceFile *sfile;
	char str[32];
	
	if(id==NULL) {
		lb= wich_libbase(G.main, idcode);
		id= lb->first;
	}
	
	if(id) BLI_strncpy(str, id->name, sizeof(str));
	else return;
	
	activate_fileselect_(FILE_MAIN, "SELECT DATABLOCK", str, menup, NULL, NULL, NULL, func, arg1, arg2);
	
	sfile= curarea->spacedata.first;
	sfile->ipotype= fromcode;
}

void filesel_prevspace()
{
	SpaceFile *sfile= curarea->spacedata.first;
	
	/* cleanup */
	if(sfile->spacetype==SPACE_FILE) {
		if(sfile->pupmenu) {
			MEM_freeN(sfile->pupmenu);
			sfile->pupmenu= NULL;
		}
	}

	if(sfile->next) {
	
		BLI_remlink(&curarea->spacedata, sfile);
		BLI_addtail(&curarea->spacedata, sfile);

		sfile= curarea->spacedata.first;

		if (sfile->spacetype == SPACE_SCRIPT) {
			SpaceScript *sc = (SpaceScript *)sfile;
			if (sc->script) sc->script->flags &=~SCRIPT_FILESEL;
		}

		newspace(curarea, sfile->spacetype);
	}
	else newspace(curarea, SPACE_INFO);
}

static int countselect(SpaceFile *sfile)
{
	int a, count=0;

	for(a=0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & ACTIVE) {
			count++;
		}
	}
	return count;
}

static int getotherdir(void)
{
	ScrArea *sa;
	SpaceFile *sfile=0;
	
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa!=curarea) {
			if(sa->spacetype==SPACE_FILE) {
				
				/* already found one */
				if(sfile) return 0;
		
				sfile= sa->spacedata.first;

				if(sfile->type & FILE_UNIX) {
					otherarea= sa;
					BLI_make_file_string(G.sce, otherdir, sfile->dir, "");
				}
				else sfile= 0;
			}
		}
		sa= sa->next;
	}
	if(sfile) return 1;
	return 0;
}

static void reread_other_fs(void)
{
	SpaceFile *sfile;
	
	/* watch it: only call when getotherdir returned OK */
	
	sfile= otherarea->spacedata.first;
	freefilelist(sfile);
	scrarea_queue_winredraw(otherarea);
}


void free_filesel_spec(char *dir)
{
	/* all filesels with 'dir' are freed */
	bScreen *sc;
		
	sc= G.main->screen.first;
	while(sc) {
		ScrArea *sa= sc->areabase.first;
		while(sa) {
			SpaceLink  *sl= sa->spacedata.first;
			while(sl) {
				if(sl->spacetype==SPACE_FILE) {
					SpaceFile *sfile= (SpaceFile*) sl;
					if (BLI_streq(sfile->dir, dir)) {
						freefilelist(sfile);
					}
				}
				sl= sl->next;
			}
			sa= sa->next;
		}
		sc= sc->id.next;
	}
}

/* NOTE: this is called for file read, after the execfunc no UI memory is valid! */
static void filesel_execute(SpaceFile *sfile)
{
	struct direntry *files;
	char name[FILE_MAX];
	int a;

	/* check for added length of dir and filename - annoying, but now that dir names can already be FILE_MAX
	   we need to prevent overwriting. Alternative of shortening the name behind the user's back is greater evil 
	   - elubie */ 
	if (strlen(sfile->dir) + strlen(sfile->file) >= FILE_MAX) {
		okee("File and Directory name together are too long. Please use shorter names.");
		return;
	}
	
#ifdef WIN32
	if ( (sfile->type!=FILE_LOADLIB) && (sfile->type!=FILE_MAIN) ) {
		if (!check_file_chars(sfile->file)) {
			error("You have illegal characters in the filename. Check console for more info.");
			printf("Characters '*?:|\"<>\\/' are illegal in a filename.\n");
			return;
		}
	}
#endif

	filesel_prevspace();

	if(sfile->type==FILE_LOADLIB) {
		if(sfile->flag & FILE_STRINGCODE) {
			if (!G.relbase_valid) {
				okee("You have to save the .blend file before using relative paths! Using absolute path instead.");
				sfile->flag &= ~FILE_STRINGCODE;
			}
		}

		do_library_append(sfile);
		
		BIF_undo_push( ((sfile->flag & FILE_LINK)==0) ? "Append from file" : "Link from file");
		
		allqueue(REDRAWALL, 1);
	}
	else if(filesel_has_func(sfile)) {
		fsmenu_insert_entry(sfile->dir, 1, 0);
	
		if(sfile->type==FILE_MAIN) { /* DATABROWSE */
			if (sfile->menup) {	/* with value pointing to ID block index */
				int notfound = 1;

				/*	Need special handling since hiding .* datablocks means that
					sfile->act is no longer the same as files->nr.

					Also, toggle HIDE_DOT on and off can make sfile->act not longer
					correct (meaning it doesn't point to the correct item in the filelist.
					
					sfile->file is always correct, so first with check if, for the item
					corresponding to sfile->act, the name is the same.

					If it isn't (or if sfile->act is not good), go over filelist and take
					the correct one.

					This means that selecting a datablock than hiding it makes it
					unselectable. Not really a problem.

					- theeth
				 */

				*sfile->menup= -1;

				if(sfile->act>=0 && sfile->act<sfile->totfile) {
					if(sfile->filelist) {
						files= sfile->filelist+sfile->act;
						if ( strcmp(files->relname, sfile->file)==0) {
							notfound = 0;
							*sfile->menup= files->nr;
						}
					}	
				}
				if (notfound) {
					for(a=0; a<sfile->totfile; a++) {
						if( strcmp(sfile->filelist[a].relname, sfile->file)==0) {
							*sfile->menup= sfile->filelist[a].nr;
							break;
						}
					}
				}
			}
			if(sfile->returnfunc_event)
				sfile->returnfunc_event(sfile->retval);
			else if(sfile->returnfunc_args)
				sfile->returnfunc_args(NULL, sfile->arg1, sfile->arg2);
		}
		else {
			if(strncmp(sfile->title, "Save", 4)==0) free_filesel_spec(sfile->dir);
			if(strncmp(sfile->title, "Export", 6)==0) free_filesel_spec(sfile->dir);
			
			BLI_strncpy(name, sfile->dir, sizeof(name));
			strcat(name, sfile->file);
			
			if(sfile->flag & FILE_STRINGCODE) {
				/* still weak, but we don't want saving files to make relative paths */
				if(G.relbase_valid && strncmp(sfile->title, "Save", 4)) {
					BLI_makestringcode(G.sce, name);
				} else {
					/* if we don't have a valid relative base (.blend file hasn't been saved yet)
					   then we don't save the path as relative (for texture images, background image).
					   Warning message not shown when saving files (doesn't make sense there)
					*/
					if (strncmp(sfile->title, "Save", 4)) {
						printf("Relative path setting has been ignored because .blend file hasn't been saved yet.\n");
					}
					sfile->flag &= ~FILE_STRINGCODE;
				}
			}
			if(sfile->returnfunc)
				sfile->returnfunc(name);
			else if(sfile->returnfunc_args)
				sfile->returnfunc_args(name, sfile->arg1, sfile->arg2);
		}
	}
}

static void do_filesel_buttons(short event, SpaceFile *sfile)
{
	char butname[FILE_MAX];
	
	if (event == B_FS_FILENAME) {
		if (strchr(sfile->file, '*') || strchr(sfile->file, '?') || strchr(sfile->file, '[')) {
			int i, match = FALSE;
			
			for (i = 2; i < sfile->totfile; i++) {
				if (fnmatch(sfile->file, sfile->filelist[i].relname, 0) == 0) {
					sfile->filelist[i].flags |= ACTIVE;
					match = TRUE;
				}
			}
			if (match) sfile->file[0] = '\0';
			if(sfile->type==FILE_MAIN) filesel_select_objects(sfile);
			scrarea_queue_winredraw(curarea);
		}
	}
	else if(event== B_FS_DIRNAME) {
		/* reuse the butname variable */
		
		/* convienence shortcut '~' -> $HOME
		 * If the first char is ~ then this is invalid on all OS's so its safe to replace with home */
		if ( sfile->dir[0] == '~' ) {
			if (sfile->dir[1] == '\0') {
				BLI_strncpy(sfile->dir, BLI_gethome(), sizeof(sfile->dir) );
			} else {
				/* replace ~ with home */
				char tmpstr[FILE_MAX];
				BLI_join_dirfile(tmpstr, BLI_gethome(), sfile->dir+1);
				BLI_strncpy(sfile->dir, tmpstr, sizeof(sfile->dir));
			}
		}
		
		BLI_cleanup_dir(G.sce, sfile->dir);

		BLI_make_file_string(G.sce, butname, sfile->dir, "");
		BLI_strncpy(sfile->dir, butname, sizeof(sfile->dir));

		/* strip the trailing slash if its a real dir */
		if (strlen(butname)!=1)
			butname[strlen(butname)-1]=0;
		
		if(sfile->type & FILE_UNIX) {
			if (!BLI_exists(butname)) {
				if (okee("Makedir")) {
					BLI_recurdir_fileops(butname);
					if (!BLI_exists(butname)) parent(sfile);
				} else parent(sfile);
			}
		}
		freefilelist(sfile);
		sfile->ofs= 0;
		scrarea_queue_winredraw(curarea);
	}
	else if(event== B_FS_DIR_MENU) {
		char *selected= fsmenu_get_entry(sfile->menu-1);
		
		/* which string */
		if (selected) {
			BLI_strncpy(sfile->dir, selected, sizeof(sfile->dir));
			BLI_make_exist(sfile->dir);
			BLI_cleanup_dir(G.sce, sfile->dir);
			freefilelist(sfile);
			sfile->ofs= 0;
			scrarea_queue_winredraw(curarea);
		}

		sfile->act= -1;
		
	}
	else if(event== B_FS_PARDIR) 
		parent(sfile);
	else if(event== B_FS_LOAD) {
		if(sfile->type) 
			filesel_execute(sfile);
	}
	else if(event== B_FS_CANCEL) 
		filesel_prevspace();
	else if(event== B_FS_LIBNAME) {
		Library *lib= BLI_findlink(&G.main->library, sfile->menu);
		if(lib) {
			BLI_strncpy(sfile->dir, lib->filename, sizeof(sfile->dir));
			BLI_make_exist(sfile->dir);
			BLI_cleanup_dir(G.sce, sfile->dir);
			freefilelist(sfile);
			sfile->ofs= 0;
			scrarea_queue_winredraw(curarea);
			sfile->act= -1;
		}
	}
	
}

/****/

typedef void (*ReplaceFP)(ID *oldblock, ID *newblock);

static void change_id_link(void *linkpv, void *newlinkv) {
	ID **linkp= (ID**) linkpv;
	ID *newlink= newlinkv;

	if (*linkp) {
		(*linkp)->us--;
	}
	(*linkp)= newlink;
	if (newlink) {
		id_us_plus(newlink);
	}
}

static void replace_image(ID *oldblock, ID *newblock) {
	Image *oldima= (Image*) oldblock;
	Image *newima= (Image*) newblock;
	bScreen *sc;
	Scene *sce;
	Tex *tex;
	Mesh *me;

	for (tex= G.main->tex.first; tex; tex= tex->id.next) {
		if (tex->env && tex->env->type == ENV_LOAD && tex->env->ima == oldima)
			change_id_link(&tex->env->ima, newima);
		if (tex->ima == oldima)
			change_id_link(&tex->ima, newima);
	}

	for (sce= G.main->scene.first; sce; sce= sce->id.next) {
		if (sce->ima == oldima)
			change_id_link(&sce->ima, newima);
	}

	for (sc= G.main->screen.first; sc; sc= sc->id.next) {
		ScrArea *sa;

		for (sa= sc->areabase.first; sa; sa= sa->next) {
			SpaceLink *sl;

			for (sl= sa->spacedata.first; sl; sl= sl->next) {
				if (sl->spacetype == SPACE_VIEW3D) {
					View3D *v3d= (View3D*) sl;
					BGpic *bgp= v3d->bgpic;

					if (bgp && bgp->ima == oldima) 
						change_id_link(&bgp->ima, newima);
				} else if (sl->spacetype == SPACE_IMAGE) {
					SpaceImage *sima= (SpaceImage*) sl;
					
					if (sima->image == oldima)
						change_id_link(&sima->image, newima);
				}
			}
		}
	}

	for (me= G.main->mesh.first; me; me= me->id.next) {
		int i, a;
		MTFace *tface;

		for(i=0; i<me->fdata.totlayer; i++) {
			if(me->fdata.layers[i].type == CD_MTFACE) {
				tface= (MTFace*)me->fdata.layers[i].data;

				for (a=0; a<me->totface; a++, tface++) {
					if (tface->tpage == oldima) {
							/* not change_id_link, tpage's aren't owners :(
							 * see hack below.
							 */
						tface->tpage= newima;
					}
				}
			}
		}
	}

		/* Nasty hack, necessary because tpages don't act
		 * as a user, so there lots of image user count
		 * munging occurs... this will ensure the image
		 * really dies.
		 */
	oldima->id.us= 0;
}

static void replace_material(ID *oldblock, ID *newblock)
{
	Material *old= (Material*) oldblock;
	Material *new= (Material*) newblock;
	Material ***matarar;
	ID *id;
	Object *ob;
	int a;
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->totcol && ob->id.lib==0) {
			matarar= give_matarar(ob);
			for(a=1; a<=ob->totcol; a++) {
				if(ob->mat[a-1] == old) {
					if(old) old->id.us--;
					id_us_plus((ID *)new);
					ob->mat[a-1]= new;
				}
				id= ob->data;
				if( (*matarar)[a-1] == old  && id->lib==0) {
					if(old) old->id.us--;
					id_us_plus((ID *)new);
					(*matarar)[a-1]= new;
				}
			}
		}
		ob= ob->id.next;
	}
}

static ReplaceFP get_id_replace_function(int idcode) {
	switch (idcode) {
	case ID_MA:
		return &replace_material;
	case ID_IM:
		return &replace_image;
	default:
		return NULL;
	}
}

static void databrowse_replace(SpaceFile *sfile, int idcode)
{
	ReplaceFP replace_func= get_id_replace_function(idcode);

	if (!replace_func) {
		error("Replacing %s blocks is unsupported", BLO_idcode_to_name(idcode));
	} else if (sfile->act==-1) {
		error("Select target with leftmouse");
	} else {
		ID *target= (ID*) sfile->filelist[sfile->act].poin;

		if (target) {
			char buf[128];

			sprintf(buf, "Replace with %s: %s", BLO_idcode_to_name(idcode), target->name+2);

			if (okee(buf)) {
				int i;

				for (i = 0; i <sfile->totfile; i++)
					if ((sfile->filelist[i].flags&ACTIVE) && sfile->filelist[i].poin!=target)
						replace_func(sfile->filelist[i].poin, target);
			}
		}
	}

	freefilelist(sfile);
	scrarea_queue_winredraw(curarea);
}

static void fs_fake_users(SpaceFile *sfile)
{
	ID *id;
	int a;
	
	/* only for F4 DATABROWSE */
	if(filesel_has_func(sfile)) return;
	
	for(a=0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & ACTIVE) {
			id= (ID *)sfile->filelist[a].poin;
			if(id) {
				if( id->flag & LIB_FAKEUSER) {
					id->flag -= LIB_FAKEUSER;
					id->us--;
				}
				else {
					id->flag |= LIB_FAKEUSER;
					id->us++;
				}
			}
		}
	}
	freefilelist(sfile);
	scrarea_queue_winredraw(curarea);
}


static int get_hilited_entry(SpaceFile *sfile)
{
	int a;

	for(a=0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & HILITE) {
			return a;
		}
	}
	return -1;
}


void winqreadfilespace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
	static int acto=0;
	SpaceFile *sfile;
	int act, do_draw= 0, i, test, ret = 0;
	short qual, mval[2];
	char str[FILE_MAX+12];
	
	sfile= curarea->spacedata.first;
	if(sfile==0) return;
	if(sfile->filelist==0) {
		return;
	}
	
	if(curarea->win==0) return;
	calc_file_rcts(sfile);
	getmouseco_areawin(mval);

	/* prevent looping */
	if(selecting && !(get_mbut() & R_MOUSE)) selecting= 0;

	if(val) {

		if( event!=RETKEY && event!=PADENTER)
			if( uiDoBlocks(&curarea->uiblocks, event, 1)!=UI_NOTHING ) event= 0;

		switch(event) {
		
		case UI_BUT_EVENT:
			do_filesel_buttons(val, sfile);
			break;		
		
		case WHEELDOWNMOUSE:
			do_filescrollwheel(sfile, U.wheellinescroll);
			act= find_active_file(sfile, mval[0], mval[1]);
			set_active_file(sfile, act);
			do_draw= 1;
			break;
		case WHEELUPMOUSE:
			do_filescrollwheel(sfile, -U.wheellinescroll);
			act= find_active_file(sfile, mval[0], mval[1]);
			set_active_file(sfile, act);
			do_draw= 1;
			break;

		case LEFTMOUSE:
		case MIDDLEMOUSE:
			if(mval[0]>scrollrct.xmin && mval[0]<scrollrct.xmax && mval[1]>scrollrct.ymin && mval[1]<scrollrct.ymax) {
				do_filescroll(sfile);
			}
			else if(mval[0]>textrct.xmin && mval[0]<textrct.xmax && mval[1]>textrct.ymin && mval[1]<textrct.ymax) {
				
				/* sfile->act is used in databrowse: double names of library objects */
				
				sfile->act= act= find_active_file(sfile, mval[0], mval[1]);
				
				if(act>=0 && act<sfile->totfile) {
					if(S_ISDIR(sfile->filelist[act].type)) {
						/* the path is too long and we are not going up! */
						if (strcmp(sfile->filelist[act].relname, ".") &&
							strcmp(sfile->filelist[act].relname, "..") &&
							strlen(sfile->dir) + strlen(sfile->filelist[act].relname) >= FILE_MAX ) 
						{
							error("Path too long, cannot enter this directory");
						} else {
							strcat(sfile->dir, sfile->filelist[act].relname);
							strcat(sfile->dir,"/");
							BLI_cleanup_dir(G.sce, sfile->dir);
							freefilelist(sfile);						
							sfile->ofs= 0;
							do_draw= 1;
						}
					} else {
						if( strcmp(sfile->file, sfile->filelist[act].relname)) {
							BLI_strncpy(sfile->file, sfile->filelist[act].relname, sizeof(sfile->file));
							do_draw = 1;
							
#ifdef INTERNATIONAL
							if (sfile->type==FILE_LOADFONT && event!=MIDDLEMOUSE) {
								/* Font Preview */
								char tmpstr[240];
								if (sfile->f_fp) {
									sprintf (tmpstr, "%s%s", sfile->dir, sfile->file);
									
									if (!FTF_GetNewFont ((const unsigned char *)tmpstr, 0, U.fontsize)) {
										error ("No font file");
									}
								}
							}
#endif
						}
						if(event==MIDDLEMOUSE && sfile->type) filesel_execute(sfile);
					}
				}
			}
			break;
		case RIGHTMOUSE:
			act= find_active_file(sfile, mval[0], mval[1]);
			acto= act;
			if(act>=0 && act<sfile->totfile) {

				if (sfile->filelist[act].flags & ACTIVE) {
					sfile->filelist[act].flags &= ~ACTIVE;
					selecting = INACTIVATE;
				}
				else {
					test= sfile->filelist[act].relname[0];
					if (act>=2 || test!='.') sfile->filelist[act].flags |= ACTIVE;
					
					selecting = ACTIVATE;
				}
				do_draw= 1;
			}
			break;
		case MOUSEY:
			act= find_active_file(sfile, mval[0], mval[1]);
			if (act!=acto) {
				set_active_file(sfile, act);
			}
			if(selecting && act!=acto) {
					
				while(1) {
					if (acto >= 2 && acto < sfile->totfile) {
						if (selecting == ACTIVATE) sfile->filelist[acto].flags |= ACTIVE;
						else if (selecting == INACTIVATE) sfile->filelist[acto].flags &= ~ACTIVE;
					}
					if (acto < act) acto++;
					else if (acto > act) acto--;
					else break;
					
				}

			}
			acto= act;
			break;
		
		case PAGEUPKEY:
			sfile->ofs-= page_ofs;
			do_draw= 1;
			break;
		case PAGEDOWNKEY:
			sfile->ofs+= page_ofs;
			do_draw= 1;
			break;
		case HOMEKEY:
			sfile->ofs= 0;
			do_draw= 1;
			break;
		case ENDKEY:
			sfile->ofs= sfile->totfile;
			do_draw= 1;
			break;
		
		case AKEY:
			swapselect_file(sfile);
			if(sfile->type==FILE_MAIN) filesel_select_objects(sfile);
			do_draw= 1;
			break;
			
		case BKEY:
		case CKEY:
		case LKEY:
			if(event==LKEY && sfile->type==FILE_MAIN && (G.qual & LR_CTRLKEY)) {
				databrowse_replace(sfile, groupname_to_code(sfile->dir));
				break;
			}
			/* pass */
		case MKEY:
			if(sfile->type==FILE_MAIN) break;

			if(!countselect(sfile)) {
				error("No files selected");
				break;
			}
			
			if(!getotherdir()) {
				error("No second fileselect");
				break;
			}
			
			if (!strcmp(sfile->dir, otherdir)) {
				error("Same directories");
				break;
			}

			if(event==BKEY) sprintf(str, "Backup to %s", otherdir);
			else if(event==CKEY) sprintf(str, "Copy to %s", otherdir);
			else if(event==LKEY) sprintf(str, "Linked copy to %s", otherdir);
			else if(event==MKEY) sprintf(str, "Move to %s", otherdir);
					
			if (!okee(str)) break;

			for (i = 0; i<sfile->totfile; i++){
				if (sfile->filelist[i].flags & ACTIVE) {			
					BLI_make_file_string(G.sce, str, sfile->dir, sfile->filelist[i].relname);

					if(event==CKEY) ret= BLI_copy_fileops(str, otherdir);
					else if(event==LKEY) ret= BLI_link(str, otherdir);
					else if(event==MKEY) ret= BLI_move(str, otherdir);

					if (ret) {error("Command failed, see console"); break;}
					else sfile->filelist[i].flags &= ~ACTIVE;
				}
			}
			do_draw= 1;
			if(event==BKEY || event==MKEY) 
				freefilelist(sfile);
				
			reread_other_fs();
			
			break;

		case XKEY:
			test = get_hilited_entry(sfile);

			if (test != -1 && !(S_ISDIR(sfile->filelist[test].type))){
				BLI_make_file_string(G.sce, str, sfile->dir, sfile->filelist[test].relname);

				if( okee("Remove %s", str) ) {
					ret = BLI_delete(str, 0, 0);
					if (ret) {
						error("Command failed, see console");
					} else {
						freefilelist(sfile);
						do_draw= 1;
					}
				}
			}
			break;

		case RKEY:
			if(sfile->type==FILE_MAIN) {
				databrowse_replace(sfile, groupname_to_code(sfile->dir));
				break;
			}
			/* pass to TKEY! */
			
		case TKEY:
			if(sfile->type==FILE_MAIN) break;
			
			if(!countselect(sfile)) {
				error("No files selected");
				break;
			}

			if(event==TKEY) sprintf(str, "Touch");
			else if(event==RKEY) sprintf(str, "Remove from %s", sfile->dir);
			
			qual= G.qual;	/* because after okee() you released the SHIFT */
			if (!okee(str)) break;
			
			for (i = 0; i <sfile->totfile; i++) {
				if (sfile->filelist[i].flags & ACTIVE) {
					BLI_make_file_string(G.sce, str, sfile->dir, sfile->filelist[i].relname);

					if(event==TKEY) ret= BLI_touch(str);
					else if(event==RKEY) {
						if(qual & LR_SHIFTKEY) ret= BLI_delete(str, 0, 1);
						else if(S_ISDIR(sfile->filelist[i].type)) ret= BLI_delete(str, 1, 0);
						else ret= BLI_delete(str, 0, 0);
					}

					if (ret) {error("Command failed, see console"); break;}
					else sfile->filelist[i].flags &= ~ACTIVE;
				}
			}
			do_draw= 1;
			freefilelist(sfile);

			break;
				
		case PKEY:
			if(G.qual & LR_SHIFTKEY) {
				extern char bprogname[];	/* usiblender.c */
#ifdef WIN32			
				sprintf(str, "%s -a \"%s%s\"", bprogname, sfile->dir, sfile->file);
#else
				sprintf(str, "\"%s\" -a \"%s%s\"", bprogname, sfile->dir, sfile->file);
#endif
				system(str);
			}
			else 
				parent(sfile);
				
			break;

		case IKEY:
			if(sfile->type==FILE_MAIN) break;
			
			sprintf(str, "$IMAGEEDITOR %s%s", sfile->dir, sfile->file);
			system(str);
			break;
		
		case EKEY:
			if(sfile->type==FILE_MAIN) break;
			
			sprintf(str, "$WINEDITOR %s%s", sfile->dir, sfile->file);
			system(str);
			break;
		
		case FKEY:
			if(sfile->type==FILE_MAIN) {
				fs_fake_users(sfile);
			}
			break;
		case HKEY:
			sfile->flag ^= FILE_HIDE_DOT;
			BLI_hide_dot_files(sfile->flag & FILE_HIDE_DOT);
			freefilelist(sfile);
			scrarea_queue_winredraw(curarea);
			break;
		case PADPLUSKEY:
		case EQUALKEY:
			if (G.qual & LR_CTRLKEY) BLI_newname(sfile->file, +100);
			else if (G.qual & LR_SHIFTKEY) BLI_newname(sfile->file, +10);
			else BLI_newname(sfile->file, +1);
			
			do_draw= 1;
			break;
			
		case PADMINUS:
		case MINUSKEY:
			if (G.qual & LR_CTRLKEY) BLI_newname(sfile->file, -100);
			else if (G.qual & LR_SHIFTKEY) BLI_newname(sfile->file, -10);
			else BLI_newname(sfile->file, -1);
			
			do_draw= 1;
			break;
			
		case BACKSLASHKEY:
		case SLASHKEY:
			if(sfile->type==FILE_MAIN) break;

#ifdef WIN32
			BLI_strncpy(sfile->dir, "\\", sizeof(sfile->dir));
#else
			BLI_strncpy(sfile->dir, "/", sizeof(sfile->dir));
#endif
			freefilelist(sfile);
			sfile->ofs= 0;
			do_draw= 1;
			break;
		case PERIODKEY:
			freefilelist(sfile);
			do_draw= 1;
			break;
		case ESCKEY:
			filesel_prevspace();
			break;
		case PADENTER:
		case RETKEY:
			if(sfile->type) filesel_execute(sfile);
			break;
		}
	}
	else if(event==RIGHTMOUSE) {
		selecting = NOTACTIVE;
		if(sfile->type==FILE_MAIN) filesel_select_objects(sfile);
	}
	else if(event==LEFTMOUSE) {
		if(sfile->type==FILE_MAIN) active_file_object(sfile);
	}

		/* XXX, stupid patch, curarea can become undone
		 * because of file loading... fixme zr
		 */
	if(do_draw && curarea) scrarea_queue_winredraw(curarea);
}




/* ************* LIBRARY FILESEL ******************* */

static int groupname_to_code(char *group)
{
	char buf[GROUP_MAX];
	char *lslash;
	
	BLI_strncpy(buf, group, GROUP_MAX);
	lslash= BLI_last_slash(buf);
	if (lslash)
		lslash[0]= '\0';

	return BLO_idcode_from_name(buf);
}

static int is_a_library(SpaceFile *sfile, char *dir, char *group)
{
	/* return ok when a blenderfile, in dir is the filename,
	 * in group the type of libdata
	 */
	int len;
	char *fd;
	
	strcpy(dir, sfile->dir);
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
		/* the last part of the dir is a .blend file, no group follows */
		*fd= '/'; /* put back the removed slash separating the dir and the .blend file name */
	}
	else {		
		char *gp = fd+1; // in case we have a .blend file, gp points to the group

		/* Find the last slash */
		fd= (strrchr(dir, '/')>strrchr(dir, '\\'))?strrchr(dir, '/'):strrchr(dir, '\\');
		if (!fd || !BLO_has_bfile_extension(fd+1)) return 0;

		/* now we know that we are in a blend file and it is safe to 
		   assume that gp actually points to a group */
		if (BLI_streq("Screen", gp)==0)
			BLI_strncpy(group, gp, GROUP_MAX);
	}
	return 1;
}

static void do_library_append(SpaceFile *sfile)
{
	Library *lib;
	char dir[FILE_MAX], group[GROUP_MAX];
	
	if ( is_a_library(sfile, dir, group)==0 ) {
		error("Not a library");
	} else if (!sfile->libfiledata) {
		error("Library not loaded");
	} else if (group[0]==0) {
		error("Nothing indicated");
	} else if (BLI_streq(G.main->name, dir)) {
		error("Cannot use current file as library");
	} else {
		Object *ob;
		int idcode = groupname_to_code(group);
		
		if((sfile->flag & FILE_LINK)==0)
			/* tag everything, all untagged data can be made local */
			flag_all_listbases_ids(LIB_APPEND_TAG, 1);
		
		BLO_library_append(sfile, dir, idcode);
		
		/* DISPLISTS? */
		ob= G.main->object.first;
		while(ob) {
			if(ob->id.lib) {
				ob->recalc |= OB_RECALC;
			}
			ob= ob->id.next;
		}
	
		/* and now find the latest append lib file */
		lib= G.main->library.first;
		while(lib) {
			if (BLI_streq(dir, lib->filename)) break;
			lib= lib->id.next;
		}
		
		/* make local */
		if(lib && (sfile->flag & FILE_LINK)==0) {
			all_local(lib, 1);
			/* important we unset, otherwise these object wont
			 * link into other scenes from this blend file */
			flag_all_listbases_ids(LIB_APPEND_TAG, 0);
		}
		
		DAG_scene_sort(G.scene);
		
		/* in sfile->dir is the whole lib name */
		BLI_strncpy(G.lib, sfile->dir, sizeof(G.lib) );
	}
}

static void library_to_filelist(SpaceFile *sfile)
{
	LinkNode *l, *names;
	int ok, i, nnames, idcode;
	char filename[FILE_MAX];
	char dir[FILE_MAX], group[GROUP_MAX];
	
	/* name test */
	ok= is_a_library(sfile, dir, group);
	if (!ok) {
		/* free */
		if(sfile->libfiledata) BLO_blendhandle_close(sfile->libfiledata);
		sfile->libfiledata= 0;
		return;
	}
	
	BLI_strncpy(filename, G.sce, sizeof(filename));	// G.sce = last file loaded, for UI
	
	/* there we go */
	/* for the time being only read filedata when libfiledata==0 */
	if (sfile->libfiledata==0) {
		sfile->libfiledata= BLO_blendhandle_from_file(dir);	// this sets G.sce, we dont want it
		
		if(sfile->libfiledata==0) return;
	}
	
	idcode= groupname_to_code(group);

		// memory for strings is passed into filelist[i].relname
		// and free'd in freefilelist
	if (idcode) {
		names= BLO_blendhandle_get_datablock_names(sfile->libfiledata, idcode);
	} else {
		names= BLO_blendhandle_get_linkable_groups(sfile->libfiledata);
	}
	
	nnames= BLI_linklist_length(names);

	sfile->totfile= nnames + 2;
	sfile->filelist= malloc(sfile->totfile * sizeof(*sfile->filelist));
	memset(sfile->filelist, 0, sfile->totfile * sizeof(*sfile->filelist));

	sfile->filelist[0].relname= BLI_strdup(".");
	sfile->filelist[0].type |= S_IFDIR;
	sfile->filelist[1].relname= BLI_strdup("..");
	sfile->filelist[1].type |= S_IFDIR;
		
	for (i=0, l= names; i<nnames; i++, l= l->next) {
		char *blockname= BLI_strdup(l->link); 

		sfile->filelist[i + 2].relname= blockname;
		if (!idcode)
			sfile->filelist[i + 2].type |= S_IFDIR;
	}
		
	BLI_linklist_free(names, free);
	
	qsort(sfile->filelist, sfile->totfile, sizeof(struct direntry), compare_name);
	
	sfile->maxnamelen= 0;
	for(i=0; i<sfile->totfile; i++) {
		int len = BMF_GetStringWidth(G.font, sfile->filelist[i].relname);
		if (len > sfile->maxnamelen)
			sfile->maxnamelen = len;
	}
	
	BLI_strncpy(G.sce, filename, sizeof(filename));	// prevent G.sce to change

}

/* ******************* DATA SELECT ********************* */

static void filesel_select_objects(SpaceFile *sfile)
{
	Object *ob;
	Base *base;
	Scene *sce;
	int a;
	
	/* only when F4 DATABROWSE */
	if(filesel_has_func(sfile)) return;
	
	if( strcmp(sfile->dir, "Object/")==0 ) {
		for(a=0; a<sfile->totfile; a++) {
			
			ob= (Object *)sfile->filelist[a].poin;
			
			if(ob && (ob->flag & OB_RESTRICT_VIEW)==0) {
				if(sfile->filelist[a].flags & ACTIVE) ob->flag |= SELECT;
				else ob->flag &= ~SELECT;
			}

		}
		base= FIRSTBASE;
		while(base) {
			base->flag= base->object->flag;
			base= base->next;
		}
		countall();
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWOOPS, 0);
	}
	else if( strcmp(sfile->dir, "Scene/")==0 ) {
		
		for(a=0; a<sfile->totfile; a++) {
			
			sce= (Scene *)sfile->filelist[a].poin;
			if(sce) {
				if(sfile->filelist[a].flags & ACTIVE) sce->r.scemode |= R_BG_RENDER;
				else sce->r.scemode &= ~R_BG_RENDER;
			}

		}
		allqueue(REDRAWBUTSSCENE, 0);
	}
}

static void active_file_object(SpaceFile *sfile)
{
	Object *ob;
	
	/* only when F4 DATABROWSE */
	if(filesel_has_func(sfile)) return;
	
	if( strcmp(sfile->dir, "Object/")==0 ) {
		if(sfile->act >= 0 && sfile->act < sfile->totfile) {
			
			ob= (Object *)sfile->filelist[sfile->act].poin;
			
			if(ob && (ob->flag & OB_RESTRICT_VIEW)==0) {
				set_active_object(ob);
				if(BASACT && BASACT->object==ob) {
					BASACT->flag |= SELECT;
					sfile->filelist[sfile->act].flags |= ACTIVE;
					allqueue(REDRAWVIEW3D, 0);
					allqueue(REDRAWOOPS, 0);
					scrarea_queue_winredraw(curarea);
				}
			}
		}
	}
}


void main_to_filelist(SpaceFile *sfile)
{
	ID *id;
	struct direntry *files, *firstlib = NULL;
	ListBase *lb;
	int a, fake, idcode, len, ok, totlib, totbl;
	short hide = 0;

	if (sfile->flag & FILE_HIDE_DOT)
		hide = 1;

	if(sfile->dir[0]=='/') sfile->dir[0]= 0;
	
	if(sfile->dir[0]) {
		idcode= groupname_to_code(sfile->dir);
		if(idcode==0) sfile->dir[0]= 0;
	}
	
	if( sfile->dir[0]==0) {
		
		/* make directories */
		sfile->totfile= 24;
		sfile->filelist= (struct direntry *)malloc(sfile->totfile * sizeof(struct direntry));
		
		for(a=0; a<sfile->totfile; a++) {
			memset( &(sfile->filelist[a]), 0 , sizeof(struct direntry));
			sfile->filelist[a].type |= S_IFDIR;
		}
		
		sfile->filelist[0].relname= BLI_strdup("..");
		sfile->filelist[1].relname= BLI_strdup(".");
		sfile->filelist[2].relname= BLI_strdup("Scene");
		sfile->filelist[3].relname= BLI_strdup("Group");
		sfile->filelist[4].relname= BLI_strdup("Object");
		sfile->filelist[5].relname= BLI_strdup("Mesh");
		sfile->filelist[6].relname= BLI_strdup("Curve");
		sfile->filelist[7].relname= BLI_strdup("Metaball");
		sfile->filelist[8].relname= BLI_strdup("Material");
		sfile->filelist[9].relname= BLI_strdup("Texture");
		sfile->filelist[10].relname= BLI_strdup("Image");
		sfile->filelist[11].relname= BLI_strdup("Wave");
		sfile->filelist[12].relname= BLI_strdup("Lattice");
		sfile->filelist[13].relname= BLI_strdup("Lamp");
		sfile->filelist[14].relname= BLI_strdup("Camera");
		sfile->filelist[15].relname= BLI_strdup("Ipo");
		sfile->filelist[16].relname= BLI_strdup("World");
		sfile->filelist[17].relname= BLI_strdup("Screen");
		sfile->filelist[18].relname= BLI_strdup("VFont");
		sfile->filelist[19].relname= BLI_strdup("Text");
		sfile->filelist[20].relname= BLI_strdup("Armature");
		sfile->filelist[21].relname= BLI_strdup("Action");
		sfile->filelist[22].relname= BLI_strdup("NodeTree");
		sfile->filelist[23].relname= BLI_strdup("Brush");
		
		qsort(sfile->filelist, sfile->totfile, sizeof(struct direntry), compare_name);
	}
	else {

		/* make files */
		idcode= groupname_to_code(sfile->dir);
		
		lb= wich_libbase(G.main, idcode );
		if(lb==0) return;
		
		id= lb->first;
		sfile->totfile= 0;
		while(id) {
			if(filesel_has_func(sfile) && idcode==ID_IP) {
				if(sfile->ipotype== ((Ipo *)id)->blocktype) sfile->totfile++;
			}
			else if (hide==0 || id->name[2] != '.')
				sfile->totfile++;

			id= id->next;
		}
		
		if(!filesel_has_func(sfile)) sfile->totfile+= 2;
		sfile->filelist= (struct direntry *)malloc(sfile->totfile * sizeof(struct direntry));
		
		files= sfile->filelist;
		
		if(!filesel_has_func(sfile)) {
			memset( &(sfile->filelist[0]), 0 , sizeof(struct direntry));
			sfile->filelist[0].relname= BLI_strdup(".");
			sfile->filelist[0].type |= S_IFDIR;
			memset( &(sfile->filelist[1]), 0 , sizeof(struct direntry));
			sfile->filelist[1].relname= BLI_strdup("..");
			sfile->filelist[1].type |= S_IFDIR;
		
			files+= 2;
		}
		
		id= lb->first;
		totlib= totbl= 0;
		
		while(id) {
			
			ok= 0;
			if(filesel_has_func(sfile) && idcode==ID_IP) {
				if(sfile->ipotype== ((Ipo *)id)->blocktype) ok= 1;
			}
			else ok= 1;
			
			if(ok) {
		
				if (hide==0 || id->name[2] != '.') {
					memset( files, 0 , sizeof(struct direntry));
					if(id->lib==NULL)
						files->relname= BLI_strdup(id->name+2);
					else {
						char tmp[FILE_MAX], fi[FILE_MAXFILE];
						BLI_strncpy(tmp, id->lib->name, FILE_MAX);
						BLI_splitdirstring(tmp, fi);
						files->relname= MEM_mallocN(FILE_MAXFILE+32, "filename for lib");
						sprintf(files->relname, "%s / %s", fi, id->name+2);
					}
					
					if(!filesel_has_func(sfile)) { /* F4 DATA BROWSE */
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
		
		/* only qsort of libraryblokken */
		if(totlib>1) {
			qsort(firstlib, totlib, sizeof(struct direntry), compare_name);
		}
	}

	sfile->maxnamelen= 0;
	for(a=0; a<sfile->totfile; a++) {
		len = BMF_GetStringWidth(G.font, sfile->filelist[a].relname);
		if (len > sfile->maxnamelen) sfile->maxnamelen = len;
		
		if(filetoname) {
			if( strcmp(sfile->file, sfile->filelist[a].relname)==0) {
				sfile->ofs= a-( sfile->collums*(curarea->winy-FILESELHEAD-10)/(2*FILESEL_DY));
				filetoname= 0;
				if(filesel_has_func(sfile)) sfile->filelist[a].flags |= ACTIVE;
			}
		}
	}
}


void clever_numbuts_filesel()
{
	SpaceFile *sfile;
	char orgname[FILE_MAX+12];
	char filename[FILE_MAX+12];
	char newname[FILE_MAX+12];
	int test;
	int len;
	
	sfile= curarea->spacedata.first;

	if(sfile->type==FILE_MAIN) return;
	
	len = 110;
	test = get_hilited_entry(sfile);

	if (test != -1 && !(S_ISDIR(sfile->filelist[test].type))){
		BLI_make_file_string(G.sce, orgname, sfile->dir, sfile->filelist[test].relname);
		BLI_strncpy(filename, sfile->filelist[test].relname, sizeof(filename));
		
		add_numbut(0, TEX, "", 0, len, filename, "Rename File");

		if( do_clever_numbuts("Rename File", 1, REDRAW) ) {
			BLI_make_file_string(G.sce, newname, sfile->dir, filename);

			if( strcmp(orgname, newname) != 0 ) {
				BLI_rename(orgname, newname);
				freefilelist(sfile);
			}
		}

		scrarea_queue_winredraw(curarea);
	}
}

