/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_brush_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_types.hh"

/* parent type */
static const EnumPropertyItem parent_type_items[] = {
    {PAROBJECT, "OBJECT", 0, "Object", "The layer is parented to an object"},
    {PARSKEL, "ARMATURE", 0, "Armature", ""},
    {PARBONE, "BONE", 0, "Bone", "The layer is parented to a bone"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifndef RNA_RUNTIME
static EnumPropertyItem rna_enum_gpencil_stroke_depth_order_items[] = {
    {GP_DRAWMODE_2D,
     "2D",
     0,
     "2D Layers",
     "Display strokes using grease pencil layers to define order"},
    {GP_DRAWMODE_3D, "3D", 0, "3D Location", "Display strokes using real 3D position in 3D space"},
    {0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_keyframe_type_items[] = {
    {BEZT_KEYTYPE_KEYFRAME,
     "KEYFRAME",
     ICON_KEYTYPE_KEYFRAME_VEC,
     "Keyframe",
     "Normal keyframe, e.g. for key poses"},
    {BEZT_KEYTYPE_BREAKDOWN,
     "BREAKDOWN",
     ICON_KEYTYPE_BREAKDOWN_VEC,
     "Breakdown",
     "A breakdown pose, e.g. for transitions between key poses"},
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
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_onion_keyframe_type_items[] = {
    {-1, "ALL", 0, "All", "Include all Keyframe types"},
    {BEZT_KEYTYPE_KEYFRAME,
     "KEYFRAME",
     ICON_KEYTYPE_KEYFRAME_VEC,
     "Keyframe",
     "Normal keyframe, e.g. for key poses"},
    {BEZT_KEYTYPE_BREAKDOWN,
     "BREAKDOWN",
     ICON_KEYTYPE_BREAKDOWN_VEC,
     "Breakdown",
     "A breakdown pose, e.g. for transitions between key poses"},
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
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_gplayer_move_type_items[] = {
    {-1, "UP", 0, "Up", ""},
    {1, "DOWN", 0, "Down", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_layer_blend_modes_items[] = {
    {eGplBlendMode_Regular, "REGULAR", 0, "Regular", ""},
    {eGplBlendMode_HardLight, "HARDLIGHT", 0, "Hard Light", ""},
    {eGplBlendMode_Add, "ADD", 0, "Add", ""},
    {eGplBlendMode_Subtract, "SUBTRACT", 0, "Subtract", ""},
    {eGplBlendMode_Multiply, "MULTIPLY", 0, "Multiply", ""},
    {eGplBlendMode_Divide, "DIVIDE", 0, "Divide", ""},
    {0, nullptr, 0, nullptr, nullptr}};

static const EnumPropertyItem rna_enum_gpencil_caps_modes_items[] = {
    {GP_STROKE_CAP_ROUND, "ROUND", 0, "Rounded", ""},
    {GP_STROKE_CAP_FLAT, "FLAT", 0, "Flat", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BLI_ghash.h"
#  include "BLI_listbase.h"
#  include "BLI_string_utils.hh"

#  include "WM_api.hh"

#  include "BKE_action.h"
#  include "BKE_animsys.h"
#  include "BKE_deform.hh"
#  include "BKE_gpencil_curve_legacy.h"
#  include "BKE_gpencil_geom_legacy.h"
#  include "BKE_gpencil_legacy.h"
#  include "BKE_gpencil_update_cache_legacy.h"
#  include "BKE_icons.h"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

static void rna_GPencil_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
#  if 0
  /* In case a property on a layer changed, tag it with a light update. */
  if (ptr->type == &RNA_GPencilLayer) {
    BKE_gpencil_tag_light_update(
        (bGPdata *)(ptr->owner_id), (bGPDlayer *)(ptr->data), nullptr, nullptr);
  }
#  endif
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
}

static void rna_GpencilLayerMatrix_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;

  loc_eul_size_to_mat4(gpl->layer_mat, gpl->location, gpl->rotation, gpl->scale);
  invert_m4_m4(gpl->layer_invmat, gpl->layer_mat);

  rna_GPencil_update(bmain, scene, ptr);
}

static void rna_GPencil_curve_edit_mode_toggle(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ToolSettings *ts = scene->toolsettings;
  bGPdata *gpd = (bGPdata *)ptr->owner_id;

  /* Curve edit mode is turned on. */
  if (GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) {
    /* If the current select mode is segment and the Bezier mode is on, change
     * to Point because segment is not supported. */
    if (ts->gpencil_selectmode_edit == GP_SELECTMODE_SEGMENT) {
      ts->gpencil_selectmode_edit = GP_SELECTMODE_POINT;
    }

    BKE_gpencil_strokes_selected_update_editcurve(gpd);
  }
  /* Curve edit mode is turned off. */
  else {
    BKE_gpencil_strokes_selected_sync_selection_editcurve(gpd);
  }

  /* Standard update. */
  rna_GPencil_update(bmain, scene, ptr);
}

static void rna_GPencil_stroke_curve_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;

  if (GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      if (gpl->actframe != nullptr) {
        bGPDframe *gpf = gpl->actframe;
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          if (gps->editcurve != nullptr) {
            gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
            BKE_gpencil_stroke_geometry_update(gpd, gps);
          }
        }
      }
    }
  }

  rna_GPencil_update(bmain, scene, ptr);
}

static void rna_GPencil_stroke_curve_resolution_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;

  if (GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      if (gpl->actframe != nullptr) {
        bGPDframe *gpf = gpl->actframe;
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          if (gps->editcurve != nullptr) {
            gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
            BKE_gpencil_stroke_geometry_update(gpd, gps);
          }
        }
      }
    }
  }
  rna_GPencil_update(bmain, scene, ptr);
}

static void rna_GPencil_dependency_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_TRANSFORM);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_PARENT, ptr->owner_id);

  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
}

static void rna_GPencil_uv_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;
  /* Force to recalc the UVs. */
  bGPDstroke *gps = (bGPDstroke *)ptr->data;

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);

  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
}

static void rna_GPencil_autolock(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;
  BKE_gpencil_layer_autolock_set(gpd, true);

  /* standard update */
  rna_GPencil_update(bmain, scene, ptr);
}

static void rna_GPencil_editmode_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  /* Notify all places where GPencil data lives that the editing state is different */
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
  WM_main_add_notifier(NC_SCENE | ND_MODE | NC_MOVIECLIP, nullptr);
}

/* Poll Callback to filter GP Datablocks to only show those for Annotations */
bool rna_GPencil_datablocks_annotations_poll(PointerRNA * /*ptr*/, const PointerRNA value)
{
  bGPdata *gpd = static_cast<bGPdata *>(value.data);
  return (gpd->flag & GP_DATA_ANNOTATIONS) != 0;
}

/* Poll Callback to filter GP Datablocks to only show those for GP Objects */
bool rna_GPencil_datablocks_obdata_poll(PointerRNA * /*ptr*/, const PointerRNA value)
{
  bGPdata *gpd = static_cast<bGPdata *>(value.data);
  return (gpd->flag & GP_DATA_ANNOTATIONS) == 0;
}

static std::optional<std::string> rna_GPencilLayer_path(const PointerRNA *ptr)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;
  char name_esc[sizeof(gpl->info) * 2];

  BLI_str_escape(name_esc, gpl->info, sizeof(name_esc));

  return fmt::format("layers[\"{}\"]", name_esc);
}

static int rna_GPencilLayer_active_frame_editable(const PointerRNA *ptr, const char ** /*r_info*/)
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
    invert_m4_m4(gpl->inverse, par->object_to_world);
    gpl->parent = par;
    gpl->partype |= PAROBJECT;
    gpl->parsubstr[0] = 0;
  }
  else if (type == PARSKEL) {
    invert_m4_m4(gpl->inverse, par->object_to_world);
    gpl->parent = par;
    gpl->partype |= PARSKEL;
    gpl->parsubstr[0] = 0;
  }
  else if (type == PARBONE) {
    bPoseChannel *pchan = BKE_pose_channel_find_name(par->pose, substr);
    if (pchan) {
      float tmp_mat[4][4];
      mul_m4_m4m4(tmp_mat, par->object_to_world, pchan->pose_mat);

      invert_m4_m4(gpl->inverse, tmp_mat);
      gpl->parent = par;
      gpl->partype |= PARBONE;
      STRNCPY(gpl->parsubstr, substr);
    }
    else {
      invert_m4_m4(gpl->inverse, par->object_to_world);
      gpl->parent = par;
      gpl->partype |= PAROBJECT;
      gpl->parsubstr[0] = 0;
    }
  }
}

/* set parent object and inverse matrix */
static void rna_GPencilLayer_parent_set(PointerRNA *ptr,
                                        PointerRNA value,
                                        ReportList * /*reports*/)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;
  Object *par = (Object *)value.data;

  if (par != nullptr) {
    set_parent(gpl, par, gpl->partype, gpl->parsubstr);
  }
  else {
    /* clear parent */
    gpl->parent = nullptr;
  }
}

