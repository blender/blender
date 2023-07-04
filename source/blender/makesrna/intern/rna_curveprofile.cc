/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <stdio.h>
#include <stdlib.h>

#include "DNA_curve_types.h"
#include "DNA_curveprofile_types.h"

#include "RNA_define.h"
#include "rna_internal.h"

#include "BLT_translation.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#  include "RNA_access.h"

#  include "BKE_curveprofile.h"

/**
 * Set both handle types for all selected points in the profile-- faster than changing types
 * for many points individually. Also set both handles for the points.
 */
static void rna_CurveProfilePoint_handle_type_set(PointerRNA *ptr, int value)
{
  CurveProfilePoint *point = static_cast<CurveProfilePoint *>(ptr->data);
  CurveProfile *profile = point->profile;

  if (profile) {
    BKE_curveprofile_selected_handle_set(profile, value, value);
    BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
    WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
  }
}

static void rna_CurveProfile_clip_set(PointerRNA *ptr, bool value)
{
  CurveProfile *profile = (CurveProfile *)ptr->data;

  if (value) {
    profile->flag |= PROF_USE_CLIP;
  }
  else {
    profile->flag &= ~PROF_USE_CLIP;
  }

  BKE_curveprofile_update(profile, PROF_UPDATE_CLIP);
}

static void rna_CurveProfile_sample_straight_set(PointerRNA *ptr, bool value)
{
  CurveProfile *profile = (CurveProfile *)ptr->data;

  if (value) {
    profile->flag |= PROF_SAMPLE_STRAIGHT_EDGES;
  }
  else {
    profile->flag &= ~PROF_SAMPLE_STRAIGHT_EDGES;
  }

  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
}

static void rna_CurveProfile_sample_even_set(PointerRNA *ptr, bool value)
{
  CurveProfile *profile = (CurveProfile *)ptr->data;

  if (value) {
    profile->flag |= PROF_SAMPLE_EVEN_LENGTHS;
  }
  else {
    profile->flag &= ~PROF_SAMPLE_EVEN_LENGTHS;
  }

  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
}

static void rna_CurveProfile_remove_point(CurveProfile *profile,
                                          ReportList *reports,
                                          PointerRNA *point_ptr)
{
  CurveProfilePoint *point = static_cast<CurveProfilePoint *>(point_ptr->data);
  if (BKE_curveprofile_remove_point(profile, point) == false) {
    BKE_report(reports, RPT_ERROR, "Unable to remove path point");
    return;
  }

  RNA_POINTER_INVALIDATE(point_ptr);
}

static void rna_CurveProfile_evaluate(CurveProfile *profile,
                                      ReportList *reports,
                                      float length_portion,
                                      float location[2])
{
  if (!profile->table) {
    BKE_report(reports, RPT_ERROR, "CurveProfile table not initialized, call initialize()");
  }
  BKE_curveprofile_evaluate_length_portion(profile, length_portion, &location[0], &location[1]);
}

static void rna_CurveProfile_initialize(CurveProfile *profile, int segments_len)
{
  BKE_curveprofile_init(profile, (short)segments_len);
}

static void rna_CurveProfile_update(CurveProfile *profile)
{
  BKE_curveprofile_update(profile, PROF_UPDATE_REMOVE_DOUBLES | PROF_UPDATE_CLIP);
}

#else

