/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
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
 * \ingroup edrend
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_editmesh.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_workspace.h"
#include "BKE_world.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#ifdef WITH_FREESTYLE
#  include "BKE_freestyle.h"
#  include "FRS_freestyle.h"
#  include "RNA_enum_types.h"
#endif

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_curve.h"
#include "ED_mesh.h"
#include "ED_node.h"
#include "ED_object.h"
#include "ED_paint.h"
#include "ED_render.h"
#include "ED_scene.h"
#include "ED_screen.h"

#include "RNA_define.h"

#include "UI_interface.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "engines/eevee/eevee_lightcache.h"

#include "render_intern.h" /* own include */

static bool object_materials_supported_poll_ex(bContext *C, const Object *ob);

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static bool object_array_for_shading_edit_mode_enabled_filter(Object *ob, void *user_data)
{
  bContext *C = user_data;
  if (object_materials_supported_poll_ex(C, ob)) {
    if (BKE_object_is_in_editmode(ob) == true) {
      return true;
    }
  }
  return false;
}

static Object **object_array_for_shading_edit_mode_enabled(bContext *C, uint *r_objects_len)
{
  return ED_object_array_in_mode_or_selected(
      C, object_array_for_shading_edit_mode_enabled_filter, C, r_objects_len);
}

static bool object_array_for_shading_edit_mode_disabled_filter(Object *ob, void *user_data)
{
  bContext *C = user_data;
  if (object_materials_supported_poll_ex(C, ob)) {
    if (BKE_object_is_in_editmode(ob) == false) {
      return true;
    }
  }
  return false;
}

static Object **object_array_for_shading_edit_mode_disabled(bContext *C, uint *r_objects_len)
{
  return ED_object_array_in_mode_or_selected(
      C, object_array_for_shading_edit_mode_disabled_filter, C, r_objects_len);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Operator Poll Functions
 * \{ */

static bool object_materials_supported_poll_ex(bContext *C, const Object *ob)
{
  if (!ED_operator_object_active_local_editable_ex(C, ob)) {
    return false;
  }
  if (!OB_TYPE_SUPPORT_MATERIAL(ob->type)) {
    return false;
  }

  /* Material linked to object. */
  if (ob->matbits && ob->actcol && ob->matbits[ob->actcol - 1]) {
    return true;
  }

  /* Material linked to obdata. */
  const ID *data = ob->data;
  return (data && !ID_IS_LINKED(data) && !ID_IS_OVERRIDE_LIBRARY(data));
}

static bool object_materials_supported_poll(bContext *C)
{
  Object *ob = ED_object_context(C);
  return object_materials_supported_poll_ex(C, ob);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot Add Operator
 * \{ */

static int material_slot_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_context(C);

  if (!ob) {
    return OPERATOR_CANCELLED;
  }

  BKE_object_material_slot_add(bmain, ob);

  if (ob->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(scene, ob, NULL, NULL, NULL, NULL);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, NULL);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, ob);
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_PREVIEW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Material Slot";
  ot->idname = "OBJECT_OT_material_slot_add";
  ot->description = "Add a new material slot";

  /* api callbacks */
  ot->exec = material_slot_add_exec;
  ot->poll = object_materials_supported_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot Remove Operator
 * \{ */

static int material_slot_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);

  if (!ob) {
    return OPERATOR_CANCELLED;
  }

  /* Removing material slots in edit mode screws things up, see bug T21822.*/
  if (ob == CTX_data_edit_object(C)) {
    BKE_report(op->reports, RPT_ERROR, "Unable to remove material slot in edit mode");
    return OPERATOR_CANCELLED;
  }

  BKE_object_material_slot_remove(CTX_data_main(C), ob);

  if (ob->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(scene, ob, NULL, NULL, NULL, NULL);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, NULL);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, ob);
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_PREVIEW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Material Slot";
  ot->idname = "OBJECT_OT_material_slot_remove";
  ot->description = "Remove the selected material slot";

  /* api callbacks */
  ot->exec = material_slot_remove_exec;
  ot->poll = object_materials_supported_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot Assign Operator
 * \{ */

static int material_slot_assign_exec(bContext *C, wmOperator *UNUSED(op))
{
  View3D *v3d = CTX_wm_view3d(C);
  bool changed_multi = false;

  Object *obact = CTX_data_active_object(C);
  const Material *mat_active = obact ? BKE_object_material_get(obact, obact->actcol) : NULL;

  uint objects_len = 0;
  Object **objects = object_array_for_shading_edit_mode_enabled(C, &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    short mat_nr_active = -1;

    if (ob->totcol == 0) {
      continue;
    }
    if (obact && (mat_active == BKE_object_material_get(ob, obact->actcol))) {
      /* Avoid searching since there may be multiple slots with the same material.
       * For the active object or duplicates: match the material slot index first. */
      mat_nr_active = obact->actcol - 1;
    }
    else {
      /* Find the first matching material.
       * Note: there may be multiple but that's not a common use case. */
      for (int i = 0; i < ob->totcol; i++) {
        const Material *mat = BKE_object_material_get(ob, i + 1);
        if (mat_active == mat) {
          mat_nr_active = i;
          break;
        }
      }
      if (mat_nr_active == -1) {
        continue;
      }
    }

    bool changed = false;
    if (ob->type == OB_MESH) {
      BMEditMesh *em = BKE_editmesh_from_object(ob);
      BMFace *efa;
      BMIter iter;

      if (em) {
        BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
          if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
            changed = true;
            efa->mat_nr = mat_nr_active;
          }
        }
      }
    }
    else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
      Nurb *nu;
      ListBase *nurbs = BKE_curve_editNurbs_get((Curve *)ob->data);

      if (nurbs) {
        for (nu = nurbs->first; nu; nu = nu->next) {
          if (ED_curve_nurb_select_check(v3d, nu)) {
            changed = true;
            nu->mat_nr = mat_nr_active;
          }
        }
      }
    }
    else if (ob->type == OB_FONT) {
      EditFont *ef = ((Curve *)ob->data)->editfont;
      int i, selstart, selend;

      if (ef && BKE_vfont_select_get(ob, &selstart, &selend)) {
        for (i = selstart; i <= selend; i++) {
          changed = true;
          ef->textbufinfo[i].mat_nr = mat_nr_active + 1;
        }
      }
    }

    if (changed) {
      changed_multi = true;
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
    }
  }
  MEM_freeN(objects);

  return (changed_multi) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void OBJECT_OT_material_slot_assign(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Assign Material Slot";
  ot->idname = "OBJECT_OT_material_slot_assign";
  ot->description = "Assign active material slot to selection";

  /* api callbacks */
  ot->exec = material_slot_assign_exec;
  ot->poll = object_materials_supported_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot (De)Select Operator
 * \{ */

static int material_slot_de_select(bContext *C, bool select)
{
  bool changed_multi = false;
  Object *obact = CTX_data_active_object(C);
  const Material *mat_active = obact ? BKE_object_material_get(obact, obact->actcol) : NULL;

  uint objects_len = 0;
  Object **objects = object_array_for_shading_edit_mode_enabled(C, &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    short mat_nr_active = -1;

    if (ob->totcol == 0) {
      continue;
    }
    if (obact && (mat_active == BKE_object_material_get(ob, obact->actcol))) {
      /* Avoid searching since there may be multiple slots with the same material.
       * For the active object or duplicates: match the material slot index first. */
      mat_nr_active = obact->actcol - 1;
    }
    else {
      /* Find the first matching material.
       * Note: there may be multiple but that's not a common use case. */
      for (int i = 0; i < ob->totcol; i++) {
        const Material *mat = BKE_object_material_get(ob, i + 1);
        if (mat_active == mat) {
          mat_nr_active = i;
          break;
        }
      }
      if (mat_nr_active == -1) {
        continue;
      }
    }

    bool changed = false;

    if (ob->type == OB_MESH) {
      BMEditMesh *em = BKE_editmesh_from_object(ob);

      if (em) {
        changed = EDBM_deselect_by_material(em, mat_nr_active, select);
      }
    }
    else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
      ListBase *nurbs = BKE_curve_editNurbs_get((Curve *)ob->data);
      Nurb *nu;
      BPoint *bp;
      BezTriple *bezt;
      int a;

      if (nurbs) {
        for (nu = nurbs->first; nu; nu = nu->next) {
          if (nu->mat_nr == mat_nr_active) {
            if (nu->bezt) {
              a = nu->pntsu;
              bezt = nu->bezt;
              while (a--) {
                if (bezt->hide == 0) {
                  changed = true;
                  if (select) {
                    bezt->f1 |= SELECT;
                    bezt->f2 |= SELECT;
                    bezt->f3 |= SELECT;
                  }
                  else {
                    bezt->f1 &= ~SELECT;
                    bezt->f2 &= ~SELECT;
                    bezt->f3 &= ~SELECT;
                  }
                }
                bezt++;
              }
            }
            else if (nu->bp) {
              a = nu->pntsu * nu->pntsv;
              bp = nu->bp;
              while (a--) {
                if (bp->hide == 0) {
                  changed = true;
                  if (select) {
                    bp->f1 |= SELECT;
                  }
                  else {
                    bp->f1 &= ~SELECT;
                  }
                }
                bp++;
              }
            }
          }
        }
      }
    }

    if (changed) {
      changed_multi = true;
      DEG_id_tag_update(ob->data, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
    }
  }

  MEM_freeN(objects);

  return (changed_multi) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int material_slot_select_exec(bContext *C, wmOperator *UNUSED(op))
{
  return material_slot_de_select(C, true);
}

void OBJECT_OT_material_slot_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Material Slot";
  ot->idname = "OBJECT_OT_material_slot_select";
  ot->description = "Select by active material slot";

  /* api callbacks */
  ot->exec = material_slot_select_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

static int material_slot_deselect_exec(bContext *C, wmOperator *UNUSED(op))
{
  return material_slot_de_select(C, false);
}

void OBJECT_OT_material_slot_deselect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Deselect Material Slot";
  ot->idname = "OBJECT_OT_material_slot_deselect";
  ot->description = "Deselect by active material slot";

  /* api callbacks */
  ot->exec = material_slot_deselect_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot Copy Operator
 * \{ */

static int material_slot_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_context(C);
  Material ***matar_obdata;

  if (!ob || !(matar_obdata = BKE_object_material_array_p(ob))) {
    return OPERATOR_CANCELLED;
  }

  BLI_assert(ob->totcol == *BKE_object_material_len_p(ob));

  Material ***matar_object = &ob->mat;

  Material **matar = MEM_callocN(sizeof(*matar) * (size_t)ob->totcol, __func__);
  for (int i = ob->totcol; i--;) {
    matar[i] = ob->matbits[i] ? (*matar_object)[i] : (*matar_obdata)[i];
  }

  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob != ob_iter && BKE_object_material_array_p(ob_iter)) {
      /* If we are using the same obdata, we only assign slots in ob_iter that are using object
       * materials, and not obdata ones. */
      const bool is_same_obdata = ob->data == ob_iter->data;
      BKE_object_material_array_assign(bmain, ob_iter, &matar, ob->totcol, is_same_obdata);

      if (ob_iter->totcol == ob->totcol) {
        ob_iter->actcol = ob->actcol;
        DEG_id_tag_update(&ob_iter->id, ID_RECALC_GEOMETRY);
        WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob_iter);
      }
    }
  }
  CTX_DATA_END;

  MEM_freeN(matar);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Material to Selected";
  ot->idname = "OBJECT_OT_material_slot_copy";
  ot->description = "Copy material to selected objects";

  /* api callbacks */
  ot->exec = material_slot_copy_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot Move Operator
 * \{ */

static int material_slot_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);

  uint *slot_remap;
  int index_pair[2];

  int dir = RNA_enum_get(op->ptr, "direction");

  if (!ob || ob->totcol < 2) {
    return OPERATOR_CANCELLED;
  }

  /* up */
  if (dir == 1 && ob->actcol > 1) {
    index_pair[0] = ob->actcol - 2;
    index_pair[1] = ob->actcol - 1;
    ob->actcol--;
  }
  /* down */
  else if (dir == -1 && ob->actcol < ob->totcol) {
    index_pair[0] = ob->actcol - 1;
    index_pair[1] = ob->actcol - 0;
    ob->actcol++;
  }
  else {
    return OPERATOR_CANCELLED;
  }

  slot_remap = MEM_mallocN(sizeof(uint) * ob->totcol, __func__);

  range_vn_u(slot_remap, ob->totcol, 0);

  slot_remap[index_pair[0]] = index_pair[1];
  slot_remap[index_pair[1]] = index_pair[0];

  BKE_object_material_remap(ob, slot_remap);

  MEM_freeN(slot_remap);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DATA, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_move(wmOperatorType *ot)
{
  static const EnumPropertyItem material_slot_move[] = {
      {1, "UP", 0, "Up", ""},
      {-1, "DOWN", 0, "Down", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Move Material";
  ot->idname = "OBJECT_OT_material_slot_move";
  ot->description = "Move the active material up/down in the list";

  /* api callbacks */
  ot->exec = material_slot_move_exec;
  ot->poll = object_materials_supported_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "direction",
               material_slot_move,
               0,
               "Direction",
               "Direction to move the active material towards");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot Remove Unused Operator
 * \{ */

static int material_slot_remove_unused_exec(bContext *C, wmOperator *op)
{
  /* Removing material slots in edit mode screws things up, see bug T21822. */
  Object *ob_active = CTX_data_active_object(C);
  if (ob_active && BKE_object_is_in_editmode(ob_active)) {
    BKE_report(op->reports, RPT_ERROR, "Unable to remove material slot in edit mode");
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  int removed = 0;

  uint objects_len = 0;
  Object **objects = object_array_for_shading_edit_mode_disabled(C, &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    int actcol = ob->actcol;
    for (int slot = 1; slot <= ob->totcol; slot++) {
      while (slot <= ob->totcol && !BKE_object_material_slot_used(ob->data, slot)) {
        ob->actcol = slot;
        BKE_object_material_slot_remove(bmain, ob);

        if (actcol >= slot) {
          actcol--;
        }

        removed++;
      }
    }
    ob->actcol = actcol;

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
  MEM_freeN(objects);

  if (!removed) {
    return OPERATOR_CANCELLED;
  }

  BKE_reportf(op->reports, RPT_INFO, "Removed %d slots", removed);

  if (ob_active->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(scene, ob_active, NULL, NULL, NULL, NULL);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, NULL);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob_active);
  WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, ob_active);
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_PREVIEW, ob_active);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_remove_unused(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Unused Slots";
  ot->idname = "OBJECT_OT_material_slot_remove_unused";
  ot->description = "Remove unused material slots";

  /* api callbacks */
  ot->exec = material_slot_remove_unused_exec;
  ot->poll = object_materials_supported_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Material Operator
 * \{ */

static int new_material_exec(bContext *C, wmOperator *UNUSED(op))
{
  Material *ma = CTX_data_pointer_get_type(C, "material", &RNA_Material).data;
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr, idptr;
  PropertyRNA *prop;

  /* hook into UI */
  UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

  Object *ob = (prop && RNA_struct_is_a(ptr.type, &RNA_Object)) ? ptr.data : NULL;

  /* add or copy material */
  if (ma) {
    Material *new_ma = (Material *)BKE_id_copy_ex(
        bmain, &ma->id, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS);
    ma = new_ma;
  }
  else {
    const char *name = DATA_("Material");
    if (!(ob != NULL && ob->type == OB_GPENCIL)) {
      ma = BKE_material_add(bmain, name);
    }
    else {
      ma = BKE_gpencil_material_add(bmain, name);
    }
    ED_node_shader_default(C, &ma->id);
    ma->use_nodes = true;
  }

  if (prop) {
    if (ob != NULL) {
      /* Add slot follows user-preferences for creating new slots,
       * RNA pointer assignment doesn't, see: T60014. */
      if (BKE_object_material_get_p(ob, ob->actcol) == NULL) {
        BKE_object_material_slot_add(bmain, ob);
      }
    }

    /* when creating new ID blocks, use is already 1, but RNA
     * pointer use also increases user, so this compensates it */
    id_us_min(&ma->id);

    RNA_id_pointer_create(&ma->id, &idptr);
    RNA_property_pointer_set(&ptr, prop, idptr, NULL);
    RNA_property_update(C, &ptr, prop);
  }

  WM_event_add_notifier(C, NC_MATERIAL | NA_ADDED, ma);

  return OPERATOR_FINISHED;
}

void MATERIAL_OT_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Material";
  ot->idname = "MATERIAL_OT_new";
  ot->description = "Add a new material";

  /* api callbacks */
  ot->exec = new_material_exec;
  ot->poll = object_materials_supported_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Texture Operator
 * \{ */

static int new_texture_exec(bContext *C, wmOperator *UNUSED(op))
{
  Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr, idptr;
  PropertyRNA *prop;

  /* add or copy texture */
  if (tex) {
    tex = (Tex *)BKE_id_copy(bmain, &tex->id);
  }
  else {
    tex = BKE_texture_add(bmain, DATA_("Texture"));
  }

  /* hook into UI */
  UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

  if (prop) {
    /* when creating new ID blocks, use is already 1, but RNA
     * pointer use also increases user, so this compensates it */
    id_us_min(&tex->id);

    RNA_id_pointer_create(&tex->id, &idptr);
    RNA_property_pointer_set(&ptr, prop, idptr, NULL);
    RNA_property_update(C, &ptr, prop);
  }

  WM_event_add_notifier(C, NC_TEXTURE | NA_ADDED, tex);

  return OPERATOR_FINISHED;
}

void TEXTURE_OT_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Texture";
  ot->idname = "TEXTURE_OT_new";
  ot->description = "Add a new texture";

  /* api callbacks */
  ot->exec = new_texture_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name new world operator
 * \{ */

static int new_world_exec(bContext *C, wmOperator *UNUSED(op))
{
  World *wo = CTX_data_pointer_get_type(C, "world", &RNA_World).data;
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr, idptr;
  PropertyRNA *prop;

  /* add or copy world */
  if (wo) {
    World *new_wo = (World *)BKE_id_copy_ex(
        bmain, &wo->id, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS);
    wo = new_wo;
  }
  else {
    wo = BKE_world_add(bmain, DATA_("World"));
    ED_node_shader_default(C, &wo->id);
    wo->use_nodes = true;
  }

  /* hook into UI */
  UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

  if (prop) {
    /* when creating new ID blocks, use is already 1, but RNA
     * pointer use also increases user, so this compensates it */
    id_us_min(&wo->id);

    RNA_id_pointer_create(&wo->id, &idptr);
    RNA_property_pointer_set(&ptr, prop, idptr, NULL);
    RNA_property_update(C, &ptr, prop);
  }

  WM_event_add_notifier(C, NC_WORLD | NA_ADDED, wo);

  return OPERATOR_FINISHED;
}

void WORLD_OT_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New World";
  ot->idname = "WORLD_OT_new";
  ot->description = "Create a new world Data-Block";

  /* api callbacks */
  ot->exec = new_world_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Layer Add Operator
 * \{ */

static int view_layer_add_exec(bContext *C, wmOperator *op)
{
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer_current = WM_window_get_active_view_layer(win);
  ViewLayer *view_layer_new = BKE_view_layer_add(
      scene, view_layer_current->name, view_layer_current, RNA_enum_get(op->ptr, "type"));

  if (win) {
    WM_window_set_active_view_layer(win, view_layer_new);
  }

  DEG_id_tag_update(&scene->id, 0);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_view_layer_add(wmOperatorType *ot)
{
  static EnumPropertyItem type_items[] = {
      {VIEWLAYER_ADD_NEW, "NEW", 0, "New", "Add a new view layer"},
      {VIEWLAYER_ADD_COPY, "COPY", 0, "Copy Settings", "Copy settings of current view layer"},
      {VIEWLAYER_ADD_EMPTY,
       "EMPTY",
       0,
       "Blank",
       "Add a new view layer with all collections disabled"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Add View Layer";
  ot->idname = "SCENE_OT_view_layer_add";
  ot->description = "Add a view layer";

  /* api callbacks */
  ot->exec = view_layer_add_exec;
  ot->invoke = WM_menu_invoke;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Layer Remove Operator
 * \{ */

static bool view_layer_remove_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  return (scene->view_layers.first != scene->view_layers.last);
}

static int view_layer_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (!ED_scene_view_layer_delete(bmain, scene, view_layer, NULL)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_view_layer_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove View Layer";
  ot->idname = "SCENE_OT_view_layer_remove";
  ot->description = "Remove the selected view layer";

  /* api callbacks */
  ot->exec = view_layer_remove_exec;
  ot->poll = view_layer_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Add AOV Operator
 * \{ */

static int view_layer_add_aov_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  BKE_view_layer_add_aov(view_layer);

  RenderEngineType *engine_type = RE_engines_find(scene->r.engine);
  if (engine_type->update_render_passes) {
    RenderEngine *engine = RE_engine_create(engine_type);
    if (engine) {
      BKE_view_layer_verify_aov(engine, scene, view_layer);
    }
    RE_engine_free(engine);
    engine = NULL;
  }

  DEG_id_tag_update(&scene->id, 0);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_view_layer_add_aov(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add AOV";
  ot->idname = "SCENE_OT_view_layer_add_aov";
  ot->description = "Add a Shader AOV";

  /* api callbacks */
  ot->exec = view_layer_add_aov_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Remove AOV Operator
 * \{ */

static int view_layer_remove_aov_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (view_layer->active_aov == NULL) {
    return OPERATOR_FINISHED;
  }

  BKE_view_layer_remove_aov(view_layer, view_layer->active_aov);

  RenderEngineType *engine_type = RE_engines_find(scene->r.engine);
  if (engine_type->update_render_passes) {
    RenderEngine *engine = RE_engine_create(engine_type);
    if (engine) {
      BKE_view_layer_verify_aov(engine, scene, view_layer);
    }
    RE_engine_free(engine);
    engine = NULL;
  }

  DEG_id_tag_update(&scene->id, 0);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_view_layer_remove_aov(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove AOV";
  ot->idname = "SCENE_OT_view_layer_remove_aov";
  ot->description = "Remove Active AOV";

  /* api callbacks */
  ot->exec = view_layer_remove_aov_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Cache Bake Operator
 * \{ */

enum {
  LIGHTCACHE_SUBSET_ALL = 0,
  LIGHTCACHE_SUBSET_DIRTY,
  LIGHTCACHE_SUBSET_CUBE,
};

static void light_cache_bake_tag_cache(Scene *scene, wmOperator *op)
{
  if (scene->eevee.light_cache_data != NULL) {
    int subset = RNA_enum_get(op->ptr, "subset");
    switch (subset) {
      case LIGHTCACHE_SUBSET_ALL:
        scene->eevee.light_cache_data->flag |= LIGHTCACHE_UPDATE_GRID | LIGHTCACHE_UPDATE_CUBE;
        break;
      case LIGHTCACHE_SUBSET_CUBE:
        scene->eevee.light_cache_data->flag |= LIGHTCACHE_UPDATE_CUBE;
        break;
      case LIGHTCACHE_SUBSET_DIRTY:
        /* Leave tag untouched. */
        break;
    }
  }
}

/* catch esc */
static int light_cache_bake_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = (Scene *)op->customdata;

  /* no running blender, remove handler and pass through */
  if (0 == WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_RENDER)) {
    LightCache *lcache = scene->eevee.light_cache_data;
    if (lcache && (lcache->flag & LIGHTCACHE_INVALID)) {
      BKE_report(op->reports, RPT_ERROR, "Lightcache cannot allocate resources");
      return OPERATOR_CANCELLED;
    }
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* running render */
  switch (event->type) {
    case EVT_ESCKEY:
      return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_PASS_THROUGH;
}

static void light_cache_bake_cancel(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  Scene *scene = (Scene *)op->customdata;

  /* kill on cancel, because job is using op->reports */
  WM_jobs_kill_type(wm, scene, WM_JOB_TYPE_RENDER);
}

/* executes blocking render */
static int light_cache_bake_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  G.is_break = false;

  /* TODO abort if selected engine is not eevee. */
  void *rj = EEVEE_lightbake_job_data_alloc(bmain, view_layer, scene, false, scene->r.cfra);

  light_cache_bake_tag_cache(scene, op);

  short stop = 0, do_update;
  float progress; /* Not actually used. */
  EEVEE_lightbake_job(rj, &stop, &do_update, &progress);
  EEVEE_lightbake_job_data_free(rj);

  /* No redraw needed, we leave state as we entered it. */
  ED_update_for_newframe(bmain, CTX_data_depsgraph_pointer(C));

  WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);

  return OPERATOR_FINISHED;
}

static int light_cache_bake_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  int delay = RNA_int_get(op->ptr, "delay");

  wmJob *wm_job = EEVEE_lightbake_job_create(
      wm, win, bmain, view_layer, scene, delay, scene->r.cfra);

  if (!wm_job) {
    return OPERATOR_CANCELLED;
  }

  /* add modal handler for ESC */
  WM_event_add_modal_handler(C, op);

  light_cache_bake_tag_cache(scene, op);

  /* store actual owner of job, so modal operator could check for it,
   * the reason of this is that active scene could change when rendering
   * several layers from compositor T31800. */
  op->customdata = scene;

  WM_jobs_start(wm, wm_job);

  WM_cursor_wait(false);

  return OPERATOR_RUNNING_MODAL;
}

void SCENE_OT_light_cache_bake(wmOperatorType *ot)
{
  static const EnumPropertyItem light_cache_subset_items[] = {
      {LIGHTCACHE_SUBSET_ALL,
       "ALL",
       0,
       "All Light Probes",
       "Bake both irradiance grids and reflection cubemaps"},
      {LIGHTCACHE_SUBSET_DIRTY,
       "DIRTY",
       0,
       "Dirty Only",
       "Only bake light probes that are marked as dirty"},
      {LIGHTCACHE_SUBSET_CUBE,
       "CUBEMAPS",
       0,
       "Cubemaps Only",
       "Try to only bake reflection cubemaps if irradiance grids are up to date"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Bake Light Cache";
  ot->idname = "SCENE_OT_light_cache_bake";
  ot->description = "Bake the active view layer lighting";

  /* api callbacks */
  ot->invoke = light_cache_bake_invoke;
  ot->modal = light_cache_bake_modal;
  ot->cancel = light_cache_bake_cancel;
  ot->exec = light_cache_bake_exec;

  ot->prop = RNA_def_int(ot->srna,
                         "delay",
                         0,
                         0,
                         2000,
                         "Delay",
                         "Delay in millisecond before baking starts",
                         0,
                         2000);
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);

  ot->prop = RNA_def_enum(
      ot->srna, "subset", light_cache_subset_items, 0, "Subset", "Subset of probes to update");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Cache Free Operator
 * \{ */

static bool light_cache_free_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);

  return scene->eevee.light_cache_data;
}

static int light_cache_free_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);

  /* kill potential bake job first (see T57011) */
  wmWindowManager *wm = CTX_wm_manager(C);
  WM_jobs_kill_type(wm, scene, WM_JOB_TYPE_LIGHT_BAKE);

  if (!scene->eevee.light_cache_data) {
    return OPERATOR_CANCELLED;
  }

  EEVEE_lightcache_free(scene->eevee.light_cache_data);
  scene->eevee.light_cache_data = NULL;

  EEVEE_lightcache_info_update(&scene->eevee);

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_light_cache_free(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Light Cache";
  ot->idname = "SCENE_OT_light_cache_free";
  ot->description = "Delete cached indirect lighting";

  /* api callbacks */
  ot->exec = light_cache_free_exec;
  ot->poll = light_cache_free_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render View Remove Operator
 * \{ */

static bool render_view_remove_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);

  /* don't allow user to remove "left" and "right" views */
  return scene->r.actview > 1;
}

static int render_view_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);

  BKE_scene_add_render_view(scene, NULL);
  scene->r.actview = BLI_listbase_count(&scene->r.views) - 1;

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_render_view_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Render View";
  ot->idname = "SCENE_OT_render_view_add";
  ot->description = "Add a render view";

  /* api callbacks */
  ot->exec = render_view_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render View Add Operator
 * \{ */

static int render_view_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  SceneRenderView *rv = BLI_findlink(&scene->r.views, scene->r.actview);

  if (!BKE_scene_remove_render_view(scene, rv)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_render_view_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Render View";
  ot->idname = "SCENE_OT_render_view_remove";
  ot->description = "Remove the selected render view";

  /* api callbacks */
  ot->exec = render_view_remove_exec;
  ot->poll = render_view_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

#ifdef WITH_FREESTYLE

/* -------------------------------------------------------------------- */
/** \name Free Style Module Add Operator
 * \{ */

static bool freestyle_linestyle_check_report(FreestyleLineSet *lineset, ReportList *reports)
{
  if (!lineset) {
    BKE_report(reports,
               RPT_ERROR,
               "No active lineset and associated line style to manipulate the modifier");
    return false;
  }
  if (!lineset->linestyle) {
    BKE_report(reports,
               RPT_ERROR,
               "The active lineset does not have a line style (indicating data corruption)");
    return false;
  }

  return true;
}

static bool freestyle_active_module_poll(bContext *C)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "freestyle_module", &RNA_FreestyleModuleSettings);
  FreestyleModuleConfig *module = ptr.data;

  return module != NULL;
}

static int freestyle_module_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  BKE_freestyle_module_add(&view_layer->freestyle_config);

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_module_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Freestyle Module";
  ot->idname = "SCENE_OT_freestyle_module_add";
  ot->description = "Add a style module into the list of modules";

  /* api callbacks */
  ot->exec = freestyle_module_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Module Remove Operator
 * \{ */

static int freestyle_module_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "freestyle_module", &RNA_FreestyleModuleSettings);
  FreestyleModuleConfig *module = ptr.data;

  BKE_freestyle_module_delete(&view_layer->freestyle_config, module);

  DEG_id_tag_update(&scene->id, 0);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_module_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Freestyle Module";
  ot->idname = "SCENE_OT_freestyle_module_remove";
  ot->description = "Remove the style module from the stack";

  /* api callbacks */
  ot->poll = freestyle_active_module_poll;
  ot->exec = freestyle_module_remove_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

static int freestyle_module_move_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "freestyle_module", &RNA_FreestyleModuleSettings);
  FreestyleModuleConfig *module = ptr.data;
  int dir = RNA_enum_get(op->ptr, "direction");

  if (BKE_freestyle_module_move(&view_layer->freestyle_config, module, dir)) {
    DEG_id_tag_update(&scene->id, 0);
    WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);
  }

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Module Move Operator
 * \{ */

void SCENE_OT_freestyle_module_move(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Move Freestyle Module";
  ot->idname = "SCENE_OT_freestyle_module_move";
  ot->description = "Change the position of the style module within in the list of style modules";

  /* api callbacks */
  ot->poll = freestyle_active_module_poll;
  ot->exec = freestyle_module_move_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* props */
  RNA_def_enum(ot->srna,
               "direction",
               direction_items,
               0,
               "Direction",
               "Direction to move the chosen style module towards");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Line Set Add Operator
 * \{ */

static int freestyle_lineset_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  BKE_freestyle_lineset_add(bmain, &view_layer->freestyle_config, NULL);

  DEG_id_tag_update(&scene->id, 0);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Line Set";
  ot->idname = "SCENE_OT_freestyle_lineset_add";
  ot->description = "Add a line set into the list of line sets";

  /* api callbacks */
  ot->exec = freestyle_lineset_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Line Set Copy Operator
 * \{ */

static bool freestyle_active_lineset_poll(bContext *C)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (!view_layer) {
    return false;
  }

  return BKE_freestyle_lineset_get_active(&view_layer->freestyle_config) != NULL;
}

static int freestyle_lineset_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);

  FRS_copy_active_lineset(&view_layer->freestyle_config);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Line Set";
  ot->idname = "SCENE_OT_freestyle_lineset_copy";
  ot->description = "Copy the active line set to a buffer";

  /* api callbacks */
  ot->exec = freestyle_lineset_copy_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Line Set Paste Operator
 * \{ */

static int freestyle_lineset_paste_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  FRS_paste_active_lineset(&view_layer->freestyle_config);

  DEG_id_tag_update(&scene->id, 0);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Line Set";
  ot->idname = "SCENE_OT_freestyle_lineset_paste";
  ot->description = "Paste the buffer content to the active line set";

  /* api callbacks */
  ot->exec = freestyle_lineset_paste_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Line Set Remove Operator
 * \{ */

static int freestyle_lineset_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  FRS_delete_active_lineset(&view_layer->freestyle_config);

  DEG_id_tag_update(&scene->id, 0);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Line Set";
  ot->idname = "SCENE_OT_freestyle_lineset_remove";
  ot->description = "Remove the active line set from the list of line sets";

  /* api callbacks */
  ot->exec = freestyle_lineset_remove_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Line Set Move Operator
 * \{ */

static int freestyle_lineset_move_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int dir = RNA_enum_get(op->ptr, "direction");

  if (FRS_move_active_lineset(&view_layer->freestyle_config, dir)) {
    DEG_id_tag_update(&scene->id, 0);
    WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);
  }

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_move(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Move Line Set";
  ot->idname = "SCENE_OT_freestyle_lineset_move";
  ot->description = "Change the position of the active line set within the list of line sets";

  /* api callbacks */
  ot->exec = freestyle_lineset_move_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* props */
  RNA_def_enum(ot->srna,
               "direction",
               direction_items,
               0,
               "Direction",
               "Direction to move the active line set towards");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Line Set New Operator
 * \{ */

static int freestyle_linestyle_new_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);

  if (!lineset) {
    BKE_report(op->reports, RPT_ERROR, "No active lineset to add a new line style to");
    return OPERATOR_CANCELLED;
  }
  if (lineset->linestyle) {
    id_us_min(&lineset->linestyle->id);
    lineset->linestyle = (FreestyleLineStyle *)BKE_id_copy(bmain, &lineset->linestyle->id);
  }
  else {
    lineset->linestyle = BKE_linestyle_new(bmain, "LineStyle");
  }
  DEG_id_tag_update(&lineset->linestyle->id, 0);
  WM_event_add_notifier(C, NC_LINESTYLE, lineset->linestyle);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_linestyle_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Line Style";
  ot->idname = "SCENE_OT_freestyle_linestyle_new";
  ot->description = "Create a new line style, reusable by multiple line sets";

  /* api callbacks */
  ot->exec = freestyle_linestyle_new_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Modifier Add "Color" Operator
 * \{ */

static int freestyle_color_modifier_add_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  int type = RNA_enum_get(op->ptr, "type");

  if (!freestyle_linestyle_check_report(lineset, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_linestyle_color_modifier_add(lineset->linestyle, NULL, type) == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Unknown line color modifier type");
    return OPERATOR_CANCELLED;
  }
  DEG_id_tag_update(&lineset->linestyle->id, 0);
  WM_event_add_notifier(C, NC_LINESTYLE, lineset->linestyle);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_color_modifier_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Line Color Modifier";
  ot->idname = "SCENE_OT_freestyle_color_modifier_add";
  ot->description =
      "Add a line color modifier to the line style associated with the active lineset";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = freestyle_color_modifier_add_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_linestyle_color_modifier_type_items, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Modifier Add "Alpha" Operator
 * \{ */

static int freestyle_alpha_modifier_add_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  int type = RNA_enum_get(op->ptr, "type");

  if (!freestyle_linestyle_check_report(lineset, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_linestyle_alpha_modifier_add(lineset->linestyle, NULL, type) == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Unknown alpha transparency modifier type");
    return OPERATOR_CANCELLED;
  }
  DEG_id_tag_update(&lineset->linestyle->id, 0);
  WM_event_add_notifier(C, NC_LINESTYLE, lineset->linestyle);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_alpha_modifier_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Alpha Transparency Modifier";
  ot->idname = "SCENE_OT_freestyle_alpha_modifier_add";
  ot->description =
      "Add an alpha transparency modifier to the line style associated with the active lineset";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = freestyle_alpha_modifier_add_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_linestyle_alpha_modifier_type_items, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Modifier Add "Thickness" Operator
 * \{ */

static int freestyle_thickness_modifier_add_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  int type = RNA_enum_get(op->ptr, "type");

  if (!freestyle_linestyle_check_report(lineset, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_linestyle_thickness_modifier_add(lineset->linestyle, NULL, type) == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Unknown line thickness modifier type");
    return OPERATOR_CANCELLED;
  }
  DEG_id_tag_update(&lineset->linestyle->id, 0);
  WM_event_add_notifier(C, NC_LINESTYLE, lineset->linestyle);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_thickness_modifier_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Line Thickness Modifier";
  ot->idname = "SCENE_OT_freestyle_thickness_modifier_add";
  ot->description =
      "Add a line thickness modifier to the line style associated with the active lineset";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = freestyle_thickness_modifier_add_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_linestyle_thickness_modifier_type_items, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Modifier Add "Geometry" Operator
 * \{ */

static int freestyle_geometry_modifier_add_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  int type = RNA_enum_get(op->ptr, "type");

  if (!freestyle_linestyle_check_report(lineset, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_linestyle_geometry_modifier_add(lineset->linestyle, NULL, type) == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Unknown stroke geometry modifier type");
    return OPERATOR_CANCELLED;
  }
  DEG_id_tag_update(&lineset->linestyle->id, 0);
  WM_event_add_notifier(C, NC_LINESTYLE, lineset->linestyle);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_geometry_modifier_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Stroke Geometry Modifier";
  ot->idname = "SCENE_OT_freestyle_geometry_modifier_add";
  ot->description =
      "Add a stroke geometry modifier to the line style associated with the active lineset";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = freestyle_geometry_modifier_add_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_linestyle_geometry_modifier_type_items, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Modifier Remove Operator
 * \{ */

static int freestyle_get_modifier_type(PointerRNA *ptr)
{
  if (RNA_struct_is_a(ptr->type, &RNA_LineStyleColorModifier)) {
    return LS_MODIFIER_TYPE_COLOR;
  }
  if (RNA_struct_is_a(ptr->type, &RNA_LineStyleAlphaModifier)) {
    return LS_MODIFIER_TYPE_ALPHA;
  }
  if (RNA_struct_is_a(ptr->type, &RNA_LineStyleThicknessModifier)) {
    return LS_MODIFIER_TYPE_THICKNESS;
  }
  if (RNA_struct_is_a(ptr->type, &RNA_LineStyleGeometryModifier)) {
    return LS_MODIFIER_TYPE_GEOMETRY;
  }
  return -1;
}

static int freestyle_modifier_remove_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_LineStyleModifier);
  LineStyleModifier *modifier = ptr.data;

  if (!freestyle_linestyle_check_report(lineset, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  switch (freestyle_get_modifier_type(&ptr)) {
    case LS_MODIFIER_TYPE_COLOR:
      BKE_linestyle_color_modifier_remove(lineset->linestyle, modifier);
      break;
    case LS_MODIFIER_TYPE_ALPHA:
      BKE_linestyle_alpha_modifier_remove(lineset->linestyle, modifier);
      break;
    case LS_MODIFIER_TYPE_THICKNESS:
      BKE_linestyle_thickness_modifier_remove(lineset->linestyle, modifier);
      break;
    case LS_MODIFIER_TYPE_GEOMETRY:
      BKE_linestyle_geometry_modifier_remove(lineset->linestyle, modifier);
      break;
    default:
      BKE_report(
          op->reports, RPT_ERROR, "The object the data pointer refers to is not a valid modifier");
      return OPERATOR_CANCELLED;
  }
  DEG_id_tag_update(&lineset->linestyle->id, 0);
  WM_event_add_notifier(C, NC_LINESTYLE, lineset->linestyle);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_modifier_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Modifier";
  ot->idname = "SCENE_OT_freestyle_modifier_remove";
  ot->description = "Remove the modifier from the list of modifiers";

  /* api callbacks */
  ot->exec = freestyle_modifier_remove_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Modifier Copy Operator
 * \{ */

static int freestyle_modifier_copy_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_LineStyleModifier);
  LineStyleModifier *modifier = ptr.data;

  if (!freestyle_linestyle_check_report(lineset, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  switch (freestyle_get_modifier_type(&ptr)) {
    case LS_MODIFIER_TYPE_COLOR:
      BKE_linestyle_color_modifier_copy(lineset->linestyle, modifier, 0);
      break;
    case LS_MODIFIER_TYPE_ALPHA:
      BKE_linestyle_alpha_modifier_copy(lineset->linestyle, modifier, 0);
      break;
    case LS_MODIFIER_TYPE_THICKNESS:
      BKE_linestyle_thickness_modifier_copy(lineset->linestyle, modifier, 0);
      break;
    case LS_MODIFIER_TYPE_GEOMETRY:
      BKE_linestyle_geometry_modifier_copy(lineset->linestyle, modifier, 0);
      break;
    default:
      BKE_report(
          op->reports, RPT_ERROR, "The object the data pointer refers to is not a valid modifier");
      return OPERATOR_CANCELLED;
  }
  DEG_id_tag_update(&lineset->linestyle->id, 0);
  WM_event_add_notifier(C, NC_LINESTYLE, lineset->linestyle);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_modifier_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Modifier";
  ot->idname = "SCENE_OT_freestyle_modifier_copy";
  ot->description = "Duplicate the modifier within the list of modifiers";

  /* api callbacks */
  ot->exec = freestyle_modifier_copy_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Modifier Move Operator
 * \{ */

static int freestyle_modifier_move_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_LineStyleModifier);
  LineStyleModifier *modifier = ptr.data;
  int dir = RNA_enum_get(op->ptr, "direction");
  bool changed = false;

  if (!freestyle_linestyle_check_report(lineset, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  switch (freestyle_get_modifier_type(&ptr)) {
    case LS_MODIFIER_TYPE_COLOR:
      changed = BKE_linestyle_color_modifier_move(lineset->linestyle, modifier, dir);
      break;
    case LS_MODIFIER_TYPE_ALPHA:
      changed = BKE_linestyle_alpha_modifier_move(lineset->linestyle, modifier, dir);
      break;
    case LS_MODIFIER_TYPE_THICKNESS:
      changed = BKE_linestyle_thickness_modifier_move(lineset->linestyle, modifier, dir);
      break;
    case LS_MODIFIER_TYPE_GEOMETRY:
      changed = BKE_linestyle_geometry_modifier_move(lineset->linestyle, modifier, dir);
      break;
    default:
      BKE_report(
          op->reports, RPT_ERROR, "The object the data pointer refers to is not a valid modifier");
      return OPERATOR_CANCELLED;
  }

  if (changed) {
    DEG_id_tag_update(&lineset->linestyle->id, 0);
    WM_event_add_notifier(C, NC_LINESTYLE, lineset->linestyle);
  }

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_modifier_move(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Move Modifier";
  ot->idname = "SCENE_OT_freestyle_modifier_move";
  ot->description = "Move the modifier within the list of modifiers";

  /* api callbacks */
  ot->exec = freestyle_modifier_move_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* props */
  RNA_def_enum(ot->srna,
               "direction",
               direction_items,
               0,
               "Direction",
               "Direction to move the chosen modifier towards");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Stroke Material Create Operator
 * \{ */

static int freestyle_stroke_material_create_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineStyle *linestyle = BKE_linestyle_active_from_view_layer(view_layer);

  if (!linestyle) {
    BKE_report(op->reports, RPT_ERROR, "No active line style in the current scene");
    return OPERATOR_CANCELLED;
  }

  FRS_create_stroke_material(bmain, linestyle);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_stroke_material_create(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Create Freestyle Stroke Material";
  ot->idname = "SCENE_OT_freestyle_stroke_material_create";
  ot->description = "Create Freestyle stroke material for testing";

  /* api callbacks */
  ot->exec = freestyle_stroke_material_create_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

#endif /* WITH_FREESTYLE */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Slot Move Operator
 * \{ */

static int texture_slot_move_exec(bContext *C, wmOperator *op)
{
  ID *id = CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).owner_id;

  if (id) {
    MTex **mtex_ar, *mtexswap;
    short act;
    int type = RNA_enum_get(op->ptr, "type");
    struct AnimData *adt = BKE_animdata_from_id(id);

    give_active_mtex(id, &mtex_ar, &act);

    if (type == -1) { /* Up */
      if (act > 0) {
        mtexswap = mtex_ar[act];
        mtex_ar[act] = mtex_ar[act - 1];
        mtex_ar[act - 1] = mtexswap;

        BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, act - 1, -1, 0);
        BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, act, act - 1, 0);
        BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, -1, act, 0);

        set_active_mtex(id, act - 1);
      }
    }
    else { /* Down */
      if (act < MAX_MTEX - 1) {
        mtexswap = mtex_ar[act];
        mtex_ar[act] = mtex_ar[act + 1];
        mtex_ar[act + 1] = mtexswap;

        BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, act + 1, -1, 0);
        BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, act, act + 1, 0);
        BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, -1, act, 0);

        set_active_mtex(id, act + 1);
      }
    }

    DEG_id_tag_update(id, 0);
    WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));
  }

  return OPERATOR_FINISHED;
}

void TEXTURE_OT_slot_move(wmOperatorType *ot)
{
  static const EnumPropertyItem slot_move[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Move Texture Slot";
  ot->idname = "TEXTURE_OT_slot_move";
  ot->description = "Move texture slots up and down";

  /* api callbacks */
  ot->exec = texture_slot_move_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Copy Operator
 * \{ */

/* material copy/paste */
static int copy_material_exec(bContext *C, wmOperator *UNUSED(op))
{
  Material *ma = CTX_data_pointer_get_type(C, "material", &RNA_Material).data;

  if (ma == NULL) {
    return OPERATOR_CANCELLED;
  }

  BKE_material_copybuf_copy(CTX_data_main(C), ma);

  return OPERATOR_FINISHED;
}

void MATERIAL_OT_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Material";
  ot->idname = "MATERIAL_OT_copy";
  ot->description = "Copy the material settings and nodes";

  /* api callbacks */
  ot->exec = copy_material_exec;

  /* flags */
  /* no undo needed since no changes are made to the material */
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Paste Operator
 * \{ */

static int paste_material_exec(bContext *C, wmOperator *UNUSED(op))
{
  Material *ma = CTX_data_pointer_get_type(C, "material", &RNA_Material).data;

  if (ma == NULL) {
    return OPERATOR_CANCELLED;
  }

  BKE_material_copybuf_paste(CTX_data_main(C), ma);

  DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, ma);

  return OPERATOR_FINISHED;
}

void MATERIAL_OT_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Material";
  ot->idname = "MATERIAL_OT_paste";
  ot->description = "Paste the material settings and nodes";

  /* api callbacks */
  ot->exec = paste_material_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MTex Copy/Paste Utilities
 * \{ */

static short mtexcopied = 0; /* must be reset on file load */
static MTex mtexcopybuf;

void ED_render_clear_mtex_copybuf(void)
{ /* use for file reload */
  mtexcopied = 0;
}

static void copy_mtex_copybuf(ID *id)
{
  MTex **mtex = NULL;

  switch (GS(id->name)) {
    case ID_PA:
      mtex = &(((ParticleSettings *)id)->mtex[(int)((ParticleSettings *)id)->texact]);
      break;
    case ID_LS:
      mtex = &(((FreestyleLineStyle *)id)->mtex[(int)((FreestyleLineStyle *)id)->texact]);
      break;
    default:
      break;
  }

  if (mtex && *mtex) {
    memcpy(&mtexcopybuf, *mtex, sizeof(MTex));
    mtexcopied = 1;
  }
  else {
    mtexcopied = 0;
  }
}

static void paste_mtex_copybuf(ID *id)
{
  MTex **mtex = NULL;

  if (mtexcopied == 0 || mtexcopybuf.tex == NULL) {
    return;
  }

  switch (GS(id->name)) {
    case ID_PA:
      mtex = &(((ParticleSettings *)id)->mtex[(int)((ParticleSettings *)id)->texact]);
      break;
    case ID_LS:
      mtex = &(((FreestyleLineStyle *)id)->mtex[(int)((FreestyleLineStyle *)id)->texact]);
      break;
    default:
      BLI_assert(!"invalid id type");
      return;
  }

  if (mtex) {
    if (*mtex == NULL) {
      *mtex = MEM_mallocN(sizeof(MTex), "mtex copy");
    }
    else if ((*mtex)->tex) {
      id_us_min(&(*mtex)->tex->id);
    }

    memcpy(*mtex, &mtexcopybuf, sizeof(MTex));

    id_us_plus((ID *)mtexcopybuf.tex);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Slot Copy Operator
 * \{ */

static int copy_mtex_exec(bContext *C, wmOperator *UNUSED(op))
{
  ID *id = CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).owner_id;

  if (id == NULL) {
    /* copying empty slot */
    ED_render_clear_mtex_copybuf();
    return OPERATOR_CANCELLED;
  }

  copy_mtex_copybuf(id);

  return OPERATOR_FINISHED;
}

static bool copy_mtex_poll(bContext *C)
{
  ID *id = CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).owner_id;

  return (id != NULL);
}

void TEXTURE_OT_slot_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Texture Slot Settings";
  ot->idname = "TEXTURE_OT_slot_copy";
  ot->description = "Copy the material texture settings and nodes";

  /* api callbacks */
  ot->exec = copy_mtex_exec;
  ot->poll = copy_mtex_poll;

  /* flags */
  /* no undo needed since no changes are made to the mtex */
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Slot Paste Operator
 * \{ */

static int paste_mtex_exec(bContext *C, wmOperator *UNUSED(op))
{
  ID *id = CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).owner_id;

  if (id == NULL) {
    Material *ma = CTX_data_pointer_get_type(C, "material", &RNA_Material).data;
    Light *la = CTX_data_pointer_get_type(C, "light", &RNA_Light).data;
    World *wo = CTX_data_pointer_get_type(C, "world", &RNA_World).data;
    ParticleSystem *psys =
        CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem).data;
    FreestyleLineStyle *linestyle =
        CTX_data_pointer_get_type(C, "line_style", &RNA_FreestyleLineStyle).data;

    if (ma) {
      id = &ma->id;
    }
    else if (la) {
      id = &la->id;
    }
    else if (wo) {
      id = &wo->id;
    }
    else if (psys) {
      id = &psys->part->id;
    }
    else if (linestyle) {
      id = &linestyle->id;
    }

    if (id == NULL) {
      return OPERATOR_CANCELLED;
    }
  }

  paste_mtex_copybuf(id);

  WM_event_add_notifier(C, NC_TEXTURE | ND_SHADING_LINKS, NULL);

  return OPERATOR_FINISHED;
}

void TEXTURE_OT_slot_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Texture Slot Settings";
  ot->idname = "TEXTURE_OT_slot_paste";
  ot->description = "Copy the texture settings and nodes";

  /* api callbacks */
  ot->exec = paste_mtex_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */
