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
 * Contributor(s): Hos, Robert Wenzlaff.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */


/* External modules: */
#include "MEM_guardedalloc.h"
#include "BLI_arithb.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "MTC_matrixops.h"
#include "BKE_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"

#include "BKE_global.h"
#include "BKE_texture.h"

/* local include */
#include "RE_callbacks.h"
#include "old_zbuffer_types.h"
#include "render.h"
#include "render_intern.h"
#include "zbuf.h"		/* stuff like bgnaccumbuf, fillrect, ...*/
#include "pixelblending.h"
#include "shadbuf.h"
#include "renderHelp.h"

#include "jitter.h"

/* system includes */
#include <math.h>
#include <string.h>
#include <stdlib.h>


/* own include */
#include "rendercore.h"
#include "rendercore_int.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* globals for this file */
/* moved to renderData.c? Not yet... */
RE_Render R;
Osa O;

extern float Zjitx,Zjity, Zmulx, Zmuly;
PixStrMain psmfirst;
int psmteller;

float holoofs= 0.0, fmask[256], centLut[16];
unsigned short usegamtab=0, shortcol[4], *mask1[9], *mask2[9], *igamtab1, *igamtab2, *gamtab;
char cmask[256], *centmask;

/* functions */
/* comes from texture.c (only used here !) */
/*  extern void do_halo_tex(HaloRen *har, float xn, float yn, float *colf); */

void gamtabdit(unsigned short *in, char *out);
/*  int count_mask(unsigned short ); */
void scanlinehalo(unsigned int *rectz, unsigned int *rectt, short ys);
/*  void add_halo_flare(void); */
void edge_enhance(void);

/* Dither with gamma table? */
void gamtabdit(unsigned short *in, char *out)
/*  unsigned short *in; */
/*  char *out; */
{
	static short rerr=0, gerr=0, berr=0;
	unsigned int col;
	char *cp;

	cp= (char *)&col;
	out[0]= in[0]>>8;

	col= gamtab[in[2]]+berr;
	if(col>65535) col= 65535;
	out[1]= cp[2];
	berr= cp[3];

	col= gamtab[in[4]]+gerr;
	if(col>65535) col= 65535;
	out[2]= cp[2];
	gerr= cp[3];

	col= gamtab[in[6]]+rerr;
	if(col>65535) col= 65535;
	out[3]= cp[2];
	rerr= cp[3];

}

float mistfactor(float *co)	/* dist en height, return alpha */
{
	float fac, hi;
	
	fac= R.zcor - R.wrld.miststa;	/* R.zcor is calculated per pixel */

	/* fac= -co[2]-R.wrld.miststa; */

	if(fac>0.0) {
		if(fac< R.wrld.mistdist) {
			
			fac= (fac/(R.wrld.mistdist));
			
			if(R.wrld.mistype==0) fac*= fac;
			else if(R.wrld.mistype==1);
			else fac= sqrt(fac);
		}
		else fac= 1.0;
	}
	else fac= 0.0;
	
	/* height switched off mist */
	if(R.wrld.misthi!=0.0 && fac!=0.0) {
		/* at height misthi the mist is completely gone */

		hi= R.viewinv[0][2]*co[0]+R.viewinv[1][2]*co[1]+R.viewinv[2][2]*co[2]+R.viewinv[3][2];
		
		if(hi>R.wrld.misthi) fac= 0.0;
		else if(hi>0.0) {
			hi= (R.wrld.misthi-hi)/R.wrld.misthi;
			fac*= hi*hi;
		}
	}

	return (1.0-fac)* (1-R.wrld.misi);	
}

void RE_sky(char *col)
{
	float rf, gf, bf;

	if((R.wrld.skytype & (WO_SKYBLEND+WO_SKYTEX))==0) {
		*( (unsigned int *)col)= R.wrld.fastcol;
		return;
	}

	R.wrld.skytype |= WO_ZENUP;
	
	if(R.wrld.skytype & WO_SKYREAL) {
	
		R.inprz= R.view[0]*R.grvec[0]+ R.view[1]*R.grvec[1]+ R.view[2]*R.grvec[2];

		if(R.inprz<0.0) R.wrld.skytype-= WO_ZENUP;
		R.inprz= fabs(R.inprz);
	}
	else if(R.wrld.skytype & WO_SKYPAPER) {
		R.inprz= 0.5+ 0.5*R.view[1];
	}
	else {
		R.inprz= fabs(0.5+ R.view[1]);
	}

	if(R.wrld.skytype & WO_SKYTEX) {
		VECCOPY(R.lo, R.view);
		if(R.wrld.skytype & WO_SKYREAL) {
			
			MTC_Mat3MulVecfl(R.imat, R.lo);

			SWAP(float, R.lo[1],  R.lo[2]);
			
		}

		R.osatex= 0;

		do_sky_tex();
		
	}

	if(R.inprz>1.0) R.inprz= 1.0;
	R.inprh= 1.0-R.inprz;

	if(R.wrld.skytype & WO_SKYBLEND) {
		rf= 255.0*(R.inprh*R.wrld.horr + R.inprz*R.wrld.zenr);
		gf= 255.0*(R.inprh*R.wrld.horg + R.inprz*R.wrld.zeng);
		bf= 255.0*(R.inprh*R.wrld.horb + R.inprz*R.wrld.zenb);

		if (rf>255.0) col[0]= 255; 
		else col[0]= (char)rf;
		if (gf>255.0) col[1]= 255; 
		else col[1]= (char)gf;
		if (bf>255.0) col[2]= 255; 
		else col[2]= (char)bf;
	}
	else {
		col[0]= 255.0*R.wrld.horr;
		col[1]= 255.0*R.wrld.horg;
		col[2]= 255.0*R.wrld.horb;
	}
	col[3]= 1;	/* to prevent wrong optimalisation alphaover of flares */
}

/* ------------------------------------------------------------------------- */

void scanlinesky(char *rect, int y)
{
	/* have to type this! set to :  addalphaUnder: char*, char*
	 * addalphaUnderGamma: ditto called with char *, uint* !!!
	 * unmangle this shit... */
	void (*alphafunc)();
	float fac;
	int dx, x, ofs;
	unsigned int col=0, *rt;
	short *sp;
	char *cp, *cp1;
	float u, v;
	
	if(R.r.alphamode & R_ALPHAPREMUL) return;
	
	if(R.r.alphamode & R_ALPHAKEY) {
		
		cp= (char *)&col;
		cp[3]= 1;
		cp[0]= 255.0*R.wrld.horr;
		cp[1]= 255.0*R.wrld.horg;
		cp[2]= 255.0*R.wrld.horb;
		
		for(x=0; x<R.rectx; x++, rect+= 4) {
			if(rect[3]==0) {
				*( ( unsigned int *)rect)= col;
			}
			else {
				/* prevent  'col' to be in the image */
				cp1= (char *)rect;
				if( cp[0]==cp1[0] && cp[1]==cp1[1] && cp[2]==cp1[2] ) {

  					if(cp1[3]==255) cp1[3]= 254; 
  					else cp1[3]++; 
				}

  				if(rect[3]!=255) { 
  					keyalpha(rect); 
  				} 
			}
		}
		return;
	}

	if(R.wrld.mode & WO_MIST) alphafunc= addalphaUnder;
	else alphafunc= addalphaUnderGamma;


	if(R.r.bufflag & 1) {
		if(R.backbuf->ok) {
			if(R.backbuf->ibuf==0) {
				R.backbuf->ibuf= IMB_loadiffname(R.backbuf->name, IB_rect);
				if(R.backbuf->ibuf==0) {
					R.backbuf->ok= 0;
					return;
				}
			}
			/* which scanline/ */
			y= ((y+R.afmy+R.ystart)*R.backbuf->ibuf->y)/(2*R.afmy);
			
			if(R.flag & R_SEC_FIELD) {
				if((R.r.mode & R_ODDFIELD)==0) {
					if( y<R.backbuf->ibuf->y) y++;
				}
				else {
					if( y>0) y--;
				}
			}
				
			rt= (R.backbuf->ibuf->rect + y*R.backbuf->ibuf->x);
			
			/* at which location */
			fac= ((float)R.backbuf->ibuf->x)/(float)(2*R.afmx);
			ofs= (R.afmx+R.xstart)*fac;
			rt+= ofs;

			dx= (int) (65536.0*fac);
			sp= (short *)&ofs;
			
			#ifdef BBIG_ENDIAN
			#else
				sp++; 
			#endif
			
			ofs= 0;
			x= R.rectx;
			while( x-- ) {
  				if( rect[3] != 255) { 
					if(rect[3]==0) *((unsigned int *)rect)= *rt;
					else {
						alphafunc(rect, rt);
					}
				}
				rect+= 4;
				
				ofs+= dx;
				while( *sp>0 ) {
					ofs-= 65536;
					rt++;
				}
			}

		}
		return;
	}

	if((R.wrld.skytype & (WO_SKYBLEND+WO_SKYTEX))==0) {
		for(x=0; x<R.rectx; x++, rect+= 4) {
  			if(rect[3] != 255) { 
				if(rect[3]==0) *((unsigned int *)rect)= R.wrld.fastcol;
				else {
					alphafunc(rect, &R.wrld.fastcol);
				}
			}
		}
	}
	else {
		
		for(x=0; x<R.rectx; x++, rect+= 4) {
  			if(rect[3] < 254) { 
				if(R.wrld.skytype & WO_SKYPAPER) {
					R.view[0]= (x+(R.xstart))/(float)R.afmx;
					R.view[1]= (y+(R.ystart))/(float)R.afmy;
					R.view[2]= 0.0;
				}
				else {
					R.view[0]= (x+(R.xstart)+1.0);
		
					if(R.flag & R_SEC_FIELD) {
						if(R.r.mode & R_ODDFIELD) R.view[1]= (y+R.ystart+0.5)*R.ycor;
						else R.view[1]= (y+R.ystart+1.5)*R.ycor;
					}
					else R.view[1]= (y+R.ystart+1.0)*R.ycor;
					
					R.view[2]= -R.viewfac;
	
					fac= Normalise(R.view);
					if(R.wrld.skytype & WO_SKYTEX) {
						O.dxview= 1.0/fac;
						O.dyview= R.ycor/fac;
					}
				}
				
				if(R.r.mode & R_PANORAMA) {
					float panoco, panosi;
					panoco = getPanovCo();
					panosi = getPanovSi();
					u= R.view[0]; v= R.view[2];
					
					R.view[0]= panoco*u + panosi*v;
					R.view[2]= -panosi*u + panoco*v;
				}

				RE_sky((char *)&col);
	
				if(rect[3]==0) *((unsigned int *)rect)= col;
				else alphafunc(rect, &col);
			}
		}
	}	
}

/* ************************************** */



extern float Tin;  /* texture.c */
void do_lamphalo_tex(LampRen *lar, float *p1, float *p2, float *intens)
{
	/* p1 p2 already are rotated in spot-space */
	float v1[3], vec[3];
	float dist, totin=0.0;
	int a, steps= 32;
	
	v1[0]= p1[0];
	v1[1]= p1[1];
	v1[2]= p1[2]/lar->sh_zfac;
	
	VecSubf(vec, p2, p1);
	dist= Normalise(vec);
	
	/* fixed number of steps */
	dist/= (float)steps;

	for(a=0; a<steps; a++) {
		
		VecAddf(v1, v1, vec);
		VECCOPY(R.co, v1);
		do_lamp_tex(lar, v1);
		totin+= Tin;
		
	}
	
	totin/= (float)steps;
	
	*intens *= totin;
}



