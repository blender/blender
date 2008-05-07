/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Deals with the first-level data in multires (edge flags, weights, and UVs)
 *
 * multires.h
 *
 */

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BIF_editmesh.h"

#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"

#include "BLI_editVert.h"

#include "MEM_guardedalloc.h"

#include "blendef.h"

#include <string.h>

MDeformVert *subdivide_dverts(MDeformVert *src, MultiresLevel *lvl);
MTFace *subdivide_mtfaces(MTFace *src, MultiresLevel *lvl);
void multires_update_edge_flags(Mesh *me, EditMesh *em);
void eed_to_medge_flag(EditEdge *eed, short *flag, char *crease);

/***********    Generic     ***********/

CustomDataMask cdmask(const int type)
{
	if(type == CD_MDEFORMVERT)
		return CD_MASK_MDEFORMVERT;
	else if(type == CD_MTFACE)
		return CD_MASK_MTFACE;
	return -1;
}

char type_ok(const int type)
{
	return (type == CD_MDEFORMVERT) || (type == CD_MTFACE);
}

/* Copy vdata or fdata from Mesh or EditMesh to Multires. */
void multires_update_customdata(MultiresLevel *lvl1, EditMesh *em, CustomData *src, CustomData *dst, const int type)
{
	if(src && dst && type_ok(type)) {
		const int tot= (type == CD_MDEFORMVERT ? lvl1->totvert : lvl1->totface);
		int i;
		
		CustomData_free(dst, tot);
		
		if(CustomData_has_layer(src, type)) {
			if(em) {
				EditVert *eve= G.editMesh->verts.first;
				EditFace *efa= G.editMesh->faces.first;
				CustomData_copy(src, dst, cdmask(type), CD_CALLOC, tot);
				for(i=0; i<tot; ++i) {
					if(type == CD_MDEFORMVERT) {
						CustomData_from_em_block(&G.editMesh->vdata, dst, eve->data, i);
						eve= eve->next;
					}
					else if(type == CD_MTFACE) {
						CustomData_from_em_block(&G.editMesh->fdata, dst, efa->data, i);
						efa= efa->next;
					}
				}
			}
			else
				CustomData_copy(src, dst, cdmask(type), CD_DUPLICATE, tot);
		}
	}
}

/* Uses subdivide_dverts or subdivide_mtfaces to subdivide src to match lvl_end. Does not free src. */
void *subdivide_customdata_to_level(void *src, MultiresLevel *lvl_start,
                                    MultiresLevel *lvl_end, const int type)
{
	if(src && lvl_start && lvl_end && type_ok(type)) {
		MultiresLevel *lvl;
		void *cr_data= NULL, *pr_data= NULL;
		
		pr_data= src;
		for(lvl= lvl_start; lvl && lvl != lvl_end; lvl= lvl->next) {
			if(type == CD_MDEFORMVERT)
				cr_data= subdivide_dverts(pr_data, lvl);
			else if(type == CD_MTFACE)
				cr_data= subdivide_mtfaces(pr_data, lvl);
			
			/* Free previous subdivision level's data */
			if(lvl != lvl_start) {
				if(type == CD_MDEFORMVERT)
					free_dverts(pr_data, lvl->totvert);
				else if(type == CD_MTFACE)
					MEM_freeN(pr_data);
			}

			pr_data= cr_data;
			cr_data= NULL;
		}
		
		return pr_data;
	}
	
	return NULL;
}

/* Directly copy src into dst (handles both Mesh and EditMesh) */
void customdata_to_mesh(Mesh *me, EditMesh *em, CustomData *src, CustomData *dst, const int tot, const int type)
{
	if(me && me->mr && src && dst && type_ok(type)) {
		if(em) {
			int i;
			EditVert *eve= em->verts.first;
			EditFace *efa= em->faces.first;
			CustomData_copy(src, dst, cdmask(type), CD_CALLOC, 0);
			
			for(i=0; i<tot; ++i) {
				if(type == CD_MDEFORMVERT) {
					CustomData_to_em_block(src, dst, i, &eve->data);
					eve= eve->next;
				}
				else if(type == CD_MTFACE) {
					CustomData_to_em_block(src, dst, i, &efa->data);
					efa= efa->next;
				}
			}
		} else {
			CustomData_merge(src, dst, cdmask(type), CD_DUPLICATE, tot);
		}
	}
}

