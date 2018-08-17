/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_screen_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_SCREEN_TYPES_H__
#define __DNA_SCREEN_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_view2d_types.h"
#include "DNA_vec_types.h"

#include "DNA_ID.h"

struct SpaceType;
struct SpaceLink;
struct ARegion;
struct ARegionType;
struct PanelType;
struct Scene;
struct uiLayout;
struct wmDrawBuffer;
struct wmTimer;
struct wmTooltipState;


/* TODO Doing this is quite ugly :)
 * Once the top-bar is merged bScreen should be refactored to use ScrAreaMap. */
#define AREAMAP_FROM_SCREEN(screen) ((ScrAreaMap *)&(screen)->vertbase)

typedef struct bScreen {
	ID id;

	/* TODO Should become ScrAreaMap now.
	 * ** NOTE: KEEP ORDER IN SYNC WITH ScrAreaMap! (see AREAMAP_FROM_SCREEN macro above) ** */
	ListBase vertbase;					/* screens have vertices/edges to define areas */
	ListBase edgebase;
	ListBase areabase;

	ListBase regionbase;				/* screen level regions (menus), runtime only */

	struct Scene *scene DNA_DEPRECATED;

	short flag;                         /* general flags */
	short winid;						/* winid from WM, starts with 1 */
	short redraws_flag;					/* user-setting for which editors get redrawn during anim playback (used to be time->redraws) */

	char temp;							/* temp screen in a temp window, don't save (like user prefs) */
	char state;							/* temp screen for image render display or fileselect */
	char do_draw;						/* notifier for drawing edges */
	char do_refresh;					/* notifier for scale screen, changed screen, etc */
	char do_draw_gesture;				/* notifier for gesture draw. */
	char do_draw_paintcursor;			/* notifier for paint cursor draw. */
	char do_draw_drag;					/* notifier for dragging draw. */
	char skip_handling;					/* set to delay screen handling after switching back from maximized area */
	char scrubbing;						/* set when scrubbing to avoid some costly updates */
	char pad[1];

	struct ARegion *active_region;		/* active region that has mouse focus */

	struct wmTimer *animtimer;			/* if set, screen has timer handler added in window */
	void *context;						/* context callback */

	struct wmTooltipState *tool_tip;	/* runtime */

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
	short border;			/* 1 when at edge of screen */
	short flag;
	int pad;
} ScrEdge;

typedef struct ScrAreaMap {
	/* ** NOTE: KEEP ORDER IN SYNC WITH LISTBASES IN bScreen! ** */

	ListBase vertbase;  /* ScrVert - screens have vertices/edges to define areas */
	ListBase edgebase;  /* ScrEdge */
	ListBase areabase;  /* ScrArea */
} ScrAreaMap;

typedef struct Panel {		/* the part from uiBlock that needs saved in file */
	struct Panel *next, *prev;

	struct PanelType *type;			/* runtime */
	struct uiLayout *layout;		/* runtime for drawing */

	char panelname[64], tabname[64];	/* defined as UI_MAX_NAME_STR */
	char drawname[64];					/* panelname is identifier for restoring location */
	int ofsx, ofsy;                     /* offset within the region */
	int sizex, sizey;                   /* panel size including children */
	int blocksizex, blocksizey;         /* panel size excluding children */
	short labelofs, pad;
	short flag, runtime_flag;
	short control;
	short snap;
	int sortorder;			/* panels are aligned according to increasing sortorder */
	struct Panel *paneltab;		/* this panel is tabbed in *paneltab */
	void *activedata;			/* runtime for panel manipulation */
	ListBase children;          /* sub panels */
} Panel;


/* Notes on Panel Catogories:
 *
 * ar->panels_category (PanelCategoryDyn) is a runtime only list of categories collected during draw.
 *
 * ar->panels_category_active (PanelCategoryStack) is basically a list of strings (category id's).
 *
 * Clicking on a tab moves it to the front of ar->panels_category_active,
 * If the context changes so this tab is no longer displayed,
 * then the first-most tab in ar->panels_category_active is used.
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

/* region stack of active tabs */
typedef struct PanelCategoryStack {
	struct PanelCategoryStack *next, *prev;
	char idname[64];
} PanelCategoryStack;


/* uiList dynamic data... */
/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct uiListDyn {
	int height;                   /* Number of rows needed to draw all elements. */
	int visual_height;            /* Actual visual height of the list (in rows). */
	int visual_height_min;        /* Minimal visual height of the list (in rows). */

	int items_len;                /* Number of items in collection. */
	int items_shown;              /* Number of items actually visible after filtering. */

	/* Those are temp data used during drag-resize with GRIP button (they are in pixels, the meaningful data is the
	 * difference between resize_prev and resize)...
	 */
	int resize;
	int resize_prev;

	/* Filtering data. */
	int *items_filter_flags;      /* items_len length. */
	int *items_filter_neworder;   /* org_idx -> new_idx, items_len length. */
} uiListDyn;

typedef struct uiList {           /* some list UI data need to be saved in file */
	struct uiList *next, *prev;

	struct uiListType *type;      /* runtime */

	char list_id[64];             /* defined as UI_MAX_NAME_STR */

	int layout_type;              /* How items are layedout in the list */
	int flag;

	int list_scroll;
	int list_grip;
	int list_last_len;
	int list_last_activei;

	/* Filtering data. */
	char filter_byname[64];       /* defined as UI_MAX_NAME_STR */
	int filter_flag;
	int filter_sort_flag;

	/* Custom sub-classes properties. */
	IDProperty *properties;

	/* Dynamic data (runtime). */
	uiListDyn *dyn_data;
} uiList;

typedef struct TransformOrientation {
	struct TransformOrientation *next, *prev;
	char name[64];	/* MAX_NAME */
	float mat[3][3];
	int pad;
} TransformOrientation;

typedef struct uiPreview {           /* some preview UI data need to be saved in file */
	struct uiPreview *next, *prev;

	char preview_id[64];             /* defined as UI_MAX_NAME_STR */
	short height;
	short pad1[3];
} uiPreview;

/* These two lines with # tell makesdna this struct can be excluded.
 * Should be: #ifndef WITH_GLOBAL_AREA_WRITING */
#
#
typedef struct ScrGlobalAreaData {
	/* Global areas have a non-dynamic size. That means, changing the window
	 * size doesn't affect their size at all. However, they can still be
	 * 'collapsed', by changing this value. Ignores DPI (ED_area_global_size_y
	 * and winx/winy don't) */
	short cur_fixed_height;
	/* For global areas, this is the min and max size they can use depending on
	 * if they are 'collapsed' or not. Value is set on area creation and not
	 * touched afterwards. */
	short size_min, size_max;
	short align; /* GlobalAreaAlign */

	short flag; /* GlobalAreaFlag */
	short pad;
} ScrGlobalAreaData;

enum GlobalAreaFlag {
	GLOBAL_AREA_IS_HIDDEN = (1 << 0),
};

typedef enum GlobalAreaAlign {
	GLOBAL_AREA_ALIGN_TOP,
	GLOBAL_AREA_ALIGN_BOTTOM,
} GlobalAreaAlign;

typedef struct ScrArea_Runtime {
	struct bToolRef *tool;
	char          is_tool_set;
	char _pad0[7];
} ScrArea_Runtime;

typedef struct ScrArea {
	struct ScrArea *next, *prev;

	ScrVert *v1, *v2, *v3, *v4;		/* ordered (bl, tl, tr, br) */
	bScreen *full;			/* if area==full, this is the parent */

	rcti totrct;			/* rect bound by v1 v2 v3 v4 */

	char spacetype;     /* eSpace_Type (SPACE_FOO) */
	/* Temporarily used while switching area type, otherwise this should be
	 * SPACE_EMPTY. Also, versioning uses it to nicely replace deprecated
	 * editors. It's been there for ages, name doesn't fit any more... */
	char butspacetype;  /* eSpace_Type (SPACE_FOO) */
	short butspacetype_subtype;

	short winx, winy;				/* size */

	char headertype DNA_DEPRECATED;/* OLD! 0=no header, 1= down, 2= up */
	char do_refresh;				/* private, for spacetype refresh callback */
	short flag;
	short region_active_win;		/* index of last used region of 'RGN_TYPE_WINDOW'
									 * runtime variable, updated by executing operators */
	char temp, pad;

	struct SpaceType *type;		/* callbacks for this space type */

	/* Non-NULL if this area is global. */
	ScrGlobalAreaData *global;

	/* A list of space links (editors) that were open in this area before. When
	 * changing the editor type, we try to reuse old editor data from this list.
	 * The first item is the active/visible one.
	 */
	ListBase spacedata;  /* SpaceLink */
	/* NOTE: This region list is the one from the active/visible editor (first item in
	 * spacedata list). Use SpaceLink.regionbase if it's inactive (but only then)!
	 */
	ListBase regionbase; /* ARegion */
	ListBase handlers;   /* wmEventHandler */

	ListBase actionzones;	/* AZone */

	ScrArea_Runtime runtime;
} ScrArea;


typedef struct ARegion_Runtime {
	/* Panel category to use between 'layout' and 'draw'. */
	const char *category;
} ARegion_Runtime;

typedef struct ARegion {
	struct ARegion *next, *prev;

	View2D v2d;					/* 2D-View scrolling/zoom info (most regions are 2d anyways) */
	rcti winrct;				/* coordinates of region */
	rcti drawrct;				/* runtime for partial redraw, same or smaller than winrct */
	short winx, winy;			/* size */

	short visible;              /* region is currently visible on screen */
	short regiontype;			/* window, header, etc. identifier for drawing */
	short alignment;			/* how it should split */
	short flag;					/* hide, ... */

	float fsize;				/* current split size in float (unused) */
	short sizex, sizey;			/* current split size in pixels (if zero it uses regiontype) */

	short do_draw;				/* private, cached notifier events */
	short do_draw_overlay;		/* private, cached notifier events */
	short overlap;				/* private, set for indicate drawing overlapped */
	short flagfullscreen;		/* temporary copy of flag settings for clean fullscreen */
	short pad1, pad2;

	struct ARegionType *type;	/* callbacks for this region type */

	ListBase uiblocks;			/* uiBlock */
	ListBase panels;			/* Panel */
	ListBase panels_category_active;	/* Stack of panel categories */
	ListBase ui_lists;			/* uiList */
	ListBase ui_previews;		/* uiPreview */
	ListBase handlers;			/* wmEventHandler */
	ListBase panels_category;	/* Panel categories runtime */

	struct wmGizmoMap *gizmo_map; /* gizmo-map of this region */
	struct wmTimer *regiontimer; /* blend in/out */
	struct wmDrawBuffer *draw_buffer;

	char *headerstr;			/* use this string to draw info */
	void *regiondata;			/* XXX 2.50, need spacedata equivalent? */

	ARegion_Runtime runtime;
} ARegion;

/* area->flag */
enum {
	HEADER_NO_PULLDOWN           = (1 << 0),
//	AREA_FLAG_DEPRECATED_1       = (1 << 1),
//	AREA_FLAG_DEPRECATED_2       = (1 << 2),
#ifdef DNA_DEPRECATED_ALLOW
	AREA_TEMP_INFO               = (1 << 3), /* versioned to make slot reusable */
#endif
	/* update size of regions within the area */
	AREA_FLAG_REGION_SIZE_UPDATE = (1 << 3),
//	AREA_FLAG_DEPRECATED_4       = (1 << 4),
//	AREA_FLAG_DEPRECATED_5       = (1 << 5),
	/* used to check if we should switch back to prevspace (of a different type) */
	AREA_FLAG_TEMP_TYPE          = (1 << 6),
	/* for temporary fullscreens (file browser, image editor render) that are opened above user set fullscreens */
	AREA_FLAG_STACKED_FULLSCREEN = (1 << 7),
	/* update action zones (even if the mouse is not intersecting them) */
	AREA_FLAG_ACTIONZONES_UPDATE = (1 << 8),
};

#define EDGEWIDTH	1
#define AREAGRID	4
#define AREAMINX	32
#define HEADERY		26

/* screen->flag */
enum {
	SCREEN_COLLAPSE_TOPBAR    = 1,
	SCREEN_COLLAPSE_STATUSBAR = 2,
};

/* screen->state */
enum {
	SCREENNORMAL     = 0,
	SCREENMAXIMIZED  = 1, /* one editor taking over the screen */
	SCREENFULL       = 2, /* one editor taking over the screen with no bare-minimum UI elements */
};

/* Panel->flag */
enum {
	PNL_SELECT      = (1 << 0),
	PNL_CLOSEDX     = (1 << 1),
	PNL_CLOSEDY     = (1 << 2),
	PNL_CLOSED      = (PNL_CLOSEDX | PNL_CLOSEDY),
	/*PNL_TABBED    = (1 << 3), */ /*UNUSED*/
	PNL_OVERLAP     = (1 << 4),
	PNL_PIN         = (1 << 5),
	PNL_POPOVER     = (1 << 6),
};

/* Panel->snap - for snapping to screen edges */
#define PNL_SNAP_NONE		0
/* #define PNL_SNAP_TOP		1 */
/* #define PNL_SNAP_RIGHT		2 */
#define PNL_SNAP_BOTTOM		4
/* #define PNL_SNAP_LEFT		8 */

/* #define PNL_SNAP_DIST		9.0 */

/* paneltype flag */
#define PNL_DEFAULT_CLOSED		1
#define PNL_NO_HEADER			2

/* Fallback panel category (only for old scripts which need updating) */
#define PNL_CATEGORY_FALLBACK "Misc"

/* uiList layout_type */
enum {
	UILST_LAYOUT_DEFAULT          = 0,
	UILST_LAYOUT_COMPACT          = 1,
	UILST_LAYOUT_GRID             = 2,
};

/* uiList flag */
enum {
	UILST_SCROLL_TO_ACTIVE_ITEM   = 1 << 0,          /* Scroll list to make active item visible. */
};

/* Value (in number of items) we have to go below minimum shown items to enable auto size. */
#define UI_LIST_AUTO_SIZE_THRESHOLD 1

/* uiList filter flags (dyn_data) */
/* WARNING! Those values are used by integer RNA too, which does not handle well values > INT_MAX...
 *          So please do not use 32nd bit here. */
enum {
	UILST_FLT_ITEM      = 1 << 30,  /* This item has passed the filter process successfully. */
};

/* uiList filter options */
enum {
	UILST_FLT_SHOW      = 1 << 0,          /* Show filtering UI. */
	UILST_FLT_EXCLUDE   = UILST_FLT_ITEM,  /* Exclude filtered items, *must* use this same value. */
};

/* uiList filter orderby type */
enum {
	UILST_FLT_SORT_ALPHA        = 1 << 0,
	UILST_FLT_SORT_REVERSE      = 1u << 31  /* Special value, bitflag used to reverse order! */
};

#define UILST_FLT_SORT_MASK (((unsigned int)UILST_FLT_SORT_REVERSE) - 1)

/* regiontype, first two are the default set */
/* Do NOT change order, append on end. Types are hardcoded needed */
enum {
	RGN_TYPE_WINDOW = 0,
	RGN_TYPE_HEADER = 1,
	RGN_TYPE_CHANNELS = 2,
	RGN_TYPE_TEMPORARY = 3,
	RGN_TYPE_UI = 4,
	RGN_TYPE_TOOLS = 5,
	RGN_TYPE_TOOL_PROPS = 6,
	RGN_TYPE_PREVIEW = 7,
	RGN_TYPE_HUD = 8,
};
/* use for function args */
#define RGN_TYPE_ANY -1

/* region alignment */
#define RGN_ALIGN_NONE		0
#define RGN_ALIGN_TOP		1
#define RGN_ALIGN_BOTTOM	2
#define RGN_ALIGN_LEFT		3
#define RGN_ALIGN_RIGHT		4
#define RGN_ALIGN_HSPLIT	5
#define RGN_ALIGN_VSPLIT	6
#define RGN_ALIGN_FLOAT		7
#define RGN_ALIGN_QSPLIT	8

#define RGN_SPLIT_PREV		32

/* region flag */
enum {
	RGN_FLAG_HIDDEN             = (1 << 0),
	RGN_FLAG_TOO_SMALL          = (1 << 1),
	/* Force delayed reinit of region size data, so that region size is calculated
	 * just big enough to show all its content (if enough space is available).
	 * Note that only ED_region_header supports this right now. */
	RGN_FLAG_DYNAMIC_SIZE       = (1 << 2),
	/* Region data is NULL'd on read, never written. */
	RGN_FLAG_TEMP_REGIONDATA    = (1 << 3),
};

/* region do_draw */
#define RGN_DRAW			1
#define RGN_DRAW_PARTIAL	2
#define RGN_DRAWING			4
#define RGN_DRAW_REFRESH_UI	8  /* re-create uiBlock's where possible */
#define RGN_DRAW_NO_REBUILD	16
#endif
