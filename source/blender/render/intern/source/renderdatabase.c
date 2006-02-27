/**
 * $Id$
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
 * Contributor(s): 2004-2006, Blender Foundation, full recode
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/*
 * Storage, retrieval and query of render specific data.
 *
 * All data from a Blender scene is converter by the renderconverter/
 * into a special format that is used by the render module to make
 * images out of. These functions interface to the render-specific
 * database.  
 *
 * The blo{ha/ve/vl} arrays store pointers to blocks of 256 data
 * entries each.
 *
 * The index of an entry is >>8 (the highest 24 * bits), to find an
 * offset in a 256-entry block.
 *
 * - If the 256-entry block entry has an entry in the
 * vertnodes/bloha/blovl array of the current block, the i-th entry in
 * that block is allocated to this entry.
 *
 * - If the entry has no block allocated for it yet, memory is
 * allocated.
 *
 * The pointer to the correct entry is returned. Memory is guarateed
 * to exist (as long as the malloc does not break). Since guarded
 * allocation is used, memory _must_ be available. Otherwise, an
 * exit(0) would occur.
 * 
 */

#include <limits.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "DNA_material_types.h" 
#include "DNA_mesh_types.h" 
#include "DNA_meshdata_types.h" 
#include "DNA_texture_types.h" 

#include "BKE_texture.h" 

#include "RE_render_ext.h"	/* externtex */

#include "renderpipeline.h"
#include "render_types.h"
#include "renderdatabase.h"
#include "texture.h"
#include "zbuf.h"

/* ------------------------------------------------------------------------- */

/* More dynamic allocation of options for render vertices, so we dont
   have to reserve this space inside vertices.
   Important; vertices should have been created already (to get tables checked) 
   that's a reason why the calls demand VertRen * as arg, not the index */

/* NOTE! the hardcoded table size 256 is used still in code for going quickly over vertices/faces */

#define RE_STICKY_ELEMS		2
#define RE_STRESS_ELEMS		1
#define RE_RAD_ELEMS		4
#define RE_STRAND_ELEMS		1
#define RE_TANGENT_ELEMS	3
#define RE_STRESS_ELEMS		1
#define RE_WINSPEED_ELEMS	4

float *RE_vertren_get_sticky(Render *re, VertRen *ver, int verify)
{
	float *sticky;
	int nr= ver->index>>8;
	
	sticky= re->vertnodes[nr].sticky;
	if(sticky==NULL) {
		if(verify) 
			sticky= re->vertnodes[nr].sticky= MEM_mallocN(256*RE_STICKY_ELEMS*sizeof(float), "sticky table");
		else
			return NULL;
	}
	return sticky + (ver->index & 255)*RE_STICKY_ELEMS;
}

float *RE_vertren_get_stress(Render *re, VertRen *ver, int verify)
{
	float *stress;
	int nr= ver->index>>8;
	
	stress= re->vertnodes[nr].stress;
	if(stress==NULL) {
		if(verify) 
			stress= re->vertnodes[nr].stress= MEM_mallocN(256*RE_STRESS_ELEMS*sizeof(float), "stress table");
		else
			return NULL;
	}
	return stress + (ver->index & 255)*RE_STRESS_ELEMS;
}

/* this one callocs! */
float *RE_vertren_get_rad(Render *re, VertRen *ver, int verify)
{
	float *rad;
	int nr= ver->index>>8;
	
	rad= re->vertnodes[nr].rad;
	if(rad==NULL) {
		if(verify) 
			rad= re->vertnodes[nr].rad= MEM_callocN(256*RE_RAD_ELEMS*sizeof(float), "rad table");
		else
			return NULL;
	}
	return rad + (ver->index & 255)*RE_RAD_ELEMS;
}

float *RE_vertren_get_strand(Render *re, VertRen *ver, int verify)
{
	float *strand;
	int nr= ver->index>>8;
	
	strand= re->vertnodes[nr].strand;
	if(strand==NULL) {
		if(verify) 
			strand= re->vertnodes[nr].strand= MEM_mallocN(256*RE_STRAND_ELEMS*sizeof(float), "strand table");
		else
			return NULL;
	}
	return strand + (ver->index & 255)*RE_STRAND_ELEMS;
}

/* needs calloc */
float *RE_vertren_get_tangent(Render *re, VertRen *ver, int verify)
{
	float *tangent;
	int nr= ver->index>>8;
	
	tangent= re->vertnodes[nr].tangent;
	if(tangent==NULL) {
		if(verify) 
			tangent= re->vertnodes[nr].tangent= MEM_callocN(256*RE_TANGENT_ELEMS*sizeof(float), "tangent table");
		else
			return NULL;
	}
	return tangent + (ver->index & 255)*RE_TANGENT_ELEMS;
}

