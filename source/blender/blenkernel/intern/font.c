/*  font.c     
 *  
 * 
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <wchar.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_vfontdata.h"

#include "DNA_packedFile_types.h"
#include "DNA_curve_types.h"
#include "DNA_vfont_types.h"
#include "DNA_scene_types.h"

#include "BKE_utildefines.h"

#include "BKE_packedFile.h"

#include "BKE_library.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_anim.h"
#include "BKE_curve.h"
#include "BKE_displist.h"

static ListBase ttfdata= {NULL, NULL};

/* UTF-8 <-> wchar transformations */
void
chtoutf8(unsigned long c, char *o)
{
	// Variables and initialization
/*	memset(o, 0, 16);	*/

	// Create the utf-8 string
	if (c < 0x80)
	{
		o[0] = (char) c;
	}
	else if (c < 0x800)
	{
		o[0] = (0xC0 | (c>>6));
		o[1] = (0x80 | (c & 0x3f));
	}
	else if (c < 0x10000)
	{
		o[0] = (0xe0 | (c >> 12));
		o[1] = (0x80 | (c >>6 & 0x3f));
		o[2] = (0x80 | (c & 0x3f));
	}
	else if (c < 0x200000)
	{
	o[0] = (0xf0 | (c>>18));
	o[1] = (0x80 | (c >>12 & 0x3f));
	o[2] = (0x80 | (c >> 6 & 0x3f));
	o[3] = (0x80 | (c & 0x3f));
	}
}

void
wcs2utf8s(char *dst, wchar_t *src)
{
	char ch[5];

	while(*src)
	{
		memset(ch, 0, 5);
		chtoutf8(*src++, ch);
		strcat(dst, ch);
	}
}

int
wcsleninu8(wchar_t *src)
{
	char ch[16];
	int len = 0;

	while(*src)
	{
		memset(ch, 0, 16);
		chtoutf8(*src++, ch);
		len = len + strlen(ch);
	}

	return len;
}

int
static utf8slen(char *src)
{
	int size = 0, index = 0;
	unsigned char c;
	
	c = src[index++];
	while(c)
	{    
		if((c & 0x80) == 0)
		{
			index += 0;
		}
		else if((c & 0xe0) == 0xe0)
		{
			index += 2;
		}
		else
		{
			index += 1;
		}
		size += 1;
		c = src[index++];		
	}
	
	return size;
}


/* Converts Unicode to wchar

According to RFC 3629 "UTF-8, a transformation format of ISO 10646"
(http://tools.ietf.org/html/rfc3629), the valid UTF-8 encoding are:

  Char. number range  |        UTF-8 octet sequence
	  (hexadecimal)    |              (binary)
   --------------------+---------------------------------------------
   0000 0000-0000 007F | 0xxxxxxx
   0000 0080-0000 07FF | 110xxxxx 10xxxxxx
   0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
   0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

If the encoding incidated by the first character is incorrect (because the
1 to 3 following characters do not match 10xxxxxx), the output is a '?' and
only a single input character is consumed.

*/

int utf8towchar(wchar_t *w, char *c)
{
	int len=0;

	if(w==NULL || c==NULL) return(0);

	while(*c) {
		if ((*c & 0xe0) == 0xc0) {
			if((c[1] & 0x80) && (c[1] & 0x40) == 0x00) {
				*w=((c[0] &0x1f)<<6) | (c[1]&0x3f);
				c++;
			} else {
				*w = '?';
			}
		} else if ((*c & 0xf0) == 0xe0) {
			if((c[1] & c[2] & 0x80) && ((c[1] | c[2]) & 0x40) == 0x00) {
				*w=((c[0] & 0x0f)<<12) | ((c[1]&0x3f)<<6) | (c[2]&0x3f);
				c += 2;
			} else {
				*w = '?';
			}
		} else if ((*c & 0xf8) == 0xf0) {
			if((c[1] & c[2] & c[3] & 0x80) && ((c[1] | c[2] | c[3]) & 0x40) == 0x00) {
				*w=((c[0] & 0x07)<<18) | ((c[1]&0x1f)<<12) | ((c[2]&0x3f)<<6) | (c[3]&0x3f);
				c += 3;
			} else {
				*w = '?';
			}
		} else
			*w=(c[0] & 0x7f);

		c++;
		w++;
		len++;
	}
	return len;
}

/* The vfont code */
void free_vfont(struct VFont *vf)
{
	if (vf == 0) return;

	if (vf->data) {
		while(vf->data->characters.first)
		{
			VChar *che = vf->data->characters.first;
			
			while (che->nurbsbase.first) {
				Nurb *nu = che->nurbsbase.first;
				if (nu->bezt) MEM_freeN(nu->bezt);
				BLI_freelinkN(&che->nurbsbase, nu);
			}
	
			BLI_freelinkN(&vf->data->characters, che);
		}

		MEM_freeN(vf->data);
		vf->data = NULL;
	}
	
	if (vf->packedfile) {
		freePackedFile(vf->packedfile);
		vf->packedfile = NULL;
	}
}

