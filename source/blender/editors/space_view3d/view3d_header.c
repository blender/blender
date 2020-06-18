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
 * The Original Code is Copyright (C) 2004-2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spview3d
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_undo.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "view3d_intern.h"

static void do_view3d_header_buttons(bContext *C, void *arg, int event);

#define B_SEL_VERT 110
#define B_SEL_EDGE 111
#define B_SEL_FACE 112

/* -------------------------------------------------------------------- */
/** \name Toggle Matcap Flip Operator
 * \{ */

static int toggle_matcap_flip(bContext *C, wmOperator *UNUSED(op))
{
  View3D *v3d = CTX_wm_view3d(C);

  if (v3d) {
    v3d->shading.flag ^= V3D_SHADING_MATCAP_FLIP_X;
    ED_view3d_shade_update(CTX_data_main(C), v3d, CTX_wm_area(C));
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
  }
  else {
    Scene *scene = CTX_data_scene(C);
    scene->display.shading.flag ^= V3D_SHADING_MATCAP_FLIP_X;
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
    WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_toggle_matcap_flip(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flip MatCap";
  ot->description = "Flip MatCap";
  ot->idname = "VIEW3D_OT_toggle_matcap_flip";

  /* api callbacks */
  ot->exec = toggle_matcap_flip;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Templates
 * \{ */

static void do_view3d_header_buttons(bContext *C, void *UNUSED(arg), int event)
{
  wmWindow *win = CTX_wm_window(C);
  const int ctrl = win->eventstate->ctrl, shift = win->eventstate->shift;

  /* watch it: if area->win does not exist, check that when calling direct drawing routines */

  switch (event) {
    case B_SEL_VERT:
      if (EDBM_selectmode_toggle_multi(C, SCE_SELECT_VERTEX, -1, shift, ctrl)) {
        ED_undo_push(C, "Selectmode Set: Vertex");
      }
      break;
    case B_SEL_EDGE:
      if (EDBM_selectmode_toggle_multi(C, SCE_SELECT_EDGE, -1, shift, ctrl)) {
        ED_undo_push(C, "Selectmode Set: Edge");
      }
      break;
    case B_SEL_FACE:
      if (EDBM_selectmode_toggle_multi(C, SCE_SELECT_FACE, -1, shift, ctrl)) {
        ED_undo_push(C, "Selectmode Set: Face");
      }
      break;
    default:
      break;
  }
}

void uiTemplateEditModeSelection(uiLayout *layout, struct bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  uiBlock *block = uiLayoutGetBlock(layout);

  UI_block_func_handle_set(block, do_view3d_header_buttons, NULL);

  if (obedit && (obedit->type == OB_MESH)) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    uiLayout *row;
    uiBut *but;

    row = uiLayoutRow(layout, true);
    block = uiLayoutGetBlock(row);
    but = uiDefIconButBitS(
        block,
        UI_BTYPE_TOGGLE,
        SCE_SELECT_VERTEX,
        B_SEL_VERT,
        ICON_VERTEXSEL,
        0,
        0,
        UI_UNIT_X,
        UI_UNIT_Y,
        &em->selectmode,
        1.0,
        0.0,
        0,
        0,
        TIP_("Vertex select - Shift-Click for multiple modes, Ctrl-Click contracts selection"));
    UI_but_flag_disable(but, UI_BUT_UNDO);
    but = uiDefIconButBitS(
        block,
        UI_BTYPE_TOGGLE,
        SCE_SELECT_EDGE,
        B_SEL_EDGE,
        ICON_EDGESEL,
        0,
        0,
        ceilf(UI_UNIT_X - U.pixelsize),
        UI_UNIT_Y,
        &em->selectmode,
        1.0,
        0.0,
        0,
        0,
        TIP_("Edge select - Shift-Click for multiple modes, "
             "Ctrl-Click expands/contracts selection depending on the current mode"));
    UI_but_flag_disable(but, UI_BUT_UNDO);
    but = uiDefIconButBitS(
        block,
        UI_BTYPE_TOGGLE,
        SCE_SELECT_FACE,
        B_SEL_FACE,
        ICON_FACESEL,
        0,
        0,
        ceilf(UI_UNIT_X - U.pixelsize),
        UI_UNIT_Y,
        &em->selectmode,
        1.0,
        0.0,
        0,
        0,
        TIP_("Face select - Shift-Click for multiple modes, Ctrl-Click expands selection"));
    UI_but_flag_disable(but, UI_BUT_UNDO);
  }
}

static void uiTemplatePaintModeSelection(uiLayout *layout, struct bContext *C)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);

  /* Gizmos aren't used in paint modes */
  if (!ELEM(ob->mode, OB_MODE_SCULPT, OB_MODE_PARTICLE_EDIT)) {
    /* masks aren't used for sculpt and particle painting */
    PointerRNA meshptr;

    RNA_pointer_create(ob->data, &RNA_Mesh, ob->data, &meshptr);
    if (ob->mode & OB_MODE_TEXTURE_PAINT) {
      uiItemR(layout, &meshptr, "use_paint_mask", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
    }
    else {
      uiLayout *row = uiLayoutRow(layout, true);
      uiItemR(row, &meshptr, "use_paint_mask", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
      uiItemR(row, &meshptr, "use_paint_mask_vertex", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
    }
  }
}

void uiTemplateHeader3D_mode(uiLayout *layout, struct bContext *C)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  Object *obedit = CTX_data_edit_object(C);
  bGPdata *gpd = CTX_data_gpencil_data(C);

  bool is_paint = (ob && !(gpd && (gpd->flag & GP_DATA_STROKE_EDITMODE)) &&
                   ELEM(ob->mode,
                        OB_MODE_SCULPT,
                        OB_MODE_VERTEX_PAINT,
                        OB_MODE_WEIGHT_PAINT,
                        OB_MODE_TEXTURE_PAINT));

  uiTemplateEditModeSelection(layout, C);
  if ((obedit == NULL) && is_paint) {
    uiTemplatePaintModeSelection(layout, C);
  }
}

/** \} */
