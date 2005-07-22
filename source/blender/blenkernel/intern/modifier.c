#include "string.h"

#include "BLI_blenlib.h"
#include "BLI_rand.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "BLI_editVert.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_modifier.h"
#include "BKE_lattice.h"
#include "BKE_subsurf.h"
#include "BKE_object.h"
#include "BKE_mesh.h"
#include "depsgraph_private.h"

#include "CCGSubSurf.h"

/***/

static int noneModifier_isDisabled(ModifierData *md)
{
	return 1;
}

/* Curve */

static int curveModifier_isDisabled(ModifierData *md)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	return !cmd->object;
}

static void curveModifier_updateDepgraph(ModifierData *md, DagForest *forest, Object *ob, DagNode *obNode)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	if (cmd->object) {
		DagNode *curNode = dag_get_node(forest, cmd->object);

		dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA);
	}
}

static void curveModifier_deformVerts(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int numVerts)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	curve_deform_verts(cmd->object, ob, vertexCos, numVerts);
}

static void curveModifier_deformVertsEM(ModifierData *md, Object *ob, void *editData, void *derivedData, float (*vertexCos)[3], int numVerts)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	curve_deform_verts(cmd->object, ob, vertexCos, numVerts);
}

/* Lattice */

static int latticeModifier_isDisabled(ModifierData *md)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	return !lmd->object;
}

static void latticeModifier_updateDepgraph(ModifierData *md, DagForest *forest, Object *ob, DagNode *obNode)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	if (lmd->object) {
		DagNode *latNode = dag_get_node(forest, lmd->object);

		dag_add_relation(forest, latNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA);
	}
}

static void latticeModifier_deformVerts(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int numVerts)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	lattice_deform_verts(lmd->object, ob, vertexCos, numVerts);
}

static void latticeModifier_deformVertsEM(ModifierData *md, Object *ob, void *editData, void *derivedData, float (*vertexCos)[3], int numVerts)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	lattice_deform_verts(lmd->object, ob, vertexCos, numVerts);
}

/* Subsurf */

static void subsurfModifier_initData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;
	
	smd->levels = 1;
	smd->renderLevels = 2;
}

static void subsurfModifier_freeData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;

	if (smd->mCache) {
		ccgSubSurf_free(smd->mCache);
	}
	if (smd->emCache) {
		ccgSubSurf_free(smd->emCache);
	}
}	

static void *subsurfModifier_applyModifier(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData;
	SubsurfModifierData *smd = (SubsurfModifierData*) md;
	Mesh *me = ob->data;

	if (dm) {
		DispListMesh *dlm = dm->convertToDispListMesh(dm); // XXX what if verts were shared
		int i;

		if (vertexCos) {
			int numVerts = dm->getNumVerts(dm);

			for (i=0; i<numVerts; i++) {
				VECCOPY(dlm->mvert[i].co, vertexCos[i]);
			}
		}
		dm->release(dm);

		dm = subsurf_make_derived_from_mesh(me, dlm, smd, useRenderParams, NULL, isFinalCalc);

		return dm;
	} else {
		return subsurf_make_derived_from_mesh(me, NULL, smd, useRenderParams, vertexCos, isFinalCalc);
	}
}

static void *subsurfModifier_applyModifierEM(ModifierData *md, Object *ob, void *editData, void *derivedData, float (*vertexCos)[3])
{
	EditMesh *em = editData;
	DerivedMesh *dm = derivedData;
	SubsurfModifierData *smd = (SubsurfModifierData*) md;

	if (dm) {
		DispListMesh *dlm = dm->convertToDispListMesh(dm); // XXX what if verts were shared
		int i;

		if (vertexCos) {
			int numVerts = dm->getNumVerts(dm);

			for (i=0; i<numVerts; i++) {
				VECCOPY(dlm->mvert[i].co, vertexCos[i]);
			}
		}
		dm->release(dm);

			// XXX, should I worry about reuse of mCache in editmode?
		dm = subsurf_make_derived_from_mesh(NULL, dlm, smd, 0, NULL, 1);

		return dm;
	} else {
		return subsurf_make_derived_from_editmesh(em, smd, vertexCos);
	}
}

