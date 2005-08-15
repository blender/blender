#include "string.h"
#include "stdarg.h"
#include "math.h"

#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_arithb.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_effect_types.h"
#include "DNA_scene_types.h"
#include "BLI_editVert.h"

#include "BKE_bad_level_calls.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_modifier.h"
#include "BKE_lattice.h"
#include "BKE_subsurf.h"
#include "BKE_object.h"
#include "BKE_mesh.h"
#include "BKE_softbody.h"
#include "depsgraph_private.h"

#include "LOD_DependKludge.h"
#include "LOD_decimation.h"

#include "CCGSubSurf.h"

/***/

static int noneModifier_isDisabled(ModifierData *md)
{
	return 1;
}

/* Curve */

static void curveModifier_copyData(ModifierData *md, ModifierData *target)
{
	CurveModifierData *cmd = (CurveModifierData*) md;
	CurveModifierData *tcmd = (CurveModifierData*) target;

	tcmd->object = cmd->object;
}

static int curveModifier_isDisabled(ModifierData *md)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	return !cmd->object;
}

static void curveModifier_foreachObjectLink(ModifierData *md, Object *ob, void (*walk)(void *userData, Object *ob, Object **obpoin), void *userData)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	walk(userData, ob, &cmd->object);
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
	curveModifier_deformVerts(md, ob, NULL, vertexCos, numVerts);
}

/* Lattice */

static void latticeModifier_copyData(ModifierData *md, ModifierData *target)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;
	LatticeModifierData *tlmd = (LatticeModifierData*) target;

	tlmd->object = lmd->object;
}

static int latticeModifier_isDisabled(ModifierData *md)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	return !lmd->object;
}

static void latticeModifier_foreachObjectLink(ModifierData *md, Object *ob, void (*walk)(void *userData, Object *ob, Object **obpoin), void *userData)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	walk(userData, ob, &lmd->object);
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
	latticeModifier_deformVerts(md, ob, NULL, vertexCos, numVerts);
}

/* Subsurf */

static void subsurfModifier_initData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;
	
	smd->levels = 1;
	smd->renderLevels = 2;
}

static void subsurfModifier_copyData(ModifierData *md, ModifierData *target)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;
	SubsurfModifierData *tsmd = (SubsurfModifierData*) target;

	tsmd->flags = smd->flags;
	tsmd->levels = smd->levels;
	tsmd->renderLevels = smd->renderLevels;
	tsmd->subdivType = smd->subdivType;
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
		DispListMesh *dlm = dm->convertToDispListMesh(dm, 0);
		int i;

		if (vertexCos) {
			int numVerts = dm->getNumVerts(dm);

			for (i=0; i<numVerts; i++) {
				VECCOPY(dlm->mvert[i].co, vertexCos[i]);
			}
		}

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
		EditVert **vertMap;
		EditEdge **edgeMap;
		EditFace **faceMap;
		DispListMesh *dlm = dm->convertToDispListMeshMapped(dm, 0, &vertMap, &edgeMap, &faceMap);

		dm = subsurf_make_derived_from_dlm_em(dlm, smd, vertexCos, vertMap, edgeMap, faceMap);

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

static void buildModifier_copyData(ModifierData *md, ModifierData *target)
{
	BuildModifierData *bmd = (BuildModifierData*) md;
	BuildModifierData *tbmd = (BuildModifierData*) target;

	tbmd->start = bmd->start;
	tbmd->length = bmd->length;
	tbmd->randomize = bmd->randomize;
	tbmd->seed = bmd->seed;
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
		dlm = dm->convertToDispListMesh(dm, 1);
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

	if (dlm) displistmesh_free(dlm);

	mesh_calc_normals(ndlm->mvert, ndlm->totvert, ndlm->mface, ndlm->totface, &ndlm->nors);
	
	return derivedmesh_from_displistmesh(ndlm, NULL, NULL, NULL, NULL);
}

/* Mirror */

static void mirrorModifier_initData(ModifierData *md)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	mmd->tolerance = 0.001;
}

static void mirrorModifier_copyData(ModifierData *md, ModifierData *target)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;
	MirrorModifierData *tmmd = (MirrorModifierData*) target;

	tmmd->axis = mmd->axis;
	tmmd->tolerance = mmd->tolerance;
}

