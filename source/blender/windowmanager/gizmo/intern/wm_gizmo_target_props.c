/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_types.h"

#include "wm.h"

#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_view3d.h"

/* own includes */
#include "wm_gizmo_intern.h"
#include "wm_gizmo_wmapi.h"

/* -------------------------------------------------------------------- */
/** \name Property Definition
 * \{ */

BLI_INLINE wmGizmoProperty *wm_gizmo_target_property_array(wmGizmo *gz)
{
  return (wmGizmoProperty *)POINTER_OFFSET(gz, gz->type->struct_size);
}

wmGizmoProperty *WM_gizmo_target_property_array(wmGizmo *gz)
{
  return wm_gizmo_target_property_array(gz);
}

wmGizmoProperty *WM_gizmo_target_property_at_index(wmGizmo *gz, int index)
{
  BLI_assert(index < gz->type->target_property_defs_len);
  BLI_assert(index != -1);
  wmGizmoProperty *gz_prop_array = wm_gizmo_target_property_array(gz);
  return &gz_prop_array[index];
}

wmGizmoProperty *WM_gizmo_target_property_find(wmGizmo *gz, const char *idname)
{
  int index = BLI_findstringindex(
      &gz->type->target_property_defs, idname, offsetof(wmGizmoPropertyType, idname));
  if (index != -1) {
    return WM_gizmo_target_property_at_index(gz, index);
  }
  return NULL;
}

void WM_gizmo_target_property_def_rna_ptr(wmGizmo *gz,
                                          const wmGizmoPropertyType *gz_prop_type,
                                          PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          int index)
{
  wmGizmoProperty *gz_prop = WM_gizmo_target_property_at_index(gz, gz_prop_type->index_in_type);

  /* if gizmo evokes an operator we cannot use it for property manipulation */
  BLI_assert(gz->op_data == NULL);
  BLI_assert(prop != NULL);

  gz_prop->type = gz_prop_type;

  gz_prop->ptr = *ptr;
  gz_prop->prop = prop;
  gz_prop->index = index;

  if (gz->type->property_update) {
    gz->type->property_update(gz, gz_prop);
  }
}

void WM_gizmo_target_property_def_rna(
    wmGizmo *gz, const char *idname, PointerRNA *ptr, const char *propname, int index)
{
  const wmGizmoPropertyType *gz_prop_type = WM_gizmotype_target_property_find(gz->type, idname);
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  if (prop == NULL) {
    RNA_warning("%s: %s.%s not found", __func__, RNA_struct_identifier(ptr->type), propname);
  }
  WM_gizmo_target_property_def_rna_ptr(gz, gz_prop_type, ptr, prop, index);
}

void WM_gizmo_target_property_def_func_ptr(wmGizmo *gz,
                                           const wmGizmoPropertyType *gz_prop_type,
                                           const wmGizmoPropertyFnParams *params)
{
  wmGizmoProperty *gz_prop = WM_gizmo_target_property_at_index(gz, gz_prop_type->index_in_type);

  /* if gizmo evokes an operator we cannot use it for property manipulation */
  BLI_assert(gz->op_data == NULL);

  gz_prop->type = gz_prop_type;

  gz_prop->custom_func.value_get_fn = params->value_get_fn;
  gz_prop->custom_func.value_set_fn = params->value_set_fn;
  gz_prop->custom_func.range_get_fn = params->range_get_fn;
  gz_prop->custom_func.free_fn = params->free_fn;
  gz_prop->custom_func.user_data = params->user_data;

  if (gz->type->property_update) {
    gz->type->property_update(gz, gz_prop);
  }
}

void WM_gizmo_target_property_def_func(wmGizmo *gz,
                                       const char *idname,
                                       const wmGizmoPropertyFnParams *params)
{
  const wmGizmoPropertyType *gz_prop_type = WM_gizmotype_target_property_find(gz->type, idname);
  WM_gizmo_target_property_def_func_ptr(gz, gz_prop_type, params);
}

