/* ***************************************
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
 */
 
/* radrender.c, aug 2003
 *
 * Most of the code here is copied from radiosity code, to optimize for renderfaces.
 * Shared function calls mostly reside in radfactors.c
 * No adaptive subdivision takes place
 *
 * - do_radio_render();  main call, extern
 *   - initradfaces(); add radface structs in render faces, init radio globals
 *   - 
 *   - initradiosity(); LUTs
 *   - inithemiwindows();
 *   - progressiverad(); main itteration loop
 *     - hemi zbuffers
 *     - calc rad factors
 * 
 *   - closehemiwindows();
 *   - freeAllRad();
 *   - make vertex colors
 *
 * - during render, materials use totrad as ambient replacement
 * - free radfaces
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "BIF_screen.h"

#include "radio.h"

/* the radiosity module uses internal includes from render! */
#include "renderpipeline.h" 
#include "render_types.h" 
#include "renderdatabase.h" 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* only needed now for a print, if its useful move to RG */
static float maxenergy;	

/* find the face with maximum energy to become shooter */
/* nb: _rr means rad-render version of existing radio call */
static void findshoot_rr(Render *re, VlakRen **shoot_p, RadFace **shootrf_p)
{
	RadFace *rf, *shootrf, **radface;
	ObjectRen *obr;
	VlakRen *vlr=NULL, *shoot;
	float energy;
	int a;
	
	shoot= NULL;
	shootrf= NULL;
	maxenergy= 0.0;
	
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			if((radface=RE_vlakren_get_radface(obr, vlr, 0)) && *radface) {
				rf= *radface;
				rf->flag &= ~RAD_SHOOT;
				
				energy= rf->unshot[0]*rf->area;
				energy+= rf->unshot[1]*rf->area;
				energy+= rf->unshot[2]*rf->area;

				if(energy>maxenergy) {
					shoot= vlr;
					shootrf= rf;
					maxenergy= energy;
				}
			}
		}
	}

	if(shootrf) {
		maxenergy/= RG.totenergy;
		if(maxenergy<RG.convergence) {
			*shoot_p= NULL;
			*shootrf_p= NULL;
			return;
		}
		shootrf->flag |= RAD_SHOOT;
	}

	*shoot_p= shoot;
	*shootrf_p= shootrf;
}

static void backface_test_rr(Render *re, VlakRen *shoot, RadFace *shootrf)
{
	ObjectRen *obr;
	VlakRen *vlr=NULL;
	RadFace *rf, **radface;
	float tvec[3];
	int a;
	
	/* backface testing */
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			if(vlr != shoot && (radface=RE_vlakren_get_radface(obr, vlr, 0)) && *radface) {
				rf= *radface;
				VecSubf(tvec, shootrf->cent, rf->cent);
				
				if(tvec[0]*rf->norm[0]+ tvec[1]*rf->norm[1]+ tvec[2]*rf->norm[2] < 0.0)
					rf->flag |= RAD_BACKFACE;
			}
		}
	}
}

static void clear_backface_test_rr(Render *re)
{
	ObjectRen *obr;
	VlakRen *vlr=NULL;
	RadFace *rf, **radface;
	int a;
	
	/* backface flag clear */
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			
			if((radface=RE_vlakren_get_radface(obr, vlr, 0)) && *radface) {
				rf= *radface;
				rf->flag &= ~RAD_BACKFACE;
			}
		}
	}
}

extern RadView hemitop, hemiside; // radfactors.c