/* set parent type */
static void rna_GPencilLayer_parent_type_set(PointerRNA *ptr, int value)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;
  Object *par = gpl->parent;
  gpl->partype = value;

  if (par != nullptr) {
    set_parent(gpl, par, value, gpl->parsubstr);
  }
}

/* set parent bone */
static void rna_GPencilLayer_parent_bone_set(PointerRNA *ptr, const char *value)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;

  Object *par = gpl->parent;
  gpl->partype = PARBONE;

  if (par != nullptr) {
    set_parent(gpl, par, gpl->partype, value);
  }
}

static std::optional<std::string> rna_GPencilLayerMask_path(const PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
  bGPDlayer_Mask *mask = (bGPDlayer_Mask *)ptr->data;

  char gpl_info_esc[sizeof(gpl->info) * 2];
  char mask_name_esc[sizeof(mask->name) * 2];

  BLI_str_escape(gpl_info_esc, gpl->info, sizeof(gpl_info_esc));
  BLI_str_escape(mask_name_esc, mask->name, sizeof(mask_name_esc));

  return fmt::format("layers[\"{}\"].mask_layers[\"{}\"]", gpl_info_esc, mask_name_esc);
}

static int rna_GPencil_active_mask_index_get(PointerRNA *ptr)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;
  return gpl->act_mask - 1;
}

static void rna_GPencil_active_mask_index_set(PointerRNA *ptr, int value)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;
  gpl->act_mask = value + 1;
}

static void rna_GPencil_active_mask_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&gpl->mask_layers) - 1);
}

/* parent types enum */
static const EnumPropertyItem *rna_Object_parent_type_itemf(bContext * /*C*/,
                                                            PointerRNA *ptr,
                                                            PropertyRNA * /*prop*/,
                                                            bool *r_free)
{
  bGPDlayer *gpl = (bGPDlayer *)ptr->data;
  EnumPropertyItem *item = nullptr;
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
  return (gpl->parent != nullptr);
}

static PointerRNA rna_GPencil_active_layer_get(PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;

  if (GS(gpd->id.name) == ID_GD_LEGACY) { /* why would this ever be not GD */
    bGPDlayer *gl;

    for (gl = static_cast<bGPDlayer *>(gpd->layers.first); gl; gl = gl->next) {
      if (gl->flag & GP_LAYER_ACTIVE) {
        break;
      }
    }

    if (gl) {
      return rna_pointer_inherit_refine(ptr, &RNA_GPencilLayer, gl);
    }
  }

  return rna_pointer_inherit_refine(ptr, nullptr, nullptr);
}

static void rna_GPencil_active_layer_set(PointerRNA *ptr,
                                         PointerRNA value,
                                         ReportList * /*reports*/)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;

  /* Don't allow setting active layer to nullptr if layers exist
   * as this breaks various tools. Tools should be used instead
   * if it's necessary to remove layers
   */
  if (value.data == nullptr) {
    printf("%s: Setting active layer to None is not allowed\n", __func__);
    return;
  }

  if (GS(gpd->id.name) == ID_GD_LEGACY) { /* why would this ever be not GD */
    bGPDlayer *gl;

    for (gl = static_cast<bGPDlayer *>(gpd->layers.first); gl; gl = gl->next) {
      if (gl == value.data) {
        gl->flag |= GP_LAYER_ACTIVE;
      }
      else {
        gl->flag &= ~GP_LAYER_ACTIVE;
      }
    }

    WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
  }
}

static int rna_GPencil_active_layer_index_get(PointerRNA *ptr)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  return BLI_findindex(&gpd->layers, gpl);
}

static void rna_GPencil_active_layer_index_set(PointerRNA *ptr, int value)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;
  bGPDlayer *gpl = static_cast<bGPDlayer *>(BLI_findlink(&gpd->layers, value));

  BKE_gpencil_layer_active_set(gpd, gpl);

  /* Now do standard updates... */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, nullptr);
}

static void rna_GPencil_active_layer_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&gpd->layers) - 1);

  *softmin = *min;
  *softmax = *max;
}

static const EnumPropertyItem *rna_GPencil_active_layer_itemf(bContext *C,
                                                              PointerRNA *ptr,
                                                              PropertyRNA * /*prop*/,
                                                              bool *r_free)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;
  bGPDlayer *gpl;
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;
  int i = 0;

  if (ELEM(nullptr, C, gpd)) {
    return rna_enum_dummy_NULL_items;
  }

  /* Existing layers */
  for (gpl = static_cast<bGPDlayer *>(gpd->layers.first), i = 0; gpl; gpl = gpl->next, i++) {
    item_tmp.identifier = gpl->info;
    item_tmp.name = gpl->info;
    item_tmp.value = i;

    item_tmp.icon = (gpd->flag & GP_DATA_ANNOTATIONS) ? BKE_icon_gplayer_color_ensure(gpl) :
                                                        ICON_GREASEPENCIL;

    RNA_enum_item_add(&item, &totitem, &item_tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static void rna_GPencilLayer_info_set(PointerRNA *ptr, const char *value)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;
  bGPDlayer *gpl = static_cast<bGPDlayer *>(ptr->data);

  char oldname[128] = "";
  STRNCPY(oldname, gpl->info);

  /* copy the new name into the name slot */
  STRNCPY_UTF8(gpl->info, value);

  BLI_uniquename(
      &gpd->layers, gpl, DATA_("GP_Layer"), '.', offsetof(bGPDlayer, info), sizeof(gpl->info));

  /* now fix animation paths */
  BKE_animdata_fix_paths_rename_all(&gpd->id, "layers", oldname, gpl->info);

  /* Fix mask layers. */
  LISTBASE_FOREACH (bGPDlayer *, gpl_, &gpd->layers) {
    LISTBASE_FOREACH (bGPDlayer_Mask *, mask, &gpl_->mask_layers) {
      if (STREQ(mask->name, oldname)) {
        STRNCPY(mask->name, gpl->info);
      }
    }
  }
}

static void rna_GPencilLayer_mask_info_set(PointerRNA *ptr, const char *value)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;
  bGPDlayer_Mask *mask = static_cast<bGPDlayer_Mask *>(ptr->data);
  char oldname[128] = "";
  STRNCPY(oldname, mask->name);

  /* Really is changing the layer name. */
  bGPDlayer *gpl = BKE_gpencil_layer_named_get(gpd, oldname);
  if (gpl) {
    /* copy the new name into the name slot */
    STRNCPY_UTF8(gpl->info, value);

    BLI_uniquename(
        &gpd->layers, gpl, DATA_("GP_Layer"), '.', offsetof(bGPDlayer, info), sizeof(gpl->info));

    /* now fix animation paths */
    BKE_animdata_fix_paths_rename_all(&gpd->id, "layers", oldname, gpl->info);

    /* Fix mask layers. */
    LISTBASE_FOREACH (bGPDlayer *, gpl_, &gpd->layers) {
      LISTBASE_FOREACH (bGPDlayer_Mask *, mask_, &gpl_->mask_layers) {
        if (STREQ(mask_->name, oldname)) {
          STRNCPY(mask_->name, gpl->info);
        }
      }
    }
  }
}

static bGPDstroke *rna_GPencil_stroke_point_find_stroke(const bGPdata *gpd,
                                                        const bGPDspoint *pt,
                                                        bGPDlayer **r_gpl,
                                                        bGPDframe **r_gpf)
{
  bGPDlayer *gpl;
  bGPDstroke *gps;

  /* sanity checks */
  if (ELEM(nullptr, gpd, pt)) {
    return nullptr;
  }

  if (r_gpl) {
    *r_gpl = nullptr;
  }
  if (r_gpf) {
    *r_gpf = nullptr;
  }

  /* there's no faster alternative than just looping over everything... */
  for (gpl = static_cast<bGPDlayer *>(gpd->layers.first); gpl; gpl = gpl->next) {
    if (gpl->actframe) {
      for (gps = static_cast<bGPDstroke *>(gpl->actframe->strokes.first); gps; gps = gps->next) {
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
  return nullptr;
}

static void rna_GPencil_stroke_point_select_set(PointerRNA *ptr, const bool value)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;
  bGPDspoint *pt = static_cast<bGPDspoint *>(ptr->data);
  bGPDstroke *gps = nullptr;

  /* Ensure that corresponding stroke is set
   * - Since we don't have direct access, we're going to have to search
   * - We don't apply selection value unless we can find the corresponding
   *   stroke, so that they don't get out of sync
   */
  gps = rna_GPencil_stroke_point_find_stroke(gpd, pt, nullptr, nullptr);
  if (gps) {
    /* Set the new selection state for the point */
    if (value) {
      pt->flag |= GP_SPOINT_SELECT;
    }
    else {
      pt->flag &= ~GP_SPOINT_SELECT;
    }

    /* Check if the stroke should be selected or not... */
    BKE_gpencil_stroke_sync_selection(gpd, gps);
  }
}

static void rna_GPencil_stroke_point_add(
    ID *id, bGPDstroke *stroke, int count, float pressure, float strength)
{
  bGPdata *gpd = (bGPdata *)id;

  if (count > 0) {
    /* create space at the end of the array for extra points */
    stroke->points = static_cast<bGPDspoint *>(MEM_recallocN_id(
        stroke->points, sizeof(bGPDspoint) * (stroke->totpoints + count), "gp_stroke_points"));
    stroke->dvert = static_cast<MDeformVert *>(MEM_recallocN_id(
        stroke->dvert, sizeof(MDeformVert) * (stroke->totpoints + count), "gp_stroke_weight"));

    /* init the pressure and strength values so that old scripts won't need to
     * be modified to give these initial values...
     */
    for (int i = 0; i < count; i++) {
      bGPDspoint *pt = stroke->points + (stroke->totpoints + i);
      MDeformVert *dvert = stroke->dvert + (stroke->totpoints + i);
      pt->pressure = pressure;
      pt->strength = strength;

      dvert->totweight = 0;
      dvert->dw = nullptr;
    }

    stroke->totpoints += count;

    /* Calc geometry data. */
    BKE_gpencil_stroke_geometry_update(gpd, stroke);

    DEG_id_tag_update(&gpd->id,
                      ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);

    WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
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

  stroke->points = static_cast<bGPDspoint *>(
      MEM_callocN(sizeof(bGPDspoint) * stroke->totpoints, "gp_stroke_points"));
  if (pt_dvert != nullptr) {
    stroke->dvert = static_cast<MDeformVert *>(
        MEM_callocN(sizeof(MDeformVert) * stroke->totpoints, "gp_stroke_weights"));
  }

  if (index > 0) {
    memcpy(static_cast<void *>(stroke->points), pt_tmp, sizeof(bGPDspoint) * index);
    /* verify weight data is available */
    if (pt_dvert != nullptr) {
      memcpy(stroke->dvert, pt_dvert, sizeof(MDeformVert) * index);
    }
  }

  if (index < stroke->totpoints) {
    memcpy(static_cast<void *>(&stroke->points[index]),
           &pt_tmp[index + 1],
           sizeof(bGPDspoint) * (stroke->totpoints - index));
    if (pt_dvert != nullptr) {
      memcpy(&stroke->dvert[index],
             &pt_dvert[index + 1],
             sizeof(MDeformVert) * (stroke->totpoints - index));
    }
  }

  /* free temp buffer */
  MEM_freeN(pt_tmp);
  if (pt_dvert != nullptr) {
    MEM_freeN(pt_dvert);
  }

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, stroke);

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
}

static void rna_GPencil_stroke_point_update(ID *id, bGPDstroke *stroke)
{
  bGPdata *gpd = (bGPdata *)id;

  /* Calc geometry data. */
  if (stroke) {
    BKE_gpencil_stroke_geometry_update(gpd, stroke);

    DEG_id_tag_update(&gpd->id,
                      ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);

    WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
  }
}

static float rna_GPencilStrokePoints_weight_get(bGPDstroke *stroke,
                                                ReportList *reports,
                                                int vertex_group_index,
                                                int point_index)
{
  MDeformVert *dvert = stroke->dvert;
  if (dvert == nullptr) {
    BKE_report(reports, RPT_ERROR, "Groups: No groups for this stroke");
    return -1.0f;
  }

  if (stroke->totpoints <= point_index || point_index < 0) {
    BKE_report(reports, RPT_ERROR, "GPencilStrokePoints: index out of range");
    return -1.0f;
  }

  MDeformVert *pt_dvert = stroke->dvert + point_index;

  MDeformWeight *dw = BKE_defvert_find_index(pt_dvert, vertex_group_index);
  if (dw) {
    return dw->weight;
  }

  return -1.0f;
}

static void rna_GPencilStrokePoints_weight_set(
    bGPDstroke *stroke, ReportList *reports, int vertex_group_index, int point_index, float weight)
{
  BKE_gpencil_dvert_ensure(stroke);

  MDeformVert *dvert = stroke->dvert;
  if (dvert == nullptr) {
    BKE_report(reports, RPT_ERROR, "Groups: No groups for this stroke");
    return;
  }

  if (stroke->totpoints <= point_index || point_index < 0) {
    BKE_report(reports, RPT_ERROR, "GPencilStrokePoints: index out of range");
    return;
  }

  MDeformVert *pt_dvert = stroke->dvert + point_index;
  MDeformWeight *dw = BKE_defvert_ensure_index(pt_dvert, vertex_group_index);
  if (dw) {
    dw->weight = weight;
  }
}

static bGPDstroke *rna_GPencil_stroke_new(bGPDframe *frame)
{
  bGPDstroke *stroke = BKE_gpencil_stroke_new(0, 0, 1.0f);
  BLI_addtail(&frame->strokes, stroke);

  return stroke;
}

static void rna_GPencil_stroke_remove(ID *id,
                                      bGPDframe *frame,
                                      ReportList *reports,
                                      PointerRNA *stroke_ptr)
{
  bGPdata *gpd = (bGPdata *)id;

  bGPDstroke *stroke = static_cast<bGPDstroke *>(stroke_ptr->data);
  if (BLI_findindex(&frame->strokes, stroke) == -1) {
    BKE_report(reports, RPT_ERROR, "Stroke not found in grease pencil frame");
    return;
  }

  BLI_remlink(&frame->strokes, stroke);
  BKE_gpencil_free_stroke(stroke);
  RNA_POINTER_INVALIDATE(stroke_ptr);

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static void rna_GPencil_stroke_close(ID *id,
                                     bGPDframe *frame,
                                     ReportList *reports,
                                     PointerRNA *stroke_ptr)
{
  bGPdata *gpd = (bGPdata *)id;
  bGPDstroke *stroke = static_cast<bGPDstroke *>(stroke_ptr->data);
  if (BLI_findindex(&frame->strokes, stroke) == -1) {
    BKE_report(reports, RPT_ERROR, "Stroke not found in grease pencil frame");
    return;
  }

  BKE_gpencil_stroke_close(stroke);

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static void rna_GPencil_stroke_select_set(PointerRNA *ptr, const bool value)
{
  bGPdata *gpd = (bGPdata *)ptr->owner_id;
  bGPDstroke *gps = static_cast<bGPDstroke *>(ptr->data);
  bGPDspoint *pt;
  int i;

  /* set new value */
  if (value) {
    gps->flag |= GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_set(gpd, gps);
  }
  else {
    gps->flag &= ~GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_reset(gps);
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

static void rna_GPencil_curve_select_set(PointerRNA *ptr, const bool value)
{
  bGPDcurve *gpc = static_cast<bGPDcurve *>(ptr->data);

  /* Set new value. */
  if (value) {
    gpc->flag |= GP_CURVE_SELECT;
  }
  else {
    gpc->flag &= ~GP_CURVE_SELECT;
  }
  /* Ensure that the curves's points are selected in the same way. */
  for (int i = 0; i < gpc->tot_curve_points; i++) {
    bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
    BezTriple *bezt = &gpc_pt->bezt;
    if (value) {
      gpc_pt->flag |= GP_CURVE_POINT_SELECT;
      BEZT_SEL_ALL(bezt);
    }
    else {
      gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
      BEZT_DESEL_ALL(bezt);
    }
  }
}

static bGPDframe *rna_GPencil_frame_new(bGPDlayer *layer,
                                        ReportList *reports,
                                        int frame_number,
                                        bool active)
{
  bGPDframe *frame;

  if (BKE_gpencil_layer_frame_find(layer, frame_number)) {
    BKE_reportf(reports, RPT_ERROR, "Frame already exists on this frame number %d", frame_number);
    return nullptr;
  }

  frame = BKE_gpencil_frame_addnew(layer, frame_number);
  if (active) {
    layer->actframe = BKE_gpencil_layer_frame_get(layer, frame_number, GP_GETFRAME_USE_PREV);
  }
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);

  return frame;
}

static void rna_GPencil_frame_remove(bGPDlayer *layer, ReportList *reports, PointerRNA *frame_ptr)
{
  bGPDframe *frame = static_cast<bGPDframe *>(frame_ptr->data);
  if (BLI_findindex(&layer->frames, frame) == -1) {
    BKE_report(reports, RPT_ERROR, "Frame not found in grease pencil layer");
    return;
  }

  BKE_gpencil_layer_frame_delete(layer, frame);
  RNA_POINTER_INVALIDATE(frame_ptr);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
}

static bGPDframe *rna_GPencil_frame_copy(bGPDlayer *layer, bGPDframe *src)
{
  bGPDframe *frame = BKE_gpencil_frame_duplicate(src, true);

  while (BKE_gpencil_layer_frame_find(layer, frame->framenum)) {
    frame->framenum++;
  }

  BLI_addtail(&layer->frames, frame);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);

  return frame;
}

static bGPDlayer *rna_GPencil_layer_new(bGPdata *gpd, const char *name, bool setactive)
{
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, name, setactive != 0, false);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return gpl;
}

static void rna_GPencil_layer_remove(bGPdata *gpd, ReportList *reports, PointerRNA *layer_ptr)
{
  bGPDlayer *layer = static_cast<bGPDlayer *>(layer_ptr->data);
  if (BLI_findindex(&gpd->layers, layer) == -1) {
    BKE_report(reports, RPT_ERROR, "Layer not found in grease pencil data");
    return;
  }

  BKE_gpencil_layer_delete(gpd, layer);
  RNA_POINTER_INVALIDATE(layer_ptr);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static void rna_GPencil_layer_move(bGPdata *gpd,
                                   ReportList *reports,
                                   PointerRNA *layer_ptr,
                                   int type)
{
  bGPDlayer *gpl = static_cast<bGPDlayer *>(layer_ptr->data);
  if (BLI_findindex(&gpd->layers, gpl) == -1) {
    BKE_report(reports, RPT_ERROR, "Layer not found in grease pencil data");
    return;
  }

  BLI_assert(ELEM(type, -1, 0, 1)); /* we use value below */

  const int direction = type * -1;

  if (BLI_listbase_link_move(&gpd->layers, gpl, direction)) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static void rna_GPencil_layer_mask_add(bGPDlayer *gpl, PointerRNA *layer_ptr)
{
  bGPDlayer *gpl_mask = static_cast<bGPDlayer *>(layer_ptr->data);

  BKE_gpencil_layer_mask_add(gpl, gpl_mask->info);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static void rna_GPencil_layer_mask_remove(bGPDlayer *gpl,
                                          ReportList *reports,
                                          PointerRNA *mask_ptr)
{
  bGPDlayer_Mask *mask = static_cast<bGPDlayer_Mask *>(mask_ptr->data);
  if (BLI_findindex(&gpl->mask_layers, mask) == -1) {
    BKE_report(reports, RPT_ERROR, "Mask not found in mask list");
    return;
  }

  BKE_gpencil_layer_mask_remove(gpl, mask);
  RNA_POINTER_INVALIDATE(mask_ptr);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static void rna_GPencil_frame_clear(ID *id, bGPDframe *frame)
{
  BKE_gpencil_free_strokes(frame);

  bGPdata *gpd = (bGPdata *)id;
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static void rna_GPencil_layer_clear(ID *id, bGPDlayer *layer)
{
  BKE_gpencil_free_frames(layer);

  bGPdata *gpd = (bGPdata *)id;
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static void rna_GPencil_clear(bGPdata *gpd)
{
  BKE_gpencil_free_layers(&gpd->layers);

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static std::optional<std::string> rna_GreasePencilGrid_path(const PointerRNA * /*ptr*/)
{
  return "grid";
}

static void rna_GpencilCurvePoint_BezTriple_handle1_get(PointerRNA *ptr, float *values)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  copy_v3_v3(values, cpt->bezt.vec[0]);
}

static void rna_GpencilCurvePoint_BezTriple_handle1_set(PointerRNA *ptr, const float *values)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  copy_v3_v3(cpt->bezt.vec[0], values);
}

static bool rna_GpencilCurvePoint_BezTriple_handle1_select_get(PointerRNA *ptr)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  return cpt->bezt.f1;
}

static void rna_GpencilCurvePoint_BezTriple_handle1_select_set(PointerRNA *ptr, const bool value)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  cpt->bezt.f1 = value;
}

static void rna_GpencilCurvePoint_BezTriple_handle2_get(PointerRNA *ptr, float *values)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  copy_v3_v3(values, cpt->bezt.vec[2]);
}

static void rna_GpencilCurvePoint_BezTriple_handle2_set(PointerRNA *ptr, const float *values)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  copy_v3_v3(cpt->bezt.vec[2], values);
}

static bool rna_GpencilCurvePoint_BezTriple_handle2_select_get(PointerRNA *ptr)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  return cpt->bezt.f3;
}

static void rna_GpencilCurvePoint_BezTriple_handle2_select_set(PointerRNA *ptr, const bool value)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  cpt->bezt.f3 = value;
}

static void rna_GpencilCurvePoint_BezTriple_ctrlpoint_get(PointerRNA *ptr, float *values)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  copy_v3_v3(values, cpt->bezt.vec[1]);
}

static void rna_GpencilCurvePoint_BezTriple_ctrlpoint_set(PointerRNA *ptr, const float *values)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  copy_v3_v3(cpt->bezt.vec[1], values);
}

static bool rna_GpencilCurvePoint_BezTriple_ctrlpoint_select_get(PointerRNA *ptr)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  return cpt->bezt.f2;
}

static void rna_GpencilCurvePoint_BezTriple_ctrlpoint_select_set(PointerRNA *ptr, const bool value)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  cpt->bezt.f2 = value;
}

static bool rna_GpencilCurvePoint_BezTriple_hide_get(PointerRNA *ptr)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  return bool(cpt->bezt.hide);
}

static void rna_GpencilCurvePoint_BezTriple_hide_set(PointerRNA *ptr, const bool value)
{
  bGPDcurve_point *cpt = (bGPDcurve_point *)ptr->data;
  cpt->bezt.hide = value;
}

static bool rna_stroke_has_edit_curve_get(PointerRNA *ptr)
{
  bGPDstroke *gps = (bGPDstroke *)ptr->data;
  if (gps->editcurve != nullptr) {
    return true;
  }

  return false;
}

#else

static void rna_def_gpencil_stroke_point(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilStrokePoint", nullptr);
  RNA_def_struct_sdna(srna, "bGPDspoint");
  RNA_def_struct_ui_text(
      srna, "Grease Pencil Stroke Point", "Data point for freehand stroke curve");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Coordinates", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "pressure", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "pressure");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Pressure", "Pressure of tablet at point when drawing it");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "strength");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Strength", "Color intensity (alpha factor)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "uv_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "uv_fac");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "UV Factor", "Internal UV factor");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "uv_rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "uv_rot");
  RNA_def_property_range(prop, -M_PI_2, M_PI_2);
  RNA_def_property_ui_text(prop, "UV Rotation", "Internal UV factor for dot mode");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "uv_fill", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "uv_fill");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "UV Fill", "Internal UV factor for filling");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SPOINT_SELECT);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_GPencil_stroke_point_select_set");
  RNA_def_property_ui_text(prop, "Select", "Point is selected for viewport editing");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "time", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "time");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Time", "Time relative to stroke start");

  /* Vertex color. */
  prop = RNA_def_property(srna, "vertex_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "vert_color");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Vertex Color", "Color used to mix with point color to get final color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
}

static void rna_def_gpencil_stroke_points_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "GPencilStrokePoints");
  srna = RNA_def_struct(brna, "GPencilStrokePoints", nullptr);
  RNA_def_struct_sdna(srna, "bGPDstroke");
  RNA_def_struct_ui_text(
      srna, "Grease Pencil Stroke Points", "Collection of grease pencil stroke points");

  func = RNA_def_function(srna, "add", "rna_GPencil_stroke_point_add");
  RNA_def_function_ui_description(func, "Add a new grease pencil stroke point");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_int(
      func, "count", 1, 0, INT_MAX, "Number", "Number of points to add to the stroke", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
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

  func = RNA_def_function(srna, "update", "rna_GPencil_stroke_point_update");
  RNA_def_function_ui_description(func, "Recalculate internal triangulation data");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);

  func = RNA_def_function(srna, "weight_get", "rna_GPencilStrokePoints_weight_get");
  RNA_def_function_ui_description(func, "Get vertex group point weight");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_int(func,
              "vertex_group_index",
              0,
              0,
              INT_MAX,
              "Vertex Group Index",
              "Index of Vertex Group in the array of groups",
              0,
              INT_MAX);
  RNA_def_int(func,
              "point_index",
              0,
              0,
              INT_MAX,
              "Point Index",
              "Index of the Point in the array",
              0,
              INT_MAX);
  parm = RNA_def_float(
      func, "weight", 0, -FLT_MAX, FLT_MAX, "Weight", "Point Weight", -FLT_MAX, FLT_MAX);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "weight_set", "rna_GPencilStrokePoints_weight_set");
  RNA_def_function_ui_description(func, "Set vertex group point weight");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_int(func,
              "vertex_group_index",
              0,
              0,
              INT_MAX,
              "Vertex Group Index",
              "Index of Vertex Group in the array of groups",
              0,
              INT_MAX);
  RNA_def_int(func,
              "point_index",
              0,
              0,
              INT_MAX,
              "Point Index",
              "Index of the Point in the array",
              0,
              INT_MAX);
  RNA_def_float(func, "weight", 0, -FLT_MAX, FLT_MAX, "Weight", "Point Weight", -FLT_MAX, FLT_MAX);
}

/* This information is read only and it can be used by add-ons */
static void rna_def_gpencil_triangle(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilTriangle", nullptr);
  RNA_def_struct_sdna(srna, "bGPDtriangle");
  RNA_def_struct_ui_text(srna, "Triangle", "Triangulation data for Grease Pencil fills");

  /* point v1 */
  prop = RNA_def_property(srna, "v1", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "verts[0]");
  RNA_def_property_ui_text(prop, "v1", "First triangle vertex index");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* point v2 */
  prop = RNA_def_property(srna, "v2", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "verts[1]");
  RNA_def_property_ui_text(prop, "v2", "Second triangle vertex index");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* point v3 */
  prop = RNA_def_property(srna, "v3", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "verts[2]");
  RNA_def_property_ui_text(prop, "v3", "Third triangle vertex index");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_gpencil_curve_point(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilEditCurvePoint", nullptr);
  RNA_def_struct_sdna(srna, "bGPDcurve_point");
  RNA_def_struct_ui_text(srna, "Bézier Curve Point", "Bézier curve point with two handles");

  /* Boolean values */
  prop = RNA_def_property(srna, "select_left_handle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_GpencilCurvePoint_BezTriple_handle1_select_get",
                                 "rna_GpencilCurvePoint_BezTriple_handle1_select_set");
  RNA_def_property_ui_text(prop, "Handle 1 selected", "Handle 1 selection status");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "select_right_handle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_GpencilCurvePoint_BezTriple_handle2_select_get",
                                 "rna_GpencilCurvePoint_BezTriple_handle2_select_set");
  RNA_def_property_ui_text(prop, "Handle 2 selected", "Handle 2 selection status");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "select_control_point", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_GpencilCurvePoint_BezTriple_ctrlpoint_select_get",
                                 "rna_GpencilCurvePoint_BezTriple_ctrlpoint_select_set");
  RNA_def_property_ui_text(prop, "Control Point selected", "Control point selection status");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_GpencilCurvePoint_BezTriple_hide_get",
                                 "rna_GpencilCurvePoint_BezTriple_hide_set");
  RNA_def_property_ui_text(prop, "Hide", "Visibility status");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Vector values */
  prop = RNA_def_property(srna, "handle_left", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop,
                               "rna_GpencilCurvePoint_BezTriple_handle1_get",
                               "rna_GpencilCurvePoint_BezTriple_handle1_set",
                               nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Handle 1", "Coordinates of the first handle");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_stroke_curve_update");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop,
                               "rna_GpencilCurvePoint_BezTriple_ctrlpoint_get",
                               "rna_GpencilCurvePoint_BezTriple_ctrlpoint_set",
                               nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Control Point", "Coordinates of the control point");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_stroke_curve_update");

  prop = RNA_def_property(srna, "handle_right", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop,
                               "rna_GpencilCurvePoint_BezTriple_handle2_get",
                               "rna_GpencilCurvePoint_BezTriple_handle2_set",
                               nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Handle 2", "Coordinates of the second handle");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_stroke_curve_update");

  /* Pressure */
  prop = RNA_def_property(srna, "pressure", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "pressure");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Pressure", "Pressure of the grease pencil stroke point");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_stroke_curve_update");

  /* Strength */
  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "strength");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Strength", "Color intensity (alpha factor) of the grease pencil stroke point");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_stroke_curve_update");

  /* read-only index */
  prop = RNA_def_property(srna, "point_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "point_index");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Point Index", "Index of the corresponding grease pencil stroke point");

  prop = RNA_def_property(srna, "uv_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "uv_fac");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "UV Factor", "Internal UV factor");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_stroke_curve_update");

  prop = RNA_def_property(srna, "uv_rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "uv_rot");
  RNA_def_property_range(prop, -M_PI_2, M_PI_2);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "UV Rotation", "Internal UV factor for dot mode");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_stroke_curve_update");

  prop = RNA_def_property(srna, "vertex_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "vert_color");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Vertex Color", "Vertex color of the grease pencil stroke point");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_stroke_curve_update");
}

/* Editing Curve data. */
static void rna_def_gpencil_curve(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilEditCurve", nullptr);
  RNA_def_struct_sdna(srna, "bGPDcurve");
  RNA_def_struct_ui_text(srna, "Edit Curve", "Edition Curve");

  prop = RNA_def_property(srna, "curve_points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "curve_points", "tot_curve_points");
  RNA_def_property_struct_type(prop, "GPencilEditCurvePoint");
  RNA_def_property_ui_text(prop, "Curve Points", "Curve data points");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_CURVE_SELECT);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_GPencil_curve_select_set");
  RNA_def_property_ui_text(prop, "Select", "Curve is selected for viewport editing");
  RNA_def_property_update(prop, 0, "rna_GPencil_update");
}

static void rna_def_gpencil_mvert_group(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GpencilVertexGroupElement", nullptr);
  RNA_def_struct_sdna(srna, "MDeformWeight");
  RNA_def_struct_ui_text(
      srna, "Vertex Group Element", "Weight value of a vertex in a vertex group");
  RNA_def_struct_ui_icon(srna, ICON_GROUP_VERTEX);

  /* we can't point to actual group, it is in the object and so
   * there is no unique group to point to, hence the index */
  prop = RNA_def_property(srna, "group", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "def_nr");
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "GPencilStroke", nullptr);
  RNA_def_struct_sdna(srna, "bGPDstroke");
  RNA_def_struct_ui_text(srna, "Grease Pencil Stroke", "Freehand curve defining part of a sketch");

  /* Points */
  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "points", "totpoints");
  RNA_def_property_struct_type(prop, "GPencilStrokePoint");
  RNA_def_property_ui_text(prop, "Stroke Points", "Stroke data points");
  rna_def_gpencil_stroke_points_api(brna, prop);

  /* Triangles */
  prop = RNA_def_property(srna, "triangles", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "triangles", "tot_triangles");
  RNA_def_property_struct_type(prop, "GPencilTriangle");
  RNA_def_property_ui_text(prop, "Triangles", "Triangulation data for HQ fill");

  /* Edit Curve. */
  prop = RNA_def_property(srna, "edit_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "editcurve");
  RNA_def_property_struct_type(prop, "GPencilEditCurve");
  RNA_def_property_ui_text(prop, "Edit Curve", "Temporary data for Edit Curve");

  /* Material Index */
  prop = RNA_def_property(srna, "material_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "mat_nr");
  RNA_def_property_ui_text(prop, "Material Index", "Material slot index of this stroke");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Settings */
  prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, stroke_display_mode_items);
  RNA_def_property_ui_text(prop, "Display Mode", "Coordinate space that stroke is in");
  RNA_def_property_update(prop, 0, "rna_GPencil_update");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_STROKE_SELECT);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_GPencil_stroke_select_set");
  RNA_def_property_ui_text(prop, "Select", "Stroke is selected for viewport editing");
  RNA_def_property_update(prop, 0, "rna_GPencil_update");

  /* Cyclic: Draw a line from end to start point */
  prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_STROKE_CYCLIC);
  RNA_def_property_ui_text(prop, "Cyclic", "Enable cyclic drawing, closing the stroke");
  RNA_def_property_update(prop, 0, "rna_GPencil_update");

  /* The stroke has Curve Edit data. */
  prop = RNA_def_property(srna, "has_edit_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_stroke_has_edit_curve_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Has Curve Data", "Stroke has Curve data to edit shape");

  /* Caps mode */
  prop = RNA_def_property(srna, "start_cap_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "caps[0]");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_caps_modes_items);
  RNA_def_property_ui_text(prop, "Start Cap", "Stroke start extreme cap style");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "end_cap_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "caps[1]");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_caps_modes_items);
  RNA_def_property_ui_text(prop, "End Cap", "Stroke end extreme cap style");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* No fill: The stroke never must fill area and must use fill color as stroke color
   * (this is a special flag for fill brush). */
  prop = RNA_def_property(srna, "is_nofill_stroke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_STROKE_NOFILL);
  RNA_def_property_ui_text(prop, "No Fill", "Special stroke to use as boundary for filling areas");
  RNA_def_property_update(prop, 0, "rna_GPencil_update");

  /* Line Thickness */
  prop = RNA_def_property(srna, "line_width", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "thickness");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_range(prop, 1, 10, 1, 0);
  RNA_def_property_ui_text(prop, "Thickness", "Thickness of stroke (in pixels)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* gradient control along y */
  prop = RNA_def_property(srna, "hardness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "hardness");
  RNA_def_property_range(prop, 0.001f, 1.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Hardness", "Amount of gradient along section of stroke");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Stroke bound box */
  prop = RNA_def_property(srna, "bound_box_min", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "boundbox_min");
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Boundbox Min", "");

  prop = RNA_def_property(srna, "bound_box_max", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "boundbox_max");
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Boundbox Max", "");

  /* gradient shape ratio */
  prop = RNA_def_property(srna, "aspect", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "aspect_ratio");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, 0.01f, 1.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Aspect", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* UV translation. */
  prop = RNA_def_property(srna, "uv_translation", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "uv_translation");
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(prop, "UV Translation", "Translation of default UV position");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_uv_update");

  /* UV rotation. */
  prop = RNA_def_property(srna, "uv_rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "uv_rotation");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(prop, "UV Rotation", "Rotation of the UV");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_uv_update");

  /* UV scale. */
  prop = RNA_def_property(srna, "uv_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "uv_scale");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.01f, 100.0f);
  RNA_def_property_ui_text(prop, "UV Scale", "Scale of the UV");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_uv_update");

  /* Vertex Color for Fill. */
  prop = RNA_def_property(srna, "vertex_color_fill", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "vert_color_fill");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Vertex Fill Color", "Color used to mix with fill color to get final color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Selection Index */
  prop = RNA_def_property(srna, "select_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "select_index");
  RNA_def_property_ui_text(prop, "Select Index", "Index of selection used for interpolation");

  /* Init time */
  prop = RNA_def_property(srna, "time_start", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "inittime");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Init Time", "Initial time of the stroke");
}

static void rna_def_gpencil_strokes_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "GPencilStrokes");
  srna = RNA_def_struct(brna, "GPencilStrokes", nullptr);
  RNA_def_struct_sdna(srna, "bGPDframe");
  RNA_def_struct_ui_text(srna, "Grease Pencil Frames", "Collection of grease pencil stroke");

  func = RNA_def_function(srna, "new", "rna_GPencil_stroke_new");
  RNA_def_function_ui_description(func, "Add a new grease pencil stroke");
  parm = RNA_def_pointer(func, "stroke", "GPencilStroke", "", "The newly created stroke");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_GPencil_stroke_remove");
  RNA_def_function_ui_description(func, "Remove a grease pencil stroke");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  parm = RNA_def_pointer(func, "stroke", "GPencilStroke", "Stroke", "The stroke to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "close", "rna_GPencil_stroke_close");
  RNA_def_function_ui_description(func, "Close a grease pencil stroke adding geometry");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  parm = RNA_def_pointer(func, "stroke", "GPencilStroke", "Stroke", "The stroke to close");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_gpencil_frame(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;

  srna = RNA_def_struct(brna, "GPencilFrame", nullptr);
  RNA_def_struct_sdna(srna, "bGPDframe");
  RNA_def_struct_ui_text(
      srna, "Grease Pencil Frame", "Collection of related sketches on a particular frame");

  /* Strokes */
  prop = RNA_def_property(srna, "strokes", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "strokes", nullptr);
  RNA_def_property_struct_type(prop, "GPencilStroke");
  RNA_def_property_ui_text(prop, "Strokes", "Freehand curves defining the sketch on this frame");
  rna_def_gpencil_strokes_api(brna, prop);

  /* Frame Number */
  prop = RNA_def_property(srna, "frame_number", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "framenum");
  /* XXX NOTE: this cannot occur on the same frame as another sketch. */
  RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "Frame Number", "The frame on which this sketch appears");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "keyframe_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "key_type");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_enum_items(prop, rna_enum_keyframe_type_items);
  RNA_def_property_ui_text(prop, "Keyframe Type", "Type of keyframe");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Flags */
  prop = RNA_def_property(srna, "is_edited", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "flag", GP_FRAME_PAINT); /* XXX should it be editable? */
  RNA_def_property_ui_text(prop, "Paint Lock", "Frame is being edited (painted on)");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_FRAME_SELECT);
  RNA_def_property_ui_text(prop, "Select", "Frame is selected for editing in the Dope Sheet");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* API */
  func = RNA_def_function(srna, "clear", "rna_GPencil_frame_clear");
  RNA_def_function_ui_description(func, "Remove all the grease pencil frame data");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
}

static void rna_def_gpencil_frames_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "GPencilFrames");
  srna = RNA_def_struct(brna, "GPencilFrames", nullptr);
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
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "active", false, "Active", "");
  parm = RNA_def_pointer(func, "frame", "GPencilFrame", "", "The newly created frame");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_GPencil_frame_remove");
  RNA_def_function_ui_description(func, "Remove a grease pencil frame");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "frame", "GPencilFrame", "Frame", "The frame to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "copy", "rna_GPencil_frame_copy");
  RNA_def_function_ui_description(func, "Copy a grease pencil frame");
  parm = RNA_def_pointer(func, "source", "GPencilFrame", "Source", "The source frame");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "copy", "GPencilFrame", "", "The newly copied frame");
  RNA_def_function_return(func, parm);
}

static void rna_def_gpencil_layers_mask_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "GreasePencilMaskLayers");
  srna = RNA_def_struct(brna, "GreasePencilMaskLayers", nullptr);
  RNA_def_struct_sdna(srna, "bGPDlayer");
  RNA_def_struct_ui_text(
      srna, "Grease Pencil Mask Layers", "Collection of grease pencil masking layers");

  prop = RNA_def_property(srna, "active_mask_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_GPencil_active_mask_index_get",
                             "rna_GPencil_active_mask_index_set",
                             "rna_GPencil_active_mask_index_range");
  RNA_def_property_ui_text(prop, "Active Layer Mask Index", "Active index in layer mask array");

  func = RNA_def_function(srna, "add", "rna_GPencil_layer_mask_add");
  RNA_def_function_ui_description(func, "Add a layer to mask list");
  parm = RNA_def_pointer(func, "layer", "GPencilLayer", "", "Layer to add as mask");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "remove", "rna_GPencil_layer_mask_remove");
  RNA_def_function_ui_description(func, "Remove a layer from mask list");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "mask", "GPencilLayerMask", "", "Mask to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_gpencil_layer_mask(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilLayerMask", nullptr);
  RNA_def_struct_sdna(srna, "bGPDlayer_Mask");
  RNA_def_struct_ui_text(srna, "Grease Pencil Masking Layers", "List of Mask Layers");
  RNA_def_struct_path_func(srna, "rna_GPencilLayerMask_path");

  /* Name */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Layer", "Mask layer name");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_GPencilLayer_mask_info_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_RENAME, nullptr);

  /* Flags */
  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MASK_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide", "Set mask Visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MASK_INVERT);
  RNA_def_property_ui_icon(prop, ICON_SELECT_INTERSECT, 1);
  RNA_def_property_ui_text(prop, "Invert", "Invert mask");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
}

