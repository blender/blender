/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include <memory>
#include <string>

#include "BLI_compiler_attrs.h"
#include "BLI_enum_flags.hh"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_vec_types.h"

#include "RNA_types.hh"

#include "BKE_context.hh"

namespace blender::bke::id {
class IDRemapper;
}

namespace blender::asset_system {
class AssetRepresentation;
}

struct ARegion;
struct AssetShelfType;
struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct Header;
struct ID;
struct LayoutPanelState;
struct LibraryForeachIDData;
struct ListBase;
struct Menu;
struct Panel;
struct Scene;
struct ScrArea;
struct ScrAreaMap;
struct ScrEdge;
struct ScrVert;
struct SpaceType;
struct View3D;
struct View3DShading;
struct WorkSpace;
struct bContext;
struct bScreen;
struct uiBlock;
struct uiLayout;
struct uiList;
struct wmDrawBuffer;
struct wmGizmoMap;
struct wmKeyConfig;
struct wmMsgBus;
struct wmNotifier;
struct wmTimer;
struct wmWindow;
struct wmWindowManager;

/* spacetype has everything stored to get an editor working, it gets initialized via
 * #ED_spacetypes_init() in `editors/space_api/spacetypes.cc` */
/* an editor in Blender is a combined ScrArea + SpaceType + SpaceData. */

#define BKE_ST_MAXNAME 64

struct wmSpaceTypeListenerParams {
  wmWindow *window;
  ScrArea *area;
  const wmNotifier *notifier;
  const Scene *scene;
};

struct SpaceType {
  /** For menus. */
  char name[BKE_ST_MAXNAME];
  /** Unique space identifier. */
  int spaceid;
  /** Icon lookup for menus. */
  int iconid;

  /**
   * Initial allocation, after this WM will call init() too.
   * Some editors need area and scene data (e.g. frame range) to set their initial scrolling.
   */
  SpaceLink *(*create)(const ScrArea *area, const Scene *scene);
  /** Not free spacelink itself. */
  void (*free)(SpaceLink *sl);

  /** Init is to cope with file load, screen (size) changes, check handlers. */
  void (*init)(wmWindowManager *wm, ScrArea *area);
  /** Exit is called when the area is hidden or removed. */
  void (*exit)(wmWindowManager *wm, ScrArea *area);
  /** Listeners can react to bContext changes. */
  void (*listener)(const wmSpaceTypeListenerParams *params);

  /** Called when the mouse moves out of the area. */
  void (*deactivate)(ScrArea *area);

  /** Refresh context, called after file-reads, #ED_area_tag_refresh(). */
  void (*refresh)(const bContext *C, ScrArea *area);

  /** After a spacedata copy, an init should result in exact same situation. */
  SpaceLink *(*duplicate)(SpaceLink *sl);

  /** Register operator types on startup. */
  void (*operatortypes)();
  /** Add default items to WM keymap. */
  void (*keymap)(wmKeyConfig *keyconf);
  /** On startup, define dropboxes for spacetype+regions. */
  void (*dropboxes)();

  /** Initialize gizmo-map-types and gizmo-group-types with the region. */
  void (*gizmos)();

  /** Return context data. */
  bContextDataCallback context;

  /** Used when we want to replace an ID by another (or NULL). */
  void (*id_remap)(ScrArea *area, SpaceLink *sl, const blender::bke::id::IDRemapper &mappings);

  /**
   * foreach_id callback to process all ID pointers of the editor. Used indirectly by lib_query's
   * #BKE_library_foreach_ID_link when #IDWALK_INCLUDE_UI bit-flag is set (through WM's foreach_id
   * usage of #BKE_screen_foreach_id_screen_area).
   */
  void (*foreach_id)(SpaceLink *space_link, LibraryForeachIDData *data);

  int (*space_subtype_get)(ScrArea *area);
  void (*space_subtype_set)(ScrArea *area, int value);
  void (*space_subtype_item_extend)(bContext *C, EnumPropertyItem **item, int *totitem);

  /** Return a custom name, based on subtype or other reason. */
  blender::StringRefNull (*space_name_get)(const ScrArea *area);
  /** Return a custom icon, based on subtype or other reason. */
  int (*space_icon_get)(const ScrArea *area);

