/**
 * $Id$
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
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <io.h>
#include <direct.h>
#include "BLI_winstuff.h"
#else
#include <unistd.h>
#include <sys/times.h>
#endif   

#include <sys/stat.h>
#include <sys/types.h>
#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_linklist.h"
#include "BLI_storage_types.h"
#include "BLI_dynstr.h"

#include "IMB_imbuf.h"

#include "DNA_curve_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_vfont_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_displist.h"
#include "BKE_library.h"
#include "BKE_curve.h"
#include "BKE_font.h"
#include "BKE_material.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"
#include "BIF_editview.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_resources.h"

#include "BLO_readfile.h"

#include "BDR_editcurve.h"
#include "BSE_filesel.h"
#include "BSE_view.h"

#include "mydevice.h"
#include "blendef.h"
#include "render.h"
#include "nla.h"


#if defined WIN32 || defined __BeOS
	int fnmatch(){return 0;}
#else
	#include <fnmatch.h>
#endif

#ifndef WIN32
#include <sys/param.h>
#endif

#define FILESELHEAD		60
#define FILESEL_DY		16

#define NOTACTIVE			0
#define ACTIVATE			1
#define INACTIVATE			2

#define STARTSWITH(x, y) (strncmp(x, y, sizeof(x) - 1) == 0)

static int is_a_library(SpaceFile *sfile, char *dir, char *group);
static void do_library_append(SpaceFile *sfile);
static void library_to_filelist(SpaceFile *sfile);
static void filesel_select_objects(struct SpaceFile *sfile);
static void active_file_object(struct SpaceFile *sfile);
static int groupname_to_code(char *group);

/* local globals */

static rcti scrollrct, textrct, bar;
static int filebuty1, filebuty2, page_ofs, collumwidth, selecting=0;
static int filetoname= 0;
static float pixels_to_ofs;
static char otherdir[FILE_MAXDIR];
static ScrArea *otherarea;

/* FSMENU HANDLING */

	/* FSMenuEntry's without paths indicate seperators */
typedef struct _FSMenuEntry FSMenuEntry;
struct _FSMenuEntry {
	FSMenuEntry *next;

	char *path;
};

static FSMenuEntry *fsmenu= 0;

int fsmenu_get_nentries(void)
{
	FSMenuEntry *fsme;
	int count= 0;

	for (fsme= fsmenu; fsme; fsme= fsme->next) 
		count++;

	return count;
}
int fsmenu_is_entry_a_seperator(int idx)
{
	FSMenuEntry *fsme;

	for (fsme= fsmenu; fsme && idx; fsme= fsme->next)
		idx--;

	return (fsme && !fsme->path)?1:0;
}
char *fsmenu_get_entry(int idx)
{
	FSMenuEntry *fsme;

	for (fsme= fsmenu; fsme && idx; fsme= fsme->next)
		idx--;

	return fsme?fsme->path:NULL;
}
char *fsmenu_build_menu(void)
{
	DynStr *ds= BLI_dynstr_new();
	FSMenuEntry *fsme;
	char *menustr;

	for (fsme= fsmenu; fsme; fsme= fsme->next) {
		if (!fsme->path) {
				/* clean consecutive seperators and ignore trailing ones */
			if (fsme->next) {
				if (fsme->next->path) {
					BLI_dynstr_append(ds, "%l|");
				} else {
					FSMenuEntry *next= fsme->next;
					fsme->next= next->next;
					MEM_freeN(next);
				}
			}
		} else {
			BLI_dynstr_append(ds, fsme->path);
			if (fsme->next) BLI_dynstr_append(ds, "|");
		}
	}

	menustr= BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	return menustr;
}
static FSMenuEntry *fsmenu_get_last_separator(void) 
{
	FSMenuEntry *fsme, *lsep=NULL;

	for (fsme= fsmenu; fsme; fsme= fsme->next)
		if (!fsme->path)
			lsep= fsme;

	return lsep;
}
void fsmenu_insert_entry(char *path, int sorted)
{
	FSMenuEntry *prev= fsmenu_get_last_separator();
	FSMenuEntry *fsme= prev?prev->next:fsmenu;

	for (; fsme; prev= fsme, fsme= fsme->next) {
		if (fsme->path) {
			if (BLI_streq(path, fsme->path)) {
				return;
			} else if (sorted && strcmp(path, fsme->path)<0) {
				break;
			}
		}
	}
	
	fsme= MEM_mallocN(sizeof(*fsme), "fsme");
	fsme->path= BLI_strdup(path);

	if (prev) {
		fsme->next= prev->next;
		prev->next= fsme;
	} else {
		fsme->next= fsmenu;
		fsmenu= fsme;
	}
}
void fsmenu_append_seperator(void)
{
	if (fsmenu) {
		FSMenuEntry *fsme= fsmenu;

		while (fsme->next) fsme= fsme->next;

		fsme->next= MEM_mallocN(sizeof(*fsme), "fsme");
		fsme->next->next= NULL;
		fsme->next->path= NULL;
	}
}
void fsmenu_remove_entry(int idx)
{
	FSMenuEntry *prev= NULL, *fsme= fsmenu;

	for (fsme= fsmenu; fsme && idx; prev= fsme, fsme= fsme->next)
		if (fsme->path)
			idx--;

	if (fsme) {
		if (prev) {
			prev->next= fsme->next;
		} else {
			fsmenu= fsme->next;
		}

		MEM_freeN(fsme->path);
		MEM_freeN(fsme);
	}
}
void fsmenu_free(void)
{
	FSMenuEntry *fsme= fsmenu;

	while (fsme) {
		FSMenuEntry *n= fsme->next;

		if (fsme->path) MEM_freeN(fsme->path);
		MEM_freeN(fsme);

		fsme= n;
	}
}

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
	return (strcasecmp(entry1->relname,entry2->relname));
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

