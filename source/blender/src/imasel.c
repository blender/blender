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

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include "BLI_winstuff.h"
#include <io.h>
#include <direct.h>
#endif   
#include <fcntl.h>
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BIF_imasel.h"
#include "BIF_space.h"
#include "BIF_screen.h"

#include "blendef.h"
#include "mydevice.h"

#ifndef WIN32
#include <dirent.h>
#endif

#include <sys/stat.h>
#include "datatoc.h"

/* locals */
void longtochar(char *des, unsigned int *src, int size);
void chartolong(unsigned int *des, char *src, int size);
int dir_compare(const void *a1, const void *a2);
void issort( int te, ImaDir **firstentry);
int ima_compare(const void *a1, const void *a2);
void imsort(OneSelectableIma **firstentry);
void append_pib(SpaceImaSel *simasel, OneSelectableIma *ima);  
void add_ima(int who, SpaceImaSel *simasel, ImaDir *direntry);

/* implementation */
int  bitset(int l,  int bit)
{	return (( l & bit) == bit);  }

void longtochar(char *des, unsigned int *src, int size)
{	int i;for (i = 0; i<size; i++){ des[i] = src[i] & 0xFF; }}

void chartolong(unsigned int *des, char *src, int size)
{	int i;for (i = 0; i<size; i++){ des[i] = src[i]; }}

int dir_compare(const void *a1, const void *a2)
{
	ImaDir **in1, **in2;
	ImaDir *use1, *use2;

	in1= (ImaDir **)a1;
	in2= (ImaDir **)a2;

	use1 = *in1;
	use2 = *in2;
	
	return strcasecmp(use1->name,  use2->name);
}

void issort( int te, ImaDir **firstentry)
{
	ImaDir **sort;
	ImaDir *use;
	int i = 0;
	
	sort = MEM_mallocN(te * sizeof(void *),  "dir Sorteer temp");
	use = *firstentry;
	
	while (use){
		sort[i++] = use;
		use = use->next;
	}
	
	qsort (sort, te, sizeof(void *), dir_compare);
	
	*firstentry = sort[0];
	use = *firstentry;
	
	
	for (i=0; i<te; i++){
		if (i != 0)    use->prev = sort[i-1]; else use->prev = 0;
		if (i != te-1) use->next = sort[i+1]; else use->next = 0;
	
		use = use->next;
	}
	
	MEM_freeN(sort);
}


int ima_compare(const void *a1, const void *a2)
{
	OneSelectableIma **in1, **in2;
	OneSelectableIma *use1, *use2;

	in1= (OneSelectableIma **)a1;
	in2= (OneSelectableIma **)a2;

	use1 = *in1; use2 = *in2;
	return strcasecmp(use1->file_name,  use2->file_name);
}

void imsort(OneSelectableIma **firstentry)
{
	OneSelectableIma **sort;
	OneSelectableIma *use;
	int tot = 0, i = 0;
	
	use = *firstentry;
	while (use){
		tot++;
		use = use->next;
	}
	
	if (tot){
		sort = MEM_mallocN(tot * sizeof(void *),  "Sorteer imsort temp");
		use = *firstentry;
		while (use){
			sort[i++] = use;
			use = use->next;
		}
		
		qsort (sort, tot, sizeof(void *), ima_compare);
		
		*firstentry = sort[0];
		use = *firstentry;
		for (i=0; i<tot; i++){
			if (i != 0)     use->prev = sort[i-1]; else use->prev = 0;
			if (i != tot-1) use->next = sort[i+1]; else use->next = 0;
		
			use = use->next;
		}
		MEM_freeN(sort);
	}
}

