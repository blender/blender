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
 * Storage, retrieval and query of render specific data.
 */

/*
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
 * blove/bloha/blovl array of the current block, the i-th entry in
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

#include <math.h>
#include <string.h>
#include "MEM_guardedalloc.h"
#include "BLI_arithb.h"

#include "DNA_texture_types.h" 
#include "BKE_texture.h" 

#include "render.h"
#include "render_intern.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */

VertRen *RE_findOrAddVert(int nr)
{
	VertRen *v, **temp;
	static int rblovelen=TABLEINITSIZE;
	int a;

	if(nr<0) {
		printf("error in findOrAddVert: %d\n",nr);
		return R.blove[0];
	}
	a= nr>>8;
	
	if (a>=rblovelen){  /* Need to allocate more columns...*/
		// printf("Allocating %i more vert groups.  %i total.\n", 
		//	TABLEINITSIZE, rblovelen+TABLEINITSIZE );
		temp=R.blove;
		R.blove=(VertRen**)MEM_callocN(sizeof(void*)*(rblovelen+TABLEINITSIZE) , "Blove");
		memcpy(R.blove, temp, rblovelen*sizeof(void*));
		memset(&(R.blove[a]), 0, TABLEINITSIZE*sizeof(void*));
		rblovelen+=TABLEINITSIZE; 
		MEM_freeN(temp);	
	}
	
	v= R.blove[a];
	if(v==0) {
		v= (VertRen *)MEM_callocN(256*sizeof(VertRen),"findOrAddVert");
		R.blove[a]= v;
	}
	v+= (nr & 255);
	return v;
}

/* ------------------------------------------------------------------------ */
int rblohalen=TABLEINITSIZE;
HaloRen *RE_findOrAddHalo(int nr)
{
	HaloRen *h, **temp;
	int a;

	if(nr<0) {
		printf("error in findOrAddHalo: %d\n",nr);
		return R.bloha[0];
	}
	a= nr>>8;
	
	if (a>=rblohalen){  /* Need to allocate more columns...*/
		//printf("Allocating %i more halo groups.  %i total.\n", 
		//	TABLEINITSIZE, rblohalen+TABLEINITSIZE );
		temp=R.bloha;
		R.bloha=(HaloRen**)MEM_callocN(sizeof(void*)*(rblohalen+TABLEINITSIZE) , "Blove");
		memcpy(R.bloha, temp, rblohalen*sizeof(void*));
		memset(&(R.bloha[a]), 0, TABLEINITSIZE*sizeof(void*));
		rblohalen+=TABLEINITSIZE;  /*Does this really need to be power of 2?*/
		MEM_freeN(temp);	
	}
	
	h= R.bloha[a];
	if(h==0) {
		h= (HaloRen *)MEM_callocN(256*sizeof(HaloRen),"findOrAdHalo");
		R.bloha[a]= h;
	}
	h+= (nr & 255);
	return h;
}

/* ------------------------------------------------------------------------ */

VlakRen *RE_findOrAddVlak(int nr)
{
	VlakRen *v, **temp;
	static int rblovllen=TABLEINITSIZE;
	int a;

	if(nr<0) {
		printf("error in findOrAddVlak: %d\n",nr);
		return R.blovl[0];
	}
	a= nr>>8;
	
	if (a>=rblovllen){  /* Need to allocate more columns...*/
		// printf("Allocating %i more face groups.  %i total.\n", 
		//	TABLEINITSIZE, rblovllen+TABLEINITSIZE );
		temp=R.blovl;
		R.blovl=(VlakRen**)MEM_callocN(sizeof(void*)*(rblovllen+TABLEINITSIZE) , "Blove");
		memcpy(R.blovl, temp, rblovllen*sizeof(void*));
		memset(&(R.blovl[a]), 0, TABLEINITSIZE*sizeof(void*));
		rblovllen+=TABLEINITSIZE;  /*Does this really need to be power of 2?*/
		MEM_freeN(temp);	
	}
	
	v= R.blovl[a];
	
	if(v==0) {
		v= (VlakRen *)MEM_callocN(256*sizeof(VlakRen),"findOrAddVlak");
		R.blovl[a]= v;
	}
	v+= (nr & 255);
	return v;
}

/* ------------------------------------------------------------------------- */

extern float Tin, Tr, Tg, Tb;
HaloRen *RE_inithalo(Material *ma, 
				  float *vec, 
				  float *vec1, 
				  float *orco, 
				  float hasize, 
				  float vectsize)
{
	HaloRen *har;
	MTex *mtex;
	float xn, yn, zn, texvec[3], hoco[4], hoco1[4];

	if(hasize==0) return 0;

	RE_projectverto(vec, hoco);
	if(hoco[3]==0.0) return 0;
	if(vec1) {
		RE_projectverto(vec1, hoco1);
		if(hoco1[3]==0.0) return 0;
	}

	har= RE_findOrAddHalo(R.tothalo++);
	VECCOPY(har->co, vec);
	har->hasize= hasize;

	/* projectvert is done function 'zbufvlaggen' because of parts/border/pano */

	/* halovect */
	if(vec1) {

		har->type |= HA_VECT;

		zn= hoco[3];
		har->xs= 0.5*R.rectx*(hoco[0]/zn);
		har->ys= 0.5*R.recty*(hoco[1]/zn);
		har->zs= 0x7FFFFF*(1.0+hoco[2]/zn);

      har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn); 

		xn=  har->xs - 0.5*R.rectx*(hoco1[0]/hoco1[3]);
		yn=  har->ys - 0.5*R.recty*(hoco1[1]/hoco1[3]);
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
	har->r= 255.0*ma->r;
	har->g= 255.0*ma->g;
	har->b= 255.0*ma->b;
	har->add= 255.0*ma->add;
	har->mat= ma->ren;
	har->hard= ma->har;
	har->seed= ma->ren->seed1 % 256;

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

			externtex(mtex, texvec);

			yn= Tin*mtex->colfac;
			zn= Tin*mtex->varfac;

			if(mtex->mapto & MAP_COL) {
				zn= 1.0-yn;
				har->r= 255.0*(yn*Tr+ zn*ma->r);
				har->g= 255.0*(yn*Tg+ zn*ma->g);
				har->b= 255.0*(yn*Tb+ zn*ma->b);
			}
			if(mtex->texco & 16) {
				har->alfa= 255.0*Tin;
			}
		}
	}

	return har;
}
/* ------------------------------------------------------------------------- */
