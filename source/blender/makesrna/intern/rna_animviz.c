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

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

/* Which part of bone(s) get baked */
// TODO: icons?
const EnumPropertyItem rna_enum_motionpath_bake_location_items[] = {
    {MOTIONPATH_BAKE_HEADS, "HEADS", 0, "Heads", "Calculate bone paths from heads"},
    {0, "TAILS", 0, "Tails", "Calculate bone paths from tails"},
#if 0
    {MOTIONPATH_BAKE_CENTERS,
     "CENTROID",
     0,
     "Centers",
     "Calculate bone paths from center of mass"},
#endif
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

static PointerRNA rna_AnimViz_motion_paths_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_AnimVizMotionPaths, ptr->data);
}

static void rna_AnimViz_path_start_frame_set(PointerRNA *ptr, int value)
{
  bAnimVizSettings *data = (bAnimVizSettings *)ptr->data;

  /* XXX: Watch it! Path Start > MAXFRAME/2 could be a problem. */
  data->path_sf = value;
  FRAMENUMBER_MIN_CLAMP(data->path_sf);

  CLAMP(data->path_ef, data->path_sf + 1, MAXFRAME / 2);
}

static void rna_AnimViz_path_end_frame_set(PointerRNA *ptr, int value)
{
  bAnimVizSettings *data = (bAnimVizSettings *)ptr->data;

  data->path_ef = value;
  CLAMP_MAX(data->path_sf, data->path_ef - 1);
  if (U.flag & USER_NONEGFRAMES) {
    CLAMP_MIN(data->path_sf, 0);
    CLAMP_MIN(data->path_ef, 1);
  }
}

#else

void rna_def_motionpath_common(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "motion_path", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "mpath");
  RNA_def_property_ui_text(prop, "Motion Path", "Motion Path for this element");
}

static void rna_def_animviz_motionpath_vert(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MotionPathVert", NULL);
  RNA_def_struct_sdna(srna, "bMotionPathVert");
  RNA_def_struct_ui_text(srna, "Motion Path Cache Point", "Cached location on path");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Coordinates", "");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOTIONPATH_VERT_SEL);
  RNA_def_property_ui_text(prop, "Select", "Path point is selected for editing");
}

static void rna_def_animviz_motion_path(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MotionPath", NULL);
  RNA_def_struct_sdna(srna, "bMotionPath");
  RNA_def_struct_ui_text(
      srna, "Motion Path", "Cache of the world-space positions of an element over a frame range");

  /* Collections */
  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "points", "length");
  RNA_def_property_struct_type(prop, "MotionPathVert");
  RNA_def_property_ui_text(prop, "Motion Path Points", "Cached positions per frame");

  /* Playback Ranges */
  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "start_frame");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Start Frame", "Starting frame of the stored range");

  prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "end_frame");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "End Frame", "End frame of the stored range");

  prop = RNA_def_property(srna, "length", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Length", "Number of frames cached");

  /* Custom Color */
  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Color", "Custom color for motion path");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  /* Line width */
  prop = RNA_def_property(srna, "line_thickness", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "line_thickness");
  RNA_def_property_range(prop, 1, 6);
  RNA_def_property_ui_text(prop, "Line Thickness", "Line thickness for motion path");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  /* Settings */
  prop = RNA_def_property(srna, "use_bone_head", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOTIONPATH_FLAG_BHEAD);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* xxx */
  RNA_def_property_ui_text(
      prop,
      "Use Bone Heads",
      "For PoseBone paths, use the bone head location when calculating this path");

  /* FIXME: Motion Paths are not currently editable... */
  prop = RNA_def_property(srna, "is_modified", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOTIONPATH_FLAG_EDIT);
  RNA_def_property_ui_text(prop, "Edit Path", "Path is being edited");

  /* Use custom color */
  prop = RNA_def_property(srna, "use_custom_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOTIONPATH_FLAG_CUSTOM);
  RNA_def_property_ui_text(prop, "Custom Colors", "Use custom color for this motion path");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  /* Draw lines between keyframes */
  prop = RNA_def_property(srna, "lines", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOTIONPATH_FLAG_LINES);
  RNA_def_property_ui_text(prop, "Lines", "Use straight lines between keyframe points");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);
}

/* --- */

