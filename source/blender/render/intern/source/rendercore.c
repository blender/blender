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
#include "texture.h"

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

PixStrMain psmfirst;
int psmteller;

float fmask[256], centLut[16];
unsigned short usegamtab=0, *mask1[9], *mask2[9], *igamtab1, *igamtab2, *gamtab;
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

void RE_sky(float *view, float *col)
{
	float lo[3];

	R.wrld.skytype |= WO_ZENUP;
	
	if(R.wrld.skytype & WO_SKYREAL) {
	
		R.inprz= view[0]*R.grvec[0]+ view[1]*R.grvec[1]+ view[2]*R.grvec[2];

		if(R.inprz<0.0) R.wrld.skytype-= WO_ZENUP;
		R.inprz= fabs(R.inprz);
	}
	else if(R.wrld.skytype & WO_SKYPAPER) {
		R.inprz= 0.5+ 0.5*view[1];
	}
	else {
		R.inprz= fabs(0.5+ view[1]);
	}

	if(R.wrld.skytype & WO_SKYTEX) {
		VECCOPY(lo, view);
		if(R.wrld.skytype & WO_SKYREAL) {
			
			MTC_Mat3MulVecfl(R.imat, lo);
	
			SWAP(float, lo[1],  lo[2]);
			
		}

		do_sky_tex(lo);
		
	}

	if(R.inprz>1.0) R.inprz= 1.0;
	R.inprh= 1.0-R.inprz;

	if(R.wrld.skytype & WO_SKYBLEND) {
		col[0]= (R.inprh*R.wrld.horr + R.inprz*R.wrld.zenr);
		col[1]= (R.inprh*R.wrld.horg + R.inprz*R.wrld.zeng);
		col[2]= (R.inprh*R.wrld.horb + R.inprz*R.wrld.zenb);
	}
	else {
		col[0]= R.wrld.horr;
		col[1]= R.wrld.horg;
		col[2]= R.wrld.horb;
	}
}

void RE_sky_char(float *view, char *col)
{
	float f, colf[3];
	
	RE_sky(view, colf);
	f= 255.0*colf[0];
	if(f<=0.0) col[0]= 0; else if(f>255.0) col[0]= 255;
	else col[0]= (char)f;
	f= 255.0*colf[1];
	if(f<=0.0) col[1]= 0; else if(f>255.0) col[1]= 255;
	else col[1]= (char)f;
	f= 255.0*colf[2];
	if(f<=0.0) col[2]= 0; else if(f>255.0) col[2]= 255;
	else col[2]= (char)f;
	col[3]= 1;	/* to prevent wrong optimalisation alphaover of flares */
}

/* ------------------------------------------------------------------------- */

void scanlinesky(char *rect, int y)
{
	/* have to type this! set to :  addalphaUnder: char*, char*
	 * addalphaUnderGamma: ditto called with char *, uint* !!!
	 * unmangle this shit... */
	void (*alphafunc)();
	float fac, u, v, view[3];
	int dx, x, ofs;
	unsigned int col=0, *rt;
	char *cp, *cp1;
	
	if(R.r.alphamode & R_ALPHAPREMUL) return;
	
	if(R.r.alphamode & R_ALPHAKEY) {
		
		cp= (char *)&col;
		cp[3]= 0;
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
			y= ((y+R.afmy+R.ystart)*R.backbuf->ibuf->y)/(R.recty);
			
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
				while( ofs > 65535 ) {
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
					view[0]= (x+(R.xstart))/(float)R.afmx;
					view[1]= (y+(R.ystart))/(float)R.afmy;
					view[2]= 0.0;
				}
				else {
					view[0]= (x+(R.xstart)+0.5);
		
					if(R.flag & R_SEC_FIELD) {
						if(R.r.mode & R_ODDFIELD) view[1]= (y+R.ystart)*R.ycor;
						else view[1]= (y+R.ystart+1.0)*R.ycor;
					}
					else view[1]= (y+R.ystart+0.5)*R.ycor;
					
					view[2]= -R.viewfac;
	
					fac= Normalise(view);
					if(R.wrld.skytype & WO_SKYTEX) {
						O.dxview= 1.0/fac;
						O.dyview= R.ycor/fac;
					}
				}
				
				if(R.r.mode & R_PANORAMA) {
					float panoco, panosi;
					panoco = getPanovCo();
					panosi = getPanovSi();
					u= view[0]; v= view[2];
					
					view[0]= panoco*u + panosi*v;
					view[2]= -panosi*u + panoco*v;
				}

				RE_sky_char(view, (char *)&col);
	
				if(rect[3]==0) *((unsigned int *)rect)= col;
				else alphafunc(rect, &col);
			}
		}
	}	
}

/* ************************************** */


void spothalo(struct LampRen *lar, ShadeInput *shi, float *intens)
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
	VECCOPY(nray, shi->view);
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
	if(shi->co[2]==0.0) doclip= 0;	/* for when halo at sky */
	else {
		p1[0]= shi->co[0]-lar->co[0];
		p1[1]= shi->co[1]-lar->co[1];
		p1[2]= shi->co[2]-lar->co[2];
	
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
		
	}
}

static void renderspothalo(ShadeInput *shi, float *col)
{
	LampRen *lar;
	float i;
	int a;
	
	for(a=0; a<R.totlamp; a++) {
		lar= R.la[a];
		if(lar->type==LA_SPOT && (lar->mode & LA_HALO) && lar->haint>0) {
	
			if(lar->org) {
				lar->r= lar->org->r;
				lar->g= lar->org->g;
				lar->b= lar->org->b;
			}

			spothalo(lar, shi, &i);
			if(i>0.0) {
				col[3]+= i;
				col[0]+= i*lar->r;
				col[1]+= i*lar->g;
				col[2]+= i*lar->b;			
			}
		}
	}
	/* clip alpha, is needed for unified 'alpha threshold' (vanillaRenderPipe.c) */
	if(col[3]>1.0) col[3]= 1.0;
}

void render_lighting_halo(HaloRen *har, float *colf)
{
	LampRen *lar;
	float i, inp, inpr, rco[3], lv[3], lampdist, ld, t, *vn;
	float ir, ig, ib, shadfac, soft;
	int a;
	
	ir= ig= ib= 0.0;
	VECCOPY(rco, har->co);
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
			lv[0]= rco[0]-lar->co[0];
			lv[1]= rco[1]-lar->co[1];
			lv[2]= rco[2]-lar->co[2];
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
		
		if(lar->mode & LA_TEXTURE) {
			ShadeInput shi;
			VECCOPY(shi.co, rco);
			shi.osatex= 0;
			do_lamp_tex(lar, lv, &shi);
		}
		
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
							shadfac = testshadowbuf(lar->shb, rco, inp);
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
				shadfac = testshadowbuf(lar->shb, rco, inp);
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


void RE_shadehalo(HaloRen *har, char *col, unsigned int zz, float dist, float xn, float yn, short flarec)
{
	/* fill in in col */
	extern float hashvectf[];
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
	short minx,maxx,x,aantal, aantalm, flarec;
	char col[4];

	for(a=0;a<R.tothalo;a++) {
		if((a & 255)==0) har= R.bloha[a>>8];
		else har++;

		if( RE_local_test_break() ) break;  

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
							aantal= 0;
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

		if(RE_local_test_break() ) break; 

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

		if(RE_local_test_break() ) break; 

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
					
					if(RE_local_test_break() ) break; 
				}

			}
		}
	}
}

