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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_SCREEN_H__
#define __ED_SCREEN_H__

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_view3d_types.h"
#include "DNA_workspace_types.h"

#include "DNA_object_enums.h"

#include "BLI_compiler_attrs.h"

struct ARegion;
struct Depsgraph;
struct IDProperty;
struct Main;
struct MenuType;
struct PropertyRNA;
struct Scene;
struct SpaceLink;
struct ViewLayer;
struct WorkSpace;
struct WorkSpaceInstanceHook;
struct bContext;
struct bScreen;
struct rcti;
struct uiBlock;
struct uiLayout;
struct wmEvent;
struct wmKeyConfig;
struct wmMsgBus;
struct wmMsgSubscribeKey;
struct wmMsgSubscribeValue;
struct wmNotifier;
struct wmOperatorType;
struct wmWindow;
struct wmWindowManager;

/* regions */
void ED_region_do_listen(struct wmWindow *win,
                         struct ScrArea *sa,
                         struct ARegion *ar,
                         struct wmNotifier *note,
                         const Scene *scene);
void ED_region_do_layout(struct bContext *C, struct ARegion *ar);
void ED_region_do_draw(struct bContext *C, struct ARegion *ar);
void ED_region_exit(struct bContext *C, struct ARegion *ar);
void ED_region_pixelspace(struct ARegion *ar);
void ED_region_update_rect(struct ARegion *ar);
void ED_region_init(struct ARegion *ar);
void ED_region_tag_redraw(struct ARegion *ar);
void ED_region_tag_redraw_partial(struct ARegion *ar, const struct rcti *rct);
void ED_region_tag_redraw_overlay(struct ARegion *ar);
void ED_region_tag_redraw_no_rebuild(struct ARegion *ar);
void ED_region_tag_refresh_ui(struct ARegion *ar);

void ED_region_panels_init(struct wmWindowManager *wm, struct ARegion *ar);
void ED_region_panels_ex(const struct bContext *C,
                         struct ARegion *ar,
                         const char *contexts[],
                         int contextnr,
                         const bool vertical);
void ED_region_panels(const struct bContext *C, struct ARegion *ar);
void ED_region_panels_layout_ex(const struct bContext *C,
                                struct ARegion *ar,
                                const char *contexts[],
                                int contextnr,
                                const bool vertical);
void ED_region_panels_layout(const struct bContext *C, struct ARegion *ar);
void ED_region_panels_draw(const struct bContext *C, struct ARegion *ar);

void ED_region_header_init(struct ARegion *ar);
void ED_region_header(const struct bContext *C, struct ARegion *ar);
void ED_region_header_layout(const struct bContext *C, struct ARegion *ar);
void ED_region_header_draw(const struct bContext *C, struct ARegion *ar);

void ED_region_cursor_set(struct wmWindow *win, struct ScrArea *sa, struct ARegion *ar);
void ED_region_toggle_hidden(struct bContext *C, struct ARegion *ar);
void ED_region_visibility_change_update(struct bContext *C,
                                        struct ScrArea *sa,
                                        struct ARegion *ar);
/* screen_ops.c */
void ED_region_visibility_change_update_animated(struct bContext *C,
                                                 struct ScrArea *sa,
                                                 struct ARegion *ar);

void ED_region_info_draw(struct ARegion *ar,
                         const char *text,
                         float fill_color[4],
                         const bool full_redraw);
void ED_region_info_draw_multiline(ARegion *ar,
                                   const char *text_array[],
                                   float fill_color[4],
                                   const bool full_redraw);
void ED_region_image_metadata_draw(
    int x, int y, struct ImBuf *ibuf, const rctf *frame, float zoomx, float zoomy);
void ED_region_image_metadata_panel_draw(struct ImBuf *ibuf, struct uiLayout *layout);
void ED_region_grid_draw(struct ARegion *ar, float zoomx, float zoomy);
float ED_region_blend_alpha(struct ARegion *ar);
void ED_region_visible_rect(struct ARegion *ar, struct rcti *rect);
bool ED_region_is_overlap(int spacetype, int regiontype);

int ED_region_snap_size_test(const struct ARegion *ar);
bool ED_region_snap_size_apply(struct ARegion *ar, int snap_flag);

/* message_bus callbacks */
void ED_region_do_msg_notify_tag_redraw(struct bContext *C,
                                        struct wmMsgSubscribeKey *msg_key,
                                        struct wmMsgSubscribeValue *msg_val);
void ED_area_do_msg_notify_tag_refresh(struct bContext *C,
                                       struct wmMsgSubscribeKey *msg_key,
                                       struct wmMsgSubscribeValue *msg_val);

void ED_area_do_mgs_subscribe_for_tool_header(const struct bContext *C,
                                              struct WorkSpace *workspace,
                                              struct Scene *scene,
                                              struct bScreen *screen,
                                              struct ScrArea *sa,
                                              struct ARegion *ar,
                                              struct wmMsgBus *mbus);
void ED_area_do_mgs_subscribe_for_tool_ui(const struct bContext *C,
                                          struct WorkSpace *workspace,
                                          struct Scene *scene,
                                          struct bScreen *screen,
                                          struct ScrArea *sa,
                                          struct ARegion *ar,
                                          struct wmMsgBus *mbus);

/* message bus */
void ED_region_message_subscribe(struct bContext *C,
                                 struct WorkSpace *workspace,
                                 struct Scene *scene,
                                 struct bScreen *screen,
                                 struct ScrArea *sa,
                                 struct ARegion *ar,
                                 struct wmMsgBus *mbus);

/* spaces */
void ED_spacetypes_keymap(struct wmKeyConfig *keyconf);
int ED_area_header_switchbutton(const struct bContext *C, struct uiBlock *block, int yco);

/* areas */
void ED_area_initialize(struct wmWindowManager *wm, struct wmWindow *win, struct ScrArea *sa);
void ED_area_exit(struct bContext *C, struct ScrArea *sa);
int ED_screen_area_active(const struct bContext *C);
void ED_screen_global_areas_refresh(struct wmWindow *win);
void ED_screen_global_areas_sync(struct wmWindow *win);
void ED_area_do_listen(struct wmWindow *win, ScrArea *sa, struct wmNotifier *note, Scene *scene);
void ED_area_tag_redraw(ScrArea *sa);
void ED_area_tag_redraw_no_rebuild(ScrArea *sa);
void ED_area_tag_redraw_regiontype(ScrArea *sa, int type);
void ED_area_tag_refresh(ScrArea *sa);
void ED_area_do_refresh(struct bContext *C, ScrArea *sa);
struct AZone *ED_area_azones_update(ScrArea *sa, const int mouse_xy[]);
void ED_area_status_text(ScrArea *sa, const char *str);
void ED_area_newspace(struct bContext *C, ScrArea *sa, int type, const bool skip_ar_exit);
void ED_area_prevspace(struct bContext *C, ScrArea *sa);
void ED_area_swapspace(struct bContext *C, ScrArea *sa1, ScrArea *sa2);
int ED_area_headersize(void);
int ED_area_header_alignment_or_fallback(const ScrArea *area, int fallback);
int ED_area_header_alignment(const ScrArea *area);
int ED_area_footersize(void);
int ED_area_footer_alignment_or_fallback(const ScrArea *area, int fallback);
int ED_area_footer_alignment(const ScrArea *area);
int ED_area_global_size_y(const ScrArea *area);
int ED_area_global_min_size_y(const ScrArea *area);
int ED_area_global_max_size_y(const ScrArea *area);
bool ED_area_is_global(const ScrArea *area);
int ED_region_global_size_y(void);
void ED_area_update_region_sizes(struct wmWindowManager *wm,
                                 struct wmWindow *win,
                                 struct ScrArea *area);

ScrArea *ED_screen_areas_iter_first(const struct wmWindow *win, const bScreen *screen);
ScrArea *ED_screen_areas_iter_next(const bScreen *screen, const ScrArea *area);
/**
 * Iterate over all areas visible in the screen (screen as in everything
 * visible in the window, not just bScreen).
 * \note Skips global areas with flag GLOBAL_AREA_IS_HIDDEN.
 */
#define ED_screen_areas_iter(win, screen, area_name) \
  for (ScrArea *area_name = ED_screen_areas_iter_first(win, screen); area_name != NULL; \
       area_name = ED_screen_areas_iter_next(screen, area_name))
