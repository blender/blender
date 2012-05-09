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
#include "BKE_tracking.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "WM_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#ifdef RNA_RUNTIME

#include "BKE_depsgraph.h"

static void rna_MovieClip_reload_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;

	BKE_movieclip_reload(clip);
	DAG_id_tag_update(&clip->id, 0);
}

static void rna_MovieClip_size_get(PointerRNA *ptr, int *values)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;

	values[0] = clip->lastsize[0];
	values[1] = clip->lastsize[1];
}

#else

static void rna_def_movieclip_proxy(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem clip_tc_items[] = {
		{IMB_TC_NONE, "NONE", 0, "No TC in use", ""},
		{IMB_TC_RECORD_RUN, "RECORD_RUN", 0, "Record Run", "Use images in the order they are recorded"},
		{IMB_TC_FREE_RUN, "FREE_RUN", 0, "Free Run", "Use global timestamp written by recording device"},
		{IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN, "FREE_RUN_REC_DATE", 0, "Free Run (rec date)",
		                                        "Interpolate a global timestamp using the record date and time "
		                                        "written by recording device"},
		{IMB_TC_RECORD_RUN_NO_GAPS, "FREE_RUN_NO_GAPS", 0, "Free Run No Gaps",
		                            "Record run, but ignore timecode, changes in framerate or dropouts"},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "MovieClipProxy", NULL);
	RNA_def_struct_ui_text(srna, "Movie Clip Proxy", "Proxy parameters for a movie clip");
	RNA_def_struct_sdna(srna, "MovieClipProxy");

	/* build proxy sized */
	prop = RNA_def_property(srna, "build_25", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flag", MCLIP_PROXY_SIZE_25);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "25%", "Build proxy resolution 25% of the original footage dimension");

	prop = RNA_def_property(srna, "build_50", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flag", MCLIP_PROXY_SIZE_50);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "50%", "Build proxy resolution 50% of the original footage dimension");

	prop = RNA_def_property(srna, "build_75", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flag", MCLIP_PROXY_SIZE_75);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "75%", "Build proxy resolution 75% of the original footage dimension");

	prop = RNA_def_property(srna, "build_100", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flag", MCLIP_PROXY_SIZE_100);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "100%", "Build proxy resolution 100% of the original footage dimension");

	prop = RNA_def_property(srna, "build_undistorted_25", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flag", MCLIP_PROXY_UNDISTORTED_SIZE_25);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "25%", "Build proxy resolution 25% of the original undistorted footage dimension");

	prop = RNA_def_property(srna, "build_undistorted_50", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flag", MCLIP_PROXY_UNDISTORTED_SIZE_50);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "50%", "Build proxy resolution 50% of the original undistorted footage dimension");

	prop = RNA_def_property(srna, "build_undistorted_75", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flag", MCLIP_PROXY_UNDISTORTED_SIZE_75);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "75%", "Build proxy resolution 75% of the original undistorted footage dimension");

	prop = RNA_def_property(srna, "build_undistorted_100", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_size_flag", MCLIP_PROXY_UNDISTORTED_SIZE_100);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "100%",
	                         "Build proxy resolution 100% of the original undistorted footage dimension");

	/* build timecodes */
	prop = RNA_def_property(srna, "build_record_run", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_tc_flag", IMB_TC_RECORD_RUN);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Rec Run", "Build record run time code index");

	prop = RNA_def_property(srna, "build_free_run", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_tc_flag", IMB_TC_FREE_RUN);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Free Run", "Build free run time code index");

	prop = RNA_def_property(srna, "build_free_run_rec_date", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "build_tc_flag", IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Free Run (Rec Date)", "Build free run time code index using Record Date/Time");

	/* quality of proxied image */
	prop = RNA_def_property(srna, "quality", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "quality");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Quality", "JPEG quality of proxy images");
	RNA_def_property_ui_range(prop, 1, 100, 1, 0);

	prop = RNA_def_property(srna, "timecode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "tc");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, clip_tc_items);
	RNA_def_property_ui_text(prop, "Timecode", "");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* directory */
	prop = RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "dir");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Directory", "Location to store the proxy files");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_MovieClip_reload_update");
}

static void rna_def_moviecliUser(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem clip_render_size_items[] = {
		{MCLIP_PROXY_RENDER_SIZE_25, "PROXY_25", 0, "Proxy size 25%", ""},
		{MCLIP_PROXY_RENDER_SIZE_50, "PROXY_50", 0, "Proxy size 50%", ""},
		{MCLIP_PROXY_RENDER_SIZE_75, "PROXY_75", 0, "Proxy size 75%", ""},
		{MCLIP_PROXY_RENDER_SIZE_100, "PROXY_100", 0, "Proxy size 100%", ""},
		{MCLIP_PROXY_RENDER_SIZE_FULL, "FULL", 0, "No proxy, full render", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "MovieClipUser", NULL);
	RNA_def_struct_ui_text(srna, "Movie Clip User",
	                       "Parameters defining how a MovieClip datablock is used by another datablock");

	prop = RNA_def_property(srna, "current_frame", PROP_INT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_sdna(prop, NULL, "framenr");
	RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Current Frame", "Current frame number in movie or image sequence");

	/* render size */
	prop = RNA_def_property(srna, "proxy_render_size", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "render_size");
	RNA_def_property_enum_items(prop, clip_render_size_items);
	RNA_def_property_ui_text(prop, "Proxy render size",
	                         "Draw preview using full resolution or different proxy resolutions");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* render undistorted */
	prop = RNA_def_property(srna, "use_render_undistorted", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "render_flag", MCLIP_PROXY_RENDER_UNDISTORT);
	RNA_def_property_ui_text(prop, "Render Undistorted", "Render preview using undistorted proxy");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);
}

static void rna_def_movieClipScopes(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "MovieClipScopes", NULL);
	RNA_def_struct_ui_text(srna, "MovieClipScopes", "Scopes for statistical view of a movie clip");
}


static void rna_def_movieclip(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem clip_source_items[] = {
		{MCLIP_SRC_SEQUENCE, "SEQUENCE", 0, "Image Sequence", "Multiple image files, as a sequence"},
		{MCLIP_SRC_MOVIE, "MOVIE", 0, "Movie File", "Movie file"},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "MovieClip", "ID");
	RNA_def_struct_ui_text(srna, "MovieClip", "MovieClip datablock referencing an external movie file");
	RNA_def_struct_ui_icon(srna, ICON_SEQUENCE);

	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "File Path", "Filename of the movie or sequence file");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_MovieClip_reload_update");

	prop = RNA_def_property(srna, "tracking", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTracking");

	prop = RNA_def_property(srna, "proxy", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieClipProxy");

	/* use proxy */
	prop = RNA_def_property(srna, "use_proxy", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MCLIP_USE_PROXY);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Use Proxy / Timecode",
	                         "Use a preview proxy and/or timecode index for this clip");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	prop = RNA_def_int_vector(srna, "size", 2, NULL, 0, 0, "Size",
	                          "Width and height in pixels, zero when image data cant be loaded", 0, 0);
	RNA_def_property_int_funcs(prop, "rna_MovieClip_size_get", NULL, NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "display_aspect", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "aspx");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, 0.1f, 5000.0f);
	RNA_def_property_ui_range(prop, 0.1f, 5000.0f, 1, 2);
	RNA_def_property_ui_text(prop, "Display Aspect", "Display Aspect for this clip, does not affect rendering");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* source */
	prop = RNA_def_property(srna, "source", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, clip_source_items);
	RNA_def_property_ui_text(prop, "Source", "Where the clip comes from");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	/* custom proxy directory */
	prop = RNA_def_property(srna, "use_proxy_custom_directory", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MCLIP_USE_PROXY_CUSTOM_DIR);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Proxy Custom Directory",
	                         "Create proxy images in a custom directory (default is movie location)");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_MovieClip_reload_update");

	/* grease pencil */
	prop = RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "gpd");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "GreasePencil");
	RNA_def_property_ui_text(prop, "Grease Pencil", "Grease pencil data for this movie clip");
}

void RNA_def_movieclip(BlenderRNA *brna)
{
	rna_def_movieclip(brna);
	rna_def_movieclip_proxy(brna);
	rna_def_moviecliUser(brna);
	rna_def_movieClipScopes(brna);
}

#endif