/* hemi-zbuffering, delivers formfactors array */
static void makeformfactors_rr(Render *re, VlakRen *shoot, RadFace *shootrf)
{
	ObjectRen *obr;
	VlakRen *vlr=NULL;
	RadFace *rf, **radface;
	float len, vec[3], up[3], side[3], tar[5][3], *fp;
	int a;

	memset(RG.formfactors, 0, sizeof(float)*RG.totelem);

	/* set up hemiview */
	/* first: upvector for hemitop, we use diagonal hemicubes to prevent aliasing */
	
	VecSubf(vec, shoot->v1->co, shootrf->cent);
	Crossf(up, shootrf->norm, vec);
	len= Normalize(up);
	
	VECCOPY(hemitop.up, up);
	VECCOPY(hemiside.up, shootrf->norm);

	Crossf(side, shootrf->norm, up);

	/* five targets */
	VecAddf(tar[0], shootrf->cent, shootrf->norm);
	VecAddf(tar[1], shootrf->cent, up);
	VecSubf(tar[2], shootrf->cent, up);
	VecAddf(tar[3], shootrf->cent, side);
	VecSubf(tar[4], shootrf->cent, side);

	/* camera */
	VECCOPY(hemiside.cam, shootrf->cent);
	VECCOPY(hemitop.cam, shootrf->cent);

	/* do it! */
	VECCOPY(hemitop.tar, tar[0]);
	hemizbuf(&hemitop);

	for(a=1; a<5; a++) {
		VECCOPY(hemiside.tar, tar[a]);
		hemizbuf(&hemiside);
	}

	/* convert factors to real radiosity */
	fp= RG.formfactors;

	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			
			if((radface=RE_vlakren_get_radface(obr, vlr, 0)) && *radface) {
				rf= *radface;
				if(*fp!=0.0 && rf->area!=0.0) {
					*fp *= shootrf->area/rf->area;
					if(*fp>1.0) *fp= 1.0001;
				}
				fp++;
			}
		}
	}
}

/* based at RG.formfactors array, distribute shoot energy over other faces */
static void applyformfactors_rr(Render *re, VlakRen *shoot, RadFace *shootrf)
{
	ObjectRen *obr;
	VlakRen *vlr=NULL;
	RadFace *rf, **radface;
	float *fp, *ref, unr, ung, unb, r, g, b;
	int a;

	unr= shootrf->unshot[0];
	ung= shootrf->unshot[1];
	unb= shootrf->unshot[2];

	fp= RG.formfactors;
	
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			
			if((radface=RE_vlakren_get_radface(obr, vlr, 0)) && *radface) {
				rf= *radface;
				if(*fp!= 0.0) {
					
					ref= &(vlr->mat->r);
					
					r= (*fp)*unr*ref[0];
					g= (*fp)*ung*ref[1];
					b= (*fp)*unb*ref[2];
					
					// if(rf->flag & RAD_BACKFACE) {
					
					rf->totrad[0]+= r;
					rf->totrad[1]+= g;
					rf->totrad[2]+= b;
					
					rf->unshot[0]+= r;
					rf->unshot[1]+= g;
					rf->unshot[2]+= b;
				}
				fp++;
			}
		}
	}
	/* shoot energy has been shot */
	shootrf->unshot[0]= shootrf->unshot[1]= shootrf->unshot[2]= 0.0;
}


/* main loop for itterations */
static void progressiverad_rr(Render *re)
{
	VlakRen *shoot;
	RadFace *shootrf;
	float unshot[3];
	int it= 0;
	
	findshoot_rr(re, &shoot, &shootrf);
	while( shoot ) {
		
		/* backfaces receive no energy, but are zbuffered... */
		backface_test_rr(re, shoot, shootrf);
		
		/* ...unless it's two sided */
		if(shootrf->flag & RAD_TWOSIDED) {
			VECCOPY(unshot, shootrf->unshot);
			VecMulf(shootrf->norm, -1.0);
			makeformfactors_rr(re, shoot, shootrf);
			applyformfactors_rr(re, shoot, shootrf);
			VecMulf(shootrf->norm, -1.0);
			VECCOPY(shootrf->unshot, unshot);
		}

		/* hemi-zbuffers */
		makeformfactors_rr(re, shoot, shootrf);
		/* based at RG.formfactors array, distribute shoot energy over other faces */
		applyformfactors_rr(re, shoot, shootrf);
		
		it++;
		re->timecursor(it);
		
		clear_backface_test_rr(re);
		
		if(re->test_break()) break;
		if(RG.maxiter && RG.maxiter<=it) break;
		
		findshoot_rr(re, &shoot, &shootrf);
	}
	printf(" Unshot energy:%f\n", 1000.0*maxenergy);
	
	re->timecursor((G.scene->r.cfra));
}

