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
 */

/** \file
 * \ingroup wm
 *
 * \name Window-Manager XR Action Maps
 *
 * XR actionmap API, similar to WM keymap API.
 */

#include <math.h>
#include <string.h>

#include "BKE_context.h"
#include "BKE_idprop.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "GHOST_Types.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm_xr_intern.h"

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

  XrActionMapBinding *amb = MEM_callocN(sizeof(XrActionMapBinding), __func__);
  BLI_strncpy(amb->name, name, MAX_NAME);
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
  return NULL;
}

/**
 * Ensure unique name among all action map bindings.
 */
void WM_xr_actionmap_binding_ensure_unique(XrActionMapItem *ami, XrActionMapBinding *amb)
{
  char name[MAX_NAME];
  char *suffix;
  size_t baselen;
  size_t idx = 0;

  BLI_strncpy(name, amb->name, MAX_NAME);
  baselen = BLI_strnlen(name, MAX_NAME);
  suffix = &name[baselen];

  while (wm_xr_actionmap_binding_find_except(ami, name, amb)) {
    if ((baselen + 1) + (log10(++idx) + 1) > MAX_NAME) {
      /* Use default base name. */
      BLI_strncpy(name, WM_XR_ACTIONMAP_BINDING_STR_DEFAULT, MAX_NAME);
      baselen = BLI_strnlen(name, MAX_NAME);
      suffix = &name[baselen];
      idx = 0;
    }
    else {
      BLI_snprintf(suffix, MAX_NAME, "%zu", idx);
    }
  }

  BLI_strncpy(amb->name, name, MAX_NAME);
}

