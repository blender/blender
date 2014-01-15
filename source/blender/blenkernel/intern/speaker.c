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

/** \file blender/blenkernel/intern/speaker.c
 *  \ingroup bke
 */

#include "DNA_object_types.h"
#include "DNA_sound_types.h"
#include "DNA_speaker_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_speaker.h"

void *BKE_speaker_add(Main *bmain, const char *name)
{
	Speaker *spk;

	spk =  BKE_libblock_alloc(bmain, ID_SPK, name);

	spk->attenuation = 1.0f;
	spk->cone_angle_inner = 360.0f;
	spk->cone_angle_outer = 360.0f;
	spk->cone_volume_outer = 1.0f;
	spk->distance_max = FLT_MAX;
	spk->distance_reference = 1.0f;
	spk->flag = 0;
	spk->pitch = 1.0f;
	spk->sound = NULL;
	spk->volume = 1.0f;
	spk->volume_max = 1.0f;
	spk->volume_min = 0.0f;

	return spk;
}

Speaker *BKE_speaker_copy(Speaker *spk)
{
	Speaker *spkn;

	spkn = BKE_libblock_copy(&spk->id);
	if (spkn->sound)
		spkn->sound->id.us++;

	return spkn;
}

void BKE_speaker_make_local(Speaker *spk)
{
	Main *bmain = G.main;
	Object *ob;
	int is_local = FALSE, is_lib = FALSE;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */

	if (spk->id.lib == NULL) return;
	if (spk->id.us == 1) {
		id_clear_lib_data(bmain, &spk->id);
		return;
	}

	ob = bmain->object.first;
	while (ob) {
		if (ob->data == spk) {
			if (ob->id.lib) is_lib = TRUE;
			else is_local = TRUE;
		}
		ob = ob->id.next;
	}

	if (is_local && is_lib == FALSE) {
		id_clear_lib_data(bmain, &spk->id);
	}
	else if (is_local && is_lib) {
		Speaker *spk_new = BKE_speaker_copy(spk);
		spk_new->id.us = 0;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, spk->id.lib, &spk_new->id);

		ob = bmain->object.first;
		while (ob) {
			if (ob->data == spk) {

				if (ob->id.lib == NULL) {
					ob->data = spk_new;
					spk_new->id.us++;
					spk->id.us--;
				}
			}
			ob = ob->id.next;
		}
	}
}

void BKE_speaker_free(Speaker *spk)
{
	if (spk->sound)
		spk->sound->id.us--;

	BKE_free_animdata((ID *)spk);
}
