/*
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
 * Modifier stack implementation.
 *
 * BKE_modifier.h contains the function prototypes for this file.
 */

/** \file
 * \ingroup bke
 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_linklist.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "BKE_appdir.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_idcode.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_DerivedMesh.h"

/* may move these, only for modifier_path_relbase */
#include "BKE_main.h"
/* end */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.modifier"};
static ModifierTypeInfo *modifier_types[NUM_MODIFIER_TYPES] = {NULL};
static VirtualModifierData virtualModifierCommonData;

void BKE_modifier_init(void)
{
  ModifierData *md;

  /* Initialize modifier types */
  modifier_type_init(modifier_types); /* MOD_utils.c */

  /* Initialize global cmmon storage used for virtual modifier list */
  md = modifier_new(eModifierType_Armature);
  virtualModifierCommonData.amd = *((ArmatureModifierData *)md);
  modifier_free(md);

  md = modifier_new(eModifierType_Curve);
  virtualModifierCommonData.cmd = *((CurveModifierData *)md);
  modifier_free(md);

  md = modifier_new(eModifierType_Lattice);
  virtualModifierCommonData.lmd = *((LatticeModifierData *)md);
  modifier_free(md);

  md = modifier_new(eModifierType_ShapeKey);
  virtualModifierCommonData.smd = *((ShapeKeyModifierData *)md);
  modifier_free(md);

  virtualModifierCommonData.amd.modifier.mode |= eModifierMode_Virtual;
  virtualModifierCommonData.cmd.modifier.mode |= eModifierMode_Virtual;
  virtualModifierCommonData.lmd.modifier.mode |= eModifierMode_Virtual;
  virtualModifierCommonData.smd.modifier.mode |= eModifierMode_Virtual;
}

const ModifierTypeInfo *modifierType_getInfo(ModifierType type)
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
  const ModifierTypeInfo *mti = modifierType_getInfo(type);
  ModifierData *md = MEM_callocN(mti->structSize, mti->structName);

  /* note, this name must be made unique later */
  BLI_strncpy(md->name, DATA_(mti->name), sizeof(md->name));

  md->type = type;
  md->mode = eModifierMode_Realtime | eModifierMode_Render | eModifierMode_Expanded;
  md->flag = eModifierFlag_OverrideLibrary_Local;

  if (mti->flags & eModifierTypeFlag_EnableInEditmode) {
    md->mode |= eModifierMode_Editmode;
  }

  if (mti->initData) {
    mti->initData(md);
  }

  return md;
}

static void modifier_free_data_id_us_cb(void *UNUSED(userData),
                                        Object *UNUSED(ob),
                                        ID **idpoin,
                                        int cb_flag)
{
  ID *id = *idpoin;
  if (id != NULL && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_min(id);
  }
}

void modifier_free_ex(ModifierData *md, const int flag)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (mti->foreachIDLink) {
      mti->foreachIDLink(md, NULL, modifier_free_data_id_us_cb, NULL);
    }
    else if (mti->foreachObjectLink) {
      mti->foreachObjectLink(md, NULL, (ObjectWalkFunc)modifier_free_data_id_us_cb, NULL);
    }
  }

  if (mti->freeData) {
    mti->freeData(md);
  }
  if (md->error) {
    MEM_freeN(md->error);
  }

  MEM_freeN(md);
}

void modifier_free(ModifierData *md)
{
  modifier_free_ex(md, 0);
}

bool modifier_unique_name(ListBase *modifiers, ModifierData *md)
{
  if (modifiers && md) {
    const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

    return BLI_uniquename(
        modifiers, md, DATA_(mti->name), '.', offsetof(ModifierData, name), sizeof(md->name));
  }
  return false;
}

bool modifier_dependsOnTime(ModifierData *md)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

  return mti->dependsOnTime && mti->dependsOnTime(md);
}

bool modifier_supportsMapping(ModifierData *md)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

  return (mti->type == eModifierTypeType_OnlyDeform ||
          (mti->flags & eModifierTypeFlag_SupportsMapping));
}

