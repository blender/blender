/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): 2004-2006, Blender Foundation, full recode
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/renderdatabase.c
 *  \ingroup render
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
 * The pointer to the correct entry is returned. Memory is guaranteed
 * to exist (as long as the malloc does not break). Since guarded
 * allocation is used, memory _must_ be available. Otherwise, an
 * exit(0) would occur.
 * 
 */

#include <limits.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"


#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_hash.h"

#include "DNA_material_types.h" 
#include "DNA_meshdata_types.h" 
#include "DNA_texture_types.h"
#include "DNA_listBase.h"
#include "DNA_particle_types.h"

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"

#include "RE_render_ext.h"	/* externtex */

#include "rayintersection.h"
#include "rayobject.h"
#include "render_types.h"
#include "renderdatabase.h"
#include "zbuf.h"

/* ------------------------------------------------------------------------- */

/* More dynamic allocation of options for render vertices and faces, so we don't
 * have to reserve this space inside vertices.
 * Important; vertices and faces, should have been created already (to get tables
 * checked) that's a reason why the calls demand VertRen/VlakRen * as arg, not
 * the index */

/* NOTE! the hardcoded table size 256 is used still in code for going quickly over vertices/faces */
#define RE_STRESS_ELEMS		1
#define RE_RAD_ELEMS		4
#define RE_STRAND_ELEMS		1
#define RE_TANGENT_ELEMS	3
#define RE_WINSPEED_ELEMS	4
#define RE_MTFACE_ELEMS		1
#define RE_MCOL_ELEMS		4
#define RE_UV_ELEMS			2
#define RE_VLAK_ORIGINDEX_ELEMS	1
#define RE_VERT_ORIGINDEX_ELEMS	1
#define RE_SURFNOR_ELEMS	3
#define RE_RADFACE_ELEMS	1
#define RE_SIMPLIFY_ELEMS	2
#define RE_FACE_ELEMS		1
#define RE_NMAP_TANGENT_ELEMS	16

float *RE_vertren_get_stress(ObjectRen *obr, VertRen *ver, int verify)
{
	float *stress;
	int nr= ver->index>>8;
	
	stress= obr->vertnodes[nr].stress;
	if (stress==NULL) {
		if (verify) 
			stress= obr->vertnodes[nr].stress= MEM_mallocN(256*RE_STRESS_ELEMS*sizeof(float), "stress table");
		else
			return NULL;
	}
	return stress + (ver->index & 255)*RE_STRESS_ELEMS;
}

/* this one callocs! */
float *RE_vertren_get_rad(ObjectRen *obr, VertRen *ver, int verify)
{
	float *rad;
	int nr= ver->index>>8;
	
	rad= obr->vertnodes[nr].rad;
	if (rad==NULL) {
		if (verify) 
			rad= obr->vertnodes[nr].rad= MEM_callocN(256*RE_RAD_ELEMS*sizeof(float), "rad table");
		else
			return NULL;
	}
	return rad + (ver->index & 255)*RE_RAD_ELEMS;
}

float *RE_vertren_get_strand(ObjectRen *obr, VertRen *ver, int verify)
{
	float *strand;
	int nr= ver->index>>8;
	
	strand= obr->vertnodes[nr].strand;
	if (strand==NULL) {
		if (verify) 
			strand= obr->vertnodes[nr].strand= MEM_mallocN(256*RE_STRAND_ELEMS*sizeof(float), "strand table");
		else
			return NULL;
	}
	return strand + (ver->index & 255)*RE_STRAND_ELEMS;
}

/* needs calloc */
float *RE_vertren_get_tangent(ObjectRen *obr, VertRen *ver, int verify)
{
	float *tangent;
	int nr= ver->index>>8;
	
	tangent= obr->vertnodes[nr].tangent;
	if (tangent==NULL) {
		if (verify) 
			tangent= obr->vertnodes[nr].tangent= MEM_callocN(256*RE_TANGENT_ELEMS*sizeof(float), "tangent table");
		else
			return NULL;
	}
	return tangent + (ver->index & 255)*RE_TANGENT_ELEMS;
}

/* needs calloc! not all renderverts have them */
/* also winspeed is exception, it is stored per instance */
float *RE_vertren_get_winspeed(ObjectInstanceRen *obi, VertRen *ver, int verify)
{
	float *winspeed;
	int totvector;
	
	winspeed= obi->vectors;
	if (winspeed==NULL) {
		if (verify) {
			totvector= obi->obr->totvert + obi->obr->totstrand;
			winspeed= obi->vectors= MEM_callocN(totvector*RE_WINSPEED_ELEMS*sizeof(float), "winspeed table");
		}
		else
			return NULL;
	}
	return winspeed + ver->index*RE_WINSPEED_ELEMS;
}

int *RE_vertren_get_origindex(ObjectRen *obr, VertRen *ver, int verify)
{
	int *origindex;
	int nr= ver->index>>8;

	origindex= obr->vertnodes[nr].origindex;
	if (origindex==NULL) {
		if (verify)
			origindex= obr->vertnodes[nr].origindex= MEM_mallocN(256*RE_VERT_ORIGINDEX_ELEMS*sizeof(int), "origindex table");
		else
			return NULL;
	}
	return origindex + (ver->index & 255)*RE_VERT_ORIGINDEX_ELEMS;
}

VertRen *RE_vertren_copy(ObjectRen *obr, VertRen *ver)
{
	VertRen *v1= RE_findOrAddVert(obr, obr->totvert++);
	float *fp1, *fp2;
	int *int1, *int2;
	int index= v1->index;
	
	*v1= *ver;
	v1->index= index;

	fp1= RE_vertren_get_stress(obr, ver, 0);
	if (fp1) {
		fp2= RE_vertren_get_stress(obr, v1, 1);
		memcpy(fp2, fp1, RE_STRESS_ELEMS*sizeof(float));
	}
	fp1= RE_vertren_get_rad(obr, ver, 0);
	if (fp1) {
		fp2= RE_vertren_get_rad(obr, v1, 1);
		memcpy(fp2, fp1, RE_RAD_ELEMS*sizeof(float));
	}
	fp1= RE_vertren_get_strand(obr, ver, 0);
	if (fp1) {
		fp2= RE_vertren_get_strand(obr, v1, 1);
		memcpy(fp2, fp1, RE_STRAND_ELEMS*sizeof(float));
	}
	fp1= RE_vertren_get_tangent(obr, ver, 0);
	if (fp1) {
		fp2= RE_vertren_get_tangent(obr, v1, 1);
		memcpy(fp2, fp1, RE_TANGENT_ELEMS*sizeof(float));
	}
	int1= RE_vertren_get_origindex(obr, ver, 0);
	if (int1) {
		int2= RE_vertren_get_origindex(obr, v1, 1);
		memcpy(int2, int1, RE_VERT_ORIGINDEX_ELEMS*sizeof(int));
	}
	return v1;
}

