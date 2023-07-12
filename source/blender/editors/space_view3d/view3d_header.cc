/* SPDX-FileCopyrightText: 2004-2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_undo.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "view3d_intern.h"

/* -------------------------------------------------------------------- */
/** \name Toggle Matcap Flip Operator
 * \{ */

static int toggle_matcap_flip(bContext *C, wmOperator * /*op*/)
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

void uiTemplateEditModeSelection(uiLayout *layout, bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (!obedit || obedit->type != OB_MESH) {
    return;
  }

  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  uiLayout *row = uiLayoutRow(layout, true);

  PointerRNA op_ptr;
  wmOperatorType *ot = WM_operatortype_find("MESH_OT_select_mode", true);
  uiItemFullO_ptr(row,
                  ot,
                  "",
                  ICON_VERTEXSEL,
                  nullptr,
                  WM_OP_INVOKE_DEFAULT,
                  (em->selectmode & SCE_SELECT_VERTEX) ? UI_ITEM_O_DEPRESS : 0,
                  &op_ptr);
  RNA_enum_set(&op_ptr, "type", SCE_SELECT_VERTEX);
  uiItemFullO_ptr(row,
                  ot,
                  "",
                  ICON_EDGESEL,
                  nullptr,
                  WM_OP_INVOKE_DEFAULT,
                  (em->selectmode & SCE_SELECT_EDGE) ? UI_ITEM_O_DEPRESS : 0,
                  &op_ptr);
  RNA_enum_set(&op_ptr, "type", SCE_SELECT_EDGE);
  uiItemFullO_ptr(row,
                  ot,
                  "",
                  ICON_FACESEL,
                  nullptr,
                  WM_OP_INVOKE_DEFAULT,
                  (em->selectmode & SCE_SELECT_FACE) ? UI_ITEM_O_DEPRESS : 0,
                  &op_ptr);
  RNA_enum_set(&op_ptr, "type", SCE_SELECT_FACE);
}

static void uiTemplatePaintModeSelection(uiLayout *layout, bContext *C)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  /* Gizmos aren't used in paint modes */
  if (!ELEM(ob->mode, OB_MODE_SCULPT, OB_MODE_PARTICLE_EDIT)) {
    /* masks aren't used for sculpt and particle painting */
    PointerRNA meshptr;

    RNA_pointer_create(static_cast<ID *>(ob->data), &RNA_Mesh, ob->data, &meshptr);
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

void uiTemplateHeader3D_mode(uiLayout *layout, bContext *C)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Object *obedit = CTX_data_edit_object(C);
  bGPdata *gpd = CTX_data_gpencil_data(C);

  bool is_paint = (ob && !(gpd && (gpd->flag & GP_DATA_STROKE_EDITMODE)) &&
                   ELEM(ob->mode,
                        OB_MODE_SCULPT,
                        OB_MODE_VERTEX_PAINT,
                        OB_MODE_WEIGHT_PAINT,
                        OB_MODE_TEXTURE_PAINT));

  uiTemplateEditModeSelection(layout, C);
  if ((obedit == nullptr) && is_paint) {
    uiTemplatePaintModeSelection(layout, C);
  }
}

/** \} */