static void mirrorModifier__doMirror(MirrorModifierData *mmd, DispListMesh *ndlm, float (*vertexCos)[3], EditVert **vertMap, EditEdge **edgeMap, EditFace **faceMap, EditVert ***vertMap_r, EditEdge ***edgeMap_r, EditFace ***faceMap_r)
{
	int totvert=ndlm->totvert, totedge=ndlm->totedge, totface=ndlm->totface;
	int i, axis = mmd->axis;
	float tolerance = mmd->tolerance;
	EditVert **vMapOut;
	EditEdge **eMapOut;
	EditFace **fMapOut;

	if (vertMap) {
		*vertMap_r = vMapOut = MEM_mallocN(sizeof(*vMapOut)*totvert*2, "vmap");
		*edgeMap_r = eMapOut = MEM_mallocN(sizeof(*eMapOut)*totedge*2, "emap");
		*faceMap_r = fMapOut = MEM_mallocN(sizeof(*fMapOut)*totface*2, "fmap");
	} else {
		*vertMap_r = vMapOut = NULL;
		*edgeMap_r = eMapOut = NULL;
		*faceMap_r = fMapOut = NULL;
	}

	for (i=0; i<totvert; i++) {
		MVert *mv = &ndlm->mvert[i];
		int isShared = ABS(mv->co[axis])<=tolerance;

				/* Because the topology result (# of vertices) must stuff the same
				 * if the mesh data is overridden by vertex cos, have to calc sharedness
				 * based on original coordinates. Only write new cos for non-shared
				 * vertices. This is why we test before copy.
				 */
		if (vertexCos) {
			VECCOPY(mv->co, vertexCos[i]);
		}

		if (vMapOut) vMapOut[i] = vertMap[i];

		if (isShared) {
			mv->co[axis] = 0;
			*((int*) mv->no) = i;
		} else {
			MVert *nmv = &ndlm->mvert[ndlm->totvert];

			memcpy(nmv, mv, sizeof(*mv));
			nmv ->co[axis] = -nmv ->co[axis];

			if (vMapOut) vMapOut[ndlm->totvert] = vertMap[i];

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

			if (eMapOut) eMapOut[i] = edgeMap[i];

			if (nmed->v1!=med->v1 || nmed->v2!=med->v2) {
				if (eMapOut) eMapOut[ndlm->totedge] = edgeMap[i];

				ndlm->totedge++;
			}
		}
	}

	for (i=0; i<totface; i++) {
		MFace *mf = &ndlm->mface[i];
		MFace *nmf = &ndlm->mface[ndlm->totface];
		TFace *tf=NULL, *ntf=NULL; /* gcc's mother is uninitialized! */
		MCol *mc=NULL, *nmc=NULL;
		
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

		if (fMapOut) fMapOut[i] = faceMap[i];

			/* If all vertices shared don't duplicate face */
		if (nmf->v1==mf->v1 && nmf->v2==mf->v2 && nmf->v3==mf->v3 && nmf->v4==mf->v4)
			continue;

		if (fMapOut) fMapOut[ndlm->totface] = faceMap[i];

		if (nmf->v3) {
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
				if (nmf->v4) {
					SWAP(int, nmf->v2, nmf->v4);

					if (ndlm->tface) {
						SWAP(unsigned int, ntf->col[1], ntf->col[3]);
						SWAP(float, ntf->uv[1][0], ntf->uv[3][0]);
						SWAP(float, ntf->uv[1][1], ntf->uv[3][1]);
					} else if (ndlm->mcol) {
						SWAP(MCol, nmc[1], nmc[3]);
					}
				} else {
					SWAP(int, nmf->v2, nmf->v3);

					if (ndlm->tface) {
						SWAP(unsigned int, ntf->col[1], ntf->col[2]);
						SWAP(float, ntf->uv[1][0], ntf->uv[2][0]);
						SWAP(float, ntf->uv[1][1], ntf->uv[2][1]);
					} else if (ndlm->mcol) {
						SWAP(MCol, nmc[1], nmc[2]);
					}
				}
			}
		}

		ndlm->totface++;
	}
}