/* Subdivide vdata or fdata from Multires into either Mesh or EditMesh. */
void multires_customdata_to_mesh(Mesh *me, EditMesh *em, MultiresLevel *lvl, CustomData *src,
                                 CustomData *dst, const int type)
{	
	if(me && me->mr && lvl && src && dst && type_ok(type) &&
	   CustomData_has_layer(src, type)) {
		const int tot= (type == CD_MDEFORMVERT ? lvl->totvert : lvl->totface);
		if(lvl == me->mr->levels.first) {
			customdata_to_mesh(me, em, src, dst, tot, type);
		}
		else {
			CustomData cdf;
			const int count = CustomData_number_of_layers(src, type);
			int i;
			
			/* Construct a new CustomData containing the subdivided data */
			CustomData_copy(src, &cdf, cdmask(type), CD_ASSIGN, tot);
			for(i=0; i<count; ++i) {
				void *layer= CustomData_get_layer_n(&cdf, type, i);
				CustomData_set_layer_n(&cdf, type, i,
					subdivide_customdata_to_level(layer, me->mr->levels.first, lvl, type));
			}
			
			customdata_to_mesh(me, em, &cdf, dst, tot, type);
			CustomData_free(&cdf, tot);
		}
	}
}

/* Subdivide the first-level customdata up to cr_lvl, then delete the original data */
void multires_del_lower_customdata(Multires *mr, MultiresLevel *cr_lvl)
{
	MultiresLevel *lvl1= mr->levels.first;
	MDeformVert *dverts= NULL;
	CustomData cdf;
	int i;

	/* dverts */
	dverts= subdivide_customdata_to_level(CustomData_get(&mr->vdata, 0, CD_MDEFORMVERT),
	                                      lvl1, cr_lvl, CD_MDEFORMVERT);
	if(dverts) {
		CustomData_free_layers(&mr->vdata, CD_MDEFORMVERT, lvl1->totvert);
		CustomData_add_layer(&mr->vdata, CD_MDEFORMVERT, CD_ASSIGN, dverts, cr_lvl->totvert);
	}
	
	/* mtfaces */
	CustomData_copy(&mr->fdata, &cdf, CD_MASK_MTFACE, CD_ASSIGN, cr_lvl->totface);
	for(i=0; i<CustomData_number_of_layers(&mr->fdata, CD_MTFACE); ++i) {
		MTFace *mtfaces=
			subdivide_customdata_to_level(CustomData_get_layer_n(&mr->fdata, CD_MTFACE, i),
			                              lvl1, cr_lvl, CD_MTFACE);
		if(mtfaces)
			CustomData_set_layer_n(&cdf, CD_MTFACE, i, mtfaces);
	}
	
	CustomData_free(&mr->fdata, lvl1->totface);
	mr->fdata= cdf;
}

/* Update all special first-level data, if the first-level is active */
void multires_update_first_level(Mesh *me, EditMesh *em)
{
	if(me && me->mr && me->mr->current == 1) {
		multires_update_customdata(me->mr->levels.first, em, em ? &em->vdata : &me->vdata,
		                           &me->mr->vdata, CD_MDEFORMVERT);
		multires_update_customdata(me->mr->levels.first, em, em ? &em->fdata : &me->fdata,
		                           &me->mr->fdata, CD_MTFACE);
		multires_update_edge_flags(me, em);
	}
}

/*********** Multires.edge_flags ***********/
void multires_update_edge_flags(Mesh *me, EditMesh *em)
{
	MultiresLevel *lvl= me->mr->levels.first;
	EditEdge *eed= NULL;
	int i;
	
	if(em) eed= em->edges.first;
	for(i=0; i<lvl->totedge; ++i) {
		if(em) {
			me->mr->edge_flags[i]= 0;
			eed_to_medge_flag(eed, &me->mr->edge_flags[i], &me->mr->edge_creases[i]);
			eed= eed->next;
		}
		else {
			me->mr->edge_flags[i]= me->medge[i].flag;
			me->mr->edge_creases[i]= me->medge[i].crease;
		}
	}
}



/*********** Multires.vdata ***********/

/* MDeformVert */

/* Add each weight from in to out. Scale each weight by w. */
void multires_add_dvert(MDeformVert *out, const MDeformVert *in, const float w)
{
	if(out && in) {
		int i, j;
		char found;

		for(i=0; i<in->totweight; ++i) {
			found= 0;
			for(j=0; j<out->totweight; ++j) {
				if(out->dw[j].def_nr==in->dw[i].def_nr) {
					out->dw[j].weight += in->dw[i].weight * w;
					found= 1;
				}
			}
			if(!found) {
				MDeformWeight *newdw= MEM_callocN(sizeof(MDeformWeight)*(out->totweight+1),
				                                  "multires dvert");
				if(out->dw) {
					memcpy(newdw, out->dw, sizeof(MDeformWeight)*out->totweight);
					MEM_freeN(out->dw);
				}

				out->dw= newdw;
				out->dw[out->totweight].weight= in->dw[i].weight * w;
				out->dw[out->totweight].def_nr= in->dw[i].def_nr;

				++out->totweight;
			}
		}
	}
}

