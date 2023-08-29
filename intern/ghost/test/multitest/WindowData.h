/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

typedef void (*WindowDataHandler)(void *priv, GHOST_EventHandle evt);
typedef struct _WindowData WindowData;

/***/

WindowData *windowdata_new(void *data, WindowDataHandler handler);
void windowdata_handle(WindowData *wb, GHOST_EventHandle evt);
void windowdata_free(WindowData *wb);
