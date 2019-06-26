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
 * \ingroup RNA
 */

#include <stdlib.h>

#include "BLI_math.h"

#include "DNA_meshdata_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

/* parent type */
static const EnumPropertyItem parent_type_items[] = {
    {PAROBJECT, "OBJECT", 0, "Object", "The layer is parented to an object"},
    {PARSKEL, "ARMATURE", 0, "Armature", ""},
    {PARBONE, "BONE", 0, "Bone", "The layer is parented to a bone"},
    {0, NULL, 0, NULL, NULL},
};

#ifndef RNA_RUNTIME
static EnumPropertyItem rna_enum_gpencil_stroke_depth_order_items[] = {
    {GP_DRAWMODE_2D,
     "2D",
     0,
     "2D Layers",
     "Display strokes using grease pencil layers to define order"},
    {GP_DRAWMODE_3D, "3D", 0, "3D Location", "Display strokes using real 3D position in 3D space"},
    {0, NULL, 0, NULL, NULL},
};

static EnumPropertyItem rna_enum_gpencil_onion_modes_items[] = {
    {GP_ONION_MODE_ABSOLUTE,
     "ABSOLUTE",
     0,
     "Frames",
     "Frames in absolute range of the scene frame"},
    {GP_ONION_MODE_RELATIVE,
     "RELATIVE",
     0,
     "Keyframes",
     "Frames in relative range of the Grease Pencil keyframes"},
    {GP_ONION_MODE_SELECTED, "SELECTED", 0, "Selected", "Only selected keyframes"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_onion_keyframe_type_items[] = {
    {-1, "ALL", ICON_ACTION, "All Types", "Include all Keyframe types"},
    {BEZT_KEYTYPE_KEYFRAME,
     "KEYFRAME",
     ICON_KEYTYPE_KEYFRAME_VEC,
     "Keyframe",
     "Normal keyframe - e.g. for key poses"},
    {BEZT_KEYTYPE_BREAKDOWN,
     "BREAKDOWN",
     ICON_KEYTYPE_BREAKDOWN_VEC,
     "Breakdown",
     "A breakdown pose - e.g. for transitions between key poses"},
    {BEZT_KEYTYPE_MOVEHOLD,
     "MOVING_HOLD",
     ICON_KEYTYPE_MOVING_HOLD_VEC,
     "Moving Hold",
     "A keyframe that is part of a moving hold"},
    {BEZT_KEYTYPE_EXTREME,
     "EXTREME",
     ICON_KEYTYPE_EXTREME_VEC,
     "Extreme",
     "An 'extreme' pose, or some other purpose as needed"},
    {BEZT_KEYTYPE_JITTER,
     "JITTER",
     ICON_KEYTYPE_JITTER_VEC,
     "Jitter",
     "A filler or baked keyframe for keying on ones, or some other purpose as needed"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_gplayer_move_type_items[] = {
    {-1, "UP", 0, "Up", ""},
    {1, "DOWN", 0, "Down", ""},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_layer_blend_modes_items[] = {
    {eGplBlendMode_Regular, "REGULAR", 0, "Regular", ""},
    {eGplBlendMode_Overlay, "OVERLAY", 0, "Overlay", ""},
    {eGplBlendMode_Add, "ADD", 0, "Add", ""},
    {eGplBlendMode_Subtract, "SUBTRACT", 0, "Subtract", ""},
    {eGplBlendMode_Multiply, "MULTIPLY", 0, "Multiply", ""},
    {eGplBlendMode_Divide, "DIVIDE", 0, "Divide", ""},
    {0, NULL, 0, NULL, NULL}};

static EnumPropertyItem rna_enum_gpencil_caps_modes_items[] = {
    {GP_STROKE_CAP_ROUND, "ROUND", 0, "Rounded", ""},
    {GP_STROKE_CAP_FLAT, "FLAT", 0, "Flat", ""},
    {0, NULL, 0, NULL, NULL},
};
#endif

#ifdef RNA_RUNTIME

#  include "BLI_ghash.h"
#  include "BLI_string_utils.h"

#  include "WM_api.h"

#  include "BKE_action.h"
#  include "BKE_animsys.h"
#  include "BKE_gpencil.h"
#  include "BKE_icons.h"

#  include "DEG_depsgraph.h"

static void rna_GPencil_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->id.data, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
}

static void rna_GPencil_autolock(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->id.data;
  bGPDlayer *gpl = NULL;

  if (gpd->flag & GP_DATA_AUTOLOCK_LAYERS) {
    bGPDlayer *layer = BKE_gpencil_layer_getactive(gpd);

    /* Lock all other layers */
    for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
      /* unlock active layer */
      if (gpl == layer) {
        gpl->flag &= ~GP_LAYER_LOCKED;
      }
      else {
        gpl->flag |= GP_LAYER_LOCKED;
      }
    }
  }
  else {
    /* If disable is better unlock all layers by default or it looks there is
     * a problem in the UI because the user expects all layers will be unlocked
     */
    for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
      gpl->flag &= ~GP_LAYER_LOCKED;
    }
  }

  /* standard update */
  rna_GPencil_update(bmain, scene, ptr);
}

static void rna_GPencil_editmode_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->id.data;
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  /* Notify all places where GPencil data lives that the editing state is different */
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
  WM_main_add_notifier(NC_SCENE | ND_MODE | NC_MOVIECLIP, NULL);
}

static void UNUSED_FUNCTION(rna_GPencil_onion_skinning_update)(Main *bmain,
                                                               Scene *scene,
                                                               PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->id.data;
  bGPDlayer *gpl;
  bool enabled = false;

  /* Ensure that the data-block's onion-skinning toggle flag
   * stays in sync with the status of the actual layers
   */
  for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    if (gpl->onion_flag & GP_LAYER_ONIONSKIN) {
      enabled = true;
    }
  }

  if (enabled) {
    gpd->flag |= GP_DATA_SHOW_ONIONSKINS;
  }
  else {
    gpd->flag &= ~GP_DATA_SHOW_ONIONSKINS;
  }

  /* Now do standard updates... */
  rna_GPencil_update(bmain, scene, ptr);
}

/* Poll Callback to filter GP Datablocks to only show those for Annotations */
bool rna_GPencil_datablocks_annotations_poll(PointerRNA *UNUSED(ptr), const PointerRNA value)
{
  bGPdata *gpd = value.data;
  return (gpd->flag & GP_DATA_ANNOTATIONS) != 0;
}

/* Poll Callback to filter GP Datablocks to only show those for GP Objects */
bool rna_GPencil_datablocks_obdata_poll(PointerRNA *UNUSED(ptr), const PointerRNA value)
{
  bGPdata *gpd = value.data;
  return (gpd->flag & GP_DATA_ANNOTATIONS) == 0;
}

