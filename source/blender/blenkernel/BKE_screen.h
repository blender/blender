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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_SCREEN_H__
#define __BKE_SCREEN_H__

/** \file BKE_screen.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

struct ARegion;
struct Header;
struct ID;
struct ListBase;
struct Menu;
struct Panel;
struct Scene;
struct ScrArea;
struct ScrVert;
struct SpaceType;
struct TransformOrientation;
struct View3D;
struct View3DShading;
struct bContext;
struct bContextDataResult;
struct bScreen;
struct uiLayout;
struct uiList;
struct wmKeyConfig;
struct wmGizmoMap;
struct wmNotifier;
struct wmWindow;
struct wmWindowManager;
struct WorkSpace;
struct GPUFXSettings;
struct wmMsgBus;
struct ScrAreaMap;

#include "BLI_compiler_attrs.h"

#include "RNA_types.h"

/* spacetype has everything stored to get an editor working, it gets initialized via
 * ED_spacetypes_init() in editors/space_api/spacetypes.c   */
/* an editor in Blender is a combined ScrArea + SpaceType + SpaceData */

#define BKE_ST_MAXNAME  64

typedef struct SpaceType {
	struct SpaceType *next, *prev;

	char name[BKE_ST_MAXNAME];                  /* for menus */
	int spaceid;                                /* unique space identifier */
	int iconid;                                 /* icon lookup for menus */

	/* Initial allocation, after this WM will call init() too. Some editors need
	 * area and scene data (e.g. frame range) to set their initial scrolling. */
	struct SpaceLink *(*new)(const struct ScrArea *sa, const struct Scene *scene);
	/* not free spacelink itself */
	void (*free)(struct SpaceLink *sl);

	/* init is to cope with file load, screen (size) changes, check handlers */
	void (*init)(struct wmWindowManager *wm, struct ScrArea *sa);
	/* exit is called when the area is hidden or removed */
	void (*exit)(struct wmWindowManager *wm, struct ScrArea *sa);
	/* Listeners can react to bContext changes */
	void (*listener)(struct wmWindow *win, struct ScrArea *sa,
	                 struct wmNotifier *wmn, struct Scene *scene);

	/* refresh context, called after filereads, ED_area_tag_refresh() */
	void (*refresh)(const struct bContext *C, struct ScrArea *sa);

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
	int (*context)(const struct bContext *C, const char *member, struct bContextDataResult *result);

	/* Used when we want to replace an ID by another (or NULL). */
	void (*id_remap)(struct ScrArea *sa, struct SpaceLink *sl, struct ID *old_id, struct ID *new_id);

	int  (*space_subtype_get)(struct ScrArea *sa);
	void (*space_subtype_set)(struct ScrArea *sa, int value);
	void (*space_subtype_item_extend)(struct bContext *C, EnumPropertyItem **item, int *totitem);

	/* region type definitions */
	ListBase regiontypes;

	/* read and write... */

	/* default keymaps to add */
	int keymapflag;

} SpaceType;

/* region types are also defined using spacetypes_init, via a callback */

typedef struct ARegionType {
	struct ARegionType *next, *prev;

	int regionid;           /* unique identifier within this space, defines RGN_TYPE_xxxx */

	/* add handlers, stuff you only do once or on area/region type/size changes */
	void (*init)(struct wmWindowManager *wm, struct ARegion *ar);
	/* exit is called when the region is hidden or removed */
	void (*exit)(struct wmWindowManager *wm, struct ARegion *ar);
	/* draw entirely, view changes should be handled here */
	void (*draw)(const struct bContext *C, struct ARegion *ar);
	/* optional, compute button layout before drawing for dynamic size */
	void (*layout)(const struct bContext *C, struct ARegion *ar);
	/* snap the size of the region (can be NULL for no snapping). */
	int (*snap_size)(const struct ARegion *ar, int size, int axis);
	/* contextual changes should be handled here */
	void (*listener)(struct wmWindow *win, struct ScrArea *sa, struct ARegion *ar,
	                 struct wmNotifier *wmn, const struct Scene *scene);
	/* Optional callback to generate subscriptions. */
	void (*message_subscribe)(
	        const struct bContext *C,
	        struct WorkSpace *workspace, struct Scene *scene,
	        struct bScreen *sc, struct ScrArea *sa, struct ARegion *ar,
	        struct wmMsgBus *mbus);

	void (*free)(struct ARegion *);

	/* split region, copy data optionally */
	void *(*duplicate)(void *poin);


	/* register operator types on startup */
	void (*operatortypes)(void);
	/* add own items to keymap */
	void (*keymap)(struct wmKeyConfig *keyconf);
	/* allows default cursor per region */
	void (*cursor)(struct wmWindow *win, struct ScrArea *sa, struct ARegion *ar);

	/* return context data */
	int (*context)(const struct bContext *C, const char *member, struct bContextDataResult *result);

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
	/* return without drawing. lock is set by region definition, and copied to do_lock by render. can become flag */
	short do_lock, lock;
	/* call cursor function on each move event */
	short event_cursor;
} ARegionType;

