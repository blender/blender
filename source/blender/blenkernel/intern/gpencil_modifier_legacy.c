/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. */

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
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h"
#include "BKE_deform.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lattice.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_shrinkwrap.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_legacy_lineart.h"
#include "MOD_gpencil_legacy_modifiertypes.h"

#include "BLO_read_write.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.gpencil_modifier"};
static GpencilModifierTypeInfo *modifier_gpencil_types[NUM_GREASEPENCIL_MODIFIER_TYPES] = {NULL};
#if 0
/* Note that GPencil actually does not support these at the moment, but might do in the future. */
static GpencilVirtualModifierData virtualModifierCommonData;
#endif

/* Lattice Modifier ---------------------------------- */
/* Usually, evaluation of the lattice modifier is self-contained.
 * However, since GP's modifiers operate on a per-stroke basis,
 * we need to these two extra functions that called before/after
 * each loop over all the geometry being evaluated.
 */

void BKE_gpencil_cache_data_init(Depsgraph *depsgraph, Object *ob)
{
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    switch (md->type) {
      case eGpencilModifierType_Lattice: {
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
        break;
      }
      case eGpencilModifierType_Shrinkwrap: {
        ShrinkwrapGpencilModifierData *mmd = (ShrinkwrapGpencilModifierData *)md;
        ob = mmd->target;
        if (!ob) {
          return;
        }
        if (mmd->cache_data) {
          BKE_shrinkwrap_free_tree(mmd->cache_data);
          MEM_SAFE_FREE(mmd->cache_data);
        }
        Object *ob_target = DEG_get_evaluated_object(depsgraph, ob);
        Mesh *target = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_target);
        mmd->cache_data = MEM_callocN(sizeof(ShrinkwrapTreeData), __func__);
        if (BKE_shrinkwrap_init_tree(
                mmd->cache_data, target, mmd->shrink_type, mmd->shrink_mode, false)) {
        }
        else {
          MEM_SAFE_FREE(mmd->cache_data);
        }
        break;
      }

      default:
        break;
    }
  }
}

void BKE_gpencil_cache_data_clear(Object *ob)
{
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    switch (md->type) {
      case eGpencilModifierType_Lattice: {
        LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;
        if ((mmd) && (mmd->cache_data)) {
          BKE_lattice_deform_data_destroy(mmd->cache_data);
          mmd->cache_data = NULL;
        }
        break;
      }
      case eGpencilModifierType_Shrinkwrap: {
        ShrinkwrapGpencilModifierData *mmd = (ShrinkwrapGpencilModifierData *)md;
        if ((mmd) && (mmd->cache_data)) {
          BKE_shrinkwrap_free_tree(mmd->cache_data);
          MEM_SAFE_FREE(mmd->cache_data);
        }
        break;
      }
      default:
        break;
    }
  }
}

/* *************************************************** */
/* Modifier Methods - Evaluation Loops, etc. */

GpencilModifierData *BKE_gpencil_modifiers_get_virtual_modifierlist(
    const Object *ob, GpencilVirtualModifierData *UNUSED(virtualModifierData))
{
  GpencilModifierData *md = ob->greasepencil_modifiers.first;

#if 0
  /* Note that GPencil actually does not support these at the moment,
   * but might do in the future. */
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

bool BKE_gpencil_has_transform_modifiers(Object *ob)
{
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    /* Only if enabled in edit mode. */
    if (!GPENCIL_MODIFIER_EDIT(md, true) && GPENCIL_MODIFIER_ACTIVE(md, false)) {
      if (ELEM(md->type,
               eGpencilModifierType_Armature,
               eGpencilModifierType_Hook,
               eGpencilModifierType_Lattice,
               eGpencilModifierType_Offset))
      {
        return true;
      }
    }
  }
  return false;
}

GpencilLineartLimitInfo BKE_gpencil_get_lineart_modifier_limits(const Object *ob)
{
  GpencilLineartLimitInfo info = {0};
  bool is_first = true;
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    if (md->type == eGpencilModifierType_Lineart) {
      LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
      if (is_first || (lmd->flags & LRT_GPENCIL_USE_CACHE)) {
        info.min_level = MIN2(info.min_level, lmd->level_start);
        info.max_level = MAX2(info.max_level,
                              (lmd->use_multiple_levels ? lmd->level_end : lmd->level_start));
        info.edge_types |= lmd->edge_types;
        info.shadow_selection = MAX2(lmd->shadow_selection, info.shadow_selection);
        info.silhouette_selection = MAX2(lmd->silhouette_selection, info.silhouette_selection);
        is_first = false;
      }
    }
  }
  return info;
}

