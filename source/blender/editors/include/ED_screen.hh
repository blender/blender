/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_workspace_types.h"

#include "ED_screen_types.hh"

#include "WM_types.hh"

#include "BLI_compiler_attrs.h"

struct ARegion;
struct AZone;
struct Depsgraph;
struct IDProperty;
struct Main;
struct MenuType;
struct Scene;
struct SpaceLink;
struct WorkSpace;
struct WorkSpaceInstanceHook;
struct bContext;
struct bScreen;
struct rcti;
struct uiBlock;
struct uiLayout;
struct wmKeyConfig;
struct wmMsgSubscribeKey;
struct wmMsgSubscribeValue;
struct wmNotifier;
struct wmOperatorType;
struct wmRegionListenerParams;
struct wmRegionMessageSubscribeParams;
struct wmSpaceTypeListenerParams;
struct wmWindow;
struct wmWindowManager;

/* regions */
/** Only exported for WM. */
void ED_region_do_listen(wmRegionListenerParams *params);
/** Only exported for WM. */
void ED_region_do_layout(bContext *C, ARegion *region);
/** Only exported for WM. */
void ED_region_do_draw(bContext *C, ARegion *region);
void ED_region_exit(bContext *C, ARegion *region);
/**
 * Utility to exit and free an area-region. Screen level regions (menus/popups) need to be treated
 * slightly differently, see #ui_region_temp_remove().
 */
void ED_region_remove(bContext *C, ScrArea *area, ARegion *region);
void ED_region_pixelspace(const ARegion *region);
/**
 * Call to move a popup window (keep OpenGL context free!)
 */
void ED_region_update_rect(ARegion *region);
/**
 * Externally called for floating regions like menus.
 */
void ED_region_floating_init(ARegion *region);
void ED_region_tag_redraw(ARegion *region);
void ED_region_tag_redraw_partial(ARegion *region, const rcti *rct, bool rebuild);
void ED_region_tag_redraw_cursor(ARegion *region);
void ED_region_tag_redraw_no_rebuild(ARegion *region);
void ED_region_tag_refresh_ui(ARegion *region);
/**
 * Tag editor overlays to be redrawn. If in doubt about which parts need to be redrawn (partial
 * clipping rectangle set), redraw everything.
 */
void ED_region_tag_redraw_editor_overlays(ARegion *region);

/**
 * If the region has tag RGN_FLAG_INDICATE_OVERFLOW then draw
 * a line or gradient on edges if there is content overflowing.
 */
void ED_region_draw_overflow_indication(const ScrArea *area,
                                        const ARegion *region,
                                        const rcti *mask = nullptr);

/**
 * Set the temporary update flag for property search.
 */
void ED_region_search_filter_update(const ScrArea *area, ARegion *region);
/**
 * Returns the search string if the space type and region type support property search.
 */
const char *ED_area_region_search_filter_get(const ScrArea *area, const ARegion *region);
/**
 * Returns the maximum size a region can grow to so it still fits in the area.
 */
int ED_area_max_regionsize(const ScrArea *area, const ARegion *scale_region, const AZEdge edge);

void ED_region_panels_init(wmWindowManager *wm, ARegion *region);
void ED_region_panels_ex(const bContext *C,
                         ARegion *region,
                         blender::wm::OpCallContext op_context,
                         const char *contexts[]);
void ED_region_panels(const bContext *C, ARegion *region);
/**
 * \param contexts: A NULL terminated array of context strings to match against.
 * Matching against any of these strings will draw the panel.
 * Can be NULL to skip context checks.
 */
void ED_region_panels_layout_ex(const bContext *C,
                                ARegion *region,
                                ListBase *paneltypes,
                                blender::wm::OpCallContext op_context,
                                const char *contexts[],
                                const char *category_override);
/**
 * Build the same panel list as #ED_region_panels_layout_ex and checks whether any
 * of the panels contain a search result based on the area / region's search filter.
 */