static void *builtin_font_data= NULL;
static int builtin_font_size= 0;

void BKE_font_register_builtin(void *mem, int size)
{
	builtin_font_data= mem;
	builtin_font_size= size;
}

static PackedFile *get_builtin_packedfile(void)
{
	if (!builtin_font_data) {
		printf("Internal error, builtin font not loaded\n");

		return NULL;
	} else {
		void *mem= MEM_mallocN(builtin_font_size, "vfd_builtin");

		memcpy(mem, builtin_font_data, builtin_font_size);
	
		return newPackedFileMemory(mem, builtin_font_size);
	}
}

void free_ttfont(void)
{
	struct TmpFont *tf;
	
	tf= ttfdata.first;
	while(tf) {
		freePackedFile(tf->pf);
		tf->pf= NULL;
		tf->vfont= NULL;
		tf= tf->next;
	}
	BLI_freelistN(&ttfdata);
}

struct TmpFont *vfont_find_tmpfont(VFont *vfont)
{
	struct TmpFont *tmpfnt = NULL;
	
	if(vfont==NULL) return NULL;
	
	// Try finding the font from font list
	tmpfnt = ttfdata.first;
	while(tmpfnt)
	{
		if(tmpfnt->vfont == vfont)
			break;
		tmpfnt = tmpfnt->next;
	}
	return tmpfnt;
}

static VFontData *vfont_get_data(VFont *vfont)
{
	struct TmpFont *tmpfnt = NULL;
	PackedFile *tpf;
	
	if(vfont==NULL) return NULL;
	
	// Try finding the font from font list
	tmpfnt = vfont_find_tmpfont(vfont);
	
	// And then set the data	
	if (!vfont->data) {
		PackedFile *pf;
		
		if (BLI_streq(vfont->name, "<builtin>")) {
			pf= get_builtin_packedfile();
		} else {
			if (vfont->packedfile) {
				pf= vfont->packedfile;
				
				// We need to copy a tmp font to memory unless it is already there
				if(!tmpfnt)
				{
					tpf= MEM_callocN(sizeof(*tpf), "PackedFile");
					tpf->data= MEM_mallocN(pf->size, "packFile");
					tpf->size= pf->size;
					memcpy(tpf->data, pf->data, pf->size);
					
					// Add temporary packed file to globals
					tmpfnt= (struct TmpFont *) MEM_callocN(sizeof(struct TmpFont), "temp_font");
					tmpfnt->pf= tpf;
					tmpfnt->vfont= vfont;
					BLI_addtail(&ttfdata, tmpfnt);
				}
			} else {
				pf= newPackedFile(NULL, vfont->name);
				
				if(!tmpfnt)
				{
					tpf= newPackedFile(NULL, vfont->name);
					
					// Add temporary packed file to globals
					tmpfnt= (struct TmpFont *) MEM_callocN(sizeof(struct TmpFont), "temp_font");
					tmpfnt->pf= tpf;
					tmpfnt->vfont= vfont;
					BLI_addtail(&ttfdata, tmpfnt);
				}
			}
			if(!pf) {
				printf("Font file doesn't exist: %s\n", vfont->name);

				strcpy(vfont->name, "<builtin>");
				pf= get_builtin_packedfile();
			}
		}
		
		if (pf) {
			vfont->data= BLI_vfontdata_from_freetypefont(pf);
			if (pf != vfont->packedfile) {
				freePackedFile(pf);
			}
		}
	}
	
	return vfont->data;	
}

VFont *load_vfont(char *name)
{
	char filename[FILE_MAXFILE];
	VFont *vfont= NULL;
	PackedFile *pf;
	PackedFile *tpf = NULL;	
	int is_builtin;
	struct TmpFont *tmpfnt;
	
	if (BLI_streq(name, "<builtin>")) {
		strcpy(filename, name);
		
		pf= get_builtin_packedfile();
		is_builtin= 1;
	} else {
		char dir[FILE_MAXDIR];
		
		strcpy(dir, name);
		BLI_splitdirstring(dir, filename);

		pf= newPackedFile(NULL, name);
		tpf= newPackedFile(NULL, name);		
		
		is_builtin= 0;
	}

	if (pf) {
		VFontData *vfd;

		vfd= BLI_vfontdata_from_freetypefont(pf);
		if (vfd) {
			vfont = alloc_libblock(&G.main->vfont, ID_VF, filename);
			vfont->data = vfd;

			/* if there's a font name, use it for the ID name */
			if (strcmp(vfd->name, "")!=0) {
				BLI_strncpy(vfont->id.name+2, vfd->name, 21);
			}
			BLI_strncpy(vfont->name, name, sizeof(vfont->name));

			// if autopack is on store the packedfile in de font structure
			if (!is_builtin && (G.fileflags & G_AUTOPACK)) {
				vfont->packedfile = pf;
			}
			
			// Do not add <builtin> to temporary listbase
			if(strcmp(filename, "<builtin>"))
			{
				tmpfnt= (struct TmpFont *) MEM_callocN(sizeof(struct TmpFont), "temp_font");
				tmpfnt->pf= tpf;
				tmpfnt->vfont= vfont;
				BLI_addtail(&ttfdata, tmpfnt);
			}			
		}
		
		// Free the packed file
		if (!vfont || vfont->packedfile != pf) {
			freePackedFile(pf);
		}
	
		//XXX waitcursor(0);
	}
	
	return vfont;
}