void spothalo(struct LampRen *lar, float *view, float *intens)
{
	double a, b, c, disc, nray[3], npos[3];
	float t0, t1 = 0.0, t2= 0.0, t3, haint;
	float p1[3], p2[3], ladist, maxz = 0.0, maxy = 0.0;
	int snijp, doclip=1, use_yco=0;
	int ok1=0, ok2=0;
	
	*intens= 0.0;
	haint= lar->haint;
	
	VECCOPY(npos, lar->sh_invcampos);	/* in initlamp calculated */
	
	/* rotate view */
	VECCOPY(nray, view);
	MTC_Mat3MulVecd(lar->imat, nray);
	
	if(R.wrld.mode & WO_MIST) {
		/* patchy... */
		R.zcor= -lar->co[2];
		haint *= mistfactor(lar->co);
		if(haint==0.0) {
			return;
		}
	}


	/* rotate maxz */
	if(R.co[2]==0) doclip= 0;	/* for when halo at sky */
	else {
		p1[0]= R.co[0]-lar->co[0];
		p1[1]= R.co[1]-lar->co[1];
		p1[2]= R.co[2]-lar->co[2];
	
		maxz= lar->imat[0][2]*p1[0]+lar->imat[1][2]*p1[1]+lar->imat[2][2]*p1[2];
		maxz*= lar->sh_zfac;
		maxy= lar->imat[0][1]*p1[0]+lar->imat[1][1]*p1[1]+lar->imat[2][1]*p1[2];

		if( fabs(nray[2]) <0.000001 ) use_yco= 1;
	}
	
	/* scale z to make sure volume is normalized */	
	nray[2]*= lar->sh_zfac;
	/* nray does not need normalization */
	
	ladist= lar->sh_zfac*lar->dist;
	
	/* solve */
	a = nray[0] * nray[0] + nray[1] * nray[1] - nray[2]*nray[2];
	b = nray[0] * npos[0] + nray[1] * npos[1] - nray[2]*npos[2];
	c = npos[0] * npos[0] + npos[1] * npos[1] - npos[2]*npos[2];

	snijp= 0;
	if (fabs(a) < 0.00000001) {
		/*
		 * Only one intersection point...
		 */
		return;
	}
	else {
		disc = b*b - a*c;
		
		if(disc==0.0) {
			t1=t2= (-b)/ a;
			snijp= 2;
		}
		else if (disc > 0.0) {
			disc = sqrt(disc);
			t1 = (-b + disc) / a;
			t2 = (-b - disc) / a;
			snijp= 2;
		}
	}
	if(snijp==2) {
		/* sort */
		if(t1>t2) {
			a= t1; t1= t2; t2= a;
		}

		/* z of intersection points with diabolo */
		p1[2]= npos[2] + t1*nray[2];
		p2[2]= npos[2] + t2*nray[2];

		/* evaluate both points */
		if(p1[2]<=0.0) ok1= 1;
		if(p2[2]<=0.0 && t1!=t2) ok2= 1;
		
		/* at least 1 point with negative z */
		if(ok1==0 && ok2==0) return;
		
		/* intersction point with -ladist, the bottom of the cone */
		if(use_yco==0) {
			t3= (-ladist-npos[2])/nray[2];
				
			/* de we have to replace one of the intersection points? */
			if(ok1) {
				if(p1[2]<-ladist) t1= t3;
			}
			else {
				ok1= 1;
				t1= t3;
			}
			if(ok2) {
				if(p2[2]<-ladist) t2= t3;
			}
			else {
				ok2= 1;
				t2= t3;
			}
		}
		else if(ok1==0 || ok2==0) return;
		
		/* at least 1 visible interesction point */
		if(t1<0.0 && t2<0.0) return;
		
		if(t1<0.0) t1= 0.0;
		if(t2<0.0) t2= 0.0;
		
		if(t1==t2) return;
		
		/* sort again to be sure */
		if(t1>t2) {
			a= t1; t1= t2; t2= a;
		}
		
		/* calculate t0: is the maximum visible z (when halo is intersected by face) */ 
		if(doclip) {
			if(use_yco==0) t0= (maxz-npos[2])/nray[2];
			else t0= (maxy-npos[1])/nray[1];

			if(t0<t1) return;
			if(t0<t2) t2= t0;
		}

		/* calc points */
		p1[0]= npos[0] + t1*nray[0];
		p1[1]= npos[1] + t1*nray[1];
		p1[2]= npos[2] + t1*nray[2];
		p2[0]= npos[0] + t2*nray[0];
		p2[1]= npos[1] + t2*nray[1];
		p2[2]= npos[2] + t2*nray[2];
		
			
		/* now we have 2 points, make three lengths with it */
		
		a= sqrt(p1[0]*p1[0]+p1[1]*p1[1]+p1[2]*p1[2]);
		b= sqrt(p2[0]*p2[0]+p2[1]*p2[1]+p2[2]*p2[2]);
		c= VecLenf(p1, p2);
		
		a/= ladist;
		a= sqrt(a);
		b/= ladist; 
		b= sqrt(b);
		c/= ladist;
		
		*intens= c*( (1.0-a)+(1.0-b) );

		/* WATCH IT: do not clip a,b en c at 1.0, this gives nasty little overflows
			at the edges (especially with narrow halos) */
		if(*intens<=0.0) return;

		/* soft area */
		/* not needed because t0 has been used for p1/p2 as well */
		/* if(doclip && t0<t2) { */
		/* 	*intens *= (t0-t1)/(t2-t1); */
		/* } */
		
		*intens *= haint;
		
		if(lar->shb && lar->shb->shadhalostep) {
			*intens *= shadow_halo(lar, p1, p2);
		}
		/* if(lar->mode & LA_TEXTURE)  do_lamphalo_tex(lar, p1, p2, intens); */
		
	}
}

void renderspothalo(unsigned short *col)
{
	LampRen *lar;
	float i;
	int colt, a;
	unsigned short scol[4];
	
	
	for(a=0; a<R.totlamp; a++) {
		lar= R.la[a];
		if(lar->type==LA_SPOT && (lar->mode & LA_HALO) && lar->haint>0) {
	
			if(lar->org) {
				lar->r= lar->org->r;
				lar->g= lar->org->g;
				lar->b= lar->org->b;
			}

			spothalo(lar, R.view, &i);
			
			if(i>0.0) {

				i*= 65536.0;
				
				colt= i;
				if(colt>65535) scol[3]= 65535; else scol[3]= colt;

				/* really bad: gamma corrected rendering AND addalphaADD doesnt work together */
				/* actually we should invent a special add-gamma type */
				
				colt= i*lar->r;
				if(colt>65535) scol[0]= 65535; else scol[0]= colt;
				if(usegamtab)  scol[0]= igamtab2[scol[0]];
				colt= i*lar->g;
				if(colt>65535) scol[1]= 65535; else scol[1]= colt;
				if(usegamtab)  scol[1]= igamtab2[scol[1]];
				colt= i*lar->b;
				if(colt>65535) scol[2]= 65535; else scol[2]= colt;
				if(usegamtab) scol[2]= igamtab2[scol[2]];
				
				addalphaAddshort(col, scol);
			}
		}
	}
}

void render_lighting_halo(HaloRen *har, float *colf)
{
	LampRen *lar;
	float i, inp, inpr, lv[3], lampdist, ld, t, *vn;
	float ir, ig, ib, shadfac, soft;
	int a;
	
	ir= ig= ib= 0.0;
	VECCOPY(R.co, har->co);
	vn= har->no;
	
	O.dxco[0]= har->hasize;
	O.dxco[1]= 0.0;
	O.dxco[2]= 0.0;

	O.dyco[0]= 0.0;
	O.dyco[1]= har->hasize;
	O.dyco[2]= 0.0;

	for(a=0; a<R.totlamp; a++) {
		lar= R.la[a];

		/* test for lamplayer */
		if(lar->mode & LA_LAYER) if((lar->lay & har->lay)==0) continue;

		/* lampdist cacluation */
		if(lar->type==LA_SUN || lar->type==LA_HEMI) {
			VECCOPY(lv, lar->vec);
			lampdist= 1.0;
		}
		else {
			lv[0]= R.co[0]-lar->co[0];
			lv[1]= R.co[1]-lar->co[1];
			lv[2]= R.co[2]-lar->co[2];
			ld= sqrt(lv[0]*lv[0]+lv[1]*lv[1]+lv[2]*lv[2]);
			lv[0]/= ld;
			lv[1]/= ld;
			lv[2]/= ld;
			
			/* ld is re-used further on (texco's) */
			
			if(lar->mode & LA_QUAD) {
				t= 1.0;
				if(lar->ld1>0.0)
					t= lar->dist/(lar->dist+lar->ld1*ld);
				if(lar->ld2>0.0)
					t*= lar->distkw/(lar->distkw+lar->ld2*ld*ld);

				lampdist= t;
			}
			else {
				lampdist= (lar->dist/(lar->dist+ld));
			}

			if(lar->mode & LA_SPHERE) {
				t= lar->dist - ld;
				if(t<0.0) continue;
				
				t/= lar->dist;
				lampdist*= (t);
			}
			
		}
		
		if(lar->mode & LA_TEXTURE)  do_lamp_tex(lar, lv);

		if(lar->type==LA_SPOT) {

			if(lar->mode & LA_SQUARE) {
				if(lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2]>0.0) {
					float x, lvrot[3];
					
					/* rotate view to lampspace */
					VECCOPY(lvrot, lv);
					MTC_Mat3MulVecfl(lar->imat, lvrot);
					
					x= MAX2(fabs(lvrot[0]/lvrot[2]) , fabs(lvrot[1]/lvrot[2]));
					/* 1.0/(sqrt(1+x*x)) is equivalent to cos(atan(x)) */

					inpr= 1.0/(sqrt(1.0+x*x));
				}
				else inpr= 0.0;
			}
			else {
				inpr= lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2];
			}

			t= lar->spotsi;
			if(inpr<t) continue;
			else {
				t= inpr-t;
				i= 1.0;
				soft= 1.0;
				if(t<lar->spotbl && lar->spotbl!=0.0) {
					/* soft area */
					i= t/lar->spotbl;
					t= i*i;
					soft= (3.0*t-2.0*t*i);
					inpr*= soft;
				}
				if(lar->mode & LA_ONLYSHADOW) {
					/* if(ma->mode & MA_SHADOW) { */
						/* dot product positive: front side face! */
						inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
						if(inp>0.0) {
							/* testshadowbuf==0.0 : 100% shadow */
							shadfac = testshadowbuf(lar->shb, inp);
							if( shadfac>0.0 ) {
								shadfac*= inp*soft*lar->energy;
								ir -= shadfac;
								ig -= shadfac;
								ib -= shadfac;
								
								continue;
							}
						}
					/* } */
				}
				lampdist*=inpr;
			}
			if(lar->mode & LA_ONLYSHADOW) continue;

		}

		/* dot product and  reflectivity*/
		
		inp= 1.0-fabs(vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2]);
		
		/* inp= cos(0.5*M_PI-acos(inp)); */
		
		i= inp;
		
		if(lar->type==LA_HEMI) {
			i= 0.5*i+0.5;
		}
		if(i>0.0) {
			i*= lampdist;
			/* i*= lampdist*ma->ref; */
		}

		/* shadow  */
		if(i> -0.41) {			/* heuristic valua! */
			shadfac= 1.0;
			if(lar->shb) {
				/* if(ma->mode & MA_SHADOW) { */
				shadfac = testshadowbuf(lar->shb, inp);
				if(shadfac==0.0) continue;
				i*= shadfac;
				/* } */
			}
		}
		
		if(i>0.0) {
			ir+= i*lar->r;
			ig+= i*lar->g;
			ib+= i*lar->b;
		}
	}
	
	if(ir<0.0) ir= 0.0;
	if(ig<0.0) ig= 0.0;
	if(ib<0.0) ib= 0.0;

	colf[0]*= ir;
	colf[1]*= ig;
	colf[2]*= ib;
	
}


