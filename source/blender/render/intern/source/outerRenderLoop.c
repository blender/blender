/**  
 * The outer loop for rendering, but only for calling the
 * unified renderer :)
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

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"

#include "BKE_global.h"
#include "BKE_object.h"

#include "render.h"
#include "render_intern.h"
#include "outerRenderLoop.h"
#include "renderPreAndPost.h"
#include "vanillaRenderPipe.h"
#include "renderHelp.h"
#include "RE_callbacks.h"

extern short pa; /* can move to inside the outer loop */
/* should make a gamtab module, just like the jitter... */
extern unsigned short *mask1[9], *mask2[9], *igamtab2;


/* Parts bookkeeping: done on this level, because it seems appropriate to me.*/
static short partsCoordinates[65][4];
short  setPart(short nr);/* return 0 if this is a bad part. Sets data in R....*/
void   initParts(void);
void   addPartToRect(short nr, Part *part);
void addToBlurBuffer(int blur);
	
/* ------------------------------------------------------------------------- */
void addToBlurBuffer(int blur)
{
	static unsigned int *blurrect= 0;
	int tot, gamval;
	short facr, facb;
	char *rtr, *rtb;

	if(blur<0) {
		if(blurrect) {
			if(R.rectot) MEM_freeN(R.rectot);
			R.rectot= blurrect;
			blurrect= 0;
		}
	}
	else if(blur==R.osa-1) {
		/* eerste keer */
		blurrect= MEM_mallocN(R.rectx*R.recty*sizeof(int), "rectblur");
		if(R.rectot) memcpy(blurrect, R.rectot, R.rectx*R.recty*4);
	}
	else if(blurrect) {
		/* accumuleren */

		facr= 256/(R.osa-blur);
		facb= 256-facr;

		if(R.rectot) {
			rtr= (char *)R.rectot;
			rtb= (char *)blurrect;
			tot= R.rectx*R.recty;
			while(tot--) {
				if( *((unsigned int *)rtb) != *((unsigned int *)rtr) ) {

					if(R.r.mode & R_GAMMA) {
						gamval= (facr* igamtab2[ rtr[0]<<8 ] + facb* igamtab2[ rtb[0]<<8 ])>>8;
						rtb[0]= gamtab[ gamval ]>>8;
						gamval= (facr* igamtab2[ rtr[1]<<8 ] + facb* igamtab2[ rtb[1]<<8 ])>>8;
						rtb[1]= gamtab[ gamval ]>>8;
						gamval= (facr* igamtab2[ rtr[2]<<8 ] + facb* igamtab2[ rtb[2]<<8 ])>>8;
						rtb[2]= gamtab[ gamval ]>>8;
						gamval= (facr* igamtab2[ rtr[3]<<8 ] + facb* igamtab2[ rtb[3]<<8 ])>>8;
						rtb[3]= gamtab[ gamval ]>>8;
					}
					else {
						rtb[0]= (facr*rtr[0] + facb*rtb[0])>>8;
						rtb[1]= (facr*rtr[1] + facb*rtb[1])>>8;
						rtb[2]= (facr*rtr[2] + facb*rtb[2])>>8;
						rtb[3]= (facr*rtr[3] + facb*rtb[3])>>8;
					}
				}
				rtr+= 4;
				rtb+= 4;
			}
		}
		if(blur==0) {
			/* laatste keer */
			if(R.rectot) MEM_freeN(R.rectot);
			R.rectot= blurrect;
			blurrect= 0;
		}
	}
}


/* ------------------------------------------------------------------------- */

void addPartToRect(short nr, Part *part)
{
	unsigned int *rt, *rp;
	short y, heigth, len;

	/* de juiste offset in rectot */

	rt= R.rectot+ (partsCoordinates[nr][1]*R.rectx+ partsCoordinates[nr][0]);
	rp= part->rect;
	len= (partsCoordinates[nr][2]-partsCoordinates[nr][0]);
	heigth= (partsCoordinates[nr][3]-partsCoordinates[nr][1]);

	for(y=0;y<heigth;y++) {
		memcpy(rt, rp, 4*len);
		rt+=R.rectx;
		rp+= len;
	}
}

