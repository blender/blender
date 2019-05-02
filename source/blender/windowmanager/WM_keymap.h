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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

#ifndef __WM_KEYMAP_H__
#define __WM_KEYMAP_H__

/** \file
 * \ingroup wm
 */

/* dna-savable wmStructs here */
#include "DNA_windowmanager_types.h"
#include "BLI_utildefines.h"

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

void WM_keymap_clear(struct wmKeyMap *keymap);

wmKeyMapItem *WM_keymap_verify_item(
    struct wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier);
wmKeyMapItem *WM_keymap_add_item(
    struct wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier);
wmKeyMapItem *WM_keymap_add_item_copy(struct wmKeyMap *keymap, wmKeyMapItem *kmi_src);

bool WM_keymap_remove_item(struct wmKeyMap *keymap, struct wmKeyMapItem *kmi);
int WM_keymap_item_to_string(wmKeyMapItem *kmi,
                             const bool compact,
                             char *result,
                             const int result_len);

wmKeyMap *WM_keymap_list_find(ListBase *lb, const char *idname, int spaceid, int regionid);
wmKeyMap *WM_keymap_list_find_spaceid_or_empty(ListBase *lb,
                                               const char *idname,
                                               int spaceid,
                                               int regionid);
wmKeyMap *WM_keymap_ensure(struct wmKeyConfig *keyconf,
                           const char *idname,
                           int spaceid,
                           int regionid);
wmKeyMap *WM_keymap_find_all(const struct bContext *C,
                             const char *idname,
                             int spaceid,
                             int regionid);
wmKeyMap *WM_keymap_find_all_spaceid_or_empty(const struct bContext *C,
                                              const char *idname,
                                              int spaceid,
                                              int regionid);
wmKeyMap *WM_keymap_active(struct wmWindowManager *wm, struct wmKeyMap *keymap);
bool WM_keymap_remove(struct wmKeyConfig *keyconfig, struct wmKeyMap *keymap);
bool WM_keymap_poll(struct bContext *C, struct wmKeyMap *keymap);

wmKeyMapItem *WM_keymap_item_find_id(struct wmKeyMap *keymap, int id);
bool WM_keymap_item_compare(struct wmKeyMapItem *k1, struct wmKeyMapItem *k2);

/* keymap_utils.c */

/** Wrappers for #WM_keymap_add_item */
wmKeyMapItem *WM_keymap_add_menu(
    struct wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier);
wmKeyMapItem *WM_keymap_add_menu_pie(
    struct wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier);
wmKeyMapItem *WM_keymap_add_panel(
    struct wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier);
wmKeyMapItem *WM_keymap_add_tool(
    struct wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier);

void WM_keymap_add_context_enum_set_items(wmKeyMap *keymap,
                                          const struct EnumPropertyItem *items,
                                          const char *data_path,
                                          int type_start,
                                          int val,
                                          int modifier,
                                          int keymodifier);

wmKeyMap *WM_keymap_guess_from_context(const struct bContext *C);
wmKeyMap *WM_keymap_guess_opname(const struct bContext *C, const char *opname);

void WM_keymap_fix_linking(void);

/* Modal Keymap */

int WM_modalkeymap_items_to_string(struct wmKeyMap *km,
                                   const int propvalue,
                                   const bool compact,
                                   char *result,
                                   const int result_len);
int WM_modalkeymap_operator_items_to_string(struct wmOperatorType *ot,
                                            const int propvalue,
                                            const bool compact,
                                            char *result,
                                            const int result_len);
char *WM_modalkeymap_operator_items_to_string_buf(struct wmOperatorType *ot,
                                                  const int propvalue,
                                                  const bool compact,
                                                  const int max_len,
                                                  int *r_available_len,
                                                  char **r_result);

wmKeyMap *WM_modalkeymap_add(struct wmKeyConfig *keyconf,
                             const char *idname,
                             const struct EnumPropertyItem *items);
wmKeyMap *WM_modalkeymap_get(struct wmKeyConfig *keyconf, const char *idname);
wmKeyMapItem *WM_modalkeymap_add_item(
    struct wmKeyMap *km, int type, int val, int modifier, int keymodifier, int value);
wmKeyMapItem *WM_modalkeymap_add_item_str(
    struct wmKeyMap *km, int type, int val, int modifier, int keymodifier, const char *value);
wmKeyMapItem *WM_modalkeymap_find_propvalue(wmKeyMap *km, const int propvalue);
void WM_modalkeymap_assign(struct wmKeyMap *km, const char *opname);

/* Keymap Editor */

void WM_keymap_restore_to_default(struct wmKeyMap *keymap, struct bContext *C);
void WM_keymap_item_properties_reset(struct wmKeyMapItem *kmi, struct IDProperty *properties);
void WM_keymap_item_restore_to_default(struct bContext *C,
                                       struct wmKeyMap *keymap,
                                       struct wmKeyMapItem *kmi);
int WM_keymap_item_map_type_get(const struct wmKeyMapItem *kmi);

/* Key Event */

const char *WM_key_event_string(const short type, const bool compact);
int WM_keymap_item_raw_to_string(const short shift,
                                 const short ctrl,
                                 const short alt,
                                 const short oskey,
                                 const short keymodifier,
                                 const short val,
                                 const short type,
                                 const bool compact,
                                 char *result,
                                 const int result_len);
wmKeyMapItem *WM_key_event_operator(const struct bContext *C,
                                    const char *opname,
                                    int opcontext,
                                    struct IDProperty *properties,
                                    const short include_mask,
                                    const short exclude_mask,
                                    struct wmKeyMap **r_keymap);
char *WM_key_event_operator_string(const struct bContext *C,
                                   const char *opname,
                                   int opcontext,
                                   struct IDProperty *properties,
                                   const bool is_strict,
                                   char *result,
                                   const int result_len);

wmKeyMapItem *WM_key_event_operator_from_keymap(struct wmKeyMap *keymap,
                                                const char *opname,
                                                struct IDProperty *properties,
                                                const short include_mask,
                                                const short exclude_mask);

const char *WM_bool_as_string(bool test);

#ifdef __cplusplus
}
#endif

#endif /* __WM_KEYMAP_H__ */
