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
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2005 by the Blender Foundation.
* All rights reserved.
*
* Contributor(s): Joseph Eagar
*
* ***** END GPL LICENSE BLOCK *****
*
*/

/* EdgeSplit modifier: Splits edges in the mesh according to sharpness flag
 * or edge angle (can be used to achieve autosmoothing) */

#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_memarena.h"
#include "BLI_edgehash.h"
#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_smallhash.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_tessmesh.h"
#include "BKE_mesh.h"

#include "MEM_guardedalloc.h"

/* EdgeSplit */
/* EdgeSplit modifier: Splits edges in the mesh according to sharpness flag
 * or edge angle (can be used to achieve autosmoothing)
*/

/*new cddm-based edge split code*/
typedef struct VertUser {
	int ov, v, done;
	ListBase users;
} VertUser;

typedef struct EdgeNode {
	struct EdgeNode *next, *prev;
	struct EdgeData *edge;
} EdgeNode;

typedef struct EdgeData {
	EdgeNode v1node, v2node;
	VertUser *v1user, *v2user;
	float fno[3]; /*used to calculate face angles*/
	int has_fno;
	int tag;
	int v1, v2;
	int used;
	int idx;
	int ml1, ml2, mp1, mp2;
} EdgeData;

typedef struct LoopPair {
	int ml1, ml2, mp1, mp2;
} LoopPair;

typedef struct MemBase {
	BLI_mempool *vertuserpool;
} MemBase;

BM_INLINE EdgeData *edge_get_next(EdgeData *e, int ov) {
	if (ov == e->v1)
		return e->v1node.next ? e->v1node.next->edge : NULL;
	else return e->v2node.next ? e->v2node.next->edge : NULL;
}

BM_INLINE EdgeNode *edge_get_node(EdgeData *e, int ov)
{
	if (ov == e->v1)
		return &e->v1node;
	else return &e->v2node;
		return NULL;
}

BM_INLINE VertUser *edge_get_vuser(MemBase *UNUSED(b), EdgeData *edge, int ov)
{
	if (ov == edge->v1)
		return edge->v1user;
	else if (ov == edge->v2)
		return edge->v2user;
	else {
		printf("yeek!!\n");
		return NULL;
	}
}

BM_INLINE void edge_set_vuser(MemBase *b, EdgeData *e, int ov, VertUser *vu)

{
	VertUser *olduser = edge_get_vuser(b, e, ov);

	if (vu == olduser)
		return;

	if (olduser)
		BLI_remlink(&olduser->users, ov==e->v1 ? &e->v1node : &e->v2node);
	BLI_addtail(&vu->users, ov==e->v1 ? &e->v1node : &e->v2node);

	if (ov == e->v1)
		e->v1user = vu;
	else e->v2user = vu;
}

BM_INLINE VertUser *new_vuser(MemBase *base)
{
	VertUser *vusr = BLI_mempool_calloc(base->vertuserpool);

	return vusr;
}

BM_INLINE MemBase *new_membase(void)
{
	MemBase *b = MEM_callocN(sizeof(MemBase), "MemBase for edgesplit in modifier.c");
	b->vertuserpool = BLI_mempool_create(sizeof(VertUser), 1, 2048, 1, 0);

	return b;
}

BM_INLINE void free_membase(MemBase *b)
{
	BLI_mempool_destroy(b->vertuserpool);
	MEM_freeN(b);
}

BM_INLINE EdgeData *edge_get_first(VertUser *vu)
{
	return vu->users.first ? ((EdgeNode*)vu->users.first)->edge : NULL;
}

