
/*  font.c     
 *  
 * 
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
#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_vfontdata.h"

#include "DNA_packedFile_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_vfont_types.h"
#include "DNA_scene_types.h"

#include "BKE_utildefines.h"
#include "BKE_bad_level_calls.h"

#include "BKE_packedFile.h"

#include "BKE_library.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_anim.h"
#include "BKE_curve.h"
#include "BKE_displist.h"

#define callocstructN(x,y,name) (x*)MEM_callocN((y)* sizeof(x),name)
 
struct SelBox *selboxes= NULL;

struct chartrans {
	float xof, yof;
	float rot;
	short linenr,charnr;
	char dobreak;
};

void free_vfont(struct VFont *vf)
{
	int i;

	if (vf == 0) return;

	if (vf->data) {
		for (i = 0; i < MAX_VF_CHARS; i++){
			while (vf->data->nurbsbase[i].first) {
				Nurb *nu = vf->data->nurbsbase[i].first;
				if (nu->bezt) MEM_freeN(nu->bezt);
				BLI_freelinkN(&vf->data->nurbsbase[i], nu);
			}
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

static VFontData *vfont_get_data(VFont *vfont)
{
	if (!vfont->data) {
		PackedFile *pf;
		
		if (BLI_streq(vfont->name, "<builtin>")) {
			pf= get_builtin_packedfile();
		} else {
			if (vfont->packedfile) {
				pf= vfont->packedfile;
			} else {
				pf= newPackedFile(vfont->name);
			}
			if(!pf) {
				printf("Font file doesn't exist: %s\n", vfont->name);

				strcpy(vfont->name, "<builtin>");
				pf= get_builtin_packedfile();
			}
		}
		
		if (pf) {
#ifdef WITH_FREETYPE2
			vfont->data= BLI_vfontdata_from_freetypefont(pf);
#else
			vfont->data= BLI_vfontdata_from_psfont(pf);
#endif			
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
	int is_builtin;
	
	if (BLI_streq(name, "<builtin>")) {
		strcpy(filename, name);
		
		pf= get_builtin_packedfile();
		is_builtin= 1;
	} else {
		char dir[FILE_MAXDIR];
		
		strcpy(dir, name);
		BLI_splitdirstring(dir, filename);

		pf= newPackedFile(name);
		is_builtin= 0;
	}

	if (pf) {
		VFontData *vfd;
		
		waitcursor(1);

#ifdef WITH_FREETYPE2
		vfd= BLI_vfontdata_from_freetypefont(pf);
#else
		vfd= BLI_vfontdata_from_psfont(pf);
#endif			
		
		if (vfd) {
			vfont = alloc_libblock(&G.main->vfont, ID_VF, filename);
			vfont->data = vfd;
			
			BLI_strncpy(vfont->name, name, sizeof(vfont->name));

			// if autopack is on store the packedfile in de font structure
			if (!is_builtin && (G.fileflags & G_AUTOPACK)) {
				vfont->packedfile = pf;
			}
		}
		if (!vfont || vfont->packedfile != pf) {
			freePackedFile(pf);
		}
	
		waitcursor(0);
	}
	
	return vfont;
}

static VFont *which_vfont(Curve *cu, CharInfo *info)
{
   	switch(info->flag & CU_STYLE) {
   		case CU_BOLD:
   			return(cu->vfontb);
   		case CU_ITALIC:
   			return(cu->vfonti);
   		case (CU_BOLD|CU_ITALIC):
   			return(cu->vfontbi);
   		default:
   			return(cu->vfont);
   	}			
}

static void buildchar(Curve *cu, unsigned char ascii, CharInfo *info, float ofsx, float ofsy, float rot, int charidx)
{
	BezTriple *bezt1, *bezt2;
	Nurb *nu1, *nu2;
	float *fp, fsize, shear, x, si, co;
	VFontData *vfd;
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

	nu1 = vfd->nurbsbase[ascii].first;
	while(nu1)
	{
		bezt1 = nu1->bezt;
		if (bezt1){
			nu2 =(Nurb*) MEM_mallocN(sizeof(Nurb),"duplichar_nurb");
			if (nu2 == 0) break;
			memcpy(nu2, nu1, sizeof(struct Nurb));
			nu2->resolu= cu->resolu;
			nu2->bp = 0;
			nu2->knotsu = nu2->knotsv = 0;
			nu2->flag= CU_SMOOTH;
			nu2->charidx = charidx;
			if (info->mat_nr) nu2->mat_nr= info->mat_nr-1;
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

int getselection(int *start, int *end)
{
	Curve *cu;
	
	if (G.obedit==NULL || G.obedit->type != OB_FONT) return 0;
	
	cu= G.obedit->data;

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

struct chartrans *text_to_curve(Object *ob, int mode) 
{
	VFont *vfont, *oldvfont;
	VFontData *vfd= NULL;
	Curve *cu, *cucu;
	struct chartrans *chartransdata, *ct;
	float distfac, tabfac, ctime, dtime, tvec[4], vec[4], rotvec[3], minx, maxx, miny, maxy;
	float cmat[3][3], timeofs, si, co, sizefac;
	float *f, maxlen=0, xof, yof, xtrax, linedist, *linedata, *linedata2, *linedata3;
	int i, slen, oldflag, j;
	short cnr=0, lnr=0;
	char ascii, *mem;
	int outta;
	float vecyo[3];
	CharInfo *info;
	float wsfac;
	TextBox *tb;
	int curbox;
	int selstart, selend;
	SelBox *sb= NULL;	/* to please gcc */

	/* renark: do calculations including the trailing '\0' of a string
	   because the cursor can be at that location */

	if(ob->type!=OB_FONT) return 0;

	cu= ob->data;
	mem= cu->str;
	slen = strlen(mem);	

	if (cu->str==0) return 0;
	if (cu->strinfo==NULL) {	/* old file */
		cu->strinfo = MEM_callocN((slen+1) * sizeof(CharInfo), "strinfo compat");
	}

	/* calc offset and rotation of each char */
	ct = chartransdata =
		(struct chartrans*)MEM_callocN((slen+1)* sizeof(struct chartrans),"buildtext");

	/* We assume the worst case: 1 character per line (is freed at end anyway) */

	linedata= MEM_mallocN(sizeof(float)*(slen+2),"buildtext2");
	linedata2= MEM_mallocN(sizeof(float)*(slen+2),"buildtext3");
	linedata3= MEM_callocN(sizeof(float)*(slen+2),"buildtext4");	
	
	linedist= cu->linedist;
	
	xof= cu->xof + (cu->tb[0].x/cu->fsize);
	yof= cu->yof + (cu->tb[0].y/cu->fsize);

	xtrax= 0.5f*cu->spacing-0.5f;

	oldvfont = NULL;

	for (i=0; i<slen; i++) cu->strinfo[i].flag &= ~CU_WRAP;

	if (selboxes) MEM_freeN(selboxes);
	selboxes = NULL;
	if (getselection(&selstart, &selend))
		selboxes = MEM_callocN((selend-selstart+1)*sizeof(SelBox), "font selboxes");

	tb = &(cu->tb[0]);
	curbox= 0;
	for (i = 0 ; i<=slen ; i++) {
	makebreak:
		ascii = cu->str[i];
		info = &(cu->strinfo[i]);
		vfont = which_vfont(cu, info);
    	if (vfont==0) return 0;
    	if (vfont != oldvfont) {
    		vfd= vfont_get_data(vfont);
    		oldvfont = vfont;
    	}
    	if (!vfd) return 0;    	
    	if((tb->w != 0.0) && (ct->dobreak==0) && ((xof-(tb->x/cu->fsize)+vfd->width[ascii])*cu->fsize) > tb->w) {
//     		fprintf(stderr, "linewidth exceeded: %c%c%c...\n", cu->str[i], cu->str[i+1], cu->str[i+2]);
    		for (j=i; j && (cu->str[j] != '\n') && (cu->str[j] != '\r') && (chartransdata[j].dobreak==0); j--) {
    			if (cu->str[j]==' ') {
					ct -= (i-(j-1));
					cnr -= (i-(j-1));
					i = j-1;
					xof = ct->xof;
					ct[1].dobreak = 1;
					cu->strinfo[i+1].flag |= CU_WRAP;
					goto makebreak;
    			}
    			if (chartransdata[j].dobreak) {
//    				fprintf(stderr, "word too long: %c%c%c...\n", cu->str[j], cu->str[j+1], cu->str[j+2]);
    				ct->dobreak= 1;
    				cu->strinfo[i+1].flag |= CU_WRAP;
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
			
			if ( (tb->h != 0.0) &&
			     ((-(yof-(tb->y/cu->fsize))) > ((tb->h/cu->fsize)-(linedist*cu->fsize))) &&
			     (cu->totbox > (curbox+1)) ) {
				maxlen= 0;
				tb++;
				curbox++;
				yof= cu->yof + tb->y/cu->fsize;
			}
			
			xof= cu->xof + (tb->x/cu->fsize);
			lnr++;
			cnr= 0;
		}
		else if(ascii==9) {	/* TAB */
			ct->xof= xof;
			ct->yof= yof;
			ct->linenr= lnr;
			ct->charnr= cnr++;

			tabfac= (xof-cu->xof+0.01f);
			tabfac= (float)(2.0*ceil(tabfac/2.0));
			xof= cu->xof+tabfac;
		}
		else {
			ct->xof= xof;
			ct->yof= yof;
			ct->linenr= lnr;
			ct->charnr= cnr++;

   			if (selboxes && (i>=selstart) && (i<=selend)) {
    			sb = &(selboxes[i-selstart]);
    			sb->y = yof*cu->fsize-linedist*cu->fsize*0.1;
    			sb->h = linedist*cu->fsize;
    			sb->w = xof*cu->fsize;
    		}
	
			if (ascii==32) wsfac = cu->wordspace; else wsfac = 1.0;
			xof += (vfd->width[ascii]*wsfac*(1.0+(info->kern/40.0)) ) + xtrax;
			
			if (selboxes && (i>=selstart) && (i<=selend)) sb->w = (xof*cu->fsize) - sb->w;
		}
		ct++;
	}
	

	
	cu->lines= 1;
	ct= chartransdata;
	for (i= 0; i<=slen; i++, mem++, ct++) {
		ascii = *mem;
		if(ascii== '\n' || ascii== '\r' || ct->dobreak) cu->lines++;
	}	

	// linedata is now: width of line
	// linedata2 is now: number of characters
	// linedata3 is now: maxlen of that line

	if(cu->spacemode!=CU_LEFT && lnr>1) {
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
		} else if((cu->spacemode==CU_FLUSH || cu->spacemode==CU_FORCEFLUSH) &&
		          (cu->tb[0].w != 0.0)) {
			for(i=0;i<lnr;i++)
				if(linedata2[i]>1)
					linedata[i]= (linedata3[i]-linedata[i])/(linedata2[i]-1);
			for (i=0; i<=slen; i++) {
				for (j=i; (cu->str[j]) && (cu->str[j]!='\n') && 
				          (cu->str[j]!='\r') && (chartransdata[j].dobreak==0) && (j<slen); j++);
				if ((cu->str[j]!='\r') && (cu->str[j]!='\n') && 
				    (cu->spacemode==CU_FORCEFLUSH || (chartransdata[j].dobreak!=0))) {
					ct->xof+= ct->charnr*linedata[ct->linenr];
				}
				ct++;
			}
		}
	}
	
	/* TEXT ON CURVE */
	if(cu->textoncurve) {
		cucu= cu->textoncurve->data;
		
		oldflag= cucu->flag;
		cucu->flag |= (CU_PATH+CU_FOLLOW);
		
		if(cucu->path==0) calc_curvepath(cu->textoncurve);
		if(cucu->path) {
			

			Mat3CpyMat4(cmat, cu->textoncurve->obmat);
			sizefac= Normalise(cmat[0])/cu->fsize;
			
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
				else if(cu->spacemode==CU_FLUSH || cu->spacemode==CU_FORCEFLUSH) distfac= 1.0f;
				
			}
			else distfac= 1.0;
			
			distfac/= (maxx-minx);
			
			timeofs+= distfac*cu->xof;	/* not cyclic */
			
			ct= chartransdata;
			for (i=0; i<=slen; i++, ct++) {
				
				/* rotate around centre character */
				ascii = cu->str[i];
				dtime= distfac*0.35f*vfd->width[ascii];	/* why not 0.5? */
				dtime= distfac*0.0f*vfd->width[ascii];	/* why not 0.5? */
				
				ctime= timeofs + distfac*( ct->xof - minx);
				CLAMP(ctime, 0.0, 1.0);

				/* calc the right loc AND the right rot separately */
				/* vec, tvec need 4 items */
				where_on_path(cu->textoncurve, ctime, vec, tvec);
				where_on_path(cu->textoncurve, ctime+dtime, tvec, rotvec);
				
				VecMulf(vec, sizefac);
				
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

	if (selboxes) {
		ct= chartransdata;
		for (i=0; i<=selend; i++, ct++) {
			if (i>=selstart) {
				selboxes[i-selstart].x = ct->xof*cu->fsize;
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
	if(ob==G.obedit) {
		ct= chartransdata+cu->pos;
		si= (float)sin(ct->rot);
		co= (float)cos(ct->rot);
				
		f= G.textcurs[0];
		
		f[0]= cu->fsize*(-0.1f*co + ct->xof);
		f[1]= cu->fsize*(0.1f*si + ct->yof);
		
		f[2]= cu->fsize*(0.1f*co + ct->xof);
		f[3]= cu->fsize*(-0.1f*si + ct->yof);
		
		f[4]= cu->fsize*( 0.1f*co + 0.8f*si + ct->xof);
		f[5]= cu->fsize*(-0.1f*si + 0.8f*co + ct->yof);
		
		f[6]= cu->fsize*(-0.1f*co + 0.8f*si + ct->xof);
		f[7]= cu->fsize*( 0.1f*si + 0.8f*co + ct->yof);
		
	}

	if (mode == FO_SELCHANGE) {
		MEM_freeN(chartransdata);
		MEM_freeN(linedata);
		MEM_freeN(linedata2);		
		MEM_freeN(linedata3);
		return NULL;
	}

	if(mode==0) {
		/* make nurbdata */
		
		freeNurblist(&cu->nurb);
		
		ct= chartransdata;
		if (cu->sepchar==0) {
    		for (i= 0; i<slen; i++) {
    			ascii = cu->str[i];
    			info = &(cu->strinfo[i]);
   				buildchar(cu, ascii, info, ct->xof, ct->yof, ct->rot, i);
    			ct++;
    		}
		}
		else {
    		outta = 0;
    		for (i= 0; (i<slen) && (outta==0); i++) {
    			ascii = cu->str[i];
    			info = &(cu->strinfo[i]);
				if (cu->sepchar == (i+1)) {
					cu->str[0] = ascii;
					cu->str[1] = 0;
					cu->strinfo[0]= *info;
					cu->pos = 1;
					cu->len = 1;
					vecyo[0] = ct->xof;
					vecyo[1] = ct->yof;
					vecyo[2] = 0;
					Mat4MulVecfl(ob->obmat, vecyo);
					VECCOPY(ob->loc, vecyo);
 					outta = 1;
 					cu->sepchar = 0;
				}
    			ct++;
    		}
    	}
	}

	MEM_freeN(linedata);
	MEM_freeN(linedata2);
	MEM_freeN(linedata3);	

	if(mode==FO_DUPLI) {
		return chartransdata;
	}

	MEM_freeN(chartransdata);
	return 0;
}


/* ***************** DUPLI  ***************** */

static Object *find_family_object(Object **obar, char *family, char ch)
{
	Object *ob;
	int flen;
	
	if( obar[ch] ) return obar[ch];
	
	flen= strlen(family);
	
	ob= G.main->object.first;
	while(ob) {
		if( ob->id.name[flen+2]==ch ) {
			if( strncmp(ob->id.name+2, family, flen)==0 ) break;
		}
		ob= ob->id.next;
	}
	
	obar[ch]= ob;
	
	return ob;
}


void font_duplilist(Object *par)
{
	extern ListBase duplilist;
	Object *ob, *newob, *obar[256];
	Curve *cu;
	struct chartrans *ct, *chartransdata;
	float vec[3], pmat[4][4], fsize, xof, yof;
	int slen, a;
	
	Mat4CpyMat4(pmat, par->obmat);

	/* in par the family name is stored, use this to find the other objects */

	chartransdata= text_to_curve(par, FO_DUPLI);
	if(chartransdata==0) return;
	
	memset(obar, 0, 256*4);
	
	cu= par->data;
	slen= strlen(cu->str);
	fsize= cu->fsize;
	xof= cu->xof;
	yof= cu->yof;
	
	ct= chartransdata;
	set_displist_onlyzero(1);

	for(a=0; a<slen; a++, ct++) {
	
		ob= find_family_object(obar, cu->family, cu->str[a]);
		if(ob) {
			
			makeDispList(ob);
			
			vec[0]= fsize*(ct->xof - xof);
			vec[1]= fsize*(ct->yof - yof);
			vec[2]= 0.0;
	
			Mat4MulVecfl(pmat, vec);
			
			newob= MEM_mallocN(sizeof(Object), "newobj dupli");
			memcpy(newob, ob, sizeof(Object));
			newob->flag |= OB_FROMDUPLI;
			newob->id.newid= (ID *)par;		/* keep duplicator */
			newob->totcol= par->totcol;	/* for give_current_material */
			
			Mat4CpyMat4(newob->obmat, par->obmat);
			VECCOPY(newob->obmat[3], vec);
			
			newob->parent= 0;
			newob->track= 0;
			
			BLI_addtail(&duplilist, newob);
		}
		
	}
	set_displist_onlyzero(0);
	MEM_freeN(chartransdata);
}
