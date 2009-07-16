/**
 * BME_interp.c    August 2008
 *
 *	BM interpolation functions.
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h" 
#include "BKE_utildefines.h"

#include "bmesh.h"
#include "bmesh_private.h"

/*
 * BME_INTERP.C
 *
 * Functions for interpolating data across the surface of a mesh.
 *
*/

/**
 *			bmesh_data_interp_from_verts
 *
 *  Interpolates per-vertex data from two sources to a target.
 * 
 *  Returns -
 *	Nothing
 */
void BM_Data_Interp_From_Verts(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v, float fac)
{
	void *src[2];
	float w[2];
	if (v1->head.data && v2->head.data) {
		src[0]= v1->head.data;
		src[1]= v2->head.data;
		w[0] = 1.0f-fac;
		w[1] = fac;
		CustomData_bmesh_interp(&bm->vdata, src, w, NULL, 2, v->head.data);
	}
}

/**
 *			bmesh_data_facevert_edgeinterp
 *
 *  Walks around the faces of an edge and interpolates the per-face-edge
 *  data between two sources to a target.
 * 
 *  Returns -
 *	Nothing
*/
 
void BM_Data_Facevert_Edgeinterp(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v, BMEdge *e1, float fac){
	void *src[2];
	float w[2];
	BMLoop *l=NULL, *v1loop = NULL, *vloop = NULL, *v2loop = NULL;
	
	w[1] = 1.0f - fac;
	w[0] = fac;

	if(!e1->loop) return;
	l = e1->loop;
	do{
		if(l->v == v1){ 
			v1loop = l;
			vloop = (BMLoop*)(v1loop->head.next);
			v2loop = (BMLoop*)(vloop->head.next);
		}else if(l->v == v){
			v1loop = (BMLoop*)(l->head.next);
			vloop = l;
			v2loop = (BMLoop*)(l->head.prev);
			
		}

		src[0] = v1loop->head.data;
		src[1] = v2loop->head.data;					

		CustomData_bmesh_interp(&bm->ldata, src,w, NULL, 2, vloop->head.data); 				
		l = l->radial.next->data;
	}while(l!=e1->loop);
}

void BM_loops_to_corners(BMesh *bm, Mesh *me, int findex,
                         BMFace *f, int numTex, int numCol) 
{
	BMLoop *l;
	BMIter iter;
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;
	int i, j;

	for(i=0; i < numTex; i++){
		texface = CustomData_get_n(&me->fdata, CD_MTFACE, findex, i);
		texpoly = CustomData_bmesh_get_n(&bm->pdata, f->head.data, CD_MTEXPOLY, i);
		
		texface->tpage = texpoly->tpage;
		texface->flag = texpoly->flag;
		texface->transp = texpoly->transp;
		texface->mode = texpoly->mode;
		texface->tile = texpoly->tile;
		texface->unwrap = texpoly->unwrap;

		j = 0;
		BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
			mloopuv = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPUV, i);
			texface->uv[j][0] = mloopuv->uv[0];
			texface->uv[j][1] = mloopuv->uv[1];

			j++;
		}

	}

	for(i=0; i < numCol; i++){
		mcol = CustomData_get_n(&me->fdata, CD_MCOL, findex, i);

		j = 0;
		BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
			mloopcol = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPCOL, i);
			mcol[j].r = mloopcol->r;
			mcol[j].g = mloopcol->g;
			mcol[j].b = mloopcol->b;
			mcol[j].a = mloopcol->a;

			j++;
		}
	}
}

//static void bmesh_data_interp_from_face(BME_Mesh *bm, BMFace *source, BMFace *target)
//{
//
//}
/*insert BM_data_interp_from_face here for mean value coordinates...*/


static void update_data_blocks(BMesh *bm, CustomData *olddata, CustomData *data)
{
	BMIter iter;
	BLI_mempool *oldpool = olddata->pool;
	void *block;

	CustomData_bmesh_init_pool(data, data==&bm->ldata ? 2048 : 512);

	if (data == &bm->vdata) {
		BMVert *eve;
		
		BM_ITER(eve, &iter, bm, BM_VERTS_OF_MESH, NULL) {
			block = NULL;
			CustomData_bmesh_set_default(data, &block);
			CustomData_bmesh_copy_data(olddata, data, eve->head.data, &block);
			CustomData_bmesh_free_block(olddata, &eve->head.data);
			eve->head.data= block;
		}
	}
	else if (data == &bm->edata) {
		BMEdge *eed;

		BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			block = NULL;
			CustomData_bmesh_set_default(data, &block);
			CustomData_bmesh_copy_data(olddata, data, eed->head.data, &block);
			CustomData_bmesh_free_block(olddata, &eed->head.data);
			eed->head.data= block;
		}
	}
	else if (data == &bm->pdata || data == &bm->ldata) {
		BMIter liter;
		BMFace *efa;
		BMLoop *l;

		BM_ITER(efa, &iter, bm, BM_FACES_OF_MESH, NULL) {
			if (data == &bm->pdata) {
				block = NULL;
				CustomData_bmesh_set_default(data, &block);
				CustomData_bmesh_copy_data(olddata, data, efa->head.data, &block);
				CustomData_bmesh_free_block(olddata, &efa->head.data);
				efa->head.data= block;
			}

			if (data == &bm->ldata) {
				BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, efa) {
					block = NULL;
					CustomData_bmesh_set_default(data, &block);
					CustomData_bmesh_copy_data(olddata, data, l->head.data, &block);
					CustomData_bmesh_free_block(olddata, &l->head.data);
					l->head.data= block;
				}
			}
		}
	}
}


void BM_add_data_layer(BMesh *bm, CustomData *data, int type)
{
	CustomData olddata;

	olddata= *data;
	olddata.layers= (olddata.layers)? MEM_dupallocN(olddata.layers): NULL;
	CustomData_add_layer(data, type, CD_CALLOC, NULL, 0);

	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

void BM_free_data_layer(BMesh *bm, CustomData *data, int type)
{
	CustomData olddata;

	olddata= *data;
	olddata.layers= (olddata.layers)? MEM_dupallocN(olddata.layers): NULL;
	CustomData_free_layer_active(data, type, 0);

	update_data_blocks(bm, &olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}