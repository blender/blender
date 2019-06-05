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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 */

#ifndef __WM_EVENT_SYSTEM_H__
#define __WM_EVENT_SYSTEM_H__

/* return value of handler-operator call */
#define WM_HANDLER_CONTINUE 0
#define WM_HANDLER_BREAK 1
#define WM_HANDLER_HANDLED 2
#define WM_HANDLER_MODAL 4 /* MODAL|BREAK means unhandled */

struct ARegion;
struct ScrArea;

/* wmKeyMap is in DNA_windowmanager.h, it's saveable */

/** Custom types for handlers, for signaling, freeing */
enum eWM_EventHandlerType {
  WM_HANDLER_TYPE_GIZMO = 1,
  WM_HANDLER_TYPE_UI,
  WM_HANDLER_TYPE_OP,
  WM_HANDLER_TYPE_DROPBOX,
  WM_HANDLER_TYPE_KEYMAP,
};

typedef bool (*EventHandlerPoll)(const ARegion *ar, const wmEvent *event);

typedef struct wmEventHandler {
  struct wmEventHandler *next, *prev;

  enum eWM_EventHandlerType type;
  char flag; /* WM_HANDLER_BLOCKING, ... */

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

/* goes over entire hierarchy:  events -> window -> screen -> area -> region */
void wm_event_do_handlers(bContext *C);

void wm_event_add_ghostevent(wmWindowManager *wm, wmWindow *win, int type, void *customdata);

void wm_event_do_depsgraph(bContext *C, bool is_after_open_file);
void wm_event_do_refresh_wm_and_depsgraph(bContext *C);
void wm_event_do_notifiers(bContext *C);

float wm_pressure_curve(float raw_pressure);

/* wm_keymap.c */

/* wm_dropbox.c */
void wm_dropbox_free(void);
void wm_drags_check_ops(bContext *C, const wmEvent *event);
void wm_drags_draw(bContext *C, wmWindow *win, rcti *rect);

#endif /* __WM_EVENT_SYSTEM_H__ */
