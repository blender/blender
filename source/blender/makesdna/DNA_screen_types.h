/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_enum_flags.hh"

#include "DNA_asset_types.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_vec_types.h"
#include "DNA_view2d_types.h"

#include "DNA_ID.h"

struct ARegion;
struct ARegionType;
struct PanelType;
struct PointerRNA;
struct Scene;
struct SpaceLink;
struct SpaceType;
struct uiBlock;
struct uiLayout;
struct uiList;
struct uiListType;
struct wmDrawBuffer;
struct wmTimer;
struct wmTooltipState;
struct Panel_Runtime;
#ifdef __cplusplus
namespace blender::bke {
struct ARegionRuntime;
struct FileHandlerType;
}  // namespace blender::bke
using ARegionRuntimeHandle = blender::bke::ARegionRuntime;
using FileHandlerTypeHandle = blender::bke::FileHandlerType;
#else
typedef struct ARegionRuntimeHandle ARegionRuntimeHandle;
typedef struct FileHandlerTypeHandle FileHandlerTypeHandle;
#endif

/* TODO: Doing this is quite ugly :)
 * Once the top-bar is merged bScreen should be refactored to use ScrAreaMap. */
#define AREAMAP_FROM_SCREEN(screen) ((ScrAreaMap *)&(screen)->vertbase)

typedef struct bScreen {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_SCR;
#endif

  ID id;

  /* TODO: Should become ScrAreaMap now.
   * NOTE: KEEP ORDER IN SYNC WITH #ScrAreaMap! (see AREAMAP_FROM_SCREEN macro above). */
  /** Screens have vertices/edges to define areas. */
  ListBase vertbase;
  ListBase edgebase;
  ListBase areabase;
  /* End variables that must be in sync with #ScrAreaMap. */

  /** Screen level regions (menus), runtime only. */
  ListBase regionbase;

  struct Scene *scene DNA_DEPRECATED;

  /** General flags. */
  short flag;
  /** Window-ID from WM, starts with 1. */
  short winid;
  /** User-setting for which editors get redrawn during animation playback. */
  short redraws_flag;

  /** Temp screen in a temp window, don't save (like user-preferences). */
  char temp;
  /** Temp screen for image render display or file-select. */
  char state;
  /** Notifier for drawing edges. */
  char do_draw;
  /** Notifier for scale screen, changed screen, etc. */
  char do_refresh;
  /** Notifier for gesture draw. */
  char do_draw_gesture;
  /** Notifier for paint cursor draw. */
  char do_draw_paintcursor;
  /** Notifier for dragging draw. */
  char do_draw_drag;
  /** Set to delay screen handling after switching back from maximized area. */
  char skip_handling;
  /** Set when scrubbing to avoid some costly updates. */
  char scrubbing;
  char _pad[1];

  /** Active region that has mouse focus. */
  struct ARegion *active_region;

  /** If set, screen has timer handler added in window. */
  struct wmTimer *animtimer;
  /** Context callback. */
  void /*bContextDataCallback*/ *context;

  /* Used to restore after SCREENFULL state. */
  short fullscreen_flag;
  char _pad2[6];

  /** Runtime. */
  struct wmTooltipState *tool_tip;

  PreviewImage *preview;
} bScreen;

typedef struct ScrVert {
  struct ScrVert *next, *prev, *newv;
  vec2s vec;
  /* first one used internally, second one for tools */
  short flag, editflag;
} ScrVert;

typedef struct ScrEdge {
  struct ScrEdge *next, *prev;
  ScrVert *v1, *v2;
  /** 1 when at edge of screen. */
  short border;
  short flag;
  char _pad[4];
} ScrEdge;

typedef struct ScrAreaMap {
  /* ** NOTE: KEEP ORDER IN SYNC WITH LISTBASES IN bScreen! ** */

  /** ScrVert - screens have vertices/edges to define areas. */
  ListBase vertbase;
  /** ScrEdge. */
  ListBase edgebase;
  /** ScrArea. */
  ListBase areabase;
} ScrAreaMap;

