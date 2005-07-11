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
#include "MTC_matrixops.h"

#include "BKE_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_texture_types.h"

#include "BKE_global.h"
#include "BKE_texture.h"

#include "BLI_rand.h"

/* local include */
#include "RE_callbacks.h"
#include "render.h"
#include "zbuf.h"		/* stuff like bgnaccumbuf, fillrect, ...*/
#include "pixelblending.h"
#include "pixelshading.h"
#include "vanillaRenderPipe.h"	/* transfercolour... */
#include "gammaCorrectionTables.h"
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "SDL_thread.h"


/* global for this file. struct render will be more dynamic later, to allow multiple renderers */
RE_Render R;

float bluroffsx=0.0, bluroffsy=0.0;	// set in initrender.c (bad, ton)

/* x and y are current pixels to be rendered */
void calc_view_vector(float *view, float x, float y)
{
	
	if(R.r.mode & R_ORTHO) {
		view[0]= view[1]= 0.0;
	}
	else {
		view[0]= (x+(R.xstart)+bluroffsx +0.5);
		
		if(R.flag & R_SEC_FIELD) {
			if(R.r.mode & R_ODDFIELD) view[1]= (y+R.ystart)*R.ycor;
			else view[1]= (y+R.ystart+1.0)*R.ycor;
		}
		else view[1]= (y+R.ystart+bluroffsy+0.5)*R.ycor;
	}	
	view[2]= -R.viewfac;
	
	if(R.r.mode & R_PANORAMA) {
		float panoco, panosi, u, v;
		panoco = getPanovCo();
		panosi = getPanovSi();
		
		u= view[0]; v= view[2];
		view[0]= panoco*u + panosi*v;
		view[2]= -panosi*u + panoco*v;
	}
}

float mistfactor(float zcor, float *co)	/* dist en height, return alpha */
{
	float fac, hi;
	
	fac= zcor - R.wrld.miststa;	/* zcor is calculated per pixel */

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

	return (1.0-fac)* (1.0-R.wrld.misi);	
}

/* external for preview only */
void RE_sky_char(float *view, char *col)
{
	float f, colf[3];
	float dither_value;

	dither_value = ( (BLI_frand()-0.5)*R.r.dither_intensity)/256.0; 
	
	shadeSkyPixelFloat(colf, view, NULL);
	
	f= 255.0*(colf[0]+dither_value);
	if(f<=0.0) col[0]= 0; else if(f>255.0) col[0]= 255;
	else col[0]= (char)f;
	f= 255.0*(colf[1]+dither_value);
	if(f<=0.0) col[1]= 0; else if(f>255.0) col[1]= 255;
	else col[1]= (char)f;
	f= 255.0*(colf[2]+dither_value);
	if(f<=0.0) col[2]= 0; else if(f>255.0) col[2]= 255;
	else col[2]= (char)f;
	col[3]= 1;	/* to prevent wrong optimalisation alphaover of flares */
}


/* ************************************** */


static void spothalo(struct LampRen *lar, ShadeInput *shi, float *intens)
{
	double a, b, c, disc, nray[3], npos[3];
	float t0, t1 = 0.0, t2= 0.0, t3, haint;
	float p1[3], p2[3], ladist, maxz = 0.0, maxy = 0.0;
	int snijp, doclip=1, use_yco=0;
	int ok1=0, ok2=0;
	
	*intens= 0.0;
	haint= lar->haint;
	
	if(R.r.mode & R_ORTHO) {
		/* camera pos (view vector) cannot be used... */
		/* camera position (cox,coy,0) rotate around lamp */
		p1[0]= shi->co[0]-lar->co[0];
		p1[1]= shi->co[1]-lar->co[1];
		p1[2]= -lar->co[2];
		MTC_Mat3MulVecfl(lar->imat, p1);
		VECCOPY(npos, p1);	// npos is double!
	}
	else {
		VECCOPY(npos, lar->sh_invcampos);	/* in initlamp calculated */
	}
	
	/* rotate view */
	VECCOPY(nray, shi->view);
	MTC_Mat3MulVecd(lar->imat, nray);
	
	if(R.wrld.mode & WO_MIST) {
		/* patchy... */
		haint *= mistfactor(-lar->co[2], lar->co);
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


static int calchalo_z(HaloRen *har, int zz)
{

	if(har->type & HA_ONLYSKY) {
		if(zz!=0x7FFFFFFF) zz= - 0x7FFFFF;
	}
	else {
		zz= (zz>>8);
	}
	return zz;
}

static void scanlinehaloPS(int *rectz, long *rectdelta, float *rowbuf, short ys)
{
	HaloRen *har = NULL;
	PixStr *ps;
	float dist, xsq, ysq, xn, yn;
	float *rb;
	float col[4], accol[4];
	int a, *rz, zz, didgamma=0;
	long *rd;
	short minx, maxx, x, amount, amountm, flarec;

	for(a=0; a<R.tothalo; a++) {
		if((a & 255)==0) {
			har= R.bloha[a>>8];
			if( RE_local_test_break() ) break;  
		}
		else har++;

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

				rb= rowbuf + 4*minx;
				rd= rectdelta + minx;
				rz= rectz + minx;

				yn= (ys-har->ys)*R.ycor;
				ysq= yn*yn;
				for(x=minx; x<=maxx; x++) {
					
					xn= x-har->xs;
					xsq= xn*xn;
					dist= xsq+ysq;
					
					if(dist<har->radsq) {
						
						/* well yah, halo adding shouldnt be done gamma corrected, have to bypass it this way */
						/* alternative is moving it outside of thread renderlineDA */
						/* on positive side; the invert correct cancels out correcting halo color */
						if(do_gamma && didgamma==0) {
							float *buf= rowbuf;
							int xt;
							for(xt=0; xt<R.rectx; xt++, buf+=4) {
								buf[0]= sqrt(buf[0]);	// invers gamma 2.0
								buf[1]= sqrt(buf[1]);
								buf[2]= sqrt(buf[2]);
							}
							didgamma= 1;
						}
						
						flarec= har->flarec;	/* har->pixels is only allowed to count once */
						
						if(*rd) {				/* theres a pixel struct */
							
							ps= (PixStr *)(*rd);
							amount= 0;
							accol[0]=accol[1]=accol[2]=accol[3]= 0.0;
							
							while(ps) {
								amountm= count_mask(ps->mask);
								amount+= amountm;

								zz= calchalo_z(har, ps->z);
								if(zz> har->zs) {
									float fac;
									
									shadeHaloFloat(har, col, zz, dist, xn, yn, flarec);
									fac= ((float)amountm)/(float)R.osa;
									accol[0]+= fac*col[0];
									accol[1]+= fac*col[1];
									accol[2]+= fac*col[2];
									accol[3]+= fac*col[3];
									flarec= 0;
								}

								ps= ps->next;
							}
							/* now do the sky sub-pixels */
							amount= R.osa-amount;
							if(amount) {
								float fac;

								shadeHaloFloat(har, col, 0x7FFFFF, dist, xn, yn, flarec);
								fac= ((float)amount)/(float)R.osa;
								accol[0]+= fac*col[0];
								accol[1]+= fac*col[1];
								accol[2]+= fac*col[2];
								accol[3]+= fac*col[3];
							}
							col[0]= accol[0];
							col[1]= accol[1];
							col[2]= accol[2];
							col[3]= accol[3];

							addalphaAddfacFloat(rb, col, har->add);
						}
						else {
							zz= calchalo_z(har, *rz);
							if(zz> har->zs) {

								shadeHaloFloat(har, col, zz, dist, xn, yn, flarec);
								addalphaAddfacFloat(rb, col, har->add);
							}
						}
					}
					rb+=4;
					rz++;
					rd++;
				}
			}
		}
	}

	/* the entire scanline has to be put back in gammaspace */
	if(didgamma) {
		float *buf= rowbuf;
		int xt;
		for(xt=0; xt<R.rectx; xt++, buf+=4) {
			buf[0]*= (buf[0]);	// gamma 2.0
			buf[1]*= (buf[1]);
			buf[2]*= (buf[2]);
		}
	}

}