static void *mirrorModifier_applyModifier__internal(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int useRenderParams, int isFinalCalc, int needsMaps)
{
	DerivedMesh *dm = derivedData;
	MirrorModifierData *mmd = (MirrorModifierData*) md;
	DispListMesh *dlm=NULL, *ndlm = MEM_callocN(sizeof(*dlm), "mm_dlm");
	MVert *mvert;
	MEdge *medge;
	MFace *mface;
	TFace *tface;
	MCol *mcol;
	EditVert **vertMapOut, **vertMap = NULL;
	EditEdge **edgeMapOut, **edgeMap = NULL;
	EditFace **faceMapOut, **faceMap = NULL;

	if (dm) {
		if (needsMaps) {
			dlm = dm->convertToDispListMeshMapped(dm, 1, &vertMap, &edgeMap, &faceMap);
		} else {
			dlm = dm->convertToDispListMesh(dm, 1);
		}

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

	mirrorModifier__doMirror(mmd, ndlm, vertexCos, vertMap, edgeMap, faceMap, &vertMapOut, &edgeMapOut, &faceMapOut);

	if (dlm) {
		displistmesh_free(dlm);
		if (needsMaps) {
			MEM_freeN(vertMap);
			MEM_freeN(edgeMap);
			MEM_freeN(faceMap);
		}
	}

	mesh_calc_normals(ndlm->mvert, ndlm->totvert, ndlm->mface, ndlm->totface, &ndlm->nors);
	
	return derivedmesh_from_displistmesh(ndlm, NULL, vertMapOut, edgeMapOut, faceMapOut);
}

static void *mirrorModifier_applyModifier(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int useRenderParams, int isFinalCalc)
{
	return mirrorModifier_applyModifier__internal(md, ob, derivedData, vertexCos, 0, 1, 0);
}

static void *mirrorModifier_applyModifierEM(ModifierData *md, Object *ob, void *editData, void *derivedData, float (*vertexCos)[3])
{
	if (derivedData) {
		return mirrorModifier_applyModifier__internal(md, ob, derivedData, vertexCos, 0, 1, 1);
	} else {
		MirrorModifierData *mmd = (MirrorModifierData*) md;
		DispListMesh *ndlm = MEM_callocN(sizeof(*ndlm), "mm_dlm");
		EditMesh *em = editData;
		EditVert *eve, *preveve;
		EditEdge *eed;
		EditFace *efa;
		EditVert **vertMapOut, **vertMap;
		EditEdge **edgeMapOut, **edgeMap;
		EditFace **faceMapOut, **faceMap;
		int i;

		for (i=0,eve=em->verts.first; eve; eve= eve->next)
			eve->prev = (EditVert*) i++;

		ndlm->totvert = BLI_countlist(&em->verts);
		ndlm->totedge = BLI_countlist(&em->edges);
		ndlm->totface = BLI_countlist(&em->faces);

		vertMap = MEM_mallocN(sizeof(*vertMap)*ndlm->totvert, "mm_vmap");
		edgeMap = MEM_mallocN(sizeof(*edgeMap)*ndlm->totedge, "mm_emap");
		faceMap = MEM_mallocN(sizeof(*faceMap)*ndlm->totface, "mm_fmap");

		ndlm->mvert = MEM_mallocN(sizeof(*ndlm->mvert)*ndlm->totvert*2, "mm_mv");
		ndlm->medge = MEM_mallocN(sizeof(*ndlm->medge)*ndlm->totedge*2, "mm_med");
		ndlm->mface = MEM_mallocN(sizeof(*ndlm->mface)*ndlm->totface*2, "mm_mf");

		for (i=0,eve=em->verts.first; i<ndlm->totvert; i++,eve=eve->next) {
			MVert *mv = &ndlm->mvert[i];

			VECCOPY(mv->co, eve->co);

			vertMap[i] = eve;
		}
		for (i=0,eed=em->edges.first; i<ndlm->totedge; i++,eed=eed->next) {
			MEdge *med = &ndlm->medge[i];

			med->v1 = (int) eed->v1->prev;
			med->v2 = (int) eed->v2->prev;
			med->crease = (unsigned char) (eed->crease*255.0f);

			edgeMap[i] = eed;
		}
		for (i=0,efa=em->faces.first; i<ndlm->totface; i++,efa=efa->next) {
			MFace *mf = &ndlm->mface[i];
			mf->v1 = (int) efa->v1->prev;
			mf->v2 = (int) efa->v2->prev;
			mf->v3 = (int) efa->v3->prev;
			mf->v4 = efa->v4?(int) efa->v4->prev:0;
			mf->mat_nr = efa->mat_nr;
			mf->flag = efa->flag;

			test_index_mface(mf, efa->v4?4:3);

			faceMap[i] = efa;
		}

		mirrorModifier__doMirror(mmd, ndlm, vertexCos, vertMap, edgeMap, faceMap, &vertMapOut, &edgeMapOut, &faceMapOut);

		for (preveve=NULL, eve=em->verts.first; eve; preveve=eve, eve= eve->next)
			eve->prev = preveve;

		mesh_calc_normals(ndlm->mvert, ndlm->totvert, ndlm->mface, ndlm->totface, &ndlm->nors);
		
		MEM_freeN(faceMap);
		MEM_freeN(edgeMap);
		MEM_freeN(vertMap);

		return derivedmesh_from_displistmesh(ndlm, NULL, vertMapOut, edgeMapOut, faceMapOut);
	}
}

/* Decimate */

static void decimateModifier_initData(ModifierData *md)
{
	DecimateModifierData *dmd = (DecimateModifierData*) md;

	dmd->percent = 1.0;
}

static void *decimateModifier_applyModifier(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int useRenderParams, int isFinalCalc)
{
	DecimateModifierData *dmd = (DecimateModifierData*) md;
	DerivedMesh *dm = derivedData;
	Mesh *me = ob->data;
	MVert *mvert;
	MFace *mface;
	DispListMesh *ndlm=NULL, *dlm=NULL;
	LOD_Decimation_Info lod;
	int totvert, totface;
	int a, numTris;

	if (dm) {
		dlm = dm->convertToDispListMesh(dm, 1);
		mvert = dlm->mvert;
		mface = dlm->mface;
		totvert = dlm->totvert;
		totface = dlm->totface;
	} else {
		mvert = me->mvert;
		mface = me->mface;
		totvert = me->totvert;
		totface = me->totface;
	}

	numTris = 0;
	for (a=0; a<totface; a++) {
		MFace *mf = &mface[a];
		if (mf->v3) {
			numTris++;
			if (mf->v4) numTris++;
		}
	}

	if(numTris<3) {
		modifier_setError(md, "There must be more than 3 input faces (triangles).");
		goto exit;
	}

	lod.vertex_buffer= MEM_mallocN(3*sizeof(float)*totvert, "vertices");
	lod.vertex_normal_buffer= MEM_mallocN(3*sizeof(float)*totvert, "normals");
	lod.triangle_index_buffer= MEM_mallocN(3*sizeof(int)*numTris, "trias");
	lod.vertex_num= totvert;
	lod.face_num= numTris;

	for(a=0; a<totvert; a++) {
		MVert *mv = &mvert[a];
		float *vbCo = &lod.vertex_buffer[a*3];
		float *vbNo = &lod.vertex_normal_buffer[a*3];

		if (vertexCos) {
			VECCOPY(vbCo, vertexCos[a]);
		} else {
			VECCOPY(vbCo, mv->co);
		}

		vbNo[0] = mv->no[0]/32767.0f;
		vbNo[1] = mv->no[1]/32767.0f;
		vbNo[2] = mv->no[2]/32767.0f;
	}

	numTris = 0;
	for(a=0; a<totface; a++) {
		MFace *mf = &mface[a];

		if(mf->v3) {
			int *tri = &lod.triangle_index_buffer[3*numTris++];
			tri[0]= mf->v1;
			tri[1]= mf->v2;
			tri[2]= mf->v3;

			if(mf->v4) {
				tri = &lod.triangle_index_buffer[3*numTris++];
				tri[0]= mf->v1;
				tri[1]= mf->v3;
				tri[2]= mf->v4;
			}
		}
	}

	dmd->faceCount = 0;
	if(LOD_LoadMesh(&lod) ) {
		if( LOD_PreprocessMesh(&lod) ) {

			/* we assume the decim_faces tells how much to reduce */

			while(lod.face_num > numTris*dmd->percent) {
				if( LOD_CollapseEdge(&lod)==0) break;
			}

			ndlm= MEM_callocN(sizeof(DispListMesh), "dispmesh");
			ndlm->mvert= MEM_callocN(lod.vertex_num*sizeof(MVert), "mvert");
			ndlm->mface= MEM_callocN(lod.face_num*sizeof(MFace), "mface");
			ndlm->totvert= lod.vertex_num;
			ndlm->totface= dmd->faceCount = lod.face_num;

			for(a=0; a<lod.vertex_num; a++) {
				MVert *mv = &ndlm->mvert[a];
				float *vbCo = &lod.vertex_buffer[a*3];
				
				VECCOPY(mv->co, vbCo);
			}

			for(a=0; a<lod.face_num; a++) {
				MFace *mf = &ndlm->mface[a];
				int *tri = &lod.triangle_index_buffer[a*3];
				mf->v1 = tri[0];
				mf->v2 = tri[1];
				mf->v3 = tri[2];
				test_index_mface(mf, 3);
			}
		}
		else {
			modifier_setError(md, "Out of memory.");
		}

		LOD_FreeDecimationData(&lod);
	}
	else {
		modifier_setError(md, "Non-manifold mesh as input.");
	}

	MEM_freeN(lod.vertex_buffer);
	MEM_freeN(lod.vertex_normal_buffer);
	MEM_freeN(lod.triangle_index_buffer);

exit:
	if (dlm) displistmesh_free(dlm);

	if (ndlm) {
		mesh_calc_normals(ndlm->mvert, ndlm->totvert, ndlm->mface, ndlm->totface, &ndlm->nors);

		return derivedmesh_from_displistmesh(ndlm, NULL, NULL, NULL, NULL);
	} else {
		return NULL;
	}
}

/* Wave */

static void waveModifier_initData(ModifierData *md) 
{
	WaveModifierData *wmd = (WaveModifierData*) md; // whadya know, moved here from Iraq
		
	wmd->flag |= (WAV_X+WAV_Y+WAV_CYCL);
	
	wmd->height= 0.5f;
	wmd->width= 1.5f;
	wmd->speed= 0.5f;
	wmd->narrow= 1.5f;
	wmd->lifetime= 0.0f;
	wmd->damp= 10.0f;
}

static void waveModifier_copyData(ModifierData *md, ModifierData *target)
{
	WaveModifierData *wmd = (WaveModifierData*) md;
	WaveModifierData *twmd = (WaveModifierData*) target;

	twmd->damp = wmd->damp;
	twmd->flag = wmd->flag;
	twmd->height = wmd->height;
	twmd->lifetime = wmd->lifetime;
	twmd->narrow = wmd->narrow;
	twmd->speed = wmd->speed;
	twmd->startx = wmd->startx;
	twmd->starty = wmd->starty;
	twmd->timeoffs = wmd->timeoffs;
	twmd->width = wmd->width;
}

static int waveModifier_dependsOnTime(ModifierData *md)
{
	return 1;
}

static void waveModifier_deformVerts(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int numVerts)
{
	WaveModifierData *wmd = (WaveModifierData*) md;
	float ctime = bsystem_time(ob, 0, (float)G.scene->r.cfra, 0.0);
	float minfac = (float)(1.0/exp(wmd->width*wmd->narrow*wmd->width*wmd->narrow));
	float lifefac = wmd->height;

	if(wmd->damp==0) wmd->damp= 10.0f;

	if(wmd->lifetime!=0.0) {
		float x= ctime - wmd->timeoffs;

		if(x>wmd->lifetime) {
			lifefac= x-wmd->lifetime;
			
			if(lifefac > wmd->damp) lifefac= 0.0;
			else lifefac= (float)(wmd->height*(1.0 - sqrt(lifefac/wmd->damp)));
		}
	}

	if(lifefac!=0.0) {
		int i;

		for (i=0; i<numVerts; i++) {
			float *co = vertexCos[i];
			float x= co[0]-wmd->startx;
			float y= co[1]-wmd->starty;
			float amplit;

			if(wmd->flag & WAV_X) {
				if(wmd->flag & WAV_Y) amplit= (float)sqrt( (x*x + y*y));
				else amplit= x;
			}
			else amplit= y;
			
			/* this way it makes nice circles */
			amplit-= (ctime-wmd->timeoffs)*wmd->speed;

			if(wmd->flag & WAV_CYCL) {
				amplit = (float)fmod(amplit-wmd->width, 2.0*wmd->width) + wmd->width;
			}

				/* GAUSSIAN */
			if(amplit> -wmd->width && amplit<wmd->width) {
				amplit = amplit*wmd->narrow;
				amplit= (float)(1.0/exp(amplit*amplit) - minfac);

				co[2]+= lifefac*amplit;
			}
		}
	}
}

static void waveModifier_deformVertsEM(ModifierData *md, Object *ob, void *editData, void *derivedData, float (*vertexCos)[3], int numVerts)
{
	waveModifier_deformVerts(md, ob, NULL, vertexCos, numVerts);
}

/* Armature */

static void armatureModifier_copyData(ModifierData *md, ModifierData *target)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;
	ArmatureModifierData *tamd = (ArmatureModifierData*) target;

	tamd->object = amd->object;
}