static char *rna_GPencilLayer_path(PointerRNA *ptr)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;
  char name_esc[sizeof(gpl->info) * 2];

  BLI_strescape(name_esc, gpl->info, sizeof(name_esc));

  return BLI_sprintfN("layers[\"%s\"]", name_esc);
}

static int rna_GPencilLayer_active_frame_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;

  /* surely there must be other criteria too... */
  if (gpl->flag & GP_LAYER_LOCKED) {
    return 0;
  }
  else {
    return PROP_EDITABLE;
  }
}

/* set parent */
static void set_parent(bGPDlayer *gpl, Object *par, const int type, const char *substr)
{
  if (type == PAROBJECT) {
    invert_m4_m4(gpl->inverse, par->obmat);
    gpl->parent = par;
    gpl->partype |= PAROBJECT;
    gpl->parsubstr[0] = 0;
  }
  else if (type == PARSKEL) {
    invert_m4_m4(gpl->inverse, par->obmat);
    gpl->parent = par;
    gpl->partype |= PARSKEL;
    gpl->parsubstr[0] = 0;
  }
  else if (type == PARBONE) {
    bPoseChannel *pchan = BKE_pose_channel_find_name(par->pose, substr);
    if (pchan) {
      float tmp_mat[4][4];
      mul_m4_m4m4(tmp_mat, par->obmat, pchan->pose_mat);

      invert_m4_m4(gpl->inverse, tmp_mat);
      gpl->parent = par;
      gpl->partype |= PARBONE;
      BLI_strncpy(gpl->parsubstr, substr, sizeof(gpl->parsubstr));
    }
  }
}

/* set parent object and inverse matrix */
static void rna_GPencilLayer_parent_set(PointerRNA *ptr,
                                        PointerRNA value,
                                        struct ReportList *UNUSED(reports))
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;
  Object *par = (Object *)value.data;

  if (par != NULL) {
    set_parent(gpl, par, gpl->partype, gpl->parsubstr);
  }
  else {
    /* clear parent */
    gpl->parent = NULL;
  }
}

/* set parent type */
static void rna_GPencilLayer_parent_type_set(PointerRNA *ptr, int value)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;
  Object *par = gpl->parent;
  gpl->partype = value;

  if (par != NULL) {
    set_parent(gpl, par, value, gpl->parsubstr);
  }
}

/* set parent bone */
static void rna_GPencilLayer_parent_bone_set(PointerRNA *ptr, const char *value)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;

  Object *par = gpl->parent;
  gpl->partype = PARBONE;

  if (par != NULL) {
    set_parent(gpl, par, gpl->partype, value);
  }
}

/* parent types enum */
static const EnumPropertyItem *rna_Object_parent_type_itemf(bContext *UNUSED(C),
                                                            PointerRNA *ptr,
                                                            PropertyRNA *UNUSED(prop),
                                                            bool *r_free)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;
  EnumPropertyItem *item = NULL;
  int totitem = 0;

  RNA_enum_items_add_value(&item, &totitem, parent_type_items, PAROBJECT);

  if (gpl->parent) {
    Object *par = gpl->parent;

    if (par->type == OB_ARMATURE) {
      /* special hack: prevents this being overridden */
      RNA_enum_items_add_value(&item, &totitem, &parent_type_items[1], PARSKEL);
      RNA_enum_items_add_value(&item, &totitem, parent_type_items, PARBONE);
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static bool rna_GPencilLayer_is_parented_get(PointerRNA *ptr)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;
  return (gpl->parent != NULL);
}

static PointerRNA rna_GPencil_active_layer_get(PointerRNA *ptr)
{
  bGPdata *gpd = ptr->id.data;

  if (GS(gpd->id.name) == ID_GD) { /* why would this ever be not GD */
    bGPDlayer *gl;

    for (gl = gpd->layers.first; gl; gl = gl->next) {
      if (gl->flag & GP_LAYER_ACTIVE) {
        break;
      }
    }

    if (gl) {
      return rna_pointer_inherit_refine(ptr, &RNA_GPencilLayer, gl);
    }
  }

  return rna_pointer_inherit_refine(ptr, NULL, NULL);
}

static void rna_GPencil_active_layer_set(PointerRNA *ptr,
                                         PointerRNA value,
                                         struct ReportList *UNUSED(reports))
{
  bGPdata *gpd = ptr->id.data;

  /* Don't allow setting active layer to NULL if layers exist
   * as this breaks various tools. Tools should be used instead
   * if it's necessary to remove layers
   */
  if (value.data == NULL) {
    printf("%s: Setting active layer to None is not allowed\n", __func__);
    return;
  }

  if (GS(gpd->id.name) == ID_GD) { /* why would this ever be not GD */
    bGPDlayer *gl;

    for (gl = gpd->layers.first; gl; gl = gl->next) {
      if (gl == value.data) {
        gl->flag |= GP_LAYER_ACTIVE;
      }
      else {
        gl->flag &= ~GP_LAYER_ACTIVE;
      }
    }

    WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
  }
}

static int rna_GPencil_active_layer_index_get(PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->id.data;
  bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);

  return BLI_findindex(&gpd->layers, gpl);
}

static void rna_GPencil_active_layer_index_set(PointerRNA *ptr, int value)
{
  bGPdata *gpd = (bGPdata *)ptr->id.data;
  bGPDlayer *gpl = BLI_findlink(&gpd->layers, value);

  BKE_gpencil_layer_setactive(gpd, gpl);

  /* Now do standard updates... */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, NULL);
}

static void rna_GPencil_active_layer_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  bGPdata *gpd = (bGPdata *)ptr->id.data;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&gpd->layers) - 1);

  *softmin = *min;
  *softmax = *max;
}

