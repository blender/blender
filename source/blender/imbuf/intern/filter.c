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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * filter.c
 *
 * $Id$
 */

#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filter.h"


/************************************************************************/
/*				FILTERS					*/
/************************************************************************/

static void filtrow(unsigned char *point, int x)
{
	unsigned int c1,c2,c3,error;

	if (x>1){
		c1 = c2 = *point;
		error = 2;
		for(x--;x>0;x--){
			c3 = point[4];
			c1 += (c2<<1) + c3 + error;
			error = c1 & 3;
			*point = c1 >> 2;
			point += 4;
			c1=c2;
			c2=c3;
		}
		*point = (c1 + (c2<<1) + c2 + error) >> 2;
	}
}

static void filtrowf(float *point, int x)
{
	float c1,c2,c3;
	
	if (x>1){
		c1 = c2 = *point;
		for(x--;x>0;x--){
			c3 = point[4];
			c1 += (c2 * 2) + c3;
			*point = 0.25f*c1;
			point += 4;
			c1=c2;
			c2=c3;
		}
		*point = 0.25f*(c1 + (c2 * 2) + c2);
	}
}



static void filtcolum(unsigned char *point, int y, int skip)
{
	unsigned int c1,c2,c3,error;
	unsigned char *point2;

	if (y>1){
		c1 = c2 = *point;
		point2 = point;
		error = 2;
		for(y--;y>0;y--){
			point2 += skip;
			c3 = *point2;
			c1 += (c2<<1) + c3 +error;
			error = c1 & 3;
			*point = c1 >> 2;
			point=point2;
			c1=c2;
			c2=c3;
		}
		*point = (c1 + (c2<<1) + c2 + error) >> 2;
	}
}

static void filtcolumf(float *point, int y, int skip)
{
	float c1,c2,c3, *point2;
	
	if (y>1){
		c1 = c2 = *point;
		point2 = point;
		for(y--;y>0;y--){
			point2 += skip;
			c3 = *point2;
			c1 += (c2 * 2) + c3;
			*point = 0.25f*c1;
			point=point2;
			c1=c2;
			c2=c3;
		}
		*point = 0.25f*(c1 + (c2 * 2) + c2);
	}
}

void IMB_filtery(struct ImBuf *ibuf)
{
	unsigned char *point;
	float *pointf;
	int x, y, skip;

	point = (unsigned char *)ibuf->rect;
	pointf = ibuf->rect_float;

	x = ibuf->x;
	y = ibuf->y;
	skip = x<<2;

	for (;x>0;x--){
		if (point) {
			if (ibuf->depth > 24) filtcolum(point,y,skip);
			point++;
			filtcolum(point,y,skip);
			point++;
			filtcolum(point,y,skip);
			point++;
			filtcolum(point,y,skip);
			point++;
		}
		if (pointf) {
			if (ibuf->depth > 24) filtcolumf(pointf,y,skip);
			pointf++;
			filtcolumf(pointf,y,skip);
			pointf++;
			filtcolumf(pointf,y,skip);
			pointf++;
			filtcolumf(pointf,y,skip);
			pointf++;
		}
	}
}


void imb_filterx(struct ImBuf *ibuf)
{
	unsigned char *point;
	float *pointf;
	int x, y, skip;

	point = (unsigned char *)ibuf->rect;
	pointf = ibuf->rect_float;

	x = ibuf->x;
	y = ibuf->y;
	skip = (x<<2) - 3;

	for (;y>0;y--){
		if (point) {
			if (ibuf->depth > 24) filtrow(point,x);
			point++;
			filtrow(point,x);
			point++;
			filtrow(point,x);
			point++;
			filtrow(point,x);
			point+=skip;
		}
		if (pointf) {
			if (ibuf->depth > 24) filtrowf(pointf,x);
			pointf++;
			filtrowf(pointf,x);
			pointf++;
			filtrowf(pointf,x);
			pointf++;
			filtrowf(pointf,x);
			pointf+=skip;
		}
	}
}

