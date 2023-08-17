/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_attrs.h"

#include "RNA_types.hh"

#include "BKE_context.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct Header;
struct ID;
struct IDRemapper;
struct LibraryForeachIDData;
struct ListBase;
struct Menu;
struct Panel;
struct Scene;
struct ScrArea;
struct ScrAreaMap;
struct ScrVert;
struct SpaceType;
struct View3D;
struct View3DShading;
struct WorkSpace;
struct bContext;
struct bScreen;
struct uiLayout;
struct uiList;
struct wmGizmoMap;
struct wmKeyConfig;
struct wmMsgBus;
struct wmNotifier;
struct wmWindow;
struct wmWindowManager;

/* spacetype has everything stored to get an editor working, it gets initialized via
 * #ED_spacetypes_init() in `editors/space_api/spacetypes.cc` */
/* an editor in Blender is a combined ScrArea + SpaceType + SpaceData */

#define BKE_ST_MAXNAME 64

typedef struct wmSpaceTypeListenerParams {
  struct wmWindow *window;
  struct ScrArea *area;
  const struct wmNotifier *notifier;
  const struct Scene *scene;
} wmSpaceTypeListenerParams;

typedef struct SpaceType {
  struct SpaceType *next, *prev;

  char name[BKE_ST_MAXNAME]; /* for menus */
  int spaceid;               /* unique space identifier */
  int iconid;                /* icon lookup for menus */

  /* Initial allocation, after this WM will call init() too. Some editors need
   * area and scene data (e.g. frame range) to set their initial scrolling. */
  struct SpaceLink *(*create)(const struct ScrArea *area, const struct Scene *scene);
  /* not free spacelink itself */
  void (*free)(struct SpaceLink *sl);

  /* init is to cope with file load, screen (size) changes, check handlers */
  void (*init)(struct wmWindowManager *wm, struct ScrArea *area);
  /* exit is called when the area is hidden or removed */
  void (*exit)(struct wmWindowManager *wm, struct ScrArea *area);
  /* Listeners can react to bContext changes */
  void (*listener)(const wmSpaceTypeListenerParams *params);

  /* called when the mouse moves out of the area */
  void (*deactivate)(struct ScrArea *area);

  /* refresh context, called after filereads, ED_area_tag_refresh() */
  void (*refresh)(const struct bContext *C, struct ScrArea *area);

  /* after a spacedata copy, an init should result in exact same situation */
  struct SpaceLink *(*duplicate)(struct SpaceLink *sl);

  /* register operator types on startup */
  void (*operatortypes)(void);
  /* add default items to WM keymap */
  void (*keymap)(struct wmKeyConfig *keyconf);
  /* on startup, define dropboxes for spacetype+regions */
  void (*dropboxes)(void);

  /* initialize gizmo-map-types and gizmo-group-types with the region */
  void (*gizmos)(void);

  /* return context data */
  bContextDataCallback context;

  /* Used when we want to replace an ID by another (or NULL). */
  void (*id_remap)(struct ScrArea *area, struct SpaceLink *sl, const struct IDRemapper *mappings);

  /**
   * foreach_id callback to process all ID pointers of the editor. Used indirectly by lib_query's
   * #BKE_library_foreach_ID_link when #IDWALK_INCLUDE_UI bitflag is set (through WM's foreach_id
   * usage of #BKE_screen_foreach_id_screen_area).
   */
  void (*foreach_id)(struct SpaceLink *space_link, struct LibraryForeachIDData *data);

  int (*space_subtype_get)(struct ScrArea *area);
  void (*space_subtype_set)(struct ScrArea *area, int value);
  void (*space_subtype_item_extend)(struct bContext *C, EnumPropertyItem **item, int *totitem);

  /**
   * Update pointers for all structs directly owned by this space.
   */
  void (*blend_read_data)(struct BlendDataReader *reader, struct SpaceLink *space_link);

  /**
   * Update pointers to other id data blocks.
   */
  void (*blend_read_lib)(struct BlendLibReader *reader,
                         struct ID *parent_id,
                         struct SpaceLink *space_link);

  /**
   * Write all structs that should be saved in a .blend file.
   */
  void (*blend_write)(struct BlendWriter *writer, struct SpaceLink *space_link);

  /* region type definitions */
  ListBase regiontypes;

  /** Asset shelf type definitions. */
  ListBase asset_shelf_types; /* #AssetShelfType */

  /* read and write... */

  /** Default key-maps to add. */
  int keymapflag;

} SpaceType;

/* region types are also defined using spacetypes_init, via a callback */