bool ED_region_property_search(const bContext *C,
                               ARegion *region,
                               ListBase *paneltypes,
                               const char *contexts[],
                               const char *category_override);

void ED_region_panels_layout(const bContext *C, ARegion *region);
void ED_region_panels_draw(const bContext *C, ARegion *region);

void ED_region_header_init(ARegion *region);
void ED_region_header(const bContext *C, ARegion *region);
void ED_region_header_layout(const bContext *C, ARegion *region);
void ED_region_header_draw(const bContext *C, ARegion *region);
/* Forward declare enum. */
enum class uiButtonSectionsAlign : int8_t;
/** Version of #ED_region_header() that draws with button sections. */
void ED_region_header_with_button_sections(const bContext *C,
                                           ARegion *region,
                                           uiButtonSectionsAlign align);
/** Version of #ED_region_header_draw() that draws with button sections. */
void ED_region_header_draw_with_button_sections(const bContext *C,
                                                const ARegion *region,
                                                uiButtonSectionsAlign align);

void ED_region_cursor_set(wmWindow *win, ScrArea *area, ARegion *region);
/**
 * Exported to all editors, uses fading default.
 */
void ED_region_toggle_hidden(bContext *C, ARegion *region);
/**
 * For use after changing visibility of regions.
 */
void ED_region_visibility_change_update_ex(
    bContext *C, ScrArea *area, ARegion *region, bool is_hidden, bool do_init);
void ED_region_visibility_change_update(bContext *C, ScrArea *area, ARegion *region);
/* `screen_ops.cc` */

/**
 * \note Assumes that \a region itself is not a split version from previous region.
 */
void ED_region_visibility_change_update_animated(bContext *C, ScrArea *area, ARegion *region);

void ED_region_clear(const bContext *C, const ARegion *region, int /*ThemeColorID*/ colorid);

void ED_region_info_draw(ARegion *region,
                         const char *text,
                         const float fill_color[4],
                         bool full_redraw);
void ED_region_info_draw_multiline(ARegion *region,
                                   const char *text_array[],
                                   const float fill_color[4],
                                   bool full_redraw);
void ED_region_image_metadata_panel_draw(ImBuf *ibuf, uiLayout *layout);
void ED_region_grid_draw(ARegion *region, float zoomx, float zoomy, float x0, float y0);
float ED_region_blend_alpha(ARegion *region);
const rcti *ED_region_visible_rect(ARegion *region);
/**
 * Overlapping regions only in the following restricted cases.
 */
bool ED_region_is_overlap(int spacetype, int regiontype);

int ED_region_snap_size_test(const ARegion *region);
bool ED_region_snap_size_apply(ARegion *region, int snap_flag);

/* message_bus callbacks */
void ED_region_do_msg_notify_tag_redraw(bContext *C,
                                        wmMsgSubscribeKey *msg_key,
                                        wmMsgSubscribeValue *msg_val);
void ED_area_do_msg_notify_tag_refresh(bContext *C,
                                       wmMsgSubscribeKey *msg_key,
                                       wmMsgSubscribeValue *msg_val);

/**
 * Follow #ARegionType.message_subscribe.
 */
void ED_area_do_mgs_subscribe_for_tool_header(const wmRegionMessageSubscribeParams *params);
void ED_area_do_mgs_subscribe_for_tool_ui(const wmRegionMessageSubscribeParams *params);

/* message bus */

/**
 * Generate subscriptions for this region.
 */
void ED_region_message_subscribe(wmRegionMessageSubscribeParams *params);

/* spaces */

/**
 * \note Keymap definitions are registered only once per WM initialize,
 * usually on file read, using the keymap the actual areas/regions add the handlers.
 * \note Called in `wm.cc`. */
void ED_spacetypes_keymap(wmKeyConfig *keyconf);
/**
 * Returns offset for next button in header.
 */
int ED_area_header_switchbutton(const bContext *C, uiBlock *block, int yco);

