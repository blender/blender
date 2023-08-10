/* SPDX-FileCopyrightText: 2009 Blender Foundation
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
#include "BLI_path_util.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_appdir.h"
#include "BKE_blender_copybuffer.h"
#include "BKE_brush.hh"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_lightprobe.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.hh"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_vfont.h"
#include "BKE_workspace.h"
#include "BKE_world.h"

#include "NOD_composite.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

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

#include "RNA_define.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "engines/eevee/eevee_lightcache.h"
#include "engines/eevee_next/eevee_lightcache.hh"

#include "render_intern.hh" /* own include */

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

static Object **object_array_for_shading_edit_mode_enabled(bContext *C, uint *r_objects_len)
{
  return ED_object_array_in_mode_or_selected(
      C, object_array_for_shading_edit_mode_enabled_filter, C, r_objects_len);
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
  const ID *data = static_cast<ID *>(ob->data);
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

static int material_slot_add_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_context(C);

  if (!ob) {
    return OPERATOR_CANCELLED;
  }

  BKE_object_material_slot_add(bmain, ob);

  if (ob->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(scene, ob, nullptr, nullptr, nullptr, nullptr);
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

  /* Removing material slots in edit mode screws things up, see bug #21822. */
  if (ob == CTX_data_edit_object(C)) {
    BKE_report(op->reports, RPT_ERROR, "Unable to remove material slot in edit mode");
    return OPERATOR_CANCELLED;
  }

  BKE_object_material_slot_remove(CTX_data_main(C), ob);

  if (ob->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(scene, ob, nullptr, nullptr, nullptr, nullptr);
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

static int material_slot_assign_exec(bContext *C, wmOperator * /*op*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  bool changed_multi = false;

  Object *obact = CTX_data_active_object(C);
  const Material *mat_active = obact ? BKE_object_material_get(obact, obact->actcol) : nullptr;

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
  const Material *mat_active = obact ? BKE_object_material_get(obact, obact->actcol) : nullptr;

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

  MEM_freeN(objects);

  return (changed_multi) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int material_slot_select_exec(bContext *C, wmOperator * /*op*/)
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

static int material_slot_deselect_exec(bContext *C, wmOperator * /*op*/)
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

static int material_slot_copy_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_context(C);
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

  slot_remap = static_cast<uint *>(MEM_mallocN(sizeof(uint) * ob->totcol, __func__));

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
  /* Removing material slots in edit mode screws things up, see bug #21822. */
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
  MEM_freeN(objects);

  if (!removed) {
    return OPERATOR_CANCELLED;
  }

  BKE_reportf(op->reports, RPT_INFO, "Removed %d slots", removed);

  if (ob_active->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(scene, ob_active, nullptr, nullptr, nullptr, nullptr);
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

static int new_material_exec(bContext *C, wmOperator * /*op*/)
{
  Material *ma = static_cast<Material *>(
      CTX_data_pointer_get_type(C, "material", &RNA_Material).data);
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr, idptr;
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
    if (!(ob != nullptr && ob->type == OB_GPENCIL_LEGACY)) {
      ma = BKE_material_add(bmain, name);
    }
    else {
      ma = BKE_gpencil_material_add(bmain, name);
    }
    ED_node_shader_default(C, &ma->id);
    ma->use_nodes = true;
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

    RNA_id_pointer_create(&ma->id, &idptr);
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

static int new_texture_exec(bContext *C, wmOperator * /*op*/)
{
  Tex *tex = static_cast<Tex *>(CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data);
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
    RNA_property_pointer_set(&ptr, prop, idptr, nullptr);
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

static int new_world_exec(bContext *C, wmOperator * /*op*/)
{
  World *wo = static_cast<World *>(CTX_data_pointer_get_type(C, "world", &RNA_World).data);
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr, idptr;
  PropertyRNA *prop;

  /* add or copy world */
  if (wo) {
    World *new_wo = (World *)BKE_id_copy_ex(
        bmain, &wo->id, nullptr, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS);
    wo = new_wo;
  }
  else {
    wo = BKE_world_add(bmain, CTX_DATA_(BLT_I18NCONTEXT_ID_WORLD, "World"));
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

static int view_layer_remove_exec(bContext *C, wmOperator * /*op*/)
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

static int view_layer_add_aov_exec(bContext *C, wmOperator * /*op*/)
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

  if (scene->nodetree) {
    ntreeCompositUpdateRLayers(scene->nodetree);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
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

static int view_layer_remove_aov_exec(bContext *C, wmOperator * /*op*/)
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

  if (scene->nodetree) {
    ntreeCompositUpdateRLayers(scene->nodetree);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
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
/** \name View Layer Add Lightgroup Operator
 * \{ */

static int view_layer_add_lightgroup_exec(bContext *C, wmOperator *op)
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

  if (scene->nodetree) {
    ntreeCompositUpdateRLayers(scene->nodetree);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
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

  /* api callbacks */
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

static int view_layer_remove_lightgroup_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (view_layer->active_lightgroup == nullptr) {
    return OPERATOR_FINISHED;
  }

  BKE_view_layer_remove_lightgroup(view_layer, view_layer->active_lightgroup);

  if (scene->nodetree) {
    ntreeCompositUpdateRLayers(scene->nodetree);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
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

  /* api callbacks */
  ot->exec = view_layer_remove_lightgroup_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Add Used Lightgroups Operator
 * \{ */

static GSet *get_used_lightgroups(Scene *scene)
{
  GSet *used_lightgroups = BLI_gset_str_new(__func__);

  FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
    if (ob->lightgroup && ob->lightgroup->name[0]) {
      BLI_gset_add(used_lightgroups, ob->lightgroup->name);
    }
  }
  FOREACH_SCENE_OBJECT_END;

  if (scene->world && scene->world->lightgroup && scene->world->lightgroup->name[0]) {
    BLI_gset_add(used_lightgroups, scene->world->lightgroup->name);
  }

  return used_lightgroups;
}

static int view_layer_add_used_lightgroups_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  GSet *used_lightgroups = get_used_lightgroups(scene);
  GSET_FOREACH_BEGIN (const char *, used_lightgroup, used_lightgroups) {
    if (!BLI_findstring(
            &view_layer->lightgroups, used_lightgroup, offsetof(ViewLayerLightgroup, name)))
    {
      BKE_view_layer_add_lightgroup(view_layer, used_lightgroup);
    }
  }
  GSET_FOREACH_END();
  BLI_gset_free(used_lightgroups, nullptr);

  if (scene->nodetree) {
    ntreeCompositUpdateRLayers(scene->nodetree);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
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

  /* api callbacks */
  ot->exec = view_layer_add_used_lightgroups_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Remove Unused Lightgroups Operator
 * \{ */

static int view_layer_remove_unused_lightgroups_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  GSet *used_lightgroups = get_used_lightgroups(scene);
  LISTBASE_FOREACH_MUTABLE (ViewLayerLightgroup *, lightgroup, &view_layer->lightgroups) {
    if (!BLI_gset_haskey(used_lightgroups, lightgroup->name)) {
      BKE_view_layer_remove_lightgroup(view_layer, lightgroup);
    }
  }
  BLI_gset_free(used_lightgroups, nullptr);

  if (scene->nodetree) {
    ntreeCompositUpdateRLayers(scene->nodetree);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
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

  /* api callbacks */
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
  LIGHTCACHE_SUBSET_DIRTY,
  LIGHTCACHE_SUBSET_CUBE,
  LIGHTCACHE_SUBSET_SELECTED,
  LIGHTCACHE_SUBSET_ACTIVE,
};

static void light_cache_bake_tag_cache(Scene *scene, wmOperator *op)
{
  if (scene->eevee.light_cache_data != nullptr) {
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
      default:
        BLI_assert_unreachable();
        break;
    }
  }
}

/** Catch escape key to cancel. */
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

  /* TODO: abort if selected engine is not eevee. */
  void *rj = EEVEE_lightbake_job_data_alloc(bmain, view_layer, scene, false, scene->r.cfra);

  light_cache_bake_tag_cache(scene, op);

  bool stop = false, do_update;
  float progress; /* Not actually used. */
  /* Do the job. */
  EEVEE_lightbake_job(rj, &stop, &do_update, &progress);
  /* Free baking data. Result is already stored in the scene data. */
  EEVEE_lightbake_job_data_free(rj);

  /* No redraw needed, we leave state as we entered it. */
  ED_update_for_newframe(bmain, CTX_data_depsgraph_pointer(C));

  WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);

  return OPERATOR_FINISHED;
}

static int light_cache_bake_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
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
   * several layers from compositor #31800. */
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
      {0, nullptr, 0, nullptr, nullptr},
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

/* NOTE: New version destined to replace the old lightcache bake operator. */

static blender::Vector<Object *> lightprobe_cache_irradiance_volume_subset_get(bContext *C,
                                                                               wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);

  auto is_irradiance_volume = [](Object *ob) -> bool {
    return ob->type == OB_LIGHTPROBE &&
           static_cast<LightProbe *>(ob->data)->type == LIGHTPROBE_TYPE_GRID;
  };

  blender::Vector<Object *> probes;

  auto irradiance_volume_setup = [&](Object *ob) {
    BKE_lightprobe_cache_free(ob);
    BKE_lightprobe_cache_create(ob);
    DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
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
    case LIGHTCACHE_SUBSET_DIRTY: {
      FOREACH_OBJECT_BEGIN (scene, view_layer, ob) {
        if (is_irradiance_volume(ob) && ob->lightprobe_cache && ob->lightprobe_cache->dirty) {
          irradiance_volume_setup(ob);
        }
      }
      FOREACH_OBJECT_END;
      break;
    }
    case LIGHTCACHE_SUBSET_SELECTED: {
      uint objects_len = 0;
      ObjectsInViewLayerParams parameters;
      parameters.filter_fn = nullptr;
      parameters.no_dup_data = true;
      Object **objects = BKE_view_layer_array_selected_objects_params(
          view_layer, nullptr, &objects_len, &parameters);
      for (Object *ob : blender::MutableSpan<Object *>(objects, objects_len)) {
        if (is_irradiance_volume(ob)) {
          irradiance_volume_setup(ob);
        }
      }
      MEM_freeN(objects);
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

static int lightprobe_cache_bake_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  int delay = RNA_int_get(op->ptr, "delay");

  blender::Vector<Object *> probes = lightprobe_cache_irradiance_volume_subset_get(C, op);

  if (probes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  wmJob *wm_job = EEVEE_NEXT_lightbake_job_create(
      wm, win, bmain, view_layer, scene, probes, scene->r.cfra, delay);

  WM_event_add_modal_handler(C, op);

  /* Store actual owner of job, so modal operator could check for it,
   * the reason of this is that active scene could change when rendering
   * several layers from compositor #31800. */
  op->customdata = scene;

  WM_jobs_start(wm, wm_job);

  WM_cursor_wait(false);

  return OPERATOR_RUNNING_MODAL;
}

static int lightprobe_cache_bake_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = (Scene *)op->customdata;

  /* No running bake, remove handler and pass through. */
  if (0 == WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_LIGHT_BAKE)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* Running bake. */
  switch (event->type) {
    case EVT_ESCKEY:
      return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_PASS_THROUGH;
}

static void lightprobe_cache_bake_cancel(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  Scene *scene = (Scene *)op->customdata;

  /* Kill on cancel, because job is using op->reports. */
  WM_jobs_kill_type(wm, scene, WM_JOB_TYPE_LIGHT_BAKE);
}

/* Executes blocking bake. */
static int lightprobe_cache_bake_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  blender::Vector<Object *> probes = lightprobe_cache_irradiance_volume_subset_get(C, op);

  /* TODO: abort if selected engine is not eevee. */
  void *rj = EEVEE_NEXT_lightbake_job_data_alloc(bmain, view_layer, scene, probes, scene->r.cfra);
  /* Do the job. */
  EEVEE_NEXT_lightbake_job(rj, nullptr, nullptr, nullptr);
  /* Free baking data. Result is already stored in the scene data. */
  EEVEE_NEXT_lightbake_job_data_free(rj);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_lightprobe_cache_bake(wmOperatorType *ot)
{
  static const EnumPropertyItem light_cache_subset_items[] = {
      {LIGHTCACHE_SUBSET_ALL, "ALL", 0, "All Light Probes", "Bake all light probes"},
      {LIGHTCACHE_SUBSET_DIRTY,
       "DIRTY",
       0,
       "Dirty Only",
       "Only bake light probes that are marked as dirty"},
      {LIGHTCACHE_SUBSET_SELECTED,
       "SELECTED",
       0,
       "Selected Only",
       "Only bake selected light probes"},
      {LIGHTCACHE_SUBSET_ACTIVE, "ACTIVE", 0, "Active Only", "Only bake the active light probe"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Bake Light Cache";
  ot->idname = "OBJECT_OT_lightprobe_cache_bake";
  ot->description = "Bake irradiance volume light cache";

  /* api callbacks */
  ot->invoke = lightprobe_cache_bake_invoke;
  ot->modal = lightprobe_cache_bake_modal;
  ot->cancel = lightprobe_cache_bake_cancel;
  ot->exec = lightprobe_cache_bake_exec;

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

static int light_cache_free_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  /* kill potential bake job first (see #57011) */
  wmWindowManager *wm = CTX_wm_manager(C);
  WM_jobs_kill_type(wm, scene, WM_JOB_TYPE_LIGHT_BAKE);

  if (!scene->eevee.light_cache_data) {
    return OPERATOR_CANCELLED;
  }

  EEVEE_lightcache_free(scene->eevee.light_cache_data);
  scene->eevee.light_cache_data = nullptr;

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

/* NOTE: New version destined to replace the old lightcache bake operator. */

static int lightprobe_cache_free_exec(bContext *C, wmOperator *op)
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
    DEG_id_tag_update(&object->id, ID_RECALC_COPY_ON_WRITE);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, scene);

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

  /* api callbacks */
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

static int render_view_add_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  BKE_scene_add_render_view(scene, nullptr);
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

static int render_view_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  SceneRenderView *rv = static_cast<SceneRenderView *>(
      BLI_findlink(&scene->r.views, scene->r.actview));

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
  FreestyleModuleConfig *module = static_cast<FreestyleModuleConfig *>(ptr.data);

  return module != nullptr;
}

static int freestyle_module_add_exec(bContext *C, wmOperator * /*op*/)
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

static int freestyle_module_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "freestyle_module", &RNA_FreestyleModuleSettings);
  FreestyleModuleConfig *module = static_cast<FreestyleModuleConfig *>(ptr.data);

  BKE_freestyle_module_delete(&view_layer->freestyle_config, module);

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
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
  FreestyleModuleConfig *module = static_cast<FreestyleModuleConfig *>(ptr.data);
  int dir = RNA_enum_get(op->ptr, "direction");

  if (BKE_freestyle_module_move(&view_layer->freestyle_config, module, dir)) {
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
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

static int freestyle_lineset_add_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  BKE_freestyle_lineset_add(bmain, &view_layer->freestyle_config, nullptr);

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
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

  return BKE_freestyle_lineset_get_active(&view_layer->freestyle_config) != nullptr;
}

static int freestyle_lineset_copy_exec(bContext *C, wmOperator * /*op*/)
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

static int freestyle_lineset_paste_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  FRS_paste_active_lineset(&view_layer->freestyle_config);

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

  return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Line Set";
  ot->idname = "SCENE_OT_freestyle_lineset_paste";
  ot->description = "Paste the internal clipboard content to the active line set";

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

static int freestyle_lineset_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  FRS_delete_active_lineset(&view_layer->freestyle_config);

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
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
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
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

/** \} */

#endif /* WITH_FREESTYLE */

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

static int copy_material_exec(bContext *C, wmOperator *op)
{
  Material *ma = static_cast<Material *>(
      CTX_data_pointer_get_type(C, "material", &RNA_Material).data);

  if (ma == nullptr) {
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  Main *bmain = CTX_data_main(C);

  /* Mark is the material to use (others may be expanded). */
  BKE_copybuffer_copy_begin(bmain);

  BKE_copybuffer_copy_tag_ID(&ma->id);

  material_copybuffer_filepath_get(filepath, sizeof(filepath));
  BKE_copybuffer_copy_end(bmain, filepath, op->reports);

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

static int paste_material_exec(bContext *C, wmOperator *op)
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
    if (ma_iter->id.flag & LIB_CLIPBOARD_MARK) {
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

    ntreeFreeEmbeddedTree(nodetree);
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
  SWAP_MEMBER(use_nodes);
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
      *mtex = MEM_new<MTex>("mtex copy");
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

static int copy_mtex_exec(bContext *C, wmOperator * /*op*/)
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

static int paste_mtex_exec(bContext *C, wmOperator * /*op*/)
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

  /* api callbacks */
  ot->exec = paste_mtex_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */
