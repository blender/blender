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
 * The Original Code is Copyright (C) 2018, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup bke
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_shader_fx_types.h"

#include "BKE_gpencil.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_object.h"
#include "BKE_shader_fx.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "FX_shader_types.h"

static ShaderFxTypeInfo *shader_fx_types[NUM_SHADER_FX_TYPES] = {NULL};

/* *************************************************** */
/* Methods - Evaluation Loops, etc. */

/* check if exist grease pencil effects */
bool BKE_shaderfx_has_gpencil(Object *ob)
{
  ShaderFxData *fx;
  for (fx = ob->shader_fx.first; fx; fx = fx->next) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(fx->type);
    if (fxi->type == eShaderFxType_GpencilType) {
      return true;
    }
  }
  return false;
}

void BKE_shaderfx_init(void)
{
  /* Initialize shaders */
  shaderfx_type_init(shader_fx_types); /* FX_shader_util.c */
}

ShaderFxData *BKE_shaderfx_new(int type)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(type);
  ShaderFxData *fx = MEM_callocN(fxi->struct_size, fxi->struct_name);

  /* note, this name must be made unique later */
  BLI_strncpy(fx->name, DATA_(fxi->name), sizeof(fx->name));

  fx->type = type;
  fx->mode = eShaderFxMode_Realtime | eShaderFxMode_Render;
  fx->flag = eShaderFxFlag_OverrideLibrary_Local;
  fx->ui_expand_flag = 1; /* Expand only the parent panel by default. */

  if (fxi->flags & eShaderFxTypeFlag_EnableInEditmode) {
    fx->mode |= eShaderFxMode_Editmode;
  }

  if (fxi->initData) {
    fxi->initData(fx);
  }

  return fx;
}

static void shaderfx_free_data_id_us_cb(void *UNUSED(userData),
                                        Object *UNUSED(ob),
                                        ID **idpoin,
                                        int cb_flag)
{
  ID *id = *idpoin;
  if (id != NULL && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_min(id);
  }
}

void BKE_shaderfx_free_ex(ShaderFxData *fx, const int flag)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(fx->type);

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (fxi->foreachIDLink) {
      fxi->foreachIDLink(fx, NULL, shaderfx_free_data_id_us_cb, NULL);
    }
    else if (fxi->foreachObjectLink) {
      fxi->foreachObjectLink(fx, NULL, (ShaderFxObjectWalkFunc)shaderfx_free_data_id_us_cb, NULL);
    }
  }

  if (fxi->freeData) {
    fxi->freeData(fx);
  }
  if (fx->error) {
    MEM_freeN(fx->error);
  }

  MEM_freeN(fx);
}

void BKE_shaderfx_free(ShaderFxData *fx)
{
  BKE_shaderfx_free_ex(fx, 0);
}

/* check unique name */
bool BKE_shaderfx_unique_name(ListBase *shaders, ShaderFxData *fx)
{
  if (shaders && fx) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(fx->type);
    return BLI_uniquename(
        shaders, fx, DATA_(fxi->name), '.', offsetof(ShaderFxData, name), sizeof(fx->name));
  }
  return false;
}

bool BKE_shaderfx_depends_ontime(ShaderFxData *fx)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(fx->type);

  return fxi->dependsOnTime && fxi->dependsOnTime(fx);
}

const ShaderFxTypeInfo *BKE_shaderfx_get_info(ShaderFxType type)
{
  /* type unsigned, no need to check < 0 */
  if (type < NUM_SHADER_FX_TYPES && type > 0 && shader_fx_types[type]->name[0] != '\0') {
    return shader_fx_types[type];
  }
  else {
    return NULL;
  }
}

/**
 * Get an effect's panel type, which was defined in the #panelRegister callback.
 *
 * \note ShaderFx panel types are assumed to be named with the struct name field concatenated to
 * the defined prefix.
 */
void BKE_shaderfxType_panel_id(ShaderFxType type, char *r_idname)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(type);

  strcpy(r_idname, SHADERFX_TYPE_PANEL_PREFIX);
  strcat(r_idname, fxi->name);
}

void BKE_shaderfx_copydata_generic(const ShaderFxData *fx_src, ShaderFxData *fx_dst)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(fx_src->type);

  /* fx_dst may have already be fully initialized with some extra allocated data,
   * we need to free it now to avoid memleak. */
  if (fxi->freeData) {
    fxi->freeData(fx_dst);
  }

  const size_t data_size = sizeof(ShaderFxData);
  const char *fx_src_data = ((const char *)fx_src) + data_size;
  char *fx_dst_data = ((char *)fx_dst) + data_size;
  BLI_assert(data_size <= (size_t)fxi->struct_size);
  memcpy(fx_dst_data, fx_src_data, (size_t)fxi->struct_size - data_size);
}

static void shaderfx_copy_data_id_us_cb(void *UNUSED(userData),
                                        Object *UNUSED(ob),
                                        ID **idpoin,
                                        int cb_flag)
{
  ID *id = *idpoin;
  if (id != NULL && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_plus(id);
  }
}

void BKE_shaderfx_copydata_ex(ShaderFxData *fx, ShaderFxData *target, const int flag)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(fx->type);

  target->mode = fx->mode;
  target->flag = fx->flag;
  target->ui_expand_flag = fx->ui_expand_flag;

  if (fxi->copyData) {
    fxi->copyData(fx, target);
  }

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (fxi->foreachIDLink) {
      fxi->foreachIDLink(target, NULL, shaderfx_copy_data_id_us_cb, NULL);
    }
    else if (fxi->foreachObjectLink) {
      fxi->foreachObjectLink(
          target, NULL, (ShaderFxObjectWalkFunc)shaderfx_copy_data_id_us_cb, NULL);
    }
  }
}

void BKE_shaderfx_copydata(ShaderFxData *fx, ShaderFxData *target)
{
  BKE_shaderfx_copydata_ex(fx, target, 0);
}

ShaderFxData *BKE_shaderfx_findby_type(Object *ob, ShaderFxType type)
{
  ShaderFxData *fx = ob->shader_fx.first;

  for (; fx; fx = fx->next) {
    if (fx->type == type) {
      break;
    }
  }

  return fx;
}

void BKE_shaderfx_foreach_ID_link(Object *ob, ShaderFxIDWalkFunc walk, void *userData)
{
  ShaderFxData *fx = ob->shader_fx.first;

  for (; fx; fx = fx->next) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(fx->type);

    if (fxi->foreachIDLink) {
      fxi->foreachIDLink(fx, ob, walk, userData);
    }
    else if (fxi->foreachObjectLink) {
      /* each Object can masquerade as an ID, so this should be OK */
      ShaderFxObjectWalkFunc fp = (ShaderFxObjectWalkFunc)walk;
      fxi->foreachObjectLink(fx, ob, fp, userData);
    }
  }
}

ShaderFxData *BKE_shaderfx_findby_name(Object *ob, const char *name)
{
  return BLI_findstring(&(ob->shader_fx), name, offsetof(ShaderFxData, name));
}