/* Build */

static void buildModifier_initData(ModifierData *md)
{
	BuildModifierData *bmd = (BuildModifierData*) md;

	bmd->start = 1.0;
	bmd->length = 100.0;
}

static int buildModifier_dependsOnTime(ModifierData *md)
{
	return 1;
}

static void *buildModifier_applyModifier(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData;
	BuildModifierData *bmd = (BuildModifierData*) md;
	DispListMesh *dlm=NULL, *ndlm = MEM_callocN(sizeof(*ndlm), "build_dlm");
	MVert *mvert;
	MEdge *medge;
	MFace *mface;
	MCol *mcol;
	TFace *tface;
	int totvert, totedge, totface;
	int i,j;
	float frac;

	if (dm) {
		dlm = dm->convertToDispListMesh(dm);
		mvert = dlm->mvert;
		medge = dlm->medge;
		mface = dlm->mface;
		mcol = dlm->mcol;
		tface = dlm->tface;
		totvert = dlm->totvert;
		totedge = dlm->totedge;
		totface = dlm->totface;
	} else {
		Mesh *me = ob->data;
		mvert = me->mvert;
		medge = me->medge;
		mface = me->mface;
		mcol = me->mcol;
		tface = me->tface;
		totvert = me->totvert;
		totedge = me->totedge;
		totface = me->totface;
	}

	if (ob) {
		frac = bsystem_time(ob, 0, (float)G.scene->r.cfra, bmd->start-1.0f)/bmd->length;
	} else {
		frac = G.scene->r.cfra - bmd->start/bmd->length;
	}
	CLAMP(frac, 0.0, 1.0);

	ndlm->totface = totface*frac;
	ndlm->totedge = totedge*frac;
	if (ndlm->totface) {
		ndlm->mvert = MEM_mallocN(sizeof(*ndlm->mvert)*totvert, "build_mvert");
		memcpy(ndlm->mvert, mvert, sizeof(*mvert)*totvert);
		for (i=0; i<totvert; i++) {
			if (vertexCos)
				VECCOPY(ndlm->mvert[i].co, vertexCos[i]);
			ndlm->mvert[i].flag = 0;
		}

		if (bmd->randomize) {
			ndlm->mface = MEM_dupallocN(mface);
			BLI_array_randomize(ndlm->mface, sizeof(*mface), totface, bmd->seed);

			if (tface) {
				ndlm->tface = MEM_dupallocN(tface);
				BLI_array_randomize(ndlm->tface, sizeof(*tface), totface, bmd->seed);
			} else if (mcol) {
				ndlm->mcol = MEM_dupallocN(mcol);
				BLI_array_randomize(ndlm->mcol, sizeof(*mcol)*4, totface, bmd->seed);
			}
		} else {
			ndlm->mface = MEM_mallocN(sizeof(*ndlm->mface)*ndlm->totface, "build_mf");
			memcpy(ndlm->mface, mface, sizeof(*mface)*ndlm->totface);

			if (tface) {
				ndlm->tface = MEM_mallocN(sizeof(*ndlm->tface)*ndlm->totface, "build_tf");
				memcpy(ndlm->tface, tface, sizeof(*tface)*ndlm->totface);
			} else if (mcol) {
				ndlm->mcol = MEM_mallocN(sizeof(*ndlm->mcol)*4*ndlm->totface, "build_mcol");
				memcpy(ndlm->mcol, mcol, sizeof(*mcol)*4*ndlm->totface);
			}
		}

		for (i=0; i<ndlm->totface; i++) {
			MFace *mf = &ndlm->mface[i];

			ndlm->mvert[mf->v1].flag = 1;
			ndlm->mvert[mf->v2].flag = 1;
			if (mf->v3) {
				ndlm->mvert[mf->v3].flag = 1;
				if (mf->v4) ndlm->mvert[mf->v4].flag = 1;
			}
		}

			/* Store remapped indices in *((int*) mv->no) */
		ndlm->totvert = 0;
		for (i=0; i<totvert; i++) {
			MVert *mv = &ndlm->mvert[i];

			if (mv->flag) 
				*((int*) mv->no) = ndlm->totvert++;
		}

			/* Remap face vertex indices */
		for (i=0; i<ndlm->totface; i++) {
			MFace *mf = &ndlm->mface[i];

			mf->v1 = *((int*) ndlm->mvert[mf->v1].no);
			mf->v2 = *((int*) ndlm->mvert[mf->v2].no);
			if (mf->v3) {
				mf->v3 = *((int*) ndlm->mvert[mf->v3].no);
				if (mf->v4) mf->v4 = *((int*) ndlm->mvert[mf->v4].no);
			}
		}
			/* Copy in all edges that have both vertices (remap in process) */
		if (totedge) {
			ndlm->totedge = 0;
			ndlm->medge = MEM_mallocN(sizeof(*ndlm->medge)*totedge, "build_med");

			for (i=0; i<totedge; i++) {
				MEdge *med = &medge[i];

				if (ndlm->mvert[med->v1].flag && ndlm->mvert[med->v2].flag) {
					MEdge *nmed = &ndlm->medge[ndlm->totedge++];

					memcpy(nmed, med, sizeof(*med));

					nmed->v1 = *((int*) ndlm->mvert[nmed->v1].no);
					nmed->v2 = *((int*) ndlm->mvert[nmed->v2].no);
				}
			}
		}

			/* Collapse vertex array to remove unused verts */
		for(i=j=0; i<totvert; i++) {
			MVert *mv = &ndlm->mvert[i];

			if (mv->flag) {
				if (j!=i) 
					memcpy(&ndlm->mvert[j], mv, sizeof(*mv));
				j++;
			}
		}
	} else if (ndlm->totedge) {
		ndlm->mvert = MEM_mallocN(sizeof(*ndlm->mvert)*totvert, "build_mvert");
		memcpy(ndlm->mvert, mvert, sizeof(*mvert)*totvert);
		for (i=0; i<totvert; i++) {
			if (vertexCos)
				VECCOPY(ndlm->mvert[i].co, vertexCos[i]);
			ndlm->mvert[i].flag = 0;
		}

		if (bmd->randomize) {
			ndlm->medge = MEM_dupallocN(medge);
			BLI_array_randomize(ndlm->medge, sizeof(*medge), totedge, bmd->seed);
		} else {
			ndlm->medge = MEM_mallocN(sizeof(*ndlm->medge)*ndlm->totedge, "build_mf");
			memcpy(ndlm->medge, medge, sizeof(*medge)*ndlm->totedge);
		}

		for (i=0; i<ndlm->totedge; i++) {
			MEdge *med = &ndlm->medge[i];

			ndlm->mvert[med->v1].flag = 1;
			ndlm->mvert[med->v2].flag = 1;
		}

			/* Store remapped indices in *((int*) mv->no) */
		ndlm->totvert = 0;
		for (i=0; i<totvert; i++) {
			MVert *mv = &ndlm->mvert[i];

			if (mv->flag) 
				*((int*) mv->no) = ndlm->totvert++;
		}

			/* Remap edge vertex indices */
		for (i=0; i<ndlm->totedge; i++) {
			MEdge *med = &ndlm->medge[i];

			med->v1 = *((int*) ndlm->mvert[med->v1].no);
			med->v2 = *((int*) ndlm->mvert[med->v2].no);
		}

			/* Collapse vertex array to remove unused verts */
		for(i=j=0; i<totvert; i++) {
			MVert *mv = &ndlm->mvert[i];

			if (mv->flag) {
				if (j!=i) 
					memcpy(&ndlm->mvert[j], mv, sizeof(*mv));
				j++;
			}
		}
	} else {
		ndlm->totvert = totvert*frac;

		if (bmd->randomize) {
			ndlm->mvert = MEM_dupallocN(mvert);
			BLI_array_randomize(ndlm->mvert, sizeof(*mvert), totvert, bmd->seed);
		} else {
			ndlm->mvert = MEM_mallocN(sizeof(*ndlm->mvert)*ndlm->totvert, "build_mvert");
			memcpy(ndlm->mvert, mvert, sizeof(*mvert)*ndlm->totvert);
		}

		if (vertexCos) {
			for (i=0; i<ndlm->totvert; i++) {
				VECCOPY(ndlm->mvert[i].co, vertexCos[i]);
			}
		}
	}

	if (dm) dm->release(dm);
	if (dlm) displistmesh_free(dlm);

	mesh_calc_normals(ndlm->mvert, ndlm->totvert, ndlm->mface, ndlm->totface, &ndlm->nors);
	
	return derivedmesh_from_displistmesh(ndlm);
}

