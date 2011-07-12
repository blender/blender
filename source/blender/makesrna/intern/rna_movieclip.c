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
#include "BKE_tracking.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_depsgraph.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

static void rna_MovieClip_reload_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	MovieClip *clip= (MovieClip*)ptr->data;

	BKE_movieclip_reload(clip);
	DAG_id_tag_update(&clip->id, 0);
}

static void rna_MovieClip_size_get(PointerRNA *ptr, int *values)
{
	MovieClip *clip= (MovieClip*)ptr->data;

	values[0]= clip->lastsize[0];
	values[1]= clip->lastsize[1];
}

static void rna_MovieClip_resolution_get(PointerRNA *ptr, float *values)
{
	MovieClip *clip= (MovieClip*)ptr->data;
	ImBuf *ibuf;

	ibuf= BKE_movieclip_acquire_ibuf(clip, NULL);
	if (ibuf) {
		values[0]= ibuf->ppm[0];
		values[1]= ibuf->ppm[1];

		IMB_freeImBuf(ibuf);
	}
	else {
		values[0]= 0;
		values[1]= 0;
	}
}

#else

static void rna_def_moviecliUuser(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MovieClipUser", NULL);
	RNA_def_struct_ui_text(srna, "Movie Clip User", "Parameters defining how an MovieClip datablock is used by another datablock");

	prop= RNA_def_property(srna, "current_frame", PROP_INT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_sdna(prop, NULL, "framenr");
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Current Frame", "Get frame number user is points to in clip");
}

static void rna_def_movieclip(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MovieClip", "ID");
	RNA_def_struct_ui_text(srna, "MovieClip", "MovieClip datablock referencing an external movie file");
	RNA_def_struct_ui_icon(srna, ICON_SEQUENCE);

	prop= RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "File Path", "Filename of the text file");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_MovieClip_reload_update");

	prop= RNA_def_property(srna, "tracking", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTracking");

	prop= RNA_def_int_vector(srna, "size" , 2 , NULL , 0, 0, "Size" , "Width and height in pixels, zero when image data cant be loaded" , 0 , 0);
	RNA_def_property_int_funcs(prop, "rna_MovieClip_size_get" , NULL, NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_float_vector(srna, "resolution" , 2 , NULL , 0, 0, "Resolution" , "X/Y pixels per meter" , 0 , 0);
	RNA_def_property_float_funcs(prop, "rna_MovieClip_resolution_get", NULL, NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

void RNA_def_movieclip(BlenderRNA *brna)
{
	rna_def_movieclip(brna);
	rna_def_moviecliUuser(brna);
}

#endif
