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
 * \ingroup edinterface
 */

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "ED_screen.h"
#include "ED_gizmo_library.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

/* -------------------------------------------------------------------- */
/** \name View2D Navigation Gizmo Group
 *
 * A simpler version of #VIEW3D_GGT_navigate
 *
 * Written to be used by different kinds of 2D view types.
 * \{ */

/* Size of main icon. */
#define GIZMO_SIZE 80
/* Factor for size of smaller button. */
#define GIZMO_MINI_FAC 0.35f
/* How much mini buttons offset from the primary. */
#define GIZMO_MINI_OFFSET_FAC 0.38f

enum {
  GZ_INDEX_MOVE = 0,
  GZ_INDEX_ZOOM = 1,

  GZ_INDEX_TOTAL = 2,
};

struct NavigateGizmoInfo {
  const char *opname;
  const char *gizmo;
  uint icon;
};

static struct NavigateGizmoInfo g_navigate_params_for_space_image[GZ_INDEX_TOTAL] = {
    {
        .opname = "IMAGE_OT_view_pan",
        .gizmo = "GIZMO_GT_button_2d",
        ICON_VIEW_PAN,
    },
    {
        .opname = "IMAGE_OT_view_zoom",
        .gizmo = "GIZMO_GT_button_2d",
        ICON_VIEW_ZOOM,
    },
};

static struct NavigateGizmoInfo g_navigate_params_for_space_clip[GZ_INDEX_TOTAL] = {
    {
        .opname = "CLIP_OT_view_pan",
        .gizmo = "GIZMO_GT_button_2d",
        ICON_VIEW_PAN,
    },
    {
        .opname = "CLIP_OT_view_zoom",
        .gizmo = "GIZMO_GT_button_2d",
        ICON_VIEW_ZOOM,
    },
};

static struct NavigateGizmoInfo g_navigate_params_for_view2d[GZ_INDEX_TOTAL] = {
    {
        .opname = "VIEW2D_OT_pan",
        .gizmo = "GIZMO_GT_button_2d",
        ICON_VIEW_PAN,
    },
    {
        .opname = "VIEW2D_OT_zoom",
        .gizmo = "GIZMO_GT_button_2d",
        ICON_VIEW_ZOOM,
    },
};

static struct NavigateGizmoInfo *navigate_params_from_space_type(short space_type)
{
  switch (space_type) {
    case SPACE_IMAGE:
      return g_navigate_params_for_space_image;
    case SPACE_CLIP:
      return g_navigate_params_for_space_clip;
    default:
      /* Used for sequencer.  */
      return g_navigate_params_for_view2d;
  }
}

struct NavigateWidgetGroup {
  wmGizmo *gz_array[GZ_INDEX_TOTAL];
  /* Store the view state to check for changes. */
  struct {
    rcti rect_visible;
  } state;
  int region_size[2];
};

static bool WIDGETGROUP_navigate_poll(const bContext *UNUSED(C), wmGizmoGroupType *UNUSED(gzgt))
{
  if ((U.uiflag & USER_SHOW_GIZMO_NAVIGATE) == 0) {
    return false;
  }
  return true;
}