/* ------------------------------------------------------------------------- */

void initParts()
{
	short nr, xd, yd, xpart, ypart, xparts, yparts;
	short a, xminb, xmaxb, yminb, ymaxb;

	if(R.r.mode & R_BORDER) {
		xminb= R.r.border.xmin*R.rectx;
		xmaxb= R.r.border.xmax*R.rectx;

		yminb= R.r.border.ymin*R.recty;
		ymaxb= R.r.border.ymax*R.recty;

		if(xminb<0) xminb= 0;
		if(xmaxb>R.rectx) xmaxb= R.rectx;
		if(yminb<0) yminb= 0;
		if(ymaxb>R.recty) ymaxb= R.recty;
	}
	else {
		xminb=yminb= 0;
		xmaxb= R.rectx;
		ymaxb= R.recty;
	}

	xparts= R.r.xparts;	/* voor border */
	yparts= R.r.yparts;

	for(nr=0;nr<xparts*yparts;nr++)
		partsCoordinates[nr][0]= -1;	/* array leegmaken */

	xpart= R.rectx/xparts;
	ypart= R.recty/yparts;

	/* als border: testen of aantal parts minder kan */
	if(R.r.mode & R_BORDER) {
		a= (xmaxb-xminb-1)/xpart+1; /* zoveel parts in border */
		if(a<xparts) xparts= a;
		a= (ymaxb-yminb-1)/ypart+1; /* zoveel parts in border */
		if(a<yparts) yparts= a;

		xpart= (xmaxb-xminb)/xparts;
		ypart= (ymaxb-yminb)/yparts;
	}

	for(nr=0; nr<xparts*yparts; nr++) {

		if(R.r.mode & R_PANORAMA) {
			partsCoordinates[nr][0]= 0;
			partsCoordinates[nr][1]= 0;
			partsCoordinates[nr][2]= R.rectx;
			partsCoordinates[nr][3]= R.recty;
		}
		else {
			xd= (nr % xparts);
			yd= (nr-xd)/xparts;

			partsCoordinates[nr][0]= xminb+ xd*xpart;
			partsCoordinates[nr][1]= yminb+ yd*ypart;
			if(xd<R.r.xparts-1) partsCoordinates[nr][2]= partsCoordinates[nr][0]+xpart;
			else partsCoordinates[nr][2]= xmaxb;
			if(yd<R.r.yparts-1) partsCoordinates[nr][3]= partsCoordinates[nr][1]+ypart;
			else partsCoordinates[nr][3]= ymaxb;

			if(partsCoordinates[nr][2]-partsCoordinates[nr][0]<=0) partsCoordinates[nr][0]= -1;
			if(partsCoordinates[nr][3]-partsCoordinates[nr][1]<=0) partsCoordinates[nr][0]= -1;
		}
	}
}

short setPart(short nr)	/* return 0 als geen goede part */
{

	if(partsCoordinates[nr][0]== -1) return 0;

	R.xstart= partsCoordinates[nr][0]-R.afmx;
	R.ystart= partsCoordinates[nr][1]-R.afmy;
	R.xend= partsCoordinates[nr][2]-R.afmx;
	R.yend= partsCoordinates[nr][3]-R.afmy;
	R.rectx= R.xend-R.xstart;
	R.recty= R.yend-R.ystart;

	return 1;
}

/* ------------------------------------------------------------------------- */