VertRen *RE_findOrAddVert(ObjectRen *obr, int nr)
{
	VertTableNode *temp;
	VertRen *v;
	int a;

	if (nr<0) {
		printf("error in findOrAddVert: %d\n", nr);
		return NULL;
	}
	a= nr>>8;
	
	if (a>=obr->vertnodeslen-1) {  /* Need to allocate more columns..., and keep last element NULL for free loop */
		temp= obr->vertnodes;
		
		obr->vertnodes= MEM_mallocN(sizeof(VertTableNode)*(obr->vertnodeslen+TABLEINITSIZE), "vertnodes");
		if (temp) memcpy(obr->vertnodes, temp, obr->vertnodeslen*sizeof(VertTableNode));
		memset(obr->vertnodes+obr->vertnodeslen, 0, TABLEINITSIZE*sizeof(VertTableNode));
		
		obr->vertnodeslen+=TABLEINITSIZE; 
		if (temp) MEM_freeN(temp);
	}
	
	v= obr->vertnodes[a].vert;
	if (v==NULL) {
		int i;
		
		v= (VertRen *)MEM_callocN(256*sizeof(VertRen), "findOrAddVert");
		obr->vertnodes[a].vert= v;
		
		for (i= (nr & 0xFFFFFF00), a=0; a<256; a++, i++) {
			v[a].index= i;
		}
	}
	v+= (nr & 255);
	return v;
}

/* ------------------------------------------------------------------------ */

MTFace *RE_vlakren_get_tface(ObjectRen *obr, VlakRen *vlr, int n, char **name, int verify)
{
	VlakTableNode *node;
	int nr= vlr->index>>8, vlakindex= (vlr->index&255);
	int index= (n<<8) + vlakindex;

	node= &obr->vlaknodes[nr];

	if (verify) {
		if (n>=node->totmtface) {
			MTFace *mtface= node->mtface;
			int size= (n+1)*256;

			node->mtface= MEM_callocN(size*sizeof(MTFace), "Vlak mtface");

			if (mtface) {
				size= node->totmtface*256;
				memcpy(node->mtface, mtface, size*sizeof(MTFace));
				MEM_freeN(mtface);
			}

			node->totmtface= n+1;
		}
	}
	else {
		if (n>=node->totmtface)
			return NULL;

		if (name) *name= obr->mtface[n];
	}

	return node->mtface + index;
}

MCol *RE_vlakren_get_mcol(ObjectRen *obr, VlakRen *vlr, int n, char **name, int verify)
{
	VlakTableNode *node;
	int nr= vlr->index>>8, vlakindex= (vlr->index&255);
	int index= (n<<8) + vlakindex;

	node= &obr->vlaknodes[nr];

	if (verify) {
		if (n>=node->totmcol) {
			MCol *mcol= node->mcol;
			int size= (n+1)*256;

			node->mcol= MEM_callocN(size*sizeof(MCol)*RE_MCOL_ELEMS, "Vlak mcol");

			if (mcol) {
				size= node->totmcol*256;
				memcpy(node->mcol, mcol, size*sizeof(MCol)*RE_MCOL_ELEMS);
				MEM_freeN(mcol);
			}

			node->totmcol= n+1;
		}
	}
	else {
		if (n>=node->totmcol)
			return NULL;

		if (name) *name= obr->mcol[n];
	}

	return node->mcol + index*RE_MCOL_ELEMS;
}

int *RE_vlakren_get_origindex(ObjectRen *obr, VlakRen *vlak, int verify)
{
	int *origindex;
	int nr= vlak->index>>8;

	origindex= obr->vlaknodes[nr].origindex;
	if (origindex==NULL) {
		if (verify)
			origindex= obr->vlaknodes[nr].origindex= MEM_callocN(256*RE_VLAK_ORIGINDEX_ELEMS*sizeof(int), "origindex table");
		else
			return NULL;
	}
	return origindex + (vlak->index & 255)*RE_VLAK_ORIGINDEX_ELEMS;
}

float *RE_vlakren_get_surfnor(ObjectRen *obr, VlakRen *vlak, int verify)
{
	float *surfnor;
	int nr= vlak->index>>8;
	
	surfnor= obr->vlaknodes[nr].surfnor;
	if (surfnor==NULL) {
		if (verify) 
			surfnor= obr->vlaknodes[nr].surfnor= MEM_callocN(256*RE_SURFNOR_ELEMS*sizeof(float), "surfnor table");
		else
			return NULL;
	}
	return surfnor + (vlak->index & 255)*RE_SURFNOR_ELEMS;
}

float *RE_vlakren_get_nmap_tangent(ObjectRen *obr, VlakRen *vlak, int index, bool verify)
{
	float **tangents;
	int nr= vlak->index>>8;

	tangents = obr->vlaknodes[nr].tangent_arrays;

	if (index + 1 > 8) {
		return NULL;
	}

	index = index < 0 ? 0: index;

	if (tangents[index] == NULL) {
		if (verify) {
			tangents[index] = MEM_callocN(256*RE_NMAP_TANGENT_ELEMS*sizeof(float), "tangent table");
		}
		else
			return NULL;
	}

	return tangents[index] + (vlak->index & 255)*RE_NMAP_TANGENT_ELEMS;
}

RadFace **RE_vlakren_get_radface(ObjectRen *obr, VlakRen *vlak, int verify)
{
	RadFace **radface;
	int nr= vlak->index>>8;
	
	radface= obr->vlaknodes[nr].radface;
	if (radface==NULL) {
		if (verify) 
			radface = obr->vlaknodes[nr].radface= MEM_callocN(256 * RE_RADFACE_ELEMS * sizeof(void *), "radface table");
		else
			return NULL;
	}
	return radface + (vlak->index & 255)*RE_RADFACE_ELEMS;
}