static VFont *which_vfont(Curve *cu, CharInfo *info)
{
	switch(info->flag & CU_STYLE) {
		case CU_BOLD:
			if (cu->vfontb) return(cu->vfontb); else return(cu->vfont);
		case CU_ITALIC:
			if (cu->vfonti) return(cu->vfonti); else return(cu->vfont);
		case (CU_BOLD|CU_ITALIC):
			if (cu->vfontbi) return(cu->vfontbi); else return(cu->vfont);
		default:
			return(cu->vfont);
	}			
}

VFont *get_builtin_font(void)
{
	VFont *vf;
	
	for (vf= G.main->vfont.first; vf; vf= vf->id.next)
		if (BLI_streq(vf->name, "<builtin>"))
			return vf;
	
	return load_vfont("<builtin>");
}

static void build_underline(Curve *cu, float x1, float y1, float x2, float y2, int charidx, short mat_nr)
{
	Nurb *nu2;
	BPoint *bp;
	
	nu2 =(Nurb*) MEM_callocN(sizeof(Nurb),"underline_nurb");
	if (nu2 == NULL) return;
	nu2->resolu= cu->resolu;
	nu2->bezt = NULL;
	nu2->knotsu = nu2->knotsv = NULL;
	nu2->flag= CU_2D;
	nu2->charidx = charidx+1000;
	if (mat_nr > 0) nu2->mat_nr= mat_nr-1;
	nu2->pntsu = 4;
	nu2->pntsv = 1;
	nu2->orderu = 4;
	nu2->orderv = 1;
	nu2->flagu = CU_NURB_CYCLIC;

	bp = (BPoint*)MEM_callocN(4 * sizeof(BPoint),"underline_bp"); 
	if (bp == 0){
		MEM_freeN(nu2);
		return;
	}
	nu2->bp = bp;

	nu2->bp[0].vec[0] = x1;
	nu2->bp[0].vec[1] = y1;	
	nu2->bp[0].vec[2] = 0;
	nu2->bp[0].vec[3] = 1.0;
	nu2->bp[1].vec[0] = x2;
	nu2->bp[1].vec[1] = y1;
	nu2->bp[1].vec[2] = 0;	
	nu2->bp[1].vec[3] = 1.0;	
	nu2->bp[2].vec[0] = x2;
	nu2->bp[2].vec[1] = y2;	
	nu2->bp[2].vec[2] = 0;
	nu2->bp[2].vec[3] = 1.0; 
	nu2->bp[3].vec[0] = x1;
	nu2->bp[3].vec[1] = y2;
	nu2->bp[3].vec[2] = 0;	
	nu2->bp[3].vec[3] = 1.0;	
	
	BLI_addtail(&(cu->nurb), nu2);	

}

