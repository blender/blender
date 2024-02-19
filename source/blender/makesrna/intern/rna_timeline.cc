/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_scene_types.h"

#include "RNA_define.hh"

#include "rna_internal.hh"

#include "WM_types.hh"

#ifdef RNA_RUNTIME

#  include "BKE_idprop.h"
#  include "BKE_scene.h"
#  include "BKE_screen.hh"
#  include "WM_api.hh"

#  include "DEG_depsgraph_build.hh"

static IDProperty **rna_TimelineMarker_idprops(PointerRNA *ptr)
{
  TimeMarker *marker = static_cast<TimeMarker *>(ptr->data);
  return &marker->prop;
}

static void rna_TimelineMarker_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA * /*ptr*/)
{
  WM_main_add_notifier(NC_SCENE | ND_MARKERS, nullptr);
  WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, nullptr);
}

static void rna_TimelineMarker_camera_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  Scene *scene = (Scene *)ptr->owner_id;

  BKE_scene_camera_switch_update(scene);
  WM_windows_scene_data_sync(&wm->windows, scene);
  DEG_relations_tag_update(bmain);

  WM_main_add_notifier(NC_SCENE | ND_MARKERS, nullptr);
  WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, nullptr);
  WM_main_add_notifier(NC_SCENE | NA_EDITED, scene); /* so we get view3d redraws */
}

#else

static void rna_def_timeline_marker(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "TimelineMarker", nullptr);
  RNA_def_struct_sdna(srna, "TimeMarker");
  RNA_def_struct_ui_text(srna, "Marker", "Marker for noting points in the timeline");
  RNA_def_struct_idprops_func(srna, "rna_TimelineMarker_idprops");

  /* String values */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, 0, "rna_TimelineMarker_update");

  prop = RNA_def_property(srna, "frame", PROP_INT, PROP_TIME);
  RNA_def_property_ui_text(prop, "Frame", "The frame on which the timeline marker appears");
  RNA_def_property_update(prop, 0, "rna_TimelineMarker_update");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", 1 /*SELECT*/);
  RNA_def_property_ui_text(prop, "Select", "Marker selection state");
  RNA_def_property_update(prop, 0, "rna_TimelineMarker_update");

#  ifdef DURIAN_CAMERA_SWITCH
  prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Camera", "Camera that becomes active on this frame");
  RNA_def_property_update(prop, 0, "rna_TimelineMarker_camera_update");
#  endif
}

void RNA_def_timeline_marker(BlenderRNA *brna)
{
  rna_def_timeline_marker(brna);
}

#endif