typedef struct wmRegionListenerParams {
  struct wmWindow *window;
  struct ScrArea *area; /* Can be NULL when the region is not part of an area. */
  struct ARegion *region;
  const struct wmNotifier *notifier;
  const struct Scene *scene;
} wmRegionListenerParams;

typedef struct wmRegionMessageSubscribeParams {
  const struct bContext *context;
  struct wmMsgBus *message_bus;
  struct WorkSpace *workspace;
  struct Scene *scene;
  struct bScreen *screen;
  struct ScrArea *area;
  struct ARegion *region;
} wmRegionMessageSubscribeParams;

typedef struct RegionPollParams {
  const struct bScreen *screen;
  const struct ScrArea *area;
  const struct ARegion *region;

  /** Full context, if WM context above is not enough. */
  const struct bContext *context;
} RegionPollParams;

typedef struct ARegionType {
  struct ARegionType *next, *prev;

  int regionid; /* unique identifier within this space, defines RGN_TYPE_xxxx */

  /* add handlers, stuff you only do once or on area/region type/size changes */
  void (*init)(struct wmWindowManager *wm, struct ARegion *region);
  /* exit is called when the region is hidden or removed */
  void (*exit)(struct wmWindowManager *wm, struct ARegion *region);
  /**
   * Optional callback to decide whether the region should be treated as existing given the
   * current context. When returning false, the region will be kept in storage, but is not
   * available to the user in any way. Callbacks can assume that context has the owning area and
   * space-data set.
   */
  bool (*poll)(const RegionPollParams *params);
  /* draw entirely, view changes should be handled here */
  void (*draw)(const struct bContext *C, struct ARegion *region);
  /**
   * Handler to draw overlays. This handler is called every draw loop.
   *
   * \note Some editors should return early if the interface is locked
   * (check with #CTX_wm_interface_locked) to avoid accessing scene data
   * that another thread may be modifying
   */
  void (*draw_overlay)(const struct bContext *C, struct ARegion *region);
  /* optional, compute button layout before drawing for dynamic size */
  void (*layout)(const struct bContext *C, struct ARegion *region);
  /* snap the size of the region (can be NULL for no snapping). */
  int (*snap_size)(const struct ARegion *region, int size, int axis);
  /* contextual changes should be handled here */
  void (*listener)(const wmRegionListenerParams *params);
  /* Optional callback to generate subscriptions. */
  void (*message_subscribe)(const wmRegionMessageSubscribeParams *params);

  void (*free)(struct ARegion *);

  /* split region, copy data optionally */
  void *(*duplicate)(void *poin);

  /* register operator types on startup */
  void (*operatortypes)(void);
  /* add own items to keymap */
  void (*keymap)(struct wmKeyConfig *keyconf);
  /* allows default cursor per region */
  void (*cursor)(struct wmWindow *win, struct ScrArea *area, struct ARegion *region);

  /* return context data */
  bContextDataCallback context;

  /* Is called whenever the current visible View2D's region changes.
   *
   * Used from user code such as view navigation/zoom operators to inform region about changes.
   * The goal is to support zoom-to-fit features which gets disabled when manual navigation is
   * performed.
   *
   * This callback is not called on indirect changes of the current viewport (which could happen
   * when the `v2d->tot is changed and `cur` is adopted accordingly). */
  void (*on_view2d_changed)(const struct bContext *C, struct ARegion *region);

  /* custom drawing callbacks */
  ListBase drawcalls;

  /* panels type definitions */
  ListBase paneltypes;

  /* header type definitions */
  ListBase headertypes;

  /* hardcoded constraints, smaller than these values region is not visible */
  int minsizex, minsizey;
  /* when new region opens (region prefsizex/y are zero then */
  int prefsizex, prefsizey;
  /* default keymaps to add */
  int keymapflag;
  /* return without drawing.
   * lock is set by region definition, and copied to do_lock by render. can become flag. */
  short do_lock, lock;
  /** Don't handle gizmos events behind #uiBlock's with #UI_BLOCK_CLIP_EVENTS flag set. */
  bool clip_gizmo_events_by_ui;
  /* call cursor function on each move event */
  short event_cursor;
} ARegionType;

/* panel types */

typedef struct PanelType {
  struct PanelType *next, *prev;

  char idname[BKE_ST_MAXNAME]; /* unique name */
  char label[BKE_ST_MAXNAME];  /* for panel header */
  const char *description;     /* for panel tooltip */
  char translation_context[BKE_ST_MAXNAME];
  char context[BKE_ST_MAXNAME];   /* for buttons window */
  char category[BKE_ST_MAXNAME];  /* for category tabs */
  char owner_id[BKE_ST_MAXNAME];  /* for work-spaces to selectively show. */
  char parent_id[BKE_ST_MAXNAME]; /* parent idname for sub-panels */
  /** Boolean property identifier of the panel custom data. Used to draw a highlighted border. */
  char active_property[BKE_ST_MAXNAME];
  short space_type;
  short region_type;
  /* For popovers, 0 for default. */
  int ui_units_x;
  int order;

  int flag;

  /* verify if the panel should draw or not */
  bool (*poll)(const struct bContext *C, struct PanelType *pt);
  /* draw header (optional) */
  void (*draw_header)(const struct bContext *C, struct Panel *panel);
  /* draw header preset (optional) */
  void (*draw_header_preset)(const struct bContext *C, struct Panel *panel);
  /* draw entirely, view changes should be handled here */
  void (*draw)(const struct bContext *C, struct Panel *panel);

  /* For instanced panels corresponding to a list: */

  /** Reorder function, called when drag and drop finishes. */
  void (*reorder)(struct bContext *C, struct Panel *pa, int new_index);
  /**
   * Get the panel and sub-panel's expansion state from the expansion flag in the corresponding
   * data item. Called on draw updates.
   * \note Sub-panels are indexed in depth first order,
   * the visual order you would see if all panels were expanded.
   */
  short (*get_list_data_expand_flag)(const struct bContext *C, struct Panel *pa);
  /**
   * Set the expansion bit-field from the closed / open state of this panel and its sub-panels.
   * Called when the expansion state of the panel changes with user input.
   * \note Sub-panels are indexed in depth first order,
   * the visual order you would see if all panels were expanded.
   */
  void (*set_list_data_expand_flag)(const struct bContext *C, struct Panel *pa, short expand_flag);

  /* sub panels */
  struct PanelType *parent;
  ListBase children;

  /* RNA integration */
  ExtensionRNA rna_ext;
} PanelType;

/* #PanelType.flag */
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

/* uilist types */

/* Draw an item in the uiList */
typedef void (*uiListDrawItemFunc)(struct uiList *ui_list,
                                   const struct bContext *C,
                                   struct uiLayout *layout,
                                   struct PointerRNA *dataptr,
                                   struct PointerRNA *itemptr,
                                   int icon,
                                   struct PointerRNA *active_dataptr,
                                   const char *active_propname,
                                   int index,
                                   int flt_flag);

/* Draw the filtering part of an uiList */
typedef void (*uiListDrawFilterFunc)(struct uiList *ui_list,
                                     const struct bContext *C,
                                     struct uiLayout *layout);

/* Filter items of an uiList */
typedef void (*uiListFilterItemsFunc)(struct uiList *ui_list,
                                      const struct bContext *C,
                                      struct PointerRNA *,
                                      const char *propname);

/* Listen to notifiers. Only for lists defined in C. */
typedef void (*uiListListener)(struct uiList *ui_list, wmRegionListenerParams *params);

typedef struct uiListType {
  struct uiListType *next, *prev;

  char idname[BKE_ST_MAXNAME]; /* unique name */

  uiListDrawItemFunc draw_item;
  uiListDrawFilterFunc draw_filter;
  uiListFilterItemsFunc filter_items;

  /* For lists defined in C only. */
  uiListListener listener;

  /* RNA integration */
  ExtensionRNA rna_ext;
} uiListType;

/* header types */

typedef struct HeaderType {
  struct HeaderType *next, *prev;

  char idname[BKE_ST_MAXNAME]; /* unique name */
  int space_type;
  int region_type;

  bool (*poll)(const struct bContext *C, struct HeaderType *ht);
  /* draw entirely, view changes should be handled here */
  void (*draw)(const struct bContext *C, struct Header *header);

  /* RNA integration */
  ExtensionRNA rna_ext;
} HeaderType;

typedef struct Header {
  struct HeaderType *type; /* runtime */
  struct uiLayout *layout; /* runtime for drawing */
} Header;

/* menu types */

typedef struct MenuType {
  struct MenuType *next, *prev;

  char idname[BKE_ST_MAXNAME]; /* unique name */
  char label[BKE_ST_MAXNAME];  /* for button text */
  char translation_context[BKE_ST_MAXNAME];
  char owner_id[BKE_ST_MAXNAME]; /* optional, see: #wmOwnerID */
  const char *description;

  /* verify if the menu should draw or not */
  bool (*poll)(const struct bContext *C, struct MenuType *mt);
  /* draw entirely, view changes should be handled here */
  void (*draw)(const struct bContext *C, struct Menu *menu);
  void (*listener)(const wmRegionListenerParams *params);

  /* RNA integration */
  ExtensionRNA rna_ext;
} MenuType;

typedef struct Menu {
  struct MenuType *type;   /* runtime */
  struct uiLayout *layout; /* runtime for drawing */
} Menu;

/* asset shelf types */

/* #AssetShelfType.flag */
typedef enum AssetShelfTypeFlag {
  /** Do not trigger asset dragging on drag events. Drag events can be overridden with custom
   * keymap items then. */
  ASSET_SHELF_TYPE_FLAG_NO_ASSET_DRAG = (1 << 0),

  ASSET_SHELF_TYPE_FLAG_MAX
} AssetShelfTypeFlag;
ENUM_OPERATORS(AssetShelfTypeFlag, ASSET_SHELF_TYPE_FLAG_MAX);

typedef struct AssetShelfType {
  struct AssetShelfType *next, *prev;

  char idname[BKE_ST_MAXNAME]; /* unique name */

  int space_type;

  AssetShelfTypeFlag flag;

  /** Determine if asset shelves of this type should be available in current context or not. */
  bool (*poll)(const struct bContext *C, const struct AssetShelfType *shelf_type);

  /** Determine if an individual asset should be visible or not. May be a temporary design,
   * visibility should first and foremost be controlled by asset traits. */
  bool (*asset_poll)(const struct AssetShelfType *shelf_type, const struct AssetHandle *asset);

  /** Asset shelves can define their own context menu via this layout definition callback. */
  void (*draw_context_menu)(const struct bContext *C,
                            const struct AssetShelfType *shelf_type,
                            const struct AssetHandle *asset,
                            struct uiLayout *layout);

  /* RNA integration */
  ExtensionRNA rna_ext;
} AssetShelfType;

/* Space-types. */

struct SpaceType *BKE_spacetype_from_id(int spaceid);
struct ARegionType *BKE_regiontype_from_id_or_first(const struct SpaceType *st, int regionid);
struct ARegionType *BKE_regiontype_from_id(const struct SpaceType *st, int regionid);
const struct ListBase *BKE_spacetypes_list(void);
void BKE_spacetype_register(struct SpaceType *st);
bool BKE_spacetype_exists(int spaceid);
void BKE_spacetypes_free(void); /* only for quitting blender */

/* Space-data. */

void BKE_spacedata_freelist(ListBase *lb);
/**
 * \param lb_dst: should be empty (will be cleared).
 */
void BKE_spacedata_copylist(ListBase *lb_dst, ListBase *lb_src);

/**
 * Facility to set locks for drawing to survive (render) threads accessing drawing data.
 *
 * \note Lock can become bit-flag too.
 * \note Should be replaced in future by better local data handling for threads.
 */
void BKE_spacedata_draw_locks(bool set);

/**
 * Version of #BKE_area_find_region_type that also works if \a slink
 * is not the active space of \a area.
 */
struct ARegion *BKE_spacedata_find_region_type(const struct SpaceLink *slink,
                                               const struct ScrArea *area,
                                               int region_type) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

void BKE_spacedata_callback_id_remap_set(void (*func)(
    struct ScrArea *area, struct SpaceLink *sl, struct ID *old_id, struct ID *new_id));
/**
 * Currently unused!
 */
void BKE_spacedata_id_unref(struct ScrArea *area, struct SpaceLink *sl, struct ID *id);

/* Area/regions. */

struct ARegion *BKE_area_region_copy(const struct SpaceType *st, const struct ARegion *region);
/**
 * Doesn't free the region itself.
 */
void BKE_area_region_free(struct SpaceType *st, struct ARegion *region);
void BKE_area_region_panels_free(struct ListBase *panels);
/**
 * Doesn't free the area itself.
 */
void BKE_screen_area_free(struct ScrArea *area);
/**
 * Gizmo-maps of a region need to be freed with the region.
 * Uses callback to avoid low-level call.
 */
void BKE_region_callback_free_gizmomap_set(void (*callback)(struct wmGizmoMap *));
void BKE_region_callback_refresh_tag_gizmomap_set(void (*callback)(struct wmGizmoMap *));

/**
 * Find a region of type \a region_type in provided \a regionbase.
 *
 * \note this is useful for versioning where either the #Area or #SpaceLink regionbase are typical
 * inputs
 */
struct ARegion *BKE_region_find_in_listbase_by_type(const struct ListBase *regionbase,
                                                    const int region_type);
/**
 * Find a region of type \a region_type in the currently active space of \a area.
 *
 * \note This does _not_ work if the region to look up is not in the active space.
 * Use #BKE_spacedata_find_region_type if that may be the case.
 */
struct ARegion *BKE_area_find_region_type(const struct ScrArea *area, int type);
struct ARegion *BKE_area_find_region_active_win(const struct ScrArea *area);
struct ARegion *BKE_area_find_region_xy(const struct ScrArea *area,
                                        int regiontype,
                                        const int xy[2]) ATTR_NONNULL(3);
/**
 * \note This is only for screen level regions (typically menus/popups).
 */
struct ARegion *BKE_screen_find_region_xy(const struct bScreen *screen,
                                          int regiontype,
                                          const int xy[2]) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 3);

struct ARegion *BKE_screen_find_main_region_at_xy(const struct bScreen *screen,
                                                  int space_type,
                                                  const int xy[2]) ATTR_NONNULL(1, 3);
/**
 * \note Ideally we can get the area from the context,
 * there are a few places however where this isn't practical.
 */
struct ScrArea *BKE_screen_find_area_from_space(const struct bScreen *screen,
                                                const struct SpaceLink *sl) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);
/**
 * \note Using this function is generally a last resort, you really want to be
 * using the context when you can - campbell
 */
struct ScrArea *BKE_screen_find_big_area(const struct bScreen *screen, int spacetype, short min);
struct ScrArea *BKE_screen_area_map_find_area_xy(const struct ScrAreaMap *areamap,
                                                 int spacetype,
                                                 const int xy[2]) ATTR_NONNULL(1, 3);
struct ScrArea *BKE_screen_find_area_xy(const struct bScreen *screen,
                                        int spacetype,
                                        const int xy[2]) ATTR_NONNULL(1, 3);

void BKE_screen_gizmo_tag_refresh(struct bScreen *screen);

void BKE_screen_view3d_sync(struct View3D *v3d, struct Scene *scene);
void BKE_screen_view3d_scene_sync(struct bScreen *screen, struct Scene *scene);
bool BKE_screen_is_fullscreen_area(const struct bScreen *screen) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
bool BKE_screen_is_used(const struct bScreen *screen) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* Zoom factor conversion. */

float BKE_screen_view3d_zoom_to_fac(float camzoom);
float BKE_screen_view3d_zoom_from_fac(float zoomfac);

void BKE_screen_view3d_shading_init(struct View3DShading *shading);

/* Screen. */

/**
 * Callback used by lib_query to walk over all ID usages
 * (mimics `foreach_id` callback of #IDTypeInfo structure).
 */
void BKE_screen_foreach_id_screen_area(struct LibraryForeachIDData *data, struct ScrArea *area);

/**
 * Free (or release) any data used by this screen (does not free the screen itself).
 */
void BKE_screen_free_data(struct bScreen *screen);
void BKE_screen_area_map_free(struct ScrAreaMap *area_map) ATTR_NONNULL();

struct ScrEdge *BKE_screen_find_edge(const struct bScreen *screen,
                                     struct ScrVert *v1,
                                     struct ScrVert *v2);
void BKE_screen_sort_scrvert(struct ScrVert **v1, struct ScrVert **v2);
void BKE_screen_remove_double_scrverts(struct bScreen *screen);
void BKE_screen_remove_double_scredges(struct bScreen *screen);
void BKE_screen_remove_unused_scredges(struct bScreen *screen);
void BKE_screen_remove_unused_scrverts(struct bScreen *screen);

void BKE_screen_header_alignment_reset(struct bScreen *screen);

/* .blend file I/O */

void BKE_screen_view3d_shading_blend_write(struct BlendWriter *writer,
                                           struct View3DShading *shading);
void BKE_screen_view3d_shading_blend_read_data(struct BlendDataReader *reader,
                                               struct View3DShading *shading);

void BKE_screen_area_map_blend_write(struct BlendWriter *writer, struct ScrAreaMap *area_map);
/**
 * \return false on error.
 */
bool BKE_screen_area_map_blend_read_data(struct BlendDataReader *reader,
                                         struct ScrAreaMap *area_map);
/**
 * And as patch for 2.48 and older.
 * For the saved 2.50 files without `regiondata`.
 */
void BKE_screen_view3d_do_versions_250(struct View3D *v3d, ListBase *regions);
void BKE_screen_area_blend_read_lib(struct BlendLibReader *reader,
                                    struct ID *parent_id,
                                    struct ScrArea *area);
/**
 * Cannot use #IDTypeInfo callback yet, because of the return value.
 */
bool BKE_screen_blend_read_data(struct BlendDataReader *reader, struct bScreen *screen);

#ifdef __cplusplus
}
#endif