bool modifier_isPreview(ModifierData *md)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

  /* Constructive modifiers are highly likely to also modify data like vgroups or vcol! */
  if (!((mti->flags & eModifierTypeFlag_UsesPreview) ||
        (mti->type == eModifierTypeType_Constructive))) {
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

  for (; md; md = md->next) {
    if (md->type == type) {
      break;
    }
  }

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

void modifiers_foreachObjectLink(Object *ob, ObjectWalkFunc walk, void *userData)
{
  ModifierData *md = ob->modifiers.first;

  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

    if (mti->foreachObjectLink) {
      mti->foreachObjectLink(md, ob, walk, userData);
    }
  }
}

void modifiers_foreachIDLink(Object *ob, IDWalkFunc walk, void *userData)
{
  ModifierData *md = ob->modifiers.first;

  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

    if (mti->foreachIDLink) {
      mti->foreachIDLink(md, ob, walk, userData);
    }
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
    const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

    if (mti->foreachTexLink) {
      mti->foreachTexLink(md, ob, walk, userData);
    }
  }
}

/* callback's can use this
 * to avoid copying every member.
 */
void modifier_copyData_generic(const ModifierData *md_src,
                               ModifierData *md_dst,
                               const int UNUSED(flag))
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md_src->type);

  /* md_dst may have already be fully initialized with some extra allocated data,
   * we need to free it now to avoid memleak. */
  if (mti->freeData) {
    mti->freeData(md_dst);
  }

  const size_t data_size = sizeof(ModifierData);
  const char *md_src_data = ((const char *)md_src) + data_size;
  char *md_dst_data = ((char *)md_dst) + data_size;
  BLI_assert(data_size <= (size_t)mti->structSize);
  memcpy(md_dst_data, md_src_data, (size_t)mti->structSize - data_size);

  /* Runtime fields are never to be preserved. */
  md_dst->runtime = NULL;
}

static void modifier_copy_data_id_us_cb(void *UNUSED(userData),
                                        Object *UNUSED(ob),
                                        ID **idpoin,
                                        int cb_flag)
{
  ID *id = *idpoin;
  if (id != NULL && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_plus(id);
  }
}

void modifier_copyData_ex(ModifierData *md, ModifierData *target, const int flag)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

  target->mode = md->mode;
  target->flag = md->flag;

  if (mti->copyData) {
    mti->copyData(md, target, flag);
  }

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (mti->foreachIDLink) {
      mti->foreachIDLink(target, NULL, modifier_copy_data_id_us_cb, NULL);
    }
    else if (mti->foreachObjectLink) {
      mti->foreachObjectLink(target, NULL, (ObjectWalkFunc)modifier_copy_data_id_us_cb, NULL);
    }
  }
}

void modifier_copyData(ModifierData *md, ModifierData *target)
{
  modifier_copyData_ex(md, target, 0);
}

bool modifier_supportsCage(struct Scene *scene, ModifierData *md)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

  return ((!mti->isDisabled || !mti->isDisabled(scene, md, 0)) &&
          (mti->flags & eModifierTypeFlag_SupportsEditmode) && modifier_supportsMapping(md));
}

bool modifier_couldBeCage(struct Scene *scene, ModifierData *md)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

  return ((md->mode & eModifierMode_Realtime) && (md->mode & eModifierMode_Editmode) &&
          (!mti->isDisabled || !mti->isDisabled(scene, md, 0)) && modifier_supportsMapping(md));
}

bool modifier_isSameTopology(ModifierData *md)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
  return ELEM(mti->type, eModifierTypeType_OnlyDeform, eModifierTypeType_NonGeometrical);
}

bool modifier_isNonGeometrical(ModifierData *md)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
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

  if (md->error) {
    MEM_freeN(md->error);
  }

  md->error = BLI_strdup(buffer);

  CLOG_STR_ERROR(&LOG, md->error);
}

/* used for buttons, to find out if the 'draw deformed in editmode' option is
 * there
 *
 * also used in transform_conversion.c, to detect CrazySpace [tm] (2nd arg
 * then is NULL)
 * also used for some mesh tools to give warnings
 */
