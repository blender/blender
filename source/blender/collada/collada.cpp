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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/collada.cpp
 *  \ingroup collada
 */


/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include "DocumentExporter.h"
#include "DocumentImporter.h"

extern "C"
{
#include "BKE_scene.h"
#include "BKE_context.h"

/* make dummy file */
#include "BLI_storage.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"

	int collada_import(bContext *C, const char *filepath)
	{
		DocumentImporter imp (C, filepath);
		imp.import();

		return 1;
	}

	int collada_export(Scene *sce, const char *filepath)
	{
		DocumentExporter exp;

		/* annoying, collada crashes if file cant be created! [#27162] */
		if(!BLI_exist(filepath)) {
			BLI_make_existing_file(filepath); /* makes the dir if its not there */
			if(BLI_touch(filepath) == 0) {
				return 0;
			}
		}
		/* end! */

		exp.exportCurrentScene(sce, filepath);

		return 1;
	}
}
