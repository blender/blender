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

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BLI_arithb.h"

#include "DNA_material_types.h" 
#include "DNA_mesh_types.h" 
#include "DNA_texture_types.h" 

#include "BKE_texture.h" 

#include "render.h"

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

/* render allocates totvert/256 of these nodes, for lookup and quick alloc */
typedef struct VertTableNode {
	struct VertRen *vert;
	float *rad;
	float *sticky;
	float *strand;
	float *tangent;
	float *stress;
} VertTableNode;

float *RE_vertren_get_sticky(VertRen *ver, int verify)
{
	float *sticky;
	int nr= ver->index>>8;
	
	sticky= R.vertnodes[nr].sticky;
	if(sticky==NULL) {
		if(verify) 
			sticky= R.vertnodes[nr].sticky= MEM_mallocN(256*RE_STICKY_ELEMS*sizeof(float), "sticky table");
		else
			return NULL;
	}
	return sticky + (ver->index & 255)*RE_STICKY_ELEMS;
}

float *RE_vertren_get_stress(VertRen *ver, int verify)
{
	float *stress;
	int nr= ver->index>>8;
	
	stress= R.vertnodes[nr].stress;
	if(stress==NULL) {
		if(verify) 
			stress= R.vertnodes[nr].stress= MEM_mallocN(256*RE_STRESS_ELEMS*sizeof(float), "stress table");
		else
			return NULL;
	}
	return stress + (ver->index & 255)*RE_STRESS_ELEMS;
}

/* this one callocs! */
float *RE_vertren_get_rad(VertRen *ver, int verify)
{
	float *rad;
	int nr= ver->index>>8;
	
	rad= R.vertnodes[nr].rad;
	if(rad==NULL) {
		if(verify) 
			rad= R.vertnodes[nr].rad= MEM_callocN(256*RE_RAD_ELEMS*sizeof(float), "rad table");
		else
			return NULL;
	}
	return rad + (ver->index & 255)*RE_RAD_ELEMS;
}

float *RE_vertren_get_strand(VertRen *ver, int verify)
{
	float *strand;
	int nr= ver->index>>8;
	
	strand= R.vertnodes[nr].strand;
	if(strand==NULL) {
		if(verify) 
			strand= R.vertnodes[nr].strand= MEM_mallocN(256*RE_STRAND_ELEMS*sizeof(float), "strand table");
		else
			return NULL;
	}
	return strand + (ver->index & 255)*RE_STRAND_ELEMS;
}

/* needs calloc */
float *RE_vertren_get_tangent(VertRen *ver, int verify)
{
	float *tangent;
	int nr= ver->index>>8;
	
	tangent= R.vertnodes[nr].tangent;
	if(tangent==NULL) {
		if(verify) 
			tangent= R.vertnodes[nr].tangent= MEM_callocN(256*RE_TANGENT_ELEMS*sizeof(float), "tangent table");
		else
			return NULL;
	}
	return tangent + (ver->index & 255)*RE_TANGENT_ELEMS;
}


VertRen *RE_findOrAddVert(int nr)
{
	VertTableNode *temp;
	VertRen *v;
	static int rvertnodeslen=TABLEINITSIZE;
	int a;

	if(nr<0) {
		printf("error in findOrAddVert: %d\n",nr);
		return R.vertnodes[0].vert;
	}
	a= nr>>8;
	
	if (a>=rvertnodeslen-1){  /* Need to allocate more columns..., and keep last element NULL for free loop */
		temp= R.vertnodes;
		
		R.vertnodes= MEM_mallocN(sizeof(VertTableNode)*(rvertnodeslen+TABLEINITSIZE) , "vertnodes");
		memcpy(R.vertnodes, temp, rvertnodeslen*sizeof(VertTableNode));
		memset(R.vertnodes+rvertnodeslen, 0, TABLEINITSIZE*sizeof(VertTableNode));
		
		rvertnodeslen+=TABLEINITSIZE; 
		MEM_freeN(temp);	
	}
	
	v= R.vertnodes[a].vert;
	if(v==NULL) {
		int i;
		
		v= (VertRen *)MEM_callocN(256*sizeof(VertRen),"findOrAddVert");
		R.vertnodes[a].vert= v;
		
		for(i= (nr & 0xFFFFFF00), a=0; a<256; a++, i++) {
			v[a].index= i;
		}
	}
	v+= (nr & 255);
	return v;
}

