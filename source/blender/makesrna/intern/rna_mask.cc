/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <climits>
#include <cstdlib>

#include "DNA_mask_types.h"
#include "DNA_object_types.h" /* SELECT */
#include "DNA_scene_types.h"

#include "BLT_translation.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_types.hh"

#ifdef RNA_RUNTIME

#  include <algorithm>
#  include <fmt/format.h>

#  include "DNA_defaults.h"
#  include "DNA_movieclip_types.h"

#  include "BLI_math_vector.h"

#  include "BKE_mask.h"
#  include "BKE_movieclip.h"
#  include "BKE_tracking.h"

#  include "DEG_depsgraph.hh"

#  include "RNA_access.hh"

#  include "WM_api.hh"

static void rna_Mask_update_data(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Mask *mask = (Mask *)ptr->owner_id;

  WM_main_add_notifier(NC_MASK | ND_DATA, mask);
  DEG_id_tag_update(&mask->id, 0);
}

static void rna_Mask_update_parent(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  MaskParent *parent = static_cast<MaskParent *>(ptr->data);

  if (parent->id) {
    if (GS(parent->id->name) == ID_MC) {
      MovieClip *clip = (MovieClip *)parent->id;
      MovieTracking *tracking = &clip->tracking;
      MovieTrackingObject *tracking_object = BKE_tracking_object_get_named(tracking,
                                                                           parent->parent);

      if (tracking_object) {
        int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, scene->r.cfra);

        if (parent->type == MASK_PARENT_POINT_TRACK) {
          MovieTrackingTrack *track = BKE_tracking_object_find_track_with_name(tracking_object,
                                                                               parent->sub_parent);

          if (track) {
            MovieTrackingMarker *marker = BKE_tracking_marker_get(track, clip_framenr);
            float marker_pos_ofs[2], parmask_pos[2];
            MovieClipUser user = *DNA_struct_default_get(MovieClipUser);

            BKE_movieclip_user_set_frame(&user, scene->r.cfra);

            add_v2_v2v2(marker_pos_ofs, marker->pos, track->offset);

            BKE_mask_coord_from_movieclip(clip, &user, parmask_pos, marker_pos_ofs);

            copy_v2_v2(parent->parent_orig, parmask_pos);
          }
        }
        else /* if (parent->type == MASK_PARENT_PLANE_TRACK) */ {
          MovieTrackingPlaneTrack *plane_track = BKE_tracking_object_find_plane_track_with_name(
              tracking_object, parent->sub_parent);
          if (plane_track) {
            MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track,
                                                                                   clip_framenr);

            memcpy(parent->parent_corners_orig,
                   plane_marker->corners,
                   sizeof(parent->parent_corners_orig));
            zero_v2(parent->parent_orig);
          }
        }
      }
    }
  }

  rna_Mask_update_data(bmain, scene, ptr);
}

/* NOTE: this function exists only to avoid id reference-counting. */
static void rna_MaskParent_id_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  MaskParent *mpar = (MaskParent *)ptr->data;

  mpar->id = static_cast<ID *>(value.data);
}

static StructRNA *rna_MaskParent_id_typef(PointerRNA *ptr)
{
  MaskParent *mpar = (MaskParent *)ptr->data;

  return ID_code_to_RNA_type(mpar->id_type);
}

static void rna_MaskParent_id_type_set(PointerRNA *ptr, int value)
{
  MaskParent *mpar = (MaskParent *)ptr->data;

  /* change ID-type to the new type */
  mpar->id_type = value;

  /* clear the id-block if the type is invalid */
  if ((mpar->id) && (GS(mpar->id->name) != mpar->id_type)) {
    mpar->id = nullptr;
  }
}

static void rna_Mask_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mask *mask = (Mask *)ptr->owner_id;

  rna_iterator_listbase_begin(iter, ptr, &mask->masklayers, nullptr);
}

static int rna_Mask_layer_active_index_get(PointerRNA *ptr)
{
  Mask *mask = (Mask *)ptr->owner_id;

  return mask->masklay_act;
}

static void rna_Mask_layer_active_index_set(PointerRNA *ptr, int value)
{
  Mask *mask = (Mask *)ptr->owner_id;

  mask->masklay_act = value;
}

static void rna_Mask_layer_active_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  Mask *mask = (Mask *)ptr->owner_id;

  *min = 0;
  *max = max_ii(0, mask->masklay_tot - 1);

  *softmin = *min;
  *softmax = *max;
}

static std::optional<std::string> rna_MaskLayer_path(const PointerRNA *ptr)
{
  const MaskLayer *masklay = (MaskLayer *)ptr->data;
  char name_esc[sizeof(masklay->name) * 2];
  BLI_str_escape(name_esc, masklay->name, sizeof(name_esc));
  return fmt::format("layers[\"{}\"]", name_esc);
}