int modifiers_getCageIndex(struct Scene *scene,
                           Object *ob,
                           int *r_lastPossibleCageIndex,
                           bool is_virtual)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = (is_virtual) ? modifiers_getVirtualModifierList(ob, &virtualModifierData) :
                                    ob->modifiers.first;
  int i, cageIndex = -1;

  if (r_lastPossibleCageIndex) {
    /* ensure the value is initialized */
    *r_lastPossibleCageIndex = -1;
  }

  /* Find the last modifier acting on the cage. */
  for (i = 0; md; i++, md = md->next) {
    const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
    bool supports_mapping;

    if (mti->isDisabled && mti->isDisabled(scene, md, 0)) {
      continue;
    }
    if (!(mti->flags & eModifierTypeFlag_SupportsEditmode)) {
      continue;
    }
    if (md->mode & eModifierMode_DisableTemporary) {
      continue;
    }

    supports_mapping = modifier_supportsMapping(md);
    if (r_lastPossibleCageIndex && supports_mapping) {
      *r_lastPossibleCageIndex = i;
    }

    if (!(md->mode & eModifierMode_Realtime)) {
      continue;
    }
    if (!(md->mode & eModifierMode_Editmode)) {
      continue;
    }

    if (!supports_mapping) {
      break;
    }

    if (md->mode & eModifierMode_OnCage) {
      cageIndex = i;
    }
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

/**
 * Check whether is enabled.
 *
 * \param scene: Current scene, may be NULL,
 * in which case isDisabled callback of the modifier is never called.
 */
bool modifier_isEnabled(const struct Scene *scene, ModifierData *md, int required_mode)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

  if ((md->mode & required_mode) != required_mode) {
    return false;
  }
  if (scene != NULL && mti->isDisabled &&
      mti->isDisabled(scene, md, required_mode == eModifierMode_Render)) {
    return false;
  }
  if (md->mode & eModifierMode_DisableTemporary) {
    return false;
  }
  if ((required_mode & eModifierMode_Editmode) &&
      !(mti->flags & eModifierTypeFlag_SupportsEditmode)) {
    return false;
  }

  return true;
}

CDMaskLink *modifiers_calcDataMasks(struct Scene *scene,
                                    Object *ob,
                                    ModifierData *md,
                                    CustomData_MeshMasks *final_datamask,
                                    int required_mode,
                                    ModifierData *previewmd,
                                    const CustomData_MeshMasks *previewmask)
{
  CDMaskLink *dataMasks = NULL;
  CDMaskLink *curr, *prev;
  bool have_deform_modifier = false;

  /* build a list of modifier data requirements in reverse order */
  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

    curr = MEM_callocN(sizeof(CDMaskLink), "CDMaskLink");

    if (modifier_isEnabled(scene, md, required_mode)) {
      if (mti->type == eModifierTypeType_OnlyDeform) {
        have_deform_modifier = true;
      }

      if (mti->requiredDataMask) {
        mti->requiredDataMask(ob, md, &curr->mask);
      }

      if (previewmd == md && previewmask != NULL) {
        CustomData_MeshMasks_update(&curr->mask, previewmask);
      }
    }

    if (!have_deform_modifier) {
      /* Don't create orco layer when there is no deformation, we fall
       * back to regular vertex coordinates */
      curr->mask.vmask &= ~CD_MASK_ORCO;
    }

    /* prepend new datamask */
    curr->next = dataMasks;
    dataMasks = curr;
  }

  if (!have_deform_modifier) {
    final_datamask->vmask &= ~CD_MASK_ORCO;
  }

  /* build the list of required data masks - each mask in the list must
   * include all elements of the masks that follow it
   *
   * note the list is currently in reverse order, so "masks that follow it"
   * actually means "masks that precede it" at the moment
   */
  for (curr = dataMasks, prev = NULL; curr; prev = curr, curr = curr->next) {
    if (prev) {
      CustomData_MeshMasks_update(&curr->mask, &prev->mask);
    }
    else {
      CustomData_MeshMasks_update(&curr->mask, final_datamask);
    }
  }

  /* reverse the list so it's in the correct order */
  BLI_linklist_reverse((LinkNode **)&dataMasks);

  return dataMasks;
}