VlakRen *RE_vlakren_copy(ObjectRen *obr, VlakRen *vlr)
{
	VlakRen *vlr1 = RE_findOrAddVlak(obr, obr->totvlak++);
	MTFace *mtface, *mtface1;
	MCol *mcol, *mcol1;
	float *surfnor, *surfnor1;
	float *tangent, *tangent1;
	int *origindex, *origindex1;
	RadFace **radface, **radface1;
	int i, index = vlr1->index;
	char *name;

	*vlr1= *vlr;
	vlr1->index= index;

	for (i=0; (mtface=RE_vlakren_get_tface(obr, vlr, i, &name, 0)) != NULL; i++) {
		mtface1= RE_vlakren_get_tface(obr, vlr1, i, &name, 1);
		memcpy(mtface1, mtface, sizeof(MTFace)*RE_MTFACE_ELEMS);
	}

	for (i=0; (mcol=RE_vlakren_get_mcol(obr, vlr, i, &name, 0)) != NULL; i++) {
		mcol1= RE_vlakren_get_mcol(obr, vlr1, i, &name, 1);
		memcpy(mcol1, mcol, sizeof(MCol)*RE_MCOL_ELEMS);
	}

	origindex= RE_vlakren_get_origindex(obr, vlr, 0);
	if (origindex) {
		origindex1= RE_vlakren_get_origindex(obr, vlr1, 1);
		/* Just an int, but memcpy for consistency. */
		memcpy(origindex1, origindex, sizeof(int)*RE_VLAK_ORIGINDEX_ELEMS);
	}

	surfnor= RE_vlakren_get_surfnor(obr, vlr, 0);
	if (surfnor) {
		surfnor1= RE_vlakren_get_surfnor(obr, vlr1, 1);
		copy_v3_v3(surfnor1, surfnor);
	}

	for (i=0; i < MAX_MTFACE; i++) {
		tangent = RE_vlakren_get_nmap_tangent(obr, vlr, i, false);
		if (!tangent)
			continue;
		tangent1 = RE_vlakren_get_nmap_tangent(obr, vlr1, i, true);
		memcpy(tangent1, tangent, sizeof(float)*RE_NMAP_TANGENT_ELEMS);
	}

	radface= RE_vlakren_get_radface(obr, vlr, 0);
	if (radface) {
		radface1= RE_vlakren_get_radface(obr, vlr1, 1);
		*radface1= *radface;
	}

	return vlr1;
}

void RE_vlakren_get_normal(Render *UNUSED(re), ObjectInstanceRen *obi, VlakRen *vlr, float r_nor[3])
{
	float (*nmat)[3]= obi->nmat;

	if (obi->flag & R_TRANSFORMED) {
		mul_v3_m3v3(r_nor, nmat, vlr->n);
		normalize_v3(r_nor);
	}
	else {
		copy_v3_v3(r_nor, vlr->n);
	}
}

void RE_set_customdata_names(ObjectRen *obr, CustomData *data)
{
	/* CustomData layer names are stored per object here, because the
	 * DerivedMesh which stores the layers is freed */
	
	CustomDataLayer *layer;
	int numtf = 0, numcol = 0, i, mtfn, mcn;

	if (CustomData_has_layer(data, CD_MTFACE)) {
		numtf= CustomData_number_of_layers(data, CD_MTFACE);
		obr->mtface= MEM_callocN(sizeof(*obr->mtface)*numtf, "mtfacenames");
	}

	if (CustomData_has_layer(data, CD_MCOL)) {
		numcol= CustomData_number_of_layers(data, CD_MCOL);
		obr->mcol= MEM_callocN(sizeof(*obr->mcol)*numcol, "mcolnames");
	}

	for (i=0, mtfn=0, mcn=0; i < data->totlayer; i++) {
		layer= &data->layers[i];

		if (layer->type == CD_MTFACE) {
			BLI_strncpy(obr->mtface[mtfn++], layer->name, sizeof(layer->name));
			obr->actmtface= CLAMPIS(layer->active_rnd, 0, numtf);
			obr->bakemtface= layer->active;
		}
		else if (layer->type == CD_MCOL) {
			BLI_strncpy(obr->mcol[mcn++], layer->name, sizeof(layer->name));
			obr->actmcol= CLAMPIS(layer->active_rnd, 0, numcol);
		}
	}
}

VlakRen *RE_findOrAddVlak(ObjectRen *obr, int nr)
{
	VlakTableNode *temp;
	VlakRen *v;
	int a;

	if (nr<0) {
		printf("error in findOrAddVlak: %d\n", nr);
		return obr->vlaknodes[0].vlak;
	}
	a= nr>>8;
	
	if (a>=obr->vlaknodeslen-1) {  /* Need to allocate more columns..., and keep last element NULL for free loop */
		temp= obr->vlaknodes;
		
		obr->vlaknodes= MEM_mallocN(sizeof(VlakTableNode)*(obr->vlaknodeslen+TABLEINITSIZE), "vlaknodes");
		if (temp) memcpy(obr->vlaknodes, temp, obr->vlaknodeslen*sizeof(VlakTableNode));
		memset(obr->vlaknodes+obr->vlaknodeslen, 0, TABLEINITSIZE*sizeof(VlakTableNode));

		obr->vlaknodeslen+=TABLEINITSIZE;  /*Does this really need to be power of 2?*/
		if (temp) MEM_freeN(temp);
	}

	v= obr->vlaknodes[a].vlak;
	
	if (v==NULL) {
		int i;

		v= (VlakRen *)MEM_callocN(256*sizeof(VlakRen), "findOrAddVlak");
		obr->vlaknodes[a].vlak= v;

		for (i= (nr & 0xFFFFFF00), a=0; a<256; a++, i++)
			v[a].index= i;
	}
	v+= (nr & 255);
	return v;
}

/* ------------------------------------------------------------------------ */

float *RE_strandren_get_surfnor(ObjectRen *obr, StrandRen *strand, int verify)
{
	float *surfnor;
	int nr= strand->index>>8;
	
	surfnor= obr->strandnodes[nr].surfnor;
	if (surfnor==NULL) {
		if (verify) 
			surfnor= obr->strandnodes[nr].surfnor= MEM_callocN(256*RE_SURFNOR_ELEMS*sizeof(float), "surfnor strand table");
		else
			return NULL;
	}
	return surfnor + (strand->index & 255)*RE_SURFNOR_ELEMS;
}

float *RE_strandren_get_uv(ObjectRen *obr, StrandRen *strand, int n, char **name, int verify)
{
	StrandTableNode *node;
	int nr= strand->index>>8, strandindex= (strand->index&255);
	int index= (n<<8) + strandindex;

	node= &obr->strandnodes[nr];

	if (verify) {
		if (n>=node->totuv) {
			float *uv= node->uv;
			int size= (n+1)*256;

			node->uv= MEM_callocN(size*sizeof(float)*RE_UV_ELEMS, "strand uv table");

			if (uv) {
				size= node->totuv*256;
				memcpy(node->uv, uv, size*sizeof(float)*RE_UV_ELEMS);
				MEM_freeN(uv);
			}

			node->totuv= n+1;
		}
	}
	else {
		if (n>=node->totuv)
			return NULL;

		if (name) *name= obr->mtface[n];
	}

	return node->uv + index*RE_UV_ELEMS;
}

MCol *RE_strandren_get_mcol(ObjectRen *obr, StrandRen *strand, int n, char **name, int verify)
{
	StrandTableNode *node;
	int nr= strand->index>>8, strandindex= (strand->index&255);
	int index= (n<<8) + strandindex;

	node= &obr->strandnodes[nr];

	if (verify) {
		if (n>=node->totmcol) {
			MCol *mcol= node->mcol;
			int size= (n+1)*256;

			node->mcol= MEM_callocN(size*sizeof(MCol)*RE_MCOL_ELEMS, "strand mcol table");

			if (mcol) {
				size= node->totmcol*256;
				memcpy(node->mcol, mcol, size*sizeof(MCol)*RE_MCOL_ELEMS);
				MEM_freeN(mcol);
			}

			node->totmcol= n+1;
		}
	}
	else {
		if (n>=node->totmcol)
			return NULL;

		if (name) *name= obr->mcol[n];
	}

	return node->mcol + index*RE_MCOL_ELEMS;
}

