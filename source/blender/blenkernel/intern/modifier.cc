/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * Modifier stack implementation.
 * BKE_modifier.h contains the function prototypes for this file.
 */

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_cloth_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_fluid_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_session_uuid.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_DerivedMesh.h"
#include "BKE_appdir.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.hh"
#include "BKE_effect.h"
#include "BKE_fluid.h"
#include "BKE_global.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_idtype.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_pointcache.h"
#include "BKE_screen.h"

/* may move these, only for BKE_modifier_path_relbase */
#include "BKE_main.h"
/* end */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.hh"

#include "BLO_read_write.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.modifier"};
static ModifierTypeInfo *modifier_types[NUM_MODIFIER_TYPES] = {nullptr};
static VirtualModifierData virtualModifierCommonData;

void BKE_modifier_init()
{
  ModifierData *md;

  /* Initialize modifier types */
  modifier_type_init(modifier_types); /* MOD_utils.c */

  /* Initialize global common storage used for virtual modifier list. */
  md = BKE_modifier_new(eModifierType_Armature);
  virtualModifierCommonData.amd = *((ArmatureModifierData *)md);
  BKE_modifier_free(md);

  md = BKE_modifier_new(eModifierType_Curve);
  virtualModifierCommonData.cmd = *((CurveModifierData *)md);
  BKE_modifier_free(md);

  md = BKE_modifier_new(eModifierType_Lattice);
  virtualModifierCommonData.lmd = *((LatticeModifierData *)md);
  BKE_modifier_free(md);

  md = BKE_modifier_new(eModifierType_ShapeKey);
  virtualModifierCommonData.smd = *((ShapeKeyModifierData *)md);
  BKE_modifier_free(md);

  virtualModifierCommonData.amd.modifier.mode |= eModifierMode_Virtual;
  virtualModifierCommonData.cmd.modifier.mode |= eModifierMode_Virtual;
  virtualModifierCommonData.lmd.modifier.mode |= eModifierMode_Virtual;
  virtualModifierCommonData.smd.modifier.mode |= eModifierMode_Virtual;
}

const ModifierTypeInfo *BKE_modifier_get_info(ModifierType type)
{
  /* type unsigned, no need to check < 0 */
  if (type < NUM_MODIFIER_TYPES && modifier_types[type] && modifier_types[type]->name[0] != '\0') {
    return modifier_types[type];
  }

  return nullptr;
}

void BKE_modifier_type_panel_id(ModifierType type, char *r_idname)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(type);
  BLI_string_join(r_idname, sizeof(PanelType::idname), MODIFIER_TYPE_PANEL_PREFIX, mti->name);
}

void BKE_modifier_panel_expand(ModifierData *md)
{
  md->ui_expand_flag |= UI_PANEL_DATA_EXPAND_ROOT;
}

/***/

static ModifierData *modifier_allocate_and_init(ModifierType type)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(type);
  ModifierData *md = static_cast<ModifierData *>(MEM_callocN(mti->structSize, mti->structName));

  /* NOTE: this name must be made unique later. */
  STRNCPY_UTF8(md->name, DATA_(mti->name));

  md->type = type;
  md->mode = eModifierMode_Realtime | eModifierMode_Render;
  md->flag = eModifierFlag_OverrideLibrary_Local;
  /* Only open the main panel at the beginning, not the sub-panels. */
  md->ui_expand_flag = UI_PANEL_DATA_EXPAND_ROOT;

  if (mti->flags & eModifierTypeFlag_EnableInEditmode) {
    md->mode |= eModifierMode_Editmode;
  }

  if (mti->initData) {
    mti->initData(md);
  }

  return md;
}

ModifierData *BKE_modifier_new(int type)
{
  ModifierData *md = modifier_allocate_and_init(ModifierType(type));

  BKE_modifier_session_uuid_generate(md);

  return md;
}

static void modifier_free_data_id_us_cb(void * /*userData*/,
                                        Object * /*ob*/,
                                        ID **idpoin,
                                        int cb_flag)
{
  ID *id = *idpoin;
  if (id != nullptr && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_min(id);
  }
}

void BKE_modifier_free_ex(ModifierData *md, const int flag)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (mti->foreachIDLink) {
      mti->foreachIDLink(md, nullptr, modifier_free_data_id_us_cb, nullptr);
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

void BKE_modifier_free(ModifierData *md)
{
  BKE_modifier_free_ex(md, 0);
}

void BKE_modifier_remove_from_list(Object *ob, ModifierData *md)
{
  BLI_assert(BLI_findindex(&ob->modifiers, md) != -1);

  if (md->flag & eModifierFlag_Active) {
    /* Prefer the previous modifier but use the next if this modifier is the first in the list. */
    if (md->next != nullptr) {
      BKE_object_modifier_set_active(ob, md->next);
    }
    else if (md->prev != nullptr) {
      BKE_object_modifier_set_active(ob, md->prev);
    }
  }

  BLI_remlink(&ob->modifiers, md);
}

void BKE_modifier_session_uuid_generate(ModifierData *md)
{
  md->session_uuid = BLI_session_uuid_generate();
}

bool BKE_modifier_unique_name(ListBase *modifiers, ModifierData *md)
{
  if (modifiers && md) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

    return BLI_uniquename(
        modifiers, md, DATA_(mti->name), '.', offsetof(ModifierData, name), sizeof(md->name));
  }
  return false;
}

bool BKE_modifier_depends_ontime(Scene *scene, ModifierData *md)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

  return mti->dependsOnTime && mti->dependsOnTime(scene, md);
}

bool BKE_modifier_supports_mapping(ModifierData *md)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

  return (mti->type == eModifierTypeType_OnlyDeform ||
          (mti->flags & eModifierTypeFlag_SupportsMapping));
}

bool BKE_modifier_is_preview(ModifierData *md)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

  /* Constructive modifiers are highly likely to also modify data like vgroups or vertex-colors! */
  if (!((mti->flags & eModifierTypeFlag_UsesPreview) ||
        (mti->type == eModifierTypeType_Constructive)))
  {
    return false;
  }

  if (md->mode & eModifierMode_Realtime) {
    return true;
  }

  return false;
}

ModifierData *BKE_modifiers_findby_type(const Object *ob, ModifierType type)
{
  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->type == type) {
      return md;
    }
  }
  return nullptr;
}

ModifierData *BKE_modifiers_findby_name(const Object *ob, const char *name)
{
  return static_cast<ModifierData *>(
      BLI_findstring(&(ob->modifiers), name, offsetof(ModifierData, name)));
}

ModifierData *BKE_modifiers_findby_session_uuid(const Object *ob, const SessionUUID *session_uuid)
{
  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (BLI_session_uuid_is_equal(&md->session_uuid, session_uuid)) {
      return md;
    }
  }
  return nullptr;
}

void BKE_modifiers_clear_errors(Object *ob)
{
  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->error) {
      MEM_freeN(md->error);
      md->error = nullptr;
    }
  }
}

void BKE_modifiers_foreach_ID_link(Object *ob, IDWalkFunc walk, void *userData)
{
  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

    if (mti->foreachIDLink) {
      mti->foreachIDLink(md, ob, walk, userData);
    }
  }
}

void BKE_modifiers_foreach_tex_link(Object *ob, TexWalkFunc walk, void *userData)
{
  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

    if (mti->foreachTexLink) {
      mti->foreachTexLink(md, ob, walk, userData);
    }
  }
}

ModifierData *BKE_modifier_copy_ex(const ModifierData *md, int flag)
{
  ModifierData *md_dst = modifier_allocate_and_init(ModifierType(md->type));

  STRNCPY(md_dst->name, md->name);
  BKE_modifier_copydata_ex(md, md_dst, flag);

  return md_dst;
}

void BKE_modifier_copydata_generic(const ModifierData *md_src,
                                   ModifierData *md_dst,
                                   const int /*flag*/)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md_src->type));

  /* `md_dst` may have already be fully initialized with some extra allocated data,
   * we need to free it now to avoid a memory leak. */
  if (mti->freeData) {
    mti->freeData(md_dst);
  }

  const size_t data_size = sizeof(ModifierData);
  const char *md_src_data = ((const char *)md_src) + data_size;
  char *md_dst_data = ((char *)md_dst) + data_size;
  BLI_assert(data_size <= size_t(mti->structSize));
  memcpy(md_dst_data, md_src_data, size_t(mti->structSize) - data_size);

  /* Runtime fields are never to be preserved. */
  md_dst->runtime = nullptr;
}

static void modifier_copy_data_id_us_cb(void * /*userData*/,
                                        Object * /*ob*/,
                                        ID **idpoin,
                                        int cb_flag)
{
  ID *id = *idpoin;
  if (id != nullptr && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_plus(id);
  }
}

void BKE_modifier_copydata_ex(const ModifierData *md, ModifierData *target, const int flag)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

  target->mode = md->mode;
  target->flag = md->flag;
  target->ui_expand_flag = md->ui_expand_flag;

  if (mti->copyData) {
    mti->copyData(md, target, flag);
  }

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (mti->foreachIDLink) {
      mti->foreachIDLink(target, nullptr, modifier_copy_data_id_us_cb, nullptr);
    }
  }

  if (flag & LIB_ID_CREATE_NO_MAIN) {
    /* Make sure UUID is the same between the source and the target.
     * This is needed in the cases when UUID is to be preserved and when there is no copyData
     * callback, or the copyData does not do full byte copy of the modifier data. */
    target->session_uuid = md->session_uuid;
  }
  else {
    /* In the case copyData made full byte copy force UUID to be re-generated. */
    BKE_modifier_session_uuid_generate(target);
  }
}

void BKE_modifier_copydata(const ModifierData *md, ModifierData *target)
{
  BKE_modifier_copydata_ex(md, target, 0);
}

bool BKE_modifier_supports_cage(Scene *scene, ModifierData *md)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

  return ((!mti->isDisabled || !mti->isDisabled(scene, md, false)) &&
          (mti->flags & eModifierTypeFlag_SupportsEditmode) && BKE_modifier_supports_mapping(md));
}

bool BKE_modifier_couldbe_cage(Scene *scene, ModifierData *md)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

  return ((md->mode & eModifierMode_Realtime) && (md->mode & eModifierMode_Editmode) &&
          (!mti->isDisabled || !mti->isDisabled(scene, md, false)) &&
          BKE_modifier_supports_mapping(md));
}

bool BKE_modifier_is_same_topology(ModifierData *md)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
  return ELEM(mti->type, eModifierTypeType_OnlyDeform, eModifierTypeType_NonGeometrical);
}

bool BKE_modifier_is_non_geometrical(ModifierData *md)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
  return (mti->type == eModifierTypeType_NonGeometrical);
}

void BKE_modifier_set_error(const Object *ob, ModifierData *md, const char *_format, ...)
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

#ifndef NDEBUG
  if ((md->mode & eModifierMode_Virtual) == 0) {
    /* Ensure correct object is passed in. */
    BLI_assert(BKE_modifier_get_original(ob, md) != nullptr);
  }
#endif

  CLOG_ERROR(&LOG, "Object: \"%s\", Modifier: \"%s\", %s", ob->id.name + 2, md->name, md->error);
}

void BKE_modifier_set_warning(const Object *ob, ModifierData *md, const char *_format, ...)
{
  char buffer[512];
  va_list ap;
  const char *format = TIP_(_format);

  va_start(ap, _format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);
  buffer[sizeof(buffer) - 1] = '\0';

  /* Store the warning in the same field as the error.
   * It is not expected to have both error and warning and having a single place to store the
   * message simplifies interface code. */

  if (md->error) {
    MEM_freeN(md->error);
  }

  md->error = BLI_strdup(buffer);

#ifndef NDEBUG
  if ((md->mode & eModifierMode_Virtual) == 0) {
    /* Ensure correct object is passed in. */
    BLI_assert(BKE_modifier_get_original(ob, md) != nullptr);
  }
#endif

  UNUSED_VARS_NDEBUG(ob);
}

int BKE_modifiers_get_cage_index(const Scene *scene,
                                 Object *ob,
                                 int *r_lastPossibleCageIndex,
                                 bool is_virtual)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = (is_virtual) ?
                         BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData) :
                         static_cast<ModifierData *>(ob->modifiers.first);

  if (r_lastPossibleCageIndex) {
    /* ensure the value is initialized */
    *r_lastPossibleCageIndex = -1;
  }

  /* Find the last modifier acting on the cage. */
  int cageIndex = -1;
  for (int i = 0; md; i++, md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
    bool supports_mapping;

    if (mti->isDisabled && mti->isDisabled(scene, md, false)) {
      continue;
    }
    if (!(mti->flags & eModifierTypeFlag_SupportsEditmode)) {
      continue;
    }
    if (md->mode & eModifierMode_DisableTemporary) {
      continue;
    }

    supports_mapping = BKE_modifier_supports_mapping(md);
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

bool BKE_modifiers_is_softbody_enabled(Object *ob)
{
  ModifierData *md = BKE_modifiers_findby_type(ob, eModifierType_Softbody);

  return (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render));
}

bool BKE_modifiers_is_cloth_enabled(Object *ob)
{
  ModifierData *md = BKE_modifiers_findby_type(ob, eModifierType_Cloth);

  return (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render));
}

bool BKE_modifiers_is_modifier_enabled(Object *ob, int modifierType)
{
  ModifierData *md = BKE_modifiers_findby_type(ob, ModifierType(modifierType));

  return (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render));
}

bool BKE_modifiers_is_particle_enabled(Object *ob)
{
  ModifierData *md = BKE_modifiers_findby_type(ob, eModifierType_ParticleSystem);

  return (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render));
}

bool BKE_modifier_is_enabled(const Scene *scene, ModifierData *md, int required_mode)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

  if ((md->mode & required_mode) != required_mode) {
    return false;
  }
  if (scene != nullptr && mti->isDisabled &&
      mti->isDisabled(scene, md, required_mode == eModifierMode_Render))
  {
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

bool BKE_modifier_is_nonlocal_in_liboverride(const Object *ob, const ModifierData *md)
{
  return (ID_IS_OVERRIDE_LIBRARY(ob) &&
          (md == nullptr || (md->flag & eModifierFlag_OverrideLibrary_Local) == 0));
}

CDMaskLink *BKE_modifier_calc_data_masks(const Scene *scene,
                                         ModifierData *md,
                                         CustomData_MeshMasks *final_datamask,
                                         int required_mode,
                                         ModifierData *previewmd,
                                         const CustomData_MeshMasks *previewmask)
{
  CDMaskLink *dataMasks = nullptr;
  CDMaskLink *curr, *prev;
  bool have_deform_modifier = false;

  /* build a list of modifier data requirements in reverse order */
  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

    curr = MEM_cnew<CDMaskLink>(__func__);

    if (BKE_modifier_is_enabled(scene, md, required_mode)) {
      if (mti->type == eModifierTypeType_OnlyDeform) {
        have_deform_modifier = true;
      }

      if (mti->requiredDataMask) {
        mti->requiredDataMask(md, &curr->mask);
      }

      if (previewmd == md && previewmask != nullptr) {
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
  for (curr = dataMasks, prev = nullptr; curr; prev = curr, curr = curr->next) {
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

ModifierData *BKE_modifier_get_last_preview(const Scene *scene,
                                            ModifierData *md,
                                            int required_mode)
{
  ModifierData *tmp_md = nullptr;

  if ((required_mode & ~eModifierMode_Editmode) != eModifierMode_Realtime) {
    return tmp_md;
  }

  /* Find the latest modifier in stack generating preview. */
  for (; md; md = md->next) {
    if (BKE_modifier_is_enabled(scene, md, required_mode) && BKE_modifier_is_preview(md)) {
      tmp_md = md;
    }
  }
  return tmp_md;
}

ModifierData *BKE_modifiers_get_virtual_modifierlist(const Object *ob,
                                                     VirtualModifierData *virtualModifierData)
{
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.first);

  *virtualModifierData = virtualModifierCommonData;

  if (ob->parent) {
    if (ob->parent->type == OB_ARMATURE && ob->partype == PARSKEL) {
      virtualModifierData->amd.object = ob->parent;
      virtualModifierData->amd.modifier.next = md;
      virtualModifierData->amd.deformflag = ((bArmature *)(ob->parent->data))->deformflag;
      md = &virtualModifierData->amd.modifier;
    }
    else if (ob->parent->type == OB_CURVES_LEGACY && ob->partype == PARSKEL) {
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
  if (ELEM(ob->type, OB_MESH, OB_LATTICE) && BKE_key_from_object((Object *)ob)) {
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

Object *BKE_modifiers_is_deformed_by_armature(Object *ob)
{
  if (ob->type == OB_GPENCIL_LEGACY) {
    GpencilVirtualModifierData gpencilvirtualModifierData;
    ArmatureGpencilModifierData *agmd = nullptr;
    GpencilModifierData *gmd = BKE_gpencil_modifiers_get_virtual_modifierlist(
        ob, &gpencilvirtualModifierData);

    /* return the first selected armature, this lets us use multiple armatures */
    for (; gmd; gmd = gmd->next) {
      if (gmd->type == eGpencilModifierType_Armature) {
        agmd = (ArmatureGpencilModifierData *)gmd;
        if (agmd->object && (agmd->object->base_flag & BASE_SELECTED)) {
          return agmd->object;
        }
      }
    }
    /* If we're still here then return the last armature. */
    if (agmd) {
      return agmd->object;
    }
  }
  else {
    VirtualModifierData virtualModifierData;
    ArmatureModifierData *amd = nullptr;
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);

    /* return the first selected armature, this lets us use multiple armatures */
    for (; md; md = md->next) {
      if (md->type == eModifierType_Armature) {
        amd = (ArmatureModifierData *)md;
        if (amd->object && (amd->object->base_flag & BASE_SELECTED)) {
          return amd->object;
        }
      }
    }
    /* If we're still here then return the last armature. */
    if (amd) {
      return amd->object;
    }
  }

  return nullptr;
}

Object *BKE_modifiers_is_deformed_by_meshdeform(Object *ob)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
  MeshDeformModifierData *mdmd = nullptr;

  /* return the first selected armature, this lets us use multiple armatures */
  for (; md; md = md->next) {
    if (md->type == eModifierType_MeshDeform) {
      mdmd = (MeshDeformModifierData *)md;
      if (mdmd->object && (mdmd->object->base_flag & BASE_SELECTED)) {
        return mdmd->object;
      }
    }
  }

  if (mdmd) { /* if we're still here then return the last armature */
    return mdmd->object;
  }

  return nullptr;
}

Object *BKE_modifiers_is_deformed_by_lattice(Object *ob)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
  LatticeModifierData *lmd = nullptr;

  /* return the first selected lattice, this lets us use multiple lattices */
  for (; md; md = md->next) {
    if (md->type == eModifierType_Lattice) {
      lmd = (LatticeModifierData *)md;
      if (lmd->object && (lmd->object->base_flag & BASE_SELECTED)) {
        return lmd->object;
      }
    }
  }

  if (lmd) { /* if we're still here then return the last lattice */
    return lmd->object;
  }

  return nullptr;
}

Object *BKE_modifiers_is_deformed_by_curve(Object *ob)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
  CurveModifierData *cmd = nullptr;

  /* return the first selected curve, this lets us use multiple curves */
  for (; md; md = md->next) {
    if (md->type == eModifierType_Curve) {
      cmd = (CurveModifierData *)md;
      if (cmd->object && (cmd->object->base_flag & BASE_SELECTED)) {
        return cmd->object;
      }
    }
  }

  if (cmd) { /* if we're still here then return the last curve */
    return cmd->object;
  }

  return nullptr;
}

bool BKE_modifiers_uses_multires(Object *ob)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
  MultiresModifierData *mmd = nullptr;

  for (; md; md = md->next) {
    if (md->type == eModifierType_Multires) {
      mmd = (MultiresModifierData *)md;
      if (mmd->totlvl != 0) {
        return true;
      }
    }
  }
  return false;
}

bool BKE_modifiers_uses_armature(Object *ob, bArmature *arm)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);

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

bool BKE_modifier_is_correctable_deformed(ModifierData *md)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
  return mti->deformMatricesEM != nullptr;
}

bool BKE_modifiers_is_correctable_deformed(const Scene *scene, Object *ob)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
  int required_mode = eModifierMode_Realtime;

  if (ob->mode == OB_MODE_EDIT) {
    required_mode |= eModifierMode_Editmode;
  }
  for (; md; md = md->next) {
    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      /* pass */
    }
    else if (BKE_modifier_is_correctable_deformed(md)) {
      return true;
    }
  }
  return false;
}

void BKE_modifier_free_temporary_data(ModifierData *md)
{
  if (md->type == eModifierType_Armature) {
    ArmatureModifierData *amd = (ArmatureModifierData *)md;

    MEM_SAFE_FREE(amd->vert_coords_prev);
  }
}

void BKE_modifiers_test_object(Object *ob)
{
  /* just multires checked for now, since only multires
   * modifies mesh data */

  if (ob->type != OB_MESH) {
    return;
  }

  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->type == eModifierType_Multires) {
      MultiresModifierData *mmd = (MultiresModifierData *)md;

      multiresModifier_set_levels_from_disps(mmd, ob);
    }
  }
}

const char *BKE_modifier_path_relbase(Main *bmain, Object *ob)
{
  /* - If the ID is from a library, return library path.
   * - Else if the file has been saved return the blend file path.
   * - Else if the file isn't saved and the ID isn't from a library, return the temp dir.
   */
  if ((bmain->filepath[0] != '\0') || ID_IS_LINKED(ob)) {
    return ID_BLEND_PATH(bmain, &ob->id);
  }

  /* Last resort, better than using "" which resolves to the current working directory. */
  return BKE_tempdir_session();
}

const char *BKE_modifier_path_relbase_from_global(Object *ob)
{
  return BKE_modifier_path_relbase(G_MAIN, ob);
}

void BKE_modifier_path_init(char *path, int path_maxncpy, const char *name)
{
  const char *blendfile_path = BKE_main_blendfile_path_from_global();
  BLI_path_join(path, path_maxncpy, blendfile_path[0] ? "//" : BKE_tempdir_session(), name);
}

/**
 * Call when #ModifierTypeInfo.dependsOnNormals callback requests normals.
 */
static void modwrap_dependsOnNormals(Mesh *me)
{
  switch (me->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH: {
      blender::bke::EditMeshData *edit_data = me->runtime->edit_data;
      if (!edit_data->vertexCos.is_empty()) {
        /* Note that 'ensure' is acceptable here since these values aren't modified in-place.
         * If that changes we'll need to recalculate. */
        BKE_editmesh_cache_ensure_vert_normals(me->edit_mesh, edit_data);
      }
      else {
        BM_mesh_normals_update(me->edit_mesh->bm);
      }
      break;
    }
    case ME_WRAPPER_TYPE_SUBD:
      /* Not an expected case. */
      break;
    case ME_WRAPPER_TYPE_MDATA:
      /* Normals are calculated lazily. */
      break;
  }
}

/* wrapper around ModifierTypeInfo.modifyMesh that ensures valid normals */

Mesh *BKE_modifier_modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *me)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

  if (me->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH) {
    if ((mti->flags & eModifierTypeFlag_AcceptsBMesh) == 0) {
      BKE_mesh_wrapper_ensure_mdata(me);
    }
  }

  if (mti->dependsOnNormals && mti->dependsOnNormals(md)) {
    modwrap_dependsOnNormals(me);
  }
  return mti->modifyMesh(md, ctx, me);
}

void BKE_modifier_deform_verts(ModifierData *md,
                               const ModifierEvalContext *ctx,
                               Mesh *me,
                               float (*vertexCos)[3],
                               int numVerts)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
  if (me && mti->dependsOnNormals && mti->dependsOnNormals(md)) {
    modwrap_dependsOnNormals(me);
  }
  mti->deformVerts(md, ctx, me, vertexCos, numVerts);
  if (me) {
    BKE_mesh_tag_positions_changed(me);
  }
}

void BKE_modifier_deform_vertsEM(ModifierData *md,
                                 const ModifierEvalContext *ctx,
                                 BMEditMesh *em,
                                 Mesh *me,
                                 float (*vertexCos)[3],
                                 int numVerts)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
  if (me && mti->dependsOnNormals && mti->dependsOnNormals(md)) {
    modwrap_dependsOnNormals(me);
  }
  mti->deformVertsEM(md, ctx, em, me, vertexCos, numVerts);
}

/* end modifier callback wrappers */

Mesh *BKE_modifier_get_evaluated_mesh_from_evaluated_object(Object *ob_eval)
{
  Mesh *me = nullptr;

  if ((ob_eval->type == OB_MESH) && (ob_eval->mode & OB_MODE_EDIT)) {
    /* In EditMode, evaluated mesh is stored in BMEditMesh, not the object... */
    BMEditMesh *em = BKE_editmesh_from_object(ob_eval);
    /* 'em' might not exist yet in some cases, just after loading a .blend file, see #57878. */
    if (em != nullptr) {
      me = BKE_object_get_editmesh_eval_final(ob_eval);
    }
  }
  if (me == nullptr) {
    me = BKE_object_get_evaluated_mesh(ob_eval);
  }

  return me;
}

ModifierData *BKE_modifier_get_original(const Object *object, ModifierData *md)
{
  const Object *object_orig = DEG_get_original_object((Object *)object);
  return BKE_modifiers_findby_session_uuid(object_orig, &md->session_uuid);
}

ModifierData *BKE_modifier_get_evaluated(Depsgraph *depsgraph, Object *object, ModifierData *md)
{
  Object *object_eval = DEG_get_evaluated_object(depsgraph, object);
  if (object_eval == object) {
    return md;
  }
  return BKE_modifiers_findby_session_uuid(object_eval, &md->session_uuid);
}

void BKE_modifier_check_uuids_unique_and_report(const Object *object)
{
  GSet *used_uuids = BLI_gset_new(
      BLI_session_uuid_ghash_hash, BLI_session_uuid_ghash_compare, "modifier used uuids");

  LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
    const SessionUUID *session_uuid = &md->session_uuid;
    if (!BLI_session_uuid_is_generated(session_uuid)) {
      printf("Modifier %s -> %s does not have UUID generated.\n", object->id.name + 2, md->name);
      continue;
    }

    if (BLI_gset_lookup(used_uuids, session_uuid) != nullptr) {
      printf("Modifier %s -> %s has duplicate UUID generated.\n", object->id.name + 2, md->name);
      continue;
    }

    BLI_gset_insert(used_uuids, (void *)session_uuid);
  }

  BLI_gset_free(used_uuids, nullptr);
}

void BKE_modifier_blend_write(BlendWriter *writer, const ID *id_owner, ListBase *modbase)
{
  if (modbase == nullptr) {
    return;
  }

  LISTBASE_FOREACH (ModifierData *, md, modbase) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
    if (mti == nullptr) {
      continue;
    }

    /* If the blendWrite callback is defined, it should handle the whole writing process. */
    if (mti->blendWrite != nullptr) {
      mti->blendWrite(writer, id_owner, md);
      continue;
    }

    BLO_write_struct_by_name(writer, mti->structName, md);

    if (md->type == eModifierType_Cloth) {
      ClothModifierData *clmd = (ClothModifierData *)md;

      BLO_write_struct(writer, ClothSimSettings, clmd->sim_parms);
      BLO_write_struct(writer, ClothCollSettings, clmd->coll_parms);
      BLO_write_struct(writer, EffectorWeights, clmd->sim_parms->effector_weights);
      BKE_ptcache_blend_write(writer, &clmd->ptcaches);
    }
    else if (md->type == eModifierType_Fluid) {
      FluidModifierData *fmd = (FluidModifierData *)md;

      if (fmd->type & MOD_FLUID_TYPE_DOMAIN) {
        BLO_write_struct(writer, FluidDomainSettings, fmd->domain);

        if (fmd->domain) {
          BKE_ptcache_blend_write(writer, &(fmd->domain->ptcaches[0]));

          /* create fake pointcache so that old blender versions can read it */
          fmd->domain->point_cache[1] = BKE_ptcache_add(&fmd->domain->ptcaches[1]);
          fmd->domain->point_cache[1]->flag |= PTCACHE_DISK_CACHE | PTCACHE_FAKE_SMOKE;
          fmd->domain->point_cache[1]->step = 1;

          BKE_ptcache_blend_write(writer, &(fmd->domain->ptcaches[1]));

          if (fmd->domain->coba) {
            BLO_write_struct(writer, ColorBand, fmd->domain->coba);
          }

          /* cleanup the fake pointcache */
          BKE_ptcache_free_list(&fmd->domain->ptcaches[1]);
          fmd->domain->point_cache[1] = nullptr;

          BLO_write_struct(writer, EffectorWeights, fmd->domain->effector_weights);
        }
      }
      else if (fmd->type & MOD_FLUID_TYPE_FLOW) {
        BLO_write_struct(writer, FluidFlowSettings, fmd->flow);
      }
      else if (fmd->type & MOD_FLUID_TYPE_EFFEC) {
        BLO_write_struct(writer, FluidEffectorSettings, fmd->effector);
      }
    }
    else if (md->type == eModifierType_Fluidsim) {
      FluidsimModifierData *fluidmd = (FluidsimModifierData *)md;

      BLO_write_struct(writer, FluidsimSettings, fluidmd->fss);
    }
    else if (md->type == eModifierType_DynamicPaint) {
      DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

      if (pmd->canvas) {
        BLO_write_struct(writer, DynamicPaintCanvasSettings, pmd->canvas);

        /* write surfaces */
        LISTBASE_FOREACH (DynamicPaintSurface *, surface, &pmd->canvas->surfaces) {
          BLO_write_struct(writer, DynamicPaintSurface, surface);
        }
        /* write caches and effector weights */
        LISTBASE_FOREACH (DynamicPaintSurface *, surface, &pmd->canvas->surfaces) {
          BKE_ptcache_blend_write(writer, &(surface->ptcaches));

          BLO_write_struct(writer, EffectorWeights, surface->effector_weights);
        }
      }
      if (pmd->brush) {
        BLO_write_struct(writer, DynamicPaintBrushSettings, pmd->brush);
        BLO_write_struct(writer, ColorBand, pmd->brush->paint_ramp);
        BLO_write_struct(writer, ColorBand, pmd->brush->vel_ramp);
      }
    }
    else if (md->type == eModifierType_Collision) {

#if 0
      CollisionModifierData *collmd = (CollisionModifierData *)md;
      /* TODO: CollisionModifier should use pointcache
       * + have proper reset events before enabling this. */
      writestruct(wd, DATA, float[3], collmd->numverts, collmd->x);
      writestruct(wd, DATA, float[3], collmd->numverts, collmd->xnew);
      writestruct(wd, DATA, MFace, collmd->numfaces, collmd->mfaces);
#endif
    }
  }
}

/* TODO(sergey): Find a better place for this.
 *
 * Unfortunately, this can not be done as a regular do_versions() since the modifier type is
 * set to NONE, so the do_versions code wouldn't know where the modifier came from.
 *
 * The best approach seems to have the functionality in versioning_280.c but still call the
 * function from #BKE_modifier_blend_read_data().
 */

/* Domain, inflow, ... */
static void modifier_ensure_type(FluidModifierData *fluid_modifier_data, int type)
{
  fluid_modifier_data->type = type;
  BKE_fluid_modifier_free(fluid_modifier_data);
  BKE_fluid_modifier_create_type_data(fluid_modifier_data);
}

/**
 * \note The old_modifier_data is NOT linked.
 * This means that in order to access sub-data pointers #BLO_read_get_new_data_address is to be
 * used.
 */
