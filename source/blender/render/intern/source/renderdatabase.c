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
 * All data from a Blender scene is converted by the renderconverter/
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
 * vertnodes/vlaknodes/bloha array of the current block, the i-th entry in
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
#include "BLI_memarena.h"

#include "DNA_material_types.h" 
#include "DNA_mesh_types.h" 
#include "DNA_meshdata_types.h" 
#include "DNA_texture_types.h" 

#include "BKE_customdata.h"
#include "BKE_texture.h" 
#include "BKE_DerivedMesh.h"

#include "RE_render_ext.h"	/* externtex */

#include "renderpipeline.h"
#include "render_types.h"
#include "renderdatabase.h"
#include "texture.h"
#include "zbuf.h"

/* ------------------------------------------------------------------------- */

/* More dynamic allocation of options for render vertices and faces, so we dont
   have to reserve this space inside vertices.
   Important; vertices and faces, should have been created already (to get tables
   checked) that's a reason why the calls demand VertRen/VlakRen * as arg, not
   the index */

/* NOTE! the hardcoded table size 256 is used still in code for going quickly over vertices/faces */

#define RE_STICKY_ELEMS		2
#define RE_STRESS_ELEMS		1
#define RE_RAD_ELEMS		4
#define RE_STRAND_ELEMS		1
#define RE_TANGENT_ELEMS	3
#define RE_STRESS_ELEMS		1
#define RE_WINSPEED_ELEMS	4
#define RE_MTFACE_ELEMS		1
#define RE_MCOL_ELEMS		4

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

