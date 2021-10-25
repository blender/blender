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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/cachefile.c
 *  \ingroup bke
 */

#include "DNA_anim_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_cachefile.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

static SpinLock spin;

void BKE_cachefiles_init(void)
{
	BLI_spin_init(&spin);
}

void BKE_cachefiles_exit(void)
{
	BLI_spin_end(&spin);
}

void *BKE_cachefile_add(Main *bmain, const char *name)
{
	CacheFile *cache_file = BKE_libblock_alloc(bmain, ID_CF, name);

	BKE_cachefile_init(cache_file);

	return cache_file;
}

void BKE_cachefile_init(CacheFile *cache_file)
{
	cache_file->handle = NULL;
	cache_file->filepath[0] = '\0';
	cache_file->override_frame = false;
	cache_file->frame = 0.0f;
	cache_file->is_sequence = false;
	cache_file->scale = 1.0f;
	cache_file->handle_mutex = BLI_mutex_alloc();
	BLI_listbase_clear(&cache_file->object_paths);
}

/** Free (or release) any data used by this cachefile (does not free the cachefile itself). */
void BKE_cachefile_free(CacheFile *cache_file)
{
	BKE_animdata_free((ID *)cache_file, false);

#ifdef WITH_ALEMBIC
	ABC_free_handle(cache_file->handle);
#endif

	if (cache_file->handle_mutex) {
		BLI_mutex_free(cache_file->handle_mutex);
	}
	BLI_freelistN(&cache_file->object_paths);
}

CacheFile *BKE_cachefile_copy(Main *bmain, const CacheFile *cache_file)
{
	CacheFile *new_cache_file = BKE_libblock_copy(bmain, &cache_file->id);
	new_cache_file->handle = NULL;

	BLI_listbase_clear(&new_cache_file->object_paths);

	BKE_id_copy_ensure_local(bmain, &cache_file->id, &new_cache_file->id);

	return new_cache_file;
}

void BKE_cachefile_make_local(Main *bmain, CacheFile *cache_file, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &cache_file->id, true, lib_local);
}

void BKE_cachefile_reload(const Main *bmain, CacheFile *cache_file)
{
	char filepath[FILE_MAX];

	BLI_strncpy(filepath, cache_file->filepath, sizeof(filepath));
	BLI_path_abs(filepath, ID_BLEND_PATH(bmain, &cache_file->id));

#ifdef WITH_ALEMBIC
	if (cache_file->handle) {
		ABC_free_handle(cache_file->handle);
	}

	cache_file->handle = ABC_create_handle(filepath, &cache_file->object_paths);
#endif
}

void BKE_cachefile_ensure_handle(const Main *bmain, CacheFile *cache_file)
{
	BLI_spin_lock(&spin);
	if (cache_file->handle_mutex == NULL) {
		cache_file->handle_mutex = BLI_mutex_alloc();
	}
	BLI_spin_unlock(&spin);

	BLI_mutex_lock(cache_file->handle_mutex);

	if (cache_file->handle == NULL) {
		BKE_cachefile_reload(bmain, cache_file);
	}

	BLI_mutex_unlock(cache_file->handle_mutex);
}

void BKE_cachefile_update_frame(Main *bmain, Scene *scene, const float ctime, const float fps)
{
	CacheFile *cache_file;
	char filename[FILE_MAX];

	for (cache_file = bmain->cachefiles.first; cache_file; cache_file = cache_file->id.next) {
		/* Execute drivers only, as animation has already been done. */
		BKE_animsys_evaluate_animdata(scene, &cache_file->id, cache_file->adt, ctime, ADT_RECALC_DRIVERS);

		if (!cache_file->is_sequence) {
			continue;
		}

		const float time = BKE_cachefile_time_offset(cache_file, ctime, fps);

		if (BKE_cachefile_filepath_get(bmain, cache_file, time, filename)) {
			BKE_cachefile_clean(scene, cache_file);
#ifdef WITH_ALEMBIC
			ABC_free_handle(cache_file->handle);
			cache_file->handle = ABC_create_handle(filename, NULL);
#endif
		}
	}
}

bool BKE_cachefile_filepath_get(
        const Main *bmain, const CacheFile *cache_file, float frame,
        char r_filepath[FILE_MAX])
{
	BLI_strncpy(r_filepath, cache_file->filepath, FILE_MAX);
	BLI_path_abs(r_filepath, ID_BLEND_PATH(bmain, &cache_file->id));

	int fframe;
	int frame_len;

	if (cache_file->is_sequence && BLI_path_frame_get(r_filepath, &fframe, &frame_len)) {
		char ext[32];
		BLI_path_frame_strip(r_filepath, true, ext);
		BLI_path_frame(r_filepath, frame, frame_len);
		BLI_ensure_extension(r_filepath, FILE_MAX, ext);

		/* TODO(kevin): store sequence range? */
		return BLI_exists(r_filepath);
	}

	return true;
}

float BKE_cachefile_time_offset(CacheFile *cache_file, const float time, const float fps)
{
	const float frame = (cache_file->override_frame ? cache_file->frame : time);
	return cache_file->is_sequence ? frame : frame / fps;
}

/* TODO(kevin): replace this with some depsgraph mechanism, or something similar. */
void BKE_cachefile_clean(Scene *scene, CacheFile *cache_file)
{
	for (Base *base = scene->base.first; base; base = base->next) {
		Object *ob = base->object;

		ModifierData *md = modifiers_findByType(ob, eModifierType_MeshSequenceCache);

		if (md) {
			MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *)md;

			if (cache_file == mcmd->cache_file) {
#ifdef WITH_ALEMBIC
				if (mcmd->reader != NULL) {
					CacheReader_free(mcmd->reader);
				}
#endif
				mcmd->reader = NULL;
			}
		}

		for (bConstraint *con = ob->constraints.first; con; con = con->next) {
			if (con->type != CONSTRAINT_TYPE_TRANSFORM_CACHE) {
				continue;
			}

			bTransformCacheConstraint *data = con->data;

			if (cache_file == data->cache_file) {
#ifdef WITH_ALEMBIC
				if (data->reader != NULL) {
					CacheReader_free(data->reader);
				}
#endif
				data->reader = NULL;
			}
		}
	}
}
