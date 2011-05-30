/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_movieclip.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BKE_movieclip.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_depsgraph.h"

static void rna_MovieClip_reload_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	MovieClip *clip= (MovieClip*)ptr->data;

	BKE_movieclip_reload(clip);
	DAG_id_tag_update(&clip->id, 0);
}

#else

static void rna_def_movie_trackingCamera(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MovieTrackingCamera", NULL);
	RNA_def_struct_ui_text(srna, "Movie tracking camera data", "Match-moving camera data for tracking");

	prop= RNA_def_property(srna, "focal_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "focal");
	RNA_def_property_range(prop, 1.0f, 5000.0f);
	RNA_def_property_ui_text(prop, "Focal Length", "Camera's focal length in millimeters");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);
}

static void rna_def_movie_tracking(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	rna_def_movie_trackingCamera(brna);

	srna= RNA_def_struct(brna, "MovieTracking", NULL);
	RNA_def_struct_ui_text(srna, "Movie tracking data", "Match-moving data for tracking");

	prop= RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingCamera");
}

static void rna_def_movieclip(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	rna_def_movie_tracking(brna);

	srna= RNA_def_struct(brna, "MovieClip", "ID");
	RNA_def_struct_ui_text(srna, "MovieClip", "MovieClip datablock referencing an external movie file");
	RNA_def_struct_ui_icon(srna, ICON_SEQUENCE);

	prop= RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "File Path", "Filename of the text file");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_MovieClip_reload_update");

	prop= RNA_def_property(srna, "tracking", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTracking");
}

static void rna_def_movieclip_tools(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem tools_items[] = {
		{MCLIP_TOOL_NONE, "NONE", 0, "None", "Don't use any tool"},
		{MCLIP_TOOL_FOOTAGE, "FOOTAGE", ICON_SEQUENCE, "Footage", "Footage sequence/movie tools"},
		{MCLIP_TOOL_CAMERA, "CAMERA", ICON_CAMERA_DATA, "Camera", "Camera tools"},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "MovieClipEditSettings", NULL);
	RNA_def_struct_ui_text(srna, "MovieClipEditSettings", "MovieClip editing tool settings");

	prop= RNA_def_property(srna, "tool", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "tool");
	RNA_def_property_enum_items(prop, tools_items);
	RNA_def_property_ui_text(prop, "Tool", "Tool to interact with movie clip");
}

void RNA_def_movieclip(BlenderRNA *brna)
{
	rna_def_movieclip(brna);
	rna_def_movieclip_tools(brna);
}

#endif