void BKE_gpencil_set_lineart_modifier_limits(GpencilModifierData *md,
                                             const GpencilLineartLimitInfo *info,
                                             const bool is_first_lineart)
{
  BLI_assert(md->type == eGpencilModifierType_Lineart);
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
  if (is_first_lineart || lmd->flags & LRT_GPENCIL_USE_CACHE) {
    lmd->level_start_override = info->min_level;
    lmd->level_end_override = info->max_level;
    lmd->edge_types_override = info->edge_types;
    lmd->shadow_selection_override = info->shadow_selection;
    lmd->shadow_use_silhouette_override = info->silhouette_selection;
  }
  else {
    lmd->level_start_override = lmd->level_start;
    lmd->level_end_override = lmd->level_end;
    lmd->edge_types_override = lmd->edge_types;
    lmd->shadow_selection_override = lmd->shadow_selection;
    lmd->shadow_use_silhouette_override = lmd->silhouette_selection;
  }
}

bool BKE_gpencil_is_first_lineart_in_stack(const Object *ob, const GpencilModifierData *md)
{
  if (md->type != eGpencilModifierType_Lineart) {
    return false;
  }
  LISTBASE_FOREACH (GpencilModifierData *, gmd, &ob->greasepencil_modifiers) {
    if (gmd->type == eGpencilModifierType_Lineart) {
      if (gmd == md) {
        return true;
      }
      return false;
    }
  }
  /* If we reach here it means md is not in ob's modifier stack. */
  BLI_assert(false);
  return false;
}

int BKE_gpencil_time_modifier_cfra(Depsgraph *depsgraph,
                                   Scene *scene,
                                   Object *ob,
                                   bGPDlayer *gpl,
                                   const int cfra,
                                   const bool is_render)
{
  bGPdata *gpd = ob->data;
  const bool is_edit = GPENCIL_ANY_EDIT_MODE(gpd);
  int nfra = cfra;

  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    if (GPENCIL_MODIFIER_ACTIVE(md, is_render)) {
      const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

      if (GPENCIL_MODIFIER_EDIT(md, is_edit) && (!is_render)) {
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

void BKE_gpencil_modifier_init(void)
{
  /* Initialize modifier types */
  gpencil_modifier_type_init(modifier_gpencil_types); /* MOD_gpencil_legacy_util.c */

#if 0
  /* Note that GPencil actually does not support these at the moment,
   * but might do in the future. */
  /* Initialize global common storage used for virtual modifier list. */
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

GpencilModifierData *BKE_gpencil_modifier_new(int type)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(type);
  GpencilModifierData *md = MEM_callocN(mti->struct_size, mti->struct_name);

  /* NOTE: this name must be made unique later. */
  BLI_strncpy(md->name, DATA_(mti->name), sizeof(md->name));

  md->type = type;
  md->mode = eGpencilModifierMode_Realtime | eGpencilModifierMode_Render;
  md->flag = eGpencilModifierFlag_OverrideLibrary_Local;
  /* Only expand the parent panel at first. */
  md->ui_expand_flag = UI_PANEL_DATA_EXPAND_ROOT;

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

void BKE_gpencil_modifier_free(GpencilModifierData *md)
{
  BKE_gpencil_modifier_free_ex(md, 0);
}

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

bool BKE_gpencil_modifier_depends_ontime(GpencilModifierData *md)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

  return mti->dependsOnTime && mti->dependsOnTime(md);
}

const GpencilModifierTypeInfo *BKE_gpencil_modifier_get_info(GpencilModifierType type)
{
  /* type unsigned, no need to check < 0 */
  if (type < NUM_GREASEPENCIL_MODIFIER_TYPES && type > 0 &&
      modifier_gpencil_types[type]->name[0] != '\0')
  {
    return modifier_gpencil_types[type];
  }

  return NULL;
}

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

void BKE_gpencil_modifier_set_error(GpencilModifierData *md, const char *format, ...)
{
  char buffer[512];
  va_list ap;
  const char *format_tip = TIP_(format);

  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format_tip, ap);
  va_end(ap);
  buffer[sizeof(buffer) - 1] = '\0';

  if (md->error) {
    MEM_freeN(md->error);
  }

  md->error = BLI_strdup(buffer);

  CLOG_STR_ERROR(&LOG, md->error);
}

bool BKE_gpencil_modifier_is_nonlocal_in_liboverride(const Object *ob,
                                                     const GpencilModifierData *gmd)
{
  return (ID_IS_OVERRIDE_LIBRARY(ob) &&
          (gmd == NULL || (gmd->flag & eGpencilModifierFlag_OverrideLibrary_Local) == 0));
}

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
    remap_cfra = BKE_gpencil_time_modifier_cfra(depsgraph, scene, ob, gpl, cfra_eval, is_render);
  }

  return remap_cfra;
}

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