static int armatureModifier_isDisabled(ModifierData *md)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;

	return !amd->object;
}

static void armatureModifier_foreachObjectLink(ModifierData *md, Object *ob, void (*walk)(void *userData, Object *ob, Object **obpoin), void *userData)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;

	walk(userData, ob, &amd->object);
}

static void armatureModifier_updateDepgraph(ModifierData *md, DagForest *forest, Object *ob, DagNode *obNode)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;

	if (amd->object) {
		DagNode *curNode = dag_get_node(forest, amd->object);

		dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA);
	}
}

static void armatureModifier_deformVerts(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int numVerts)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;

	armature_deform_verts(amd->object, ob, vertexCos, numVerts);
}

static void armatureModifier_deformVertsEM(ModifierData *md, Object *ob, void *editData, void *derivedData, float (*vertexCos)[3], int numVerts)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;

	armature_deform_verts(amd->object, ob, vertexCos, numVerts);
}

/* Hook */

static void hookModifier_initData(ModifierData *md) 
{
	HookModifierData *hmd = (HookModifierData*) md;

	hmd->force= 1.0;
}

static void hookModifier_copyData(ModifierData *md, ModifierData *target)
{
	HookModifierData *hmd = (HookModifierData*) md;
	HookModifierData *thmd = (HookModifierData*) target;

	VECCOPY(thmd->cent, hmd->cent);
	thmd->falloff = hmd->falloff;
	thmd->force = hmd->force;
	thmd->object = hmd->object;
	thmd->totindex = hmd->totindex;
	thmd->indexar = MEM_dupallocN(hmd->indexar);
	memcpy(thmd->parentinv, hmd->parentinv, sizeof(hmd->parentinv));
}

