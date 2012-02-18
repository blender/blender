/* xdnd.c, xdnd.h - C program library for handling the Xdnd protocol
   Copyright (C) 1996-2000 Paul Sheer

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.
 */


/* 
   Released 1998-08-07
   Changes:

   2000-08-08: INCR protocol implemented.

*/

/*
    DONE:
     - INCR protocol now implemented

    TODO:
     - action_choose_dialog not yet supported (never called)
     - widget_delete_selection not yet supported and DELETE requests are ignored
     - not yet tested with applications that only supported XDND 0 or 1
*/

#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#include "xdnd.h"

static void xdnd_send_enter (DndClass * dnd, Window window, Window from, Atom * typelist);
static void xdnd_send_position (DndClass * dnd, Window window, Window from, Atom action, int x, int y,
                                unsigned long etime);
static void xdnd_send_status (DndClass * dnd, Window window, Window from, int will_accept, int want_position,
                              int x, int y, int w, int h, Atom action);
static void xdnd_send_leave (DndClass * dnd, Window window, Window from);
static void xdnd_send_drop (DndClass * dnd, Window window, Window from, unsigned long etime);
static void xdnd_send_finished (DndClass * dnd, Window window, Window from, int error);
static int xdnd_convert_selection (DndClass * dnd, Window window, Window requester, Atom type);
static void xdnd_selection_send (DndClass * dnd, XSelectionRequestEvent * request, unsigned char *data,
                                 int length);
static int xdnd_get_selection (DndClass * dnd, Window from, Atom property, Window insert);


/* just to remind us : */

#if 0
typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Atom message_type;
    int format;
    union {
        char b[20];
        short s[10];
        long l[5];
    } data;
} XClientMessageEvent;
XClientMessageEvent xclient;
#endif

/* #define DND_DEBUG */

#define xdnd_xfree(x) {if (x) { free (x); x = 0; }}

#ifdef DND_DEBUG

#include <sys/time.h>
#include <unistd.h>

char *xdnd_debug_milliseconds (void)
{
    struct timeval tv;
    static char r[22];
    gettimeofday (&tv, 0);
    sprintf (r, "%.2ld.%.3ld", tv.tv_sec % 100L, tv.tv_usec / 1000L);
    return r;
}

#define dnd_debug1(a)       printf("%s: %d: %s: " a "\n", __FILE__, __LINE__, xdnd_debug_milliseconds ())
#define dnd_debug2(a,b)     printf("%s: %d: %s: " a "\n", __FILE__, __LINE__, xdnd_debug_milliseconds (), b)
#define dnd_debug3(a,b,c)   printf("%s: %d: %s: " a "\n", __FILE__, __LINE__, xdnd_debug_milliseconds (), b, c)
#define dnd_debug4(a,b,c,d) printf("%s: %d: %s: " a "\n", __FILE__, __LINE__, xdnd_debug_milliseconds (), b, c, d)
#else
#define dnd_debug1(a)       
#define dnd_debug2(a,b)     
#define dnd_debug3(a,b,c)   
#define dnd_debug4(a,b,c,d) 
#endif

#define dnd_warning(a) fprintf (stderr, a)

#define dnd_version_at_least(a,b) ((a) >= (b))

static unsigned char dnd_copy_cursor_bits[] =
{
  0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0x00, 0x02, 0x00, 0x08, 0x01,
  0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0xe8, 0x0f,
  0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0x08, 0x01,
  0x02, 0x00, 0x08, 0x00, 0x02, 0x04, 0x08, 0x00, 0x02, 0x0c, 0x08, 0x00,
  0x02, 0x1c, 0x08, 0x00, 0x02, 0x3c, 0x08, 0x00, 0x02, 0x7c, 0x08, 0x00,
  0x02, 0xfc, 0x08, 0x00, 0x02, 0xfc, 0x09, 0x00, 0x02, 0xfc, 0x0b, 0x00,
  0x02, 0x7c, 0x08, 0x00, 0xfe, 0x6d, 0x0f, 0x00, 0x00, 0xc4, 0x00, 0x00,
  0x00, 0xc0, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00};

static unsigned char dnd_copy_mask_bits[] =
{
  0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f,
  0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
  0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
  0x07, 0x06, 0xfc, 0x1f, 0x07, 0x0e, 0xfc, 0x1f, 0x07, 0x1e, 0x1c, 0x00,
  0x07, 0x3e, 0x1c, 0x00, 0x07, 0x7e, 0x1c, 0x00, 0x07, 0xfe, 0x1c, 0x00,
  0x07, 0xfe, 0x1d, 0x00, 0x07, 0xfe, 0x1f, 0x00, 0x07, 0xfe, 0x1f, 0x00,
  0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0x1e, 0x00, 0xff, 0xef, 0x1f, 0x00,
  0x00, 0xe6, 0x01, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x00, 0xc0, 0x03, 0x00,
  0x00, 0x80, 0x01, 0x00};

static unsigned char dnd_move_cursor_bits[] =
{
  0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0x02, 0x00, 0x08, 0x02, 0x00, 0x08,
  0x02, 0x00, 0x08, 0x02, 0x00, 0x08, 0x02, 0x00, 0x08, 0x02, 0x00, 0x08,
  0x02, 0x00, 0x08, 0x02, 0x00, 0x08, 0x02, 0x04, 0x08, 0x02, 0x0c, 0x08,
  0x02, 0x1c, 0x08, 0x02, 0x3c, 0x08, 0x02, 0x7c, 0x08, 0x02, 0xfc, 0x08,
  0x02, 0xfc, 0x09, 0x02, 0xfc, 0x0b, 0x02, 0x7c, 0x08, 0xfe, 0x6d, 0x0f,
  0x00, 0xc4, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01,
  0x00, 0x00, 0x00};

static unsigned char dnd_move_mask_bits[] =
{
  0xff, 0xff, 0x1f, 0xff, 0xff, 0x1f, 0xff, 0xff, 0x1f, 0x07, 0x00, 0x1c,
  0x07, 0x00, 0x1c, 0x07, 0x00, 0x1c, 0x07, 0x00, 0x1c, 0x07, 0x00, 0x1c,
  0x07, 0x00, 0x1c, 0x07, 0x06, 0x1c, 0x07, 0x0e, 0x1c, 0x07, 0x1e, 0x1c,
  0x07, 0x3e, 0x1c, 0x07, 0x7e, 0x1c, 0x07, 0xfe, 0x1c, 0x07, 0xfe, 0x1d,
  0x07, 0xfe, 0x1f, 0x07, 0xfe, 0x1f, 0xff, 0xff, 0x1f, 0xff, 0xff, 0x1e,
  0xff, 0xef, 0x1f, 0x00, 0xe6, 0x01, 0x00, 0xc0, 0x03, 0x00, 0xc0, 0x03,
  0x00, 0x80, 0x01};

static unsigned char dnd_link_cursor_bits[] =
{
  0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0x00, 0x02, 0x00, 0x08, 0x01,
  0x02, 0x00, 0x88, 0x00, 0x02, 0x00, 0x48, 0x00, 0x02, 0x00, 0xe8, 0x0f,
  0x02, 0x00, 0x48, 0x00, 0x02, 0x00, 0x88, 0x00, 0x02, 0x00, 0x08, 0x01,
  0x02, 0x00, 0x08, 0x00, 0x02, 0x04, 0x08, 0x00, 0x02, 0x0c, 0x08, 0x00,
  0x02, 0x1c, 0x08, 0x00, 0x02, 0x3c, 0x08, 0x00, 0x02, 0x7c, 0x08, 0x00,
  0x02, 0xfc, 0x08, 0x00, 0x02, 0xfc, 0x09, 0x00, 0x02, 0xfc, 0x0b, 0x00,
  0x02, 0x7c, 0x08, 0x00, 0xfe, 0x6d, 0x0f, 0x00, 0x00, 0xc4, 0x00, 0x00,
  0x00, 0xc0, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00};

static unsigned char dnd_link_mask_bits[] =
{
  0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f,
  0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
  0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
  0x07, 0x06, 0xfc, 0x1f, 0x07, 0x0e, 0xfc, 0x1f, 0x07, 0x1e, 0x1c, 0x00,
  0x07, 0x3e, 0x1c, 0x00, 0x07, 0x7e, 0x1c, 0x00, 0x07, 0xfe, 0x1c, 0x00,
  0x07, 0xfe, 0x1d, 0x00, 0x07, 0xfe, 0x1f, 0x00, 0x07, 0xfe, 0x1f, 0x00,
  0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0x1e, 0x00, 0xff, 0xef, 0x1f, 0x00,
  0x00, 0xe6, 0x01, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x00, 0xc0, 0x03, 0x00,
  0x00, 0x80, 0x01, 0x00};

static unsigned char dnd_ask_cursor_bits[] =
{
  0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0x00, 0x02, 0x00, 0x88, 0x03,
  0x02, 0x00, 0x48, 0x04, 0x02, 0x00, 0x08, 0x04, 0x02, 0x00, 0x08, 0x02,
  0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0x08, 0x00,
  0x02, 0x00, 0x08, 0x01, 0x02, 0x04, 0x08, 0x00, 0x02, 0x0c, 0x08, 0x00,
  0x02, 0x1c, 0x08, 0x00, 0x02, 0x3c, 0x08, 0x00, 0x02, 0x7c, 0x08, 0x00,
  0x02, 0xfc, 0x08, 0x00, 0x02, 0xfc, 0x09, 0x00, 0x02, 0xfc, 0x0b, 0x00,
  0x02, 0x7c, 0x08, 0x00, 0xfe, 0x6d, 0x0f, 0x00, 0x00, 0xc4, 0x00, 0x00,
  0x00, 0xc0, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00};

static unsigned char dnd_ask_mask_bits[] =
{
  0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f,
  0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
  0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
  0x07, 0x06, 0xfc, 0x1f, 0x07, 0x0e, 0xfc, 0x1f, 0x07, 0x1e, 0x1c, 0x00,
  0x07, 0x3e, 0x1c, 0x00, 0x07, 0x7e, 0x1c, 0x00, 0x07, 0xfe, 0x1c, 0x00,
  0x07, 0xfe, 0x1d, 0x00, 0x07, 0xfe, 0x1f, 0x00, 0x07, 0xfe, 0x1f, 0x00,
  0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0x1e, 0x00, 0xff, 0xef, 0x1f, 0x00,
  0x00, 0xe6, 0x01, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x00, 0xc0, 0x03, 0x00,
  0x00, 0x80, 0x01, 0x00};