static void scanlinehalo(int *rectz, float *rowbuf, short ys)
{
	HaloRen *har = NULL;
	float dist, xsq, ysq, xn, yn, *rb;
	float col[4];
	int a, *rz, zz;
	short minx, maxx, x;

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

				rb= rowbuf + 4*minx;
				rz= rectz + minx;

				yn= (ys-har->ys)*R.ycor;
				ysq= yn*yn;
				for(x=minx; x<=maxx; x++) {
				
					zz= calchalo_z(har, *rz);
					if(zz> har->zs) {
						xn= x- har->xs;
						xsq= xn*xn;
						dist= xsq+ysq;
						if(dist<har->radsq) {
							shadeHaloFloat(har, col, zz, dist, xn, yn, har->flarec);
							addalphaAddfacFloat(rb, col, har->add);
						}
					}

					rb+=4;
					rz++;
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

	rad[0]= saacos(rad[0]);
	rad[1]= saacos(rad[1]);
	rad[2]= saacos(rad[2]);
	rad[3]= saacos(rad[3]);

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
	ang = saacos(nh);

	i= f * g * exp((double)(-(ang*ang) / (2.0*spec_power*spec_power)));
	if(i<0.0) i= 0.0;
	
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
	
	ang = saacos( rslt ); 
	
	if( ang < size ) rslt = 1.0;
	else if( ang >= (size + smooth) || smooth == 0.0 ) rslt = 0.0;
	else rslt = 1.0 - ((ang - size) / smooth);
	
	return rslt;
}

/* Ward isotropic gaussian spec */
float WardIso_Spec( float *n, float *l, float *v, float rms)
{
	float i, nh, nv, nl, h[3], angle, alpha;


	/* half-way vector */
	h[0] = l[0] + v[0];
	h[1] = l[1] + v[1];
	h[2] = l[2] + v[2];
	Normalise(h);

	nh = n[0]*h[0]+n[1]*h[1]+n[2]*h[2]; /* Dot product between surface normal and half-way vector */
	if(nh<=0.0) nh = 0.001;
	
	nv = n[0]*v[0]+n[1]*v[1]+n[2]*v[2]; /* Dot product between surface normal and view vector */
	if(nv<=0.0) nv = 0.001;

	nl = n[0]*l[0]+n[1]*l[1]+n[2]*l[2]; /* Dot product between surface normal and light vector */
	if(nl<=0.0) nl = 0.001;

	angle = tan(saacos(nh));
	alpha = MAX2(rms,0.001);

	i= nl * (1.0/(4*PI*alpha*alpha)) * (exp( -(angle*angle)/(alpha*alpha))/(sqrt(nv*nl)));

	return i;
}

/* cartoon render diffuse */
float Toon_Diff( float *n, float *l, float *v, float size, float smooth )
{
	float rslt, ang;

	rslt = n[0]*l[0] + n[1]*l[1] + n[2]*l[2];

	ang = saacos( (double)(rslt) );

	if( ang < size ) rslt = 1.0;
	else if( ang >= (size + smooth) || smooth == 0.0 ) rslt = 0.0;
	else rslt = 1.0 - ((ang - size) / smooth);

	return rslt;
}

/* Oren Nayar diffuse */

/* 'nl' is either dot product, or return value of area light */
/* in latter case, only last multiplication uses 'nl' */
static float OrenNayar_Diff_i(float nl, float *n, float *l, float *v, float rough )
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
	
	Lit_A = saacos(realnl);
	View_A = saacos( nv );
	
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

/* Minnaert diffuse */
float Minnaert_Diff(float nl, float *n, float *v, float darkness)
{

float i, nv;

        /* nl = dot product between surface normal and light vector */
        if (nl <= 0.0)
	        return 0;

	/* nv = dot product between surface normal and view vector */
	nv = n[0]*v[0]+n[1]*v[1]+n[2]*v[2];
	if (nv < 0.0)
		nv = 0;

	if (darkness <= 1)
		i = nl * pow(MAX2(nv*nl, 0.1), (darkness - 1) ); /*The Real model*/
	else
		i = nl * pow( (1.001 - nv), (darkness  - 1) ); /*Nvidia model*/

	return i;
}

/* --------------------------------------------- */
/* also called from texture.c */
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
			i= -2*( (shi->vn[0]+shi->dxno[0])*(shi->view[0]+shi->dxview) +
				(shi->vn[1]+shi->dxno[1])*shi->view[1]+ (shi->vn[2]+shi->dxno[2])*shi->view[2] );

			shi->dxref[0]= shi->ref[0]- ( shi->view[0]+shi->dxview+i*(shi->vn[0]+shi->dxno[0]));
			shi->dxref[1]= shi->ref[1]- (shi->view[1]+ i*(shi->vn[1]+shi->dxno[1]));
			shi->dxref[2]= shi->ref[2]- (shi->view[2]+ i*(shi->vn[2]+shi->dxno[2]));

			i= -2*( (shi->vn[0]+shi->dyno[0])*shi->view[0]+
				(shi->vn[1]+shi->dyno[1])*(shi->view[1]+shi->dyview)+ (shi->vn[2]+shi->dyno[2])*shi->view[2] );

			shi->dyref[0]= shi->ref[0]- (shi->view[0]+ i*(shi->vn[0]+shi->dyno[0]));
			shi->dyref[1]= shi->ref[1]- (shi->view[1]+shi->dyview+i*(shi->vn[1]+shi->dyno[1]));
			shi->dyref[2]= shi->ref[2]- (shi->view[2]+ i*(shi->vn[2]+shi->dyno[2]));

		}
		else {

			i= -2*( shi->vn[0]*(shi->view[0]+shi->dxview) +
				shi->vn[1]*shi->view[1]+ shi->vn[2]*shi->view[2] );

			shi->dxref[0]= shi->ref[0]- (shi->view[0]+shi->dxview+i*shi->vn[0]);
			shi->dxref[1]= shi->ref[1]- (shi->view[1]+ i*shi->vn[1]);
			shi->dxref[2]= shi->ref[2]- (shi->view[2]+ i*shi->vn[2]);

			i= -2*( shi->vn[0]*shi->view[0]+
				shi->vn[1]*(shi->view[1]+shi->dyview)+ shi->vn[2]*shi->view[2] );

			shi->dyref[0]= shi->ref[0]- (shi->view[0]+ i*shi->vn[0]);
			shi->dyref[1]= shi->ref[1]- (shi->view[1]+shi->dyview+i*shi->vn[1]);
			shi->dyref[2]= shi->ref[2]- (shi->view[2]+ i*shi->vn[2]);
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
	Material *ma= shi->mat;

	if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
		shi->r= shi->vcol[0];
		shi->g= shi->vcol[1];
		shi->b= shi->vcol[2];
	}
	
	if(ma->texco) {
		if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
			shi->r= shi->vcol[0];
			shi->g= shi->vcol[1];
			shi->b= shi->vcol[2];
		}
		do_material_tex(shi);
	}

	if(ma->mode & (MA_ZTRA|MA_RAYTRANSP)) {
		if(ma->fresnel_tra!=0.0) 
			shi->alpha*= fresnel_fac(shi->view, shi->vn, ma->fresnel_tra_i, ma->fresnel_tra);
	}

	shr->diff[0]= shi->r;
	shr->diff[1]= shi->g;
	shr->diff[2]= shi->b;
	shr->alpha= shi->alpha;
}

/* r g b = 1 value, col = vector */
static void ramp_blend(int type, float *r, float *g, float *b, float fac, float *col)
{
	float tmp, facm= 1.0-fac;
	
	switch (type) {
	case MA_RAMP_BLEND:
		*r = facm*(*r) + fac*col[0];
		*g = facm*(*g) + fac*col[1];
		*b = facm*(*b) + fac*col[2];
		break;
	case MA_RAMP_ADD:
		*r += fac*col[0];
		*g += fac*col[1];
		*b += fac*col[2];
		break;
	case MA_RAMP_MULT:
		*r *= (facm + fac*col[0]);
		*g *= (facm + fac*col[1]);
		*b *= (facm + fac*col[2]);
		break;
	case MA_RAMP_SCREEN:
		*r = 1.0-(facm + (1.0 - col[0]))*(1.0 - *r);
		*g = 1.0-(facm + (1.0 - col[1]))*(1.0 - *g);
		*b = 1.0-(facm + (1.0 - col[2]))*(1.0 - *b);
		break;
	case MA_RAMP_SUB:
		*r -= fac*col[0];
		*g -= fac*col[1];
		*b -= fac*col[2];
		break;
	case MA_RAMP_DIV:
		if(col[0]!=0.0)
			*r = facm*(*r) + fac*(*r)/col[0];
		if(col[1]!=0.0)
			*g = facm*(*g) + fac*(*g)/col[1];
		if(col[2]!=0.0)
			*b = facm*(*b) + fac*(*b)/col[2];
		break;
	case MA_RAMP_DIFF:
		*r = facm*(*r) + fac*fabs(*r-col[0]);
		*g = facm*(*g) + fac*fabs(*g-col[1]);
		*b = facm*(*b) + fac*fabs(*b-col[2]);
		break;
	case MA_RAMP_DARK:
		tmp= fac*col[0];
		if(tmp < *r) *r= tmp; 
		tmp= fac*col[1];
		if(tmp < *g) *g= tmp; 
		tmp= fac*col[2];
		if(tmp < *b) *b= tmp; 
		break;
	case MA_RAMP_LIGHT:
		tmp= fac*col[0];
		if(tmp > *r) *r= tmp; 
		tmp= fac*col[1];
		if(tmp > *g) *g= tmp; 
		tmp= fac*col[2];
		if(tmp > *b) *b= tmp; 
		break;
	}

}

/* ramp for at end of shade */
void ramp_diffuse_result(float *diff, ShadeInput *shi)
{
	Material *ma= shi->mat;
	float col[4], fac=0;

	if(ma->ramp_col) {
		if(ma->rampin_col==MA_RAMP_IN_RESULT) {
			
			fac= 0.3*diff[0] + 0.58*diff[1] + 0.12*diff[2];
			do_colorband(ma->ramp_col, fac, col);
			
			/* blending method */
			fac= col[3]*ma->rampfac_col;
			
			ramp_blend(ma->rampblend_col, diff, diff+1, diff+2, fac, col);
		}
	}
}