static void WIDGETGROUP_navigate_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
  struct NavigateWidgetGroup *navgroup = MEM_callocN(sizeof(struct NavigateWidgetGroup), __func__);

  navgroup->region_size[0] = -1;
  navgroup->region_size[1] = -1;

  const struct NavigateGizmoInfo *navigate_params = navigate_params_from_space_type(
      gzgroup->type->gzmap_params.spaceid);

  for (int i = 0; i < GZ_INDEX_TOTAL; i++) {
    const struct NavigateGizmoInfo *info = &navigate_params[i];
    navgroup->gz_array[i] = WM_gizmo_new(info->gizmo, gzgroup, NULL);
    wmGizmo *gz = navgroup->gz_array[i];
    gz->flag |= WM_GIZMO_MOVE_CURSOR | WM_GIZMO_DRAW_MODAL;

    {
      uchar icon_color[3];
      UI_GetThemeColor3ubv(TH_TEXT, icon_color);
      int color_tint, color_tint_hi;
      if (icon_color[0] > 128) {
        color_tint = -40;
        color_tint_hi = 60;
        gz->color[3] = 0.5f;
        gz->color_hi[3] = 0.5f;
      }
      else {
        color_tint = 60;
        color_tint_hi = 60;
        gz->color[3] = 0.5f;
        gz->color_hi[3] = 0.75f;
      }
      UI_GetThemeColorShade3fv(TH_HEADER, color_tint, gz->color);
      UI_GetThemeColorShade3fv(TH_HEADER, color_tint_hi, gz->color_hi);
    }

    /* may be overwritten later */
    gz->scale_basis = (GIZMO_SIZE * GIZMO_MINI_FAC) / 2;
    if (info->icon != 0) {
      PropertyRNA *prop = RNA_struct_find_property(gz->ptr, "icon");
      RNA_property_enum_set(gz->ptr, prop, info->icon);
      RNA_enum_set(
          gz->ptr, "draw_options", ED_GIZMO_BUTTON_SHOW_OUTLINE | ED_GIZMO_BUTTON_SHOW_BACKDROP);
    }

    /* Not needed, just match 3D view where it is needed. */
    WM_gizmo_set_flag(gz, WM_GIZMO_EVENT_HANDLE_ALL, true);

    wmOperatorType *ot = WM_operatortype_find(info->opname, true);
    WM_gizmo_operator_set(gz, 0, ot, NULL);
  }

  /* Modal operators, don't use initial mouse location since we're clicking on a button. */
  {
    int gz_ids[] = {GZ_INDEX_ZOOM};
    for (int i = 0; i < ARRAY_SIZE(gz_ids); i++) {
      wmGizmo *gz = navgroup->gz_array[gz_ids[i]];
      wmGizmoOpElem *gzop = WM_gizmo_operator_get(gz, 0);
      RNA_boolean_set(&gzop->ptr, "use_cursor_init", false);
    }
  }

  gzgroup->customdata = navgroup;
}

static void WIDGETGROUP_navigate_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  struct NavigateWidgetGroup *navgroup = gzgroup->customdata;
  ARegion *ar = CTX_wm_region(C);

  rcti rect_visible;
  ED_region_visible_rect(ar, &rect_visible);

  if ((navgroup->state.rect_visible.xmax == rect_visible.xmax) &&
      (navgroup->state.rect_visible.ymax == rect_visible.ymax)) {
    return;
  }

  navgroup->state.rect_visible = rect_visible;

  const float icon_size = GIZMO_SIZE;
  const float icon_offset_mini = icon_size * GIZMO_MINI_OFFSET_FAC * UI_DPI_FAC;
  const float co[2] = {
      rect_visible.xmax - (icon_offset_mini * 0.75f),
      rect_visible.ymax - (icon_offset_mini * 0.75f),
  };

  wmGizmo *gz;

  for (uint i = 0; i < ARRAY_SIZE(navgroup->gz_array); i++) {
    gz = navgroup->gz_array[i];
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
  }

  int icon_mini_slot = 0;

  gz = navgroup->gz_array[GZ_INDEX_ZOOM];
  gz->matrix_basis[3][0] = co[0] - (icon_offset_mini * icon_mini_slot++);
  gz->matrix_basis[3][1] = co[1];
  WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);

  gz = navgroup->gz_array[GZ_INDEX_MOVE];
  gz->matrix_basis[3][0] = co[0] - (icon_offset_mini * icon_mini_slot++);
  gz->matrix_basis[3][1] = co[1];
  WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);
}

/* Caller defines the name for gizmo group. */
void VIEW2D_GGT_navigate_impl(wmGizmoGroupType *gzgt, const char *idname)
{
  gzgt->name = "View2D Navigate";
  gzgt->idname = idname;

  gzgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT | WM_GIZMOGROUPTYPE_SCALE |
                 WM_GIZMOGROUPTYPE_DRAW_MODAL_ALL);

  gzgt->poll = WIDGETGROUP_navigate_poll;
  gzgt->setup = WIDGETGROUP_navigate_setup;
  gzgt->draw_prepare = WIDGETGROUP_navigate_draw_prepare;
}

/** \} */