/* Mirror */

static void mirrorModifier_initData(ModifierData *md)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	mmd->tolerance = 0.001;
}

static void mirrorModifier__doMirror(MirrorModifierData *mmd, DispListMesh *ndlm, float (*vertexCos)[3])
{
	int totvert=ndlm->totvert, totedge=ndlm->totedge, totface=ndlm->totface;
	int i, axis = mmd->axis;
	float tolerance = mmd->tolerance;

	for (i=0; i<totvert; i++) {
		MVert *mv = &ndlm->mvert[i];

		if (ABS(mv->co[axis])<=tolerance) {
			mv->co[axis] = 0;
			*((int*) mv->no) = i;
		} else {
			MVert *nmv = &ndlm->mvert[ndlm->totvert];

				/* Because the topology result (# of vertices) must stuff the same
				 * if the mesh data is overridden by vertex cos, have to calc sharedness
				 * based on original coordinates. Only write new cos for non-shared
				 * vertices.
				 */
			if (vertexCos) {
				VECCOPY(mv->co, vertexCos[i]);
			}

			memcpy(nmv, mv, sizeof(*mv));
			nmv ->co[axis] = -nmv ->co[axis];

			*((int*) mv->no) = ndlm->totvert++;
		}
	}

	if (ndlm->medge) {
		for (i=0; i<totedge; i++) {
			MEdge *med = &ndlm->medge[i];
			MEdge *nmed = &ndlm->medge[ndlm->totedge];

			memcpy(nmed, med, sizeof(*med));

			nmed->v1 = *((int*) ndlm->mvert[nmed->v1].no);
			nmed->v2 = *((int*) ndlm->mvert[nmed->v2].no);

			if (nmed->v1!=med->v1 || nmed->v2!=med->v2) {
				ndlm->totedge++;
			}
		}
	}

	for (i=0; i<totface; i++) {
		MFace *mf = &ndlm->mface[i];
		MFace *nmf = &ndlm->mface[ndlm->totface];
		TFace *tf=NULL, *ntf=NULL; /* gcc's mother is uninitialized! */
		MCol *mc=NULL, *nmc=NULL; /* gcc's mother is uninitialized! */

		memcpy(nmf, mf, sizeof(*mf));
		if (ndlm->tface) {
			ntf = &ndlm->tface[ndlm->totface];
			tf = &ndlm->tface[i];
			memcpy(ntf, tf, sizeof(*ndlm->tface));
		} else if (ndlm->mcol) {
			nmc = &ndlm->mcol[ndlm->totface*4];
			mc = &ndlm->mcol[i*4];
			memcpy(nmc, mc, sizeof(*ndlm->mcol)*4);
		}

			/* Map vertices to shared */

		nmf->v1 = *((int*) ndlm->mvert[nmf->v1].no);
		nmf->v2 = *((int*) ndlm->mvert[nmf->v2].no);
		if (nmf->v3) {
			nmf->v3 = *((int*) ndlm->mvert[nmf->v3].no);
			if (nmf->v4) nmf->v4 = *((int*) ndlm->mvert[nmf->v4].no);
		}

			/* If all vertices shared don't duplicate face */
		if (nmf->v1==mf->v1 && nmf->v2==mf->v2 && nmf->v3==mf->v3 && nmf->v4==mf->v4)
			continue;

		if (nmf->v3) {
			if (nmf->v4) {
				int copyIdx;

					/* If three in order vertices are shared then duplicating the face 
					* will be strange (don't want two quads sharing three vertices in a
					* mesh. Instead modify the original quad to leave out the middle vertice
					* and span the gap. Vertice will remain in mesh and still have edges
					* to it but will not interfere with normals.
					*/
				if (nmf->v4==mf->v4 && nmf->v1==mf->v1 && nmf->v2==mf->v2) {
					mf->v1 = nmf->v3;
					copyIdx = 0;
				} else if (nmf->v1==mf->v1 && nmf->v2==mf->v2 && nmf->v3==mf->v3) {
					mf->v2 = nmf->v4;
					copyIdx = 1;
				}  else if (nmf->v2==mf->v2 && nmf->v3==mf->v3 && nmf->v4==mf->v4) {
					mf->v3 = nmf->v1;
					copyIdx = 2;
				} else if (nmf->v3==mf->v3 && nmf->v4==mf->v4 && nmf->v1==mf->v1) {
					mf->v4 = nmf->v2;
					copyIdx = 3;
				} else {
					copyIdx = -1;
				}

				if (copyIdx!=-1) {
					int fromIdx = (copyIdx+2)%4;

					if (ndlm->tface) {
						tf->col[copyIdx] = ntf->col[fromIdx];
						tf->uv[copyIdx][0] = ntf->uv[fromIdx][0];
						tf->uv[copyIdx][1] = ntf->uv[fromIdx][1];
					} else if (ndlm->mcol) {
						mc[copyIdx] = nmc[fromIdx];
					}

					continue;
				}
			}

				/* Need to flip face normal, pick which verts to flip
				 * in order to prevent nmf->v3==0 or nmf->v4==0
				 */
			if (nmf->v1) {
				SWAP(int, nmf->v1, nmf->v3);

				if (ndlm->tface) {
					SWAP(unsigned int, ntf->col[0], ntf->col[2]);
					SWAP(float, ntf->uv[0][0], ntf->uv[2][0]);
					SWAP(float, ntf->uv[0][1], ntf->uv[2][1]);
				} else if (ndlm->mcol) {
					SWAP(MCol, nmc[0], nmc[2]);
				}
			} else {
				SWAP(int, nmf->v2, nmf->v4);

				if (ndlm->tface) {
					SWAP(unsigned int, ntf->col[1], ntf->col[3]);
					SWAP(float, ntf->uv[1][0], ntf->uv[3][0]);
					SWAP(float, ntf->uv[1][1], ntf->uv[3][1]);
				} else if (ndlm->mcol) {
					SWAP(MCol, nmc[1], nmc[3]);
				}
			}
		}

		ndlm->totface++;
	}
}

