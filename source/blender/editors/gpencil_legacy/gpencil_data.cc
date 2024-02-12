/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 *
 * Operators for dealing with GP data-blocks and layers.
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_fcurve_driver.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_object.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "gpencil_intern.h"

/* ************************************************ */
/* Datablock Operators */

/* ******************* Add New Data ************************ */
static bool gpencil_data_add_poll(bContext *C)
{

  /* the base line we have is that we have somewhere to add Grease Pencil data */
  return ED_annotation_data_get_pointers(C, nullptr) != nullptr;
}

/* add new datablock - wrapper around API */
static int gpencil_data_add_exec(bContext *C, wmOperator *op)
{
  PointerRNA gpd_owner = {nullptr};
  bGPdata **gpd_ptr = ED_annotation_data_get_pointers(C, &gpd_owner);

  if (gpd_ptr == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
    return OPERATOR_CANCELLED;
  }

  /* decrement user count and add new datablock */
  /* TODO: if a datablock exists,
   * we should make a copy of it instead of starting fresh (as in other areas) */
  Main *bmain = CTX_data_main(C);

  /* decrement user count of old GP datablock */
  if (*gpd_ptr) {
    bGPdata *gpd = (*gpd_ptr);
    id_us_min(&gpd->id);
  }

  /* Add new datablock, with a single layer ready to use
   * (so users don't have to perform an extra step). */
  bGPdata *gpd = BKE_gpencil_data_addnew(bmain, DATA_("Annotations"));
  *gpd_ptr = gpd;

  /* tag for annotations */
  gpd->flag |= GP_DATA_ANNOTATIONS;

  /* add new layer (i.e. a "note") */
  BKE_gpencil_layer_addnew(*gpd_ptr, DATA_("Note"), true, false);

  /* notifiers */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_annotation_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Annotation Add New";
  ot->idname = "GPENCIL_OT_annotation_add";
  ot->description = "Add new Annotation data-block";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_data_add_exec;
  ot->poll = gpencil_data_add_poll;
}

/* ******************* Unlink Data ************************ */

/* poll callback for adding data/layers - special */
static bool gpencil_data_unlink_poll(bContext *C)
{
  bGPdata **gpd_ptr = ED_annotation_data_get_pointers(C, nullptr);

  /* only unlink annotation datablocks */
  if ((gpd_ptr != nullptr) && (*gpd_ptr != nullptr)) {
    bGPdata *gpd = (*gpd_ptr);
    if ((gpd->flag & GP_DATA_ANNOTATIONS) == 0) {
      return false;
    }
  }
  /* if we have access to some active data, make sure there's a datablock before enabling this */
  return (gpd_ptr && *gpd_ptr);
}

