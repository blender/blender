/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edrend
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
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
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_appdir.hh"
#include "BKE_blender_copybuffer.hh"
#include "BKE_blendfile.hh"
#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_lightprobe.h"
#include "BKE_linestyle.h"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_material.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_texture.h"
#include "BKE_vfont.hh"
#include "BKE_workspace.hh"
#include "BKE_world.h"

#include "NOD_composite.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#ifdef WITH_FREESTYLE
#  include "BKE_freestyle.h"
#  include "FRS_freestyle.h"
#  include "RNA_enum_types.hh"
#endif

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_curve.hh"
#include "ED_mesh.hh"
#include "ED_node.hh"
#include "ED_object.hh"
#include "ED_paint.hh"
#include "ED_render.hh"
#include "ED_scene.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"

#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "engines/eevee/eevee_lightcache.hh"

#include "render_intern.hh" /* own include */

using blender::Vector;

static bool object_materials_supported_poll_ex(bContext *C, const Object *ob);

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static void material_copybuffer_filepath_get(char filepath[FILE_MAX], size_t filepath_maxncpy)
{
  BLI_path_join(filepath, filepath_maxncpy, BKE_tempdir_base(), "copybuffer_material.blend");
}

static bool object_array_for_shading_edit_mode_enabled_filter(const Object *ob, void *user_data)
{
  bContext *C = static_cast<bContext *>(user_data);
  if (object_materials_supported_poll_ex(C, ob)) {
    if (BKE_object_is_in_editmode(ob) == true) {
      return true;
    }
  }
  return false;
}

static Vector<Object *> object_array_for_shading_edit_mode_enabled(bContext *C)
{
  return blender::ed::object::objects_in_mode_or_selected(
      C, object_array_for_shading_edit_mode_enabled_filter, C);
}

static bool object_array_for_shading_edit_mode_disabled_filter(const Object *ob, void *user_data)
{
  bContext *C = static_cast<bContext *>(user_data);
  if (object_materials_supported_poll_ex(C, ob)) {
    if (BKE_object_is_in_editmode(ob) == false) {
      return true;
    }
  }
  return false;
}

static Vector<Object *> object_array_for_shading_edit_mode_disabled(bContext *C)
{
  return blender::ed::object::objects_in_mode_or_selected(
      C, object_array_for_shading_edit_mode_disabled_filter, C);
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
  const ID *data = static_cast<ID *>(ob->data);
  return (data && ID_IS_EDITABLE(data) && !ID_IS_OVERRIDE_LIBRARY(data));
}

static bool object_materials_supported_poll(bContext *C)
{
  Object *ob = blender::ed::object::context_object(C);
  return object_materials_supported_poll_ex(C, ob);
}