typedef struct LayoutPanelState {
  struct LayoutPanelState *next, *prev;
  /** Identifier of the panel. */
  char *idname;
  uint8_t flag;
  char _pad[3];
  /**
   * A logical time set from #layout_panel_states_clock when the panel is used by the UI. This is
   * used to detect the least-recently-used panel states when some panel states should be removed.
   */
  uint32_t last_used;
} LayoutPanelState;

enum LayoutPanelStateFlag {
  /** If set, the panel is currently open. Otherwise it is collapsed. */
  LAYOUT_PANEL_STATE_FLAG_OPEN = (1 << 0),
};

/** The part from uiBlock that needs saved in file. */
typedef struct Panel {
  struct Panel *next, *prev;

  /** Runtime. */
  struct PanelType *type;
  /** Runtime for drawing. */
  struct uiLayout *layout;

  char panelname[/*BKE_ST_MAXNAME*/ 64];
  /** Panel name is identifier for restoring location. */
  char *drawname;
  /** Offset within the region. */
  int ofsx, ofsy;
  /** Panel size including children. */
  int sizex, sizey;
  /** Panel size excluding children. */
  int blocksizex, blocksizey;
  short labelofs;
  short flag, runtime_flag;
  char _pad[6];
  /** Panels are aligned according to increasing sort-order. */
  int sortorder;
  /** Runtime for panel manipulation. */
  void *activedata;
  /** Sub panels. */
  ListBase children;

  /**
   * List of #LayoutPanelState. This stores the open-close-state of layout-panels created with
   * `layout.panel(...)` in Python. For more information on layout-panels, see
   * `uiLayout::panel_prop`.
   */
  ListBase layout_panel_states;
  /**
   * This is increased whenever a layout panel state is used by the UI. This is used to allow for
   * some garbage collection of panel states when #layout_panel_states becomes large. It works by
   * removing all least-recently-used panel states up to a certain threshold.
   */
  uint32_t layout_panel_states_clock;
  char _pad2[4];

  struct Panel_Runtime *runtime;
} Panel;

/**
 * Used for passing expansion between instanced panel data and the panels themselves.
 * There are 16 defines because the expansion data is typically stored in a short.
 *
 * \note Expansion for instanced panels is stored in depth first order. For example, the value of
 * UI_SUBPANEL_DATA_EXPAND_2 correspond to mean the expansion of the second sub-panel or the first
 * sub-panel's first sub-panel.
 */
typedef enum uiPanelDataExpansion {
  UI_PANEL_DATA_EXPAND_ROOT = (1 << 0),
  UI_SUBPANEL_DATA_EXPAND_1 = (1 << 1),
  UI_SUBPANEL_DATA_EXPAND_2 = (1 << 2),
  UI_SUBPANEL_DATA_EXPAND_3 = (1 << 3),
  UI_SUBPANEL_DATA_EXPAND_4 = (1 << 4),
  UI_SUBPANEL_DATA_EXPAND_5 = (1 << 5),
  UI_SUBPANEL_DATA_EXPAND_6 = (1 << 6),
  UI_SUBPANEL_DATA_EXPAND_7 = (1 << 7),
  UI_SUBPANEL_DATA_EXPAND_8 = (1 << 8),
  UI_SUBPANEL_DATA_EXPAND_9 = (1 << 9),
  UI_SUBPANEL_DATA_EXPAND_10 = (1 << 10),
  UI_SUBPANEL_DATA_EXPAND_11 = (1 << 11),
  UI_SUBPANEL_DATA_EXPAND_12 = (1 << 12),
  UI_SUBPANEL_DATA_EXPAND_13 = (1 << 13),
  UI_SUBPANEL_DATA_EXPAND_14 = (1 << 14),
  UI_SUBPANEL_DATA_EXPAND_15 = (1 << 15),
} uiPanelDataExpansion;

/**
 * Notes on Panel Categories:
 *
 * - #ARegion.panels_category (#PanelCategoryDyn)
 *   is a runtime only list of categories collected during draw.
 *
 * - #ARegion.panels_category_active (#PanelCategoryStack)
 *   is basically a list of strings (category id's).
 *
 * Clicking on a tab moves it to the front of region->panels_category_active,
 * If the context changes so this tab is no longer displayed,
 * then the first-most tab in #ARegion.panels_category_active is used.
 *
 * This way you can change modes and always have the tab you last clicked on.
 */

