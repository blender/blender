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

#include "BLI_listbase.h"
#include "BLI_memarena.h"
#include "BLI_edgehash.h"
#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_utildefines.h"

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
} EdgeData;

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

BM_INLINE VertUser *edge_get_vuser(MemBase *b, EdgeData *edge, int ov)
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
	DerivedMesh *cddm = CDDM_copy(dm, 0);
	MEdge *medge;
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
	int i, j, curv, cure;
	float threshold = cos((emd->split_angle + 0.00001) * M_PI / 180.0);
	float no[3], edge_angle_cos;

	if (!cddm->numVertData || !cddm->numEdgeData)
		return cddm;

	membase = new_membase();

	etags = MEM_callocN(sizeof(EdgeData)*cddm->numEdgeData, "edgedata tag thingies");
	BLI_array_set_length(etags, cddm->numEdgeData);

	mvert = cddm->getVertArray(cddm);
	BLI_array_set_length(mvert, cddm->numVertData);
	medge = cddm->getEdgeArray(cddm);
	BLI_array_set_length(medge, cddm->numEdgeData);
	mloop = CustomData_get_layer(&cddm->loopData, CD_MLOOP);
	mpoly = CustomData_get_layer(&cddm->polyData, CD_MPOLY);

	for (i=0; i<cddm->numEdgeData; i++) {
		etags[i].v1 = medge[i].v1;
		etags[i].v2 = medge[i].v2;

		etags[i].tag = (medge[i].flag & ME_SHARP) != 0;

		etags[i].v1node.edge = etags+i;
		etags[i].v2node.edge = etags+i;
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
			if (etags[ml->e].tag)
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

			/*continue if previous edge is tagged*/
			if (etags[prevl->e].tag)
				continue;

			/*merge together adjacent split vert users*/
			if (edge_get_vuser(membase, etags+prevl->e, ml->v)
				!= edge_get_vuser(membase, etags+ml->e, ml->v))
			{
				vu = edge_get_vuser(membase, etags+prevl->e, ml->v);
				vu2 = edge_get_vuser(membase, etags+ml->e, ml->v);

				/*remove from vu2's users list and add to vu's*/
				for (e=edge_get_first(vu2); e; e=enext) {
					enext = edge_get_next(e, ml->v);
					edge_set_vuser(membase, e, ml->v, vu);
				}
			}
		}
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

	curv = cddm->numVertData;
	cure = cddm->numEdgeData;
	mp = mpoly;
	for (i=0; i<cddm->numPolyData; i++, mp++) {
		ml = mloop + mp->loopstart;
		for (j=0; j<mp->totloop; j++, ml++) {
			e = etags + ml->e;
			if (e->v1user && !e->v1user->done) {
				e->v1user->done = 1;
				BLI_array_growone(mvert);

				mvert[curv] = mvert[e->v1user->ov];
				e->v1user->v = curv;

				curv++;
			}

			if (e->v2user && !e->v2user->done) {
				e->v2user->done = 1;
				BLI_array_growone(mvert);

				mvert[curv] = mvert[e->v2user->ov];
				e->v2user->v = curv;

				curv++;
			}

			vu = edge_get_vuser(membase, e, ml->v);
			if (!vu)
				continue;
			ml->v = vu->v;

#if 0 //BMESH_TODO should really handle edges here, but for now use cddm_calc_edges
			/*ok, now we have to deal with edges. . .*/
			if (etags[ml->e].tag) {
				if (etags[ml->e].used) {
					BLI_array_growone(medge);
					BLI_array_growone(etags);
					medge[cure] = medge[ml->e];

					ml->e = cure;
					etags[cure].used = 1;
					cure++;
				}

				vu = etags[ml->e].v1user;
				vu2 = etags[ml->e].v2user;

				if (vu)
					medge[ml->e].v1 = vu->v;
				if (vu2)
					medge[ml->e].v2 = vu2->v;
			} else {
				etags[ml->e].used = 1;

				if (vu->ov == etags[ml->e].v1)
					medge[ml->e].v1 = vu->v;
				else if (vu->ov == etags[ml->e].v2)
					medge[ml->e].v2 = vu->v;
			}
#endif
		}
	}


	/*resize customdata arrays and add new medge/mvert arrays*/
	vdata = cddm->vertData;
	edata = cddm->edgeData;

	/*make sure we don't copy over mvert/medge layers*/
	CustomData_set_layer(&vdata, CD_MVERT, NULL);
	CustomData_set_layer(&edata, CD_MEDGE, NULL);
	CustomData_free_layer_active(&vdata, CD_MVERT, cddm->numVertData);
	CustomData_free_layer_active(&edata, CD_MEDGE, cddm->numEdgeData);

	memset(&cddm->vertData, 0, sizeof(CustomData));
	memset(&cddm->edgeData, 0, sizeof(CustomData));

	CustomData_copy(&vdata, &cddm->vertData, CD_MASK_DERIVEDMESH, CD_CALLOC, curv);
	CustomData_copy_data(&vdata, &cddm->vertData, 0, 0, cddm->numVertData);
	CustomData_free(&vdata, cddm->numVertData);
	cddm->numVertData = curv;

	CustomData_copy(&edata, &cddm->edgeData, CD_MASK_DERIVEDMESH, CD_CALLOC, cure);
	CustomData_copy_data(&edata, &cddm->edgeData, 0, 0, cddm->numEdgeData);
	CustomData_free(&edata, cddm->numEdgeData);
	cddm->numEdgeData = cure;

	CDDM_set_mvert(cddm, mvert);
	CDDM_set_medge(cddm, medge);

	free_membase(membase);
	MEM_freeN(etags);

	/*edge calculation isn't working correctly, so just brute force it*/
	cddm->numEdgeData = 0;
	CDDM_calc_edges_poly(cddm);

	cddm->numFaceData = mesh_recalcTesselation(&cddm->faceData,
		&cddm->loopData, &cddm->polyData,
		mvert, cddm->numFaceData,
		cddm->numLoopData, cddm->numPolyData, 1, 0);

	CDDM_set_mface(cddm, DM_get_tessface_data_layer(cddm, CD_MFACE));
	CDDM_calc_normals(cddm);

	return cddm;
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
					 Object *ob, DerivedMesh *dm)
{
	if(!(emd->flags & (MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG)))
		return dm;

	return doEdgeSplit(dm, emd);
}

static DerivedMesh *applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;
	EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;

	result = edgesplitModifier_do(emd, ob, derivedData);

	if(result != derivedData)
		CDDM_calc_normals(result);

	return result;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *ob,
									BMEditMesh *editData, DerivedMesh *derivedData)
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
	/* deformVerts */       0,
	/* deformMatrices */  0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  0,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    0,
	/* dependsOnTime */     0,
	/* dependsOnNormal */     0,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
