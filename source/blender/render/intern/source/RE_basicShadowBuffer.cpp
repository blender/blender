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

#include <math.h>
#include <string.h>

//#include <iostream.h>
#include "MEM_guardedalloc.h"
#include "BLI_arithb.h"

#include "DNA_lamp_types.h"
/* c stuff */
#include "MTC_matrixops.h"
#include "render.h"
#include "render_intern.h"
#include "renderHelp.h"
#include "jitter.h"
#include "zbuf.h"
#include "shadbuf.h"

/* own include */
#include "RE_basicShadowBuffer.h"
/* crud */
#define MIN2(x,y)               ( (x)<(y) ? (x) : (y) )  
/* ------------------------------------------------------------------------- */
/* The implementation of this one is a bit of a fraud still, as it
 * still relies on everything internally to be done in C. Memory is
 * allocated on the fly, and deallocated elsewhere... There's not much
 * more than a handle for the implementation here. This is an exact
 * copy of the old code, retrofitted for integration in the unified
 * renderer. 
 *
 * - the shadow values are tripled to make a shadow vector out of a 
 * single shadow value
 */

RE_BasicShadowBuffer::RE_BasicShadowBuffer(struct LampRen *lar, float mat[][4])
{
//  	cout << "Constructing basic SB\n";
	bias = 0x00500000;
	initshadowbuf(lar, mat); /* a ref to the shb is stored in the lar */
}

RE_BasicShadowBuffer::~RE_BasicShadowBuffer(void)
{
	/* clean-up is done when the lar's are deleted */
//  	cout << "Destroying basic SB\n";
}

void RE_BasicShadowBuffer::lrectreadRectz(int x1, int y1, 
										  int x2, int y2, 
										  char *r1) /* leest deel uit rectz in r1 */
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


int RE_BasicShadowBuffer::sizeoflampbuf(struct ShadBuf *shb)
{
	int num,count=0;
	char *cp;
	
	cp= shb->cbuf;
	num= (shb->size*shb->size)/256;

	while(num--) count+= *(cp++);
	
	return 256*count;
}

float* RE_BasicShadowBuffer::give_jitter_tab(int samp)
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

