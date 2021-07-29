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
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_speaker.h"

void BKE_speaker_init(Speaker *spk)
{
	BLI_assert(MEMCMP_STRUCT_OFS_IS_ZERO(spk, id));

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
}

void *BKE_speaker_add(Main *bmain, const char *name)
{
	Speaker *spk;

	spk =  BKE_libblock_alloc(bmain, ID_SPK, name);

	BKE_speaker_init(spk);

	return spk;
}

Speaker *BKE_speaker_copy(Main *bmain, const Speaker *spk)
{
	Speaker *spkn;

	spkn = BKE_libblock_copy(bmain, &spk->id);

	if (spkn->sound)
		id_us_plus(&spkn->sound->id);

	BKE_id_copy_ensure_local(bmain, &spk->id, &spkn->id);

	return spkn;
}

void BKE_speaker_make_local(Main *bmain, Speaker *spk, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &spk->id, true, lib_local);
}

void BKE_speaker_free(Speaker *spk)
{
	BKE_animdata_free((ID *)spk, false);
}