/* r,g,b denote energy, ramp is used with different values to make new material color */
void add_to_diffuse(float *diff, ShadeInput *shi, float is, float r, float g, float b)
{
	Material *ma= shi->mat;
	float col[4], colt[3], fac=0;
	
	if(ma->ramp_col && (ma->mode & MA_RAMP_COL)) {
		
		/* MA_RAMP_IN_RESULT is exceptional */
		if(ma->rampin_col==MA_RAMP_IN_RESULT) {
			// normal add
			diff[0] += r * shi->r;
			diff[1] += g * shi->g;
			diff[2] += b * shi->b;
		}
		else {
			/* input */
			switch(ma->rampin_col) {
			case MA_RAMP_IN_ENERGY:
				fac= 0.3*r + 0.58*g + 0.12*b;
				break;
			case MA_RAMP_IN_SHADER:
				fac= is;
				break;
			case MA_RAMP_IN_NOR:
				fac= shi->view[0]*shi->vn[0] + shi->view[1]*shi->vn[1] + shi->view[2]*shi->vn[2];
				break;
			}
	
			do_colorband(ma->ramp_col, fac, col);
			
			/* blending method */
			fac= col[3]*ma->rampfac_col;
			colt[0]= shi->r; colt[1]= shi->g; colt[2]= shi->b;

			ramp_blend(ma->rampblend_col, colt, colt+1, colt+2, fac, col);

			/* output to */
			diff[0] += r * colt[0];
			diff[1] += g * colt[1];
			diff[2] += b * colt[2];
		}
	}
	else {
		diff[0] += r * shi->r;
		diff[1] += g * shi->g;
		diff[2] += b * shi->b;
	}
}

void ramp_spec_result(float *specr, float *specg, float *specb, ShadeInput *shi)
{
	Material *ma= shi->mat;
	float col[4];
	float fac;
	
	if(ma->ramp_spec && (ma->rampin_spec==MA_RAMP_IN_RESULT)) {
		fac= 0.3*(*specr) + 0.58*(*specg) + 0.12*(*specb);
		do_colorband(ma->ramp_spec, fac, col);
		
		/* blending method */
		fac= col[3]*ma->rampfac_spec;
		
		ramp_blend(ma->rampblend_spec, specr, specg, specb, fac, col);
		
	}
}

/* is = dot product shade, t = spec energy */
void do_specular_ramp(ShadeInput *shi, float is, float t, float *spec)
{
	Material *ma= shi->mat;
	float col[4];
	float fac=0.0;
	
	spec[0]= shi->specr;
	spec[1]= shi->specg;
	spec[2]= shi->specb;

	/* MA_RAMP_IN_RESULT is exception */
	if(ma->ramp_spec && (ma->rampin_spec!=MA_RAMP_IN_RESULT)) {
		
		/* input */
		switch(ma->rampin_spec) {
		case MA_RAMP_IN_ENERGY:
			fac= t;
			break;
		case MA_RAMP_IN_SHADER:
			fac= is;
			break;
		case MA_RAMP_IN_NOR:
			fac= shi->view[0]*shi->vn[0] + shi->view[1]*shi->vn[1] + shi->view[2]*shi->vn[2];
			break;
		}
		
		do_colorband(ma->ramp_spec, fac, col);
		
		/* blending method */
		fac= col[3]*ma->rampfac_spec;
		
		ramp_blend(ma->rampblend_spec, spec, spec+1, spec+2, fac, col);
	}
}



static void ambient_occlusion(World *wrld, ShadeInput *shi, ShadeResult *shr)
{
	float f, shadfac[4];
	
	if((wrld->mode & WO_AMB_OCC) && (R.r.mode & R_RAYTRACE) && shi->amb!=0.0) {
		ray_ao(shi, wrld, shadfac);

		if(wrld->aocolor==WO_AOPLAIN) {
			if (wrld->aomix==WO_AOADDSUB) shadfac[3] = 2.0*shadfac[3]-1.0;
			else if (wrld->aomix==WO_AOSUB) shadfac[3] = shadfac[3]-1.0;

			f= wrld->aoenergy*shadfac[3]*shi->amb;
			add_to_diffuse(shr->diff, shi, f, f, f, f);
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
			f= wrld->aoenergy*shi->amb;
			add_to_diffuse(shr->diff, shi, f, f*shadfac[0], f*shadfac[1], f*shadfac[2]);
		}
	}
}