float *RE_strandren_get_simplify(struct ObjectRen *obr, struct StrandRen *strand, int verify)
{
	float *simplify;
	int nr= strand->index>>8;
	
	simplify= obr->strandnodes[nr].simplify;
	if (simplify==NULL) {
		if (verify) 
			simplify= obr->strandnodes[nr].simplify= MEM_callocN(256*RE_SIMPLIFY_ELEMS*sizeof(float), "simplify strand table");
		else
			return NULL;
	}
	return simplify + (strand->index & 255)*RE_SIMPLIFY_ELEMS;
}

int *RE_strandren_get_face(ObjectRen *obr, StrandRen *strand, int verify)
{
	int *face;
	int nr= strand->index>>8;
	
	face= obr->strandnodes[nr].face;
	if (face==NULL) {
		if (verify) 
			face= obr->strandnodes[nr].face= MEM_callocN(256*RE_FACE_ELEMS*sizeof(int), "face strand table");
		else
			return NULL;
	}
	return face + (strand->index & 255)*RE_FACE_ELEMS;
}

/* winspeed is exception, it is stored per instance */
float *RE_strandren_get_winspeed(ObjectInstanceRen *obi, StrandRen *strand, int verify)
{
	float *winspeed;
	int totvector;
	
	winspeed= obi->vectors;
	if (winspeed==NULL) {
		if (verify) {
			totvector= obi->obr->totvert + obi->obr->totstrand;
			winspeed= obi->vectors= MEM_callocN(totvector*RE_WINSPEED_ELEMS*sizeof(float), "winspeed strand table");
		}
		else
			return NULL;
	}
	return winspeed + (obi->obr->totvert + strand->index)*RE_WINSPEED_ELEMS;
}

StrandRen *RE_findOrAddStrand(ObjectRen *obr, int nr)
{
	StrandTableNode *temp;
	StrandRen *v;
	int a;

	if (nr<0) {
		printf("error in findOrAddStrand: %d\n", nr);
		return obr->strandnodes[0].strand;
	}
	a= nr>>8;
	
	if (a>=obr->strandnodeslen-1) {  /* Need to allocate more columns..., and keep last element NULL for free loop */
		temp= obr->strandnodes;
		
		obr->strandnodes= MEM_mallocN(sizeof(StrandTableNode)*(obr->strandnodeslen+TABLEINITSIZE), "strandnodes");
		if (temp) memcpy(obr->strandnodes, temp, obr->strandnodeslen*sizeof(StrandTableNode));
		memset(obr->strandnodes+obr->strandnodeslen, 0, TABLEINITSIZE*sizeof(StrandTableNode));

		obr->strandnodeslen+=TABLEINITSIZE;  /*Does this really need to be power of 2?*/
		if (temp) MEM_freeN(temp);
	}

	v= obr->strandnodes[a].strand;
	
	if (v==NULL) {
		int i;

		v= (StrandRen *)MEM_callocN(256*sizeof(StrandRen), "findOrAddStrand");
		obr->strandnodes[a].strand= v;

		for (i= (nr & 0xFFFFFF00), a=0; a<256; a++, i++)
			v[a].index= i;
	}
	v+= (nr & 255);
	return v;
}

StrandBuffer *RE_addStrandBuffer(ObjectRen *obr, int totvert)
{
	StrandBuffer *strandbuf;

	strandbuf= MEM_callocN(sizeof(StrandBuffer), "StrandBuffer");
	strandbuf->vert= MEM_callocN(sizeof(StrandVert)*totvert, "StrandVert");
	strandbuf->totvert= totvert;
	strandbuf->obr= obr;

	obr->strandbuf= strandbuf;

	return strandbuf;
}

/* ------------------------------------------------------------------------ */

ObjectRen *RE_addRenderObject(Render *re, Object *ob, Object *par, int index, int psysindex, int lay)
{
	ObjectRen *obr= MEM_callocN(sizeof(ObjectRen), "object render struct");
	
	BLI_addtail(&re->objecttable, obr);
	obr->ob= ob;
	obr->par= par;
	obr->index= index;
	obr->psysindex= psysindex;
	obr->lay= lay;

	return obr;
}

void free_renderdata_vertnodes(VertTableNode *vertnodes)
{
	int a;
	
	if (vertnodes==NULL) return;
	
	for (a=0; vertnodes[a].vert; a++) {
		MEM_freeN(vertnodes[a].vert);
		
		if (vertnodes[a].rad)
			MEM_freeN(vertnodes[a].rad);
		if (vertnodes[a].strand)
			MEM_freeN(vertnodes[a].strand);
		if (vertnodes[a].tangent)
			MEM_freeN(vertnodes[a].tangent);
		if (vertnodes[a].stress)
			MEM_freeN(vertnodes[a].stress);
		if (vertnodes[a].winspeed)
			MEM_freeN(vertnodes[a].winspeed);
		if (vertnodes[a].origindex)
			MEM_freeN(vertnodes[a].origindex);
	}
	
	MEM_freeN(vertnodes);
}

void free_renderdata_vlaknodes(VlakTableNode *vlaknodes)
{
	int a;
	
	if (vlaknodes==NULL) return;
	
	for (a=0; vlaknodes[a].vlak; a++) {
		MEM_freeN(vlaknodes[a].vlak);
		
		if (vlaknodes[a].mtface)
			MEM_freeN(vlaknodes[a].mtface);
		if (vlaknodes[a].mcol)
			MEM_freeN(vlaknodes[a].mcol);
		if (vlaknodes[a].origindex)
			MEM_freeN(vlaknodes[a].origindex);
		if (vlaknodes[a].surfnor)
			MEM_freeN(vlaknodes[a].surfnor);
		for (int b = 0; b < MAX_MTFACE; b++) {
			if (vlaknodes[a].tangent_arrays[b])
				MEM_freeN(vlaknodes[a].tangent_arrays[b]);
		}
		if (vlaknodes[a].radface)
			MEM_freeN(vlaknodes[a].radface);
	}
	
	MEM_freeN(vlaknodes);
}