/* unlink datablock - wrapper around API */
static int gpencil_data_unlink_exec(bContext *C, wmOperator *op)
{
  bGPdata **gpd_ptr = ED_annotation_data_get_pointers(C, nullptr);

  if (gpd_ptr == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
    return OPERATOR_CANCELLED;
  }
  /* just unlink datablock now, decreasing its user count */
  bGPdata *gpd = (*gpd_ptr);

  id_us_min(&gpd->id);
  *gpd_ptr = nullptr;

  /* notifiers */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_data_unlink(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Annotation Unlink";
  ot->idname = "GPENCIL_OT_data_unlink";
  ot->description = "Unlink active Annotation data-block";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_data_unlink_exec;
  ot->poll = gpencil_data_unlink_poll;
}

/* ************************************************ */
/* Layer Operators */

/* ******************* Add New Layer ************************ */

/* add new layer - wrapper around API */
static int gpencil_layer_add_exec(bContext *C, wmOperator *op)
{
  const bool is_annotation = STREQ(op->idname, "GPENCIL_OT_layer_annotation_add");

  PointerRNA gpd_owner = {nullptr};
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  bGPdata *gpd = nullptr;

  if (is_annotation) {
    bGPdata **gpd_ptr = ED_annotation_data_get_pointers(C, &gpd_owner);
    /* if there's no existing Grease-Pencil data there, add some */
    if (gpd_ptr == nullptr) {
      BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
      return OPERATOR_CANCELLED;
    }
    /* Annotations */
    if (*gpd_ptr == nullptr) {
      *gpd_ptr = BKE_gpencil_data_addnew(bmain, DATA_("Annotations"));
    }

    /* mark as annotation */
    (*gpd_ptr)->flag |= GP_DATA_ANNOTATIONS;
    BKE_gpencil_layer_addnew(*gpd_ptr, DATA_("Note"), true, false);
    gpd = *gpd_ptr;
  }
  else {
    /* GP Object */
    Object *ob = CTX_data_active_object(C);
    if ((ob != nullptr) && (ob->type == OB_GPENCIL_LEGACY)) {
      gpd = (bGPdata *)ob->data;
      PropertyRNA *prop;
      char name[128];
      prop = RNA_struct_find_property(op->ptr, "new_layer_name");
      if (RNA_property_is_set(op->ptr, prop)) {
        RNA_property_string_get(op->ptr, prop, name);
      }
      else {
        STRNCPY(name, "GP_Layer");
      }
      bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, name, true, false);

      /* Add a new frame to make it visible in Dopesheet. */
      if (gpl != nullptr) {
        gpl->actframe = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, GP_GETFRAME_ADD_NEW);
      }
    }
  }

  /* notifiers */
  if (gpd) {
    DEG_id_tag_update(&gpd->id,
                      ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  }
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}
static int gpencil_layer_add_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  const int tmp = ED_gpencil_new_layer_dialog(C, op);
  if (tmp != 0) {
    return tmp;
  }
  return gpencil_layer_add_exec(C, op);
}
void GPENCIL_OT_layer_add(wmOperatorType *ot)
{
  PropertyRNA *prop;
  /* identifiers */
  ot->name = "Add New Layer";
  ot->idname = "GPENCIL_OT_layer_add";
  ot->description = "Add new layer or note for the active data-block";

  /* callbacks */
  ot->exec = gpencil_layer_add_exec;
  ot->invoke = gpencil_layer_add_invoke;
  ot->poll = gpencil_add_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  prop = RNA_def_int(ot->srna, "layer", 0, -1, INT_MAX, "Grease Pencil Layer", "", -1, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_string(
      ot->srna, "new_layer_name", nullptr, MAX_NAME, "Name", "Name of the newly added layer");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

static bool gpencil_add_annotation_poll(bContext *C)
{
  return ED_annotation_data_get_pointers(C, nullptr) != nullptr;
}

void GPENCIL_OT_layer_annotation_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add New Annotation Layer";
  ot->idname = "GPENCIL_OT_layer_annotation_add";
  ot->description = "Add new Annotation layer or note for the active data-block";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_layer_add_exec;
  ot->poll = gpencil_add_annotation_poll;
}
/* ******************* Remove Active Layer ************************* */

static int gpencil_layer_remove_exec(bContext *C, wmOperator *op)
{
  const bool is_annotation = STREQ(op->idname, "GPENCIL_OT_layer_annotation_remove");

  bGPdata *gpd = (!is_annotation) ? ED_gpencil_data_get_active(C) :
                                    ED_annotation_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  /* sanity checks */
  if (ELEM(nullptr, gpd, gpl)) {
    return OPERATOR_CANCELLED;
  }

  if (gpl->flag & GP_LAYER_LOCKED) {
    BKE_report(op->reports, RPT_ERROR, "Cannot delete locked layers");
    return OPERATOR_CANCELLED;
  }

  /* make the layer before this the new active layer
   * - use the one after if this is the first
   * - if this is the only layer, this naturally becomes nullptr
   */
  if (gpl->prev) {
    BKE_gpencil_layer_active_set(gpd, gpl->prev);
  }
  else {
    BKE_gpencil_layer_active_set(gpd, gpl->next);
  }

  /* delete the layer now... */
  BKE_gpencil_layer_delete(gpd, gpl);

  /* Reorder masking. */
  BKE_gpencil_layer_mask_sort_all(gpd);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  /* Free Grease Pencil data block when last annotation layer is removed, see: #112683. */
  if (is_annotation && gpd->layers.first == nullptr) {
    BKE_gpencil_free_data(gpd, true);

    bGPdata **gpd_ptr = ED_annotation_data_get_pointers(C, nullptr);
    *gpd_ptr = nullptr;

    Main *bmain = CTX_data_main(C);
    BKE_id_free_us(bmain, gpd);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Layer";
  ot->idname = "GPENCIL_OT_layer_remove";
  ot->description = "Remove active Grease Pencil layer";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_layer_remove_exec;
  ot->poll = gpencil_active_layer_poll;
}

static bool gpencil_active_layer_annotation_poll(bContext *C)
{
  bGPdata *gpd = ED_annotation_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  return (gpl != nullptr);
}

void GPENCIL_OT_layer_annotation_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Annotation Layer";
  ot->idname = "GPENCIL_OT_layer_annotation_remove";
  ot->description = "Remove active Annotation layer";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_layer_remove_exec;
  ot->poll = gpencil_active_layer_annotation_poll;
}
/* ******************* Move Layer Up/Down ************************** */

enum {
  GP_LAYER_MOVE_UP = -1,
  GP_LAYER_MOVE_DOWN = 1,
};

static int gpencil_layer_move_exec(bContext *C, wmOperator *op)
{
  const bool is_annotation = STREQ(op->idname, "GPENCIL_OT_layer_annotation_move");

  bGPdata *gpd = (!is_annotation) ? ED_gpencil_data_get_active(C) :
                                    ED_annotation_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  const int direction = RNA_enum_get(op->ptr, "type") * -1;

  /* sanity checks */
  if (ELEM(nullptr, gpd, gpl)) {
    return OPERATOR_CANCELLED;
  }

  BLI_assert(ELEM(direction, -1, 0, 1)); /* we use value below */
  if (BLI_listbase_link_move(&gpd->layers, gpl, direction)) {
    /* Reorder masking. */
    BKE_gpencil_layer_mask_sort_all(gpd);

    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_move(wmOperatorType *ot)
{
  static const EnumPropertyItem slot_move[] = {
      {GP_LAYER_MOVE_UP, "UP", 0, "Up", ""},
      {GP_LAYER_MOVE_DOWN, "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Grease Pencil Layer";
  ot->idname = "GPENCIL_OT_layer_move";
  ot->description = "Move the active Grease Pencil layer up/down in the list";

  /* api callbacks */
  ot->exec = gpencil_layer_move_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

void GPENCIL_OT_layer_annotation_move(wmOperatorType *ot)
{
  static const EnumPropertyItem slot_move[] = {
      {GP_LAYER_MOVE_UP, "UP", 0, "Up", ""},
      {GP_LAYER_MOVE_DOWN, "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Annotation Layer";
  ot->idname = "GPENCIL_OT_layer_annotation_move";
  ot->description = "Move the active Annotation layer up/down in the list";

  /* api callbacks */
  ot->exec = gpencil_layer_move_exec;
  ot->poll = gpencil_active_layer_annotation_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}
/* ********************* Duplicate Layer ************************** */
enum {
  GP_LAYER_DUPLICATE_ALL = 0,
  GP_LAYER_DUPLICATE_EMPTY = 1,
};

static int gpencil_layer_copy_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
  bGPDlayer *new_layer;
  const int mode = RNA_enum_get(op->ptr, "mode");
  const bool dup_strokes = bool(mode == GP_LAYER_DUPLICATE_ALL);
  /* sanity checks */
  if (ELEM(nullptr, gpd, gpl)) {
    return OPERATOR_CANCELLED;
  }

  /* Make copy of layer, and add it immediately after or before the existing layer. */
  new_layer = BKE_gpencil_layer_duplicate(gpl, true, dup_strokes);
  if (dup_strokes) {
    BLI_insertlinkafter(&gpd->layers, gpl, new_layer);
  }
  else {
    /* For empty strokes is better add below. */
    BLI_insertlinkbefore(&gpd->layers, gpl, new_layer);
  }

  /* ensure new layer has a unique name, and is now the active layer */
  BLI_uniquename(&gpd->layers,
                 new_layer,
                 DATA_("GP_Layer"),
                 '.',
                 offsetof(bGPDlayer, info),
                 sizeof(new_layer->info));
  BKE_gpencil_layer_active_set(gpd, new_layer);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_duplicate(wmOperatorType *ot)
{
  static const EnumPropertyItem copy_mode[] = {
      {GP_LAYER_DUPLICATE_ALL, "ALL", 0, "All Data", ""},
      {GP_LAYER_DUPLICATE_EMPTY, "EMPTY", 0, "Empty Keyframes", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Duplicate Layer";
  ot->idname = "GPENCIL_OT_layer_duplicate";
  ot->description = "Make a copy of the active Grease Pencil layer";

  /* callbacks */
  ot->exec = gpencil_layer_copy_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "mode", copy_mode, GP_LAYER_DUPLICATE_ALL, "Mode", "");
}

/* ********************* Duplicate Layer in a new object ************************** */
enum {
  GP_LAYER_COPY_OBJECT_ALL_FRAME = 0,
  GP_LAYER_COPY_OBJECT_ACT_FRAME = 1,
};

static bool gpencil_layer_duplicate_object_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }

  bGPdata *gpd = (bGPdata *)ob->data;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  if (gpl == nullptr) {
    return false;
  }

  return true;
}

static int gpencil_layer_duplicate_object_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const bool only_active = RNA_boolean_get(op->ptr, "only_active");
  const int mode = RNA_enum_get(op->ptr, "mode");

  Object *ob_src = CTX_data_active_object(C);
  bGPdata *gpd_src = (bGPdata *)ob_src->data;
  bGPDlayer *gpl_active = BKE_gpencil_layer_active_get(gpd_src);

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if ((ob == ob_src) || (ob->type != OB_GPENCIL_LEGACY)) {
      continue;
    }
    bGPdata *gpd_dst = (bGPdata *)ob->data;
    LISTBASE_FOREACH_BACKWARD (bGPDlayer *, gpl_src, &gpd_src->layers) {
      if ((only_active) && (gpl_src != gpl_active)) {
        continue;
      }
      /* Create new layer (adding at head of the list). */
      bGPDlayer *gpl_dst = BKE_gpencil_layer_addnew(gpd_dst, gpl_src->info, true, true);
      /* Need to copy some variables (not all). */
      gpl_dst->onion_flag = gpl_src->onion_flag;
      gpl_dst->thickness = gpl_src->thickness;
      gpl_dst->line_change = gpl_src->line_change;
      copy_v4_v4(gpl_dst->tintcolor, gpl_src->tintcolor);
      gpl_dst->opacity = gpl_src->opacity;

      /* Create all frames. */
      LISTBASE_FOREACH (bGPDframe *, gpf_src, &gpl_src->frames) {

        if ((mode == GP_LAYER_COPY_OBJECT_ACT_FRAME) && (gpf_src != gpl_src->actframe)) {
          continue;
        }

        /* Create new frame. */
        bGPDframe *gpf_dst = BKE_gpencil_frame_addnew(gpl_dst, gpf_src->framenum);

        /* Copy strokes. */
        LISTBASE_FOREACH (bGPDstroke *, gps_src, &gpf_src->strokes) {

          /* Make copy of source stroke. */
          bGPDstroke *gps_dst = BKE_gpencil_stroke_duplicate(gps_src, true, true);

          /* Check if material is in destination object,
           * otherwise add the slot with the material. */
          Material *ma_src = BKE_object_material_get(ob_src, gps_src->mat_nr + 1);
          if (ma_src != nullptr) {
            int idx = BKE_gpencil_object_material_ensure(bmain, ob, ma_src);

            /* Reassign the stroke material to the right slot in destination object. */
            gps_dst->mat_nr = idx;
          }

          /* Add new stroke to frame. */
          BLI_addtail(&gpf_dst->strokes, gps_dst);
        }
      }
    }
    /* notifiers */
    DEG_id_tag_update(&gpd_dst->id,
                      ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
    DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
  }
  CTX_DATA_END;

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_duplicate_object(wmOperatorType *ot)
{
  PropertyRNA *prop;

  static const EnumPropertyItem copy_mode[] = {
      {GP_LAYER_COPY_OBJECT_ALL_FRAME, "ALL", 0, "All Frames", ""},
      {GP_LAYER_COPY_OBJECT_ACT_FRAME, "ACTIVE", 0, "Active Frame", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Duplicate Layer to New Object";
  ot->idname = "GPENCIL_OT_layer_duplicate_object";
  ot->description = "Make a copy of the active Grease Pencil layer to selected object";

  /* callbacks */
  ot->exec = gpencil_layer_duplicate_object_exec;
  ot->poll = gpencil_layer_duplicate_object_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "mode", copy_mode, GP_LAYER_COPY_OBJECT_ALL_FRAME, "Mode", "");

  prop = RNA_def_boolean(ot->srna,
                         "only_active",
                         true,
                         "Only Active",
                         "Copy only active Layer, uncheck to append all layers");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* ********************* Duplicate Frame ************************** */
enum {
  GP_FRAME_DUP_ACTIVE = 0,
  GP_FRAME_DUP_ALL = 1,
};

static int gpencil_frame_duplicate_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *gpl_active = BKE_gpencil_layer_active_get(gpd);
  Scene *scene = CTX_data_scene(C);

  int mode = RNA_enum_get(op->ptr, "mode");

  /* sanity checks */
  if (ELEM(nullptr, gpd, gpl_active)) {
    return OPERATOR_CANCELLED;
  }

  if (mode == 0) {
    BKE_gpencil_frame_addcopy(gpl_active, scene->r.cfra);
  }
  else {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      if ((gpl->flag & GP_LAYER_LOCKED) == 0) {
        BKE_gpencil_frame_addcopy(gpl, scene->r.cfra);
      }
    }
  }
  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_frame_duplicate(wmOperatorType *ot)
{
  static const EnumPropertyItem duplicate_mode[] = {
      {GP_FRAME_DUP_ACTIVE, "ACTIVE", 0, "Active", "Duplicate frame in active layer only"},
      {GP_FRAME_DUP_ALL, "ALL", 0, "All", "Duplicate active frames in all layers"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Duplicate Frame";
  ot->idname = "GPENCIL_OT_frame_duplicate";
  ot->description = "Make a copy of the active Grease Pencil Frame";

  /* callbacks */
  ot->exec = gpencil_frame_duplicate_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "mode", duplicate_mode, GP_FRAME_DUP_ACTIVE, "Mode", "");
}

/* ********************* Clean Fill Boundaries on Frame ************************** */
enum {
  GP_FRAME_CLEAN_FILL_ACTIVE = 0,
  GP_FRAME_CLEAN_FILL_ALL = 1,
};

static int gpencil_frame_clean_fill_exec(bContext *C, wmOperator *op)
{
  bool changed = false;
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  int mode = RNA_enum_get(op->ptr, "mode");

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = gpl->actframe;
    if (mode == GP_FRAME_CLEAN_FILL_ALL) {
      init_gpf = static_cast<bGPDframe *>(gpl->frames.first);
    }

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || (mode == GP_FRAME_CLEAN_FILL_ALL)) {

        if (gpf == nullptr) {
          continue;
        }

        /* simply delete strokes which are no fill */
        LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          /* free stroke */
          if (gps->flag & GP_STROKE_NOFILL) {
            /* free stroke memory arrays, then stroke itself */
            if (gps->points) {
              MEM_freeN(gps->points);
            }
            if (gps->dvert) {
              BKE_gpencil_free_stroke_weights(gps);
              MEM_freeN(gps->dvert);
            }
            MEM_SAFE_FREE(gps->triangles);
            BLI_freelinkN(&gpf->strokes, gps);

            changed = true;
          }
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_frame_clean_fill(wmOperatorType *ot)
{
  static const EnumPropertyItem duplicate_mode[] = {
      {GP_FRAME_CLEAN_FILL_ACTIVE, "ACTIVE", 0, "Active Frame Only", "Clean active frame only"},
      {GP_FRAME_CLEAN_FILL_ALL, "ALL", 0, "All Frames", "Clean all frames in all layers"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Clean Fill Boundaries";
  ot->idname = "GPENCIL_OT_frame_clean_fill";
  ot->description = "Remove 'no fill' boundary strokes";

  /* callbacks */
  ot->exec = gpencil_frame_clean_fill_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "mode", duplicate_mode, GP_FRAME_DUP_ACTIVE, "Mode", "");
}

/* ********************* Clean Loose Boundaries on Frame ************************** */
static int gpencil_frame_clean_loose_exec(bContext *C, wmOperator *op)
{
  bool changed = false;
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  int limit = RNA_int_get(op->ptr, "limit");
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        /* simply delete strokes which are no loose */
        LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          /* free stroke */
          if (gps->totpoints <= limit) {
            /* free stroke memory arrays, then stroke itself */
            if (gps->points) {
              MEM_freeN(gps->points);
            }
            if (gps->dvert) {
              BKE_gpencil_free_stroke_weights(gps);
              MEM_freeN(gps->dvert);
            }
            MEM_SAFE_FREE(gps->triangles);
            BLI_freelinkN(&gpf->strokes, gps);

            changed = true;
          }
        }
      }

      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_frame_clean_loose(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clean Loose Points";
  ot->idname = "GPENCIL_OT_frame_clean_loose";
  ot->description = "Remove loose points";

  /* callbacks */
  ot->exec = gpencil_frame_clean_loose_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "limit",
              1,
              1,
              INT_MAX,
              "Limit",
              "Number of points to consider stroke as loose",
              1,
              INT_MAX);
}

/* ********************* Clean Duplicate Frames ************************** */
static bool gpencil_frame_is_equal(const bGPDframe *gpf_a, const bGPDframe *gpf_b)
{
  if ((gpf_a == nullptr) || (gpf_b == nullptr)) {
    return false;
  }
  /* If the number of strokes is different, cannot be equal. */
  const int totstrokes_a = BLI_listbase_count(&gpf_a->strokes);
  const int totstrokes_b = BLI_listbase_count(&gpf_b->strokes);
  if ((totstrokes_a == 0) || (totstrokes_b == 0) || (totstrokes_a != totstrokes_b)) {
    return false;
  }
  /* Loop all strokes and check. */
  const bGPDstroke *gps_a = static_cast<const bGPDstroke *>(gpf_a->strokes.first);
  const bGPDstroke *gps_b = static_cast<const bGPDstroke *>(gpf_b->strokes.first);
  for (int i = 0; i < totstrokes_a; i++) {
    /* If the number of points is different, cannot be equal. */
    if (gps_a->totpoints != gps_b->totpoints) {
      return false;
    }
    /* Check other variables. */
    if (!equals_v4v4(gps_a->vert_color_fill, gps_b->vert_color_fill)) {
      return false;
    }
    if (gps_a->thickness != gps_b->thickness) {
      return false;
    }
    if (gps_a->mat_nr != gps_b->mat_nr) {
      return false;
    }
    if (gps_a->caps[0] != gps_b->caps[0]) {
      return false;
    }
    if (gps_a->caps[1] != gps_b->caps[1]) {
      return false;
    }
    if (gps_a->hardness != gps_b->hardness) {
      return false;
    }
    if (!equals_v2v2(gps_a->aspect_ratio, gps_b->aspect_ratio)) {
      return false;
    }
    if (gps_a->uv_rotation != gps_b->uv_rotation) {
      return false;
    }
    if (!equals_v2v2(gps_a->uv_translation, gps_b->uv_translation)) {
      return false;
    }
    if (gps_a->uv_scale != gps_b->uv_scale) {
      return false;
    }

    /* Loop points and check if equals or not. */
    for (int p = 0; p < gps_a->totpoints; p++) {
      const bGPDspoint *pt_a = &gps_a->points[p];
      const bGPDspoint *pt_b = &gps_b->points[p];
      if (!equals_v3v3(&pt_a->x, &pt_b->x)) {
        return false;
      }
      if (pt_a->pressure != pt_b->pressure) {
        return false;
      }
      if (pt_a->strength != pt_b->strength) {
        return false;
      }
      if (pt_a->uv_fac != pt_b->uv_fac) {
        return false;
      }
      if (pt_a->uv_rot != pt_b->uv_rot) {
        return false;
      }
      if (!equals_v4v4(pt_a->vert_color, pt_b->vert_color)) {
        return false;
      }
    }

    /* Look at next pair of strokes. */
    gps_a = gps_a->next;
    gps_b = gps_b->next;
  }

  return true;
}

static int gpencil_frame_clean_duplicate_exec(bContext *C, wmOperator *op)
{
#define SELECTED 1

  bool changed = false;
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  const int type = RNA_enum_get(op->ptr, "type");

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* Only editable and visible layers are considered. */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->frames.first != nullptr)) {
      bGPDframe *gpf = static_cast<bGPDframe *>(gpl->frames.first);

      if ((type == SELECTED) && ((gpf->flag & GP_FRAME_SELECT) == 0)) {
        continue;
      }

      while (gpf != nullptr) {
        if (gpencil_frame_is_equal(gpf, gpf->next)) {
          /* Remove frame. */
          BKE_gpencil_layer_frame_delete(gpl, gpf->next);
          /* Tag for recalc. */
          changed = true;
        }
        else {
          gpf = gpf->next;
        }
      }
    }
  }

  /* notifiers */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_frame_clean_duplicate(wmOperatorType *ot)
{
  static const EnumPropertyItem clean_type[] = {
      {0, "ALL", 0, "All Frames", ""},
      {1, "SELECTED", 0, "Selected Frames", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Clean Duplicate Frames";
  ot->idname = "GPENCIL_OT_frame_clean_duplicate";
  ot->description = "Remove duplicate keyframes";

  /* callbacks */
  ot->exec = gpencil_frame_clean_duplicate_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", clean_type, 0, "Type", "");
}

/* *********************** Hide Layers ******************************** */

static int gpencil_hide_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *layer = BKE_gpencil_layer_active_get(gpd);
  bool unselected = RNA_boolean_get(op->ptr, "unselected");

  /* sanity checks */
  if (ELEM(nullptr, gpd, layer)) {
    return OPERATOR_CANCELLED;
  }

  if (unselected) {
    /* hide unselected */
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      if (gpl != layer) {
        gpl->flag |= GP_LAYER_HIDE;
      }
      else {
        /* Be sure the active layer is unhidden. */
        gpl->flag &= ~GP_LAYER_HIDE;
      }
    }
  }
  else {
    /* hide selected/active */
    layer->flag |= GP_LAYER_HIDE;
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Layer(s)";
  ot->idname = "GPENCIL_OT_hide";
  ot->description = "Hide selected/unselected Grease Pencil layers";

  /* callbacks */
  ot->exec = gpencil_hide_exec;
  ot->poll = gpencil_active_layer_poll; /* NOTE: we need an active layer to play with */

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(
      ot->srna, "unselected", false, "Unselected", "Hide unselected rather than selected layers");
}

/* ********************** Show All Layers ***************************** */

/* poll callback for showing layers */
static bool gpencil_reveal_poll(bContext *C)
{
  return ED_gpencil_data_get_active(C) != nullptr;
}

static void gpencil_reveal_select_frame(bContext *C, bGPDframe *frame, bool select)
{
  LISTBASE_FOREACH (bGPDstroke *, gps, &frame->strokes) {

    /* only deselect strokes that are valid in this view */
    if (ED_gpencil_stroke_can_use(C, gps)) {

      /* (de)select points */
      int i;
      bGPDspoint *pt;
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        SET_FLAG_FROM_TEST(pt->flag, select, GP_SPOINT_SELECT);
      }

      /* (de)select stroke */
      SET_FLAG_FROM_TEST(gps->flag, select, GP_STROKE_SELECT);
    }
  }
}

static int gpencil_reveal_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool select = RNA_boolean_get(op->ptr, "select");

  /* sanity checks */
  if (gpd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      gpl->flag &= ~GP_LAYER_HIDE;

      /* select or deselect if requested, only on hidden layers */
      if (gpd->flag & GP_DATA_STROKE_EDITMODE) {
        if (select) {
          /* select all strokes on active frame only (same as select all operator) */
          if (gpl->actframe) {
            gpencil_reveal_select_frame(C, gpl->actframe, true);
          }
        }
        else {
          /* deselect strokes on all frames (same as deselect all operator) */
          LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
            gpencil_reveal_select_frame(C, gpf, false);
          }
        }
      }
    }
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_reveal(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show All Layers";
  ot->idname = "GPENCIL_OT_reveal";
  ot->description = "Show all Grease Pencil layers";

  /* callbacks */
  ot->exec = gpencil_reveal_exec;
  ot->poll = gpencil_reveal_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(ot->srna, "select", true, "Select", "");
}

/* ***************** Lock/Unlock All Layers ************************ */

static int gpencil_lock_all_exec(bContext *C, wmOperator * /*op*/)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* sanity checks */
  if (gpd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* make all layers non-editable */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    gpl->flag |= GP_LAYER_LOCKED;
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_lock_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Lock All Layers";
  ot->idname = "GPENCIL_OT_lock_all";
  ot->description =
      "Lock all Grease Pencil layers to prevent them from being accidentally modified";

  /* callbacks */
  ot->exec = gpencil_lock_all_exec;
  ot->poll = gpencil_reveal_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------- */

static int gpencil_unlock_all_exec(bContext *C, wmOperator * /*op*/)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* sanity checks */
  if (gpd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* make all layers editable again */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    gpl->flag &= ~GP_LAYER_LOCKED;
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_unlock_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unlock All Layers";
  ot->idname = "GPENCIL_OT_unlock_all";
  ot->description = "Unlock all Grease Pencil layers so that they can be edited";

  /* callbacks */
  ot->exec = gpencil_unlock_all_exec;
  ot->poll = gpencil_reveal_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************** Isolate Layer **************************** */

static int gpencil_isolate_layer_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *layer = BKE_gpencil_layer_active_get(gpd);
  int flags = GP_LAYER_LOCKED;
  bool isolate = false;

  if (RNA_boolean_get(op->ptr, "affect_visibility")) {
    flags |= GP_LAYER_HIDE;
  }

  if (ELEM(nullptr, gpd, layer)) {
    BKE_report(op->reports, RPT_ERROR, "No active layer to isolate");
    return OPERATOR_CANCELLED;
  }

  /* Test whether to isolate or clear all flags */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* Skip if this is the active layer */
    if (gpl == layer) {
      continue;
    }

    /* If the flags aren't set, that means that the layer is
     * not alone, so we have some layers to isolate still
     */
    if ((gpl->flag & flags) == 0) {
      isolate = true;
      break;
    }
  }

  /* Set/Clear flags as appropriate */
  /* TODO: Include onion-skinning on this list? */
  if (isolate) {
    /* Set flags on all "other" layers */
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      if (gpl == layer) {
        continue;
      }
      gpl->flag |= flags;
    }
  }
  else {
    /* Clear flags - Restore everything else */
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      gpl->flag &= ~flags;
    }
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_isolate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Isolate Layer";
  ot->idname = "GPENCIL_OT_layer_isolate";
  ot->description =
      "Toggle whether the active layer is the only one that can be edited and/or visible";

  /* callbacks */
  ot->exec = gpencil_isolate_layer_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "affect_visibility",
                  false,
                  "Affect Visibility",
                  "In addition to toggling the editability, also affect the visibility");
}

/* ********************** Merge Layer with the next layer **************************** */
enum {
  GP_LAYER_MERGE_ACTIVE = 0,
  GP_LAYER_MERGE_ALL = 1,
};

static void apply_layer_settings(bGPDlayer *gpl)
{
  /* Apply layer attributes. */
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      gps->fill_opacity_fac *= gpl->opacity;
      gps->vert_color_fill[3] *= gpl->opacity;
      for (int p = 0; p < gps->totpoints; p++) {
        bGPDspoint *pt = &gps->points[p];
        float factor = ((float(gps->thickness) * pt->pressure) + float(gpl->line_change)) /
                       (float(gps->thickness) * pt->pressure);
        pt->pressure *= factor;
        pt->strength *= gpl->opacity;

        /* Layer transformation. */
        mul_v3_m4v3(&pt->x, gpl->layer_mat, &pt->x);
        zero_v3(gpl->location);
        zero_v3(gpl->rotation);
        copy_v3_fl(gpl->scale, 1.0f);
      }
    }
  }

  gpl->line_change = 0;
  gpl->opacity = 1.0f;
  unit_m4(gpl->layer_mat);
  invert_m4_m4(gpl->layer_invmat, gpl->layer_mat);
}

static int gpencil_merge_layer_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *gpl_active = BKE_gpencil_layer_active_get(gpd);
  bGPDlayer *gpl_dst = gpl_active->prev;
  const int mode = RNA_enum_get(op->ptr, "mode");

  if (mode == GP_LAYER_MERGE_ACTIVE) {
    if (ELEM(nullptr, gpd, gpl_dst, gpl_active)) {
      BKE_report(op->reports, RPT_ERROR, "No layers to merge");
      return OPERATOR_CANCELLED;
    }
  }
  else {
    if (ELEM(nullptr, gpd, gpl_active)) {
      BKE_report(op->reports, RPT_ERROR, "No layers to flatten");
      return OPERATOR_CANCELLED;
    }
  }

  if (mode == GP_LAYER_MERGE_ACTIVE) {
    /* Apply destination layer attributes. */
    apply_layer_settings(gpl_active);
    ED_gpencil_layer_merge(gpd, gpl_active, gpl_dst, false);
  }
  else if (mode == GP_LAYER_MERGE_ALL) {
    /* Apply layer attributes to all layers. */
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      apply_layer_settings(gpl);
    }
    gpl_dst = gpl_active;
    /* Merge layers on top of active layer. */
    if (gpd->layers.last != gpl_dst) {
      LISTBASE_FOREACH_BACKWARD_MUTABLE (bGPDlayer *, gpl, &gpd->layers) {
        if (gpl == gpl_dst) {
          break;
        }
        ED_gpencil_layer_merge(gpd, gpl, gpl->prev, false);
      }
    }
    /* Merge layers below active layer. */
    LISTBASE_FOREACH_BACKWARD_MUTABLE (bGPDlayer *, gpl, &gpd->layers) {
      if (gpl == gpl_dst) {
        continue;
      }
      ED_gpencil_layer_merge(gpd, gpl, gpl_dst, true);
    }
    /* Set general layers settings to default values. */
    gpl_active->blend_mode = eGplBlendMode_Regular;
    gpl_active->flag &= ~GP_LAYER_LOCKED;
    gpl_active->flag &= ~GP_LAYER_HIDE;
    gpl_active->flag |= GP_LAYER_USE_LIGHTS;
    gpl_active->onion_flag |= GP_LAYER_ONIONSKIN;
  }
  else {
    return OPERATOR_CANCELLED;
  }

  /* Clear any invalid mask. Some other layer could be using the merged layer. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    BKE_gpencil_layer_mask_cleanup(gpd, gpl);
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_merge(wmOperatorType *ot)
{
  static const EnumPropertyItem merge_modes[] = {
      {GP_LAYER_MERGE_ACTIVE, "ACTIVE", 0, "Active", "Combine active layer into the layer below"},
      {GP_LAYER_MERGE_ALL, "ALL", 0, "All", "Combine all layers into the active layer"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Merge Down";
  ot->idname = "GPENCIL_OT_layer_merge";
  ot->description = "Combine Layers";

  /* callbacks */
  ot->exec = gpencil_merge_layer_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "mode", merge_modes, GP_LAYER_MERGE_ACTIVE, "Mode", "");
}

