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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup bke
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_armature_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lattice.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_material.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_modifiertypes.h"

#include "BLO_read_write.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.gpencil_modifier"};
static GpencilModifierTypeInfo *modifier_gpencil_types[NUM_GREASEPENCIL_MODIFIER_TYPES] = {NULL};
#if 0
/* Note that GPencil actually does not support these atm, but might do in the future. */
static GpencilVirtualModifierData virtualModifierCommonData;
#endif

/* Lattice Modifier ---------------------------------- */
/* Usually, evaluation of the lattice modifier is self-contained.
 * However, since GP's modifiers operate on a per-stroke basis,
 * we need to these two extra functions that called before/after
 * each loop over all the geometry being evaluated.
 */

/**
 * Init grease pencil lattice deform data.
 * \param ob: Grease pencil object
 */
void BKE_gpencil_lattice_init(Object *ob)
{
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    if (md->type == eGpencilModifierType_Lattice) {
      LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;
      Object *latob = NULL;

      latob = mmd->object;
      if ((!latob) || (latob->type != OB_LATTICE)) {
        return;
      }
      if (mmd->cache_data) {
        BKE_lattice_deform_data_destroy(mmd->cache_data);
      }

      /* init deform data */
      mmd->cache_data = BKE_lattice_deform_data_create(latob, ob);
    }
  }
}

/**
 * Clear grease pencil lattice deform data.
 * \param ob: Grease pencil object
 */
void BKE_gpencil_lattice_clear(Object *ob)
{
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    if (md->type == eGpencilModifierType_Lattice) {
      LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;
      if ((mmd) && (mmd->cache_data)) {
        BKE_lattice_deform_data_destroy(mmd->cache_data);
        mmd->cache_data = NULL;
      }
    }
  }
}

/* *************************************************** */
/* Modifier Methods - Evaluation Loops, etc. */

/* This is to include things that are not modifiers in the evaluation of the modifier stack, for
 * example parenting to an armature or lattice without having a real modifier. */
GpencilModifierData *BKE_gpencil_modifiers_get_virtual_modifierlist(
    const Object *ob, GpencilVirtualModifierData *UNUSED(virtualModifierData))
{
  GpencilModifierData *md = ob->greasepencil_modifiers.first;

#if 0
  /* Note that GPencil actually does not support these atm, but might do in the future. */
  *virtualModifierData = virtualModifierCommonData;
  if (ob->parent) {
    if (ob->parent->type == OB_ARMATURE && ob->partype == PARSKEL) {
      virtualModifierData->amd.object = ob->parent;
      virtualModifierData->amd.modifier.next = md;
      virtualModifierData->amd.deformflag = ((bArmature *)(ob->parent->data))->deformflag;
      md = &virtualModifierData->amd.modifier;
    }
    else if (ob->parent->type == OB_LATTICE && ob->partype == PARSKEL) {
      virtualModifierData->lmd.object = ob->parent;
      virtualModifierData->lmd.modifier.next = md;
      md = &virtualModifierData->lmd.modifier;
    }
  }
#endif

  return md;
}

/**
 * Check if object has grease pencil Geometry modifiers.
 * \param ob: Grease pencil object
 * \return True if exist
 */
bool BKE_gpencil_has_geometry_modifiers(Object *ob)
{
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

    if (mti && mti->generateStrokes) {
      return true;
    }
  }
  return false;
}

/**
 * Check if object has grease pencil Time modifiers.
 * \param ob: Grease pencil object
 * \return True if exist
 */
bool BKE_gpencil_has_time_modifiers(Object *ob)
{
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

    if (mti && mti->remapTime) {
      return true;
    }
  }
  return false;
}

/**
 * Check if object has grease pencil transform stroke modifiers.
 * \param ob: Grease pencil object
 * \return True if exist
 */
bool BKE_gpencil_has_transform_modifiers(Object *ob)
{
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    /* Only if enabled in edit mode. */
    if (!GPENCIL_MODIFIER_EDIT(md, true) && GPENCIL_MODIFIER_ACTIVE(md, false)) {
      if ((md->type == eGpencilModifierType_Armature) || (md->type == eGpencilModifierType_Hook) ||
          (md->type == eGpencilModifierType_Lattice) ||
          (md->type == eGpencilModifierType_Offset)) {
        return true;
      }
    }
  }
  return false;
}

