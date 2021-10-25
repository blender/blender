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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/blender_copybuffer.c
 *  \ingroup bke
 *
 * Used for copy/paste operator, (using a temporary file).
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_callbacks.h"

#include "IMB_imbuf.h"
#include "IMB_moviecache.h"

#include "BKE_blender_copybuffer.h"  /* own include */
#include "BKE_blendfile.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "BLO_readfile.h"
#include "BLO_writefile.h"

#include "IMB_colormanagement.h"


/* -------------------------------------------------------------------- */

/** \name Copy/Paste `.blend`, partial saves.
 * \{ */

void BKE_copybuffer_begin(Main *bmain_src)
{
	BKE_blendfile_write_partial_begin(bmain_src);
}

void BKE_copybuffer_tag_ID(ID *id)
{
	BKE_blendfile_write_partial_tag_ID(id, true);
}

/**
 * \return Success.
 */
bool BKE_copybuffer_save(Main *bmain_src, const char *filename, ReportList *reports)
{
	const int write_flags = G_FILE_RELATIVE_REMAP;

	bool retval = BKE_blendfile_write_partial(bmain_src, filename, write_flags, reports);

	BKE_blendfile_write_partial_end(bmain_src);

	return retval;
}

bool BKE_copybuffer_read(Main *bmain_dst, const char *libname, ReportList *reports)
{
	BlendHandle *bh = BLO_blendhandle_from_file(libname, reports);
	if (bh == NULL) {
		/* Error reports will have been made by BLO_blendhandle_from_file(). */
		return false;
	}
	/* Here appending/linking starts. */
	Main *mainl = BLO_library_link_begin(bmain_dst, &bh, libname);
	BLO_library_link_copypaste(mainl, bh);
	BLO_library_link_end(mainl, &bh, 0, NULL, NULL);
	/* Mark all library linked objects to be updated. */
	BKE_main_lib_objects_recalc_all(bmain_dst);
	IMB_colormanagement_check_file_config(bmain_dst);
	/* Append, rather than linking. */
	Library *lib = BLI_findstring(&bmain_dst->library, libname, offsetof(Library, filepath));
	BKE_library_make_local(bmain_dst, lib, NULL, true, false);
	/* Important we unset, otherwise these object wont
	 * link into other scenes from this blend file.
	 */
	BKE_main_id_tag_all(bmain_dst, LIB_TAG_PRE_EXISTING, false);
	BLO_blendhandle_close(bh);
	return true;
}

/**
 * \return Success.
 */
bool BKE_copybuffer_paste(bContext *C, const char *libname, const short flag, ReportList *reports)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	Main *mainl = NULL;
	Library *lib;
	BlendHandle *bh;

	bh = BLO_blendhandle_from_file(libname, reports);

	if (bh == NULL) {
		/* error reports will have been made by BLO_blendhandle_from_file() */
		return false;
	}

	BKE_scene_base_deselect_all(scene);

	/* tag everything, all untagged data can be made local
	 * its also generally useful to know what is new
	 *
	 * take extra care BKE_main_id_flag_all(bmain, LIB_TAG_PRE_EXISTING, false) is called after! */
	BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);

	/* here appending/linking starts */
	mainl = BLO_library_link_begin(bmain, &bh, libname);

	BLO_library_link_copypaste(mainl, bh);

	BLO_library_link_end(mainl, &bh, flag, scene, v3d);

	/* mark all library linked objects to be updated */
	BKE_main_lib_objects_recalc_all(bmain);
	IMB_colormanagement_check_file_config(bmain);

	/* append, rather than linking */
	lib = BLI_findstring(&bmain->library, libname, offsetof(Library, filepath));
	BKE_library_make_local(bmain, lib, NULL, true, false);

	/* important we unset, otherwise these object wont
	 * link into other scenes from this blend file */
	BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

	/* recreate dependency graph to include new objects */
	DAG_relations_tag_update(bmain);

	BLO_blendhandle_close(bh);
	/* remove library... */

	return true;
}

/** \} */