/* areas */
/**
 * Ensure #ScrArea.type and #ARegion.type are set and valid.
 */
void ED_area_and_region_types_init(ScrArea *area);
/**
 * Called in screen_refresh, or screens_init, also area size changes.
 */
void ED_area_init(bContext *C, const wmWindow *win, ScrArea *area);
void ED_area_exit(bContext *C, ScrArea *area);
blender::StringRefNull ED_area_name(const ScrArea *area);
int ED_area_icon(const ScrArea *area);
int ED_screen_area_active(const bContext *C);
void ED_screen_global_areas_refresh(wmWindow *win);
void ED_screen_global_areas_sync(wmWindow *win);
/** Only exported for WM. */
void ED_area_do_listen(wmSpaceTypeListenerParams *params);
void ED_area_tag_redraw(ScrArea *area);
void ED_area_tag_redraw_no_rebuild(ScrArea *area);
void ED_area_tag_redraw_regiontype(ScrArea *area, int regiontype);
void ED_area_tag_refresh(ScrArea *area);
/**
 * For regions that change the region size in their #ARegionType.layout() callback: Mark the area
 * as having a changed region size, requiring refitting of regions within the area.
 */
void ED_area_tag_region_size_update(ScrArea *area, ARegion *changed_region);
/**
 * Only exported for WM.
 */
void ED_area_do_refresh(bContext *C, ScrArea *area);
AZone *ED_area_azones_update(ScrArea *area, const int mouse_xy[2]);
/**
 * Show the given text in the area's header, instead of its regular contents.
 * Use NULL to disable this and show the regular header contents again.
 */
void ED_area_status_text(ScrArea *area, const char *str);
/**
 * \param skip_region_exit: Skip calling area exit callback. Set for opening temp spaces.
 */
void ED_area_newspace(bContext *C, ScrArea *area, int type, bool skip_region_exit);
void ED_area_prevspace(bContext *C, ScrArea *area);
void ED_area_swapspace(bContext *C, ScrArea *sa1, ScrArea *sa2);
int ED_area_headersize();
int ED_area_footersize();
/**
 * \return the final height of a global \a area, accounting for DPI.
 */
int ED_area_global_size_y(const ScrArea *area);
int ED_area_global_min_size_y(const ScrArea *area);
int ED_area_global_max_size_y(const ScrArea *area);
bool ED_area_is_global(const ScrArea *area);
/**
 * For now we just assume all global areas are made up out of horizontal bars
 * with the same size. A fixed size could be stored in ARegion instead if needed.
 *
 * \return the DPI aware height of a single bar/region in global areas.
 */
int ED_region_global_size_y();
void ED_area_update_region_sizes(wmWindowManager *wm, wmWindow *win, ScrArea *area);
bool ED_area_has_shared_border(ScrArea *a, ScrArea *b);
ScrArea *ED_area_offscreen_create(wmWindow *win, eSpace_Type space_type);
void ED_area_offscreen_free(wmWindowManager *wm, wmWindow *win, ScrArea *area);

/**
 * Search all screens, even non-active or overlapping (multiple windows), return the most-likely
 * area of interest. xy is relative to active window, like all similar functions.
 */
ScrArea *ED_area_find_under_cursor(const bContext *C, int spacetype, const int event_xy[2]);

ScrArea *ED_screen_areas_iter_first(const wmWindow *win, const bScreen *screen);
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
                                (ScrVert *)(win)->global_areas.vertbase.first : \
                                (ScrVert *)(screen)->vertbase.first; \
       vert_name != NULL; \
       vert_name = (vert_name == (win)->global_areas.vertbase.last) ? \
                       (ScrVert *)(screen)->vertbase.first : \
                       vert_name->next)

/**
 * Update all areas that are supposed to follow the timeline current-frame indicator.
 */
void ED_areas_do_frame_follow(bContext *C, bool center_view);

/* screens */

/**
 * File read, set all screens, ....
 */