void WM_gizmo_target_property_clear_rna_ptr(wmGizmo *gz, const wmGizmoPropertyType *gz_prop_type)
{
  wmGizmoProperty *gz_prop = WM_gizmo_target_property_at_index(gz, gz_prop_type->index_in_type);

  /* if gizmo evokes an operator we cannot use it for property manipulation */
  BLI_assert(gz->op_data == NULL);

  gz_prop->type = NULL;

  gz_prop->ptr = PointerRNA_NULL;
  gz_prop->prop = NULL;
  gz_prop->index = -1;
}

void WM_gizmo_target_property_clear_rna(wmGizmo *gz, const char *idname)
{
  const wmGizmoPropertyType *gz_prop_type = WM_gizmotype_target_property_find(gz->type, idname);
  WM_gizmo_target_property_clear_rna_ptr(gz, gz_prop_type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Property Access
 * \{ */

bool WM_gizmo_target_property_is_valid_any(wmGizmo *gz)
{
  wmGizmoProperty *gz_prop_array = wm_gizmo_target_property_array(gz);
  for (int i = 0; i < gz->type->target_property_defs_len; i++) {
    wmGizmoProperty *gz_prop = &gz_prop_array[i];
    if (WM_gizmo_target_property_is_valid(gz_prop)) {
      return true;
    }
  }
  return false;
}

bool WM_gizmo_target_property_is_valid(const wmGizmoProperty *gz_prop)
{
  return ((gz_prop->prop != NULL) ||
          (gz_prop->custom_func.value_get_fn && gz_prop->custom_func.value_set_fn));
}

float WM_gizmo_target_property_float_get(const wmGizmo *gz, wmGizmoProperty *gz_prop)
{
  if (gz_prop->custom_func.value_get_fn) {
    float value = 0.0f;
    BLI_assert(gz_prop->type->array_length == 1);
    gz_prop->custom_func.value_get_fn(gz, gz_prop, &value);
    return value;
  }

  if (gz_prop->index == -1) {
    return RNA_property_float_get(&gz_prop->ptr, gz_prop->prop);
  }
  return RNA_property_float_get_index(&gz_prop->ptr, gz_prop->prop, gz_prop->index);
}

void WM_gizmo_target_property_float_set(bContext *C,
                                        const wmGizmo *gz,
                                        wmGizmoProperty *gz_prop,
                                        const float value)
{
  if (gz_prop->custom_func.value_set_fn) {
    BLI_assert(gz_prop->type->array_length == 1);
    gz_prop->custom_func.value_set_fn(gz, gz_prop, &value);
    return;
  }

  /* reset property */
  if (gz_prop->index == -1) {
    RNA_property_float_set(&gz_prop->ptr, gz_prop->prop, value);
  }
  else {
    RNA_property_float_set_index(&gz_prop->ptr, gz_prop->prop, gz_prop->index, value);
  }
  RNA_property_update(C, &gz_prop->ptr, gz_prop->prop);
}

void WM_gizmo_target_property_float_get_array(const wmGizmo *gz,
                                              wmGizmoProperty *gz_prop,
                                              float *value)
{
  if (gz_prop->custom_func.value_get_fn) {
    gz_prop->custom_func.value_get_fn(gz, gz_prop, value);
    return;
  }
  RNA_property_float_get_array(&gz_prop->ptr, gz_prop->prop, value);
}

void WM_gizmo_target_property_float_set_array(bContext *C,
                                              const wmGizmo *gz,
                                              wmGizmoProperty *gz_prop,
                                              const float *value)
{
  if (gz_prop->custom_func.value_set_fn) {
    gz_prop->custom_func.value_set_fn(gz, gz_prop, value);
    return;
  }
  RNA_property_float_set_array(&gz_prop->ptr, gz_prop->prop, value);

  RNA_property_update(C, &gz_prop->ptr, gz_prop->prop);
}

bool WM_gizmo_target_property_float_range_get(const wmGizmo *gz,
                                              wmGizmoProperty *gz_prop,
                                              float range[2])
{
  if (gz_prop->custom_func.value_get_fn) {
    if (gz_prop->custom_func.range_get_fn) {
      gz_prop->custom_func.range_get_fn(gz, gz_prop, range);
      return true;
    }
    return false;
  }

  float step, precision;
  RNA_property_float_ui_range(
      &gz_prop->ptr, gz_prop->prop, &range[0], &range[1], &step, &precision);
  return true;
}

int WM_gizmo_target_property_array_length(const wmGizmo *UNUSED(gz), wmGizmoProperty *gz_prop)
{
  if (gz_prop->custom_func.value_get_fn) {
    return gz_prop->type->array_length;
  }
  return RNA_property_array_length(&gz_prop->ptr, gz_prop->prop);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Property Define
 * \{ */

const wmGizmoPropertyType *WM_gizmotype_target_property_find(const wmGizmoType *gzt,
                                                             const char *idname)
{
  return BLI_findstring(&gzt->target_property_defs, idname, offsetof(wmGizmoPropertyType, idname));
}

void WM_gizmotype_target_property_def(wmGizmoType *gzt,
                                      const char *idname,
                                      int data_type,
                                      int array_length)
{

  BLI_assert(WM_gizmotype_target_property_find(gzt, idname) == NULL);

  const uint idname_size = strlen(idname) + 1;
  wmGizmoPropertyType *gz_prop_type = MEM_callocN(sizeof(wmGizmoPropertyType) + idname_size,
                                                  __func__);
  memcpy(gz_prop_type->idname, idname, idname_size);
  gz_prop_type->data_type = data_type;
  gz_prop_type->array_length = array_length;
  gz_prop_type->index_in_type = gzt->target_property_defs_len;
  gzt->target_property_defs_len += 1;
  BLI_addtail(&gzt->target_property_defs, gz_prop_type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Property Utilities
 * \{ */

void WM_gizmo_do_msg_notify_tag_refresh(bContext *UNUSED(C),
                                        wmMsgSubscribeKey *UNUSED(msg_key),
                                        wmMsgSubscribeValue *msg_val)
{
  ARegion *region = msg_val->owner;
  wmGizmoMap *gzmap = msg_val->user_data;

  /* Could possibly avoid a full redraw and only tag for editor overlays
   * redraw in some cases, see #ED_region_tag_redraw_editor_overlays(). */
  ED_region_tag_redraw(region);

  WM_gizmomap_tag_refresh(gzmap);
}

void WM_gizmo_target_property_subscribe_all(wmGizmo *gz, struct wmMsgBus *mbus, ARegion *region)
{
  if (gz->type->target_property_defs_len) {
    wmGizmoProperty *gz_prop_array = WM_gizmo_target_property_array(gz);
    for (int i = 0; i < gz->type->target_property_defs_len; i++) {
      wmGizmoProperty *gz_prop = &gz_prop_array[i];
      if (WM_gizmo_target_property_is_valid(gz_prop)) {
        if (gz_prop->prop) {
          WM_msg_subscribe_rna(mbus,
                               &gz_prop->ptr,
                               gz_prop->prop,
                               &(const wmMsgSubscribeValue){
                                   .owner = region,
                                   .user_data = region,
                                   .notify = ED_region_do_msg_notify_tag_redraw,
                               },
                               __func__);
          WM_msg_subscribe_rna(mbus,
                               &gz_prop->ptr,
                               gz_prop->prop,
                               &(const wmMsgSubscribeValue){
                                   .owner = region,
                                   .user_data = gz->parent_gzgroup->parent_gzmap,
                                   .notify = WM_gizmo_do_msg_notify_tag_refresh,
                               },
                               __func__);
        }
      }
    }
  }
}

void WM_gizmo_target_property_anim_autokey(bContext *C,
                                           const wmGizmo *UNUSED(gz),
                                           wmGizmoProperty *gz_prop)
{
  if (gz_prop->prop != NULL) {
    Scene *scene = CTX_data_scene(C);
    const float cfra = (float)scene->r.cfra;
    const int index = gz_prop->index == -1 ? 0 : gz_prop->index;
    ED_autokeyframe_property(C, scene, &gz_prop->ptr, gz_prop->prop, index, cfra, false);
  }
}

/** \} */