static RadFace *radfaces=NULL;

static void initradfaces(Render *re)	
{
	ObjectRen *obr;
	VlakRen *vlr= NULL;
	RadFace *rf, **radface;
	int a, b;
	
	/* globals */
	RG.totenergy= 0.0;
	RG.totpatch= 0;		// we count initial emittors here
	RG.totelem= 0;		// total # faces are put here (so we can use radfactors.c calls)
	/* size is needed for hemicube clipping */
	RG.min[0]= RG.min[1]= RG.min[2]= 1.0e20;
	RG.max[0]= RG.max[1]= RG.max[2]= -1.0e20;
	
	/* count first for fast malloc */
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			
			if(vlr->mat->mode & MA_RADIO) {
				if(vlr->mat->emit > 0.0) {
					RG.totpatch++;
				}
				RG.totelem++;
			}
		}
	}
		
printf(" Rad elems: %d emittors %d\n", RG.totelem, RG.totpatch);	
	if(RG.totelem==0 || RG.totpatch==0) return;

	/* make/init radfaces */
	rf=radfaces= MEM_callocN(RG.totelem*sizeof(RadFace), "radfaces");
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			
			if(vlr->mat->mode & MA_RADIO) {
				
				/* during render, vlr->n gets flipped/corrected, we cannot have that */
				if (obr->ob->transflag & OB_NEG_SCALE){
					/* The object has negative scale that will cause the normals to flip.
						 To counter this unwanted normal flip, swap vertex 2 and 4 for a quad
						 or vertex 2 and 3 (see flip_face) for a triangle in the call to CalcNormFloat4 
						 in order to flip the normals back to the way they were in the original mesh. */
					if(vlr->v4) CalcNormFloat4(vlr->v1->co, vlr->v4->co, vlr->v3->co, vlr->v2->co, rf->norm);
					else CalcNormFloat(vlr->v1->co, vlr->v3->co, vlr->v2->co, rf->norm);
				}else{
					if(vlr->v4) CalcNormFloat4(vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->v4->co, rf->norm);
					else CalcNormFloat(vlr->v1->co, vlr->v2->co, vlr->v3->co, rf->norm);
				}

				rf->totrad[0]= vlr->mat->emit*vlr->mat->r;
				rf->totrad[1]= vlr->mat->emit*vlr->mat->g;
				rf->totrad[2]= vlr->mat->emit*vlr->mat->b;
				VECCOPY(rf->unshot, rf->totrad);
				
				if(vlr->v4) {
					rf->area= AreaQ3Dfl(vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->v4->co);
					CalcCent4f(rf->cent, vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->v4->co);
				}
				else {
					rf->area= AreaT3Dfl(vlr->v1->co, vlr->v2->co, vlr->v3->co);
					CalcCent3f(rf->cent, vlr->v1->co, vlr->v2->co, vlr->v3->co);
				}
				
				RG.totenergy+= rf->unshot[0]*rf->area;
				RG.totenergy+= rf->unshot[1]*rf->area;
				RG.totenergy+= rf->unshot[2]*rf->area;
				
				for(b=0; b<3; b++) {
					RG.min[b]= MIN2(RG.min[b], rf->cent[b]);
					RG.max[b]= MAX2(RG.max[b], rf->cent[b]);
				}

	// uncommented; this isnt satisfying, but i leave it in the code for now (ton)			
	//			if(vlr->mat->translucency!=0.0) rf->flag |= RAD_TWOSIDED;
				
				radface=RE_vlakren_get_radface(obr, vlr, 1);
				*radface= rf++;
			}
		}
	}
	RG.size[0]= (RG.max[0]- RG.min[0]);
	RG.size[1]= (RG.max[1]- RG.min[1]);
	RG.size[2]= (RG.max[2]- RG.min[2]);
	RG.maxsize= MAX3(RG.size[0],RG.size[1],RG.size[2]);

	/* formfactor array */
	if(RG.formfactors) MEM_freeN(RG.formfactors);
	if(RG.totelem)
		RG.formfactors= MEM_mallocN(sizeof(float)*RG.totelem, "formfactors");
	else
		RG.formfactors= NULL;
	
}