/* ---------------- shaders ----------------------- */

static double Normalise_d(double *n)
{
	double d;
	
	d= n[0]*n[0]+n[1]*n[1]+n[2]*n[2];

	if(d>0.00000000000000001) {
		d= sqrt(d);

		n[0]/=d; 
		n[1]/=d; 
		n[2]/=d;
	} else {
		n[0]=n[1]=n[2]= 0.0;
		d= 0.0;
	}
	return d;
}


/* Stoke's form factor. Need doubles here for extreme small area sizes */
static float area_lamp_energy(float *co, float *vn, LampRen *lar)
{
	double fac;
	double vec[4][3];	/* vectors of rendered co to vertices lamp */
	double cross[4][3];	/* cross products of this */
	double rad[4];		/* angles between vecs */

	VECSUB(vec[0], co, lar->area[0]);
	VECSUB(vec[1], co, lar->area[1]);
	VECSUB(vec[2], co, lar->area[2]);
	VECSUB(vec[3], co, lar->area[3]);
	
	Normalise_d(vec[0]);
	Normalise_d(vec[1]);
	Normalise_d(vec[2]);
	Normalise_d(vec[3]);

	/* cross product */
	CROSS(cross[0], vec[0], vec[1]);
	CROSS(cross[1], vec[1], vec[2]);
	CROSS(cross[2], vec[2], vec[3]);
	CROSS(cross[3], vec[3], vec[0]);

	Normalise_d(cross[0]);
	Normalise_d(cross[1]);
	Normalise_d(cross[2]);
	Normalise_d(cross[3]);

	/* angles */
	rad[0]= vec[0][0]*vec[1][0]+ vec[0][1]*vec[1][1]+ vec[0][2]*vec[1][2];
	rad[1]= vec[1][0]*vec[2][0]+ vec[1][1]*vec[2][1]+ vec[1][2]*vec[2][2];
	rad[2]= vec[2][0]*vec[3][0]+ vec[2][1]*vec[3][1]+ vec[2][2]*vec[3][2];
	rad[3]= vec[3][0]*vec[0][0]+ vec[3][1]*vec[0][1]+ vec[3][2]*vec[0][2];

	rad[0]= acos(rad[0]);
	rad[1]= acos(rad[1]);
	rad[2]= acos(rad[2]);
	rad[3]= acos(rad[3]);

	/* Stoke formula */
	VECMUL(cross[0], rad[0]);
	VECMUL(cross[1], rad[1]);
	VECMUL(cross[2], rad[2]);
	VECMUL(cross[3], rad[3]);

	fac=  vn[0]*cross[0][0]+ vn[1]*cross[0][1]+ vn[2]*cross[0][2];
	fac+= vn[0]*cross[1][0]+ vn[1]*cross[1][1]+ vn[2]*cross[1][2];
	fac+= vn[0]*cross[2][0]+ vn[1]*cross[2][1]+ vn[2]*cross[2][2];
	fac+= vn[0]*cross[3][0]+ vn[1]*cross[3][1]+ vn[2]*cross[3][2];

	if(fac<=0.0) return 0.0;
	return pow(fac*lar->areasize, lar->k);	// corrected for buttons size and lar->dist^2
}


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
	b1*= b1;
	if(hard & 128) inp*=b1;

	if(b1<0.001) b1= 0.0;	

	if(hard & 256) {
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
	float a, b, c, g=0.0, p, f, ang;

	if(refrac < 1.0) return 0.0;
	if(spec_power == 0.0) return 0.0;
	
	/* conversion from 'hardness' (1-255) to 'spec_power' (50 maps at 0.1) */
	if(spec_power<100.0)
		spec_power= sqrt(1.0/spec_power);
	else spec_power= 10.0/spec_power;
	
	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	Normalise(h);

	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */

	if(nh<0.0) return 0.0;

	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */

	if(nv<=0.0) nv= 0.01;

	nl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */

	if(nl<=0.0) {
		nl= 0.0;
		return 0.0;
	}

	vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; /* Dot product between view vector and half-way vector */
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

/* 'nl' is either dot product, or return value of area light */
/* in latter case, only last multiplication uses 'nl' */
float OrenNayar_Diff_i(float nl, float *n, float *l, float *v, float rough )
{
	float i, nh, nv, vh, realnl, h[3];
	float a, b, t, A, B;
	float Lit_A, View_A, Lit_B[3], View_B[3];
	
	h[0]= v[0]+l[0];
	h[1]= v[1]+l[1];
	h[2]= v[2]+l[2];
	Normalise(h);
	
	nh= n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if(nh<0.0) nh = 0.0;
	
	nv= n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if(nv<=0.0) nv= 0.0;
	
	realnl= n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if(realnl<=0.0) return 0.0;
	if(nl<0.0) return 0.0;		/* value from area light */
	
	vh= v[0]*h[0]+v[1]*h[1]+v[2]*h[2]; /* Dot product between view vector and halfway vector */
	if(vh<=0.0) vh= 0.0;
	
	Lit_A = acos(realnl);
	View_A = acos( nv );
	
	Lit_B[0] = l[0] - (realnl * n[0]);
	Lit_B[1] = l[1] - (realnl * n[1]);
	Lit_B[2] = l[2] - (realnl * n[2]);
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
	
	b*= 0.95;	/* prevent tangens from shooting to inf, 'nl' can be not a dot product here. */
				/* overflow only happens with extreme size area light, and higher roughness */
	i = nl * ( A + ( B * t * sin(a) * tan(b) ) );
	
	return i;
}

/* Oren Nayar diffuse */
float OrenNayar_Diff(float *n, float *l, float *v, float rough )
{
	float nl= n[0]*l[0] + n[1]*l[1] + n[2]*l[2];
	return OrenNayar_Diff_i(nl, n, l, v, rough);
}


/* --------------------------------------------- */

void calc_R_ref(ShadeInput *shi)
{
	float i;

	/* shi->vn dot shi->view */
	i= -2*(shi->vn[0]*shi->view[0]+shi->vn[1]*shi->view[1]+shi->vn[2]*shi->view[2]);

	shi->ref[0]= (shi->view[0]+i*shi->vn[0]);
	shi->ref[1]= (shi->view[1]+i*shi->vn[1]);
	shi->ref[2]= (shi->view[2]+i*shi->vn[2]);
	if(shi->osatex) {
		if(shi->vlr->flag & R_SMOOTH) {
			i= -2*( (shi->vn[0]+O.dxno[0])*(shi->view[0]+O.dxview) +
				(shi->vn[1]+O.dxno[1])*shi->view[1]+ (shi->vn[2]+O.dxno[2])*shi->view[2] );

			O.dxref[0]= shi->ref[0]- ( shi->view[0]+O.dxview+i*(shi->vn[0]+O.dxno[0]));
			O.dxref[1]= shi->ref[1]- (shi->view[1]+ i*(shi->vn[1]+O.dxno[1]));
			O.dxref[2]= shi->ref[2]- (shi->view[2]+ i*(shi->vn[2]+O.dxno[2]));

			i= -2*( (shi->vn[0]+O.dyno[0])*shi->view[0]+
				(shi->vn[1]+O.dyno[1])*(shi->view[1]+O.dyview)+ (shi->vn[2]+O.dyno[2])*shi->view[2] );

			O.dyref[0]= shi->ref[0]- (shi->view[0]+ i*(shi->vn[0]+O.dyno[0]));
			O.dyref[1]= shi->ref[1]- (shi->view[1]+O.dyview+i*(shi->vn[1]+O.dyno[1]));
			O.dyref[2]= shi->ref[2]- (shi->view[2]+ i*(shi->vn[2]+O.dyno[2]));

		}
		else {

			i= -2*( shi->vn[0]*(shi->view[0]+O.dxview) +
				shi->vn[1]*shi->view[1]+ shi->vn[2]*shi->view[2] );

			O.dxref[0]= shi->ref[0]- (shi->view[0]+O.dxview+i*shi->vn[0]);
			O.dxref[1]= shi->ref[1]- (shi->view[1]+ i*shi->vn[1]);
			O.dxref[2]= shi->ref[2]- (shi->view[2]+ i*shi->vn[2]);

			i= -2*( shi->vn[0]*shi->view[0]+
				shi->vn[1]*(shi->view[1]+O.dyview)+ shi->vn[2]*shi->view[2] );

			O.dyref[0]= shi->ref[0]- (shi->view[0]+ i*shi->vn[0]);
			O.dyref[1]= shi->ref[1]- (shi->view[1]+O.dyview+i*shi->vn[1]);
			O.dyref[2]= shi->ref[2]- (shi->view[2]+ i*shi->vn[2]);
		}
	}

}

/* mix of 'real' fresnel and allowing control. grad defines blending gradient */
float fresnel_fac(float *view, float *vn, float grad, float fac)
{
	float t1, t2;
	
	if(fac==0.0) return 1.0;
	
	t1= (view[0]*vn[0] + view[1]*vn[1] + view[2]*vn[2]);
	if(t1>0.0)  t2= 1.0+t1;
	else t2= 1.0-t1;
	
	t2= grad + (1.0-grad)*pow(t2, fac);

	if(t2<0.0) return 0.0;
	else if(t2>1.0) return 1.0;
	return t2;
}

void shade_color(ShadeInput *shi, ShadeResult *shr)
{
	Material *ma= shi->matren;

	if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
		ma->r= shi->vcol[0];
		ma->g= shi->vcol[1];
		ma->b= shi->vcol[2];
	}
	
	ma->alpha= shi->mat->alpha;	// copy to render material, for fresnel and spectra

	if(ma->texco) {
		if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
			shi->mat->r= shi->vcol[0];
			shi->mat->g= shi->vcol[1];
			shi->mat->b= shi->vcol[2];
		}
		do_material_tex(shi);
	}

	if(ma->mode & (MA_ZTRA|MA_RAYTRANSP)) {
		if(ma->fresnel_tra!=1.0) 
			ma->alpha*= fresnel_fac(shi->view, shi->vn, ma->fresnel_tra_i, ma->fresnel_tra);
	}

	shr->diff[0]= ma->r;
	shr->diff[1]= ma->g;
	shr->diff[2]= ma->b;
	shr->alpha= ma->alpha;
}