void ED_screens_init(bContext *C, Main *bmain, wmWindowManager *wm);
/**
 * Only for edge lines between areas.
 */
void ED_screen_draw_edges(wmWindow *win);

/**
 * Make this screen usable.
 * for file read and first use, for scaling window, area moves.
 */
void ED_screen_refresh(bContext *C, wmWindowManager *wm, wmWindow *win);
void ED_screen_ensure_updated(bContext *C, wmWindowManager *wm, wmWindow *win);
void ED_screen_do_listen(bContext *C, const wmNotifier *note);
/**
 * \brief Change the active screen.
 *
 * Operator call, WM + Window + screen already existed before
 *
 * \warning Do NOT call in area/region queues!
 * \returns if screen changing was successful.
 */
bool ED_screen_change(bContext *C, bScreen *screen);
void ED_screen_scene_change(bContext *C, wmWindow *win, Scene *scene, bool refresh_toolsystem);
/**
 * Called in `wm_event_system.cc`. sets state vars in screen, cursors.
 * event type is mouse move.
 */
void ED_screen_set_active_region(bContext *C, wmWindow *win, const int xy[2]);
void ED_screen_exit(bContext *C, wmWindow *window, bScreen *screen);
/**
 * redraws: uses defines from `stime->redraws`
 * \param enable: 1 - forward on, -1 - backwards on, 0 - off.
 */
void ED_screen_animation_timer(
    bContext *C, Scene *scene, ViewLayer *view_layer, int redraws, int sync, int enable);
void ED_screen_animation_timer_update(bScreen *screen, int redraws);
void ED_screen_restore_temp_type(bContext *C, ScrArea *area);
ScrArea *ED_screen_full_newspace(bContext *C, ScrArea *area, int type);
/**
 * \a was_prev_temp for the case previous space was a temporary full-screen as well
 */
void ED_screen_full_prevspace(bContext *C, ScrArea *area);
/**
 * Restore a screen / area back to default operation, after temp full-screen modes.
 */
void ED_screen_full_restore(bContext *C, ScrArea *area);
/**
 * Create a new temporary screen with a maximized, empty area.
 * This can be closed with #ED_screen_state_toggle().
 *
 * Use this to just create a new maximized screen/area, rather than maximizing an existing one.
 * Otherwise, maximize with #ED_screen_state_toggle().
 */
bScreen *ED_screen_state_maximized_create(bContext *C);
/**
 * This function toggles: if area is maximized/full then the parent will be restored.
 *
 * Use #ED_screen_state_maximized_create() if you do not want the toggle behavior when changing to
 * a maximized area. I.e. if you just want to open a new maximized screen/area, not maximize a
 * specific area. In the former case, space data of the maximized and non-maximized area should be
 * independent, in the latter it should be the same.
 *
 * \warning \a area may be freed.
 */
ScrArea *ED_screen_state_toggle(bContext *C, wmWindow *win, ScrArea *area, short state);
/**
 * Wrapper to open a temporary space either as full-screen space, or as separate window,
 * as defined by \a display_type.
 *
 * \param title: Title to set for the window, if a window is spawned.
 * \param rect_unscaled: Position & size of the window, if a window is spawned.
 */
ScrArea *ED_screen_temp_space_open(bContext *C,
                                   const char *title,
                                   eSpace_Type space_type,
                                   int display_type,
                                   bool dialog) ATTR_NONNULL(1);
void ED_screens_header_tools_menu_create(bContext *C, uiLayout *layout, void *arg);
void ED_screens_footer_tools_menu_create(bContext *C, uiLayout *layout, void *arg);
void ED_screens_region_flip_menu_create(bContext *C, uiLayout *layout, void *arg);
/**
 * \return true if any active area requires to see in 3D.
 */
bool ED_screen_stereo3d_required(const bScreen *screen, const Scene *scene);
Scene *ED_screen_scene_find(const bScreen *screen, const wmWindowManager *wm);
/**
 * Find the scene displayed in \a screen.
 * \note Assumes \a screen to be visible/active!
 */