/* ********************** Change Layer ***************************** */

static int gpencil_layer_change_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  uiPopupMenu *pup;
  uiLayout *layout;

  /* call the menu, which will call this operator again, hence the canceled */
  pup = UI_popup_menu_begin(C, op->type->name, ICON_NONE);
  layout = UI_popup_menu_layout(pup);
  uiItemsEnumO(layout, "GPENCIL_OT_layer_change", "layer");
  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static int gpencil_layer_change_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  if (gpd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bGPDlayer *gpl = nullptr;
  int layer_num = RNA_enum_get(op->ptr, "layer");

  /* Get layer or create new one */
  if (layer_num == -1) {
    /* Create layer */
    gpl = BKE_gpencil_layer_addnew(gpd, DATA_("GP_Layer"), true, false);
  }
  else {
    /* Try to get layer */
    gpl = static_cast<bGPDlayer *>(BLI_findlink(&gpd->layers, layer_num));

    if (gpl == nullptr) {
      BKE_reportf(
          op->reports, RPT_ERROR, "Cannot change to non-existent layer (index = %d)", layer_num);
      return OPERATOR_CANCELLED;
    }
  }

  /* Set active layer */
  BKE_gpencil_layer_active_set(gpd, gpl);

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_change(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Change Layer";
  ot->idname = "GPENCIL_OT_layer_change";
  ot->description = "Change active Grease Pencil layer";

  /* callbacks */
  ot->invoke = gpencil_layer_change_invoke;
  ot->exec = gpencil_layer_change_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* gp layer to use (dynamic enum) */
  ot->prop = RNA_def_enum(
      ot->srna, "layer", rna_enum_dummy_DEFAULT_items, 0, "Grease Pencil Layer", "");
  RNA_def_enum_funcs(ot->prop, ED_gpencil_layers_with_new_enum_itemf);
}

static int gpencil_layer_active_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  int layer_num = RNA_int_get(op->ptr, "layer");

  /* Try to get layer */
  bGPDlayer *gpl = static_cast<bGPDlayer *>(BLI_findlink(&gpd->layers, layer_num));

  if (gpl == nullptr) {
    BKE_reportf(
        op->reports, RPT_ERROR, "Cannot change to non-existent layer (index = %d)", layer_num);
    return OPERATOR_CANCELLED;
  }

  /* Set active layer */
  BKE_gpencil_layer_active_set(gpd, gpl);

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_active(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Active Layer";
  ot->idname = "GPENCIL_OT_layer_active";
  ot->description = "Active Grease Pencil layer";

  /* callbacks */
  ot->exec = gpencil_layer_active_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* GPencil layer to use. */
  ot->prop = RNA_def_int(ot->srna, "layer", 0, 0, INT_MAX, "Grease Pencil Layer", "", 0, INT_MAX);
  RNA_def_property_flag(ot->prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}
/* ************************************************ */

/* ******************* Arrange Stroke Up/Down in drawing order ************************** */

enum {
  GP_STROKE_MOVE_UP = -1,
  GP_STROKE_MOVE_DOWN = 1,
  GP_STROKE_MOVE_TOP = 2,
  GP_STROKE_MOVE_BOTTOM = 3,
};

static int gpencil_stroke_arrange_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *gpl_act = BKE_gpencil_layer_active_get(gpd);

  /* sanity checks */
  if (ELEM(nullptr, gpd, gpl_act, gpl_act->actframe)) {
    return OPERATOR_CANCELLED;
  }

  const int direction = RNA_enum_get(op->ptr, "direction");
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  bGPDstroke *gps_target = nullptr;

  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    /* temp listbase to store selected strokes */
    ListBase selected = {nullptr};

    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);
    bGPDstroke *gps = nullptr;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }
        /* verify if any selected stroke is in the extreme of the stack and select to move */
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* only if selected */
          if (gps->flag & GP_STROKE_SELECT) {
            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* check if the color is editable */
            if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
              continue;
            }
            bool gpf_lock = false;
            /* Some stroke is already at front. */
            if (ELEM(direction, GP_STROKE_MOVE_TOP, GP_STROKE_MOVE_UP)) {
              if (gps == gpf->strokes.last) {
                gpf_lock = true;
                gps_target = gps;
              }
            }
            /* Some stroke is already at bottom. */
            if (ELEM(direction, GP_STROKE_MOVE_BOTTOM, GP_STROKE_MOVE_DOWN)) {
              if (gps == gpf->strokes.first) {
                gpf_lock = true;
                gps_target = gps;
              }
            }
            /* add to list (if not locked) */
            if (!gpf_lock) {
              BLI_addtail(&selected, BLI_genericNodeN(gps));
            }
          }
        }

        const int target_index = (gps_target) ? BLI_findindex(&gpf->strokes, gps_target) : -1;
        int prev_index = target_index;
        /* Now do the movement of the stroke */
        switch (direction) {
          /* Bring to Front */
          case GP_STROKE_MOVE_TOP:
            LISTBASE_FOREACH (LinkData *, link, &selected) {
              gps = static_cast<bGPDstroke *>(link->data);
              BLI_remlink(&gpf->strokes, gps);
              if (gps_target) {
                BLI_insertlinkbefore(&gpf->strokes, gps_target, gps);
              }
              else {
                BLI_addtail(&gpf->strokes, gps);
              }
              changed = true;
            }
            break;
          /* Bring Forward */
          case GP_STROKE_MOVE_UP:
            LISTBASE_FOREACH_BACKWARD (LinkData *, link, &selected) {
              gps = static_cast<bGPDstroke *>(link->data);
              if (gps_target) {
                int gps_index = BLI_findindex(&gpf->strokes, gps);
                if (gps_index + 1 >= prev_index) {
                  prev_index = gps_index;
                  continue;
                }
                prev_index = gps_index;
              }
              BLI_listbase_link_move(&gpf->strokes, gps, 1);
              changed = true;
            }
            break;
          /* Send Backward */
          case GP_STROKE_MOVE_DOWN:
            LISTBASE_FOREACH (LinkData *, link, &selected) {
              gps = static_cast<bGPDstroke *>(link->data);
              if (gps_target) {
                int gps_index = BLI_findindex(&gpf->strokes, gps);
                if (gps_index - 1 <= prev_index) {
                  prev_index = gps_index;
                  continue;
                }
                prev_index = gps_index;
              }
              BLI_listbase_link_move(&gpf->strokes, gps, -1);
              changed = true;
            }
            break;
          /* Send to Back */
          case GP_STROKE_MOVE_BOTTOM:
            LISTBASE_FOREACH_BACKWARD (LinkData *, link, &selected) {
              gps = static_cast<bGPDstroke *>(link->data);
              BLI_remlink(&gpf->strokes, gps);
              if (gps_target) {
                BLI_insertlinkafter(&gpf->strokes, gps_target, gps);
              }
              else {
                BLI_addhead(&gpf->strokes, gps);
              }
              changed = true;
            }
            break;
          default:
            BLI_assert(0);
            break;
        }
        BLI_freelistN(&selected);
      }

      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    /* notifiers */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_arrange(wmOperatorType *ot)
{
  static const EnumPropertyItem slot_move[] = {
      {GP_STROKE_MOVE_TOP, "TOP", 0, "Bring to Front", ""},
      {GP_STROKE_MOVE_UP, "UP", 0, "Bring Forward", ""},
      {GP_STROKE_MOVE_DOWN, "DOWN", 0, "Send Backward", ""},
      {GP_STROKE_MOVE_BOTTOM, "BOTTOM", 0, "Send to Back", ""},
      {0, nullptr, 0, nullptr, nullptr}};

  /* identifiers */
  ot->name = "Arrange Stroke";
  ot->idname = "GPENCIL_OT_stroke_arrange";
  ot->description = "Arrange selected strokes up/down in the display order of the active layer";

  /* callbacks */
  ot->exec = gpencil_stroke_arrange_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "direction", slot_move, GP_STROKE_MOVE_UP, "Direction", "");
}