static void buildchar(Curve *cu, unsigned long character, CharInfo *info, float ofsx, float ofsy, float rot, int charidx)
{
	BezTriple *bezt1, *bezt2;
	Nurb *nu1 = NULL, *nu2 = NULL;
	float *fp, fsize, shear, x, si, co;
	VFontData *vfd = NULL;
	VChar *che = NULL;
	int i, sel=0;

	vfd= vfont_get_data(which_vfont(cu, info));	
	if (!vfd) return;

	if (cu->selend < cu->selstart) {
		if ((charidx >= (cu->selend)) && (charidx <= (cu->selstart-2)))
			sel= 1;
	}
	else {
		if ((charidx >= (cu->selstart-1)) && (charidx <= (cu->selend-1)))
			sel= 1;
	}

	/* make a copy at distance ofsx,ofsy with shear*/
	fsize= cu->fsize;
	shear= cu->shear;
	si= (float)sin(rot);
	co= (float)cos(rot);

	// Find the correct character from the font
	che = vfd->characters.first;
	while(che)
	{
		if(che->index == character)
			break;
		che = che->next;
	}
	
	// Select the glyph data
	if(che)
		nu1 = che->nurbsbase.first;

	// Create the character
	while(nu1)
	{
		bezt1 = nu1->bezt;
		if (bezt1){
			nu2 =(Nurb*) MEM_mallocN(sizeof(Nurb),"duplichar_nurb");
			if (nu2 == 0) break;
			memcpy(nu2, nu1, sizeof(struct Nurb));
			nu2->resolu= cu->resolu;
			nu2->bp = 0;
			nu2->knotsu = nu2->knotsv = NULL;
			nu2->flag= CU_SMOOTH;
			nu2->charidx = charidx;
			if (info->mat_nr) {
				nu2->mat_nr= info->mat_nr-1;
			}
			else {
				nu2->mat_nr= 0;
			}
			/* nu2->trim.first = 0; */
			/* nu2->trim.last = 0; */
			i = nu2->pntsu;

			bezt2 = (BezTriple*)MEM_mallocN(i * sizeof(BezTriple),"duplichar_bezt2"); 
			if (bezt2 == 0){
				MEM_freeN(nu2);
				break;
			}
			memcpy(bezt2, bezt1, i * sizeof(struct BezTriple));
			nu2->bezt = bezt2;
			
			if (shear != 0.0) {
				bezt2 = nu2->bezt;
				
				for (i= nu2->pntsu; i > 0; i--) {
					bezt2->vec[0][0] += shear * bezt2->vec[0][1];
					bezt2->vec[1][0] += shear * bezt2->vec[1][1];
					bezt2->vec[2][0] += shear * bezt2->vec[2][1];
					bezt2++;
				}
			}
			if(rot!=0.0) {
				bezt2= nu2->bezt;
				for (i=nu2->pntsu; i > 0; i--) {
					fp= bezt2->vec[0];
					
					x= fp[0];
					fp[0]= co*x + si*fp[1];
					fp[1]= -si*x + co*fp[1];
					x= fp[3];
					fp[3]= co*x + si*fp[4];
					fp[4]= -si*x + co*fp[4];
					x= fp[6];
					fp[6]= co*x + si*fp[7];
					fp[7]= -si*x + co*fp[7];

					bezt2++;
				}
			}
			bezt2 = nu2->bezt;

			for (i= nu2->pntsu; i > 0; i--) {
				fp= bezt2->vec[0];

				fp[0]= (fp[0]+ofsx)*fsize;
				fp[1]= (fp[1]+ofsy)*fsize;
				fp[3]= (fp[3]+ofsx)*fsize;
				fp[4]= (fp[4]+ofsy)*fsize;
				fp[6]= (fp[6]+ofsx)*fsize;
				fp[7]= (fp[7]+ofsy)*fsize;
				bezt2++;
			}
			
			BLI_addtail(&(cu->nurb), nu2);
		}
		
		nu1 = nu1->next;
	}
}

int BKE_font_getselection(Object *ob, int *start, int *end)
{
	Curve *cu= ob->data;
	
	if (cu->editfont==NULL || ob->type != OB_FONT) return 0;

	if (cu->selstart == 0) return 0;
	if (cu->selstart <= cu->selend) {
		*start = cu->selstart-1;
		*end = cu->selend-1;
		return 1;
	}
	else {
		*start = cu->selend;
		*end = cu->selstart-2;
		return -1;
	}
}

struct chartrans *BKE_text_to_curve(Scene *scene, Object *ob, int mode) 
{
	VFont *vfont, *oldvfont;
	VFontData *vfd= NULL;
	Curve *cu;
	CharInfo *info, *custrinfo;
	TextBox *tb;
	VChar *che;
	struct chartrans *chartransdata=NULL, *ct;
	float *f, xof, yof, xtrax, linedist, *linedata, *linedata2, *linedata3, *linedata4;
	float twidth, maxlen= 0;
	int i, slen, j;
	int curbox;
	int selstart, selend;
	int utf8len;
	short cnr=0, lnr=0, wsnr= 0;
	wchar_t *mem, *tmp, ascii;

	/* renark: do calculations including the trailing '\0' of a string
	   because the cursor can be at that location */

	if(ob->type!=OB_FONT) return 0;

	// Set font data
	cu= (Curve *) ob->data;
	vfont= cu->vfont;
	
	if(cu->str == NULL) return 0;
	if(vfont == NULL) return 0;

	// Create unicode string
	utf8len = utf8slen(cu->str);
	tmp = mem = MEM_callocN(((utf8len + 1) * sizeof(wchar_t)), "convertedmem");
	
	utf8towchar(mem, cu->str);

	// Count the wchar_t string length
	slen = wcslen(mem);

	if (cu->ulheight == 0.0) 
		cu->ulheight = 0.05;
	
