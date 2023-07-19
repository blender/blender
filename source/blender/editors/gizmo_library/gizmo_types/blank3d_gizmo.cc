/* SPDX-FileCopyrightText: 2014 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgizmolib
 *
 * \name Blank Gizmo
 *
 * \brief Gizmo to use as a fallback (catch events).
 */

#include "BKE_context.h"

#include "ED_gizmo_library.h"

#include "WM_api.h"
#include "WM_types.h"

/* own includes */

static void gizmo_blank_draw(const bContext * /*C*/, wmGizmo * /*gz*/)
{
  /* pass */
}

static int gizmo_blank_invoke(bContext * /*C*/, wmGizmo * /*gz*/, const wmEvent * /*event*/)
{
  return OPERATOR_RUNNING_MODAL;
}

static int gizmo_blank_test_select(bContext * /*C*/, wmGizmo * /*gz*/, const int[2] /*mval*/)
{
  return 0;
}

/* -------------------------------------------------------------------- */
/** \name Blank Gizmo API
 * \{ */

static void GIZMO_GT_blank_3d(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "GIZMO_GT_blank_3d";

  /* api callbacks */
  gzt->draw = gizmo_blank_draw;
  gzt->invoke = gizmo_blank_invoke;
  gzt->test_select = gizmo_blank_test_select;

  gzt->struct_size = sizeof(wmGizmo);
}

void ED_gizmotypes_blank_3d(void)
{
  WM_gizmotype_append(GIZMO_GT_blank_3d);
}

/** \} */