/* panel types */

typedef struct PanelType {
	struct PanelType *next, *prev;

	char idname[BKE_ST_MAXNAME];              /* unique name */
	char label[BKE_ST_MAXNAME];               /* for panel header */
	char translation_context[BKE_ST_MAXNAME];
	char context[BKE_ST_MAXNAME];             /* for buttons window */
	char category[BKE_ST_MAXNAME];            /* for category tabs */
	char owner_id[BKE_ST_MAXNAME];            /* for work-spaces to selectively show. */
	char parent_id[BKE_ST_MAXNAME];           /* parent idname for subpanels */
	short space_type;
	short region_type;
	/* For popovers, 0 for default. */
	int ui_units_x;

	int flag;

	/* verify if the panel should draw or not */
	bool (*poll)(const struct bContext *C, struct PanelType *pt);
	/* draw header (optional) */
	void (*draw_header)(const struct bContext *C, struct Panel *pa);
	/* draw header preset (optional) */
	void (*draw_header_preset)(const struct bContext *C, struct Panel *pa);
	/* draw entirely, view changes should be handled here */
	void (*draw)(const struct bContext *C, struct Panel *pa);

	/* sub panels */
	struct PanelType *parent;
	ListBase children;

	/* RNA integration */
	ExtensionRNA ext;
} PanelType;

/* uilist types */

/* Draw an item in the uiList */
typedef void (*uiListDrawItemFunc)(
        struct uiList *ui_list, struct bContext *C, struct uiLayout *layout, struct PointerRNA *dataptr,
        struct PointerRNA *itemptr, int icon, struct PointerRNA *active_dataptr, const char *active_propname,
        int index, int flt_flag);

/* Draw the filtering part of an uiList */
typedef void (*uiListDrawFilterFunc)(
        struct uiList *ui_list, struct bContext *C, struct uiLayout *layout);

/* Filter items of an uiList */
typedef void (*uiListFilterItemsFunc)(
        struct uiList *ui_list, struct bContext *C, struct PointerRNA *, const char *propname);

typedef struct uiListType {
	struct uiListType *next, *prev;

	char idname[BKE_ST_MAXNAME];            /* unique name */

	uiListDrawItemFunc draw_item;
	uiListDrawFilterFunc draw_filter;
	uiListFilterItemsFunc filter_items;

	/* RNA integration */
	ExtensionRNA ext;
} uiListType;

/* header types */

typedef struct HeaderType {
	struct HeaderType *next, *prev;

	char idname[BKE_ST_MAXNAME];        /* unique name */
	int space_type;
	int region_type;

	/* draw entirely, view changes should be handled here */
	void (*draw)(const struct bContext *C, struct Header *header);

	/* RNA integration */
	ExtensionRNA ext;
} HeaderType;

typedef struct Header {
	struct HeaderType *type;    /* runtime */
	struct uiLayout *layout;    /* runtime for drawing */
} Header;


/* menu types */

typedef struct MenuType {
	struct MenuType *next, *prev;

	char idname[BKE_ST_MAXNAME];        /* unique name */
	char label[BKE_ST_MAXNAME];         /* for button text */
	char translation_context[BKE_ST_MAXNAME];
	char owner_id[BKE_ST_MAXNAME];  /* optional, see: #wmOwnerID */
	const char *description;

	/* verify if the menu should draw or not */
	bool (*poll)(const struct bContext *C, struct MenuType *mt);
	/* draw entirely, view changes should be handled here */
	void (*draw)(const struct bContext *C, struct Menu *menu);

	/* RNA integration */
	ExtensionRNA ext;
} MenuType;