extern float hashvectf[];
void RE_shadehalo(HaloRen *har, char *col, unsigned int zz, float dist, float xn, float yn, short flarec)
{
	/* fill in in col */
	float t, zn, radist, ringf=0.0, linef=0.0, alpha, si, co, colf[4];
	int colt, a;
   
	if(R.wrld.mode & WO_MIST) {
       if(har->type & HA_ONLYSKY) {
           /* stars have no mist */
           alpha= har->alfa;
       }
       else {
           /* patchy... */
           R.zcor= -har->co[2];
           alpha= mistfactor(har->co)*har->alfa;
       }
	}
	else alpha= har->alfa;
	
	if(alpha==0.0) {
		col[0] = 0;
		col[1] = 0;
		col[2] = 0;
		col[3] = 0;

/*  		*( (int *)col )=0; */

		return;
	}

	radist= sqrt(dist);

	/* watch it: abused value: flarec was set to zero in pixstruct */
	if(flarec) har->pixels+= (int)(har->rad-radist);

	if(har->ringc) {
		float *rc, fac;
		int ofs;
		
		/* per ring an antialised circle */
		ofs= har->seed;
		
		for(a= har->ringc; a>0; a--, ofs+=2) {
			
			rc= hashvectf + (ofs % 768);
			
			fac= fabs( rc[1]*(har->rad*fabs(rc[0]) - radist) );
			
			if(fac< 1.0) {
				ringf+= (1.0-fac);
			}
		}
	}

	if(har->type & HA_VECT) {
		dist= fabs( har->cos*(yn) - har->sin*(xn) )/har->rad;
		if(dist>1.0) dist= 1.0;
		if(har->tex) {
			zn= har->sin*xn - har->cos*yn;
			yn= har->cos*xn + har->sin*yn;
			xn= zn;
		}
	}
	else dist= dist/har->radsq;

	if(har->type & HA_FLARECIRC) {
		
		dist= 0.5+fabs(dist-0.5);
		
	}

	if(har->hard>=30) {
		dist= sqrt(dist);
		if(har->hard>=40) {
			dist= sin(dist*M_PI_2);
			if(har->hard>=50) {
				dist= sqrt(dist);
			}
		}
	}
	else if(har->hard<20) dist*=dist;

	dist=(1.0-dist);
	
	if(har->linec) {
		float *rc, fac;
		int ofs;
		
		/* per starpoint an antialaised line */
		ofs= har->seed;
		
		for(a= har->linec; a>0; a--, ofs+=3) {
			
			rc= hashvectf + (ofs % 768);
			
			fac= fabs( (xn)*rc[0]+(yn)*rc[1]);
			
			if(fac< 1.0 ) {
				linef+= (1.0-fac);
			}
		}
		
		linef*= dist;
		
	}

	if(har->starpoints) {
		float ster, hoek;
		/* rotation */
		hoek= atan2(yn, xn);
		hoek*= (1.0+0.25*har->starpoints);
		
		co= cos(hoek);
		si= sin(hoek);
		
		hoek= (co*xn+si*yn)*(co*yn-si*xn);
		
		ster= fabs(hoek);
		if(ster>1.0) {
			ster= (har->rad)/(ster);
			
			if(ster<1.0) dist*= sqrt(ster);
		}
	}
	
	/* halo intersected? */
	if(har->zs> zz-har->zd) {
		t= ((float)(zz-har->zs))/(float)har->zd;
		alpha*= sqrt(sqrt(t));
	}

	dist*= alpha;
	ringf*= dist;
	linef*= alpha;
	
	if(dist<0.003) {
		*( (int *)col )=0;
		return;
	}

	/* texture? */
	if(har->tex) {
		colf[3]= dist;
		do_halo_tex(har, xn, yn, colf);
		
		/* dist== colf[3]; */
		
		colf[0]*= colf[3];
		colf[1]*= colf[3];
		colf[2]*= colf[3];
		
	}
	else {
		colf[0]= dist*har->r;
		colf[1]= dist*har->g;
		colf[2]= dist*har->b;
		
		if(har->type & HA_XALPHA) colf[3]= dist*dist;
		else colf[3]= dist;
	}

	if(har->mat && har->mat->mode & MA_HALO_SHADE) {
		/* we test for lights because of preview... */
		if(R.totlamp) render_lighting_halo(har, colf);
	}

	if(linef!=0.0) {
		Material *ma= har->mat;
		
		colf[0]+= 255.0*linef*ma->specr;
		colf[1]+= 255.0*linef*ma->specg;
		colf[2]+= 255.0*linef*ma->specb;
		
		if(har->type & HA_XALPHA) colf[3]+= linef*linef;
		else colf[3]+= linef;
	}
	if(ringf!=0.0) {
		Material *ma= har->mat;
		
		colf[0]+= 255.0*ringf*ma->mirr;
		colf[1]+= 255.0*ringf*ma->mirg;
		colf[2]+= 255.0*ringf*ma->mirb;
		
		if(har->type & HA_XALPHA) colf[3]+= ringf*ringf;
		else colf[3]+= ringf;
	}

	colt= 255.0*colf[3];
	if(colt>254) col[3]= 255; else col[3]= colt;

	colt= colf[2];
	if(colt>254) col[2]= 255; else col[2]= colt;

	colt= colf[1];
	if(colt>254) col[1]= 255; else col[1]= colt;

	colt= colf[0];
	if(colt>254) col[0]= 255; else col[0]= colt;

}


unsigned int calchalo_z(HaloRen *har, unsigned int zz)
{

	if(har->type & HA_ONLYSKY) {
		if(zz!=0x7FFFFFFF) zz= 0;
	}
	else {
		zz= (zz>>8);
		if(zz<0x800000) zz= (zz+0x7FFFFF);
		else zz= (zz-0x800000);
	}
	return zz;
}

void scanlinehaloPS(unsigned int *rectz, long *rectdelta, unsigned int *rectt, short ys)
{
	HaloRen *har = NULL;
	PixStr *ps;
	float dist,xsq,ysq,xn,yn;
	unsigned int a, *rz, *rt, zz;
	long *rd;
	int accol[4];
	short minx,maxx,x,aantal, aantalm, behind, flarec;
	char col[4];

	for(a=0;a<R.tothalo;a++) {
		if((a & 255)==0) har= R.bloha[a>>8];
		else har++;

		if( !(a % 256) && RE_local_test_break() ) break;  /* Hos, RPW - fix slow render bug, */
													/* !(loopvar%256) keeps checking for */
													/* ESC too often and bogging down render */
													/* (Based on discovery by Rob Haarsma) */
		if(ys>har->maxy);
		else if(ys<har->miny);
		else {
			minx= floor(har->xs-har->rad);
			maxx= ceil(har->xs+har->rad);
			
			if(maxx<0);
			else if(R.rectx<minx);
			else {
				if(minx<0) minx= 0;
				if(maxx>=R.rectx) maxx= R.rectx-1;

				rt= rectt+minx;
				rd= rectdelta+minx;
				rz= rectz+minx;

				yn= (ys-har->ys)*R.ycor;
				ysq= yn*yn;
				for(x=minx; x<=maxx; x++) {
					
					flarec= har->flarec;	/* har->pixels is inly allowd to count once */

					if( IS_A_POINTER_CODE(*rd)) {
						xn= x-har->xs;
						xsq= xn*xn;
						dist= xsq+ysq;
						if(dist<har->radsq) {
							ps= (PixStr *) POINTER_FROM_CODE(*rd);
							aantal= behind= 0;
							accol[0]=accol[1]=accol[2]=accol[3]= 0;
							while(ps) {
								aantalm= count_mask(ps->mask);
								aantal+= aantalm;

								zz= calchalo_z(har, ps->z);
								if(zz> har->zs) {
									*( (int *)col )= 0;
									RE_shadehalo(har, col, zz, dist, xn, yn, flarec);
									accol[0]+= aantalm*col[0];
									accol[1]+= aantalm*col[1];
									accol[2]+= aantalm*col[2];
									accol[3]+= aantalm*col[3];
									flarec= 0;
								}

								ps= ps->next;
							}
							ps= (PixStr *) POINTER_FROM_CODE(*rd);
							aantal= R.osa-aantal;
							
							zz= calchalo_z(har, *rz);
							if(zz> har->zs) {
								*( (int *)col )= 0;
								RE_shadehalo(har, col, zz, dist, xn, yn, flarec);
								accol[0]+= aantal*col[0];
								accol[1]+= aantal*col[1];
								accol[2]+= aantal*col[2];
								accol[3]+= aantal*col[3];
							}


							col[0]= accol[0]/R.osa;
							col[1]= accol[1]/R.osa;
							col[2]= accol[2]/R.osa;
							col[3]= accol[3]/R.osa;

							/* if(behind > (R.osa>>1)) addalphaUnder(rt,col); */
							RE_addalphaAddfac((char *)rt, (char *)col, har->add);
						}
					}
					else {
						zz= calchalo_z(har, *rz);
						if(zz> har->zs) {
							xn= x- har->xs;
							xsq= xn*xn;
							dist= xsq+ysq;
							if(dist<har->radsq) {
								RE_shadehalo(har, col, zz, dist, xn, yn, flarec);
								RE_addalphaAddfac((char *)rt, (char *)col, har->add);
							}
						}
					}
					rt++;
					rz++;
					rd++;
				}
				
			}
		}
	}

}

void scanlinehalo(unsigned int *rectz, unsigned int *rectt, short ys)
{
	HaloRen *har = NULL;
	float dist,xsq,ysq,xn,yn;
	unsigned int a, *rz, *rt, zz;
	short minx,maxx, x;
	char col[4];

	for(a=0; a<R.tothalo; a++) {
		if((a & 255)==0) har= R.bloha[a>>8];
		else har++;

		if( !(a % 256) && RE_local_test_break() ) break; /* Hos, RPW, fixes Slow Render Bug */

		if(ys>har->maxy);
		else if(ys<har->miny);
		else {
			minx= floor(har->xs-har->rad);
			maxx= ceil(har->xs+har->rad);
			
			if(maxx<0);
			else if(R.rectx<minx);
			else {
				if(minx<0) minx= 0;
				if(maxx>=R.rectx) maxx= R.rectx-1;

				rt= rectt+minx;
				rz= rectz+minx;

				yn= (ys-har->ys)*R.ycor;
				ysq= yn*yn;
				for(x=minx; x<=maxx; x++) {
				
					zz= calchalo_z(har, *rz);
					if(zz> har->zs) {
						xn= x- har->xs;
						xsq= xn*xn;
						dist= xsq+ysq;
						if(dist<har->radsq) {
							RE_shadehalo(har, col, zz, dist, xn, yn, har->flarec);
							RE_addalphaAddfac((char *)rt, (char *)col, har->add);
						}
					}

					rt++;
					rz++;
				}
			}
		}
	}

}

void halovert()
{
	HaloRen *har = NULL;
	float dist, xsq, ysq, xn, yn;
	unsigned int a, *rectz, *rz, *rectt, *rt, zz;
	int minx, maxx, miny, maxy, x, y;
	char col[4];


	for(a=0;a<R.tothalo;a++) {
		if((a & 255)==0) har= R.bloha[a>>8];
		else har++;

		if( !(a % 256) && RE_local_test_break() ) break; /* Hos, RPW, fixes slow render bug */

		if(har->maxy<0);
		else if(R.recty<har->miny);
		else {
			minx= floor(har->xs-har->rad);
			maxx= ceil(har->xs+har->rad);
			
			if(maxx<0);
			else if(R.rectx<minx);
			else {
			
				miny= floor(har->ys-har->rad);
				maxy= ceil(har->ys+har->rad);

				if(minx<0) minx= 0;
				if(maxx>=R.rectx) maxx= R.rectx-1;
				if(miny<0) miny= 0;
				if(maxy>R.recty) maxy= R.recty;

				rectt= R.rectot+ R.rectx*miny;
				rectz= R.rectz+ R.rectx*miny;

				for(y=miny;y<maxy;y++) {

					rz= rectz+minx;

					rt= (rectt+minx);

					yn= (y - har->ys)*R.ycor;
					ysq= yn*yn;
					for(x=minx;x<=maxx;x++) {
						
						zz= calchalo_z(har, *rz);
						
						if(zz> har->zs) {

							xn= x - har->xs;
							xsq= xn*xn;
							dist= xsq+ysq;
							if(dist<har->radsq) {
								RE_shadehalo(har, col, zz, dist, xn, yn, har->flarec);
								
								RE_addalphaAddfac((char *)rt, (char *)col, har->add);
								
							}
						}
						rt++;
						rz++;
					}

					rectt+= R.rectx;
					rectz+= R.rectx;
					
					if( !(y % 256) && RE_local_test_break() ) break; /* Hos,RPW, Fixes slow render bug */
				}

			}
		}
	}
}

/* ---------------- shaders ----------------------- */


float spec(float inp, int hard)	
{
	float b1;
	
	if(inp>=1.0) return 1.0;
	else if (inp<=0.0) return 0.0;
	
	b1= inp*inp;
	/* avoid FPE */
	if(b1<0.01) b1= 0.01;	
	
	if((hard & 1)==0)  inp= 1.0;
	if(hard & 2)  inp*= b1;
	b1*= b1;
	if(hard & 4)  inp*= b1;
	b1*= b1;
	if(hard & 8)  inp*= b1;
	b1*= b1;
	if(hard & 16) inp*= b1;
	b1*= b1;

	/* avoid FPE */
	if(b1<0.001) b1= 0.0;	

	if(hard & 32) inp*= b1;
	b1*= b1;
	if(hard & 64) inp*=b1;

	if(hard & 128) {
		b1*= b1;
		inp*=b1;
	}

	return inp;
}