static void free_renderdata_strandnodes(StrandTableNode *strandnodes)
{
	int a;
	
	if (strandnodes==NULL) return;
	
	for (a=0; strandnodes[a].strand; a++) {
		MEM_freeN(strandnodes[a].strand);
		
		if (strandnodes[a].uv)
			MEM_freeN(strandnodes[a].uv);
		if (strandnodes[a].mcol)
			MEM_freeN(strandnodes[a].mcol);
		if (strandnodes[a].winspeed)
			MEM_freeN(strandnodes[a].winspeed);
		if (strandnodes[a].surfnor)
			MEM_freeN(strandnodes[a].surfnor);
		if (strandnodes[a].simplify)
			MEM_freeN(strandnodes[a].simplify);
		if (strandnodes[a].face)
			MEM_freeN(strandnodes[a].face);
	}
	
	MEM_freeN(strandnodes);
}

void free_renderdata_tables(Render *re)
{
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	StrandBuffer *strandbuf;
	int a=0;
	
	for (obr=re->objecttable.first; obr; obr=obr->next) {
		if (obr->vertnodes) {
			free_renderdata_vertnodes(obr->vertnodes);
			obr->vertnodes= NULL;
			obr->vertnodeslen= 0;
		}

		if (obr->vlaknodes) {
			free_renderdata_vlaknodes(obr->vlaknodes);
			obr->vlaknodes= NULL;
			obr->vlaknodeslen= 0;
			obr->totvlak= 0;
		}

		if (obr->bloha) {
			for (a=0; obr->bloha[a]; a++)
				MEM_freeN(obr->bloha[a]);

			MEM_freeN(obr->bloha);
			obr->bloha= NULL;
			obr->blohalen= 0;
		}

		if (obr->strandnodes) {
			free_renderdata_strandnodes(obr->strandnodes);
			obr->strandnodes= NULL;
			obr->strandnodeslen= 0;
		}

		strandbuf= obr->strandbuf;
		if (strandbuf) {
			if (strandbuf->vert) MEM_freeN(strandbuf->vert);
			if (strandbuf->bound) MEM_freeN(strandbuf->bound);
			MEM_freeN(strandbuf);
		}

		if (obr->mtface)
			MEM_freeN(obr->mtface);

		if (obr->mcol)
			MEM_freeN(obr->mcol);
			
		if (obr->rayfaces) {
			MEM_freeN(obr->rayfaces);
			obr->rayfaces = NULL;
		}

		if (obr->rayprimitives) {
			MEM_freeN(obr->rayprimitives);
			obr->rayprimitives = NULL;
		}

		if (obr->raytree) {
			RE_rayobject_free(obr->raytree);
			obr->raytree = NULL;
		}
	}

	if (re->objectinstance) {
		for (obi=re->instancetable.first; obi; obi=obi->next) {
			if (obi->vectors)
				MEM_freeN(obi->vectors);

			if (obi->raytree)
				RE_rayobject_free(obi->raytree);
		}

		MEM_freeN(re->objectinstance);
		re->objectinstance= NULL;
		re->totinstance= 0;
		re->instancetable.first= re->instancetable.last= NULL;
	}

	if (re->sortedhalos) {
		MEM_freeN(re->sortedhalos);
		re->sortedhalos= NULL;
	}

	BLI_freelistN(&re->customdata_names);
	BLI_freelistN(&re->objecttable);
	BLI_freelistN(&re->instancetable);
}

/* ------------------------------------------------------------------------ */

HaloRen *RE_findOrAddHalo(ObjectRen *obr, int nr)
{
	HaloRen *h, **temp;
	int a;

	if (nr<0) {
		printf("error in findOrAddHalo: %d\n", nr);
		return NULL;
	}
	a= nr>>8;
	
	if (a>=obr->blohalen-1) {  /* Need to allocate more columns..., and keep last element NULL for free loop */
		//printf("Allocating %i more halo groups.  %i total.\n", 
		//	TABLEINITSIZE, obr->blohalen+TABLEINITSIZE );
		temp=obr->bloha;
		
		obr->bloha = (HaloRen **)MEM_callocN(sizeof(void *) * (obr->blohalen + TABLEINITSIZE), "Bloha");
		if (temp) memcpy(obr->bloha, temp, obr->blohalen*sizeof(void *));
		memset(&(obr->bloha[obr->blohalen]), 0, TABLEINITSIZE * sizeof(void *));
		obr->blohalen+=TABLEINITSIZE;  /*Does this really need to be power of 2?*/
		if (temp) MEM_freeN(temp);
	}
	
	h= obr->bloha[a];
	if (h==NULL) {
		h= (HaloRen *)MEM_callocN(256*sizeof(HaloRen), "findOrAdHalo");
		obr->bloha[a]= h;
	}
	h+= (nr & 255);
	return h;
}

/* ------------------------------------------------------------------------- */

HaloRen *RE_inithalo(Render *re, ObjectRen *obr, Material *ma,
                     const float vec[3], const float vec1[3],
                     const float *orco, float hasize, float vectsize, int seed)
{
	const bool skip_load_image = (re->r.scemode & R_NO_IMAGE_LOAD) != 0;
	const bool texnode_preview = (re->r.scemode & R_TEXNODE_PREVIEW) != 0;
	HaloRen *har;
	MTex *mtex;
	float tin, tr, tg, tb, ta;
	float xn, yn, zn, texvec[3], hoco[4], hoco1[4];

	if (hasize==0.0f) return NULL;

	projectverto(vec, re->winmat, hoco);
	if (hoco[3]==0.0f) return NULL;
	if (vec1) {
		projectverto(vec1, re->winmat, hoco1);
		if (hoco1[3]==0.0f) return NULL;
	}

	har= RE_findOrAddHalo(obr, obr->tothalo++);
	copy_v3_v3(har->co, vec);
	har->hasize= hasize;

	/* actual projectvert is done in function project_renderdata() because of parts/border/pano */
	/* we do it here for sorting of halos */
	zn= hoco[3];
	har->xs= 0.5f*re->winx*(hoco[0]/zn);
	har->ys= 0.5f*re->winy*(hoco[1]/zn);
	har->zs= 0x7FFFFF*(hoco[2]/zn);
	
	har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn); 
	
	/* halovect */
	if (vec1) {

		har->type |= HA_VECT;

		xn=  har->xs - 0.5f*re->winx*(hoco1[0]/hoco1[3]);
		yn=  har->ys - 0.5f*re->winy*(hoco1[1]/hoco1[3]);
		if (yn == 0.0f && xn >= 0.0f) zn = 0.0f;
		else zn = atan2f(yn, xn);

		har->sin = sinf(zn);
		har->cos = cosf(zn);
		zn= len_v3v3(vec1, vec);

		har->hasize= vectsize*zn + (1.0f-vectsize)*hasize;
		
		sub_v3_v3v3(har->no, vec, vec1);
		normalize_v3(har->no);
	}

	if (ma->mode & MA_HALO_XALPHA) har->type |= HA_XALPHA;

	har->alfa= ma->alpha;
	har->r= ma->r;
	har->g= ma->g;
	har->b= ma->b;
	har->add= (255.0f*ma->add);
	har->mat= ma;
	har->hard= ma->har;
	har->seed= seed % 256;

	if (ma->mode & MA_STAR) har->starpoints= ma->starc;
	if (ma->mode & MA_HALO_LINES) har->linec= ma->linec;
	if (ma->mode & MA_HALO_RINGS) har->ringc= ma->ringc;
	if (ma->mode & MA_HALO_FLARE) har->flarec= ma->flarec;


	if (ma->mtex[0]) {

		if (ma->mode & MA_HALOTEX) {
			har->tex = 1;
		}
		else if (har->mat->septex & (1 << 0)) {
			/* only 1 level textures */
		}
		else {
			mtex= ma->mtex[0];
			copy_v3_v3(texvec, vec);

			if (mtex->texco & TEXCO_NORM) {
				;
			}
			else if (mtex->texco & TEXCO_OBJECT) {
				/* texvec[0]+= imatbase->ivec[0]; */
				/* texvec[1]+= imatbase->ivec[1]; */
				/* texvec[2]+= imatbase->ivec[2]; */
				/* mul_m3_v3(imatbase->imat, texvec); */
			}
			else {
				if (orco) {
					copy_v3_v3(texvec, orco);
				}
			}

			externtex(mtex,
			          texvec,
			          &tin, &tr, &tg, &tb, &ta,
			          0,
			          re->pool,
			          skip_load_image,
			          texnode_preview);

			yn= tin*mtex->colfac;
			//zn= tin*mtex->alphafac;

			if (mtex->mapto & MAP_COL) {
				zn= 1.0f-yn;
				har->r= (yn*tr+ zn*ma->r);
				har->g= (yn*tg+ zn*ma->g);
				har->b= (yn*tb+ zn*ma->b);
			}
			if (mtex->texco & TEXCO_UV) {
				har->alfa= tin;
			}
			if (mtex->mapto & MAP_ALPHA)
				har->alfa= tin;
		}
	}

	har->pool = re->pool;
	har->skip_load_image = skip_load_image;
	har->texnode_preview = texnode_preview;

	return har;
}