/* apply time modifiers */
static int gpencil_time_modifier(
    Depsgraph *depsgraph, Scene *scene, Object *ob, bGPDlayer *gpl, int cfra, bool is_render)
{
  bGPdata *gpd = ob->data;
  const bool is_edit = GPENCIL_ANY_EDIT_MODE(gpd);
  int nfra = cfra;

  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    if (GPENCIL_MODIFIER_ACTIVE(md, is_render)) {
      const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

      if ((GPENCIL_MODIFIER_EDIT(md, is_edit)) && (!is_render)) {
        continue;
      }

      if (mti->remapTime) {
        nfra = mti->remapTime(md, depsgraph, scene, ob, gpl, cfra);
        /* if the frame number changed, don't evaluate more and return */
        if (nfra != cfra) {
          return nfra;
        }
      }
    }
  }

  /* if no time modifier, return original frame number */
  return nfra;
}

/**
 * Set current grease pencil active frame.
 * \param depsgraph: Current depsgraph
 * \param gpd: Grease pencil data-block.
 */
void BKE_gpencil_frame_active_set(Depsgraph *depsgraph, bGPdata *gpd)
{
  DEG_debug_print_eval(depsgraph, __func__, gpd->id.name, gpd);
  int ctime = (int)DEG_get_ctime(depsgraph);

  /* update active frame */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    gpl->actframe = BKE_gpencil_layer_frame_get(gpl, ctime, GP_GETFRAME_USE_PREV);
  }

  if (DEG_is_active(depsgraph)) {
    bGPdata *gpd_orig = (bGPdata *)DEG_get_original_id(&gpd->id);

    /* sync "actframe" changes back to main-db too,
     * so that editing tools work with copy-on-write
     * when the current frame changes
     */
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_orig->layers) {
      gpl->actframe = BKE_gpencil_layer_frame_get(gpl, ctime, GP_GETFRAME_USE_PREV);
    }
  }
}

/**
 * Initialize grease pencil modifier.
 */
void BKE_gpencil_modifier_init(void)
{
  /* Initialize modifier types */
  gpencil_modifier_type_init(modifier_gpencil_types); /* MOD_gpencil_util.c */

#if 0
  /* Note that GPencil actually does not support these atm, but might do in the future. */
  /* Initialize global cmmon storage used for virtual modifier list */
  GpencilModifierData *md;
  md = BKE_gpencil_modifier_new(eGpencilModifierType_Armature);
  virtualModifierCommonData.amd = *((ArmatureGpencilModifierData *)md);
  BKE_gpencil_modifier_free(md);

  md = BKE_gpencil_modifier_new(eGpencilModifierType_Lattice);
  virtualModifierCommonData.lmd = *((LatticeGpencilModifierData *)md);
  BKE_gpencil_modifier_free(md);

  virtualModifierCommonData.amd.modifier.mode |= eGpencilModifierMode_Virtual;
  virtualModifierCommonData.lmd.modifier.mode |= eGpencilModifierMode_Virtual;
#endif
}

/**
 * Create new grease pencil modifier.
 * \param type: Type of modifier
 * \return New modifier pointer
 */