/* needs calloc! not all renderverts have them */
float *RE_vertren_get_winspeed(Render *re, VertRen *ver, int verify)
{
	float *winspeed;
	int nr= ver->index>>8;
	
	winspeed= re->vertnodes[nr].winspeed;
	if(winspeed==NULL) {
		if(verify) 
			winspeed= re->vertnodes[nr].winspeed= MEM_callocN(256*RE_WINSPEED_ELEMS*sizeof(float), "winspeed table");
		else
			return NULL;
	}
	return winspeed + (ver->index & 255)*RE_WINSPEED_ELEMS;
}

VertRen *RE_findOrAddVert(Render *re, int nr)
{
	VertTableNode *temp;
	VertRen *v;
	int a;

	if(nr<0) {
		printf("error in findOrAddVert: %d\n",nr);
		return NULL;
	}
	a= nr>>8;
	
	if (a>=re->vertnodeslen-1) {  /* Need to allocate more columns..., and keep last element NULL for free loop */
		temp= re->vertnodes;
		
		re->vertnodes= MEM_mallocN(sizeof(VertTableNode)*(re->vertnodeslen+TABLEINITSIZE) , "vertnodes");
		if(temp) memcpy(re->vertnodes, temp, re->vertnodeslen*sizeof(VertTableNode));
		memset(re->vertnodes+re->vertnodeslen, 0, TABLEINITSIZE*sizeof(VertTableNode));
		
		re->vertnodeslen+=TABLEINITSIZE; 
		if(temp) MEM_freeN(temp);	
	}
	
	v= re->vertnodes[a].vert;
	if(v==NULL) {
		int i;
		
		v= (VertRen *)MEM_callocN(256*sizeof(VertRen),"findOrAddVert");
		re->vertnodes[a].vert= v;
		
		for(i= (nr & 0xFFFFFF00), a=0; a<256; a++, i++) {
			v[a].index= i;
		}
	}
	v+= (nr & 255);
	return v;
}

void RE_addRenderObject(Render *re, Object *ob, Object *par, int index, int sve, int eve, int sfa, int efa)
{
	ObjectRen *obr= MEM_mallocN(sizeof(ObjectRen), "object render struct");
	
	BLI_addtail(&re->objecttable, obr);
	obr->ob= ob;
	obr->par= par;
	obr->index= index;
	obr->startvert= sve;
	obr->endvert= eve;
	obr->startface= sfa;
	obr->endface= efa;
}

void free_renderdata_vertnodes(VertTableNode *vertnodes)
{
	int a;
	
	if(vertnodes==NULL) return;
	
	for(a=0; vertnodes[a].vert; a++) {
		MEM_freeN(vertnodes[a].vert);
		
		if(vertnodes[a].rad)
			MEM_freeN(vertnodes[a].rad);
		if(vertnodes[a].sticky)
			MEM_freeN(vertnodes[a].sticky);
		if(vertnodes[a].strand)
			MEM_freeN(vertnodes[a].strand);
		if(vertnodes[a].tangent)
			MEM_freeN(vertnodes[a].tangent);
		if(vertnodes[a].stress)
			MEM_freeN(vertnodes[a].stress);
		if(vertnodes[a].winspeed)
			MEM_freeN(vertnodes[a].winspeed);
	}
	
	MEM_freeN(vertnodes);
	
}

void free_renderdata_tables(Render *re)
{
	int a=0;
	
	if(re->blovl) {
		for(a=0; re->blovl[a]; a++)
			MEM_freeN(re->blovl[a]);
		
		MEM_freeN(re->blovl);
		re->blovl= NULL;
		re->blovllen= 0;
	}
	
	if(re->bloha) {
		for(a=0; re->bloha[a]; a++)
			MEM_freeN(re->bloha[a]);

		MEM_freeN(re->bloha);
		re->bloha= NULL;
		re->blohalen= 0;
	}

	if(re->vertnodes) {
		free_renderdata_vertnodes(re->vertnodes);
		re->vertnodes= NULL;
		re->vertnodeslen= 0;
	}
	
	BLI_freelistN(&re->objecttable);
}


/* ------------------------------------------------------------------------ */

