/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Campbell Barton, Roland Hess
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_sound_types.h"
#include "DNA_property_types.h"

#ifdef RNA_RUNTIME

#else

/* sample and listener are internal .. */

#if 0
static void rna_def_sample(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* sound types */
	static EnumPropertyItem prop_sample_type_items[] = {
		{SAMPLE_INVALID, "INVALID", "Invalid", ""},
		{SAMPLE_UNKNOWN, "UNKNOWN", "Unknown", ""},
		{SAMPLE_RAW, "RAW", "Raw", ""},
		{SAMPLE_WAV, "WAV", "WAV", "Uncompressed"},
		{SAMPLE_MP2, "MP2", "MP2", "MPEG-1 Audio Layer 2"},
		{SAMPLE_MP3, "MP3", "MP3", "MPEG-1 Audio Layer 3"},
		{SAMPLE_OGG_VORBIS, "OGG_VORBIS", "Ogg Vorbis", ""},
		{SAMPLE_WMA, "WMA", "WMA", "Windows Media Audio"},
		{SAMPLE_ASF, "ASF", "ASF", "Windows Advanced Systems Format"},
		{SAMPLE_AIFF, "AIFF", "AIFF", "Audio Interchange File Format"},
		{0, NULL, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "SoundSample", "ID");
	RNA_def_struct_sdna(srna, "bSample");
	RNA_def_struct_ui_text(srna, "SoundSample", "Sound data loaded from a sound datablock.");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_sample_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); 
	RNA_def_property_ui_text(prop, "Types", "");

	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); 
	RNA_def_property_ui_text(prop, "Filename", "Full path filename of the sample");

	prop= RNA_def_property(srna, "length", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "len");
	RNA_def_property_ui_text(prop, "Length", "The length of sample in seconds");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "rate", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Rate", "Sample rate in kHz");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "bits", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Bits", "Bit-depth of sample");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "channels", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Channels", "Number of channels (mono=1; stereo=2)");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); 
}

static void rna_def_soundlistener(BlenderRNA *brna)
{

	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "SoundListener", "ID");
	RNA_def_struct_sdna(srna, "bSoundListener");
	RNA_def_struct_ui_text(srna, "Sound Listener", "Sound listener defining parameters about how sounds are played.");

	prop= RNA_def_property(srna, "gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Gain", "Overall volume for Game Engine sound.");
	RNA_def_property_ui_range(prop, 0.0, 1.0, 10, 4);

	prop= RNA_def_property(srna, "doppler_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dopplerfactor");
	RNA_def_property_ui_text(prop, "Doppler Factor", "Amount of Doppler effect in Game Engine sound.");
	RNA_def_property_ui_range(prop, 0.0, 10.0, 1, 4);

	prop= RNA_def_property(srna, "doppler_velocity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dopplervelocity");
	RNA_def_property_ui_text(prop, "Doppler Velocity", "The speed of sound in the Game Engine.");
	RNA_def_property_ui_range(prop, 0.0, 10000.0, 0.1, 4);

	prop= RNA_def_property(srna, "num_sounds_blender", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "numsoundsblender");
	RNA_def_property_ui_text(prop, "Total Sounds in Blender", "The total number of sounds currently linked and available.");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "num_sounds_gameengine", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "numsoundsgameengine");
	RNA_def_property_ui_text(prop, "Total Sounds in Game Engine", "The total number of sounds in the Game Engine.");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}
#endif

static void rna_def_sound(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Sound", "ID");
	RNA_def_struct_sdna(srna, "bSound");
	RNA_def_struct_ui_text(srna, "Sound", "Sound datablock referencing an external or packed sound file.");

	//rna_def_ipo_common(srna);

	/*prop= RNA_def_property(srna, "sample", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "SoundSample");
	RNA_def_property_ui_text(prop, "Sample", "Sound sample.");*/

	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Filename", "Sound sample file used by this Sound datablock.");

	prop= RNA_def_property(srna, "packed_file", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "packedfile");
	RNA_def_property_ui_text(prop, "Packed File", "");

	/* game engine settings */
	prop= RNA_def_property(srna, "volume", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "volume");
	RNA_def_property_ui_range(prop, 0.0, 1.0, 10, 4);
	RNA_def_property_ui_text(prop, "Volume", "Game engine only: volume for this sound.");

	prop= RNA_def_property(srna, "pitch", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pitch");
	RNA_def_property_ui_range(prop, -12.0, 12.0, 10, 4);
	RNA_def_property_ui_text(prop, "Pitch", "Game engine only: set the pitch of this sound.");

	prop= RNA_def_property(srna, "loop", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_LOOP);
	RNA_def_property_ui_text(prop, "Sound Loop", "Game engine only: toggle between looping on/off.");

	prop= RNA_def_property(srna, "ping_pong", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_BIDIRECTIONAL_LOOP);
	RNA_def_property_ui_text(prop, "Ping Pong", "Game engine only: Toggle between A->B and A->B->A looping.");

	prop= RNA_def_property(srna, "sound_3d", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_3D);
	RNA_def_property_ui_text(prop, "3D Sound", "Game engine only: turns 3D sound on.");

	prop= RNA_def_property(srna, "attenuation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "attenuation");
	RNA_def_property_range(prop, 0.0, 5.0);
	RNA_def_property_ui_text(prop, "Attenuation", "Game engine only: sets the surround scaling factor for 3D sound.");

	/* gain */
	prop= RNA_def_property(srna, "min_gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min_gain");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Min Gain", "Minimal gain which is always guaranteed for this sound.");

	prop= RNA_def_property(srna, "max_gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_gain");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Max Gain", "Maximal gain which is always guaranteed for this sound.");

	prop= RNA_def_property(srna, "reference_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "distance");
	RNA_def_property_ui_text(prop, "Reference Distance", "Reference distance at which the listener will experience gain.");
	RNA_def_property_ui_range(prop, 0.0, 1000.0, 10, 4); /* NOT used anywhere */

	/* unused
	prop= RNA_def_property(srna, "panning", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "panning");
	RNA_def_property_ui_range(prop, -1.0, 1.0, 10, 4);
	RNA_def_property_ui_text(prop, "Panning", "Pan the sound from left to right"); */

	/* unused
	prop= RNA_def_property(srna, "fixed_volume", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_FIXED_VOLUME);
	RNA_def_property_ui_text(prop, "Fixed Volume", "Constraint sound to fixed volume."); */

	/* unused
	prop= RNA_def_property(srna, "fixed_panning", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_FIXED_PANNING);
	RNA_def_property_ui_text(prop, "Fixed Panning", "Constraint sound to fixed panning."); */

	/* unused
	prop= RNA_def_property(srna, "priority", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_PRIORITY);
	RNA_def_property_ui_text(prop, "Priority", "Make sound higher priority."); */
}

void RNA_def_sound(BlenderRNA *brna)
{
	//rna_def_sample(brna);
	//rna_def_soundlistener(brna);
	rna_def_sound(brna);
}

#endif


