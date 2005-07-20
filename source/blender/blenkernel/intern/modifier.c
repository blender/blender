#include "string.h"

#include "BLI_rand.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

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

/***/

static void *allocModifierData(int type, int size)
{
	ModifierData *md = MEM_callocN(size, "md");
	md->type = type;
	md->mode = eModifierMode_RealtimeAndRender;

	return md;
}

static ModifierData *noneModifier_allocData(void)
{
	return allocModifierData(eModifierType_None, sizeof(ModifierData));
}

static int noneModifier_isDisabled(ModifierData *md)
{
	return 1;
}

/* Curve */

static ModifierData *curveModifier_allocData(void)
{
	return allocModifierData(eModifierType_Curve, sizeof(CurveModifierData));
}

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

static void curveModifier_deformVerts(ModifierData *md, Object *ob, float (*vertexCos)[3], int numVerts)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	curve_deform_verts(cmd->object, ob, vertexCos, numVerts);
}

/* Lattice */

static ModifierData *latticeModifier_allocData(void)
{
	return allocModifierData(eModifierType_Lattice, sizeof(LatticeModifierData));
}

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

static void latticeModifier_deformVerts(ModifierData *md, Object *ob, float (*vertexCos)[3], int numVerts)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	lattice_deform_verts(lmd->object, ob, vertexCos, numVerts);
}

/* Subsurf */

static ModifierData *subsurfModifier_allocData(void)
{
	SubsurfModifierData *smd = allocModifierData(eModifierType_Subsurf, sizeof(SubsurfModifierData));

	smd->levels = 1;
	smd->renderLevels = 2;

	return (ModifierData*) smd;
}

static int subsurfModifier_isDisabled(ModifierData *md)
{
	return 0;
}

static void *subsurfModifier_applyModifier(ModifierData *md, Object *ob, DerivedMesh *dm, float (*vertexCos)[3], int useRenderParams)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;
	int levels = useRenderParams?smd->renderLevels:smd->levels;
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

		dm = subsurf_make_derived_from_dlm(dlm, smd->subdivType, levels);
		displistmesh_free(dlm);

		return dm;
	} else {
		return subsurf_make_derived_from_mesh(me, smd->subdivType, levels, vertexCos);
	}
}

/* Build */

static ModifierData *buildModifier_allocData(void)
{
	BuildModifierData *bmd = allocModifierData(eModifierType_Build, sizeof(BuildModifierData));

	bmd->start = 1.0;
	bmd->length = 100.0;

	return (ModifierData*) bmd;
}

static int buildModifier_isDisabled(ModifierData *md)
{
	return 0;
}

static int buildModifier_dependsOnTime(ModifierData *md)
{
	return 1;
}

static void *buildModifier_applyModifier(ModifierData *md, Object *ob, DerivedMesh *dm, float (*vertexCos)[3], int useRenderParams)
{
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

		ndlm->mvert = MEM_mallocN(sizeof(*ndlm->mvert)*ndlm->totvert, "build_mvert");
		memcpy(ndlm->mvert, mvert, sizeof(*mvert)*ndlm->totvert);
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

/***/

static ModifierTypeInfo typeArr[NUM_MODIFIER_TYPES];
static int typeArrInit = 1;

ModifierTypeInfo *modifierType_get_info(ModifierType type)
{
	if (typeArrInit) {
		ModifierTypeInfo *mti;

		memset(typeArr, 0, sizeof(typeArr));

		mti = &typeArr[eModifierType_None];
		strcpy(mti->name, "None");
		strcpy(mti->structName, "ModifierData");
		mti->type = eModifierType_None;
		mti->flags = eModifierTypeFlag_AcceptsMesh|eModifierTypeFlag_AcceptsCVs;
		mti->allocData = noneModifier_allocData;
		mti->isDisabled = noneModifier_isDisabled;

		mti = &typeArr[eModifierType_Curve];
		strcpy(mti->name, "Curve");
		strcpy(mti->structName, "CurveModifierData");
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs;
		mti->allocData = curveModifier_allocData;
		mti->isDisabled = curveModifier_isDisabled;
		mti->updateDepgraph = curveModifier_updateDepgraph;
		mti->deformVerts = curveModifier_deformVerts;

		mti = &typeArr[eModifierType_Lattice];
		strcpy(mti->name, "Lattice");
		strcpy(mti->structName, "LatticeModifierData");
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs;
		mti->allocData = latticeModifier_allocData;
		mti->isDisabled = latticeModifier_isDisabled;
		mti->updateDepgraph = latticeModifier_updateDepgraph;
		mti->deformVerts = latticeModifier_deformVerts;

		mti = &typeArr[eModifierType_Subsurf];
		strcpy(mti->name, "Subsurf");
		strcpy(mti->structName, "SubsurfModifierData");
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh|eModifierTypeFlag_SupportsMapping;
		mti->allocData = subsurfModifier_allocData;
		mti->isDisabled = subsurfModifier_isDisabled;
		mti->applyModifier = subsurfModifier_applyModifier;

		mti = &typeArr[eModifierType_Build];
		strcpy(mti->name, "Build");
		strcpy(mti->structName, "BuildModifierData");
		mti->type = eModifierTypeType_Nonconstructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh;
		mti->allocData = buildModifier_allocData;
		mti->isDisabled = buildModifier_isDisabled;
		mti->dependsOnTime = buildModifier_dependsOnTime;
		mti->applyModifier = buildModifier_applyModifier;

		typeArrInit = 0;
	}

	if (type>=0 && type<NUM_MODIFIER_TYPES && typeArr[type].name[0]!='\0') {
		return &typeArr[type];
	} else {
		return NULL;
	}
}

int modifier_dependsOnTime(ModifierData *md) 
{
	ModifierTypeInfo *mti = modifierType_get_info(md->type);

	return mti->dependsOnTime && mti->dependsOnTime(md);
}