static bool material_slot_populated_poll(bContext *C)
{
  const Object *ob_active = CTX_data_active_object(C);
  if (ob_active == nullptr) {
    return false;
  }
  return ob_active->actcol > 0;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot Add Operator
 * \{ */

static wmOperatorStatus material_slot_add_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = blender::ed::object::context_object(C);

  if (!ob) {
    return OPERATOR_CANCELLED;
  }

  BKE_object_material_slot_add(bmain, ob);

  if (ob->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(*scene, *ob, nullptr, nullptr, nullptr, nullptr);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
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

  /* API callbacks. */
  ot->exec = material_slot_add_exec;
  ot->poll = object_materials_supported_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot Remove Operator
 * \{ */

static bool material_slot_remove_poll(bContext *C)
{
  const Object *ob = blender::ed::object::context_object(C);

  if (!object_materials_supported_poll_ex(C, ob)) {
    return false;
  }

  /* Removing material slots in edit mode screws things up, see bug #21822. */
  if (BKE_object_is_in_editmode(ob)) {
    CTX_wm_operator_poll_msg_set(C, "Unable to remove material slot in edit mode");
    return false;
  }
  if (!material_slot_populated_poll(C)) {
    return false;
  }

  return true;
}

static wmOperatorStatus material_slot_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = blender::ed::object::context_object(C);

  if (!ob) {
    return OPERATOR_CANCELLED;
  }

  BKE_object_material_slot_remove(CTX_data_main(C), ob);

  if (ob->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(*scene, *ob, nullptr, nullptr, nullptr, nullptr);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
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

  /* API callbacks. */
  ot->exec = material_slot_remove_exec;
  ot->poll = material_slot_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot Assign Operator
 * \{ */

static wmOperatorStatus material_slot_assign_exec(bContext *C, wmOperator * /*op*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  bool changed_multi = false;

  Object *obact = CTX_data_active_object(C);
  const Material *mat_active = obact ? BKE_object_material_get(obact, obact->actcol) : nullptr;

  Vector<Object *> objects = object_array_for_shading_edit_mode_enabled(C);
  for (Object *ob : objects) {
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
       * NOTE: there may be multiple but that's not a common use case. */
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
    else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
      ListBase *nurbs = BKE_curve_editNurbs_get((Curve *)ob->data);

      if (nurbs) {
        LISTBASE_FOREACH (Nurb *, nu, nurbs) {
          if (ED_curve_nurb_select_check(v3d, nu)) {
            changed = true;
            nu->mat_nr = mat_nr_active;
          }
        }
      }
    }
    else if (ob->type == OB_FONT) {
      const Curve *cu = static_cast<const Curve *>(ob->data);
      EditFont *ef = cu->editfont;
      int i, selstart, selend;

      if (ef && BKE_vfont_select_get(cu, &selstart, &selend)) {
        for (i = selstart; i <= selend; i++) {
          changed = true;
          ef->textbufinfo[i].mat_nr = mat_nr_active;
        }
      }
    }

    if (changed) {
      changed_multi = true;
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
    }
  }

  return (changed_multi) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void OBJECT_OT_material_slot_assign(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Assign Material Slot";
  ot->idname = "OBJECT_OT_material_slot_assign";
  ot->description = "Assign active material slot to selection";

  /* API callbacks. */
  ot->exec = material_slot_assign_exec;
  ot->poll = object_materials_supported_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot (De)Select Operator
 * \{ */

static wmOperatorStatus material_slot_de_select(bContext *C, bool select)
{
  bool changed_multi = false;
  Object *obact = CTX_data_active_object(C);
  const Material *mat_active = obact ? BKE_object_material_get(obact, obact->actcol) : nullptr;

  Vector<Object *> objects = object_array_for_shading_edit_mode_enabled(C);
  for (Object *ob : objects) {
    if (ob->totcol == 0) {
      continue;
    }

    const short mat_nr_active = BKE_object_material_index_get_with_hint(
        ob, mat_active, obact ? obact->actcol - 1 : -1);

    if (mat_nr_active == -1) {
      continue;
    }

    bool changed = false;

    if (ob->type == OB_MESH) {
      BMEditMesh *em = BKE_editmesh_from_object(ob);

      if (em) {
        changed = EDBM_deselect_by_material(em, mat_nr_active, select);
      }
    }
    else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
      ListBase *nurbs = BKE_curve_editNurbs_get((Curve *)ob->data);
      BPoint *bp;
      BezTriple *bezt;
      int a;

      if (nurbs) {
        LISTBASE_FOREACH (Nurb *, nu, nurbs) {
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
      DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
    }
  }

  return (changed_multi) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static wmOperatorStatus material_slot_select_exec(bContext *C, wmOperator * /*op*/)
{
  return material_slot_de_select(C, true);
}

void OBJECT_OT_material_slot_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Material Slot";
  ot->idname = "OBJECT_OT_material_slot_select";
  ot->description = "Select by active material slot";

  /* API callbacks. */
  ot->exec = material_slot_select_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

static wmOperatorStatus material_slot_deselect_exec(bContext *C, wmOperator * /*op*/)
{
  return material_slot_de_select(C, false);
}

void OBJECT_OT_material_slot_deselect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Deselect Material Slot";
  ot->idname = "OBJECT_OT_material_slot_deselect";
  ot->description = "Deselect by active material slot";

  /* API callbacks. */
  ot->exec = material_slot_deselect_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot Copy Operator
 * \{ */

static wmOperatorStatus material_slot_copy_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = blender::ed::object::context_object(C);
  Material ***matar_obdata;

  if (!ob || !(matar_obdata = BKE_object_material_array_p(ob))) {
    return OPERATOR_CANCELLED;
  }

  BLI_assert(ob->totcol == *BKE_object_material_len_p(ob));

  Material ***matar_object = &ob->mat;

  Material **matar = static_cast<Material **>(
      MEM_callocN(sizeof(*matar) * size_t(ob->totcol), __func__));
  for (int i = ob->totcol; i--;) {
    matar[i] = ob->matbits[i] ? (*matar_object)[i] : (*matar_obdata)[i];
  }

  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob != ob_iter && BKE_object_material_array_p(ob_iter)) {
      /* If we are using the same obdata, we only assign slots in ob_iter that are using object
       * materials, and not obdata ones. */
      const bool is_same_obdata = ob->data == ob_iter->data;

      /* If we are using the same obdata, make the target object inherit the matbits of the active
       * object. Without this, object material slots are not copied unless the target object
       * already had its material slot link set to object. */
      if (is_same_obdata) {
        for (int i = ob->totcol; i--;) {
          if (ob->matbits[i]) {
            ob_iter->matbits[i] = ob->matbits[i];
          }
        }
      }

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

  /* API callbacks. */
  ot->exec = material_slot_copy_exec;
  ot->poll = material_slot_populated_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slot Move Operator
 * \{ */

static wmOperatorStatus material_slot_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = blender::ed::object::context_object(C);

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

  slot_remap = MEM_malloc_arrayN<uint>(ob->totcol, __func__);

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
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Material";
  ot->idname = "OBJECT_OT_material_slot_move";
  ot->description = "Move the active material up/down in the list";

  /* API callbacks. */
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

static wmOperatorStatus material_slot_remove_unused_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  int removed = 0;

  Vector<Object *> objects = object_array_for_shading_edit_mode_disabled(C);
  for (Object *ob : objects) {
    int actcol = ob->actcol;
    for (int slot = 1; slot <= ob->totcol; slot++) {
      while (slot <= ob->totcol && !BKE_object_material_slot_used(ob, slot)) {
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

  if (!removed) {
    return OPERATOR_CANCELLED;
  }

  BKE_reportf(op->reports, RPT_INFO, "Removed %d slots", removed);

  Object *ob_active = CTX_data_active_object(C);
  if (ob_active->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(*scene, *ob_active, nullptr, nullptr, nullptr, nullptr);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
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

  /* API callbacks. */
  ot->exec = material_slot_remove_unused_exec;
  ot->poll = material_slot_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus material_slot_remove_all_exec(bContext *C, wmOperator *op)
{
  /* Removing material slots in edit mode screws things up, see bug #21822. */
  Object *ob_active = CTX_data_active_object(C);
  Main *bmain = CTX_data_main(C);
  int removed = 0;

  Vector<Object *> objects = object_array_for_shading_edit_mode_disabled(C);
  for (Object *ob : objects) {
    int actcol = ob->actcol;
    for (int slot = 1; slot <= ob->totcol; slot++) {
      while (slot <= ob->totcol) {
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

  if (!removed) {
    return OPERATOR_CANCELLED;
  }

  BKE_reportf(op->reports, RPT_INFO, "Removed %d materials", removed);

  if (ob_active->mode == OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(*scene, *ob_active, nullptr, nullptr, nullptr, nullptr);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob_active);
  WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, ob_active);
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_PREVIEW, ob_active);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_remove_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove All Materials";
  ot->idname = "OBJECT_OT_material_slot_remove_all";
  ot->description = "Remove all materials";

  /* API callbacks. */
  ot->exec = material_slot_remove_all_exec;
  ot->poll = material_slot_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Material Operator
 * \{ */

static wmOperatorStatus new_material_exec(bContext *C, wmOperator * /*op*/)
{
  Material *ma = static_cast<Material *>(
      CTX_data_pointer_get_type(C, "material", &RNA_Material).data);
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr;
  PropertyRNA *prop;

  /* hook into UI */
  UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

  Object *ob = static_cast<Object *>((prop && RNA_struct_is_a(ptr.type, &RNA_Object)) ? ptr.data :
                                                                                        nullptr);

  /* add or copy material */
  if (ma) {
    Material *new_ma = (Material *)BKE_id_copy_ex(
        bmain, &ma->id, nullptr, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS);
    ma = new_ma;
  }
  else {
    const char *name = DATA_("Material");
    if (!(ob != nullptr && ob->type == OB_GREASE_PENCIL)) {
      ma = BKE_material_add(bmain, name);
    }
    else {
      ma = BKE_gpencil_material_add(bmain, name);
    }
    ED_node_shader_default(C, bmain, &ma->id);
  }

  if (prop) {
    if (ob != nullptr) {
      /* Add slot follows user-preferences for creating new slots,
       * RNA pointer assignment doesn't, see: #60014. */
      if (BKE_object_material_get_p(ob, ob->actcol) == nullptr) {
        BKE_object_material_slot_add(bmain, ob);
      }
    }

    /* when creating new ID blocks, use is already 1, but RNA
     * pointer use also increases user, so this compensates it */
    id_us_min(&ma->id);

    if (ptr.owner_id) {
      BKE_id_move_to_same_lib(*bmain, ma->id, *ptr.owner_id);
    }

    PointerRNA idptr = RNA_id_pointer_create(&ma->id);
    RNA_property_pointer_set(&ptr, prop, idptr, nullptr);
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

  /* API callbacks. */
  ot->exec = new_material_exec;
  ot->poll = object_materials_supported_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Texture Operator
 * \{ */

static wmOperatorStatus new_texture_exec(bContext *C, wmOperator *op)
{
  Tex *tex = static_cast<Tex *>(CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data);
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr;
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

  bool linked_id_created = false;
  if (prop) {
    /* when creating new ID blocks, use is already 1, but RNA
     * pointer use also increases user, so this compensates it */
    id_us_min(&tex->id);

    if (ptr.owner_id) {
      BKE_id_move_to_same_lib(*bmain, tex->id, *ptr.owner_id);
      linked_id_created = ID_IS_LINKED(&tex->id);
    }

    PointerRNA idptr = RNA_id_pointer_create(&tex->id);
    RNA_property_pointer_set(&ptr, prop, idptr, nullptr);
    RNA_property_update(C, &ptr, prop);
  }

  if (!linked_id_created) {
    ED_undo_push_op(C, op);
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

  /* API callbacks. */
  ot->exec = new_texture_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name new world operator
 * \{ */

static wmOperatorStatus new_world_exec(bContext *C, wmOperator * /*op*/)
{
  World *wo = static_cast<World *>(CTX_data_pointer_get_type(C, "world", &RNA_World).data);
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr;
  PropertyRNA *prop;

  /* add or copy world */
  if (wo) {
    World *new_wo = (World *)BKE_id_copy_ex(
        bmain, &wo->id, nullptr, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS);
    wo = new_wo;
  }
  else {
    wo = BKE_world_add(bmain, CTX_DATA_(BLT_I18NCONTEXT_ID_WORLD, "World"));
    ED_node_shader_default(C, bmain, &wo->id);
  }

  /* hook into UI */
  UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

  if (prop) {
    /* when creating new ID blocks, use is already 1, but RNA
     * pointer use also increases user, so this compensates it */
    id_us_min(&wo->id);

    if (ptr.owner_id) {
      BKE_id_move_to_same_lib(*bmain, wo->id, *ptr.owner_id);
    }

    PointerRNA idptr = RNA_id_pointer_create(&wo->id);
    RNA_property_pointer_set(&ptr, prop, idptr, nullptr);
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

  /* API callbacks. */
  ot->exec = new_world_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Layer Add Operator
 * \{ */

static wmOperatorStatus view_layer_add_exec(bContext *C, wmOperator *op)
{
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = CTX_data_scene(C);

  ViewLayer *view_layer_current = win ? WM_window_get_active_view_layer(win) : nullptr;
  int type = RNA_enum_get(op->ptr, "type");
  /* Copy requires a source. */
  if (type == VIEWLAYER_ADD_COPY) {
    if (view_layer_current == nullptr) {
      type = VIEWLAYER_ADD_NEW;
    }
  }
  ViewLayer *view_layer_new = BKE_view_layer_add(
      scene, view_layer_current ? view_layer_current->name : nullptr, view_layer_current, type);

  if (win) {
    WM_window_set_active_view_layer(win, view_layer_new);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Add View Layer";
  ot->idname = "SCENE_OT_view_layer_add";
  ot->description = "Add a view layer";

  /* API callbacks. */
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

static wmOperatorStatus view_layer_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (!ED_scene_view_layer_delete(bmain, scene, view_layer, nullptr)) {
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

  /* API callbacks. */
  ot->exec = view_layer_remove_exec;
  ot->poll = view_layer_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Add AOV Operator
 * \{ */

static wmOperatorStatus view_layer_add_aov_exec(bContext *C, wmOperator * /*op*/)
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
    engine = nullptr;
  }

  if (scene->compositing_node_group) {
    ntreeCompositUpdateRLayers(scene->compositing_node_group);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
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

  /* API callbacks. */
  ot->exec = view_layer_add_aov_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Remove AOV Operator
 * \{ */

static wmOperatorStatus view_layer_remove_aov_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (view_layer->active_aov == nullptr) {
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
    engine = nullptr;
  }

  if (scene->compositing_node_group) {
    ntreeCompositUpdateRLayers(scene->compositing_node_group);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
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

  /* API callbacks. */
  ot->exec = view_layer_remove_aov_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Add Lightgroup Operator
 * \{ */

static wmOperatorStatus view_layer_add_lightgroup_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  char name[MAX_NAME];
  name[0] = '\0';
  /* If a name is provided, ensure that it is unique. */
  if (RNA_struct_property_is_set(op->ptr, "name")) {
    RNA_string_get(op->ptr, "name", name);
    /* Ensure that there are no dots in the name. */
    BLI_string_replace_char(name, '.', '_');
    LISTBASE_FOREACH (ViewLayerLightgroup *, lightgroup, &view_layer->lightgroups) {
      if (STREQ(lightgroup->name, name)) {
        return OPERATOR_CANCELLED;
      }
    }
  }

  BKE_view_layer_add_lightgroup(view_layer, name);

  if (scene->compositing_node_group) {
    ntreeCompositUpdateRLayers(scene->compositing_node_group);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_view_layer_add_lightgroup(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Lightgroup";
  ot->idname = "SCENE_OT_view_layer_add_lightgroup";
  ot->description = "Add a Light Group";

  /* API callbacks. */
  ot->exec = view_layer_add_lightgroup_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  ot->prop = RNA_def_string(ot->srna,
                            "name",
                            nullptr,
                            sizeof(ViewLayerLightgroup::name),
                            "Name",
                            "Name of newly created lightgroup");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Remove Lightgroup Operator
 * \{ */

static wmOperatorStatus view_layer_remove_lightgroup_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (view_layer->active_lightgroup == nullptr) {
    return OPERATOR_FINISHED;
  }

  BKE_view_layer_remove_lightgroup(view_layer, view_layer->active_lightgroup);

  if (scene->compositing_node_group) {
    ntreeCompositUpdateRLayers(scene->compositing_node_group);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_view_layer_remove_lightgroup(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Lightgroup";
  ot->idname = "SCENE_OT_view_layer_remove_lightgroup";
  ot->description = "Remove Active Lightgroup";

  /* API callbacks. */
  ot->exec = view_layer_remove_lightgroup_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Add Used Lightgroups Operator
 * \{ */

static blender::Set<blender::StringRefNull> get_used_lightgroups(Scene *scene)
{
  blender::Set<blender::StringRefNull> used_lightgroups;

  FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
    if (ob->lightgroup && ob->lightgroup->name[0]) {
      used_lightgroups.add_as(ob->lightgroup->name);
    }
  }
  FOREACH_SCENE_OBJECT_END;

  if (scene->world && scene->world->lightgroup && scene->world->lightgroup->name[0]) {
    used_lightgroups.add_as(scene->world->lightgroup->name);
  }

  return used_lightgroups;
}

static wmOperatorStatus view_layer_add_used_lightgroups_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  blender::Set<blender::StringRefNull> used_lightgroups = get_used_lightgroups(scene);
  for (const blender::StringRefNull used_lightgroup : used_lightgroups) {
    if (!BLI_findstring(&view_layer->lightgroups,
                        used_lightgroup.c_str(),
                        offsetof(ViewLayerLightgroup, name)))
    {
      BKE_view_layer_add_lightgroup(view_layer, used_lightgroup.c_str());
    }
  }

  if (scene->compositing_node_group) {
    ntreeCompositUpdateRLayers(scene->compositing_node_group);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_view_layer_add_used_lightgroups(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Used Lightgroups";
  ot->idname = "SCENE_OT_view_layer_add_used_lightgroups";
  ot->description = "Add all used Light Groups";

  /* API callbacks. */
  ot->exec = view_layer_add_used_lightgroups_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Remove Unused Lightgroups Operator
 * \{ */

static wmOperatorStatus view_layer_remove_unused_lightgroups_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  blender::Set<blender::StringRefNull> used_lightgroups = get_used_lightgroups(scene);
  LISTBASE_FOREACH_MUTABLE (ViewLayerLightgroup *, lightgroup, &view_layer->lightgroups) {
    if (!used_lightgroups.contains_as(lightgroup->name)) {
      BKE_view_layer_remove_lightgroup(view_layer, lightgroup);
    }
  }

  if (scene->compositing_node_group) {
    ntreeCompositUpdateRLayers(scene->compositing_node_group);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_view_layer_remove_unused_lightgroups(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Unused Lightgroups";
  ot->idname = "SCENE_OT_view_layer_remove_unused_lightgroups";
  ot->description = "Remove all unused Light Groups";

  /* API callbacks. */
  ot->exec = view_layer_remove_unused_lightgroups_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Cache Bake Operator
 * \{ */

enum {
  LIGHTCACHE_SUBSET_ALL = 0,
  LIGHTCACHE_SUBSET_SELECTED,
  LIGHTCACHE_SUBSET_ACTIVE,
};

static blender::Vector<Object *> lightprobe_cache_irradiance_volume_subset_get(bContext *C,
                                                                               wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);

  auto is_irradiance_volume = [](Object *ob) -> bool {
    return ob->type == OB_LIGHTPROBE &&
           static_cast<LightProbe *>(ob->data)->type == LIGHTPROBE_TYPE_VOLUME;
  };

  blender::Vector<Object *> probes;

  auto irradiance_volume_setup = [&](Object *ob) {
    BKE_lightprobe_cache_free(ob);
    BKE_lightprobe_cache_create(ob);
    DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SHADING);
    probes.append(ob);
  };

  int subset = RNA_enum_get(op->ptr, "subset");
  switch (subset) {
    case LIGHTCACHE_SUBSET_ALL: {
      FOREACH_OBJECT_BEGIN (scene, view_layer, ob) {
        if (is_irradiance_volume(ob)) {
          irradiance_volume_setup(ob);
        }
      }
      FOREACH_OBJECT_END;
      break;
    }
    case LIGHTCACHE_SUBSET_SELECTED: {
      ObjectsInViewLayerParams parameters;
      parameters.filter_fn = nullptr;
      parameters.no_dup_data = true;
      Vector<Object *> objects = BKE_view_layer_array_selected_objects_params(
          view_layer, nullptr, &parameters);
      for (Object *ob : objects) {
        if (is_irradiance_volume(ob)) {
          irradiance_volume_setup(ob);
        }
      }
      break;
    }
    case LIGHTCACHE_SUBSET_ACTIVE: {
      Object *active_ob = CTX_data_active_object(C);
      if (is_irradiance_volume(active_ob)) {
        irradiance_volume_setup(active_ob);
      }
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }

  return probes;
}

struct BakeOperatorData {
  /* Store actual owner of job, so modal operator could check for it,
   * the reason of this is that active scene could change when rendering
   * several layers from compositor #31800. */
  Scene *scene;

  std::string report;
};

static wmOperatorStatus lightprobe_cache_bake_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent * /*event*/)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  blender::Vector<Object *> probes = lightprobe_cache_irradiance_volume_subset_get(C, op);

  if (probes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  BakeOperatorData *data = MEM_new<BakeOperatorData>(__func__);
  data->scene = scene;
  data->report = "";

  wmJob *wm_job = EEVEE_lightbake_job_create(
      wm, win, bmain, view_layer, scene, probes, data->report, scene->r.cfra, 0);
  if (wm_job == nullptr) {
    MEM_delete(data);
    BKE_report(op->reports, RPT_WARNING, "Cannot bake light probe while rendering");
    return OPERATOR_CANCELLED;
  }

  WM_event_add_modal_handler(C, op);

  op->customdata = static_cast<void *>(data);

  WM_jobs_start(wm, wm_job);

  WM_cursor_wait(false);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus lightprobe_cache_bake_modal(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  BakeOperatorData *data = static_cast<BakeOperatorData *>(op->customdata);
  Scene *scene = data->scene;

  /* No running bake, remove handler and pass through. */
  if (0 == WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_LIGHT_BAKE)) {
    std::string report = data->report;

    MEM_delete(data);
    op->customdata = nullptr;

    if (!report.empty()) {
      BKE_report(op->reports, RPT_ERROR, report.c_str());
      return OPERATOR_CANCELLED;
    }
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* Running bake. */
  switch (event->type) {
    case EVT_ESCKEY:
      return OPERATOR_RUNNING_MODAL;
    default: {
      break;
    }
  }
  return OPERATOR_PASS_THROUGH;
}

static void lightprobe_cache_bake_cancel(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  Scene *scene = static_cast<BakeOperatorData *>(op->customdata)->scene;

  /* Kill on cancel, because job is using op->reports. */
  WM_jobs_kill_type(wm, scene, WM_JOB_TYPE_LIGHT_BAKE);
}

/* Executes blocking bake. */
static wmOperatorStatus lightprobe_cache_bake_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  G.is_break = false;

  blender::Vector<Object *> probes = lightprobe_cache_irradiance_volume_subset_get(C, op);

  std::string report;
  void *rj = EEVEE_lightbake_job_data_alloc(
      bmain, view_layer, scene, probes, report, scene->r.cfra);
  /* Do the job. */
  wmJobWorkerStatus worker_status = {};
  EEVEE_lightbake_job(rj, &worker_status);
  /* Move baking data to original object and then free it. */
  EEVEE_lightbake_update(rj);
  EEVEE_lightbake_job_data_free(rj);

  if (!report.empty()) {
    BKE_report(op->reports, RPT_ERROR, report.c_str());
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_lightprobe_cache_bake(wmOperatorType *ot)
{
  static const EnumPropertyItem light_cache_subset_items[] = {
      {LIGHTCACHE_SUBSET_ALL, "ALL", 0, "All Volumes", "Bake all light probe volumes"},
      {LIGHTCACHE_SUBSET_SELECTED,
       "SELECTED",
       0,
       "Selected Only",
       "Only bake selected light probe volumes"},
      {LIGHTCACHE_SUBSET_ACTIVE,
       "ACTIVE",
       0,
       "Active Only",
       "Only bake the active light probe volume"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Bake Light Cache";
  ot->idname = "OBJECT_OT_lightprobe_cache_bake";
  ot->description = "Bake irradiance volume light cache";

  /* API callbacks. */
  ot->invoke = lightprobe_cache_bake_invoke;
  ot->modal = lightprobe_cache_bake_modal;
  ot->cancel = lightprobe_cache_bake_cancel;
  ot->exec = lightprobe_cache_bake_exec;

  ot->prop = RNA_def_enum(
      ot->srna, "subset", light_cache_subset_items, 0, "Subset", "Subset of probes to update");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Cache Free Operator
 * \{ */

static wmOperatorStatus lightprobe_cache_free_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  /* Kill potential bake job first (see #57011). */
  wmWindowManager *wm = CTX_wm_manager(C);
  WM_jobs_kill_type(wm, scene, WM_JOB_TYPE_LIGHT_BAKE);

  blender::Vector<Object *> probes = lightprobe_cache_irradiance_volume_subset_get(C, op);

  for (Object *object : probes) {
    if (object->lightprobe_cache == nullptr) {
      continue;
    }
    BKE_lightprobe_cache_free(object);
    DEG_id_tag_update(&object->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SHADING);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, scene);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_lightprobe_cache_free(wmOperatorType *ot)
{
  static const EnumPropertyItem lightprobe_subset_items[] = {
      {LIGHTCACHE_SUBSET_ALL,
       "ALL",
       0,
       "All Light Probes",
       "Delete all light probes' baked lighting data"},
      {LIGHTCACHE_SUBSET_SELECTED,
       "SELECTED",
       0,
       "Selected Only",
       "Only delete selected light probes' baked lighting data"},
      {LIGHTCACHE_SUBSET_ACTIVE,
       "ACTIVE",
       0,
       "Active Only",
       "Only delete the active light probe's baked lighting data"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Delete Light Cache";
  ot->idname = "OBJECT_OT_lightprobe_cache_free";
  ot->description = "Delete cached indirect lighting";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->exec = lightprobe_cache_free_exec;

  ot->prop = RNA_def_enum(ot->srna,
                          "subset",
                          lightprobe_subset_items,
                          LIGHTCACHE_SUBSET_SELECTED,
                          "Subset",
                          "Subset of probes to update");
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

static wmOperatorStatus render_view_add_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  BKE_scene_add_render_view(scene, nullptr);
  scene->r.actview = BLI_listbase_count(&scene->r.views) - 1;

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  BKE_ntree_update_tag_id_changed(bmain, &scene->id);
  BKE_main_ensure_invariants(*bmain);

  return OPERATOR_FINISHED;
}

void SCENE_OT_render_view_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Render View";
  ot->idname = "SCENE_OT_render_view_add";
  ot->description = "Add a render view";

  /* API callbacks. */
  ot->exec = render_view_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render View Add Operator
 * \{ */

static wmOperatorStatus render_view_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SceneRenderView *rv = static_cast<SceneRenderView *>(
      BLI_findlink(&scene->r.views, scene->r.actview));

  if (!BKE_scene_remove_render_view(scene, rv)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  BKE_ntree_update_tag_id_changed(bmain, &scene->id);
  BKE_main_ensure_invariants(*bmain);

  return OPERATOR_FINISHED;
}

void SCENE_OT_render_view_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Render View";
  ot->idname = "SCENE_OT_render_view_remove";
  ot->description = "Remove the selected render view";

  /* API callbacks. */
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
  FreestyleModuleConfig *module = static_cast<FreestyleModuleConfig *>(ptr.data);

  return module != nullptr;
}

static wmOperatorStatus freestyle_module_add_exec(bContext *C, wmOperator * /*op*/)
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

  /* API callbacks. */
  ot->exec = freestyle_module_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Module Remove Operator
 * \{ */

static wmOperatorStatus freestyle_module_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "freestyle_module", &RNA_FreestyleModuleSettings);
  FreestyleModuleConfig *module = static_cast<FreestyleModuleConfig *>(ptr.data);

  BKE_freestyle_module_delete(&view_layer->freestyle_config, module);

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_module_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Freestyle Module";
  ot->idname = "SCENE_OT_freestyle_module_remove";
  ot->description = "Remove the style module from the stack";

  /* API callbacks. */
  ot->poll = freestyle_active_module_poll;
  ot->exec = freestyle_module_remove_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

static wmOperatorStatus freestyle_module_move_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "freestyle_module", &RNA_FreestyleModuleSettings);
  FreestyleModuleConfig *module = static_cast<FreestyleModuleConfig *>(ptr.data);
  int dir = RNA_enum_get(op->ptr, "direction");

  if (BKE_freestyle_module_move(&view_layer->freestyle_config, module, dir)) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Freestyle Module";
  ot->idname = "SCENE_OT_freestyle_module_move";
  ot->description = "Change the position of the style module within in the list of style modules";

  /* API callbacks. */
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

static wmOperatorStatus freestyle_lineset_add_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  BKE_freestyle_lineset_add(bmain, &view_layer->freestyle_config, nullptr);

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Line Set";
  ot->idname = "SCENE_OT_freestyle_lineset_add";
  ot->description = "Add a line set into the list of line sets";

  /* API callbacks. */
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

  return BKE_freestyle_lineset_get_active(&view_layer->freestyle_config) != nullptr;
}

static wmOperatorStatus freestyle_lineset_copy_exec(bContext *C, wmOperator * /*op*/)
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
  ot->description = "Copy the active line set to the internal clipboard";

  /* API callbacks. */
  ot->exec = freestyle_lineset_copy_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Line Set Paste Operator
 * \{ */

static wmOperatorStatus freestyle_lineset_paste_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  FRS_paste_active_lineset(&view_layer->freestyle_config);

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Line Set";
  ot->idname = "SCENE_OT_freestyle_lineset_paste";
  ot->description = "Paste the internal clipboard content to the active line set";

  /* API callbacks. */
  ot->exec = freestyle_lineset_paste_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Line Set Remove Operator
 * \{ */

static wmOperatorStatus freestyle_lineset_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  FRS_delete_active_lineset(&view_layer->freestyle_config);

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Line Set";
  ot->idname = "SCENE_OT_freestyle_lineset_remove";
  ot->description = "Remove the active line set from the list of line sets";

  /* API callbacks. */
  ot->exec = freestyle_lineset_remove_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Line Set Move Operator
 * \{ */

static wmOperatorStatus freestyle_lineset_move_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int dir = RNA_enum_get(op->ptr, "direction");

  if (FRS_move_active_lineset(&view_layer->freestyle_config, dir)) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
    WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);
  }

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_move(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Line Set";
  ot->idname = "SCENE_OT_freestyle_lineset_move";
  ot->description = "Change the position of the active line set within the list of line sets";

  /* API callbacks. */
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

static wmOperatorStatus freestyle_linestyle_new_exec(bContext *C, wmOperator *op)
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
    lineset->linestyle = BKE_linestyle_new(bmain, DATA_("LineStyle"));
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

  /* API callbacks. */
  ot->exec = freestyle_linestyle_new_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Modifier Add "Color" Operator
 * \{ */

static wmOperatorStatus freestyle_color_modifier_add_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  int type = RNA_enum_get(op->ptr, "type");

  if (!freestyle_linestyle_check_report(lineset, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_linestyle_color_modifier_add(lineset->linestyle, nullptr, type) == nullptr) {
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

  /* API callbacks. */
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

static wmOperatorStatus freestyle_alpha_modifier_add_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  int type = RNA_enum_get(op->ptr, "type");

  if (!freestyle_linestyle_check_report(lineset, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_linestyle_alpha_modifier_add(lineset->linestyle, nullptr, type) == nullptr) {
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

  /* API callbacks. */
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

static wmOperatorStatus freestyle_thickness_modifier_add_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  int type = RNA_enum_get(op->ptr, "type");

  if (!freestyle_linestyle_check_report(lineset, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_linestyle_thickness_modifier_add(lineset->linestyle, nullptr, type) == nullptr) {
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

  /* API callbacks. */
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

static wmOperatorStatus freestyle_geometry_modifier_add_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  int type = RNA_enum_get(op->ptr, "type");

  if (!freestyle_linestyle_check_report(lineset, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_linestyle_geometry_modifier_add(lineset->linestyle, nullptr, type) == nullptr) {
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

  /* API callbacks. */
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

static wmOperatorStatus freestyle_modifier_remove_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_LineStyleModifier);
  LineStyleModifier *modifier = static_cast<LineStyleModifier *>(ptr.data);

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

  /* API callbacks. */
  ot->exec = freestyle_modifier_remove_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Modifier Copy Operator
 * \{ */

static wmOperatorStatus freestyle_modifier_copy_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_LineStyleModifier);
  LineStyleModifier *modifier = static_cast<LineStyleModifier *>(ptr.data);

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

  /* API callbacks. */
  ot->exec = freestyle_modifier_copy_exec;
  ot->poll = freestyle_active_lineset_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free Style Modifier Move Operator
 * \{ */

static wmOperatorStatus freestyle_modifier_move_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&view_layer->freestyle_config);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_LineStyleModifier);
  LineStyleModifier *modifier = static_cast<LineStyleModifier *>(ptr.data);
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Modifier";
  ot->idname = "SCENE_OT_freestyle_modifier_move";
  ot->description = "Move the modifier within the list of modifiers";

  /* API callbacks. */
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

static wmOperatorStatus freestyle_stroke_material_create_exec(bContext *C, wmOperator *op)
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

  /* API callbacks. */
  ot->exec = freestyle_stroke_material_create_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

#endif /* WITH_FREESTYLE */

/* -------------------------------------------------------------------- */
/** \name Texture Slot Move Operator
 * \{ */

static wmOperatorStatus texture_slot_move_exec(bContext *C, wmOperator *op)
{
  ID *id = CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).owner_id;

  if (id) {
    MTex **mtex_ar, *mtexswap;
    short act;
    int type = RNA_enum_get(op->ptr, "type");
    AnimData *adt = BKE_animdata_from_id(id);

    give_active_mtex(id, &mtex_ar, &act);

    if (type == -1) { /* Up */
      if (act > 0) {
        mtexswap = mtex_ar[act];
        mtex_ar[act] = mtex_ar[act - 1];
        mtex_ar[act - 1] = mtexswap;

        BKE_animdata_fix_paths_rename(
            id, adt, nullptr, "texture_slots", nullptr, nullptr, act - 1, -1, false);
        BKE_animdata_fix_paths_rename(
            id, adt, nullptr, "texture_slots", nullptr, nullptr, act, act - 1, false);
        BKE_animdata_fix_paths_rename(
            id, adt, nullptr, "texture_slots", nullptr, nullptr, -1, act, false);

        set_active_mtex(id, act - 1);
      }
    }
    else { /* Down */
      if (act < MAX_MTEX - 1) {
        mtexswap = mtex_ar[act];
        mtex_ar[act] = mtex_ar[act + 1];
        mtex_ar[act + 1] = mtexswap;

        BKE_animdata_fix_paths_rename(
            id, adt, nullptr, "texture_slots", nullptr, nullptr, act + 1, -1, false);
        BKE_animdata_fix_paths_rename(
            id, adt, nullptr, "texture_slots", nullptr, nullptr, act, act + 1, false);
        BKE_animdata_fix_paths_rename(
            id, adt, nullptr, "texture_slots", nullptr, nullptr, -1, act, false);

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
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Texture Slot";
  ot->idname = "TEXTURE_OT_slot_move";
  ot->description = "Move texture slots up and down";

  /* API callbacks. */
  ot->exec = texture_slot_move_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Copy Operator
 * \{ */

static wmOperatorStatus copy_material_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::blendfile;

  Material *ma = static_cast<Material *>(
      CTX_data_pointer_get_type(C, "material", &RNA_Material).data);

  if (ma == nullptr) {
    return OPERATOR_CANCELLED;
  }
  if (ID_IS_PACKED(&ma->id)) {
    /* Direct link/append of packed IDs is not supported currently, so neither is their
     * copy/pasting. */
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  PartialWriteContext copybuffer{*bmain};

  /* Add the material to the copybuffer (and all of its dependencies). */
  copybuffer.id_add(
      &ma->id,
      PartialWriteContext::IDAddOptions{(PartialWriteContext::IDAddOperations::SET_FAKE_USER |
                                         PartialWriteContext::IDAddOperations::SET_CLIPBOARD_MARK |
                                         PartialWriteContext::IDAddOperations::ADD_DEPENDENCIES)},
      nullptr);

  char filepath[FILE_MAX];
  material_copybuffer_filepath_get(filepath, sizeof(filepath));
  copybuffer.write(filepath, *op->reports);

  /* We are all done! */
  BKE_report(op->reports, RPT_INFO, "Copied material to internal clipboard");

  return OPERATOR_FINISHED;
}

void MATERIAL_OT_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Material";
  ot->idname = "MATERIAL_OT_copy";
  ot->description = "Copy the material settings and nodes";

  /* API callbacks. */
  ot->exec = copy_material_exec;
  ot->poll = material_slot_populated_poll;

  /* flags */
  /* no undo needed since no changes are made to the material */
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Paste Operator
 * \{ */

/**
 * Clear ID's as freeing the data-block doesn't handle reference counting.
 */
static int paste_material_nodetree_ids_decref(LibraryIDLinkCallbackData *cb_data)
{
  if (cb_data->cb_flag & IDWALK_CB_USER) {
    id_us_min(*cb_data->id_pointer);
  }
  *cb_data->id_pointer = nullptr;
  return IDWALK_RET_NOP;
}

/**
 * Re-map ID's from the clipboard to ID's in `bmain`, by name.
 */
static int paste_material_nodetree_ids_relink_or_clear(LibraryIDLinkCallbackData *cb_data)
{
  Main *bmain = static_cast<Main *>(cb_data->user_data);
  ID **id_p = cb_data->id_pointer;
  if (*id_p) {
    if (cb_data->cb_flag & IDWALK_CB_USER) {
      id_us_min(*id_p);
    }
    ListBase *lb = which_libbase(bmain, GS((*id_p)->name));
    ID *id_local = static_cast<ID *>(
        BLI_findstring(lb, (*id_p)->name + 2, offsetof(ID, name) + 2));
    *id_p = id_local;
    if (cb_data->cb_flag & IDWALK_CB_USER) {
      id_us_plus(id_local);
    }
    else if (cb_data->cb_flag & IDWALK_CB_USER_ONE) {
      id_us_ensure_real(id_local);
    }
    id_lib_extern(id_local);
  }
  return IDWALK_RET_NOP;
}

static wmOperatorStatus paste_material_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Material *ma = static_cast<Material *>(
      CTX_data_pointer_get_type(C, "material", &RNA_Material).data);

  if (ma == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "Cannot paste without a material");
    return OPERATOR_CANCELLED;
  }

  /* Read copy buffer .blend file. */
  char filepath[FILE_MAX];
  Main *temp_bmain = BKE_main_new();

  STRNCPY(temp_bmain->filepath, BKE_main_blendfile_path_from_global());

  material_copybuffer_filepath_get(filepath, sizeof(filepath));

  /* NOTE(@ideasman42) The node tree might reference different kinds of ID types.
   * It's not clear-cut which ID types should be included, although it's unlikely
   * users would want an entire scene & it's objects to be included.
   * Filter a subset of ID types with some reasons for including them. */
  const uint64_t ntree_filter = (
      /* Material is necessary for reading the clipboard. */
      FILTER_ID_MA |
      /* Node-groups. */
      FILTER_ID_NT |
      /* Image textures. */
      FILTER_ID_IM |
      /* Internal text (scripts). */
      FILTER_ID_TXT |
      /* Texture coordinates may reference objects.
       * Note that object data is *not* included. */
      FILTER_ID_OB);

  if (!BKE_copybuffer_read(temp_bmain, filepath, op->reports, ntree_filter)) {
    BKE_report(op->reports, RPT_ERROR, "Internal clipboard is empty");
    BKE_main_free(temp_bmain);
    return OPERATOR_CANCELLED;
  }

  /* There may be multiple materials,
   * check for a property that marks this as the active material. */
  Material *ma_from = nullptr;
  LISTBASE_FOREACH (Material *, ma_iter, &temp_bmain->materials) {
    if (ma_iter->id.flag & ID_FLAG_CLIPBOARD_MARK) {
      ma_from = ma_iter;
      break;
    }
  }

  /* Make sure data from this file is usable for material paste. */
  if (ma_from == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Internal clipboard is not from a material");
    BKE_main_free(temp_bmain);
    return OPERATOR_CANCELLED;
  }

  /* Keep animation by moving local animation to the paste node-tree. */
  if (ma->nodetree && ma_from->nodetree) {
    BLI_assert(ma_from->nodetree->adt == nullptr);
    std::swap(ma->nodetree->adt, ma_from->nodetree->adt);
  }

  /* Needed to update #SpaceNode::nodetree else a stale pointer is used. */
  if (ma->nodetree) {
    bNodeTree *nodetree = ma->nodetree;
    BKE_libblock_remap(bmain, ma->nodetree, ma_from->nodetree, ID_REMAP_FORCE_UI_POINTERS);

    /* Free & clear data here, so user counts are handled, otherwise it's
     * freed as part of #BKE_main_free which doesn't handle user-counts. */
    /* Walk over all the embedded nodes ID's (non-recursively). */
    BKE_library_foreach_ID_link(
        bmain, &nodetree->id, paste_material_nodetree_ids_decref, nullptr, IDWALK_NOP);

    blender::bke::node_tree_free_embedded_tree(nodetree);
    MEM_freeN(nodetree);
    ma->nodetree = nullptr;
  }

/* Swap data-block content, while swapping isn't always needed,
 * it means memory is properly freed in the case of allocations.. */
#define SWAP_MEMBER(member) std::swap(ma->member, ma_from->member)

  /* Intentionally skip:
   * - Texture painting slots.
   * - Preview render.
   * - Grease pencil styles (we could although they reference many ID's themselves).
   */
  SWAP_MEMBER(flag);
  SWAP_MEMBER(r);
  SWAP_MEMBER(g);
  SWAP_MEMBER(b);
  SWAP_MEMBER(a);
  SWAP_MEMBER(specr);
  SWAP_MEMBER(specg);
  SWAP_MEMBER(specb);
  SWAP_MEMBER(spec);
  SWAP_MEMBER(roughness);
  SWAP_MEMBER(metallic);
  SWAP_MEMBER(index);
  SWAP_MEMBER(nodetree);
  SWAP_MEMBER(line_col);
  SWAP_MEMBER(line_priority);
  SWAP_MEMBER(vcol_alpha);

  SWAP_MEMBER(alpha_threshold);
  SWAP_MEMBER(refract_depth);
  SWAP_MEMBER(blend_method);
  SWAP_MEMBER(blend_shadow);
  SWAP_MEMBER(blend_flag);

  SWAP_MEMBER(lineart);

#undef SWAP_MEMBER

  /* The node-tree from the clipboard is now assigned to the local material,
   * however the ID's it references are still part of `temp_bmain`.
   * These data-blocks references must be cleared or replaces with references to `bmain`.
   * TODO(@ideasman42): support merging indirectly referenced data-blocks besides the material,
   * this would be useful for pasting materials with node-groups between files. */
  if (ma->nodetree) {
    /* This implicitly points to local data, assign after remapping. */
    ma->nodetree->owner_id = nullptr;

    /* Map remote ID's to local ones. */
    BKE_library_foreach_ID_link(
        bmain, &ma->nodetree->id, paste_material_nodetree_ids_relink_or_clear, bmain, IDWALK_NOP);

    ma->nodetree->owner_id = &ma->id;
  }
  BKE_main_free(temp_bmain);

  /* Important to run this when the embedded tree if freed,
   * otherwise the depsgraph holds a reference to the (now freed) `ma->nodetree`.
   * Also run this when a new node-tree is set to ensure it's accounted for.
   * This also applies to animation data which is likely to be stored in the depsgraph.
   * Always call instead of checking when it *might* be needed. */
  DEG_relations_tag_update(bmain);

  /* There are some custom updates to the node tree above, better do a full update pass. */
  BKE_ntree_update_tag_all(ma->nodetree);
  BKE_main_ensure_invariants(*bmain);

  DEG_id_tag_update(&ma->id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, ma);

  return OPERATOR_FINISHED;
}

void MATERIAL_OT_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Material";
  ot->idname = "MATERIAL_OT_paste";
  ot->description = "Paste the material settings and nodes";

  /* API callbacks. */
  ot->exec = paste_material_exec;
  ot->poll = object_materials_supported_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MTex Copy/Paste Utilities
 * \{ */

static short mtexcopied = 0; /* must be reset on file load */
static MTex mtexcopybuf;

void ED_render_clear_mtex_copybuf()
{ /* use for file reload */
  mtexcopied = 0;
}

static void copy_mtex_copybuf(ID *id)
{
  MTex **mtex = nullptr;

  switch (GS(id->name)) {
    case ID_PA:
      mtex = &(((ParticleSettings *)id)->mtex[int(((ParticleSettings *)id)->texact)]);
      break;
    case ID_LS:
      mtex = &(((FreestyleLineStyle *)id)->mtex[int(((FreestyleLineStyle *)id)->texact)]);
      break;
    default:
      break;
  }

  if (mtex && *mtex) {
    mtexcopybuf = blender::dna::shallow_copy(**mtex);
    mtexcopied = 1;
  }
  else {
    mtexcopied = 0;
  }
}

static void paste_mtex_copybuf(ID *id)
{
  MTex **mtex = nullptr;

  if (mtexcopied == 0 || mtexcopybuf.tex == nullptr) {
    return;
  }

  switch (GS(id->name)) {
    case ID_PA:
      mtex = &(((ParticleSettings *)id)->mtex[int(((ParticleSettings *)id)->texact)]);
      break;
    case ID_LS:
      mtex = &(((FreestyleLineStyle *)id)->mtex[int(((FreestyleLineStyle *)id)->texact)]);
      break;
    default:
      BLI_assert_msg(0, "invalid id type");
      return;
  }

  if (mtex) {
    if (*mtex == nullptr) {
      *mtex = MEM_callocN<MTex>("mtex copy");
    }
    else if ((*mtex)->tex) {
      id_us_min(&(*mtex)->tex->id);
    }

    **mtex = blender::dna::shallow_copy(mtexcopybuf);

    /* NOTE(@ideasman42): the simple memory copy has no special handling for ID data-blocks.
     * Ideally this would use `BKE_copybuffer_*` API's, however for common using
     * copy-pasting between slots, the case a users expects to copy between files
     * seems quite niche. So, do primitive ID validation. */

    /* WARNING: This isn't a fool-proof solution as it's possible memory locations are reused,
     * or that the ID was relocated in memory since it was copied.
     * it does however guard against references to dangling pointers. */
    if ((*mtex)->tex && (BLI_findindex(&G_MAIN->textures, (*mtex)->tex) == -1)) {
      (*mtex)->tex = nullptr;
    }
    if ((*mtex)->object && (BLI_findindex(&G_MAIN->objects, (*mtex)->object) == -1)) {
      (*mtex)->object = nullptr;
    }
    id_us_plus((ID *)(*mtex)->tex);
    id_lib_extern((ID *)(*mtex)->object);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Slot Copy Operator
 * \{ */

static wmOperatorStatus copy_mtex_exec(bContext *C, wmOperator * /*op*/)
{
  ID *id = CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).owner_id;

  if (id == nullptr) {
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

  return (id != nullptr);
}

void TEXTURE_OT_slot_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Texture Slot Settings";
  ot->idname = "TEXTURE_OT_slot_copy";
  ot->description = "Copy the material texture settings and nodes";

  /* API callbacks. */
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

static wmOperatorStatus paste_mtex_exec(bContext *C, wmOperator * /*op*/)
{
  ID *id = CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).owner_id;

  if (id == nullptr) {
    Material *ma = static_cast<Material *>(
        CTX_data_pointer_get_type(C, "material", &RNA_Material).data);
    Light *la = static_cast<Light *>(CTX_data_pointer_get_type(C, "light", &RNA_Light).data);
    World *wo = static_cast<World *>(CTX_data_pointer_get_type(C, "world", &RNA_World).data);
    ParticleSystem *psys = static_cast<ParticleSystem *>(
        CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem).data);
    FreestyleLineStyle *linestyle = static_cast<FreestyleLineStyle *>(
        CTX_data_pointer_get_type(C, "line_style", &RNA_FreestyleLineStyle).data);

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

    if (id == nullptr) {
      return OPERATOR_CANCELLED;
    }
  }

  paste_mtex_copybuf(id);

  WM_event_add_notifier(C, NC_TEXTURE | ND_SHADING_LINKS, nullptr);

  return OPERATOR_FINISHED;
}

void TEXTURE_OT_slot_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Texture Slot Settings";
  ot->idname = "TEXTURE_OT_slot_paste";
  ot->description = "Copy the texture settings and nodes";

  /* API callbacks. */
  ot->exec = paste_mtex_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */
