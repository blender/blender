/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Window-Manager XR Action Maps
 *
 * XR actionmap API, similar to WM keymap API.
 */

#include <cmath>
#include <cstring>

#include "BKE_context.hh"
#include "BKE_idprop.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "GHOST_Types.h"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_xr_intern.hh"

#define WM_XR_ACTIONMAP_STR_DEFAULT "actionmap"
#define WM_XR_ACTIONMAP_ITEM_STR_DEFAULT "action"
#define WM_XR_ACTIONMAP_BINDING_STR_DEFAULT "binding"

/* -------------------------------------------------------------------- */
/** \name Action Map Binding
 *
 * Binding in an XR action map item, that maps an action to an XR input.
 * \{ */

XrActionMapBinding *WM_xr_actionmap_binding_new(XrActionMapItem *ami,
                                                const char *name,
                                                bool replace_existing)
{
  XrActionMapBinding *amb_prev = WM_xr_actionmap_binding_find(ami, name);
  if (amb_prev && replace_existing) {
    return amb_prev;
  }

  XrActionMapBinding *amb = static_cast<XrActionMapBinding *>(
      MEM_callocN(sizeof(XrActionMapBinding), __func__));
  STRNCPY(amb->name, name);
  if (amb_prev) {
    WM_xr_actionmap_binding_ensure_unique(ami, amb);
  }

  BLI_addtail(&ami->bindings, amb);

  /* Set non-zero threshold by default. */
  amb->float_threshold = 0.3f;

  return amb;
}

static XrActionMapBinding *wm_xr_actionmap_binding_find_except(XrActionMapItem *ami,
                                                               const char *name,
                                                               XrActionMapBinding *ambexcept)
{
  LISTBASE_FOREACH (XrActionMapBinding *, amb, &ami->bindings) {
    if (STREQLEN(name, amb->name, MAX_NAME) && (amb != ambexcept)) {
      return amb;
    }
  }
  return nullptr;
}

void WM_xr_actionmap_binding_ensure_unique(XrActionMapItem *ami, XrActionMapBinding *amb)
{
  char name[MAX_NAME];
  char *suffix;
  size_t baselen;
  size_t idx = 0;

  STRNCPY(name, amb->name);
  baselen = BLI_strnlen(name, MAX_NAME);
  suffix = &name[baselen];

  while (wm_xr_actionmap_binding_find_except(ami, name, amb)) {
    if ((baselen + 1) + (log10(++idx) + 1) > MAX_NAME) {
      /* Use default base name. */
      STRNCPY(name, WM_XR_ACTIONMAP_BINDING_STR_DEFAULT);
      baselen = BLI_strnlen(name, MAX_NAME);
      suffix = &name[baselen];
      idx = 0;
    }
    else {
      BLI_snprintf(suffix, MAX_NAME, "%zu", idx);
    }
  }

  STRNCPY(amb->name, name);
}

static XrActionMapBinding *wm_xr_actionmap_binding_copy(XrActionMapBinding *amb_src)
{
  XrActionMapBinding *amb_dst = static_cast<XrActionMapBinding *>(MEM_dupallocN(amb_src));
  amb_dst->prev = amb_dst->next = nullptr;

  BLI_listbase_clear(&amb_dst->component_paths);
  LISTBASE_FOREACH (XrComponentPath *, path, &amb_src->component_paths) {
    XrComponentPath *path_new = static_cast<XrComponentPath *>(MEM_dupallocN(path));
    BLI_addtail(&amb_dst->component_paths, path_new);
  }

  return amb_dst;
}

XrActionMapBinding *WM_xr_actionmap_binding_add_copy(XrActionMapItem *ami,
                                                     XrActionMapBinding *amb_src)
{
  XrActionMapBinding *amb_dst = wm_xr_actionmap_binding_copy(amb_src);

  WM_xr_actionmap_binding_ensure_unique(ami, amb_dst);

  BLI_addtail(&ami->bindings, amb_dst);

  return amb_dst;
}

static void wm_xr_actionmap_binding_clear(XrActionMapBinding *amb)
{
  BLI_freelistN(&amb->component_paths);
}

bool WM_xr_actionmap_binding_remove(XrActionMapItem *ami, XrActionMapBinding *amb)
{
  int idx = BLI_findindex(&ami->bindings, amb);

  if (idx != -1) {
    wm_xr_actionmap_binding_clear(amb);
    BLI_freelinkN(&ami->bindings, amb);

    if (idx <= ami->selbinding) {
      if (--ami->selbinding < 0) {
        ami->selbinding = 0;
      }
    }

    return true;
  }

  return false;
}

