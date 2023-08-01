/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdio>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"

#include "BKE_gpencil_legacy.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_object.h"
#include "BKE_screen.h"
#include "BKE_shader_fx.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "FX_shader_types.h"

#include "BLO_read_write.h"

static ShaderFxTypeInfo *shader_fx_types[NUM_SHADER_FX_TYPES] = {nullptr};

/* *************************************************** */
/* Methods - Evaluation Loops, etc. */

bool BKE_shaderfx_has_gpencil(const Object *ob)
{
  const ShaderFxData *fx;
  for (fx = static_cast<const ShaderFxData *>(ob->shader_fx.first); fx; fx = fx->next) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));
    if (fxi->type == eShaderFxType_GpencilType) {
      return true;
    }
  }
  return false;
}

void BKE_shaderfx_init()
{
  /* Initialize shaders */
  shaderfx_type_init(shader_fx_types); /* `FX_shader_util.cc`. */
}

ShaderFxData *BKE_shaderfx_new(int type)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(type));
  ShaderFxData *fx = static_cast<ShaderFxData *>(MEM_callocN(fxi->struct_size, fxi->struct_name));

  /* NOTE: this name must be made unique later. */
  STRNCPY_UTF8(fx->name, DATA_(fxi->name));

  fx->type = type;
  fx->mode = eShaderFxMode_Realtime | eShaderFxMode_Render;
  fx->flag = eShaderFxFlag_OverrideLibrary_Local;
  /* Expand only the parent panel by default. */
  fx->ui_expand_flag = UI_PANEL_DATA_EXPAND_ROOT;

  if (fxi->flags & eShaderFxTypeFlag_EnableInEditmode) {
    fx->mode |= eShaderFxMode_Editmode;
  }

  if (fxi->init_data) {
    fxi->init_data(fx);
  }

  return fx;
}

static void shaderfx_free_data_id_us_cb(void * /*user_data*/,
                                        Object * /*ob*/,
                                        ID **idpoin,
                                        int cb_flag)
{
  ID *id = *idpoin;
  if (id != nullptr && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_min(id);
  }
}

void BKE_shaderfx_free_ex(ShaderFxData *fx, const int flag)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (fxi->foreach_ID_link) {
      fxi->foreach_ID_link(fx, nullptr, shaderfx_free_data_id_us_cb, nullptr);
    }
  }

  if (fxi->free_data) {
    fxi->free_data(fx);
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

bool BKE_shaderfx_unique_name(ListBase *shaders, ShaderFxData *fx)
{
  if (shaders && fx) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));
    return BLI_uniquename(
        shaders, fx, DATA_(fxi->name), '.', offsetof(ShaderFxData, name), sizeof(fx->name));
  }
  return false;
}

bool BKE_shaderfx_depends_ontime(ShaderFxData *fx)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));

  return fxi->depends_on_time && fxi->depends_on_time(fx);
}

const ShaderFxTypeInfo *BKE_shaderfx_get_info(ShaderFxType type)
{
  /* type unsigned, no need to check < 0 */
  if (type < NUM_SHADER_FX_TYPES && type > 0 && shader_fx_types[type]->name[0] != '\0') {
    return shader_fx_types[type];
  }

  return nullptr;
}

bool BKE_shaderfx_is_nonlocal_in_liboverride(const Object *ob, const ShaderFxData *shaderfx)
{
  return (ID_IS_OVERRIDE_LIBRARY(ob) &&
          ((shaderfx == nullptr) || (shaderfx->flag & eShaderFxFlag_OverrideLibrary_Local) == 0));
}

void BKE_shaderfxType_panel_id(ShaderFxType type, char *r_idname)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(type);
  BLI_string_join(r_idname, BKE_ST_MAXNAME, SHADERFX_TYPE_PANEL_PREFIX, fxi->name);
}

void BKE_shaderfx_panel_expand(ShaderFxData *fx)
{
  fx->ui_expand_flag |= UI_PANEL_DATA_EXPAND_ROOT;
}