Scene *ED_screen_scene_find_with_window(const bScreen *screen,
                                        const wmWindowManager *wm,
                                        wmWindow **r_window);
ScrArea *ED_screen_area_find_with_spacedata(const bScreen *screen,
                                            const SpaceLink *sl,
                                            bool only_visible);
wmWindow *ED_screen_window_find(const bScreen *screen, const wmWindowManager *wm);

/* workspaces */

WorkSpace *ED_workspace_add(Main *bmain, const char *name) ATTR_NONNULL();
/**
 * \brief Change the active workspace.
 *
 * Operator call, WM + Window + screen already existed before
 * Pretty similar to #ED_screen_change since changing workspace also changes screen.
 *
 * \warning Do NOT call in area/region queues!
 * \returns if workspace changing was successful.
 */
bool ED_workspace_change(WorkSpace *workspace_new, bContext *C, wmWindowManager *wm, wmWindow *win)
    ATTR_NONNULL();
/**
 * Duplicate a workspace including its layouts. Does not activate the workspace, but
 * it stores the screen-layout to be activated (BKE_workspace_temp_layout_store)
 */
WorkSpace *ED_workspace_duplicate(WorkSpace *workspace_old, Main *bmain, wmWindow *win);
/**
 * \return if succeeded.
 */
bool ED_workspace_delete(WorkSpace *workspace, Main *bmain, bContext *C, wmWindowManager *wm)
    ATTR_NONNULL();
/**
 * Some editor data may need to be synced with scene data (3D View camera and layers).
 * This function ensures data is synced for editors in active layout of \a workspace.
 */
void ED_workspace_scene_data_sync(WorkSpaceInstanceHook *hook, Scene *scene) ATTR_NONNULL();
/**
 * Make sure there is a non-full-screen layout to switch to that isn't used yet by an other window.
 * Needed for workspace or screen switching to ensure valid screens.
 *
 * \param layout_fallback_base: As last resort, this layout is duplicated and returned.
 */
WorkSpaceLayout *ED_workspace_screen_change_ensure_unused_layout(
    Main *bmain,
    WorkSpace *workspace,
    WorkSpaceLayout *layout_new,
    const WorkSpaceLayout *layout_fallback_base,
    wmWindow *win) ATTR_NONNULL();
/**
 * Empty screen, with 1 dummy area without space-data. Uses window size.
 */
WorkSpaceLayout *ED_workspace_layout_add(Main *bmain,
                                         WorkSpace *workspace,
                                         wmWindow *win,
                                         const char *name) ATTR_NONNULL();
WorkSpaceLayout *ED_workspace_layout_duplicate(Main *bmain,
                                               WorkSpace *workspace,
                                               const WorkSpaceLayout *layout_old,
                                               wmWindow *win) ATTR_NONNULL();
/**
 * \warning Only call outside of area/region loops!
 * \return true if succeeded.
 */
bool ED_workspace_layout_delete(WorkSpace *workspace, WorkSpaceLayout *layout_old, bContext *C)
    ATTR_NONNULL();
bool ED_workspace_layout_cycle(WorkSpace *workspace, short direction, bContext *C) ATTR_NONNULL();

void ED_workspace_status_text(bContext *C, const char *str);

class WorkspaceStatus {
  WorkSpace *workspace_;
  wmWindowManager *wm_;

 public:
  WorkspaceStatus(bContext *C);

  /**
   * Add a static status entry and up to two icons.
   *
   * Example:
   *   [LMB][Enter] Confirm
   */
  void item(std::string text, int icon1, int icon2 = 0);

  /**
   * Add extra (or negative) space between items.
   */
  void separator(float factor = 1.0f);

  /**
   * Add a dynamic status entry with up to two icons that change appearance.
   * Example:
   *   [CTRL] Tweak
   */
  void item_bool(std::string text, bool inverted, int icon1, int icon2 = 0);