static void *mirrorModifier_applyModifier(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData;
	MirrorModifierData *mmd = (MirrorModifierData*) md;
	DispListMesh *dlm=NULL, *ndlm = MEM_callocN(sizeof(*dlm), "mm_dlm");
	MVert *mvert;
	MEdge *medge;
	MFace *mface;
	TFace *tface;
	MCol *mcol;

	if (dm) {
		dlm = dm->convertToDispListMesh(dm);

		mvert = dlm->mvert;
		medge = dlm->medge;
		mface = dlm->mface;
		tface = dlm->tface;
		mcol = dlm->mcol;
		ndlm->totvert = dlm->totvert;
		ndlm->totedge = dlm->totedge;
		ndlm->totface = dlm->totface;
	} else {
		Mesh *me = ob->data;

		mvert = me->mvert;
		medge = me->medge;
		mface = me->mface;
		tface = me->tface;
		mcol = me->mcol;
		ndlm->totvert = me->totvert;
		ndlm->totedge = me->totedge;
		ndlm->totface = me->totface;
	}

	ndlm->mvert = MEM_mallocN(sizeof(*mvert)*ndlm->totvert*2, "mm_mv");
	memcpy(ndlm->mvert, mvert, sizeof(*mvert)*ndlm->totvert);

	if (medge) {
		ndlm->medge = MEM_mallocN(sizeof(*medge)*ndlm->totedge*2, "mm_med");
		memcpy(ndlm->medge, medge, sizeof(*medge)*ndlm->totedge);
	}

	ndlm->mface = MEM_mallocN(sizeof(*mface)*ndlm->totface*2, "mm_mf");
	memcpy(ndlm->mface, mface, sizeof(*mface)*ndlm->totface);

	if (tface) {
		ndlm->tface = MEM_mallocN(sizeof(*tface)*ndlm->totface*2, "mm_tf");
		memcpy(ndlm->tface, tface, sizeof(*tface)*ndlm->totface);
	} else if (mcol) {
		ndlm->mcol = MEM_mallocN(sizeof(*mcol)*4*ndlm->totface*2, "mm_mcol");
		memcpy(ndlm->mcol, mcol, sizeof(*mcol)*4*ndlm->totface);
	}

	mirrorModifier__doMirror(mmd, ndlm, vertexCos);

	if (dlm) displistmesh_free(dlm);
	if (dm) dm->release(dm);

	mesh_calc_normals(ndlm->mvert, ndlm->totvert, ndlm->mface, ndlm->totface, &ndlm->nors);
	
	return derivedmesh_from_displistmesh(ndlm);
}