HaloRen *RE_inithalo_particle(Render *re, ObjectRen *obr, DerivedMesh *dm, Material *ma,
                              const float vec[3], const float vec1[3],
                              const float *orco, const float *uvco, float hasize, float vectsize, int seed, const float pa_co[3])
{
	const bool skip_load_image = (re->r.scemode & R_NO_IMAGE_LOAD) != 0;
	const bool texnode_preview = (re->r.scemode & R_TEXNODE_PREVIEW) != 0;
	HaloRen *har;
	MTex *mtex;
	float tin, tr, tg, tb, ta;
	float xn, yn, zn, texvec[3], hoco[4], hoco1[4], in[3], tex[3], out[3];
	int i, hasrgb;

	if (hasize==0.0f) return NULL;

	projectverto(vec, re->winmat, hoco);
	if (hoco[3]==0.0f) return NULL;
	if (vec1) {
		projectverto(vec1, re->winmat, hoco1);
		if (hoco1[3]==0.0f) return NULL;
	}

	har= RE_findOrAddHalo(obr, obr->tothalo++);
	copy_v3_v3(har->co, vec);
	har->hasize= hasize;

	/* actual projectvert is done in function project_renderdata() because of parts/border/pano */
	/* we do it here for sorting of halos */
	zn= hoco[3];
	har->xs= 0.5f*re->winx*(hoco[0]/zn);
	har->ys= 0.5f*re->winy*(hoco[1]/zn);
	har->zs= 0x7FFFFF*(hoco[2]/zn);
	
	har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn); 
	
	/* halovect */
	if (vec1) {

		har->type |= HA_VECT;

		xn=  har->xs - 0.5f*re->winx*(hoco1[0]/hoco1[3]);
		yn=  har->ys - 0.5f*re->winy*(hoco1[1]/hoco1[3]);
		if (yn == 0.0f && xn >= 0.0f) zn = 0.0f;
		else zn = atan2f(yn, xn);

		har->sin = sinf(zn);
		har->cos = cosf(zn);
		zn= len_v3v3(vec1, vec)*0.5f;

		har->hasize= vectsize*zn + (1.0f-vectsize)*hasize;
		
		sub_v3_v3v3(har->no, vec, vec1);
		normalize_v3(har->no);
	}

	if (ma->mode & MA_HALO_XALPHA) har->type |= HA_XALPHA;

	har->alfa= ma->alpha;
	har->r= ma->r;
	har->g= ma->g;
	har->b= ma->b;
	har->add= (255.0f*ma->add);
	har->mat= ma;
	har->hard= ma->har;
	har->seed= seed % 256;

	if (ma->mode & MA_STAR) har->starpoints= ma->starc;
	if (ma->mode & MA_HALO_LINES) har->linec= ma->linec;
	if (ma->mode & MA_HALO_RINGS) har->ringc= ma->ringc;
	if (ma->mode & MA_HALO_FLARE) har->flarec= ma->flarec;

	if ((ma->mode & MA_HALOTEX) && ma->mtex[0])
		har->tex= 1;
	
	for (i=0; i<MAX_MTEX; i++)
		if (ma->mtex[i] && (ma->septex & (1<<i))==0) {
			mtex= ma->mtex[i];
			copy_v3_v3(texvec, vec);

			if (mtex->texco & TEXCO_NORM) {
				;
			}
			else if (mtex->texco & TEXCO_OBJECT) {
				if (mtex->object)
					mul_m4_v3(mtex->object->imat_ren, texvec);
			}
			else if (mtex->texco & TEXCO_GLOB) {
				copy_v3_v3(texvec, vec);
			}
			else if (mtex->texco & TEXCO_UV && uvco) {
				int uv_index=CustomData_get_named_layer_index(&dm->faceData, CD_MTFACE, mtex->uvname);
				if (uv_index<0)
					uv_index=CustomData_get_active_layer_index(&dm->faceData, CD_MTFACE);

				uv_index-=CustomData_get_layer_index(&dm->faceData, CD_MTFACE);

				texvec[0]=2.0f*uvco[2*uv_index]-1.0f;
				texvec[1]=2.0f*uvco[2*uv_index+1]-1.0f;
				texvec[2]=0.0f;
			}
			else if (mtex->texco & TEXCO_PARTICLE) {
				/* particle coordinates in range [0, 1] */
				texvec[0] = 2.f * pa_co[0] - 1.f;
				texvec[1] = 2.f * pa_co[1] - 1.f;
				texvec[2] = pa_co[2];
			}
			else if (orco) {
				copy_v3_v3(texvec, orco);
			}

			hasrgb = externtex(mtex,
			                   texvec,
			                   &tin, &tr, &tg, &tb, &ta,
			                   0,
			                   re->pool,
			                   skip_load_image,
			                   texnode_preview);

			//yn= tin*mtex->colfac;
			//zn= tin*mtex->alphafac;
			if (mtex->mapto & MAP_COL) {
				tex[0]=tr;
				tex[1]=tg;
				tex[2]=tb;
				out[0]=har->r;
				out[1]=har->g;
				out[2]=har->b;

				texture_rgb_blend(in, tex, out, tin, mtex->colfac, mtex->blendtype);
			//	zn= 1.0-yn;
				//har->r= (yn*tr+ zn*ma->r);
				//har->g= (yn*tg+ zn*ma->g);
				//har->b= (yn*tb+ zn*ma->b);
				har->r= in[0];
				har->g= in[1];
				har->b= in[2];
			}

			/* alpha returned, so let's use it instead of intensity */
			if (hasrgb)
				tin = ta;

			if (mtex->mapto & MAP_ALPHA)
				har->alfa = texture_value_blend(mtex->def_var, har->alfa, tin, mtex->alphafac, mtex->blendtype);
			if (mtex->mapto & MAP_HAR)
				har->hard = 1.0f+126.0f*texture_value_blend(mtex->def_var, ((float)har->hard)/127.0f, tin, mtex->hardfac, mtex->blendtype);
			if (mtex->mapto & MAP_RAYMIRR)
				har->hasize = 100.0f*texture_value_blend(mtex->def_var, har->hasize/100.0f, tin, mtex->raymirrfac, mtex->blendtype);
			if (mtex->mapto & MAP_TRANSLU) {
				float add = texture_value_blend(mtex->def_var, (float)har->add/255.0f, tin, mtex->translfac, mtex->blendtype);
				CLAMP(add, 0.f, 1.f);
				har->add = 255.0f*add;
			}
			/* now what on earth is this good for?? */
			//if (mtex->texco & 16) {
			//	har->alfa= tin;
			//}
		}

	har->pool = re->pool;
	har->skip_load_image = (re->r.scemode & R_NO_IMAGE_LOAD) != 0;
	har->texnode_preview = (re->r.scemode & R_TEXNODE_PREVIEW) != 0;

	return har;
}