static void hookModifier_freeData(ModifierData *md)
{
	HookModifierData *hmd = (HookModifierData*) md;

	if (hmd->indexar) MEM_freeN(hmd->indexar);
}

static int hookModifier_isDisabled(ModifierData *md)
{
	HookModifierData *hmd = (HookModifierData*) md;

	return !hmd->object;
}

static void hookModifier_foreachObjectLink(ModifierData *md, Object *ob, void (*walk)(void *userData, Object *ob, Object **obpoin), void *userData)
{
	HookModifierData *hmd = (HookModifierData*) md;

	walk(userData, ob, &hmd->object);
}

static void hookModifier_updateDepgraph(ModifierData *md, DagForest *forest, Object *ob, DagNode *obNode)
{
	HookModifierData *hmd = (HookModifierData*) md;

	if (hmd->object) {
		DagNode *curNode = dag_get_node(forest, hmd->object);

		dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA);
	}
}

static void hookModifier_deformVerts(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int numVerts)
{
	HookModifierData *hmd = (HookModifierData*) md;
	float vec[3], mat[4][4];
	int i;

	Mat4Invert(ob->imat, ob->obmat);
	Mat4MulSerie(mat, ob->imat, hmd->object->obmat, hmd->parentinv, NULL, NULL, NULL, NULL, NULL);

	for (i=0; i<hmd->totindex; i++) {
		int index = hmd->indexar[i];

			/* This should always be true and I don't generally like 
			 * "paranoid" style code like this, but old files can have
			 * indices that are out of range because old blender did
			 * not correct them on exit editmode. - zr
			 */
		if (index<numVerts) {
			float *co = vertexCos[index];
			float fac = hmd->force;

			if(hmd->falloff!=0.0) {
				float len= VecLenf(co, hmd->cent);
				if(len > hmd->falloff) fac = 0.0;
				else if(len>0.0) fac *= sqrt(1.0 - len/hmd->falloff);
			}

			if(fac!=0.0) {
				VecMat4MulVecfl(vec, mat, co);
				VecLerpf(co, co, vec, fac);
			}
		}
	}
}

static void hookModifier_deformVertsEM(ModifierData *md, Object *ob, void *editData, void *derivedData, float (*vertexCos)[3], int numVerts)
{
	HookModifierData *hmd = (HookModifierData*) md;

	hookModifier_deformVerts(md, ob, derivedData, vertexCos, numVerts);
}

/* Softbody */

static void softbodyModifier_deformVerts(ModifierData *md, Object *ob, void *derivedData, float (*vertexCos)[3], int numVerts)
{
	SoftbodyModifierData *hmd = (SoftbodyModifierData*) md;

	sbObjectStep(ob, (float)G.scene->r.cfra, vertexCos, numVerts);
}

/***/

static ModifierTypeInfo typeArr[NUM_MODIFIER_TYPES];
static int typeArrInit = 1;

ModifierTypeInfo *modifierType_getInfo(ModifierType type)
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
		mti->copyData = curveModifier_copyData;
		mti->isDisabled = curveModifier_isDisabled;
		mti->foreachObjectLink = curveModifier_foreachObjectLink;
		mti->updateDepgraph = curveModifier_updateDepgraph;
		mti->deformVerts = curveModifier_deformVerts;
		mti->deformVertsEM = curveModifier_deformVertsEM;

		mti = INIT_TYPE(Lattice);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_SupportsEditmode;
		mti->copyData = latticeModifier_copyData;
		mti->isDisabled = latticeModifier_isDisabled;
		mti->foreachObjectLink = latticeModifier_foreachObjectLink;
		mti->updateDepgraph = latticeModifier_updateDepgraph;
		mti->deformVerts = latticeModifier_deformVerts;
		mti->deformVertsEM = latticeModifier_deformVertsEM;

		mti = INIT_TYPE(Subsurf);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping | eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode;
		mti->initData = subsurfModifier_initData;
		mti->copyData = subsurfModifier_copyData;
		mti->freeData = subsurfModifier_freeData;
		mti->applyModifier = subsurfModifier_applyModifier;
		mti->applyModifierEM = subsurfModifier_applyModifierEM;

		mti = INIT_TYPE(Build);
		mti->type = eModifierTypeType_Nonconstructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh;
		mti->initData = buildModifier_initData;
		mti->copyData = buildModifier_copyData;
		mti->dependsOnTime = buildModifier_dependsOnTime;
		mti->applyModifier = buildModifier_applyModifier;

		mti = INIT_TYPE(Mirror);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping | eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode;
		mti->initData = mirrorModifier_initData;
		mti->copyData = mirrorModifier_copyData;
		mti->applyModifier = mirrorModifier_applyModifier;
		mti->applyModifierEM = mirrorModifier_applyModifierEM;

		mti = INIT_TYPE(Decimate);
		mti->type = eModifierTypeType_Nonconstructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh;
		mti->initData = decimateModifier_initData;
		mti->applyModifier = decimateModifier_applyModifier;

		mti = INIT_TYPE(Wave);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_SupportsEditmode;
		mti->initData = waveModifier_initData;
		mti->copyData = waveModifier_copyData;
		mti->dependsOnTime = waveModifier_dependsOnTime;
		mti->deformVerts = waveModifier_deformVerts;
		mti->deformVertsEM = waveModifier_deformVertsEM;

		mti = INIT_TYPE(Armature);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_RequiresOriginalData;
		mti->copyData = armatureModifier_copyData;
		mti->isDisabled = armatureModifier_isDisabled;
		mti->foreachObjectLink = armatureModifier_foreachObjectLink;
		mti->updateDepgraph = armatureModifier_updateDepgraph;
		mti->deformVerts = armatureModifier_deformVerts;
		mti->deformVertsEM = armatureModifier_deformVertsEM;

		mti = INIT_TYPE(Hook);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_RequiresOriginalData;
		mti->initData = hookModifier_initData;
		mti->copyData = hookModifier_copyData;
		mti->freeData = hookModifier_freeData;
		mti->isDisabled = hookModifier_isDisabled;
		mti->foreachObjectLink = hookModifier_foreachObjectLink;
		mti->updateDepgraph = hookModifier_updateDepgraph;
		mti->deformVerts = hookModifier_deformVerts;
		mti->deformVertsEM = hookModifier_deformVertsEM;

		mti = INIT_TYPE(Softbody);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_RequiresOriginalData;
		mti->deformVerts = softbodyModifier_deformVerts;

		typeArrInit = 0;
#undef INIT_TYPE
	}

	if (type>=0 && type<NUM_MODIFIER_TYPES && typeArr[type].name[0]!='\0') {
		return &typeArr[type];
	} else {
		return NULL;
	}
}

/***/

ModifierData *modifier_new(int type)
{
	ModifierTypeInfo *mti = modifierType_getInfo(type);
	ModifierData *md = MEM_callocN(mti->structSize, mti->structName);

	strcpy(md->name, mti->name);

	md->type = type;
	md->mode = eModifierMode_Realtime|eModifierMode_Render|eModifierMode_Expanded;

	if (mti->flags&eModifierTypeFlag_EnableInEditmode)
		md->mode |= eModifierMode_Editmode;

	if (mti->initData) mti->initData(md);

	return md;
}

void modifier_free(ModifierData *md) 
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	if (mti->freeData) mti->freeData(md);
	if (md->error) MEM_freeN(md->error);

	MEM_freeN(md);
}

int modifier_dependsOnTime(ModifierData *md) 
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	return mti->dependsOnTime && mti->dependsOnTime(md);
}

int modifier_supportsMapping(ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	return (	(mti->flags&eModifierTypeFlag_SupportsEditmode) &&
				(	(mti->type==eModifierTypeType_OnlyDeform ||
					(mti->flags&eModifierTypeFlag_SupportsMapping))) );
}