DerivedMesh *doEdgeSplit(DerivedMesh *dm, EdgeSplitModifierData *emd)
{
	DerivedMesh *cddm = CDDM_copy(dm, 0), *cddm2;
	MEdge *medge = NULL;
	BLI_array_declare(stack);
	BLI_array_declare(medge);
	MLoop *mloop, *ml, *prevl;
	MPoly *mpoly, *mp;
	MVert *mvert;
	BLI_array_declare(mvert);
	EdgeData *etags, *e, *enext;
	BLI_array_declare(etags);
	VertUser *vu, *vu2;
	MemBase *membase;
	CustomData edata, vdata;
	int *origv = NULL, *orige = NULL;
	BLI_array_declare(origv);
	BLI_array_declare(orige);
	EdgeHash *eh, *pairh;
	float threshold = cos((emd->split_angle + 0.00001) * M_PI / 180.0);
	float no[3], edge_angle_cos;
	LoopPair *pairs = NULL, *lp;
	BLI_array_declare(pairs);
	int i, j, curv, cure;

	if (!cddm->numVertData || !cddm->numEdgeData)
		return cddm;
	
	eh = BLI_edgehash_new();
	pairh = BLI_edgehash_new();
	membase = new_membase();

	etags = MEM_callocN(sizeof(EdgeData)*cddm->numEdgeData, "edgedata tag thingies");
	BLI_array_set_length(etags, cddm->numEdgeData);

	mvert = cddm->dupVertArray(cddm);
	BLI_array_set_length(mvert, cddm->numVertData);
	medge = cddm->dupEdgeArray(cddm);
	BLI_array_set_length(medge, cddm->numEdgeData);
	
	mloop = CDDM_get_loops(cddm);
	mpoly = CDDM_get_polys(cddm);

	for (i=0; i<cddm->numEdgeData; i++) {
		etags[i].v1 = medge[i].v1;
		etags[i].v2 = medge[i].v2;

		etags[i].tag = (medge[i].flag & ME_SHARP) != 0;

		etags[i].v1node.edge = etags+i;
		etags[i].v2node.edge = etags+i;
		etags[i].idx = i;
		
		BLI_edgehash_insert(eh, medge[i].v1, medge[i].v2, SET_INT_IN_POINTER(i));
		BLI_array_append(orige, i);
	}
	
	for (i=0; i<cddm->numVertData; i++) {
		BLI_array_append(origv, i);
	}

	if (emd->flags & MOD_EDGESPLIT_FROMANGLE) {
		mp = mpoly;
		for (i=0; i<cddm->numPolyData; i++, mp++) {
			mesh_calc_poly_normal(mp, mloop+mp->loopstart, mvert, no);

			ml = mloop + mp->loopstart;
			for (j=0; j<mp->totloop; j++, ml++) {
				if (!etags[ml->e].has_fno) {
					VECCOPY(etags[ml->e].fno, no);
					etags[ml->e].has_fno = 1;
				} else if (!etags[ml->e].tag) {
					edge_angle_cos = INPR(etags[ml->e].fno, no);
					if (edge_angle_cos < threshold) {
						etags[ml->e].tag = 1;
					}
				}
			}
		}
	}
	
	mp = mpoly;
	for (i=0; i<cddm->numPolyData; i++, mp++) {
		ml = mloop + mp->loopstart;
		for (j=0; j<mp->totloop; j++, ml++) {
			/*create loop pairs*/
			e = etags + ml->e;
			if (!e->mp1) {
				e->mp1 = i+1;
				e->ml1 = mp->loopstart+j+1;
			} else if (!e->mp2) {
				e->mp2 = i+1;
				e->ml2 = mp->loopstart+j+1;
			}
			
			if (e->tag)
				continue;

			prevl = mloop + mp->loopstart + ((j-1)+mp->totloop) % mp->totloop;

			if (!edge_get_vuser(membase, etags+prevl->e, ml->v)) {
				vu = new_vuser(membase);
				vu->ov = vu->v = ml->v;
				edge_set_vuser(membase, etags+prevl->e, ml->v, vu);
			}

			if (!edge_get_vuser(membase, etags+ml->e, ml->v)) {
				vu = new_vuser(membase);
				vu->ov = vu->v = ml->v;
				edge_set_vuser(membase, etags+ml->e, ml->v, vu);
			}
		}
	}
	
	/*build list of loop pairs*/
	for (i=0; i<cddm->numEdgeData; i++) {
		e = etags + i;
		if (e->tag && e->mp2) {
			BLI_array_growone(pairs);
			lp = pairs + BLI_array_count(pairs)-1;
			lp->mp1 = e->mp1-1; lp->ml1 = e->ml1-1;
			lp->mp2 = e->mp2-1; lp->ml2 = e->ml2-1;
		}
	}
	
	/*find contiguous face regions*/
	while (1) {
		int ok = 1;
		
		lp = pairs;
		for (i=0; i<BLI_array_count(pairs); i++, lp++) {
			MLoop *ml2;
			MPoly *mp2;
			EdgeData *e2;
			int k, lastj;

			j = lp->ml1;
			k = lp->mp1;
			ml = mloop + lp->ml1;
			mp = mp2 = mpoly + lp->mp1;

			/*walk edges around ml->v*/
			do {
				lastj = j;
				if (mloop[j].v == ml->v)
					j = mp2->loopstart + (j-mp2->loopstart-1 + mp2->totloop)%mp2->totloop;
				else
					j = mp2->loopstart + (j-mp2->loopstart+1)%mp2->totloop;
				
				prevl = mloop + j;
				e2 = etags + prevl->e;
				
				if (!e2->tag && e2->mp1 && e2->mp2) {
					j = e2->ml1-1 == j ? e2->ml2-1 : e2->ml1-1;
					k = e2->mp1-1 == k ? e2->mp2-1 : e2->mp1-1;
				} else
					break;
				
				mp2 = mpoly + k;
			} while (!etags[j].tag && (mloop+j) != ml && j != lastj);
			
			ml2 = mloop + j;
			e = etags + ml->e;
			e2 = etags + ml2->e;
			if (e2->tag && e != e2 && (e->mp1-1==lp->mp1) != (e2->mp1-1 == k)) {
				SWAP(int, e->ml1, e->ml2);
				SWAP(int, e->mp1, e->mp2);
				SWAP(int, lp->ml1, lp->ml2);
				SWAP(int, lp->mp1, lp->mp2);
				ok = 0;
			}
		}
		
		if (ok)
			break;
	}
	
	mp = mpoly;
	for (i=0; i<cddm->numPolyData; i++, mp++) {
		ml = mloop + mp->loopstart;
		for (j=0; j<mp->totloop; j++, ml++) {
			if (!etags[ml->e].tag)
				continue;
			
			prevl = mloop + mp->loopstart + ((j-1)+mp->totloop) % mp->totloop;

			if (!etags[prevl->e].tag) {
				vu = edge_get_vuser(membase, etags+prevl->e, ml->v);
				if (!vu) {
					vu = new_vuser(membase);
					vu->ov = vu->v = ml->v;
					edge_set_vuser(membase, etags+prevl->e, ml->v, vu);
				}

				edge_set_vuser(membase, etags+ml->e, ml->v, vu);
			} else {
				vu = new_vuser(membase);
				vu->ov = vu->v = ml->v;
				edge_set_vuser(membase, etags+ml->e, ml->v, vu);
			}
		}
	}
	
	mp = mpoly;
	for (i=0; i<cddm->numPolyData; i++, mp++) {
		ml = mloop + mp->loopstart;
		for (j=0; j<mp->totloop; j++, ml++) {
			prevl = mloop + mp->loopstart + ((j-1)+mp->totloop) % mp->totloop;
			
			/*merge together adjacent split vert users*/
			if (edge_get_vuser(membase, etags+prevl->e, ml->v)
				!= edge_get_vuser(membase, etags+ml->e, ml->v))
			{
				vu = edge_get_vuser(membase, etags+prevl->e, ml->v);
				vu2 = edge_get_vuser(membase, etags+ml->e, ml->v);
				
				if (!vu) {
					edge_set_vuser(membase, etags+prevl->e, ml->v, vu2);
				} else if (!vu2) {
					edge_set_vuser(membase, etags+ml->e, ml->v, vu);
				} else {
					/*remove from vu2's users list and add to vu's*/
					for (e=edge_get_first(vu2); e; e=enext) {
						enext = edge_get_next(e, ml->v);
						
						if (e == etags+prevl->e)
							continue;
						
						edge_set_vuser(membase, e, ml->v, vu);
					}
					
					edge_set_vuser(membase, etags+ml->e, ml->v, vu);
				}
			}
		}
	}
	
	mp = mpoly;
	for (i=0; i<cddm->numPolyData; i++, mp++) {
		ml = mloop + mp->loopstart;
		for (j=0; j<mp->totloop; j++, ml++) {
			int tot_tag = 0;
			
			vu = edge_get_vuser(membase, etags+ml->e, ml->v);
			if (!vu)
				continue;
			
			for (e=edge_get_first(vu); e; e=edge_get_next(e, ml->v)) {
				tot_tag += e->tag;	
			}
			
			if (tot_tag < 2) {
				vu->done = 1;
			}
		}
	}
	
	curv = cddm->numVertData;
	cure = cddm->numEdgeData;

	mp = mpoly;
	for (i=0; i<cddm->numPolyData; i++, mp++) {
		ml = mloop + mp->loopstart;
		for (j=0; j<mp->totloop; j++, ml++) {
			int v1, v2;
			
			e = etags + ml->e;
			
			if (e->v1user && !e->v1user->done) {
				e->v1user->done = 1;
				BLI_array_growone(mvert);
				BLI_array_append(origv, e->v1user->ov);

				mvert[curv] = mvert[e->v1user->ov];
				e->v1user->v = curv;

				curv++;
			}

			if (e->v2user && !e->v2user->done) {
				e->v2user->done = 1;
				BLI_array_growone(mvert);
				BLI_array_append(origv, e->v2user->ov);
				BLI_array_growone(etags);
				etags[BLI_array_count(etags)-1].idx = BLI_array_count(etags)-1;

				mvert[curv] = mvert[e->v2user->ov];
				e->v2user->v = curv;

				curv++;
			}
		}
	}
	
	mp = mpoly;
	for (i=0; i<cddm->numPolyData; i++, mp++) {
		ml = mloop + mp->loopstart;
		for (j=0; j<mp->totloop; j++, ml++) {
			MLoop *nextl = mloop + mp->loopstart + (j+1)%mp->totloop;
			
			e = etags + ml->e;
			vu = edge_get_vuser(membase, e, ml->v);
			vu2 = edge_get_vuser(membase, e, nextl->v);
			
			if (!vu || etags[ml->e].tag)
				continue;
			
			/*don't duplicate cross edges*/
			if ((vu->v!=vu->ov) + (vu2->v!=vu2->ov) == 1) {
				if (vu->v != vu->ov) {
					if (vu->ov == medge[ml->e].v1) {
						medge[ml->e].v1 = vu->v;
					} else if (vu->ov == medge[ml->e].v2) {
						medge[ml->e].v2 = vu->v;
					}
				} else if (vu2->v != vu2->ov) {
					if (vu2->ov == medge[ml->e].v1) {
						medge[ml->e].v1 = vu2->v;
					} else if (vu2->ov == medge[ml->e].v2) {
						medge[ml->e].v2 = vu2->v;
					}
				}
				
				BLI_edgehash_insert(eh, medge[ml->e].v1, medge[ml->e].v2, SET_INT_IN_POINTER(ml->e));
			}
		}
	}
	
	mp = mpoly;
	for (i=0; i<cddm->numPolyData; i++, mp++) {
		for (j=0; j<mp->totloop; j++) {
			int v1, v2, k;
			
			ml = mloop + mp->loopstart + j;
			e = etags + ml->e;
			
			if (!e->tag || j+mp->loopstart != e->ml1-1)
				continue;
			
			for (k=0; k<2; k++) {
				if (k) {
					ml = mloop + mp->loopstart + (j+1)%mp->totloop;
				}
				
				e = etags + ml->e;
				
				v1 = e->v1user ? e->v1user->v : e->v1;
				v2 = e->v2user ? e->v2user->v : e->v2;
				
				if (!BLI_edgehash_haskey(eh, v1, v2)) {
					BLI_array_growone(medge);
					BLI_array_append(orige, ml->e);
					
					medge[cure] = medge[ml->e];
					medge[cure].v1 = v1;
					medge[cure].v2 = v2;
					
					BLI_edgehash_insert(eh, v1, v2, SET_INT_IN_POINTER(cure));
					cure++;
				}
	
				ml->e = GET_INT_FROM_POINTER(BLI_edgehash_lookup(eh, v1, v2));
							
				vu = edge_get_vuser(membase, e, ml->v);
				ml->v = vu->v;
			}
		}
	}
	
	cddm2 = CDDM_from_template(cddm, curv, cure, 0, cddm->numLoopData, cddm->numPolyData);

	/*copy vert/edge data*/	
	for (i=0; i<curv; i++) {
		DM_copy_vert_data(cddm, cddm2, origv[i], i, 1);
	}
	
	for (i=0; i<cure; i++) {
		DM_copy_edge_data(cddm, cddm2, orige[i], i, 1);
	}
	
	/*copy loop/face data*/
	CustomData_copy_data(&cddm->loopData, &cddm2->loopData, 0, 0, cddm->numLoopData);
	CustomData_copy_data(&cddm->polyData, &cddm2->polyData, 0, 0, cddm->numPolyData);
	
	CustomData_free_layer_active(&cddm2->vertData, CD_MVERT, cddm2->numVertData);
	CustomData_free_layer_active(&cddm2->edgeData, CD_MEDGE, cddm2->numEdgeData);
	
	/*set new mvert/medge layers*/
	CustomData_set_layer(&cddm2->vertData, CD_MVERT, mvert);
	CustomData_set_layer(&cddm2->edgeData, CD_MEDGE, medge);
	CDDM_set_mvert(cddm2, mvert);
	CDDM_set_medge(cddm2, medge);
	
	/*warning fixes*/
	BLI_array_fake_user(mvert);
	BLI_array_fake_user(medge);

	free_membase(membase);
	BLI_array_free(etags);
	BLI_array_free(origv);
	BLI_array_free(orige);
	BLI_array_free(pairs);
	BLI_edgehash_free(eh, NULL);
	BLI_edgehash_free(pairh, NULL);
	
	CDDM_calc_normals(cddm2);
	
	if (cddm != dm) {
		cddm->needsFree = 1;
		cddm->release(cddm);
	}
	
	CDDM_calc_edges_poly(cddm2);
	
	return cddm2;
}

static void initData(ModifierData *md)
{
	EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;

	/* default to 30-degree split angle, sharpness from both angle & flag
	*/
	emd->split_angle = 30;
	emd->flags = MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;
	EdgeSplitModifierData *temd = (EdgeSplitModifierData*) target;

	temd->split_angle = emd->split_angle;
	temd->flags = emd->flags;
}

static DerivedMesh *edgesplitModifier_do(EdgeSplitModifierData *emd,
					 Object *UNUSED(ob), DerivedMesh *dm)
{
	if(!(emd->flags & (MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG)))
		return dm;

	return doEdgeSplit(dm, emd);
}

static DerivedMesh *applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
		int UNUSED(useRenderParams), int UNUSED(isFinalCalc))
{
	DerivedMesh *result;
	EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;

	result = edgesplitModifier_do(emd, ob, derivedData);

	if(result != derivedData)
		CDDM_calc_normals(result);

	return result;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *ob,
									BMEditMesh *UNUSED(editData), DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}


ModifierTypeInfo modifierType_EdgeSplit = {
	/* name */              "EdgeSplit",
	/* structName */        "EdgeSplitModifierData",
	/* structSize */        sizeof(EdgeSplitModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsMapping
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
};