static bGPdata *gpencil_copy_structure_for_eval(bGPdata *gpd)
{
  /* Create a temporary copy gpd. */
  ID *newid = NULL;
  BKE_libblock_copy_ex(NULL, &gpd->id, &newid, LIB_ID_COPY_LOCALIZE);
  bGPdata *gpd_eval = (bGPdata *)newid;
  BLI_listbase_clear(&gpd_eval->layers);

  if (gpd->mat != NULL) {
    gpd_eval->mat = MEM_dupallocN(gpd->mat);
  }

  BKE_defgroup_copy_list(&gpd_eval->vertex_group_names, &gpd->vertex_group_names);

  /* Duplicate structure: layers and frames without strokes. */
  LISTBASE_FOREACH (bGPDlayer *, gpl_orig, &gpd->layers) {
    bGPDlayer *gpl_eval = BKE_gpencil_layer_duplicate(gpl_orig, true, false);
    BLI_addtail(&gpd_eval->layers, gpl_eval);
    gpl_eval->runtime.gpl_orig = gpl_orig;
    /* Update frames orig pointers (helps for faster lookup in copy_frame_to_eval_cb). */
    BKE_gpencil_layer_original_pointers_update(gpl_orig, gpl_eval);
  }

  return gpd_eval;
}

static void copy_frame_to_eval_ex(bGPDframe *gpf_orig, bGPDframe *gpf_eval)
{
  /* Free any existing eval stroke data. This happens in case we have a single user on the data
   * block and the strokes have not been deleted. */
  if (!BLI_listbase_is_empty(&gpf_eval->strokes)) {
    BKE_gpencil_free_strokes(gpf_eval);
  }
  /* Copy strokes to eval frame and update internal orig pointers. */
  BKE_gpencil_frame_copy_strokes(gpf_orig, gpf_eval);
  BKE_gpencil_frame_original_pointers_update(gpf_orig, gpf_eval);
}

static void copy_frame_to_eval_cb(bGPDlayer *gpl,
                                  bGPDframe *gpf,
                                  bGPDstroke *UNUSED(gps),
                                  void *UNUSED(thunk))
{
  /* Early return when callback:
   * - Is not provided with a frame.
   * - When the frame is the layer's active frame (already handled in
   * gpencil_copy_visible_frames_to_eval).
   */
  if (ELEM(gpf, NULL, gpl->actframe)) {
    return;
  }

  copy_frame_to_eval_ex(gpf->runtime.gpf_orig, gpf);
}

static void gpencil_copy_visible_frames_to_eval(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  /* Remap layers active frame with time modifiers applied. */
  bGPdata *gpd_eval = ob->data;
  LISTBASE_FOREACH (bGPDlayer *, gpl_eval, &gpd_eval->layers) {
    bGPDframe *gpf_eval = gpl_eval->actframe;
    int remap_cfra = gpencil_remap_time_get(depsgraph, scene, ob, gpl_eval);
    if (gpf_eval == NULL || gpf_eval->framenum != remap_cfra) {
      gpl_eval->actframe = BKE_gpencil_layer_frame_get(gpl_eval, remap_cfra, GP_GETFRAME_USE_PREV);
    }
    /* Always copy active frame to eval, because the modifiers always evaluate the active frame,
     * even if it's not visible (e.g. the layer is hidden). */
    if (gpl_eval->actframe != NULL) {
      copy_frame_to_eval_ex(gpl_eval->actframe->runtime.gpf_orig, gpl_eval->actframe);
    }
  }

  /* Copy visible frames that are not the active one to evaluated version. */
  BKE_gpencil_visible_stroke_advanced_iter(
      NULL, ob, copy_frame_to_eval_cb, NULL, NULL, true, scene->r.cfra);
}

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

    /* Only do layer transformations for non-zero or animated transforms. */
    bool transformed = (!is_zero_v3(gpl->location) || !is_zero_v3(gpl->rotation) ||
                        !is_one_v3(gpl->scale));
    float tmp_mat[4][4];
    loc_eul_size_to_mat4(tmp_mat, gpl->location, gpl->rotation, gpl->scale);
    transformed |= !equals_m4m4(gpl->layer_mat, tmp_mat);
    if (transformed) {
      do_transform = true;
      break;
    }
  }

  DEG_debug_print_eval(depsgraph, __func__, gpd_eval->id.name, gpd_eval);

  /* Delete any previously created runtime copy. */
  if (ob->runtime.gpd_eval != NULL) {
    /* Make sure to clear the pointer in case the runtime eval data points to the same data block.
     * This can happen when the gpencil data block was not tagged for a depsgraph update after last
     * call to this function (e.g. a frame change). */
    if (gpd_eval == ob->runtime.gpd_eval) {
      gpd_eval = NULL;
    }
    BKE_gpencil_eval_delete(ob->runtime.gpd_eval);
    ob->runtime.gpd_eval = NULL;
    ob->data = gpd_eval;
  }

  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_orig);
  const bool is_curve_edit = (bool)GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd_orig);
  const bool do_modifiers = (bool)((!is_multiedit) && (!is_curve_edit) &&
                                   (ob_orig->greasepencil_modifiers.first != NULL) &&
                                   !GPENCIL_SIMPLIFY_MODIF(scene));
  if ((!do_modifiers) && (!do_parent) && (!do_transform)) {
    BLI_assert(ob->data != NULL);
    return;
  }

  /* If datablock has only one user, we can update its eval data directly.
   * Otherwise, we need to have distinct copies for each instance, since applied transformations
   * may differ. */
  if (gpd_orig->id.us > 1) {
    /* Copy of the original datablock's structure (layers and empty frames). */
    ob->runtime.gpd_eval = gpencil_copy_structure_for_eval(gpd_orig);
    /* Overwrite ob->data with gpd_eval here. */
    gpencil_assign_object_eval(ob);
  }

  BLI_assert(ob->data != NULL);
  /* Only copy strokes from visible frames to evaluated data. */
  gpencil_copy_visible_frames_to_eval(depsgraph, scene, ob);
}