GpencilModifierData *BKE_gpencil_modifier_new(int type)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(type);
  GpencilModifierData *md = MEM_callocN(mti->struct_size, mti->struct_name);

  /* note, this name must be made unique later */
  BLI_strncpy(md->name, DATA_(mti->name), sizeof(md->name));

  md->type = type;
  md->mode = eGpencilModifierMode_Realtime | eGpencilModifierMode_Render;
  md->flag = eGpencilModifierFlag_OverrideLibrary_Local;
  md->ui_expand_flag = 1; /* Only expand the parent panel at first. */

  if (mti->flags & eGpencilModifierTypeFlag_EnableInEditmode) {
    md->mode |= eGpencilModifierMode_Editmode;
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

/**
 * Free grease pencil modifier data
 * \param md: Modifier data
 * \param flag: Flags
 */
void BKE_gpencil_modifier_free_ex(GpencilModifierData *md, const int flag)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (mti->foreachIDLink) {
      mti->foreachIDLink(md, NULL, modifier_free_data_id_us_cb, NULL);
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

/**
 * Free grease pencil modifier data
 * \param md: Modifier data
 */
void BKE_gpencil_modifier_free(GpencilModifierData *md)
{
  BKE_gpencil_modifier_free_ex(md, 0);
}

/* check unique name */
bool BKE_gpencil_modifier_unique_name(ListBase *modifiers, GpencilModifierData *gmd)
{
  if (modifiers && gmd) {
    const GpencilModifierTypeInfo *gmti = BKE_gpencil_modifier_get_info(gmd->type);
    return BLI_uniquename(modifiers,
                          gmd,
                          DATA_(gmti->name),
                          '.',
                          offsetof(GpencilModifierData, name),
                          sizeof(gmd->name));
  }
  return false;
}

/**
 * Check if grease pencil modifier depends on time.
 * \param md: Modifier data
 * \return True if depends on time
 */
bool BKE_gpencil_modifier_depends_ontime(GpencilModifierData *md)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

  return mti->dependsOnTime && mti->dependsOnTime(md);
}

/**
 * Get grease pencil modifier information.
 * \param type: Type of modifier
 * \return Pointer to type
 */
const GpencilModifierTypeInfo *BKE_gpencil_modifier_get_info(GpencilModifierType type)
{
  /* type unsigned, no need to check < 0 */
  if (type < NUM_GREASEPENCIL_MODIFIER_TYPES && type > 0 &&
      modifier_gpencil_types[type]->name[0] != '\0') {
    return modifier_gpencil_types[type];
  }

  return NULL;
}

/**
 * Get the idname of the modifier type's panel, which was defined in the #panelRegister callback.
 *
 * \param type: Type of modifier
 * \param r_idname: ID name
 */
void BKE_gpencil_modifierType_panel_id(GpencilModifierType type, char *r_idname)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(type);

  strcpy(r_idname, GPENCIL_MODIFIER_TYPE_PANEL_PREFIX);
  strcat(r_idname, mti->name);
}

void BKE_gpencil_modifier_panel_expand(GpencilModifierData *md)
{
  md->ui_expand_flag |= UI_PANEL_DATA_EXPAND_ROOT;
}

/**
 * Generic grease pencil modifier copy data.
 * \param md_src: Source modifier data
 * \param md_dst: Target modifier data
 */
void BKE_gpencil_modifier_copydata_generic(const GpencilModifierData *md_src,
                                           GpencilModifierData *md_dst)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md_src->type);

  /* md_dst may have already be fully initialized with some extra allocated data,
   * we need to free it now to avoid memleak. */
  if (mti->freeData) {
    mti->freeData(md_dst);
  }

  const size_t data_size = sizeof(GpencilModifierData);
  const char *md_src_data = ((const char *)md_src) + data_size;
  char *md_dst_data = ((char *)md_dst) + data_size;
  BLI_assert(data_size <= (size_t)mti->struct_size);
  memcpy(md_dst_data, md_src_data, (size_t)mti->struct_size - data_size);
}

static void gpencil_modifier_copy_data_id_us_cb(void *UNUSED(userData),
                                                Object *UNUSED(ob),
                                                ID **idpoin,
                                                int cb_flag)
{
  ID *id = *idpoin;
  if (id != NULL && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_plus(id);
  }
}

/**
 * Copy grease pencil modifier data.
 * \param md: Source modifier data
 * \param target: Target modifier data
 * \parm flag: Flags
 */
void BKE_gpencil_modifier_copydata_ex(GpencilModifierData *md,
                                      GpencilModifierData *target,
                                      const int flag)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

  target->mode = md->mode;
  target->flag = md->flag;
  target->ui_expand_flag = md->ui_expand_flag; /* Expand the parent panel by default. */

  if (mti->copyData) {
    mti->copyData(md, target);
  }

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (mti->foreachIDLink) {
      mti->foreachIDLink(target, NULL, gpencil_modifier_copy_data_id_us_cb, NULL);
    }
  }
}

/**
 * Copy grease pencil modifier data.
 * \param md: Source modifier data
 * \param target: Target modifier data
 */
void BKE_gpencil_modifier_copydata(GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_ex(md, target, 0);
}

GpencilModifierData *BKE_gpencil_modifiers_findby_type(Object *ob, GpencilModifierType type)
{
  GpencilModifierData *md = ob->greasepencil_modifiers.first;

  for (; md; md = md->next) {
    if (md->type == type) {
      break;
    }
  }

  return md;
}

/**
 * Set grease pencil modifier error.
 * \param md: Modifier data
 * \param _format: Format
 */
void BKE_gpencil_modifier_set_error(GpencilModifierData *md, const char *_format, ...)
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