float Phong_Spec( float *n, float *l, float *v, int hard )
{
	float h[3];
	float rslt;

	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	Normalise(h);

	rslt = h[0]*n[0] + h[1]*n[1] + h[2]*n[2];

	if( rslt > 0.0 ) rslt= spec(rslt, hard);
	else rslt = 0.0;

	return rslt;
}


/* reduced cook torrance spec (for off-specular peak) */
float CookTorr_Spec(float *n, float *l, float *v, int hard)
{
	float i, nh, nv, h[3];

	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	Normalise(h);

	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2];
	if(nh<0.0) return 0.0;
	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2];
	if(nv<0.0) nv= 0.0;
	i= spec(nh, hard);

	i= i/(0.1+nv);
	return i;
}

/* Blinn spec */
float Blinn_Spec(float *n, float *l, float *v, float refrac, float spec_power )
{
	float i, nh, nv, nl, vh, h[3];
	float a, b, c, g, p, f, ang;

	if(refrac < 1.0) return 0.0;
	if(spec_power == 0.0) return 0.0;
	
	/* conversion from 'hardness' (1-255) to 'spec_power' (50 maps at 0.1) */
	spec_power= sqrt(1.0/spec_power);
	
	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	Normalise(h);

	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector. */

	if(nh<0.0) return 0.0;

	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector. */

	if(nv<=0.0) nv= 0.01;

	nl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector. */

	if(nl<=0.0) {
		nl= 0.0;
		return 0.0;
	}

	vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; /* Dot product between view vector and half-way vector. */
	if(vh<=0.0) vh= 0.01;

	a = 1.0;
	b = (2.0*nh*nv)/vh;
	c = (2.0*nh*nl)/vh;

	if( a < b && a < c ) g = a;
	else if( b < a && b < c ) g = b;
	else if( c < a && c < b ) g = c;

	p = sqrt( (double)((refrac * refrac)+(vh*vh)-1.0) );
	f = (((p-vh)*(p-vh))/((p+vh)*(p+vh)))*(1+((((vh*(p+vh))-1.0)*((vh*(p+vh))-1.0))/(((vh*(p-vh))+1.0)*((vh*(p-vh))+1.0))));
	ang = (float)acos((double)(nh));

	i= f * g * exp((double)(-(ang*ang) / (2.0*spec_power*spec_power)));

	return i;
}

/* cartoon render spec */
float Toon_Spec( float *n, float *l, float *v, float size, float smooth )
{
	float h[3];
	float ang;
	float rslt;
	
	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	Normalise(h);
	
	rslt = h[0]*n[0] + h[1]*n[1] + h[2]*n[2];
	
	ang = acos( rslt ); 
	
	if( ang < size ) rslt = 1.0;
	else if( ang >= (size + smooth) || smooth == 0.0 ) rslt = 0.0;
	else rslt = 1.0 - ((ang - size) / smooth);
	
	return rslt;
}

/* cartoon render diffuse */
float Toon_Diff( float *n, float *l, float *v, float size, float smooth )
{
	float rslt, ang;

	rslt = n[0]*l[0] + n[1]*l[1] + n[2]*l[2];

	ang = acos( (double)(rslt) );

	if( ang < size ) rslt = 1.0;
	else if( ang >= (size + smooth) || smooth == 0.0 ) rslt = 0.0;
	else rslt = 1.0 - ((ang - size) / smooth);

	return rslt;
}

/* Oren Nayar diffuse */
float OrenNayar_Diff(float *n, float *l, float *v, float rough )
{
	float i, nh, nv, nl, vh, h[3];
	float a, b, t, A, B;
	float Lit_A, View_A, Lit_B[3], View_B[3];
	
	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	Normalise(h);
	
	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector. */
	if(nh<0.0) nh = 0.0;
	
	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector. */
	if(nv<=0.0) nv= 0.0;
	
	nl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector. */
	if(nl<0.0) nl= 0.0;
	
	vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; /* Dot product between view vector and halfway vector. */
	if(vh<=0.0) vh= 0.0;
	
	Lit_A = acos( nl );
	View_A = acos( nv );
	
	Lit_B[0] = l[0] - (nl * n[0]);
	Lit_B[1] = l[1] - (nl * n[1]);
	Lit_B[2] = l[2] - (nl * n[2]);
	Normalise( Lit_B );
	
	View_B[0] = v[0] - (nv * n[0]);
	View_B[1] = v[1] - (nv * n[1]);
	View_B[2] = v[2] - (nv * n[2]);
	Normalise( View_B );
	
	t = Lit_B[0]*View_B[0] + Lit_B[1]*View_B[1] + Lit_B[2]*View_B[2];
	if( t < 0 ) t = 0;
	
	if( Lit_A > View_A ) {
		a = Lit_A;
		b = View_A;
	}
	else {
		a = View_A;
		b = Lit_A;
	}
	
	A = 1 - (0.5 * ((rough * rough) / ((rough * rough) + 0.33)));
	B = 0.45 * ((rough * rough) / ((rough * rough) + 0.09));
	
	i = nl * ( A + ( B * t * sin(a) * tan(b) ) );
	
	return i;
}

/* --------------------------------------------- */

void RE_calc_R_ref()
{
	float i;

	/* R.vn dot R.view */
	i= -2*(R.vn[0]*R.view[0]+R.vn[1]*R.view[1]+R.vn[2]*R.view[2]);

	R.ref[0]= (R.view[0]+i*R.vn[0]);
	R.ref[1]= (R.view[1]+i*R.vn[1]);
	R.ref[2]= (R.view[2]+i*R.vn[2]);
	if(R.osatex) {
		if(R.vlr->flag & R_SMOOTH) {
			i= -2*( (R.vn[0]+O.dxno[0])*(R.view[0]+O.dxview) +
				(R.vn[1]+O.dxno[1])*R.view[1]+ (R.vn[2]+O.dxno[2])*R.view[2] );

			O.dxref[0]= R.ref[0]- ( R.view[0]+O.dxview+i*(R.vn[0]+O.dxno[0]));
			O.dxref[1]= R.ref[1]- (R.view[1]+ i*(R.vn[1]+O.dxno[1]));
			O.dxref[2]= R.ref[2]- (R.view[2]+ i*(R.vn[2]+O.dxno[2]));

			i= -2*( (R.vn[0]+O.dyno[0])*R.view[0]+
				(R.vn[1]+O.dyno[1])*(R.view[1]+O.dyview)+ (R.vn[2]+O.dyno[2])*R.view[2] );

			O.dyref[0]= R.ref[0]- (R.view[0]+ i*(R.vn[0]+O.dyno[0]));
			O.dyref[1]= R.ref[1]- (R.view[1]+O.dyview+i*(R.vn[1]+O.dyno[1]));
			O.dyref[2]= R.ref[2]- (R.view[2]+ i*(R.vn[2]+O.dyno[2]));

		}
		else {

			i= -2*( R.vn[0]*(R.view[0]+O.dxview) +
				R.vn[1]*R.view[1]+ R.vn[2]*R.view[2] );

			O.dxref[0]= R.ref[0]- (R.view[0]+O.dxview+i*R.vn[0]);
			O.dxref[1]= R.ref[1]- (R.view[1]+ i*R.vn[1]);
			O.dxref[2]= R.ref[2]- (R.view[2]+ i*R.vn[2]);

			i= -2*( R.vn[0]*R.view[0]+
				R.vn[1]*(R.view[1]+O.dyview)+ R.vn[2]*R.view[2] );

			O.dyref[0]= R.ref[0]- (R.view[0]+ i*R.vn[0]);
			O.dyref[1]= R.ref[1]- (R.view[1]+O.dyview+i*R.vn[1]);
			O.dyref[2]= R.ref[2]- (R.view[2]+ i*R.vn[2]);
		}
	}

}