  /**
   * Update pointers for all structs directly owned by this space.
   */
  void (*blend_read_data)(BlendDataReader *reader, SpaceLink *space_link);

  /**
   * Update pointers to other id data blocks.
   */
  void (*blend_read_after_liblink)(BlendLibReader *reader, ID *parent_id, SpaceLink *space_link);

  /**
   * Write all structs that should be saved in a .blend file.
   */
  void (*blend_write)(BlendWriter *writer, SpaceLink *space_link);

  /** Region type definitions. */
  ListBase regiontypes;

  /* read and write... */

  /** Default key-maps to add. */
  int keymapflag;

  ~SpaceType();
};

/* Region types are also defined using spacetypes_init, via a callback. */

struct wmRegionListenerParams {
  wmWindow *window;
  /** Can be NULL when the region is not part of an area. */
  ScrArea *area;
  ARegion *region;
  const wmNotifier *notifier;
  const Scene *scene;
};

struct wmRegionMessageSubscribeParams {
  const bContext *context;
  wmMsgBus *message_bus;
  WorkSpace *workspace;
  Scene *scene;
  bScreen *screen;
  ScrArea *area;
  ARegion *region;
};

struct RegionPollParams {
  const bScreen *screen;
  const ScrArea *area;
  const ARegion *region;

  /** Full context, if WM context above is not enough. */
  const bContext *context;
};

/* #ARegionType::lock */
enum ARegionDrawLockFlags {
  REGION_DRAW_LOCK_NONE = 0,
  REGION_DRAW_LOCK_RENDER = (1 << 0),
  REGION_DRAW_LOCK_BAKING = (1 << 1),
  REGION_DRAW_LOCK_ALL = (REGION_DRAW_LOCK_RENDER | REGION_DRAW_LOCK_BAKING)
};

struct ARegionType {
  ARegionType *next, *prev;
  /** Unique identifier within this space, defines `RGN_TYPE_xxxx`. */
  int regionid;

  /** Add handlers, stuff you only do once or on area/region type/size changes. */
  void (*init)(wmWindowManager *wm, ARegion *region);
  /** Exit is called when the region is hidden or removed. */
  void (*exit)(wmWindowManager *wm, ARegion *region);
  /**
   * Optional callback to decide whether the region should be treated as existing given the
   * current context. When returning false, the region will be kept in storage, but is not
   * available to the user in any way. Callbacks can assume that context has the owning area and
   * space-data set.
   */
  bool (*poll)(const RegionPollParams *params);
  /** Draw entirely, view changes should be handled here. */
  void (*draw)(const bContext *C, ARegion *region);
  /**
   * Handler to draw overlays. This handler is called every draw loop.
   *
   * \note Some editors should return early if the interface is locked
   * (check with #CTX_wm_interface_locked) to avoid accessing scene data
   * that another thread may be modifying
   */
  void (*draw_overlay)(const bContext *C, ARegion *region);
  /** Optional, compute button layout before drawing for dynamic size. */
  void (*layout)(const bContext *C, ARegion *region);
  /** Snap the size of the region (can be NULL for no snapping). */
  int (*snap_size)(const ARegion *region, int size, int axis);
  /** Contextual changes should be handled here. */
  void (*listener)(const wmRegionListenerParams *params);
  /** Optional callback to generate subscriptions. */
  void (*message_subscribe)(const wmRegionMessageSubscribeParams *params);

  void (*free)(ARegion *);

  /** Split region, copy data optionally. */
  void *(*duplicate)(void *poin);

  /** Register operator types on startup. */
  void (*operatortypes)();
  /** Add items to keymap. */
  void (*keymap)(wmKeyConfig *keyconf);
  /** Allows default cursor per region. */
  void (*cursor)(wmWindow *win, ScrArea *area, ARegion *region);

  /** Return context data. */
  bContextDataCallback context;

  /**
   * Called on every frame in which the region's poll succeeds, regardless of visibility, before
   * drawing, visibility evaluation and initialization. Allows the region to override visibility.
   */
  void (*on_poll_success)(const bContext *C, ARegion *region);

