/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Generic helper utilities that aren't associated with a particular area.
 */

#include "WM_api.hh"
#include "WM_types.hh"

#include "MEM_guardedalloc.h"

/* -------------------------------------------------------------------- */
/** \name Generic Callback
 * \{ */

void WM_generic_callback_free(wmGenericCallback *callback)
{
  if (callback->free_user_data) {
    callback->free_user_data(callback->user_data);
  }
  MEM_freeN(callback);
}

static void do_nothing(bContext * /*C*/, void * /*user_data*/) {}

wmGenericCallback *WM_generic_callback_steal(wmGenericCallback *callback)
{
  wmGenericCallback *new_callback = static_cast<wmGenericCallback *>(MEM_dupallocN(callback));
  callback->exec = do_nothing;
  callback->free_user_data = nullptr;
  callback->user_data = nullptr;
  return new_callback;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic User Data
 * \{ */

void WM_generic_user_data_free(wmGenericUserData *wm_userdata)
{
  if (wm_userdata->data && wm_userdata->use_free) {
    if (wm_userdata->free_fn) {
      wm_userdata->free_fn(wm_userdata->data);
    }
    else {
      MEM_freeN(wm_userdata->data);
    }
  }
}

/** \} */
