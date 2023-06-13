/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup wm
 */

/* dna-savable wmStructs here */
#include "BLI_utildefines.h"
#include "DNA_windowmanager_types.h"
#include "WM_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct EnumPropertyItem;

/* Key Configuration */

void WM_keyconfig_init(struct bContext *C);
void WM_keyconfig_reload(struct bContext *C);

wmKeyConfig *WM_keyconfig_new(struct wmWindowManager *wm, const char *idname, bool user_defined);
wmKeyConfig *WM_keyconfig_new_user(struct wmWindowManager *wm, const char *idname);
bool WM_keyconfig_remove(struct wmWindowManager *wm, struct wmKeyConfig *keyconf);
void WM_keyconfig_clear(struct wmKeyConfig *keyconf);
void WM_keyconfig_free(struct wmKeyConfig *keyconf);

void WM_keyconfig_set_active(struct wmWindowManager *wm, const char *idname);

void WM_keyconfig_update(struct wmWindowManager *wm);
void WM_keyconfig_update_tag(struct wmKeyMap *keymap, struct wmKeyMapItem *kmi);
void WM_keyconfig_update_operatortype(void);

/* Keymap */

/** Parameters for matching events, passed into functions that create key-map items. */
typedef struct KeyMapItem_Params {
  /** #wmKeyMapItem.type */
  int16_t type;
  /** #wmKeyMapItem.val */
  int8_t value;
  /** #wmKeyMapItem `ctrl, shift, alt, oskey` */
  int8_t modifier;
  /** #wmKeyMapItem.keymodifier */
  int16_t keymodifier;
  /** #wmKeyMapItem.direction */
  int8_t direction;
} KeyMapItem_Params;

void WM_keymap_clear(struct wmKeyMap *keymap);

/**
 * Always add item.
 */
wmKeyMapItem *WM_keymap_add_item(struct wmKeyMap *keymap,
                                 const char *idname,
                                 const KeyMapItem_Params *params);
wmKeyMapItem *WM_keymap_add_item_copy(struct wmKeyMap *keymap, wmKeyMapItem *kmi_src);

bool WM_keymap_remove_item(struct wmKeyMap *keymap, struct wmKeyMapItem *kmi);
int WM_keymap_item_to_string(const wmKeyMapItem *kmi,
                             bool compact,
                             char *result,
                             int result_maxncpy);

wmKeyMap *WM_keymap_list_find(ListBase *lb, const char *idname, int spaceid, int regionid);
wmKeyMap *WM_keymap_list_find_spaceid_or_empty(ListBase *lb,
                                               const char *idname,
                                               int spaceid,
                                               int regionid);
wmKeyMap *WM_keymap_ensure(struct wmKeyConfig *keyconf,
                           const char *idname,
                           int spaceid,
                           int regionid);
wmKeyMap *WM_keymap_find_all(struct wmWindowManager *wm,
                             const char *idname,
                             int spaceid,
                             int regionid);
wmKeyMap *WM_keymap_find_all_spaceid_or_empty(struct wmWindowManager *wm,
                                              const char *idname,
                                              int spaceid,
                                              int regionid);
wmKeyMap *WM_keymap_active(const struct wmWindowManager *wm, struct wmKeyMap *keymap);
bool WM_keymap_remove(struct wmKeyConfig *keyconfig, struct wmKeyMap *keymap);
bool WM_keymap_poll(struct bContext *C, struct wmKeyMap *keymap);

wmKeyMapItem *WM_keymap_item_find_id(struct wmKeyMap *keymap, int id);
bool WM_keymap_item_compare(const struct wmKeyMapItem *k1, const struct wmKeyMapItem *k2);

/* keymap_utils.c */

/* Wrappers for #WM_keymap_add_item */

/**
 * Menu wrapper for #WM_keymap_add_item.
 */
wmKeyMapItem *WM_keymap_add_menu(struct wmKeyMap *keymap,
                                 const char *idname,
                                 const KeyMapItem_Params *params);
/**
 * Pie-menu wrapper for #WM_keymap_add_item.
 */
wmKeyMapItem *WM_keymap_add_menu_pie(struct wmKeyMap *keymap,
                                     const char *idname,
                                     const KeyMapItem_Params *params);