ModifierData *modifiers_findByType(Object *ob, ModifierType type)
{
	ModifierData *md = ob->modifiers.first;

	for (; md; md=md->next)
		if (md->type==type)
			break;

	return md;
}

void modifiers_clearErrors(Object *ob)
{
	ModifierData *md = ob->modifiers.first;
	int qRedraw = 0;

	for (; md; md=md->next) {
		if (md->error) {
			MEM_freeN(md->error);
			md->error = NULL;

			qRedraw = 1;
		}
	}

	if (qRedraw) allqueue(REDRAWBUTSEDIT, 0);
}

void modifiers_foreachObjectLink(Object *ob, void (*walk)(void *userData, Object *ob, Object **obpoin), void *userData)
{
	ModifierData *md = ob->modifiers.first;

	for (; md; md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->foreachObjectLink) mti->foreachObjectLink(md, ob, walk, userData);
	}
}

void modifier_copyData(ModifierData *md, ModifierData *target)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	target->mode = md->mode;

	if (mti->copyData)
		mti->copyData(md, target);
}

int modifier_couldBeCage(ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	return (	(md->mode&eModifierMode_Realtime) &&
				(md->mode&eModifierMode_Editmode) &&
				(!mti->isDisabled || !mti->isDisabled(md)) &&
				modifier_supportsMapping(md));	
}

void modifier_setError(ModifierData *md, char *format, ...)
{
	char buffer[2048];
	va_list ap;

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);

	if (md->error)
		MEM_freeN(md->error);

	md->error = BLI_strdup(buffer);

	allqueue(REDRAWBUTSEDIT, 0);
}

int modifiers_getCageIndex(Object *ob, int *lastPossibleCageIndex_r)
{
	ModifierData *md = ob->modifiers.first;
	int i, cageIndex = -1;

		/* Find the last modifier acting on the cage. */
	for (i=0; md; i++,md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!(md->mode&eModifierMode_Realtime)) continue;
		if (!(md->mode&eModifierMode_Editmode)) continue;
		if (mti->isDisabled && mti->isDisabled(md)) continue;
		if (!(mti->flags&eModifierTypeFlag_SupportsEditmode)) continue;

		if (!modifier_supportsMapping(md))
			break;

		if (lastPossibleCageIndex_r) *lastPossibleCageIndex_r = i;
		if (md->mode&eModifierMode_OnCage)
			cageIndex = i;
	}

	return cageIndex;
}


int modifiers_isSoftbodyEnabled(Object *ob)
{
	ModifierData *md = modifiers_findByType(ob, eModifierType_Softbody);

		/* Softbody not allowed in this situation, enforce! */
	if (md && ob->pd && ob->pd->deflect) {
		md->mode &= ~(eModifierMode_Realtime|eModifierMode_Render|eModifierMode_Editmode);
		md = NULL;
	}

	return (md && md->mode&(eModifierMode_Realtime|eModifierMode_Render));
}

ModifierData *modifiers_getVirtualModifierList(Object *ob)
{
		/* Kinda hacky, but should be fine since we are never
		 * reentrant and avoid free hassles.
		 */
	static ArmatureModifierData amd;
	static CurveModifierData cmd;
	static LatticeModifierData lmd;
	static int init = 1;

	if (init) {
		ModifierData *md;

		md = modifier_new(eModifierType_Armature);
		amd = *((ArmatureModifierData*) md);
		modifier_free(md);

		md = modifier_new(eModifierType_Curve);
		cmd = *((CurveModifierData*) md);
		modifier_free(md);

		md = modifier_new(eModifierType_Lattice);
		lmd = *((LatticeModifierData*) md);
		modifier_free(md);

		amd.modifier.mode |= eModifierMode_Virtual;
		cmd.modifier.mode |= eModifierMode_Virtual;
		lmd.modifier.mode |= eModifierMode_Virtual;

		init = 0;
	}

	if (ob->parent) {
		if(ob->parent->type==OB_ARMATURE && ob->partype==PARSKEL) {
			amd.object = ob->parent;
			amd.modifier.next = ob->modifiers.first;
			return &amd.modifier;
		} else if(ob->parent->type==OB_CURVE && ob->partype==PARSKEL) {
			cmd.object = ob->parent;
			cmd.modifier.next = ob->modifiers.first;
			return &cmd.modifier;
		} else if(ob->parent->type==OB_LATTICE && ob->partype==PARSKEL) {
			lmd.object = ob->parent;
			lmd.modifier.next = ob->modifiers.first;
			return &lmd.modifier;
		}
	}

	return ob->modifiers.first;
}
