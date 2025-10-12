/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Configurable key-maps - add/remove/find/compare/patch...
 */

#include <cstring>
#include <fmt/format.h>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "CLG_log.h"
#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLF_api.hh"

#include "UI_interface.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_types.hh"
#include "wm_event_system.hh"
#include "wm_event_types.hh"

#include "ED_screen.hh"

struct wmKeyMapItemFind_Params {
  bool (*filter_fn)(const wmKeyMap *km, const wmKeyMapItem *kmi, void *user_data);
  void *user_data;
};

/* -------------------------------------------------------------------- */
/** \name Keymap Item
 *
 * Item in a keymap, that maps from an event to an operator or modal map item.
 * \{ */

static wmKeyMapItem *wm_keymap_item_copy(const wmKeyMapItem *kmi)
{
  wmKeyMapItem *kmin = static_cast<wmKeyMapItem *>(MEM_dupallocN(kmi));

  kmin->prev = kmin->next = nullptr;
  kmin->flag &= ~KMI_UPDATE;

  if (kmin->properties) {
    kmin->ptr = MEM_new<PointerRNA>("UserKeyMapItemPtr");
    WM_operator_properties_create(kmin->ptr, kmin->idname);

    /* Signal for no context, see #STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID. */
    kmin->ptr->owner_id = nullptr;

    kmin->properties = IDP_CopyProperty(kmin->properties);
    kmin->ptr->data = kmin->properties;
  }
  else {
    kmin->properties = nullptr;
    kmin->ptr = nullptr;
  }

  return kmin;
}

static void wm_keymap_item_free_data(wmKeyMapItem *kmi)
{
  /* Not the `kmi` itself. */
  if (kmi->ptr) {
    WM_operator_properties_free(kmi->ptr);
    MEM_delete(kmi->ptr);
    kmi->ptr = nullptr;
    kmi->properties = nullptr;
  }
  else if (kmi->properties) {
    IDP_FreeProperty(kmi->properties);
    kmi->properties = nullptr;
  }
}

static void wm_keymap_item_clear_runtime(wmKeyMapItem *kmi)
{
  IDProperty *properties = kmi->properties;
  kmi->properties = nullptr;
  if (kmi->ptr) {
    kmi->ptr->data = nullptr;
  }
  wm_keymap_item_free_data(kmi);
  kmi->properties = properties;
}

static void wm_keymap_item_properties_set(wmKeyMapItem *kmi)
{
  WM_operator_properties_alloc(&(kmi->ptr), &(kmi->properties), kmi->idname);
  WM_operator_properties_sanitize(kmi->ptr, true);

  /* Signal for no context, see #STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID. */
  kmi->ptr->owner_id = nullptr;
}

/**
 * Similar to #wm_keymap_item_properties_set
 * but checks for the #wmOperatorType having changed, see #38042.
 */
static void wm_keymap_item_properties_update_ot(wmKeyMapItem *kmi, const bool keep_properties)
{
  if (kmi->idname[0] == 0) {
    BLI_assert(kmi->ptr == nullptr);
    return;
  }

  if (kmi->ptr == nullptr) {
    wm_keymap_item_properties_set(kmi);
  }
  else {
    wmOperatorType *ot = WM_operatortype_find(kmi->idname, false);
    if (ot) {
      if (ot->srna != kmi->ptr->type) {
        /* Matches #wm_keymap_item_properties_set but doesn't alloc new ptr. */
        WM_operator_properties_create_ptr(kmi->ptr, ot);
        /* 'kmi->ptr->data' nullptr'd above, keep using existing properties.
         * NOTE: the operators property types may have changed,
         * we will need a more comprehensive sanitize function to support this properly.
         */
        if (kmi->properties) {
          kmi->ptr->data = kmi->properties;
        }
        WM_operator_properties_sanitize(kmi->ptr, true);

        /* Signal for no context, see #STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID. */
        kmi->ptr->owner_id = nullptr;
      }
    }
    else {
      /* Zombie key-map item. */
      if (keep_properties) {
        wm_keymap_item_clear_runtime(kmi);
      }
      else {
        wm_keymap_item_free_data(kmi);
      }
    }
  }
}

static void wm_keymap_item_properties_update_ot_from_list(ListBase *km_lb,
                                                          const bool keep_properties)
{
  LISTBASE_FOREACH (wmKeyMap *, km, km_lb) {
    LISTBASE_FOREACH (wmKeyMapItem *, kmi, &km->items) {
      wm_keymap_item_properties_update_ot(kmi, keep_properties);
    }

    LISTBASE_FOREACH (wmKeyMapDiffItem *, kmdi, &km->diff_items) {
      if (kmdi->add_item) {
        wm_keymap_item_properties_update_ot(kmdi->add_item, keep_properties);
      }
      if (kmdi->remove_item) {
        wm_keymap_item_properties_update_ot(kmdi->remove_item, keep_properties);
      }
    }
  }
}

static bool wm_keymap_item_equals_result(const wmKeyMapItem *a, const wmKeyMapItem *b)
{
  return (STREQ(a->idname, b->idname) &&
          /* We do not really care about which Main we pass here, TBH. */
          RNA_struct_equals(G_MAIN, a->ptr, b->ptr, RNA_EQ_UNSET_MATCH_NONE) &&
          (a->flag & KMI_INACTIVE) == (b->flag & KMI_INACTIVE) && a->propvalue == b->propvalue);
}

static bool wm_keymap_item_equals(const wmKeyMapItem *a, const wmKeyMapItem *b)
{
  return (wm_keymap_item_equals_result(a, b) && a->type == b->type && a->val == b->val &&
          a->shift == b->shift && a->ctrl == b->ctrl && a->alt == b->alt && a->oskey == b->oskey &&
          a->hyper == b->hyper && a->keymodifier == b->keymodifier && a->maptype == b->maptype &&
          ((a->val != KM_PRESS_DRAG) || (a->direction == b->direction)) &&
          ((ISKEYBOARD(a->type) == 0) ||
           (a->flag & KMI_REPEAT_IGNORE) == (b->flag & KMI_REPEAT_IGNORE)));
}

void WM_keymap_item_properties_reset(wmKeyMapItem *kmi, IDProperty *properties)
{
  if (LIKELY(kmi->ptr)) {
    WM_operator_properties_free(kmi->ptr);
    MEM_delete(kmi->ptr);

    kmi->ptr = nullptr;
  }
  else if (kmi->properties) {
    IDP_FreeProperty(kmi->properties);
  }

  kmi->properties = properties;

  wm_keymap_item_properties_set(kmi);
}

int WM_keymap_item_map_type_get(const wmKeyMapItem *kmi)
{
  if (ISTIMER(kmi->type)) {
    return KMI_TYPE_TIMER;
  }
  if (ISKEYBOARD(kmi->type)) {
    return KMI_TYPE_KEYBOARD;
  }
  if (ISMOUSE(kmi->type)) {
    return KMI_TYPE_MOUSE;
  }
  if (ISNDOF(kmi->type)) {
    return KMI_TYPE_NDOF;
  }
  if (kmi->type == KM_TEXTINPUT) {
    return KMI_TYPE_TEXTINPUT;
  }
  if (ELEM(kmi->type, TABLET_STYLUS, TABLET_ERASER)) {
    return KMI_TYPE_MOUSE;
  }
  return KMI_TYPE_KEYBOARD;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keymap Diff Item
 *
 * Item in a diff keymap, used for saving diff of keymaps in user preferences.
 * \{ */

static wmKeyMapDiffItem *wm_keymap_diff_item_copy(wmKeyMapDiffItem *kmdi)
{
  wmKeyMapDiffItem *kmdin = static_cast<wmKeyMapDiffItem *>(MEM_dupallocN(kmdi));

  kmdin->next = kmdin->prev = nullptr;
  if (kmdi->add_item) {
    kmdin->add_item = wm_keymap_item_copy(kmdi->add_item);
  }
  if (kmdi->remove_item) {
    kmdin->remove_item = wm_keymap_item_copy(kmdi->remove_item);
  }

  return kmdin;
}

static void wm_keymap_diff_item_free(wmKeyMapDiffItem *kmdi)
{
  if (kmdi->remove_item) {
    wm_keymap_item_free_data(kmdi->remove_item);
    MEM_freeN(kmdi->remove_item);
  }
  if (kmdi->add_item) {
    wm_keymap_item_free_data(kmdi->add_item);
    MEM_freeN(kmdi->add_item);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Key Configuration
 *
 * List of keymaps for all editors, modes, etc.
 * There is a builtin default key configuration,
 * a user key configuration, and other preset configurations.
 * \{ */

wmKeyConfig *WM_keyconfig_new(wmWindowManager *wm, const char *idname, bool user_defined)
{
  BLI_assert(!BLI_findstring(&wm->runtime->keyconfigs, idname, offsetof(wmKeyConfig, idname)));
  /* Create new configuration. */
  wmKeyConfig *keyconf = MEM_callocN<wmKeyConfig>("wmKeyConfig");
  STRNCPY_UTF8(keyconf->idname, idname);
  BLI_addtail(&wm->runtime->keyconfigs, keyconf);

  if (user_defined) {
    keyconf->flag |= KEYCONF_USER;
  }

  return keyconf;
}

wmKeyConfig *WM_keyconfig_ensure(wmWindowManager *wm, const char *idname, bool user_defined)
{
  wmKeyConfig *keyconf = static_cast<wmKeyConfig *>(
      BLI_findstring(&wm->runtime->keyconfigs, idname, offsetof(wmKeyConfig, idname)));
  if (keyconf) {
    if (keyconf == wm->runtime->defaultconf) {
      /* For default configuration, we need to keep keymap
       * modal items and poll functions intact. */
      LISTBASE_FOREACH (wmKeyMap *, km, &keyconf->keymaps) {
        WM_keymap_clear(km);
      }
    }
    else {
      /* For user defined key configuration, clear all keymaps. */
      WM_keyconfig_clear(keyconf);
    }

    return keyconf;
  }

  return WM_keyconfig_new(wm, idname, user_defined);
}

void WM_keyconfig_remove(wmWindowManager *wm, wmKeyConfig *keyconf)
{
  BLI_assert(BLI_findindex(&wm->runtime->keyconfigs, keyconf) != -1);
  if (STREQLEN(U.keyconfigstr, keyconf->idname, sizeof(U.keyconfigstr))) {
    STRNCPY(U.keyconfigstr, wm->runtime->defaultconf->idname);
    U.runtime.is_dirty = true;
    WM_keyconfig_update_tag(nullptr, nullptr);
  }

  BLI_remlink(&wm->runtime->keyconfigs, keyconf);
  WM_keyconfig_free(keyconf);

  /* Clear pointers. */
  wmKeyConfig **keyconf_arr_p[] = {WM_KEYCONFIG_ARRAY_P(wm)};
  for (int i = 0; i < ARRAY_SIZE(keyconf_arr_p); i++) {
    wmKeyConfig **kc_p = keyconf_arr_p[i];
    if (*kc_p == keyconf) {
      *kc_p = nullptr;
    }
  }
}

void WM_keyconfig_clear(wmKeyConfig *keyconf)
{
  LISTBASE_FOREACH_MUTABLE (wmKeyMap *, km, &keyconf->keymaps) {
    WM_keymap_clear(km);
    MEM_freeN(km);
  }

  BLI_listbase_clear(&keyconf->keymaps);
}

void WM_keyconfig_free(wmKeyConfig *keyconf)
{
  WM_keyconfig_clear(keyconf);
  MEM_freeN(keyconf);
}

static wmKeyConfig *WM_keyconfig_active(wmWindowManager *wm)
{
  wmKeyConfig *keyconf;

  /* First try from preset. */
  keyconf = static_cast<wmKeyConfig *>(
      BLI_findstring(&wm->runtime->keyconfigs, U.keyconfigstr, offsetof(wmKeyConfig, idname)));
  if (keyconf) {
    return keyconf;
  }

  /* Otherwise use default. */
  return wm->runtime->defaultconf;
}

void WM_keyconfig_set_active(wmWindowManager *wm, const char *idname)
{
  /* Setting a different key configuration as active:
   * we ensure all is updated properly before and after making the change. */

  WM_keyconfig_update(wm);

  STRNCPY(U.keyconfigstr, idname);
  if (wm->init_flag & WM_INIT_FLAG_KEYCONFIG) {
    U.runtime.is_dirty = true;
  }

  WM_keyconfig_update_tag(nullptr, nullptr);
  WM_keyconfig_update(wm);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keymap
 *
 * List of keymap items for one editor, mode, modal operator.
 * \{ */

static wmKeyMap *wm_keymap_new(const char *idname, int spaceid, int regionid)
{
  wmKeyMap *km = MEM_callocN<wmKeyMap>("keymap list");

  STRNCPY_UTF8(km->idname, idname);
  km->spaceid = spaceid;
  km->regionid = regionid;

  {
    const char *owner_id = RNA_struct_state_owner_get();
    if (owner_id) {
      STRNCPY(km->owner_id, owner_id);
    }
  }
  return km;
}

static wmKeyMap *wm_keymap_copy(wmKeyMap *keymap)
{
  wmKeyMap *keymapn = static_cast<wmKeyMap *>(MEM_dupallocN(keymap));

  keymapn->modal_items = keymap->modal_items;
  keymapn->poll = keymap->poll;
  keymapn->poll_modal_item = keymap->poll_modal_item;
  BLI_listbase_clear(&keymapn->items);
  keymapn->flag &= ~(KEYMAP_UPDATE | KEYMAP_EXPANDED);

  LISTBASE_FOREACH (wmKeyMapDiffItem *, kmdi, &keymap->diff_items) {
    wmKeyMapDiffItem *kmdi_new = wm_keymap_diff_item_copy(kmdi);
    BLI_addtail(&keymapn->items, kmdi_new);
  }

  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
    wmKeyMapItem *kmi_new = wm_keymap_item_copy(kmi);
    BLI_addtail(&keymapn->items, kmi_new);
  }

  return keymapn;
}

void WM_keymap_clear(wmKeyMap *keymap)
{
  LISTBASE_FOREACH_MUTABLE (wmKeyMapDiffItem *, kmdi, &keymap->diff_items) {
    wm_keymap_diff_item_free(kmdi);
    MEM_freeN(kmdi);
  }

  LISTBASE_FOREACH_MUTABLE (wmKeyMapItem *, kmi, &keymap->items) {
    wm_keymap_item_free_data(kmi);
    MEM_freeN(kmi);
  }

  BLI_listbase_clear(&keymap->diff_items);
  BLI_listbase_clear(&keymap->items);
}

void WM_keymap_remove(wmKeyConfig *keyconf, wmKeyMap *keymap)
{
  BLI_assert(BLI_findindex(&keyconf->keymaps, keymap) != -1);
  WM_keymap_clear(keymap);
  BLI_remlink(&keyconf->keymaps, keymap);
  MEM_freeN(keymap);
}

bool WM_keymap_poll(bContext *C, wmKeyMap *keymap)
{
  /* If we're tagged, only use compatible. */
  if (keymap->owner_id[0] != '\0') {
    const WorkSpace *workspace = CTX_wm_workspace(C);
    if (BKE_workspace_owner_id_check(workspace, keymap->owner_id) == false) {
      return false;
    }
  }

  if (UNLIKELY(BLI_listbase_is_empty(&keymap->items))) {
    /* Empty key-maps may be missing more there may be a typo in the name.
     * Warn early to avoid losing time investigating each case.
     * When developing a customized Blender though you may want empty keymaps. */
    if (!U.app_template[0] &&
        /* Fallback key-maps may be intentionally empty, don't flood the output. */
        !BLI_str_endswith(keymap->idname, " (fallback)") &&
        /* This is an exception which may be empty.
         * Longer term we might want a flag to indicate an empty key-map is intended. */
        !STREQ(keymap->idname, "Node Tool: Tweak") &&
        /* Another exception: Asset shelf keymap is meant for add-ons to use, it's empty by
         * default. */
        !STREQ(keymap->idname, "Asset Shelf") &&
        /* Same for the asset browser keymap. */
        !STREQ(keymap->idname, "Asset Browser Main"))
    {
      CLOG_WARN(WM_LOG_EVENTS, "empty keymap '%s'", keymap->idname);
    }
  }

  if (keymap->poll != nullptr) {
    return keymap->poll(C);
  }
  return true;
}

static bool wm_keymap_is_match(const wmKeyMap *km_a, const wmKeyMap *km_b)
{
  return ((km_a->spaceid == km_b->spaceid) && (km_a->regionid == km_b->regionid) &&
          STREQ(km_a->idname, km_b->idname));
}

static void keymap_event_set(wmKeyMapItem *kmi, const KeyMapItem_Params *params)
{
  kmi->type = params->type;
  kmi->val = params->value;
  kmi->keymodifier = params->keymodifier;
  kmi->direction = params->direction;

  if (params->modifier == KM_ANY) {
    kmi->shift = kmi->ctrl = kmi->alt = kmi->oskey = kmi->hyper = KM_ANY;
  }
  else {
    const int8_t mod = params->modifier & 0xff;
    const int8_t mod_any = KMI_PARAMS_MOD_FROM_ANY(params->modifier);

    /* Only one of the flags should be set. */
    BLI_assert((mod & mod_any) == 0);

    kmi->shift = ((mod & KM_SHIFT) ? KM_MOD_HELD : ((mod_any & KM_SHIFT) ? KM_ANY : KM_NOTHING));
    kmi->ctrl = ((mod & KM_CTRL) ? KM_MOD_HELD : ((mod_any & KM_CTRL) ? KM_ANY : KM_NOTHING));
    kmi->alt = ((mod & KM_ALT) ? KM_MOD_HELD : ((mod_any & KM_ALT) ? KM_ANY : KM_NOTHING));
    kmi->oskey = ((mod & KM_OSKEY) ? KM_MOD_HELD : ((mod_any & KM_OSKEY) ? KM_ANY : KM_NOTHING));
    kmi->hyper = ((mod & KM_HYPER) ? KM_MOD_HELD : ((mod_any & KM_HYPER) ? KM_ANY : KM_NOTHING));
  }
}

static void keymap_item_set_id(wmKeyMap *keymap, wmKeyMapItem *kmi)
{
  keymap->kmi_id++;
  if ((keymap->flag & KEYMAP_USER) == 0) {
    kmi->id = keymap->kmi_id;
  }
  else {
    kmi->id = -keymap->kmi_id; /* User defined keymap entries have negative ids. */
  }
}

wmKeyMapItem *WM_keymap_add_item(wmKeyMap *keymap,
                                 const char *idname,
                                 const KeyMapItem_Params *params)
{
  wmKeyMapItem *kmi = MEM_callocN<wmKeyMapItem>("keymap entry");

  BLI_addtail(&keymap->items, kmi);
  STRNCPY(kmi->idname, idname);

  keymap_event_set(kmi, params);
  wm_keymap_item_properties_set(kmi);

  keymap_item_set_id(keymap, kmi);

  WM_keyconfig_update_tag(keymap, kmi);

  return kmi;
}

wmKeyMapItem *WM_keymap_add_item_copy(wmKeyMap *keymap, const wmKeyMapItem *kmi_src)
{
  wmKeyMapItem *kmi_dst = wm_keymap_item_copy(kmi_src);

  BLI_addtail(&keymap->items, kmi_dst);

  keymap_item_set_id(keymap, kmi_dst);

  WM_keyconfig_update_tag(keymap, kmi_dst);

  return kmi_dst;
}

void WM_keymap_remove_item(wmKeyMap *keymap, wmKeyMapItem *kmi)
{
  BLI_assert(BLI_findindex(&keymap->items, kmi) != -1);
  if (kmi->ptr) {
    WM_operator_properties_free(kmi->ptr);
    MEM_delete(kmi->ptr);
  }
  else if (kmi->properties) {
    IDP_FreeProperty(kmi->properties);
  }
  BLI_freelinkN(&keymap->items, kmi);

  WM_keyconfig_update_tag(keymap, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keymap Diff and Patch
 *
 * Rather than saving the entire keymap for user preferences, we only save a
 * diff so that changes in the defaults get synced. This system is not perfect
 * but works better than overriding the keymap entirely when only few items
 * are changed.
 * \{ */

static void wm_keymap_addon_add(wmKeyMap *keymap, wmKeyMap *addonmap)
{
  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &addonmap->items) {
    wmKeyMapItem *kmi_new = wm_keymap_item_copy(kmi);
    keymap_item_set_id(keymap, kmi_new);
    BLI_addhead(&keymap->items, kmi_new);
  }
}

static wmKeyMapItem *wm_keymap_find_item_equals(wmKeyMap *km, const wmKeyMapItem *needle)
{
  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &km->items) {
    if (wm_keymap_item_equals(kmi, needle)) {
      return kmi;
    }
  }

  return nullptr;
}

static wmKeyMapItem *wm_keymap_find_item_equals_result(wmKeyMap *km, const wmKeyMapItem *needle)
{
  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &km->items) {
    if (wm_keymap_item_equals_result(kmi, needle)) {
      return kmi;
    }
  }

  return nullptr;
}

static void wm_keymap_diff(
    wmKeyMap *diff_km, wmKeyMap *from_km, wmKeyMap *to_km, wmKeyMap *orig_km, wmKeyMap *addon_km)
{
  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &from_km->items) {
    wmKeyMapItem *to_kmi = WM_keymap_item_find_id(to_km, kmi->id);

    if (!to_kmi) {
      /* Remove item. */
      wmKeyMapDiffItem *kmdi = MEM_callocN<wmKeyMapDiffItem>("wmKeyMapDiffItem");
      kmdi->remove_item = wm_keymap_item_copy(kmi);
      BLI_addtail(&diff_km->diff_items, kmdi);
    }
    else if (to_kmi && !wm_keymap_item_equals(kmi, to_kmi)) {
      /* Replace item. */
      wmKeyMapDiffItem *kmdi = MEM_callocN<wmKeyMapDiffItem>("wmKeyMapDiffItem");
      kmdi->remove_item = wm_keymap_item_copy(kmi);
      kmdi->add_item = wm_keymap_item_copy(to_kmi);
      BLI_addtail(&diff_km->diff_items, kmdi);
    }

    /* Sync expanded flag back to original so we don't lose it on re-patch. */
    if (to_kmi) {
      wmKeyMapItem *orig_kmi = WM_keymap_item_find_id(orig_km, kmi->id);

      if (!orig_kmi && addon_km) {
        orig_kmi = wm_keymap_find_item_equals(addon_km, kmi);
      }

      if (orig_kmi) {
        orig_kmi->flag &= ~KMI_EXPANDED;
        orig_kmi->flag |= (to_kmi->flag & KMI_EXPANDED);
      }
    }
  }

  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &to_km->items) {
    if (kmi->id < 0) {
      /* Add item. */
      wmKeyMapDiffItem *kmdi = MEM_callocN<wmKeyMapDiffItem>("wmKeyMapDiffItem");
      kmdi->add_item = wm_keymap_item_copy(kmi);
      BLI_addtail(&diff_km->diff_items, kmdi);
    }
  }
}

static void wm_keymap_patch(wmKeyMap *km, wmKeyMap *diff_km)
{
  LISTBASE_FOREACH (wmKeyMapDiffItem *, kmdi, &diff_km->diff_items) {
    /* Find item to remove. */
    wmKeyMapItem *kmi_remove = nullptr;
    if (kmdi->remove_item) {
      kmi_remove = wm_keymap_find_item_equals(km, kmdi->remove_item);
      if (!kmi_remove) {
        kmi_remove = wm_keymap_find_item_equals_result(km, kmdi->remove_item);
      }
    }

    /* Add item. */
    if (kmdi->add_item) {
      /* Do not re-add an already existing keymap item! See #42088. */
      /* We seek only for exact copy here! See #42137. */
      wmKeyMapItem *kmi_add = wm_keymap_find_item_equals(km, kmdi->add_item);

      /* If kmi_add is same as kmi_remove (can happen in some cases,
       * typically when we got kmi_remove from #wm_keymap_find_item_equals_result()),
       * no need to add or remove anything, see #45579. */

      /**
       * \note This typically happens when we apply user-defined keymap diff to a base one that
       * was exported with that customized keymap already. In that case:
       *
       * - wm_keymap_find_item_equals(km, kmdi->remove_item) finds nothing
       *   (because actual shortcut of current base does not match kmdi->remove_item any more).
       * - wm_keymap_find_item_equals_result(km, kmdi->remove_item) finds the current kmi from
       *   base keymap (because it does exactly the same thing).
       * - wm_keymap_find_item_equals(km, kmdi->add_item) finds the same kmi,
       *   since base keymap was exported with that user-defined shortcut already!
       *
       *       Maybe we should rather keep user-defined keymaps specific to a given base one? */
      if (kmi_add != nullptr && kmi_add == kmi_remove) {
        kmi_remove = nullptr;
      }
      /* Only if nothing to remove or item to remove found. */
      else if (!kmi_add && (!kmdi->remove_item || kmi_remove)) {
        kmi_add = wm_keymap_item_copy(kmdi->add_item);
        kmi_add->flag |= KMI_USER_MODIFIED;

        if (kmi_remove) {
          kmi_add->flag &= ~KMI_EXPANDED;
          kmi_add->flag |= (kmi_remove->flag & KMI_EXPANDED);
          kmi_add->id = kmi_remove->id;
          BLI_insertlinkbefore(&km->items, kmi_remove, kmi_add);
        }
        else {
          keymap_item_set_id(km, kmi_add);
          BLI_addtail(&km->items, kmi_add);
        }
      }
    }

    /* Remove item. */
    if (kmi_remove) {
      wm_keymap_item_free_data(kmi_remove);
      BLI_freelinkN(&km->items, kmi_remove);
    }
  }
}

static wmKeyMap *wm_keymap_patch_update(ListBase *lb,
                                        wmKeyMap *defaultmap,
                                        wmKeyMap *addonmap,
                                        wmKeyMap *usermap)
{
  int expanded = 0;

  /* Remove previous keymap in list, we will replace it. */
  wmKeyMap *km = WM_keymap_list_find(
      lb, defaultmap->idname, defaultmap->spaceid, defaultmap->regionid);
  if (km) {
    expanded = (km->flag & (KEYMAP_EXPANDED | KEYMAP_CHILDREN_EXPANDED));
    WM_keymap_clear(km);
    BLI_freelinkN(lb, km);
  }

  /* Copy new keymap from an existing one. */
  if (usermap && !(usermap->flag & KEYMAP_DIFF)) {
    /* For compatibility with old user preferences with non-diff
     * keymaps we override the original entirely. */

    km = wm_keymap_copy(usermap);

    /* Try to find corresponding id's for items. */
    LISTBASE_FOREACH (wmKeyMapItem *, kmi, &km->items) {
      wmKeyMapItem *orig_kmi = wm_keymap_find_item_equals(defaultmap, kmi);
      if (!orig_kmi) {
        orig_kmi = wm_keymap_find_item_equals_result(defaultmap, kmi);
      }

      if (orig_kmi) {
        kmi->id = orig_kmi->id;
      }
      else {
        kmi->id = -(km->kmi_id++);
      }
    }

    km->flag |= KEYMAP_UPDATE; /* Update again to create diff. */
  }
  else {
    km = wm_keymap_copy(defaultmap);
  }

  /* Add addon keymap items. */
  if (addonmap) {
    wm_keymap_addon_add(km, addonmap);
  }

  /* Tag as being user edited. */
  if (usermap) {
    km->flag |= KEYMAP_USER_MODIFIED;
  }
  km->flag |= KEYMAP_USER | expanded;

  /* Apply user changes of diff keymap. */
  if (usermap && (usermap->flag & KEYMAP_DIFF)) {
    wm_keymap_patch(km, usermap);
  }

  /* Add to list. */
  BLI_addtail(lb, km);

  return km;
}

static void wm_keymap_diff_update(ListBase *lb,
                                  wmKeyMap *defaultmap,
                                  wmKeyMap *addonmap,
                                  wmKeyMap *km)
{
  /* Create temporary default + addon keymap for diff. */
  wmKeyMap *origmap = defaultmap;

  if (addonmap) {
    defaultmap = wm_keymap_copy(defaultmap);
    wm_keymap_addon_add(defaultmap, addonmap);
  }

  /* Remove previous diff keymap in list, we will replace it. */
  wmKeyMap *prevmap = WM_keymap_list_find(lb, km->idname, km->spaceid, km->regionid);
  if (prevmap) {
    WM_keymap_clear(prevmap);
    BLI_freelinkN(lb, prevmap);
  }

  /* Create diff keymap. */
  wmKeyMap *diffmap = wm_keymap_new(km->idname, km->spaceid, km->regionid);
  diffmap->flag |= KEYMAP_DIFF;
  if (defaultmap->flag & KEYMAP_MODAL) {
    diffmap->flag |= KEYMAP_MODAL;
  }
  wm_keymap_diff(diffmap, defaultmap, km, origmap, addonmap);

  /* Add to list if not empty. */
  if (diffmap->diff_items.first) {
    BLI_addtail(lb, diffmap);
  }
  else {
    WM_keymap_clear(diffmap);
    MEM_freeN(diffmap);
  }

  /* Free temporary default map. */
  if (addonmap) {
    WM_keymap_clear(defaultmap);
    MEM_freeN(defaultmap);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Storage in WM
 *
 * Name id's are for storing general or multiple keymaps.
 *
 * - Space/region ids are same as `DNA_space_types.h`.
 * - Gets freed in `wm.cc`.
 * \{ */

wmKeyMap *WM_keymap_list_find(ListBase *lb, const char *idname, int spaceid, int regionid)
{
  LISTBASE_FOREACH (wmKeyMap *, km, lb) {
    if (km->spaceid == spaceid && km->regionid == regionid) {
      if (STREQLEN(idname, km->idname, KMAP_MAX_NAME)) {
        return km;
      }
    }
  }

  return nullptr;
}

wmKeyMap *WM_keymap_list_find_spaceid_or_empty(ListBase *lb,
                                               const char *idname,
                                               int spaceid,
                                               int regionid)
{
  LISTBASE_FOREACH (wmKeyMap *, km, lb) {
    if (ELEM(km->spaceid, spaceid, SPACE_EMPTY) && km->regionid == regionid) {
      if (STREQLEN(idname, km->idname, KMAP_MAX_NAME)) {
        return km;
      }
    }
  }

  return nullptr;
}

wmKeyMap *WM_keymap_ensure(wmKeyConfig *keyconf, const char *idname, int spaceid, int regionid)
{
  wmKeyMap *km = WM_keymap_list_find(&keyconf->keymaps, idname, spaceid, regionid);

  if (km == nullptr) {
    km = wm_keymap_new(idname, spaceid, regionid);
    BLI_addtail(&keyconf->keymaps, km);

    WM_keyconfig_update_tag(km, nullptr);
  }

  return km;
}

wmKeyMap *WM_keymap_find_all(wmWindowManager *wm, const char *idname, int spaceid, int regionid)
{
  return WM_keymap_list_find(&wm->runtime->userconf->keymaps, idname, spaceid, regionid);
}

wmKeyMap *WM_keymap_find_all_spaceid_or_empty(wmWindowManager *wm,
                                              const char *idname,
                                              int spaceid,
                                              int regionid)
{
  return WM_keymap_list_find_spaceid_or_empty(
      &wm->runtime->userconf->keymaps, idname, spaceid, regionid);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modal Keymaps
 *
 * Modal key-maps get linked to a running operator,
 * and filter the keys before sending to #wmOperatorType.modal callback.
 * \{ */

wmKeyMap *WM_modalkeymap_ensure(wmKeyConfig *keyconf,
                                const char *idname,
                                const EnumPropertyItem *items)
{
  wmKeyMap *km = WM_keymap_ensure(keyconf, idname, SPACE_EMPTY, RGN_TYPE_WINDOW);
  km->flag |= KEYMAP_MODAL;

  /* Initialize modal items from default configuration. */
  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
  if (wm->runtime->defaultconf && wm->runtime->defaultconf != keyconf) {
    wmKeyMap *defaultkm = WM_keymap_list_find(
        &wm->runtime->defaultconf->keymaps, km->idname, SPACE_EMPTY, RGN_TYPE_WINDOW);

    if (defaultkm) {
      km->modal_items = defaultkm->modal_items;
      km->poll = defaultkm->poll;
      km->poll_modal_item = defaultkm->poll_modal_item;
    }
  }

  if (items) {
    km->modal_items = items;
  }

  return km;
}

wmKeyMap *WM_modalkeymap_find(wmKeyConfig *keyconf, const char *idname)
{
  LISTBASE_FOREACH (wmKeyMap *, km, &keyconf->keymaps) {
    if (km->flag & KEYMAP_MODAL) {
      if (STREQLEN(idname, km->idname, KMAP_MAX_NAME)) {
        return km;
      }
    }
  }

  return nullptr;
}

wmKeyMapItem *WM_modalkeymap_add_item(wmKeyMap *km, const KeyMapItem_Params *params, int value)
{
  wmKeyMapItem *kmi = MEM_callocN<wmKeyMapItem>("keymap entry");

  BLI_addtail(&km->items, kmi);
  kmi->propvalue = value;

  keymap_event_set(kmi, params);

  keymap_item_set_id(km, kmi);

  WM_keyconfig_update_tag(km, kmi);

  return kmi;
}

wmKeyMapItem *WM_modalkeymap_add_item_str(wmKeyMap *km,
                                          const KeyMapItem_Params *params,
                                          const char *value)
{
  wmKeyMapItem *kmi = MEM_callocN<wmKeyMapItem>("keymap entry");

  BLI_addtail(&km->items, kmi);
  STRNCPY(kmi->propvalue_str, value);

  keymap_event_set(kmi, params);

  keymap_item_set_id(km, kmi);

  WM_keyconfig_update_tag(km, kmi);

  return kmi;
}

static const wmKeyMapItem *wm_modalkeymap_find_propvalue_iter(const wmKeyMap *km,
                                                              const wmKeyMapItem *kmi,
                                                              const int propvalue)
{
  if (km->flag & KEYMAP_MODAL) {
    kmi = static_cast<const wmKeyMapItem *>(kmi ? kmi->next : km->items.first);
    for (; kmi; kmi = kmi->next) {
      if (kmi->propvalue == propvalue) {
        return kmi;
      }
    }
  }
  else {
    BLI_assert_msg(0, "called with non modal keymap");
  }

  return nullptr;
}

const wmKeyMapItem *WM_modalkeymap_find_propvalue(const wmKeyMap *km, const int propvalue)
{
  return wm_modalkeymap_find_propvalue_iter(km, nullptr, propvalue);
}

void WM_modalkeymap_assign(wmKeyMap *km, const char *opname)
{
  wmOperatorType *ot = WM_operatortype_find(opname, false);

  if (ot) {
    ot->modalkeymap = km;
  }
  else {
    CLOG_ERROR(WM_LOG_OPERATORS, "unknown operator '%s' in modal keymap", opname);
  }
}

static void wm_user_modal_keymap_set_items(wmWindowManager *wm, wmKeyMap *km)
{
  /* Here we convert propvalue string values delayed, due to python keymaps
   * being created before the actual modal keymaps, so no modal_items. */

  if (km && (km->flag & KEYMAP_MODAL) && !km->modal_items) {
    if (wm->runtime->defaultconf == nullptr) {
      return;
    }

    wmKeyMap *defaultkm = WM_keymap_list_find(
        &wm->runtime->defaultconf->keymaps, km->idname, SPACE_EMPTY, RGN_TYPE_WINDOW);
    if (!defaultkm) {
      return;
    }

    km->modal_items = defaultkm->modal_items;
    km->poll = defaultkm->poll;
    km->poll_modal_item = defaultkm->poll_modal_item;

    if (km->modal_items) {
      LISTBASE_FOREACH (wmKeyMapItem *, kmi, &km->items) {
        if (kmi->propvalue_str[0]) {
          int propvalue;
          if (RNA_enum_value_from_id(static_cast<const EnumPropertyItem *>(km->modal_items),
                                     kmi->propvalue_str,
                                     &propvalue))
          {
            kmi->propvalue = propvalue;
          }
          kmi->propvalue_str[0] = '\0';
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text from Key Events
 * \{ */

static const char *key_event_glyph_or_text(const int font_id,
                                           const char *text,
                                           const char *single_glyph)
{
  BLI_assert(single_glyph == nullptr || (BLI_strlen_utf8(single_glyph) == 1));
  return (single_glyph && BLF_has_glyph(font_id, BLI_str_utf8_as_unicode_or_error(single_glyph))) ?
             single_glyph :
             text;
}

const char *WM_key_event_string(const short type, const bool compact)
{
  if (compact) {
    /* String storing a single unicode character or nullptr. */
    const char *single_glyph = nullptr;
    int font_id = BLF_default();
    const enum {
      UNIX,
      MACOS,
      MSWIN,
    } platform =

#if defined(__APPLE__)
        MACOS
#elif defined(_WIN32)
        MSWIN
#else
        UNIX
#endif
        ;

    switch (type) {
      case EVT_LEFTSHIFTKEY:
      case EVT_RIGHTSHIFTKEY: {
        if (platform == MACOS) {
          single_glyph = BLI_STR_UTF8_UPWARDS_WHITE_ARROW;
        }
        return key_event_glyph_or_text(
            font_id, CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "Shift"), single_glyph);
      }
      case EVT_LEFTCTRLKEY:
      case EVT_RIGHTCTRLKEY:
        if (platform == MACOS) {
          return key_event_glyph_or_text(font_id, "^", BLI_STR_UTF8_UP_ARROWHEAD);
        }
        return IFACE_("Ctrl");
      case EVT_LEFTALTKEY:
      case EVT_RIGHTALTKEY: {
        if (platform == MACOS) {
          /* Option symbol on Mac keyboard. */
          single_glyph = BLI_STR_UTF8_OPTION_KEY;
        }
        return key_event_glyph_or_text(font_id, IFACE_("Alt"), single_glyph);
      }
      case EVT_OSKEY: {
        /* Keep these labels in sync with: `scripts/modules/rna_keymap_ui.py`. */
        if (platform == MACOS) {
          return key_event_glyph_or_text(
              font_id, IFACE_("Cmd"), BLI_STR_UTF8_PLACE_OF_INTEREST_SIGN);
        }
        if (platform == MSWIN) {
          return key_event_glyph_or_text(
              font_id, IFACE_("Win"), BLI_STR_UTF8_BLACK_DIAMOND_MINUS_WHITE_X);
        }
        return IFACE_("OS");
      }
      case EVT_TABKEY:
        return key_event_glyph_or_text(
            font_id, CTX_N_(BLT_I18NCONTEXT_UI_EVENTS, "Tab"), BLI_STR_UTF8_HORIZONTAL_TAB_KEY);
      case EVT_BACKSPACEKEY:
        return key_event_glyph_or_text(font_id, IFACE_("Bksp"), BLI_STR_UTF8_ERASE_TO_THE_LEFT);
      case EVT_ESCKEY:
        if (platform == MACOS) {
          single_glyph = BLI_STR_UTF8_BROKEN_CIRCLE_WITH_NORTHWEST_ARROW;
        }
        return key_event_glyph_or_text(font_id, IFACE_("Esc"), single_glyph);
      case EVT_RETKEY:
        return key_event_glyph_or_text(font_id, IFACE_("Enter"), BLI_STR_UTF8_RETURN_SYMBOL);
      case EVT_SPACEKEY:
        return key_event_glyph_or_text(
            font_id, CTX_IFACE_(BLT_I18NCONTEXT_UI_EVENTS, "Space"), BLI_STR_UTF8_OPEN_BOX);
      case EVT_LEFTARROWKEY:
        return key_event_glyph_or_text(font_id, IFACE_("Left"), BLI_STR_UTF8_LEFTWARDS_ARROW);
      case EVT_UPARROWKEY:
        return key_event_glyph_or_text(font_id, IFACE_("Up"), BLI_STR_UTF8_UPWARDS_ARROW);
      case EVT_RIGHTARROWKEY:
        return key_event_glyph_or_text(font_id, IFACE_("Right"), BLI_STR_UTF8_RIGHTWARDS_ARROW);
      case EVT_DOWNARROWKEY:
        return key_event_glyph_or_text(font_id, IFACE_("Down"), BLI_STR_UTF8_DOWNWARDS_ARROW);
    }
  }

  const EnumPropertyItem *it;
  const int i = RNA_enum_from_value(rna_enum_event_type_items, int(type));

  if (i == -1) {
    return "";
  }
  it = &rna_enum_event_type_items[i];

  /* We first try enum items' description (abused as short-name here),
   * and fall back to usual name if empty. */
  if (compact && it->description[0]) {
    /* XXX No context for enum descriptions... In practice shall not be an issue though. */
    return IFACE_(it->description);
  }

  return CTX_IFACE_(BLT_I18NCONTEXT_UI_EVENTS, it->name);
}

std::optional<std::string> WM_keymap_item_raw_to_string(const int8_t shift,
                                                        const int8_t ctrl,
                                                        const int8_t alt,
                                                        const int8_t oskey,
                                                        const int8_t hyper,
                                                        const short keymodifier,
                                                        const short val,
                                                        const short type,
                                                        const bool compact)
{
  /* TODO: also support (some) value, like e.g. double-click? */
  blender::Vector<std::string_view, 12> result_array;

  const char *space = " ";

  /* When a modifier is #KM_ANY, it isn't shown as this would end up being overly verbose. */

  if (shift == KM_MOD_HELD) {
    result_array.append(WM_key_event_string(EVT_LEFTSHIFTKEY, true));
    result_array.append(space);
  }
  if (ctrl == KM_MOD_HELD) {
    result_array.append(WM_key_event_string(EVT_LEFTCTRLKEY, true));
    result_array.append(space);
  }
  if (alt == KM_MOD_HELD) {
    result_array.append(WM_key_event_string(EVT_LEFTALTKEY, true));
    result_array.append(space);
  }
  if (oskey == KM_MOD_HELD) {
    result_array.append(WM_key_event_string(EVT_OSKEY, true));
    result_array.append(space);
  }
  if (hyper == KM_MOD_HELD) {
    result_array.append(WM_key_event_string(EVT_HYPER, true));
    result_array.append(space);
  }

  if (keymodifier) {
    result_array.append(WM_key_event_string(keymodifier, compact));
    result_array.append(space);
  }

  if (type) {
    if (val == KM_DBL_CLICK) {
      result_array.append(IFACE_("dbl-"));
    }
    else if (val == KM_PRESS_DRAG) {
      result_array.append(IFACE_("drag-"));
    }
    result_array.append(WM_key_event_string(type, compact));
  }

  if (!result_array.is_empty() && (result_array.last() == space)) {
    result_array.remove_last();
  }

  return fmt::to_string(fmt::join(result_array, ""));
}

std::optional<std::string> WM_keymap_item_to_string(const wmKeyMapItem *kmi, const bool compact)
{
  return WM_keymap_item_raw_to_string(kmi->shift,
                                      kmi->ctrl,
                                      kmi->alt,
                                      kmi->oskey,
                                      kmi->hyper,
                                      kmi->keymodifier,
                                      kmi->val,
                                      kmi->type,
                                      compact);
}

std::optional<std::string> WM_modalkeymap_items_to_string(const wmKeyMap *km,
                                                          const int propvalue,
                                                          const bool compact)
{
  const wmKeyMapItem *kmi;
  if (km == nullptr || (kmi = WM_modalkeymap_find_propvalue(km, propvalue)) == nullptr) {
    return std::nullopt;
  }

  std::string result;
  do {
    result += WM_keymap_item_to_string(kmi, compact).value_or("");

    if ((kmi = wm_modalkeymap_find_propvalue_iter(km, kmi, propvalue)) == nullptr) {
      break;
    }
    result += '/';
  } while (true);

  return result;
}

std::optional<std::string> WM_modalkeymap_operator_items_to_string(wmOperatorType *ot,
                                                                   const int propvalue,
                                                                   const bool compact)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
  wmKeyMap *keymap = WM_keymap_active(wm, ot->modalkeymap);
  return WM_modalkeymap_items_to_string(keymap, propvalue, compact);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keymap Finding Utilities
 * \{ */

static wmKeyMapItem *wm_keymap_item_find_in_keymap(wmKeyMap *keymap,
                                                   const char *opname,
                                                   const IDProperty *properties,
                                                   const bool is_strict,
                                                   const wmKeyMapItemFind_Params *params)
{
  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
    /* Skip disabled keymap items, see: #38447. */
    if (kmi->flag & KMI_INACTIVE) {
      continue;
    }
    if (!STREQ(kmi->idname, opname)) {
      continue;
    }

    bool kmi_match = false;
    if (properties) {
/* Example of debugging keymaps. */
#if 0
      if (kmi->ptr) {
        if (STREQ("MESH_OT_rip_move", opname)) {
          printf("OPERATOR\n");
          IDP_print(properties);
          printf("KEYMAP\n");
          IDP_print(kmi->ptr->data);
        }
      }
#endif

      if (kmi->ptr && IDP_EqualsProperties_ex(
                          properties, static_cast<const IDProperty *>(kmi->ptr->data), is_strict))
      {
        kmi_match = true;
      }
      /* Debug only, helps spotting mismatches between menu entries and shortcuts! */
      else if (G.debug & G_DEBUG_WM) {
        if (is_strict && kmi->ptr) {
          wmOperatorType *ot = WM_operatortype_find(opname, true);
          if (ot) {
            /* Make a copy of the properties and set unset ones to their default values. */
            IDProperty *properties_default = IDP_CopyProperty(
                static_cast<const IDProperty *>(kmi->ptr->data));

            PointerRNA opptr = RNA_pointer_create_discrete(nullptr, ot->srna, properties_default);
            WM_operator_properties_default(&opptr, true);

            if (IDP_EqualsProperties_ex(properties, properties_default, is_strict)) {
              std::string kmi_str = WM_keymap_item_to_string(kmi, false).value_or("");
              /* NOTE: given properties could come from other things than menu entry. */
              printf(
                  "%s: Some set values in menu entry match default op values, "
                  "this might not be desired!\n",
                  opname);
              printf("\tkm: '%s', kmi: '%s'\n", keymap->idname, kmi_str.c_str());
#ifndef NDEBUG
#  ifdef WITH_PYTHON
              printf("OPERATOR\n");
              IDP_print(properties);
              printf("KEYMAP\n");
              IDP_print(static_cast<IDProperty *>(kmi->ptr->data));
#  endif
#endif
              printf("\n");
            }

            IDP_FreeProperty(properties_default);
          }
        }
      }
    }
    else {
      kmi_match = true;
    }

    if (kmi_match) {
      if ((params == nullptr) || params->filter_fn(keymap, kmi, params->user_data)) {
        return kmi;
      }
    }
  }
  return nullptr;
}

static wmKeyMapItem *wm_keymap_item_find_handlers(const bContext *C,
                                                  wmWindowManager *wm,
                                                  wmWindow *win,
                                                  ListBase *handlers,
                                                  const char *opname,
                                                  blender::wm::OpCallContext /*opcontext*/,
                                                  const IDProperty *properties,
                                                  const bool is_strict,
                                                  const wmKeyMapItemFind_Params *params,
                                                  wmKeyMap **r_keymap)
{
  /* Find keymap item in handlers. */
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
      wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
      wmEventHandler_KeymapResult km_result;
      WM_event_get_keymaps_from_handler(wm, win, handler, &km_result);
      for (int km_index = 0; km_index < km_result.keymaps_len; km_index++) {
        wmKeyMap *keymap = km_result.keymaps[km_index];
        if (WM_keymap_poll((bContext *)C, keymap)) {
          wmKeyMapItem *kmi = wm_keymap_item_find_in_keymap(
              keymap, opname, properties, is_strict, params);
          if (kmi != nullptr) {
            if (r_keymap) {
              *r_keymap = keymap;
            }
            return kmi;
          }
        }
      }
    }
  }
  /* Ensure un-initialized keymap is never used. */
  if (r_keymap) {
    *r_keymap = nullptr;
  }
  return nullptr;
}

static wmKeyMapItem *wm_keymap_item_find_props(const bContext *C,
                                               const char *opname,
                                               blender::wm::OpCallContext opcontext,
                                               const IDProperty *properties,
                                               const bool is_strict,
                                               const wmKeyMapItemFind_Params *params,
                                               wmKeyMap **r_keymap)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  wmKeyMapItem *found = nullptr;

  /* Look into multiple handler lists to find the item. */
  if (win) {
    found = wm_keymap_item_find_handlers(C,
                                         wm,
                                         win,
                                         &win->modalhandlers,
                                         opname,
                                         opcontext,
                                         properties,
                                         is_strict,
                                         params,
                                         r_keymap);
    if (found == nullptr) {
      found = wm_keymap_item_find_handlers(
          C, wm, win, &win->handlers, opname, opcontext, properties, is_strict, params, r_keymap);
    }
  }

  if (area && found == nullptr) {
    found = wm_keymap_item_find_handlers(
        C, wm, win, &area->handlers, opname, opcontext, properties, is_strict, params, r_keymap);
  }

  if (found == nullptr) {
    if (ELEM(opcontext,
             blender::wm::OpCallContext::ExecRegionWin,
             blender::wm::OpCallContext::InvokeRegionWin))
    {
      if (area) {
        if (!(region && region->regiontype == RGN_TYPE_WINDOW)) {
          region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
        }

        if (region) {
          found = wm_keymap_item_find_handlers(C,
                                               wm,
                                               win,
                                               &region->runtime->handlers,
                                               opname,
                                               opcontext,
                                               properties,
                                               is_strict,
                                               params,
                                               r_keymap);
        }
      }
    }
    else if (ELEM(opcontext,
                  blender::wm::OpCallContext::ExecRegionChannels,
                  blender::wm::OpCallContext::InvokeRegionChannels))
    {
      if (!(region && region->regiontype == RGN_TYPE_CHANNELS)) {
        region = BKE_area_find_region_type(area, RGN_TYPE_CHANNELS);
      }

      if (region) {
        found = wm_keymap_item_find_handlers(C,
                                             wm,
                                             win,
                                             &region->runtime->handlers,
                                             opname,
                                             opcontext,
                                             properties,
                                             is_strict,
                                             params,
                                             r_keymap);
      }
    }
    else if (ELEM(opcontext,
                  blender::wm::OpCallContext::ExecRegionPreview,
                  blender::wm::OpCallContext::InvokeRegionPreview))
    {
      if (!(region && region->regiontype == RGN_TYPE_PREVIEW)) {
        region = BKE_area_find_region_type(area, RGN_TYPE_PREVIEW);
      }

      if (region) {
        found = wm_keymap_item_find_handlers(C,
                                             wm,
                                             win,
                                             &region->runtime->handlers,
                                             opname,
                                             opcontext,
                                             properties,
                                             is_strict,
                                             params,
                                             r_keymap);
      }
    }
    else {
      if (region) {
        found = wm_keymap_item_find_handlers(C,
                                             wm,
                                             win,
                                             &region->runtime->handlers,
                                             opname,
                                             opcontext,
                                             properties,
                                             is_strict,
                                             params,
                                             r_keymap);
      }
    }
  }

  return found;
}

static wmKeyMapItem *wm_keymap_item_find(const bContext *C,
                                         const char *opname,
                                         blender::wm::OpCallContext opcontext,
                                         IDProperty *properties,
                                         bool is_strict,
                                         const wmKeyMapItemFind_Params *params,
                                         wmKeyMap **r_keymap)
{
  /* XXX Hack! Macro operators in menu entry have their whole props defined,
   * which is not the case for relevant keymap entries.
   * Could be good to check and harmonize this,
   * but for now always compare non-strict in this case. */
  wmOperatorType *ot = WM_operatortype_find(opname, true);
  if (ot) {
    is_strict = is_strict && ((ot->flag & OPTYPE_MACRO) == 0);
  }

  wmKeyMapItem *found = wm_keymap_item_find_props(
      C, opname, opcontext, properties, is_strict, params, r_keymap);

  /* This block is *only* useful in one case: when op uses an enum menu in its prop member
   * (then, we want to rerun a comparison with that 'prop' unset). Note this remains brittle,
   * since now any enum prop may be used in UI (specified by name), ot->prop is not so much used...
   * Otherwise:
   *     * If non-strict, unset properties always match set ones in IDP_EqualsProperties_ex.
   *     * If strict, unset properties never match set ones in IDP_EqualsProperties_ex,
   *       and we do not want that to change (else we get things like #41757)!
   * ...so in either case, re-running a comparison with unset props set to default is useless.
   */
  if (!found && properties) {
    if (ot && ot->prop) { /* XXX Shall we also check ot->prop is actually an enum? */
      /* Make a copy of the properties and unset the 'ot->prop' one if set. */
      IDProperty *properties_temp = IDP_CopyProperty(properties);

      PointerRNA opptr = RNA_pointer_create_discrete(nullptr, ot->srna, properties_temp);

      if (RNA_property_is_set(&opptr, ot->prop)) {
        /* For operator that has enum menu,
         * unset it so its value does not affect comparison result. */
        RNA_property_unset(&opptr, ot->prop);

        found = wm_keymap_item_find_props(
            C, opname, opcontext, properties_temp, is_strict, params, r_keymap);
      }

      IDP_FreeProperty(properties_temp);
    }
  }

  /* Debug only, helps spotting mismatches between menu entries and shortcuts! */
  if (G.debug & G_DEBUG_WM) {
    if (!found && is_strict && properties) {
      if (ot) {
        /* Make a copy of the properties and set unset ones to their default values. */
        IDProperty *properties_default = IDP_CopyProperty(properties);

        PointerRNA opptr = RNA_pointer_create_discrete(nullptr, ot->srna, properties_default);
        WM_operator_properties_default(&opptr, true);

        wmKeyMap *km;
        wmKeyMapItem *kmi = wm_keymap_item_find_props(
            C, opname, opcontext, properties_default, is_strict, params, &km);
        if (kmi) {
          std::string kmi_str = WM_keymap_item_to_string(kmi, false).value_or("");
          printf(
              "%s: Some set values in keymap entry match default op values, "
              "this might not be desired!\n",
              opname);
          printf("\tkm: '%s', kmi: '%s'\n", km->idname, kmi_str.c_str());
#ifndef NDEBUG
#  ifdef WITH_PYTHON
          printf("OPERATOR\n");
          IDP_print(properties);
          printf("KEYMAP\n");
          IDP_print(static_cast<IDProperty *>(kmi->ptr->data));
#  endif
#endif
          printf("\n");
        }

        IDP_FreeProperty(properties_default);
      }
    }
  }

  return found;
}

static bool kmi_filter_is_visible(const wmKeyMap * /*km*/,
                                  const wmKeyMapItem *kmi,
                                  void * /*user_data*/)
{
  return ((WM_key_event_string(kmi->type, false)[0] != '\0') &&
          (IS_EVENT_ACTIONZONE(kmi->type) == false));
}

std::optional<std::string> WM_key_event_operator_string(const bContext *C,
                                                        const char *opname,
                                                        blender::wm::OpCallContext opcontext,
                                                        IDProperty *properties,
                                                        const bool is_strict)
{
  wmKeyMapItemFind_Params params{};
  params.filter_fn = kmi_filter_is_visible;
  params.user_data = nullptr;
  wmKeyMapItem *kmi = wm_keymap_item_find(
      C, opname, opcontext, properties, is_strict, &params, nullptr);
  if (kmi) {
    return WM_keymap_item_to_string(kmi, false);
  }

  return std::nullopt;
}

static bool kmi_filter_is_visible_type_mask(const wmKeyMap *km,
                                            const wmKeyMapItem *kmi,
                                            void *user_data)
{
  short *mask_pair = static_cast<short int *>(user_data);
  return ((WM_event_type_mask_test(kmi->type, eEventType_Mask(mask_pair[0])) == true) &&
          (WM_event_type_mask_test(kmi->type, eEventType_Mask(mask_pair[1])) == false) &&
          kmi_filter_is_visible(km, kmi, user_data));
}

wmKeyMapItem *WM_key_event_operator(const bContext *C,
                                    const char *opname,
                                    blender::wm::OpCallContext opcontext,
                                    IDProperty *properties,
                                    const short include_mask,
                                    const short exclude_mask,
                                    wmKeyMap **r_keymap)
{
  short user_data_mask[2] = {include_mask, exclude_mask};
  bool use_mask = (include_mask != EVT_TYPE_MASK_ALL) || (exclude_mask != 0);
  wmKeyMapItemFind_Params params{};
  params.filter_fn = use_mask ? kmi_filter_is_visible_type_mask : kmi_filter_is_visible;
  params.user_data = use_mask ? user_data_mask : nullptr;
  return wm_keymap_item_find(C, opname, opcontext, properties, true, &params, r_keymap);
}

wmKeyMapItem *WM_key_event_operator_from_keymap(wmKeyMap *keymap,
                                                const char *opname,
                                                IDProperty *properties,
                                                const short include_mask,
                                                const short exclude_mask)
{
  short user_data_mask[2] = {include_mask, exclude_mask};
  bool use_mask = (include_mask != EVT_TYPE_MASK_ALL) || (exclude_mask != 0);
  wmKeyMapItemFind_Params params{};
  params.filter_fn = use_mask ? kmi_filter_is_visible_type_mask : kmi_filter_is_visible;
  params.user_data = use_mask ? user_data_mask : nullptr;

  return wm_keymap_item_find_in_keymap(keymap, opname, properties, true, &params);
}

bool WM_keymap_item_compare(const wmKeyMapItem *k1, const wmKeyMapItem *k2)
{
  if (k1->flag & KMI_INACTIVE || k2->flag & KMI_INACTIVE) {
    return false;
  }

  /* Take event mapping into account. */
  int k1type = WM_userdef_event_map(k1->type);
  int k2type = WM_userdef_event_map(k2->type);

  if (k1type != KM_ANY && k2type != KM_ANY && k1type != k2type) {
    return false;
  }

  if (k1->val != KM_ANY && k2->val != KM_ANY) {
    /* Take click, press, release conflict into account. */
    if (k1->val == KM_CLICK && ELEM(k2->val, KM_PRESS, KM_RELEASE, KM_CLICK) == 0) {
      return false;
    }
    if (k2->val == KM_CLICK && ELEM(k1->val, KM_PRESS, KM_RELEASE, KM_CLICK) == 0) {
      return false;
    }
    if (k1->val != k2->val) {
      return false;
    }
    if (k1->val == KM_PRESS_DRAG && (k1->direction != k2->direction)) {
      return false;
    }
  }

  if (k1->shift != KM_ANY && k2->shift != KM_ANY && k1->shift != k2->shift) {
    return false;
  }

  if (k1->ctrl != KM_ANY && k2->ctrl != KM_ANY && k1->ctrl != k2->ctrl) {
    return false;
  }

  if (k1->alt != KM_ANY && k2->alt != KM_ANY && k1->alt != k2->alt) {
    return false;
  }

  if (k1->oskey != KM_ANY && k2->oskey != KM_ANY && k1->oskey != k2->oskey) {
    return false;
  }

  if (k1->hyper != KM_ANY && k2->hyper != KM_ANY && k1->hyper != k2->hyper) {
    return false;
  }

  if (k1->keymodifier != k2->keymodifier) {
    return false;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update Final Configuration
 *
 * On load or other changes, the final user key configuration is rebuilt from the preset,
 * add-on and user preferences keymaps. We also test if the final configuration changed and write
 * the changes to the user preferences.
 * \{ */

/* So operator removal can trigger update. */
enum {
  WM_KEYMAP_UPDATE_RECONFIGURE = (1 << 0),

  /* Ensure all wmKeyMap have their operator types validated after removing an operator. */
  WM_KEYMAP_UPDATE_OPERATORTYPE = (1 << 1),

  WM_KEYMAP_UPDATE_POSTPONE = (1 << 2),
};

static char wm_keymap_update_flag = 0;

static char wm_keymap_update_suppress_flag = 0;
#ifndef NDEBUG
static int8_t wm_keymap_update_suppress_count = 0;
#endif

void WM_keyconfig_update_tag(wmKeyMap *keymap, wmKeyMapItem *kmi)
{
  /* Quick tag to do delayed keymap updates. */
  wm_keymap_update_flag |= WM_KEYMAP_UPDATE_RECONFIGURE;

  if (keymap) {
    keymap->flag |= KEYMAP_UPDATE;
  }
  if (kmi) {
    kmi->flag |= KMI_UPDATE;
  }
}

void WM_keyconfig_update_operatortype_tag()
{
  wm_keymap_update_flag |= WM_KEYMAP_UPDATE_OPERATORTYPE;
}

/* NOTE(@ideasman42): regarding suppressing updates.
 * If this becomes a common operation it would be better use something more general,
 * a key-map flag for example to signify that the key-map is stored outside of a #wmKeyConfig
 * and should not receive updates on modification. At the moment this has the down-side of
 * needing to be supported in quite a few places for something which isn't used much.
 * Since the use case for this is limited, add functions to begin/end suppression.
 * If these end up being used a lot we can consider alternatives. */

void WM_keyconfig_update_suppress_begin()
{
#ifndef NDEBUG
  BLI_assert(wm_keymap_update_suppress_count == 0);
  wm_keymap_update_suppress_count += 1;
#endif
  wm_keymap_update_suppress_flag = wm_keymap_update_flag;
}

void WM_keyconfig_update_suppress_end()
{
#ifndef NDEBUG
  BLI_assert(wm_keymap_update_suppress_count == 1);
  wm_keymap_update_suppress_count -= 1;
#endif
  wm_keymap_update_flag = wm_keymap_update_suppress_flag;
  wm_keymap_update_suppress_flag = 0;
}

void WM_keyconfig_update_postpone_begin()
{
  wm_keymap_update_flag |= WM_KEYMAP_UPDATE_POSTPONE;
}

void WM_keyconfig_update_postpone_end()
{
  wm_keymap_update_flag &= ~WM_KEYMAP_UPDATE_POSTPONE;
}

static bool wm_keymap_test_and_clear_update(wmKeyMap *km)
{
  int update = (km->flag & KEYMAP_UPDATE);
  km->flag &= ~KEYMAP_UPDATE;

  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &km->items) {
    update = update || (kmi->flag & KMI_UPDATE);
    kmi->flag &= ~KMI_UPDATE;
  }

  return (update != 0);
}

static wmKeyMap *wm_keymap_preset(wmWindowManager *wm, wmKeyConfig *keyconf, wmKeyMap *km)
{
  BLI_assert(keyconf == WM_keyconfig_active(wm));
  wmKeyMap *keymap = WM_keymap_list_find(&keyconf->keymaps, km->idname, km->spaceid, km->regionid);
  if (!keymap && wm->runtime->defaultconf) {
    keymap = WM_keymap_list_find(
        &wm->runtime->defaultconf->keymaps, km->idname, km->spaceid, km->regionid);
  }

  return keymap;
}

void WM_keyconfig_update_on_startup(wmWindowManager *wm)
{
  /* Ignore #WM_KEYMAP_UPDATE_OPERATORTYPE flag on startup,
   * it's likely to be enabled because it's set when registering any operator
   * however running this is unnecessary since the key-map hasn't been initialized.
   * It's harmless but would add redundant initialization every startup. */
  wm_keymap_update_flag &= ~WM_KEYMAP_UPDATE_OPERATORTYPE;

  WM_keyconfig_update_ex(wm, false);
}

void WM_keyconfig_update(wmWindowManager *wm)
{
  WM_keyconfig_update_ex(wm, false);
}

void WM_keyconfig_update_ex(wmWindowManager *wm, bool keep_properties)
{
  if (wm_keymap_update_flag == 0) {
    return;
  }

  bool compat_update = false;

  /* Update drop-boxes when the operators have been added or removed. While this isn't an ideal
   * place to update drop-boxes, they share characteristics with key-map items.
   * We could consider renaming this to API function so it's not only relating to keymaps. */
  bool dropbox_update = false;

  /* Postpone update until after the key-map has been initialized
   * to ensure add-ons have been loaded, see: #113603. */
  if (wm_keymap_update_flag & WM_KEYMAP_UPDATE_POSTPONE) {
    return;
  }

  if (wm_keymap_update_flag & WM_KEYMAP_UPDATE_OPERATORTYPE) {
    /* One or more operator-types have been removed, this won't happen often
     * but when it does we have to check _every_ key-map item. */
    wm_keymap_item_properties_update_ot_from_list(&U.user_keymaps, keep_properties);
    LISTBASE_FOREACH (wmKeyConfig *, kc, &wm->runtime->keyconfigs) {
      wm_keymap_item_properties_update_ot_from_list(&kc->keymaps, keep_properties);
    }

    /* An operator has been removed, refresh. */
    dropbox_update = true;

    wm_keymap_update_flag &= ~WM_KEYMAP_UPDATE_OPERATORTYPE;
  }

  if (wm_keymap_update_flag & WM_KEYMAP_UPDATE_RECONFIGURE) {
    wmKeyConfig *kc_active = WM_keyconfig_active(wm);

    /* Update operator properties for non-modal user keymaps. */
    LISTBASE_FOREACH (wmKeyMap *, km, &U.user_keymaps) {
      if ((km->flag & KEYMAP_MODAL) == 0) {
        LISTBASE_FOREACH (wmKeyMapDiffItem *, kmdi, &km->diff_items) {
          if (kmdi->add_item) {
            wm_keymap_item_properties_set(kmdi->add_item);
          }
          if (kmdi->remove_item) {
            wm_keymap_item_properties_set(kmdi->remove_item);
          }
        }

        LISTBASE_FOREACH (wmKeyMapItem *, kmi, &km->items) {
          wm_keymap_item_properties_set(kmi);
        }
      }
    }

    /* Update `U.user_keymaps` with user key configuration changes. */
    LISTBASE_FOREACH (wmKeyMap *, km, &wm->runtime->userconf->keymaps) {
      /* Only diff if the user keymap was modified. */
      if (wm_keymap_test_and_clear_update(km)) {
        /* Find keymaps. */
        wmKeyMap *defaultmap = wm_keymap_preset(wm, kc_active, km);
        wmKeyMap *addonmap = WM_keymap_list_find(
            &wm->runtime->addonconf->keymaps, km->idname, km->spaceid, km->regionid);

        /* Diff. */
        if (defaultmap) {
          wm_keymap_diff_update(&U.user_keymaps, defaultmap, addonmap, km);
        }
      }
    }

    /* Create user key configuration from preset + addon + user preferences. */
    LISTBASE_FOREACH (wmKeyMap *, km, &wm->runtime->defaultconf->keymaps) {
      /* Find keymaps. */
      wmKeyMap *defaultmap = wm_keymap_preset(wm, kc_active, km);
      wmKeyMap *addonmap = WM_keymap_list_find(
          &wm->runtime->addonconf->keymaps, km->idname, km->spaceid, km->regionid);
      wmKeyMap *usermap = WM_keymap_list_find(
          &U.user_keymaps, km->idname, km->spaceid, km->regionid);

      /* For now only the default map defines modal key-maps,
       * if we support modal keymaps for 'addonmap', these will need to be enabled too. */
      wm_user_modal_keymap_set_items(wm, defaultmap);

      /* Add. */
      wmKeyMap *kmn = wm_keymap_patch_update(
          &wm->runtime->userconf->keymaps, defaultmap, addonmap, usermap);

      if (kmn) {
        kmn->modal_items = km->modal_items;
        kmn->poll = km->poll;
        kmn->poll_modal_item = km->poll_modal_item;
      }

      /* In case of old non-diff keymaps, force extra update to create diffs. */
      compat_update = compat_update || (usermap && !(usermap->flag & KEYMAP_DIFF));
    }

    /* An operator may have been added, refresh. */
    dropbox_update = true;

    wm_keymap_update_flag &= ~WM_KEYMAP_UPDATE_RECONFIGURE;
  }

  BLI_assert(wm_keymap_update_flag == 0);

  if (compat_update) {
    WM_keyconfig_update_tag(nullptr, nullptr);
    WM_keyconfig_update(wm);
  }

  if (dropbox_update) {
    WM_dropbox_update_ot();
  }

  /* NOTE(@ideasman42): open preferences will contain "stale" #wmKeyMapItem data.
   *
   * The common case this solves is using Blender with the key-map editor open,
   * an action in the view-port for example may manipulate the key-map causing it to be rebuilt.
   * Later interaction with the key-map editor may then attempt to access freed data.
   *
   * Take care, this is _not_ fool proof because it's possible:
   * - Key-map options to be shown in any region (using scripts).
   * - Key-map re-generation could happen while the preferences is open,
   *   where the data becomes stale before the UI has a chance to redraw.
   *
   * In practice both cases are quite unlikely though. */
  if (U.space_data.section_active == USER_SECTION_KEYMAP) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      bScreen *screen = WM_window_get_active_screen(win);
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        if (area->spacetype == SPACE_USERPREF) {
          ED_area_tag_redraw(area);
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Handling
 *
 * Handlers have pointers to the keymap in the default configuration.
 * During event handling this function is called to get the keymap from the final configuration.
 * \{ */

wmKeyMap *WM_keymap_active(const wmWindowManager *wm, wmKeyMap *keymap)
{
  if (!keymap) {
    return nullptr;
  }

  /* First user defined keymaps. */
  wmKeyMap *km = WM_keymap_list_find(
      &wm->runtime->userconf->keymaps, keymap->idname, keymap->spaceid, keymap->regionid);

  if (km) {
    return km;
  }

  return keymap;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keymap Editor
 *
 * In the keymap editor the user key configuration is edited.
 * \{ */

void WM_keymap_item_restore_to_default(wmWindowManager *wm, wmKeyMap *keymap, wmKeyMapItem *kmi)
{
  if (!keymap) {
    return;
  }

  /* Construct default keymap from preset + addons. */
  wmKeyConfig *kc_active = WM_keyconfig_active(wm);
  wmKeyMap *defaultmap = wm_keymap_preset(wm, kc_active, keymap);
  wmKeyMap *addonmap = WM_keymap_list_find(
      &wm->runtime->addonconf->keymaps, keymap->idname, keymap->spaceid, keymap->regionid);

  if (addonmap) {
    defaultmap = wm_keymap_copy(defaultmap);
    wm_keymap_addon_add(defaultmap, addonmap);
  }

  /* Find original item. */
  wmKeyMapItem *orig = WM_keymap_item_find_id(defaultmap, kmi->id);

  if (orig) {
    /* Restore to original. */
    if (!STREQ(orig->idname, kmi->idname)) {
      STRNCPY(kmi->idname, orig->idname);
      WM_keymap_item_properties_reset(kmi, nullptr);
    }

    if (orig->properties) {
      if (kmi->properties) {
        IDP_FreeProperty(kmi->properties);
        kmi->properties = nullptr;
      }

      kmi->properties = IDP_CopyProperty(orig->properties);
      if (kmi->ptr) {
        kmi->ptr->data = kmi->properties;
      }
    }

    kmi->propvalue = orig->propvalue;
    kmi->type = orig->type;
    kmi->val = orig->val;
    kmi->shift = orig->shift;
    kmi->ctrl = orig->ctrl;
    kmi->alt = orig->alt;
    kmi->oskey = orig->oskey;
    kmi->hyper = orig->hyper;
    kmi->keymodifier = orig->keymodifier;
    kmi->maptype = orig->maptype;
    kmi->flag = (kmi->flag & ~(KMI_REPEAT_IGNORE | KMI_INACTIVE)) |
                (orig->flag & (KMI_REPEAT_IGNORE | KMI_INACTIVE));

    WM_keyconfig_update_tag(keymap, kmi);
  }

  /* Free temporary keymap. */
  if (addonmap) {
    WM_keymap_clear(defaultmap);
    MEM_freeN(defaultmap);
  }
}

void WM_keymap_restore_to_default(wmKeyMap *keymap, wmWindowManager *wm)
{
  /* Remove keymap from U.user_keymaps and update. */
  wmKeyMap *usermap = WM_keymap_list_find(
      &U.user_keymaps, keymap->idname, keymap->spaceid, keymap->regionid);

  if (usermap) {
    WM_keymap_clear(usermap);
    BLI_freelinkN(&U.user_keymaps, usermap);

    WM_keyconfig_update_tag(nullptr, nullptr);
    WM_keyconfig_update(wm);
  }
}

const char *WM_bool_as_string(bool test)
{
  return test ? IFACE_("ON") : IFACE_("OFF");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keymap Queries
 * \{ */

wmKeyMapItem *WM_keymap_item_find_id(wmKeyMap *keymap, const int id)
{
  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
    if (kmi->id == id) {
      return kmi;
    }
  }

  return nullptr;
}

wmKeyMapItem *WM_keymap_item_find_match(wmKeyMap *km_base,
                                        wmKeyMap *km_match,
                                        const wmKeyMapItem *kmi_match,
                                        ReportList *reports)
{
  /* NOTE: this is called by RNA, some of the reports in this function
   * would be asserts when called from C++. */

  if (wm_keymap_update_flag != 0) {
    /* NOTE: this could be limited to the key-maps marked for updating.
     * However #KEYMAP_UPDATE is only cleared for `wm->runtime->userconf`
     * so only check the global flag for now.
     *
     * Use a warning not an error because scripts cannot prevent other scripts from manipulating
     * key-map items, so we won't want scripts to create exceptions in unrelated scripts.
     * Ideally we could detect which key-maps have been modified. */
    BKE_reportf(reports,
                RPT_WARNING,
                "KeyMap item result may be incorrect since an update is pending, call "
                "`context.window_manager.keyconfigs.update()` to ensure matches can be found.");
  }

  if (km_base == km_match) {
    /* We could also return `kmi_match` (it's technically correct)
     * however this is almost certainly API misuse (as it's a no-op). */
    BKE_report(reports, RPT_ERROR, "KeyMaps are equal");
    return nullptr;
  }

  const char *idname = km_base->idname;
  const short spaceid = km_base->spaceid;
  const short regionid = km_base->regionid;

  if (!wm_keymap_is_match(km_base, km_match)) {
    BKE_reportf(
        reports, RPT_ERROR, "KeyMap \"%s\" does not match \"%s\"", idname, km_match->idname);
    return nullptr;
  }

  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
  wmKeyConfig *kc_active = WM_keyconfig_active(wm);

  /* NOTE: the key-maps could store this, it would simplify checks here. */
  enum {
    KM_TYPE_UNKNOWN = 0,
    KM_TYPE_USER,
    KM_TYPE_ADDON,
    KM_TYPE_ACTIVE,
    /* No support yet for preferences. */
  } base_type = KM_TYPE_UNKNOWN,
    match_type = KM_TYPE_UNKNOWN;

  if (km_base->flag & KEYMAP_USER) {
    if (km_base == WM_keymap_list_find(&wm->runtime->userconf->keymaps, idname, spaceid, regionid))
    {
      base_type = KM_TYPE_USER;
    }
  }

  if ((km_match->flag & KEYMAP_USER) == 0) {
    if (km_match ==
        WM_keymap_list_find(&wm->runtime->addonconf->keymaps, idname, spaceid, regionid))
    {
      match_type = KM_TYPE_ADDON;
    }
    else if (km_match == WM_keymap_list_find(&kc_active->keymaps, idname, spaceid, regionid)) {
      match_type = KM_TYPE_ACTIVE;
    }
  }

  if (base_type == KM_TYPE_UNKNOWN) {
    BKE_reportf(reports, RPT_ERROR, "KeyMap \"%s\" (base) must be a user keymap", idname);
    return nullptr;
  }

  if (match_type == KM_TYPE_UNKNOWN) {
    BKE_reportf(
        reports, RPT_ERROR, "KeyMap \"%s\" (other) must be an add-on or active keymap", idname);
    return nullptr;
  }

  const int kmi_index = BLI_findindex(&km_match->items, kmi_match);
  if (kmi_index == -1) {
    BKE_reportf(reports, RPT_ERROR, "KeyMap \"%s\" item not part of the keymap", idname);
    return nullptr;
  }

  int kmi_id;
  if (match_type == KM_TYPE_ADDON) {
    /* Perform the following lookup that calculates the ID that *would* be used
     * if the user key-map was re-created, see: #WM_keymap_item_restore_to_default.
     *
     * Find the index of the key-map item and add this to the `defaultmap`'s key-map index
     * since this is how the "user" key-map ID's are generated.
     *
     * This is needed so add-ons can show the user key-map items in preferences. */

    wmKeyMap *defaultmap = wm_keymap_preset(wm, kc_active, km_base);
    if (defaultmap == nullptr) {
      /* This should practically never fail, it could be caused by failure
       * to refresh the user key-map after manipulating the add-on key-map. */
      return nullptr;
    }
    kmi_id = defaultmap->kmi_id + kmi_index + 1;
  }
  else {
    kmi_id = kmi_match->id;
  }

  /* Returning null here isn't an error because it's possible there is no match. */
  return WM_keymap_item_find_id(km_base, kmi_id);
}

/** \} */