/**
 * Check whether given modifier is not local (i.e. from linked data) when the object is a library
 * override.
 *
 * \param gmd: May be NULL, in which case we consider it as a non-local modifier case.
 */
bool BKE_gpencil_modifier_is_nonlocal_in_liboverride(const Object *ob,
                                                     const GpencilModifierData *gmd)
{
  return (ID_IS_OVERRIDE_LIBRARY(ob) &&
          (gmd == NULL || (gmd->flag & eGpencilModifierFlag_OverrideLibrary_Local) == 0));
}

/**
 * Link grease pencil modifier related IDs.
 * \param ob: Grease pencil object
 * \param walk: Walk option
 * \param userData: User data
 */
void BKE_gpencil_modifiers_foreach_ID_link(Object *ob, GreasePencilIDWalkFunc walk, void *userData)
{
  GpencilModifierData *md = ob->greasepencil_modifiers.first;

  for (; md; md = md->next) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

    if (mti->foreachIDLink) {
      mti->foreachIDLink(md, ob, walk, userData);
    }
  }
}

/**
 * Link grease pencil modifier related Texts.
 * \param ob: Grease pencil object
 * \param walk: Walk option
 * \param userData: User data
 */
void BKE_gpencil_modifiers_foreach_tex_link(Object *ob,
                                            GreasePencilTexWalkFunc walk,
                                            void *userData)
{
  GpencilModifierData *md = ob->greasepencil_modifiers.first;

  for (; md; md = md->next) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

    if (mti->foreachTexLink) {
      mti->foreachTexLink(md, ob, walk, userData);
    }
  }
}

/**
 * Find grease pencil modifier by name.
 * \param ob: Grease pencil object
 * \param name: Name to find
 * \return Pointer to modifier
 */
GpencilModifierData *BKE_gpencil_modifiers_findby_name(Object *ob, const char *name)
{
  return BLI_findstring(&(ob->greasepencil_modifiers), name, offsetof(GpencilModifierData, name));
}

/**
 * Remap grease pencil frame (Time modifier)
 * \param depsgraph: Current depsgraph
 * \param scene: Current scene
 * \param ob: Grease pencil object
 * \param gpl: Grease pencil layer
 * \return New frame number
 */
static int gpencil_remap_time_get(Depsgraph *depsgraph, Scene *scene, Object *ob, bGPDlayer *gpl)
{
  const bool is_render = (bool)(DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const bool time_remap = BKE_gpencil_has_time_modifiers(ob);
  int cfra_eval = (int)DEG_get_ctime(depsgraph);

  int remap_cfra = cfra_eval;
  if (time_remap) {
    remap_cfra = gpencil_time_modifier(depsgraph, scene, ob, gpl, cfra_eval, is_render);
  }

  return remap_cfra;
}

/** Get the current frame re-timed with time modifiers.
 * \param depsgraph: Current depsgraph.
 * \param scene: Current scene
 * \param ob: Grease pencil object
 * \param gpl: Grease pencil layer
 * \return New frame number
 */
bGPDframe *BKE_gpencil_frame_retime_get(Depsgraph *depsgraph,
                                        Scene *scene,
                                        Object *ob,
                                        bGPDlayer *gpl)
{
  int remap_cfra = gpencil_remap_time_get(depsgraph, scene, ob, gpl);
  bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, remap_cfra, GP_GETFRAME_USE_PREV);

  return gpf;
}

static void gpencil_assign_object_eval(Object *object)
{
  BLI_assert(object->id.tag & LIB_TAG_COPIED_ON_WRITE);

  bGPdata *gpd_eval = object->runtime.gpd_eval;

  gpd_eval->id.tag |= LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT;

  if (object->id.tag & LIB_TAG_COPIED_ON_WRITE) {
    object->data = gpd_eval;
  }
}