void IMB_filterN(ImBuf *out, ImBuf *in)
{
	register char *row1, *row2, *row3;
	register char *cp, *r11, *r13, *r21, *r23, *r31, *r33;
	int rowlen, x, y;
	
	rowlen= in->x;
	
	for(y=0; y<in->y; y++) {
		/* setup rows */
		row2= (char*)(in->rect + y*rowlen);
		row1= (y == 0)? row2: row2 - 4*rowlen;
		row3= (y == in->y-1)? row2: row2 + 4*rowlen;
		
		cp= (char *)(out->rect + y*rowlen);
		
		for(x=0; x<rowlen; x++) {
			if(x == 0) {
				r11 = row1;
				r21 = row1;
				r31 = row1;
			}
			else {
				r11 = row1-4;
				r21 = row1-4;
				r31 = row1-4;
			}

			if(x == rowlen-1) {
				r13 = row1;
				r23 = row1;
				r33 = row1;
			}
			else {
				r13 = row1+4;
				r23 = row1+4;
				r33 = row1+4;
			}

			cp[0]= (r11[0] + 2*row1[0] + r13[0] + 2*r21[0] + 4*row2[0] + 2*r23[0] + r31[0] + 2*row3[0] + r33[0])>>4;
			cp[1]= (r11[1] + 2*row1[1] + r13[1] + 2*r21[1] + 4*row2[1] + 2*r23[1] + r31[1] + 2*row3[1] + r33[1])>>4;
			cp[2]= (r11[2] + 2*row1[2] + r13[2] + 2*r21[2] + 4*row2[2] + 2*r23[2] + r31[2] + 2*row3[2] + r33[2])>>4;
			cp[3]= (r11[3] + 2*row1[3] + r13[3] + 2*r21[3] + 4*row2[3] + 2*r23[3] + r31[3] + 2*row3[3] + r33[3])>>4;
			cp+=4; row1+=4; row2+=4; row3+=4;
		}
	}
}

void IMB_filter(struct ImBuf *ibuf)
{
	IMB_filtery(ibuf);
	imb_filterx(ibuf);
}

#define EXTEND_PIXEL(color, w) if((color)[3]) {r+= w*(color)[0]; g+= w*(color)[1]; b+= w*(color)[2]; a+= w*(color)[3]; tot+=w;}

/* if alpha is zero, it checks surrounding pixels and averages color. sets new alphas to 1.0
 * 
 * When a mask is given, only effect pixels with a mask value of 1, defined as BAKE_MASK_MARGIN in rendercore.c
 * */
void IMB_filter_extend(struct ImBuf *ibuf, char *mask)
{
	register char *row1, *row2, *row3;
	register char *cp; 
	int rowlen, x, y;
	
	rowlen= ibuf->x;
	
	
	if (ibuf->rect_float) {
		float *temprect;
		float *row1f, *row2f, *row3f;
		float *fp;
		temprect= MEM_dupallocN(ibuf->rect_float);
		
		for(y=1; y<=ibuf->y; y++) {
			/* setup rows */
			row1f= (float *)(temprect + (y-2)*rowlen*4);
			row2f= row1f + 4*rowlen;
			row3f= row2f + 4*rowlen;
			if(y==1)
				row1f= row2f;
			else if(y==ibuf->y)
				row3f= row2f;
			
			fp= (float *)(ibuf->rect_float + (y-1)*rowlen*4);
				
			for(x=0; x<rowlen; x++) {
				if((mask==NULL && fp[3]==0.0f) || (mask && mask[((y-1)*rowlen)+x]==1)) {
					int tot= 0;
					float r=0.0f, g=0.0f, b=0.0f, a=0.0f;
					
					EXTEND_PIXEL(row1f, 1);
					EXTEND_PIXEL(row2f, 2);
					EXTEND_PIXEL(row3f, 1);
					EXTEND_PIXEL(row1f+4, 2);
					EXTEND_PIXEL(row3f+4, 2);
					if(x!=rowlen-1) {
						EXTEND_PIXEL(row1f+8, 1);
						EXTEND_PIXEL(row2f+8, 2);
						EXTEND_PIXEL(row3f+8, 1);
					}					
					if(tot) {
						fp[0]= r/tot;
						fp[1]= g/tot;
						fp[2]= b/tot;
						fp[3]= a/tot;
					}
				}
				fp+=4; 
				
				if(x!=0) {
					row1f+=4; row2f+=4; row3f+=4;
				}
			}
		}

		MEM_freeN(temprect);
	}
	else if(ibuf->rect) {
		int *temprect;
		
		/* make a copy, to prevent flooding */
		temprect= MEM_dupallocN(ibuf->rect);
		
		for(y=1; y<=ibuf->y; y++) {
			/* setup rows */
			row1= (char *)(temprect + (y-2)*rowlen);
			row2= row1 + 4*rowlen;
			row3= row2 + 4*rowlen;
			if(y==1)
				row1= row2;
			else if(y==ibuf->y)
				row3= row2;
			
			cp= (char *)(ibuf->rect + (y-1)*rowlen);
			
			for(x=0; x<rowlen; x++) {
				/*if(cp[3]==0) {*/
				if((mask==NULL && cp[3]==0) || (mask && mask[((y-1)*rowlen)+x]==1)) {
					int tot= 0, r=0, g=0, b=0, a=0;
					
					EXTEND_PIXEL(row1, 1);
					EXTEND_PIXEL(row2, 2);
					EXTEND_PIXEL(row3, 1);
					EXTEND_PIXEL(row1+4, 2);
					EXTEND_PIXEL(row3+4, 2);
					if(x!=rowlen-1) {
						EXTEND_PIXEL(row1+8, 1);
						EXTEND_PIXEL(row2+8, 2);
						EXTEND_PIXEL(row3+8, 1);
					}					
					if(tot) {
						cp[0]= r/tot;
						cp[1]= g/tot;
						cp[2]= b/tot;
						cp[3]= a/tot;
					}
				}
				cp+=4;
				
				if(x!=0) {
					row1+=4; row2+=4; row3+=4;
				}
			}
		}
		
		MEM_freeN(temprect);
	}
}

#if 0
void IMB_makemipmap(ImBuf *ibuf, int use_filter)
{
	ImBuf *hbuf= ibuf;
	int minsize, curmap=0;
	
	minsize= ibuf->x<ibuf->y?ibuf->x:ibuf->y;
	
	while(minsize>10 && curmap<IB_MIPMAP_LEVELS) {
		if(use_filter) {
			ImBuf *nbuf= IMB_allocImBuf(hbuf->x, hbuf->y, 32, IB_rect, 0);
			IMB_filterN(nbuf, hbuf);
			ibuf->mipmap[curmap]= IMB_onehalf(nbuf);
			IMB_freeImBuf(nbuf);
		}
		else {
			ibuf->mipmap[curmap]= IMB_onehalf(hbuf);
		}
		hbuf= ibuf->mipmap[curmap];
		
		curmap++;
		minsize= hbuf->x<hbuf->y?hbuf->x:hbuf->y;
	}
}
#endif

void IMB_makemipmap(ImBuf *ibuf, int use_filter, int SAT)
{
	if (SAT) {
		// to maximize precision subtract image average, use intermediate double SAT,
		// only convert to float at the end
		const double dv = 1.0/255.0;
		double avg[4] = {0, 0, 0, 0};
		const int x4 = ibuf->x << 2;
		int x, y, i;
		ImBuf* sbuf = IMB_allocImBuf(ibuf->x, ibuf->y, 32, IB_rectfloat, 0);
		double *satp, *satbuf = MEM_callocN(sizeof(double)*ibuf->x*ibuf->y*4, "tmp SAT buf");
		const double mf = ibuf->x*ibuf->y;
		float* fp;
		ibuf->mipmap[0] = sbuf;
		if (ibuf->rect_float) {
			fp = ibuf->rect_float;
			for (y=0; y<ibuf->y; ++y)
				for (x=0; x<ibuf->x; ++x) {
					avg[0] += *fp++;
					avg[1] += *fp++;
					avg[2] += *fp++;
					avg[3] += *fp++;
				}
		}
		else {
			char* cp = (char*)ibuf->rect;
			for (y=0; y<ibuf->y; ++y)
				for (x=0; x<ibuf->x; ++x) {
					avg[0] += *cp++ * dv;
					avg[1] += *cp++ * dv;
					avg[2] += *cp++ * dv;
					avg[3] += *cp++ * dv;
				}
		}
		avg[0] /= mf;
		avg[1] /= mf;
		avg[2] /= mf;
		avg[3] /= mf;
		for (y=0; y<ibuf->y; ++y)
			for (x=0; x<ibuf->x; ++x) {
				const unsigned int p = (x + y*ibuf->x) << 2;
				char* cp = (char*)ibuf->rect + p;
				fp = ibuf->rect_float + p;
				satp = satbuf + p;
				for (i=0; i<4; ++i, ++cp, ++fp, ++satp) {
					double sv = (ibuf->rect_float ? (double)*fp : (double)(*cp)*dv) - avg[i];
					if (x > 0) sv += satp[-4];
					if (y > 0) sv += satp[-x4];
					if (x > 0 && y > 0) sv -= satp[-x4 - 4];
					*satp = sv;
				}
			}
		fp = sbuf->rect_float;
		satp = satbuf;
		for (y=0; y<ibuf->y; ++y)
			for (x=0; x<ibuf->x; ++x) {
				*fp++ = (float)*satp++;
				*fp++ = (float)*satp++;
				*fp++ = (float)*satp++;
				*fp++ = (float)*satp++;
			}
		MEM_freeN(satbuf);
		fp = &sbuf->rect_float[(sbuf->x - 1 + (sbuf->y - 1)*sbuf->x) << 2];
		fp[0] = avg[0];
		fp[1] = avg[1];
		fp[2] = avg[2];
		fp[3] = avg[3];
	}
	else {
		ImBuf *hbuf = ibuf;
		int curmap = 0;
		while (curmap < IB_MIPMAP_LEVELS) {
			if (use_filter) {
				ImBuf *nbuf= IMB_allocImBuf(hbuf->x, hbuf->y, 32, IB_rect, 0);
				IMB_filterN(nbuf, hbuf);
				ibuf->mipmap[curmap] = IMB_onehalf(nbuf);
				IMB_freeImBuf(nbuf);
			}
			else ibuf->mipmap[curmap] = IMB_onehalf(hbuf);
			hbuf = ibuf->mipmap[curmap];
			if (hbuf->x == 1 && hbuf->y == 1) break;
			curmap++;
		}
	}
}