static XrActionMapBinding *wm_xr_actionmap_binding_copy(XrActionMapBinding *amb_src)
{
  XrActionMapBinding *amb_dst = MEM_dupallocN(amb_src);

  amb_dst->prev = amb_dst->next = NULL;

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

bool WM_xr_actionmap_binding_remove(XrActionMapItem *ami, XrActionMapBinding *amb)
{
  int idx = BLI_findindex(&ami->bindings, amb);

  if (idx != -1) {
    BLI_freelinkN(&ami->bindings, amb);

    if (BLI_listbase_is_empty(&ami->bindings)) {
      ami->selbinding = -1;
    }
    else {
      if (idx <= ami->selbinding) {
        if (--ami->selbinding < 0) {
          ami->selbinding = 0;
        }
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
  return NULL;
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
  WM_operator_properties_sanitize(ami->op_properties_ptr, 1);
}

static void wm_xr_actionmap_item_properties_free(XrActionMapItem *ami)
{
  if (ami->op_properties_ptr) {
    WM_operator_properties_free(ami->op_properties_ptr);
    MEM_freeN(ami->op_properties_ptr);
    ami->op_properties_ptr = NULL;
    ami->op_properties = NULL;
  }
  else {
    BLI_assert(ami->op_properties == NULL);
  }
}

/**
 * Similar to #wm_xr_actionmap_item_properties_set()
 * but checks for the #eXrActionType and #wmOperatorType having changed.
 */
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

  if (ami->op_properties_ptr == NULL) {
    wm_xr_actionmap_item_properties_set(ami);
  }
  else {
    wmOperatorType *ot = WM_operatortype_find(ami->op, 0);
    if (ot) {
      if (ot->srna != ami->op_properties_ptr->type) {
        /* Matches wm_xr_actionmap_item_properties_set() but doesn't alloc new ptr. */
        WM_operator_properties_create_ptr(ami->op_properties_ptr, ot);
        if (ami->op_properties) {
          ami->op_properties_ptr->data = ami->op_properties;
        }
        WM_operator_properties_sanitize(ami->op_properties_ptr, 1);
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

  XrActionMapItem *ami = MEM_callocN(sizeof(XrActionMapItem), __func__);
  BLI_strncpy(ami->name, name, MAX_NAME);
  if (ami_prev) {
    WM_xr_actionmap_item_ensure_unique(actionmap, ami);
  }
  ami->selbinding = -1;

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
  return NULL;
}

/**
 * Ensure unique name among all action map items.
 */
void WM_xr_actionmap_item_ensure_unique(XrActionMap *actionmap, XrActionMapItem *ami)
{
  char name[MAX_NAME];
  char *suffix;
  size_t baselen;
  size_t idx = 0;

  BLI_strncpy(name, ami->name, MAX_NAME);
  baselen = BLI_strnlen(name, MAX_NAME);
  suffix = &name[baselen];

  while (wm_xr_actionmap_item_find_except(actionmap, name, ami)) {
    if ((baselen + 1) + (log10(++idx) + 1) > MAX_NAME) {
      /* Use default base name. */
      BLI_strncpy(name, WM_XR_ACTIONMAP_ITEM_STR_DEFAULT, MAX_NAME);
      baselen = BLI_strnlen(name, MAX_NAME);
      suffix = &name[baselen];
      idx = 0;
    }
    else {
      BLI_snprintf(suffix, MAX_NAME, "%zu", idx);
    }
  }

  BLI_strncpy(ami->name, name, MAX_NAME);
}

static XrActionMapItem *wm_xr_actionmap_item_copy(XrActionMapItem *ami)
{
  XrActionMapItem *amin = MEM_dupallocN(ami);

  amin->prev = amin->next = NULL;

  if (amin->op_properties) {
    amin->op_properties_ptr = MEM_callocN(sizeof(PointerRNA), "wmOpItemPtr");
    WM_operator_properties_create(amin->op_properties_ptr, amin->op);

    amin->op_properties = IDP_CopyProperty(amin->op_properties);
    amin->op_properties_ptr->data = amin->op_properties;
  }
  else {
    amin->op_properties = NULL;
    amin->op_properties_ptr = NULL;
  }

  return amin;
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
    if (ami->op_properties_ptr) {
      WM_operator_properties_free(ami->op_properties_ptr);
      MEM_freeN(ami->op_properties_ptr);
    }
    BLI_freelinkN(&actionmap->items, ami);

    if (BLI_listbase_is_empty(&actionmap->items)) {
      actionmap->selitem = -1;
    }
    else {
      if (idx <= actionmap->selitem) {
        if (--actionmap->selitem < 0) {
          actionmap->selitem = 0;
        }
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
  return NULL;
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

  XrActionMap *am = MEM_callocN(sizeof(struct XrActionMap), __func__);
  BLI_strncpy(am->name, name, MAX_NAME);
  if (am_prev) {
    WM_xr_actionmap_ensure_unique(runtime, am);
  }
  am->selitem = -1;

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

  return NULL;
}

/**
 * Ensure unique name among all action maps.
 */
void WM_xr_actionmap_ensure_unique(wmXrRuntimeData *runtime, XrActionMap *actionmap)
{
  char name[MAX_NAME];
  char *suffix;
  size_t baselen;
  size_t idx = 0;

  BLI_strncpy(name, actionmap->name, MAX_NAME);
  baselen = BLI_strnlen(name, MAX_NAME);
  suffix = &name[baselen];

  while (wm_xr_actionmap_find_except(runtime, name, actionmap)) {
    if ((baselen + 1) + (log10(++idx) + 1) > MAX_NAME) {
      /* Use default base name. */
      BLI_strncpy(name, WM_XR_ACTIONMAP_STR_DEFAULT, MAX_NAME);
      baselen = BLI_strnlen(name, MAX_NAME);
      suffix = &name[baselen];
      idx = 0;
    }
    else {
      BLI_snprintf(suffix, MAX_NAME, "%zu", idx);
    }
  }

  BLI_strncpy(actionmap->name, name, MAX_NAME);
}

static XrActionMap *wm_xr_actionmap_copy(XrActionMap *am_src)
{
  XrActionMap *am_dst = MEM_dupallocN(am_src);

  am_dst->prev = am_dst->next = NULL;
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

    if (BLI_listbase_is_empty(&runtime->actionmaps)) {
      runtime->actactionmap = runtime->selactionmap = -1;
    }
    else {
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
  return NULL;
}

void WM_xr_actionmap_clear(XrActionMap *actionmap)
{
  LISTBASE_FOREACH (XrActionMapItem *, ami, &actionmap->items) {
    wm_xr_actionmap_item_properties_free(ami);
  }

  BLI_freelistN(&actionmap->items);

  actionmap->selitem = -1;
}

void WM_xr_actionmaps_clear(wmXrRuntimeData *runtime)
{
  LISTBASE_FOREACH (XrActionMap *, am, &runtime->actionmaps) {
    WM_xr_actionmap_clear(am);
  }

  BLI_freelistN(&runtime->actionmaps);

  runtime->actactionmap = runtime->selactionmap = -1;
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
  BLI_assert(idx < BLI_listbase_count(&runtime->actionmaps));
  runtime->actactionmap = idx;
}

short WM_xr_actionmap_selected_index_get(const wmXrRuntimeData *runtime)
{
  return runtime->selactionmap;
}

void WM_xr_actionmap_selected_index_set(wmXrRuntimeData *runtime, short idx)
{
  BLI_assert(idx < BLI_listbase_count(&runtime->actionmaps));
  runtime->selactionmap = idx;
}

/** \} */