static void *mirrorModifier_applyModifierEM(ModifierData *md, Object *ob, void *editData, void *derivedData, float (*vertexCos)[3])
{
	if (derivedData) {
		return mirrorModifier_applyModifier(md, ob, derivedData, vertexCos, 0, 1);
	} else {
		MirrorModifierData *mmd = (MirrorModifierData*) md;
		DispListMesh *ndlm = MEM_callocN(sizeof(*ndlm), "mm_dlm");
		int i, axis = mmd->axis;
		float tolerance = mmd->tolerance;
		EditMesh *em = editData;
		EditVert *eve, *preveve;
		EditEdge *eed;
		EditFace *efa;

		for (i=0,eve=em->verts.first; eve; eve= eve->next)
			eve->prev = (EditVert*) i++;

		ndlm->totvert = BLI_countlist(&em->verts);
		ndlm->totedge = BLI_countlist(&em->edges);
		ndlm->totface = BLI_countlist(&em->faces);

		ndlm->mvert = MEM_mallocN(sizeof(*ndlm->mvert)*ndlm->totvert*2, "mm_mv");
		ndlm->medge = MEM_mallocN(sizeof(*ndlm->medge)*ndlm->totedge*2, "mm_med");
		ndlm->mface = MEM_mallocN(sizeof(*ndlm->mface)*ndlm->totface*2, "mm_mf");

		for (i=0,eve=em->verts.first; i<ndlm->totvert; i++,eve=eve->next) {
			MVert *mv = &ndlm->mvert[i];

			VECCOPY(mv->co, eve->co);
		}
		for (i=0,eed=em->edges.first; i<ndlm->totedge; i++,eed=eed->next) {
			MEdge *med = &ndlm->medge[i];

			med->v1 = (int) eed->v1->prev;
			med->v2 = (int) eed->v2->prev;
			med->crease = eed->crease;
		}
		for (i=0,efa=em->faces.first; i<ndlm->totface; i++,efa=efa->next) {
			MFace *mf = &ndlm->mface[i];
			mf->v1 = (int) efa->v1->prev;
			mf->v2 = (int) efa->v2->prev;
			mf->v3 = (int) efa->v3->prev;
			mf->v4 = efa->v4?(int) efa->v4->prev:0;
			mf->mat_nr = efa->mat_nr;
			mf->flag = efa->flag;
		}

		mirrorModifier__doMirror(mmd, ndlm, vertexCos);

		for (preveve=NULL, eve=em->verts.first; eve; preveve=eve, eve= eve->next)
			eve->prev = preveve;

		mesh_calc_normals(ndlm->mvert, ndlm->totvert, ndlm->mface, ndlm->totface, &ndlm->nors);
		
		return derivedmesh_from_displistmesh(ndlm);
	}
}

