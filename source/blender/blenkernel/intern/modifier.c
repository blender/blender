#include "string.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_modifier.h"
#include "BKE_lattice.h"
#include "BKE_subsurf.h"

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

static void *subsurfModifier_applyModifier(ModifierData *md, void *data, Object *ob, DerivedMesh *dm, float (*vertexCos)[3], int useRenderParams)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;
	int levels = useRenderParams?smd->renderLevels:smd->levels;
	Mesh *me = data;

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
		mti->deformVerts = curveModifier_deformVerts;

		mti = &typeArr[eModifierType_Lattice];
		strcpy(mti->name, "Lattice");
		strcpy(mti->structName, "LatticeModifierData");
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs;
		mti->allocData = latticeModifier_allocData;
		mti->isDisabled = latticeModifier_isDisabled;
		mti->deformVerts = latticeModifier_deformVerts;

		mti = &typeArr[eModifierType_Subsurf];
		strcpy(mti->name, "Subsurf");
		strcpy(mti->structName, "SubsurfModifierData");
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh|eModifierTypeFlag_SupportsMapping;
		mti->allocData = subsurfModifier_allocData;
		mti->isDisabled = subsurfModifier_isDisabled;
		mti->applyModifier = subsurfModifier_applyModifier;

		typeArrInit = 0;
	}

	if (type>=0 && type<NUM_MODIFIER_TYPES && typeArr[type].name[0]!='\0') {
		return &typeArr[type];
	} else {
		return NULL;
	}
}