typedef struct Menu {
	struct MenuType *type;      /* runtime */
	struct uiLayout *layout;    /* runtime for drawing */
} Menu;

/* spacetypes */
struct SpaceType *BKE_spacetype_from_id(int spaceid);
struct ARegionType *BKE_regiontype_from_id_or_first(struct SpaceType *st, int regionid);
struct ARegionType *BKE_regiontype_from_id(struct SpaceType *st, int regionid);
const struct ListBase *BKE_spacetypes_list(void);
void BKE_spacetype_register(struct SpaceType *st);
bool BKE_spacetype_exists(int spaceid);
void BKE_spacetypes_free(void); /* only for quitting blender */

/* spacedata */
void BKE_spacedata_freelist(ListBase *lb);
void BKE_spacedata_copylist(ListBase *lb1, ListBase *lb2);
void BKE_spacedata_draw_locks(int set);

void BKE_spacedata_callback_id_remap_set(
        void (*func)(struct ScrArea *sa, struct SpaceLink *sl, struct ID *old_id, struct ID *new_id));
void BKE_spacedata_id_unref(struct ScrArea *sa, struct SpaceLink *sl, struct ID *id);

/* area/regions */
struct ARegion *BKE_area_region_copy(struct SpaceType *st, struct ARegion *ar);
void            BKE_area_region_free(struct SpaceType *st, struct ARegion *ar);
void            BKE_area_region_panels_free(struct ListBase *panels);
void            BKE_screen_area_free(struct ScrArea *sa);
/* Gizmo-maps of a region need to be freed with the region. Uses callback to avoid low-level call. */
void BKE_region_callback_free_gizmomap_set(void (*callback)(struct wmGizmoMap *));
void BKE_region_callback_refresh_tag_gizmomap_set(void (*callback)(struct wmGizmoMap *));

struct ARegion *BKE_area_find_region_type(struct ScrArea *sa, int type);
struct ARegion *BKE_area_find_region_active_win(struct ScrArea *sa);
struct ARegion *BKE_area_find_region_xy(struct ScrArea *sa, const int regiontype, int x, int y);
struct ScrArea *BKE_screen_find_area_from_space(struct bScreen *sc, struct SpaceLink *sl) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);
struct ScrArea *BKE_screen_find_big_area(struct bScreen *sc, const int spacetype, const short min);
struct ScrArea *BKE_screen_area_map_find_area_xy(const struct ScrAreaMap *areamap, const int spacetype, int x, int y);
struct ScrArea *BKE_screen_find_area_xy(struct bScreen *sc, const int spacetype, int x, int y);

unsigned int BKE_screen_view3d_layer_active_ex(
        const struct View3D *v3d, const struct Scene *scene, bool use_localvd) ATTR_NONNULL(2);
unsigned int BKE_screen_view3d_layer_active(
        const struct View3D *v3d, const struct Scene *scene) ATTR_NONNULL(2);

unsigned int BKE_screen_view3d_layer_all(const struct bScreen *sc) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

void BKE_screen_gizmo_tag_refresh(struct bScreen *sc);

void BKE_screen_view3d_sync(struct View3D *v3d, struct Scene *scene);
void BKE_screen_view3d_scene_sync(struct bScreen *sc, struct Scene *scene);
bool BKE_screen_is_fullscreen_area(const struct bScreen *screen) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BKE_screen_is_used(const struct bScreen *screen) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* zoom factor conversion */
float BKE_screen_view3d_zoom_to_fac(float camzoom);
float BKE_screen_view3d_zoom_from_fac(float zoomfac);

void BKE_screen_view3d_shading_init(struct View3DShading *shading);

/* screen */
void BKE_screen_free(struct bScreen *sc);
void BKE_screen_area_map_free(struct ScrAreaMap *area_map) ATTR_NONNULL();
unsigned int BKE_screen_visible_layers(struct bScreen *screen, struct Scene *scene);

struct ScrEdge *BKE_screen_find_edge(struct bScreen *sc, struct ScrVert *v1, struct ScrVert *v2);
void BKE_screen_sort_scrvert(struct ScrVert **v1, struct ScrVert **v2);
void BKE_screen_remove_double_scrverts(struct bScreen *sc);
void BKE_screen_remove_double_scredges(struct bScreen *sc);
void BKE_screen_remove_unused_scredges(struct bScreen *sc);
void BKE_screen_remove_unused_scrverts(struct bScreen *sc);

#endif