void BKE_shaderfx_copydata_generic(const ShaderFxData *fx_src, ShaderFxData *fx_dst)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx_src->type));

  /* fx_dst may have already be fully initialized with some extra allocated data,
   * we need to free it now to avoid memleak. */
  if (fxi->free_data) {
    fxi->free_data(fx_dst);
  }

  const size_t data_size = sizeof(ShaderFxData);
  const char *fx_src_data = ((const char *)fx_src) + data_size;
  char *fx_dst_data = ((char *)fx_dst) + data_size;
  BLI_assert(data_size <= size_t(fxi->struct_size));
  memcpy(fx_dst_data, fx_src_data, size_t(fxi->struct_size) - data_size);
}

static void shaderfx_copy_data_id_us_cb(void * /*user_data*/,
                                        Object * /*ob*/,
                                        ID **idpoin,
                                        int cb_flag)
{
  ID *id = *idpoin;
  if (id != nullptr && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_plus(id);
  }
}

void BKE_shaderfx_copydata_ex(ShaderFxData *fx, ShaderFxData *target, const int flag)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));

  target->mode = fx->mode;
  target->flag = fx->flag;
  target->ui_expand_flag = fx->ui_expand_flag;

  if (fxi->copy_data) {
    fxi->copy_data(fx, target);
  }

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (fxi->foreach_ID_link) {
      fxi->foreach_ID_link(target, nullptr, shaderfx_copy_data_id_us_cb, nullptr);
    }
  }
}

void BKE_shaderfx_copydata(ShaderFxData *fx, ShaderFxData *target)
{
  BKE_shaderfx_copydata_ex(fx, target, 0);
}

void BKE_shaderfx_copy(ListBase *dst, const ListBase *src)
{
  ShaderFxData *fx;
  ShaderFxData *srcfx;

  BLI_listbase_clear(dst);
  BLI_duplicatelist(dst, src);

  for (srcfx = static_cast<ShaderFxData *>(src->first),
      fx = static_cast<ShaderFxData *>(dst->first);
       srcfx && fx;
       srcfx = srcfx->next, fx = fx->next)
  {
    BKE_shaderfx_copydata(srcfx, fx);
  }
}

ShaderFxData *BKE_shaderfx_findby_type(Object *ob, ShaderFxType type)
{
  ShaderFxData *fx = static_cast<ShaderFxData *>(ob->shader_fx.first);

  for (; fx; fx = fx->next) {
    if (fx->type == type) {
      break;
    }
  }

  return fx;
}

void BKE_shaderfx_foreach_ID_link(Object *ob, ShaderFxIDWalkFunc walk, void *user_data)
{
  ShaderFxData *fx = static_cast<ShaderFxData *>(ob->shader_fx.first);

  for (; fx; fx = fx->next) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));

    if (fxi->foreach_ID_link) {
      fxi->foreach_ID_link(fx, ob, walk, user_data);
    }
  }
}

ShaderFxData *BKE_shaderfx_findby_name(Object *ob, const char *name)
{
  return static_cast<ShaderFxData *>(
      BLI_findstring(&(ob->shader_fx), name, offsetof(ShaderFxData, name)));
}

void BKE_shaderfx_blend_write(BlendWriter *writer, ListBase *fxbase)
{
  if (fxbase == nullptr) {
    return;
  }

  LISTBASE_FOREACH (ShaderFxData *, fx, fxbase) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));
    if (fxi == nullptr) {
      return;
    }

    BLO_write_struct_by_name(writer, fxi->struct_name, fx);
  }
}

void BKE_shaderfx_blend_read_data(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_list(reader, lb);

  LISTBASE_FOREACH (ShaderFxData *, fx, lb) {
    fx->error = nullptr;

    /* if shader disappear, or for upward compatibility */
    if (nullptr == BKE_shaderfx_get_info(ShaderFxType(fx->type))) {
      fx->type = eShaderFxType_None;
    }
  }
}

void BKE_shaderfx_blend_read_lib(BlendLibReader *reader, Object *ob)
{
  BKE_shaderfx_foreach_ID_link(ob, BKE_object_modifiers_lib_link_common, reader);

  /* If linking from a library, clear 'local' library override flag. */
  if (ID_IS_LINKED(ob)) {
    LISTBASE_FOREACH (ShaderFxData *, fx, &ob->shader_fx) {
      fx->flag &= ~eShaderFxFlag_OverrideLibrary_Local;
    }
  }
}