HaloRen *RE_findOrAddHalo(Render *re, int nr)
{
	HaloRen *h, **temp;
	int a;

	if(nr<0) {
		printf("error in findOrAddHalo: %d\n",nr);
		return NULL;
	}
	a= nr>>8;
	
	if (a>=re->blohalen-1){  /* Need to allocate more columns..., and keep last element NULL for free loop */
		//printf("Allocating %i more halo groups.  %i total.\n", 
		//	TABLEINITSIZE, re->blohalen+TABLEINITSIZE );
		temp=re->bloha;
		
		re->bloha=(HaloRen**)MEM_callocN(sizeof(void*)*(re->blohalen+TABLEINITSIZE) , "Bloha");
		if(temp) memcpy(re->bloha, temp, re->blohalen*sizeof(void*));
		memset(&(re->bloha[re->blohalen]), 0, TABLEINITSIZE*sizeof(void*));
		re->blohalen+=TABLEINITSIZE;  /*Does this really need to be power of 2?*/
		if(temp) MEM_freeN(temp);	
	}
	
	h= re->bloha[a];
	if(h==NULL) {
		h= (HaloRen *)MEM_callocN(256*sizeof(HaloRen),"findOrAdHalo");
		re->bloha[a]= h;
	}
	h+= (nr & 255);
	return h;
}

/* ------------------------------------------------------------------------ */

VlakRen *RE_findOrAddVlak(Render *re, int nr)
{
	VlakRen *v, **temp;
	int a;

	if(nr<0) {
		printf("error in findOrAddVlak: %d\n",nr);
		return re->blovl[0];
	}
	a= nr>>8;
	
	if (a>=re->blovllen-1){  /* Need to allocate more columns..., and keep last element NULL for free loop */
		// printf("Allocating %i more face groups.  %i total.\n", 
		//	TABLEINITSIZE, re->blovllen+TABLEINITSIZE );
		temp= re->blovl;
		
		re->blovl=(VlakRen**)MEM_callocN(sizeof(void*)*(re->blovllen+TABLEINITSIZE) , "Blovl");
		if(temp) memcpy(re->blovl, temp, re->blovllen*sizeof(void*));
		memset(&(re->blovl[re->blovllen]), 0, TABLEINITSIZE*sizeof(void*));
		re->blovllen+=TABLEINITSIZE;  /*Does this really need to be power of 2?*/
		if(temp) MEM_freeN(temp);	
	}
	
	v= re->blovl[a];
	
	if(v==NULL) {
		v= (VlakRen *)MEM_callocN(256*sizeof(VlakRen),"findOrAddVlak");
		re->blovl[a]= v;
	}
	v+= (nr & 255);
	return v;
}

/* ------------------------------------------------------------------------- */

HaloRen *RE_inithalo(Render *re, Material *ma,   float *vec,   float *vec1, 
				  float *orco,   float hasize,   float vectsize, int seed)
{
	HaloRen *har;
	MTex *mtex;
	float tin, tr, tg, tb, ta;
	float xn, yn, zn, texvec[3], hoco[4], hoco1[4];

	if(hasize==0.0) return NULL;

	projectverto(vec, re->winmat, hoco);
	if(hoco[3]==0.0) return NULL;
	if(vec1) {
		projectverto(vec1, re->winmat, hoco1);
		if(hoco1[3]==0.0) return NULL;
	}

	har= RE_findOrAddHalo(re, re->tothalo++);
	VECCOPY(har->co, vec);
	har->hasize= hasize;

	/* actual projectvert is done in function transform_renderdata() because of parts/border/pano */

	/* halovect */
	if(vec1) {

		har->type |= HA_VECT;

		zn= hoco[3];
		har->xs= 0.5*re->winx*(hoco[0]/zn);
		har->ys= 0.5*re->winy*(hoco[1]/zn);
		har->zs= 0x7FFFFF*(hoco[2]/zn);

		har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn); 

		xn=  har->xs - 0.5*re->winx*(hoco1[0]/hoco1[3]);
		yn=  har->ys - 0.5*re->winy*(hoco1[1]/hoco1[3]);
		if(xn==0.0 || (xn==0.0 && yn==0.0)) zn= 0.0;
		else zn= atan2(yn, xn);

		har->sin= sin(zn);
		har->cos= cos(zn);
		zn= VecLenf(vec1, vec);

		har->hasize= vectsize*zn + (1.0-vectsize)*hasize;
		
		VecSubf(har->no, vec, vec1);
		Normalise(har->no);
	}

	if(ma->mode & MA_HALO_XALPHA) har->type |= HA_XALPHA;

	har->alfa= ma->alpha;
	har->r= ma->r;
	har->g= ma->g;
	har->b= ma->b;
	har->add= (255.0*ma->add);
	har->mat= ma;
	har->hard= ma->har;
	har->seed= seed % 256;

	if(ma->mode & MA_STAR) har->starpoints= ma->starc;
	if(ma->mode & MA_HALO_LINES) har->linec= ma->linec;
	if(ma->mode & MA_HALO_RINGS) har->ringc= ma->ringc;
	if(ma->mode & MA_HALO_FLARE) har->flarec= ma->flarec;


	if(ma->mtex[0]) {

		if( (ma->mode & MA_HALOTEX) ) har->tex= 1;
		else {

			mtex= ma->mtex[0];
			VECCOPY(texvec, vec);

			if(mtex->texco & TEXCO_NORM) {
				;
			}
			else if(mtex->texco & TEXCO_OBJECT) {
				/* texvec[0]+= imatbase->ivec[0]; */
				/* texvec[1]+= imatbase->ivec[1]; */
				/* texvec[2]+= imatbase->ivec[2]; */
				/* Mat3MulVecfl(imatbase->imat, texvec); */
			}
			else {
				if(orco) {
					VECCOPY(texvec, orco);
				}
			}

			externtex(mtex, texvec, &tin, &tr, &tg, &tb, &ta);

			yn= tin*mtex->colfac;
			zn= tin*mtex->varfac;

			if(mtex->mapto & MAP_COL) {
				zn= 1.0-yn;
				har->r= (yn*tr+ zn*ma->r);
				har->g= (yn*tg+ zn*ma->g);
				har->b= (yn*tb+ zn*ma->b);
			}
			if(mtex->texco & 16) {
				har->alfa= tin;
			}
		}
	}

	return har;
}