void shade_lamp_loop()
{
	LampRen *lar;
	Material *ma;
	float i, inp, inpr, t, lv[3], lampdist, ld = 0, ir, ig, ib, isr=0,isg=0,isb=0;
	float lvrot[3], *vn, *view, shadfac, soft;
	int a;

	vn= R.vn;
	view= R.view;
	ma= R.matren;
	
	/* separate loop */
	if(ma->mode & MA_ONLYSHADOW) {
		shadfac= ir= 0.0;
		for(a=0; a<R.totlamp; a++) {
			lar= R.la[a];
			
			if(lar->mode & LA_LAYER) if((lar->lay & R.vlr->lay)==0) continue;
			
			if(lar->shb) {
				/* only test within spotbundel */
				lv[0]= R.co[0]-lar->co[0];
				lv[1]= R.co[1]-lar->co[1];
				lv[2]= R.co[2]-lar->co[2];
				Normalise(lv);
				inpr= lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2];
				if(inpr>lar->spotsi) {
					
					inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
					
					i = testshadowbuf(lar->shb, inp);
					
					t= inpr - lar->spotsi;
					if(t<lar->spotbl && lar->spotbl!=0.0) {
						t/= lar->spotbl;
						t*= t;
						i= t*i+(1.0-t);
					}
					
					shadfac+= i;
					ir+= 1.0;
				}
				else {
					shadfac+= 1.0;
					ir+= 1.0;
				}
			}
		}
		if(ir>0.0) shadfac/= ir;
		ma->alpha= (R.mat->alpha)*(1.0-shadfac);
		
		shortcol[0]=shortcol[1]=shortcol[2]= 0;
		
		return;
	}
		
	if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
		ma->r= R.vcol[0];
		ma->g= R.vcol[1];
		ma->b= R.vcol[2];
	}

	/* mirror reflection colour */
	R.refcol[0]= R.refcol[1]= R.refcol[2]= R.refcol[3]= 0.0;

	if(ma->texco) {

		if(ma->texco & TEXCO_REFL) {
			RE_calc_R_ref();
		}
		
		if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
			R.mat->r= R.vcol[0];
			R.mat->g= R.vcol[1];
			R.mat->b= R.vcol[2];
		}

		do_material_tex();
	}
	
	if(ma->mode & MA_SHLESS) {
		if( (ma->mode & (MA_VERTEXCOL+MA_VERTEXCOLP+MA_FACETEXTURE) )) {
			ir= R.vcol[0]*ma->r;
			ig= R.vcol[1]*ma->g;
			ib= R.vcol[2]*ma->b;
		}
		else {
			ir= ma->r;
			ig= ma->g;
			ib= ma->b;
		}
	
		if(usegamtab) {
			a= 65535*ir;
			if(a<0) a=0; else if(a>65535) a= 65535;
			shortcol[0]= igamtab2[a];
			a= 65535*ig;
			if(a<0) a=0; else if(a>65535) a= 65535;
			shortcol[1]= igamtab2[a];
			a= 65535*ib;
			if(a<0) a=0; else if(a>65535) a= 65535;
			shortcol[2]= igamtab2[a];
		}
		else {
			a= 65535*ir;
			if(a<0) shortcol[0]= 0; else if(a>65535) shortcol[0]= 65535; else shortcol[0]= a;
			a= 65535*ig;
			if(a<0) shortcol[1]= 0; else if(a>65535) shortcol[1]= 65535; else shortcol[1]= a;
			a= 65535*ib;
			if(a<0) shortcol[2]= 0; else if(a>65535) shortcol[2]= 65535; else shortcol[2]= a;
		}
		return;
	}

	if( (ma->mode & (MA_VERTEXCOL+MA_VERTEXCOLP))== MA_VERTEXCOL ) {
		ir= ma->emit+R.vcol[0];
		ig= ma->emit+R.vcol[1];
		ib= ma->emit+R.vcol[2];
	}
	else ir= ig= ib= ma->emit;

	for(a=0; a<R.totlamp; a++) {
		lar= R.la[a];

		/* test for lamp layer */
		if(lar->mode & LA_LAYER) if((lar->lay & R.vlr->lay)==0) continue;
		
		/* lampdist calculation */
		if(lar->type==LA_SUN || lar->type==LA_HEMI) {
			VECCOPY(lv, lar->vec);
			lampdist= 1.0;
		}
		else {
			lv[0]= R.co[0]-lar->co[0];
			lv[1]= R.co[1]-lar->co[1];
			lv[2]= R.co[2]-lar->co[2];
			ld= sqrt(lv[0]*lv[0]+lv[1]*lv[1]+lv[2]*lv[2]);
			lv[0]/= ld;
			lv[1]/= ld;
			lv[2]/= ld;
			
			/* ld is re-used further on (texco's) */
			
			if(lar->mode & LA_QUAD) {
				t= 1.0;
				if(lar->ld1>0.0)
					t= lar->dist/(lar->dist+lar->ld1*ld);
				if(lar->ld2>0.0)
					t*= lar->distkw/(lar->distkw+lar->ld2*ld*ld);

				lampdist= t;
			}
			else {
				lampdist= (lar->dist/(lar->dist+ld));
			}

			if(lar->mode & LA_SPHERE) {
				t= lar->dist - ld;
				if(t<0.0) continue;
				
				t/= lar->dist;
				lampdist*= (t);
			}
			
		}
		
		if(lar->mode & LA_TEXTURE)  do_lamp_tex(lar, lv);

		if(lar->type==LA_SPOT) {
			
			if(lar->mode & LA_SQUARE) {
				if(lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2]>0.0) {
					float x;
					
					/* rotate view to lampspace */
					VECCOPY(lvrot, lv);
					MTC_Mat3MulVecfl(lar->imat, lvrot);
					
					x= MAX2(fabs(lvrot[0]/lvrot[2]) , fabs(lvrot[1]/lvrot[2]));
					/* 1.0/(sqrt(1+x*x)) is equivalent to cos(atan(x)) */

					inpr= 1.0/(sqrt(1+x*x));
				}
				else inpr= 0.0;
			}
			else {
				inpr= lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2];
			}

			t= lar->spotsi;
			if(inpr<t) continue;
			else {
				t= inpr-t;
				i= 1.0;
				soft= 1.0;
				if(t<lar->spotbl && lar->spotbl!=0.0) {
					/* soft area */
					i= t/lar->spotbl;
					t= i*i;
					soft= (3.0*t-2.0*t*i);
					inpr*= soft;
				}
				if((lar->mode & LA_ONLYSHADOW) && lar->shb) {
					if(ma->mode & MA_SHADOW) {
						/* dot product positive: front side face! */
						inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
						if(inp>0.0) {
							/* testshadowbuf==0.0 : 100% shadow */
							shadfac = 1.0 - testshadowbuf(lar->shb, inp);
							if(shadfac>0.0) {
								shadfac*= inp*soft*lar->energy;
								ir -= shadfac;
								ig -= shadfac;
								ib -= shadfac;
								
								continue;
							}
						}
					}
				}
				lampdist*=inpr;
			}
			if(lar->mode & LA_ONLYSHADOW) continue;

			if(lar->mode & LA_OSATEX) {
				R.osatex= 1;	/* signal for multitex() */
				
				O.dxlv[0]= lv[0] - (R.co[0]-lar->co[0]+O.dxco[0])/ld;
				O.dxlv[1]= lv[1] - (R.co[1]-lar->co[1]+O.dxco[1])/ld;
				O.dxlv[2]= lv[2] - (R.co[2]-lar->co[2]+O.dxco[2])/ld;

				O.dylv[0]= lv[0] - (R.co[0]-lar->co[0]+O.dyco[0])/ld;
				O.dylv[1]= lv[1] - (R.co[1]-lar->co[1]+O.dyco[1])/ld;
				O.dylv[2]= lv[2] - (R.co[2]-lar->co[2]+O.dyco[2])/ld;
			}
			
		}

		/* dot product and reflectivity*/
		inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
		
		if(lar->mode & LA_NO_DIFF) {
			i= 0.0;	// skip shaders
		}
		else if(lar->type==LA_HEMI) {
			i= 0.5*inp + 0.5;
		}
		else {
			/* diffuse shaders */
			if(ma->diff_shader==MA_DIFF_ORENNAYAR) i= OrenNayar_Diff(vn, lv, view, ma->roughness);
			else if(ma->diff_shader==MA_DIFF_TOON) i= Toon_Diff(vn, lv, view, ma->param[0], ma->param[1]);
			else i= inp;	// Lambert
		}
		
		if(i>0.0) {
			i*= lampdist*ma->ref;
		}

		/* shadow and spec */
		if(inp> -0.41) {			/* heuristic value */
			shadfac= 1.0;
			if(lar->shb) {
				if(ma->mode & MA_SHADOW) {
					shadfac = testshadowbuf(lar->shb, inp);
					if(shadfac==0.0) continue;
					i*= shadfac;
				}
			}
			/* specularity */
			
			if(ma->spec!=0.0 && !(lar->mode & LA_NO_SPEC)) {
				
				if(lar->type==LA_HEMI) {
					/* hemi uses no spec shaders (yet) */
					
					lv[0]+= view[0];
					lv[1]+= view[1];
					lv[2]+= view[2];
					
					Normalise(lv);
					
					t= vn[0]*lv[0]+vn[1]*lv[1]+vn[2]*lv[2];
					
					if(lar->type==LA_HEMI) {
						t= 0.5*t+0.5;
					}
					
					t= ma->spec*spec(t, ma->har);
					isr+= t*(lar->r * ma->specr);
					isg+= t*(lar->g * ma->specg);
					isb+= t*(lar->b * ma->specb);
				}
				else {
					/* specular shaders */
					float specfac;

					/* we force a different lamp vector for sun light */
					if(lar->type==LA_SUN) lv[2]-= 1.0;
										
					if(ma->spec_shader==MA_SPEC_PHONG) 
						specfac= Phong_Spec(vn, lv, view, ma->har);
					else if(ma->spec_shader==MA_SPEC_COOKTORR) 
						specfac= CookTorr_Spec(vn, lv, view, ma->har);
					else if(ma->spec_shader==MA_SPEC_BLINN) 
						specfac= Blinn_Spec(vn, lv, view, ma->refrac, (float)ma->har);
					else 
						specfac= Toon_Spec(vn, lv, view, ma->param[2], ma->param[3]);
					
					t= shadfac*ma->spec*lampdist*specfac;
					
					isr+= t*(lar->r * ma->specr);
					isg+= t*(lar->g * ma->specg);
					isb+= t*(lar->b * ma->specb);
				}
			}
		}
		/* in case 'no diffuse' we still do most calculus, spec can be in shadow */
		if(i>0.0 && !(lar->mode & LA_NO_DIFF)) {
			ir+= i*lar->r;
			ig+= i*lar->g;
			ib+= i*lar->b;
		}
	}
	
	if(ir<0.0) ir= 0.0;
	if(ig<0.0) ig= 0.0;
	if(ib<0.0) ib= 0.0;
	if(isr<0.0) isr= 0.0;
	if(isg<0.0) isg= 0.0;
	if(isb<0.0) isb= 0.0;

	if(ma->mode & MA_ZTRA) {	/* ztra shade */
		if(ma->spectra!=0.0) {

			t = MAX3(isr, isb, isg);
			t *= ma->spectra;
			if(t>1.0) t= 1.0;
			if(ma->mapto & MAP_ALPHA) ma->alpha= (1.0-t)*ma->alpha+t;
			else ma->alpha= (1.0-t)*R.mat->alpha+t;
		}
	}
		
	if(R.refcol[0]==0.0) {
		a= 65535.0*( ma->r*ir +ma->ambr +isr);
		if(a>65535) a=65535; else if(a<0) a= 0;
		shortcol[0]= a;
		a= 65535.0*(ma->g*ig +ma->ambg +isg);
		if(a>65535) a=65535; else if(a<0) a= 0;
		shortcol[1]= a;
		a= 65535*(ma->b*ib +ma->ambb +isb);
		if(a>65535) a=65535; else if(a<0) a= 0;
		shortcol[2]= a;
	}
	else {
		a= 65535.0*( ma->mirr*R.refcol[1] + (1.0 - ma->mirr*R.refcol[0])*(ma->r*ir +ma->ambr) +isr);
		if(a>65535) a=65535; else if(a<0) a= 0;
		shortcol[0]= a;
		a= 65535.0*( ma->mirg*R.refcol[2] + (1.0 - ma->mirg*R.refcol[0])*(ma->g*ig +ma->ambg) +isg);
		if(a>65535) a=65535; else if(a<0) a= 0;
		shortcol[1]= a;
		a= 65535.0*( ma->mirb*R.refcol[3] + (1.0 - ma->mirb*R.refcol[0])*(ma->b*ib +ma->ambb) +isb);
		if(a>65535) a=65535; else if(a<0) a= 0;
		shortcol[2]= a;
	}

	if(usegamtab) {
		shortcol[0]= igamtab2[ shortcol[0] ];
		shortcol[1]= igamtab2[ shortcol[1] ];
		shortcol[2]= igamtab2[ shortcol[2] ];
	}
}


