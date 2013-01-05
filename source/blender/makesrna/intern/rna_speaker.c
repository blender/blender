/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Jörg Müller.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_speaker.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_speaker_types.h"
#include "DNA_sound_types.h"

#include "BLF_translation.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BKE_depsgraph.h"
#include "BKE_main.h"

#include "WM_api.h"
#include "WM_types.h"

#else

static void rna_def_speaker(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Speaker", "ID");
	RNA_def_struct_ui_text(srna, "Speaker", "Speaker datablock for 3D audio speaker objects");
	RNA_def_struct_ui_icon(srna, ICON_SPEAKER);

	prop = RNA_def_property(srna, "muted", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SPK_MUTED);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Mute", "Mute the speaker");
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_SOUND);
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

#if 0 /* This shouldn't be changed actually, hiding it! */
	prop = RNA_def_property(srna, "relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SPK_RELATIVE);
	RNA_def_property_ui_text(prop, "Relative", "Whether the source is relative to the camera or not");
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */
#endif

	prop = RNA_def_property(srna, "sound", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "sound");
	RNA_def_property_struct_type(prop, "Sound");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Sound", "Sound datablock used by this speaker");
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_sound_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	prop = RNA_def_property(srna, "volume_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "volume_max");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Maximum Volume", "Maximum volume, no matter how near the object is");
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_volume_max_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	prop = RNA_def_property(srna, "volume_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "volume_min");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Minimum Volume", "Minimum volume, no matter how far away the object is");
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_volume_min_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	prop = RNA_def_property(srna, "distance_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "distance_max");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Maximum Distance",
	                         "Maximum distance for volume calculation, no matter how far away the object is");
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_distance_max_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	prop = RNA_def_property(srna, "distance_reference", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "distance_reference");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Reference Distance", "Reference distance at which volume is 100 %");
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_distance_reference_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	prop = RNA_def_property(srna, "attenuation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "attenuation");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Attenuation", "How strong the distance affects volume, depending on distance model");
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_attenuation_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	prop = RNA_def_property(srna, "cone_angle_outer", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cone_angle_outer");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, 360.0f);
	RNA_def_property_ui_text(prop, "Outer Cone Angle",
	                         "Angle of the outer cone, in degrees, outside this cone the volume is "
	                         "the outer cone volume, between inner and outer cone the volume is interpolated");
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_cone_angle_outer_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	prop = RNA_def_property(srna, "cone_angle_inner", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cone_angle_inner");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, 360.0f);
	RNA_def_property_ui_text(prop, "Inner Cone Angle",
	                         "Angle of the inner cone, in degrees, inside the cone the volume is 100 %");
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_cone_angle_inner_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	prop = RNA_def_property(srna, "cone_volume_outer", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cone_volume_outer");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Outer Cone Volume", "Volume outside the outer cone");
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_cone_volume_outer_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	prop = RNA_def_property(srna, "volume", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "volume");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Volume", "How loud the sound is");
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_SOUND);
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_volume_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	prop = RNA_def_property(srna, "pitch", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pitch");
	RNA_def_property_range(prop, 0.1f, 10.0f);
	RNA_def_property_ui_text(prop, "Pitch", "Playback pitch of the sound");
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_SOUND);
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_pitch_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	/* common */
	rna_def_animdata_common(srna);
}


void RNA_def_speaker(BlenderRNA *brna)
{
	rna_def_speaker(brna);
}

#endif

