/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_sound_types.h"
#include "DNA_speaker_types.h"

#include "BLT_translation.h"

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "BKE_main.h"

#  include "WM_api.h"
#  include "WM_types.h"

#else

static void rna_def_speaker(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Speaker", "ID");
  RNA_def_struct_ui_text(srna, "Speaker", "Speaker data-block for 3D audio speaker objects");
  RNA_def_struct_ui_icon(srna, ICON_SPEAKER);

  prop = RNA_def_property(srna, "muted", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPK_MUTED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Mute", "Mute the speaker");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
#  if 0
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  prop = RNA_def_property(srna, "sound", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Sound");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Sound", "Sound data-block used by this speaker");
#  if 0
  RNA_def_property_float_funcs(prop, nullptr, "rna_Speaker_sound_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  prop = RNA_def_property(srna, "volume_max", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Maximum Volume", "Maximum volume, no matter how near the object is");
#  if 0
  RNA_def_property_float_funcs(prop, nullptr, "rna_Speaker_volume_max_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  prop = RNA_def_property(srna, "volume_min", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Minimum Volume", "Minimum volume, no matter how far away the object is");
#  if 0
  RNA_def_property_float_funcs(prop, nullptr, "rna_Speaker_volume_min_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  prop = RNA_def_property(srna, "distance_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(
      prop,
      "Maximum Distance",
      "Maximum distance for volume calculation, no matter how far away the object is");
#  if 0
  RNA_def_property_float_funcs(prop, nullptr, "rna_Speaker_distance_max_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  prop = RNA_def_property(srna, "distance_reference", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Reference Distance", "Reference distance at which volume is 100%");
#  if 0
  RNA_def_property_float_funcs(prop, nullptr, "rna_Speaker_distance_reference_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  prop = RNA_def_property(srna, "attenuation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Attenuation", "How strong the distance affects volume, depending on distance model");
#  if 0
  RNA_def_property_float_funcs(prop, nullptr, "rna_Speaker_attenuation_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  prop = RNA_def_property(srna, "cone_angle_outer", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, 360.0f);
  RNA_def_property_ui_text(
      prop,
      "Outer Cone Angle",
      "Angle of the outer cone, in degrees, outside this cone the volume is "
      "the outer cone volume, between inner and outer cone the volume is interpolated");
#  if 0
  RNA_def_property_float_funcs(prop, nullptr, "rna_Speaker_cone_angle_outer_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  prop = RNA_def_property(srna, "cone_angle_inner", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, 360.0f);
  RNA_def_property_ui_text(
      prop,
      "Inner Cone Angle",
      "Angle of the inner cone, in degrees, inside the cone the volume is 100%");
#  if 0
  RNA_def_property_float_funcs(prop, nullptr, "rna_Speaker_cone_angle_inner_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  prop = RNA_def_property(srna, "cone_volume_outer", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Outer Cone Volume", "Volume outside the outer cone");
#  if 0
  RNA_def_property_float_funcs(prop, nullptr, "rna_Speaker_cone_volume_outer_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  prop = RNA_def_property(srna, "volume", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Volume", "How loud the sound is");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
#  if 0
  RNA_def_property_float_funcs(prop, nullptr, "rna_Speaker_volume_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  prop = RNA_def_property(srna, "pitch", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.1f, 10.0f);
  RNA_def_property_ui_text(prop, "Pitch", "Playback pitch of the sound");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
#  if 0
  RNA_def_property_float_funcs(prop, nullptr, "rna_Speaker_pitch_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Speaker_update");
#  endif

  /* common */
  rna_def_animdata_common(srna);
}

void RNA_def_speaker(BlenderRNA *brna)
{
  rna_def_speaker(brna);
}

#endif