static void ambient_occlusion(World *wrld, ShadeInput *shi, ShadeResult *shr)
{
	float f, shadfac[4];
	
	if((wrld->mode & WO_AMB_OCC) && (R.r.mode & R_RAYTRACE) && shi->matren->amb!=0.0) {
		ray_ao(shi, wrld, shadfac);

		if(wrld->aocolor==WO_AOPLAIN) {
			if (wrld->aomix==WO_AOADDSUB) shadfac[3] = 2.0*shadfac[3]-1.0;
			else if (wrld->aomix==WO_AOSUB) shadfac[3] = shadfac[3]-1.0;

			f= wrld->aoenergy*shadfac[3]*shi->matren->amb;
			shr->diff[0] += f;
			shr->diff[1] += f;
			shr->diff[2] += f;
		}
		else {
			if (wrld->aomix==WO_AOADDSUB) {
				shadfac[0] = 2.0*shadfac[0]-1.0;
				shadfac[1] = 2.0*shadfac[1]-1.0;
				shadfac[2] = 2.0*shadfac[2]-1.0;
			}
			else if (wrld->aomix==WO_AOSUB) {
				shadfac[0] = shadfac[0]-1.0;
				shadfac[1] = shadfac[1]-1.0;
				shadfac[2] = shadfac[2]-1.0;
			}
			f= wrld->aoenergy*shi->matren->amb;
			shr->diff[0] += f*shadfac[0];
			shr->diff[1] += f*shadfac[1];
			shr->diff[2] += f*shadfac[2];
		}
	}
}


