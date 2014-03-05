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

/** \file blender/blenloader/intern/versioning_270.c
 *  \ingroup blenloader
 */

#include "BLI_utildefines.h"
#include "BLI_compiler_attrs.h"

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_sdna_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_sdna_types.h"

#include "DNA_genfile.h"


#include "BKE_main.h"
#include "BKE_node.h"

#include "BLO_readfile.h"

#include "readfile.h"


void blo_do_versions_270(FileData *fd, Library *UNUSED(lib), Main *main)
{
	if (!MAIN_VERSION_ATLEAST(main, 270, 0)) {

		if (!DNA_struct_elem_find(fd->filesdna, "BevelModifierData", "float", "profile")) {
			Object *ob;

			for (ob = main->object.first; ob; ob = ob->id.next) {
				ModifierData *md;
				for (md = ob->modifiers.first; md; md = md->next) {
					if (md->type == eModifierType_Bevel) {
						BevelModifierData *bmd = (BevelModifierData *)md;
						bmd->profile = 0.5f;
						bmd->val_flags = MOD_BEVEL_AMT_OFFSET;
					}
				}
			}
		}

		/* nodes don't use fixed node->id any more, clean up */
		FOREACH_NODETREE(main, ntree, id) {
			if (ntree->type == NTREE_COMPOSIT) {
				bNode *node;
				for (node = ntree->nodes.first; node; node = node->next) {
					if (ELEM(node->type, CMP_NODE_COMPOSITE, CMP_NODE_OUTPUT_FILE)) {
						node->id = NULL;
					}
				}
			}
		} FOREACH_NODETREE_END

		{
			bScreen *screen;

			for (screen = main->screen.first; screen; screen = screen->id.next) {
				ScrArea *area;
				for (area = screen->areabase.first; area; area = area->next) {
					SpaceLink *space_link;
					for (space_link = area->spacedata.first; space_link; space_link = space_link->next) {
						if (space_link->spacetype == SPACE_CLIP) {
							SpaceClip *space_clip = (SpaceClip *) space_link;
							if (space_clip->mode != SC_MODE_MASKEDIT) {
								space_clip->mode = SC_MODE_TRACKING;
							}
						}
					}
				}
			}
		}

		if (!DNA_struct_elem_find(fd->filesdna, "MovieTrackingSettings", "float", "default_weight")) {
			MovieClip *clip;
			for (clip = main->movieclip.first; clip; clip = clip->id.next) {
				clip->tracking.settings.default_weight = 1.0f;
			}
		}
	}
}
