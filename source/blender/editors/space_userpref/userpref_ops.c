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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spuserpref
 */

#include <string.h>

#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "RNA_types.h"

#include "UI_interface.h"

#include "../interface/interface_intern.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_userpref.h"

/* -------------------------------------------------------------------- */
/** \name Reset Default Theme
 * \{ */

static int reset_default_theme_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  UI_theme_init_default();
  UI_style_init_default();
  WM_reinit_gizmomap_all(bmain);
  WM_event_add_notifier(C, NC_WINDOW, NULL);
  U.runtime.is_dirty = true;
  return OPERATOR_FINISHED;
}

static void PREFERENCES_OT_reset_default_theme(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset to Default Theme";
  ot->idname = "PREFERENCES_OT_reset_default_theme";
  ot->description = "Reset to the default theme colors";

  /* callbacks */
  ot->exec = reset_default_theme_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Auto-Save Override
 *
 * This operator only exists so there is a useful tool-tip for for adjusting the global flag.
 * \{ */

static int preferences_autosave_override_toggle_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
  G.f ^= G_FLAG_USERPREF_NO_SAVE_ON_EXIT;
  return OPERATOR_FINISHED;
}

static void PREFERENCES_OT_autosave_override_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Override Auto-Save";
  ot->idname = "PREFERENCES_OT_autosave_override_toggle";
  ot->description =
      "The current session has \"Factory Preferences\" loaded "
      "which disables automatically saving.\n"
      "Disable this to auto-save the preferences";

  /* callbacks */
  ot->exec = preferences_autosave_override_toggle_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

void ED_operatortypes_userpref(void)
{
  WM_operatortype_append(PREFERENCES_OT_reset_default_theme);
  WM_operatortype_append(PREFERENCES_OT_autosave_override_toggle);
}