void shade_lamp_loop(ShadeInput *shi, ShadeResult *shr)
{
	LampRen *lar;
	Material *ma= shi->mat;
	VlakRen *vlr= shi->vlr;
	float i, inp, inpr, is, t, lv[3], lacol[3], lampdist, ld = 0;
	float lvrot[3], *vn, *view, shadfac[4], soft, phongcorr;	// shadfac = rgba
	int a;

	vn= shi->vn;
	view= shi->view;
	
	memset(shr, 0, sizeof(ShadeResult));
	
	/* separate loop */
	if(ma->mode & MA_ONLYSHADOW) {
		float ir;
		
		if(R.r.mode & R_SHADOW) {
			
			shadfac[3]= ir= 0.0;
			for(a=0; a<R.totlamp; a++) {
				lar= R.la[a];
				/* yafray: ignore shading by photonlights, not used in Blender */
				if (lar->type==LA_YF_PHOTON) continue;
				
				if(lar->mode & LA_LAYER) if((lar->lay & vlr->lay)==0) continue;
				
				lv[0]= shi->co[0]-lar->co[0];
				lv[1]= shi->co[1]-lar->co[1];
				lv[2]= shi->co[2]-lar->co[2];

				if(lar->type==LA_SPOT) {
					/* only test within spotbundel */
					if(lar->shb || (lar->mode & LA_SHAD_RAY)) {

						Normalise(lv);
						inpr= lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2];
						if(inpr>lar->spotsi) {
							
							inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];
							
							if(lar->shb) i = testshadowbuf(lar->shb, shi->co, shi->dxco, shi->dyco, inp);
							else {
								float shad[4];
								ray_shadow(shi, lar, shad);
								i= shad[3];
							}
							
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
				}
				else if(lar->mode & LA_SHAD_RAY) {
					float shad[4];
					
					/* single sided? */
					if( shi->facenor[0]*lv[0] + shi->facenor[1]*lv[1] + shi->facenor[2]*lv[2] > -0.01) {
						ray_shadow(shi, lar, shad);
						shadfac[3]+= shad[3];
						ir+= 1.0;
					}
				}

			}
			if(ir>0.0) {
				shadfac[3]/= ir;
				shr->alpha= (shi->alpha)*(1.0-shadfac[3]);
			}
		}
		
		if((R.wrld.mode & WO_AMB_OCC) && (R.r.mode & R_RAYTRACE) && shi->amb!=0.0) {
			float f;

			ray_ao(shi, &R.wrld, shadfac);	// shadfac==0: full light
			shadfac[3]= 1.0-shadfac[3];
			
			f= R.wrld.aoenergy*shadfac[3]*shi->amb;
			
			if(R.wrld.aomix==WO_AOADD) {
				shr->alpha += f;
				shr->alpha *= f;
			}
			else if(R.wrld.aomix==WO_AOSUB) {
				shr->alpha += f;
			}
			else {
				shr->alpha *= f;
				shr->alpha += f;
			}
		}
		
		return;
	}
		
	if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
		shi->r= shi->vcol[0];
		shi->g= shi->vcol[1];
		shi->b= shi->vcol[2];
	}
	
	/* envmap hack, always reset */
	shi->refcol[0]= shi->refcol[1]= shi->refcol[2]= shi->refcol[3]= 0.0;

	if(ma->texco) {
		if(ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE)) {
			shi->r= shi->vcol[0];
			shi->g= shi->vcol[1];
			shi->b= shi->vcol[2];
		}
		do_material_tex(shi);
	}
	
	if(ma->mode & MA_SHLESS) {
		shr->diff[0]= shi->r;
		shr->diff[1]= shi->g;
		shr->diff[2]= shi->b;
		shr->alpha= shi->alpha;
		return;
	}

	if( (ma->mode & (MA_VERTEXCOL|MA_VERTEXCOLP))== MA_VERTEXCOL ) {	// vertexcolor light
		// add_to_diffuse(shr->diff, shi, 1.0, ma->emit+shi->vcol[0], ma->emit+shi->vcol[1], ma->emit+shi->vcol[2]);
		shr->diff[0]= shi->r*(shi->emit+shi->vcol[0]);
		shr->diff[1]= shi->g*(shi->emit+shi->vcol[1]);
		shr->diff[2]= shi->b*(shi->emit+shi->vcol[2]);
	}
	else {
		// add_to_diffuse(shr->diff, shi, 1.0, ma->emit, ma->emit, ma->emit);
		shr->diff[0]= shi->r*shi->emit;
		shr->diff[1]= shi->g*shi->emit;
		shr->diff[2]= shi->b*shi->emit;
	}
	
	ambient_occlusion(&R.wrld, shi, shr);

	for(a=0; a<R.totlamp; a++) {
		lar= R.la[a];
		/* yafray: ignore shading by photonlights, not used in Blender */
		if (lar->type==LA_YF_PHOTON) continue;

		/* test for lamp layer */
		if(lar->mode & LA_LAYER) if((lar->lay & vlr->lay)==0) continue;
		
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

		lacol[0]= lar->r;
		lacol[1]= lar->g;
		lacol[2]= lar->b;
		
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
				
				shi->dxlv[0]= lv[0] - (shi->co[0]-lar->co[0]+shi->dxco[0])/ld;
				shi->dxlv[1]= lv[1] - (shi->co[1]-lar->co[1]+shi->dxco[1])/ld;
				shi->dxlv[2]= lv[2] - (shi->co[2]-lar->co[2]+shi->dxco[2])/ld;

				shi->dylv[0]= lv[0] - (shi->co[0]-lar->co[0]+shi->dyco[0])/ld;
				shi->dylv[1]= lv[1] - (shi->co[1]-lar->co[1]+shi->dyco[1])/ld;
				shi->dylv[2]= lv[2] - (shi->co[2]-lar->co[2]+shi->dyco[2])/ld;
			}
			
		}

		if(lar->mode & LA_TEXTURE)  do_lamp_tex(lar, lv, shi, lacol);
		
		/* dot product and reflectivity */
		/* inp = dotproduct, is = shader result, i = lamp energy (with shadow) */
		
		inp= vn[0]*lv[0] + vn[1]*lv[1] + vn[2]*lv[2];

		/* phong threshold to prevent backfacing faces having artefacts on ray shadow (terminator problem) */
		if((ma->mode & MA_RAYBIAS) && (lar->mode & LA_SHAD_RAY) && (vlr->flag & R_SMOOTH)) {
			float thresh= vlr->ob->smoothresh;
			if(inp>thresh)
				phongcorr= (inp-thresh)/(inp*(1.0-thresh));
			else
				phongcorr= 0.0;
		}
		else phongcorr= 1.0;
		
		/* diffuse shaders */
		if(lar->mode & LA_NO_DIFF) {
			is= 0.0;	// skip shaders
		}
		else if(lar->type==LA_HEMI) {
			is= 0.5*inp + 0.5;
		}
		else {
		
			if(lar->type==LA_AREA) {
				/* single sided */
				if(lv[0]*lar->vec[0]+lv[1]*lar->vec[1]+lv[2]*lar->vec[2]>0.0)
					inp= area_lamp_energy(shi->co, shi->vn, lar);
				else inp= 0.0;
			}
			
			/* diffuse shaders (oren nayer gets inp from area light) */
			if(ma->diff_shader==MA_DIFF_ORENNAYAR) is= OrenNayar_Diff_i(inp, vn, lv, view, ma->roughness);
			else if(ma->diff_shader==MA_DIFF_TOON) is= Toon_Diff(vn, lv, view, ma->param[0], ma->param[1]);
			else if(ma->diff_shader==MA_DIFF_MINNAERT) is= Minnaert_Diff(inp, vn, view, ma->darkness);
			else is= inp;	// Lambert
		}
		
		i= is*phongcorr;
		
		if(i>0.0) {
			i*= lampdist*shi->refl;
		}

		/* shadow and spec, (lampdist==0 outside spot) */
		if(lampdist> 0.0) {
			
			if(i>0.0 && (R.r.mode & R_SHADOW)) {
				if(ma->mode & MA_SHADOW) {
					if(lar->type==LA_HEMI);	// no shadow
					else {
						if(lar->shb) {
							shadfac[3] = testshadowbuf(lar->shb, shi->co, shi->dxco, shi->dyco, inp);
						}
						else if(lar->mode & LA_SHAD_RAY) {
							ray_shadow(shi, lar, shadfac);
						}
	
						/* warning, here it skips the loop */
						if(lar->mode & LA_ONLYSHADOW) {
							
							shadfac[3]= i*lar->energy*(1.0-shadfac[3]);
							shr->diff[0] -= shadfac[3]*shi->r;
							shr->diff[1] -= shadfac[3]*shi->g;
							shr->diff[2] -= shadfac[3]*shi->b;
							
							continue;
						}
						
						if(shadfac[3]==0.0) continue;
	
						i*= shadfac[3];
					}
				}
			}
		
			/* specularity */
			if(shadfac[3]>0.0 && shi->spec!=0.0 && !(lar->mode & LA_NO_SPEC)) {
				
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
					
					t= shadfac[3]*shi->spec*spec(t, shi->har);
					shr->spec[0]+= t*(lacol[0] * shi->specr);
					shr->spec[1]+= t*(lacol[1] * shi->specg);
					shr->spec[2]+= t*(lacol[2] * shi->specb);
				}
				else {
					/* specular shaders */
					float specfac;

					if(ma->spec_shader==MA_SPEC_PHONG) 
						specfac= Phong_Spec(vn, lv, view, shi->har);
					else if(ma->spec_shader==MA_SPEC_COOKTORR) 
						specfac= CookTorr_Spec(vn, lv, view, shi->har);
					else if(ma->spec_shader==MA_SPEC_BLINN) 
						specfac= Blinn_Spec(vn, lv, view, ma->refrac, (float)shi->har);
					else if(ma->spec_shader==MA_SPEC_WARDISO)
						specfac= WardIso_Spec( vn, lv, view, ma->rms);
					else 
						specfac= Toon_Spec(vn, lv, view, ma->param[2], ma->param[3]);
				
					/* area lamp correction */
					if(lar->type==LA_AREA) specfac*= inp;
					
					t= shadfac[3]*shi->spec*lampdist*specfac;
					
					if(ma->mode & MA_RAMP_SPEC) {
						float spec[3];
						do_specular_ramp(shi, specfac, t, spec);
						shr->spec[0]+= t*(lacol[0] * spec[0]);
						shr->spec[1]+= t*(lacol[1] * spec[1]);
						shr->spec[2]+= t*(lacol[2] * spec[2]);
					}
					else {
						shr->spec[0]+= t*(lacol[0] * shi->specr);
						shr->spec[1]+= t*(lacol[1] * shi->specg);
						shr->spec[2]+= t*(lacol[2] * shi->specb);
					}
				}
			}
		}
		
		/* in case 'no diffuse' we still do most calculus, spec can be in shadow */
		if(i>0.0 && !(lar->mode & LA_NO_DIFF)) {
			if(ma->mode & MA_SHADOW_TRA) {
				add_to_diffuse(shr->diff, shi, is, i*shadfac[0]*lacol[0], i*shadfac[1]*lacol[1], i*shadfac[2]*lacol[2]);
			}
			else {
				add_to_diffuse(shr->diff, shi, is, i*lacol[0], i*lacol[1], i*lacol[2]);
			}
		}
	}

	if(ma->mode & (MA_ZTRA|MA_RAYTRANSP)) {
		if(ma->fresnel_tra!=0.0) 
			shi->alpha*= fresnel_fac(shi->view, shi->vn, ma->fresnel_tra_i, ma->fresnel_tra);

		if(shi->spectra!=0.0) {

			t = MAX3(shr->spec[0], shr->spec[1], shr->spec[2]);
			t *= shi->spectra;
			if(t>1.0) t= 1.0;
			shi->alpha= (1.0-t)*shi->alpha+t;
		}
	}

	shr->alpha= shi->alpha;

	if(shr->spec[0]<0.0) shr->spec[0]= 0.0;
	if(shr->spec[1]<0.0) shr->spec[1]= 0.0;
	if(shr->spec[2]<0.0) shr->spec[2]= 0.0;

	shr->diff[0]+= shi->r*shi->amb*shi->rad[0];
	shr->diff[0]+= shi->ambr;
	if(shr->diff[0]<0.0) shr->diff[0]= 0.0;
	
	shr->diff[1]+= shi->g*shi->amb*shi->rad[1];
	shr->diff[1]+= shi->ambg;
	if(shr->diff[1]<0.0) shr->diff[1]= 0.0;
	
	shr->diff[2]+= shi->b*shi->amb*shi->rad[2];
	shr->diff[2]+= shi->ambb;
	if(shr->diff[2]<0.0) shr->diff[2]= 0.0;
	
	if(ma->mode & MA_RAMP_COL) ramp_diffuse_result(shr->diff, shi);
	if(ma->mode & MA_RAMP_SPEC) ramp_spec_result(shr->spec, shr->spec+1, shr->spec+2, shi);
	
	/* refcol is for envmap only */
	if(shi->refcol[0]!=0.0) {
		shr->diff[0]= shi->mirr*shi->refcol[1] + (1.0 - shi->mirr*shi->refcol[0])*shr->diff[0];
		shr->diff[1]= shi->mirg*shi->refcol[2] + (1.0 - shi->mirg*shi->refcol[0])*shr->diff[1];
		shr->diff[2]= shi->mirb*shi->refcol[3] + (1.0 - shi->mirb*shi->refcol[0])*shr->diff[2];
	}

}