/* ******************* Move Stroke to new color ************************** */

static int gpencil_stroke_change_color_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Material *ma = nullptr;
  char name[MAX_ID_NAME - 2];
  RNA_string_get(op->ptr, "material", name);

  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);
  if (name[0] == '\0') {
    ma = BKE_gpencil_material(ob, ob->actcol);
  }
  else {
    ma = (Material *)BKE_libblock_find_name(bmain, ID_MA, name);
    if (ma == nullptr) {
      return OPERATOR_CANCELLED;
    }
  }
  /* try to find slot */
  int idx = BKE_gpencil_object_material_index_get(ob, ma);
  if (idx < 0) {
    return OPERATOR_CANCELLED;
  }

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  if (ELEM(nullptr, ma)) {
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  /* loop all strokes */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* only if selected */
          if (gps->flag & GP_STROKE_SELECT) {
            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* check if the color is editable */
            if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
              continue;
            }

            /* assign new color */
            gps->mat_nr = idx;

            changed = true;
          }
        }
      }
      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    /* notifiers */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_change_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Change Stroke Color";
  ot->idname = "GPENCIL_OT_stroke_change_color";
  ot->description = "Move selected strokes to active material";

  /* callbacks */
  ot->exec = gpencil_stroke_change_color_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_string(
      ot->srna, "material", nullptr, MAX_ID_NAME - 2, "Material", "Name of the material");
}

/* ******************* Lock color of non selected Strokes colors ************************** */

