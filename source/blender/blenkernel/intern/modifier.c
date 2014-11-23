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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Daniel Dunbar
 *                 Ton Roosendaal,
 *                 Ben Batt,
 *                 Brecht Van Lommel,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Modifier stack implementation.
 *
 * BKE_modifier.h contains the function prototypes for this file.
 *
 */

/** \file blender/blenkernel/intern/modifier.c
 *  \ingroup bke
 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_listbase.h"
#include "BLI_linklist.h"
#include "BLI_string.h"

#include "BLF_translation.h"

#include "BKE_appdir.h"
#include "BKE_key.h"
#include "BKE_multires.h"
#include "BKE_DerivedMesh.h"

/* may move these, only for modifier_path_relbase */
#include "BKE_global.h" /* ugh, G.main->name only */
#include "BKE_main.h"
/* end */

#include "MOD_modifiertypes.h"

static ModifierTypeInfo *modifier_types[NUM_MODIFIER_TYPES] = {NULL};
static VirtualModifierData virtualModifierCommonData;

void BKE_modifier_init(void)
{
	ModifierData *md;

	/* Initialize modifier types */
	modifier_type_init(modifier_types); /* MOD_utils.c */

	/* Initialize global cmmon storage used for virtual modifier list */
	md = modifier_new(eModifierType_Armature);
	virtualModifierCommonData.amd = *((ArmatureModifierData *) md);
	modifier_free(md);

	md = modifier_new(eModifierType_Curve);
	virtualModifierCommonData.cmd = *((CurveModifierData *) md);
	modifier_free(md);

	md = modifier_new(eModifierType_Lattice);
	virtualModifierCommonData.lmd = *((LatticeModifierData *) md);
	modifier_free(md);

	md = modifier_new(eModifierType_ShapeKey);
	virtualModifierCommonData.smd = *((ShapeKeyModifierData *) md);
	modifier_free(md);

	virtualModifierCommonData.amd.modifier.mode |= eModifierMode_Virtual;
	virtualModifierCommonData.cmd.modifier.mode |= eModifierMode_Virtual;
	virtualModifierCommonData.lmd.modifier.mode |= eModifierMode_Virtual;
	virtualModifierCommonData.smd.modifier.mode |= eModifierMode_Virtual;
}

ModifierTypeInfo *modifierType_getInfo(ModifierType type)
{
	/* type unsigned, no need to check < 0 */
	if (type < NUM_MODIFIER_TYPES && modifier_types[type]->name[0] != '\0') {
		return modifier_types[type];
	}
	else {
		return NULL;
	}
}

/***/

ModifierData *modifier_new(int type)
{
	ModifierTypeInfo *mti = modifierType_getInfo(type);
	ModifierData *md = MEM_callocN(mti->structSize, mti->structName);
	
	/* note, this name must be made unique later */
	BLI_strncpy(md->name, DATA_(mti->name), sizeof(md->name));

	md->type = type;
	md->mode = eModifierMode_Realtime | eModifierMode_Render | eModifierMode_Expanded;

	if (mti->flags & eModifierTypeFlag_EnableInEditmode)
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

void modifier_unique_name(ListBase *modifiers, ModifierData *md)
{
	if (modifiers && md) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		BLI_uniquename(modifiers, md, DATA_(mti->name), '.', offsetof(ModifierData, name), sizeof(md->name));
	}
}

bool modifier_dependsOnTime(ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	return mti->dependsOnTime && mti->dependsOnTime(md);
}

bool modifier_supportsMapping(ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	return (mti->type == eModifierTypeType_OnlyDeform ||
	        (mti->flags & eModifierTypeFlag_SupportsMapping));
}

bool modifier_isPreview(ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	/* Constructive modifiers are highly likely to also modify data like vgroups or vcol! */
	if (!((mti->flags & eModifierTypeFlag_UsesPreview) || (mti->type == eModifierTypeType_Constructive))) {
		return false;
	}

	if (md->mode & eModifierMode_Realtime) {
		return true;
	}

	return false;
}

ModifierData *modifiers_findByType(Object *ob, ModifierType type)
{
	ModifierData *md = ob->modifiers.first;

	for (; md; md = md->next)
		if (md->type == type)
			break;

	return md;
}

ModifierData *modifiers_findByName(Object *ob, const char *name)
{
	return BLI_findstring(&(ob->modifiers), name, offsetof(ModifierData, name));
}