/* Helper: Copy active frame from original datablock to evaluated datablock for modifiers. */
static void gpencil_copy_activeframe_to_eval(
    Depsgraph *depsgraph, Scene *scene, Object *ob, bGPdata *gpd_orig, bGPdata *gpd_eval)
{

  bGPDlayer *gpl_eval = gpd_eval->layers.first;
  LISTBASE_FOREACH (bGPDlayer *, gpl_orig, &gpd_orig->layers) {

    if (gpl_eval != NULL) {
      bGPDframe *gpf_orig = gpl_orig->actframe;

      int remap_cfra = gpencil_remap_time_get(depsgraph, scene, ob, gpl_orig);
      if (gpf_orig && gpf_orig->framenum != remap_cfra) {
        gpf_orig = BKE_gpencil_layer_frame_get(gpl_orig, remap_cfra, GP_GETFRAME_USE_PREV);
      }

      if (gpf_orig != NULL) {
        int gpf_index = BLI_findindex(&gpl_orig->frames, gpf_orig);
        bGPDframe *gpf_eval = BLI_findlink(&gpl_eval->frames, gpf_index);

        if (gpf_eval != NULL) {
          /* Delete old strokes. */
          BKE_gpencil_free_strokes(gpf_eval);
          /* Copy again strokes. */
          BKE_gpencil_frame_copy_strokes(gpf_orig, gpf_eval);

          gpf_eval->runtime.gpf_orig = (bGPDframe *)gpf_orig;
          BKE_gpencil_frame_original_pointers_update(gpf_orig, gpf_eval);
        }
      }

      gpl_eval = gpl_eval->next;
    }
  }
}

static bGPdata *gpencil_copy_for_eval(bGPdata *gpd)
{
  const int flags = LIB_ID_COPY_LOCALIZE;

  bGPdata *result = (bGPdata *)BKE_id_copy_ex(NULL, &gpd->id, NULL, flags);
  return result;
}

/**
 * Prepare grease pencil eval data for modifiers
 * \param depsgraph: Current depsgraph
 * \param scene: Current scene
 * \param ob: Grease pencil object
 */
void BKE_gpencil_prepare_eval_data(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  bGPdata *gpd_eval = (bGPdata *)ob->data;
  Object *ob_orig = (Object *)DEG_get_original_id(&ob->id);
  bGPdata *gpd_orig = (bGPdata *)ob_orig->data;

  /* Need check if some layer is parented or transformed. */
  bool do_parent = false;
  bool do_transform = false;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_orig->layers) {
    if (gpl->parent != NULL) {
      do_parent = true;
      break;
    }
    if ((!is_zero_v3(gpl->location)) || (!is_zero_v3(gpl->rotation)) || (!is_one_v3(gpl->scale))) {
      do_transform = true;
      break;
    }
  }

  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_eval);
  const bool is_curve_edit = (bool)GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd_eval);
  const bool do_modifiers = (bool)((!is_multiedit) && (!is_curve_edit) &&
                                   (ob->greasepencil_modifiers.first != NULL) &&
                                   (!GPENCIL_SIMPLIFY_MODIF(scene)));
  if ((!do_modifiers) && (!do_parent) && (!do_transform)) {
    return;
  }
  DEG_debug_print_eval(depsgraph, __func__, gpd_eval->id.name, gpd_eval);

  /* If only one user, don't need a new copy, just update data of the frame. */
  if (gpd_orig->id.us == 1) {
    ob->runtime.gpd_eval = NULL;
    gpencil_copy_activeframe_to_eval(depsgraph, scene, ob, ob_orig->data, gpd_eval);
    return;
  }

  /* Copy full Datablock to evaluated version. */
  ob->runtime.gpd_orig = gpd_orig;
  if (ob->runtime.gpd_eval != NULL) {
    BKE_gpencil_eval_delete(ob->runtime.gpd_eval);
    ob->runtime.gpd_eval = NULL;
    ob->data = ob->runtime.gpd_orig;
  }
  ob->runtime.gpd_eval = gpencil_copy_for_eval(ob->runtime.gpd_orig);
  gpencil_assign_object_eval(ob);
  BKE_gpencil_update_orig_pointers(ob_orig, ob);
}

/** Calculate gpencil modifiers.
 * \param depsgraph: Current depsgraph
 * \param scene: Current scene
 * \param ob: Grease pencil object
 */