static ModifierData *modifier_replace_with_fluid(BlendDataReader *reader,
                                                 Object *object,
                                                 ListBase *modifiers,
                                                 ModifierData *old_modifier_data)
{
  ModifierData *new_modifier_data = BKE_modifier_new(eModifierType_Fluid);
  FluidModifierData *fluid_modifier_data = (FluidModifierData *)new_modifier_data;

  if (old_modifier_data->type == eModifierType_Fluidsim) {
    FluidsimModifierData *old_fluidsim_modifier_data = (FluidsimModifierData *)old_modifier_data;
    FluidsimSettings *old_fluidsim_settings = static_cast<FluidsimSettings *>(
        BLO_read_get_new_data_address(reader, old_fluidsim_modifier_data->fss));
    switch (old_fluidsim_settings->type) {
      case OB_FLUIDSIM_ENABLE:
        modifier_ensure_type(fluid_modifier_data, 0);
        break;
      case OB_FLUIDSIM_DOMAIN:
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_DOMAIN);
        BKE_fluid_domain_type_set(object, fluid_modifier_data->domain, FLUID_DOMAIN_TYPE_LIQUID);
        break;
      case OB_FLUIDSIM_FLUID:
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_FLOW);
        BKE_fluid_flow_type_set(object, fluid_modifier_data->flow, FLUID_FLOW_TYPE_LIQUID);
        /* No need to emit liquid far away from surface. */
        fluid_modifier_data->flow->surface_distance = 0.0f;
        break;
      case OB_FLUIDSIM_OBSTACLE:
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_EFFEC);
        BKE_fluid_effector_type_set(
            object, fluid_modifier_data->effector, FLUID_EFFECTOR_TYPE_COLLISION);
        break;
      case OB_FLUIDSIM_INFLOW:
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_FLOW);
        BKE_fluid_flow_type_set(object, fluid_modifier_data->flow, FLUID_FLOW_TYPE_LIQUID);
        BKE_fluid_flow_behavior_set(object, fluid_modifier_data->flow, FLUID_FLOW_BEHAVIOR_INFLOW);
        /* No need to emit liquid far away from surface. */
        fluid_modifier_data->flow->surface_distance = 0.0f;
        break;
      case OB_FLUIDSIM_OUTFLOW:
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_FLOW);
        BKE_fluid_flow_type_set(object, fluid_modifier_data->flow, FLUID_FLOW_TYPE_LIQUID);
        BKE_fluid_flow_behavior_set(
            object, fluid_modifier_data->flow, FLUID_FLOW_BEHAVIOR_OUTFLOW);
        break;
      case OB_FLUIDSIM_PARTICLE:
        /* "Particle" type objects not being used by Mantaflow fluid simulations.
         * Skip this object, secondary particles can only be enabled through the domain object. */
        break;
      case OB_FLUIDSIM_CONTROL:
        /* "Control" type objects not being used by Mantaflow fluid simulations.
         * Use guiding type instead which is similar. */
        modifier_ensure_type(fluid_modifier_data, MOD_FLUID_TYPE_EFFEC);
        BKE_fluid_effector_type_set(
            object, fluid_modifier_data->effector, FLUID_EFFECTOR_TYPE_GUIDE);
        break;
    }
  }
  else if (old_modifier_data->type == eModifierType_Smoke) {
    SmokeModifierData *old_smoke_modifier_data = (SmokeModifierData *)old_modifier_data;
    modifier_ensure_type(fluid_modifier_data, old_smoke_modifier_data->type);
    if (fluid_modifier_data->type == MOD_FLUID_TYPE_DOMAIN) {
      BKE_fluid_domain_type_set(object, fluid_modifier_data->domain, FLUID_DOMAIN_TYPE_GAS);
    }
    else if (fluid_modifier_data->type == MOD_FLUID_TYPE_FLOW) {
      BKE_fluid_flow_type_set(object, fluid_modifier_data->flow, FLUID_FLOW_TYPE_SMOKE);
    }
    else if (fluid_modifier_data->type == MOD_FLUID_TYPE_EFFEC) {
      BKE_fluid_effector_type_set(
          object, fluid_modifier_data->effector, FLUID_EFFECTOR_TYPE_COLLISION);
    }
  }

  /* Replace modifier data in the stack. */
  new_modifier_data->next = old_modifier_data->next;
  new_modifier_data->prev = old_modifier_data->prev;
  if (new_modifier_data->prev != nullptr) {
    new_modifier_data->prev->next = new_modifier_data;
  }
  if (new_modifier_data->next != nullptr) {
    new_modifier_data->next->prev = new_modifier_data;
  }
  if (modifiers->first == old_modifier_data) {
    modifiers->first = new_modifier_data;
  }
  if (modifiers->last == old_modifier_data) {
    modifiers->last = new_modifier_data;
  }

  /* Free old modifier data. */
  MEM_freeN(old_modifier_data);

  return new_modifier_data;
}