	if (cu->strinfo==NULL)	/* old file */
		cu->strinfo = MEM_callocN((slen+4) * sizeof(CharInfo), "strinfo compat");
	
	custrinfo= cu->strinfo;
	if (cu->editfont)
		custrinfo= cu->editfont->textbufinfo;
	
	if (cu->tb==NULL)
		cu->tb= MEM_callocN(MAXTEXTBOX*sizeof(TextBox), "TextBox compat");

	vfd= vfont_get_data(vfont);

	/* The VFont Data can not be found */
	if(!vfd) {
		if(mem)
			MEM_freeN(mem);	
		return 0;
	}

	/* calc offset and rotation of each char */
	ct = chartransdata =
		(struct chartrans*)MEM_callocN((slen+1)* sizeof(struct chartrans),"buildtext");

	/* We assume the worst case: 1 character per line (is freed at end anyway) */

	linedata= MEM_mallocN(sizeof(float)*(slen*2 + 1),"buildtext2");
	linedata2= MEM_mallocN(sizeof(float)*(slen*2 + 1),"buildtext3");
	linedata3= MEM_callocN(sizeof(float)*(slen*2 + 1),"buildtext4");	
	linedata4= MEM_callocN(sizeof(float)*(slen*2 + 1),"buildtext5");		
	
	linedist= cu->linedist;
	
	xof= cu->xof + (cu->tb[0].x/cu->fsize);
	yof= cu->yof + (cu->tb[0].y/cu->fsize);

	xtrax= 0.5f*cu->spacing-0.5f;

	oldvfont = NULL;

	for (i=0; i<slen; i++) custrinfo[i].flag &= ~CU_WRAP;

	if (cu->selboxes) MEM_freeN(cu->selboxes);
	cu->selboxes = NULL;
	if (BKE_font_getselection(ob, &selstart, &selend))
		cu->selboxes = MEM_callocN((selend-selstart+1)*sizeof(SelBox), "font selboxes");

	tb = &(cu->tb[0]);
	curbox= 0;
	for (i = 0 ; i<=slen ; i++) {
	makebreak:
		// Characters in the list
		che = vfd->characters.first;
		ascii = mem[i];
		info = &(custrinfo[i]);
		vfont = which_vfont(cu, info);
		
		if(vfont==NULL) break;
		
		// Find the character
		while(che) {
			if(che->index == ascii)
				break;
			che = che->next;
		}

		/*
		 * The character wasn't in the current curve base so load it
		 * But if the font is <builtin> then do not try loading since
		 * whole font is in the memory already
		 */
		if(che == NULL && strcmp(vfont->name, "<builtin>"))	{
			BLI_vfontchar_from_freetypefont(vfont, ascii);
		}

		/* Try getting the character again from the list */
		che = vfd->characters.first;
		while(che) {
			if(che->index == ascii)
				break;
			che = che->next;
		}

		/* No VFont found */
		if (vfont==0) {
			if(mem)
				MEM_freeN(mem);
			MEM_freeN(chartransdata);
			return 0;
		}

		if (vfont != oldvfont) {
			vfd= vfont_get_data(vfont);
			oldvfont = vfont;
		}

		/* VFont Data for VFont couldn't be found */
		if (!vfd) {
			if(mem)
				MEM_freeN(mem);
			MEM_freeN(chartransdata);
			return 0;
		}

		// The character wasn't found, propably ascii = 0, then the width shall be 0 as well
		if(!che)
			twidth = 0;
		else
			twidth = che->width;

		// Calculate positions
		if((tb->w != 0.0) && (ct->dobreak==0) && ((xof-(tb->x/cu->fsize)+twidth)*cu->fsize) > tb->w) {
	//		fprintf(stderr, "linewidth exceeded: %c%c%c...\n", mem[i], mem[i+1], mem[i+2]);
			for (j=i; j && (mem[j] != '\n') && (mem[j] != '\r') && (chartransdata[j].dobreak==0); j--) {
				if (mem[j]==' ' || mem[j]=='-') {
					ct -= (i-(j-1));
					cnr -= (i-(j-1));
					if (mem[j] == ' ') wsnr--;
					if (mem[j] == '-') wsnr++;
					i = j-1;
					xof = ct->xof;
					ct[1].dobreak = 1;
					custrinfo[i+1].flag |= CU_WRAP;
					goto makebreak;
				}
				if (chartransdata[j].dobreak) {
	//				fprintf(stderr, "word too long: %c%c%c...\n", mem[j], mem[j+1], mem[j+2]);
					ct->dobreak= 1;
					custrinfo[i+1].flag |= CU_WRAP;
					ct -= 1;
					cnr -= 1;
					i--;
					xof = ct->xof;
					goto makebreak;
				}
			}
		}
		if(ascii== '\n' || ascii== '\r' || ascii==0 || ct->dobreak) {
			ct->xof= xof;
			ct->yof= yof;
			ct->linenr= lnr;
			ct->charnr= cnr;
			
			yof-= linedist;
			
			maxlen= MAX2(maxlen, (xof-tb->x/cu->fsize));
			linedata[lnr]= xof-tb->x/cu->fsize;
			linedata2[lnr]= cnr;
			linedata3[lnr]= tb->w/cu->fsize;
			linedata4[lnr]= wsnr;
			
			if ( (tb->h != 0.0) &&
				 ((-(yof-(tb->y/cu->fsize))) > ((tb->h/cu->fsize)-(linedist*cu->fsize))) &&
				 (cu->totbox > (curbox+1)) ) {
				maxlen= 0;
				tb++;
				curbox++;
				yof= cu->yof + tb->y/cu->fsize;
			}

			if(ascii == '\n' || ascii == '\r')
				xof = cu->xof;
			else
				xof= cu->xof + (tb->x/cu->fsize);

			xof= cu->xof + (tb->x/cu->fsize);
			lnr++;
			cnr= 0;
			wsnr= 0;
		}
		else if(ascii==9) {	/* TAB */
			float tabfac;
			
			ct->xof= xof;
			ct->yof= yof;
			ct->linenr= lnr;
			ct->charnr= cnr++;

			tabfac= (xof-cu->xof+0.01f);
			tabfac= (float)(2.0*ceil(tabfac/2.0));
			xof= cu->xof+tabfac;
		}
		else {
			SelBox *sb= NULL;
			float wsfac;

			ct->xof= xof;
			ct->yof= yof;
			ct->linenr= lnr;
			ct->charnr= cnr++;

			if (cu->selboxes && (i>=selstart) && (i<=selend)) {
				sb = &(cu->selboxes[i-selstart]);
				sb->y = yof*cu->fsize-linedist*cu->fsize*0.1;
				sb->h = linedist*cu->fsize;
				sb->w = xof*cu->fsize;
			}
	
			if (ascii==32) {
				wsfac = cu->wordspace; 
				wsnr++;
			} 
			else wsfac = 1.0;
			
			// Set the width of the character
			if(!che)
				twidth = 0;
			else 
				twidth = che->width;

			xof += (twidth*wsfac*(1.0+(info->kern/40.0)) ) + xtrax;
			
			if (sb) 
				sb->w = (xof*cu->fsize) - sb->w;
		}
		ct++;
	}
	
