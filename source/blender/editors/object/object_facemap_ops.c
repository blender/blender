/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_workspace_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_object_facemap.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_object.h"

#include "object_intern.h"

void ED_object_facemap_face_add(Object *ob, bFaceMap *fmap, int facenum)
{
  int fmap_nr;
  if (GS(((ID *)ob->data)->name) != ID_ME) {
    return;
  }

  /* get the face map number, exit if it can't be found */
  fmap_nr = BLI_findindex(&ob->fmaps, fmap);

  if (fmap_nr != -1) {
    int *facemap;
    Mesh *me = ob->data;

    /* if there's is no facemap layer then create one */
    if ((facemap = CustomData_get_layer_for_write(&me->pdata, CD_FACEMAP, me->totpoly)) == NULL) {
      facemap = CustomData_add_layer(&me->pdata, CD_FACEMAP, CD_SET_DEFAULT, me->totpoly);
    }

    facemap[facenum] = fmap_nr;
  }
}

void ED_object_facemap_face_remove(Object *ob, bFaceMap *fmap, int facenum)
{
  int fmap_nr;
  if (GS(((ID *)ob->data)->name) != ID_ME) {
    return;
  }

  /* get the face map number, exit if it can't be found */
  fmap_nr = BLI_findindex(&ob->fmaps, fmap);

  if (fmap_nr != -1) {
    int *facemap;
    Mesh *me = ob->data;

    if ((facemap = CustomData_get_layer_for_write(&me->pdata, CD_FACEMAP, me->totpoly)) == NULL) {
      return;
    }

    facemap[facenum] = -1;
  }
}

static void object_fmap_remap_edit_mode(Object *ob, const int *remap)
{
  if (ob->type != OB_MESH) {
    return;
  }

  Mesh *me = ob->data;
  if (me->edit_mesh) {
    BMEditMesh *em = me->edit_mesh;
    const int cd_fmap_offset = CustomData_get_offset(&em->bm->pdata, CD_FACEMAP);

    if (cd_fmap_offset != -1) {
      BMFace *efa;
      BMIter iter;
      int *map;

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        map = BM_ELEM_CD_GET_VOID_P(efa, cd_fmap_offset);

        if (map && *map != -1) {
          *map = remap[*map];
        }
      }
    }
  }
}

static void object_fmap_remap_object_mode(Object *ob, const int *remap)
{
  if (ob->type != OB_MESH) {
    return;
  }

  Mesh *me = ob->data;
  if (CustomData_has_layer(&me->pdata, CD_FACEMAP)) {
    int *map = CustomData_get_layer_for_write(&me->pdata, CD_FACEMAP, me->totpoly);
    if (map) {
      for (int i = 0; i < me->totpoly; i++) {
        if (map[i] != -1) {
          map[i] = remap[map[i]];
        }
      }
    }
  }
}

static void object_facemap_remap(Object *ob, const int *remap)
{
  if (BKE_object_is_in_editmode(ob)) {
    object_fmap_remap_edit_mode(ob, remap);
  }
  else {
    object_fmap_remap_object_mode(ob, remap);
  }
}

static bool face_map_supported_poll(bContext *C)
{
  Object *ob = ED_object_context(C);
  ID *data = (ob) ? ob->data : NULL;
  return (ob && !ID_IS_LINKED(ob) && !ID_IS_OVERRIDE_LIBRARY(ob) && ob->type == OB_MESH && data &&
          !ID_IS_LINKED(data) && !ID_IS_OVERRIDE_LIBRARY(data));
}

static bool face_map_supported_edit_mode_poll(bContext *C)
{
  Object *ob = ED_object_context(C);

  if (face_map_supported_poll(C)) {
    if (ob->mode == OB_MODE_EDIT) {
      return true;
    }
  }
  return false;
}

static bool face_map_supported_remove_poll(bContext *C)
{
  if (!face_map_supported_poll(C)) {
    return false;
  }

  Object *ob = ED_object_context(C);
  bFaceMap *fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);
  if (fmap) {
    return true;
  }

  return false;
}