static const EnumPropertyItem *rna_GPencil_active_layer_itemf(bContext *C,
                                                              PointerRNA *ptr,
                                                              PropertyRNA *UNUSED(prop),
                                                              bool *r_free)
{
  bGPdata *gpd = (bGPdata *)ptr->id.data;
  bGPDlayer *gpl;
  EnumPropertyItem *item = NULL, item_tmp = {0};
  int totitem = 0;
  int i = 0;

  if (ELEM(NULL, C, gpd)) {
    return DummyRNA_NULL_items;
  }

  /* Existing layers */
  for (gpl = gpd->layers.first, i = 0; gpl; gpl = gpl->next, i++) {
    item_tmp.identifier = gpl->info;
    item_tmp.name = gpl->info;
    item_tmp.value = i;

    item_tmp.icon = BKE_icon_gplayer_color_ensure(gpl);

    RNA_enum_item_add(&item, &totitem, &item_tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static void rna_GPencilLayer_info_set(PointerRNA *ptr, const char *value)
{
  bGPdata *gpd = ptr->id.data;
  bGPDlayer *gpl = ptr->data;

  char oldname[128] = "";
  BLI_strncpy(oldname, gpl->info, sizeof(oldname));

  /* copy the new name into the name slot */
  BLI_strncpy_utf8(gpl->info, value, sizeof(gpl->info));

  BLI_uniquename(
      &gpd->layers, gpl, DATA_("GP_Layer"), '.', offsetof(bGPDlayer, info), sizeof(gpl->info));

  /* now fix animation paths */
  BKE_animdata_fix_paths_rename_all(&gpd->id, "layers", oldname, gpl->info);
}

static bGPDstroke *rna_GPencil_stroke_point_find_stroke(const bGPdata *gpd,
                                                        const bGPDspoint *pt,
                                                        bGPDlayer **r_gpl,
                                                        bGPDframe **r_gpf)
{
  bGPDlayer *gpl;
  bGPDstroke *gps;

  /* sanity checks */
  if (ELEM(NULL, gpd, pt)) {
    return NULL;
  }

  if (r_gpl) {
    *r_gpl = NULL;
  }
  if (r_gpf) {
    *r_gpf = NULL;
  }

  /* there's no faster alternative than just looping over everything... */
  for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    if (gpl->actframe) {
      for (gps = gpl->actframe->strokes.first; gps; gps = gps->next) {
        if ((pt >= gps->points) && (pt < &gps->points[gps->totpoints])) {
          /* found it */
          if (r_gpl) {
            *r_gpl = gpl;
          }
          if (r_gpf) {
            *r_gpf = gpl->actframe;
          }

          return gps;
        }
      }
    }
  }

  /* didn't find it */
  return NULL;
}

static void rna_GPencil_stroke_point_select_set(PointerRNA *ptr, const bool value)
{
  bGPdata *gpd = ptr->id.data;
  bGPDspoint *pt = ptr->data;
  bGPDstroke *gps = NULL;

  /* Ensure that corresponding stroke is set
   * - Since we don't have direct access, we're going to have to search
   * - We don't apply selection value unless we can find the corresponding
   *   stroke, so that they don't get out of sync
   */
  gps = rna_GPencil_stroke_point_find_stroke(gpd, pt, NULL, NULL);
  if (gps) {
    /* Set the new selection state for the point */
    if (value) {
      pt->flag |= GP_SPOINT_SELECT;
    }
    else {
      pt->flag &= ~GP_SPOINT_SELECT;
    }

    /* Check if the stroke should be selected or not... */
    BKE_gpencil_stroke_sync_selection(gps);
  }
}

static void rna_GPencil_stroke_point_add(
    ID *id, bGPDstroke *stroke, int count, float pressure, float strength)
{
  bGPdata *gpd = (bGPdata *)id;

  if (count > 0) {
    /* create space at the end of the array for extra points */
    stroke->points = MEM_recallocN_id(
        stroke->points, sizeof(bGPDspoint) * (stroke->totpoints + count), "gp_stroke_points");
    stroke->dvert = MEM_recallocN_id(
        stroke->dvert, sizeof(MDeformVert) * (stroke->totpoints + count), "gp_stroke_weight");

    /* init the pressure and strength values so that old scripts won't need to
     * be modified to give these initial values...
     */
    for (int i = 0; i < count; i++) {
      bGPDspoint *pt = stroke->points + (stroke->totpoints + i);
      MDeformVert *dvert = stroke->dvert + (stroke->totpoints + i);
      pt->pressure = pressure;
      pt->strength = strength;

      dvert->totweight = 0;
      dvert->dw = NULL;
    }

    stroke->totpoints += count;

    stroke->flag |= GP_STROKE_RECALC_GEOMETRY;

    gpd->flag |= GP_DATA_PYTHON_UPDATED;
    DEG_id_tag_update(&gpd->id,
                      ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);

    WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
  }
}

static void rna_GPencil_stroke_point_pop(ID *id,
                                         bGPDstroke *stroke,
                                         ReportList *reports,
                                         int index)
{
  bGPdata *gpd = (bGPdata *)id;
  bGPDspoint *pt_tmp = stroke->points;
  MDeformVert *pt_dvert = stroke->dvert;

  /* python style negative indexing */
  if (index < 0) {
    index += stroke->totpoints;
  }

  if (stroke->totpoints <= index || index < 0) {
    BKE_report(reports, RPT_ERROR, "GPencilStrokePoints.pop: index out of range");
    return;
  }

  stroke->totpoints--;

  stroke->points = MEM_callocN(sizeof(bGPDspoint) * stroke->totpoints, "gp_stroke_points");
  if (pt_dvert != NULL) {
    stroke->dvert = MEM_callocN(sizeof(MDeformVert) * stroke->totpoints, "gp_stroke_weights");
  }

  if (index > 0) {
    memcpy(stroke->points, pt_tmp, sizeof(bGPDspoint) * index);
    /* verify weight data is available */
    if (pt_dvert != NULL) {
      memcpy(stroke->dvert, pt_dvert, sizeof(MDeformVert) * index);
    }
  }

  if (index < stroke->totpoints) {
    memcpy(&stroke->points[index],
           &pt_tmp[index + 1],
           sizeof(bGPDspoint) * (stroke->totpoints - index));
    if (pt_dvert != NULL) {
      memcpy(&stroke->dvert[index],
             &pt_dvert[index + 1],
             sizeof(MDeformVert) * (stroke->totpoints - index));
    }
  }

  /* free temp buffer */
  MEM_freeN(pt_tmp);
  if (pt_dvert != NULL) {
    MEM_freeN(pt_dvert);
  }

  stroke->flag |= GP_STROKE_RECALC_GEOMETRY;

  gpd->flag |= GP_DATA_PYTHON_UPDATED;
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
}

static bGPDstroke *rna_GPencil_stroke_new(bGPDframe *frame)
{
  bGPDstroke *stroke = MEM_callocN(sizeof(bGPDstroke), "gp_stroke");
  stroke->gradient_f = 1.0f;
  ARRAY_SET_ITEMS(stroke->gradient_s, 1.0f, 1.0f);
  BLI_addtail(&frame->strokes, stroke);

  return stroke;
}

static void rna_GPencil_stroke_remove(bGPDframe *frame,
                                      ReportList *reports,
                                      PointerRNA *stroke_ptr)
{
  bGPDstroke *stroke = stroke_ptr->data;
  if (BLI_findindex(&frame->strokes, stroke) == -1) {
    BKE_report(reports, RPT_ERROR, "Stroke not found in grease pencil frame");
    return;
  }

  BLI_freelinkN(&frame->strokes, stroke);
  RNA_POINTER_INVALIDATE(stroke_ptr);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
}

static void rna_GPencil_stroke_select_set(PointerRNA *ptr, const bool value)
{
  bGPDstroke *gps = ptr->data;
  bGPDspoint *pt;
  int i;

  /* set new value */
  if (value) {
    gps->flag |= GP_STROKE_SELECT;
  }
  else {
    gps->flag &= ~GP_STROKE_SELECT;
  }

  /* ensure that the stroke's points are selected in the same way */
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    if (value) {
      pt->flag |= GP_SPOINT_SELECT;
    }
    else {
      pt->flag &= ~GP_SPOINT_SELECT;
    }
  }
}

static bGPDframe *rna_GPencil_frame_new(bGPDlayer *layer, ReportList *reports, int frame_number)
{
  bGPDframe *frame;

  if (BKE_gpencil_layer_find_frame(layer, frame_number)) {
    BKE_reportf(reports, RPT_ERROR, "Frame already exists on this frame number %d", frame_number);
    return NULL;
  }

  frame = BKE_gpencil_frame_addnew(layer, frame_number);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);

  return frame;
}

static void rna_GPencil_frame_remove(bGPDlayer *layer, ReportList *reports, PointerRNA *frame_ptr)
{
  bGPDframe *frame = frame_ptr->data;
  if (BLI_findindex(&layer->frames, frame) == -1) {
    BKE_report(reports, RPT_ERROR, "Frame not found in grease pencil layer");
    return;
  }

  BKE_gpencil_layer_delframe(layer, frame);
  RNA_POINTER_INVALIDATE(frame_ptr);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
}

static bGPDframe *rna_GPencil_frame_copy(bGPDlayer *layer, bGPDframe *src)
{
  bGPDframe *frame = BKE_gpencil_frame_duplicate(src);

  while (BKE_gpencil_layer_find_frame(layer, frame->framenum)) {
    frame->framenum++;
  }

  BLI_addtail(&layer->frames, frame);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);

  return frame;
}

static bGPDlayer *rna_GPencil_layer_new(bGPdata *gpd, const char *name, bool setactive)
{
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, name, setactive != 0);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return gpl;
}

static void rna_GPencil_layer_remove(bGPdata *gpd, ReportList *reports, PointerRNA *layer_ptr)
{
  bGPDlayer *layer = layer_ptr->data;
  if (BLI_findindex(&gpd->layers, layer) == -1) {
    BKE_report(reports, RPT_ERROR, "Layer not found in grease pencil data");
    return;
  }

  BKE_gpencil_layer_delete(gpd, layer);
  RNA_POINTER_INVALIDATE(layer_ptr);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

static void rna_GPencil_layer_move(bGPdata *gpd,
                                   ReportList *reports,
                                   PointerRNA *layer_ptr,
                                   int type)
{
  bGPDlayer *gpl = layer_ptr->data;
  if (BLI_findindex(&gpd->layers, gpl) == -1) {
    BKE_report(reports, RPT_ERROR, "Layer not found in grease pencil data");
    return;
  }

  BLI_assert(ELEM(type, -1, 0, 1)); /* we use value below */

  const int direction = type * -1;

  if (BLI_listbase_link_move(&gpd->layers, gpl, direction)) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

static void rna_GPencil_frame_clear(bGPDframe *frame)
{
  BKE_gpencil_free_strokes(frame);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

static void rna_GPencil_layer_clear(bGPDlayer *layer)
{
  BKE_gpencil_free_frames(layer);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

static void rna_GPencil_clear(bGPdata *gpd)
{
  BKE_gpencil_free_layers(&gpd->layers);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

static void rna_GpencilVertex_groups_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  bGPDstroke *gps = ptr->data;

  if (gps->dvert) {
    MDeformVert *dvert = gps->dvert;

    rna_iterator_array_begin(
        iter, (void *)dvert->dw, sizeof(MDeformWeight), dvert->totweight, 0, NULL);
  }
  else {
    rna_iterator_array_begin(iter, NULL, 0, 0, 0, NULL);
  }
}

static char *rna_GreasePencilGrid_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("grid");
}

#else

static void rna_def_gpencil_stroke_point(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilStrokePoint", NULL);
  RNA_def_struct_sdna(srna, "bGPDspoint");
  RNA_def_struct_ui_text(
      srna, "Grease Pencil Stroke Point", "Data point for freehand stroke curve");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "x");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Coordinates", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "pressure", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "pressure");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Pressure", "Pressure of tablet at point when drawing it");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "strength");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Strength", "Color intensity (alpha factor)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "uv_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "uv_fac");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "UV Factor", "Internal UV factor");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "uv_rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "uv_rot");
  RNA_def_property_range(prop, 0.0f, M_PI * 2);
  RNA_def_property_ui_text(prop, "UV Rotation", "Internal UV factor for dot mode");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SPOINT_SELECT);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_GPencil_stroke_point_select_set");
  RNA_def_property_ui_text(prop, "Select", "Point is selected for viewport editing");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
}

static void rna_def_gpencil_stroke_points_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "GPencilStrokePoints");
  srna = RNA_def_struct(brna, "GPencilStrokePoints", NULL);
  RNA_def_struct_sdna(srna, "bGPDstroke");
  RNA_def_struct_ui_text(
      srna, "Grease Pencil Stroke Points", "Collection of grease pencil stroke points");

  func = RNA_def_function(srna, "add", "rna_GPencil_stroke_point_add");
  RNA_def_function_ui_description(func, "Add a new grease pencil stroke point");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_int(
      func, "count", 1, 0, INT_MAX, "Number", "Number of points to add to the stroke", 0, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_float(func,
                "pressure",
                1.0f,
                0.0f,
                FLT_MAX,
                "Pressure",
                "Pressure for newly created points",
                0.0f,
                FLT_MAX);
  RNA_def_float(func,
                "strength",
                1.0f,
                0.0f,
                1.0f,
                "Strength",
                "Color intensity (alpha factor) for newly created points",
                0.0f,
                1.0f);

  func = RNA_def_function(srna, "pop", "rna_GPencil_stroke_point_pop");
  RNA_def_function_ui_description(func, "Remove a grease pencil stroke point");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  RNA_def_int(func, "index", -1, INT_MIN, INT_MAX, "Index", "point index", INT_MIN, INT_MAX);
}

/* This information is read only and it can be used by add-ons */
static void rna_def_gpencil_triangle(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilTriangle", NULL);
  RNA_def_struct_sdna(srna, "bGPDtriangle");
  RNA_def_struct_ui_text(srna, "Triangle", "Triangulation data for Grease Pencil fills");

  /* point v1 */
  prop = RNA_def_property(srna, "v1", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "verts[0]");
  RNA_def_property_ui_text(prop, "v1", "First triangle vertex index");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* point v2 */
  prop = RNA_def_property(srna, "v2", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "verts[1]");
  RNA_def_property_ui_text(prop, "v2", "Second triangle vertex index");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* point v3 */
  prop = RNA_def_property(srna, "v3", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "verts[2]");
  RNA_def_property_ui_text(prop, "v3", "Third triangle vertex index");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* texture coord for point v1 */
  prop = RNA_def_property(srna, "uv1", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, NULL, "uv[0]");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "uv1", "First triangle vertex texture coordinates");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* texture coord for point v2 */
  prop = RNA_def_property(srna, "uv2", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, NULL, "uv[1]");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "uv2", "Second triangle vertex texture coordinates");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* texture coord for point v3 */
  prop = RNA_def_property(srna, "uv3", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, NULL, "uv[2]");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "uv3", "Third triangle vertex texture coordinates");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_gpencil_mvert_group(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GpencilVertexGroupElement", NULL);
  RNA_def_struct_sdna(srna, "MDeformWeight");
  RNA_def_struct_ui_text(
      srna, "Vertex Group Element", "Weight value of a vertex in a vertex group");
  RNA_def_struct_ui_icon(srna, ICON_GROUP_VERTEX);

  /* we can't point to actual group, it is in the object and so
   * there is no unique group to point to, hence the index */
  prop = RNA_def_property(srna, "group", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "def_nr");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Group Index", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Weight", "Vertex Weight");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
}

static void rna_def_gpencil_stroke(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem stroke_display_mode_items[] = {
      {0, "SCREEN", 0, "Screen", "Stroke is in screen-space"},
      {GP_STROKE_3DSPACE, "3DSPACE", 0, "3D Space", "Stroke is in 3D-space"},
      {GP_STROKE_2DSPACE, "2DSPACE", 0, "2D Space", "Stroke is in 2D-space"},
      {GP_STROKE_2DIMAGE,
       "2DIMAGE",
       0,
       "2D Image",
       "Stroke is in 2D-space (but with special 'image' scaling)"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "GPencilStroke", NULL);
  RNA_def_struct_sdna(srna, "bGPDstroke");
  RNA_def_struct_ui_text(srna, "Grease Pencil Stroke", "Freehand curve defining part of a sketch");

  /* Points */
  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "points", "totpoints");
  RNA_def_property_struct_type(prop, "GPencilStrokePoint");
  RNA_def_property_ui_text(prop, "Stroke Points", "Stroke data points");
  rna_def_gpencil_stroke_points_api(brna, prop);

  /* vertex groups */
  prop = RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_GpencilVertex_groups_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "GpencilVertexGroupElement");
  RNA_def_property_ui_text(
      prop, "Groups", "Weights for the vertex groups this vertex is member of");

  /* Triangles */
  prop = RNA_def_property(srna, "triangles", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "triangles", "tot_triangles");
  RNA_def_property_struct_type(prop, "GPencilTriangle");
  RNA_def_property_ui_text(prop, "Triangles", "Triangulation data for HQ fill");

  /* Material Index */
  prop = RNA_def_property(srna, "material_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "mat_nr");
  RNA_def_property_ui_text(prop, "Material Index", "Index of material used in this stroke");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Settings */
  prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, stroke_display_mode_items);
  RNA_def_property_ui_text(prop, "Draw Mode", "Coordinate space that stroke is in");
  RNA_def_property_update(prop, 0, "rna_GPencil_update");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STROKE_SELECT);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_GPencil_stroke_select_set");
  RNA_def_property_ui_text(prop, "Select", "Stroke is selected for viewport editing");
  RNA_def_property_update(prop, 0, "rna_GPencil_update");

  /* Cyclic: Draw a line from end to start point */
  prop = RNA_def_property(srna, "draw_cyclic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STROKE_CYCLIC);
  RNA_def_property_ui_text(prop, "Cyclic", "Enable cyclic drawing, closing the stroke");
  RNA_def_property_update(prop, 0, "rna_GPencil_update");

  /* Caps mode */
  prop = RNA_def_property(srna, "start_cap_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "caps[0]");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_caps_modes_items);
  RNA_def_property_ui_text(prop, "Start Cap", "Stroke start extreme cap style");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "end_cap_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "caps[1]");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_caps_modes_items);
  RNA_def_property_ui_text(prop, "End Cap", "Stroke end extreme cap style");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* No fill: The stroke never must fill area and must use fill color as stroke color
   * (this is a special flag for fill brush). */
  prop = RNA_def_property(srna, "is_nofill_stroke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STROKE_NOFILL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "No Fill", "Special stroke to use as boundary for filling areas");
  RNA_def_property_update(prop, 0, "rna_GPencil_update");

  /* Line Thickness */
  prop = RNA_def_property(srna, "line_width", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "thickness");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_range(prop, 1, 10, 1, 0);
  RNA_def_property_ui_text(prop, "Thickness", "Thickness of stroke (in pixels)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* gradient control along y */
  prop = RNA_def_property(srna, "gradient_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "gradient_f");
  RNA_def_property_range(prop, 0.001f, 1.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(
      prop, "Border Opacity Factor", "Amount of gradient along section of stroke");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, 0);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* gradient shape ratio */
  prop = RNA_def_property(srna, "gradient_shape", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "gradient_s");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, 0.01f, 1.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Aspect Ratio", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
}

static void rna_def_gpencil_strokes_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "GPencilStrokes");
  srna = RNA_def_struct(brna, "GPencilStrokes", NULL);
  RNA_def_struct_sdna(srna, "bGPDframe");
  RNA_def_struct_ui_text(srna, "Grease Pencil Frames", "Collection of grease pencil stroke");

  func = RNA_def_function(srna, "new", "rna_GPencil_stroke_new");
  RNA_def_function_ui_description(func, "Add a new grease pencil stroke");
  parm = RNA_def_pointer(func, "stroke", "GPencilStroke", "", "The newly created stroke");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_GPencil_stroke_remove");
  RNA_def_function_ui_description(func, "Remove a grease pencil stroke");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "stroke", "GPencilStroke", "Stroke", "The stroke to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_gpencil_frame(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;

  srna = RNA_def_struct(brna, "GPencilFrame", NULL);
  RNA_def_struct_sdna(srna, "bGPDframe");
  RNA_def_struct_ui_text(
      srna, "Grease Pencil Frame", "Collection of related sketches on a particular frame");

  /* Strokes */
  prop = RNA_def_property(srna, "strokes", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "strokes", NULL);
  RNA_def_property_struct_type(prop, "GPencilStroke");
  RNA_def_property_ui_text(prop, "Strokes", "Freehand curves defining the sketch on this frame");
  rna_def_gpencil_strokes_api(brna, prop);

  /* Frame Number */
  prop = RNA_def_property(srna, "frame_number", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "framenum");
  /* XXX note: this cannot occur on the same frame as another sketch */
  RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "Frame Number", "The frame on which this sketch appears");

  /* Flags */
  prop = RNA_def_property(srna, "is_edited", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, NULL, "flag", GP_FRAME_PAINT); /* XXX should it be editable? */
  RNA_def_property_ui_text(prop, "Paint Lock", "Frame is being edited (painted on)");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_FRAME_SELECT);
  RNA_def_property_ui_text(prop, "Select", "Frame is selected for editing in the Dope Sheet");

  /* API */
  func = RNA_def_function(srna, "clear", "rna_GPencil_frame_clear");
  RNA_def_function_ui_description(func, "Remove all the grease pencil frame data");
}

static void rna_def_gpencil_frames_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "GPencilFrames");
  srna = RNA_def_struct(brna, "GPencilFrames", NULL);
  RNA_def_struct_sdna(srna, "bGPDlayer");
  RNA_def_struct_ui_text(srna, "Grease Pencil Frames", "Collection of grease pencil frames");

  func = RNA_def_function(srna, "new", "rna_GPencil_frame_new");
  RNA_def_function_ui_description(func, "Add a new grease pencil frame");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(func,
                     "frame_number",
                     1,
                     MINAFRAME,
                     MAXFRAME,
                     "Frame Number",
                     "The frame on which this sketch appears",
                     MINAFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "frame", "GPencilFrame", "", "The newly created frame");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_GPencil_frame_remove");
  RNA_def_function_ui_description(func, "Remove a grease pencil frame");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "frame", "GPencilFrame", "Frame", "The frame to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  func = RNA_def_function(srna, "copy", "rna_GPencil_frame_copy");
  RNA_def_function_ui_description(func, "Copy a grease pencil frame");
  parm = RNA_def_pointer(func, "source", "GPencilFrame", "Source", "The source frame");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "copy", "GPencilFrame", "", "The newly copied frame");
  RNA_def_function_return(func, parm);
}

static void rna_def_gpencil_layer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  static const float default_onion_color_b[] = {0.302f, 0.851f, 0.302f};
  static const float default_onion_color_a[] = {0.250f, 0.1f, 1.0f};

  srna = RNA_def_struct(brna, "GPencilLayer", NULL);
  RNA_def_struct_sdna(srna, "bGPDlayer");
  RNA_def_struct_ui_text(srna, "Grease Pencil Layer", "Collection of related sketches");
  RNA_def_struct_path_func(srna, "rna_GPencilLayer_path");

  /* Name */
  prop = RNA_def_property(srna, "info", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Info", "Layer name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_GPencilLayer_info_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_RENAME, NULL);

  /* Frames */
  prop = RNA_def_property(srna, "frames", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "frames", NULL);
  RNA_def_property_struct_type(prop, "GPencilFrame");
  RNA_def_property_ui_text(prop, "Frames", "Sketches for this layer on different frames");
  rna_def_gpencil_frames_api(brna, prop);

  /* Active Frame */
  prop = RNA_def_property(srna, "active_frame", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "actframe");
  RNA_def_property_ui_text(prop, "Active Frame", "Frame currently being displayed for this layer");
  RNA_def_property_editable_func(prop, "rna_GPencilLayer_active_frame_editable");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);

  /* Layer Opacity */
  prop = RNA_def_property(srna, "opacity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "opacity");
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(prop, "Opacity", "Layer Opacity");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* layer channel color (grease pencil)  */
  prop = RNA_def_property(srna, "channel_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Custom Channel Color", "Custom color for animation channel in Dopesheet");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Stroke Drawing Color (Annotations) */
  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Color", "Color for all strokes in this layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Line Thickness (Annotations) */
  prop = RNA_def_property(srna, "thickness", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "thickness");
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Thickness", "Thickness of annotation strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Tint Color */
  prop = RNA_def_property(srna, "tint_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "tintcolor");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Tint Color", "Color for tinting stroke colors");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Tint factor */
  prop = RNA_def_property(srna, "tint_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "tintcolor[3]");
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(prop, "Tint Factor", "Factor of tinting color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Line Thickness Change */
  prop = RNA_def_property(srna, "line_change", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "line_change");
  RNA_def_property_range(prop, -300, 300);
  RNA_def_property_ui_range(prop, -100, 100, 1.0, 1);
  RNA_def_property_ui_text(
      prop, "Thickness Change", "Thickness change to apply to current strokes (in pixels)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Onion-Skinning */
  prop = RNA_def_property(srna, "use_onion_skinning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "onion_flag", GP_LAYER_ONIONSKIN);
  RNA_def_property_ui_text(
      prop, "Onion Skinning", "Display onion skins before and after the current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_annotation_onion_skinning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "onion_flag", GP_LAYER_ONIONSKIN);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Onion Skinning", "Display annotation onion skins before and after the current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "annotation_onion_before_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "gstep");
  RNA_def_property_range(prop, -1, 120);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Frames Before", "Maximum number of frames to show before current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "annotation_onion_after_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "gstep_next");
  RNA_def_property_range(prop, -1, 120);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Frames After", "Maximum number of frames to show after current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "annotation_onion_before_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "gcolor_prev");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, default_onion_color_b);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Before Color", "Base color for ghosts before the active frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "annotation_onion_after_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "gcolor_next");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, default_onion_color_a);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "After Color", "Base color for ghosts after the active frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* pass index for compositing and modifiers */
  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_ui_text(prop, "Pass Index", "Index number for the \"Layer Index\" pass");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "viewlayer_render", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "viewlayername");
  RNA_def_property_ui_text(
      prop,
      "ViewLayer",
      "Only include Layer in this View Layer render output (leave blank to include always)");

  /* blend mode */
  prop = RNA_def_property(srna, "blend_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "blend_mode");
  RNA_def_property_enum_items(prop, rna_enum_layer_blend_modes_items);
  RNA_def_property_ui_text(prop, "Blend Mode", "Blend mode");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Flags */
  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide", "Set layer Visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "annotation_hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Hide", "Set annotation Visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_LOCKED);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_ui_text(
      prop, "Locked", "Protect layer from further editing and/or frame changes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "lock_frame", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_FRAMELOCK);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Frame Locked", "Lock current frame displayed by layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "lock_material", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GP_LAYER_UNLOCK_COLOR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Disallow Locked Materials Editing", "Avoids editing locked materials in the layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);

  prop = RNA_def_property(srna, "clamp_layer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_USE_MASK);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Clamp Layer", "Clamp any pixel outside underlying layers drawing");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* solo mode: Only display frames with keyframe */
  prop = RNA_def_property(srna, "use_solo_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_SOLO_MODE);
  RNA_def_property_ui_text(
      prop, "Solo Mode", "In Paint mode display only layers with keyframe in current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* exposed as layers.active */
#  if 0
  prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_ACTIVE);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_GPencilLayer_active_set");
  RNA_def_property_ui_text(prop, "Active", "Set active layer for editing");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, NULL);
#  endif

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_SELECT);
  RNA_def_property_ui_text(prop, "Select", "Layer is selected for editing in the Dope Sheet");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, "rna_GPencil_update");

  prop = RNA_def_property(srna, "show_points", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_DRAWDEBUG);
  RNA_def_property_ui_text(
      prop, "Show Points", "Draw the points which make up the strokes (for debugging purposes)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* In Front */
  prop = RNA_def_property(srna, "show_in_front", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GP_LAYER_NO_XRAY);
  RNA_def_property_ui_text(prop, "In Front", "Make the layer draw in front of objects");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Parent object */
  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(prop, NULL, "rna_GPencilLayer_parent_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_ui_text(prop, "Parent", "Parent Object");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* parent type */
  prop = RNA_def_property(srna, "parent_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "partype");
  RNA_def_property_enum_items(prop, parent_type_items);
  RNA_def_property_enum_funcs(
      prop, NULL, "rna_GPencilLayer_parent_type_set", "rna_Object_parent_type_itemf");
  RNA_def_property_ui_text(prop, "Parent Type", "Type of parent relation");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* parent bone */
  prop = RNA_def_property(srna, "parent_bone", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "parsubstr");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_GPencilLayer_parent_bone_set");
  RNA_def_property_ui_text(
      prop, "Parent Bone", "Name of parent bone in case of a bone parenting relation");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* matrix */
  prop = RNA_def_property(srna, "matrix_inverse", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, NULL, "inverse");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Inverse Matrix", "Parent inverse transformation matrix");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* read only parented flag */
  prop = RNA_def_property(srna, "is_parented", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_GPencilLayer_is_parented_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Is Parented", "True when the layer parent object is set");

  /* Layers API */
  func = RNA_def_function(srna, "clear", "rna_GPencil_layer_clear");
  RNA_def_function_ui_description(func, "Remove all the grease pencil layer data");
}

static void rna_def_gpencil_layers_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "GreasePencilLayers");
  srna = RNA_def_struct(brna, "GreasePencilLayers", NULL);
  RNA_def_struct_sdna(srna, "bGPdata");
  RNA_def_struct_ui_text(srna, "Grease Pencil Layers", "Collection of grease pencil layers");

  func = RNA_def_function(srna, "new", "rna_GPencil_layer_new");
  RNA_def_function_ui_description(func, "Add a new grease pencil layer");
  parm = RNA_def_string(func, "name", "GPencilLayer", MAX_NAME, "Name", "Name of the layer");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_boolean(
      func, "set_active", true, "Set Active", "Set the newly created layer to the active layer");
  parm = RNA_def_pointer(func, "layer", "GPencilLayer", "", "The newly created layer");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_GPencil_layer_remove");
  RNA_def_function_ui_description(func, "Remove a grease pencil layer");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "GPencilLayer", "", "The layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  func = RNA_def_function(srna, "move", "rna_GPencil_layer_move");
  RNA_def_function_ui_description(func, "Move a grease pencil layer in the layer stack");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "GPencilLayer", "", "The layer to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
  parm = RNA_def_enum(
      func, "type", rna_enum_gplayer_move_type_items, 1, "", "Direction of movement");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "GPencilLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_GPencil_active_layer_get", "rna_GPencil_active_layer_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Layer", "Active grease pencil layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, NULL);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_GPencil_active_layer_index_get",
                             "rna_GPencil_active_layer_index_set",
                             "rna_GPencil_active_layer_index_range");
  RNA_def_property_ui_text(prop, "Active Layer Index", "Index of active grease pencil layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, NULL);

  /* Active Layer - As an enum (for selecting active layer for annotations) */
  prop = RNA_def_property(srna, "active_note", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop,
                              "rna_GPencil_active_layer_index_get",
                              "rna_GPencil_active_layer_index_set",
                              "rna_GPencil_active_layer_itemf");
  RNA_def_property_enum_items(
      prop, DummyRNA_DEFAULT_items); /* purely dynamic, as it maps to user-data */
  RNA_def_property_ui_text(prop, "Active Note", "Note/Layer to add annotation strokes to");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
}

static void rna_def_gpencil_grid(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const float default_grid_color[] = {0.5f, 0.5f, 0.5f};

  srna = RNA_def_struct(brna, "GreasePencilGrid", NULL);
  RNA_def_struct_sdna(srna, "bGPgrid");
  RNA_def_struct_nested(brna, srna, "GreasePencil");

  RNA_def_struct_path_func(srna, "rna_GreasePencilGrid_path");
  RNA_def_struct_ui_text(
      srna, "Grid and Canvas Settings", "Settings for grid and canvas in 3D viewport");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "scale");
  RNA_def_property_range(prop, 0.01f, FLT_MAX);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Grid Scale", "Grid scale");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, default_grid_color);
  RNA_def_property_ui_text(prop, "Grid Color", "Color for grid lines");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "lines", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "lines");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_int_default(prop, GP_DEFAULT_GRID_LINES);
  RNA_def_property_ui_text(
      prop, "Grid Subdivisions", "Number of subdivisions in each side of symmetry line");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, NULL, "offset");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Offset", "Offset of the canvas");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
}

static void rna_def_gpencil_data(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;

  static float default_1[4] = {0.6f, 0.6f, 0.6f, 0.5f};
  static float onion_dft1[3] = {0.145098f, 0.419608f, 0.137255f}; /* green */
  static float onion_dft2[3] = {0.125490f, 0.082353f, 0.529412f}; /* blue */

  static const EnumPropertyItem stroke_thickness_items[] = {
      {0, "WORLDSPACE", 0, "World Space", "Set stroke thickness relative to the world space"},
      {GP_DATA_STROKE_KEEPTHICKNESS,
       "SCREENSPACE",
       0,
       "Screen Space",
       "Set stroke thickness relative to the screen space"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "GreasePencil", "ID");
  RNA_def_struct_sdna(srna, "bGPdata");
  RNA_def_struct_ui_text(srna, "Grease Pencil", "Freehand annotation sketchbook");
  RNA_def_struct_ui_icon(srna, ICON_OUTLINER_DATA_GREASEPENCIL);

  /* Layers */
  prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "layers", NULL);
  RNA_def_property_struct_type(prop, "GPencilLayer");
  RNA_def_property_ui_text(prop, "Layers", "");
  rna_def_gpencil_layers_api(brna, prop);

  /* Animation Data */
  rna_def_animdata_common(srna);

  /* materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.c */
  RNA_def_property_collection_funcs(
      prop, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "rna_IDMaterials_assign_int");

  /* Depth */
  prop = RNA_def_property(srna, "stroke_depth_order", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "draw_mode");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_stroke_depth_order_items);
  RNA_def_property_ui_text(
      prop, "Stroke Depth Order", "Defines how the strokes are ordered in 3D space");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Flags */
  prop = RNA_def_property(srna, "use_stroke_edit_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_STROKE_EDITMODE);
  RNA_def_property_ui_text(
      prop, "Stroke Edit Mode", "Edit Grease Pencil strokes instead of viewport data");
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, "rna_GPencil_editmode_update");

  prop = RNA_def_property(srna, "is_stroke_paint_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_STROKE_PAINTMODE);
  RNA_def_property_ui_text(prop, "Stroke Paint Mode", "Draw Grease Pencil strokes on click/drag");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, "rna_GPencil_editmode_update");

  prop = RNA_def_property(srna, "is_stroke_sculpt_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_STROKE_SCULPTMODE);
  RNA_def_property_ui_text(
      prop, "Stroke Sculpt Mode", "Sculpt Grease Pencil strokes instead of viewport data");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, "rna_GPencil_editmode_update");

  prop = RNA_def_property(srna, "is_stroke_weight_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_STROKE_WEIGHTMODE);
  RNA_def_property_ui_text(prop, "Stroke Weight Paint Mode", "Grease Pencil weight paint");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, "rna_GPencil_editmode_update");

  prop = RNA_def_property(srna, "use_onion_skinning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_SHOW_ONIONSKINS);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(
      prop, "Onion Skins", "Show ghosts of the keyframes before and after the current frame");
  RNA_def_property_update(
      prop, NC_SCREEN | NC_SCENE | ND_TOOLSETTINGS | ND_DATA | NC_GPENCIL, "rna_GPencil_update");

  prop = RNA_def_property(srna, "show_stroke_direction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_SHOW_DIRECTION);
  RNA_def_property_ui_text(prop,
                           "Show Direction",
                           "Show stroke drawing direction with a bigger green dot (start) "
                           "and smaller red dot (end) points");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "stroke_thickness_space", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, stroke_thickness_items);
  RNA_def_property_ui_text(
      prop, "Stroke Thickness", "Set stroke thickness in screen space or world space");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "pixel_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "pixfactor");
  RNA_def_property_range(prop, 0.1f, 30.0f);
  RNA_def_property_ui_range(prop, 0.1f, 30.0f, 1, 2);
  RNA_def_property_ui_text(
      prop,
      "Scale",
      "Scale conversion factor for pixel size (use larger values for thicker lines)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_multiedit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_STROKE_MULTIEDIT);
  RNA_def_property_ui_text(prop,
                           "MultiFrame",
                           "Edit strokes from multiple grease pencil keyframes at the same time "
                           "(keyframes must be selected to be included)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_force_fill_recalc", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_STROKE_FORCE_RECALC);
  RNA_def_property_ui_text(
      prop,
      "Force Fill Update",
      "Force recalc of fill data after use deformation modifiers (reduce FPS)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_adaptive_uv", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_UV_ADAPTIVE);
  RNA_def_property_ui_text(
      prop, "Adaptive UV", "Automatic UVs are calculated depending of the stroke size");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_autolock_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_AUTOLOCK_LAYERS);
  RNA_def_property_ui_text(
      prop,
      "Autolock Layers",
      "Lock automatically all layers except active one to avoid accidental changes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_autolock");

  prop = RNA_def_property(srna, "edit_line_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "line_color");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, default_1);
  RNA_def_property_ui_text(prop, "Edit Line Color", "Color for editing line");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* onion skinning */
  prop = RNA_def_property(srna, "ghost_before_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "gstep");
  RNA_def_property_range(prop, 0, 120);
  RNA_def_property_int_default(prop, 1);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, 0);
  RNA_def_property_ui_text(prop,
                           "Frames Before",
                           "Maximum number of frames to show before current frame "
                           "(0 = don't show any frames before current)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "ghost_after_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "gstep_next");
  RNA_def_property_range(prop, 0, 120);
  RNA_def_property_int_default(prop, 1);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, 0);
  RNA_def_property_ui_text(prop,
                           "Frames After",
                           "Maximum number of frames to show after current frame "
                           "(0 = don't show any frames after current)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_ghost_custom_colors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, NULL, "onion_flag", GP_ONION_GHOST_PREVCOL | GP_ONION_GHOST_NEXTCOL);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, 0);
  RNA_def_property_ui_text(prop, "Use Custom Ghost Colors", "Use custom colors for ghost frames");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "before_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "gcolor_prev");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, onion_dft1);
  RNA_def_property_ui_text(prop, "Before Color", "Base color for ghosts before the active frame");
  RNA_def_property_update(
      prop, NC_SCREEN | NC_SCENE | ND_TOOLSETTINGS | ND_DATA | NC_GPENCIL, "rna_GPencil_update");

  prop = RNA_def_property(srna, "after_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "gcolor_next");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, onion_dft2);
  RNA_def_property_ui_text(prop, "After Color", "Base color for ghosts after the active frame");
  RNA_def_property_update(
      prop, NC_SCREEN | NC_SCENE | ND_TOOLSETTINGS | ND_DATA | NC_GPENCIL, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_ghosts_always", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "onion_flag", GP_ONION_GHOST_ALWAYS);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, 0);
  RNA_def_property_ui_text(prop,
                           "Always Show Ghosts",
                           "Ghosts are shown in renders and animation playback. Useful for "
                           "special effects (e.g. motion blur)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "onion_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "onion_mode");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_onion_modes_items);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, 0);
  RNA_def_property_ui_text(prop, "Mode", "Mode to display frames");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "onion_keyframe_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "onion_keytype");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, 0);
  RNA_def_property_enum_items(prop, rna_enum_onion_keyframe_type_items);
  RNA_def_property_ui_text(prop, "Filter By Type", "Type of keyframe (for filtering)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_onion_fade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "onion_flag", GP_ONION_FADE);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, 0);
  RNA_def_property_ui_text(
      prop, "Fade", "Display onion keyframes with a fade in color transparency");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_onion_loop", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "onion_flag", GP_ONION_LOOP);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, 0);
  RNA_def_property_ui_text(prop,
                           "Loop",
                           "Display first onion keyframes using next frame color to show "
                           "indication of loop start frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "onion_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "onion_factor");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, 0);
  RNA_def_property_ui_text(prop, "Onion Opacity", "Change fade opacity of displayed onion frames");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "zdepth_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "zdepth_offset");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 5);
  RNA_def_property_ui_text(prop, "Surface Offset", "Offset amount when drawing in surface mode");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "is_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_ANNOTATIONS);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Annotation", "Current datablock is an annotation");

  /* Nested Structs */
  prop = RNA_def_property(srna, "grid", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "GreasePencilGrid");
  RNA_def_property_ui_text(
      prop, "Grid Settings", "Settings for grid and canvas in the 3D viewport");

  rna_def_gpencil_grid(brna);

  /* API Functions */
  func = RNA_def_function(srna, "clear", "rna_GPencil_clear");
  RNA_def_function_ui_description(func, "Remove all the Grease Pencil data");
}

/* --- */

void RNA_def_gpencil(BlenderRNA *brna)
{
  rna_def_gpencil_data(brna);

  rna_def_gpencil_layer(brna);
  rna_def_gpencil_frame(brna);

  rna_def_gpencil_stroke(brna);
  rna_def_gpencil_stroke_point(brna);
  rna_def_gpencil_triangle(brna);

  rna_def_gpencil_mvert_group(brna);
}

#endif