	cu->lines= 1;
	ct= chartransdata;
	tmp = mem;
	for (i= 0; i<=slen; i++, tmp++, ct++) {
		ascii = *tmp;
		if(ascii== '\n' || ascii== '\r' || ct->dobreak) cu->lines++;
	}	

	// linedata is now: width of line
	// linedata2 is now: number of characters
	// linedata3 is now: maxlen of that line
	// linedata4 is now: number of whitespaces of line

	if(cu->spacemode!=CU_LEFT) {
		ct= chartransdata;

		if(cu->spacemode==CU_RIGHT) {
			for(i=0;i<lnr;i++) linedata[i]= linedata3[i]-linedata[i];
			for (i=0; i<=slen; i++) {
				ct->xof+= linedata[ct->linenr];
				ct++;
			}
		} else if(cu->spacemode==CU_MIDDLE) {
			for(i=0;i<lnr;i++) linedata[i]= (linedata3[i]-linedata[i])/2;
			for (i=0; i<=slen; i++) {
				ct->xof+= linedata[ct->linenr];
				ct++;
			}
		} else if((cu->spacemode==CU_FLUSH) &&
				  (cu->tb[0].w != 0.0)) {
			for(i=0;i<lnr;i++)
				if(linedata2[i]>1)
					linedata[i]= (linedata3[i]-linedata[i])/(linedata2[i]-1);
			for (i=0; i<=slen; i++) {
				for (j=i; (mem[j]) && (mem[j]!='\n') && 
						  (mem[j]!='\r') && (chartransdata[j].dobreak==0) && (j<slen); j++);
//				if ((mem[j]!='\r') && (mem[j]!='\n') && (mem[j])) {
					ct->xof+= ct->charnr*linedata[ct->linenr];
//				}
				ct++;
			}
		} 
		else if((cu->spacemode==CU_JUSTIFY) && (cu->tb[0].w != 0.0)) {
			float curofs= 0.0f;
			for (i=0; i<=slen; i++) {
				for (j=i; (mem[j]) && (mem[j]!='\n') && 
						  (mem[j]!='\r') && (chartransdata[j].dobreak==0) && (j<slen); j++);
				if ((mem[j]!='\r') && (mem[j]!='\n') &&
					((chartransdata[j].dobreak!=0))) {
					if (mem[i]==' ') curofs += (linedata3[ct->linenr]-linedata[ct->linenr])/linedata4[ct->linenr];
					ct->xof+= curofs;
				}
				if (mem[i]=='\n' || mem[i]=='\r' || chartransdata[i].dobreak) curofs= 0;
				ct++;
			}			
		}
	}
	