void RE_BasicShadowBuffer::importScene(LampRen *lar)
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
	
	/* viewvars onthouden */
	temprx= R.rectx; tempry= R.recty;
	R.rectx= R.recty= shb->size;

	shb->jit= give_jitter_tab(shb->samp);

	/* matrices en window: in R.winmat komt transformatie
		van obsview naar lampview,  inclusief lampwinmat */
	
	wsize= shb->pixsize*(shb->size/2.0);

	i_window(-wsize, wsize, -wsize, wsize, shb->d, shb->far, shb->winmat);

	MTC_Mat4MulMat4(shb->persmat, shb->viewmat, shb->winmat);
	
	/* temp, will be restored */
	MTC_Mat4SwapMat4(shb->persmat, R.winmat);

	/* zbufferen */
	if(R.rectz) MEM_freeN(R.rectz);
 	R.rectz= (unsigned int *)MEM_mallocN(sizeof(int)*shb->size*shb->size,"makeshadbuf");
	rcline= (char*) MEM_mallocN(256*4+sizeof(int),"makeshadbuf2");

	/* onthouden: panorama rot */
	temp= panophi;
	panophi= 0.0;
	pushTempPanoPhi(0.0);

	/* pano interference here? */
	setzbufvlaggen(projectvert);

	popTempPanoPhi();
	panophi= temp;
	
	zbuffershad(lar);

	/* alle pixels 1 x ingevuld verwijderen (oneven) */
	/* probleem hierbij kan geven dat er abrupte overgangen van zacht gebied
	 * naar geen zacht gebied is: bijv als eronder een klein vlakje zit
	 * DAAROM ER WEER UIT
	 * ook vanwege shadowhalo!
	 * 
		a= shb->size*shb->size;
		rz= R.rectz;
		while(a--) {
		    if(*rz & 1) *rz= 0x7FFFFFFF;
		    rz++;
		}
	 */
	
	square= lar->mode & LA_SQUARE;
	
	/* Z tiles aanmaken: dit systeem is 24 bits!!! */
	
	ztile= shb->zbuf;
	ctile= shb->cbuf;
	for(y=0; y<shb->size; y+=16) {
		if(y< shb->size/2) miny= y+15-shb->size/2;
		else miny= y-shb->size/2;	
				
		for(x=0; x<shb->size; x+=16) {

			/* ligt rechthoek binnen spotbundel? */
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
					if( (*rz1 & 0xFFFFFF00) != verg) break;
				}
			}
			if(a==256) { /* compleet leeg vakje */
				*ctile= 0;
				*ztile= *(rz1-1);
			}
			else {
				
				/* ACOMP enz. zijn defined L/B endian */
				
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
				if(byt1 && byt2) {	/* alleen byte opslaan */
					*ctile= 1;
					*ztile= (unsigned long)MEM_mallocN(256+4, "tile1");
					rz= (int *)*ztile;
					*rz= *rz1;

					zt= (char *)(rz+1);
					rc= rcline;
					for(a=0; a<256; a++, zt++, rc+=4) *zt= rc[GCOMP];	
				}
				else if(byt1) {		/* short opslaan */
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
				else {			/* triple opslaan */
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

int RE_BasicShadowBuffer::firstreadshadbuf(struct ShadBuf *shb, int xs, int ys, int nr)
{
	/* return 1 als volledig gecomprimeerde shadbuftile && z==const */
	static int *rz;
	int ofs;
	char *ct;

	/* always test borders of shadowbuffer */
	if(xs<0) xs= 0; else if(xs>=shb->size) xs= shb->size-1;
	if(ys<0) ys= 0; else if(ys>=shb->size) ys= shb->size-1;
   
	/* z berekenen */
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

float RE_BasicShadowBuffer::readshadowbuf(struct ShadBuf *shb, 
										  int xs, int ys, int zs)	/* return 1.0 : volledig licht */
{
	float temp;
	int *rz, ofs;
	int zsamp;
	char *ct, *cz;

	/* simpleclip */
	/* if(xs<0 || ys<0) return 1.0; */
	/* if(xs>=shb->size || ys>=shb->size) return 1.0; */
	
	/* always test borders of shadowbuffer */
	if(xs<0) xs= 0; else if(xs>=shb->size) xs= shb->size-1;
	if(ys<0) ys= 0; else if(ys>=shb->size) ys= shb->size-1;

	/* z berekenen */
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
		/* got warning on this from alpha .... */
		/* but it's working code! (ton) */
 		zsamp= (int) rz;
	}

	/* if(zsamp >= 0x7FFFFE00) return 1.0; */	/* geen schaduw als op oneindig wordt gesampeld*/

	if(zsamp > zs) return 1.0; 		/* absoluut geen schaduw */
	else if( zsamp < zs-bias) return 0.0 ;	/* absoluut wel schaduw */
	else {					/* zacht gebied */

		temp=  ( (float)(zs- zsamp) )/(float)bias;
		return 1.0 - temp*temp;
			
	}
}


void RE_BasicShadowBuffer::readShadowValue(struct ShadBuf *shb, 
										   float inp, 
										   float * shadres)  	/* return 1.0: geen schaduw */
{
	float fac, co[4], dx[3], dy[3], aantal=0;
	float xs1,ys1, siz, *j, xres, yres;
	int xs,ys, zs;
	short a,num;
	float shadowfactor = 1.0;
	
#ifdef RE_NO_SHADOWS
	shadres[0] = shadowfactor;
	shadres[1] = shadowfactor;
	shadres[2] = shadowfactor;	
	return;
#endif
	
	/* if(inp <= 0.0) return 1.0; */

	/* renderco en osaco roteren */
	siz= 0.5*(float)shb->size;
	VECCOPY(co, R.co);
	co[3]= 1.0;

	MTC_Mat4MulVec4fl(shb->persmat, co);	/* rationele hom co */

	xs1= siz*(1.0+co[0]/co[3]);
	ys1= siz*(1.0+co[1]/co[3]);

	/* Clip for z: near and far clip values of the shadow buffer. We
     * can test for -1.0/1.0 because of the properties of the
     * coordinate transformations. */
	fac= (co[2]/co[3]);

	if(fac>=1.0) {
		shadres[0] = 0.0;
		shadres[1] = 0.0;
		shadres[2] = 0.0;	
		return;
	} else if(fac<= -1.0) {
		shadres[0] = 1.0;
		shadres[1] = 1.0;
		shadres[2] = 1.0;	
		return;
	}

	zs = (int) (((float)0x7FFFFFFF)*fac);

	/* num*num samples nemen, gebied met fac vergroten */
	num= shb->samp*shb->samp;
	fac= shb->soft;
	
	
	bias = (int) ((1.1-inp*inp)*shb->bias);

	if(num==1) {
		shadowfactor = readshadowbuf(shb,(int)xs1, (int)ys1, zs);
		shadres[0] = shadowfactor;
		shadres[1] = shadowfactor;
		shadres[2] = shadowfactor;	
		return;
	}

	co[0]= R.co[0]+O.dxco[0];
	co[1]= R.co[1]+O.dxco[1];
	co[2]= R.co[2]+O.dxco[2];
	co[3]= 1.0;
	MTC_Mat4MulVec4fl(shb->persmat,co);	/* rationele hom co */
	dx[0]= xs1- siz*(1.0+co[0]/co[3]);
	dx[1]= ys1- siz*(1.0+co[1]/co[3]);

	co[0]= R.co[0]+O.dyco[0];
	co[1]= R.co[1]+O.dyco[1];
	co[2]= R.co[2]+O.dyco[2];
	co[3]= 1.0;
	MTC_Mat4MulVec4fl(shb->persmat,co);	/* rationele hom co */
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
						/* this return should do some renormalization, methinks */
						shadowfactor = readshadowbuf(shb,(int)xs1, (int)ys1, zs);
						shadres[0] = shadowfactor;
						shadres[1] = shadowfactor;
						shadres[2] = shadowfactor;	
						return;
					}
				}
			}
	    }
	}

	for(a=num;a>0;a--) {	
		    /* i.p.v. jit ook met random geprobeerd: lelijk! */
		xs= (int) (xs1 + xres*j[0]);
		ys= (int) (ys1 + yres*j[1]);
		j+=2;
		
		aantal+= readshadowbuf(shb, xs, ys, zs);
	}

	/* Renormalizes for the sample number: */
	shadowfactor = aantal/( (float)(num) ); 
	shadres[0] = shadowfactor;
	shadres[1] = shadowfactor;
	shadres[2] = shadowfactor;	
	return;
}