  /**
   * Add a static status entry showing two icons separated by a dash.
   * Example:
   *   [A]-[Z] Search
   */
  void range(std::string text, int icon1, int icon2);

  /**
   * Add a dynamic status entry for a given property in an operator's keymap.
   * Example:
   *   [V] X-Ray
   */
  void opmodal(std::string text, const wmOperatorType *ot, int propvalue, bool inverted = false);
};

void ED_workspace_do_listen(bContext *C, const wmNotifier *note);

/* anim */
/**
 * Results in fully updated anim system.
 */
void ED_update_for_newframe(Main *bmain, Depsgraph *depsgraph);

/**
 * Toggle operator.
 */
void ED_reset_audio_device(bContext *C);
wmOperatorStatus ED_screen_animation_play(bContext *C, int sync, int mode);
/**
 * Find window that owns the animation timer.
 */
bScreen *ED_screen_animation_playing(const wmWindowManager *wm);
bScreen *ED_screen_animation_no_scrub(const wmWindowManager *wm);

/* screen keymaps */
/* called in `spacetypes.cc`. */
void ED_operatortypes_screen();
/* called in `spacetypes.cc`. */
void ED_keymap_screen(wmKeyConfig *keyconf);
/**
 * Workspace key-maps.
 */
void ED_operatortypes_workspace();

/* operators; context poll callbacks */

bool ED_operator_screenactive(bContext *C);
bool ED_operator_screenactive_nobackground(bContext *C);
/**
 * When mouse is over area-edge.
 */
bool ED_operator_screen_mainwinactive(bContext *C);
bool ED_operator_areaactive(bContext *C);
bool ED_operator_regionactive(bContext *C);

bool ED_operator_scene(bContext *C);
bool ED_operator_scene_editable(bContext *C);
bool ED_operator_sequencer_scene(bContext *C);
bool ED_operator_sequencer_scene_editable(bContext *C);

bool ED_operator_objectmode(bContext *C);
/**
 * Same as #ED_operator_objectmode() but additionally sets a "disabled hint". That is, a message
 * to be displayed to the user explaining why the operator can't be used in current context.
 */
bool ED_operator_objectmode_poll_msg(bContext *C);
bool ED_operator_objectmode_with_view3d_poll_msg(bContext *C);

bool ED_operator_view3d_active(bContext *C);
bool ED_operator_region_view3d_active(bContext *C);
bool ED_operator_region_gizmo_active(bContext *C);

/**
 * Generic for any view2d which uses anim_ops.
 */
bool ED_operator_animview_active(bContext *C);
bool ED_operator_outliner_active(bContext *C);
bool ED_operator_region_outliner_active(bContext *C);
bool ED_operator_outliner_active_no_editobject(bContext *C);
/**
 * \note Will return true for file spaces in either file or asset browsing mode! See
 * #ED_operator_file_browsing_active() (file browsing only) and
 * #ED_operator_asset_browsing_active() (asset browsing only).
 */
bool ED_operator_file_active(bContext *C);
/**
 * \note Will only return true if the file space is in file browsing mode, not asset browsing! See
 * #ED_operator_file_active() (file or asset browsing) and
 * #ED_operator_asset_browsing_active() (asset browsing only).
 */
bool ED_operator_file_browsing_active(bContext *C);
bool ED_operator_asset_browsing_active(bContext *C);
bool ED_operator_spreadsheet_active(bContext *C);
bool ED_operator_action_active(bContext *C);
bool ED_operator_buttons_active(bContext *C);
bool ED_operator_node_active(bContext *C);
bool ED_operator_node_editable(bContext *C);
bool ED_operator_graphedit_active(bContext *C);
bool ED_operator_sequencer_active(bContext *C);
bool ED_operator_sequencer_active_editable(bContext *C);
bool ED_operator_image_active(bContext *C);
bool ED_operator_nla_active(bContext *C);
bool ED_operator_info_active(bContext *C);
bool ED_operator_console_active(bContext *C);