void shadepixel(float x, float y, int vlaknr)
  /* x,y: window coordinate from 0 to rectx,y */
{
	static VlakRen *vlr;
	static VertRen *v1, *v2, *v3;
	static float t00, t01, t10, t11, dvlak, n1[3], n2[3], n3[3];
	static float s00, s01, s10, s11;
	float *o1, *o2, *o3;
	float u, v, l, dl, hox, hoy, detsh, fac, deler, alpha;
	char *cp1, *cp2, *cp3;
	
	if(R.vlaknr== -1) {	/* does initrender */
		vlr= R.vlr= 0;
	}
	
	if(vlaknr<=0) {	/* sky */
		R.vlaknr= 0;
		shortcol[3]= 0;
	}
	else if( (vlaknr & 0x7FFFFF) <= R.totvlak) {

		if(vlaknr!=R.vlaknr) {
			vlr= RE_findOrAddVlak( (vlaknr-1) & 0x7FFFFF);

			R.mat= vlr->mat;
			R.matren= R.mat->ren;

			if(R.matren==0) {	/* for debug */
				shortcol[3]= 65535;
				shortcol[2]= 0;
				shortcol[1]= 65535;
				shortcol[0]= 65535;
				return;
			}
			
			R.vlr= vlr;

			R.vno= vlr->n;
			R.osatex= (R.matren->texco & TEXCO_OSA);
			R.vlaknr= vlaknr;

			v1= vlr->v1;
			dvlak= v1->co[0]*vlr->n[0]+v1->co[1]*vlr->n[1]+v1->co[2]*vlr->n[2];

			if( (vlr->flag & R_SMOOTH) || (R.matren->texco & NEED_UV)) {	/* uv needed */
				if(vlaknr & 0x800000) {
					v2= vlr->v3;
					v3= vlr->v4;
				}
				else {
					v2= vlr->v2;
					v3= vlr->v3;
				}
				
				if(vlr->snproj==0) {
					t00= v3->co[0]-v1->co[0]; t01= v3->co[1]-v1->co[1];
					t10= v3->co[0]-v2->co[0]; t11= v3->co[1]-v2->co[1];
				}
				else if(vlr->snproj==1) {
					t00= v3->co[0]-v1->co[0]; t01= v3->co[2]-v1->co[2];
					t10= v3->co[0]-v2->co[0]; t11= v3->co[2]-v2->co[2];
				}
				else {
					t00= v3->co[1]-v1->co[1]; t01= v3->co[2]-v1->co[2];
					t10= v3->co[1]-v2->co[1]; t11= v3->co[2]-v2->co[2];
				}
				
				detsh= t00*t11-t10*t01;
				t00/= detsh; t01/=detsh; 
				t10/=detsh; t11/=detsh;
				
				if(vlr->flag & R_SMOOTH) { /* adjust punos (vertexnormals) */
					if(vlr->puno & ME_FLIPV1) {
						n1[0]= -v1->n[0]; n1[1]= -v1->n[1]; n1[2]= -v1->n[2];
					} else {
						n1[0]= v1->n[0]; n1[1]= v1->n[1]; n1[2]= v1->n[2];
					}
					if(vlaknr & 0x800000) {
						if(vlr->puno & ME_FLIPV3) {
							n2[0]= -v2->n[0]; n2[1]= -v2->n[1]; n2[2]= -v2->n[2];
						} else {
							n2[0]= v2->n[0]; n2[1]= v2->n[1]; n2[2]= v2->n[2];
						}
						
						if(vlr->puno & ME_FLIPV4) {
							n3[0]= -v3->n[0]; n3[1]= -v3->n[1]; n3[2]= -v3->n[2];
						} else {
							n3[0]= v3->n[0]; n3[1]= v3->n[1]; n3[2]= v3->n[2];
						}
						
					}
					else {
						if(vlr->puno & ME_FLIPV2) {
							n2[0]= -v2->n[0]; n2[1]= -v2->n[1]; n2[2]= -v2->n[2];
						} else {
							n2[0]= v2->n[0]; n2[1]= v2->n[1]; n2[2]= v2->n[2];
						}
						
						if(vlr->puno & ME_FLIPV3) {
							n3[0]= -v3->n[0]; n3[1]= -v3->n[1]; n3[2]= -v3->n[2];
						} else {
							n3[0]= v3->n[0]; n3[1]= v3->n[1]; n3[2]= v3->n[2];
						}
					}
				}
				if(R.matren->texco & TEXCO_STICKY) {
					s00= v3->ho[0]/v3->ho[3] - v1->ho[0]/v1->ho[3];
					s01= v3->ho[1]/v3->ho[3] - v1->ho[1]/v1->ho[3];
					s10= v3->ho[0]/v3->ho[3] - v2->ho[0]/v2->ho[3];
					s11= v3->ho[1]/v3->ho[3] - v2->ho[1]/v2->ho[3];
					
					detsh= s00*s11-s10*s01;
					s00/= detsh; s01/=detsh; 
					s10/=detsh; s11/=detsh;
				}
			}
		}

		/* COXYZ  */
		if( (G.special1 & G_HOLO) && ((Camera *)G.scene->camera->data)->flag & CAM_HOLO2) {
			R.view[0]= (x+(R.xstart)+1.0+holoofs);
		}
		else {
			R.view[0]= (x+(R.xstart)+1.0);
		}

		if(R.flag & R_SEC_FIELD) {
			if(R.r.mode & R_ODDFIELD) R.view[1]= (y+R.ystart+0.5)*R.ycor;
			else R.view[1]= (y+R.ystart+1.5)*R.ycor;
		}
		else R.view[1]= (y+R.ystart+1.0)*R.ycor;
		
		R.view[2]= -R.viewfac;

		if(R.r.mode & R_PANORAMA) {
			float panoco, panosi;
			panoco = getPanovCo();
			panosi = getPanovSi();

			u= R.view[0]; v= R.view[2];
			R.view[0]= panoco*u + panosi*v;
			R.view[2]= -panosi*u + panoco*v;
		}

		deler= vlr->n[0]*R.view[0] + vlr->n[1]*R.view[1] + vlr->n[2]*R.view[2];
		if (deler!=0.0) fac= R.zcor= dvlak/deler;
		else fac= R.zcor= 0.0;
		
		R.co[0]= fac*R.view[0];
		R.co[1]= fac*R.view[1];
		R.co[2]= fac*R.view[2];
		
		if(R.osatex || (R.r.mode & R_SHADOW) ) {
			u= dvlak/(deler-vlr->n[0]);
			v= dvlak/(deler- R.ycor*vlr->n[1]);

			O.dxco[0]= R.co[0]- (R.view[0]-1.0)*u;
			O.dxco[1]= R.co[1]- (R.view[1])*u;
			O.dxco[2]= R.co[2]- (R.view[2])*u;

			O.dyco[0]= R.co[0]- (R.view[0])*v;
			O.dyco[1]= R.co[1]- (R.view[1]-1.0*R.ycor)*v;
			O.dyco[2]= R.co[2]- (R.view[2])*v;

		}

		fac= Normalise(R.view);
		R.zcor*= fac;	/* for mist */
		
		if(R.osatex) {
			if( (R.matren->texco & TEXCO_REFL) ) {
				O.dxview= 1.0/fac;
				O.dyview= R.ycor/fac;
			}
		}

		/* UV and TEX*/
		if( (vlr->flag & R_SMOOTH) || (R.matren->texco & NEED_UV)) {
			if(vlr->snproj==0) {
				u= (R.co[0]-v3->co[0])*t11-(R.co[1]-v3->co[1])*t10;
				v= (R.co[1]-v3->co[1])*t00-(R.co[0]-v3->co[0])*t01;
				if(R.osatex) {
					O.dxuv[0]=  O.dxco[0]*t11- O.dxco[1]*t10;
					O.dxuv[1]=  O.dxco[1]*t00- O.dxco[0]*t01;
					O.dyuv[0]=  O.dyco[0]*t11- O.dyco[1]*t10;
					O.dyuv[1]=  O.dyco[1]*t00- O.dyco[0]*t01;
				}
			}
			else if(vlr->snproj==1) {
				u= (R.co[0]-v3->co[0])*t11-(R.co[2]-v3->co[2])*t10;
				v= (R.co[2]-v3->co[2])*t00-(R.co[0]-v3->co[0])*t01;
				if(R.osatex) {
					O.dxuv[0]=  O.dxco[0]*t11- O.dxco[2]*t10;
					O.dxuv[1]=  O.dxco[2]*t00- O.dxco[0]*t01;
					O.dyuv[0]=  O.dyco[0]*t11- O.dyco[2]*t10;
					O.dyuv[1]=  O.dyco[2]*t00- O.dyco[0]*t01;
				}
			}
			else {
				u= (R.co[1]-v3->co[1])*t11-(R.co[2]-v3->co[2])*t10;
				v= (R.co[2]-v3->co[2])*t00-(R.co[1]-v3->co[1])*t01;
				if(R.osatex) {
					O.dxuv[0]=  O.dxco[1]*t11- O.dxco[2]*t10;
					O.dxuv[1]=  O.dxco[2]*t00- O.dxco[1]*t01;
					O.dyuv[0]=  O.dyco[1]*t11- O.dyco[2]*t10;
					O.dyuv[1]=  O.dyco[2]*t00- O.dyco[1]*t01;
				}
			}
			l= 1.0+u+v;

			if(vlr->flag & R_SMOOTH) {
				R.vn[0]= l*n3[0]-u*n1[0]-v*n2[0];
				R.vn[1]= l*n3[1]-u*n1[1]-v*n2[1];
				R.vn[2]= l*n3[2]-u*n1[2]-v*n2[2];

				Normalise(R.vn);
				if(R.osatex && (R.matren->texco & (TEXCO_NORM+TEXCO_REFL)) ) {
               dl= O.dxuv[0]+O.dxuv[1];
					O.dxno[0]= dl*n3[0]-O.dxuv[0]*n1[0]-O.dxuv[1]*n2[0];
					O.dxno[1]= dl*n3[1]-O.dxuv[0]*n1[1]-O.dxuv[1]*n2[1];
					O.dxno[2]= dl*n3[2]-O.dxuv[0]*n1[2]-O.dxuv[1]*n2[2];
					dl= O.dyuv[0]+O.dyuv[1];
					O.dyno[0]= dl*n3[0]-O.dyuv[0]*n1[0]-O.dyuv[1]*n2[0];
					O.dyno[1]= dl*n3[1]-O.dyuv[0]*n1[1]-O.dyuv[1]*n2[1];
					O.dyno[2]= dl*n3[2]-O.dyuv[0]*n1[2]-O.dyuv[1]*n2[2];

				}
			}
			else {
				VECCOPY(R.vn, vlr->n);
			}

			if(R.matren->mode & MA_ZINV) {	/* z invert */
				/* R.vn[0]= -R.vn[0]; */
				/* R.vn[1]= -R.vn[1]; */
			}

			if(R.matren->texco & TEXCO_ORCO) {
				if(v2->orco) {
					o1= v1->orco;
					o2= v2->orco;
					o3= v3->orco;
					
					R.lo[0]= l*o3[0]-u*o1[0]-v*o2[0];
					R.lo[1]= l*o3[1]-u*o1[1]-v*o2[1];
					R.lo[2]= l*o3[2]-u*o1[2]-v*o2[2];

					if(R.osatex) {
						dl= O.dxuv[0]+O.dxuv[1];
						O.dxlo[0]= dl*o3[0]-O.dxuv[0]*o1[0]-O.dxuv[1]*o2[0];
						O.dxlo[1]= dl*o3[1]-O.dxuv[0]*o1[1]-O.dxuv[1]*o2[1];
						O.dxlo[2]= dl*o3[2]-O.dxuv[0]*o1[2]-O.dxuv[1]*o2[2];
						dl= O.dyuv[0]+O.dyuv[1];
						O.dylo[0]= dl*o3[0]-O.dyuv[0]*o1[0]-O.dyuv[1]*o2[0];
						O.dylo[1]= dl*o3[1]-O.dyuv[0]*o1[1]-O.dyuv[1]*o2[1];
						O.dylo[2]= dl*o3[2]-O.dyuv[0]*o1[2]-O.dyuv[1]*o2[2];
					}
				}
			}
			
			if(R.matren->texco & TEXCO_GLOB) {
				VECCOPY(R.gl, R.co);
				MTC_Mat4MulVecfl(R.viewinv, R.gl);
				if(R.osatex) {
					VECCOPY(O.dxgl, O.dxco);
					MTC_Mat3MulVecfl(R.imat, O.dxco);
					VECCOPY(O.dygl, O.dyco);
					MTC_Mat3MulVecfl(R.imat, O.dyco);
				}
			}
			if((R.matren->texco & TEXCO_UV) || (R.matren->mode & (MA_VERTEXCOL|MA_FACETEXTURE)))  {
				if(R.vlr->tface) {
					float *uv1, *uv2, *uv3;
					
					uv1= R.vlr->tface->uv[0];
					if( (vlaknr & 0x800000) || (R.vlr->flag & R_FACE_SPLIT) ) {
						uv2= R.vlr->tface->uv[2];
						uv3= R.vlr->tface->uv[3];
					}
					else {
						uv2= R.vlr->tface->uv[1];
						uv3= R.vlr->tface->uv[2];
					}
					
					R.uv[0]= -1.0 + 2.0*(l*uv3[0]-u*uv1[0]-v*uv2[0]);
					R.uv[1]= -1.0 + 2.0*(l*uv3[1]-u*uv1[1]-v*uv2[1]);

					if(R.osatex) {
						float duv[2];
						
						dl= O.dxuv[0]+O.dxuv[1];
						duv[0]= O.dxuv[0]; 
						duv[1]= O.dxuv[1];
						
						O.dxuv[0]= 2.0*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
						O.dxuv[1]= 2.0*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);

						dl= O.dyuv[0]+O.dyuv[1];
						duv[0]= O.dyuv[0]; 
						duv[1]= O.dyuv[1];

						O.dyuv[0]= 2.0*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
						O.dyuv[1]= 2.0*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);
					}
					
				}
				else {
					R.uv[0]= 2.0*(u+.5);
					R.uv[1]= 2.0*(v+.5);
				}
			}
			if(R.matren->texco & TEXCO_NORM) {
				R.orn[0]= R.vn[0];
				R.orn[1]= -R.vn[1];
				R.orn[2]= R.vn[2];
			}
			if(R.matren->mode & MA_VERTEXCOL) {
				cp1= (char *)vlr->vcol;
				if(cp1) {
					if( (vlaknr & 0x800000) || (R.vlr->flag & R_FACE_SPLIT) ) {
						cp2= (char *)(vlr->vcol+2);
						cp3= (char *)(vlr->vcol+3);
					}
					else {
						cp2= (char *)(vlr->vcol+1);
						cp3= (char *)(vlr->vcol+2);
					}
					R.vcol[0]= (l*cp3[3]-u*cp1[3]-v*cp2[3])/255.0;
					R.vcol[1]= (l*cp3[2]-u*cp1[2]-v*cp2[2])/255.0;
					R.vcol[2]= (l*cp3[1]-u*cp1[1]-v*cp2[1])/255.0;
					
				}
				else {
					R.vcol[0]= 0.0;
					R.vcol[1]= 0.0;
					R.vcol[2]= 0.0;
				}
			}
			if(R.matren->mode & MA_FACETEXTURE) {
				if((R.matren->mode & MA_VERTEXCOL)==0) {
					R.vcol[0]= 1.0;
					R.vcol[1]= 1.0;
					R.vcol[2]= 1.0;
				}
				if(vlr->tface) render_realtime_texture();
			}
			
			
			/* after this the u and v AND O.dxuv and O.dyuv are incorrect */
			if(R.matren->texco & TEXCO_STICKY) {
				if(v2->sticky) {
					
					/* recalc u and v again */
					hox= x/Zmulx -1.0;
					hoy= y/Zmuly -1.0;
					u= (hox - v3->ho[0]/v3->ho[3])*s11 - (hoy - v3->ho[1]/v3->ho[3])*s10;
					v= (hoy - v3->ho[1]/v3->ho[3])*s00 - (hox - v3->ho[0]/v3->ho[3])*s01;
					l= 1.0+u+v;
					
					o1= v1->sticky;
					o2= v2->sticky;
					o3= v3->sticky;
					
					R.sticky[0]= l*o3[0]-u*o1[0]-v*o2[0];
					R.sticky[1]= l*o3[1]-u*o1[1]-v*o2[1];

					if(R.osatex) {
						O.dxuv[0]=  s11/Zmulx;
						O.dxuv[1]=  - s01/Zmulx;
						O.dyuv[0]=  - s10/Zmuly;
						O.dyuv[1]=  s00/Zmuly;
						
						dl= O.dxuv[0]+O.dxuv[1];
						O.dxsticky[0]= dl*o3[0]-O.dxuv[0]*o1[0]-O.dxuv[1]*o2[0];
						O.dxsticky[1]= dl*o3[1]-O.dxuv[0]*o1[1]-O.dxuv[1]*o2[1];
						dl= O.dyuv[0]+O.dyuv[1];
						O.dysticky[0]= dl*o3[0]-O.dyuv[0]*o1[0]-O.dyuv[1]*o2[0];
						O.dysticky[1]= dl*o3[1]-O.dyuv[0]*o1[1]-O.dyuv[1]*o2[1];
					}
				}
			}
		}
		else {
			VECCOPY(R.vn, vlr->n);
		}
		if(R.matren->texco & TEXCO_WINDOW) {
			R.winco[0]= (x+(R.xstart))/(float)R.afmx;
			R.winco[1]= (y+(R.ystart))/(float)R.afmy;
		}
	

		shade_lamp_loop();

		/* MIST */
		if( (R.wrld.mode & WO_MIST) && (R.matren->mode & MA_NOMIST)==0 ){
			alpha= mistfactor(R.co);
		}
		else alpha= 1.0;

		/* RAYTRACE WAS HERE! */

		if(R.matren->alpha!=1.0 || alpha!=1.0) {
			fac= alpha*(R.matren->alpha);
			
				/* gamma */
			if(R.osa && usegamtab) fac*= fac;
			
			shortcol[3]= 65535.0*fac;
			shortcol[0]*= fac;
			shortcol[1]*= fac;
			shortcol[2]*= fac;
		}
		else {
			shortcol[3]= 65535;
		}
	}
	else {
		shortcol[0]= 65535;
		shortcol[1]= 65535;
		shortcol[2]= 0;
		shortcol[3]= 65535;
	}
	
	if(R.flag & R_LAMPHALO) {
		if(vlaknr<=0) {	/* calc view vector and put R.co at far */
		
			if( (G.special1 & G_HOLO) && ((Camera *)G.scene->camera->data)->flag & CAM_HOLO2) {
				R.view[0]= (x+(R.xstart)+1.0+holoofs);
			}
			else {
				R.view[0]= (x+(R.xstart)+1.0);
			}

			if(R.flag & R_SEC_FIELD) {
				if(R.r.mode & R_ODDFIELD) R.view[1]= (y+R.ystart+0.5)*R.ycor;
				else R.view[1]= (y+R.ystart+1.5)*R.ycor;
			}
			else R.view[1]= (y+R.ystart+1.0)*R.ycor;
			
			R.view[2]= -R.viewfac;
			
			if(R.r.mode & R_PANORAMA) {
				float panoco, panosi;
				panoco = getPanovCo();
				panosi = getPanovSi();
				
				u= R.view[0]; v= R.view[2];
				R.view[0]= panoco*u + panosi*v;
				R.view[2]= -panosi*u + panoco*v;
			}

			R.co[2]= 0.0;
			
		}
		renderspothalo(shortcol);
	}
}