void shade_lamp_loop(ShadeInput *shi, ShadeResult *shr)
{
	LampRen *lar;
	Material *ma;
	float i, inp, inpr, t, lv[3], lampdist, ld = 0;
	float lvrot[3], *vn, *view, shadfac[4], soft;	// shadfac = rgba
	int a;

	vn= shi->vn;
	view= shi->view;
	ma= shi->matren;
	
	memset(shr, 0, sizeof(ShadeResult));
	
	/* separate loop */
	if(ma->mode & MA_ONLYSHADOW) {
		float ir;
		
		shadfac[3]= ir= 0.0;
		for(a=0; a<R.totlamp; a++) {
			lar= R.la[a];
			
			if(lar->mode & LA_LAYER) if((lar->lay & shi->vlr->lay)==0) continue;
			
			lv[0]= shi->co[0]-lar->co[0];
			lv[1]= shi->co[1]-lar->co[1];
			lv[2]= shi->co[2]-lar->co[2];

			if(lar->shb) {
				/* only test within spotbundel */
				Normalise(lv);
				inpr= lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2];
				if(inpr>lar->spotsi) {
					
					inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
					
					i = testshadowbuf(lar->shb, shi->co, inp);
					
					t= inpr - lar->spotsi;
					if(t<lar->spotbl && lar->spotbl!=0.0) {
						t/= lar->spotbl;
						t*= t;
						i= t*i+(1.0-t);
					}
					
					shadfac[3]+= i;
					ir+= 1.0;
				}
				else {
					shadfac[3]+= 1.0;
					ir+= 1.0;
				}
			}
			else if(lar->mode & LA_SHAD_RAY) {
				float shad[4];
				
				/* single sided? */
				if( shi->vlr->n[0]*lv[0] + shi->vlr->n[1]*lv[1] + shi->vlr->n[2]*lv[2] > -0.01) {
					ray_shadow(shi, lar, shad);
					shadfac[3]+= shad[3];
					ir+= 1.0;
				}
			}

		}
		if(ir>0.0) shadfac[3]/= ir;
		shr->alpha= (shi->mat->alpha)*(1.0-shadfac[3]);
		
		return;
	}
		
	if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
		ma->r= shi->vcol[0];
		ma->g= shi->vcol[1];
		ma->b= shi->vcol[2];
	}
	
	ma->alpha= shi->mat->alpha;	// copy to render material, for fresnel and spectra
	
	/* envmap hack, always reset */
	shi->refcol[0]= shi->refcol[1]= shi->refcol[2]= shi->refcol[3]= 0.0;

	if(ma->texco) {
		if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
			shi->mat->r= shi->vcol[0];
			shi->mat->g= shi->vcol[1];
			shi->mat->b= shi->vcol[2];
		}
		do_material_tex(shi);
	}
	
	if(ma->mode & MA_SHLESS) {
		shr->diff[0]= ma->r;
		shr->diff[1]= ma->g;
		shr->diff[2]= ma->b;
		shr->alpha= ma->alpha;
		return;
	}

	if( (ma->mode & (MA_VERTEXCOL+MA_VERTEXCOLP))== MA_VERTEXCOL ) {
		shr->diff[0]= ma->emit+shi->vcol[0];
		shr->diff[1]= ma->emit+shi->vcol[1];
		shr->diff[2]= ma->emit+shi->vcol[2];
	}
	else shr->diff[0]= shr->diff[1]= shr->diff[2]= ma->emit;

	ambient_occlusion(&R.wrld, shi, shr);

	for(a=0; a<R.totlamp; a++) {
		lar= R.la[a];

		/* test for lamp layer */
		if(lar->mode & LA_LAYER) if((lar->lay & shi->vlr->lay)==0) continue;
		
		/* lampdist calculation */
		if(lar->type==LA_SUN || lar->type==LA_HEMI) {
			VECCOPY(lv, lar->vec);
			lampdist= 1.0;
		}
		else {
			lv[0]= shi->co[0]-lar->co[0];
			lv[1]= shi->co[1]-lar->co[1];
			lv[2]= shi->co[2]-lar->co[2];
			ld= sqrt(lv[0]*lv[0]+lv[1]*lv[1]+lv[2]*lv[2]);
			lv[0]/= ld;
			lv[1]/= ld;
			lv[2]/= ld;
			
			/* ld is re-used further on (texco's) */
			if(lar->type==LA_AREA) {
				lampdist= 1.0;
			}
			else {
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
		}
		
		if(lar->mode & LA_TEXTURE)  do_lamp_tex(lar, lv, shi);

		/* init transp shadow */
		shadfac[3]= 1.0;
		if(ma->mode & MA_SHADOW_TRA) shadfac[0]= shadfac[1]= shadfac[2]= 1.0;

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
				lampdist*=inpr;
			}

			if(lar->mode & LA_OSATEX) {
				shi->osatex= 1;	/* signal for multitex() */
				
				O.dxlv[0]= lv[0] - (shi->co[0]-lar->co[0]+O.dxco[0])/ld;
				O.dxlv[1]= lv[1] - (shi->co[1]-lar->co[1]+O.dxco[1])/ld;
				O.dxlv[2]= lv[2] - (shi->co[2]-lar->co[2]+O.dxco[2])/ld;

				O.dylv[0]= lv[0] - (shi->co[0]-lar->co[0]+O.dyco[0])/ld;
				O.dylv[1]= lv[1] - (shi->co[1]-lar->co[1]+O.dyco[1])/ld;
				O.dylv[2]= lv[2] - (shi->co[2]-lar->co[2]+O.dyco[2])/ld;
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
		
			if(lar->type==LA_AREA) {
				/* single sided */
				if(lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2]>0.0)
					inp= area_lamp_energy(shi->co, shi->vn, lar);
				else inp= 0.0;
			}
			
			/* diffuse shaders (oren nayer gets inp from area light) */
			if(ma->diff_shader==MA_DIFF_ORENNAYAR) i= OrenNayar_Diff_i(inp, vn, lv, view, ma->roughness);
			else if(ma->diff_shader==MA_DIFF_TOON) i= Toon_Diff(vn, lv, view, ma->param[0], ma->param[1]);
			else i= inp;	// Lambert
		}
		
		if(i>0.0) {
			i*= lampdist*ma->ref;
		}

		/* shadow and spec, (lampdist==0 outside spot) */
		if(lampdist> 0.0) {
			
			if(i>0.0 && (R.r.mode & R_SHADOW)) {
				if(ma->mode & MA_SHADOW) {
					if(lar->type==LA_HEMI);	// no shadow
					else {
						if(lar->shb) {
							shadfac[3] = testshadowbuf(lar->shb, shi->co, inp);
						}
						else if(lar->mode & LA_SHAD_RAY) {
							// this extra 0.001 prevents boundary cases (shadow on smooth sphere)
							if((shi->vlr->n[0]*lv[0] + shi->vlr->n[1]*lv[1] + shi->vlr->n[2]*lv[2]) > -0.001) 
								ray_shadow(shi, lar, shadfac);
							else shadfac[3]= 0.0;
						}
	
						/* warning, here it skips the loop */
						if(lar->mode & LA_ONLYSHADOW) {
							
							shadfac[3]= i*lar->energy*(1.0-shadfac[3]);
							shr->diff[0] -= shadfac[3];
							shr->diff[1] -= shadfac[3];
							shr->diff[2] -= shadfac[3];
							
							continue;
						}
						
						if(shadfac[3]==0.0) continue;
	
						i*= shadfac[3];
					}
				}
			}
		
			/* specularity */
			if(shadfac[3]>0.0 && ma->spec!=0.0 && !(lar->mode & LA_NO_SPEC)) {
				
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
					
					t= shadfac[3]*ma->spec*spec(t, ma->har);
					shr->spec[0]+= t*(lar->r * ma->specr);
					shr->spec[1]+= t*(lar->g * ma->specg);
					shr->spec[2]+= t*(lar->b * ma->specb);
				}
				else {
					/* specular shaders */
					float specfac;

					if(ma->spec_shader==MA_SPEC_PHONG) 
						specfac= Phong_Spec(vn, lv, view, ma->har);
					else if(ma->spec_shader==MA_SPEC_COOKTORR) 
						specfac= CookTorr_Spec(vn, lv, view, ma->har);
					else if(ma->spec_shader==MA_SPEC_BLINN) 
						specfac= Blinn_Spec(vn, lv, view, ma->refrac, (float)ma->har);
					else 
						specfac= Toon_Spec(vn, lv, view, ma->param[2], ma->param[3]);
				
					/* area lamp correction */
					if(lar->type==LA_AREA) specfac*= inp;
					
					t= shadfac[3]*ma->spec*lampdist*specfac;
					
					shr->spec[0]+= t*(lar->r * ma->specr);
					shr->spec[1]+= t*(lar->g * ma->specg);
					shr->spec[2]+= t*(lar->b * ma->specb);
				}
			}
		}
		
		/* in case 'no diffuse' we still do most calculus, spec can be in shadow */
		if(i>0.0 && !(lar->mode & LA_NO_DIFF)) {
			if(ma->mode & MA_SHADOW_TRA) {
				shr->diff[0]+= i*shadfac[0]*lar->r;
				shr->diff[1]+= i*shadfac[1]*lar->g;
				shr->diff[2]+= i*shadfac[2]*lar->b;
			}
			else {
				shr->diff[0]+= i*lar->r;
				shr->diff[1]+= i*lar->g;
				shr->diff[2]+= i*lar->b;
			}
		}
	}

	if(ma->mode & (MA_ZTRA|MA_RAYTRANSP)) {
		if(ma->fresnel_tra!=1.0) 
			ma->alpha*= fresnel_fac(shi->view, shi->vn, ma->fresnel_tra_i, ma->fresnel_tra);

		if(ma->spectra!=0.0) {

			t = MAX3(shr->spec[0], shr->spec[1], shr->spec[2]);
			t *= ma->spectra;
			if(t>1.0) t= 1.0;
			ma->alpha= (1.0-t)*ma->alpha+t;
		}
	}

	shr->alpha= ma->alpha;

	if(shr->spec[0]<0.0) shr->spec[0]= 0.0;
	if(shr->spec[1]<0.0) shr->spec[1]= 0.0;
	if(shr->spec[2]<0.0) shr->spec[2]= 0.0;

	
	shr->diff[0]+= ma->amb*shi->rad[0];
	shr->diff[0]*= ma->r;
	shr->diff[0]+= ma->ambr;
	if(shr->diff[0]<0.0) shr->diff[0]= 0.0;
	
	shr->diff[1]+= ma->amb*shi->rad[1];
	shr->diff[1]*= ma->g;
	shr->diff[1]+= ma->ambg;
	if(shr->diff[1]<0.0) shr->diff[1]= 0.0;
	
	shr->diff[2]+= ma->amb*shi->rad[2];
	shr->diff[2]*= ma->b;
	shr->diff[2]+= ma->ambb;
	if(shr->diff[2]<0.0) shr->diff[2]= 0.0;
	
	/* refcol is for envmap only */
	if(shi->refcol[0]!=0.0) {
		shr->diff[0]= ma->mirr*shi->refcol[1] + (1.0 - ma->mirr*shi->refcol[0])*shr->diff[0];
		shr->diff[1]= ma->mirg*shi->refcol[2] + (1.0 - ma->mirg*shi->refcol[0])*shr->diff[1];
		shr->diff[2]= ma->mirb*shi->refcol[3] + (1.0 - ma->mirb*shi->refcol[0])*shr->diff[2];
	}

}

