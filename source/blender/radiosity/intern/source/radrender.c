/* ***************************************
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
static VlakRen *findshoot_rr(Render *re)
{
	RadFace *rf;
	ObjectRen *obr;
	VlakRen *vlr=NULL, *shoot;
	float energy;
	int a;
	
	shoot= NULL;
	maxenergy= 0.0;
	
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			if(vlr->radface) {
				rf= vlr->radface;
				rf->flag &= ~RAD_SHOOT;
				
				energy= rf->unshot[0]*rf->area;
				energy+= rf->unshot[1]*rf->area;
				energy+= rf->unshot[2]*rf->area;

				if(energy>maxenergy) {
					shoot= vlr;
					maxenergy= energy;
				}
			}
		}
	}

	if(shoot) {
		maxenergy/= RG.totenergy;
		if(maxenergy<RG.convergence) return NULL;
		shoot->radface->flag |= RAD_SHOOT;
	}

	return shoot;
}

static void backface_test_rr(Render *re, VlakRen *shoot)
{
	ObjectRen *obr;
	VlakRen *vlr=NULL;
	RadFace *rf;
	float tvec[3];
	int a;
	
	/* backface testing */
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			if(vlr->radface && vlr!=shoot) {
				rf= vlr->radface;
				VecSubf(tvec, shoot->radface->cent, rf->cent);
				
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
	RadFace *rf;
	int a;
	
	/* backface flag clear */
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			
			if(vlr->radface) {
				rf= vlr->radface;
				rf->flag &= ~RAD_BACKFACE;
			}
		}
	}
}

extern RadView hemitop, hemiside; // radfactors.c

/* hemi-zbuffering, delivers formfactors array */
static void makeformfactors_rr(Render *re, VlakRen *shoot)
{
	ObjectRen *obr;
	VlakRen *vlr=NULL;
	RadFace *rf;
	float len, vec[3], up[3], side[3], tar[5][3], *fp;
	int a;

	memset(RG.formfactors, 0, sizeof(float)*RG.totelem);

	/* set up hemiview */
	/* first: upvector for hemitop, we use diagonal hemicubes to prevent aliasing */
	
	VecSubf(vec, shoot->v1->co, shoot->radface->cent);
	Crossf(up, shoot->radface->norm, vec);
	len= Normalize(up);
	
	VECCOPY(hemitop.up, up);
	VECCOPY(hemiside.up, shoot->radface->norm);

	Crossf(side, shoot->radface->norm, up);

	/* five targets */
	VecAddf(tar[0], shoot->radface->cent, shoot->radface->norm);
	VecAddf(tar[1], shoot->radface->cent, up);
	VecSubf(tar[2], shoot->radface->cent, up);
	VecAddf(tar[3], shoot->radface->cent, side);
	VecSubf(tar[4], shoot->radface->cent, side);

	/* camera */
	VECCOPY(hemiside.cam, shoot->radface->cent);
	VECCOPY(hemitop.cam, shoot->radface->cent);

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
			
			if(vlr->radface) {
				rf= vlr->radface;
				if(*fp!=0.0 && rf->area!=0.0) {
					*fp *= shoot->radface->area/rf->area;
					if(*fp>1.0) *fp= 1.0001;
				}
				fp++;
			}
		}
	}
}

/* based at RG.formfactors array, distribute shoot energy over other faces */
static void applyformfactors_rr(Render *re, VlakRen *shoot)
{
	ObjectRen *obr;
	VlakRen *vlr=NULL;
	RadFace *rf;
	float *fp, *ref, unr, ung, unb, r, g, b;
	int a;

	unr= shoot->radface->unshot[0];
	ung= shoot->radface->unshot[1];
	unb= shoot->radface->unshot[2];

	fp= RG.formfactors;
	
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			
			if(vlr->radface) {
				rf= vlr->radface;
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
	shoot->radface->unshot[0]= shoot->radface->unshot[1]= shoot->radface->unshot[2]= 0.0;
}


/* main loop for itterations */
static void progressiverad_rr(Render *re)
{
	VlakRen *shoot;
	float unshot[3];
	int it= 0;
	
	shoot= findshoot_rr(re);
	while( shoot ) {
		
		/* backfaces receive no energy, but are zbuffered... */
		backface_test_rr(re, shoot);
		
		/* ...unless it's two sided */
		if(shoot->radface->flag & RAD_TWOSIDED) {
			VECCOPY(unshot, shoot->radface->unshot);
			VecMulf(shoot->radface->norm, -1.0);
			makeformfactors_rr(re, shoot);
			applyformfactors_rr(re, shoot);
			VecMulf(shoot->radface->norm, -1.0);
			VECCOPY(shoot->radface->unshot, unshot);
		}

		/* hemi-zbuffers */
		makeformfactors_rr(re, shoot);
		/* based at RG.formfactors array, distribute shoot energy over other faces */
		applyformfactors_rr(re, shoot);
		
		it++;
		re->timecursor(it);
		
		clear_backface_test_rr(re);
		
		if(re->test_break()) break;
		if(RG.maxiter && RG.maxiter<=it) break;
		
		shoot= findshoot_rr(re);
	}
	printf(" Unshot energy:%f\n", 1000.0*maxenergy);
	
	re->timecursor((G.scene->r.cfra));
}

static RadFace *radfaces=NULL;

static void initradfaces(Render *re)	
{
	ObjectRen *obr;
	VlakRen *vlr= NULL;
	RadFace *rf;
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
				if(vlr->v4) CalcNormFloat4(vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->v4->co, rf->norm);
				else CalcNormFloat(vlr->v1->co, vlr->v2->co, vlr->v3->co, rf->norm);
				
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
				
				vlr->radface= rf++;
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
	RadFace *rf;
	float *col;
	int a;

	RG.igamma= 1.0/RG.gamma;
	RG.radfactor= RG.radfac*pow(64*64, RG.igamma)/128.0; /* compatible with radio-tool */

	/* accumulate vertexcolors */
	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak; else vlr++;
			
			if(vlr->radface) {
				rf= vlr->radface;
				
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