/* -------------------------- operations on entire database ----------------------- */

/* ugly function for halos in panorama */
static int panotestclip(Render *re, int do_pano, float *v)
{
	/* to be used for halos en infos */
	float abs4;
	short c=0;

	if(do_pano) return testclip(v);

	abs4= fabs(v[3]);

	if(v[2]< -abs4) c=16;		/* this used to be " if(v[2]<0) ", see clippz() */
	else if(v[2]> abs4) c+= 32;

	if( v[1]>abs4) c+=4;
	else if( v[1]< -abs4) c+=8;

	abs4*= re->xparts;
	if( v[0]>abs4) c+=2;
	else if( v[0]< -abs4) c+=1;

	return c;
}

/*
  This adds the hcs coordinates to vertices. It iterates over all
  vertices, halos and faces. After the conversion, we clip in hcs.

  Elsewhere, all primites are converted to vertices. 
  Called in 
  - envmapping (envmap.c)
  - shadow buffering (shadbuf.c)
*/

void project_renderdata(Render *re, void (*projectfunc)(float *, float mat[][4], float *),  int do_pano, float xoffs)
{
	VlakRen *vlr = NULL;
	VertRen *ver = NULL;
	HaloRen *har = NULL;
	float zn, vec[3], hoco[4];
	int a;
	
	if(do_pano) {
		float panophi= xoffs;
		
		re->panosi= sin(panophi);
		re->panoco= cos(panophi);
	}
	
   /* calculate view coordinates (and zbuffer value) */
	for(a=0; a< re->totvert;a++) {
		if((a & 255)==0) ver= RE_findOrAddVert(re, a);
		else ver++;

		if(do_pano) {
			vec[0]= re->panoco*ver->co[0] + re->panosi*ver->co[2];
			vec[1]= ver->co[1];
			vec[2]= -re->panosi*ver->co[0] + re->panoco*ver->co[2];
		}
		else {
			VECCOPY(vec, ver->co);
		}
		/* Go from wcs to hcs ... */
		projectfunc(vec, re->winmat, ver->ho);
		/* ... and clip in that system. */
		ver->clip = testclip(ver->ho);
		/* 
		   Because all other ops are performed in other systems, this is 
		   the only thing that has to be done.
		*/
	}

   /* calculate view coordinates (and zbuffer value) */
	for(a=0; a<re->tothalo; a++) {
		if((a & 255)==0) har= re->bloha[a>>8];
		else har++;

		if(do_pano) {
			vec[0]= re->panoco*har->co[0] + re->panosi*har->co[2];
			vec[1]= har->co[1];
			vec[2]= -re->panosi*har->co[0] + re->panoco*har->co[2];
		}
		else {
			VECCOPY(vec, har->co);
		}

		projectfunc(vec, re->winmat, hoco);
		
		/* we clip halos less critical, but not for the Z */
		hoco[0]*= 0.5;
		hoco[1]*= 0.5;
		
		if( panotestclip(re, do_pano, hoco) ) {
			har->miny= har->maxy= -10000;	/* that way render clips it */
		}
		else if(hoco[3]<0.0) {
			har->miny= har->maxy= -10000;	/* render clips it */
		}
		else /* do the projection...*/
		{
			/* bring back hocos */
			hoco[0]*= 2.0;
			hoco[1]*= 2.0;
			
			zn= hoco[3];
			har->xs= 0.5*re->winx*(1.0+hoco[0]/zn); /* the 0.5 negates the previous 2...*/
			har->ys= 0.5*re->winy*(1.0+hoco[1]/zn);
		
			/* this should be the zbuffer coordinate */
			har->zs= 0x7FFFFF*(hoco[2]/zn);
			/* taking this from the face clip functions? seems ok... */
			har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn);
			
			vec[0]+= har->hasize;
			projectfunc(vec, re->winmat, hoco);
			vec[0]-= har->hasize;
			zn= hoco[3];
			har->rad= fabs(har->xs- 0.5*re->winx*(1.0+hoco[0]/zn));
		
			/* this clip is not really OK, to prevent stars to become too large */
			if(har->type & HA_ONLYSKY) {
				if(har->rad>3.0) har->rad= 3.0;
			}
		
			har->radsq= har->rad*har->rad;
		
			har->miny= har->ys - har->rad/re->ycor;
			har->maxy= har->ys + har->rad/re->ycor;
		
			/* the Zd value is still not really correct for pano */
		
			vec[2]-= har->hasize;	/* z negative, otherwise it's clipped */
			projectfunc(vec, re->winmat, hoco);
			zn= hoco[3];
			zn= fabs( (float)har->zs - 0x7FFFFF*(hoco[2]/zn));
			har->zd= CLAMPIS(zn, 0, INT_MAX);
		
		}
		
	}

	/* set flags at 0 if clipped away */
	for(a=0; a<re->totvlak; a++) {
		if((a & 255)==0) vlr= re->blovl[a>>8];
		else vlr++;

			vlr->flag |= R_VISIBLE;
			if(vlr->v4) {
				if(vlr->v1->clip & vlr->v2->clip & vlr->v3->clip & vlr->v4->clip) vlr->flag &= ~R_VISIBLE;
			}
			else if(vlr->v1->clip & vlr->v2->clip & vlr->v3->clip) vlr->flag &= ~R_VISIBLE;

		}

}

