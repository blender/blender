/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "GHOST_C-api.h"

#include "WindowData.h"

struct _WindowData {
  void *data;
  WindowDataHandler handler;
};

WindowData *windowdata_new(void *data, WindowDataHandler handler)
{
  WindowData *wb = MEM_mallocN(sizeof(*wb), "windowdata_new");
  wb->data = data;
  wb->handler = handler;

  return wb;
}

void windowdata_handle(WindowData *wb, GHOST_EventHandle evt)
{
  wb->handler(wb->data, evt);
}

void windowdata_free(WindowData *wb)
{
  MEM_freeN(wb);
}