/***/

static ModifierTypeInfo typeArr[NUM_MODIFIER_TYPES];
static int typeArrInit = 1;

ModifierTypeInfo *modifierType_get_info(ModifierType type)
{
	if (typeArrInit) {
		ModifierTypeInfo *mti;

		memset(typeArr, 0, sizeof(typeArr));

		/* Initialize and return the appropriate type info structure,
		 * assumes that modifier has:
		 *  name == typeName, 
		 *  structName == typeName + 'ModifierData'
		 */
#define INIT_TYPE(typeName) \
	(	strcpy(typeArr[eModifierType_##typeName].name, #typeName), \
		strcpy(typeArr[eModifierType_##typeName].structName, #typeName "ModifierData"), \
		typeArr[eModifierType_##typeName].structSize = sizeof(typeName##ModifierData), \
		&typeArr[eModifierType_##typeName])

		mti = &typeArr[eModifierType_None];
		strcpy(mti->name, "None");
		strcpy(mti->structName, "ModifierData");
		mti->structSize = sizeof(ModifierData);
		mti->type = eModifierType_None;
		mti->flags = eModifierTypeFlag_AcceptsMesh|eModifierTypeFlag_AcceptsCVs;
		mti->isDisabled = noneModifier_isDisabled;

		mti = INIT_TYPE(Curve);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_SupportsEditmode;
		mti->isDisabled = curveModifier_isDisabled;
		mti->updateDepgraph = curveModifier_updateDepgraph;
		mti->deformVerts = curveModifier_deformVerts;
		mti->deformVertsEM = curveModifier_deformVertsEM;

		mti = INIT_TYPE(Lattice);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_SupportsEditmode;
		mti->isDisabled = latticeModifier_isDisabled;
		mti->updateDepgraph = latticeModifier_updateDepgraph;
		mti->deformVerts = latticeModifier_deformVerts;
		mti->deformVertsEM = latticeModifier_deformVertsEM;

		mti = INIT_TYPE(Subsurf);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping | eModifierTypeFlag_SupportsEditmode;
		mti->initData = subsurfModifier_initData;
		mti->freeData = subsurfModifier_freeData;
		mti->applyModifier = subsurfModifier_applyModifier;
		mti->applyModifierEM = subsurfModifier_applyModifierEM;

		mti = INIT_TYPE(Build);
		mti->type = eModifierTypeType_Nonconstructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh;
		mti->initData = buildModifier_initData;
		mti->dependsOnTime = buildModifier_dependsOnTime;
		mti->applyModifier = buildModifier_applyModifier;

		mti = INIT_TYPE(Mirror);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode;
		mti->initData = mirrorModifier_initData;
		mti->applyModifier = mirrorModifier_applyModifier;
		mti->applyModifierEM = mirrorModifier_applyModifierEM;

		typeArrInit = 0;
#undef INIT_TYPE
	}

	if (type>=0 && type<NUM_MODIFIER_TYPES && typeArr[type].name[0]!='\0') {
		return &typeArr[type];
	} else {
		return NULL;
	}
}

ModifierData *modifier_new(int type)
{
	ModifierTypeInfo *mti = modifierType_get_info(type);
	ModifierData *md = MEM_callocN(mti->structSize, mti->structName);

	md->type = type;
	md->mode = eModifierMode_RealtimeAndRender;

	if (mti->initData) mti->initData(md);

	return md;
}

void modifier_free(ModifierData *md) 
{
	ModifierTypeInfo *mti = modifierType_get_info(md->type);

	if (mti->freeData) mti->freeData(md);

	MEM_freeN(md);
}

int modifier_dependsOnTime(ModifierData *md) 
{
	ModifierTypeInfo *mti = modifierType_get_info(md->type);

	return mti->dependsOnTime && mti->dependsOnTime(md);
}
