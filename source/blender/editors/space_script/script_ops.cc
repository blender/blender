/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spscript
 */

#include <cmath>
#include <cstdlib>

#include "WM_api.hh"

#include "script_intern.hh"

/* ************************** registration **********************************/

void script_operatortypes()
{
  WM_operatortype_append(SCRIPT_OT_python_file_run);
  WM_operatortype_append(SCRIPT_OT_reload);
}

void script_keymap(wmKeyConfig * /*keyconf*/)
{
  /* Script space is deprecated, and doesn't need a keymap */
}