static int write_msb_int(int fd, int i) {
	unsigned int ui= (unsigned int) i;
	unsigned char buf[4];
	buf[0]= (ui>>24)&0xFF;
	buf[1]= (ui>>16)&0xFF;
	buf[2]= (ui>>8)&0xFF;
	buf[3]= (ui>>0)&0xFF;
	return write(fd, buf, 4);
}
static int write_msb_short(int fd, short s) {
	unsigned short us= (unsigned short) s;
	unsigned char buf[2];
	buf[0]= (us>>8)&0xFF;
	buf[1]= (us>>0)&0xFF;
	return write(fd, buf, 2);
}

static int read_msb_int(int fd, int *i_r) {
	unsigned char buf[4];
	int rcount= read(fd, buf, 4);
	
	if (i_r)
		*i_r= (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|(buf[3]<<0);

	return rcount;
}
static int read_msb_short(int fd, short *s_r) {
	unsigned char buf[2];
	int rcount= read(fd, buf, 2);
	
	if (s_r)
		*s_r= (buf[0]<<8)|(buf[1]<<0);

	return rcount;
}

void append_pib(SpaceImaSel *simasel, OneSelectableIma *ima)
{
	int  file;
	char name[FILE_MAXDIR+FILE_MAXFILE];
	
	if ( bitset (simasel->fase, IMS_WRITE_NO_BIP)) return;
	
	strcpy(name, simasel->dir);
	strcat(name, ".Bpib");
	
	file = open(name, O_BINARY|O_APPEND | O_RDWR | O_CREAT, 0666);
	if (file == -1) {
		/* printf("Could not write .Bpib file in dir %s\n", simasel->dir); */
		simasel->fase |= IMS_WRITE_NO_BIP;
		return;
	}
	
	lseek(file, 0, SEEK_END);
	
	write(file, "BIP2", 4);
	write_msb_int(file,		ima->ibuf_type);
	write_msb_int(file,		0);
	write_msb_int(file,		0);
	write_msb_int(file,		0);
	write_msb_short(file,	ima->cmap);
	write_msb_short(file,	ima->image);
	write_msb_short(file,	ima->draw_me);
	write_msb_short(file,	ima->rt);
	write_msb_short(file,	ima->sx);
	write_msb_short(file,	ima->sy);
	write_msb_short(file,	ima->ex);
	write_msb_short(file,	ima->ey);
	write_msb_short(file,	ima->dw);
	write_msb_short(file,	ima->dh);
	write_msb_short(file,	ima->selectable);
	write_msb_short(file,	ima->selected);
	write_msb_int(file,		ima->mtime);
	write_msb_int(file,		ima->disksize);
	write(file, ima->file_name, 64);
	write_msb_short(file,	ima->orgx);
	write_msb_short(file,	ima->orgy);
	write_msb_short(file,	ima->orgd);
	write_msb_short(file,	ima->anim);
	write_msb_int(file,		0); /* pad to 128 boundary */
	write(file, ima->pict_rect, 3968);	

	close(file);
}

void write_new_pib(SpaceImaSel *simasel)
{
	OneSelectableIma *ima;
	char name[FILE_MAXDIR+FILE_MAXFILE];
	
	strcpy(name, simasel->dir);
	strcat(name, ".Bpib");
	remove(name);
	
	ima = simasel->first_sel_ima;
	while (ima) {
		append_pib(simasel, ima);
		ima = ima->next;
	}
}

void free_ima_dir(ImaDir *firstdir)
{
	ImaDir *n;
	
	while(firstdir){
		n = firstdir->next;
		MEM_freeN(firstdir);
		firstdir = n;
	}
}

void free_sel_ima(OneSelectableIma *firstima)
{
	OneSelectableIma *n;
	
	while(firstima){
		
		if (firstima->pict) {
			IMB_freeImBuf(firstima->pict);
		}
		n = firstima->next;
		MEM_freeN(firstima);
		firstima = n;
	}
}

void check_for_pib(SpaceImaSel *simasel)
{
	ImaDir  *direntry;
	
	direntry = simasel->firstfile;
	while(direntry){
		if ((strlen(direntry->name) > 4) && (0==strcmp(direntry->name, ".Bpib")) ){
			simasel->fase |= IMS_FOUND_BIP;
			direntry = 0;
		}else{
			direntry = direntry->next;
		}
	}
}

void clear_ima_dir(SpaceImaSel *simasel)
{
	if(simasel->first_sel_ima)	free_sel_ima(simasel->first_sel_ima);
	if(simasel->firstdir)		free_ima_dir(simasel->firstdir);
	if(simasel->firstfile) 		free_ima_dir(simasel->firstfile);

	simasel->first_sel_ima	=  0;
	simasel->firstdir		=  0;
	simasel->firstfile		=  0;
	
	simasel->totaldirs		=  0;
	simasel->totalfiles		=  0;
	simasel->totalima		=  0;
	simasel->topdir			= -1;
	simasel->topfile		= -1;
	simasel->topima			=  0;
	simasel->image_slider	=  0.0;
	simasel->slider_height	=  0.0;
	simasel->slider_space	=  0.0;
	simasel->hilite			= -1;
	simasel->curimax		=  0;
	simasel->curimay		=  0;
	
	simasel->total_selected =  0;
	simasel->fase			=  0;
	simasel->subfase		=  0;
	simasel->imafase		=  0;
	simasel->ima_redraw     =  0;
}

int get_ima_dir(char *dirname, int dtype, int *td, ImaDir **first)
{
	DIR *dirp;
	struct dirent *dep;
	struct ImaDir *temp;
	struct ImaDir *dnext = NULL, *fnext;
	struct stat status;
	char olddir[FILE_MAXDIR+FILE_MAXFILE];
	char getdirname[FILE_MAXDIR+FILE_MAXFILE];
	int  /*  i=0, */ tot=0; 
	int isdir;
	
	if(!BLI_getwdN(olddir)) return -1;
	
	if (chdir(dirname) == -1) return(-1);
	
	strcpy(getdirname, ".");
	
	dirp = (DIR *) opendir(getdirname);
	if (dirp == NULL) return (-1);

	waitcursor(1);

	while((dep = (struct dirent*) readdir(dirp)) != NULL){
		
		strcpy(getdirname, dirname);
		strcat(getdirname,dep->d_name);
		
		stat(getdirname, &status);
		isdir = S_ISDIR(status.st_mode);
		
		if ( ((dtype == IMS_DIR)  && isdir) || ((dtype == IMS_FILE)  && !isdir)){			
			/* yes, searching for this type */
			tot++;
			if (tot == 1){
				dnext   = MEM_callocN(sizeof(struct ImaDir), "get first");
				*first  = dnext;
				
				dnext->prev     = 0;
				dnext->next     = 0;
			}else{
				fnext		    = MEM_callocN(sizeof(struct ImaDir), "get nextdir");
				dnext->next     = fnext;
				
				temp  = dnext;
				dnext = fnext;
				
				dnext ->prev     = temp;
				dnext ->next     = 0;
			}
			
			dnext->type     = dtype;
			dnext->selected = 0;
			dnext->hilite   = 0;
			
			dnext->mtime    = status.st_ctime;
			dnext->size     = (int)status.st_size;
			strcpy(dnext->name, dep->d_name);
		}
	}
	closedir(dirp);
	
	if (tot) issort(tot, first);
	
	waitcursor(0);
	
	*td = tot;
	
	chdir (olddir);
	
	return (tot);
}

void imadir_parent(SpaceImaSel *simasel)
{

#ifdef WIN32
	if (strlen(simasel->dir) > 1){
		simasel->dir[strlen(simasel->dir)-1] = 0;
		while(simasel->dir[strlen(simasel->dir)-1] != '\\'){
			if(strlen(simasel->dir)==0) break;
			simasel->dir[strlen(simasel->dir)-1] = 0;	
		}
	}
#else
	if (strlen(simasel->dir) > 1){
		simasel->dir[strlen(simasel->dir)-1] = 0;
		while(simasel->dir[strlen(simasel->dir)-1] != '/') {
			if(strlen(simasel->dir)==0) break;
			simasel->dir[strlen(simasel->dir)-1] = 0;	
		}
	}
#endif
}


void get_next_image(SpaceImaSel *simasel)
{
	OneSelectableIma * ima;
	ImBuf            * ibuf;
	struct anim      * anim;
	int     i = 0, size;
	char    name[FILE_MAXDIR+FILE_MAXFILE];
	
	ima = simasel->first_sel_ima;
	if (ima == 0){
		simasel->imafase = 0;
		simasel->fase |=  IMS_KNOW_IMA;
		simasel->fase &= ~IMS_DOTHE_IMA;
		return;	
	}
	if (simasel->imafase > simasel->totalima){
		simasel->imafase = 0;
		simasel->fase &= ~IMS_DOTHE_IMA;
		simasel->fase |=  IMS_KNOW_IMA;
	}
	
	ima = simasel->first_sel_ima;
	i = 0;
	while(i < simasel->imafase){
		if ((ima) && (ima->next)) ima = ima->next;
		i++;
	}
	
	if (ima->image == 0) {
		if (ima->anim == 1) {
			/* open movie, get len, get middle picture */
			
			strcpy(name, simasel->dir);
			strcat(name, ima->file_name);

			anim = IMB_open_anim(name, IB_rect);
			
			if (anim == 0) {
				// ibuf= IMB_loadiffmem((int*)datatoc_cmovie_tga, IB_rect);
				ibuf= IMB_ibImageFromMemory((int *)datatoc_cmovie_tga, datatoc_cmovie_tga_size, IB_rect);
			}
			else{
				int animlen;
				
				ibuf = IMB_anim_nextpic(anim);
				IMB_freeImBuf(ibuf);
				
				animlen= IMB_anim_get_duration(anim);
				ibuf = IMB_anim_absolute(anim, animlen / 2);

				if(ibuf) {
					//get icon dimensions for movie
					ima->orgx = ibuf->x;
					ima->orgy = ibuf->y;
//					ima->orgd = ibuf->depth;

					if (ima->orgx > ima->orgy){
						ima->dw = 64;
						ima->dh = (short)(62 * ((float)ima->orgy / (float)ima->orgx));
					}else{
						ima->dw = (short)(64 * ((float)ima->orgx / (float)ima->orgy));
						ima->dh = 62;
					}
				}
				
				IMB_free_anim(anim);
			}
		}
		else {
			
			strcpy(name, simasel->dir);
			strcat(name, ima->file_name);

			ibuf = IMB_loadiffname(name, IB_rect);
			if(ibuf && ibuf->zbuf) IMB_freezbufImBuf(ibuf);
		}
		
		if (ibuf){
			if (ima->dw < 4) ima->dw = 4;
			if (ima->dh < 4) ima->dh = 4;
			
			IMB_scaleImBuf(ibuf, ima->dw, ima->dh);
			/* the whole cmap system is wacko */
			
			if (G.order==B_ENDIAN)
				IMB_convert_rgba_to_abgr(ima->dw*ima->dh, ibuf->rect);
			
			ibuf->mincol =   0;
			ibuf->maxcol = 256;
			ibuf->cbits  =   5;
			ibuf->depth  =   8;
			
			IMB_freecmapImBuf(ibuf);
			ibuf->cmap = simasel->cmap->cmap;
			
			IMB_converttocmap(ibuf);
			
			/* copy ibuf->rect to ima->pict_rect */ 
			size = ima->dw * ima->dh; if (size > 3968) size = 3968;
			longtochar(ima->pict_rect, ibuf->rect, size); 

			IMB_applycmap(ibuf);
			IMB_convert_rgba_to_abgr(size, ibuf->rect);
			
			if (ima->pict) IMB_freeImBuf(ima->pict);
			ima->pict = ibuf;
			ibuf = 0;
			ima->cmap  = 1;
			ima->image = 1;
			
			append_pib(simasel, ima);
		}
	}
	simasel->ima_redraw++;
	simasel->imafase ++;
	if (simasel->imafase == simasel->totalima){
		simasel->imafase = 0;
		simasel->fase &= ~IMS_DOTHE_IMA;
		simasel->fase |= IMS_KNOW_IMA;
	}
}

void add_ima(int who, SpaceImaSel *simasel, ImaDir *direntry)
{
	OneSelectableIma *ima, *prev_ima;
	ImBuf   *ibuf;
	char    name[FILE_MAXDIR+FILE_MAXFILE];
	
	strcpy(name ,  simasel->dir);
	strcat(name ,  direntry->name);
	
	prev_ima = simasel->first_sel_ima;
	while((prev_ima)&&(prev_ima->next)){
		prev_ima = prev_ima->next;
	}
	
	ima = MEM_callocN(sizeof(OneSelectableIma), "OSIbip");
	if (direntry->type == IMS_IMA){
		/* Picture is an Image */
		ibuf = IMB_loadiffname(name, IB_test);
		if (ibuf){
			ima->anim	  = 0;
			ima->pict     = ibuf;
			ima->ibuf_type= ibuf->ftype;
			ima->orgx     = ibuf->x;
			ima->orgy     = ibuf->y;
			ima->orgd     = ibuf->depth;
			
			ima->dw    = 64;
			ima->dh    = 51;
			ima->cmap  =  0;
			ima->image =  0;  
			if (ima->orgx > ima->orgy){
				ima->dw = 64;
				ima->dh = (short)(62 * ((float)ima->orgy / (float)ima->orgx));
			}else{
				ima->dw = (short)(64 * ((float)ima->orgx / (float)ima->orgy));
				ima->dh = 62;
			}
		}else{
			printf("%s image with no imbuf ???\n", name);
		}
		ibuf = 0;	
	}else{
		/* Picture is an Animation */
	
		ima->pict     =  0;
		ima->anim	  =  1;
		ima->ibuf_type=  0;
		ima->orgx	  = 64;
		ima->orgy     = 51;
		ima->orgd     = 24;
		
		ima->dw    = 64;
		ima->dh    = 51;
		ima->cmap  =  0;
		ima->image =  0;  
	}
		
	strcpy(name, direntry->name); name[63] = 0;
	strcpy(ima->file_name, name);
	ima->disksize = (int)direntry->size;
	ima->mtime    = (int)direntry->mtime;
	
	ima->next = 0;
	ima->prev = prev_ima;

	if (prev_ima)	{	
		prev_ima->next = ima;
	}else{   
		simasel->first_sel_ima =  ima;	
	}
	
	simasel->ima_redraw++;
	simasel->totalima++;
}


void get_file_info(SpaceImaSel *simasel)
{
	OneSelectableIma *prev_ima;
	ImaDir  *direntry;
	char    name[FILE_MAXDIR+FILE_MAXFILE];
	int     i = 0;
	
	if (!simasel->firstfile){
		simasel->subfase = 0;
		simasel->fase |= IMS_KNOW_INF;
		simasel->fase &= ~IMS_DOTHE_INF;
		return;	
	}
	if (simasel->subfase > simasel->totalfiles){
		simasel->subfase = 0;
		simasel->fase |= IMS_KNOW_INF;
		simasel->fase &= ~IMS_DOTHE_INF;
	}

	direntry = simasel->firstfile;
	while(i < simasel->subfase){
		direntry = direntry->next;
		i++;
	}
	
	prev_ima = simasel->first_sel_ima;
	while((prev_ima)&&(prev_ima->next)){
		prev_ima = prev_ima->next;
	}
	
	strcpy(name ,  simasel->dir);
	strcat(name ,  direntry->name);
	
	if(direntry->name[0] == '.') {
		direntry->type = IMS_NOIMA;
	} else {
		if (IMB_ispic(name)) {
			direntry->type = IMS_IMA;
		}else{
			if (IMB_isanim(name)) {
				direntry->type = IMS_ANIM;
			}else{
				direntry->type = IMS_NOIMA;
			}
		}
	}
	
	if (direntry->type != IMS_NOIMA){	
		add_ima(1, simasel, direntry);
	}
	
	simasel->subfase++;
	
	if (simasel->subfase == simasel->totalfiles){
		simasel->subfase = 0;
		simasel->fase |= IMS_KNOW_INF;
		simasel->fase &= ~IMS_DOTHE_INF;	
	}
}

/* Note: the thumbnails are saved in ABGR format in the .Bpib
cache file */

void get_pib_file(SpaceImaSel *simasel)
{
	ImaDir           *direntry, *prev_dir, *next_dir;
	OneSelectableIma *ima, *prev_ima;
	int flen;
	int  dl, file, first, trd=0, rd, size, found, ima_added = 0;
	char name[FILE_MAXDIR+FILE_MAXFILE];
	
	if (bitset(simasel->fase , IMS_KNOW_BIP)) return;
	
	waitcursor(1);

	strcpy(name,  simasel->dir);
	strcat(name,  ".Bpib");
		
	file = open(name, O_BINARY|O_RDONLY);
	
	flen = BLI_filesize(file);

	simasel->totalima = 0;
	prev_ima = 0;
	first = 1;
	trd = 0;
	
	while(trd < flen){
		char header[5];
		
		ima = MEM_callocN(sizeof(OneSelectableIma), "Ima");
		
		rd= 0;
		rd+= read(file, header, 4);
		rd+= read_msb_int(file,		&ima->ibuf_type);
		rd+= read_msb_int(file,		NULL);
		rd+= read_msb_int(file,		NULL);
		rd+= read_msb_int(file,		NULL);
		rd+= read_msb_short(file,	&ima->cmap);
		rd+= read_msb_short(file,	&ima->image);
		rd+= read_msb_short(file,	&ima->draw_me);
		rd+= read_msb_short(file,	&ima->rt);
		rd+= read_msb_short(file,	&ima->sx);
		rd+= read_msb_short(file,	&ima->sy);
		rd+= read_msb_short(file,	&ima->ex);
		rd+= read_msb_short(file,	&ima->ey);
		rd+= read_msb_short(file,	&ima->dw);
		rd+= read_msb_short(file,	&ima->dh);
		rd+= read_msb_short(file,	&ima->selectable);
		rd+= read_msb_short(file,	&ima->selected);
		rd+= read_msb_int(file,		&ima->mtime);
		rd+= read_msb_int(file,		&ima->disksize);
		rd+= read(file, ima->file_name, 64);
		rd+= read_msb_short(file,	&ima->orgx);
		rd+= read_msb_short(file,	&ima->orgy);
		rd+= read_msb_short(file,	&ima->orgd);
		rd+= read_msb_short(file,	&ima->anim);
		rd+= read_msb_int(file,		NULL);
		rd+= read(file, ima->pict_rect, 3968);	

		found = 0;

		if (rd != sizeof(OneSelectableIma) || memcmp(header, "BIP2", 4)!=0) {
			printf("Error in Bpib file\n");
			strcpy(name, simasel->dir);
			strcat(name, ".Bpib");
			dl = remove(name);
			if (dl == 0) printf("corrupt Bpib file removed\n");
			trd = flen;
		} else {
				/* find matching direntry (if possible) */
			for (direntry= simasel->firstfile; direntry; direntry= direntry->next)
				if (BLI_streq(direntry->name, ima->file_name))
					break;
			
			if (direntry) {
				if (direntry->mtime == ima->mtime) {
						/*  ima found and same, load pic */
					size = ima->dw * ima->dh;
					if (size > 3968) size = 3968;
					if (size) {
						ima->pict = IMB_allocImBuf(ima->dw, ima->dh, 24, IB_rect | IB_cmap, 0);
						chartolong(ima->pict->rect, ima->pict_rect, size);
						ima->pict->cmap = simasel->cmap->cmap;
						ima->pict->maxcol = 256;
						IMB_applycmap(ima->pict);
						IMB_convert_rgba_to_abgr(size, ima->pict->rect);
					}
					ima->selected   = 0;
					ima->selectable = 0;
					
					if(prev_ima) prev_ima->next = ima;
					ima->next      = 0;
					ima->prev      = prev_ima;
					
					prev_ima = ima;
		
					if (first){ first = 0;simasel->first_sel_ima = ima; }
					simasel->totalima++;
					found = 1;
				}
					
					/* remove direntry */
				prev_dir = direntry->prev; 
				next_dir = direntry->next;
							
				if(prev_dir) prev_dir->next = next_dir; 
				if(next_dir) next_dir->prev = prev_dir;
							
				MEM_freeN(direntry);
			}
		}
		if (!found) MEM_freeN(ima);
		
		trd+=rd;
	}
	close(file);
	
	direntry = simasel->firstfile;
	
	while(direntry){
		
		strcpy(name ,  simasel->dir);
		strcat(name ,  direntry->name);
		
		if (IMB_ispic(name)) {
			direntry->type = IMS_IMA;
		}else{
			if (IMB_isanim(name)) {
				direntry->type = IMS_ANIM;
			}else{
				direntry->type = IMS_NOIMA;
			}
		}
		
		if (direntry->type != IMS_NOIMA){
			prev_ima = simasel->first_sel_ima;
			while((prev_ima)&&(prev_ima->next)){
				prev_ima = prev_ima->next;
			}
			add_ima(2, simasel, direntry);
			ima_added = 1;
		}
		direntry = direntry->next;
	}
	
	imsort(&simasel->first_sel_ima);
	
	simasel->fase |= IMS_KNOW_BIP;
	simasel->fase |= IMS_KNOW_INF;
	simasel->fase |= IMS_KNOW_IMA;
	
	if (ima_added){
		simasel->fase |= IMS_DOTHE_IMA;
		simasel->fase &= ~IMS_KNOW_IMA;
		addafterqueue(curarea->win, AFTERIMASELGET, 1);
	}else{
		write_new_pib(simasel);
	}		
	
	waitcursor(0);
}

void change_imadir(SpaceImaSel *simasel)
{
	ImaDir  *direntry;
	int i;
	
	direntry = simasel->firstdir; 
	for (i=0; i<simasel->hilite; i++){
		direntry = direntry->next;	
	}
	
	if(direntry==NULL);
	else if (direntry->name[0] != '.'){
		strcat(simasel->dir, direntry->name);
		strcat(simasel->dir, "/");
	}
	else {
		if (direntry->name[1] == '.'){
			imadir_parent(simasel);	
		}
	}
	
	clear_ima_dir(simasel);
}

void check_imasel_copy(SpaceImaSel *simasel)
{

	/* WATCH IT: also used when reading blender file */
	/* initialize stuff, malloc, etc */
	simasel->first_sel_ima	=  0;
	simasel->hilite_ima	    =  0;
	simasel->firstdir		=  0;
	simasel->firstfile		=  0;
	simasel->cmap           =  0;
	clear_ima_dir(simasel);
	
	// simasel->cmap= IMB_loadiffmem((int*)datatoc_cmap_tga, IB_rect|IB_cmap);
	simasel->cmap= IMB_ibImageFromMemory((int *)datatoc_cmap_tga, datatoc_cmap_tga_size, IB_rect|IB_cmap);
}

void free_imasel(SpaceImaSel *simasel)
{
	/* do not free imasel itself */
	
	clear_ima_dir(simasel);
	IMB_freeImBuf(simasel->cmap);
}