/* this function sets all coords for render (shared with raytracer) */
/* warning; exception for ortho render is here, can be done better! */
void shade_input_set_coords(ShadeInput *shi, float u, float v, int i1, int i2, int i3)
{
	VertRen *v1, *v2, *v3;
	VlakRen *vlr= shi->vlr;
	float l, dl;
	short texco= shi->mat->texco;
	int mode= shi->mat->mode;
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
		/* exception case for wire render of edge */
		if(vlr->v2==vlr->v3);
		else if( (vlr->flag & R_SMOOTH) || (texco & NEED_UV) ) {
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
			
			detsh= 1.0/(t00*t11-t10*t01);
			t00*= detsh; t01*=detsh; 
			t10*=detsh; t11*=detsh;
		
			if(vlr->snproj==0) {
				u= (shi->co[0]-v3->co[0])*t11-(shi->co[1]-v3->co[1])*t10;
				v= (shi->co[1]-v3->co[1])*t00-(shi->co[0]-v3->co[0])*t01;
				if(shi->osatex) {
					shi->dxuv[0]=  shi->dxco[0]*t11- shi->dxco[1]*t10;
					shi->dxuv[1]=  shi->dxco[1]*t00- shi->dxco[0]*t01;
					shi->dyuv[0]=  shi->dyco[0]*t11- shi->dyco[1]*t10;
					shi->dyuv[1]=  shi->dyco[1]*t00- shi->dyco[0]*t01;
				}
			}
			else if(vlr->snproj==1) {
				u= (shi->co[0]-v3->co[0])*t11-(shi->co[2]-v3->co[2])*t10;
				v= (shi->co[2]-v3->co[2])*t00-(shi->co[0]-v3->co[0])*t01;
				if(shi->osatex) {
					shi->dxuv[0]=  shi->dxco[0]*t11- shi->dxco[2]*t10;
					shi->dxuv[1]=  shi->dxco[2]*t00- shi->dxco[0]*t01;
					shi->dyuv[0]=  shi->dyco[0]*t11- shi->dyco[2]*t10;
					shi->dyuv[1]=  shi->dyco[2]*t00- shi->dyco[0]*t01;
				}
			}
			else {
				u= (shi->co[1]-v3->co[1])*t11-(shi->co[2]-v3->co[2])*t10;
				v= (shi->co[2]-v3->co[2])*t00-(shi->co[1]-v3->co[1])*t01;
				if(shi->osatex) {
					shi->dxuv[0]=  shi->dxco[1]*t11- shi->dxco[2]*t10;
					shi->dxuv[1]=  shi->dxco[2]*t00- shi->dxco[1]*t01;
					shi->dyuv[0]=  shi->dyco[1]*t11- shi->dyco[2]*t10;
					shi->dyuv[1]=  shi->dyco[2]*t00- shi->dyco[1]*t01;
				}
			}
		}
	
	}
	l= 1.0+u+v;
	
	/* calculate punos (vertexnormals) */
	if(vlr->flag & R_SMOOTH) { 
		float n1[3], n2[3], n3[3];
		
		if(shi->puno & p1) {
			n1[0]= -v1->n[0]; n1[1]= -v1->n[1]; n1[2]= -v1->n[2];
		} else {
			n1[0]= v1->n[0]; n1[1]= v1->n[1]; n1[2]= v1->n[2];
		}
		if(shi->puno & p2) {
			n2[0]= -v2->n[0]; n2[1]= -v2->n[1]; n2[2]= -v2->n[2];
		} else {
			n2[0]= v2->n[0]; n2[1]= v2->n[1]; n2[2]= v2->n[2];
		}
		
		if(shi->puno & p3) {
			n3[0]= -v3->n[0]; n3[1]= -v3->n[1]; n3[2]= -v3->n[2];
		} else {
			n3[0]= v3->n[0]; n3[1]= v3->n[1]; n3[2]= v3->n[2];
		}

		shi->vn[0]= l*n3[0]-u*n1[0]-v*n2[0];
		shi->vn[1]= l*n3[1]-u*n1[1]-v*n2[1];
		shi->vn[2]= l*n3[2]-u*n1[2]-v*n2[2];

		Normalise(shi->vn);

		if(shi->osatex && (texco & (TEXCO_NORM|TEXCO_REFL)) ) {
			dl= shi->dxuv[0]+shi->dxuv[1];
			shi->dxno[0]= dl*n3[0]-shi->dxuv[0]*n1[0]-shi->dxuv[1]*n2[0];
			shi->dxno[1]= dl*n3[1]-shi->dxuv[0]*n1[1]-shi->dxuv[1]*n2[1];
			shi->dxno[2]= dl*n3[2]-shi->dxuv[0]*n1[2]-shi->dxuv[1]*n2[2];
			dl= shi->dyuv[0]+shi->dyuv[1];
			shi->dyno[0]= dl*n3[0]-shi->dyuv[0]*n1[0]-shi->dyuv[1]*n2[0];
			shi->dyno[1]= dl*n3[1]-shi->dyuv[0]*n1[1]-shi->dyuv[1]*n2[1];
			shi->dyno[2]= dl*n3[2]-shi->dyuv[0]*n1[2]-shi->dyuv[1]*n2[2];

		}
	}
	else {
		VECCOPY(shi->vn, shi->facenor);
	}

	/* texture coordinates. shi->dxuv shi->dyuv have been set */
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
					dl= shi->dxuv[0]+shi->dxuv[1];
					shi->dxlo[0]= dl*o3[0]-shi->dxuv[0]*o1[0]-shi->dxuv[1]*o2[0];
					shi->dxlo[1]= dl*o3[1]-shi->dxuv[0]*o1[1]-shi->dxuv[1]*o2[1];
					shi->dxlo[2]= dl*o3[2]-shi->dxuv[0]*o1[2]-shi->dxuv[1]*o2[2];
					dl= shi->dyuv[0]+shi->dyuv[1];
					shi->dylo[0]= dl*o3[0]-shi->dyuv[0]*o1[0]-shi->dyuv[1]*o2[0];
					shi->dylo[1]= dl*o3[1]-shi->dyuv[0]*o1[1]-shi->dyuv[1]*o2[1];
					shi->dylo[2]= dl*o3[2]-shi->dyuv[0]*o1[2]-shi->dyuv[1]*o2[2];
				}
			}
		}
		
		if(texco & TEXCO_GLOB) {
			VECCOPY(shi->gl, shi->co);
			MTC_Mat4MulVecfl(R.viewinv, shi->gl);
			if(shi->osatex) {
				VECCOPY(shi->dxgl, shi->dxco);
				MTC_Mat3MulVecfl(R.imat, shi->dxco);
				VECCOPY(shi->dygl, shi->dyco);
				MTC_Mat3MulVecfl(R.imat, shi->dyco);
			}
		}
		if((texco & TEXCO_UV) || (mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE)))  {
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
			
			if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP)) {
				
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
					
					dl= shi->dxuv[0]+shi->dxuv[1];
					duv[0]= shi->dxuv[0]; 
					duv[1]= shi->dxuv[1];
					
					shi->dxuv[0]= 2.0*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
					shi->dxuv[1]= 2.0*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);
	
					dl= shi->dyuv[0]+shi->dyuv[1];
					duv[0]= shi->dyuv[0]; 
					duv[1]= shi->dyuv[1];
	
					shi->dyuv[0]= 2.0*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
					shi->dyuv[1]= 2.0*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);
				}
				
				if(mode & MA_FACETEXTURE) {
					if((mode & (MA_VERTEXCOL|MA_VERTEXCOLP))==0) {
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
				if(mode & MA_FACETEXTURE) {
					/* no tface? set at 1.0 */
					shi->vcol[0]= 1.0;
					shi->vcol[1]= 1.0;
					shi->vcol[2]= 1.0;
				}
			}
		}
		if(texco & TEXCO_NORM) {
			shi->orn[0]= -shi->vn[0];
			shi->orn[1]= -shi->vn[1];
			shi->orn[2]= -shi->vn[2];
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

#if 0
/* return labda for view vector being closest to line v3-v4 */
/* was used for wire render */
static float isec_view_line(float *view, float *v3, float *v4)
{
	float vec[3];
	float dot0, dot1, dot2, veclen, viewlen;
	float fac, div;
	
	vec[0]= v4[0] - v3[0];
	vec[1]= v4[1] - v3[1];
	vec[2]= v4[2] - v3[2];
	
	dot0 = v3[0]*vec[0] + v3[1]*vec[1] + v3[2]*vec[2];
	dot1 = vec[0]*view[0] + vec[1]*view[1] + vec[2]*view[2];
	dot2 = v3[0]*view[0] + v3[1]*view[1] + v3[2]*view[2];
	
	veclen = vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2];
	viewlen = view[0]*view[0] + view[1]*view[1] + view[2]*view[2];
	
	div = viewlen*veclen - dot1*dot1;
	if (div==0.0) return 0.0;
	
	fac = dot2*veclen - dot0*dot1;
	return fac/div;
}
#endif

  
/* x,y: window coordinate from 0 to rectx,y */
/* return pointer to rendered face */
void *shadepixel(float x, float y, int z, int facenr, int mask, float *col)
{
	ShadeResult shr;
	ShadeInput shi;
	VlakRen *vlr=NULL;
	
	if(facenr< 0) {	/* error */
		return NULL;
	}
	/* currently in use for dithering (soft shadow) and detecting thread */
	shi.xs= x;
	shi.ys= y;
	
	/* mask is used to indicate amount of samples (ray shad/mir and AO) */
	shi.mask= mask;
	shi.depth= 0;	// means first hit, not raytracing
	
	if(facenr==0) {	/* sky */
		col[0]= 0.0; col[1]= 0.0; col[2]= 0.0; col[3]= 0.0;
	}
	else if( (facenr & 0x7FFFFF) <= R.totvlak) {
		VertRen *v1, *v2, *v3;
		float alpha, fac, zcor;
		
		vlr= RE_findOrAddVlak( (facenr-1) & 0x7FFFFF);
		
		shi.vlr= vlr;
		shi.mat= vlr->mat;
		
		// copy all relevant material vars, note, keep this synced with render_types.h
		memcpy(&shi.r, &shi.mat->r, 23*sizeof(float));
		// set special cases:
		shi.har= shi.mat->har;
		if((shi.mat->mode & MA_RAYMIRROR)==0) shi.ray_mirror= 0.0;
		shi.osatex= (shi.mat->texco & TEXCO_OSA);
		
		/* copy the face normal (needed because it gets flipped for tracing */
		VECCOPY(shi.facenor, vlr->n);
		shi.puno= vlr->puno;
		
		v1= vlr->v1;
		
		/* COXYZ AND VIEW VECTOR  */
		calc_view_vector(shi.view, x, y);

		/* wire cannot use normal for calculating shi.co */
		if(shi.mat->mode & MA_WIRE) {
			float zco;
			/* inverse of zbuf calc: zbuf = MAXZ*hoco_z/hoco_w */
			
			zco= ((float)z)/(float)0x7FFFFFFF;
			shi.co[2]= R.winmat[3][2]/( R.winmat[2][3]*zco - R.winmat[2][2] );
			
			fac= zcor= shi.co[2]/shi.view[2];
			
			shi.co[0]= fac*shi.view[0];
			shi.co[1]= fac*shi.view[1];
		}
		else {
			float dface;
			
			dface= v1->co[0]*shi.facenor[0]+v1->co[1]*shi.facenor[1]+v1->co[2]*shi.facenor[2];
			
			/* ortho viewplane cannot intersect using view vector originating in (0,0,0) */
			if(R.r.mode & R_ORTHO) {
				/* x and y 3d coordinate can be derived from pixel coord and winmat */
				float fx= 2.0/(R.rectx*R.winmat[0][0]);
				float fy= 2.0/(R.recty*R.winmat[1][1]);
				
				shi.co[0]= (0.5 + x - 0.5*R.rectx)*fx - R.winmat[3][0]/R.winmat[0][0];
				shi.co[1]= (0.5 + y - 0.5*R.recty)*fy - R.winmat[3][1]/R.winmat[1][1];
				
				/* using a*x + b*y + c*z = d equation, (a b c) is normal */
				shi.co[2]= (dface - shi.facenor[0]*shi.co[0] - shi.facenor[1]*shi.co[1])/shi.facenor[2];
				
				zcor= 1.0; // only to prevent not-initialize
				
				if(shi.osatex || (R.r.mode & R_SHADOW) ) {
					shi.dxco[0]= fx;
					shi.dxco[1]= 0.0;
					shi.dxco[2]= (shi.facenor[0]*fx)/shi.facenor[2];
					
					shi.dyco[0]= 0.0;
					shi.dyco[1]= fy;
					shi.dyco[2]= (shi.facenor[1]*fy)/shi.facenor[2];
				}
			}
			else {
				float div;
				
				div= shi.facenor[0]*shi.view[0] + shi.facenor[1]*shi.view[1] + shi.facenor[2]*shi.view[2];
				if (div!=0.0) fac= zcor= dface/div;
				else fac= zcor= 0.0;
				
				shi.co[0]= fac*shi.view[0];
				shi.co[1]= fac*shi.view[1];
				shi.co[2]= fac*shi.view[2];
			
				/* pixel dx/dy for render coord */
				if(shi.osatex || (R.r.mode & R_SHADOW) ) {
					float u= dface/(div-shi.facenor[0]);
					float v= dface/(div- R.ycor*shi.facenor[1]);

					shi.dxco[0]= shi.co[0]- (shi.view[0]-1.0)*u;
					shi.dxco[1]= shi.co[1]- (shi.view[1])*u;
					shi.dxco[2]= shi.co[2]- (shi.view[2])*u;

					shi.dyco[0]= shi.co[0]- (shi.view[0])*v;
					shi.dyco[1]= shi.co[1]- (shi.view[1]-1.0*R.ycor)*v;
					shi.dyco[2]= shi.co[2]- (shi.view[2])*v;

				}
			}
		}
		
		/* cannot normalise earlier, code above needs it at pixel level */
		fac= Normalise(shi.view);
		zcor*= fac;	// for mist, distance of point from camera
		
		if(shi.osatex) {
			if( (shi.mat->texco & TEXCO_REFL) ) {
				shi.dxview= -1.0/fac;
				shi.dyview= -R.ycor/fac;
			}
		}
		
		/* calcuate normals, texture coords, vertex colors, etc */
		if(facenr & 0x800000)
			shade_input_set_coords(&shi, 1.0, 1.0, 0, 2, 3);
		else 
			shade_input_set_coords(&shi, 1.0, 1.0, 0, 1, 2);

		/* this only avalailable for scanline */
		if(shi.mat->texco & TEXCO_WINDOW) {
			shi.winco[0]= (x+(R.xstart))/(float)R.afmx;
			shi.winco[1]= (y+(R.ystart))/(float)R.afmy;
			shi.winco[2]= 0.0;
			if(shi.osatex) {
				shi.dxwin[0]= 0.5/(float)R.r.xsch;
				shi.dywin[1]= 0.5/(float)R.r.ysch;
				shi.dxwin[1]= shi.dxwin[2]= 0.0;
				shi.dywin[0]= shi.dywin[2]= 0.0;
			}
		}
		/* after this the u and v AND shi.dxuv and shi.dyuv are incorrect */
		if(shi.mat->texco & TEXCO_STICKY) {
			if(v1->sticky) {
				extern float Zmulx, Zmuly;
				float *o1, *o2, *o3, hox, hoy, l, dl, u, v;
				float s00, s01, s10, s11, detsh;
				
				if(facenr & 0x800000) {
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
				shi.sticky[2]= 0.0;
				
				if(shi.osatex) {
					shi.dxuv[0]=  s11/Zmulx;
					shi.dxuv[1]=  - s01/Zmulx;
					shi.dyuv[0]=  - s10/Zmuly;
					shi.dyuv[1]=  s00/Zmuly;
					
					dl= shi.dxuv[0]+shi.dxuv[1];
					shi.dxsticky[0]= dl*o3[0]-shi.dxuv[0]*o1[0]-shi.dxuv[1]*o2[0];
					shi.dxsticky[1]= dl*o3[1]-shi.dxuv[0]*o1[1]-shi.dxuv[1]*o2[1];
					dl= shi.dyuv[0]+shi.dyuv[1];
					shi.dysticky[0]= dl*o3[0]-shi.dyuv[0]*o1[0]-shi.dyuv[1]*o2[0];
					shi.dysticky[1]= dl*o3[1]-shi.dyuv[0]*o1[1]-shi.dyuv[1]*o2[1];
				}
			}
		}
		
		/* ------  main shading loop */
		shade_lamp_loop(&shi, &shr);

		if(shi.translucency!=0.0) {
			ShadeResult shr_t;
			
			VecMulf(shi.vn, -1.0);
			VecMulf(shi.facenor, -1.0);
			shade_lamp_loop(&shi, &shr_t);
			shr.diff[0]+= shi.translucency*shr_t.diff[0];
			shr.diff[1]+= shi.translucency*shr_t.diff[1];
			shr.diff[2]+= shi.translucency*shr_t.diff[2];
			VecMulf(shi.vn, -1.0);
			VecMulf(shi.facenor, -1.0);
		}
		
		if(R.r.mode & R_RAYTRACE) {
			if(shi.ray_mirror!=0.0 || ((shi.mat->mode & MA_RAYTRANSP) && shr.alpha!=1.0)) {
				ray_trace(&shi, &shr);
			}
		}
		else {
			// doesnt look 'correct', but is better for preview, plus envmaps dont raytrace this
			if(shi.mat->mode & MA_RAYTRANSP) shr.alpha= 1.0;
		}
		
		VECADD(col, shr.diff, shr.spec);
		
		/* exposure correction */
		if(R.wrld.exp!=0.0 || R.wrld.range!=1.0) {
			if((shi.mat->mode & MA_SHLESS)==0) {
				col[0]= R.wrld.linfac*(1.0-exp( col[0]*R.wrld.logfac) );
				col[1]= R.wrld.linfac*(1.0-exp( col[1]*R.wrld.logfac) );
				col[2]= R.wrld.linfac*(1.0-exp( col[2]*R.wrld.logfac) );
			}
		}
		
		/* MIST */
		if( (R.wrld.mode & WO_MIST) && (shi.mat->mode & MA_NOMIST)==0 ) {
			if(R.r.mode & R_ORTHO)
				alpha= mistfactor(-shi.co[2], shi.co);
			else
				alpha= mistfactor(zcor, shi.co);
		}
		else alpha= 1.0;

		if(shr.alpha!=1.0 || alpha!=1.0) {
			if(shi.mat->mode & MA_RAYTRANSP) {
				// sky was applied allready for ray transp, only do mist
				col[3]= shr.alpha;
				fac= alpha;	
			}
			else {
				fac= alpha*(shr.alpha);
				col[3]= fac;
			}			
			col[0]*= fac;
			col[1]*= fac;
			col[2]*= fac;
		}
		else col[3]= 1.0;
	}
	
	if(R.flag & R_LAMPHALO) {
		if(facenr<=0) {	/* calc view vector and put shi.co at far */
		
			calc_view_vector(shi.view, x, y);
			shi.co[2]= 0.0;
			
		}
		renderspothalo(&shi, col);
	}
	
	return vlr;
}

static void shadepixel_sky(float x, float y, int z, int facenr, int mask, float *colf)
{
	VlakRen *vlr;
	float collector[4];
	
	vlr= shadepixel(x, y, z, facenr, mask, colf);
	if(colf[3] != 1.0) {
		/* bail out when raytrace transparency (sky included already) */
		if(vlr && (R.r.mode & R_RAYTRACE))
			if(vlr->mat->mode & MA_RAYTRANSP) return;

		renderSkyPixelFloat(collector, x, y);
		addAlphaOverFloat(collector, colf);
		QUATCOPY(colf, collector);
	}
}

/* ************* pixel struct ******** */

static PixStrMain psmfirst;
static int psmteller;

static PixStr *addpsmain(void)
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

static void freeps(void)
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

static void addps(long *rd, int facenr, int z, unsigned short mask)
{
	static PixStr *cur;
	PixStr *ps, *last = NULL;

	if(*rd) {	
		ps= (PixStr *)(*rd);
		
		while(ps) {
			if( ps->facenr == facenr ) {
				ps->mask |= mask;
				return;
			}
			last= ps;
			ps= ps->next;
		}
	}

	/* make new PS (pixel struct) */
	if((psmteller & 4095)==0) cur= addpsmain();
	else cur++;
	psmteller++;

	if(last) last->next= cur;
	else *rd= (long)cur;

	cur->next= NULL;
	cur->facenr= facenr;
	cur->z= z;
	cur->mask = mask;
}


int count_mask(unsigned short mask)
{
	extern char cmask[256];
	return (cmask[mask & 255]+cmask[mask>>8]);
}

static void edge_enhance(void)
{
	/* use zbuffer to define edges, add it to the image */
	int val, y, x, col, *rz, *rz1, *rz2, *rz3;
	int zval1, zval2, zval3;
	char *cp;
	
	/* shift values in zbuffer 4 to the right, for filter we need multiplying with 12 max */
	rz= (int *)R.rectz;
	if(rz==NULL) return;
	
	for(y=0; y<R.recty; y++) {
		for(x=0; x<R.rectx; x++, rz++) (*rz)>>= 4;
	}
	
	rz1= (int *)R.rectz;
	rz2= rz1+R.rectx;
	rz3= rz2+R.rectx;

	if(R.r.mode & R_OSA) {
		cp= (char *)(R.rectaccu+R.rectx);
	}
	else {
		cp= (char *)(R.rectot+R.rectx);
	}
	cp+= 4;
	
	for(y=0; y<R.recty-2; y++) {

		for(x=0; x<R.rectx-2; x++, rz++, rz1++, rz2++, rz3++, cp+=4) {
			
			/* prevent overflow with sky z values */
			zval1=   rz1[0] + 2*rz1[1] +   rz1[2];
			zval2=  2*rz2[0]           + 2*rz2[2];
			zval3=   rz3[0] + 2*rz3[1] +   rz3[2];
			
			col= abs ( 4*rz2[1] - (zval1 + zval2 + zval3)/3 );
			
			col >>= 5;
			if(col > (1<<16)) col= (1<<16);
			else col= (R.r.edgeint*col)>>8;
			
			if(col>0) {
				if(col>255) col= 255;
				
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
struct renderlineDA {
	long *rd;
	int *rz;
	float *rb1, *rb2, *rb3;
	float *acol;
	int y;
};

static int do_renderlineDA(void *poin)
{
	struct renderlineDA *rl= poin;
	PixStr *ps;
	float xs, ys;
	float fcol[4], *acol=NULL, *rb1, *rb2, *rb3;
	long *rd= rl->rd;
	int zbuf, samp, curmask, face, mask, fullmask;
	int b, x, full_osa;
		
	fullmask= (1<<R.osa)-1;
	rb1= rl->rb1;
	rb2= rl->rb2;
	rb3= rl->rb3;
	
	if(R.flag & R_ZTRA) {		/* zbuf tra */
		abufsetrow(rl->acol, rl->y); 
		acol= rl->acol;
	}

	for(x=0; x<R.rectx; x++, rd++) {
				
		ps= (PixStr *)(*rd);
		mask= 0;
		
		/* complex loop, because empty spots are sky, without mask */
		while(TRUE) {
			
			if(ps==NULL) {
				face= 0;
				curmask= (~mask) & fullmask;
				zbuf= *(rl->rz+x);
			}
			else {
				face= ps->facenr;
				curmask= ps->mask;
				zbuf= ps->z;
			}
			
			/* check osa level */
			if(face==0) full_osa= 0;
			else {
				VlakRen *vlr= RE_findOrAddVlak( (face-1) & 0x7FFFFF);
				full_osa= (vlr->flag & R_FULL_OSA);
			}
			
			if(full_osa) {
				for(samp=0; samp<R.osa; samp++) {
					if(curmask & (1<<samp)) {
						xs= (float)x + jit[samp][0];
						ys= (float)rl->y + jit[samp][1];
						shadepixel_sky(xs, ys, zbuf, face, (1<<samp), fcol);
						
						if(acol && acol[3]!=0.0) addAlphaOverFloat(fcol, acol);
						if(do_gamma) {
							fcol[0]= gammaCorrect(fcol[0]);
							fcol[1]= gammaCorrect(fcol[1]);
							fcol[2]= gammaCorrect(fcol[2]);
						}
						add_filt_fmask(1<<samp, fcol, rb1, rb2, rb3);
					}
				}
			}
			else {
				extern char *centmask;	// initrender.c
				extern float centLut[16];
				
				b= centmask[curmask];
				xs= (float)x+centLut[b & 15];
				ys= (float)rl->y+centLut[b>>4];
				shadepixel_sky(xs, ys, zbuf, face, curmask, fcol);
				
				if(acol && acol[3]!=0.0) addAlphaOverFloat(fcol, acol);
				
				if(do_gamma) {
					fcol[0]= gammaCorrect(fcol[0]);
					fcol[1]= gammaCorrect(fcol[1]);
					fcol[2]= gammaCorrect(fcol[2]);
				}
				add_filt_fmask(curmask, fcol, rb1, rb2, rb3);
			}
			
			mask |= curmask;
			
			if(ps==NULL) break;
			else ps= ps->next;
		}
		
		rb1+=4; 
		rb2+=4; 
		rb3+=4;
		if(acol) acol+=4;
	}

	return 1;
}

void zbufshadeDA(void)	/* Delta Accum Pixel Struct */
{
	extern float Zjitx,Zjity;
	struct renderlineDA rl1, rl2;
	float xd, yd, *rf;
	long *rd;
	int *rz, *rp, *rt;
	float  *rowbuf1, *rowbuf2, *rowbuf3, *rowbuf0, *rowbuf1a, *rowbuf2a, *rb3;
	int a;
	short v, x, y;

	R.rectdaps= MEM_callocN(sizeof(long)*R.rectx*R.recty+4,"zbufDArectd");
	
	if(R.flag & R_ZTRA) {
		bgnaccumbuf();
		rl1.acol= MEM_callocN((R.rectx+4)*4*sizeof(float), "Acol");
		rl2.acol= MEM_callocN((R.rectx+4)*4*sizeof(float), "Acol");
	}
	
	psmteller= 0;

	if(R.r.mode & R_EDGE) {
		R.rectaccu= (int *)MEM_callocN(sizeof(int)*R.rectx*R.recty,"zbufshadeDA");
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

		rd= R.rectdaps;
		rp= R.rectot;
		rz= R.rectz;
		for(y=0; y<R.recty; y++) {
			for(x=0; x<R.rectx; x++, rp++, rd++) {
				if(*rp) {
					addps(rd, *rp, *(rz+x), 1<<v);
				}
			}
			rz+= R.rectx;
		}

		if(R.r.mode & R_EDGE) edge_enhance();
		
		if(RE_local_test_break()) break; 
	}
	
	rd= R.rectdaps;
	rz= R.rectz;
	rt= R.rectot;
	rf= R.rectftot;

	/* the rowbuf is 4 pixels larger than an image! */
	rowbuf0= MEM_callocN((R.rectx+4)*4*sizeof(float), "ZbufshadeDA3");
	rowbuf1= MEM_callocN((R.rectx+4)*4*sizeof(float), "ZbufshadeDA3");
	rowbuf2= MEM_callocN((R.rectx+4)*4*sizeof(float), "ZbufshadeDA3");
	rowbuf1a= MEM_callocN((R.rectx+4)*4*sizeof(float), "ZbufshadeDA3");
	rowbuf2a= MEM_callocN((R.rectx+4)*4*sizeof(float), "ZbufshadeDA3");
	rowbuf3= MEM_callocN((R.rectx+4)*4*sizeof(float), "ZbufshadeDA3");

	for(y=0; y<=R.recty; y++, rd+=R.rectx, rt+=R.rectx, rz+= R.rectx) {

		if(y<R.recty) {
			rl1.rd= rd;
			rl1.rz= rz;
			rl1.y= y;
			rl1.rb1= rowbuf1;
			rl1.rb2= rowbuf2;
			rl1.rb3= rowbuf3;
			
			if( (R.r.mode & R_THREADS) && y!=R.recty-1) {	// odd amount of total y pixels...
				if((y & 1)==0) {
					SDL_Thread *thread;

					thread = SDL_CreateThread(do_renderlineDA, &rl1);
					if ( thread == NULL ) {
						fprintf(stderr, "Unable to create thread");
						G.afbreek= 1;
						break;
					}
					
					rl2.rd= rd+R.rectx;
					rl2.rz= rz+R.rectx;
					rl2.y= y+1;
					rl2.rb1= rowbuf0;
					rl2.rb2= rowbuf1a;
					rl2.rb3= rowbuf2a;
					
					do_renderlineDA(&rl2);
					SDL_WaitThread(thread, NULL);
					
					if(R.r.mode & R_GAUSS) {
						float *rb1= rowbuf1, *rb2= rowbuf2, *rb1a= rowbuf1a, *rb2a= rowbuf2a;
						a= 4*(R.rectx + 4);
						while(a--) {
							*rb1 += *rb1a;
							*rb2 += *rb2a;
							*(rb1a++)= 0; rb1++;
							*(rb2a++)= 0; rb2++;
						}
					}
					else {
						SWAP(float *, rowbuf1a, rowbuf1);
					}
				}
			}
			else do_renderlineDA(&rl1);
			
		}
		if(y>0) {
			/* halos are alpha-added, not in thread loop (yet) because of gauss mask */
			if(R.flag & R_HALO) {
				/* one scanline older... */
				scanlinehaloPS(rz-R.rectx, rd-R.rectx, rowbuf3+4, y-1);
			}
			
			/* convert 4x32 bits buffer to 4x8, this can't be threaded due to gauss */
			transferColourBufferToOutput(rowbuf3+4, y-1);
			if(R.rectftot) {
				memcpy(rf, rowbuf3+4, 4*sizeof(float)*R.rectx);
				rf+= 4*R.rectx;
			}
			
		}
		if(y<R.recty) {
			memset(rowbuf3, 0, (R.rectx+4)*4*sizeof(int));
			rb3= rowbuf3;
			rowbuf3= rowbuf2;
			rowbuf2= rowbuf1;
			rowbuf1= rowbuf0;
			rowbuf0= rb3;

			if( y>0) {
				if((y & 1)==0) {
					RE_local_render_display(y-2, y-1,  R.rectx, R.recty, R.rectot);
				}
			}
		}
		if(RE_local_test_break()) break; 
	}

	if( (R.r.mode & R_EDGE) && RE_local_test_break()==0) {
		if(R.rectftot) {
			float *rtf= R.rectftot, colf[4];
			rp= R.rectaccu;
			for(a= R.rectx*R.recty; a>0; a--, rtf+=4, rp++) {
				cpCharColV2FloatColV((char *)rp, colf);
				addAlphaOverFloat(rtf, colf);
			}
			RE_floatbuffer_to_output();
		}
		else {
			rt= R.rectot;
			rp= R.rectaccu;
			for(a= R.rectx*R.recty; a>0; a--, rt++, rp++) {
				addalphaOver((char *)rt, (char *)rp);
			}
		}
	}
	
	MEM_freeN(R.rectdaps); 
	freeps();
	MEM_freeN(rowbuf0); 
	MEM_freeN(rowbuf1); 
	MEM_freeN(rowbuf2); 
	MEM_freeN(rowbuf1a); 
	MEM_freeN(rowbuf2a); 
	MEM_freeN(rowbuf3);
	R.rectdaps= NULL;

	if(R.r.mode & R_EDGE) if(R.rectaccu) MEM_freeN(R.rectaccu);
	R.rectaccu= NULL;
	if(R.flag & R_ZTRA) {
		endaccumbuf();
		MEM_freeN(rl1.acol);
		MEM_freeN(rl2.acol);
	}

} /* end of void zbufshadeDA() */

/* ------------------------------------------------------------------------ */

struct renderline {
	float *rowbuf, *acol;
	int *rp;
	int *rz;
	short ys;
	float y;
};

static int do_renderline(void *poin)
{
	struct renderline *rl= poin;
	float *fcol= rl->rowbuf;
	float *acol=NULL;
	int x, *rz, *rp;
	
	if(R.flag & R_ZTRA) {		/* zbuf tra */
		abufsetrow(rl->acol, rl->ys); 
		acol= rl->acol;
	}
	
	for(x=0, rz= rl->rz, rp= rl->rp; x<R.rectx; x++, rz++, rp++, fcol+=4) {
		shadepixel_sky((float)x, rl->y, *rz, *rp, 0, fcol);
		if(acol) {
			if(acol[3]!=0.0) addAlphaOverFloat(fcol, acol);
			acol+= 4;
		}
	}

	if(R.flag & R_HALO) {
		scanlinehalo(rl->rz, rl->rowbuf, rl->ys);
	}

	transferColourBufferToOutput(rl->rowbuf, rl->y);

	if(R.rectftot) {
		memcpy(R.rectftot + 4*rl->ys*R.rectx, rl->rowbuf, 4*sizeof(float)*R.rectx);
	}

	return 1;
}


void zbufshade(void)
{
	struct renderline rl1, rl2;
	extern float Zjitx,Zjity;
	int *rz, *rp;
	float fy;
	int y;

	rl1.rowbuf= MEM_callocN((R.rectx+4)*4*sizeof(float), "Zbufshade");
	rl2.rowbuf= MEM_callocN((R.rectx+4)*4*sizeof(float), "Zbufshade");
	
	Zjitx=Zjity= -0.5;

	zbufferall();

	/* SHADE */
	rp= R.rectot;
	rz= R.rectz;

	if(R.flag & R_ZTRA) {
		rl1.acol= MEM_callocN((R.rectx+4)*4*sizeof(float), "Acol");
		rl2.acol= MEM_callocN((R.rectx+4)*4*sizeof(float), "Acol");
		bgnaccumbuf();
	}

	for(y=0; y<R.recty; y++) {
		fy= y;
		
		rl1.rp= rp;
		rl1.rz= rz;
		rl1.y= fy;
		rl1.ys= y;
		
		if(R.r.mode & R_THREADS) {
			SDL_Thread *thread;
			
			thread = SDL_CreateThread(do_renderline, &rl1);
			if ( thread == NULL ) {
				fprintf(stderr, "Unable to create thread");
				G.afbreek= 1;
				break;
			}
			rp+= R.rectx;
			rz+= R.rectx;
			
			if(y < R.recty-1) {
				rl2.rp= rp;
				rl2.rz= rz;
				rl2.y= fy+1.0;
				rl2.ys= y+1;
				do_renderline(&rl2);
				rp+= R.rectx;
				rz+= R.rectx;
				y++;
			}			
			SDL_WaitThread(thread, NULL);
		}
		else {
			do_renderline(&rl1);
			rp+= R.rectx;
			rz+= R.rectx;
		}
		
		if(y & 1) {
			RE_local_render_display(y-1, y, R.rectx, R.recty, R.rectot);
		}
		
		if(RE_local_test_break()) break; 
	}
	
	MEM_freeN(rl1.rowbuf);
	MEM_freeN(rl2.rowbuf);
	
	if(R.flag & R_ZTRA) {
		endaccumbuf();
		MEM_freeN(rl1.acol);
		MEM_freeN(rl2.acol);
	}
	
	if(R.r.mode & R_EDGE) edge_enhance();

} /* end of void zbufshade() */

/* ------------------------------------------------------------------------ */

void RE_shadehalo(HaloRen *har, char *col, float *colf, int zz, float dist, float xn, float yn, short flarec)
{

	shadeHaloFloat(har, colf, zz, dist, xn, yn, flarec);
	
	if(colf[0]<=0.0) col[0]= 0; else if(colf[0]>=1.0) col[0]= 255; else col[0]= 255.0*colf[0];
	if(colf[1]<=0.0) col[1]= 0; else if(colf[1]>=1.0) col[1]= 255; else col[1]= 255.0*colf[1];
	if(colf[2]<=0.0) col[2]= 0; else if(colf[2]>=1.0) col[2]= 255; else col[2]= 255.0*colf[2];
	if(colf[3]<=0.0) col[3]= 0; else if(colf[3]>=1.0) col[3]= 255; else col[3]= 255.0*colf[3];
	
}

static void renderhalo(HaloRen *har)	/* postprocess version */
{
	
	float dist, xsq, ysq, xn, yn, colf[4], *rectft, *rtf;
	int *rectt, *rt;
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
			rectft= R.rectftot+ 4*R.rectx*miny;

			for(y=miny; y<maxy; y++) {
	
				rt= rectt+minx;
				rtf= rectft+4*minx;
				
				yn= (y - har->ys)*R.ycor;
				ysq= yn*yn;
				
				for(x=minx; x<=maxx; x++) {
					xn= x - har->xs;
					xsq= xn*xn;
					dist= xsq+ysq;
					if(dist<har->radsq) {
						
						shadeHaloFloat(har, colf, 0x7FFFFF, dist, xn, yn, har->flarec);
						if(R.rectftot) addalphaAddfacFloat(rtf, colf, har->add);
						else {
							std_floatcol_to_charcol(colf, col);
							RE_addalphaAddfac((char *)rt, col, har->add);
						}
					}
					rt++;
					rtf+=4;
				}
	
				rectt+= R.rectx;
				rectft+= 4*R.rectx;
				
				if(RE_local_test_break()) break; 
			}
		}
	}
} 
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
	har->zs= fla.zs= 0;
	
	har->alfa= alfa*visifac;

	renderhalo(har);
	
	/* next halo's: the flares */
	rc= hashvectf + ma->seed2;
	
	for(b=1; b<har->flarec; b++) {
		
		fla.r= fabs(rc[0]);
		fla.g= fabs(rc[1]);
		fla.b= fabs(rc[2]);
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

	if(R.rectftot) RE_floatbuffer_to_output();
}


/* end of render.c */