  /**
   * Called whenever the user changes the region's size. Not called when the size is changed
   * through other means, like to adjust for a scaled down window.
   */
  void (*on_user_resize)(const ARegion *region);
  /**
   * Is called whenever the current visible View2D's region changes.
   *
   * Used from user code such as view navigation/zoom operators to inform region about changes.
   * The goal is to support zoom-to-fit features which gets disabled when manual navigation is
   * performed.
   *
   * This callback is not called on indirect changes of the current viewport (which could happen
   * when the `v2d->tot` is changed and `cur` is adopted accordingly).
   */
  void (*on_view2d_changed)(const bContext *C, ARegion *region);

  /** Custom drawing callbacks. */
  ListBase drawcalls;

  /** Panels type definitions. */
  ListBase paneltypes;

  /** Header type definitions. */
  ListBase headertypes;

  /** Hardcoded constraints, smaller than these values region is not visible. */
  int minsizex, minsizey;
  /** When new region opens (region prefsizex/y are zero then. */
  int prefsizex, prefsizey;
  /** Default keymaps to add. */
  int keymapflag;
  /**
   * Return without drawing.
   * lock is set by region definition, and copied to do_lock by render.
   * Set as bitflag value in #ARegionDrawLockFlags.
   */
  short do_lock, lock;
  /** Don't handle gizmos events behind #uiBlock's with #UI_BLOCK_CLIP_EVENTS flag set. */
  bool clip_gizmo_events_by_ui;
  /** Call cursor function on each move event. */
  short event_cursor;
};

/* Panel types. */

struct PanelType {
  PanelType *next, *prev;

  /** Unique name. */
  char idname[BKE_ST_MAXNAME];
  /** For panel header. */
  char label[BKE_ST_MAXNAME];
  /** For panel tooltip. */
  const char *description;
  char translation_context[BKE_ST_MAXNAME];
  /** For buttons window. */
  char context[BKE_ST_MAXNAME];
  /** For category tabs. */
  char category[BKE_ST_MAXNAME];
  /** For work-spaces to selectively show. */
  char owner_id[128];
  /** Parent idname for sub-panels. */
  char parent_id[BKE_ST_MAXNAME];
  /** Boolean property identifier of the panel custom data. Used to draw a highlighted border. */
  char active_property[BKE_ST_MAXNAME];
  char pin_to_last_property[BKE_ST_MAXNAME];
  short space_type;
  short region_type;
  /** For popovers, 0 for default. */
  int ui_units_x;
  /**
   * For popovers, position the popover at the given offset (multiplied by #UI_UNIT_X/#UI_UNIT_Y)
   * relative to the top left corner, if it's not attached to a button.
   */
  blender::float2 offset_units_xy;
  int order;

  int flag;

  /** Verify if the panel should draw or not. */
  bool (*poll)(const bContext *C, PanelType *pt);
  /** Draw header (optional) */
  void (*draw_header)(const bContext *C, Panel *panel);
  /** Draw header preset (optional) */
  void (*draw_header_preset)(const bContext *C, Panel *panel);
  /** Draw entirely, view changes should be handled here. */
  void (*draw)(const bContext *C, Panel *panel);
  /**
   * Listener to redraw the region this is contained in on changes. Only used for panels displayed
   * in popover regions.
   */
  void (*listener)(const wmRegionListenerParams *params);

  /* For instanced panels corresponding to a list: */

  /** Reorder function, called when drag and drop finishes. */
  void (*reorder)(bContext *C, Panel *pa, int new_index);
  /**
   * Get the panel and sub-panel's expansion state from the expansion flag in the corresponding
   * data item. Called on draw updates.
   * \note Sub-panels are indexed in depth first order,
   * the visual order you would see if all panels were expanded.
   */
  short (*get_list_data_expand_flag)(const bContext *C, Panel *pa);
  /**
   * Set the expansion bit-field from the closed / open state of this panel and its sub-panels.
   * Called when the expansion state of the panel changes with user input.
   * \note Sub-panels are indexed in depth first order,
   * the visual order you would see if all panels were expanded.
   */
  void (*set_list_data_expand_flag)(const bContext *C, Panel *pa, short expand_flag);

  /** Sub panels. */
  PanelType *parent;
  ListBase children;

  /** RNA integration. */
  ExtensionRNA rna_ext;
};

/** #PanelType.flag */
enum {
  PANEL_TYPE_DEFAULT_CLOSED = (1 << 0),
  PANEL_TYPE_NO_HEADER = (1 << 1),
  /** Makes buttons in the header shrink/stretch to fill full layout width. */
  PANEL_TYPE_HEADER_EXPAND = (1 << 2),
  PANEL_TYPE_LAYOUT_VERT_BAR = (1 << 3),
  /** This panel type represents data external to the UI. */
  PANEL_TYPE_INSTANCED = (1 << 4),
  /** Don't search panels with this type during property search. */
  PANEL_TYPE_NO_SEARCH = (1 << 7),
};

struct LayoutPanelHeader {
  float start_y;
  float end_y;
  PointerRNA open_owner_ptr;
  std::string open_prop_name;
};

struct LayoutPanelBody {
  float start_y;
  float end_y;
};

/**
 * "Layout Panels" are panels which are defined as part of the #uiLayout. As such they have a
 * specific place in the layout and can not be freely dragged around like top level panels.
 *
 * This struct gathers information about the layout panels created by layout code. This is then
 * used for example drawing the backdrop of nested panels and to support opening and closing
 * multiple panels with a single mouse gesture.
 */
struct LayoutPanels {
  blender::Vector<LayoutPanelHeader> headers;
  blender::Vector<LayoutPanelBody> bodies;

  void clear()
  {
    this->headers.clear();
    this->bodies.clear();
  }
};

struct Panel_Runtime {
  /** Applied to Panel.ofsx, but saved separately so we can track changes between redraws. */
  int region_ofsx = 0;

  /**
   * Pointer for storing which data the panel corresponds to.
   * Useful when there can be multiple instances of the same panel type.
   *
   * \note A panel and its sub-panels share the same custom data pointer.
   * This avoids freeing the same pointer twice when panels are removed.
   */
  PointerRNA *custom_data_ptr = nullptr;

  /**
   * Pointer to the panel's block. Useful when changes to panel #uiBlocks
   * need some context from traversal of the panel "tree".
   */
  uiBlock *block = nullptr;

  /** Non-owning pointer. The context is stored in the block. */
  bContextStore *context = nullptr;

  /** Information about nested layout panels generated in layout code. */
  LayoutPanels layout_panels;
};

namespace blender::bke {

struct ARegionRuntime {
  /** Callbacks for this region type. */
  struct ARegionType *type;

  /** Runtime for partial redraw, same or smaller than #ARegion::winrct. */
  rcti drawrct = {};

  /**
   * The visible part of the region, use with region overlap not to draw
   * on top of the overlapping regions.
   *
   * Lazy initialize, zeroed when unset, relative to #ARegion.winrct x/y min.
   */
  rcti visible_rect = {};

  /**
   * The offset needed to not overlap with window scroll-bars.
   * Only used by HUD regions for now.
   */
  int offset_x = 0;
  int offset_y = 0;

  /** Panel category to use between 'layout' and 'draw'. */
  const char *category = nullptr;

  /** Maps #uiBlock::name to uiBlock for faster lookups. */
  Map<std::string, uiBlock *> block_name_map;
  /** #uiBlock. */
  ListBase uiblocks = {};

  /** #wmEventHandler. */
  ListBase handlers = {};

  /** Use this string to draw info. */
  char *headerstr = nullptr;

  /** Gizmo-map of this region. */
  wmGizmoMap *gizmo_map = nullptr;

  /** Blend in/out. */
  wmTimer *regiontimer = nullptr;

  wmDrawBuffer *draw_buffer = nullptr;

  /** Panel categories runtime. */
  ListBase panels_category = {};

  /** Region is currently visible on screen. */
  short visible = 0;

  /** Private, cached notifier events. */
  short do_draw = 0;

  /** Private, cached notifier events. */
  short do_draw_paintcursor;

  /** Dummy panel used in popups so they can support layout panels. */
  Panel *popup_block_panel = nullptr;
};

}  // namespace blender::bke

/* #uiList types. */

/** Draw an item in the `ui_list`. */
using uiListDrawItemFunc = void (*)(uiList *ui_list,
                                    const bContext *C,
                                    uiLayout *layout,
                                    PointerRNA *dataptr,
                                    PointerRNA *itemptr,
                                    int icon,
                                    PointerRNA *active_dataptr,
                                    const char *active_propname,
                                    int index,
                                    int flt_flag);

/** Draw the filtering part of an uiList. */
using uiListDrawFilterFunc = void (*)(uiList *ui_list, const bContext *C, uiLayout *layout);

/** Filter items of an uiList. */
using uiListFilterItemsFunc = void (*)(uiList *ui_list,
                                       const bContext *C,
                                       PointerRNA *,
                                       const char *propname);

/** Listen to notifiers. Only for lists defined in C. */
using uiListListener = void (*)(uiList *ui_list, wmRegionListenerParams *params);

struct uiListType {
  uiListType *next, *prev;

  /** Unique name. */
  char idname[BKE_ST_MAXNAME];

  uiListDrawItemFunc draw_item;
  uiListDrawFilterFunc draw_filter;
  uiListFilterItemsFunc filter_items;

  /** For lists defined in C only. */
  uiListListener listener;

  /** RNA integration. */
  ExtensionRNA rna_ext;
};

/* Header types. */

struct HeaderType {
  HeaderType *next, *prev;
  /** Unique name. */
  char idname[BKE_ST_MAXNAME];
  int space_type;
  int region_type;

  bool (*poll)(const bContext *C, HeaderType *ht);
  /** Draw entirely, view changes should be handled here. */
  void (*draw)(const bContext *C, Header *header);

  /** RNA integration. */
  ExtensionRNA rna_ext;
};

struct Header {
  /** Runtime. */
  HeaderType *type;
  /** Runtime for drawing. */
  uiLayout *layout;
};

/* Menu types. */

enum class MenuTypeFlag {
  /**
   * Whether the menu depends on data retrieved via #CTX_data_pointer_get. If it is context
   * dependent, menu search has to scan it in different contexts.
   */
  ContextDependent = (1 << 0),
  /**
   * Automatically start searching in the menu when pressing a key.
   */
  SearchOnKeyPress = (1 << 1),
};
ENUM_OPERATORS(MenuTypeFlag)

struct MenuType {
  MenuType *next, *prev;

  /** Unique name. */
  char idname[BKE_ST_MAXNAME];
  /** For button text. */
  char label[BKE_ST_MAXNAME];
  char translation_context[BKE_ST_MAXNAME];
  /** Optional, see: #wmOwnerID. */
  char owner_id[128];
  const char *description;

  /** Verify if the menu should draw or not. */
  bool (*poll)(const bContext *C, MenuType *mt);
  /** Draw entirely, view changes should be handled here. */
  void (*draw)(const bContext *C, Menu *menu);
  void (*listener)(const wmRegionListenerParams *params);

  MenuTypeFlag flag;

  /** RNA integration. */
  ExtensionRNA rna_ext;
};

struct Menu {
  /** Runtime. */
  MenuType *type;
  /** Runtime for drawing. */
  uiLayout *layout;
};

/* Asset shelf types. */

/* #AssetShelfType.flag */
enum AssetShelfTypeFlag {
  /**
   * Do not trigger asset dragging on drag events.
   * Drag events can be overridden with custom keymap items then.
   */
  ASSET_SHELF_TYPE_FLAG_NO_ASSET_DRAG = (1 << 0),
  ASSET_SHELF_TYPE_FLAG_DEFAULT_VISIBLE = (1 << 1),
  ASSET_SHELF_TYPE_FLAG_STORE_CATALOGS_IN_PREFS = (1 << 2),
  /**
   * When spawning a context menu for an asset, activate the asset and call the activate operator
   * (`bl_activate_operator`/#AssetShelfType.activate_operator) if present, rather than just
   * highlighting the asset as active.
   */
  ASSET_SHELF_TYPE_FLAG_ACTIVATE_FOR_CONTEXT_MENU = (1 << 3),
};
ENUM_OPERATORS(AssetShelfTypeFlag);

#define ASSET_SHELF_PREVIEW_SIZE_DEFAULT 48

struct AssetShelfType {
  /** Unique name. */
  char idname[BKE_ST_MAXNAME];

  int space_type;

  /** Operator to call when activating a grid view item. */
  std::string activate_operator;
  /** Operator to call when dragging a grid view item. */
  std::string drag_operator;

  AssetShelfTypeFlag flag;

  short default_preview_size;

  /** Determine if asset shelves of this type should be available in current context or not. */
  bool (*poll)(const bContext *C, const AssetShelfType *shelf_type);

  /**
   * Determine if an individual asset should be visible or not. May be a temporary design,
   * visibility should first and foremost be controlled by asset traits.
   */
  bool (*asset_poll)(const AssetShelfType *shelf_type,
                     const blender::asset_system::AssetRepresentation *asset);

  /** Asset shelves can define their own context menu via this layout definition callback. */
  void (*draw_context_menu)(const bContext *C,
                            const AssetShelfType *shelf_type,
                            const blender::asset_system::AssetRepresentation *asset,
                            uiLayout *layout);

  const AssetWeakReference *(*get_active_asset)(const AssetShelfType *shelf_type);

  /** RNA integration. */
  ExtensionRNA rna_ext;
};

/* Space-types. */

SpaceType *BKE_spacetype_from_id(int spaceid);
ARegionType *BKE_regiontype_from_id(const SpaceType *st, int regionid);
blender::Span<std::unique_ptr<SpaceType>> BKE_spacetypes_list();
void BKE_spacetype_register(std::unique_ptr<SpaceType> st);
bool BKE_spacetype_exists(int spaceid);
/** Only for quitting blender. */
void BKE_spacetypes_free();

/* Space-data. */

void BKE_spacedata_freelist(ListBase *lb);
/**
 * \param lb_dst: should be empty (will be cleared).
 */
void BKE_spacedata_copylist(ListBase *lb_dst, ListBase *lb_src);

/**
 * Facility to set locks for drawing to survive (render) threads accessing drawing data.
 *
 * \note Should be replaced in future by better local data handling for threads.
 * \note Effect of multiple calls to this function is not accumulative. The locking flags
 * will be set to by the last call.
 */
void BKE_spacedata_draw_locks(ARegionDrawLockFlags lock_flags);

/**
 * Version of #BKE_area_find_region_type that also works if \a slink
 * is not the active space of \a area.
 */
ARegion *BKE_spacedata_find_region_type(const SpaceLink *slink,
                                        const ScrArea *area,
                                        int region_type) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

void BKE_spacedata_callback_id_remap_set(
    void (*func)(ScrArea *area, SpaceLink *sl, ID *old_id, ID *new_id));
/**
 * Currently unused!
 */
void BKE_spacedata_id_unref(ScrArea *area, SpaceLink *sl, ID *id);

/* Area/regions. */

ARegion *BKE_area_region_copy(const SpaceType *st, const ARegion *region);

ARegion *BKE_area_region_new();

/**
 * Doesn't free the region itself.
 */
void BKE_area_region_free(SpaceType *st, ARegion *region);
void BKE_area_region_panels_free(ListBase *panels);
/**
 * Create and free panels.
 */
Panel *BKE_panel_new(PanelType *panel_type);
void BKE_panel_free(Panel *panel);
/**
 * Doesn't free the area itself.
 */
void BKE_screen_area_free(ScrArea *area);
/**
 * Gizmo-maps of a region need to be freed with the region.
 * Uses callback to avoid low-level call.
 */
void BKE_region_callback_free_gizmomap_set(void (*callback)(wmGizmoMap *));
void BKE_region_callback_refresh_tag_gizmomap_set(void (*callback)(wmGizmoMap *));

/**
 * Get the layout panel state for the given idname. If it does not exist yet, initialize a new
 * panel state with the given default value.
 */
LayoutPanelState *BKE_panel_layout_panel_state_ensure(Panel *panel,
                                                      blender::StringRef idname,
                                                      bool default_closed);

/**
 * Find a region of type \a region_type in provided \a regionbase.
 *
 * \note this is useful for versioning where either the #Area or #SpaceLink regionbase are typical
 * inputs
 */
ARegion *BKE_region_find_in_listbase_by_type(const ListBase *regionbase, const int region_type);
/**
 * Find a region of type \a region_type in the currently active space of \a area.
 *
 * \note This does _not_ work if the region to look up is not in the active space.
 * Use #BKE_spacedata_find_region_type if that may be the case.
 */
ARegion *BKE_area_find_region_type(const ScrArea *area, int region_type);
ARegion *BKE_area_find_region_active_win(const ScrArea *area);
ARegion *BKE_area_find_region_xy(const ScrArea *area, int regiontype, const int xy[2])
    ATTR_NONNULL(3);

/**
 * \note This is only for screen level regions (typically menus/popups).
 */
ARegion *BKE_screen_find_region_type(const bScreen *screen, int region_type) ATTR_NONNULL(1);
/**
 * \note This is only for screen level regions (typically menus/popups).
 */
ARegion *BKE_screen_find_region_xy(const bScreen *screen,
                                   int regiontype,
                                   const int xy[2]) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 3);

ARegion *BKE_screen_find_main_region_at_xy(const bScreen *screen, int space_type, const int xy[2])
    ATTR_NONNULL(1, 3);
/**
 * \note Ideally we can get the area from the context,
 * there are a few places however where this isn't practical.
 */
ScrArea *BKE_screen_find_area_from_space(const bScreen *screen,
                                         const SpaceLink *sl) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);
/**
 * \note used to get proper RNA paths for spaces (editors).
 */
std::optional<std::string> BKE_screen_path_from_screen_to_space(const PointerRNA *ptr);
/**
 * \note Using this function is generally a last resort, you really want to be
 * using the context when you can - campbell
 */
ScrArea *BKE_screen_find_big_area(const bScreen *screen, int spacetype, short min);
ScrArea *BKE_screen_area_map_find_area_xy(const ScrAreaMap *areamap,
                                          int spacetype,
                                          const int xy[2]) ATTR_NONNULL(1, 3);
ScrArea *BKE_screen_find_area_xy(const bScreen *screen, int spacetype, const int xy[2])
    ATTR_NONNULL(1, 3);

void BKE_screen_gizmo_tag_refresh(bScreen *screen);

/**
 * Refresh any screen data that should be set on file-load
 * with "Load UI" disabled.
 */
void BKE_screen_runtime_refresh_for_blendfile(bScreen *screen);

void BKE_screen_view3d_sync(View3D *v3d, Scene *scene);
void BKE_screen_view3d_scene_sync(bScreen *screen, Scene *scene);
bool BKE_screen_is_fullscreen_area(const bScreen *screen) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BKE_screen_is_used(const bScreen *screen) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* Zoom factor conversion. */

float BKE_screen_view3d_zoom_to_fac(float camzoom);
float BKE_screen_view3d_zoom_from_fac(float zoomfac);

void BKE_screen_view3d_shading_init(View3DShading *shading);

/* Screen. */

/**
 * Callback used by lib_query to walk over all ID usages
 * (mimics `foreach_id` callback of #IDTypeInfo structure).
 */
void BKE_screen_foreach_id_screen_area(LibraryForeachIDData *data, ScrArea *area);

/**
 * Free (or release) any data used by this screen (does not free the screen itself).
 */
void BKE_screen_free_data(bScreen *screen);
void BKE_screen_area_map_free(ScrAreaMap *area_map) ATTR_NONNULL();

ScrEdge *BKE_screen_find_edge(const bScreen *screen, ScrVert *v1, ScrVert *v2);
void BKE_screen_sort_scrvert(ScrVert **v1, ScrVert **v2);
void BKE_screen_remove_double_scrverts(bScreen *screen);
void BKE_screen_remove_double_scredges(bScreen *screen);
void BKE_screen_remove_unused_scredges(bScreen *screen);
void BKE_screen_remove_unused_scrverts(bScreen *screen);

void BKE_screen_header_alignment_reset(bScreen *screen);

/* .blend file I/O */

void BKE_screen_view3d_shading_blend_write(BlendWriter *writer, View3DShading *shading);
void BKE_screen_view3d_shading_blend_read_data(BlendDataReader *reader, View3DShading *shading);

void BKE_screen_area_map_blend_write(BlendWriter *writer, ScrAreaMap *area_map);
/**
 * \return false on error.
 */
bool BKE_screen_area_map_blend_read_data(BlendDataReader *reader, ScrAreaMap *area_map);
/**
 * And as patch for 2.48 and older.
 * For the saved 2.50 files without `regiondata`.
 */
void BKE_screen_view3d_do_versions_250(View3D *v3d, ListBase *regions);

/**
 * Called after lib linking process is done, to perform some validation on the read data, or some
 * complex specific reading process that requires the data to be fully read and ID pointers to be
 * valid.
 */
void BKE_screen_area_blend_read_after_liblink(BlendLibReader *reader,
                                              ID *parent_id,
                                              ScrArea *area);
/**
 * Cannot use #IDTypeInfo callback yet, because of the return value.
 */
bool BKE_screen_blend_read_data(BlendDataReader *reader, bScreen *screen);