	/* TEXT ON CURVE */
	/* Note: Only OB_CURVE objects could have a path  */
	if(cu->textoncurve && cu->textoncurve->type==OB_CURVE) {
		Curve *cucu= cu->textoncurve->data;
		int oldflag= cucu->flag;
		
		cucu->flag |= (CU_PATH+CU_FOLLOW);
		
		if(cucu->path==NULL) makeDispListCurveTypes(scene, cu->textoncurve, 0);
		if(cucu->path) {
			float distfac, imat[4][4], imat3[3][3], cmat[3][3];
			float minx, maxx, miny, maxy;
			float timeofs, sizefac;
			
			invert_m4_m4(imat, ob->obmat);
			copy_m3_m4(imat3, imat);

			copy_m3_m4(cmat, cu->textoncurve->obmat);
			mul_m3_m3m3(cmat, cmat, imat3);
			sizefac= normalize_v3(cmat[0])/cu->fsize;
			
			minx=miny= 1.0e20f;
			maxx=maxy= -1.0e20f;
			ct= chartransdata;
			for (i=0; i<=slen; i++, ct++) {
				if(minx>ct->xof) minx= ct->xof;
				if(maxx<ct->xof) maxx= ct->xof;
				if(miny>ct->yof) miny= ct->yof;
				if(maxy<ct->yof) maxy= ct->yof;
			}
			
			/* we put the x-coordinaat exact at the curve, the y is rotated */
			
			/* length correction */
			distfac= sizefac*cucu->path->totdist/(maxx-minx);
			timeofs= 0.0;
			
			if(distfac > 1.0) {
				/* path longer than text: spacemode involves */
				distfac= 1.0f/distfac;
				
				if(cu->spacemode==CU_RIGHT) {
					timeofs= 1.0f-distfac;
				}
				else if(cu->spacemode==CU_MIDDLE) {
					timeofs= (1.0f-distfac)/2.0f;
				}
				else if(cu->spacemode==CU_FLUSH) distfac= 1.0f;
				
			}
			else distfac= 1.0;
			
			distfac/= (maxx-minx);
			
			timeofs+= distfac*cu->xof;	/* not cyclic */
			
			ct= chartransdata;
			for (i=0; i<=slen; i++, ct++) {
				float ctime, dtime, vec[4], tvec[4], rotvec[3];
				float si, co;
				
				/* rotate around center character */
				ascii = mem[i];
				
				// Find the character
				che = vfd->characters.first;
				while(che) {
					if(che->index == ascii)
						break;
					che = che->next;
				}
	
				if(che)
					twidth = che->width;
				else
					twidth = 0;
				
				dtime= distfac*0.35f*twidth;	/* why not 0.5? */
				dtime= distfac*0.5f*twidth;	/* why not 0.5? */
				
				ctime= timeofs + distfac*( ct->xof - minx);
				CLAMP(ctime, 0.0, 1.0);

				/* calc the right loc AND the right rot separately */
				/* vec, tvec need 4 items */
				where_on_path(cu->textoncurve, ctime, vec, tvec, NULL, NULL);
				where_on_path(cu->textoncurve, ctime+dtime, tvec, rotvec, NULL, NULL);
				
				mul_v3_fl(vec, sizefac);
				
				ct->rot= (float)(M_PI-atan2(rotvec[1], rotvec[0]));

				si= (float)sin(ct->rot);
				co= (float)cos(ct->rot);

				yof= ct->yof;
				
				ct->xof= vec[0] + si*yof;
				ct->yof= vec[1] + co*yof;
				
			}
			cucu->flag= oldflag;
		}
	}

	if (cu->selboxes) {
		ct= chartransdata;
		for (i=0; i<=selend; i++, ct++) {
			if (i>=selstart) {
				cu->selboxes[i-selstart].x = ct->xof*cu->fsize;
				cu->selboxes[i-selstart].y = ct->yof*cu->fsize;				
			}
		}
	}

	if(mode==FO_CURSUP || mode==FO_CURSDOWN || mode==FO_PAGEUP || mode==FO_PAGEDOWN) {
		/* 2: curs up
		   3: curs down */
		ct= chartransdata+cu->pos;
		
		if((mode==FO_CURSUP || mode==FO_PAGEUP) && ct->linenr==0);
		else if((mode==FO_CURSDOWN || mode==FO_PAGEDOWN) && ct->linenr==lnr);
		else {
			switch(mode) {
				case FO_CURSUP:		lnr= ct->linenr-1; break;
				case FO_CURSDOWN:	lnr= ct->linenr+1; break;
				case FO_PAGEUP:		lnr= ct->linenr-10; break;
				case FO_PAGEDOWN:	lnr= ct->linenr+10; break;
			}
			cnr= ct->charnr;
			/* seek for char with lnr en cnr */
			cu->pos= 0;
			ct= chartransdata;
			for (i= 0; i<slen; i++) {
				if(ct->linenr==lnr) {
					if(ct->charnr==cnr) break;
					if( (ct+1)->charnr==0) break;
				}
				else if(ct->linenr>lnr) break;
				cu->pos++;
				ct++;
			}
		}
	}
	