void BKE_modifier_blend_read_data(BlendDataReader *reader, ListBase *lb, Object *ob)
{
  BLO_read_list(reader, lb);

  LISTBASE_FOREACH (ModifierData *, md, lb) {
    BKE_modifier_session_uuid_generate(md);

    md->error = nullptr;
    md->runtime = nullptr;

    /* Modifier data has been allocated as a part of data migration process and
     * no reading of nested fields from file is needed. */
    bool is_allocated = false;

    if (md->type == eModifierType_Fluidsim) {
      BLO_reportf_wrap(
          BLO_read_data_reports(reader),
          RPT_WARNING,
          TIP_("Possible data loss when saving this file! %s modifier is deprecated (Object: %s)"),
          md->name,
          ob->id.name + 2);
      md = modifier_replace_with_fluid(reader, ob, lb, md);
      is_allocated = true;
    }
    else if (md->type == eModifierType_Smoke) {
      BLO_reportf_wrap(
          BLO_read_data_reports(reader),
          RPT_WARNING,
          TIP_("Possible data loss when saving this file! %s modifier is deprecated (Object: %s)"),
          md->name,
          ob->id.name + 2);
      md = modifier_replace_with_fluid(reader, ob, lb, md);
      is_allocated = true;
    }

    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

    /* if modifiers disappear, or for upward compatibility */
    if (mti == nullptr) {
      md->type = eModifierType_None;
    }

    if (is_allocated) {
      /* All the fields has been properly allocated. */
    }
    else if (md->type == eModifierType_Cloth) {
      ClothModifierData *clmd = (ClothModifierData *)md;

      clmd->clothObject = nullptr;
      clmd->hairdata = nullptr;

      BLO_read_data_address(reader, &clmd->sim_parms);
      BLO_read_data_address(reader, &clmd->coll_parms);

      BKE_ptcache_blend_read_data(reader, &clmd->ptcaches, &clmd->point_cache, 0);

      if (clmd->sim_parms) {
        if (clmd->sim_parms->presets > 10) {
          clmd->sim_parms->presets = 0;
        }

        clmd->sim_parms->reset = 0;

        BLO_read_data_address(reader, &clmd->sim_parms->effector_weights);

        if (!clmd->sim_parms->effector_weights) {
          clmd->sim_parms->effector_weights = BKE_effector_add_weights(nullptr);
        }
      }

      clmd->solver_result = nullptr;
    }
    else if (md->type == eModifierType_Fluid) {

      FluidModifierData *fmd = (FluidModifierData *)md;

      if (fmd->type == MOD_FLUID_TYPE_DOMAIN) {
        fmd->flow = nullptr;
        fmd->effector = nullptr;
        BLO_read_data_address(reader, &fmd->domain);
        fmd->domain->fmd = fmd;

        fmd->domain->fluid = nullptr;
        fmd->domain->fluid_mutex = BLI_rw_mutex_alloc();
        fmd->domain->tex_density = nullptr;
        fmd->domain->tex_color = nullptr;
        fmd->domain->tex_shadow = nullptr;
        fmd->domain->tex_flame = nullptr;
        fmd->domain->tex_flame_coba = nullptr;
        fmd->domain->tex_coba = nullptr;
        fmd->domain->tex_field = nullptr;
        fmd->domain->tex_velocity_x = nullptr;
        fmd->domain->tex_velocity_y = nullptr;
        fmd->domain->tex_velocity_z = nullptr;
        fmd->domain->tex_wt = nullptr;
        BLO_read_data_address(reader, &fmd->domain->coba);

        BLO_read_data_address(reader, &fmd->domain->effector_weights);
        if (!fmd->domain->effector_weights) {
          fmd->domain->effector_weights = BKE_effector_add_weights(nullptr);
        }

        BKE_ptcache_blend_read_data(
            reader, &(fmd->domain->ptcaches[0]), &(fmd->domain->point_cache[0]), 1);

        /* Manta sim uses only one cache from now on, so store pointer convert */
        if (fmd->domain->ptcaches[1].first || fmd->domain->point_cache[1]) {
          if (fmd->domain->point_cache[1]) {
            PointCache *cache = static_cast<PointCache *>(
                BLO_read_get_new_data_address(reader, fmd->domain->point_cache[1]));
            if (cache->flag & PTCACHE_FAKE_SMOKE) {
              /* Manta-sim/smoke was already saved in "new format" and this cache is a fake one. */
            }
            else {
              printf(
                  "High resolution manta cache not available due to pointcache update. Please "
                  "reset the simulation.\n");
            }
            BKE_ptcache_free(cache);
          }
          BLI_listbase_clear(&fmd->domain->ptcaches[1]);
          fmd->domain->point_cache[1] = nullptr;
        }
      }
      else if (fmd->type == MOD_FLUID_TYPE_FLOW) {
        fmd->domain = nullptr;
        fmd->effector = nullptr;
        BLO_read_data_address(reader, &fmd->flow);
        fmd->flow->fmd = fmd;
        fmd->flow->mesh = nullptr;
        fmd->flow->verts_old = nullptr;
        fmd->flow->numverts = 0;
        BLO_read_data_address(reader, &fmd->flow->psys);
      }
      else if (fmd->type == MOD_FLUID_TYPE_EFFEC) {
        fmd->flow = nullptr;
        fmd->domain = nullptr;
        BLO_read_data_address(reader, &fmd->effector);
        if (fmd->effector) {
          fmd->effector->fmd = fmd;
          fmd->effector->verts_old = nullptr;
          fmd->effector->numverts = 0;
          fmd->effector->mesh = nullptr;
        }
        else {
          fmd->type = 0;
          fmd->flow = nullptr;
          fmd->domain = nullptr;
          fmd->effector = nullptr;
        }
      }
    }
    else if (md->type == eModifierType_DynamicPaint) {
      DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

      if (pmd->canvas) {
        BLO_read_data_address(reader, &pmd->canvas);
        pmd->canvas->pmd = pmd;
        pmd->canvas->flags &= ~MOD_DPAINT_BAKING; /* just in case */

        if (pmd->canvas->surfaces.first) {
          BLO_read_list(reader, &pmd->canvas->surfaces);

          LISTBASE_FOREACH (DynamicPaintSurface *, surface, &pmd->canvas->surfaces) {
            surface->canvas = pmd->canvas;
            surface->data = nullptr;
            BKE_ptcache_blend_read_data(reader, &(surface->ptcaches), &(surface->pointcache), 1);

            BLO_read_data_address(reader, &surface->effector_weights);
            if (surface->effector_weights == nullptr) {
              surface->effector_weights = BKE_effector_add_weights(nullptr);
            }
          }
        }
      }
      if (pmd->brush) {
        BLO_read_data_address(reader, &pmd->brush);
        pmd->brush->pmd = pmd;
        BLO_read_data_address(reader, &pmd->brush->psys);
        BLO_read_data_address(reader, &pmd->brush->paint_ramp);
        BLO_read_data_address(reader, &pmd->brush->vel_ramp);
      }
    }

    if ((mti != nullptr) && (mti->blendRead != nullptr)) {
      mti->blendRead(reader, md);
    }
  }
}

void BKE_modifier_blend_read_lib(BlendLibReader *reader, Object *ob)
{
  BKE_modifiers_foreach_ID_link(ob, BKE_object_modifiers_lib_link_common, reader);

  /* If linking from a library, clear 'local' library override flag. */
  if (ID_IS_LINKED(ob)) {
    LISTBASE_FOREACH (ModifierData *, mod, &ob->modifiers) {
      mod->flag &= ~eModifierFlag_OverrideLibrary_Local;
    }
  }
}

namespace blender::bke {

using Clock = std::chrono::high_resolution_clock;

static double get_current_time_in_seconds()
{
  return std::chrono::duration<double, std::chrono::seconds::period>(
             Clock::now().time_since_epoch())
      .count();
}

ScopedModifierTimer::ScopedModifierTimer(ModifierData &md) : md_(md)
{
  start_time_ = get_current_time_in_seconds();
}

ScopedModifierTimer::~ScopedModifierTimer()
{
  const double end_time = get_current_time_in_seconds();
  const double duration = end_time - start_time_;
  md_.execution_time = duration;
}

}  // namespace blender::bke