static PointerRNA rna_Mask_layer_active_get(PointerRNA *ptr)
{
  Mask *mask = (Mask *)ptr->owner_id;
  MaskLayer *masklay = BKE_mask_layer_active(mask);

  return RNA_pointer_create_with_parent(*ptr, &RNA_MaskLayer, masklay);
}

static void rna_Mask_layer_active_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  Mask *mask = (Mask *)ptr->owner_id;
  MaskLayer *masklay = (MaskLayer *)value.data;

  BKE_mask_layer_active_set(mask, masklay);
}

static void rna_MaskLayer_splines_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  MaskLayer *masklay = (MaskLayer *)ptr->data;

  rna_iterator_listbase_begin(iter, ptr, &masklay->splines, nullptr);
}

static void rna_MaskLayer_name_set(PointerRNA *ptr, const char *value)
{
  Mask *mask = (Mask *)ptr->owner_id;
  MaskLayer *masklay = (MaskLayer *)ptr->data;
  char oldname[sizeof(masklay->name)], newname[sizeof(masklay->name)];

  /* need to be on the stack */
  STRNCPY(oldname, masklay->name);
  STRNCPY_UTF8(newname, value);

  BKE_mask_layer_rename(mask, masklay, oldname, newname);
}

static PointerRNA rna_MaskLayer_active_spline_get(PointerRNA *ptr)
{
  MaskLayer *masklay = (MaskLayer *)ptr->data;

  return RNA_pointer_create_with_parent(*ptr, &RNA_MaskSpline, masklay->act_spline);
}

static void rna_MaskLayer_active_spline_set(PointerRNA *ptr,
                                            PointerRNA value,
                                            ReportList * /*reports*/)
{
  MaskLayer *masklay = (MaskLayer *)ptr->data;
  MaskSpline *spline = (MaskSpline *)value.data;
  int index = BLI_findindex(&masklay->splines, spline);

  if (index != -1) {
    masklay->act_spline = spline;
  }
  else {
    masklay->act_spline = nullptr;
  }
}

static PointerRNA rna_MaskLayer_active_spline_point_get(PointerRNA *ptr)
{
  MaskLayer *masklay = (MaskLayer *)ptr->data;

  return RNA_pointer_create_with_parent(*ptr, &RNA_MaskSplinePoint, masklay->act_point);
}

static void rna_MaskLayer_active_spline_point_set(PointerRNA *ptr,
                                                  PointerRNA value,
                                                  ReportList * /*reports*/)
{
  MaskLayer *masklay = (MaskLayer *)ptr->data;
  MaskSpline *spline;
  MaskSplinePoint *point = (MaskSplinePoint *)value.data;

  masklay->act_point = nullptr;

  for (spline = static_cast<MaskSpline *>(masklay->splines.first); spline; spline = spline->next) {
    if (point >= spline->points && point < spline->points + spline->tot_point) {
      masklay->act_point = point;

      break;
    }
  }
}

static void rna_MaskSplinePoint_handle1_get(PointerRNA *ptr, float *values)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;
  copy_v2_v2(values, bezt->vec[0]);
}

static void rna_MaskSplinePoint_handle1_set(PointerRNA *ptr, const float *values)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;
  copy_v2_v2(bezt->vec[0], values);
}

static void rna_MaskSplinePoint_handle2_get(PointerRNA *ptr, float *values)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;
  copy_v2_v2(values, bezt->vec[2]);
}

static void rna_MaskSplinePoint_handle2_set(PointerRNA *ptr, const float *values)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;
  copy_v2_v2(bezt->vec[2], values);
}

static void rna_MaskSplinePoint_ctrlpoint_get(PointerRNA *ptr, float *values)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;
  copy_v2_v2(values, bezt->vec[1]);
}

static void rna_MaskSplinePoint_ctrlpoint_set(PointerRNA *ptr, const float *values)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;
  copy_v2_v2(bezt->vec[1], values);
}

static int rna_MaskSplinePoint_handle_type_get(PointerRNA *ptr)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;

  return bezt->h1;
}

static MaskSpline *mask_spline_from_point(Mask *mask, MaskSplinePoint *point)
{
  MaskLayer *mask_layer;
  for (mask_layer = static_cast<MaskLayer *>(mask->masklayers.first); mask_layer;
       mask_layer = mask_layer->next)
  {
    MaskSpline *spline;
    for (spline = static_cast<MaskSpline *>(mask_layer->splines.first); spline;
         spline = spline->next)
    {
      if (point >= spline->points && point < spline->points + spline->tot_point) {
        return spline;
      }
    }
  }
  return nullptr;
}