#define ED_screen_verts_iter(win, screen, vert_name) \
  for (ScrVert *vert_name = (win)->global_areas.vertbase.first ? \
                                (win)->global_areas.vertbase.first : \
                                screen->vertbase.first; \
       vert_name != NULL; \
       vert_name = (vert_name == (win)->global_areas.vertbase.last) ? (screen)->vertbase.first : \
                                                                      vert_name->next)

/* screens */
void ED_screens_initialize(struct Main *bmain, struct wmWindowManager *wm);
void ED_screen_draw_edges(struct wmWindow *win);
void ED_screen_draw_join_shape(struct ScrArea *sa1, struct ScrArea *sa2);
void ED_screen_draw_split_preview(struct ScrArea *sa, const int dir, const float fac);
void ED_screen_refresh(struct wmWindowManager *wm, struct wmWindow *win);
void ED_screen_ensure_updated(struct wmWindowManager *wm,
                              struct wmWindow *win,
                              struct bScreen *screen);
void ED_screen_do_listen(struct bContext *C, struct wmNotifier *note);
bool ED_screen_change(struct bContext *C, struct bScreen *sc);
void ED_screen_scene_change(struct bContext *C, struct wmWindow *win, struct Scene *scene);
void ED_screen_set_active_region(struct bContext *C, struct wmWindow *win, const int xy[2]);
void ED_screen_exit(struct bContext *C, struct wmWindow *window, struct bScreen *screen);
void ED_screen_animation_timer(struct bContext *C, int redraws, int refresh, int sync, int enable);
void ED_screen_animation_timer_update(struct bScreen *screen, int redraws, int refresh);
void ED_screen_restore_temp_type(struct bContext *C, ScrArea *sa);
ScrArea *ED_screen_full_newspace(struct bContext *C, ScrArea *sa, int type);
void ED_screen_full_prevspace(struct bContext *C, ScrArea *sa);
void ED_screen_full_restore(struct bContext *C, ScrArea *sa);
struct ScrArea *ED_screen_state_toggle(struct bContext *C,
                                       struct wmWindow *win,
                                       struct ScrArea *sa,
                                       const short state);
void ED_screens_header_tools_menu_create(struct bContext *C, struct uiLayout *layout, void *arg);
void ED_screens_footer_tools_menu_create(struct bContext *C, struct uiLayout *layout, void *arg);
void ED_screens_navigation_bar_tools_menu_create(struct bContext *C,
                                                 struct uiLayout *layout,
                                                 void *arg);
bool ED_screen_stereo3d_required(const struct bScreen *screen, const struct Scene *scene);
Scene *ED_screen_scene_find(const struct bScreen *screen, const struct wmWindowManager *wm);
Scene *ED_screen_scene_find_with_window(const struct bScreen *screen,
                                        const struct wmWindowManager *wm,
                                        struct wmWindow **r_window);
ScrArea *ED_screen_area_find_with_spacedata(const bScreen *screen,
                                            const struct SpaceLink *sl,
                                            const bool only_visible);
struct wmWindow *ED_screen_window_find(const struct bScreen *screen,
                                       const struct wmWindowManager *wm);
void ED_screen_preview_render(const struct bScreen *screen,
                              int size_x,
                              int size_y,
                              unsigned int *r_rect) ATTR_NONNULL();

/* workspaces */
struct WorkSpace *ED_workspace_add(struct Main *bmain, const char *name) ATTR_NONNULL();
bool ED_workspace_change(struct WorkSpace *workspace_new,
                         struct bContext *C,
                         struct wmWindowManager *wm,
                         struct wmWindow *win) ATTR_NONNULL();
struct WorkSpace *ED_workspace_duplicate(struct WorkSpace *workspace_old,
                                         struct Main *bmain,
                                         struct wmWindow *win);
bool ED_workspace_delete(struct WorkSpace *workspace,
                         struct Main *bmain,
                         struct bContext *C,
                         struct wmWindowManager *wm) ATTR_NONNULL();
void ED_workspace_scene_data_sync(struct WorkSpaceInstanceHook *hook, Scene *scene) ATTR_NONNULL();
struct WorkSpaceLayout *ED_workspace_layout_add(struct Main *bmain,
                                                struct WorkSpace *workspace,
                                                struct wmWindow *win,
                                                const char *name) ATTR_NONNULL();
