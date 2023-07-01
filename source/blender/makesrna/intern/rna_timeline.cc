/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_scene_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#  include "BKE_idprop.h"
#  include "WM_api.h"

static IDProperty **rna_TimelineMarker_idprops(PointerRNA *ptr)
{
  TimeMarker *marker = static_cast<TimeMarker*>(ptr->data);
  return &marker->prop;
}

static void rna_TimelineMarker_update(Main * /*bmain*/,
                                      Scene * /*scene*/,
                                      PointerRNA * /*ptr*/)
{
  WM_main_add_notifier(NC_SCENE | ND_MARKERS, nullptr);
  WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, nullptr);
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
#  endif
}

void RNA_def_timeline_marker(BlenderRNA *brna)
{
  rna_def_timeline_marker(brna);
}

#endif