static void mask_point_check_stick(MaskSplinePoint *point)
{
  BezTriple *bezt = &point->bezt;
  if (bezt->h1 == HD_ALIGN && bezt->h2 == HD_ALIGN) {
    float vec[3];
    sub_v3_v3v3(vec, bezt->vec[0], bezt->vec[1]);
    add_v3_v3v3(bezt->vec[2], bezt->vec[1], vec);
  }
}

static void rna_MaskSplinePoint_handle_type_set(PointerRNA *ptr, int value)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;
  MaskSpline *spline = mask_spline_from_point((Mask *)ptr->owner_id, point);

  bezt->h1 = bezt->h2 = value;
  mask_point_check_stick(point);
  BKE_mask_calc_handle_point(spline, point);
}

static int rna_MaskSplinePoint_handle_left_type_get(PointerRNA *ptr)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;

  return bezt->h1;
}

static void rna_MaskSplinePoint_handle_left_type_set(PointerRNA *ptr, int value)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;
  MaskSpline *spline = mask_spline_from_point((Mask *)ptr->owner_id, point);

  bezt->h1 = value;
  mask_point_check_stick(point);
  BKE_mask_calc_handle_point(spline, point);
}

static int rna_MaskSplinePoint_handle_right_type_get(PointerRNA *ptr)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;

  return bezt->h2;
}

static void rna_MaskSplinePoint_handle_right_type_set(PointerRNA *ptr, int value)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;
  BezTriple *bezt = &point->bezt;
  MaskSpline *spline = mask_spline_from_point((Mask *)ptr->owner_id, point);

  bezt->h2 = value;
  mask_point_check_stick(point);
  BKE_mask_calc_handle_point(spline, point);
}

/* ** API ** */

static MaskLayer *rna_Mask_layers_new(Mask *mask, const char *name)
{
  MaskLayer *masklay = BKE_mask_layer_new(mask, name);

  WM_main_add_notifier(NC_MASK | NA_EDITED, mask);

  return masklay;
}

static void rna_Mask_layers_remove(Mask *mask, ReportList *reports, PointerRNA *masklay_ptr)
{
  MaskLayer *masklay = static_cast<MaskLayer *>(masklay_ptr->data);
  if (BLI_findindex(&mask->masklayers, masklay) == -1) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Mask layer '%s' not found in mask '%s'",
                masklay->name,
                mask->id.name + 2);
    return;
  }

  BKE_mask_layer_remove(mask, masklay);
  masklay_ptr->invalidate();

  WM_main_add_notifier(NC_MASK | NA_EDITED, mask);
}

static void rna_Mask_layers_clear(Mask *mask)
{
  BKE_mask_layer_free_list(&mask->masklayers);

  WM_main_add_notifier(NC_MASK | NA_EDITED, mask);
}

static void rna_MaskSplinePoint_handle_single_select_set(PointerRNA *ptr, bool value)
{
  Mask *mask = (Mask *)ptr->owner_id;
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;

  BKE_mask_point_select_set_handle(point, MASK_WHICH_HANDLE_STICK, value);

  DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
  WM_main_add_notifier(NC_MASK | NA_SELECTED, mask);
}

static bool rna_MaskSplinePoint_handle_single_select_get(PointerRNA *ptr)
{
  MaskSplinePoint *point = (MaskSplinePoint *)ptr->data;

  return MASKPOINT_ISSEL_HANDLE(point, MASK_WHICH_HANDLE_STICK);
}

static MaskSpline *rna_MaskLayer_spline_new(ID *id, MaskLayer *mask_layer)
{
  Mask *mask = (Mask *)id;
  MaskSpline *new_spline;

  new_spline = BKE_mask_spline_add(mask_layer);

  WM_main_add_notifier(NC_MASK | NA_EDITED, mask);

  return new_spline;
}

static void rna_MaskLayer_spline_remove(ID *id,
                                        MaskLayer *mask_layer,
                                        ReportList *reports,
                                        PointerRNA *spline_ptr)
{
  Mask *mask = (Mask *)id;
  MaskSpline *spline = static_cast<MaskSpline *>(spline_ptr->data);

  if (BKE_mask_spline_remove(mask_layer, spline) == false) {
    BKE_reportf(
        reports, RPT_ERROR, "Mask layer '%s' does not contain spline given", mask_layer->name);
    return;
  }

  spline_ptr->invalidate();

  DEG_id_tag_update(&mask->id, ID_RECALC_GEOMETRY);
}

static void rna_Mask_start_frame_set(PointerRNA *ptr, int value)
{
  Mask *data = (Mask *)ptr->data;
  /* MINFRAME not MINAFRAME, since some output formats can't taken negative frames */
  CLAMP(value, MINFRAME, MAXFRAME);
  data->sfra = value;

  if (data->sfra >= data->efra) {
    data->efra = std::min(data->sfra, MAXFRAME);
  }
}

static void rna_Mask_end_frame_set(PointerRNA *ptr, int value)
{
  Mask *data = (Mask *)ptr->data;
  CLAMP(value, MINFRAME, MAXFRAME);
  data->efra = value;

  if (data->sfra >= data->efra) {
    data->sfra = std::max(data->efra, MINFRAME);
  }
}

static void rna_MaskSpline_points_add(ID *id, MaskSpline *spline, int count)
{
  Mask *mask = (Mask *)id;
  MaskLayer *layer;
  int active_point_index = -1;
  int i, spline_shape_index;

  if (count <= 0) {
    return;
  }

  for (layer = static_cast<MaskLayer *>(mask->masklayers.first); layer; layer = layer->next) {
    if (BLI_findindex(&layer->splines, spline) != -1) {
      break;
    }
  }

  if (!layer) {
    /* Shall not happen actually */
    BLI_assert_msg(0, "No layer found for the spline");
    return;
  }

  if (layer->act_spline == spline) {
    active_point_index = layer->act_point - spline->points;
  }

  spline->points = static_cast<MaskSplinePoint *>(
      MEM_recallocN(spline->points, sizeof(MaskSplinePoint) * (spline->tot_point + count)));
  spline->tot_point += count;

  if (active_point_index >= 0) {
    layer->act_point = spline->points + active_point_index;
  }

  spline_shape_index = BKE_mask_layer_shape_spline_to_index(layer, spline);

  for (i = 0; i < count; i++) {
    int point_index = spline->tot_point - count + i;
    MaskSplinePoint *new_point = spline->points + point_index;
    new_point->bezt.h1 = new_point->bezt.h2 = HD_ALIGN;
    BKE_mask_calc_handle_point_auto(spline, new_point, true);
    BKE_mask_parent_init(&new_point->parent);

    /* Not efficient, but there's no other way for now */
    BKE_mask_layer_shape_changed_add(layer, spline_shape_index + point_index, true, true);
  }

  WM_main_add_notifier(NC_MASK | ND_DATA, mask);
  DEG_id_tag_update(&mask->id, 0);
}

static void rna_MaskSpline_point_remove(ID *id,
                                        MaskSpline *spline,
                                        ReportList *reports,
                                        PointerRNA *point_ptr)
{
  Mask *mask = (Mask *)id;
  MaskSplinePoint *point = static_cast<MaskSplinePoint *>(point_ptr->data);
  MaskSplinePoint *new_point_array;
  MaskLayer *layer;
  int active_point_index = -1;
  int point_index;

  for (layer = static_cast<MaskLayer *>(mask->masklayers.first); layer; layer = layer->next) {
    if (BLI_findindex(&layer->splines, spline) != -1) {
      break;
    }
  }

  if (!layer) {
    /* Shall not happen actually */
    BKE_report(reports, RPT_ERROR, "Mask layer not found for given spline");
    return;
  }

  if (point < spline->points || point >= spline->points + spline->tot_point) {
    BKE_report(reports, RPT_ERROR, "Point is not found in given spline");
    return;
  }

  if (layer->act_spline == spline) {
    active_point_index = layer->act_point - spline->points;
  }

  point_index = point - spline->points;

  new_point_array = MEM_malloc_arrayN<MaskSplinePoint>(size_t(spline->tot_point) - 1,
                                                       "remove mask point");

  memcpy(new_point_array, spline->points, sizeof(MaskSplinePoint) * point_index);
  memcpy(new_point_array + point_index,
         spline->points + point_index + 1,
         sizeof(MaskSplinePoint) * (spline->tot_point - point_index - 1));

  MEM_freeN(spline->points);
  spline->points = new_point_array;
  spline->tot_point--;

  if (active_point_index >= 0) {
    if (active_point_index == point_index) {
      layer->act_point = nullptr;
    }
    else if (active_point_index < point_index) {
      layer->act_point = spline->points + active_point_index;
    }
    else {
      layer->act_point = spline->points + active_point_index - 1;
    }
  }

  BKE_mask_layer_shape_changed_remove(
      layer, BKE_mask_layer_shape_spline_to_index(layer, spline) + point_index, 1);

  WM_main_add_notifier(NC_MASK | ND_DATA, mask);
  DEG_id_tag_update(&mask->id, 0);

  point_ptr->invalidate();
}

