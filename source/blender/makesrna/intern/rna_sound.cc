/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "RNA_define.hh"

#include "rna_internal.hh"

#include "DNA_sound_types.h"

#include "BKE_sound.hh"

/* Enumeration for Audio Channels, compatible with eSoundChannels */
static const EnumPropertyItem rna_enum_audio_channels_items[] = {
    {SOUND_CHANNELS_INVALID, "INVALID", ICON_NONE, "Invalid", "Invalid"},
    {SOUND_CHANNELS_MONO, "MONO", ICON_NONE, "Mono", "Mono"},
    {SOUND_CHANNELS_STEREO, "STEREO", ICON_NONE, "Stereo", "Stereo"},
    {SOUND_CHANNELS_STEREO_LFE, "STEREO_LFE", ICON_NONE, "Stereo LFE", "Stereo FX"},
    {SOUND_CHANNELS_SURROUND4, "CHANNELS_4", ICON_NONE, "4 Channels", "4 Channels"},
    {SOUND_CHANNELS_SURROUND5, "CHANNELS_5", ICON_NONE, "5 Channels", "5 Channels"},
    {SOUND_CHANNELS_SURROUND51, "SURROUND_51", ICON_NONE, "5.1 Surround", "5.1 Surround"},
    {SOUND_CHANNELS_SURROUND61, "SURROUND_61", ICON_NONE, "6.1 Surround", "6.1 Surround"},
    {SOUND_CHANNELS_SURROUND71, "SURROUND_71", ICON_NONE, "7.1 Surround", "7.1 Surround"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "BKE_context.hh"
#  include "BKE_library.hh"

#  include "DEG_depsgraph.hh"

#  include "SEQ_sequencer.hh"
#  include "SEQ_utils.hh"

static void rna_Sound_update(Main * /*bmain*/, Scene *scene, PointerRNA *ptr)
{
  bSound *sound = (bSound *)ptr->data;
  blender::seq::media_presence_invalidate_sound(scene, sound);
  DEG_id_tag_update(&sound->id, ID_RECALC_AUDIO);
}

static void rna_Sound_caching_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Sound_update(bmain, scene, ptr);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
}

#else

static void rna_def_sound(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Sound", "ID");
  RNA_def_struct_sdna(srna, "bSound");
  RNA_def_struct_ui_text(
      srna, "Sound", "Sound data-block referencing an external or packed sound file");
  RNA_def_struct_ui_icon(srna, ICON_SOUND);

  // rna_def_ipo_common(srna);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, nullptr, "filepath");
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_ui_text(prop, "File Path", "Sound sample file used by this Sound data-block");
  RNA_def_property_update(prop, 0, "rna_Sound_update");

  prop = RNA_def_property(srna, "packed_file", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "packedfile");
  RNA_def_property_ui_text(prop, "Packed File", "");

  prop = RNA_def_property(srna, "use_memory_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", SOUND_FLAGS_CACHING);
  RNA_def_property_ui_text(prop, "Caching", "The sound file is decoded and loaded into RAM");
  RNA_def_property_update(prop, 0, "rna_Sound_caching_update");

  prop = RNA_def_property(srna, "use_mono", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", SOUND_FLAGS_MONO);
  RNA_def_property_ui_text(
      prop,
      "Mono",
      "If the file contains multiple audio channels they are rendered to a single one");
  RNA_def_property_update(prop, 0, "rna_Sound_update");

  prop = RNA_def_property(srna, "samplerate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "samplerate");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Sample Rate", "Sample rate of the audio in Hz");

  prop = RNA_def_property(srna, "channels", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "audio_channels");
  RNA_def_property_enum_items(prop, rna_enum_audio_channels_items);
  RNA_def_property_enum_default(prop, SOUND_CHANNELS_INVALID);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Audio channels", "Definition of audio channels");

  RNA_api_sound(srna);
}

void RNA_def_sound(BlenderRNA *brna)
{
  rna_def_sound(brna);
}

#endif
