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
 * \ingroup spview3d
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "ED_gizmo_utils.h"
#include "ED_screen.h"

#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "view3d_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Mesh Pre-Select Element Gizmo
 *
 * \{ */

struct GizmoGroupPreSelElem {
  wmGizmo *gizmo;
};

static void WIDGETGROUP_mesh_preselect_elem_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
  const wmGizmoType *gzt_presel = WM_gizmotype_find("GIZMO_GT_mesh_preselect_elem_3d", true);
  struct GizmoGroupPreSelElem *ggd = MEM_callocN(sizeof(struct GizmoGroupPreSelElem), __func__);
  gzgroup->customdata = ggd;

  wmGizmo *gz = ggd->gizmo = WM_gizmo_new_ptr(gzt_presel, gzgroup, NULL);
  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
  UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);
}

void VIEW3D_GGT_mesh_preselect_elem(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Mesh Preselect Element";
  gzgt->idname = "VIEW3D_GGT_mesh_preselect_elem";

  gzgt->flag = WM_GIZMOGROUPTYPE_3D;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = ED_gizmo_poll_or_unlink_delayed_from_tool;
  gzgt->setup = WIDGETGROUP_mesh_preselect_elem_setup;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Pre-Select Edge Ring Gizmo
 *
 * \{ */

struct GizmoGroupPreSelEdgeRing {
  wmGizmo *gizmo;
};

static void WIDGETGROUP_mesh_preselect_edgering_setup(const bContext *UNUSED(C),
                                                      wmGizmoGroup *gzgroup)
{
  const wmGizmoType *gzt_presel = WM_gizmotype_find("GIZMO_GT_mesh_preselect_edgering_3d", true);
  struct GizmoGroupPreSelEdgeRing *ggd = MEM_callocN(sizeof(struct GizmoGroupPreSelEdgeRing),
                                                     __func__);
  gzgroup->customdata = ggd;

  wmGizmo *gz = ggd->gizmo = WM_gizmo_new_ptr(gzt_presel, gzgroup, NULL);
  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
  UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);
}

void VIEW3D_GGT_mesh_preselect_edgering(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Mesh Preselect Edge Ring";
  gzgt->idname = "VIEW3D_GGT_mesh_preselect_edgering";

  gzgt->flag = WM_GIZMOGROUPTYPE_3D;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = ED_gizmo_poll_or_unlink_delayed_from_tool;
  gzgt->setup = WIDGETGROUP_mesh_preselect_edgering_setup;
}

/** \} */