ModifierData *modifiers_getLastPreview(struct Scene *scene, ModifierData *md, int required_mode)
{
  ModifierData *tmp_md = NULL;

  if ((required_mode & ~eModifierMode_Editmode) != eModifierMode_Realtime) {
    return tmp_md;
  }

  /* Find the latest modifier in stack generating preview. */
  for (; md; md = md->next) {
    if (modifier_isEnabled(scene, md, required_mode) && modifier_isPreview(md)) {
      tmp_md = md;
    }
  }
  return tmp_md;
}

/* NOTE: This is to support old files from before Blender supported modifiers,
 * in some cases versioning code updates these so for new files this will
 * return an empty list. */
ModifierData *modifiers_getVirtualModifierList(Object *ob,
                                               VirtualModifierData *virtualModifierData)
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
    if (ob->type == OB_MESH && (ob->shapeflag & OB_SHAPE_EDIT_MODE)) {
      virtualModifierData->smd.modifier.mode |= eModifierMode_Editmode | eModifierMode_OnCage;
    }
    else {
      virtualModifierData->smd.modifier.mode &= ~eModifierMode_Editmode | eModifierMode_OnCage;
    }

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
      amd = (ArmatureModifierData *)md;
      if (amd->object && (amd->object->base_flag & BASE_SELECTED)) {
        return amd->object;
      }
    }
  }

  if (amd) { /* if were still here then return the last armature */
    return amd->object;
  }

  return NULL;
}

Object *modifiers_isDeformedByMeshDeform(Object *ob)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
  MeshDeformModifierData *mdmd = NULL;

  /* return the first selected armature, this lets us use multiple armatures */
  for (; md; md = md->next) {
    if (md->type == eModifierType_MeshDeform) {
      mdmd = (MeshDeformModifierData *)md;
      if (mdmd->object && (mdmd->object->base_flag & BASE_SELECTED)) {
        return mdmd->object;
      }
    }
  }

  if (mdmd) { /* if were still here then return the last armature */
    return mdmd->object;
  }

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
      lmd = (LatticeModifierData *)md;
      if (lmd->object && (lmd->object->base_flag & BASE_SELECTED)) {
        return lmd->object;
      }
    }
  }

  if (lmd) { /* if were still here then return the last lattice */
    return lmd->object;
  }

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
      cmd = (CurveModifierData *)md;
      if (cmd->object && (cmd->object->base_flag & BASE_SELECTED)) {
        return cmd->object;
      }
    }
  }

  if (cmd) { /* if were still here then return the last curve */
    return cmd->object;
  }

  return NULL;
}

bool modifiers_usesArmature(Object *ob, bArmature *arm)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

  for (; md; md = md->next) {
    if (md->type == eModifierType_Armature) {
      ArmatureModifierData *amd = (ArmatureModifierData *)md;
      if (amd->object && amd->object->data == arm) {
        return true;
      }
    }
  }

  return false;
}

bool modifier_isCorrectableDeformed(ModifierData *md)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
  return mti->deformMatricesEM != NULL;
}

bool modifiers_isCorrectableDeformed(struct Scene *scene, Object *ob)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
  int required_mode = eModifierMode_Realtime;

  if (ob->mode == OB_MODE_EDIT) {
    required_mode |= eModifierMode_Editmode;
  }
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
    if (modifier_isPreview(md)) {
      return true;
    }
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

  if (ob->type != OB_MESH) {
    return;
  }

  for (md = ob->modifiers.first; md; md = md->next) {
    if (md->type == eModifierType_Multires) {
      MultiresModifierData *mmd = (MultiresModifierData *)md;

      multiresModifier_set_levels_from_disps(mmd, ob);
    }
  }
}

/* where should this go?, it doesn't fit well anywhere :S - campbell */

/* elubie: changed this to default to the same dir as the render output
 * to prevent saving to C:\ on Windows */

/* campbell: logic behind this...
 *
 * - if the ID is from a library, return library path
 * - else if the file has been saved return the blend file path.
 * - else if the file isn't saved and the ID isn't from a library, return the temp dir.
 */