void shade_input_set_coords(ShadeInput *shi, float u, float v, int i1, int i2, int i3)
{
	VertRen *v1, *v2, *v3;
	VlakRen *vlr= shi->vlr;
	float l, dl;
	short texco= shi->matren->texco;
	int mode= shi->matren->mode;
	char p1, p2, p3;
	
	/* for rendering of quads, the following values are used to denote vertices:
	   0 1 2	scanline tria & first half quad, and ray tria
	   0 2 3    scanline 2nd half quad
	   0 1 3    raytracer first half quad
	   2 1 3    raytracer 2nd half quad
	*/

	if(i1==0) {
		v1= vlr->v1;
		p1= ME_FLIPV1;
	} else {
		v1= vlr->v3;
		p1= ME_FLIPV3;
	}

	if(i2==1) {
		v2= vlr->v2;
		p2= ME_FLIPV2;
	} else {
		v2= vlr->v3;
		p2= ME_FLIPV3;
	}
	
	if(i3==2) {
		v3= vlr->v3;
		p3= ME_FLIPV3;
	} else {
		v3= vlr->v4;
		p3= ME_FLIPV4;
	}

	/* calculate U and V, for scanline (normal u and v are -1 to 0) */
	if(u==1.0) {
		if( (vlr->flag & R_SMOOTH) || (texco & NEED_UV)) {
			float detsh, t00, t10, t01, t11;
			
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
		
			if(vlr->snproj==0) {
				u= (shi->co[0]-v3->co[0])*t11-(shi->co[1]-v3->co[1])*t10;
				v= (shi->co[1]-v3->co[1])*t00-(shi->co[0]-v3->co[0])*t01;
				if(shi->osatex) {
					O.dxuv[0]=  O.dxco[0]*t11- O.dxco[1]*t10;
					O.dxuv[1]=  O.dxco[1]*t00- O.dxco[0]*t01;
					O.dyuv[0]=  O.dyco[0]*t11- O.dyco[1]*t10;
					O.dyuv[1]=  O.dyco[1]*t00- O.dyco[0]*t01;
				}
			}
			else if(vlr->snproj==1) {
				u= (shi->co[0]-v3->co[0])*t11-(shi->co[2]-v3->co[2])*t10;
				v= (shi->co[2]-v3->co[2])*t00-(shi->co[0]-v3->co[0])*t01;
				if(shi->osatex) {
					O.dxuv[0]=  O.dxco[0]*t11- O.dxco[2]*t10;
					O.dxuv[1]=  O.dxco[2]*t00- O.dxco[0]*t01;
					O.dyuv[0]=  O.dyco[0]*t11- O.dyco[2]*t10;
					O.dyuv[1]=  O.dyco[2]*t00- O.dyco[0]*t01;
				}
			}
			else {
				u= (shi->co[1]-v3->co[1])*t11-(shi->co[2]-v3->co[2])*t10;
				v= (shi->co[2]-v3->co[2])*t00-(shi->co[1]-v3->co[1])*t01;
				if(shi->osatex) {
					O.dxuv[0]=  O.dxco[1]*t11- O.dxco[2]*t10;
					O.dxuv[1]=  O.dxco[2]*t00- O.dxco[1]*t01;
					O.dyuv[0]=  O.dyco[1]*t11- O.dyco[2]*t10;
					O.dyuv[1]=  O.dyco[2]*t00- O.dyco[1]*t01;
				}
			}
		}
	
	}
	l= 1.0+u+v;
	
	/* calculate punos (vertexnormals) */
	if(vlr->flag & R_SMOOTH) { 
		float n1[3], n2[3], n3[3];
		
		if(vlr->puno & p1) {
			n1[0]= -v1->n[0]; n1[1]= -v1->n[1]; n1[2]= -v1->n[2];
		} else {
			n1[0]= v1->n[0]; n1[1]= v1->n[1]; n1[2]= v1->n[2];
		}
		if(vlr->puno & p2) {
			n2[0]= -v2->n[0]; n2[1]= -v2->n[1]; n2[2]= -v2->n[2];
		} else {
			n2[0]= v2->n[0]; n2[1]= v2->n[1]; n2[2]= v2->n[2];
		}
		
		if(vlr->puno & p3) {
			n3[0]= -v3->n[0]; n3[1]= -v3->n[1]; n3[2]= -v3->n[2];
		} else {
			n3[0]= v3->n[0]; n3[1]= v3->n[1]; n3[2]= v3->n[2];
		}

		shi->vn[0]= l*n3[0]-u*n1[0]-v*n2[0];
		shi->vn[1]= l*n3[1]-u*n1[1]-v*n2[1];
		shi->vn[2]= l*n3[2]-u*n1[2]-v*n2[2];

		Normalise(shi->vn);

		if(shi->osatex && (texco & (TEXCO_NORM|TEXCO_REFL)) ) {
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
		VECCOPY(shi->vn, vlr->n);
	}

	/* texture coordinates. O.dxuv O.dyuv have been set */
	if(texco & NEED_UV) {
		if(texco & TEXCO_ORCO) {
			if(v1->orco) {
				float *o1, *o2, *o3;
				
				o1= v1->orco;
				o2= v2->orco;
				o3= v3->orco;
				
				shi->lo[0]= l*o3[0]-u*o1[0]-v*o2[0];
				shi->lo[1]= l*o3[1]-u*o1[1]-v*o2[1];
				shi->lo[2]= l*o3[2]-u*o1[2]-v*o2[2];
	
				if(shi->osatex) {
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
		
		if(texco & TEXCO_GLOB) {
			VECCOPY(shi->gl, shi->co);
			MTC_Mat4MulVecfl(R.viewinv, shi->gl);
			if(shi->osatex) {
				VECCOPY(O.dxgl, O.dxco);
				MTC_Mat3MulVecfl(R.imat, O.dxco);
				VECCOPY(O.dygl, O.dyco);
				MTC_Mat3MulVecfl(R.imat, O.dyco);
			}
		}
		if((texco & TEXCO_UV) || (mode & (MA_VERTEXCOL|MA_FACETEXTURE)))  {
			int j1=i1, j2=i2, j3=i3;
			
			/* to prevent storing new tfaces or vcols, we check a split runtime */
			/* 		4---3		4---3 */
			/*		|\ 1|	or  |1 /| */
			/*		|0\ |		|/ 0| */
			/*		1---2		1---2 	0 = orig face, 1 = new face */
			
			/* Update vert nums to point to correct verts of original face */
			if(vlr->flag & R_DIVIDE_24) {  
				if(vlr->flag & R_FACE_SPLIT) {
					j1++; j2++; j3++;
				}
				else {
					j3++;
				}
			}
			else if(vlr->flag & R_FACE_SPLIT) {
				j2++; j3++; 
			}
			
			if(mode & MA_VERTEXCOL) {
				
				if(vlr->vcol) {
					char *cp1, *cp2, *cp3;
					
					cp1= (char *)(vlr->vcol+j1);
					cp2= (char *)(vlr->vcol+j2);
					cp3= (char *)(vlr->vcol+j3);

					shi->vcol[0]= (l*cp3[3]-u*cp1[3]-v*cp2[3])/255.0;
					shi->vcol[1]= (l*cp3[2]-u*cp1[2]-v*cp2[2])/255.0;
					shi->vcol[2]= (l*cp3[1]-u*cp1[1]-v*cp2[1])/255.0;
					
				}
				else {
					shi->vcol[0]= 0.0;
					shi->vcol[1]= 0.0;
					shi->vcol[2]= 0.0;
				}
			}
			if(vlr->tface) {
				float *uv1, *uv2, *uv3;
				
				uv1= vlr->tface->uv[j1];
				uv2= vlr->tface->uv[j2];
				uv3= vlr->tface->uv[j3];
				
				shi->uv[0]= -1.0 + 2.0*(l*uv3[0]-u*uv1[0]-v*uv2[0]);
				shi->uv[1]= -1.0 + 2.0*(l*uv3[1]-u*uv1[1]-v*uv2[1]);
				shi->uv[2]= 0.0;	// texture.c assumes there are 3 coords
				
				if(shi->osatex) {
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
				
				if(mode & MA_FACETEXTURE) {
					if((mode & MA_VERTEXCOL)==0) {
						shi->vcol[0]= 1.0;
						shi->vcol[1]= 1.0;
						shi->vcol[2]= 1.0;
					}
					if(vlr->tface) render_realtime_texture(shi);
				}
			}
			else {
				shi->uv[0]= 2.0*(u+.5);
				shi->uv[1]= 2.0*(v+.5);
				shi->uv[2]= 0.0;	// texture.c assumes there are 3 coords
			}
		}
		if(texco & TEXCO_NORM) {
			shi->orn[0]= shi->vn[0];
			shi->orn[1]= shi->vn[1];
			shi->orn[2]= shi->vn[2];
		}
				
		if(mode & MA_RADIO) {
			shi->rad[0]= (l*v3->rad[0] - u*v1->rad[0] - v*v2->rad[0]);
			shi->rad[1]= (l*v3->rad[1] - u*v1->rad[1] - v*v2->rad[1]);
			shi->rad[2]= (l*v3->rad[2] - u*v1->rad[2] - v*v2->rad[2]);
		}
		else {
			shi->rad[0]= shi->rad[1]= shi->rad[2]= 0.0;
		}
			
		if(texco & TEXCO_REFL) {
			/* mirror reflection colour textures (and envmap) */
			calc_R_ref(shi);
		}
		
	}
	else {
		shi->rad[0]= shi->rad[1]= shi->rad[2]= 0.0;
	}
}

  /* x,y: window coordinate from 0 to rectx,y */
  /* return pointer to rendered face */
  
float bluroffsx, bluroffsy;	// set in initrender.c (ton)

void *shadepixel(float x, float y, int vlaknr, int mask, float *col)
{
	ShadeResult shr;
	ShadeInput shi;
	VlakRen *vlr=NULL;
	
	if(vlaknr< 0) {	/* error */
		return NULL;
	}
	/* currently in use for dithering soft shadow */
	shi.xs= x;
	shi.ys= y;
	
	/* mask is used to indicate amount of samples (ray shad/mir and AO) */
	shi.mask= mask;
	
	if(vlaknr==0) {	/* sky */
		col[0]= 0.0; col[1]= 0.0; col[2]= 0.0; col[3]= 0.0;
	}
	else if( (vlaknr & 0x7FFFFF) <= R.totvlak) {
		VertRen *v1, *v2, *v3;
		float alpha, fac, dvlak, deler;
		
		vlr= RE_findOrAddVlak( (vlaknr-1) & 0x7FFFFF);
		
		shi.mat= vlr->mat;
		shi.matren= shi.mat->ren;
		shi.vlr= vlr;
		shi.osatex= (shi.matren->texco & TEXCO_OSA);

		v1= vlr->v1;
		dvlak= v1->co[0]*vlr->n[0]+v1->co[1]*vlr->n[1]+v1->co[2]*vlr->n[2];

		/* COXYZ AND VIEW VECTOR  */
		shi.view[0]= (x+(R.xstart)+bluroffsx +0.5);

		if(R.flag & R_SEC_FIELD) {
			if(R.r.mode & R_ODDFIELD) shi.view[1]= (y+R.ystart)*R.ycor;
			else shi.view[1]= (y+R.ystart+1.0)*R.ycor;
		}
		else shi.view[1]= (y+R.ystart+bluroffsy+0.5)*R.ycor;
		
		shi.view[2]= -R.viewfac;

		if(R.r.mode & R_PANORAMA) {
			float panoco, panosi, u, v;
			panoco = getPanovCo();
			panosi = getPanovSi();

			u= shi.view[0]; v= shi.view[2];
			shi.view[0]= panoco*u + panosi*v;
			shi.view[2]= -panosi*u + panoco*v;
		}

		deler= vlr->n[0]*shi.view[0] + vlr->n[1]*shi.view[1] + vlr->n[2]*shi.view[2];
		if (deler!=0.0) fac= R.zcor= dvlak/deler;
		else fac= R.zcor= 0.0;
		
		shi.co[0]= fac*shi.view[0];
		shi.co[1]= fac*shi.view[1];
		shi.co[2]= fac*shi.view[2];
		
		/* pixel dx/dy for render coord */
		if(shi.osatex || (R.r.mode & R_SHADOW) ) {
			float u= dvlak/(deler-vlr->n[0]);
			float v= dvlak/(deler- R.ycor*vlr->n[1]);

			O.dxco[0]= shi.co[0]- (shi.view[0]-1.0)*u;
			O.dxco[1]= shi.co[1]- (shi.view[1])*u;
			O.dxco[2]= shi.co[2]- (shi.view[2])*u;

			O.dyco[0]= shi.co[0]- (shi.view[0])*v;
			O.dyco[1]= shi.co[1]- (shi.view[1]-1.0*R.ycor)*v;
			O.dyco[2]= shi.co[2]- (shi.view[2])*v;

		}

		fac= Normalise(shi.view);
		R.zcor*= fac;	/* for mist */
		
		if(shi.osatex) {
			if( (shi.matren->texco & TEXCO_REFL) ) {
				O.dxview= -1.0/fac;
				O.dyview= -R.ycor/fac;
			}
		}
		
		/* calcuate normals, texture coords, vertex colors, etc */
		if(vlaknr & 0x800000)
			shade_input_set_coords(&shi, 1.0, 1.0, 0, 2, 3);
		else 
			shade_input_set_coords(&shi, 1.0, 1.0, 0, 1, 2);

		/* this only avalailable for scanline */
		if(shi.matren->texco & TEXCO_WINDOW) {
			shi.winco[0]= (x+(R.xstart))/(float)R.afmx;
			shi.winco[1]= (y+(R.ystart))/(float)R.afmy;
		}
		/* after this the u and v AND O.dxuv and O.dyuv are incorrect */
		if(shi.matren->texco & TEXCO_STICKY) {
			if(v1->sticky) {
				extern float Zmulx, Zmuly;
				float *o1, *o2, *o3, hox, hoy, l, dl, u, v;
				float s00, s01, s10, s11, detsh;
				
				if(vlaknr & 0x800000) {
					v2= vlr->v3; v3= vlr->v4;
				} else {
					v2= vlr->v2; v3= vlr->v3;
				}
				
				s00= v3->ho[0]/v3->ho[3] - v1->ho[0]/v1->ho[3];
				s01= v3->ho[1]/v3->ho[3] - v1->ho[1]/v1->ho[3];
				s10= v3->ho[0]/v3->ho[3] - v2->ho[0]/v2->ho[3];
				s11= v3->ho[1]/v3->ho[3] - v2->ho[1]/v2->ho[3];
				
				detsh= s00*s11-s10*s01;
				s00/= detsh; s01/=detsh; 
				s10/=detsh; s11/=detsh;
	
				/* recalc u and v again */
				hox= x/Zmulx -1.0;
				hoy= y/Zmuly -1.0;
				u= (hox - v3->ho[0]/v3->ho[3])*s11 - (hoy - v3->ho[1]/v3->ho[3])*s10;
				v= (hoy - v3->ho[1]/v3->ho[3])*s00 - (hox - v3->ho[0]/v3->ho[3])*s01;
				l= 1.0+u+v;
				
				o1= v1->sticky;
				o2= v2->sticky;
				o3= v3->sticky;
				
				shi.sticky[0]= l*o3[0]-u*o1[0]-v*o2[0];
				shi.sticky[1]= l*o3[1]-u*o1[1]-v*o2[1];
	
				if(shi.osatex) {
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
		
		/* ------  main shading loop */
		shade_lamp_loop(&shi, &shr);
		
		if(shi.matren->translucency!=0.0) {
			ShadeResult shr_t;
			
			VecMulf(shi.vn, -1.0);
			VecMulf(shi.vlr->n, -1.0);
			shade_lamp_loop(&shi, &shr_t);
			shr.diff[0]+= shi.matren->translucency*shr_t.diff[0];
			shr.diff[1]+= shi.matren->translucency*shr_t.diff[1];
			shr.diff[2]+= shi.matren->translucency*shr_t.diff[2];
			VecMulf(shi.vn, -1.0);
			VecMulf(shi.vlr->n, -1.0);
		}
		
		if(R.r.mode & R_RAYTRACE) {
			if(shi.matren->ray_mirror!=0.0 || (shi.mat->mode & MA_RAYTRANSP && shr.alpha!=1.0)) {
				ray_trace(&shi, &shr);
			}
		}
		else {
			// doesnt look 'correct', but is better for preview, plus envmaps dont raytrace this
			if(shi.mat->mode & MA_RAYTRANSP) shr.alpha= 1.0;
		}
		
		VecAddf(col, shr.diff, shr.spec);
		
		/* exposure correction */
		if(R.wrld.exp!=0.0 || R.wrld.range!=1.0) {
			if((shi.matren->mode & MA_SHLESS)==0) {
				col[0]= R.wrld.linfac*(1.0-exp( col[0]*R.wrld.logfac) );
				col[1]= R.wrld.linfac*(1.0-exp( col[1]*R.wrld.logfac) );
				col[2]= R.wrld.linfac*(1.0-exp( col[2]*R.wrld.logfac) );
			}
		}
		
		/* MIST */
		if( (R.wrld.mode & WO_MIST) && (shi.matren->mode & MA_NOMIST)==0 ){
			alpha= mistfactor(shi.co);
		}
		else alpha= 1.0;

		if(shr.alpha!=1.0 || alpha!=1.0) {
			fac= alpha*(shr.alpha);
			
			col[3]= fac;
			col[0]*= fac;
			col[1]*= fac;
			col[2]*= fac;
		}
		else col[3]= 1.0;
	}
	
	if(R.flag & R_LAMPHALO) {
		if(vlaknr<=0) {	/* calc view vector and put shi.co at far */
		
			shi.view[0]= (x+(R.xstart)+0.5);

			if(R.flag & R_SEC_FIELD) {
				if(R.r.mode & R_ODDFIELD) shi.view[1]= (y+R.ystart)*R.ycor;
				else shi.view[1]= (y+R.ystart+1.0)*R.ycor;
			}
			else shi.view[1]= (y+R.ystart+0.5)*R.ycor;
			
			shi.view[2]= -R.viewfac;
			
			if(R.r.mode & R_PANORAMA) {
				float u,v, panoco, panosi;
				panoco = getPanovCo();
				panosi = getPanovSi();
				
				u= shi.view[0]; v= shi.view[2];
				shi.view[0]= panoco*u + panosi*v;
				shi.view[2]= -panosi*u + panoco*v;
			}

			shi.co[2]= 0.0;
			
		}
		renderspothalo(&shi, col);
	}
	
	return vlr;
}

void shadepixel_short(float x, float y, int vlaknr, int mask, unsigned short *shortcol)
{
	float colf[4];
	
	shadepixel(x, y, vlaknr, mask, colf);
	
	if(colf[0]<=0.0) shortcol[0]= 0; else if(colf[0]>=1.0) shortcol[0]= 65535;
	else shortcol[0]= 65535.0*colf[0];
	if(colf[1]<=0.0) shortcol[1]= 0; else if(colf[1]>=1.0) shortcol[1]= 65535;
	else shortcol[1]= 65535.0*colf[1];
	if(colf[2]<=0.0) shortcol[2]= 0; else if(colf[2]>=1.0) shortcol[2]= 65535;
	else shortcol[2]= 65535.0*colf[2];
	if(colf[3]<=0.0) shortcol[3]= 0; else if(colf[3]>=1.0) shortcol[3]= 65535;
	else shortcol[3]= 65535.0*colf[3];

	if(usegamtab) {
		shortcol[0]= igamtab2[ shortcol[0] ];
		shortcol[1]= igamtab2[ shortcol[1] ];
		shortcol[2]= igamtab2[ shortcol[2] ];
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

	if( IS_A_POINTER_CODE(*rd)) {	
		ps= (PixStr *) POINTER_FROM_CODE(*rd);
		
		if(ps->vlak0==vlak) return; 
		
		while(ps) {
			if( ps->vlak == vlak ) {
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
	extern float Zjitx,Zjity;
	PixStr *ps;
	float xd, yd, xs, ys;
	unsigned int *rz, *rp, *rt, mask, fullmask;
	unsigned int  *rowbuf1, *rowbuf2, *rowbuf3, *rb1, *rb2, *rb3;
	int a, b;
	long *rd;
	unsigned short *colrb, *acol, shortcol[4];
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
		Zjitx= -xd -0.5;
		Zjity= -yd -0.5;

		if((R.r.mode & R_MBLUR)==0) RE_local_printrenderinfo(0.0, v);

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
		
		if(RE_local_test_break()) break; 
	}
	if(R.flag & (R_ZTRA+R_HALO) ) {	 /* to get back correct values of zbuffer Z for transp and halos */
		xd= jit[0][0];
		yd= jit[0][1];
		Zjitx= -xd -0.5;
		Zjity= -yd -0.5;
		RE_setwindowclip(0, -1);
		if((R.r.mode & R_MBLUR)==0) RE_local_printrenderinfo(0.0, v);
		zbufferall();
	}

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
				int samp, curmask, face;
				
				if( IS_A_POINTER_CODE(*rd))
					ps= (PixStr *) POINTER_FROM_CODE(*rd);
				else ps= NULL;
				
				if(TRUE) {
					for(samp=0; samp<R.osa; samp++) {
						curmask= 1<<samp;
					
						if(ps) {
							PixStr *ps1= ps;
							while(ps1) {
								if(ps1->mask & curmask) break;
								ps1= ps1->next;
							}
							if(ps1) face= ps1->vlak;
							else face= ps->vlak0;
						}
						else face= (int)*rd;
	
						xs= (float)x + jit[samp][0];
						ys= (float)y + jit[samp][1];
						shadepixel_short(xs, ys, face, curmask, shortcol);
	
						if(shortcol[3]) add_filt_mask(curmask, shortcol, rb1, rb2, rb3);
					}
				}
				else {	/* do collected faces together */
					if(ps) face= ps->vlak0;
					else face= (int)*rd;
					mask= 0;

					while(ps) {
						b= centmask[ps->mask];
						xs= (float)x+centLut[b & 15];
						ys= (float)y+centLut[b>>4];

						shadepixel_short(xs, ys, ps->vlak, ps->mask, shortcol);

						if(shortcol[3]) add_filt_mask(ps->mask, shortcol, rb1, rb2, rb3);

						mask |= ps->mask;
						ps= ps->next;
					}

					mask= (~mask) & fullmask;
					if(mask) {
						b= centmask[mask];
						xs= (float)x+centLut[b & 15];
						ys= (float)y+centLut[b>>4];

						shadepixel_short(xs, ys, face, mask, shortcol);
	
						if(shortcol[3]) add_filt_mask(mask, shortcol, rb1, rb2, rb3);
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
			
		}
		if(y<R.recty) {
			memset(rowbuf3, 0, (R.rectx+4)*4*4);
			rb3= rowbuf3;
			rowbuf3= rowbuf2;
			rowbuf2= rowbuf1;
			rowbuf1= rb3;

			if( y>0) {
				if((y & 1)==0) {
					RE_local_render_display(y-2, y-1,  R.rectx, R.recty, R.rectot);
				}
			}
			rz+= R.rectx;
		}
		if(RE_local_test_break()) break; 
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
	extern float Zjitx,Zjity;
	unsigned int *rz,*rp;
	float fy;
	int x,y;
	unsigned short *acol, shortcol[4];
	char *charcol, *rt;

	Zjitx=Zjity= -0.5;

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
		fy= y;
		
		if(R.flag & R_ZTRA) {		/* zbuf tra */
  			abufsetrow(y); 
			acol= Acolrow;
			
			for(x=0; x<R.rectx; x++, rp++, acol+= 4) {

  				shadepixel_short((float)x, fy, *rp, 0, shortcol);
				
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
				shadepixel_short((float)x, fy, *rp, 0, shortcol);
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
		
		if(y & 1) {
			RE_local_render_display(y-1, y, R.rectx, R.recty, R.rectot);
		}
		
		if(RE_local_test_break()) break; 
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
				
				if(RE_local_test_break()) break; 
			}
	
		}
	}
} /* end of void renderhalo(HaloRen *har), postprocess version */

/* ------------------------------------------------------------------------ */

void RE_renderflare(HaloRen *har)
{
	extern float hashvectf[];
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