void modifiers_clearErrors(Object *ob)
{
	ModifierData *md = ob->modifiers.first;
	/* int qRedraw = 0; */

	for (; md; md = md->next) {
		if (md->error) {
			MEM_freeN(md->error);
			md->error = NULL;

			/* qRedraw = 1; */
		}
	}
}

void modifiers_foreachObjectLink(Object *ob, ObjectWalkFunc walk,
                                 void *userData)
{
	ModifierData *md = ob->modifiers.first;

	for (; md; md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->foreachObjectLink)
			mti->foreachObjectLink(md, ob, walk, userData);
	}
}

void modifiers_foreachIDLink(Object *ob, IDWalkFunc walk, void *userData)
{
	ModifierData *md = ob->modifiers.first;

	for (; md; md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->foreachIDLink) mti->foreachIDLink(md, ob, walk, userData);
		else if (mti->foreachObjectLink) {
			/* each Object can masquerade as an ID, so this should be OK */
			ObjectWalkFunc fp = (ObjectWalkFunc)walk;
			mti->foreachObjectLink(md, ob, fp, userData);
		}
	}
}

void modifiers_foreachTexLink(Object *ob, TexWalkFunc walk, void *userData)
{
	ModifierData *md = ob->modifiers.first;

	for (; md; md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->foreachTexLink)
			mti->foreachTexLink(md, ob, walk, userData);
	}
}

/* callback's can use this
 * to avoid copying every member.
 */
void modifier_copyData_generic(const ModifierData *md_src, ModifierData *md_dst)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md_src->type);
	const size_t data_size = sizeof(ModifierData);
	const char *md_src_data = ((char *)md_src) + data_size;
	char       *md_dst_data = ((char *)md_dst) + data_size;
	BLI_assert(data_size <= (size_t)mti->structSize);
	memcpy(md_dst_data, md_src_data, (size_t)mti->structSize - data_size);
}

void modifier_copyData(ModifierData *md, ModifierData *target)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	target->mode = md->mode;

	if (mti->copyData)
		mti->copyData(md, target);
}


bool modifier_supportsCage(struct Scene *scene, ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	md->scene = scene;

	return ((!mti->isDisabled || !mti->isDisabled(md, 0)) &&
	        (mti->flags & eModifierTypeFlag_SupportsEditmode) &&
	        modifier_supportsMapping(md));
}

bool modifier_couldBeCage(struct Scene *scene, ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	md->scene = scene;

	return ((md->mode & eModifierMode_Realtime) &&
	        (md->mode & eModifierMode_Editmode) &&
	        (!mti->isDisabled || !mti->isDisabled(md, 0)) &&
	        modifier_supportsMapping(md));
}

bool modifier_isSameTopology(ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	return ELEM(mti->type, eModifierTypeType_OnlyDeform, eModifierTypeType_NonGeometrical);
}

bool modifier_isNonGeometrical(ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	return (mti->type == eModifierTypeType_NonGeometrical);
}

void modifier_setError(ModifierData *md, const char *_format, ...)
{
	char buffer[512];
	va_list ap;
	const char *format = TIP_(_format);

	va_start(ap, _format);
	vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);
	buffer[sizeof(buffer) - 1] = '\0';

	if (md->error)
		MEM_freeN(md->error);

	md->error = BLI_strdup(buffer);

}

/* used for buttons, to find out if the 'draw deformed in editmode' option is
 * there
 * 
 * also used in transform_conversion.c, to detect CrazySpace [tm] (2nd arg
 * then is NULL) 
 * also used for some mesh tools to give warnings
 */
int modifiers_getCageIndex(struct Scene *scene, Object *ob, int *r_lastPossibleCageIndex, bool is_virtual)
{
	VirtualModifierData virtualModifierData;
	ModifierData *md = (is_virtual) ? modifiers_getVirtualModifierList(ob, &virtualModifierData) : ob->modifiers.first;
	int i, cageIndex = -1;

	if (r_lastPossibleCageIndex) {
		/* ensure the value is initialized */
		*r_lastPossibleCageIndex = -1;
	}

	/* Find the last modifier acting on the cage. */
	for (i = 0; md; i++, md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);
		bool supports_mapping;

		md->scene = scene;

		if (mti->isDisabled && mti->isDisabled(md, 0)) continue;
		if (!(mti->flags & eModifierTypeFlag_SupportsEditmode)) continue;
		if (md->mode & eModifierMode_DisableTemporary) continue;

		supports_mapping = modifier_supportsMapping(md);
		if (r_lastPossibleCageIndex && supports_mapping) {
			*r_lastPossibleCageIndex = i;
		}

		if (!(md->mode & eModifierMode_Realtime)) continue;
		if (!(md->mode & eModifierMode_Editmode)) continue;

		if (!supports_mapping)
			break;

		if (md->mode & eModifierMode_OnCage)
			cageIndex = i;
	}

	return cageIndex;
}


bool modifiers_isSoftbodyEnabled(Object *ob)
{
	ModifierData *md = modifiers_findByType(ob, eModifierType_Softbody);

	return (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render));
}

bool modifiers_isClothEnabled(Object *ob)
{
	ModifierData *md = modifiers_findByType(ob, eModifierType_Cloth);

	return (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render));
}

bool modifiers_isModifierEnabled(Object *ob, int modifierType)
{
	ModifierData *md = modifiers_findByType(ob, modifierType);

	return (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render));
}

bool modifiers_isParticleEnabled(Object *ob)
{
	ModifierData *md = modifiers_findByType(ob, eModifierType_ParticleSystem);

	return (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render));
}

bool modifier_isEnabled(struct Scene *scene, ModifierData *md, int required_mode)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	md->scene = scene;

	if ((md->mode & required_mode) != required_mode) return false;
	if (mti->isDisabled && mti->isDisabled(md, required_mode == eModifierMode_Render)) return false;
	if (md->mode & eModifierMode_DisableTemporary) return false;
	if ((required_mode & eModifierMode_Editmode) && !(mti->flags & eModifierTypeFlag_SupportsEditmode)) return false;
	
	return true;
}

CDMaskLink *modifiers_calcDataMasks(struct Scene *scene, Object *ob, ModifierData *md,
                                    CustomDataMask dataMask, int required_mode,
                                    ModifierData *previewmd, CustomDataMask previewmask)
{
	CDMaskLink *dataMasks = NULL;
	CDMaskLink *curr, *prev;

	/* build a list of modifier data requirements in reverse order */
	for (; md; md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		curr = MEM_callocN(sizeof(CDMaskLink), "CDMaskLink");
		
		if (modifier_isEnabled(scene, md, required_mode)) {
			if (mti->requiredDataMask)
				curr->mask = mti->requiredDataMask(ob, md);

			if (previewmd == md) {
				curr->mask |= previewmask;
			}
		}

		/* prepend new datamask */
		curr->next = dataMasks;
		dataMasks = curr;
	}

	/* build the list of required data masks - each mask in the list must
	 * include all elements of the masks that follow it
	 *
	 * note the list is currently in reverse order, so "masks that follow it"
	 * actually means "masks that precede it" at the moment
	 */
	for (curr = dataMasks, prev = NULL; curr; prev = curr, curr = curr->next) {
		if (prev) {
			CustomDataMask prev_mask = prev->mask;
			CustomDataMask curr_mask = curr->mask;

			curr->mask = curr_mask | prev_mask;
		}
		else {
			CustomDataMask curr_mask = curr->mask;

			curr->mask = curr_mask | dataMask;
		}
	}

	/* reverse the list so it's in the correct order */
	BLI_linklist_reverse((LinkNode **)&dataMasks);

	return dataMasks;
}

ModifierData *modifiers_getLastPreview(struct Scene *scene, ModifierData *md, int required_mode)
{
	ModifierData *tmp_md = NULL;

	if ((required_mode & ~eModifierMode_Editmode) != eModifierMode_Realtime)
		return tmp_md;

	/* Find the latest modifier in stack generating preview. */
	for (; md; md = md->next) {
		if (modifier_isEnabled(scene, md, required_mode) && modifier_isPreview(md))
			tmp_md = md;
	}
	return tmp_md;
}

/* NOTE: This is to support old files from before Blender supported modifiers,
 * in some cases versioning code updates these so for new files this will
 * return an empty list. */
ModifierData *modifiers_getVirtualModifierList(Object *ob, VirtualModifierData *virtualModifierData)
{
	ModifierData *md;

	md = ob->modifiers.first;

	*virtualModifierData = virtualModifierCommonData;

	if (ob->parent) {
		if (ob->parent->type == OB_ARMATURE && ob->partype == PARSKEL) {
			virtualModifierData->amd.object = ob->parent;
			virtualModifierData->amd.modifier.next = md;
			virtualModifierData->amd.deformflag = ((bArmature *)(ob->parent->data))->deformflag;
			md = &virtualModifierData->amd.modifier;
		}
		else if (ob->parent->type == OB_CURVE && ob->partype == PARSKEL) {
			virtualModifierData->cmd.object = ob->parent;
			virtualModifierData->cmd.defaxis = ob->trackflag + 1;
			virtualModifierData->cmd.modifier.next = md;
			md = &virtualModifierData->cmd.modifier;
		}
		else if (ob->parent->type == OB_LATTICE && ob->partype == PARSKEL) {
			virtualModifierData->lmd.object = ob->parent;
			virtualModifierData->lmd.modifier.next = md;
			md = &virtualModifierData->lmd.modifier;
		}
	}

	/* shape key modifier, not yet for curves */
	if (ELEM(ob->type, OB_MESH, OB_LATTICE) && BKE_key_from_object(ob)) {
		if (ob->type == OB_MESH && (ob->shapeflag & OB_SHAPE_EDIT_MODE))
			virtualModifierData->smd.modifier.mode |= eModifierMode_Editmode | eModifierMode_OnCage;
		else
			virtualModifierData->smd.modifier.mode &= ~eModifierMode_Editmode | eModifierMode_OnCage;

		virtualModifierData->smd.modifier.next = md;
		md = &virtualModifierData->smd.modifier;
	}

	return md;
}

/* Takes an object and returns its first selected armature, else just its armature
 * This should work for multiple armatures per object
 */
Object *modifiers_isDeformedByArmature(Object *ob)
{
	VirtualModifierData virtualModifierData;
	ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
	ArmatureModifierData *amd = NULL;
	
	/* return the first selected armature, this lets us use multiple armatures */
	for (; md; md = md->next) {
		if (md->type == eModifierType_Armature) {
			amd = (ArmatureModifierData *) md;
			if (amd->object && (amd->object->flag & SELECT))
				return amd->object;
		}
	}
	
	if (amd) /* if were still here then return the last armature */
		return amd->object;
	
	return NULL;
}

/* Takes an object and returns its first selected lattice, else just its lattice
 * This should work for multiple lattices per object
 */
Object *modifiers_isDeformedByLattice(Object *ob)
{
	VirtualModifierData virtualModifierData;
	ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
	LatticeModifierData *lmd = NULL;
	
	/* return the first selected lattice, this lets us use multiple lattices */
	for (; md; md = md->next) {
		if (md->type == eModifierType_Lattice) {
			lmd = (LatticeModifierData *) md;
			if (lmd->object && (lmd->object->flag & SELECT))
				return lmd->object;
		}
	}
	
	if (lmd) /* if were still here then return the last lattice */
		return lmd->object;
	
	return NULL;
}

/* Takes an object and returns its first selected curve, else just its curve
 * This should work for multiple curves per object
 */
Object *modifiers_isDeformedByCurve(Object *ob)
{
	VirtualModifierData virtualModifierData;
	ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
	CurveModifierData *cmd = NULL;
	
	/* return the first selected curve, this lets us use multiple curves */
	for (; md; md = md->next) {
		if (md->type == eModifierType_Curve) {
			cmd = (CurveModifierData *) md;
			if (cmd->object && (cmd->object->flag & SELECT))
				return cmd->object;
		}
	}
	
	if (cmd) /* if were still here then return the last curve */
		return cmd->object;
	
	return NULL;
}

bool modifiers_usesArmature(Object *ob, bArmature *arm)
{
	VirtualModifierData virtualModifierData;
	ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

	for (; md; md = md->next) {
		if (md->type == eModifierType_Armature) {
			ArmatureModifierData *amd = (ArmatureModifierData *) md;
			if (amd->object && amd->object->data == arm)
				return true;
		}
	}

	return false;
}

bool modifier_isCorrectableDeformed(ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	return (mti->deformMatricesEM != NULL);
}

bool modifiers_isCorrectableDeformed(struct Scene *scene, Object *ob)
{
	VirtualModifierData virtualModifierData;
	ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
	int required_mode = eModifierMode_Realtime;

	if (ob->mode == OB_MODE_EDIT)
		required_mode |= eModifierMode_Editmode;
	
	for (; md; md = md->next) {
		if (!modifier_isEnabled(scene, md, required_mode)) {
			/* pass */
		}
		else if (modifier_isCorrectableDeformed(md)) {
			return true;
		}
	}
	return false;
}

/* Check whether the given object has a modifier in its stack that uses WEIGHT_MCOL CD layer
 * to preview something... Used by DynamicPaint and WeightVG currently. */
bool modifiers_isPreview(Object *ob)
{
	ModifierData *md = ob->modifiers.first;

	for (; md; md = md->next) {
		if (modifier_isPreview(md))
			return true;
	}

	return false;
}

void modifier_freeTemporaryData(ModifierData *md)
{
	if (md->type == eModifierType_Armature) {
		ArmatureModifierData *amd = (ArmatureModifierData *)md;

		if (amd->prevCos) {
			MEM_freeN(amd->prevCos);
			amd->prevCos = NULL;
		}
	}
}

/* ensure modifier correctness when changing ob->data */
void test_object_modifiers(Object *ob)
{
	ModifierData *md;

	/* just multires checked for now, since only multires
	 * modifies mesh data */

	if (ob->type != OB_MESH) return;

	for (md = ob->modifiers.first; md; md = md->next) {
		if (md->type == eModifierType_Multires) {
			MultiresModifierData *mmd = (MultiresModifierData *)md;

			multiresModifier_set_levels_from_disps(mmd, ob);
		}
	}
}

/* where should this go?, it doesnt fit well anywhere :S - campbell */

/* elubie: changed this to default to the same dir as the render output
 * to prevent saving to C:\ on Windows */

/* campbell: logic behind this...
 *
 * - if the ID is from a library, return library path
 * - else if the file has been saved return the blend file path.
 * - else if the file isn't saved and the ID isn't from a library, return the temp dir.
 */
const char *modifier_path_relbase(Object *ob)
{
	if (G.relbase_valid || ob->id.lib) {
		return ID_BLEND_PATH(G.main, &ob->id);
	}
	else {
		/* last resort, better then using "" which resolves to the current
		 * working directory */
		return BKE_tempdir_session();
	}
}

/* initializes the path with either */
void modifier_path_init(char *path, int path_maxlen, const char *name)
{
	/* elubie: changed this to default to the same dir as the render output
	 * to prevent saving to C:\ on Windows */
	BLI_join_dirfile(path, path_maxlen,
	                 G.relbase_valid ? "//" : BKE_tempdir_session(),
	                 name);
}


/* wrapper around ModifierTypeInfo.applyModifier that ensures valid normals */

struct DerivedMesh *modwrap_applyModifier(
        ModifierData *md, Object *ob,
        struct DerivedMesh *dm,
        ModifierApplyFlag flag)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	BLI_assert(CustomData_has_layer(&dm->polyData, CD_NORMAL) == false);

	if (mti->dependsOnNormals && mti->dependsOnNormals(md)) {
		DM_ensure_normals(dm);
	}
	return mti->applyModifier(md, ob, dm, flag);
}

struct DerivedMesh *modwrap_applyModifierEM(
        ModifierData *md, Object *ob,
        struct BMEditMesh *em,
        DerivedMesh *dm,
        ModifierApplyFlag flag)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	BLI_assert(CustomData_has_layer(&dm->polyData, CD_NORMAL) == false);

	if (mti->dependsOnNormals && mti->dependsOnNormals(md)) {
		DM_ensure_normals(dm);
	}
	return mti->applyModifierEM(md, ob, em, dm, flag);
}

void modwrap_deformVerts(
        ModifierData *md, Object *ob,
        DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts,
        ModifierApplyFlag flag)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	BLI_assert(!dm || CustomData_has_layer(&dm->polyData, CD_NORMAL) == false);

	if (dm && mti->dependsOnNormals && mti->dependsOnNormals(md)) {
		DM_ensure_normals(dm);
	}
	mti->deformVerts(md, ob, dm, vertexCos, numVerts, flag);
}

void modwrap_deformVertsEM(
        ModifierData *md, Object *ob,
        struct BMEditMesh *em, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	BLI_assert(!dm || CustomData_has_layer(&dm->polyData, CD_NORMAL) == false);

	if (dm && mti->dependsOnNormals && mti->dependsOnNormals(md)) {
		DM_ensure_normals(dm);
	}
	mti->deformVertsEM(md, ob, em, dm, vertexCos, numVerts);
}
/* end modifier callback wrappers */