	/* cursor first */
	if(cu->editfont) {
		float si, co;
		
		ct= chartransdata+cu->pos;
		si= (float)sin(ct->rot);
		co= (float)cos(ct->rot);
				
		f= cu->editfont->textcurs[0];
		
		f[0]= cu->fsize*(-0.1f*co + ct->xof);
		f[1]= cu->fsize*(0.1f*si + ct->yof);
		
		f[2]= cu->fsize*(0.1f*co + ct->xof);
		f[3]= cu->fsize*(-0.1f*si + ct->yof);
		
		f[4]= cu->fsize*( 0.1f*co + 0.8f*si + ct->xof);
		f[5]= cu->fsize*(-0.1f*si + 0.8f*co + ct->yof);
		
		f[6]= cu->fsize*(-0.1f*co + 0.8f*si + ct->xof);
		f[7]= cu->fsize*( 0.1f*si + 0.8f*co + ct->yof);
		
	}

	MEM_freeN(linedata);
	MEM_freeN(linedata2);		
	MEM_freeN(linedata3);
	MEM_freeN(linedata4);

	if (mode == FO_SELCHANGE) {
		MEM_freeN(chartransdata);
		MEM_freeN(mem);
		return NULL;
	}

	if(mode==0) {
		/* make nurbdata */
		unsigned long cha;
		
		freeNurblist(&cu->nurb);
		
		ct= chartransdata;
		if (cu->sepchar==0) {
			for (i= 0; i<slen; i++) {
				cha = (uintptr_t) mem[i];
				info = &(custrinfo[i]);
				if (info->mat_nr > (ob->totcol)) {
					/* printf("Error: Illegal material index (%d) in text object, setting to 0\n", info->mat_nr); */
					info->mat_nr = 0;
				}
				// We do not want to see any character for \n or \r
				if(cha != '\n' && cha != '\r')
					buildchar(cu, cha, info, ct->xof, ct->yof, ct->rot, i);
				
				if ((info->flag & CU_UNDERLINE) && (cu->textoncurve == NULL) && (cha != '\n') && (cha != '\r')) {
					float ulwidth, uloverlap= 0.0f;
					
					if ( (i<(slen-1)) && (mem[i+1] != '\n') && (mem[i+1] != '\r') &&
						 ((mem[i+1] != ' ') || (custrinfo[i+1].flag & CU_UNDERLINE)) && ((custrinfo[i+1].flag & CU_WRAP)==0)
						 ) {
						uloverlap = xtrax + 0.1;
					}
					// Find the character, the characters has to be in the memory already 
					// since character checking has been done earlier already.
					che = vfd->characters.first;
					while(che) {
						if(che->index == cha)
							break;
						che = che->next;
					}
					
					if(!che) twidth =0; else twidth=che->width;
					ulwidth = cu->fsize * ((twidth* (1.0+(info->kern/40.0)))+uloverlap);
					build_underline(cu, ct->xof*cu->fsize, ct->yof*cu->fsize + (cu->ulpos-0.05)*cu->fsize, 
									ct->xof*cu->fsize + ulwidth, 
									ct->yof*cu->fsize + (cu->ulpos-0.05)*cu->fsize - cu->ulheight*cu->fsize, 
									i, info->mat_nr);
				}
				ct++;
			}
		}
		else {
			int outta = 0;
			for (i= 0; (i<slen) && (outta==0); i++) {
				ascii = mem[i];
				info = &(custrinfo[i]);
				if (cu->sepchar == (i+1)) {
					float vecyo[3];
					
					mem[0] = ascii;
					mem[1] = 0;
					custrinfo[0]= *info;
					cu->pos = 1;
					cu->len = 1;
					vecyo[0] = ct->xof;
					vecyo[1] = ct->yof;
					vecyo[2] = 0;
					mul_m4_v3(ob->obmat, vecyo);
					VECCOPY(ob->loc, vecyo);
					outta = 1;
					cu->sepchar = 0;
				}
				ct++;
			}
		}
	}

	if(mode==FO_DUPLI) {
		MEM_freeN(mem);
		return chartransdata;
	}

	if(mem)
		MEM_freeN(mem);

	MEM_freeN(chartransdata);
	return 0;
}