/* -------------------------- operations on entire database ----------------------- */

/* ugly function for halos in panorama */
static int panotestclip(Render *re, bool do_pano, float v[4])
{
	/* part size (ensure we run RE_parts_clamp first) */
	BLI_assert(re->partx == min_ii(re->r.tilex, re->rectx));
	BLI_assert(re->party == min_ii(re->r.tiley, re->recty));

	if (do_pano == false) {
		return testclip(v);
	}
	else {
		/* to be used for halos en infos */
		float abs4;
		short c = 0;

		int xparts = (re->rectx + re->partx - 1) / re->partx;

		abs4= fabsf(v[3]);

		if (v[2]< -abs4) c=16;		/* this used to be " if (v[2]<0) ", see clippz() */
		else if (v[2]> abs4) c+= 32;

		if ( v[1]>abs4) c+=4;
		else if ( v[1]< -abs4) c+=8;

		abs4*= xparts;
		if ( v[0]>abs4) c+=2;
		else if ( v[0]< -abs4) c+=1;

		return c;
	}
}

/**
 * This adds the hcs coordinates to vertices. It iterates over all
 * vertices, halos and faces. After the conversion, we clip in hcs.
 *
 * Elsewhere, all primites are converted to vertices.
 * Called in
 * - envmapping (envmap.c)
 * - shadow buffering (shadbuf.c)
 */

void project_renderdata(Render *re,
                        void (*projectfunc)(const float *, float mat[4][4], float *),
                        bool do_pano, float xoffs, bool UNUSED(do_buckets))
{
	ObjectRen *obr;
	HaloRen *har = NULL;
	float zn, vec[3], hoco[4];
	int a;

	if (do_pano) {
		float panophi= xoffs;
		
		re->panosi = sinf(panophi);
		re->panoco = cosf(panophi);
	}

	for (obr=re->objecttable.first; obr; obr=obr->next) {
		/* calculate view coordinates (and zbuffer value) */
		for (a=0; a<obr->tothalo; a++) {
			if ((a & 255)==0) har= obr->bloha[a>>8];
			else har++;

			if (do_pano) {
				vec[0]= re->panoco*har->co[0] + re->panosi*har->co[2];
				vec[1]= har->co[1];
				vec[2]= -re->panosi*har->co[0] + re->panoco*har->co[2];
			}
			else {
				copy_v3_v3(vec, har->co);
			}

			projectfunc(vec, re->winmat, hoco);
			
			/* we clip halos less critical, but not for the Z */
			hoco[0]*= 0.5f;
			hoco[1]*= 0.5f;
			
			if ( panotestclip(re, do_pano, hoco) ) {
				har->miny= har->maxy= -10000;	/* that way render clips it */
			}
			else if (hoco[3]<0.0f) {
				har->miny= har->maxy= -10000;	/* render clips it */
			}
			else { /* do the projection...*/
				/* bring back hocos */
				hoco[0]*= 2.0f;
				hoco[1]*= 2.0f;
				
				zn= hoco[3];
				har->xs= 0.5f*re->winx*(1.0f+hoco[0]/zn); /* the 0.5 negates the previous 2...*/
				har->ys= 0.5f*re->winy*(1.0f+hoco[1]/zn);
			
				/* this should be the zbuffer coordinate */
				har->zs= 0x7FFFFF*(hoco[2]/zn);
				/* taking this from the face clip functions? seems ok... */
				har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn);
				
				vec[0]+= har->hasize;
				projectfunc(vec, re->winmat, hoco);
				vec[0]-= har->hasize;
				zn= hoco[3];
				har->rad= fabsf(har->xs- 0.5f*re->winx*(1.0f+hoco[0]/zn));
			
				/* this clip is not really OK, to prevent stars to become too large */
				if (har->type & HA_ONLYSKY) {
					if (har->rad>3.0f) har->rad= 3.0f;
				}
			
				har->radsq= har->rad*har->rad;
			
				har->miny= har->ys - har->rad/re->ycor;
				har->maxy= har->ys + har->rad/re->ycor;
			
				/* the Zd value is still not really correct for pano */
			
				vec[2] -= har->hasize;  /* z negative, otherwise it's clipped */
				projectfunc(vec, re->winmat, hoco);
				zn = hoco[3];
				zn = fabsf((float)har->zs - 0x7FFFFF * (hoco[2] / zn));
				har->zd = CLAMPIS(zn, 0, INT_MAX);
			
			}
			
		}
	}
}

/* ------------------------------------------------------------------------- */

