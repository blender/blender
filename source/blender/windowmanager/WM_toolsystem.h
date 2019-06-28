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
#ifndef __WM_TOOLSYSTEM_H__
#define __WM_TOOLSYSTEM_H__

/** \file
 * \ingroup wm
 */

/* dna-savable wmStructs here */
#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct IDProperty;
struct Main;
struct PointerRNA;
struct ScrArea;
struct StructRNA;
struct WorkSpace;
struct bContext;
struct bToolRef_Runtime;
struct wmMsgSubscribeKey;
struct wmMsgSubscribeValue;
struct wmOperatorType;

/* wm_toolsystem.c  */

#define WM_TOOLSYSTEM_SPACE_MASK ((1 << SPACE_IMAGE) | (1 << SPACE_NODE) | (1 << SPACE_VIEW3D))

/* Values that define a categoey of active tool. */
typedef struct bToolKey {
  int space_type;
  int mode;
} bToolKey;

struct bToolRef *WM_toolsystem_ref_from_context(struct bContext *C);
struct bToolRef *WM_toolsystem_ref_find(struct WorkSpace *workspace, const bToolKey *tkey);
bool WM_toolsystem_ref_ensure(struct WorkSpace *workspace,
                              const bToolKey *tkey,
                              struct bToolRef **r_tref);
struct bToolRef *WM_toolsystem_ref_set_by_id(struct bContext *C,
                                             struct WorkSpace *workspace,
                                             const bToolKey *tkey,
                                             const char *name,
                                             bool cycle);

struct bToolRef_Runtime *WM_toolsystem_runtime_from_context(struct bContext *C);
struct bToolRef_Runtime *WM_toolsystem_runtime_find(struct WorkSpace *workspace,
                                                    const bToolKey *tkey);

void WM_toolsystem_unlink(struct bContext *C, struct WorkSpace *workspace, const bToolKey *tkey);
void WM_toolsystem_refresh(struct bContext *C, struct WorkSpace *workspace, const bToolKey *tkey);
void WM_toolsystem_reinit(struct bContext *C, struct WorkSpace *workspace, const bToolKey *tkey);

void WM_toolsystem_unlink_all(struct bContext *C, struct WorkSpace *workspace);
void WM_toolsystem_refresh_all(struct bContext *C, struct WorkSpace *workspace);
void WM_toolsystem_reinit_all(struct bContext *C, struct wmWindow *win);

void WM_toolsystem_ref_set_from_runtime(struct bContext *C,
                                        struct WorkSpace *workspace,
                                        struct bToolRef *tref,
                                        const struct bToolRef_Runtime *tool,
                                        const char *idname);

void WM_toolsystem_ref_sync_from_context(struct Main *bmain,
                                         struct WorkSpace *workspace,
                                         struct bToolRef *tref);

void WM_toolsystem_init(struct bContext *C);

int WM_toolsystem_mode_from_spacetype(struct ViewLayer *view_layer,
                                      struct ScrArea *sa,
                                      int space_type);
bool WM_toolsystem_key_from_context(struct ViewLayer *view_layer,
                                    struct ScrArea *sa,
                                    bToolKey *tkey);

void WM_toolsystem_update_from_context_view3d(struct bContext *C);
void WM_toolsystem_update_from_context(struct bContext *C,
                                       struct WorkSpace *workspace,
                                       struct ViewLayer *view_layer,
                                       struct ScrArea *sa);

bool WM_toolsystem_active_tool_is_brush(const struct bContext *C);

struct IDProperty *WM_toolsystem_ref_properties_ensure_idprops(struct bToolRef *tref);
void WM_toolsystem_ref_properties_ensure_ex(struct bToolRef *tref,
                                            const char *idname,
                                            struct StructRNA *type,
                                            struct PointerRNA *r_ptr);

#define WM_toolsystem_ref_properties_ensure_from_operator(tref, ot, r_ptr) \
  WM_toolsystem_ref_properties_ensure_ex(tref, (ot)->idname, (ot)->srna, r_ptr)
#define WM_toolsystem_ref_properties_ensure_from_gizmo_group(tref, gzgroup, r_ptr) \
  WM_toolsystem_ref_properties_ensure_ex(tref, (gzgroup)->idname, (gzgroup)->srna, r_ptr)

bool WM_toolsystem_ref_properties_get_ex(struct bToolRef *tref,
                                         const char *idname,
                                         struct StructRNA *type,
                                         struct PointerRNA *r_ptr);
#define WM_toolsystem_ref_properties_get_from_operator(tref, ot, r_ptr) \
  WM_toolsystem_ref_properties_get_ex(tref, (ot)->idname, (ot)->srna, r_ptr)
#define WM_toolsystem_ref_properties_get_from_gizmo_group(tref, gzgroup, r_ptr) \
  WM_toolsystem_ref_properties_get_ex(tref, (gzgroup)->idname, (gzgroup)->srna, r_ptr)

void WM_toolsystem_ref_properties_init_for_keymap(struct bToolRef *tref,
                                                  struct PointerRNA *dst_ptr,
                                                  struct PointerRNA *src_ptr,
                                                  struct wmOperatorType *ot);

void WM_toolsystem_refresh_active(struct bContext *C);

void WM_toolsystem_refresh_screen_area(struct WorkSpace *workspace,
                                       struct ViewLayer *view_layer,
                                       struct ScrArea *sa);
void WM_toolsystem_refresh_screen_all(struct Main *bmain);

#ifdef __cplusplus
}
#endif

#endif /* __WM_TOOLSYSTEM_API_H__ */