void unifiedRenderingLoop(void)  /* hierbinnen de PART en FIELD lussen */
{
	Part *part;
	unsigned int *rt, *rt1, *rt2;
	int len;
	short blur, a,fields,fi,parts;  /* pa is globaal ivm print */
	unsigned int *border_buf= NULL;
	unsigned int border_x= 0;
	unsigned int border_y= 0;
	
	if((R.r.mode & R_BORDER) && !(R.r.mode & R_MOVIECROP)) {
		border_buf= R.rectot;
		border_x= R.rectx;
		border_y= R.recty;
		R.rectot= 0;
	}


	if (R.rectz) MEM_freeN(R.rectz);
	R.rectz = 0;

	/* FIELDLUS */
	fields= 1;
	parts= R.r.xparts*R.r.yparts;

	if(R.r.mode & R_FIELDS) {
		fields= 2;
		R.rectf1= R.rectf2= 0;	/* fieldrecten */
		R.r.ysch/= 2;
		R.afmy/= 2;
		R.r.yasp*= 2;
		R.ycor= ( (float)R.r.yasp)/( (float)R.r.xasp);

	}

	
	for(fi=0; fi<fields; fi++) {

		/* INIT */
		BLI_srand( 2*(G.scene->r.cfra)+fi);
	

		R.vlaknr= -1;
		R.flag|= R_RENDERING;
		if(fi==1) R.flag |= R_SEC_FIELD;
	

		/* MOTIONBLUR lus */
		if(R.r.mode & R_MBLUR) blur= R.osa;
		else blur= 1;

	
		while(blur--) {

			/* WINDOW */
			R.rectx= R.r.xsch;
			R.recty= R.r.ysch;
			R.xstart= -R.afmx;
			R.ystart= -R.afmy;
			R.xend= R.xstart+R.rectx-1;
			R.yend= R.ystart+R.recty-1;

	
			if(R.r.mode & R_MBLUR) set_mblur_offs(R.osa-blur);

			initParts(); /* altijd doen ivm border */
			setPart(0);
	
			RE_local_init_render_display();
			RE_local_clear_render_display(R.win);
			RE_local_timecursor((G.scene->r.cfra));

			prepareScene();
						
			/* PARTS */
			R.parts.first= R.parts.last= 0;
			for(pa=0; pa<parts; pa++) {
				
				if(RE_local_test_break()) break;
				
				if(pa) {	/* want pa==0 is al gedaan */
					if(setPart(pa)==0) break;
				}

				if(R.r.mode & R_MBLUR) RE_setwindowclip(0, blur);
				else RE_setwindowclip(0,-1);

				if(R.r.mode & R_PANORAMA) setPanoRot(pa);

				/* HOMOGENE COORDINATEN EN ZBUF EN CLIP OPT (per part) */
				/* There may be some interference with z-coordinate    */
				/* calculation here?                                   */

				doClipping(RE_projectverto);
				if(RE_local_test_break()) break;

				
				/* ZBUFFER & SHADE: zbuffer stores int distances, int face indices */
				R.rectot= (unsigned int *)MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");

				if(R.r.mode & R_MBLUR) RE_local_printrenderinfo(0.0, R.osa - blur);
				else RE_local_printrenderinfo(0.0, -1);

				/* The inner loop */
				zBufShadeAdvanced();
				
				if(RE_local_test_break()) break;
				
				/* uitzondering */
				if( (R.r.mode & R_BORDER) && (R.r.mode & R_MOVIECROP));
				else {
					/* PART OF BORDER AFHANDELEN */
					if(parts>1 || (R.r.mode & R_BORDER)) {
						
						part= MEM_callocN(sizeof(Part), "part");
						BLI_addtail(&R.parts, part);
						part->rect= R.rectot;
						R.rectot= 0;
						
						if (R.rectz) {
							MEM_freeN(R.rectz);
							R.rectz= 0;
						}
					}
				}
			}

			/* PARTS SAMENVOEGEN OF BORDER INVOEGEN */

			/* uitzondering: crop */
			if( (R.r.mode & R_BORDER) && (R.r.mode & R_MOVIECROP)) ;
			else {
				R.rectx= R.r.xsch;
				R.recty= R.r.ysch;

				if(R.r.mode & R_PANORAMA) R.rectx*= R.r.xparts;

				if(parts>1 || (R.r.mode & R_BORDER)) {
					if(R.rectot) MEM_freeN(R.rectot);
					if(R.r.mode & R_BORDER) {
						if(border_x<R.rectx || border_y<R.recty || border_buf==NULL)
							R.rectot= (unsigned int *)MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");
						else 
							R.rectot= border_buf;
					}
					else R.rectot=(unsigned int *)MEM_mallocN(sizeof(int)*R.rectx*R.recty, "rectot");
					
					part= R.parts.first;
					for(pa=0; pa<parts; pa++) {
						if(partsCoordinates[pa][0]== -1) break;
						if(part==0) break;
						
						if(R.r.mode & R_PANORAMA) {
							if(pa) {
								partsCoordinates[pa][0] += pa*R.r.xsch;
								partsCoordinates[pa][2] += pa*R.r.xsch;
							}
						}
						addPartToRect(pa, part);
						
						part= part->next;
					}
					
					part= R.parts.first;
					while(part) {
						MEM_freeN(part->rect);
						part= part->next;
					}
					BLI_freelistN(&R.parts);
				}
			}

			/* don't do this for unified renderer? */
/*  			if( (R.flag & R_HALO) && !G.magic) { */
/*  				add_halo_flare(); */
/*  			} */

			if(R.r.mode & R_MBLUR) {
				addToBlurBuffer(blur);
			}

			/* EINDE (blurlus) */
			finalizeScene();

			if(RE_local_test_break()) break;
		}

		/* definitief vrijgeven */
		addToBlurBuffer(-1);

		/* FIELD AFHANDELEN */
		if(R.r.mode & R_FIELDS) {
			if(R.flag & R_SEC_FIELD) R.rectf2= R.rectot;
			else R.rectf1= R.rectot;
			R.rectot= 0;
		}

		if(RE_local_test_break()) break;
	}

	/* FIELDS SAMENVOEGEN */
	if(R.r.mode & R_FIELDS) {
		R.r.ysch*= 2;
		R.afmy*= 2;
		R.recty*= 2;
		R.r.yasp/=2;

		if(R.rectot) MEM_freeN(R.rectot);	/* komt voor bij afbreek */
		R.rectot=(unsigned int *)MEM_mallocN(sizeof(int)*R.rectx*R.recty, "rectot");

		if(RE_local_test_break()==0) {
			rt= R.rectot;

			if(R.r.mode & R_ODDFIELD) {
				rt2= R.rectf1;
				rt1= R.rectf2;
			}
			else {
				rt1= R.rectf1;
				rt2= R.rectf2;
			}

			len= 4*R.rectx;

			for(a=0; a<R.recty; a+=2) {
				memcpy(rt, rt1, len);
				rt+= R.rectx;
				rt1+= R.rectx;
				memcpy(rt, rt2, len);
				rt+= R.rectx;
				rt2+= R.rectx;
			}
		}
	}

	/* R.rectx= R.r.xsch; */
	/* if(R.r.mode & R_PANORAMA) R.rectx*= R.r.xparts; */
	/* R.recty= R.r.ysch; */

	/* als border: wel de skybuf doen */
	/*
	  This may be tricky
	 */
/*  	if(R.r.mode & R_BORDER) { */
/*  		if( (R.r.mode & R_MOVIECROP)==0) { */
/*  			if(R.r.bufflag & 1) { */
/*  				R.xstart= -R.afmx; */
/*  				R.ystart= -R.afmy; */
/*  				rt= R.rectot; */
/*  				for(y=0; y<R.recty; y++, rt+= R.rectx) scanLineSkyFloat((char *)rt, y); */
/*  			} */
/*  		} */
/*  	} */

	set_mblur_offs(0);

	/* VRIJGEVEN */

	/* zbuf test */

	/* don't free R.rectz, only when its size is not the same as R.rectot */

	if (R.rectz && parts == 1 && (R.r.mode & R_FIELDS) == 0);
	else {
		if(R.rectz) MEM_freeN(R.rectz);
		R.rectz= 0;
	}

	if(R.rectf1) MEM_freeN(R.rectf1);
	R.rectf1= 0;
	if(R.rectf2) MEM_freeN(R.rectf2);
	R.rectf2= 0;
} /* End of void unifiedRenderingLoop()*/

/* ------------------------------------------------------------------------- */

/* eof */