VertRen *RE_vertren_copy(Render *re, VertRen *ver)
{
	VertRen *v1= RE_findOrAddVert(re, re->totvert++);
	float *fp1, *fp2;
	int index= v1->index;
	
	*v1= *ver;
	v1->index= index;
	
	fp1= RE_vertren_get_sticky(re, ver, 0);
	if(fp1) {
		fp2= RE_vertren_get_sticky(re, v1, 1);
		memcpy(fp2, fp1, RE_STICKY_ELEMS*sizeof(float));
	}
	fp1= RE_vertren_get_stress(re, ver, 0);
	if(fp1) {
		fp2= RE_vertren_get_stress(re, v1, 1);
		memcpy(fp2, fp1, RE_STRESS_ELEMS*sizeof(float));
	}
	fp1= RE_vertren_get_rad(re, ver, 0);
	if(fp1) {
		fp2= RE_vertren_get_rad(re, v1, 1);
		memcpy(fp2, fp1, RE_RAD_ELEMS*sizeof(float));
	}
	fp1= RE_vertren_get_strand(re, ver, 0);
	if(fp1) {
		fp2= RE_vertren_get_strand(re, v1, 1);
		memcpy(fp2, fp1, RE_STRAND_ELEMS*sizeof(float));
	}
	fp1= RE_vertren_get_tangent(re, ver, 0);
	if(fp1) {
		fp2= RE_vertren_get_tangent(re, v1, 1);
		memcpy(fp2, fp1, RE_TANGENT_ELEMS*sizeof(float));
	}
	fp1= RE_vertren_get_winspeed(re, ver, 0);
	if(fp1) {
		fp2= RE_vertren_get_winspeed(re, v1, 1);
		memcpy(fp2, fp1, RE_WINSPEED_ELEMS*sizeof(float));
	}
	return v1;
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

/* ------------------------------------------------------------------------ */

MTFace *RE_vlakren_get_tface(Render *re, VlakRen *vlr, int n, char **name, int verify)
{
	VlakTableNode *node;
	int nr= vlr->index>>8, vlakindex= (vlr->index&255);
	int index= (n<<8) + vlakindex;

	node= &re->vlaknodes[nr];

	if(verify) {
		if(n>=node->totmtface) {
			MTFace **mtface= node->mtface;
			int size= size= (n+1)*256;

			node->mtface= MEM_callocN(size*sizeof(MTFace*), "Vlak mtface");

			if(mtface) {
				size= node->totmtface*256;
				memcpy(node->mtface, mtface, size*sizeof(MTFace*));
				MEM_freeN(mtface);
			}

			node->totmtface= n+1;

			if (!node->names) {
				size= sizeof(*node->names)*256;
				node->names= MEM_callocN(size, "Vlak names");
			}
		}

		if(node->mtface[index]==NULL) {
			node->mtface[index]= BLI_memarena_alloc(re->memArena,
				sizeof(MTFace)*RE_MTFACE_ELEMS);

			node->names[vlakindex]= re->customdata_names.last;
		}
	}
	else {
		if(n>=node->totmtface || node->mtface[index]==NULL)
			return NULL;

		if(name) *name= node->names[vlakindex]->mtface[n];
	}

	return node->mtface[index];
}

MCol *RE_vlakren_get_mcol(Render *re, VlakRen *vlr, int n, char **name, int verify)
{
	VlakTableNode *node;
	int nr= vlr->index>>8, vlakindex= (vlr->index&255);
	int index= (n<<8) + vlakindex;

	node= &re->vlaknodes[nr];

	if(verify) {
		if(n>=node->totmcol) {
			MCol **mcol= node->mcol;
			int size= (n+1)*256;

			node->mcol= MEM_callocN(size*sizeof(MCol*), "Vlak mcol");

			if(mcol) {
				size= node->totmcol*256;
				memcpy(node->mcol, mcol, size*sizeof(MCol*));
				MEM_freeN(mcol);
			}

			node->totmcol= n+1;

			if (!node->names) {
				size= sizeof(*node->names)*256;
				node->names= MEM_callocN(size, "Vlak names");
			}
		}

		if(node->mcol[index]==NULL) {
			node->mcol[index]= BLI_memarena_alloc(re->memArena,
				sizeof(MCol)*RE_MCOL_ELEMS);

			node->names[vlakindex]= re->customdata_names.last;
		}
	}
	else {
		if(n>=node->totmcol || node->mcol[index]==NULL)
			return NULL;

		if(name) *name= node->names[vlakindex]->mcol[n];
	}

	return node->mcol[index];
}

VlakRen *RE_vlakren_copy(Render *re, VlakRen *vlr)
{
	VlakRen *vlr1 = RE_findOrAddVlak(re, re->totvlak++);
	MTFace *mtface, *mtface1;
	MCol *mcol, *mcol1;
	VlakTableNode *node = &re->vlaknodes[vlr->index>>8];
	VlakTableNode *node1 = &re->vlaknodes[vlr1->index>>8];
	int i, index = vlr1->index;
	char *name;

	*vlr1= *vlr;
	vlr1->index= index;

	for (i=0; (mtface=RE_vlakren_get_tface(re, vlr, i, &name, 0)) != NULL; i++) {
		mtface1= RE_vlakren_get_tface(re, vlr1, i, &name, 1);
		memcpy(mtface1, mtface, sizeof(MTFace)*RE_MTFACE_ELEMS);
	}

	for (i=0; (mcol=RE_vlakren_get_mcol(re, vlr, i, &name, 0)) != NULL; i++) {
		mcol1= RE_vlakren_get_mcol(re, vlr1, i, &name, 1);
		memcpy(mcol1, mcol, sizeof(MCol)*RE_MCOL_ELEMS);
	}

	if (node->names && node1->names)
		node1->names[vlr1->index&255] = node->names[vlr->index&255];

	return vlr1;
}

static int vlakren_remap_layer_num(int n, int active)
{
	/* make the active layer the first */
	if (n == active) return 0;
	else if (n < active) return n+1;
	else return n;
}

void RE_vlakren_set_customdata_names(Render *re, CustomData *data)
{
	/* CustomData layer names are stored per object here, because the
	   DerivedMesh which stores the layers is freed */
	
	CustomDataNames *cdn= MEM_callocN(sizeof(*cdn), "CustomDataNames");
	CustomDataLayer *layer;
	int numlayers, i, mtfn, mcn, n;

	BLI_addtail(&re->customdata_names, cdn);

	if (CustomData_has_layer(data, CD_MTFACE)) {
		numlayers= CustomData_number_of_layers(data, CD_MTFACE);
		cdn->mtface= MEM_callocN(sizeof(*cdn->mtface)*numlayers, "mtfacenames");
	}

	if (CustomData_has_layer(data, CD_MCOL)) {
		numlayers= CustomData_number_of_layers(data, CD_MCOL);
		cdn->mcol= MEM_callocN(sizeof(*cdn->mcol)*numlayers, "mcolnames");
	}

	for (i=0, mtfn=0, mcn=0; i < data->totlayer; i++) {
		layer= &data->layers[i];

		if (layer->type == CD_MTFACE) {
			n= vlakren_remap_layer_num(mtfn++, layer->active_rnd);
			strcpy(cdn->mtface[n], layer->name);
		}
		else if (layer->type == CD_MCOL) {
			n= vlakren_remap_layer_num(mcn++, layer->active_rnd);
			strcpy(cdn->mcol[n], layer->name);
		}
	}
}

VlakRen *RE_findOrAddVlak(Render *re, int nr)
{
	VlakTableNode *temp;
	VlakRen *v;
	int a;

	if(nr<0) {
		printf("error in findOrAddVlak: %d\n",nr);
		return re->vlaknodes[0].vlak;
	}
	a= nr>>8;
	
	if (a>=re->vlaknodeslen-1){  /* Need to allocate more columns..., and keep last element NULL for free loop */
		temp= re->vlaknodes;
		
		re->vlaknodes= MEM_mallocN(sizeof(VlakTableNode)*(re->vlaknodeslen+TABLEINITSIZE) , "vlaknodes");
		if(temp) memcpy(re->vlaknodes, temp, re->vlaknodeslen*sizeof(VlakTableNode));
		memset(re->vlaknodes+re->vlaknodeslen, 0, TABLEINITSIZE*sizeof(VlakTableNode));

		re->vlaknodeslen+=TABLEINITSIZE;  /*Does this really need to be power of 2?*/
		if(temp) MEM_freeN(temp);	
	}

	v= re->vlaknodes[a].vlak;
	
	if(v==NULL) {
		int i;

		v= (VlakRen *)MEM_callocN(256*sizeof(VlakRen),"findOrAddVlak");
		re->vlaknodes[a].vlak= v;

		for(i= (nr & 0xFFFFFF00), a=0; a<256; a++, i++)
			v[a].index= i;
	}
	v+= (nr & 255);
	return v;
}

/* ------------------------------------------------------------------------ */

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

void free_renderdata_vlaknodes(VlakTableNode *vlaknodes)
{
	int a;
	
	if(vlaknodes==NULL) return;
	
	for(a=0; vlaknodes[a].vlak; a++) {
		MEM_freeN(vlaknodes[a].vlak);
		
		if(vlaknodes[a].mtface)
			MEM_freeN(vlaknodes[a].mtface);
		if(vlaknodes[a].mcol)
			MEM_freeN(vlaknodes[a].mcol);
		if(vlaknodes[a].names)
			MEM_freeN(vlaknodes[a].names);
	}
	
	MEM_freeN(vlaknodes);
}

void free_renderdata_tables(Render *re)
{
	int a=0;
	CustomDataNames *cdn;
	
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

	if(re->vlaknodes) {
		free_renderdata_vlaknodes(re->vlaknodes);
		re->vlaknodes= NULL;
		re->vlaknodeslen= 0;
	}

	for(cdn=re->customdata_names.first; cdn; cdn=cdn->next) {
		if(cdn->mtface)
			MEM_freeN(cdn->mtface);
		if(cdn->mcol)
			MEM_freeN(cdn->mcol);
	}

	BLI_freelistN(&re->customdata_names);
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

	/* actual projectvert is done in function project_renderdata() because of parts/border/pano */
	/* we do it here for sorting of halos */
	zn= hoco[3];
	har->xs= 0.5*re->winx*(hoco[0]/zn);
	har->ys= 0.5*re->winy*(hoco[1]/zn);
	har->zs= 0x7FFFFF*(hoco[2]/zn);
	
	har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn); 
	
	/* halovect */
	if(vec1) {

		har->type |= HA_VECT;

		xn=  har->xs - 0.5*re->winx*(hoco1[0]/hoco1[3]);
		yn=  har->ys - 0.5*re->winy*(hoco1[1]/hoco1[3]);
		if(xn==0.0 || (xn==0.0 && yn==0.0)) zn= 0.0;
		else zn= atan2(yn, xn);

		har->sin= sin(zn);
		har->cos= cos(zn);
		zn= VecLenf(vec1, vec);

		har->hasize= vectsize*zn + (1.0-vectsize)*hasize;
		
		VecSubf(har->no, vec, vec1);
		Normalize(har->no);
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

HaloRen *RE_inithalo_particle(Render *re, DerivedMesh *dm, Material *ma,   float *vec,   float *vec1, 
				  float *orco, float *uvco, float hasize, float vectsize, int seed)
{
	HaloRen *har;
	MTex *mtex;
	float tin, tr, tg, tb, ta;
	float xn, yn, zn, texvec[3], hoco[4], hoco1[4], in[3],tex[3],out[3];
	int i;

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

	/* actual projectvert is done in function project_renderdata() because of parts/border/pano */
	/* we do it here for sorting of halos */
	zn= hoco[3];
	har->xs= 0.5*re->winx*(hoco[0]/zn);
	har->ys= 0.5*re->winy*(hoco[1]/zn);
	har->zs= 0x7FFFFF*(hoco[2]/zn);
	
	har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn); 
	
	/* halovect */
	if(vec1) {

		har->type |= HA_VECT;

		xn=  har->xs - 0.5*re->winx*(hoco1[0]/hoco1[3]);
		yn=  har->ys - 0.5*re->winy*(hoco1[1]/hoco1[3]);
		if(xn==0.0 || (xn==0.0 && yn==0.0)) zn= 0.0;
		else zn= atan2(yn, xn);

		har->sin= sin(zn);
		har->cos= cos(zn);
		zn= VecLenf(vec1, vec)*0.5;

		har->hasize= vectsize*zn + (1.0-vectsize)*hasize;
		
		VecSubf(har->no, vec, vec1);
		Normalize(har->no);
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

	if((ma->mode & MA_HALOTEX) && ma->mtex[0]){
		har->tex= 1;
		i=1;
	}
	
	for(i=0; i<MAX_MTEX; i++)
		if(ma->mtex[i] && (ma->septex & (1<<i))==0) {
			mtex= ma->mtex[i];
			VECCOPY(texvec, vec);

			if(mtex->texco & TEXCO_NORM) {
				;
			}
			else if(mtex->texco & TEXCO_OBJECT) {
				if(mtex->object){
					float imat[4][4];
					/* imat should really be cached somewhere before this */
					Mat4Invert(imat,mtex->object->obmat);
					Mat4MulVecfl(imat,texvec);
				}
				/* texvec[0]+= imatbase->ivec[0]; */
				/* texvec[1]+= imatbase->ivec[1]; */
				/* texvec[2]+= imatbase->ivec[2]; */
				/* Mat3MulVecfl(imatbase->imat, texvec); */
			}
			else if(mtex->texco & TEXCO_GLOB){
				VECCOPY(texvec,vec);
			}
			else if(mtex->texco & TEXCO_UV && uvco){
				int uv_index=CustomData_get_named_layer_index(&dm->faceData,CD_MTFACE,mtex->uvname);
				if(uv_index<0)
					uv_index=CustomData_get_active_layer_index(&dm->faceData,CD_MTFACE);

				uv_index-=CustomData_get_layer_index(&dm->faceData,CD_MTFACE);

				texvec[0]=2.0f*uvco[2*uv_index]-1.0f;
				texvec[1]=2.0f*uvco[2*uv_index+1]-1.0f;
				texvec[2]=0.0f;
			}
			else if(orco) {
				VECCOPY(texvec, orco);
			}

			externtex(mtex, texvec, &tin, &tr, &tg, &tb, &ta);

			//yn= tin*mtex->colfac;
			//zn= tin*mtex->varfac;
			if(mtex->mapto & MAP_COL) {
				tex[0]=tr;
				tex[1]=tg;
				tex[2]=tb;
				out[0]=har->r;
				out[1]=har->g;
				out[2]=har->b;

				texture_rgb_blend(in,tex,out,tin,mtex->colfac,mtex->blendtype);
			//	zn= 1.0-yn;
				//har->r= (yn*tr+ zn*ma->r);
				//har->g= (yn*tg+ zn*ma->g);
				//har->b= (yn*tb+ zn*ma->b);
				har->r= in[0];
				har->g= in[1];
				har->b= in[2];
			}
			if(mtex->mapto & MAP_ALPHA)
				har->alfa = texture_value_blend(mtex->def_var,har->alfa,tin,mtex->varfac,mtex->blendtype,mtex->maptoneg & MAP_ALPHA);
			if(mtex->mapto & MAP_HAR)
				har->hard = 1.0+126.0*texture_value_blend(mtex->def_var,((float)har->hard)/127.0,tin,mtex->varfac,mtex->blendtype,mtex->maptoneg & MAP_HAR);
			if(mtex->mapto & MAP_RAYMIRR)
				har->hasize = 100.0*texture_value_blend(mtex->def_var,har->hasize/100.0,tin,mtex->varfac,mtex->blendtype,mtex->maptoneg & MAP_RAYMIRR);
			/* now what on earth is this good for?? */
			//if(mtex->texco & 16) {
			//	har->alfa= tin;
			//}
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

	if(do_pano==0) return testclip(v);

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
		if((a & 255)==0) vlr= re->vlaknodes[a>>8].vlak;
		else vlr++;

		if(!re->excludeob || vlr->ob != re->excludeob) {
			vlr->flag |= R_VISIBLE;
			if(vlr->v4) {
				if(vlr->v1->clip & vlr->v2->clip & vlr->v3->clip & vlr->v4->clip) vlr->flag &= ~R_VISIBLE;
			}
			else if(vlr->v1->clip & vlr->v2->clip & vlr->v3->clip) vlr->flag &= ~R_VISIBLE;
		}
		else
			vlr->flag &= ~R_VISIBLE;
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
		if((a1 & 255)==0) vlr= re->vlaknodes[a1>>8].vlak;
		else vlr++;
		
		vlr->noflag= 0;

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
				vlr->noflag |= R_FLIPPED_NO;
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
		if(zn>=xn && zn>=yn) vlr->noflag |= R_SNPROJ_X;
		else if(yn>=xn && yn>=zn) vlr->noflag |= R_SNPROJ_Y;
		else vlr->noflag |= R_SNPROJ_Z;

	}
}