/* Takes an input array of dverts and subdivides them (linear) using the topology of lvl */
MDeformVert *subdivide_dverts(MDeformVert *src, MultiresLevel *lvl)
{
	if(lvl && lvl->next) {
		MDeformVert *out = MEM_callocN(sizeof(MDeformVert)*lvl->next->totvert, "dvert prop array");
		int i, j;
		
		/* Copy lower level */
		for(i=0; i<lvl->totvert; ++i)
			multires_add_dvert(&out[i], &src[i], 1);
		/* Edge verts */
		for(i=0; i<lvl->totedge; ++i) {
			for(j=0; j<2; ++j)
			multires_add_dvert(&out[lvl->totvert+i], &src[lvl->edges[i].v[j]],0.5);
		}
		
		/* Face verts */
		for(i=0; i<lvl->totface; ++i) {
			for(j=0; j<(lvl->faces[i].v[3]?4:3); ++j)
				multires_add_dvert(&out[lvl->totvert + lvl->totedge + i],
				                   &src[lvl->faces[i].v[j]],
				                   lvl->faces[i].v[3]?0.25:(1.0f/3.0f));
		}
		
		return out;
	}
	
	return NULL;
}



/*********** Multires.fdata ***********/

/* MTFace */

void multires_uv_avg2(float out[2], const float a[2], const float b[2])
{
	int i;
	for(i=0; i<2; ++i)
		out[i] = (a[i] + b[i]) / 2.0f;
}

/* Takes an input array of mtfaces and subdivides them (linear) using the topology of lvl */
MTFace *subdivide_mtfaces(MTFace *src, MultiresLevel *lvl)
{
	if(lvl && lvl->next) {
		MTFace *out= MEM_callocN(sizeof(MultiresColFace)*lvl->next->totface,"Multirescolfaces");
		int i, j, curf;
		
		for(i=0, curf=0; i<lvl->totface; ++i) {
			const char sides= lvl->faces[i].v[3]?4:3;
			float cntr[2]= {0, 0};

			/* Find average uv coord of the current face */
			for(j=0; j<sides; ++j) {
				cntr[0]+= src[i].uv[j][0];
				cntr[1]+= src[i].uv[j][1];
			}
			cntr[0]/= sides;
			cntr[1]/= sides;

			for(j=0; j<sides; ++j, ++curf) {
				out[curf]= src[i];
			
				multires_uv_avg2(out[curf].uv[0], src[i].uv[j], src[i].uv[j==0?sides-1:j-1]);
						  
				out[curf].uv[1][0]= src[i].uv[j][0];
				out[curf].uv[1][1]= src[i].uv[j][1];
				
				multires_uv_avg2(out[curf].uv[2], src[i].uv[j], src[i].uv[j==sides-1?0:j+1]);
						  
				out[curf].uv[3][0]= cntr[0];
				out[curf].uv[3][1]= cntr[1];
			}
		}
		
		return out;
	}
	
	return NULL;
}

void multires_delete_layer(Mesh *me, CustomData *cd, const int type, int n)
{
	if(me && me->mr && cd) {
		MultiresLevel *lvl1= me->mr->levels.first;
		
		multires_update_levels(me, 0);
		
		CustomData_set_layer_active(cd, type, n);
		CustomData_free_layer_active(cd, type, lvl1->totface);
		
		multires_level_to_mesh(OBACT, me, 0);
	}
}

void multires_add_layer(Mesh *me, CustomData *cd, const int type, const int n)
{
	if(me && me->mr && cd) {
		multires_update_levels(me, 0);
	
		if(CustomData_has_layer(cd, type))
			CustomData_add_layer(cd, type, CD_DUPLICATE, CustomData_get_layer(cd, type),
			                     current_level(me->mr)->totface);
		else
			CustomData_add_layer(cd, type, CD_DEFAULT, NULL, current_level(me->mr)->totface);

		CustomData_set_layer_active(cd, type, n);
		multires_level_to_mesh(OBACT, me, 0);
	}
}
