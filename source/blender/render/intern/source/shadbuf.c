/*  shadbuf.c        RENDER
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
 * 
 *  april 95
 * 
 * $Id$
 *
 * 27-Jun-2001 switched the shadow buffer for UR to the object-shadow
 * buffers, and removed all references and fixes for UR rendering from
 * this one.
 * */

#include <math.h>
#include <string.h>

#include "MTC_matrixops.h"
#include "MEM_guardedalloc.h"
#include "BLI_arithb.h"

#include "DNA_lamp_types.h"

#include "render.h"
#include "render_intern.h"

#include "shadbuf.h"
#include "renderHelp.h"
#include "jitter.h"
#include "zbuf.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* if defined: objects don't cast shadows anymore                            */
/*  #define RE_NO_SHADOWS */

/* unused? */
static int bias= 0x00500000;
/* crud */
#define MIN2(x,y)               ( (x)<(y) ? (x) : (y) )

/* ------------------------------------------------------------------------- */

void lrectreadRectz(int x1, int y1, int x2, int y2, char *r1);
int sizeoflampbuf(struct ShadBuf *shb);
int firstreadshadbuf(struct ShadBuf *shb, int xs, int ys, int nr);
float readshadowbuf(struct ShadBuf *shb, int xs, int ys, int zs);
float readshadowbuf_halo(struct ShadBuf *shb, int xs, int ys, int zs);
float *give_jitter_tab(int samp);
/* ------------------------------------------------------------------------- */


void initshadowbuf(LampRen *lar, float mat[][4])
{
	struct ShadBuf *shb;
	float hoek, temp, viewinv[4][4];

	/* if(la->spsi<16) return; */

	/* memory reservation */
	shb= (struct ShadBuf *)MEM_callocN( sizeof(struct ShadBuf),"initshadbuf");
	lar->shb= shb;

	if(shb==0) return;

	VECCOPY(shb->co, lar->co);
	
	/* percentage render: keep track of min and max */
	shb->size= (lar->bufsize*R.r.size)/100;
	if(shb->size<512) shb->size= 512;
	else if(shb->size > lar->bufsize) shb->size= lar->bufsize;

	shb->samp= lar->samp;
	shb->soft= lar->soft;
	shb->shadhalostep= lar->shadhalostep;
	
	shb->zbuf= (unsigned long *)MEM_mallocN( sizeof(unsigned long)*(shb->size*shb->size)/256, "initshadbuf2");
	shb->cbuf= (char *)MEM_callocN( (shb->size*shb->size)/256, "initshadbuf3");

	if(shb->zbuf==0 || shb->cbuf==0) {
		if(shb->zbuf) MEM_freeN(shb->zbuf);
		MEM_freeN(lar->shb);
		lar->shb= 0;		
		return;
	}

	MTC_Mat4Ortho(mat);
	MTC_Mat4Invert(shb->winmat, mat);	/* winmat is temp */

	/* matrix: combination of inverse view and lampmat */
	/* calculate again: the ortho-render has no correct viewinv */
	MTC_Mat4Invert(viewinv, R.viewmat);
	MTC_Mat4MulMat4(shb->viewmat, viewinv, shb->winmat);

	/* projection */
	hoek= saacos(lar->spotsi);
	temp= 0.5*shb->size*cos(hoek)/sin(hoek);
	shb->d= lar->clipsta;

	shb->pixsize= (shb->d)/temp;

	shb->far= lar->clipend;
	/* bias is percentage, made 2x karger because of correction for angle of incidence */
	/* when a ray is closer to parallel of a face, bias value is increased during render */
	shb->bias= (0.02*lar->bias)*0x7FFFFFFF;
	shb->bias= shb->bias*(100/R.r.size);
	
}
/* ------------------------------------------------------------------------- */


void lrectreadRectz(int x1, int y1, int x2, int y2, char *r1) /* reads part from rectz in r1 */
{
	unsigned int len4, *rz;	

	if(x1>=R.rectx || x2>=R.rectx || y1>=R.recty || y2>=R.recty) return;
	if(x1>x2 || y1>y2) return;

	len4= 4*(x2- x1+1);
	rz= R.rectz+R.rectx*y1+x1;
	for(;y1<=y2;y1++) {
		memcpy(r1,rz,len4);
		rz+= R.rectx;
		r1+= len4;
	}
}


int sizeoflampbuf(struct ShadBuf *shb)
{
	int num,count=0;
	char *cp;
	
	cp= shb->cbuf;
	num= (shb->size*shb->size)/256;

	while(num--) count+= *(cp++);
	
	return 256*count;
}

float *give_jitter_tab(int samp)
{
	/* these are all possible jitter tables, takes up some
	 * 12k, not really bad!
	 * For soft shadows, it saves memory and render time
	 */
	static int tab[17]={1, 4, 9, 16, 25, 36, 49, 64, 81, 100, 121, 144, 169, 196, 225, 256};
	static float jit[1496][2];
	static char ctab[17]= {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int a, offset=0;
	
	if(samp<2) samp= 2;
	else if(samp>16) samp= 16;

	for(a=0; a<samp-1; a++) offset+= tab[a];

	if(ctab[samp]==0) {
		initjit(jit[offset], samp*samp);
		ctab[samp]= 1;
	}
		
	return jit[offset];
	
}

void makeshadowbuf(LampRen *lar)
{
	struct ShadBuf *shb= lar->shb;
	float panophi;
	float temp, wsize, dist;
	int *rz, *rz1, verg, verg1;
	unsigned long *ztile;
	int a, x, y, minx, miny, byt1, byt2;
	short temprx,tempry, square;
	char *rc, *rcline, *ctile, *zt;

	panophi = getPanoPhi();
	
	/* store viewvars */
	temprx= R.rectx; tempry= R.recty;
	R.rectx= R.recty= shb->size;

	shb->jit= give_jitter_tab(shb->samp);

	/* matrices and window: in R.winmat the transformation is being put,
		transforming from observer view to lamp view, including lamp window matrix */
	
	wsize= shb->pixsize*(shb->size/2.0);

	i_window(-wsize, wsize, -wsize, wsize, shb->d, shb->far, shb->winmat);

	MTC_Mat4MulMat4(shb->persmat, shb->viewmat, shb->winmat);
	
	/* temp, will be restored */
	MTC_Mat4SwapMat4(shb->persmat, R.winmat);

	/* zbuffering */
	if(R.rectz) MEM_freeN(R.rectz);
 	R.rectz= (unsigned int *)MEM_mallocN(sizeof(int)*shb->size*shb->size,"makeshadbuf");
	rcline= MEM_mallocN(256*4+sizeof(int),"makeshadbuf2");

	/* store: panorama rot */
	temp= panophi;
	panophi= 0.0;
	pushTempPanoPhi(0.0);

	/* pano interference here? */
	setzbufvlaggen(projectvert);

	popTempPanoPhi();
	panophi= temp;
	
	zbuffershad(lar);
	
	square= lar->mode & LA_SQUARE;

	/* create Z tiles (for compression): this system is 24 bits!!! */
	
	ztile= shb->zbuf;
	ctile= shb->cbuf;
	for(y=0; y<shb->size; y+=16) {
		if(y< shb->size/2) miny= y+15-shb->size/2;
		else miny= y-shb->size/2;	
				
		for(x=0; x<shb->size; x+=16) {

			/* is tile within spotbundle? */
			a= shb->size/2;
			if(x< a) minx= x+15-a;
			else minx= x-a;	
			
			dist= sqrt( (float)(minx*minx+miny*miny) );

			if(square==0 && dist>(float)(a+12)) {	/* 12, tested with a onlyshadow lamp */
				a= 256; verg= 0; /* 0x80000000; */ /* 0x7FFFFFFF; */
				rz1= (&verg)+1;
			} 
			else {
				lrectreadRectz(x, y, MIN2(shb->size-1,x+15), MIN2(shb->size-1,y+15), rcline);
				rz1= (int *)rcline;
				
				verg= (*rz1 & 0xFFFFFF00);

				for(a=0;a<256;a++,rz1++) {
					if( (*rz1 & 0xFFFFFF00) !=verg) break;
				}
			}
			if(a==256) { /* complete empty tile */
				*ctile= 0;
				*ztile= *(rz1-1);
			}
			else {

				/* ACOMP etc. are defined to work L/B endian */
				
				rc= rcline;
				rz1= (int *)rcline;
				verg=  rc[ACOMP];
				verg1= rc[BCOMP];
				rc+= 4;
				byt1= 1; byt2= 1;
				for(a=1;a<256;a++,rc+=4) {
					byt1 &= (verg==rc[ACOMP]);
					byt2 &= (verg1==rc[BCOMP]);

					if(byt1==0) break;
				}
				if(byt1 && byt2) {	/* only store byte */
					*ctile= 1;
					*ztile= (unsigned long)MEM_mallocN(256+4, "tile1");
					rz= (int *)*ztile;
					*rz= *rz1;

					zt= (char *)(rz+1);
					rc= rcline;
					for(a=0; a<256; a++, zt++, rc+=4) *zt= rc[GCOMP];	
				}
				else if(byt1) {		/* only store short */
					*ctile= 2;
					*ztile= (unsigned long)MEM_mallocN(2*256+4,"Tile2");
					rz= (int *)*ztile;
					*rz= *rz1;

					zt= (char *)(rz+1);
					rc= rcline;
					for(a=0; a<256; a++, zt+=2, rc+=4) {
						zt[0]= rc[BCOMP];
						zt[1]= rc[GCOMP];
					}
				}
				else {			/* store triple */
					*ctile= 3;
					*ztile= (unsigned long)MEM_mallocN(3*256,"Tile3");

					zt= (char *)*ztile;
					rc= rcline;
					for(a=0; a<256; a++, zt+=3, rc+=4) {
						zt[0]= rc[ACOMP];
						zt[1]= rc[BCOMP];
						zt[2]= rc[GCOMP];
					}
				}
			}
			ztile++;
			ctile++;
		}
	}

	MEM_freeN(rcline);
	MEM_freeN(R.rectz); R.rectz= 0;

	R.rectx= temprx; R.recty= tempry;
	MTC_Mat4SwapMat4(shb->persmat, R.winmat);

	/* printf("lampbuf %d\n", sizeoflampbuf(shb)); */
}

int firstreadshadbuf(struct ShadBuf *shb, int xs, int ys, int nr)
{
	/* return a 1 if fully compressed shadbuf-tile && z==const */
	static int *rz;
	int ofs;
	char *ct;

	/* always test borders of shadowbuffer */
	if(xs<0) xs= 0; else if(xs>=shb->size) xs= shb->size-1;
	if(ys<0) ys= 0; else if(ys>=shb->size) ys= shb->size-1;
   
	/* calc z */
	ofs= (ys>>4)*(shb->size>>4) + (xs>>4);
	ct= shb->cbuf+ofs;
	if(*ct==0) {
	    if(nr==0) {
			rz= *( (int **)(shb->zbuf+ofs) );
			return 1;
	    }
		else if(rz!= *( (int **)(shb->zbuf+ofs) )) return 0;
		
	    return 1;
	}
	
	return 0;
}

float readshadowbuf(struct ShadBuf *shb, int xs, int ys, int zs)	/* return 1.0 : fully in light */
{
	float temp;
	int *rz, ofs;
	int zsamp=0;
	char *ct, *cz;

	/* simpleclip */
	/* if(xs<0 || ys<0) return 1.0; */
	/* if(xs>=shb->size || ys>=shb->size) return 1.0; */
	
	/* always test borders of shadowbuffer */
	if(xs<0) xs= 0; else if(xs>=shb->size) xs= shb->size-1;
	if(ys<0) ys= 0; else if(ys>=shb->size) ys= shb->size-1;

	/* calc z */
	ofs= (ys>>4)*(shb->size>>4) + (xs>>4);
	ct= shb->cbuf+ofs;
	rz= *( (int **)(shb->zbuf+ofs) );

	if(*ct==3) {
		ct= ((char *)rz)+3*16*(ys & 15)+3*(xs & 15);
		cz= (char *)&zsamp;
		cz[ACOMP]= ct[0];
		cz[BCOMP]= ct[1];
		cz[GCOMP]= ct[2];
	}
	else if(*ct==2) {
		ct= ((char *)rz);
		ct+= 4+2*16*(ys & 15)+2*(xs & 15);
		zsamp= *rz;
	
		cz= (char *)&zsamp;
		cz[BCOMP]= ct[0];
		cz[GCOMP]= ct[1];
	}
	else if(*ct==1) {
		ct= ((char *)rz);
		ct+= 4+16*(ys & 15)+(xs & 15);
		zsamp= *rz;

		cz= (char *)&zsamp;
		cz[GCOMP]= ct[0];

	}
	else {
		/* got warning on this from DEC alpha (64 bits).... */
		/* but it's working code! (ton) */
 		zsamp= (int) rz;
	}

	/* if(zsamp >= 0x7FFFFE00) return 1.0; */	/* no shaduw when sampling at eternal distance */

	if(zsamp > zs) return 1.0; 		/* absolute no shadow */
	else if( zsamp < zs-bias) return 0.0 ;	/* absolute in shadow */
	else {					/* soft area */
		
		temp=  ( (float)(zs- zsamp) )/(float)bias;
		return 1.0 - temp*temp;
			
	}
}


float testshadowbuf(struct ShadBuf *shb, float inp)  	/* return 1.0: no shadow at all */
{
	float fac, co[4], dx[3], dy[3], aantal=0;
	float xs1,ys1, siz, *j, xres, yres;
	int xs,ys, zs;
	short a,num;
	
#ifdef RE_NO_SHADOWS
	return 1.0;
#endif
	
	/* if(inp <= 0.0) return 1.0; */

	/* rotate renderco en osaco */
	siz= 0.5*(float)shb->size;
	VECCOPY(co, R.co);
	co[3]= 1.0;

	MTC_Mat4MulVec4fl(shb->persmat, co);	/* rational hom co */

	xs1= siz*(1.0+co[0]/co[3]);
	ys1= siz*(1.0+co[1]/co[3]);

	/* Clip for z: near and far clip values of the shadow buffer. We
		* can test for -1.0/1.0 because of the properties of the
		* coordinate transformations. */
	fac= (co[2]/co[3]);

	if(fac>=1.0) {
		return 0.0;
	} else if(fac<= -1.0) {
		return 1.0;
	}

	zs= ((float)0x7FFFFFFF)*fac;

	/* take num*num samples, increase area with fac */
	num= shb->samp*shb->samp;
	fac= shb->soft;
	
	
	bias= (1.1-inp*inp)*shb->bias;

	if(num==1) {
		return readshadowbuf(shb,(int)xs1, (int)ys1, zs);
	}

	co[0]= R.co[0]+O.dxco[0];
	co[1]= R.co[1]+O.dxco[1];
	co[2]= R.co[2]+O.dxco[2];
	co[3]= 1.0;
	MTC_Mat4MulVec4fl(shb->persmat,co);	/* rational hom co */
	dx[0]= xs1- siz*(1.0+co[0]/co[3]);
	dx[1]= ys1- siz*(1.0+co[1]/co[3]);

	co[0]= R.co[0]+O.dyco[0];
	co[1]= R.co[1]+O.dyco[1];
	co[2]= R.co[2]+O.dyco[2];
	co[3]= 1.0;
	MTC_Mat4MulVec4fl(shb->persmat,co);	/* rational hom co */
	dy[0]= xs1- siz*(1.0+co[0]/co[3]);
	dy[1]= ys1- siz*(1.0+co[1]/co[3]);

	xres= fac*( fabs(dx[0])+fabs(dy[0]) );
	yres= fac*( fabs(dx[1])+fabs(dy[1]) );

	if(xres<fac) xres= fac;
	if(yres<fac) yres= fac;
	
	xs1-= (xres)/2;
	ys1-= (yres)/2;

	j= shb->jit;

	if(xres<16.0 && yres<16.0) {
	    if(firstreadshadbuf(shb, (int)xs1, (int)ys1, 0)) {
			if(firstreadshadbuf(shb, (int)(xs1+xres), (int)ys1, 1)) {
				if(firstreadshadbuf(shb, (int)xs1, (int)(ys1+yres), 1)) {
					if(firstreadshadbuf(shb, (int)(xs1+xres), (int)(ys1+yres), 1)) {
						return readshadowbuf(shb,(int)xs1, (int)ys1, zs);
					}
				}
			}
	    }
	}

	for(a=num;a>0;a--) {
		/* instead of jit i tried random: ugly! */
		xs= xs1 + xres*j[0];
		ys= ys1 + yres*j[1];
		j+=2;
		
		aantal+= readshadowbuf(shb, xs, ys, zs);
	}

	/* Renormalizes for the sample number: */
	return aantal/( (float)(num) );
}

/* different function... sampling behind clipend can be LIGHT, bias is negative! */
/* return: light */
float readshadowbuf_halo(struct ShadBuf *shb, int xs, int ys, int zs)
{
	float temp;
	int *rz, ofs;
	int zbias, zsamp;
	char *ct, *cz;

	/* simpleclip */
	if(xs<0 || ys<0) return 0.0;
	if(xs>=shb->size || ys>=shb->size) return 0.0;

	/* calc z */
	ofs= (ys>>4)*(shb->size>>4) + (xs>>4);
	ct= shb->cbuf+ofs;
	rz= *( (int **)(shb->zbuf+ofs) );

	if(*ct==3) {
		ct= ((char *)rz)+3*16*(ys & 15)+3*(xs & 15);
		cz= (char *)&zsamp;
		zsamp= 0;
		cz[ACOMP]= ct[0];
		cz[BCOMP]= ct[1];
		cz[GCOMP]= ct[2];
	}
	else if(*ct==2) {
		ct= ((char *)rz);
		ct+= 4+2*16*(ys & 15)+2*(xs & 15);
		zsamp= *rz;
	
		cz= (char *)&zsamp;
		cz[BCOMP]= ct[0];
		cz[GCOMP]= ct[1];
	}
	else if(*ct==1) {
		ct= ((char *)rz);
		ct+= 4+16*(ys & 15)+(xs & 15);
		zsamp= *rz;

		cz= (char *)&zsamp;
		cz[GCOMP]= ct[0];

	}
	else {
		/* same as before */
		/* still working code! (ton) */
 		zsamp= (int) rz;
	}

	/* NO schadow when sampled at 'eternal' distance */

	if(zsamp >= 0x7FFFFE00) return 1.0; 

	if(zsamp > zs) return 1.0; 		/* absolute no shadww */
	else {
		/* bias is negative, so the (zs-bias) can be beyond 0x7fffffff */
		zbias= 0x7fffffff - zs;
		if(zbias > -bias) {
			if( zsamp < zs-bias) return 0.0 ;	/* absolute in shadow */
		}
		else return 0.0 ;	/* absolute shadow */
	}

	/* soft area */
	
	temp=  ( (float)(zs- zsamp) )/(float)bias;
	return 1.0 - temp*temp;
}


float shadow_halo(LampRen *lar, float *p1, float *p2)
{
	/* p1 p2 already are rotated in spot-space */
	ShadBuf *shb= lar->shb;
	float co[4], siz;
	float labda, labdao, labdax, labday, ldx, ldy;
	float zf, xf1, yf1, zf1, xf2, yf2, zf2;
	float count, lightcount;
	int x, y, z, xs1, ys1;
	int dx = 0, dy = 0;
	
	siz= 0.5*(float)shb->size;
	/* negative! The other side is more important */
	bias= -shb->bias;
	
	co[0]= p1[0];
	co[1]= p1[1];
	co[2]= p1[2]/lar->sh_zfac;
	co[3]= 1.0;
	MTC_Mat4MulVec4fl(shb->winmat, co);	/* rational hom co */
	xf1= siz*(1.0+co[0]/co[3]);
	yf1= siz*(1.0+co[1]/co[3]);
	zf1= (co[2]/co[3]);


	co[0]= p2[0];
	co[1]= p2[1];
	co[2]= p2[2]/lar->sh_zfac;
	co[3]= 1.0;
	MTC_Mat4MulVec4fl(shb->winmat, co);	/* rational hom co */
	xf2= siz*(1.0+co[0]/co[3]);
	yf2= siz*(1.0+co[1]/co[3]);
	zf2= (co[2]/co[3]);

	/* the 2dda (a pixel line formula) */

	xs1= (int)xf1;
	ys1= (int)yf1;

	if(xf1 != xf2) {
		if(xf2-xf1 > 0.0) {
			labdax= (xf1-xs1-1.0)/(xf1-xf2);
			ldx= -shb->shadhalostep/(xf1-xf2);
			dx= shb->shadhalostep;
		}
		else {
			labdax= (xf1-xs1)/(xf1-xf2);
			ldx= shb->shadhalostep/(xf1-xf2);
			dx= -shb->shadhalostep;
		}
	}
	else {
		labdax= 1.0;
		ldx= 0.0;
	}

	if(yf1 != yf2) {
		if(yf2-yf1 > 0.0) {
			labday= (yf1-ys1-1.0)/(yf1-yf2);
			ldy= -shb->shadhalostep/(yf1-yf2);
			dy= shb->shadhalostep;
		}
		else {
			labday= (yf1-ys1)/(yf1-yf2);
			ldy= shb->shadhalostep/(yf1-yf2);
			dy= -shb->shadhalostep;
		}
	}
	else {
		labday= 1.0;
		ldy= 0.0;
	}
	
	x= xs1;
	y= ys1;
	labda= count= lightcount= 0.0;

/* printf("start %x %x	\n", (int)(0x7FFFFFFF*zf1), (int)(0x7FFFFFFF*zf2)); */

	while(1) {
		labdao= labda;
		
		if(labdax==labday) {
			labdax+= ldx;
			x+= dx;
			labday+= ldy;
			y+= dy;
		}
		else {
			if(labdax<labday) {
				labdax+= ldx;
				x+= dx;
			} else {
				labday+= ldy;
				y+= dy;
			}
		}
		
		labda= MIN2(labdax, labday);
		if(labda==labdao || labda>=1.0) break;
		
		zf= zf1 + labda*(zf2-zf1);
		count+= 1.0;

		if(zf<= 0.0) lightcount += 1.0;	/* close to the spot */
		else {
		
			/* make sure, behind the clipend we extend halolines. */
			if(zf>=1.0) z= 0x7FFFF000;
			else z= (int)(0x7FFFF000*zf);
			
			lightcount+= readshadowbuf_halo(shb, x, y, z);
			
		}
	}
	
	if(count!=0.0) return (lightcount/count);
	return 0.0;
	
}