static void rna_def_gpencil_layer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  static const float default_onion_color_b[] = {0.302f, 0.851f, 0.302f};
  static const float default_onion_color_a[] = {0.250f, 0.1f, 1.0f};

  srna = RNA_def_struct(brna, "GPencilLayer", nullptr);
  RNA_def_struct_sdna(srna, "bGPDlayer");
  RNA_def_struct_ui_text(srna, "Grease Pencil Layer", "Collection of related sketches");
  RNA_def_struct_path_func(srna, "rna_GPencilLayer_path");

  /* Name */
  prop = RNA_def_property(srna, "info", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Info", "Layer name");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_GPencilLayer_info_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_RENAME, "rna_GPencil_update");

  /* Frames */
  prop = RNA_def_property(srna, "frames", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "frames", nullptr);
  RNA_def_property_struct_type(prop, "GPencilFrame");
  RNA_def_property_ui_text(prop, "Frames", "Sketches for this layer on different frames");
  rna_def_gpencil_frames_api(brna, prop);

  /* Mask Layers */
  prop = RNA_def_property(srna, "mask_layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "mask_layers", nullptr);
  RNA_def_property_struct_type(prop, "GPencilLayerMask");
  RNA_def_property_ui_text(prop, "Masks", "List of Masking Layers");
  rna_def_gpencil_layers_mask_api(brna, prop);

  /* Active Frame */
  prop = RNA_def_property(srna, "active_frame", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "actframe");
  RNA_def_property_ui_text(prop, "Active Frame", "Frame currently being displayed for this layer");
  RNA_def_property_editable_func(prop, "rna_GPencilLayer_active_frame_editable");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Layer Opacity */
  prop = RNA_def_property(srna, "opacity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "opacity");
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(prop, "Opacity", "Layer Opacity");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Layer channel color (grease pencil). */
  prop = RNA_def_property(srna, "channel_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Custom Channel Color", "Custom color for animation channel in Dopesheet");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Layer Opacity (Annotations). */
  prop = RNA_def_property(srna, "annotation_opacity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "opacity");
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Opacity", "Annotation Layer Opacity");
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
  RNA_def_property_int_sdna(prop, nullptr, "thickness");
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Thickness", "Thickness of annotation strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Tint Color */
  prop = RNA_def_property(srna, "tint_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "tintcolor");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Tint Color", "Color for tinting stroke colors");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Tint factor */
  prop = RNA_def_property(srna, "tint_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "tintcolor[3]");
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(prop, "Tint Factor", "Factor of tinting color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Vertex Paint opacity factor */
  prop = RNA_def_property(srna, "vertex_paint_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "vertex_paint_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Vertex Paint Opacity", "Vertex Paint mix factor");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Line Thickness Change */
  prop = RNA_def_property(srna, "line_change", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "line_change");
  RNA_def_property_range(prop, -300, 300);
  RNA_def_property_ui_range(prop, -100, 100, 1.0, 1);
  RNA_def_property_ui_text(
      prop, "Thickness Change", "Thickness change to apply to current strokes (in pixels)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Onion-Skinning */
  prop = RNA_def_property(srna, "use_onion_skinning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "onion_flag", GP_LAYER_ONIONSKIN);
  RNA_def_property_ui_text(
      prop, "Onion Skinning", "Display onion skins before and after the current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_annotation_onion_skinning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "onion_flag", GP_LAYER_ONIONSKIN);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Onion Skinning", "Display annotation onion skins before and after the current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "annotation_onion_before_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "gstep");
  RNA_def_property_range(prop, -1, 120);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Frames Before", "Maximum number of frames to show before current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "annotation_onion_after_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "gstep_next");
  RNA_def_property_range(prop, -1, 120);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Frames After", "Maximum number of frames to show after current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "annotation_onion_before_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "gcolor_prev");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, default_onion_color_b);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Before Color", "Base color for ghosts before the active frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "annotation_onion_after_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "gcolor_next");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, default_onion_color_a);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "After Color", "Base color for ghosts after the active frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* pass index for compositing and modifiers */
  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_ui_text(prop, "Pass Index", "Index number for the \"Layer Index\" pass");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "viewlayer_render", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "viewlayername");
  RNA_def_property_ui_text(
      prop,
      "ViewLayer",
      "Only include Layer in this View Layer render output (leave blank to include always)");

  prop = RNA_def_property(srna, "use_viewlayer_masks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "flag", GP_LAYER_DISABLE_MASKS_IN_VIEWLAYER);
  RNA_def_property_ui_text(
      prop, "Use Masks in Render", "Include the mask layers when rendering the view-layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* blend mode */
  prop = RNA_def_property(srna, "blend_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "blend_mode");
  RNA_def_property_enum_items(prop, rna_enum_layer_blend_modes_items);
  RNA_def_property_ui_text(prop, "Blend Mode", "Blend mode");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Layer transforms. */
  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "location");
  RNA_def_property_ui_text(prop, "Location", "Values for change location");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilLayerMatrix_update");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, nullptr, "rotation");
  RNA_def_property_ui_text(prop, "Rotation", "Values for changes in rotation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilLayerMatrix_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "scale");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Scale", "Values for changes in scale");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilLayerMatrix_update");

  /* Layer matrix. */
  prop = RNA_def_property(srna, "matrix_layer", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "layer_mat");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Matrix Layer", "Local Layer transformation matrix");

  /* Layer inverse matrix. */
  prop = RNA_def_property(srna, "matrix_inverse_layer", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "layer_invmat");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Matrix Layer Inverse", "Local Layer transformation inverse matrix");

  /* Flags */
  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide", "Set layer Visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "annotation_hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Hide", "Set annotation Visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_LOCKED);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_ui_text(
      prop, "Locked", "Protect layer from further editing and/or frame changes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "lock_frame", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_FRAMELOCK);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Frame Locked", "Lock current frame displayed by layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "lock_material", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", GP_LAYER_UNLOCK_COLOR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Disallow Locked Materials Editing", "Avoids editing locked materials in the layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_mask_layer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_USE_MASK);
  RNA_def_property_ui_text(
      prop,
      "Use Mask",
      "The visibility of drawings on this layer is affected by the layers in its masks list");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_lights", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_USE_LIGHTS);
  RNA_def_property_ui_text(
      prop, "Use Lights", "Enable the use of lights on stroke and fill materials");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* solo mode: Only display frames with keyframe */
  prop = RNA_def_property(srna, "use_solo_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_SOLO_MODE);
  RNA_def_property_ui_text(
      prop, "Solo Mode", "In Draw Mode only display layers with keyframe in current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Layer is used as Ruler. */
  prop = RNA_def_property(srna, "is_ruler", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_IS_RULER);
  RNA_def_property_ui_text(prop, "Ruler", "This is a special ruler layer");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_SELECT);
  RNA_def_property_ui_text(prop, "Select", "Layer is selected for editing in the Dope Sheet");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, "rna_GPencil_update");

  prop = RNA_def_property(srna, "show_points", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_DRAWDEBUG);
  RNA_def_property_ui_text(
      prop, "Show Points", "Show the points which make up the strokes (for debugging purposes)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* In Front */
  prop = RNA_def_property(srna, "show_in_front", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", GP_LAYER_NO_XRAY);
  RNA_def_property_ui_text(prop, "In Front", "Make the layer display in front of objects");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Parent object */
  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_GPencilLayer_parent_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Parent", "Parent object");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_dependency_update");

  /* parent type */
  prop = RNA_def_property(srna, "parent_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "partype");
  RNA_def_property_enum_items(prop, parent_type_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, "rna_GPencilLayer_parent_type_set", "rna_Object_parent_type_itemf");
  RNA_def_property_ui_text(prop, "Parent Type", "Type of parent relation");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_dependency_update");

  /* parent bone */
  prop = RNA_def_property(srna, "parent_bone", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "parsubstr");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_GPencilLayer_parent_bone_set");
  RNA_def_property_ui_text(
      prop, "Parent Bone", "Name of parent bone in case of a bone parenting relation");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_dependency_update");

  /* matrix */
  prop = RNA_def_property(srna, "matrix_inverse", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "inverse");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Inverse Matrix", "Parent inverse transformation matrix");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* read only parented flag */
  prop = RNA_def_property(srna, "is_parented", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_GPencilLayer_is_parented_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Is Parented", "True when the layer parent object is set");

  /* Layers API */
  func = RNA_def_function(srna, "clear", "rna_GPencil_layer_clear");
  RNA_def_function_ui_description(func, "Remove all the grease pencil layer data");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
}

static void rna_def_gpencil_layers_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "GreasePencilLayers");
  srna = RNA_def_struct(brna, "GreasePencilLayers", nullptr);
  RNA_def_struct_sdna(srna, "bGPdata");
  RNA_def_struct_ui_text(srna, "Grease Pencil Layers", "Collection of grease pencil layers");

  func = RNA_def_function(srna, "new", "rna_GPencil_layer_new");
  RNA_def_function_ui_description(func, "Add a new grease pencil layer");
  parm = RNA_def_string(func, "name", "GPencilLayer", MAX_NAME, "Name", "Name of the layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(
      func, "set_active", true, "Set Active", "Set the newly created layer to the active layer");
  parm = RNA_def_pointer(func, "layer", "GPencilLayer", "", "The newly created layer");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_GPencil_layer_remove");
  RNA_def_function_ui_description(func, "Remove a grease pencil layer");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "GPencilLayer", "", "The layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "move", "rna_GPencil_layer_move");
  RNA_def_function_ui_description(func, "Move a grease pencil layer in the layer stack");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "GPencilLayer", "", "The layer to move");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  parm = RNA_def_enum(
      func, "type", rna_enum_gplayer_move_type_items, 1, "", "Direction of movement");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "GPencilLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_GPencil_active_layer_get", "rna_GPencil_active_layer_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Layer", "Active grease pencil layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_GPencil_active_layer_index_get",
                             "rna_GPencil_active_layer_index_set",
                             "rna_GPencil_active_layer_index_range");
  RNA_def_property_ui_text(prop, "Active Layer Index", "Index of active grease pencil layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  /* Active Layer - As an enum (for selecting active layer for annotations) */
  prop = RNA_def_property(srna, "active_note", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop,
                              "rna_GPencil_active_layer_index_get",
                              "rna_GPencil_active_layer_index_set",
                              "rna_GPencil_active_layer_itemf");
  RNA_def_property_enum_items(
      prop, rna_enum_dummy_DEFAULT_items); /* purely dynamic, as it maps to user-data */
  RNA_def_property_ui_text(prop, "Active Note", "Note/Layer to add annotation strokes to");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
}

static void rna_def_gpencil_grid(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const float default_grid_color[] = {0.5f, 0.5f, 0.5f};

  srna = RNA_def_struct(brna, "GreasePencilGrid", nullptr);
  RNA_def_struct_sdna(srna, "bGPgrid");
  RNA_def_struct_nested(brna, srna, "GreasePencil");

  RNA_def_struct_path_func(srna, "rna_GreasePencilGrid_path");
  RNA_def_struct_ui_text(
      srna, "Grid and Canvas Settings", "Settings for grid and canvas in 3D viewport");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "scale");
  RNA_def_property_range(prop, 0.01f, FLT_MAX);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Grid Scale", "Grid scale");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, default_grid_color);
  RNA_def_property_ui_text(prop, "Grid Color", "Color for grid lines");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "lines", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "lines");
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_int_default(prop, GP_DEFAULT_GRID_LINES);
  RNA_def_property_ui_text(
      prop, "Grid Subdivisions", "Number of subdivisions in each side of symmetry line");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "offset");
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "GreasePencil", "ID");
  RNA_def_struct_sdna(srna, "bGPdata");
  RNA_def_struct_ui_text(srna, "Grease Pencil", "Freehand annotation sketchbook");
  RNA_def_struct_ui_icon(srna, ICON_OUTLINER_DATA_GREASEPENCIL);

  /* Layers */
  prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "layers", nullptr);
  RNA_def_property_struct_type(prop, "GPencilLayer");
  RNA_def_property_ui_text(prop, "Layers", "");
  rna_def_gpencil_layers_api(brna, prop);

  /* Animation Data */
  rna_def_animdata_common(srna);

  /* materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "mat", "totcol");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.cc */
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_IDMaterials_assign_int");

  /* Depth */
  prop = RNA_def_property(srna, "stroke_depth_order", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "draw_mode");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_stroke_depth_order_items);
  RNA_def_property_ui_text(
      prop,
      "Stroke Depth Order",
      "Defines how the strokes are ordered in 3D space (for objects not displayed 'In Front')");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Flags */
  prop = RNA_def_property(srna, "use_stroke_edit_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DATA_STROKE_EDITMODE);
  RNA_def_property_ui_text(
      prop, "Stroke Edit Mode", "Edit Grease Pencil strokes instead of viewport data");
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, "rna_GPencil_editmode_update");

  prop = RNA_def_property(srna, "is_stroke_paint_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DATA_STROKE_PAINTMODE);
  RNA_def_property_ui_text(prop, "Stroke Paint Mode", "Draw Grease Pencil strokes on click/drag");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, "rna_GPencil_editmode_update");

  prop = RNA_def_property(srna, "is_stroke_sculpt_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DATA_STROKE_SCULPTMODE);
  RNA_def_property_ui_text(
      prop, "Stroke Sculpt Mode", "Sculpt Grease Pencil strokes instead of viewport data");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, "rna_GPencil_editmode_update");

  prop = RNA_def_property(srna, "is_stroke_weight_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DATA_STROKE_WEIGHTMODE);
  RNA_def_property_ui_text(prop, "Stroke Weight Paint Mode", "Grease Pencil weight paint");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, "rna_GPencil_editmode_update");

  prop = RNA_def_property(srna, "is_stroke_vertex_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DATA_STROKE_VERTEXMODE);
  RNA_def_property_ui_text(prop, "Stroke Vertex Paint Mode", "Grease Pencil vertex paint");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, "rna_GPencil_editmode_update");

  prop = RNA_def_property(srna, "use_onion_skinning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DATA_SHOW_ONIONSKINS);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(
      prop, "Onion Skins", "Show ghosts of the keyframes before and after the current frame");
  RNA_def_property_update(
      prop, NC_SCREEN | NC_SCENE | ND_TOOLSETTINGS | ND_DATA | NC_GPENCIL, "rna_GPencil_update");

  prop = RNA_def_property(srna, "stroke_thickness_space", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, stroke_thickness_items);
  RNA_def_property_ui_text(
      prop, "Stroke Thickness", "Set stroke thickness in screen space or world space");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "pixel_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "pixfactor");
  RNA_def_property_range(prop, 0.1f, 30.0f);
  RNA_def_property_ui_range(prop, 0.1f, 30.0f, 1, 2);
  RNA_def_property_ui_text(
      prop,
      "Scale",
      "Scale conversion factor for pixel size (use larger values for thicker lines)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "edit_curve_resolution", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "curve_edit_resolution");
  RNA_def_property_range(prop, 1, 256);
  RNA_def_property_ui_range(prop, 1, 64, 1, 1);
  RNA_def_property_int_default(prop, GP_DEFAULT_CURVE_RESOLUTION);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(
      prop,
      "Curve Resolution",
      "Number of segments generated between control points when editing strokes in curve mode");
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA, "rna_GPencil_stroke_curve_resolution_update");

  prop = RNA_def_property(srna, "use_adaptive_curve_resolution", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DATA_CURVE_ADAPTIVE_RESOLUTION);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop,
                           "Adaptive Resolution",
                           "Set the resolution of each editcurve segment dynamically depending on "
                           "the length of the segment. The resolution is the number of points "
                           "generated per unit distance");
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA, "rna_GPencil_stroke_curve_resolution_update");

  /* Curve editing error threshold. */
  prop = RNA_def_property(srna, "curve_edit_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "curve_edit_threshold");
  RNA_def_property_range(prop, FLT_MIN, 10.0);
  RNA_def_property_float_default(prop, GP_DEFAULT_CURVE_ERROR);
  RNA_def_property_ui_text(prop, "Threshold", "Curve conversion error threshold");
  RNA_def_property_ui_range(prop, FLT_MIN, 10.0, 2, 5);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  /* Curve editing corner angle. */
  prop = RNA_def_property(srna, "curve_edit_corner_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "curve_edit_corner_angle");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_float_default(prop, DEG2RADF(90.0f));
  RNA_def_property_ui_text(prop, "Corner Angle", "Angles above this are considered corners");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_multiedit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DATA_STROKE_MULTIEDIT);
  RNA_def_property_ui_text(prop,
                           "Multiframe",
                           "Edit strokes from multiple grease pencil keyframes at the same time "
                           "(keyframes must be selected to be included)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_curve_edit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DATA_CURVE_EDIT_MODE);
  RNA_def_property_ui_text(prop, "Curve Editing", "Edit strokes using curve handles");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_curve_edit_mode_toggle");

  prop = RNA_def_property(srna, "use_autolock_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DATA_AUTOLOCK_LAYERS);
  RNA_def_property_ui_text(
      prop,
      "Auto-Lock Layers",
      "Automatically lock all layers except the active one to avoid accidental changes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_autolock");

  prop = RNA_def_property(srna, "edit_line_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "line_color");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, default_1);
  RNA_def_property_ui_text(prop, "Edit Line Color", "Color for editing line");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* onion skinning */
  prop = RNA_def_property(srna, "ghost_before_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "gstep");
  RNA_def_property_range(prop, 0, 120);
  RNA_def_property_int_default(prop, 1);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop,
                           "Frames Before",
                           "Maximum number of frames to show before current frame "
                           "(0 = don't show any frames before current)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "ghost_after_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "gstep_next");
  RNA_def_property_range(prop, 0, 120);
  RNA_def_property_int_default(prop, 1);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop,
                           "Frames After",
                           "Maximum number of frames to show after current frame "
                           "(0 = don't show any frames after current)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_ghost_custom_colors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "onion_flag", GP_ONION_GHOST_PREVCOL | GP_ONION_GHOST_NEXTCOL);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "Use Custom Ghost Colors", "Use custom colors for ghost frames");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "before_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "gcolor_prev");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, onion_dft1);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "Before Color", "Base color for ghosts before the active frame");
  RNA_def_property_update(
      prop, NC_SCREEN | NC_SCENE | ND_TOOLSETTINGS | ND_DATA | NC_GPENCIL, "rna_GPencil_update");

  prop = RNA_def_property(srna, "after_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "gcolor_next");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, onion_dft2);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "After Color", "Base color for ghosts after the active frame");
  RNA_def_property_update(
      prop, NC_SCREEN | NC_SCENE | ND_TOOLSETTINGS | ND_DATA | NC_GPENCIL, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_ghosts_always", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "onion_flag", GP_ONION_GHOST_ALWAYS);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop,
                           "Always Show Ghosts",
                           "Ghosts are shown in renders and animation playback. Useful for "
                           "special effects (e.g. motion blur)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "onion_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "onion_mode");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_onion_modes_items);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "Mode", "Mode to display frames");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "onion_keyframe_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "onion_keytype");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_enum_items(prop, rna_enum_onion_keyframe_type_items);
  RNA_def_property_ui_text(prop, "Filter by Type", "Type of keyframe (for filtering)");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_onion_fade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "onion_flag", GP_ONION_FADE);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(
      prop, "Fade", "Display onion keyframes with a fade in color transparency");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_onion_loop", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "onion_flag", GP_ONION_LOOP);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(
      prop, "Show Start Frame", "Display onion keyframes for looping animations");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "onion_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "onion_factor");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_ui_text(prop, "Onion Opacity", "Change fade opacity of displayed onion frames");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "zdepth_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "zdepth_offset");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 3);
  RNA_def_property_float_default(prop, 0.150f);
  RNA_def_property_ui_text(prop, "Surface Offset", "Offset amount when drawing in surface mode");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "is_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DATA_ANNOTATIONS);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Annotation", "Current data-block is an annotation");

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
  rna_def_gpencil_layer_mask(brna);
  rna_def_gpencil_frame(brna);

  rna_def_gpencil_stroke(brna);
  rna_def_gpencil_stroke_point(brna);
  rna_def_gpencil_triangle(brna);
  rna_def_gpencil_curve(brna);
  rna_def_gpencil_curve_point(brna);

  rna_def_gpencil_mvert_group(brna);
}

#endif
