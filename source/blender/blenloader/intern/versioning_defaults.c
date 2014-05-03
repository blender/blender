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

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_freestyle_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_mesh_types.h"

#include "BKE_main.h"

#include "BLO_readfile.h"

/* Update defaults in startup.blend, without having to save and embed the file.
 * This function can be emptied each time the startup.blend is updated. */
void BLO_update_defaults_startup_blend(Main *main)
{
	Scene *scene;
	SceneRenderLayer *srl;
	FreestyleLineStyle *linestyle;
	Mesh *me;

	for (scene = main->scene.first; scene; scene = scene->id.next) {
		scene->r.im_format.planes = R_IMF_PLANES_RGBA;
		scene->r.im_format.compress = 15;

		for (srl = scene->r.layers.first; srl; srl = srl->next) {
			srl->freestyleConfig.sphere_radius = 0.1f;
			srl->pass_alpha_threshold = 0.5f;
		}
	}

	for (linestyle = main->linestyle.first; linestyle; linestyle = linestyle->id.next) {
		linestyle->flag = LS_SAME_OBJECT | LS_NO_SORTING | LS_TEXTURE;
		linestyle->sort_key = LS_SORT_KEY_DISTANCE_FROM_CAMERA;
		linestyle->integration_type = LS_INTEGRATION_MEAN;
		linestyle->texstep = 1.0;
	}

	{
		bScreen *screen;

		for (screen = main->screen.first; screen; screen = screen->id.next) {
			ScrArea *area;
			for (area = screen->areabase.first; area; area = area->next) {
				SpaceLink *space_link;
				for (space_link = area->spacedata.first; space_link; space_link = space_link->next) {
					if (space_link->spacetype == SPACE_CLIP) {
						SpaceClip *space_clip = (SpaceClip *) space_link;
						space_clip->flag &= ~SC_MANUAL_CALIBRATION;
					}
				}
			}
		}
	}

	for (me = main->mesh.first; me; me = me->id.next) {
		me->smoothresh = DEG2RADF(180.0f);
	}
}