static void vecaddfac(float *vec, float *v1, float *v2, float fac)
{
	vec[0]= v1[0] + fac*v2[0];
	vec[1]= v1[1] + fac*v2[1];
	vec[2]= v1[2] + fac*v2[2];

}

/* unused now, doesnt work..., find it in cvs of nov 2005 or older */
/* static void filter_rad_values(void) */


static void make_vertex_rad_values(Render *re)
{
	ObjectRen *obr;
	VertRen *v1=NULL;
	VlakRen *vlr=NULL;
	RadFace *rf, **radface;
	float *col;
	int a;

	RG.igamma= 1.0/RG.gamma;
	RG.radfactor= RG.radfac*pow(64*64, RG.igamma)/128.0; /* compatible with radio-tool */

	/* accumulate vertexcolors */
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			
			if((radface=RE_vlakren_get_radface(obr, vlr, 0)) && *radface) {
				rf= *radface;
				
				/* apply correction */
				rf->totrad[0]= RG.radfactor*pow( rf->totrad[0], RG.igamma);
				rf->totrad[1]= RG.radfactor*pow( rf->totrad[1], RG.igamma);
				rf->totrad[2]= RG.radfactor*pow( rf->totrad[2], RG.igamma);
				
				/* correct rf->rad values for color */
				if(vlr->mat->r > 0.0) rf->totrad[0]/= vlr->mat->r;
				if(vlr->mat->g > 0.0) rf->totrad[1]/= vlr->mat->g;
				if(vlr->mat->b > 0.0) rf->totrad[2]/= vlr->mat->b;
				
				col= RE_vertren_get_rad(obr, vlr->v1, 1);
				vecaddfac(col, col, rf->totrad, rf->area); 
				col[3]+= rf->area;
				
				col= RE_vertren_get_rad(obr, vlr->v2, 1);
				vecaddfac(col, col, rf->totrad, rf->area); 
				col[3]+= rf->area;
				
				col= RE_vertren_get_rad(obr, vlr->v3, 1);
				vecaddfac(col, col, rf->totrad, rf->area); 
				col[3]+= rf->area;

				if(vlr->v4) {
					col= RE_vertren_get_rad(obr, vlr->v4, 1);
					vecaddfac(col, col, rf->totrad, rf->area); 
					col[3]+= rf->area;
				}
			}
		}
	
		/* make vertex colors */
		for(a=0; a<obr->totvert; a++) {
			if((a & 255)==0) v1= RE_findOrAddVert(obr, a); else v1++;
			
			col= RE_vertren_get_rad(obr, v1, 0);
			if(col && col[3]>0.0) {
				col[0]/= col[3];
				col[1]/= col[3];
				col[2]/= col[3];
			}
		}
	}
}

/* main call, extern */
void do_radio_render(Render *re)
{
	if(G.scene->radio==NULL) add_radio();
	freeAllRad();	/* just in case radio-tool is still used */
	
	set_radglobal(); /* init the RG struct */
	RG.re= re;		/* only used by hemizbuf(), prevents polluting radio code all over */
	
	initradfaces(re);	 /* add radface structs to render faces */
	if(RG.totenergy>0.0) {

		initradiosity();	/* LUT's */
		inithemiwindows();	/* views, need RG.maxsize for clipping */
	
		progressiverad_rr(re); /* main radio loop */
		
		make_vertex_rad_values(re); /* convert face energy to vertex ones */

	}
	
	freeAllRad();	/* luts, hemis, sets vars at zero */
}

/* free call, after rendering, extern */
void end_radio_render(void)
{
	if(radfaces) MEM_freeN(radfaces);
	radfaces= NULL;
}

