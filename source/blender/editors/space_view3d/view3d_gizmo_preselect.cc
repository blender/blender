/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "ED_gizmo_utils.hh"
#include "ED_screen.hh"

#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "view3d_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Mesh Pre-Select Element Gizmo
 * \{ */

struct GizmoGroupPreSelElem {
  wmGizmo *gizmo;
};

static void WIDGETGROUP_mesh_preselect_elem_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  const wmGizmoType *gzt_presel = WM_gizmotype_find("GIZMO_GT_mesh_preselect_elem_3d", true);
  GizmoGroupPreSelElem *ggd = static_cast<GizmoGroupPreSelElem *>(
      MEM_callocN(sizeof(GizmoGroupPreSelElem), __func__));
  gzgroup->customdata = ggd;

  wmGizmo *gz = ggd->gizmo = WM_gizmo_new_ptr(gzt_presel, gzgroup, nullptr);
  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
  UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);
}

void VIEW3D_GGT_mesh_preselect_elem(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Mesh Preselect Element";
  gzgt->idname = "VIEW3D_GGT_mesh_preselect_elem";

  gzgt->flag = WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP | WM_GIZMOGROUPTYPE_3D;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = ED_gizmo_poll_or_unlink_delayed_from_tool;
  gzgt->setup = WIDGETGROUP_mesh_preselect_elem_setup;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Pre-Select Edge Ring Gizmo
 * \{ */

struct GizmoGroupPreSelEdgeRing {
  wmGizmo *gizmo;
};

static void WIDGETGROUP_mesh_preselect_edgering_setup(const bContext * /*C*/,
                                                      wmGizmoGroup *gzgroup)
{
  const wmGizmoType *gzt_presel = WM_gizmotype_find("GIZMO_GT_mesh_preselect_edgering_3d", true);
  GizmoGroupPreSelEdgeRing *ggd = static_cast<GizmoGroupPreSelEdgeRing *>(
      MEM_callocN(sizeof(GizmoGroupPreSelEdgeRing), __func__));
  gzgroup->customdata = ggd;

  wmGizmo *gz = ggd->gizmo = WM_gizmo_new_ptr(gzt_presel, gzgroup, nullptr);
  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
  UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);
}

void VIEW3D_GGT_mesh_preselect_edgering(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Mesh Preselect Edge Ring";
  gzgt->idname = "VIEW3D_GGT_mesh_preselect_edgering";

  gzgt->flag = WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP | WM_GIZMOGROUPTYPE_3D;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = ED_gizmo_poll_or_unlink_delayed_from_tool;
  gzgt->setup = WIDGETGROUP_mesh_preselect_edgering_setup;
}

/** \} */