/** Only check there is an active object (no visibility check). */
bool ED_operator_object_active_only(bContext *C);
bool ED_operator_object_active(bContext *C);
bool ED_operator_object_active_editable_ex(bContext *C, const Object *ob);
bool ED_operator_object_active_editable(bContext *C);
/**
 * Object must be editable and fully local (i.e. not an override).
 */
bool ED_operator_object_active_local_editable_ex(bContext *C, const Object *ob);
bool ED_operator_object_active_local_editable(bContext *C);
bool ED_operator_object_active_editable_mesh(bContext *C);
bool ED_operator_object_active_editable_font(bContext *C);
bool ED_operator_editable_mesh(bContext *C);
bool ED_operator_editmesh(bContext *C);
bool ED_operator_editmesh_view3d(bContext *C);
bool ED_operator_editmesh_region_view3d(bContext *C);
bool ED_operator_editarmature(bContext *C);
bool ED_operator_editcurve(bContext *C);
bool ED_operator_editcurve_3d(bContext *C);
bool ED_operator_editsurf(bContext *C);
bool ED_operator_editsurfcurve(bContext *C);
bool ED_operator_editsurfcurve_region_view3d(bContext *C);
bool ED_operator_editfont(bContext *C);
bool ED_operator_editlattice(bContext *C);
bool ED_operator_editmball(bContext *C);
/**
 * Wrapper for #ED_space_image_show_uvedit.
 */
bool ED_operator_uvedit(bContext *C);
bool ED_operator_uvedit_space_image(bContext *C);
bool ED_operator_uvmap(bContext *C);
bool ED_operator_posemode_exclusive(bContext *C);
/**
 * Object must be editable, fully local (i.e. not an override), and exclusively in Pose mode.
 */
bool ED_operator_object_active_local_editable_posemode_exclusive(bContext *C);
/**
 * Allows for pinned pose objects to be used in the object buttons
 * and the non-active pose object to be used in the 3D view.
 */
bool ED_operator_posemode_context(bContext *C);
bool ED_operator_posemode(bContext *C);
bool ED_operator_posemode_local(bContext *C);
bool ED_operator_camera_poll(bContext *C);

/* `screen_user_menu.cc` */

bUserMenu **ED_screen_user_menus_find(const bContext *C, uint *r_len);
bUserMenu *ED_screen_user_menu_ensure(bContext *C);

/**
 * Finds a menu item associated with an operator in user menus (aka Quick Favorites)
 *
 * \param op_prop_enum: name of an operator property when the operator is called with an enum (to
 * be an empty string otherwise)
 */
bUserMenuItem_Op *ED_screen_user_menu_item_find_operator(ListBase *lb,
                                                         const wmOperatorType *ot,
                                                         IDProperty *prop,
                                                         const char *op_prop_enum,
                                                         blender::wm::OpCallContext opcontext);
bUserMenuItem_Menu *ED_screen_user_menu_item_find_menu(ListBase *lb, const MenuType *mt);
bUserMenuItem_Prop *ED_screen_user_menu_item_find_prop(ListBase *lb,
                                                       const char *context_data_path,
                                                       const char *prop_id,
                                                       int prop_index);

void ED_screen_user_menu_item_add_operator(ListBase *lb,
                                           const char *ui_name,
                                           const wmOperatorType *ot,
                                           const IDProperty *prop,
                                           const char *op_prop_enum,
                                           blender::wm::OpCallContext opcontext);
void ED_screen_user_menu_item_add_menu(ListBase *lb, const char *ui_name, const MenuType *mt);
void ED_screen_user_menu_item_add_prop(ListBase *lb,
                                       const char *ui_name,
                                       const char *context_data_path,
                                       const char *prop_id,
                                       int prop_index);