static int face_map_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_object_context(C);

  BKE_object_facemap_add(ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Face Map";
  ot->idname = "OBJECT_OT_face_map_add";
  ot->description = "Add a new face map to the active object";

  /* api callbacks */
  ot->poll = face_map_supported_poll;
  ot->exec = face_map_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int face_map_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_object_context(C);
  bFaceMap *fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);

  if (fmap) {
    BKE_object_facemap_remove(ob, fmap);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  }
  return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Face Map";
  ot->idname = "OBJECT_OT_face_map_remove";
  ot->description = "Remove a face map from the active object";

  /* api callbacks */
  ot->poll = face_map_supported_remove_poll;
  ot->exec = face_map_remove_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int face_map_assign_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_object_context(C);
  bFaceMap *fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);

  if (fmap) {
    Mesh *me = ob->data;
    BMEditMesh *em = me->edit_mesh;
    BMFace *efa;
    BMIter iter;
    int *map;
    int cd_fmap_offset;

    if (!CustomData_has_layer(&em->bm->pdata, CD_FACEMAP)) {
      BM_data_layer_add(em->bm, &em->bm->pdata, CD_FACEMAP);
    }

    cd_fmap_offset = CustomData_get_offset(&em->bm->pdata, CD_FACEMAP);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      map = BM_ELEM_CD_GET_VOID_P(efa, cd_fmap_offset);

      if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
        *map = ob->actfmap - 1;
      }
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  }
  return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_assign(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Assign Face Map";
  ot->idname = "OBJECT_OT_face_map_assign";
  ot->description = "Assign faces to a face map";

  /* api callbacks */
  ot->poll = face_map_supported_edit_mode_poll;
  ot->exec = face_map_assign_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int face_map_remove_from_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_object_context(C);
  bFaceMap *fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);

  if (fmap) {
    Mesh *me = ob->data;
    BMEditMesh *em = me->edit_mesh;
    BMFace *efa;
    BMIter iter;
    int *map;
    int cd_fmap_offset;
    int mapindex = ob->actfmap - 1;

    if (!CustomData_has_layer(&em->bm->pdata, CD_FACEMAP)) {
      return OPERATOR_CANCELLED;
    }

    cd_fmap_offset = CustomData_get_offset(&em->bm->pdata, CD_FACEMAP);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      map = BM_ELEM_CD_GET_VOID_P(efa, cd_fmap_offset);

      if (BM_elem_flag_test(efa, BM_ELEM_SELECT) && *map == mapindex) {
        *map = -1;
      }
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  }
  return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_remove_from(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove from Face Map";
  ot->idname = "OBJECT_OT_face_map_remove_from";
  ot->description = "Remove faces from a face map";

  /* api callbacks */
  ot->poll = face_map_supported_edit_mode_poll;
  ot->exec = face_map_remove_from_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void fmap_select(Object *ob, bool select)
{
  Mesh *me = ob->data;
  BMEditMesh *em = me->edit_mesh;
  BMFace *efa;
  BMIter iter;
  int *map;
  int cd_fmap_offset;
  int mapindex = ob->actfmap - 1;

  if (!CustomData_has_layer(&em->bm->pdata, CD_FACEMAP)) {
    BM_data_layer_add(em->bm, &em->bm->pdata, CD_FACEMAP);
  }

  cd_fmap_offset = CustomData_get_offset(&em->bm->pdata, CD_FACEMAP);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    map = BM_ELEM_CD_GET_VOID_P(efa, cd_fmap_offset);

    if (*map == mapindex) {
      BM_face_select_set(em->bm, efa, select);
    }
  }
}

static int face_map_select_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_object_context(C);
  bFaceMap *fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);

  if (fmap) {
    fmap_select(ob, true);

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  }
  return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Face Map Faces";
  ot->idname = "OBJECT_OT_face_map_select";
  ot->description = "Select faces belonging to a face map";

  /* api callbacks */
  ot->poll = face_map_supported_edit_mode_poll;
  ot->exec = face_map_select_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int face_map_deselect_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_object_context(C);
  bFaceMap *fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);

  if (fmap) {
    fmap_select(ob, false);

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  }
  return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_deselect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Deselect Face Map Faces";
  ot->idname = "OBJECT_OT_face_map_deselect";
  ot->description = "Deselect faces belonging to a face map";

  /* api callbacks */
  ot->poll = face_map_supported_edit_mode_poll;
  ot->exec = face_map_deselect_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int face_map_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  bFaceMap *fmap;
  int dir = RNA_enum_get(op->ptr, "direction");

  fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);
  if (!fmap) {
    return OPERATOR_CANCELLED;
  }

  if (!fmap->prev && !fmap->next) {
    return OPERATOR_CANCELLED;
  }

  int pos1 = BLI_findindex(&ob->fmaps, fmap);
  int pos2 = pos1 - dir;
  int len = BLI_listbase_count(&ob->fmaps);
  int *map = MEM_mallocN(len * sizeof(*map), __func__);

  if (!IN_RANGE(pos2, -1, len)) {
    const int offset = len - dir;
    for (int i = 0; i < len; i++) {
      map[i] = (i + offset) % len;
    }
    pos2 = map[pos1];
  }
  else {
    range_vn_i(map, len, 0);
    SWAP(int, map[pos1], map[pos2]);
  }

  void *prev = fmap->prev;
  void *next = fmap->next;
  BLI_remlink(&ob->fmaps, fmap);
  if (dir == 1) { /*up*/
    BLI_insertlinkbefore(&ob->fmaps, prev, fmap);
  }
  else { /*down*/
    BLI_insertlinkafter(&ob->fmaps, next, fmap);
  }

  /* Iterate through mesh and substitute the indices as necessary. */
  object_facemap_remap(ob, map);
  MEM_freeN(map);

  ob->actfmap = pos2 + 1;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_move(wmOperatorType *ot)
{
  static EnumPropertyItem fmap_slot_move[] = {
      {1, "UP", 0, "Up", ""},
      {-1, "DOWN", 0, "Down", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Move Face Map";
  ot->idname = "OBJECT_OT_face_map_move";
  ot->description = "Move the active face map up/down in the list";

  /* api callbacks */
  ot->poll = face_map_supported_poll;
  ot->exec = face_map_move_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(
      ot->srna, "direction", fmap_slot_move, 0, "Direction", "Direction to move, up or down");
}