void RE_free_vertex_tables(void)
{
	int a=0;
	
	while(R.vertnodes[a].vert) {
		if(R.vertnodes[a].vert) {
			MEM_freeN(R.vertnodes[a].vert);
			R.vertnodes[a].vert= NULL;

			if(R.vertnodes[a].rad) {
				MEM_freeN(R.vertnodes[a].rad);
				R.vertnodes[a].rad= NULL;
			}
			if(R.vertnodes[a].sticky) {
				MEM_freeN(R.vertnodes[a].sticky);
				R.vertnodes[a].sticky= NULL;
			}
			if(R.vertnodes[a].strand) {
				MEM_freeN(R.vertnodes[a].strand);
				R.vertnodes[a].strand= NULL;
			}
			if(R.vertnodes[a].tangent) {
				MEM_freeN(R.vertnodes[a].tangent);
				R.vertnodes[a].tangent= NULL;
			}
			if(R.vertnodes[a].stress) {
				MEM_freeN(R.vertnodes[a].stress);
				R.vertnodes[a].stress= NULL;
			}
		}
		a++;
	}
}

/* only once, on startup */
void RE_init_vertex_tables(void)
{
	R.vertnodes= MEM_callocN(sizeof(VertTableNode)*TABLEINITSIZE , "vertnodes");
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
	
	if (a>=rblohalen-1){  /* Need to allocate more columns..., and keep last element NULL for free loop */
		//printf("Allocating %i more halo groups.  %i total.\n", 
		//	TABLEINITSIZE, rblohalen+TABLEINITSIZE );
		temp=R.bloha;
		R.bloha=(HaloRen**)MEM_callocN(sizeof(void*)*(rblohalen+TABLEINITSIZE) , "Bloha");
		memcpy(R.bloha, temp, rblohalen*sizeof(void*));
		memset(&(R.bloha[rblohalen]), 0, TABLEINITSIZE*sizeof(void*));
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
	
	if (a>=rblovllen-1){  /* Need to allocate more columns..., and keep last element NULL for free loop */
		// printf("Allocating %i more face groups.  %i total.\n", 
		//	TABLEINITSIZE, rblovllen+TABLEINITSIZE );
		temp=R.blovl;
		R.blovl=(VlakRen**)MEM_callocN(sizeof(void*)*(rblovllen+TABLEINITSIZE) , "Blovl");
		memcpy(R.blovl, temp, rblovllen*sizeof(void*));
		memset(&(R.blovl[rblovllen]), 0, TABLEINITSIZE*sizeof(void*));
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

HaloRen *RE_inithalo(Material *ma,   float *vec,   float *vec1, 
				  float *orco,   float hasize,   float vectsize, int seed)
{
	HaloRen *har;
	MTex *mtex;
	float tin, tr, tg, tb, ta;
	float xn, yn, zn, texvec[3], hoco[4], hoco1[4];

	if(hasize==0.0) return NULL;

	RE_projectverto(vec, hoco);
	if(hoco[3]==0.0) return NULL;
	if(vec1) {
		RE_projectverto(vec1, hoco1);
		if(hoco1[3]==0.0) return NULL;
	}

	har= RE_findOrAddHalo(R.tothalo++);
	VECCOPY(har->co, vec);
	har->hasize= hasize;

	/* projectvert is done in function 'zbufvlaggen' because of parts/border/pano */

	/* halovect */
	if(vec1) {

		har->type |= HA_VECT;

		zn= hoco[3];
		har->xs= 0.5*R.rectx*(hoco[0]/zn);
		har->ys= 0.5*R.recty*(hoco[1]/zn);
		har->zs= 0x7FFFFF*(hoco[2]/zn);

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
/* ------------------------------------------------------------------------- */