struct WorkSpaceLayout *ED_workspace_layout_duplicate(struct Main *bmain,
                                                      struct WorkSpace *workspace,
                                                      const struct WorkSpaceLayout *layout_old,
                                                      struct wmWindow *win) ATTR_NONNULL();
bool ED_workspace_layout_delete(struct WorkSpace *workspace,
                                struct WorkSpaceLayout *layout_old,
                                struct bContext *C) ATTR_NONNULL();
bool ED_workspace_layout_cycle(struct WorkSpace *workspace,
                               const short direction,
                               struct bContext *C) ATTR_NONNULL();

void ED_workspace_status_text(struct bContext *C, const char *str);

/* anim */
void ED_update_for_newframe(struct Main *bmain, struct Depsgraph *depsgraph);

void ED_refresh_viewport_fps(struct bContext *C);
int ED_screen_animation_play(struct bContext *C, int sync, int mode);
bScreen *ED_screen_animation_playing(const struct wmWindowManager *wm);
bScreen *ED_screen_animation_no_scrub(const struct wmWindowManager *wm);

/* screen keymaps */
void ED_operatortypes_screen(void);
void ED_keymap_screen(struct wmKeyConfig *keyconf);
/* workspace keymaps */
void ED_operatortypes_workspace(void);

/* operators; context poll callbacks */
bool ED_operator_screenactive(struct bContext *C);
bool ED_operator_screen_mainwinactive(struct bContext *C);
bool ED_operator_areaactive(struct bContext *C);
bool ED_operator_regionactive(struct bContext *C);

bool ED_operator_scene(struct bContext *C);
bool ED_operator_scene_editable(struct bContext *C);
bool ED_operator_objectmode(struct bContext *C);

bool ED_operator_view3d_active(struct bContext *C);
bool ED_operator_region_view3d_active(struct bContext *C);
bool ED_operator_animview_active(struct bContext *C);
bool ED_operator_outliner_active(struct bContext *C);
bool ED_operator_outliner_active_no_editobject(struct bContext *C);
bool ED_operator_file_active(struct bContext *C);
bool ED_operator_action_active(struct bContext *C);
bool ED_operator_buttons_active(struct bContext *C);
bool ED_operator_node_active(struct bContext *C);
bool ED_operator_node_editable(struct bContext *C);
bool ED_operator_graphedit_active(struct bContext *C);
bool ED_operator_sequencer_active(struct bContext *C);
bool ED_operator_sequencer_active_editable(struct bContext *C);
bool ED_operator_image_active(struct bContext *C);
bool ED_operator_nla_active(struct bContext *C);
bool ED_operator_info_active(struct bContext *C);
bool ED_operator_console_active(struct bContext *C);

bool ED_operator_object_active(struct bContext *C);
bool ED_operator_object_active_editable(struct bContext *C);
bool ED_operator_object_active_editable_mesh(struct bContext *C);
bool ED_operator_object_active_editable_font(struct bContext *C);
bool ED_operator_editmesh(struct bContext *C);
bool ED_operator_editmesh_view3d(struct bContext *C);
bool ED_operator_editmesh_region_view3d(struct bContext *C);
bool ED_operator_editmesh_auto_smooth(struct bContext *C);
bool ED_operator_editarmature(struct bContext *C);
bool ED_operator_editcurve(struct bContext *C);
bool ED_operator_editcurve_3d(struct bContext *C);
bool ED_operator_editsurf(struct bContext *C);
bool ED_operator_editsurfcurve(struct bContext *C);
bool ED_operator_editsurfcurve_region_view3d(struct bContext *C);
bool ED_operator_editfont(struct bContext *C);
bool ED_operator_editlattice(struct bContext *C);
bool ED_operator_editmball(struct bContext *C);
bool ED_operator_uvedit(struct bContext *C);
bool ED_operator_uvedit_space_image(struct bContext *C);
bool ED_operator_uvmap(struct bContext *C);
bool ED_operator_posemode_exclusive(struct bContext *C);
bool ED_operator_posemode_context(struct bContext *C);
bool ED_operator_posemode(struct bContext *C);
bool ED_operator_posemode_local(struct bContext *C);
bool ED_operator_mask(struct bContext *C);
bool ED_operator_camera(struct bContext *C);

/* screen_user_menu.c */