static const EnumPropertyItem prop_handle_type_items[] = {
    {HD_AUTO, "AUTO", ICON_HANDLE_AUTO, "Auto Handle", ""},
    {HD_VECT, "VECTOR", ICON_HANDLE_VECTOR, "Vector Handle", ""},
    {HD_FREE, "FREE", ICON_HANDLE_FREE, "Free Handle", ""},
    {HD_ALIGN, "ALIGN", ICON_HANDLE_ALIGNED, "Aligned Free Handles", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rna_def_curveprofilepoint(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CurveProfilePoint", nullptr);
  RNA_def_struct_ui_text(srna, "CurveProfilePoint", "Point of a path used to define a profile");

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Location", "X/Y coordinates of the path point");

  prop = RNA_def_property(srna, "handle_type_1", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "h1");
  RNA_def_property_enum_items(prop, prop_handle_type_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_CurveProfilePoint_handle_type_set", nullptr);
  RNA_def_property_ui_text(prop, "First Handle Type", "Path interpolation at this point");

  prop = RNA_def_property(srna, "handle_type_2", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "h2");
  RNA_def_property_enum_items(prop, prop_handle_type_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_CurveProfilePoint_handle_type_set", nullptr);
  RNA_def_property_ui_text(prop, "Second Handle Type", "Path interpolation at this point");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PROF_SELECT);
  RNA_def_property_ui_text(prop, "Select", "Selection state of the path point");
}

static void rna_def_curveprofile_points_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "CurveProfilePoints");
  srna = RNA_def_struct(brna, "CurveProfilePoints", nullptr);
  RNA_def_struct_sdna(srna, "CurveProfile");
  RNA_def_struct_ui_text(srna, "Profile Point", "Collection of Profile Points");

  func = RNA_def_function(srna, "add", "BKE_curveprofile_insert");
  RNA_def_function_ui_description(func, "Add point to the profile");
  parm = RNA_def_float(func,
                       "x",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "X Position",
                       "X Position for new point",
                       -FLT_MAX,
                       FLT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float(func,
                       "y",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Y Position",
                       "Y Position for new point",
                       -FLT_MAX,
                       FLT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "point", "CurveProfilePoint", "", "New point");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_CurveProfile_remove_point");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Delete point from the profile");
  parm = RNA_def_pointer(func, "point", "CurveProfilePoint", "", "Point to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_curveprofile(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  PropertyRNA *parm;
  FunctionRNA *func;

  static const EnumPropertyItem rna_enum_curveprofile_preset_items[] = {
      {PROF_PRESET_LINE, "LINE", 0, "Line", "Default"},
      {PROF_PRESET_SUPPORTS, "SUPPORTS", 0, "Support Loops", "Loops on each side of the profile"},
      {PROF_PRESET_CORNICE, "CORNICE", 0, "Cornice Molding", ""},
      {PROF_PRESET_CROWN, "CROWN", 0, "Crown Molding", ""},
      {PROF_PRESET_STEPS, "STEPS", 0, "Steps", "A number of steps defined by the segments"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "CurveProfile", nullptr);
  RNA_def_struct_ui_text(srna, "CurveProfile", "Profile Path editor used to build a profile path");

  prop = RNA_def_property(srna, "preset", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "preset");
  RNA_def_property_enum_items(prop, rna_enum_curveprofile_preset_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MESH);
  RNA_def_property_ui_text(prop, "Preset", "");

  prop = RNA_def_property(srna, "use_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PROF_USE_CLIP);
  RNA_def_property_ui_text(prop, "Clip", "Force the path view to fit a defined boundary");
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_CurveProfile_clip_set");

  prop = RNA_def_property(srna, "use_sample_straight_edges", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PROF_SAMPLE_STRAIGHT_EDGES);
  RNA_def_property_ui_text(prop, "Sample Straight Edges", "Sample edges with vector handles");
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_CurveProfile_sample_straight_set");

  prop = RNA_def_property(srna, "use_sample_even_lengths", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PROF_SAMPLE_EVEN_LENGTHS);
  RNA_def_property_ui_text(prop, "Sample Even Lengths", "Sample edges with even lengths");
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_CurveProfile_sample_even_set");

  func = RNA_def_function(srna, "update", "rna_CurveProfile_update");
  RNA_def_function_ui_description(func, "Refresh internal data, remove doubles and clip points");

  func = RNA_def_function(srna, "reset_view", "BKE_curveprofile_reset_view");
  RNA_def_function_ui_description(func, "Reset the curve profile grid to its clipping size");

  func = RNA_def_function(srna, "initialize", "rna_CurveProfile_initialize");
  parm = RNA_def_int(func,
                     "totsegments",
                     1,
                     1,
                     1000,
                     "",
                     "The number of segment values to"
                     " initialize the segments table with",
                     1,
                     100);
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_function_ui_description(func, "Set the number of display segments and fill tables");

  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "path", "path_len");
  RNA_def_property_struct_type(prop, "CurveProfilePoint");
  RNA_def_property_ui_text(prop, "Points", "Profile control points");
  rna_def_curveprofile_points_api(brna, prop);

  prop = RNA_def_property(srna, "segments", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "segments", "segments_len");
  RNA_def_property_struct_type(prop, "CurveProfilePoint");
  RNA_def_property_ui_text(prop, "Segments", "Segments sampled from control points");

  func = RNA_def_function(srna, "evaluate", "rna_CurveProfile_evaluate");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Evaluate the at the given portion of the path length");
  parm = RNA_def_float(func,
                       "length_portion",
                       0.0f,
                       0.0f,
                       1.0f,
                       "Length Portion",
                       "Portion of the path length to travel before evaluation",
                       0.0f,
                       1.0f);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float_vector(func,
                              "location",
                              2,
                              nullptr,
                              -100.0f,
                              100.0f,
                              "Location",
                              "The location at the given portion of the profile",
                              -100.0f,
                              100.0f);
  RNA_def_function_output(func, parm);
}

void RNA_def_profile(BlenderRNA *brna)
{
  rna_def_curveprofilepoint(brna);
  rna_def_curveprofile(brna);
}

#endif
