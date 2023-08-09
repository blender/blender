/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "ED_gizmo_library.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_view2d.hh"

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

static NavigateGizmoInfo g_navigate_params_for_space_image[GZ_INDEX_TOTAL] = {
    {
        "IMAGE_OT_view_pan",
        "GIZMO_GT_button_2d",
        ICON_VIEW_PAN,
    },
    {
        "IMAGE_OT_view_zoom",
        "GIZMO_GT_button_2d",
        ICON_VIEW_ZOOM,
    },
};

static NavigateGizmoInfo g_navigate_params_for_space_clip[GZ_INDEX_TOTAL] = {
    {
        "CLIP_OT_view_pan",
        "GIZMO_GT_button_2d",
        ICON_VIEW_PAN,
    },
    {
        "CLIP_OT_view_zoom",
        "GIZMO_GT_button_2d",
        ICON_VIEW_ZOOM,
    },
};

static NavigateGizmoInfo g_navigate_params_for_view2d[GZ_INDEX_TOTAL] = {
    {
        "VIEW2D_OT_pan",
        "GIZMO_GT_button_2d",
        ICON_VIEW_PAN,
    },
    {
        "VIEW2D_OT_zoom",
        "GIZMO_GT_button_2d",
        ICON_VIEW_ZOOM,
    },
};

static NavigateGizmoInfo *navigate_params_from_space_type(short space_type)
{
  switch (space_type) {
    case SPACE_IMAGE:
      return g_navigate_params_for_space_image;
    case SPACE_CLIP:
      return g_navigate_params_for_space_clip;
    default:
      /* Used for sequencer. */
      return g_navigate_params_for_view2d;
  }
}

struct NavigateWidgetGroup {
  wmGizmo *gz_array[GZ_INDEX_TOTAL];
  /* Store the view state to check for changes. */
  struct {
    rcti rect_visible;
  } state;
};

static bool WIDGETGROUP_navigate_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  if ((U.uiflag & USER_SHOW_GIZMO_NAVIGATE) == 0) {
    return false;
  }
  ScrArea *area = CTX_wm_area(C);
  if (area == nullptr) {
    return false;
  }
  switch (area->spacetype) {
    case SPACE_SEQ: {
      const SpaceSeq *sseq = static_cast<const SpaceSeq *>(area->spacedata.first);
      if (sseq->gizmo_flag & (SEQ_GIZMO_HIDE | SEQ_GIZMO_HIDE_NAVIGATE)) {
        return false;
      }
      break;
    }
    case SPACE_IMAGE: {
      const SpaceImage *sima = static_cast<const SpaceImage *>(area->spacedata.first);
      if (sima->gizmo_flag & (SI_GIZMO_HIDE | SI_GIZMO_HIDE_NAVIGATE)) {
        return false;
      }
      break;
    }
    case SPACE_CLIP: {
      const SpaceClip *sc = static_cast<const SpaceClip *>(area->spacedata.first);
      if (sc->gizmo_flag & (SCLIP_GIZMO_HIDE | SCLIP_GIZMO_HIDE_NAVIGATE)) {
        return false;
      }
      break;
    }
  }
  return true;
}

static void WIDGETGROUP_navigate_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  NavigateWidgetGroup *navgroup = MEM_cnew<NavigateWidgetGroup>(__func__);

  const NavigateGizmoInfo *navigate_params = navigate_params_from_space_type(
      gzgroup->type->gzmap_params.spaceid);

  for (int i = 0; i < GZ_INDEX_TOTAL; i++) {
    const NavigateGizmoInfo *info = &navigate_params[i];
    navgroup->gz_array[i] = WM_gizmo_new(info->gizmo, gzgroup, nullptr);
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

    wmOperatorType *ot = WM_operatortype_find(info->opname, true);
    WM_gizmo_operator_set(gz, 0, ot, nullptr);
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
  NavigateWidgetGroup *navgroup = static_cast<NavigateWidgetGroup *>(gzgroup->customdata);
  ARegion *region = CTX_wm_region(C);

  const rcti *rect_visible = ED_region_visible_rect(region);

  if ((navgroup->state.rect_visible.xmax == rect_visible->xmax) &&
      (navgroup->state.rect_visible.ymax == rect_visible->ymax))
  {
    return;
  }

  navgroup->state.rect_visible = *rect_visible;

  const float icon_size = GIZMO_SIZE;
  const float icon_offset_mini = icon_size * GIZMO_MINI_OFFSET_FAC * UI_SCALE_FAC;
  const float co[2] = {
      roundf(rect_visible->xmax - (icon_offset_mini * 0.75f)),
      roundf(rect_visible->ymax - (icon_offset_mini * 0.75f)),
  };

  wmGizmo *gz;

  for (uint i = 0; i < ARRAY_SIZE(navgroup->gz_array); i++) {
    gz = navgroup->gz_array[i];
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
  }

  int icon_mini_slot = 0;

  gz = navgroup->gz_array[GZ_INDEX_ZOOM];
  gz->matrix_basis[3][0] = roundf(co[0]);
  gz->matrix_basis[3][1] = roundf(co[1] - (icon_offset_mini * icon_mini_slot++));
  WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);

  gz = navgroup->gz_array[GZ_INDEX_MOVE];
  gz->matrix_basis[3][0] = roundf(co[0]);
  gz->matrix_basis[3][1] = roundf(co[1] - (icon_offset_mini * icon_mini_slot++));
  WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);
}

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