static int gpencil_material_lock_unsused_exec(bContext *C, wmOperator * /*op*/)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);
  short *totcol = BKE_object_material_len_p(ob);

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* first lock all colors */
  for (short i = 0; i < *totcol; i++) {
    Material *tmp_ma = BKE_object_material_get(ob, i + 1);
    if (tmp_ma) {
      tmp_ma->gp_style->flag |= GP_MATERIAL_LOCKED;
      DEG_id_tag_update(&tmp_ma->id, ID_RECALC_COPY_ON_WRITE);
    }
  }

  bool changed = false;
  /* loop all selected strokes and unlock any color */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* only editable and visible layers are considered */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != nullptr)) {
      LISTBASE_FOREACH_BACKWARD (bGPDstroke *, gps, &gpl->actframe->strokes) {
        /* only if selected */
        if (gps->flag & GP_STROKE_SELECT) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          /* unlock color */
          Material *tmp_ma = BKE_object_material_get(ob, gps->mat_nr + 1);
          if (tmp_ma) {
            tmp_ma->gp_style->flag &= ~GP_MATERIAL_LOCKED;
            DEG_id_tag_update(&tmp_ma->id, ID_RECALC_COPY_ON_WRITE);
          }

          changed = true;
        }
      }
    }
  }

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* copy on write tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

    /* notifiers */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_material_lock_unused(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Lock Unused Materials";
  ot->idname = "GPENCIL_OT_material_lock_unused";
  ot->description = "Lock any material not used in any selected stroke";

  /* api callbacks */
  ot->exec = gpencil_material_lock_unsused_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************************************ */
/* Drawing Brushes Operators */

/* ******************* Brush resets ************************** */
static int gpencil_brush_reset_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  const enum eContextObjectMode mode = CTX_data_mode_enum(C);
  Brush *brush = nullptr;

  switch (mode) {
    case CTX_MODE_PAINT_GPENCIL_LEGACY: {
      Paint *paint = &ts->gp_paint->paint;
      brush = paint->brush;
      if (brush && brush->gpencil_settings) {
        BKE_gpencil_brush_preset_set(bmain, brush, brush->gpencil_settings->preset_type);
      }
      break;
    }
    case CTX_MODE_SCULPT_GPENCIL_LEGACY: {
      Paint *paint = &ts->gp_sculptpaint->paint;
      brush = paint->brush;
      if (brush && brush->gpencil_settings) {
        BKE_gpencil_brush_preset_set(bmain, brush, brush->gpencil_settings->preset_type);
      }
      break;
    }
    case CTX_MODE_WEIGHT_GPENCIL_LEGACY: {
      Paint *paint = &ts->gp_weightpaint->paint;
      brush = paint->brush;
      if (brush && brush->gpencil_settings) {
        BKE_gpencil_brush_preset_set(bmain, brush, brush->gpencil_settings->preset_type);
      }
      break;
    }
    case CTX_MODE_VERTEX_GPENCIL_LEGACY: {
      Paint *paint = &ts->gp_vertexpaint->paint;
      brush = paint->brush;
      if (brush && brush->gpencil_settings) {
        BKE_gpencil_brush_preset_set(bmain, brush, brush->gpencil_settings->preset_type);
      }
      break;
    }
    default:
      break;
  }

  /* notifiers */
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_brush_reset(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Brush";
  ot->idname = "GPENCIL_OT_brush_reset";
  ot->description = "Reset brush to default parameters";

  /* api callbacks */
  ot->exec = gpencil_brush_reset_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static Brush *gpencil_brush_get_first_by_mode(Main *bmain,
                                              Paint * /*paint*/,
                                              const enum eContextObjectMode mode,
                                              char tool)
{
  Brush *brush_next = nullptr;
  for (Brush *brush = static_cast<Brush *>(bmain->brushes.first); brush; brush = brush_next) {
    brush_next = static_cast<Brush *>(brush->id.next);

    if (brush->gpencil_settings == nullptr) {
      continue;
    }

    if ((mode == CTX_MODE_PAINT_GPENCIL_LEGACY) && (brush->gpencil_tool == tool)) {
      return brush;
    }

    if ((mode == CTX_MODE_SCULPT_GPENCIL_LEGACY) && (brush->gpencil_sculpt_tool == tool)) {
      return brush;
    }

    if ((mode == CTX_MODE_WEIGHT_GPENCIL_LEGACY) && (brush->gpencil_weight_tool == tool)) {
      return brush;
    }

    if ((mode == CTX_MODE_VERTEX_GPENCIL_LEGACY) && (brush->gpencil_vertex_tool == tool)) {
      return brush;
    }
  }

  return nullptr;
}

static void gpencil_brush_delete_mode_brushes(Main *bmain,
                                              Paint *paint,
                                              const enum eContextObjectMode mode)
{
  Brush *brush_active = paint->brush;
  Brush *brush_next = nullptr;
  for (Brush *brush = static_cast<Brush *>(bmain->brushes.first); brush; brush = brush_next) {
    brush_next = static_cast<Brush *>(brush->id.next);

    if ((brush->gpencil_settings == nullptr) && (brush->ob_mode != OB_MODE_PAINT_GPENCIL_LEGACY)) {
      continue;
    }

    short preset = (brush->gpencil_settings) ? brush->gpencil_settings->preset_type :
                                               short(GP_BRUSH_PRESET_UNKNOWN);

    if (preset != GP_BRUSH_PRESET_UNKNOWN) {
      /* Verify to delete only the brushes of the current mode. */
      if (mode == CTX_MODE_PAINT_GPENCIL_LEGACY) {
        if ((preset < GP_BRUSH_PRESET_AIRBRUSH) || (preset > GP_BRUSH_PRESET_TINT)) {
          continue;
        }
        if ((brush_active) && (brush_active->gpencil_tool != brush->gpencil_tool)) {
          continue;
        }
      }

      if (mode == CTX_MODE_SCULPT_GPENCIL_LEGACY) {
        if ((preset < GP_BRUSH_PRESET_SMOOTH_STROKE) || (preset > GP_BRUSH_PRESET_CLONE_STROKE)) {
          continue;
        }
        if ((brush_active) && (brush_active->gpencil_sculpt_tool != brush->gpencil_sculpt_tool)) {
          continue;
        }
      }

      if (mode == CTX_MODE_WEIGHT_GPENCIL_LEGACY) {
        if ((preset < GP_BRUSH_PRESET_WEIGHT_DRAW) || (preset > GP_BRUSH_PRESET_WEIGHT_SMEAR)) {
          continue;
        }
        if ((brush_active) && (brush_active->gpencil_weight_tool != brush->gpencil_weight_tool)) {
          continue;
        }
      }

      if (mode == CTX_MODE_VERTEX_GPENCIL_LEGACY) {
        if ((preset < GP_BRUSH_PRESET_VERTEX_DRAW) || (preset > GP_BRUSH_PRESET_VERTEX_REPLACE)) {
          continue;
        }
        if ((brush_active) && (brush_active->gpencil_vertex_tool != brush->gpencil_vertex_tool)) {
          continue;
        }
      }
    }

    /* Before delete, un-pin any material of the brush. */
    if ((brush->gpencil_settings) && (brush->gpencil_settings->material != nullptr)) {
      brush->gpencil_settings->material = nullptr;
      brush->gpencil_settings->flag &= ~GP_BRUSH_MATERIAL_PINNED;
    }

    BKE_brush_delete(bmain, brush);
    if (brush == brush_active) {
      brush_active = nullptr;
    }
  }
}

static int gpencil_brush_reset_all_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  const enum eContextObjectMode mode = CTX_data_mode_enum(C);
  Paint *paint = nullptr;

  switch (mode) {
    case CTX_MODE_PAINT_GPENCIL_LEGACY: {
      paint = &ts->gp_paint->paint;
      break;
    }
    case CTX_MODE_SCULPT_GPENCIL_LEGACY: {
      paint = &ts->gp_sculptpaint->paint;
      break;
    }
    case CTX_MODE_WEIGHT_GPENCIL_LEGACY: {
      paint = &ts->gp_weightpaint->paint;
      break;
    }
    case CTX_MODE_VERTEX_GPENCIL_LEGACY: {
      paint = &ts->gp_vertexpaint->paint;
      break;
    }
    default:
      break;
  }

  char tool = '0';
  if (paint) {
    if (paint->brush) {
      Brush *brush_active = paint->brush;
      switch (mode) {
        case CTX_MODE_PAINT_GPENCIL_LEGACY: {
          tool = brush_active->gpencil_tool;
          break;
        }
        case CTX_MODE_SCULPT_GPENCIL_LEGACY: {
          tool = brush_active->gpencil_sculpt_tool;
          break;
        }
        case CTX_MODE_WEIGHT_GPENCIL_LEGACY: {
          tool = brush_active->gpencil_weight_tool;
          break;
        }
        case CTX_MODE_VERTEX_GPENCIL_LEGACY: {
          tool = brush_active->gpencil_vertex_tool;
          break;
        }
        default: {
          tool = brush_active->gpencil_tool;
          break;
        }
      }
    }

    gpencil_brush_delete_mode_brushes(bmain, paint, mode);

    switch (mode) {
      case CTX_MODE_PAINT_GPENCIL_LEGACY: {
        BKE_brush_gpencil_paint_presets(bmain, ts, true);
        break;
      }
      case CTX_MODE_SCULPT_GPENCIL_LEGACY: {
        BKE_brush_gpencil_sculpt_presets(bmain, ts, true);
        break;
      }
      case CTX_MODE_WEIGHT_GPENCIL_LEGACY: {
        BKE_brush_gpencil_weight_presets(bmain, ts, true);
        break;
      }
      case CTX_MODE_VERTEX_GPENCIL_LEGACY: {
        BKE_brush_gpencil_vertex_presets(bmain, ts, true);
        break;
      }
      default: {
        break;
      }
    }

    BKE_paint_toolslots_brush_validate(bmain, paint);

    /* Set Again the first brush of the mode. */
    Brush *deft_brush = gpencil_brush_get_first_by_mode(bmain, paint, mode, tool);
    if (deft_brush) {
      BKE_paint_brush_set(paint, deft_brush);
    }
    /* notifiers */
    DEG_relations_tag_update(bmain);
    WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_brush_reset_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset All Brushes";
  ot->idname = "GPENCIL_OT_brush_reset_all";
  ot->description = "Delete all mode brushes and recreate a default set";

  /* api callbacks */
  ot->exec = gpencil_brush_reset_all_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/*********************** Vertex Groups ***********************************/

static bool gpencil_vertex_group_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    Main *bmain = CTX_data_main(C);
    const bGPdata *gpd = (const bGPdata *)ob->data;
    if (BKE_id_is_editable(bmain, &ob->id) &&
        BKE_id_is_editable(bmain, static_cast<const ID *>(ob->data)) &&
        !BLI_listbase_is_empty(&gpd->vertex_group_names))
    {
      if (ELEM(ob->mode, OB_MODE_EDIT_GPENCIL_LEGACY, OB_MODE_SCULPT_GPENCIL_LEGACY)) {
        return true;
      }
    }
  }

  return false;
}

static bool gpencil_vertex_group_weight_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    Main *bmain = CTX_data_main(C);
    const bGPdata *gpd = (const bGPdata *)ob->data;
    if (BKE_id_is_editable(bmain, &ob->id) &&
        BKE_id_is_editable(bmain, static_cast<const ID *>(ob->data)) &&
        !BLI_listbase_is_empty(&gpd->vertex_group_names))
    {
      if (ob->mode == OB_MODE_WEIGHT_GPENCIL_LEGACY) {
        return true;
      }
    }
  }

  return false;
}

static int gpencil_vertex_group_assign_exec(bContext *C, wmOperator * /*op*/)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);

  /* sanity checks */
  if (ELEM(nullptr, ts, ob, ob->data)) {
    return OPERATOR_CANCELLED;
  }

  ED_gpencil_vgroup_assign(C, ob, ts->vgroup_weight);

  /* notifiers */
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_assign(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Assign to Vertex Group";
  ot->idname = "GPENCIL_OT_vertex_group_assign";
  ot->description = "Assign the selected vertices to the active vertex group";

  /* api callbacks */
  ot->poll = gpencil_vertex_group_poll;
  ot->exec = gpencil_vertex_group_assign_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* remove point from vertex group */
static int gpencil_vertex_group_remove_from_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);

  /* sanity checks */
  if (ELEM(nullptr, ob, ob->data)) {
    return OPERATOR_CANCELLED;
  }

  ED_gpencil_vgroup_remove(C, ob);

  /* notifiers */
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_remove_from(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove from Vertex Group";
  ot->idname = "GPENCIL_OT_vertex_group_remove_from";
  ot->description = "Remove the selected vertices from active or all vertex group(s)";

  /* api callbacks */
  ot->poll = gpencil_vertex_group_poll;
  ot->exec = gpencil_vertex_group_remove_from_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int gpencil_vertex_group_select_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);

  /* sanity checks */
  if (ELEM(nullptr, ob, ob->data)) {
    return OPERATOR_CANCELLED;
  }

  ED_gpencil_vgroup_select(C, ob);

  /* notifiers */
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Vertex Group";
  ot->idname = "GPENCIL_OT_vertex_group_select";
  ot->description = "Select all the vertices assigned to the active vertex group";

  /* api callbacks */
  ot->poll = gpencil_vertex_group_poll;
  ot->exec = gpencil_vertex_group_select_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int gpencil_vertex_group_deselect_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);

  /* sanity checks */
  if (ELEM(nullptr, ob, ob->data)) {
    return OPERATOR_CANCELLED;
  }

  ED_gpencil_vgroup_deselect(C, ob);

  /* notifiers */
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_deselect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Deselect Vertex Group";
  ot->idname = "GPENCIL_OT_vertex_group_deselect";
  ot->description = "Deselect all selected vertices assigned to the active vertex group";

  /* api callbacks */
  ot->poll = gpencil_vertex_group_poll;
  ot->exec = gpencil_vertex_group_deselect_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* invert */