/*
	if ( entry1->s.st_ctime < entry2->s.st_ctime) return 1;
	if ( entry1->s.st_ctime > entry2->s.st_ctime) return -1;
*/
	if ( entry1->s.st_mtime < entry2->s.st_mtime) return 1;
	if ( entry1->s.st_mtime > entry2->s.st_mtime) return -1;
	else return strcasecmp(entry1->relname,entry2->relname);
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

	if ( entry1->s.st_size < entry2->s.st_size) return 1;
	if ( entry1->s.st_size > entry2->s.st_size) return -1;
	else return strcasecmp(entry1->relname,entry2->relname);
}


/* **************************************** */

void clear_global_filesel_vars()
{
	selecting= 0;
}


void filesel_statistics(SpaceFile *sfile, int *totfile, int *selfile, float *totlen, float *sellen)
{
	int a, len;
	
	*totfile= *selfile= 0;
	*totlen= *sellen= 0;
	
	if(sfile->filelist==0) return;
	
	for(a=0; a<sfile->totfile; a++) {
		if( (sfile->filelist[a].type & S_IFDIR)==0 ) {
			(*totfile) ++;

			len = sfile->filelist[a].s.st_size;
			(*totlen) += (len/1048576.0); 		

			if(sfile->filelist[a].flags & ACTIVE) {
				(*selfile) ++;
				(*sellen) += (len/1048576.0);
			}
		}
	}
}

/* *************** HELP FUNCTIONS ******************* */

/* This is a really ugly function... its purpose is to
 * take the space file name and clean it up, replacing
 * excess file entry stuff (like /tmp/../tmp/../)
 */

void checkdir(char *dir)
{
	short a;
	char *start, *eind;
	char tmp[FILE_MAXDIR+FILE_MAXFILE];

	BLI_make_file_string(G.sce, tmp, dir, "");
	strcpy(dir, tmp);
	
#ifdef WIN32
	if(dir[0]=='.') {	/* happens for example in FILE_MAIN */
		dir[0]= '\\';
		dir[1]= 0;
		return;
	}	

	while (start = strstr(dir, "\\..\\")) {
		eind = start + strlen("\\..\\") - 1;
		a = start-dir-1;
		while (a>0) {
			if (dir[a] == '\\') break;
			a--;
		}
		strcpy(dir+a,eind);
	}

	while (start = strstr(dir,"\\.\\")){
		eind = start + strlen("\\.\\") - 1;
		strcpy(start,eind);
	}

	while (start = strstr(dir,"\\\\" )){
		eind = start + strlen("\\\\") - 1;
		strcpy(start,eind);
	}

	if(a = strlen(dir)){				/* remove the '\\' at the end */
		while(a>0 && dir[a-1] == '\\'){
			a--;
			dir[a] = 0;
		}
	}

	strcat(dir, "\\");
#else	
	if(dir[0]=='.') {	/* happens, for example in FILE_MAIN */
		dir[0]= '/';
		dir[1]= 0;
		return;
	}	
	
	while ( (start = strstr(dir, "/../")) ) {
		eind = start + strlen("/../") - 1;
		a = start-dir-1;
		while (a>0) {
			if (dir[a] == '/') break;
			a--;
		}
		strcpy(dir+a,eind);
	}

	while ( (start = strstr(dir,"/./")) ){
		eind = start + strlen("/./") - 1;
		strcpy(start,eind);
	}

	while ( (start = strstr(dir,"//" )) ){
		eind = start + strlen("//") - 1;
		strcpy(start,eind);
	}

	if( (a = strlen(dir)) ){				/* remove all '/' at the end */
		while(dir[a-1] == '/'){
			a--;
			dir[a] = 0;
			if (a<=0) break;
		}
	}

	strcat(dir, "/");
#endif
}

void test_flags_file(SpaceFile *sfile)
{
	struct direntry *file;
	int num;

	file= sfile->filelist;
	
	for(num=0; num<sfile->totfile; num++, file++) {
		file->flags= 0;
		file->type= file->s.st_mode;	/* restore the mess below */ 
		
			/* Don't check extensions for directories */ 
		if (file->type&S_IFDIR)
			continue;
			
		if(sfile->type==FILE_BLENDER || sfile->type==FILE_LOADLIB) {
			if(BLO_has_bfile_extension(file->relname)) {
				file->flags |= BLENDERFILE;
				if(sfile->type==FILE_LOADLIB) {
					file->type &= ~S_IFMT;
					file->type |= S_IFDIR;
				}
			}
			else if(BLI_testextensie(file->relname, ".psx")) {
				file->flags |= PSXFILE;
			}
		} else if (sfile->type==FILE_SPECIAL){
			if(BLI_testextensie(file->relname, ".py")) {
				file->flags |= PYSCRIPTFILE;			
			} else if( BLI_testextensie(file->relname, ".ttf")
					|| BLI_testextensie(file->relname, ".ttc")
					|| BLI_testextensie(file->relname, ".pfb")
					|| BLI_testextensie(file->relname, ".otf")
					|| BLI_testextensie(file->relname, ".otc")) {
				file->flags |= FTFONTFILE;			
			} else if (G.have_quicktime){
				if(		BLI_testextensie(file->relname, ".jpg")
					||	BLI_testextensie(file->relname, ".jpeg")
					||	BLI_testextensie(file->relname, ".tga")
					||	BLI_testextensie(file->relname, ".rgb")
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
#ifdef WITH_FREEIMAGE
				||	BLI_testextensie(file->relname, ".jng")
				||	BLI_testextensie(file->relname, ".mng")
				||	BLI_testextensie(file->relname, ".pbm")
				||	BLI_testextensie(file->relname, ".pgm")
				||	BLI_testextensie(file->relname, ".ppm")
				||	BLI_testextensie(file->relname, ".wbmp")
				||	BLI_testextensie(file->relname, ".cut")
				||	BLI_testextensie(file->relname, ".ico")
				||	BLI_testextensie(file->relname, ".koala")
				||	BLI_testextensie(file->relname, ".pcd")
				||	BLI_testextensie(file->relname, ".pcx")
				||	BLI_testextensie(file->relname, ".ras")
#endif
					||	BLI_testextensie(file->relname, ".sgi")) {
					file->flags |= IMAGEFILE;			
				}
				else if(BLI_testextensie(file->relname, ".avi")
					||	BLI_testextensie(file->relname, ".flc")
					||	BLI_testextensie(file->relname, ".mov")
					||	BLI_testextensie(file->relname, ".movie")
					||	BLI_testextensie(file->relname, ".mv")) {
					file->flags |= MOVIEFILE;			
				}
			} else { // no quicktime
				if(BLI_testextensie(file->relname, ".jpg")
					||	BLI_testextensie(file->relname, ".tga")
					||	BLI_testextensie(file->relname, ".rgb")
					||	BLI_testextensie(file->relname, ".bmp")
					||	BLI_testextensie(file->relname, ".png")
					||	BLI_testextensie(file->relname, ".iff")
					||	BLI_testextensie(file->relname, ".lbm")
#ifdef WITH_FREEIMAGE
				||	BLI_testextensie(file->relname, ".jng")
				||	BLI_testextensie(file->relname, ".mng")
				||	BLI_testextensie(file->relname, ".pbm")
				||	BLI_testextensie(file->relname, ".pgm")
				||	BLI_testextensie(file->relname, ".ppm")
				||	BLI_testextensie(file->relname, ".wbmp")
				||	BLI_testextensie(file->relname, ".cut")
				||	BLI_testextensie(file->relname, ".ico")
				||	BLI_testextensie(file->relname, ".koala")
				||	BLI_testextensie(file->relname, ".pcd")
				||	BLI_testextensie(file->relname, ".pcx")
				||	BLI_testextensie(file->relname, ".ras")
				||	BLI_testextensie(file->relname, ".gif")
				||	BLI_testextensie(file->relname, ".psd")
				||	BLI_testextensie(file->relname, ".tif")
				||	BLI_testextensie(file->relname, ".tiff")
#endif
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
		qsort(sfile->filelist, sfile->totfile, sizeof(struct direntry), compare_name);	
		break;
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
	char wdir[FILE_MAXDIR];

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
		free(sfile->filelist[num].relname);
		
		if (sfile->filelist[num].string) free(sfile->filelist[num].string);
	}
	free(sfile->filelist);
	sfile->filelist= 0;
}

static void split_sfile(SpaceFile *sfile, char *s1)
{
	char string[FILE_MAXDIR+FILE_MAXFILE], dir[FILE_MAXDIR], file[FILE_MAXFILE];

	strcpy(string, s1);

	BLI_split_dirfile(string, dir, file);
	
	if(sfile->filelist) {
		if(strcmp(dir, sfile->dir)!=0) {
			freefilelist(sfile);
		}
		else test_flags_file(sfile);
	}
	strcpy(sfile->file, file);
	BLI_make_file_string(G.sce, sfile->dir, dir, "");
}


void parent(SpaceFile *sfile)
{
	short a;
	char *dir;
	
	/* if databrowse: no parent */
	if(sfile->type==FILE_MAIN && sfile->returnfunc) return;

	dir= sfile->dir;
	
#ifdef WIN32
	if(a = strlen(dir)) {				/* remove all '/' at the end */
		while(dir[a-1] == '\\') {
			a--;
			dir[a] = 0;
			if (a<=0) break;
		}
	}
	if(a = strlen(dir)) {				/* then remove all until '/' */
		while(dir[a-1] != '\\') {
			a--;
			dir[a] = 0;
			if (a<=0) break;
		}
	}
	if (a = strlen(dir)) {
		if (dir[a-1] != '\\') strcat(dir,"\\");
	}
	else if(sfile->type!=FILE_MAIN) strcpy(dir,"\\");
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
	int ofs;
	
	if(y > textrct.ymax) y= textrct.ymax;
	if(y <= textrct.ymin) y= textrct.ymin+1;
	
	ofs= (x-textrct.xmin)/collumwidth;
	if(ofs<0) ofs= 0;
	ofs*= (textrct.ymax-textrct.ymin);

	return sfile->ofs+ (ofs+textrct.ymax-y)/FILESEL_DY;
	
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

	if((U.flag & FSCOLLUM)==0) if(sfile->type!=FILE_MAIN) sfile->collums= 1;
	
	collumwidth= (textrct.xmax-textrct.xmin)/sfile->collums;
	

	totfile= sfile->totfile + 0.5;

	tot= FILESEL_DY*totfile;
	if(tot) fac= ((float)sfile->collums*(scrollrct.ymax-scrollrct.ymin))/( (float)tot);
	else fac= 1.0;
	
	if(sfile->ofs<0) sfile->ofs= 0;
	
	if(tot) start= ( (float)sfile->ofs)/(totfile);
	else start= 0.0;
	if(fac>1.0) fac= 1.0;

	if(start+fac>1.0) {
		sfile->ofs= ceil((1.0-fac)*totfile);
		start= ( (float)sfile->ofs)/(totfile);
		fac= 1.0-start;
	}

	bar.xmin= scrollrct.xmin+2;
	bar.xmax= scrollrct.xmax-2;
	h= (scrollrct.ymax-scrollrct.ymin)-4;
	bar.ymax= scrollrct.ymax-2- start*h;
	bar.ymin= bar.ymax- fac*h;

	pixels_to_ofs= (totfile)/(float)(h+3);
	page_ofs= fac*totfile;
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

static void regelrect(int id, int x, int y)
{
	if(id & ACTIVE) {
		if(id & HILITE) BIF_ThemeColorShade(TH_HILITE, 20);
		else BIF_ThemeColor(TH_HILITE);
	}
	else if(id & HILITE) BIF_ThemeColorShade(TH_BACK, 20);
	else BIF_ThemeColor(TH_BACK);
	
	glRects(x-17,  y-3,  x+collumwidth-21,  y+11);

}

static void printregel(SpaceFile *sfile, struct direntry *files, int x, int y)
{
	int boxcol=0;
	char *s;

	boxcol= files->flags & (HILITE + ACTIVE);

	if(boxcol) {
		regelrect(boxcol, x, y);
	}

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
		BMF_DrawString(G.font, files->relname);
		
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


static int calc_filesel_regel(SpaceFile *sfile, int nr, int *valx, int *valy)
{
	/* get screen coordinate of a 'regel', dutch for line */
	int val, coll;

	nr-= sfile->ofs;

	/* amount of lines */
	val= (textrct.ymax-textrct.ymin)/FILESEL_DY;
	coll= nr/val;
	nr -= coll*val;
	
	*valy= textrct.ymax-FILESEL_DY+3 - nr*FILESEL_DY;
	*valx= coll*collumwidth + textrct.xmin+20;
	
	if(nr<0 || coll > sfile->collums) return 0;
	return 1;
}

static void set_active_file(SpaceFile *sfile, int act)
{
	struct direntry *file;
	int num, redraw= 0, newflag;
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
	
	if(redraw==2) {
		int x, y;
		
		glDrawBuffer(GL_FRONT);

		glScissor(curarea->winrct.xmin, curarea->winrct.ymin, curarea->winx-12, curarea->winy);

		if( calc_filesel_regel(sfile, old, &x, &y) ) {
			regelrect(0, x, y);
			printregel(sfile, sfile->filelist+old, x, y);
		}
		if( calc_filesel_regel(sfile, newi, &x, &y) ) {
			printregel(sfile, sfile->filelist+newi, x, y);
		}
		
		glScissor(curarea->winrct.xmin, curarea->winrct.ymin, curarea->winx, curarea->winy);

		glFinish();		/* for geforce, to show it in the frontbuffer */
		glDrawBuffer(GL_BACK);
	}
	else if(redraw) {
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
	
		if( calc_filesel_regel(sfile, a, &x, &y)==0 ) break;
		
		printregel(sfile, files, x, y);
	}

	/* clear drawing errors, with text at the right hand side: */
	BIF_ThemeColor(TH_HEADER);
	glRecti(textrct.xmax,  textrct.ymin,  textrct.xmax+10,  textrct.ymax);
	uiEmboss(textrct.xmin, textrct.ymin, textrct.xmax, textrct.ymax, 1);
}

void drawfilespace(ScrArea *sa, void *spacedata)
{
	SpaceFile *sfile;
	uiBlock *block;
	float col[3];
	int act, loadbutton;
	short mval[2];
	char name[20];
	char *menu;

	myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);

	BIF_GetThemeColor3fv(TH_HEADER, col);	// basic undrawn color is border
	glClearColor(col[0], col[1], col[2], 0.0); 
	glClear(GL_COLOR_BUFFER_BIT);

	sfile= curarea->spacedata.first;	
	if(sfile->filelist==0) {
		read_dir(sfile);
		
		calc_file_rcts(sfile);
		
		/* calculate act */ 
		getmouseco_areawin(mval);
		act= find_active_file(sfile, mval[0], mval[1]);
		if(act>=0 && act<sfile->totfile)
			sfile->filelist[act].flags |= HILITE;
	}
	else calc_file_rcts(sfile);

	/* HEADER */
	sprintf(name, "win %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, name, UI_EMBOSS, UI_HELV, curarea->win);
	
	uiSetButLock( sfile->type==FILE_MAIN && sfile->returnfunc, NULL);

	/* space available for load/save buttons? */
	loadbutton= MAX2(80, 20+BMF_GetStringWidth(G.font, sfile->title));
	if(textrct.xmax-textrct.xmin > loadbutton+20) {
		if(sfile->title[0]==0) loadbutton= 0;
	}
	else loadbutton= 0;

	uiDefBut(block, TEX,	    1,"",	textrct.xmin, filebuty1, textrct.xmax-textrct.xmin-loadbutton, 21, sfile->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	uiDefBut(block, TEX,	    2,"",	textrct.xmin, filebuty2, textrct.xmax-textrct.xmin-loadbutton, 21, sfile->dir, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	if(loadbutton) {
		uiSetCurFont(block, UI_HELV);
		uiDefBut(block, BUT,	    5, sfile->title,	textrct.xmax-loadbutton, filebuty2, loadbutton, 21, sfile->dir, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
		uiDefBut(block, BUT,	    6, "Cancel",	textrct.xmax-loadbutton, filebuty1, loadbutton, 21, sfile->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	}

	menu= fsmenu_build_menu();
	if(menu[0])	// happens when no .Bfs is there, and first time browse
		uiDefButS(block, MENU,	3, menu, scrollrct.xmin, filebuty1, scrollrct.xmax-scrollrct.xmin, 21, &sfile->menu, 0, 0, 0, 0, "");
	MEM_freeN(menu);

	uiDefBut(block, BUT,		4, "P", scrollrct.xmin, filebuty2, scrollrct.xmax-scrollrct.xmin, 21, 0, 0, 0, 0, 0, "Move to the parent directory (PKEY)");

	uiDrawBlock(block);

	draw_filescroll(sfile);
	draw_filetext(sfile);
	
	/* others diskfree etc ? */
	scrarea_queue_headredraw(curarea);	
	
	myortho2(-0.375, (float)(sa->winx)-0.375, -0.375, (float)(sa->winy)-0.375);
	draw_area_emboss(sa);
	
	curarea->win_swap= WIN_BACK_OK;
}

static void do_filescroll(SpaceFile *sfile)
{
	short mval[2], oldy, yo;
	
	calc_file_rcts(sfile);
	
	filescrollselect= 1;
	/* for beauty */

	glDrawBuffer(GL_FRONT);
	draw_filescroll(sfile);
	glDrawBuffer(GL_BACK);
	
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
	glDrawBuffer(GL_FRONT);
	draw_filescroll(sfile);
	glDrawBuffer(GL_BACK);
	
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

void activate_fileselect(int type, char *title, char *file, void (*func)(char *))
{
	SpaceFile *sfile;
	char group[24], name[FILE_MAXDIR], temp[FILE_MAXDIR];
	
	if(curarea==0) return;
	if(curarea->win==0) return;
	
	newspace(curarea, SPACE_FILE);
	scrarea_queue_winredraw(curarea);
	
	/* sometime double, when area already is SPACE_FILE with a different file name */
	addqueue(curarea->headwin, CHANGED, 1);
	

	name[2]= 0;
	strcpy(name, file);
	
	sfile= curarea->spacedata.first;
	/* sfile wants a (*)(short), but get (*)(char*) */
	sfile->returnfunc= func;
	sfile->type= type;
	sfile->ofs= 0;
	/* sfile->act is used for databrowse: double names of library objects */
	sfile->act= -1;
	
	if(BLI_convertstringcode(name, G.sce, G.scene->r.cfra)) sfile->flag |= FILE_STRINGCODE;
	else sfile->flag &= ~FILE_STRINGCODE;

	if(type==FILE_MAIN) {
		char *groupname;
		
		strcpy(sfile->file, name+2);

		groupname = BLO_idcode_to_name( GS(name) );
		if (groupname) {
			strcpy(sfile->dir, groupname);
			strcat(sfile->dir, "/");
		}

		/* free all */
		if(sfile->libfiledata) BLO_blendhandle_close(sfile->libfiledata);
		sfile->libfiledata= 0;
		
		freefilelist(sfile);
	}
	else if(type==FILE_LOADLIB) {
		strcpy(sfile->dir, name);
		if( is_a_library(sfile, temp, group) ) {
			/* to force a reload of the library-filelist */
			if(sfile->libfiledata==0) {
				freefilelist(sfile);
			}
		}
		else {
			split_sfile(sfile, name);
			if(sfile->libfiledata) BLO_blendhandle_close(sfile->libfiledata);
			sfile->libfiledata= 0;
		}
	}
	else {	/* FILE_BLENDER */
		split_sfile(sfile, name);	/* test filelist too */
		
		/* free: filelist and libfiledata became incorrect */
		if(sfile->libfiledata) BLO_blendhandle_close(sfile->libfiledata);
		sfile->libfiledata= 0;
	}
	BLI_strncpy(sfile->title, title, sizeof(sfile->title));
	filetoname= 1;
}

void activate_imageselect(int type, char *title, char *file, void (*func)(char *))
{
	SpaceImaSel *simasel;
	char dir[FILE_MAXDIR], name[FILE_MAXFILE];
	
	if(curarea==0) return;
	if(curarea->win==0) return;
	
	newspace(curarea, SPACE_IMASEL);
	
	/* sometimes double, when area is already SPACE_FILE with a different file name */
	addqueue(curarea->headwin, CHANGED, 1);
	addqueue(curarea->win, CHANGED, 1);

	name[2]= 0;
	strcpy(name, file);

	simasel= curarea->spacedata.first;
	simasel->returnfunc= func;

	if(BLI_convertstringcode(name, G.sce, G.scene->r.cfra)) simasel->mode |= IMS_STRINGCODE;
	else simasel->mode &= ~IMS_STRINGCODE;
	
	BLI_split_dirfile(name, dir, simasel->file);
	if(strcmp(dir, simasel->dir)!=0) simasel->fase= 0;
	strcpy(simasel->dir, dir);
	
	BLI_strncpy(simasel->title, title, sizeof(simasel->title));

	
	
	/* filetoname= 1; */
}


void activate_databrowse(ID *id, int idcode, int fromcode, int retval, short *menup, void (*func)(unsigned short))
{
	ListBase *lb;
	SpaceFile *sfile;
	char str[32];
	
	if(id==0) {
		lb= wich_libbase(G.main, idcode);
		id= lb->last;
	}
	
	if(id) strcpy(str, id->name);
	else return;
	
	activate_fileselect(FILE_MAIN, "SELECT DATABLOCK", str, (void (*) (char*))func);
	
	sfile= curarea->spacedata.first;
	sfile->retval= retval;
	sfile->ipotype= fromcode;
	sfile->menup= menup;
}

void filesel_prevspace()
{
	SpaceFile *sfile;
	
	sfile= curarea->spacedata.first;
	if(sfile->next) {
	
		BLI_remlink(&curarea->spacedata, sfile);
		BLI_addtail(&curarea->spacedata, sfile);

		sfile= curarea->spacedata.first;
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


static void filesel_execute(SpaceFile *sfile)
{
	struct direntry *files;
	char name[FILE_MAXDIR];
	int a;
	
	filesel_prevspace();

	if(sfile->type==FILE_LOADLIB) {
		do_library_append(sfile);
		
		allqueue(REDRAWALL, 1);
	}
	else if(sfile->returnfunc) {
		fsmenu_insert_entry(sfile->dir, 1);
	
		if(sfile->type==FILE_MAIN) {
			if (sfile->menup) {
				if(sfile->act>=0) {
					if(sfile->filelist) {
						files= sfile->filelist+sfile->act;
						*sfile->menup= files->nr;
					}	
					else *sfile->menup= sfile->act+1;
				}
				else {
					*sfile->menup= -1;
					for(a=0; a<sfile->totfile; a++) {
						if( strcmp(sfile->filelist[a].relname, sfile->file)==0) {
							*sfile->menup= a+1;
							break;
						}
					}
				}
			}
			sfile->returnfunc((char*) (long)sfile->retval);
		}
		else {
			if(strncmp(sfile->title, "SAVE", 4)==0) free_filesel_spec(sfile->dir);
			
			strcpy(name, sfile->dir);
			strcat(name, sfile->file);
			
			if(sfile->flag & FILE_STRINGCODE) BLI_makestringcode(G.sce, name);

			sfile->returnfunc(name);
		}
	}
}

static void do_filesel_buttons(short event, SpaceFile *sfile)
{
	char butname[FILE_MAXDIR];
	
	if (event == 1) {
		if (strchr(sfile->file, '*') || strchr(sfile->file, '?') || strchr(sfile->file, '[')) {
			int i, match = FALSE;
			
			for (i = 2; i < sfile->totfile; i++) {
				if (fnmatch(sfile->file, sfile->filelist[i].relname, 0) == 0) {
					sfile->filelist[i].flags |= ACTIVE;
					match = TRUE;
				}
			}
			if (match) strcpy(sfile->file, "");
			if(sfile->type==FILE_MAIN) filesel_select_objects(sfile);
			scrarea_queue_winredraw(curarea);
		}
	}
	else if(event== 2) {
		/* reuse the butname variable */
		checkdir(sfile->dir);

		BLI_make_file_string(G.sce, butname, sfile->dir, "");
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
	else if(event== 3) {
		char *selected= fsmenu_get_entry(sfile->menu-1);
		
		/* which string */
		if (selected) {
			strcpy(sfile->dir, selected);
			BLI_make_exist(sfile->dir);
			checkdir(sfile->dir);
			freefilelist(sfile);
			sfile->ofs= 0;
			scrarea_queue_winredraw(curarea);
		}

		sfile->act= -1;
		
	}
	else if(event== 4) parent(sfile);
	else if(event== 5) {
		if(sfile->type) filesel_execute(sfile);
	}
	else if(event== 6) filesel_prevspace();
	
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
		TFace *tfaces= me->tface;

		if (tfaces) {
			int i;

			for (i=0; i<me->totface; i++) {
				TFace *tf= &tfaces[i];

				if (tf->tpage == oldima) {
						/* not change_id_link, tpage's aren't owners :(
						 * see hack below.
						 */
					tf->tpage= newima;
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
	if(sfile->returnfunc) return;
	
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
	char str[FILE_MAXDIR+FILE_MAXFILE+12];
	
	sfile= curarea->spacedata.first;
	if(sfile==0) return;
	if(sfile->filelist==0) {
		/* but do buttons */
		if(val && event==LEFTMOUSE) {
			/* FrontbufferButs(TRUE); */
			/* event= DoButtons(); */
			/* FrontbufferButs(FALSE); */
					/*  NIET de headerbuttons! */
			/* if(event) do_filesel_buttons(event, sfile); */
		}
		return;
	}
	
	if(curarea->win==0) return;
	calc_file_rcts(sfile);
	getmouseco_areawin(mval);

	/* prevent looping */
	if(selecting && !(get_mbut() & R_MOUSE)) selecting= 0;

	if(val) {

		if( event!=RETKEY && event!=PADENTER)
			if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

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
						strcat(sfile->dir, sfile->filelist[act].relname);
						strcat(sfile->dir,"/");
						checkdir(sfile->dir);
						freefilelist(sfile);
						sfile->ofs= 0;
						do_draw= 1;
					}
					else {
						if( strcmp(sfile->file, sfile->filelist[act].relname)) {
							do_draw= 1;
							strcpy(sfile->file, sfile->filelist[act].relname);
						}
						if(event==MIDDLEMOUSE && sfile->type) filesel_execute(sfile);
					}
				}
			}
			else {
				/* FrontbufferButs(TRUE); */
				/* event= DoButtons(); */
				/* FrontbufferButs(FALSE); */
					/*  NOT the headerbuttons! */
				/* if(event) do_filesel_buttons(event, sfile);	 */
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

					if(event==BKEY) ret= BLI_backup(sfile->filelist[i].relname, sfile->dir, otherdir);
					else if(event==CKEY) ret= BLI_copy_fileops(str, otherdir);
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
			
				sprintf(str, "%s -a \"%s%s\"", bprogname, sfile->dir, sfile->file);
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
			strcpy(sfile->dir, "\\");
#else
			strcpy(sfile->dir, "/");
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
	char buf[32];
	char *lslash;
	
	strcpy(buf, group);
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
		*fd= '/';
	}
	else {
		strcpy(group, fd+1);
			
		/* Find the last slash */
		fd= (strrchr(dir, '/')>strrchr(dir, '\\'))?strrchr(dir, '/'):strrchr(dir, '\\');
		if (!fd || !BLO_has_bfile_extension(fd+1)) return 0;
	}
	return 1;
}

static void do_library_append(SpaceFile *sfile)
{
	char dir[FILE_MAXDIR], group[32];
	
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
		
		BLO_library_append(sfile, dir, idcode);

		/* DISPLISTS */
		ob= G.main->object.first;
		set_displist_onlyzero(1);
		while(ob) {
			if(ob->id.lib) {
				if(ob->type==OB_FONT) {
					Curve *cu= ob->data;
					if(cu->nurb.first==0) text_to_curve(ob, 0);
				}
				makeDispList(ob);
			}
			else if(ob->type==OB_MESH && ob->parent && ob->parent->type==OB_LATTICE ) {
				makeDispList(ob);
			}
			
			ob= ob->id.next;
		}
		set_displist_onlyzero(0);
	
		/* in sfile->dir is the whole lib name */
		strcpy(G.lib, sfile->dir);
		
		if((sfile->flag & FILE_LINK)==0) all_local();
	}
}

static void library_to_filelist(SpaceFile *sfile)
{
	char dir[FILE_MAXDIR], group[24];
	int ok, i, nnames, idcode;
	LinkNode *l, *names;
	
	/* name test */
	ok= is_a_library(sfile, dir, group);
	if (!ok) {
		/* free */
		if(sfile->libfiledata) BLO_blendhandle_close(sfile->libfiledata);
		sfile->libfiledata= 0;
		return;
	}
	
	/* there we go */
	/* for the time being only read filedata when libfiledata==0 */
	if (sfile->libfiledata==0) {
		sfile->libfiledata= BLO_blendhandle_from_file(dir);
		if(sfile->libfiledata==0) return;
	}
	
	idcode= groupname_to_code(group);
	if (idcode) {
		names= BLO_blendhandle_get_datablock_names(sfile->libfiledata, idcode);
	} else {
		names= BLO_blendhandle_get_linkable_groups(sfile->libfiledata);
	}
	
	nnames= BLI_linklist_length(names);

	sfile->totfile= nnames + 2;
	sfile->filelist= malloc(sfile->totfile * sizeof(*sfile->filelist));
	memset(sfile->filelist, 0, sfile->totfile * sizeof(*sfile->filelist));

	sfile->filelist[0].relname= strdup(".");
	sfile->filelist[0].type |= S_IFDIR;
	sfile->filelist[1].relname= strdup("..");
	sfile->filelist[1].type |= S_IFDIR;
		
	for (i=0, l= names; i<nnames; i++, l= l->next) {
		char *blockname= l->link;

		sfile->filelist[i + 2].relname= blockname;
		if (!idcode)
			sfile->filelist[i + 2].type |= S_IFDIR;
	}
		
	BLI_linklist_free(names, NULL);
	
	qsort(sfile->filelist, sfile->totfile, sizeof(struct direntry), compare_name);
	
	sfile->maxnamelen= 0;
	for(i=0; i<sfile->totfile; i++) {
		int len = BMF_GetStringWidth(G.font, sfile->filelist[i].relname);
		if (len > sfile->maxnamelen)
			sfile->maxnamelen = len;
	}
}

/* ******************* DATA SELECT ********************* */

static void filesel_select_objects(SpaceFile *sfile)
{
	Object *ob;
	Base *base;
	Scene *sce;
	int a;
	
	/* only when F4 DATABROWSE */
	if(sfile->returnfunc) return;
	
	if( strcmp(sfile->dir, "Object/")==0 ) {
		for(a=0; a<sfile->totfile; a++) {
			
			ob= (Object *)sfile->filelist[a].poin;
			
			if(ob) {
				if(sfile->filelist[a].flags & ACTIVE) ob->flag |= SELECT;
				else ob->flag &= ~SELECT;
			}

		}
		base= FIRSTBASE;
		while(base) {
			base->flag= base->object->flag;
			base= base->next;
		}
		allqueue(REDRAWVIEW3D, 0);
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
	if(sfile->returnfunc) return;
	
	if( strcmp(sfile->dir, "Object/")==0 ) {
		if(sfile->act >= 0) {
			
			ob= (Object *)sfile->filelist[sfile->act].poin;
			
			if(ob) {
				set_active_object(ob);
				if(BASACT && BASACT->object==ob) {
					BASACT->flag |= SELECT;
					sfile->filelist[sfile->act].flags |= ACTIVE;
					allqueue(REDRAWVIEW3D, 0);
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

	if(sfile->dir[0]=='/') sfile->dir[0]= 0;
	
	if(sfile->dir[0]) {
		idcode= groupname_to_code(sfile->dir);
		if(idcode==0) sfile->dir[0]= 0;
	}
	
	if( sfile->dir[0]==0) {
		
		/* make directories */
		sfile->totfile= 22;
		sfile->filelist= (struct direntry *)malloc(sfile->totfile * sizeof(struct direntry));
		
		for(a=0; a<sfile->totfile; a++) {
			memset( &(sfile->filelist[a]), 0 , sizeof(struct direntry));
			sfile->filelist[a].type |= S_IFDIR;
		}
		
		sfile->filelist[0].relname= strdup("..");
		sfile->filelist[1].relname= strdup(".");
		sfile->filelist[2].relname= strdup("Scene");
		sfile->filelist[3].relname= strdup("Object");
		sfile->filelist[4].relname= strdup("Mesh");
		sfile->filelist[5].relname= strdup("Curve");
		sfile->filelist[6].relname= strdup("Metaball");
		sfile->filelist[7].relname= strdup("Material");
		sfile->filelist[8].relname= strdup("Texture");
		sfile->filelist[9].relname= strdup("Image");
		sfile->filelist[10].relname= strdup("Ika");
		sfile->filelist[11].relname= strdup("Wave");
		sfile->filelist[12].relname= strdup("Lattice");
		sfile->filelist[13].relname= strdup("Lamp");
		sfile->filelist[14].relname= strdup("Camera");
		sfile->filelist[15].relname= strdup("Ipo");
		sfile->filelist[16].relname= strdup("World");
		sfile->filelist[17].relname= strdup("Screen");
		sfile->filelist[18].relname= strdup("VFont");
		sfile->filelist[19].relname= strdup("Text");
		sfile->filelist[20].relname= strdup("Armature");
		sfile->filelist[21].relname= strdup("Action");
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
			
			if(sfile->returnfunc && idcode==ID_IP) {
				if(sfile->ipotype== ((Ipo *)id)->blocktype) sfile->totfile++;
			}
			else sfile->totfile++;
			
			id= id->next;
		}
		
		if(sfile->returnfunc==0) sfile->totfile+= 2;
		sfile->filelist= (struct direntry *)malloc(sfile->totfile * sizeof(struct direntry));
		
		files= sfile->filelist;
		
		if(sfile->returnfunc==0) {
			memset( &(sfile->filelist[0]), 0 , sizeof(struct direntry));
			sfile->filelist[0].relname= strdup(".");
			sfile->filelist[0].type |= S_IFDIR;
			memset( &(sfile->filelist[1]), 0 , sizeof(struct direntry));
			sfile->filelist[1].relname= strdup("..");
			sfile->filelist[1].type |= S_IFDIR;
		
			files+= 2;
		}
		
		id= lb->first;
		totlib= totbl= 0;
		
		while(id) {
			
			ok= 0;
			if(sfile->returnfunc && idcode==ID_IP) {
				if(sfile->ipotype== ((Ipo *)id)->blocktype) ok= 1;
			}
			else ok= 1;
			
			if(ok) {
		
				memset( files, 0 , sizeof(struct direntry));
				files->relname= strdup(id->name+2);
				
				if(sfile->returnfunc==0) { /* F4 DATA BROWSE */
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
				if(sfile->returnfunc) sfile->filelist[a].flags |= ACTIVE;
			}
		}
	}
}


void clever_numbuts_filesel()
{
	SpaceFile *sfile;
	char orgname[FILE_MAXDIR+FILE_MAXFILE+12];
	char filename[FILE_MAXDIR+FILE_MAXFILE+12];
	char newname[FILE_MAXDIR+FILE_MAXFILE+12];
	int test;
	int len;
	
	sfile= curarea->spacedata.first;

	if(sfile->type==FILE_MAIN) return;
	
	len = 110;
	test = get_hilited_entry(sfile);

	if (test != -1 && !(S_ISDIR(sfile->filelist[test].type))){
		BLI_make_file_string(G.sce, orgname, sfile->dir, sfile->filelist[test].relname);
		strcpy(filename, sfile->filelist[test].relname);
		
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