PixStr *addpsmain()
{
	PixStrMain *psm;

	psm= &psmfirst;

	while(psm->next) {
		psm= psm->next;
	}

	psm->next= (PixStrMain *)MEM_mallocN(sizeof(PixStrMain),"pixstrMain");

	psm= psm->next;
	psm->next=0;
	psm->ps= (PixStr *)MEM_mallocN(4096*sizeof(PixStr),"pixstr");
	psmteller= 0;

	return psm->ps;
}

void freeps()
{
	PixStrMain *psm,*next;

	psm= &psmfirst;

	while(psm) {
		next= psm->next;
		if(psm->ps) {
			MEM_freeN(psm->ps);
			psm->ps= 0;
		}
		if(psm!= &psmfirst) MEM_freeN(psm);
		psm= next;
	}

	psmfirst.next= 0;
	psmfirst.ps= 0;
}

void addps(long *rd, int vlak, unsigned int z, short ronde)
{
	static PixStr *prev;
	PixStr *ps, *last = NULL;
	int vlakand;

	if( IS_A_POINTER_CODE(*rd)) {	
		ps= (PixStr *) POINTER_FROM_CODE(*rd);
		vlakand= (vlak & 0x7FFFFF);
		
		if( (ps->vlak0 & 0x7FFFFF) == vlakand ) return;
		
		while(ps) {
			if( (ps->vlak & 0x7FFFFF) == vlakand ) {
				ps->mask |= (1<<ronde);
				return;
			}
			last= ps;
			ps= ps->next;
		}

		if((psmteller & 4095)==0) prev= addpsmain();
		else prev++;
		psmteller++;

		last->next= prev;
		prev->next= 0;
		prev->vlak= vlak;
		prev->z= z;
		prev->mask = (1<<ronde);
		prev->ronde= ronde;
		
		return;
	}

	/* make first PS (pixel struct) */
	if((psmteller & 4095)==0) prev= addpsmain();
	else prev++;
	psmteller++;

	prev->next= 0;
	prev->vlak0= (int) *rd;
	prev->vlak= vlak;
	prev->z= z;
	prev->mask = (1<<ronde);
	prev->ronde= ronde;
	*rd= POINTER_TO_CODE(prev);
}


int count_mask(unsigned short mask)
{
	return (cmask[mask & 255]+cmask[mask>>8]);
}

float count_maskf(unsigned short mask)
{
	return (fmask[mask & 255]+fmask[mask>>8]);
}


void add_filt_mask(unsigned int mask, unsigned short *col, unsigned int *rb1, unsigned int *rb2, unsigned int *rb3)
{
	/* calc the value of mask */
	unsigned int a, maskand, maskshift;
	int j;
	unsigned short val, r, g, b, al;
	
	al= col[3];
	r= col[0];
	g= col[1];
	b= col[2];

	maskand= (mask & 255);
	maskshift= (mask >>8);

	for(j=2; j>=0; j--) {

		a= j;

		val= *(mask1[a] +maskand) + *(mask2[a] +maskshift);
		if(val) {
			rb1[3]+= val*al;
			rb1[0]+= val*r;
			rb1[1]+= val*g;
			rb1[2]+= val*b;
		}
		a+=3;

		val= *(mask1[a] +maskand) + *(mask2[a] +maskshift);
		if(val) {
			rb2[3]+= val*al;
			rb2[0]+= val*r;
			rb2[1]+= val*g;
			rb2[2]+= val*b;
		}
		a+=3;

		val= *(mask1[a] +maskand) + *(mask2[a] +maskshift);
		if(val) {
			rb3[3]+= val*al;
			rb3[0]+= val*r;
			rb3[1]+= val*g;
			rb3[2]+= val*b;
		}

		rb1+= 4;
		rb2+= 4;
		rb3+= 4;
	}
}

void edge_enhance(void)
{
	/* use zbuffer to define edges, add it to the image */
	int val, y, x, col, *rz, *rz1, *rz2, *rz3;
	char *cp;
	
	/* shift values in zbuffer 3 to the right */
	rz= (int *)R.rectz;
	if(rz==0) return;
	
	for(y=0; y<R.recty; y++) {
		for(x=0; x<R.rectx; x++, rz++) {
			(*rz)>>= 3;
		}
	}

	rz1= (int *)R.rectz;
	rz2= rz1+R.rectx;
	rz3= rz2+R.rectx;
	rz= (int *)R.rectot+R.rectx;

	if(R.r.mode & R_OSA) {
		cp= (char *)(R.rectaccu+R.rectx);
	}
	else {
		cp= (char *)(R.rectot+R.rectx);
	}

	/* rz itself does not seem to be used. */
	
	for(y=0; y<R.recty-2; y++) {

		rz++;
		for(x=0; x<R.rectx-2; x++, rz++, rz1++, rz2++, rz3++, cp+=4) {

			col= abs(12*rz2[1]-rz1[0]-2*rz1[1]-rz1[2]-2*rz2[0]-2*rz2[2]-rz3[0]-2*rz3[1]-rz3[2])/3;
			/* removed the abs... now, only front/back? pixels are           */
			/* accentuated? No, the lines seem shifted strangely. the does   */
			/* not seem to be any overlap? strange...                        */
/*  			col= -( 12*rz2[1] */
/*  					-   rz1[0] - 2*rz1[1] -   rz1[2] */
/*  					- 2*rz2[0]            - 2*rz2[2] */
/*  					-   rz3[0] - 2*rz3[1] -   rz3[2]) /3; */
			
			col= (R.r.edgeint*col)>>14;
			if(col>255) col= 255;
			
			if(col>0) {
				if(R.r.mode & R_OSA) {
					col/= R.osa;
					
					val= cp[3]+col;
					if(val>255) cp[3]= 255; else cp[3]= val;
				}
				else {
					val= cp[0]- col;
					if(val<0) cp[0]= 0; else cp[0]= val;
					val= cp[1]- col;
					if(val<0) cp[1]= 0; else cp[1]= val;
					val= cp[2]- col;
					if(val<0) cp[2]= 0; else cp[2]= val;
				}
			}
		}
		rz++;
		rz1+= 2;
		rz2+= 2;
		rz3+= 2;
		cp+= 8;
	}

}

/* ********************* MAINLOOPS ******************** */

