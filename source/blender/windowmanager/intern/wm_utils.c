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
 */

/** \file
 * \ingroup wm
 *
 * Generic helper utilities that aren't associated with a particular area.
 */

#include "WM_types.h"
#include "WM_api.h"

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

static void do_nothing(struct bContext *UNUSED(C), void *UNUSED(user_data))
{
}

wmGenericCallback *WM_generic_callback_steal(wmGenericCallback *callback)
{
  wmGenericCallback *new_callback = MEM_dupallocN(callback);
  callback->exec = do_nothing;
  callback->free_user_data = NULL;
  callback->user_data = NULL;
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
