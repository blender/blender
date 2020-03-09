/*
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
 */

/** \file
 * \ingroup bke
 */

#include "DNA_object_types.h"
#include "DNA_sound_types.h"
#include "DNA_speaker_types.h"
#include "DNA_defaults.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_speaker.h"

static void speaker_init_data(ID *id)
{
  Speaker *speaker = (Speaker *)id;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(speaker, id));

  MEMCPY_STRUCT_AFTER(speaker, DNA_struct_default_get(Speaker), id);
}

IDTypeInfo IDType_ID_SPK = {
    .id_code = ID_SPK,
    .id_filter = FILTER_ID_SPK,
    .main_listbase_index = INDEX_ID_SPK,
    .struct_size = sizeof(Speaker),
    .name = "Speaker",
    .name_plural = "speakers",
    .translation_context = BLT_I18NCONTEXT_ID_SPEAKER,
    .flags = 0,

    .init_data = speaker_init_data,
    .copy_data = NULL,
    .free_data = NULL,
    .make_local = NULL,
};

void *BKE_speaker_add(Main *bmain, const char *name)
{
  Speaker *spk;

  spk = BKE_libblock_alloc(bmain, ID_SPK, name, 0);

  speaker_init_data(&spk->id);

  return spk;
}

Speaker *BKE_speaker_copy(Main *bmain, const Speaker *spk)
{
  Speaker *spk_copy;
  BKE_id_copy(bmain, &spk->id, (ID **)&spk_copy);
  return spk_copy;
}