const char *modifier_path_relbase(Main *bmain, Object *ob)
{
  if (G.relbase_valid || ID_IS_LINKED(ob)) {
    return ID_BLEND_PATH(bmain, &ob->id);
  }
  else {
    /* last resort, better then using "" which resolves to the current
     * working directory */
    return BKE_tempdir_session();
  }
}

const char *modifier_path_relbase_from_global(Object *ob)
{
  if (G.relbase_valid || ID_IS_LINKED(ob)) {
    return ID_BLEND_PATH_FROM_GLOBAL(&ob->id);
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
  BLI_join_dirfile(path, path_maxlen, G.relbase_valid ? "//" : BKE_tempdir_session(), name);
}

/* wrapper around ModifierTypeInfo.applyModifier that ensures valid normals */

struct Mesh *modwrap_applyModifier(ModifierData *md,
                                   const ModifierEvalContext *ctx,
                                   struct Mesh *me)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
  BLI_assert(CustomData_has_layer(&me->pdata, CD_NORMAL) == false);

  if (mti->dependsOnNormals && mti->dependsOnNormals(md)) {
    BKE_mesh_calc_normals(me);
  }
  return mti->applyModifier(md, ctx, me);
}

void modwrap_deformVerts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *me,
                         float (*vertexCos)[3],
                         int numVerts)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
  BLI_assert(!me || CustomData_has_layer(&me->pdata, CD_NORMAL) == false);

  if (me && mti->dependsOnNormals && mti->dependsOnNormals(md)) {
    BKE_mesh_calc_normals(me);
  }
  mti->deformVerts(md, ctx, me, vertexCos, numVerts);
}

void modwrap_deformVertsEM(ModifierData *md,
                           const ModifierEvalContext *ctx,
                           struct BMEditMesh *em,
                           Mesh *me,
                           float (*vertexCos)[3],
                           int numVerts)
{
  const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
  BLI_assert(!me || CustomData_has_layer(&me->pdata, CD_NORMAL) == false);

  if (me && mti->dependsOnNormals && mti->dependsOnNormals(md)) {
    BKE_mesh_calc_normals(me);
  }
  mti->deformVertsEM(md, ctx, em, me, vertexCos, numVerts);
}

/* end modifier callback wrappers */

/**
 * Get evaluated mesh for other evaluated object, which is used as an operand for the modifier,
 * e.g. second operand for boolean modifier.
 * Note that modifiers in stack always get fully evaluated COW ID pointers,
 * never original ones. Makes things simpler.
 *
 * \param get_cage_mesh: Return evaluated mesh with only deforming modifiers applied
 * (i.e. mesh topology remains the same as original one, a.k.a. 'cage' mesh).
 */
Mesh *BKE_modifier_get_evaluated_mesh_from_evaluated_object(Object *ob_eval,
                                                            const bool get_cage_mesh)
{
  Mesh *me = NULL;

  if ((ob_eval->type == OB_MESH) && (ob_eval->mode & OB_MODE_EDIT)) {
    /* In EditMode, evaluated mesh is stored in BMEditMesh, not the object... */
    BMEditMesh *em = BKE_editmesh_from_object(ob_eval);
    /* 'em' might not exist yet in some cases, just after loading a .blend file, see T57878. */
    if (em != NULL) {
      me = (get_cage_mesh && em->mesh_eval_cage != NULL) ? em->mesh_eval_cage :
                                                           em->mesh_eval_final;
    }
  }
  if (me == NULL) {
    me = (get_cage_mesh && ob_eval->runtime.mesh_deform_eval != NULL) ?
             ob_eval->runtime.mesh_deform_eval :
             ob_eval->runtime.mesh_eval;
  }

  return me;
}

ModifierData *modifier_get_original(ModifierData *md)
{
  if (md->orig_modifier_data == NULL) {
    return md;
  }
  return md->orig_modifier_data;
}

struct ModifierData *modifier_get_evaluated(Depsgraph *depsgraph, Object *object, ModifierData *md)
{
  Object *object_eval = DEG_get_evaluated_object(depsgraph, object);
  if (object_eval == object) {
    return md;
  }
  return modifiers_findByName(object_eval, md->name);
}
