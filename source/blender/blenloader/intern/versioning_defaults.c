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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenloader/intern/versioning_defaults.c
 *  \ingroup blenloader
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_object_types.h"
#include "DNA_workspace_types.h"

#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_screen.h"

#include "BLO_readfile.h"

/**
 * Override values in in-memory startup.blend, avoids resaving for small changes.
 */
void BLO_update_defaults_userpref_blend(void)
{
	/* default so DPI is detected automatically */
	U.dpi = 0;
	U.ui_scale = 1.0f;

#ifdef WITH_PYTHON_SECURITY
	/* use alternative setting for security nuts
	 * otherwise we'd need to patch the binary blob - startup.blend.c */
	U.flag |= USER_SCRIPT_AUTOEXEC_DISABLE;
#else
	U.flag &= ~USER_SCRIPT_AUTOEXEC_DISABLE;
#endif

	/* Ignore the theme saved in the blend file,
	 * instead use the theme from 'userdef_default_theme.c' */
	{
		bTheme *theme = U.themes.first;
		memcpy(theme, &U_theme_default, sizeof(bTheme));
	}
}

/**
 * Update defaults in startup.blend, without having to save and embed the file.
 * This function can be emptied each time the startup.blend is updated. */
void BLO_update_defaults_startup_blend(Main *bmain)
{
	for (bScreen *screen = bmain->screen.first; screen; screen = screen->id.next) {
		for (ScrArea *area = screen->areabase.first; area; area = area->next) {
			for (ARegion *ar = area->regionbase.first; ar; ar = ar->next) {
				/* Remove all stored panels, we want to use defaults (order, open/closed) as defined by UI code here! */
				BKE_area_region_panels_free(&ar->panels);

				/* some toolbars have been saved as initialized,
				 * we don't want them to have odd zoom-level or scrolling set, see: T47047 */
				if (ELEM(ar->regiontype, RGN_TYPE_UI, RGN_TYPE_TOOLS, RGN_TYPE_TOOL_PROPS)) {
					ar->v2d.flag &= ~V2D_IS_INITIALISED;
				}
			}

			if (area->spacetype == SPACE_FILE) {
				SpaceFile *sfile = area->spacedata.first;

				if (sfile->params) {
					if (STREQ(screen->id.name, "SRDefault.003")) {
						/* Shading. */
						sfile->params->filter = FILE_TYPE_FOLDER |
						                        FILE_TYPE_IMAGE;
					}
					else {
						/* Video Editing. */
						sfile->params->filter = FILE_TYPE_FOLDER |
						                        FILE_TYPE_IMAGE |
						                        FILE_TYPE_MOVIE |
						                        FILE_TYPE_SOUND;
					}
				}
			}
		}
	}

	for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
		BLI_strncpy(scene->r.engine, RE_engine_id_BLENDER_EEVEE, sizeof(scene->r.engine));

		scene->r.cfra = 1.0f;

		/* Don't enable compositing nodes. */
		if (scene->nodetree) {
			ntreeFreeTree(scene->nodetree);
			MEM_freeN(scene->nodetree);
			scene->nodetree = NULL;
			scene->use_nodes = false;
		}

		/* Select only cube by default. */
		for (ViewLayer *layer = scene->view_layers.first; layer; layer = layer->next) {
			for (Base *base = layer->object_bases.first; base; base = base->next) {
				if (STREQ(base->object->id.name + 2, "Cube")) {
					base->flag |= BASE_SELECTED;
				}
				else {
					base->flag &= ~BASE_SELECTED;
				}
			}

			BKE_layer_collection_sync(scene, layer);
		}
	}
}