#else
static void rna_def_maskParent(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem mask_id_type_items[] = {
      {ID_MC, "MOVIECLIP", ICON_SEQUENCE, "Movie Clip", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem parent_type_items[] = {
      {MASK_PARENT_POINT_TRACK, "POINT_TRACK", 0, "Point Track", ""},
      {MASK_PARENT_PLANE_TRACK, "PLANE_TRACK", 0, "Plane Track", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "MaskParent", nullptr);
  RNA_def_struct_ui_text(srna, "Mask Parent", "Parenting settings for masking element");

  /* Target Properties - ID-block to Drive */
  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  // RNA_def_property_editable_func(prop, "rna_maskSpline_id_editable");
  /* NOTE: custom set function is ONLY to avoid rna setting a user for this. */
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_MaskParent_id_set", "rna_MaskParent_id_typef", nullptr);
  RNA_def_property_ui_text(
      prop, "ID", "ID-block to which masking element would be parented to or to its property");
  RNA_def_property_update(prop, 0, "rna_Mask_update_parent");

  prop = RNA_def_property(srna, "id_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "id_type");
  RNA_def_property_enum_items(prop, mask_id_type_items);
  RNA_def_property_enum_default(prop, ID_MC);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_MaskParent_id_type_set", nullptr);
  // RNA_def_property_editable_func(prop, "rna_MaskParent_id_type_editable");
  RNA_def_property_ui_text(prop, "ID Type", "Type of ID-block that can be used");
  RNA_def_property_update(prop, 0, "rna_Mask_update_parent");

  /* type */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, parent_type_items);
  RNA_def_property_ui_text(prop, "Parent Type", "Parent Type");
  RNA_def_property_update(prop, 0, "rna_Mask_update_parent");

  /* parent */
  prop = RNA_def_property(srna, "parent", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Parent", "Name of parent object in specified data-block to which parenting happens");
  RNA_def_property_string_maxlength(prop, MAX_ID_NAME - 2);
  RNA_def_property_update(prop, 0, "rna_Mask_update_parent");

  /* sub_parent */
  prop = RNA_def_property(srna, "sub_parent", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Sub Parent",
      "Name of parent sub-object in specified data-block to which parenting happens");
  RNA_def_property_string_maxlength(prop, MAX_ID_NAME - 2);
  RNA_def_property_update(prop, 0, "rna_Mask_update_parent");
}

static void rna_def_maskSplinePointUW(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MaskSplinePointUW", nullptr);
  RNA_def_struct_ui_text(
      srna, "Mask Spline UW Point", "Single point in spline segment defining feather");

  /* u */
  prop = RNA_def_property(srna, "u", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "u");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "U", "U coordinate of point along spline segment");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  /* weight */
  prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "w");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Weight", "Weight of feather point");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  /* select */
  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SELECT);
  RNA_def_property_ui_text(prop, "Select", "Selection status");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");
}

static void rna_def_maskSplinePoint(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem handle_type_items[] = {
      {HD_AUTO, "AUTO", 0, "Auto", ""},
      {HD_VECT, "VECTOR", 0, "Vector", ""},
      {HD_ALIGN, "ALIGNED", 0, "Aligned Single", ""},
      {HD_ALIGN_DOUBLESIDE, "ALIGNED_DOUBLESIDE", 0, "Aligned", ""},
      {HD_FREE, "FREE", 0, "Free", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  rna_def_maskSplinePointUW(brna);

  srna = RNA_def_struct(brna, "MaskSplinePoint", nullptr);
  RNA_def_struct_ui_text(
      srna, "Mask Spline Point", "Single point in spline used for defining mask");

  /* Vector values */
  prop = RNA_def_property(srna, "handle_left", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_funcs(
      prop, "rna_MaskSplinePoint_handle1_get", "rna_MaskSplinePoint_handle1_set", nullptr);
  RNA_def_property_ui_text(prop, "Handle 1", "Coordinates of the first handle");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_funcs(
      prop, "rna_MaskSplinePoint_ctrlpoint_get", "rna_MaskSplinePoint_ctrlpoint_set", nullptr);
  RNA_def_property_ui_text(prop, "Control Point", "Coordinates of the control point");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  prop = RNA_def_property(srna, "handle_right", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_funcs(
      prop, "rna_MaskSplinePoint_handle2_get", "rna_MaskSplinePoint_handle2_set", nullptr);
  RNA_def_property_ui_text(prop, "Handle 2", "Coordinates of the second handle");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  /* handle_type */
  prop = RNA_def_property(srna, "handle_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(
      prop, "rna_MaskSplinePoint_handle_type_get", "rna_MaskSplinePoint_handle_type_set", nullptr);
  RNA_def_property_enum_items(prop, handle_type_items);
  RNA_def_property_ui_text(prop, "Handle Type", "Handle type");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  /* handle_type */
  prop = RNA_def_property(srna, "handle_left_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop,
                              "rna_MaskSplinePoint_handle_left_type_get",
                              "rna_MaskSplinePoint_handle_left_type_set",
                              nullptr);
  RNA_def_property_enum_items(prop, handle_type_items);
  RNA_def_property_ui_text(prop, "Handle 1 Type", "Handle type");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  /* handle_right */
  prop = RNA_def_property(srna, "handle_right_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop,
                              "rna_MaskSplinePoint_handle_right_type_get",
                              "rna_MaskSplinePoint_handle_right_type_set",
                              nullptr);
  RNA_def_property_enum_items(prop, handle_type_items);
  RNA_def_property_ui_text(prop, "Handle 2 Type", "Handle type");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  /* weight */
  prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "bezt.weight");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Weight", "Weight of the point");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  /* select */

  /* DEPRECATED */
  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bezt.f2", SELECT);
  RNA_def_property_ui_text(
      prop,
      "Select",
      "Selection status of the control point. (Deprecated: use Select Control Point instead)");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  prop = RNA_def_property(srna, "select_left_handle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bezt.f1", SELECT);
  RNA_def_property_ui_text(prop, "Select Left Handle", "Selection status of the left handle");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  prop = RNA_def_property(srna, "select_control_point", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bezt.f2", SELECT);
  RNA_def_property_ui_text(prop, "Select Control Point", "Selection status of the control point");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  prop = RNA_def_property(srna, "select_right_handle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bezt.f3", SELECT);
  RNA_def_property_ui_text(prop, "Select Right Handle", "Selection status of the right handle");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  prop = RNA_def_property(srna, "select_single_handle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_MaskSplinePoint_handle_single_select_get",
                                 "rna_MaskSplinePoint_handle_single_select_set");
  RNA_def_property_ui_text(
      prop, "Select Aligned Single Handle", "Selection status of the Aligned Single handle");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  /* parent */
  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MaskParent");

  /* feather points */
  prop = RNA_def_property(srna, "feather_points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MaskSplinePointUW");
  RNA_def_property_collection_sdna(prop, nullptr, "uw", "tot_uw");
  RNA_def_property_ui_text(prop, "Feather Points", "Points defining feather");
}

static void rna_def_mask_splines(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *prop;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "MaskSplines", nullptr);
  RNA_def_struct_sdna(srna, "MaskLayer");
  RNA_def_struct_ui_text(srna, "Mask Splines", "Collection of masking splines");

  /* Create new spline */
  func = RNA_def_function(srna, "new", "rna_MaskLayer_spline_new");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Add a new spline to the layer");
  parm = RNA_def_pointer(func, "spline", "MaskSpline", "", "The newly created spline");
  RNA_def_function_return(func, parm);

  /* Remove the spline */
  func = RNA_def_function(srna, "remove", "rna_MaskLayer_spline_remove");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Remove a spline from a layer");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "spline", "MaskSpline", "", "The spline to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* active spline */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MaskSpline");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_MaskLayer_active_spline_get",
                                 "rna_MaskLayer_active_spline_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Spline", "Active spline of masking layer");

  /* active point */
  prop = RNA_def_property(srna, "active_point", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MaskSplinePoint");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_MaskLayer_active_spline_point_get",
                                 "rna_MaskLayer_active_spline_point_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Point", "Active point of masking layer");
}

static void rna_def_maskSplinePoints(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "MaskSplinePoints", nullptr);
  RNA_def_struct_sdna(srna, "MaskSpline");
  RNA_def_struct_ui_text(srna, "Mask Spline Points", "Collection of masking spline points");

  /* Create new point */
  func = RNA_def_function(srna, "add", "rna_MaskSpline_points_add");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Add a number of point to this spline");
  parm = RNA_def_int(
      func, "count", 1, 0, INT_MAX, "Number", "Number of points to add to the spline", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* Remove the point */
  func = RNA_def_function(srna, "remove", "rna_MaskSpline_point_remove");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Remove a point from a spline");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "point", "MaskSplinePoint", "", "The point to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_maskSpline(BlenderRNA *brna)
{
  static const EnumPropertyItem spline_interpolation_items[] = {
      {MASK_SPLINE_INTERP_LINEAR, "LINEAR", 0, "Linear", ""},
      {MASK_SPLINE_INTERP_EASE, "EASE", 0, "Ease", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem spline_offset_mode_items[] = {
      {MASK_SPLINE_OFFSET_EVEN, "EVEN", 0, "Even", "Calculate even feather offset"},
      {MASK_SPLINE_OFFSET_SMOOTH,
       "SMOOTH",
       0,
       "Smooth",
       "Calculate feather offset as a second curve"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_maskSplinePoint(brna);

  srna = RNA_def_struct(brna, "MaskSpline", nullptr);
  RNA_def_struct_ui_text(srna, "Mask spline", "Single spline used for defining mask shape");

  /* offset mode */
  prop = RNA_def_property(srna, "offset_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "offset_mode");
  RNA_def_property_enum_items(prop, spline_offset_mode_items);
  RNA_def_property_ui_text(
      prop, "Feather Offset", "The method used for calculating the feather offset");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MASK);
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  /* weight interpolation */
  prop = RNA_def_property(srna, "weight_interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "weight_interp");
  RNA_def_property_enum_items(prop, spline_interpolation_items);
  RNA_def_property_ui_text(
      prop, "Weight Interpolation", "The type of weight interpolation for spline");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");

  /* cyclic */
  prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", MASK_SPLINE_CYCLIC);
  RNA_def_property_ui_text(prop, "Cyclic", "Make this spline a closed loop");
  RNA_def_property_update(prop, NC_MASK | NA_EDITED, "rna_Mask_update_data");

  /* fill */
  prop = RNA_def_property(srna, "use_fill", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", MASK_SPLINE_NOFILL);
  RNA_def_property_ui_text(prop, "Fill", "Make this spline filled");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MASK);
  RNA_def_property_update(prop, NC_MASK | NA_EDITED, "rna_Mask_update_data");

  /* self-intersection check */
  prop = RNA_def_property(srna, "use_self_intersection_check", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", MASK_SPLINE_NOINTERSECT);
  RNA_def_property_ui_text(
      prop, "Self Intersection Check", "Prevent feather from self-intersections");
  RNA_def_property_update(prop, NC_MASK | NA_EDITED, "rna_Mask_update_data");

  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MaskSplinePoint");
  RNA_def_property_collection_sdna(prop, nullptr, "points", "tot_point");
  RNA_def_property_ui_text(prop, "Points", "Collection of points");
  RNA_def_property_srna(prop, "MaskSplinePoints");
}

static void rna_def_mask_layer(BlenderRNA *brna)
{
  static const EnumPropertyItem masklay_blend_mode_items[] = {
      {MASK_BLEND_MERGE_ADD, "MERGE_ADD", 0, "Merge Add", ""},
      {MASK_BLEND_MERGE_SUBTRACT, "MERGE_SUBTRACT", 0, "Merge Subtract", ""},
      {MASK_BLEND_ADD, "ADD", 0, "Add", ""},
      {MASK_BLEND_SUBTRACT, "SUBTRACT", 0, "Subtract", ""},
      {MASK_BLEND_LIGHTEN, "LIGHTEN", 0, "Lighten", ""},
      {MASK_BLEND_DARKEN, "DARKEN", 0, "Darken", ""},
      {MASK_BLEND_MUL, "MUL", 0, "Multiply", ""},
      {MASK_BLEND_REPLACE, "REPLACE", 0, "Replace", ""},
      {MASK_BLEND_DIFFERENCE, "DIFFERENCE", 0, "Difference", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_maskSpline(brna);
  rna_def_mask_splines(brna);
  rna_def_maskSplinePoints(brna);

  srna = RNA_def_struct(brna, "MaskLayer", nullptr);
  RNA_def_struct_ui_text(srna, "Mask Layer", "Single layer used for masking pixels");
  RNA_def_struct_path_func(srna, "rna_MaskLayer_path");

  /* name */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Unique name of layer");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_MaskLayer_name_set");
  RNA_def_property_string_maxlength(prop, MAX_ID_NAME - 2);
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");
  RNA_def_struct_name_property(srna, prop);

  /* splines */
  prop = RNA_def_property(srna, "splines", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_MaskLayer_splines_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MaskSpline");
  RNA_def_property_ui_text(prop, "Splines", "Collection of splines which defines this layer");
  RNA_def_property_srna(prop, "MaskSplines");

  /* restrict */
  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", MASK_HIDE_VIEW);
  RNA_def_property_ui_text(prop, "Restrict View", "Restrict visibility in the viewport");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, -1);
  RNA_def_property_update(prop, NC_MASK | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "hide_select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", MASK_HIDE_SELECT);
  RNA_def_property_ui_text(prop, "Restrict Select", "Restrict selection in the viewport");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, -1);
  RNA_def_property_update(prop, NC_MASK | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "hide_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", MASK_HIDE_RENDER);
  RNA_def_property_ui_text(prop, "Restrict Render", "Restrict renderability");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, -1);
  RNA_def_property_update(prop, NC_MASK | NA_EDITED, nullptr);

  /* Select (for dope-sheet). */
  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", MASK_LAYERFLAG_SELECT);
  RNA_def_property_ui_text(prop, "Select", "Layer is selected for editing in the Dope Sheet");
  //  RNA_def_property_update(prop, NC_SCREEN | ND_MASK, nullptr);

  /* render settings */
  prop = RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "alpha");
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Opacity", "Render Opacity");
  RNA_def_property_update(prop, NC_MASK | NA_EDITED, nullptr);

  /* weight interpolation */
  prop = RNA_def_property(srna, "blend", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "blend");
  RNA_def_property_enum_items(prop, masklay_blend_mode_items);
  RNA_def_property_ui_text(prop, "Blend", "Method of blending mask layers");
  RNA_def_property_update(prop, 0, "rna_Mask_update_data");
  RNA_def_property_update(prop, NC_MASK | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "blend_flag", MASK_BLENDFLAG_INVERT);
  RNA_def_property_ui_text(prop, "Invert", "Invert the mask black/white");
  RNA_def_property_update(prop, NC_MASK | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "falloff", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "falloff");
  RNA_def_property_enum_items(prop, rna_enum_proportional_falloff_curve_only_items);
  RNA_def_property_ui_text(prop, "Falloff", "Falloff type of the feather");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */
  RNA_def_property_update(prop, NC_MASK | NA_EDITED, nullptr);

  /* filling options */
  prop = RNA_def_property(srna, "use_fill_holes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", MASK_LAYERFLAG_FILL_DISCRETE);
  RNA_def_property_ui_text(
      prop, "Calculate Holes", "Calculate holes when filling overlapping curves");
  RNA_def_property_update(prop, NC_MASK | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "use_fill_overlap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", MASK_LAYERFLAG_FILL_OVERLAP);
  RNA_def_property_ui_text(
      prop, "Calculate Overlap", "Calculate self intersections and overlap before filling");
  RNA_def_property_update(prop, NC_MASK | NA_EDITED, nullptr);
}

static void rna_def_masklayers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MaskLayers");
  srna = RNA_def_struct(brna, "MaskLayers", nullptr);
  RNA_def_struct_sdna(srna, "Mask");
  RNA_def_struct_ui_text(srna, "Mask Layers", "Collection of layers used by mask");

  func = RNA_def_function(srna, "new", "rna_Mask_layers_new");
  RNA_def_function_ui_description(func, "Add layer to this mask");
  RNA_def_string(func, "name", nullptr, 0, "Name", "Name of new layer");
  parm = RNA_def_pointer(func, "layer", "MaskLayer", "", "New mask layer");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Mask_layers_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove layer from this mask");
  parm = RNA_def_pointer(func, "layer", "MaskLayer", "", "Shape to be removed");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* clear all layers */
  func = RNA_def_function(srna, "clear", "rna_Mask_layers_clear");
  RNA_def_function_ui_description(func, "Remove all mask layers");

  /* active layer */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MaskLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_Mask_layer_active_get", "rna_Mask_layer_active_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Shape", "Active layer in this mask");
}

static void rna_def_mask(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_mask_layer(brna);

  srna = RNA_def_struct(brna, "Mask", "ID");
  RNA_def_struct_ui_text(srna, "Mask", "Mask data-block defining mask for compositing");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MASK);

  /* mask layers */
  prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mask_layers_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MaskLayer");
  RNA_def_property_ui_text(prop, "Layers", "Collection of layers which defines this mask");
  rna_def_masklayers(brna, prop);

  /* active masklay index */
  prop = RNA_def_property(srna, "active_layer_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "masklay_act");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_Mask_layer_active_index_get",
                             "rna_Mask_layer_active_index_set",
                             "rna_Mask_layer_active_index_range");
  RNA_def_property_ui_text(
      prop, "Active Shape Index", "Index of active layer in list of all mask's layers");
  RNA_def_property_update(prop, NC_MASK | ND_DRAW, nullptr);

  /* frame range */
  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "sfra");
  RNA_def_property_int_funcs(prop, nullptr, "rna_Mask_start_frame_set", nullptr);
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "Start Frame", "First frame of the mask (used for sequencer)");
  RNA_def_property_update(prop, NC_MASK | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "efra");
  RNA_def_property_int_funcs(prop, nullptr, "rna_Mask_end_frame_set", nullptr);
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "End Frame", "Final frame of the mask (used for sequencer)");
  RNA_def_property_update(prop, NC_MASK | ND_DRAW, nullptr);

  /* pointers */
  rna_def_animdata_common(srna);
}

void RNA_def_mask(BlenderRNA *brna)
{
  rna_def_maskParent(brna);
  rna_def_mask(brna);
}

#endif