static void rna_def_animviz_paths(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_type_items[] = {
      {MOTIONPATH_TYPE_ACFRA,
       "CURRENT_FRAME",
       0,
       "Around Frame",
       "Display Paths of poses within a fixed number of frames around the current frame"},
      {MOTIONPATH_TYPE_RANGE,
       "RANGE",
       0,
       "In Range",
       "Display Paths of poses within specified range"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "AnimVizMotionPaths", NULL);
  RNA_def_struct_sdna(srna, "bAnimVizSettings");
  RNA_def_struct_nested(brna, srna, "AnimViz");
  RNA_def_struct_ui_text(
      srna, "Motion Path Settings", "Motion Path settings for animation visualization");

  RNA_define_lib_overridable(true);

  /* Enums */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "path_type");
  RNA_def_property_enum_items(prop, prop_type_items);
  RNA_def_property_ui_text(prop, "Paths Type", "Type of range to show for Motion Paths");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  prop = RNA_def_property(srna, "bake_location", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "path_bakeflag");
  RNA_def_property_enum_items(prop, rna_enum_motionpath_bake_location_items);
  RNA_def_property_ui_text(prop, "Bake Location", "When calculating Bone Paths, use Head or Tips");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  /* Settings */
  prop = RNA_def_property(srna, "show_frame_numbers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "path_viewflag", MOTIONPATH_VIEW_FNUMS);
  RNA_def_property_ui_text(prop, "Show Frame Numbers", "Show frame numbers on Motion Paths");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  prop = RNA_def_property(srna, "show_keyframe_highlight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "path_viewflag", MOTIONPATH_VIEW_KFRAS);
  RNA_def_property_ui_text(
      prop, "Highlight Keyframes", "Emphasize position of keyframes on Motion Paths");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  prop = RNA_def_property(srna, "show_keyframe_numbers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "path_viewflag", MOTIONPATH_VIEW_KFNOS);
  RNA_def_property_ui_text(
      prop, "Show Keyframe Numbers", "Show frame numbers of Keyframes on Motion Paths");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  prop = RNA_def_property(srna, "show_keyframe_action_all", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "path_viewflag", MOTIONPATH_VIEW_KFACT);
  RNA_def_property_ui_text(
      prop,
      "All Action Keyframes",
      "For bone motion paths, search whole Action for keyframes instead of in group"
      " with matching name only (is slower)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  prop = RNA_def_property(srna, "frame_step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "path_step");
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_text(
      prop,
      "Frame Step",
      "Number of frames between paths shown (not for 'On Keyframes' Onion-skinning method)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  /* Playback Ranges */
  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "path_sf");
  RNA_def_property_int_funcs(prop, NULL, "rna_AnimViz_path_start_frame_set", NULL);
  RNA_def_property_ui_text(prop,
                           "Start Frame",
                           "Starting frame of range of paths to display/calculate "
                           "(not for 'Around Current Frame' Onion-skinning method)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "path_ef");
  RNA_def_property_int_funcs(prop, NULL, "rna_AnimViz_path_end_frame_set", NULL);
  RNA_def_property_ui_text(prop,
                           "End Frame",
                           "End frame of range of paths to display/calculate "
                           "(not for 'Around Current Frame' Onion-skinning method)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  /* Around Current Ranges */
  prop = RNA_def_property(srna, "frame_before", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "path_bc");
  RNA_def_property_range(prop, 1, MAXFRAMEF / 2);
  RNA_def_property_ui_text(prop,
                           "Before Current",
                           "Number of frames to show before the current frame "
                           "(only for 'Around Current Frame' Onion-skinning method)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  prop = RNA_def_property(srna, "frame_after", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "path_ac");
  RNA_def_property_range(prop, 1, MAXFRAMEF / 2);
  RNA_def_property_ui_text(prop,
                           "After Current",
                           "Number of frames to show after the current frame "
                           "(only for 'Around Current Frame' Onion-skinning method)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW_ANIMVIZ, NULL);

  /* Readonly Property - Do any motion paths exist/need updating? (Mainly for bone paths) */
  prop = RNA_def_property(srna, "has_motion_paths", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "path_bakeflag", MOTIONPATH_BAKE_HAS_PATHS);
  /* NOTE: This is really an internal state var for convenience, so don't allow edits! */
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Has Motion Paths", "Are there any bone paths that will need updating (read-only)");

  RNA_define_lib_overridable(false);
}

/* --- */

void rna_def_animviz_common(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "animation_visualization", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "avs");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Animation Visualization", "Animation data for this data-block");
}

static void rna_def_animviz(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AnimViz", NULL);
  RNA_def_struct_sdna(srna, "bAnimVizSettings");
  RNA_def_struct_ui_text(
      srna, "Animation Visualization", "Settings for the visualization of motion");

  /* motion path settings (nested struct) */
  prop = RNA_def_property(srna, "motion_path", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_struct_type(prop, "AnimVizMotionPaths");
  RNA_def_property_pointer_funcs(prop, "rna_AnimViz_motion_paths_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Motion Paths", "Motion Path settings for visualization");
}

/* --- */

void RNA_def_animviz(BlenderRNA *brna)
{
  rna_def_animviz(brna);
  rna_def_animviz_paths(brna);

  rna_def_animviz_motion_path(brna);
  rna_def_animviz_motionpath_vert(brna);
}

#endif
