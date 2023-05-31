/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

struct ARegion;
struct GHOST_TabletData;
struct ScrArea;
enum wmOperatorCallContext;

#ifdef WITH_XR_OPENXR
struct wmXrActionData;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* wmKeyMap is in DNA_windowmanager.h, it's saveable */

/** Custom types for handlers, for signaling, freeing */
typedef enum eWM_EventHandlerType {
  WM_HANDLER_TYPE_GIZMO = 1,
  WM_HANDLER_TYPE_UI,
  WM_HANDLER_TYPE_OP,
  WM_HANDLER_TYPE_DROPBOX,
  WM_HANDLER_TYPE_KEYMAP,
} eWM_EventHandlerType;

typedef bool (*EventHandlerPoll)(const ARegion *region, const wmEvent *event);

typedef struct wmEventHandler {
  struct wmEventHandler *next, *prev;

  eWM_EventHandlerType type;
  eWM_EventHandlerFlag flag;

  EventHandlerPoll poll;
} wmEventHandler;

/** Run after the keymap item runs. */
struct wmEventHandler_KeymapPost {
  void (*post_fn)(wmKeyMap *keymap, wmKeyMapItem *kmi, void *user_data);
  void *user_data;
};

/** Support for a getter function that looks up the keymap each access. */
struct wmEventHandler_KeymapDynamic {
  wmEventHandler_KeymapDynamicFn *keymap_fn;
  void *user_data;
};

/** #WM_HANDLER_TYPE_KEYMAP */
typedef struct wmEventHandler_Keymap {
  wmEventHandler head;

  /** Pointer to builtin/custom keymaps (never NULL). */
  wmKeyMap *keymap;

  struct wmEventHandler_KeymapPost post;
  struct wmEventHandler_KeymapDynamic dynamic;

  struct bToolRef *keymap_tool;
} wmEventHandler_Keymap;

/** #WM_HANDLER_TYPE_GIZMO */
typedef struct wmEventHandler_Gizmo {
  wmEventHandler head;

  /** Gizmo handler (never NULL). */
  struct wmGizmoMap *gizmo_map;
} wmEventHandler_Gizmo;

/** #WM_HANDLER_TYPE_UI */
typedef struct wmEventHandler_UI {
  wmEventHandler head;

  wmUIHandlerFunc handle_fn;       /* callback receiving events */
  wmUIHandlerRemoveFunc remove_fn; /* callback when handler is removed */
  void *user_data;                 /* user data pointer */

  /** Store context for this handler for derived/modal handlers. */
  struct {
    struct ScrArea *area;
    struct ARegion *region;
    struct ARegion *menu;
  } context;
} wmEventHandler_UI;

/** #WM_HANDLER_TYPE_OP */
typedef struct wmEventHandler_Op {
  wmEventHandler head;

  /** Operator can be NULL. */
  wmOperator *op;

  /** Hack, special case for file-select. */
  bool is_fileselect;

  /** Store context for this handler for derived/modal handlers. */
  struct {
    /* To override the window, and hence the screen. Set for few cases only, usually window/screen
     * can be taken from current context. */
    struct wmWindow *win;

    struct ScrArea *area;
    struct ARegion *region;
    short region_type;
  } context;
} wmEventHandler_Op;

/** #WM_HANDLER_TYPE_DROPBOX */
typedef struct wmEventHandler_Dropbox {
  wmEventHandler head;

  /** Never NULL. */
  ListBase *dropboxes;
} wmEventHandler_Dropbox;

/* wm_event_system.c */

void wm_event_free_all(wmWindow *win);
void wm_event_free(wmEvent *event);
void wm_event_free_handler(wmEventHandler *handler);

/**
 * Goes over entire hierarchy: events -> window -> screen -> area -> region.
 *
 * \note Called in main loop.
 */
void wm_event_do_handlers(bContext *C);

/**
 * Windows store own event queues #wmWindow.event_queue (no #bContext here).
 */
void wm_event_add_ghostevent(wmWindowManager *wm, wmWindow *win, int type, void *customdata);
#ifdef WITH_XR_OPENXR
void wm_event_add_xrevent(wmWindow *win, struct wmXrActionData *actiondata, short val);
#endif

void wm_event_do_depsgraph(bContext *C, bool is_after_open_file);
/**
 * Was part of #wm_event_do_notifiers,
 * split out so it can be called once before entering the #WM_main loop.
 * This ensures operators don't run before the UI and depsgraph are initialized.
 */
void wm_event_do_refresh_wm_and_depsgraph(bContext *C);
/**
 * Called in main-loop.
 */
void wm_event_do_notifiers(bContext *C);

void wm_event_handler_ui_cancel_ex(bContext *C,
                                   wmWindow *win,
                                   ARegion *region,
                                   bool reactivate_button);

/* wm_event_query.c */

/**
 * Applies the global tablet pressure correction curve.
 */
float wm_pressure_curve(float raw_pressure);
void wm_tablet_data_from_ghost(const struct GHOST_TabletData *tablet_data, wmTabletData *wmtab);

/* wm_dropbox.c */

void wm_dropbox_free(void);
/**
 * Additional work to cleanly end dragging. Additional because this doesn't actually remove the
 * drag items. Should be called whenever dragging is stopped
 * (successful or not, also when canceled).
 */
void wm_drags_exit(wmWindowManager *wm, wmWindow *win);
void wm_drop_prepare(bContext *C, wmDrag *drag, wmDropBox *drop);
void wm_drop_end(bContext *C, wmDrag *drag, wmDropBox *drop);
/**
 * Called in inner handler loop, region context.
 */
void wm_drags_check_ops(bContext *C, const wmEvent *event);
/**
 * The operator of a dropbox should always be executed in the context determined by the mouse
 * coordinates. The dropbox poll should check the context area and region as needed.
 * So this always returns #WM_OP_INVOKE_DEFAULT.
 */
wmOperatorCallContext wm_drop_operator_context_get(const wmDropBox *drop);
/**
 * Called in #wm_draw_window_onscreen.
 */
void wm_drags_draw(bContext *C, wmWindow *win);

#ifdef __cplusplus
}
#endif