XrActionMapBinding *WM_xr_actionmap_binding_find(XrActionMapItem *ami, const char *name)
{
  LISTBASE_FOREACH (XrActionMapBinding *, amb, &ami->bindings) {
    if (STREQLEN(name, amb->name, MAX_NAME)) {
      return amb;
    }
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Action Map Item
 *
 * Item in an XR action map, that maps an XR event to an operator, pose, or haptic output.
 * \{ */

static void wm_xr_actionmap_item_properties_set(XrActionMapItem *ami)
{
  WM_operator_properties_alloc(&(ami->op_properties_ptr), &(ami->op_properties), ami->op);
  WM_operator_properties_sanitize(ami->op_properties_ptr, true);
}

static void wm_xr_actionmap_item_properties_free(XrActionMapItem *ami)
{
  if (ami->op_properties_ptr) {
    WM_operator_properties_free(ami->op_properties_ptr);
    MEM_freeN(ami->op_properties_ptr);
    ami->op_properties_ptr = nullptr;
    ami->op_properties = nullptr;
  }
  else {
    BLI_assert(ami->op_properties == nullptr);
  }
}

static void wm_xr_actionmap_item_clear(XrActionMapItem *ami)
{
  LISTBASE_FOREACH (XrActionMapBinding *, amb, &ami->bindings) {
    wm_xr_actionmap_binding_clear(amb);
  }
  BLI_freelistN(&ami->bindings);
  ami->selbinding = 0;

  wm_xr_actionmap_item_properties_free(ami);

  BLI_freelistN(&ami->user_paths);
}

void WM_xr_actionmap_item_properties_update_ot(XrActionMapItem *ami)
{
  switch (ami->type) {
    case XR_BOOLEAN_INPUT:
    case XR_FLOAT_INPUT:
    case XR_VECTOR2F_INPUT:
      break;
    case XR_POSE_INPUT:
    case XR_VIBRATION_OUTPUT:
      wm_xr_actionmap_item_properties_free(ami);
      memset(ami->op, 0, sizeof(ami->op));
      return;
  }

  if (ami->op[0] == 0) {
    wm_xr_actionmap_item_properties_free(ami);
    return;
  }

  if (ami->op_properties_ptr == nullptr) {
    wm_xr_actionmap_item_properties_set(ami);
  }
  else {
    wmOperatorType *ot = WM_operatortype_find(ami->op, false);
    if (ot) {
      if (ot->srna != ami->op_properties_ptr->type) {
        /* Matches wm_xr_actionmap_item_properties_set() but doesn't alloc new ptr. */
        WM_operator_properties_create_ptr(ami->op_properties_ptr, ot);
        if (ami->op_properties) {
          ami->op_properties_ptr->data = ami->op_properties;
        }
        WM_operator_properties_sanitize(ami->op_properties_ptr, true);
      }
    }
    else {
      wm_xr_actionmap_item_properties_free(ami);
    }
  }
}

XrActionMapItem *WM_xr_actionmap_item_new(XrActionMap *actionmap,
                                          const char *name,
                                          bool replace_existing)
{
  XrActionMapItem *ami_prev = WM_xr_actionmap_item_find(actionmap, name);
  if (ami_prev && replace_existing) {
    wm_xr_actionmap_item_properties_free(ami_prev);
    return ami_prev;
  }

  XrActionMapItem *ami = static_cast<XrActionMapItem *>(
      MEM_callocN(sizeof(XrActionMapItem), __func__));
  STRNCPY(ami->name, name);
  if (ami_prev) {
    WM_xr_actionmap_item_ensure_unique(actionmap, ami);
  }

  BLI_addtail(&actionmap->items, ami);

  /* Set type to float (button) input by default. */
  ami->type = XR_FLOAT_INPUT;

  return ami;
}

static XrActionMapItem *wm_xr_actionmap_item_find_except(XrActionMap *actionmap,
                                                         const char *name,
                                                         const XrActionMapItem *amiexcept)
{
  LISTBASE_FOREACH (XrActionMapItem *, ami, &actionmap->items) {
    if (STREQLEN(name, ami->name, MAX_NAME) && (ami != amiexcept)) {
      return ami;
    }
  }
  return nullptr;
}

void WM_xr_actionmap_item_ensure_unique(XrActionMap *actionmap, XrActionMapItem *ami)
{
  char name[MAX_NAME];
  char *suffix;
  size_t baselen;
  size_t idx = 0;

  STRNCPY(name, ami->name);
  baselen = BLI_strnlen(name, MAX_NAME);
  suffix = &name[baselen];

  while (wm_xr_actionmap_item_find_except(actionmap, name, ami)) {
    if ((baselen + 1) + (log10(++idx) + 1) > MAX_NAME) {
      /* Use default base name. */
      STRNCPY(name, WM_XR_ACTIONMAP_ITEM_STR_DEFAULT);
      baselen = BLI_strnlen(name, MAX_NAME);
      suffix = &name[baselen];
      idx = 0;
    }
    else {
      BLI_snprintf(suffix, MAX_NAME, "%zu", idx);
    }
  }

  STRNCPY(ami->name, name);
}

static XrActionMapItem *wm_xr_actionmap_item_copy(XrActionMapItem *ami_src)
{
  XrActionMapItem *ami_dst = static_cast<XrActionMapItem *>(MEM_dupallocN(ami_src));
  ami_dst->prev = ami_dst->next = nullptr;

  BLI_listbase_clear(&ami_dst->bindings);
  LISTBASE_FOREACH (XrActionMapBinding *, amb, &ami_src->bindings) {
    XrActionMapBinding *amb_new = wm_xr_actionmap_binding_copy(amb);
    BLI_addtail(&ami_dst->bindings, amb_new);
  }

  if (ami_dst->op_properties) {
    ami_dst->op_properties_ptr = static_cast<PointerRNA *>(
        MEM_callocN(sizeof(PointerRNA), "wmOpItemPtr"));
    WM_operator_properties_create(ami_dst->op_properties_ptr, ami_dst->op);
    ami_dst->op_properties = IDP_CopyProperty(ami_src->op_properties);
    ami_dst->op_properties_ptr->data = ami_dst->op_properties;
  }
  else {
    ami_dst->op_properties = nullptr;
    ami_dst->op_properties_ptr = nullptr;
  }

  BLI_listbase_clear(&ami_dst->user_paths);
  LISTBASE_FOREACH (XrUserPath *, path, &ami_src->user_paths) {
    XrUserPath *path_new = static_cast<XrUserPath *>(MEM_dupallocN(path));
    BLI_addtail(&ami_dst->user_paths, path_new);
  }

  return ami_dst;
}

XrActionMapItem *WM_xr_actionmap_item_add_copy(XrActionMap *actionmap, XrActionMapItem *ami_src)
{
  XrActionMapItem *ami_dst = wm_xr_actionmap_item_copy(ami_src);

  WM_xr_actionmap_item_ensure_unique(actionmap, ami_dst);

  BLI_addtail(&actionmap->items, ami_dst);

  return ami_dst;
}

bool WM_xr_actionmap_item_remove(XrActionMap *actionmap, XrActionMapItem *ami)
{
  int idx = BLI_findindex(&actionmap->items, ami);

  if (idx != -1) {
    wm_xr_actionmap_item_clear(ami);
    BLI_freelinkN(&actionmap->items, ami);

    if (idx <= actionmap->selitem) {
      if (--actionmap->selitem < 0) {
        actionmap->selitem = 0;
      }
    }

    return true;
  }

  return false;
}

XrActionMapItem *WM_xr_actionmap_item_find(XrActionMap *actionmap, const char *name)
{
  LISTBASE_FOREACH (XrActionMapItem *, ami, &actionmap->items) {
    if (STREQLEN(name, ami->name, MAX_NAME)) {
      return ami;
    }
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Action Map
 *
 * List of XR action map items.
 * \{ */

XrActionMap *WM_xr_actionmap_new(wmXrRuntimeData *runtime, const char *name, bool replace_existing)
{
  XrActionMap *am_prev = WM_xr_actionmap_find(runtime, name);
  if (am_prev && replace_existing) {
    WM_xr_actionmap_clear(am_prev);
    return am_prev;
  }

  XrActionMap *am = static_cast<XrActionMap *>(MEM_callocN(sizeof(XrActionMap), __func__));
  STRNCPY(am->name, name);
  if (am_prev) {
    WM_xr_actionmap_ensure_unique(runtime, am);
  }

  BLI_addtail(&runtime->actionmaps, am);

  return am;
}

static XrActionMap *wm_xr_actionmap_find_except(wmXrRuntimeData *runtime,
                                                const char *name,
                                                const XrActionMap *am_except)
{
  LISTBASE_FOREACH (XrActionMap *, am, &runtime->actionmaps) {
    if (STREQLEN(name, am->name, MAX_NAME) && (am != am_except)) {
      return am;
    }
  }

  return nullptr;
}

void WM_xr_actionmap_ensure_unique(wmXrRuntimeData *runtime, XrActionMap *actionmap)
{
  char name[MAX_NAME];
  char *suffix;
  size_t baselen;
  size_t idx = 0;

  STRNCPY(name, actionmap->name);
  baselen = BLI_strnlen(name, MAX_NAME);
  suffix = &name[baselen];

  while (wm_xr_actionmap_find_except(runtime, name, actionmap)) {
    if ((baselen + 1) + (log10(++idx) + 1) > MAX_NAME) {
      /* Use default base name. */
      STRNCPY(name, WM_XR_ACTIONMAP_STR_DEFAULT);
      baselen = BLI_strnlen(name, MAX_NAME);
      suffix = &name[baselen];
      idx = 0;
    }
    else {
      BLI_snprintf(suffix, MAX_NAME, "%zu", idx);
    }
  }

  STRNCPY(actionmap->name, name);
}

static XrActionMap *wm_xr_actionmap_copy(XrActionMap *am_src)
{
  XrActionMap *am_dst = static_cast<XrActionMap *>(MEM_dupallocN(am_src));
  am_dst->prev = am_dst->next = nullptr;

  BLI_listbase_clear(&am_dst->items);
  LISTBASE_FOREACH (XrActionMapItem *, ami, &am_src->items) {
    XrActionMapItem *ami_new = wm_xr_actionmap_item_copy(ami);
    BLI_addtail(&am_dst->items, ami_new);
  }

  return am_dst;
}

XrActionMap *WM_xr_actionmap_add_copy(wmXrRuntimeData *runtime, XrActionMap *am_src)
{
  XrActionMap *am_dst = wm_xr_actionmap_copy(am_src);

  WM_xr_actionmap_ensure_unique(runtime, am_dst);

  BLI_addtail(&runtime->actionmaps, am_dst);

  return am_dst;
}

bool WM_xr_actionmap_remove(wmXrRuntimeData *runtime, XrActionMap *actionmap)
{
  int idx = BLI_findindex(&runtime->actionmaps, actionmap);

  if (idx != -1) {
    WM_xr_actionmap_clear(actionmap);
    BLI_freelinkN(&runtime->actionmaps, actionmap);

    if (idx <= runtime->actactionmap) {
      if (--runtime->actactionmap < 0) {
        runtime->actactionmap = 0;
      }
    }
    if (idx <= runtime->selactionmap) {
      if (--runtime->selactionmap < 0) {
        runtime->selactionmap = 0;
      }
    }

    return true;
  }

  return false;
}

XrActionMap *WM_xr_actionmap_find(wmXrRuntimeData *runtime, const char *name)
{
  LISTBASE_FOREACH (XrActionMap *, am, &runtime->actionmaps) {
    if (STREQLEN(name, am->name, MAX_NAME)) {
      return am;
    }
  }
  return nullptr;
}

void WM_xr_actionmap_clear(XrActionMap *actionmap)
{
  LISTBASE_FOREACH (XrActionMapItem *, ami, &actionmap->items) {
    wm_xr_actionmap_item_clear(ami);
  }
  BLI_freelistN(&actionmap->items);
  actionmap->selitem = 0;
}

void WM_xr_actionmaps_clear(wmXrRuntimeData *runtime)
{
  LISTBASE_FOREACH (XrActionMap *, am, &runtime->actionmaps) {
    WM_xr_actionmap_clear(am);
  }
  BLI_freelistN(&runtime->actionmaps);
  runtime->actactionmap = runtime->selactionmap = 0;
}

ListBase *WM_xr_actionmaps_get(wmXrRuntimeData *runtime)
{
  return &runtime->actionmaps;
}

short WM_xr_actionmap_active_index_get(const wmXrRuntimeData *runtime)
{
  return runtime->actactionmap;
}

void WM_xr_actionmap_active_index_set(wmXrRuntimeData *runtime, short idx)
{
  runtime->actactionmap = idx;
}

short WM_xr_actionmap_selected_index_get(const wmXrRuntimeData *runtime)
{
  return runtime->selactionmap;
}

void WM_xr_actionmap_selected_index_set(wmXrRuntimeData *runtime, short idx)
{
  runtime->selactionmap = idx;
}

/** \} */