static int gpencil_vertex_group_invert_exec(bContext *C, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  /* sanity checks */
  if (ELEM(nullptr, ts, ob, ob->data)) {
    return OPERATOR_CANCELLED;
  }

  MDeformVert *dvert;
  const int def_nr = gpd->vertex_group_active_index - 1;

  bDeformGroup *defgroup = static_cast<bDeformGroup *>(
      BLI_findlink(&gpd->vertex_group_names, def_nr));
  if (defgroup == nullptr) {
    return OPERATOR_CANCELLED;
  }
  if (defgroup->flag & DG_LOCK_WEIGHT) {
    BKE_report(op->reports, RPT_ERROR, "Current Vertex Group is locked");
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    /* Verify the strokes has something to change. */
    if ((gps->totpoints == 0) || (gps->dvert == nullptr)) {
      continue;
    }

    for (int i = 0; i < gps->totpoints; i++) {
      dvert = &gps->dvert[i];
      MDeformWeight *dw = BKE_defvert_find_index(dvert, def_nr);
      if (dw == nullptr) {
        BKE_defvert_add_index_notest(dvert, def_nr, 1.0f);
      }
      else if (dw->weight == 1.0f) {
        BKE_defvert_remove_group(dvert, dw);
      }
      else {
        dw->weight = 1.0f - dw->weight;
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_invert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Invert Vertex Group";
  ot->idname = "GPENCIL_OT_vertex_group_invert";
  ot->description = "Invert weights to the active vertex group";

  /* api callbacks */
  ot->poll = gpencil_vertex_group_weight_poll;
  ot->exec = gpencil_vertex_group_invert_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* smooth */
static int gpencil_vertex_group_smooth_exec(bContext *C, wmOperator *op)
{
  const float fac = RNA_float_get(op->ptr, "factor");
  const int repeat = RNA_int_get(op->ptr, "repeat");

  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  /* sanity checks */
  if (ELEM(nullptr, ts, ob, ob->data)) {
    return OPERATOR_CANCELLED;
  }

  const int def_nr = gpd->vertex_group_active_index - 1;
  bDeformGroup *defgroup = static_cast<bDeformGroup *>(
      BLI_findlink(&gpd->vertex_group_names, def_nr));
  if (defgroup == nullptr) {
    return OPERATOR_CANCELLED;
  }
  if (defgroup->flag & DG_LOCK_WEIGHT) {
    BKE_report(op->reports, RPT_ERROR, "Current Vertex Group is locked");
    return OPERATOR_CANCELLED;
  }

  bGPDspoint *pta, *ptb, *ptc;
  MDeformVert *dverta, *dvertb;

  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    /* Verify the strokes has something to change. */
    if ((gps->totpoints == 0) || (gps->dvert == nullptr)) {
      continue;
    }

    for (int s = 0; s < repeat; s++) {
      for (int i = 0; i < gps->totpoints; i++) {
        /* previous point */
        if (i > 0) {
          pta = &gps->points[i - 1];
          dverta = &gps->dvert[i - 1];
        }
        else {
          pta = &gps->points[i];
          dverta = &gps->dvert[i];
        }
        /* current */
        ptb = &gps->points[i];
        dvertb = &gps->dvert[i];
        /* next point */
        if (i + 1 < gps->totpoints) {
          ptc = &gps->points[i + 1];
        }
        else {
          ptc = &gps->points[i];
        }

        float wa = BKE_defvert_find_weight(dverta, def_nr);
        float wb = BKE_defvert_find_weight(dvertb, def_nr);

        /* the optimal value is the corresponding to the interpolation of the weight
         * at the distance of point b
         */
        const float opfac = line_point_factor_v3(&ptb->x, &pta->x, &ptc->x);
        const float optimal = interpf(wa, wb, opfac);
        /* Based on influence factor, blend between original and optimal */
        MDeformWeight *dw = BKE_defvert_ensure_index(dvertb, def_nr);
        if (dw) {
          dw->weight = interpf(wb, optimal, fac);
          CLAMP(dw->weight, 0.0, 1.0f);
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_smooth(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Smooth Vertex Group";
  ot->idname = "GPENCIL_OT_vertex_group_smooth";
  ot->description = "Smooth weights to the active vertex group";

  /* api callbacks */
  ot->poll = gpencil_vertex_group_weight_poll;
  ot->exec = gpencil_vertex_group_smooth_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float(ot->srna, "factor", 0.5f, 0.0f, 1.0, "Factor", "", 0.0f, 1.0f);
  RNA_def_int(ot->srna, "repeat", 1, 1, 10000, "Iterations", "", 1, 200);
}

/* normalize */
static int gpencil_vertex_group_normalize_exec(bContext *C, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  /* sanity checks */
  if (ELEM(nullptr, ts, ob, ob->data)) {
    return OPERATOR_CANCELLED;
  }

  MDeformVert *dvert = nullptr;
  MDeformWeight *dw = nullptr;
  const int def_nr = gpd->vertex_group_active_index - 1;
  bDeformGroup *defgroup = static_cast<bDeformGroup *>(
      BLI_findlink(&gpd->vertex_group_names, def_nr));
  if (defgroup == nullptr) {
    return OPERATOR_CANCELLED;
  }
  if (defgroup->flag & DG_LOCK_WEIGHT) {
    BKE_report(op->reports, RPT_ERROR, "Current Vertex Group is locked");
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    /* Verify the strokes has something to change. */
    if ((gps->totpoints == 0) || (gps->dvert == nullptr)) {
      continue;
    }

    /* look for max value */
    float maxvalue = 0.0f;
    for (int i = 0; i < gps->totpoints; i++) {
      dvert = &gps->dvert[i];
      dw = BKE_defvert_find_index(dvert, def_nr);
      if ((dw != nullptr) && (dw->weight > maxvalue)) {
        maxvalue = dw->weight;
      }
    }

    /* normalize weights */
    if (maxvalue > 0.0f) {
      for (int i = 0; i < gps->totpoints; i++) {
        dvert = &gps->dvert[i];
        dw = BKE_defvert_find_index(dvert, def_nr);
        if (dw != nullptr) {
          dw->weight = dw->weight / maxvalue;
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_normalize(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Normalize Vertex Group";
  ot->idname = "GPENCIL_OT_vertex_group_normalize";
  ot->description = "Normalize weights to the active vertex group";

  /* api callbacks */
  ot->poll = gpencil_vertex_group_weight_poll;
  ot->exec = gpencil_vertex_group_normalize_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* normalize all */
static int gpencil_vertex_group_normalize_all_exec(bContext *C, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  bool lock_active = RNA_boolean_get(op->ptr, "lock_active");
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  /* sanity checks */
  if (ELEM(nullptr, ts, ob, ob->data)) {
    return OPERATOR_CANCELLED;
  }

  bDeformGroup *defgroup = nullptr;
  MDeformVert *dvert = nullptr;
  MDeformWeight *dw = nullptr;
  const int def_nr = gpd->vertex_group_active_index - 1;
  const int defbase_tot = BLI_listbase_count(&gpd->vertex_group_names);
  if (defbase_tot == 0) {
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    /* Verify the strokes has something to change. */
    if ((gps->totpoints == 0) || (gps->dvert == nullptr)) {
      continue;
    }

    /* Loop all points in stroke. */
    for (int i = 0; i < gps->totpoints; i++) {
      int v;
      float sum = 0.0f;
      float sum_lock = 0.0f;
      float sum_unlock = 0.0f;

      /* Get vertex groups and weights. */
      dvert = &gps->dvert[i];

      /* Sum weights. */
      for (v = 0; v < defbase_tot; v++) {
        /* Get vertex group. */
        defgroup = static_cast<bDeformGroup *>(BLI_findlink(&gpd->vertex_group_names, v));
        if (defgroup == nullptr) {
          continue;
        }

        /* Get weight in vertex group. */
        dw = BKE_defvert_find_index(dvert, v);
        if (dw == nullptr) {
          continue;
        }
        sum += dw->weight;

        /* Vertex group locked or unlocked? */
        if ((defgroup->flag & DG_LOCK_WEIGHT) || ((lock_active) && (v == def_nr))) {
          sum_lock += dw->weight;
        }
        else {
          sum_unlock += dw->weight;
        }
      }

      if ((sum == 1.0f) || (sum_unlock == 0.0f)) {
        continue;
      }

      /* Normalize weights. */
      float fac = std::max(0.0f, (1.0f - sum_lock) / sum_unlock);

      for (v = 0; v < defbase_tot; v++) {
        /* Get vertex group. */
        defgroup = static_cast<bDeformGroup *>(BLI_findlink(&gpd->vertex_group_names, v));
        if (defgroup == nullptr) {
          continue;
        }

        /* Get weight in vertex group. */
        dw = BKE_defvert_find_index(dvert, v);
        if (dw == nullptr) {
          continue;
        }

        /* Normalize in unlocked vertex groups only. */
        if (!((defgroup->flag & DG_LOCK_WEIGHT) || ((lock_active) && (v == def_nr)))) {
          dw->weight *= fac;
          CLAMP(dw->weight, 0.0f, 1.0f);
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_normalize_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Normalize All Vertex Group";
  ot->idname = "GPENCIL_OT_vertex_group_normalize_all";
  ot->description =
      "Normalize all weights of all vertex groups, "
      "so that for each vertex, the sum of all weights is 1.0";

  /* api callbacks */
  ot->poll = gpencil_vertex_group_weight_poll;
  ot->exec = gpencil_vertex_group_normalize_all_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(ot->srna,
                  "lock_active",
                  true,
                  "Lock Active",
                  "Keep the values of the active group while normalizing others");
}

/****************************** Join ***********************************/

/** User-data for #gpencil_joined_fix_animdata_cb(). */
struct tJoinGPencil_AdtFixData {
  bGPdata *src_gpd;
  bGPdata *tar_gpd;

  GHash *names_map;
};

/**
 * Callback to pass to #BKE_fcurves_main_cb()
 * for RNA Paths attached to each F-Curve used in the #AnimData.
 */
static void gpencil_joined_fix_animdata_cb(ID *id, FCurve *fcu, void *user_data)
{
  tJoinGPencil_AdtFixData *afd = (tJoinGPencil_AdtFixData *)user_data;
  ID *src_id = &afd->src_gpd->id;
  ID *dst_id = &afd->tar_gpd->id;

  GHashIterator gh_iter;

  /* Fix paths - If this is the target datablock, it will have some "dirty" paths */
  if ((id == src_id) && fcu->rna_path && strstr(fcu->rna_path, "layers[")) {
    GHASH_ITER (gh_iter, afd->names_map) {
      const char *old_name = static_cast<const char *>(BLI_ghashIterator_getKey(&gh_iter));
      const char *new_name = static_cast<const char *>(BLI_ghashIterator_getValue(&gh_iter));

      /* only remap if changed;
       * this still means there will be some waste if there aren't many drivers/keys */
      if (!STREQ(old_name, new_name) && strstr(fcu->rna_path, old_name)) {
        fcu->rna_path = BKE_animsys_fix_rna_path_rename(
            id, fcu->rna_path, "layers", old_name, new_name, 0, 0, false);

        /* We don't want to apply a second remapping on this F-Curve now,
         * so stop trying to fix names. */
        break;
      }
    }
  }

  /* Fix driver targets */
  if (fcu->driver) {
    /* Fix driver references to invalid ID's */
    LISTBASE_FOREACH (DriverVar *, dvar, &fcu->driver->variables) {
      /* Only change the used targets, since the others will need fixing manually anyway. */
      DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
        /* Change the ID's used. */
        if (dtar->id == src_id) {
          dtar->id = dst_id;

          /* Also check on the sub-target.
           * We duplicate the logic from #drivers_path_rename_fix() here, with our own
           * little twists so that we know that it isn't going to clobber the wrong data
           */
          if (dtar->rna_path && strstr(dtar->rna_path, "layers[")) {
            GHASH_ITER (gh_iter, afd->names_map) {
              const char *old_name = static_cast<const char *>(BLI_ghashIterator_getKey(&gh_iter));
              const char *new_name = static_cast<const char *>(
                  BLI_ghashIterator_getValue(&gh_iter));

              /* Only remap if changed. */
              if (!STREQ(old_name, new_name)) {
                if ((dtar->rna_path) && strstr(dtar->rna_path, old_name)) {
                  /* Fix up path */
                  dtar->rna_path = BKE_animsys_fix_rna_path_rename(
                      id, dtar->rna_path, "layers", old_name, new_name, 0, 0, false);
                  break; /* no need to try any more names for layer path */
                }
              }
            }
          }
        }
      }
      DRIVER_TARGETS_LOOPER_END;
    }
  }
}

int ED_gpencil_join_objects_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob_active = CTX_data_active_object(C);
  bool ok = false;

  /* Ensure we're in right mode and that the active object is correct */
  if (!ob_active || ob_active->type != OB_GPENCIL_LEGACY) {
    return OPERATOR_CANCELLED;
  }

  bGPdata *gpd = (bGPdata *)ob_active->data;
  if ((!gpd) || GPENCIL_ANY_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* Ensure all rotations are applied before */
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter->type == OB_GPENCIL_LEGACY) {
      if ((ob_iter->rot[0] != 0) || (ob_iter->rot[1] != 0) || (ob_iter->rot[2] != 0)) {
        BKE_report(op->reports, RPT_ERROR, "Apply all rotations before join objects");
        return OPERATOR_CANCELLED;
      }
    }
  }
  CTX_DATA_END;

  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter == ob_active) {
      ok = true;
      break;
    }
  }
  CTX_DATA_END;

  /* that way the active object is always selected */
  if (ok == false) {
    BKE_report(op->reports, RPT_WARNING, "Active object is not a selected grease pencil");
    return OPERATOR_CANCELLED;
  }

  bGPdata *gpd_dst = static_cast<bGPdata *>(ob_active->data);
  Object *ob_dst = ob_active;

  /* loop and join all data */
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if ((ob_iter->type == OB_GPENCIL_LEGACY) && (ob_iter != ob_active)) {
      /* we assume that each datablock is not already used in active object */
      if (ob_active->data != ob_iter->data) {
        Object *ob_src = ob_iter;
        bGPdata *gpd_src = static_cast<bGPdata *>(ob_iter->data);

        /* Apply all GP modifiers before */
        LISTBASE_FOREACH (GpencilModifierData *, md, &ob_iter->greasepencil_modifiers) {
          const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(
              GpencilModifierType(md->type));
          if (mti->bake_modifier) {
            mti->bake_modifier(bmain, depsgraph, md, ob_iter);
          }
        }

        /* copy vertex groups to the base one's */
        int old_idx = 0;
        LISTBASE_FOREACH (bDeformGroup *, dg, &gpd_src->vertex_group_names) {
          bDeformGroup *vgroup = static_cast<bDeformGroup *>(MEM_dupallocN(dg));
          int idx = BLI_listbase_count(&gpd_dst->vertex_group_names);
          BKE_object_defgroup_unique_name(vgroup, ob_active);
          BLI_addtail(&gpd_dst->vertex_group_names, vgroup);
          /* update vertex groups in strokes in original data */
          LISTBASE_FOREACH (bGPDlayer *, gpl_src, &gpd->layers) {
            LISTBASE_FOREACH (bGPDframe *, gpf, &gpl_src->frames) {
              LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
                MDeformVert *dvert;
                int i;
                if (gps->dvert == nullptr) {
                  continue;
                }
                for (i = 0, dvert = gps->dvert; i < gps->totpoints; i++, dvert++) {
                  if ((dvert->dw != nullptr) && (dvert->dw->def_nr == old_idx)) {
                    dvert->dw->def_nr = idx;
                  }
                }
              }
            }
          }
          old_idx++;
        }
        if (!BLI_listbase_is_empty(&gpd_dst->vertex_group_names) &&
            gpd_dst->vertex_group_active_index == 0)
        {
          gpd_dst->vertex_group_active_index = 1;
        }

        /* add missing materials reading source materials and checking in destination object */
        short *totcol = BKE_object_material_len_p(ob_src);

        for (short i = 0; i < *totcol; i++) {
          Material *tmp_ma = BKE_gpencil_material(ob_src, i + 1);
          BKE_gpencil_object_material_ensure(bmain, ob_dst, tmp_ma);
        }

        /* Duplicate #bGPDlayers. */
        tJoinGPencil_AdtFixData afd = {nullptr};
        afd.src_gpd = gpd_src;
        afd.tar_gpd = gpd_dst;
        afd.names_map = BLI_ghash_str_new("joined_gp_layers_map");

        float imat[3][3], bmat[3][3];
        float offset_global[3];
        float offset_local[3];

        sub_v3_v3v3(offset_global, ob_active->loc, ob_iter->object_to_world[3]);
        copy_m3_m4(bmat, ob_active->object_to_world);

        /* Inverse transform for all selected curves in this object,
         * See #object_join_exec for detailed comment on why the safe version is used. */
        invert_m3_m3_safe_ortho(imat, bmat);
        mul_m3_v3(imat, offset_global);
        mul_v3_m3v3(offset_local, imat, offset_global);

        LISTBASE_FOREACH (bGPDlayer *, gpl_src, &gpd_src->layers) {
          bGPDlayer *gpl_new = BKE_gpencil_layer_duplicate(gpl_src, true, true);
          float diff_mat[4][4];
          float inverse_diff_mat[4][4];

          /* recalculate all stroke points */
          BKE_gpencil_layer_transform_matrix_get(depsgraph, ob_iter, gpl_src, diff_mat);
          invert_m4_m4_safe_ortho(inverse_diff_mat, diff_mat);

          Material *ma_src = nullptr;
          LISTBASE_FOREACH (bGPDframe *, gpf, &gpl_new->frames) {
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {

              /* Reassign material. Look old material and try to find in destination. */
              ma_src = BKE_gpencil_material(ob_src, gps->mat_nr + 1);
              gps->mat_nr = BKE_gpencil_object_material_ensure(bmain, ob_dst, ma_src);

              bGPDspoint *pt;
              int i;
              for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                float mpt[3];
                mul_v3_m4v3(mpt, inverse_diff_mat, &pt->x);
                sub_v3_v3(mpt, offset_local);
                mul_v3_m4v3(&pt->x, diff_mat, mpt);
              }
            }
          }

          /* be sure name is unique in new object */
          BLI_uniquename(&gpd_dst->layers,
                         gpl_new,
                         DATA_("GP_Layer"),
                         '.',
                         offsetof(bGPDlayer, info),
                         sizeof(gpl_new->info));
          BLI_ghash_insert(afd.names_map, BLI_strdup(gpl_src->info), gpl_new->info);

          /* add to destination datablock */
          BLI_addtail(&gpd_dst->layers, gpl_new);
        }

        /* Fix all the animation data */
        BKE_fcurves_main_cb(bmain, gpencil_joined_fix_animdata_cb, &afd);
        BLI_ghash_free(afd.names_map, MEM_freeN, nullptr);

        /* Only copy over animdata now, after all the remapping has been done,
         * so that we don't have to worry about ambiguities re which datablock
         * a layer came from!
         */
        if (ob_iter->adt) {
          if (ob_active->adt == nullptr) {
            /* no animdata, so just use a copy of the whole thing */
            ob_active->adt = BKE_animdata_copy(bmain, ob_iter->adt, 0);
          }
          else {
            /* merge in data - we'll fix the drivers manually */
            BKE_animdata_merge_copy(
                bmain, &ob_active->id, &ob_iter->id, ADT_MERGECOPY_KEEP_DST, false);
          }
        }

        if (gpd_src->adt) {
          if (gpd_dst->adt == nullptr) {
            /* no animdata, so just use a copy of the whole thing */
            gpd_dst->adt = BKE_animdata_copy(bmain, gpd_src->adt, 0);
          }
          else {
            /* merge in data - we'll fix the drivers manually */
            BKE_animdata_merge_copy(
                bmain, &gpd_dst->id, &gpd_src->id, ADT_MERGECOPY_KEEP_DST, false);
          }
        }
        DEG_id_tag_update(&gpd_src->id,
                          ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
      }

      /* Free the old object */
      ED_object_base_free_and_unlink(bmain, scene, ob_iter);
    }
  }
  CTX_DATA_END;

  DEG_id_tag_update(&gpd_dst->id,
                    ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain); /* because we removed object(s) */

  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  return OPERATOR_FINISHED;
}

/* Color Handle operator */
static bool gpencil_active_material_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob && ob->data && (ob->type == OB_GPENCIL_LEGACY)) {
    short *totcolp = BKE_object_material_len_p(ob);
    return *totcolp > 0;
  }
  return false;
}

/* **************** Lock and hide any color non used in current layer **************************
 */
static int gpencil_lock_layer_exec(bContext *C, wmOperator * /*op*/)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);
  MaterialGPencilStyle *gp_style = nullptr;

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* first lock and hide all colors */
  Material *ma = nullptr;
  short *totcol = BKE_object_material_len_p(ob);
  if (totcol == nullptr) {
    return OPERATOR_CANCELLED;
  }

  for (short i = 0; i < *totcol; i++) {
    ma = BKE_gpencil_material(ob, i + 1);
    if (ma) {
      gp_style = ma->gp_style;
      gp_style->flag |= GP_MATERIAL_LOCKED;
      gp_style->flag |= GP_MATERIAL_HIDE;
      DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);
    }
  }

  /* loop all selected strokes and unlock any color used in active layer */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* only editable and visible layers are considered */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != nullptr) &&
        (gpl->flag & GP_LAYER_ACTIVE))
    {
      LISTBASE_FOREACH_BACKWARD (bGPDstroke *, gps, &gpl->actframe->strokes) {
        /* skip strokes that are invalid for current view */
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }

        ma = BKE_gpencil_material(ob, gps->mat_nr + 1);
        DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);

        gp_style = ma->gp_style;
        /* unlock/unhide color if not unlocked before */
        if (gp_style != nullptr) {
          gp_style->flag &= ~GP_MATERIAL_LOCKED;
          gp_style->flag &= ~GP_MATERIAL_HIDE;
        }
      }
    }
  }
  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_lock_layer(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Disable Unused Layer Colors";
  ot->idname = "GPENCIL_OT_lock_layer";
  ot->description = "Lock and hide any color not used in any layer";

  /* api callbacks */
  ot->exec = gpencil_lock_layer_exec;
  ot->poll = gpencil_active_layer_poll;
}

/* ********************** Isolate gpencil_ color **************************** */

static int gpencil_material_isolate_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);
  Material *active_ma = BKE_gpencil_material(ob, ob->actcol);
  MaterialGPencilStyle *active_color = BKE_gpencil_material_settings(ob, ob->actcol);
  MaterialGPencilStyle *gp_style;

  int flags = GP_MATERIAL_LOCKED;
  bool isolate = false;

  if (RNA_boolean_get(op->ptr, "affect_visibility")) {
    flags |= GP_MATERIAL_HIDE;
  }

  if (ELEM(nullptr, gpd, active_color)) {
    BKE_report(op->reports, RPT_ERROR, "No active color to isolate");
    return OPERATOR_CANCELLED;
  }

  /* Test whether to isolate or clear all flags */
  Material *ma = nullptr;
  short *totcol = BKE_object_material_len_p(ob);
  for (short i = 0; i < *totcol; i++) {
    ma = BKE_gpencil_material(ob, i + 1);
    /* Skip if this is the active one */
    if (ELEM(ma, nullptr, active_ma)) {
      continue;
    }

    /* If the flags aren't set, that means that the color is
     * not alone, so we have some colors to isolate still
     */
    gp_style = ma->gp_style;
    if ((gp_style->flag & flags) == 0) {
      isolate = true;
      break;
    }
  }

  /* Set/Clear flags as appropriate */
  if (isolate) {
    /* Set flags on all "other" colors */
    for (short i = 0; i < *totcol; i++) {
      ma = BKE_gpencil_material(ob, i + 1);
      if (ma == nullptr) {
        continue;
      }
      gp_style = ma->gp_style;
      if (gp_style == active_color) {
        continue;
      }
      gp_style->flag |= flags;
      DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);
    }
  }
  else {
    /* Clear flags - Restore everything else */
    for (short i = 0; i < *totcol; i++) {
      ma = BKE_gpencil_material(ob, i + 1);
      if (ma == nullptr) {
        continue;
      }
      gp_style = ma->gp_style;
      gp_style->flag &= ~flags;
      DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);
    }
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_material_isolate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Isolate Material";
  ot->idname = "GPENCIL_OT_material_isolate";
  ot->description =
      "Toggle whether the active material is the only one that is editable and/or visible";

  /* callbacks */
  ot->exec = gpencil_material_isolate_exec;
  ot->poll = gpencil_active_material_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "affect_visibility",
                  false,
                  "Affect Visibility",
                  "In addition to toggling "
                  "the editability, also affect the visibility");
}