/* region level tabs */
#
#
typedef struct PanelCategoryDyn {
  struct PanelCategoryDyn *next, *prev;
  char idname[64];
  rcti rect;
} PanelCategoryDyn;

/** Region stack of active tabs. */
typedef struct PanelCategoryStack {
  struct PanelCategoryStack *next, *prev;
  char idname[64];
} PanelCategoryStack;

typedef void (*uiListFreeRuntimeDataFunc)(struct uiList *ui_list);

/* uiList dynamic data... */
/* These two lines with # tell `makesdna` this struct can be excluded. */
#
#
typedef struct uiListDyn {
  /** Callback to free UI data when freeing UI-Lists in BKE. */
  uiListFreeRuntimeDataFunc free_runtime_data_fn;

  /** Number of rows needed to draw all elements. */
  int height;
  /** Actual visual height of the list (in rows). */
  int visual_height;
  /** Minimal visual height of the list (in rows). */
  int visual_height_min;

  /** Number of columns drawn for grid layouts. */
  int columns;

  /** Number of items in collection. */
  int items_len;
  /** Number of items actually visible after filtering. */
  int items_shown;

  /* Those are temp data used during drag-resize with GRIP button
   * (they are in pixels, the meaningful data is the
   * difference between resize_prev and resize)...
   */
  int resize;
  int resize_prev;

  /** Allocated custom data. Freed together with the #uiList (and when re-assigning). */
  void *customdata;

  /* Filtering data. */
  /** This bit-field is effectively exposed in Python, and scripts are explicitly allowed to assign
   * any own meaning to the lower 16 ones.
   * #items_len length. */
  int *items_filter_flags;
  /** Org_idx -> new_idx, items_len length. */
  int *items_filter_neworder;

  struct wmOperatorType *custom_drag_optype;
} uiListDyn;

typedef struct uiList { /* some list UI data need to be saved in file */
  struct uiList *next, *prev;

  /** Runtime. */
  struct uiListType *type;

  char list_id[/*UI_MAX_NAME_STR*/ 256];

  /** How items are laid out in the list. */
  int layout_type;
  int flag;

  int list_scroll;
  int list_grip;
  int list_last_len;
  int list_last_activei;

  /* Filtering data. */
  /** Defined as . */
  char filter_byname[/*UI_MAX_NAME_STR*/ 256];
  int filter_flag;
  int filter_sort_flag;

  /** Custom sub-classes properties. */
  IDProperty *properties;

  /** Dynamic data (runtime). */
  uiListDyn *dyn_data;
} uiList;

typedef enum uiViewStateFlag {
  UI_VIEW_SHOW_FILTER_OPTIONS = (1 << 0),
} uiViewStateFlag;

/** See #uiViewStateLink. */
typedef struct uiViewState {
  /**
   * User set height of the view in unscaled pixels. A value of 0 means no custom height was set
   * and the default should be used.
   */
  int custom_height;
  /**
   * Amount of vertical scrolling. View types decide on the unit:
   * - Tree views: Number of items scrolled out of view (#scroll_offset of 5 means 5 items are
   *   scrolled out of view).
   */
  int scroll_offset;
  uint16_t flag; /* #uiViewStateFlag */
  char _pad[6];

  char search_string[/*UI_MAX_NAME_STR*/ 256];
} uiViewState;

/**
 * Persistent storage for some state of views (#ui::AbstractView), for storage in a region. The
 * view state is matched to the view using the view's idname.
 *
 * The actual state is stored in #uiViewState, so views can manage this conveniently without having
 * to care about the idname and listbase pointers themselves.
 */
typedef struct uiViewStateLink {
  struct uiViewStateLink *next, *prev;

  char idname[/*BKE_ST_MAXNAME*/ 64];

  uiViewState state;
} uiViewStateLink;

typedef struct TransformOrientation {
  struct TransformOrientation *next, *prev;
  char name[/*MAX_NAME*/ 64];
  float mat[3][3];
  char _pad[4];
} TransformOrientation;

/** Some preview UI data need to be saved in file. */
typedef struct uiPreview {
  struct uiPreview *next, *prev;

  char preview_id[/*BKE_ST_MAXNAME*/ 64];
  short height;

  /* Unset on file read. */
  short tag; /* #uiPreviewTag */

  /** #ID.session_uid of the ID this preview is made for. Unset on file read. */
  unsigned int id_session_uid;
} uiPreview;