extern unsigned short *Acolrow;
/*  short zbuffermetdehand(); */
void zbufshadeDA(void)	/* Delta Accum Pixel Struct */
{
	PixStr *ps;
	float xd, yd, xs, ys;
	unsigned int *rz, *rp, *rt, mask, fullmask;
	unsigned int  *rowbuf1, *rowbuf2, *rowbuf3, *rb1, *rb2, *rb3;
	int a, b;
	long *rd;
	unsigned short *colrb, *acol;
	short v, x, y;
	char *colrt, tempcol[4];

	R.rectdaps= MEM_callocN(sizeof(long)*R.rectx*R.recty+4,"zbufDArectd");
	if(R.flag & R_ZTRA) bgnaccumbuf();

	psmteller= 0;

	if(R.r.mode & R_EDGE) {
		R.rectaccu= (unsigned int *)MEM_callocN(sizeof(int)*R.rectx*R.recty,"zbufshadeDA");
	}

	for(v=0; v<R.osa; v++) {

		xd= jit[v][0];
		yd= jit[v][1];
		Zjitx= -xd;
		Zjity= -yd;

		RE_local_printrenderinfo(0.0, v);

		/* RECTDELTA  */
		fillrect(R.rectot,R.rectx,R.recty,0);

		zbufferall();

		if(v==0) {
			a= R.rectx*R.recty;
			rt= R.rectot;
			rd= R.rectdaps;
			while(a--) {
				*rd= (long)*rt;
				rd++; rt++;
			}
		}
		else {
			rd= R.rectdaps;
			rp= R.rectot;
			rz= R.rectz;
			for(y=0; y<R.recty; y++) {
				for(x=0; x<R.rectx; x++, rp++, rd++) {
					if(*rd!= (long) *rp) {
						addps(rd, *rp, *(rz+x), v);
					}
				}
				rz+= R.rectx;
			}
		}
			/* 1 is for osa */
		if(R.r.mode & R_EDGE) edge_enhance();
		
		if( !(v % 256) && RE_local_test_break()) break; /*Hos,RPW, fixes slow render bug*/
	}
	if(R.flag & (R_ZTRA+R_HALO) ) {	 /* to get back correct values of zbuffer Z for transp and halos */
		xd= jit[0][0];
		yd= jit[0][1];
		Zjitx= -xd;
		Zjity= -yd;
		RE_setwindowclip(0, 0);
		RE_local_printrenderinfo(0.0, v);
		zbufferall();
	}

	R.vlaknr= -1;
	rd= R.rectdaps;
	rz= R.rectz;
	colrt= (char *)R.rectot;


	fullmask= (1<<R.osa)-1;
	/* the rowbuf is 4 pixels larger than an image! */
	rowbuf1= MEM_callocN(3*(R.rectx+4)*4*sizeof(float), "ZbufshadeDA3");
	rowbuf2= MEM_callocN(3*(R.rectx+4)*4*sizeof(float), "ZbufshadeDA3");
	rowbuf3= MEM_callocN(3*(R.rectx+4)*4*sizeof(float), "ZbufshadeDA3");

	for(y=0;y<=R.recty;y++) {

		rb1= rowbuf1;
		rb2= rowbuf2;
		rb3= rowbuf3;

		if(y<R.recty) {
			for(x=0; x<R.rectx; x++, rd++) {

				if( IS_A_POINTER_CODE(*rd)) {	
					ps= (PixStr *) POINTER_FROM_CODE(*rd);
					mask= 0;

					while(ps) {
						b= centmask[ps->mask];
						xs= (float)x+centLut[b & 15];
						ys= (float)y+centLut[b>>4];

						shadepixel(xs, ys, ps->vlak);

						if(shortcol[3]) {
							add_filt_mask(ps->mask, shortcol, rb1, rb2, rb3);
						}
						mask |= ps->mask;

						ps= ps->next;
					}
					ps= (PixStr *) POINTER_FROM_CODE(*rd);
					mask= (~mask) & fullmask;

					b= centmask[mask];
					xs= (float)x+centLut[b & 15];
					ys= (float)y+centLut[b>>4];

					shadepixel(xs, ys, ps->vlak0);

					if(shortcol[3]) {
						add_filt_mask(mask, shortcol, rb1, rb2, rb3);
					}
				}
				else {
					shadepixel((float)x, (float)y, (int)*rd);
					if(shortcol[3]) {
						add_filt_mask(fullmask, shortcol, rb1, rb2, rb3);
					}
				}

				rb1+=4; 
				rb2+=4; 
				rb3+=4;
			}
		}
		if(y>0) {
		
			colrb= (unsigned short *)(rowbuf3+4);
			
			/* WATCH IT: ENDIAN */
			
			for(x=0; x<R.rectx; x++,colrt+=4) {
				colrt[0]= ( (char *) (gamtab+colrb[0+MOST_SIG_BYTE]) )[MOST_SIG_BYTE];
				colrt[1]= ( (char *) (gamtab+colrb[2+MOST_SIG_BYTE]) )[MOST_SIG_BYTE];
				colrt[2]= ( (char *) (gamtab+colrb[4+MOST_SIG_BYTE]) )[MOST_SIG_BYTE];
				colrt[3]= ( (char *) (gamtab+colrb[6+MOST_SIG_BYTE]) )[MOST_SIG_BYTE];
				colrb+= 8;
			}
			if(R.flag & R_ZTRA) {
				abufsetrow(y-1);
				acol= Acolrow;
				colrt-= 4*R.rectx;
				
				for(x=0; x<R.rectx; x++, colrt+=4, acol+=4) {
					if(acol[3]) {
						tempcol[0]= (acol[0]>>8);
						tempcol[1]= (acol[1]>>8);
						tempcol[2]= (acol[2]>>8);
						tempcol[3]= (acol[3]>>8);
						addalphaOver(colrt, tempcol);
					}
				}
			}
			
			if(R.flag & R_HALO) {
				/* from these pixels the pixstr is 1 scanline old */
				scanlinehaloPS(rz-R.rectx, rd-2*R.rectx, ((unsigned int *)colrt)-R.rectx, y-1);
				
			}		
			scanlinesky(colrt-4*R.rectx, y-1);
			
			/* scanline starts nicely: halos use textures as well! */
			R.vlaknr= -1;
			
		}
		if(y<R.recty) {
			memset(rowbuf3, 0, (R.rectx+4)*4*4);
			rb3= rowbuf3;
			rowbuf3= rowbuf2;
			rowbuf2= rowbuf1;
			rowbuf1= rb3;

			if( y>0 && G.background==0) {
				if((y & 1)==0) {
					RE_local_render_display(y-2, y-1, 
											R.rectx, R.recty,
											R.rectot);
				}
			}
			rz+= R.rectx;
		}
		if(!(y % 256) && RE_local_test_break()) break; /* Hos,RPW, fixes slow render bug */
	}

	if( (R.r.mode & R_EDGE) && RE_local_test_break()==0) {
		rt= R.rectot;
		rp= R.rectaccu;
		for(a= R.rectx*R.recty; a>0; a--, rt++, rp++) {
			addalphaOver((char *)rt, (char *)rp);
		}
	}
	
	MEM_freeN(R.rectdaps); 
	freeps();
	MEM_freeN(rowbuf1); 
	MEM_freeN(rowbuf2); 
	MEM_freeN(rowbuf3);
	R.rectdaps= 0;

	if(R.r.mode & R_EDGE) if(R.rectaccu) MEM_freeN(R.rectaccu);
	R.rectaccu= 0;
	if(R.flag & R_ZTRA) endaccumbuf();
} /* end of void zbufshadeDA() */

/* ------------------------------------------------------------------------ */

void zbufshade(void)
{
	unsigned int *rz,*rp;
	float fy;
	int x,y;
	unsigned short *acol;
	char *charcol, *rt;

	Zjitx=Zjity= -.5;

	zbufferall();

	/* SHADE */
	rp= R.rectot;
	rz= R.rectz;
	charcol= (char *)shortcol;

	#ifdef BBIG_ENDIAN
	#else
	charcol++;		/* short is read different then */
	#endif

	if(R.flag & R_ZTRA) bgnaccumbuf();

	for(y=0; y<R.recty; y++) {
		R.vlaknr= -1;
		fy= y;
		
		if(R.flag & R_ZTRA) {		/* zbuf tra */
  			abufsetrow(y); 
			acol= Acolrow;
			
			for(x=0; x<R.rectx; x++, rp++, acol+= 4) {
				/* spothalo's added here... *rp is the target colour? */
  				shadepixel((float)x, fy, *rp);
				
				if(acol[3]) addAlphaOverShort(shortcol, acol);
				
				if(shortcol[3]) {
					rt= (char *)rp;
					rt[0]= charcol[0];
					rt[1]= charcol[2];
					rt[2]= charcol[4];
					rt[3]= charcol[6];
				}
				else *rp= 0;
			}
		}
		else {
			for(x=0; x<R.rectx; x++, rp++) {
				shadepixel((float)x, fy, *rp);
				if(shortcol[3]) {
					rt= (char *)rp;
					rt[0]= charcol[0];
					rt[1]= charcol[2];
					rt[2]= charcol[4];
					rt[3]= charcol[6];
				}
				else *rp= 0;
			}
		}
		
  		if(R.flag & R_HALO) {
  			scanlinehalo(rz, (rp-R.rectx), y);
  			rz+= R.rectx;
  		}
		scanlinesky( (char *)(rp-R.rectx), y);
		
		if((y & 1) && G.background!=1) {
			RE_local_render_display(y-1, y, 
									R.rectx, R.recty,
									R.rectot);
		}
		
		if(!(y % 256) && RE_local_test_break()) break; /*Hos,RPW, Fixes Slow render bug */
	}

	if(R.flag & R_ZTRA) endaccumbuf();
	
	if(R.r.mode & R_EDGE) edge_enhance();

	/* if((R.flag & R_HALO) && blender_test_break()==0) halovert(); */

} /* end of void zbufshade() */

/* ------------------------------------------------------------------------ */

void renderhalo(HaloRen *har)	/* postprocess version */
{
	
	float dist, xsq, ysq, xn, yn;
	unsigned int *rectt, *rt;
	int minx, maxx, miny, maxy, x, y;
	char col[4];


	har->miny= miny= har->ys - har->rad/R.ycor;
	har->maxy= maxy= har->ys + har->rad/R.ycor;

	if(maxy<0);
	else if(R.recty<miny);
	else {
		minx= floor(har->xs-har->rad);
		maxx= ceil(har->xs+har->rad);
			
		if(maxx<0);
		else if(R.rectx<minx);
		else {
		
			if(minx<0) minx= 0;
			if(maxx>=R.rectx) maxx= R.rectx-1;
			if(miny<0) miny= 0;
			if(maxy>R.recty) maxy= R.recty;
	
			rectt= R.rectot+ R.rectx*miny;
	
			for(y=miny;y<maxy;y++) {
	
				rt= (rectt+minx);
	
				yn= (y - har->ys)*R.ycor;
				ysq= yn*yn;
				
				for(x=minx; x<=maxx; x++) {
					xn= x - har->xs;
					xsq= xn*xn;
					dist= xsq+ysq;
					if(dist<har->radsq) {
						RE_shadehalo(har, col, 0, dist, xn, yn, har->flarec);
							
						RE_addalphaAddfac((char *)rt, col, har->add);
					}
					rt++;
				}
	
				rectt+= R.rectx;
				
				if( !(y % 256) && RE_local_test_break()) break; /* Hos,RPW, fixes slow render bug */
			}
	
		}
	}
} /* end of void renderhalo(HaloRen *har), postprocess version */

/* ------------------------------------------------------------------------ */

/*  extern float hashvectf[]; */
void RE_renderflare(HaloRen *har)
{
	HaloRen fla;
	Material *ma;
	float *rc, rad, alfa, visifac, vec[3];
	int b, type;
	
	fla= *har;
	fla.linec= fla.ringc= fla.flarec= 0;
	
	rad= har->rad;
	alfa= har->alfa;
	
	visifac= R.ycor*(har->pixels);
	/* all radials added / r^3  == 1.0! */
	visifac /= (har->rad*har->rad*har->rad);
	visifac*= visifac;

	ma= har->mat;
	
	/* first halo: just do */
	
	har->rad= rad*ma->flaresize*visifac;
	har->radsq= har->rad*har->rad;
	har->zs= 0.0;
	
	har->alfa= alfa*visifac;
	
	renderhalo(har);
	
	/* next halo's: the flares */
	rc= hashvectf + ma->seed2;
	
	for(b=1; b<har->flarec; b++) {
		
		fla.r= fabs(255.0*rc[0]);
		fla.g= fabs(255.0*rc[1]);
		fla.b= fabs(255.0*rc[2]);
		fla.alfa= ma->flareboost*fabs(alfa*visifac*rc[3]);
		fla.hard= 20.0 + fabs(70*rc[7]);
		fla.tex= 0;
		
		type= (int)(fabs(3.9*rc[6]));

		fla.rad= ma->subsize*sqrt(fabs(2.0*har->rad*rc[4]));
		
		if(type==3) {
			fla.rad*= 3.0;
			fla.rad+= R.rectx/10;
		}
		
		fla.radsq= fla.rad*fla.rad;
		
		vec[0]= 1.4*rc[5]*(har->xs-R.afmx);
		vec[1]= 1.4*rc[5]*(har->ys-R.afmy);
		vec[2]= 32.0*sqrt(vec[0]*vec[0] + vec[1]*vec[1] + 1.0);
		
		fla.xs= R.afmx + vec[0] + (1.2+rc[8])*R.rectx*vec[0]/vec[2];
		fla.ys= R.afmy + vec[1] + (1.2+rc[8])*R.rectx*vec[1]/vec[2];

		if(R.flag & R_SEC_FIELD) {
			if(R.r.mode & R_ODDFIELD) fla.ys += 0.5;
			else fla.ys -= 0.5;
		}
		if(type & 1) fla.type= HA_FLARECIRC;
		else fla.type= 0;
		renderhalo(&fla);

		fla.alfa*= 0.5;
		if(type & 2) fla.type= HA_FLARECIRC;
		else fla.type= 0;
		renderhalo(&fla);
		
		rc+= 7;
	}
} /* end of void renderflare(HaloRen *har) */

void add_halo_flare(void)
{
/*  	extern void RE_projectverto(); */ /*  zbuf.c */
	HaloRen *har = NULL;
	int a, mode;
	
	mode= R.r.mode;
	R.r.mode &= ~R_PANORAMA;
	R.xstart= -R.afmx; 
	R.ystart= -R.afmy;
	R.xend= R.xstart+R.rectx-1;
	R.yend= R.ystart+R.recty-1;

	RE_setwindowclip(1,-1); /*  no jit:(-1) */
	setzbufvlaggen(RE_projectverto);
	
	for(a=0; a<R.tothalo; a++) {
		if((a & 255)==0) har= R.bloha[a>>8];
		else har++;
		
		if(har->flarec) {
			RE_renderflare(har);
		}
	}

	R.r.mode= mode;	
} /* end of void add_halo_flare() */


/* end of render.c */