/* *********************** Hide colors ******************************** */

static int gpencil_material_hide_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  MaterialGPencilStyle *active_color = BKE_gpencil_material_settings(ob, ob->actcol);

  bool unselected = RNA_boolean_get(op->ptr, "unselected");

  Material *ma = nullptr;
  short *totcol = BKE_object_material_len_p(ob);
  if (totcol == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (unselected) {
    /* hide unselected */
    MaterialGPencilStyle *color = nullptr;
    for (short i = 0; i < *totcol; i++) {
      ma = BKE_gpencil_material(ob, i + 1);
      if (ma) {
        color = ma->gp_style;
        if (active_color != color) {
          color->flag |= GP_MATERIAL_HIDE;
          DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);
        }
      }
    }
  }
  else {
    /* hide selected/active */
    active_color->flag |= GP_MATERIAL_HIDE;
  }

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  /* notifiers */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_material_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Material(s)";
  ot->idname = "GPENCIL_OT_material_hide";
  ot->description = "Hide selected/unselected Grease Pencil materials";

  /* callbacks */
  ot->exec = gpencil_material_hide_exec;
  ot->poll = gpencil_active_material_poll; /* NOTE: we need an active color to play with */

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(
      ot->srna, "unselected", false, "Unselected", "Hide unselected rather than selected colors");
}

/* ********************** Show All Colors ***************************** */