typedef enum uiPreviewTag {
  /** Preview needs re-rendering, handled in #ED_preview_draw(). */
  UI_PREVIEW_TAG_DIRTY = (1 << 0),
} uiPreviewTag;

typedef struct ScrGlobalAreaData {
  /**
   * Global areas have a non-dynamic size. That means, changing the window size doesn't
   * affect their size at all. However, they can still be 'collapsed', by changing this value.
   * Ignores DPI (#ED_area_global_size_y and winx/winy don't).
   */
  short cur_fixed_height;
  /**
   * For global areas, this is the min and max size they can use depending on
   * if they are 'collapsed' or not.
   */
  short size_min, size_max;
  /** GlobalAreaAlign. */
  short align;

  /** GlobalAreaFlag. */
  short flag;
  char _pad[2];
} ScrGlobalAreaData;

enum GlobalAreaFlag {
  GLOBAL_AREA_IS_HIDDEN = (1 << 0),
};

typedef enum GlobalAreaAlign {
  GLOBAL_AREA_ALIGN_TOP = 0,
  GLOBAL_AREA_ALIGN_BOTTOM = 1,
} GlobalAreaAlign;

typedef struct ScrArea_Runtime {
  struct bToolRef *tool;
  char is_tool_set;
  char _pad0[7];
} ScrArea_Runtime;

typedef struct ScrArea {
  DNA_DEFINE_CXX_METHODS(ScrArea)

  struct ScrArea *next, *prev;

  /** Ordered (bottom-left, top-left, top-right, bottom-right). */
  ScrVert *v1, *v2, *v3, *v4;
  /** If area==full, this is the parent. */
  bScreen *full;

  /** Rect bound by v1 v2 v3 v4. */
  rcti totrct;

  /** eSpace_Type (SPACE_FOO). */
  char spacetype;
  /**
   * eSpace_Type (SPACE_FOO).
   *
   * Temporarily used while switching area type, otherwise this should be SPACE_EMPTY.
   * Also, versioning uses it to nicely replace deprecated editors.
   * It's been there for ages, name doesn't fit any more.
   */
  char butspacetype;
  short butspacetype_subtype;

  /** Size. */
  short winx, winy;

  /** OLD! 0=no header, 1= down, 2= up. */
  char headertype DNA_DEPRECATED;
  /** Private, for spacetype refresh callback. */
  char do_refresh;
  short flag;
  /**
   * Index of last used region of 'RGN_TYPE_WINDOW'
   * runtime variable, updated by executing operators.
   */
  short region_active_win;
  char _pad[2];

  /** Callbacks for this space type. */
  struct SpaceType *type;

  /** Non-NULL if this area is global. */
  ScrGlobalAreaData *global;

  /**
   * #SpaceLink.
   * A list of space links (editors) that were open in this area before. When
   * changing the editor type, we try to reuse old editor data from this list.
   * The first item is the active/visible one.
   */
  ListBase spacedata;
  /**
   * #ARegion.
   * \note This region list is the one from the active/visible editor (first item in
   * spacedata list). Use SpaceLink.regionbase if it's inactive (but only then)!
   */
  ListBase regionbase;
  /** #wmEventHandler. */
  ListBase handlers;

  /** #AZone. */
  ListBase actionzones;

  ScrArea_Runtime runtime;
} ScrArea;

typedef struct ARegion {
  struct ARegion *next, *prev;

  /** 2D-View scrolling/zoom info (most regions are 2d anyways). */
  View2D v2d;
  /** Coordinates of region. */
  rcti winrct;
  /** Size. */
  short winx, winy;
  /**
   * This is a Y offset on the panel tabs that represents pixels,
   * where zero represents no scroll - the first category always shows first at the top.
   */
  int category_scroll;

  /** Window, header, etc. identifier for drawing. */
  short regiontype;
  /** How it should split. */
  short alignment;
  /** Hide, .... */
  short flag;

  /** Current split size in unscaled pixels (if zero it uses regiontype).
   * To convert to pixels use: `UI_SCALE_FAC * region->sizex + 0.5f`.
   * However to get the current region size, you should usually use winx/winy from above, not this!
   */
  short sizex, sizey;

  /** Private, set for indicate drawing overlapped. */
  short overlap;
  /** Temporary copy of flag settings for clean full-screen. */
  short flagfullscreen;

  char _pad[2];

  /** Panel. */
  ListBase panels;
  /** Stack of panel categories. */
  ListBase panels_category_active;
  /** #uiList. */
  ListBase ui_lists;
  /** #uiPreview. */
  ListBase ui_previews;
  /**
   * Permanent state storage of #ui::AbstractView instances, so hiding regions with views or
   * loading files remembers the view state.
   */
  ListBase view_states; /* #uiViewStateLink */

  /** XXX 2.50, need spacedata equivalent? */
  void *regiondata;

  ARegionRuntimeHandle *runtime;
} ARegion;

/** #ScrArea.flag */
enum {
  HEADER_NO_PULLDOWN = (1 << 0),
//  AREA_FLAG_UNUSED_1           = (1 << 1),
//  AREA_FLAG_UNUSED_2           = (1 << 2),
#ifdef DNA_DEPRECATED_ALLOW
  AREA_TEMP_INFO = (1 << 3), /* versioned to make slot reusable */
#endif
  /** Update size of regions within the area. */
  AREA_FLAG_REGION_SIZE_UPDATE = (1 << 3),
  AREA_FLAG_ACTIVE_TOOL_UPDATE = (1 << 4),
  // AREA_FLAG_UNUSED_5 = (1 << 5),

  AREA_FLAG_UNUSED_6 = (1 << 6), /* cleared */

  /**
   * For temporary full-screens (file browser, image editor render)
   * that are opened above user set full-screens.
   */
  AREA_FLAG_STACKED_FULLSCREEN = (1 << 7),
  /** Update action zones (even if the mouse is not intersecting them). */
  AREA_FLAG_ACTIONZONES_UPDATE = (1 << 8),
  /** For off-screen areas. */
  AREA_FLAG_OFFSCREEN = (1 << 9),
};

#define AREAGRID 1
#define AREAMINX 29
#define HEADER_PADDING_Y 6
#define HEADERY (20 + HEADER_PADDING_Y)

/** #bScreen.flag */
enum {
  SCREEN_DEPRECATED = 1,
  SCREEN_COLLAPSE_STATUSBAR = 2,
};

/** #bScreen.state */
enum {
  SCREENNORMAL = 0,
  /** One editor taking over the screen. */
  SCREENMAXIMIZED = 1,
  /**
   * One editor taking over the screen with no bare-minimum UI elements.
   *
   * Besides making the area full-screen this disables navigation & statistics because
   * this is part of a stereo 3D pipeline where these elements would interfere, see: !142418.
   */
  SCREENFULL = 2,
};

/** #bScreen.fullscreen_flag */
typedef enum eScreen_Fullscreen_Flag {
  FULLSCREEN_RESTORE_GIZMO_NAVIGATE = (1 << 0),
  FULLSCREEN_RESTORE_TEXT = (1 << 1),
  FULLSCREEN_RESTORE_STATS = (1 << 2),
} eScreen_Fullscreen_Flag;

/** #bScreen.redraws_flag */
typedef enum eScreen_Redraws_Flag {
  TIME_REGION = (1 << 0),
  TIME_ALL_3D_WIN = (1 << 1),
  TIME_ALL_ANIM_WIN = (1 << 2),
  TIME_ALL_BUTS_WIN = (1 << 3),
  // TIME_WITH_SEQ_AUDIO    = (1 << 4), /* DEPRECATED */
  TIME_SEQ = (1 << 5),
  TIME_ALL_IMAGE_WIN = (1 << 6),
  // TIME_CONTINUE_PHYSICS  = (1 << 7), /* UNUSED */
  TIME_NODES = (1 << 8),
  TIME_CLIPS = (1 << 9),
  TIME_SPREADSHEETS = (1 << 10),

  TIME_FOLLOW = (1 << 15),
} eScreen_Redraws_Flag;

/** #Panel.flag */
enum {
  PNL_SELECT = (1 << 0),
  PNL_UNUSED_1 = (1 << 1), /* Cleared */
  PNL_CLOSED = (1 << 2),
  // PNL_TABBED = (1 << 3),  /* UNUSED */
  // PNL_OVERLAP = (1 << 4), /* UNUSED */
  PNL_PIN = (1 << 5),
  PNL_POPOVER = (1 << 6),
  /** The panel has been drag-drop reordered and the instanced panel list needs to be rebuilt. */
  PNL_INSTANCED_LIST_ORDER_CHANGED = (1 << 7),
};

/** Fallback panel category (only for old scripts which need updating). */
#define PNL_CATEGORY_FALLBACK "Misc"

/** #uiList.layout_type */
enum {
  UILST_LAYOUT_DEFAULT = 0,
  UILST_LAYOUT_COMPACT = 1,
  UILST_LAYOUT_BIG_PREVIEW_GRID = 3,
};

/** #uiList.flag */
enum {
  /** Scroll list to make active item visible. */
  UILST_SCROLL_TO_ACTIVE_ITEM = 1 << 0,
};

/** Value (in number of items) we have to go below minimum shown items to enable auto size. */
#define UI_LIST_AUTO_SIZE_THRESHOLD 1

/** uiList filter flags (dyn_data)
 *
 * \warning Lower 16 bits are meant for custom use in Python, don't use them here! Only use the
 *          higher 16 bits.
 * \warning Those values are used by integer RNA too, which does not handle well values > INT_MAX.
 *          So please do not use 32nd bit here.
 */
enum {
  /* Don't use (1 << 0) to (1 << 15) here! See warning above. */

  /* Filtering returned #UI_LIST_ITEM_NEVER_SHOW. */
  UILST_FLT_ITEM_NEVER_SHOW = (1 << 16),
  UILST_FLT_ITEM = 1 << 30, /* This item has passed the filter process successfully. */
};

/** #uiList.filter_flag */
enum {
  UILST_FLT_SHOW = 1 << 0,            /* Show filtering UI. */
  UILST_FLT_EXCLUDE = UILST_FLT_ITEM, /* Exclude filtered items, *must* use this same value. */
};

/** #uiList.filter_sort_flag */
enum {
  /* Plain values (only one is valid at a time, once masked with UILST_FLT_SORT_MASK. */
  /** Just for sake of consistency. */
  /* UILST_FLT_SORT_INDEX = 0, */ /* UNUSED */
  UILST_FLT_SORT_ALPHA = 1,

  /* Bitflags affecting behavior of any kind of sorting. */
  /** Special flag to indicate that order is locked (not user-changeable). */
  UILST_FLT_SORT_LOCK = 1u << 30,
  /** Special value, bit-flag used to reverse order! */
  UILST_FLT_SORT_REVERSE = 1u << 31,
};

#define UILST_FLT_SORT_MASK (((unsigned int)(UILST_FLT_SORT_REVERSE | UILST_FLT_SORT_LOCK)) - 1)

/**
 * regiontype, first two are the default set.
 * \warning Do NOT change order, append on end. Types are hard-coded needed.
 */
typedef enum eRegion_Type {
  RGN_TYPE_WINDOW = 0,
  RGN_TYPE_HEADER = 1,
  RGN_TYPE_CHANNELS = 2,
  RGN_TYPE_TEMPORARY = 3,
  RGN_TYPE_UI = 4,
  RGN_TYPE_TOOLS = 5,
  RGN_TYPE_TOOL_PROPS = 6,
  RGN_TYPE_PREVIEW = 7,
  RGN_TYPE_HUD = 8,
  /* Region to navigate the main region from (RGN_TYPE_WINDOW). */
  RGN_TYPE_NAV_BAR = 9,
  /* A place for buttons to trigger execution of something that was set up in other regions. */
  RGN_TYPE_EXECUTE = 10,
  RGN_TYPE_FOOTER = 11,
  RGN_TYPE_TOOL_HEADER = 12,
  /* Region type used exclusively by internal code and add-ons to register draw callbacks to the XR
   * context (surface, mirror view). Does not represent any real region. */
  RGN_TYPE_XR = 13,
  RGN_TYPE_ASSET_SHELF = 14,
  RGN_TYPE_ASSET_SHELF_HEADER = 15,

#define RGN_TYPE_NUM (RGN_TYPE_ASSET_SHELF_HEADER + 1)
} eRegion_Type;

/** Use for function args. */
#define RGN_TYPE_ANY -1

/** Region supports panel tabs (categories). */
#define RGN_TYPE_HAS_CATEGORY_MASK (1 << RGN_TYPE_UI)

/** Check for any kind of header region. */
#define RGN_TYPE_IS_HEADER_ANY(regiontype) \
  (((1 << (regiontype)) & ((1 << RGN_TYPE_HEADER) | 1 << (RGN_TYPE_TOOL_HEADER) | \
                           (1 << RGN_TYPE_FOOTER) | (1 << RGN_TYPE_ASSET_SHELF_HEADER))) != 0)

/** #ARegion.alignment */
enum {
  RGN_ALIGN_NONE = 0,
  RGN_ALIGN_TOP = 1,
  RGN_ALIGN_BOTTOM = 2,
  RGN_ALIGN_LEFT = 3,
  RGN_ALIGN_RIGHT = 4,
  RGN_ALIGN_HSPLIT = 5,
  RGN_ALIGN_VSPLIT = 6,
  RGN_ALIGN_FLOAT = 7,
  RGN_ALIGN_QSPLIT = 8,
  /* Maximum 15. */

  /* Flags start here. */
  /** Region is split into the previous one, they share the same space along a common edge.
   * Includes the #RGN_ALIGN_HIDE_WITH_PREV behavior. */
  RGN_SPLIT_PREV = 1 << 5,
  /** Always let scaling this region scale the previous region instead. Useful to let regions
   * appear like they are one (while having independent layout, scrolling, etc.). */
  RGN_SPLIT_SCALE_PREV = 1 << 6,
  /** Whenever the previous region is hidden, this region becomes invisible too. #RGN_FLAG_HIDDEN
   * should only be set for the previous region, not this. The evaluated visibility respecting this
   * flag can be queried via #ARegion.visible */
  RGN_ALIGN_HIDE_WITH_PREV = 1 << 7,
};

/** Mask out flags so we can check the alignment. */
#define RGN_ALIGN_ENUM_FROM_MASK(align) ((align) & ((1 << 4) - 1))
#define RGN_ALIGN_FLAG_FROM_MASK(align) ((align) & ~((1 << 4) - 1))

/** #ARegion.flag */
enum {
  RGN_FLAG_HIDDEN = (1 << 0),
  RGN_FLAG_TOO_SMALL = (1 << 1),
  /** Enable dynamically changing the region size in the #ARegionType::layout() callback. */
  RGN_FLAG_DYNAMIC_SIZE = (1 << 2),
  /** Region data is NULL'd on read, never written. */
  RGN_FLAG_TEMP_REGIONDATA = (1 << 3),
  /** Region resizing by the user is disabled, but the region edge can still be dragged to
   * hide/unhide the region. */
  RGN_FLAG_NO_USER_RESIZE = (1 << 4),
  /** Size has been clamped (floating regions only). */
  RGN_FLAG_SIZE_CLAMP_X = (1 << 5),
  RGN_FLAG_SIZE_CLAMP_Y = (1 << 6),
  /** When the user sets the region is hidden,
   * needed for floating regions that may be hidden for other reasons. */
  RGN_FLAG_HIDDEN_BY_USER = (1 << 7),
  /** Property search filter is active. */
  RGN_FLAG_SEARCH_FILTER_ACTIVE = (1 << 8),
  /**
   * Update the expansion of the region's panels and switch contexts. Only Set
   * temporarily when the search filter is updated and cleared at the end of the
   * region's layout pass. so that expansion is still interactive,
   */
  RGN_FLAG_SEARCH_FILTER_UPDATE = (1 << 9),
  /** #ARegionType.poll() failed for the current context, and the region should be treated as if it
   * wouldn't exist. Runtime only flag. */
  RGN_FLAG_POLL_FAILED = (1 << 10),
  RGN_FLAG_RESIZE_RESPECT_BUTTON_SECTIONS = (1 << 11),
  RGN_FLAG_INDICATE_OVERFLOW = (1 << 12),
};