/* ------------------------------------------------------------------------- */

void set_normalflags(Render *re)
{
	VlakRen *vlr = NULL;
	float *v1, xn, yn, zn;
	int a1, doflip;
	
	/* switch normal 'snproj' values (define which axis is the optimal one for calculations) */
	for(a1=0; a1<re->totvlak; a1++) {
		if((a1 & 255)==0) vlr= re->blovl[a1>>8];
		else vlr++;
		
		/* abuse of this flag... this is code that just sets face normal in direction of camera */
		/* that convention we should get rid of */
		if((vlr->flag & R_NOPUNOFLIP)==0) {
			
			doflip= 0;
			if(re->r.mode & R_ORTHO) {
				if(vlr->n[2]>0.0) doflip= 1;
			}
			else {
				v1= vlr->v1->co;
				if( (v1[0]*vlr->n[0] +v1[1]*vlr->n[1] +v1[2]*vlr->n[2])<0.0 ) doflip= 1;
			}
			if(doflip) {
				vlr->n[0]= -vlr->n[0];
				vlr->n[1]= -vlr->n[1];
				vlr->n[2]= -vlr->n[2];
			}
		}
		
		/* recalculate puno. Displace & flipped matrices can screw up */
		vlr->puno= 0;
		if(!(vlr->flag & R_TANGENT)) {
			if( Inpf(vlr->n, vlr->v1->n) < 0.0 ) vlr->puno |= ME_FLIPV1;
			if( Inpf(vlr->n, vlr->v2->n) < 0.0 ) vlr->puno |= ME_FLIPV2;
			if( Inpf(vlr->n, vlr->v3->n) < 0.0 ) vlr->puno |= ME_FLIPV3;
			if(vlr->v4 && Inpf(vlr->n, vlr->v4->n) < 0.0 ) vlr->puno |= ME_FLIPV4;
		}				
		xn= fabs(vlr->n[0]);
		yn= fabs(vlr->n[1]);
		zn= fabs(vlr->n[2]);
		if(zn>=xn && zn>=yn) vlr->snproj= 0;
		else if(yn>=xn && yn>=zn) vlr->snproj= 1;
		else vlr->snproj= 2;

	}
}