static int gpencil_material_reveal_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  Material *ma = nullptr;
  short *totcol = BKE_object_material_len_p(ob);

  if (totcol == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* make all colors visible */
  MaterialGPencilStyle *gp_style = nullptr;

  for (short i = 0; i < *totcol; i++) {
    ma = BKE_gpencil_material(ob, i + 1);
    if (ma) {
      gp_style = ma->gp_style;
      gp_style->flag &= ~GP_MATERIAL_HIDE;
      DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);
    }
  }

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  /* notifiers */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_material_reveal(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show All Materials";
  ot->idname = "GPENCIL_OT_material_reveal";
  ot->description = "Unhide all hidden Grease Pencil materials";

  /* callbacks */
  ot->exec = gpencil_material_reveal_exec;
  ot->poll = gpencil_active_material_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ***************** Lock/Unlock All colors ************************ */

static int gpencil_material_lock_all_exec(bContext *C, wmOperator * /*op*/)
{

  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  Material *ma = nullptr;
  short *totcol = BKE_object_material_len_p(ob);

  if (totcol == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* make all layers non-editable */
  MaterialGPencilStyle *gp_style = nullptr;

  for (short i = 0; i < *totcol; i++) {
    ma = BKE_gpencil_material(ob, i + 1);
    if (ma) {
      gp_style = ma->gp_style;
      gp_style->flag |= GP_MATERIAL_LOCKED;
      DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);
    }
  }

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  /* notifiers */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_material_lock_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Lock All Materials";
  ot->idname = "GPENCIL_OT_material_lock_all";
  ot->description =
      "Lock all Grease Pencil materials to prevent them from being accidentally modified";

  /* callbacks */
  ot->exec = gpencil_material_lock_all_exec;
  ot->poll = gpencil_active_material_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------- */

static int gpencil_material_unlock_all_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  Material *ma = nullptr;
  short *totcol = BKE_object_material_len_p(ob);

  if (totcol == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Make all layers editable again. */
  MaterialGPencilStyle *gp_style = nullptr;

  for (short i = 0; i < *totcol; i++) {
    ma = BKE_gpencil_material(ob, i + 1);
    if (ma) {
      gp_style = ma->gp_style;
      gp_style->flag &= ~GP_MATERIAL_LOCKED;
      DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);
    }
  }

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  /* notifiers */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_material_unlock_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unlock All Materials";
  ot->idname = "GPENCIL_OT_material_unlock_all";
  ot->description = "Unlock all Grease Pencil materials so that they can be edited";

  /* callbacks */
  ot->exec = gpencil_material_unlock_all_exec;
  ot->poll = gpencil_active_material_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ***************** Select all strokes using color ************************ */

static int gpencil_material_select_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);
  MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, ob->actcol);
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const bool deselected = RNA_boolean_get(op->ptr, "deselect");

  /* sanity checks */
  if (ELEM(nullptr, gpd, gp_style)) {
    return OPERATOR_CANCELLED;
  }

  /* Read all strokes and select. */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

        /* verify something to do */
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          /* check if the color is editable */
          if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
            continue;
          }

          /* select */
          if (ob->actcol == gps->mat_nr + 1) {
            bGPDspoint *pt;
            int i;

            if (!deselected) {
              gps->flag |= GP_STROKE_SELECT;
              BKE_gpencil_stroke_select_index_set(gpd, gps);
            }
            else {
              gps->flag &= ~GP_STROKE_SELECT;
              BKE_gpencil_stroke_select_index_reset(gps);
            }
            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              if (!deselected) {
                pt->flag |= GP_SPOINT_SELECT;
              }
              else {
                pt->flag &= ~GP_SPOINT_SELECT;
              }
            }
          }
        }
      }
      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);

  /* notifiers */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_material_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Material";
  ot->idname = "GPENCIL_OT_material_select";
  ot->description = "Select/Deselect all Grease Pencil strokes using current material";

  /* callbacks */
  ot->exec = gpencil_material_select_exec;
  ot->poll = gpencil_active_material_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "Unselect strokes");
  RNA_def_property_flag(ot->prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* ***************** Set active material ************************* */
static int gpencil_material_set_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  int slot = RNA_enum_get(op->ptr, "slot");

  /* Try to get material */
  if ((slot < 1) || (slot > ob->totcol)) {
    BKE_reportf(
        op->reports, RPT_ERROR, "Cannot change to non-existent material (index = %d)", slot);
    return OPERATOR_CANCELLED;
  }

  /* Set active material. */
  ob->actcol = slot;

  /* updates */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_material_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Material";
  ot->idname = "GPENCIL_OT_material_set";
  ot->description = "Set active material";

  /* callbacks */
  ot->exec = gpencil_material_set_exec;
  ot->poll = gpencil_active_material_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Material to use (dynamic enum) */
  ot->prop = RNA_def_enum(ot->srna, "slot", rna_enum_dummy_DEFAULT_items, 0, "Material Slot", "");
  RNA_def_enum_funcs(ot->prop, ED_gpencil_material_enum_itemf);
}

/* ***************** Set selected stroke material the active material ************************ */

static int gpencil_set_active_material_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* Sanity checks. */
  if (gpd == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  /* Loop all selected strokes. */
  GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    if (gps->flag & GP_STROKE_SELECT) {
      /* Change Active material. */
      ob->actcol = gps->mat_nr + 1;
      changed = true;
      break;
    }
  }
  GP_EDITABLE_STROKES_END(gpstroke_iter);

  /* notifiers */
  if (changed) {
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_set_active_material(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set active material";
  ot->idname = "GPENCIL_OT_set_active_material";
  ot->description = "Set the selected stroke material as the active material";

  /* callbacks */
  ot->exec = gpencil_set_active_material_exec;
  ot->poll = gpencil_active_material_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************* Append Materials in a new object ************************** */
static bool gpencil_materials_copy_to_object_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }
  short *totcolp = BKE_object_material_len_p(ob);
  if (*totcolp == 0) {
    return false;
  }

  return true;
}

static int gpencil_materials_copy_to_object_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const bool only_active = RNA_boolean_get(op->ptr, "only_active");
  Object *ob_src = CTX_data_active_object(C);
  Material *ma_active = BKE_gpencil_material(ob_src, ob_src->actcol);

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if ((ob == ob_src) || (ob->type != OB_GPENCIL_LEGACY)) {
      continue;
    }
    /* Duplicate materials. */
    for (int i = 0; i < ob_src->totcol; i++) {
      Material *ma_src = BKE_object_material_get(ob_src, i + 1);
      if (only_active && ma_src != ma_active) {
        continue;
      }

      if (ma_src != nullptr) {
        BKE_gpencil_object_material_ensure(bmain, ob, ma_src);
      }
    }

    /* notifiers */
    DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
  }
  CTX_DATA_END;

  /* notifiers */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_materials_copy_to_object(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Copy Materials to Selected Object";
  ot->idname = "GPENCIL_OT_materials_copy_to_object";
  ot->description = "Append Materials of the active Grease Pencil to other object";

  /* callbacks */
  ot->exec = gpencil_materials_copy_to_object_exec;
  ot->poll = gpencil_materials_copy_to_object_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_boolean(ot->srna,
                         "only_active",
                         true,
                         "Only Active",
                         "Append only active material, uncheck to append all materials");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

bool ED_gpencil_add_lattice_modifier(const bContext *C,
                                     ReportList *reports,
                                     Object *ob,
                                     Object *ob_latt)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  if (ob == nullptr) {
    return false;
  }

  /* if no lattice modifier, add a new one */
  GpencilModifierData *md = BKE_gpencil_modifiers_findby_type(ob, eGpencilModifierType_Lattice);
  if (md == nullptr) {
    md = ED_object_gpencil_modifier_add(
        reports, bmain, scene, ob, "Lattice", eGpencilModifierType_Lattice);
    if (md == nullptr) {
      BKE_report(reports, RPT_ERROR, "Unable to add a new Lattice modifier to object");
      return false;
    }
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }

  /* verify lattice */
  LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;
  if (mmd->object == nullptr) {
    mmd->object = ob_latt;
  }
  else {
    if (ob_latt != mmd->object) {
      BKE_report(reports,
                 RPT_ERROR,
                 "The existing Lattice modifier is already using a different Lattice object");
      return false;
    }
  }

  return true;
}

/* Masking operators */
static int gpencil_layer_mask_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return OPERATOR_CANCELLED;
  }

  bGPdata *gpd = (bGPdata *)ob->data;
  bGPDlayer *gpl_active = BKE_gpencil_layer_active_get(gpd);
  if (gpl_active == nullptr) {
    return OPERATOR_CANCELLED;
  }
  char name[128];
  RNA_string_get(op->ptr, "name", name);
  bGPDlayer *gpl = BKE_gpencil_layer_named_get(gpd, name);

  if (gpl == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Unable to find layer to add");
    return OPERATOR_CANCELLED;
  }

  if (gpl == gpl_active) {
    BKE_report(op->reports, RPT_ERROR, "Cannot add active layer as mask");
    return OPERATOR_CANCELLED;
  }

  if (BKE_gpencil_layer_mask_named_get(gpl_active, name)) {
    BKE_report(op->reports, RPT_ERROR, "Layer already added");
    return OPERATOR_CANCELLED;
  }

  if (gpl_active->act_mask == 256) {
    BKE_report(op->reports, RPT_ERROR, "Maximum number of masking layers reached");
    return OPERATOR_CANCELLED;
  }

  BKE_gpencil_layer_mask_add(gpl_active, name);

  /* Reorder masking. */
  BKE_gpencil_layer_mask_sort(gpd, gpl_active);

  /* notifiers */
  if (gpd) {
    DEG_id_tag_update(&gpd->id,
                      ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  }
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_mask_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add New Mask Layer";
  ot->idname = "GPENCIL_OT_layer_mask_add";
  ot->description = "Add new layer as masking";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_layer_mask_add_exec;
  ot->poll = gpencil_add_poll;

  /* properties */
  RNA_def_string(ot->srna, "name", nullptr, 128, "Layer", "Name of the layer");
}

static int gpencil_layer_mask_remove_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return OPERATOR_CANCELLED;
  }

  bGPdata *gpd = (bGPdata *)ob->data;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
  if (gpl == nullptr) {
    return OPERATOR_CANCELLED;
  }
  if (gpl->act_mask > 0) {
    bGPDlayer_Mask *mask = static_cast<bGPDlayer_Mask *>(
        BLI_findlink(&gpl->mask_layers, gpl->act_mask - 1));
    if (mask != nullptr) {
      BKE_gpencil_layer_mask_remove(gpl, mask);
      if ((gpl->mask_layers.first != nullptr) && (gpl->act_mask == 0)) {
        gpl->act_mask = 1;
      }
    }
  }

  /* Reorder masking. */
  BKE_gpencil_layer_mask_sort(gpd, gpl);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_mask_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Mask Layer";
  ot->idname = "GPENCIL_OT_layer_mask_remove";
  ot->description = "Remove Layer Mask";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_layer_mask_remove_exec;
  ot->poll = gpencil_active_layer_poll;
}

static int gpencil_layer_mask_move_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
  const int direction = RNA_enum_get(op->ptr, "type");

  /* sanity checks */
  if (ELEM(nullptr, gpd, gpl)) {
    return OPERATOR_CANCELLED;
  }
  if (gpl->act_mask > 0) {
    bGPDlayer_Mask *mask = static_cast<bGPDlayer_Mask *>(
        BLI_findlink(&gpl->mask_layers, gpl->act_mask - 1));
    if (mask != nullptr) {
      BLI_assert(ELEM(direction, -1, 0, 1)); /* we use value below */
      if (BLI_listbase_link_move(&gpl->mask_layers, mask, direction)) {
        gpl->act_mask += direction;
        DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
        WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
      }
    }
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_mask_move(wmOperatorType *ot)
{
  static const EnumPropertyItem slot_move[] = {
      {GP_LAYER_MOVE_UP, "UP", 0, "Up", ""},
      {GP_LAYER_MOVE_DOWN, "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Grease Pencil Layer Mask";
  ot->idname = "GPENCIL_OT_layer_mask_move";
  ot->description = "Move the active Grease Pencil mask layer up/down in the list";

  /* api callbacks */
  ot->exec = gpencil_layer_mask_move_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}