/** #ARegion.do_draw */
enum {
  /** Region must be fully redrawn. */
  RGN_DRAW = 1,
  /**
   * Redraw only part of region, for sculpting and painting to get smoother
   * stroke painting on heavy meshes.
   */
  RGN_DRAW_PARTIAL = 2,
  /**
   * For outliner, to do faster redraw without rebuilding outliner tree.
   * For 3D viewport, to display a new progressive render sample without
   * while other buffers and overlays remain unchanged.
   */
  RGN_DRAW_NO_REBUILD = 4,

  /** Set while region is being drawn. */
  RGN_DRAWING = 8,
  /** For popups, to refresh UI layout along with drawing. */
  RGN_REFRESH_UI = 16,

  /** Only editor overlays (currently gizmos only!) should be redrawn. */
  RGN_DRAW_EDITOR_OVERLAYS = 32,
};

typedef struct AssetShelfSettings {
  AssetLibraryReference asset_library_reference;

  ListBase enabled_catalog_paths; /* #AssetCatalogPathLink */
  /** If not set (null or empty string), all assets will be displayed ("All" catalog behavior). */
  const char *active_catalog_path;

  /** For filtering assets displayed in the asset view. */
  char search_string[64];

  short preview_size;
  short display_flag; /* #AssetShelfSettings_DisplayFlag */
  char _pad1[4];

#ifdef __cplusplus
  /* Zero initializes. */
  AssetShelfSettings();
  /* Proper deep copy. */
  AssetShelfSettings(const AssetShelfSettings &other);
  AssetShelfSettings &operator=(const AssetShelfSettings &other);
  ~AssetShelfSettings();
#endif
} AssetShelfSettings;

typedef struct AssetShelf {
  DNA_DEFINE_CXX_METHODS(AssetShelf)

  struct AssetShelf *next, *prev;

  /** Identifier that matches the #AssetShelfType.idname this shelf was created with. Used to
   * restore the #AssetShelf.type pointer below on file read. */
  char idname[/*MAX_NAME*/ 64];
  /** Runtime. */
  struct AssetShelfType *type;

  AssetShelfSettings settings;

  /** Only for the permanent asset shelf regions, not asset shelves in temporary popups. */
  short preferred_row_count;
  short instance_flag;
  char _pad[4];
} AssetShelf;

/**
 * Region-data for the main asset shelf region (#RGN_TYPE_ASSET_SHELF). Managed by the asset shelf
 * internals.
 *
 * Contains storage for all previously activated asset shelf instances plus info on the currently
 * active one (only one can be active at any time).
 */
typedef struct RegionAssetShelf {
  /** Owning list of previously activated asset shelves. */
  ListBase shelves;
  /**
   * The currently active shelf, if any. Updated on redraw, so that context changes are reflected.
   * Note that this may still be set even though the shelf isn't available anymore
   * (#AssetShelfType.poll() fails). The pointer isn't necessarily unset when polling.
   */
  AssetShelf *active_shelf; /* Non-owning. */
#ifdef __cplusplus
  static RegionAssetShelf *get_from_asset_shelf_region(const ARegion &region);
  /** Creates the asset shelf region data if necessary, and returns it. */
  static RegionAssetShelf *ensure_from_asset_shelf_region(ARegion &region);
#endif
} RegionAssetShelf;

/* #AssetShelfSettings.display_flag */
typedef enum AssetShelfSettings_DisplayFlag {
  ASSETSHELF_SHOW_NAMES = (1 << 0),
} AssetShelfSettings_DisplayFlag;
ENUM_OPERATORS(AssetShelfSettings_DisplayFlag);

/* #AssetShelfSettings.instance_flag */
typedef enum AssetShelf_InstanceFlag {
  /**
   * Remember the last known region visibility state or this shelf, so it can be restored if the
   * shelf is reactivated. Practically this makes the shelf visibility be remembered per mode.
   * Continuously updated for the visible region.
   */
  ASSETSHELF_REGION_IS_HIDDEN = (1 << 0),
} AssetShelf_InstanceFlag;
ENUM_OPERATORS(AssetShelf_InstanceFlag);

typedef struct FileHandler {
  DNA_DEFINE_CXX_METHODS(FileHandler)
  /** Runtime. */
  FileHandlerTypeHandle *type;
} FileHandler;