void BKE_gpencil_modifiers_calc(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_edit = GPENCIL_ANY_EDIT_MODE(gpd);
  const bool is_render = (bool)(DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const bool is_curve_edit = (bool)(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd) && !is_render);
  const bool is_multiedit = (bool)(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd) && !is_render);
  const bool do_modifiers = (bool)((!is_multiedit) && (!is_curve_edit) &&
                                   (ob->greasepencil_modifiers.first != NULL) &&
                                   !GPENCIL_SIMPLIFY_MODIF(scene));
  if (!do_modifiers) {
    return;
  }

  /* Init general modifiers data. */
  BKE_gpencil_cache_data_init(depsgraph, ob);

  const bool time_remap = BKE_gpencil_has_time_modifiers(ob);
  bool is_first_lineart = true;
  GpencilLineartLimitInfo info = BKE_gpencil_get_lineart_modifier_limits(ob);

  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {

    if (GPENCIL_MODIFIER_ACTIVE(md, is_render)) {
      const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

      if (GPENCIL_MODIFIER_EDIT(md, is_edit) && (!is_render)) {
        continue;
      }

      if (md->type == eGpencilModifierType_Lineart) {
        BKE_gpencil_set_lineart_modifier_limits(md, &info, is_first_lineart);
        is_first_lineart = false;
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

  /* Clear any cache data. */
  BKE_gpencil_cache_data_clear(ob);

  MOD_lineart_clear_cache(&gpd->runtime.lineart_cache);
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
    else if (md->type == eGpencilModifierType_Dash) {
      DashGpencilModifierData *gpmd = (DashGpencilModifierData *)md;
      BLO_write_struct_array(
          writer, DashGpencilModifierSegment, gpmd->segments_len, gpmd->segments);
    }
    else if (md->type == eGpencilModifierType_Time) {
      TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)md;
      BLO_write_struct_array(
          writer, TimeGpencilModifierSegment, gpmd->segments_len, gpmd->segments);
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
        /* Initialize the curve. Maybe this could be moved to modifier logic. */
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
    else if (md->type == eGpencilModifierType_Dash) {
      DashGpencilModifierData *gpmd = (DashGpencilModifierData *)md;
      BLO_read_data_address(reader, &gpmd->segments);
      for (int i = 0; i < gpmd->segments_len; i++) {
        gpmd->segments[i].dmd = gpmd;
      }
    }
    else if (md->type == eGpencilModifierType_Time) {
      TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)md;
      BLO_read_data_address(reader, &gpmd->segments);
      for (int i = 0; i < gpmd->segments_len; i++) {
        gpmd->segments[i].gpmd = gpmd;
      }
    }
    if (md->type == eGpencilModifierType_Shrinkwrap) {
      ShrinkwrapGpencilModifierData *gpmd = (ShrinkwrapGpencilModifierData *)md;
      gpmd->cache_data = NULL;
    }
  }
}

void BKE_gpencil_modifier_blend_read_lib(BlendLibReader *reader, Object *ob)
{
  BKE_gpencil_modifiers_foreach_ID_link(ob, BKE_object_modifiers_lib_link_common, reader);

  /* If linking from a library, clear 'local' library override flag. */
  if (ID_IS_LINKED(ob)) {
    LISTBASE_FOREACH (GpencilModifierData *, mod, &ob->greasepencil_modifiers) {
      mod->flag &= ~eGpencilModifierFlag_OverrideLibrary_Local;
    }
  }
}
