/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup wm
 */

#include "BLI_compiler_attrs.h"

struct IDProperty;
struct Main;
struct PointerRNA;
struct Scene;
struct ScrArea;
struct StructRNA;
struct WorkSpace;
struct bContext;
struct bToolRef;
struct bToolRef_Runtime;
struct ViewLayer;
struct wmMsgSubscribeKey;
struct wmMsgSubscribeValue;
struct wmOperatorType;
struct wmWindow;

/* `wm_toolsystem.cc` */

#define WM_TOOLSYSTEM_SPACE_MASK \
  ((1 << SPACE_IMAGE) | (1 << SPACE_NODE) | (1 << SPACE_VIEW3D) | (1 << SPACE_SEQ))
/**
 * Space-types that define their own "mode" (as returned by #WM_toolsystem_mode_from_spacetype).
 */
#define WM_TOOLSYSTEM_SPACE_MASK_MODE_FROM_SPACE ((1 << SPACE_IMAGE) | (1 << SPACE_SEQ))

/* Values that define a category of active tool. */
struct bToolKey {
  int space_type;
  int mode;
};

bToolRef *WM_toolsystem_ref_from_context(bContext *C);
bToolRef *WM_toolsystem_ref_find(WorkSpace *workspace, const bToolKey *tkey);
bool WM_toolsystem_ref_ensure(WorkSpace *workspace, const bToolKey *tkey, bToolRef **r_tref);

bToolRef *WM_toolsystem_ref_set_by_id_ex(
    bContext *C, WorkSpace *workspace, const bToolKey *tkey, const char *name, bool cycle);
bToolRef *WM_toolsystem_ref_set_by_id(bContext *C, const char *name);

bToolRef_Runtime *WM_toolsystem_runtime_from_context(bContext *C);
bToolRef_Runtime *WM_toolsystem_runtime_find(WorkSpace *workspace, const bToolKey *tkey);

void WM_toolsystem_unlink(bContext *C, WorkSpace *workspace, const bToolKey *tkey);
void WM_toolsystem_refresh(bContext *C, WorkSpace *workspace, const bToolKey *tkey);
void WM_toolsystem_reinit(bContext *C, WorkSpace *workspace, const bToolKey *tkey);

/**
 * Operate on all active tools.
 */
void WM_toolsystem_unlink_all(bContext *C, WorkSpace *workspace);
void WM_toolsystem_refresh_all(bContext *C, WorkSpace *workspace);
void WM_toolsystem_reinit_all(bContext *C, wmWindow *win);

void WM_toolsystem_ref_set_from_runtime(bContext *C,
                                        WorkSpace *workspace,
                                        bToolRef *tref,
                                        const bToolRef_Runtime *tref_rt,
                                        const char *idname);

/**
 * Sync the internal active state of a tool back into the tool system,
 * this is needed for active brushes where the real active state is not stored in the tool system.
 *
 * \see #toolsystem_ref_link
 */
void WM_toolsystem_ref_sync_from_context(Main *bmain, WorkSpace *workspace, bToolRef *tref);

void WM_toolsystem_init(bContext *C);

int WM_toolsystem_mode_from_spacetype(const Scene *scene,
                                      ViewLayer *view_layer,
                                      ScrArea *area,
                                      int space_type);
bool WM_toolsystem_key_from_context(const Scene *scene,
                                    ViewLayer *view_layer,
                                    ScrArea *area,
                                    bToolKey *tkey);

void WM_toolsystem_update_from_context_view3d(bContext *C);
void WM_toolsystem_update_from_context(
    bContext *C, WorkSpace *workspace, const Scene *scene, ViewLayer *view_layer, ScrArea *area);

/**
 * For paint modes to support non-brush tools.
 */
bool WM_toolsystem_active_tool_is_brush(const bContext *C);

/** Follow #wmMsgNotifyFn spec. */
void WM_toolsystem_do_msg_notify_tag_refresh(bContext *C,
                                             wmMsgSubscribeKey *msg_key,
                                             wmMsgSubscribeValue *msg_val);

IDProperty *WM_toolsystem_ref_properties_get_idprops(bToolRef *tref);
IDProperty *WM_toolsystem_ref_properties_ensure_idprops(bToolRef *tref);
void WM_toolsystem_ref_properties_ensure_ex(bToolRef *tref,
                                            const char *idname,
                                            StructRNA *type,
                                            PointerRNA *r_ptr);

#define WM_toolsystem_ref_properties_ensure_from_operator(tref, ot, r_ptr) \
  WM_toolsystem_ref_properties_ensure_ex(tref, (ot)->idname, (ot)->srna, r_ptr)
#define WM_toolsystem_ref_properties_ensure_from_gizmo_group(tref, gzgroup, r_ptr) \
  WM_toolsystem_ref_properties_ensure_ex(tref, (gzgroup)->idname, (gzgroup)->srna, r_ptr)

bool WM_toolsystem_ref_properties_get_ex(bToolRef *tref,
                                         const char *idname,
                                         StructRNA *type,
                                         PointerRNA *r_ptr);
#define WM_toolsystem_ref_properties_get_from_operator(tref, ot, r_ptr) \
  WM_toolsystem_ref_properties_get_ex(tref, (ot)->idname, (ot)->srna, r_ptr)
#define WM_toolsystem_ref_properties_get_from_gizmo_group(tref, gzgroup, r_ptr) \
  WM_toolsystem_ref_properties_get_ex(tref, (gzgroup)->idname, (gzgroup)->srna, r_ptr)

void WM_toolsystem_ref_properties_init_for_keymap(bToolRef *tref,
                                                  PointerRNA *dst_ptr,
                                                  PointerRNA *src_ptr,
                                                  wmOperatorType *ot);

/**
 * Use to update the active tool (shown in the top bar) in the least disruptive way.
 *
 * This is a little involved since there may be multiple valid active tools
 * depending on the mode and space type.
 *
 * Used when undoing since the active mode may have changed.
 */
void WM_toolsystem_refresh_active(bContext *C);

void WM_toolsystem_refresh_screen_area(WorkSpace *workspace,
                                       const Scene *scene,
                                       ViewLayer *view_layer,
                                       ScrArea *area);
void WM_toolsystem_refresh_screen_window(wmWindow *win);
void WM_toolsystem_refresh_screen_all(Main *bmain);