/**
 * Panel (popover) wrapper for #WM_keymap_add_item.
 */
wmKeyMapItem *WM_keymap_add_panel(struct wmKeyMap *keymap,
                                  const char *idname,
                                  const KeyMapItem_Params *params);
/**
 * Tool wrapper for #WM_keymap_add_item.
 */
wmKeyMapItem *WM_keymap_add_tool(struct wmKeyMap *keymap,
                                 const char *idname,
                                 const KeyMapItem_Params *params);

wmKeyMap *WM_keymap_guess_from_context(const struct bContext *C);

/**
 * Guess an appropriate key-map from the operator name.
 *
 * \note Needs to be kept up to date with Key-map and Operator naming.
 */
wmKeyMap *WM_keymap_guess_opname(const struct bContext *C, const char *opname);

bool WM_keymap_uses_event_modifier(const wmKeyMap *keymap, int event_modifier);

void WM_keymap_fix_linking(void);

/* Modal Keymap */

int WM_modalkeymap_items_to_string(
    const struct wmKeyMap *km, int propvalue, bool compact, char *result, int result_maxncpy);
int WM_modalkeymap_operator_items_to_string(
    struct wmOperatorType *ot, int propvalue, bool compact, char *result, int result_maxncpy);
char *WM_modalkeymap_operator_items_to_string_buf(struct wmOperatorType *ot,
                                                  int propvalue,
                                                  bool compact,
                                                  int result_maxncpy,
                                                  int *r_available_len,
                                                  char **r_result);

wmKeyMap *WM_modalkeymap_ensure(struct wmKeyConfig *keyconf,
                                const char *idname,
                                const struct EnumPropertyItem *items);
wmKeyMap *WM_modalkeymap_find(struct wmKeyConfig *keyconf, const char *idname);
wmKeyMapItem *WM_modalkeymap_add_item(struct wmKeyMap *km,
                                      const KeyMapItem_Params *params,
                                      int value);
wmKeyMapItem *WM_modalkeymap_add_item_str(struct wmKeyMap *km,
                                          const KeyMapItem_Params *params,
                                          const char *value);
const wmKeyMapItem *WM_modalkeymap_find_propvalue(const wmKeyMap *km, int propvalue);
void WM_modalkeymap_assign(struct wmKeyMap *km, const char *opname);

/* Keymap Editor */

void WM_keymap_restore_to_default(struct wmKeyMap *keymap, struct wmWindowManager *wm);
/**
 * Properties can be NULL, otherwise the arg passed is used and ownership is given to the `kmi`.
 */
void WM_keymap_item_properties_reset(struct wmKeyMapItem *kmi, struct IDProperty *properties);
void WM_keymap_item_restore_to_default(wmWindowManager *wm,
                                       struct wmKeyMap *keymap,
                                       struct wmKeyMapItem *kmi);
int WM_keymap_item_map_type_get(const struct wmKeyMapItem *kmi);

/* Key Event */

const char *WM_key_event_string(short type, bool compact);
int WM_keymap_item_raw_to_string(short shift,
                                 short ctrl,
                                 short alt,
                                 short oskey,
                                 short keymodifier,
                                 short val,
                                 short type,
                                 bool compact,
                                 char *result,
                                 int result_maxncpy);
/**
 * \param include_mask, exclude_mask:
 * Event types to include/exclude when looking up keys (#eEventType_Mask).
 */
wmKeyMapItem *WM_key_event_operator(const struct bContext *C,
                                    const char *opname,
                                    wmOperatorCallContext opcontext,
                                    struct IDProperty *properties,
                                    short include_mask,
                                    short exclude_mask,
                                    struct wmKeyMap **r_keymap);
char *WM_key_event_operator_string(const struct bContext *C,
                                   const char *opname,
                                   wmOperatorCallContext opcontext,
                                   struct IDProperty *properties,
                                   bool is_strict,
                                   char *result,
                                   int result_maxncpy);

wmKeyMapItem *WM_key_event_operator_from_keymap(struct wmKeyMap *keymap,
                                                const char *opname,
                                                struct IDProperty *properties,
                                                short include_mask,
                                                short exclude_mask);

const char *WM_bool_as_string(bool test);

#ifdef __cplusplus
}
#endif
