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

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_sound_types.h"
#include "DNA_property_types.h"

#ifdef RNA_RUNTIME

#else

void RNA_def_sample(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* sound types */
	static EnumPropertyItem prop_sample_type_items[] = {
		{SAMPLE_INVALID, "SAMPLE_INVALID", "Invalid", ""},
		{SAMPLE_UNKNOWN, "SAMPLE_UNKNOWN", "Unknown", ""},
		{SAMPLE_RAW, "SAMPLE_RAW", "Raw", ""},
		{SAMPLE_WAV, "SAMPLE_WAV", "WAV", "Uncompressed"},
		{SAMPLE_MP2, "SAMPLE_MP2", "MP2", "MPEG-1 Audio Layer 2"},
		{SAMPLE_MP3, "SAMPLE_MP3", "MP3", "MPEG-1 Audio Layer 3"},
		{SAMPLE_OGG_VORBIS, "SAMPLE_OGG_VORBIS", "Ogg Vorbis", ""},
		{SAMPLE_WMA, "SAMPLE_WMA", "WMA", "Windows Media Audio"},
		{SAMPLE_ASF, "SAMPLE_ASF", "ASF", "Windows Advanced Systems Format"},
		{SAMPLE_AIFF, "SAMPLE_AIFF", "AIFF", "Audio Interchange File Format"},
		{0, NULL, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "sample", "ID");
	RNA_def_struct_sdna(srna, "bSample");
	RNA_def_struct_ui_text(srna, "SoundSample", "Sound Sample");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_sample_type_items);
	RNA_def_property_ui_text(prop, "Types", "");

	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE); 
	RNA_def_property_ui_text(prop, "Filename", "Full path filename of the sample");

	prop= RNA_def_property(srna, "length", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "len");
	RNA_def_property_ui_text(prop, "Length", "The length of sample in seconds");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);

	prop= RNA_def_property(srna, "rate", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Rate", "Sample rate in kHz");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);

	prop= RNA_def_property(srna, "bits", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Bits", "Bit-depth of sample");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);

	prop= RNA_def_property(srna, "channels", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Channels", "Number of channels (mono=1; stereo=2)");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE); 

}

void RNA_def_sound(BlenderRNA *brna)
{
	/* TODO - bSoundListener */

	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Sound", "ID");
	RNA_def_struct_sdna(srna, "bSound");
	RNA_def_struct_ui_text(srna, "Sound", "DOC_BROKEN");

	prop= RNA_def_property(srna, "sample", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_ui_text(prop, "Sample", "Sound Sample.");

	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE); /* ? */
	RNA_def_property_ui_text(prop, "Filename", "DOC_BROKEN");

	/* floats */
	prop= RNA_def_property(srna, "volume", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "volume");
	RNA_def_property_ui_text(prop, "Volume", "The volume for this sound in the game engine only");
	RNA_def_property_ui_range(prop, 0.0, 1.0, 10, 4);

	prop= RNA_def_property(srna, "panning", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "panning");
	RNA_def_property_ui_text(prop, "Panning", "Pan the sound from left to right");
	RNA_def_property_ui_range(prop, -1.0, 1.0, 10, 4); /* TODO - this isnt used anywhere :/ */

	prop= RNA_def_property(srna, "attenuation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "attenuation");
	RNA_def_property_ui_text(prop, "Attenuation", "DOC_BROKEN");
	RNA_def_property_ui_range(prop, 0.0, 100.0, 10, 4); /* TODO check limits */

	prop= RNA_def_property(srna, "pitch", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pitch");
	RNA_def_property_ui_text(prop, "Pitch", "Set the pitch of this sound for the game engine only");
	RNA_def_property_ui_range(prop, -12.0, 12.0, 10, 4);

	prop= RNA_def_property(srna, "min_gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min_gain");
	RNA_def_property_ui_text(prop, "Min Gain", "DOC_BROKEN");
	RNA_def_property_ui_range(prop, 0.0, 1.0, 10, 4); /* NOT used anywhere */

	prop= RNA_def_property(srna, "max_gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_gain");
	RNA_def_property_ui_text(prop, "Max Gain", "DOC_BROKEN");
	RNA_def_property_ui_range(prop, 0.0, 1.0, 10, 4); /* NOT used anywhere */

	prop= RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "distance");
	RNA_def_property_ui_text(prop, "Distance", "Reference distance at which the listener will experience gain");
	RNA_def_property_ui_range(prop, 0.0, 1000.0, 10, 4); /* NOT used anywhere */

	prop= RNA_def_property(srna, "ipo", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Ipo");
	RNA_def_property_ui_text(prop, "Ipo", "DOC_BROKEN");


	/* flags */
	prop= RNA_def_property(srna, "loop", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_LOOP); /* use bitflags */
	RNA_def_property_ui_text(prop, "Sound Loop", "DOC_BROKEN");

	prop= RNA_def_property(srna, "fixed_volume", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_FIXED_VOLUME); /* use bitflags */
	RNA_def_property_ui_text(prop, "Fixed Volume", "DOC_BROKEN");

	prop= RNA_def_property(srna, "fixed_panning", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_FIXED_PANNING); /* use bitflags */
	RNA_def_property_ui_text(prop, "Fixed Panning", "DOC_BROKEN");

	prop= RNA_def_property(srna, "spacial", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_3D); /* use bitflags */
	RNA_def_property_ui_text(prop, "3D", "DOC_BROKEN");

	prop= RNA_def_property(srna, "bidirectional_loop", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_BIDIRECTIONAL_LOOP); /* use bitflags */
	RNA_def_property_ui_text(prop, "Bidirectional Loop", "DOC_BROKEN");

	prop= RNA_def_property(srna, "priority", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_PRIORITY); /* use bitflags */
	RNA_def_property_ui_text(prop, "Priority", "DOC_BROKEN");

	prop= RNA_def_property(srna, "sequence", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SOUND_FLAGS_SEQUENCE); /* use bitflags */
	RNA_def_property_ui_text(prop, "Priority", "DOC_BROKEN");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	
}

#endif