/* different function... sampling behind clipend can be LIGHT, bias is negative! */
/* return: light */
float RE_BasicShadowBuffer::readshadowbuf_halo(struct ShadBuf *shb, int xs, int ys, int zs)
{
	float temp;
	int *rz, ofs;
	int zbias, zsamp;
	char *ct, *cz;

	/* simpleclip */
	if(xs<0 || ys<0) return 0.0;
	if(xs>=shb->size || ys>=shb->size) return 0.0;

	/* z berekenen */
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

	/* geen schaduw als op oneindig wordt gesampeld*/

	if(zsamp >= 0x7FFFFE00) return 1.0; 

	if(zsamp > zs) return 1.0; 		/* absoluut geen schaduw */
	else {
		/* bias is negative, so the (zs-bias) can be beyond 0x7fffffff */
		zbias= 0x7fffffff - zs;
		if(zbias > -bias) {
			if( zsamp < zs-bias) return 0.0 ;	/* absoluut wel schaduw */
		}
		else return 0.0 ;	/* absoluut wel schaduw */
	}
	
	/* zacht gebied */

	temp=  ( (float)(zs- zsamp) )/(float)bias;
	return 1.0 - temp*temp;
}


float RE_BasicShadowBuffer::shadow_halo(LampRen *lar, float *p1, float *p2)
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
	MTC_Mat4MulVec4fl(shb->winmat, co);	/* rationele hom co */
	xf1= siz*(1.0+co[0]/co[3]);
	yf1= siz*(1.0+co[1]/co[3]);
	zf1= (co[2]/co[3]);


	co[0]= p2[0];
	co[1]= p2[1];
	co[2]= p2[2]/lar->sh_zfac;
	co[3]= 1.0;
	MTC_Mat4MulVec4fl(shb->winmat, co);	/* rationele hom co */
	xf2= siz*(1.0+co[0]/co[3]);
	yf2= siz*(1.0+co[1]/co[3]);
	zf2= (co[2]/co[3]);

	/* de 2dda */

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




/* sampelen met filter
	xstart= xs-1;
	ystart= ys-1;
	if(xstart<0) xstart= 0;
	if(ystart<0) ystart= 0;
	xend= xstart+2;
	yend= ystart+2;
	if(xend>=shb->size) { xstart= shb->size-3; xend= shb->size-1;}
	if(yend>=shb->size) { ystart= shb->size-3; yend= shb->size-1;}

	fid= filt3;
	for(ys=ystart;ys<=yend;ys++) {
		rz= shb->buf+ ys*shb->size+ xstart;
		for(xs= xstart;xs<=xend;xs++,rz++) {
			if( *rz+0x100000<zs) aantal+= *fid;
			fid++;
		}
	}
	

	return 1.0-((float)aantal)/16.0;
*/