void BKE_gpencil_modifiers_calc(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_edit = GPENCIL_ANY_EDIT_MODE(gpd);
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  const bool is_curve_edit = (bool)GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd);
  const bool is_render = (bool)(DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const bool do_modifiers = (bool)((!is_multiedit) && (!is_curve_edit) &&
                                   (ob->greasepencil_modifiers.first != NULL) &&
                                   (!GPENCIL_SIMPLIFY_MODIF(scene)));
  if (!do_modifiers) {
    return;
  }

  /* Init general modifiers data. */
  BKE_gpencil_lattice_init(ob);

  const bool time_remap = BKE_gpencil_has_time_modifiers(ob);

  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {

    if (GPENCIL_MODIFIER_ACTIVE(md, is_render)) {
      const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

      if ((GPENCIL_MODIFIER_EDIT(md, is_edit)) && (!is_render)) {
        continue;
      }

      /* Apply geometry modifiers (add new geometry). */
      if (mti && mti->generateStrokes) {
        mti->generateStrokes(md, depsgraph, ob);
      }

      /* Apply deform modifiers and Time remap (only change geometry). */
      if ((time_remap) || (mti && mti->deformStroke)) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
          if (gpf == NULL) {
            continue;
          }

          if (mti->deformStroke) {
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
              mti->deformStroke(md, depsgraph, ob, gpl, gpf, gps);
            }
          }
        }
      }
    }
  }

  /* Clear any lattice data. */
  BKE_gpencil_lattice_clear(ob);
}

void BKE_gpencil_modifier_blend_write(BlendWriter *writer, ListBase *modbase)
{
  if (modbase == NULL) {
    return;
  }

  LISTBASE_FOREACH (GpencilModifierData *, md, modbase) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);
    if (mti == NULL) {
      return;
    }

    BLO_write_struct_by_name(writer, mti->struct_name, md);

    if (md->type == eGpencilModifierType_Thick) {
      ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;

      if (gpmd->curve_thickness) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_thickness);
      }
    }
    else if (md->type == eGpencilModifierType_Noise) {
      NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;

      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Hook) {
      HookGpencilModifierData *gpmd = (HookGpencilModifierData *)md;

      if (gpmd->curfalloff) {
        BKE_curvemapping_blend_write(writer, gpmd->curfalloff);
      }
    }
    else if (md->type == eGpencilModifierType_Tint) {
      TintGpencilModifierData *gpmd = (TintGpencilModifierData *)md;
      if (gpmd->colorband) {
        BLO_write_struct(writer, ColorBand, gpmd->colorband);
      }
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Smooth) {
      SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Color) {
      ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Opacity) {
      OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
  }
}

void BKE_gpencil_modifier_blend_read_data(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_list(reader, lb);

  LISTBASE_FOREACH (GpencilModifierData *, md, lb) {
    md->error = NULL;

    /* if modifiers disappear, or for upward compatibility */
    if (NULL == BKE_gpencil_modifier_get_info(md->type)) {
      md->type = eModifierType_None;
    }

    if (md->type == eGpencilModifierType_Lattice) {
      LatticeGpencilModifierData *gpmd = (LatticeGpencilModifierData *)md;
      gpmd->cache_data = NULL;
    }
    else if (md->type == eGpencilModifierType_Hook) {
      HookGpencilModifierData *hmd = (HookGpencilModifierData *)md;

      BLO_read_data_address(reader, &hmd->curfalloff);
      if (hmd->curfalloff) {
        BKE_curvemapping_blend_read(reader, hmd->curfalloff);
      }
    }
    else if (md->type == eGpencilModifierType_Noise) {
      NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;

      BLO_read_data_address(reader, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        /* initialize the curve. Maybe this could be moved to modififer logic */
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Thick) {
      ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;

      BLO_read_data_address(reader, &gpmd->curve_thickness);
      if (gpmd->curve_thickness) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_thickness);
        BKE_curvemapping_init(gpmd->curve_thickness);
      }
    }
    else if (md->type == eGpencilModifierType_Tint) {
      TintGpencilModifierData *gpmd = (TintGpencilModifierData *)md;
      BLO_read_data_address(reader, &gpmd->colorband);
      BLO_read_data_address(reader, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Smooth) {
      SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;
      BLO_read_data_address(reader, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Color) {
      ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;
      BLO_read_data_address(reader, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Opacity) {
      OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;
      BLO_read_data_address(reader, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
  }
}

void BKE_gpencil_modifier_blend_read_lib(BlendLibReader *reader, Object *ob)
{
  BKE_gpencil_modifiers_foreach_ID_link(ob, BKE_object_modifiers_lib_link_common, reader);

  /* If linking from a library, clear 'local' library override flag. */
  if (ob->id.lib != NULL) {
    LISTBASE_FOREACH (GpencilModifierData *, mod, &ob->greasepencil_modifiers) {
      mod->flag &= ~eGpencilModifierFlag_OverrideLibrary_Local;
    }
  }
}