bUserMenu **ED_screen_user_menus_find(const struct bContext *C, uint *r_len);
struct bUserMenu *ED_screen_user_menu_ensure(struct bContext *C);

struct bUserMenuItem_Op *ED_screen_user_menu_item_find_operator(struct ListBase *lb,
                                                                const struct wmOperatorType *ot,
                                                                struct IDProperty *prop,
                                                                short opcontext);
struct bUserMenuItem_Menu *ED_screen_user_menu_item_find_menu(struct ListBase *lb,
                                                              const struct MenuType *mt);
struct bUserMenuItem_Prop *ED_screen_user_menu_item_find_prop(struct ListBase *lb,
                                                              const char *context_data_path,
                                                              const char *prop_id,
                                                              int prop_index);

void ED_screen_user_menu_item_add_operator(struct ListBase *lb,
                                           const char *ui_name,
                                           const struct wmOperatorType *ot,
                                           const struct IDProperty *prop,
                                           short opcontext);
void ED_screen_user_menu_item_add_menu(struct ListBase *lb,
                                       const char *ui_name,
                                       const struct MenuType *mt);
void ED_screen_user_menu_item_add_prop(ListBase *lb,
                                       const char *ui_name,
                                       const char *context_data_path,
                                       const char *prop_id,
                                       int prop_index);

void ED_screen_user_menu_item_remove(struct ListBase *lb, struct bUserMenuItem *umi);
void ED_screen_user_menu_register(void);

/* Cache display helpers */

void ED_region_cache_draw_background(const struct ARegion *ar);
void ED_region_cache_draw_curfra_label(const int framenr, const float x, const float y);
void ED_region_cache_draw_cached_segments(const struct ARegion *ar,
                                          const int num_segments,
                                          const int *points,
                                          const int sfra,
                                          const int efra);

/* area_utils.c */
void ED_region_generic_tools_region_message_subscribe(const struct bContext *C,
                                                      struct WorkSpace *workspace,
                                                      struct Scene *scene,
                                                      struct bScreen *screen,
                                                      struct ScrArea *sa,
                                                      struct ARegion *ar,
                                                      struct wmMsgBus *mbus);
int ED_region_generic_tools_region_snap_size(const struct ARegion *ar, int size, int axis);

/* area_query.c */
bool ED_region_overlap_isect_x(const ARegion *ar, const int event_x);
bool ED_region_overlap_isect_y(const ARegion *ar, const int event_y);
bool ED_region_overlap_isect_xy(const ARegion *ar, const int event_xy[2]);
bool ED_region_overlap_isect_x_with_margin(const ARegion *ar, const int event_x, const int margin);
bool ED_region_overlap_isect_y_with_margin(const ARegion *ar, const int event_y, const int margin);
bool ED_region_overlap_isect_xy_with_margin(const ARegion *ar,
                                            const int event_xy[2],
                                            const int margin);

bool ED_region_panel_category_gutter_calc_rect(const ARegion *ar, rcti *r_ar_gutter);
bool ED_region_panel_category_gutter_isect_xy(const ARegion *ar, const int event_xy[2]);

bool ED_region_contains_xy(const struct ARegion *ar, const int event_xy[2]);

/* interface_region_hud.c */
struct ARegionType *ED_area_type_hud(int space_type);
void ED_area_type_hud_clear(struct wmWindowManager *wm, ScrArea *sa_keep);
void ED_area_type_hud_ensure(struct bContext *C, struct ScrArea *sa);

/* default keymaps, bitflags (matches order of evaluation). */
enum {
  ED_KEYMAP_UI = (1 << 1),
  ED_KEYMAP_GIZMO = (1 << 2),
  ED_KEYMAP_TOOL = (1 << 3),
  ED_KEYMAP_VIEW2D = (1 << 4),
  ED_KEYMAP_ANIMATION = (1 << 5),
  ED_KEYMAP_FRAMES = (1 << 6),
  ED_KEYMAP_HEADER = (1 << 7),
  ED_KEYMAP_GPENCIL = (1 << 8),
  ED_KEYMAP_FOOTER = (1 << 9),
};

/* SCREEN_OT_space_context_cycle direction */
enum {
  SPACE_CONTEXT_CYCLE_PREV,
  SPACE_CONTEXT_CYCLE_NEXT,
};

#endif /* __ED_SCREEN_H__ */