void ED_screen_user_menu_item_remove(ListBase *lb, bUserMenuItem *umi);
void ED_screen_user_menu_register();

/* Cache display helpers */

void ED_region_cache_draw_background(ARegion *region);
void ED_region_cache_draw_curfra_label(int framenr, float x, float y);
void ED_region_cache_draw_cached_segments(
    ARegion *region, int num_segments, const int *points, int sfra, int efra);

/* `area_utils.cc` */

/**
 * Callback for #ARegionType.message_subscribe
 */
void ED_region_generic_tools_region_message_subscribe(
    const wmRegionMessageSubscribeParams *params);
/**
 * Callback for #ARegionType.snap_size
 */
int ED_region_generic_tools_region_snap_size(const ARegion *region, int size, int axis);
int ED_region_generic_panel_region_snap_size(const ARegion *region, int size, int axis);

/* `area_query.cc` */

bool ED_region_overlap_isect_x(const ARegion *region, int event_x);
bool ED_region_overlap_isect_y(const ARegion *region, int event_y);
bool ED_region_overlap_isect_xy(const ARegion *region, const int event_xy[2]);
bool ED_region_overlap_isect_any_xy(const ScrArea *area, const int event_xy[2]);
bool ED_region_overlap_isect_x_with_margin(const ARegion *region, int event_x, int margin);
bool ED_region_overlap_isect_y_with_margin(const ARegion *region, int event_y, int margin);
bool ED_region_overlap_isect_xy_with_margin(const ARegion *region,
                                            const int event_xy[2],
                                            int margin);

bool ED_region_panel_category_gutter_calc_rect(const ARegion *region, rcti *r_region_gutter);
bool ED_region_panel_category_gutter_isect_xy(const ARegion *region, const int event_xy[2]);

/**
 * \note This may return true for multiple overlapping regions.
 * If it matters, check overlapped regions first (#ARegion.overlap).
 */
bool ED_region_contains_xy(const ARegion *region, const int event_xy[2]);
/**
 * Similar to #BKE_area_find_region_xy() but when \a event_xy intersects an overlapping region,
 * this returns the region that is visually under the cursor. E.g. when over the
 * transparent part of the region, it returns the region underneath.
 *
 * The overlapping region is determined using the #ED_region_contains_xy() query.
 */
ARegion *ED_area_find_region_xy_visual(const ScrArea *area, int regiontype, const int event_xy[2]);

/* `interface_region_hud.cc` */

ARegionType *ED_area_type_hud(int space_type);
void ED_area_type_hud_clear(wmWindowManager *wm, ScrArea *area_keep);
void ED_area_type_hud_ensure(bContext *C, ScrArea *area);
/**
 * Lookup the region the operation was executed in, and which should be used to redo the
 * operation. The lookup is based on the region type, so it can return a different region when the
 * same region type is present multiple times.
 */
ARegion *ED_area_type_hud_redo_region_find(const ScrArea *area, const ARegion *hud_region);

/**
 * Default key-maps, bit-flags (matches order of evaluation).
 */
enum {
  ED_KEYMAP_UI = (1 << 1),
  ED_KEYMAP_GIZMO = (1 << 2),
  ED_KEYMAP_TOOL = (1 << 3),
  ED_KEYMAP_VIEW2D = (1 << 4),
  ED_KEYMAP_ANIMATION = (1 << 6),
  ED_KEYMAP_FRAMES = (1 << 7),
  ED_KEYMAP_HEADER = (1 << 8),
  ED_KEYMAP_FOOTER = (1 << 9),
  ED_KEYMAP_GPENCIL = (1 << 10),
  ED_KEYMAP_NAVBAR = (1 << 11),
  ED_KEYMAP_ASSET_SHELF = (1 << 12),
};

/** #SCREEN_OT_space_context_cycle direction. */
enum eScreenCycle {
  SPACE_CONTEXT_CYCLE_PREV,
  SPACE_CONTEXT_CYCLE_NEXT,
};