static DndCursor dnd_cursors[] =
{
    {29, 25, 10, 10, dnd_copy_cursor_bits, dnd_copy_mask_bits, "XdndActionCopy", 0, 0, 0, 0},
    {21, 25, 10, 10, dnd_move_cursor_bits, dnd_move_mask_bits, "XdndActionMove", 0, 0, 0, 0},
    {29, 25, 10, 10, dnd_link_cursor_bits, dnd_link_mask_bits, "XdndActionLink", 0, 0, 0, 0},
    {29, 25, 10, 10, dnd_ask_cursor_bits, dnd_ask_mask_bits, "XdndActionAsk", 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

void xdnd_reset (DndClass * dnd)
{
    dnd->stage = XDND_DROP_STAGE_IDLE;
    dnd->dragging_version = 0;
    dnd->internal_drag = 0;
    dnd->want_position = 0;
    dnd->ready_to_drop = 0;
    dnd->will_accept = 0;
    dnd->rectangle.x = dnd->rectangle.y = 0;
    dnd->rectangle.width = dnd->rectangle.height = 0;
    dnd->dropper_window = 0;
    dnd->dropper_toplevel = 0;
    dnd->dragger_window = 0;
    dnd->dragger_typelist = 0;
    dnd->desired_type = 0;
    dnd->time = 0;
}

void xdnd_init (DndClass * dnd, Display * display)
{
    DndCursor *cursor;
    XColor black, white;
    memset (dnd, 0, sizeof (*dnd));

    dnd->display = display;
    dnd->root_window = DefaultRootWindow (display);
    dnd->version = XDND_VERSION;

    dnd->XdndAware = XInternAtom (dnd->display, "XdndAware", False);
    dnd->XdndSelection = XInternAtom (dnd->display, "XdndSelection", False);
    dnd->XdndEnter = XInternAtom (dnd->display, "XdndEnter", False);
    dnd->XdndLeave = XInternAtom (dnd->display, "XdndLeave", False);
    dnd->XdndPosition = XInternAtom (dnd->display, "XdndPosition", False);
    dnd->XdndDrop = XInternAtom (dnd->display, "XdndDrop", False);
    dnd->XdndFinished = XInternAtom (dnd->display, "XdndFinished", False);
    dnd->XdndStatus = XInternAtom (dnd->display, "XdndStatus", False);
    dnd->XdndActionCopy = XInternAtom (dnd->display, "XdndActionCopy", False);
    dnd->XdndActionMove = XInternAtom (dnd->display, "XdndActionMove", False);
    dnd->XdndActionLink = XInternAtom (dnd->display, "XdndActionLink", False);
    dnd->XdndActionAsk = XInternAtom (dnd->display, "XdndActionAsk", False);
    dnd->XdndActionPrivate = XInternAtom (dnd->display, "XdndActionPrivate", False);
    dnd->XdndTypeList = XInternAtom (dnd->display, "XdndTypeList", False);
    dnd->XdndActionList = XInternAtom (dnd->display, "XdndActionList", False);
    dnd->XdndActionDescription = XInternAtom (dnd->display, "XdndActionDescription", False);

    dnd->Xdnd_NON_PROTOCOL_ATOM = XInternAtom (dnd->display, "JXSelectionWindowProperty", False);

    xdnd_reset (dnd);

    dnd->cursors = dnd_cursors;

    black.pixel = BlackPixel (dnd->display, DefaultScreen (dnd->display));
    white.pixel = WhitePixel (dnd->display, DefaultScreen (dnd->display));

    XQueryColor (dnd->display, DefaultColormap (dnd->display, DefaultScreen (dnd->display)), &black);
    XQueryColor (dnd->display, DefaultColormap (dnd->display, DefaultScreen (dnd->display)), &white);

    for (cursor = &dnd->cursors[0]; cursor->width; cursor++) {
        cursor->image_pixmap = XCreateBitmapFromData \
            (dnd->display, dnd->root_window, (char *) cursor->image_data, cursor->width, cursor->height);
        cursor->mask_pixmap = XCreateBitmapFromData \
            (dnd->display, dnd->root_window, (char *) cursor->mask_data, cursor->width, cursor->height);
        cursor->cursor = XCreatePixmapCursor (dnd->display, cursor->image_pixmap,
              cursor->mask_pixmap, &black, &white, cursor->x, cursor->y);
        XFreePixmap (dnd->display, cursor->image_pixmap);
        XFreePixmap (dnd->display, cursor->mask_pixmap);
        cursor->action = XInternAtom (dnd->display, cursor->_action, False);
    }
}

void xdnd_shut (DndClass * dnd)
{
    DndCursor *cursor;
    for (cursor = &dnd->cursors[0]; cursor->width; cursor++)
        XFreeCursor (dnd->display, cursor->cursor);
    memset (dnd, 0, sizeof (*dnd));
    return;
}


/* typelist is a null terminated array */
static int array_length (Atom * a)
{
    int n;
    for (n = 0; a[n]; n++);
    return n;
}

void xdnd_set_dnd_aware (DndClass * dnd, Window window, Atom * typelist)
{
    Window root_return, parent;
    unsigned int nchildren_return;
    Window *children_return = 0;
    int r, s;
    if(!window) return;
    if (dnd->widget_exists)
        if (!(*dnd->widget_exists) (dnd, window))
            return;
    s = XChangeProperty (dnd->display, window, dnd->XdndAware, XA_ATOM, 32, PropModeReplace,
                         (unsigned char *) &dnd->version, 1);
#if 1
    dnd_debug4 ("XChangeProperty() = %d, window = %ld, widget = %s", s, window, "<WIDGET>");
#endif
    if (s && typelist) {
        int n;
        n = array_length (typelist);
        if (n)
            s = XChangeProperty (dnd->display, window, dnd->XdndAware, XA_ATOM, 32, PropModeAppend,
                                 (unsigned char *) typelist, n);
    }
    r =
        XQueryTree (dnd->display, window, &root_return, &parent, &children_return,
                    &nchildren_return);
    if (children_return)
        XFree (children_return);
    if (r)
        xdnd_set_dnd_aware (dnd, parent, typelist);
}

int xdnd_is_dnd_aware (DndClass * dnd, Window window, int *version, Atom * typelist)
{
    Atom actual;
    int format;
    unsigned long count, remaining;
    unsigned char *data = 0;
    Atom *types, *t;
    int result = 1;

    *version = 0;
    XGetWindowProperty (dnd->display, window, dnd->XdndAware,
                        0, 0x8000000L, False, XA_ATOM,
                        &actual, &format,
                        &count, &remaining, &data);

    if (actual != XA_ATOM || format != 32 || count == 0 || !data) {
        dnd_debug2 ("XGetWindowProperty failed in xdnd_is_dnd_aware - XdndAware = %ld", dnd->XdndAware);
        if (data)
            XFree (data);
        return 0;
    }
    types = (Atom *) data;
#if XDND_VERSION >= 3
    if (types[0] < 3) {
        if (data)
            XFree (data);
        return 0;
    }
#endif
    *version = dnd->version < types[0] ? dnd->version : types[0];        /* minimum */
    dnd_debug2 ("Using XDND version %d", *version);
    if (count > 1) {
        result = 0;
        for (t = typelist; *t; t++) {
            int j;
            for (j = 1; j < count; j++) {
                if (types[j] == *t) {
                    result = 1;
                    break;
                }
            }
            if (result)
                break;
        }
    }
    XFree (data);
    return result;
}

void xdnd_set_type_list (DndClass * dnd, Window window, Atom * typelist)
{
    int n;
    n = array_length (typelist);
    XChangeProperty (dnd->display, window, dnd->XdndTypeList, XA_ATOM, 32,
                     PropModeReplace, (unsigned char *) typelist, n);
}

/* result must be free'd */
void xdnd_get_type_list (DndClass * dnd, Window window, Atom ** typelist)
{
    Atom type, *a;
    int format, i;
    unsigned long count, remaining;
    unsigned char *data = NULL;

    *typelist = 0;

    XGetWindowProperty (dnd->display, window, dnd->XdndTypeList,
                        0, 0x8000000L, False, XA_ATOM,
                        &type, &format, &count, &remaining, &data);

    if (type != XA_ATOM || format != 32 || count == 0 || !data) {
        if (data)
            XFree (data);
        dnd_debug2 ("XGetWindowProperty failed in xdnd_get_type_list - dnd->XdndTypeList = %ld", dnd->XdndTypeList);
        return;
    }
    *typelist = malloc ((count + 1) * sizeof (Atom));
    a = (Atom *) data;
    for (i = 0; i < count; i++)
        (*typelist)[i] = a[i];
    (*typelist)[count] = 0;

    XFree (data);
}

void xdnd_get_three_types (DndClass * dnd, XEvent * xevent, Atom ** typelist)
{
    int i;
    *typelist = malloc ((XDND_THREE + 1) * sizeof (Atom));
    for (i = 0; i < XDND_THREE; i++)
        (*typelist)[i] = XDND_ENTER_TYPE (xevent, i);
    (*typelist)[XDND_THREE] = 0;        /* although (*typelist)[1] or (*typelist)[2] may also be set to nill */
}

/* result must be free'd */
static char *concat_string_list (char **t, int *bytes)
{
    int l, n;
    char *s;
    for (l = n = 0;; n++) {
        if (!t[n])
            break;
        if (!t[n][0])
            break;
        l += strlen (t[n]) + 1;
    }
    s = malloc (l + 1);
    for (l = n = 0;; n++) {
        if (!t[n])
            break;
        if (!(t[n][0]))
            break;
        strcpy (s + l, t[n]);
        l += strlen (t[n]) + 1;
    }
    *bytes = l;
    s[l] = '\0';
    return s;
}

void xdnd_set_actions (DndClass * dnd, Window window, Atom * actions, char **descriptions)
{
    int n, l;
    char *s;
    n = array_length (actions);

    XChangeProperty (dnd->display, window, dnd->XdndActionList, XA_ATOM, 32,
                     PropModeReplace, (unsigned char *) actions, n);

    s = concat_string_list (descriptions, &l);
    XChangeProperty (dnd->display, window, dnd->XdndActionList, XA_STRING, 8,
                     PropModeReplace, (unsigned char *) s, l);
    xdnd_xfree (s);
}

/* returns 1 on error or no actions, otherwise result must be free'd 
   xdnd_get_actions (window, &actions, &descriptions);
   free (actions); free (descriptions); */
int xdnd_get_actions (DndClass * dnd, Window window, Atom ** actions, char ***descriptions)
{
    Atom type, *a;
    int format, i;
    unsigned long count, dcount, remaining;
    unsigned char *data = 0, *r;

    *actions = 0;
    *descriptions = 0;
    XGetWindowProperty (dnd->display, window, dnd->XdndActionList,
                        0, 0x8000000L, False, XA_ATOM,
                        &type, &format, &count, &remaining, &data);

    if (type != XA_ATOM || format != 32 || count == 0 || !data) {
        if (data)
            XFree (data);
        return 1;
    }
    *actions = malloc ((count + 1) * sizeof (Atom));
    a = (Atom *) data;
    for (i = 0; i < count; i++)
        (*actions)[i] = a[i];
    (*actions)[count] = 0;

    XFree (data);

    data = 0;
    XGetWindowProperty (dnd->display, window, dnd->XdndActionDescription,
                        0, 0x8000000L, False, XA_STRING, &type, &format,
                        &dcount, &remaining, &data);

    if (type != XA_STRING || format != 8 || dcount == 0) {
        if (data)
            XFree (data);
        *descriptions = malloc ((count + 1) * sizeof (char *));
        dnd_warning ("XGetWindowProperty no property or wrong format for action descriptions");
        for (i = 0; i < count; i++)
            (*descriptions)[i] = "";
        (*descriptions)[count] = 0;
    } else {
        int l;
        l = (count + 1) * sizeof (char *);
        *descriptions = malloc (l + dcount);
        memcpy (*descriptions + l, data, dcount);
        XFree (data);
        data = (unsigned char *) *descriptions;
        data += l;
        l = 0;
        for (i = 0, r = data;; r += l + 1, i++) {
            l = strlen ((char *) r);
            if (!l || i >= count)
                break;
            (*descriptions)[i] = (char *) r;
        }
        for (; i < count; i++) {
            (*descriptions)[i] = "";
        }
        (*descriptions)[count] = 0;
    }
    return 0;
}

/* returns non-zero on cancel */
int xdnd_choose_action_dialog (DndClass * dnd, Atom * actions, char **descriptions, Atom * result)
{
    if (!actions[0])
        return 1;
    if (!dnd->action_choose_dialog) {        /* default to return the first action if no dialog set */
        *result = actions[0];
        return 0;
    }
    return (*dnd->action_choose_dialog) (dnd, descriptions, actions, result);
}

static void xdnd_send_event (DndClass * dnd, Window window, XEvent * xevent)
{
    dnd_debug4 ("xdnd_send_event(), window = %ld, l[0] = %ld, l[4] = %ld",
    window, xevent->xclient.data.l[0], xevent->xclient.data.l[4]);
    dnd_debug2 ("xdnd_send_event(), from widget widget %s", (char *) "<WIDGET>");
    XSendEvent (dnd->display, window, 0, 0, xevent);
}

static void xdnd_send_enter (DndClass * dnd, Window window, Window from, Atom * typelist)
{
    XEvent xevent;
    int n, i;
    n = array_length (typelist);

    memset (&xevent, 0, sizeof (xevent));

    xevent.xany.type = ClientMessage;
    xevent.xany.display = dnd->display;
    xevent.xclient.window = window;
    xevent.xclient.message_type = dnd->XdndEnter;
    xevent.xclient.format = 32;

    XDND_ENTER_SOURCE_WIN (&xevent) = from;
    XDND_ENTER_THREE_TYPES_SET (&xevent, n > XDND_THREE);
    XDND_ENTER_VERSION_SET (&xevent, dnd->version);
    for (i = 0; i < n && i < XDND_THREE; i++)
        XDND_ENTER_TYPE (&xevent, i) = typelist[i];
    xdnd_send_event (dnd, window, &xevent);
}

static void xdnd_send_position (DndClass * dnd, Window window, Window from, Atom action, int x, int y, unsigned long time)
{
    XEvent xevent;

    memset (&xevent, 0, sizeof (xevent));

    xevent.xany.type = ClientMessage;
    xevent.xany.display = dnd->display;
    xevent.xclient.window = window;
    xevent.xclient.message_type = dnd->XdndPosition;
    xevent.xclient.format = 32;

    XDND_POSITION_SOURCE_WIN (&xevent) = from;
    XDND_POSITION_ROOT_SET (&xevent, x, y);
    if (dnd_version_at_least (dnd->dragging_version, 1))
        XDND_POSITION_TIME (&xevent) = time;
    if (dnd_version_at_least (dnd->dragging_version, 2))
        XDND_POSITION_ACTION (&xevent) = action;

    xdnd_send_event (dnd, window, &xevent);
}

static void xdnd_send_status (DndClass * dnd, Window window, Window from, int will_accept, \
              int want_position, int x, int y, int w, int h, Atom action)
{
    XEvent xevent;

    memset (&xevent, 0, sizeof (xevent));

    xevent.xany.type = ClientMessage;
    xevent.xany.display = dnd->display;
    xevent.xclient.window = window;
    xevent.xclient.message_type = dnd->XdndStatus;
    xevent.xclient.format = 32;

    XDND_STATUS_TARGET_WIN (&xevent) = from;
    XDND_STATUS_WILL_ACCEPT_SET (&xevent, will_accept);
    if (will_accept)
        XDND_STATUS_WANT_POSITION_SET (&xevent, want_position);
    if (want_position)
        XDND_STATUS_RECT_SET (&xevent, x, y, w, h);
    if (dnd_version_at_least (dnd->dragging_version, 2))
        if (will_accept)
            XDND_STATUS_ACTION (&xevent) = action;

    xdnd_send_event (dnd, window, &xevent);
}

static void xdnd_send_leave (DndClass * dnd, Window window, Window from)
{
    XEvent xevent;

    memset (&xevent, 0, sizeof (xevent));

    xevent.xany.type = ClientMessage;
    xevent.xany.display = dnd->display;
    xevent.xclient.window = window;
    xevent.xclient.message_type = dnd->XdndLeave;
    xevent.xclient.format = 32;

    XDND_LEAVE_SOURCE_WIN (&xevent) = from;

    xdnd_send_event (dnd, window, &xevent);
}

static void xdnd_send_drop (DndClass * dnd, Window window, Window from, unsigned long time)
{
    XEvent xevent;

    memset (&xevent, 0, sizeof (xevent));

    xevent.xany.type = ClientMessage;
    xevent.xany.display = dnd->display;
    xevent.xclient.window = window;
    xevent.xclient.message_type = dnd->XdndDrop;
    xevent.xclient.format = 32;

    XDND_DROP_SOURCE_WIN (&xevent) = from;
    if (dnd_version_at_least (dnd->dragging_version, 1))
        XDND_DROP_TIME (&xevent) = time;

    xdnd_send_event (dnd, window, &xevent);
}

/* error is not actually used, i think future versions of the protocol should return an error status
   to the calling window with the XdndFinished client message */
static void xdnd_send_finished (DndClass * dnd, Window window, Window from, int error)
{
    XEvent xevent;

    memset (&xevent, 0, sizeof (xevent));

    xevent.xany.type = ClientMessage;
    xevent.xany.display = dnd->display;
    xevent.xclient.window = window;
    xevent.xclient.message_type = dnd->XdndFinished;
    xevent.xclient.format = 32;

    XDND_FINISHED_TARGET_WIN (&xevent) = from;

    xdnd_send_event (dnd, window, &xevent);
}

/* returns non-zero on error - i.e. no selection owner set. Type is of course the mime type */
static int xdnd_convert_selection (DndClass * dnd, Window window, Window requester, Atom type)
{
    if (!(window = XGetSelectionOwner (dnd->display, dnd->XdndSelection))) {
        dnd_debug1 ("xdnd_convert_selection(): XGetSelectionOwner failed");
        return 1;
    }
    XConvertSelection (dnd->display, dnd->XdndSelection, type,
                    dnd->Xdnd_NON_PROTOCOL_ATOM, requester, CurrentTime);
    return 0;
}

/* returns non-zero on error */
static int xdnd_set_selection_owner (DndClass * dnd, Window window, Atom type, Time time)
{
    if (!XSetSelectionOwner (dnd->display, dnd->XdndSelection, window, time)) {
        dnd_debug1 ("xdnd_set_selection_owner(): XSetSelectionOwner failed");
        return 1;
    }
    return 0;
}

static void xdnd_selection_send (DndClass * dnd, XSelectionRequestEvent * request, unsigned char *data, int length)
{
    XEvent xevent;
    dnd_debug2 ("      requestor = %ld", request->requestor);
    dnd_debug2 ("      property = %ld", request->property);
    dnd_debug2 ("      length = %d", length);
    XChangeProperty (dnd->display, request->requestor, request->property,
                     request->target, 8, PropModeReplace, data, length);
    xevent.xselection.type = SelectionNotify;
    xevent.xselection.property = request->property;
    xevent.xselection.display = request->display;
    xevent.xselection.requestor = request->requestor;
    xevent.xselection.selection = request->selection;
    xevent.xselection.target = request->target;
    xevent.xselection.time = request->time;
    xdnd_send_event (dnd, request->requestor, &xevent);
}

#if 0
/* respond to a notification that a primary selection has been sent */
int xdnd_get_selection (DndClass * dnd, Window from, Atom property, Window insert)
{
    long read;
    int error = 0;
    unsigned long remaining;
    if (!property)
        return 1;
    read = 0;
    do {
        unsigned char *s;
        Atom actual;
        int format;
        unsigned long count;
        if (XGetWindowProperty (dnd->display, insert, property, read / 4, 65536, 1,
                                AnyPropertyType, &actual, &format,
                                &count, &remaining,
                                &s) != Success) {
            XFree (s);
            return 1;
        }
        read += count;
        if (dnd->widget_insert_drop && !error)
            error = (*dnd->widget_insert_drop) (dnd, s, count, remaining, insert, from, actual);
        XFree (s);
    } while (remaining);
    return error;
}
#endif

static int paste_prop_internal (DndClass * dnd, Window from, Window insert, unsigned long prop, int delete_prop)
{
    long nread = 0;
    unsigned long nitems;
    unsigned long bytes_after;
    int error = 0;
    do {
        Atom actual_type;
        int actual_fmt;
        unsigned char *s = 0;
        if (XGetWindowProperty (dnd->display, insert, prop,
                                nread / 4, 65536, delete_prop,
                                AnyPropertyType, &actual_type, &actual_fmt,
                                &nitems, &bytes_after, &s) != Success) {
            XFree (s);
            return 1;
        }
        nread += nitems;
        if (dnd->widget_insert_drop && !error)
            error = (*dnd->widget_insert_drop) (dnd, s, nitems, bytes_after, insert, from, actual_fmt);
        XFree (s);
    } while (bytes_after);
    if (!nread)
        return 1;
    return 0;
}

/*
 * Respond to a notification that a primary selection has been sent (supports INCR)
 */
static int xdnd_get_selection (DndClass * dnd, Window from, Atom prop, Window insert)
{
    struct timeval tv, tv_start;
    unsigned long bytes_after;
    Atom actual_type;
    int actual_fmt;
    unsigned long nitems;
    unsigned char *s = 0;
    if (prop == None)
        return 1;
    if (XGetWindowProperty
        (dnd->display, insert, prop, 0, 8, False, AnyPropertyType, &actual_type, &actual_fmt,
         &nitems, &bytes_after, &s) != Success) {
        XFree (s);
        return 1;
    }
    XFree (s);
    if (actual_type != XInternAtom (dnd->display, "INCR", False))
        return paste_prop_internal (dnd, from, insert, prop, True);
    XDeleteProperty (dnd->display, insert, prop);
    gettimeofday (&tv_start, 0);
    for (;;) {
        long t;
        fd_set r;
        XEvent xe;
        if (XCheckMaskEvent (dnd->display, PropertyChangeMask, &xe)) {
            if (xe.type == PropertyNotify && xe.xproperty.state == PropertyNewValue) {
/* time between arrivals of data */
                gettimeofday (&tv_start, 0);
                if (paste_prop_internal (dnd, from, insert, prop, True))
                    break;
            }
        } else {
            tv.tv_sec = 0;
            tv.tv_usec = 10000;
            FD_ZERO (&r);
            FD_SET (ConnectionNumber (dnd->display), &r);
            select (ConnectionNumber (dnd->display) + 1, &r, 0, 0, &tv);
            if (FD_ISSET (ConnectionNumber (dnd->display), &r))
                continue;
        }
        gettimeofday (&tv, 0);
        t = (tv.tv_sec - tv_start.tv_sec) * 1000000L + (tv.tv_usec - tv_start.tv_usec);
/* no data for five seconds, so quit */
        if (t > 5000000L)
            return 1;
    }
    return 0;
}


int outside_rectangle (int x, int y, XRectangle * r)
{
    return (x < r->x || y < r->y || x >= r->x + r->width || y >= r->y + r->height);
}

/* avoids linking with the maths library */
static float xdnd_sqrt (float x)
{
    float last_ans, ans = 2, a;
    if (x <= 0.0)
        return 0.0;
    do {
        last_ans = ans;
        ans = (ans + x / ans) / 2;
        a = (ans - last_ans) / ans;
        if (a < 0.0)
            a = (-a);
    } while (a > 0.001);
    return ans;
}

#define print_marks print_win_marks(from,__FILE__,__LINE__);

/* returns action on success, 0 otherwise */
Atom xdnd_drag (DndClass * dnd, Window from, Atom action, Atom * typelist)
{
    XEvent xevent, xevent_temp;
    Window over_window = 0, last_window = 0;
#if XDND_VERSION >= 3
    Window last_dropper_toplevel = 0;
    int internal_dropable = 1;
#endif
    int n;
    DndCursor *cursor;
    float x_mouse, y_mouse;
    int result = 0, dnd_aware;

    if (!typelist)
        dnd_warning ("xdnd_drag() called with typelist = 0");

/* first wait until the mouse moves more than five pixels */
    do {
        XNextEvent (dnd->display, &xevent);
        if (xevent.type == ButtonRelease) {
            dnd_debug1 ("button release - no motion");
            XSendEvent (dnd->display, xevent.xany.window, 0, ButtonReleaseMask, &xevent);
            return 0;
        }
    } while (xevent.type != MotionNotify);

    x_mouse = (float) xevent.xmotion.x_root;
    y_mouse = (float) xevent.xmotion.y_root;

    if (!dnd->drag_threshold)
        dnd->drag_threshold = 4.0;
    for (;;) {
        XNextEvent (dnd->display, &xevent);
        if (xevent.type == MotionNotify)
            if (xdnd_sqrt ((x_mouse - xevent.xmotion.x_root) * (x_mouse - xevent.xmotion.x_root) +
                           (y_mouse - xevent.xmotion.y_root) * (y_mouse - xevent.xmotion.y_root)) > dnd->drag_threshold)
                break;
        if (xevent.type == ButtonRelease) {
            XSendEvent (dnd->display, xevent.xany.window, 0, ButtonReleaseMask, &xevent);
            return 0;
        }
    }

    dnd_debug1 ("moved 5 pixels - going to drag");

    n = array_length (typelist);
    if (n > XDND_THREE)
        xdnd_set_type_list (dnd, from, typelist);

    xdnd_reset (dnd);

    dnd->stage = XDND_DRAG_STAGE_DRAGGING;

    for (cursor = &dnd->cursors[0]; cursor->width; cursor++)
        if (cursor->action == action)
            break;
    if (!cursor->width)
        cursor = &dnd->cursors[0];

/* the mouse has been dragged a little, so this is a drag proper */
    if (XGrabPointer (dnd->display, dnd->root_window, False,
                      ButtonMotionMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                      GrabModeAsync, GrabModeAsync, None,
                      cursor->cursor, CurrentTime) != GrabSuccess)
        dnd_debug1 ("Unable to grab pointer");


    while (xevent.xany.type != ButtonRelease) {
        XAllowEvents (dnd->display, SyncPointer, CurrentTime);
        XNextEvent (dnd->display, &xevent);
        switch (xevent.type) {
        case Expose:
            if (dnd->handle_expose_events)
                (*dnd->handle_expose_events) (dnd, &xevent);
            break;
        case EnterNotify:
/* this event is not actually reported, so we find out by ourselves from motion events */
            break;
        case LeaveNotify:
/* this event is not actually reported, so we find out by ourselves from motion events */
            break;
        case ButtonRelease:
/* done, but must send a leave event */
            dnd_debug1 ("ButtonRelease - exiting event loop");
            break;
        case MotionNotify:
            dnd_aware = 0;
            dnd->dropper_toplevel = 0;
            memcpy (&xevent_temp, &xevent, sizeof (xevent));
            xevent.xmotion.subwindow = xevent.xmotion.window;
            {
                Window root_return, child_return;
                int x_temp, y_temp;
                unsigned int mask_return;
                while (XQueryPointer (dnd->display, xevent.xmotion.subwindow, &root_return, &child_return,
                                      &x_temp, &y_temp, &xevent.xmotion.x,
                                      &xevent.xmotion.y, &mask_return)) {
#if XDND_VERSION >= 3
                    if (!dnd_aware) {
                        if ((dnd_aware = xdnd_is_dnd_aware (dnd, xevent.xmotion.subwindow, &dnd->dragging_version, typelist))) {
                            dnd->dropper_toplevel = xevent.xmotion.subwindow;
                            xevent.xmotion.x_root = x_temp;
                            xevent.xmotion.y_root = y_temp;
                        }
                    }
#else
                    xevent.xmotion.x_root = x_temp;
                    xevent.xmotion.y_root = y_temp;
#endif
                    if (!child_return)
                        goto found_descendent;
                    xevent.xmotion.subwindow = child_return;
                }
                break;
            }
          found_descendent:

/* last_window is just for debug purposes */
            if (last_window != xevent.xmotion.subwindow) {
                dnd_debug2 ("window crossing to %ld", xevent.xmotion.subwindow);
                dnd_debug2 ("  current window is %ld", over_window);
                dnd_debug3 ("     last_window = %ld, xmotion.subwindow = %ld", last_window, xevent.xmotion.subwindow);
#if XDND_VERSION >= 3
                dnd_debug3 ("     dropper_toplevel = %ld, last_dropper_toplevel.subwindow = %ld", dnd->dropper_toplevel, last_dropper_toplevel);
#endif
                dnd_debug3 ("     dnd_aware = %d, dnd->options & XDND_OPTION_NO_HYSTERESIS = %ld", dnd_aware, (long) dnd->options & XDND_OPTION_NO_HYSTERESIS);
            }

#if XDND_VERSION < 3
/* is the new window dnd aware? if not stay in the old window */
            if (over_window != xevent.xmotion.subwindow &&
                last_window != xevent.xmotion.subwindow &&
                (
                    (dnd_aware = xdnd_is_dnd_aware (dnd, xevent.xmotion.subwindow, &dnd->dragging_version, typelist))
                    ||
                    (dnd->options & XDND_OPTION_NO_HYSTERESIS)
                ))
#else
            internal_dropable = 1;
            if (dnd->widget_exists && (*dnd->widget_exists) (dnd, xevent.xmotion.subwindow))
                if (!xdnd_is_dnd_aware (dnd, xevent.xmotion.subwindow, &dnd->dragging_version, typelist))
                    internal_dropable = 0;
            dnd_debug3 ("dnd->dropper_toplevel = %ld, last_dropper_toplevel = %ld\n", dnd->dropper_toplevel, last_dropper_toplevel);
            if ((dnd->dropper_toplevel != last_dropper_toplevel ||
                last_window != xevent.xmotion.subwindow) && internal_dropable &&
                (
                    (dnd_aware)
                    ||
                    (dnd->options & XDND_OPTION_NO_HYSTERESIS)
                ))
#endif
            {
/* leaving window we were over */
                if (over_window) {
                    if (dnd->stage == XDND_DRAG_STAGE_ENTERED) {
                        dnd_debug1 ("got leave at right stage");
                        dnd->stage = XDND_DRAG_STAGE_DRAGGING;
                        if (dnd->internal_drag) {
                            dnd_debug1 ("  our own widget");
                            if (dnd->widget_apply_leave)
                                (*dnd->widget_apply_leave) (dnd, over_window);
                        } else {
                            dnd_debug1 ("  not our widget - sending XdndLeave");
#if XDND_VERSION < 3
                            xdnd_send_leave (dnd, over_window, from);
#else
                            if (dnd->dropper_toplevel != last_dropper_toplevel) {
                                xdnd_send_leave (dnd, last_dropper_toplevel, from);
                            } else {
                                dnd_debug1 ("    not sending leave --> dnd->dropper_toplevel == last_dropper_toplevel");
                            }
#endif
                        }
                        dnd->internal_drag = 0;
                        dnd->dropper_window = 0;
                        dnd->ready_to_drop = 0;
                    } else {
                        dnd_debug1 ("got leave at wrong stage - ignoring");
                    }
                }
/* entering window we are currently over */
                over_window = xevent.xmotion.subwindow;
                if (dnd_aware) {
                    dnd_debug1 ("  is dnd aware");
                    dnd->stage = XDND_DRAG_STAGE_ENTERED;
                    if (dnd->widget_exists && (*dnd->widget_exists) (dnd, over_window))
                        dnd->internal_drag = 1;
                    if (dnd->internal_drag) {
                        dnd_debug1 ("    our own widget");
                    } else {
                        dnd_debug2 ("    not our widget - sending XdndEnter to %ld", over_window);
#if XDND_VERSION < 3
                        xdnd_send_enter (dnd, over_window, from, typelist);
#else
                        if (dnd->dropper_toplevel != last_dropper_toplevel)
                            xdnd_send_enter (dnd, dnd->dropper_toplevel, from, typelist);
#endif
                    }
                    dnd->want_position = 1;
                    dnd->ready_to_drop = 0;
                    dnd->rectangle.width = dnd->rectangle.height = 0;
                    dnd->dropper_window = over_window;
/* we want an additional motion event in case the pointer enters and then stops */
                    XSendEvent (dnd->display, from, 0, ButtonMotionMask, &xevent_temp);
                    XSync (dnd->display, 0);
                }
#if XDND_VERSION >= 3
                last_dropper_toplevel = dnd->dropper_toplevel;
#endif
/* we are now officially in a new window */
            } else {
/* got here, so we are just moving `inside' the same window */
                if (dnd->stage == XDND_DRAG_STAGE_ENTERED) {
                    dnd->supported_action = dnd->XdndActionCopy;
                    dnd_debug1 ("got motion at right stage");
                    dnd->x = xevent.xmotion.x_root;
                    dnd->y = xevent.xmotion.y_root;
                    if (dnd->want_position || outside_rectangle (dnd->x, dnd->y, &dnd->rectangle)) {
                        dnd_debug1 ("  want position and outside rectangle");
                        if (dnd->internal_drag) {
                            dnd_debug1 ("    our own widget");
                            dnd->ready_to_drop = (*dnd->widget_apply_position) (dnd, over_window, from,
                                                                                action, dnd->x, dnd->y, xevent.xmotion.time, typelist,
                                                                                &dnd->want_position, &dnd->supported_action, &dnd->desired_type, &dnd->rectangle);
                            /* if not ready, keep sending positions, this check is repeated below for XdndStatus from external widgets */
                            if (!dnd->ready_to_drop) {
                                dnd->want_position = 1;
                                dnd->rectangle.width = dnd->rectangle.height = 0;
                            }
                            dnd_debug2 ("      return action=%ld", dnd->supported_action);
                        } else {
#if XDND_VERSION < 3
                            dnd_debug3 ("    not our own widget - sending XdndPosition to %ld, action %ld", over_window, action);
                            xdnd_send_position (dnd, over_window, from, action, dnd->x, dnd->y, xevent.xmotion.time);
#else
                            dnd_debug3 ("    not our own widget - sending XdndPosition to %ld, action %ld", dnd->dropper_toplevel, action);
                            xdnd_send_position (dnd, dnd->dropper_toplevel, from, action, dnd->x, dnd->y, xevent.xmotion.time);
#endif
                        }
                    } else if (dnd->want_position) {
                        dnd_debug1 ("  inside rectangle");
                    } else {
                        dnd_debug1 ("  doesn't want position");
                    }
                }
            }
            last_window = xevent.xmotion.subwindow;
            break;
        case ClientMessage:
            dnd_debug1 ("ClientMessage recieved");
            if (xevent.xclient.message_type == dnd->XdndStatus && !dnd->internal_drag) {
                dnd_debug1 ("  XdndStatus recieved");
                if (dnd->stage == XDND_DRAG_STAGE_ENTERED 
#if XDND_VERSION < 3
                        && XDND_STATUS_TARGET_WIN (&xevent) == dnd->dropper_window
#endif
                ) {
                    dnd_debug1 ("    XdndStatus stage correct, dropper window correct");
                    dnd->want_position = XDND_STATUS_WANT_POSITION (&xevent);
                    dnd->ready_to_drop = XDND_STATUS_WILL_ACCEPT (&xevent);
                    dnd->rectangle.x = XDND_STATUS_RECT_X (&xevent);
                    dnd->rectangle.y = XDND_STATUS_RECT_Y (&xevent);
                    dnd->rectangle.width = XDND_STATUS_RECT_WIDTH (&xevent);
                    dnd->rectangle.height = XDND_STATUS_RECT_HEIGHT (&xevent);
                    dnd->supported_action = dnd->XdndActionCopy;
                    if (dnd_version_at_least (dnd->dragging_version, 2))
                        dnd->supported_action = XDND_STATUS_ACTION (&xevent);
                    dnd_debug3 ("      return action=%ld, ready=%d", dnd->supported_action, dnd->ready_to_drop);
                    /* if not ready, keep sending positions, this check is repeated above for internal widgets */
                    if (!dnd->ready_to_drop) {
                        dnd->want_position = 1;
                        dnd->rectangle.width = dnd->rectangle.height = 0;
                    }
                    dnd_debug3 ("      rectangle = (x=%d, y=%d, ", dnd->rectangle.x, dnd->rectangle.y);
                    dnd_debug4                               ("w=%d, h=%d), want_position=%d\n", dnd->rectangle.width, dnd->rectangle.height, dnd->want_position);
                }
#if XDND_VERSION < 3
                else if (XDND_STATUS_TARGET_WIN (&xevent) != dnd->dropper_window) {
                    dnd_debug3 ("    XdndStatus XDND_STATUS_TARGET_WIN (&xevent) = %ld, dnd->dropper_window = %ld", XDND_STATUS_TARGET_WIN (&xevent), dnd->dropper_window);
                }
#endif
                else {
                    dnd_debug2 ("    XdndStatus stage incorrect dnd->stage = %d", dnd->stage);
                }
            }
            break;
        case SelectionRequest:{
/* the target widget MAY request data, so wait for SelectionRequest */
                int length = 0;
                unsigned char *data = 0;
                dnd_debug1 ("SelectionRequest - getting widget data");

                (*dnd->widget_get_data) (dnd, from, &data, &length, xevent.xselectionrequest.target);
                if (data) {
                    dnd_debug1 ("  sending selection");
                    xdnd_selection_send (dnd, &xevent.xselectionrequest, data, length);
                    xdnd_xfree (data);
                }
            }
            break;
        }
    }

    if (dnd->ready_to_drop) {
        Time time;
        dnd_debug1 ("ready_to_drop - sending XdndDrop");
        time = xevent.xbutton.time;
        if (dnd->internal_drag) {
/* we are dealing with our own widget, no need to send drop events, just put the data straight */
            int length = 0;
            unsigned char *data = 0;
            if (dnd->widget_insert_drop) {
                (*dnd->widget_get_data) (dnd, from, &data, &length, dnd->desired_type);
                if (data) {
                    if (!(*dnd->widget_insert_drop) (dnd, data, length, 0, dnd->dropper_window, from, dnd->desired_type)) {
                        result = dnd->supported_action;                /* success - so return action to caller */
                        dnd_debug1 ("  inserted data into widget - success");
                    } else {
                        dnd_debug1 ("  inserted data into widget - failed");
                    }
                    xdnd_xfree (data);
                } else {
                    dnd_debug1 ("  got data from widget, but data is null");
                }
            }
        } else {
            xdnd_set_selection_owner (dnd, from, dnd->desired_type, time);
#if XDND_VERSION < 3
            xdnd_send_drop (dnd, dnd->dropper_window, from, time);
#else
            xdnd_send_drop (dnd, dnd->dropper_toplevel, from, time);
#endif
        }
        if (!dnd->internal_drag)
            for (;;) {
                XAllowEvents (dnd->display, SyncPointer, CurrentTime);
                XNextEvent (dnd->display, &xevent);
                if (xevent.type == ClientMessage && xevent.xclient.message_type == dnd->XdndFinished) {
                    dnd_debug1 ("XdndFinished");
#if XDND_VERSION < 3
                    if (XDND_FINISHED_TARGET_WIN (&xevent) == dnd->dropper_window) {
#endif
                        dnd_debug2 ("  source correct - exiting event loop, action=%ld", dnd->supported_action);
                        result = dnd->supported_action;                /* success - so return action to caller */
                        break;
#if XDND_VERSION < 3
                    }
#endif
                } else if (xevent.type == Expose) {
                    if (dnd->handle_expose_events)
                        (*dnd->handle_expose_events) (dnd, &xevent);
                } else if (xevent.type == MotionNotify) {
                    if (xevent.xmotion.time > time + (dnd->time_out ? dnd->time_out * 1000 : 10000)) {        /* allow a ten second timeout as default */
                        dnd_debug1 ("timeout - exiting event loop");
                        break;
                    }
                } else if (xevent.type == SelectionRequest && xevent.xselectionrequest.selection == dnd->XdndSelection) {
/* the target widget is going to request data, so check for SelectionRequest events */
                    int length = 0;
                    unsigned char *data = 0;

                    dnd_debug1 ("SelectionRequest - getting widget data");
                    (*dnd->widget_get_data) (dnd, from, &data, &length, xevent.xselectionrequest.target);
                    if (data) {
                        dnd_debug1 ("  sending selection");
                        xdnd_selection_send (dnd, &xevent.xselectionrequest, data, length);
                        xdnd_xfree (data);
                    }
/* don't wait for a XdndFinished event */
                    if (!dnd_version_at_least (dnd->dragging_version, 2))
                        break;
                }
            }
    } else {
        dnd_debug1 ("not ready_to_drop - ungrabbing pointer");
    }
    XUngrabPointer (dnd->display, CurrentTime);
    xdnd_reset (dnd);
    return result;
}

/* returns non-zero if event is handled */
int xdnd_handle_drop_events (DndClass * dnd, XEvent * xevent)
{
    int result = 0;
    if (xevent->type == SelectionNotify) {
        dnd_debug1 ("got SelectionNotify");
        if (xevent->xselection.property == dnd->Xdnd_NON_PROTOCOL_ATOM && dnd->stage == XDND_DROP_STAGE_CONVERTING) {
            int error;
            dnd_debug1 ("  property is Xdnd_NON_PROTOCOL_ATOM - getting selection");
            error = xdnd_get_selection (dnd, dnd->dragger_window, xevent->xselection.property, xevent->xany.window);
/* error is not actually used, i think future versions of the protocol maybe should return 
   an error status to the calling window with the XdndFinished client message */
            if (dnd_version_at_least (dnd->dragging_version, 2)) {
#if XDND_VERSION >= 3
                xdnd_send_finished (dnd, dnd->dragger_window, dnd->dropper_toplevel, error);
#else
                xdnd_send_finished (dnd, dnd->dragger_window, dnd->dropper_window, error);
#endif
                dnd_debug1 ("    sending finished");
            }
            xdnd_xfree (dnd->dragger_typelist);
            xdnd_reset (dnd);
            dnd->stage = XDND_DROP_STAGE_IDLE;
            result = 1;
        } else {
            dnd_debug1 ("  property is not Xdnd_NON_PROTOCOL_ATOM - ignoring");
        }
    } else if (xevent->type == ClientMessage) {
        dnd_debug2 ("got ClientMessage to xevent->xany.window = %ld", xevent->xany.window);
        if (xevent->xclient.message_type == dnd->XdndEnter) {
            dnd_debug2 ("  message_type is XdndEnter, version = %ld", XDND_ENTER_VERSION (xevent));
#if XDND_VERSION >= 3
            if (XDND_ENTER_VERSION (xevent) < 3)
                return 0;
#endif
            xdnd_reset (dnd);
            dnd->dragger_window = XDND_ENTER_SOURCE_WIN (xevent);
#if XDND_VERSION >= 3
            dnd->dropper_toplevel = xevent->xany.window;
            dnd->dropper_window = 0;     /* enter goes to the top level window only,
                                            so we don't really know what the
                                            sub window is yet */
#else
            dnd->dropper_window = xevent->xany.window;
#endif
            xdnd_xfree (dnd->dragger_typelist);
            if (XDND_ENTER_THREE_TYPES (xevent)) {
                dnd_debug1 ("    three types only");
                xdnd_get_three_types (dnd, xevent, &dnd->dragger_typelist);
            } else {
                dnd_debug1 ("    more than three types - getting list");
                xdnd_get_type_list (dnd, dnd->dragger_window, &dnd->dragger_typelist);
            }
            if (dnd->dragger_typelist)
                dnd->stage = XDND_DROP_STAGE_ENTERED;
            else
                dnd_debug1 ("      typelist returned as zero!");
            dnd->dragging_version = XDND_ENTER_VERSION (xevent);
            result = 1;
        } else if (xevent->xclient.message_type == dnd->XdndLeave) {
#if XDND_VERSION >= 3
            if (xevent->xany.window == dnd->dropper_toplevel && dnd->dropper_window)
                xevent->xany.window = dnd->dropper_window;
#endif
            dnd_debug1 ("  message_type is XdndLeave");
            if (dnd->dragger_window == XDND_LEAVE_SOURCE_WIN (xevent) && dnd->stage == XDND_DROP_STAGE_ENTERED) {
                dnd_debug1 ("    leaving");
                if (dnd->widget_apply_leave)
                    (*dnd->widget_apply_leave) (dnd, xevent->xany.window);
                dnd->stage = XDND_DROP_STAGE_IDLE;
                xdnd_xfree (dnd->dragger_typelist);
                result = 1;
                dnd->dropper_toplevel = dnd->dropper_window = 0;
            } else {
                dnd_debug1 ("    wrong stage or from wrong window");
            }
        } else if (xevent->xclient.message_type == dnd->XdndPosition) {
            dnd_debug2 ("  message_type is XdndPosition to %ld", xevent->xany.window);
            if (dnd->dragger_window == XDND_POSITION_SOURCE_WIN (xevent) && dnd->stage == XDND_DROP_STAGE_ENTERED) {
                int want_position;
                Atom action;
                XRectangle rectangle;
                Window last_window;
                last_window = dnd->dropper_window;
#if XDND_VERSION >= 3
/* version 3 gives us the top-level window only. WE have to find the child that the pointer is over: */
                if (1 || xevent->xany.window != dnd->dropper_toplevel || !dnd->dropper_window) {
                    Window parent, child, new_child = 0;
                    dnd->dropper_toplevel = xevent->xany.window;
                    parent = dnd->root_window;
                    child = dnd->dropper_toplevel;
                    for (;;) {
                        int xd, yd;
                        new_child = 0;
                        if (!XTranslateCoordinates (dnd->display, parent, child, 
                                    XDND_POSITION_ROOT_X (xevent), XDND_POSITION_ROOT_Y (xevent),
                                    &xd, &yd, &new_child))
                            break;
                        if (!new_child)
                            break;
                        child = new_child;
                    }
                    dnd->dropper_window = xevent->xany.window = child;
                    dnd_debug2 ("   child window translates to %ld", dnd->dropper_window);
                } else if (xevent->xany.window == dnd->dropper_toplevel && dnd->dropper_window) {
                    xevent->xany.window = dnd->dropper_window;
                    dnd_debug2 ("   child window previously found: %ld", dnd->dropper_window);
                }
#endif
                action = dnd->XdndActionCopy;
                dnd->supported_action = dnd->XdndActionCopy;
                dnd->x = XDND_POSITION_ROOT_X (xevent);
                dnd->y = XDND_POSITION_ROOT_Y (xevent);
                dnd->time = CurrentTime;
                if (dnd_version_at_least (dnd->dragging_version, 1))
                    dnd->time = XDND_POSITION_TIME (xevent);
                if (dnd_version_at_least (dnd->dragging_version, 1))
                    action = XDND_POSITION_ACTION (xevent);
#if XDND_VERSION >= 3
                if (last_window && last_window != xevent->xany.window)
                    if (dnd->widget_apply_leave)
                        (*dnd->widget_apply_leave) (dnd, last_window);
#endif
                dnd->will_accept = (*dnd->widget_apply_position) (dnd, xevent->xany.window, dnd->dragger_window,
                action, dnd->x, dnd->y, dnd->time, dnd->dragger_typelist,
                                                                  &want_position, &dnd->supported_action, &dnd->desired_type, &rectangle);
                dnd_debug2 ("    will accept = %d", dnd->will_accept);
#if XDND_VERSION >= 3
                dnd_debug2 ("    sending status of %ld", dnd->dropper_toplevel);
                xdnd_send_status (dnd, dnd->dragger_window, dnd->dropper_toplevel, dnd->will_accept,
                                  want_position, rectangle.x, rectangle.y, rectangle.width, rectangle.height, dnd->supported_action);
#else
                dnd_debug2 ("    sending status of %ld", xevent->xany.window);
                xdnd_send_status (dnd, dnd->dragger_window, xevent->xany.window, dnd->will_accept,
                                  want_position, rectangle.x, rectangle.y, rectangle.width, rectangle.height, dnd->supported_action);
#endif
                result = 1;
            } else {
                dnd_debug1 ("    wrong stage or from wrong window");
            }
        } else if (xevent->xclient.message_type == dnd->XdndDrop) {
#if XDND_VERSION >= 3
            if (xevent->xany.window == dnd->dropper_toplevel && dnd->dropper_window)
                xevent->xany.window = dnd->dropper_window;
#endif
            dnd_debug1 ("  message_type is XdndDrop");
            if (dnd->dragger_window == XDND_DROP_SOURCE_WIN (xevent) && dnd->stage == XDND_DROP_STAGE_ENTERED) {
                dnd->time = CurrentTime;
                if (dnd_version_at_least (dnd->dragging_version, 1))
                    dnd->time = XDND_DROP_TIME (xevent);
                if (dnd->will_accept) {
                    dnd_debug1 ("    will_accept is true - converting selectiong");
                    dnd_debug2 ("      my window is %ld", dnd->dropper_window);
                    dnd_debug2 ("        source window is %ld", dnd->dragger_window);
                    xdnd_convert_selection (dnd, dnd->dragger_window, dnd->dropper_window, dnd->desired_type);
                    dnd->stage = XDND_DROP_STAGE_CONVERTING;
                } else {
                    dnd_debug1 ("    will_accept is false - sending finished");
                    if (dnd_version_at_least (dnd->dragging_version, 2)) {
#if XDND_VERSION >= 3
                        xdnd_send_finished (dnd, dnd->dragger_window, dnd->dropper_toplevel, 1);
#else
                        xdnd_send_finished (dnd, dnd->dragger_window, xevent->xany.window, 1);
#endif
                    }
                    xdnd_xfree (dnd->dragger_typelist);
                    xdnd_reset (dnd);
                    dnd->stage = XDND_DROP_STAGE_IDLE;
                }
                result = 1;
            } else {
                dnd_debug1 ("    wrong stage or from wrong window");
            }
        }
    }
    return result;
}

/*
   Following here is a sample implementation: Suppose we want a window
   to recieve drops, but do not want to be concerned with setting up all
   the DndClass methods. All we then do is call xdnd_get_drop() whenever a
   ClientMessage is recieved. If the message has nothing to do with XDND,
   xdnd_get_drop quickly returns 0. If it is a XdndEnter message, then
   xdnd_get_drop enters its own XNextEvent loop and handles all XDND
   protocol messages internally, returning the action requested.

   You should pass a desired typelist and actionlist to xdnd_get_type.
   These must be null terminated arrays of atoms, or a null pointer
   if you would like any action or type to be accepted. If typelist
   is null then the first type of the dragging widgets typelist will
   be the one used. If actionlist is null, then only XdndActionCopy will
   be accepted.

   The result is stored in *data, length, type, x and y.
   *data must be free'd.
 */

struct xdnd_get_drop_info {
    unsigned char *drop_data;
    int drop_data_length;
    int x, y;
    Atom return_type;
    Atom return_action;
    Atom *typelist;
    Atom *actionlist;
};

static int widget_insert_drop (DndClass * dnd, unsigned char *data, int length, int remaining, Window into, Window from, Atom type)
{
    struct xdnd_get_drop_info *i;
    i = (struct xdnd_get_drop_info *) dnd->user_hook1;
    if (!i->drop_data) {
        i->drop_data = malloc (length);
        if (!i->drop_data)
            return 1;
        memcpy (i->drop_data, data, length);
        i->drop_data_length = length;
    } else {
        unsigned char *t;
        t = malloc (i->drop_data_length + length);
        if (!t) {
            free (i->drop_data);
            i->drop_data = 0;
            return 1;
        }
        memcpy (t, i->drop_data, i->drop_data_length);
        memcpy (t + i->drop_data_length, data, length);
        free (i->drop_data);
        i->drop_data = t;
        i->drop_data_length += length;
    }
    return 0;
}

static int widget_apply_position (DndClass * dnd, Window widgets_window, Window from,
                      Atom action, int x, int y, Time t, Atom * typelist,
 int *want_position, Atom * supported_action_return, Atom * desired_type,
                                  XRectangle * rectangle)
{
    int i, j;
    struct xdnd_get_drop_info *info;
    Atom *dropper_typelist, supported_type = 0;
    Atom *supported_actions, supported_action = 0;

    info = (struct xdnd_get_drop_info *) dnd->user_hook1;
    dropper_typelist = info->typelist;
    supported_actions = info->actionlist;

    if (dropper_typelist) {
/* find a correlation: */
        for (j = 0; dropper_typelist[j]; j++) {
            for (i = 0; typelist[i]; i++) {
                if (typelist[i] == dropper_typelist[j]) {
                    supported_type = typelist[i];
                    break;
                }
            }
            if (supported_type)
                break;
        }
    } else {
/* user did not specify, so return first type */
        supported_type = typelist[0];
    }
/* not supported, so return false */
    if (!supported_type)
        return 0;

    if (supported_actions) {
        for (j = 0; supported_actions[j]; j++) {
            if (action == supported_actions[j]) {
                supported_action = action;
                break;
            }
        }
    } else {
/* user did not specify */
        if (action == dnd->XdndActionCopy)
            supported_action = action;
    }
    if (!supported_action)
        return 0;

    *want_position = 1;
    rectangle->x = rectangle->y = 0;
    rectangle->width = rectangle->height = 0;

    info->return_action = *supported_action_return = supported_action;
    info->return_type = *desired_type = supported_type;
    info->x = x;
    info->y = y;

    return 1;
}

Atom xdnd_get_drop (Display * display, XEvent * xevent, Atom * typelist, Atom * actionlist,
          unsigned char **data, int *length, Atom * type, int *x, int *y)
{
    Atom action = 0;
    static int initialised = 0;
    static DndClass dnd;
    if (!initialised) {
        xdnd_init (&dnd, display);
        initialised = 1;
    }
    if (xevent->type != ClientMessage || xevent->xclient.message_type != dnd.XdndEnter) {
        return 0;
    } else {
        struct xdnd_get_drop_info i;

/* setup user structure */
        memset (&i, 0, sizeof (i));
        i.actionlist = actionlist;
        i.typelist = typelist;
        dnd.user_hook1 = &i;

/* setup methods */
        dnd.widget_insert_drop = widget_insert_drop;
        dnd.widget_apply_position = widget_apply_position;

/* main loop */
        for (;;) {
            xdnd_handle_drop_events (&dnd, xevent);
            if (dnd.stage == XDND_DROP_STAGE_IDLE)
                break;
            XNextEvent (dnd.display, xevent);
        }

/* return results */
        if (i.drop_data) {
            *length = i.drop_data_length;
            *data = i.drop_data;
            action = i.return_action;
            *type = i.return_type;
            *x = i.x;
            *y = i.y;
        }
    }
    return action;
}