void RE_updateRenderInstance(Render *re, ObjectInstanceRen *obi, int flag)
{
	/* flag specifies what things have changed. */
	if (flag & RE_OBJECT_INSTANCES_UPDATE_OBMAT) {
		copy_m4_m4(obi->obmat, obi->ob->obmat);
		invert_m4_m4(obi->obinvmat, obi->obmat);
	}
	if (flag & RE_OBJECT_INSTANCES_UPDATE_VIEW) {
		mul_m4_m4m4(obi->localtoviewmat, re->viewmat, obi->obmat);
		mul_m4_m4m4(obi->localtoviewinvmat, obi->obinvmat, re->viewinv);
	}
}

void RE_updateRenderInstances(Render *re, int flag)
{
	int i = 0;
	for (i = 0; i < re->totinstance; i++)
		RE_updateRenderInstance(re, &re->objectinstance[i], flag);
}

ObjectInstanceRen *RE_addRenderInstance(
        Render *re, ObjectRen *obr, Object *ob, Object *par,
        int index, int psysindex, float mat[4][4], int lay, const DupliObject *dob)
{
	ObjectInstanceRen *obi;
	float mat3[3][3];

	obi= MEM_callocN(sizeof(ObjectInstanceRen), "ObjectInstanceRen");
	obi->obr= obr;
	obi->ob= ob;
	obi->par= par;
	obi->index= index;
	obi->psysindex= psysindex;
	obi->lay= lay;

	/* Fill particle info */
	if (par && dob) {
		const ParticleSystem *psys = dob->particle_system;
		if (psys) {
			int part_index;
			if (obi->index < psys->totpart) {
				part_index = obi->index;
			}
			else if (psys->child) {
				part_index = psys->child[obi->index - psys->totpart].parent;
			}
			else {
				part_index = -1;
			}

			if (part_index >= 0) {
				const ParticleData *p = &psys->particles[part_index];
				obi->part_index = part_index;
				obi->part_size = p->size;
				obi->part_age = RE_GetStats(re)->cfra - p->time;
				obi->part_lifetime = p->lifetime;

				copy_v3_v3(obi->part_co, p->state.co);
				copy_v3_v3(obi->part_vel, p->state.vel);
				copy_v3_v3(obi->part_avel, p->state.ave);
			}
		}
	}

	/* Fill object info */
	if (dob) {
		obi->random_id = dob->random_id;
	}
	else {
		obi->random_id = BLI_hash_int_2d(BLI_hash_string(obi->ob->id.name + 2), 0);
	}

	RE_updateRenderInstance(re, obi, RE_OBJECT_INSTANCES_UPDATE_OBMAT | RE_OBJECT_INSTANCES_UPDATE_VIEW);

	if (mat) {
		copy_m4_m4(obi->mat, mat);
		copy_m3_m4(mat3, mat);
		invert_m3_m3(obi->nmat, mat3);
		transpose_m3(obi->nmat);
		obi->flag |= R_DUPLI_TRANSFORMED;
	}

	BLI_addtail(&re->instancetable, obi);

	return obi;
}

void RE_instance_get_particle_info(struct ObjectInstanceRen *obi, float *index, float *age, float *lifetime, float co[3], float *size, float vel[3], float angvel[3])
{
	*index = obi->part_index;
	*age = obi->part_age;
	*lifetime = obi->part_lifetime;
	copy_v3_v3(co, obi->part_co);
	*size = obi->part_size;
	copy_v3_v3(vel, obi->part_vel);
	copy_v3_v3(angvel, obi->part_avel);
}


void RE_makeRenderInstances(Render *re)
{
	ObjectInstanceRen *obi, *oldobi;
	ListBase newlist;
	int tot;

	/* convert list of object instances to an array for index based lookup */
	tot= BLI_listbase_count(&re->instancetable);
	re->objectinstance= MEM_callocN(sizeof(ObjectInstanceRen)*tot, "ObjectInstance");
	re->totinstance= tot;
	newlist.first= newlist.last= NULL;

	obi= re->objectinstance;
	for (oldobi=re->instancetable.first; oldobi; oldobi=oldobi->next) {
		*obi= *oldobi;

		if (obi->obr) {
			obi->prev= obi->next= NULL;
			BLI_addtail(&newlist, obi);
			obi++;
		}
		else
			re->totinstance--;
	}

	BLI_freelistN(&re->instancetable);
	re->instancetable= newlist;
}

/* four functions to facilitate envmap rotation for raytrace */
void RE_instance_rotate_ray_start(ObjectInstanceRen *obi, Isect *is)
{
	if (obi && (obi->flag & R_ENV_TRANSFORMED)) {
		copy_v3_v3(is->origstart, is->start);
		mul_m4_v3(obi->imat, is->start);
	}
}

void RE_instance_rotate_ray_dir(ObjectInstanceRen *obi, Isect *is)
{
	if (obi && (obi->flag & R_ENV_TRANSFORMED)) {
		float end[3];

		copy_v3_v3(is->origdir, is->dir);
		add_v3_v3v3(end, is->origstart, is->dir);

		mul_m4_v3(obi->imat, end);
		sub_v3_v3v3(is->dir, end, is->start);
	}
}

void RE_instance_rotate_ray(ObjectInstanceRen *obi, Isect *is)
{
	RE_instance_rotate_ray_start(obi, is);
	RE_instance_rotate_ray_dir(obi, is);
}

void RE_instance_rotate_ray_restore(ObjectInstanceRen *obi, Isect *is)
{
	if (obi && (obi->flag & R_ENV_TRANSFORMED)) {
		copy_v3_v3(is->start, is->origstart);
		copy_v3_v3(is->dir, is->origdir);
	}
}

int clip_render_object(float boundbox[2][3], float bounds[4], float winmat[4][4])
{
	float mat[4][4], vec[4];
	int a, fl, flag = -1;

	copy_m4_m4(mat, winmat);

	for (a=0; a < 8; a++) {
		vec[0]= (a & 1)? boundbox[0][0]: boundbox[1][0];
		vec[1]= (a & 2)? boundbox[0][1]: boundbox[1][1];
		vec[2]= (a & 4)? boundbox[0][2]: boundbox[1][2];
		vec[3]= 1.0;
		mul_m4_v4(mat, vec);

		fl = 0;
		if (bounds) {
			if      (vec[0] < bounds[0] * vec[3]) fl |= 1;
			else if (vec[0] > bounds[1] * vec[3]) fl |= 2;
			
			if      (vec[1] > bounds[3] * vec[3]) fl |= 4;
			else if (vec[1] < bounds[2] * vec[3]) fl |= 8;
		}
		else {
			if      (vec[0] < -vec[3]) fl |= 1;
			else if (vec[0] >  vec[3]) fl |= 2;
			
			if      (vec[1] >  vec[3]) fl |= 4;
			else if (vec[1] < -vec[3]) fl |= 8;
		}
		if      (vec[2] < -vec[3]) fl |= 16;
		else if (vec[2] >  vec[3]) fl |= 32;

		flag &= fl;
		if (flag == 0) {
			return 0;
		}
	}

	return flag;
}

