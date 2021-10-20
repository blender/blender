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
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_base.h"
#include "BLI_math_color.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"

#include "paint_intern.h" /* own include */

static bool vertex_weight_paint_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *me = BKE_mesh_from_object(ob);
  return (ob && (ELEM(ob->mode, OB_MODE_VERTEX_PAINT, OB_MODE_WEIGHT_PAINT))) &&
         (me && me->totpoly && me->dvert);
}

static void tag_object_after_update(Object *object)
{
  BLI_assert(object->type == OB_MESH);
  Mesh *mesh = object->data;
  DEG_id_tag_update(&mesh->id, ID_RECALC_COPY_ON_WRITE);
  /* NOTE: Original mesh is used for display, so tag it directly here. */
  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);
}

/* -------------------------------------------------------------------- */
/** \name Set Vertex Colors Operator
 * \{ */

static bool vertex_color_set(Object *ob, uint paintcol)
{
  Mesh *me;
  if (((me = BKE_mesh_from_object(ob)) == NULL) || (ED_mesh_color_ensure(me, NULL) == false)) {
    return false;
  }

  const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

  const MPoly *mp = me->mpoly;
  for (int i = 0; i < me->totpoly; i++, mp++) {
    MLoopCol *lcol = me->mloopcol + mp->loopstart;

    if (use_face_sel && !(mp->flag & ME_FACE_SEL)) {
      continue;
    }

    int j = 0;
    do {
      uint vidx = me->mloop[mp->loopstart + j].v;
      if (!(use_vert_sel && !(me->mvert[vidx].flag & SELECT))) {
        *(int *)lcol = paintcol;
      }
      lcol++;
      j++;
    } while (j < mp->totloop);
  }

  /* remove stale me->mcol, will be added later */
  BKE_mesh_tessface_clear(me);

  tag_object_after_update(ob);

  return true;
}

static int vertex_color_set_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Object *obact = CTX_data_active_object(C);
  uint paintcol = vpaint_get_current_col(scene, scene->toolsettings->vpaint, false);

  if (vertex_color_set(obact, paintcol)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Vertex Colors";
  ot->idname = "PAINT_OT_vertex_color_set";
  ot->description = "Fill the active vertex color layer with the current paint color";

  /* api callbacks */
  ot->exec = vertex_color_set_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Color from Weight Operator
 * \{ */

static bool vertex_paint_from_weight(Object *ob)
{
  Mesh *me;
  const MPoly *mp;
  int vgroup_active;

  if (((me = BKE_mesh_from_object(ob)) == NULL || (ED_mesh_color_ensure(me, NULL)) == false)) {
    return false;
  }

  /* TODO: respect selection. */
  /* TODO: Do we want to take weights from evaluated mesh instead? 2.7x was not doing it anyway. */
  mp = me->mpoly;
  vgroup_active = me->vertex_group_active_index - 1;
  for (int i = 0; i < me->totpoly; i++, mp++) {
    MLoopCol *lcol = &me->mloopcol[mp->loopstart];
    uint j = 0;
    do {
      uint vidx = me->mloop[mp->loopstart + j].v;
      const float weight = BKE_defvert_find_weight(&me->dvert[vidx], vgroup_active);
      const uchar grayscale = weight * 255;
      lcol->r = grayscale;
      lcol->b = grayscale;
      lcol->g = grayscale;
      lcol++;
      j++;
    } while (j < mp->totloop);
  }

  tag_object_after_update(ob);

  return true;
}

static int vertex_paint_from_weight_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *obact = CTX_data_active_object(C);
  if (vertex_paint_from_weight(obact)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_from_weight(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Color from Weight";
  ot->idname = "PAINT_OT_vertex_color_from_weight";
  ot->description = "Convert active weight into gray scale vertex colors";

  /* api callback */
  ot->exec = vertex_paint_from_weight_exec;
  ot->poll = vertex_weight_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* TODO: invert, alpha */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smooth Vertex Colors Operator
 * \{ */

static void vertex_color_smooth_looptag(Mesh *me, const bool *mlooptag)
{
  const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const MPoly *mp;
  int(*scol)[4];
  bool has_shared = false;

  /* if no mloopcol: do not do */
  /* if mtexpoly: only the involved faces, otherwise all */

  if (me->mloopcol == NULL || me->totvert == 0 || me->totpoly == 0) {
    return;
  }

  scol = MEM_callocN(sizeof(int) * me->totvert * 5, "scol");

  int i;
  for (i = 0, mp = me->mpoly; i < me->totpoly; i++, mp++) {
    if ((use_face_sel == false) || (mp->flag & ME_FACE_SEL)) {
      const MLoop *ml = me->mloop + mp->loopstart;
      MLoopCol *lcol = me->mloopcol + mp->loopstart;
      for (int j = 0; j < mp->totloop; j++, ml++, lcol++) {
        scol[ml->v][0] += lcol->r;
        scol[ml->v][1] += lcol->g;
        scol[ml->v][2] += lcol->b;
        scol[ml->v][3] += 1;
        has_shared = 1;
      }
    }
  }

  if (has_shared) {
    for (i = 0; i < me->totvert; i++) {
      if (scol[i][3] != 0) {
        scol[i][0] = divide_round_i(scol[i][0], scol[i][3]);
        scol[i][1] = divide_round_i(scol[i][1], scol[i][3]);
        scol[i][2] = divide_round_i(scol[i][2], scol[i][3]);
      }
    }

    for (i = 0, mp = me->mpoly; i < me->totpoly; i++, mp++) {
      if ((use_face_sel == false) || (mp->flag & ME_FACE_SEL)) {
        const MLoop *ml = me->mloop + mp->loopstart;
        MLoopCol *lcol = me->mloopcol + mp->loopstart;
        for (int j = 0; j < mp->totloop; j++, ml++, lcol++) {
          if (mlooptag[mp->loopstart + j]) {
            lcol->r = scol[ml->v][0];
            lcol->g = scol[ml->v][1];
            lcol->b = scol[ml->v][2];
          }
        }
      }
    }
  }

  MEM_freeN(scol);
}

static bool vertex_color_smooth(Object *ob)
{
  Mesh *me;
  const MPoly *mp;
  int i, j;

  bool *mlooptag;

  if (((me = BKE_mesh_from_object(ob)) == NULL) || (ED_mesh_color_ensure(me, NULL) == false)) {
    return false;
  }

  const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

  mlooptag = MEM_callocN(sizeof(bool) * me->totloop, "VPaintData mlooptag");

  /* simply tag loops of selected faces */
  mp = me->mpoly;
  for (i = 0; i < me->totpoly; i++, mp++) {
    const MLoop *ml = me->mloop + mp->loopstart;

    if (use_face_sel && !(mp->flag & ME_FACE_SEL)) {
      continue;
    }

    j = 0;
    do {
      if (!(use_vert_sel && !(me->mvert[ml->v].flag & SELECT))) {
        mlooptag[mp->loopstart + j] = true;
      }
      ml++;
      j++;
    } while (j < mp->totloop);
  }

  /* remove stale me->mcol, will be added later */
  BKE_mesh_tessface_clear(me);

  vertex_color_smooth_looptag(me, mlooptag);

  MEM_freeN(mlooptag);

  tag_object_after_update(ob);

  return true;
}

static int vertex_color_smooth_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *obact = CTX_data_active_object(C);
  if (vertex_color_smooth(obact)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_smooth(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Smooth Vertex Colors";
  ot->idname = "PAINT_OT_vertex_color_smooth";
  ot->description = "Smooth colors across vertices";

  /* api callbacks */
  ot->exec = vertex_color_smooth_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Color Transformation Operators
 * \{ */

struct VPaintTx_BrightContrastData {
  /* pre-calculated */
  float gain;
  float offset;
};

static void vpaint_tx_brightness_contrast(const float col[3],
                                          const void *user_data,
                                          float r_col[3])
{
  const struct VPaintTx_BrightContrastData *data = user_data;

  for (int i = 0; i < 3; i++) {
    r_col[i] = data->gain * col[i] + data->offset;
  }
}

static int vertex_color_brightness_contrast_exec(bContext *C, wmOperator *op)
{
  Object *obact = CTX_data_active_object(C);

  float gain, offset;
  {
    float brightness = RNA_float_get(op->ptr, "brightness");
    float contrast = RNA_float_get(op->ptr, "contrast");
    brightness /= 100.0f;
    float delta = contrast / 200.0f;
    /*
     * The algorithm is by Werner D. Streidt
     * (http://visca.com/ffactory/archives/5-99/msg00021.html)
     * Extracted of OpenCV demhist.c
     */
    if (contrast > 0) {
      gain = 1.0f - delta * 2.0f;
      gain = 1.0f / max_ff(gain, FLT_EPSILON);
      offset = gain * (brightness - delta);
    }
    else {
      delta *= -1;
      gain = max_ff(1.0f - delta * 2.0f, 0.0f);
      offset = gain * brightness + delta;
    }
  }

  const struct VPaintTx_BrightContrastData user_data = {
      .gain = gain,
      .offset = offset,
  };

  if (ED_vpaint_color_transform(obact, vpaint_tx_brightness_contrast, &user_data)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_brightness_contrast(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Vertex Paint Brightness/Contrast";
  ot->idname = "PAINT_OT_vertex_color_brightness_contrast";
  ot->description = "Adjust vertex color brightness/contrast";

  /* api callbacks */
  ot->exec = vertex_color_brightness_contrast_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  const float min = -100, max = +100;
  prop = RNA_def_float(ot->srna, "brightness", 0.0f, min, max, "Brightness", "", min, max);
  prop = RNA_def_float(ot->srna, "contrast", 0.0f, min, max, "Contrast", "", min, max);
  RNA_def_property_ui_range(prop, min, max, 1, 1);
}

struct VPaintTx_HueSatData {
  float hue;
  float sat;
  float val;
};

static void vpaint_tx_hsv(const float col[3], const void *user_data, float r_col[3])
{
  const struct VPaintTx_HueSatData *data = user_data;
  float hsv[3];
  rgb_to_hsv_v(col, hsv);

  hsv[0] += (data->hue - 0.5f);
  if (hsv[0] > 1.0f) {
    hsv[0] -= 1.0f;
  }
  else if (hsv[0] < 0.0f) {
    hsv[0] += 1.0f;
  }
  hsv[1] *= data->sat;
  hsv[2] *= data->val;

  hsv_to_rgb_v(hsv, r_col);
}

static int vertex_color_hsv_exec(bContext *C, wmOperator *op)
{
  Object *obact = CTX_data_active_object(C);

  const struct VPaintTx_HueSatData user_data = {
      .hue = RNA_float_get(op->ptr, "h"),
      .sat = RNA_float_get(op->ptr, "s"),
      .val = RNA_float_get(op->ptr, "v"),
  };

  if (ED_vpaint_color_transform(obact, vpaint_tx_hsv, &user_data)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_hsv(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Hue Saturation Value";
  ot->idname = "PAINT_OT_vertex_color_hsv";
  ot->description = "Adjust vertex color HSV values";

  /* api callbacks */
  ot->exec = vertex_color_hsv_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  RNA_def_float(ot->srna, "h", 0.5f, 0.0f, 1.0f, "Hue", "", 0.0f, 1.0f);
  RNA_def_float(ot->srna, "s", 1.0f, 0.0f, 2.0f, "Saturation", "", 0.0f, 2.0f);
  RNA_def_float(ot->srna, "v", 1.0f, 0.0f, 2.0f, "Value", "", 0.0f, 2.0f);
}

static void vpaint_tx_invert(const float col[3], const void *UNUSED(user_data), float r_col[3])
{
  for (int i = 0; i < 3; i++) {
    r_col[i] = 1.0f - col[i];
  }
}

static int vertex_color_invert_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *obact = CTX_data_active_object(C);

  if (ED_vpaint_color_transform(obact, vpaint_tx_invert, NULL)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_invert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Invert";
  ot->idname = "PAINT_OT_vertex_color_invert";
  ot->description = "Invert RGB values";

  /* api callbacks */
  ot->exec = vertex_color_invert_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

struct VPaintTx_LevelsData {
  float gain;
  float offset;
};

static void vpaint_tx_levels(const float col[3], const void *user_data, float r_col[3])
{
  const struct VPaintTx_LevelsData *data = user_data;
  for (int i = 0; i < 3; i++) {
    r_col[i] = data->gain * (col[i] + data->offset);
  }
}

static int vertex_color_levels_exec(bContext *C, wmOperator *op)
{
  Object *obact = CTX_data_active_object(C);

  const struct VPaintTx_LevelsData user_data = {
      .gain = RNA_float_get(op->ptr, "gain"),
      .offset = RNA_float_get(op->ptr, "offset"),
  };

  if (ED_vpaint_color_transform(obact, vpaint_tx_levels, &user_data)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obact);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_levels(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Levels";
  ot->idname = "PAINT_OT_vertex_color_levels";
  ot->description = "Adjust levels of vertex colors";

  /* api callbacks */
  ot->exec = vertex_color_levels_exec;
  ot->poll = vertex_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  RNA_def_float(
      ot->srna, "offset", 0.0f, -1.0f, 1.0f, "Offset", "Value to add to colors", -1.0f, 1.0f);
  RNA_def_float(
      ot->srna, "gain", 1.0f, 0.0f, FLT_MAX, "Gain", "Value to multiply colors by", 0.0f, 10.0f);
}

/** \} */
